// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/types.h>

#include "cmd.h"
#include "context.h"
#include "control.h"
#include "hid.h"
#include "resources.h"
#include "spec-device.h"

static void ipts_receiver_next_doorbell(struct ipts_context *ipts)
{
	u32 *doorbell = (u32 *)ipts->resources.doorbell.address;
	*doorbell = *doorbell + 1;
}

static u32 ipts_receiver_current_doorbell(struct ipts_context *ipts)
{
	u32 *doorbell = (u32 *)ipts->resources.doorbell.address;
	return *doorbell;
}

static void ipts_receiver_backoff(time64_t last, u32 n)
{
	/*
	 * If the last change was less than n seconds ago,
	 * sleep for a shorter period so that new data can be
	 * processed quickly. If there was no change for more than
	 * n seconds, sleep longer to avoid wasting CPU cycles.
	 */
	if (last + n > ktime_get_seconds())
		msleep(20);
	else
		msleep(200);
}

static int ipts_receiver_event_loop(void *data)
{
	int ret;
	u32 buffer;

	struct ipts_context *ipts = data;
	time64_t last = ktime_get_seconds();

	if (!ipts)
		return -EFAULT;

	dev_info(ipts->dev, "IPTS running in event mode\n");

	while (!kthread_should_stop()) {
		ret = ipts_control_wait_data(ipts, false);
		if (ret == -EAGAIN) {
			ipts_receiver_backoff(last, 5);
			continue;
		}

		if (ret) {
			dev_err(ipts->dev, "Failed to read ready for data response: %d\n", ret);
			continue;
		}

		buffer = ipts_receiver_current_doorbell(ipts) % IPTS_BUFFERS;
		ipts_receiver_next_doorbell(ipts);

		ret = ipts_hid_input_data(ipts, buffer);
		if (ret)
			dev_err(ipts->dev, "Failed to process buffer: %d\n", ret);

		ret = ipts_control_refill_buffer(ipts, buffer);
		if (ret)
			dev_err(ipts->dev, "Failed to send feedback: %d\n", ret);

		ret = ipts_control_request_data(ipts);
		if (ret)
			dev_err(ipts->dev, "Failed to request data: %d\n", ret);

		last = ktime_get_seconds();
	}

	ret = ipts_control_request_flush(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to request flush: %d\n", ret);
		return ret;
	}

	ret = ipts_control_wait_data(ipts, true);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for data: %d\n", ret);
		return ret;
	}

	ret = ipts_control_wait_flush(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for flush: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ipts_receiver_doorbell_loop(void *data)
{
	int ret;
	u32 buffer;

	u32 doorbell = 0;
	u32 lastdb = 0;

	struct ipts_context *ipts = data;
	time64_t last = ktime_get_seconds();

	if (!ipts)
		return -EFAULT;

	dev_info(ipts->dev, "IPTS running in doorbell mode\n");

	while (true) {
		if (kthread_should_stop()) {
			ret = ipts_control_request_flush(ipts);
			if (ret) {
				dev_err(ipts->dev, "Failed to request flush: %d\n", ret);
				return ret;
			}
		}

		doorbell = ipts_receiver_current_doorbell(ipts);

		/*
		 * After filling up one of the data buffers, IPTS will increment
		 * the doorbell. The value of the doorbell stands for the *next*
		 * buffer that IPTS is going to fill.
		 */
		while (lastdb != doorbell) {
			buffer = lastdb % IPTS_BUFFERS;

			ret = ipts_hid_input_data(ipts, buffer);
			if (ret)
				dev_err(ipts->dev, "Failed to process buffer: %d\n", ret);

			ret = ipts_control_refill_buffer(ipts, buffer);
			if (ret)
				dev_err(ipts->dev, "Failed to send feedback: %d\n", ret);

			last = ktime_get_seconds();
			lastdb++;
		}

		if (kthread_should_stop())
			break;

		ipts_receiver_backoff(last, 5);
	}

	ret = ipts_control_wait_data(ipts, true);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for data: %d\n", ret);
		return ret;
	}

	ret = ipts_control_wait_flush(ipts);
	if (ret) {
		dev_err(ipts->dev, "Failed to wait for flush: %d\n", ret);
		return ret;
	}

	return 0;
}

void ipts_receiver_start(struct ipts_context *ipts)
{
	if (!ipts)
		return;

	if (ipts->mode == IPTS_MODE_EVENT)
		ipts->event_loop = kthread_run(ipts_receiver_event_loop, ipts, "ipts_event");

	if (ipts->mode == IPTS_MODE_DOORBELL)
		ipts->doorbell_loop = kthread_run(ipts_receiver_doorbell_loop, ipts, "ipts_db");
}

void ipts_receiver_stop(struct ipts_context *ipts)
{
	if (!ipts)
		return;

	if (ipts->event_loop) {
		kthread_stop(ipts->event_loop);
		ipts->event_loop = NULL;
	}

	if (ipts->doorbell_loop) {
		kthread_stop(ipts->doorbell_loop);
		ipts->doorbell_loop = NULL;
	}
}
