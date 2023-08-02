/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_SPEC_DEVICE_H
#define IPTS_SPEC_DEVICE_H

#include <linux/build_bug.h>
#include <linux/types.h>

/*
 * The amount of buffers that IPTS can use for data transfer.
 */
#define IPTS_BUFFERS 16

/*
 * The buffer ID that is used for HID2ME feedback
 */
#define IPTS_HID2ME_BUFFER IPTS_BUFFERS

/**
 * enum ipts_command - Commands that can be sent to the IPTS hardware.
 * @IPTS_CMD_GET_DEVICE_INFO:  Retrieves vendor information from the device.
 * @IPTS_CMD_SET_MODE:         Changes the mode that the device will operate in.
 * @IPTS_CMD_SET_MEM_WINDOW:   Configures memory buffers for passing data between device and driver.
 * @IPTS_CMD_QUIESCE_IO:       Stops the data flow from the device to the driver.
 * @IPTS_CMD_READY_FOR_DATA:   Informs the device that the driver is ready to receive data.
 * @IPTS_CMD_FEEDBACK:         Informs the device that a buffer was processed and can be refilled.
 * @IPTS_CMD_CLEAR_MEM_WINDOW: Stops the data flow and clears the buffer addresses on the device.
 * @IPTS_CMD_RESET_SENSOR:     Resets the sensor to its default state.
 * @IPTS_CMD_GET_DESCRIPTOR:   Retrieves the HID descriptor of the device.
 */
enum ipts_command_code {
	IPTS_CMD_GET_DEVICE_INFO = 0x01,
	IPTS_CMD_SET_MODE = 0x02,
	IPTS_CMD_SET_MEM_WINDOW = 0x03,
	IPTS_CMD_QUIESCE_IO = 0x04,
	IPTS_CMD_READY_FOR_DATA = 0x05,
	IPTS_CMD_FEEDBACK = 0x06,
	IPTS_CMD_CLEAR_MEM_WINDOW = 0x07,
	IPTS_CMD_RESET_SENSOR = 0x0B,
	IPTS_CMD_GET_DESCRIPTOR = 0x0F,
};

/**
 * enum ipts_status - Possible status codes returned by the IPTS device.
 * @IPTS_STATUS_SUCCESS:                 Operation completed successfully.
 * @IPTS_STATUS_INVALID_PARAMS:          Command contained an invalid payload.
 * @IPTS_STATUS_ACCESS_DENIED:           ME could not validate a buffer address.
 * @IPTS_STATUS_CMD_SIZE_ERROR:          Command contains an invalid payload.
 * @IPTS_STATUS_NOT_READY:               Buffer addresses have not been set.
 * @IPTS_STATUS_REQUEST_OUTSTANDING:     There is an outstanding command of the same type.
 * @IPTS_STATUS_NO_SENSOR_FOUND:         No sensor could be found.
 * @IPTS_STATUS_OUT_OF_MEMORY:           Not enough free memory for requested operation.
 * @IPTS_STATUS_INTERNAL_ERROR:          An unexpected error occurred.
 * @IPTS_STATUS_SENSOR_DISABLED:         The sensor has been disabled and must be reinitialized.
 * @IPTS_STATUS_COMPAT_CHECK_FAIL:       Compatibility revision check between sensor and ME failed.
 *                                       The host can ignore this error and attempt to continue.
 * @IPTS_STATUS_SENSOR_EXPECTED_RESET:   The sensor went through a reset initiated by the driver.
 * @IPTS_STATUS_SENSOR_UNEXPECTED_RESET: The sensor went through an unexpected reset.
 * @IPTS_STATUS_RESET_FAILED:            Requested sensor reset failed to complete.
 * @IPTS_STATUS_TIMEOUT:                 The operation timed out.
 * @IPTS_STATUS_TEST_MODE_FAIL:          Test mode pattern did not match expected values.
 * @IPTS_STATUS_SENSOR_FAIL_FATAL:       The sensor reported an error during reset sequence.
 *                                       Further progress is not possible.
 * @IPTS_STATUS_SENSOR_FAIL_NONFATAL:    The sensor reported an error during reset sequence.
 *                                       The driver can attempt to continue.
 * @IPTS_STATUS_INVALID_DEVICE_CAPS:     The device reported invalid capabilities.
 * @IPTS_STATUS_QUIESCE_IO_IN_PROGRESS:  Command cannot be completed until Quiesce IO is done.
 */
enum ipts_status {
	IPTS_STATUS_SUCCESS = 0x00,
	IPTS_STATUS_INVALID_PARAMS = 0x01,
	IPTS_STATUS_ACCESS_DENIED = 0x02,
	IPTS_STATUS_CMD_SIZE_ERROR = 0x03,
	IPTS_STATUS_NOT_READY = 0x04,
	IPTS_STATUS_REQUEST_OUTSTANDING = 0x05,
	IPTS_STATUS_NO_SENSOR_FOUND = 0x06,
	IPTS_STATUS_OUT_OF_MEMORY = 0x07,
	IPTS_STATUS_INTERNAL_ERROR = 0x08,
	IPTS_STATUS_SENSOR_DISABLED = 0x09,
	IPTS_STATUS_COMPAT_CHECK_FAIL = 0x0A,
	IPTS_STATUS_SENSOR_EXPECTED_RESET = 0x0B,
	IPTS_STATUS_SENSOR_UNEXPECTED_RESET = 0x0C,
	IPTS_STATUS_RESET_FAILED = 0x0D,
	IPTS_STATUS_TIMEOUT = 0x0E,
	IPTS_STATUS_TEST_MODE_FAIL = 0x0F,
	IPTS_STATUS_SENSOR_FAIL_FATAL = 0x10,
	IPTS_STATUS_SENSOR_FAIL_NONFATAL = 0x11,
	IPTS_STATUS_INVALID_DEVICE_CAPS = 0x12,
	IPTS_STATUS_QUIESCE_IO_IN_PROGRESS = 0x13,
};

/**
 * struct ipts_command - Message that is sent to the device for calling a command.
 * @cmd:     The command that will be called.
 * @payload: Payload containing parameters for the called command.
 */
struct ipts_command {
	enum ipts_command_code cmd;
	u8 payload[320];
} __packed;

static_assert(sizeof(struct ipts_command) == 324);

/**
 * enum ipts_mode - Configures what data the device produces and how its sent.
 * @IPTS_MODE_EVENT:    The device will send an event once a buffer was filled.
 *                      Older devices will return singletouch data in this mode.
 * @IPTS_MODE_DOORBELL: The device will notify the driver by incrementing the doorbell value.
 *                      Older devices will return multitouch data in this mode.
 */
enum ipts_mode {
	IPTS_MODE_EVENT = 0x00,
	IPTS_MODE_DOORBELL = 0x01,
};

/**
 * struct ipts_set_mode - Payload for the SET_MODE command.
 * @mode: Changes the mode that IPTS will operate in.
 */
struct ipts_set_mode {
	enum ipts_mode mode;
	u8 reserved[12];
} __packed;

static_assert(sizeof(struct ipts_set_mode) == 16);

#define IPTS_WORKQUEUE_SIZE	 8192
#define IPTS_WORKQUEUE_ITEM_SIZE 16

/**
 * struct ipts_mem_window - Payload for the SET_MEM_WINDOW command.
 * @data_addr_lower:      Lower 32 bits of the data buffer addresses.
 * @data_addr_upper:      Upper 32 bits of the data buffer addresses.
 * @workqueue_addr_lower: Lower 32 bits of the workqueue buffer address.
 * @workqueue_addr_upper: Upper 32 bits of the workqueue buffer address.
 * @doorbell_addr_lower:  Lower 32 bits of the doorbell buffer address.
 * @doorbell_addr_upper:  Upper 32 bits of the doorbell buffer address.
 * @feedbackaddr_lower:   Lower 32 bits of the feedback buffer addresses.
 * @feedbackaddr_upper:   Upper 32 bits of the feedback buffer addresses.
 * @hid2me_addr_lower:    Lower 32 bits of the hid2me buffer address.
 * @hid2me_addr_upper:    Upper 32 bits of the hid2me buffer address.
 * @hid2me_size:          Size of the hid2me feedback buffer.
 * @workqueue_item_size:  Magic value. Must be 16.
 * @workqueue_size:       Magic value. Must be 8192.
 *
 * The workqueue related items in this struct are required for using
 * GuC submission with binary processing firmware. Since this driver does
 * not use GuC submission and instead exports raw data to userspace, these
 * items are not actually used, but they need to be allocated and passed
 * to the device, otherwise initialization will fail.
 */
struct ipts_mem_window {
	u32 data_addr_lower[IPTS_BUFFERS];
	u32 data_addr_upper[IPTS_BUFFERS];
	u32 workqueue_addr_lower;
	u32 workqueue_addr_upper;
	u32 doorbell_addr_lower;
	u32 doorbell_addr_upper;
	u32 feedback_addr_lower[IPTS_BUFFERS];
	u32 feedback_addr_upper[IPTS_BUFFERS];
	u32 hid2me_addr_lower;
	u32 hid2me_addr_upper;
	u32 hid2me_size;
	u8 reserved1;
	u8 workqueue_item_size;
	u16 workqueue_size;
	u8 reserved[32];
} __packed;

static_assert(sizeof(struct ipts_mem_window) == 320);

/**
 * struct ipts_quiesce_io - Payload for the QUIESCE_IO command.
 */
struct ipts_quiesce_io {
	u8 reserved[12];
} __packed;

static_assert(sizeof(struct ipts_quiesce_io) == 12);

/**
 * struct ipts_feedback - Payload for the FEEDBACK command.
 * @buffer: The buffer that the device should refill.
 */
struct ipts_feedback {
	u32 buffer;
	u8 reserved[12];
} __packed;

static_assert(sizeof(struct ipts_feedback) == 16);

/**
 * enum ipts_reset_type - Possible ways of resetting the device.
 * @IPTS_RESET_TYPE_HARD: Perform hardware reset using GPIO pin.
 * @IPTS_RESET_TYPE_SOFT: Perform software reset using SPI command.
 */
enum ipts_reset_type {
	IPTS_RESET_TYPE_HARD = 0x00,
	IPTS_RESET_TYPE_SOFT = 0x01,
};

/**
 * struct ipts_reset - Payload for the RESET_SENSOR command.
 * @type: How the device should get reset.
 */
struct ipts_reset_sensor {
	enum ipts_reset_type type;
	u8 reserved[4];
} __packed;

static_assert(sizeof(struct ipts_reset_sensor) == 8);

/**
 * struct ipts_get_descriptor - Payload for the GET_DESCRIPTOR command.
 * @addr_lower: The lower 32 bits of the descriptor buffer address.
 * @addr_upper: The upper 32 bits of the descriptor buffer address.
 * @magic:      A magic value. Must be 8.
 */
struct ipts_get_descriptor {
	u32 addr_lower;
	u32 addr_upper;
	u32 magic;
	u8 reserved[12];
} __packed;

static_assert(sizeof(struct ipts_get_descriptor) == 24);

/*
 * The type of a response is indicated by a
 * command code, with the most significant bit flipped to 1.
 */
#define IPTS_RSP_BIT BIT(31)

/**
 * struct ipts_response - Data returned from the device in response to a command.
 * @cmd:     The command that this response answers (IPTS_RSP_BIT will be 1).
 * @status:  The return code of the command.
 * @payload: The data that was produced by the command.
 */
struct ipts_response {
	enum ipts_command_code cmd;
	enum ipts_status status;
	u8 payload[80];
} __packed;

static_assert(sizeof(struct ipts_response) == 88);

/**
 * struct ipts_device_info - Vendor information of the IPTS device.
 * @vendor:        Vendor ID of this device.
 * @product:       Product ID of this device.
 * @hw_version:    Hardware revision of this device.
 * @fw_version:    Firmware revision of this device.
 * @data_size:     Requested size for a data buffer.
 * @feedback_size: Requested size for a feedback buffer.
 * @mode:          Mode that the device currently operates in.
 * @max_contacts:  Maximum amount of concurrent touches the sensor can process.
 */
struct ipts_device_info {
	u16 vendor;
	u16 product;
	u32 hw_version;
	u32 fw_version;
	u32 data_size;
	u32 feedback_size;
	enum ipts_mode mode;
	u8 max_contacts;
	u8 reserved1[3];
	u8 sensor_min_eds;
	u8 sensor_maj_eds;
	u8 me_min_eds;
	u8 me_maj_eds;
	u8 intf_eds;
	u8 reserved2[11];
} __packed;

static_assert(sizeof(struct ipts_device_info) == 44);

#endif /* IPTS_SPEC_DEVICE_H */
