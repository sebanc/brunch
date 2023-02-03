// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _IOCTL_LINUX_C_

#include <drv_types.h>
#include <rtw_mp.h>
#include <rtw_mp_ioctl.h>
#include "../../hal/phydm/phydm_precomp.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27))
#define  iwe_stream_add_event(a, b, c, d, e)  iwe_stream_add_event(b, c, d, e)
#define  iwe_stream_add_point(a, b, c, d, e)  iwe_stream_add_point(b, c, d, e)
#endif



#define RTL_IOCTL_WPA_SUPPLICANT	(SIOCIWFIRSTPRIV+30)

#define SCAN_ITEM_SIZE 768
#define MAX_CUSTOM_LEN 64
#define RATE_COUNT 4

/* combo scan */
#define WEXT_CSCAN_AMOUNT 9
#define WEXT_CSCAN_BUF_LEN		360
#define WEXT_CSCAN_HEADER		"CSCAN S\x01\x00\x00S\x00"
#define WEXT_CSCAN_HEADER_SIZE		12
#define WEXT_CSCAN_SSID_SECTION		'S'
#define WEXT_CSCAN_CHANNEL_SECTION	'C'
#define WEXT_CSCAN_NPROBE_SECTION	'N'
#define WEXT_CSCAN_ACTV_DWELL_SECTION	'A'
#define WEXT_CSCAN_PASV_DWELL_SECTION	'P'
#define WEXT_CSCAN_HOME_DWELL_SECTION	'H'
#define WEXT_CSCAN_TYPE_SECTION		'T'


extern u8 key_2char2num(u8 hch, u8 lch);
extern u8 str_2char2num(u8 hch, u8 lch);
extern void macstr2num(u8 *dst, u8 *src);
extern u8 convert_ip_addr(u8 hch, u8 mch, u8 lch);

#ifdef CONFIG_WIRELESS_EXT
static u32 rtw_rates[] = {1000000, 2000000, 5500000, 11000000,
	6000000, 9000000, 12000000, 18000000, 24000000, 36000000, 48000000, 54000000};
#endif

static const char *const iw_operation_mode[] = {
	"Auto", "Ad-Hoc", "Managed",  "Master", "Repeater", "Secondary", "Monitor"
};

/**
 * hwaddr_aton - Convert ASCII string to MAC address
 * @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 */
static int hwaddr_aton_i(const char *txt, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++) {
		int a, b;

		a = hex2num_i(*txt++);
		if (a < 0)
			return -1;
		b = hex2num_i(*txt++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
		if (i < 5 && *txt++ != ':')
			return -1;
	}

	return 0;
}

#ifdef CONFIG_SUPPORT_HW_WPS_PBC
void rtw_request_wps_pbc_event(struct adapter *adapt)
{
#ifdef RTK_DMP_PLATFORM
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 12))
	kobject_uevent(&adapt->pnetdev->dev.kobj, KOBJ_NET_PBC);
#else
	kobject_hotplug(&adapt->pnetdev->class_dev.kobj, KOBJ_NET_PBC);
#endif
#else

	if (adapt->pid[0] == 0) {
		/*	0 is the default value and it means the application monitors the HW PBC doesn't privde its pid to driver. */
		return;
	}

	rtw_signal_process(adapt->pid[0], SIGUSR1);

#endif

	rtw_led_control(adapt, LED_CTL_START_WPS_BOTTON);
}
#endif/* #ifdef CONFIG_SUPPORT_HW_WPS_PBC */

void indicate_wx_scan_complete_event(struct adapter *adapt)
{
	union iwreq_data wrqu;

	memset(&wrqu, 0, sizeof(union iwreq_data));
}

void rtw_indicate_wx_assoc_event(struct adapter *adapt)
{
	union iwreq_data wrqu;
	struct	mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex		*pnetwork = (struct wlan_bssid_ex *)(&(pmlmeinfo->network));

	memset(&wrqu, 0, sizeof(union iwreq_data));

	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
		memcpy(wrqu.ap_addr.sa_data, pnetwork->MacAddress, ETH_ALEN);
	else
		memcpy(wrqu.ap_addr.sa_data, pmlmepriv->cur_network.network.MacAddress, ETH_ALEN);

	RTW_PRINT("assoc success\n");
}

void rtw_indicate_wx_disassoc_event(struct adapter *adapt)
{
	union iwreq_data wrqu;

	memset(&wrqu, 0, sizeof(union iwreq_data));

	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
}

/*
uint	rtw_is_cckrates_included(u8 *rate)
{
		u32	i = 0;

		while(rate[i]!=0)
		{
			if  (  (((rate[i]) & 0x7f) == 2)	|| (((rate[i]) & 0x7f) == 4) ||
			(((rate[i]) & 0x7f) == 11)  || (((rate[i]) & 0x7f) == 22) )
			return true;
			i++;
		}

		return false;
}

uint	rtw_is_cckratesonly_included(u8 *rate)
{
	u32 i = 0;

	while(rate[i]!=0)
	{
			if  (  (((rate[i]) & 0x7f) != 2) && (((rate[i]) & 0x7f) != 4) &&
				(((rate[i]) & 0x7f) != 11)  && (((rate[i]) & 0x7f) != 22) )
			return false;
			i++;
	}

	return true;
}
*/

#ifdef CONFIG_WIRELESS_EXT
static int search_p2p_wfd_ie(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop)
{
	struct wifidirect_info	*pwdinfo = &adapt->wdinfo;
	if (SCAN_RESULT_ALL == pwdinfo->wfd_info->scan_result_type) {

	} else if ((SCAN_RESULT_P2P_ONLY == pwdinfo->wfd_info->scan_result_type) ||
		(SCAN_RESULT_WFD_TYPE == pwdinfo->wfd_info->scan_result_type))
	{
		if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
			u32	blnGotP2PIE = false;

			/*	User is doing the P2P device discovery */
			/*	The prefix of SSID should be "DIRECT-" and the IE should contains the P2P IE. */
			/*	If not, the driver should ignore this AP and go to the next AP. */

			/*	Verifying the SSID */
			if (!memcmp(pnetwork->network.Ssid.Ssid, pwdinfo->p2p_wildcard_ssid, P2P_WILDCARD_SSID_LEN)) {
				u32	p2pielen = 0;

				/*	Verifying the P2P IE */
				if (rtw_bss_ex_get_p2p_ie(&pnetwork->network, NULL, &p2pielen))
					blnGotP2PIE = true;
			}

			if (!blnGotP2PIE)
				return false;
		}
	}
	if (SCAN_RESULT_WFD_TYPE == pwdinfo->wfd_info->scan_result_type) {
		u32	blnGotWFD = false;
		u8 *wfd_ie;
		uint wfd_ielen = 0;

		wfd_ie = rtw_bss_ex_get_wfd_ie(&pnetwork->network, NULL, &wfd_ielen);
		if (wfd_ie) {
			u8 *wfd_devinfo;
			uint wfd_devlen;

			wfd_devinfo = rtw_get_wfd_attr_content(wfd_ie, wfd_ielen, WFD_ATTR_DEVICE_INFO, NULL, &wfd_devlen);
			if (wfd_devinfo) {
				if (pwdinfo->wfd_info->wfd_device_type == WFD_DEVINFO_PSINK) {
					/*	the first two bits will indicate the WFD device type */
					if ((wfd_devinfo[1] & 0x03) == WFD_DEVINFO_SOURCE) {
						/*	If this device is Miracast PSink device, the scan reuslt should just provide the Miracast source. */
						blnGotWFD = true;
					}
				} else if (pwdinfo->wfd_info->wfd_device_type == WFD_DEVINFO_SOURCE) {
					/*	the first two bits will indicate the WFD device type */
					if ((wfd_devinfo[1] & 0x03) == WFD_DEVINFO_PSINK) {
						/*	If this device is Miracast source device, the scan reuslt should just provide the Miracast PSink. */
						/*	Todo: How about the SSink?! */
						blnGotWFD = true;
					}
				}
			}
		}

		if (!blnGotWFD)
			return false;
	}
	return true;
}
#endif

static inline char *iwe_stream_mac_addr_proess(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	/*  AP MAC address */
	iwe->cmd = SIOCGIWAP;
	iwe->u.ap_addr.sa_family = ARPHRD_ETHER;

	memcpy(iwe->u.ap_addr.sa_data, pnetwork->network.MacAddress, ETH_ALEN);
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_ADDR_LEN);
	return start;
}
static inline char *iwe_stream_essid_proess(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{

	/* Add the ESSID */
	iwe->cmd = SIOCGIWESSID;
	iwe->u.data.flags = 1;
	iwe->u.data.length = min((u16)pnetwork->network.Ssid.SsidLength, (u16)32);
	start = iwe_stream_add_point(info, start, stop, iwe, pnetwork->network.Ssid.Ssid);
	return start;
}

static inline char *iwe_stream_chan_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	if (pnetwork->network.Configuration.DSConfig < 1 /*|| pnetwork->network.Configuration.DSConfig>14*/)
		pnetwork->network.Configuration.DSConfig = 1;

	/* Add frequency/channel */
	iwe->cmd = SIOCGIWFREQ;
	iwe->u.freq.m = rtw_ch2freq(pnetwork->network.Configuration.DSConfig) * 100000;
	iwe->u.freq.e = 1;
	iwe->u.freq.i = pnetwork->network.Configuration.DSConfig;
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_FREQ_LEN);
	return start;
}
static inline char *iwe_stream_mode_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe, u16 cap)
{
	/* Add mode */
	if (cap & (WLAN_CAPABILITY_IBSS | WLAN_CAPABILITY_BSS)) {
		iwe->cmd = SIOCGIWMODE;
		if (cap & WLAN_CAPABILITY_BSS)
			iwe->u.mode = IW_MODE_MASTER;
		else
			iwe->u.mode = IW_MODE_ADHOC;

		start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_UINT_LEN);
	}
	return start;
}
static inline char *iwe_stream_encryption_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe, u16 cap)
{

	/* Add encryption capability */
	iwe->cmd = SIOCGIWENCODE;
	if (cap & WLAN_CAPABILITY_PRIVACY)
		iwe->u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe->u.data.flags = IW_ENCODE_DISABLED;
	iwe->u.data.length = 0;
	start = iwe_stream_add_point(info, start, stop, iwe, pnetwork->network.Ssid.Ssid);
	return start;

}

static inline char *iwe_stream_protocol_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	u16 ht_cap = false;
	u32 ht_ielen = 0;
	char *p;
	u8 ie_offset = (pnetwork->network.Reserved[0] == BSS_TYPE_PROB_REQ ? 0 : 12); /* Probe Request	 */

	/* parsing HT_CAP_IE	 */
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], _HT_CAPABILITY_IE_, &ht_ielen, pnetwork->network.IELength - ie_offset);
	if (p && ht_ielen > 0)
		ht_cap = true;

	/* Add the protocol name */
	iwe->cmd = SIOCGIWNAME;
	if ((rtw_is_cckratesonly_included((u8 *)&pnetwork->network.SupportedRates))) {
		if (ht_cap)
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11bn");
		else
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11b");
	} else if ((rtw_is_cckrates_included((u8 *)&pnetwork->network.SupportedRates))) {
		if (ht_cap)
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11bgn");
		else
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11bg");
	} else {
		if (pnetwork->network.Configuration.DSConfig > 14) {
			if (ht_cap)
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11an");
			else
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11a");
		} else {
			if (ht_cap)
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11gn");
			else
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11g");
		}
	}
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_CHAR_LEN);
	return start;
}

static inline char *iwe_stream_rate_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	u32 ht_ielen = 0;
	char *p;
	u16 max_rate = 0, rate, ht_cap = false;
	u32 i = 0;
	u8 bw_40MHz = 0, short_GI = 0;
	u16 mcs_rate = 0;
	char custom[MAX_CUSTOM_LEN] = {0};
	u8 ie_offset = (pnetwork->network.Reserved[0] == BSS_TYPE_PROB_REQ ? 0 : 12); /* Probe Request	 */

	/* parsing HT_CAP_IE	 */
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], _HT_CAPABILITY_IE_, &ht_ielen, pnetwork->network.IELength - ie_offset);
	if (p && ht_ielen > 0) {
		struct rtw_ieee80211_ht_cap *pht_capie;
		ht_cap = true;
		pht_capie = (struct rtw_ieee80211_ht_cap *)(p + 2);
		memcpy(&mcs_rate , pht_capie->supp_mcs_set, 2);
		bw_40MHz = (le16_to_cpu(pht_capie->cap_info) & IEEE80211_HT_CAP_SUP_WIDTH) ? 1 : 0;
		short_GI = (le16_to_cpu(pht_capie->cap_info) & (IEEE80211_HT_CAP_SGI_20 | IEEE80211_HT_CAP_SGI_40)) ? 1 : 0;
	}

	/*Add basic and extended rates */
	p = custom;
	p += snprintf(p, MAX_CUSTOM_LEN - (p - custom), " Rates (Mb/s): ");
	while (pnetwork->network.SupportedRates[i] != 0) {
		rate = pnetwork->network.SupportedRates[i] & 0x7F;
		if (rate > max_rate)
			max_rate = rate;
		p += snprintf(p, MAX_CUSTOM_LEN - (p - custom),
			      "%d%s ", rate >> 1, (rate & 1) ? ".5" : "");
		i++;
	}
	if (ht_cap) {
		if (mcs_rate & 0x8000) /* MCS15 */
			max_rate = (bw_40MHz) ? ((short_GI) ? 300 : 270) : ((short_GI) ? 144 : 130);

		else if (mcs_rate & 0x0080) /* MCS7 */
			max_rate = (bw_40MHz) ? ((short_GI) ? 150 : 135) : ((short_GI) ? 72 : 65);
		else { /* default MCS7 */
			/* RTW_INFO("wx_get_scan, mcs_rate_bitmap=0x%x\n", mcs_rate); */
			max_rate = (bw_40MHz) ? ((short_GI) ? 150 : 135) : ((short_GI) ? 72 : 65);
		}

		max_rate = max_rate * 2; /* Mbps/2;		 */
	}

	iwe->cmd = SIOCGIWRATE;
	iwe->u.bitrate.fixed = iwe->u.bitrate.disabled = 0;
	iwe->u.bitrate.value = max_rate * 500000;
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_PARAM_LEN);
	return start ;
}

static inline char *iwe_stream_wpa_wpa2_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	int buf_size = MAX_WPA_IE_LEN * 2;
	/* u8 pbuf[buf_size]={0};	 */
	u8 *pbuf = rtw_zmalloc(buf_size);

	u8 wpa_ie[255] = {0}, rsn_ie[255] = {0};
	u16 i, wpa_len = 0, rsn_len = 0;
	u8 *p;
	int out_len = 0;


	if (pbuf) {
		p = pbuf;

		/* parsing WPA/WPA2 IE */
		if (pnetwork->network.Reserved[0] != BSS_TYPE_PROB_REQ) { /* Probe Request */
			out_len = rtw_get_sec_ie(pnetwork->network.IEs , pnetwork->network.IELength, rsn_ie, &rsn_len, wpa_ie, &wpa_len);

			if (wpa_len > 0) {

				memset(pbuf, 0, buf_size);
				p += sprintf(p, "wpa_ie=");
				for (i = 0; i < wpa_len; i++)
					p += sprintf(p, "%02x", wpa_ie[i]);

				if (wpa_len > 100) {
					printk("-----------------Len %d----------------\n", wpa_len);
					for (i = 0; i < wpa_len; i++)
						printk("%02x ", wpa_ie[i]);
					printk("\n");
					printk("-----------------Len %d----------------\n", wpa_len);
				}

				memset(iwe, 0, sizeof(*iwe));
				iwe->cmd = IWEVCUSTOM;
				iwe->u.data.length = strlen(pbuf);
				start = iwe_stream_add_point(info, start, stop, iwe, pbuf);

				memset(iwe, 0, sizeof(*iwe));
				iwe->cmd = IWEVGENIE;
				iwe->u.data.length = wpa_len;
				start = iwe_stream_add_point(info, start, stop, iwe, wpa_ie);
			}
			if (rsn_len > 0) {

				memset(pbuf, 0, buf_size);
				p += sprintf(p, "rsn_ie=");
				for (i = 0; i < rsn_len; i++)
					p += sprintf(p, "%02x", rsn_ie[i]);
				memset(iwe, 0, sizeof(*iwe));
				iwe->cmd = IWEVCUSTOM;
				iwe->u.data.length = strlen(pbuf);
				start = iwe_stream_add_point(info, start, stop, iwe, pbuf);

				memset(iwe, 0, sizeof(*iwe));
				iwe->cmd = IWEVGENIE;
				iwe->u.data.length = rsn_len;
				start = iwe_stream_add_point(info, start, stop, iwe, rsn_ie);
			}
		}

		rtw_mfree(pbuf, buf_size);
	}
	return start;
}

static inline char *iwe_stream_wps_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	/* parsing WPS IE */
	uint cnt = 0, total_ielen;
	u8 *wpsie_ptr = NULL;
	uint wps_ielen = 0;
	u8 ie_offset = (pnetwork->network.Reserved[0] == BSS_TYPE_PROB_REQ ? 0 : 12);

	u8 *ie_ptr = pnetwork->network.IEs + ie_offset;
	total_ielen = pnetwork->network.IELength - ie_offset;

	if (pnetwork->network.Reserved[0] == BSS_TYPE_PROB_REQ) { /* Probe Request */
		ie_ptr = pnetwork->network.IEs;
		total_ielen = pnetwork->network.IELength;
	} else { /* Beacon or Probe Respones */
		ie_ptr = pnetwork->network.IEs + _FIXED_IE_LENGTH_;
		total_ielen = pnetwork->network.IELength - _FIXED_IE_LENGTH_;
	}
	while (cnt < total_ielen) {
		if (rtw_is_wps_ie(&ie_ptr[cnt], &wps_ielen) && (wps_ielen > 2)) {
			wpsie_ptr = &ie_ptr[cnt];
			iwe->cmd = IWEVGENIE;
			iwe->u.data.length = (u16)wps_ielen;
			start = iwe_stream_add_point(info, start, stop, iwe, wpsie_ptr);
		}
		cnt += ie_ptr[cnt + 1] + 2; /* goto next */
	}
	return start;
}

static inline char *iwe_stream_wapi_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	return start;
}

static inline char   *iwe_stream_rssi_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	u8 ss, sq;
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct hal_com_data *pHal = GET_HAL_DATA(adapt);

	/* Add quality statistics */
	iwe->cmd = IWEVQUAL;
	iwe->u.qual.updated = IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED
			      | IW_QUAL_NOISE_INVALID
			      ;

	if (check_fwstate(pmlmepriv, _FW_LINKED) &&
	    is_same_network(&pmlmepriv->cur_network.network, &pnetwork->network, 0)) {
		ss = adapt->recvpriv.signal_strength;
		sq = adapt->recvpriv.signal_qual;
	} else {
		ss = pnetwork->network.PhyInfo.SignalStrength;
		sq = pnetwork->network.PhyInfo.SignalQuality;
	}

	/* Do signal scale mapping when using percentage as the unit of signal strength, since the scale mapping is skipped in odm */

	iwe->u.qual.level = (u8)phydm_signal_scale_mapping(&pHal->odmpriv, ss);

	iwe->u.qual.qual = (u8)sq;   /* signal quality */

#ifdef CONFIG_PLATFORM_ROCKCHIPS
	iwe->u.qual.noise = -100; /* noise level suggest by zhf@rockchips */
#else
	iwe->u.qual.noise = 0; /* noise level */
#endif /* CONFIG_PLATFORM_ROCKCHIPS */

	/* RTW_INFO("iqual=%d, ilevel=%d, inoise=%d, iupdated=%d\n", iwe.u.qual.qual, iwe.u.qual.level , iwe.u.qual.noise, iwe.u.qual.updated); */

	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_QUAL_LEN);
	return start;
}

static inline char   *iwe_stream_net_rsv_process(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop, struct iw_event *iwe)
{
	u8 buf[32] = {0};
	u8 *p, *pos;
	p = buf;
	pos = pnetwork->network.Reserved;

	p += sprintf(p, "fm=%02X%02X", pos[1], pos[0]);
	memset(iwe, 0, sizeof(*iwe));
	iwe->cmd = IWEVCUSTOM;
	iwe->u.data.length = strlen(buf);
	start = iwe_stream_add_point(info, start, stop, iwe, buf);
	return start;
}

#ifdef CONFIG_WIRELESS_EXT
static char *translate_scan(struct adapter *adapt,
		struct iw_request_info *info, struct wlan_network *pnetwork,
		char *start, char *stop)
{
	struct iw_event iwe;
	u16 cap = 0;
	__le16 le_tmp;

	memset(&iwe, 0, sizeof(iwe));
	if (false == search_p2p_wfd_ie(adapt, info, pnetwork, start, stop))
		return start;

	start = iwe_stream_mac_addr_proess(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_essid_proess(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_protocol_process(adapt, info, pnetwork, start, stop, &iwe);
	if (pnetwork->network.Reserved[0] == BSS_TYPE_PROB_REQ) /* Probe Request */
		cap = 0;
	else {
		memcpy((u8 *)&le_tmp, rtw_get_capability_from_ie(pnetwork->network.IEs), 2);
		cap = le16_to_cpu(le_tmp);
	}

	start = iwe_stream_mode_process(adapt, info, pnetwork, start, stop, &iwe, cap);
	start = iwe_stream_chan_process(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_encryption_process(adapt, info, pnetwork, start, stop, &iwe, cap);
	start = iwe_stream_rate_process(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_wpa_wpa2_process(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_wps_process(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_wapi_process(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_rssi_process(adapt, info, pnetwork, start, stop, &iwe);
	start = iwe_stream_net_rsv_process(adapt, info, pnetwork, start, stop, &iwe);

	return start;
}
#endif

static int wpa_set_auth_algs(struct net_device *dev, u32 value)
{
	struct adapter *adapt = (struct adapter *) rtw_netdev_priv(dev);
	int ret = 0;

	if ((value & AUTH_ALG_SHARED_KEY) && (value & AUTH_ALG_OPEN_SYSTEM)) {
		RTW_INFO("wpa_set_auth_algs, AUTH_ALG_SHARED_KEY and  AUTH_ALG_OPEN_SYSTEM [value:0x%x]\n", value);
		adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
		adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeAutoSwitch;
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
	} else if (value & AUTH_ALG_SHARED_KEY) {
		RTW_INFO("wpa_set_auth_algs, AUTH_ALG_SHARED_KEY  [value:0x%x]\n", value);
		adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;

#ifdef CONFIG_PLATFORM_MT53XX
		adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeAutoSwitch;
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
#else
		adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeShared;
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Shared;
#endif
	} else if (value & AUTH_ALG_OPEN_SYSTEM) {
		RTW_INFO("wpa_set_auth_algs, AUTH_ALG_OPEN_SYSTEM\n");
		/* adapt->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled; */
		if (adapt->securitypriv.ndisauthtype < Ndis802_11AuthModeWPAPSK) {
#ifdef CONFIG_PLATFORM_MT53XX
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeAutoSwitch;
			adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
#else
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeOpen;
			adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open;
#endif
		}

	} else if (value & AUTH_ALG_LEAP)
		RTW_INFO("wpa_set_auth_algs, AUTH_ALG_LEAP\n");
	else {
		RTW_INFO("wpa_set_auth_algs, error!\n");
		ret = -EINVAL;
	}

	return ret;

}

static int wpa_set_encryption(struct net_device *dev, struct ieee_param *param, u32 param_len)
{
	int ret = 0;
	u32 wep_key_idx, wep_key_len;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct wifidirect_info *pwdinfo = &adapt->wdinfo;

	param->u.crypt.err = 0;
	param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

	if (param_len < (u32)((u8 *) param->u.crypt.key - (u8 *) param) + param->u.crypt.key_len) {
		ret =  -EINVAL;
		goto exit;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {

		if (param->u.crypt.idx >= WEP_KEYS
#ifdef CONFIG_IEEE80211W
		    && param->u.crypt.idx > BIP_MAX_KEYID
#endif /* CONFIG_IEEE80211W */
		   ) {
			ret = -EINVAL;
			goto exit;
		}
	} else {
		ret = -EINVAL;
		goto exit;
	}

	if (strcmp(param->u.crypt.alg, "WEP") == 0) {
		RTW_INFO("wpa_set_encryption, crypt.alg = WEP\n");

		wep_key_idx = param->u.crypt.idx;
		wep_key_len = param->u.crypt.key_len;

		if ((wep_key_idx >= WEP_KEYS) || (wep_key_len <= 0)) {
			ret = -EINVAL;
			goto exit;
		}

		if (psecuritypriv->bWepDefaultKeyIdxSet == 0) {
			/* wep default key has not been set, so use this key index as default key.*/

			wep_key_len = wep_key_len <= 5 ? 5 : 13;

			psecuritypriv->ndisencryptstatus = Ndis802_11Encryption1Enabled;
			psecuritypriv->dot11PrivacyAlgrthm = _WEP40_;
			psecuritypriv->dot118021XGrpPrivacy = _WEP40_;

			if (wep_key_len == 13) {
				psecuritypriv->dot11PrivacyAlgrthm = _WEP104_;
				psecuritypriv->dot118021XGrpPrivacy = _WEP104_;
			}

			psecuritypriv->dot11PrivacyKeyIndex = wep_key_idx;
		}

		memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), param->u.crypt.key, wep_key_len);

		psecuritypriv->dot11DefKeylen[wep_key_idx] = wep_key_len;

		psecuritypriv->key_mask |= BIT(wep_key_idx);

		goto exit;
	}

	if (adapt->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) { /* 802_1x */
		struct sta_info *psta, *pbcmc_sta;
		struct sta_priv *pstapriv = &adapt->stapriv;

		if (check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_MP_STATE)) { /* sta mode */
			psta = rtw_get_stainfo(pstapriv, get_bssid(pmlmepriv));
			if (!psta) {
				/* DEBUG_ERR( ("Set wpa_set_encryption: Obtain Sta_info fail\n")); */
			} else {
				/* Jeff: don't disable ieee8021x_blocked while clearing key */
				if (strcmp(param->u.crypt.alg, "none") != 0)
					psta->ieee8021x_blocked = false;

				if ((adapt->securitypriv.ndisencryptstatus == Ndis802_11Encryption2Enabled) ||
				    (adapt->securitypriv.ndisencryptstatus ==  Ndis802_11Encryption3Enabled))
					psta->dot118021XPrivacy = adapt->securitypriv.dot11PrivacyAlgrthm;

				if (param->u.crypt.set_tx == 1) { /* pairwise key */
					RTW_INFO(FUNC_ADPT_FMT" set %s PTK idx:%u, len:%u\n"
						, FUNC_ADPT_ARG(adapt), param->u.crypt.alg, param->u.crypt.idx, param->u.crypt.key_len);
					memcpy(psta->dot118021x_UncstKey.skey,  param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));
					if (strcmp(param->u.crypt.alg, "TKIP") == 0) { /* set mic key */
						memcpy(psta->dot11tkiptxmickey.skey, &(param->u.crypt.key[16]), 8);
						memcpy(psta->dot11tkiprxmickey.skey, &(param->u.crypt.key[24]), 8);
						adapt->securitypriv.busetkipkey = false;
					}
					psta->dot11txpn.val = RTW_GET_LE64(param->u.crypt.seq);
					psta->dot11rxpn.val = RTW_GET_LE64(param->u.crypt.seq);
					psta->bpairwise_key_installed = true;
					rtw_setstakey_cmd(adapt, psta, UNICAST_KEY, true);

				} else { /* group key */
					if (strcmp(param->u.crypt.alg, "TKIP") == 0 || strcmp(param->u.crypt.alg, "CCMP") == 0) {
						RTW_INFO(FUNC_ADPT_FMT" set %s GTK idx:%u, len:%u\n"
							, FUNC_ADPT_ARG(adapt), param->u.crypt.alg, param->u.crypt.idx, param->u.crypt.key_len);
						memcpy(adapt->securitypriv.dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key,
							(param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));
						/* only TKIP group key need to install this */
						if (param->u.crypt.key_len > 16) {
							memcpy(adapt->securitypriv.dot118021XGrptxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[16]), 8);
							memcpy(adapt->securitypriv.dot118021XGrprxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[24]), 8);
						}
						adapt->securitypriv.binstallGrpkey = true;
						if (param->u.crypt.idx < 4) 
							memcpy(adapt->securitypriv.iv_seq[param->u.crypt.idx], param->u.crypt.seq, 8);
						adapt->securitypriv.dot118021XGrpKeyid = param->u.crypt.idx;
						rtw_set_key(adapt, &adapt->securitypriv, param->u.crypt.idx, 1, true);

					#ifdef CONFIG_IEEE80211W
					} else if (strcmp(param->u.crypt.alg, "BIP") == 0) {
						RTW_INFO(FUNC_ADPT_FMT" set IGTK idx:%u, len:%u\n"
							, FUNC_ADPT_ARG(adapt), param->u.crypt.idx, param->u.crypt.key_len);
						memcpy(adapt->securitypriv.dot11wBIPKey[param->u.crypt.idx].skey,  param->u.crypt.key,
							(param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));
						psecuritypriv->dot11wBIPKeyid = param->u.crypt.idx;
						psecuritypriv->dot11wBIPrxpn.val = RTW_GET_LE64(param->u.crypt.seq);
						psecuritypriv->binstallBIPkey = true;
					#endif /* CONFIG_IEEE80211W */

					}

					if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_PROVISIONING_ING))
						rtw_p2p_set_state(pwdinfo, P2P_STATE_PROVISIONING_DONE);
				}
			}

			pbcmc_sta = rtw_get_bcmc_stainfo(adapt);
			if (!pbcmc_sta) {
				/* DEBUG_ERR( ("Set OID_802_11_ADD_KEY: bcmc stainfo is null\n")); */
			} else {
				/* Jeff: don't disable ieee8021x_blocked while clearing key */
				if (strcmp(param->u.crypt.alg, "none") != 0)
					pbcmc_sta->ieee8021x_blocked = false;

				if ((adapt->securitypriv.ndisencryptstatus == Ndis802_11Encryption2Enabled) ||
				    (adapt->securitypriv.ndisencryptstatus ==  Ndis802_11Encryption3Enabled))
					pbcmc_sta->dot118021XPrivacy = adapt->securitypriv.dot11PrivacyAlgrthm;
			}
		} else if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) { /* adhoc mode */
		}
	}

exit:

	return ret;
}

static int rtw_set_wpa_ie(struct adapter *adapt, char *pie, unsigned short ielen)
{
	u8 *buf = NULL, *pos = NULL;
	int group_cipher = 0, pairwise_cipher = 0;
	u8 mfp_opt = MFP_NO;
	int ret = 0;
	u8 null_addr[] = {0, 0, 0, 0, 0, 0};
	struct wifidirect_info *pwdinfo = &adapt->wdinfo;

	if ((ielen > MAX_WPA_IE_LEN) || (!pie)) {
		_clr_fwstate_(&adapt->mlmepriv, WIFI_UNDER_WPS);
		if (!pie)
			return ret;
		else
			return -EINVAL;
	}

	if (ielen) {
		buf = rtw_zmalloc(ielen);
		if (!buf) {
			ret =  -ENOMEM;
			goto exit;
		}

		memcpy(buf, pie , ielen);

		/* dump */
		{
			int i;
			RTW_INFO("\n wpa_ie(length:%d):\n", ielen);
			for (i = 0; i < ielen; i = i + 8)
				RTW_INFO("0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n", buf[i], buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
		}

		pos = buf;
		if (ielen < RSN_HEADER_LEN) {
			ret  = -1;
			goto exit;
		}

		if (rtw_parse_wpa_ie(buf, ielen, &group_cipher, &pairwise_cipher, NULL) == _SUCCESS) {
			adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeWPAPSK;
			memcpy(adapt->securitypriv.supplicant_ie, &buf[0], ielen);
		}

		if (rtw_parse_wpa2_ie(buf, ielen, &group_cipher, &pairwise_cipher, NULL, &mfp_opt) == _SUCCESS) {
			adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeWPA2PSK;
			memcpy(adapt->securitypriv.supplicant_ie, &buf[0], ielen);
		}

		if (group_cipher == 0)
			group_cipher = WPA_CIPHER_NONE;
		if (pairwise_cipher == 0)
			pairwise_cipher = WPA_CIPHER_NONE;

		switch (group_cipher) {
		case WPA_CIPHER_NONE:
			adapt->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
			break;
		case WPA_CIPHER_WEP40:
			adapt->securitypriv.dot118021XGrpPrivacy = _WEP40_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
			break;
		case WPA_CIPHER_TKIP:
			adapt->securitypriv.dot118021XGrpPrivacy = _TKIP_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
			break;
		case WPA_CIPHER_CCMP:
			adapt->securitypriv.dot118021XGrpPrivacy = _AES_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
			break;
		case WPA_CIPHER_WEP104:
			adapt->securitypriv.dot118021XGrpPrivacy = _WEP104_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
			break;
		}

		switch (pairwise_cipher) {
		case WPA_CIPHER_NONE:
			adapt->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
			break;
		case WPA_CIPHER_WEP40:
			adapt->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
			break;
		case WPA_CIPHER_TKIP:
			adapt->securitypriv.dot11PrivacyAlgrthm = _TKIP_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
			break;
		case WPA_CIPHER_CCMP:
			adapt->securitypriv.dot11PrivacyAlgrthm = _AES_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
			break;
		case WPA_CIPHER_WEP104:
			adapt->securitypriv.dot11PrivacyAlgrthm = _WEP104_;
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
			break;
		}

		if (mfp_opt == MFP_INVALID) {
			RTW_INFO(FUNC_ADPT_FMT" invalid MFP setting\n", FUNC_ADPT_ARG(adapt));
			ret = -EINVAL;
			goto exit;
		}
		adapt->securitypriv.mfp_opt = mfp_opt;

		_clr_fwstate_(&adapt->mlmepriv, WIFI_UNDER_WPS);
		{/* set wps_ie	 */
			u16 cnt = 0;
			u8 eid, wps_oui[4] = {0x0, 0x50, 0xf2, 0x04};

			while (cnt < ielen) {
				eid = buf[cnt];

				if ((eid == _VENDOR_SPECIFIC_IE_) && (!memcmp(&buf[cnt + 2], wps_oui, 4))) {
					RTW_INFO("SET WPS_IE\n");

					adapt->securitypriv.wps_ie_len = ((buf[cnt + 1] + 2) < MAX_WPS_IE_LEN) ? (buf[cnt + 1] + 2) : MAX_WPS_IE_LEN;

					memcpy(adapt->securitypriv.wps_ie, &buf[cnt], adapt->securitypriv.wps_ie_len);

					set_fwstate(&adapt->mlmepriv, WIFI_UNDER_WPS);

					if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_GONEGO_OK))
						rtw_p2p_set_state(pwdinfo, P2P_STATE_PROVISIONING_ING);
					cnt += buf[cnt + 1] + 2;
					break;
				} else {
					cnt += buf[cnt + 1] + 2; /* goto next	 */
				}
			}
		}
	}

	/* TKIP and AES disallow multicast packets until installing group key */
	if (adapt->securitypriv.dot11PrivacyAlgrthm == _TKIP_
	    || adapt->securitypriv.dot11PrivacyAlgrthm == _TKIP_WTMIC_
	    || adapt->securitypriv.dot11PrivacyAlgrthm == _AES_)
		/* WPS open need to enable multicast
		 * || check_fwstate(&adapt->mlmepriv, WIFI_UNDER_WPS)) */
		rtw_hal_set_hwreg(adapt, HW_VAR_OFF_RCR_AM, null_addr);


exit:

	if (buf)
		rtw_mfree(buf, ielen);

	return ret;
}

#ifdef CONFIG_WIRELESS_EXT
static int rtw_wx_get_name(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u32 ht_ielen = 0;
	char *p;
	u8 ht_cap = false;
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex  *pcur_bss = &pmlmepriv->cur_network.network;
	NDIS_802_11_RATES_EX *prates = NULL;



	if (check_fwstate(pmlmepriv, _FW_LINKED | WIFI_ADHOC_MASTER_STATE)) {
		/* parsing HT_CAP_IE */
		p = rtw_get_ie(&pcur_bss->IEs[12], _HT_CAPABILITY_IE_, &ht_ielen, pcur_bss->IELength - 12);
		if (p && ht_ielen > 0)
			ht_cap = true;

		prates = &pcur_bss->SupportedRates;

		if (rtw_is_cckratesonly_included((u8 *)prates)) {
			if (ht_cap)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11b");
		} else if ((rtw_is_cckrates_included((u8 *)prates))) {
			if (ht_cap)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bgn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bg");
		} else {
			if (pcur_bss->Configuration.DSConfig > 14) {
				if (ht_cap)
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11an");
				else
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11a");
			} else {
				if (ht_cap)
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11gn");
				else
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11g");
			}
		}
	} else {
		/* prates = &adapt->registrypriv.dev_network.SupportedRates; */
		/* snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11g"); */
		snprintf(wrqu->name, IFNAMSIZ, "unassociated");
	}


	return 0;
}

static int rtw_wx_set_freq(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	int exp = 1, freq = 0, div = 0;

	rtw_ps_deny(adapt, PS_DENY_IOCTL);
	if (!rtw_pwr_wakeup(adapt))
		goto exit;
	if (wrqu->freq.m <= 1000) {
		if (wrqu->freq.flags == IW_FREQ_AUTO) {
			if (rtw_chset_search_ch(adapter_to_chset(adapt), wrqu->freq.m) > 0) {
				adapt->mlmeextpriv.cur_channel = wrqu->freq.m;
				RTW_INFO("%s: channel is auto, set to channel %d\n", __func__, wrqu->freq.m);
			} else {
				adapt->mlmeextpriv.cur_channel = 1;
				RTW_INFO("%s: channel is auto, Channel Plan don't match just set to channel 1\n", __func__);
			}
		} else {
			adapt->mlmeextpriv.cur_channel = wrqu->freq.m;
			RTW_INFO("%s: set to channel %d\n", __func__, adapt->mlmeextpriv.cur_channel);
		}
	} else {
		while (wrqu->freq.e) {
			exp *= 10;
			wrqu->freq.e--;
		}

		freq = wrqu->freq.m;

		while (!(freq % 10)) {
			freq /= 10;
			exp *= 10;
		}

		/* freq unit is MHz here */
		div = 1000000 / exp;

		if (div)
			freq /= div;
		else {
			div = exp / 1000000;
			freq *= div;
		}

		/* If freq is invalid, rtw_freq2ch() will return channel 1 */
		adapt->mlmeextpriv.cur_channel = rtw_freq2ch(freq);
		RTW_INFO("%s: set to channel %d\n", __func__, adapt->mlmeextpriv.cur_channel);
	}
	set_channel_bwmode(adapt, adapt->mlmeextpriv.cur_channel, HAL_PRIME_CHNL_OFFSET_DONT_CARE, CHANNEL_WIDTH_20);
exit:
	rtw_ps_deny_cancel(adapt, PS_DENY_IOCTL);

	return 0;
}

static int rtw_wx_get_freq(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex  *pcur_bss = &pmlmepriv->cur_network.network;

	if (check_fwstate(pmlmepriv, _FW_LINKED) && !check_fwstate(pmlmepriv, WIFI_MONITOR_STATE)) {

		wrqu->freq.m = rtw_ch2freq(pcur_bss->Configuration.DSConfig) * 100000;
		wrqu->freq.e = 1;
		wrqu->freq.i = pcur_bss->Configuration.DSConfig;

	} else {
		wrqu->freq.m = rtw_ch2freq(adapt->mlmeextpriv.cur_channel) * 100000;
		wrqu->freq.e = 1;
		wrqu->freq.i = adapt->mlmeextpriv.cur_channel;
	}

	return 0;
}

static int rtw_wx_set_mode(struct net_device *dev, struct iw_request_info *a,
			   union iwreq_data *wrqu, char *b)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	enum ndis_802_11_network_infrastructure networkType ;
	int ret = 0;


	if (_FAIL == rtw_pwr_wakeup(adapt)) {
		ret = -EPERM;
		goto exit;
	}

	if (!rtw_is_hw_init_completed(adapt)) {
		ret = -EPERM;
		goto exit;
	}

	/* initial default type */
	dev->type = ARPHRD_ETHER;

	if (wrqu->mode == IW_MODE_MONITOR) {
		rtw_ps_deny(adapt, PS_DENY_MONITOR_MODE);
		LeaveAllPowerSaveMode(adapt);
	} else {
		rtw_ps_deny_cancel(adapt, PS_DENY_MONITOR_MODE);
	}

	switch (wrqu->mode) {
	case IW_MODE_MONITOR:
		networkType = Ndis802_11Monitor;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
		dev->type = ARPHRD_IEEE80211_RADIOTAP; /* IEEE 802.11 + radiotap header : 803 */
		RTW_INFO("set_mode = IW_MODE_MONITOR\n");
#else
		RTW_INFO("kernel version < 2.6.24 not support IW_MODE_MONITOR\n");
#endif
		break;

	case IW_MODE_AUTO:
		networkType = Ndis802_11AutoUnknown;
		RTW_INFO("set_mode = IW_MODE_AUTO\n");
		break;
	case IW_MODE_ADHOC:
		networkType = Ndis802_11IBSS;
		RTW_INFO("set_mode = IW_MODE_ADHOC\n");
		break;
	case IW_MODE_MASTER:
		networkType = Ndis802_11APMode;
		RTW_INFO("set_mode = IW_MODE_MASTER\n");
		break;
	case IW_MODE_INFRA:
		networkType = Ndis802_11Infrastructure;
		RTW_INFO("set_mode = IW_MODE_INFRA\n");
		break;

	default:
		ret = -EINVAL;;
		goto exit;
	}

	if (!rtw_set_802_11_infrastructure_mode(adapt, networkType)) {

		ret = -EPERM;
		goto exit;

	}

	rtw_setopmode_cmd(adapt, networkType, RTW_CMDF_WAIT_ACK);

	if (check_fwstate(pmlmepriv, WIFI_MONITOR_STATE))
		rtw_indicate_connect(adapt);

exit:


	return ret;

}

static int rtw_wx_get_mode(struct net_device *dev, struct iw_request_info *a,
			   union iwreq_data *wrqu, char *b)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);



	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE))
		wrqu->mode = IW_MODE_INFRA;
	else if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) ||
		 (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)))

		wrqu->mode = IW_MODE_ADHOC;
	else if (check_fwstate(pmlmepriv, WIFI_AP_STATE))
		wrqu->mode = IW_MODE_MASTER;
	else if (check_fwstate(pmlmepriv, WIFI_MONITOR_STATE))
		wrqu->mode = IW_MODE_MONITOR;
	else
		wrqu->mode = IW_MODE_AUTO;


	return 0;

}


static int rtw_wx_set_pmkid(struct net_device *dev,
			    struct iw_request_info *a,
			    union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8          j, blInserted = false;
	int         intReturn = false;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct iw_pmksa  *pPMK = (struct iw_pmksa *) extra;
	u8     strZeroMacAddress[ETH_ALEN] = { 0x00 };
	u8     strIssueBssid[ETH_ALEN] = { 0x00 };

	memcpy(strIssueBssid, pPMK->bssid.sa_data, ETH_ALEN);
	if (pPMK->cmd == IW_PMKSA_ADD) {
		RTW_INFO("[rtw_wx_set_pmkid] IW_PMKSA_ADD!\n");
		if (!memcmp(strIssueBssid, strZeroMacAddress, ETH_ALEN))
			return intReturn ;
		else
			intReturn = true;
		blInserted = false;

		/* overwrite PMKID */
		for (j = 0 ; j < NUM_PMKID_CACHE; j++) {
			if (!memcmp(psecuritypriv->PMKIDList[j].Bssid, strIssueBssid, ETH_ALEN)) {
				/* BSSID is matched, the same AP => rewrite with new PMKID. */

				RTW_INFO("[rtw_wx_set_pmkid] BSSID exists in the PMKList.\n");

				memcpy(psecuritypriv->PMKIDList[j].PMKID, pPMK->pmkid, IW_PMKID_LEN);
				psecuritypriv->PMKIDList[j].bUsed = true;
				psecuritypriv->PMKIDIndex = j + 1;
				blInserted = true;
				break;
			}
		}

		if (!blInserted) {
			/* Find a new entry */
			RTW_INFO("[rtw_wx_set_pmkid] Use the new entry index = %d for this PMKID.\n",
				 psecuritypriv->PMKIDIndex);

			memcpy(psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].Bssid, strIssueBssid, ETH_ALEN);
			memcpy(psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].PMKID, pPMK->pmkid, IW_PMKID_LEN);

			psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].bUsed = true;
			psecuritypriv->PMKIDIndex++ ;
			if (psecuritypriv->PMKIDIndex == 16)
				psecuritypriv->PMKIDIndex = 0;
		}
	} else if (pPMK->cmd == IW_PMKSA_REMOVE) {
		RTW_INFO("[rtw_wx_set_pmkid] IW_PMKSA_REMOVE!\n");
		intReturn = true;
		for (j = 0 ; j < NUM_PMKID_CACHE; j++) {
			if (!memcmp(psecuritypriv->PMKIDList[j].Bssid, strIssueBssid, ETH_ALEN)) {
				/* BSSID is matched, the same AP => Remove this PMKID information and reset it. */
				memset(psecuritypriv->PMKIDList[j].Bssid, 0x00, ETH_ALEN);
				psecuritypriv->PMKIDList[j].bUsed = false;
				break;
			}
		}
	} else if (pPMK->cmd == IW_PMKSA_FLUSH) {
		RTW_INFO("[rtw_wx_set_pmkid] IW_PMKSA_FLUSH!\n");
		memset(&psecuritypriv->PMKIDList[0], 0x00, sizeof(struct rt_pkmid_list) * NUM_PMKID_CACHE);
		psecuritypriv->PMKIDIndex = 0;
		intReturn = true;
	}
	return intReturn ;
}

static int rtw_wx_get_sens(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
#ifdef CONFIG_PLATFORM_ROCKCHIPS
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	/*
	*  20110311 Commented by Jeff
	*  For rockchip platform's wpa_driver_wext_get_rssi
	*/
	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
		/* wrqu->sens.value=-adapt->recvpriv.signal_strength; */
		wrqu->sens.value = -adapt->recvpriv.rssi;
		/* RTW_INFO("%s: %d\n", __func__, wrqu->sens.value); */
		wrqu->sens.fixed = 0; /* no auto select */
	} else
#endif
	{
		wrqu->sens.value = 0;
		wrqu->sens.fixed = 0;	/* no auto select */
		wrqu->sens.disabled = 1;
	}
	return 0;
}

static int rtw_wx_get_range(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapt);
	u16 val;
	int i;

	wrqu->data.length = sizeof(*range);
	memset(range, 0, sizeof(*range));

	/* Let's try to keep this struct in the same order as in
	 * linux/include/wireless.h
	 */

	/* TODO: See what values we can set, and remove the ones we can't
	 * set, or fill them with some default data.
	 */

	/* ~5 Mb/s real (802.11b) */
	range->throughput = 5 * 1000 * 1000;

	/* TODO: Not used in 802.11b?
	*	range->min_nwid;	 Minimal NWID we are able to set  */
	/* TODO: Not used in 802.11b?
	*	range->max_nwid;	 Maximal NWID we are able to set  */

	/* Old Frequency (backward compat - moved lower ) */
	/*	range->old_num_channels;
	 *	range->old_num_frequency;
	 * 	range->old_freq[6];  Filler to keep "version" at the same offset  */

	/* signal level threshold range */

	/* Quality of link & SNR stuff */
	/* Quality range (link, level, noise)
	 * If the quality is absolute, it will be in the range [0 ; max_qual],
	 * if the quality is dBm, it will be in the range [max_qual ; 0].
	 * Don't forget that we use 8 bit arithmetics...
	 *
	 * If percentage range is 0~100
	 * Signal strength dbm range logical is -100 ~ 0
	 * but usually value is -90 ~ -20
	 */
	range->max_qual.qual = 100;
	/* percent values between 0 and 100. */
	range->max_qual.level = 100;
	range->max_qual.noise = 100;
	range->max_qual.updated = IW_QUAL_ALL_UPDATED; /* Updated all three */

	/* This should contain the average/typical values of the quality
	 * indicator. This should be the threshold between a "good" and
	 * a "bad" link (example : monitor going from green to orange).
	 * Currently, user space apps like quality monitors don't have any
	 * way to calibrate the measurement. With this, they can split
	 * the range between 0 and max_qual in different quality level
	 * (using a geometric subdivision centered on the average).
	 * I expect that people doing the user space apps will feedback
	 * us on which value we need to put in each driver... */
	range->avg_qual.qual = 92; /* > 8% missed beacons is 'bad' */
	/* TODO: Find real 'good' to 'bad' threshol value for RSSI */
	range->avg_qual.level = 30;
	range->avg_qual.noise = 100;
	range->avg_qual.updated = IW_QUAL_ALL_UPDATED; /* Updated all three */

	range->num_bitrates = RATE_COUNT;

	for (i = 0; i < RATE_COUNT && i < IW_MAX_BITRATES; i++)
		range->bitrate[i] = rtw_rates[i];

	range->min_frag = MIN_FRAG_THRESHOLD;
	range->max_frag = MAX_FRAG_THRESHOLD;

	range->pm_capa = 0;

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 16;

	/*	range->retry_capa;	 What retry options are supported
	 *	range->retry_flags;	 How to decode max/min retry limit
	 *	range->r_time_flags;	 How to decode max/min retry life
	 *	range->min_retry;	 Minimal number of retries
	 *	range->max_retry;	 Maximal number of retries
	 *	range->min_r_time;	 Minimal retry lifetime
	 *	range->max_r_time;	 Maximal retry lifetime  */

	for (i = 0, val = 0; i < rfctl->max_chan_nums; i++) {

		/* Include only legal frequencies for some countries */
		if (rfctl->channel_set[i].ChannelNum != 0) {
			range->freq[val].i = rfctl->channel_set[i].ChannelNum;
			range->freq[val].m = rtw_ch2freq(rfctl->channel_set[i].ChannelNum) * 100000;
			range->freq[val].e = 1;
			val++;
		}

		if (val == IW_MAX_FREQUENCIES)
			break;
	}

	range->num_channels = val;
	range->num_frequency = val;

	/* Commented by Albert 2009/10/13
	 * The following code will proivde the security capability to network manager.
	 * If the driver doesn't provide this capability to network manager,
	 * the WPA/WPA2 routers can't be choosen in the network manager. */

	/*
	#define IW_SCAN_CAPA_NONE		0x00
	#define IW_SCAN_CAPA_ESSID		0x01
	#define IW_SCAN_CAPA_BSSID		0x02
	#define IW_SCAN_CAPA_CHANNEL	0x04
	#define IW_SCAN_CAPA_MODE		0x08
	#define IW_SCAN_CAPA_RATE		0x10
	#define IW_SCAN_CAPA_TYPE		0x20
	#define IW_SCAN_CAPA_TIME		0x40
	*/

#if WIRELESS_EXT > 17
	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
			  IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;
#endif

#ifdef IW_SCAN_CAPA_ESSID /* WIRELESS_EXT > 21 */
	range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_TYPE | IW_SCAN_CAPA_BSSID |
		   IW_SCAN_CAPA_CHANNEL | IW_SCAN_CAPA_MODE | IW_SCAN_CAPA_RATE;
#endif



	return 0;

}

/* set bssid flow
 * s1. rtw_set_802_11_infrastructure_mode()
 * s2. rtw_set_802_11_authentication_mode()
 * s3. set_802_11_encryption_mode()
 * s4. rtw_set_802_11_bssid() */
static int rtw_wx_set_wap(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *awrq,
			  char *extra)
{
	unsigned long	irqL;
	uint ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct sockaddr *temp = (struct sockaddr *)awrq;
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct list_head	*phead;
	u8 *dst_bssid, *src_bssid;
	struct __queue	*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	enum ndis_802_11_authentication_mode	authmode;

#ifdef CONFIG_CONCURRENT_MODE
	if (rtw_mi_buddy_check_fwstate(adapt, _FW_UNDER_SURVEY | _FW_UNDER_LINKING)) {
		RTW_INFO("set bssid, but buddy_intf is under scanning or linking\n");

		return -EINVAL;
	}
#endif

	rtw_ps_deny(adapt, PS_DENY_JOIN);
	if (_FAIL == rtw_pwr_wakeup(adapt)) {
		ret = -1;
		goto cancel_ps_deny;
	}

	if (!adapt->bup) {
		ret = -1;
		goto cancel_ps_deny;
	}


	if (temp->sa_family != ARPHRD_ETHER) {
		ret = -EINVAL;
		goto cancel_ps_deny;
	}

	authmode = adapt->securitypriv.ndisauthtype;
	_enter_critical_bh(&queue->lock, &irqL);
	phead = get_list_head(queue);
	pmlmepriv->pscanned = get_next(phead);

	while (1) {
		if ((rtw_end_of_queue_search(phead, pmlmepriv->pscanned))) {
			break;
		}

		pnetwork = container_of(pmlmepriv->pscanned, struct wlan_network, list);

		pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

		dst_bssid = pnetwork->network.MacAddress;

		src_bssid = temp->sa_data;

		if ((!memcmp(dst_bssid, src_bssid, ETH_ALEN))) {
			if (!rtw_set_802_11_infrastructure_mode(adapt, pnetwork->network.InfrastructureMode)) {
				ret = -1;
				_exit_critical_bh(&queue->lock, &irqL);
				goto cancel_ps_deny;
			}

			break;
		}

	}
	_exit_critical_bh(&queue->lock, &irqL);

	rtw_set_802_11_authentication_mode(adapt, authmode);
	/* set_802_11_encryption_mode(adapt, adapt->securitypriv.ndisencryptstatus); */
	if (!rtw_set_802_11_bssid(adapt, temp->sa_data)) {
		ret = -1;
		goto cancel_ps_deny;
	}

cancel_ps_deny:
	rtw_ps_deny_cancel(adapt, PS_DENY_JOIN);

	return ret;
}

static int rtw_wx_get_wap(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{

	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex  *pcur_bss = &pmlmepriv->cur_network.network;

	wrqu->ap_addr.sa_family = ARPHRD_ETHER;

	memset(wrqu->ap_addr.sa_data, 0, ETH_ALEN);



	if (((check_fwstate(pmlmepriv, _FW_LINKED))) ||
	    ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) ||
	    ((check_fwstate(pmlmepriv, WIFI_AP_STATE))))

		memcpy(wrqu->ap_addr.sa_data, pcur_bss->MacAddress, ETH_ALEN);
	else
		memset(wrqu->ap_addr.sa_data, 0, ETH_ALEN);


	return 0;

}

static int rtw_wx_set_mlme(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	u16 reason;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_mlme *mlme = (struct iw_mlme *) extra;


	if (!mlme)
		return -1;

	RTW_INFO("%s\n", __func__);

	reason = mlme->reason_code;


	RTW_INFO("%s, cmd=%d, reason=%d\n", __func__, mlme->cmd, reason);


	switch (mlme->cmd) {
	case IW_MLME_DEAUTH:
		if (!rtw_set_802_11_disassociate(adapt))
			ret = -1;
		break;

	case IW_MLME_DISASSOC:
		if (!rtw_set_802_11_disassociate(adapt))
			ret = -1;

		break;

	default:
		return -EOPNOTSUPP;
	}
	return ret;
}

static int rtw_wx_set_scan(struct net_device *dev, struct iw_request_info *a,
			   union iwreq_data *wrqu, char *extra)
{
	u8 _status = false;
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct sitesurvey_parm parm;
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);

	if (rtw_is_scan_deny(adapt)) {
		indicate_wx_scan_complete_event(adapt);
		goto exit;
	}

	rtw_ps_deny(adapt, PS_DENY_SCAN);
	if (_FAIL == rtw_pwr_wakeup(adapt)) {
		ret = -1;
		goto cancel_ps_deny;
	}

	if (!rtw_is_adapter_up(adapt)) {
		ret = -1;
		goto cancel_ps_deny;
	}

	/* When Busy Traffic, driver do not site survey. So driver return success. */
	/* wpa_supplicant will not issue SIOCSIWSCAN cmd again after scan timeout. */
	/* modify by thomas 2011-02-22. */
	if (rtw_mi_busy_traffic_check(adapt, false)) {
		indicate_wx_scan_complete_event(adapt);
		goto cancel_ps_deny;
	}
	if (check_fwstate(pmlmepriv, WIFI_AP_STATE) && check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		RTW_INFO("AP mode process WPS\n");
		indicate_wx_scan_complete_event(adapt);
		goto cancel_ps_deny;
	}

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY | _FW_UNDER_LINKING)) {
		indicate_wx_scan_complete_event(adapt);
		goto cancel_ps_deny;
	}

#ifdef CONFIG_CONCURRENT_MODE
	if (rtw_mi_buddy_check_fwstate(adapt,
		       _FW_UNDER_SURVEY | _FW_UNDER_LINKING | WIFI_UNDER_WPS)) {

		indicate_wx_scan_complete_event(adapt);
		goto cancel_ps_deny;
	}
#endif

	if (pwdinfo->p2p_state != P2P_STATE_NONE) {
		rtw_p2p_set_pre_state(pwdinfo, rtw_p2p_state(pwdinfo));
		rtw_p2p_set_state(pwdinfo, P2P_STATE_FIND_PHASE_SEARCH);
		rtw_p2p_findphase_ex_set(pwdinfo, P2P_FINDPHASE_EX_FULL);
		rtw_free_network_queue(adapt, true);
	}

#if WIRELESS_EXT >= 17
	if (wrqu->data.length == sizeof(struct iw_scan_req)) {
		struct iw_scan_req *req = (struct iw_scan_req *)extra;

		if (wrqu->data.flags & IW_SCAN_THIS_ESSID) {
			int len = min((int)req->essid_len, IW_ESSID_MAX_SIZE);

			rtw_init_sitesurvey_parm(adapt, &parm);
			memcpy(&parm.ssid[0].Ssid, &req->essid, len);
			parm.ssid[0].SsidLength = len;
			parm.ssid_num = 1;

			RTW_INFO("IW_SCAN_THIS_ESSID, ssid=%s, len=%d\n", req->essid, req->essid_len);

			_status = rtw_set_802_11_bssid_list_scan(adapt, &parm);

		} else if (req->scan_type == IW_SCAN_TYPE_PASSIVE)
			RTW_INFO("rtw_wx_set_scan, req->scan_type == IW_SCAN_TYPE_PASSIVE\n");

	} else
#endif

		if (wrqu->data.length >= WEXT_CSCAN_HEADER_SIZE
		    && !memcmp(extra, WEXT_CSCAN_HEADER, WEXT_CSCAN_HEADER_SIZE)
		   ) {
			int len = wrqu->data.length - WEXT_CSCAN_HEADER_SIZE;
			char *pos = extra + WEXT_CSCAN_HEADER_SIZE;
			char section;
			char sec_len;
			int ssid_index = 0;

			/* RTW_INFO("%s COMBO_SCAN header is recognized\n", __func__); */
			rtw_init_sitesurvey_parm(adapt, &parm);

			while (len >= 1) {
				section = *(pos++);
				len -= 1;

				switch (section) {
				case WEXT_CSCAN_SSID_SECTION:
					/* RTW_INFO("WEXT_CSCAN_SSID_SECTION\n"); */
					if (len < 1) {
						len = 0;
						break;
					}

					sec_len = *(pos++);
					len -= 1;

					if (sec_len > 0 && sec_len <= len) {

						parm.ssid[ssid_index].SsidLength = sec_len;
						memcpy(&parm.ssid[ssid_index].Ssid, pos, sec_len);

						/* RTW_INFO("%s COMBO_SCAN with specific parm.ssid:%s, %d\n", __func__ */
						/*	, parm.ssid[ssid_index].Ssid, parm.ssid[ssid_index].SsidLength); */
						ssid_index++;
					}

					pos += sec_len;
					len -= sec_len;
					break;


				case WEXT_CSCAN_CHANNEL_SECTION:
					/* RTW_INFO("WEXT_CSCAN_CHANNEL_SECTION\n"); */
					pos += 1;
					len -= 1;
					break;
				case WEXT_CSCAN_ACTV_DWELL_SECTION:
					/* RTW_INFO("WEXT_CSCAN_ACTV_DWELL_SECTION\n"); */
					pos += 2;
					len -= 2;
					break;
				case WEXT_CSCAN_PASV_DWELL_SECTION:
					/* RTW_INFO("WEXT_CSCAN_PASV_DWELL_SECTION\n"); */
					pos += 2;
					len -= 2;
					break;
				case WEXT_CSCAN_HOME_DWELL_SECTION:
					/* RTW_INFO("WEXT_CSCAN_HOME_DWELL_SECTION\n"); */
					pos += 2;
					len -= 2;
					break;
				case WEXT_CSCAN_TYPE_SECTION:
					/* RTW_INFO("WEXT_CSCAN_TYPE_SECTION\n"); */
					pos += 1;
					len -= 1;
					break;
				default:
					/* RTW_INFO("Unknown CSCAN section %c\n", section); */
					len = 0; /* stop parsing */
				}
				/* RTW_INFO("len:%d\n", len); */

			}
			parm.ssid_num = ssid_index;

			/* jeff: it has still some scan paramater to parse, we only do this now... */
			_status = rtw_set_802_11_bssid_list_scan(adapt, &parm);

		} else

			_status = rtw_set_802_11_bssid_list_scan(adapt, NULL);

	if (!_status)
		ret = -1;

cancel_ps_deny:
	rtw_ps_deny_cancel(adapt, PS_DENY_SCAN);

exit:
	return ret;
}

static int rtw_wx_get_scan(struct net_device *dev, struct iw_request_info *a,
			   union iwreq_data *wrqu, char *extra)
{
	unsigned long	irqL;
	struct list_head					*plist, *phead;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct __queue				*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	char *ev = extra;
	char *stop = ev + wrqu->data.length;
	u32 ret = 0;
	u32 wait_for_surveydone;
	int wait_status;
	struct	wifidirect_info	*pwdinfo = &adapt->wdinfo;

	if (adapter_to_pwrctl(adapt)->brfoffbyhw && rtw_is_drv_stopped(adapt)) {
		ret = -EINVAL;
		goto exit;
	}

	if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
		wait_for_surveydone = 200;
	else {
		/*	P2P is disabled */
		wait_for_surveydone = 100;
	}
	wait_status = _FW_UNDER_SURVEY | _FW_UNDER_LINKING;

	while (check_fwstate(pmlmepriv, wait_status))
		return -EAGAIN;
	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		if ((stop - ev) < SCAN_ITEM_SIZE) {
			ret = -E2BIG;
			break;
		}

		pnetwork = container_of(plist, struct wlan_network, list);

		/* report network only if the current channel set contains the channel to which this network belongs */
		if (rtw_chset_search_ch(adapter_to_chset(adapt), pnetwork->network.Configuration.DSConfig) >= 0
		    && rtw_mlme_band_check(adapt, pnetwork->network.Configuration.DSConfig)
		    && rtw_validate_ssid(&(pnetwork->network.Ssid))
		   )
			ev = translate_scan(adapt, a, pnetwork, ev, stop);

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	wrqu->data.length = ev - extra;
	wrqu->data.flags = 0;

exit:

	return ret ;
}

/* set ssid flow
 * s1. rtw_set_802_11_infrastructure_mode()
 * s2. set_802_11_authenticaion_mode()
 * s3. set_802_11_encryption_mode()
 * s4. rtw_set_802_11_ssid() */
static int rtw_wx_set_essid(struct net_device *dev,
			    struct iw_request_info *a,
			    union iwreq_data *wrqu, char *extra)
{
	unsigned long irqL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct __queue *queue = &pmlmepriv->scanned_queue;
	struct list_head *phead;
	struct wlan_network *pnetwork = NULL;
	enum ndis_802_11_authentication_mode authmode;
	struct ndis_802_11_ssid ndis_ssid;
	u8 *dst_ssid, *src_ssid;
	uint ret = 0, len;

#if WIRELESS_EXT <= 20
	if ((wrqu->essid.length - 1) > IW_ESSID_MAX_SIZE) {
#else
	if (wrqu->essid.length > IW_ESSID_MAX_SIZE) {
#endif
		ret = -E2BIG;
		goto exit;
	}

	rtw_ps_deny(adapt, PS_DENY_JOIN);
	if (_FAIL == rtw_pwr_wakeup(adapt)) {
		ret = -1;
		goto cancel_ps_deny;
	}

	if (!adapt->bup) {
		ret = -1;
		goto cancel_ps_deny;
	}

	if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		ret = -1;
		goto cancel_ps_deny;
	}

#ifdef CONFIG_CONCURRENT_MODE
	if (rtw_mi_buddy_check_fwstate(adapt, _FW_UNDER_SURVEY | _FW_UNDER_LINKING)) {
		RTW_INFO("set ssid, but buddy_intf is under scanning or linking\n");
		ret = -EINVAL;
		goto cancel_ps_deny;
	}
#endif
	authmode = adapt->securitypriv.ndisauthtype;
	RTW_INFO("=>%s\n", __func__);
	if (wrqu->essid.flags && wrqu->essid.length) {
		/* Commented by Albert 20100519 */
		/* We got the codes in "set_info" function of iwconfig source code. */
		/*	========================================= */
		/*	wrq.u.essid.length = strlen(essid) + 1; */
		/*	if(we_kernel_version > 20) */
		/*		wrq.u.essid.length--; */
		/*	========================================= */
		/*	That means, if the WIRELESS_EXT less than or equal to 20, the correct ssid len should subtract 1. */
#if WIRELESS_EXT <= 20
		len = ((wrqu->essid.length - 1) < IW_ESSID_MAX_SIZE) ? (wrqu->essid.length - 1) : IW_ESSID_MAX_SIZE;
#else
		len = (wrqu->essid.length < IW_ESSID_MAX_SIZE) ? wrqu->essid.length : IW_ESSID_MAX_SIZE;
#endif

		if (wrqu->essid.length != 33)
			RTW_INFO("ssid=%s, len=%d\n", extra, wrqu->essid.length);

		memset(&ndis_ssid, 0, sizeof(struct ndis_802_11_ssid));
		ndis_ssid.SsidLength = len;
		memcpy(ndis_ssid.Ssid, extra, len);
		src_ssid = ndis_ssid.Ssid;

		_enter_critical_bh(&queue->lock, &irqL);
		phead = get_list_head(queue);
		pmlmepriv->pscanned = get_next(phead);

		while (1) {
			if (rtw_end_of_queue_search(phead, pmlmepriv->pscanned)) {
				break;
			}

			pnetwork = container_of(pmlmepriv->pscanned, struct wlan_network, list);

			pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

			dst_ssid = pnetwork->network.Ssid.Ssid;


			if ((!memcmp(dst_ssid, src_ssid, ndis_ssid.SsidLength)) &&
			    (pnetwork->network.Ssid.SsidLength == ndis_ssid.SsidLength)) {

				if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) {
					if (pnetwork->network.InfrastructureMode != pmlmepriv->cur_network.network.InfrastructureMode)
						continue;
				}

				if (!rtw_set_802_11_infrastructure_mode(adapt, pnetwork->network.InfrastructureMode)) {
					ret = -1;
					_exit_critical_bh(&queue->lock, &irqL);
					goto cancel_ps_deny;
				}

				break;
			}
		}
		_exit_critical_bh(&queue->lock, &irqL);
		rtw_set_802_11_authentication_mode(adapt, authmode);
		/* set_802_11_encryption_mode(adapt, adapt->securitypriv.ndisencryptstatus); */
		if (!rtw_set_802_11_ssid(adapt, &ndis_ssid)) {
			ret = -1;
			goto cancel_ps_deny;
		}
	}

cancel_ps_deny:
	rtw_ps_deny_cancel(adapt, PS_DENY_JOIN);

exit:
	RTW_INFO("<=%s, ret %d\n", __func__, ret);
	return ret;
}

static int rtw_wx_get_essid(struct net_device *dev,
			    struct iw_request_info *a,
			    union iwreq_data *wrqu, char *extra)
{
	u32 len, ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct wlan_bssid_ex  *pcur_bss = &pmlmepriv->cur_network.network;



	if ((check_fwstate(pmlmepriv, _FW_LINKED)) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) {
		len = pcur_bss->Ssid.SsidLength;

		wrqu->essid.length = len;

		memcpy(extra, pcur_bss->Ssid.Ssid, len);

		wrqu->essid.flags = 1;
	} else {
		ret = -1;
		goto exit;
	}

exit:


	return ret;

}

static int rtw_wx_set_rate(struct net_device *dev,
			   struct iw_request_info *a,
			   union iwreq_data *wrqu, char *extra)
{
	int	i, ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8	datarates[NumRates];
	u32	target_rate = wrqu->bitrate.value;
	u32	fixed = wrqu->bitrate.fixed;
	u32	ratevalue = 0;
	u8 mpdatarate[NumRates] = {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0xff};



	if (target_rate == -1) {
		ratevalue = 11;
		goto set_rate;
	}
	target_rate = target_rate / 100000;

	switch (target_rate) {
	case 10:
		ratevalue = 0;
		break;
	case 20:
		ratevalue = 1;
		break;
	case 55:
		ratevalue = 2;
		break;
	case 60:
		ratevalue = 3;
		break;
	case 90:
		ratevalue = 4;
		break;
	case 110:
		ratevalue = 5;
		break;
	case 120:
		ratevalue = 6;
		break;
	case 180:
		ratevalue = 7;
		break;
	case 240:
		ratevalue = 8;
		break;
	case 360:
		ratevalue = 9;
		break;
	case 480:
		ratevalue = 10;
		break;
	case 540:
		ratevalue = 11;
		break;
	default:
		ratevalue = 11;
		break;
	}

set_rate:

	for (i = 0; i < NumRates; i++) {
		if (ratevalue == mpdatarate[i]) {
			datarates[i] = mpdatarate[i];
			if (fixed == 0)
				break;
		} else
			datarates[i] = 0xff;

	}

	if (rtw_setdatarate_cmd(adapt, datarates) != _SUCCESS) {
		ret = -1;
	}


	return ret;
}

static int rtw_wx_get_rate(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	u16 max_rate = 0;

	max_rate = rtw_get_cur_max_rate((struct adapter *)rtw_netdev_priv(dev));

	if (max_rate == 0)
		return -EPERM;

	wrqu->bitrate.fixed = 0;	/* no auto select */
	wrqu->bitrate.value = max_rate * 100000;

	return 0;
}

static int rtw_wx_set_rts(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	if (wrqu->rts.disabled)
		adapt->registrypriv.rts_thresh = 2347;
	else {
		if (wrqu->rts.value < 0 ||
		    wrqu->rts.value > 2347)
			return -EINVAL;

		adapt->registrypriv.rts_thresh = wrqu->rts.value;
	}

	RTW_INFO("%s, rts_thresh=%d\n", __func__, adapt->registrypriv.rts_thresh);


	return 0;

}

static int rtw_wx_get_rts(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	RTW_INFO("%s, rts_thresh=%d\n", __func__, adapt->registrypriv.rts_thresh);

	wrqu->rts.value = adapt->registrypriv.rts_thresh;
	wrqu->rts.fixed = 0;	/* no auto select */
	/* wrqu->rts.disabled = (wrqu->rts.value == DEFAULT_RTS_THRESHOLD); */


	return 0;
}

static int rtw_wx_set_frag(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	if (wrqu->frag.disabled)
		adapt->xmitpriv.frag_len = MAX_FRAG_THRESHOLD;
	else {
		if (wrqu->frag.value < MIN_FRAG_THRESHOLD ||
		    wrqu->frag.value > MAX_FRAG_THRESHOLD)
			return -EINVAL;

		adapt->xmitpriv.frag_len = wrqu->frag.value & ~0x1;
	}

	RTW_INFO("%s, frag_len=%d\n", __func__, adapt->xmitpriv.frag_len);


	return 0;

}

static int rtw_wx_get_frag(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	RTW_INFO("%s, frag_len=%d\n", __func__, adapt->xmitpriv.frag_len);

	wrqu->frag.value = adapt->xmitpriv.frag_len;
	wrqu->frag.fixed = 0;	/* no auto select */
	/* wrqu->frag.disabled = (wrqu->frag.value == DEFAULT_FRAG_THRESHOLD); */


	return 0;
}

static int rtw_wx_get_retry(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	/* struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev); */


	wrqu->retry.value = 7;
	wrqu->retry.fixed = 0;	/* no auto select */
	wrqu->retry.disabled = 1;

	return 0;

}

static int rtw_wx_set_enc(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *keybuf)
{
	u32 key, ret = 0;
	u32 keyindex_provided;
	struct ndis_802_11_wep	 wep;
	enum ndis_802_11_authentication_mode authmode;

	struct iw_point *erq = &(wrqu->encoding);
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	RTW_INFO("+rtw_wx_set_enc, flags=0x%x\n", erq->flags);

	memset(&wep, 0, sizeof(struct ndis_802_11_wep));

	key = erq->flags & IW_ENCODE_INDEX;


	if (erq->flags & IW_ENCODE_DISABLED) {
		RTW_INFO("EncryptionDisabled\n");
		adapt->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
		adapt->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
		adapt->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open; /* open system */
		authmode = Ndis802_11AuthModeOpen;
		adapt->securitypriv.ndisauthtype = authmode;

		goto exit;
	}

	if (key) {
		if (key > WEP_KEYS)
			return -EINVAL;
		key--;
		keyindex_provided = 1;
	} else {
		keyindex_provided = 0;
		key = adapt->securitypriv.dot11PrivacyKeyIndex;
		RTW_INFO("rtw_wx_set_enc, key=%d\n", key);
	}

	/* set authentication mode	 */
	if (erq->flags & IW_ENCODE_OPEN) {
		RTW_INFO("rtw_wx_set_enc():IW_ENCODE_OPEN\n");
		adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;/* Ndis802_11EncryptionDisabled; */

#ifdef CONFIG_PLATFORM_MT53XX
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
#else
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open;
#endif

		adapt->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
		adapt->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
		authmode = Ndis802_11AuthModeOpen;
		adapt->securitypriv.ndisauthtype = authmode;
	} else if (erq->flags & IW_ENCODE_RESTRICTED) {
		RTW_INFO("rtw_wx_set_enc():IW_ENCODE_RESTRICTED\n");
		adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;

#ifdef CONFIG_PLATFORM_MT53XX
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
#else
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Shared;
#endif

		adapt->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
		adapt->securitypriv.dot118021XGrpPrivacy = _WEP40_;
		authmode = Ndis802_11AuthModeShared;
		adapt->securitypriv.ndisauthtype = authmode;
	} else {
		RTW_INFO("rtw_wx_set_enc():erq->flags=0x%x\n", erq->flags);

		adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;/* Ndis802_11EncryptionDisabled; */
		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open; /* open system */
		adapt->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
		adapt->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
		authmode = Ndis802_11AuthModeOpen;
		adapt->securitypriv.ndisauthtype = authmode;
	}

	wep.KeyIndex = key;
	if (erq->length > 0) {
		wep.KeyLength = erq->length <= 5 ? 5 : 13;

		wep.Length = wep.KeyLength + FIELD_OFFSET(struct ndis_802_11_wep, KeyMaterial);
	} else {
		wep.KeyLength = 0 ;

		if (keyindex_provided == 1) { /* set key_id only, no given KeyMaterial(erq->length==0). */
			adapt->securitypriv.dot11PrivacyKeyIndex = key;

			RTW_INFO("(keyindex_provided == 1), keyid=%d, key_len=%d\n", key, adapt->securitypriv.dot11DefKeylen[key]);

			switch (adapt->securitypriv.dot11DefKeylen[key]) {
			case 5:
				adapt->securitypriv.dot11PrivacyAlgrthm = _WEP40_;
				break;
			case 13:
				adapt->securitypriv.dot11PrivacyAlgrthm = _WEP104_;
				break;
			default:
				adapt->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
				break;
			}

			goto exit;

		}

	}

	wep.KeyIndex |= 0x80000000;

	memcpy(wep.KeyMaterial, keybuf, wep.KeyLength);

	if (!rtw_set_802_11_add_wep(adapt, &wep)) {
		if (rf_on == pwrpriv->rf_pwrstate)
			ret = -EOPNOTSUPP;
		goto exit;
	}

exit:


	return ret;

}

static int rtw_wx_get_enc(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *keybuf)
{
	uint key, ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_point *erq = &(wrqu->encoding);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);


	if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
		if (!check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) {
			erq->length = 0;
			erq->flags |= IW_ENCODE_DISABLED;
			return 0;
		}
	}


	key = erq->flags & IW_ENCODE_INDEX;

	if (key) {
		if (key > WEP_KEYS)
			return -EINVAL;
		key--;
	} else
		key = adapt->securitypriv.dot11PrivacyKeyIndex;

	erq->flags = key + 1;

	/* if(adapt->securitypriv.ndisauthtype == Ndis802_11AuthModeOpen) */
	/* { */
	/* erq->flags |= IW_ENCODE_OPEN; */
	/* }	  */

	switch (adapt->securitypriv.ndisencryptstatus) {
	case Ndis802_11EncryptionNotSupported:
	case Ndis802_11EncryptionDisabled:

		erq->length = 0;
		erq->flags |= IW_ENCODE_DISABLED;

		break;

	case Ndis802_11Encryption1Enabled:

		erq->length = adapt->securitypriv.dot11DefKeylen[key];

		if (erq->length) {
			memcpy(keybuf, adapt->securitypriv.dot11DefKey[key].skey, adapt->securitypriv.dot11DefKeylen[key]);

			erq->flags |= IW_ENCODE_ENABLED;

			if (adapt->securitypriv.ndisauthtype == Ndis802_11AuthModeOpen)
				erq->flags |= IW_ENCODE_OPEN;
			else if (adapt->securitypriv.ndisauthtype == Ndis802_11AuthModeShared)
				erq->flags |= IW_ENCODE_RESTRICTED;
		} else {
			erq->length = 0;
			erq->flags |= IW_ENCODE_DISABLED;
		}

		break;

	case Ndis802_11Encryption2Enabled:
	case Ndis802_11Encryption3Enabled:

		erq->length = 16;
		erq->flags |= (IW_ENCODE_ENABLED | IW_ENCODE_OPEN | IW_ENCODE_NOKEY);

		break;

	default:
		erq->length = 0;
		erq->flags |= IW_ENCODE_DISABLED;

		break;

	}


	return ret;

}

static int rtw_wx_get_power(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	/* struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev); */

	wrqu->power.value = 0;
	wrqu->power.fixed = 0;	/* no auto select */
	wrqu->power.disabled = 1;

	return 0;

}

static int rtw_wx_set_gen_ie(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int ret;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	ret = rtw_set_wpa_ie(adapt, extra, wrqu->data.length);

	return ret;
}

static int rtw_wx_set_auth(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_param *param = (struct iw_param *)&(wrqu->param);
	int ret = 0;

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
		break;
	case IW_AUTH_CIPHER_PAIRWISE:
		break;
	case IW_AUTH_CIPHER_GROUP:
		break;
	case IW_AUTH_KEY_MGMT:
		break;
	case IW_AUTH_TKIP_COUNTERMEASURES: {
		if (param->value) {
			/* wpa_supplicant is enabling the tkip countermeasure. */
			adapt->securitypriv.btkip_countermeasure = true;
		} else {
			/* wpa_supplicant is disabling the tkip countermeasure. */
			adapt->securitypriv.btkip_countermeasure = false;
		}
		break;
	}
	case IW_AUTH_DROP_UNENCRYPTED: {
		/* HACK:
		 *
		 * wpa_supplicant calls set_wpa_enabled when the driver
		 * is loaded and unloaded, regardless of if WPA is being
		 * used.  No other calls are made which can be used to
		 * determine if encryption will be used or not prior to
		 * association being expected.  If encryption is not being
		 * used, drop_unencrypted is set to false, else true -- we
		 * can use this to determine if the CAP_PRIVACY_ON bit should
		 * be set.
		 */

		if (adapt->securitypriv.ndisencryptstatus == Ndis802_11Encryption1Enabled) {
			break;/* it means init value, or using wep, ndisencryptstatus = Ndis802_11Encryption1Enabled, */
			/* then it needn't reset it; */
		}

		if (param->value) {
			adapt->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
			adapt->securitypriv.dot11PrivacyAlgrthm = _NO_PRIVACY_;
			adapt->securitypriv.dot118021XGrpPrivacy = _NO_PRIVACY_;
			adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open; /* open system */
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeOpen;
		}

		break;
	}

	case IW_AUTH_80211_AUTH_ALG:
		/*
		 *  It's the starting point of a link layer connection using wpa_supplicant
		*/
		if (check_fwstate(&adapt->mlmepriv, _FW_LINKED)) {
			LeaveAllPowerSaveMode(adapt);
			rtw_disassoc_cmd(adapt, 500, RTW_CMDF_DIRECTLY);
			RTW_INFO("%s...call rtw_indicate_disconnect\n ", __func__);
			rtw_indicate_disconnect(adapt, 0, false);
			rtw_free_assoc_resources(adapt, 1);
		}


		ret = wpa_set_auth_algs(dev, (u32)param->value);

		break;
	case IW_AUTH_WPA_ENABLED:
		break;

	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		break;

	case IW_AUTH_PRIVACY_INVOKED:
		break;
	default:
		return -EOPNOTSUPP;
	}

	return ret;
}

static int rtw_wx_set_enc_ext(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	char *alg_name;
	u32 param_len;
	struct ieee_param *param = NULL;
	struct iw_point *pencoding = &wrqu->encoding;
	struct iw_encode_ext *pext = (struct iw_encode_ext *)extra;
	int ret = 0;

	param_len = sizeof(struct ieee_param) + pext->key_len;
	param = (struct ieee_param *)rtw_malloc(param_len);
	if (!param)
		return -1;

	memset(param, 0, param_len);

	param->cmd = IEEE_CMD_SET_ENCRYPTION;
	memset(param->sta_addr, 0xff, ETH_ALEN);


	switch (pext->alg) {
	case IW_ENCODE_ALG_NONE:
		/* todo: remove key */
		/* remove = 1;	 */
		alg_name = "none";
		break;
	case IW_ENCODE_ALG_WEP:
		alg_name = "WEP";
		break;
	case IW_ENCODE_ALG_TKIP:
		alg_name = "TKIP";
		break;
	case IW_ENCODE_ALG_CCMP:
		alg_name = "CCMP";
		break;
#ifdef CONFIG_IEEE80211W
	case IW_ENCODE_ALG_AES_CMAC:
		alg_name = "BIP";
		break;
#endif /* CONFIG_IEEE80211W */
	default:
		ret = -1;
		goto exit;
	}

	strncpy((char *)param->u.crypt.alg, alg_name, IEEE_CRYPT_ALG_NAME_LEN);

	if (pext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
		param->u.crypt.set_tx = 1;

	/* cliW: WEP does not have group key
	 * just not checking GROUP key setting
	 */
	if ((pext->alg != IW_ENCODE_ALG_WEP) &&
	    ((pext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
#ifdef CONFIG_IEEE80211W
	     || (pext->ext_flags & IW_ENCODE_ALG_AES_CMAC)
#endif /* CONFIG_IEEE80211W */
	    ))
		param->u.crypt.set_tx = 0;

	param->u.crypt.idx = (pencoding->flags & 0x00FF) - 1 ;

	if (pext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID)
		memcpy(param->u.crypt.seq, pext->rx_seq, 8);

	if (pext->key_len) {
		param->u.crypt.key_len = pext->key_len;
		/* memcpy(param + 1, pext + 1, pext->key_len); */
		memcpy(param->u.crypt.key, pext + 1, pext->key_len);
	}

	if (pencoding->flags & IW_ENCODE_DISABLED) {
		/* todo: remove key */
		/* remove = 1; */
	}

	ret =  wpa_set_encryption(dev, param, param_len);

exit:
	if (param)
		rtw_mfree((u8 *)param, param_len);

	return ret;
}


static int rtw_wx_get_nick(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	/* struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev); */
	/* struct mlme_priv *pmlmepriv = &(adapt->mlmepriv); */
	/* struct security_priv *psecuritypriv = &adapt->securitypriv; */

	if (extra) {
		wrqu->data.length = 14;
		wrqu->data.flags = 1;
		memcpy(extra, "<WIFI@REALTEK>", 14);
	}
	return 0;
}
#endif

static int rtw_wx_read32(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
	struct adapter * adapt;
	struct iw_point *p;
	u16 len;
	u32 addr;
	u32 data32;
	u32 bytes;
	u8 *ptmp;
	int ret;


	ret = 0;
	adapt = (struct adapter *)rtw_netdev_priv(dev);
	p = &wrqu->data;
	len = p->length;
	if (0 == len)
		return -EINVAL;

	ptmp = (u8 *)rtw_malloc(len);
	if (!ptmp)
		return -ENOMEM;

	if (copy_from_user(ptmp, p->pointer, len)) {
		ret = -EFAULT;
		goto exit;
	}

	bytes = 0;
	addr = 0;
	sscanf(ptmp, "%d,%x", &bytes, &addr);

	switch (bytes) {
	case 1:
		data32 = rtw_read8(adapt, addr);
		sprintf(extra, "0x%02X", data32);
		break;
	case 2:
		data32 = rtw_read16(adapt, addr);
		sprintf(extra, "0x%04X", data32);
		break;
	case 4:
		data32 = rtw_read32(adapt, addr);
		sprintf(extra, "0x%08X", data32);
		break;
	default:
		RTW_INFO("%s: usage> read [bytes],[address(hex)]\n", __func__);
		ret = -EINVAL;
		goto exit;
	}
	RTW_INFO("%s: addr=0x%08X data=%s\n", __func__, addr, extra);

exit:
	rtw_mfree(ptmp, len);

	return 0;
}

static int rtw_wx_write32(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct adapter * adapt = (struct adapter *)rtw_netdev_priv(dev);

	u32 addr;
	u32 data32;
	u32 bytes;


	bytes = 0;
	addr = 0;
	data32 = 0;
	sscanf(extra, "%d,%x,%x", &bytes, &addr, &data32);

	switch (bytes) {
	case 1:
		rtw_write8(adapt, addr, (u8)data32);
		RTW_INFO("%s: addr=0x%08X data=0x%02X\n", __func__, addr, (u8)data32);
		break;
	case 2:
		rtw_write16(adapt, addr, (u16)data32);
		RTW_INFO("%s: addr=0x%08X data=0x%04X\n", __func__, addr, (u16)data32);
		break;
	case 4:
		rtw_write32(adapt, addr, data32);
		RTW_INFO("%s: addr=0x%08X data=0x%08X\n", __func__, addr, data32);
		break;
	default:
		RTW_INFO("%s: usage> write [bytes],[address(hex)],[data(hex)]\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int rtw_wx_read_rf(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u32 path, addr, data32;


	path = *(u32 *)extra;
	addr = *((u32 *)extra + 1);
	data32 = rtw_hal_read_rfreg(adapt, path, addr, 0xFFFFF);
	/*	RTW_INFO("%s: path=%d addr=0x%02x data=0x%05x\n", __func__, path, addr, data32); */
	/*
	 * IMPORTANT!!
	 * Only when wireless private ioctl is at odd order,
	 * "extra" would be copied to user space.
	 */
	sprintf(extra, "0x%05x", data32);

	return 0;
}

static int rtw_wx_write_rf(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u32 path, addr, data32;


	path = *(u32 *)extra;
	addr = *((u32 *)extra + 1);
	data32 = *((u32 *)extra + 2);
	/*	RTW_INFO("%s: path=%d addr=0x%02x data=0x%05x\n", __func__, path, addr, data32); */
	rtw_hal_write_rfreg(adapt, path, addr, 0xFFFFF, data32);

	return 0;
}

static int rtw_wx_priv_null(struct net_device *dev, struct iw_request_info *a,
			    union iwreq_data *wrqu, char *b)
{
	return -1;
}

#ifdef CONFIG_WIRELESS_EXT
static int dummy(struct net_device *dev, struct iw_request_info *a,
		 union iwreq_data *wrqu, char *b)
{
	/* struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);	 */
	/* struct mlme_priv *pmlmepriv = &(adapt->mlmepriv); */

	/* RTW_INFO("cmd_code=%x, fwstate=0x%x\n", a->cmd, get_fwstate(pmlmepriv)); */

	return -1;

}
#endif

static int rtw_wx_set_channel_plan(struct net_device *dev,
				   struct iw_request_info *info,
				   union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 channel_plan_req = (u8)(*((int *)wrqu));

	if (_SUCCESS != rtw_set_channel_plan(adapt, channel_plan_req))
		return -EPERM;

	return 0;
}

static int rtw_wx_set_mtk_wps_probe_ie(struct net_device *dev,
				       struct iw_request_info *a,
				       union iwreq_data *wrqu, char *b)
{
#ifdef CONFIG_PLATFORM_MT53XX
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;

#endif
	return 0;
}

static int rtw_wx_get_sensitivity(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *buf)
{
#ifdef CONFIG_PLATFORM_MT53XX
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	/*	Modified by Albert 20110914 */
	/*	This is in dbm format for MTK platform. */
	wrqu->qual.level = adapt->recvpriv.rssi;
	RTW_INFO(" level = %u\n",  wrqu->qual.level);
#endif
	return 0;
}

static int rtw_wx_set_mtk_wps_ie(struct net_device *dev,
				 struct iw_request_info *info,
				 union iwreq_data *wrqu, char *extra)
{
#ifdef CONFIG_PLATFORM_MT53XX
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	return rtw_set_wpa_ie(adapt, wrqu->data.pointer, wrqu->data.length);
#else
	return 0;
#endif
}

#ifdef MP_IOCTL_HDL
static int rtw_mp_ioctl_hdl(struct net_device *dev, struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	u32 BytesRead, BytesWritten, BytesNeeded;
	struct oid_par_priv	oid_par;
	struct mp_ioctl_handler	*phandler;
	struct mp_ioctl_param	*poidparam;
	uint status = 0;
	u16 len;
	u8 *pparmbuf = NULL, bset;
	struct adapter * adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_point *p = &wrqu->data;

	if ((!p->length) || (!p->pointer)) {
		ret = -EINVAL;
		goto _rtw_mp_ioctl_hdl_exit;
	}

	pparmbuf = NULL;
	bset = (u8)(p->flags & 0xFFFF);
	len = p->length;
	pparmbuf = (u8 *)rtw_malloc(len);
	if (!pparmbuf) {
		ret = -ENOMEM;
		goto _rtw_mp_ioctl_hdl_exit;
	}

	if (copy_from_user(pparmbuf, p->pointer, len)) {
		ret = -EFAULT;
		goto _rtw_mp_ioctl_hdl_exit;
	}

	poidparam = (struct mp_ioctl_param *)pparmbuf;

	if (poidparam->subcode >= MAX_MP_IOCTL_SUBCODE) {
		ret = -EINVAL;
		goto _rtw_mp_ioctl_hdl_exit;
	}

	rtw_dbg_mode_hdl(adapt, poidparam->subcode, poidparam->data, poidparam->len);

	if (bset == 0x00) {/* query info */
		if (copy_to_user(p->pointer, pparmbuf, len))
			ret = -EFAULT;
	}

	if (status) {
		ret = -EFAULT;
		goto _rtw_mp_ioctl_hdl_exit;
	}

_rtw_mp_ioctl_hdl_exit:

	if (pparmbuf)
		rtw_mfree(pparmbuf, len);

	return ret;
}
#endif
static int rtw_get_ap_info(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	u32 cnt = 0, wpa_ielen;
	unsigned long	irqL;
	struct list_head	*plist, *phead;
	unsigned char *pbuf;
	u8 bssid[ETH_ALEN];
	char data[32];
	struct wlan_network *pnetwork = NULL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct __queue *queue = &(pmlmepriv->scanned_queue);
	struct iw_point *pdata = &wrqu->data;

	RTW_INFO("+rtw_get_aplist_info\n");

	if (rtw_is_drv_stopped(adapt) || (!pdata)) {
		ret = -EINVAL;
		goto exit;
	}

	while ((check_fwstate(pmlmepriv, (_FW_UNDER_SURVEY | _FW_UNDER_LINKING)))) {
		rtw_msleep_os(30);
		cnt++;
		if (cnt > 100)
			break;
	}


	/* pdata->length = 0; */ /* ?	 */
	pdata->flags = 0;
	if (pdata->length >= 32) {
		if (copy_from_user(data, pdata->pointer, 32)) {
			ret = -EINVAL;
			goto exit;
		}
	} else {
		ret = -EINVAL;
		goto exit;
	}

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;


		pnetwork = container_of(plist, struct wlan_network, list);

		/* if(hwaddr_aton_i(pdata->pointer, bssid)) */
		if (hwaddr_aton_i(data, bssid)) {
			RTW_INFO("Invalid BSSID '%s'.\n", (u8 *)data);
			_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
			return -EINVAL;
		}


		if (!memcmp(bssid, pnetwork->network.MacAddress, ETH_ALEN)) { /* BSSID match, then check if supporting wpa/wpa2 */
			RTW_INFO("BSSID:" MAC_FMT "\n", MAC_ARG(bssid));

			pbuf = rtw_get_wpa_ie(&pnetwork->network.IEs[12], &wpa_ielen, pnetwork->network.IELength - 12);
			if (pbuf && (wpa_ielen > 0)) {
				pdata->flags = 1;
				break;
			}

			pbuf = rtw_get_wpa2_ie(&pnetwork->network.IEs[12], &wpa_ielen, pnetwork->network.IELength - 12);
			if (pbuf && (wpa_ielen > 0)) {
				pdata->flags = 2;
				break;
			}

		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (pdata->length >= 34) {
		if (copy_to_user((u8 *)pdata->pointer + 32, (u8 *)&pdata->flags, 1)) {
			ret = -EINVAL;
			goto exit;
		}
	}

exit:

	return ret;

}

static int rtw_set_pid(struct net_device *dev,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = rtw_netdev_priv(dev);
	int *pdata = (int *)wrqu;
	int selector;

	if (rtw_is_drv_stopped(adapt) || (!pdata)) {
		ret = -EINVAL;
		goto exit;
	}

	selector = *pdata;
	if (selector < 3 && selector >= 0) {
		adapt->pid[selector] = *(pdata + 1);
		ui_pid[selector] = *(pdata + 1);
		RTW_INFO("%s set pid[%d]=%d\n", __func__, selector , adapt->pid[selector]);
	} else
		RTW_INFO("%s selector %d error\n", __func__, selector);

exit:

	return ret;
}

static int rtw_wps_start(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_point *pdata = &wrqu->data;
	u32   u32wps_start = 0;
	unsigned int uintRet = 0;

	if (RTW_CANNOT_RUN(adapt) || (!pdata)) {
		ret = -EINVAL;
		goto exit;
	}

	uintRet = copy_from_user((void *) &u32wps_start, pdata->pointer, 4);
	if (u32wps_start == 0)
		u32wps_start = *extra;

	RTW_INFO("[%s] wps_start = %d\n", __func__, u32wps_start);

	if (u32wps_start == 1)   /* WPS Start */
		rtw_led_control(adapt, LED_CTL_START_WPS);
	else if (u32wps_start == 2)   /* WPS Stop because of wps success */
		rtw_led_control(adapt, LED_CTL_STOP_WPS);
	else if (u32wps_start == 3)   /* WPS Stop because of wps fail */
		rtw_led_control(adapt, LED_CTL_STOP_WPS_FAIL);

#ifdef CONFIG_INTEL_WIDI
	process_intel_widi_wps_status(adapt, u32wps_start);
#endif /* CONFIG_INTEL_WIDI */

exit:

	return ret;

}

static int rtw_wext_p2p_enable(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	enum P2P_ROLE init_role = P2P_ROLE_DISABLE;

	if (*extra == '0')
		init_role = P2P_ROLE_DISABLE;
	else if (*extra == '1')
		init_role = P2P_ROLE_DEVICE;
	else if (*extra == '2')
		init_role = P2P_ROLE_CLIENT;
	else if (*extra == '3')
		init_role = P2P_ROLE_GO;

	if (_FAIL == rtw_p2p_enable(adapt, init_role)) {
		ret = -EFAULT;
		goto exit;
	}

	/* set channel/bandwidth */
	if (init_role != P2P_ROLE_DISABLE) {
		u8 channel, ch_offset;
		u16 bwmode;

		if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_LISTEN)) {
			/*	Stay at the listen state and wait for discovery. */
			channel = pwdinfo->listen_channel;
			pwdinfo->operating_channel = pwdinfo->listen_channel;
			ch_offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
			bwmode = CHANNEL_WIDTH_20;
		}
#ifdef CONFIG_CONCURRENT_MODE
		else if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_IDLE)) {

			_set_timer(&pwdinfo->ap_p2p_switch_timer, pwdinfo->ext_listen_interval);

			channel = rtw_mi_get_union_chan(adapt);
			ch_offset = rtw_mi_get_union_offset(adapt);
			bwmode = rtw_mi_get_union_bw(adapt);

			pwdinfo->operating_channel = channel;
		}
#endif
		else {
			pwdinfo->operating_channel = pmlmeext->cur_channel;

			channel = pwdinfo->operating_channel;
			ch_offset = pmlmeext->cur_ch_offset;
			bwmode = pmlmeext->cur_bwmode;
		}

		set_channel_bwmode(adapt, channel, ch_offset, bwmode);
	}

exit:
	return ret;

}

static int rtw_p2p_set_go_nego_ssid(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] ssid = %s, len = %zu\n", __func__, extra, strlen(extra));
	memcpy(pwdinfo->nego_ssid, extra, strlen(extra));
	pwdinfo->nego_ssidlen = strlen(extra);

	return ret;

}


static int rtw_p2p_set_intent(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int							ret = 0;
	struct adapter						*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info			*pwdinfo = &(adapt->wdinfo);
	u8							intent = pwdinfo->intent;

	extra[wrqu->data.length] = 0x00;

	intent = rtw_atoi(extra);

	if (intent <= 15)
		pwdinfo->intent = intent;
	else
		ret = -1;

	RTW_INFO("[%s] intent = %d\n", __func__, intent);

	return ret;

}

static int rtw_p2p_set_listen_ch(struct net_device *dev,
				 struct iw_request_info *info,
				 union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);
	u8	listen_ch = pwdinfo->listen_channel;	/*	Listen channel number */

	extra[wrqu->data.length] = 0x00;
	listen_ch = rtw_atoi(extra);

	if ((listen_ch == 1) || (listen_ch == 6) || (listen_ch == 11)) {
		pwdinfo->listen_channel = listen_ch;
		set_channel_bwmode(adapt, pwdinfo->listen_channel, HAL_PRIME_CHNL_OFFSET_DONT_CARE, CHANNEL_WIDTH_20);
	} else
		ret = -1;

	RTW_INFO("[%s] listen_ch = %d\n", __func__, pwdinfo->listen_channel);

	return ret;

}

static int rtw_p2p_set_op_ch(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	/*	Commented by Albert 20110524
	 *	This function is used to set the operating channel if the driver will become the group owner */

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);
	u8	op_ch = pwdinfo->operating_channel;	/*	Operating channel number */

	extra[wrqu->data.length] = 0x00;

	op_ch = (u8) rtw_atoi(extra);
	if (op_ch > 0)
		pwdinfo->operating_channel = op_ch;
	else
		ret = -1;

	RTW_INFO("[%s] op_ch = %d\n", __func__, pwdinfo->operating_channel);

	return ret;

}


static int rtw_p2p_profilefound(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);

	/*	Comment by Albert 2010/10/13 */
	/*	Input data format: */
	/*	Ex:  0 */
	/*	Ex:  1XX:XX:XX:XX:XX:XXYYSSID */
	/*	0 => Reflush the profile record list. */
	/*	1 => Add the profile list */
	/*	XX:XX:XX:XX:XX:XX => peer's MAC Address ( ex: 00:E0:4C:00:00:01 ) */
	/*	YY => SSID Length */
	/*	SSID => SSID for persistence group */

	RTW_INFO("[%s] In value = %s, len = %d\n", __func__, extra, wrqu->data.length - 1);


	/*	The upper application should pass the SSID to driver by using this rtw_p2p_profilefound function. */
	if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
		if (extra[0] == '0') {
			/*	Remove all the profile information of wifidirect_info structure. */
			memset(&pwdinfo->profileinfo[0], 0x00, sizeof(struct profile_info) * P2P_MAX_PERSISTENT_GROUP_NUM);
			pwdinfo->profileindex = 0;
		} else {
			if (pwdinfo->profileindex >= P2P_MAX_PERSISTENT_GROUP_NUM)
				ret = -1;
			else {
				int jj, kk;

				/*	Add this profile information into pwdinfo->profileinfo */
				/*	Ex:  1XX:XX:XX:XX:XX:XXYYSSID */
				for (jj = 0, kk = 1; jj < ETH_ALEN; jj++, kk += 3)
					pwdinfo->profileinfo[pwdinfo->profileindex].peermac[jj] = key_2char2num(extra[kk], extra[kk + 1]);

				/* pwdinfo->profileinfo[pwdinfo->profileindex].ssidlen = ( extra[18] - '0' ) * 10 + ( extra[19] - '0' ); */
				/* memcpy( pwdinfo->profileinfo[pwdinfo->profileindex].ssid, &extra[20], pwdinfo->profileinfo[pwdinfo->profileindex].ssidlen ); */
				pwdinfo->profileindex++;
			}
		}
	}

	return ret;

}

static int rtw_p2p_setDN(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);


	RTW_INFO("[%s] %s %d\n", __func__, extra, wrqu->data.length - 1);
	memset(pwdinfo->device_name, 0x00, WPS_MAX_DEVICE_NAME_LEN);
	memcpy(pwdinfo->device_name, extra, wrqu->data.length - 1);
	pwdinfo->device_name_len = wrqu->data.length - 1;

	return ret;

}

static int rtw_p2p_get_status(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	if (adapt->bShowGetP2PState) {
		RTW_INFO("[%s] Role = %d, Status = %d, peer addr = %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n", __func__, rtw_p2p_role(pwdinfo), rtw_p2p_state(pwdinfo),
			pwdinfo->p2p_peer_interface_addr[0], pwdinfo->p2p_peer_interface_addr[1], pwdinfo->p2p_peer_interface_addr[2],
			pwdinfo->p2p_peer_interface_addr[3], pwdinfo->p2p_peer_interface_addr[4], pwdinfo->p2p_peer_interface_addr[5]);
	}

	/*	Commented by Albert 2010/10/12 */
	/*	Because of the output size limitation, I had removed the "Role" information. */
	/*	About the "Role" information, we will use the new private IOCTL to get the "Role" information. */
	sprintf(extra, "\n\nStatus=%.2d\n", rtw_p2p_state(pwdinfo));
	wrqu->data.length = strlen(extra);

	return ret;

}

/*	Commented by Albert 20110520
 *	This function will return the config method description
 *	This config method description will show us which config method the remote P2P device is intented to use
 *	by sending the provisioning discovery request frame. */

static int rtw_p2p_get_req_cm(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	sprintf(extra, "\n\nCM=%s\n", pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req);
	wrqu->data.length = strlen(extra);
	return ret;
}

static int rtw_p2p_get_role(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] Role = %d, Status = %d, peer addr = %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n", __func__, rtw_p2p_role(pwdinfo), rtw_p2p_state(pwdinfo),
		pwdinfo->p2p_peer_interface_addr[0], pwdinfo->p2p_peer_interface_addr[1], pwdinfo->p2p_peer_interface_addr[2],
		pwdinfo->p2p_peer_interface_addr[3], pwdinfo->p2p_peer_interface_addr[4], pwdinfo->p2p_peer_interface_addr[5]);

	sprintf(extra, "\n\nRole=%.2d\n", rtw_p2p_role(pwdinfo));
	wrqu->data.length = strlen(extra);
	return ret;
}

static int rtw_p2p_get_peer_ifaddr(struct net_device *dev,
				   struct iw_request_info *info,
				   union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] Role = %d, Status = %d, peer addr = %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n", __func__, rtw_p2p_role(pwdinfo), rtw_p2p_state(pwdinfo),
		pwdinfo->p2p_peer_interface_addr[0], pwdinfo->p2p_peer_interface_addr[1], pwdinfo->p2p_peer_interface_addr[2],
		pwdinfo->p2p_peer_interface_addr[3], pwdinfo->p2p_peer_interface_addr[4], pwdinfo->p2p_peer_interface_addr[5]);

	sprintf(extra, "\nMAC %.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
		pwdinfo->p2p_peer_interface_addr[0], pwdinfo->p2p_peer_interface_addr[1], pwdinfo->p2p_peer_interface_addr[2],
		pwdinfo->p2p_peer_interface_addr[3], pwdinfo->p2p_peer_interface_addr[4], pwdinfo->p2p_peer_interface_addr[5]);
	wrqu->data.length = strlen(extra);
	return ret;
}

static int rtw_p2p_get_peer_devaddr(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] Role = %d, Status = %d, peer addr = %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n", __func__, rtw_p2p_role(pwdinfo), rtw_p2p_state(pwdinfo),
		pwdinfo->rx_prov_disc_info.peerDevAddr[0], pwdinfo->rx_prov_disc_info.peerDevAddr[1],
		pwdinfo->rx_prov_disc_info.peerDevAddr[2], pwdinfo->rx_prov_disc_info.peerDevAddr[3],
		pwdinfo->rx_prov_disc_info.peerDevAddr[4], pwdinfo->rx_prov_disc_info.peerDevAddr[5]);
	sprintf(extra, "\n%.2X%.2X%.2X%.2X%.2X%.2X",
		pwdinfo->rx_prov_disc_info.peerDevAddr[0], pwdinfo->rx_prov_disc_info.peerDevAddr[1],
		pwdinfo->rx_prov_disc_info.peerDevAddr[2], pwdinfo->rx_prov_disc_info.peerDevAddr[3],
		pwdinfo->rx_prov_disc_info.peerDevAddr[4], pwdinfo->rx_prov_disc_info.peerDevAddr[5]);
	wrqu->data.length = strlen(extra);
	return ret;

}

static int rtw_p2p_get_peer_devaddr_by_invitation(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra)

{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] Role = %d, Status = %d, peer addr = %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n", __func__, rtw_p2p_role(pwdinfo), rtw_p2p_state(pwdinfo),
		pwdinfo->p2p_peer_device_addr[0], pwdinfo->p2p_peer_device_addr[1],
		pwdinfo->p2p_peer_device_addr[2], pwdinfo->p2p_peer_device_addr[3],
		pwdinfo->p2p_peer_device_addr[4], pwdinfo->p2p_peer_device_addr[5]);
	sprintf(extra, "\nMAC %.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
		pwdinfo->p2p_peer_device_addr[0], pwdinfo->p2p_peer_device_addr[1],
		pwdinfo->p2p_peer_device_addr[2], pwdinfo->p2p_peer_device_addr[3],
		pwdinfo->p2p_peer_device_addr[4], pwdinfo->p2p_peer_device_addr[5]);
	wrqu->data.length = strlen(extra);
	return ret;
}

static int rtw_p2p_get_groupid(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	sprintf(extra, "\n%.2X:%.2X:%.2X:%.2X:%.2X:%.2X %s",
		pwdinfo->groupid_info.go_device_addr[0], pwdinfo->groupid_info.go_device_addr[1],
		pwdinfo->groupid_info.go_device_addr[2], pwdinfo->groupid_info.go_device_addr[3],
		pwdinfo->groupid_info.go_device_addr[4], pwdinfo->groupid_info.go_device_addr[5],
		pwdinfo->groupid_info.ssid);
	wrqu->data.length = strlen(extra);
	return ret;
}

static int rtw_p2p_get_op_ch(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)

{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);


	RTW_INFO("[%s] Op_ch = %02x\n", __func__, pwdinfo->operating_channel);

	sprintf(extra, "\n\nOp_ch=%.2d\n", pwdinfo->operating_channel);
	wrqu->data.length = strlen(extra);
	return ret;

}

static int rtw_p2p_get_wps_configmethod(struct net_device *dev,
					struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra, char *subcmd)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 peerMAC[ETH_ALEN] = { 0x00 };
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct __queue *queue = &(pmlmepriv->scanned_queue);
	struct wlan_network *pnetwork = NULL;
	u8 blnMatch = 0;
	u16	attr_content = 0;
	uint attr_contentlen = 0;
	u8	attr_content_str[P2P_PRIVATE_IOCTL_SET_LEN] = { 0x00 };

	/*	Commented by Albert 20110727 */
	/*	The input data is the MAC address which the application wants to know its WPS config method. */
	/*	After knowing its WPS config method, the application can decide the config method for provisioning discovery. */
	/*	Format: iwpriv wlanx p2p_get_wpsCM 00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, subcmd);

	macstr2num(peerMAC, subcmd);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
			u8 *wpsie;
			uint	wpsie_len = 0;
			__be16 be_attr_content;

			/*	The mac address is matched. */

			wpsie = rtw_get_wps_ie_from_scan_queue(&pnetwork->network.IEs[0], pnetwork->network.IELength, NULL, &wpsie_len, pnetwork->network.Reserved[0]);
			if (wpsie) {
				rtw_get_wps_attr_content(wpsie, wpsie_len, WPS_ATTR_CONF_METHOD, (u8 *)&be_attr_content, &attr_contentlen);
				if (attr_contentlen) {
					attr_content = be16_to_cpu(be_attr_content);
					sprintf(attr_content_str, "\n\nM=%.4d", attr_content);
					blnMatch = 1;
				}
			}

			break;
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (!blnMatch)
		sprintf(attr_content_str, "\n\nM=0000");

	wrqu->data.length = strlen(attr_content_str);
	memcpy(extra, attr_content_str, wrqu->data.length);

	return ret;

}

static int rtw_p2p_get_peer_wfd_port(struct net_device *dev,
				     struct iw_request_info *info,
				     union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] p2p_state = %d\n", __func__, rtw_p2p_state(pwdinfo));

	sprintf(extra, "\n\nPort=%d\n", pwdinfo->wfd_info->peer_rtsp_ctrlport);
	RTW_INFO("[%s] remote port = %d\n", __func__, pwdinfo->wfd_info->peer_rtsp_ctrlport);

	wrqu->data.length = strlen(extra);
	return ret;

}

static int rtw_p2p_get_peer_wfd_preferred_connection(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	sprintf(extra, "\n\nwfd_pc=%d\n", pwdinfo->wfd_info->wfd_pc);
	RTW_INFO("[%s] wfd_pc = %d\n", __func__, pwdinfo->wfd_info->wfd_pc);

	wrqu->data.length = strlen(extra);
	pwdinfo->wfd_info->wfd_pc = false;	/*	Reset the WFD preferred connection to P2P */
	return ret;

}

static int rtw_p2p_get_peer_wfd_session_available(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);

	sprintf(extra, "\n\nwfd_sa=%d\n", pwdinfo->wfd_info->peer_session_avail);
	RTW_INFO("[%s] wfd_sa = %d\n", __func__, pwdinfo->wfd_info->peer_session_avail);

	wrqu->data.length = strlen(extra);
	pwdinfo->wfd_info->peer_session_avail = true;	/*	Reset the WFD session available */
	return ret;

}

static int rtw_p2p_get_go_device_address(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra, char *subcmd)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 peerMAC[ETH_ALEN] = { 0x00 };
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct __queue *queue	= &(pmlmepriv->scanned_queue);
	struct wlan_network *pnetwork = NULL;
	u8 blnMatch = 0;
	u8 *p2pie;
	uint p2pielen = 0, attr_contentlen = 0;
	u8 attr_content[100] = { 0x00 };
	u8 go_devadd_str[P2P_PRIVATE_IOCTL_SET_LEN] = { 0x00 };

	/*	Commented by Albert 20121209 */
	/*	The input data is the GO's interface address which the application wants to know its device address. */
	/*	Format: iwpriv wlanx p2p_get2 go_devadd=00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, subcmd);

	macstr2num(peerMAC, subcmd);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
			/*	Commented by Albert 2011/05/18 */
			/*	Match the device address located in the P2P IE */
			/*	This is for the case that the P2P device address is not the same as the P2P interface address. */

			p2pie = rtw_bss_ex_get_p2p_ie(&pnetwork->network, NULL, &p2pielen);
			if (p2pie) {
				while (p2pie) {
					/*	The P2P Device ID attribute is included in the Beacon frame. */
					/*	The P2P Device Info attribute is included in the probe response frame. */

					memset(attr_content, 0x00, 100);
					if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_ID, attr_content, &attr_contentlen)) {
						/*	Handle the P2P Device ID attribute of Beacon first */
						blnMatch = 1;
						break;

					} else if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_INFO, attr_content, &attr_contentlen)) {
						/*	Handle the P2P Device Info attribute of probe response */
						blnMatch = 1;
						break;
					}

					/* Get the next P2P IE */
					p2pie = rtw_get_p2p_ie(p2pie + p2pielen, BSS_EX_TLV_IES_LEN(&pnetwork->network) - (p2pie + p2pielen - BSS_EX_TLV_IES(&pnetwork->network)), NULL, &p2pielen);
				}
			}
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (!blnMatch)
		sprintf(go_devadd_str, "\n\ndev_add=NULL");
	else {
		sprintf(go_devadd_str, "\n\ndev_add=%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
			attr_content[0], attr_content[1], attr_content[2], attr_content[3], attr_content[4], attr_content[5]);
	}

	wrqu->data.length = strlen(go_devadd_str);
	memcpy(extra, go_devadd_str, wrqu->data.length);

	return ret;

}

static int rtw_p2p_get_device_type(struct net_device *dev,
				   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra, char *subcmd)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 peerMAC[ETH_ALEN] = { 0x00 };
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct __queue *queue = &(pmlmepriv->scanned_queue);
	struct wlan_network *pnetwork = NULL;
	u8 blnMatch = 0;
	u8 dev_type[8] = { 0x00 };
	uint dev_type_len = 0;
	u8 dev_type_str[P2P_PRIVATE_IOCTL_SET_LEN] = { 0x00 };    /* +9 is for the str "dev_type=", we have to clear it at wrqu->data.pointer */

	/*	Commented by Albert 20121209 */
	/*	The input data is the MAC address which the application wants to know its device type. */
	/*	Such user interface could know the device type. */
	/*	Format: iwpriv wlanx p2p_get2 dev_type=00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, subcmd);

	macstr2num(peerMAC, subcmd);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
			u8 *wpsie;
			uint	wpsie_len = 0;

			/*	The mac address is matched. */

			wpsie = rtw_get_wps_ie_from_scan_queue(&pnetwork->network.IEs[0], pnetwork->network.IELength, NULL, &wpsie_len, pnetwork->network.Reserved[0]);
			if (wpsie) {
				rtw_get_wps_attr_content(wpsie, wpsie_len, WPS_ATTR_PRIMARY_DEV_TYPE, dev_type, &dev_type_len);
				if (dev_type_len) {
					__be16	be_type = 0;
					u16 type;

					memcpy(&be_type, dev_type, 2);
					type = be16_to_cpu(be_type);
					sprintf(dev_type_str, "\n\nN=%.2d", type);
					blnMatch = 1;
				}
			}
			break;
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (!blnMatch)
		sprintf(dev_type_str, "\n\nN=00");

	wrqu->data.length = strlen(dev_type_str);
	memcpy(extra, dev_type_str, wrqu->data.length);

	return ret;

}

static int rtw_p2p_get_device_name(struct net_device *dev,
				   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra, char *subcmd)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 peerMAC[ETH_ALEN] = { 0x00 };
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct __queue *queue = &(pmlmepriv->scanned_queue);
	struct wlan_network *pnetwork = NULL;
	u8 blnMatch = 0;
	u8 dev_name[WPS_MAX_DEVICE_NAME_LEN] = { 0x00 };
	uint dev_len = 0;
	u8 dev_name_str[P2P_PRIVATE_IOCTL_SET_LEN] = { 0x00 };

	/*	Commented by Albert 20121225 */
	/*	The input data is the MAC address which the application wants to know its device name. */
	/*	Such user interface could show peer device's device name instead of ssid. */
	/*	Format: iwpriv wlanx p2p_get2 devN=00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, subcmd);

	macstr2num(peerMAC, subcmd);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
			u8 *wpsie;
			uint	wpsie_len = 0;

			/*	The mac address is matched. */

			wpsie = rtw_get_wps_ie_from_scan_queue(&pnetwork->network.IEs[0], pnetwork->network.IELength, NULL, &wpsie_len, pnetwork->network.Reserved[0]);
			if (wpsie) {
				rtw_get_wps_attr_content(wpsie, wpsie_len, WPS_ATTR_DEVICE_NAME, dev_name, &dev_len);
				if (dev_len) {
					sprintf(dev_name_str, "\n\nN=%s", dev_name);
					blnMatch = 1;
				}
			}
			break;
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (!blnMatch)
		sprintf(dev_name_str, "\n\nN=0000");

	wrqu->data.length = strlen(dev_name_str);
	memcpy(extra, dev_name_str, wrqu->data.length);

	return ret;

}

static int rtw_p2p_get_invitation_procedure(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra, char *subcmd)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 peerMAC[ETH_ALEN] = { 0x00 };
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct __queue *queue	= &(pmlmepriv->scanned_queue);
	struct wlan_network *pnetwork = NULL;
	u8 blnMatch = 0;
	u8 *p2pie;
	uint p2pielen = 0, attr_contentlen = 0;
	u8 attr_content[2] = { 0x00 };
	u8 inv_proc_str[P2P_PRIVATE_IOCTL_SET_LEN] = { 0x00 };

	/*	Commented by Ouden 20121226 */
	/*	The application wants to know P2P initation procedure is support or not. */
	/*	Format: iwpriv wlanx p2p_get2 InvProc=00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, subcmd);

	macstr2num(peerMAC, subcmd);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
			/*	Commented by Albert 20121226 */
			/*	Match the device address located in the P2P IE */
			/*	This is for the case that the P2P device address is not the same as the P2P interface address. */

			p2pie = rtw_bss_ex_get_p2p_ie(&pnetwork->network, NULL, &p2pielen);
			if (p2pie) {
				while (p2pie) {
					/* memset( attr_content, 0x00, 2); */
					if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_CAPABILITY, attr_content, &attr_contentlen)) {
						/*	Handle the P2P capability attribute */
						blnMatch = 1;
						break;

					}

					/* Get the next P2P IE */
					p2pie = rtw_get_p2p_ie(p2pie + p2pielen, BSS_EX_TLV_IES_LEN(&pnetwork->network) - (p2pie + p2pielen - BSS_EX_TLV_IES(&pnetwork->network)), NULL, &p2pielen);
				}
			}
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (!blnMatch)
		sprintf(inv_proc_str, "\nIP=-1");
	else {
		if ((attr_content[0] & 0x20) == 0x20)
			sprintf(inv_proc_str, "\nIP=1");
		else
			sprintf(inv_proc_str, "\nIP=0");
	}

	wrqu->data.length = strlen(inv_proc_str);
	memcpy(extra, inv_proc_str, wrqu->data.length);

	return ret;

}

static int rtw_p2p_connect(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter				*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);
	u8					peerMAC[ETH_ALEN] = { 0x00 };
	int					jj, kk;
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	unsigned long				irqL;
	struct list_head					*plist, *phead;
	struct __queue				*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	uint					uintPeerChannel = 0;

	/*	Commented by Albert 20110304 */
	/*	The input data contains two informations. */
	/*	1. First information is the MAC address which wants to formate with */
	/*	2. Second information is the WPS PINCode or "pbc" string for push button method */
	/*	Format: 00:E0:4C:00:00:05 */
	/*	Format: 00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (pwdinfo->p2p_state == P2P_STATE_NONE) {
		RTW_INFO("[%s] WiFi Direct is disable!\n", __func__);
		return ret;
	}

#ifdef CONFIG_INTEL_WIDI
	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY)) {
		RTW_INFO("[%s] WiFi is under survey!\n", __func__);
		return ret;
	}
#endif /* CONFIG_INTEL_WIDI	 */

	if (pwdinfo->ui_got_wps_info == P2P_NO_WPSINFO)
		return -1;

	for (jj = 0, kk = 0; jj < ETH_ALEN; jj++, kk += 3)
		peerMAC[jj] = key_2char2num(extra[kk], extra[kk + 1]);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
			if (pnetwork->network.Configuration.DSConfig != 0)
				uintPeerChannel = pnetwork->network.Configuration.DSConfig;
			else if (pwdinfo->nego_req_info.peer_ch != 0)
				uintPeerChannel = pnetwork->network.Configuration.DSConfig = pwdinfo->nego_req_info.peer_ch;
			else {
				/* Unexpected case */
				uintPeerChannel = 0;
				RTW_INFO("%s  uintPeerChannel = 0\n", __func__);
			}
			break;
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (uintPeerChannel) {
#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED))
			_cancel_timer_ex(&pwdinfo->ap_p2p_switch_timer);
#endif /* CONFIG_CONCURRENT_MODE */

		memset(&pwdinfo->nego_req_info, 0x00, sizeof(struct tx_nego_req_info));
		memset(&pwdinfo->groupid_info, 0x00, sizeof(struct group_id_info));

		pwdinfo->nego_req_info.peer_channel_num[0] = uintPeerChannel;
		memcpy(pwdinfo->nego_req_info.peerDevAddr, pnetwork->network.MacAddress, ETH_ALEN);
		pwdinfo->nego_req_info.benable = true;

		_cancel_timer_ex(&pwdinfo->restore_p2p_state_timer);
		if (rtw_p2p_state(pwdinfo) != P2P_STATE_GONEGO_OK) {
			/*	Restore to the listen state if the current p2p state is not nego OK */
			rtw_p2p_set_state(pwdinfo, P2P_STATE_LISTEN);
		}

		rtw_p2p_set_pre_state(pwdinfo, rtw_p2p_state(pwdinfo));
		rtw_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_ING);

#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED)) {
			u8 union_ch = rtw_mi_get_union_chan(adapt);
			u8 union_bw = rtw_mi_get_union_bw(adapt);
			u8 union_offset = rtw_mi_get_union_offset(adapt);

			set_channel_bwmode(adapt, union_ch, union_offset, union_bw);
			rtw_leave_opch(adapt);
		}
#endif /* CONFIG_CONCURRENT_MODE */

		RTW_INFO("[%s] Start PreTx Procedure!\n", __func__);
		_set_timer(&pwdinfo->pre_tx_scan_timer, P2P_TX_PRESCAN_TIMEOUT);
#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED))
			_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_CONCURRENT_GO_NEGO_TIMEOUT);
		else
			_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_GO_NEGO_TIMEOUT);
#else
		_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_GO_NEGO_TIMEOUT);
#endif /* CONFIG_CONCURRENT_MODE		 */

	} else {
		RTW_INFO("[%s] Not Found in Scanning Queue~\n", __func__);
#ifdef CONFIG_INTEL_WIDI
		_cancel_timer_ex(&pwdinfo->restore_p2p_state_timer);
		rtw_p2p_set_state(pwdinfo, P2P_STATE_FIND_PHASE_SEARCH);
		rtw_p2p_findphase_ex_set(pwdinfo, P2P_FINDPHASE_EX_NONE);
		rtw_free_network_queue(adapt, true);
		/**
		 * For WiDi, if we can't find candidate device in scanning queue,
		 * driver will do scanning itself
		 */
		_enter_critical_bh(&pmlmepriv->lock, &irqL);
		rtw_sitesurvey_cmd(adapt, NULL);
		_exit_critical_bh(&pmlmepriv->lock, &irqL);
#endif /* CONFIG_INTEL_WIDI */
		ret = -1;
	}
	return ret;
}

static int rtw_p2p_invite_req(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter					*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info		*pwdinfo = &(adapt->wdinfo);
	int						jj, kk;
	struct mlme_priv			*pmlmepriv = &adapt->mlmepriv;
	struct list_head						*plist, *phead;
	struct __queue					*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network		*pnetwork = NULL;
	uint						uintPeerChannel = 0;
	u8						attr_content[50] = { 0x00 };
	u8						*p2pie;
	uint						p2pielen = 0, attr_contentlen = 0;
	unsigned long					irqL;
	struct tx_invite_req_info	*pinvite_req_info = &pwdinfo->invitereq_info;

	/*	Commented by Albert 20120321 */
	/*	The input data contains two informations. */
	/*	1. First information is the P2P device address which you want to send to.	 */
	/*	2. Second information is the group id which combines with GO's mac address, space and GO's ssid. */
	/*	Command line sample: iwpriv wlan0 p2p_set invite="00:11:22:33:44:55 00:E0:4C:00:00:05 DIRECT-xy" */
	/*	Format: 00:11:22:33:44:55 00:E0:4C:00:00:05 DIRECT-xy */

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (wrqu->data.length <=  37) {
		RTW_INFO("[%s] Wrong format!\n", __func__);
		return ret;
	}

	if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
		RTW_INFO("[%s] WiFi Direct is disable!\n", __func__);
		return ret;
	} else {
		/*	Reset the content of struct tx_invite_req_info */
		pinvite_req_info->benable = false;
		memset(pinvite_req_info->go_bssid, 0x00, ETH_ALEN);
		memset(pinvite_req_info->go_ssid, 0x00, WLAN_SSID_MAXLEN);
		pinvite_req_info->ssidlen = 0x00;
		pinvite_req_info->operating_ch = pwdinfo->operating_channel;
		memset(pinvite_req_info->peer_macaddr, 0x00, ETH_ALEN);
		pinvite_req_info->token = 3;
	}

	for (jj = 0, kk = 0; jj < ETH_ALEN; jj++, kk += 3)
		pinvite_req_info->peer_macaddr[jj] = key_2char2num(extra[kk], extra[kk + 1]);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);

		/*	Commented by Albert 2011/05/18 */
		/*	Match the device address located in the P2P IE */
		/*	This is for the case that the P2P device address is not the same as the P2P interface address. */

		p2pie = rtw_bss_ex_get_p2p_ie(&pnetwork->network, NULL, &p2pielen);
		if (p2pie) {
			/*	The P2P Device ID attribute is included in the Beacon frame. */
			/*	The P2P Device Info attribute is included in the probe response frame. */

			if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_ID, attr_content, &attr_contentlen)) {
				/*	Handle the P2P Device ID attribute of Beacon first */
				if (!memcmp(attr_content, pinvite_req_info->peer_macaddr, ETH_ALEN)) {
					uintPeerChannel = pnetwork->network.Configuration.DSConfig;
					break;
				}
			} else if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_INFO, attr_content, &attr_contentlen)) {
				/*	Handle the P2P Device Info attribute of probe response */
				if (!memcmp(attr_content, pinvite_req_info->peer_macaddr, ETH_ALEN)) {
					uintPeerChannel = pnetwork->network.Configuration.DSConfig;
					break;
				}
			}

		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (hal_chk_wl_func(adapt, WL_FUNC_MIRACAST) && uintPeerChannel) {
		struct wifi_display_info *pwfd_info = pwdinfo->wfd_info;
		u8 *wfd_ie;
		uint wfd_ielen = 0;

		wfd_ie = rtw_bss_ex_get_wfd_ie(&pnetwork->network, NULL, &wfd_ielen);
		if (wfd_ie) {
			u8 *wfd_devinfo;
			uint wfd_devlen;

			RTW_INFO("[%s] Found WFD IE!\n", __func__);
			wfd_devinfo = rtw_get_wfd_attr_content(wfd_ie, wfd_ielen, WFD_ATTR_DEVICE_INFO, NULL, &wfd_devlen);
			if (wfd_devinfo) {
				u16	wfd_devinfo_field = 0;

				/*	Commented by Albert 20120319 */
				/*	The first two bytes are the WFD device information field of WFD device information subelement. */
				/*	In big endian format. */
				wfd_devinfo_field = RTW_GET_BE16(wfd_devinfo);
				if (wfd_devinfo_field & WFD_DEVINFO_SESSION_AVAIL)
					pwfd_info->peer_session_avail = true;
				else
					pwfd_info->peer_session_avail = false;
			}
		}

		if (false == pwfd_info->peer_session_avail) {
			RTW_INFO("[%s] WFD Session not avaiable!\n", __func__);
			goto exit;
		}
	}

	if (uintPeerChannel) {
#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED))
			_cancel_timer_ex(&pwdinfo->ap_p2p_switch_timer);
#endif /* CONFIG_CONCURRENT_MODE */

		/*	Store the GO's bssid */
		for (jj = 0, kk = 18; jj < ETH_ALEN; jj++, kk += 3)
			pinvite_req_info->go_bssid[jj] = key_2char2num(extra[kk], extra[kk + 1]);

		/*	Store the GO's ssid */
		pinvite_req_info->ssidlen = wrqu->data.length - 36;
		memcpy(pinvite_req_info->go_ssid, &extra[36], (u32) pinvite_req_info->ssidlen);
		pinvite_req_info->benable = true;
		pinvite_req_info->peer_ch = uintPeerChannel;

		rtw_p2p_set_pre_state(pwdinfo, rtw_p2p_state(pwdinfo));
		rtw_p2p_set_state(pwdinfo, P2P_STATE_TX_INVITE_REQ);

#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED)) {
			u8 union_ch = rtw_mi_get_union_chan(adapt);
			u8 union_bw = rtw_mi_get_union_bw(adapt);
			u8 union_offset = rtw_mi_get_union_offset(adapt);

			set_channel_bwmode(adapt, union_ch, union_offset, union_bw);
			rtw_leave_opch(adapt);

		} else
			set_channel_bwmode(adapt, uintPeerChannel, HAL_PRIME_CHNL_OFFSET_DONT_CARE, CHANNEL_WIDTH_20);
#else
		set_channel_bwmode(adapt, uintPeerChannel, HAL_PRIME_CHNL_OFFSET_DONT_CARE, CHANNEL_WIDTH_20);
#endif/*CONFIG_CONCURRENT_MODE*/

		_set_timer(&pwdinfo->pre_tx_scan_timer, P2P_TX_PRESCAN_TIMEOUT);

#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED))
			_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_CONCURRENT_INVITE_TIMEOUT);
		else
			_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_INVITE_TIMEOUT);
#else
		_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_INVITE_TIMEOUT);
#endif /* CONFIG_CONCURRENT_MODE		 */


	} else
		RTW_INFO("[%s] NOT Found in the Scanning Queue!\n", __func__);
exit:

	return ret;

}

static int rtw_p2p_set_persistent(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter					*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info		*pwdinfo = &(adapt->wdinfo);

	/*	Commented by Albert 20120328 */
	/*	The input data is 0 or 1 */
	/*	0: disable persistent group functionality */
	/*	1: enable persistent group founctionality */

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
		RTW_INFO("[%s] WiFi Direct is disable!\n", __func__);
		return ret;
	} else {
		if (extra[0] == '0')	/*	Disable the persistent group function. */
			pwdinfo->persistent_supported = false;
		else if (extra[0] == '1')	/*	Enable the persistent group function. */
			pwdinfo->persistent_supported = true;
		else
			pwdinfo->persistent_supported = false;
	}
	printk("[%s] persistent_supported = %d\n", __func__, pwdinfo->persistent_supported);
	return ret;
}

static int uuid_str2bin(const char *str, u8 *bin)
{
	const char *pos;
	u8 *opos;

	pos = str;
	opos = bin;

	if (hexstr2bin(pos, opos, 4))
		return -1;
	pos += 8;
	opos += 4;

	if (*pos++ != '-' || hexstr2bin(pos, opos, 2))
		return -1;
	pos += 4;
	opos += 2;

	if (*pos++ != '-' || hexstr2bin(pos, opos, 2))
		return -1;
	pos += 4;
	opos += 2;

	if (*pos++ != '-' || hexstr2bin(pos, opos, 2))
		return -1;
	pos += 4;
	opos += 2;

	if (*pos++ != '-' || hexstr2bin(pos, opos, 6))
		return -1;

	return 0;
}

static int rtw_p2p_set_wps_uuid(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter				*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info			*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if ((36 == strlen(extra)) && (uuid_str2bin(extra, pwdinfo->uuid) == 0))
		pwdinfo->external_uuid = 1;
	else {
		pwdinfo->external_uuid = 0;
		ret = -EINVAL;
	}

	return ret;

}

static int rtw_p2p_set_pc(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter				*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);
	u8					peerMAC[ETH_ALEN] = { 0x00 };
	int					jj, kk;
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct list_head					*plist, *phead;
	struct __queue				*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	u8					attr_content[50] = { 0x00 };
	u8 *p2pie;
	uint					p2pielen = 0, attr_contentlen = 0;
	unsigned long				irqL;
	uint					uintPeerChannel = 0;

	struct wifi_display_info	*pwfd_info = pwdinfo->wfd_info;

	/*	Commented by Albert 20120512 */
	/*	1. Input information is the MAC address which wants to know the Preferred Connection bit (PC bit) */
	/*	Format: 00:E0:4C:00:00:05 */

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
		RTW_INFO("[%s] WiFi Direct is disable!\n", __func__);
		return ret;
	}

	for (jj = 0, kk = 0; jj < ETH_ALEN; jj++, kk += 3)
		peerMAC[jj] = key_2char2num(extra[kk], extra[kk + 1]);

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);

		/*	Commented by Albert 2011/05/18 */
		/*	Match the device address located in the P2P IE */
		/*	This is for the case that the P2P device address is not the same as the P2P interface address. */

		p2pie = rtw_bss_ex_get_p2p_ie(&pnetwork->network, NULL, &p2pielen);
		if (p2pie) {
			/*	The P2P Device ID attribute is included in the Beacon frame. */
			/*	The P2P Device Info attribute is included in the probe response frame. */
			printk("[%s] Got P2P IE\n", __func__);
			if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_ID, attr_content, &attr_contentlen)) {
				/*	Handle the P2P Device ID attribute of Beacon first */
				printk("[%s] P2P_ATTR_DEVICE_ID\n", __func__);
				if (!memcmp(attr_content, peerMAC, ETH_ALEN)) {
					uintPeerChannel = pnetwork->network.Configuration.DSConfig;
					break;
				}
			} else if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_INFO, attr_content, &attr_contentlen)) {
				/*	Handle the P2P Device Info attribute of probe response */
				printk("[%s] P2P_ATTR_DEVICE_INFO\n", __func__);
				if (!memcmp(attr_content, peerMAC, ETH_ALEN)) {
					uintPeerChannel = pnetwork->network.Configuration.DSConfig;
					break;
				}
			}

		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
	printk("[%s] channel = %d\n", __func__, uintPeerChannel);

	if (uintPeerChannel) {
		u8 *wfd_ie;
		uint wfd_ielen = 0;

		wfd_ie = rtw_bss_ex_get_wfd_ie(&pnetwork->network, NULL, &wfd_ielen);
		if (wfd_ie) {
			u8 *wfd_devinfo;
			uint wfd_devlen;

			RTW_INFO("[%s] Found WFD IE!\n", __func__);
			wfd_devinfo = rtw_get_wfd_attr_content(wfd_ie, wfd_ielen, WFD_ATTR_DEVICE_INFO, NULL, &wfd_devlen);
			if (wfd_devinfo) {
				u16	wfd_devinfo_field = 0;

				/*	Commented by Albert 20120319 */
				/*	The first two bytes are the WFD device information field of WFD device information subelement. */
				/*	In big endian format. */
				wfd_devinfo_field = RTW_GET_BE16(wfd_devinfo);
				if (wfd_devinfo_field & WFD_DEVINFO_PC_TDLS)
					pwfd_info->wfd_pc = true;
				else
					pwfd_info->wfd_pc = false;
			}
		}
	} else
		RTW_INFO("[%s] NOT Found in the Scanning Queue!\n", __func__);
	return ret;
}

static int rtw_p2p_set_wfd_device_type(struct net_device *dev,
				       struct iw_request_info *info,
				       union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter					*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info		*pwdinfo = &(adapt->wdinfo);
	struct wifi_display_info		*pwfd_info = pwdinfo->wfd_info;

	/*	Commented by Albert 20120328 */
	/*	The input data is 0 or 1 */
	/*	0: specify to Miracast source device */
	/*	1 or others: specify to Miracast sink device (display device) */

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (extra[0] == '0')	/*	Set to Miracast source device. */
		pwfd_info->wfd_device_type = WFD_DEVINFO_SOURCE;
	else					/*	Set to Miracast sink device. */
		pwfd_info->wfd_device_type = WFD_DEVINFO_PSINK;

	return ret;
}

static int rtw_p2p_set_wfd_enable(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	/*	Commented by Kurt 20121206
	 *	This function is used to set wfd enabled */

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);

	if (*extra == '0')
		rtw_wfd_enable(adapt, 0);
	else if (*extra == '1')
		rtw_wfd_enable(adapt, 1);

	RTW_INFO("[%s] wfd_enable = %d\n", __func__, pwdinfo->wfd_info->wfd_enable);

	return ret;

}

static int rtw_p2p_set_driver_iface(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	/*	Commented by Kurt 20121206
	 *	This function is used to set driver iface is WEXT or CFG80211 */
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info *pwdinfo = &(adapt->wdinfo);

	if (*extra == '1') {
		pwdinfo->driver_interface = DRIVER_WEXT;
		RTW_INFO("[%s] driver_interface = WEXT\n", __func__);
	} else if (*extra == '2') {
		pwdinfo->driver_interface = DRIVER_CFG80211;
		RTW_INFO("[%s] driver_interface = CFG80211\n", __func__);
	}

	return ret;

}

/*	To set the WFD session available to enable or disable */
static int rtw_p2p_set_sa(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter					*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info		*pwdinfo = &(adapt->wdinfo);

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (extra[0] == '0')	/*	Disable the session available. */
		pwdinfo->session_available = false;
	else if (extra[0] == '1')	/*	Enable the session available. */
		pwdinfo->session_available = true;
	else
		pwdinfo->session_available = false;
	printk("[%s] session available = %d\n", __func__, pwdinfo->session_available);

	return ret;
}

static int rtw_p2p_prov_disc(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter				*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);
	u8					peerMAC[ETH_ALEN] = { 0x00 };
	int					jj, kk;
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct list_head					*plist, *phead;
	struct __queue				*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	uint					uintPeerChannel = 0;
	u8					attr_content[100] = { 0x00 };
	u8 *p2pie;
	uint					p2pielen = 0, attr_contentlen = 0;
	unsigned long				irqL;

	/*	Commented by Albert 20110301 */
	/*	The input data contains two informations. */
	/*	1. First information is the MAC address which wants to issue the provisioning discovery request frame. */
	/*	2. Second information is the WPS configuration method which wants to discovery */
	/*	Format: 00:E0:4C:00:00:05_display */
	/*	Format: 00:E0:4C:00:00:05_keypad */
	/*	Format: 00:E0:4C:00:00:05_pbc */
	/*	Format: 00:E0:4C:00:00:05_label */

	RTW_INFO("[%s] data = %s\n", __func__, extra);

	if (pwdinfo->p2p_state == P2P_STATE_NONE) {
		RTW_INFO("[%s] WiFi Direct is disable!\n", __func__);
		return ret;
	} else {
#ifdef CONFIG_INTEL_WIDI
		if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY)) {
			RTW_INFO("[%s] WiFi is under survey!\n", __func__);
			return ret;
		}
#endif /* CONFIG_INTEL_WIDI */

		/*	Reset the content of struct tx_provdisc_req_info excluded the wps_config_method_request. */
		memset(pwdinfo->tx_prov_disc_info.peerDevAddr, 0x00, ETH_ALEN);
		memset(pwdinfo->tx_prov_disc_info.peerIFAddr, 0x00, ETH_ALEN);
		memset(&pwdinfo->tx_prov_disc_info.ssid, 0x00, sizeof(struct ndis_802_11_ssid));
		pwdinfo->tx_prov_disc_info.peer_channel_num[0] = 0;
		pwdinfo->tx_prov_disc_info.peer_channel_num[1] = 0;
		pwdinfo->tx_prov_disc_info.benable = false;
	}

	for (jj = 0, kk = 0; jj < ETH_ALEN; jj++, kk += 3)
		peerMAC[jj] = key_2char2num(extra[kk], extra[kk + 1]);

	if (!memcmp(&extra[18], "display", 7))
		pwdinfo->tx_prov_disc_info.wps_config_method_request = WPS_CM_DISPLYA;
	else if (!memcmp(&extra[18], "keypad", 7))
		pwdinfo->tx_prov_disc_info.wps_config_method_request = WPS_CM_KEYPAD;
	else if (!memcmp(&extra[18], "pbc", 3))
		pwdinfo->tx_prov_disc_info.wps_config_method_request = WPS_CM_PUSH_BUTTON;
	else if (!memcmp(&extra[18], "label", 5))
		pwdinfo->tx_prov_disc_info.wps_config_method_request = WPS_CM_LABEL;
	else {
		RTW_INFO("[%s] Unknown WPS config methodn", __func__);
		return ret ;
	}

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		if (uintPeerChannel != 0)
			break;

		pnetwork = container_of(plist, struct wlan_network, list);

		/*	Commented by Albert 2011/05/18 */
		/*	Match the device address located in the P2P IE */
		/*	This is for the case that the P2P device address is not the same as the P2P interface address. */

		p2pie = rtw_bss_ex_get_p2p_ie(&pnetwork->network, NULL, &p2pielen);
		if (p2pie) {
			while (p2pie) {
				/*	The P2P Device ID attribute is included in the Beacon frame. */
				/*	The P2P Device Info attribute is included in the probe response frame. */

				if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_ID, attr_content, &attr_contentlen)) {
					/*	Handle the P2P Device ID attribute of Beacon first */
					if (!memcmp(attr_content, peerMAC, ETH_ALEN)) {
						uintPeerChannel = pnetwork->network.Configuration.DSConfig;
						break;
					}
				} else if (rtw_get_p2p_attr_content(p2pie, p2pielen, P2P_ATTR_DEVICE_INFO, attr_content, &attr_contentlen)) {
					/*	Handle the P2P Device Info attribute of probe response */
					if (!memcmp(attr_content, peerMAC, ETH_ALEN)) {
						uintPeerChannel = pnetwork->network.Configuration.DSConfig;
						break;
					}
				}

				/* Get the next P2P IE */
				p2pie = rtw_get_p2p_ie(p2pie + p2pielen, BSS_EX_TLV_IES_LEN(&pnetwork->network) - (p2pie + p2pielen - BSS_EX_TLV_IES(&pnetwork->network)), NULL, &p2pielen);
			}

		}

#ifdef CONFIG_INTEL_WIDI
		/* Some Intel WiDi source may not provide P2P IE, */
		/* so we could only compare mac addr by 802.11 Source Address */
		if (pmlmepriv->widi_state == INTEL_WIDI_STATE_WFD_CONNECTION
		    && uintPeerChannel == 0) {
			if (!memcmp(pnetwork->network.MacAddress, peerMAC, ETH_ALEN)) {
				uintPeerChannel = pnetwork->network.Configuration.DSConfig;
				break;
			}
		}
#endif /* CONFIG_INTEL_WIDI */

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	if (uintPeerChannel) {
		if (hal_chk_wl_func(adapt, WL_FUNC_MIRACAST)) {
			struct wifi_display_info *pwfd_info = pwdinfo->wfd_info;
			u8 *wfd_ie;
			uint wfd_ielen = 0;

			wfd_ie = rtw_bss_ex_get_wfd_ie(&pnetwork->network, NULL, &wfd_ielen);
			if (wfd_ie) {
				u8 *wfd_devinfo;
				uint wfd_devlen;

				RTW_INFO("[%s] Found WFD IE!\n", __func__);
				wfd_devinfo = rtw_get_wfd_attr_content(wfd_ie, wfd_ielen, WFD_ATTR_DEVICE_INFO, NULL, &wfd_devlen);
				if (wfd_devinfo) {
					u16	wfd_devinfo_field = 0;

					/*	Commented by Albert 20120319 */
					/*	The first two bytes are the WFD device information field of WFD device information subelement. */
					/*	In big endian format. */
					wfd_devinfo_field = RTW_GET_BE16(wfd_devinfo);
					if (wfd_devinfo_field & WFD_DEVINFO_SESSION_AVAIL)
						pwfd_info->peer_session_avail = true;
					else
						pwfd_info->peer_session_avail = false;
				}
			}

			if (false == pwfd_info->peer_session_avail) {
				RTW_INFO("[%s] WFD Session not avaiable!\n", __func__);
				goto exit;
			}
		}

		RTW_INFO("[%s] peer channel: %d!\n", __func__, uintPeerChannel);
#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED))
			_cancel_timer_ex(&pwdinfo->ap_p2p_switch_timer);
#endif /* CONFIG_CONCURRENT_MODE */
		memcpy(pwdinfo->tx_prov_disc_info.peerIFAddr, pnetwork->network.MacAddress, ETH_ALEN);
		memcpy(pwdinfo->tx_prov_disc_info.peerDevAddr, peerMAC, ETH_ALEN);
		pwdinfo->tx_prov_disc_info.peer_channel_num[0] = (u16) uintPeerChannel;
		pwdinfo->tx_prov_disc_info.benable = true;
		rtw_p2p_set_pre_state(pwdinfo, rtw_p2p_state(pwdinfo));
		rtw_p2p_set_state(pwdinfo, P2P_STATE_TX_PROVISION_DIS_REQ);

		if (rtw_p2p_chk_role(pwdinfo, P2P_ROLE_CLIENT))
			memcpy(&pwdinfo->tx_prov_disc_info.ssid, &pnetwork->network.Ssid, sizeof(struct ndis_802_11_ssid));
		else if (rtw_p2p_chk_role(pwdinfo, P2P_ROLE_DEVICE) || rtw_p2p_chk_role(pwdinfo, P2P_ROLE_GO)) {
			memcpy(pwdinfo->tx_prov_disc_info.ssid.Ssid, pwdinfo->p2p_wildcard_ssid, P2P_WILDCARD_SSID_LEN);
			pwdinfo->tx_prov_disc_info.ssid.SsidLength = P2P_WILDCARD_SSID_LEN;
		}

#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED)) {
			u8 union_ch = rtw_mi_get_union_chan(adapt);
			u8 union_bw = rtw_mi_get_union_bw(adapt);
			u8 union_offset = rtw_mi_get_union_offset(adapt);

			set_channel_bwmode(adapt, union_ch, union_offset, union_bw);
			rtw_leave_opch(adapt);

		} else
			set_channel_bwmode(adapt, uintPeerChannel, HAL_PRIME_CHNL_OFFSET_DONT_CARE, CHANNEL_WIDTH_20);
#else
		set_channel_bwmode(adapt, uintPeerChannel, HAL_PRIME_CHNL_OFFSET_DONT_CARE, CHANNEL_WIDTH_20);
#endif

		_set_timer(&pwdinfo->pre_tx_scan_timer, P2P_TX_PRESCAN_TIMEOUT);

#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_check_status(adapt, MI_LINKED))
			_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_CONCURRENT_PROVISION_TIMEOUT);
		else
			_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_PROVISION_TIMEOUT);
#else
		_set_timer(&pwdinfo->restore_p2p_state_timer, P2P_PROVISION_TIMEOUT);
#endif /* CONFIG_CONCURRENT_MODE		 */

	} else {
		RTW_INFO("[%s] NOT Found in the Scanning Queue!\n", __func__);
#ifdef CONFIG_INTEL_WIDI
		_cancel_timer_ex(&pwdinfo->restore_p2p_state_timer);
		rtw_p2p_set_state(pwdinfo, P2P_STATE_FIND_PHASE_SEARCH);
		rtw_p2p_findphase_ex_set(pwdinfo, P2P_FINDPHASE_EX_NONE);
		rtw_free_network_queue(adapt, true);
		_enter_critical_bh(&pmlmepriv->lock, &irqL);
		rtw_sitesurvey_cmd(adapt, NULL);
		_exit_critical_bh(&pmlmepriv->lock, &irqL);
#endif /* CONFIG_INTEL_WIDI */
	}
exit:

	return ret;

}

/*	Added by Albert 20110328
 *	This function is used to inform the driver the user had specified the pin code value or pbc
 *	to application. */

static int rtw_p2p_got_wpsinfo(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter				*adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);


	RTW_INFO("[%s] data = %s\n", __func__, extra);
	/*	Added by Albert 20110328 */
	/*	if the input data is P2P_NO_WPSINFO -> reset the wpsinfo */
	/*	if the input data is P2P_GOT_WPSINFO_PEER_DISPLAY_PIN -> the utility just input the PIN code got from the peer P2P device. */
	/*	if the input data is P2P_GOT_WPSINFO_SELF_DISPLAY_PIN -> the utility just got the PIN code from itself. */
	/*	if the input data is P2P_GOT_WPSINFO_PBC -> the utility just determine to use the PBC */

	if (*extra == '0')
		pwdinfo->ui_got_wps_info = P2P_NO_WPSINFO;
	else if (*extra == '1')
		pwdinfo->ui_got_wps_info = P2P_GOT_WPSINFO_PEER_DISPLAY_PIN;
	else if (*extra == '2')
		pwdinfo->ui_got_wps_info = P2P_GOT_WPSINFO_SELF_DISPLAY_PIN;
	else if (*extra == '3')
		pwdinfo->ui_got_wps_info = P2P_GOT_WPSINFO_PBC;
	else
		pwdinfo->ui_got_wps_info = P2P_NO_WPSINFO;

	return ret;

}

static int rtw_p2p_set(struct net_device *dev,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_INFO("[%s] extra = %s\n", __func__, extra);

	if (!memcmp(extra, "enable=", 7))
		rtw_wext_p2p_enable(dev, info, wrqu, &extra[7]);
	else if (!memcmp(extra, "setDN=", 6)) {
		wrqu->data.length -= 6;
		rtw_p2p_setDN(dev, info, wrqu, &extra[6]);
	} else if (!memcmp(extra, "profilefound=", 13)) {
		wrqu->data.length -= 13;
		rtw_p2p_profilefound(dev, info, wrqu, &extra[13]);
	} else if (!memcmp(extra, "prov_disc=", 10)) {
		wrqu->data.length -= 10;
		rtw_p2p_prov_disc(dev, info, wrqu, &extra[10]);
	} else if (!memcmp(extra, "nego=", 5)) {
		wrqu->data.length -= 5;
		rtw_p2p_connect(dev, info, wrqu, &extra[5]);
	} else if (!memcmp(extra, "intent=", 7)) {
		/*	Commented by Albert 2011/03/23 */
		/*	The wrqu->data.length will include the null character */
		/*	So, we will decrease 7 + 1 */
		wrqu->data.length -= 8;
		rtw_p2p_set_intent(dev, info, wrqu, &extra[7]);
	} else if (!memcmp(extra, "ssid=", 5)) {
		wrqu->data.length -= 5;
		rtw_p2p_set_go_nego_ssid(dev, info, wrqu, &extra[5]);
	} else if (!memcmp(extra, "got_wpsinfo=", 12)) {
		wrqu->data.length -= 12;
		rtw_p2p_got_wpsinfo(dev, info, wrqu, &extra[12]);
	} else if (!memcmp(extra, "listen_ch=", 10)) {
		/*	Commented by Albert 2011/05/24 */
		/*	The wrqu->data.length will include the null character */
		/*	So, we will decrease (10 + 1)	 */
		wrqu->data.length -= 11;
		rtw_p2p_set_listen_ch(dev, info, wrqu, &extra[10]);
	} else if (!memcmp(extra, "op_ch=", 6)) {
		/*	Commented by Albert 2011/05/24 */
		/*	The wrqu->data.length will include the null character */
		/*	So, we will decrease (6 + 1)	 */
		wrqu->data.length -= 7;
		rtw_p2p_set_op_ch(dev, info, wrqu, &extra[6]);
	} else if (!memcmp(extra, "invite=", 7)) {
		wrqu->data.length -= 8;
		rtw_p2p_invite_req(dev, info, wrqu, &extra[7]);
	} else if (!memcmp(extra, "persistent=", 11)) {
		wrqu->data.length -= 11;
		rtw_p2p_set_persistent(dev, info, wrqu, &extra[11]);
	} else if (!memcmp(extra, "uuid=", 5)) {
		wrqu->data.length -= 5;
		ret = rtw_p2p_set_wps_uuid(dev, info, wrqu, &extra[5]);
	}

	if (hal_chk_wl_func(adapt, WL_FUNC_MIRACAST)) {
		if (!memcmp(extra, "sa=", 3)) {
			/* sa: WFD Session Available information */
			wrqu->data.length -= 3;
			rtw_p2p_set_sa(dev, info, wrqu, &extra[3]);
		} else if (!memcmp(extra, "pc=", 3)) {
			/* pc: WFD Preferred Connection */
			wrqu->data.length -= 3;
			rtw_p2p_set_pc(dev, info, wrqu, &extra[3]);
		} else if (!memcmp(extra, "wfd_type=", 9)) {
			wrqu->data.length -= 9;
			rtw_p2p_set_wfd_device_type(dev, info, wrqu, &extra[9]);
		} else if (!memcmp(extra, "wfd_enable=", 11)) {
			wrqu->data.length -= 11;
			rtw_p2p_set_wfd_enable(dev, info, wrqu, &extra[11]);
		} else if (!memcmp(extra, "driver_iface=", 13)) {
			wrqu->data.length -= 13;
			rtw_p2p_set_driver_iface(dev, info, wrqu, &extra[13]);
		}
	}
	return ret;
}

static int rtw_p2p_get(struct net_device *dev,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	int ret = 0;

	if (adapt->bShowGetP2PState)
		RTW_INFO("[%s] extra = %s\n", __func__, (char *) wrqu->data.pointer);

	if (!memcmp(wrqu->data.pointer, "status", 6))
		rtw_p2p_get_status(dev, info, wrqu, extra);
	else if (!memcmp(wrqu->data.pointer, "role", 4))
		rtw_p2p_get_role(dev, info, wrqu, extra);
	else if (!memcmp(wrqu->data.pointer, "peer_ifa", 8))
		rtw_p2p_get_peer_ifaddr(dev, info, wrqu, extra);
	else if (!memcmp(wrqu->data.pointer, "req_cm", 6))
		rtw_p2p_get_req_cm(dev, info, wrqu, extra);
	else if (!memcmp(wrqu->data.pointer, "peer_deva", 9)) {
		/*	Get the P2P device address when receiving the provision discovery request frame. */
		rtw_p2p_get_peer_devaddr(dev, info, wrqu, extra);
	} else if (!memcmp(wrqu->data.pointer, "group_id", 8))
		rtw_p2p_get_groupid(dev, info, wrqu, extra);
	else if (!memcmp(wrqu->data.pointer, "inv_peer_deva", 13)) {
		/*	Get the P2P device address when receiving the P2P Invitation request frame. */
		rtw_p2p_get_peer_devaddr_by_invitation(dev, info, wrqu, extra);
	} else if (!memcmp(wrqu->data.pointer, "op_ch", 5))
		rtw_p2p_get_op_ch(dev, info, wrqu, extra);

	if (hal_chk_wl_func(adapt, WL_FUNC_MIRACAST)) {
		if (!memcmp(wrqu->data.pointer, "peer_port", 9))
			rtw_p2p_get_peer_wfd_port(dev, info, wrqu, extra);
		else if (!memcmp(wrqu->data.pointer, "wfd_sa", 6))
			rtw_p2p_get_peer_wfd_session_available(dev, info, wrqu, extra);
		else if (!memcmp(wrqu->data.pointer, "wfd_pc", 6))
			rtw_p2p_get_peer_wfd_preferred_connection(dev, info, wrqu, extra);
	}

	return ret;
}

static int rtw_p2p_get2(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{

	int ret = 0;
	int length = wrqu->data.length;
	char *buffer = (u8 *)rtw_malloc(length);

	if (!buffer) {
		ret = -ENOMEM;
		goto bad;
	}

	if (copy_from_user(buffer, wrqu->data.pointer, wrqu->data.length)) {
		ret = -EFAULT;
		goto bad;
	}

	RTW_INFO("[%s] buffer = %s\n", __func__, buffer);

	if (!memcmp(buffer, "wpsCM=", 6))
		ret = rtw_p2p_get_wps_configmethod(dev, info, wrqu, extra, &buffer[6]);
	else if (!memcmp(buffer, "devN=", 5))
		ret = rtw_p2p_get_device_name(dev, info, wrqu, extra, &buffer[5]);
	else if (!memcmp(buffer, "dev_type=", 9))
		ret = rtw_p2p_get_device_type(dev, info, wrqu, extra, &buffer[9]);
	else if (!memcmp(buffer, "go_devadd=", 10))
		ret = rtw_p2p_get_go_device_address(dev, info, wrqu, extra, &buffer[10]);
	else if (!memcmp(buffer, "InvProc=", 8))
		ret = rtw_p2p_get_invitation_procedure(dev, info, wrqu, extra, &buffer[8]);
	else {
		snprintf(extra, sizeof("Command not found."), "Command not found.");
		wrqu->data.length = strlen(extra);
	}

bad:
	if (buffer)
		rtw_mfree(buffer, length);
	return ret;
}

static int rtw_rereg_nd_name(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct rereg_nd_name_data *rereg_priv = &adapt->rereg_nd_name_priv;
	char new_ifname[IFNAMSIZ];

	if (rereg_priv->old_ifname[0] == 0) {
		char *reg_ifname;
#ifdef CONFIG_CONCURRENT_MODE
		if (adapt->isprimary)
			reg_ifname = adapt->registrypriv.ifname;
		else
#endif
			reg_ifname = adapt->registrypriv.if2name;

		strncpy(rereg_priv->old_ifname, reg_ifname, IFNAMSIZ);
		rereg_priv->old_ifname[IFNAMSIZ - 1] = 0;
	}

	/* RTW_INFO("%s wrqu->data.length:%d\n", __func__, wrqu->data.length); */
	if (wrqu->data.length > IFNAMSIZ)
		return -EFAULT;

	if (copy_from_user(new_ifname, wrqu->data.pointer, IFNAMSIZ))
		return -EFAULT;

	if (0 == strcmp(rereg_priv->old_ifname, new_ifname))
		return ret;

	RTW_INFO("%s new_ifname:%s\n", __func__, new_ifname);
	rtw_set_rtnl_lock_holder(dvobj, current);
	ret = rtw_change_ifname(adapt, new_ifname);
	rtw_set_rtnl_lock_holder(dvobj, NULL);
	if (0 != ret)
		goto exit;

	if (!memcmp(rereg_priv->old_ifname, "disable%d", 9)) {
		adapt->ledpriv.bRegUseLed = rereg_priv->old_bRegUseLed;
		rtw_hal_sw_led_init(adapt);
	}

	strncpy(rereg_priv->old_ifname, new_ifname, IFNAMSIZ);
	rereg_priv->old_ifname[IFNAMSIZ - 1] = 0;

	if (!memcmp(new_ifname, "disable%d", 9)) {

		RTW_INFO("%s disable\n", __func__);
		/* free network queue for Android's timming issue */
		rtw_free_network_queue(adapt, true);

		/* close led */
		rtw_led_control(adapt, LED_CTL_POWER_OFF);
		rereg_priv->old_bRegUseLed = adapt->ledpriv.bRegUseLed;
		adapt->ledpriv.bRegUseLed = false;
		rtw_hal_sw_led_deinit(adapt);
	}
exit:
	return ret;

}

static int rtw_dbg_port(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	unsigned long irqL;
	int ret = 0;
	u8 major_cmd, minor_cmd;
	u16 arg;
	u32 extra_arg, *pdata, val32;
	struct sta_info *psta;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct wlan_network *cur_network = &(pmlmepriv->cur_network);
	struct sta_priv *pstapriv = &adapt->stapriv;


	pdata = (u32 *)&wrqu->data;

	val32 = *pdata;
	arg = (u16)(val32 & 0x0000ffff);
	major_cmd = (u8)(val32 >> 24);
	minor_cmd = (u8)((val32 >> 16) & 0x00ff);

	extra_arg = *(pdata + 1);

	switch (major_cmd) {
	case 0x70: /* read_reg */
		switch (minor_cmd) {
		case 1:
			RTW_INFO("rtw_read8(0x%x)=0x%02x\n", arg, rtw_read8(adapt, arg));
			break;
		case 2:
			RTW_INFO("rtw_read16(0x%x)=0x%04x\n", arg, rtw_read16(adapt, arg));
			break;
		case 4:
			RTW_INFO("rtw_read32(0x%x)=0x%08x\n", arg, rtw_read32(adapt, arg));
			break;
		}
		break;
	case 0x71: /* write_reg */
		switch (minor_cmd) {
		case 1:
			rtw_write8(adapt, arg, extra_arg);
			RTW_INFO("rtw_write8(0x%x)=0x%02x\n", arg, rtw_read8(adapt, arg));
			break;
		case 2:
			rtw_write16(adapt, arg, extra_arg);
			RTW_INFO("rtw_write16(0x%x)=0x%04x\n", arg, rtw_read16(adapt, arg));
			break;
		case 4:
			rtw_write32(adapt, arg, extra_arg);
			RTW_INFO("rtw_write32(0x%x)=0x%08x\n", arg, rtw_read32(adapt, arg));
			break;
		}
		break;
	case 0x72: /* read_bb */
		RTW_INFO("read_bbreg(0x%x)=0x%x\n", arg, rtw_hal_read_bbreg(adapt, arg, 0xffffffff));
		break;
	case 0x73: /* write_bb */
		rtw_hal_write_bbreg(adapt, arg, 0xffffffff, extra_arg);
		RTW_INFO("write_bbreg(0x%x)=0x%x\n", arg, rtw_hal_read_bbreg(adapt, arg, 0xffffffff));
		break;
	case 0x74: /* read_rf */
		RTW_INFO("read RF_reg path(0x%02x),offset(0x%x),value(0x%08x)\n", minor_cmd, arg, rtw_hal_read_rfreg(adapt, minor_cmd, arg, 0xffffffff));
		break;
	case 0x75: /* write_rf */
		rtw_hal_write_rfreg(adapt, minor_cmd, arg, 0xffffffff, extra_arg);
		RTW_INFO("write RF_reg path(0x%02x),offset(0x%x),value(0x%08x)\n", minor_cmd, arg, rtw_hal_read_rfreg(adapt, minor_cmd, arg, 0xffffffff));
		break;

	case 0x76:
		switch (minor_cmd) {
		case 0x00: /* normal mode, */
			adapt->recvpriv.is_signal_dbg = 0;
			break;
		case 0x01: /* dbg mode */
			adapt->recvpriv.is_signal_dbg = 1;
			extra_arg = extra_arg > 100 ? 100 : extra_arg;
			adapt->recvpriv.signal_strength_dbg = extra_arg;
			break;
		}
		break;
	case 0x78: /* IOL test */
		break;
	case 0x79: {
		/*
		* dbg 0x79000000 [value], set RESP_TXAGC to + value, value:0~15
		* dbg 0x79010000 [value], set RESP_TXAGC to - value, value:0~15
		*/
		u8 value =  extra_arg & 0x0f;
		u8 sign = minor_cmd;
		u16 write_value = 0;

		RTW_INFO("%s set RESP_TXAGC to %s %u\n", __func__, sign ? "minus" : "plus", value);

		if (sign)
			value = value | 0x10;

		write_value = value | (value << 5);
		rtw_write16(adapt, 0x6d9, write_value);
	}
		break;
	case 0x7a:
		receive_disconnect(adapt, pmlmeinfo->network.MacAddress
				   , WLAN_REASON_EXPIRATION_CHK, false);
		break;
	case 0x7F:
		switch (minor_cmd) {
		case 0x0:
			RTW_INFO("fwstate=0x%x\n", get_fwstate(pmlmepriv));
			break;
		case 0x01:
			RTW_INFO("auth_alg=0x%x, enc_alg=0x%x, auth_type=0x%x, enc_type=0x%x\n",
				psecuritypriv->dot11AuthAlgrthm, psecuritypriv->dot11PrivacyAlgrthm,
				psecuritypriv->ndisauthtype, psecuritypriv->ndisencryptstatus);
			break;
		case 0x02:
			RTW_INFO("pmlmeinfo->state=0x%x\n", pmlmeinfo->state);
			RTW_INFO("DrvBcnEarly=%d\n", pmlmeext->DrvBcnEarly);
			RTW_INFO("DrvBcnTimeOut=%d\n", pmlmeext->DrvBcnTimeOut);
			break;
		case 0x03:
			RTW_INFO("qos_option=%d\n", pmlmepriv->qospriv.qos_option);
			RTW_INFO("ht_option=%d\n", pmlmepriv->htpriv.ht_option);
			break;
		case 0x04:
			RTW_INFO("cur_ch=%d\n", pmlmeext->cur_channel);
			RTW_INFO("cur_bw=%d\n", pmlmeext->cur_bwmode);
			RTW_INFO("cur_ch_off=%d\n", pmlmeext->cur_ch_offset);

			RTW_INFO("oper_ch=%d\n", rtw_get_oper_ch(adapt));
			RTW_INFO("oper_bw=%d\n", rtw_get_oper_bw(adapt));
			RTW_INFO("oper_ch_offet=%d\n", rtw_get_oper_choffset(adapt));

			break;
		case 0x05:
			psta = rtw_get_stainfo(pstapriv, cur_network->network.MacAddress);
			if (psta) {
				RTW_INFO("SSID=%s\n", cur_network->network.Ssid.Ssid);
				RTW_INFO("sta's macaddr:" MAC_FMT "\n", MAC_ARG(psta->cmn.mac_addr));
				RTW_INFO("cur_channel=%d, cur_bwmode=%d, cur_ch_offset=%d\n", pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset);
				RTW_INFO("rtsen=%d, cts2slef=%d\n", psta->rtsen, psta->cts2self);
				RTW_INFO("state=0x%x, aid=%d, macid=%d, raid=%d\n",
					psta->state, psta->cmn.aid, psta->cmn.mac_id, psta->cmn.ra_info.rate_id);
				RTW_INFO("qos_en=%d, ht_en=%d, init_rate=%d\n", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
				RTW_INFO("bwmode=%d, ch_offset=%d, sgi_20m=%d,sgi_40m=%d\n"
					, psta->cmn.bw_mode, psta->htpriv.ch_offset, psta->htpriv.sgi_20m, psta->htpriv.sgi_40m);
				RTW_INFO("ampdu_enable = %d\n", psta->htpriv.ampdu_enable);
				RTW_INFO("agg_enable_bitmap=%x, candidate_tid_bitmap=%x\n", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);

				sta_rx_reorder_ctl_dump(RTW_DBGDUMP, psta);
			} else
				RTW_INFO("can't get sta's macaddr, cur_network's macaddr:" MAC_FMT "\n", MAC_ARG(cur_network->network.MacAddress));
			break;
		case 0x06:
			break;
		case 0x07:
			RTW_INFO("bSurpriseRemoved=%s, bDriverStopped=%s\n"
				, rtw_is_surprise_removed(adapt) ? "True" : "False"
				, rtw_is_drv_stopped(adapt) ? "True" : "False");
			break;
		case 0x08: {
			struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
			struct recv_priv  *precvpriv = &adapt->recvpriv;

			RTW_INFO("free_xmitbuf_cnt=%d, free_xmitframe_cnt=%d"
				", free_xmit_extbuf_cnt=%d, free_xframe_ext_cnt=%d"
				 ", free_recvframe_cnt=%d\n",
				pxmitpriv->free_xmitbuf_cnt, pxmitpriv->free_xmitframe_cnt,
				pxmitpriv->free_xmit_extbuf_cnt, pxmitpriv->free_xframe_ext_cnt,
				 precvpriv->free_recvframe_cnt);
			RTW_INFO("rx_urb_pending_cn=%d\n", ATOMIC_READ(&(precvpriv->rx_pending_cnt)));
		}
			break;
		case 0x09: {
			int i;
			struct list_head	*plist, *phead;

			RTW_INFO_DUMP("sta_dz_bitmap:", pstapriv->sta_dz_bitmap, pstapriv->aid_bmp_len);
			RTW_INFO_DUMP("tim_bitmap:", pstapriv->tim_bitmap, pstapriv->aid_bmp_len);
			_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

			for (i = 0; i < NUM_STA; i++) {
				phead = &(pstapriv->sta_hash[i]);
				plist = get_next(phead);

				while ((!rtw_end_of_queue_search(phead, plist))) {
					psta = container_of(plist, struct sta_info, hash_list);

					plist = get_next(plist);

					if (extra_arg == psta->cmn.aid) {
						RTW_INFO("sta's macaddr:" MAC_FMT "\n", MAC_ARG(psta->cmn.mac_addr));
						RTW_INFO("rtsen=%d, cts2slef=%d\n", psta->rtsen, psta->cts2self);
						RTW_INFO("state=0x%x, aid=%d, macid=%d, raid=%d\n",
							psta->state, psta->cmn.aid, psta->cmn.mac_id, psta->cmn.ra_info.rate_id);
						RTW_INFO("qos_en=%d, ht_en=%d, init_rate=%d\n", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
						RTW_INFO("bwmode=%d, ch_offset=%d, sgi_20m=%d,sgi_40m=%d\n",
							psta->cmn.bw_mode, psta->htpriv.ch_offset, psta->htpriv.sgi_20m,
							psta->htpriv.sgi_40m);
						RTW_INFO("ampdu_enable = %d\n", psta->htpriv.ampdu_enable);
						RTW_INFO("agg_enable_bitmap=%x, candidate_tid_bitmap=%x\n", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);

						RTW_INFO("capability=0x%x\n", psta->capability);
						RTW_INFO("flags=0x%x\n", psta->flags);
						RTW_INFO("wpa_psk=0x%x\n", psta->wpa_psk);
						RTW_INFO("wpa2_group_cipher=0x%x\n", psta->wpa2_group_cipher);
						RTW_INFO("wpa2_pairwise_cipher=0x%x\n", psta->wpa2_pairwise_cipher);
						RTW_INFO("qos_info=0x%x\n", psta->qos_info);
						RTW_INFO("dot118021XPrivacy=0x%x\n", psta->dot118021XPrivacy);

						sta_rx_reorder_ctl_dump(RTW_DBGDUMP, psta);
					}

				}
			}

			_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

		}
			break;

		case 0x0b: { /* Enable=1, Disable=0 driver control vrtl_carrier_sense. */
			/* u8 driver_vcs_en; */ /* Enable=1, Disable=0 driver control vrtl_carrier_sense. */
			/* u8 driver_vcs_type; */ /* force 0:disable VCS, 1:RTS-CTS, 2:CTS-to-self when vcs_en=1. */

			if (arg == 0) {
				RTW_INFO("disable driver ctrl vcs\n");
				adapt->driver_vcs_en = 0;
			} else if (arg == 1) {
				RTW_INFO("enable driver ctrl vcs = %d\n", extra_arg);
				adapt->driver_vcs_en = 1;

				if (extra_arg > 2)
					adapt->driver_vcs_type = 1;
				else
					adapt->driver_vcs_type = extra_arg;
			}
		}
			break;
		case 0x0c: { /* dump rx/tx packet */
			if (arg == 0) {
				RTW_INFO("dump rx packet (%d)\n", extra_arg);
				/* pHalData->bDumpRxPkt =extra_arg;						 */
				rtw_hal_set_def_var(adapt, HAL_DEF_DBG_DUMP_RXPKT, &(extra_arg));
			} else if (arg == 1) {
				RTW_INFO("dump tx packet (%d)\n", extra_arg);
				rtw_hal_set_def_var(adapt, HAL_DEF_DBG_DUMP_TXPKT, &(extra_arg));
			}
		}
			break;
		case 0x0e: {
			if (arg == 0) {
				RTW_INFO("disable driver ctrl rx_ampdu_factor\n");
				adapt->driver_rx_ampdu_factor = 0xFF;
			} else if (arg == 1) {

				RTW_INFO("enable driver ctrl rx_ampdu_factor = %d\n", extra_arg);

				if (extra_arg > 0x03)
					adapt->driver_rx_ampdu_factor = 0xFF;
				else
					adapt->driver_rx_ampdu_factor = extra_arg;
			}
		}
			break;
		case 0x10: /* driver version display */
			dump_drv_version(RTW_DBGDUMP);
			break;
		case 0x11: { /* dump linked status */
			int pre_mode;
			pre_mode = adapt->bLinkInfoDump;
			/* linked_info_dump(adapt,extra_arg); */
			if (extra_arg == 1 || (extra_arg == 0 && pre_mode == 1)) /* not consider pwr_saving 0: */
				adapt->bLinkInfoDump = extra_arg;

			else if ((extra_arg == 2) || (extra_arg == 0 && pre_mode == 2)) { /* consider power_saving */
				/* RTW_INFO("linked_info_dump =%s\n", (adapt->bLinkInfoDump)?"enable":"disable") */
				linked_info_dump(adapt, extra_arg);
			}



		}
			break;
		case 0x12: { /* set rx_stbc */
			struct registry_priv	*pregpriv = &adapt->registrypriv;
			/* 0: disable, bit(0):enable 2.4g, bit(1):enable 5g, 0x3: enable both 2.4g and 5g */
			/* default is set to enable 2.4GHZ for IOT issue with bufflao's AP at 5GHZ */
			if (pregpriv && (extra_arg == 0 || extra_arg == 1 || extra_arg == 2 || extra_arg == 3)) {
				pregpriv->rx_stbc = extra_arg;
				RTW_INFO("set rx_stbc=%d\n", pregpriv->rx_stbc);
			} else
				RTW_INFO("get rx_stbc=%d\n", pregpriv->rx_stbc);

		}
			break;
		case 0x13: { /* set ampdu_enable */
			struct registry_priv	*pregpriv = &adapt->registrypriv;
			/* 0: disable, 0x1:enable */
			if (pregpriv && extra_arg < 2) {
				pregpriv->ampdu_enable = extra_arg;
				RTW_INFO("set ampdu_enable=%d\n", pregpriv->ampdu_enable);
			} else
				RTW_INFO("get ampdu_enable=%d\n", pregpriv->ampdu_enable);

		}
			break;
		case 0x14: { /* get wifi_spec */
			struct registry_priv	*pregpriv = &adapt->registrypriv;
			RTW_INFO("get wifi_spec=%d\n", pregpriv->wifi_spec);

		}
			break;
		case 0x19: {
			struct registry_priv	*pregistrypriv = &adapt->registrypriv;
			/* extra_arg : */
			/* BIT0: Enable VHT LDPC Rx, BIT1: Enable VHT LDPC Tx, */
			/* BIT4: Enable HT LDPC Rx, BIT5: Enable HT LDPC Tx */
			if (arg == 0) {
				RTW_INFO("driver disable LDPC\n");
				pregistrypriv->ldpc_cap = 0x00;
			} else if (arg == 1) {
				RTW_INFO("driver set LDPC cap = 0x%x\n", extra_arg);
				pregistrypriv->ldpc_cap = (u8)(extra_arg & 0x33);
			}
		}
			break;
		case 0x1a: {
			struct registry_priv	*pregistrypriv = &adapt->registrypriv;
			/* extra_arg : */
			/* BIT0: Enable VHT STBC Rx, BIT1: Enable VHT STBC Tx, */
			/* BIT4: Enable HT STBC Rx, BIT5: Enable HT STBC Tx */
			if (arg == 0) {
				RTW_INFO("driver disable STBC\n");
				pregistrypriv->stbc_cap = 0x00;
			} else if (arg == 1) {
				RTW_INFO("driver set STBC cap = 0x%x\n", extra_arg);
				pregistrypriv->stbc_cap = (u8)(extra_arg & 0x33);
			}
		}
			break;
		case 0x1b: {
			struct registry_priv	*pregistrypriv = &adapt->registrypriv;

			if (arg == 0) {
				RTW_INFO("disable driver ctrl max_rx_rate, reset to default_rate_set\n");
				init_mlme_default_rate_set(adapt);
				pregistrypriv->ht_enable = (u8)rtw_ht_enable;
			} else if (arg == 1) {

				int i;
				u8 max_rx_rate;

				RTW_INFO("enable driver ctrl max_rx_rate = 0x%x\n", extra_arg);

				max_rx_rate = (u8)extra_arg;

				if (max_rx_rate < 0xc) { /* max_rx_rate < MSC0->B or G -> disable HT */
					pregistrypriv->ht_enable = 0;
					for (i = 0; i < NumRates; i++) {
						if (pmlmeext->datarate[i] > max_rx_rate)
							pmlmeext->datarate[i] = 0xff;
					}

				} else if (max_rx_rate < 0x1c) { /* mcs0~mcs15 */
					u32 mcs_bitmap = 0x0;

					for (i = 0; i < ((max_rx_rate + 1) - 0xc); i++)
						mcs_bitmap |= BIT(i);

					set_mcs_rate_by_mask(pmlmeext->default_supported_mcs_set, mcs_bitmap);
				}
			}
		}
			break;
		case 0x1c: { /* enable/disable driver control AMPDU Density for peer sta's rx */
			if (arg == 0) {
				RTW_INFO("disable driver ctrl ampdu density\n");
				adapt->driver_ampdu_spacing = 0xFF;
			} else if (arg == 1) {

				RTW_INFO("enable driver ctrl ampdu density = %d\n", extra_arg);

				if (extra_arg > 0x07)
					adapt->driver_ampdu_spacing = 0xFF;
				else
					adapt->driver_ampdu_spacing = extra_arg;
			}
		}
			break;
		case 0x20:
			if (arg == 0xAA) {
				u8 page_offset, page_num;

				page_offset = (u8)(extra_arg >> 16);
				page_num = (u8)(extra_arg & 0xFF);
				rtw_dump_rsvd_page(RTW_DBGDUMP, adapt, page_offset, page_num);
			}
		break;
		case 0x23: {
			RTW_INFO("turn %s the bNotifyChannelChange Variable\n", (extra_arg == 1) ? "on" : "off");
			adapt->bNotifyChannelChange = extra_arg;
			break;
		}
		case 0x24: {
			RTW_INFO("turn %s the bShowGetP2PState Variable\n", (extra_arg == 1) ? "on" : "off");
			adapt->bShowGetP2PState = extra_arg;
			break;
		}
		case 0xaa: {
			if ((extra_arg & 0x7F) > 0x3F)
				extra_arg = 0xFF;
			RTW_INFO("chang data rate to :0x%02x\n", extra_arg);
			adapt->fix_rate = extra_arg;
		}
			break;
		case 0xdd: { /* registers dump , 0 for mac reg,1 for bb reg, 2 for rf reg */
			if (extra_arg == 0)
				mac_reg_dump(RTW_DBGDUMP, adapt);
			else if (extra_arg == 1)
				bb_reg_dump(RTW_DBGDUMP, adapt);
			else if (extra_arg == 2)
				rf_reg_dump(RTW_DBGDUMP, adapt);
			else if (extra_arg == 11)
				bb_reg_dump_ex(RTW_DBGDUMP, adapt);
		}
			break;

		case 0xee: {
			RTW_INFO(" === please control /proc  to trun on/off PHYDM func ===\n");
		}
			break;

		case 0xfd:
			rtw_write8(adapt, 0xc50, arg);
			RTW_INFO("wr(0xc50)=0x%x\n", rtw_read8(adapt, 0xc50));
			rtw_write8(adapt, 0xc58, arg);
			RTW_INFO("wr(0xc58)=0x%x\n", rtw_read8(adapt, 0xc58));
			break;
		case 0xfe:
			RTW_INFO("rd(0xc50)=0x%x\n", rtw_read8(adapt, 0xc50));
			RTW_INFO("rd(0xc58)=0x%x\n", rtw_read8(adapt, 0xc58));
			break;
		case 0xff: {
			RTW_INFO("dbg(0x210)=0x%x\n", rtw_read32(adapt, 0x210));
			RTW_INFO("dbg(0x608)=0x%x\n", rtw_read32(adapt, 0x608));
			RTW_INFO("dbg(0x280)=0x%x\n", rtw_read32(adapt, 0x280));
			RTW_INFO("dbg(0x284)=0x%x\n", rtw_read32(adapt, 0x284));
			RTW_INFO("dbg(0x288)=0x%x\n", rtw_read32(adapt, 0x288));

			RTW_INFO("dbg(0x664)=0x%x\n", rtw_read32(adapt, 0x664));


			RTW_INFO("\n");

			RTW_INFO("dbg(0x430)=0x%x\n", rtw_read32(adapt, 0x430));
			RTW_INFO("dbg(0x438)=0x%x\n", rtw_read32(adapt, 0x438));

			RTW_INFO("dbg(0x440)=0x%x\n", rtw_read32(adapt, 0x440));

			RTW_INFO("dbg(0x458)=0x%x\n", rtw_read32(adapt, 0x458));

			RTW_INFO("dbg(0x484)=0x%x\n", rtw_read32(adapt, 0x484));
			RTW_INFO("dbg(0x488)=0x%x\n", rtw_read32(adapt, 0x488));

			RTW_INFO("dbg(0x444)=0x%x\n", rtw_read32(adapt, 0x444));
			RTW_INFO("dbg(0x448)=0x%x\n", rtw_read32(adapt, 0x448));
			RTW_INFO("dbg(0x44c)=0x%x\n", rtw_read32(adapt, 0x44c));
			RTW_INFO("dbg(0x450)=0x%x\n", rtw_read32(adapt, 0x450));
		}
			break;
		}
		break;
	default:
		RTW_INFO("error dbg cmd!\n");
		break;
	}


	return ret;

}

static int wpa_set_param(struct net_device *dev, u8 name, u32 value)
{
	uint ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	switch (name) {
	case IEEE_PARAM_WPA_ENABLED:

		adapt->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_8021X; /* 802.1x */

		/* ret = ieee80211_wpa_enable(ieee, value); */

		switch ((value) & 0xff) {
		case 1: /* WPA */
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeWPAPSK; /* WPA_PSK */
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
			break;
		case 2: /* WPA2 */
			adapt->securitypriv.ndisauthtype = Ndis802_11AuthModeWPA2PSK; /* WPA2_PSK */
			adapt->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
			break;
		}


		break;

	case IEEE_PARAM_TKIP_COUNTERMEASURES:
		/* ieee->tkip_countermeasures=value; */
		break;

	case IEEE_PARAM_DROP_UNENCRYPTED: {
		/* HACK:
		 *
		 * wpa_supplicant calls set_wpa_enabled when the driver
		 * is loaded and unloaded, regardless of if WPA is being
		 * used.  No other calls are made which can be used to
		 * determine if encryption will be used or not prior to
		 * association being expected.  If encryption is not being
		 * used, drop_unencrypted is set to false, else true -- we
		 * can use this to determine if the CAP_PRIVACY_ON bit should
		 * be set.
		 */

		break;

	}
	case IEEE_PARAM_PRIVACY_INVOKED:

		/* ieee->privacy_invoked=value; */

		break;

	case IEEE_PARAM_AUTH_ALGS:

		ret = wpa_set_auth_algs(dev, value);

		break;

	case IEEE_PARAM_IEEE_802_1X:

		/* ieee->ieee802_1x=value;		 */

		break;

	case IEEE_PARAM_WPAX_SELECT:

		/* added for WPA2 mixed mode */
		/*RTW_WARN("------------------------>wpax value = %x\n", value);*/
		/*
		spin_lock_irqsave(&ieee->wpax_suitlist_lock,flags);
		ieee->wpax_type_set = 1;
		ieee->wpax_type_notify = value;
		spin_unlock_irqrestore(&ieee->wpax_suitlist_lock,flags);
		*/

		break;

	default:



		ret = -EOPNOTSUPP;


		break;

	}

	return ret;

}

static int wpa_mlme(struct net_device *dev, u32 command, u32 reason)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	switch (command) {
	case IEEE_MLME_STA_DEAUTH:

		if (!rtw_set_802_11_disassociate(adapt))
			ret = -1;

		break;

	case IEEE_MLME_STA_DISASSOC:

		if (!rtw_set_802_11_disassociate(adapt))
			ret = -1;

		break;

	default:
		ret = -EOPNOTSUPP;
		break;
	}
	return ret;
}

static int wpa_supplicant_ioctl(struct net_device *dev, struct iw_point *p)
{
	struct ieee_param *param;
	uint ret = 0;

	/* down(&ieee->wx_sem);	 */

	if (p->length < sizeof(struct ieee_param) || !p->pointer) {
		ret = -EINVAL;
		goto out;
	}

	param = (struct ieee_param *)rtw_malloc(p->length);
	if (!param) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, p->pointer, p->length)) {
		rtw_mfree((u8 *)param, p->length);
		ret = -EFAULT;
		goto out;
	}

	switch (param->cmd) {

	case IEEE_CMD_SET_WPA_PARAM:
		ret = wpa_set_param(dev, param->u.wpa_param.name, param->u.wpa_param.value);
		break;

	case IEEE_CMD_SET_WPA_IE:
		/* ret = wpa_set_wpa_ie(dev, param, p->length); */
		ret =  rtw_set_wpa_ie((struct adapter *)rtw_netdev_priv(dev), (char *)param->u.wpa_ie.data, (u16)param->u.wpa_ie.len);
		break;

	case IEEE_CMD_SET_ENCRYPTION:
		ret = wpa_set_encryption(dev, param, p->length);
		break;

	case IEEE_CMD_MLME:
		ret = wpa_mlme(dev, param->u.mlme.command, param->u.mlme.reason_code);
		break;

	default:
		RTW_INFO("Unknown WPA supplicant request: %d\n", param->cmd);
		ret = -EOPNOTSUPP;
		break;

	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;

	rtw_mfree((u8 *)param, p->length);

out:

	/* up(&ieee->wx_sem); */

	return ret;

}

static int rtw_set_encryption(struct net_device *dev, struct ieee_param *param, u32 param_len)
{
	int ret = 0;
	u32 wep_key_idx, wep_key_len, wep_total_len;
	struct ndis_802_11_wep	*pwep = NULL;
	struct sta_info *psta = NULL, *pbcmc_sta = NULL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	struct sta_priv *pstapriv = &adapt->stapriv;

	RTW_INFO("%s\n", __func__);

	param->u.crypt.err = 0;
	param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

	/* sizeof(struct ieee_param) = 64 bytes; */
	/* if (param_len !=  (u32) ((u8 *) param->u.crypt.key - (u8 *) param) + param->u.crypt.key_len) */
	if (param_len !=  sizeof(struct ieee_param) + param->u.crypt.key_len) {
		ret =  -EINVAL;
		goto exit;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
		if (param->u.crypt.idx >= WEP_KEYS
#ifdef CONFIG_IEEE80211W
		    && param->u.crypt.idx > BIP_MAX_KEYID
#endif /* CONFIG_IEEE80211W */
		   ) {
			ret = -EINVAL;
			goto exit;
		}
	} else {
		psta = rtw_get_stainfo(pstapriv, param->sta_addr);
		if (!psta) {
			/* ret = -EINVAL; */
			RTW_INFO("rtw_set_encryption(), sta has already been removed or never been added\n");
			goto exit;
		}
	}

	if (strcmp(param->u.crypt.alg, "none") == 0 && (!psta)) {
		/* todo:clear default encryption keys */

		psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_Open;
		psecuritypriv->ndisencryptstatus = Ndis802_11EncryptionDisabled;
		psecuritypriv->dot11PrivacyAlgrthm = _NO_PRIVACY_;
		psecuritypriv->dot118021XGrpPrivacy = _NO_PRIVACY_;

		RTW_INFO("clear default encryption keys, keyid=%d\n", param->u.crypt.idx);

		goto exit;
	}


	if (strcmp(param->u.crypt.alg, "WEP") == 0 && (!psta)) {
		RTW_INFO("r871x_set_encryption, crypt.alg = WEP\n");

		wep_key_idx = param->u.crypt.idx;
		wep_key_len = param->u.crypt.key_len;

		RTW_INFO("r871x_set_encryption, wep_key_idx=%d, len=%d\n", wep_key_idx, wep_key_len);

		if ((wep_key_idx >= WEP_KEYS) || (wep_key_len <= 0)) {
			ret = -EINVAL;
			goto exit;
		}


		if (wep_key_len > 0) {
			wep_key_len = wep_key_len <= 5 ? 5 : 13;
			wep_total_len = wep_key_len + FIELD_OFFSET(struct ndis_802_11_wep, KeyMaterial);
			pwep = (struct ndis_802_11_wep *)rtw_malloc(wep_total_len);
			if (!pwep) {
				RTW_INFO(" r871x_set_encryption: pwep allocate fail !!!\n");
				goto exit;
			}

			memset(pwep, 0, wep_total_len);

			pwep->KeyLength = wep_key_len;
			pwep->Length = wep_total_len;

		}

		pwep->KeyIndex = wep_key_idx;

		memcpy(pwep->KeyMaterial,  param->u.crypt.key, pwep->KeyLength);

		if (param->u.crypt.set_tx) {
			RTW_INFO("wep, set_tx=1\n");

			psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
			psecuritypriv->ndisencryptstatus = Ndis802_11Encryption1Enabled;
			psecuritypriv->dot11PrivacyAlgrthm = _WEP40_;
			psecuritypriv->dot118021XGrpPrivacy = _WEP40_;

			if (pwep->KeyLength == 13) {
				psecuritypriv->dot11PrivacyAlgrthm = _WEP104_;
				psecuritypriv->dot118021XGrpPrivacy = _WEP104_;
			}


			psecuritypriv->dot11PrivacyKeyIndex = wep_key_idx;

			memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), pwep->KeyMaterial, pwep->KeyLength);

			psecuritypriv->dot11DefKeylen[wep_key_idx] = pwep->KeyLength;

			rtw_ap_set_wep_key(adapt, pwep->KeyMaterial, pwep->KeyLength, wep_key_idx, 1);
		} else {
			RTW_INFO("wep, set_tx=0\n");

			/* don't update "psecuritypriv->dot11PrivacyAlgrthm" and  */
			/* "psecuritypriv->dot11PrivacyKeyIndex=keyid", but can rtw_set_key to cam */

			memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), pwep->KeyMaterial, pwep->KeyLength);

			psecuritypriv->dot11DefKeylen[wep_key_idx] = pwep->KeyLength;

			rtw_ap_set_wep_key(adapt, pwep->KeyMaterial, pwep->KeyLength, wep_key_idx, 0);
		}

		goto exit;

	}


	if (!psta && check_fwstate(pmlmepriv, WIFI_AP_STATE)) /*  */ { /* group key */
		if (param->u.crypt.set_tx == 1) {
			if (strcmp(param->u.crypt.alg, "WEP") == 0) {
				RTW_INFO(FUNC_ADPT_FMT" set WEP TX GTK idx:%u, len:%u\n"
					, FUNC_ADPT_ARG(adapt), param->u.crypt.idx, param->u.crypt.key_len);
				memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));
				psecuritypriv->dot118021XGrpPrivacy = _WEP40_;
				if (param->u.crypt.key_len == 13)
					psecuritypriv->dot118021XGrpPrivacy = _WEP104_;

			} else if (strcmp(param->u.crypt.alg, "TKIP") == 0) {
				RTW_INFO(FUNC_ADPT_FMT" set TKIP TX GTK idx:%u, len:%u\n"
					, FUNC_ADPT_ARG(adapt), param->u.crypt.idx, param->u.crypt.key_len);
				psecuritypriv->dot118021XGrpPrivacy = _TKIP_;
				memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));
				/* set mic key */
				memcpy(psecuritypriv->dot118021XGrptxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[16]), 8);
				memcpy(psecuritypriv->dot118021XGrprxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[24]), 8);
				psecuritypriv->busetkipkey = true;

			} else if (strcmp(param->u.crypt.alg, "CCMP") == 0) {
				RTW_INFO(FUNC_ADPT_FMT" set CCMP TX GTK idx:%u, len:%u\n"
					, FUNC_ADPT_ARG(adapt), param->u.crypt.idx, param->u.crypt.key_len);
				psecuritypriv->dot118021XGrpPrivacy = _AES_;
				memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));

			#ifdef CONFIG_IEEE80211W
			} else if (strcmp(param->u.crypt.alg, "BIP") == 0) {
				RTW_INFO(FUNC_ADPT_FMT" set TX IGTK idx:%u, len:%u\n"
					, FUNC_ADPT_ARG(adapt), param->u.crypt.idx, param->u.crypt.key_len);
				memcpy(adapt->securitypriv.dot11wBIPKey[param->u.crypt.idx].skey, param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));
				psecuritypriv->dot11wBIPKeyid = param->u.crypt.idx;
				psecuritypriv->dot11wBIPtxpn.val = RTW_GET_LE64(param->u.crypt.seq);
				psecuritypriv->binstallBIPkey = true;
				goto exit;
			#endif /* CONFIG_IEEE80211W */

			} else if (strcmp(param->u.crypt.alg, "none") == 0) {
				RTW_INFO(FUNC_ADPT_FMT" clear group key, idx:%u\n"
					, FUNC_ADPT_ARG(adapt), param->u.crypt.idx);
				psecuritypriv->dot118021XGrpPrivacy = _NO_PRIVACY_;
			} else {
				RTW_WARN(FUNC_ADPT_FMT" set group key, not support\n"
					, FUNC_ADPT_ARG(adapt));
				goto exit;
			}

			psecuritypriv->dot118021XGrpKeyid = param->u.crypt.idx;
			pbcmc_sta = rtw_get_bcmc_stainfo(adapt);
			if (pbcmc_sta) {
				pbcmc_sta->dot11txpn.val = RTW_GET_LE64(param->u.crypt.seq);
				pbcmc_sta->ieee8021x_blocked = false;
				pbcmc_sta->dot118021XPrivacy = psecuritypriv->dot118021XGrpPrivacy; /* rx will use bmc_sta's dot118021XPrivacy			 */
			}
			psecuritypriv->binstallGrpkey = true;
			psecuritypriv->dot11PrivacyAlgrthm = psecuritypriv->dot118021XGrpPrivacy;/* !!! */

			rtw_ap_set_group_key(adapt, param->u.crypt.key, psecuritypriv->dot118021XGrpPrivacy, param->u.crypt.idx);
		}

		goto exit;

	}

	if (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X && psta) { /* psk/802_1x */
		if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
			if (param->u.crypt.set_tx == 1) {
				memcpy(psta->dot118021x_UncstKey.skey,  param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16 : param->u.crypt.key_len));

				if (strcmp(param->u.crypt.alg, "WEP") == 0) {
					RTW_INFO(FUNC_ADPT_FMT" set WEP PTK of "MAC_FMT" idx:%u, len:%u\n"
						, FUNC_ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr)
						, param->u.crypt.idx, param->u.crypt.key_len);
					psta->dot118021XPrivacy = _WEP40_;
					if (param->u.crypt.key_len == 13)
						psta->dot118021XPrivacy = _WEP104_;

				} else if (strcmp(param->u.crypt.alg, "TKIP") == 0) {
					RTW_INFO(FUNC_ADPT_FMT" set TKIP PTK of "MAC_FMT" idx:%u, len:%u\n"
						, FUNC_ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr)
						, param->u.crypt.idx, param->u.crypt.key_len);
					psta->dot118021XPrivacy = _TKIP_;
					/* set mic key */
					memcpy(psta->dot11tkiptxmickey.skey, &(param->u.crypt.key[16]), 8);
					memcpy(psta->dot11tkiprxmickey.skey, &(param->u.crypt.key[24]), 8);
					psecuritypriv->busetkipkey = true;

				} else if (strcmp(param->u.crypt.alg, "CCMP") == 0) {
					RTW_INFO(FUNC_ADPT_FMT" set CCMP PTK of "MAC_FMT" idx:%u, len:%u\n"
						, FUNC_ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr)
						, param->u.crypt.idx, param->u.crypt.key_len);
					psta->dot118021XPrivacy = _AES_;

				} else if (strcmp(param->u.crypt.alg, "none") == 0) {
					RTW_INFO(FUNC_ADPT_FMT" clear pairwise key of "MAC_FMT" idx:%u\n"
						, FUNC_ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr)
						, param->u.crypt.idx);
					psta->dot118021XPrivacy = _NO_PRIVACY_;

				} else {
					RTW_WARN(FUNC_ADPT_FMT" set pairwise key of "MAC_FMT", not support\n"
						, FUNC_ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr));
					goto exit;
				}

				psta->dot11txpn.val = RTW_GET_LE64(param->u.crypt.seq);
				psta->dot11rxpn.val = RTW_GET_LE64(param->u.crypt.seq);
				psta->ieee8021x_blocked = false;
				psta->bpairwise_key_installed = true;
				rtw_ap_set_pairwise_key(adapt, psta);

			} else {
				RTW_WARN(FUNC_ADPT_FMT" set group key of "MAC_FMT", not support\n"
					, FUNC_ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr));
				goto exit;
			}

		}

	}

exit:

	if (pwep)
		rtw_mfree((u8 *)pwep, wep_total_len);

	return ret;

}

static int rtw_set_beacon(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct sta_priv *pstapriv = &adapt->stapriv;
	unsigned char *pbuf = param->u.bcn_ie.buf;


	RTW_INFO("%s, len=%d\n", __func__, len);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	memcpy(&pstapriv->max_num_sta, param->u.bcn_ie.reserved, 2);

	if ((pstapriv->max_num_sta > NUM_STA) || (pstapriv->max_num_sta <= 0))
		pstapriv->max_num_sta = NUM_STA;


	if (rtw_check_beacon_data(adapt, pbuf, (len - 12 - 2)) == _SUCCESS) /* 12 = param header, 2:no packed */
		ret = 0;
	else
		ret = -EINVAL;


	return ret;

}

static int rtw_hostapd_sta_flush(struct net_device *dev)
{
	/* unsigned long irqL; */
	/* _list	*phead, *plist; */
	int ret = 0;
	/* struct sta_info *psta = NULL; */
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	/* struct sta_priv *pstapriv = &adapt->stapriv; */

	RTW_INFO("%s\n", __func__);

	flush_all_cam_entry(adapt);	/* clear CAM */
	ret = rtw_sta_flush(adapt, true);
	return ret;

}

static int rtw_add_sta(struct net_device *dev, struct ieee_param *param)
{
	int ret = 0;
	struct sta_info *psta = NULL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct sta_priv *pstapriv = &adapt->stapriv;

	RTW_INFO("rtw_add_sta(aid=%d)=" MAC_FMT "\n", param->u.add_sta.aid, MAC_ARG(param->sta_addr));

	if (!check_fwstate(pmlmepriv, (_FW_LINKED | WIFI_AP_STATE)))
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
		return -EINVAL;

	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if (psta) {
		int flags = param->u.add_sta.flags;

		psta->cmn.aid = param->u.add_sta.aid;/* aid=1~2007 */

		memcpy(psta->bssrateset, param->u.add_sta.tx_supp_rates, 16);


		/* check wmm cap. */
		if (WLAN_STA_WME & flags)
			psta->qos_option = 1;
		else
			psta->qos_option = 0;

		if (pmlmepriv->qospriv.qos_option == 0)
			psta->qos_option = 0;


		/* chec 802.11n ht cap. */
		if (WLAN_STA_HT & flags) {
			psta->htpriv.ht_option = true;
			psta->qos_option = 1;
			memcpy((void *)&psta->htpriv.ht_cap, (void *)&param->u.add_sta.ht_cap, sizeof(struct rtw_ieee80211_ht_cap));
		} else
			psta->htpriv.ht_option = false;

		if (!pmlmepriv->htpriv.ht_option)
			psta->htpriv.ht_option = false;

		update_sta_info_apmode(adapt, psta);
	} else
		ret = -ENOMEM;

	return ret;
}

static int rtw_del_sta(struct net_device *dev, struct ieee_param *param)
{
	unsigned long irqL;
	int ret = 0;
	struct sta_info *psta = NULL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct sta_priv *pstapriv = &adapt->stapriv;

	RTW_INFO("rtw_del_sta=" MAC_FMT "\n", MAC_ARG(param->sta_addr));

	if (!check_fwstate(pmlmepriv, (_FW_LINKED | WIFI_AP_STATE)))
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
		return -EINVAL;

	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if (psta) {
		u8 updated = false;

		/* RTW_INFO("free psta=%p, aid=%d\n", psta, psta->cmn.aid); */

		_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
		if (!rtw_is_list_empty(&psta->asoc_list)) {
			rtw_list_delete(&psta->asoc_list);
			pstapriv->asoc_list_cnt--;
			updated = ap_free_sta(adapt, psta, true, WLAN_REASON_DEAUTH_LEAVING, true);

		}
		_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

		associated_clients_update(adapt, updated, STA_INFO_UPDATE_ALL);

		psta = NULL;

	} else {
		RTW_INFO("rtw_del_sta(), sta has already been removed or never been added\n");

		/* ret = -1; */
	}


	return ret;

}

static int rtw_ioctl_get_sta_data(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct sta_info *psta = NULL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct ieee_param_ex *param_ex = (struct ieee_param_ex *)param;
	struct sta_data *psta_data = (struct sta_data *)param_ex->data;

	RTW_INFO("rtw_ioctl_get_sta_info, sta_addr: " MAC_FMT "\n", MAC_ARG(param_ex->sta_addr));

	if (!check_fwstate(pmlmepriv, (_FW_LINKED | WIFI_AP_STATE)))
		return -EINVAL;

	if (param_ex->sta_addr[0] == 0xff && param_ex->sta_addr[1] == 0xff &&
	    param_ex->sta_addr[2] == 0xff && param_ex->sta_addr[3] == 0xff &&
	    param_ex->sta_addr[4] == 0xff && param_ex->sta_addr[5] == 0xff)
		return -EINVAL;

	psta = rtw_get_stainfo(pstapriv, param_ex->sta_addr);
	if (psta) {
		psta_data->aid = (u16)psta->cmn.aid;
		psta_data->capability = psta->capability;
		psta_data->flags = psta->flags;

		/*
				nonerp_set : BIT(0)
				no_short_slot_time_set : BIT(1)
				no_short_preamble_set : BIT(2)
				no_ht_gf_set : BIT(3)
				no_ht_set : BIT(4)
				ht_20mhz_set : BIT(5)
		*/

		psta_data->sta_set = ((psta->nonerp_set) |
				      (psta->no_short_slot_time_set << 1) |
				      (psta->no_short_preamble_set << 2) |
				      (psta->no_ht_gf_set << 3) |
				      (psta->no_ht_set << 4) |
				      (psta->ht_20mhz_set << 5));

		psta_data->tx_supp_rates_len =  psta->bssratelen;
		memcpy(psta_data->tx_supp_rates, psta->bssrateset, psta->bssratelen);
		memcpy(&psta_data->ht_cap, &psta->htpriv.ht_cap, sizeof(struct rtw_ieee80211_ht_cap));
		psta_data->rx_pkts = psta->sta_stats.rx_data_pkts;
		psta_data->rx_bytes = psta->sta_stats.rx_bytes;
		psta_data->rx_drops = psta->sta_stats.rx_drops;

		psta_data->tx_pkts = psta->sta_stats.tx_pkts;
		psta_data->tx_bytes = psta->sta_stats.tx_bytes;
		psta_data->tx_drops = psta->sta_stats.tx_drops;


	} else
		ret = -1;

	return ret;

}

static int rtw_get_sta_wpaie(struct net_device *dev, struct ieee_param *param)
{
	int ret = 0;
	struct sta_info *psta = NULL;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct sta_priv *pstapriv = &adapt->stapriv;

	RTW_INFO("rtw_get_sta_wpaie, sta_addr: " MAC_FMT "\n", MAC_ARG(param->sta_addr));

	if (!check_fwstate(pmlmepriv, (_FW_LINKED | WIFI_AP_STATE)))
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
		return -EINVAL;

	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if (psta) {
		if ((psta->wpa_ie[0] == WLAN_EID_RSN) || (psta->wpa_ie[0] == WLAN_EID_GENERIC)) {
			int wpa_ie_len;
			int copy_len;

			wpa_ie_len = psta->wpa_ie[1];

			copy_len = ((wpa_ie_len + 2) > sizeof(psta->wpa_ie)) ? (sizeof(psta->wpa_ie)) : (wpa_ie_len + 2);

			param->u.wpa_ie.len = copy_len;

			memcpy(param->u.wpa_ie.reserved, psta->wpa_ie, copy_len);
		} else {
			/* ret = -1; */
			RTW_INFO("sta's wpa_ie is NONE\n");
		}
	} else
		ret = -1;

	return ret;

}

static int rtw_set_wps_beacon(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	unsigned char wps_oui[4] = {0x0, 0x50, 0xf2, 0x04};
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	int ie_len;

	RTW_INFO("%s, len=%d\n", __func__, len);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	ie_len = len - 12 - 2; /* 12 = param header, 2:no packed */


	if (pmlmepriv->wps_beacon_ie) {
		rtw_mfree(pmlmepriv->wps_beacon_ie, pmlmepriv->wps_beacon_ie_len);
		pmlmepriv->wps_beacon_ie = NULL;
	}

	if (ie_len > 0) {
		pmlmepriv->wps_beacon_ie = rtw_malloc(ie_len);
		pmlmepriv->wps_beacon_ie_len = ie_len;
		if (!pmlmepriv->wps_beacon_ie) {
			RTW_INFO("%s()-%d: rtw_malloc() ERROR!\n", __func__, __LINE__);
			return -EINVAL;
		}

		memcpy(pmlmepriv->wps_beacon_ie, param->u.bcn_ie.buf, ie_len);

		update_beacon(adapt, _VENDOR_SPECIFIC_IE_, wps_oui, true);

		pmlmeext->bstart_bss = true;

	}


	return ret;

}

static int rtw_set_wps_probe_resp(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	int ie_len;

	RTW_INFO("%s, len=%d\n", __func__, len);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	ie_len = len - 12 - 2; /* 12 = param header, 2:no packed */


	if (pmlmepriv->wps_probe_resp_ie) {
		rtw_mfree(pmlmepriv->wps_probe_resp_ie, pmlmepriv->wps_probe_resp_ie_len);
		pmlmepriv->wps_probe_resp_ie = NULL;
	}

	if (ie_len > 0) {
		pmlmepriv->wps_probe_resp_ie = rtw_malloc(ie_len);
		pmlmepriv->wps_probe_resp_ie_len = ie_len;
		if (!pmlmepriv->wps_probe_resp_ie) {
			RTW_INFO("%s()-%d: rtw_malloc() ERROR!\n", __func__, __LINE__);
			return -EINVAL;
		}
		memcpy(pmlmepriv->wps_probe_resp_ie, param->u.bcn_ie.buf, ie_len);
	}


	return ret;

}

static int rtw_set_wps_assoc_resp(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	int ie_len;

	RTW_INFO("%s, len=%d\n", __func__, len);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	ie_len = len - 12 - 2; /* 12 = param header, 2:no packed */


	if (pmlmepriv->wps_assoc_resp_ie) {
		rtw_mfree(pmlmepriv->wps_assoc_resp_ie, pmlmepriv->wps_assoc_resp_ie_len);
		pmlmepriv->wps_assoc_resp_ie = NULL;
	}

	if (ie_len > 0) {
		pmlmepriv->wps_assoc_resp_ie = rtw_malloc(ie_len);
		pmlmepriv->wps_assoc_resp_ie_len = ie_len;
		if (!pmlmepriv->wps_assoc_resp_ie) {
			RTW_INFO("%s()-%d: rtw_malloc() ERROR!\n", __func__, __LINE__);
			return -EINVAL;
		}

		memcpy(pmlmepriv->wps_assoc_resp_ie, param->u.bcn_ie.buf, ie_len);
	}


	return ret;

}

static int rtw_set_hidden_ssid(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlmepriv = &(adapter->mlmepriv);
	struct mlme_ext_priv	*mlmeext = &(adapter->mlmeextpriv);
	struct mlme_ext_info	*mlmeinfo = &(mlmeext->mlmext_info);
	int ie_len;
	u8 *ssid_ie;
	char ssid[NDIS_802_11_LENGTH_SSID + 1];
	int ssid_len = 0;
	u8 ignore_broadcast_ssid;

	if (!check_fwstate(mlmepriv, WIFI_AP_STATE))
		return -EPERM;

	if (param->u.bcn_ie.reserved[0] != 0xea)
		return -EINVAL;

	mlmeinfo->hidden_ssid_mode = ignore_broadcast_ssid = param->u.bcn_ie.reserved[1];

	ie_len = len - 12 - 2; /* 12 = param header, 2:no packed */
	ssid_ie = rtw_get_ie(param->u.bcn_ie.buf,  WLAN_EID_SSID, &ssid_len, ie_len);

	if (ssid_ie && ssid_len > 0 && ssid_len <= NDIS_802_11_LENGTH_SSID) {
		struct wlan_bssid_ex *pbss_network = &mlmepriv->cur_network.network;
		struct wlan_bssid_ex *pbss_network_ext = &mlmeinfo->network;

		memcpy(ssid, ssid_ie + 2, ssid_len);
		ssid[ssid_len] = 0x0;

		memcpy(pbss_network->Ssid.Ssid, (void *)ssid, ssid_len);
		pbss_network->Ssid.SsidLength = ssid_len;
		memcpy(pbss_network_ext->Ssid.Ssid, (void *)ssid, ssid_len);
		pbss_network_ext->Ssid.SsidLength = ssid_len;
	}

	RTW_INFO(FUNC_ADPT_FMT" ignore_broadcast_ssid:%d, %s,%d\n", FUNC_ADPT_ARG(adapter),
		ignore_broadcast_ssid, ssid, ssid_len);

	return ret;
}

#if CONFIG_RTW_MACADDR_ACL
static int rtw_ioctl_acl_remove_sta(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
		return -EINVAL;

	ret = rtw_acl_remove_sta(adapt, param->sta_addr);

	return ret;

}

static int rtw_ioctl_acl_add_sta(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
		return -EINVAL;

	ret = rtw_acl_add_sta(adapt, param->sta_addr);

	return ret;

}

static int rtw_ioctl_set_macaddr_acl(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	if (!check_fwstate(pmlmepriv, WIFI_AP_STATE))
		return -EINVAL;

	rtw_set_macaddr_acl(adapt, param->u.mlme.command);

	return ret;
}
#endif /* CONFIG_RTW_MACADDR_ACL */

static int rtw_hostapd_ioctl(struct net_device *dev, struct iw_point *p)
{
	struct ieee_param *param;
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	/* RTW_INFO("%s\n", __func__); */

	/*
	* this function is expect to call in master mode, which allows no power saving
	* so, we just check hw_init_completed
	*/

	if (!rtw_is_hw_init_completed(adapt)) {
		ret = -EPERM;
		goto out;
	}


	/* if (p->length < sizeof(struct ieee_param) || !p->pointer){ */
	if (!p->pointer) {
		ret = -EINVAL;
		goto out;
	}

	param = (struct ieee_param *)rtw_malloc(p->length);
	if (!param) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, p->pointer, p->length)) {
		rtw_mfree((u8 *)param, p->length);
		ret = -EFAULT;
		goto out;
	}

	/* RTW_INFO("%s, cmd=%d\n", __func__, param->cmd); */

	switch (param->cmd) {
	case RTL871X_HOSTAPD_FLUSH:

		ret = rtw_hostapd_sta_flush(dev);

		break;

	case RTL871X_HOSTAPD_ADD_STA:

		ret = rtw_add_sta(dev, param);

		break;

	case RTL871X_HOSTAPD_REMOVE_STA:

		ret = rtw_del_sta(dev, param);

		break;

	case RTL871X_HOSTAPD_SET_BEACON:

		ret = rtw_set_beacon(dev, param, p->length);

		break;

	case RTL871X_SET_ENCRYPTION:

		ret = rtw_set_encryption(dev, param, p->length);

		break;

	case RTL871X_HOSTAPD_GET_WPAIE_STA:

		ret = rtw_get_sta_wpaie(dev, param);

		break;

	case RTL871X_HOSTAPD_SET_WPS_BEACON:

		ret = rtw_set_wps_beacon(dev, param, p->length);

		break;

	case RTL871X_HOSTAPD_SET_WPS_PROBE_RESP:

		ret = rtw_set_wps_probe_resp(dev, param, p->length);

		break;

	case RTL871X_HOSTAPD_SET_WPS_ASSOC_RESP:

		ret = rtw_set_wps_assoc_resp(dev, param, p->length);

		break;

	case RTL871X_HOSTAPD_SET_HIDDEN_SSID:

		ret = rtw_set_hidden_ssid(dev, param, p->length);

		break;

	case RTL871X_HOSTAPD_GET_INFO_STA:

		ret = rtw_ioctl_get_sta_data(dev, param, p->length);

		break;

#if CONFIG_RTW_MACADDR_ACL
	case RTL871X_HOSTAPD_SET_MACADDR_ACL:
		ret = rtw_ioctl_set_macaddr_acl(dev, param, p->length);
		break;
	case RTL871X_HOSTAPD_ACL_ADD_STA:
		ret = rtw_ioctl_acl_add_sta(dev, param, p->length);
		break;
	case RTL871X_HOSTAPD_ACL_REMOVE_STA:
		ret = rtw_ioctl_acl_remove_sta(dev, param, p->length);
		break;
#endif /* CONFIG_RTW_MACADDR_ACL */

	default:
		RTW_INFO("Unknown hostapd request: %d\n", param->cmd);
		ret = -EOPNOTSUPP;
		break;

	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;


	rtw_mfree((u8 *)param, p->length);

out:

	return ret;

}

#ifdef CONFIG_WIRELESS_EXT
static int rtw_wx_set_priv(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *awrq,
			   char *extra)
{

#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	char *ext_dbg;
#endif

	int ret = 0;
	int len = 0;
	char *ext;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_point *dwrq = (struct iw_point *)awrq;

	if (dwrq->length == 0)
		return -EFAULT;

	len = dwrq->length;
	ext = vmalloc(len);
	if (!ext)
		return -ENOMEM;

	if (copy_from_user(ext, dwrq->pointer, len)) {
		vfree(ext);
		return -EFAULT;
	}



#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	ext_dbg = vmalloc(len);
	if (!ext_dbg) {
		vfree(ext);
		return -ENOMEM;
	}

	memcpy(ext_dbg, ext, len);
#endif

	/* added for wps2.0 @20110524 */
	if (dwrq->flags == 0x8766 && len > 8) {
		u32 cp_sz;
		struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
		u8 *probereq_wpsie = ext;
		int probereq_wpsie_len = len;
		u8 wps_oui[4] = {0x0, 0x50, 0xf2, 0x04};

		if ((_VENDOR_SPECIFIC_IE_ == probereq_wpsie[0]) &&
		    (!memcmp(&probereq_wpsie[2], wps_oui, 4))) {
			cp_sz = probereq_wpsie_len > MAX_WPS_IE_LEN ? MAX_WPS_IE_LEN : probereq_wpsie_len;

			if (pmlmepriv->wps_probe_req_ie) {
				u32 free_len = pmlmepriv->wps_probe_req_ie_len;
				pmlmepriv->wps_probe_req_ie_len = 0;
				rtw_mfree(pmlmepriv->wps_probe_req_ie, free_len);
				pmlmepriv->wps_probe_req_ie = NULL;
			}

			pmlmepriv->wps_probe_req_ie = rtw_malloc(cp_sz);
			if (!pmlmepriv->wps_probe_req_ie) {
				printk("%s()-%d: rtw_malloc() ERROR!\n", __func__, __LINE__);
				ret =  -EINVAL;
				goto FREE_EXT;

			}

			memcpy(pmlmepriv->wps_probe_req_ie, probereq_wpsie, cp_sz);
			pmlmepriv->wps_probe_req_ie_len = cp_sz;

		}

		goto FREE_EXT;

	}

	if (len >= WEXT_CSCAN_HEADER_SIZE
		&& !memcmp(ext, WEXT_CSCAN_HEADER, WEXT_CSCAN_HEADER_SIZE)
	) {
		ret = rtw_wx_set_scan(dev, info, awrq, ext);
		goto FREE_EXT;
	}

FREE_EXT:

	vfree(ext);
	#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	vfree(ext_dbg);
	#endif

	return ret;

}
#endif

static int rtw_pm_set(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	unsigned	mode = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_INFO("[%s] extra = %s\n", __func__, extra);

	if (!memcmp(extra, "lps=", 4)) {
		sscanf(extra + 4, "%u", &mode);
		ret = rtw_pm_set_lps(adapt, mode);
	} else if (!memcmp(extra, "ips=", 4)) {
		sscanf(extra + 4, "%u", &mode);
		ret = rtw_pm_set_ips(adapt, mode);
	} else if (!memcmp(extra, "lps_level=", 10)) {
		if (sscanf(extra + 10, "%u", &mode) > 0)
			ret = rtw_pm_set_lps_level(adapt, mode);
	} else
		ret = -EINVAL;

	return ret;
}

static int rtw_mp_efuse_get(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wdata, char *extra)
{
	struct adapter * adapt = rtw_netdev_priv(dev);
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);
	struct efuse_hal * pEfuseHal;
	struct iw_point *wrqu;
	u8 ips_mode = IPS_NUM; /* init invalid value */
	u8 lps_mode = PS_MODE_NUM; /* init invalid value */
	struct pwrctrl_priv *pwrctrlpriv ;
	u8 *data = NULL;
	u8 *rawdata = NULL;
	char *pch, *ptmp, *token, *tmp[3] = {NULL, NULL, NULL};
	u16 i = 0, j = 0, mapLen = 0, addr = 0, cnts = 0;
	u16 max_available_len = 0, raw_cursize = 0, raw_maxsize = 0;
	u16 mask_len;
	u8 mask_buf[64] = "";
	int err;
	char *pextra = NULL;

	wrqu = (struct iw_point *)wdata;
	pwrctrlpriv = adapter_to_pwrctl(adapt);
	pEfuseHal = &pHalData->EfuseHal;

	err = 0;
	data = rtw_zmalloc(EFUSE_BT_MAX_MAP_LEN);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	rawdata = rtw_zmalloc(EFUSE_BT_MAX_MAP_LEN);
	if (!rawdata) {
		err = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(extra, wrqu->pointer, wrqu->length)) {
		err = -EFAULT;
		goto exit;
	}

	*(extra + wrqu->length) = '\0';

	lps_mode = pwrctrlpriv->power_mgnt;/* keep org value */
	rtw_pm_set_lps(adapt, PS_MODE_ACTIVE);

	ips_mode = pwrctrlpriv->ips_mode;/* keep org value */
	rtw_pm_set_ips(adapt, IPS_NONE);

	pch = extra;
	RTW_INFO("%s: in=%s\n", __func__, extra);

	i = 0;
	/* mac 16 "00e04c871200" rmap,00,2 */
	while ((token = strsep(&pch, ","))) {
		if (i > 2)
			break;
		tmp[i] = token;
		i++;
	}
	if (strcmp(tmp[0], "status") == 0) {
		sprintf(extra, "Load File efuse=%s,Load File MAC=%s"
			, pHalData->efuse_file_status == EFUSE_FILE_FAILED ? "FAIL" : "OK"
			, pHalData->macaddr_file_status == MACADDR_FILE_FAILED ? "FAIL" : "OK"
		       );
		goto exit;
	} else if (strcmp(tmp[0], "drvmap") == 0) {
		static u8 drvmaporder = 0;
		u8 *efuse;
		u32 shift, cnt;
		u32 blksz = 0x200; /* The size of one time show, default 512 */
		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, false);

		efuse = pHalData->efuse_eeprom_data;

		shift = blksz * drvmaporder;
		efuse += shift;
		cnt = mapLen - shift;

		if (cnt > blksz) {
			cnt = blksz;
			drvmaporder++;
		} else
			drvmaporder = 0;

		sprintf(extra, "\n");
		for (i = 0; i < cnt; i += 16) {
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%02x\t", shift + i);
			for (j = 0; j < 8; j++)
				pextra += sprintf(pextra, "%02X ", efuse[i + j]);
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++)
				pextra += sprintf(pextra, "%02X ", efuse[i + j]);
			pextra += sprintf(pextra, "\n");
		}
		if ((shift + cnt) < mapLen)
			pextra += sprintf(pextra, "\t...more (left:%d/%d)\n", mapLen-(shift + cnt), mapLen);

	} else if (strcmp(tmp[0], "realmap") == 0) {
		static u8 order = 0;
		u8 *efuse;
		u32 shift, cnt;
		u32 blksz = 0x200; /* The size of one time show, default 512 */

		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN , (void *)&mapLen, false);
		efuse = pEfuseHal->fakeEfuseInitMap;
		if (rtw_efuse_mask_map_read(adapt, 0, mapLen, efuse) == _FAIL) {
			RTW_INFO("%s: read realmap Fail!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}
		shift = blksz * order;
		efuse += shift;
		cnt = mapLen - shift;
		if (cnt > blksz) {
			cnt = blksz;
			order++;
		} else
			order = 0;

		sprintf(extra, "\n");
		for (i = 0; i < cnt; i += 16) {
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%02x\t", shift + i);
			for (j = 0; j < 8; j++)
				pextra += sprintf(pextra, "%02X ", efuse[i + j]);
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++)
				pextra += sprintf(pextra, "%02X ", efuse[i + j]);
			pextra += sprintf(pextra, "\n");
		}
		if ((shift + cnt) < mapLen)
			pextra += sprintf(pextra, "\t...more (left:%d/%d)\n", mapLen-(shift + cnt), mapLen);
	} else if (strcmp(tmp[0], "rmap") == 0) {
		if ((!tmp[1]) || (!tmp[2])) {
			RTW_INFO("%s: rmap Fail!! Parameters error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}

		/* rmap addr cnts */
		addr = simple_strtoul(tmp[1], &ptmp, 16);
		RTW_INFO("%s: addr=%x\n", __func__, addr);

		cnts = simple_strtoul(tmp[2], &ptmp, 10);
		if (cnts == 0) {
			RTW_INFO("%s: rmap Fail!! cnts error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		RTW_INFO("%s: cnts=%d\n", __func__, cnts);

		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN , (void *)&max_available_len, false);
		if ((addr + cnts) > max_available_len) {
			RTW_INFO("%s: addr(0x%X)+cnts(%d) parameter error!\n", __func__, addr, cnts);
			err = -EINVAL;
			goto exit;
		}

		if (rtw_efuse_mask_map_read(adapt, addr, cnts, data) == _FAIL) {
			RTW_INFO("%s: rtw_efuse_mask_map_read error!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		/*		RTW_INFO("%s: data={", __func__); */
		*extra = 0;
		pextra = extra;
		for (i = 0; i < cnts; i++) {
			/*			RTW_INFO("0x%02x ", data[i]); */
			pextra += sprintf(pextra, "0x%02X ", data[i]);
		}
		/*		RTW_INFO("}\n"); */
	} else if (strcmp(tmp[0], "realraw") == 0) {
		static u8 raw_order = 0;
		u32 shift, cnt;
		u32 blksz = 0x200; /* The size of one time show, default 512 */

		addr = 0;
		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_REAL_CONTENT_LEN , (void *)&mapLen, false);
		RTW_INFO("Real content len = %d\n",mapLen );

		if (rtw_efuse_access(adapt, false, addr, mapLen, rawdata) == _FAIL) {
			RTW_INFO("%s: rtw_efuse_access Fail!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		memset(extra, '\0', strlen(extra));

		shift = blksz * raw_order;
		rawdata += shift;
		cnt = mapLen - shift;
		if (cnt > blksz) {
			cnt = blksz;
			raw_order++;
		} else
			raw_order = 0;

		sprintf(extra, "\n");
		for (i = 0; i < cnt; i += 16) {
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%02x\t", shift + i);
			for (j = 0; j < 8; j++)
				pextra += sprintf(pextra, "%02X ", rawdata[i + j]);
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++)
				pextra += sprintf(pextra, "%02X ", rawdata[i + j]);
			pextra += sprintf(pextra, "\n");
		}
		if ((shift + cnt) < mapLen)
			pextra += sprintf(pextra, "\t...more (left:%d/%d)\n", mapLen-(shift + cnt), mapLen);

	} else if (strcmp(tmp[0], "btrealraw") == 0) {
		static u8 bt_raw_order = 0;
		u32 shift, cnt;
		u32 blksz = 0x200; /* The size of one time show, default 512 */

		addr = 0;
		EFUSE_GetEfuseDefinition(adapt, EFUSE_BT, TYPE_EFUSE_REAL_CONTENT_LEN, (void *)&mapLen, false);
		RTW_INFO("Real content len = %d\n", mapLen);
		rtw_write8(adapt, 0x35, 0x1);

		if (rtw_efuse_access(adapt, false, addr, mapLen, rawdata) == _FAIL) {
			RTW_INFO("%s: rtw_efuse_access Fail!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}
		memset(extra, '\0', strlen(extra));

		shift = blksz * bt_raw_order;
		rawdata += shift;
		cnt = mapLen - shift;
		if (cnt > blksz) {
			cnt = blksz;
			bt_raw_order++;
		} else
			bt_raw_order = 0;

		sprintf(extra, "\n");
		for (i = 0; i < cnt; i += 16) {
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%02x\t", shift + i);
			for (j = 0; j < 8; j++)
				pextra += sprintf(pextra, "%02X ", rawdata[i + j]);
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++)
				pextra += sprintf(pextra, "%02X ", rawdata[i + j]);
			pextra += sprintf(pextra, "\n");
		}
		if ((shift + cnt) < mapLen)
			pextra += sprintf(pextra, "\t...more (left:%d/%d)\n", mapLen-(shift + cnt), mapLen);

	} else if (strcmp(tmp[0], "mac") == 0) {
		if (hal_efuse_macaddr_offset(adapt) == -1) {
			err = -EFAULT;
			goto exit;
		}

		addr = hal_efuse_macaddr_offset(adapt);
		cnts = 6;

		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN, (void *)&max_available_len, false);
		if ((addr + cnts) > max_available_len) {
			RTW_INFO("%s: addr(0x%02x)+cnts(%d) parameter error!\n", __func__, addr, cnts);
			err = -EFAULT;
			goto exit;
		}

		if (rtw_efuse_mask_map_read(adapt, addr, cnts, data) == _FAIL) {
			RTW_INFO("%s: rtw_efuse_mask_map_read error!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		/*		RTW_INFO("%s: MAC address={", __func__); */
		*extra = 0;
		pextra = extra;
		for (i = 0; i < cnts; i++) {
			/*			RTW_INFO("%02X", data[i]); */
			pextra += sprintf(pextra, "%02X", data[i]);
			if (i != (cnts - 1)) {
				/*				RTW_INFO(":"); */
				pextra += sprintf(pextra, ":");
			}
		}
		/*		RTW_INFO("}\n"); */
	} else if (strcmp(tmp[0], "vidpid") == 0) {
		addr = EEPROM_VID_8723DU;

		cnts = 4;

		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN, (void *)&max_available_len, false);
		if ((addr + cnts) > max_available_len) {
			RTW_INFO("%s: addr(0x%02x)+cnts(%d) parameter error!\n", __func__, addr, cnts);
			err = -EFAULT;
			goto exit;
		}
		if (rtw_efuse_mask_map_read(adapt, addr, cnts, data) == _FAIL) {
			RTW_INFO("%s: rtw_efuse_access error!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		/*		RTW_INFO("%s: {VID,PID}={", __func__); */
		*extra = 0;
		pextra = extra;
		for (i = 0; i < cnts; i++) {
			/*			RTW_INFO("0x%02x", data[i]); */
			pextra += sprintf(pextra, "0x%02X", data[i]);
			if (i != (cnts - 1)) {
				/*				RTW_INFO(","); */
				pextra += sprintf(pextra, ",");
			}
		}
		/*		RTW_INFO("}\n"); */
	} else if (strcmp(tmp[0], "ableraw") == 0) {
		efuse_GetCurrentSize(adapt, &raw_cursize);
		raw_maxsize = efuse_GetMaxSize(adapt);
		sprintf(extra, "[available raw size]= %d bytes\n", raw_maxsize - raw_cursize);
	} else if (strcmp(tmp[0], "btableraw") == 0) {
		efuse_bt_GetCurrentSize(adapt, &raw_cursize);
		raw_maxsize = efuse_bt_GetMaxSize(adapt);
		sprintf(extra, "[available raw size]= %d bytes\n", raw_maxsize - raw_cursize);
	} else if (strcmp(tmp[0], "btfmap") == 0) {

		BTEfuse_PowerSwitch(adapt, 1, true);

		mapLen = EFUSE_BT_MAX_MAP_LEN;
		if (rtw_BT_efuse_map_read(adapt, 0, mapLen, pEfuseHal->BTEfuseInitMap) == _FAIL) {
			RTW_INFO("%s: rtw_BT_efuse_map_read Fail!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		/*		RTW_INFO("OFFSET\tVALUE(hex)\n"); */
		sprintf(extra, "\n");
		for (i = 0; i < 512; i += 16) { /* set 512 because the iwpriv's extra size have limit 0x7FF */
			/*			RTW_INFO("0x%03x\t", i); */
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%03x\t", i);
			for (j = 0; j < 8; j++) {
				/*				RTW_INFO("%02X ", pEfuseHal->BTEfuseInitMap[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->BTEfuseInitMap[i+j]);
			}
			/*			RTW_INFO("\t"); */
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++) {
				/*				RTW_INFO("%02X ", pEfuseHal->BTEfuseInitMap[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->BTEfuseInitMap[i+j]);
			}
			/*			RTW_INFO("\n"); */
			pextra += sprintf(pextra, "\n");
		}
		/*		RTW_INFO("\n"); */
	} else if (strcmp(tmp[0], "btbmap") == 0) {
		BTEfuse_PowerSwitch(adapt, 1, true);

		mapLen = EFUSE_BT_MAX_MAP_LEN;
		if (rtw_BT_efuse_map_read(adapt, 0, mapLen, pEfuseHal->BTEfuseInitMap) == _FAIL) {
			RTW_INFO("%s: rtw_BT_efuse_map_read Fail!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		/*		RTW_INFO("OFFSET\tVALUE(hex)\n"); */
		sprintf(extra, "\n");
		for (i = 512; i < 1024 ; i += 16) {
			/*			RTW_INFO("0x%03x\t", i); */
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%03x\t", i);
			for (j = 0; j < 8; j++) {
				/*				RTW_INFO("%02X ", data[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->BTEfuseInitMap[i+j]);
			}
			/*			RTW_INFO("\t"); */
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++) {
				/*				RTW_INFO("%02X ", data[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->BTEfuseInitMap[i+j]);
			}
			/*			RTW_INFO("\n"); */
			pextra += sprintf(pextra, "\n");
		}
		/*		RTW_INFO("\n"); */
	} else if (strcmp(tmp[0], "btrmap") == 0) {
		u8 BTStatus;

		rtw_write8(adapt, 0xa3, 0x05); /* For 8723AB ,8821S ? */
		BTStatus = rtw_read8(adapt, 0xa0);

		RTW_INFO("%s: Check 0xa0 BT Status =0x%x\n", __func__, BTStatus);
		if (BTStatus != 0x04) {
			sprintf(extra, "BT Status not Active ,can't to read BT eFuse\n");
			goto exit;
		}

		if ((!tmp[1]) || (!tmp[2])) {
			err = -EINVAL;
			goto exit;
		}

		BTEfuse_PowerSwitch(adapt, 1, true);

		/* rmap addr cnts */
		addr = simple_strtoul(tmp[1], &ptmp, 16);
		RTW_INFO("%s: addr=0x%X\n", __func__, addr);

		cnts = simple_strtoul(tmp[2], &ptmp, 10);
		if (cnts == 0) {
			RTW_INFO("%s: btrmap Fail!! cnts error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		RTW_INFO("%s: cnts=%d\n", __func__, cnts);
		EFUSE_GetEfuseDefinition(adapt, EFUSE_BT, TYPE_EFUSE_MAP_LEN, (void *)&max_available_len, false);
		if ((addr + cnts) > max_available_len) {
			RTW_INFO("%s: addr(0x%X)+cnts(%d) parameter error!\n", __func__, addr, cnts);
			err = -EFAULT;
			goto exit;
		}
		if (rtw_BT_efuse_map_read(adapt, addr, cnts, data) == _FAIL) {
			RTW_INFO("%s: rtw_BT_efuse_map_read error!!\n", __func__);
			err = -EFAULT;
			goto exit;
		}

		*extra = 0;
		pextra = extra;
		/*		RTW_INFO("%s: bt efuse data={", __func__); */
		for (i = 0; i < cnts; i++) {
			/*			RTW_INFO("0x%02x ", data[i]); */
			pextra += sprintf(pextra, " 0x%02X ", data[i]);
		}
		/*		RTW_INFO("}\n"); */
		RTW_INFO(FUNC_ADPT_FMT ": BT MAC=[%s]\n", FUNC_ADPT_ARG(adapt), extra);
	} else if (strcmp(tmp[0], "btffake") == 0) {
		/*		RTW_INFO("OFFSET\tVALUE(hex)\n"); */
		sprintf(extra, "\n");
		for (i = 0; i < 512; i += 16) {
			/*			RTW_INFO("0x%03x\t", i); */
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%03x\t", i);
			for (j = 0; j < 8; j++) {
				/*				RTW_INFO("%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]);
			}
			/*			RTW_INFO("\t"); */
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++) {
				/*				RTW_INFO("%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]);
			}
			/*			RTW_INFO("\n"); */
			pextra += sprintf(pextra, "\n");
		}
		/*		RTW_INFO("\n"); */
	} else if (strcmp(tmp[0], "btbfake") == 0) {
		/*		RTW_INFO("OFFSET\tVALUE(hex)\n"); */
		sprintf(extra, "\n");
		for (i = 512; i < 1024; i += 16) {
			/*			RTW_INFO("0x%03x\t", i); */
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%03x\t", i);
			for (j = 0; j < 8; j++) {
				/*				RTW_INFO("%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]);
			}
			/*			RTW_INFO("\t"); */
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++) {
				/*				RTW_INFO("%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]); */
				pextra += sprintf(pextra, "%02X ", pEfuseHal->fakeBTEfuseModifiedMap[i+j]);
			}
			/*			RTW_INFO("\n"); */
			pextra += sprintf(pextra, "\n");
		}
		/*		RTW_INFO("\n"); */
	} else if (strcmp(tmp[0], "wlrfkmap") == 0) {
		static u8 fk_order = 0;
		u8 *efuse;
		u32 shift, cnt;
		u32 blksz = 0x200; /* The size of one time show, default 512 */

		EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN , (void *)&mapLen, false);
		efuse = pEfuseHal->fakeEfuseModifiedMap;

		shift = blksz * fk_order;
		efuse += shift;
		cnt = mapLen - shift;
		if (cnt > blksz) {
			cnt = blksz;
			fk_order++;
		} else
			fk_order = 0;

		sprintf(extra, "\n");
		for (i = 0; i < cnt; i += 16) {
			pextra = extra + strlen(extra);
			pextra += sprintf(pextra, "0x%02x\t", shift + i);
			for (j = 0; j < 8; j++)
				pextra += sprintf(pextra, "%02X ", efuse[i + j]);
			pextra += sprintf(pextra, "\t");
			for (; j < 16; j++)
				pextra += sprintf(pextra, "%02X ", efuse[i + j]);
			pextra += sprintf(pextra, "\n");
		}
		if ((shift + cnt) < mapLen)
			pextra += sprintf(pextra, "\t...more\n");

	} else if (strcmp(tmp[0], "wlrfkrmap") == 0) {
		if ((!tmp[1]) || (!tmp[2])) {
			RTW_INFO("%s: rmap Fail!! Parameters error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		/* rmap addr cnts */
		addr = simple_strtoul(tmp[1], &ptmp, 16);
		RTW_INFO("%s: addr=%x\n", __func__, addr);

		cnts = simple_strtoul(tmp[2], &ptmp, 10);
		if (cnts == 0) {
			RTW_INFO("%s: rmap Fail!! cnts error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		RTW_INFO("%s: cnts=%d\n", __func__, cnts);

		/*		RTW_INFO("%s: data={", __func__); */
		*extra = 0;
		pextra = extra;
		for (i = 0; i < cnts; i++) {
			RTW_INFO("wlrfkrmap = 0x%02x\n", pEfuseHal->fakeEfuseModifiedMap[addr + i]);
			pextra += sprintf(pextra, "0x%02X ", pEfuseHal->fakeEfuseModifiedMap[addr+i]);
		}
	} else if (strcmp(tmp[0], "btrfkrmap") == 0) {
		if ((!tmp[1]) || (!tmp[2])) {
			RTW_INFO("%s: rmap Fail!! Parameters error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		/* rmap addr cnts */
		addr = simple_strtoul(tmp[1], &ptmp, 16);
		RTW_INFO("%s: addr=%x\n", __func__, addr);

		cnts = simple_strtoul(tmp[2], &ptmp, 10);
		if (cnts == 0) {
			RTW_INFO("%s: rmap Fail!! cnts error!\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		RTW_INFO("%s: cnts=%d\n", __func__, cnts);

		/*		RTW_INFO("%s: data={", __func__); */
		*extra = 0;
		pextra = extra;
		for (i = 0; i < cnts; i++) {
			RTW_INFO("wlrfkrmap = 0x%02x\n", pEfuseHal->fakeBTEfuseModifiedMap[addr + i]);
			pextra += sprintf(pextra, "0x%02X ", pEfuseHal->fakeBTEfuseModifiedMap[addr+i]);
		}
	} else if (strcmp(tmp[0], "mask") == 0) {
		*extra = 0;
		mask_len = sizeof(u8) * rtw_get_efuse_mask_arraylen(adapt);
		rtw_efuse_mask_array(adapt, mask_buf);

		if (adapt->registrypriv.bFileMaskEfuse)
			memcpy(mask_buf, maskfileBuffer, mask_len);

		sprintf(extra, "\n");
		pextra = extra + strlen(extra);
		for (i = 0; i < mask_len; i++)
			pextra += sprintf(pextra, "0x%02X\n", mask_buf[i]);

	} else
		sprintf(extra, "Command not found!");

exit:
	if (data)
		rtw_mfree(data, EFUSE_BT_MAX_MAP_LEN);
	if (rawdata)
		rtw_mfree(rawdata, EFUSE_BT_MAX_MAP_LEN);
	if (!err)
		wrqu->length = strlen(extra);

	if (adapt->registrypriv.mp_mode == 0) {
		rtw_pm_set_ips(adapt, ips_mode);

		rtw_pm_set_lps(adapt, lps_mode);
	}
	return err;
}

static int rtw_priv_set(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wdata, char *extra)
{
	struct iw_point *wrqu = (struct iw_point *)wdata;
	u32 subcmd = wrqu->flags;
	struct adapter * adapt = rtw_netdev_priv(dev);

	if (!adapt)
		return -ENETDOWN;

	if (!adapt->bup) {
		RTW_INFO(" %s fail =>(adapt->bup == false )\n", __func__);
		return -ENETDOWN;
	}

	if (RTW_CANNOT_RUN(adapt)) {
		RTW_INFO("%s fail =>(bSurpriseRemoved) || ( bDriverStopped)\n", __func__);
		return -ENETDOWN;
	}

	if (!extra) {
		wrqu->length = 0;
		return -EIO;
	}

	if (subcmd < MP_NULL) {
		return 0;
	}
	return -EIO;
}


static int rtw_priv_get(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wdata, char *extra)
{
	struct iw_point *wrqu = (struct iw_point *)wdata;
	u32 subcmd = wrqu->flags;
	struct adapter * adapt = rtw_netdev_priv(dev);


	if (!adapt)
		return -ENETDOWN;

	if (!adapt->bup) {
		RTW_INFO(" %s fail =>(adapt->bup == false )\n", __func__);
		return -ENETDOWN;
	}

	if (RTW_CANNOT_RUN(adapt)) {
		RTW_INFO("%s fail =>(adapt->bSurpriseRemoved) || ( adapt->bDriverStopped)\n", __func__);
		return -ENETDOWN;
	}

	if (!extra) {
		wrqu->length = 0;
		return -EIO;
	}

	if (subcmd < MP_NULL)
		return 0;
	return -EIO;
}

static int rtw_tdls(struct net_device *dev,
		    struct iw_request_info *info,
		    union iwreq_data *wrqu, char *extra)
{
	int ret = 0;

	return ret;
}


static int rtw_tdls_get(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret = 0;

	return ret;
}

#ifdef CONFIG_INTEL_WIDI
static int rtw_widi_set(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	process_intel_widi_cmd(adapt, extra);

	return ret;
}

static int rtw_widi_set_probe_request(struct net_device *dev,
				      struct iw_request_info *info,
				      union iwreq_data *wrqu, char *extra)
{
	int	ret = 0;
	u8	*pbuf = NULL;
	struct adapter	*adapt = (struct adapter *)rtw_netdev_priv(dev);

	pbuf = rtw_malloc(sizeof(l2_msg_t));
	if (pbuf) {
		if (copy_from_user(pbuf, wrqu->data.pointer, wrqu->data.length))
			ret = -EFAULT;
		/* memcpy(pbuf, wrqu->data.pointer, wrqu->data.length); */

		if (wrqu->data.flags == 0)
			intel_widi_wk_cmd(adapt, INTEL_WIDI_ISSUE_PROB_WK, pbuf, sizeof(l2_msg_t));
		else if (wrqu->data.flags == 1)
			rtw_set_wfd_rds_sink_info(adapt, (l2_msg_t *)pbuf);
	}
	return ret;
}
#endif /* CONFIG_INTEL_WIDI */

#ifdef CONFIG_MAC_LOOPBACK_DRIVER

/* extern void rtl8723d_cal_txdesc_chksum(struct tx_desc *ptxdesc); */
#define cal_txdesc_chksum rtl8723d_cal_txdesc_chksum
/* extern void rtl8723d_fill_default_txdesc(struct xmit_frame *pxmitframe, u8 *pbuf); */
#define fill_default_txdesc rtl8723d_fill_default_txdesc

static int initLoopback(struct adapter * adapt)
{
	struct loopbackdata * ploopback;


	if (!adapt->ploopback) {
		ploopback = (struct loopbackdata *)rtw_zmalloc(sizeof(struct loopbackdata));
		if (!ploopback)
			return -ENOMEM;

		sema_init(&ploopback->sema, 0);
		ploopback->bstop = true;
		ploopback->cnt = 0;
		ploopback->size = 300;
		memset(ploopback->msg, 0, sizeof(ploopback->msg));

		adapt->ploopback = ploopback;
	}

	return 0;
}

static void freeLoopback(struct adapter * adapt)
{
	struct loopbackdata * ploopback;


	ploopback = adapt->ploopback;
	if (ploopback) {
		rtw_mfree((u8 *)ploopback, sizeof(struct loopbackdata));
		adapt->ploopback = NULL;
	}
}

static int initpseudoadhoc(struct adapter * adapt)
{
	enum ndis_802_11_network_infrastructure networkType;
	int err;

	networkType = Ndis802_11IBSS;
	err = rtw_set_802_11_infrastructure_mode(adapt, networkType);
	if (!err)
		return _FAIL;

	err = rtw_setopmode_cmd(adapt, networkType, RTW_CMDF_WAIT_ACK);
	if (err == _FAIL)
		return _FAIL;

	return _SUCCESS;
}

static int createpseudoadhoc(struct adapter * adapt)
{
	enum ndis_802_11_authentication_mode authmode;
	struct mlme_priv *pmlmepriv;
	struct ndis_802_11_ssid *passoc_ssid;
	struct wlan_bssid_ex *pdev_network;
	u8 *pibss;
	u8 ssid[] = "pseduo_ad-hoc";
	int err;
	unsigned long irqL;


	pmlmepriv = &adapt->mlmepriv;

	authmode = Ndis802_11AuthModeOpen;
	err = rtw_set_802_11_authentication_mode(adapt, authmode);
	if (!err)
		return _FAIL;

	passoc_ssid = &pmlmepriv->assoc_ssid;
	memset(passoc_ssid, 0, sizeof(struct ndis_802_11_ssid));
	passoc_ssid->SsidLength = sizeof(ssid) - 1;
	memcpy(passoc_ssid->Ssid, ssid, passoc_ssid->SsidLength);

	pdev_network = &adapt->registrypriv.dev_network;
	pibss = adapt->registrypriv.dev_network.MacAddress;
	memcpy(&pdev_network->Ssid, passoc_ssid, sizeof(struct ndis_802_11_ssid));

	rtw_update_registrypriv_dev_network(adapt);
	rtw_generate_random_ibss(pibss);

	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	/*pmlmepriv->fw_state = WIFI_ADHOC_MASTER_STATE;*/
	init_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE);

	_exit_critical_bh(&pmlmepriv->lock, &irqL);

	{
		struct wlan_network *pcur_network;
		struct sta_info *psta;

		/* 3  create a new psta */
		pcur_network = &pmlmepriv->cur_network;

		/* clear psta in the cur_network, if any */
		psta = rtw_get_stainfo(&adapt->stapriv, pcur_network->network.MacAddress);
		if (psta)
			rtw_free_stainfo(adapt, psta);

		psta = rtw_alloc_stainfo(&adapt->stapriv, pibss);
		if (!psta)
			return _FAIL;

		/* 3  join psudo AdHoc */
		pcur_network->join_res = 1;
		pcur_network->aid = psta->cmn.aid = 1;
		memcpy(&pcur_network->network, pdev_network, get_wlan_bssid_ex_sz(pdev_network));

		/* set msr to WIFI_FW_ADHOC_STATE */
		adapt->hw_port = HW_PORT0;
		Set_MSR(adapt, WIFI_FW_ADHOC_STATE);

	}

	return _SUCCESS;
}

static struct xmit_frame *createloopbackpkt(struct adapter * adapt, u32 size)
{
	struct xmit_priv *pxmitpriv;
	struct xmit_frame *pframe;
	struct xmit_buf *pxmitbuf;
	struct pkt_attrib *pattrib;
	struct tx_desc *desc;
	u8 *pkt_start, *pkt_end, *ptr;
	struct rtw_ieee80211_hdr *hdr;
	int bmcast;
	unsigned long irqL;


	if ((TXDESC_SIZE + WLANHDR_OFFSET + size) > MAX_XMITBUF_SZ)
		return NULL;

	pxmitpriv = &adapt->xmitpriv;
	pframe = NULL;

	/* 2 1. allocate xmit frame */
	pframe = rtw_alloc_xmitframe(pxmitpriv);
	if (!pframe)
		return NULL;
	pframe->adapt = adapt;

	/* 2 2. allocate xmit buffer */
	_enter_critical_bh(&pxmitpriv->lock, &irqL);
	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	_exit_critical_bh(&pxmitpriv->lock, &irqL);
	if (!pxmitbuf) {
		rtw_free_xmitframe(pxmitpriv, pframe);
		return NULL;
	}

	pframe->pxmitbuf = pxmitbuf;
	pframe->buf_addr = pxmitbuf->pbuf;
	pxmitbuf->priv_data = pframe;

	/* 2 3. update_attrib() */
	pattrib = &pframe->attrib;

	/* init xmitframe attribute */
	memset(pattrib, 0, sizeof(struct pkt_attrib));

	pattrib->ether_type = 0x8723;
	memcpy(pattrib->src, adapter_mac_addr(adapt), ETH_ALEN);
	memcpy(pattrib->ta, pattrib->src, ETH_ALEN);
	memset(pattrib->dst, 0xFF, ETH_ALEN);
	memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);

	pattrib->ack_policy = 0;
	pattrib->hdrlen = WLAN_HDR_A3_LEN;
	pattrib->subtype = WIFI_DATA;
	pattrib->priority = 0;
	pattrib->qsel = pattrib->priority;
	pattrib->nr_frags = 1;
	pattrib->encrypt = 0;
	pattrib->bswenc = false;
	pattrib->qos_en = false;

	bmcast = IS_MCAST(pattrib->ra);
	if (bmcast)
		pattrib->psta = rtw_get_bcmc_stainfo(adapt);
	else
		pattrib->psta = rtw_get_stainfo(&adapt->stapriv, get_bssid(&adapt->mlmepriv));

	pattrib->mac_id = pattrib->psta->cmn.mac_id;
	pattrib->pktlen = size;
	pattrib->last_txcmdsz = pattrib->hdrlen + pattrib->pktlen;

	/* 2 4. fill TX descriptor */
	desc = (struct tx_desc *)pframe->buf_addr;
	memset(desc, 0, TXDESC_SIZE);

	fill_default_txdesc(pframe, (u8 *)desc);

	/* Hw set sequence number */
	((PTXDESC)desc)->hwseq_en = 0; /* HWSEQ_EN, 0:disable, 1:enable
 * ((PTXDESC)desc)->hwseq_sel = 0;  */ /* HWSEQ_SEL */

	((PTXDESC)desc)->disdatafb = 1;

	/* convert to little endian */
	desc->txdw0 = cpu_to_le32(desc->txdw0);
	desc->txdw1 = cpu_to_le32(desc->txdw1);
	desc->txdw2 = cpu_to_le32(desc->txdw2);
	desc->txdw3 = cpu_to_le32(desc->txdw3);
	desc->txdw4 = cpu_to_le32(desc->txdw4);
	desc->txdw5 = cpu_to_le32(desc->txdw5);
	desc->txdw6 = cpu_to_le32(desc->txdw6);
	desc->txdw7 = cpu_to_le32(desc->txdw7);
	desc->txdw8 = cpu_to_le32(desc->txdw8);
	desc->txdw9 = cpu_to_le32(desc->txdw9);
	desc->txdw10 = cpu_to_le32(desc->txdw10);
	desc->txdw11 = cpu_to_le32(desc->txdw11);
	desc->txdw12 = cpu_to_le32(desc->txdw12);
	desc->txdw13 = cpu_to_le32(desc->txdw13);
	desc->txdw14 = cpu_to_le32(desc->txdw14);
	desc->txdw15 = cpu_to_le32(desc->txdw15);

	cal_txdesc_chksum(desc);

	/* 2 5. coalesce */
	pkt_start = pframe->buf_addr + TXDESC_SIZE;
	pkt_end = pkt_start + pattrib->last_txcmdsz;

	/* 3 5.1. make wlan header, make_wlanhdr() */
	hdr = (struct rtw_ieee80211_hdr *)pkt_start;
	set_frame_sub_type(&hdr->frame_ctl, pattrib->subtype);
	memcpy(hdr->addr1, pattrib->dst, ETH_ALEN); /* DA */
	memcpy(hdr->addr2, pattrib->src, ETH_ALEN); /* SA */
	memcpy(hdr->addr3, get_bssid(&adapt->mlmepriv), ETH_ALEN); /* RA, BSSID */

	/* 3 5.2. make payload */
	ptr = pkt_start + pattrib->hdrlen;
	get_random_bytes(ptr, pkt_end - ptr);

	pxmitbuf->len = TXDESC_SIZE + pattrib->last_txcmdsz;
	pxmitbuf->ptail += pxmitbuf->len;

	return pframe;
}

static void freeloopbackpkt(struct adapter * adapt, struct xmit_frame *pframe)
{
	struct xmit_priv *pxmitpriv;
	struct xmit_buf *pxmitbuf;


	pxmitpriv = &adapt->xmitpriv;
	pxmitbuf = pframe->pxmitbuf;

	rtw_free_xmitframe(pxmitpriv, pframe);
	rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
}

static void printdata(u8 *pbuf, u32 len)
{
	u32 i, val;


	for (i = 0; (i + 4) <= len; i += 4) {
		printk("%08X", *(u32 *)(pbuf + i));
		if ((i + 4) & 0x1F)
			printk(" ");
		else
			printk("\n");
	}

	if (i < len) {
#if __BIG_ENDIAN
		for (; i < len, i++)
			printk("%02X", pbuf + i);
#else /* __LITTLE_ENDIAN */
		u8 str[9];
		u8 n;
		val = 0;
		n = len - i;
		memcpy(&val, pbuf + i, n);
		sprintf(str, "%08X", val);
		n = (4 - n) * 2;
		printk("%8s", str + n);
#endif
	}
	printk("\n");
}

static u8 pktcmp(struct adapter * adapt, u8 *txbuf, u32 txsz, u8 *rxbuf, u32 rxsz)
{
	struct hal_com_data * phal;
	struct recv_stat *prxstat;
	struct recv_stat report;
	PRXREPORT prxreport;
	u32 drvinfosize;
	u32 rxpktsize;
	u8 fcssize;
	u8 ret = false;

	prxstat = (struct recv_stat *)rxbuf;
	report.rxdw0 = le32_to_cpu(prxstat->rxdw0);
	report.rxdw1 = le32_to_cpu(prxstat->rxdw1);
	report.rxdw2 = le32_to_cpu(prxstat->rxdw2);
	report.rxdw3 = le32_to_cpu(prxstat->rxdw3);
	report.rxdw4 = le32_to_cpu(prxstat->rxdw4);
	report.rxdw5 = le32_to_cpu(prxstat->rxdw5);

	prxreport = (PRXREPORT)&report;
	drvinfosize = prxreport->drvinfosize << 3;
	rxpktsize = prxreport->pktlen;

	phal = GET_HAL_DATA(adapt);
	if (rtw_hal_rcr_check(adapt, RCR_APPFCS))
		fcssize = IEEE80211_FCS_LEN;
	else
		fcssize = 0;

	if ((txsz - TXDESC_SIZE) != (rxpktsize - fcssize)) {
		RTW_INFO("%s: ERROR! size not match tx/rx=%d/%d !\n",
			 __func__, txsz - TXDESC_SIZE, rxpktsize - fcssize);
		ret = false;
	} else {
		ret = !memcmp(txbuf + TXDESC_SIZE, \
				  rxbuf + RXDESC_SIZE + drvinfosize, \
				  txsz - TXDESC_SIZE);
		if (!ret)
			RTW_INFO("%s: ERROR! pkt content mismatch!\n", __func__);
	}

	if (!ret) {
		RTW_INFO("\n%s: TX PKT total=%d, desc=%d, content=%d\n",
			 __func__, txsz, TXDESC_SIZE, txsz - TXDESC_SIZE);
		RTW_INFO("%s: TX DESC size=%d\n", __func__, TXDESC_SIZE);
		printdata(txbuf, TXDESC_SIZE);
		RTW_INFO("%s: TX content size=%d\n", __func__, txsz - TXDESC_SIZE);
		printdata(txbuf + TXDESC_SIZE, txsz - TXDESC_SIZE);

		RTW_INFO("\n%s: RX PKT read=%d offset=%d(%d,%d) content=%d\n",
			__func__, rxsz, RXDESC_SIZE + drvinfosize, RXDESC_SIZE, drvinfosize, rxpktsize);
		if (rxpktsize != 0) {
			RTW_INFO("%s: RX DESC size=%d\n", __func__, RXDESC_SIZE);
			printdata(rxbuf, RXDESC_SIZE);
			RTW_INFO("%s: RX drvinfo size=%d\n", __func__, drvinfosize);
			printdata(rxbuf + RXDESC_SIZE, drvinfosize);
			RTW_INFO("%s: RX content size=%d\n", __func__, rxpktsize);
			printdata(rxbuf + RXDESC_SIZE + drvinfosize, rxpktsize);
		} else {
			RTW_INFO("%s: RX data size=%d\n", __func__, rxsz);
			printdata(rxbuf, rxsz);
		}
	}

	return ret;
}

int lbk_thread(void *context)
{
	int err;
	struct adapter * adapt;
	struct loopbackdata * ploopback;
	struct xmit_frame *pxmitframe;
	u32 cnt, ok, fail, headerlen;
	u32 pktsize;
	u32 ff_hwaddr;


	adapt = (struct adapter *)context;
	ploopback = adapt->ploopback;
	if (!ploopback)
		return -1;
	cnt = 0;
	ok = 0;
	fail = 0;

	daemonize("%s", "RTW_LBK_THREAD");
	allow_signal(SIGTERM);

	do {
		if (ploopback->size == 0) {
			get_random_bytes(&pktsize, 4);
			pktsize = (pktsize % 1535) + 1; /* 1~1535 */
		} else
			pktsize = ploopback->size;

		pxmitframe = createloopbackpkt(adapt, pktsize);
		if (!pxmitframe) {
			sprintf(ploopback->msg, "loopback FAIL! 3. create Packet FAIL!");
			break;
		}

		ploopback->txsize = TXDESC_SIZE + pxmitframe->attrib.last_txcmdsz;
		memcpy(ploopback->txbuf, pxmitframe->buf_addr, ploopback->txsize);
		ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);
		cnt++;
		RTW_INFO("%s: wirte port cnt=%d size=%d\n", __func__, cnt, ploopback->txsize);
		pxmitframe->pxmitbuf->pdata = ploopback->txbuf;
		rtw_write_port(adapt, ff_hwaddr, ploopback->txsize, (u8 *)pxmitframe->pxmitbuf);

		/* wait for rx pkt */
		_rtw_down_sema(&ploopback->sema);

		err = pktcmp(adapt, ploopback->txbuf, ploopback->txsize, ploopback->rxbuf, ploopback->rxsize);
		if (err)
			ok++;
		else
			fail++;

		ploopback->txsize = 0;
		memset(ploopback->txbuf, 0, 0x8000);
		ploopback->rxsize = 0;
		memset(ploopback->rxbuf, 0, 0x8000);

		freeloopbackpkt(adapt, pxmitframe);
		pxmitframe = NULL;

		flush_signals_thread();

		if ((ploopback->bstop) ||
		    ((ploopback->cnt != 0) && (ploopback->cnt == cnt))) {
			u32 ok_rate, fail_rate, all;
			all = cnt;
			ok_rate = (ok * 100) / all;
			fail_rate = (fail * 100) / all;
			sprintf(ploopback->msg, \
				"loopback result: ok=%d%%(%d/%d),error=%d%%(%d/%d)", \
				ok_rate, ok, all, fail_rate, fail, all);
			break;
		}
	} while (1);

	ploopback->bstop = true;

	thread_exit(NULL);
	return 0;
}

static void loopbackTest(struct adapter * adapt, u32 cnt, u32 size, u8 *pmsg)
{
	struct loopbackdata * ploopback;
	u32 len;
	int err;


	ploopback = adapt->ploopback;

	if (ploopback) {
		if (!ploopback->bstop) {
			ploopback->bstop = true;
			up(&ploopback->sema);
		}
		len = 0;
		do {
			len = strlen(ploopback->msg);
			if (len)
				break;
			rtw_msleep_os(1);
		} while (1);
		memcpy(pmsg, ploopback->msg, len + 1);
		freeLoopback(adapt);

		return;
	}

	/* disable dynamic algorithm	 */
	rtw_phydm_ability_backup(adapt);
	rtw_phydm_func_disable_all(adapt);

	/* create pseudo ad-hoc connection */
	err = initpseudoadhoc(adapt);
	if (err == _FAIL) {
		sprintf(pmsg, "loopback FAIL! 1.1 init ad-hoc FAIL!");
		return;
	}

	err = createpseudoadhoc(adapt);
	if (err == _FAIL) {
		sprintf(pmsg, "loopback FAIL! 1.2 create ad-hoc master FAIL!");
		return;
	}

	err = initLoopback(adapt);
	if (err) {
		sprintf(pmsg, "loopback FAIL! 2. init FAIL! error code=%d", err);
		return;
	}

	ploopback = adapt->ploopback;

	ploopback->bstop = false;
	ploopback->cnt = cnt;
	ploopback->size = size;
	ploopback->lbkthread = kthread_run(lbk_thread, adapt, "RTW_LBK_THREAD");
	if (IS_ERR(adapt->lbkthread)) {
		freeLoopback(adapt);
		ploopback->lbkthread = NULL;
		sprintf(pmsg, "loopback start FAIL! cnt=%d", cnt);
		return;
	}

	sprintf(pmsg, "loopback start! cnt=%d", cnt);
}
#endif /* CONFIG_MAC_LOOPBACK_DRIVER */

static int rtw_test(
	struct net_device *dev,
	struct iw_request_info *info,
	union iwreq_data *wrqu, char *extra)
{
	u32 len;
	u8 *pbuf, *pch;
	char *ptmp;
	u8 *delim = ",";
	struct adapter * adapt = rtw_netdev_priv(dev);


	RTW_INFO("+%s\n", __func__);
	len = wrqu->data.length;

	pbuf = (u8 *)rtw_zmalloc(len + 1);
	if (!pbuf) {
		RTW_INFO("%s: no memory!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(pbuf, wrqu->data.pointer, len)) {
		rtw_mfree(pbuf, len + 1);
		RTW_INFO("%s: copy from user fail!\n", __func__);
		return -EFAULT;
	}

	pbuf[len] = '\0';
	
	RTW_INFO("%s: string=\"%s\"\n", __func__, pbuf);

	ptmp = (char *)pbuf;
	pch = strsep(&ptmp, delim);
	if ((!pch) || (strlen(pch) == 0)) {
		rtw_mfree(pbuf, len);
		RTW_INFO("%s: parameter error(level 1)!\n", __func__);
		return -EFAULT;
	}

#ifdef CONFIG_MAC_LOOPBACK_DRIVER
	if (strcmp(pch, "loopback") == 0) {
		int cnt = 0;
		u32 size = 64;

		pch = strsep(&ptmp, delim);
		if ((!pch) || (strlen(pch) == 0)) {
			rtw_mfree(pbuf, len);
			RTW_INFO("%s: parameter error(level 2)!\n", __func__);
			return -EFAULT;
		}

		sscanf(pch, "%d", &cnt);
		RTW_INFO("%s: loopback cnt=%d\n", __func__, cnt);

		pch = strsep(&ptmp, delim);
		if ((!pch) || (strlen(pch) == 0)) {
			rtw_mfree(pbuf, len);
			RTW_INFO("%s: parameter error(level 2)!\n", __func__);
			return -EFAULT;
		}

		sscanf(pch, "%d", &size);
		RTW_INFO("%s: loopback size=%d\n", __func__, size);

		loopbackTest(adapt, cnt, size, extra);
		wrqu->data.length = strlen(extra) + 1;

		goto free_buf;
	}
#endif

	if (strcmp(pch, "bton") == 0) {
		rtw_btcoex_SetManualControl(adapt, false);
		goto free_buf;
	} else if (strcmp(pch, "btoff") == 0) {
		rtw_btcoex_SetManualControl(adapt, true);
		goto free_buf;
	}

	if (strcmp(pch, "h2c") == 0) {
		u8 param[8];
		u8 count = 0;
		u32 tmp;
		u8 i;
		u32 pos;
		u8 ret;

		do {
			pch = strsep(&ptmp, delim);
			if ((!pch) || (strlen(pch) == 0))
				break;

			sscanf(pch, "%x", &tmp);
			param[count++] = (u8)tmp;
		} while (count < 8);

		if (count == 0) {
			rtw_mfree(pbuf, len);
			RTW_INFO("%s: parameter error(level 2)!\n", __func__);
			return -EFAULT;
		}

		ret = rtw_test_h2c_cmd(adapt, param, count);

		pos = sprintf(extra, "H2C ID=0x%02x content=", param[0]);
		for (i = 1; i < count; i++)
			pos += sprintf(extra + pos, "%02x,", param[i]);
		extra[pos] = 0;
		pos--;
		pos += sprintf(extra + pos, " %s", ret == _FAIL ? "FAIL" : "OK");

		wrqu->data.length = strlen(extra) + 1;

		goto free_buf;
	}

free_buf:
	rtw_mfree(pbuf, len);
	return 0;
}

#ifdef CONFIG_WIRELESS_EXT
static iw_handler rtw_handlers[] = {
	NULL,					/* SIOCSIWCOMMIT */
	rtw_wx_get_name,		/* SIOCGIWNAME */
	dummy,					/* SIOCSIWNWID */
	dummy,					/* SIOCGIWNWID */
	rtw_wx_set_freq,		/* SIOCSIWFREQ */
	rtw_wx_get_freq,		/* SIOCGIWFREQ */
	rtw_wx_set_mode,		/* SIOCSIWMODE */
	rtw_wx_get_mode,		/* SIOCGIWMODE */
	dummy,					/* SIOCSIWSENS */
	rtw_wx_get_sens,		/* SIOCGIWSENS */
	NULL,					/* SIOCSIWRANGE */
	rtw_wx_get_range,		/* SIOCGIWRANGE */
	rtw_wx_set_priv,		/* SIOCSIWPRIV */
	NULL,					/* SIOCGIWPRIV */
	NULL,					/* SIOCSIWSTATS */
	NULL,					/* SIOCGIWSTATS */
	dummy,					/* SIOCSIWSPY */
	dummy,					/* SIOCGIWSPY */
	NULL,					/* SIOCGIWTHRSPY */
	NULL,					/* SIOCWIWTHRSPY */
	rtw_wx_set_wap,		/* SIOCSIWAP */
	rtw_wx_get_wap,		/* SIOCGIWAP */
	rtw_wx_set_mlme,		/* request MLME operation; uses struct iw_mlme */
	dummy,					/* SIOCGIWAPLIST -- depricated */
	rtw_wx_set_scan,		/* SIOCSIWSCAN */
	rtw_wx_get_scan,		/* SIOCGIWSCAN */
	rtw_wx_set_essid,		/* SIOCSIWESSID */
	rtw_wx_get_essid,		/* SIOCGIWESSID */
	dummy,					/* SIOCSIWNICKN */
	rtw_wx_get_nick,		/* SIOCGIWNICKN */
	NULL,					/* -- hole -- */
	NULL,					/* -- hole -- */
	rtw_wx_set_rate,		/* SIOCSIWRATE */
	rtw_wx_get_rate,		/* SIOCGIWRATE */
	rtw_wx_set_rts,			/* SIOCSIWRTS */
	rtw_wx_get_rts,			/* SIOCGIWRTS */
	rtw_wx_set_frag,		/* SIOCSIWFRAG */
	rtw_wx_get_frag,		/* SIOCGIWFRAG */
	dummy,					/* SIOCSIWTXPOW */
	dummy,					/* SIOCGIWTXPOW */
	dummy,					/* SIOCSIWRETRY */
	rtw_wx_get_retry,		/* SIOCGIWRETRY */
	rtw_wx_set_enc,			/* SIOCSIWENCODE */
	rtw_wx_get_enc,			/* SIOCGIWENCODE */
	dummy,					/* SIOCSIWPOWER */
	rtw_wx_get_power,		/* SIOCGIWPOWER */
	NULL,					/*---hole---*/
	NULL,					/*---hole---*/
	rtw_wx_set_gen_ie,		/* SIOCSIWGENIE */
	NULL,					/* SIOCGWGENIE */
	rtw_wx_set_auth,		/* SIOCSIWAUTH */
	NULL,					/* SIOCGIWAUTH */
	rtw_wx_set_enc_ext,		/* SIOCSIWENCODEEXT */
	NULL,					/* SIOCGIWENCODEEXT */
	rtw_wx_set_pmkid,		/* SIOCSIWPMKSA */
	NULL,					/*---hole---*/
};
#endif


static const struct iw_priv_args rtw_private_args[] = {
	{
		SIOCIWFIRSTPRIV + 0x0,
		IW_PRIV_TYPE_CHAR | 0x7FF, 0, "write"
	},
	{
		SIOCIWFIRSTPRIV + 0x1,
		IW_PRIV_TYPE_CHAR | 0x7FF,
		IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | IFNAMSIZ, "read"
	},
	{
		SIOCIWFIRSTPRIV + 0x2, 0, 0, "driver_ext"
	},
	{
		SIOCIWFIRSTPRIV + 0x3, 0, 0, "mp_ioctl"
	},
	{
		SIOCIWFIRSTPRIV + 0x4,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "apinfo"
	},
	{
		SIOCIWFIRSTPRIV + 0x5,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setpid"
	},
	{
		SIOCIWFIRSTPRIV + 0x6,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_start"
	},
	/* for PLATFORM_MT53XX	 */
	{
		SIOCIWFIRSTPRIV + 0x7,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "get_sensitivity"
	},
	{
		SIOCIWFIRSTPRIV + 0x8,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_prob_req_ie"
	},
	{
		SIOCIWFIRSTPRIV + 0x9,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_assoc_req_ie"
	},

	/* for RTK_DMP_PLATFORM	 */
	{
		SIOCIWFIRSTPRIV + 0xA,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "channel_plan"
	},

	{
		SIOCIWFIRSTPRIV + 0xB,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "dbg"
	},
	{
		SIOCIWFIRSTPRIV + 0xC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "rfw"
	},
	{
		SIOCIWFIRSTPRIV + 0xD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | IFNAMSIZ, "rfr"
	},
	{
		SIOCIWFIRSTPRIV + 0x10,
		IW_PRIV_TYPE_CHAR | 1024, 0, "p2p_set"
	},
	{
		SIOCIWFIRSTPRIV + 0x11,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK , "p2p_get"
	},
	{
		SIOCIWFIRSTPRIV + 0x12, 0, 0, "NULL"
	},
	{
		SIOCIWFIRSTPRIV + 0x13,
		IW_PRIV_TYPE_CHAR | 64, IW_PRIV_TYPE_CHAR | 64 , "p2p_get2"
	},
	{
		SIOCIWFIRSTPRIV + 0x14,
		IW_PRIV_TYPE_CHAR  | 64, 0, "tdls"
	},
	{
		SIOCIWFIRSTPRIV + 0x15,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024 , "tdls_get"
	},
	{
		SIOCIWFIRSTPRIV + 0x16,
		IW_PRIV_TYPE_CHAR | 64, 0, "pm_set"
	},
	{SIOCIWFIRSTPRIV + 0x18, IW_PRIV_TYPE_CHAR | IFNAMSIZ , 0 , "rereg_nd_name"},
	{SIOCIWFIRSTPRIV + 0x1A, IW_PRIV_TYPE_CHAR | 1024, 0,  "NULL"},
	{SIOCIWFIRSTPRIV + 0x1B, IW_PRIV_TYPE_CHAR | 128, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "efuse_get"},
	{
		SIOCIWFIRSTPRIV + 0x1D,
		IW_PRIV_TYPE_CHAR | 40, IW_PRIV_TYPE_CHAR | 0x7FF, "test"
	},

#ifdef CONFIG_INTEL_WIDI
	{
		SIOCIWFIRSTPRIV + 0x1E,
		IW_PRIV_TYPE_CHAR | 1024, 0, "widi_set"
	},
	{
		SIOCIWFIRSTPRIV + 0x1F,
		IW_PRIV_TYPE_CHAR | 128, 0, "widi_prob_req"
	},
#endif /* CONFIG_INTEL_WIDI */

	{ SIOCIWFIRSTPRIV + 0x0E, IW_PRIV_TYPE_CHAR | 1024, 0 , ""},  /* set  */
	{ SIOCIWFIRSTPRIV + 0x0F, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK , ""},/* get
 * --- sub-ioctls definitions --- */
};


static const struct iw_priv_args rtw_mp_private_args[] = {
	/* --- sub-ioctls definitions --- */
};

static iw_handler rtw_private_handler[] = {
	rtw_wx_write32,					/* 0x00 */
	rtw_wx_read32,					/* 0x01 */
	NULL,					/* 0x02 */
#ifdef MP_IOCTL_HDL
	rtw_mp_ioctl_hdl,				/* 0x03 */
#else
	rtw_wx_priv_null,
#endif
	/* for MM DTV platform */
	rtw_get_ap_info,					/* 0x04 */

	rtw_set_pid,						/* 0x05 */
	rtw_wps_start,					/* 0x06 */

	/* for PLATFORM_MT53XX */
	rtw_wx_get_sensitivity,			/* 0x07 */
	rtw_wx_set_mtk_wps_probe_ie,	/* 0x08 */
	rtw_wx_set_mtk_wps_ie,			/* 0x09 */

	/* for RTK_DMP_PLATFORM
	 * Set Channel depend on the country code */
	rtw_wx_set_channel_plan,		/* 0x0A */

	rtw_dbg_port,					/* 0x0B */
	rtw_wx_write_rf,					/* 0x0C */
	rtw_wx_read_rf,					/* 0x0D */

	rtw_priv_set,					/*0x0E*/
	rtw_priv_get,					/*0x0F*/

	rtw_p2p_set,					/* 0x10 */
	rtw_p2p_get,					/* 0x11 */
	NULL,							/* 0x12 */
	rtw_p2p_get2,					/* 0x13 */

	rtw_tdls,						/* 0x14 */
	rtw_tdls_get,					/* 0x15 */

	rtw_pm_set,						/* 0x16 */
	rtw_wx_priv_null,				/* 0x17 */
	rtw_rereg_nd_name,				/* 0x18 */
	rtw_wx_priv_null,				/* 0x19 */
	rtw_wx_priv_null,				/* 0x1A */
	rtw_mp_efuse_get,				/* 0x1B */
	NULL,							/* 0x1C is reserved for hostapd */
	rtw_test,						/* 0x1D */
#ifdef CONFIG_INTEL_WIDI
	rtw_widi_set,					/* 0x1E */
	rtw_widi_set_probe_request,		/* 0x1F */
#endif /* CONFIG_INTEL_WIDI */
};

#ifdef CONFIG_WIRELESS_EXT
#if WIRELESS_EXT >= 17
static struct iw_statistics *rtw_get_wireless_stats(struct net_device *dev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct iw_statistics *piwstats = &adapt->iwstats;
	int tmp_level = 0;
	int tmp_qual = 0;
	int tmp_noise = 0;

	if (!check_fwstate(&adapt->mlmepriv, _FW_LINKED)) {
		piwstats->qual.qual = 0;
		piwstats->qual.level = 0;
		piwstats->qual.noise = 0;
	} else {
		/* Do signal scale mapping when using percentage as the unit of signal strength, since the scale mapping is skipped in odm */
		struct hal_com_data *pHal = GET_HAL_DATA(adapt);

		tmp_level = (u8)phydm_signal_scale_mapping(&pHal->odmpriv, adapt->recvpriv.signal_strength);

		tmp_qual = adapt->recvpriv.signal_qual;
		piwstats->qual.level = tmp_level;
		piwstats->qual.qual = tmp_qual;
		piwstats->qual.noise = tmp_noise;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14))
	piwstats->qual.updated = IW_QUAL_ALL_UPDATED ;/* |IW_QUAL_DBM; */
#else
#ifdef RTK_DMP_PLATFORM
	/* IW_QUAL_DBM= 0x8, if driver use this flag, wireless extension will show value of dbm. */
	/* remove this flag for show percentage 0~100 */
	piwstats->qual.updated = 0x07;
#else
	piwstats->qual.updated = 0x0f;
#endif
#endif
	return &adapt->iwstats;
}
#endif

struct iw_handler_def rtw_handlers_def = {
	.standard = rtw_handlers,
	.num_standard = sizeof(rtw_handlers) / sizeof(iw_handler),
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)) || defined(CONFIG_WEXT_PRIV)
	.private = rtw_private_handler,
	.private_args = (struct iw_priv_args *)rtw_private_args,
	.num_private = sizeof(rtw_private_handler) / sizeof(iw_handler),
	.num_private_args = sizeof(rtw_private_args) / sizeof(struct iw_priv_args),
#endif
#if WIRELESS_EXT >= 17
	.get_wireless_stats = rtw_get_wireless_stats,
#endif
};
#endif

/* copy from net/wireless/wext.c start
 * ----------------------------------------------------------------
 *
 * Calculate size of private arguments
 */
static const char iw_priv_type_size[] = {
	0,                              /* IW_PRIV_TYPE_NONE */
	1,                              /* IW_PRIV_TYPE_BYTE */
	1,                              /* IW_PRIV_TYPE_CHAR */
	0,                              /* Not defined */
	sizeof(__u32),                  /* IW_PRIV_TYPE_INT */
	sizeof(struct iw_freq),         /* IW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr),        /* IW_PRIV_TYPE_ADDR */
	0,                              /* Not defined */
};

static int get_priv_size(__u16 args)
{
	int num = args & IW_PRIV_SIZE_MASK;
	int type = (args & IW_PRIV_TYPE_MASK) >> 12;

	return num * iw_priv_type_size[type];
}
/* copy from net/wireless/wext.c end */


static int _rtw_ioctl_wext_private(struct net_device *dev, union iwreq_data *wrq_data)
{
	int err = 0;
	u8 *input = NULL;
	u32 input_len = 0;
	const char delim[] = " ";
	u8 *output = NULL;
	u32 output_len = 0;
	u32 count = 0;
	u8 *buffer = NULL;
	u32 buffer_len = 0;
	char *ptr = NULL;
	u8 cmdname[17] = {0}; /* IFNAMSIZ+1 */
	u32 cmdlen;
	int len;
	u8 *extra = NULL;
	u32 extra_size = 0;

	int k;
	const iw_handler *priv;		/* Private ioctl */
	const struct iw_priv_args *priv_args;	/* Private ioctl description */
	const struct iw_priv_args *mp_priv_args;	/*MP Private ioctl description */
	const struct iw_priv_args *sel_priv_args;	/*Selected Private ioctl description */
	u32 num_priv;				/* Number of ioctl */
	u32 num_priv_args;			/* Number of descriptions */
	u32 num_mp_priv_args;			/*Number of MP descriptions */
	u32 num_sel_priv_args;			/*Number of Selected descriptions */
	iw_handler handler;
	int temp;
	int subcmd = 0;				/* sub-ioctl index */
	int offset = 0;				/* Space for sub-ioctl index */

	union iwreq_data wdata;

	memcpy(&wdata, wrq_data, sizeof(wdata));

	input_len = wdata.data.length;
	if (!input_len)
		return -EINVAL;
	input = rtw_zmalloc(input_len);
	if (!input)
		return -ENOMEM;
	if (copy_from_user(input, wdata.data.pointer, input_len)) {
		err = -EFAULT;
		goto exit;
	}
	input[input_len - 1] = '\0';
	ptr = input;
	len = input_len;

	if (!ptr) {
		err = -EOPNOTSUPP;
		goto exit;
	}

	sscanf(ptr, "%16s", cmdname);
	cmdlen = strlen(cmdname);
	RTW_INFO("%s: cmd=%s\n", __func__, cmdname);

	/* skip command string */
	if (cmdlen > 0)
		cmdlen += 1; /* skip one space */
	ptr += cmdlen;
	len -= cmdlen;
	RTW_INFO("%s: parameters=%s\n", __func__, ptr);

	priv = rtw_private_handler;
	priv_args = rtw_private_args;
	mp_priv_args = rtw_mp_private_args;
	num_priv = sizeof(rtw_private_handler) / sizeof(iw_handler);
	num_priv_args = sizeof(rtw_private_args) / sizeof(struct iw_priv_args);
	num_mp_priv_args = sizeof(rtw_mp_private_args) / sizeof(struct iw_priv_args);

	if (num_priv_args == 0) {
		err = -EOPNOTSUPP;
		goto exit;
	}

	/* Search the correct ioctl */
	k = -1;
	sel_priv_args = priv_args;
	num_sel_priv_args = num_priv_args;
	while
	((++k < num_sel_priv_args) && strcmp(sel_priv_args[k].name, cmdname))
		;

	/* If not found... */
	if (k == num_sel_priv_args) {
		k = -1;
		sel_priv_args = mp_priv_args;
		num_sel_priv_args = num_mp_priv_args;
		while
		((++k < num_sel_priv_args) && strcmp(sel_priv_args[k].name, cmdname))
			;

		if (k == num_sel_priv_args) {
			err = -EOPNOTSUPP;
			goto exit;
		}
	}

	/* Watch out for sub-ioctls ! */
	if (sel_priv_args[k].cmd < SIOCDEVPRIVATE) {
		int j = -1;

		/* Find the matching *real* ioctl */
		while ((++j < num_priv_args) && ((priv_args[j].name[0] != '\0') ||
			 (priv_args[j].set_args != sel_priv_args[k].set_args) ||
			 (priv_args[j].get_args != sel_priv_args[k].get_args)))
			;

		/* If not found... */
		if (j == num_priv_args) {
			err = -EINVAL;
			goto exit;
		}

		/* Save sub-ioctl number */
		subcmd = sel_priv_args[k].cmd;
		/* Reserve one int (simplify alignment issues) */
		offset = sizeof(__u32);
		/* Use real ioctl definition from now on */
		k = j;
	}

	buffer = rtw_zmalloc(4096);
	if (!buffer) {
		err = -ENOMEM;
		goto exit;
	}

	if (k >= num_priv_args) {
		err = -EINVAL;
		goto exit;
	}

	/* If we have to set some data */
	if ((priv_args[k].set_args & IW_PRIV_TYPE_MASK) &&
	    (priv_args[k].set_args & IW_PRIV_SIZE_MASK)) {
		u8 *str;

		switch (priv_args[k].set_args & IW_PRIV_TYPE_MASK) {
		case IW_PRIV_TYPE_BYTE:
			/* Fetch args */
			count = 0;
			do {
				str = strsep(&ptr, delim);
				if (!str)
					break;
				sscanf(str, "%i", &temp);
				buffer[count++] = (u8)temp;
			} while (1);
			buffer_len = count;

			/* Number of args to fetch */
			wdata.data.length = count;
			if (wdata.data.length > (priv_args[k].set_args & IW_PRIV_SIZE_MASK))
				wdata.data.length = priv_args[k].set_args & IW_PRIV_SIZE_MASK;

			break;

		case IW_PRIV_TYPE_INT:
			/* Fetch args */
			count = 0;
			do {
				str = strsep(&ptr, delim);
				if (!str)
					break;
				sscanf(str, "%i", &temp);
				((int *)buffer)[count++] = (int)temp;
			} while (1);
			buffer_len = count * sizeof(int);

			/* Number of args to fetch */
			wdata.data.length = count;
			if (wdata.data.length > (priv_args[k].set_args & IW_PRIV_SIZE_MASK))
				wdata.data.length = priv_args[k].set_args & IW_PRIV_SIZE_MASK;

			break;

		case IW_PRIV_TYPE_CHAR:
			if (len > 0) {
				/* Size of the string to fetch */
				wdata.data.length = len;
				if (wdata.data.length > (priv_args[k].set_args & IW_PRIV_SIZE_MASK))
					wdata.data.length = priv_args[k].set_args & IW_PRIV_SIZE_MASK;

				/* Fetch string */
				memcpy(buffer, ptr, wdata.data.length);
			} else {
				wdata.data.length = 1;
				buffer[0] = '\0';
			}
			buffer_len = wdata.data.length;
			break;

		default:
			RTW_INFO("%s: Not yet implemented...\n", __func__);
			err = -1;
			goto exit;
		}

		if ((priv_args[k].set_args & IW_PRIV_SIZE_FIXED) &&
		    (wdata.data.length != (priv_args[k].set_args & IW_PRIV_SIZE_MASK))) {
			RTW_INFO("%s: The command %s needs exactly %d argument(s)...\n",
				__func__, cmdname, priv_args[k].set_args & IW_PRIV_SIZE_MASK);
			err = -EINVAL;
			goto exit;
		}
	}   /* if args to set */
	else
		wdata.data.length = 0L;

	/* Those two tests are important. They define how the driver
	* will have to handle the data */
	if ((priv_args[k].set_args & IW_PRIV_SIZE_FIXED) &&
	    ((get_priv_size(priv_args[k].set_args) + offset) <= IFNAMSIZ)) {
		/* First case : all SET args fit within wrq */
		if (offset)
			wdata.mode = subcmd;
		memcpy(wdata.name + offset, buffer, IFNAMSIZ - offset);
	} else {
		if ((priv_args[k].set_args == 0) &&
		    (priv_args[k].get_args & IW_PRIV_SIZE_FIXED) &&
		    (get_priv_size(priv_args[k].get_args) <= IFNAMSIZ)) {
			/* Second case : no SET args, GET args fit within wrq */
			if (offset)
				wdata.mode = subcmd;
		} else {
			/* Third case : args won't fit in wrq, or variable number of args */
			if (copy_to_user(wdata.data.pointer, buffer, buffer_len)) {
				err = -EFAULT;
				goto exit;
			}
			wdata.data.flags = subcmd;
		}
	}

	rtw_mfree(input, input_len);
	input = NULL;

	extra_size = 0;
	if (IW_IS_SET(priv_args[k].cmd)) {
		/* Size of set arguments */
		extra_size = get_priv_size(priv_args[k].set_args);

		/* Does it fits in iwr ? */
		if ((priv_args[k].set_args & IW_PRIV_SIZE_FIXED) &&
		    ((extra_size + offset) <= IFNAMSIZ))
			extra_size = 0;
	} else {
		/* Size of get arguments */
		extra_size = get_priv_size(priv_args[k].get_args);

		/* Does it fits in iwr ? */
		if ((priv_args[k].get_args & IW_PRIV_SIZE_FIXED) &&
		    (extra_size <= IFNAMSIZ))
			extra_size = 0;
	}

	if (extra_size == 0) {
		extra = (u8 *)&wdata;
		rtw_mfree(buffer, 4096);
		buffer = NULL;
	} else
		extra = buffer;

	handler = priv[priv_args[k].cmd - SIOCIWFIRSTPRIV];
	err = handler(dev, NULL, &wdata, extra);

	/* If we have to get some data */
	if ((priv_args[k].get_args & IW_PRIV_TYPE_MASK) &&
	    (priv_args[k].get_args & IW_PRIV_SIZE_MASK)) {
		int j;
		int n = 0;	/* number of args */
		u8 str[20] = {0};

		/* Check where is the returned data */
		if ((priv_args[k].get_args & IW_PRIV_SIZE_FIXED) &&
		    (get_priv_size(priv_args[k].get_args) <= IFNAMSIZ))
			n = priv_args[k].get_args & IW_PRIV_SIZE_MASK;
		else
			n = wdata.data.length;

		output = rtw_zmalloc(4096);
		if (!output) {
			err =  -ENOMEM;
			goto exit;
		}

		switch (priv_args[k].get_args & IW_PRIV_TYPE_MASK) {
		case IW_PRIV_TYPE_BYTE:
			/* Display args */
			for (j = 0; j < n; j++) {
				sprintf(str, "%d  ", extra[j]);
				len = strlen(str);
				output_len = strlen(output);
				if ((output_len + len + 1) > 4096) {
					err = -E2BIG;
					goto exit;
				}
				memcpy(output + output_len, str, len);
			}
			break;

		case IW_PRIV_TYPE_INT:
			/* Display args */
			for (j = 0; j < n; j++) {
				sprintf(str, "%d  ", ((int *)extra)[j]);
				len = strlen(str);
				output_len = strlen(output);
				if ((output_len + len + 1) > 4096) {
					err = -E2BIG;
					goto exit;
				}
				memcpy(output + output_len, str, len);
			}
			break;

		case IW_PRIV_TYPE_CHAR:
			/* Display args */
			memcpy(output, extra, n);
			break;

		default:
			RTW_INFO("%s: Not yet implemented...\n", __func__);
			err = -1;
			goto exit;
		}

		output_len = strlen(output) + 1;
		wrq_data->data.length = output_len;
		if (copy_to_user(wrq_data->data.pointer, output, output_len)) {
			err = -EFAULT;
			goto exit;
		}
	}   /* if args to set */
	else
		wrq_data->data.length = 0;

exit:
	if (input)
		rtw_mfree(input, input_len);
	if (buffer)
		rtw_mfree(buffer, 4096);
	if (output)
		rtw_mfree(output, 4096);

	return err;
}

#ifdef CONFIG_COMPAT
static int rtw_ioctl_compat_wext_private(struct net_device *dev, struct ifreq *rq)
{
	struct compat_iw_point iwp_compat;
	union iwreq_data wrq_data;
	int err = 0;
	RTW_INFO("%s:...\n", __func__);
	if (copy_from_user(&iwp_compat, rq->ifr_ifru.ifru_data, sizeof(struct compat_iw_point)))
		return -EFAULT;

	wrq_data.data.pointer = compat_ptr(iwp_compat.pointer);
	wrq_data.data.length = iwp_compat.length;
	wrq_data.data.flags = iwp_compat.flags;

	err = _rtw_ioctl_wext_private(dev, &wrq_data);

	iwp_compat.pointer = ptr_to_compat(wrq_data.data.pointer);
	iwp_compat.length = wrq_data.data.length;
	iwp_compat.flags = wrq_data.data.flags;
	if (copy_to_user(rq->ifr_ifru.ifru_data, &iwp_compat, sizeof(struct compat_iw_point)))
		return -EFAULT;

	return err;
}
#endif /* CONFIG_COMPAT */

static int rtw_ioctl_standard_wext_private(struct net_device *dev, struct ifreq *rq)
{
	struct iw_point *iwp;
	union iwreq_data wrq_data;
	int err = 0;
	iwp = &wrq_data.data;
	RTW_INFO("%s:...\n", __func__);
	if (copy_from_user(iwp, rq->ifr_ifru.ifru_data, sizeof(struct iw_point)))
		return -EFAULT;

	err = _rtw_ioctl_wext_private(dev, &wrq_data);

	if (copy_to_user(rq->ifr_ifru.ifru_data, iwp, sizeof(struct iw_point)))
		return -EFAULT;

	return err;
}

static int rtw_ioctl_wext_private(struct net_device *dev, struct ifreq *rq)
{
#ifdef CONFIG_COMPAT
#if (KERNEL_VERSION(4, 6, 0) > LINUX_VERSION_CODE)
	if (is_compat_task())
#else
	if (in_compat_syscall())
#endif
		return rtw_ioctl_compat_wext_private(dev, rq);
	else
#endif /* CONFIG_COMPAT */
		return rtw_ioctl_standard_wext_private(dev, rq);
}

int rtw_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct iwreq *wrq = (struct iwreq *)rq;
	int ret = 0;

	switch (cmd) {
	case RTL_IOCTL_WPA_SUPPLICANT:
		ret = wpa_supplicant_ioctl(dev, &wrq->u.data);
		break;
	case RTL_IOCTL_HOSTAPD:
		ret = rtw_hostapd_ioctl(dev, &wrq->u.data);
		break;
#ifdef CONFIG_WIRELESS_EXT
	case SIOCSIWMODE:
		ret = rtw_wx_set_mode(dev, NULL, &wrq->u, NULL);
		break;
#endif
	case SIOCDEVPRIVATE:
		ret = rtw_ioctl_wext_private(dev, rq);
		break;
	case (SIOCDEVPRIVATE+1):
		ret = rtw_android_priv_cmd(dev, rq, cmd);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
