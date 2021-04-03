#ifndef _ASM_X86_SWITCH_TO_H
#define _ASM_X86_SWITCH_TO_H

#include <asm/nospec-branch.h>

struct task_struct; /* one of the stranger aspects of C forward declarations */

struct task_struct *__switch_to_asm(struct task_struct *prev,
				    struct task_struct *next);

__visible struct task_struct *__switch_to(struct task_struct *prev,
					   struct task_struct *next);

#ifdef CONFIG_CC_STACKPROTECTOR
#define __switch_canary							\
	"movl %P[task_canary](%[next]), %%ebx\n\t"			\
	"movl %%ebx, "__percpu_arg([stack_canary])"\n\t"
#define __switch_canary_oparam						\
	, [stack_canary] "=m" (stack_canary.canary)
#define __switch_canary_iparam						\
	, [task_canary] "i" (offsetof(struct task_struct, stack_canary))
#else	/* CC_STACKPROTECTOR */
#define __switch_canary
#define __switch_canary_oparam
#define __switch_canary_iparam
#endif	/* CC_STACKPROTECTOR */

/* data that is pointed to by thread.sp */
struct inactive_task_frame {
	unsigned long flags;
#ifdef CONFIG_X86_64
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
#else
	unsigned long si;
	unsigned long di;
#endif
	unsigned long bx;
	unsigned long bp;
	unsigned long ret_addr;
};

struct fork_frame {
	struct inactive_task_frame frame;
	struct pt_regs regs;
};

#ifdef CONFIG_RETPOLINE
	/*
	 * When switching from a shallower to a deeper call stack
	 * the RSB may either underflow or use entries populated
	 * with userspace addresses. On CPUs where those concerns
	 * exist, overwrite the RSB with entries which capture
	 * speculative execution to prevent attack.
	 */
#define __retpoline_fill_return_buffer					\
	ALTERNATIVE("jmp 910f",						\
		__stringify(__FILL_RETURN_BUFFER(%%ebx, RSB_CLEAR_LOOPS, %%esp)),\
		X86_FEATURE_RSB_CTXSW)					\
	"910:\n\t"
#else
#define __retpoline_fill_return_buffer
#endif

#define switch_to(prev, next, last)					\
do {									\
	((last) = __switch_to_asm((prev), (next)));			\
} while (0)

#endif /* _ASM_X86_SWITCH_TO_H */
