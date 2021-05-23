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

#ifndef __HALRF_IQK_8814A_H__
#define __HALRF_IQK_8814A_H__

#if (RTL8814A_SUPPORT == 1)
/*--------------------------Define Parameters-------------------------------*/
#define MAC_REG_NUM_8814 2
#define BB_REG_NUM_8814 14
#define RF_REG_NUM_8814 1
/*---------------------------End Define Parameters-------------------------------*/

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void do_iqk_8814a(void *dm_void, u8 delta_thermal_index, u8 thermal_value,
		  u8 threshold);
#else
void do_iqk_8814a(void *dm_void, u8 delta_thermal_index, u8 thermal_value,
		  u8 threshold);
#endif

void phy_iq_calibrate_8814a_init(void *dm_void);

void phy_iq_calibrate_8814a(void *dm_void, boolean is_recovery);

#else /* (RTL8814A_SUPPORT == 0)*/

#define phy_iq_calibrate_8814a(_pdm_void, _recovery) 0
#define phy_iq_calibrate_8814a_init(_pdm_void)

#endif /* RTL8814A_SUPPORT */

#endif /*#ifndef __HALRF_IQK_8814A_H__*/
