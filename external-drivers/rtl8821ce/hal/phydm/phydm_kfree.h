
/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

#ifndef	__PHYDMKFREE_H__
#define    __PHYDKFREE_H__

#define KFREE_VERSION	"1.0"

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)

#define	BB_GAIN_NUM		6
#define KFREE_FLAG_ON				BIT(0)
#define KFREE_FLAG_THERMAL_K_ON		BIT(1)

#endif

#define PPG_BB_GAIN_2G_TXA_OFFSET_8821C		0x1EE
#define PPG_BB_GAIN_5GL1_TXA_OFFSET_8821C		0x1EC
#define PPG_BB_GAIN_5GL2_TXA_OFFSET_8821C		0x1E8
#define PPG_BB_GAIN_5GM1_TXA_OFFSET_8821C		0x1E4
#define PPG_BB_GAIN_5GM2_TXA_OFFSET_8821C		0x1E0
#define PPG_BB_GAIN_5GH1_TXA_OFFSET_8821C		0x1DC

#define PPG_THERMAL_OFFSET_8821C		0x1EF



struct odm_power_trim_data {
	u8 flag;
	s8 bb_gain[BB_GAIN_NUM][MAX_RF_PATH];
	s8 thermal;
};



enum phydm_kfree_channeltosw {
	PHYDM_2G = 0,
	PHYDM_5GLB1 = 1,
	PHYDM_5GLB2 = 2,
	PHYDM_5GMB1 = 3,
	PHYDM_5GMB2 = 4,
	PHYDM_5GHB = 5,
};



void
phydm_get_thermal_trim_offset(
	void	*p_dm_void
);

void
phydm_get_power_trim_offset(
	void	*p_dm_void
);

s8
phydm_get_thermal_offset(
	void	*p_dm_void
);

void
phydm_config_kfree(
	void	*p_dm_void,
	u8	channel_to_sw
);


#endif
