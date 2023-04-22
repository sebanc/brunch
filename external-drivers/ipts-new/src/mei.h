/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_MEI_H
#define IPTS_MEI_H

#include <linux/list.h>
#include <linux/mei_cl_bus.h>
#include <linux/rwsem.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "spec-device.h"

struct ipts_mei_message {
	struct list_head list;
	struct ipts_response rsp;
};

struct ipts_mei {
	struct mei_cl_device *cldev;

	struct list_head messages;

	wait_queue_head_t message_queue;
	struct rw_semaphore message_lock;
};

/*
 * ipts_mei_recv() - Receive data from a MEI device.
 * @mei: The IPTS MEI device context.
 * @code: The IPTS command code to look for.
 * @rsp: The address that the received data will be copied to.
 * @timeout: How many milliseconds the function will wait at most.
 *
 * A negative timeout means to wait forever.
 *
 * Returns: 0 on success, <0 on error, -EAGAIN if no response has been received.
 */
int ipts_mei_recv(struct ipts_mei *mei, enum ipts_command_code code, struct ipts_response *rsp,
		  u64 timeout);

/*
 * ipts_mei_send() - Send data to a MEI device.
 * @ipts: The IPTS MEI device context.
 * @data: The data to send.
 * @size: The size of the data.
 *
 * Returns: 0 on success, <0 on error.
 */
int ipts_mei_send(struct ipts_mei *mei, void *data, size_t length);

/*
 * ipts_mei_init() - Initialize the MEI device context.
 * @mei: The MEI device context to initialize.
 * @cldev: The MEI device the context will be bound to.
 *
 * Returns: 0 on success, <0 on error.
 */
int ipts_mei_init(struct ipts_mei *mei, struct mei_cl_device *cldev);

#endif /* IPTS_MEI_H */
