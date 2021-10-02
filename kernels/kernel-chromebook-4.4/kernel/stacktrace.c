/*
 * kernel/stacktrace.c
 *
 * Stack trace management functions
 *
 *  Copyright (C) 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/kallsyms.h>
#include <linux/stacktrace.h>

void print_stack_trace(struct stack_trace *trace, int spaces)
{
	int i;

	if (WARN_ON(!trace->entries))
		return;

	for (i = 0; i < trace->nr_entries; i++) {
		printk("%*c", 1 + spaces, ' ');
		print_ip_sym(trace->entries[i]);
	}
}
EXPORT_SYMBOL_GPL(print_stack_trace);

int snprint_stack_trace(char *buf, size_t size,
			struct stack_trace *trace, int spaces)
{
	int i;
	unsigned long ip;
	int generated;
	int total = 0;

	if (WARN_ON(!trace->entries))
		return 0;

	for (i = 0; i < trace->nr_entries; i++) {
		ip = trace->entries[i];
		generated = snprintf(buf, size, "%*c[<%p>] %pS\n",
				1 + spaces, ' ', (void *) ip, (void *) ip);

		total += generated;

		/* Assume that generated isn't a negative number */
		if (generated >= size) {
			buf += size;
			size = 0;
		} else {
			buf += generated;
			size -= generated;
		}
	}

	return total;
}
EXPORT_SYMBOL_GPL(snprint_stack_trace);

/*
 * Architectures that do not implement save_stack_trace_tsk or
 * save_stack_trace_regs get this weak alias and a once-per-bootup warning
 * (whenever this facility is utilized - for example by procfs):
 */
__weak void
save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
	WARN_ONCE(1, KERN_INFO "save_stack_trace_tsk() not implemented yet.\n");
}

__weak void
save_stack_trace_regs(struct pt_regs *regs, struct stack_trace *trace)
{
	WARN_ONCE(1, KERN_INFO "save_stack_trace_regs() not implemented yet.\n");
}

/*
 * Architectures that do not implement the task stack dedup will fallback to
 * the default functionality.
 */
__weak void
clear_dup_stack_traces(void)
{
}

__weak unsigned int
save_dup_stack_trace(struct task_struct *tsk)
{
	return 0;
}

__weak void
show_stack_dedup(struct task_struct *task, unsigned long *sp,
			pid_t dup_stack_pid)
{
	show_stack(task, sp);
}
