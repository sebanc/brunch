// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_AP_C_

#include <drv_types.h>
#include <hal_data.h>

extern unsigned char	RTW_WPA_OUI[];
extern unsigned char	WMM_OUI[];
extern unsigned char	WPS_OUI[];
extern unsigned char	P2P_OUI[];
extern unsigned char	WFD_OUI[];

void init_mlme_ap_info(struct adapter *adapt)
{
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	spin_lock_init(&pmlmepriv->bcn_update_lock);
}

void free_mlme_ap_info(struct adapter *adapt)
{
	stop_ap_mode(adapt);
}

/*
* Set TIM IE
* return length of total TIM IE
*/
u8 rtw_set_tim_ie(u8 dtim_cnt, u8 dtim_period
	, const u8 *tim_bmp, u8 tim_bmp_len, u8 *tim_ie)
{
	u8 *p = tim_ie;
	u8 i, n1, n2;
	u8 bmp_len;

	if (rtw_bmp_not_empty(tim_bmp, tim_bmp_len)) {
		/* find the first nonzero octet in tim_bitmap */
		for (i = 0; i < tim_bmp_len; i++)
			if (tim_bmp[i])
				break;
		n1 = i & 0xFE;
	
		/* find the last nonzero octet in tim_bitmap, except octet 0 */
		for (i = tim_bmp_len - 1; i > 0; i--)
			if (tim_bmp[i])
				break;
		n2 = i;
		bmp_len = n2 - n1 + 1;
	} else {
		n1 = n2 = 0;
		bmp_len = 1;
	}

	*p++ = WLAN_EID_TIM;
	*p++ = 2 + 1 + bmp_len;
	*p++ = dtim_cnt;
	*p++ = dtim_period;
	*p++ = (rtw_bmp_is_set(tim_bmp, tim_bmp_len, 0) ? BIT0 : 0) | n1;
	memcpy(p, tim_bmp + n1, bmp_len);

	return 2 + 2 + 1 + bmp_len;
}

static void update_BCNTIM(struct adapter *adapt)
{
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct mlme_ext_priv *pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex *pnetwork_mlmeext = &(pmlmeinfo->network);
	unsigned char *pie = pnetwork_mlmeext->IEs;

	if (true) {
		u8 *p, *dst_ie, *premainder_ie = NULL, *pbackup_remainder_ie = NULL;
		uint offset, tmp_len, tim_ielen, tim_ie_offset, remainder_ielen;

		p = rtw_get_ie(pie + _FIXED_IE_LENGTH_, _TIM_IE_, &tim_ielen, pnetwork_mlmeext->IELength - _FIXED_IE_LENGTH_);
		if (p && tim_ielen > 0) {
			tim_ielen += 2;

			premainder_ie = p + tim_ielen;

			tim_ie_offset = (int)(p - pie);

			remainder_ielen = pnetwork_mlmeext->IELength - tim_ie_offset - tim_ielen;

			/*append TIM IE from dst_ie offset*/
			dst_ie = p;
		} else {
			tim_ielen = 0;

			/*calculate head_len*/
			offset = _FIXED_IE_LENGTH_;

			/* get ssid_ie len */
			p = rtw_get_ie(pie + _BEACON_IE_OFFSET_, _SSID_IE_, &tmp_len, (pnetwork_mlmeext->IELength - _BEACON_IE_OFFSET_));
			if (p)
				offset += tmp_len + 2;

			/*get supported rates len*/
			p = rtw_get_ie(pie + _BEACON_IE_OFFSET_, _SUPPORTEDRATES_IE_, &tmp_len, (pnetwork_mlmeext->IELength - _BEACON_IE_OFFSET_));
			if (p !=  NULL)
				offset += tmp_len + 2;

			/*DS Parameter Set IE, len=3*/
			offset += 3;

			premainder_ie = pie + offset;

			remainder_ielen = pnetwork_mlmeext->IELength - offset - tim_ielen;

			/*append TIM IE from offset*/
			dst_ie = pie + offset;

		}

		if (remainder_ielen > 0) {
			pbackup_remainder_ie = rtw_malloc(remainder_ielen);
			if (pbackup_remainder_ie && premainder_ie)
				memcpy(pbackup_remainder_ie, premainder_ie, remainder_ielen);
		}

		/* append TIM IE */
		dst_ie += rtw_set_tim_ie(0, 1, pstapriv->tim_bitmap, pstapriv->aid_bmp_len, dst_ie);

		/*copy remainder IE*/
		if (pbackup_remainder_ie) {
			memcpy(dst_ie, pbackup_remainder_ie, remainder_ielen);

			rtw_mfree(pbackup_remainder_ie, remainder_ielen);
		}

		offset = (uint)(dst_ie - pie);
		pnetwork_mlmeext->IELength = offset + remainder_ielen;

	}
}

void rtw_add_bcn_ie(struct adapter *adapt, struct wlan_bssid_ex *pnetwork, u8 index, u8 *data, u8 len)
{
	struct ndis_802_11_variable_ies *	pIE;
	u8	bmatch = false;
	u8	*pie = pnetwork->IEs;
	u8	*p = NULL, *dst_ie = NULL, *premainder_ie = NULL, *pbackup_remainder_ie = NULL;
	u32	i, offset, ielen = 0, ie_offset, remainder_ielen = 0;

	for (i = sizeof(struct ndis_802_11_fixed_ies); i < pnetwork->IELength;) {
		pIE = (struct ndis_802_11_variable_ies *)(pnetwork->IEs + i);

		if (pIE->ElementID > index)
			break;
		else if (pIE->ElementID == index) { /* already exist the same IE */
			p = (u8 *)pIE;
			ielen = pIE->Length;
			bmatch = true;
			break;
		}

		p = (u8 *)pIE;
		ielen = pIE->Length;
		i += (pIE->Length + 2);
	}

	if (p && ielen > 0) {
		ielen += 2;

		premainder_ie = p + ielen;

		ie_offset = (int)(p - pie);

		remainder_ielen = pnetwork->IELength - ie_offset - ielen;

		if (bmatch)
			dst_ie = p;
		else
			dst_ie = (p + ielen);
	}

	if (!dst_ie)
		return;

	if (remainder_ielen > 0) {
		pbackup_remainder_ie = rtw_malloc(remainder_ielen);
		if (pbackup_remainder_ie && premainder_ie)
			memcpy(pbackup_remainder_ie, premainder_ie, remainder_ielen);
	}

	*dst_ie++ = index;
	*dst_ie++ = len;

	memcpy(dst_ie, data, len);
	dst_ie += len;

	/* copy remainder IE */
	if (pbackup_remainder_ie) {
		memcpy(dst_ie, pbackup_remainder_ie, remainder_ielen);

		rtw_mfree(pbackup_remainder_ie, remainder_ielen);
	}

	offset = (uint)(dst_ie - pie);
	pnetwork->IELength = offset + remainder_ielen;
}

void rtw_remove_bcn_ie(struct adapter *adapt, struct wlan_bssid_ex *pnetwork, u8 index)
{
	u8 *p, *dst_ie = NULL, *premainder_ie = NULL, *pbackup_remainder_ie = NULL;
	uint offset, ielen, ie_offset, remainder_ielen = 0;
	u8	*pie = pnetwork->IEs;

	p = rtw_get_ie(pie + _FIXED_IE_LENGTH_, index, &ielen, pnetwork->IELength - _FIXED_IE_LENGTH_);
	if (p && ielen > 0) {
		ielen += 2;

		premainder_ie = p + ielen;

		ie_offset = (int)(p - pie);

		remainder_ielen = pnetwork->IELength - ie_offset - ielen;

		dst_ie = p;
	} else
		return;

	if (remainder_ielen > 0) {
		pbackup_remainder_ie = rtw_malloc(remainder_ielen);
		if (pbackup_remainder_ie && premainder_ie)
			memcpy(pbackup_remainder_ie, premainder_ie, remainder_ielen);
	}

	/* copy remainder IE */
	if (pbackup_remainder_ie) {
		memcpy(dst_ie, pbackup_remainder_ie, remainder_ielen);

		rtw_mfree(pbackup_remainder_ie, remainder_ielen);
	}

	offset = (uint)(dst_ie - pie);
	pnetwork->IELength = offset + remainder_ielen;
}


u8 chk_sta_is_alive(struct sta_info *psta);
u8 chk_sta_is_alive(struct sta_info *psta)
{
	u8 ret = false;

	if ((psta->sta_stats.last_rx_data_pkts + psta->sta_stats.last_rx_ctrl_pkts) == (psta->sta_stats.rx_data_pkts + psta->sta_stats.rx_ctrl_pkts)) {
	} else
		ret = true;

	sta_update_last_rx_pkts(psta);

	return ret;
}

void	expire_timeout_chk(struct adapter *adapt)
{
	unsigned long irqL;
	struct list_head	*phead, *plist;
	u8 updated = false;
	struct sta_info *psta = NULL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	u8 chk_alive_num = 0;
	char chk_alive_list[NUM_STA];
	int i;


	_enter_critical_bh(&pstapriv->auth_list_lock, &irqL);

	phead = &pstapriv->auth_list;
	plist = get_next(phead);

	/* check auth_queue */
	while ((!rtw_end_of_queue_search(phead, plist))) {
		psta = container_of(plist, struct sta_info, auth_list);

		plist = get_next(plist);

		if (psta->expire_to > 0) {
			psta->expire_to--;
			if (psta->expire_to == 0) {
				rtw_list_delete(&psta->auth_list);
				pstapriv->auth_list_cnt--;

				RTW_INFO("auth expire %02X%02X%02X%02X%02X%02X\n",
					psta->cmn.mac_addr[0], psta->cmn.mac_addr[1], psta->cmn.mac_addr[2],
					psta->cmn.mac_addr[3], psta->cmn.mac_addr[4], psta->cmn.mac_addr[5]);

				_exit_critical_bh(&pstapriv->auth_list_lock, &irqL);

				rtw_free_stainfo(adapt, psta);

				_enter_critical_bh(&pstapriv->auth_list_lock, &irqL);
			}
		}

	}

	_exit_critical_bh(&pstapriv->auth_list_lock, &irqL);
	psta = NULL;


	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	phead = &pstapriv->asoc_list;
	plist = get_next(phead);

	/* check asoc_queue */
	while ((!rtw_end_of_queue_search(phead, plist))) {
		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);
		if (chk_sta_is_alive(psta) || !psta->expire_to) {
			psta->expire_to = pstapriv->expire_to;
			psta->keep_alive_trycnt = 0;
			psta->under_exist_checking = 0;
		} else
			psta->expire_to--;

		if (psta->expire_to <= 0) {
			struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;

			if (adapt->registrypriv.wifi_spec == 1) {
				psta->expire_to = pstapriv->expire_to;
				continue;
			}

			if (psta->state & WIFI_SLEEP_STATE) {
				if (!(psta->state & WIFI_STA_ALIVE_CHK_STATE)) {
					/* to check if alive by another methods if staion is at ps mode.					 */
					psta->expire_to = pstapriv->expire_to;
					psta->state |= WIFI_STA_ALIVE_CHK_STATE;

					/* RTW_INFO("alive chk, sta:" MAC_FMT " is at ps mode!\n", MAC_ARG(psta->cmn.mac_addr)); */

					/* to update bcn with tim_bitmap for this station */
					rtw_tim_map_set(adapt, pstapriv->tim_bitmap, psta->cmn.aid);
					update_beacon(adapt, _TIM_IE_, NULL, true);

					if (!pmlmeext->active_keep_alive_check)
						continue;
				}
			}

			{
				int stainfo_offset;

				stainfo_offset = rtw_stainfo_offset(pstapriv, psta);
				if (stainfo_offset_valid(stainfo_offset))
					chk_alive_list[chk_alive_num++] = stainfo_offset;
				continue;
			}
		} else {
			/* TODO: Aging mechanism to digest frames in sleep_q to avoid running out of xmitframe */
			if (psta->sleepq_len > (NR_XMITFRAME / pstapriv->asoc_list_cnt)
			    && adapt->xmitpriv.free_xmitframe_cnt < ((NR_XMITFRAME / pstapriv->asoc_list_cnt) / 2)
			   ) {
				RTW_INFO("%s sta:"MAC_FMT", sleepq_len:%u, free_xmitframe_cnt:%u, asoc_list_cnt:%u, clear sleep_q\n", __func__
					 , MAC_ARG(psta->cmn.mac_addr)
					, psta->sleepq_len, adapt->xmitpriv.free_xmitframe_cnt, pstapriv->asoc_list_cnt);
				wakeup_sta_to_xmit(adapt, psta);
			}
		}
	}

	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	if (chk_alive_num) {
		u8 backup_ch = 0, backup_bw = 0, backup_offset = 0;
		u8 union_ch = 0, union_bw = 0, union_offset = 0;
		u8 switch_channel_by_drv = true;
		struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
		char del_asoc_list[NUM_STA];

		memset(del_asoc_list, NUM_STA, NUM_STA);

		if (pmlmeext->active_keep_alive_check) {
			if (!rtw_mi_get_ch_setting_union(adapt, &union_ch, &union_bw, &union_offset)
				|| pmlmeext->cur_channel != union_ch)
				switch_channel_by_drv = false;

			/* switch to correct channel of current network  before issue keep-alive frames */
			if (switch_channel_by_drv && rtw_get_oper_ch(adapt) != pmlmeext->cur_channel) {
				backup_ch = rtw_get_oper_ch(adapt);
				backup_bw = rtw_get_oper_bw(adapt);
				backup_offset = rtw_get_oper_choffset(adapt);
				set_channel_bwmode(adapt, union_ch, union_offset, union_bw);
			}
		}

		/* check loop */
		for (i = 0; i < chk_alive_num; i++) {
			int ret = _FAIL;

			psta = rtw_get_stainfo_by_offset(pstapriv, chk_alive_list[i]);

			if (!(psta->state & _FW_LINKED))
				continue;

			if (pmlmeext->active_keep_alive_check) {
				/* issue null data to check sta alive*/
				if (psta->state & WIFI_SLEEP_STATE)
					ret = issue_nulldata(adapt, psta->cmn.mac_addr, 0, 1, 50);
				else
					ret = issue_nulldata(adapt, psta->cmn.mac_addr, 0, 3, 50);

				psta->keep_alive_trycnt++;
				if (ret == _SUCCESS) {
					RTW_INFO("asoc check, sta(" MAC_FMT ") is alive\n", MAC_ARG(psta->cmn.mac_addr));
					psta->expire_to = pstapriv->expire_to;
					psta->keep_alive_trycnt = 0;
					continue;
				} else if (psta->keep_alive_trycnt <= 3) {
					RTW_INFO("ack check for asoc expire, keep_alive_trycnt=%d\n", psta->keep_alive_trycnt);
					psta->expire_to = 1;
					continue;
				}
			}

			psta->keep_alive_trycnt = 0;
			del_asoc_list[i] = chk_alive_list[i];
			_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
			if (!rtw_is_list_empty(&psta->asoc_list)) {
				rtw_list_delete(&psta->asoc_list);
				pstapriv->asoc_list_cnt--;
			}
			_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);
		}

		/* delete loop */
		for (i = 0; i < chk_alive_num; i++) {
			u8 sta_addr[ETH_ALEN];

			if (del_asoc_list[i] >= NUM_STA)
				continue;

			psta = rtw_get_stainfo_by_offset(pstapriv, del_asoc_list[i]);
			memcpy(sta_addr, psta->cmn.mac_addr, ETH_ALEN);

			RTW_INFO("asoc expire "MAC_FMT", state=0x%x\n", MAC_ARG(psta->cmn.mac_addr), psta->state);
			updated = ap_free_sta(adapt, psta, false, WLAN_REASON_DEAUTH_LEAVING, false);
		}

		if (pmlmeext->active_keep_alive_check) {
			/* back to the original operation channel */
			if (switch_channel_by_drv && backup_ch > 0)
				set_channel_bwmode(adapt, backup_ch, backup_offset, backup_bw);
		}
	}

	associated_clients_update(adapt, updated, STA_INFO_UPDATE_ALL);
}

void rtw_ap_update_sta_ra_info(struct adapter *adapt, struct sta_info *psta)
{
	unsigned char sta_band = 0;
	u64 tx_ra_bitmap = 0;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex *pcur_network = (struct wlan_bssid_ex *)&pmlmepriv->cur_network.network;

	if (!psta)
		return;

	if (!(psta->state & _FW_LINKED))
		return;

	rtw_hal_update_sta_ra_info(adapt, psta);
	tx_ra_bitmap = psta->cmn.ra_info.ramask;

	if (pcur_network->Configuration.DSConfig > 14) {

		if (tx_ra_bitmap & 0xffff000)
			sta_band |= WIRELESS_11_5N;

		if (tx_ra_bitmap & 0xff0)
			sta_band |= WIRELESS_11A;
	} else {
		if (tx_ra_bitmap & 0xffff000)
			sta_band |= WIRELESS_11_24N;

		if (tx_ra_bitmap & 0xff0)
			sta_band |= WIRELESS_11G;

		if (tx_ra_bitmap & 0x0f)
			sta_band |= WIRELESS_11B;
	}

	psta->wireless_mode = sta_band;
	rtw_hal_update_sta_wset(adapt, psta);
	RTW_INFO("%s=> mac_id:%d , tx_ra_bitmap:0x%016llx, networkType:0x%02x\n",
			__func__, psta->cmn.mac_id, tx_ra_bitmap, psta->wireless_mode);
}

#ifdef CONFIG_BMC_TX_RATE_SELECT
u8 rtw_ap_find_mini_tx_rate(struct adapter *adapter)
{
	unsigned long irqL;
	struct list_head	*phead, *plist;
	u8 miini_tx_rate = ODM_RATEVHTSS4MCS9, sta_tx_rate;
	struct sta_info *psta = NULL;
	struct sta_priv *pstapriv = &adapter->stapriv;

	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
	phead = &pstapriv->asoc_list;
	plist = get_next(phead);
	while ((!rtw_end_of_queue_search(phead, plist))) {
		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);

		sta_tx_rate = psta->cmn.ra_info.curr_tx_rate & 0x7F;
		if (sta_tx_rate < miini_tx_rate)
			miini_tx_rate = sta_tx_rate;
	}
	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	return miini_tx_rate;
}

u8 rtw_ap_find_bmc_rate(struct adapter *adapter, u8 tx_rate)
{
	struct hal_com_data *	hal_data = GET_HAL_DATA(adapter);
	u8 tx_ini_rate = ODM_RATE6M;

	switch (tx_rate) {
	case ODM_RATEVHTSS3MCS9:
	case ODM_RATEVHTSS3MCS8:
	case ODM_RATEVHTSS3MCS7:
	case ODM_RATEVHTSS3MCS6:
	case ODM_RATEVHTSS3MCS5:
	case ODM_RATEVHTSS3MCS4:
	case ODM_RATEVHTSS3MCS3:
	case ODM_RATEVHTSS2MCS9:
	case ODM_RATEVHTSS2MCS8:
	case ODM_RATEVHTSS2MCS7:
	case ODM_RATEVHTSS2MCS6:
	case ODM_RATEVHTSS2MCS5:
	case ODM_RATEVHTSS2MCS4:
	case ODM_RATEVHTSS2MCS3:
	case ODM_RATEVHTSS1MCS9:
	case ODM_RATEVHTSS1MCS8:
	case ODM_RATEVHTSS1MCS7:
	case ODM_RATEVHTSS1MCS6:
	case ODM_RATEVHTSS1MCS5:
	case ODM_RATEVHTSS1MCS4:
	case ODM_RATEVHTSS1MCS3:
	case ODM_RATEMCS15:
	case ODM_RATEMCS14:
	case ODM_RATEMCS13:
	case ODM_RATEMCS12:
	case ODM_RATEMCS11:
	case ODM_RATEMCS7:
	case ODM_RATEMCS6:
	case ODM_RATEMCS5:
	case ODM_RATEMCS4:
	case ODM_RATEMCS3:
	case ODM_RATE54M:
	case ODM_RATE48M:
	case ODM_RATE36M:
	case ODM_RATE24M:
		tx_ini_rate = ODM_RATE24M;
		break;
	case ODM_RATEVHTSS3MCS2:
	case ODM_RATEVHTSS3MCS1:
	case ODM_RATEVHTSS2MCS2:
	case ODM_RATEVHTSS2MCS1:
	case ODM_RATEVHTSS1MCS2:
	case ODM_RATEVHTSS1MCS1:
	case ODM_RATEMCS10:
	case ODM_RATEMCS9:
	case ODM_RATEMCS2:
	case ODM_RATEMCS1:
	case ODM_RATE18M:
	case ODM_RATE12M:
		tx_ini_rate = ODM_RATE12M;
		break;
	case ODM_RATEVHTSS3MCS0:
	case ODM_RATEVHTSS2MCS0:
	case ODM_RATEVHTSS1MCS0:
	case ODM_RATEMCS8:
	case ODM_RATEMCS0:
	case ODM_RATE9M:
	case ODM_RATE6M:
		tx_ini_rate = ODM_RATE6M;
		break;
	case ODM_RATE11M:
	case ODM_RATE5_5M:
	case ODM_RATE2M:
	case ODM_RATE1M:
		tx_ini_rate = ODM_RATE1M;
		break;
	default:
		tx_ini_rate = ODM_RATE6M;
		break;
	}

	if (hal_data->current_band_type == BAND_ON_5G)
		if (tx_ini_rate < ODM_RATE6M)
			tx_ini_rate = ODM_RATE6M;

	return tx_ini_rate;
}

void rtw_update_bmc_sta_tx_rate(struct adapter *adapter)
{
	struct sta_info *psta = NULL;
	u8 tx_rate;

	psta = rtw_get_bcmc_stainfo(adapter);
	if (!psta) {
		RTW_ERR(ADPT_FMT "could not get bmc_sta !!\n", ADPT_ARG(adapter));
		return;
	}

	if (adapter->bmc_tx_rate != MGN_UNKNOWN) {
		psta->init_rate = adapter->bmc_tx_rate;
		goto _exit;
	}

	if (adapter->stapriv.asoc_sta_count <= 2)
		goto _exit;

	tx_rate = rtw_ap_find_mini_tx_rate(adapter);
	#ifdef CONFIG_BMC_TX_LOW_RATE
	tx_rate = rtw_ap_find_bmc_rate(adapter, tx_rate);
	#endif

	psta->init_rate = hw_rate_to_m_rate(tx_rate);

_exit:
	RTW_INFO(ADPT_FMT" BMC Tx rate - %s\n", ADPT_ARG(adapter), MGN_RATE_STR(psta->init_rate));
}
#endif

static void rtw_init_bmc_sta_tx_rate(struct adapter *adapt, struct sta_info *psta)
{
	u8 rate_idx = 0;
	u8 brate_table[] = {MGN_1M, MGN_2M, MGN_5_5M, MGN_11M,
		MGN_6M, MGN_9M, MGN_12M, MGN_18M, MGN_24M, MGN_36M, MGN_48M, MGN_54M};

	if (!MLME_IS_AP(adapt) && !MLME_IS_MESH(adapt))
		return;

	if (adapt->bmc_tx_rate != MGN_UNKNOWN)
		psta->init_rate = adapt->bmc_tx_rate;
	else {
		#ifdef CONFIG_BMC_TX_LOW_RATE
		if (IsEnableHWOFDM(pmlmeext->cur_wireless_mode) && (psta->cmn.ra_info.ramask && 0xFF0))
			rate_idx = get_lowest_rate_idx_ex(psta->cmn.ra_info.ramask, 4); /*from basic rate*/
		else
			rate_idx = get_lowest_rate_idx(psta->cmn.ra_info.ramask); /*from basic rate*/
		#else
		rate_idx = get_highest_rate_idx(psta->cmn.ra_info.ramask); /*from basic rate*/
		#endif
		if (rate_idx < 12)
			psta->init_rate = brate_table[rate_idx];
		else
			psta->init_rate = MGN_1M;
	}

	RTW_INFO(ADPT_FMT" BMC Init Tx rate - %s\n", ADPT_ARG(adapt), MGN_RATE_STR(psta->init_rate));
}

void update_bmc_sta(struct adapter *adapt)
{
	unsigned long	irqL;
	unsigned char	network_type;
	int supportRateNum = 0;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex *pcur_network = (struct wlan_bssid_ex *)&pmlmepriv->cur_network.network;
	struct sta_info *psta = rtw_get_bcmc_stainfo(adapt);

	if (psta) {
		psta->cmn.aid = 0;/* default set to 0 */
		psta->qos_option = 0;
		psta->htpriv.ht_option = false;

		psta->ieee8021x_blocked = 0;

		memset((void *)&psta->sta_stats, 0, sizeof(struct stainfo_stats));

		/* psta->dot118021XPrivacy = _NO_PRIVACY_; */ /* !!! remove it, because it has been set before this. */

		supportRateNum = rtw_get_rateset_len((u8 *)&pcur_network->SupportedRates);
		network_type = rtw_check_network_type((u8 *)&pcur_network->SupportedRates, supportRateNum, pcur_network->Configuration.DSConfig);
		if (IsSupportedTxCCK(network_type))
			network_type = WIRELESS_11B;
		else if (network_type == WIRELESS_INVALID) { /* error handling */
			if (pcur_network->Configuration.DSConfig > 14)
				network_type = WIRELESS_11A;
			else
				network_type = WIRELESS_11B;
		}
		update_sta_basic_rate(psta, network_type);
		psta->wireless_mode = network_type;

		rtw_hal_update_sta_ra_info(adapt, psta);

		_enter_critical_bh(&psta->lock, &irqL);
		psta->state = _FW_LINKED;
		_exit_critical_bh(&psta->lock, &irqL);

		rtw_sta_media_status_rpt(adapt, psta, 1);
		rtw_init_bmc_sta_tx_rate(adapt, psta);

	} else
		RTW_INFO("add_RATid_bmc_sta error!\n");

}

/* notes:
 * AID: 1~MAX for sta and 0 for bc/mc in ap/adhoc mode  */
void update_sta_info_apmode(struct adapter *adapt, struct sta_info *psta)
{
	unsigned long	irqL;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct ht_priv	*phtpriv_ap = &pmlmepriv->htpriv;
	struct ht_priv	*phtpriv_sta = &psta->htpriv;
	u8	cur_ldpc_cap = 0, cur_stbc_cap = 0;
	/* set intf_tag to if1 */
	/* psta->intf_tag = 0; */

	RTW_INFO("%s\n", __func__);

	/*alloc macid when call rtw_alloc_stainfo(),release macid when call rtw_free_stainfo()*/

	if (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X)
		psta->ieee8021x_blocked = true;
	else
		psta->ieee8021x_blocked = false;


	/* update sta's cap */

	/* ERP */
	VCS_update(adapt, psta);
	/* HT related cap */
	if (phtpriv_sta->ht_option) {
		/* check if sta supports rx ampdu */
		phtpriv_sta->ampdu_enable = phtpriv_ap->ampdu_enable;

		phtpriv_sta->rx_ampdu_min_spacing = (phtpriv_sta->ht_cap.ampdu_params_info & IEEE80211_HT_CAP_AMPDU_DENSITY) >> 2;

		/* bwmode */
		if ((phtpriv_sta->ht_cap.cap_info & phtpriv_ap->ht_cap.cap_info) & cpu_to_le16(IEEE80211_HT_CAP_SUP_WIDTH))
			psta->cmn.bw_mode = CHANNEL_WIDTH_40;
		else
			psta->cmn.bw_mode = CHANNEL_WIDTH_20;

		if (psta->ht_40mhz_intolerant)
			psta->cmn.bw_mode = CHANNEL_WIDTH_20;

		if (pmlmeext->cur_bwmode < psta->cmn.bw_mode)
			psta->cmn.bw_mode = pmlmeext->cur_bwmode;

		phtpriv_sta->ch_offset = pmlmeext->cur_ch_offset;


		/* check if sta support s Short GI 20M */
		if ((phtpriv_sta->ht_cap.cap_info & phtpriv_ap->ht_cap.cap_info) & cpu_to_le16(IEEE80211_HT_CAP_SGI_20))
			phtpriv_sta->sgi_20m = true;

		/* check if sta support s Short GI 40M */
		if ((phtpriv_sta->ht_cap.cap_info & phtpriv_ap->ht_cap.cap_info) & cpu_to_le16(IEEE80211_HT_CAP_SGI_40)) {
			if (psta->cmn.bw_mode == CHANNEL_WIDTH_40) /* according to psta->bw_mode */
				phtpriv_sta->sgi_40m = true;
			else
				phtpriv_sta->sgi_40m = false;
		}

		psta->qos_option = true;

		/* B0 Config LDPC Coding Capability */
		if (TEST_FLAG(phtpriv_ap->ldpc_cap, LDPC_HT_ENABLE_TX) &&
		    GET_HT_CAP_ELE_LDPC_CAP((u8 *)(&phtpriv_sta->ht_cap))) {
			SET_FLAG(cur_ldpc_cap, (LDPC_HT_ENABLE_TX | LDPC_HT_CAP_TX));
			RTW_INFO("Enable HT Tx LDPC for STA(%d)\n", psta->cmn.aid);
		}

		/* B7 B8 B9 Config STBC setting */
		if (TEST_FLAG(phtpriv_ap->stbc_cap, STBC_HT_ENABLE_TX) &&
		    GET_HT_CAP_ELE_RX_STBC((u8 *)(&phtpriv_sta->ht_cap))) {
			SET_FLAG(cur_stbc_cap, (STBC_HT_ENABLE_TX | STBC_HT_CAP_TX));
			RTW_INFO("Enable HT Tx STBC for STA(%d)\n", psta->cmn.aid);
		}
	} else {
		phtpriv_sta->ampdu_enable = false;

		phtpriv_sta->sgi_20m = false;
		phtpriv_sta->sgi_40m = false;
		psta->cmn.bw_mode = CHANNEL_WIDTH_20;
		phtpriv_sta->ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
	}

	phtpriv_sta->ldpc_cap = cur_ldpc_cap;
	phtpriv_sta->stbc_cap = cur_stbc_cap;

	/* Rx AMPDU */
	send_delba(adapt, 0, psta->cmn.mac_addr);/* recipient */

	/* TX AMPDU */
	send_delba(adapt, 1, psta->cmn.mac_addr);/*  */ /* originator */
	phtpriv_sta->agg_enable_bitmap = 0x0;/* reset */
	phtpriv_sta->candidate_tid_bitmap = 0x0;/* reset */

	psta->cmn.ra_info.is_support_sgi = query_ra_short_GI(psta, rtw_get_tx_bw_mode(adapt, psta));
	update_ldpc_stbc_cap(psta);

	/* todo: init other variables */

	memset((void *)&psta->sta_stats, 0, sizeof(struct stainfo_stats));


	/* add ratid */
	/* add_RATid(adapt, psta); */ /* move to ap_sta_info_defer_update() */

	/* ap mode */
	rtw_hal_set_odm_var(adapt, HAL_ODM_STA_INFO, psta, true);

	_enter_critical_bh(&psta->lock, &irqL);
	psta->state |= _FW_LINKED;
	_exit_critical_bh(&psta->lock, &irqL);


}

static void update_ap_info(struct adapter *adapt, struct sta_info *psta)
{
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex *pnetwork = (struct wlan_bssid_ex *)&pmlmepriv->cur_network.network;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct ht_priv	*phtpriv_ap = &pmlmepriv->htpriv;

	psta->wireless_mode = pmlmeext->cur_wireless_mode;

	psta->bssratelen = rtw_get_rateset_len(pnetwork->SupportedRates);
	memcpy(psta->bssrateset, pnetwork->SupportedRates, psta->bssratelen);

	/* HT related cap */
	if (phtpriv_ap->ht_option) {
		/* check if sta supports rx ampdu */
		/* phtpriv_ap->ampdu_enable = phtpriv_ap->ampdu_enable; */

		/* check if sta support s Short GI 20M */
		if ((phtpriv_ap->ht_cap.cap_info) & cpu_to_le16(IEEE80211_HT_CAP_SGI_20))
			phtpriv_ap->sgi_20m = true;
		/* check if sta support s Short GI 40M */
		if ((phtpriv_ap->ht_cap.cap_info) & cpu_to_le16(IEEE80211_HT_CAP_SGI_40))
			phtpriv_ap->sgi_40m = true;

		psta->qos_option = true;
	} else {
		phtpriv_ap->ampdu_enable = false;

		phtpriv_ap->sgi_20m = false;
		phtpriv_ap->sgi_40m = false;
	}

	psta->cmn.bw_mode = pmlmeext->cur_bwmode;
	phtpriv_ap->ch_offset = pmlmeext->cur_ch_offset;

	phtpriv_ap->agg_enable_bitmap = 0x0;/* reset */
	phtpriv_ap->candidate_tid_bitmap = 0x0;/* reset */

	memcpy(&psta->htpriv, &pmlmepriv->htpriv, sizeof(struct ht_priv));

	psta->state |= WIFI_AP_STATE; /* Aries, add,fix bug of flush_cam_entry at STOP AP mode , 0724 */
}

static void rtw_set_hw_wmm_param(struct adapter *adapt)
{
	u8	AIFS, ECWMin, ECWMax, aSifsTime;
	u8	acm_mask;
	u16	TXOP;
	u32	acParm, i;
	u32	edca[4], inx[4];
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct xmit_priv		*pxmitpriv = &adapt->xmitpriv;
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	acm_mask = 0;

	if (is_supported_5g(pmlmeext->cur_wireless_mode) ||
	    (pmlmeext->cur_wireless_mode & WIRELESS_11_24N))
		aSifsTime = 16;
	else
		aSifsTime = 10;

	if (pmlmeinfo->WMM_enable == 0) {
		adapt->mlmepriv.acm_mask = 0;

		AIFS = aSifsTime + (2 * pmlmeinfo->slotTime);

		if (pmlmeext->cur_wireless_mode & (WIRELESS_11G | WIRELESS_11A)) {
			ECWMin = 4;
			ECWMax = 10;
		} else if (pmlmeext->cur_wireless_mode & WIRELESS_11B) {
			ECWMin = 5;
			ECWMax = 10;
		} else {
			ECWMin = 4;
			ECWMax = 10;
		}

		TXOP = 0;
		acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BE, (u8 *)(&acParm));
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BK, (u8 *)(&acParm));
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VI, (u8 *)(&acParm));

		ECWMin = 2;
		ECWMax = 3;
		TXOP = 0x2f;
		acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VO, (u8 *)(&acParm));

	} else {
		edca[0] = edca[1] = edca[2] = edca[3] = 0;

		/*TODO:*/
		acm_mask = 0;
		adapt->mlmepriv.acm_mask = acm_mask;

		AIFS = (7 * pmlmeinfo->slotTime) + aSifsTime;
		ECWMin = 4;
		ECWMax = 10;
		TXOP = 0;
		acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BK, (u8 *)(&acParm));
		edca[XMIT_BK_QUEUE] = acParm;
		RTW_INFO("WMM(BK): %x\n", acParm);

		/* BE */
		AIFS = (3 * pmlmeinfo->slotTime) + aSifsTime;
		ECWMin = 4;
		ECWMax = 6;
		TXOP = 0;
		acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BE, (u8 *)(&acParm));
		edca[XMIT_BE_QUEUE] = acParm;
		RTW_INFO("WMM(BE): %x\n", acParm);

		/* VI */
		AIFS = (1 * pmlmeinfo->slotTime) + aSifsTime;
		ECWMin = 3;
		ECWMax = 4;
		TXOP = 94;
		acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VI, (u8 *)(&acParm));
		edca[XMIT_VI_QUEUE] = acParm;
		RTW_INFO("WMM(VI): %x\n", acParm);

		/* VO */
		AIFS = (1 * pmlmeinfo->slotTime) + aSifsTime;
		ECWMin = 2;
		ECWMax = 3;
		TXOP = 47;
		acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
		rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VO, (u8 *)(&acParm));
		edca[XMIT_VO_QUEUE] = acParm;
		RTW_INFO("WMM(VO): %x\n", acParm);


		if (adapt->registrypriv.acm_method == 1)
			rtw_hal_set_hwreg(adapt, HW_VAR_ACM_CTRL, (u8 *)(&acm_mask));
		else
			adapt->mlmepriv.acm_mask = acm_mask;

		inx[0] = 0;
		inx[1] = 1;
		inx[2] = 2;
		inx[3] = 3;

		if (pregpriv->wifi_spec == 1) {
			u32	j, tmp, change_inx = false;

			/* entry indx: 0->vo, 1->vi, 2->be, 3->bk. */
			for (i = 0 ; i < 4 ; i++) {
				for (j = i + 1 ; j < 4 ; j++) {
					/* compare CW and AIFS */
					if ((edca[j] & 0xFFFF) < (edca[i] & 0xFFFF))
						change_inx = true;
					else if ((edca[j] & 0xFFFF) == (edca[i] & 0xFFFF)) {
						/* compare TXOP */
						if ((edca[j] >> 16) > (edca[i] >> 16))
							change_inx = true;
					}

					if (change_inx) {
						tmp = edca[i];
						edca[i] = edca[j];
						edca[j] = tmp;

						tmp = inx[i];
						inx[i] = inx[j];
						inx[j] = tmp;

						change_inx = false;
					}
				}
			}
		}

		for (i = 0 ; i < 4 ; i++) {
			pxmitpriv->wmm_para_seq[i] = inx[i];
			RTW_INFO("wmm_para_seq(%d): %d\n", i, pxmitpriv->wmm_para_seq[i]);
		}

	}

}

static void update_hw_ht_param(struct adapter *adapt)
{
	unsigned char		max_AMPDU_len;
	unsigned char		min_MPDU_spacing;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	RTW_INFO("%s\n", __func__);


	/* handle A-MPDU parameter field */
	/*
		AMPDU_para [1:0]:Max AMPDU Len => 0:8k , 1:16k, 2:32k, 3:64k
		AMPDU_para [4:2]:Min MPDU Start Spacing
	*/
	max_AMPDU_len = pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x03;

	min_MPDU_spacing = (pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x1c) >> 2;

	rtw_hal_set_hwreg(adapt, HW_VAR_AMPDU_MIN_SPACE, (u8 *)(&min_MPDU_spacing));

	rtw_hal_set_hwreg(adapt, HW_VAR_AMPDU_FACTOR, (u8 *)(&max_AMPDU_len));

	/*  */
	/* Config SM Power Save setting */
	/*  */
	pmlmeinfo->SM_PS = (le16_to_cpu(pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info) & 0x0C) >> 2;
	if (pmlmeinfo->SM_PS == WLAN_HT_CAP_SM_PS_STATIC) {
		RTW_INFO("%s(): WLAN_HT_CAP_SM_PS_STATIC\n", __func__);
	}

	/*  */
	/* Config current HT Protection mode. */
	/*  */
	/* pmlmeinfo->HT_protection = pmlmeinfo->HT_info.infos[1] & 0x3; */

}

static void rtw_ap_check_scan(struct adapter *adapt)
{
	unsigned long	irqL;
	struct list_head		*plist, *phead;
	u32	delta_time, lifetime;
	struct	wlan_network	*pnetwork = NULL;
	struct wlan_bssid_ex *pbss = NULL;
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct __queue	*queue	= &(pmlmepriv->scanned_queue);
	u8 do_scan = false;
	u8 reason = RTW_AUTO_SCAN_REASON_UNSPECIFIED;

	lifetime = SCANQUEUE_LIFETIME; /* 20 sec */

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
	phead = get_list_head(queue);
	if (rtw_end_of_queue_search(phead, get_next(phead)))
		if (adapt->registrypriv.wifi_spec) {
			do_scan = true;
			reason |= RTW_AUTO_SCAN_REASON_2040_BSS;
		}
	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (do_scan) {
		RTW_INFO("%s : drv scans by itself and wait_completed\n", __func__);
		rtw_drv_scan_by_self(adapt, reason);
		rtw_scan_wait_completed(adapt);
	}

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {

		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);

		if (rtw_chset_search_ch(adapter_to_chset(adapt), pnetwork->network.Configuration.DSConfig) >= 0
		    && rtw_mlme_band_check(adapt, pnetwork->network.Configuration.DSConfig)
		    && rtw_validate_ssid(&(pnetwork->network.Ssid))) {
			delta_time = (u32) rtw_get_passing_time_ms(pnetwork->last_scanned);

			if (delta_time < lifetime) {

				uint ie_len = 0;
				u8 *pbuf = NULL;
				u8 *ie = NULL;

				pbss = &pnetwork->network;
				ie = pbss->IEs;

				/*check if HT CAP INFO IE exists or not*/
				pbuf = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _HT_CAPABILITY_IE_, &ie_len, (pbss->IELength - _BEACON_IE_OFFSET_));
				if (!pbuf) {
					/* HT CAP INFO IE don't exist, it is b/g mode bss.*/

					if (false == ATOMIC_READ(&pmlmepriv->olbc))
						ATOMIC_SET(&pmlmepriv->olbc, true);

					if (false == ATOMIC_READ(&pmlmepriv->olbc_ht))
						ATOMIC_SET(&pmlmepriv->olbc_ht, true);
					
					if (adapt->registrypriv.wifi_spec)
						RTW_INFO("%s: %s is a/b/g ap\n", __func__, pnetwork->network.Ssid.Ssid);
				}
			}
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	pmlmepriv->num_sta_no_ht = 0; /* reset to 0 after ap do scanning*/

}

void rtw_start_bss_hdl_after_chbw_decided(struct adapter *adapter)
{
	struct wlan_bssid_ex *pnetwork = &(adapter->mlmepriv.cur_network.network);
	struct sta_info *sta = NULL;

	/* update cur_wireless_mode */
	update_wireless_mode(adapter);

	/* update RRSR and RTS_INIT_RATE register after set channel and bandwidth */
	UpdateBrateTbl(adapter, pnetwork->SupportedRates);
	rtw_hal_set_hwreg(adapter, HW_VAR_BASIC_RATE, pnetwork->SupportedRates);

	/* update capability after cur_wireless_mode updated */
	update_capinfo(adapter, rtw_get_capability(pnetwork));

	/* update bc/mc sta_info */
	update_bmc_sta(adapter);

	/* update AP's sta info */
	sta = rtw_get_stainfo(&adapter->stapriv, pnetwork->MacAddress);
	if (!sta) {
		RTW_INFO(FUNC_ADPT_FMT" !sta for macaddr="MAC_FMT"\n", FUNC_ADPT_ARG(adapter), MAC_ARG(pnetwork->MacAddress));
		rtw_warn_on(1);
		return;
	}

	update_ap_info(adapter, sta);
}

void start_bss_network(struct adapter *adapt, struct createbss_parm *parm)
{
#define DUMP_ADAPTERS_STATUS 0
	u8 self_action = MLME_ACTION_UNKNOWN;
	u8 val8;
	u16 bcn_interval;
	u32	acparm;
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	struct wlan_bssid_ex *pnetwork = (struct wlan_bssid_ex *)&pmlmepriv->cur_network.network; /* used as input */
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex *pnetwork_mlmeext = &(pmlmeinfo->network);
	s16 req_ch = -1, req_bw = -1, req_offset = -1;
	bool ch_setting_changed = false;
	u8 ch_to_set = 0, bw_to_set, offset_to_set;
	u8 doiqk = false;
	/* use for check ch bw offset can be allowed or not */
	u8 chbw_allow = true;

	if (MLME_IS_AP(adapt))
		self_action = MLME_AP_STARTED;
	else if (MLME_IS_MESH(adapt))
		self_action = MLME_MESH_STARTED;
	else
		rtw_warn_on(1);

	if (parm->req_ch != 0) {
		/* bypass other setting, go checking ch, bw, offset */
		req_ch = parm->req_ch;
		req_bw = parm->req_bw;
		req_offset = parm->req_offset;
		goto chbw_decision;
	} else {
		/* inform this request comes from upper layer */
		req_ch = 0;
		memcpy(pnetwork_mlmeext, pnetwork, pnetwork->Length);
	}

	bcn_interval = (u16)pnetwork->Configuration.BeaconPeriod;

	/* check if there is wps ie, */
	/* if there is wpsie in beacon, the hostapd will update beacon twice when stating hostapd, */
	/* and at first time the security ie ( RSN/WPA IE) will not include in beacon. */
	if (!rtw_get_wps_ie(pnetwork->IEs + _FIXED_IE_LENGTH_, pnetwork->IELength - _FIXED_IE_LENGTH_, NULL, NULL))
		pmlmeext->bstart_bss = true;

	/* todo: update wmm, ht cap */
	/* pmlmeinfo->WMM_enable; */
	/* pmlmeinfo->HT_enable; */
	if (pmlmepriv->qospriv.qos_option)
		pmlmeinfo->WMM_enable = true;
	if (pmlmepriv->htpriv.ht_option) {
		pmlmeinfo->WMM_enable = true;
		pmlmeinfo->HT_enable = true;
		/* pmlmeinfo->HT_info_enable = true; */
		/* pmlmeinfo->HT_caps_enable = true; */

		update_hw_ht_param(adapt);
	}

	if (!pmlmepriv->cur_network.join_res) { /* setting only at  first time */
		/* WEP Key will be set before this function, do not clear CAM. */
		if ((psecuritypriv->dot11PrivacyAlgrthm != _WEP40_) && (psecuritypriv->dot11PrivacyAlgrthm != _WEP104_))
			flush_all_cam_entry(adapt);	/* clear CAM */
	}

	/* set MSR to AP_Mode		 */
	Set_MSR(adapt, _HW_STATE_AP_);

	/* Set BSSID REG */
	rtw_hal_set_hwreg(adapt, HW_VAR_BSSID, pnetwork->MacAddress);

	/* Set EDCA param reg */
#ifdef CONFIG_CONCURRENT_MODE
	acparm = 0x005ea42b;
#else
	acparm = 0x002F3217; /* VO */
#endif
	rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VO, (u8 *)(&acparm));
	acparm = 0x005E4317; /* VI */
	rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VI, (u8 *)(&acparm));
	/* acparm = 0x00105320; */ /* BE */
	acparm = 0x005ea42b;
	rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BE, (u8 *)(&acparm));
	acparm = 0x0000A444; /* BK */
	rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BK, (u8 *)(&acparm));

	/* Set Security */
	val8 = (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) ? 0xcc : 0xcf;
	rtw_hal_set_hwreg(adapt, HW_VAR_SEC_CFG, (u8 *)(&val8));

	/* Beacon Control related register */
	rtw_hal_set_hwreg(adapt, HW_VAR_BEACON_INTERVAL, (u8 *)(&bcn_interval));

chbw_decision:
	ch_setting_changed = rtw_ap_chbw_decision(adapt, req_ch, req_bw, req_offset
		     , &ch_to_set, &bw_to_set, &offset_to_set, &chbw_allow);

	/* let pnetwork_mlme == pnetwork_mlmeext */
	memcpy(pnetwork, pnetwork_mlmeext, pnetwork_mlmeext->Length);

	rtw_start_bss_hdl_after_chbw_decided(adapt);

	rtw_hal_rcr_set_chk_bssid(adapt, self_action);

	if (!IS_CH_WAITING(adapter_to_rfctl(adapt))) {

		doiqk = true;
		rtw_hal_set_hwreg(adapt , HW_VAR_DO_IQK , &doiqk);
	}

	if (ch_to_set != 0) {
		set_channel_bwmode(adapt, ch_to_set, offset_to_set, bw_to_set);
		rtw_mi_update_union_chan_inf(adapt, ch_to_set, offset_to_set, bw_to_set);
	}

	doiqk = false;
	rtw_hal_set_hwreg(adapt , HW_VAR_DO_IQK , &doiqk);

	if (ch_setting_changed
		&& (MLME_IS_GO(adapt) || MLME_IS_MESH(adapt)) /* pure AP is not needed*/
		&& check_fwstate(pmlmepriv, WIFI_ASOC_STATE)
	) {
		#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0))
		rtw_cfg80211_ch_switch_notify(adapt
			, pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset
			, pmlmepriv->htpriv.ht_option);
		#endif
	}

	if (DUMP_ADAPTERS_STATUS) {
		RTW_INFO(FUNC_ADPT_FMT" done\n", FUNC_ADPT_ARG(adapt));
		dump_adapters_status(RTW_DBGDUMP , adapter_to_dvobj(adapt));
	}
	/* update beacon content only if bstart_bss is true */
	if (pmlmeext->bstart_bss) {
		if ((ATOMIC_READ(&pmlmepriv->olbc)) || (ATOMIC_READ(&pmlmepriv->olbc_ht))) {
			/* AP is not starting a 40 MHz BSS in presence of an 802.11g BSS. */

			pmlmepriv->ht_op_mode &= (~HT_INFO_OPERATION_MODE_OP_MODE_MASK);
			pmlmepriv->ht_op_mode |= OP_MODE_MAY_BE_LEGACY_STAS;
			update_beacon(adapt, _HT_ADD_INFO_IE_, NULL, false);
		}

		update_beacon(adapt, _TIM_IE_, NULL, false);
	}

	rtw_scan_wait_completed(adapt);

	/* send beacon */
	if (!rtw_mi_check_fwstate(adapt, _FW_UNDER_SURVEY)) {

		/*update_beacon(adapt, _TIM_IE_, NULL, true);*/

		/* other case will  tx beacon when bcn interrupt coming in. */
		if (send_beacon(adapt) == _FAIL)
			RTW_INFO("issue_beacon, fail!\n");
	}

	/*Set EDCA param reg after update cur_wireless_mode & update_capinfo*/
	if (pregpriv->wifi_spec == 1)
		rtw_set_hw_wmm_param(adapt);

	/*pmlmeext->bstart_bss = true;*/
}

int rtw_check_beacon_data(struct adapter *adapt, u8 *pbuf,  int len)
{
	int ret = _SUCCESS;
	u8 *p;
	u8 *pHT_caps_ie = NULL;
	u8 *pHT_info_ie = NULL;
	u16 cap, ht_cap = false;
	uint ie_len = 0;
	int group_cipher, pairwise_cipher;
	u8 mfp_opt = MFP_NO;
	u8	channel, network_type, supportRate[NDIS_802_11_LENGTH_RATES_EX];
	int supportRateNum = 0;
	u8 OUI1[] = {0x00, 0x50, 0xf2, 0x01};
	u8 WMM_PARA_IE[] = {0x00, 0x50, 0xf2, 0x02, 0x01, 0x01};
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex *pbss_network = (struct wlan_bssid_ex *)&pmlmepriv->cur_network.network;
	u8 *ie = pbss_network->IEs;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	/* SSID */
	/* Supported rates */
	/* DS Params */
	/* WLAN_EID_COUNTRY */
	/* ERP Information element */
	/* Extended supported rates */
	/* WPA/WPA2 */
	/* Wi-Fi Wireless Multimedia Extensions */
	/* ht_capab, ht_oper */
	/* WPS IE */

	RTW_INFO("%s, len=%d\n", __func__, len);

	if (!MLME_IS_AP(adapt) && !MLME_IS_MESH(adapt))
		return _FAIL;


	if (len > MAX_IE_SZ)
		return _FAIL;

	pbss_network->IELength = len;

	memset(ie, 0, MAX_IE_SZ);

	memcpy(ie, pbuf, pbss_network->IELength);


	if (pbss_network->InfrastructureMode != Ndis802_11APMode) {
		rtw_warn_on(1);
		return _FAIL;
	}


	rtw_ap_check_scan(adapt);


	pbss_network->Rssi = 0;

	memcpy(pbss_network->MacAddress, adapter_mac_addr(adapt), ETH_ALEN);

	/* beacon interval */
	p = rtw_get_beacon_interval_from_ie(ie);/* ie + 8;	 */ /* 8: TimeStamp, 2: Beacon Interval 2:Capability */
	/* pbss_network->Configuration.BeaconPeriod = le16_to_cpu(*(unsigned short*)p); */
	pbss_network->Configuration.BeaconPeriod = RTW_GET_LE16(p);

	/* capability */
	/* cap = *(unsigned short *)rtw_get_capability_from_ie(ie); */
	/* cap = le16_to_cpu(cap); */
	cap = RTW_GET_LE16(ie);

	/* SSID */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _SSID_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p && ie_len > 0) {
		memset(&pbss_network->Ssid, 0, sizeof(struct ndis_802_11_ssid));
		memcpy(pbss_network->Ssid.Ssid, (p + 2), ie_len);
		pbss_network->Ssid.SsidLength = ie_len;
		memcpy(adapt->wdinfo.p2p_group_ssid, pbss_network->Ssid.Ssid, pbss_network->Ssid.SsidLength);
		adapt->wdinfo.p2p_group_ssid_len = pbss_network->Ssid.SsidLength;
	}

	/* chnnel */
	channel = 0;
	pbss_network->Configuration.Length = 0;
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _DSSET_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p && ie_len > 0)
		channel = *(p + 2);

	pbss_network->Configuration.DSConfig = channel;


	memset(supportRate, 0, NDIS_802_11_LENGTH_RATES_EX);
	/* get supported rates */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _SUPPORTEDRATES_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p !=  NULL) {
		memcpy(supportRate, p + 2, ie_len);
		supportRateNum = ie_len;
	}

	/* get ext_supported rates */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _EXT_SUPPORTEDRATES_IE_, &ie_len, pbss_network->IELength - _BEACON_IE_OFFSET_);
	if (p !=  NULL) {
		memcpy(supportRate + supportRateNum, p + 2, ie_len);
		supportRateNum += ie_len;

	}

	network_type = rtw_check_network_type(supportRate, supportRateNum, channel);

	rtw_set_supported_rate(pbss_network->SupportedRates, network_type);


	/* parsing ERP_IE */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _ERPINFO_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p && ie_len > 0)
		ERP_IE_handler(adapt, (struct ndis_802_11_variable_ies *)p);

	/* update privacy/security */
	if (cap & BIT(4))
		pbss_network->Privacy = 1;
	else
		pbss_network->Privacy = 0;

	psecuritypriv->wpa_psk = 0;

	/* wpa2 */
	group_cipher = 0;
	pairwise_cipher = 0;
	psecuritypriv->wpa2_group_cipher = _NO_PRIVACY_;
	psecuritypriv->wpa2_pairwise_cipher = _NO_PRIVACY_;
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _RSN_IE_2_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p && ie_len > 0) {
		if (rtw_parse_wpa2_ie(p, ie_len + 2, &group_cipher, &pairwise_cipher, NULL, &mfp_opt) == _SUCCESS) {
			psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;

			psecuritypriv->dot8021xalg = 1;/* psk,  todo:802.1x */
			psecuritypriv->wpa_psk |= BIT(1);

			psecuritypriv->wpa2_group_cipher = group_cipher;
			psecuritypriv->wpa2_pairwise_cipher = pairwise_cipher;
		}
	}

	/* wpa */
	ie_len = 0;
	group_cipher = 0;
	pairwise_cipher = 0;
	psecuritypriv->wpa_group_cipher = _NO_PRIVACY_;
	psecuritypriv->wpa_pairwise_cipher = _NO_PRIVACY_;
	for (p = ie + _BEACON_IE_OFFSET_; ; p += (ie_len + 2)) {
		p = rtw_get_ie(p, _SSN_IE_1_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_ - (ie_len + 2)));
		if ((p) && (!memcmp(p + 2, OUI1, 4))) {
			if (rtw_parse_wpa_ie(p, ie_len + 2, &group_cipher, &pairwise_cipher, NULL) == _SUCCESS) {
				psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;

				psecuritypriv->dot8021xalg = 1;/* psk,  todo:802.1x */

				psecuritypriv->wpa_psk |= BIT(0);

				psecuritypriv->wpa_group_cipher = group_cipher;
				psecuritypriv->wpa_pairwise_cipher = pairwise_cipher;
			}
			break;
		}

		if ((!p) || (ie_len == 0))
			break;
	}

	if (mfp_opt == MFP_INVALID) {
		RTW_INFO(FUNC_ADPT_FMT" invalid MFP setting\n", FUNC_ADPT_ARG(adapt));
		return _FAIL;
	}
	psecuritypriv->mfp_opt = mfp_opt;

	/* wmm */
	ie_len = 0;
	pmlmepriv->qospriv.qos_option = 0;
	if (pregistrypriv->wmm_enable) {
		for (p = ie + _BEACON_IE_OFFSET_; ; p += (ie_len + 2)) {
			p = rtw_get_ie(p, _VENDOR_SPECIFIC_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_ - (ie_len + 2)));
			if ((p) && !memcmp(p + 2, WMM_PARA_IE, 6)) {
				pmlmepriv->qospriv.qos_option = 1;

				*(p + 8) |= BIT(7); /* QoS Info, support U-APSD */

				/* disable all ACM bits since the WMM admission control is not supported */
				*(p + 10) &= ~BIT(4); /* BE */
				*(p + 14) &= ~BIT(4); /* BK */
				*(p + 18) &= ~BIT(4); /* VI */
				*(p + 22) &= ~BIT(4); /* VO */

				break;
			}

			if ((!p) || (ie_len == 0))
				break;
		}
	}
	/* parsing HT_CAP_IE */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _HT_CAPABILITY_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p && ie_len > 0) {
		u8 rf_type = 0;
		enum ht_cap_ampdu_factor max_rx_ampdu_factor = MAX_AMPDU_FACTOR_64K;
		struct rtw_ieee80211_ht_cap *pht_cap = (struct rtw_ieee80211_ht_cap *)(p + 2);

		pHT_caps_ie = p;

		ht_cap = true;
		network_type |= WIRELESS_11_24N;

		rtw_ht_use_default_setting(adapt);

		/* Update HT Capabilities Info field */
		if (!pmlmepriv->htpriv.sgi_20m)
			pht_cap->cap_info &= cpu_to_le16(~(IEEE80211_HT_CAP_SGI_20));

		if (!pmlmepriv->htpriv.sgi_40m)
			pht_cap->cap_info &= cpu_to_le16(~(IEEE80211_HT_CAP_SGI_40));

		if (!TEST_FLAG(pmlmepriv->htpriv.ldpc_cap, LDPC_HT_ENABLE_RX))
			pht_cap->cap_info &= ~cpu_to_le16((IEEE80211_HT_CAP_LDPC_CODING));

		if (!TEST_FLAG(pmlmepriv->htpriv.stbc_cap, STBC_HT_ENABLE_TX))
			pht_cap->cap_info &= cpu_to_le16(~(IEEE80211_HT_CAP_TX_STBC));

		if (!TEST_FLAG(pmlmepriv->htpriv.stbc_cap, STBC_HT_ENABLE_RX))
			pht_cap->cap_info &= cpu_to_le16(~(IEEE80211_HT_CAP_RX_STBC_3R));

		/* Update A-MPDU Parameters field */
		pht_cap->ampdu_params_info &= ~(IEEE80211_HT_CAP_AMPDU_FACTOR | IEEE80211_HT_CAP_AMPDU_DENSITY);

		if ((psecuritypriv->wpa_pairwise_cipher & WPA_CIPHER_CCMP) ||
		    (psecuritypriv->wpa2_pairwise_cipher & WPA_CIPHER_CCMP))
			pht_cap->ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_DENSITY & (0x07 << 2));
		else
			pht_cap->ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_DENSITY & 0x00);

		rtw_hal_get_def_var(adapt, HW_VAR_MAX_RX_AMPDU_FACTOR, &max_rx_ampdu_factor);
		pht_cap->ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_FACTOR & max_rx_ampdu_factor); /* set  Max Rx AMPDU size  to 64K */

		memcpy(&(pmlmeinfo->HT_caps), pht_cap, sizeof(struct HT_caps_element));

		/* Update Supported MCS Set field */
		{
			struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapt);
			u8 rx_nss = 0;
			int i;

			rtw_hal_get_hwreg(adapt, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
			rx_nss = rtw_min(rf_type_to_rf_rx_cnt(rf_type), hal_spec->rx_nss_num);

			/* RX MCS Bitmask */
			switch (rx_nss) {
			case 1:
				set_mcs_rate_by_mask(HT_CAP_ELE_RX_MCS_MAP(pht_cap), MCS_RATE_1R);
				break;
			case 2:
				set_mcs_rate_by_mask(HT_CAP_ELE_RX_MCS_MAP(pht_cap), MCS_RATE_2R);
				break;
			case 3:
				set_mcs_rate_by_mask(HT_CAP_ELE_RX_MCS_MAP(pht_cap), MCS_RATE_3R);
				break;
			case 4:
				set_mcs_rate_by_mask(HT_CAP_ELE_RX_MCS_MAP(pht_cap), MCS_RATE_4R);
				break;
			default:
				RTW_WARN("rf_type:%d or rx_nss:%u is not expected\n", rf_type, hal_spec->rx_nss_num);
			}
			for (i = 0; i < 10; i++)
				*(HT_CAP_ELE_RX_MCS_MAP(pht_cap) + i) &= adapt->mlmeextpriv.default_supported_mcs_set[i];
		}

		memcpy(&pmlmepriv->htpriv.ht_cap, p + 2, ie_len);
	}

	/* parsing HT_INFO_IE */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _HT_ADD_INFO_IE_, &ie_len, (pbss_network->IELength - _BEACON_IE_OFFSET_));
	if (p && ie_len > 0) {
		pHT_info_ie = p;
		if (channel == 0)
			pbss_network->Configuration.DSConfig = GET_HT_OP_ELE_PRI_CHL(pHT_info_ie + 2);
		else if (channel != GET_HT_OP_ELE_PRI_CHL(pHT_info_ie + 2)) {
			RTW_INFO(FUNC_ADPT_FMT" ch inconsistent, DSSS:%u, HT primary:%u\n"
				, FUNC_ADPT_ARG(adapt), channel, GET_HT_OP_ELE_PRI_CHL(pHT_info_ie + 2));
		}
	}

	switch (network_type) {
	case WIRELESS_11B:
		pbss_network->NetworkTypeInUse = Ndis802_11DS;
		break;
	case WIRELESS_11G:
	case WIRELESS_11BG:
	case WIRELESS_11G_24N:
	case WIRELESS_11BG_24N:
		pbss_network->NetworkTypeInUse = Ndis802_11OFDM24;
		break;
	case WIRELESS_11A:
		pbss_network->NetworkTypeInUse = Ndis802_11OFDM5;
		break;
	default:
		pbss_network->NetworkTypeInUse = Ndis802_11OFDM24;
		break;
	}

	pmlmepriv->cur_network.network_type = network_type;

	pmlmepriv->htpriv.ht_option = false;

	if ((psecuritypriv->wpa2_pairwise_cipher & WPA_CIPHER_TKIP) ||
	    (psecuritypriv->wpa_pairwise_cipher & WPA_CIPHER_TKIP)) {
		/* todo: */
		/* ht_cap = false; */
	}

	/* ht_cap	 */
	if (pregistrypriv->ht_enable && ht_cap) {
		pmlmepriv->htpriv.ht_option = true;
		pmlmepriv->qospriv.qos_option = 1;

		pmlmepriv->htpriv.ampdu_enable = pregistrypriv->ampdu_enable ? true : false;

		HT_caps_handler(adapt, (struct ndis_802_11_variable_ies *)pHT_caps_ie);

		HT_info_handler(adapt, (struct ndis_802_11_variable_ies *)pHT_info_ie);
	}

	if(pbss_network->Configuration.DSConfig <= 14 && adapt->registrypriv.wifi_spec == 1) {
		uint len = 0;

		SET_EXT_CAPABILITY_ELE_BSS_COEXIST(pmlmepriv->ext_capab_ie_data, 1);
		pmlmepriv->ext_capab_ie_len = 10;
		rtw_set_ie(pbss_network->IEs + pbss_network->IELength, EID_EXTCapability, 8, pmlmepriv->ext_capab_ie_data, &len);
		pbss_network->IELength += pmlmepriv->ext_capab_ie_len;
	}

	pbss_network->Length = get_wlan_bssid_ex_sz((struct wlan_bssid_ex *)pbss_network);

	rtw_ies_get_chbw(pbss_network->IEs + _BEACON_IE_OFFSET_, pbss_network->IELength - _BEACON_IE_OFFSET_
		, &pmlmepriv->ori_ch, &pmlmepriv->ori_bw, &pmlmepriv->ori_offset);
	rtw_warn_on(pmlmepriv->ori_ch == 0);

	{
		/* alloc sta_info for ap itself */

		struct sta_info *sta;

		sta = rtw_get_stainfo(&adapt->stapriv, pbss_network->MacAddress);
		if (!sta) {
			sta = rtw_alloc_stainfo(&adapt->stapriv, pbss_network->MacAddress);
			if (!sta)
				return _FAIL;
		}
	}

	rtw_startbss_cmd(adapt, RTW_CMDF_WAIT_ACK);
	{
		int sk_band = RTW_GET_SCAN_BAND_SKIP(adapt);

		if (sk_band)
			RTW_CLR_SCAN_BAND_SKIP(adapt, sk_band);
	}

	rtw_indicate_connect(adapt);

	pmlmepriv->cur_network.join_res = true;/* for check if already set beacon */

	/* update bc/mc sta_info */
	/* update_bmc_sta(adapt); */

	return ret;

}

#if CONFIG_RTW_MACADDR_ACL
static void rtw_macaddr_acl_init(struct adapter *adapter)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;
	struct __queue *acl_node_q = &acl->acl_node_q;
	int i;
	unsigned long irqL;

	_enter_critical_bh(&(acl_node_q->lock), &irqL);
	INIT_LIST_HEAD(&(acl_node_q->queue));
	acl->num = 0;
	acl->mode = RTW_ACL_MODE_DISABLED;
	for (i = 0; i < NUM_ACL; i++) {
		INIT_LIST_HEAD(&acl->aclnode[i].list);
		acl->aclnode[i].valid = false;
	}
	_exit_critical_bh(&(acl_node_q->lock), &irqL);
}

static void rtw_macaddr_acl_deinit(struct adapter *adapter)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;
	struct __queue *acl_node_q = &acl->acl_node_q;
	unsigned long irqL;
	struct list_head *head, *list;
	struct rtw_wlan_acl_node *acl_node;

	_enter_critical_bh(&(acl_node_q->lock), &irqL);
	head = get_list_head(acl_node_q);
	list = get_next(head);
	while (!rtw_end_of_queue_search(head, list)) {
		acl_node = container_of(list, struct rtw_wlan_acl_node, list);
		list = get_next(list);

		if (acl_node->valid) {
			acl_node->valid = false;
			rtw_list_delete(&acl_node->list);
			acl->num--;
		}
	}
	_exit_critical_bh(&(acl_node_q->lock), &irqL);

	rtw_warn_on(acl->num);
	acl->mode = RTW_ACL_MODE_DISABLED;
}

void rtw_set_macaddr_acl(struct adapter *adapter, int mode)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;

	RTW_INFO(FUNC_ADPT_FMT" mode=%d\n", FUNC_ADPT_ARG(adapter), mode);

	acl->mode = mode;

	if (mode == RTW_ACL_MODE_DISABLED)
		rtw_macaddr_acl_deinit(adapter);
}

int rtw_acl_add_sta(struct adapter *adapter, const u8 *addr)
{
	unsigned long irqL;
	struct list_head *list, *head;
	u8 existed = 0;
	int i = -1, ret = 0;
	struct rtw_wlan_acl_node *acl_node;
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;
	struct __queue *acl_node_q = &acl->acl_node_q;

	_enter_critical_bh(&(acl_node_q->lock), &irqL);

	head = get_list_head(acl_node_q);
	list = get_next(head);

	/* search for existed entry */
	while (!rtw_end_of_queue_search(head, list)) {
		acl_node = container_of(list, struct rtw_wlan_acl_node, list);
		list = get_next(list);

		if (!memcmp(acl_node->addr, addr, ETH_ALEN)) {
			if (acl_node->valid) {
				existed = 1;
				break;
			}
		}
	}
	if (existed)
		goto release_lock;

	if (acl->num >= NUM_ACL)
		goto release_lock;

	/* find empty one and use */
	for (i = 0; i < NUM_ACL; i++) {

		acl_node = &acl->aclnode[i];
		if (!acl_node->valid) {

			INIT_LIST_HEAD(&acl_node->list);
			memcpy(acl_node->addr, addr, ETH_ALEN);
			acl_node->valid = true;

			list_add_tail(&acl_node->list, get_list_head(acl_node_q));
			acl->num++;
			break;
		}
	}

release_lock:
	_exit_critical_bh(&(acl_node_q->lock), &irqL);

	if (!existed && (i < 0 || i >= NUM_ACL))
		ret = -1;

	RTW_INFO(FUNC_ADPT_FMT" "MAC_FMT" %s (acl_num=%d)\n"
		 , FUNC_ADPT_ARG(adapter), MAC_ARG(addr)
		, (existed ? "existed" : ((i < 0 || i >= NUM_ACL) ? "no room" : "added"))
		 , acl->num);

	return ret;
}

int rtw_acl_remove_sta(struct adapter *adapter, const u8 *addr)
{
	unsigned long irqL;
	struct list_head *list, *head;
	int ret = 0;
	struct rtw_wlan_acl_node *acl_node;
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;
	struct __queue	*acl_node_q = &acl->acl_node_q;
	u8 is_baddr = is_broadcast_mac_addr(addr);
	u8 match = 0;

	_enter_critical_bh(&(acl_node_q->lock), &irqL);

	head = get_list_head(acl_node_q);
	list = get_next(head);

	while (!rtw_end_of_queue_search(head, list)) {
		acl_node = container_of(list, struct rtw_wlan_acl_node, list);
		list = get_next(list);

		if (is_baddr || !memcmp(acl_node->addr, addr, ETH_ALEN)) {
			if (acl_node->valid) {
				acl_node->valid = false;
				rtw_list_delete(&acl_node->list);
				acl->num--;
				match = 1;
			}
		}
	}

	_exit_critical_bh(&(acl_node_q->lock), &irqL);

	RTW_INFO(FUNC_ADPT_FMT" "MAC_FMT" %s (acl_num=%d)\n"
		 , FUNC_ADPT_ARG(adapter), MAC_ARG(addr)
		 , is_baddr ? "clear all" : (match ? "match" : "no found")
		 , acl->num);

	return ret;
}
#endif /* CONFIG_RTW_MACADDR_ACL */

u8 rtw_ap_set_pairwise_key(struct adapter *adapt, struct sta_info *psta)
{
	struct cmd_obj			*ph2c;
	struct set_stakey_parm	*psetstakey_para;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	psetstakey_para = (struct set_stakey_parm *)rtw_zmalloc(sizeof(struct set_stakey_parm));
	if (!psetstakey_para) {
		rtw_mfree((u8 *) ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetstakey_para, _SetStaKey_CMD_);


	psetstakey_para->algorithm = (u8)psta->dot118021XPrivacy;

	memcpy(psetstakey_para->addr, psta->cmn.mac_addr, ETH_ALEN);

	memcpy(psetstakey_para->key, &psta->dot118021x_UncstKey, 16);


	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:

	return res;

}

static int rtw_ap_set_key(struct adapter *adapt, u8 *key, u8 alg, int keyid, u8 set_tx)
{
	u8 keylen;
	struct cmd_obj *pcmd;
	struct setkey_parm *psetkeyparm;
	struct cmd_priv	*pcmdpriv = &(adapt->cmdpriv);
	int res = _SUCCESS;

	/* RTW_INFO("%s\n", __func__); */

	pcmd = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!pcmd) {
		res = _FAIL;
		goto exit;
	}
	psetkeyparm = (struct setkey_parm *)rtw_zmalloc(sizeof(struct setkey_parm));
	if (!psetkeyparm) {
		rtw_mfree((unsigned char *)pcmd, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	memset(psetkeyparm, 0, sizeof(struct setkey_parm));

	psetkeyparm->keyid = (u8)keyid;
	if (is_wep_enc(alg))
		adapt->securitypriv.key_mask |= BIT(psetkeyparm->keyid);

	psetkeyparm->algorithm = alg;

	psetkeyparm->set_tx = set_tx;

	switch (alg) {
	case _WEP40_:
		keylen = 5;
		break;
	case _WEP104_:
		keylen = 13;
		break;
	case _TKIP_:
	case _TKIP_WTMIC_:
	case _AES_:
	default:
		keylen = 16;
	}

	memcpy(&(psetkeyparm->key[0]), key, keylen);

	pcmd->cmdcode = _SetKey_CMD_;
	pcmd->parmbuf = (u8 *)psetkeyparm;
	pcmd->cmdsz = (sizeof(struct setkey_parm));
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;


	INIT_LIST_HEAD(&pcmd->list);

	res = rtw_enqueue_cmd(pcmdpriv, pcmd);

exit:

	return res;
}

int rtw_ap_set_group_key(struct adapter *adapt, u8 *key, u8 alg, int keyid)
{
	RTW_INFO("%s\n", __func__);

	return rtw_ap_set_key(adapt, key, alg, keyid, 1);
}

int rtw_ap_set_wep_key(struct adapter *adapt, u8 *key, u8 keylen, int keyid, u8 set_tx)
{
	u8 alg;

	switch (keylen) {
	case 5:
		alg = _WEP40_;
		break;
	case 13:
		alg = _WEP104_;
		break;
	default:
		alg = _NO_PRIVACY_;
	}

	RTW_INFO("%s\n", __func__);

	return rtw_ap_set_key(adapt, key, alg, keyid, set_tx);
}

static void associated_stainfo_update(struct adapter *adapt, struct sta_info *psta, u32 sta_info_type)
{
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	RTW_INFO("%s: "MAC_FMT", updated_type=0x%x\n", __func__, MAC_ARG(psta->cmn.mac_addr), sta_info_type);

	if (sta_info_type & STA_INFO_UPDATE_BW) {

		if ((psta->flags & WLAN_STA_HT) && !psta->ht_20mhz_set) {
			if (pmlmepriv->sw_to_20mhz) {
				psta->cmn.bw_mode = CHANNEL_WIDTH_20;
				/*psta->htpriv.ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;*/
				psta->htpriv.sgi_40m = false;
			} else {
				/*TODO: Switch back to 40MHZ?80MHZ*/
			}
		}
	}

	/*
		if (sta_info_type & STA_INFO_UPDATE_RATE) {

		}
	*/

	if (sta_info_type & STA_INFO_UPDATE_PROTECTION_MODE)
		VCS_update(adapt, psta);

	/*
		if (sta_info_type & STA_INFO_UPDATE_CAP) {

		}

		if (sta_info_type & STA_INFO_UPDATE_HT_CAP) {

		}

		if (sta_info_type & STA_INFO_UPDATE_VHT_CAP) {

		}
	*/

}

static void update_bcn_ext_capab_ie(struct adapter *adapt)
{
	int ie_len = 0;
	unsigned char	*pbuf;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex *pnetwork = &(pmlmeinfo->network);
	u8 *ie = pnetwork->IEs;
	u8 null_extcap_data[8] = {0};

	pbuf = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _EXT_CAP_IE_, &ie_len, (pnetwork->IELength - _BEACON_IE_OFFSET_));
	if (pbuf && ie_len > 0)
		rtw_remove_bcn_ie(adapt, pnetwork, _EXT_CAP_IE_);

	if ((pmlmepriv->ext_capab_ie_len > 0) &&
	    (memcmp(pmlmepriv->ext_capab_ie_data, null_extcap_data, sizeof(null_extcap_data))))
		rtw_add_bcn_ie(adapt, pnetwork, _EXT_CAP_IE_, pmlmepriv->ext_capab_ie_data, pmlmepriv->ext_capab_ie_len);

}

static void update_bcn_erpinfo_ie(struct adapter *adapt)
{
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex *pnetwork = &(pmlmeinfo->network);
	unsigned char *p, *ie = pnetwork->IEs;
	u32 len = 0;

	RTW_INFO("%s, ERP_enable=%d\n", __func__, pmlmeinfo->ERP_enable);

	if (!pmlmeinfo->ERP_enable)
		return;

	/* parsing ERP_IE */
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _ERPINFO_IE_, &len, (pnetwork->IELength - _BEACON_IE_OFFSET_));
	if (p && len > 0) {
		struct ndis_802_11_variable_ies * pIE = (struct ndis_802_11_variable_ies *)p;

		if (pmlmepriv->num_sta_non_erp == 1)
			pIE->data[0] |= RTW_ERP_INFO_NON_ERP_PRESENT | RTW_ERP_INFO_USE_PROTECTION;
		else
			pIE->data[0] &= ~(RTW_ERP_INFO_NON_ERP_PRESENT | RTW_ERP_INFO_USE_PROTECTION);

		if (pmlmepriv->num_sta_no_short_preamble > 0)
			pIE->data[0] |= RTW_ERP_INFO_BARKER_PREAMBLE_MODE;
		else
			pIE->data[0] &= ~(RTW_ERP_INFO_BARKER_PREAMBLE_MODE);

		ERP_IE_handler(adapt, pIE);
	}

}

static void update_bcn_htcap_ie(struct adapter *adapt)
{
	RTW_INFO("%s\n", __func__);

}

static void update_bcn_htinfo_ie(struct adapter *adapt)
{
	/*
	u8 beacon_updated = false;
	u32 sta_info_update_type = STA_INFO_UPDATE_NONE;
	*/
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex *pnetwork = &(pmlmeinfo->network);
	unsigned char *p, *ie = pnetwork->IEs;
	u32 len = 0;

	if (!pmlmepriv->htpriv.ht_option)
		return;

	if (pmlmeinfo->HT_info_enable != 1)
		return;


	RTW_INFO("%s current operation mode=0x%X\n",
		 __func__, pmlmepriv->ht_op_mode);

	RTW_INFO("num_sta_40mhz_intolerant(%d), 20mhz_width_req(%d), intolerant_ch_rpt(%d), olbc(%d)\n",
		pmlmepriv->num_sta_40mhz_intolerant, pmlmepriv->ht_20mhz_width_req, pmlmepriv->ht_intolerant_ch_reported, ATOMIC_READ(&pmlmepriv->olbc));

	/*parsing HT_INFO_IE, currently only update ht_op_mode - pht_info->infos[1] & pht_info->infos[2] for wifi logo test*/
	p = rtw_get_ie(ie + _BEACON_IE_OFFSET_, _HT_ADD_INFO_IE_, &len, (pnetwork->IELength - _BEACON_IE_OFFSET_));
	if (p && len > 0) {
		struct HT_info_element *pht_info = NULL;

		pht_info = (struct HT_info_element *)(p + 2);

		/* for STA Channel Width/Secondary Channel Offset*/
		if ((pmlmepriv->sw_to_20mhz == 0) && (pmlmeext->cur_channel <= 14)) {
			if ((pmlmepriv->num_sta_40mhz_intolerant > 0) || (pmlmepriv->ht_20mhz_width_req)
			    || (pmlmepriv->ht_intolerant_ch_reported) || (ATOMIC_READ(&pmlmepriv->olbc))) {
				SET_HT_OP_ELE_2ND_CHL_OFFSET(pht_info, 0);
				SET_HT_OP_ELE_STA_CHL_WIDTH(pht_info, 0);

				pmlmepriv->sw_to_20mhz = 1;
				/*
				sta_info_update_type |= STA_INFO_UPDATE_BW;
				beacon_updated = true;
				*/

				RTW_INFO("%s:switching to 20Mhz\n", __func__);

				/*TODO : cur_bwmode/cur_ch_offset switches to 20Mhz*/
			}
		} else {

			if ((pmlmepriv->num_sta_40mhz_intolerant == 0) && (!pmlmepriv->ht_20mhz_width_req)
			    && (!pmlmepriv->ht_intolerant_ch_reported) && (!ATOMIC_READ(&pmlmepriv->olbc))) {

				if (pmlmeext->cur_bwmode >= CHANNEL_WIDTH_40) {

					SET_HT_OP_ELE_STA_CHL_WIDTH(pht_info, 1);

					SET_HT_OP_ELE_2ND_CHL_OFFSET(pht_info,
						(pmlmeext->cur_ch_offset == HAL_PRIME_CHNL_OFFSET_LOWER) ?
						HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE : HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW);

					pmlmepriv->sw_to_20mhz = 0;
					/*
					sta_info_update_type |= STA_INFO_UPDATE_BW;
					beacon_updated = true;
					*/

					RTW_INFO("%s:switching back to 40Mhz\n", __func__);
				}
			}
		}

		/* to update  ht_op_mode*/
		*(__le16 *)(pht_info->infos + 1) = cpu_to_le16(pmlmepriv->ht_op_mode);

	}

	/*associated_clients_update(adapt, beacon_updated, sta_info_update_type);*/

}

static void update_bcn_rsn_ie(struct adapter *adapt)
{
	RTW_INFO("%s\n", __func__);

}

static void update_bcn_wpa_ie(struct adapter *adapt)
{
	RTW_INFO("%s\n", __func__);

}

static void update_bcn_wmm_ie(struct adapter *adapt)
{
	RTW_INFO("%s\n", __func__);

}

static void update_bcn_wps_ie(struct adapter *adapt)
{
	u8 *pwps_ie = NULL, *pwps_ie_src, *premainder_ie, *pbackup_remainder_ie = NULL;
	uint wps_ielen = 0, wps_offset, remainder_ielen;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex *pnetwork = &(pmlmeinfo->network);
	unsigned char *ie = pnetwork->IEs;
	u32 ielen = pnetwork->IELength;


	RTW_INFO("%s\n", __func__);

	pwps_ie = rtw_get_wps_ie(ie + _FIXED_IE_LENGTH_, ielen - _FIXED_IE_LENGTH_, NULL, &wps_ielen);

	if (!pwps_ie || wps_ielen == 0)
		return;

	pwps_ie_src = pmlmepriv->wps_beacon_ie;
	if (!pwps_ie_src)
		return;

	wps_offset = (uint)(pwps_ie - ie);

	premainder_ie = pwps_ie + wps_ielen;

	remainder_ielen = ielen - wps_offset - wps_ielen;

	if (remainder_ielen > 0) {
		pbackup_remainder_ie = rtw_malloc(remainder_ielen);
		if (pbackup_remainder_ie)
			memcpy(pbackup_remainder_ie, premainder_ie, remainder_ielen);
	}

	wps_ielen = (uint)pwps_ie_src[1];/* to get ie data len */
	if ((wps_offset + wps_ielen + 2 + remainder_ielen) <= MAX_IE_SZ) {
		memcpy(pwps_ie, pwps_ie_src, wps_ielen + 2);
		pwps_ie += (wps_ielen + 2);

		if (pbackup_remainder_ie)
			memcpy(pwps_ie, pbackup_remainder_ie, remainder_ielen);

		/* update IELength */
		pnetwork->IELength = wps_offset + (wps_ielen + 2) + remainder_ielen;
	}

	if (pbackup_remainder_ie)
		rtw_mfree(pbackup_remainder_ie, remainder_ielen);
}

static void update_bcn_p2p_ie(struct adapter *adapt)
{
}

static void update_bcn_vendor_spec_ie(struct adapter *adapt, u8 *oui)
{
	RTW_INFO("%s\n", __func__);

	if (!memcmp(RTW_WPA_OUI, oui, 4))
		update_bcn_wpa_ie(adapt);
	else if (!memcmp(WMM_OUI, oui, 4))
		update_bcn_wmm_ie(adapt);
	else if (!memcmp(WPS_OUI, oui, 4))
		update_bcn_wps_ie(adapt);
	else if (!memcmp(P2P_OUI, oui, 4))
		update_bcn_p2p_ie(adapt);
	else
		RTW_INFO("unknown OUI type!\n");


}

void _update_beacon(struct adapter *adapt, u8 ie_id, u8 *oui, u8 tx, const char *tag)
{
	unsigned long irqL;
	struct mlme_priv *pmlmepriv;
	struct mlme_ext_priv *pmlmeext;

	if (!adapt)
		return;

	pmlmepriv = &(adapt->mlmepriv);
	pmlmeext = &(adapt->mlmeextpriv);

	if (!pmlmeext->bstart_bss)
		return;

	_enter_critical_bh(&pmlmepriv->bcn_update_lock, &irqL);

	switch (ie_id) {
	case _TIM_IE_:
		update_BCNTIM(adapt);
		break;

	case _ERPINFO_IE_:
		update_bcn_erpinfo_ie(adapt);
		break;

	case _HT_CAPABILITY_IE_:
		update_bcn_htcap_ie(adapt);
		break;

	case _RSN_IE_2_:
		update_bcn_rsn_ie(adapt);
		break;

	case _HT_ADD_INFO_IE_:
		update_bcn_htinfo_ie(adapt);
		break;

	case _EXT_CAP_IE_:
		update_bcn_ext_capab_ie(adapt);
		break;

	case _VENDOR_SPECIFIC_IE_:
		update_bcn_vendor_spec_ie(adapt, oui);
		break;

	case 0xFF:
	default:
		break;
	}

	pmlmepriv->update_bcn = true;

	_exit_critical_bh(&pmlmepriv->bcn_update_lock, &irqL);

	if (tx) {
		/* send_beacon must execute on TSR level */
		set_tx_beacon_cmd(adapt);
	}
}

void rtw_process_public_act_bsscoex(struct adapter *adapt, u8 *pframe, uint frame_len)
{
	struct sta_info *psta;
	struct sta_priv *pstapriv = &adapt->stapriv;
	u8 beacon_updated = false;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	u8 *frame_body = pframe + sizeof(struct rtw_ieee80211_hdr_3addr);
	uint frame_body_len = frame_len - sizeof(struct rtw_ieee80211_hdr_3addr);
	u8 category, action;

	psta = rtw_get_stainfo(pstapriv, get_addr2_ptr(pframe));
	if (!psta)
		return;


	category = frame_body[0];
	action = frame_body[1];

	if (frame_body_len > 0) {
		if ((frame_body[2] == EID_BSSCoexistence) && (frame_body[3] > 0)) {
			u8 ie_data = frame_body[4];

			if (ie_data & RTW_WLAN_20_40_BSS_COEX_40MHZ_INTOL) {
				if (psta->ht_40mhz_intolerant == 0) {
					psta->ht_40mhz_intolerant = 1;
					pmlmepriv->num_sta_40mhz_intolerant++;
					beacon_updated = true;
				}
			} else if (ie_data & RTW_WLAN_20_40_BSS_COEX_20MHZ_WIDTH_REQ)	{
				if (!pmlmepriv->ht_20mhz_width_req) {
					pmlmepriv->ht_20mhz_width_req = true;
					beacon_updated = true;
				}
			} else
				beacon_updated = false;
		}
	}

	if (frame_body_len > 8) {
		/* if EID_BSSIntolerantChlReport ie exists */
		if ((frame_body[5] == EID_BSSIntolerantChlReport) && (frame_body[6] > 0)) {
			/*todo:*/
			if (!pmlmepriv->ht_intolerant_ch_reported) {
				pmlmepriv->ht_intolerant_ch_reported = true;
				beacon_updated = true;
			}
		}
	}

	if (beacon_updated) {

		update_beacon(adapt, _HT_ADD_INFO_IE_, NULL, true);

		associated_stainfo_update(adapt, psta, STA_INFO_UPDATE_BW);
	}



}

void rtw_process_ht_action_smps(struct adapter *adapt, u8 *ta, u8 ctrl_field)
{
	u8 e_field, m_field;
	struct sta_info *psta;
	struct sta_priv *pstapriv = &adapt->stapriv;

	psta = rtw_get_stainfo(pstapriv, ta);
	if (!psta)
		return;

	e_field = (ctrl_field & BIT(0)) ? 1 : 0;
	m_field = (ctrl_field & BIT(1)) ? 1 : 0;

	if (e_field) {

		/* enable */
		/* 0:static SMPS, 1:dynamic SMPS, 3:SMPS disabled, 2:reserved*/

		if (m_field) /*mode*/
			psta->htpriv.smps_cap = 1;
		else
			psta->htpriv.smps_cap = 0;
	} else {
		/*disable*/
		psta->htpriv.smps_cap = 3;
	}

	rtw_dm_ra_mask_wk_cmd(adapt, (u8 *)psta);

}

/*
op_mode
Set to 0 (HT pure) under the followign conditions
	- all STAs in the BSS are 20/40 MHz HT in 20/40 MHz BSS or
	- all STAs in the BSS are 20 MHz HT in 20 MHz BSS
Set to 1 (HT non-member protection) if there may be non-HT STAs
	in both the primary and the secondary channel
Set to 2 if only HT STAs are associated in BSS,
	however and at least one 20 MHz HT STA is associated
Set to 3 (HT mixed mode) when one or more non-HT STAs are associated
	(currently non-GF HT station is considered as non-HT STA also)
*/
int rtw_ht_operation_update(struct adapter *adapt)
{
	u16 cur_op_mode, new_op_mode;
	int op_mode_changes = 0;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct ht_priv	*phtpriv_ap = &pmlmepriv->htpriv;

	if (!pmlmepriv->htpriv.ht_option)
		return 0;

	/*if (!iface->conf->ieee80211n || iface->conf->ht_op_mode_fixed)
		return 0;*/

	RTW_INFO("%s current operation mode=0x%X\n",
		 __func__, pmlmepriv->ht_op_mode);

	if (!(pmlmepriv->ht_op_mode & HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT)
	    && pmlmepriv->num_sta_ht_no_gf) {
		pmlmepriv->ht_op_mode |=
			HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT;
		op_mode_changes++;
	} else if ((pmlmepriv->ht_op_mode &
		    HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT) &&
		   pmlmepriv->num_sta_ht_no_gf == 0) {
		pmlmepriv->ht_op_mode &=
			~HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT;
		op_mode_changes++;
	}

	if (!(pmlmepriv->ht_op_mode & HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT) &&
	    (pmlmepriv->num_sta_no_ht || ATOMIC_READ(&pmlmepriv->olbc_ht))) {
		pmlmepriv->ht_op_mode |= HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT;
		op_mode_changes++;
	} else if ((pmlmepriv->ht_op_mode &
		    HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT) &&
		   (pmlmepriv->num_sta_no_ht == 0 && !ATOMIC_READ(&pmlmepriv->olbc_ht))) {
		pmlmepriv->ht_op_mode &=
			~HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT;
		op_mode_changes++;
	}

	/* Note: currently we switch to the MIXED op mode if HT non-greenfield
	 * station is associated. Probably it's a theoretical case, since
	 * it looks like all known HT STAs support greenfield.
	 */
	new_op_mode = 0;
	if (pmlmepriv->num_sta_no_ht /*||
	    (pmlmepriv->ht_op_mode & HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT)*/)
		new_op_mode = OP_MODE_MIXED;
	else if ((phtpriv_ap->ht_cap.cap_info & cpu_to_le16(IEEE80211_HT_CAP_SUP_WIDTH))
		 && pmlmepriv->num_sta_ht_20mhz)
		new_op_mode = OP_MODE_20MHZ_HT_STA_ASSOCED;
	else if (ATOMIC_READ(&pmlmepriv->olbc_ht))
		new_op_mode = OP_MODE_MAY_BE_LEGACY_STAS;
	else
		new_op_mode = OP_MODE_PURE;

	cur_op_mode = pmlmepriv->ht_op_mode & HT_INFO_OPERATION_MODE_OP_MODE_MASK;
	if (cur_op_mode != new_op_mode) {
		pmlmepriv->ht_op_mode &= ~HT_INFO_OPERATION_MODE_OP_MODE_MASK;
		pmlmepriv->ht_op_mode |= new_op_mode;
		op_mode_changes++;
	}

	RTW_INFO("%s new operation mode=0x%X changes=%d\n",
		 __func__, pmlmepriv->ht_op_mode, op_mode_changes);

	return op_mode_changes;

}

void associated_clients_update(struct adapter *adapt, u8 updated, u32 sta_info_type)
{
	/* update associcated stations cap. */
	if (updated) {
		unsigned long irqL;
		struct list_head	*phead, *plist;
		struct sta_info *psta = NULL;
		struct sta_priv *pstapriv = &adapt->stapriv;

		_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);

		phead = &pstapriv->asoc_list;
		plist = get_next(phead);

		/* check asoc_queue */
		while ((!rtw_end_of_queue_search(phead, plist))) {
			psta = container_of(plist, struct sta_info, asoc_list);

			plist = get_next(plist);

			associated_stainfo_update(adapt, psta, sta_info_type);
		}

		_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	}

}

/* called > TSR LEVEL for USB or SDIO Interface*/
void bss_cap_update_on_sta_join(struct adapter *adapt, struct sta_info *psta)
{
	u8 beacon_updated = false;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &(adapt->mlmeextpriv);

	if (!(psta->flags & WLAN_STA_SHORT_PREAMBLE)) {
		if (!psta->no_short_preamble_set) {
			psta->no_short_preamble_set = 1;

			pmlmepriv->num_sta_no_short_preamble++;

			if ((pmlmeext->cur_wireless_mode > WIRELESS_11B) &&
			    (pmlmepriv->num_sta_no_short_preamble == 1))
				beacon_updated = true;
		}
	} else {
		if (psta->no_short_preamble_set) {
			psta->no_short_preamble_set = 0;

			pmlmepriv->num_sta_no_short_preamble--;

			if ((pmlmeext->cur_wireless_mode > WIRELESS_11B) &&
			    (pmlmepriv->num_sta_no_short_preamble == 0))
				beacon_updated = true;
		}
	}

	if (psta->flags & WLAN_STA_NONERP) {
		if (!psta->nonerp_set) {
			psta->nonerp_set = 1;

			pmlmepriv->num_sta_non_erp++;

			if (pmlmepriv->num_sta_non_erp == 1) {
				beacon_updated = true;
				update_beacon(adapt, _ERPINFO_IE_, NULL, false);
			}
		}

	} else {
		if (psta->nonerp_set) {
			psta->nonerp_set = 0;

			pmlmepriv->num_sta_non_erp--;

			if (pmlmepriv->num_sta_non_erp == 0) {
				beacon_updated = true;
				update_beacon(adapt, _ERPINFO_IE_, NULL, false);
			}
		}

	}

	if (!(psta->capability & WLAN_CAPABILITY_SHORT_SLOT)) {
		if (!psta->no_short_slot_time_set) {
			psta->no_short_slot_time_set = 1;

			pmlmepriv->num_sta_no_short_slot_time++;

			if ((pmlmeext->cur_wireless_mode > WIRELESS_11B) &&
			    (pmlmepriv->num_sta_no_short_slot_time == 1))
				beacon_updated = true;
		}
	} else {
		if (psta->no_short_slot_time_set) {
			psta->no_short_slot_time_set = 0;

			pmlmepriv->num_sta_no_short_slot_time--;

			if ((pmlmeext->cur_wireless_mode > WIRELESS_11B) &&
			    (pmlmepriv->num_sta_no_short_slot_time == 0))
				beacon_updated = true;
		}
	}

	if (psta->flags & WLAN_STA_HT) {
		u16 ht_capab = le16_to_cpu(psta->htpriv.ht_cap.cap_info);

		RTW_INFO("HT: STA " MAC_FMT " HT Capabilities Info: 0x%04x\n",
			MAC_ARG(psta->cmn.mac_addr), ht_capab);

		if (psta->no_ht_set) {
			psta->no_ht_set = 0;
			pmlmepriv->num_sta_no_ht--;
		}

		if ((ht_capab & IEEE80211_HT_CAP_GRN_FLD) == 0) {
			if (!psta->no_ht_gf_set) {
				psta->no_ht_gf_set = 1;
				pmlmepriv->num_sta_ht_no_gf++;
			}
			RTW_INFO("%s STA " MAC_FMT " - no "
				 "greenfield, num of non-gf stations %d\n",
				 __func__, MAC_ARG(psta->cmn.mac_addr),
				 pmlmepriv->num_sta_ht_no_gf);
		}

		if ((ht_capab & IEEE80211_HT_CAP_SUP_WIDTH) == 0) {
			if (!psta->ht_20mhz_set) {
				psta->ht_20mhz_set = 1;
				pmlmepriv->num_sta_ht_20mhz++;
			}
			RTW_INFO("%s STA " MAC_FMT " - 20 MHz HT, "
				 "num of 20MHz HT STAs %d\n",
				 __func__, MAC_ARG(psta->cmn.mac_addr),
				 pmlmepriv->num_sta_ht_20mhz);
		}

		if (((ht_capab & RTW_IEEE80211_HT_CAP_40MHZ_INTOLERANT) != 0) &&
			(psta->ht_40mhz_intolerant == 0)) {
			psta->ht_40mhz_intolerant = 1;
			pmlmepriv->num_sta_40mhz_intolerant++;
			RTW_INFO("%s STA " MAC_FMT " - 40MHZ_INTOLERANT, ",
				   __func__, MAC_ARG(psta->cmn.mac_addr));
		}

	} else {
		if (!psta->no_ht_set) {
			psta->no_ht_set = 1;
			pmlmepriv->num_sta_no_ht++;
		}
		if (pmlmepriv->htpriv.ht_option) {
			RTW_INFO("%s STA " MAC_FMT
				 " - no HT, num of non-HT stations %d\n",
				 __func__, MAC_ARG(psta->cmn.mac_addr),
				 pmlmepriv->num_sta_no_ht);
		}
	}

	if (rtw_ht_operation_update(adapt) > 0) {
		update_beacon(adapt, _HT_CAPABILITY_IE_, NULL, false);
		update_beacon(adapt, _HT_ADD_INFO_IE_, NULL, false);
		beacon_updated = true;
	}

	if (beacon_updated)
		update_beacon(adapt, 0xFF, NULL, true);

	/* update associcated stations cap. */
	associated_clients_update(adapt,  beacon_updated, STA_INFO_UPDATE_ALL);

	RTW_INFO("%s, updated=%d\n", __func__, beacon_updated);

}

u8 bss_cap_update_on_sta_leave(struct adapter *adapt, struct sta_info *psta)
{
	u8 beacon_updated = false;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &(adapt->mlmeextpriv);

	if (!psta)
		return beacon_updated;

	if (rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, psta->cmn.aid)) {
		rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, psta->cmn.aid);
		beacon_updated = true;
		update_beacon(adapt, _TIM_IE_, NULL, false);
	}

	if (psta->no_short_preamble_set) {
		psta->no_short_preamble_set = 0;
		pmlmepriv->num_sta_no_short_preamble--;
		if (pmlmeext->cur_wireless_mode > WIRELESS_11B
		    && pmlmepriv->num_sta_no_short_preamble == 0)
			beacon_updated = true;
	}

	if (psta->nonerp_set) {
		psta->nonerp_set = 0;
		pmlmepriv->num_sta_non_erp--;
		if (pmlmepriv->num_sta_non_erp == 0) {
			beacon_updated = true;
			update_beacon(adapt, _ERPINFO_IE_, NULL, false);
		}
	}

	if (psta->no_short_slot_time_set) {
		psta->no_short_slot_time_set = 0;
		pmlmepriv->num_sta_no_short_slot_time--;
		if (pmlmeext->cur_wireless_mode > WIRELESS_11B
		    && pmlmepriv->num_sta_no_short_slot_time == 0)
			beacon_updated = true;
	}

	if (psta->no_ht_gf_set) {
		psta->no_ht_gf_set = 0;
		pmlmepriv->num_sta_ht_no_gf--;
	}

	if (psta->no_ht_set) {
		psta->no_ht_set = 0;
		pmlmepriv->num_sta_no_ht--;
	}

	if (psta->ht_20mhz_set) {
		psta->ht_20mhz_set = 0;
		pmlmepriv->num_sta_ht_20mhz--;
	}

	if (psta->ht_40mhz_intolerant) {
		psta->ht_40mhz_intolerant = 0;
		if (pmlmepriv->num_sta_40mhz_intolerant > 0)
			pmlmepriv->num_sta_40mhz_intolerant--;
		else
			rtw_warn_on(1);
	}

	if (rtw_ht_operation_update(adapt) > 0) {
		update_beacon(adapt, _HT_CAPABILITY_IE_, NULL, false);
		update_beacon(adapt, _HT_ADD_INFO_IE_, NULL, false);
	}

	if (beacon_updated)
		update_beacon(adapt, 0xFF, NULL, true);

	RTW_INFO("%s, updated=%d\n", __func__, beacon_updated);

	return beacon_updated;

}

u8 ap_free_sta(struct adapter *adapt, struct sta_info *psta, bool active, u16 reason, bool enqueue)
{
	unsigned long irqL;
	u8 beacon_updated = false;

	if (!psta)
		return beacon_updated;

	if (active) {
		/* tear down Rx AMPDU */
		send_delba(adapt, 0, psta->cmn.mac_addr);/* recipient */

		/* tear down TX AMPDU */
		send_delba(adapt, 1, psta->cmn.mac_addr);/*  */ /* originator */

		issue_deauth(adapt, psta->cmn.mac_addr, reason);
	}

	psta->htpriv.agg_enable_bitmap = 0x0;/* reset */
	psta->htpriv.candidate_tid_bitmap = 0x0;/* reset */

	/* clear cam entry / key */
	rtw_clearstakey_cmd(adapt, psta, enqueue);


	_enter_critical_bh(&psta->lock, &irqL);
	psta->state &= ~_FW_LINKED;
	_exit_critical_bh(&psta->lock, &irqL);

	#ifdef COMPAT_KERNEL_RELEASE
	rtw_cfg80211_indicate_sta_disassoc(adapt, psta->cmn.mac_addr, reason);
	#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)) && !defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER)
	rtw_cfg80211_indicate_sta_disassoc(adapt, psta->cmn.mac_addr, reason);
	#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)) && !defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER) */
	/* will call rtw_cfg80211_indicate_sta_disassoc() in cmd_thread for old API context */
	#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)) && !defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER) */

	report_del_sta_event(adapt, psta->cmn.mac_addr, reason, enqueue, false);

	beacon_updated = bss_cap_update_on_sta_leave(adapt, psta);

	/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL);					 */
	rtw_free_stainfo(adapt, psta);
	/* _exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL); */


	return beacon_updated;

}

int rtw_ap_inform_ch_switch(struct adapter *adapt, u8 new_ch, u8 ch_offset)
{
	unsigned long irqL;
	struct list_head	*phead, *plist;
	int ret = 0;
	struct sta_info *psta = NULL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if ((pmlmeinfo->state & 0x03) != WIFI_FW_AP_STATE)
		return ret;

	RTW_INFO(FUNC_NDEV_FMT" with ch:%u, offset:%u\n",
		 FUNC_NDEV_ARG(adapt->pnetdev), new_ch, ch_offset);

	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
	phead = &pstapriv->asoc_list;
	plist = get_next(phead);

	/* for each sta in asoc_queue */
	while ((!rtw_end_of_queue_search(phead, plist))) {
		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);

		issue_action_spct_ch_switch(adapt, psta->cmn.mac_addr, new_ch, ch_offset);
		psta->expire_to = ((pstapriv->expire_to * 2) > 5) ? 5 : (pstapriv->expire_to * 2);
	}
	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	issue_action_spct_ch_switch(adapt, bc_addr, new_ch, ch_offset);

	return ret;
}

int rtw_sta_flush(struct adapter *adapt, bool enqueue)
{
	unsigned long irqL;
	struct list_head	*phead, *plist;
	int ret = 0;
	struct sta_info *psta = NULL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u8 flush_num = 0;
	char flush_list[NUM_STA];
	int i;

	if (!MLME_IS_AP(adapt) && !MLME_IS_MESH(adapt))
		return ret;

	RTW_INFO(FUNC_NDEV_FMT"\n", FUNC_NDEV_ARG(adapt->pnetdev));

	/* pick sta from sta asoc_queue */
	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
	phead = &pstapriv->asoc_list;
	plist = get_next(phead);
	while ((!rtw_end_of_queue_search(phead, plist))) {
		int stainfo_offset;

		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);

		rtw_list_delete(&psta->asoc_list);
		pstapriv->asoc_list_cnt--;

		stainfo_offset = rtw_stainfo_offset(pstapriv, psta);
		if (stainfo_offset_valid(stainfo_offset))
			flush_list[flush_num++] = stainfo_offset;
		else
			rtw_warn_on(1);
	}
	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	/* call ap_free_sta() for each sta picked */
	for (i = 0; i < flush_num; i++) {
		psta = rtw_get_stainfo_by_offset(pstapriv, flush_list[i]);
		ap_free_sta(adapt, psta, true, WLAN_REASON_DEAUTH_LEAVING, enqueue);
	}

	issue_deauth(adapt, bc_addr, WLAN_REASON_DEAUTH_LEAVING);

	associated_clients_update(adapt, true, STA_INFO_UPDATE_ALL);

	return ret;
}

/* called > TSR LEVEL for USB or SDIO Interface*/
void sta_info_update(struct adapter *adapt, struct sta_info *psta)
{
	int flags = psta->flags;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);


	/* update wmm cap. */
	if (WLAN_STA_WME & flags)
		psta->qos_option = 1;
	else
		psta->qos_option = 0;

	if (pmlmepriv->qospriv.qos_option == 0)
		psta->qos_option = 0;


	/* update 802.11n ht cap. */
	if (WLAN_STA_HT & flags) {
		psta->htpriv.ht_option = true;
		psta->qos_option = 1;

		psta->htpriv.smps_cap = le16_to_cpu(psta->htpriv.ht_cap.cap_info) & IEEE80211_HT_CAP_SM_PS >> 2;
	} else
		psta->htpriv.ht_option = false;

	if (!pmlmepriv->htpriv.ht_option)
		psta->htpriv.ht_option = false;

	update_sta_info_apmode(adapt, psta);
}

/* called >= TSR LEVEL for USB or SDIO Interface*/
void ap_sta_info_defer_update(struct adapter *adapt, struct sta_info *psta)
{
	if (psta->state & _FW_LINKED)
		rtw_hal_update_ra_mask(psta); /* DM_RATR_STA_INIT */
}
/* restore hw setting from sw data structures */
void rtw_ap_restore_network(struct adapter *adapt)
{
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct sta_info *psta;
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	unsigned long irqL;
	struct list_head	*phead, *plist;
	u8 chk_alive_num = 0;
	char chk_alive_list[NUM_STA];
	int i;

	rtw_setopmode_cmd(adapt, Ndis802_11APMode, RTW_CMDF_DIRECTLY);

	set_channel_bwmode(adapt, pmlmeext->cur_channel, pmlmeext->cur_ch_offset, pmlmeext->cur_bwmode);

	rtw_startbss_cmd(adapt, RTW_CMDF_DIRECTLY);

	if ((adapt->securitypriv.dot11PrivacyAlgrthm == _TKIP_) ||
	    (adapt->securitypriv.dot11PrivacyAlgrthm == _AES_)) {
		/* restore group key, WEP keys is restored in ips_leave() */
		rtw_set_key(adapt, psecuritypriv, psecuritypriv->dot118021XGrpKeyid, 0, false);
	}

	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	phead = &pstapriv->asoc_list;
	plist = get_next(phead);

	while ((!rtw_end_of_queue_search(phead, plist))) {
		int stainfo_offset;

		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);

		stainfo_offset = rtw_stainfo_offset(pstapriv, psta);
		if (stainfo_offset_valid(stainfo_offset))
			chk_alive_list[chk_alive_num++] = stainfo_offset;
	}

	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	for (i = 0; i < chk_alive_num; i++) {
		psta = rtw_get_stainfo_by_offset(pstapriv, chk_alive_list[i]);

		if (!psta)
			RTW_INFO(FUNC_ADPT_FMT" sta_info is null\n", FUNC_ADPT_ARG(adapt));
		else if (psta->state & _FW_LINKED) {
			rtw_sta_media_status_rpt(adapt, psta, 1);
			Update_RA_Entry(adapt, psta);
			/* pairwise key */
			/* per sta pairwise key and settings */
			if ((adapt->securitypriv.dot11PrivacyAlgrthm == _TKIP_) ||
			    (adapt->securitypriv.dot11PrivacyAlgrthm == _AES_))
				rtw_setstakey_cmd(adapt, psta, UNICAST_KEY, false);
		}
	}

}

void start_ap_mode(struct adapter *adapt)
{
	int i;
	struct sta_info *psta = NULL;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	pmlmepriv->update_bcn = false;

	/*init_mlme_ap_info(adapt);*/

	pmlmeext->bstart_bss = false;

	pmlmepriv->num_sta_non_erp = 0;

	pmlmepriv->num_sta_no_short_slot_time = 0;

	pmlmepriv->num_sta_no_short_preamble = 0;

	pmlmepriv->num_sta_ht_no_gf = 0;
	pmlmepriv->num_sta_no_ht = 0;
	pmlmeinfo->HT_info_enable = 0;
	pmlmeinfo->HT_caps_enable = 0;
	pmlmeinfo->HT_enable = 0;

	pmlmepriv->num_sta_ht_20mhz = 0;
	pmlmepriv->num_sta_40mhz_intolerant = 0;
	ATOMIC_SET(&pmlmepriv->olbc, false);
	ATOMIC_SET(&pmlmepriv->olbc_ht, false);

	pmlmepriv->ht_20mhz_width_req = false;
	pmlmepriv->ht_intolerant_ch_reported = false;
	pmlmepriv->ht_op_mode = 0;
	pmlmepriv->sw_to_20mhz = 0;

	memset(pmlmepriv->ext_capab_ie_data, 0, sizeof(pmlmepriv->ext_capab_ie_data));
	pmlmepriv->ext_capab_ie_len = 0;

#ifdef CONFIG_CONCURRENT_MODE
	adapt->securitypriv.dot118021x_bmc_cam_id = INVALID_SEC_MAC_CAM_ID;
#endif

	for (i = 0 ;  i < pstapriv->max_aid; i++)
		pstapriv->sta_aid[i] = NULL;

#if CONFIG_RTW_MACADDR_ACL
	rtw_macaddr_acl_init(adapt);
#endif

	psta = rtw_get_bcmc_stainfo(adapt);
	/*_enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL);*/
	if (psta)
		rtw_free_stainfo(adapt, psta);
	/*_exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL);*/

	rtw_init_bcmc_stainfo(adapt);

	if (rtw_mi_get_ap_num(adapt))
		RTW_SET_SCAN_BAND_SKIP(adapt, BAND_5G);

}

static void rtw_ap_bcmc_sta_flush(struct adapter *adapt)
{
#ifdef CONFIG_CONCURRENT_MODE
	int cam_id = -1;
	u8 *addr = adapter_mac_addr(adapt);

	cam_id = rtw_iface_bcmc_id_get(adapt);
	if (cam_id != INVALID_SEC_MAC_CAM_ID) {
		RTW_PRINT("clear group key for "ADPT_FMT" addr:"MAC_FMT", camid:%d\n",
			ADPT_ARG(adapt), MAC_ARG(addr), cam_id);
		clear_cam_entry(adapt, cam_id);
		rtw_camid_free(adapt, cam_id);
		rtw_iface_bcmc_id_set(adapt, INVALID_SEC_MAC_CAM_ID);	/*init default value*/
	}
#else
	invalidate_cam_all(adapt);
#endif
}

void stop_ap_mode(struct adapter *adapt)
{
	u8 self_action = MLME_ACTION_UNKNOWN;
	struct sta_info *psta = NULL;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;

	RTW_INFO("%s -"ADPT_FMT"\n", __func__, ADPT_ARG(adapt));

	if (MLME_IS_AP(adapt))
		self_action = MLME_AP_STOPPED;
	else if (MLME_IS_MESH(adapt))
		self_action = MLME_MESH_STOPPED;
	else
		rtw_warn_on(1);

	pmlmepriv->update_bcn = false;
	adapt->netif_up = false;
	memset((unsigned char *)&adapt->securitypriv, 0, sizeof(struct security_priv));
	adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeOpen;
	adapt->securitypriv.ndisencryptstatus = Ndis802_11WEPDisabled;

	/* free scan queue */
	rtw_free_network_queue(adapt, true);

#if CONFIG_RTW_MACADDR_ACL
	rtw_macaddr_acl_deinit(adapt);
#endif

	rtw_sta_flush(adapt, true);
	rtw_ap_bcmc_sta_flush(adapt);

	/* free_assoc_sta_resources	 */
	rtw_free_all_stainfo(adapt);

	psta = rtw_get_bcmc_stainfo(adapt);
	if (psta) {
		rtw_sta_mstatus_disc_rpt(adapt, psta->cmn.mac_id);
		/* _enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL);		 */
		rtw_free_stainfo(adapt, psta);
		/*_exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL);*/
	}

	rtw_free_mlme_priv_ie_data(pmlmepriv);

	pmlmeext->bstart_bss = false;

	rtw_hal_rcr_set_chk_bssid(adapt, self_action);

	rtw_btcoex_MediaStatusNotify(adapt, 0); /* disconnect */
}

void rtw_ap_update_bss_chbw(struct adapter *adapter, struct wlan_bssid_ex *bss, u8 ch, u8 bw, u8 offset)
{
#define UPDATE_VHT_CAP 1
#define UPDATE_HT_CAP 1
	u8 *p;
	int ie_len;
	u8 old_ch = bss->Configuration.DSConfig;
	bool change_band = false;

	if ((ch <= 14 && old_ch >= 36) || (ch >= 36 && old_ch <= 14))
		change_band = true;

	/* update channel in IE */
	p = rtw_get_ie((bss->IEs + sizeof(struct ndis_802_11_fixed_ies)), _DSSET_IE_, &ie_len, (bss->IELength - sizeof(struct ndis_802_11_fixed_ies)));
	if (p && ie_len > 0)
		*(p + 2) = ch;

	bss->Configuration.DSConfig = ch;

	/* band is changed, update ERP, support rate, ext support rate IE */
	if (change_band)
		change_band_update_ie(adapter, bss, ch);

	{
		struct ht_priv	*htpriv = &adapter->mlmepriv.htpriv;
		u8 *ht_cap_ie, *ht_op_ie;
		int ht_cap_ielen, ht_op_ielen;

		ht_cap_ie = rtw_get_ie((bss->IEs + sizeof(struct ndis_802_11_fixed_ies)), EID_HTCapability, &ht_cap_ielen, (bss->IELength - sizeof(struct ndis_802_11_fixed_ies)));
		ht_op_ie = rtw_get_ie((bss->IEs + sizeof(struct ndis_802_11_fixed_ies)), EID_HTInfo, &ht_op_ielen, (bss->IELength - sizeof(struct ndis_802_11_fixed_ies)));

		/* update ht cap ie */
		if (ht_cap_ie && ht_cap_ielen) {
			#if UPDATE_HT_CAP
			if (bw >= CHANNEL_WIDTH_40)
				SET_HT_CAP_ELE_CHL_WIDTH(ht_cap_ie + 2, 1);
			else
				SET_HT_CAP_ELE_CHL_WIDTH(ht_cap_ie + 2, 0);

			if (bw >= CHANNEL_WIDTH_40 && htpriv->sgi_40m)
				SET_HT_CAP_ELE_SHORT_GI40M(ht_cap_ie + 2, 1);
			else
				SET_HT_CAP_ELE_SHORT_GI40M(ht_cap_ie + 2, 0);

			if (htpriv->sgi_20m)
				SET_HT_CAP_ELE_SHORT_GI20M(ht_cap_ie + 2, 1);
			else
				SET_HT_CAP_ELE_SHORT_GI20M(ht_cap_ie + 2, 0);
			#endif
		}

		/* update ht op ie */
		if (ht_op_ie && ht_op_ielen) {
			SET_HT_OP_ELE_PRI_CHL(ht_op_ie + 2, ch);
			switch (offset) {
			case HAL_PRIME_CHNL_OFFSET_LOWER:
				SET_HT_OP_ELE_2ND_CHL_OFFSET(ht_op_ie + 2, SCA);
				break;
			case HAL_PRIME_CHNL_OFFSET_UPPER:
				SET_HT_OP_ELE_2ND_CHL_OFFSET(ht_op_ie + 2, SCB);
				break;
			case HAL_PRIME_CHNL_OFFSET_DONT_CARE:
			default:
				SET_HT_OP_ELE_2ND_CHL_OFFSET(ht_op_ie + 2, SCN);
				break;
			}

			if (bw >= CHANNEL_WIDTH_40)
				SET_HT_OP_ELE_STA_CHL_WIDTH(ht_op_ie + 2, 1);
			else
				SET_HT_OP_ELE_STA_CHL_WIDTH(ht_op_ie + 2, 0);
		}
	}
}

bool rtw_ap_chbw_decision(struct adapter *adapter, s16 req_ch, s8 req_bw, s8 req_offset
			  , u8 *ch, u8 *bw, u8 *offset, u8 *chbw_allow)
{
	u8 cur_ie_ch, cur_ie_bw, cur_ie_offset;
	u8 dec_ch, dec_bw, dec_offset;
	u8 u_ch = 0, u_offset, u_bw;
	bool changed = false;
	struct mlme_ext_priv *mlmeext = &(adapter->mlmeextpriv);
	struct wlan_bssid_ex *network = &(mlmeext->mlmext_info.network);
	struct mi_state mstate;
	bool set_u_ch = false, set_dec_ch = false;

	rtw_ies_get_chbw(BSS_EX_TLV_IES(network), BSS_EX_TLV_IES_LEN(network)
		, &cur_ie_ch, &cur_ie_bw, &cur_ie_offset);

	/* use chbw of cur_ie updated with specifying req as temporary decision */
	dec_ch = (req_ch <= 0) ? cur_ie_ch : req_ch;
	dec_bw = (req_bw < 0) ? cur_ie_bw : req_bw;
	dec_offset = (req_offset < 0) ? cur_ie_offset : req_offset;

	rtw_mi_status_no_self(adapter, &mstate);
	RTW_INFO(FUNC_ADPT_FMT" ld_sta_num:%u, lg_sta_num%u, ap_num:%u, mesh_num:%u\n"
		, FUNC_ADPT_ARG(adapter), MSTATE_STA_LD_NUM(&mstate), MSTATE_STA_LG_NUM(&mstate)
		, MSTATE_AP_NUM(&mstate), MSTATE_MESH_NUM(&mstate));

	if (MSTATE_STA_LD_NUM(&mstate) || MSTATE_AP_NUM(&mstate) || MSTATE_MESH_NUM(&mstate)) {
		/* has linked STA or AP/Mesh mode, follow */

		rtw_warn_on(!rtw_mi_get_ch_setting_union_no_self(adapter, &u_ch, &u_bw, &u_offset));

		RTW_INFO(FUNC_ADPT_FMT" union no self: %u,%u,%u\n", FUNC_ADPT_ARG(adapter), u_ch, u_bw, u_offset);
		RTW_INFO(FUNC_ADPT_FMT" req: %d,%d,%d\n", FUNC_ADPT_ARG(adapter), req_ch, req_bw, req_offset);

		rtw_adjust_chbw(adapter, u_ch, &dec_bw, &dec_offset);
		rtw_sync_chbw(&dec_ch, &dec_bw, &dec_offset
			      , &u_ch, &u_bw, &u_offset);

		rtw_ap_update_bss_chbw(adapter, network, dec_ch, dec_bw, dec_offset);

		set_u_ch = true;
	} else if (MSTATE_STA_LG_NUM(&mstate)) {
		/* has linking STA */

		rtw_warn_on(!rtw_mi_get_ch_setting_union_no_self(adapter, &u_ch, &u_bw, &u_offset));

		RTW_INFO(FUNC_ADPT_FMT" union no self: %u,%u,%u\n", FUNC_ADPT_ARG(adapter), u_ch, u_bw, u_offset);
		RTW_INFO(FUNC_ADPT_FMT" req: %d,%d,%d\n", FUNC_ADPT_ARG(adapter), req_ch, req_bw, req_offset);

		rtw_adjust_chbw(adapter, dec_ch, &dec_bw, &dec_offset);

		if (rtw_is_chbw_grouped(u_ch, u_bw, u_offset, dec_ch, dec_bw, dec_offset)) {

			rtw_sync_chbw(&dec_ch, &dec_bw, &dec_offset
				      , &u_ch, &u_bw, &u_offset);

			rtw_ap_update_bss_chbw(adapter, network, dec_ch, dec_bw, dec_offset);

			set_u_ch = true;

			/* channel bw offset can be allowed, not need MCC */
			*chbw_allow = true;
		} else {
			/* set this for possible ch change when join down*/
			set_fwstate(&adapter->mlmepriv, WIFI_OP_CH_SWITCHING);
		}
	} else {
		/* single AP/Mesh mode */

		RTW_INFO(FUNC_ADPT_FMT" req: %d,%d,%d\n", FUNC_ADPT_ARG(adapter), req_ch, req_bw, req_offset);

		/* check temporary decision first */
		rtw_adjust_chbw(adapter, dec_ch, &dec_bw, &dec_offset);
		if (!rtw_get_offset_by_chbw(dec_ch, dec_bw, &dec_offset)) {
			if (req_ch == -1 || req_bw == -1)
				goto choose_chbw;
			RTW_WARN(FUNC_ADPT_FMT" req: %u,%u has no valid offset\n", FUNC_ADPT_ARG(adapter), dec_ch, dec_bw);
			*chbw_allow = false;
			goto exit;
		}

		if (!rtw_chset_is_chbw_valid(adapter_to_chset(adapter), dec_ch, dec_bw, dec_offset)) {
			if (req_ch == -1 || req_bw == -1)
				goto choose_chbw;
			RTW_WARN(FUNC_ADPT_FMT" req: %u,%u,%u doesn't fit in chplan\n", FUNC_ADPT_ARG(adapter), dec_ch, dec_bw, dec_offset);
			*chbw_allow = false;
			goto exit;
		}

		if (rtw_odm_dfs_domain_unknown(adapter) && rtw_is_dfs_chbw(dec_ch, dec_bw, dec_offset)) {
			if (req_ch >= 0)
				RTW_WARN(FUNC_ADPT_FMT" DFS channel %u,%u,%u can't be used\n", FUNC_ADPT_ARG(adapter), dec_ch, dec_bw, dec_offset);
			if (req_ch > 0) {
				/* specific channel and not from IE => don't change channel setting */
				*chbw_allow = false;
				goto exit;
			}
			goto choose_chbw;
		}

		if (!rtw_chset_is_ch_non_ocp(adapter_to_chset(adapter), dec_ch, dec_bw, dec_offset))
			goto update_bss_chbw;

choose_chbw:
		if (req_bw < 0)
			req_bw = cur_ie_bw;

		if (!rtw_choose_shortest_waiting_ch(adapter, req_bw, &dec_ch, &dec_bw, &dec_offset, RTW_CHF_DFS)) {
			RTW_WARN(FUNC_ADPT_FMT" no available channel\n", FUNC_ADPT_ARG(adapter));
			*chbw_allow = false;
			goto exit;
		}

update_bss_chbw:
		rtw_ap_update_bss_chbw(adapter, network, dec_ch, dec_bw, dec_offset);

		/* channel bw offset can be allowed for single AP, not need MCC */
		*chbw_allow = true;
		set_dec_ch = true;
	}

	if (rtw_mi_check_fwstate(adapter, _FW_UNDER_SURVEY)) {
		/* scanning, leave ch setting to scan state machine */
		set_u_ch = set_dec_ch = false;
	}

	if (mlmeext->cur_channel != dec_ch
	    || mlmeext->cur_bwmode != dec_bw
	    || mlmeext->cur_ch_offset != dec_offset)
		changed = true;

	if (changed && rtw_linked_check(adapter))
		rtw_sta_flush(adapter, false);

	mlmeext->cur_channel = dec_ch;
	mlmeext->cur_bwmode = dec_bw;
	mlmeext->cur_ch_offset = dec_offset;

	if (u_ch != 0)
		RTW_INFO(FUNC_ADPT_FMT" union: %u,%u,%u\n", FUNC_ADPT_ARG(adapter), u_ch, u_bw, u_offset);

	RTW_INFO(FUNC_ADPT_FMT" dec: %u,%u,%u\n", FUNC_ADPT_ARG(adapter), dec_ch, dec_bw, dec_offset);

	if (set_u_ch) {
		*ch = u_ch;
		*bw = u_bw;
		*offset = u_offset;
	} else if (set_dec_ch) {
		*ch = dec_ch;
		*bw = dec_bw;
		*offset = dec_offset;
	}
exit:
	return changed;
}

u8 rtw_ap_sta_linking_state_check(struct adapter *adapter)
{
	struct sta_info *psta;
	struct sta_priv *pstapriv = &adapter->stapriv;
	struct list_head *plist, *phead;
	unsigned long irqL;
	u8 rst = false;

	if (!MLME_IS_AP(adapter) && !MLME_IS_MESH(adapter))
		return false;

	if (pstapriv->auth_list_cnt !=0)
		return true;

	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
	phead = &pstapriv->asoc_list;
	plist = get_next(phead);
	while ((!rtw_end_of_queue_search(phead, plist))) {
		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);
		if (!(psta->state &_FW_LINKED)) {
			rst = true;
			break;
		}
	}
	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);
	return rst;
}

void rtw_ap_parse_sta_capability(struct adapter *adapter, struct sta_info *sta, u8 *cap)
{
	sta->capability = RTW_GET_LE16(cap);
	if (sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE)
		sta->flags |= WLAN_STA_SHORT_PREAMBLE;
	else
		sta->flags &= ~WLAN_STA_SHORT_PREAMBLE;
}

u16 rtw_ap_parse_sta_supported_rates(struct adapter *adapter, struct sta_info *sta, u8 *tlv_ies, u16 tlv_ies_len)
{
	u8 rate_set[16];
	u8 rate_num;
	int i;
	u16 status = _STATS_SUCCESSFUL_;

	rtw_ies_get_supported_rate(tlv_ies, tlv_ies_len, rate_set, &rate_num);
	if (rate_num == 0) {
		RTW_INFO(FUNC_ADPT_FMT" sta "MAC_FMT" with no supported rate\n"
			, FUNC_ADPT_ARG(adapter), MAC_ARG(sta->cmn.mac_addr));
		status = _STATS_FAILURE_;
		goto exit;
	}

	memcpy(sta->bssrateset, rate_set, rate_num);
	sta->bssratelen = rate_num;

	if (MLME_IS_AP(adapter)) {
		/* this function force only CCK rates to be bassic rate... */
		UpdateBrateTblForSoftAP(sta->bssrateset, sta->bssratelen);
	}

	/* if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G) */ /* ? */
	sta->flags |= WLAN_STA_NONERP;
	for (i = 0; i < sta->bssratelen; i++) {
		if ((sta->bssrateset[i] & 0x7f) > 22) {
			sta->flags &= ~WLAN_STA_NONERP;
			break;
		}
	}

exit:
	return status;
}

u16 rtw_ap_parse_sta_security_ie(struct adapter *adapter, struct sta_info *sta, struct rtw_ieee802_11_elems *elems)
{
	struct security_priv *sec = &adapter->securitypriv;
	u8 *wpa_ie;
	int wpa_ie_len;
	int group_cipher = 0, pairwise_cipher = 0;
	u8 mfp_opt = MFP_NO;
	u16 status = _STATS_SUCCESSFUL_;

	sta->dot8021xalg = 0;
	sta->wpa_psk = 0;
	sta->wpa_group_cipher = 0;
	sta->wpa2_group_cipher = 0;
	sta->wpa_pairwise_cipher = 0;
	sta->wpa2_pairwise_cipher = 0;
	memset(sta->wpa_ie, 0, sizeof(sta->wpa_ie));

	if ((sec->wpa_psk & BIT(1)) && elems->rsn_ie) {
		wpa_ie = elems->rsn_ie;
		wpa_ie_len = elems->rsn_ie_len;

		if (rtw_parse_wpa2_ie(wpa_ie - 2, wpa_ie_len + 2, &group_cipher, &pairwise_cipher, NULL, &mfp_opt) == _SUCCESS) {
			sta->dot8021xalg = 1;/* psk, todo:802.1x */
			sta->wpa_psk |= BIT(1);

			sta->wpa2_group_cipher = group_cipher & sec->wpa2_group_cipher;
			sta->wpa2_pairwise_cipher = pairwise_cipher & sec->wpa2_pairwise_cipher;

			if (!sta->wpa2_group_cipher)
				status = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;

			if (!sta->wpa2_pairwise_cipher)
				status = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
		} else
			status = WLAN_STATUS_INVALID_IE;

	}
	else if ((sec->wpa_psk & BIT(0)) && elems->wpa_ie) {
		wpa_ie = elems->wpa_ie;
		wpa_ie_len = elems->wpa_ie_len;

		if (rtw_parse_wpa_ie(wpa_ie - 2, wpa_ie_len + 2, &group_cipher, &pairwise_cipher, NULL) == _SUCCESS) {
			sta->dot8021xalg = 1;/* psk, todo:802.1x */
			sta->wpa_psk |= BIT(0);

			sta->wpa_group_cipher = group_cipher & sec->wpa_group_cipher;
			sta->wpa_pairwise_cipher = pairwise_cipher & sec->wpa_pairwise_cipher;

			if (!sta->wpa_group_cipher)
				status = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;

			if (!sta->wpa_pairwise_cipher)
				status = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
		} else
			status = WLAN_STATUS_INVALID_IE;

	} else {
		wpa_ie = NULL;
		wpa_ie_len = 0;
	}

	if ((sec->mfp_opt == MFP_REQUIRED && mfp_opt == MFP_NO) || mfp_opt == MFP_INVALID) 
		status = WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
	else if (sec->mfp_opt >= MFP_OPTIONAL && mfp_opt >= MFP_OPTIONAL)
		sta->flags |= WLAN_STA_MFP;

	if (status != _STATS_SUCCESSFUL_)
		goto exit;

	if (!MLME_IS_AP(adapter))
		goto exit;

	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS);
	/* if (!hapd->conf->wps_state && wpa_ie) { */ /* todo: to check ap if supporting WPS */
	if (!wpa_ie) {
		if (elems->wps_ie) {
			RTW_INFO("STA included WPS IE in "
				 "(Re)Association Request - assume WPS is "
				 "used\n");
			sta->flags |= WLAN_STA_WPS;
			/* wpabuf_free(sta->wps_ie); */
			/* sta->wps_ie = wpabuf_alloc_copy(elems.wps_ie + 4, */
			/*				elems.wps_ie_len - 4); */
		} else {
			RTW_INFO("STA did not include WPA/RSN IE "
				 "in (Re)Association Request - possible WPS "
				 "use\n");
			sta->flags |= WLAN_STA_MAYBE_WPS;
		}

		/* AP support WPA/RSN, and sta is going to do WPS, but AP is not ready */
		/* that the selected registrar of AP is _FLASE */
		if ((sec->wpa_psk > 0)
			&& (sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS))
		) {
			struct mlme_priv *mlme = &adapter->mlmepriv;

			if (mlme->wps_beacon_ie) {
				u8 selected_registrar = 0;

				rtw_get_wps_attr_content(mlme->wps_beacon_ie, mlme->wps_beacon_ie_len, WPS_ATTR_SELECTED_REGISTRAR, &selected_registrar, NULL);

				if (!selected_registrar) {
					RTW_INFO("selected_registrar is false , or AP is not ready to do WPS\n");
					status = _STATS_UNABLE_HANDLE_STA_;
					goto exit;
				}
			}
		}

	} else {
		int copy_len;

		if (sec->wpa_psk == 0) {
			RTW_INFO("STA " MAC_FMT
				": WPA/RSN IE in association request, but AP don't support WPA/RSN\n",
				MAC_ARG(sta->cmn.mac_addr));
			status = WLAN_STATUS_INVALID_IE;
			goto exit;
		}

		if (elems->wps_ie) {
			RTW_INFO("STA included WPS IE in "
				 "(Re)Association Request - WPS is "
				 "used\n");
			sta->flags |= WLAN_STA_WPS;
			copy_len = 0;
		} else
			copy_len = ((wpa_ie_len + 2) > sizeof(sta->wpa_ie)) ? (sizeof(sta->wpa_ie)) : (wpa_ie_len + 2);

		if (copy_len > 0)
			memcpy(sta->wpa_ie, wpa_ie - 2, copy_len);
	}

exit:
	return status;
}

void rtw_ap_parse_sta_wmm_ie(struct adapter *adapter, struct sta_info *sta, u8 *tlv_ies, u16 tlv_ies_len)
{
	struct mlme_priv *mlme = &adapter->mlmepriv;
	unsigned char WMM_IE[] = {0x00, 0x50, 0xf2, 0x02, 0x00, 0x01};
	u8 *p;

	sta->flags &= ~WLAN_STA_WME;
	sta->qos_option = 0;
	sta->qos_info = 0;
	sta->has_legacy_ac = true;
	sta->uapsd_vo = 0;
	sta->uapsd_vi = 0;
	sta->uapsd_be = 0;
	sta->uapsd_bk = 0;

	if (!mlme->qospriv.qos_option)
		goto exit;

	p = rtw_get_ie_ex(tlv_ies, tlv_ies_len, WLAN_EID_VENDOR_SPECIFIC, WMM_IE, 6, NULL, NULL);
	if (!p)
		goto exit;

	sta->flags |= WLAN_STA_WME;
	sta->qos_option = 1;
	sta->qos_info = *(p + 8);
	sta->max_sp_len = (sta->qos_info >> 5) & 0x3;

	if ((sta->qos_info & 0xf) != 0xf)
		sta->has_legacy_ac = true;
	else
		sta->has_legacy_ac = false;

	if (sta->qos_info & 0xf) {
		if (sta->qos_info & BIT(0))
			sta->uapsd_vo = BIT(0) | BIT(1);
		else
			sta->uapsd_vo = 0;

		if (sta->qos_info & BIT(1))
			sta->uapsd_vi = BIT(0) | BIT(1);
		else
			sta->uapsd_vi = 0;

		if (sta->qos_info & BIT(2))
			sta->uapsd_bk = BIT(0) | BIT(1);
		else
			sta->uapsd_bk = 0;

		if (sta->qos_info & BIT(3))
			sta->uapsd_be = BIT(0) | BIT(1);
		else
			sta->uapsd_be = 0;
	}

exit:
	return;
}

void rtw_ap_parse_sta_ht_ie(struct adapter *adapter, struct sta_info *sta, struct rtw_ieee802_11_elems *elems)
{
	struct mlme_priv *mlme = &adapter->mlmepriv;

	sta->flags &= ~WLAN_STA_HT;

	if (!mlme->htpriv.ht_option)
		goto exit;

	/* save HT capabilities in the sta object */
	memset(&sta->htpriv.ht_cap, 0, sizeof(struct rtw_ieee80211_ht_cap));
	if (elems->ht_capabilities && elems->ht_capabilities_len >= sizeof(struct rtw_ieee80211_ht_cap)) {
		sta->flags |= WLAN_STA_HT;
		sta->flags |= WLAN_STA_WME;
		memcpy(&sta->htpriv.ht_cap, elems->ht_capabilities, sizeof(struct rtw_ieee80211_ht_cap));
	}
exit:

	return;
}

void rtw_ap_parse_sta_vht_ie(struct adapter *adapter, struct sta_info *sta, struct rtw_ieee802_11_elems *elems)
{
	sta->flags &= ~WLAN_STA_VHT;
}
