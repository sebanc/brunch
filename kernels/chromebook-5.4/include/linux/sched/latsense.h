/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_LATSENSE_H
#define _LINUX_SCHED_LATSENSE_H

extern int proc_sched_set_latency_sensitive(struct task_struct *p, int val);
extern int proc_sched_get_latency_sensitive(struct task_struct *p);

#endif /* _LINUX_SCHED_LATSENSE_H */
