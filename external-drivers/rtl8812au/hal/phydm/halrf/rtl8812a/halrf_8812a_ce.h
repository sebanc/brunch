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

#ifndef __HALRF_8812A_H__
#define __HALRF_8812A_H__

/*--------------------------Define Parameters-------------------------------*/
#define IQK_DELAY_TIME_8812A 10 /* ms */
#define IQK_DEFERRED_TIME_8812A 4 /* sec */
#define index_mapping_NUM_8812A 15
#define AVG_THERMAL_NUM_8812A 4
#define RF_T_METER_8812A 0x42

void configure_txpower_track_8812a(struct txpwrtrack_cfg *config);

void get_delta_swing_table_8812a(void *dm_void, u8 **temperature_up_a,
				 u8 **temperature_down_a, u8 **temperature_up_b,
				 u8 **temperature_down_b);

void do_iqk_8812a(void *dm_void, u8 delta_thermal_index, u8 thermal_value,
		  u8 threshold);

void odm_tx_pwr_track_set_pwr8812a(void *dm_void, enum pwrtrack_method method,
				   u8 rf_path, u8 channel_mapped_index);

/* 1 7.	IQK */

void phy_iq_calibrate_8812a(void *dm_void, boolean is_recovery);

/*
 * LC calibrate
 *   */
void phy_lc_calibrate_8812a(void *dm_void);

#if 0
void
phy_digital_predistortion_8812a(void	*adapter);
#endif

void phy_dp_calibrate_8812a(struct dm_struct *dm);

void halrf_rf_lna_setting_8812a(struct dm_struct *dm, enum halrf_lna_set type);

void phy_set_rf_path_switch_8812a(
#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
				  struct dm_struct *dm,
#else
				  void *adapter,
#endif
				  boolean is_main);
#endif /*#ifndef __HALRF_8812A_H__*/
