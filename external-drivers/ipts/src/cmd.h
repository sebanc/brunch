/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_CMD_H
#define IPTS_CMD_H

#include <linux/types.h>

#include "context.h"
#include "spec-device.h"

/*
 * The default timeout for receiving responses
 */
#define IPTS_CMD_DEFAULT_TIMEOUT 1000

/*
 * ipts_cmd_recv_timeout() - Receives a response to a command.
 * @ipts: The IPTS driver context.
 * @code: The type of the command / response.
 * @rsp: The address that the received response will be copied to.
 * @timeout: How many milliseconds the function will wait at most.
 *
 * A negative timeout means to wait forever.
 *
 * Returns: 0 on success, <0 on error, -EAGAIN if no response has been received.
 */
int ipts_cmd_recv_timeout(struct ipts_context *ipts, enum ipts_command_code code,
			  struct ipts_response *rsp, u64 timeout);

/*
 * ipts_cmd_recv() - Receives a response to a command.
 * @ipts: The IPTS driver context.
 * @code: The type of the command / response.
 * @rsp: The address that the received response will be copied to.
 *
 * Returns: 0 on success, <0 on error, -EAGAIN if no response has been received.
 */
static inline int ipts_cmd_recv(struct ipts_context *ipts, enum ipts_command_code code,
				struct ipts_response *rsp)
{
	return ipts_cmd_recv_timeout(ipts, code, rsp, IPTS_CMD_DEFAULT_TIMEOUT);
}

/*
 * ipts_cmd_send() - Executes a command on the device.
 * @ipts: The IPTS driver context.
 * @code: The type of the command to execute.
 * @data: The payload containing parameters for the command.
 * @size: The size of the payload.
 *
 * Returns: 0 on success, <0 on error.
 */
int ipts_cmd_send(struct ipts_context *ipts, enum ipts_command_code code, void *data, size_t size);

#endif /* IPTS_CMD_H */
