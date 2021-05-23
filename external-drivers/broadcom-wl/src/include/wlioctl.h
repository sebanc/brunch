/*
 * Custom OID/ioctl definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wlioctl.h 464835 2014-03-26 01:35:40Z $
 */

#ifndef _wlioctl_h_
#define	_wlioctl_h_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmip.h>
#include <proto/bcmevent.h>
#include <proto/802.11.h>
#include <bcmwifi_channels.h>
#include <bcmwifi_rates.h>

#define BWL_DEFAULT_PACKING
#include <packed_section_start.h>

#define	WL_BSS_INFO_VERSION	109		

typedef struct wl_bss_info {
	uint32		version;		
	uint32		length;			
	struct ether_addr BSSID;
	uint16		beacon_period;		
	uint16		capability;		
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			
		uint8	rates[16];		
	} rateset;				
	chanspec_t	chanspec;		
	uint16		atim_window;		
	uint8		dtim_period;		
	int16		RSSI;			
	int8		phy_noise;		

	uint8		n_cap;			
	uint32		nbss_cap;		
	uint8		ctl_ch;			
	uint8		padding1[3];		
	uint16		vht_rxmcsmap;	
	uint16		vht_txmcsmap;	
	uint8		flags;			
	uint8		vht_cap;		
	uint8		reserved[2];		
	uint8		basic_mcs[MCSSET_LEN];	

	uint16		ie_offset;		
	uint32		ie_length;		
	int16		SNR;			

} wl_bss_info_t;

#define WL_BSS_FLAGS_FROM_BEACON	0x01	
#define WL_BSS_FLAGS_FROM_CACHE		0x02	
#define WL_BSS_FLAGS_RSSI_ONCHANNEL 0x04 

#define VHT_BI_SGI_80MHZ			0x00000100
#define VHT_BI_80MHZ			    0x00000200
#define VHT_BI_160MHZ			    0x00000400
#define VHT_BI_8080MHZ			    0x00000800

typedef struct wlc_ssid {
	uint32		SSID_len;
	uchar		SSID[32];
} wlc_ssid_t;

typedef struct wl_scan_results {
	uint32 buflen;
	uint32 version;
	uint32 count;
	wl_bss_info_t bss_info[1];
} wl_scan_results_t;

#define WL_MAXRATES_IN_SET		16	
typedef struct wl_rateset {
	uint32	count;			
	uint8	rates[WL_MAXRATES_IN_SET];	
} wl_rateset_t;

typedef struct wl_rateset_args {
	uint32	count;			
	uint8	rates[WL_MAXRATES_IN_SET];	
	uint8   mcs[MCSSET_LEN];        
	uint16 vht_mcs[VHT_CAP_MCS_MAP_NSS_MAX]; 
} wl_rateset_args_t;

#define TXBF_RATE_MCS_ALL		4
#define TXBF_RATE_VHT_ALL		4
#define TXBF_RATE_OFDM_ALL		8

typedef struct wl_txbf_rateset {
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];	
	uint8	txbf_rate_mcs_bcm[TXBF_RATE_MCS_ALL];	
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];	
	uint16	txbf_rate_vht_bcm[TXBF_RATE_VHT_ALL];	
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];	
	uint8	txbf_rate_ofdm_bcm[TXBF_RATE_OFDM_ALL]; 
	uint8	txbf_rate_ofdm_cnt;
	uint8	txbf_rate_ofdm_cnt_bcm;
} wl_txbf_rateset_t;

#define OFDM_RATE_MASK			0x0000007f
typedef uint8 ofdm_rates_t;

typedef struct wl_uint32_list {

	uint32 count;

	uint32 element[1];
} wl_uint32_list_t;

typedef struct wl_assoc_params {
	struct ether_addr bssid;	
	int32 chanspec_num;		
	chanspec_t chanspec_list[1];	
} wl_assoc_params_t;
#define WL_ASSOC_PARAMS_FIXED_SIZE 	OFFSETOF(wl_assoc_params_t, chanspec_list)

typedef wl_assoc_params_t wl_reassoc_params_t;
#define WL_REASSOC_PARAMS_FIXED_SIZE	WL_ASSOC_PARAMS_FIXED_SIZE

typedef wl_assoc_params_t wl_join_assoc_params_t;
#define WL_JOIN_ASSOC_PARAMS_FIXED_SIZE	WL_ASSOC_PARAMS_FIXED_SIZE

typedef struct wl_join_params {
	wlc_ssid_t ssid;
	wl_assoc_params_t params;	
} wl_join_params_t;

#define WLC_CNTRY_BUF_SZ	4		

#define	CRYPTO_ALGO_OFF			0
#define	CRYPTO_ALGO_WEP1		1
#define	CRYPTO_ALGO_TKIP		2
#define	CRYPTO_ALGO_WEP128		3
#define CRYPTO_ALGO_AES_CCM		4
#define CRYPTO_ALGO_AES_OCB_MSDU	5
#define CRYPTO_ALGO_AES_OCB_MPDU	6
#define CRYPTO_ALGO_NALG		7
#define CRYPTO_ALGO_PMK			12	
#define CRYPTO_ALGO_BIP			13  

#define WSEC_GEN_MIC_ERROR	0x0001
#define WSEC_GEN_REPLAY		0x0002
#define WSEC_GEN_ICV_ERROR	0x0004
#define WSEC_GEN_MFP_ACT_ERROR	0x0008
#define WSEC_GEN_MFP_DISASSOC_ERROR	0x0010
#define WSEC_GEN_MFP_DEAUTH_ERROR	0x0020

#define WL_SOFT_KEY	(1 << 0)	
#define WL_PRIMARY_KEY	(1 << 1)	
#define WL_KF_RES_4	(1 << 4)	
#define WL_KF_RES_5	(1 << 5)	
#define WL_IBSS_PEER_GROUP_KEY	(1 << 6)	

typedef struct wl_wsec_key {
	uint32		index;		
	uint32		len;		
	uint8		data[DOT11_MAX_KEY_SIZE];	
	uint32		pad_1[18];
	uint32		algo;		
	uint32		flags;		
	uint32		pad_2[2];
	int		pad_3;
	int		iv_initialized;	
	int		pad_4;

	struct {
		uint32	hi;		
		uint16	lo;		
	} rxiv;
	uint32		pad_5[2];
	struct ether_addr ea;		
} wl_wsec_key_t;

#define WSEC_MIN_PSK_LEN	8
#define WSEC_MAX_PSK_LEN	64

#define WSEC_PASSPHRASE		(1<<0)

typedef struct {
	ushort	key_len;		
	ushort	flags;			
	uint8	key[WSEC_MAX_PSK_LEN];	
} wsec_pmk_t;

#define WEP_ENABLED		0x0001
#define TKIP_ENABLED		0x0002
#define AES_ENABLED		0x0004
#define WSEC_SWFLAG		0x0008
#define SES_OW_ENABLED		0x0040	

#define WSEC_WEP_ENABLED(wsec)	((wsec) & WEP_ENABLED)
#define WSEC_TKIP_ENABLED(wsec)	((wsec) & TKIP_ENABLED)
#define WSEC_AES_ENABLED(wsec)	((wsec) & AES_ENABLED)

#define WSEC_ENABLED(wsec)	((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED))
#define WSEC_SES_OW_ENABLED(wsec)	((wsec) & SES_OW_ENABLED)

#define MFP_CAPABLE		0x0200
#define MFP_REQUIRED	0x0400
#define MFP_SHA256		0x0800 

#define WPA_AUTH_DISABLED	0x0000	
#define WPA_AUTH_NONE		0x0001	
#define WPA_AUTH_UNSPECIFIED	0x0002	
#define WPA_AUTH_PSK		0x0004	

#define WPA2_AUTH_UNSPECIFIED	0x0040	
#define WPA2_AUTH_PSK		0x0080	
#define BRCM_AUTH_PSK           0x0100  
#define BRCM_AUTH_DPT		0x0200	
#define WPA2_AUTH_MFP           0x1000  
#define WPA2_AUTH_TPK		0x2000 	
#define WPA2_AUTH_FT		0x4000 	
#define WPA_AUTH_PFN_ANY	0xffffffff	

#define	MAXPMKID		16

typedef struct _pmkid {
	struct ether_addr	BSSID;
	uint8			PMKID[WPA2_PMKID_LEN];
} pmkid_t;

typedef struct _pmkid_list {
	uint32	npmkid;
	pmkid_t	pmkid[1];
} pmkid_list_t;

typedef struct _pmkid_cand {
	struct ether_addr	BSSID;
	uint8			preauth;
} pmkid_cand_t;

typedef struct _pmkid_cand_list {
	uint32	npmkid_cand;
	pmkid_cand_t	pmkid_cand[1];
} pmkid_cand_list_t;

typedef struct {
	uint32	val;
	struct ether_addr ea;
} scb_val_t;

typedef struct {
	uint32 code;
	scb_val_t ioctl_args;
} authops_t;

typedef struct channel_info {
	int hw_channel;
	int target_channel;
	int scan_channel;
} channel_info_t;

struct maclist {
	uint count;			
	struct ether_addr ea[1];	
};

typedef struct wl_ioctl {
	uint cmd;	
	void *buf;	
	uint len;	
	uint8 set;		
	uint used;	
	uint needed;	
} wl_ioctl_t;

#define ioctl_subtype	set		
#define ioctl_pid	used		
#define ioctl_status	needed		

typedef struct wlc_rev_info {
	uint		vendorid;	
	uint		deviceid;	
	uint		radiorev;	
	uint		chiprev;	
	uint		corerev;	
	uint		boardid;	
	uint		boardvendor;	
	uint		boardrev;	
	uint		driverrev;	
	uint		ucoderev;	
	uint		bus;		
	uint		chipnum;	
	uint		phytype;	
	uint		phyrev;		
	uint		anarev;		
	uint		chippkg;	
	uint		nvramrev;	
} wlc_rev_info_t;

#define WL_REV_INFO_LEGACY_LENGTH	48

#define	WLC_IOCTL_MAXLEN		8192	
#define	WLC_IOCTL_SMLEN			256	
#define WLC_IOCTL_MEDLEN		1536    
#if defined(LCNCONF) || defined(LCN40CONF)
#define WLC_SAMPLECOLLECT_MAXLEN	8192	
#else
#define WLC_SAMPLECOLLECT_MAXLEN	10240	
#endif
#define WLC_SAMPLECOLLECT_MAXLEN_LCN40  8192

#define WLC_GET_MAGIC				0
#define WLC_GET_VERSION				1
#define WLC_UP					2
#define WLC_DOWN				3
#define WLC_GET_LOOP				4
#define WLC_SET_LOOP				5
#define WLC_DUMP				6
#define WLC_GET_MSGLEVEL			7
#define WLC_SET_MSGLEVEL			8
#define WLC_GET_PROMISC				9
#define WLC_SET_PROMISC				10

#define WLC_GET_RATE				12
#define WLC_GET_MAX_RATE			13
#define WLC_GET_INSTANCE			14

#define WLC_GET_INFRA				19
#define WLC_SET_INFRA				20
#define WLC_GET_AUTH				21
#define WLC_SET_AUTH				22
#define WLC_GET_BSSID				23
#define WLC_SET_BSSID				24
#define WLC_GET_SSID				25
#define WLC_SET_SSID				26
#define WLC_RESTART				27
#define WLC_TERMINATED             		28

#define WLC_GET_CHANNEL				29
#define WLC_SET_CHANNEL				30
#define WLC_GET_SRL				31
#define WLC_SET_SRL				32
#define WLC_GET_LRL				33
#define WLC_SET_LRL				34
#define WLC_GET_PLCPHDR				35
#define WLC_SET_PLCPHDR				36
#define WLC_GET_RADIO				37
#define WLC_SET_RADIO				38
#define WLC_GET_PHYTYPE				39
#define WLC_DUMP_RATE				40
#define WLC_SET_RATE_PARAMS			41
#define WLC_GET_FIXRATE				42
#define WLC_SET_FIXRATE				43

#define WLC_GET_KEY				44
#define WLC_SET_KEY				45
#define WLC_GET_REGULATORY			46
#define WLC_SET_REGULATORY			47
#define WLC_GET_PASSIVE_SCAN			48
#define WLC_SET_PASSIVE_SCAN			49
#define WLC_SCAN				50
#define WLC_SCAN_RESULTS			51
#define WLC_DISASSOC				52
#define WLC_REASSOC				53
#define WLC_GET_ROAM_TRIGGER			54
#define WLC_SET_ROAM_TRIGGER			55
#define WLC_GET_ROAM_DELTA			56
#define WLC_SET_ROAM_DELTA			57
#define WLC_GET_ROAM_SCAN_PERIOD		58
#define WLC_SET_ROAM_SCAN_PERIOD		59
#define WLC_EVM					60	
#define WLC_GET_TXANT				61
#define WLC_SET_TXANT				62
#define WLC_GET_ANTDIV				63
#define WLC_SET_ANTDIV				64

#define WLC_GET_CLOSED				67
#define WLC_SET_CLOSED				68
#define WLC_GET_MACLIST				69
#define WLC_SET_MACLIST				70
#define WLC_GET_RATESET				71
#define WLC_SET_RATESET				72

#define WLC_LONGTRAIN				74
#define WLC_GET_BCNPRD				75
#define WLC_SET_BCNPRD				76
#define WLC_GET_DTIMPRD				77
#define WLC_SET_DTIMPRD				78
#define WLC_GET_SROM				79
#define WLC_SET_SROM				80
#define WLC_GET_WEP_RESTRICT			81
#define WLC_SET_WEP_RESTRICT			82
#define WLC_GET_COUNTRY				83
#define WLC_SET_COUNTRY				84
#define WLC_GET_PM				85
#define WLC_SET_PM				86
#define WLC_GET_WAKE				87
#define WLC_SET_WAKE				88

#define WLC_GET_FORCELINK			90	
#define WLC_SET_FORCELINK			91	
#define WLC_FREQ_ACCURACY			92	
#define WLC_CARRIER_SUPPRESS			93	
#define WLC_GET_PHYREG				94
#define WLC_SET_PHYREG				95
#define WLC_GET_RADIOREG			96
#define WLC_SET_RADIOREG			97
#define WLC_GET_REVINFO				98
#define WLC_GET_UCANTDIV			99
#define WLC_SET_UCANTDIV			100
#define WLC_R_REG				101
#define WLC_W_REG				102

#define WLC_GET_MACMODE				105
#define WLC_SET_MACMODE				106
#define WLC_GET_MONITOR				107
#define WLC_SET_MONITOR				108
#define WLC_GET_GMODE				109
#define WLC_SET_GMODE				110
#define WLC_GET_LEGACY_ERP			111
#define WLC_SET_LEGACY_ERP			112
#define WLC_GET_RX_ANT				113
#define WLC_GET_CURR_RATESET			114	
#define WLC_GET_SCANSUPPRESS			115
#define WLC_SET_SCANSUPPRESS			116
#define WLC_GET_AP				117
#define WLC_SET_AP				118
#define WLC_GET_EAP_RESTRICT			119
#define WLC_SET_EAP_RESTRICT			120
#define WLC_SCB_AUTHORIZE			121
#define WLC_SCB_DEAUTHORIZE			122
#define WLC_GET_WDSLIST				123
#define WLC_SET_WDSLIST				124
#define WLC_GET_ATIM				125
#define WLC_SET_ATIM				126
#define WLC_GET_RSSI				127
#define WLC_GET_PHYANTDIV			128
#define WLC_SET_PHYANTDIV			129
#define WLC_AP_RX_ONLY				130
#define WLC_GET_TX_PATH_PWR			131
#define WLC_SET_TX_PATH_PWR			132
#define WLC_GET_WSEC				133
#define WLC_SET_WSEC				134
#define WLC_GET_PHY_NOISE			135
#define WLC_GET_BSS_INFO			136
#define WLC_GET_PKTCNTS				137
#define WLC_GET_LAZYWDS				138
#define WLC_SET_LAZYWDS				139
#define WLC_GET_BANDLIST			140

#define WLC_GET_PHYLIST				180
#define WLC_SCB_DEAUTHENTICATE_FOR_REASON	201
#define WLC_GET_VALID_CHANNELS			217
#define WLC_GET_FAKEFRAG			218
#define WLC_SET_FAKEFRAG			219
#define WLC_GET_PWROUT_PERCENTAGE		220
#define WLC_SET_PWROUT_PERCENTAGE		221
#define WLC_SET_BAD_FRAME_PREEMPT		222
#define WLC_GET_BAD_FRAME_PREEMPT		223
#define WLC_SET_LEAP_LIST			224
#define WLC_GET_LEAP_LIST			225
#define WLC_GET_CWMIN				226
#define WLC_SET_CWMIN				227
#define WLC_GET_CWMAX				228
#define WLC_SET_CWMAX				229
#define WLC_GET_WET				230
#define WLC_SET_WET				231
#define WLC_GET_PUB				232

#define WLC_GET_KEY_PRIMARY			235
#define WLC_SET_KEY_PRIMARY			236

#define WLC_GET_VAR				262	
#define WLC_SET_VAR				263	

#define WL_RADIO_SW_DISABLE		(1<<0)
#define WL_RADIO_HW_DISABLE		(1<<1)
#define WL_RADIO_MPC_DISABLE		(1<<2)
#define WL_RADIO_COUNTRY_DISABLE	(1<<3)	

#define	WL_SPURAVOID_OFF	0
#define	WL_SPURAVOID_ON1	1
#define	WL_SPURAVOID_ON2	2

#define WL_4335_SPURAVOID_ON1	1
#define WL_4335_SPURAVOID_ON2	2
#define WL_4335_SPURAVOID_ON3	3
#define WL_4335_SPURAVOID_ON4	4
#define WL_4335_SPURAVOID_ON5	5
#define WL_4335_SPURAVOID_ON6	6
#define WL_4335_SPURAVOID_ON7	7
#define WL_4335_SPURAVOID_ON8	8
#define WL_4335_SPURAVOID_ON9	9

#define WL_TXPWR_OVERRIDE	(1U<<31)
#define WL_TXPWR_NEG   (1U<<30)

#define	WLC_PHY_TYPE_A		0
#define	WLC_PHY_TYPE_B		1
#define	WLC_PHY_TYPE_G		2
#define	WLC_PHY_TYPE_N		4
#define	WLC_PHY_TYPE_LP		5
#define	WLC_PHY_TYPE_SSN	6
#define	WLC_PHY_TYPE_HT		7
#define	WLC_PHY_TYPE_LCN	8
#define	WLC_PHY_TYPE_LCN40	10
#define WLC_PHY_TYPE_AC		11
#define	WLC_PHY_TYPE_NULL	0xf

#define PM_OFF	0
#define PM_MAX	1
#define PM_FAST 2
#define PM_FORCE_OFF 3 		

#define	NFIFO			6	
#define NREINITREASONCOUNT	8
#define REINITREASONIDX(_x)	(((_x) < NREINITREASONCOUNT) ? (_x) : 0)

#define	WL_CNT_T_VERSION	10	

typedef struct {
	uint16	version;	
	uint16	length;		

	uint32	txframe;	
	uint32	txbyte;		
	uint32	txretrans;	
	uint32	txerror;	
	uint32	txctl;		
	uint32	txprshort;	
	uint32	txserr;		
	uint32	txnobuf;	
	uint32	txnoassoc;	
	uint32	txrunt;		
	uint32	txchit;		
	uint32	txcmiss;	

	uint32	txuflo;		
	uint32	txphyerr;	
	uint32	txphycrs;

	uint32	rxframe;	
	uint32	rxbyte;		
	uint32	rxerror;	
	uint32	rxctl;		
	uint32	rxnobuf;	
	uint32	rxnondata;	
	uint32	rxbadds;	
	uint32	rxbadcm;	
	uint32	rxfragerr;	
	uint32	rxrunt;		
	uint32	rxgiant;	
	uint32	rxnoscb;	
	uint32	rxbadproto;	
	uint32	rxbadsrcmac;	
	uint32	rxbadda;	
	uint32	rxfilter;	

	uint32	rxoflo;		
	uint32	rxuflo[NFIFO];	

	uint32	d11cnt_txrts_off;	
	uint32	d11cnt_rxcrc_off;	
	uint32	d11cnt_txnocts_off;	

	uint32	dmade;		
	uint32	dmada;		
	uint32	dmape;		
	uint32	reset;		
	uint32	tbtt;		
	uint32	txdmawar;
	uint32	pkt_callback_reg_fail;	

	uint32	txallfrm;	
	uint32	txrtsfrm;	
	uint32	txctsfrm;	
	uint32	txackfrm;	
	uint32	txdnlfrm;	
	uint32	txbcnfrm;	
	uint32	txfunfl[8];	
	uint32	txtplunfl;	
	uint32	txphyerror;	
	uint32	rxfrmtoolong;	
	uint32	rxfrmtooshrt;	
	uint32	rxinvmachdr;	
	uint32	rxbadfcs;	
	uint32	rxbadplcp;	
	uint32	rxcrsglitch;	
	uint32	rxstrt;		
	uint32	rxdfrmucastmbss; 
	uint32	rxmfrmucastmbss; 
	uint32	rxcfrmucast;	
	uint32	rxrtsucast;	
	uint32	rxctsucast;	
	uint32	rxackucast;	
	uint32	rxdfrmocast;	
	uint32	rxmfrmocast;	
	uint32	rxcfrmocast;	
	uint32	rxrtsocast;	
	uint32	rxctsocast;	
	uint32	rxdfrmmcast;	
	uint32	rxmfrmmcast;	
	uint32	rxcfrmmcast;	
	uint32	rxbeaconmbss;	
	uint32	rxdfrmucastobss; 
	uint32	rxbeaconobss;	
	uint32	rxrsptmout;	
	uint32	bcntxcancl;	
	uint32	rxf0ovfl;	
	uint32	rxf1ovfl;	
	uint32	rxf2ovfl;	
	uint32	txsfovfl;	
	uint32	pmqovfl;	
	uint32	rxcgprqfrm;	
	uint32	rxcgprsqovfl;	
	uint32	txcgprsfail;	
	uint32	txcgprssuc;	
	uint32	prs_timeout;	
	uint32	rxnack;		
	uint32	frmscons;	
	uint32	txnack;		
	uint32	txglitch_nack;	
	uint32	txburst;	

	uint32	txfrag;		
	uint32	txmulti;	
	uint32	txfail;		
	uint32	txretry;	
	uint32	txretrie;	
	uint32	rxdup;		
	uint32	txrts;		
	uint32	txnocts;	
	uint32	txnoack;	
	uint32	rxfrag;		
	uint32	rxmulti;	
	uint32	rxcrc;		
	uint32	txfrmsnt;	
	uint32	rxundec;	

	uint32	tkipmicfaill;	
	uint32	tkipcntrmsr;	
	uint32	tkipreplay;	
	uint32	ccmpfmterr;	
	uint32	ccmpreplay;	
	uint32	ccmpundec;	
	uint32	fourwayfail;	
	uint32	wepundec;	
	uint32	wepicverr;	
	uint32	decsuccess;	
	uint32	tkipicverr;	
	uint32	wepexcluded;	

	uint32	txchanrej;	
	uint32	psmwds;		
	uint32	phywatchdog;	

	uint32	prq_entries_handled;	
	uint32	prq_undirected_entries;	
	uint32	prq_bad_entries;	
	uint32	atim_suppress_count;	
	uint32	bcn_template_not_ready;	
	uint32	bcn_template_not_ready_done; 
	uint32	late_tbtt_dpc;	

	uint32  rx1mbps;	
	uint32  rx2mbps;	
	uint32  rx5mbps5;	
	uint32  rx6mbps;	
	uint32  rx9mbps;	
	uint32  rx11mbps;	
	uint32  rx12mbps;	
	uint32  rx18mbps;	
	uint32  rx24mbps;	
	uint32  rx36mbps;	
	uint32  rx48mbps;	
	uint32  rx54mbps;	
	uint32  rx108mbps; 	
	uint32  rx162mbps;	
	uint32  rx216mbps;	
	uint32  rx270mbps;	
	uint32  rx324mbps;	
	uint32  rx378mbps;	
	uint32  rx432mbps;	
	uint32  rx486mbps;	
	uint32  rx540mbps;	

	uint32	pktengrxducast; 
	uint32	pktengrxdmcast; 

	uint32	rfdisable;	
	uint32	bphy_rxcrsglitch;	
	uint32  bphy_badplcp;

	uint32	txexptime;	

	uint32	txmpdu_sgi;	
	uint32	rxmpdu_sgi;	
	uint32	txmpdu_stbc;	
	uint32	rxmpdu_stbc;	

	uint32	rxundec_mcst;	

	uint32	tkipmicfaill_mcst;	
	uint32	tkipcntrmsr_mcst;	
	uint32	tkipreplay_mcst;	
	uint32	ccmpfmterr_mcst;	
	uint32	ccmpreplay_mcst;	
	uint32	ccmpundec_mcst;	
	uint32	fourwayfail_mcst;	
	uint32	wepundec_mcst;	
	uint32	wepicverr_mcst;	
	uint32	decsuccess_mcst;	
	uint32	tkipicverr_mcst;	
	uint32	wepexcluded_mcst;	

	uint32	dma_hang;	
	uint32	reinit;		

	uint32  pstatxucast;	
	uint32  pstatxnoassoc;	
	uint32  pstarxucast;	
	uint32  pstarxbcmc;	
	uint32  pstatxbcmc;	

	uint32  cso_passthrough; 
	uint32 	cso_normal;	
	uint32	chained;	
	uint32	chainedsz1;	
	uint32	unchained;	
	uint32	maxchainsz;	
	uint32	currchainsz;	
	uint32	pciereset;	
	uint32	cfgrestore;	
	uint32	reinitreason[NREINITREASONCOUNT]; 
} wl_cnt_t;

typedef struct {
	bool auto_en;
	uint8 active_ant;
	uint32 rxcount;
	int16 avg_snr_per_ant0;
	int16 avg_snr_per_ant1;
	uint32 swap_ge_rxcount0;
	uint32 swap_ge_rxcount1;
	uint32 swap_ge_snrthresh0;
	uint32 swap_ge_snrthresh1;
	uint32 swap_txfail0;
	uint32 swap_txfail1;
	uint32 swap_timer0;
	uint32 swap_timer1;
	uint32 swap_alivecheck0;
	uint32 swap_alivecheck1;
	uint32 rxcount_per_ant0;
	uint32 rxcount_per_ant1;
	uint32 acc_rxcount;
	uint32 acc_rxcount_per_ant0;
	uint32 acc_rxcount_per_ant1;
} wlc_swdiv_stats_t;

#define WL_WME_CNT_VERSION	1	

typedef struct {
	uint32 packets;
	uint32 bytes;
} wl_traffic_stats_t;

typedef struct {
	uint16	version;	
	uint16	length;		

	wl_traffic_stats_t tx[AC_COUNT];	
	wl_traffic_stats_t tx_failed[AC_COUNT];	
	wl_traffic_stats_t rx[AC_COUNT];	
	wl_traffic_stats_t rx_failed[AC_COUNT];	

	wl_traffic_stats_t forward[AC_COUNT];	

	wl_traffic_stats_t tx_expired[AC_COUNT];	

} wl_wme_cnt_t;

typedef struct wl_mkeep_alive_pkt {
	uint16	version; 
	uint16	length; 
	uint32	period_msec;
	uint16	len_bytes;
	uint8	keep_alive_id; 
	uint8	data[1];
} wl_mkeep_alive_pkt_t;

#define WL_MKEEP_ALIVE_VERSION		1
#define WL_MKEEP_ALIVE_FIXED_LEN	OFFSETOF(wl_mkeep_alive_pkt_t, data)
#define WL_MKEEP_ALIVE_PRECISION	500

typedef struct wl_mtcpkeep_alive_conn_pkt {
	struct ether_addr saddr;		
	struct ether_addr daddr;		
	struct ipv4_addr sipaddr;		
	struct ipv4_addr dipaddr;		
	uint16 sport;				
	uint16 dport;				
	uint32 seq;				
	uint32 ack;				
	uint16 tcpwin;				
} wl_mtcpkeep_alive_conn_pkt_t;

typedef struct wl_mtcpkeep_alive_timers_pkt {
	uint16 interval;		
	uint16 retry_interval;		
	uint16 retry_count;		
} wl_mtcpkeep_alive_timers_pkt_t;

#ifndef ETHER_MAX_DATA
#define ETHER_MAX_DATA	1500
#endif 

typedef struct wake_info {
	uint32          wake_reason;
	uint32          wake_info_len;		
	uchar			packet[1];
} wake_info_t;

typedef struct wake_pkt {
	uint32          wake_pkt_len;		
	uchar			packet[1];
} wake_pkt_t;

#define WL_MTCPKEEP_ALIVE_VERSION		1

#ifdef WLBA

#define WLC_BA_CNT_VERSION  1   

typedef struct wlc_ba_cnt {
	uint16  version;    
	uint16  length;     

	uint32 txpdu;       
	uint32 txsdu;       
	uint32 txfc;        
	uint32 txfci;       
	uint32 txretrans;   
	uint32 txbatimer;   
	uint32 txdrop;      
	uint32 txaddbareq;  
	uint32 txaddbaresp; 
	uint32 txdelba;     
	uint32 txba;        
	uint32 txbar;       
	uint32 txpad[4];    

	uint32 rxpdu;       
	uint32 rxqed;       
	uint32 rxdup;       
	uint32 rxnobuf;     
	uint32 rxaddbareq;  
	uint32 rxaddbaresp; 
	uint32 rxdelba;     
	uint32 rxba;        
	uint32 rxbar;       
	uint32 rxinvba;     
	uint32 rxbaholes;   
	uint32 rxunexp;     
	uint32 rxpad[4];    
} wlc_ba_cnt_t;
#endif 

struct ampdu_tid_control {
	uint8 tid;			
	uint8 enable;			
};

struct ampdu_ea_tid {
	struct ether_addr ea;		
	uint8 tid;			
};

struct ampdu_retry_tid {
	uint8 tid;	
	uint8 retry;	
};

#define TOE_TX_CSUM_OL		0x00000001
#define TOE_RX_CSUM_OL		0x00000002

#define TOE_ERRTEST_TX_CSUM	0x00000001
#define TOE_ERRTEST_RX_CSUM	0x00000002
#define TOE_ERRTEST_RX_CSUM2	0x00000004

struct toe_ol_stats_t {

	uint32 tx_summed;

	uint32 tx_iph_fill;
	uint32 tx_tcp_fill;
	uint32 tx_udp_fill;
	uint32 tx_icmp_fill;

	uint32 rx_iph_good;
	uint32 rx_iph_bad;
	uint32 rx_tcp_good;
	uint32 rx_tcp_bad;
	uint32 rx_udp_good;
	uint32 rx_udp_bad;
	uint32 rx_icmp_good;
	uint32 rx_icmp_bad;

	uint32 tx_tcp_errinj;
	uint32 tx_udp_errinj;
	uint32 tx_icmp_errinj;

	uint32 rx_tcp_errinj;
	uint32 rx_udp_errinj;
	uint32 rx_icmp_errinj;
};

#define ARP_OL_AGENT		0x00000001
#define ARP_OL_SNOOP		0x00000002
#define ARP_OL_HOST_AUTO_REPLY	0x00000004
#define ARP_OL_PEER_AUTO_REPLY	0x00000008

#define ARP_ERRTEST_REPLY_PEER	0x1
#define ARP_ERRTEST_REPLY_HOST	0x2

#define ARP_MULTIHOMING_MAX	8	
#define ND_MULTIHOMING_MAX 10	

struct arp_ol_stats_t {
	uint32  host_ip_entries;	
	uint32  host_ip_overflow;	

	uint32  arp_table_entries;	
	uint32  arp_table_overflow;	

	uint32  host_request;		
	uint32  host_reply;		
	uint32  host_service;		

	uint32  peer_request;		
	uint32  peer_request_drop;	
	uint32  peer_reply;		
	uint32  peer_reply_drop;	
	uint32  peer_service;		
};

struct nd_ol_stats_t {
	uint32  host_ip_entries;    
	uint32  host_ip_overflow;   
	uint32  peer_request;       
	uint32  peer_request_drop;  
	uint32  peer_reply_drop;    
	uint32  peer_service;       
};

typedef struct wl_keep_alive_pkt {
	uint32	period_msec;	
	uint16	len_bytes;	
	uint8	data[1];	
} wl_keep_alive_pkt_t;

#define WL_KEEP_ALIVE_FIXED_LEN		OFFSETOF(wl_keep_alive_pkt_t, data)

typedef enum wl_pkt_filter_type {
	WL_PKT_FILTER_TYPE_PATTERN_MATCH	
} wl_pkt_filter_type_t;

#define WL_PKT_FILTER_TYPE wl_pkt_filter_type_t

typedef struct wl_pkt_filter_pattern {
	uint32	offset;		
	uint32	size_bytes;	
	uint8   mask_and_pattern[1]; 

} wl_pkt_filter_pattern_t;

typedef struct wl_pkt_filter {
	uint32	id;		
	uint32	type;		
	uint32	negate_match;	
	union {			
		wl_pkt_filter_pattern_t pattern;	
	} u;
} wl_pkt_filter_t;

typedef struct wl_tcp_keep_set {
	uint32	val1;
	uint32	val2;
} wl_tcp_keep_set_t;

#define WL_PKT_FILTER_FIXED_LEN		  OFFSETOF(wl_pkt_filter_t, u)
#define WL_PKT_FILTER_PATTERN_FIXED_LEN	  OFFSETOF(wl_pkt_filter_pattern_t, mask_and_pattern)

typedef struct wl_pkt_filter_enable {
	uint32	id;		
	uint32	enable;		
} wl_pkt_filter_enable_t;

typedef struct wl_pkt_filter_list {
	uint32	num;		
	wl_pkt_filter_t	filter[1];	
} wl_pkt_filter_list_t;

#define WL_PKT_FILTER_LIST_FIXED_LEN	  OFFSETOF(wl_pkt_filter_list_t, filter)

typedef struct wl_pkt_filter_stats {
	uint32	num_pkts_matched;	
	uint32	num_pkts_forwarded;	
	uint32	num_pkts_discarded;	
} wl_pkt_filter_stats_t;

#define WL_WOWL_MAGIC           (1 << 0)    
#define WL_WOWL_NET             (1 << 1)    
#define WL_WOWL_DIS             (1 << 2)    
#define WL_WOWL_RETR            (1 << 3)    
#define WL_WOWL_BCN             (1 << 4)    
#define WL_WOWL_TST             (1 << 5)    
#define WL_WOWL_M1              (1 << 6)    
#define WL_WOWL_EAPID           (1 << 7)    
#define WL_WOWL_PME_GPIO        (1 << 8)    
#define WL_WOWL_NEEDTKIP1       (1 << 9)    
#define WL_WOWL_GTK_FAILURE     (1 << 10)   
#define WL_WOWL_EXTMAGPAT       (1 << 11)   
#define WL_WOWL_ARPOFFLOAD      (1 << 12)   
#define WL_WOWL_WPA2            (1 << 13)   
#define WL_WOWL_KEYROT          (1 << 14)   
#define WL_WOWL_BCAST           (1 << 15)   
#define WL_WOWL_SCANOL          (1 << 16)   
#define WL_WOWL_TCPKEEP_TIME    (1 << 17)   
#define WL_WOWL_MDNS_CONFLICT   (1 << 18)   
#define WL_WOWL_MDNS_SERVICE    (1 << 19)   
#define WL_WOWL_TCPKEEP_DATA    (1 << 20)   
#define WL_WOWL_FW_HALT         (1 << 21)   
#define WL_WOWL_ENAB_HWRADIO    (1 << 22)   
#define WL_WOWL_MIC_FAIL        (1 << 23)   
#define WL_WOWL_LINKDOWN        (1 << 31)   

#define MAGIC_PKT_MINLEN	102    
#define MAGIC_PKT_NUM_MAC_ADDRS	16

typedef enum {
	wowl_pattern_type_bitmap = 0,
	wowl_pattern_type_arp,
	wowl_pattern_type_na
} wowl_pattern_type_t;

typedef struct wl_wowl_pattern {
	uint32		    masksize;		
	uint32		    offset;		
	uint32		    patternoffset;	
	uint32		    patternsize;	
	uint32		    id;			
	uint32		    reasonsize;		
	wowl_pattern_type_t type;		

} wl_wowl_pattern_t;

typedef struct wl_wowl_pattern_list {
	uint			count;
	wl_wowl_pattern_t	pattern[1];
} wl_wowl_pattern_list_t;

typedef struct wl_wowl_wakeind {
	uint8	pci_wakeind;	
	uint32	ucode_wakeind;	
} wl_wowl_wakeind_t;

typedef struct {
	uint32		pktlen;		    
	void		*sdu;
} tcp_keepalive_wake_pkt_infop_t;

typedef BWL_PRE_PACKED_STRUCT struct nbr_element {
	uint8 id;
	uint8 len;
	struct ether_addr bssid;
	uint32 bssid_info;
	uint8 reg;
	uint8 channel;
	uint8 phytype;
} BWL_POST_PACKED_STRUCT nbr_element_t;

#include <packed_section_end.h>

#include <packed_section_start.h>

#include <packed_section_end.h>

typedef struct bcnreq {
	uint8 bcn_mode;
	int dur;
	int channel;
	struct ether_addr da;
	uint16 random_int;
	wlc_ssid_t ssid;
	uint16 reps;
} bcnreq_t;

typedef struct rrmreq {
	struct ether_addr da;
	uint8 reg;
	uint8 chan;
	uint16 random_int;
	uint16 dur;
	uint16 reps;
} rrmreq_t;

typedef struct framereq {
	struct ether_addr da;
	uint8 reg;
	uint8 chan;
	uint16 random_int;
	uint16 dur;
	struct ether_addr ta;
	uint16 reps;
} framereq_t;

typedef struct statreq {
	struct ether_addr da;
	struct ether_addr peer;
	uint16 random_int;
	uint16 dur;
	uint8 group_id;
	uint16 reps;
} statreq_t;

typedef struct wlc_l2keepalive_ol_params {
	uint8 	flags;
	uint8	prio;
	uint16	period_ms;
} wlc_l2keepalive_ol_params_t;

typedef struct wlc_dwds_config {
	uint32		mode;
	struct ether_addr ea;
} wlc_dwds_config_t;

#define WLC_KCK_LEN		16
#define WLC_KEK_LEN		16
#define WLC_REPLAY_CTR_LEN	8

typedef struct wlc_rekey_info {
	uint32	offload_id;
	uint8	kek[WLC_KEK_LEN];
	uint8	kck[WLC_KCK_LEN];
	uint8	replay_counter[WLC_REPLAY_CTR_LEN];
} wlc_rekey_info_t;

#endif 
