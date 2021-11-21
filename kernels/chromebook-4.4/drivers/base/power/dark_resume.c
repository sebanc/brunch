/* Copyright (C) 2013 Google, Inc.
 *
 * Author:
 *      Derek Basehore <dbasehore@chromium.org>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A dark resume is meant to be a reduced functionality state to resume the
 * system to. It is not meant for user interaction, but to determine whether to
 * shut down/hibernate or suspend again. This is done for power saving measures.
 * To make the experience as seamless as possible drivers may want to alter
 * their resume path to do something different in this case (prevent flashing
 * screens and blinking usb ports).
 *
 * To make this system agnostic, much of the configuration is done in user
 * space.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/pm_dark_resume.h>
#include <linux/suspend.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "power.h"

LIST_HEAD(source_list);
static DEFINE_MUTEX(source_list_lock);
static bool dark_resume_enabled;
static bool dark_resume_always;

/**
 * dev_dark_resume_set_active - This sets up the device to check the dark resume
 * state during resume.
 * @dev: device to set querying of the dark resume state to active or inactive
 * @is_active: true for the device to query the dark resume state, false to not
 * query the dark resume state.
 *
 * Part of dark resume is that devices may resume differently (such as the
 * backlight coming on). This enables a driver to do something different in the
 * resume path by calling dev_dark_resume_active.
 */
void dev_dark_resume_set_active(struct device *dev, bool is_active)
{
	dev->power.use_dark_resume = is_active;
}
EXPORT_SYMBOL_GPL(dev_dark_resume_set_active);

/**
 * dev_dark_resume_add_consumer - Initialize dark resume consumer state and adds
 * a sysfs within the devices power directory.
 * @dev: The device which will observe dark resumes if enabled via sysfs
 */
void dev_dark_resume_add_consumer(struct device *dev)
{
	/* Must be called after device_add since sysfs attributes are added */
	if (!device_is_registered(dev))
		return;

	dev_dark_resume_set_active(dev, false);
	dark_resume_consumer_sysfs_add(dev);
}
EXPORT_SYMBOL_GPL(dev_dark_resume_add_consumer);

/**
 * dev_dark_resume_remove_consumer - Set dark resume consumer state and remove
 * sysfs file.
 * @dev: device struct to remove associations to dark resume from.
 */
void dev_dark_resume_remove_consumer(struct device *dev)
{
	dev_dark_resume_set_active(dev, false);
	dark_resume_consumer_sysfs_remove(dev);
}
EXPORT_SYMBOL_GPL(dev_dark_resume_remove_consumer);

/**
 * pm_dark_resume_active - Returns the state of dark resume.
 */
bool pm_dark_resume_active(void)
{
	return dark_resume_enabled && (dark_resume_always ||
	       pm_get_wakeup_source_type() == WAKEUP_AUTOMATIC);
}
EXPORT_SYMBOL_GPL(pm_dark_resume_active);

/**
 * pm_dark_resume_always - Returns whether we always wake up in dark resume from
 * a suspend.
 */
bool pm_dark_resume_always(void)
{
	return dark_resume_always;
}
EXPORT_SYMBOL_GPL(pm_dark_resume_always);

/**
 * pm_dark_resume_set_always - Sets whether we always wake up in dark resume
 * from a suspend.
 * @always: bool that tells whether we always wake up in dark resume
 */
void pm_dark_resume_set_always(bool always)
{
	dark_resume_always = always;
}
EXPORT_SYMBOL_GPL(pm_dark_resume_set_always);

void pm_dark_resume_set_enabled(bool enabled)
{
	dark_resume_enabled = enabled;
}

static int dark_resume_dvr_suspend(void)
{
	pm_dark_resume_set_enabled(true);
	return NOTIFY_OK;
}

static int dark_resume_pm_notifier(struct notifier_block *this,
			   unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		return dark_resume_dvr_suspend();
	default:
		return NOTIFY_DONE;
	}
}

static struct notifier_block pm_notifier = {
	.notifier_call = dark_resume_pm_notifier,
};

static int __init dark_resume_init(void)
{
	register_pm_notifier(&pm_notifier);
	return 0;
}

static void __exit dark_resume_exit(void)
{
	unregister_pm_notifier(&pm_notifier);
}

late_initcall(dark_resume_init);
module_exit(dark_resume_exit);
