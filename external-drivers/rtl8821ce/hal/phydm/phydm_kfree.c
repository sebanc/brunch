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

/*============================================================*/
/*include files*/
/*============================================================*/
#include "mp_precomp.h"
#include "phydm_precomp.h"


/*<YuChen, 150720> Add for KFree Feature Requested by RF David.*/
/*This is a phydm API*/



void
phydm_set_kfree_to_rf_8814a(
	void		*p_dm_void,
	u8		e_rf_path,
	u8		data
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_rf_calibration_structure	*p_rf_calibrate_info = &(p_dm_odm->rf_calibrate_info);
	boolean is_odd;

	if ((data % 2) != 0) {	/*odd->positive*/
		data = data - 1;
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(19), 1);
		is_odd = true;
	} else {		/*even->negative*/
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(19), 0);
		is_odd = false;
	}
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("phy_ConfigKFree8814A(): RF_0x55[19]= %d\n", is_odd));
	switch (data) {
	case 0:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 0);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 0);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 0;
		break;
	case 2:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 1);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 0);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 0;
		break;
	case 4:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 0);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 1);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 1;
		break;
	case 6:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 1);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 1);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 1;
		break;
	case 8:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 0);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 2);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 2;
		break;
	case 10:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 1);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 2);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 2;
		break;
	case 12:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 0);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 3);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 3;
		break;
	case 14:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 1);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 3);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 3;
		break;
	case 16:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 0);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 4);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 4;
		break;
	case 18:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 1);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 4);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 4;
		break;
	case 20:
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(14), 0);
		odm_set_rf_reg(p_dm_odm, e_rf_path, REG_RF_TX_GAIN_OFFSET, BIT(17) | BIT(16) | BIT(15), 5);
		p_rf_calibrate_info->kfree_offset[e_rf_path] = 5;
		break;

	default:
		break;
	}

	if (is_odd == false) {
		/*that means Kfree offset is negative, we need to record it.*/
		p_rf_calibrate_info->kfree_offset[e_rf_path] = (-1) * p_rf_calibrate_info->kfree_offset[e_rf_path];
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("phy_ConfigKFree8814A(): kfree_offset = %d\n", p_rf_calibrate_info->kfree_offset[e_rf_path]));
	} else
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("phy_ConfigKFree8814A(): kfree_offset = %d\n", p_rf_calibrate_info->kfree_offset[e_rf_path]));

}



void
phydm_get_thermal_trim_offset_8821c(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_power_trim_data	*p_power_trim_info = &(p_dm_odm->power_trim_data);

	u8 pg_therm = 0xff;

	odm_efuse_one_byte_read(p_dm_odm, PPG_THERMAL_OFFSET_8821C, &pg_therm, false);

	if (pg_therm != 0xff) {
		pg_therm = pg_therm & 0x1f;
		if ((pg_therm & BIT0) == 0)
			p_power_trim_info->thermal = (-1 * (pg_therm >> 1));
		else
			p_power_trim_info->thermal = (pg_therm >> 1);

		p_power_trim_info->flag |= KFREE_FLAG_THERMAL_K_ON;
	}

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] power trim flag:0x%02x\n", p_power_trim_info->flag));

	if (p_power_trim_info->flag & KFREE_FLAG_THERMAL_K_ON)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] thermal:%d\n", p_power_trim_info->thermal));
}



void
phydm_get_power_trim_offset_8821c(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_power_trim_data	*p_power_trim_info = &(p_dm_odm->power_trim_data);

	u8 pg_power = 0xff, i;

	odm_efuse_one_byte_read(p_dm_odm, PPG_BB_GAIN_2G_TXA_OFFSET_8821C, &pg_power, false);

	if (pg_power != 0xff) {
		p_power_trim_info->bb_gain[0][0] = pg_power;
		odm_efuse_one_byte_read(p_dm_odm, PPG_BB_GAIN_5GL1_TXA_OFFSET_8821C, &pg_power, false);
		p_power_trim_info->bb_gain[1][0] = pg_power;
		odm_efuse_one_byte_read(p_dm_odm, PPG_BB_GAIN_5GL2_TXA_OFFSET_8821C, &pg_power, false);
		p_power_trim_info->bb_gain[2][0] = pg_power;
		odm_efuse_one_byte_read(p_dm_odm, PPG_BB_GAIN_5GM1_TXA_OFFSET_8821C, &pg_power, false);
		p_power_trim_info->bb_gain[3][0] = pg_power;
		odm_efuse_one_byte_read(p_dm_odm, PPG_BB_GAIN_5GM2_TXA_OFFSET_8821C, &pg_power, false);
		p_power_trim_info->bb_gain[4][0] = pg_power;
		odm_efuse_one_byte_read(p_dm_odm, PPG_BB_GAIN_5GH1_TXA_OFFSET_8821C, &pg_power, false);
		p_power_trim_info->bb_gain[5][0] = pg_power;
		p_power_trim_info->flag |= KFREE_FLAG_ON;
	}

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] power trim flag:0x%02x\n", p_power_trim_info->flag));

	if (p_power_trim_info->flag & KFREE_FLAG_ON) {
		for (i = 0; i < 6; i++)
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree]  power_trim_data->bb_gain[%d][0]=0x%X\n", i, p_power_trim_info->bb_gain[i][0]));
	}
}



void
phydm_set_kfree_to_rf_8821c(
	void		*p_dm_void,
	u8		e_rf_path,
	boolean		wlg_btg,
	u8		data
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_rf_calibration_structure	*p_rf_calibrate_info = &(p_dm_odm->rf_calibrate_info);
	boolean is_odd;
	u8	wlg, btg;

	odm_set_rf_reg(p_dm_odm, e_rf_path, 0xde, BIT(0), 1);
	odm_set_rf_reg(p_dm_odm, e_rf_path, 0xde, BIT(5), 1);
	odm_set_rf_reg(p_dm_odm, e_rf_path, 0x55, BIT(6), 1);
	odm_set_rf_reg(p_dm_odm, e_rf_path, 0x65, BIT(6), 1);

	if (wlg_btg == true) {
		wlg = data & 0xf;
		btg = (data & 0xf0) >> 4;

		odm_set_rf_reg(p_dm_odm, e_rf_path, 0x55, BIT(19), (wlg & BIT(0)));
		odm_set_rf_reg(p_dm_odm, e_rf_path, 0x55, (BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14)), (wlg >> 1));

		odm_set_rf_reg(p_dm_odm, e_rf_path, 0x65, BIT(19), (btg & BIT(0)));
		odm_set_rf_reg(p_dm_odm, e_rf_path, 0x65, (BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14)), (btg >> 1));
	} else {
		odm_set_rf_reg(p_dm_odm, e_rf_path, 0x55, BIT(19), (data & BIT(0)));
		odm_set_rf_reg(p_dm_odm, e_rf_path, 0x55, (BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14)), ((data & 0x1f) >> 1));
	}

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD,
		("[kfree] 0x55[19:14]=0x%X 0x65[19:14]=0x%X\n",
		odm_get_rf_reg(p_dm_odm, e_rf_path, 0x55, (BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14))),
		odm_get_rf_reg(p_dm_odm, e_rf_path, 0x65, (BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14)))
		));
}

void
phydm_set_kfree_to_rf(
	void		*p_dm_void,
	u8		e_rf_path,
	u8		data
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (p_dm_odm->support_ic_type & ODM_RTL8814A)
		phydm_set_kfree_to_rf_8814a(p_dm_odm, e_rf_path, data);

	if ((p_dm_odm->support_ic_type & ODM_RTL8821C) && (*p_dm_odm->p_band_type == ODM_BAND_2_4G))
		phydm_set_kfree_to_rf_8821c(p_dm_odm, e_rf_path, true, data);
	else if (p_dm_odm->support_ic_type & ODM_RTL8821C)
		phydm_set_kfree_to_rf_8821c(p_dm_odm, e_rf_path, false, data);
}



void
phydm_get_thermal_trim_offset(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	struct _ADAPTER		*adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);
	PEFUSE_HAL		pEfuseHal = &(p_hal_data->EfuseHal);
	u1Byte			eFuseContent[DCMD_EFUSE_MAX_SECTION_NUM * EFUSE_MAX_WORD_UNIT * 2];

	if (HAL_MAC_Dump_EFUSE(&GET_HAL_MAC_INFO(adapter), EFUSE_WIFI, eFuseContent, pEfuseHal->PhysicalLen_WiFi, HAL_MAC_EFUSE_PHYSICAL, HAL_MAC_EFUSE_PARSE_DRV) != RT_STATUS_SUCCESS)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] dump efuse fail !!!\n"));
#endif

	if (p_dm_odm->support_ic_type & ODM_RTL8821C)
		phydm_get_thermal_trim_offset_8821c(p_dm_void);
}



void
phydm_get_power_trim_offset(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	struct _ADAPTER		*adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);
	PEFUSE_HAL		pEfuseHal = &(p_hal_data->EfuseHal);
	u1Byte			eFuseContent[DCMD_EFUSE_MAX_SECTION_NUM * EFUSE_MAX_WORD_UNIT * 2];

	if (HAL_MAC_Dump_EFUSE(&GET_HAL_MAC_INFO(adapter), EFUSE_WIFI, eFuseContent, pEfuseHal->PhysicalLen_WiFi, HAL_MAC_EFUSE_PHYSICAL, HAL_MAC_EFUSE_PARSE_DRV) != RT_STATUS_SUCCESS)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] dump efuse fail !!!\n"));
#endif

	if (p_dm_odm->support_ic_type & ODM_RTL8821C)
		phydm_get_power_trim_offset_8821c(p_dm_void);
}



s8
phydm_get_thermal_offset(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_power_trim_data	*p_power_trim_info = &(p_dm_odm->power_trim_data);

	if (p_power_trim_info->flag & KFREE_FLAG_THERMAL_K_ON)
		return p_power_trim_info->thermal;
	else
		return 0;
}



void
phydm_config_kfree(
	void	*p_dm_void,
	u8	channel_to_sw
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_rf_calibration_structure	*p_rf_calibrate_info = &(p_dm_odm->rf_calibrate_info);
	struct odm_power_trim_data	*p_power_trim_info = &(p_dm_odm->power_trim_data);

	u8			rfpath = 0, max_rf_path = 0;
	u8			channel_idx = 0, i;

	if (p_dm_odm->support_ic_type & ODM_RTL8814A)
		max_rf_path = 4;	/*0~3*/
	else if (p_dm_odm->support_ic_type & (ODM_RTL8812 | ODM_RTL8192E | ODM_RTL8822B))
		max_rf_path = 2;	/*0~1*/
	else if (p_dm_odm->support_ic_type & ODM_RTL8821C)
		max_rf_path = 1;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("===>[kfree] phy_ConfigKFree()\n"));

	if (p_rf_calibrate_info->reg_rf_kfree_enable == 2) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] phy_ConfigKFree(): reg_rf_kfree_enable == 2, Disable\n"));
		return;
	} else if (p_rf_calibrate_info->reg_rf_kfree_enable == 1 || p_rf_calibrate_info->reg_rf_kfree_enable == 0) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] phy_ConfigKFree(): reg_rf_kfree_enable == true\n"));
		/*Make sure the targetval is defined*/
		if (p_power_trim_info->flag & KFREE_FLAG_ON) {
			/*if kfree_table[0] == 0xff, means no Kfree*/
			if (*p_dm_odm->p_band_type == ODM_BAND_2_4G) {
				if (channel_to_sw >= 1 && channel_to_sw <= 14)
					channel_idx = PHYDM_2G;
			} else if (*p_dm_odm->p_band_type == ODM_BAND_5G) {
				if (channel_to_sw >= 36 && channel_to_sw <= 48)
					channel_idx = PHYDM_5GLB1;
				if (channel_to_sw >= 52 && channel_to_sw <= 64)
					channel_idx = PHYDM_5GLB2;
				if (channel_to_sw >= 100 && channel_to_sw <= 120)
					channel_idx = PHYDM_5GMB1;
				if (channel_to_sw >= 122 && channel_to_sw <= 144)
					channel_idx = PHYDM_5GMB2;
				if (channel_to_sw >= 149 && channel_to_sw <= 177)
					channel_idx = PHYDM_5GHB;
			}

			for (i = 0; i < (max_rf_path * BB_GAIN_NUM); i++)
				ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] p_power_trim_info->bb_gain[%d][%d]=0x%X\n", i, max_rf_path - 1, p_power_trim_info->bb_gain[i][max_rf_path - 1]));

			for (rfpath = ODM_RF_PATH_A;  rfpath < max_rf_path; rfpath++) {
				ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] phydm_kfree(): channel_to_sw=%d PATH_%d: 0x%X\n", channel_to_sw, rfpath, p_power_trim_info->bb_gain[channel_idx][rfpath]));

				phydm_set_kfree_to_rf(p_dm_odm, rfpath, p_power_trim_info->bb_gain[channel_idx][rfpath]);
			}
		} else {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("[kfree] phy_ConfigKFree(): targetval not defined, Don't execute KFree Process.\n"));
			return;
		}
	}
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_MP, ODM_DBG_LOUD, ("<===[kfree] phy_ConfigKFree()\n"));
}
