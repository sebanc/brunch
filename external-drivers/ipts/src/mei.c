// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/delay.h>
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
#include "spec-mei.h"

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
	int i = 0;
	ssize_t ret = 0;

	struct ipts_mei_message *entry = NULL;
	struct ipts_context *ipts = mei_cldev_get_drvdata(cldev);

	entry = devm_kzalloc(ipts->dev, sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;

	INIT_LIST_HEAD(&entry->list);

	/*
	 * System calls can interrupt the MEI bus API functions.
	 * If this happens, try to repeat the call until it starts working.
	 */

	for (i = 0; i < 100; i++) {
		ret = mei_cldev_recv(cldev, (u8 *)&entry->response, sizeof(entry->response));

		if (ret != -EINTR)
			break;

		msleep(100);
	}

	if (ret < 0) {
		dev_err(ipts->dev, "Failed to read MEI message: %ld\n", ret);
		return;
	}

	if (ret == 0) {
		dev_err(ipts->dev, "Received empty MEI message\n");
		return;
	}

	dev_dbg(ipts->dev, "MEI thread received message with code 0x%X and status 0x%X\n",
		entry->response.cmd, entry->response.status);

	locked_list_add(&entry->list, &ipts->mei.messages, &ipts->mei.message_lock);
	wake_up_all(&ipts->mei.message_queue);
}

static int ipts_mei_search(struct ipts_mei *mei, enum ipts_command_code code,
			   struct ipts_response *response)
{
	struct ipts_mei_message *entry = NULL;

	down_read(&mei->message_lock);

	/*
	 * Iterate over the list of received messages, and check if there is one
	 * matching the requested command code.
	 */
	list_for_each_entry(entry, &mei->messages, list) {
		if (entry->response.cmd == IPTS_ME_2_HOST_MSG(code))
			break;
	}

	up_read(&mei->message_lock);

	/*
	 * If entry is not the list head, this means that the loop above has been stopped early,
	 * and that we found a matching element. We drop the message from the list and return it.
	 */
	if (!list_entry_is_head(entry, &mei->messages, list)) {
		locked_list_del(&entry->list, &mei->message_lock);

		*response = entry->response;
		devm_kfree(&mei->cldev->dev, entry);

		dev_dbg(&mei->cldev->dev, "Driver read message with code 0x%X and status 0x%X\n",
			response->cmd, response->status);

		if (response->status == IPTS_STATUS_TIMEOUT)
			return -EAGAIN;

		/*
		 * Ignore all errors that the spec allows us to ignore.
		 */

		if (response->status == IPTS_STATUS_COMPAT_CHECK_FAIL)
			response->status = IPTS_STATUS_SUCCESS;

		if (response->status == IPTS_STATUS_INVALID_DEVICE_CAPS)
			response->status = IPTS_STATUS_SUCCESS;

		if (response->status == IPTS_STATUS_SENSOR_FAIL_NONFATAL)
			response->status = IPTS_STATUS_SUCCESS;

		return 0;
	}

	return -EAGAIN;
}

int ipts_mei_recv_timeout(struct ipts_mei *mei, enum ipts_command_code code,
			  struct ipts_response *response, u64 timeout)
{
	int ret = 0;

	/*
	 * A timeout of 0 means check and return immideately.
	 */
	if (timeout == 0)
		return ipts_mei_search(mei, code, response);

	/*
	 * A timeout of less than 0 means to wait forever.
	 */
	if (timeout < 0) {
		wait_event(mei->message_queue, ipts_mei_search(mei, code, response) == 0);
		return 0;
	}

	ret = wait_event_timeout(mei->message_queue, ipts_mei_search(mei, code, response) == 0,
				 msecs_to_jiffies(timeout));

	if (ret > 0)
		return 0;

	return -EAGAIN;
}

int ipts_mei_send(struct ipts_mei *mei, enum ipts_command_code code, void *payload, size_t size)
{
	int i = 0;
	int ret = 0;

	struct ipts_command cmd = { 0 };

	cmd.cmd = code;

	if (payload && size > 0)
		memcpy(cmd.payload.raw, payload, size);

	dev_dbg(&mei->cldev->dev, "Driver sent message with code 0x%X and %ld bytes payload\n",
		code, size);

	/*
	 * System calls can interrupt the MEI bus API functions.
	 * If this happens, try to repeat the call until it starts working.
	 */

	for (i = 0; i < 100; i++) {
		ret = mei_cldev_send(mei->cldev, (u8 *)&cmd, sizeof(cmd.cmd) + size);

		if (ret != -EINTR)
			break;

		msleep(100);
	}

	if (ret < 0) {
		dev_err(&mei->cldev->dev, "Failed to send MEI message: %d\n", ret);
		return ret;
	}

	return 0;
}

void ipts_mei_init(struct ipts_mei *mei, struct mei_cl_device *cldev)
{
	mei->cldev = cldev;

	INIT_LIST_HEAD(&mei->messages);
	init_waitqueue_head(&mei->message_queue);
	init_rwsem(&mei->message_lock);

	mei_cldev_register_rx_cb(cldev, ipts_mei_incoming);
}
