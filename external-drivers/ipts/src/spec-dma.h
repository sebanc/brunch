/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_SPEC_DMA_H
#define IPTS_SPEC_DMA_H

#include <linux/build_bug.h>
#include <linux/types.h>

/**
 * enum ipts_data_type - The type of data that is contained in a data buffer.
 *
 * @IPTS_DATA_TYPE_FRAME:
 *     The buffer contains IPTS data frames, a custom format for storing raw multitouch data
 *     such as capacitive heatmap data or stylus antenna measurements.
 *
 *     They are intended to be used together with vendor provided GPGPU kernels and GuC submission.
 *
 * @IPTS_DATA_TYPE_ERROR:
 *     The buffer contains error data.
 *
 * @IPTS_DATA_TYPE_VENDOR:
 *     The buffer contains vendor specific data.
 *
 * @IPTS_DATA_TYPE_HID:
 *     The buffer contains a HID report.
 *
 * @IPTS_DATA_TYPE_GET_FEATURES:
 *     The buffer contains the response to a HID feature request.
 *
 * @IPTS_DATA_TYPE_HID_DESCRIPTOR:
 *     The buffer contains a HID descriptor.
 */
enum ipts_data_type {
	IPTS_DATA_TYPE_FRAME = 0x00,
	IPTS_DATA_TYPE_ERROR = 0x01,
	IPTS_DATA_TYPE_VENDOR = 0x02,
	IPTS_DATA_TYPE_HID = 0x03,
	IPTS_DATA_TYPE_GET_FEATURES = 0x04,
	IPTS_DATA_TYPE_HID_DESCRIPTOR = 0x05,
};

/**
 * enum ipts_feedback_cmd_type - Commands that can be executed on the sensor through feedback.
 */
enum ipts_feedback_cmd_type {
	IPTS_FEEDBACK_CMD_TYPE_NONE = 0x00,
	IPTS_FEEDBACK_CMD_TYPE_SOFT_RESET = 0x01,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_ARMED = 0x02,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_SENSING = 0x03,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_SLEEP = 0x04,
	IPTS_FEEDBACK_CMD_TYPE_GOTO_DOZE = 0x05,
	IPTS_FEEDBACK_CMD_TYPE_HARD_RESET = 0x06,
};

/**
 * enum ipts_feedback_data_type - The type of data that is contained in a feedback buffer.
 *
 * @IPTS_FEEDBACK_DATA_TYPE_VENDOR:
 *     The buffer contains vendor specific feedback data.
 *
 *     Note: The expected format is unknown, but could be figured out by reverse engineering
 *           the vendor GPGPU kernels from Microsoft.
 *
 * @IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES:
 *     The buffer contains a HID SET_FEATURE report. This means one byte with the
 *     report ID followed by the data to write, as defined by the HID descriptor.
 *
 * @IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES:
 *     The buffer contains a HID GET_FEATURE report. This means one byte with the
 *     report ID, followed by enough free space to write the requested data, as
 *     defined by the HID descriptor.
 *
 * @IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT:
 *     The buffer contains a HID output report. This means one byte with the
 *     report ID followed by the data to write, as defined by the HID descriptor.
 *
 * @IPTS_FEEDBACK_DATA_TYPE_STORE_DATA:
 *     The buffer contains calibration data for the sensor.
 *
 *     Note: The expected format for this is unknown as there is no reference from Intel.
 *           It is most likely vendor specific, just like %IPTS_FEEDBACK_DATA_TYPE_VENDOR.
 */
enum ipts_feedback_data_type {
	IPTS_FEEDBACK_DATA_TYPE_VENDOR = 0x00,
	IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES = 0x01,
	IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES = 0x02,
	IPTS_FEEDBACK_DATA_TYPE_OUTPUT_REPORT = 0x03,
	IPTS_FEEDBACK_DATA_TYPE_STORE_DATA = 0x04,
};

/**
 * struct ipts_data_buffer - A message from the ME to the host through the DMA interface.
 *
 * @type:
 *     The type of the data following this header. See &enum ipts_data_type.
 *
 * @size:
 *     How many bytes of the data are following this header.
 *
 * @total_index:
 *     The index of this message within all of the messages that were sent so far.
 *     Building the modulo between this and the amount of buffers in use will yielt the
 *     index of the buffer that this message was written to.
 *
 * @protocol_ver:
 *     Must match protocol version of the EDS.
 *
 * @vendor_compat_ver:
 *     Version number that is used to indicate which vendor kernel can process this data.
 *     Only relevant when using GuC submission.
 *
 * @reserved1:
 *     Padding to extend header to full 64 bytes and allow for growth.
 *
 * @transaction:
 *     Transaction ID that will be passed back to the ME in &struct ipts_cmd_feedback->transaction.
 *     Used to track round trip of a given transaction for performance measurements.
 *
 * @reserved2:
 *     Padding to extend header to full 64 bytes and allow for growth.
 *
 * @data:
 *     The contents of the buffer after this header.
 */
struct ipts_data_buffer {
	u32 type;
	u32 size;
	u32 total_index;
	u32 protocol_ver;
	u8 vendor_compat_ver;
	u8 reserved1[15];
	u32 transaction;
	u8 reserved2[28];
	u8 data[];
} __packed;

static_assert(sizeof(struct ipts_data_buffer) == 64);

/**
 * struct ipts_feedback_buffer - A message from the host to the ME through the DMA interface.
 *
 * @cmd_type:
 *     The command that the ME should execute on the touch sensor.
 *     See &enum ipts_feedback_cmd_type.
 *
 * @size:
 *     How many bytes of the data are following this header.
 *
 * @total_index:
 *     The index of this message within all of the messages that were sent so far.
 *     See &struct ipts_data_buffer->total_index.
 *
 * @protocol_ver:
 *     Must match the EDS protocol version of the sensor.
 *
 * @data_type:
 *     The type of data following this header. See &enum ipts_feedback_data_type.
 *
 * @spi_offset:
 *     The offset at which the ME should write the data to the touch sensor. Maximum is 0x1EFFF.
 *
 * @reserved:
 *     Padding to extend header to full 64 bytes and allow for growth.
 *
 * @data:
 *     The data that is sent to the ME and written to the touch sensor.
 */
struct ipts_feedback_buffer {
	u32 cmd_type;
	u32 size;
	u32 total_index;
	u32 protocol_ver;
	u32 data_type;
	u32 spi_offset;
	u8 reserved[40];
	u8 data[];
} __packed;

static_assert(sizeof(struct ipts_feedback_buffer) == 64);

#endif /* IPTS_SPEC_DMA_H */
