// SPDX-License-Identifier: GPL-2.0

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/nmi.h>
#include <linux/percpu-defs.h>

static DEFINE_PER_CPU(bool, watchdog_touch);
static DEFINE_PER_CPU(bool, hard_watchdog_warn);
static cpumask_t __read_mostly watchdog_cpus;

int __init watchdog_nmi_probe(void)
{
	return 0;
}

notrace void buddy_cpu_touch_watchdog(void)
{
	/*
	 * Using __raw here because some code paths have
	 * preemption enabled.  If preemption is enabled
	 * then interrupts should be enabled too, in which
	 * case we shouldn't have to worry about the watchdog
	 * going off.
	 */
	raw_cpu_write(watchdog_touch, true);
}
EXPORT_SYMBOL_GPL(buddy_cpu_touch_watchdog);

static unsigned int watchdog_next_cpu(unsigned int cpu)
{
	cpumask_t cpus = watchdog_cpus;
	unsigned int next_cpu;

	next_cpu = cpumask_next(cpu, &cpus);
	if (next_cpu >= nr_cpu_ids)
		next_cpu = cpumask_first(&cpus);

	if (next_cpu == cpu)
		return nr_cpu_ids;

	return next_cpu;
}

int watchdog_nmi_enable(unsigned int cpu)
{
	/*
	 * The new cpu will be marked online before the first hrtimer interrupt
	 * runs on it.  If another cpu tests for a hardlockup on the new cpu
	 * before it has run its first hrtimer, it will get a false positive.
	 * Touch the watchdog on the new cpu to delay the first check for at
	 * least 3 sampling periods to guarantee one hrtimer has run on the new
	 * cpu.
	 */
	per_cpu(watchdog_touch, cpu) = true;
	smp_wmb();
	cpumask_set_cpu(cpu, &watchdog_cpus);
	return 0;
}

void watchdog_nmi_disable(unsigned int cpu)
{
	unsigned int next_cpu = watchdog_next_cpu(cpu);

	/*
	 * Offlining this cpu will cause the cpu before this one to start
	 * checking the one after this one.  If this cpu just finished checking
	 * the next cpu and updating hrtimer_interrupts_saved, and then the
	 * previous cpu checks it within one sample period, it will trigger a
	 * false positive.  Touch the watchdog on the next cpu to prevent it.
	 */
	if (next_cpu < nr_cpu_ids)
		per_cpu(watchdog_touch, next_cpu) = true;
	smp_wmb();
	cpumask_clear_cpu(cpu, &watchdog_cpus);
}

static int is_hardlockup_buddy_cpu(unsigned int cpu)
{
	unsigned long hrint = per_cpu(hrtimer_interrupts, cpu);

	if (per_cpu(hrtimer_interrupts_saved, cpu) == hrint)
		return 1;

	per_cpu(hrtimer_interrupts_saved, cpu) = hrint;
	return 0;
}

void watchdog_check_hardlockup(void)
{
	unsigned int next_cpu;

	/*
	 * Test for hardlockups every 3 samples.  The sample period is
	 *  watchdog_thresh * 2 / 5, so 3 samples gets us back to slightly over
	 *  watchdog_thresh (over by 20%).
	 */
	if (__this_cpu_read(hrtimer_interrupts) % 3 != 0)
		return;

	/* check for a hardlockup on the next cpu */
	next_cpu = watchdog_next_cpu(smp_processor_id());
	if (next_cpu >= nr_cpu_ids)
		return;

	smp_rmb();

	if (per_cpu(watchdog_touch, next_cpu) == true) {
		per_cpu(watchdog_touch, next_cpu) = false;
		return;
	}

	if (is_hardlockup_buddy_cpu(next_cpu)) {
		/* only warn once */
		if (per_cpu(hard_watchdog_warn, next_cpu) == true)
			return;

		if (hardlockup_panic)
			panic("Watchdog detected hard LOCKUP on cpu %u", next_cpu);
		else
			WARN(1, "Watchdog detected hard LOCKUP on cpu %u", next_cpu);

		per_cpu(hard_watchdog_warn, next_cpu) = true;
	} else {
		per_cpu(hard_watchdog_warn, next_cpu) = false;
	}
}
