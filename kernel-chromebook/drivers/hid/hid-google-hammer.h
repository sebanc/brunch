/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * HID driver for Google Hammer device.
 *
 * Copyright (c) 2018 Google Inc.
 */
#ifndef _HID_GOOGLE_HAMMER_H
#define _HID_GOOGLE_HAMMER_H

int hammer_input_configured(struct hid_device *hdev, struct hid_input *hi);

#endif
