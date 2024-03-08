/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_SPEC_MEI_H
#define IPTS_SPEC_MEI_H

#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/mei.h>
#include <linux/types.h>

/**
 * MEI_UUID_IPTS - The MEI client ID for IPTS functionality.
 */
#define MEI_UUID_IPTS                                                                              \
	UUID_LE(0x3e8d0870, 0x271a, 0x4208, 0x8e, 0xb5, 0x9a, 0xcb, 0x94, 0x02, 0xae, 0x04)

/**
 * IPTS_MAX_BUFFERS - How many buffers can be used by the ME for transferring data.
 */
#define IPTS_MAX_BUFFERS 16

/**
 * IPTS_DIRECTION_BIT - The bit that determines the direction in which a message is going.
 *
 * See &enum ipts_command_code as well as &struct ipts_response->cmd.
 */
#define IPTS_DIRECTION_BIT BIT(31)

/**
 * IPTS_HOST_2_ME_MSG() - Explicitly mark the direction bit of a command code as host to ME.
 */
#define IPTS_HOST_2_ME_MSG(CODE) ((CODE) & ~IPTS_DIRECTION_BIT)

/**
 * IPTS_ME_2_HOST_MSG() - Explicitly mark the direction bit of a command code as ME to host.
 */
#define IPTS_ME_2_HOST_MSG(CODE) ((CODE) | IPTS_DIRECTION_BIT)

/**
 * IPTS_HID_2_ME_BUFFER_INDEX - The index of the HID2ME buffer.
 *
 * The HID2ME buffer is a special feedback buffer, intended for passing HID feature and output
 * reports from a HID interface to the ME.
 *
 * Setting &struct ipts_cmd_feedback->buffer_index to this value indicates that the ME should
 * get feedback data from the HID2ME buffer.
 *
 * HID2ME feedback is only implemented on devices where both, ME and touch sensor, implement EDS
 * interface revision 2. These devices natively support HID and have a queryable HID descriptor.
 */
#define IPTS_HID_2_ME_BUFFER_INDEX IPTS_MAX_BUFFERS

/**
 * IPTS_HID_2_ME_BUFFER_SIZE - The maximum size of the HID2ME buffer.
 */
#define IPTS_HID_2_ME_BUFFER_SIZE 1024

/**
 * IPTS_DEFAULT_WQ_SIZE - Default value of &struct ipts_cmd_set_mem_window->wq_size.
 *
 * This is only relevant when not using GuC submission.
 * When using GuC submission, this value comes from the vendor GPGPU kernel.
 */
#define IPTS_DEFAULT_WQ_SIZE 8192

/**
 * IPTS_DEFAULT_WQ_ITEM_SIZE - Default value of &struct ipts_cmd_set_mem_window->wq_item_size.
 *
 * This is only relevant when not using GuC submission.
 * When using GuC submission, this value comes from the vendor GPGPU kernel.
 */
#define IPTS_DEFAULT_WQ_ITEM_SIZE 16

/**
 * IPTS_DEFAULT_DOZE_TIMER_SECONDS - Default value of &struct ipts_policy->doze_timer.
 */
#define IPTS_DEFAULT_DOZE_TIMER_SECONDS 30

/**
 * enum ipts_command_code - Command codes accepted by the ME.
 *
 * When communicating with the ME, the most significant bit of the command code indicates the
 * direction in which the message is going.
 *
 * MSB == 0 => Host to ME (H2M)
 * MSB == 1 => ME to Host (M2H)
 *
 * These commands can optionally require a payload in the command and return one in the response.
 *
 * @IPTS_CMD_GET_DEVICE_INFO:
 *     Reads information about the device from the ME.
 *
 *     H2M: No payload, M2H: &struct ipts_rsp_get_device_info
 *
 * @IPTS_CMD_SET_MODE:
 *     Sets the operating mode of the ME.
 *
 *     H2M: &struct ipts_cmd_set_mode, M2H: No payload
 *
 * @IPTS_CMD_SET_MEM_WINDOW:
 *     Sets physical buffer addresses for passing data between host and ME.
 *
 *     H2M: &struct ipts_cmd_set_mem_window, M2H: No payload
 *
 * @IPTS_CMD_QUIESCE_IO:
 *     Stops the data flow from ME to host without clearing the buffer addresses.
 *
 *     After sending this command, the host must return any outstanding buffers to the ME using
 *     feedback before a response will be generated.
 *
 *     H2M: &struct ipts_cmd_quiesce_io, M2H: No payload
 *
 * @IPTS_CMD_READY_FOR_DATA:
 *     Informs the ME that the host is ready to receive data.
 *
 *     In event mode, the ME will indicate that new data is available by sending a response to
 *     this command. After processing the data, new data has to be requested by sending this
 *     command again.
 *
 *     In poll mode, this command will not generate a response as long as the data flow is active.
 *     The ME will indicate that new data is available by incrementing the unsigned integer in the
 *     doorbell buffer. This command will return, once the Quiesce IO flow has been completed and
 *     the data flow from ME to host has been stopped.
 *
 *     H2M: No payload, M2H: &struct ipts_rsp_ready_for_data
 *
 * @IPTS_CMD_FEEDBACK:
 *     Submits a feedback buffer to the ME.
 *
 *     Every data buffer has an accompanying feedback buffer that can contain either vendor
 *     specific data or commands that will be executed on the touch sensor.
 *
 *     Once filled, a data buffer will not be touched again by the ME until the host explicitly
 *     returns it to the ME using feedback.
 *
 *     H2M: &struct ipts_cmd_feedback, M2H: &struct ipts_rsp_feedback
 *
 * @IPTS_CMD_CLEAR_MEM_WINDOW:
 *     Stops the data flow from ME to host and clears the buffer addresses.
 *
 *     After sending this command, the host must return any outstanding buffers to the ME using
 *     feedback before a response will be generated.
 *
 *     H2M: No payload, M2H: No payload
 *
 * @IPTS_CMD_NOTIFY_DEV_READY:
 *     Requests that the ME notifies the host when a touch sensor has been detected.
 *
 *     H2M: No payload, M2H: No payload
 *
 * @IPTS_CMD_SET_POLICY:
 *     Sets optional parameters on the ME.
 *
 *     H2M: &struct ipts_cmd_set_policy, M2H: No payload
 *
 * @IPTS_CMD_GET_POLICY:
 *     Reads optional parameters from the ME.
 *
 *     H2M: No payload, M2H: &struct ipts_rsp_get_policy
 *
 * @IPTS_CMD_RESET_SENSOR:
 *     Instructs the ME to reset the touch sensor.
 *
 *     Note: This command will not work on some devices. The sensor will not come back from
 *           resetting and every command will return with an error saying that the sensor has been
 *           reset. A reboot is needed to fix it.
 *
 *     H2M: &struct ipts_cmd_reset_sensor, M2H: No payload
 *
 * @IPTS_CMD_GET_HID_DESC:
 *     Reads the HID descriptor from the ME.
 *
 *     Note: This command was reverse engineered without a reference from Intel.
 *           It is only available on devices that implement EDS v2.
 *
 *     H2M: &struct ipts_cmd_get_hid_desc, M2H: Unknown
 */
enum ipts_command_code {
	IPTS_CMD_GET_DEVICE_INFO = 0x01,
	IPTS_CMD_SET_MODE = 0x02,
	IPTS_CMD_SET_MEM_WINDOW = 0x03,
	IPTS_CMD_QUIESCE_IO = 0x04,
	IPTS_CMD_READY_FOR_DATA = 0x05,
	IPTS_CMD_FEEDBACK = 0x06,
	IPTS_CMD_CLEAR_MEM_WINDOW = 0x07,
	IPTS_CMD_NOTIFY_DEV_READY = 0x08,
	IPTS_CMD_SET_POLICY = 0x09,
	IPTS_CMD_GET_POLICY = 0x0A,
	IPTS_CMD_RESET_SENSOR = 0x0B,
	IPTS_CMD_GET_HID_DESC = 0x0F,
};

/**
 * enum ipts_status - Possible status codes returned by the ME.
 *
 * @IPTS_STATUS_SUCCESS:
 *     Command was processed successfully.
 *
 * @IPTS_STATUS_INVALID_PARAMS:
 *     Input parameters are out of range.
 *
 * @IPTS_STATUS_ACCESS_DENIED:
 *     Unable to map host address ranges for DMA.
 *
 * @IPTS_STATUS_CMD_SIZE_ERROR:
 *     Command sent did not match expected size.
 *
 * @IPTS_STATUS_NOT_READY:
 *     Memory window not set or device is not armed for operation.
 *
 * @IPTS_STATUS_REQUEST_OUTSTANDING:
 *     Previous request is still outstanding, ME firmware cannot handle another request for the
 *     same command.
 *
 * @IPTS_STATUS_NO_SENSOR_FOUND:
 *     No sensor could be found. Either no sensor is connected, the sensor has not yet initialized,
 *     or the system is improperly configured.
 *
 * @IPTS_STATUS_OUT_OF_MEMORY:
 *     Not enough memory/storage for requested operation.
 *
 * @IPTS_STATUS_INTERNAL_ERROR:
 *     Unexpected error occurred.
 *
 * @IPTS_STATUS_SENSOR_DISABLED:
 *     Touch sensor has been disabled or reset and must be reinitialized.
 *
 * @IPTS_STATUS_COMPAT_CHECK_FAIL:
 *     Compatibility revision check between sensor and ME failed. The host can ignore this error
 *     and attempt to continue.
 *
 * @IPTS_STATUS_SENSOR_EXPECTED_RESET:
 *     Sensor signaled a Reset Interrupt. ME either directly requested this reset, or it was
 *     expected as part of a defined flow in the EDS.
 *
 * @IPTS_STATUS_SENSOR_UNEXPECTED_RESET:
 *     Sensor signaled a Reset Interrupt. ME did not expect this and has no info about why this
 *     occurred.
 *
 * @IPTS_STATUS_RESET_FAILED:
 *     Requested sensor reset failed to complete. Sensor generated an invalid or unexpected
 *     interrupt.
 *
 * @IPTS_STATUS_TIMEOUT:
 *     Sensor did not generate a reset interrupt in the time allotted. Could indicate sensor is
 *     not connected or malfunctioning.
 *
 * @IPTS_STATUS_TEST_MODE_FAIL:
 *     Test mode pattern did not match expected values.
 *
 * @IPTS_STATUS_SENSOR_FAIL_FATAL:
 *     Indicates sensor reported fatal error during reset sequence. Further progress is not
 *     possible.
 *
 * @IPTS_STATUS_SENSOR_FAIL_NONFATAL:
 *     Indicates sensor reported non-fatal error during reset sequence. Host should log the error
 *     and attempt to continue.
 *
 * @IPTS_STATUS_INVALID_DEVICE_CAPS:
 *     Indicates sensor does not support minimum required frequency or I/O Mode. ME firmware will
 *     choose the best possible option and the host should attempt to continue.
 *
 * @IPTS_STATUS_QUIESCE_IO_IN_PROGRESS:
 *     Indicates that command cannot be complete until ongoing Quiesce I/O flow has completed.
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
 * enum ipts_mode - The operation mode of the ME.
 *
 * This mode defines, how the ME will notify the host about new data.
 * On EDS v1 devices, it will additionally change the type of data that is sent.
 *
 * @IPTS_MODE_EVENT:
 *      In event mode, the ME indicates new data by producing a response to
 *      IPTS_CMD_READY_FOR_DATA. New data has to be requested by re-sending this command.
 *
 *      On EDS v1 devices, this mode will return fallback singletouch data.
 *
 * @IPTS_MODE_POLL:
 *      In poll mode, the ME indicates new data by incrementing the unsigned integer in the
 *      doorbell buffer. No event will be generated and it is not necessary to expliticly
 *      request new data.
 *
 *      On EDS v1 devices, this mode will return unprocessed multitouch data, for example
 *      capacitive heatmaps or stylus antenna measurements.
 */
enum ipts_mode {
	IPTS_MODE_EVENT = 0x00,
	IPTS_MODE_POLL = 0x01,
};

/**
 * enum ipts_quiesce_flags - Flags that change the behaviour of %IPTS_CMD_QUIESCE_IO.
 *
 * @IPTS_QUIESCE_FLAGS_GUC_RESET:
 *     Indicates that the GuC got reset and the ME has to re-read the tail offset and doorbell.
 */
enum ipts_quiesce_flags {
	IPTS_QUIESCE_FLAGS_GUC_RESET = BIT(0),
};

/**
 * enum ipts_reset_type - Possible ways of resetting the touch sensor.
 *
 * @IPTS_RESET_TYPE_HARD:
 *     Perform hardware reset using GPIO pin.
 *
 * @IPTS_RESET_TYPE_SOFT:
 *     Perform software reset using SPI command.
 */
enum ipts_reset_type {
	IPTS_RESET_TYPE_HARD = 0x00,
	IPTS_RESET_TYPE_SOFT = 0x01,
};

/**
 * enum ipts_reset_reason - Reason why the touch sensor was reset.
 *
 * @IPTS_RESET_REASON_UNKNOWN:
 *     Reason for sensor reset is not known.
 *
 * @IPTS_RESET_REASON_FEEDBACK_REQUEST:
 *     Reset was requested as part of %IPTS_CMD_FEEDBACK.
 *
 * @IPTS_RESET_REASON_MEI_REQUEST:
 *     Reset was requested via %IPTS_CMD_RESET_SENSOR.
 */
enum ipts_reset_reason {
	IPTS_RESET_REASON_UNKNOWN = 0x00,
	IPTS_RESET_REASON_FEEDBACK_REQUEST = 0x01,
	IPTS_RESET_REASON_MEI_REQUEST = 0x02,
};

/**
 * enum ipts_spi_freq - The SPI bus frequency of the touch sensor.
 *
 * @IPTS_SPI_FREQ_RESERVED:
 *     Reserved value.
 *
 * @IPTS_SPI_FREQ_17MHZ:
 *     Sensor set for 17MHz operation.
 *
 * @IPTS_SPI_FREQ_30MHZ:
 *     Sensor set for 30MHz operation.
 */
enum ipts_spi_freq {
	IPTS_SPI_FREQ_RESERVED = 0x00,
	IPTS_SPI_FREQ_17MHZ = 0x01,
	IPTS_SPI_FREQ_30MHZ = 0x02,
};

/**
 * enum ipts_spi_freq_override - Override the default SPI bus frequency of the touch sensor.
 *
 * @IPTS_SPI_FREQ_OVERRIDE_NONE:
 *     Do not apply any override.
 *
 * @IPTS_SPI_FREQ_OVERRIDE_10MHZ:
 *     Force frequency to 10MHz (not currently supported).
 *
 * @IPTS_SPI_FREQ_OVERRIDE_17MHZ:
 *     Force frequency to 17MHz.
 *
 * @IPTS_SPI_FREQ_OVERRIDE_30MHZ:
 *     Force frequency to 30MHz.
 *
 * @IPTS_SPI_FREQ_OVERRIDE_50MHZ:
 *     Force frequency to 50MHz (not currently supported).
 */
enum ipts_spi_freq_override {
	IPTS_SPI_FREQ_OVERRIDE_NONE = 0x00,
	IPTS_SPI_FREQ_OVERRIDE_10MHZ = 0x01,
	IPTS_SPI_FREQ_OVERRIDE_17MHZ = 0x02,
	IPTS_SPI_FREQ_OVERRIDE_30MHZ = 0x03,
	IPTS_SPI_FREQ_OVERRIDE_50MHZ = 0x04,
};

/**
 * enum ipts_spi_io - The SPI IO mode of the touch sensor.
 *
 * @IPTS_SPI_IO_SINGLE:
 *     Sensor set for Single I/O SPI.
 *
 * @IPTS_SPI_IO_DUAL:
 *     Sensor set for Dual I/O SPI.
 *
 * @IPTS_SPI_IO_QUAD:
 *     Sensor set for Quad I/O SPI.
 */
enum ipts_spi_io {
	IPTS_SPI_IO_SINGLE = 0x00,
	IPTS_SPI_IO_DUAL = 0x01,
	IPTS_SPI_IO_QUAD = 0x02,
};

/**
 * enum ipts_spi_io_override - Override the default SPI IO mode of the touch sensor.
 *
 * @IPTS_SPI_IO_MODE_OVERRIDE_NONE:
 *     Do not apply any override.
 *
 * @IPTS_SPI_IO_MODE_OVERRIDE_SINGLE:
 *     Force Single I/O SPI.
 *
 * @IPTS_SPI_IO_MODE_OVERRIDE_DUAL:
 *     Force Dual I/O SPI.
 *
 * @IPTS_SPI_IO_MODE_OVERRIDE_QUAD:
 *     Force Quad I/O SPI.
 */
enum ipts_spi_io_override {
	IPTS_SPI_IO_MODE_OVERRIDE_NONE = 0x00,
	IPTS_SPI_IO_MODE_OVERRIDE_SINGLE = 0x01,
	IPTS_SPI_IO_MODE_OVERRIDE_DUAL = 0x02,
	IPTS_SPI_IO_MODE_OVERRIDE_QUAD = 0x03,
};

/**
 * enum ipts_debug_override - Override the default debug policy of the touch sensor.
 */
enum ipts_debug_override {
	IPTS_DEBUG_OVERRIDE_DISABLE_STARTUP_TIMER = BIT(0),
	IPTS_DEBUG_OVERRIDE_DISABLE_SYNC_BYTE = BIT(1),
	IPTS_DEBUG_OVERRIDE_DISABLE_ERROR_RESET = BIT(2),
};

/**
 * struct ipts_cmd_set_mode - Payload for %IPTS_CMD_SET_MODE.
 *
 * @mode:
 *     The desired sensor mode. See &enum ipts_mode.
 *
 * @reserved:
 *     For future expansion.
 */
struct ipts_cmd_set_mode {
	u32 mode;
	u8 reserved[12];
} __packed;

static_assert(sizeof(struct ipts_cmd_set_mode) == 16);

/**
 * struct ipts_cmd_set_mem_window - Payload for %IPTS_CMD_SET_MEM_WINDOW.
 *
 * @data_addr_lower:
 *     Lower 32 bits of the physical address to the touch data buffer.
 *     Size of each buffer should be &struct ipts_rsp_get_device_info->data_size.
 *
 * @data_addr_upper:
 *     Upper 32 bits of the physical address to the touch data buffer.
 *     Size of each buffer should be &struct ipts_rsp_get_device_info->data_size.
 *
 * @tail_offset_addr_lower:
 *     Lower 32 bits of the physical address to the tail offset.
 *     Size of the buffer is 32 bit, incremented by &struct ipts_cmd_set_mem_window->wq_item_size.
 *
 *     Note: This buffer is required for using vendor GPGPU kernels through GuC submission.
 *           Without using GuC submission, this still must be allocated and passed to the ME,
 *           otherwise the command will return an error.
 *
 * @tail_offset_addr_upper:
 *     Upper 32 bits of the physical address to the tail offset.
 *     Size of the buffer is 32 bit, incremented by &struct ipts_cmd_set_mem_window->wq_item_size.
 *
 *     Note: This buffer is required for using vendor GPGPU kernels through GuC submission.
 *           Without using GuC submission, this still must be allocated and passed to the ME,
 *           otherwise the command will return an error.
 *
 * @doorbell_addr_lower:
 *     Lower 32 bits of the physical address to the doorbell register.
 *     Size of the buffer is 32 bit, incremented as unsigned integer, rolls over to 1.
 *
 * @doorbell_addr_upper:
 *     Upper 32 bits of the physical address to the doorbell register.
 *     Size of the buffer is 32 bit, incremented as unsigned integer, rolls over to 1.
 *
 * @feedback_addr_lower:
 *     Lower 32 bits of the physical address to the feedback buffer.
 *     Size of each buffer should be &struct ipts_rsp_get_device_info->feedback_size.
 *
 * @feedback_addr_upper:
 *     Upper 32 bits of the physical address to the feedback buffer.
 *     Size of each buffer should be &struct ipts_rsp_get_device_info->feedback_size.
 *
 * @hid2me_addr_lower:
 *     Lower 32 bits of the physical address to the dedicated HID to ME buffer.
 *     Size of the buffer is &struct ipts_cmd_set_mem_window->hid2me_size.
 *
 * @hid2me_addr_upper:
 *     Upper 32 bits of the physical address to the dedicated HID to ME buffer.
 *     Size of the buffer is &struct ipts_cmd_set_mem_window->hid2me_size.
 *
 * @hid2me_size:
 *     Size in bytes of the HID to ME buffer, cannot be bigger than %IPTS_HID_2_ME_BUFFER_SIZE.
 *
 * @reserved1:
 *     For future expansion.
 *
 * @wq_item_size:
 *     Size in bytes of the workqueue item pointed to by the tail offset.
 *
 *     Note: This field is required for using vendor GPGPU kernels through GuC submission.
 *           Without using GuC submission, this must be set to %IPTS_DEFAULT_WQ_ITEM_SIZE.
 *
 * @wq_size:
 *     Size in bytes of the entire work queue.
 *
 *     Note: This field is required for using vendor GPGPU kernels through GuC submission.
 *           Without using GuC submission, this must be set to %IPTS_DEFAULT_WQ_SIZE.
 *
 * @reserved2:
 *     For future expansion.
 */
struct ipts_cmd_set_mem_window {
	u32 data_addr_lower[IPTS_MAX_BUFFERS];
	u32 data_addr_upper[IPTS_MAX_BUFFERS];
	u32 tail_offset_addr_lower;
	u32 tail_offset_addr_upper;
	u32 doorbell_addr_lower;
	u32 doorbell_addr_upper;
	u32 feedback_addr_lower[IPTS_MAX_BUFFERS];
	u32 feedback_addr_upper[IPTS_MAX_BUFFERS];
	u32 hid2me_addr_lower;
	u32 hid2me_addr_upper;
	u32 hid2me_size;
	u8 reserved1;
	u8 wq_item_size;
	u16 wq_size;
	u8 reserved2[32];
} __packed;

static_assert(sizeof(struct ipts_cmd_set_mem_window) == 320);

/**
 * struct ipts_cmd_quiesce_io - Payload for %IPTS_CMD_QUIESCE_IO.
 *
 * @flags:
 *     Optionally set flags that modify the commands behaviour. See &enum ipts_quiesce_flags.
 *
 * @reserved:
 *     For future expansion.
 */
struct ipts_cmd_quiesce_io {
	u32 flags;
	u8 reserved[8];
} __packed;

static_assert(sizeof(struct ipts_cmd_quiesce_io) == 12);

/**
 * struct ipts_cmd_feedback - Payload for %IPTS_CMD_FEEDBACK.
 *
 * @buffer_index:
 *     Index value from 0 to %IPTS_MAX_BUFFERS used to indicate which feedback buffer to use.
 *     Using special value %IPTS_HID_2_ME_BUFFER_INDEX is an indication to the ME to get feedback
 *     data from the HID to ME buffer instead of one of the standard feedback buffers.
 *
 * @reserved1:
 *     For future expansion.
 *
 * @transaction:
 *     Transaction ID that was passed to the host in &struct ipts_data_buffer->transaction. Used to
 *     track round trip of a given transaction for performance measurements.
 *
 * @reserved2:
 *     For future expansion.
 */
struct ipts_cmd_feedback {
	u8 buffer_index;
	u8 reserved1[3];
	u32 transaction;
	u8 reserved2[8];
} __packed;

static_assert(sizeof(struct ipts_cmd_feedback) == 16);

/**
 * struct ipts_policy - Optional parameters for ME operation.
 *
 * @reserved1:
 *     For future expansion.
 *
 * @doze_timer:
 *     Value in seconds, after which the ME will put the sensor into doze power state if no
 *     activity occurs. Set to 0 to disable doze mode (not recommended). Value will be set to
 *     %IPTS_DEFAULT_DOZE_TIMER_SECONDS by default.
 *
 * @spi_freq_override:
 *     Override SPI frequency requested by sensor. See &enum ipts_spi_freq_override.
 *
 * @spi_io_mode_override:
 *     Override SPI IO mode requested by sensor. See &enum ipts_spi_io_override.
 *
 * @reserved2:
 *     For future expansion.
 *
 * @debug_override:
 *     Normally all bits will be zero. Set bits as needed for enabling special debug features.
 *     See &enum ipts_debug_override.
 */
struct ipts_policy {
	u8 reserved1[4];
	u16 doze_timer;
	u8 spi_freq_override : 3;
	u8 spi_io_mode_override : 3;
	u64 reserved2 : 42;
	u32 debug_override;
} __packed;

static_assert(sizeof(struct ipts_policy) == 16);

/**
 * struct ipts_cmd_set_policy - Payload for %IPTS_CMD_SET_POLICY.
 *
 * @policy:
 *     The desired policy to apply.
 */
struct ipts_cmd_set_policy {
	struct ipts_policy policy;
} __packed;

static_assert(sizeof(struct ipts_cmd_set_policy) == 16);

/**
 * struct ipts_cmd_reset_sensor - Payload for %IPTS_CMD_RESET_SENSOR.
 *
 * @type:
 *     How the ME should reset the sensor. See &enum ipts_reset_type.
 *
 * @reserved:
 *     For future expansion.
 */
struct ipts_cmd_reset_sensor {
	u32 type;
	u8 reserved[4];
} __packed;

static_assert(sizeof(struct ipts_cmd_reset_sensor) == 8);

/**
 * struct ipts_cmd_get_hid_desc - Payload for %IPTS_CMD_GET_HID_DESC.
 *
 * Note:
 *     This command is only implemented on EDS v2 devices.
 *
 * @addr_lower:
 *     Lower 32 bits of the physical address to the HID descriptor buffer.
 *     Size of the buffer should be &struct ipts_rsp_get_device_info->data_size + 8.
 *
 * @addr_upper:
 *     Upper 32 bits of the physical address to the HID descriptor buffer.
 *     Size of the buffer should be &struct ipts_rsp_get_device_info->data_size + 8.
 *
 * @magic:
 *     A magic value with unknown significance. Must be set to 8.
 *
 * @reserved:
 *     For future expansion.
 */
struct ipts_cmd_get_hid_desc {
	u32 addr_lower;
	u32 addr_upper;
	u32 magic;
	u8 reserved[12];
} __packed;

static_assert(sizeof(struct ipts_cmd_get_hid_desc) == 24);

/**
 * struct ipts_command - Structure that represents a message going from host to ME.
 *
 * @cmd:
 *     The code of the command that will be executed. See &enum ipts_command_code.
 */
struct ipts_command {
	u32 cmd;
	union {
		struct ipts_cmd_set_mode set_mode;
		struct ipts_cmd_set_mem_window set_mem_window;
		struct ipts_cmd_quiesce_io quiesce_io;
		struct ipts_cmd_feedback feedback;
		struct ipts_cmd_set_policy set_policy;
		struct ipts_cmd_reset_sensor reset_sensor;
		struct ipts_cmd_get_hid_desc get_hid_desc;
		u8 raw[320];
	} payload;
} __packed;

static_assert(sizeof(struct ipts_command) == 324);

/**
 * struct ipts_rsp_get_device_info - Payload of the response to %IPTS_CMD_GET_DEVICE_INFO.
 *
 * @vendor:
 *     The vendor ID of the touch sensor.
 *
 * @product:
 *     The product ID of the touch sensor.
 *
 * @hw_rev:
 *     Touch sensor hardware revision.
 *
 * @fw_rev:
 *     Touch sensor firmware revision.
 *
 * @data_size:
 *     The maximum amount of data returned by the touch sensor in bytes. This data will be
 *     &struct ipts_data_buffer followed by raw data or a HID structure.
 *
 * @feedback_size:
 *     Maximum size of one feedback structure in bytes.
 *
 * @sensor_mode:
 *     Current operating mode of the sensor. See &enum ipts_mode.
 *
 * @max_touch_points:
 *     Maximum number of simultaneous touch points that can be reported by sensor.
 *
 * @spi_frequency:
 *     SPI frequency supported by sensor and ME firmware. See &enum ipts_spi_freq.
 *
 * @spi_io_mode:
 *     SPI I/O mode supported by sensor and ME firmware. See &enum ipts_spi_io_mode.
 *
 * @reserved1:
 *     For future expansion.
 *
 * @sensor_minor_eds_rev:
 *     Minor version number of EDS spec supported by sensor.
 *
 * @sensor_major_eds_rev:
 *     Major version number of EDS spec supported by sensor.
 *
 * @me_minor_eds_rev:
 *     Minor version number of EDS spec supported by ME.
 *
 * @me_major_eds_rev:
 *     Major version number of EDS spec supported by ME.
 *
 * @sensor_eds_intf_rev:
 *     EDS interface revision supported by sensor.
 *
 * @me_eds_intf_rev:
 *     EDS interface revision supported by ME.
 *
 * @vendor_compat_ver:
 *     Vendor specific version number that indicates compatibility with GPGPU kernels.
 *     Only relevant when using GuC submission.
 *
 * @reserved2:
 *     For future expansion.
 */
struct ipts_rsp_get_device_info {
	u16 vendor;
	u16 product;
	u32 hw_rev;
	u32 fw_rev;
	u32 data_size;
	u32 feedback_size;
	u32 sensor_mode;
	u8 max_touch_points;
	u8 spi_frequency;
	u8 spi_io_mode;
	u8 reserved1;
	u8 sensor_minor_eds_rev;
	u8 sensor_major_eds_rev;
	u8 me_minor_eds_rev;
	u8 me_major_eds_rev;
	u8 sensor_eds_intf_rev;
	u8 me_eds_intf_rev;
	u8 vendor_compat_ver;
	u8 reserved2[9];
} __packed;

static_assert(sizeof(struct ipts_rsp_get_device_info) == 44);

/**
 * struct ipts_rsp_ready_for_data - Payload of the response to %IPTS_CMD_READY_FOR_DATA.
 *
 * @size:
 *     How many bytes the ME wrote into the data buffer.
 *
 * @buffer_index:
 *     The index of the buffer that the ME filled with data.
 *
 * @reset_reason:
 *     If the status of the response is %IPTS_STATUS_SENSOR_EXPECTED_RESET, the ME will provide the
 *     reason for the reset. See &enum ipts_reset_reason.
 *
 * @reserved:
 *     For future expansion.
 */
struct ipts_rsp_ready_for_data {
	u32 size;
	u8 buffer_index;
	u8 reset_reason;
	u8 reserved[22];
} __packed;

static_assert(sizeof(struct ipts_rsp_ready_for_data) == 28);

/**
 * struct ipts_rsp_feedback - Payload of the response to %IPTS_CMD_FEEDBACK.
 *
 * @feedback_index:
 *     Index value from 0 to %IPTS_MAX_BUFFERS used to indicate which feedback buffer was used.
 */
struct ipts_rsp_feedback {
	u8 feedback_index;
	u8 reserved[27];
} __packed;

static_assert(sizeof(struct ipts_rsp_feedback) == 28);

/**
 * struct ipts_rsp_get_policy - Payload of the response to %IPTS_GET_POLICY.
 *
 * @policy:
 *     The current policy.
 */
struct ipts_rsp_get_policy {
	struct ipts_policy policy;
} __packed;

static_assert(sizeof(struct ipts_rsp_get_policy) == 16);

/**
 * struct ipts_response - Structure that represents a message going from ME to host.
 *
 * @cmd:
 *     The code of the command that was executed. See &enum ipts_command_code.
 *
 * @status:
 *     The status of the sensor after executing the command. See &enum ipts_status.
 */
struct ipts_response {
	u32 cmd;
	u32 status;
	union {
		struct ipts_rsp_get_device_info get_device_info;
		struct ipts_rsp_ready_for_data ready_for_data;
		struct ipts_rsp_feedback feedback;
		struct ipts_rsp_get_policy get_policy;
		u8 raw[80];
	} payload;
} __packed;

static_assert(sizeof(struct ipts_response) == 88);

#endif /* IPTS_SPEC_MEI_H */
