/*
 * Wake-on-Wireless related header file
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
 * $Id: wlc_wowl.h 458427 2014-02-26 23:12:38Z $
*/

#ifndef _wlc_wowl_h_
#define _wlc_wowl_h_

#define WLC_WOWL_OFFLOADS

extern wowl_info_t *wlc_wowl_attach(wlc_info_t *wlc);
extern void wlc_wowl_detach(wowl_info_t *wowl);
extern bool wlc_wowl_cap(struct wlc_info *wlc);
extern bool wlc_wowl_enable(wowl_info_t *wowl);
extern uint32 wlc_wowl_clear(wowl_info_t *wowl);
void wlc_wowl_wake_reason_process(wlc_info_t *wlc);
#if defined(WLC_WOWL_OFFLOADS)
extern void wlc_wowl_set_wpa_m1(wowl_info_t *wowl);
extern void wlc_wowl_set_eapol_id(wowl_info_t *wowl);
extern int wlc_wowl_set_key_info(wowl_info_t *wowl, uint32 offload_id, void *kek,
	int kek_len,	void* kck, int kck_len, void *replay_counter, int replay_counter_len);
extern int wlc_wowl_add_offload_ipv4_arp(wowl_info_t *wowl, uint32 offload_id,
	uint8 * RemoteIPv4Address, uint8 *HostIPv4Address, uint8 * MacAddress);
extern int wlc_wowl_add_offload_ipv6_ns(wowl_info_t *wowl, uint32 offload_id,
	uint8 * RemoteIPv6Address, uint8 *SolicitedNodeIPv6Address,
	uint8 * MacAddress, uint8 * TargetIPv6Address1, uint8 * TargetIPv6Address2);

extern void wlc_wowl_set_keepalive(wowl_info_t *wowl, uint16 period_keepalive);
extern uint8 *wlc_wowl_solicitipv6_addr(uint8 *TargetIPv6Address1, uint8 *solicitaddress);
extern int wlc_wowl_remove_offload(wowl_info_t *wowl, uint32 offload_id, uint32 * type);
extern int wlc_wowl_get_replay_counter(wowl_info_t *wowl, void *replay_counter, int *len);

extern void wlc_wowl_enable_completed(wowl_info_t *wowl);
extern void wlc_wowl_disable_completed(wowl_info_t *wowl, void *wowl_host_info);
#endif 

#define WOWL_IPV4_ARP_TYPE		0
#define WOWL_IPV6_NS_TYPE		1
#define WOWL_DOT11_RSN_REKEY_TYPE	2
#define WOWL_OFFLOAD_INVALID_TYPE	3

#define	WOWL_IPV4_ARP_IDX		0
#define	WOWL_IPV6_NS_0_IDX		1
#define	WOWL_IPV6_NS_1_IDX		2
#define	WOWL_DOT11_RSN_REKEY_IDX	3
#define	WOWL_OFFLOAD_INVALID_IDX	4

#define	MAX_WOWL_OFFLOAD_ROWS		4
#define	MAX_WOWL_IPV6_ARP_PATTERNS	1
#define	MAX_WOWL_IPV6_NS_PATTERNS	2	
#define	MAX_WOWL_IPV6_NS_OFFLOADS	1	

#define	WOWL_INT_RESERVED_MASK      0xFF000000  
#define	WOWL_INT_DATA_MASK          0x00FFFFFF  
#define	WOWL_INT_PATTERN_FLAG       0x80000000  
#define	WOWL_INT_NS_TA2_FLAG        0x40000000  
#define	WOWL_INT_PATTERN_IDX_MASK   0x0F000000  
#define	WOWL_INT_PATTERN_IDX_SHIFT  24          

#define MAXPATTERNS(wlc)									\
	(wlc_wowl_cap(wlc) ?									\
	(WLOFFLD_CAP(wlc) ? 12 :								\
	((D11REV_GE((wlc)->pub->corerev, 15) && D11REV_LT((wlc)->pub->corerev, 40)) ? 12 : 4))	\
	: 0)

#define WOWL_OFFLOAD_ENABLED(wlc) \
	((CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID) || WIN7_AND_UP_OS(wlc->pub))

#define WOWL_KEEPALIVE_FIXED_PARAM	11

#endif 
