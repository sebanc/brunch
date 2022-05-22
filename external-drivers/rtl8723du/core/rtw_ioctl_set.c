// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_IOCTL_SET_C_

#include <drv_types.h>
#include <hal_data.h>


extern void indicate_wx_scan_complete_event(struct adapter *adapt);

#define IS_MAC_ADDRESS_BROADCAST(addr) \
	(\
	 ((addr[0] == 0xff) && (addr[1] == 0xff) && \
	  (addr[2] == 0xff) && (addr[3] == 0xff) && \
	  (addr[4] == 0xff) && (addr[5] == 0xff)) ? true : false \
	)

u8 rtw_validate_bssid(u8 *bssid)
{
	u8 ret = true;

	if (is_zero_mac_addr(bssid)
	    || is_broadcast_mac_addr(bssid)
	    || is_multicast_mac_addr(bssid)
	   )
		ret = false;

	return ret;
}

u8 rtw_validate_ssid(struct ndis_802_11_ssid *ssid)
{
#ifdef CONFIG_VALIDATE_SSID
	u8	 i;
#endif
	u8	ret = true;


	if (ssid->SsidLength > 32) {
		ret = false;
		goto exit;
	}

#ifdef CONFIG_VALIDATE_SSID
	for (i = 0; i < ssid->SsidLength; i++) {
		/* wifi, printable ascii code must be supported */
		if (!((ssid->Ssid[i] >= 0x20) && (ssid->Ssid[i] <= 0x7e))) {
			ret = false;
			break;
		}
	}
#endif /* CONFIG_VALIDATE_SSID */

exit:


	return ret;
}

u8 rtw_do_join(struct adapter *adapt);
u8 rtw_do_join(struct adapter *adapt)
{
	unsigned long	irqL;
	struct list_head	*plist, *phead;
	u8 *pibss = NULL;
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct sitesurvey_parm parm;
	struct __queue	*queue	= &(pmlmepriv->scanned_queue);
	u8 ret = _SUCCESS;


	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
	phead = get_list_head(queue);
	plist = get_next(phead);


	pmlmepriv->cur_network.join_res = -2;

	set_fwstate(pmlmepriv, _FW_UNDER_LINKING);

	pmlmepriv->pscanned = plist;

	pmlmepriv->to_join = true;

	rtw_init_sitesurvey_parm(adapt, &parm);
	memcpy(&parm.ssid[0], &pmlmepriv->assoc_ssid, sizeof(struct ndis_802_11_ssid));
	parm.ssid_num = 1;

	if (_rtw_queue_empty(queue)) {
		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);

		/* when set_ssid/set_bssid for rtw_do_join(), but scanning queue is empty */
		/* we try to issue sitesurvey firstly	 */

		if (!pmlmepriv->LinkDetectInfo.bBusyTraffic
		    || rtw_to_roam(adapt) > 0
		   ) {
			/* submit site_survey_cmd */
			ret = rtw_sitesurvey_cmd(adapt, &parm);
			if (_SUCCESS != ret) {
				pmlmepriv->to_join = false;
			}
		} else {
			pmlmepriv->to_join = false;
			ret = _FAIL;
		}

		goto exit;
	} else {
		int select_ret;
		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		select_ret = rtw_select_and_join_from_scanned_queue(pmlmepriv);
		if (select_ret == _SUCCESS) {
			pmlmepriv->to_join = false;
			_set_timer(&pmlmepriv->assoc_timer, MAX_JOIN_TIMEOUT);
		} else {
			if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) {
				/* submit createbss_cmd to change to a ADHOC_MASTER */

				/* pmlmepriv->lock has been acquired by caller... */
				struct wlan_bssid_ex    *pdev_network = &(adapt->registrypriv.dev_network);

				/*pmlmepriv->fw_state = WIFI_ADHOC_MASTER_STATE;*/
				init_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE);

				pibss = adapt->registrypriv.dev_network.MacAddress;

				memset(&pdev_network->Ssid, 0, sizeof(struct ndis_802_11_ssid));
				memcpy(&pdev_network->Ssid, &pmlmepriv->assoc_ssid, sizeof(struct ndis_802_11_ssid));

				rtw_update_registrypriv_dev_network(adapt);

				rtw_generate_random_ibss(pibss);

				if (rtw_create_ibss_cmd(adapt, 0) != _SUCCESS) {
					ret =  false;
					goto exit;
				}

				pmlmepriv->to_join = false;


			} else {
				/* can't associate ; reset under-linking			 */
				_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);
				/* when set_ssid/set_bssid for rtw_do_join(), but there are no desired bss in scanning queue */
				/* we try to issue sitesurvey firstly			 */
				if (!pmlmepriv->LinkDetectInfo.bBusyTraffic
				    || rtw_to_roam(adapt) > 0
				   ) {
					/* RTW_INFO("rtw_do_join() when   no desired bss in scanning queue\n"); */
					ret = rtw_sitesurvey_cmd(adapt, &parm);
					if (_SUCCESS != ret) {
						pmlmepriv->to_join = false;
					}
				} else {
					ret = _FAIL;
					pmlmepriv->to_join = false;
				}
			}

		}

	}

exit:

	return ret;
}

u8 rtw_set_802_11_bssid(struct adapter *adapt, u8 *bssid)
{
	unsigned long irqL;
	u8 status = _SUCCESS;

	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;


	RTW_PRINT("set bssid:%pM\n", bssid);

	if ((bssid[0] == 0x00 && bssid[1] == 0x00 && bssid[2] == 0x00 && bssid[3] == 0x00 && bssid[4] == 0x00 && bssid[5] == 0x00) ||
	    (bssid[0] == 0xFF && bssid[1] == 0xFF && bssid[2] == 0xFF && bssid[3] == 0xFF && bssid[4] == 0xFF && bssid[5] == 0xFF)) {
		status = _FAIL;
		goto exit;
	}

	_enter_critical_bh(&pmlmepriv->lock, &irqL);


	RTW_INFO("Set BSSID under fw_state=0x%08x\n", get_fwstate(pmlmepriv));
	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		goto handle_tkip_countermeasure;
	else if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING))
		goto release_mlme_lock;

	if (check_fwstate(pmlmepriv, _FW_LINKED | WIFI_ADHOC_MASTER_STATE)) {

		if (!memcmp(&pmlmepriv->cur_network.network.MacAddress, bssid, ETH_ALEN)) {
			if (!check_fwstate(pmlmepriv, WIFI_STATION_STATE))
				goto release_mlme_lock;/* it means driver is in WIFI_ADHOC_MASTER_STATE, we needn't create bss again. */
		} else {

			rtw_disassoc_cmd(adapt, 0, 0);

			if (check_fwstate(pmlmepriv, _FW_LINKED))
				rtw_indicate_disconnect(adapt, 0, false);

			rtw_free_assoc_resources(adapt, 1);

			if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) {
				_clr_fwstate_(pmlmepriv, WIFI_ADHOC_MASTER_STATE);
				set_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
			}
		}
	}

handle_tkip_countermeasure:
	if (rtw_handle_tkip_countermeasure(adapt, __func__) == _FAIL) {
		status = _FAIL;
		goto release_mlme_lock;
	}

	memset(&pmlmepriv->assoc_ssid, 0, sizeof(struct ndis_802_11_ssid));
	memcpy(&pmlmepriv->assoc_bssid, bssid, ETH_ALEN);
	pmlmepriv->assoc_by_bssid = true;

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		pmlmepriv->to_join = true;
	else
		status = rtw_do_join(adapt);

release_mlme_lock:
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

exit:


	return status;
}

u8 rtw_set_802_11_ssid(struct adapter *adapt, struct ndis_802_11_ssid *ssid)
{
	unsigned long irqL;
	u8 status = _SUCCESS;

	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct wlan_network *pnetwork = &pmlmepriv->cur_network;


	RTW_PRINT("set ssid [%s] fw_state=0x%08x\n",
		  ssid->Ssid, get_fwstate(pmlmepriv));

	if (!rtw_is_hw_init_completed(adapt)) {
		status = _FAIL;
		goto exit;
	}

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	RTW_INFO("Set SSID under fw_state=0x%08x\n", get_fwstate(pmlmepriv));
	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		goto handle_tkip_countermeasure;
	else if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING))
		goto release_mlme_lock;

	if (check_fwstate(pmlmepriv, _FW_LINKED | WIFI_ADHOC_MASTER_STATE)) {

		if ((pmlmepriv->assoc_ssid.SsidLength == ssid->SsidLength) &&
		    (!memcmp(&pmlmepriv->assoc_ssid.Ssid, ssid->Ssid, ssid->SsidLength))) {
			if ((!check_fwstate(pmlmepriv, WIFI_STATION_STATE))) {

				if (!rtw_is_same_ibss(adapt, pnetwork)) {
					/* if in WIFI_ADHOC_MASTER_STATE | WIFI_ADHOC_STATE, create bss or rejoin again */
					rtw_disassoc_cmd(adapt, 0, 0);

					if (check_fwstate(pmlmepriv, _FW_LINKED))
						rtw_indicate_disconnect(adapt, 0, false);

					rtw_free_assoc_resources(adapt, 1);

					if (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) {
						_clr_fwstate_(pmlmepriv, WIFI_ADHOC_MASTER_STATE);
						set_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
					}
				} else {
					goto release_mlme_lock;/* it means driver is in WIFI_ADHOC_MASTER_STATE, we needn't create bss again. */
				}
			} else {
				rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_JOINBSS, 1);
			}
		} else {

			rtw_disassoc_cmd(adapt, 0, 0);

			if (check_fwstate(pmlmepriv, _FW_LINKED))
				rtw_indicate_disconnect(adapt, 0, false);

			rtw_free_assoc_resources(adapt, 1);

			if (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) {
				_clr_fwstate_(pmlmepriv, WIFI_ADHOC_MASTER_STATE);
				set_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
			}
		}
	}

handle_tkip_countermeasure:
	if (rtw_handle_tkip_countermeasure(adapt, __func__) == _FAIL) {
		status = _FAIL;
		goto release_mlme_lock;
	}

	if (!rtw_validate_ssid(ssid)) {
		status = _FAIL;
		goto release_mlme_lock;
	}

	memcpy(&pmlmepriv->assoc_ssid, ssid, sizeof(struct ndis_802_11_ssid));
	pmlmepriv->assoc_by_bssid = false;

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		pmlmepriv->to_join = true;
	else
		status = rtw_do_join(adapt);

release_mlme_lock:
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

exit:


	return status;

}

u8 rtw_set_802_11_connect(struct adapter *adapt, u8 *bssid, struct ndis_802_11_ssid *ssid)
{
	unsigned long irqL;
	u8 status = _SUCCESS;
	bool bssid_valid = true;
	bool ssid_valid = true;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;


	if (!ssid || !rtw_validate_ssid(ssid))
		ssid_valid = false;

	if (!bssid || !rtw_validate_bssid(bssid))
		bssid_valid = false;

	if (!ssid_valid && !bssid_valid) {
		RTW_INFO(FUNC_ADPT_FMT" ssid:%p, ssid_valid:%d, bssid:%p, bssid_valid:%d\n",
			FUNC_ADPT_ARG(adapt), ssid, ssid_valid, bssid, bssid_valid);
		status = _FAIL;
		goto exit;
	}

	if (!rtw_is_hw_init_completed(adapt)) {
		status = _FAIL;
		goto exit;
	}

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	RTW_PRINT(FUNC_ADPT_FMT"  fw_state=0x%08x\n",
		  FUNC_ADPT_ARG(adapt), get_fwstate(pmlmepriv));

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		goto handle_tkip_countermeasure;
	else if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING))
		goto release_mlme_lock;

handle_tkip_countermeasure:
	if (rtw_handle_tkip_countermeasure(adapt, __func__) == _FAIL) {
		status = _FAIL;
		goto release_mlme_lock;
	}

	if (ssid && ssid_valid)
		memcpy(&pmlmepriv->assoc_ssid, ssid, sizeof(struct ndis_802_11_ssid));
	else
		memset(&pmlmepriv->assoc_ssid, 0, sizeof(struct ndis_802_11_ssid));

	if (bssid && bssid_valid) {
		memcpy(&pmlmepriv->assoc_bssid, bssid, ETH_ALEN);
		pmlmepriv->assoc_by_bssid = true;
	} else
		pmlmepriv->assoc_by_bssid = false;

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY))
		pmlmepriv->to_join = true;
	else
		status = rtw_do_join(adapt);

release_mlme_lock:
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

exit:


	return status;
}

u8 rtw_set_802_11_infrastructure_mode(struct adapter *adapt,
			      enum ndis_802_11_network_infrastructure networktype)
{
	unsigned long irqL;
	struct	mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct	wlan_network	*cur_network = &pmlmepriv->cur_network;
	enum ndis_802_11_network_infrastructure *pold_state = &(cur_network->network.InfrastructureMode);
	u8 ap2sta_mode = false;
	u8 ret = true;

	if (*pold_state != networktype) {
		/* RTW_INFO("change mode, old_mode=%d, new_mode=%d, fw_state=0x%x\n", *pold_state, networktype, get_fwstate(pmlmepriv)); */

		if (*pold_state == Ndis802_11APMode) {
			/* change to other mode from Ndis802_11APMode			 */
			cur_network->join_res = -1;
			ap2sta_mode = true;
			stop_ap_mode(adapt);
		}

		_enter_critical_bh(&pmlmepriv->lock, &irqL);

		if ((check_fwstate(pmlmepriv, _FW_LINKED)) || (*pold_state == Ndis802_11IBSS))
			rtw_disassoc_cmd(adapt, 0, 0);

		if ((check_fwstate(pmlmepriv, _FW_LINKED)) ||
		    (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)))
			rtw_free_assoc_resources(adapt, 1);

		if ((*pold_state == Ndis802_11Infrastructure) || (*pold_state == Ndis802_11IBSS)) {
			if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				rtw_indicate_disconnect(adapt, 0, false); /*will clr Linked_state; before this function, we must have checked whether issue dis-assoc_cmd or not*/
			}
		}

		*pold_state = networktype;

		_clr_fwstate_(pmlmepriv, ~WIFI_NULL_STATE);

		switch (networktype) {
		case Ndis802_11IBSS:
			set_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
			break;

		case Ndis802_11Infrastructure:
			set_fwstate(pmlmepriv, WIFI_STATION_STATE);

			if (ap2sta_mode)
				rtw_init_bcmc_stainfo(adapt);
			break;

		case Ndis802_11APMode:
			set_fwstate(pmlmepriv, WIFI_AP_STATE);
			start_ap_mode(adapt);
			break;
		case Ndis802_11AutoUnknown:
		case Ndis802_11InfrastructureMax:
			break;
		case Ndis802_11Monitor:
			set_fwstate(pmlmepriv, WIFI_MONITOR_STATE);
			break;
		default:
			ret = false;
			rtw_warn_on(1);
		}
		_exit_critical_bh(&pmlmepriv->lock, &irqL);
	}
	return ret;
}


u8 rtw_set_802_11_disassociate(struct adapter *adapt)
{
	unsigned long irqL;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;


	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	if (check_fwstate(pmlmepriv, _FW_LINKED)) {

		rtw_disassoc_cmd(adapt, 0, 0);
		rtw_indicate_disconnect(adapt, 0, false);
		/* modify for CONFIG_IEEE80211W, none 11w can use it */
		rtw_free_assoc_resources_cmd(adapt);
		if (_FAIL == rtw_pwr_wakeup(adapt))
			RTW_INFO("%s(): rtw_pwr_wakeup fail !!!\n", __func__);
	}

	_exit_critical_bh(&pmlmepriv->lock, &irqL);


	return true;
}

u8 rtw_set_802_11_bssid_list_scan(struct adapter *adapt, struct sitesurvey_parm *pparm)
{
	unsigned long	irqL;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	u8	res = true;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	res = rtw_sitesurvey_cmd(adapt, pparm);
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

	return res;
}

u8 rtw_set_802_11_authentication_mode(struct adapter *adapt, enum ndis_802_11_authentication_mode authmode)
{
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	int res;
	u8 ret;



	psecuritypriv->ndisauthtype = authmode;


	if (psecuritypriv->ndisauthtype > 3)
		psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;

	res = rtw_set_auth(adapt, psecuritypriv);

	if (res == _SUCCESS)
		ret = true;
	else
		ret = false;


	return ret;
}

u8 rtw_set_802_11_add_wep(struct adapter *adapt, struct ndis_802_11_wep *wep)
{

	u8		bdefaultkey;
	u8		btransmitkey;
	int		keyid, res;
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	u8		ret = _SUCCESS;


	bdefaultkey = (wep->KeyIndex & 0x40000000) > 0 ? false : true; /* for ??? */
	btransmitkey = (wep->KeyIndex & 0x80000000) > 0 ? true  : false;	/* for ??? */
	keyid = wep->KeyIndex & 0x3fffffff;

	if (keyid >= 4) {
		ret = false;
		goto exit;
	}

	switch (wep->KeyLength) {
	case 5:
		psecuritypriv->dot11PrivacyAlgrthm = _WEP40_;
		break;
	case 13:
		psecuritypriv->dot11PrivacyAlgrthm = _WEP104_;
		break;
	default:
		psecuritypriv->dot11PrivacyAlgrthm = _NO_PRIVACY_;
		break;
	}


	memcpy(&(psecuritypriv->dot11DefKey[keyid].skey[0]), &(wep->KeyMaterial), wep->KeyLength);

	psecuritypriv->dot11DefKeylen[keyid] = wep->KeyLength;

	psecuritypriv->dot11PrivacyKeyIndex = keyid;


	res = rtw_set_key(adapt, psecuritypriv, keyid, 1, true);

	if (res == _FAIL)
		ret = false;
exit:


	return ret;

}

/*
* rtw_get_cur_max_rate -
* @adapter: pointer to struct adapter structure
*
* Return 0 or 100Kbps
*/
u16 rtw_get_cur_max_rate(struct adapter *adapter)
{
	int	i = 0;
	u16	rate = 0, max_rate = 0;
	struct mlme_priv	*pmlmepriv = &adapter->mlmepriv;
	struct wlan_bssid_ex	*pcur_bss = &pmlmepriv->cur_network.network;
	struct sta_info *psta = NULL;
	u8	short_GI = 0;
	u8	rf_type = 0;

	if (!check_fwstate(pmlmepriv, _FW_LINKED) &&
	    !check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
		return 0;

	psta = rtw_get_stainfo(&adapter->stapriv, get_bssid(pmlmepriv));
	if (!psta)
		return 0;

	short_GI = query_ra_short_GI(psta, rtw_get_tx_bw_mode(adapter, psta));

	if (is_supported_ht(psta->wireless_mode)) {
		rtw_hal_get_hwreg(adapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
		max_rate = rtw_mcs_rate(rf_type
			, (psta->cmn.bw_mode == CHANNEL_WIDTH_40) ? 1 : 0
			, short_GI
			, psta->htpriv.ht_cap.supp_mcs_set
		);
	} else {
		while ((pcur_bss->SupportedRates[i] != 0) && (pcur_bss->SupportedRates[i] != 0xFF)) {
			rate = pcur_bss->SupportedRates[i] & 0x7F;
			if (rate > max_rate)
				max_rate = rate;
			i++;
		}

		max_rate = max_rate * 10 / 2;
	}

	return max_rate;
}

/*
* rtw_set_scan_mode -
* @adapter: pointer to struct adapter structure
* @scan_mode:
*
* Return _SUCCESS or _FAIL
*/
int rtw_set_scan_mode(struct adapter *adapter, enum rt_scan_type scan_mode)
{
	if (scan_mode != SCAN_ACTIVE && scan_mode != SCAN_PASSIVE)
		return _FAIL;

	adapter->mlmepriv.scan_mode = scan_mode;

	return _SUCCESS;
}

/*
* rtw_set_channel_plan -
* @adapter: pointer to struct adapter structure
* @channel_plan:
*
* Return _SUCCESS or _FAIL
*/
int rtw_set_channel_plan(struct adapter *adapter, u8 channel_plan)
{
	/* handle by cmd_thread to sync with scan operation */
	return rtw_set_chplan_cmd(adapter, RTW_CMDF_WAIT_ACK, channel_plan, 1);
}

/*
* rtw_set_country -
* @adapter: pointer to struct adapter structure
* @country_code: string of country code
*
* Return _SUCCESS or _FAIL
*/
int rtw_set_country(struct adapter *adapter, const char *country_code)
{
#ifdef CONFIG_RTW_IOCTL_SET_COUNTRY
	return rtw_set_country_cmd(adapter, RTW_CMDF_WAIT_ACK, country_code, 1);
#else
	RTW_INFO("%s(): not applied\n", __func__);
	return _SUCCESS;
#endif
}

/*
* rtw_set_band -
* @adapter: pointer to struct adapter structure
* @band: band to set
*
* Return _SUCCESS or _FAIL
*/
int rtw_set_band(struct adapter *adapter, u8 band)
{
	if (rtw_band_valid(band)) {
		RTW_INFO(FUNC_ADPT_FMT" band:%d\n", FUNC_ADPT_ARG(adapter), band);
		adapter->setband = band;
		return _SUCCESS;
	}

	RTW_PRINT(FUNC_ADPT_FMT" band:%d fail\n", FUNC_ADPT_ARG(adapter), band);
	return _FAIL;
}
