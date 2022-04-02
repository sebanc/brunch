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

/*************************************************************
 * include files
 ************************************************************/
#include "mp_precomp.h"
#include "phydm_precomp.h"

/* *********************Power training init************************ */
void phydm_pow_train_init(void *dm_void)
{
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	void *adapter = dm->adapter;
	PMGNT_INFO mgnt_info = &((PADAPTER)adapter)->MgntInfo;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	/* This is for power training init @ 11N serious */
	#if DEV_BUS_TYPE == RT_USB_INTERFACE
	if (RT_GetInterfaceSelection((PADAPTER)adapter) == INTF_SEL1_USB_High_Power) {
		odm_dynamic_tx_power_save_power_index(dm);
	}
	#else

	/* so 92c pci do not need dynamic tx power? vivi check it later */
	#endif
#endif
}

void odm_dynamic_tx_power_save_power_index(void *dm_void)
{
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 index;
	u32 power_index_reg[6] = {0xc90, 0xc91, 0xc92, 0xc98, 0xc99, 0xc9a};

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	/* Save PT index, but nothing used?? */
	void *adapter = dm->adapter;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	for (index = 0; index < 6; index++)
		hal_data->PowerIndex_backup[index] = PlatformEFIORead1Byte((PADAPTER)adapter, power_index_reg[index]);

#endif
#endif
}

void odm_dynamic_tx_power_restore_power_index(void *dm_void)
{
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 index;
	void *adapter = dm->adapter;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	u32 power_index_reg[6] = {0xc90, 0xc91, 0xc92, 0xc98, 0xc99, 0xc9a};

	for (index = 0; index < 6; index++)
		PlatformEFIOWrite1Byte(adapter, power_index_reg[index], hal_data->PowerIndex_backup[index]);

#endif
}

void odm_dynamic_tx_power_write_power_index(void *dm_void, u8 value)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 index;
	u32 power_index_reg[6] = {0xc90, 0xc91, 0xc92, 0xc98, 0xc99, 0xc9a};

	for (index = 0; index < 6; index++)
		/* platform_efio_write_1byte(adapter, power_index_reg[index], value); */
		odm_write_1byte(dm, power_index_reg[index], value);
}

/* ************************************************************ */

#ifdef CONFIG_DYNAMIC_TX_TWR

boolean
phydm_check_rates(
	void *dm_void,
	u8 rate_idx)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 check_rate_bitmap0 = 0x08080808; /* check CCK11M, OFDM54M, MCS7, MCS15*/
	u32 check_rate_bitmap1 = 0x80200808; /* check MCS23, MCS31, VHT1SS M9, VHT2SS M9*/
	u32 check_rate_bitmap2 = 0x00080200; /* check VHT3SS M9, VHT4SS M9*/
	u32 bitmap_result;

#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8822B) {
		check_rate_bitmap2 &= 0;
		check_rate_bitmap1 &= 0xfffff000;
		check_rate_bitmap0 &= 0x0fffffff;
	}
#endif

#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F) {
		check_rate_bitmap2 &= 0;
		check_rate_bitmap1 &= 0;
		check_rate_bitmap0 &= 0x0fffffff;
	}
#endif

#if (RTL8192E_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192E) {
		check_rate_bitmap2 &= 0;
		check_rate_bitmap1 &= 0;
		check_rate_bitmap0 &= 0x0fffffff;
	}
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192F) {
		check_rate_bitmap2 &= 0;
		check_rate_bitmap1 &= 0;
		check_rate_bitmap0 &= 0x0fffffff;
	}
#endif
#if (RTL8821C_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8821C) {
		check_rate_bitmap2 &= 0;
		check_rate_bitmap1 &= 0x003ff000;
		check_rate_bitmap0 &= 0x000fffff;
	}
#endif

	if (rate_idx >= 64)
		bitmap_result = BIT(rate_idx - 64) & check_rate_bitmap2;
	else if (rate_idx >= 32)
		bitmap_result = BIT(rate_idx - 32) & check_rate_bitmap1;
	else if (rate_idx <= 31)
		bitmap_result = BIT(rate_idx) & check_rate_bitmap0;

	if (bitmap_result != 0)
		return true;
	else
		return false;
}

enum rf_path
phydm_check_paths(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	enum rf_path max_path = RF_PATH_A;

	if (dm->num_rf_path == 1)
		max_path = RF_PATH_A;
	if (dm->num_rf_path == 2)
		max_path = RF_PATH_B;
	if (dm->num_rf_path == 3)
		max_path = RF_PATH_C;
	if (dm->num_rf_path == 4)
		max_path = RF_PATH_D;

	return max_path;
}

#ifndef PHYDM_COMMON_API_SUPPORT
u8 phydm_dtp_get_txagc(
	void *dm_void,
	enum rf_path path,
	u8 hw_rate)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 ret = 0xff;
#if (RTL8192E_SUPPORT == 1)
	ret = config_phydm_read_txagc_n(dm, path, hw_rate);
#endif
	return ret;
}
#endif

u8 phydm_search_min_power_index(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	enum rf_path path;
	enum rf_path max_path;
	u8 min_gain_index = 0x3f;
	u8 gain_index;
	u8 rate_idx;

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s\n", __func__);
	max_path = phydm_check_paths(dm);
	for (path = 0; path <= max_path; path++)
		for (rate_idx = 0; rate_idx < 84; rate_idx++)
			if (phydm_check_rates(dm, rate_idx)) {
#ifdef PHYDM_COMMON_API_SUPPORT
				/*This is for API support IC : 97F,8822B,92F,8821C*/
				gain_index = phydm_api_get_txagc(dm, path, rate_idx);
#else
				/*This is for API non-support IC : 92E */
				gain_index = phydm_dtp_get_txagc(dm, path, rate_idx);
#endif
				if (gain_index == 0xff) {
					min_gain_index = 0x20;
					PHYDM_DBG(dm, DBG_DYN_TXPWR, "Error Gain idx!! Rewite to: ((%d))\n", min_gain_index);
					break;
				}
				PHYDM_DBG(dm, DBG_DYN_TXPWR,
					  "Support Rate: ((%d)) -> Gain idx: ((%d))\n",
					  rate_idx, gain_index);
				if (gain_index < min_gain_index)
					min_gain_index = gain_index;
			}

	return min_gain_index;
}

void phydm_dynamic_tx_power_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 i;
	dm->last_dtp_lvl = tx_high_pwr_level_normal;
	dm->dynamic_tx_high_power_lvl = tx_high_pwr_level_normal;
	for (i = 0; i < 3; i++) {
		dm->enhance_pwr_th[i] = 0xff;
	}
	dm->set_pwr_th[0] = TX_POWER_NEAR_FIELD_THRESH_LVL1;
	dm->set_pwr_th[1] = TX_POWER_NEAR_FIELD_THRESH_LVL2;
	dm->set_pwr_th[2] = 0xff;
	dm->min_power_index = phydm_search_min_power_index(dm);
	PHYDM_DBG(dm, DBG_DYN_TXPWR, "DTP init: Min Gain idx: ((%d))\n",
		  dm->min_power_index);
}

void phydm_noisy_enhance_hp_th(
	void *dm_void,
	u8 noisy_state)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	if (noisy_state == 0) {
		dm->enhance_pwr_th[0] = dm->set_pwr_th[0];
		dm->enhance_pwr_th[1] = dm->set_pwr_th[1];
		dm->enhance_pwr_th[2] = dm->set_pwr_th[2];
	} else {
		dm->enhance_pwr_th[0] = dm->set_pwr_th[0] + 8;
		dm->enhance_pwr_th[1] = dm->set_pwr_th[1] + 5;
		dm->enhance_pwr_th[2] = dm->set_pwr_th[2];
	}
	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "DTP hp_th: Lv1_th =%d ,Lv2_th = %d ,Lv3_th = %d\n",
		  dm->enhance_pwr_th[0], dm->enhance_pwr_th[1],
		  dm->enhance_pwr_th[2]);
}

u8 phydm_pwr_lvl_check(
	void *dm_void,
	u8 input_rssi)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	if (input_rssi >= dm->enhance_pwr_th[2]) {
		return tx_high_pwr_level_level3;
		/**/
	} else if (input_rssi < (dm->enhance_pwr_th[2] - 3) && input_rssi >= dm->enhance_pwr_th[1]) {
		return tx_high_pwr_level_level2;
		/**/
	} else if (input_rssi < (dm->enhance_pwr_th[1] - 3) && input_rssi >= dm->enhance_pwr_th[0]) {
		return tx_high_pwr_level_level1;
		/**/
	} else if (input_rssi < (dm->enhance_pwr_th[0] - 3)) {
		return tx_high_pwr_level_normal;
		/**/
	} else {
		return tx_high_pwr_level_unchange;
	}
}

u8 phydm_pwr_lv_mapping(
	u8 tx_pwr_lv)
{
	if (tx_pwr_lv == tx_high_pwr_level_level3)
		return PHYDM_OFFSET_MINUS_11DB;
	else if (tx_pwr_lv == tx_high_pwr_level_level2)
		return PHYDM_OFFSET_MINUS_7DB;
	else if (tx_pwr_lv == tx_high_pwr_level_level1)
		return PHYDM_OFFSET_MINUS_3DB;
	else
		return PHYDM_OFFSET_ZERO;
}

void phydm_dynamic_response_power(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 reg_resp_pwr_offset;
	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;
	if (dm->dynamic_tx_high_power_lvl == tx_high_pwr_level_unchange) {
		dm->dynamic_tx_high_power_lvl = dm->last_dtp_lvl;
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "RespPwr not change\n");
		return;
	}

	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "RespPwr update_DTP_lv: ((%d)) -> ((%d))\n", dm->last_dtp_lvl,
		  dm->dynamic_tx_high_power_lvl);
	dm->last_dtp_lvl = dm->dynamic_tx_high_power_lvl;
	reg_resp_pwr_offset = phydm_pwr_lv_mapping(dm->dynamic_tx_high_power_lvl);
	odm_set_mac_reg(dm, ODM_REG_RESP_TX_11AC, BIT(20) | BIT(19) | BIT(18), reg_resp_pwr_offset);
	PHYDM_DBG(dm, DBG_DYN_TXPWR, "RespPwr Set TxPwr: Lv (%d)\n",
		  dm->dynamic_tx_high_power_lvl);
}

void phydm_dtp_fill_cmninfo(
	void *dm_void,
	u8 macid,
	u8 dtp_lvl)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dtp_info *dtp = NULL;
	dtp = &dm->phydm_sta_info[macid]->dtp_stat;
	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;
	dtp->dyn_tx_power = phydm_pwr_lv_mapping(dtp_lvl);
	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "Fill cmninfo TxPwr: macid=(%d), PwrLv (%d)\n", macid,
		  dtp->dyn_tx_power);
}

void phydm_dtp_per_sta(
	void *dm_void,
	u8 macid)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct cmn_sta_info *sta = dm->phydm_sta_info[macid];
	struct dtp_info *dtp = NULL;
	struct rssi_info *rssi = NULL;
	if (is_sta_active(sta)) {
		dtp = &sta->dtp_stat;
		rssi = &sta->rssi_stat;
		dtp->sta_tx_high_power_lvl = phydm_pwr_lvl_check(dm, rssi->rssi);
		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "STA=%d , RSSI: %d , GetPwrLv: %d\n", macid,
			  rssi->rssi, dtp->sta_tx_high_power_lvl);
		if (dtp->sta_tx_high_power_lvl == tx_high_pwr_level_unchange || dtp->sta_tx_high_power_lvl == dtp->sta_last_dtp_lvl) {
			dtp->sta_tx_high_power_lvl = dtp->sta_last_dtp_lvl;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "DTP_lv not change: ((%d))\n",
				  dtp->sta_tx_high_power_lvl);
			return;
		}

		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "DTP_lv update: ((%d)) -> ((%d))\n", dm->last_dtp_lvl,
			  dm->dynamic_tx_high_power_lvl);
		dtp->sta_last_dtp_lvl = dtp->sta_tx_high_power_lvl;
		phydm_dtp_fill_cmninfo(dm, macid, dtp->sta_tx_high_power_lvl);
	}
}

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)

void odm_dynamic_tx_power_8821(void *dm_void, u8 *desc, u8 mac_id)
{
#if (RTL8821A_SUPPORT == 1)

	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct cmn_sta_info *entry;
	u8 reg0xc56_byte;
	u8 txpwr_offset = 0;

	entry = dm->phydm_sta_info[mac_id];

	reg0xc56_byte = odm_read_1byte(dm, 0xc56);

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "reg0xc56_byte=%d\n", reg0xc56_byte);

	if (entry[mac_id].rssi_stat.rssi > 85) {
		/* Avoid TXAGC error after TX power offset is applied.
		For example: Reg0xc56=0x6, if txpwr_offset=3( reduce 11dB )
		Total power = 6-11= -5( overflow!! ), PA may be burned !
		so txpwr_offset should be adjusted by Reg0xc56*/

		if (reg0xc56_byte < 14)
			txpwr_offset = 1;
		else if (reg0xc56_byte < 22)
			txpwr_offset = 2;
		else
			txpwr_offset = 3;

		SET_TX_DESC_TX_POWER_OFFSET_8812(desc, txpwr_offset);
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: RSSI=%d, txpwr_offset=%d\n",
			  __func__, entry[mac_id].rssi_stat.rssi, txpwr_offset);

	} else {
		SET_TX_DESC_TX_POWER_OFFSET_8812(desc, txpwr_offset);
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: RSSI=%d, txpwr_offset=%d\n",
			  __func__, entry[mac_id].rssi_stat.rssi, txpwr_offset);
	}
#endif /*#if (RTL8821A_SUPPORT==1)*/
}

void odm_dynamic_tx_power_8814a(
	void *dm_void)
{
#if (RTL8814A_SUPPORT == 1)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	void *adapter = dm->adapter;
	PMGNT_INFO mgnt_info = &((PADAPTER)adapter)->MgntInfo;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	s32 undecorated_smoothed_pwdb = dm->rssi_min;

	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "TxLevel=%d mgnt_info->iot_action=%x mgnt_info->is_dynamic_tx_power_enable=%d\n",
		  hal_data->DynamicTxHighPowerLvl, mgnt_info->IOTAction,
		  mgnt_info->bDynamicTxPowerEnable);

	/*STA not connected and AP not connected*/
	if (!mgnt_info->bMediaConnect && hal_data->EntryMinUndecoratedSmoothedPWDB == 0) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "Not connected to any reset power lvl\n");
		hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
		return;
	}

	if (!mgnt_info->bDynamicTxPowerEnable || mgnt_info->IOTAction & HT_IOT_ACT_DISABLE_HIGH_POWER)
		hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
	else {
		/*Should we separate as 2.4G/5G band?*/
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "rssi_tmp = %d\n",
			  undecorated_smoothed_pwdb);

		if (undecorated_smoothed_pwdb >= TX_POWER_NEAR_FIELD_THRESH_LVL2) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_level2;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "tx_high_pwr_level_level1 (TxPwr=0x0)\n");
		} else if ((undecorated_smoothed_pwdb < (TX_POWER_NEAR_FIELD_THRESH_LVL2 - 3)) &&
			   (undecorated_smoothed_pwdb >= TX_POWER_NEAR_FIELD_THRESH_LVL1)) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_level1;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "tx_high_pwr_level_level1 (TxPwr=0x10)\n");
		} else if (undecorated_smoothed_pwdb < (TX_POWER_NEAR_FIELD_THRESH_LVL1 - 5)) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "tx_high_pwr_level_normal\n");
		}
	}

	if (hal_data->DynamicTxHighPowerLvl != hal_data->LastDTPLvl) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: channel = %d\n", __func__,
			  hal_data->CurrentChannel);
		odm_set_tx_power_level8814(adapter, hal_data->CurrentChannel, hal_data->DynamicTxHighPowerLvl);
	}

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: channel = %d  TXpower lvl=%d/%d\n",
		  __func__, hal_data->CurrentChannel, hal_data->LastDTPLvl,
		  hal_data->DynamicTxHighPowerLvl);

	hal_data->LastDTPLvl = hal_data->DynamicTxHighPowerLvl;
#endif
}

/**/
/*For normal driver we always use the FW method to configure TX power index to reduce I/O transaction.*/
/**/
/**/
void odm_set_tx_power_level8814(
	void *adapter,
	u8 channel,
	u8 pwr_lvl)
{
#if (RTL8814A_SUPPORT == 1)
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

				/*for Query bus type to recude tx power.*/
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
#endif
}
#endif

void odm_set_dyntxpwr(
	void *dm_void,
	u8 *desc,
	u8 macid)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dtp_info *dtp = NULL;
	dtp = &dm->phydm_sta_info[macid]->dtp_stat;
	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;
	if (dm->fill_desc_dyntxpwr)
		dm->fill_desc_dyntxpwr(dm, desc, dtp->dyn_tx_power);
	else
		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "%s: fill_desc_dyntxpwr is null!\n", __func__);
	if (dtp->last_tx_power != dtp->dyn_tx_power) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "%s: last_offset=%d, txpwr_offset=%d\n", __func__,
			  dtp->last_tx_power, dtp->dyn_tx_power);
		dtp->last_tx_power = dtp->dyn_tx_power;
	}
}

#else

void phydm_dynamic_tx_power_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	void *adapter = dm->adapter;
	PMGNT_INFO mgnt_info = &((PADAPTER)adapter)->MgntInfo;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);

	/*if (!IS_HARDWARE_TYPE_8814A(adapter)) {*/
	/*	PHYDM_DBG(dm,DBG_DYN_TXPWR, */
	/*	("DynamicTxPowerEnable=%d\n", mgnt_info->is_dynamic_tx_power_enable));*/
	/*	return;*/
	/*} else*/
	{
		mgnt_info->bDynamicTxPowerEnable = true;
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "DynamicTxPowerEnable=%d\n",
			  mgnt_info->bDynamicTxPowerEnable);
	}

#if DEV_BUS_TYPE == RT_USB_INTERFACE
	if (RT_GetInterfaceSelection((PADAPTER)adapter) == INTF_SEL1_USB_High_Power) {
		mgnt_info->bDynamicTxPowerEnable = true;
	} else
#else
	/* so 92c pci do not need dynamic tx power? vivi check it later */
	mgnt_info->bDynamicTxPowerEnable = false;
#endif

		hal_data->LastDTPLvl = tx_high_pwr_level_normal;
	hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;

#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)

	dm->last_dtp_lvl = tx_high_pwr_level_normal;
	dm->dynamic_tx_high_power_lvl = tx_high_pwr_level_normal;
	dm->tx_agc_ofdm_18_6 = odm_get_bb_reg(dm, R_0xc24, MASKDWORD); /*TXAGC {18M 12M 9M 6M}*/

#endif
}

void odm_dynamic_tx_power_nic_ce(void *dm_void)
{
#if (DM_ODM_SUPPORT_TYPE & (ODM_CE))
#if (RTL8821A_SUPPORT == 1)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 val;
	u8 rssi_tmp = dm->rssi_min;

	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;

	if (rssi_tmp >= TX_POWER_NEAR_FIELD_THRESH_LVL2) {
		dm->dynamic_tx_high_power_lvl = tx_high_pwr_level_level2;
		/**/
	} else if (rssi_tmp >= TX_POWER_NEAR_FIELD_THRESH_LVL1) {
		dm->dynamic_tx_high_power_lvl = tx_high_pwr_level_level1;
		/**/
	} else if (rssi_tmp < (TX_POWER_NEAR_FIELD_THRESH_LVL1 - 5)) {
		dm->dynamic_tx_high_power_lvl = tx_high_pwr_level_normal;
		/**/
	}

	if (dm->last_dtp_lvl == dm->dynamic_tx_high_power_lvl)
		return;

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "update_DTP_lv: ((%d)) -> ((%d))\n",
		  dm->last_dtp_lvl, dm->dynamic_tx_high_power_lvl);

	dm->last_dtp_lvl = dm->dynamic_tx_high_power_lvl;

	if (dm->support_ic_type & (ODM_RTL8821)) {
		if (dm->dynamic_tx_high_power_lvl == tx_high_pwr_level_level2) {
			odm_set_mac_reg(dm, R_0x6d8, BIT(20) | BIT19 | BIT18, 1); /* Resp TXAGC offset = -3dB*/

			val = dm->tx_agc_ofdm_18_6 & 0xff;
			if (val >= 0x20)
				val -= 0x16;

			odm_set_bb_reg(dm, R_0xc24, 0xff, val);
			PHYDM_DBG(dm, DBG_DYN_TXPWR, "Set TX power: level 2\n");
		} else if (dm->dynamic_tx_high_power_lvl == tx_high_pwr_level_level1) {
			odm_set_mac_reg(dm, R_0x6d8, BIT(20) | BIT19 | BIT18, 1); /* Resp TXAGC offset = -3dB*/

			val = dm->tx_agc_ofdm_18_6 & 0xff;
			if (val >= 0x20)
				val -= 0x10;

			odm_set_bb_reg(dm, R_0xc24, 0xff, val);
			PHYDM_DBG(dm, DBG_DYN_TXPWR, "Set TX power: level 1\n");
		} else if (dm->dynamic_tx_high_power_lvl == tx_high_pwr_level_normal) {
			odm_set_mac_reg(dm, R_0x6d8, BIT(20) | BIT19 | BIT18, 0); /* Resp TXAGC offset = 0dB*/
			odm_set_bb_reg(dm, R_0xc24, MASKDWORD, dm->tx_agc_ofdm_18_6);
			PHYDM_DBG(dm, DBG_DYN_TXPWR, "Set TX power: normal\n");
		}
	}

#endif
#endif
}

void odm_dynamic_tx_power(void *dm_void)
{
	/*  */
	/* For AP/ADSL use struct rtl8192cd_priv* */
	/* For CE/NIC use struct void* */
	/*  */
#if 0
	struct void*		adapter = dm->adapter;
	struct rtl8192cd_priv*	priv		= dm->priv;
#endif
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;
	/*  */
	/* 2011/09/29 MH In HW integration first stage, we provide 4 different handle to operate */
	/* at the same time. In the stage2/3, we need to prive universal interface and merge all */
	/* HW dynamic mechanism. */
	/*  */
	switch (dm->support_platform) {
	case ODM_WIN:
		odm_dynamic_tx_power_nic(dm);
		break;
	case ODM_CE:
		odm_dynamic_tx_power_nic_ce(dm);
		break;
	default:
		break;
	}
}

void odm_dynamic_tx_power_nic(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (RTL8814A_SUPPORT == 1)
	if (dm->support_ic_type == ODM_RTL8814A)
		odm_dynamic_tx_power_8814a(dm);
#endif

#if (RTL8821A_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8821) {
		void *adapter = dm->adapter;
		PMGNT_INFO mgnt_info = GetDefaultMgntInfo((PADAPTER)adapter);

		if (mgnt_info->RegRspPwr == 1) {
			if (dm->rssi_min > 60)
				odm_set_mac_reg(dm, ODM_REG_RESP_TX_11AC, BIT(20) | BIT19 | BIT18, 1); /*Resp TXAGC offset = -3dB*/
			else if (dm->rssi_min < 55)
				odm_set_mac_reg(dm, ODM_REG_RESP_TX_11AC, BIT(20) | BIT19 | BIT18, 0); /*Resp TXAGC offset = 0dB*/
		}
	}
#endif
#endif
}

void odm_dynamic_tx_power_8821(void *dm_void, u8 *desc, u8 mac_id)
{
#if (RTL8821A_SUPPORT == 1)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct cmn_sta_info *entry;
	u8 reg0xc56_byte;
	u8 txpwr_offset = 0;

	entry = dm->phydm_sta_info[mac_id];

	reg0xc56_byte = odm_read_1byte(dm, 0xc56);

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "reg0xc56_byte=%d\n", reg0xc56_byte);

	if (entry[mac_id].rssi_stat.rssi > 85) {
		/* Avoid TXAGC error after TX power offset is applied.
		For example: Reg0xc56=0x6, if txpwr_offset=3( reduce 11dB )
		Total power = 6-11= -5( overflow!! ), PA may be burned !
		so txpwr_offset should be adjusted by Reg0xc56*/

		if (reg0xc56_byte < 7)
			txpwr_offset = 1;
		else if (reg0xc56_byte < 11)
			txpwr_offset = 2;
		else
			txpwr_offset = 3;

		SET_TX_DESC_TX_POWER_OFFSET_8812(desc, txpwr_offset);
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: RSSI=%d, txpwr_offset=%d\n",
			  __func__, entry[mac_id].rssi_stat.rssi, txpwr_offset);

	} else {
		SET_TX_DESC_TX_POWER_OFFSET_8812(desc, txpwr_offset);
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: RSSI=%d, txpwr_offset=%d\n",
			  __func__, entry[mac_id].rssi_stat.rssi, txpwr_offset);
	}
#endif /*#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)*/
#endif /*#if (RTL8821A_SUPPORT==1)*/
}

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
void odm_dynamic_tx_power_8814a(
	void *dm_void)
{
#if (RTL8814A_SUPPORT == 1)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	void *adapter = dm->adapter;
	PMGNT_INFO mgnt_info = &((PADAPTER)adapter)->MgntInfo;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	s32 undecorated_smoothed_pwdb = dm->rssi_min;

	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "TxLevel=%d mgnt_info->iot_action=%x mgnt_info->is_dynamic_tx_power_enable=%d\n",
		  hal_data->DynamicTxHighPowerLvl, mgnt_info->IOTAction,
		  mgnt_info->bDynamicTxPowerEnable);

	/*STA not connected and AP not connected*/
	if (!mgnt_info->bMediaConnect && hal_data->EntryMinUndecoratedSmoothedPWDB == 0) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR,
			  "Not connected to any reset power lvl\n");
		hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
		return;
	}

	if (!mgnt_info->bDynamicTxPowerEnable || mgnt_info->IOTAction & HT_IOT_ACT_DISABLE_HIGH_POWER)
		hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
	else {
		/*Should we separate as 2.4G/5G band?*/
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "rssi_tmp = %d\n",
			  undecorated_smoothed_pwdb);

		if (undecorated_smoothed_pwdb >= TX_POWER_NEAR_FIELD_THRESH_LVL2) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_level2;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "tx_high_pwr_level_level1 (TxPwr=0x0)\n");
		} else if ((undecorated_smoothed_pwdb < (TX_POWER_NEAR_FIELD_THRESH_LVL2 - 3)) &&
			   (undecorated_smoothed_pwdb >= TX_POWER_NEAR_FIELD_THRESH_LVL1)) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_level1;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "tx_high_pwr_level_level1 (TxPwr=0x10)\n");
		} else if (undecorated_smoothed_pwdb < (TX_POWER_NEAR_FIELD_THRESH_LVL1 - 5)) {
			hal_data->DynamicTxHighPowerLvl = tx_high_pwr_level_normal;
			PHYDM_DBG(dm, DBG_DYN_TXPWR,
				  "tx_high_pwr_level_normal\n");
		}
	}

	if (hal_data->DynamicTxHighPowerLvl != hal_data->LastDTPLvl) {
		PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: channel = %d\n", __func__,
			  hal_data->CurrentChannel);
		odm_set_tx_power_level8814(adapter, hal_data->CurrentChannel, hal_data->DynamicTxHighPowerLvl);
	}

	PHYDM_DBG(dm, DBG_DYN_TXPWR, "%s: channel = %d  TXpower lvl=%d/%d\n",
		  __func__, hal_data->CurrentChannel, hal_data->LastDTPLvl,
		  hal_data->DynamicTxHighPowerLvl);

	hal_data->LastDTPLvl = hal_data->DynamicTxHighPowerLvl;
#endif
}

/**/
/*For normal driver we always use the FW method to configure TX power index to reduce I/O transaction.*/
/**/
/**/
void odm_set_tx_power_level8814(
	void *adapter,
	u8 channel,
	u8 pwr_lvl)
{
#if (RTL8814A_SUPPORT == 1)
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

				/*for Query bus type to recude tx power.*/
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
#endif
}
#endif

#endif /* #ifdef CONFIG_DYNAMIC_TX_TWR */

void phydm_dtp_debug(void *dm_void, char input[][16], u32 *_used, char *output,
		     u32 *_out_len, u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;
#ifdef CONFIG_DYNAMIC_TX_TWR
	char help[] = "-h";
	u32 var1[3] = {0};
	u8 set_pwr_th1, set_pwr_th2, set_pwr_th3;
	u8 i;

	if ((strcmp(input[1], help) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "{DynTxPwr} {TH1 TH2 TH3} \n");

	} else {
		for (i = 0; i < 3; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);
			}
		}
		for (i = 0; i < 3; i++)
			if (var1[i] == 0 || var1[i] > 100)
				dm->set_pwr_th[i] = 0xff;
			else
				dm->set_pwr_th[i] = (u8)var1[i];
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Phydm Set DTP : TH1 = (( 0x%x)), TH2 = (( 0x%x)), TH3 = (( 0x%x))\n",
			 dm->set_pwr_th[0], dm->set_pwr_th[1],
			 dm->set_pwr_th[2]);
	}
#else
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "Dynamic TxPwr not support\n");
#endif
	*_used = used;
	*_out_len = out_len;
}

void phydm_dynamic_tx_power(void *dm_void)
{
#ifdef CONFIG_DYNAMIC_TX_TWR
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct cmn_sta_info *sta = NULL;
	u8 i;
	u8 cnt = 0;
	u8 rssi_min = dm->rssi_min;
	u8 rssi_tmp;
	if (!(dm->support_ability & ODM_BB_DYNAMIC_TXPWR))
		return;
	PHYDM_DBG(dm, DBG_DYN_TXPWR,
		  "Dynamic TxPwr : RSSI_min = %d, Noisy_dec = %d\n", rssi_min,
		  dm->noisy_decision);
	phydm_noisy_enhance_hp_th(dm, dm->noisy_decision);
	/* Response Power */
	dm->dynamic_tx_high_power_lvl = phydm_pwr_lvl_check(dm, rssi_min);
	phydm_dynamic_response_power(dm);
	/* Per STA Tx power */
	for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
		phydm_dtp_per_sta(dm, i);
		cnt++;
		if (cnt >= dm->number_linked_client)
			break;
	}
#endif
}
