/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_MLME_EXT_H_
#define __RTW_MLME_EXT_H_


/*	Commented by Albert 20101105
 *	Increase the SURVEY_TO value from 100 to 150  ( 100ms to 150ms )
 *	The Realtek 8188CE SoftAP will spend around 100ms to send the probe response after receiving the probe request.
 *	So, this driver tried to extend the dwell time for each scanning channel.
 *	This will increase the chance to receive the probe response from SoftAP. */
#define SURVEY_TO		(100)

#define REAUTH_TO		(300) /* (50) */
#define REASSOC_TO		(300) /* (50) */
/* #define DISCONNECT_TO	(3000) */
#define ADDBA_TO			(2000)

#define LINKED_TO (1) /* unit:2 sec, 1x2 = 2 sec */

#define REAUTH_LIMIT	(4)
#define REASSOC_LIMIT	(4)
#define READDBA_LIMIT	(2)

#define ROAMING_LIMIT	8
/* #define	IOCMD_REG0		0x10250370 */
/* #define	IOCMD_REG1		0x10250374 */
/* #define	IOCMD_REG2		0x10250378 */

/* #define	FW_DYNAMIC_FUN_SWITCH	0x10250364 */

/* #define	WRITE_BB_CMD		0xF0000001 */
/* #define	SET_CHANNEL_CMD	0xF3000000 */
/* #define	UPDATE_RA_CMD	0xFD0000A2 */

#define _HW_STATE_NOLINK_		0x00
#define _HW_STATE_ADHOC_		0x01
#define _HW_STATE_STATION_	0x02
#define _HW_STATE_AP_			0x03
#define _HW_STATE_MONITOR_ 0x04


#define		_1M_RATE_	0
#define		_2M_RATE_	1
#define		_5M_RATE_	2
#define		_11M_RATE_	3
#define		_6M_RATE_	4
#define		_9M_RATE_	5
#define		_12M_RATE_	6
#define		_18M_RATE_	7
#define		_24M_RATE_	8
#define		_36M_RATE_	9
#define		_48M_RATE_	10
#define		_54M_RATE_	11

/********************************************************
MCS rate definitions
*********************************************************/
#define MCS_RATE_1R	(0x000000ff)
#define MCS_RATE_2R	(0x0000ffff)
#define MCS_RATE_3R	(0x00ffffff)
#define MCS_RATE_4R	(0xffffffff)
#define MCS_RATE_2R_13TO15_OFF	(0x00001fff)


extern unsigned char RTW_WPA_OUI[];
extern unsigned char WMM_OUI[];
extern unsigned char WPS_OUI[];
extern unsigned char WFD_OUI[];
extern unsigned char P2P_OUI[];

extern unsigned char WMM_INFO_OUI[];
extern unsigned char WMM_PARA_OUI[];

enum rt_channel_domain {
	/* ===== 0x00 ~ 0x1F, legacy channel plan ===== */
	RTW_CHPLAN_FCC = 0x00,
	RTW_CHPLAN_IC = 0x01,
	RTW_CHPLAN_ETSI = 0x02,
	RTW_CHPLAN_SPAIN = 0x03,
	RTW_CHPLAN_FRANCE = 0x04,
	RTW_CHPLAN_MKK = 0x05,
	RTW_CHPLAN_MKK1 = 0x06,
	RTW_CHPLAN_ISRAEL = 0x07,
	RTW_CHPLAN_TELEC = 0x08,
	RTW_CHPLAN_GLOBAL_DOAMIN = 0x09,
	RTW_CHPLAN_WORLD_WIDE_13 = 0x0A,
	RTW_CHPLAN_TAIWAN = 0x0B,
	RTW_CHPLAN_CHINA = 0x0C,
	RTW_CHPLAN_SINGAPORE_INDIA_MEXICO = 0x0D,
	RTW_CHPLAN_KOREA = 0x0E,
	RTW_CHPLAN_TURKEY = 0x0F,
	RTW_CHPLAN_JAPAN = 0x10,
	RTW_CHPLAN_FCC_NO_DFS = 0x11,
	RTW_CHPLAN_JAPAN_NO_DFS = 0x12,
	RTW_CHPLAN_WORLD_WIDE_5G = 0x13,
	RTW_CHPLAN_TAIWAN_NO_DFS = 0x14,

	/* ===== 0x20 ~ 0x7F, new channel plan ===== */
	RTW_CHPLAN_WORLD_NULL = 0x20,
	RTW_CHPLAN_ETSI1_NULL = 0x21,
	RTW_CHPLAN_FCC1_NULL = 0x22,
	RTW_CHPLAN_MKK1_NULL = 0x23,
	RTW_CHPLAN_ETSI2_NULL = 0x24,
	RTW_CHPLAN_FCC1_FCC1 = 0x25,
	RTW_CHPLAN_WORLD_ETSI1 = 0x26,
	RTW_CHPLAN_MKK1_MKK1 = 0x27,
	RTW_CHPLAN_WORLD_KCC1 = 0x28,
	RTW_CHPLAN_WORLD_FCC2 = 0x29,
	RTW_CHPLAN_FCC2_NULL = 0x2A,
	RTW_CHPLAN_IC1_IC2 = 0x2B,
	RTW_CHPLAN_MKK2_NULL = 0x2C,
	RTW_CHPLAN_WORLD_CHILE1= 0x2D,
	RTW_CHPLAN_WORLD_FCC3 = 0x30,
	RTW_CHPLAN_WORLD_FCC4 = 0x31,
	RTW_CHPLAN_WORLD_FCC5 = 0x32,
	RTW_CHPLAN_WORLD_FCC6 = 0x33,
	RTW_CHPLAN_FCC1_FCC7 = 0x34,
	RTW_CHPLAN_WORLD_ETSI2 = 0x35,
	RTW_CHPLAN_WORLD_ETSI3 = 0x36,
	RTW_CHPLAN_MKK1_MKK2 = 0x37,
	RTW_CHPLAN_MKK1_MKK3 = 0x38,
	RTW_CHPLAN_FCC1_NCC1 = 0x39,
	RTW_CHPLAN_FCC1_NCC2 = 0x40,
	RTW_CHPLAN_GLOBAL_NULL = 0x41,
	RTW_CHPLAN_ETSI1_ETSI4 = 0x42,
	RTW_CHPLAN_FCC1_FCC2 = 0x43,
	RTW_CHPLAN_FCC1_NCC3 = 0x44,
	RTW_CHPLAN_WORLD_ACMA1 = 0x45,
	RTW_CHPLAN_FCC1_FCC8 = 0x46,
	RTW_CHPLAN_WORLD_ETSI6 = 0x47,
	RTW_CHPLAN_WORLD_ETSI7 = 0x48,
	RTW_CHPLAN_WORLD_ETSI8 = 0x49,
	RTW_CHPLAN_WORLD_ETSI9 = 0x50,
	RTW_CHPLAN_WORLD_ETSI10 = 0x51,
	RTW_CHPLAN_WORLD_ETSI11 = 0x52,
	RTW_CHPLAN_FCC1_NCC4 = 0x53,
	RTW_CHPLAN_WORLD_ETSI12 = 0x54,
	RTW_CHPLAN_FCC1_FCC9 = 0x55,
	RTW_CHPLAN_WORLD_ETSI13 = 0x56,
	RTW_CHPLAN_FCC1_FCC10 = 0x57,
	RTW_CHPLAN_MKK2_MKK4 = 0x58,
	RTW_CHPLAN_WORLD_ETSI14 = 0x59,
	RTW_CHPLAN_FCC1_FCC5 = 0x60,
	RTW_CHPLAN_FCC2_FCC7 = 0x61,
	RTW_CHPLAN_FCC2_FCC1 = 0x62,
	RTW_CHPLAN_WORLD_ETSI15 = 0x63,
	RTW_CHPLAN_MKK2_MKK5 = 0x64,
	RTW_CHPLAN_ETSI1_ETSI16 = 0x65,
	RTW_CHPLAN_FCC1_FCC14 = 0x66,
	RTW_CHPLAN_FCC1_FCC12 = 0x67,
	RTW_CHPLAN_FCC2_FCC14 = 0x68,
	RTW_CHPLAN_FCC2_FCC12 = 0x69,
	RTW_CHPLAN_ETSI1_ETSI17 = 0x6A,
	RTW_CHPLAN_WORLD_FCC16 = 0x6B,
	RTW_CHPLAN_WORLD_FCC13 = 0x6C,
	RTW_CHPLAN_FCC2_FCC15 = 0x6D,
	RTW_CHPLAN_WORLD_FCC12 = 0x6E,
	RTW_CHPLAN_NULL_ETSI8 = 0x6F,
	RTW_CHPLAN_NULL_ETSI18 = 0x70,
	RTW_CHPLAN_NULL_ETSI17 = 0x71,
	RTW_CHPLAN_NULL_ETSI19 = 0x72,
	RTW_CHPLAN_WORLD_FCC7 = 0x73,
	RTW_CHPLAN_FCC2_FCC17 = 0x74,
	RTW_CHPLAN_WORLD_ETSI20 = 0x75,
	RTW_CHPLAN_FCC2_FCC11 = 0x76,
	RTW_CHPLAN_WORLD_ETSI21 = 0x77,
	RTW_CHPLAN_FCC1_FCC18 = 0x78,
	RTW_CHPLAN_MKK2_MKK1 = 0x79,

	RTW_CHPLAN_MAX,
	RTW_CHPLAN_REALTEK_DEFINE = 0x7F,
	RTW_CHPLAN_UNSPECIFIED = 0xFF,
};

bool rtw_chplan_is_empty(u8 id);
#define rtw_is_channel_plan_valid(chplan) (((chplan) < RTW_CHPLAN_MAX || (chplan) == RTW_CHPLAN_REALTEK_DEFINE) && !rtw_chplan_is_empty(chplan))
#define rtw_is_legacy_channel_plan(chplan) ((chplan) < 0x20)

struct rt_channel_plan {
	unsigned char	Channel[MAX_CHANNEL_NUM];
	unsigned char	Len;
};

struct ch_list_t {
	u8 *len_ch;
};

#define CH_LIST_ENT(_len, arg...) \
	{.len_ch = (u8[_len + 1]) {_len, ##arg}, }

#define CH_LIST_LEN(_ch_list) (_ch_list.len_ch[0])
#define CH_LIST_CH(_ch_list, _i) (_ch_list.len_ch[_i + 1])

struct rt_channel_plan_map {
	u8 Index2G;
	u8 regd; /* value of REGULATION_TXPWR_LMT */
};

#define CHPLAN_ENT(i2g, i5g, regd) {i2g, regd}

enum Associated_AP {
	atherosAP	= 0,
	broadcomAP	= 1,
	ciscoAP		= 2,
	marvellAP	= 3,
	ralinkAP	= 4,
	realtekAP	= 5,
	airgocapAP	= 6,
	unknownAP	= 7,
	maxAP,
};

enum ht_iot_peer {
	HT_IOT_PEER_UNKNOWN			= 0,
	HT_IOT_PEER_REALTEK			= 1,
	HT_IOT_PEER_REALTEK_92SE		= 2,
	HT_IOT_PEER_BROADCOM		= 3,
	HT_IOT_PEER_RALINK			= 4,
	HT_IOT_PEER_ATHEROS			= 5,
	HT_IOT_PEER_CISCO				= 6,
	HT_IOT_PEER_MERU				= 7,
	HT_IOT_PEER_MARVELL			= 8,
	HT_IOT_PEER_REALTEK_SOFTAP 	= 9,/* peer is RealTek SOFT_AP, by Bohn, 2009.12.17 */
	HT_IOT_PEER_SELF_SOFTAP 		= 10, /* Self is SoftAP */
	HT_IOT_PEER_AIRGO				= 11,
	HT_IOT_PEER_INTEL				= 12,
	HT_IOT_PEER_RTK_APCLIENT		= 13,
	HT_IOT_PEER_REALTEK_81XX		= 14,
	HT_IOT_PEER_REALTEK_WOW		= 15,
	HT_IOT_PEER_REALTEK_JAGUAR_BCUTAP = 16,
	HT_IOT_PEER_REALTEK_JAGUAR_CCUTAP = 17,
	HT_IOT_PEER_MAX				= 18
};

struct mlme_handler {
	unsigned int   num;
	char *str;
	unsigned int (*func)(struct adapter *adapt, union recv_frame *precv_frame);
};

struct action_handler {
	unsigned int   num;
	char *str;
	unsigned int (*func)(struct adapter *adapt, union recv_frame *precv_frame);
};

enum SCAN_STATE {
	SCAN_DISABLE = 0,
	SCAN_START = 1,
	SCAN_PS_ANNC_WAIT = 2,
	SCAN_ENTER = 3,
	SCAN_PROCESS = 4,

	/* backop */
	SCAN_BACKING_OP = 5,
	SCAN_BACK_OP = 6,
	SCAN_LEAVING_OP = 7,
	SCAN_LEAVE_OP = 8,

	/* SW antenna diversity (before linked) */
	SCAN_SW_ANTDIV_BL = 9,

	/* legacy p2p */
	SCAN_TO_P2P_LISTEN = 10,
	SCAN_P2P_LISTEN = 11,

	SCAN_COMPLETE = 12,
	SCAN_STATE_MAX,
};

const char *scan_state_str(u8 state);

enum ss_backop_flag {
	SS_BACKOP_EN = BIT0, /* backop when linked */
	SS_BACKOP_EN_NL = BIT1, /* backop even when no linked */

	SS_BACKOP_PS_ANNC = BIT4,
	SS_BACKOP_TX_RESUME = BIT5,
};

struct ss_res {
	u8 state;
	u8 next_state; /* will set to state on next cmd hdl */
	int	bss_cnt;
	int	channel_idx;
#ifdef CONFIG_DFS
	u8 dfs_ch_ssid_scan;
#endif
	int	scan_mode;
	u16 scan_ch_ms;
	u32 scan_timeout_ms;
	u8 rx_ampdu_accept;
	u8 rx_ampdu_size;
	u8 igi_scan;
	u8 igi_before_scan; /* used for restoring IGI value without enable DIG & FA_CNT */
	u8 backop_flags_sta; /* policy for station mode*/
	u8 backop_flags_ap; /* policy for ap mode */
	u8 backop_flags; /* per backop runtime decision */
	u8 scan_cnt;
	u8 scan_cnt_max;
	unsigned long backop_time; /* the start time of backop */
	u16 backop_ms;
	u8 ssid_num;
	u8 ch_num;
	struct ndis_802_11_ssid ssid[RTW_SSID_SCAN_AMOUNT];
	struct rtw_ieee80211_channel ch[RTW_CHANNEL_SCAN_AMOUNT];

	u32 token; 	/* 0: use to identify caller */
	u16 duration;	/* 0: use default */
	u8 igi;		/* 0: use defalut */
	u8 bw;		/* 0: use default */
};

/* #define AP_MODE				0x0C */
/* #define STATION_MODE	0x08 */
/* #define AD_HOC_MODE		0x04 */
/* #define NO_LINK_MODE	0x00 */

#define	WIFI_FW_NULL_STATE			_HW_STATE_NOLINK_
#define	WIFI_FW_STATION_STATE		_HW_STATE_STATION_
#define	WIFI_FW_AP_STATE				_HW_STATE_AP_
#define	WIFI_FW_ADHOC_STATE			_HW_STATE_ADHOC_

#define WIFI_FW_PRE_LINK			0x00000800
#define	WIFI_FW_AUTH_NULL			0x00000100
#define	WIFI_FW_AUTH_STATE			0x00000200
#define	WIFI_FW_AUTH_SUCCESS			0x00000400

#define	WIFI_FW_ASSOC_STATE			0x00002000
#define	WIFI_FW_ASSOC_SUCCESS		0x00004000

#define	WIFI_FW_LINKING_STATE		(WIFI_FW_AUTH_NULL | WIFI_FW_AUTH_STATE | WIFI_FW_AUTH_SUCCESS | WIFI_FW_ASSOC_STATE)

/*
 * Usage:
 * When one iface acted as AP mode and the other iface is STA mode and scanning,
 * it should switch back to AP's operating channel periodically.
 * Parameters info:
 * When the driver scanned RTW_SCAN_NUM_OF_CH channels, it would switch back to AP's operating channel for
 * RTW_BACK_OP_CH_MS milliseconds.
 * Example:
 * For chip supports 2.4G + 5GHz and AP mode is operating in channel 1,
 * RTW_SCAN_NUM_OF_CH is 8, RTW_BACK_OP_CH_MS is 300
 * When it's STA mode gets set_scan command,
 * it would
 * 1. Doing the scan on channel 1.2.3.4.5.6.7.8
 * 2. Back to channel 1 for 300 milliseconds
 * 3. Go through doing site survey on channel 9.10.11.36.40.44.48.52
 * 4. Back to channel 1 for 300 milliseconds
 * 5. ... and so on, till survey done.
 */
#define RTW_SCAN_NUM_OF_CH 3
#define RTW_BACK_OP_CH_MS 400

#define RTW_IP_ADDR_LEN 4
#define RTW_IPv6_ADDR_LEN 16

struct mlme_ext_info {
	u32	state;
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	u8	hw_media_state;
#endif
	u32	reauth_count;
	u32	reassoc_count;
	u32	link_count;
	u32	auth_seq;
	u32	auth_algo;	/* 802.11 auth, could be open, shared, auto */
	u32	authModeToggle;
	u32	enc_algo;/* encrypt algorithm; */
	u32	key_index;	/* this is only valid for legendary wep, 0~3 for key id. */
	u32	iv;
	u8	chg_txt[128];
	u16	aid;
	u16	bcn_interval;
	u16	capability;
	u8	assoc_AP_vendor;
	u8	slotTime;
	u8	preamble_mode;
	u8	WMM_enable;
	u8	ERP_enable;
	u8	ERP_IE;
	u8	HT_enable;
	u8	HT_caps_enable;
	u8	HT_info_enable;
	u8	HT_protection;
	u8	turboMode_cts2self;
	u8	turboMode_rtsen;
	u8	SM_PS;
	u8	agg_enable_bitmap;
	u8	ADDBA_retry_count;
	u8	candidate_tid_bitmap;
	u8	dialogToken;
	/* Accept ADDBA Request */
	bool bAcceptAddbaReq;
	u8	bwmode_updated;
	u8	hidden_ssid_mode;
	u8	VHT_enable;

	u8 ip_addr[RTW_IP_ADDR_LEN];
	u8 ip6_addr[RTW_IPv6_ADDR_LEN];

	struct ADDBA_request		ADDBA_req;
	struct WMM_para_element	WMM_param;
	struct HT_caps_element	HT_caps;
	struct HT_info_element		HT_info;
	struct wlan_bssid_ex			network;/* join network or bss_network, if in ap mode, it is the same to cur_network.network */
};

/* The channel information about this channel including joining, scanning, and power constraints. */
struct rt_channel_info {
	u8				ChannelNum;		/* The channel number. */
	enum rt_scan_type	ScanType;		/* Scan type such as passive or active scan. */
	/* u16				ScanPeriod;		 */ /* Listen time in millisecond in this channel. */
	/* int				MaxTxPwrDbm;	 */ /* Max allowed tx power. */
	/* u32				ExInfo;			 */ /* Extended Information for this channel. */
#ifdef CONFIG_DFS
	u8 hidden_bss_cnt; /* per scan count */
#endif
};

#define DFS_MASTER_TIMER_MS 100
#define CAC_TIME_MS (60*1000)
#define CAC_TIME_CE_MS (10*60*1000)
#define NON_OCP_TIME_MS (30*60*1000)

#ifdef CONFIG_TXPWR_LIMIT
void rtw_txpwr_init_regd(struct rf_ctl_t *rfctl);
#endif
void rtw_rfctl_init(struct adapter *adapter);
void rtw_rfctl_deinit(struct adapter *adapter);

#define CH_IS_NON_OCP(rt_ch_info) 0
#define rtw_chset_is_ch_non_ocp(ch_set, ch, bw, offset) false
#define rtw_rfctl_is_tx_blocked_by_ch_waiting(rfctl) false

enum {
	RTW_CHF_2G = BIT0,
	RTW_CHF_5G = BIT1,
	RTW_CHF_DFS = BIT2,
	RTW_CHF_LONG_CAC = BIT3,
	RTW_CHF_NON_DFS = BIT4,
	RTW_CHF_NON_LONG_CAC = BIT5,
	RTW_CHF_NON_OCP = BIT6,
};

bool rtw_choose_shortest_waiting_ch(struct adapter *adapter, u8 req_bw, u8 *dec_ch, u8 *dec_bw, u8 *dec_offset, u8 d_flags);

void dump_country_chplan(void *sel, const struct country_chplan *ent);
void dump_country_chplan_map(void *sel);
void dump_chplan_id_list(void *sel);
void dump_chplan_test(void *sel);
void dump_chset(void *sel, struct rt_channel_info *ch_set);
void dump_cur_chset(void *sel, struct adapter *adapter);

int rtw_chset_search_ch(struct rt_channel_info *ch_set, const u32 ch);
u8 rtw_chset_is_chbw_valid(struct rt_channel_info *ch_set, u8 ch, u8 bw, u8 offset);

bool rtw_mlme_band_check(struct adapter *adapter, const u32 ch);


enum {
	BAND_24G = BIT0,
	BAND_5G = BIT1,
};
void RTW_SET_SCAN_BAND_SKIP(struct adapter *adapt, int skip_band);
void RTW_CLR_SCAN_BAND_SKIP(struct adapter *adapt, int skip_band);
int RTW_GET_SCAN_BAND_SKIP(struct adapter *adapt);

bool rtw_mlme_ignore_chan(struct adapter *adapter, const u32 ch);

/* P2P_MAX_REG_CLASSES - Maximum number of regulatory classes */
#define P2P_MAX_REG_CLASSES 10

/* P2P_MAX_REG_CLASS_CHANNELS - Maximum number of channels per regulatory class */
#define P2P_MAX_REG_CLASS_CHANNELS 20

/* struct p2p_channels - List of supported channels */
struct p2p_channels {
	/* struct p2p_reg_class - Supported regulatory class */
	struct p2p_reg_class {
		/* reg_class - Regulatory class (IEEE 802.11-2007, Annex J) */
		u8 reg_class;

		/* channel - Supported channels */
		u8 channel[P2P_MAX_REG_CLASS_CHANNELS];

		/* channels - Number of channel entries in use */
		size_t channels;
	} reg_class[P2P_MAX_REG_CLASSES];

	/* reg_classes - Number of reg_class entries in use */
	size_t reg_classes;
};

struct p2p_oper_class_map {
	enum hw_mode {IEEE80211G, IEEE80211A} mode;
	u8 op_class;
	u8 min_chan;
	u8 max_chan;
	u8 inc;
	enum { BW20, BW40PLUS, BW40MINUS } bw;
};

struct mlme_ext_priv {
	struct adapter	*adapt;
	u8	mlmeext_init;
	ATOMIC_T		event_seq;
	u16	mgnt_seq;
#ifdef CONFIG_IEEE80211W
	u16	sa_query_seq;
#endif
	/* struct fw_priv 	fwpriv; */

	unsigned char	cur_channel;
	unsigned char	cur_bwmode;
	unsigned char	cur_ch_offset;/* PRIME_CHNL_OFFSET */
	unsigned char	cur_wireless_mode;	/* NETWORK_TYPE */

	unsigned char	basicrate[NumRates];
	unsigned char	datarate[NumRates];
	unsigned char default_supported_mcs_set[16];

	struct ss_res		sitesurvey_res;
	struct mlme_ext_info	mlmext_info;/* for sta/adhoc mode, including current scanning/connecting/connected related info.
                                                      * for ap mode, network includes ap's cap_info */
	struct timer_list		survey_timer;
	struct timer_list		link_timer;

#ifdef CONFIG_RTW_80211R
	struct timer_list		ft_link_timer;
	struct timer_list		ft_roam_timer;
#endif

	unsigned long last_scan_time;
	u8	scan_abort;
	u8 join_abort;
	u8	tx_rate; /* TXRATE when USERATE is set. */

	u32	retry; /* retry for issue probereq */

	u64 TSFValue;

	/* for LPS-32K to adaptive bcn early and timeout */
	u8 adaptive_tsf_done;
	u32 bcn_delay_cnt[9];
	u32 bcn_delay_ratio[9];
	u32 bcn_cnt;
	u8 DrvBcnEarly;
	u8 DrvBcnTimeOut;

	unsigned char bstart_bss;

	u8 update_channel_plan_by_ap_done;
	/* recv_decache check for Action_public frame */
	u8 action_public_dialog_token;
	u16	 action_public_rxseq;

	u8 active_keep_alive_check;
	/* set hw sync bcn tsf register or not */
	u8 en_hw_update_tsf;
};

static inline u8 check_mlmeinfo_state(struct mlme_ext_priv *plmeext, int state)
{
	if ((plmeext->mlmext_info.state & 0x03) == state)
		return true;

	return false;
}

void sitesurvey_set_offch_state(struct adapter *adapter, u8 scan_state);

#define mlmeext_msr(mlmeext) ((mlmeext)->mlmext_info.state & 0x03)
#define mlmeext_scan_state(mlmeext) ((mlmeext)->sitesurvey_res.state)
#define mlmeext_scan_state_str(mlmeext) scan_state_str((mlmeext)->sitesurvey_res.state)
#define mlmeext_chk_scan_state(mlmeext, _state) ((mlmeext)->sitesurvey_res.state == (_state))
#define mlmeext_set_scan_state(mlmeext, _state) \
	do { \
		((mlmeext)->sitesurvey_res.state = (_state)); \
		((mlmeext)->sitesurvey_res.next_state = (_state)); \
		rtw_mi_update_iface_status(&((container_of(mlmeext, struct adapter, mlmeextpriv)->mlmepriv)), 0); \
		/* RTW_INFO("set_scan_state:%s\n", scan_state_str(_state)); */ \
		sitesurvey_set_offch_state(container_of(mlmeext, struct adapter, mlmeextpriv), _state); \
	} while (0)

#define mlmeext_scan_next_state(mlmeext) ((mlmeext)->sitesurvey_res.next_state)
#define mlmeext_set_scan_next_state(mlmeext, _state) \
	do { \
		((mlmeext)->sitesurvey_res.next_state = (_state)); \
		/* RTW_INFO("set_scan_next_state:%s\n", scan_state_str(_state)); */ \
	} while (0)

#define mlmeext_scan_backop_flags(mlmeext) ((mlmeext)->sitesurvey_res.backop_flags)
#define mlmeext_chk_scan_backop_flags(mlmeext, flags) ((mlmeext)->sitesurvey_res.backop_flags & (flags))
#define mlmeext_assign_scan_backop_flags(mlmeext, flags) \
	do { \
		((mlmeext)->sitesurvey_res.backop_flags = (flags)); \
		RTW_INFO("assign_scan_backop_flags:0x%02x\n", (mlmeext)->sitesurvey_res.backop_flags); \
	} while (0)

#define mlmeext_scan_backop_flags_sta(mlmeext) ((mlmeext)->sitesurvey_res.backop_flags_sta)
#define mlmeext_chk_scan_backop_flags_sta(mlmeext, flags) ((mlmeext)->sitesurvey_res.backop_flags_sta & (flags))
#define mlmeext_assign_scan_backop_flags_sta(mlmeext, flags) \
	do { \
		((mlmeext)->sitesurvey_res.backop_flags_sta = (flags)); \
	} while (0)

#define mlmeext_scan_backop_flags_ap(mlmeext) ((mlmeext)->sitesurvey_res.backop_flags_ap)
#define mlmeext_chk_scan_backop_flags_ap(mlmeext, flags) ((mlmeext)->sitesurvey_res.backop_flags_ap & (flags))
#define mlmeext_assign_scan_backop_flags_ap(mlmeext, flags) \
	do { \
		((mlmeext)->sitesurvey_res.backop_flags_ap = (flags)); \
	} while (0)

u32 rtw_scan_timeout_decision(struct adapter *adapt);

void init_mlme_default_rate_set(struct adapter *adapt);
int init_mlme_ext_priv(struct adapter *adapt);
int init_hw_mlme_ext(struct adapter *adapt);
void free_mlme_ext_priv(struct mlme_ext_priv *pmlmeext);
extern struct xmit_frame *alloc_mgtxmitframe(struct xmit_priv *pxmitpriv);
struct xmit_frame *alloc_mgtxmitframe_once(struct xmit_priv *pxmitpriv);

u8 judge_network_type(struct adapter *adapt, unsigned char *rate, int ratelen);
void get_rate_set(struct adapter *adapt, unsigned char *pbssrate, int *bssrate_len);
void set_mcs_rate_by_mask(u8 *mcs_set, u32 mask);
void UpdateBrateTbl(struct adapter *adapt, u8 *mBratesOS);
void UpdateBrateTblForSoftAP(u8 *bssrateset, u32 bssratelen);
void change_band_update_ie(struct adapter *adapt, struct wlan_bssid_ex *pnetwork, u8 ch);

void Set_MSR(struct adapter *adapt, u8 type);

u8 rtw_get_oper_ch(struct adapter *adapter);
void rtw_set_oper_ch(struct adapter *adapter, u8 ch);
u8 rtw_get_oper_bw(struct adapter *adapter);
void rtw_set_oper_bw(struct adapter *adapter, u8 bw);
u8 rtw_get_oper_choffset(struct adapter *adapter);
void rtw_set_oper_choffset(struct adapter *adapter, u8 offset);
u8	rtw_get_center_ch(u8 channel, u8 chnl_bw, u8 chnl_offset);
unsigned long rtw_get_on_oper_ch_time(struct adapter *adapter);
unsigned long rtw_get_on_cur_ch_time(struct adapter *adapter);

u8 rtw_get_offset_by_chbw(u8 ch, u8 bw, u8 *r_offset);
u8 rtw_get_offset_by_ch(u8 channel);

void set_channel_bwmode(struct adapter *adapt, unsigned char channel, unsigned char channel_offset, unsigned short bwmode);

unsigned int decide_wait_for_beacon_timeout(unsigned int bcn_interval);

void _clear_cam_entry(struct adapter *adapt, u8 entry);
void write_cam_from_cache(struct adapter *adapter, u8 id);
void rtw_sec_cam_swap(struct adapter *adapter, u8 cam_id_a, u8 cam_id_b);
void rtw_clean_dk_section(struct adapter *adapter);
void rtw_clean_hw_dk_cam(struct adapter *adapter);

/* modify both HW and cache */
void write_cam(struct adapter *adapt, u8 id, u16 ctrl, u8 *mac, u8 *key);
void clear_cam_entry(struct adapter *adapt, u8 id);

/* modify cache only */
void write_cam_cache(struct adapter *adapter, u8 id, u16 ctrl, u8 *mac, u8 *key);
void clear_cam_cache(struct adapter *adapter, u8 id);

void invalidate_cam_all(struct adapter *adapt);
void CAM_empty_entry(struct adapter * Adapter, u8 ucIndex);

void flush_all_cam_entry(struct adapter *adapt);

bool IsLegal5GChannel(struct adapter * Adapter, u8 channel);

void site_survey(struct adapter *adapt, u8 survey_channel, enum rt_scan_type ScanType);
u8 collect_bss_info(struct adapter *adapt, union recv_frame *precv_frame, struct wlan_bssid_ex *bssid);
void update_network(struct wlan_bssid_ex *dst, struct wlan_bssid_ex *src, struct adapter *adapt, bool update_ie);

u8 *get_my_bssid(struct wlan_bssid_ex *pnetwork);
u16 get_beacon_interval(struct wlan_bssid_ex *bss);

int is_client_associated_to_ap(struct adapter *adapt);
int is_client_associated_to_ibss(struct adapter *adapt);
int is_IBSS_empty(struct adapter *adapt);

unsigned char check_assoc_AP(u8 *pframe, uint len);

int WMM_param_handler(struct adapter *adapt, struct ndis_802_11_variable_ies *	pIE);
void rtw_process_wfd_ie(struct adapter *adapter, u8 *ie, u8 ie_len, const char *tag);
void rtw_process_wfd_ies(struct adapter *adapter, u8 *ies, u8 ies_len, const char *tag);
void WMMOnAssocRsp(struct adapter *adapt);

void HT_caps_handler(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE);
void HT_info_handler(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE);
void HTOnAssocRsp(struct adapter *adapt);

void ERP_IE_handler(struct adapter *adapt, struct ndis_802_11_variable_ies * pIE);
void VCS_update(struct adapter *adapt, struct sta_info *psta);
void	update_ldpc_stbc_cap(struct sta_info *psta);

int rtw_get_bcn_keys(struct adapter *Adapter, u8 *pframe, u32 packet_len,
		struct beacon_keys *recv_beacon);
int validate_beacon_len(u8 *pframe, uint len);
void rtw_dump_bcn_keys(struct beacon_keys *recv_beacon);
int rtw_check_bcn_info(struct adapter *adapt, u8 *pframe, u32 packet_len);
void update_beacon_info(struct adapter *adapt, u8 *pframe, uint len, struct sta_info *psta);
#ifdef CONFIG_DFS
void process_csa_ie(struct adapter *adapt, u8 *pframe, uint len);
#endif /* CONFIG_DFS */
void update_capinfo(struct adapter * Adapter, u16 updateCap);
void update_wireless_mode(struct adapter *adapt);
void update_tx_basic_rate(struct adapter *adapt, u8 modulation);
void update_sta_basic_rate(struct sta_info *psta, u8 wireless_mode);
int rtw_ies_get_supported_rate(u8 *ies, uint ies_len, u8 *rate_set, u8 *rate_num);

/* for sta/adhoc mode */
void update_sta_info(struct adapter *adapt, struct sta_info *psta);
unsigned int update_basic_rate(unsigned char *ptn, unsigned int ptn_sz);
unsigned int update_supported_rate(unsigned char *ptn, unsigned int ptn_sz);
void Update_RA_Entry(struct adapter *adapt, struct sta_info *psta);
void set_sta_rate(struct adapter *adapt, struct sta_info *psta);

unsigned int receive_disconnect(struct adapter *adapt, unsigned char *MacAddr, unsigned short reason, u8 locally_generated);

unsigned char get_highest_rate_idx(u64 mask);
unsigned char get_lowest_rate_idx_ex(u64 mask, int start_bit);
#define get_lowest_rate_idx(mask) get_lowest_rate_idx_ex(mask, 0)

int support_short_GI(struct adapter *adapt, struct HT_caps_element *pHT_caps, u8 bwmode);
unsigned int is_ap_in_tkip(struct adapter *adapt);
unsigned int is_ap_in_wep(struct adapter *adapt);
unsigned int should_forbid_n_rate(struct adapter *adapt);

bool _rtw_camctl_chk_cap(struct adapter *adapter, u8 cap);
void _rtw_camctl_set_flags(struct adapter *adapter, u32 flags);
void rtw_camctl_set_flags(struct adapter *adapter, u32 flags);
void _rtw_camctl_clr_flags(struct adapter *adapter, u32 flags);
void rtw_camctl_clr_flags(struct adapter *adapter, u32 flags);
bool _rtw_camctl_chk_flags(struct adapter *adapter, u32 flags);

struct sec_cam_bmp;
void dump_sec_cam_map(void *sel, struct sec_cam_bmp *map, u8 max_num);
void rtw_sec_cam_map_clr_all(struct sec_cam_bmp *map);

bool _rtw_camid_is_gk(struct adapter *adapter, u8 cam_id);
bool rtw_camid_is_gk(struct adapter *adapter, u8 cam_id);
s16 rtw_camid_search(struct adapter *adapter, u8 *addr, s16 kid, s8 gk);
s16 rtw_camid_alloc(struct adapter *adapter, struct sta_info *sta, u8 kid, bool *used);
void rtw_camid_free(struct adapter *adapter, u8 cam_id);
u8 rtw_get_sec_camid(struct adapter *adapter, u8 max_bk_key_num, u8 *sec_key_id);

struct macid_bmp;
struct macid_ctl_t;
void dump_macid_map(void *sel, struct macid_bmp *map, u8 max_num);
bool rtw_macid_is_set(struct macid_bmp *map, u8 id);
bool rtw_macid_is_used(struct macid_ctl_t *macid_ctl, u8 id);
bool rtw_macid_is_bmc(struct macid_ctl_t *macid_ctl, u8 id);
u8 rtw_macid_get_iface_bmp(struct macid_ctl_t *macid_ctl, u8 id);
bool rtw_macid_is_iface_shared(struct macid_ctl_t *macid_ctl, u8 id);
bool rtw_macid_is_iface_specific(struct macid_ctl_t *macid_ctl, u8 id, struct adapter *adapter);
s8 rtw_macid_get_ch_g(struct macid_ctl_t *macid_ctl, u8 id);
void rtw_alloc_macid(struct adapter *adapt, struct sta_info *psta);
void rtw_release_macid(struct adapter *adapt, struct sta_info *psta);
u8 rtw_search_max_mac_id(struct adapter *adapt);
void rtw_macid_ctl_set_h2c_msr(struct macid_ctl_t *macid_ctl, u8 id, u8 h2c_msr);
void rtw_macid_ctl_set_bw(struct macid_ctl_t *macid_ctl, u8 id, u8 bw);
void rtw_macid_ctl_set_vht_en(struct macid_ctl_t *macid_ctl, u8 id, u8 en);
void rtw_macid_ctl_set_rate_bmp0(struct macid_ctl_t *macid_ctl, u8 id, u32 bmp);
void rtw_macid_ctl_set_rate_bmp1(struct macid_ctl_t *macid_ctl, u8 id, u32 bmp);
void rtw_macid_ctl_init_sleep_reg(struct macid_ctl_t *macid_ctl, u16 m0, u16 m1, u16 m2, u16 m3);
void rtw_macid_ctl_init(struct macid_ctl_t *macid_ctl);
void rtw_macid_ctl_deinit(struct macid_ctl_t *macid_ctl);
u8 rtw_iface_bcmc_id_get(struct adapter *adapt);
void rtw_iface_bcmc_id_set(struct adapter *adapt, u8 mac_id);

bool rtw_bmp_is_set(const u8 *bmp, u8 bmp_len, u8 id);
void rtw_bmp_set(u8 *bmp, u8 bmp_len, u8 id);
void rtw_bmp_clear(u8 *bmp, u8 bmp_len, u8 id);
bool rtw_bmp_not_empty(const u8 *bmp, u8 bmp_len);
bool rtw_bmp_not_empty_exclude_bit0(const u8 *bmp, u8 bmp_len);

bool rtw_tim_map_is_set(struct adapter *adapt, const u8 *map, u8 id);
void rtw_tim_map_set(struct adapter *adapt, u8 *map, u8 id);
void rtw_tim_map_clear(struct adapter *adapt, u8 *map, u8 id);
bool rtw_tim_map_anyone_be_set(struct adapter *adapt, const u8 *map);
bool rtw_tim_map_anyone_be_set_exclude_aid0(struct adapter *adapt, const u8 *map);

u32 report_join_res(struct adapter *adapt, int res);
void report_survey_event(struct adapter *adapt, union recv_frame *precv_frame);
void report_surveydone_event(struct adapter *adapt);
u32 report_del_sta_event(struct adapter *adapt, unsigned char *MacAddr, unsigned short reason, bool enqueue, u8 locally_generated);
void report_add_sta_event(struct adapter *adapt, unsigned char *MacAddr);
bool rtw_port_switch_chk(struct adapter *adapter);
void report_wmm_edca_update(struct adapter *adapt);

void beacon_timing_control(struct adapter *adapt);
u8 chk_bmc_sleepq_cmd(struct adapter *adapt);
extern u8 set_tx_beacon_cmd(struct adapter *adapt);
unsigned int setup_beacon_frame(struct adapter *adapt, unsigned char *beacon_frame);
void update_mgnt_tx_rate(struct adapter *adapt, u8 rate);
void update_monitor_frame_attrib(struct adapter *adapt, struct pkt_attrib *pattrib);
void update_mgntframe_attrib(struct adapter *adapt, struct pkt_attrib *pattrib);
void update_mgntframe_attrib_addr(struct adapter *adapt, struct xmit_frame *pmgntframe);
void dump_mgntframe(struct adapter *adapt, struct xmit_frame *pmgntframe);
int dump_mgntframe_and_wait(struct adapter *adapt, struct xmit_frame *pmgntframe, int timeout_ms);
int dump_mgntframe_and_wait_ack(struct adapter *adapt, struct xmit_frame *pmgntframe);
int dump_mgntframe_and_wait_ack_timeout(struct adapter *adapt, struct xmit_frame *pmgntframe, int timeout_ms);

int get_reg_classes_full_count(struct p2p_channels *channel_list);
void issue_probersp_p2p(struct adapter *adapt, unsigned char *da);
void issue_p2p_provision_request(struct adapter *adapt, u8 *pssid, u8 ussidlen, u8 *pdev_raddr);
void issue_p2p_GO_request(struct adapter *adapt, u8 *raddr);
void issue_probereq_p2p(struct adapter *adapt, u8 *da);
int issue_probereq_p2p_ex(struct adapter *adapter, u8 *da, int try_cnt, int wait_ms);
void issue_p2p_invitation_response(struct adapter *adapt, u8 *raddr, u8 dialogToken, u8 success);
void issue_p2p_invitation_request(struct adapter *adapt, u8 *raddr);
void issue_beacon(struct adapter *adapt, int timeout_ms);
void issue_probersp(struct adapter *adapt, unsigned char *da, u8 is_valid_p2p_probereq);
void _issue_assocreq(struct adapter *adapt, u8 is_assoc);
void issue_assocreq(struct adapter *adapt);
void issue_reassocreq(struct adapter *adapt);
void issue_asocrsp(struct adapter *adapt, unsigned short status, struct sta_info *pstat, int pkt_type);
void issue_auth(struct adapter *adapt, struct sta_info *psta, unsigned short status);
void issue_probereq(struct adapter *adapt, struct ndis_802_11_ssid *pssid, u8 *da);
int issue_probereq_ex(struct adapter *adapt, struct ndis_802_11_ssid *pssid, u8 *da, u8 ch, bool append_wps, int try_cnt, int wait_ms);
int issue_nulldata(struct adapter *adapt, unsigned char *da, unsigned int power_mode, int try_cnt, int wait_ms);
int issue_qos_nulldata(struct adapter *adapt, unsigned char *da, u16 tid, int try_cnt, int wait_ms);
int issue_deauth(struct adapter *adapt, unsigned char *da, unsigned short reason);
int issue_deauth_ex(struct adapter *adapt, u8 *da, unsigned short reason, int try_cnt, int wait_ms);
void issue_action_spct_ch_switch(struct adapter *adapt, u8 *ra, u8 new_ch, u8 ch_offset);
void issue_addba_req(struct adapter *adapter, unsigned char *ra, u8 tid);
void issue_addba_rsp(struct adapter *adapter, unsigned char *ra, u8 tid, u16 status, u8 size);
u8 issue_addba_rsp_wait_ack(struct adapter *adapter, unsigned char *ra, u8 tid, u16 status, u8 size, int try_cnt, int wait_ms);
void issue_del_ba(struct adapter *adapter, unsigned char *ra, u8 tid, u16 reason, u8 initiator);
int issue_del_ba_ex(struct adapter *adapter, unsigned char *ra, u8 tid, u16 reason, u8 initiator, int try_cnt, int wait_ms);
void issue_action_BSSCoexistPacket(struct adapter *adapt);

#ifdef CONFIG_IEEE80211W
void issue_action_SA_Query(struct adapter *adapt, unsigned char *raddr, unsigned char action, unsigned short tid, u8 key_type);
int issue_deauth_11w(struct adapter *adapt, unsigned char *da, unsigned short reason, u8 key_type);
#endif /* CONFIG_IEEE80211W */
int issue_action_SM_PS(struct adapter *adapt ,  unsigned char *raddr , u8 NewMimoPsMode);
int issue_action_SM_PS_wait_ack(struct adapter *adapt, unsigned char *raddr, u8 NewMimoPsMode, int try_cnt, int wait_ms);

unsigned int send_delba_sta_tid(struct adapter *adapter, u8 initiator, struct sta_info *sta, u8 tid, u8 force);
unsigned int send_delba_sta_tid_wait_ack(struct adapter *adapter, u8 initiator, struct sta_info *sta, u8 tid, u8 force);

unsigned int send_delba(struct adapter *adapt, u8 initiator, u8 *addr);
unsigned int send_beacon(struct adapter *adapt);

void start_clnt_assoc(struct adapter *adapt);
void start_clnt_auth(struct adapter *adapt);
void start_clnt_join(struct adapter *adapt);
void start_create_ibss(struct adapter *adapt);

unsigned int OnAssocReq(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAssocRsp(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnProbeReq(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnProbeRsp(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int DoReserved(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnBeacon(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAtim(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnDisassoc(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAuth(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAuthClient(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnDeAuth(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction(struct adapter *adapt, union recv_frame *precv_frame);

unsigned int on_action_spct(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_qos(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_dls(struct adapter *adapt, union recv_frame *precv_frame);
#ifdef CONFIG_RTW_WNM
unsigned int on_action_wnm(struct adapter *adapter, union recv_frame *rframe);
#endif

#define RX_AMPDU_ACCEPT_INVALID 0xFF
#define RX_AMPDU_SIZE_INVALID 0xFF

enum rx_ampdu_reason {
	RX_AMPDU_DRV_FIXED = 1,
	RX_AMPDU_BTCOEX = 2, /* not used, because BTCOEX has its own variable management */
	RX_AMPDU_DRV_SCAN = 3,
};
u8 rtw_rx_ampdu_size(struct adapter *adapter);
bool rtw_rx_ampdu_is_accept(struct adapter *adapter);
bool rtw_rx_ampdu_set_size(struct adapter *adapter, u8 size, u8 reason);
bool rtw_rx_ampdu_set_accept(struct adapter *adapter, u8 accept, u8 reason);
u8 rx_ampdu_apply_sta_tid(struct adapter *adapter, struct sta_info *sta, u8 tid, u8 accept, u8 size);
u8 rx_ampdu_size_sta_limit(struct adapter *adapter, struct sta_info *sta);
u8 rx_ampdu_apply_sta(struct adapter *adapter, struct sta_info *sta, u8 accept, u8 size);
u16 rtw_rx_ampdu_apply(struct adapter *adapter);

unsigned int OnAction_back(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int on_action_public(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_ft(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_ht(struct adapter *adapt, union recv_frame *precv_frame);
#ifdef CONFIG_IEEE80211W
unsigned int OnAction_sa_query(struct adapter *adapt, union recv_frame *precv_frame);
#endif /* CONFIG_IEEE80211W */
unsigned int on_action_rm(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_wmm(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_vht(struct adapter *adapt, union recv_frame *precv_frame);
unsigned int OnAction_p2p(struct adapter *adapt, union recv_frame *precv_frame);

#ifdef CONFIG_RTW_80211R
void rtw_ft_update_bcn(struct adapter *adapt, union recv_frame *precv_frame);
void rtw_ft_start_clnt_join(struct adapter *adapt);
u8 rtw_ft_update_rsnie(struct adapter *adapt, u8 bwrite, 
	struct pkt_attrib *pattrib, u8 **pframe);
void rtw_ft_build_auth_req_ies(struct adapter *adapt, 
	struct pkt_attrib *pattrib, u8 **pframe);
void rtw_ft_build_assoc_req_ies(struct adapter *adapt, 
	u8 is_reassoc, struct pkt_attrib *pattrib, u8 **pframe);
u8 rtw_ft_update_auth_rsp_ies(struct adapter *adapt, u8 *pframe, u32 len);
void rtw_ft_start_roam(struct adapter *adapt, u8 *pTargetAddr);
void rtw_ft_issue_action_req(struct adapter *adapt, u8 *pTargetAddr);
void rtw_ft_report_evt(struct adapter *adapt);
void rtw_ft_report_reassoc_evt(struct adapter *adapt, u8 *pMacAddr);
void rtw_ft_link_timer_hdl(void *ctx);
void rtw_ft_roam_timer_hdl(void *ctx);
void rtw_ft_roam_status_reset(struct adapter *adapt);
#endif
#ifdef CONFIG_RTW_WNM
void rtw_wnm_roam_scan_hdl(void *ctx);
void rtw_wnm_process_btm_req(struct adapter *adapt,  u8* pframe, u32 frame_len);
void rtw_wnm_reset_btm_candidate(struct roam_nb_info *pnb);
void rtw_wnm_reset_btm_state(struct adapter *adapt);
void rtw_wnm_issue_action(struct adapter *adapt, u8 action, u8 reason);
#endif
#if defined(CONFIG_RTW_WNM)
u32 rtw_wnm_btm_candidates_survey(struct adapter *adapt, u8* pframe, u32 elem_len, u8 is_preference);
#endif
void mlmeext_joinbss_event_callback(struct adapter *adapt, int join_res);
void mlmeext_sta_del_event_callback(struct adapter *adapt);
void mlmeext_sta_add_event_callback(struct adapter *adapt, struct sta_info *psta);

void linked_status_chk(struct adapter *adapt, u8 from_timer);

void _linked_info_dump(struct adapter *adapt);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void survey_timer_hdl(struct timer_list *t);
#else
void survey_timer_hdl (void *FunctionContext);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void link_timer_hdl(struct timer_list *t);
void addba_timer_hdl(struct timer_list *t);
#else
void link_timer_hdl (void *FunctionContext);
void addba_timer_hdl(void *ctx);
#endif
#ifdef CONFIG_IEEE80211W
void sa_query_timer_hdl(void *ctx);
#endif /* CONFIG_IEEE80211W */

#define set_survey_timer(mlmeext, ms) \
	do { \
		/*RTW_INFO("%s set_survey_timer(%p, %d)\n", __func__, (mlmeext), (ms));*/ \
		_set_timer(&(mlmeext)->survey_timer, (ms)); \
	} while (0)

#define set_link_timer(mlmeext, ms) \
	do { \
		/*RTW_INFO("%s set_link_timer(%p, %d)\n", __func__, (mlmeext), (ms));*/ \
		_set_timer(&(mlmeext)->link_timer, (ms)); \
	} while (0)

extern int cckrates_included(unsigned char *rate, int ratelen);
extern int cckratesonly_included(unsigned char *rate, int ratelen);

extern void process_addba_req(struct adapter *adapt, u8 *paddba_req, u8 *addr);

extern void update_TSF(struct mlme_ext_priv *pmlmeext, u8 *pframe, uint len);
extern void correct_TSF(struct adapter *adapt, struct mlme_ext_priv *pmlmeext);
extern void adaptive_early_32k(struct mlme_ext_priv *pmlmeext, u8 *pframe, uint len);
extern u8 traffic_status_watchdog(struct adapter *adapt, u8 from_timer);

void rtw_process_bar_frame(struct adapter *adapt, union recv_frame *precv_frame);
void rtw_join_done_chk_ch(struct adapter *adapt, int join_res);

int rtw_chk_start_clnt_join(struct adapter *adapt, u8 *ch, u8 *bw, u8 *offset);

#ifdef CONFIG_PLATFORM_ARM_SUN8I
	#define BUSY_TRAFFIC_SCAN_DENY_PERIOD	8000
#else
	#define BUSY_TRAFFIC_SCAN_DENY_PERIOD	12000
#endif

struct cmd_hdl {
	uint	parmsize;
	u8(*h2cfuns)(struct adapter *adapt, u8 *pbuf);
};

void rtw_leave_opch(struct adapter *adapter);
void rtw_back_opch(struct adapter *adapter);

u8 read_macreg_hdl(struct adapter *adapt, u8 *pbuf);
u8 write_macreg_hdl(struct adapter *adapt, u8 *pbuf);
u8 read_bbreg_hdl(struct adapter *adapt, u8 *pbuf);
u8 write_bbreg_hdl(struct adapter *adapt, u8 *pbuf);
u8 read_rfreg_hdl(struct adapter *adapt, u8 *pbuf);
u8 write_rfreg_hdl(struct adapter *adapt, u8 *pbuf);


u8 NULL_hdl(struct adapter *adapt, u8 *pbuf);
u8 join_cmd_hdl(struct adapter *adapt, u8 *pbuf);
u8 disconnect_hdl(struct adapter *adapt, u8 *pbuf);
u8 createbss_hdl(struct adapter *adapt, u8 *pbuf);
u8 setopmode_hdl(struct adapter *adapt, u8 *pbuf);
u8 sitesurvey_cmd_hdl(struct adapter *adapt, u8 *pbuf);
u8 setauth_hdl(struct adapter *adapt, u8 *pbuf);
u8 setkey_hdl(struct adapter *adapt, u8 *pbuf);
u8 set_stakey_hdl(struct adapter *adapt, u8 *pbuf);
u8 set_assocsta_hdl(struct adapter *adapt, u8 *pbuf);
u8 del_assocsta_hdl(struct adapter *adapt, u8 *pbuf);
u8 add_ba_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 add_ba_rsp_hdl(struct adapter *adapt, unsigned char *pbuf);

void rtw_ap_wep_pk_setting(struct adapter *adapter, struct sta_info *psta);

u8 mlme_evt_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 h2c_msg_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 chk_bmc_sleepq_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 tx_beacon_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 rtw_set_chbw_hdl(struct adapter *adapt, u8 *pbuf);
u8 set_chplan_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 led_blink_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 set_csa_hdl(struct adapter *adapt, unsigned char *pbuf);	/* Kurt: Handling DFS channel switch announcement ie. */
u8 tdls_hdl(struct adapter *adapt, unsigned char *pbuf);
u8 run_in_thread_hdl(struct adapter *adapt, u8 *pbuf);
u8 rtw_getmacreg_hdl(struct adapter *adapt, u8 *pbuf);

#define GEN_DRV_CMD_HANDLER(size, cmd)	{size, &cmd ## _hdl},
#define GEN_MLME_EXT_HANDLER(size, cmd)	{size, cmd},

#ifdef _RTW_CMD_C_

static struct cmd_hdl wlancmds[] = {
	GEN_DRV_CMD_HANDLER(sizeof(struct readMAC_parm), rtw_getmacreg) /*0*/
	GEN_DRV_CMD_HANDLER(0, NULL)
	GEN_DRV_CMD_HANDLER(0, NULL)
	GEN_DRV_CMD_HANDLER(0, NULL)
	GEN_DRV_CMD_HANDLER(0, NULL)
	GEN_DRV_CMD_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL) /*10*/
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct joinbss_parm), join_cmd_hdl)  /*14*/
	GEN_MLME_EXT_HANDLER(sizeof(struct disconnect_parm), disconnect_hdl)
	GEN_MLME_EXT_HANDLER(sizeof(struct createbss_parm), createbss_hdl)
	GEN_MLME_EXT_HANDLER(sizeof(struct setopmode_parm), setopmode_hdl)
	GEN_MLME_EXT_HANDLER(sizeof(struct sitesurvey_parm), sitesurvey_cmd_hdl)  /*18*/
	GEN_MLME_EXT_HANDLER(sizeof(struct setauth_parm), setauth_hdl)
	GEN_MLME_EXT_HANDLER(sizeof(struct setkey_parm), setkey_hdl)  /*20*/
	GEN_MLME_EXT_HANDLER(sizeof(struct set_stakey_parm), set_stakey_hdl)
	GEN_MLME_EXT_HANDLER(sizeof(struct set_assocsta_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct del_assocsta_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct setstapwrstate_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct setbasicrate_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct getbasicrate_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct setdatarate_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct getdatarate_parm), NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)   /*30*/
	GEN_MLME_EXT_HANDLER(sizeof(struct setphy_parm), NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct getphy_parm), NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)	/*40*/
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct addBaReq_parm), add_ba_hdl)
	GEN_MLME_EXT_HANDLER(sizeof(struct set_ch_parm), rtw_set_chbw_hdl) /* 46 */
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL) /*50*/
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(0, NULL)
	GEN_MLME_EXT_HANDLER(sizeof(struct Tx_Beacon_param), tx_beacon_hdl) /*55*/

	GEN_MLME_EXT_HANDLER(0, mlme_evt_hdl) /*56*/
	GEN_MLME_EXT_HANDLER(0, rtw_drvextra_cmd_hdl) /*57*/

	GEN_MLME_EXT_HANDLER(0, h2c_msg_hdl) /*58*/
	GEN_MLME_EXT_HANDLER(sizeof(struct SetChannelPlan_param), set_chplan_hdl) /*59*/
	GEN_MLME_EXT_HANDLER(sizeof(struct LedBlink_param), led_blink_hdl) /*60*/

	GEN_MLME_EXT_HANDLER(sizeof(struct SetChannelSwitch_param), set_csa_hdl) /*61*/
	GEN_MLME_EXT_HANDLER(sizeof(struct TDLSoption_param), tdls_hdl) /*62*/
	GEN_MLME_EXT_HANDLER(0, chk_bmc_sleepq_hdl) /*63*/
	GEN_MLME_EXT_HANDLER(sizeof(struct RunInThread_param), run_in_thread_hdl) /*64*/
	GEN_MLME_EXT_HANDLER(sizeof(struct addBaRsp_parm), add_ba_rsp_hdl) /* 65 */
	GEN_MLME_EXT_HANDLER(sizeof(struct rm_event), rm_post_event_hdl) /* 66 */
};

#endif

struct C2HEvent_Header {

#if defined(__LITTLE_ENDIAN)

	unsigned int len:16;
	unsigned int ID:8;
	unsigned int seq:8;

#else

	unsigned int seq:8;
	unsigned int ID:8;
	unsigned int len:16;
#endif

	unsigned int rsvd;

};

void rtw_dummy_event_callback(struct adapter *adapter , u8 *pbuf);
void rtw_fwdbg_event_callback(struct adapter *adapter , u8 *pbuf);

enum rtw_c2h_event {
	GEN_EVT_CODE(_Read_MACREG) = 0, /*0*/
	GEN_EVT_CODE(_Read_BBREG),
	GEN_EVT_CODE(_Read_RFREG),
	GEN_EVT_CODE(_Read_EEPROM),
	GEN_EVT_CODE(_Read_EFUSE),
	GEN_EVT_CODE(_Read_CAM),			/*5*/
	GEN_EVT_CODE(_Get_BasicRate),
	GEN_EVT_CODE(_Get_DataRate),
	GEN_EVT_CODE(_Survey),	 /*8*/
	GEN_EVT_CODE(_SurveyDone),	 /*9*/

	GEN_EVT_CODE(_JoinBss) , /*10*/
	GEN_EVT_CODE(_AddSTA),
	GEN_EVT_CODE(_DelSTA),
	GEN_EVT_CODE(_AtimDone) ,
	GEN_EVT_CODE(_TX_Report),
	GEN_EVT_CODE(_CCX_Report),			/*15*/
	GEN_EVT_CODE(_DTM_Report),
	GEN_EVT_CODE(_TX_Rate_Statistics),
	GEN_EVT_CODE(_C2HLBK),
	GEN_EVT_CODE(_FWDBG),
	GEN_EVT_CODE(_C2HFEEDBACK),               /*20*/
	GEN_EVT_CODE(_ADDBA),
	GEN_EVT_CODE(_C2HBCN),
	GEN_EVT_CODE(_ReportPwrState),		/* filen: only for PCIE, USB	 */
	GEN_EVT_CODE(_CloseRF),				/* filen: only for PCIE, work around ASPM */
	GEN_EVT_CODE(_WMM),					/*25*/
#ifdef CONFIG_IEEE80211W
	GEN_EVT_CODE(_TimeoutSTA),
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_RTW_80211R
	GEN_EVT_CODE(_FT_REASSOC),
#endif
	MAX_C2HEVT
};


#ifdef _RTW_MLME_EXT_C_

static struct fwevent wlanevents[] = {
	{0, rtw_dummy_event_callback},	/*0*/
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, &rtw_survey_event_callback},		/*8*/
	{sizeof(struct surveydone_event), &rtw_surveydone_event_callback},	/*9*/

	{0, &rtw_joinbss_event_callback},		/*10*/
	{sizeof(struct stassoc_event), &rtw_stassoc_event_callback},
	{sizeof(struct stadel_event), &rtw_stadel_event_callback},
	{0, &rtw_atimdone_event_callback},
	{0, rtw_dummy_event_callback},
	{0, NULL},	/*15*/
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, rtw_fwdbg_event_callback},
	{0, NULL},	 /*20*/
	{0, NULL},
	{0, NULL},
	{0, &rtw_cpwm_event_callback},
	{0, NULL},
	{0, &rtw_wmm_event_callback}, /*25*/
#ifdef CONFIG_IEEE80211W
	{sizeof(struct stadel_event), &rtw_sta_timeout_event_callback},
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_RTW_80211R
	{sizeof(struct stassoc_event), &rtw_ft_reassoc_event_callback},
#endif
};

#endif/* _RTW_MLME_EXT_C_ */

#endif
