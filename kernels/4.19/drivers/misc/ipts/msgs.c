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

#include <linux/delay.h>
#include <linux/intel-ipts.h>
#include <linux/moduleparam.h>

#include "hid.h"
#include "msgs.h"
#include "resources.h"

static bool no_feedback = false;
module_param(no_feedback, bool, 0644);
MODULE_PARM_DESC(no_feedback,
				"Disable feedback for SP4 and SB1 users experiencing touch input crashes. "
				"(default: false)");

static int ipts_handle_cmd(ipts_info_t *ipts, u32 cmd, void *data, int data_size)
{
	int ret = 0;
	touch_sensor_msg_h2m_t h2m_msg;
	int len = 0;

	memset(&h2m_msg, 0, sizeof(h2m_msg));

	h2m_msg.command_code = cmd;
	len = sizeof(h2m_msg.command_code) + data_size;
	if (data != NULL && data_size != 0)
		memcpy(&h2m_msg.h2m_data, data, data_size);

	ret = mei_cldev_send(ipts->cldev, (u8*)&h2m_msg, len);
	if (ret < 0) {
		dev_err(&ipts->cldev->dev, "mei_cldev_send() error 0x%X:%d\n",
							cmd, ret);
		return ret;
	}

	return 0;
}

static int ipts_send_sensor_quiesce_io_cmd(ipts_info_t *ipts)
{
	int ret;
	int cmd_len;
	touch_sensor_quiesce_io_cmd_data_t quiesce_io_cmd;

	cmd_len = sizeof(touch_sensor_quiesce_io_cmd_data_t);
	memset(&quiesce_io_cmd, 0, cmd_len);

	ret = ipts_handle_cmd(ipts, TOUCH_SENSOR_QUIESCE_IO_CMD,
				&quiesce_io_cmd, cmd_len);

	return ret;
}

static int ipts_send_sensor_hid_ready_for_data_cmd(ipts_info_t *ipts)
{
	return ipts_handle_cmd(ipts, TOUCH_SENSOR_HID_READY_FOR_DATA_CMD, NULL, 0);
}

static int ipts_send_sensor_clear_mem_window_cmd(ipts_info_t *ipts)
{
	return ipts_handle_cmd(ipts, TOUCH_SENSOR_CLEAR_MEM_WINDOW_CMD, NULL, 0);
}

static void get_raw_data_only_smw_cmd_data(ipts_info_t *ipts,
				touch_sensor_set_mem_window_cmd_data_t *data,
				ipts_resources_t *resrc)
{
	u64 wq_tail_phy_addr;
	u64 cookie_phy_addr;
	ipts_buffer_info_t *touch_buf;
	ipts_buffer_info_t *feedback_buf;
	int i;

	touch_buf = resrc->touch_data_buffer_raw;
	feedback_buf = resrc->feedback_buffer;

	for (i = 0; i < TOUCH_SENSOR_MAX_DATA_BUFFERS; i++) {
		data->touch_data_buffer_addr_lower[i] =
					lower_32_bits(touch_buf[i].dma_addr);
		data->touch_data_buffer_addr_upper[i] =
					upper_32_bits(touch_buf[i].dma_addr);
		data->feedback_buffer_addr_lower[i] =
					lower_32_bits(feedback_buf[i].dma_addr);
		data->feedback_buffer_addr_upper[i] =
					upper_32_bits(feedback_buf[i].dma_addr);
	}

	wq_tail_phy_addr = resrc->wq_info.wq_tail_phy_addr;
	data->tail_offset_addr_lower = lower_32_bits(wq_tail_phy_addr);
	data->tail_offset_addr_upper = upper_32_bits(wq_tail_phy_addr);

	cookie_phy_addr = resrc->wq_info.db_phy_addr +
						resrc->wq_info.db_cookie_offset;
	data->doorbell_cookie_addr_lower = lower_32_bits(cookie_phy_addr);
	data->doorbell_cookie_addr_upper = upper_32_bits(cookie_phy_addr);
	data->work_queue_size = resrc->wq_info.wq_size;

	data->work_queue_item_size = resrc->wq_item_size;
}

static void ipts_get_set_mem_window_cmd_data(ipts_info_t *ipts,
				touch_sensor_set_mem_window_cmd_data_t *data)
{
	ipts_resources_t *resrc = &ipts->resources;

	get_raw_data_only_smw_cmd_data(ipts, data, resrc);

	data->hid2me_buffer_addr_lower =
				lower_32_bits(resrc->hid2me_buffer.dma_addr);
	data->hid2me_buffer_addr_upper =
				upper_32_bits(resrc->hid2me_buffer.dma_addr);
	data->hid2me_buffer_size = resrc->hid2me_buffer_size;
}


static int ipts_send_feedback(ipts_info_t *ipts, int buffer_idx, u32 transaction_id)
{
	int ret;
	int cmd_len;
	touch_sensor_feedback_ready_cmd_data_t fb_ready_cmd;

	cmd_len = sizeof(touch_sensor_feedback_ready_cmd_data_t);
	memset(&fb_ready_cmd, 0, cmd_len);

	fb_ready_cmd.feedback_index = buffer_idx;
	fb_ready_cmd.transaction_id = transaction_id;

	ret = ipts_handle_cmd(ipts, TOUCH_SENSOR_FEEDBACK_READY_CMD,
				&fb_ready_cmd, cmd_len);

	return ret;
}

int ipts_hid_send_hid2me_feedback(ipts_info_t *ipts, u32 fb_data_type, __u8 *buf, size_t count)
{
	ipts_buffer_info_t *fb_buf;
	touch_feedback_hdr_t *feedback;
	u8 *payload;
	int header_size;

	header_size = sizeof(touch_feedback_hdr_t);

	if (count > ipts->resources.hid2me_buffer_size - header_size)
		return -EINVAL;

	fb_buf = &ipts->resources.hid2me_buffer;
	feedback = (touch_feedback_hdr_t *)fb_buf->addr;
	payload = fb_buf->addr + header_size;
	memset(feedback, 0, header_size);

	feedback->feedback_data_type = fb_data_type;
	feedback->feedback_cmd_type = 0;
	feedback->payload_size_bytes = count;
	feedback->buffer_id = TOUCH_SENSOR_MAX_DATA_BUFFERS;
	feedback->protocol_ver = 0;
	feedback->reserved[0] = 0xAC;

	memcpy(payload, buf, count);

	ipts_send_feedback(ipts, TOUCH_SENSOR_MAX_DATA_BUFFERS, 0);

	return 0;
}

static int handle_outputs(ipts_info_t *ipts, int parallel_idx)
{
	kernel_output_buffer_header_t *out_buf_hdr;
	ipts_buffer_info_t *output_buf, *fb_buf = NULL;
	u8 *payload;
	u32 transaction_id = 0;
	int header_size, i, payload_size, ret = 0;

	header_size = sizeof(kernel_output_buffer_header_t);
	output_buf = &ipts->resources.raw_data_mode_output_buffer[parallel_idx][0];
	for (i = 0; i < ipts->resources.num_of_outputs; i++) {
		out_buf_hdr = (kernel_output_buffer_header_t*)output_buf[i].addr;
		if (out_buf_hdr->length < header_size)
			continue;

		payload_size = out_buf_hdr->length - header_size;
		payload = out_buf_hdr->data;

		switch(out_buf_hdr->payload_type) {
			case OUTPUT_BUFFER_PAYLOAD_HID_INPUT_REPORT:
				hid_input_report(ipts->hid, HID_INPUT_REPORT,
						payload, payload_size, 1);
				break;
			case OUTPUT_BUFFER_PAYLOAD_HID_FEATURE_REPORT:
				dev_dbg(&ipts->cldev->dev, "output hid feature report\n");
				break;
			case OUTPUT_BUFFER_PAYLOAD_KERNEL_LOAD:
				dev_dbg(&ipts->cldev->dev, "output kernel load\n");
				break;
			case OUTPUT_BUFFER_PAYLOAD_FEEDBACK_BUFFER:
			{
                                fb_buf = &ipts->resources.feedback_buffer[parallel_idx];
				transaction_id = out_buf_hdr->
						hid_private_data.transaction_id;
				memcpy(fb_buf->addr, payload, payload_size);
				break;
			}
			case OUTPUT_BUFFER_PAYLOAD_ERROR:
			{
				kernel_output_payload_error_t *err_payload;

				if (payload_size == 0)
					break;

				err_payload =
					(kernel_output_payload_error_t*)payload;

				dev_err(&ipts->cldev->dev, "error : severity : %d,"
						" source : %d,"
						" code : %d:%d:%d:%d\n"
						"string %s\n",
						err_payload->severity,
						err_payload->source,
						err_payload->code[0],
						err_payload->code[1],
						err_payload->code[2],
						err_payload->code[3],
						err_payload->string);
				
				break;
			}
			default:
				dev_err(&ipts->cldev->dev, "invalid output buffer payload\n");
				break;
		}
	}

	if (no_feedback)
		return 0;

	if (fb_buf)
		ret = ipts_send_feedback(ipts, parallel_idx, transaction_id);

	return ret;
}

static int handle_output_buffers(ipts_info_t *ipts, int cur_idx, int end_idx)
{
	do {
		cur_idx++;
		cur_idx %= TOUCH_SENSOR_MAX_DATA_BUFFERS;
		handle_outputs(ipts, cur_idx);
	} while (cur_idx != end_idx);

	return 0;
}

static void ipts_handle_processed_data(ipts_info_t *ipts)
{
	int last_buffer_submitted = *ipts->resources.last_buffer_submitted;

	if (ipts->state != IPTS_STA_RAW_DATA_STARTED || ipts->resources.last_buffer_completed == last_buffer_submitted)
		return;

	handle_output_buffers(ipts, ipts->resources.last_buffer_completed, last_buffer_submitted);
	ipts->resources.last_buffer_completed = last_buffer_submitted;
}

static int check_validity(touch_sensor_msg_m2h_t *m2h_msg, u32 msg_len)
{
	int ret = 0;
	int valid_msg_len = sizeof(m2h_msg->command_code);
	u32 cmd_code = m2h_msg->command_code;

	switch (cmd_code) {
		case TOUCH_SENSOR_SET_MODE_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_set_mode_rsp_data_t);
			break;
		case TOUCH_SENSOR_SET_MEM_WINDOW_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_set_mem_window_rsp_data_t);
			break;
		case TOUCH_SENSOR_QUIESCE_IO_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_quiesce_io_rsp_data_t);
			break;
		case TOUCH_SENSOR_HID_READY_FOR_DATA_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_hid_ready_for_data_rsp_data_t);
			break;
		case TOUCH_SENSOR_FEEDBACK_READY_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_feedback_ready_rsp_data_t);
			break;
		case TOUCH_SENSOR_CLEAR_MEM_WINDOW_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_clear_mem_window_rsp_data_t);
			break;
		case TOUCH_SENSOR_NOTIFY_DEV_READY_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_notify_dev_ready_rsp_data_t);
			break;
		case TOUCH_SENSOR_SET_POLICIES_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_set_policies_rsp_data_t);
			break;
		case TOUCH_SENSOR_GET_POLICIES_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_get_policies_rsp_data_t);
			break;
		case TOUCH_SENSOR_RESET_RSP:
			valid_msg_len +=
				sizeof(touch_sensor_reset_rsp_data_t);
			break;
	}

	if (valid_msg_len != msg_len) {
		return -EINVAL;
	}

	return ret;
}

int ipts_handle_resp(ipts_info_t *ipts, touch_sensor_msg_m2h_t *m2h_msg,
								u32 msg_len)
{
	int ret = 0;
	int rsp_status = 0;
	int cmd_len = 0;
	u32 cmd;

	if (!check_validity(m2h_msg, msg_len)) {
		dev_err(&ipts->cldev->dev, "Wrong rsp\n");
		return -EINVAL;
	}

	rsp_status = m2h_msg->status;
	cmd = m2h_msg->command_code;

	dev_dbg(&ipts->cldev->dev, "Cmd received 0x%08x\n", cmd);
	switch (cmd) {
		case TOUCH_SENSOR_NOTIFY_DEV_READY_RSP:
		{
			if (rsp_status != 0 && rsp_status != TOUCH_STATUS_SENSOR_FAIL_NONFATAL) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			ret = ipts_handle_cmd(ipts,
					TOUCH_SENSOR_GET_DEVICE_INFO_CMD,
					NULL, 0);
			break;
		}
		case TOUCH_SENSOR_GET_DEVICE_INFO_RSP:
		{
			if (rsp_status != 0 &&
			  rsp_status != TOUCH_STATUS_COMPAT_CHECK_FAIL) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			memcpy(&ipts->device_info,
				&m2h_msg->m2h_data.device_info_rsp_data,
				sizeof(touch_sensor_get_device_info_rsp_data_t));

			ret = intel_ipts_connect(ipts, ipts_handle_processed_data);
			if (ret) {
				dev_err(&ipts->cldev->dev, "Intel i915 framework is not initialized");
				break;
			}

			ret = ipts_send_sensor_clear_mem_window_cmd(ipts);
			break;
		}
		case TOUCH_SENSOR_CLEAR_MEM_WINDOW_RSP:
		{
			touch_sensor_set_mode_cmd_data_t sensor_mode_cmd;

			if (rsp_status != 0 &&
					rsp_status != TOUCH_STATUS_TIMEOUT) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			ret = ipts_allocate_resources(ipts);
			if (ret) {
				dev_err(&ipts->cldev->dev, "cannot allocate default resource\n");
				break;
			}

			ipts->state = IPTS_STA_RESOURCE_READY;

			cmd_len = sizeof(touch_sensor_set_mode_cmd_data_t);
			memset(&sensor_mode_cmd, 0, cmd_len);
			sensor_mode_cmd.sensor_mode = TOUCH_SENSOR_MODE_RAW_DATA;
			ret = ipts_handle_cmd(ipts,
				TOUCH_SENSOR_SET_MODE_CMD,
				&sensor_mode_cmd, cmd_len);
			break;
		}
		case TOUCH_SENSOR_SET_MODE_RSP:
		{
			touch_sensor_set_mem_window_cmd_data_t smw_cmd;

			if (rsp_status != 0) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			cmd_len = sizeof(touch_sensor_set_mem_window_cmd_data_t);
			memset(&smw_cmd, 0, cmd_len);
			ipts_get_set_mem_window_cmd_data(ipts, &smw_cmd);
			ret = ipts_handle_cmd(ipts,
				TOUCH_SENSOR_SET_MEM_WINDOW_CMD,
				&smw_cmd, cmd_len);
			break;
		}
		case TOUCH_SENSOR_SET_MEM_WINDOW_RSP:
		{
			if (rsp_status != 0) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			ret = ipts_hid_init(ipts);
			if (ret)
				break;

			ret = ipts_send_sensor_hid_ready_for_data_cmd(ipts);
			if (ret)
				break;

			ipts->state = IPTS_STA_RAW_DATA_STARTED;

			break;
		}
		case TOUCH_SENSOR_FEEDBACK_READY_RSP:
		{
			if (rsp_status != 0 &&
			  rsp_status != TOUCH_STATUS_COMPAT_CHECK_FAIL) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			break;
		}
		case TOUCH_SENSOR_QUIESCE_IO_RSP:
		{
			if (rsp_status != 0) {
				dev_err(&ipts->cldev->dev, "0x%08x failed status = %d\n", cmd, rsp_status);
				break;
			}

			break;
		}
	}

	return ret;
}

int ipts_start(ipts_info_t *ipts)
{
	int ret;

	ipts->state = IPTS_STA_INIT;

	ret = ipts_handle_cmd(ipts, TOUCH_SENSOR_NOTIFY_DEV_READY_CMD, NULL, 0);
	if (ret) {
		dev_err(&ipts->cldev->dev, "cannot initiate message workflow\n");
		return ret;
	}

	return ret;
}

void ipts_stop(ipts_info_t *ipts)
{
	if (ipts->state == IPTS_STA_RAW_DATA_STARTED)
		ipts_send_sensor_quiesce_io_cmd(ipts);

	ipts->state = IPTS_STA_STOPPING;

	ipts_hid_fini(ipts);

	ipts_free_resources(ipts);

	intel_ipts_disconnect();

	// Let a delay for IPTS quiesce_io_cmd to be properly handled
	msleep(250);
}
