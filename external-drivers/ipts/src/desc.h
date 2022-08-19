/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_DESC_H
#define IPTS_DESC_H

#include <linux/types.h>

#define IPTS_HID_REPORT_SINGLETOUCH 64
#define IPTS_HID_REPORT_DATA	    65
#define IPTS_HID_REPORT_SET_MODE    66

#define IPTS_HID_REPORT_DATA_SIZE 7485

/*
 * HID descriptor for singletouch data.
 * This descriptor should be present on all IPTS devices.
 */
static const u8 ipts_singletouch_descriptor[] = {
	0x05, 0x0D,	  /*  Usage Page (Digitizer),            */
	0x09, 0x04,	  /*  Usage (Touchscreen),               */
	0xA1, 0x01,	  /*  Collection (Application),          */
	0x85, 0x40,	  /*      Report ID (64),                */
	0x09, 0x42,	  /*      Usage (Tip Switch),            */
	0x15, 0x00,	  /*      Logical Minimum (0),           */
	0x25, 0x01,	  /*      Logical Maximum (1),           */
	0x75, 0x01,	  /*      Report Size (1),               */
	0x95, 0x01,	  /*      Report Count (1),              */
	0x81, 0x02,	  /*      Input (Variable),              */
	0x95, 0x07,	  /*      Report Count (7),              */
	0x81, 0x03,	  /*      Input (Constant, Variable),    */
	0x05, 0x01,	  /*      Usage Page (Desktop),          */
	0x09, 0x30,	  /*      Usage (X),                     */
	0x75, 0x10,	  /*      Report Size (16),              */
	0x95, 0x01,	  /*      Report Count (1),              */
	0xA4,		  /*      Push,                          */
	0x55, 0x0E,	  /*      Unit Exponent (14),            */
	0x65, 0x11,	  /*      Unit (Centimeter),             */
	0x46, 0x76, 0x0B, /*      Physical Maximum (2934),       */
	0x26, 0xFF, 0x7F, /*      Logical Maximum (32767),       */
	0x81, 0x02,	  /*      Input (Variable),              */
	0x09, 0x31,	  /*      Usage (Y),                     */
	0x46, 0x74, 0x06, /*      Physical Maximum (1652),       */
	0x26, 0xFF, 0x7F, /*      Logical Maximum (32767),       */
	0x81, 0x02,	  /*      Input (Variable),              */
	0xB4,		  /*      Pop,                           */
	0xC0,		  /*  End Collection                     */
};

/*
 * Fallback HID descriptor for older devices that do not have
 * the ability to query their HID descriptor.
 */
static const u8 ipts_fallback_descriptor[] = {
	0x05, 0x0D,	  /*  Usage Page (Digitizer),            */
	0x09, 0x0F,	  /*  Usage (Capacitive Hm Digitizer),   */
	0xA1, 0x01,	  /*  Collection (Application),          */
	0x85, 0x41,	  /*      Report ID (65),                */
	0x09, 0x56,	  /*      Usage (Scan Time),             */
	0x95, 0x01,	  /*      Report Count (1),              */
	0x75, 0x10,	  /*      Report Size (16),              */
	0x81, 0x02,	  /*      Input (Variable),              */
	0x09, 0x61,	  /*      Usage (Gesture Char Quality),  */
	0x75, 0x08,	  /*      Report Size (8),               */
	0x96, 0x3D, 0x1D, /*      Report Count (7485),           */
	0x81, 0x03,	  /*      Input (Constant, Variable),    */
	0x85, 0x42,	  /*      Report ID (66),                */
	0x06, 0x00, 0xFF, /*      Usage Page (FF00h),            */
	0x09, 0xC8,	  /*      Usage (C8h),                   */
	0x75, 0x08,	  /*      Report Size (8),               */
	0x95, 0x01,	  /*      Report Count (1),              */
	0xB1, 0x02,	  /*      Feature (Variable),            */
	0xC0,		  /*  End Collection,                    */
};

#endif /* IPTS_DESC_H */
