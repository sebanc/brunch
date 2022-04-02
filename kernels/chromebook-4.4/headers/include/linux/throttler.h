/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_THROTTLER_H__
#define __LINUX_THROTTLER_H__

struct throttler;

extern struct throttler *throttler_setup(struct device *dev);
extern void throttler_teardown(struct throttler *thr);
extern void throttler_set_level(struct throttler *thr, unsigned int level);

#ifdef CONFIG_THROTTLER_DEBUG
#define thr_dbg(thr, fmt, ...) dev_info(thr->dev, fmt, ##__VA_ARGS__)
#else
#define thr_dbg(thr, fmt, ...) dev_dbg(thr->dev, fmt, ##__VA_ARGS__)
#endif

#define thr_info(thr, fmt, ...) dev_info(thr->dev, fmt, ##__VA_ARGS__)
#define thr_warn(thr, fmt, ...) dev_warn(thr->dev, fmt, ##__VA_ARGS__)
#define thr_err(thr, fmt, ...) dev_warn(thr->dev, fmt, ##__VA_ARGS__)

#endif /* __LINUX_THROTTLER_H__ */
