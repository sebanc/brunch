// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#include <drv_types.h>
#include <hal_data.h>
#include <rtw_sreset.h>

void sreset_init_value(struct adapter *adapt)
{
}
void sreset_reset_value(struct adapter *adapt)
{
}

u8 sreset_get_wifi_status(struct adapter *adapt)
{
	return WIFI_STATUS_SUCCESS;
}

void sreset_set_wifi_error_status(struct adapter *adapt, u32 status)
{
}

void sreset_set_trigger_point(struct adapter *adapt, int tgp)
{
}

bool sreset_inprogress(struct adapter *adapt)
{
	return false;
}

static void sreset_restore_security_station(struct adapter *adapt)
{
	struct mlme_priv *mlmepriv = &adapt->mlmepriv;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct sta_info *psta;
	struct mlme_ext_info	*pmlmeinfo = &adapt->mlmeextpriv.mlmext_info;

	{
		u8 val8;

		if (pmlmeinfo->auth_algo == dot11AuthAlgrthm_8021X)
			val8 = 0xcc;
		else
			val8 = 0xcf;
		rtw_hal_set_hwreg(adapt, HW_VAR_SEC_CFG, (u8 *)(&val8));
	}

	if ((adapt->securitypriv.dot11PrivacyAlgrthm == _TKIP_) ||
	    (adapt->securitypriv.dot11PrivacyAlgrthm == _AES_)) {
		psta = rtw_get_stainfo(pstapriv, get_bssid(mlmepriv));
		if (!psta) {
			/* DEBUG_ERR( ("Set wpa_set_encryption: Obtain Sta_info fail\n")); */
		} else {
			/* pairwise key */
			rtw_setstakey_cmd(adapt, psta, UNICAST_KEY, false);
			/* group key */
			rtw_set_key(adapt, &adapt->securitypriv, adapt->securitypriv.dot118021XGrpKeyid, 0, false);
		}
	}
}

static void sreset_restore_network_station(struct adapter *adapt)
{
	struct mlme_priv *mlmepriv = &adapt->mlmepriv;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8 doiqk = false;

	rtw_setopmode_cmd(adapt, Ndis802_11Infrastructure, RTW_CMDF_DIRECTLY);

	{
		u8 threshold;
		/* TH=1 => means that invalidate usb rx aggregation */
		/* TH=0 => means that validate usb rx aggregation, use init value. */
		if (mlmepriv->htpriv.ht_option) {
			if (adapt->registrypriv.wifi_spec == 1)
				threshold = 1;
			else
				threshold = 0;
			rtw_hal_set_hwreg(adapt, HW_VAR_RXDMA_AGG_PG_TH, (u8 *)(&threshold));
		} else {
			threshold = 1;
			rtw_hal_set_hwreg(adapt, HW_VAR_RXDMA_AGG_PG_TH, (u8 *)(&threshold));
		}
	}

	doiqk = true;
	rtw_hal_set_hwreg(adapt, HW_VAR_DO_IQK , &doiqk);

	set_channel_bwmode(adapt, pmlmeext->cur_channel, pmlmeext->cur_ch_offset, pmlmeext->cur_bwmode);

	doiqk = false;
	rtw_hal_set_hwreg(adapt , HW_VAR_DO_IQK , &doiqk);
	/* disable dynamic functions, such as high power, DIG */
	/*rtw_phydm_func_disable_all(adapt);*/

	rtw_hal_set_hwreg(adapt, HW_VAR_BSSID, pmlmeinfo->network.MacAddress);

	{
		u8	join_type = 0;
		rtw_hal_set_hwreg(adapt, HW_VAR_MLME_JOIN, (u8 *)(&join_type));
		rtw_hal_rcr_set_chk_bssid(adapt, MLME_STA_CONNECTING);
	}

	Set_MSR(adapt, (pmlmeinfo->state & 0x3));

	mlmeext_joinbss_event_callback(adapt, 1);
	/* restore Sequence No. */
	rtw_hal_set_hwreg(adapt, HW_VAR_RESTORE_HW_SEQ, NULL);

	sreset_restore_security_station(adapt);
}


static void sreset_restore_network_status(struct adapter *adapt)
{
	struct mlme_priv *mlmepriv = &adapt->mlmepriv;

	if (check_fwstate(mlmepriv, WIFI_STATION_STATE)) {
		RTW_INFO(FUNC_ADPT_FMT" fwstate:0x%08x - WIFI_STATION_STATE\n", FUNC_ADPT_ARG(adapt), get_fwstate(mlmepriv));
		sreset_restore_network_station(adapt);
	} else if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt)) {
		RTW_INFO(FUNC_ADPT_FMT" %s\n", FUNC_ADPT_ARG(adapt), MLME_IS_AP(adapt) ? "AP" : "MESH");
		rtw_ap_restore_network(adapt);
	} else if (check_fwstate(mlmepriv, WIFI_ADHOC_STATE))
		RTW_INFO(FUNC_ADPT_FMT" fwstate:0x%08x - WIFI_ADHOC_STATE\n", FUNC_ADPT_ARG(adapt), get_fwstate(mlmepriv));
	else
		RTW_INFO(FUNC_ADPT_FMT" fwstate:0x%08x - ???\n", FUNC_ADPT_ARG(adapt), get_fwstate(mlmepriv));
}

void sreset_stop_adapter(struct adapter *adapt)
{
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;

	if (!adapt)
		return;

	RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapt));

	rtw_netif_stop_queue(adapt->pnetdev);

	rtw_cancel_all_timer(adapt);

	tasklet_kill(&pxmitpriv->xmit_tasklet);

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		rtw_scan_abort(adapt);

	if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING)) {
		rtw_set_to_roam(adapt, 0);
		rtw_join_timeout_handler(&adapt->pwr_state_check_timer);
	}

}

void sreset_start_adapter(struct adapter *adapt)
{
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;

	if (!adapt)
		return;

	RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapt));

	if (check_fwstate(pmlmepriv, _FW_LINKED))
		sreset_restore_network_status(adapt);

	tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);

	if (is_primary_adapter(adapt))
		_set_timer(&adapter_to_dvobj(adapt)->dynamic_chk_timer, 2000);

	rtw_netif_wake_queue(adapt->pnetdev);
}

void sreset_reset(struct adapter *adapt)
{
}
