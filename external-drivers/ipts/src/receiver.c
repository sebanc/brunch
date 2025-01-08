// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/types.h>

#include "receiver.h"
#include "context.h"
#include "control.h"
#include "hid.h"
#include "resources.h"
#include "spec-dma.h"
#include "spec-mei.h"
#include "thread.h"

static int ipts_receiver_event(struct ipts_thread *thread)
{
	int ret = 0;
	struct ipts_context *ipts = thread->data;

	dev_info(ipts->dev, "IPTS running in event mode\n");

	while (!ipts_thread_should_stop(thread)) {
		struct ipts_rsp_ready_for_data rsp = { 0 };
		struct ipts_data_buffer *buffer = NULL;

		ret = ipts_control_wait_data(ipts, &rsp);
		if (ret == -EAGAIN)
			continue;

		if (ret) {
			dev_err(ipts->dev, "Failed to wait for data: %d\n", ret);
			continue;
		}

		buffer = (struct ipts_data_buffer *)ipts->resources.data[rsp.buffer_index].address;

		ret = ipts_hid_input_data(ipts, buffer);
		if (ret)
			dev_err(ipts->dev, "Failed to process buffer: %d\n", ret);

		ret = ipts_control_refill_buffer(ipts, buffer);
		if (ret)
			dev_err(ipts->dev, "Failed to send feedback: %d\n", ret);

		ret = ipts_control_request_data(ipts);
		if (ret)
			dev_err(ipts->dev, "Failed to request data: %d\n", ret);
	}

	ret = ipts_control_request_flush(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to request flush: %d\n", ret);
		return ret;
	}

	ret = ipts_control_wait_data(ipts, NULL);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for data: %d\n", ret);

		if (ret != -EAGAIN)
			return ret;
		else
			return 0;
	}

	ret = ipts_control_wait_flush(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for flush: %d\n", ret);

		if (ret != -EAGAIN)
			return ret;
		else
			return 0;
	}

	return 0;
}

static int ipts_receiver_poll(struct ipts_thread *thread)
{
	int ret = 0;

	struct ipts_context *ipts = thread->data;
	time64_t last = ktime_get_seconds();

	u32 current_buffer = 0;
	u32 next_buffer = 0;

	dev_info(ipts->dev, "IPTS running in poll mode\n");

	while (true) {
		if (ipts_thread_should_stop(thread)) {
			ret = ipts_control_request_flush(ipts);
			if (ret) {
				dev_err(ipts->dev, "Failed to request flush: %d\n", ret);
				return ret;
			}

			/*
			 * We have to process all outstanding data for the flush to succeed.
			 */
		}

		/*
		 * After filling up one of the data buffers, the ME will increment the doorbell.
		 * The value of the doorbell stands for the *next* buffer that the ME will fill.
		 *
		 * We read the doorbell address only once to force the loop to sleep at some point.
		 */
		next_buffer = *(u32 *)ipts->resources.doorbell.address;

		while (current_buffer != next_buffer) {
			struct ipts_data_buffer *buffer = NULL;
			size_t index = current_buffer % ipts->buffers;

			buffer = (struct ipts_data_buffer *)ipts->resources.data[index].address;

			ret = ipts_hid_input_data(ipts, buffer);
			if (ret)
				dev_err(ipts->dev, "Failed to process buffer: %d\n", ret);

			ret = ipts_control_refill_buffer(ipts, buffer);
			if (ret)
				dev_err(ipts->dev, "Failed to send feedback: %d\n", ret);

			last = ktime_get_seconds();
			current_buffer++;
		}

		if (ipts_thread_should_stop(thread))
			break;

		/*
		 * If the last change was less than 5 seconds ago, sleep for a shorter period so
		 * that new data can be processed quickly. If there was no change for more than
		 * 5 seconds, sleep longer to avoid wasting CPU cycles.
		 */
		if (last + 5 > ktime_get_seconds())
			usleep_range(1 * USEC_PER_MSEC, 5 * USEC_PER_MSEC);
		else
			msleep(200);
	}

	ret = ipts_control_wait_data(ipts, NULL);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for data: %d\n", ret);

		if (ret != -EAGAIN)
			return ret;
		else
			return 0;
	}

	ret = ipts_control_wait_flush(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for flush: %d\n", ret);

		if (ret != -EAGAIN)
			return ret;
		else
			return 0;
	}

	return 0;
}

int ipts_receiver_start(struct ipts_context *ipts)
{
	int ret = -EINVAL;

	if (ipts->mode == IPTS_MODE_EVENT)
		ret = ipts_thread_start(&ipts->receiver, ipts_receiver_event, ipts, "ipts_event");
	else if (ipts->mode == IPTS_MODE_POLL)
		ret = ipts_thread_start(&ipts->receiver, ipts_receiver_poll, ipts, "ipts_poll");

	if (ret) {
		dev_err(ipts->dev, "Failed to start receiver loop: %d\n", ret);
		return ret;
	}

	return 0;
}

int ipts_receiver_stop(struct ipts_context *ipts)
{
	int ret = 0;

	ret = ipts_thread_stop(&ipts->receiver);
	if (ret) {
		dev_err(ipts->dev, "Failed to stop receiver loop: %d\n", ret);
		return ret;
	}

	return 0;
}
