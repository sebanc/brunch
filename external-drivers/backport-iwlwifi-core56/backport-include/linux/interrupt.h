#ifndef _BP_LINUX_INTERRUPT_H
#define _BP_LINUX_INTERRUPT_H
#include <linux/version.h>
#include_next <linux/interrupt.h>
#include <linux/ktime.h>

#if LINUX_VERSION_IS_LESS(4,10,0)

/* Forward a hrtimer so it expires after now: */
static inline u64
backport_hrtimer_forward(struct hrtimer *timer, ktime_t now, s64 interval)
{
	ktime_t _interval = { .tv64 = interval };

	return hrtimer_forward(timer, now, _interval);
}
#define hrtimer_forward LINUX_BACKPORT(hrtimer_forward)

static inline bool backport_ktime_before(const s64 cmp1, const s64 cmp2)
{
	return cmp1 < cmp2;
}
#define ktime_before LINUX_BACKPORT(ktime_before)

static inline s64 backport_ns_to_ktime(u64 ns)
{
	ktime_t _time = ns_to_ktime(ns);

	return _time.tv64;
}
#define ns_to_ktime LINUX_BACKPORT(ns_to_ktime)

static inline void backport_hrtimer_start(struct hrtimer *timer, s64 time,
					  const enum hrtimer_mode mode)
{
	ktime_t _time = { .tv64 = time };
	hrtimer_start(timer, _time, mode);
}
#define hrtimer_start LINUX_BACKPORT(hrtimer_start)
#endif

#endif /* _BP_LINUX_INTERRUPT_H */
