/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __STA_INFO_H_
#define __STA_INFO_H_

#include <rtw_sta_info.h>

#define IBSS_START_MAC_ID	2
#define NUM_STA MACID_NUM_SW_LIMIT

#ifndef CONFIG_RTW_MACADDR_ACL
	#define CONFIG_RTW_MACADDR_ACL 1
#endif

#ifndef CONFIG_RTW_PRE_LINK_STA
	#define CONFIG_RTW_PRE_LINK_STA 0
#endif

#define NUM_ACL 16
#define RTW_ACL_MODE_DISABLED				0
#define RTW_ACL_MODE_ACCEPT_UNLESS_LISTED	1
#define RTW_ACL_MODE_DENY_UNLESS_LISTED		2
#define RTW_ACL_MODE_MAX					3

#if CONFIG_RTW_MACADDR_ACL
extern const char *const _acl_mode_str[];
#define acl_mode_str(mode) (((mode) >= RTW_ACL_MODE_MAX) ? _acl_mode_str[RTW_ACL_MODE_DISABLED] : _acl_mode_str[(mode)])
#endif

#ifndef RTW_PRE_LINK_STA_NUM
	#define RTW_PRE_LINK_STA_NUM 8
#endif

struct pre_link_sta_node_t {
	u8 valid;
	u8 addr[ETH_ALEN];
};

struct pre_link_sta_ctl_t {
	spinlock_t lock;
	u8 num;
	struct pre_link_sta_node_t node[RTW_PRE_LINK_STA_NUM];
};

enum sta_info_update_type {
	STA_INFO_UPDATE_NONE = 0,
	STA_INFO_UPDATE_BW = BIT(0),
	STA_INFO_UPDATE_RATE = BIT(1),
	STA_INFO_UPDATE_PROTECTION_MODE = BIT(2),
	STA_INFO_UPDATE_CAP = BIT(3),
	STA_INFO_UPDATE_HT_CAP = BIT(4),
	STA_INFO_UPDATE_VHT_CAP = BIT(5),
	STA_INFO_UPDATE_ALL = STA_INFO_UPDATE_BW
			      | STA_INFO_UPDATE_RATE
			      | STA_INFO_UPDATE_PROTECTION_MODE
			      | STA_INFO_UPDATE_CAP
			      | STA_INFO_UPDATE_HT_CAP
			      | STA_INFO_UPDATE_VHT_CAP,
	STA_INFO_UPDATE_MAX
};

struct rtw_wlan_acl_node {
	struct list_head		        list;
	u8       addr[ETH_ALEN];
	u8       valid;
};

struct wlan_acl_pool {
	int mode;
	int num;
	struct rtw_wlan_acl_node aclnode[NUM_ACL];
	struct __queue	acl_node_q;
};

struct	stainfo_stats	{

	u64 rx_mgnt_pkts;
		u64 rx_beacon_pkts;
		u64 rx_probereq_pkts;
		u64 rx_probersp_pkts; /* unicast to self */
		u64 rx_probersp_bm_pkts;
		u64 rx_probersp_uo_pkts; /* unicast to others */
	u64 rx_ctrl_pkts;
	u64 rx_data_pkts;
		u64 rx_data_bc_pkts;
		u64 rx_data_mc_pkts;
	u64 rx_data_qos_pkts[TID_NUM]; /* unicast only */

	u64	last_rx_mgnt_pkts;
		u64 last_rx_beacon_pkts;
		u64 last_rx_probereq_pkts;
		u64 last_rx_probersp_pkts; /* unicast to self */
		u64 last_rx_probersp_bm_pkts;
		u64 last_rx_probersp_uo_pkts; /* unicast to others */
	u64	last_rx_ctrl_pkts;
	u64	last_rx_data_pkts;
		u64 last_rx_data_bc_pkts;
		u64 last_rx_data_mc_pkts;
	u64 last_rx_data_qos_pkts[TID_NUM]; /* unicast only */

	u64	rx_bytes;
		u64	rx_bc_bytes;
		u64	rx_mc_bytes;
	u64	last_rx_bytes;
		u64 last_rx_bc_bytes;
		u64 last_rx_mc_bytes;
	u64	rx_drops; /* TBD */
	u16 rx_tp_mbytes;

	u64	tx_pkts;
	u64	last_tx_pkts;

	u64	tx_bytes;
	u64	last_tx_bytes;
	u64 tx_drops; /* TBD */
	u16 tx_tp_mbytes;

	/* unicast only */
	u64 last_rx_data_uc_pkts; /* For Read & Clear requirement in proc_get_rx_stat() */
	u32 duplicate_cnt;	/* Read & Clear, in proc_get_rx_stat() */
	u32 rxratecnt[128];	/* Read & Clear, in proc_get_rx_stat() */
	u32 tx_ok_cnt;		/* Read & Clear, in proc_get_tx_stat() */
	u32 tx_fail_cnt;	/* Read & Clear, in proc_get_tx_stat() */
	u32 tx_retry_cnt;	/* Read & Clear, in proc_get_tx_stat() */
};

#ifndef DBG_SESSION_TRACKER
#define DBG_SESSION_TRACKER 0
#endif

/* session tracker status */
#define ST_STATUS_NONE		0
#define ST_STATUS_CHECK		BIT0
#define ST_STATUS_ESTABLISH	BIT1
#define ST_STATUS_EXPIRE	BIT2

#define ST_EXPIRE_MS (10 * 1000)

struct session_tracker {
	struct list_head list; /* session_tracker_queue */
	u32 local_naddr;
	__be16 local_port;
	__be16 remote_port;
	u32 remote_naddr;
	unsigned long set_time;
	u8 status;
};

/* session tracker cmd */
#define ST_CMD_ADD 0
#define ST_CMD_DEL 1
#define ST_CMD_CHK 2

struct st_cmd_parm {
	u8 cmd;
	struct sta_info *sta;
	u32 local_naddr; /* TODO: IPV6 */
	u16 local_port;
	u16 remote_port;
	u32 remote_naddr; /* TODO: IPV6 */
};

typedef bool (*st_match_rule)(struct adapter *adapter, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port);

struct st_register {
	u8 s_proto;
	st_match_rule rule;
};

#define SESSION_TRACKER_REG_ID_WFD 0
#define SESSION_TRACKER_REG_ID_NUM 1

struct st_ctl_t {
	struct st_register reg[SESSION_TRACKER_REG_ID_NUM];
	struct __queue tracker_q;
};

void rtw_st_ctl_init(struct st_ctl_t *st_ctl);
void rtw_st_ctl_deinit(struct st_ctl_t *st_ctl);
void rtw_st_ctl_register(struct st_ctl_t *st_ctl, u8 st_reg_id, struct st_register *reg);
void rtw_st_ctl_unregister(struct st_ctl_t *st_ctl, u8 st_reg_id);
bool rtw_st_ctl_chk_reg_s_proto(struct st_ctl_t *st_ctl, u8 s_proto);
bool rtw_st_ctl_chk_reg_rule(struct st_ctl_t *st_ctl, struct adapter *adapter, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port);
void rtw_st_ctl_rx(struct sta_info *sta, u8 *ehdr_pos);
void dump_st_ctl(void *sel, struct st_ctl_t *st_ctl);

struct sta_info {

	spinlock_t	lock;
	struct list_head	list; /* free_sta_queue */
	struct list_head	hash_list; /* sta_hash */
	/* _list asoc_list; */ /* 20061114 */
	/* _list sleep_list; */ /* sleep_q */
	/* _list wakeup_list; */ /* wakeup_q */
	struct adapter *adapt;
	struct cmn_sta_info cmn;

	struct sta_xmit_priv sta_xmitpriv;
	struct sta_recv_priv sta_recvpriv;

	struct __queue sleep_q;
	unsigned int sleepq_len;

	uint state;
	uint qos_option;
	u16 hwseq;
	uint	ieee8021x_blocked;	/* 0: allowed, 1:blocked */
	uint	dot118021XPrivacy; /* aes, tkip... */
	union Keytype	dot11tkiptxmickey;
	union Keytype	dot11tkiprxmickey;
	union Keytype	dot118021x_UncstKey;
	union pn48		dot11txpn;			/* PN48 used for Unicast xmit */
	union pn48		dot11rxpn;			/* PN48 used for Unicast recv. */
#ifdef CONFIG_GTK_OL
	u8 kek[RTW_KEK_LEN];
	u8 kck[RTW_KCK_LEN];
	u8 replay_ctr[RTW_REPLAY_CTR_LEN];
#endif /* CONFIG_GTK_OL */
#ifdef CONFIG_IEEE80211W
	struct timer_list dot11w_expire_timer;
#endif /* CONFIG_IEEE80211W */

	u8	bssrateset[16];
	u32	bssratelen;

	u8	cts2self;
	u8	rtsen;

	u8	init_rate;
	u8	wireless_mode;	/* NETWORK_TYPE */

	struct stainfo_stats sta_stats;

	/* for A-MPDU TX, ADDBA timeout check	 */
	struct timer_list addba_retry_timer;
	/* for A-MPDU Rx reordering buffer control */
	struct recv_reorder_ctrl recvreorder_ctrl[TID_NUM];
	ATOMIC_T continual_no_rx_packet[TID_NUM];
	/* for A-MPDU Tx */
	u16	BA_starting_seqctrl[16];
	struct ht_priv	htpriv;
	/* Notes:	 */
	/* STA_Mode: */
	/* curr_network(mlme_priv/security_priv/qos/ht) + sta_info: (STA & AP) CAP/INFO	 */
	/* scan_q: AP CAP/INFO */
	/* AP_Mode: */
	/* curr_network(mlme_priv/security_priv/qos/ht) : AP CAP/INFO */
	/* sta_info: (AP & STA) CAP/INFO */
	unsigned int expire_to;
	struct list_head asoc_list;
	struct list_head auth_list;
	unsigned int auth_seq;
	unsigned int authalg;
	unsigned char chg_txt[128];
	u16 capability;
	int flags;
	int dot8021xalg;/* 0:disable, 1:psk, 2:802.1x */
	int wpa_psk;/* 0:disable, bit(0): WPA, bit(1):WPA2 */
	int wpa_group_cipher;
	int wpa2_group_cipher;
	int wpa_pairwise_cipher;
	int wpa2_pairwise_cipher;
	u8 bpairwise_key_installed;
#ifdef CONFIG_RTW_80211R
	u8 ft_pairwise_key_installed;
#endif
	u8 wpa_ie[32];
	u8 nonerp_set;
	u8 no_short_slot_time_set;
	u8 no_short_preamble_set;
	u8 no_ht_gf_set;
	u8 no_ht_set;
	u8 ht_20mhz_set;
	u8 ht_40mhz_intolerant;
	u8 qos_info;
	u8 max_sp_len;
	u8 uapsd_bk;/* BIT(0): Delivery enabled, BIT(1): Trigger enabled */
	u8 uapsd_be;
	u8 uapsd_vi;
	u8 uapsd_vo;
	u8 has_legacy_ac;
	unsigned int sleepq_ac_len;

	/* p2p priv data */
	u8 is_p2p_device;
	u8 p2p_status_code;

	/* p2p client info */
	u8 dev_addr[ETH_ALEN];
	/* u8 iface_addr[ETH_ALEN]; */ /* = hwaddr[ETH_ALEN] */
	u8 dev_cap;
	u16 config_methods;
	u8 primary_dev_type[8];
	u8 num_of_secdev_type;
	u8 secdev_types_list[32];/* 32/8 == 4; */
	u16 dev_name_len;
	u8 dev_name[32];

	u8 op_wfd_mode;

	u8 under_exist_checking;

	u8 keep_alive_trycnt;

	u8 *passoc_req;
	u32 assoc_req_len;

	u8		IOTPeer;			/* Enum value.	enum ht_iot_peer */

	/* To store the sequence number of received management frame */
	u16 RxMgmtFrameSeqNum;

	struct st_ctl_t st_ctl;
	u8 max_agg_num_minimal_record; /*keep minimal tx desc max_agg_num setting*/
	u8 curr_rx_rate;
	u8 curr_rx_rate_bmc;
};

#define sta_tx_pkts(sta) \
	(sta->sta_stats.tx_pkts)

#define sta_last_tx_pkts(sta) \
	(sta->sta_stats.last_tx_pkts)

#define sta_rx_pkts(sta) \
	(sta->sta_stats.rx_mgnt_pkts \
	 + sta->sta_stats.rx_ctrl_pkts \
	 + sta->sta_stats.rx_data_pkts)

#define sta_last_rx_pkts(sta) \
	(sta->sta_stats.last_rx_mgnt_pkts \
	 + sta->sta_stats.last_rx_ctrl_pkts \
	 + sta->sta_stats.last_rx_data_pkts)

#define sta_rx_data_pkts(sta) (sta->sta_stats.rx_data_pkts)
#define sta_last_rx_data_pkts(sta) (sta->sta_stats.last_rx_data_pkts)

#define sta_rx_data_uc_pkts(sta) (sta->sta_stats.rx_data_pkts - sta->sta_stats.rx_data_bc_pkts - sta->sta_stats.rx_data_mc_pkts)
#define sta_last_rx_data_uc_pkts(sta) (sta->sta_stats.last_rx_data_pkts - sta->sta_stats.last_rx_data_bc_pkts - sta->sta_stats.last_rx_data_mc_pkts)

#define sta_rx_data_qos_pkts(sta, i) \
	(sta->sta_stats.rx_data_qos_pkts[i])

#define sta_last_rx_data_qos_pkts(sta, i) \
	(sta->sta_stats.last_rx_data_qos_pkts[i])

#define sta_rx_mgnt_pkts(sta) \
	(sta->sta_stats.rx_mgnt_pkts)

#define sta_last_rx_mgnt_pkts(sta) \
	(sta->sta_stats.last_rx_mgnt_pkts)

#define sta_rx_beacon_pkts(sta) \
	(sta->sta_stats.rx_beacon_pkts)

#define sta_last_rx_beacon_pkts(sta) \
	(sta->sta_stats.last_rx_beacon_pkts)

#define sta_rx_probereq_pkts(sta) \
	(sta->sta_stats.rx_probereq_pkts)

#define sta_last_rx_probereq_pkts(sta) \
	(sta->sta_stats.last_rx_probereq_pkts)

#define sta_rx_probersp_pkts(sta) \
	(sta->sta_stats.rx_probersp_pkts)

#define sta_last_rx_probersp_pkts(sta) \
	(sta->sta_stats.last_rx_probersp_pkts)

#define sta_rx_probersp_bm_pkts(sta) \
	(sta->sta_stats.rx_probersp_bm_pkts)

#define sta_last_rx_probersp_bm_pkts(sta) \
	(sta->sta_stats.last_rx_probersp_bm_pkts)

#define sta_rx_probersp_uo_pkts(sta) \
	(sta->sta_stats.rx_probersp_uo_pkts)

#define sta_last_rx_probersp_uo_pkts(sta) \
	(sta->sta_stats.last_rx_probersp_uo_pkts)

#define sta_update_last_rx_pkts(sta) \
	do { \
		int __i; \
		\
		sta->sta_stats.last_rx_mgnt_pkts = sta->sta_stats.rx_mgnt_pkts; \
		sta->sta_stats.last_rx_beacon_pkts = sta->sta_stats.rx_beacon_pkts; \
		sta->sta_stats.last_rx_probereq_pkts = sta->sta_stats.rx_probereq_pkts; \
		sta->sta_stats.last_rx_probersp_pkts = sta->sta_stats.rx_probersp_pkts; \
		sta->sta_stats.last_rx_probersp_bm_pkts = sta->sta_stats.rx_probersp_bm_pkts; \
		sta->sta_stats.last_rx_probersp_uo_pkts = sta->sta_stats.rx_probersp_uo_pkts; \
		sta->sta_stats.last_rx_ctrl_pkts = sta->sta_stats.rx_ctrl_pkts; \
		\
		sta->sta_stats.last_rx_data_pkts = sta->sta_stats.rx_data_pkts; \
		sta->sta_stats.last_rx_data_bc_pkts = sta->sta_stats.rx_data_bc_pkts; \
		sta->sta_stats.last_rx_data_mc_pkts = sta->sta_stats.rx_data_mc_pkts; \
		for (__i = 0; __i < TID_NUM; __i++) \
			sta->sta_stats.last_rx_data_qos_pkts[__i] = sta->sta_stats.rx_data_qos_pkts[__i]; \
	} while (0)

#define STA_RX_PKTS_ARG(sta) \
	sta->sta_stats.rx_mgnt_pkts \
	, sta->sta_stats.rx_ctrl_pkts \
	, sta->sta_stats.rx_data_pkts

#define STA_LAST_RX_PKTS_ARG(sta) \
	sta->sta_stats.last_rx_mgnt_pkts \
	, sta->sta_stats.last_rx_ctrl_pkts \
	, sta->sta_stats.last_rx_data_pkts

#define STA_RX_PKTS_DIFF_ARG(sta) \
	sta->sta_stats.rx_mgnt_pkts - sta->sta_stats.last_rx_mgnt_pkts \
	, sta->sta_stats.rx_ctrl_pkts - sta->sta_stats.last_rx_ctrl_pkts \
	, sta->sta_stats.rx_data_pkts - sta->sta_stats.last_rx_data_pkts

#define STA_PKTS_FMT "(m:%llu, c:%llu, d:%llu)"

#define sta_rx_uc_bytes(sta) (sta->sta_stats.rx_bytes - sta->sta_stats.rx_bc_bytes - sta->sta_stats.rx_mc_bytes)
#define sta_last_rx_uc_bytes(sta) (sta->sta_stats.last_rx_bytes - sta->sta_stats.last_rx_bc_bytes - sta->sta_stats.last_rx_mc_bytes)

#define STA_OP_WFD_MODE(sta) (sta)->op_wfd_mode
#define STA_SET_OP_WFD_MODE(sta, mode) (sta)->op_wfd_mode = (mode)
#define AID_BMP_LEN(max_aid) ((max_aid + 1) / 8 + (((max_aid + 1) % 8) ? 1 : 0))

struct	sta_priv {
	u8 *pallocated_stainfo_buf;
	u8 *pstainfo_buf;
	struct __queue	free_sta_queue;

	spinlock_t sta_hash_lock;
	struct list_head   sta_hash[NUM_STA];
	int asoc_sta_count;
	struct __queue sleep_q;
	struct __queue wakeup_q;

	struct adapter *adapt;

	u32 adhoc_expire_to;

	struct list_head asoc_list;
	struct list_head auth_list;
	spinlock_t asoc_list_lock;
	spinlock_t auth_list_lock;
	u8 asoc_list_cnt;
	u8 auth_list_cnt;

	unsigned int auth_to;  /* sec, time to expire in authenticating. */
	unsigned int assoc_to; /* sec, time to expire before associating. */
	unsigned int expire_to; /* sec , time to expire after associated. */

	/*
	* pointers to STA info; based on allocated AID or NULL if AID free
	* AID is in the range 1-2007, so sta_aid[0] corresponders to AID 1
	*/
	struct sta_info **sta_aid;
	u16 max_aid;
	u16 started_aid; /* started AID for allocation search */
	bool rr_aid; /* round robin AID allocation, will modify started_aid */
	u8 aid_bmp_len; /* in byte */
	u8 *sta_dz_bitmap;
	u8 *tim_bitmap;

	u16 max_num_sta;

#if CONFIG_RTW_MACADDR_ACL
	struct wlan_acl_pool acl_list;
#endif

	#if CONFIG_RTW_PRE_LINK_STA
	struct pre_link_sta_ctl_t pre_link_sta_ctl;
	#endif

	struct sta_info *c2h_sta;
	struct submit_ctx *gotc2h;
};

static inline u32 wifi_mac_hash(const u8 *mac)
{
	u32 x;

	x = mac[0];
	x = (x << 2) ^ mac[1];
	x = (x << 2) ^ mac[2];
	x = (x << 2) ^ mac[3];
	x = (x << 2) ^ mac[4];
	x = (x << 2) ^ mac[5];

	x ^= x >> 8;
	x  = x & (NUM_STA - 1);

	return x;
}


extern u32	_rtw_init_sta_priv(struct sta_priv *pstapriv);
extern u32	_rtw_free_sta_priv(struct sta_priv *pstapriv);

#define stainfo_offset_valid(offset) (offset < NUM_STA && offset >= 0)
int rtw_stainfo_offset(struct sta_priv *stapriv, struct sta_info *sta);
struct sta_info *rtw_get_stainfo_by_offset(struct sta_priv *stapriv, int offset);

extern struct sta_info *rtw_alloc_stainfo(struct	sta_priv *pstapriv, const u8 *hwaddr);
extern u32	rtw_free_stainfo(struct adapter *adapt , struct sta_info *psta);
extern void rtw_free_all_stainfo(struct adapter *adapt);
extern struct sta_info *rtw_get_stainfo(struct sta_priv *pstapriv, const u8 *hwaddr);
extern u32 rtw_init_bcmc_stainfo(struct adapter *adapt);
extern struct sta_info *rtw_get_bcmc_stainfo(struct adapter *adapt);

u16 rtw_aid_alloc(struct adapter *adapter, struct sta_info *sta);
void dump_aid_status(void *sel, struct adapter *adapter);

#if CONFIG_RTW_MACADDR_ACL
extern u8 rtw_access_ctrl(struct adapter *adapter, u8 *mac_addr);
void dump_macaddr_acl(void *sel, struct adapter *adapter);
#endif

bool rtw_is_pre_link_sta(struct sta_priv *stapriv, u8 *addr);
#if CONFIG_RTW_PRE_LINK_STA
struct sta_info *rtw_pre_link_sta_add(struct sta_priv *stapriv, u8 *hwaddr);
void rtw_pre_link_sta_del(struct sta_priv *stapriv, u8 *hwaddr);
void rtw_pre_link_sta_ctl_reset(struct sta_priv *stapriv);
void rtw_pre_link_sta_ctl_init(struct sta_priv *stapriv);
void rtw_pre_link_sta_ctl_deinit(struct sta_priv *stapriv);
void dump_pre_link_sta_ctl(void *sel, struct sta_priv *stapriv);
#endif /* CONFIG_RTW_PRE_LINK_STA */

#endif /* _STA_INFO_H_ */
