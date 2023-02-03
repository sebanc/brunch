#ifndef __BACKPORT_LINUX_SCHED_H
#define __BACKPORT_LINUX_SCHED_H

#include_next <linux/sched.h>
#include <linux/version.h>
#if LINUX_VERSION_IS_LESS(4,10,0)
#include <linux/kcov.h>
#endif /* LINUX_VERSION_IS_LESS(4,10,0) */

/* kcov was added in 4.6 and is included since then */
#if LINUX_VERSION_IS_LESS(4,6,0)
#include <linux/kcov.h>
#endif

#if LINUX_VERSION_IS_LESS(5,9,0)
#if LINUX_VERSION_IS_GEQ(4,11,0)
#include <uapi/linux/sched/types.h>
#endif

static inline void sched_set_fifo_low(struct task_struct *p)
{
	struct sched_param sparam = {.sched_priority = 1};

	WARN_ON_ONCE(sched_setscheduler_nocheck(p, SCHED_FIFO, &sparam) != 0);
}

#endif /* < 5.9.0 */
#endif /* __BACKPORT_LINUX_SCHED_H */
