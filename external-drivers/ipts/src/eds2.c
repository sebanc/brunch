// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/completion.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "eds2.h"
#include "context.h"
#include "control.h"
#include "resources.h"
#include "spec-dma.h"
#include "spec-hid.h"

/**
 * GET_FEATURES_TIMEOUT - How long to wait for the reply to a GET_FEATURES request.
 *
 * Sometimes the answer can take 10 seconds or more to arrive,
 * so lets just wait for a long time to be sure.
 */
#define GET_FEATURES_TIMEOUT 30 * MSEC_PER_SEC

int ipts_eds2_get_descriptor(struct ipts_context *ipts, u8 **desc_buffer, size_t *desc_size)
{
	u8 *buffer = NULL;
	size_t size = 0;

	struct ipts_data_buffer *descbuffer =
		(struct ipts_data_buffer *)ipts->resources.descriptor.address;

	if (descbuffer->type != IPTS_DATA_TYPE_HID_DESCRIPTOR)
		return -ENODATA;

	size = sizeof(ipts_singletouch_descriptor) + descbuffer->size - 8;

	buffer = kzalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	memcpy(buffer, ipts_singletouch_descriptor, sizeof(ipts_singletouch_descriptor));
	memcpy(&buffer[sizeof(ipts_singletouch_descriptor)], &descbuffer->data[8],
	       descbuffer->size - 8);

	*desc_size = size;
	*desc_buffer = buffer;

	return 0;
}

static int ipts_eds2_get_feature(struct ipts_context *ipts, u8 *buffer, size_t size, u8 report_id,
				 enum ipts_feedback_data_type type)
{
	int ret = 0;

	struct ipts_buffer feature = ipts->resources.feature;
	struct ipts_data_buffer *response = (struct ipts_data_buffer *)feature.address;

	mutex_lock(&ipts->feature_lock);

	memset(buffer, 0, size);
	buffer[0] = report_id;

	reinit_completion(&ipts->feature_event);

	ret = ipts_control_hid2me_feedback(ipts, IPTS_FEEDBACK_CMD_TYPE_NONE, type, buffer, size);
	if (ret) {
		dev_err(ipts->dev, "Failed to send hid2me feedback: %d\n", ret);
		goto out;
	}

	ret = wait_for_completion_timeout(&ipts->feature_event,
					  msecs_to_jiffies(GET_FEATURES_TIMEOUT));

	if (ret == 0) {
		dev_warn(ipts->dev, "GET_FEATURES timed out!\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	if (response->size > size) {
		ret = -ETOOSMALL;
		goto out;
	}

	ret = response->size;
	memcpy(buffer, response->data, response->size);

out:
	mutex_unlock(&ipts->feature_lock);
	return ret;
}

static int ipts_eds2_set_feature(struct ipts_context *ipts, u8 *buffer, size_t size, u8 report_id,
				 enum ipts_feedback_data_type type)
{
	int ret = 0;

	buffer[0] = report_id;

	ret = ipts_control_hid2me_feedback(ipts, IPTS_FEEDBACK_CMD_TYPE_NONE, type, buffer, size);
	if (ret)
		dev_err(ipts->dev, "Failed to send hid2me feedback: %d\n", ret);

	return ret;
}

int ipts_eds2_raw_request(struct ipts_context *ipts, u8 *buffer, size_t size, u8 report_id,
			  enum hid_report_type report_type, enum hid_class_request request_type)
{
	enum ipts_feedback_data_type feedback_type = IPTS_FEEDBACK_DATA_TYPE_VENDOR;

	if (report_type == HID_OUTPUT_REPORT && request_type == HID_REQ_SET_REPORT)
		feedback_type = IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT;
	else if (report_type == HID_FEATURE_REPORT && request_type == HID_REQ_GET_REPORT)
		feedback_type = IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES;
	else if (report_type == HID_FEATURE_REPORT && request_type == HID_REQ_SET_REPORT)
		feedback_type = IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES;
	else
		return -EIO;

	if (request_type == HID_REQ_GET_REPORT)
		return ipts_eds2_get_feature(ipts, buffer, size, report_id, feedback_type);
	else
		return ipts_eds2_set_feature(ipts, buffer, size, report_id, feedback_type);
}
