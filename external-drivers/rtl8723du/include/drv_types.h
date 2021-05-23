/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/*-------------------------------------------------------------------------------

	For type defines and data structure defines

--------------------------------------------------------------------------------*/


#ifndef __DRV_TYPES_H__
#define __DRV_TYPES_H__

#include <drv_conf.h>
#include <basic_types.h>
#include <osdep_service.h>
#include <wlan_bssdef.h>
#include <wifi.h>
#include <ieee80211.h>
#ifdef CONFIG_ARP_KEEP_ALIVE
	#include <net/neighbour.h>
	#include <net/arp.h>
#endif

#define DRIVERVERSION	"v5.2.15.2_27977.20180529_COEX20171212-2222"
#define BTCOEXVERSION	"COEX20171212-2222"

extern u8 rtw_bridge_tunnel_header[];
extern u8 rtw_rfc1042_header[];

enum _NIC_VERSION {
	RTL8711_NIC,
	RTL8712_NIC,
	RTL8713_NIC,
	RTL8716_NIC
};

struct adapter;

#include <rtw_debug.h>
#include <rtw_sta_info.h>
#include <rtw_rf.h>
#include <rtw_ht.h>

#ifdef CONFIG_INTEL_WIDI
	#include <rtw_intel_widi.h>
#endif

#include <rtw_cmd.h>
#include <cmd_osdep.h>
#include <rtw_security.h>
#include <rtw_xmit.h>
#include <xmit_osdep.h>
#include <rtw_recv.h>
#include <rtw_rm.h>

#include <recv_osdep.h>
#include <rtw_efuse.h>
#include <rtw_sreset.h>
#include <hal_intf.h>
#include <hal_com.h>
#include<hal_com_h2c.h>
#include <hal_com_led.h>
#include "../hal/hal_dm.h"
#include <rtw_qos.h>
#include <rtw_pwrctrl.h>
#include <rtw_mlme.h>
#include <mlme_osdep.h>
#include <rtw_io.h>
#include <rtw_ioctl.h>
#include <rtw_ioctl_set.h>
#include <rtw_ioctl_rtl.h>
#include <osdep_intf.h>
#include <rtw_eeprom.h>
#include <sta_info.h>
#include <rtw_event.h>
#include <rtw_mlme_ext.h>
#include <rtw_mi.h>
#include <rtw_ap.h>
#include <rtw_efuse.h>
#include <rtw_odm.h>

#include <rtw_p2p.h>

#include <rtw_br_ext.h>

#include <ip.h>
#include <if_ether.h>
#include <ethernet.h>

#include <rtw_android.h>

#include <rtw_btcoex_wifionly.h>
#include <rtw_btcoex.h>

#define SPEC_DEV_ID_NONE BIT(0)
#define SPEC_DEV_ID_DISABLE_HT BIT(1)
#define SPEC_DEV_ID_ENABLE_PS BIT(2)
#define SPEC_DEV_ID_RF_CONFIG_1T1R BIT(3)
#define SPEC_DEV_ID_RF_CONFIG_2T2R BIT(4)
#define SPEC_DEV_ID_ASSIGN_IFNAME BIT(5)

#define bRFRegOffsetMask		0xfffff

struct specific_device_id {

	u32		flags;

	u16		idVendor;
	u16		idProduct;

};

struct registry_priv {
	u8	chip_version;
	u8	rfintfs;
	u8	lbkmode;
	u8	hci;
	struct ndis_802_11_ssid	ssid;
	u8	network_mode;	/* infra, ad-hoc, auto */
	u8	channel;/* ad-hoc support requirement */
	u8	wireless_mode;/* A, B, G, auto */
	u8	scan_mode;/* active, passive */
	u8	radio_enable;
	u8	preamble;/* long, short, auto */
	u8	vrtl_carrier_sense;/* Enable, Disable, Auto */
	u8	vcs_type;/* RTS/CTS, CTS-to-self */
	u16	rts_thresh;
	u16  frag_thresh;
	u8	adhoc_tx_pwr;
	u8	soft_ap;
	u8	power_mgnt;
	u8	ips_mode;
	u8	lps_level;
	u8	smart_ps;
	u8   usb_rxagg_mode;
	u8	dynamic_agg_enable;
	u8	long_retry_lmt;
	u8	short_retry_lmt;
	u16	busy_thresh;
	u8	ack_policy;
	u8	mp_mode;
	u8  mp_dm;
	u8	software_encrypt;
	u8	software_decrypt;
	u8	acm_method;
	/* WMM */
	u8	wmm_enable;
	struct wlan_bssid_ex    dev_network;

	u8 tx_bw_mode;
	u8 bmc_tx_rate;
	u8	ht_enable;
	/* 0: 20 MHz, 1: 40 MHz, 2: 80 MHz, 3: 160MHz */
	/* 2.4G use bit 0 ~ 3, 5G use bit 4 ~ 7 */
	/* 0x21 means enable 2.4G 40MHz & 5G 80MHz */
	u8	bw_mode;
	u8	ampdu_enable;/* for tx */
	u8	rx_stbc;
	u8	rx_ampdu_amsdu;/* Rx A-MPDU Supports A-MSDU is permitted */
	u8	tx_ampdu_amsdu;/* Tx A-MPDU Supports A-MSDU is permitted */
	u8 rx_ampdu_sz_limit_by_nss_bw[4][4]; /* 1~4SS, BW20~BW160 */
	/* Short GI support Bit Map */
	/* BIT0 - 20MHz, 1: support, 0: non-support */
	/* BIT1 - 40MHz, 1: support, 0: non-support */
	/* BIT2 - 80MHz, 1: support, 0: non-support */
	/* BIT3 - 160MHz, 1: support, 0: non-support */
	u8	short_gi;
	/* BIT0: Enable VHT LDPC Rx, BIT1: Enable VHT LDPC Tx, BIT4: Enable HT LDPC Rx, BIT5: Enable HT LDPC Tx */
	u8	ldpc_cap;
	/* BIT0: Enable VHT STBC Rx, BIT1: Enable VHT STBC Tx, BIT4: Enable HT STBC Rx, BIT5: Enable HT STBC Tx */
	u8	stbc_cap;
	/*
	 * BIT0: Enable VHT SU Beamformer
	 * BIT1: Enable VHT SU Beamformee
	 * BIT2: Enable VHT MU Beamformer, depend on VHT SU Beamformer
	 * BIT3: Enable VHT MU Beamformee, depend on VHT SU Beamformee
	 * BIT4: Enable HT Beamformer
	 * BIT5: Enable HT Beamformee
	 */
	u8	beamform_cap;
	u8	beamformer_rf_num;
	u8	beamformee_rf_num;

	u8	lowrate_two_xmit;

	u8	rf_config ;
	u8	low_power ;

	u8	wifi_spec;/* !turbo_mode */
	u8	special_rf_path; /* 0: 2T2R ,1: only turn on path A 1T1R */
	char alpha2[2];
	u8	channel_plan;
	u8	excl_chs[MAX_CHANNEL_NUM];
	u8	full_ch_in_p2p_handshake; /* 0: reply only softap channel, 1: reply full channel list*/

	u8	btcoex;
	u8	bt_iso;
	u8	bt_sco;
	u8	bt_ampdu;
	u8	ant_num;
	u8	single_ant_path;
	bool	bAcceptAddbaReq;

	u8	antdiv_cfg;
	u8	antdiv_type;
	u8	drv_ant_band_switch;

	u8	switch_usb_mode;

	u8	usbss_enable;/* 0:disable,1:enable */
	u8	hwpdn_mode;/* 0:disable,1:enable,2:decide by EFUSE config */
	u8	hwpwrp_detect;/* 0:disable,1:enable */

	u8	hw_wps_pbc;/* 0:disable,1:enable */

	u8	max_roaming_times; /* the max number driver will try to roaming */

	u8 enable80211d;

	u8 ifname[16];
	u8 if2name[16];

	u8 notch_filter;

	/* for pll reference clock selction */
	u8 pll_ref_clk_sel;

	/* define for tx power adjust */
#ifdef CONFIG_TXPWR_LIMIT
	u8	RegEnableTxPowerLimit;
#endif
	u8	RegEnableTxPowerByRate;

	u8 target_tx_pwr_valid;
	s8 target_tx_pwr_2g[RF_PATH_MAX][RATE_SECTION_NUM];

	s8	TxBBSwing_2G;
	s8	TxBBSwing_5G;
	u8	AmplifierType_2G;
	u8	AmplifierType_5G;
	u8	bEn_RFE;
	u8	RFE_Type;
	u8	PowerTracking_Type;
	u8	GLNA_Type;
	u8  check_fw_ps;
	u8	RegPwrTrimEnable;

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	u8	load_phy_file;
	u8	RegDecryptCustomFile;
#endif
#ifdef CONFIG_CONCURRENT_MODE
	u8 virtual_iface_num;
#endif
	u8 qos_opt_enable;

	u8 hiq_filter;
	u8 adaptivity_en;
	u8 adaptivity_mode;
	u8 adaptivity_dml;
	u8 adaptivity_dc_backoff;
	s8 adaptivity_th_l2h_ini;
	s8 adaptivity_th_edcca_hl_diff;

	u8 boffefusemask;
	bool bFileMaskEfuse;

	u32	reg_rxgain_offset_2g;
	u32	reg_rxgain_offset_5gl;
	u32	reg_rxgain_offset_5gm;
	u32	reg_rxgain_offset_5gh;

#ifdef CONFIG_RTW_NAPI
	u8 en_napi;
#ifdef CONFIG_RTW_NAPI_DYNAMIC
	u32 napi_threshold;	/* unit: Mbps */
#endif /* CONFIG_RTW_NAPI_DYNAMIC */
	u8 en_gro;
#endif /* CONFIG_RTW_NAPI */

	u8 check_hw_status;

	u32 pci_aspm_config;

	u8 iqk_fw_offload;
	u8 ch_switch_offload;

#ifdef CONFIG_ADVANCE_OTA
	u8	adv_ota;
#endif

#ifdef CONFIG_FW_OFFLOAD_PARAM_INIT
	u8 fw_param_init;
#endif
};

/* For registry parameters */
#define RGTRY_OFT(field) ((u32)FIELD_OFFSET(struct registry_priv, field))
#define RGTRY_SZ(field)   sizeof(((struct registry_priv *) 0)->field)

#define GetRegAmplifierType2G(_Adapter)	(_Adapter->registrypriv.AmplifierType_2G)
#define GetRegAmplifierType5G(_Adapter)	(_Adapter->registrypriv.AmplifierType_5G)

#define GetRegTxBBSwing_2G(_Adapter)	(_Adapter->registrypriv.TxBBSwing_2G)
#define GetRegTxBBSwing_5G(_Adapter)	(_Adapter->registrypriv.TxBBSwing_5G)

#define GetRegbENRFEType(_Adapter)	(_Adapter->registrypriv.bEn_RFE)
#define GetRegRFEType(_Adapter)	(_Adapter->registrypriv.RFE_Type)
#define GetRegGLNAType(_Adapter)	(_Adapter->registrypriv.GLNA_Type)
#define GetRegPowerTrackingType(_Adapter)	(_Adapter->registrypriv.PowerTracking_Type)

#define BSSID_OFT(field) ((u32)FIELD_OFFSET(struct wlan_bssid_ex, field))
#define BSSID_SZ(field)   sizeof(((struct wlan_bssid_ex *) 0)->field)

#define BW_MODE_2G(bw_mode) ((bw_mode) & 0x0F)
#define BW_MODE_5G(bw_mode) ((bw_mode) >> 4)
#define REGSTY_BW_2G(regsty) BW_MODE_2G((regsty)->bw_mode)
#define REGSTY_BW_5G(regsty) BW_MODE_5G((regsty)->bw_mode)
#define REGSTY_IS_BW_2G_SUPPORT(regsty, bw) (REGSTY_BW_2G((regsty)) >= (bw))
#define REGSTY_IS_BW_5G_SUPPORT(regsty, bw) (REGSTY_BW_5G((regsty)) >= (bw))

#define REGSTY_IS_11AC_ENABLE(regsty) ((regsty)->vht_enable != 0)
#define REGSTY_IS_11AC_AUTO(regsty) ((regsty)->vht_enable == 2)

#ifdef CONFIG_CONCURRENT_MODE
	#define is_primary_adapter(adapter) (adapter->adapter_type == PRIMARY_ADAPTER)
	#define is_vir_adapter(_adapter) (_adapter->adapter_type == VIRTUAL_ADAPTER)
	#define get_hw_port(adapter) (adapter->hw_port)
#else
	#define is_primary_adapter(adapter) (1)
	#define is_virstruct adapter(adapter) (0)
	#define get_hw_port(adapter) (HW_PORT0)
#endif
#define GET_PRIMARY_ADAPTER(adapt) (((struct adapter *)adapt)->dvobj->adapters[IFACE_ID0])
#define GET_IFACE_NUMS(adapt) (((struct adapter *)adapt)->dvobj->iface_nums)
#define GET_ADAPTER(adapt, iface_id) (((struct adapter *)adapt)->dvobj->adapters[iface_id])

#define GetDefaultAdapter(adapt)	adapt

enum _IFACE_ID {
	IFACE_ID0, /*PRIMARY_ADAPTER*/
	IFACE_ID1,
	IFACE_ID2,
	IFACE_ID3,
	IFACE_ID4,
	IFACE_ID5,
	IFACE_ID6,
	IFACE_ID7,
	IFACE_ID_MAX,
};

#define VIF_START_ID	1

#ifdef CONFIG_DBG_COUNTER

struct rx_logs {
	u32 intf_rx;
	u32 intf_rx_err_recvframe;
	u32 intf_rx_err_skb;
	u32 intf_rx_report;
	u32 core_rx;
	u32 core_rx_pre;
	u32 core_rx_pre_ver_err;
	u32 core_rx_pre_mgmt;
	u32 core_rx_pre_mgmt_err_80211w;
	u32 core_rx_pre_mgmt_err;
	u32 core_rx_pre_ctrl;
	u32 core_rx_pre_ctrl_err;
	u32 core_rx_pre_data;
	u32 core_rx_pre_data_wapi_seq_err;
	u32 core_rx_pre_data_wapi_key_err;
	u32 core_rx_pre_data_handled;
	u32 core_rx_pre_data_err;
	u32 core_rx_pre_data_unknown;
	u32 core_rx_pre_unknown;
	u32 core_rx_enqueue;
	u32 core_rx_dequeue;
	u32 core_rx_post;
	u32 core_rx_post_decrypt;
	u32 core_rx_post_decrypt_wep;
	u32 core_rx_post_decrypt_tkip;
	u32 core_rx_post_decrypt_aes;
	u32 core_rx_post_decrypt_wapi;
	u32 core_rx_post_decrypt_hw;
	u32 core_rx_post_decrypt_unknown;
	u32 core_rx_post_decrypt_err;
	u32 core_rx_post_defrag_err;
	u32 core_rx_post_portctrl_err;
	u32 core_rx_post_indicate;
	u32 core_rx_post_indicate_in_oder;
	u32 core_rx_post_indicate_reoder;
	u32 core_rx_post_indicate_err;
	u32 os_indicate;
	u32 os_indicate_ap_mcast;
	u32 os_indicate_ap_forward;
	u32 os_indicate_ap_self;
	u32 os_indicate_err;
	u32 os_netif_ok;
	u32 os_netif_err;
};

struct tx_logs {
	u32 os_tx;
	u32 os_tx_err_up;
	u32 os_tx_err_xmit;
	u32 os_tx_m2u;
	u32 os_tx_m2u_ignore_fw_linked;
	u32 os_tx_m2u_ignore_self;
	u32 os_tx_m2u_entry;
	u32 os_tx_m2u_entry_err_xmit;
	u32 os_tx_m2u_entry_err_skb;
	u32 os_tx_m2u_stop;
	u32 core_tx;
	u32 core_tx_err_pxmitframe;
	u32 core_tx_err_brtx;
	u32 core_tx_upd_attrib;
	u32 core_tx_upd_attrib_adhoc;
	u32 core_tx_upd_attrib_sta;
	u32 core_tx_upd_attrib_ap;
	u32 core_tx_upd_attrib_unknown;
	u32 core_tx_upd_attrib_dhcp;
	u32 core_tx_upd_attrib_icmp;
	u32 core_tx_upd_attrib_active;
	u32 core_tx_upd_attrib_err_ucast_sta;
	u32 core_tx_upd_attrib_err_ucast_ap_link;
	u32 core_tx_upd_attrib_err_sta;
	u32 core_tx_upd_attrib_err_link;
	u32 core_tx_upd_attrib_err_sec;
	u32 core_tx_ap_enqueue_warn_fwstate;
	u32 core_tx_ap_enqueue_warn_sta;
	u32 core_tx_ap_enqueue_warn_nosta;
	u32 core_tx_ap_enqueue_warn_link;
	u32 core_tx_ap_enqueue_warn_trigger;
	u32 core_tx_ap_enqueue_mcast;
	u32 core_tx_ap_enqueue_ucast;
	u32 core_tx_ap_enqueue;
	u32 intf_tx;
	u32 intf_tx_pending_ac;
	u32 intf_tx_pending_fw_under_survey;
	u32 intf_tx_pending_fw_under_linking;
	u32 intf_tx_pending_xmitbuf;
	u32 intf_tx_enqueue;
	u32 core_tx_enqueue;
	u32 core_tx_enqueue_class;
	u32 core_tx_enqueue_class_err_sta;
	u32 core_tx_enqueue_class_err_nosta;
	u32 core_tx_enqueue_class_err_fwlink;
	u32 intf_tx_direct;
	u32 intf_tx_direct_err_coalesce;
	u32 intf_tx_dequeue;
	u32 intf_tx_dequeue_err_coalesce;
	u32 intf_tx_dump_xframe;
	u32 intf_tx_dump_xframe_err_txdesc;
	u32 intf_tx_dump_xframe_err_port;
};

struct int_logs {
	u32 all;
	u32 err;
	u32 tbdok;
	u32 tbder;
	u32 bcnderr;
	u32 bcndma;
	u32 bcndma_e;
	u32 rx;
	u32 rx_rdu;
	u32 rx_fovw;
	u32 txfovw;
	u32 mgntok;
	u32 highdok;
	u32 bkdok;
	u32 bedok;
	u32 vidok;
	u32 vodok;
};

#endif /* CONFIG_DBG_COUNTER */

struct debug_priv {
	u32 dbg_sdio_free_irq_error_cnt;
	u32 dbg_sdio_alloc_irq_error_cnt;
	u32 dbg_sdio_free_irq_cnt;
	u32 dbg_sdio_alloc_irq_cnt;
	u32 dbg_sdio_deinit_error_cnt;
	u32 dbg_sdio_init_error_cnt;
	u32 dbg_suspend_error_cnt;
	u32 dbg_suspend_cnt;
	u32 dbg_resume_cnt;
	u32 dbg_resume_error_cnt;
	u32 dbg_deinit_fail_cnt;
	u32 dbg_carddisable_cnt;
	u32 dbg_carddisable_error_cnt;
	u32 dbg_ps_insuspend_cnt;
	u32 dbg_dev_unload_inIPS_cnt;
	u32 dbg_wow_leave_ps_fail_cnt;
	u32 dbg_scan_pwr_state_cnt;
	u32 dbg_downloadfw_pwr_state_cnt;
	u32 dbg_fw_read_ps_state_fail_cnt;
	u32 dbg_leave_ips_fail_cnt;
	u32 dbg_leave_lps_fail_cnt;
	u32 dbg_h2c_leave32k_fail_cnt;
	u32 dbg_diswow_dload_fw_fail_cnt;
	u32 dbg_enwow_dload_fw_fail_cnt;
	u32 dbg_ips_drvopen_fail_cnt;
	u32 dbg_poll_fail_cnt;
	u32 dbg_rpwm_toogle_cnt;
	u32 dbg_rpwm_timeout_fail_cnt;
	u32 dbg_sreset_cnt;
	u32 dbg_fw_mem_dl_error_cnt;
	u64 dbg_rx_fifo_last_overflow;
	u64 dbg_rx_fifo_curr_overflow;
	u64 dbg_rx_fifo_diff_overflow;
};

struct rtw_traffic_statistics {
	/* tx statistics */
	u64	tx_bytes;
	u64	tx_pkts;
	u64	tx_drop;
	u64	cur_tx_bytes;
	u64	last_tx_bytes;
	u32	cur_tx_tp; /* Tx throughput in MBps. */

	/* rx statistics */
	u64	rx_bytes;
	u64	rx_pkts;
	u64	rx_drop;
	u64	cur_rx_bytes;
	u64	last_rx_bytes;
	u32	cur_rx_tp; /* Rx throughput in MBps. */
};

#define SEC_CAP_CHK_BMC	BIT0

#define SEC_STATUS_STA_PK_GK_CONFLICT_DIS_BMC_SEARCH	BIT0

struct sec_cam_bmp {
	u32 m0;
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 32)
	u32 m1;
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 64)
	u32 m2;
#endif
#if (SEC_CAM_ENT_NUM_SW_LIMIT > 96)
	u32 m3;
#endif
};

struct cam_ctl_t {
	spinlock_t lock;

	u8 sec_cap;
	u32 flags;

	u8 num;
	struct sec_cam_bmp used;

	_mutex sec_cam_access_mutex;
};

struct sec_cam_ent {
	u16 ctrl;
	u8 mac[ETH_ALEN];
	u8 key[16];
};

#define KEY_FMT "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
#define KEY_ARG(x) ((u8 *)(x))[0], ((u8 *)(x))[1], ((u8 *)(x))[2], ((u8 *)(x))[3], ((u8 *)(x))[4], ((u8 *)(x))[5], \
	((u8 *)(x))[6], ((u8 *)(x))[7], ((u8 *)(x))[8], ((u8 *)(x))[9], ((u8 *)(x))[10], ((u8 *)(x))[11], \
	((u8 *)(x))[12], ((u8 *)(x))[13], ((u8 *)(x))[14], ((u8 *)(x))[15]

#define RTW_DEFAULT_MGMT_MACID 1

struct macid_bmp {
	u32 m0;
#if (MACID_NUM_SW_LIMIT > 32)
	u32 m1;
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	u32 m2;
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	u32 m3;
#endif
};

struct macid_ctl_t {
	spinlock_t lock;
	u8 num;
	struct macid_bmp used;
	struct macid_bmp bmc;
	struct macid_bmp if_g[CONFIG_IFACE_NUMBER];
	struct macid_bmp ch_g[2]; /* 2 ch concurrency */

	u8 iface_bmc[CONFIG_IFACE_NUMBER]; /* bmc TX macid for each iface*/

	u8 h2c_msr[MACID_NUM_SW_LIMIT];
	u8 bw[MACID_NUM_SW_LIMIT];
	u8 vht_en[MACID_NUM_SW_LIMIT];
	u32 rate_bmp0[MACID_NUM_SW_LIMIT];
	u32 rate_bmp1[MACID_NUM_SW_LIMIT];

	struct sta_info *sta[MACID_NUM_SW_LIMIT]; /* corresponding stainfo when macid is not shared */

	/* macid sleep registers */
	u16 reg_sleep_m0;
#if (MACID_NUM_SW_LIMIT > 32)
	u16 reg_sleep_m1;
#endif
#if (MACID_NUM_SW_LIMIT > 64)
	u16 reg_sleep_m2;
#endif
#if (MACID_NUM_SW_LIMIT > 96)
	u16 reg_sleep_m3;
#endif
};

/* used for rf_ctl_t.rate_bmp_cck_ofdm */
#define RATE_BMP_CCK		0x000F
#define RATE_BMP_OFDM		0xFFF0
#define RATE_BMP_HAS_CCK(_bmp_cck_ofdm)		(_bmp_cck_ofdm & RATE_BMP_CCK)
#define RATE_BMP_HAS_OFDM(_bmp_cck_ofdm)	(_bmp_cck_ofdm & RATE_BMP_OFDM)
#define RATE_BMP_GET_CCK(_bmp_cck_ofdm)		(_bmp_cck_ofdm & RATE_BMP_CCK)
#define RATE_BMP_GET_OFDM(_bmp_cck_ofdm)	((_bmp_cck_ofdm & RATE_BMP_OFDM) >> 4)

/* used for rf_ctl_t.rate_bmp_ht_by_bw */
#define RATE_BMP_HT_1SS		0x000000FF
#define RATE_BMP_HT_2SS		0x0000FF00
#define RATE_BMP_HT_3SS		0x00FF0000
#define RATE_BMP_HT_4SS		0xFF000000
#define RATE_BMP_HAS_HT_1SS(_bmp_ht)		(_bmp_ht & RATE_BMP_HT_1SS)
#define RATE_BMP_HAS_HT_2SS(_bmp_ht)		(_bmp_ht & RATE_BMP_HT_2SS)
#define RATE_BMP_HAS_HT_3SS(_bmp_ht)		(_bmp_ht & RATE_BMP_HT_3SS)
#define RATE_BMP_HAS_HT_4SS(_bmp_ht)		(_bmp_ht & RATE_BMP_HT_4SS)
#define RATE_BMP_GET_HT_1SS(_bmp_ht)		(_bmp_ht & RATE_BMP_HT_1SS)
#define RATE_BMP_GET_HT_2SS(_bmp_ht)		((_bmp_ht & RATE_BMP_HT_2SS) >> 8)
#define RATE_BMP_GET_HT_3SS(_bmp_ht)		((_bmp_ht & RATE_BMP_HT_3SS) >> 16)
#define RATE_BMP_GET_HT_4SS(_bmp_ht)		((_bmp_ht & RATE_BMP_HT_4SS) >> 24)

/* used for rf_ctl_t.rate_bmp_vht_by_bw */
#define RATE_BMP_VHT_1SS	0x000003FF
#define RATE_BMP_VHT_2SS	0x000FFC00
#define RATE_BMP_VHT_3SS	0x3FF00000
#define RATE_BMP_HAS_VHT_1SS(_bmp_vht)		(_bmp_vht & RATE_BMP_VHT_1SS)
#define RATE_BMP_HAS_VHT_2SS(_bmp_vht)		(_bmp_vht & RATE_BMP_VHT_2SS)
#define RATE_BMP_HAS_VHT_3SS(_bmp_vht)		(_bmp_vht & RATE_BMP_VHT_3SS)
#define RATE_BMP_GET_VHT_1SS(_bmp_vht)		(_bmp_vht & RATE_BMP_VHT_1SS)
#define RATE_BMP_GET_VHT_2SS(_bmp_vht)		((_bmp_vht & RATE_BMP_VHT_2SS) >> 10)
#define RATE_BMP_GET_VHT_3SS(_bmp_vht)		((_bmp_vht & RATE_BMP_VHT_3SS) >> 20)

#define TXPWR_LMT_REF_VHT_FROM_HT	BIT0
#define TXPWR_LMT_REF_HT_FROM_VHT	BIT1

#define TXPWR_LMT_HAS_CCK_1T	BIT0
#define TXPWR_LMT_HAS_CCK_2T	BIT1
#define TXPWR_LMT_HAS_CCK_3T	BIT2
#define TXPWR_LMT_HAS_CCK_4T	BIT3
#define TXPWR_LMT_HAS_OFDM_1T	BIT4
#define TXPWR_LMT_HAS_OFDM_2T	BIT5
#define TXPWR_LMT_HAS_OFDM_3T	BIT6
#define TXPWR_LMT_HAS_OFDM_4T	BIT7

#define OFFCHS_NONE			0
#define OFFCHS_LEAVING_OP	1
#define OFFCHS_LEAVE_OP		2
#define OFFCHS_BACKING_OP	3

struct rf_ctl_t {
	const struct country_chplan *country_ent;
	u8 ChannelPlan;
	u8 max_chan_nums;
	struct rt_channel_info channel_set[MAX_CHANNEL_NUM];
	struct p2p_channels channel_list;

	_mutex offch_mutex;
	u8 offch_state;

	/* used for debug or by tx power limit */
	u16 rate_bmp_cck_ofdm;		/* 20MHz */
	u32 rate_bmp_ht_by_bw[2];	/* 20MHz, 40MHz. 4SS supported */
	u32 rate_bmp_vht_by_bw[4];	/* 20MHz, 40MHz, 80MHz, 160MHz. up to 3SS supported */

	/* used by tx power limit */
	u8 highest_ht_rate_bw_bmp;
	u8 highest_vht_rate_bw_bmp;

#ifdef CONFIG_TXPWR_LIMIT
	_mutex txpwr_lmt_mutex;
	struct list_head reg_exc_list;
	u8 regd_exc_num;
	struct list_head txpwr_lmt_list;
	u8 txpwr_regd_num;
	const char *regd_name;

	u8 txpwr_lmt_2g_cck_ofdm_state;
#endif
};

#define IS_CAC_STOPPED(rfctl) 1
#define IS_CH_WAITING(rfctl) 0
#define IS_UNDER_CAC(rfctl) 0

#ifdef CONFIG_MI_WITH_MBSSID_CAM
#define TOTAL_MBID_CAM_NUM	8
#define INVALID_CAM_ID			0xFF
struct mbid_cam_ctl_t {
	spinlock_t lock;
	u8 bitmap;
	ATOMIC_T mbid_entry_num;
};
struct mbid_cam_cache {
	u8 iface_id;
	/*u8 role;*/ /*WIFI_STATION_STATE or WIFI_AP_STATE*/
	u8 mac_addr[ETH_ALEN];
};
#endif /*CONFIG_MI_WITH_MBSSID_CAM*/

struct dvobj_priv {
	/*-------- below is common data --------*/
	u8	chip_type;
	u8	HardwareType;
	u8	interface_type;/*USB,SDIO,SPI,PCI*/

	ATOMIC_T	bSurpriseRemoved;
	ATOMIC_T	bDriverStopped;

	int	processing_dev_remove;

	struct debug_priv drv_dbg;

	_mutex hw_init_mutex;
	_mutex h2c_fwcmd_mutex;

	_mutex customer_str_mutex;
	struct submit_ctx *customer_str_sctx;
	u8 customer_str[RTW_CUSTOMER_STR_LEN];

	_mutex setch_mutex;
	_mutex setbw_mutex;
	_mutex rf_read_reg_mutex;
	unsigned char	oper_channel; /* saved channel info when call set_channel_bw */
	unsigned char	oper_bwmode;
	unsigned char	oper_ch_offset;/* PRIME_CHNL_OFFSET */
	unsigned long on_oper_ch_time;

	struct adapter *adapters[CONFIG_IFACE_NUMBER];/*IFACE_ID_MAX*/
	u8 iface_nums; /* total number of ifaces used runtime */
	struct mi_state iface_state;

	u8 nr_ap_if; /* total interface s number of ap/go mode. */
	u16 inter_bcn_space; /* unit:ms */
	struct __queue	ap_if_q;
	struct macid_ctl_t macid_ctl;

	struct cam_ctl_t cam_ctl;
	struct sec_cam_ent cam_cache[SEC_CAM_ENT_NUM_SW_LIMIT];

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	struct mbid_cam_ctl_t mbid_cam_ctl;
	struct mbid_cam_cache mbid_cam_cache[TOTAL_MBID_CAM_NUM];
#endif

	struct rf_ctl_t rf_ctl;

	/* For 92D, DMDP have 2 interface. */
	u8	InterfaceNumber;
	u8	NumInterfaces;

	/* In /Out Pipe information */
	int	RtInPipe[2];
	int	RtOutPipe[4];
	u8	Queue2Pipe[HW_QUEUE_ENTRY];/* for out pipe mapping */

	u8	irq_alloc;
	ATOMIC_T continual_io_error;

	ATOMIC_T disable_func;

	u8 xmit_block;
	spinlock_t xmit_block_lock;

	struct pwrctrl_priv pwrctl_priv;

	struct rtw_traffic_statistics	traffic_stat;

	void * rtnl_lock_holder;

	#if defined(RTW_SINGLE_WIPHY)
	struct wiphy *wiphy;
	#endif

	struct timer_list dynamic_chk_timer; /* dynamic/periodic check timer */
	
#ifdef CONFIG_RTW_NAPI_DYNAMIC
	u8 en_napi_dynamic;
#endif /* CONFIG_RTW_NAPI_DYNAMIC */

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
	u8 default_port_id;
#endif
	/*-------- below is for SDIO INTERFACE --------*/

#ifdef INTF_DATA
	INTF_DATA intf_data;
#endif
#ifdef INTF_OPS
	INTF_OPS intf_ops;
#endif

	/*-------- below is for USB INTERFACE --------*/

	u8	usb_speed; /* 1.1, 2.0 or 3.0 */
	u8	nr_endpoint;
	u8	RtNumInPipes;
	u8	RtNumOutPipes;
	int	ep_num[6]; /* endpoint number */

	int	RegUsbSS;

	struct semaphore	usb_suspend_sema;

	_mutex  usb_vendor_req_mutex;
	u8 *usb_alloc_vendor_req_buf;
	u8 *usb_vendor_req_buf;
	struct usb_interface *pusbintf;
	struct usb_device *pusbdev;
};

#define DEV_STA_NUM(_dvobj)			MSTATE_STA_NUM(&((_dvobj)->iface_state))
#define DEV_STA_LD_NUM(_dvobj)		MSTATE_STA_LD_NUM(&((_dvobj)->iface_state))
#define DEV_STA_LG_NUM(_dvobj)		MSTATE_STA_LG_NUM(&((_dvobj)->iface_state))
#define DEV_TDLS_LD_NUM(_dvobj)		MSTATE_TDLS_LD_NUM(&((_dvobj)->iface_state))
#define DEV_AP_NUM(_dvobj)			MSTATE_AP_NUM(&((_dvobj)->iface_state))
#define DEV_AP_STARTING_NUM(_dvobj)	MSTATE_AP_STARTING_NUM(&((_dvobj)->iface_state))
#define DEV_AP_LD_NUM(_dvobj)		MSTATE_AP_LD_NUM(&((_dvobj)->iface_state))
#define DEV_ADHOC_NUM(_dvobj)		MSTATE_ADHOC_NUM(&((_dvobj)->iface_state))
#define DEV_ADHOC_LD_NUM(_dvobj)	MSTATE_ADHOC_LD_NUM(&((_dvobj)->iface_state))
#define DEV_MESH_NUM(_dvobj)		MSTATE_MESH_NUM(&((_dvobj)->iface_state))
#define DEV_MESH_LD_NUM(_dvobj)		MSTATE_MESH_LD_NUM(&((_dvobj)->iface_state))
#define DEV_SCAN_NUM(_dvobj)		MSTATE_SCAN_NUM(&((_dvobj)->iface_state))
#define DEV_WPS_NUM(_dvobj)			MSTATE_WPS_NUM(&((_dvobj)->iface_state))
#define DEV_ROCH_NUM(_dvobj)		MSTATE_ROCH_NUM(&((_dvobj)->iface_state))
#define DEV_MGMT_TX_NUM(_dvobj)		MSTATE_MGMT_TX_NUM(&((_dvobj)->iface_state))
#define DEV_U_CH(_dvobj)			MSTATE_U_CH(&((_dvobj)->iface_state))
#define DEV_U_BW(_dvobj)			MSTATE_U_BW(&((_dvobj)->iface_state))
#define DEV_U_OFFSET(_dvobj)		MSTATE_U_OFFSET(&((_dvobj)->iface_state))

#define dvobj_to_pwrctl(dvobj) (&(dvobj->pwrctl_priv))
#define pwrctl_to_dvobj(pwrctl) container_of(pwrctl, struct dvobj_priv, pwrctl_priv)
#define dvobj_to_macidctl(dvobj) (&(dvobj->macid_ctl))
#define dvobj_to_sec_camctl(dvobj) (&(dvobj->cam_ctl))
#define dvobj_to_regsty(dvobj) (&(dvobj->adapters[IFACE_ID0]->registrypriv))
#if defined(RTW_SINGLE_WIPHY)
#define dvobj_to_wiphy(dvobj) ((dvobj)->wiphy)
#endif
#define dvobj_to_rfctl(dvobj) (&(dvobj->rf_ctl))
#define rfctl_to_dvobj(rfctl) container_of((rfctl), struct dvobj_priv, rf_ctl)

static inline void dev_set_surprise_removed(struct dvobj_priv *dvobj)
{
	ATOMIC_SET(&dvobj->bSurpriseRemoved, true);
}
static inline void dev_clr_surprise_removed(struct dvobj_priv *dvobj)
{
	ATOMIC_SET(&dvobj->bSurpriseRemoved, false);
}
static inline void dev_set_drv_stopped(struct dvobj_priv *dvobj)
{
	ATOMIC_SET(&dvobj->bDriverStopped, true);
}
static inline void dev_clr_drv_stopped(struct dvobj_priv *dvobj)
{
	ATOMIC_SET(&dvobj->bDriverStopped, false);
}
#define dev_is_surprise_removed(dvobj)	(ATOMIC_READ(&dvobj->bSurpriseRemoved))
#define dev_is_drv_stopped(dvobj)	(ATOMIC_READ(&dvobj->bDriverStopped))

static inline struct device *dvobj_to_dev(struct dvobj_priv *dvobj)
{
	/* todo: get interface type from dvobj and the return the dev accordingly */

	return &dvobj->pusbintf->dev;
}

struct adapter *dvobj_get_port0_adapter(struct dvobj_priv *dvobj);
struct adapter *dvobj_get_unregisterd_adapter(struct dvobj_priv *dvobj);
struct adapter *dvobj_get_adapter_by_addr(struct dvobj_priv *dvobj, u8 *addr);
#define dvobj_get_primary_adapter(dvobj)	((dvobj)->adapters[IFACE_ID0])

enum _hw_port {
	HW_PORT0,
	HW_PORT1,
	HW_PORT2,
	HW_PORT3,
	HW_PORT4,
	MAX_HW_PORT,
};

enum _ADAPTER_TYPE {
	PRIMARY_ADAPTER,
	VIRTUAL_ADAPTER,
	MAX_ADAPTER = 0xFF,
};

enum DRIVER_STATE {
	DRIVER_NORMAL = 0,
	DRIVER_DISAPPEAR = 1,
	DRIVER_REPLACE_DONGLE = 2,
};

#ifdef CONFIG_RTW_NAPI
enum _NAPI_STATE {
	NAPI_DISABLE = 0,
	NAPI_ENABLE = 1,
};
#endif

#ifdef CONFIG_MAC_LOOPBACK_DRIVER
struct loopbackdata {
	struct semaphore	sema;
	void * lbkthread;
	u8 bstop;
	u32 cnt;
	u16 size;
	u16 txsize;
	u8 txbuf[0x8000];
	u16 rxsize;
	u8 rxbuf[0x8000];
	u8 msg[100];

} struct loopbackdata, *struct loopbackdata *;
#endif

struct tsf_info {
	u8 sync_port;/*tsf sync from portx*/
	u8 offset; /*tsf timer offset*/
};

#define ADAPTER_TX_BW_2G(adapter) BW_MODE_2G((adapter)->driver_tx_bw_mode)
#define ADAPTER_TX_BW_5G(adapter) BW_MODE_5G((adapter)->driver_tx_bw_mode)

struct adapter {
	struct timer_list	pwr_state_check_timer;
	int	pwr_state_check_interval;
	u8 pwr_state_check_cnts;
	int	DriverState;/* for disable driver using module, use dongle to replace module. */
	int	pid[3];/* process id from UI, 0:wps, 1:hostapd, 2:dhcpcd */
	int	bDongle;/* build-in module or external dongle */

	struct list_head	list;

	struct dvobj_priv *dvobj;
	struct	mlme_priv mlmepriv;
	struct	mlme_ext_priv mlmeextpriv;
	struct	cmd_priv	cmdpriv;
	struct	evt_priv	evtpriv;
	struct	io_priv	iopriv;
	struct	xmit_priv	xmitpriv;
	struct	recv_priv	recvpriv;
	struct	sta_priv	stapriv;
	struct	security_priv	securitypriv;
	spinlock_t   security_key_mutex; /* add for CONFIG_IEEE80211W, none 11w also can use */
	struct	registry_priv	registrypriv;
	struct	led_priv	ledpriv;
#ifdef CONFIG_RTW_NAPI
	struct	napi_struct napi;
	u8	napi_state;
#endif
	struct	hostapd_priv	*phostapdpriv;
	struct cfg80211_wifidirect_info	cfg80211_wdinfo;
	u32	setband;
	ATOMIC_T bandskip;

	struct wifidirect_info	wdinfo;

	struct wifi_display_info wfd_info;

	enum error_code LastError;

	void * HalData;
	u32 hal_data_sz;
	struct hal_ops	hal_func;

	u32	IsrContent;
	u32	ImrContent;

	u8	EepromAddressSize;
	u8	bDriverIsGoingToUnload;
	u8	init_adpt_in_progress;
	u8	bHaltInProgress;
	void * cmdThread;
#ifdef CONFIG_EVENT_THREAD_MODE
	void * evtThread;
#endif
	u8 registered;

	void (*intf_start)(struct adapter *adapter);
	void (*intf_stop)(struct adapter *adapter);

	struct  net_device * pnetdev;
	char old_ifname[IFNAMSIZ];

	/* used by rtw_rereg_nd_name related function */
	struct rereg_nd_name_data {
		struct  net_device * old_pnetdev;
		char old_ifname[IFNAMSIZ];
		u8 old_ips_mode;
		u8 old_bRegUseLed;
	} rereg_nd_name_priv;

	u8 ndev_unregistering;
	int bup;
	struct net_device_stats stats;
	struct iw_statistics iwstats;
	struct proc_dir_entry *dir_dev;/* for proc directory */
	struct proc_dir_entry *dir_odm;

	struct wireless_dev *rtw_wdev;
	struct rtw_wdev_priv wdev_data;
#if !defined(RTW_SINGLE_WIPHY)
	struct wiphy *wiphy;
#endif
	u8 mac_addr[ETH_ALEN];
	int net_closed;

	u8 netif_up;

	u8 bLinkInfoDump;
	/*	Added by Albert 2012/10/26 */
	/*	The driver will show up the desired channel number when this flag is 1. */
	u8 bNotifyChannelChange;
	u8 bsta_tp_dump;
	/*	Added by Albert 2012/12/06 */
	/*	The driver will show the current P2P status when the upper application reads it. */
	u8 bShowGetP2PState;
#ifdef CONFIG_AUTOSUSPEND
	u8	bDisableAutosuspend;
#endif

	u8 isprimary; /* is primary adapter or not */
	/* notes:
	**	if isprimary is true, the adapter_type value is 0, iface_id is IFACE_ID0 for PRIMARY_ADAPTER
	**	if isprimary is false, the adapter_type value is 1, iface_id is IFACE_ID1 for VIRTUAL_ADAPTER
	**	refer to iface_id if iface_nums>2 and isprimary is false and the adapter_type value is 0xff.*/
	u8 adapter_type;/*be used in  Multi-interface to recognize whether is PRIMARY_ADAPTER  or not(PRIMARY_ADAPTER/VIRTUAL_ADAPTER) .*/
	u8 hw_port; /*interface port type, it depends on HW port */
	struct tsf_info tsf;


	/*extend to support multi interface*/
	/*IFACE_ID0 is equals to PRIMARY_ADAPTER
	IFACE_ID1 is equals to VIRTUAL_ADAPTER*/
	u8 iface_id;

	spinlock_t					br_ext_lock;
	/* unsigned int			macclone_completed; */
	struct nat25_network_db_entry	*nethash[NAT25_HASH_SIZE];
	int				pppoe_connection_in_progress;
	unsigned char			pppoe_addr[MACADDRLEN];
	unsigned char			scdb_mac[MACADDRLEN];
	unsigned char			scdb_ip[4];
	struct nat25_network_db_entry	*scdb_entry;
	unsigned char			br_mac[MACADDRLEN];
	unsigned char			br_ip[4];

	struct br_ext_info		ethBrExtInfo;

#ifdef CONFIG_MAC_LOOPBACK_DRIVER
	struct loopbackdata * ploopback;
#endif
	u8 bmc_tx_rate;
	/* for debug purpose */
	u8 fix_rate;
	u8 fix_bw;
	u8 data_fb; /* data rate fallback, valid only when fix_rate is not 0xff */
	u8 power_offset;
	u8 driver_tx_bw_mode;
	u8 rsvd_page_offset;
	u8 rsvd_page_num;

	u8 driver_vcs_en; /* Enable=1, Disable=0 driver control vrtl_carrier_sense for tx */
	u8 driver_vcs_type;/* force 0:disable VCS, 1:RTS-CTS, 2:CTS-to-self when vcs_en=1. */
	u8 driver_ampdu_spacing;/* driver control AMPDU Density for peer sta's rx */
	u8 driver_rx_ampdu_factor;/* 0xff: disable drv ctrl, 0:8k, 1:16k, 2:32k, 3:64k; */
	u8 driver_rx_ampdu_spacing;  /* driver control Rx AMPDU Density */
	u8 fix_rx_ampdu_accept;
	u8 fix_rx_ampdu_size; /* 0~127, TODO:consider each sta and each TID */
#ifdef CONFIG_TX_AMSDU
	u8 tx_amsdu;
	u16 tx_amsdu_rate;
#endif
	u8 driver_tx_max_agg_num; /*fix tx desc max agg num , 0xff: disable drv ctrl*/

#ifdef CONFIG_DBG_COUNTER
	struct rx_logs rx_logs;
	struct tx_logs tx_logs;
	struct int_logs int_logs;
#endif
};

#define adapter_to_dvobj(adapter) ((adapter)->dvobj)
#define adapter_to_regsty(adapter) dvobj_to_regsty(adapter_to_dvobj((adapter)))
#define adapter_to_pwrctl(adapter) dvobj_to_pwrctl(adapter_to_dvobj((adapter)))
#define adapter_wdev_data(adapter) (&((adapter)->wdev_data))
#if defined(RTW_SINGLE_WIPHY)
#define adapter_to_wiphy(adapter) dvobj_to_wiphy(adapter_to_dvobj(adapter))
#else
#define adapter_to_wiphy(adapter) ((adapter)->wiphy)
#endif

#define adapter_to_rfctl(adapter) dvobj_to_rfctl(adapter_to_dvobj((adapter)))
#define adapter_to_macidctl(adapter) dvobj_to_macidctl(adapter_to_dvobj((adapter)))

#define adapter_mac_addr(adapter) (adapter->mac_addr)
#define adapter_to_chset(adapter) (adapter_to_rfctl((adapter))->channel_set)

#define mlme_to_adapter(mlme) container_of((mlme), struct adapter, mlmepriv)
#define tdls_info_to_adapter(tdls) container_of((tdls), struct adapter, tdlsinfo)

#define rtw_get_chip_type(_adapter) (_adapter->dvobj->chip_type)
#define rtw_get_hw_type(_adapter) (_adapter->dvobj->HardwareType)
#define rtw_get_intf_type(_adapter) (_adapter->dvobj->interface_type)

#define rtw_get_mi_nums(adapter) (_adapter->dvobj->iface_nums)

static inline void rtw_set_surprise_removed(struct adapter *adapt)
{
	dev_set_surprise_removed(adapter_to_dvobj(adapt));
}
static inline void rtw_clr_surprise_removed(struct adapter *adapt)
{
	dev_clr_surprise_removed(adapter_to_dvobj(adapt));
}
static inline void rtw_set_drv_stopped(struct adapter *adapt)
{
	dev_set_drv_stopped(adapter_to_dvobj(adapt));
}
static inline void rtw_clr_drv_stopped(struct adapter *adapt)
{
	dev_clr_drv_stopped(adapter_to_dvobj(adapt));
}
#define rtw_is_surprise_removed(adapt)	(dev_is_surprise_removed(adapter_to_dvobj(adapt)))
#define rtw_is_drv_stopped(adapt)		(dev_is_drv_stopped(adapter_to_dvobj(adapt)))

/*
 * Function disabled.
 *   */
#define DF_TX_BIT		BIT0			/*write_port_cancel*/
#define DF_RX_BIT		BIT1			/*read_port_cancel*/
#define DF_IO_BIT		BIT2

/* #define RTW_DISABLE_FUNC(adapt, func) (ATOMIC_ADD(&adapter_to_dvobj(adapt)->disable_func, (func))) */
/* #define RTW_ENABLE_FUNC(adapt, func) (ATOMIC_SUB(&adapter_to_dvobj(adapt)->disable_func, (func))) */
static inline void RTW_DISABLE_FUNC(struct adapter *adapt, int func_bit)
{
	int	df = ATOMIC_READ(&adapter_to_dvobj(adapt)->disable_func);
	df |= func_bit;
	ATOMIC_SET(&adapter_to_dvobj(adapt)->disable_func, df);
}

static inline void RTW_ENABLE_FUNC(struct adapter *adapt, int func_bit)
{
	int	df = ATOMIC_READ(&adapter_to_dvobj(adapt)->disable_func);
	df &= ~(func_bit);
	ATOMIC_SET(&adapter_to_dvobj(adapt)->disable_func, df);
}

#define RTW_CANNOT_RUN(adapt) \
	(rtw_is_surprise_removed(adapt) || \
	 rtw_is_drv_stopped(adapt))

#define RTW_IS_FUNC_DISABLED(adapt, func_bit) (ATOMIC_READ(&adapter_to_dvobj(adapt)->disable_func) & (func_bit))

#define RTW_CANNOT_IO(adapt) \
	(rtw_is_surprise_removed(adapt) || \
	 RTW_IS_FUNC_DISABLED((adapt), DF_IO_BIT))

#define RTW_CANNOT_RX(adapt) \
	(RTW_CANNOT_RUN(adapt) || \
	 RTW_IS_FUNC_DISABLED((adapt), DF_RX_BIT))

#define RTW_CANNOT_TX(adapt) \
	(RTW_CANNOT_RUN(adapt) || \
	 RTW_IS_FUNC_DISABLED((adapt), DF_TX_BIT))

int rtw_suspend_free_assoc_resource(struct adapter *adapt);
int rtw_change_ifname(struct adapter *adapt, const char *ifname);
void nat25_db_expire(struct adapter *priv);
void *scdb_findEntry(struct adapter *priv, unsigned char *macAddr, unsigned char *ipAddr);
void dhcp_flag_bcast(struct adapter *priv, struct sk_buff *skb);
void rtw_indicate_wx_assoc_event(struct adapter *adapt);
void rtw_indicate_wx_disassoc_event(struct adapter *adapt);
void indicate_wx_scan_complete_event(struct adapter *adapt);
int nat25_handle_frame(struct adapter *priv, struct sk_buff *skb);
int nat25_db_handle(struct adapter *priv, struct sk_buff *skb, int method);

/* HCI Related header file */
#include <usb_ops.h>
#include <usb_hal.h>
#include <usb_vendor_req.h>

extern char *rtw_initmac;
extern int rtw_ht_enable;
extern int rtw_bw_mode;
extern int rtw_ampdu_enable;/* for enable tx_ampdu */
extern int rtw_mc2u_disable;
void netdev_br_init(struct net_device *netdev);
extern int ui_pid[3];

#endif /* __DRV_TYPES_H__ */
