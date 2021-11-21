/*
 * Utilities for the Linux Security Module for Chromium OS
 * (Since CONFIG_AUDIT is disabled for Chrome OS, we must repurpose
 * a bunch of the audit string handling logic here instead.)
 *
 * Copyright 2012 Google Inc. All Rights Reserved
 *
 * Author:
 *      Kees Cook       <keescook@chromium.org>
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

#include <linux/module.h>
#include <linux/sched/mm.h>
#include <linux/security.h>

#include "utils.h"

/* Disallow double-quote and control characters other than space. */
static int contains_unprintable(const char *source, size_t len)
{
	const unsigned char *p;
	for (p = source; p < (const unsigned char *)source + len; p++) {
		if (*p == '"' || *p < 0x20 || *p > 0x7e)
			return 1;
	}
	return 0;
}

static char *hex_printable(const char *source, size_t len)
{
	size_t i;
	char *dest, *ptr;
	const char *hex = "0123456789ABCDEF";

	/* Need to double the length of the string, plus a NULL. */
	if (len > (INT_MAX - 1) / 2)
		return NULL;
	dest = kmalloc((len * 2) + 1, GFP_KERNEL);
	if (!dest)
		return NULL;

	for (ptr = dest, i = 0; i < len; i++) {
		*ptr++ = hex[(source[i] & 0xF0) >> 4];
		*ptr++ = hex[source[i] & 0x0F];
	}
	*ptr = '\0';

	return dest;
}

static char *quoted_printable(const char *source, size_t len)
{
	char *dest;

	/* Need to add 2 double quotes and a NULL. */
	if (len > INT_MAX - 3)
		return NULL;
	dest = kmalloc(len + 3, GFP_KERNEL);
	if (!dest)
		return NULL;

	dest[0] = '"';
	strncpy(dest + 1, source, len);
	dest[len + 1] = '"';
	dest[len + 2] = '\0';
	return dest;
}

/* Return a string that has been sanitized and is safe to log. It is either
 * in double-quotes, or is a series of hex digits.
 */
char *printable(char *source, size_t max_len)
{
	size_t len;

	if (!source)
		return NULL;

	len = strnlen(source, max_len);
	if (contains_unprintable(source, len))
		return hex_printable(source, len);
	else
		return quoted_printable(source, len);
}

/* Repurposed from fs/proc/base.c, with NULL-replacement for saner printing.
 * Allocates the buffer itself.
 */
char *printable_cmdline(struct task_struct *task)
{
	char *buffer = NULL, *sanitized;
	int res, i;
	unsigned int len;
	struct mm_struct *mm;

	mm = get_task_mm(task);
	if (!mm)
		goto out;

	if (!mm->arg_end)
		goto out_mm;	/* Shh! No looking before we're done */

	buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buffer)
		goto out_mm;

	len = mm->arg_end - mm->arg_start;

	if (len > PAGE_SIZE)
		len = PAGE_SIZE;

	res = access_process_vm(task, mm->arg_start, buffer, len, 0);

	/* Space-fill NULLs. */
	if (res > 1)
		for (i = 0; i < res - 2; ++i)
			if (buffer[i] == '\0')
				buffer[i] = ' ';

	/* If the NULL at the end of args has been overwritten, then
	 * assume application is using setproctitle(3).
	 */
	if (res > 0 && buffer[res-1] != '\0' && len < PAGE_SIZE) {
		len = strnlen(buffer, res);
		if (len < res) {
			res = len;
		} else {
			len = mm->env_end - mm->env_start;
			if (len > PAGE_SIZE - res)
				len = PAGE_SIZE - res;
			res += access_process_vm(task, mm->env_start,
						 buffer+res, len, 0);
		}
	}

	/* Make sure the buffer is always NULL-terminated. */
	buffer[PAGE_SIZE-1] = 0;

	/* Make sure result is printable. */
	sanitized = printable(buffer, res);
	kfree(buffer);
	buffer = sanitized;

out_mm:
	mmput(mm);
out:
	return buffer;
}
