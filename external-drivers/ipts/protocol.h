/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef _IPTS_PROTOCOL_H_
#define _IPTS_PROTOCOL_H_

#include <linux/types.h>

/*
 * The MEI client ID for IPTS functionality.
 */
#define IPTS_MEI_UUID                                                          \
	UUID_LE(0x3e8d0870, 0x271a, 0x4208, 0x8e, 0xb5, 0x9a, 0xcb, 0x94,      \
		0x02, 0xae, 0x04)

/*
 * Queries the device for vendor specific information.
 *
 * The command must not contain any payload.
 * The response will contain struct ipts_get_device_info_rsp as payload.
 */
#define IPTS_CMD_GET_DEVICE_INFO 0x00000001
#define IPTS_RSP_GET_DEVICE_INFO 0x80000001

/*
 * Sets the mode that IPTS will operate in.
 *
 * The command must contain struct ipts_set_mode_cmd as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_SET_MODE 0x00000002
#define IPTS_RSP_SET_MODE 0x80000002

/*
 * Configures the memory buffers that the ME will use
 * for passing data to the host.
 *
 * The command must contain struct ipts_set_mem_window_cmd as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_SET_MEM_WINDOW 0x00000003
#define IPTS_RSP_SET_MEM_WINDOW 0x80000003

/*
 * Signals that the host is ready to receive data to the ME.
 *
 * The command must not contain any payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_READY_FOR_DATA 0x00000005
#define IPTS_RSP_READY_FOR_DATA 0x80000005

/*
 * Signals that a buffer can be refilled to the ME.
 *
 * The command must contain struct ipts_feedback_cmd as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_FEEDBACK 0x00000006
#define IPTS_RSP_FEEDBACK 0x80000006

/*
 * Resets the data flow from the ME to the hosts and
 * clears the buffers that were set with SET_MEM_WINDOW.
 *
 * The command must not contain any payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_CLEAR_MEM_WINDOW 0x00000007
#define IPTS_RSP_CLEAR_MEM_WINDOW 0x80000007

/*
 * Instructs the ME to reset the touch sensor.
 *
 * The command must contain struct ipts_reset_sensor_cmd as payload.
 * The response will not contain any payload.
 */
#define IPTS_CMD_RESET_SENSOR 0x0000000B
#define IPTS_RSP_RESET_SENSOR 0x8000000B

/**
 * enum ipts_status - Possible status codes returned by IPTS commands.
 * @IPTS_STATUS_SUCCESS:                 Operation completed successfully.
 * @IPTS_STATUS_INVALID_PARAMS:          Command contained a payload with invalid parameters.
 * @IPTS_STATUS_ACCESS_DENIED:           ME could not validate buffer addresses supplied by host.
 * @IPTS_STATUS_CMD_SIZE_ERROR:          Command contains an invalid payload.
 * @IPTS_STATUS_NOT_READY:               Buffer addresses have not been set.
 * @IPTS_STATUS_REQUEST_OUTSTANDING:     There is an outstanding command of the same type.
 *                                       The host must wait for a response before sending another
 *                                       command of the same type.
 * @IPTS_STATUS_NO_SENSOR_FOUND:         No sensor could be found. Either no sensor is connected, it
 *                                       has not been initialized yet, or the system is improperly
 *                                       configured.
 * @IPTS_STATUS_OUT_OF_MEMORY:           Not enough free memory for requested operation.
 * @IPTS_STATUS_INTERNAL_ERROR:          An unexpected error occurred.
 * @IPTS_STATUS_SENSOR_DISABLED:         The sensor has been disabled and must be reinitialized.
 * @IPTS_STATUS_COMPAT_CHECK_FAIL:       Compatibility revision check between sensor and ME failed.
 *                                       The host can ignore this error and attempt to continue.
 * @IPTS_STATUS_SENSOR_EXPECTED_RESET:   The sensor went through a reset initiated by ME or host.
 * @IPTS_STATUS_SENSOR_UNEXPECTED_RESET: The sensor went through an unexpected reset.
 * @IPTS_STATUS_RESET_FAILED:            Requested sensor reset failed to complete.
 * @IPTS_STATUS_TIMEOUT:                 The operation timed out.
 * @IPTS_STATUS_TEST_MODE_FAIL:          Test mode pattern did not match expected values.
 * @IPTS_STATUS_SENSOR_FAIL_FATAL:       The sensor reported a fatal error during reset sequence.
 *                                       Further progress is not possible.
 * @IPTS_STATUS_SENSOR_FAIL_NONFATAL:    The sensor reported a fatal error during reset sequence.
 *                                       The host can attempt to continue.
 * @IPTS_STATUS_INVALID_DEVICE_CAPS:     The device reported invalid capabilities.
 * @IPTS_STATUS_QUIESCE_IO_IN_PROGRESS:  Command cannot be completed until Quiesce IO is done.
 */
enum ipts_status {
	IPTS_STATUS_SUCCESS = 0,
	IPTS_STATUS_INVALID_PARAMS = 1,
	IPTS_STATUS_ACCESS_DENIED = 2,
	IPTS_STATUS_CMD_SIZE_ERROR = 3,
	IPTS_STATUS_NOT_READY = 4,
	IPTS_STATUS_REQUEST_OUTSTANDING = 5,
	IPTS_STATUS_NO_SENSOR_FOUND = 6,
	IPTS_STATUS_OUT_OF_MEMORY = 7,
	IPTS_STATUS_INTERNAL_ERROR = 8,
	IPTS_STATUS_SENSOR_DISABLED = 9,
	IPTS_STATUS_COMPAT_CHECK_FAIL = 10,
	IPTS_STATUS_SENSOR_EXPECTED_RESET = 11,
	IPTS_STATUS_SENSOR_UNEXPECTED_RESET = 12,
	IPTS_STATUS_RESET_FAILED = 13,
	IPTS_STATUS_TIMEOUT = 14,
	IPTS_STATUS_TEST_MODE_FAIL = 15,
	IPTS_STATUS_SENSOR_FAIL_FATAL = 16,
	IPTS_STATUS_SENSOR_FAIL_NONFATAL = 17,
	IPTS_STATUS_INVALID_DEVICE_CAPS = 18,
	IPTS_STATUS_QUIESCE_IO_IN_PROGRESS = 19,
};

/*
 * The amount of buffers that is used for IPTS
 */
#define IPTS_BUFFERS 16

/*
 * The special buffer ID that is used for direct host2me feedback.
 */
#define IPTS_HOST2ME_BUFFER IPTS_BUFFERS

/**
 * enum ipts_mode - Operation mode for IPTS hardware
 * @IPTS_MODE_SINGLETOUCH: Fallback that supports only one finger and no stylus.
 *                         The data is received as a HID report with ID 64.
 * @IPTS_MODE_MULTITOUCH:  The "proper" operation mode for IPTS. It will return
 *                         stylus data as well as capacitive heatmap touch data.
 *                         This data needs to be processed in userspace.
 */
enum ipts_mode {
	IPTS_MODE_SINGLETOUCH = 0,
	IPTS_MODE_MULTITOUCH = 1,
};

/**
 * struct ipts_set_mode_cmd - Payload for the SET_MODE command.
 * @mode: The mode that IPTS should operate in.
 */
struct ipts_set_mode_cmd {
	enum ipts_mode mode;
	u8 reserved[12];
} __packed;

#define IPTS_WORKQUEUE_SIZE	 8192
#define IPTS_WORKQUEUE_ITEM_SIZE 16

/**
 * struct ipts_set_mem_window_cmd - Payload for the SET_MEM_WINDOW command.
 * @data_buffer_addr_lower:     Lower 32 bits of the data buffer addresses.
 * @data_buffer_addr_upper:     Upper 32 bits of the data buffer addresses.
 * @workqueue_addr_lower:       Lower 32 bits of the workqueue buffer address.
 * @workqueue_addr_upper:       Upper 32 bits of the workqueue buffer address.
 * @doorbell_addr_lower:        Lower 32 bits of the doorbell buffer address.
 * @doorbell_addr_upper:        Upper 32 bits of the doorbell buffer address.
 * @feedback_buffer_addr_lower: Lower 32 bits of the feedback buffer addresses.
 * @feedback_buffer_addr_upper: Upper 32 bits of the feedback buffer addresses.
 * @host2me_addr_lower:         Lower 32 bits of the host2me buffer address.
 * @host2me_addr_upper:         Upper 32 bits of the host2me buffer address.
 * @workqueue_item_size:        Magic value. (IPTS_WORKQUEUE_ITEM_SIZE)
 * @workqueue_size:             Magic value. (IPTS_WORKQUEUE_SIZE)
 *
 * The data buffers are buffers that get filled with touch data by the ME.
 * The doorbell buffer is a u32 that gets incremented by the ME once a data
 * buffer has been filled with new data.
 *
 * The other buffers are required for using GuC submission with binary
 * firmware. Since support for GuC submission has been dropped from i915,
 * they are not used anymore, but they need to be allocated and passed,
 * otherwise the hardware will refuse to start.
 */
struct ipts_set_mem_window_cmd {
	u32 data_buffer_addr_lower[IPTS_BUFFERS];
	u32 data_buffer_addr_upper[IPTS_BUFFERS];
	u32 workqueue_addr_lower;
	u32 workqueue_addr_upper;
	u32 doorbell_addr_lower;
	u32 doorbell_addr_upper;
	u32 feedback_buffer_addr_lower[IPTS_BUFFERS];
	u32 feedback_buffer_addr_upper[IPTS_BUFFERS];
	u32 host2me_addr_lower;
	u32 host2me_addr_upper;
	u32 host2me_size;
	u8 reserved1;
	u8 workqueue_item_size;
	u16 workqueue_size;
	u8 reserved[32];
} __packed;

/**
 * struct ipts_feedback_cmd - Payload for the FEEDBACK command.
 * @buffer: The buffer that the ME should refill.
 */
struct ipts_feedback_cmd {
	u32 buffer;
	u8 reserved[12];
} __packed;

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
 * enum ipts_feedback_data_type - Describes the data that a feedback buffer contains.
 * @IPTS_FEEDBACK_DATA_TYPE_VENDOR:        The buffer contains vendor specific feedback.
 * @IPTS_FEEDBACK_DATA_TYPE_SET_FEATURES:  The buffer contains a HID set features command.
 * @IPTS_FEEDBACK_DATA_TYPE_GET_FEATURES:  The buffer contains a HID get features command.
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
 * struct ipts_feedback_buffer - The contents of an IPTS feedback buffer.
 * @cmd_type: A command that should be executed on the sensor.
 * @size: The size of the payload to be written.
 * @buffer: The ID of the buffer that contains this feedback data.
 * @protocol: The protocol version of the EDS.
 * @data_type: The type of payload that the buffer contains.
 * @spi_offset: The offset at which to write the payload data.
 * @payload: Payload for the feedback command, or 0 if no payload is sent.
 */
struct ipts_feedback_buffer {
	enum ipts_feedback_cmd_type cmd_type;
	u32 size;
	u32 buffer;
	u32 protocol;
	enum ipts_feedback_data_type data_type;
	u32 spi_offset;
	u8 reserved[40];
	u8 payload[];
} __packed;

/**
 * enum ipts_reset_type - Possible ways of resetting the touch sensor
 * @IPTS_RESET_TYPE_HARD: Perform hardware reset using GPIO pin.
 * @IPTS_RESET_TYPE_SOFT: Perform software reset using SPI interface.
 */
enum ipts_reset_type {
	IPTS_RESET_TYPE_HARD = 0,
	IPTS_RESET_TYPE_SOFT = 1,
};

/**
 * struct ipts_reset_sensor_cmd - Payload for the RESET_SENSOR command.
 * @type: What type of reset should be performed.
 */
struct ipts_reset_sensor_cmd {
	enum ipts_reset_type type;
	u8 reserved[4];
} __packed;

/**
 * struct ipts_command - A message sent from the host to the ME.
 * @code:    The message code describing the command. (see IPTS_CMD_*)
 * @payload: Payload for the command, or 0 if no payload is required.
 */
struct ipts_command {
	u32 code;
	u8 payload[320];
} __packed;

/**
 * struct ipts_device_info - Payload for the GET_DEVICE_INFO response.
 * @vendor_id:     Vendor ID of the touch sensor.
 * @device_id:     Device ID of the touch sensor.
 * @hw_rev:        Hardware revision of the touch sensor.
 * @fw_rev:        Firmware revision of the touch sensor.
 * @data_size:     Required size of one data buffer.
 * @feedback_size: Required size of one feedback buffer.
 * @mode:          Current operation mode of IPTS.
 * @max_contacts:  The amount of concurrent touches supported by the sensor.
 */
struct ipts_get_device_info_rsp {
	u16 vendor_id;
	u16 device_id;
	u32 hw_rev;
	u32 fw_rev;
	u32 data_size;
	u32 feedback_size;
	enum ipts_mode mode;
	u8 max_contacts;
	u8 reserved[19];
} __packed;

/**
 * struct ipts_feedback_rsp - Payload for the FEEDBACK response.
 * @buffer: The buffer that has received feedback.
 */
struct ipts_feedback_rsp {
	u32 buffer;
} __packed;

/**
 * struct ipts_response - A message sent from the ME to the host.
 * @code:    The message code describing the response. (see IPTS_RSP_*)
 * @status:  The status code returned by the command.
 * @payload: Payload returned by the command.
 */
struct ipts_response {
	u32 code;
	enum ipts_status status;
	u8 payload[80];
} __packed;

#endif /* _IPTS_PROTOCOL_H_ */
