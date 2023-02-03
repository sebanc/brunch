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

static inline u64
backport_hrtimer_forward_now(struct hrtimer *timer, s64 interval)
{
	ktime_t _interval = { .tv64 = interval };

	return hrtimer_forward_now(timer, _interval);
}
#define hrtimer_forward_now LINUX_BACKPORT(hrtimer_forward_now)

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

#if LINUX_VERSION_IS_LESS(5,9,0)

static inline void
tasklet_setup(struct tasklet_struct *t,
	      void (*callback)(struct tasklet_struct *))
{
	void (*cb)(unsigned long data) = (void *)callback;

	tasklet_init(t, cb, (unsigned long)t);
}

#define from_tasklet(var, callback_tasklet, tasklet_fieldname) \
	container_of(callback_tasklet, typeof(*var), tasklet_fieldname)

#endif

#if LINUX_VERSION_IS_LESS(5,13,0)

#define tasklet_unlock_spin_wait LINUX_BACKPORT(tasklet_unlock_spin_wait)
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT_RT)
void tasklet_unlock_spin_wait(struct tasklet_struct *t);

#else
static inline void tasklet_unlock_spin_wait(struct tasklet_struct *t) { }
#endif

/*
 * Do not use in new code. Disabling tasklets from atomic contexts is
 * error prone and should be avoided.
 */
#define tasklet_disable_in_atomic LINUX_BACKPORT(tasklet_disable_in_atomic)
static inline void tasklet_disable_in_atomic(struct tasklet_struct *t)
{
	tasklet_disable_nosync(t);
	tasklet_unlock_spin_wait(t);
	smp_mb();
}
#endif

#endif /* _BP_LINUX_INTERRUPT_H */
