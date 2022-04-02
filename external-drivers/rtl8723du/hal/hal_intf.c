// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _HAL_INTF_C_

#include <drv_types.h>
#include <hal_data.h>

const u32 _chip_type_to_odm_ic_type[] = {
	0,
	ODM_RTL8188E,
	ODM_RTL8192E,
	ODM_RTL8812,
	ODM_RTL8821,
	ODM_RTL8723B,
	ODM_RTL8814A,
	ODM_RTL8703B,
	ODM_RTL8188F,
	ODM_RTL8822B,
	ODM_RTL8723D,
	ODM_RTL8821C,
	0,
};

void rtw_hal_chip_configure(struct adapter *adapt)
{
	adapt->hal_func.intf_chip_configure(adapt);
}

/*
 * Description:
 *	Read chip internal ROM data
 *
 * Return:
 *	_SUCCESS success
 *	_FAIL	 fail
 */
u8 rtw_hal_read_chip_info(struct adapter *adapt)
{
	u8 rtn = _SUCCESS;
	u8 hci_type = rtw_get_intf_type(adapt);
	unsigned long start = rtw_get_current_time();

	/*  before access eFuse, make sure card enable has been called */
	if ((hci_type == RTW_SDIO || hci_type == RTW_GSPI)
	    && !rtw_is_hw_init_completed(adapt))
		rtw_hal_power_on(adapt);

	rtn = adapt->hal_func.read_adapter_info(adapt);

	if ((hci_type == RTW_SDIO || hci_type == RTW_GSPI)
	    && !rtw_is_hw_init_completed(adapt))
		rtw_hal_power_off(adapt);

	RTW_INFO("%s in %d ms\n", __func__, rtw_get_passing_time_ms(start));

	return rtn;
}

void rtw_hal_read_chip_version(struct adapter *adapt)
{
	adapt->hal_func.read_chip_version(adapt);
	rtw_odm_init_ic_type(adapt);
}

void rtw_hal_def_value_init(struct adapter *adapt)
{
	if (is_primary_adapter(adapt)) {
		adapt->hal_func.init_default_value(adapt);

		rtw_init_hal_com_default_value(adapt);

		{
			struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
			struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapt);

			/* hal_spec is ready here */
			dvobj->macid_ctl.num = rtw_min(hal_spec->macid_num, MACID_NUM_SW_LIMIT);

			dvobj->cam_ctl.sec_cap = hal_spec->sec_cap;
			dvobj->cam_ctl.num = rtw_min(hal_spec->sec_cam_ent_num, SEC_CAM_ENT_NUM_SW_LIMIT);
		}
	}
}

u8 rtw_hal_data_init(struct adapter *adapt)
{
	if (is_primary_adapter(adapt)) {
		adapt->hal_data_sz = sizeof(struct hal_com_data);
		adapt->HalData = vzalloc(adapt->hal_data_sz);
		if (!adapt->HalData) {
			RTW_INFO("cant not alloc memory for HAL DATA\n");
			return _FAIL;
		}
	}
	return _SUCCESS;
}

void rtw_hal_data_deinit(struct adapter *adapt)
{
	if (is_primary_adapter(adapt)) {
		if (adapt->HalData) {
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
			phy_free_filebuf(adapt);
#endif
			vfree(adapt->HalData);
			adapt->HalData = NULL;
			adapt->hal_data_sz = 0;
		}
	}
}

void	rtw_hal_free_data(struct adapter *adapt)
{
	/* free HAL Data	 */
	rtw_hal_data_deinit(adapt);
}
void rtw_hal_dm_init(struct adapter *adapt)
{
	if (is_primary_adapter(adapt)) {
		struct hal_com_data *	pHalData = GET_HAL_DATA(adapt);

		adapt->hal_func.dm_init(adapt);

		spin_lock_init(&pHalData->IQKSpinLock);

		phy_load_tx_power_ext_info(adapt, 1);
	}
}
void rtw_hal_dm_deinit(struct adapter *adapt)
{
	if (is_primary_adapter(adapt))
		adapt->hal_func.dm_deinit(adapt);
}

void	rtw_hal_sw_led_init(struct adapter *adapt)
{
	if (adapt->hal_func.InitSwLeds)
		adapt->hal_func.InitSwLeds(adapt);
}

void rtw_hal_sw_led_deinit(struct adapter *adapt)
{
	if (adapt->hal_func.DeInitSwLeds)
		adapt->hal_func.DeInitSwLeds(adapt);
}

u32 rtw_hal_power_on(struct adapter *adapt)
{
	u32 ret = 0;
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);

	ret = adapt->hal_func.hal_power_on(adapt);

	if ((ret == _SUCCESS) && (pHalData->EEPROMBluetoothCoexist))
		rtw_btcoex_PowerOnSetting(adapt);
	return ret;
}

void rtw_hal_power_off(struct adapter *adapt)
{
	struct macid_ctl_t *macid_ctl = &adapt->dvobj->macid_ctl;

	memset(macid_ctl->h2c_msr, 0, MACID_NUM_SW_LIMIT);

	rtw_btcoex_PowerOffSetting(adapt);

	adapt->hal_func.hal_power_off(adapt);
}

static void rtw_hal_init_opmode(struct adapter *adapt)
{
	enum ndis_802_11_network_infrastructure networkType = Ndis802_11InfrastructureMax;
	struct  mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	int fw_state;

	fw_state = get_fwstate(pmlmepriv);

	if (fw_state & WIFI_ADHOC_STATE)
		networkType = Ndis802_11IBSS;
	else if (fw_state & WIFI_STATION_STATE)
		networkType = Ndis802_11Infrastructure;
	else if (fw_state & WIFI_AP_STATE)
		networkType = Ndis802_11APMode;
	else
		return;

	rtw_setopmode_cmd(adapt, networkType, RTW_CMDF_DIRECTLY);
}

uint	 rtw_hal_init(struct adapter *adapt)
{
	uint	status = _SUCCESS;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapt);
	int i;

	status = adapt->hal_func.hal_init(adapt);

	if (status == _SUCCESS) {
		pHalData->hw_init_completed = true;
		rtw_restore_mac_addr(adapt);
		if (adapt->registrypriv.notch_filter == 1)
			rtw_hal_notch_filter(adapt, 1);

		for (i = 0; i < dvobj->iface_nums; i++)
			rtw_sec_restore_wep_key(dvobj->adapters[i]);

		rtw_led_control(adapt, LED_CTL_POWER_ON);

		init_hw_mlme_ext(adapt);

		rtw_hal_init_opmode(adapt);

		rtw_bb_rf_gain_offset(adapt);
	} else {
		pHalData->hw_init_completed = false;
		RTW_INFO("rtw_hal_init: hal_init fail\n");
	}
	return status;
}

uint rtw_hal_deinit(struct adapter *adapt)
{
	uint	status = _SUCCESS;
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapt);

	status = adapt->hal_func.hal_deinit(adapt);

	if (status == _SUCCESS) {
		rtw_led_control(adapt, LED_CTL_POWER_OFF);
		pHalData->hw_init_completed = false;
	} else
		RTW_INFO("\n rtw_hal_deinit: hal_init fail\n");


	return status;
}

u8 rtw_hal_set_hwreg(struct adapter *adapt, u8 variable, u8 *val)
{
	return adapt->hal_func.set_hw_reg_handler(adapt, variable, val);
}

void rtw_hal_get_hwreg(struct adapter *adapt, u8 variable, u8 *val)
{
	adapt->hal_func.GetHwRegHandler(adapt, variable, val);
}

u8 rtw_hal_set_def_var(struct adapter *adapt, enum hal_def_variable eVariable, void * pValue)
{
	return adapt->hal_func.SetHalDefVarHandler(adapt, eVariable, pValue);
}
u8 rtw_hal_get_def_var(struct adapter *adapt, enum hal_def_variable eVariable, void * pValue)
{
	return adapt->hal_func.get_hal_def_var_handler(adapt, eVariable, pValue);
}

void rtw_hal_set_odm_var(struct adapter *adapt, enum hal_odm_variable eVariable, void * pValue1, bool bSet)
{
	adapt->hal_func.SetHalODMVarHandler(adapt, eVariable, pValue1, bSet);
}
void	rtw_hal_get_odm_var(struct adapter *adapt, enum hal_odm_variable eVariable, void * pValue1, void * pValue2)
{
	adapt->hal_func.GetHalODMVarHandler(adapt, eVariable, pValue1, pValue2);
}

/* FOR SDIO & PCIE */
void rtw_hal_enable_interrupt(struct adapter *adapt)
{
}

/* FOR SDIO & PCIE */
void rtw_hal_disable_interrupt(struct adapter *adapt)
{
}

u8 rtw_hal_check_ips_status(struct adapter *adapt)
{
	u8 val = false;
	if (adapt->hal_func.check_ips_status)
		val = adapt->hal_func.check_ips_status(adapt);
	else
		RTW_INFO("%s: hal_func.check_ips_status is NULL!\n", __func__);

	return val;
}

int rtw_hal_fw_dl(struct adapter *adapt, bool wowlan)
{
	return adapt->hal_func.fw_dl(adapt, wowlan);
}

u32	rtw_hal_inirp_init(struct adapter *adapt)
{
	if (is_primary_adapter(adapt))
		return adapt->hal_func.inirp_init(adapt);
	return _SUCCESS;
}
u32	rtw_hal_inirp_deinit(struct adapter *adapt)
{

	if (is_primary_adapter(adapt))
		return adapt->hal_func.inirp_deinit(adapt);

	return _SUCCESS;
}

/* for USB Auto-suspend */
u8	rtw_hal_intf_ps_func(struct adapter *adapt, enum hal_intf_ps_func efunc_id, u8 *val)
{
	if (adapt->hal_func.interface_ps_func)
		return adapt->hal_func.interface_ps_func(adapt, efunc_id, val);
	return _FAIL;
}

int	rtw_hal_xmitframe_enqueue(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	return adapt->hal_func.hal_xmitframe_enqueue(adapt, pxmitframe);
}

int	rtw_hal_xmit(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	return adapt->hal_func.hal_xmit(adapt, pxmitframe);
}

/*
 * [IMPORTANT] This function would be run in interrupt context.
 */
int	rtw_hal_mgnt_xmit(struct adapter *adapt, struct xmit_frame *pmgntframe)
{
	update_mgntframe_attrib_addr(adapt, pmgntframe);

#if defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH)
	if ((!MLME_IS_MESH(adapt) && SEC_IS_BIP_KEY_INSTALLED(&adapt->securitypriv))
		#ifdef CONFIG_RTW_MESH
		|| (MLME_IS_MESH(adapt) && adapt->mesh_info.mesh_auth_id)
		#endif
	)
		rtw_mgmt_xmitframe_coalesce(adapt, pmgntframe->pkt, pmgntframe);
#endif

	return adapt->hal_func.mgnt_xmit(adapt, pmgntframe);
}

int	rtw_hal_init_xmit_priv(struct adapter *adapt)
{
	return adapt->hal_func.init_xmit_priv(adapt);
}
void	rtw_hal_free_xmit_priv(struct adapter *adapt)
{
	adapt->hal_func.free_xmit_priv(adapt);
}

int	rtw_hal_init_recv_priv(struct adapter *adapt)
{
	return adapt->hal_func.init_recv_priv(adapt);
}
void	rtw_hal_free_recv_priv(struct adapter *adapt)
{
	adapt->hal_func.free_recv_priv(adapt);
}

static void rtw_sta_ra_registed(struct adapter *adapt, struct sta_info *psta)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapt);

	if (!psta) {
		RTW_ERR(FUNC_ADPT_FMT" sta is NULL\n", FUNC_ADPT_ARG(adapt));
		rtw_warn_on(1);
		return;
	}

	if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt)) {
		if (psta->cmn.aid > adapt->stapriv.max_aid) {
			RTW_ERR("station aid %d exceed the max number\n", psta->cmn.aid);
			rtw_warn_on(1);
			return;
		}
		rtw_ap_update_sta_ra_info(adapt, psta);
	}

	psta->cmn.ra_info.ra_bw_mode = rtw_get_tx_bw_mode(adapt, psta);
	/*set correct initial date rate for each mac_id */
	hal_data->INIDATA_RATE[psta->cmn.mac_id] = psta->init_rate;

	rtw_phydm_ra_registed(adapt, psta);
}

void rtw_hal_update_ra_mask(struct sta_info *psta)
{
	struct adapter *adapt;

	if (!psta)
		return;

	adapt = psta->adapt;
	rtw_sta_ra_registed(adapt, psta);
}

/*	Start specifical interface thread		*/
void	rtw_hal_start_thread(struct adapter *adapt)
{
}
/*	Start specifical interface thread		*/
void	rtw_hal_stop_thread(struct adapter *adapt)
{
}

u32	rtw_hal_read_bbreg(struct adapter *adapt, u32 RegAddr, u32 BitMask)
{
	u32 data = 0;
	if (adapt->hal_func.read_bbreg)
		data = adapt->hal_func.read_bbreg(adapt, RegAddr, BitMask);
	return data;
}
void	rtw_hal_write_bbreg(struct adapter *adapt, u32 RegAddr, u32 BitMask, u32 Data)
{
	if (adapt->hal_func.write_bbreg)
		adapt->hal_func.write_bbreg(adapt, RegAddr, BitMask, Data);
}

u32 rtw_hal_read_rfreg(struct adapter *adapt, enum rf_path eRFPath, u32 RegAddr, u32 BitMask)
{
	u32 data = 0;

	if (adapt->hal_func.read_rfreg) {
		data = adapt->hal_func.read_rfreg(adapt, eRFPath, RegAddr, BitMask);

		if (match_rf_read_sniff_ranges(eRFPath, RegAddr, BitMask)) {
			RTW_INFO("DBG_IO rtw_hal_read_rfreg(%u, 0x%04x, 0x%08x) read:0x%08x(0x%08x)\n"
				, eRFPath, RegAddr, BitMask, (data << PHY_CalculateBitShift(BitMask)), data);
		}
	}

	return data;
}

void rtw_hal_write_rfreg(struct adapter *adapt, enum rf_path eRFPath, u32 RegAddr, u32 BitMask, u32 Data)
{
	if (adapt->hal_func.write_rfreg) {

		if (match_rf_write_sniff_ranges(eRFPath, RegAddr, BitMask)) {
			RTW_INFO("DBG_IO rtw_hal_write_rfreg(%u, 0x%04x, 0x%08x) write:0x%08x(0x%08x)\n"
				, eRFPath, RegAddr, BitMask, (Data << PHY_CalculateBitShift(BitMask)), Data);
		}

		adapt->hal_func.write_rfreg(adapt, eRFPath, RegAddr, BitMask, Data);
	}
}

void	rtw_hal_set_chnl_bw(struct adapter *adapt, u8 channel, enum channel_width Bandwidth, u8 Offset40, u8 Offset80)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapt);
	u8 cch_80 = Bandwidth == CHANNEL_WIDTH_80 ? channel : 0;
	u8 cch_40 = Bandwidth == CHANNEL_WIDTH_40 ? channel : 0;
	u8 cch_20 = Bandwidth == CHANNEL_WIDTH_20 ? channel : 0;

	if (rtw_phydm_is_iqk_in_progress(adapt))
		RTW_ERR("%s, %d, IQK may race condition\n", __func__, __LINE__);
	if (cch_80 != 0)
		cch_40 = rtw_get_scch_by_cch_offset(cch_80, CHANNEL_WIDTH_80, Offset80);
	if (cch_40 != 0)
		cch_20 = rtw_get_scch_by_cch_offset(cch_40, CHANNEL_WIDTH_40, Offset40);
	pHalData->cch_80 = cch_80;
	pHalData->cch_40 = cch_40;
	pHalData->cch_20 = cch_20;

	adapt->hal_func.set_chnl_bw_handler(adapt, channel, Bandwidth, Offset40, Offset80);
}

void	rtw_hal_set_tx_power_level(struct adapter *adapt, u8 channel)
{
	if (adapt->hal_func.set_tx_power_level_handler)
		adapt->hal_func.set_tx_power_level_handler(adapt, channel);
}

void	rtw_hal_get_tx_power_level(struct adapter *adapt, int *powerlevel)
{
	if (adapt->hal_func.get_tx_power_level_handler)
		adapt->hal_func.get_tx_power_level_handler(adapt, powerlevel);
}

void	rtw_hal_dm_watchdog(struct adapter *adapt)
{

	rtw_hal_turbo_edca(adapt);
	adapt->hal_func.hal_dm_watchdog(adapt);

#ifdef CONFIG_PCI_DYNAMIC_ASPM
	rtw_pci_aspm_config_dynamic_l1_ilde_time(adapt);
#endif
}

void rtw_hal_bcn_related_reg_setting(struct adapter *adapt)
{
	adapt->hal_func.SetBeaconRelatedRegistersHandler(adapt);
}

void rtw_hal_notch_filter(struct adapter *adapter, bool enable)
{
	if (adapter->hal_func.hal_notch_filter)
		adapter->hal_func.hal_notch_filter(adapter, enable);
}

#ifdef CONFIG_FW_C2H_REG
inline bool rtw_hal_c2h_valid(struct adapter *adapter, u8 *buf)
{
	struct hal_com_data *HalData = GET_HAL_DATA(adapter);
	struct hal_version *hal_ver = &HalData->version_id;
	bool ret = _FAIL;

	ret = C2H_ID_88XX(buf) || C2H_PLEN_88XX(buf);

	return ret;
}

inline int rtw_hal_c2h_evt_read(struct adapter *adapter, u8 *buf)
{
	struct hal_com_data *HalData = GET_HAL_DATA(adapter);
	struct hal_version *hal_ver = &HalData->version_id;
	int ret = _FAIL;

	ret = c2h_evt_read_88xx(adapter, buf);

	return ret;
}

bool rtw_hal_c2h_reg_hdr_parse(struct adapter *adapter, u8 *buf, u8 *id, u8 *seq, u8 *plen, u8 **payload)
{
	struct hal_com_data *HalData = GET_HAL_DATA(adapter);
	struct hal_version *hal_ver = &HalData->version_id;
	bool ret = _FAIL;

	*id = C2H_ID_88XX(buf);
	*seq = C2H_SEQ_88XX(buf);
	*plen = C2H_PLEN_88XX(buf);
	*payload = C2H_PAYLOAD_88XX(buf);
	ret = _SUCCESS;

	return ret;
}
#endif /* CONFIG_FW_C2H_REG */

bool rtw_hal_c2h_pkt_hdr_parse(struct adapter *adapter, u8 *buf, u16 len, u8 *id, u8 *seq, u8 *plen, u8 **payload)
{
	bool ret = _FAIL;

	if (!buf || len > 256 || len < 3)
		goto exit;
	*id = C2H_ID_88XX(buf);
	*seq = C2H_SEQ_88XX(buf);
	*plen = len - 2;
	*payload = C2H_PAYLOAD_88XX(buf);
	ret = _SUCCESS;

exit:
	return ret;
}

int c2h_handler(struct adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	u8 sub_id = 0;
	int ret = _SUCCESS;

	switch (id) {
	case C2H_FW_SCAN_COMPLETE:
		RTW_INFO("[C2H], FW Scan Complete\n");
		break;
	case C2H_BT_INFO:
		rtw_btcoex_BtInfoNotify(adapter, plen, payload);
		break;
	case C2H_BT_MP_INFO:
		rtw_btcoex_BtMpRptNotify(adapter, plen, payload);
		break;
	case C2H_MAILBOX_STATUS:
		RTW_DBG_DUMP("C2H_MAILBOX_STATUS: ", payload, plen);
		break;
	case C2H_WLAN_INFO:
		rtw_btcoex_WlFwDbgInfoNotify(adapter, payload, plen);
		break;
	case C2H_IQK_FINISH:
		c2h_iqk_offload(adapter, payload, plen);
		break;
	case C2H_MAC_HIDDEN_RPT:
		c2h_mac_hidden_rpt_hdl(adapter, payload, plen);
		break;
	case C2H_MAC_HIDDEN_RPT_2:
		c2h_mac_hidden_rpt_2_hdl(adapter, payload, plen);
		break;
	case C2H_DEFEATURE_DBG:
		c2h_defeature_dbg_hdl(adapter, payload, plen);
		break;
	case C2H_CUSTOMER_STR_RPT:
		c2h_customer_str_rpt_hdl(adapter, payload, plen);
		break;
	case C2H_CUSTOMER_STR_RPT_2:
		c2h_customer_str_rpt_2_hdl(adapter, payload, plen);
		break;
	case C2H_EXTEND:
		sub_id = payload[0];
		__attribute__((__fallthrough__));
	default:
		if (!phydm_c2H_content_parsing(adapter_to_phydm(adapter), id, plen, payload))
			ret = _FAIL;
		break;
	}
	if (ret != _SUCCESS) {
		if (id == C2H_EXTEND)
			RTW_WARN("%s: unknown C2H(0x%02x, 0x%02x)\n", __func__, id, sub_id);
		else
			RTW_WARN("%s: unknown C2H(0x%02x)\n", __func__, id);
	}
	return ret;
}

int rtw_hal_c2h_handler(struct adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	int ret = _FAIL;

	ret = adapter->hal_func.c2h_handler(adapter, id, seq, plen, payload);
	if (ret != _SUCCESS)
		ret = c2h_handler(adapter, id, seq, plen, payload);

	return ret;
}

int rtw_hal_c2h_id_handle_directly(struct adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	switch (id) {
	case C2H_CCX_TX_RPT:
	case C2H_BT_MP_INFO:
	case C2H_FW_CHNL_SWITCH_COMPLETE:
	case C2H_IQK_FINISH:
	case C2H_MCC:
	case C2H_BCN_EARLY_RPT:
	case C2H_AP_REQ_TXRPT:
	case C2H_SPC_STAT:
		return true;
	default:
		return false;
	}
}

int rtw_hal_is_disable_sw_channel_plan(struct adapter * adapt)
{
	return GET_HAL_DATA(adapt)->bDisableSWChannelPlan;
}

static int _rtw_hal_macid_sleep(struct adapter *adapter, u8 macid, u8 sleep)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);
	u16 reg_sleep;
	u8 bit_shift;
	u32 val32;
	int ret = _FAIL;

	if (macid >= macid_ctl->num) {
		RTW_ERR(ADPT_FMT" %s invalid macid(%u)\n"
			, ADPT_ARG(adapter), sleep ? "sleep" : "wakeup" , macid);
		goto exit;
	}

	if (macid < 32) {
		reg_sleep = macid_ctl->reg_sleep_m0;
		bit_shift = macid;
	#if (MACID_NUM_SW_LIMIT > 32)
	} else if (macid < 64) {
		reg_sleep = macid_ctl->reg_sleep_m1;
		bit_shift = macid - 32;
	#endif
	#if (MACID_NUM_SW_LIMIT > 64)
	} else if (macid < 96) {
		reg_sleep = macid_ctl->reg_sleep_m2;
		bit_shift = macid - 64;
	#endif
	#if (MACID_NUM_SW_LIMIT > 96)
	} else if (macid < 128) {
		reg_sleep = macid_ctl->reg_sleep_m3;
		bit_shift = macid - 96;
	#endif
	} else {
		rtw_warn_on(1);
		goto exit;
	}

	if (!reg_sleep) {
		rtw_warn_on(1);
		goto exit;
	}

	val32 = rtw_read32(adapter, reg_sleep);
	RTW_INFO(ADPT_FMT" %s macid=%d, ori reg_0x%03x=0x%08x\n"
		, ADPT_ARG(adapter), sleep ? "sleep" : "wakeup"
		, macid, reg_sleep, val32);

	ret = _SUCCESS;

	if (sleep) {
		if (val32 & BIT(bit_shift))
			goto exit;
		val32 |= BIT(bit_shift);
	} else {
		if (!(val32 & BIT(bit_shift)))
			goto exit;
		val32 &= ~BIT(bit_shift);
	}

	rtw_write32(adapter, reg_sleep, val32);

exit:
	return ret;
}

inline int rtw_hal_macid_sleep(struct adapter *adapter, u8 macid)
{
	return _rtw_hal_macid_sleep(adapter, macid, 1);
}

inline int rtw_hal_macid_wakeup(struct adapter *adapter, u8 macid)
{
	return _rtw_hal_macid_sleep(adapter, macid, 0);
}

static int _rtw_hal_macid_bmp_sleep(struct adapter *adapter, struct macid_bmp *bmp, u8 sleep)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);
	u16 reg_sleep;
	u32 *m = &bmp->m0;
	u8 mid = 0;
	u32 val32;

	do {
		if (*m == 0)
			goto move_next;

		if (mid == 0)
			reg_sleep = macid_ctl->reg_sleep_m0;
		#if (MACID_NUM_SW_LIMIT > 32)
		else if (mid == 1)
			reg_sleep = macid_ctl->reg_sleep_m1;
		#endif
		#if (MACID_NUM_SW_LIMIT > 64)
		else if (mid == 2)
			reg_sleep = macid_ctl->reg_sleep_m2;
		#endif
		#if (MACID_NUM_SW_LIMIT > 96)
		else if (mid == 3)
			reg_sleep = macid_ctl->reg_sleep_m3;
		#endif
		else {
			rtw_warn_on(1);
			break;
		}

		if (!reg_sleep) {
			rtw_warn_on(1);
			break;
		}

		val32 = rtw_read32(adapter, reg_sleep);
		RTW_INFO(ADPT_FMT" %s m%u=0x%08x, ori reg_0x%03x=0x%08x\n"
			, ADPT_ARG(adapter), sleep ? "sleep" : "wakeup"
			, mid, *m, reg_sleep, val32);

		if (sleep) {
			if ((val32 & *m) == *m)
				goto move_next;
			val32 |= *m;
		} else {
			if ((val32 & *m) == 0)
				goto move_next;
			val32 &= ~(*m);
		}

		rtw_write32(adapter, reg_sleep, val32);

move_next:
		m++;
		mid++;
	} while (mid * 32 < MACID_NUM_SW_LIMIT);

	return _SUCCESS;
}

inline int rtw_hal_macid_sleep_all_used(struct adapter *adapter)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);

	return _rtw_hal_macid_bmp_sleep(adapter, &macid_ctl->used, 1);
}

inline int rtw_hal_macid_wakeup_all_used(struct adapter *adapter)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);

	return _rtw_hal_macid_bmp_sleep(adapter, &macid_ctl->used, 0);
}

int rtw_hal_fill_h2c_cmd(struct adapter * adapt, u8 ElementID, u32 CmdLen, u8 *pCmdBuffer)
{
	struct adapter *pri_adapter = GET_PRIMARY_ADAPTER(adapt);

	if (GET_HAL_DATA(pri_adapter)->bFWReady)
		return adapt->hal_func.fill_h2c_cmd(adapt, ElementID, CmdLen, pCmdBuffer);
	else if (adapt->registrypriv.mp_mode == 0)
		RTW_PRINT(FUNC_ADPT_FMT" FW doesn't exit when no MP mode, by pass H2C id:0x%02x\n"
			  , FUNC_ADPT_ARG(adapt), ElementID);
	return _FAIL;
}

void rtw_hal_fill_fake_txdesc(struct adapter *adapt, u8 *pDesc, u32 BufferLen,
			      u8 IsPsPoll, u8 IsBTQosNull, u8 bDataFrame)
{
	adapt->hal_func.fill_fake_txdesc(adapt, pDesc, BufferLen, IsPsPoll, IsBTQosNull, bDataFrame);

}

u8 rtw_hal_get_txbuff_rsvd_page_num(struct adapter *adapter, bool wowlan)
{
	u8 num = 0;


	if (adapter->hal_func.hal_get_tx_buff_rsvd_page_num) {
		num = adapter->hal_func.hal_get_tx_buff_rsvd_page_num(adapter, wowlan);
	}

	return num;
}

void rtw_hal_fw_correct_bcn(struct adapter *adapt)
{
	if (adapt->hal_func.fw_correct_bcn)
		adapt->hal_func.fw_correct_bcn(adapt);
}

void rtw_hal_set_tx_power_index(struct adapter * adapt, u32 powerindex, enum rf_path rfpath, u8 rate)
{
	return adapt->hal_func.set_tx_power_index_handler(adapt, powerindex, rfpath, rate);
}

u8 rtw_hal_get_tx_power_index(struct adapter * adapt, enum rf_path rfpath, u8 rate, u8 bandwidth, u8 channel, struct txpwr_idx_comp *tic)
{
	return adapt->hal_func.get_tx_power_index_handler(adapt, rfpath, rate, bandwidth, channel, tic);
}

#define rtw_hal_error_msg(ops_fun)		\
	RTW_PRINT("### %s - Error : Please hook hal_func.%s ###\n", __func__, ops_fun)

u8 rtw_hal_ops_check(struct adapter *adapt)
{
	u8 ret = _SUCCESS;
	/*** initialize section ***/
	if (!adapt->hal_func.read_chip_version) {
		rtw_hal_error_msg("read_chip_version");
		ret = _FAIL;
	}
	if (!adapt->hal_func.init_default_value) {
		rtw_hal_error_msg("init_default_value");
		ret = _FAIL;
	}
	if (!adapt->hal_func.intf_chip_configure) {
		rtw_hal_error_msg("intf_chip_configure");
		ret = _FAIL;
	}
	if (!adapt->hal_func.read_adapter_info) {
		rtw_hal_error_msg("read_adapter_info");
		ret = _FAIL;
	}

	if (!adapt->hal_func.hal_power_on) {
		rtw_hal_error_msg("hal_power_on");
		ret = _FAIL;
	}
	if (!adapt->hal_func.hal_power_off) {
		rtw_hal_error_msg("hal_power_off");
		ret = _FAIL;
	}

	if (!adapt->hal_func.hal_init) {
		rtw_hal_error_msg("hal_init");
		ret = _FAIL;
	}
	if (!adapt->hal_func.hal_deinit) {
		rtw_hal_error_msg("hal_deinit");
		ret = _FAIL;
	}

	/*** xmit section ***/
	if (!adapt->hal_func.init_xmit_priv) {
		rtw_hal_error_msg("init_xmit_priv");
		ret = _FAIL;
	}
	if (!adapt->hal_func.free_xmit_priv) {
		rtw_hal_error_msg("free_xmit_priv");
		ret = _FAIL;
	}
	if (!adapt->hal_func.hal_xmit) {
		rtw_hal_error_msg("hal_xmit");
		ret = _FAIL;
	}
	if (!adapt->hal_func.mgnt_xmit) {
		rtw_hal_error_msg("mgnt_xmit");
		ret = _FAIL;
	}
	if (!adapt->hal_func.hal_xmitframe_enqueue) {
		rtw_hal_error_msg("hal_xmitframe_enqueue");
		ret = _FAIL;
	}
	/*** recv section ***/
	if (!adapt->hal_func.init_recv_priv) {
		rtw_hal_error_msg("init_recv_priv");
		ret = _FAIL;
	}
	if (!adapt->hal_func.free_recv_priv) {
		rtw_hal_error_msg("free_recv_priv");
		ret = _FAIL;
	}
	if (!adapt->hal_func.inirp_init) {
		rtw_hal_error_msg("inirp_init");
		ret = _FAIL;
	}
	if (!adapt->hal_func.inirp_deinit) {
		rtw_hal_error_msg("inirp_deinit");
		ret = _FAIL;
	}

	/*** DM section ***/
	if (!adapt->hal_func.dm_init) {
		rtw_hal_error_msg("dm_init");
		ret = _FAIL;
	}
	if (!adapt->hal_func.dm_deinit) {
		rtw_hal_error_msg("dm_deinit");
		ret = _FAIL;
	}
	if (!adapt->hal_func.hal_dm_watchdog) {
		rtw_hal_error_msg("hal_dm_watchdog");
		ret = _FAIL;
	}

	/*** xxx section ***/
	if (!adapt->hal_func.set_chnl_bw_handler) {
		rtw_hal_error_msg("set_chnl_bw_handler");
		ret = _FAIL;
	}

	if (!adapt->hal_func.set_hw_reg_handler) {
		rtw_hal_error_msg("set_hw_reg_handler");
		ret = _FAIL;
	}
	if (!adapt->hal_func.GetHwRegHandler) {
		rtw_hal_error_msg("GetHwRegHandler");
		ret = _FAIL;
	}
	if (!adapt->hal_func.get_hal_def_var_handler) {
		rtw_hal_error_msg("get_hal_def_var_handler");
		ret = _FAIL;
	}
	if (!adapt->hal_func.SetHalDefVarHandler) {
		rtw_hal_error_msg("SetHalDefVarHandler");
		ret = _FAIL;
	}
	if (!adapt->hal_func.GetHalODMVarHandler) {
		rtw_hal_error_msg("GetHalODMVarHandler");
		ret = _FAIL;
	}
	if (!adapt->hal_func.SetHalODMVarHandler) {
		rtw_hal_error_msg("SetHalODMVarHandler");
		ret = _FAIL;
	}

	if (!adapt->hal_func.SetBeaconRelatedRegistersHandler) {
		rtw_hal_error_msg("SetBeaconRelatedRegistersHandler");
		ret = _FAIL;
	}

	if (!adapt->hal_func.fill_h2c_cmd) {
		rtw_hal_error_msg("fill_h2c_cmd");
		ret = _FAIL;
	}

	if (!adapt->hal_func.c2h_handler) {
		rtw_hal_error_msg("c2h_handler");
		ret = _FAIL;
	}
	if (!adapt->hal_func.fill_fake_txdesc) {
		rtw_hal_error_msg("fill_fake_txdesc");
		ret = _FAIL;
	}

	if (!adapt->hal_func.hal_get_tx_buff_rsvd_page_num) {
		rtw_hal_error_msg("hal_get_tx_buff_rsvd_page_num");
		ret = _FAIL;
	}

	if (!adapt->hal_func.fw_dl) {
		rtw_hal_error_msg("fw_dl");
		ret = _FAIL;
	}

	if (!adapt->hal_func.get_tx_power_index_handler) {
		rtw_hal_error_msg("get_tx_power_index_handler");
		ret = _FAIL;
	}

	/*** SReset section ***/
	return  ret;
}
