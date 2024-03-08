// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_EDS1_H
#define IPTS_EDS1_H

#include <linux/hid.h>
#include <linux/types.h>

#include "context.h"

int ipts_eds1_get_descriptor(struct ipts_context *ipts, u8 **desc_buffer, size_t *desc_size);
int ipts_eds1_raw_request(struct ipts_context *ipts, u8 *buffer, size_t size, u8 report_id,
			  enum hid_report_type report_type, enum hid_class_request request_type);

#endif /* IPTS_EDS1_H */
