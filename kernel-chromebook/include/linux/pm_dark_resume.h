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
 */

#ifndef _LINUX_PM_DARK_RESUME_H
#define _LINUX_PM_DARK_RESUME_H

#include <linux/device.h>
#include <linux/pm.h>
#include <linux/types.h>

#ifdef CONFIG_PM_SLEEP
extern void dev_dark_resume_set_active(struct device *dev, bool is_active);
extern void dev_dark_resume_add_consumer(struct device *dev);
extern void dev_dark_resume_remove_consumer(struct device *dev);
extern bool pm_dark_resume_active(void);
extern bool pm_dark_resume_always(void);
extern void pm_dark_resume_set_always(bool always);
extern void pm_dark_resume_set_enabled(bool state);

/*
 * Wrapper for device drivers to check if it should do anything different for a
 * dark resume and whether the global dark resume state is set.
 */
static inline bool dev_dark_resume_active(struct device *dev)
{
	return dev->power.use_dark_resume & pm_dark_resume_active();
}

#else

static inline void dev_dark_resume_set_active(struct device *dev,
					      bool is_active) { }

static inline void dev_dark_resume_add_consumer(struct device *dev) { }

static inline void dev_dark_resume_remove_consumer(struct device *dev) { }

static inline bool pm_dark_resume_active(void)
{
	return false;
}

static inline bool dev_dark_resume_active(struct device *dev)
{
	return false;
}

#endif /* !CONFIG_PM_SLEEP */
#endif /* _LINUX_PM_DARK_RESUME_H */
