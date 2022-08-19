/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_SPEC_HID_H
#define IPTS_SPEC_HID_H

#include <linux/build_bug.h>
#include <linux/types.h>

/*
 * Made-up type for passing raw IPTS data in a HID report.
 */
#define IPTS_HID_FRAME_TYPE_RAW 0xEE

/**
 * struct ipts_hid_frame - Header that is prefixed to raw IPTS data wrapped in a HID report.
 * @size: Size of the data inside the report, including this header.
 * @type: What type of data does this report contain.
 */
struct ipts_hid_header {
	u32 size;
	u8 reserved1;
	u8 type;
	u8 reserved2;
	u8 data[];
} __packed;

static_assert(sizeof(struct ipts_hid_header) == 7);

#endif /* IPTS_SPEC_HID_H */
