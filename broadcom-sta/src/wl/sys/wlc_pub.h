/*
 * Common (OS-independent) definitions for
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_pub.h 458427 2014-02-26 23:12:38Z $
 */

#ifndef _wlc_pub_h_
#define _wlc_pub_h_

#include <wlc_types.h>
#include <wlc_utils.h>
#include "proto/802.11.h"
#include "proto/bcmevent.h"

#define	MAX_TIMERS	(34 + WLC_MAXMFPS + WLC_MAXDLS_TIMERS + (2 * WLC_MAXDPT))

#define	WLC_NUMRATES	16	
#define	MAXMULTILIST	32	
#define	D11_PHY_HDR_LEN	6	

#define	PHY_TYPE_A	0	
#define	PHY_TYPE_G	2	
#define	PHY_TYPE_N	4	
#define	PHY_TYPE_LP	5	
#define	PHY_TYPE_SSN	6	
#define	PHY_TYPE_HT	7	
#define	PHY_TYPE_LCN	8	
#define	PHY_TYPE_LCNXN	9	

#define WLC_10_MHZ	10	
#define WLC_20_MHZ	20	
#define WLC_40_MHZ	40	
#define WLC_80_MHZ	80	
#define WLC_160_MHZ	160	

#define CHSPEC_WLC_BW(chanspec)(CHSPEC_IS160(chanspec) ? WLC_160_MHZ : \
				CHSPEC_IS80(chanspec) ? WLC_80_MHZ : \
				CHSPEC_IS40(chanspec) ? WLC_40_MHZ : \
				CHSPEC_IS20(chanspec) ? WLC_20_MHZ : \
							WLC_10_MHZ)

#define	WLC_RSSI_MINVAL		-200	
#define	WLC_RSSI_NO_SIGNAL	-91	
#define	WLC_RSSI_VERY_LOW	-80	
#define	WLC_RSSI_LOW		-70	
#define	WLC_RSSI_GOOD		-68	
#define	WLC_RSSI_VERY_GOOD	-58	
#define	WLC_RSSI_EXCELLENT	-57	

#define	PREFSZ			160
#define WLPREFHDRS(h, sz)	OSL_PREF_RANGE_ST((h), (sz))

struct wlc_info;
struct wlc_hw_info;
struct wlc_bsscfg;
struct wlc_if;

typedef struct wlc_tunables {
	int ntxd;	
	int nrxd;	
	int rxbufsz;			
	int nrxbufpost;			
	int maxscb;			
	int ampdunummpdu2streams;	
	int ampdunummpdu3streams;	
	int maxpktcb;			
	int maxdpt;			
	int maxucodebss;		
	int maxucodebss4;		
	int maxbss;			
	int datahiwat;			
	int ampdudatahiwat;		
	int rxbnd;			
	int txsbnd;			
	int pktcbnd;			
	int dngl_mem_restrict_rxdma;	
	int rpctxbufpost;
	int pkt_maxsegs;
	int maxscbcubbies;		
	int maxbsscfgcubbies;		
	int max_notif_servers;		
	int max_notif_clients;		
	int max_mempools;		
	int maxtdls;			
	int amsdu_resize_buflen;	
	int ampdu_pktq_size;		
	int ampdu_pktq_fav_size;	
	int ntxd_large;	
	int nrxd_large;	
	int wlfcfifocreditac0;		
	int wlfcfifocreditac1;		
	int wlfcfifocreditac2;		
	int wlfcfifocreditac3;		
	int wlfcfifocreditbcmc;		
	int wlfcfifocreditother;	
	int scan_settle_time;		
	int wlfc_fifo_cr_pending_thresh_ac_bk;	
	int wlfc_fifo_cr_pending_thresh_ac_be;	
	int wlfc_fifo_cr_pending_thresh_ac_vi;	
	int wlfc_fifo_cr_pending_thresh_ac_vo;	
	int ampdunummpdu1stream;	
} wlc_tunables_t;

typedef struct wlc_rateset {
	uint	count;			
	uint8	rates[WLC_NUMRATES];	
	uint8	htphy_membership;	
	uint8	mcs[MCSSET_LEN];	
	uint16  vht_mcsmap;		
} wlc_rateset_t;

typedef void *wlc_pkt_t;

typedef struct wlc_event {
	wl_event_msg_t	event;		
	struct ether_addr *addr;	
	struct wlc_if	*wlcif;		
	void		*data;		
	struct wlc_event *next;		
} wlc_event_t;

typedef struct wlc_bss_info
{
	struct ether_addr BSSID;	
	uint16		flags;		
	uint8		SSID_len;	
	uint8		SSID[32];	
	int16		RSSI;		
	int16		SNR;		
	uint16		beacon_period;	
	uint16		atim_window;	
	chanspec_t	chanspec;	
	int8		infra;		
	wlc_rateset_t	rateset;	
	uint8		dtim_period;	
	int8		phy_noise;	
	uint16		capability;	
	struct dot11_bcn_prb *bcn_prb;	
	uint16		bcn_prb_len;	
	uint8		wme_qosinfo;	
	struct rsn_parms wpa;
	struct rsn_parms wpa2;
	uint16		qbss_load_aac;	

	uint8		qbss_load_chan_free;	
	uint8		mcipher;	
	uint8		wpacfg;		
	uint16		mdid;		
	uint16		flags2;		
	uint32		vht_capabilities;
	uint16		vht_rxmcsmap;
	uint16		vht_txmcsmap;
} wlc_bss_info_t;

#define WLC_BSS_54G             0x0001  
#define WLC_BSS_RSSI_ON_CHANNEL 0x0002  
#define WLC_BSS_WME             0x0004  
#define WLC_BSS_BRCM            0x0008  
#define WLC_BSS_WPA             0x0010  
#define WLC_BSS_HT              0x0020  
#define WLC_BSS_40MHZ           0x0040  
#define WLC_BSS_WPA2            0x0080  
#define WLC_BSS_BEACON          0x0100  
#define WLC_BSS_40INTOL         0x0200  
#define WLC_BSS_SGI_20          0x0400  
#define WLC_BSS_SGI_40          0x0800  
#define WLC_BSS_CACHE           0x2000  
#define WLC_BSS_FBT             0x8000  

#define WLC_BSS_OVERDS_FBT      0x0001  
#define WLC_BSS_VHT             0x0002  
#define WLC_BSS_80MHZ           0x0004  
#define WLC_BSS_SGI_80			0x0008	

#define WLC_ENOIOCTL	1 
#define WLC_EINVAL	2 
#define WLC_ETOOSMALL	3 
#define WLC_ETOOBIG	4 
#define WLC_ERANGE	5 
#define WLC_EDOWN	6 
#define WLC_EUP		7 
#define WLC_ENOMEM	8 
#define WLC_EBUSY	9 

#define IOVF_BSSCFG_STA_ONLY	(1<<0)	
#define IOVF_BSSCFG_AP_ONLY	(1<<1)	
#define IOVF_BSS_SET_DOWN (1<<2)	

#define IOVF_MFG	(1<<3)  
#define IOVF_WHL	(1<<4)	
#define IOVF_NTRL	(1<<5)	

#define IOVF_SET_UP	(1<<6)	
#define IOVF_SET_DOWN	(1<<7)	
#define IOVF_SET_CLK	(1<<8)	
#define IOVF_SET_BAND	(1<<9)	

#define IOVF_GET_UP	(1<<10)	
#define IOVF_GET_DOWN	(1<<11)	
#define IOVF_GET_CLK	(1<<12)	
#define IOVF_GET_BAND	(1<<13)	
#define IOVF_OPEN_ALLOW	(1<<14)	

#define IOVF_BMAC_IOVAR	(1<<15) 

#define BAR0_INVALID		(1 << 0)
#define VENDORID_INVALID	(1 << 1)
#define NOCARD_PRESENT		(1 << 2)
#define PHY_PLL_ERROR		(1 << 3)
#define DEADCHIP_ERROR		(1 << 4)
#define MACSPEND_TIMOUT		(1 << 5)
#define MACSPEND_WOWL_TIMOUT	(1 << 6)
#define DMATX_ERROR		(1 << 7)
#define DMARX_ERROR		(1 << 8)
#define DESCRIPTOR_ERROR	(1 << 9)
#define CARD_NOT_POWERED	(1 << 10)

#define WL_HEALTH_LOG(w, s)	do {} while (0)

typedef int (*watchdog_fn_t)(void *handle);
typedef int (*up_fn_t)(void *handle);
typedef int (*down_fn_t)(void *handle);
typedef int (*dump_fn_t)(void *handle, struct bcmstrbuf *b);

typedef int (*iovar_fn_t)(void *handle, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *params, uint plen, void *arg, int alen,
	int vsize, struct wlc_if *wlcif);

#define WLC_IOCF_BSSCFG_STA_ONLY	(1<<0)	
#define WLC_IOCF_BSSCFG_AP_ONLY	(1<<1)	

#define WLC_IOCF_MFG	(1<<2)  

#define WLC_IOCF_DRIVER_UP	(1<<3)	
#define WLC_IOCF_DRIVER_DOWN	(1<<4)	
#define WLC_IOCF_CORE_CLK	(1<<5)	
#define WLC_IOCF_FIXED_BAND	(1<<6)	
#define WLC_IOCF_OPEN_ALLOW	(1<<7)	

typedef int (*wlc_ioctl_fn_t)(void *handle, int cmd, void *arg, int len, struct wlc_if *wlcif);

typedef struct wlc_ioctl_cmd_s {
	uint16 cmd;			
	uint16 flags;			
	int min_len;			
} wlc_ioctl_cmd_t;

typedef struct wlc_pub {
	void		*wlc;
	struct ether_addr	cur_etheraddr;	
	uint		unit;			
	uint		corerev;		
	osl_t		*osh;			
	si_t		*sih_obsolete;		
	char		*vars_obsolete;		
	uint		vars_size_obsolete;	
	bool		up;			
	bool		hw_off;			
	wlc_tunables_t *tunables;		
	bool		hw_up;			
	bool		_piomode;		 
	uint		_nbands;		
	uint		now;			

	bool		promisc;		
	bool		delayed_down;		
	bool		_ap;			
	bool		_apsta;			
	bool		_assoc_recreate;	
	int		_wme;			
	uint8		_mbss;			
	bool		associated;		

	bool            phytest_on;             
	bool		bf_preempt_4306;	

	bool		_wowl;			
	bool		_wowl_active;		
	bool		_ampdu_tx;		
	bool		_ampdu_rx;		
	bool		_amsdu_tx;		
	bool		_cac;			
	uint		_spect_management;	
	uint8		_n_enab;		
	bool		_n_reqd;		

	uint8		_vht_enab;		

	int8		_coex;			
	bool		_priofc;		
	bool		phy_bw40_capable;	
	bool		phy_bw80_capable;	

	uint32		wlfeatureflag;		
	int			psq_pkts_total;		

	uint16		txmaxpkts;		

	uint32		swdecrypt;		

	int 		bcmerror;		

	mbool		radio_disabled;		
	mbool		last_radio_disabled;	
	bool		radio_active;		
	uint16		roam_time_thresh;	
	bool		align_wd_tbtt;		
	uint16		boardrev;		
	uint8		sromrev;		
	uint32		boardflags;		
	uint32		boardflags2;		

	wl_cnt_t	*_cnt;			
	wl_wme_cnt_t	*_wme_cnt;		

	uint8		_ndis_cap;		
	bool		_extsta;		
	bool		_pkt_filter;		
	bool		phy_11ncapable;		
	bool		_fbt;			
	pktpool_t	*pktpool;		
	uint8		_ampdumac;	
	bool		_wleind;
	bool		_sup_enab;
	uint		driverrev;		

	bool		_11h;
	bool		_11d;
#ifdef WLCNTRY
	bool		_autocountry;
#endif
	uint32		health;
	uint8		d11tpl_phy_hdr_len; 
	uint		wsec_max_rcmta_keys;
	uint		max_addrma_idx;
	uint16		m_seckindxalgo_blk;
	uint		m_seckindxalgo_blk_sz;
	uint16		m_coremask_blk;
	uint16		m_coremask_blk_wowl;
#ifdef WL_BEAMFORMING
	bool		_txbf;
#endif 

	bool		wet_tunnel;	
	int		_ol;			

	uint16		vht_features;		

	bool		_ampdu_hostreorder;

	int8		_pktc;			
	bool		_tdls_support;		
	bool		_okc;
	bool		_p2po;			
	bool		_anqpo;			
	bool		_wl_rxearlyrc;
	bool		_tiny_pktjoin;
	si_t		*sih;			
	char		*vars;			
	uint		vars_size;		
	bool		_proxd;			

	bool		_arpoe_support;
	bool		_11u;

	bool 		_lpc_algo;
	bool		_relmcast;		
	bool		_relmcast_support;	

	bool		_l2_filter;		

	uint		bcn_tmpl_len;		
#ifdef WL_OFFLOADSTATS
	uint32		offld_cnt_received[4];
	uint32		offld_cnt_consumed[4];
#endif 
#ifdef WL_INTERRUPTSTATS
	uint32		intr_cnt[32];
#endif 

#ifdef TCPKAOE
	bool		_icmpoe;		
	bool		_tcp_keepalive;		
#endif 
	bool _olpc;				
} wlc_pub_t;

typedef struct	wl_rxsts {
	uint	pkterror;		
	uint	phytype;		
	chanspec_t chanspec;		
	uint16	datarate;		
	uint8	mcs;			
	uint8	htflags;		
	uint	antenna;		
	uint	pktlength;		
	uint32	mactime;		
	uint	sq;			
	int32	signal;			
	int32	noise;			
	uint	preamble;		
	uint	encoding;		
	uint	nfrmtype;		
	struct wl_if *wlif;		
	uint8	nss;			
	uint8   coding;
	uint16	aid;			
	uint8	gid;			
	uint8   bw;			
	uint16	vhtflags;		
	uint8	bw_nonht;		
	uint32	ampdu_counter;		
} wl_rxsts_t;

typedef struct	wl_txsts {
	uint	pkterror;		
	uint	phytype;		
	chanspec_t chanspec;		
	uint16	datarate;		
	uint8	mcs;			
	uint8	htflags;		
	uint	antenna;		
	uint	pktlength;		
	uint32	mactime;		
	uint	preamble;		
	uint	encoding;		
	uint	nfrmtype;		
	uint	txflags;		
	uint	retries;		
	struct wl_if *wlif;		
} wl_txsts_t;

typedef struct wlc_if_stats {

	uint32	txframe;		
	uint32	txbyte;			
	uint32	txerror;		
	uint32  txnobuf;		
	uint32  txrunt;			
	uint32  txfail;			

	uint32	rxframe;		
	uint32	rxbyte;			
	uint32	rxerror;		
	uint32	rxnobuf;		
	uint32  rxrunt;			
	uint32  rxfragerr;		

	uint32	txretry;		
	uint32	txretrie;		
	uint32	txfrmsnt;		
	uint32	txmulti;		
	uint32	txfrag;			

	uint32	rxmulti;		

} wlc_if_stats_t;

#define WL_RXS_CRC_ERROR		0x00000001 
#define WL_RXS_RUNT_ERROR		0x00000002 
#define WL_RXS_ALIGN_ERROR		0x00000004 
#define WL_RXS_OVERSIZE_ERROR		0x00000008 
#define WL_RXS_WEP_ICV_ERROR		0x00000010 
#define WL_RXS_WEP_ENCRYPTED		0x00000020 
#define WL_RXS_PLCP_SHORT		0x00000040 
#define WL_RXS_DECRYPT_ERR		0x00000080 
#define WL_RXS_OTHER_ERR		0x80000000 

#define WL_RXS_PHY_A			0x00000000 
#define WL_RXS_PHY_B			0x00000001 
#define WL_RXS_PHY_G			0x00000002 
#define WL_RXS_PHY_N			0x00000004 

#define WL_RXS_ENCODING_UNKNOWN		0x00000000
#define WL_RXS_ENCODING_DSSS_CCK	0x00000001 
#define WL_RXS_ENCODING_OFDM		0x00000002 
#define WL_RXS_ENCODING_HT		0x00000003 
#define WL_RXS_ENCODING_VHT		0x00000004 

#define WL_RXS_UNUSED_STUB		0x0		
#define WL_RXS_PREAMBLE_SHORT		0x00000001	
#define WL_RXS_PREAMBLE_LONG		0x00000002	
#define WL_RXS_PREAMBLE_HT_MM		0x00000003	
#define WL_RXS_PREAMBLE_HT_GF		0x00000004	

#define WL_RXS_HTF_BW_MASK		0x07
#define WL_RXS_HTF_40			0x01
#define WL_RXS_HTF_20L			0x02
#define WL_RXS_HTF_20U			0x04
#define WL_RXS_HTF_SGI			0x08
#define WL_RXS_HTF_STBC_MASK		0x30
#define WL_RXS_HTF_STBC_SHIFT		4
#define WL_RXS_HTF_LDPC			0x40

#define WL_RXS_VHTF_STBC		0x01
#define WL_RXS_VHTF_TXOP_PS		0x02
#define WL_RXS_VHTF_SGI			0x04
#define WL_RXS_VHTF_SGI_NSYM_DA		0x08
#define WL_RXS_VHTF_LDPC_EXTRA		0x10
#define WL_RXS_VHTF_BF			0x20
#define WL_RXS_VHTF_DYN_BW_NONHT 0x40

#define WL_RXS_VHTF_CODING_LDCP		0x01

#define WL_RXS_VHT_BW_20		0
#define WL_RXS_VHT_BW_40		1
#define WL_RXS_VHT_BW_20L		2
#define WL_RXS_VHT_BW_20U		3
#define WL_RXS_VHT_BW_80		4
#define WL_RXS_VHT_BW_40L		5
#define WL_RXS_VHT_BW_40U		6
#define WL_RXS_VHT_BW_20LL		7
#define WL_RXS_VHT_BW_20LU		8
#define WL_RXS_VHT_BW_20UL		9
#define WL_RXS_VHT_BW_20UU		10

#define WL_RXS_NFRM_AMPDU_FIRST		0x00000001 
#define WL_RXS_NFRM_AMPDU_SUB		0x00000002 
#define WL_RXS_NFRM_AMSDU_FIRST		0x00000004 
#define WL_RXS_NFRM_AMSDU_SUB		0x00000008 
#define WL_RXS_NFRM_AMPDU_NONE		0x00000100 

#define WL_TXS_TXF_FAIL		0x01	
#define WL_TXS_TXF_CTS		0x02	
#define WL_TXS_TXF_RTSCTS 	0x04	

#define BPRESET_ENAB(pub)	(0)

#define	AP_ENAB(pub)	(0)

#define APSTA_ENAB(pub)	(0)

#define PSTA_ENAB(pub)	(0)

#if defined(PKTC_DONGLE)
#define PKTC_ENAB(pub)	((pub)->_pktc)
#else
#define PKTC_ENAB(pub)	(0)
#endif

#if defined(WL_BEAMFORMING)
	#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define TXBF_ENAB(pub)		((pub)->_txbf)
	#elif defined(WLTXBF_DISABLED)
		#define TXBF_ENAB(pub)		(0)
	#else
		#define TXBF_ENAB(pub)		(1)
	#endif
#else
	#define TXBF_ENAB(pub)			(0)
#endif 

#define STA_ONLY(pub)	(!AP_ENAB(pub))
#define AP_ONLY(pub)	(AP_ENAB(pub) && !APSTA_ENAB(pub))

	#define PROP_TXSTATUS_ENAB(pub)		0

#define WLOFFLD_CAP(wlc)	((wlc)->ol != NULL)
#define WLOFFLD_ENAB(pub)	((pub)->_ol)
#define WLOFFLD_BCN_ENAB(pub)	((pub)->_ol & OL_BCN_ENAB)
#define WLOFFLD_ARP_ENAB(pub)	((pub)->_ol & OL_ARP_ENAB)
#define WLOFFLD_ND_ENAB(pub)	((pub)->_ol & OL_ND_ENAB)
#define WLOFFLD_ARM_TX(pub)	((pub)->_ol & OL_ARM_TX_ENAB)

#define WOWL_ENAB(pub) ((pub)->_wowl)
#define WOWL_ACTIVE(pub) ((pub)->_wowl_active)

	#define DPT_ENAB(pub) 0

	#define TDLS_SUPPORT(pub)		(0)
	#define TDLS_ENAB(pub)			(0)

#define WLDLS_ENAB(pub) 0

#ifdef WL_OKC
	#if defined(WL_ENAB_RUNTIME_CHECK)
#define OKC_ENAB(pub) ((pub)->_okc)
	#elif defined(WL_OKC_DISABLED)
		#define OKC_ENAB(pub)		(0)
#else
		#define OKC_ENAB(pub)		((pub)->_okc)
#endif
#else
	#define OKC_ENAB(pub)			(0)
#endif 

#define WLBSSLOAD_ENAB(pub)	(0)

	#define MCNX_ENAB(pub) 0

	#define P2P_ENAB(pub) 0

	#define MCHAN_ENAB(pub) (0)
	#define MCHAN_ACTIVE(pub) (0)

	#define MQUEUE_ENAB(pub) (0)

	#define BTA_ENAB(pub) (0)

#define PIO_ENAB(pub) 0

#define CAC_ENAB(pub) ((pub)->_cac)

#define COEX_ACTIVE(wlc) 0
#define COEX_ENAB(pub) 0

#define	RXIQEST_ENAB(pub)	(0)

#define EDCF_ENAB(pub) (WME_ENAB(pub))
#define QOS_ENAB(pub) (WME_ENAB(pub) || N_ENAB(pub))

#define PRIOFC_ENAB(pub) ((pub)->_priofc)

#define MONITOR_ENAB(wlc)	((wlc)->monitor != 0)

#define PROMISC_ENAB(wlc_pub)	(wlc_pub)->promisc

#define WLC_SENDUP_MGMT_ENAB(cfg)	0

	#define TOE_ENAB(pub)			(0)

	#define ARPOE_SUPPORT(pub)		(0)
	#define ARPOE_ENAB(pub)			(0)
#define ICMPOE_ENAB(pub) 0

	#define NWOE_ENAB(pub)			(0)

#define TRAFFIC_MGMT_ENAB(pub) 0

	#define L2_FILTER_ENAB(pub)		(0)

#define NET_DETECT_ENAB(pub) 0

#ifdef PACKET_FILTER
#define PKT_FILTER_ENAB(pub) 	((pub)->_pkt_filter)
#else
#define PKT_FILTER_ENAB(pub)	0
#endif

#ifdef P2PO
	#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define P2PO_ENAB(pub) ((pub)->_p2po)
	#elif defined(P2PO_DISABLED)
		#define P2PO_ENAB(pub)	(0)
	#else
		#define P2PO_ENAB(pub)	(1)
	#endif
#else
	#define P2PO_ENAB(pub) 0
#endif 

#ifdef ANQPO
	#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define ANQPO_ENAB(pub) ((pub)->_anqpo)
	#elif defined(ANQPO_DISABLED)
		#define ANQPO_ENAB(pub)	(0)
	#else
		#define ANQPO_ENAB(pub)	(1)
	#endif
#else
	#define ANQPO_ENAB(pub) 0
#endif 

#define ASSOC_RECREATE_ENAB(pub) 0

#define WLFBT_ENAB(pub)		(0)

#if 0 && (NDISVER >= 0x0620)
#define WIN7_AND_UP_OS(pub)	((pub)->_ndis_cap)
#else
#define WIN7_AND_UP_OS(pub)	0
#endif

	#define NDOE_ENAB(pub) (0)

	#define WLEXTSTA_ENAB(pub)	0

	#define IBSS_PEER_GROUP_KEY_ENAB(pub) (0)

	#define IBSS_PEER_DISCOVERY_EVENT_ENAB(pub) (0)

	#define IBSS_PEER_MGMT_ENAB(pub) (0)

	#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define WLEIND_ENAB(pub) ((pub)->_wleind)
	#elif defined(WLEIND_DISABLED)
		#define WLEIND_ENAB(pub) (0)
	#else
		#define WLEIND_ENAB(pub) (1)
	#endif

	#define CCX_ENAB(pub) 0

	#define BCMAUTH_PSK_ENAB(pub) 0

	#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define SUP_ENAB(pub)	((pub)->_sup_enab)
	#elif defined(BCMSUP_PSK_DISABLED)
		#define SUP_ENAB(pub)	(0)
	#else
		#define SUP_ENAB(pub)	(1)
	#endif

#define WLC_PREC_BMP_ALL		MAXBITVAL(WLC_PREC_COUNT)

#define WLC_PREC_BMP_AC_BE	(NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_BE)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_BE)) |	\
				NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_EE)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_EE)))
#define WLC_PREC_BMP_AC_BK	(NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_BK)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_BK)) |	\
				NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_NONE)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_NONE)))
#define WLC_PREC_BMP_AC_VI	(NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_CL)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_CL)) |	\
				NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_VI)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_VI)))
#define WLC_PREC_BMP_AC_VO	(NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_VO)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_VO)) |	\
				NBITVAL(WLC_PRIO_TO_PREC(PRIO_8021D_NC)) |	\
				NBITVAL(WLC_PRIO_TO_HI_PREC(PRIO_8021D_NC)))

#define WME_ENAB(pub) ((pub)->_wme != OFF)
#define WME_AUTO(wlc) ((wlc)->pub->_wme == AUTO)

#ifdef WLCNTRY
#define WLC_AUTOCOUNTRY_ENAB(wlc) ((wlc)->pub->_autocountry)
#else
#define WLC_AUTOCOUNTRY_ENAB(wlc) FALSE
#endif

#define WL11D_ENAB(wlc)	((wlc)->pub->_11d)

#define WL11H_ENAB(wlc)	((wlc)->pub->_11h)

#define WL11U_ENAB(wlc)	FALSE

#define WLPROBRESP_SW_ENAB(wlc)	FALSE

#define LPC_ENAB(wlc)	(FALSE)

#if defined(WLOLPC)
#define OLPC_ENAB(wlc)	((wlc)->pub->_olpc)
#else
#define OLPC_ENAB(wlc)	(FALSE)
#endif 

#ifdef WL_RELMCAST
	#if defined(WL_ENAB_RUNTIME_CHECK)
		#define RMC_SUPPORT(pub)	((pub)->_relmcast_support)
		#define RMC_ENAB(pub)		((pub)->_relmcast)
	#elif defined(WL_RELMCAST_DISABLED)
		#define RMC_SUPPORT(pub)	(0)
		#define RMC_ENAB(pub)		(0)
	#else
		#define RMC_SUPPORT(pub)	(1)
		#define RMC_ENAB(pub)		((pub)->_relmcast)
	#endif
#else
	#define RMC_SUPPORT(pub)		(0)
	#define RMC_ENAB(pub)			(0)
#endif 

#define WLC_USE_COREFLAGS	0xffffffff	

#define WLC_UPDATE_STATS(wlc)	1	
#define WLCNTINCR(a)		((a)++)	
#define WLCNTCONDINCR(c, a)	do { if (c) (a)++; } while (0)	
#define WLCNTDECR(a)		((a)--)	
#define WLCNTADD(a,delta)	((a) += (delta)) 
#define WLCNTSET(a,value)	((a) = (value)) 
#define WLCNTVAL(a)		(a)	

#if !defined(RXCHAIN_PWRSAVE) && !defined(RADIO_PWRSAVE)
#define WLPWRSAVERXFADD(wlc, v)
#define WLPWRSAVERXFINCR(wlc)
#define WLPWRSAVETXFINCR(wlc)
#define WLPWRSAVERXFVAL(wlc)	0
#define WLPWRSAVETXFVAL(wlc)	0
#endif

struct wlc_dpc_info {
	uint processed;
};

extern void *wlc_attach(void *wl, uint16 vendor, uint16 device, uint unit, bool piomode,
	osl_t *osh, void *regsva, uint bustype, void *btparam, uint *perr);
extern uint wlc_detach(struct wlc_info *wlc);
extern int  wlc_up(struct wlc_info *wlc);
extern uint wlc_down(struct wlc_info *wlc);

extern int wlc_set(struct wlc_info *wlc, int cmd, int arg);
extern int wlc_get(struct wlc_info *wlc, int cmd, int *arg);
extern int wlc_iovar_getint(struct wlc_info *wlc, const char *name, int *arg);
extern int wlc_iovar_setint(struct wlc_info *wlc, const char *name, int arg);
extern bool wlc_chipmatch(uint16 vendor, uint16 device);
extern void wlc_init(struct wlc_info *wlc);
extern void wlc_reset(struct wlc_info *wlc);
#ifdef MCAST_REGEN
extern int32 wlc_mcast_reverse_translation(struct ether_header *eh);
#endif 

extern void wlc_intrson(struct wlc_info *wlc);
extern uint32 wlc_intrsoff(struct wlc_info *wlc);
extern void wlc_intrsrestore(struct wlc_info *wlc, uint32 macintmask);
extern bool wlc_intrsupd(struct wlc_info *wlc);
extern bool wlc_isr(struct wlc_info *wlc, bool *wantdpc);
extern bool wlc_dpc(struct wlc_info *wlc, bool bounded, struct wlc_dpc_info *dpc);

extern bool wlc_sendpkt(struct wlc_info *wlc, void *sdu, struct wlc_if *wlcif);
extern bool wlc_send80211_specified(wlc_info_t *wlc, void *sdu, uint32 rspec, struct wlc_if *wlcif);
extern bool wlc_send80211_raw(struct wlc_info *wlc, wlc_if_t *wlcif, void *p, uint ac);
extern int wlc_iovar_op(struct wlc_info *wlc, const char *name, void *params, int p_len, void *arg,
	int len, bool set, struct wlc_if *wlcif);
extern int wlc_ioctl(struct wlc_info *wlc, int cmd, void *arg, int len, struct wlc_if *wlcif);

extern void wlc_statsupd(struct wlc_info *wlc);

extern wlc_pub_t *wlc_pub(void *wlc);

extern void tcm_sem_enter(wlc_info_t *wlc);
extern void tcm_sem_exit(wlc_info_t *wlc);
extern void tcm_sem_cleanup(wlc_info_t *wlc);

extern int wlc_module_register(wlc_pub_t *pub, const bcm_iovar_t *iovars,
                               const char *name, void *hdl, iovar_fn_t iovar_fn,
                               watchdog_fn_t watchdog_fn, up_fn_t up_fn, down_fn_t down_fn);
extern int wlc_module_unregister(wlc_pub_t *pub, const char *name, void *hdl);
extern int wlc_module_add_ioctl_fn(wlc_pub_t *pub, void *hdl,
                                   wlc_ioctl_fn_t ioctl_fn,
                                   int num_cmds, const wlc_ioctl_cmd_t *ioctls);
extern int wlc_module_remove_ioctl_fn(wlc_pub_t *pub, void *hdl);

#define WLC_RPCTX_PARAMS        32

extern void wlc_wlcif_stats_get(wlc_info_t *wlc, wlc_if_t *wlcif, wlc_if_stats_t *wlcif_stats);
extern wlc_if_t *wlc_wlcif_get_by_index(wlc_info_t *wlc, uint idx);

#if defined(BCMDBG)

#define WLC_PERF_STATS_ISR			0x01
#define WLC_PERF_STATS_DPC			0x02
#define WLC_PERF_STATS_TMR_DPC		0x04
#define WLC_PERF_STATS_PRB_REQ		0x08
#define WLC_PERF_STATS_PRB_RESP		0x10
#define WLC_PERF_STATS_BCN_ISR		0x20
#define WLC_PERF_STATS_BCNS			0x40

void wlc_update_perf_stats(wlc_info_t *wlc, uint32 mask);
void wlc_update_isr_stats(wlc_info_t *wlc, uint32 macintstatus);
#endif 

#define WLC_REPLAY_CNTRS_VALUE	WPA_CAP_4_REPLAY_CNTRS

#if WLC_REPLAY_CNTRS_VALUE == WPA_CAP_16_REPLAY_CNTRS
#define PRIO2IVIDX(prio)	(prio)
#elif WLC_REPLAY_CNTRS_VALUE == WPA_CAP_4_REPLAY_CNTRS
#define PRIO2IVIDX(prio)	WME_PRIO2AC(prio)
#else
#error "Neither WPA_CAP_4_REPLAY_CNTRS nor WPA_CAP_16_REPLAY_CNTRS is used"
#endif 

#define GPIO_2_PA_CTRL_5G_0		0x4 

#ifdef WL_INTERRUPTSTATS
typedef enum {
	nMI_MACSSPNDD = 0,	
	nMI_BCNTPL,		
	nMI_TBTT,		
	nMI_BCNSUCCESS,		
	nMI_BCNCANCLD,		
	nMI_ATIMWINEND,		
	nMI_PMQ,		
	nMI_NSPECGEN_0,		
	nMI_NSPECGEN_1,		
	nMI_MACTXERR,		
	nMI_NSPECGEN_3,		
	nMI_PHYTXERR,		
	nMI_PME,		
	nMI_GP0,		
	nMI_GP1,		
	nMI_DMAINT,		
	nMI_TXSTOP,		
	nMI_CCA,		
	nMI_BG_NOISE,		
	nMI_DTIM_TBTT,		
	nMI_PRQ,		
	nMI_PWRUP,		
	nMI_BT_RFACT_STUCK,	
	nMI_BT_PRED_REQ,	
	nMI_NOTUSED,
	nMI_P2P,		
	nMI_DMATX,		
	nMI_TSSI_LIMIT,		
	nMI_RFDISABLE,		
	nMI_TFS,		
	nMI_PHYCHANGED,		
	nMI_TO			
} intr_enum;

#define WLCINC_INTRCNT(intr)	(wlc->pub->intr_cnt[(intr)]++)
#else
#define WLCINC_INTRCNT(intr)
#endif 

#if defined(CONFIG_WL) || defined(CONFIG_WL_MODULE)
#define WL_RTR()	TRUE
#else
#define WL_RTR()	FALSE
#endif 

#endif 
