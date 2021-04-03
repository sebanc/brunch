// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#include <drv_types.h>
#include <hal_data.h>
#include <hal_btcoex.h>

void rtw_btcoex_Initialize(struct adapter * adapt)
{
	hal_btcoex_Initialize(adapt);
}

void rtw_btcoex_PowerOnSetting(struct adapter * adapt)
{
	hal_btcoex_PowerOnSetting(adapt);
}

void rtw_btcoex_AntInfoSetting(struct adapter * adapt)
{
	hal_btcoex_AntInfoSetting(adapt);
}

void rtw_btcoex_PowerOffSetting(struct adapter * adapt)
{
	hal_btcoex_PowerOffSetting(adapt);
}

void rtw_btcoex_PreLoadFirmware(struct adapter * adapt)
{
	hal_btcoex_PreLoadFirmware(adapt);
}

void rtw_btcoex_HAL_Initialize(struct adapter * adapt, u8 bWifiOnly)
{
	hal_btcoex_InitHwConfig(adapt, bWifiOnly);
}

void rtw_btcoex_IpsNotify(struct adapter * adapt, u8 type)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_IpsNotify(adapt, type);
}

void rtw_btcoex_LpsNotify(struct adapter * adapt, u8 type)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_LpsNotify(adapt, type);
}

void rtw_btcoex_ScanNotify(struct adapter * adapt, u8 type)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	if (false == type) {
		#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_buddy_check_fwstate(adapt, WIFI_SITE_MONITOR))
			return;
		#endif

		if (DEV_MGMT_TX_NUM(adapter_to_dvobj(adapt))
			|| DEV_ROCH_NUM(adapter_to_dvobj(adapt)))
			return;
	}
	hal_btcoex_ScanNotify(adapt, type);
}

void rtw_btcoex_ConnectNotify(struct adapter * adapt, u8 action)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

#ifdef CONFIG_CONCURRENT_MODE
	if (false == action) {
		if (rtw_mi_buddy_check_fwstate(adapt, WIFI_UNDER_LINKING))
			return;
	}
#endif

	hal_btcoex_ConnectNotify(adapt, action);
}

void rtw_btcoex_MediaStatusNotify(struct adapter * adapt, u8 mediaStatus)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

#ifdef CONFIG_CONCURRENT_MODE
	if (RT_MEDIA_DISCONNECT == mediaStatus) {
		if (rtw_mi_buddy_check_fwstate(adapt, WIFI_ASOC_STATE))
			return;
	}
#endif /* CONFIG_CONCURRENT_MODE */

	if ((RT_MEDIA_CONNECT == mediaStatus)
	    && (check_fwstate(&adapt->mlmepriv, WIFI_AP_STATE)))
		rtw_hal_set_hwreg(adapt, HW_VAR_DL_RSVD_PAGE, NULL);

	hal_btcoex_MediaStatusNotify(adapt, mediaStatus);
}

void rtw_btcoex_SpecialPacketNotify(struct adapter * adapt, u8 pktType)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_SpecialPacketNotify(adapt, pktType);
}

void rtw_btcoex_IQKNotify(struct adapter * adapt, u8 state)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_IQKNotify(adapt, state);
}

void rtw_btcoex_BtInfoNotify(struct adapter * adapt, u8 length, u8 *tmpBuf)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_BtInfoNotify(adapt, length, tmpBuf);
}

void rtw_btcoex_BtMpRptNotify(struct adapter * adapt, u8 length, u8 *tmpBuf)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	if (adapt->registrypriv.mp_mode == 1)
		return;

	hal_btcoex_BtMpRptNotify(adapt, length, tmpBuf);
}

void rtw_btcoex_SuspendNotify(struct adapter * adapt, u8 state)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_SuspendNotify(adapt, state);
}

void rtw_btcoex_HaltNotify(struct adapter * adapt)
{
	struct hal_com_data *	pHalData;
	u8 do_halt = 1;

	pHalData = GET_HAL_DATA(adapt);
	if (false == pHalData->EEPROMBluetoothCoexist)
		do_halt = 0;

	if (false == adapt->bup) {
		RTW_INFO(FUNC_ADPT_FMT ": bup=%d Skip!\n",
			 FUNC_ADPT_ARG(adapt), adapt->bup);
		do_halt = 0;
	}

	if (rtw_is_surprise_removed(adapt)) {
		RTW_INFO(FUNC_ADPT_FMT ": bSurpriseRemoved=%s Skip!\n",
			FUNC_ADPT_ARG(adapt), rtw_is_surprise_removed(adapt) ? "True" : "False");
		do_halt = 0;
	}

	hal_btcoex_HaltNotify(adapt, do_halt);
}

void rtw_btcoex_switchband_notify(u8 under_scan, u8 band_type)
{
	hal_btcoex_switchband_notify(under_scan, band_type);
}

void rtw_btcoex_WlFwDbgInfoNotify(struct adapter * adapt, u8* tmpBuf, u8 length)
{
	hal_btcoex_WlFwDbgInfoNotify(adapt, tmpBuf, length);
}

void rtw_btcoex_rx_rate_change_notify(struct adapter * adapt, u8 is_data_frame, u8 rate_id)
{
	hal_btcoex_rx_rate_change_notify(adapt, is_data_frame, rate_id);
}

void rtw_btcoex_SwitchBtTRxMask(struct adapter * adapt)
{
	hal_btcoex_SwitchBtTRxMask(adapt);
}

void rtw_btcoex_Switch(struct adapter * adapt, u8 enable)
{
	hal_btcoex_SetBTCoexist(adapt, enable);
}

u8 rtw_btcoex_IsBtDisabled(struct adapter * adapt)
{
	return hal_btcoex_IsBtDisabled(adapt);
}

void rtw_btcoex_Handler(struct adapter * adapt)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);

	if (false == pHalData->EEPROMBluetoothCoexist)
		return;

	hal_btcoex_Hanlder(adapt);
}

int rtw_btcoex_IsBTCoexRejectAMPDU(struct adapter * adapt)
{
	int coexctrl;

	coexctrl = hal_btcoex_IsBTCoexRejectAMPDU(adapt);

	return coexctrl;
}

int rtw_btcoex_IsBTCoexCtrlAMPDUSize(struct adapter * adapt)
{
	int coexctrl;

	coexctrl = hal_btcoex_IsBTCoexCtrlAMPDUSize(adapt);

	return coexctrl;
}

u32 rtw_btcoex_GetAMPDUSize(struct adapter * adapt)
{
	u32 size;

	size = hal_btcoex_GetAMPDUSize(adapt);

	return size;
}

void rtw_btcoex_SetManualControl(struct adapter * adapt, u8 manual)
{
	if (manual)
		hal_btcoex_SetManualControl(adapt, true);
	else
		hal_btcoex_SetManualControl(adapt, false);
}

u8 rtw_btcoex_1Ant(struct adapter * adapt)
{
	return hal_btcoex_1Ant(adapt);
}

u8 rtw_btcoex_IsBtControlLps(struct adapter * adapt)
{
	return hal_btcoex_IsBtControlLps(adapt);
}

u8 rtw_btcoex_IsLpsOn(struct adapter * adapt)
{
	return hal_btcoex_IsLpsOn(adapt);
}

u8 rtw_btcoex_RpwmVal(struct adapter * adapt)
{
	return hal_btcoex_RpwmVal(adapt);
}

u8 rtw_btcoex_LpsVal(struct adapter * adapt)
{
	return hal_btcoex_LpsVal(adapt);
}

u32 rtw_btcoex_GetRaMask(struct adapter * adapt)
{
	return hal_btcoex_GetRaMask(adapt);
}

void rtw_btcoex_RecordPwrMode(struct adapter * adapt, u8 *pCmdBuf, u8 cmdLen)
{
	hal_btcoex_RecordPwrMode(adapt, pCmdBuf, cmdLen);
}

void rtw_btcoex_DisplayBtCoexInfo(struct adapter * adapt, u8 *pbuf, u32 bufsize)
{
	hal_btcoex_DisplayBtCoexInfo(adapt, pbuf, bufsize);
}

void rtw_btcoex_SetDBG(struct adapter * adapt, u32 *pDbgModule)
{
	hal_btcoex_SetDBG(adapt, pDbgModule);
}

u32 rtw_btcoex_GetDBG(struct adapter * adapt, u8 *pStrBuf, u32 bufSize)
{
	return hal_btcoex_GetDBG(adapt, pStrBuf, bufSize);
}

u8 rtw_btcoex_IncreaseScanDeviceNum(struct adapter * adapt)
{
	return hal_btcoex_IncreaseScanDeviceNum(adapt);
}

u8 rtw_btcoex_IsBtLinkExist(struct adapter * adapt)
{
	return hal_btcoex_IsBtLinkExist(adapt);
}

void rtw_btcoex_pta_off_on_notify(struct adapter * adapt, u8 bBTON)
{
	hal_btcoex_pta_off_on_notify(adapt, bBTON);
}

/* ==================================================
 * Below Functions are called by BT-Coex
 * ================================================== */
void rtw_btcoex_rx_ampdu_apply(struct adapter * adapt)
{
	rtw_rx_ampdu_apply(adapt);
}

void rtw_btcoex_LPS_Enter(struct adapter * adapt)
{
	struct pwrctrl_priv *pwrpriv;
	u8 lpsVal;


	pwrpriv = adapter_to_pwrctl(adapt);

	pwrpriv->bpower_saving = true;
	lpsVal = rtw_btcoex_LpsVal(adapt);
	rtw_set_ps_mode(adapt, PS_MODE_MIN, 0, lpsVal, "BTCOEX");
}

u8 rtw_btcoex_LPS_Leave(struct adapter * adapt)
{
	struct pwrctrl_priv *pwrpriv;


	pwrpriv = adapter_to_pwrctl(adapt);

	if (pwrpriv->pwr_mode != PS_MODE_ACTIVE) {
		rtw_set_ps_mode(adapt, PS_MODE_ACTIVE, 0, 0, "BTCOEX");
		LPS_RF_ON_check(adapt, 100);
		pwrpriv->bpower_saving = false;
	}

	return true;
}

u16 rtw_btcoex_btreg_read(struct adapter * adapt, u8 type, u16 addr, u32 *data)
{
	return hal_btcoex_btreg_read(adapt, type, addr, data);
}

u16 rtw_btcoex_btreg_write(struct adapter * adapt, u8 type, u16 addr, u16 val)
{
	return hal_btcoex_btreg_write(adapt, type, addr, val);
}

u8 rtw_btcoex_get_bt_coexist(struct adapter * adapt)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);

	return pHalData->EEPROMBluetoothCoexist;
}

u8 rtw_btcoex_get_chip_type(struct adapter * adapt)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);

	return pHalData->EEPROMBluetoothType;
}

u8 rtw_btcoex_get_pg_ant_num(struct adapter * adapt)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);

	return pHalData->EEPROMBluetoothAntNum == Ant_x2 ? 2 : 1;
}

u8 rtw_btcoex_get_pg_single_ant_path(struct adapter * adapt)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);

	return pHalData->ant_path;
}

u8 rtw_btcoex_get_pg_rfe_type(struct adapter * adapt)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);

	return pHalData->rfe_type;
}

u8 rtw_btcoex_is_tfbga_package_type(struct adapter * adapt)
{
	return false;
}

u8 rtw_btcoex_get_ant_div_cfg(struct adapter * adapt)
{
	struct hal_com_data * pHalData;

	pHalData = GET_HAL_DATA(adapt);
	
	return (pHalData->AntDivCfg == 0) ? false : true;
}

/* ==================================================
 * Below Functions are BT-Coex socket related function
 * ================================================== */

void rtw_btcoex_set_ant_info(struct adapter * adapt)
{
	struct hal_com_data * hal = GET_HAL_DATA(adapt);

	if (hal->EEPROMBluetoothCoexist) {
		u8 bMacPwrCtrlOn = false;

		rtw_btcoex_AntInfoSetting(adapt);
		rtw_hal_get_hwreg(adapt, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
		if (bMacPwrCtrlOn)
			rtw_btcoex_PowerOnSetting(adapt);
	} else {
		rtw_btcoex_wifionly_AntInfoSetting(adapt);
	}
}
