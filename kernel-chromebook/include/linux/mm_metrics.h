/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _LINUX_MM_METRICS_H
#define _LINUX_MM_METRICS_H

#include <linux/timekeeping.h>
#include <linux/sched.h>
#include <linux/swapops.h>

struct histogram {
	struct rcu_head rcu;
	unsigned int size;
	u64 __percpu *buckets;
	u64 thresholds[0];
};

enum {
	MM_SWAP_REFAULT,
	MM_SWAP_LATENCY,
	MM_RECLAIM_LATENCY,
	NR_MM_METRICS,
};

extern struct histogram __rcu *mm_metrics_files[NR_MM_METRICS];

#ifdef CONFIG_MM_METRICS
#define mm_metrics_enabled(type)	rcu_access_pointer(mm_metrics_files[type])
#else
#define mm_metrics_enabled(type)	0
#endif

extern void mm_metrics_record(unsigned int type, u64 val, u64 count);

static inline void mm_metrics_swapout(swp_entry_t *swap)
{
	if (mm_metrics_enabled(MM_SWAP_REFAULT)) {
		u64 start = ktime_get_seconds();

		VM_BUG_ON(swp_type(*swap) >= MAX_SWAPFILES);
		VM_BUG_ON(!swp_offset(*swap));

		swap->val &= ~GENMASK_ULL(SWP_TM_OFF_BITS - 1, SWP_OFFSET_BITS);
		if (start < BIT_ULL(SWP_TIME_BITS))
			swap->val |= start << SWP_OFFSET_BITS;
	}
}

static inline void mm_metrics_swapin(swp_entry_t swap)
{
	if (mm_metrics_enabled(MM_SWAP_REFAULT)) {
		u64 start = _swp_offset(swap) >> SWP_OFFSET_BITS;

		VM_BUG_ON(swp_type(swap) >= MAX_SWAPFILES);
		VM_BUG_ON(!swp_offset(swap));

		if (start)
			mm_metrics_record(MM_SWAP_REFAULT,
					  ktime_get_seconds() - start, 1);
	}
}

static inline u64 mm_metrics_swapin_start(void)
{
	return mm_metrics_enabled(MM_SWAP_LATENCY) ? sched_clock() : 0;
}

static inline void mm_metrics_swapin_end(u64 start)
{
	if (mm_metrics_enabled(MM_SWAP_LATENCY) && start)
		mm_metrics_record(MM_SWAP_LATENCY, sched_clock() - start, 1);
}

static inline u64 mm_metrics_reclaim_start(void)
{
	return mm_metrics_enabled(MM_RECLAIM_LATENCY) ? sched_clock() : 0;
}

static inline void mm_metrics_reclaim_end(u64 start)
{
	if (mm_metrics_enabled(MM_RECLAIM_LATENCY) && start)
		mm_metrics_record(MM_RECLAIM_LATENCY, sched_clock() - start, 1);
}

#endif /* _LINUX_MM_METRICS_H */
