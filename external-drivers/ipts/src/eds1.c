// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/hid.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "spec-hid.h"
#include "spec-mei.h"

int ipts_eds1_get_descriptor(struct ipts_context *ipts, u8 **desc_buffer, size_t *desc_size)
{
	u8 *buffer = NULL;
	size_t size = sizeof(ipts_singletouch_descriptor) + sizeof(ipts_fallback_descriptor);

	buffer = kzalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	memcpy(buffer, ipts_singletouch_descriptor, sizeof(ipts_singletouch_descriptor));
	memcpy(&buffer[sizeof(ipts_singletouch_descriptor)], ipts_fallback_descriptor,
	       sizeof(ipts_fallback_descriptor));

	*desc_size = size;
	*desc_buffer = buffer;

	return 0;
}

static int ipts_eds1_switch_mode(struct ipts_context *ipts, enum ipts_mode mode)
{
	int ret = 0;

	if (ipts->mode == mode)
		return 0;

	ipts->mode = mode;

	ret = ipts_control_restart(ipts);
	if (ret)
		dev_err(ipts->dev, "Failed to switch modes: %d\n", ret);

	return ret;
}

int ipts_eds1_raw_request(struct ipts_context *ipts, u8 *buffer, size_t size, u8 report_id,
			  enum hid_report_type report_type, enum hid_class_request request_type)
{
	struct ipts_hid_report_set_mode *report = (struct ipts_hid_report_set_mode *)buffer;

	if (report_id != IPTS_HID_REPORT_SET_MODE)
		return -EIO;

	if (report_type != HID_FEATURE_REPORT)
		return -EIO;

	if (size != 2)
		return -EINVAL;

	/*
	 * Implement mode switching report for older devices without native HID support.
	 */

	if (request_type == HID_REQ_SET_REPORT)
		return ipts_eds1_switch_mode(ipts, report->mode);

	if (request_type != HID_REQ_GET_REPORT)
		return -EIO;

	memset(report, 0, sizeof(*report));

	report->report_id = IPTS_HID_REPORT_SET_MODE;
	report->mode = ipts->mode;

	return 0;
}
