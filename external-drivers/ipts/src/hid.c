// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/completion.h>
#include <linux/gfp.h>
#include <linux/hid.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "desc.h"
#include "hid.h"
#include "spec-data.h"
#include "spec-device.h"
#include "spec-hid.h"

DECLARE_COMPLETION(on_feature);

static int ipts_hid_start(struct hid_device *hid)
{
	return 0;
}

static void ipts_hid_stop(struct hid_device *hid)
{
}

static int ipts_hid_hid2me_feedback(struct ipts_context *ipts, enum ipts_feedback_data_type type,
				    void *data, size_t size)
{
	struct ipts_feedback_header *header;

	if (!ipts)
		return -EFAULT;

	memset(ipts->resources.hid2me.address, 0, ipts->resources.hid2me.size);
	header = (struct ipts_feedback_header *)ipts->resources.hid2me.address;

	header->cmd_type = IPTS_FEEDBACK_CMD_TYPE_NONE;
	header->buffer = IPTS_HID2ME_BUFFER;
	header->data_type = type;
	header->size = size;

	if (size + sizeof(*header) > ipts->resources.hid2me.size)
		return -EINVAL;

	if (data && size > 0)
		memcpy(header->payload, data, size);

	return ipts_control_send_feedback(ipts, IPTS_HID2ME_BUFFER);
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
	int ret;
	u8 *buffer;
	size_t size;
	struct ipts_context *ipts;

	if (!hid)
		return -EFAULT;

	ipts = hid->driver_data;

	if (!ipts)
		return -EFAULT;

	size = sizeof(ipts_singletouch_descriptor);

	if (ipts->descriptor && ipts->desc_size > 0)
		size += ipts->desc_size;
	else
		size += sizeof(ipts_fallback_descriptor);

	buffer = kzalloc(size, GFP_KERNEL);
	memcpy(buffer, ipts_singletouch_descriptor, sizeof(ipts_singletouch_descriptor));

	if (ipts->descriptor && ipts->desc_size > 0) {
		memcpy(&buffer[sizeof(ipts_singletouch_descriptor)], ipts->descriptor,
		       ipts->desc_size);
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

static int ipts_hid_raw_request(struct hid_device *hid, unsigned char reportnum, __u8 *buf,
				size_t size, unsigned char rtype, int reqtype)
{
	int ret;
	enum ipts_feedback_data_type type;
	struct ipts_context *ipts;

	if (!hid)
		return -EFAULT;

	ipts = hid->driver_data;

	if (!ipts)
		return -EFAULT;

	if (rtype == HID_OUTPUT_REPORT && reqtype == HID_REQ_SET_REPORT)
		type = IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT;
	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT)
		type = IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES;
	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_SET_REPORT)
		type = IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES;
	else
		return -EIO;

	if (type == IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES && reportnum == IPTS_HID_REPORT_SET_MODE) {
		ret = ipts_hid_switch_mode(ipts, buf[1]);
		if (ret) {
			dev_err(ipts->dev, "Failed to switch modes: %d\n", ret);
			return ret;
		}
	}

	ret = ipts_hid_hid2me_feedback(ipts, type, buf, size);
	if (ret) {
		dev_err(ipts->dev, "Failed to send hid2me feedback: %d\n", ret);
		return ret;
	}

	if (reqtype == HID_REQ_SET_REPORT)
		return 0;

	ipts->get_feature_report = NULL;
	reinit_completion(&on_feature);

	ret = wait_for_completion_timeout(&on_feature, msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_warn(ipts->dev, "GET_FEATURES timed out!\n");
		return -EIO;
	}

	if (!ipts->get_feature_report)
		return -EFAULT;

	memcpy(buf, ipts->get_feature_report, size);
	return 0;
}

static int ipts_hid_output_report(struct hid_device *hid, __u8 *data, size_t size)
{
	struct ipts_context *ipts = hid->driver_data;

	if (!ipts)
		return -EFAULT;

	return ipts_hid_hid2me_feedback(ipts, IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT, data, size);
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

int ipts_hid_input_data(struct ipts_context *ipts, int buffer)
{
	int ret;
	u8 *temp;
	struct ipts_hid_header *frame;
	struct ipts_data_header *header;

	if (!ipts)
		return -EFAULT;

	if (!ipts->hid)
		return -ENODEV;

	header = (struct ipts_data_header *)ipts->resources.data[buffer].address;

	if (header->size == 0)
		return 0;

	if (header->type == IPTS_DATA_TYPE_HID)
		return hid_input_report(ipts->hid, HID_INPUT_REPORT, header->data, header->size, 1);

	if (header->type == IPTS_DATA_TYPE_GET_FEATURES) {
		ipts->get_feature_report = header->data;
		complete_all(&on_feature);

		return 0;
	}

	if (header->type != IPTS_DATA_TYPE_FRAME)
		return 0;

	if (header->size + 3 + sizeof(struct ipts_hid_header) > IPTS_HID_REPORT_DATA_SIZE)
		return -ERANGE;

	temp = kzalloc(IPTS_HID_REPORT_DATA_SIZE, GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	/*
	 * Synthesize a HID report matching the devices that natively send HID reports
	 */
	temp[0] = IPTS_HID_REPORT_DATA;

	frame = (struct ipts_hid_header *)&temp[3];
	frame->type = IPTS_HID_FRAME_TYPE_RAW;
	frame->size = header->size + sizeof(*frame);

	memcpy(frame->data, header->data, header->size);

	ret = hid_input_report(ipts->hid, HID_INPUT_REPORT, temp, IPTS_HID_REPORT_DATA_SIZE, 1);
	kfree(temp);

	return ret;
}

int ipts_hid_init(struct ipts_context *ipts, struct ipts_device_info info)
{
	int ret;

	if (!ipts)
		return -EFAULT;

	if (ipts->hid)
		return 0;

	ipts->hid = hid_allocate_device();
	if (IS_ERR(ipts->hid)) {
		dev_err(ipts->dev, "Failed to allocate HID device: %ld\n", PTR_ERR(ipts->hid));
		return PTR_ERR(ipts->hid);
	}

	ipts->hid->driver_data = ipts;
	ipts->hid->dev.parent = ipts->dev;
	ipts->hid->ll_driver = &ipts_hid_driver;

	ipts->hid->vendor = info.vendor;
	ipts->hid->product = info.product;
	ipts->hid->group = HID_GROUP_MULTITOUCH;

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

void ipts_hid_free(struct ipts_context *ipts)
{
	hid_destroy_device(ipts->hid);
	ipts->hid = NULL;
}
