/*
 * AArch64 alt-syscall implementation
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/alt-syscall.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/syscalls.h>

#include <asm/syscall.h>

int arch_dup_sys_call_table(struct alt_sys_call_table *entry)
{
	int err;

	if (!entry)
		return -EINVAL;
	if (entry->table)
		return -EINVAL;
#ifdef CONFIG_COMPAT
	if (entry->compat_table)
		return -EINVAL;
#endif

	entry->size = __NR_syscalls;
	entry->table = kcalloc(entry->size, sizeof(sys_call_ptr_t), GFP_KERNEL);
	if (!entry->table) {
		err = -ENOMEM;
		goto failed;
	}
	memcpy(entry->table, sys_call_table,
	       entry->size * sizeof(sys_call_ptr_t));

#ifdef CONFIG_COMPAT
	entry->compat_size = __NR_compat_syscalls;
	entry->compat_table = kcalloc(entry->compat_size,
				      sizeof(sys_call_ptr_t), GFP_KERNEL);
	if (!entry->compat_table) {
		err = -ENOMEM;
		goto compat_failed;
	}
	memcpy(entry->compat_table, compat_sys_call_table,
	       entry->compat_size * sizeof(sys_call_ptr_t));
#endif

	return 0;

#ifdef CONFIG_COMPAT
compat_failed:
	entry->compat_size = 0;
	kfree(entry->table);
	entry->table = NULL;
#endif
failed:
	entry->size = 0;
	return err;
}
EXPORT_SYMBOL_GPL(arch_dup_sys_call_table);

int arch_set_sys_call_table(struct alt_sys_call_table *entry)
{
	if (!entry)
		return -EINVAL;

	current_thread_info()->nr_syscalls = entry->size;
	current_thread_info()->sys_call_table = entry->table;
#ifdef CONFIG_COMPAT
	current_thread_info()->compat_nr_syscalls = entry->compat_size;
	current_thread_info()->compat_sys_call_table = entry->compat_table;
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(arch_set_sys_call_table);
