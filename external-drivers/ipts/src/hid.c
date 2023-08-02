// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2022-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/completion.h>
#include <linux/gfp.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "desc.h"
#include "hid.h"
#include "spec-data.h"
#include "spec-device.h"
#include "spec-hid.h"

static int ipts_hid_start(struct hid_device *hid)
{
	return 0;
}

static void ipts_hid_stop(struct hid_device *hid)
{
}

static int ipts_hid_switch_mode(struct ipts_context *ipts, enum ipts_mode mode)
{
	if (!ipts)
		return -EFAULT;

	if (ipts->mode == mode)
		return 0;

	/*
	 * This is only allowed on older devices.
	 */
	if (ipts->info.intf_eds > 1)
		return 0;

	ipts->mode = mode;
	return ipts_control_restart(ipts);
}

static int ipts_hid_parse(struct hid_device *hid)
{
	int ret = 0;
	struct ipts_context *ipts = NULL;

	bool has_native_descriptor = false;

	u8 *buffer = NULL;
	size_t size = 0;

	if (!hid)
		return -ENODEV;

	ipts = hid->driver_data;

	if (!ipts)
		return -EFAULT;

	size = sizeof(ipts_singletouch_descriptor);
	has_native_descriptor = ipts->descriptor.address && ipts->descriptor.size > 0;

	if (has_native_descriptor)
		size += ipts->descriptor.size;
	else
		size += sizeof(ipts_fallback_descriptor);

	buffer = kzalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	memcpy(buffer, ipts_singletouch_descriptor, sizeof(ipts_singletouch_descriptor));

	if (has_native_descriptor) {
		memcpy(&buffer[sizeof(ipts_singletouch_descriptor)], ipts->descriptor.address,
		       ipts->descriptor.size);
	} else {
		memcpy(&buffer[sizeof(ipts_singletouch_descriptor)], ipts_fallback_descriptor,
		       sizeof(ipts_fallback_descriptor));
	}

	ret = hid_parse_report(hid, buffer, size);
	kfree(buffer);

	if (ret) {
		dev_err(ipts->dev, "Failed to parse HID descriptor: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ipts_hid_get_feature(struct ipts_context *ipts, unsigned char reportnum, __u8 *buf,
				size_t size, enum ipts_feedback_data_type type)
{
	int ret = 0;

	if (!ipts)
		return -EFAULT;

	if (!buf)
		return -EFAULT;

	mutex_lock(&ipts->feature_lock);

	memset(buf, 0, size);
	buf[0] = reportnum;

	memset(&ipts->feature_report, 0, sizeof(ipts->feature_report));
	reinit_completion(&ipts->feature_event);

	ret = ipts_control_hid2me_feedback(ipts, IPTS_FEEDBACK_CMD_TYPE_NONE, type, buf, size);
	if (ret) {
		dev_err(ipts->dev, "Failed to send hid2me feedback: %d\n", ret);
		goto out;
	}

	ret = wait_for_completion_timeout(&ipts->feature_event, msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_warn(ipts->dev, "GET_FEATURES timed out!\n");
		ret = -EIO;
		goto out;
	}

	if (!ipts->feature_report.address) {
		ret = -EFAULT;
		goto out;
	}

	if (ipts->feature_report.size > size) {
		ret = -ETOOSMALL;
		goto out;
	}

	ret = ipts->feature_report.size;
	memcpy(buf, ipts->feature_report.address, ipts->feature_report.size);

out:
	mutex_unlock(&ipts->feature_lock);
	return ret;
}

static int ipts_hid_set_feature(struct ipts_context *ipts, unsigned char reportnum, __u8 *buf,
				size_t size, enum ipts_feedback_data_type type)
{
	int ret = 0;

	if (!ipts)
		return -EFAULT;

	if (!buf)
		return -EFAULT;

	buf[0] = reportnum;

	ret = ipts_control_hid2me_feedback(ipts, IPTS_FEEDBACK_CMD_TYPE_NONE, type, buf, size);
	if (ret)
		dev_err(ipts->dev, "Failed to send hid2me feedback: %d\n", ret);

	return ret;
}

static int ipts_hid_raw_request(struct hid_device *hid, unsigned char reportnum, __u8 *buf,
				size_t size, unsigned char rtype, int reqtype)
{
	int ret = 0;
	struct ipts_context *ipts = NULL;

	enum ipts_feedback_data_type type = IPTS_FEEDBACK_DATA_TYPE_VENDOR;

	if (!hid)
		return -ENODEV;

	ipts = hid->driver_data;

	if (!ipts)
		return -EFAULT;

	if (!buf)
		return -EFAULT;

	if (rtype == HID_OUTPUT_REPORT && reqtype == HID_REQ_SET_REPORT)
		type = IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT;
	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT)
		type = IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES;
	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_SET_REPORT)
		type = IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES;
	else
		return -EIO;

	// Implemente mode switching report for older devices without native HID support
	if (type == IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES && reportnum == IPTS_HID_REPORT_SET_MODE) {
		ret = ipts_hid_switch_mode(ipts, buf[1]);
		if (ret) {
			dev_err(ipts->dev, "Failed to switch modes: %d\n", ret);
			return ret;
		}
	}

	if (reqtype == HID_REQ_GET_REPORT)
		return ipts_hid_get_feature(ipts, reportnum, buf, size, type);
	else
		return ipts_hid_set_feature(ipts, reportnum, buf, size, type);
}

static int ipts_hid_output_report(struct hid_device *hid, __u8 *data, size_t size)
{
	struct ipts_context *ipts = NULL;

	if (!hid)
		return -ENODEV;

	ipts = hid->driver_data;

	return ipts_control_hid2me_feedback(ipts, IPTS_FEEDBACK_CMD_TYPE_NONE,
					    IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT, data, size);
}

static struct hid_ll_driver ipts_hid_driver = {
	.start = ipts_hid_start,
	.stop = ipts_hid_stop,
	.open = ipts_hid_start,
	.close = ipts_hid_stop,
	.parse = ipts_hid_parse,
	.raw_request = ipts_hid_raw_request,
	.output_report = ipts_hid_output_report,
};

int ipts_hid_input_data(struct ipts_context *ipts, u32 buffer)
{
	u8 *temp = NULL;
	struct ipts_hid_header *frame = NULL;
	struct ipts_data_header *header = NULL;

	if (!ipts)
		return -EFAULT;

	if (!ipts->hid)
		return -ENODEV;

	header = (struct ipts_data_header *)ipts->resources.data[buffer].address;

	temp = ipts->resources.report.address;
	memset(temp, 0, ipts->resources.report.size);

	if (!header)
		return -EFAULT;

	if (header->size == 0)
		return 0;

	if (header->type == IPTS_DATA_TYPE_HID)
		return hid_input_report(ipts->hid, HID_INPUT_REPORT, header->data, header->size, 1);

	if (header->type == IPTS_DATA_TYPE_GET_FEATURES) {
		ipts->feature_report.address = header->data;
		ipts->feature_report.size = header->size;

		complete_all(&ipts->feature_event);
		return 0;
	}

	if (header->type != IPTS_DATA_TYPE_FRAME)
		return 0;

	if (header->size + 3 + sizeof(struct ipts_hid_header) > IPTS_HID_REPORT_DATA_SIZE)
		return -ERANGE;

	/*
	 * Synthesize a HID report matching the devices that natively send HID reports
	 */
	temp[0] = IPTS_HID_REPORT_DATA;

	frame = (struct ipts_hid_header *)&temp[3];
	frame->type = IPTS_HID_FRAME_TYPE_RAW;
	frame->size = header->size + sizeof(*frame);

	memcpy(frame->data, header->data, header->size);

	return hid_input_report(ipts->hid, HID_INPUT_REPORT, temp, IPTS_HID_REPORT_DATA_SIZE, 1);
}

int ipts_hid_init(struct ipts_context *ipts, struct ipts_device_info info)
{
	int ret = 0;

	if (!ipts)
		return -EFAULT;

	if (ipts->hid)
		return 0;

	ipts->hid = hid_allocate_device();
	if (IS_ERR(ipts->hid)) {
		int err = PTR_ERR(ipts->hid);

		dev_err(ipts->dev, "Failed to allocate HID device: %d\n", err);
		return err;
	}

	ipts->hid->driver_data = ipts;
	ipts->hid->dev.parent = ipts->dev;
	ipts->hid->ll_driver = &ipts_hid_driver;

	ipts->hid->vendor = info.vendor;
	ipts->hid->product = info.product;
	ipts->hid->group = HID_GROUP_GENERIC;

	snprintf(ipts->hid->name, sizeof(ipts->hid->name), "IPTS %04X:%04X", info.vendor,
		 info.product);

	ret = hid_add_device(ipts->hid);
	if (ret) {
		dev_err(ipts->dev, "Failed to add HID device: %d\n", ret);
		ipts_hid_free(ipts);
		return ret;
	}

	return 0;
}

int ipts_hid_free(struct ipts_context *ipts)
{
	if (!ipts)
		return -EFAULT;

	if (!ipts->hid)
		return 0;

	hid_destroy_device(ipts->hid);
	ipts->hid = NULL;

	return 0;
}
