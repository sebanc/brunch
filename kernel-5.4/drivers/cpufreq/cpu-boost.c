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
#include <linux/notifier.h>
#include <linux/pm_qos.h>
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
MODULE_PARM_DESC(input_boost_interval_ms,
		 "Interval between input events to reactivate input boost (msec)");

DEFINE_MUTEX(cpuboost_mutex);

static bool cpuboost_boost_active;

static LIST_HEAD(cpuboost_policy_list);

struct cpuboost_policy_node {
	struct list_head policy_list;
	struct freq_qos_request qos_req;
	struct cpufreq_policy *policy;
};

static int cpuboost_policy_notifier(struct notifier_block *nb,
				    unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpuboost_policy_node *node;
	int ret;
	bool found;

	switch (val) {
	case CPUFREQ_CREATE_POLICY:
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node)
			break;

		node->policy = policy;

		/*
		 * Always init to no boost and we'll get the boost the next
		 * time input comes in.
		 */
		ret = freq_qos_add_request(&policy->constraints,
					   &node->qos_req, FREQ_QOS_MIN, 0);
		if (ret < 0) {
			pr_warn("Failed to add input boost: %d\n", ret);
			kfree(node);
			break;
		}

		mutex_lock(&cpuboost_mutex);
		list_add(&node->policy_list, &cpuboost_policy_list);
		mutex_unlock(&cpuboost_mutex);

		return NOTIFY_OK;

	case CPUFREQ_REMOVE_POLICY:
		mutex_lock(&cpuboost_mutex);
		found = false;
		list_for_each_entry(node, &cpuboost_policy_list, policy_list) {
			if (node->policy == policy) {
				found = true;
				break;
			}
		}

		if (!found) {
			pr_warn("Couldn't find input boost for policy\n");
			mutex_unlock(&cpuboost_mutex);
			break;
		}
		list_del(&node->policy_list);
		mutex_unlock(&cpuboost_mutex);

		ret = freq_qos_remove_request(&node->qos_req);
		kfree(node);
		if (ret < 0) {
			pr_warn("Failed to remove input boost: %d\n", ret);
			break;
		}

		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block cpuboost_policy_nb = {
	.notifier_call = cpuboost_policy_notifier,
};

static void cpuboost_toggle_boost(bool boost_active)
{
	struct cpuboost_policy_node *node;
	int ret;
	s32 freq = 0;

	mutex_lock(&cpuboost_mutex);
	cpuboost_boost_active = boost_active;
	list_for_each_entry(node, &cpuboost_policy_list, policy_list) {
		if (boost_active)
			freq = node->policy->cpuinfo.max_freq / 100 *
			       cpuboost_input_boost_freq_percent;
		ret = freq_qos_update_request(&node->qos_req, freq);
		if (ret < 0)
			pr_warn("Error updating cpuboost request: %d\n", ret);
	}
	mutex_unlock(&cpuboost_mutex);
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

	error = cpufreq_register_notifier(&cpuboost_policy_nb,
					  CPUFREQ_POLICY_NOTIFIER);
	if (error) {
		pr_err("failed to register input handler: %d\n", error);
		return error;
	}

	error = input_register_handler(&cpuboost_input_handler);
	if (error) {
		pr_err("failed to register input handler: %d\n", error);
		cpufreq_unregister_notifier(&cpuboost_policy_nb,
					    CPUFREQ_POLICY_NOTIFIER);
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

	cpufreq_unregister_notifier(&cpuboost_policy_nb,
				    CPUFREQ_POLICY_NOTIFIER);
}
module_exit(cpuboost_exit);

MODULE_DESCRIPTION("Input event based short term CPU frequency booster");
MODULE_LICENSE("GPL v2");
