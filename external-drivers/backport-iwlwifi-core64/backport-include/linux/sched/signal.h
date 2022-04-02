#ifndef _BACKPORT_LINUX_SCHED_SIGNAL_H
#define _BACKPORT_LINUX_SCHED_SIGNAL_H

#if LINUX_VERSION_IS_LESS(4, 11, 0)
#include <linux/sched.h>
#else
#include_next <linux/sched/signal.h>
#endif

#endif /* _BACKPORT_LINUX_SCHED_SIGNAL_H */
