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

#ifndef _SECURITY_CHROMIUMOS_UTILS_H
#define _SECURITY_CHROMIUMOS_UTILS_H

#include <linux/sched.h>
#include <linux/mm.h>

char *printable(char *source, size_t max_len);
char *printable_cmdline(struct task_struct *task);

#endif /* _SECURITY_CHROMIUMOS_UTILS_H */
