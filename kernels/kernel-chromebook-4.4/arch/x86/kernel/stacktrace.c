/*
 * Stack trace management functions
 *
 *  Copyright (C) 2006-2009 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/jhash.h>
#include <asm/stacktrace.h>

static int save_stack_stack(void *data, char *name)
{
	return 0;
}

static void
__save_stack_address(void *data, unsigned long addr, bool reliable, bool nosched)
{
	struct stack_trace *trace = data;
#ifdef CONFIG_FRAME_POINTER
	if (!reliable)
		return;
#endif
	if (nosched && in_sched_functions(addr))
		return;
	if (trace->skip > 0) {
		trace->skip--;
		return;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = addr;
}

static void save_stack_address(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable, false);
}

static void
save_stack_address_nosched(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable, true);
}

static const struct stacktrace_ops save_stack_ops = {
	.stack		= save_stack_stack,
	.address	= save_stack_address,
	.walk_stack	= print_context_stack,
};

static const struct stacktrace_ops save_stack_ops_nosched = {
	.stack		= save_stack_stack,
	.address	= save_stack_address_nosched,
	.walk_stack	= print_context_stack,
};

/*
 * Save stack-backtrace addresses into a stack_trace buffer.
 */
void save_stack_trace(struct stack_trace *trace)
{
	dump_trace(current, NULL, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace);

void save_stack_trace_regs(struct pt_regs *regs, struct stack_trace *trace)
{
	dump_trace(current, regs, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

void save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
	dump_trace(tsk, NULL, NULL, 0, &save_stack_ops_nosched, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace_tsk);

/*
 * The implementation of stack trace dedup.
 *
 * It tries to reduce the duplication of task stack trace in the dump by hashing
 * the stack trace. Each time if an identical trace is found in the stack, we
 * dump only the pid of previous task. So it is easy to back track to the full
 * stack with the pid.
 *
 * Note this chould be moved out of x86-specific code for all architectures
 * use.
 */

/*
 * DEDUP_STACK_HASH: pre-allocated buffer size of the hashtable.
 * DEDUP_STACK_ENTRIES: number of task stack entries in hashtable.
 * DEDUP_HASH_MAX_ITERATIONS: in hashtable lookup, retry serveral entries if
 * there is a collision.
 */
#define DEDUP_STACK_HASH 32768
#define DEDUP_STACK_ENTRIES (DEDUP_STACK_HASH/sizeof(struct task_stack))
#define DEDUP_HASH_MAX_ITERATIONS 10

/*
 * The data structure of each hashtable entry
 */
struct task_stack {
	/* the pid of the task of the stack trace */
	pid_t pid;

	/* the length of the stack entries */
	int len;

	/* the hash value of the stack trace*/
	unsigned long hash;
};

static struct task_stack stack_hash_table[DEDUP_STACK_ENTRIES];
static struct task_stack cur_stack;
static __cacheline_aligned_in_smp DEFINE_SPINLOCK(stack_hash_lock);

/*
 * The stack hashtable uses linear probing to resolve collisions.
 * We consider two stacks to be the same if their hash values and lengths
 * are equal.
 */
static unsigned int stack_trace_lookup(void)
{
	int j;
	int index;
	unsigned int ret = 0;
	struct task_stack *stack;

	index = cur_stack.hash % DEDUP_STACK_ENTRIES;

	for (j = 0; j < DEDUP_HASH_MAX_ITERATIONS; j++) {
		stack = stack_hash_table + (index + j) % DEDUP_STACK_ENTRIES;
		if (stack->hash == 0) {
			*stack = cur_stack;
			ret = 0;
			break;
		} else {
			if (stack->hash == cur_stack.hash &&
			    stack->len == cur_stack.len) {
				ret = stack->pid;
				break;
			}
		}
	}
	if (j == DEDUP_HASH_MAX_ITERATIONS)
		stack_hash_table[index] = cur_stack;

	memset(&cur_stack, 0, sizeof(cur_stack));

	return ret;
}

static int save_dup_stack_stack(void *data, char *name)
{
	return 0;
}

static void save_dup_stack_address(void *data, unsigned long addr, int reliable)
{
	/*
	 * To improve de-duplication, we'll only record reliable entries
	 * in the stack trace.
	 */
	if (!reliable)
		return;
	cur_stack.hash = jhash(&addr, sizeof(addr), cur_stack.hash);
	cur_stack.len++;
}

static const struct stacktrace_ops save_dup_stack_ops = {
	.stack = save_dup_stack_stack,
	.address = save_dup_stack_address,
	.walk_stack = print_context_stack,
};

/*
 * Clear previously saved stack traces to ensure that later printed stacks do
 * not reference previously printed stacks.
 */
void clear_dup_stack_traces(void)
{
	memset(stack_hash_table, 0, sizeof(stack_hash_table));
}

unsigned int save_dup_stack_trace(struct task_struct *tsk)
{
	unsigned int ret = 0;
	unsigned int dummy = 0;

	spin_lock(&stack_hash_lock);
	dump_trace(tsk, NULL, NULL, 0, &save_dup_stack_ops, &dummy);
	cur_stack.pid = tsk->pid;
	ret = stack_trace_lookup();
	spin_unlock(&stack_hash_lock);

	return ret;
}

/*
 * Similar to show_stack except accepting the dup_stack_pid parameter.
 * The parameter indicates whether or not the caller side tries to do
 * a stack dedup, and the non-zero value indicates the pid of the
 * task with the same stack trace.
 */
void show_stack_dedup(struct task_struct *task, unsigned long *sp,
			pid_t dup_stack_pid)
{
	show_stack_log_lvl(task, NULL, sp, 0, "", dup_stack_pid);
}

/* Userspace stacktrace - based on kernel/trace/trace_sysprof.c */

struct stack_frame_user {
	const void __user	*next_fp;
	unsigned long		ret_addr;
};

static int
copy_stack_frame(const void __user *fp, struct stack_frame_user *frame)
{
	int ret;

	if (!access_ok(VERIFY_READ, fp, sizeof(*frame)))
		return 0;

	ret = 1;
	pagefault_disable();
	if (__copy_from_user_inatomic(frame, fp, sizeof(*frame)))
		ret = 0;
	pagefault_enable();

	return ret;
}

static inline void __save_stack_trace_user(struct stack_trace *trace)
{
	const struct pt_regs *regs = task_pt_regs(current);
	const void __user *fp = (const void __user *)regs->bp;

	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = regs->ip;

	while (trace->nr_entries < trace->max_entries) {
		struct stack_frame_user frame;

		frame.next_fp = NULL;
		frame.ret_addr = 0;
		if (!copy_stack_frame(fp, &frame))
			break;
		if ((unsigned long)fp < regs->sp)
			break;
		if (frame.ret_addr) {
			trace->entries[trace->nr_entries++] =
				frame.ret_addr;
		}
		if (fp == frame.next_fp)
			break;
		fp = frame.next_fp;
	}
}

void save_stack_trace_user(struct stack_trace *trace)
{
	/*
	 * Trace user stack if we are not a kernel thread
	 */
	if (current->mm) {
		__save_stack_trace_user(trace);
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

