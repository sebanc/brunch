/*
 * utilities related header file
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
 * $Id: wlc_p2p.h 274724 2011-08-01 17:06:47Z $
 */

#ifndef _wlc_utils_h_
#define _wlc_utils_h_

#include <typedefs.h>

struct rsn_parms {
	uint8 flags;        
	uint8 multicast;    
	uint8 ucount;       
	uint8 unicast[4];   
	uint8 acount;       
	uint8 auth[4];      
	uint8 PAD[4];       
	uint8 cap[4];       
};

typedef struct rsn_parms rsn_parms_t;

extern void wlc_uint64_add(uint32* high, uint32* low, uint32 inc_high, uint32 inc_low);
extern void wlc_uint64_sub(uint32* a_high, uint32* a_low, uint32 b_high, uint32 b_low);
extern bool wlc_uint64_lt(uint32 a_high, uint32 a_low, uint32 b_high, uint32 b_low);
extern uint32 wlc_calc_tbtt_offset(uint32 bi, uint32 tsf_h, uint32 tsf_l);
extern uint32 wlc_calc_next_pos32(uint32 tsf, uint32 cur, uint32 interval, bool wrap);
extern void wlc_tsf64_to_next_tbtt64(uint32 bcn_int, uint32 *tsf_h, uint32 *tsf_l);
extern void wlc_tbtt21_to_tbtt32(uint32 tsf_l, uint32 *tbtt_l);
extern void wlc_tbtt21_to_tbtt64(uint32 tsf_h, uint32 tsf_l, uint32 *tbtt_h, uint32 *tbtt_l);

extern bool wlc_rsn_ucast_lookup(struct rsn_parms *rsn, uint8 auth);
extern bool wlc_rsn_akm_lookup(struct rsn_parms *rsn, uint8 akm);

#endif 
