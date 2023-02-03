// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/interrupt.h>

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT_RT)
/*
 * Do not use in new code. Waiting for tasklets from atomic contexts is
 * error prone and should be avoided.
 */
void tasklet_unlock_spin_wait(struct tasklet_struct *t)
{
	while (test_bit(TASKLET_STATE_RUN, &(t)->state)) {
		if (IS_ENABLED(CONFIG_PREEMPT_RT)) {
			/*
			 * Prevent a live lock when current preempted soft
			 * interrupt processing or prevents ksoftirqd from
			 * running. If the tasklet runs on a different CPU
			 * then this has no effect other than doing the BH
			 * disable/enable dance for nothing.
			 */
			local_bh_disable();
			local_bh_enable();
		} else {
			cpu_relax();
		}
	}
}
EXPORT_SYMBOL_GPL(tasklet_unlock_spin_wait);
#endif
