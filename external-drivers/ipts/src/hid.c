// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/completion.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "context.h"
#include "eds1.h"
#include "eds2.h"
#include "hid.h"
#include "resources.h"
#include "spec-dma.h"
#include "spec-hid.h"

static int ipts_hid_start(struct hid_device *hid)
{
	return 0;
}

static void ipts_hid_stop(struct hid_device *hid)
{
}

static int ipts_hid_parse(struct hid_device *hid)
{
	int ret = 0;

	u8 *buffer = NULL;
	size_t size = 0;

	struct ipts_context *ipts = hid->driver_data;

	if (!READ_ONCE(ipts->hid_active))
		return -ENODEV;

	if (ipts->eds_intf_rev == 1)
		ret = ipts_eds1_get_descriptor(ipts, &buffer, &size);
	else
		ret = ipts_eds2_get_descriptor(ipts, &buffer, &size);

	if (ret) {
		dev_err(ipts->dev, "Failed to allocate HID descriptor: %d\n", ret);
		return ret;
	}

	ret = hid_parse_report(hid, buffer, size);
	kfree(buffer);

	if (ret) {
		dev_err(ipts->dev, "Failed to parse HID descriptor: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ipts_hid_raw_request(struct hid_device *hid, unsigned char report_id, __u8 *buffer,
				size_t size, unsigned char report_type, int request_type)
{
	struct ipts_context *ipts = hid->driver_data;

	if (!READ_ONCE(ipts->hid_active))
		return -ENODEV;

	if (ipts->eds_intf_rev == 1) {
		return ipts_eds1_raw_request(ipts, buffer, size, report_id, report_type,
					     request_type);
	} else {
		return ipts_eds2_raw_request(ipts, buffer, size, report_id, report_type,
					     request_type);
	}
}

static struct hid_ll_driver ipts_hid_driver = {
	.start = ipts_hid_start,
	.stop = ipts_hid_stop,
	.open = ipts_hid_start,
	.close = ipts_hid_stop,
	.parse = ipts_hid_parse,
	.raw_request = ipts_hid_raw_request,
};

/**
 * ipts_hid_handle_frame() - Forward an IPTS data frame to userspace.
 *
 * IPTS data frames are a custom format for wrapping raw multitouch data, for example capacitive
 * heatmaps. We cannot process this data in the kernel, so we wrap it in a HID report and forward
 * it to userspace, where a dedicated tool can do the required processing.
 *
 * @ipts:
 *     The IPTS driver context.
 *
 * @buffer:
 *     The data buffer containing the data frame that is being forwarded.
 *
 * Returns: 0 on success, negative errno code on error.
 */
static int ipts_hid_handle_frame(struct ipts_context *ipts, struct ipts_data_buffer *buffer)
{
	struct ipts_hid_report_data *report = NULL;

	if (buffer->size + sizeof(*report) > IPTS_HID_REPORT_DATA_SIZE)
		return -ERANGE;

	report = (struct ipts_hid_report_data *)ipts->resources.report.address;
	memset(report, 0, ipts->resources.report.size);

	/*
	 * Synthesize a HID report that matches how the Surface Pro 7 transmits multitouch data.
	 */

	report->report_id = IPTS_HID_REPORT_DATA;
	report->gesture_char_quality.type = IPTS_HID_FRAME_TYPE_RAW;
	report->gesture_char_quality.size = buffer->size + sizeof(report->gesture_char_quality);

	memcpy(report->gesture_char_quality.data, buffer->data, buffer->size);

	return hid_input_report(ipts->hid, HID_INPUT_REPORT, (u8 *)report,
				IPTS_HID_REPORT_DATA_SIZE, 1);
}

static int ipts_hid_handle_hid(struct ipts_context *ipts, struct ipts_data_buffer *buffer)
{
	return hid_input_report(ipts->hid, HID_INPUT_REPORT, buffer->data, buffer->size, 1);
}

/**
 * ipts_hid_handle_get_features() - Process the answer to a GET_FEATURES request.
 *
 * When doing a GET_FEATURES request using HID2ME feedback, the answer will be sent in one of the
 * normal data buffers. When we get such data, we copy it, and then signal the waiting HID thread
 * that data has arrived.
 *
 * @ipts:
 *     The IPTS driver context.
 *
 * @buffer:
 *     The data buffer containing the result of the GET_FEATURES request.
 *
 * Returns: 0 on success, negative errno code on error.
 */
static int ipts_hid_handle_get_features(struct ipts_context *ipts, struct ipts_data_buffer *buffer)
{
	size_t size = min(ipts->resources.feature.size, sizeof(*buffer) + buffer->size);

	memcpy(ipts->resources.feature.address, buffer, size);
	complete_all(&ipts->feature_event);

	return 0;
}

int ipts_hid_input_data(struct ipts_context *ipts, struct ipts_data_buffer *buffer)
{
	if (!READ_ONCE(ipts->hid_active))
		return -ENODEV;

	if (buffer->size == 0)
		return 0;

	switch (buffer->type) {
	case IPTS_DATA_TYPE_FRAME:
		return ipts_hid_handle_frame(ipts, buffer);
	case IPTS_DATA_TYPE_HID:
		return ipts_hid_handle_hid(ipts, buffer);
	case IPTS_DATA_TYPE_GET_FEATURES:
		return ipts_hid_handle_get_features(ipts, buffer);
	default:
		dev_info(ipts->dev, "Unhandled data type: %d\n", buffer->type);
	}

	return 0;
}

int ipts_hid_init(struct ipts_context *ipts)
{
	int ret = 0;

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

	ipts->hid->vendor = ipts->info.vendor;
	ipts->hid->product = ipts->info.product;
	ipts->hid->group = HID_GROUP_GENERIC;

	snprintf(ipts->hid->name, sizeof(ipts->hid->name), "IPTS %04X:%04X", ipts->info.vendor,
		 ipts->info.product);

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
	if (!ipts->hid)
		return 0;

	hid_destroy_device(ipts->hid);
	ipts->hid = NULL;

	return 0;
}
