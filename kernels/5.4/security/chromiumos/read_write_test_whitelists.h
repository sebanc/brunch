/*
 * Linux Security Module for Chromium OS
 *
 * Copyright 2018 Google LLC. All Rights Reserved
 *
 * Authors:
 *      Micah Morton <mortonm@chromium.org>
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

#ifndef READ_WRITE_TESTS_WHITELISTS_H
#define READ_WRITE_TESTS_WHITELISTS_H

/*
 * NOTE: the purpose of this header is only to pull out the definition of this
 * array from alt-syscall.c for the purposes of readability. It should not be
 * included in other .c files.
 */

#include "alt-syscall.h"

static struct syscall_whitelist_entry read_write_test_whitelist[] = {
	SYSCALL_ENTRY(exit),
	SYSCALL_ENTRY(openat),
	SYSCALL_ENTRY(close),
	SYSCALL_ENTRY(read),
	SYSCALL_ENTRY(write),
	SYSCALL_ENTRY_ALT(prctl, alt_sys_prctl),

	/* open(2) is deprecated and not wired up on ARM64. */
#ifndef CONFIG_ARM64
	SYSCALL_ENTRY(open),
#endif
}; /* end read_write_test_whitelist */

#ifdef CONFIG_COMPAT
static struct syscall_whitelist_entry read_write_test_compat_whitelist[] = {
	COMPAT_SYSCALL_ENTRY(exit),
	COMPAT_SYSCALL_ENTRY(open),
	COMPAT_SYSCALL_ENTRY(openat),
	COMPAT_SYSCALL_ENTRY(close),
	COMPAT_SYSCALL_ENTRY(read),
	COMPAT_SYSCALL_ENTRY(write),
	COMPAT_SYSCALL_ENTRY_ALT(prctl, alt_sys_prctl),
}; /* end read_write_test_compat_whitelist */
#endif /* CONFIG_COMPAT */

#endif /* READ_WRITE_TESTS_WHITELISTS_H */
