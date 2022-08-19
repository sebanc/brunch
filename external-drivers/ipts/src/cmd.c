// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/errno.h>
#include <linux/types.h>

#include "context.h"
#include "spec-device.h"

static int ipts_cmd_get_errno(struct ipts_response rsp, enum ipts_status expect)
{
	if (rsp.status == IPTS_STATUS_SUCCESS)
		return 0;

	/*
	 * If a status code was expected, dont produce an error.
	 */
	if (rsp.status == expect)
		return 0;

	/*
	 * Some devices will always return this error. It is allowed
	 * to ignore it and to try continuing.
	 */
	if (rsp.status == IPTS_STATUS_COMPAT_CHECK_FAIL)
		return 0;

	/*
	 * Return something, this is not going to be checked.
	 * Any error will just cause the driver to stop.
	 */
	return -EINVAL;
}

static int _ipts_cmd_recv(struct ipts_context *ipts, enum ipts_command_code code, void *data,
			  size_t size, bool block, enum ipts_status expect)
{
	int ret;
	struct ipts_response rsp = { 0 };

	if (!ipts)
		return -EFAULT;

	if (size > sizeof(rsp.payload))
		return -EINVAL;

	if (block)
		ret = mei_cldev_recv(ipts->cldev, (u8 *)&rsp, sizeof(rsp));
	else
		ret = mei_cldev_recv_nonblock(ipts->cldev, (u8 *)&rsp, sizeof(rsp));

	if (ret == -EAGAIN)
		return ret;

	if (ret <= 0) {
		dev_err(ipts->dev, "Error while reading response: %d\n", ret);
		return ret;
	}

	/*
	 * In a response, the command code will have the most significant bit
	 * flipped to one. We check for this and then set the bit to 0.
	 */
	if ((rsp.cmd & 0x80000000) == 0) {
		dev_err(ipts->dev, "Invalid command code received: 0x%02X\n", rsp.cmd);
		return -EINVAL;
	}

	rsp.cmd = rsp.cmd & ~(0x80000000);
	if (rsp.cmd != code) {
		dev_err(ipts->dev, "Received response to wrong command: 0x%02X\n", rsp.cmd);
		return -EINVAL;
	}

	ret = ipts_cmd_get_errno(rsp, expect);
	if (ret) {
		dev_err(ipts->dev, "Command 0x%02X failed: 0x%02X\n", rsp.cmd, rsp.status);
		return ret;
	}

	if (data && size > 0)
		memcpy(data, rsp.payload, size);

	return 0;
}

int ipts_cmd_send(struct ipts_context *ipts, enum ipts_command_code code, void *data, size_t size)
{
	int ret;
	struct ipts_command cmd = { 0 };

	if (!ipts)
		return -EFAULT;

	cmd.cmd = code;

	if (data && size > 0)
		memcpy(cmd.payload, data, size);

	ret = mei_cldev_send(ipts->cldev, (u8 *)&cmd, sizeof(cmd.cmd) + size);
	if (ret >= 0)
		return 0;

	dev_err(ipts->dev, "Error while sending: 0x%02X:%d\n", code, ret);
	return ret;
}

int ipts_cmd_recv(struct ipts_context *ipts, enum ipts_command_code code, void *data, size_t size)
{
	return _ipts_cmd_recv(ipts, code, data, size, true, IPTS_STATUS_SUCCESS);
}

int ipts_cmd_recv_nonblock(struct ipts_context *ipts, enum ipts_command_code code, void *data,
			   size_t size)
{
	return _ipts_cmd_recv(ipts, code, data, size, false, IPTS_STATUS_SUCCESS);
}

int ipts_cmd_recv_expect(struct ipts_context *ipts, enum ipts_command_code code, void *data,
			 size_t size, enum ipts_status expect)
{
	return _ipts_cmd_recv(ipts, code, data, size, true, expect);
}

int ipts_cmd_run(struct ipts_context *ipts, enum ipts_command_code code, void *in, size_t insize,
		 void *out, size_t outsize)
{
	int ret;

	ret = ipts_cmd_send(ipts, code, in, insize);
	if (ret)
		return ret;

	ret = ipts_cmd_recv(ipts, code, out, outsize);
	if (ret)
		return ret;

	return 0;
}
