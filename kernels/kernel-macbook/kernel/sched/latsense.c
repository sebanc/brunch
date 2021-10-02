// SPDX-License-Identifier: GPL-2.0
#include "sched.h"

int proc_sched_set_latency_sensitive(struct task_struct *p, int val)
{
	if (val != 0 && val != 1)
		return -EINVAL;

	p->proc_latency_sensitive = val;

	return 0;
}

int proc_sched_get_latency_sensitive(struct task_struct *p)
{
	return p->proc_latency_sensitive;
}
