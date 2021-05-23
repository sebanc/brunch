#ifndef _ALT_SYSCALL_H
#define _ALT_SYSCALL_H

#include <linux/errno.h>

#ifdef CONFIG_ALT_SYSCALL

#include <linux/list.h>
#include <asm/syscall.h>

#define ALT_SYS_CALL_NAME_MAX	32

struct alt_sys_call_table {
	char name[ALT_SYS_CALL_NAME_MAX + 1];
	sys_call_ptr_t *table;
	int size;
#if defined(CONFIG_IA32_EMULATION) || \
    (defined(CONFIG_ARM64) && defined(CONFIG_COMPAT))
	sys_call_ptr_t *compat_table;
	int compat_size;
#endif
	struct list_head node;
};

/*
 * arch_dup_sys_call_table should return the default syscall table, not
 * the current syscall table, since we want to explicitly not allow
 * syscall table composition. A selected syscall table should be treated
 * as a single execution personality.
 */

int arch_dup_sys_call_table(struct alt_sys_call_table *table);
int arch_set_sys_call_table(struct alt_sys_call_table *table);

int register_alt_sys_call_table(struct alt_sys_call_table *table);
int set_alt_sys_call_table(char __user *name);

#else

struct alt_sys_call_table;

static inline int arch_dup_sys_call_table(struct alt_sys_call_table *table)
{
	return -ENOSYS;
}
static inline int arch_set_sys_call_table(struct alt_sys_call_table *table)
{
	return -ENOSYS;
}
static inline int register_alt_sys_call_table(struct alt_sys_call_table *table)
{
	return -ENOSYS;
}
static inline int set_alt_sys_call_table(char __user *name)
{
	return -ENOSYS;
}
#endif

#endif /* _ALT_SYSCALL_H */
