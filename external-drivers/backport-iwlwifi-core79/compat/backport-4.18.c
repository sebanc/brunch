/*
 * Copyright (C) 2018 Intel Corporation
 * Copyright (c) 2020 Toradex
 */

#include <linux/hrtimer.h>

time64_t ktime_get_boottime_seconds(void)
{
	return ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
}
EXPORT_SYMBOL_GPL(ktime_get_boottime_seconds);

