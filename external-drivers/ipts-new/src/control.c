// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/delay.h>
#include <linux/dev_printk.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/types.h>

#include "cmd.h"
#include "context.h"
#include "control.h"
#include "desc.h"
#include "hid.h"
#include "receiver.h"
#include "resources.h"
#include "spec-data.h"
#include "spec-device.h"

static int ipts_control_get_device_info(struct ipts_context *ipts, struct ipts_device_info *info)
{
	int ret = 0;
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	if (!info)
		return -EFAULT;

	ret = ipts_cmd_send(ipts, IPTS_CMD_GET_DEVICE_INFO, NULL, 0);
	if (ret) {
		dev_err(ipts->dev, "GET_DEVICE_INFO: send failed: %d\n", ret);
		return ret;
	}

	ret = ipts_cmd_recv(ipts, IPTS_CMD_GET_DEVICE_INFO, &rsp);
	if (ret) {
		dev_err(ipts->dev, "GET_DEVICE_INFO: recv failed: %d\n", ret);
		return ret;
	}

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "GET_DEVICE_INFO: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	memcpy(info, rsp.payload, sizeof(*info));
	return 0;
}

static int ipts_control_set_mode(struct ipts_context *ipts, enum ipts_mode mode)
{
	int ret = 0;
	struct ipts_set_mode cmd = { 0 };
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	cmd.mode = mode;

	ret = ipts_cmd_send(ipts, IPTS_CMD_SET_MODE, &cmd, sizeof(cmd));
	if (ret) {
		dev_err(ipts->dev, "SET_MODE: send failed: %d\n", ret);
		return ret;
	}

	ret = ipts_cmd_recv(ipts, IPTS_CMD_SET_MODE, &rsp);
	if (ret) {
		dev_err(ipts->dev, "SET_MODE: recv failed: %d\n", ret);
		return ret;
	}

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "SET_MODE: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	return 0;
}

static int ipts_control_set_mem_window(struct ipts_context *ipts, struct ipts_resources *res)
{
	int ret = 0;
	struct ipts_mem_window cmd = { 0 };
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	if (!res)
		return -EFAULT;

	for (int i = 0; i < IPTS_BUFFERS; i++) {
		cmd.data_addr_lower[i] = lower_32_bits(res->data[i].dma_address);
		cmd.data_addr_upper[i] = upper_32_bits(res->data[i].dma_address);
		cmd.feedback_addr_lower[i] = lower_32_bits(res->feedback[i].dma_address);
		cmd.feedback_addr_upper[i] = upper_32_bits(res->feedback[i].dma_address);
	}

	cmd.workqueue_addr_lower = lower_32_bits(res->workqueue.dma_address);
	cmd.workqueue_addr_upper = upper_32_bits(res->workqueue.dma_address);

	cmd.doorbell_addr_lower = lower_32_bits(res->doorbell.dma_address);
	cmd.doorbell_addr_upper = upper_32_bits(res->doorbell.dma_address);

	cmd.hid2me_addr_lower = lower_32_bits(res->hid2me.dma_address);
	cmd.hid2me_addr_upper = upper_32_bits(res->hid2me.dma_address);

	cmd.workqueue_size = IPTS_WORKQUEUE_SIZE;
	cmd.workqueue_item_size = IPTS_WORKQUEUE_ITEM_SIZE;

	ret = ipts_cmd_send(ipts, IPTS_CMD_SET_MEM_WINDOW, &cmd, sizeof(cmd));
	if (ret) {
		dev_err(ipts->dev, "SET_MEM_WINDOW: send failed: %d\n", ret);
		return ret;
	}

	ret = ipts_cmd_recv(ipts, IPTS_CMD_SET_MEM_WINDOW, &rsp);
	if (ret) {
		dev_err(ipts->dev, "SET_MEM_WINDOW: recv failed: %d\n", ret);
		return ret;
	}

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "SET_MEM_WINDOW: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	return 0;
}

static int ipts_control_get_descriptor(struct ipts_context *ipts)
{
	int ret = 0;
	struct ipts_data_header *header = NULL;
	struct ipts_get_descriptor cmd = { 0 };
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	if (!ipts->resources.descriptor.address)
		return -EFAULT;

	memset(ipts->resources.descriptor.address, 0, ipts->resources.descriptor.size);

	cmd.addr_lower = lower_32_bits(ipts->resources.descriptor.dma_address);
	cmd.addr_upper = upper_32_bits(ipts->resources.descriptor.dma_address);
	cmd.magic = 8;

	ret = ipts_cmd_send(ipts, IPTS_CMD_GET_DESCRIPTOR, &cmd, sizeof(cmd));
	if (ret) {
		dev_err(ipts->dev, "GET_DESCRIPTOR: send failed: %d\n", ret);
		return ret;
	}

	ret = ipts_cmd_recv(ipts, IPTS_CMD_GET_DESCRIPTOR, &rsp);
	if (ret) {
		dev_err(ipts->dev, "GET_DESCRIPTOR: recv failed: %d\n", ret);
		return ret;
	}

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "GET_DESCRIPTOR: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	header = (struct ipts_data_header *)ipts->resources.descriptor.address;

	if (header->type == IPTS_DATA_TYPE_DESCRIPTOR) {
		ipts->descriptor.address = &header->data[8];
		ipts->descriptor.size = header->size - 8;

		return 0;
	}

	return -ENODATA;
}

int ipts_control_request_flush(struct ipts_context *ipts)
{
	int ret = 0;
	struct ipts_quiesce_io cmd = { 0 };

	if (!ipts)
		return -EFAULT;

	ret = ipts_cmd_send(ipts, IPTS_CMD_QUIESCE_IO, &cmd, sizeof(cmd));
	if (ret)
		dev_err(ipts->dev, "QUIESCE_IO: send failed: %d\n", ret);

	return ret;
}

int ipts_control_wait_flush(struct ipts_context *ipts)
{
	int ret = 0;
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	ret = ipts_cmd_recv(ipts, IPTS_CMD_QUIESCE_IO, &rsp);
	if (ret) {
		dev_err(ipts->dev, "QUIESCE_IO: recv failed: %d\n", ret);
		return ret;
	}

	if (rsp.status == IPTS_STATUS_TIMEOUT)
		return -EAGAIN;

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "QUIESCE_IO: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	return 0;
}

int ipts_control_request_data(struct ipts_context *ipts)
{
	int ret = 0;

	if (!ipts)
		return -EFAULT;

	ret = ipts_cmd_send(ipts, IPTS_CMD_READY_FOR_DATA, NULL, 0);
	if (ret)
		dev_err(ipts->dev, "READY_FOR_DATA: send failed: %d\n", ret);

	return ret;
}

int ipts_control_wait_data(struct ipts_context *ipts, bool shutdown)
{
	int ret = 0;
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	if (!shutdown)
		ret = ipts_cmd_recv_timeout(ipts, IPTS_CMD_READY_FOR_DATA, &rsp, 0);
	else
		ret = ipts_cmd_recv(ipts, IPTS_CMD_READY_FOR_DATA, &rsp);

	if (ret) {
		if (ret != -EAGAIN)
			dev_err(ipts->dev, "READY_FOR_DATA: recv failed: %d\n", ret);

		return ret;
	}

	/*
	 * During shutdown, it is possible that the sensor has already been disabled.
	 */
	if (rsp.status == IPTS_STATUS_SENSOR_DISABLED)
		return 0;

	if (rsp.status == IPTS_STATUS_TIMEOUT)
		return -EAGAIN;

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "READY_FOR_DATA: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	return 0;
}

int ipts_control_send_feedback(struct ipts_context *ipts, u32 buffer)
{
	int ret = 0;
	struct ipts_feedback cmd = { 0 };
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	cmd.buffer = buffer;

	ret = ipts_cmd_send(ipts, IPTS_CMD_FEEDBACK, &cmd, sizeof(cmd));
	if (ret) {
		dev_err(ipts->dev, "FEEDBACK: send failed: %d\n", ret);
		return ret;
	}

	ret = ipts_cmd_recv(ipts, IPTS_CMD_FEEDBACK, &rsp);
	if (ret) {
		dev_err(ipts->dev, "FEEDBACK: recv failed: %d\n", ret);
		return ret;
	}

	/*
	 * We don't know what feedback data looks like so we are sending zeros.
	 * See also ipts_control_refill_buffer.
	 */
	if (rsp.status == IPTS_STATUS_INVALID_PARAMS)
		return 0;

	if (rsp.status != IPTS_STATUS_SUCCESS) {
		dev_err(ipts->dev, "FEEDBACK: cmd failed: %d\n", rsp.status);
		return -EBADR;
	}

	return 0;
}

int ipts_control_hid2me_feedback(struct ipts_context *ipts, enum ipts_feedback_cmd_type cmd,
				 enum ipts_feedback_data_type type, void *data, size_t size)
{
	struct ipts_feedback_header *header = NULL;

	if (!ipts)
		return -EFAULT;

	if (!ipts->resources.hid2me.address)
		return -EFAULT;

	memset(ipts->resources.hid2me.address, 0, ipts->resources.hid2me.size);
	header = (struct ipts_feedback_header *)ipts->resources.hid2me.address;

	header->cmd_type = cmd;
	header->data_type = type;
	header->size = size;
	header->buffer = IPTS_HID2ME_BUFFER;

	if (size + sizeof(*header) > ipts->resources.hid2me.size)
		return -EINVAL;

	if (data && size > 0)
		memcpy(header->payload, data, size);

	return ipts_control_send_feedback(ipts, IPTS_HID2ME_BUFFER);
}

static inline int ipts_control_reset_sensor(struct ipts_context *ipts)
{
	return ipts_control_hid2me_feedback(ipts, IPTS_FEEDBACK_CMD_TYPE_SOFT_RESET,
					    IPTS_FEEDBACK_DATA_TYPE_VENDOR, NULL, 0);
}

int ipts_control_start(struct ipts_context *ipts)
{
	int ret = 0;
	struct ipts_device_info info = { 0 };

	if (!ipts)
		return -EFAULT;

	dev_info(ipts->dev, "Starting IPTS\n");

	ret = ipts_control_get_device_info(ipts, &info);
	if (ret) {
		dev_err(ipts->dev, "Failed to get device info: %d\n", ret);
		return ret;
	}

	ipts->info = info;

	ret = ipts_resources_init(&ipts->resources, ipts->dev, info.data_size, info.feedback_size);
	if (ret) {
		dev_err(ipts->dev, "Failed to allocate buffers: %d", ret);
		return ret;
	}

	dev_info(ipts->dev, "IPTS EDS Version: %d\n", info.intf_eds);

	/*
	 * Handle newer devices
	 */
	if (info.intf_eds > 1) {
		/*
		 * Fetching the descriptor will only work on newer devices.
		 * For older devices, a fallback descriptor will be used.
		 */
		ret = ipts_control_get_descriptor(ipts);
		if (ret) {
			dev_err(ipts->dev, "Failed to fetch HID descriptor: %d\n", ret);
			return ret;
		}

		/*
		 * Newer devices can be directly initialized in doorbell mode.
		 */
		ipts->mode = IPTS_MODE_DOORBELL;
	}

	ret = ipts_control_set_mode(ipts, ipts->mode);
	if (ret) {
		dev_err(ipts->dev, "Failed to set mode: %d\n", ret);
		return ret;
	}

	ret = ipts_control_set_mem_window(ipts, &ipts->resources);
	if (ret) {
		dev_err(ipts->dev, "Failed to set memory window: %d\n", ret);
		return ret;
	}

	ret = ipts_receiver_start(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to start receiver: %d\n", ret);
		return ret;
	}

	ret = ipts_control_request_data(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to request data: %d\n", ret);
		return ret;
	}

	ret = ipts_hid_init(ipts, info);
	if (ret) {
		dev_err(ipts->dev, "Failed to initialize HID device: %d\n", ret);
		return ret;
	}

	return 0;
}

static int _ipts_control_stop(struct ipts_context *ipts)
{
	int ret = 0;

	if (!ipts)
		return -EFAULT;

	dev_info(ipts->dev, "Stopping IPTS\n");

	ret = ipts_receiver_stop(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to stop receiver: %d\n", ret);
		return ret;
	}

	ret = ipts_control_reset_sensor(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to reset sensor: %d\n", ret);
		return ret;
	}

	ret = ipts_resources_free(&ipts->resources);
	if (ret) {
		dev_err(ipts->dev, "Failed to free resources: %d\n", ret);
		return ret;
	}

	return 0;
}

int ipts_control_stop(struct ipts_context *ipts)
{
	int ret = 0;

	ret = _ipts_control_stop(ipts);
	if (ret)
		return ret;

	ret = ipts_hid_free(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to free HID device: %d\n", ret);
		return ret;
	}

	return 0;
}

int ipts_control_restart(struct ipts_context *ipts)
{
	int ret = 0;

	ret = _ipts_control_stop(ipts);
	if (ret)
		return ret;

	/*
	 * Give the sensor some time to come back from resetting
	 */
	msleep(1000);

	ret = ipts_control_start(ipts);
	if (ret)
		return ret;

	return 0;
}
