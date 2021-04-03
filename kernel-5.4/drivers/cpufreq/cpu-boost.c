/*
 * Copyright (C) 2014 Google, Inc.
 *
 * Loosely based on cpu-boost.c from Android tree
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/slab.h>

static unsigned int cpuboost_input_boost_freq_percent;
module_param_named(input_boost_freq_percent,
		   cpuboost_input_boost_freq_percent, uint, 0644);
MODULE_PARM_DESC(input_boost_freq_percent,
		 "Percentage of max frequency of CPU to be used as boost frequency");

static unsigned int cpuboost_input_boost_ms = 40;
module_param_named(input_boost_ms,
		   cpuboost_input_boost_ms, uint, 0644);
MODULE_PARM_DESC(input_boost_ms, "Duration of input boost (msec)");

static unsigned int cpuboost_input_boost_interval_ms = 150;
module_param_named(input_boost_interval_ms,
		   cpuboost_input_boost_interval_ms, uint, 0644);
MODULE_PARM_DESC(input_boost_ms,
		 "Interval between input events to reactivate input boost (msec)");

static bool cpuboost_boost_active;

static void cpuboost_toggle_boost(bool boost_active)
{
	unsigned int cpu;

	cpuboost_boost_active = boost_active;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		pr_debug("%s input boost on CPU%d\n",
			 boost_active ? "Starting" : "Removing", cpu);
		cpufreq_update_policy(cpu);
	}
	put_online_cpus();
}

static void cpuboost_cancel_input_boost(struct work_struct *work)
{
	cpuboost_toggle_boost(false);
}
static DECLARE_DELAYED_WORK(cpuboost_cancel_boost_work,
			    cpuboost_cancel_input_boost);

static void cpuboost_do_input_boost(struct work_struct *work)
{
	mod_delayed_work(system_wq, &cpuboost_cancel_boost_work,
			 msecs_to_jiffies(cpuboost_input_boost_ms));

	cpuboost_toggle_boost(true);
}
static DECLARE_WORK(cpuboost_input_boost_work, cpuboost_do_input_boost);

static void cpuboost_input_event(struct input_handle *handle,
				 unsigned int type, unsigned int code,
				 int value)
{
	static unsigned long last_event_time;
	unsigned long now = jiffies;
	unsigned int threshold;

	if (!cpuboost_input_boost_freq_percent)
		return;

	threshold = msecs_to_jiffies(cpuboost_input_boost_interval_ms);
	if (time_after(now, last_event_time + threshold))
		queue_work(system_highpri_wq, &cpuboost_input_boost_work);

	last_event_time = now;
}

static int cpuboost_input_connect(struct input_handler *handler,
				  struct input_dev *dev,
				  const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpu-boost";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;

err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void cpuboost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id cpuboost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			    BIT_MASK(ABS_MT_POSITION_X) |
			    BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_X)] = BIT_MASK(ABS_X) }

	}, /* stylus or joystick device */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) },
	}, /* pointer (e.g. trackpad, mouse) */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(KEY_ESC)] = BIT_MASK(KEY_ESC) },
	}, /* keyboard */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = {[BIT_WORD(BTN_JOYSTICK)] = BIT_MASK(BTN_JOYSTICK) },
	}, /* joysticks not caught by ABS_X above */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_GAMEPAD)] = BIT_MASK(BTN_GAMEPAD) },
	}, /* gamepad */
	{ },
};

static struct input_handler cpuboost_input_handler = {
	.event          = cpuboost_input_event,
	.connect        = cpuboost_input_connect,
	.disconnect     = cpuboost_input_disconnect,
	.name           = "cpu-boost",
	.id_table       = cpuboost_ids,
};

static int __init cpuboost_init(void)
{
	int error;

	error = input_register_handler(&cpuboost_input_handler);
	if (error) {
		pr_err("failed to register input handler: %d\n", error);
		return error;
	}

	return 0;
}
module_init(cpuboost_init);

static void __exit cpuboost_exit(void)
{
	input_unregister_handler(&cpuboost_input_handler);

	flush_work(&cpuboost_input_boost_work);
	cancel_delayed_work_sync(&cpuboost_cancel_boost_work);
}
module_exit(cpuboost_exit);

MODULE_DESCRIPTION("Input event based short term CPU frequency booster");
MODULE_LICENSE("GPL v2");
