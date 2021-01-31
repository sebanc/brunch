/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifndef	__ODM_RTL8821A_H__
#define __ODM_RTL8821A_H__

s8 phydm_cck_rssi_8821a(struct dm_struct *dm, u16 lna_idx, u8 vga_idx);

void
phydm_set_ext_band_switch_8821A(
	void		*dm_void,
	u32		band
);

void
odm_hw_setting_8821a(
	struct dm_struct		*dm
);

#endif
