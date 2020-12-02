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
#define IPTS_MEI_UUID UUID_LE(0x3e8d0870, 0x271a, 0x4208, \
		0x8e, 0xb5, 0x9a, 0xcb, 0x94, 0x02, 0xae, 0x04)

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
 * Singletouch mode is a fallback that does not support
 * a stylus or more than one touch input. The data is
 * received as a HID report with report ID 64.
 */
#define IPTS_MODE_SINGLETOUCH 0x0

/*
 * Multitouch mode is the "proper" operation mode for IPTS. It will
 * return stylus data as well as capacitive heatmap touch data.
 * This data needs to be processed in userspace before it can be used.
 */
#define IPTS_MODE_MULTITOUCH 0x1

/*
 * Operation completed successfully.
 */
#define IPTS_STATUS_SUCCESS 0x0

/*
 * Command contained a payload with invalid parameters.
 */
#define IPTS_STATUS_INVALID_PARAMS 0x1

/*
 * ME was unable to validate buffer addresses supplied by the host.
 */
#define IPTS_STATUS_ACCESS_DENIED 0x2

/*
 * Command contained a payload with an invalid size.
 */
#define IPTS_STATUS_CMD_SIZE_ERROR 0x3

/*
 * Buffer addresses have not been set, or the
 * device is not ready for operation yet.
 */
#define IPTS_STATUS_NOT_READY 0x4

/*
 * There is an outstanding command of the same type. The host must
 * wait for a response before sending another command of the same type.
 */
#define IPTS_STATUS_REQUEST_OUTSTANDING 0x5

/*
 * No sensor could be found. Either no sensor is connected, it has not
 * been initialized yet, or the system is improperly configured.
 */
#define IPTS_STATUS_NO_SENSOR_FOUND 0x6

/*
 * Not enough free memory for requested operation.
 */
#define IPTS_STATUS_OUT_OF_MEMORY 0x7

/*
 * An unexpected error occured.
 */
#define IPTS_STATUS_INTERNAL_ERROR 0x8

/*
 * The sensor has been disabled / reset and must be reinitialized.
 */
#define IPTS_STATUS_SENSOR_DISABLED 0x9

/*
 * Compatibility revision check between sensor and ME failed.
 * The host can ignore this error and attempt to continue.
 */
#define IPTS_STATUS_COMPAT_CHECK_FAIL 0xA

/*
 * The sensor went through a reset initiated by the ME / the host.
 */
#define IPTS_STATUS_SENSOR_EXPECTED_RESET 0xB

/*
 * The sensor went through an unexpected reset.
 */
#define IPTS_STATUS_SENSOR_UNEXPECTED_RESET 0xC

/*
 * Requested sensor reset failed to complete.
 */
#define IPTS_STATUS_RESET_FAILED 0xD

/*
 * The operation timed out.
 */
#define IPTS_STATUS_TIMEOUT 0xE

/*
 * Test mode pattern did not match expected values.
 */
#define IPTS_STATUS_TEST_MODE_FAIL 0xF

/*
 * The sensor reported fatal error during reset sequence.
 * Futher progress is not possible.
 */
#define IPTS_STATUS_SENSOR_FAIL_FATAL 0x10

/*
 * The sensor reported fatal error during reset sequence.
 * The host can attempt to continue.
 */
#define IPTS_STATUS_SENSOR_FAIL_NONFATAL 0x11

/*
 * The sensor reported invalid capabilities.
 */
#define IPTS_STATUS_INVALID_DEVICE_CAPS 0x12

/*
 * The command cannot be completed until Quiesce IO flow has completed.
 */
#define IPTS_STATUS_QUIESCE_IO_IN_PROGRESS 0x13

/*
 * The amount of buffers that is used for IPTS
 */
#define IPTS_BUFFERS 16

#define IPTS_WORKQUEUE_SIZE 8192
#define IPTS_WORKQUEUE_ITEM_SIZE 16

/**
 * struct ipts_set_mode_cmd - Payload for the SET_MODE command.
 *
 * @mode: The mode that IPTS should operate in. (IPTS_MODE_*)
 *
 * This driver only supports multitouch mode. Singletouch mode
 * requires a different control flow that is not implemented.
 */
struct ipts_set_mode_cmd {
	u32 mode;
	u8 reserved[12];
} __packed;

/**
 * struct ipts_set_mem_window_cmd - Payload for the SET_MEM_WINDOW command.
 *
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
 * @workqueue_item_size:        Constant value. (IPTS_WORKQUEUE_ITEM_SIZE)
 * @workqueue_size:             Constant value. (IPTS_WORKQUEUE_SIZE)
 *
 * The data buffers are buffers that get filled with touch data by the ME.
 * The doorbell buffer is a u32 that gets incremented by the ME once a data
 * buffer has been filled with new data.
 *
 * The other buffers are required for using GuC submission with binary
 * firmware. Since support for GuC submission has been dropped from i915,
 * they are not used anymore, but they need to be allocated to ensure proper
 * operation.
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
 *
 * @buffer: The buffer that the ME should refill.
 */
struct ipts_feedback_cmd {
	u32 buffer;
	u8 reserved[12];
} __packed;

/**
 * struct ipts_command - A message sent from the host to the ME.
 *
 * @code:    The message code describing the command (IPTS_CMD_*)
 * @payload: Payload for the command, or 0 if no payload is required.
 */
struct ipts_command {
	u32 code;
	u8 payload[320];
} __packed;

/**
 * struct ipts_device_info - Payload for the GET_DEVICE_INFO response.
 *
 * @vendor_id:     Vendor ID of the touch sensor.
 * @device_id:     Device ID of the touch sensor.
 * @hw_rev:        Hardware revision of the touch sensor.
 * @fw_rev:        Firmware revision of the touch sensor.
 * @data_size:     Required size of one data buffer.
 * @feedback_size: Required size of one feedback buffer.
 * @mode:          Current operation mode of IPTS (IPTS_MODE_*)
 * @max_contacts:  The amount of concurrent touches supported by the sensor.
 */
struct ipts_get_device_info_rsp {
	u16 vendor_id;
	u16 device_id;
	u32 hw_rev;
	u32 fw_rev;
	u32 data_size;
	u32 feedback_size;
	u32 mode;
	u8 max_contacts;
	u8 reserved[19];
} __packed;

/**
 * struct ipts_response - A message sent from the ME to the host.
 *
 * @code:    The message code describing the response (IPTS_RSP_*)
 * @status:  The status code returned by the command. (IPTS_STATUS_*)
 * @payload: Payload returned by the command.
 */
struct ipts_response {
	u32 code;
	u32 status;
	u8 payload[80];
} __packed;

#endif /* _IPTS_PROTOCOL_H_ */

