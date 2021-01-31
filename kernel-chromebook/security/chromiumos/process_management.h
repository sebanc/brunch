/*
 * Linux Security Module for Chromium OS
 *
 * Copyright 2018 Google LLC. All Rights Reserved
 *
 * Author:
 *      Micah Morton       <mortonm@chromium.org>
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

#ifndef _SECURITY_PROCESS_MANAGEMENT_H
#define _SECURITY_PROCESS_MANAGEMENT_H

#include <linux/types.h>

/* Function type. */
enum chromiumos_process_management_file_write_type {
	CHROMIUMOS_PROCESS_MANAGEMENT_ADD, /* Add whitelist policy. */
	CHROMIUMOS_PROCESS_MANAGEMENT_FLUSH, /* Flush whitelist policies. */
};

/*
 * Add entry to chromiumos process management policies to allow user 'parent'
 * to use user 'child' for process management.
 */
int chromiumos_add_process_management_entry(kuid_t parent, kuid_t child);

void chromiumos_flush_process_management_entries(void);

#endif /* _SECURITY_PROCESS_MANAGEMENT_H */
