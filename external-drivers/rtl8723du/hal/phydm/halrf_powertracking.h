/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__HALRF_POWER_TRACKING_H__
#define    __HALRF_POWER_TRACKING_H__

#define HALRF_POWRTRACKING_ALL_VER	"1.0"

bool
odm_check_power_status(
	void		*p_dm_void
);

void
halrf_update_pwr_track(
	void		*p_dm_void,
	u8		rate
);

#endif
