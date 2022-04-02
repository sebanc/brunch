// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2014 - 2017 Realtek Corporation */


#include <drv_types.h>
#include <hal_data.h>

/* A mapping from HalData to ODM. */
void rtw_hal_update_iqk_fw_offload_cap(struct adapter *adapter)
{
	struct hal_com_data * hal = GET_HAL_DATA(adapter);
	struct PHY_DM_STRUCT *p_dm_odm = adapter_to_phydm(adapter);

	if (hal->RegIQKFWOffload) {
		rtw_sctx_init(&hal->iqk_sctx, 0);
		phydm_fwoffload_ability_init(p_dm_odm, PHYDM_RF_IQK_OFFLOAD);
	} else
		phydm_fwoffload_ability_clear(p_dm_odm, PHYDM_RF_IQK_OFFLOAD);

	RTW_INFO("IQK FW offload:%s\n", hal->RegIQKFWOffload ? "enable" : "disable");
}

#ifdef CONFIG_DBG_RF_CAL
void rtw_hal_iqk_test(struct adapter *adapter, bool recovery, bool clear, bool segment)
{
	struct PHY_DM_STRUCT *p_dm_odm = adapter_to_phydm(adapter);

	rtw_ps_deny(adapter, PS_DENY_IOCTL);
	LeaveAllPowerSaveModeDirect(adapter);

	rtw_phydm_ability_backup(adapter);
	rtw_phydm_func_disable_all(adapter);

	halrf_cmn_info_set(p_dm_odm, HALRF_CMNINFO_ABILITY, HAL_RF_IQK);

	rtw_phydm_iqk_trigger_dbg(adapter, recovery, clear, segment);
	rtw_phydm_ability_restore(adapter);

	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);
}

void rtw_hal_lck_test(struct adapter *adapter)
{
	struct PHY_DM_STRUCT *p_dm_odm = adapter_to_phydm(adapter);

	rtw_ps_deny(adapter, PS_DENY_IOCTL);
	LeaveAllPowerSaveModeDirect(adapter);

	rtw_phydm_ability_backup(adapter);
	rtw_phydm_func_disable_all(adapter);

	halrf_cmn_info_set(p_dm_odm, HALRF_CMNINFO_ABILITY, HAL_RF_LCK);

	rtw_phydm_lck_trigger(adapter);

	rtw_phydm_ability_restore(adapter);
	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);
}
#endif

#ifdef CONFIG_FW_OFFLOAD_PARAM_INIT
void rtw_hal_update_param_init_fw_offload_cap(struct adapter *adapter)
{
	struct PHY_DM_STRUCT *p_dm_odm = adapter_to_phydm(adapter);

	if (adapter->registrypriv.fw_param_init)
		phydm_fwoffload_ability_init(p_dm_odm, PHYDM_PHY_PARAM_OFFLOAD);
	else
		phydm_fwoffload_ability_clear(p_dm_odm, PHYDM_PHY_PARAM_OFFLOAD);

	RTW_INFO("Init-Parameter FW offload:%s\n", adapter->registrypriv.fw_param_init ? "enable" : "disable");
}
#endif

static void record_ra_info(void *p_dm_void, u8 macid, struct cmn_sta_info *p_sta, u64 ra_mask)
{
	struct PHY_DM_STRUCT *p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adapter *adapter = p_dm->adapter;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);

	rtw_macid_ctl_set_bw(macid_ctl, macid, p_sta->ra_info.ra_bw_mode);
	rtw_macid_ctl_set_vht_en(macid_ctl, macid, p_sta->ra_info.is_vht_enable);
	rtw_macid_ctl_set_rate_bmp0(macid_ctl, macid, ra_mask);
	rtw_macid_ctl_set_rate_bmp1(macid_ctl, macid, ra_mask >> 32);

	rtw_update_tx_rate_bmp(adapter_to_dvobj(adapter));
}

static void rtw_phydm_ops_func_init(struct PHY_DM_STRUCT *p_phydm)
{
	struct _rate_adaptive_table_ *p_ra_t = &p_phydm->dm_ra_table;

	p_ra_t->record_ra_info = record_ra_info;
}

void Init_ODM_ComInfo(struct adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapter);
	struct PHY_DM_STRUCT		*pDM_Odm = &(pHalData->odmpriv);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapter);
	int i;

	memset(pDM_Odm, 0, sizeof(*pDM_Odm));

	pDM_Odm->adapter = adapter;

	/*phydm_op_mode could be change for different scenarios: ex: SoftAP - PHYDM_BALANCE_MODE*/
	pHalData->phydm_op_mode = PHYDM_PERFORMANCE_MODE;/*Service one device*/

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_PLATFORM, ODM_CE);

	rtw_odm_init_ic_type(adapter);

	if (rtw_get_intf_type(adapter) == RTW_GSPI)
		odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_INTERFACE, ODM_ITRF_SDIO);
	else
		odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_INTERFACE, rtw_get_intf_type(adapter));

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_MP_TEST_CHIP, IS_NORMAL_CHIP(pHalData->version_id));

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_PATCH_ID, pHalData->CustomerID);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_BWIFI_TEST, adapter->registrypriv.wifi_spec);

#ifdef CONFIG_ADVANCE_OTA
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_ADVANCE_OTA, adapter->registrypriv.adv_ota);
#endif
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_RF_TYPE, pHalData->rf_type);

	{
		/* 1 ======= BoardType: ODM_CMNINFO_BOARD_TYPE ======= */
		u8 odm_board_type = ODM_BOARD_DEFAULT;

		if (pHalData->ExternalLNA_2G != 0) {
			odm_board_type |= ODM_BOARD_EXT_LNA;
			odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_EXT_LNA, 1);
		}
		if (pHalData->external_lna_5g != 0) {
			odm_board_type |= ODM_BOARD_EXT_LNA_5G;
			odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_5G_EXT_LNA, 1);
		}
		if (pHalData->ExternalPA_2G != 0) {
			odm_board_type |= ODM_BOARD_EXT_PA;
			odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_EXT_PA, 1);
		}
		if (pHalData->external_pa_5g != 0) {
			odm_board_type |= ODM_BOARD_EXT_PA_5G;
			odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_5G_EXT_PA, 1);
		}
		if (pHalData->EEPROMBluetoothCoexist)
			odm_board_type |= ODM_BOARD_BT;

		odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_BOARD_TYPE, odm_board_type);
		/* 1 ============== End of BoardType ============== */
	}

	rtw_hal_set_odm_var(adapter, HAL_ODM_REGULATION, NULL, true);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_GPA, pHalData->TypeGPA);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_APA, pHalData->TypeAPA);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_GLNA, pHalData->TypeGLNA);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_ALNA, pHalData->TypeALNA);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_RFE_TYPE, pHalData->rfe_type);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_EXT_TRSW, 0);

	/*Add by YuChen for kfree init*/
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_REGRFKFREEENABLE, adapter->registrypriv.RegPwrTrimEnable);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_RFKFREEENABLE, pHalData->RfKFreeEnable);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_RF_ANTENNA_TYPE, pHalData->TRxAntDivType);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_BE_FIX_TX_ANT, pHalData->b_fix_tx_ant);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_WITH_EXT_ANTENNA_SWITCH, pHalData->with_extenal_ant_switch);

	/* (8822B) efuse 0x3D7 & 0x3D8 for TX PA bias */
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_EFUSE0X3D7, pHalData->efuse0x3d7);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_EFUSE0X3D8, pHalData->efuse0x3d8);

	/*Add by YuChen for adaptivity init*/
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_ADAPTIVITY, &(adapter->registrypriv.adaptivity_en));
	phydm_adaptivity_info_init(pDM_Odm, PHYDM_ADAPINFO_CARRIER_SENSE_ENABLE, (adapter->registrypriv.adaptivity_mode != 0) ? true : false);
	phydm_adaptivity_info_init(pDM_Odm, PHYDM_ADAPINFO_DCBACKOFF, adapter->registrypriv.adaptivity_dc_backoff);
	phydm_adaptivity_info_init(pDM_Odm, PHYDM_ADAPINFO_DYNAMICLINKADAPTIVITY, (adapter->registrypriv.adaptivity_dml != 0) ? true : false);
	phydm_adaptivity_info_init(pDM_Odm, PHYDM_ADAPINFO_TH_L2H_INI, adapter->registrypriv.adaptivity_th_l2h_ini);
	phydm_adaptivity_info_init(pDM_Odm, PHYDM_ADAPINFO_TH_EDCCA_HL_DIFF, adapter->registrypriv.adaptivity_th_edcca_hl_diff);

	/*halrf info init*/
	halrf_cmn_info_init(pDM_Odm, HALRF_CMNINFO_EEPROM_THERMAL_VALUE, pHalData->eeprom_thermal_meter);
	halrf_cmn_info_init(pDM_Odm, HALRF_CMNINFO_FW_VER,
		((pHalData->firmware_version << 16) | pHalData->firmware_sub_version));

	if (rtw_odm_adaptivity_needed(adapter))
		rtw_odm_adaptivity_config_msg(RTW_DBGDUMP, adapter);

#ifdef CONFIG_IQK_PA_OFF
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_IQKPAOFF, 1);
#endif
	rtw_hal_update_iqk_fw_offload_cap(adapter);
	#ifdef CONFIG_FW_OFFLOAD_PARAM_INIT
	rtw_hal_update_param_init_fw_offload_cap(adapter);
	#endif

	/* Pointer reference */
	/*Antenna diversity relative parameters*/
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_ANT_DIV, &(pHalData->AntDivCfg));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_MP_MODE, &(adapter->registrypriv.mp_mode));

	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_BB_OPERATION_MODE, &(pHalData->phydm_op_mode));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_TX_UNI, &(dvobj->traffic_stat.tx_bytes));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_RX_UNI, &(dvobj->traffic_stat.rx_bytes));

	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_BAND, &(pHalData->current_band_type));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_FORCED_RATE, &(pHalData->ForcedDataRate));

	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_SEC_CHNL_OFFSET, &(pHalData->nCur40MhzPrimeSC));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_SEC_MODE, &(adapter->securitypriv.dot11PrivacyAlgrthm));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_BW, &(pHalData->current_channel_bw));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_CHNL, &(pHalData->current_channel));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_NET_CLOSED, &(adapter->net_closed));

	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_SCAN, &(pHalData->bScanInProcess));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_POWER_SAVING, &(pwrctl->bpower_saving));
	/*Add by Yuchen for phydm beamforming*/
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_TX_TP, &(dvobj->traffic_stat.cur_tx_tp));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_RX_TP, &(dvobj->traffic_stat.cur_rx_tp));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_ANT_TEST, &(pHalData->antenna_test));
	odm_cmn_info_hook(pDM_Odm, ODM_CMNINFO_HUBUSBMODE, &(dvobj->usb_speed));

	/*halrf info hook*/
	for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++)
		odm_cmn_info_ptr_array_hook(pDM_Odm, ODM_CMNINFO_STA_STATUS, i, NULL);

	phydm_init_debug_setting(pDM_Odm);
	rtw_phydm_ops_func_init(pDM_Odm);
}

static u32 edca_setting_UL[HT_IOT_PEER_MAX] =
/*UNKNOWN, REALTEK_90, REALTEK_92SE, BROADCOM,*/
/*RALINK, ATHEROS, CISCO, MERU, MARVELL, 92U_AP, SELF_AP(DownLink/Tx) */
{ 0x5e4322, 0xa44f, 0x5e4322, 0x5ea32b, 0x5ea422, 0x5ea322, 0x3ea430, 0x5ea42b, 0x5ea44f, 0x5e4322, 0x5e4322};

static u32 edca_setting_DL[HT_IOT_PEER_MAX] =
/*UNKNOWN, REALTEK_90, REALTEK_92SE, BROADCOM,*/
/*RALINK, ATHEROS, CISCO, MERU, MARVELL, 92U_AP, SELF_AP(UpLink/Rx)*/
{ 0xa44f, 0x5ea44f,	 0x5e4322, 0x5ea42b, 0xa44f, 0xa630, 0x5ea630, 0x5ea42b, 0xa44f, 0xa42b, 0xa42b};

static u32 edca_setting_dl_g_mode[HT_IOT_PEER_MAX] =
/*UNKNOWN, REALTEK_90, REALTEK_92SE, BROADCOM,*/
/*RALINK, ATHEROS, CISCO, MERU, MARVELL, 92U_AP, SELF_AP */
{ 0x4322, 0xa44f, 0x5e4322, 0xa42b, 0x5e4322, 0x4322,	 0xa42b, 0x5ea42b, 0xa44f, 0x5e4322, 0x5ea42b};

void rtw_hal_turbo_edca(struct adapter *adapter)
{
	struct hal_com_data		*hal_data = GET_HAL_DATA(adapter);
	struct dvobj_priv		*dvobj = adapter_to_dvobj(adapter);
	struct recv_priv		*precvpriv = &(adapter->recvpriv);
	struct registry_priv		*pregpriv = &adapter->registrypriv;
	struct mlme_ext_priv	*pmlmeext = &(adapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	/* Parameter suggested by Scott  */
	u32	EDCA_BE_UL = 0x5ea42b;
	u32	EDCA_BE_DL = 0x00a42b;
	u8	ic_type = rtw_get_chip_type(adapter);

	u8	iot_peer = 0;
	u8	wireless_mode = 0xFF;                 /* invalid value */
	u8	traffic_index;
	u32	edca_param;
	u64	cur_tx_bytes = 0;
	u64	cur_rx_bytes = 0;
	u8	bbtchange = true;
	u8	is_bias_on_rx = false;
	u8	is_linked = false;
	u8	interface_type;

	if (hal_data->dis_turboedca)
		return;

	if (rtw_mi_check_status(adapter, MI_ASSOC))
		is_linked = true;

	if (!is_linked) {
		precvpriv->is_any_non_be_pkts = false;
		return;
	}

	if ((pregpriv->wifi_spec == 1)) { /* || (pmlmeinfo->HT_enable == 0)) */
		precvpriv->is_any_non_be_pkts = false;
		return;
	}

	interface_type = rtw_get_intf_type(adapter);
	wireless_mode = pmlmeext->cur_wireless_mode;

	iot_peer = pmlmeinfo->assoc_AP_vendor;

	if (iot_peer >=  HT_IOT_PEER_MAX) {
		precvpriv->is_any_non_be_pkts = false;
		return;
	}

	if (ic_type == RTL8188E) {
		if ((iot_peer == HT_IOT_PEER_RALINK) || (iot_peer == HT_IOT_PEER_ATHEROS))
			is_bias_on_rx = true;
	}

	/* Check if the status needs to be changed. */
	if ((bbtchange) || (!precvpriv->is_any_non_be_pkts)) {
		cur_tx_bytes = dvobj->traffic_stat.cur_tx_bytes;
		cur_rx_bytes = dvobj->traffic_stat.cur_rx_bytes;

		/* traffic, TX or RX */
		if (is_bias_on_rx) {
			if (cur_tx_bytes > (cur_rx_bytes << 2)) {
				/* Uplink TP is present. */
				traffic_index = UP_LINK;
			} else {
				/* Balance TP is present. */
				traffic_index = DOWN_LINK;
			}
		} else {
			if (cur_rx_bytes > (cur_tx_bytes << 2)) {
				/* Downlink TP is present. */
				traffic_index = DOWN_LINK;
			} else {
				/* Balance TP is present. */
				traffic_index = UP_LINK;
			}
		}
		if (interface_type == RTW_PCIE) {
			EDCA_BE_UL = 0x6ea42b;
			EDCA_BE_DL = 0x6ea42b;
		}

		/* 92D txop can't be set to 0x3e for cisco1250 */
		if ((iot_peer == HT_IOT_PEER_CISCO) && (wireless_mode == ODM_WM_N24G)) {
			EDCA_BE_DL = edca_setting_DL[iot_peer];
			EDCA_BE_UL = edca_setting_UL[iot_peer];
		}
		/* merge from 92s_92c_merge temp*/
		else if ((iot_peer == HT_IOT_PEER_CISCO) && ((wireless_mode == ODM_WM_G) || (wireless_mode == (ODM_WM_B | ODM_WM_G)) || (wireless_mode == ODM_WM_A) || (wireless_mode == ODM_WM_B)))
			EDCA_BE_DL = edca_setting_dl_g_mode[iot_peer];
		else if ((iot_peer == HT_IOT_PEER_AIRGO) && ((wireless_mode == ODM_WM_G) || (wireless_mode == ODM_WM_A)))
			EDCA_BE_DL = 0xa630;
		else if (iot_peer == HT_IOT_PEER_MARVELL) {
			EDCA_BE_DL = edca_setting_DL[iot_peer];
			EDCA_BE_UL = edca_setting_UL[iot_peer];
		} else if (iot_peer == HT_IOT_PEER_ATHEROS) {
			/* Set DL EDCA for Atheros peer to 0x3ea42b.*/
			/* Suggested by SD3 Wilson for ASUS TP issue.*/
			EDCA_BE_DL = edca_setting_DL[iot_peer];
		}

		if ((ic_type == RTL8812) || (ic_type == RTL8821) || (ic_type == RTL8192E)) { /* add 8812AU/8812AE */
			EDCA_BE_UL = 0x5ea42b;
			EDCA_BE_DL = 0x5ea42b;

			RTW_DBG("8812A: EDCA_BE_UL=0x%x EDCA_BE_DL =0x%x\n", EDCA_BE_UL, EDCA_BE_DL);
		}

		if (interface_type == RTW_PCIE &&
			((ic_type == RTL8822B)
			|| (ic_type == RTL8814A))) {
			EDCA_BE_UL = 0x6ea42b;
			EDCA_BE_DL = 0x6ea42b;
		}

		if (traffic_index == DOWN_LINK)
			edca_param = EDCA_BE_DL;
		else
			edca_param = EDCA_BE_UL;
#ifdef 	CONFIG_RTW_CUSTOMIZE_BEEDCA
		edca_param = CONFIG_RTW_CUSTOMIZE_BEEDCA;
#endif
		rtw_hal_set_hwreg(adapter, HW_VAR_AC_PARAM_BE, (u8 *)(&edca_param));

		RTW_DBG("Turbo EDCA =0x%x\n", edca_param);

		hal_data->prv_traffic_idx = traffic_index;

		hal_data->is_turbo_edca = true;
	} else {
		/*  */
		/* Turn Off EDCA turbo here. */
		/* Restore original EDCA according to the declaration of AP. */
		/*  */
		if (hal_data->is_turbo_edca) {
			edca_param = hal_data->ac_param_be;
			rtw_hal_set_hwreg(adapter, HW_VAR_AC_PARAM_BE, (u8 *)(&edca_param));
			hal_data->is_turbo_edca = false;
		}
	}

}

s8 rtw_phydm_get_min_rssi(struct adapter *adapter)
{
	struct PHY_DM_STRUCT *phydm = adapter_to_phydm(adapter);
	s8 rssi_min = 0;

	rssi_min = phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_RSSI_MIN);
	return rssi_min;
}

u8 rtw_phydm_get_cur_igi(struct adapter *adapter)
{
	struct PHY_DM_STRUCT *phydm = adapter_to_phydm(adapter);
	u8 cur_igi = 0;

	cur_igi = phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CURR_IGI);
	return cur_igi;
}

u32 rtw_phydm_get_phy_cnt(struct adapter *adapter, enum phy_cnt cnt)
{
	struct PHY_DM_STRUCT *phydm = adapter_to_phydm(adapter);

	if (cnt == FA_OFDM)
		return  phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_FA_OFDM);
	else if (cnt == FA_CCK)
		return  phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_FA_CCK);
	else if (cnt == FA_TOTAL)
		return  phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_FA_TOTAL);
	else if (cnt == CCA_OFDM)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CCA_OFDM);
	else if (cnt == CCA_CCK)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CCA_CCK);
	else if (cnt == CCA_ALL)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CCA_ALL);
	else if (cnt == CRC32_OK_VHT)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_OK_VHT);
	else if (cnt == CRC32_OK_HT)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_OK_HT);
	else if (cnt == CRC32_OK_LEGACY)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_OK_LEGACY);
	else if (cnt == CRC32_OK_CCK)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_OK_CCK);
	else if (cnt == CRC32_ERROR_VHT)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_ERROR_VHT);
	else if (cnt == CRC32_ERROR_HT)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_ERROR_HT);
	else if (cnt == CRC32_ERROR_LEGACY)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_ERROR_LEGACY);
	else if (cnt == CRC32_ERROR_CCK)
		return	phydm_cmn_info_query(phydm, (enum phydm_info_query_e) PHYDM_INFO_CRC32_ERROR_CCK);
	else
		return 0;
}

u8 rtw_phydm_is_iqk_in_progress(struct adapter *adapter)
{
	u8 rts = false;
	struct PHY_DM_STRUCT *podmpriv = adapter_to_phydm(adapter);

	odm_acquire_spin_lock(podmpriv, RT_IQK_SPINLOCK);
	if (podmpriv->rf_calibrate_info.is_iqk_in_progress) {
		RTW_ERR("IQK InProgress\n");
		rts = true;
	}
	odm_release_spin_lock(podmpriv, RT_IQK_SPINLOCK);

	return rts;
}

void SetHalODMVar(
	struct adapter *				Adapter,
	enum hal_odm_variable		eVariable,
	void *					pValue1,
	bool					bSet)
{
	struct PHY_DM_STRUCT *podmpriv = adapter_to_phydm(Adapter);
	/* unsigned long irqL; */
	switch (eVariable) {
	case HAL_ODM_STA_INFO: {
		struct sta_info *psta = (struct sta_info *)pValue1;

		if (bSet) {
			RTW_INFO("### Set STA_(%d) info ###\n", psta->cmn.mac_id);
			odm_cmn_info_ptr_array_hook(podmpriv, ODM_CMNINFO_STA_STATUS, psta->cmn.mac_id, psta);
			psta->cmn.dm_ctrl = STA_DM_CTRL_ACTIVE;
			phydm_cmn_sta_info_hook(podmpriv, psta->cmn.mac_id, &(psta->cmn));
		} else {
			RTW_INFO("### Clean STA_(%d) info ###\n", psta->cmn.mac_id);
			/* _enter_critical_bh(&pHalData->odm_stainfo_lock, &irqL); */
			psta->cmn.dm_ctrl = 0;
			odm_cmn_info_ptr_array_hook(podmpriv, ODM_CMNINFO_STA_STATUS, psta->cmn.mac_id, NULL);
			phydm_cmn_sta_info_hook(podmpriv, psta->cmn.mac_id, NULL);

			/* _exit_critical_bh(&pHalData->odm_stainfo_lock, &irqL); */
		}
	}
		break;
	case HAL_ODM_P2P_STATE:
		odm_cmn_info_update(podmpriv, ODM_CMNINFO_WIFI_DIRECT, bSet);
		break;
	case HAL_ODM_WIFI_DISPLAY_STATE:
		odm_cmn_info_update(podmpriv, ODM_CMNINFO_WIFI_DISPLAY, bSet);
		break;
	case HAL_ODM_REGULATION:
		/* used to auto enable/disable adaptivity by SD7 */
		odm_cmn_info_init(podmpriv, ODM_CMNINFO_DOMAIN_CODE_2G, 0);
		odm_cmn_info_init(podmpriv, ODM_CMNINFO_DOMAIN_CODE_5G, 0);
		break;
	case HAL_ODM_INITIAL_GAIN: {
		u8 rx_gain = *((u8 *)(pValue1));
		/*printk("rx_gain:%x\n",rx_gain);*/
		if (rx_gain == 0xff) {/*restore rx gain*/
			/*odm_write_dig(podmpriv,pDigTable->backup_ig_value);*/
			odm_pause_dig(podmpriv, PHYDM_RESUME, PHYDM_PAUSE_LEVEL_0, rx_gain);
		} else {
			/*pDigTable->backup_ig_value = pDigTable->cur_ig_value;*/
			/*odm_write_dig(podmpriv,rx_gain);*/
			odm_pause_dig(podmpriv, PHYDM_PAUSE, PHYDM_PAUSE_LEVEL_0, rx_gain);
		}
	}
	break;
	case HAL_ODM_RX_INFO_DUMP: {
		u8 cur_igi = 0;
		s8 rssi_min;
		void *sel;

		sel = pValue1;
		cur_igi = rtw_phydm_get_cur_igi(Adapter);
		rssi_min = rtw_phydm_get_min_rssi(Adapter);

		_RTW_PRINT_SEL(sel, "============ Rx Info dump ===================\n");
		_RTW_PRINT_SEL(sel, "is_linked = %d, rssi_min = %d(%%), current_igi = 0x%x\n", podmpriv->is_linked, rssi_min, cur_igi);
		_RTW_PRINT_SEL(sel, "cnt_cck_fail = %d, cnt_ofdm_fail = %d, Total False Alarm = %d\n",
			rtw_phydm_get_phy_cnt(Adapter, FA_CCK),
			rtw_phydm_get_phy_cnt(Adapter, FA_OFDM),
			rtw_phydm_get_phy_cnt(Adapter, FA_TOTAL));

		if (podmpriv->is_linked) {
			_RTW_PRINT_SEL(sel, "rx_rate = %s", HDATA_RATE(podmpriv->rx_rate));
			_RTW_PRINT_SEL(sel, " RSSI_A = %d(%%), RSSI_B = %d(%%)\n", podmpriv->RSSI_A, podmpriv->RSSI_B);
		}
	}
		break;
	case HAL_ODM_RX_Dframe_INFO: {
		void *sel;

		sel = pValue1;
	}
		break;
	default:
		break;
	}
}

void GetHalODMVar(
	struct adapter *				Adapter,
	enum hal_odm_variable		eVariable,
	void *					pValue1,
	void *					pValue2)
{
	switch (eVariable) {
	case HAL_ODM_INITIAL_GAIN:
		*((u8 *)pValue1) = rtw_phydm_get_cur_igi(Adapter);
		break;
	default:
		break;
	}
}

enum hal_status
rtw_phydm_fw_iqk(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 clear,
	u8 segment
)
{
	return HAL_STATUS_FAILURE;
}

enum hal_status
rtw_phydm_cfg_phy_para(
	struct PHY_DM_STRUCT	*p_dm_odm,
	enum phydm_halmac_param config_type,
	u32 offset,
	u32 data,
	u32 mask,
	enum rf_path e_rf_path,
	u32 delay_time)
{
	return HAL_STATUS_SUCCESS;
}

void dump_sta_traffic(void *sel, struct adapter *adapter, struct sta_info *psta)
{
	struct ra_sta_info *ra_info;
	u8 curr_sgi = false;

	if (!psta)
		return;
	RTW_PRINT_SEL(sel, "====== mac_id : %d ======\n", psta->cmn.mac_id);

	ra_info = &psta->cmn.ra_info;
	curr_sgi = (ra_info->curr_tx_rate & 0x80) ? true : false;
	RTW_PRINT_SEL(sel, "tx_rate : %s(%s)  rx_rate : %s, rx_rate_bmc : %s, rssi : %d %%\n"
		, HDATA_RATE((ra_info->curr_tx_rate & 0x7F)), (curr_sgi) ? "S" : "L"
		, HDATA_RATE((psta->curr_rx_rate & 0x7F)), HDATA_RATE((psta->curr_rx_rate_bmc & 0x7F)), psta->cmn.rssi_stat.rssi
	);
	RTW_PRINT_SEL(sel, "TP {Tx,Rx,Total} = { %d , %d , %d } Mbps\n",
		(psta->sta_stats.tx_tp_mbytes << 3), (psta->sta_stats.rx_tp_mbytes << 3),
		(psta->sta_stats.tx_tp_mbytes + psta->sta_stats.rx_tp_mbytes) << 3);

	RTW_PRINT_SEL(sel, "Moving-AVG TP {Tx,Rx,Total} = { %d , %d , %d } Mbps\n\n",
		(psta->cmn.tx_moving_average_tp << 3), (psta->cmn.rx_moving_average_tp << 3),
		(psta->cmn.tx_moving_average_tp + psta->cmn.rx_moving_average_tp) << 3);
}

void dump_sta_info(void *sel, struct sta_info *psta)
{
	struct ra_sta_info *ra_info;
	u8 curr_tx_sgi = false;
	u8 curr_tx_rate = 0;

	if (!psta)
		return;

	ra_info = &psta->cmn.ra_info;

	RTW_PRINT_SEL(sel, "============ STA [" MAC_FMT "]  ===================\n",
		MAC_ARG(psta->cmn.mac_addr));
	RTW_PRINT_SEL(sel, "mac_id : %d\n", psta->cmn.mac_id);
	RTW_PRINT_SEL(sel, "wireless_mode : 0x%02x\n", psta->wireless_mode);
	RTW_PRINT_SEL(sel, "mimo_type : %d\n", psta->cmn.mimo_type);
	RTW_PRINT_SEL(sel, "bw_mode : %s, ra_bw_mode : %s\n",
			ch_width_str(psta->cmn.bw_mode), ch_width_str(ra_info->ra_bw_mode));
	RTW_PRINT_SEL(sel, "rate_id : %d\n", ra_info->rate_id);
	RTW_PRINT_SEL(sel, "rssi : %d (%%), rssi_level : %d\n", psta->cmn.rssi_stat.rssi, ra_info->rssi_level);
	RTW_PRINT_SEL(sel, "is_support_sgi : %s, is_vht_enable : %s\n",
			(ra_info->is_support_sgi) ? "Y" : "N", (ra_info->is_vht_enable) ? "Y" : "N");
	RTW_PRINT_SEL(sel, "disable_ra : %s, disable_pt : %s\n",
				(ra_info->disable_ra) ? "Y" : "N", (ra_info->disable_pt) ? "Y" : "N");
	RTW_PRINT_SEL(sel, "is_noisy : %s\n", (ra_info->is_noisy) ? "Y" : "N");
	RTW_PRINT_SEL(sel, "txrx_state : %d\n", ra_info->txrx_state);/*0: uplink, 1:downlink, 2:bi-direction*/

	curr_tx_sgi = (ra_info->curr_tx_rate & 0x80) ? true : false;
	curr_tx_rate = ra_info->curr_tx_rate & 0x7F;
	RTW_PRINT_SEL(sel, "curr_tx_rate : %s (%s)\n",
			HDATA_RATE(curr_tx_rate), (curr_tx_sgi) ? "S" : "L");
	RTW_PRINT_SEL(sel, "curr_tx_bw : %s\n", ch_width_str(ra_info->curr_tx_bw));
	RTW_PRINT_SEL(sel, "curr_retry_ratio : %d\n", ra_info->curr_retry_ratio);
	RTW_PRINT_SEL(sel, "ra_mask : 0x%016llx\n\n", ra_info->ramask);
}

void rtw_phydm_ra_registed(struct adapter *adapter, struct sta_info *psta)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);

	if (!psta) {
		RTW_ERR(FUNC_ADPT_FMT" sta is NULL\n", FUNC_ADPT_ARG(adapter));
		rtw_warn_on(1);
		return;
	}

	phydm_ra_registed(&hal_data->odmpriv, psta->cmn.mac_id, psta->cmn.rssi_stat.rssi);
	if (_DRV_DEBUG_ <= rtw_drv_log_level)
		dump_sta_info(RTW_DBGDUMP, psta);
}

static u8 _rtw_phydm_rfk_condition_check(struct adapter *adapter)
{
	u8 rst = false;

	if (rtw_mi_stayin_union_ch_chk(adapter))
		rst = true;

	return rst;
}

/*check the tx low rate while unlinked to any AP;for pwr tracking */
static u8 _rtw_phydm_pwr_tracking_rate_check(struct adapter *adapter)
{
	int i;
	struct adapter *iface;
	u8		if_tx_rate = 0xFF;
	u8		tx_rate = 0xFF;
	struct mlme_ext_priv	*pmlmeext = NULL;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		pmlmeext = &(iface->mlmeextpriv);
		if ((iface) && rtw_is_adapter_up(iface)) {
			if (!rtw_p2p_chk_role(&(iface)->wdinfo, P2P_ROLE_DISABLE))
				if_tx_rate = IEEE80211_OFDM_RATE_6MB;
			else
				if_tx_rate = pmlmeext->tx_rate;
			if(if_tx_rate < tx_rate)
				tx_rate = if_tx_rate;

			RTW_DBG("%s i=%d tx_rate =0x%x\n", __func__, i, if_tx_rate);
		}
	}
	RTW_DBG("%s tx_low_rate (unlinked to any AP)=0x%x\n", __func__, tx_rate);
	return tx_rate;
}

void rtw_phydm_watchdog(struct adapter *adapter)
{
	u8	bLinked = false;
	u8	bsta_state = false;
	u8	bBtDisabled = true;
	u8	rfk_forbidden = true;
	u8	tx_unlinked_low_rate = 0xFF;
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapter);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapter);

	if (!rtw_is_hw_init_completed(adapter)) {
		RTW_DBG("%s skip due to hw_init_completed == false\n", __func__);
		return;
	}
	if (rtw_mi_check_fwstate(adapter, _FW_UNDER_SURVEY))
		pHalData->bScanInProcess = true;
	else
		pHalData->bScanInProcess = false;

	if (rtw_mi_check_status(adapter, MI_ASSOC)) {
		bLinked = true;
		if (rtw_mi_check_status(adapter, MI_STA_LINKED))
		bsta_state = true;
	}

	odm_cmn_info_update(&pHalData->odmpriv, ODM_CMNINFO_LINK, bLinked);
	odm_cmn_info_update(&pHalData->odmpriv, ODM_CMNINFO_STATION_STATE, bsta_state);

	bBtDisabled = rtw_btcoex_IsBtDisabled(adapter);
	odm_cmn_info_update(&pHalData->odmpriv, ODM_CMNINFO_BT_ENABLED,
							(bBtDisabled) ? false : true);
	odm_cmn_info_update(&pHalData->odmpriv, ODM_CMNINFO_POWER_TRAINING,
							(pHalData->bDisableTXPowerTraining) ? true : false);
	if (bLinked) {
		rfk_forbidden = (_rtw_phydm_rfk_condition_check(adapter)) ? false : true;
		halrf_cmn_info_set(&pHalData->odmpriv, HALRF_CMNINFO_RFK_FORBIDDEN, rfk_forbidden);
	} else {
		tx_unlinked_low_rate = _rtw_phydm_pwr_tracking_rate_check(adapter);
		halrf_cmn_info_set(&pHalData->odmpriv, HALRF_CMNINFO_RATE_INDEX, tx_unlinked_low_rate);
	}
	if (pwrctl->bpower_saving)
		phydm_watchdog_lps(&pHalData->odmpriv);
	else
		phydm_watchdog(&pHalData->odmpriv);
	return;
}

