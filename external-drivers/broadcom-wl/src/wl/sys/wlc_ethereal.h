/*
 * Structures and defines for the prism-style rx header that Ethereal
 * understands.
 * Broadcom 802.11abg Networking Device Driver
 *  Derived from http://airsnort.shmoo.com/orinoco-09b-packet-1.diff
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
 * $Id: wlc_ethereal.h 328348 2012-04-18 22:57:38Z $
 */

#ifndef _WLC_ETHEREAL_H_
#define _WLC_ETHEREAL_H_

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW			(ETH_P_ECONET + 1)
#endif 

#ifndef ARPHRD_ETHER
#define ARPHRD_ETHER			1 
#endif 

#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM		802 
#endif 

#define DNAMELEN			16  

#define WL_MON_FRAME			0x0041	
#define WL_MON_FRAME_HOSTTIME		0x1041	
#define WL_MON_FRAME_MACTIME		0x2041	
#define WL_MON_FRAME_CHANNEL		0x3041	
#define WL_MON_FRAME_RSSI		0x4041	
#define WL_MON_FRAME_SQ			0x5041	
#define WL_MON_FRAME_SIGNAL		0x6041	
#define WL_MON_FRAME_NOISE		0x7041	
#define WL_MON_FRAME_RATE		0x8041	
#define WL_MON_FRAME_ISTX		0x9041	
#define WL_MON_FRAME_FRMLEN		0xA041	

#define P80211ITEM_OK			0	
#define P80211ITEM_NO_VALUE		1	

typedef struct p80211item
{
	uint32		did;
	uint16		status;
	uint16		len;
	uint32		data;
} p80211item_t;

typedef struct p80211msg
{
	uint32	msgcode;
	uint32	msglen;
	uint8		devname[DNAMELEN];
	p80211item_t	hosttime;
	p80211item_t	mactime;
	p80211item_t	channel;
	p80211item_t	rssi;
	p80211item_t	sq;
	p80211item_t	signal;
	p80211item_t	noise;
	p80211item_t	rate;
	p80211item_t	istx;
	p80211item_t	frmlen;
} p80211msg_t;

#define WLANCAP_MAGIC_COOKIE_V1 0x80211001  

#define WLANCAP_PHY_UNKOWN		0	
#define WLANCAP_PHY_FHSS_97		1	
#define WLANCAP_PHY_DSSS_97		2	
#define WLANCAP_PHY_IR			3	
#define WLANCAP_PHY_DSSS_11B		4	
#define WLANCAP_PHY_PBCC_11B		5	
#define WLANCAP_PHY_OFDM_11G		6	
#define WLANCAP_PHY_PBCC_11G		7	
#define WLANCAP_PHY_OFDM_11A		8	
#define WLANCAP_PHY_OFDM_11N		9	

#define WLANCAP_ENCODING_UNKNOWN	0	
#define WLANCAP_ENCODING_CCK		1	
#define WLANCAP_ENCODING_PBCC		2	
#define WLANCAP_ENCODING_OFDM		3	

#define WLANCAP_SSI_TYPE_NONE		0	
#define WLANCAP_SSI_TYPE_NORM		1	
#define WLANCAP_SSI_TYPE_DBM		2	
#define WLANCAP_SSI_TYPE_RAW		3	

#define WLANCAP_PREAMBLE_UNKNOWN	0	
#define WLANCAP_PREAMBLE_SHORT		1	
#define WLANCAP_PREAMBLE_LONG		2	
#define WLANCAP_PREAMBLE_MIMO_MM	3	
#define WLANCAP_PREAMBLE_MIMO_GF	4	

typedef struct wlan_header_v1 {
	uint32	version;
	uint32	length;
	uint32	mactime_h;
	uint32	mactime_l;
	uint32	hosttime_h;
	uint32	hosttime_l;
	uint32	phytype;
	uint32	channel;
	uint32	datarate;
	uint32	antenna;
	uint32	priority;
	uint32	ssi_type;
	int32	ssi_signal;
	int32	ssi_noise;
	uint32	preamble;
	uint32	encoding;
} wlan_header_v1_t;

#endif 
