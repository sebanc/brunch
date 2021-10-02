/*
 *  Copyright (C) 2011 The Chromium OS Authors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _DRIVERS_PLATFORM_CHROMEOS_H
#define _DRIVERS_PLATFORM_CHROMEOS_H

#define MAX_VBOOT_CONTEXT_BUFFER_SIZE 64  /* Should be enough for anything. */

#ifdef CONFIG_ACPI_CHROMEOS
extern int chromeos_legacy_set_need_recovery(void);
#else
static inline int chromeos_legacy_set_need_recovery(void) { return -ENODEV; }
#endif

struct chromeos_vbc {
	/**
	 * Read vboot context to buffer
	 *
	 * @param buf		Pointer to buffer for storing vboot context
	 * @param count		Size of buffer
	 * @return	on success, the number of bytes read is returned and
	 *		on error, -err is returned.
	 */
	ssize_t (*read)(void *buf, size_t count);

	/**
	 * Write vboot context from buffer
	 *
	 * @param buf		Pointer to buffer of new vboot context content
	 * @param count		Size of buffer
	 * @return	on success, the number of bytes written is returned and
	 *		on error, -err is returned.
	 */
	ssize_t (*write)(const void *buf, size_t count);

	const char *name;
};

/**
 * Register chromeos_vbc callbacks.
 *
 * @param chromeos_vbc	Pointer to struct holding callbacks
 * @return	on success, return 0, on error, -err is returned.
 */
int chromeos_vbc_register(struct chromeos_vbc *chromeos_vbc);

#endif /* _DRIVERS_PLATFORM_CHROMEOS_H */
