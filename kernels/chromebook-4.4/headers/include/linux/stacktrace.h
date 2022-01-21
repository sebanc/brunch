#ifndef __LINUX_STACKTRACE_H
#define __LINUX_STACKTRACE_H

#include <linux/types.h>

struct task_struct;
struct pt_regs;

#ifdef CONFIG_STACKTRACE
struct stack_trace {
	unsigned int nr_entries, max_entries;
	unsigned long *entries;
	int skip;	/* input argument: How many entries to skip */
};

extern void save_stack_trace(struct stack_trace *trace);
extern void save_stack_trace_regs(struct pt_regs *regs,
				  struct stack_trace *trace);
extern void save_stack_trace_tsk(struct task_struct *tsk,
				struct stack_trace *trace);

extern void print_stack_trace(struct stack_trace *trace, int spaces);
extern int snprint_stack_trace(char *buf, size_t size,
			struct stack_trace *trace, int spaces);

extern void clear_dup_stack_traces(void);
extern unsigned int save_dup_stack_trace(struct task_struct *tsk);
extern void show_stack_dedup(struct task_struct *task, unsigned long *sp,
				pid_t dup_stack_pid);

#ifdef CONFIG_USER_STACKTRACE_SUPPORT
extern void save_stack_trace_user(struct stack_trace *trace);
#else
# define save_stack_trace_user(trace)              do { } while (0)
#endif

#else
# define save_stack_trace(trace)			do { } while (0)
# define save_stack_trace_tsk(tsk, trace)		do { } while (0)
# define save_stack_trace_user(trace)			do { } while (0)
# define print_stack_trace(trace, spaces)		do { } while (0)
# define snprint_stack_trace(buf, size, trace, spaces)	do { } while (0)
# define clear_dup_stack_traces()			do { } while (0)
# define save_dup_stack_trace(tsk)			(0)
# define show_stack_dedup(task, sp, dup_stack_pid)	do { } while (0)
#endif

#endif
