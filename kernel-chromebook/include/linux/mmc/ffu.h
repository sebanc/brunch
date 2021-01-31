/*
 *  ffu.h
 *
 * Copyright 2015 SanDisk, Corp
 * Modified by Google Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program was created by SanDisk Corp
 */

#ifndef _FFU_H_
#define _FFU_H_

#include <linux/mmc/card.h>

/*
 * eMMC5.0 Field Firmware Update (FFU) opcodes
 */
#define MMC_FFU_INVOKE_OP	302

#define FFU_NAME_LEN		80  /* Firmware file name udev should find */

enum mmc_ffu_hack_type {
	MMC_OVERRIDE_FFU_ARG = 0,
	MMC_HACK_LEN,
};

struct mmc_ffu_hack {
	enum mmc_ffu_hack_type type;
	u64 value;
};

struct mmc_ffu_args {
	char name[FFU_NAME_LEN];
	u32 ack_nb;
	struct mmc_ffu_hack hack[0];
};

#ifdef CONFIG_MMC_FFU
int mmc_ffu_invoke(struct mmc_card *card, const struct mmc_ffu_args *args);
#else
static inline int mmc_ffu_invoke(struct mmc_card *card,
		const struct mmc_ffu_args *args)
{
	return -EOPNOTSUPP;
}
#endif
#endif /* _FFU_H_ */
