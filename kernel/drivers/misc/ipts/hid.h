/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _IPTS_HID_H_
#define	_IPTS_HID_H_

typedef enum touch_feedback_data_types
{
	TOUCH_FEEDBACK_DATA_TYPE_FEEDBACK = 0,
	TOUCH_FEEDBACK_DATA_TYPE_SET_FEATURES,
	TOUCH_FEEDBACK_DATA_TYPE_GET_FEATURES,
	TOUCH_FEEDBACK_DATA_TYPE_OUTPUT_REPORT,
	TOUCH_FEEDBACK_DATA_TYPE_STORE_DATA,
	TOUCH_FEEDBACK_DATA_TYPE_MAX
} touch_feedback_data_types_t;

int ipts_hid_init(ipts_info_t *ipts);
void ipts_hid_fini(ipts_info_t *ipts);

#endif // _IPTS_HID_H_
