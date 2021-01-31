// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/mei_cl_bus.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "protocol.h"
#include "resources.h"

static int ipts_receiver_handle_get_device_info(struct ipts_context *ipts,
		struct ipts_response *rsp)
{
	struct ipts_set_mode_cmd cmd;

	memcpy(&ipts->device_info, rsp->payload,
			sizeof(struct ipts_get_device_info_rsp));

	memset(&cmd, 0, sizeof(struct ipts_set_mode_cmd));
	cmd.mode = IPTS_MODE_MULTITOUCH;

	return ipts_control_send(ipts, IPTS_CMD_SET_MODE,
			&cmd, sizeof(struct ipts_set_mode_cmd));
}

static int ipts_receiver_handle_set_mode(struct ipts_context *ipts)
{
	int i, ret;
	struct ipts_set_mem_window_cmd cmd;

	ret = ipts_resources_alloc(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to allocate resources\n");
		return ret;
	}

	memset(&cmd, 0, sizeof(struct ipts_set_mem_window_cmd));

	for (i = 0; i < IPTS_BUFFERS; i++) {
		cmd.data_buffer_addr_lower[i] =
			lower_32_bits(ipts->data[i].dma_address);

		cmd.data_buffer_addr_upper[i] =
			upper_32_bits(ipts->data[i].dma_address);

		cmd.feedback_buffer_addr_lower[i] =
			lower_32_bits(ipts->feedback[i].dma_address);

		cmd.feedback_buffer_addr_upper[i] =
			upper_32_bits(ipts->feedback[i].dma_address);
	}

	cmd.workqueue_addr_lower = lower_32_bits(ipts->workqueue.dma_address);
	cmd.workqueue_addr_upper = upper_32_bits(ipts->workqueue.dma_address);

	cmd.doorbell_addr_lower = lower_32_bits(ipts->doorbell.dma_address);
	cmd.doorbell_addr_upper = upper_32_bits(ipts->doorbell.dma_address);

	cmd.host2me_addr_lower = lower_32_bits(ipts->host2me.dma_address);
	cmd.host2me_addr_upper = upper_32_bits(ipts->host2me.dma_address);

	cmd.workqueue_size = IPTS_WORKQUEUE_SIZE;
	cmd.workqueue_item_size = IPTS_WORKQUEUE_ITEM_SIZE;

	return ipts_control_send(ipts, IPTS_CMD_SET_MEM_WINDOW,
			&cmd, sizeof(struct ipts_set_mem_window_cmd));
}

static int ipts_receiver_handle_set_mem_window(struct ipts_context *ipts)
{
	dev_info(ipts->dev, "Device %04hX:%04hX ready\n",
			ipts->device_info.vendor_id,
			ipts->device_info.device_id);
	ipts->status = IPTS_HOST_STATUS_STARTED;

	return ipts_control_send(ipts, IPTS_CMD_READY_FOR_DATA, NULL, 0);
}

static int ipts_receiver_handle_clear_mem_window(struct ipts_context *ipts)
{
	if (ipts->restart)
		return ipts_control_start(ipts);

	ipts->status = IPTS_HOST_STATUS_STOPPED;
	return 0;
}

static bool ipts_receiver_handle_error(struct ipts_context *ipts,
		struct ipts_response *rsp)
{
	bool error;

	switch (rsp->status) {
	case IPTS_STATUS_SUCCESS:
	case IPTS_STATUS_COMPAT_CHECK_FAIL:
		error = false;
		break;
	case IPTS_STATUS_INVALID_PARAMS:
		error = rsp->code != IPTS_RSP_FEEDBACK;
		break;
	case IPTS_STATUS_SENSOR_DISABLED:
		error = ipts->status != IPTS_HOST_STATUS_STOPPING;
		break;
	default:
		error = true;
		break;
	}

	if (!error)
		return false;

	dev_err(ipts->dev, "Command 0x%08x failed: %d\n",
			rsp->code, rsp->status);

	if (rsp->code == IPTS_STATUS_SENSOR_UNEXPECTED_RESET) {
		dev_err(ipts->dev, "Sensor was reset\n");

		if (ipts_control_restart(ipts))
			dev_err(ipts->dev, "Failed to restart IPTS\n");
	}

	return true;
}

static void ipts_receiver_handle_response(struct ipts_context *ipts,
		struct ipts_response *rsp)
{
	int ret;

	if (ipts_receiver_handle_error(ipts, rsp))
		return;

	switch (rsp->code) {
	case IPTS_RSP_GET_DEVICE_INFO:
		ret = ipts_receiver_handle_get_device_info(ipts, rsp);
		break;
	case IPTS_RSP_SET_MODE:
		ret = ipts_receiver_handle_set_mode(ipts);
		break;
	case IPTS_RSP_SET_MEM_WINDOW:
		ret = ipts_receiver_handle_set_mem_window(ipts);
		break;
	case IPTS_RSP_CLEAR_MEM_WINDOW:
		ret = ipts_receiver_handle_clear_mem_window(ipts);
		break;
	default:
		ret = 0;
		break;
	}

	if (!ret)
		return;

	dev_err(ipts->dev, "Error while handling response 0x%08x: %d\n",
			rsp->code, ret);

	if (ipts_control_stop(ipts))
		dev_err(ipts->dev, "Failed to stop IPTS\n");
}

void ipts_receiver_callback(struct mei_cl_device *cldev)
{
	int ret;
	struct ipts_response rsp;
	struct ipts_context *ipts;

	ipts = mei_cldev_get_drvdata(cldev);

	ret = mei_cldev_recv(cldev, (u8 *)&rsp, sizeof(struct ipts_response));
	if (ret <= 0) {
		dev_err(ipts->dev, "Error while reading response: %d\n", ret);
		return;
	}

	ipts_receiver_handle_response(ipts, &rsp);
}

