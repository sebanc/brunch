/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
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

#include "spec-mei.h"

/**
 * struct ipts_mei - Wrapper for interacting with the MEI bus.
 *
 * It is possible that the ME will sometimes send messages out of order. To universally support
 * this behaviour, the wrapper will store incoming messages in a list and then implement a
 * custom receive logic on top of that by explicitly searching for a certain command code.
 *
 * The MEI API also only allows the driver to either wait forever, or not at all. This wrapper
 * adds the ability to wait for a configurable amount of time and then return -EAGAIN. The caller
 * can then decide if it wants to try again or error out.
 *
 * @cldev:
 *     The MEI client device.
 *
 * @messages:
 *     The list of received messages. See &struct ipts_mei_message.
 *
 * @message_lock:
 *     Prevent multiple threads from writing to the list of messages at the same time.
 *
 * @message_queue:
 *     Wait queue where callers can wait for new messages with the correct command code.
 */
struct ipts_mei {
	struct mei_cl_device *cldev;

	struct list_head messages;
	struct rw_semaphore message_lock;

	wait_queue_head_t message_queue;
};

struct ipts_mei_message {
	struct list_head list;
	struct ipts_response response;
};

int ipts_mei_send(struct ipts_mei *mei, enum ipts_command_code code, void *payload, size_t size);
int ipts_mei_recv_timeout(struct ipts_mei *mei, enum ipts_command_code code,
			  struct ipts_response *response, u64 timeout);

static inline int ipts_mei_recv(struct ipts_mei *mei, enum ipts_command_code code,
				struct ipts_response *response)
{
	return ipts_mei_recv_timeout(mei, code, response, 1 * MSEC_PER_SEC);
}

void ipts_mei_init(struct ipts_mei *mei, struct mei_cl_device *cldev);

#endif /* IPTS_MEI_H */
