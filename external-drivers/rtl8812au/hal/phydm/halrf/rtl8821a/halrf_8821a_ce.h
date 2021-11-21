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

#ifndef __HAL_PHY_RF_8821A_H__
#define __HAL_PHY_RF_8821A_H__

/*--------------------------Define Parameters-------------------------------*/

void configure_txpower_track_8821a(
	struct txpwrtrack_cfg	*config
);

void
odm_tx_pwr_track_set_pwr8821a(
	void		*dm_void,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
);

void
phy_lc_calibrate_8821a(
	void		*dm_void
);

void
get_delta_swing_table_8821a(
	void		*dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
);

void
halrf_rf_lna_setting_8821a(
	struct dm_struct	*dm,
	enum halrf_lna_set type
);

#endif	/*  #ifndef __HAL_PHY_RF_8821A_H__ */
