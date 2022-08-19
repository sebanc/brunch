/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_HID_H
#define IPTS_HID_H

#include <linux/types.h>

#include "context.h"
#include "spec-device.h"

int ipts_hid_input_data(struct ipts_context *ipts, int buffer);

int ipts_hid_init(struct ipts_context *ipts, struct ipts_device_info info);
void ipts_hid_free(struct ipts_context *ipts);

#endif /* IPTS_HID_H */
