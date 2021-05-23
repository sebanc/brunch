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

#ifndef __HALRF_8814A_H__
#define __HALRF_8814A_H__

/*--------------------------Define Parameters-------------------------------*/
#define	IQK_DELAY_TIME_8814A		10		/* ms */
#define	index_mapping_NUM_8814A	15
#define	AVG_THERMAL_NUM_8814A	4
#define	RF_T_METER_8814A		0x42
#define	MAX_PATH_NUM_8814A	4

#include "../halphyrf_ap.h"


void configure_txpower_track_8814a(
	struct txpwrtrack_cfg	*config
);

void
get_delta_swing_table_8814a(
	struct dm_struct			*dm,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
);

void
get_delta_swing_table_8814a_path_cd(
	struct dm_struct			*dm,
	u8 **temperature_up_c,
	u8 **temperature_down_c,
	u8 **temperature_up_d,
	u8 **temperature_down_d
);

void
configure_txpower_track_8814a(
	struct txpwrtrack_cfg	*config
);


void
odm_tx_pwr_track_set_pwr8814a(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
);


u8
check_rf_gain_offset(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path
);


/*
 * LC calibrate
 *   */
void
phy_lc_calibrate_8814a(
	void		*dm_void
);

void
_phy_lc_calibrate_8814a(
	struct dm_struct		*dm,
	boolean		is2T
);


/*
 * AP calibrate
 *   */
void
phy_ap_calibrate_8814a(
	struct dm_struct		*dm,
	s8		delta);


#if 0 /* FOR_8814_IQK */
void
_phy_save_adda_registers(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	u32		*adda_reg,
	u32		*adda_backup,
	u32		register_num
);

void
_phy_path_adda_on(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	u32		*adda_reg,
	boolean		is_path_a_on,
	boolean		is2T
);

void
_phy_mac_setting_calibration(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	u32		*mac_reg,
	u32		*mac_backup
);



void
_phy_path_a_stand_by(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm
#else
	void	*adapter
#endif
);

#endif


#endif	/*#ifndef __HALRF_8814A_H__*/
