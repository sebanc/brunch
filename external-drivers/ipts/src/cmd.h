/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_CMD_H
#define IPTS_CMD_H

#include <linux/types.h>

#include "context.h"
#include "spec-device.h"

/*
 * Executes the specified command with the given payload on the device.
 */
int ipts_cmd_send(struct ipts_context *ipts, enum ipts_command_code code, void *data, size_t size);

/*
 * Receives the response to the given command and copies the payload to the given buffer.
 * This function will block until a message has been received.
 */
int ipts_cmd_recv(struct ipts_context *ipts, enum ipts_command_code code, void *data, size_t size);

/*
 * Receives the response to the given command and copies the payload to the given buffer.
 * This function will not block. If no data is available, -EAGAIN will be returned.
 */
int ipts_cmd_recv_nonblock(struct ipts_context *ipts, enum ipts_command_code code, void *data,
			   size_t size);

/*
 * Receives the response to the given command and copies the payload to the given buffer.
 * This function will block until a message has been received.
 * If the command finished with the expected status code, no error will be produced.
 */
int ipts_cmd_recv_expect(struct ipts_context *ipts, enum ipts_command_code code, void *data,
			 size_t size, enum ipts_status expect);

/*
 * Executes the specified command with the given payload on the device. Then
 * receives the response to the command and copies the payload to the given buffer.
 * This function will block until the command has been completed.
 */
int ipts_cmd_run(struct ipts_context *ipts, enum ipts_command_code code, void *in, size_t insize,
		 void *out, size_t outsize);

#endif /* IPTS_CMD_H */
