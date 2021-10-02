#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/syscalls.h>
#include <linux/alt-syscall.h>

#include <asm/syscall.h>
#include <asm/syscalls.h>

int arch_dup_sys_call_table(struct alt_sys_call_table *entry)
{
	if (!entry)
		return -EINVAL;
	/* Table already allocated. */
	if (entry->table)
		return -EINVAL;
#ifdef CONFIG_IA32_EMULATION
	if (entry->compat_table)
		return -EINVAL;
#endif
	entry->size = NR_syscalls;
	entry->table = kcalloc(entry->size, sizeof(sys_call_ptr_t),
			       GFP_KERNEL);
	if (!entry->table)
		goto failed;

	memcpy(entry->table, sys_call_table,
	       entry->size * sizeof(sys_call_ptr_t));

#ifdef CONFIG_IA32_EMULATION
	entry->compat_size = IA32_NR_syscalls;
	entry->compat_table = kcalloc(entry->compat_size,
				      sizeof(sys_call_ptr_t), GFP_KERNEL);
	if (!entry->compat_table)
		goto failed;
	memcpy(entry->compat_table, ia32_sys_call_table,
	       entry->compat_size * sizeof(sys_call_ptr_t));
#endif

	return 0;

failed:
	entry->size = 0;
	kfree(entry->table);
	entry->table = NULL;
#ifdef CONFIG_IA32_EMULATION
	entry->compat_size = 0;
#endif
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(arch_dup_sys_call_table);

/* Operates on "current", which isn't racey, since it's _in_ a syscall. */
int arch_set_sys_call_table(struct alt_sys_call_table *entry)
{
	if (!entry)
		return -EINVAL;

	current_thread_info()->nr_syscalls = entry->size;
	current_thread_info()->sys_call_table = entry->table;
#ifdef CONFIG_IA32_EMULATION
	current_thread_info()->ia32_nr_syscalls = entry->compat_size;
	current_thread_info()->ia32_sys_call_table = entry->compat_table;
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(arch_set_sys_call_table);
