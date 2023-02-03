// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_PWRCTRL_C_

#include <drv_types.h>
#include <hal_data.h>
#include <hal_com_h2c.h>

int rtw_fw_ps_state(struct adapter * adapt)
{
	struct dvobj_priv *psdpriv = adapt->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;
	int ret = _FAIL, dont_care = 0;
	u16 fw_ps_state = 0;
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	struct registry_priv  *registry_par = &adapt->registrypriv;

	if (registry_par->check_fw_ps != 1)
		return _SUCCESS;

	_enter_pwrlock(&pwrpriv->check_32k_lock);

	if (RTW_CANNOT_RUN(adapt)) {
		RTW_INFO("%s: bSurpriseRemoved=%s , hw_init_completed=%d, bDriverStopped=%s\n", __func__
			 , rtw_is_surprise_removed(adapt) ? "True" : "False"
			 , rtw_get_hw_init_completed(adapt)
			 , rtw_is_drv_stopped(adapt) ? "True" : "False");
		goto exit_fw_ps_state;
	}
	rtw_hal_set_hwreg(adapt, HW_VAR_SET_REQ_FW_PS, (u8 *)&dont_care);
	{
		/* 4. if 0x88[7]=1, driver set cmd to leave LPS/IPS. */
		/* Else, hw will keep in active mode. */
		/* debug info: */
		/* 0x88[7] = 32kpermission, */
		/* 0x88[6:0] = current_ps_state */
		/* 0x89[7:0] = last_rpwm */

		rtw_hal_get_hwreg(adapt, HW_VAR_FW_PS_STATE, (u8 *)&fw_ps_state);

		if ((fw_ps_state & 0x80) == 0)
			ret = _SUCCESS;
		else {
			pdbgpriv->dbg_poll_fail_cnt++;
			RTW_INFO("%s: fw_ps_state=%04x\n", __func__, fw_ps_state);
		}
	}


exit_fw_ps_state:
	up(&pwrpriv->check_32k_lock);
	return ret;
}

void _ips_enter(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);

	pwrpriv->bips_processing = true;

	/* syn ips_mode with request */
	pwrpriv->ips_mode = pwrpriv->ips_mode_req;

	pwrpriv->ips_enter_cnts++;
	RTW_INFO("==>ips_enter cnts:%d\n", pwrpriv->ips_enter_cnts);

	if (rf_off == pwrpriv->change_rfpwrstate) {
		pwrpriv->bpower_saving = true;

		if (pwrpriv->ips_mode == IPS_LEVEL_2)
			pwrpriv->bkeepfwalive = true;

		pwrpriv->pwr_saving_start_time = rtw_get_current_time();

		rtw_ips_pwr_down(adapt);
		pwrpriv->rf_pwrstate = rf_off;
	}
	pwrpriv->bips_processing = false;

}

void ips_enter(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);

	rtw_btcoex_IpsNotify(adapt, pwrpriv->ips_mode_req);

	_enter_pwrlock(&pwrpriv->lock);
	_ips_enter(adapt);
	up(&pwrpriv->lock);
}

int _ips_leave(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	int result = _SUCCESS;

	if ((pwrpriv->rf_pwrstate == rf_off) && (!pwrpriv->bips_processing)) {
		pwrpriv->bips_processing = true;
		pwrpriv->change_rfpwrstate = rf_on;
		pwrpriv->ips_leave_cnts++;
		RTW_INFO("==>ips_leave cnts:%d\n", pwrpriv->ips_leave_cnts);

		result = rtw_ips_pwr_up(adapt);
		if (result == _SUCCESS)
			pwrpriv->rf_pwrstate = rf_on;
		
		pwrpriv->pwr_saving_time += rtw_get_passing_time_ms(pwrpriv->pwr_saving_start_time);

		RTW_INFO("==> ips_leave.....LED(0x%08x)...\n", rtw_read32(adapt, 0x4c));
		pwrpriv->bips_processing = false;

		pwrpriv->bkeepfwalive = false;
		pwrpriv->bpower_saving = false;
	}

	return result;
}

int ips_leave(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	int ret;

	if (!is_primary_adapter(adapt))
		return _SUCCESS;

	_enter_pwrlock(&pwrpriv->lock);
	ret = _ips_leave(adapt);
	up(&pwrpriv->lock);

	if (_SUCCESS == ret)
		odm_dm_reset(&GET_HAL_DATA(adapt)->odmpriv);

	if (_SUCCESS == ret)
		rtw_btcoex_IpsNotify(adapt, IPS_NONE);

	return ret;
}

#ifdef CONFIG_AUTOSUSPEND
	extern void autosuspend_enter(struct adapter *adapt);
	extern int autoresume_enter(struct adapter *adapt);
#endif

#ifdef SUPPORT_HW_RFOFF_DETECTED
	int rtw_hw_suspend(struct adapter *adapt);
	int rtw_hw_resume(struct adapter *adapt);
#endif

static bool rtw_pwr_unassociated_idle(struct adapter *adapter)
{
	u8 i;
	struct adapter *iface;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct xmit_priv *pxmit_priv = &adapter->xmitpriv;
	struct mlme_priv *pmlmepriv;
	struct wifidirect_info	*pwdinfo;
	struct cfg80211_wifidirect_info *pcfg80211_wdinfo;
	bool ret = false;

	if (adapter_to_pwrctl(adapter)->bpower_saving) {
		/* RTW_INFO("%s: already in LPS or IPS mode\n", __func__); */
		goto exit;
	}

	if (time_after(adapter_to_pwrctl(adapter)->ips_deny_time, rtw_get_current_time())) {
		/* RTW_INFO("%s ips_deny_time\n", __func__); */
		goto exit;
	}

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		if ((iface) && rtw_is_adapter_up(iface)) {
			pmlmepriv = &(iface->mlmepriv);
			pwdinfo = &(iface->wdinfo);
			pcfg80211_wdinfo = &iface->cfg80211_wdinfo;
			if (check_fwstate(pmlmepriv, WIFI_ASOC_STATE | WIFI_SITE_MONITOR)
				|| check_fwstate(pmlmepriv, WIFI_UNDER_LINKING | WIFI_UNDER_WPS)
				|| MLME_IS_AP(iface)
				|| MLME_IS_MESH(iface)
				|| check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE | WIFI_ADHOC_STATE)
				|| rtw_cfg80211_get_is_roch(iface)
				|| rtw_get_passing_time_ms(pcfg80211_wdinfo->last_ro_ch_time) < 3000
			)
				goto exit;

		}
	}

#if (MP_DRIVER == 1)
	if (adapter->registrypriv.mp_mode == 1)
		goto exit;
#endif

	if (pxmit_priv->free_xmitbuf_cnt != NR_XMITBUFF ||
	    pxmit_priv->free_xmit_extbuf_cnt != NR_XMIT_EXTBUFF) {
		RTW_PRINT("There are some pkts to transmit\n");
		RTW_PRINT("free_xmitbuf_cnt: %d, free_xmit_extbuf_cnt: %d\n",
			pxmit_priv->free_xmitbuf_cnt, pxmit_priv->free_xmit_extbuf_cnt);
		goto exit;
	}

	ret = true;

exit:
	return ret;
}


/*
 * ATTENTION:
 *	rtw_ps_processor() doesn't handle LPS.
 */
void rtw_ps_processor(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct dvobj_priv *psdpriv = adapt->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;
#ifdef SUPPORT_HW_RFOFF_DETECTED
	enum rt_rf_power_state rfpwrstate;
#endif /* SUPPORT_HW_RFOFF_DETECTED */
	u32 ps_deny = 0;

	_enter_pwrlock(&adapter_to_pwrctl(adapt)->lock);
	ps_deny = rtw_ps_deny_get(adapt);
	up(&adapter_to_pwrctl(adapt)->lock);
	if (ps_deny != 0) {
		goto exit;
	}

	if (pwrpriv->bInSuspend) { /* system suspend or autosuspend */
		pdbgpriv->dbg_ps_insuspend_cnt++;
		RTW_INFO("%s, pwrpriv->bInSuspend ignore this process\n", __func__);
		return;
	}

	pwrpriv->ps_processing = true;

#ifdef SUPPORT_HW_RFOFF_DETECTED
	if (pwrpriv->bips_processing)
		goto exit;

	/* RTW_INFO("==> fw report state(0x%x)\n",rtw_read8(adapt,0x1ca));	 */
	if (pwrpriv->bHWPwrPindetect) {
#ifdef CONFIG_AUTOSUSPEND
		if (adapt->registrypriv.usbss_enable) {
			if (pwrpriv->rf_pwrstate == rf_on) {
				if (adapt->net_closed)
					pwrpriv->ps_flag = true;

				rfpwrstate = RfOnOffDetect(adapt);
				RTW_INFO("@@@@- #1  %s==> rfstate:%s\n", __func__, (rfpwrstate == rf_on) ? "rf_on" : "rf_off");
				if (rfpwrstate != pwrpriv->rf_pwrstate) {
					if (rfpwrstate == rf_off) {
						pwrpriv->change_rfpwrstate = rf_off;

						pwrpriv->bkeepfwalive = true;
						pwrpriv->brfoffbyhw = true;

						autosuspend_enter(adapt);
					}
				}
			}
		} else
#endif /* CONFIG_AUTOSUSPEND */
		{
			rfpwrstate = RfOnOffDetect(adapt);
			RTW_INFO("@@@@- #2  %s==> rfstate:%s\n", __func__, (rfpwrstate == rf_on) ? "rf_on" : "rf_off");

			if (rfpwrstate != pwrpriv->rf_pwrstate) {
				if (rfpwrstate == rf_off) {
					pwrpriv->change_rfpwrstate = rf_off;
					pwrpriv->brfoffbyhw = true;
					rtw_hw_suspend(adapt);
				} else {
					pwrpriv->change_rfpwrstate = rf_on;
					rtw_hw_resume(adapt);
				}
				RTW_INFO("current rf_pwrstate(%s)\n", (pwrpriv->rf_pwrstate == rf_off) ? "rf_off" : "rf_on");
			}
		}
		pwrpriv->pwr_state_check_cnts++;
	}
#endif /* SUPPORT_HW_RFOFF_DETECTED */

	if (pwrpriv->ips_mode_req == IPS_NONE)
		goto exit;

	if (!rtw_pwr_unassociated_idle(adapt))
		goto exit;

	if ((pwrpriv->rf_pwrstate == rf_on) && ((adapt->pwr_state_check_cnts % 4) == 0)) {
		RTW_INFO("==>%s .fw_state(%x)\n", __func__, get_fwstate(pmlmepriv));
#if defined (CONFIG_AUTOSUSPEND)
#else
		pwrpriv->change_rfpwrstate = rf_off;
#endif
#ifdef CONFIG_AUTOSUSPEND
		if (adapt->registrypriv.usbss_enable) {
			if (pwrpriv->bHWPwrPindetect)
				pwrpriv->bkeepfwalive = true;

			if (adapt->net_closed)
				pwrpriv->ps_flag = true;

#if defined (CONFIG_AUTOSUSPEND)
			if (pwrpriv->bInternalAutoSuspend)
				RTW_INFO("<==%s .pwrpriv->bInternalAutoSuspend)(%x)\n", __func__, pwrpriv->bInternalAutoSuspend);
			else {
				pwrpriv->change_rfpwrstate = rf_off;
				RTW_INFO("<==%s .pwrpriv->bInternalAutoSuspend)(%x) call autosuspend_enter\n", __func__, pwrpriv->bInternalAutoSuspend);
				autosuspend_enter(adapt);
			}
#else
			autosuspend_enter(adapt);
#endif	/* if defined (CONFIG_AUTOSUSPEND) */
		} else if (pwrpriv->bHWPwrPindetect) {
		} else
#endif /* CONFIG_AUTOSUSPEND */
		{
#if defined (CONFIG_AUTOSUSPEND)
			pwrpriv->change_rfpwrstate = rf_off;
#endif	/* defined (CONFIG_AUTOSUSPEND) */

			ips_enter(adapt);
		}
	}
exit:
	rtw_set_pwr_state_check_timer(adapt);
	pwrpriv->ps_processing = false;
	return;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void pwr_state_check_handler(void *ctx)
#else
static void pwr_state_check_handler(struct timer_list *t)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	struct adapter *adapt = (struct adapter *)ctx;
#else
	struct adapter *adapt = from_timer(adapt, t, pwr_state_check_timer);
#endif

	rtw_ps_cmd(adapt);
}

void	traffic_check_for_leave_lps(struct adapter * adapt, u8 tx, u32 tx_packets)
{
#ifdef CONFIG_CHECK_LEAVE_LPS
	static unsigned long start_time = 0;
	static u32 xmit_cnt = 0;
	u8	bLeaveLPS = false;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;



	if (tx) { /* from tx */
		xmit_cnt += tx_packets;

		if (start_time == 0)
			start_time = rtw_get_current_time();

		if (rtw_get_passing_time_ms(start_time) > 2000) { /* 2 sec == watch dog timer */
			if (xmit_cnt > 8) {
				if ((adapter_to_pwrctl(adapt)->bLeisurePs) &&
				    (adapter_to_pwrctl(adapt)->pwr_mode != PS_MODE_ACTIVE) &&
				    (!rtw_btcoex_IsBtControlLps(adapt)))
					bLeaveLPS = true;
			}
			start_time = rtw_get_current_time();
			xmit_cnt = 0;
		}
	} else { /* from rx path */
		if (pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod > 4/*2*/) {
			if ((adapter_to_pwrctl(adapt)->bLeisurePs)
			    && (adapter_to_pwrctl(adapt)->pwr_mode != PS_MODE_ACTIVE)
			    && (!rtw_btcoex_IsBtControlLps(adapt))
			   ) {
				bLeaveLPS = true;
			}
		}
	}

	if (bLeaveLPS) {
		/* RTW_INFO("leave lps via %s, Tx = %d, Rx = %d\n", tx?"Tx":"Rx", pmlmepriv->LinkDetectInfo.NumTxOkInPeriod,pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod);	 */
		/* rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_LEAVE, 1); */
		rtw_lps_ctrl_wk_cmd(adapt, tx ? LPS_CTRL_TX_TRAFFIC_LEAVE : LPS_CTRL_RX_TRAFFIC_LEAVE, tx ? 0 : 1);
	}
#endif /* CONFIG_CHECK_LEAVE_LPS */
}

/*
 * Description:
 *	This function MUST be called under power lock protect
 *
 * Parameters
 *	adapt
 *	pslv			power state level, only could be PS_STATE_S0 ~ PS_STATE_S4
 *
 */
void rtw_set_rpwm(struct adapter * adapt, u8 pslv)
{
	u8	rpwm;
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);

	pslv = PS_STATE(pslv);

	if ((pwrpriv->rpwm == pslv) ||
	    (pwrpriv->lps_level == LPS_NORMAL))
		return;

	if (rtw_is_surprise_removed(adapt) ||
	    (!rtw_is_hw_init_completed(adapt))) {

		pwrpriv->cpwm = PS_STATE_S4;

		return;
	}

	if (rtw_is_drv_stopped(adapt))
		if (pslv < PS_STATE_S2)
			return;

	rpwm = pslv | pwrpriv->tog;

	pwrpriv->rpwm = pslv;

	rtw_hal_set_hwreg(adapt, HW_VAR_SET_RPWM, (u8 *)(&rpwm));

	pwrpriv->tog += 0x80;

	pwrpriv->cpwm = pslv;
}

static u8 PS_RDY_CHECK(struct adapter *adapt)
{
	u32 delta_ms;
	struct pwrctrl_priv	*pwrpriv = adapter_to_pwrctl(adapt);
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);

	if (pwrpriv->bInSuspend)
		return false;

	delta_ms = rtw_get_passing_time_ms(pwrpriv->DelayLPSLastTimeStamp);
	if (delta_ms < LPS_DELAY_MS)
		return false;

	if (check_fwstate(pmlmepriv, WIFI_SITE_MONITOR)
		|| check_fwstate(pmlmepriv, WIFI_UNDER_LINKING | WIFI_UNDER_WPS)
		|| MLME_IS_AP(adapt)
		|| MLME_IS_MESH(adapt)
		|| check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE | WIFI_ADHOC_STATE)
		|| rtw_cfg80211_get_is_roch(adapt)
		|| rtw_is_scan_deny(adapt))
		return false;

	if ((adapt->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) && (!adapt->securitypriv.binstallGrpkey)) {
		RTW_INFO("Group handshake still in progress !!!\n");
		return false;
	}

	if (!rtw_cfg80211_pwr_mgmt(adapt))
		return false;

	return true;
}

#if defined(CONFIG_FWLPS_IN_IPS)
void rtw_set_fw_in_ips_mode(struct adapter * adapt, u8 enable)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	int cnt = 0;
	unsigned long start_time;
	u8 val8 = 0;
	u8 cpwm_orig = 0, cpwm_now = 0;
	u8 parm[H2C_INACTIVE_PS_LEN] = {0};

	if (!adapt->netif_up) {
		RTW_INFO("%s: ERROR, netif is down\n", __func__);
		return;
	}

	/* u8 cmd_param; */ /* BIT0:enable, BIT1:NoConnect32k */
	if (enable) {
		rtw_btcoex_IpsNotify(adapt, pwrpriv->ips_mode_req);
		/* Enter IPS */
		RTW_INFO("%s: issue H2C to FW when entering IPS\n", __func__);

		parm[0] = 0x1;/* suggest by Isaac.Hsu*/

		rtw_hal_fill_h2c_cmd(adapt, /* H2C_FWLPS_IN_IPS_, */
				     H2C_INACTIVE_PS_,
				     H2C_INACTIVE_PS_LEN, parm);
		/* poll 0x1cc to make sure H2C command already finished by FW; MAC_0x1cc=0 means H2C done by FW. */
		do {
			val8 = rtw_read8(adapt, REG_HMETFR);
			cnt++;
			RTW_INFO("%s  polling REG_HMETFR=0x%x, cnt=%d\n",
				 __func__, val8, cnt);
			rtw_mdelay_os(10);
		} while (cnt < 100 && (val8 != 0));
	} else {
		/* Leave IPS */
		RTW_INFO("%s: Leaving IPS in FWLPS state\n", __func__);

		parm[0] = 0x0;
		parm[1] = 0x0;
		parm[2] = 0x0;
		rtw_hal_fill_h2c_cmd(adapt, H2C_INACTIVE_PS_,
				     H2C_INACTIVE_PS_LEN, parm);
		rtw_btcoex_IpsNotify(adapt, IPS_NONE);
	}
}
#endif

void rtw_set_ps_mode(struct adapter * adapt, u8 ps_mode, u8 smart_ps, u8 bcn_ant_mode, const char *msg)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	if (ps_mode > PM_Card_Disable) {
		return;
	}

	if (pwrpriv->pwr_mode == ps_mode) {
		if (PS_MODE_ACTIVE == ps_mode)
			return;
	}

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
	if (PS_MODE_ACTIVE != ps_mode) {
		rtw_set_ps_rsvd_page(adapt);
		rtw_set_default_port_id(adapt);
	}
#endif

	if (ps_mode == PS_MODE_ACTIVE) {
		if (1
		    && (((!rtw_btcoex_IsBtControlLps(adapt))
			 && (pwdinfo->opp_ps == 0))
			|| ((rtw_btcoex_IsBtControlLps(adapt))
			    && (!rtw_btcoex_IsLpsOn(adapt)))
		       )
		   ) {
			if (pwrpriv->lps_leave_cnts < UINT_MAX)
				pwrpriv->lps_leave_cnts++;
			else
				pwrpriv->lps_leave_cnts = 0;

			pwrpriv->pwr_mode = ps_mode;
			rtw_set_rpwm(adapt, PS_STATE_S4);

			rtw_hal_set_hwreg(adapt, HW_VAR_H2C_FW_PWRMODE, (u8 *)(&ps_mode));

			pwrpriv->bFwCurrentInPSMode = false;

			rtw_btcoex_LpsNotify(adapt, ps_mode);
		}
	} else {
		if ((PS_RDY_CHECK(adapt) && check_fwstate(&adapt->mlmepriv, WIFI_ASOC_STATE))
		    || ((rtw_btcoex_IsBtControlLps(adapt))
			&& (rtw_btcoex_IsLpsOn(adapt)))
		   ) {
			u8 pslv;

			if (pwrpriv->lps_enter_cnts < UINT_MAX)
				pwrpriv->lps_enter_cnts++;
			else
				pwrpriv->lps_enter_cnts = 0;
			rtw_btcoex_LpsNotify(adapt, ps_mode);

			pwrpriv->bFwCurrentInPSMode = true;
			pwrpriv->pwr_mode = ps_mode;
			pwrpriv->smart_ps = smart_ps;
			pwrpriv->bcn_ant_mode = bcn_ant_mode;

			rtw_hal_set_hwreg(adapt, HW_VAR_H2C_FW_PWRMODE, (u8 *)(&ps_mode));

			/* Set CTWindow after LPS */
			if (pwdinfo->opp_ps == 1)
				p2p_ps_wk_cmd(adapt, P2P_PS_ENABLE, 0);

			pslv = PS_STATE_S2;
			if ((!rtw_btcoex_IsBtDisabled(adapt))
			    && (rtw_btcoex_IsBtControlLps(adapt))) {
				u8 val8;

				val8 = rtw_btcoex_LpsVal(adapt);
				if (val8 & BIT(4))
					pslv = PS_STATE_S2;

			}
			rtw_set_rpwm(adapt, pslv);
		}
	}
}

/*
 * Return:
 *	0:	Leave OK
 *	-1:	Timeout
 *	-2:	Other error
 */
int LPS_RF_ON_check(struct adapter * adapt, u32 delay_ms)
{
	unsigned long start_time;
	u8 bAwake = false;
	int err = 0;


	start_time = rtw_get_current_time();
	while (1) {
		rtw_hal_get_hwreg(adapt, HW_VAR_FWLPS_RF_ON, &bAwake);
		if (bAwake)
			break;

		if (rtw_is_surprise_removed(adapt)) {
			err = -2;
			RTW_INFO("%s: device surprise removed!!\n", __func__);
			break;
		}

		if (rtw_get_passing_time_ms(start_time) > delay_ms) {
			err = -1;
			RTW_INFO("%s: Wait for FW LPS leave more than %u ms!!!\n", __func__, delay_ms);
			break;
		}
		rtw_usleep_os(100);
	}

	return err;
}

/*
 *	Description:
 *		Enter the leisure power save mode.
 *   */
void LPS_Enter(struct adapter * adapt, const char *msg)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct pwrctrl_priv	*pwrpriv = dvobj_to_pwrctl(dvobj);
	int n_assoc_iface = 0;
	int i;
	char buf[32] = {0};


	/*	RTW_INFO("+LeisurePSEnter\n"); */
	if (!GET_HAL_DATA(adapt)->bFWReady)
		return;

	if (rtw_btcoex_IsBtControlLps(adapt))
		return;

	/* Skip lps enter request if number of assocated adapters is not 1 */
	for (i = 0; i < dvobj->iface_nums; i++) {
		if (check_fwstate(&(dvobj->adapters[i]->mlmepriv), WIFI_ASOC_STATE))
			n_assoc_iface++;
	}
	if (n_assoc_iface != 1)
		return;

#ifndef CONFIG_FW_MULTI_PORT_SUPPORT
	/* Skip lps enter request for adapter not port0 */
	if (get_hw_port(adapt) != HW_PORT0)
		return;
#endif

	for (i = 0; i < dvobj->iface_nums; i++) {
		if (!PS_RDY_CHECK(dvobj->adapters[i]))
			return;
	}

	if (adapt->wdinfo.p2p_ps_mode == P2P_PS_NOA) {
		return;/* supporting p2p client ps NOA via H2C_8723B_P2P_PS_OFFLOAD */
	}

	if (pwrpriv->bLeisurePs) {
		/* Idle for a while if we connect to AP a while ago. */
		if (pwrpriv->LpsIdleCount >= 2) { /* 4 Sec */
			if (pwrpriv->pwr_mode == PS_MODE_ACTIVE) {
				sprintf(buf, "WIFI-%s", msg);
				pwrpriv->bpower_saving = true;
				
				pwrpriv->pwr_saving_start_time = rtw_get_current_time();

				rtw_set_ps_mode(adapt, pwrpriv->power_mgnt, adapt->registrypriv.smart_ps, 0, buf);
			}
		} else {
			pwrpriv->LpsIdleCount++;
		}
	}
}

/*
 *	Description:
 *		Leave the leisure power save mode.
 *   */
void LPS_Leave(struct adapter * adapt, const char *msg)
{
#define LPS_LEAVE_TIMEOUT_MS 100

	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct pwrctrl_priv	*pwrpriv = dvobj_to_pwrctl(dvobj);
	char buf[32] = {0};

	if (rtw_btcoex_IsBtControlLps(adapt))
		return;

	if (pwrpriv->bLeisurePs) {
		if (pwrpriv->pwr_mode != PS_MODE_ACTIVE) {
			sprintf(buf, "WIFI-%s", msg);
			rtw_set_ps_mode(adapt, PS_MODE_ACTIVE, 0, 0, buf);

			pwrpriv->pwr_saving_time += rtw_get_passing_time_ms(pwrpriv->pwr_saving_start_time);

			if (pwrpriv->pwr_mode == PS_MODE_ACTIVE)
				LPS_RF_ON_check(adapt, LPS_LEAVE_TIMEOUT_MS);
		}
	}

	pwrpriv->bpower_saving = false;
}

void rtw_wow_lps_level_decide(struct adapter *adapter, u8 wow_en)
{
}

void LeaveAllPowerSaveModeDirect(struct adapter * Adapter)
{
	struct adapter * pri_adapt = GET_PRIMARY_ADAPTER(Adapter);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(Adapter);

	RTW_INFO("%s.....\n", __func__);

	if (rtw_is_surprise_removed(Adapter)) {
		RTW_INFO(FUNC_ADPT_FMT ": bSurpriseRemoved=true Skip!\n", FUNC_ADPT_ARG(Adapter));
		return;
	}

	if (rtw_mi_check_status(Adapter, MI_LINKED)) { /*connect*/

		if (pwrpriv->pwr_mode == PS_MODE_ACTIVE) {
			RTW_INFO("%s: Driver Already Leave LPS\n", __func__);
			return;
		}
		p2p_ps_wk_cmd(pri_adapt, P2P_PS_DISABLE, 0);

		rtw_lps_ctrl_wk_cmd(pri_adapt, LPS_CTRL_LEAVE, 0);
	} else {
		if (pwrpriv->rf_pwrstate == rf_off) {
#ifdef CONFIG_AUTOSUSPEND
			if (Adapter->registrypriv.usbss_enable) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
				usb_disable_autosuspend(adapter_to_dvobj(Adapter)->pusbdev);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 34))
				adapter_to_dvobj(Adapter)->pusbdev->autosuspend_disabled = Adapter->bDisableAutosuspend;/* autosuspend disabled by the user */
#endif
			} else
#endif
			{
#if defined(CONFIG_FWLPS_IN_IPS) || defined(CONFIG_SWLPS_IN_IPS)
				if (false == ips_leave(pri_adapt))
					RTW_INFO("======> ips_leave fail.............\n");
#endif
			}
		}
	}
}

/*
 * Description: Leave all power save mode: LPS, FwLPS, IPS if needed.
 * Move code to function by tynli. 2010.03.26.
 *   */
void LeaveAllPowerSaveMode(struct adapter * Adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(Adapter);
	u8	enqueue = 0;
	int n_assoc_iface = 0;
	int i;

	if (false == Adapter->bup) {
		RTW_INFO(FUNC_ADPT_FMT ": bup=%d Skip!\n",
			 FUNC_ADPT_ARG(Adapter), Adapter->bup);
		return;
	}

	if (rtw_is_surprise_removed(Adapter)) {
		RTW_INFO(FUNC_ADPT_FMT ": bSurpriseRemoved=true Skip!\n", FUNC_ADPT_ARG(Adapter));
		return;
	}

	for (i = 0; i < dvobj->iface_nums; i++) {
		if (check_fwstate(&(dvobj->adapters[i]->mlmepriv), WIFI_ASOC_STATE))
			n_assoc_iface++;
	}

	if (n_assoc_iface) {
		/* connect */
		for (i = 0; i < dvobj->iface_nums; i++) {
			struct adapter *iface = dvobj->adapters[i];
			struct wifidirect_info *pwdinfo = &(iface->wdinfo);

			if (pwdinfo->p2p_ps_mode > P2P_PS_NONE)
				p2p_ps_wk_cmd(iface, P2P_PS_DISABLE, enqueue);
		}

		rtw_lps_ctrl_wk_cmd(Adapter, LPS_CTRL_LEAVE, enqueue);
	} else {
		if (adapter_to_pwrctl(Adapter)->rf_pwrstate == rf_off) {
#ifdef CONFIG_AUTOSUSPEND
			if (Adapter->registrypriv.usbss_enable) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
				usb_disable_autosuspend(adapter_to_dvobj(Adapter)->pusbdev);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 34))
				adapter_to_dvobj(Adapter)->pusbdev->autosuspend_disabled = Adapter->bDisableAutosuspend;/* autosuspend disabled by the user */
#endif
			} else
#endif
			{
#if defined(CONFIG_FWLPS_IN_IPS) || defined(CONFIG_SWLPS_IN_IPS)
				if (false == ips_leave(Adapter))
					RTW_INFO("======> ips_leave fail.............\n");
#endif /* CONFIG_SWLPS_IN_IPS */
			}
		}
	}
}

void rtw_init_pwrctrl_priv(struct adapter * adapt)
{
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

#if defined(CONFIG_CONCURRENT_MODE)
	if (adapt->adapter_type != PRIMARY_ADAPTER)
		return;
#endif

	_init_pwrlock(&pwrctrlpriv->lock);
	_init_pwrlock(&pwrctrlpriv->check_32k_lock);
	pwrctrlpriv->rf_pwrstate = rf_on;
	pwrctrlpriv->ips_enter_cnts = 0;
	pwrctrlpriv->ips_leave_cnts = 0;
	pwrctrlpriv->lps_enter_cnts = 0;
	pwrctrlpriv->lps_leave_cnts = 0;
	pwrctrlpriv->bips_processing = false;

	pwrctrlpriv->ips_mode = adapt->registrypriv.ips_mode;
	pwrctrlpriv->ips_mode_req = adapt->registrypriv.ips_mode;
	pwrctrlpriv->ips_deny_time = rtw_get_current_time();
	pwrctrlpriv->lps_level = adapt->registrypriv.lps_level;

	adapt->pwr_state_check_interval = RTW_PWR_STATE_CHK_INTERVAL;
	adapt->pwr_state_check_cnts = 0;
	#ifdef CONFIG_AUTOSUSPEND
	pwrctrlpriv->bInternalAutoSuspend = false;
	#endif
	pwrctrlpriv->bInSuspend = false;
	pwrctrlpriv->bkeepfwalive = false;

#ifdef CONFIG_AUTOSUSPEND
#ifdef SUPPORT_HW_RFOFF_DETECTED
	pwrctrlpriv->pwr_state_check_interval = (pwrctrlpriv->bHWPwrPindetect) ? 1000 : 2000;
#endif
#endif

	pwrctrlpriv->LpsIdleCount = 0;

	/* pwrctrlpriv->FWCtrlPSMode =adapt->registrypriv.power_mgnt; */ /* PS_MODE_MIN; */
	if (adapt->registrypriv.mp_mode == 1)
		pwrctrlpriv->power_mgnt = PS_MODE_ACTIVE ;
	else
		pwrctrlpriv->power_mgnt = adapt->registrypriv.power_mgnt; /* PS_MODE_MIN; */
	pwrctrlpriv->bLeisurePs = (PS_MODE_ACTIVE != pwrctrlpriv->power_mgnt) ? true : false;

	pwrctrlpriv->bFwCurrentInPSMode = false;

	pwrctrlpriv->rpwm = 0;
	pwrctrlpriv->cpwm = PS_STATE_S4;

	pwrctrlpriv->pwr_mode = PS_MODE_ACTIVE;
	pwrctrlpriv->smart_ps = adapt->registrypriv.smart_ps;
	pwrctrlpriv->bcn_ant_mode = 0;
	pwrctrlpriv->dtim = 0;

	pwrctrlpriv->tog = 0x80;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	timer_setup(&adapt->pwr_state_check_timer, pwr_state_check_handler, 0);
#else
	rtw_init_timer(&adapt->pwr_state_check_timer, adapt, pwr_state_check_handler, adapt);
#endif

	pwrctrlpriv->wowlan_mode = false;
	pwrctrlpriv->wowlan_ap_mode = false;
	pwrctrlpriv->wowlan_p2p_mode = false;
	pwrctrlpriv->wowlan_in_resume = false;
	pwrctrlpriv->wowlan_last_wake_reason = 0;
}

void rtw_free_pwrctrl_priv(struct adapter * adapter)
{
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapter);

#if defined(CONFIG_CONCURRENT_MODE)
	if (adapter->adapter_type != PRIMARY_ADAPTER)
		return;
#endif
	_free_pwrlock(&pwrctrlpriv->lock);
	_free_pwrlock(&pwrctrlpriv->check_32k_lock);
}

u8 rtw_interface_ps_func(struct adapter *adapt, enum hal_intf_ps_func efunc_id, u8 *val)
{
	u8 bResult = true;
	rtw_hal_intf_ps_func(adapt, efunc_id, val);

	return bResult;
}


inline void rtw_set_ips_deny(struct adapter *adapt, u32 ms)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	pwrpriv->ips_deny_time = rtw_get_current_time() + rtw_ms_to_systime(ms);
}

/*
* rtw_pwr_wakeup - Wake the NIC up from: 1)IPS. 2)USB autosuspend
* @adapter: pointer to struct adapter structure
* @ips_deffer_ms: the ms wiil prevent from falling into IPS after wakeup
* Return _SUCCESS or _FAIL
*/

int _rtw_pwr_wakeup(struct adapter *adapt, u32 ips_deffer_ms, const char *caller)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);
	struct mlme_priv *pmlmepriv;
	int ret = _SUCCESS;
	unsigned long start = rtw_get_current_time();

	/* for LPS */
	LeaveAllPowerSaveMode(adapt);

	/* IPS still bound with primary adapter */
	adapt = GET_PRIMARY_ADAPTER(adapt);
	pmlmepriv = &adapt->mlmepriv;

	if (time_after(rtw_get_current_time() + rtw_ms_to_systime(ips_deffer_ms), pwrpriv->ips_deny_time))
		pwrpriv->ips_deny_time = rtw_get_current_time() + rtw_ms_to_systime(ips_deffer_ms);


	if (pwrpriv->ps_processing) {
		RTW_INFO("%s wait ps_processing...\n", __func__);
		while (pwrpriv->ps_processing && rtw_get_passing_time_ms(start) <= 3000)
			rtw_msleep_os(10);
		if (pwrpriv->ps_processing)
			RTW_INFO("%s wait ps_processing timeout\n", __func__);
		else
			RTW_INFO("%s wait ps_processing done\n", __func__);
	}

	if (pwrpriv->bInSuspend
		#ifdef CONFIG_AUTOSUSPEND
		&& !pwrpriv->bInternalAutoSuspend
		#endif
		) {
		RTW_INFO("%s wait bInSuspend...\n", __func__);
		while (pwrpriv->bInSuspend
		       && ((rtw_get_passing_time_ms(start) <= 3000 && !rtw_is_do_late_resume(pwrpriv))
			|| (rtw_get_passing_time_ms(start) <= 500 && rtw_is_do_late_resume(pwrpriv)))
		      )
			rtw_msleep_os(10);
		if (pwrpriv->bInSuspend)
			RTW_INFO("%s wait bInSuspend timeout\n", __func__);
		else
			RTW_INFO("%s wait bInSuspend done\n", __func__);
	}

	/* System suspend is not allowed to wakeup */
	if ((pwrpriv->bInSuspend)
		#ifdef CONFIG_AUTOSUSPEND
		&& (!pwrpriv->bInternalAutoSuspend)
		#endif
	) {
		ret = _FAIL;
		goto exit;
	}
#ifdef CONFIG_AUTOSUSPEND
	/* usb autosuspend block??? */
	if ((pwrpriv->bInternalAutoSuspend)  && (adapt->net_closed)) {
		ret = _FAIL;
		goto exit;
	}
#endif
	/* I think this should be check in IPS, LPS, autosuspend functions... */
	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
#if defined (CONFIG_AUTOSUSPEND)
		if (pwrpriv->bInternalAutoSuspend) {
			if (0 == pwrpriv->autopm_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33))
				if (usb_autopm_get_interface(adapter_to_dvobj(adapt)->pusbintf) < 0)
					RTW_INFO("can't get autopm:\n");
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20))
				usb_autopm_disable(adapter_to_dvobj(adapt)->pusbintf);
#else
				usb_autoresume_device(adapter_to_dvobj(adapt)->pusbdev, 1);
#endif
				pwrpriv->autopm_cnt++;
			}
#endif	/* #if defined (CONFIG_AUTOSUSPEND) */
			ret = _SUCCESS;
			goto exit;
#if defined (CONFIG_AUTOSUSPEND)
		}
#endif	/* #if defined (CONFIG_AUTOSUSPEND) */
	}

	if (rf_off == pwrpriv->rf_pwrstate) {
#ifdef CONFIG_AUTOSUSPEND
		if (pwrpriv->brfoffbyhw) {
			RTW_INFO("hw still in rf_off state ...........\n");
			ret = _FAIL;
			goto exit;
		} else if (adapt->registrypriv.usbss_enable) {
			RTW_INFO("%s call autoresume_enter....\n", __func__);
			if (_FAIL ==  autoresume_enter(adapt)) {
				RTW_INFO("======> autoresume fail.............\n");
				ret = _FAIL;
				goto exit;
			}
		} else
#endif
		{
			RTW_INFO("%s call ips_leave....\n", __func__);
			if (_FAIL ==  ips_leave(adapt)) {
				RTW_INFO("======> ips_leave fail.............\n");
				ret = _FAIL;
				goto exit;
			}
		}
	}

	/* TODO: the following checking need to be merged... */
	if (rtw_is_drv_stopped(adapt)
	    || !adapt->bup
	    || !rtw_is_hw_init_completed(adapt)
	   ) {
		RTW_INFO("%s: bDriverStopped=%s, bup=%d, hw_init_completed=%u\n"
			 , caller
			 , rtw_is_drv_stopped(adapt) ? "True" : "False"
			 , adapt->bup
			 , rtw_get_hw_init_completed(adapt));
		ret = false;
		goto exit;
	}

exit:
	if (time_after(rtw_get_current_time() + rtw_ms_to_systime(ips_deffer_ms), pwrpriv->ips_deny_time))
		pwrpriv->ips_deny_time = rtw_get_current_time() + rtw_ms_to_systime(ips_deffer_ms);
	return ret;

}

int rtw_pm_set_lps(struct adapter *adapt, u8 mode)
{
	int	ret = 0;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	if (mode < PS_MODE_NUM) {
		if (pwrctrlpriv->power_mgnt != mode) {
			if (PS_MODE_ACTIVE == mode)
				LeaveAllPowerSaveMode(adapt);
			else
				pwrctrlpriv->LpsIdleCount = 2;
			pwrctrlpriv->power_mgnt = mode;
			pwrctrlpriv->bLeisurePs = (PS_MODE_ACTIVE != pwrctrlpriv->power_mgnt) ? true : false;
		}
	} else
		ret = -EINVAL;

	return ret;
}

int rtw_pm_set_lps_level(struct adapter *adapt, u8 level)
{
	int	ret = 0;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	if (level < LPS_LEVEL_MAX)
		pwrctrlpriv->lps_level = level;
	else
		ret = -EINVAL;

	return ret;
}

int rtw_pm_set_ips(struct adapter *adapt, u8 mode)
{
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	if (mode == IPS_NORMAL || mode == IPS_LEVEL_2) {
		rtw_ips_mode_req(pwrctrlpriv, mode);
		RTW_INFO("%s %s\n", __func__, mode == IPS_NORMAL ? "IPS_NORMAL" : "IPS_LEVEL_2");
		return 0;
	} else if (mode == IPS_NONE) {
		rtw_ips_mode_req(pwrctrlpriv, mode);
		RTW_INFO("%s %s\n", __func__, "IPS_NONE");
		if (!rtw_is_surprise_removed(adapt) && (_FAIL == rtw_pwr_wakeup(adapt)))
			return -EFAULT;
	} else
		return -EINVAL;
	return 0;
}

/*
 * ATTENTION:
 *	This function will request pwrctrl LOCK!
 */
void rtw_ps_deny(struct adapter * adapt, enum ps_deny_reason reason)
{
	struct pwrctrl_priv *pwrpriv;

	/* 	RTW_INFO("+" FUNC_ADPT_FMT ": Request PS deny for %d (0x%08X)\n",
	 *		FUNC_ADPT_ARG(adapt), reason, BIT(reason)); */

	pwrpriv = adapter_to_pwrctl(adapt);

	_enter_pwrlock(&pwrpriv->lock);
	if (pwrpriv->ps_deny & BIT(reason)) {
		RTW_INFO(FUNC_ADPT_FMT ": [WARNING] Reason %d had been set before!!\n",
			 FUNC_ADPT_ARG(adapt), reason);
	}
	pwrpriv->ps_deny |= BIT(reason);
	up(&pwrpriv->lock);

	/* 	RTW_INFO("-" FUNC_ADPT_FMT ": Now PS deny for 0x%08X\n",
	 *		FUNC_ADPT_ARG(adapt), pwrpriv->ps_deny); */
}

/*
 * ATTENTION:
 *	This function will request pwrctrl LOCK!
 */
void rtw_ps_deny_cancel(struct adapter * adapt, enum ps_deny_reason reason)
{
	struct pwrctrl_priv *pwrpriv;


	/* 	RTW_INFO("+" FUNC_ADPT_FMT ": Cancel PS deny for %d(0x%08X)\n",
	 *		FUNC_ADPT_ARG(adapt), reason, BIT(reason)); */

	pwrpriv = adapter_to_pwrctl(adapt);

	_enter_pwrlock(&pwrpriv->lock);
	if ((pwrpriv->ps_deny & BIT(reason)) == 0) {
		RTW_INFO(FUNC_ADPT_FMT ": [ERROR] Reason %d had been canceled before!!\n",
			 FUNC_ADPT_ARG(adapt), reason);
	}
	pwrpriv->ps_deny &= ~BIT(reason);
	up(&pwrpriv->lock);

	/* 	RTW_INFO("-" FUNC_ADPT_FMT ": Now PS deny for 0x%08X\n",
	 *		FUNC_ADPT_ARG(adapt), pwrpriv->ps_deny); */
}

/*
 * ATTENTION:
 *	Before calling this function pwrctrl lock should be occupied already,
 *	otherwise it may return incorrect value.
 */
u32 rtw_ps_deny_get(struct adapter * adapt)
{
	u32 deny;


	deny = adapter_to_pwrctl(adapt)->ps_deny;

	return deny;
}
