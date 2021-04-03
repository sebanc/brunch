/*
 * Alternate Syscall Table Infrastructure
 *
 * Copyright 2014 Google Inc. All Rights Reserved
 *
 * Authors:
 *      Kees Cook   <keescook@chromium.org>
 *      Will Drewry <wad@chromium.org>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/alt-syscall.h>

static LIST_HEAD(alt_sys_call_tables);
static DEFINE_SPINLOCK(alt_sys_call_tables_lock);

/* XXX: there is no "unregister" yet. */
int register_alt_sys_call_table(struct alt_sys_call_table *entry)
{
	if (!entry)
		return -EINVAL;

	spin_lock(&alt_sys_call_tables_lock);
	list_add(&entry->node, &alt_sys_call_tables);
	spin_unlock(&alt_sys_call_tables_lock);

	pr_info("table '%s' available.\n", entry->name);

	return 0;
}
EXPORT_SYMBOL_GPL(register_alt_sys_call_table);

int set_alt_sys_call_table(char * __user uname)
{
	char name[ALT_SYS_CALL_NAME_MAX + 1] = { };
	struct alt_sys_call_table *entry;

	if (copy_from_user(name, uname, ALT_SYS_CALL_NAME_MAX))
		return -EFAULT;

	spin_lock(&alt_sys_call_tables_lock);
	list_for_each_entry(entry, &alt_sys_call_tables, node) {
		if (!strcmp(entry->name, name)) {
			if (arch_set_sys_call_table(entry))
				continue;
			spin_unlock(&alt_sys_call_tables_lock);
			return 0;
		}
	}
	spin_unlock(&alt_sys_call_tables_lock);

	return -ENOENT;
}
EXPORT_SYMBOL_GPL(set_alt_sys_call_table);
