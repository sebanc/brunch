/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_SPEC_DATA_H
#define IPTS_SPEC_DATA_H

#include <linux/build_bug.h>
#include <linux/types.h>

/**
 * enum ipts_feedback_cmd_type - Commands that can be executed on the sensor through feedback.
 */
enum ipts_feedback_cmd_type {
	IPTS_FEEDBACK_CMD_TYPE_NONE = 0,
	IPTS_FEEDBACK_CMD_TYPE_SOFT_RESET = 1,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_ARMED = 2,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_SENSING = 3,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_SLEEP = 4,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_DOZE = 5,
	IPTS_FEEDBACK_CMD_TYPE_HARD_RESET = 6,
};

/**
 * enum ipts_feedback_data_type - Defines what data a feedback buffer contains.
 * @IPTS_FEEDBACK_DATA_TYPE_VENDOR:        The buffer contains vendor specific feedback.
 * @IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES:  The buffer contains a HID set features report.
 * @IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES:  The buffer contains a HID get features report.
 * @IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT: The buffer contains a HID output report.
 * @IPTS_FEEDBACK_DATA_TYPE_STORE_DATA:    The buffer contains calibration data for the sensor.
 */
enum ipts_feedback_data_type {
	IPTS_FEEDBACK_DATA_TYPE_VENDOR = 0,
	IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES = 1,
	IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES = 2,
	IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT = 3,
	IPTS_FEEDBACK_DATA_TYPE_STORE_DATA = 4,
};

/**
 * struct ipts_feedback_header - Header that is prefixed to the data in a feedback buffer.
 * @cmd_type:   A command that should be executed on the sensor.
 * @size:       The size of the payload to be written.
 * @buffer:     The ID of the buffer that contains this feedback data.
 * @protocol:   The protocol version of the EDS.
 * @data_type:  The type of data that the buffer contains.
 * @spi_offset: The offset at which to write the payload data to the sensor.
 * @payload:    Payload for the feedback command, or 0 if no payload is sent.
 */
struct ipts_feedback_header {
	enum ipts_feedback_cmd_type cmd_type;
	u32 size;
	u32 buffer;
	u32 protocol;
	enum ipts_feedback_data_type data_type;
	u32 spi_offset;
	u8 reserved[40];
	u8 payload[];
} __packed;

static_assert(sizeof(struct ipts_feedback_header) == 64);

/**
 * enum ipts_data_type - Defines what type of data a buffer contains.
 * @IPTS_DATA_TYPE_FRAME:        Raw data frame.
 * @IPTS_DATA_TYPE_ERROR:        Error data.
 * @IPTS_DATA_TYPE_VENDOR:       Vendor specific data.
 * @IPTS_DATA_TYPE_HID:          A HID report.
 * @IPTS_DATA_TYPE_GET_FEATURES: The response to a GET_FEATURES HID2ME command.
 */
enum ipts_data_type {
	IPTS_DATA_TYPE_FRAME = 0x00,
	IPTS_DATA_TYPE_ERROR = 0x01,
	IPTS_DATA_TYPE_VENDOR = 0x02,
	IPTS_DATA_TYPE_HID = 0x03,
	IPTS_DATA_TYPE_GET_FEATURES = 0x04,
	IPTS_DATA_TYPE_DESCRIPTOR = 0x05,
};

/**
 * struct ipts_data_header - Header that is prefixed to the data in a data buffer.
 * @type: What data the buffer contains.
 * @size: How much data the buffer contains.
 * @buffer: Which buffer the data is in.
 */
struct ipts_data_header {
	enum ipts_data_type type;
	u32 size;
	u32 buffer;
	u8 reserved[52];
	u8 data[];
} __packed;

static_assert(sizeof(struct ipts_data_header) == 64);

#endif /* IPTS_SPEC_DATA_H */
