/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022, MediaTek Inc.
 * Copyright (c) 2022, Intel Corporation.
 */

#ifndef __T7XX_UEVENT_H__
#define __T7XX_UEVENT_H__

/* Maximum length of user events */
#define T7XX_MAX_UEVENT_LEN 64

/* T7XX Host driver uevents*/
#define T7XX_UEVENT_MODEM_WAITING_HS1		"T7XX_MODEM_WAITING_HS1"
#define T7XX_UEVENT_MODEM_WAITING_HS2		"T7XX_MODEM_WAITING_HS2"
#define T7XX_UEVENT_MODEM_READY			"T7XX_MODEM_READY"
#define T7XX_UEVENT_MODEM_STOPPED		"T7XX_MODEM_STOPPED"
#define T7XX_UEVENT_MODEM_WARM_RESET		"T7XX_MODEM_WARM_RESET"
#define T7XX_UEVENT_MODEM_COLD_RESET		"T7XX_MODEM_COLD_RESET"
#define T7XX_UEVENT_MODEM_EXCEPTION		"T7XX_MODEM_EXCEPTION"
#define T7XX_UEVENT_MODEM_BOOT_HS_FAIL		"T7XX_MODEM_BOOT_HS_FAIL"
#define T7XX_UEVENT_MODEM_FASTBOOT_DL_MODE	"T7XX_MODEM_FASTBOOT_DL_MODE"
#define T7XX_UEVENT_MODEM_FASTBOOT_DUMP_MODE	"T7XX_MODEM_FASTBOOT_DUMP_MODE"
#define T7XX_UEVENT_MRDUMP_READY		"T7XX_MRDUMP_READY"
#define T7XX_UEVENT_LKDUMP_READY		"T7XX_LKDUMP_READY"
#define T7XX_UEVENT_MRD_DISCD			"T7XX_MRDUMP_DISCARDED"
#define T7XX_UEVENT_LKD_DISCD			"T7XX_LKDUMP_DISCARDED"
#define T7XX_UEVENT_FLASHING_SUCCESS		"T7XX_FLASHING_SUCCESS"
#define T7XX_UEVENT_FLASHING_FAILURE		"T7XX_FLASHING_FAILURE"

/**
 * struct t7xx_uevent_info - Uevent information structure.
 * @dev:	Pointer to device structure
 * @uevent:	Uevent information
 * @work:	Uevent work struct
 */
struct t7xx_uevent_info {
	struct device *dev;
	char uevent[T7XX_MAX_UEVENT_LEN];
	struct work_struct work;
};

void t7xx_uevent_send(struct device *dev, char *uevent);
#endif
