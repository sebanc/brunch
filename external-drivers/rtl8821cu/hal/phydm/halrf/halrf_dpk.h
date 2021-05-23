/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __HALRF_DPK_H__
#define __HALRF_DPK_H__

/*--------------------------Define Parameters-------------------------------*/
#define GAIN_LOSS 1
#define DO_DPK 2
#define DPK_ON 3
#define LOSS_CHK 0
#define GAIN_CHK 1
#define AVG_THERMAL_NUM	8
#define AVG_THERMAL_NUM_DPK 8
#define THERMAL_DPK_AVG_NUM 4

/*---------------------------End Define Parameters---------------------------*/

struct dm_dpk_info {

	boolean	is_dpk_enable;
	/*boolean	is_dpk_path_ok[4];*/		/*path*/
	u16	dpk_path_ok;			
	/*BIT(15)~BIT(12) : 5G reserved, BIT(11)~BIT(8) 5G_S3~5G_S0*/
	/*BIT(7)~BIT(4) : 2G reserved, BIT(3)~BIT(0) 2G_S3~2G_S0*/

#if (RTL8198F_SUPPORT == 1 || RTL8192F_SUPPORT == 1 || RTL8197F_SUPPORT == 1)
	/*2G DPK data*/
	u8 	dpk_result[4][3];		/*path/group*/
	u8 	pwsf_2g[4][3];			/*path/group*/
	u32	lut_2g_even[4][3][64];		/*path/group/LUT data*/
	u32	lut_2g_odd[4][3][64];		/*path/group/LUT data*/
#if 0
	/*5G DPK data*/
	u8 	dpk_5g_result[4][9];		/*path/group*/
	u8 	pwsf_5g[4][9];			/*path/group*/
	u32	lut_5g_even[4][9][64];		/*path/group/LUT data*/
	u32	lut_5g_odd[4][9][64];		/*path/group/LUT data*/
#endif
	u8	thermal_dpk;
	u8	thermal_dpk_avg[AVG_THERMAL_NUM_DPK];
	u8	thermal_dpk_avg_index;

#endif

#if (RTL8195B_SUPPORT == 1)
	u32 sram_even[7][64];
	u32 sram_odd[7][64];
	boolean dpk_result[7];
	u32 dpk_pwsf[7];
#endif

};

#endif /*#ifndef __HALRF_DPK_H__*/
