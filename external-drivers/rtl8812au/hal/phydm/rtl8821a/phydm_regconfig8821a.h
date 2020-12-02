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
#ifndef __INC_ODM_REGCONFIG_H_8821A
#define __INC_ODM_REGCONFIG_H_8821A

#if (RTL8821A_SUPPORT == 1)

void
odm_config_rf_reg_8821a(
	struct dm_struct				*dm,
	u32					addr,
	u32					data,
	enum rf_path     RF_PATH,
	u32				    reg_addr
);

void
odm_config_rf_radio_a_8821a(
	struct dm_struct				*dm,
	u32					addr,
	u32					data
);

void
odm_config_rf_radio_b_8821a(
	struct dm_struct				*dm,
	u32					addr,
	u32					data
);

void
odm_config_mac_8821a(
	struct dm_struct	*dm,
	u32		addr,
	u8		data
);

void
odm_config_bb_agc_8821a(
	struct dm_struct	*dm,
	u32		addr,
	u32		bitmask,
	u32		data
);

void
odm_config_bb_phy_reg_pg_8821a(
	struct dm_struct	*dm,
	u32		band,
	u32		rf_path,
	u32		tx_num,
	u32		addr,
	u32		bitmask,
	u32		data
);

void
odm_config_bb_phy_8821a(
	struct dm_struct	*dm,
	u32		addr,
	u32		bitmask,
	u32		data
);

void
odm_config_bb_txpwr_lmt_8821a(
	struct dm_struct	*dm,
	u8		*regulation,
	u8		*band,
	u8		*bandwidth,
	u8		*rate_section,
	u8		*rf_path,
	u8	*channel,
	u8		*power_limit
);

#endif
#endif /* end of SUPPORT */
