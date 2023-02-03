// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_WLAN_UTIL_C_

#include <drv_types.h>
#include <hal_data.h>

static unsigned char ARTHEROS_OUI1[] = {0x00, 0x03, 0x7f};
static unsigned char ARTHEROS_OUI2[] = {0x00, 0x13, 0x74};

static unsigned char BROADCOM_OUI1[] = {0x00, 0x10, 0x18};
static unsigned char BROADCOM_OUI2[] = {0x00, 0x0a, 0xf7};
static unsigned char BROADCOM_OUI3[] = {0x00, 0x05, 0xb5};


static unsigned char CISCO_OUI[] = {0x00, 0x40, 0x96};
static unsigned char MARVELL_OUI[] = {0x00, 0x50, 0x43};
static unsigned char RALINK_OUI[] = {0x00, 0x0c, 0x43};
static unsigned char REALTEK_OUI[] = {0x00, 0xe0, 0x4c};
static unsigned char AIRGOCAP_OUI[] = {0x00, 0x0a, 0xf5};

#define R2T_PHY_DELAY	(0)

#define WAIT_FOR_BCN_TO_MIN	(6000)
#define WAIT_FOR_BCN_TO_MAX	(20000)

#define DISCONNECT_BY_CHK_BCN_FAIL_OBSERV_PERIOD_IN_MS 1000
#define DISCONNECT_BY_CHK_BCN_FAIL_THRESHOLD 3

static u8 rtw_basic_rate_cck[4] = {
	IEEE80211_CCK_RATE_1MB | IEEE80211_BASIC_RATE_MASK, IEEE80211_CCK_RATE_2MB | IEEE80211_BASIC_RATE_MASK,
	IEEE80211_CCK_RATE_5MB | IEEE80211_BASIC_RATE_MASK, IEEE80211_CCK_RATE_11MB | IEEE80211_BASIC_RATE_MASK
};

static u8 rtw_basic_rate_ofdm[3] = {
	IEEE80211_OFDM_RATE_6MB | IEEE80211_BASIC_RATE_MASK, IEEE80211_OFDM_RATE_12MB | IEEE80211_BASIC_RATE_MASK,
	IEEE80211_OFDM_RATE_24MB | IEEE80211_BASIC_RATE_MASK
};

static u8 rtw_basic_rate_mix[7] = {
	IEEE80211_CCK_RATE_1MB | IEEE80211_BASIC_RATE_MASK, IEEE80211_CCK_RATE_2MB | IEEE80211_BASIC_RATE_MASK,
	IEEE80211_CCK_RATE_5MB | IEEE80211_BASIC_RATE_MASK, IEEE80211_CCK_RATE_11MB | IEEE80211_BASIC_RATE_MASK,
	IEEE80211_OFDM_RATE_6MB | IEEE80211_BASIC_RATE_MASK, IEEE80211_OFDM_RATE_12MB | IEEE80211_BASIC_RATE_MASK,
	IEEE80211_OFDM_RATE_24MB | IEEE80211_BASIC_RATE_MASK
};

int new_bcn_max = 3;

int cckrates_included(unsigned char *rate, int ratelen)
{
	int	i;

	for (i = 0; i < ratelen; i++) {
		if ((((rate[i]) & 0x7f) == 2)	|| (((rate[i]) & 0x7f) == 4) ||
		    (((rate[i]) & 0x7f) == 11)  || (((rate[i]) & 0x7f) == 22))
			return true;
	}

	return false;

}

int cckratesonly_included(unsigned char *rate, int ratelen)
{
	int	i;

	for (i = 0; i < ratelen; i++) {
		if ((((rate[i]) & 0x7f) != 2) && (((rate[i]) & 0x7f) != 4) &&
		    (((rate[i]) & 0x7f) != 11)  && (((rate[i]) & 0x7f) != 22))
			return false;
	}

	return true;
}

s8 rtw_get_sta_rx_nss(struct adapter *adapter, struct sta_info *psta)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 rf_type = RF_1T1R, custom_rf_type;
	s8 nss = 1;

	if (!psta)
		return nss;

	custom_rf_type = adapter->registrypriv.rf_config;
	rtw_hal_get_hwreg(adapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	if (RF_TYPE_VALID(custom_rf_type))
		rf_type = custom_rf_type;

	nss = rtw_min(rf_type_to_rf_rx_cnt(rf_type), hal_spec->rx_nss_num);

	if (psta->htpriv.ht_option)
		nss = rtw_min(nss, rtw_ht_mcsset_to_nss(psta->htpriv.ht_cap.supp_mcs_set));
	RTW_INFO("%s: %d SS\n", __func__, nss);
	return nss;
}

s8 rtw_get_sta_tx_nss(struct adapter *adapter, struct sta_info *psta)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 rf_type = RF_1T1R, custom_rf_type;
	s8 nss = 1;

	if (!psta)
		return nss;

	custom_rf_type = adapter->registrypriv.rf_config;
	rtw_hal_get_hwreg(adapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	if (RF_TYPE_VALID(custom_rf_type))
		rf_type = custom_rf_type;

	nss = rtw_min(rf_type_to_rf_tx_cnt(rf_type), hal_spec->tx_nss_num);

	if (psta->htpriv.ht_option)
		nss = rtw_min(nss, rtw_ht_mcsset_to_nss(psta->htpriv.ht_cap.supp_mcs_set));
	RTW_INFO("%s: %d SS\n", __func__, nss);
	return nss;
}

u8 judge_network_type(struct adapter *adapt, unsigned char *rate, int ratelen)
{
	u8 network_type = 0;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);


	if (pmlmeext->cur_channel > 14) {
		if (pmlmeinfo->VHT_enable)
			network_type = WIRELESS_11AC;
		else if (pmlmeinfo->HT_enable)
			network_type = WIRELESS_11_5N;

		network_type |= WIRELESS_11A;
	} else {
		if (pmlmeinfo->HT_enable)
			network_type = WIRELESS_11_24N;

		if ((cckratesonly_included(rate, ratelen)))
			network_type |= WIRELESS_11B;
		else if ((cckrates_included(rate, ratelen)))
			network_type |= WIRELESS_11BG;
		else
			network_type |= WIRELESS_11G;
	}

	return	network_type;
}

unsigned char ratetbl_val_2wifirate(unsigned char rate);
unsigned char ratetbl_val_2wifirate(unsigned char rate)
{
	unsigned char val = 0;

	switch (rate & 0x7f) {
	case 0:
		val = IEEE80211_CCK_RATE_1MB;
		break;

	case 1:
		val = IEEE80211_CCK_RATE_2MB;
		break;

	case 2:
		val = IEEE80211_CCK_RATE_5MB;
		break;

	case 3:
		val = IEEE80211_CCK_RATE_11MB;
		break;

	case 4:
		val = IEEE80211_OFDM_RATE_6MB;
		break;

	case 5:
		val = IEEE80211_OFDM_RATE_9MB;
		break;

	case 6:
		val = IEEE80211_OFDM_RATE_12MB;
		break;

	case 7:
		val = IEEE80211_OFDM_RATE_18MB;
		break;

	case 8:
		val = IEEE80211_OFDM_RATE_24MB;
		break;

	case 9:
		val = IEEE80211_OFDM_RATE_36MB;
		break;

	case 10:
		val = IEEE80211_OFDM_RATE_48MB;
		break;

	case 11:
		val = IEEE80211_OFDM_RATE_54MB;
		break;

	}

	return val;

}

int is_basicrate(struct adapter *adapt, unsigned char rate);
int is_basicrate(struct adapter *adapt, unsigned char rate)
{
	int i;
	unsigned char val;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;

	for (i = 0; i < NumRates; i++) {
		val = pmlmeext->basicrate[i];

		if ((val != 0xff) && (val != 0xfe)) {
			if (rate == ratetbl_val_2wifirate(val))
				return true;
		}
	}

	return false;
}

unsigned int ratetbl2rateset(struct adapter *adapt, unsigned char *rateset);
unsigned int ratetbl2rateset(struct adapter *adapt, unsigned char *rateset)
{
	int i;
	unsigned char rate;
	unsigned int	len = 0;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;

	for (i = 0; i < NumRates; i++) {
		rate = pmlmeext->datarate[i];

		if (rtw_get_oper_ch(adapt) > 14 && rate < _6M_RATE_) /*5G no support CCK rate*/
			continue;

		switch (rate) {
		case 0xff:
			return len;

		case 0xfe:
			continue;

		default:
			rate = ratetbl_val_2wifirate(rate);

			if (is_basicrate(adapt, rate))
				rate |= IEEE80211_BASIC_RATE_MASK;

			rateset[len] = rate;
			len++;
			break;
		}
	}
	return len;
}

void get_rate_set(struct adapter *adapt, unsigned char *pbssrate, int *bssrate_len)
{
	unsigned char supportedrates[NumRates];

	memset(supportedrates, 0, NumRates);
	*bssrate_len = ratetbl2rateset(adapt, supportedrates);
	memcpy(pbssrate, supportedrates, *bssrate_len);
}

void set_mcs_rate_by_mask(u8 *mcs_set, u32 mask)
{
	u8 mcs_rate_1r = (u8)(mask & 0xff);
	u8 mcs_rate_2r = (u8)((mask >> 8) & 0xff);
	u8 mcs_rate_3r = (u8)((mask >> 16) & 0xff);
	u8 mcs_rate_4r = (u8)((mask >> 24) & 0xff);

	mcs_set[0] &= mcs_rate_1r;
	mcs_set[1] &= mcs_rate_2r;
	mcs_set[2] &= mcs_rate_3r;
	mcs_set[3] &= mcs_rate_4r;
}

void UpdateBrateTbl(
	struct adapter *		Adapter,
	u8			*mBratesOS
)
{
	u8	i;
	u8	rate;

	/* 1M, 2M, 5.5M, 11M, 6M, 12M, 24M are mandatory. */
	for (i = 0; i < NDIS_802_11_LENGTH_RATES_EX; i++) {
		rate = mBratesOS[i] & 0x7f;
		switch (rate) {
		case IEEE80211_CCK_RATE_1MB:
		case IEEE80211_CCK_RATE_2MB:
		case IEEE80211_CCK_RATE_5MB:
		case IEEE80211_CCK_RATE_11MB:
		case IEEE80211_OFDM_RATE_6MB:
		case IEEE80211_OFDM_RATE_12MB:
		case IEEE80211_OFDM_RATE_24MB:
			mBratesOS[i] |= IEEE80211_BASIC_RATE_MASK;
			break;
		}
	}

}

void UpdateBrateTblForSoftAP(u8 *bssrateset, u32 bssratelen)
{
	u8	i;
	u8	rate;

	for (i = 0; i < bssratelen; i++) {
		rate = bssrateset[i] & 0x7f;
		switch (rate) {
		case IEEE80211_CCK_RATE_1MB:
		case IEEE80211_CCK_RATE_2MB:
		case IEEE80211_CCK_RATE_5MB:
		case IEEE80211_CCK_RATE_11MB:
			bssrateset[i] |= IEEE80211_BASIC_RATE_MASK;
			break;
		}
	}

}
void Set_MSR(struct adapter *adapt, u8 type)
{
	rtw_hal_set_hwreg(adapt, HW_VAR_MEDIA_STATUS, (u8 *)(&type));
}

inline u8 rtw_get_oper_ch(struct adapter *adapter)
{
	return adapter_to_dvobj(adapter)->oper_channel;
}

inline void rtw_set_oper_ch(struct adapter *adapter, u8 ch)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	if (dvobj->oper_channel != ch)
		dvobj->on_oper_ch_time = rtw_get_current_time();

	dvobj->oper_channel = ch;
}

inline u8 rtw_get_oper_bw(struct adapter *adapter)
{
	return adapter_to_dvobj(adapter)->oper_bwmode;
}

inline void rtw_set_oper_bw(struct adapter *adapter, u8 bw)
{
	adapter_to_dvobj(adapter)->oper_bwmode = bw;
}

inline u8 rtw_get_oper_choffset(struct adapter *adapter)
{
	return adapter_to_dvobj(adapter)->oper_ch_offset;
}

inline void rtw_set_oper_choffset(struct adapter *adapter, u8 offset)
{
	adapter_to_dvobj(adapter)->oper_ch_offset = offset;
}

u8 rtw_get_offset_by_chbw(u8 ch, u8 bw, u8 *r_offset)
{
	u8 valid = 1;
	u8 offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

	if (bw == CHANNEL_WIDTH_20)
		goto exit;

	if (bw >= CHANNEL_WIDTH_80 && ch <= 14) {
		valid = 0;
		goto exit;
	}

	if (ch >= 1 && ch <= 4)
		offset = HAL_PRIME_CHNL_OFFSET_LOWER;
	else if (ch >= 5 && ch <= 9) {
		if (*r_offset == HAL_PRIME_CHNL_OFFSET_LOWER || *r_offset == HAL_PRIME_CHNL_OFFSET_UPPER)
			offset = *r_offset; /* both lower and upper is valid, obey input value */
		else
			offset = HAL_PRIME_CHNL_OFFSET_UPPER; /* default use upper */
	} else if (ch >= 10 && ch <= 13)
		offset = HAL_PRIME_CHNL_OFFSET_UPPER;
	else if (ch == 14) {
		valid = 0; /* ch14 doesn't support 40MHz bandwidth */
		goto exit;
	} else if (ch >= 36 && ch <= 177) {
		switch (ch) {
		case 36:
		case 44:
		case 52:
		case 60:
		case 100:
		case 108:
		case 116:
		case 124:
		case 132:
		case 140:
		case 149:
		case 157:
		case 165:
		case 173:
			offset = HAL_PRIME_CHNL_OFFSET_LOWER;
			break;
		case 40:
		case 48:
		case 56:
		case 64:
		case 104:
		case 112:
		case 120:
		case 128:
		case 136:
		case 144:
		case 153:
		case 161:
		case 169:
		case 177:
			offset = HAL_PRIME_CHNL_OFFSET_UPPER;
			break;
		default:
			valid = 0;
			break;
		}
	} else
		valid = 0;

exit:
	if (valid && r_offset)
		*r_offset = offset;
	return valid;
}

u8 rtw_get_offset_by_ch(u8 channel)
{
	u8 offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

	if (channel >= 1 && channel <= 4)
		offset = HAL_PRIME_CHNL_OFFSET_LOWER;
	else if (channel >= 5 && channel <= 14)
		offset = HAL_PRIME_CHNL_OFFSET_UPPER;
	else {
		switch (channel) {
		case 36:
		case 44:
		case 52:
		case 60:
		case 100:
		case 108:
		case 116:
		case 124:
		case 132:
		case 149:
		case 157:
			offset = HAL_PRIME_CHNL_OFFSET_LOWER;
			break;
		case 40:
		case 48:
		case 56:
		case 64:
		case 104:
		case 112:
		case 120:
		case 128:
		case 136:
		case 153:
		case 161:
			offset = HAL_PRIME_CHNL_OFFSET_UPPER;
			break;
		default:
			offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
			break;
		}

	}

	return offset;

}

u8 rtw_get_center_ch(u8 channel, u8 chnl_bw, u8 chnl_offset)
{
	u8 center_ch = channel;

	if (chnl_bw == CHANNEL_WIDTH_80) {
		if (channel == 36 || channel == 40 || channel == 44 || channel == 48)
			center_ch = 42;
		else if (channel == 52 || channel == 56 || channel == 60 || channel == 64)
			center_ch = 58;
		else if (channel == 100 || channel == 104 || channel == 108 || channel == 112)
			center_ch = 106;
		else if (channel == 116 || channel == 120 || channel == 124 || channel == 128)
			center_ch = 122;
		else if (channel == 132 || channel == 136 || channel == 140 || channel == 144)
			center_ch = 138;
		else if (channel == 149 || channel == 153 || channel == 157 || channel == 161)
			center_ch = 155;
		else if (channel == 165 || channel == 169 || channel == 173 || channel == 177)
			center_ch = 171;
		else if (channel <= 14)
			center_ch = 7;
	} else if (chnl_bw == CHANNEL_WIDTH_40) {
		if (chnl_offset == HAL_PRIME_CHNL_OFFSET_LOWER)
			center_ch = channel + 2;
		else
			center_ch = channel - 2;
	} else if (chnl_bw == CHANNEL_WIDTH_20)
		center_ch = channel;
	else
		rtw_warn_on(1);

	return center_ch;
}

inline unsigned long rtw_get_on_oper_ch_time(struct adapter *adapter)
{
	return adapter_to_dvobj(adapter)->on_oper_ch_time;
}

inline unsigned long rtw_get_on_cur_ch_time(struct adapter *adapter)
{
	if (adapter->mlmeextpriv.cur_channel == adapter_to_dvobj(adapter)->oper_channel)
		return adapter_to_dvobj(adapter)->on_oper_ch_time;
	else
		return 0;
}

void set_channel_bwmode(struct adapter *adapt, unsigned char channel, unsigned char channel_offset, unsigned short bwmode)
{
	u8 center_ch, chnl_offset80 = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

	if (adapt->bNotifyChannelChange)
		RTW_INFO("[%s] ch = %d, offset = %d, bwmode = %d\n", __func__, channel, channel_offset, bwmode);

	center_ch = rtw_get_center_ch(channel, bwmode, channel_offset);

	if (bwmode == CHANNEL_WIDTH_80) {
		if (center_ch > channel)
			chnl_offset80 = HAL_PRIME_CHNL_OFFSET_LOWER;
		else if (center_ch < channel)
			chnl_offset80 = HAL_PRIME_CHNL_OFFSET_UPPER;
		else
			chnl_offset80 = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
	}
	_enter_critical_mutex(&(adapter_to_dvobj(adapt)->setch_mutex), NULL);

	/* set Channel */
	/* saved channel/bw info */
	rtw_set_oper_ch(adapt, channel);
	rtw_set_oper_bw(adapt, bwmode);
	rtw_set_oper_choffset(adapt, channel_offset);

	rtw_hal_set_chnl_bw(adapt, center_ch, bwmode, channel_offset, chnl_offset80); /* set center channel */

	_exit_critical_mutex(&(adapter_to_dvobj(adapt)->setch_mutex), NULL);
}

inline u8 *get_my_bssid(struct wlan_bssid_ex *pnetwork)
{
	return pnetwork->MacAddress;
}

u16 get_beacon_interval(struct wlan_bssid_ex *bss)
{
	__le16 val;
	memcpy((unsigned char *)&val, rtw_get_beacon_interval_from_ie(bss->IEs), 2);

	return le16_to_cpu(val);

}

int is_client_associated_to_ap(struct adapter *adapt)
{
	struct mlme_ext_priv	*pmlmeext;
	struct mlme_ext_info	*pmlmeinfo;

	if (!adapt)
		return _FAIL;

	pmlmeext = &adapt->mlmeextpriv;
	pmlmeinfo = &(pmlmeext->mlmext_info);

	if ((pmlmeinfo->state & WIFI_FW_ASSOC_SUCCESS) && ((pmlmeinfo->state & 0x03) == WIFI_FW_STATION_STATE))
		return true;
	else
		return _FAIL;
}

int is_client_associated_to_ibss(struct adapter *adapt)
{
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	if ((pmlmeinfo->state & WIFI_FW_ASSOC_SUCCESS) && ((pmlmeinfo->state & 0x03) == WIFI_FW_ADHOC_STATE))
		return true;
	else
		return _FAIL;
}

int is_IBSS_empty(struct adapter *adapt)
{
	int i;
	struct macid_ctl_t *macid_ctl = &adapt->dvobj->macid_ctl;

	for (i = 0; i < macid_ctl->num; i++) {
		if (!rtw_macid_is_used(macid_ctl, i))
			continue;
		if (!rtw_macid_is_iface_specific(macid_ctl, i, adapt))
			continue;
		if (!GET_H2CCMD_MSRRPT_PARM_OPMODE(&macid_ctl->h2c_msr[i]))
			continue;
		if (GET_H2CCMD_MSRRPT_PARM_ROLE(&macid_ctl->h2c_msr[i]) == H2C_MSR_ROLE_ADHOC)
			return _FAIL;
	}

	return true;
}

unsigned int decide_wait_for_beacon_timeout(unsigned int bcn_interval)
{
	if ((bcn_interval << 2) < WAIT_FOR_BCN_TO_MIN)
		return WAIT_FOR_BCN_TO_MIN;
	else if ((bcn_interval << 2) > WAIT_FOR_BCN_TO_MAX)
		return WAIT_FOR_BCN_TO_MAX;
	else
		return bcn_interval << 2;
}

void CAM_empty_entry(
	struct adapter *	Adapter,
	u8			ucIndex
)
{
	rtw_hal_set_hwreg(Adapter, HW_VAR_CAM_EMPTY_ENTRY, (u8 *)(&ucIndex));
}

void invalidate_cam_all(struct adapter *adapt)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;
	u8 val8 = 0;

	rtw_hal_set_hwreg(adapt, HW_VAR_CAM_INVALID_ALL, &val8);

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	rtw_sec_cam_map_clr_all(&cam_ctl->used);
	memset(dvobj->cam_cache, 0, sizeof(struct sec_cam_ent) * SEC_CAM_ENT_NUM_SW_LIMIT);
	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

void _clear_cam_entry(struct adapter *adapt, u8 entry)
{
	unsigned char null_sta[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char null_key[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	rtw_sec_write_cam_ent(adapt, entry, 0, null_sta, null_key);
}

inline void write_cam(struct adapter *adapter, u8 id, u16 ctrl, u8 *mac, u8 *key)
{
#ifdef CONFIG_WRITE_CACHE_ONLY
	write_cam_cache(adapter, id , ctrl, mac, key);
#else
	rtw_sec_write_cam_ent(adapter, id, ctrl, mac, key);
	write_cam_cache(adapter, id , ctrl, mac, key);
#endif
}

inline void clear_cam_entry(struct adapter *adapter, u8 id)
{
	_clear_cam_entry(adapter, id);
	clear_cam_cache(adapter, id);
}

inline void write_cam_from_cache(struct adapter *adapter, u8 id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;
	struct sec_cam_ent cache;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	memcpy(&cache, &dvobj->cam_cache[id], sizeof(struct sec_cam_ent));
	_exit_critical_bh(&cam_ctl->lock, &irqL);

	rtw_sec_write_cam_ent(adapter, id, cache.ctrl, cache.mac, cache.key);
}
void write_cam_cache(struct adapter *adapter, u8 id, u16 ctrl, u8 *mac, u8 *key)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;

	_enter_critical_bh(&cam_ctl->lock, &irqL);

	dvobj->cam_cache[id].ctrl = ctrl;
	memcpy(dvobj->cam_cache[id].mac, mac, ETH_ALEN);
	memcpy(dvobj->cam_cache[id].key, key, 16);

	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

void clear_cam_cache(struct adapter *adapter, u8 id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;

	_enter_critical_bh(&cam_ctl->lock, &irqL);

	memset(&(dvobj->cam_cache[id]), 0, sizeof(struct sec_cam_ent));

	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

inline bool _rtw_camctl_chk_cap(struct adapter *adapter, u8 cap)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;

	if (cam_ctl->sec_cap & cap)
		return true;
	return false;
}

inline void _rtw_camctl_set_flags(struct adapter *adapter, u32 flags)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;

	cam_ctl->flags |= flags;
}

inline void rtw_camctl_set_flags(struct adapter *adapter, u32 flags)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	_rtw_camctl_set_flags(adapter, flags);
	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

inline void _rtw_camctl_clr_flags(struct adapter *adapter, u32 flags)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;

	cam_ctl->flags &= ~flags;
}

inline void rtw_camctl_clr_flags(struct adapter *adapter, u32 flags)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	_rtw_camctl_clr_flags(adapter, flags);
	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

inline bool _rtw_camctl_chk_flags(struct adapter *adapter, u32 flags)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;

	if (cam_ctl->flags & flags)
		return true;
	return false;
}

void dump_sec_cam_map(void *sel, struct sec_cam_bmp *map, u8 max_num)
{
	RTW_PRINT_SEL(sel, "0x%08x\n", map->m0);
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	if (max_num && max_num > 32)
		RTW_PRINT_SEL(sel, "0x%08x\n", map->m1);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	if (max_num && max_num > 64)
		RTW_PRINT_SEL(sel, "0x%08x\n", map->m2);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	if (max_num && max_num > 96)
		RTW_PRINT_SEL(sel, "0x%08x\n", map->m3);
#endif
}

inline bool rtw_sec_camid_is_set(struct sec_cam_bmp *map, u8 id)
{
	if (id < 32)
		return map->m0 & BIT(id);
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	else if (id < 64)
		return map->m1 & BIT(id - 32);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	else if (id < 96)
		return map->m2 & BIT(id - 64);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	else if (id < 128)
		return map->m3 & BIT(id - 96);
#endif
	else
		rtw_warn_on(1);

	return 0;
}

inline void rtw_sec_cam_map_set(struct sec_cam_bmp *map, u8 id)
{
	if (id < 32)
		map->m0 |= BIT(id);
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	else if (id < 64)
		map->m1 |= BIT(id - 32);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	else if (id < 96)
		map->m2 |= BIT(id - 64);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	else if (id < 128)
		map->m3 |= BIT(id - 96);
#endif
	else
		rtw_warn_on(1);
}

inline void rtw_sec_cam_map_clr(struct sec_cam_bmp *map, u8 id)
{
	if (id < 32)
		map->m0 &= ~BIT(id);
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	else if (id < 64)
		map->m1 &= ~BIT(id - 32);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	else if (id < 96)
		map->m2 &= ~BIT(id - 64);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	else if (id < 128)
		map->m3 &= ~BIT(id - 96);
#endif
	else
		rtw_warn_on(1);
}

inline void rtw_sec_cam_map_clr_all(struct sec_cam_bmp *map)
{
	map->m0 = 0;
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	map->m1 = 0;
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	map->m2 = 0;
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	map->m3 = 0;
#endif
}

inline bool rtw_sec_camid_is_drv_forbid(struct cam_ctl_t *cam_ctl, u8 id)
{
	struct sec_cam_bmp forbid_map;

	forbid_map.m0 = 0x00000ff0;
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	forbid_map.m1 = 0x00000000;
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	forbid_map.m2 = 0x00000000;
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	forbid_map.m3 = 0x00000000;
#endif

	if (id < 32)
		return forbid_map.m0 & BIT(id);
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	else if (id < 64)
		return forbid_map.m1 & BIT(id - 32);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	else if (id < 96)
		return forbid_map.m2 & BIT(id - 64);
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	else if (id < 128)
		return forbid_map.m3 & BIT(id - 96);
#endif
	else
		rtw_warn_on(1);

	return 1;
}

static bool _rtw_sec_camid_is_used(struct cam_ctl_t *cam_ctl, u8 id)
{
	bool ret = false;

	if (id >= cam_ctl->num) {
		rtw_warn_on(1);
		goto exit;
	}
	ret = rtw_sec_camid_is_set(&cam_ctl->used, id);

exit:
	return ret;
}

inline bool rtw_sec_camid_is_used(struct cam_ctl_t *cam_ctl, u8 id)
{
	unsigned long irqL;
	bool ret;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	ret = _rtw_sec_camid_is_used(cam_ctl, id);
	_exit_critical_bh(&cam_ctl->lock, &irqL);

	return ret;
}
u8 rtw_get_sec_camid(struct adapter *adapter, u8 max_bk_key_num, u8 *sec_key_id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	int i;
	unsigned long irqL;
	u8 sec_cam_num = 0;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	for (i = 0; i < cam_ctl->num; i++) {
		if (_rtw_sec_camid_is_used(cam_ctl, i)) {
			sec_key_id[sec_cam_num++] = i;
			if (sec_cam_num == max_bk_key_num)
				break;
		}
	}
	_exit_critical_bh(&cam_ctl->lock, &irqL);

	return sec_cam_num;
}

inline bool _rtw_camid_is_gk(struct adapter *adapter, u8 cam_id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	bool ret = false;

	if (cam_id >= cam_ctl->num) {
		rtw_warn_on(1);
		goto exit;
	}

	if (!_rtw_sec_camid_is_used(cam_ctl, cam_id))
		goto exit;

	ret = (dvobj->cam_cache[cam_id].ctrl & BIT6) ? true : false;

exit:
	return ret;
}

inline bool rtw_camid_is_gk(struct adapter *adapter, u8 cam_id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;
	bool ret;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	ret = _rtw_camid_is_gk(adapter, cam_id);
	_exit_critical_bh(&cam_ctl->lock, &irqL);

	return ret;
}

static bool cam_cache_chk(struct adapter *adapter, u8 id, u8 *addr, s16 kid, s8 gk)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	bool ret = false;

	if (addr && memcmp(dvobj->cam_cache[id].mac, addr, ETH_ALEN))
		goto exit;
	if (kid >= 0 && kid != (dvobj->cam_cache[id].ctrl & 0x03))
		goto exit;
	if (gk != -1 && (gk ? true : false) != _rtw_camid_is_gk(adapter, id))
		goto exit;

	ret = true;

exit:
	return ret;
}

static s16 _rtw_camid_search(struct adapter *adapter, u8 *addr, s16 kid, s8 gk)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	int i;
	s16 cam_id = -1;

	for (i = 0; i < cam_ctl->num; i++) {
		if (cam_cache_chk(adapter, i, addr, kid, gk)) {
			cam_id = i;
			break;
		}
	}
	return cam_id;
}

s16 rtw_camid_search(struct adapter *adapter, u8 *addr, s16 kid, s8 gk)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;
	s16 cam_id = -1;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	cam_id = _rtw_camid_search(adapter, addr, kid, gk);
	_exit_critical_bh(&cam_ctl->lock, &irqL);

	return cam_id;
}

static s16 rtw_get_camid(struct adapter *adapter, struct sta_info *sta, u8 *addr, s16 kid)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	int i;
	u8 start_id = 0;
	s16 cam_id = -1;

	if (!addr) {
		RTW_PRINT(FUNC_ADPT_FMT" mac_address is NULL\n"
			  , FUNC_ADPT_ARG(adapter));
		rtw_warn_on(1);
		goto _exit;
	}

	/* find cam entry which has the same addr, kid (, gk bit) */
	if (_rtw_camctl_chk_cap(adapter, SEC_CAP_CHK_BMC))
		i = _rtw_camid_search(adapter, addr, kid, sta ? false : true);
	else
		i = _rtw_camid_search(adapter, addr, kid, -1);

	if (i >= 0) {
		cam_id = i;
		goto _exit;
	}

	for (i = 0; i < cam_ctl->num; i++) {
		/* bypass default key which is allocated statically */
#ifndef CONFIG_CONCURRENT_MODE
		if (((i + start_id) % cam_ctl->num) < 4)
			continue;
#endif
		if (!_rtw_sec_camid_is_used(cam_ctl, ((i + start_id) % cam_ctl->num)))
			break;
	}

	if (i == cam_ctl->num) {
		if (sta)
			RTW_PRINT(FUNC_ADPT_FMT" pairwise key with "MAC_FMT" id:%u no room\n"
				  , FUNC_ADPT_ARG(adapter), MAC_ARG(addr), kid);
		else
			RTW_PRINT(FUNC_ADPT_FMT" group key with "MAC_FMT" id:%u no room\n"
				  , FUNC_ADPT_ARG(adapter), MAC_ARG(addr), kid);
		rtw_warn_on(1);
		goto _exit;
	}

	cam_id = ((i + start_id) % cam_ctl->num);
	start_id = ((i + start_id + 1) % cam_ctl->num);

_exit:
	return cam_id;
}

s16 rtw_camid_alloc(struct adapter *adapter, struct sta_info *sta, u8 kid, bool *used)
{
	struct mlme_ext_info *mlmeinfo = &adapter->mlmeextpriv.mlmext_info;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;
	s16 cam_id = -1;

	*used = false;

	_enter_critical_bh(&cam_ctl->lock, &irqL);

	if ((((mlmeinfo->state & 0x03) == WIFI_FW_AP_STATE) || ((mlmeinfo->state & 0x03) == WIFI_FW_ADHOC_STATE))
	    && !sta) {
		/*
		* 1. non-STA mode WEP key
		* 2. group TX key
		*/
#ifndef CONFIG_CONCURRENT_MODE
		/* static alloction to default key by key ID when concurrent is not defined */
		if (kid > 3) {
			RTW_PRINT(FUNC_ADPT_FMT" group key with invalid key id:%u\n"
				  , FUNC_ADPT_ARG(adapter), kid);
			rtw_warn_on(1);
			goto bitmap_handle;
		}
		cam_id = kid;
#else
		u8 *addr = adapter_mac_addr(adapter);

		cam_id = rtw_get_camid(adapter, sta, addr, kid);
		RTW_PRINT(FUNC_ADPT_FMT" group key with "MAC_FMT" assigned cam_id:%u\n"
			, FUNC_ADPT_ARG(adapter), MAC_ARG(addr), cam_id);
#endif
	} else {
		/*
		* 1. STA mode WEP key
		* 2. STA mode group RX key
		* 3. sta key (pairwise, group RX)
		*/
		u8 *addr = sta ? sta->cmn.mac_addr : NULL;

		if (!sta) {
			if (!(mlmeinfo->state & WIFI_FW_ASSOC_SUCCESS)) {
				/* bypass STA mode group key setting before connected(ex:WEP) because bssid is not ready */
				goto bitmap_handle;
			}
			addr = get_bssid(&adapter->mlmepriv);/*A2*/
		}
		cam_id = rtw_get_camid(adapter, sta, addr, kid);
	}


bitmap_handle:
	if (cam_id >= 0) {
		*used = _rtw_sec_camid_is_used(cam_ctl, cam_id);
		rtw_sec_cam_map_set(&cam_ctl->used, cam_id);
	}

	_exit_critical_bh(&cam_ctl->lock, &irqL);

	return cam_id;
}

static void rtw_camid_set(struct adapter *adapter, u8 cam_id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;

	_enter_critical_bh(&cam_ctl->lock, &irqL);

	if (cam_id < cam_ctl->num)
		rtw_sec_cam_map_set(&cam_ctl->used, cam_id);

	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

void rtw_camid_free(struct adapter *adapter, u8 cam_id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;

	_enter_critical_bh(&cam_ctl->lock, &irqL);

	if (cam_id < cam_ctl->num)
		rtw_sec_cam_map_clr(&cam_ctl->used, cam_id);

	_exit_critical_bh(&cam_ctl->lock, &irqL);
}

/*Must pause TX/RX before use this API*/
inline void rtw_sec_cam_swap(struct adapter *adapter, u8 cam_id_a, u8 cam_id_b)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	struct sec_cam_ent cache_a, cache_b;
	unsigned long irqL;
	bool cam_a_used, cam_b_used;

	RTW_INFO(ADPT_FMT" - sec_cam %d,%d swap\n", ADPT_ARG(adapter), cam_id_a, cam_id_b);

	if (cam_id_a == cam_id_b)
		return;

#ifdef CONFIG_CONCURRENT_MODE
	rtw_mi_update_ap_bmc_camid(adapter, cam_id_a, cam_id_b);
#endif

	/*setp-1. backup org cam_info*/
	_enter_critical_bh(&cam_ctl->lock, &irqL);

	cam_a_used = _rtw_sec_camid_is_used(cam_ctl, cam_id_a);
	cam_b_used = _rtw_sec_camid_is_used(cam_ctl, cam_id_b);

	if (cam_a_used)
		memcpy(&cache_a, &dvobj->cam_cache[cam_id_a], sizeof(struct sec_cam_ent));

	if (cam_b_used)
		memcpy(&cache_b, &dvobj->cam_cache[cam_id_b], sizeof(struct sec_cam_ent));

	_exit_critical_bh(&cam_ctl->lock, &irqL);

	/*setp-2. clean cam_info*/
	if (cam_a_used) {
		rtw_camid_free(adapter, cam_id_a);
		clear_cam_entry(adapter, cam_id_a);
	}
	if (cam_b_used) {
		rtw_camid_free(adapter, cam_id_b);
		clear_cam_entry(adapter, cam_id_b);
	}

	/*setp-3. set cam_info*/
	if (cam_a_used) {
		write_cam(adapter, cam_id_b, cache_a.ctrl, cache_a.mac, cache_a.key);
		rtw_camid_set(adapter, cam_id_b);
	}

	if (cam_b_used) {
		write_cam(adapter, cam_id_a, cache_b.ctrl, cache_b.mac, cache_b.key);
		rtw_camid_set(adapter, cam_id_a);
	}
}

static s16 rtw_get_empty_cam_entry(struct adapter *adapter, u8 start_camid)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	unsigned long irqL;
	int i;
	s16 cam_id = -1;

	_enter_critical_bh(&cam_ctl->lock, &irqL);
	for (i = start_camid; i < cam_ctl->num; i++) {
		if (false == _rtw_sec_camid_is_used(cam_ctl, i)) {
			cam_id = i;
			break;
		}
	}
	_exit_critical_bh(&cam_ctl->lock, &irqL);

	return cam_id;
}
void rtw_clean_dk_section(struct adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = dvobj_to_sec_camctl(dvobj);
	s16 ept_cam_id;
	int i;

	for (i = 0; i < 4; i++) {
		if (rtw_sec_camid_is_used(cam_ctl, i)) {
			ept_cam_id = rtw_get_empty_cam_entry(adapter, 4);
			if (ept_cam_id > 0)
				rtw_sec_cam_swap(adapter, i, ept_cam_id);
		}
	}
}
void rtw_clean_hw_dk_cam(struct adapter *adapter)
{
	int i;

	for (i = 0; i < 4; i++)
		rtw_sec_clr_cam_ent(adapter, i);
		/*_clear_cam_entry(adapter, i);*/
}

void flush_all_cam_entry(struct adapter *adapt)
{
#ifdef CONFIG_CONCURRENT_MODE
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		struct sta_priv	*pstapriv = &adapt->stapriv;
		struct sta_info		*psta;

		psta = rtw_get_stainfo(pstapriv, pmlmeinfo->network.MacAddress);
		if (psta) {
			if (psta->state & WIFI_AP_STATE) {
				/*clear cam when ap free per sta_info*/
			} else
				rtw_clearstakey_cmd(adapt, psta, false);
		}
	} else if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt)) {
		int cam_id = -1;
		u8 *addr = adapter_mac_addr(adapt);

		while ((cam_id = rtw_camid_search(adapt, addr, -1, -1)) >= 0) {
			RTW_PRINT("clear wep or group key for addr:"MAC_FMT", camid:%d\n", MAC_ARG(addr), cam_id);
			clear_cam_entry(adapt, cam_id);
			rtw_camid_free(adapt, cam_id);
		}
	}

#else /*NON CONFIG_CONCURRENT_MODE*/

	invalidate_cam_all(adapt);
	/* clear default key related key search setting */
	rtw_hal_set_hwreg(adapt, HW_VAR_SEC_DK_CFG, (u8 *)false);
#endif
}

void rtw_process_wfd_ie(struct adapter *adapter, u8 *wfd_ie, u8 wfd_ielen, const char *tag)
{
	struct wifidirect_info *wdinfo = &adapter->wdinfo;

	u8 *attr_content;
	u32 attr_contentlen = 0;

	if (!hal_chk_wl_func(adapter, WL_FUNC_MIRACAST))
		return;

	RTW_INFO("[%s] Found WFD IE\n", tag);
	attr_content = rtw_get_wfd_attr_content(wfd_ie, wfd_ielen, WFD_ATTR_DEVICE_INFO, NULL, &attr_contentlen);
	if (attr_content && attr_contentlen) {
		wdinfo->wfd_info->peer_rtsp_ctrlport = RTW_GET_BE16(attr_content + 2);
		RTW_INFO("[%s] Peer PORT NUM = %d\n", tag, wdinfo->wfd_info->peer_rtsp_ctrlport);
	}
}

void rtw_process_wfd_ies(struct adapter *adapter, u8 *ies, u8 ies_len, const char *tag)
{
	u8 *wfd_ie;
	u32	wfd_ielen;

	if (!hal_chk_wl_func(adapter, WL_FUNC_MIRACAST))
		return;

	wfd_ie = rtw_get_wfd_ie(ies, ies_len, NULL, &wfd_ielen);
	while (wfd_ie) {
		rtw_process_wfd_ie(adapter, wfd_ie, wfd_ielen, tag);
		wfd_ie = rtw_get_wfd_ie(wfd_ie + wfd_ielen, (ies + ies_len) - (wfd_ie + wfd_ielen), NULL, &wfd_ielen);
	}
}

int WMM_param_handler(struct adapter *adapt, struct ndis_802_11_variable_ies *	pIE)
{
	/* struct registry_priv	*pregpriv = &adapt->registrypriv; */
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	if (pmlmepriv->qospriv.qos_option == 0) {
		pmlmeinfo->WMM_enable = 0;
		return false;
	}

	if (!memcmp(&(pmlmeinfo->WMM_param), (pIE->data + 6), sizeof(struct WMM_para_element)))
		return false;
	else
		memcpy(&(pmlmeinfo->WMM_param), (pIE->data + 6), sizeof(struct WMM_para_element));
	pmlmeinfo->WMM_enable = 1;
	return true;
}

void WMMOnAssocRsp(struct adapter *adapt)
{
	u8	ACI, ACM, AIFS, ECWMin, ECWMax, aSifsTime;
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

		for (i = 0; i < 4; i++) {
			ACI = (pmlmeinfo->WMM_param.ac_param[i].ACI_AIFSN >> 5) & 0x03;
			ACM = (pmlmeinfo->WMM_param.ac_param[i].ACI_AIFSN >> 4) & 0x01;

			/* AIFS = AIFSN * slot time + SIFS - r2t phy delay */
			AIFS = (pmlmeinfo->WMM_param.ac_param[i].ACI_AIFSN & 0x0f) * pmlmeinfo->slotTime + aSifsTime;

			ECWMin = (pmlmeinfo->WMM_param.ac_param[i].CW & 0x0f);
			ECWMax = (pmlmeinfo->WMM_param.ac_param[i].CW & 0xf0) >> 4;
			TXOP = le16_to_cpu(pmlmeinfo->WMM_param.ac_param[i].TXOP_limit);

			acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);

			switch (ACI) {
			case 0x0:
				rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BE, (u8 *)(&acParm));
				acm_mask |= (ACM ? BIT(1) : 0);
				edca[XMIT_BE_QUEUE] = acParm;
				break;

			case 0x1:
				rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_BK, (u8 *)(&acParm));
				/* acm_mask |= (ACM? BIT(0):0); */
				edca[XMIT_BK_QUEUE] = acParm;
				break;

			case 0x2:
				rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VI, (u8 *)(&acParm));
				acm_mask |= (ACM ? BIT(2) : 0);
				edca[XMIT_VI_QUEUE] = acParm;
				break;

			case 0x3:
				rtw_hal_set_hwreg(adapt, HW_VAR_AC_PARAM_VO, (u8 *)(&acParm));
				acm_mask |= (ACM ? BIT(3) : 0);
				edca[XMIT_VO_QUEUE] = acParm;
				break;
			}

			RTW_INFO("WMM(%x): %x, %x\n", ACI, ACM, acParm);
		}

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
			for (i = 0; i < 4; i++) {
				for (j = i + 1; j < 4; j++) {
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

		for (i = 0; i < 4; i++) {
			pxmitpriv->wmm_para_seq[i] = inx[i];
			RTW_INFO("wmm_para_seq(%d): %d\n", i, pxmitpriv->wmm_para_seq[i]);
		}
	}
}

static void bwmode_update_check(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE)
{
	unsigned char	 new_bwmode;
	unsigned char  new_ch_offset;
	struct HT_info_element	*pHT_info;
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	struct ht_priv			*phtpriv = &pmlmepriv->htpriv;
	u8	cbw40_enable = 0;

	if (!pIE)
		return;

	if (!phtpriv->ht_option)
		return;

	if (pmlmeext->cur_bwmode >= CHANNEL_WIDTH_80)
		return;

	if (pIE->Length > sizeof(struct HT_info_element))
		return;

	pHT_info = (struct HT_info_element *)pIE->data;

	if (hal_chk_bw_cap(adapt, BW_CAP_40M)) {
		if (pmlmeext->cur_channel > 14) {
			if (REGSTY_IS_BW_5G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		} else {
			if (REGSTY_IS_BW_2G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		}
	}

	if ((pHT_info->infos[0] & BIT(2)) && cbw40_enable) {
		new_bwmode = CHANNEL_WIDTH_40;

		switch (pHT_info->infos[0] & 0x3) {
		case 1:
			new_ch_offset = HAL_PRIME_CHNL_OFFSET_LOWER;
			break;

		case 3:
			new_ch_offset = HAL_PRIME_CHNL_OFFSET_UPPER;
			break;

		default:
			new_bwmode = CHANNEL_WIDTH_20;
			new_ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
			break;
		}
	} else {
		new_bwmode = CHANNEL_WIDTH_20;
		new_ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
	}


	if ((new_bwmode != pmlmeext->cur_bwmode || new_ch_offset != pmlmeext->cur_ch_offset)
	    && new_bwmode < pmlmeext->cur_bwmode
	   ) {
		pmlmeinfo->bwmode_updated = true;

		pmlmeext->cur_bwmode = new_bwmode;
		pmlmeext->cur_ch_offset = new_ch_offset;

		/* update HT info also */
		HT_info_handler(adapt, pIE);
	} else
		pmlmeinfo->bwmode_updated = false;


	if (pmlmeinfo->bwmode_updated) {
		struct sta_info *psta;
		struct wlan_bssid_ex	*cur_network = &(pmlmeinfo->network);
		struct sta_priv	*pstapriv = &adapt->stapriv;

		/* set_channel_bwmode(adapt, pmlmeext->cur_channel, pmlmeext->cur_ch_offset, pmlmeext->cur_bwmode); */


		/* update ap's stainfo */
		psta = rtw_get_stainfo(pstapriv, cur_network->MacAddress);
		if (psta) {
			struct ht_priv	*phtpriv_sta = &psta->htpriv;

			if (phtpriv_sta->ht_option) {
				/* bwmode				 */
				psta->cmn.bw_mode = pmlmeext->cur_bwmode;
				phtpriv_sta->ch_offset = pmlmeext->cur_ch_offset;
			} else {
				psta->cmn.bw_mode = CHANNEL_WIDTH_20;
				phtpriv_sta->ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
			}

			rtw_dm_ra_mask_wk_cmd(adapt, (u8 *)psta);
		}

		/* pmlmeinfo->bwmode_updated = false; */ /* bwmode_updated done, reset it! */
	}
}

void HT_caps_handler(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE)
{
	unsigned int	i;
	u8	rf_type = RF_1T1R;
	u8	max_AMPDU_len, min_MPDU_spacing;
	u8	cur_ldpc_cap = 0, cur_stbc_cap = 0, tx_nss = 0;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct ht_priv			*phtpriv = &pmlmepriv->htpriv;
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapt);

	if (!pIE)
		return;

	if (!phtpriv->ht_option)
		return;

	pmlmeinfo->HT_caps_enable = 1;

	for (i = 0; i < (pIE->Length); i++) {
		if (i != 2) {
			/*	Commented by Albert 2010/07/12 */
			/*	Got the endian issue here. */
			pmlmeinfo->HT_caps.u.HT_cap[i] &= (pIE->data[i]);
		} else {
			/* AMPDU Parameters field */

			/* Get MIN of MAX AMPDU Length Exp */
			if ((pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x3) > (pIE->data[i] & 0x3))
				max_AMPDU_len = (pIE->data[i] & 0x3);
			else
				max_AMPDU_len = (pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x3);

			/* Get MAX of MIN MPDU Start Spacing */
			if ((pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x1c) > (pIE->data[i] & 0x1c))
				min_MPDU_spacing = (pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x1c);
			else
				min_MPDU_spacing = (pIE->data[i] & 0x1c);

			pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para = max_AMPDU_len | min_MPDU_spacing;
		}
	}

	/*	Commented by Albert 2010/07/12 */
	/*	Have to handle the endian issue after copying. */
	/*	HT_ext_caps didn't be used yet.	 */
	pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info = pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info;
	pmlmeinfo->HT_caps.u.HT_cap_element.HT_ext_caps = pmlmeinfo->HT_caps.u.HT_cap_element.HT_ext_caps;

	/* update the MCS set */
	for (i = 0; i < 16; i++)
		pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate[i] &= pmlmeext->default_supported_mcs_set[i];

	rtw_hal_get_hwreg(adapt, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	tx_nss = rtw_min(rf_type_to_rf_tx_cnt(rf_type), hal_spec->tx_nss_num);

	switch (tx_nss) {
	case 1:
		set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_1R);
		break;
	case 2:
		set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_2R);
		break;
	case 3:
		set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_3R);
		break;
	case 4:
		set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_4R);
		break;
	default:
		RTW_WARN("rf_type:%d or tx_nss:%u is not expected\n", rf_type, hal_spec->tx_nss_num);
	}

	if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		/* Config STBC setting */
		if (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX) && GET_HT_CAP_ELE_RX_STBC(pIE->data)) {
			SET_FLAG(cur_stbc_cap, STBC_HT_ENABLE_TX);
			RTW_INFO("Enable HT Tx STBC !\n");
		}
		phtpriv->stbc_cap = cur_stbc_cap;
	} else {
		/*WIFI_STATION_STATEorI_ADHOC_STATE or WIFI_ADHOC_MASTER_STATE*/
		/* Config LDPC Coding Capability */
		if (TEST_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_TX) && GET_HT_CAP_ELE_LDPC_CAP(pIE->data)) {
			SET_FLAG(cur_ldpc_cap, (LDPC_HT_ENABLE_TX | LDPC_HT_CAP_TX));
			RTW_INFO("Enable HT Tx LDPC!\n");
		}
		phtpriv->ldpc_cap = cur_ldpc_cap;

		/* Config STBC setting */
		if (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX) && GET_HT_CAP_ELE_RX_STBC(pIE->data)) {
			SET_FLAG(cur_stbc_cap, (STBC_HT_ENABLE_TX | STBC_HT_CAP_TX));
			RTW_INFO("Enable HT Tx STBC!\n");
		}
		phtpriv->stbc_cap = cur_stbc_cap;
	}
}

void HT_info_handler(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE)
{
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct ht_priv			*phtpriv = &pmlmepriv->htpriv;

	if (!pIE)
		return;

	if (!phtpriv->ht_option)
		return;


	if (pIE->Length > sizeof(struct HT_info_element))
		return;

	pmlmeinfo->HT_info_enable = 1;
	memcpy(&(pmlmeinfo->HT_info), pIE->data, pIE->Length);
	return;
}

void HTOnAssocRsp(struct adapter *adapt)
{
	unsigned char		max_AMPDU_len;
	unsigned char		min_MPDU_spacing;
	/* struct registry_priv	 *pregpriv = &adapt->registrypriv; */
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	RTW_INFO("%s\n", __func__);

	if ((pmlmeinfo->HT_info_enable) && (pmlmeinfo->HT_caps_enable))
		pmlmeinfo->HT_enable = 1;
	else {
		pmlmeinfo->HT_enable = 0;
		/* set_channel_bwmode(adapt, pmlmeext->cur_channel, pmlmeext->cur_ch_offset, pmlmeext->cur_bwmode); */
		return;
	}

	/* handle A-MPDU parameter field */
	/*
		AMPDU_para [1:0]:Max AMPDU Len => 0:8k , 1:16k, 2:32k, 3:64k
		AMPDU_para [4:2]:Min MPDU Start Spacing
	*/
	max_AMPDU_len = pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x03;

	min_MPDU_spacing = (pmlmeinfo->HT_caps.u.HT_cap_element.AMPDU_para & 0x1c) >> 2;

	rtw_hal_set_hwreg(adapt, HW_VAR_AMPDU_MIN_SPACE, (u8 *)(&min_MPDU_spacing));

	rtw_hal_set_hwreg(adapt, HW_VAR_AMPDU_FACTOR, (u8 *)(&max_AMPDU_len));
}

void ERP_IE_handler(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE)
{
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	if (pIE->Length > 1)
		return;

	pmlmeinfo->ERP_enable = 1;
	memcpy(&(pmlmeinfo->ERP_IE), pIE->data, pIE->Length);
}

void VCS_update(struct adapter *adapt, struct sta_info *psta)
{
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	switch (pregpriv->vrtl_carrier_sense) { /* 0:off 1:on 2:auto */
	case 0: /* off */
		psta->rtsen = 0;
		psta->cts2self = 0;
		break;

	case 1: /* on */
		if (pregpriv->vcs_type == 1) { /* 1:RTS/CTS 2:CTS to self */
			psta->rtsen = 1;
			psta->cts2self = 0;
		} else {
			psta->rtsen = 0;
			psta->cts2self = 1;
		}
		break;

	case 2: /* auto */
	default:
		if (((pmlmeinfo->ERP_enable) && (pmlmeinfo->ERP_IE & BIT(1)))
			/*||(pmlmepriv->ht_op_mode & HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT)*/
		) {
			if (pregpriv->vcs_type == 1) {
				psta->rtsen = 1;
				psta->cts2self = 0;
			} else {
				psta->rtsen = 0;
				psta->cts2self = 1;
			}
		} else {
			psta->rtsen = 0;
			psta->cts2self = 0;
		}
		break;
	}
}

void	update_ldpc_stbc_cap(struct sta_info *psta)
{
	if (psta->htpriv.ht_option) {
		if (TEST_FLAG(psta->htpriv.ldpc_cap, LDPC_HT_ENABLE_TX))
			psta->cmn.ldpc_en = HT_LDPC_EN;
		else
			psta->cmn.ldpc_en = 0;

		if (TEST_FLAG(psta->htpriv.stbc_cap, STBC_HT_ENABLE_TX))
			psta->cmn.stbc_en = HT_STBC_EN;
		else
			psta->cmn.stbc_en = 0;
	} else {
		psta->cmn.ldpc_en = 0;
		psta->cmn.stbc_en = 0;
	}
}

static int check_ielen(u8 *start, uint len)
{
	int left = len;
	u8 *pos = start;
	u8 id, elen;

	while (left >= 2) {
		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left) {
			RTW_INFO("IEEE 802.11 element parse failed (id=%d elen=%d left=%lu)\n",
					id, elen, (unsigned long) left);
			return false;
		}
		if ((id == WLAN_EID_VENDOR_SPECIFIC) && (elen < 4))
				return false;

		left -= elen;
		pos += elen;
	}
	if (left)
		return false;

	return true;
}

int validate_beacon_len(u8 *pframe, u32 len)
{
	u8 ie_offset = _BEACON_IE_OFFSET_ + sizeof(struct rtw_ieee80211_hdr_3addr);

	if (len < ie_offset) {
		RTW_INFO("%s: incorrect beacon length(%d)\n", __func__, len);
		return false;
	}

	if (!check_ielen(pframe + ie_offset, len - ie_offset))
		return false;

	return true;
}

/*
 * rtw_get_bcn_keys: get beacon keys from recv frame
 *
 * TODO:
 *	WLAN_EID_COUNTRY
 *	WLAN_EID_ERP_INFO
 *	WLAN_EID_CHANNEL_SWITCH
 *	WLAN_EID_PWR_CONSTRAINT
 */
int rtw_get_bcn_keys(struct adapter *Adapter, u8 *pframe, u32 packet_len,
		     struct beacon_keys *recv_beacon)
{
	int left;
	u16 capability;
	unsigned char *pos;
	struct rtw_ieee802_11_elems elems;
	struct rtw_ieee80211_ht_cap *pht_cap = NULL;
	struct HT_info_element *pht_info = NULL;

	memset(recv_beacon, 0, sizeof(*recv_beacon));

	/* checking capabilities */
	capability = le16_to_cpu(*(__le16 *)(pframe + WLAN_HDR_A3_LEN + 10));

	/* checking IEs */
	left = packet_len - sizeof(struct rtw_ieee80211_hdr_3addr) - _BEACON_IE_OFFSET_;
	pos = pframe + sizeof(struct rtw_ieee80211_hdr_3addr) + _BEACON_IE_OFFSET_;
	if (rtw_ieee802_11_parse_elems(pos, left, &elems, 1) == ParseFailed)
		return false;

	/* check bw and channel offset */
	if (elems.ht_capabilities) {
		if (elems.ht_capabilities_len != sizeof(*pht_cap))
			return false;

		pht_cap = (struct rtw_ieee80211_ht_cap *) elems.ht_capabilities;
		recv_beacon->ht_cap_info = le16_to_cpu(pht_cap->cap_info);
	}

	if (elems.ht_operation) {
		if (elems.ht_operation_len != sizeof(*pht_info))
			return false;

		pht_info = (struct HT_info_element *) elems.ht_operation;
		recv_beacon->ht_info_infos_0_sco = pht_info->infos[0] & 0x03;
	}

	/* Checking for channel */
	if (elems.ds_params && elems.ds_params_len == sizeof(recv_beacon->bcn_channel))
		memcpy(&recv_beacon->bcn_channel, elems.ds_params,
			    sizeof(recv_beacon->bcn_channel));
	else if (pht_info)
		/* In 5G, some ap do not have DSSET IE checking HT info for channel */
		recv_beacon->bcn_channel = pht_info->primary_channel;
	else {
		/* we don't find channel IE, so don't check it */
		/* RTW_INFO("Oops: %s we don't find channel IE, so don't check it\n", __func__); */
		recv_beacon->bcn_channel = Adapter->mlmeextpriv.cur_channel;
	}

	/* checking SSID */
	if (elems.ssid) {
		if (elems.ssid_len > sizeof(recv_beacon->ssid))
			return false;

		memcpy(recv_beacon->ssid, elems.ssid, elems.ssid_len);
		recv_beacon->ssid_len = elems.ssid_len;
	} else
		; /* means hidden ssid */

	/* checking RSN first */
	if (elems.rsn_ie && elems.rsn_ie_len) {
		recv_beacon->encryp_protocol = ENCRYP_PROTOCOL_WPA2;
		rtw_parse_wpa2_ie(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
			&recv_beacon->group_cipher, &recv_beacon->pairwise_cipher,
				  &recv_beacon->is_8021x, NULL);
	}
	/* checking WPA secon */
	else if (elems.wpa_ie && elems.wpa_ie_len) {
		recv_beacon->encryp_protocol = ENCRYP_PROTOCOL_WPA;
		rtw_parse_wpa_ie(elems.wpa_ie - 2, elems.wpa_ie_len + 2,
			&recv_beacon->group_cipher, &recv_beacon->pairwise_cipher,
				 &recv_beacon->is_8021x);
	} else if (capability & BIT(4))
		recv_beacon->encryp_protocol = ENCRYP_PROTOCOL_WEP;

	return true;
}

void rtw_dump_bcn_keys(struct beacon_keys *recv_beacon)
{
	u8 ssid[IW_ESSID_MAX_SIZE + 1];

	memcpy(ssid, recv_beacon->ssid, recv_beacon->ssid_len);
	ssid[recv_beacon->ssid_len] = '\0';

	RTW_INFO("%s: ssid = %s\n", __func__, ssid);
	RTW_INFO("%s: channel = %x\n", __func__, recv_beacon->bcn_channel);
	RTW_INFO("%s: ht_cap = %x\n", __func__,	recv_beacon->ht_cap_info);
	RTW_INFO("%s: ht_info_infos_0_sco = %x\n", __func__, recv_beacon->ht_info_infos_0_sco);
	RTW_INFO("%s: sec=%d, group = %x, pair = %x, 8021X = %x\n", __func__,
		 recv_beacon->encryp_protocol, recv_beacon->group_cipher,
		 recv_beacon->pairwise_cipher, recv_beacon->is_8021x);
}

int rtw_check_bcn_info(struct adapter *Adapter, u8 *pframe, u32 packet_len)
{
	unsigned int len;
	u8 *pbssid = GetAddr3Ptr(pframe);
	struct mlme_priv *pmlmepriv = &Adapter->mlmepriv;
	struct wlan_network *cur_network = &(Adapter->mlmepriv.cur_network);
	struct beacon_keys recv_beacon;

	if (!is_client_associated_to_ap(Adapter))
		return true;

	len = packet_len - sizeof(struct rtw_ieee80211_hdr_3addr);

	if (len > MAX_IE_SZ) {
		RTW_WARN("%s IE too long for survey event\n", __func__);
		return _FAIL;
	}

	if (memcmp(cur_network->network.MacAddress, pbssid, 6)) {
		RTW_WARN("Oops: rtw_check_network_encrypt linked but recv other bssid bcn\n" MAC_FMT MAC_FMT,
			MAC_ARG(pbssid), MAC_ARG(cur_network->network.MacAddress));
		return true;
	}

	if (!rtw_get_bcn_keys(Adapter, pframe, packet_len, &recv_beacon))
		return true; /* parsing failed => broken IE */

	/* don't care hidden ssid, use current beacon ssid directly */
	if (recv_beacon.ssid_len == 0) {
		memcpy(recv_beacon.ssid, pmlmepriv->cur_beacon_keys.ssid,
			    pmlmepriv->cur_beacon_keys.ssid_len);
		recv_beacon.ssid_len = pmlmepriv->cur_beacon_keys.ssid_len;
	}

	if (!memcmp(&recv_beacon, &pmlmepriv->cur_beacon_keys, sizeof(recv_beacon)))
		pmlmepriv->new_beacon_cnts = 0;
	else if ((pmlmepriv->new_beacon_cnts == 0) ||
		memcmp(&recv_beacon, &pmlmepriv->new_beacon_keys, sizeof(recv_beacon))) {
		RTW_DBG("%s: start new beacon (seq=%d)\n", __func__, GetSequence(pframe));

		if (pmlmepriv->new_beacon_cnts == 0) {
			RTW_ERR("%s: cur beacon key\n", __func__);
			RTW_DBG_EXPR(rtw_dump_bcn_keys(&pmlmepriv->cur_beacon_keys));
		}

		RTW_DBG("%s: new beacon key\n", __func__);
		RTW_DBG_EXPR(rtw_dump_bcn_keys(&recv_beacon));

		memcpy(&pmlmepriv->new_beacon_keys, &recv_beacon, sizeof(recv_beacon));
		pmlmepriv->new_beacon_cnts = 1;
	} else {
		RTW_DBG("%s: new beacon again (seq=%d)\n", __func__, GetSequence(pframe));
		pmlmepriv->new_beacon_cnts++;
	}

	/* if counter >= max, it means beacon is changed really */
	if (pmlmepriv->new_beacon_cnts >= new_bcn_max) {
		/* check bw mode change only? */
		pmlmepriv->cur_beacon_keys.ht_cap_info = recv_beacon.ht_cap_info;
		pmlmepriv->cur_beacon_keys.ht_info_infos_0_sco = recv_beacon.ht_info_infos_0_sco;
		if (memcmp(&recv_beacon, &pmlmepriv->cur_beacon_keys,
				sizeof(recv_beacon))) {
			/* beacon is changed, have to do disconnect/connect */
			RTW_WARN("%s: new beacon occur!!\n", __func__);
			return _FAIL;
		}

		RTW_INFO("%s bw mode change\n", __func__);
		RTW_INFO("%s bcn now: ht_cap_info:%x ht_info_infos_0:%x\n", __func__,
			 cur_network->BcnInfo.ht_cap_info,
			 cur_network->BcnInfo.ht_info_infos_0);

		cur_network->BcnInfo.ht_cap_info = recv_beacon.ht_cap_info;
		cur_network->BcnInfo.ht_info_infos_0 =
			(cur_network->BcnInfo.ht_info_infos_0 & (~0x03)) |
			recv_beacon.ht_info_infos_0_sco;

		RTW_INFO("%s bcn link: ht_cap_info:%x ht_info_infos_0:%x\n", __func__,
			 cur_network->BcnInfo.ht_cap_info,
			 cur_network->BcnInfo.ht_info_infos_0);

		memcpy(&pmlmepriv->cur_beacon_keys, &recv_beacon, sizeof(recv_beacon));
		pmlmepriv->new_beacon_cnts = 0;
	}

	return _SUCCESS;
}

void update_beacon_info(struct adapter *adapt, u8 *pframe, uint pkt_len, struct sta_info *psta)
{
	unsigned int i;
	unsigned int len;
	struct ndis_802_11_variable_ies *	pIE;

	len = pkt_len - (_BEACON_IE_OFFSET_ + WLAN_HDR_A3_LEN);

	for (i = 0; i < len;) {
		pIE = (struct ndis_802_11_variable_ies *)(pframe + (_BEACON_IE_OFFSET_ + WLAN_HDR_A3_LEN) + i);

		switch (pIE->ElementID) {
		case _VENDOR_SPECIFIC_IE_:
			/* to update WMM paramter set while receiving beacon */
			if (!memcmp(pIE->data, WMM_PARA_OUI, 6) && pIE->Length == WLAN_WMM_LEN)	/* WMM */
				if (WMM_param_handler(adapt, pIE))
					 report_wmm_edca_update(adapt);
			break;

		case _HT_EXTRA_INFO_IE_:	/* HT info */
			/* HT_info_handler(adapt, pIE); */
			bwmode_update_check(adapt, pIE);
			break;
		case _ERPINFO_IE_:
			ERP_IE_handler(adapt, pIE);
			VCS_update(adapt, psta);
			break;
		default:
			break;
		}

		i += (pIE->Length + 2);
	}
}

#ifdef CONFIG_DFS
void process_csa_ie(struct adapter *adapt, u8 *pframe, uint pkt_len)
{
	unsigned int i;
	unsigned int len;
	struct ndis_802_11_variable_ies *	pIE;
	u8 new_ch_no = 0;

	if (adapt->mlmepriv.handle_dfs)
		return;

	len = pkt_len - (_BEACON_IE_OFFSET_ + WLAN_HDR_A3_LEN);

	for (i = 0; i < len;) {
		pIE = (struct ndis_802_11_variable_ies *)(pframe + (_BEACON_IE_OFFSET_ + WLAN_HDR_A3_LEN) + i);

		switch (pIE->ElementID) {
		case _CH_SWTICH_ANNOUNCE_:
			adapt->mlmepriv.handle_dfs = true;
			memcpy(&new_ch_no, pIE->data + 1, 1);
			rtw_set_csa_cmd(adapt, new_ch_no);
			break;
		default:
			break;
		}

		i += (pIE->Length + 2);
	}
}
#endif /* CONFIG_DFS */

unsigned int is_ap_in_tkip(struct adapter *adapt)
{
	u32 i;
	struct ndis_802_11_variable_ies *	pIE;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex		*cur_network = &(pmlmeinfo->network);

	if (rtw_get_capability((struct wlan_bssid_ex *)cur_network) & WLAN_CAPABILITY_PRIVACY) {
		for (i = sizeof(struct ndis_802_11_fixed_ies); i < pmlmeinfo->network.IELength;) {
			pIE = (struct ndis_802_11_variable_ies *)(pmlmeinfo->network.IEs + i);

			switch (pIE->ElementID) {
			case _VENDOR_SPECIFIC_IE_:
				if ((!memcmp(pIE->data, RTW_WPA_OUI, 4)) && (!memcmp((pIE->data + 12), WPA_TKIP_CIPHER, 4)))
					return true;
				break;

			case _RSN_IE_2_:
				if (!memcmp((pIE->data + 8), RSN_TKIP_CIPHER, 4))
					return true;

			default:
				break;
			}

			i += (pIE->Length + 2);
		}

		return false;
	} else
		return false;

}

unsigned int should_forbid_n_rate(struct adapter *adapt)
{
	u32 i;
	struct ndis_802_11_variable_ies *	pIE;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct wlan_bssid_ex  *cur_network = &pmlmepriv->cur_network.network;

	if (rtw_get_capability((struct wlan_bssid_ex *)cur_network) & WLAN_CAPABILITY_PRIVACY) {
		for (i = sizeof(struct ndis_802_11_fixed_ies); i < cur_network->IELength;) {
			pIE = (struct ndis_802_11_variable_ies *)(cur_network->IEs + i);

			switch (pIE->ElementID) {
			case _VENDOR_SPECIFIC_IE_:
				if (!memcmp(pIE->data, RTW_WPA_OUI, 4) &&
				    ((!memcmp((pIE->data + 12), WPA_CIPHER_SUITE_CCMP, 4)) ||
				     (!memcmp((pIE->data + 16), WPA_CIPHER_SUITE_CCMP, 4))))
					return false;
				break;

			case _RSN_IE_2_:
				if ((!memcmp((pIE->data + 8), RSN_CIPHER_SUITE_CCMP, 4))  ||
				    (!memcmp((pIE->data + 12), RSN_CIPHER_SUITE_CCMP, 4)))
					return false;

			default:
				break;
			}

			i += (pIE->Length + 2);
		}

		return true;
	} else
		return false;

}


unsigned int is_ap_in_wep(struct adapter *adapt)
{
	u32 i;
	struct ndis_802_11_variable_ies *	pIE;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex		*cur_network = &(pmlmeinfo->network);

	if (rtw_get_capability((struct wlan_bssid_ex *)cur_network) & WLAN_CAPABILITY_PRIVACY) {
		for (i = sizeof(struct ndis_802_11_fixed_ies); i < pmlmeinfo->network.IELength;) {
			pIE = (struct ndis_802_11_variable_ies *)(pmlmeinfo->network.IEs + i);

			switch (pIE->ElementID) {
			case _VENDOR_SPECIFIC_IE_:
				if (!memcmp(pIE->data, RTW_WPA_OUI, 4))
					return false;
				break;

			case _RSN_IE_2_:
				return false;

			default:
				break;
			}

			i += (pIE->Length + 2);
		}

		return true;
	} else
		return false;

}

int wifirate2_ratetbl_inx(unsigned char rate);
int wifirate2_ratetbl_inx(unsigned char rate)
{
	int	inx = 0;
	rate = rate & 0x7f;

	switch (rate) {
	case 54*2:
		inx = 11;
		break;

	case 48*2:
		inx = 10;
		break;

	case 36*2:
		inx = 9;
		break;

	case 24*2:
		inx = 8;
		break;

	case 18*2:
		inx = 7;
		break;

	case 12*2:
		inx = 6;
		break;

	case 9*2:
		inx = 5;
		break;

	case 6*2:
		inx = 4;
		break;

	case 11*2:
		inx = 3;
		break;
	case 11:
		inx = 2;
		break;

	case 2*2:
		inx = 1;
		break;

	case 1*2:
		inx = 0;
		break;

	}
	return inx;
}

unsigned int update_basic_rate(unsigned char *ptn, unsigned int ptn_sz)
{
	unsigned int i, num_of_rate;
	unsigned int mask = 0;

	num_of_rate = (ptn_sz > NumRates) ? NumRates : ptn_sz;

	for (i = 0; i < num_of_rate; i++) {
		if ((*(ptn + i)) & 0x80)
			mask |= 0x1 << wifirate2_ratetbl_inx(*(ptn + i));
	}
	return mask;
}

unsigned int update_supported_rate(unsigned char *ptn, unsigned int ptn_sz)
{
	unsigned int i, num_of_rate;
	unsigned int mask = 0;

	num_of_rate = (ptn_sz > NumRates) ? NumRates : ptn_sz;

	for (i = 0; i < num_of_rate; i++)
		mask |= 0x1 << wifirate2_ratetbl_inx(*(ptn + i));

	return mask;
}

int support_short_GI(struct adapter *adapt, struct HT_caps_element *pHT_caps, u8 bwmode)
{
	unsigned char					bit_offset;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	if (!(pmlmeinfo->HT_enable))
		return _FAIL;

	bit_offset = (bwmode & CHANNEL_WIDTH_40) ? 6 : 5;

	if (pHT_caps->u.HT_cap_element.HT_caps_info & cpu_to_le16((0x1 << bit_offset)))
		return _SUCCESS;
	return _FAIL;
}

unsigned char get_highest_rate_idx(u64 mask)
{
	int i;
	unsigned char rate_idx = 0;

	for (i = 63; i >= 0; i--) {
		if ((mask >> i) & 0x01) {
			rate_idx = i;
			break;
		}
	}

	return rate_idx;
}
unsigned char get_lowest_rate_idx_ex(u64 mask, int start_bit)
{
	int i;
	unsigned char rate_idx = 0;

	for (i = start_bit; i < 64; i++) {
		if ((mask >> i) & 0x01) {
			rate_idx = i;
			break;
		}
	}

	return rate_idx;
}

void Update_RA_Entry(struct adapter *adapt, struct sta_info *psta)
{
	rtw_hal_update_ra_mask(psta);
}

void set_sta_rate(struct adapter *adapt, struct sta_info *psta)
{
	/* rate adaptive	 */
	rtw_hal_update_ra_mask(psta);
}

/* Update RRSR and Rate for USERATE */
void update_tx_basic_rate(struct adapter *adapt, u8 wirelessmode)
{
	NDIS_802_11_RATES_EX	supported_rates;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct wifidirect_info	*pwdinfo = &adapt->wdinfo;

	/*	Added by Albert 2011/03/22 */
	/*	In the P2P mode, the driver should not support the b mode. */
	/*	So, the Tx packet shouldn't use the CCK rate */
	if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
		return;
#ifdef CONFIG_INTEL_WIDI
	if (adapt->mlmepriv.widi_state != INTEL_WIDI_STATE_NONE)
		return;
#endif /* CONFIG_INTEL_WIDI */

	memset(supported_rates, 0, NDIS_802_11_LENGTH_RATES_EX);

	/* clear B mod if current channel is in 5G band, avoid tx cck rate in 5G band. */
	if (pmlmeext->cur_channel > 14)
		wirelessmode &= ~(WIRELESS_11B);

	if ((wirelessmode & WIRELESS_11B) && (wirelessmode == WIRELESS_11B))
		memcpy(supported_rates, rtw_basic_rate_cck, 4);
	else if (wirelessmode & WIRELESS_11B)
		memcpy(supported_rates, rtw_basic_rate_mix, 7);
	else
		memcpy(supported_rates, rtw_basic_rate_ofdm, 3);

	if (wirelessmode & WIRELESS_11B)
		update_mgnt_tx_rate(adapt, IEEE80211_CCK_RATE_1MB);
	else
		update_mgnt_tx_rate(adapt, IEEE80211_OFDM_RATE_6MB);

	rtw_hal_set_hwreg(adapt, HW_VAR_BASIC_RATE, supported_rates);
}

unsigned char check_assoc_AP(u8 *pframe, uint len)
{
	unsigned int	i;
	struct ndis_802_11_variable_ies *	pIE;

	for (i = sizeof(struct ndis_802_11_fixed_ies); i < len;) {
		pIE = (struct ndis_802_11_variable_ies *)(pframe + i);

		switch (pIE->ElementID) {
		case _VENDOR_SPECIFIC_IE_:
			if ((!memcmp(pIE->data, ARTHEROS_OUI1, 3)) || (!memcmp(pIE->data, ARTHEROS_OUI2, 3))) {
				RTW_INFO("link to Artheros AP\n");
				return HT_IOT_PEER_ATHEROS;
			} else if ((!memcmp(pIE->data, BROADCOM_OUI1, 3))
				   || (!memcmp(pIE->data, BROADCOM_OUI2, 3))
				|| (!memcmp(pIE->data, BROADCOM_OUI3, 3))) {
				RTW_INFO("link to Broadcom AP\n");
				return HT_IOT_PEER_BROADCOM;
			} else if (!memcmp(pIE->data, MARVELL_OUI, 3)) {
				RTW_INFO("link to Marvell AP\n");
				return HT_IOT_PEER_MARVELL;
			} else if (!memcmp(pIE->data, RALINK_OUI, 3)) {
				RTW_INFO("link to Ralink AP\n");
				return HT_IOT_PEER_RALINK;
			} else if (!memcmp(pIE->data, CISCO_OUI, 3)) {
				RTW_INFO("link to Cisco AP\n");
				return HT_IOT_PEER_CISCO;
			} else if (!memcmp(pIE->data, REALTEK_OUI, 3)) {
				u32	Vender = HT_IOT_PEER_REALTEK;

				if (pIE->Length >= 5) {
					if (pIE->data[4] == 1) {
						/* if(pIE->data[5] & RT_HT_CAP_USE_LONG_PREAMBLE) */
						/*	bssDesc->BssHT.RT2RT_HT_Mode |= RT_HT_CAP_USE_LONG_PREAMBLE; */

						if (pIE->data[5] & RT_HT_CAP_USE_92SE) {
							/* bssDesc->BssHT.RT2RT_HT_Mode |= RT_HT_CAP_USE_92SE; */
							Vender = HT_IOT_PEER_REALTEK_92SE;
						}
					}

					if (pIE->data[5] & RT_HT_CAP_USE_SOFTAP)
						Vender = HT_IOT_PEER_REALTEK_SOFTAP;

					if (pIE->data[4] == 2) {
						if (pIE->data[6] & RT_HT_CAP_USE_JAGUAR_BCUT) {
							Vender = HT_IOT_PEER_REALTEK_JAGUAR_BCUTAP;
							RTW_INFO("link to Realtek JAGUAR_BCUTAP\n");
						}
						if (pIE->data[6] & RT_HT_CAP_USE_JAGUAR_CCUT) {
							Vender = HT_IOT_PEER_REALTEK_JAGUAR_CCUTAP;
							RTW_INFO("link to Realtek JAGUAR_CCUTAP\n");
						}
					}
				}

				RTW_INFO("link to Realtek AP\n");
				return Vender;
			} else if (!memcmp(pIE->data, AIRGOCAP_OUI, 3)) {
				RTW_INFO("link to Airgo Cap\n");
				return HT_IOT_PEER_AIRGO;
			} else
				break;

		default:
			break;
		}

		i += (pIE->Length + 2);
	}

	RTW_INFO("link to new AP\n");
	return HT_IOT_PEER_UNKNOWN;
}

void update_capinfo(struct adapter * Adapter, u16 updateCap)
{
	struct mlme_ext_priv	*pmlmeext = &Adapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	bool		ShortPreamble;

	/* Check preamble mode, 2005.01.06, by rcnjko. */
	/* Mark to update preamble value forever, 2008.03.18 by lanhsin */
	/* if( pMgntInfo->RegPreambleMode == PREAMBLE_AUTO ) */
	{

		if (updateCap & cShortPreamble) {
			/* Short Preamble */
			if (pmlmeinfo->preamble_mode != PREAMBLE_SHORT) { /* PREAMBLE_LONG or PREAMBLE_AUTO */
				ShortPreamble = true;
				pmlmeinfo->preamble_mode = PREAMBLE_SHORT;
				rtw_hal_set_hwreg(Adapter, HW_VAR_ACK_PREAMBLE, (u8 *)&ShortPreamble);
			}
		} else {
			/* Long Preamble */
			if (pmlmeinfo->preamble_mode != PREAMBLE_LONG) { /* PREAMBLE_SHORT or PREAMBLE_AUTO */
				ShortPreamble = false;
				pmlmeinfo->preamble_mode = PREAMBLE_LONG;
				rtw_hal_set_hwreg(Adapter, HW_VAR_ACK_PREAMBLE, (u8 *)&ShortPreamble);
			}
		}
	}

	if (updateCap & cIBSS) {
		/* Filen: See 802.11-2007 p.91 */
		pmlmeinfo->slotTime = NON_SHORT_SLOT_TIME;
	} else {
		/* Filen: See 802.11-2007 p.90 */
		if (pmlmeext->cur_wireless_mode & (WIRELESS_11_24N | WIRELESS_11A | WIRELESS_11_5N | WIRELESS_11AC))
			pmlmeinfo->slotTime = SHORT_SLOT_TIME;
		else if (pmlmeext->cur_wireless_mode & (WIRELESS_11G)) {
			if ((updateCap & cShortSlotTime) /* && (!(pMgntInfo->pHTInfo->RT2RT_HT_Mode & RT_HT_CAP_USE_LONG_PREAMBLE)) */) {
				/* Short Slot Time */
				pmlmeinfo->slotTime = SHORT_SLOT_TIME;
			} else {
				/* Long Slot Time */
				pmlmeinfo->slotTime = NON_SHORT_SLOT_TIME;
			}
		} else {
			/* B Mode */
			pmlmeinfo->slotTime = NON_SHORT_SLOT_TIME;
		}
	}

	rtw_hal_set_hwreg(Adapter, HW_VAR_SLOT_TIME, &pmlmeinfo->slotTime);

}

/*
* set adapter.mlmeextpriv.mlmext_info.HT_enable
* set adapter.mlmeextpriv.cur_wireless_mode
* set SIFS register
* set mgmt tx rate
*/
void update_wireless_mode(struct adapter *adapt)
{
	int ratelen, network_type = 0;
	u32 SIFS_Timer;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex		*cur_network = &(pmlmeinfo->network);
	unsigned char			*rate = cur_network->SupportedRates;
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	ratelen = rtw_get_rateset_len(cur_network->SupportedRates);

	if ((pmlmeinfo->HT_info_enable) && (pmlmeinfo->HT_caps_enable))
		pmlmeinfo->HT_enable = 1;

	if (pmlmeext->cur_channel > 14) {
		if (pmlmeinfo->VHT_enable)
			network_type = WIRELESS_11AC;
		else if (pmlmeinfo->HT_enable)
			network_type = WIRELESS_11_5N;

		network_type |= WIRELESS_11A;
	} else {
		if (pmlmeinfo->VHT_enable)
			network_type = WIRELESS_11AC;
		else if (pmlmeinfo->HT_enable)
			network_type = WIRELESS_11_24N;

		if ((cckratesonly_included(rate, ratelen)))
			network_type |= WIRELESS_11B;
		else if ((cckrates_included(rate, ratelen)))
			network_type |= WIRELESS_11BG;
		else
			network_type |= WIRELESS_11G;
	}

	pmlmeext->cur_wireless_mode = network_type & adapt->registrypriv.wireless_mode;
	/* RTW_INFO("network_type=%02x, adapt->registrypriv.wireless_mode=%02x\n", network_type, adapt->registrypriv.wireless_mode); */
	SIFS_Timer = 0x0a0a0808; /* 0x0808->for CCK, 0x0a0a->for OFDM
                              * change this value if having IOT issues. */

	rtw_hal_set_hwreg(adapt, HW_VAR_RESP_SIFS, (u8 *)&SIFS_Timer);

	rtw_hal_set_hwreg(adapt, HW_VAR_WIRELESS_MODE, (u8 *)&(pmlmeext->cur_wireless_mode));

	if ((pmlmeext->cur_wireless_mode & WIRELESS_11B) &&
	    (rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE) ||
	     !rtw_cfg80211_iface_has_p2p_group_cap(adapt)))
		update_mgnt_tx_rate(adapt, IEEE80211_CCK_RATE_1MB);
	else
		update_mgnt_tx_rate(adapt, IEEE80211_OFDM_RATE_6MB);
}

void update_sta_basic_rate(struct sta_info *psta, u8 wireless_mode)
{
	if (IsSupportedTxCCK(wireless_mode)) {
		/* Only B, B/G, and B/G/N AP could use CCK rate */
		memcpy(psta->bssrateset, rtw_basic_rate_cck, 4);
		psta->bssratelen = 4;
	} else {
		memcpy(psta->bssrateset, rtw_basic_rate_ofdm, 3);
		psta->bssratelen = 3;
	}
}

int rtw_ies_get_supported_rate(u8 *ies, uint ies_len, u8 *rate_set, u8 *rate_num)
{
	u8 *ie;
	unsigned int ie_len;

	if (!rate_set || !rate_num)
		return false;

	*rate_num = 0;

	ie = rtw_get_ie(ies, _SUPPORTEDRATES_IE_, &ie_len, ies_len);
	if (!ie)
		goto ext_rate;

	memcpy(rate_set, ie + 2, ie_len);
	*rate_num = ie_len;

ext_rate:
	ie = rtw_get_ie(ies, _EXT_SUPPORTEDRATES_IE_, &ie_len, ies_len);
	if (ie) {
		memcpy(rate_set + *rate_num, ie + 2, ie_len);
		*rate_num += ie_len;
	}

	if (*rate_num == 0)
		return _FAIL;

	return _SUCCESS;
}

void process_addba_req(struct adapter *adapt, u8 *paddba_req, u8 *addr)
{
	struct sta_info *psta;
	u16 tid, start_seq, param;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct ADDBA_request	*preq = (struct ADDBA_request *)paddba_req;
	u8 size, accept = false;

	psta = rtw_get_stainfo(pstapriv, addr);
	if (!psta)
		goto exit;

	start_seq = le16_to_cpu(preq->BA_starting_seqctrl) >> 4;

	param = le16_to_cpu(preq->BA_para_set);
	tid = (param >> 2) & 0x0f;


	accept = rtw_rx_ampdu_is_accept(adapt);
	if (adapt->fix_rx_ampdu_size != RX_AMPDU_SIZE_INVALID)
		size = adapt->fix_rx_ampdu_size;
	else {
		size = rtw_rx_ampdu_size(adapt);
		size = rtw_min(size, rx_ampdu_size_sta_limit(adapt, psta));
	}

	if (accept)
		rtw_addbarsp_cmd(adapt, addr, tid, 0, size, start_seq);
	else
		rtw_addbarsp_cmd(adapt, addr, tid, 37, size, start_seq); /* reject ADDBA Req */

exit:
	return;
}

void rtw_process_bar_frame(struct adapter *adapt, union recv_frame *precv_frame)
{
	struct sta_priv *pstapriv = &adapt->stapriv;
	u8 *pframe = precv_frame->u.hdr.rx_data;
	struct sta_info *psta = NULL;
	struct recv_reorder_ctrl *preorder_ctrl = NULL;
	u8 tid = 0;
	u16 start_seq=0;

	psta = rtw_get_stainfo(pstapriv, get_addr2_ptr(pframe));
	if (!psta)
		goto exit;

	tid = ((le16_to_cpu((*(__le16 *)(pframe + 16))) & 0xf000) >> 12);
	preorder_ctrl = &psta->recvreorder_ctrl[tid];
	start_seq = ((__le16_to_cpu(*(__le16 *)(pframe + 18))) >> 4);
	preorder_ctrl->indicate_seq = cpu_to_le16(start_seq);

exit:
	return;
}

void update_TSF(struct mlme_ext_priv *pmlmeext, u8 *pframe, uint len)
{
	u8 *pIE;
	__le32 *pbuf;

	pIE = pframe + sizeof(struct rtw_ieee80211_hdr_3addr);
	pbuf = (__le32 *)pIE;

	pmlmeext->TSFValue = le32_to_cpu(*(pbuf + 1));

	pmlmeext->TSFValue = pmlmeext->TSFValue << 32;

	pmlmeext->TSFValue |= le32_to_cpu(*pbuf);
}

void correct_TSF(struct adapter *adapt, struct mlme_ext_priv *pmlmeext)
{
	rtw_hal_set_hwreg(adapt, HW_VAR_CORRECT_TSF, NULL);
}

void adaptive_early_32k(struct mlme_ext_priv *pmlmeext, u8 *pframe, uint len)
{
	int i;
	u8 *pIE;
	__le32 *pbuf;
	u64 tsf = 0;
	u32 delay_ms;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);


	pmlmeext->bcn_cnt++;

	pIE = pframe + sizeof(struct rtw_ieee80211_hdr_3addr);
	pbuf = (__le32 *)pIE;

	tsf = le32_to_cpu(*(pbuf + 1));
	tsf = tsf << 32;
	tsf |= le32_to_cpu(*pbuf);

	/* RTW_INFO("%s(): tsf_upper= 0x%08x, tsf_lower=0x%08x\n", __func__, (u32)(tsf>>32), (u32)tsf); */

	/* delay = (timestamp mod 1024*100)/1000 (unit: ms) */
	/* delay_ms = do_div(tsf, (pmlmeinfo->bcn_interval*1024))/1000; */
	delay_ms = rtw_modular64(tsf, (pmlmeinfo->bcn_interval * 1024));
	delay_ms = delay_ms / 1000;

	if (delay_ms >= 8) {
		pmlmeext->bcn_delay_cnt[8]++;
		/* pmlmeext->bcn_delay_ratio[8] = (pmlmeext->bcn_delay_cnt[8] * 100) /pmlmeext->bcn_cnt; */
	} else {
		pmlmeext->bcn_delay_cnt[delay_ms]++;
		/* pmlmeext->bcn_delay_ratio[delay_ms] = (pmlmeext->bcn_delay_cnt[delay_ms] * 100) /pmlmeext->bcn_cnt; */
	}

	/*
		RTW_INFO("%s(): (a)bcn_cnt = %d\n", __func__, pmlmeext->bcn_cnt);


		for(i=0; i<9; i++)
		{
			RTW_INFO("%s():bcn_delay_cnt[%d]=%d,  bcn_delay_ratio[%d]=%d\n", __func__, i,
				pmlmeext->bcn_delay_cnt[i] , i, pmlmeext->bcn_delay_ratio[i]);
		}
	*/

	/* dump for  adaptive_early_32k */
	if (pmlmeext->bcn_cnt > 100 && (pmlmeext->adaptive_tsf_done)) {
		u8 ratio_20_delay, ratio_80_delay;
		u8 DrvBcnEarly, DrvBcnTimeOut;

		ratio_20_delay = 0;
		ratio_80_delay = 0;
		DrvBcnEarly = 0xff;
		DrvBcnTimeOut = 0xff;

		RTW_INFO("%s(): bcn_cnt = %d\n", __func__, pmlmeext->bcn_cnt);

		for (i = 0; i < 9; i++) {
			pmlmeext->bcn_delay_ratio[i] = (pmlmeext->bcn_delay_cnt[i] * 100) / pmlmeext->bcn_cnt;


			/* RTW_INFO("%s():bcn_delay_cnt[%d]=%d,  bcn_delay_ratio[%d]=%d\n", __func__, i,  */
			/*	pmlmeext->bcn_delay_cnt[i] , i, pmlmeext->bcn_delay_ratio[i]); */

			ratio_20_delay += pmlmeext->bcn_delay_ratio[i];
			ratio_80_delay += pmlmeext->bcn_delay_ratio[i];

			if (ratio_20_delay > 20 && DrvBcnEarly == 0xff) {
				DrvBcnEarly = i;
				/* RTW_INFO("%s(): DrvBcnEarly = %d\n", __func__, DrvBcnEarly); */
			}

			if (ratio_80_delay > 80 && DrvBcnTimeOut == 0xff) {
				DrvBcnTimeOut = i;
				/* RTW_INFO("%s(): DrvBcnTimeOut = %d\n", __func__, DrvBcnTimeOut); */
			}

			/* reset adaptive_early_32k cnt */
			pmlmeext->bcn_delay_cnt[i] = 0;
			pmlmeext->bcn_delay_ratio[i] = 0;
		}

		pmlmeext->DrvBcnEarly = DrvBcnEarly;
		pmlmeext->DrvBcnTimeOut = DrvBcnTimeOut;

		pmlmeext->bcn_cnt = 0;
	}

}


void beacon_timing_control(struct adapter *adapt)
{
	rtw_hal_bcn_related_reg_setting(adapt);
}

void dump_macid_map(void *sel, struct macid_bmp *map, u8 max_num)
{
	RTW_PRINT_SEL(sel, "0x%08x\n", map->m0);
#if (MACID_NUM_SW_LIMIT > 32)
	if (max_num && max_num > 32)
		RTW_PRINT_SEL(sel, "0x%08x\n", map->m1);
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	if (max_num && max_num > 64)
		RTW_PRINT_SEL(sel, "0x%08x\n", map->m2);
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	if (max_num && max_num > 96)
		RTW_PRINT_SEL(sel, "0x%08x\n", map->m3);
#endif
}

inline bool rtw_macid_is_set(struct macid_bmp *map, u8 id)
{
	if (id < 32)
		return map->m0 & BIT(id);
#if (MACID_NUM_SW_LIMIT > 32)
	else if (id < 64)
		return map->m1 & BIT(id - 32);
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	else if (id < 96)
		return map->m2 & BIT(id - 64);
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	else if (id < 128)
		return map->m3 & BIT(id - 96);
#endif
	else
		rtw_warn_on(1);

	return 0;
}

inline void rtw_macid_map_set(struct macid_bmp *map, u8 id)
{
	if (id < 32)
		map->m0 |= BIT(id);
#if (MACID_NUM_SW_LIMIT > 32)
	else if (id < 64)
		map->m1 |= BIT(id - 32);
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	else if (id < 96)
		map->m2 |= BIT(id - 64);
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	else if (id < 128)
		map->m3 |= BIT(id - 96);
#endif
	else
		rtw_warn_on(1);
}

inline void rtw_macid_map_clr(struct macid_bmp *map, u8 id)
{
	if (id < 32)
		map->m0 &= ~BIT(id);
#if (MACID_NUM_SW_LIMIT > 32)
	else if (id < 64)
		map->m1 &= ~BIT(id - 32);
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	else if (id < 96)
		map->m2 &= ~BIT(id - 64);
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	else if (id < 128)
		map->m3 &= ~BIT(id - 96);
#endif
	else
		rtw_warn_on(1);
}

inline bool rtw_macid_is_used(struct macid_ctl_t *macid_ctl, u8 id)
{
	return rtw_macid_is_set(&macid_ctl->used, id);
}

inline bool rtw_macid_is_bmc(struct macid_ctl_t *macid_ctl, u8 id)
{
	return rtw_macid_is_set(&macid_ctl->bmc, id);
}

inline u8 rtw_macid_get_iface_bmp(struct macid_ctl_t *macid_ctl, u8 id)
{
	int i;
	u8 iface_bmp = 0;

	for (i = 0; i < CONFIG_IFACE_NUMBER; i++) {
		if (rtw_macid_is_set(&macid_ctl->if_g[i], id))
			iface_bmp |= BIT(i);
	}
	return iface_bmp;
}

inline bool rtw_macid_is_iface_shared(struct macid_ctl_t *macid_ctl, u8 id)
{
	int i;
	u8 iface_bmp = 0;

	for (i = 0; i < CONFIG_IFACE_NUMBER; i++) {
		if (rtw_macid_is_set(&macid_ctl->if_g[i], id)) {
			if (iface_bmp)
				return 1;
			iface_bmp |= BIT(i);
		}
	}

	return 0;
}

inline bool rtw_macid_is_iface_specific(struct macid_ctl_t *macid_ctl, u8 id, struct adapter *adapter)
{
	int i;
	u8 iface_bmp = 0;

	for (i = 0; i < CONFIG_IFACE_NUMBER; i++) {
		if (rtw_macid_is_set(&macid_ctl->if_g[i], id)) {
			if (iface_bmp || i != adapter->iface_id)
				return 0;
			iface_bmp |= BIT(i);
		}
	}

	return iface_bmp ? 1 : 0;
}

inline s8 rtw_macid_get_ch_g(struct macid_ctl_t *macid_ctl, u8 id)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (rtw_macid_is_set(&macid_ctl->ch_g[i], id))
			return i;
	}
	return -1;
}

/*Record bc's mac-id and sec-cam-id*/
inline void rtw_iface_bcmc_id_set(struct adapter *adapt, u8 mac_id)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);

	macid_ctl->iface_bmc[adapt->iface_id] = mac_id;
}
inline u8 rtw_iface_bcmc_id_get(struct adapter *adapt)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);

	return macid_ctl->iface_bmc[adapt->iface_id];
}

void rtw_alloc_macid(struct adapter *adapt, struct sta_info *psta)
{
	int i;
	unsigned long irqL;
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	struct macid_bmp *used_map = &macid_ctl->used;
	/* static u8 last_id = 0;  for testing */
	u8 last_id = 0;
	u8 is_bc_sta = false;

	if (!memcmp(psta->cmn.mac_addr, adapter_mac_addr(adapt), ETH_ALEN)) {
		psta->cmn.mac_id = macid_ctl->num;
		return;
	}

	if (!memcmp(psta->cmn.mac_addr, bc_addr, ETH_ALEN)) {
		is_bc_sta = true;
		rtw_iface_bcmc_id_set(adapt, INVALID_SEC_MAC_CAM_ID);	/*init default value*/
	}

	if (is_bc_sta
		#ifdef CONFIG_CONCURRENT_MODE
		&& (MLME_IS_STA(adapt) || MLME_IS_NULL(adapt))
		#endif
	) {
		/* STA mode have no BMC data TX, shared with this macid */
		/* When non-concurrent, only one BMC data TX is used, shared with this macid */
		/* TODO: When concurrent, non-security BMC data TX may use this, but will not control by specific macid sleep */
		i = RTW_DEFAULT_MGMT_MACID;
		goto assigned;
	}

	_enter_critical_bh(&macid_ctl->lock, &irqL);

	for (i = last_id; i < macid_ctl->num; i++) {
		if (is_bc_sta) {
			struct cam_ctl_t *cam_ctl = dvobj_to_sec_camctl(dvobj);

			if ((!rtw_macid_is_used(macid_ctl, i)) && (!rtw_sec_camid_is_used(cam_ctl, i)))
				break;
		} else {
			if (!rtw_macid_is_used(macid_ctl, i))
				break;
		}
	}

	if (i < macid_ctl->num) {

		rtw_macid_map_set(used_map, i);

		if (is_bc_sta) {
			struct cam_ctl_t *cam_ctl = dvobj_to_sec_camctl(dvobj);

			rtw_macid_map_set(&macid_ctl->bmc, i);
			rtw_iface_bcmc_id_set(adapt, i);
			rtw_sec_cam_map_set(&cam_ctl->used, i);
		}

		rtw_macid_map_set(&macid_ctl->if_g[adapt->iface_id], i);
		macid_ctl->sta[i] = psta;

		/* TODO ch_g? */

		last_id++;
		last_id %= macid_ctl->num;
	}

	_exit_critical_bh(&macid_ctl->lock, &irqL);

	if (i >= macid_ctl->num) {
		psta->cmn.mac_id = macid_ctl->num;
		RTW_ERR(FUNC_ADPT_FMT" if%u, mac_addr:"MAC_FMT" no available macid\n"
			, FUNC_ADPT_ARG(adapt), adapt->iface_id + 1, MAC_ARG(psta->cmn.mac_addr));
		rtw_warn_on(1);
		goto exit;
	} else
		goto assigned;

assigned:
	psta->cmn.mac_id = i;
	RTW_INFO(FUNC_ADPT_FMT" if%u, mac_addr:"MAC_FMT" macid:%u\n"
		, FUNC_ADPT_ARG(adapt), adapt->iface_id + 1, MAC_ARG(psta->cmn.mac_addr), psta->cmn.mac_id);

exit:
	return;
}

void rtw_release_macid(struct adapter *adapt, struct sta_info *psta)
{
	unsigned long irqL;
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	u8 ifbmp;
	int i;

	if (!memcmp(psta->cmn.mac_addr, adapter_mac_addr(adapt), ETH_ALEN))
		goto exit;

	if (psta->cmn.mac_id >= macid_ctl->num) {
		RTW_WARN(FUNC_ADPT_FMT" if%u, mac_addr:"MAC_FMT" macid:%u not valid\n"
			, FUNC_ADPT_ARG(adapt), adapt->iface_id + 1
			, MAC_ARG(psta->cmn.mac_addr), psta->cmn.mac_id);
		rtw_warn_on(1);
		goto exit;
	}

	if (psta->cmn.mac_id == RTW_DEFAULT_MGMT_MACID)
		goto msg;

	_enter_critical_bh(&macid_ctl->lock, &irqL);

	if (!rtw_macid_is_used(macid_ctl, psta->cmn.mac_id)) {
		RTW_WARN(FUNC_ADPT_FMT" if%u, mac_addr:"MAC_FMT" macid:%u not used\n"
			, FUNC_ADPT_ARG(adapt), adapt->iface_id + 1
			, MAC_ARG(psta->cmn.mac_addr), psta->cmn.mac_id);
		_exit_critical_bh(&macid_ctl->lock, &irqL);
		rtw_warn_on(1);
		goto exit;
	}

	ifbmp = rtw_macid_get_iface_bmp(macid_ctl, psta->cmn.mac_id);
	if (!(ifbmp & BIT(adapt->iface_id))) {
		RTW_WARN(FUNC_ADPT_FMT" if%u, mac_addr:"MAC_FMT" macid:%u not used by self\n"
			, FUNC_ADPT_ARG(adapt), adapt->iface_id + 1
			, MAC_ARG(psta->cmn.mac_addr), psta->cmn.mac_id);
		_exit_critical_bh(&macid_ctl->lock, &irqL);
		rtw_warn_on(1);
		goto exit;
	}

	if (!memcmp(psta->cmn.mac_addr, bc_addr, ETH_ALEN)) {
		struct cam_ctl_t *cam_ctl = dvobj_to_sec_camctl(dvobj);
		u8 id = rtw_iface_bcmc_id_get(adapt);

		if ((id != INVALID_SEC_MAC_CAM_ID) && (id < cam_ctl->num))
			rtw_sec_cam_map_clr(&cam_ctl->used, id);

		rtw_iface_bcmc_id_set(adapt, INVALID_SEC_MAC_CAM_ID);
	}

	rtw_macid_map_clr(&macid_ctl->if_g[adapt->iface_id], psta->cmn.mac_id);

	ifbmp &= ~BIT(adapt->iface_id);
	if (!ifbmp) { /* only used by self */
		rtw_macid_map_clr(&macid_ctl->used, psta->cmn.mac_id);
		rtw_macid_map_clr(&macid_ctl->bmc, psta->cmn.mac_id);
		for (i = 0; i < 2; i++)
			rtw_macid_map_clr(&macid_ctl->ch_g[i], psta->cmn.mac_id);
		macid_ctl->sta[psta->cmn.mac_id] = NULL;
	}

	_exit_critical_bh(&macid_ctl->lock, &irqL);

msg:
	RTW_INFO(FUNC_ADPT_FMT" if%u, mac_addr:"MAC_FMT" macid:%u\n"
		, FUNC_ADPT_ARG(adapt), adapt->iface_id + 1
		, MAC_ARG(psta->cmn.mac_addr), psta->cmn.mac_id
	);

exit:
	psta->cmn.mac_id = macid_ctl->num;
}

/* For 8188E RA */
u8 rtw_search_max_mac_id(struct adapter *adapt)
{
	u8 max_mac_id = 0;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	int i;
	unsigned long irqL;

	/* TODO: Only search for connected macid? */

	_enter_critical_bh(&macid_ctl->lock, &irqL);
	for (i = (macid_ctl->num - 1); i > 0 ; i--) {
		if (rtw_macid_is_used(macid_ctl, i))
			break;
	}
	_exit_critical_bh(&macid_ctl->lock, &irqL);
	max_mac_id = i;

	return max_mac_id;
}

inline void rtw_macid_ctl_set_h2c_msr(struct macid_ctl_t *macid_ctl, u8 id, u8 h2c_msr)
{
	if (id >= macid_ctl->num) {
		rtw_warn_on(1);
		return;
	}

	macid_ctl->h2c_msr[id] = h2c_msr;
}

inline void rtw_macid_ctl_set_bw(struct macid_ctl_t *macid_ctl, u8 id, u8 bw)
{
	if (id >= macid_ctl->num) {
		rtw_warn_on(1);
		return;
	}

	macid_ctl->bw[id] = bw;
}

inline void rtw_macid_ctl_set_vht_en(struct macid_ctl_t *macid_ctl, u8 id, u8 en)
{
	if (id >= macid_ctl->num) {
		rtw_warn_on(1);
		return;
	}

	macid_ctl->vht_en[id] = en;
}

inline void rtw_macid_ctl_set_rate_bmp0(struct macid_ctl_t *macid_ctl, u8 id, u32 bmp)
{
	if (id >= macid_ctl->num) {
		rtw_warn_on(1);
		return;
	}

	macid_ctl->rate_bmp0[id] = bmp;
}

inline void rtw_macid_ctl_set_rate_bmp1(struct macid_ctl_t *macid_ctl, u8 id, u32 bmp)
{
	if (id >= macid_ctl->num) {
		rtw_warn_on(1);
		return;
	}

	macid_ctl->rate_bmp1[id] = bmp;
}

inline void rtw_macid_ctl_init_sleep_reg(struct macid_ctl_t *macid_ctl, u16 m0, u16 m1, u16 m2, u16 m3)
{
	macid_ctl->reg_sleep_m0 = m0;
#if (MACID_NUM_SW_LIMIT > 32)
	macid_ctl->reg_sleep_m1 = m1;
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	macid_ctl->reg_sleep_m2 = m2;
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	macid_ctl->reg_sleep_m3 = m3;
#endif
}

inline void rtw_macid_ctl_init(struct macid_ctl_t *macid_ctl)
{
	int i;
	u8 id = RTW_DEFAULT_MGMT_MACID;

	rtw_macid_map_set(&macid_ctl->used, id);
	rtw_macid_map_set(&macid_ctl->bmc, id);
	for (i = 0; i < CONFIG_IFACE_NUMBER; i++)
		rtw_macid_map_set(&macid_ctl->if_g[i], id);
	macid_ctl->sta[id] = NULL;

	spin_lock_init(&macid_ctl->lock);
}

inline void rtw_macid_ctl_deinit(struct macid_ctl_t *macid_ctl)
{
}

inline bool rtw_bmp_is_set(const u8 *bmp, u8 bmp_len, u8 id)
{
	if (id / 8 >= bmp_len)
		return 0;

	return bmp[id / 8] & BIT(id % 8);
}

inline void rtw_bmp_set(u8 *bmp, u8 bmp_len, u8 id)
{
	if (id / 8 < bmp_len)
		bmp[id / 8] |= BIT(id % 8);
}

inline void rtw_bmp_clear(u8 *bmp, u8 bmp_len, u8 id)
{
	if (id / 8 < bmp_len)
		bmp[id / 8] &= ~BIT(id % 8);
}

inline bool rtw_bmp_not_empty(const u8 *bmp, u8 bmp_len)
{
	int i;

	for (i = 0; i < bmp_len; i++) {
		if (bmp[i])
			return 1;
	}

	return 0;
}

inline bool rtw_bmp_not_empty_exclude_bit0(const u8 *bmp, u8 bmp_len)
{
	int i;

	for (i = 0; i < bmp_len; i++) {
		if (i == 0) {
			if (bmp[i] & 0xFE)
				return 1;
		} else {
			if (bmp[i])
				return 1;
		}
	}

	return 0;
}

/* Check the id be set or not in map , if yes , return a none zero value*/
bool rtw_tim_map_is_set(struct adapter *adapt, const u8 *map, u8 id)
{
	return rtw_bmp_is_set(map, adapt->stapriv.aid_bmp_len, id);
}

/* Set the id into map array*/
void rtw_tim_map_set(struct adapter *adapt, u8 *map, u8 id)
{
	rtw_bmp_set(map, adapt->stapriv.aid_bmp_len, id);
}

/* Clear the id from map array*/
void rtw_tim_map_clear(struct adapter *adapt, u8 *map, u8 id)
{
	rtw_bmp_clear(map, adapt->stapriv.aid_bmp_len, id);
}

/* Check have anyone bit be set , if yes return true*/
bool rtw_tim_map_anyone_be_set(struct adapter *adapt, const u8 *map)
{
	return rtw_bmp_not_empty(map, adapt->stapriv.aid_bmp_len);
}

/* Check have anyone bit be set exclude bit0 , if yes return true*/
bool rtw_tim_map_anyone_be_set_exclude_aid0(struct adapter *adapt, const u8 *map)
{
	return rtw_bmp_not_empty_exclude_bit0(map, adapt->stapriv.aid_bmp_len);
}

struct adapter *dvobj_get_port0_adapter(struct dvobj_priv *dvobj)
{
	struct adapter *port0_iface = NULL;
	int i;
	for (i = 0; i < dvobj->iface_nums; i++) {
		if (get_hw_port(dvobj->adapters[i]) == HW_PORT0)
			break;
	}

	if (i < 0 || i >= dvobj->iface_nums)
		rtw_warn_on(1);
	else
		port0_iface = dvobj->adapters[i];

	return port0_iface;
}

struct adapter *dvobj_get_unregisterd_adapter(struct dvobj_priv *dvobj)
{
	struct adapter *adapter = NULL;
	int i;

	for (i = 0; i < dvobj->iface_nums; i++) {
		if (dvobj->adapters[i]->registered == 0)
			break;
	}

	if (i < dvobj->iface_nums)
		adapter = dvobj->adapters[i];

	return adapter;
}

struct adapter *dvobj_get_adapter_by_addr(struct dvobj_priv *dvobj, u8 *addr)
{
	struct adapter *adapter = NULL;
	int i;

	for (i = 0; i < dvobj->iface_nums; i++) {
		if (!memcmp(dvobj->adapters[i]->mac_addr, addr, ETH_ALEN))
			break;
	}

	if (i < dvobj->iface_nums)
		adapter = dvobj->adapters[i];

	return adapter;
}
