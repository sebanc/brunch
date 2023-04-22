// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/mei_cl_bus.h>
#include <linux/printk.h>
#include <linux/rwsem.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "context.h"
#include "mei.h"

static void locked_list_add(struct list_head *new, struct list_head *head,
			    struct rw_semaphore *lock)
{
	down_write(lock);
	list_add(new, head);
	up_write(lock);
}

static void locked_list_del(struct list_head *entry, struct rw_semaphore *lock)
{
	down_write(lock);
	list_del(entry);
	up_write(lock);
}

static void ipts_mei_incoming(struct mei_cl_device *cldev)
{
	ssize_t ret = 0;
	struct ipts_mei_message *entry = NULL;
	struct ipts_context *ipts = NULL;

	if (!cldev) {
		pr_err("MEI device is NULL!");
		return;
	}

	ipts = mei_cldev_get_drvdata(cldev);
	if (!ipts) {
		pr_err("IPTS driver context is NULL!");
		return;
	}

	entry = devm_kzalloc(ipts->dev, sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;

	INIT_LIST_HEAD(&entry->list);

	do {
		ret = mei_cldev_recv(cldev, (u8 *)&entry->rsp, sizeof(entry->rsp));
	} while (ret == -EINTR);

	if (ret < 0) {
		dev_err(ipts->dev, "Error while reading response: %ld\n", ret);
		return;
	}

	if (ret == 0) {
		dev_err(ipts->dev, "Received empty response\n");
		return;
	}

	locked_list_add(&entry->list, &ipts->mei.messages, &ipts->mei.message_lock);
	wake_up_all(&ipts->mei.message_queue);
}

static int ipts_mei_search(struct ipts_mei *mei, enum ipts_command_code code,
			   struct ipts_response *rsp)
{
	struct ipts_mei_message *entry = NULL;

	if (!mei)
		return -EFAULT;

	if (!rsp)
		return -EFAULT;

	down_read(&mei->message_lock);

	/*
	 * Iterate over the list of received messages, and check if there is one
	 * matching the requested command code.
	 */
	list_for_each_entry(entry, &mei->messages, list) {
		if (entry->rsp.cmd == code)
			break;
	}

	up_read(&mei->message_lock);

	/*
	 * If entry is not the list head, this means that the loop above has been stopped early,
	 * and that we found a matching element. We drop the message from the list and return it.
	 */
	if (!list_entry_is_head(entry, &mei->messages, list)) {
		locked_list_del(&entry->list, &mei->message_lock);

		*rsp = entry->rsp;
		devm_kfree(&mei->cldev->dev, entry);

		return 0;
	}

	return -EAGAIN;
}

int ipts_mei_recv(struct ipts_mei *mei, enum ipts_command_code code, struct ipts_response *rsp,
		  u64 timeout)
{
	int ret = 0;

	if (!mei)
		return -EFAULT;

	/*
	 * A timeout of 0 means check and return immideately.
	 */
	if (timeout == 0)
		return ipts_mei_search(mei, code, rsp);

	/*
	 * A timeout of less than 0 means to wait forever.
	 */
	if (timeout < 0) {
		wait_event(mei->message_queue, ipts_mei_search(mei, code, rsp) == 0);
		return 0;
	}

	ret = wait_event_timeout(mei->message_queue, ipts_mei_search(mei, code, rsp) == 0,
				 msecs_to_jiffies(timeout));

	if (ret > 0)
		return 0;

	return -EAGAIN;
}

int ipts_mei_send(struct ipts_mei *mei, void *data, size_t length)
{
	int ret = 0;

	if (!mei)
		return -EFAULT;

	if (!mei->cldev)
		return -EFAULT;

	if (!data)
		return -EFAULT;

	do {
		ret = mei_cldev_send(mei->cldev, (u8 *)data, length);
	} while (ret == -EINTR);

	if (ret < 0)
		return ret;

	return 0;
}

int ipts_mei_init(struct ipts_mei *mei, struct mei_cl_device *cldev)
{
	if (!mei)
		return -EFAULT;

	if (!cldev)
		return -EFAULT;

	mei->cldev = cldev;

	INIT_LIST_HEAD(&mei->messages);
	init_waitqueue_head(&mei->message_queue);
	init_rwsem(&mei->message_lock);

	mei_cldev_register_rx_cb(cldev, ipts_mei_incoming);

	return 0;
}
