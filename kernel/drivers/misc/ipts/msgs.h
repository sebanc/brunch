/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _IPTS_MSGS_H_
#define _IPTS_MSGS_H_

#define TOUCH_SENSOR_GET_DEVICE_INFO_CMD		0x00000001
#define TOUCH_SENSOR_GET_DEVICE_INFO_RSP		0x80000001
#define TOUCH_SENSOR_SET_MODE_CMD			0x00000002
#define TOUCH_SENSOR_SET_MODE_RSP			0x80000002
#define TOUCH_SENSOR_SET_MEM_WINDOW_CMD			0x00000003
#define TOUCH_SENSOR_SET_MEM_WINDOW_RSP			0x80000003
#define TOUCH_SENSOR_QUIESCE_IO_CMD			0x00000004
#define TOUCH_SENSOR_QUIESCE_IO_RSP			0x80000004
#define TOUCH_SENSOR_HID_READY_FOR_DATA_CMD		0x00000005
#define TOUCH_SENSOR_HID_READY_FOR_DATA_RSP		0x80000005
#define TOUCH_SENSOR_FEEDBACK_READY_CMD			0x00000006
#define TOUCH_SENSOR_FEEDBACK_READY_RSP			0x80000006
#define TOUCH_SENSOR_CLEAR_MEM_WINDOW_CMD		0x00000007
#define TOUCH_SENSOR_CLEAR_MEM_WINDOW_RSP		0x80000007
#define TOUCH_SENSOR_NOTIFY_DEV_READY_CMD		0x00000008
#define TOUCH_SENSOR_NOTIFY_DEV_READY_RSP		0x80000008
#define TOUCH_SENSOR_SET_POLICIES_CMD			0x00000009
#define TOUCH_SENSOR_SET_POLICIES_RSP			0x80000009
#define TOUCH_SENSOR_GET_POLICIES_CMD			0x0000000A
#define TOUCH_SENSOR_GET_POLICIES_RSP			0x8000000A
#define TOUCH_SENSOR_RESET_CMD				0x0000000B
#define TOUCH_SENSOR_RESET_RSP				0x8000000B
#define TOUCH_SENSOR_READ_ALL_REGS_CMD			0x0000000C
#define TOUCH_SENSOR_READ_ALL_REGS_RSP			0x8000000C
#define TOUCH_SENSOR_CMD_ERROR_RSP			0x8FFFFFFF

typedef union touch_sts_reg
{
	u32 reg_value;
	struct
	{
		u32 int_status				:1;
		u32 int_type				:4;
		u32 pwr_state				:2;
		u32 init_state				:2;
		u32 busy				:1;
		u32 reserved				:14;
		u32 sync_byte				:8;
	} fields;
} touch_sts_reg_t;

typedef union touch_frame_char_reg
{
	u32 reg_value;
	struct
	{
		u32 microframe_size			:18;
		u32 microframes_per_frame		:5;
		u32 microframe_index			:5;
		u32 hid_report				:1;
		u32 reserved				:3;
	} fields;
} touch_frame_char_reg_t;

typedef union touch_err_reg
{
	u32 reg_value;
	struct
	{
		u32 invalid_fw				:1;
		u32 invalid_data			:1;
		u32 self_test_failed			:1;
		u32 reserved				:12;
		u32 fatal_error				:1;
		u32 vendor_errors			:16;
	} fields;
} touch_err_reg_t;

typedef u32  touch_id_reg_t;

typedef union touch_data_sz_reg
{
	u32 reg_value;
	struct
	{
		u32 max_frame_size			:12;
		u32 max_feedback_size			:8;
		u32 reserved				:12;
	} fields;
} touch_data_sz_reg_t;

typedef union touch_caps_reg
{
	u32 reg_value;
	struct
	{
		u32 reserved0				:1;
		u32 supported_17Mhz			:1;
		u32 supported_30Mhz			:1;
		u32 supported_50Mhz			:1;
		u32 reserved1				:4;
		u32 supported_single_io			:1;
		u32 supported_dual_io			:1;
		u32 supported_quad_io			:1;
		u32 bulk_data_max_write			:6;
		u32 read_delay_timer_value		:3;
		u32 reserved2				:4;
		u32 max_touch_points			:8;
	} fields;
} touch_caps_reg_t;

typedef union touch_cfg_reg
{
	u32 reg_value;
	struct
	{
		u32 touch_enable			:1;
		u32 dhpm				:1;
		u32 bulk_xfer_size			:4;
		u32 freq_select				:3;
		u32 reserved				:23;
	} fields;
} touch_cfg_reg_t;

typedef union touch_cmd_reg
{
	u32 reg_value;
	struct
	{
		u32 command_code			:8;
		u32 reserved				:24;
	} fields;
} touch_cmd_reg_t;

typedef union touch_pwr_mgmt_ctrl_reg
{
	u32 reg_value;
	struct
	{
		u32 pwr_state_cmd			:3;
		u32 reserved				:29;
	} fields;
} touch_pwr_mgmt_ctrl_reg_t;

typedef union touch_ven_hw_info_reg
{
	u32 reg_value;
	struct
	{
		u32 vendor_id				:16;
		u32 device_id				:16;
	} fields;
} touch_ven_hw_info_reg_t;

typedef u32 touch_hw_rev_reg_t;

typedef u32 touch_fw_rev_reg_t;

typedef union touch_compat_rev_reg
{
	u32 reg_value;
	struct
	{
		u8 minor;
		u8 major;
		u8 intf_rev;
		u8 kernel_compat_ver;
	} fields;
} touch_compat_rev_reg_t;

typedef struct touch_reg_block
{
	touch_sts_reg_t sts_reg;
	touch_frame_char_reg_t frame_char_reg;
	touch_err_reg_t error_reg;
	u32 reserved0;
	touch_id_reg_t id_reg;
	touch_data_sz_reg_t data_size_reg;
	touch_caps_reg_t caps_reg;
	touch_cfg_reg_t cfg_reg;
	touch_cmd_reg_t cmd_reg;
	touch_pwr_mgmt_ctrl_reg_t pwm_mgme_ctrl_reg;
	touch_ven_hw_info_reg_t ven_hw_info_reg;
	touch_hw_rev_reg_t hw_rev_reg;
	touch_fw_rev_reg_t fw_rev_reg;
	touch_compat_rev_reg_t compat_rev_reg;
	u32 reserved1;
	u32 reserved2;
} touch_reg_block_t;

typedef enum output_buffer_payload_type {
	OUTPUT_BUFFER_PAYLOAD_ERROR = 0,
	OUTPUT_BUFFER_PAYLOAD_HID_INPUT_REPORT,
	OUTPUT_BUFFER_PAYLOAD_HID_FEATURE_REPORT,
	OUTPUT_BUFFER_PAYLOAD_KERNEL_LOAD,
	OUTPUT_BUFFER_PAYLOAD_FEEDBACK_BUFFER
} output_buffer_payload_type_t;

typedef enum touch_status
{
	TOUCH_STATUS_SUCCESS = 0,
	TOUCH_STATUS_INVALID_PARAMS,
	TOUCH_STATUS_ACCESS_DENIED,
	TOUCH_STATUS_CMD_SIZE_ERROR,
	TOUCH_STATUS_NOT_READY,
	TOUCH_STATUS_REQUEST_OUTSTANDING,
	TOUCH_STATUS_NO_SENSOR_FOUND,
	TOUCH_STATUS_OUT_OF_MEMORY,
	TOUCH_STATUS_INTERNAL_ERROR,
	TOUCH_STATUS_SENSOR_DISABLED,
	TOUCH_STATUS_COMPAT_CHECK_FAIL,
	TOUCH_STATUS_SENSOR_EXPECTED_RESET,
	TOUCH_STATUS_SENSOR_UNEXPECTED_RESET,
	TOUCH_STATUS_RESET_FAILED,
	TOUCH_STATUS_TIMEOUT,
	TOUCH_STATUS_TEST_MODE_FAIL,
	TOUCH_STATUS_SENSOR_FAIL_FATAL,
	TOUCH_STATUS_SENSOR_FAIL_NONFATAL,
	TOUCH_STATUS_INVALID_DEVICE_CAPS,
	TOUCH_STATUS_QUIESCE_IO_IN_PROGRESS,
	TOUCH_STATUS_MAX
} touch_status_t;

typedef struct touch_hid_private_data
{
	u32 transaction_id;
	u8 reserved[28];
} touch_hid_private_data_t;

typedef struct kernel_output_buffer_header {
	u16 length;
	u8 payload_type;
	u8 reserved1;
	touch_hid_private_data_t hid_private_data;
	u8 reserved2[28];
	u8 data[0];
} kernel_output_buffer_header_t;

typedef struct kernel_output_payload_error {
	u16 severity;
	u16 source;
	u8 code[4];
	char string[128];
} kernel_output_payload_error_t;

typedef struct touch_feedback_hdr
{
	u32 feedback_cmd_type;
	u32 payload_size_bytes;
	u32 buffer_id;
	u32 protocol_ver;
	u32 feedback_data_type;
	u32 spi_offest;
	u8 reserved[40];
} touch_feedback_hdr_t;

typedef struct touch_sensor_set_mode_cmd_data
{
	touch_sensor_mode_t sensor_mode;
	u32 reserved[3];
} touch_sensor_set_mode_cmd_data_t;

typedef struct touch_sensor_set_mem_window_cmd_data
{
	u32 touch_data_buffer_addr_lower[TOUCH_SENSOR_MAX_DATA_BUFFERS];
	u32 touch_data_buffer_addr_upper[TOUCH_SENSOR_MAX_DATA_BUFFERS];
	u32 tail_offset_addr_lower;
	u32 tail_offset_addr_upper;
	u32 doorbell_cookie_addr_lower;
	u32 doorbell_cookie_addr_upper;
	u32 feedback_buffer_addr_lower[TOUCH_SENSOR_MAX_DATA_BUFFERS];
	u32 feedback_buffer_addr_upper[TOUCH_SENSOR_MAX_DATA_BUFFERS];
	u32 hid2me_buffer_addr_lower;
	u32 hid2me_buffer_addr_upper;
	u32 hid2me_buffer_size;
	u8 reserved1;
	u8 work_queue_item_size;
	u16 work_queue_size;
	u32 reserved[8];
} touch_sensor_set_mem_window_cmd_data_t;

typedef struct touch_sensor_quiesce_io_cmd_data
{
	u32 quiesce_flags;
	u32 reserved[2];
} touch_sensor_quiesce_io_cmd_data_t;

typedef struct touch_sensor_feedback_ready_cmd_data
{
	u8 feedback_index;
	u8 reserved1[3];
	u32 transaction_id;
	u32 reserved2[2];
} touch_sensor_feedback_ready_cmd_data_t;

typedef enum touch_freq_override
{
	TOUCH_FREQ_OVERRIDE_NONE,
	TOUCH_FREQ_OVERRIDE_10MHZ,
	TOUCH_FREQ_OVERRIDE_17MHZ,
	TOUCH_FREQ_OVERRIDE_30MHZ,
	TOUCH_FREQ_OVERRIDE_50MHZ,
	TOUCH_FREQ_OVERRIDE_MAX
} touch_freq_override_t;

typedef enum touch_spi_io_mode_override
{
	TOUCH_SPI_IO_MODE_OVERRIDE_NONE,
	TOUCH_SPI_IO_MODE_OVERRIDE_SINGLE,
	TOUCH_SPI_IO_MODE_OVERRIDE_DUAL,
	TOUCH_SPI_IO_MODE_OVERRIDE_QUAD,
	TOUCH_SPI_IO_MODE_OVERRIDE_MAX
} touch_spi_io_mode_override_t;

typedef struct touch_policy_data
{
	u32 reserved0;
	u32 doze_timer					:16;
	touch_freq_override_t freq_override		:3;
	touch_spi_io_mode_override_t spi_io_override	:3;
	u32 reserved1					:10;
	u32 reserved2;
	u32 debug_override;
} touch_policy_data_t;

typedef struct touch_sensor_set_policies_cmd_data
{
	touch_policy_data_t policy_data;
} touch_sensor_set_policies_cmd_data_t;

typedef enum touch_sensor_reset_type
{
	TOUCH_SENSOR_RESET_TYPE_HARD,
	TOUCH_SENSOR_RESET_TYPE_SOFT,
	TOUCH_SENSOR_RESET_TYPE_MAX
} touch_sensor_reset_type_t;

typedef struct touch_sensor_reset_cmd_data
{
	touch_sensor_reset_type_t reset_type;
	u32 reserved;
} touch_sensor_reset_cmd_data_t;

typedef struct touch_sensor_msg_h2m
{
	u32  command_code;
	union
	{
		touch_sensor_set_mode_cmd_data_t set_mode_cmd_data;
		touch_sensor_set_mem_window_cmd_data_t set_window_cmd_data;
		touch_sensor_quiesce_io_cmd_data_t quiesce_io_cmd_data;
		touch_sensor_feedback_ready_cmd_data_t feedback_ready_cmd_data;
		touch_sensor_set_policies_cmd_data_t set_policies_cmd_data;
		touch_sensor_reset_cmd_data_t reset_cmd_data;
	} h2m_data;
} touch_sensor_msg_h2m_t;

typedef struct touch_sensor_set_mode_rsp_data
{
	u32 reserved[3];
} touch_sensor_set_mode_rsp_data_t;

typedef struct touch_sensor_set_mem_window_rsp_data
{
	u32 reserved[3];
} touch_sensor_set_mem_window_rsp_data_t;

typedef struct touch_sensor_quiesce_io_rsp_data
{
	u32 reserved[3];
} touch_sensor_quiesce_io_rsp_data_t;

typedef struct touch_sensor_hid_ready_for_data_rsp_data
{
	u32 data_size;
	u8 touch_data_buffer_index;
	u8 reset_reason;
	u8 reserved1[2];
	u32 reserved2[5];
} touch_sensor_hid_ready_for_data_rsp_data_t;

typedef struct touch_sensor_feedback_ready_rsp_data
{
	u8 feedback_index;
	u8 reserved1[3];
	u32 reserved2[6];
} touch_sensor_feedback_ready_rsp_data_t;

typedef struct touch_sensor_clear_mem_window_rsp_data
{
	u32 reserved[3];
} touch_sensor_clear_mem_window_rsp_data_t;

typedef struct touch_sensor_notify_dev_ready_rsp_data
{
	touch_err_reg_t err_reg;
	u32 reserved[2];
} touch_sensor_notify_dev_ready_rsp_data_t;

typedef struct touch_sensor_set_policies_rsp_data
{
	u32 reserved[3];
} touch_sensor_set_policies_rsp_data_t;

typedef struct touch_sensor_get_policies_rsp_data
{
	touch_policy_data_t policy_data;
} touch_sensor_get_policies_rsp_data_t;

typedef struct touch_sensor_reset_rsp_data
{
	u32 reserved[3];
} touch_sensor_reset_rsp_data_t;

typedef struct touch_sensor_read_all_regs_rsp_data
{
	touch_reg_block_t sensor_regs;
	u32 reserved[4];
} touch_sensor_read_all_regs_rsp_data_t;

typedef struct touch_sensor_msg_m2h
{
	u32 command_code;
	touch_status_t status;
	union
	{
		touch_sensor_get_device_info_rsp_data_t	device_info_rsp_data;
		touch_sensor_set_mode_rsp_data_t set_mode_rsp_data;
		touch_sensor_set_mem_window_rsp_data_t set_mem_window_rsp_data;
		touch_sensor_quiesce_io_rsp_data_t quiesce_io_rsp_data;
		touch_sensor_hid_ready_for_data_rsp_data_t hid_ready_for_data_rsp_data;
		touch_sensor_feedback_ready_rsp_data_t feedback_ready_rsp_data;
		touch_sensor_clear_mem_window_rsp_data_t clear_mem_window_rsp_data;
		touch_sensor_notify_dev_ready_rsp_data_t notify_dev_ready_rsp_data;
		touch_sensor_set_policies_rsp_data_t set_policies_rsp_data;
		touch_sensor_get_policies_rsp_data_t get_policies_rsp_data;
		touch_sensor_reset_rsp_data_t reset_rsp_data;
		touch_sensor_read_all_regs_rsp_data_t read_all_regs_rsp_data;
	} m2h_data;
} touch_sensor_msg_m2h_t;

int ipts_hid_send_hid2me_feedback(ipts_info_t *ipts, u32 fb_data_type, __u8 *buf, size_t count);
int ipts_handle_resp(ipts_info_t *ipts, touch_sensor_msg_m2h_t *m2h_msg, u32 msg_len);
int ipts_start(ipts_info_t *ipts);
void ipts_stop(ipts_info_t *ipts);

#endif // _IPTS_MSGS_H_
