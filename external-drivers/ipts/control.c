// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/mei_cl_bus.h>

#include "context.h"
#include "protocol.h"
#include "resources.h"
#include "uapi.h"

int ipts_control_send(struct ipts_context *ipts,
		u32 code, void *payload, size_t size)
{
	int ret;
	struct ipts_command cmd;

	memset(&cmd, 0, sizeof(struct ipts_command));
	cmd.code = code;

	if (payload && size > 0)
		memcpy(&cmd.payload, payload, size);

	ret = mei_cldev_send(ipts->cldev, (u8 *)&cmd, sizeof(cmd.code) + size);
	if (ret >= 0 || ret == -EINTR)
		return 0;

	dev_err(ipts->dev, "Error while sending: 0x%X:%d\n", code, ret);
	return ret;
}

int ipts_control_start(struct ipts_context *ipts)
{
	if (ipts->status != IPTS_HOST_STATUS_STOPPED)
		return -EBUSY;

	dev_info(ipts->dev, "Starting IPTS\n");
	ipts->status = IPTS_HOST_STATUS_STARTING;
	ipts->restart = false;

	ipts_uapi_link(ipts);
	return ipts_control_send(ipts, IPTS_CMD_GET_DEVICE_INFO, NULL, 0);
}

int ipts_control_stop(struct ipts_context *ipts)
{
	if (ipts->status == IPTS_HOST_STATUS_STOPPING)
		return -EBUSY;

	if (ipts->status == IPTS_HOST_STATUS_STOPPED)
		return -EBUSY;

	dev_info(ipts->dev, "Stopping IPTS\n");
	ipts->status = IPTS_HOST_STATUS_STOPPING;

	ipts_uapi_unlink();
	ipts_resources_free(ipts);
	return ipts_control_send(ipts, IPTS_CMD_CLEAR_MEM_WINDOW, NULL, 0);
}

int ipts_control_restart(struct ipts_context *ipts)
{
	if (ipts->restart)
		return -EBUSY;

	ipts->restart = true;
	return ipts_control_stop(ipts);
}

