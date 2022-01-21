/*
 * Copyright (C) 2018 Intel Corporation
 * Copyright (c) 2020 Toradex
 */

#include <linux/hrtimer.h>

/*
 * Before the commit 8b618628b2b the ktime_divns was non-exported function.
 * Add the simple implementation of ktime_divns() to use in
 * ktime_get_boottime_seconds().
 */
#if LINUX_VERSION_IS_LESS(3,19,0)

#include <linux/ktime.h>

#if BITS_PER_LONG < 64
/*
 * Divide a ktime value by a nanosecond value
 */
u64 ktime_divns(const ktime_t kt, s64 div)
{
	int sft = 0;
	s64 dclc;
	u64 tmp;

	dclc = ktime_to_ns(kt);
	tmp = dclc < 0 ? -dclc : dclc;

	/* Make sure the divisor is less than 2^32: */
	while (div >> 32) {
		sft++;
		div >>= 1;
	}
	tmp >>= sft;
	do_div(tmp, (unsigned long) div);
	return dclc < 0 ? -tmp : tmp;
}
#endif /* BITS_PER_LONG >= 64 */
#endif /* < 3.19 */

time64_t ktime_get_boottime_seconds(void)
{
	return ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
}
EXPORT_SYMBOL_GPL(ktime_get_boottime_seconds);

