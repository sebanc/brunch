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

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8814A_SUPPORT == 1)
s8 phydm_cck_rssi_8814a(struct dm_struct *dm, u16 lna_idx, u8 vga_idx)
{
	s8 rx_pwr_all = 0;

	switch (lna_idx) {
	case 7:
		rx_pwr_all = -38 - (2 * vga_idx);
		break;
	case 5:
		rx_pwr_all = -28 - (2 * vga_idx);
		break;
	case 3:
		rx_pwr_all = -8 - (2 * vga_idx);
		break;
	case 2:
		rx_pwr_all = -1 - (2 * vga_idx);
		break;
	default:
		break;
	}
	return rx_pwr_all;
}

u8 phydm_spur_nbi_setting_8814a(struct dm_struct *dm)
{
	u8 set_result = 0;

	/*dm->channel means central frequency, so we can use 20M as input*/
	if (dm->rfe_type == 0 || dm->rfe_type == 1 || dm->rfe_type == 6 ||
	    dm->rfe_type == 7) {
		/*channel asked by RF Jeff*/
		if (*dm->channel == 14)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 40, 2480,
						       PHYDM_DONT_CARE);
		else if (*dm->channel >= 4 && *dm->channel <= 8)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 40, 2440,
						       PHYDM_DONT_CARE);
#if 0
		else if (*dm->channel == 153)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 20, 5760,
						       PHYDM_DONT_CARE);
		else if (*dm->channel == 54)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 40, 5280,
						       PHYDM_DONT_CARE);
		else if (*dm->channel == 118)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 40, 5600,
						       PHYDM_DONT_CARE);
		else if (*dm->channel == 151)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 40, 5760,
						       PHYDM_DONT_CARE);
		else if (*dm->channel == 58)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 80, 5280,
						       PHYDM_DONT_CARE);
		else if (*dm->channel == 122)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 80, 5600,
						       PHYDM_DONT_CARE);
		else if (*dm->channel == 155)
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE,
						       *dm->channel, 80, 5760,
						       PHYDM_DONT_CARE);
#endif
		else
			set_result = phydm_nbi_setting(dm, FUNC_DISABLE,
						       *dm->channel, 40, 2440,
						       PHYDM_DONT_CARE);
	}
	PHYDM_DBG(dm, ODM_COMP_API, "%s, set_result = 0x%x, channel = %d\n",
		  __func__, set_result, *dm->channel);
	dm->nbi_set_result = set_result;
	return set_result;
}

void phydm_dynamic_nbi_switch_8814a(struct dm_struct *dm)
{
	/*if rssi < 15%, disable nbi notch filter, if rssi > 20%, enable nbi notch filter*/
	/*add by YuChen 20160218*/
	if (dm->rfe_type == 0 || dm->rfe_type == 1 || dm->rfe_type == 6 || dm->rfe_type == 7) {
		if (dm->nbi_set_result == PHYDM_SET_SUCCESS) {
			if (dm->rssi_min <= 15)
				odm_set_bb_reg(dm, R_0x87c, BIT(13), 0x0);
			else if (dm->rssi_min >= 20)
				odm_set_bb_reg(dm, R_0x87c, BIT(13), 0x1);
		}
		PHYDM_DBG(dm, ODM_COMP_API, "%s\n", __func__);
	}
}

void phydm_hwsetting_8814a(struct dm_struct *dm)
{
	phydm_dynamic_nbi_switch_8814a(dm);
}

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
/*@For normal driver we always use the FW method to configure TX power index to reduce I/O transaction.*/
void odm_set_tx_power_level8814(void *adapter, u8 channel, u8 pwr_lvl)
{
#if (DEV_BUS_TYPE == RT_USB_INTERFACE)
	u32 i, j, k = 0;
	u32 value[264] = {0};
	u32 path = 0, power_index, txagc_table_wd = 0x00801000;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	u8 jaguar2_rates[][4] = {{MGN_1M, MGN_2M, MGN_5_5M, MGN_11M},
				 {MGN_6M, MGN_9M, MGN_12M, MGN_18M},
				 {MGN_24M, MGN_36M, MGN_48M, MGN_54M},
				 {MGN_MCS0, MGN_MCS1, MGN_MCS2, MGN_MCS3},
				 {MGN_MCS4, MGN_MCS5, MGN_MCS6, MGN_MCS7},
				 {MGN_MCS8, MGN_MCS9, MGN_MCS10, MGN_MCS11},
				 {MGN_MCS12, MGN_MCS13, MGN_MCS14, MGN_MCS15},
				 {MGN_MCS16, MGN_MCS17, MGN_MCS18, MGN_MCS19},
				 {MGN_MCS20, MGN_MCS21, MGN_MCS22, MGN_MCS23},
				 {MGN_VHT1SS_MCS0, MGN_VHT1SS_MCS1, MGN_VHT1SS_MCS2, MGN_VHT1SS_MCS3},
				 {MGN_VHT1SS_MCS4, MGN_VHT1SS_MCS5, MGN_VHT1SS_MCS6, MGN_VHT1SS_MCS7},
				 {MGN_VHT2SS_MCS8, MGN_VHT2SS_MCS9, MGN_VHT2SS_MCS0, MGN_VHT2SS_MCS1},
				 {MGN_VHT2SS_MCS2, MGN_VHT2SS_MCS3, MGN_VHT2SS_MCS4, MGN_VHT2SS_MCS5},
				 {MGN_VHT2SS_MCS6, MGN_VHT2SS_MCS7, MGN_VHT2SS_MCS8, MGN_VHT2SS_MCS9},
				 {MGN_VHT3SS_MCS0, MGN_VHT3SS_MCS1, MGN_VHT3SS_MCS2, MGN_VHT3SS_MCS3},
				 {MGN_VHT3SS_MCS4, MGN_VHT3SS_MCS5, MGN_VHT3SS_MCS6, MGN_VHT3SS_MCS7},
				 {MGN_VHT3SS_MCS8, MGN_VHT3SS_MCS9, 0, 0}};

	for (path = RF_PATH_A; path <= RF_PATH_D; ++path) {
		u8 usb_host = UsbModeQueryHubUsbType(adapter);
		u8 usb_rfset = UsbModeQueryRfSet(adapter);
		u8 usb_rf_type = RT_GetRFType((PADAPTER)adapter);

		for (i = 0; i <= 16; i++) {
			for (j = 0; j <= 3; j++) {
				if (jaguar2_rates[i][j] == 0)
					continue;

				txagc_table_wd = 0x00801000;
				power_index = (u32)PHY_GetTxPowerIndex(adapter, (u8)path, jaguar2_rates[i][j], hal_data->CurrentChannelBW, channel);

				/*@for Query bus type to recude tx power.*/
				if (usb_host != USB_MODE_U3 && usb_rfset == 1 && IS_HARDWARE_TYPE_8814AU(adapter) && usb_rf_type == RF_3T3R) {
					if (channel <= 14) {
						if (power_index >= 16)
							power_index -= 16;
						else
							power_index = 0;
					} else
						power_index = 0;
				}

				if (pwr_lvl == tx_high_pwr_level_level1) {
					if (power_index >= 0x10)
						power_index -= 0x10;
					else
						power_index = 0;
				} else if (pwr_lvl == tx_high_pwr_level_level2)
					power_index = 0;

				txagc_table_wd |= (path << 8) | MRateToHwRate(jaguar2_rates[i][j]) | (power_index << 24);

				PHY_SetTxPowerIndexShadow(adapter, (u8)power_index, (u8)path, jaguar2_rates[i][j]);

				value[k++] = txagc_table_wd;
			}
		}
	}

	if (((PADAPTER)adapter)->MgntInfo.bScanInProgress == false && ((PADAPTER)adapter)->MgntInfo.RegFWOffload == 2)
		HalDownloadTxPowerLevel8814(adapter, value);
#endif
}

void odm_dynamic_tx_power_8814a(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	void *adapter = dm->adapter;
	PMGNT_INFO mgnt_info = &((PADAPTER)adapter)->MgntInfo;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	s32 rssi = dm->rssi_min;

	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "[%s] Tx_Lv=%d, iot_action=%x DTP_en=%d\n",
		  __func__, hal_data->DynamicTxHighPowerLvl,
		  mgnt_info->IOTAction, mgnt_info->bDynamicTxPowerEnable);

	/*STA not connected and AP not connected*/
	if (!mgnt_info->bMediaConnect &&
	    hal_data->EntryMinUndecoratedSmoothedPWDB == 0) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "Not connected to any reset power lvl\n");
		hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
		return;
	}

	if (!mgnt_info->bDynamicTxPowerEnable ||
	    mgnt_info->IOTAction & HT_IOT_ACT_DISABLE_HIGH_POWER) {
		hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
	} else {
		/*Should we separate as 2.4G/5G band?*/
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "rssi_tmp = %d\n", rssi);

		if (rssi >= TX_POWER_NEAR_FIELD_THRESH_LVL2) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_level2;
		} else if ((rssi < (TX_POWER_NEAR_FIELD_THRESH_LVL2 - 3)) &&
			   (rssi >= TX_POWER_NEAR_FIELD_THRESH_LVL1)) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_level1;
		} else if (rssi < (TX_POWER_NEAR_FIELD_THRESH_LVL1 - 5)) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
		}
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "TxHighPowerLvl = %d\n",
			  hal_data->DynamicTxHighPowerLvl);
	}

	if (hal_data->DynamicTxHighPowerLvl != hal_data->LastDTPLvl) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "channel = %d\n",
			  hal_data->CurrentChannel);
		odm_set_tx_power_level8814(adapter, hal_data->CurrentChannel,
					   hal_data->DynamicTxHighPowerLvl);
	}

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "channel = %d  TXpower lvl=%d/%d\n",
		  hal_data->CurrentChannel, hal_data->LastDTPLvl,
		  hal_data->DynamicTxHighPowerLvl);

	hal_data->LastDTPLvl = hal_data->DynamicTxHighPowerLvl;
}
#endif /* #if (DM_ODM_SUPPORT_TYPE == ODM_WIN) */


#endif /* RTL8814A_SUPPORT == 1 */
