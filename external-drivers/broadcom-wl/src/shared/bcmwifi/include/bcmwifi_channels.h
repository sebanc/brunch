/*
 * Misc utility routines for WL and Apps
 * This header file housing the define and function prototype use by
 * both the wl driver, tools & Apps.
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
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: bcmwifi_channels.h 309193 2012-01-19 00:03:57Z $
 */

#ifndef	_bcmwifi_channels_h_
#define	_bcmwifi_channels_h_

#if defined(__FreeBSD__)
#include <stdbool.h>
#endif

typedef uint16 chanspec_t;

#define CH_UPPER_SB			0x01
#define CH_LOWER_SB			0x02
#define CH_EWA_VALID			0x04
#define CH_80MHZ_APART			16
#define CH_40MHZ_APART			8
#define CH_20MHZ_APART			4
#define CH_10MHZ_APART			2
#define CH_5MHZ_APART			1	
#define CH_MAX_2G_CHANNEL		14	
#define MAXCHANNEL		224	
#define MAXCHANNEL_NUM	(MAXCHANNEL - 1)	

#define CH_NUM_VALID_RANGE(ch_num) ((ch_num) > 0 && (ch_num) <= MAXCHANNEL_NUM)

#define CHSPEC_CTLOVLP(sp1, sp2, sep)	(ABS(wf_chspec_ctlchan(sp1) - wf_chspec_ctlchan(sp2)) < \
				  (sep))

#undef  D11AC_IOTYPES
#define D11AC_IOTYPES

#define WL_CHANSPEC_CHAN_MASK		0x00ff
#define WL_CHANSPEC_CHAN_SHIFT		0
#define WL_CHANSPEC_CHAN1_MASK		0x000f
#define WL_CHANSPEC_CHAN1_SHIFT		0
#define WL_CHANSPEC_CHAN2_MASK		0x00f0
#define WL_CHANSPEC_CHAN2_SHIFT		4

#define WL_CHANSPEC_CTL_SB_MASK		0x0700
#define WL_CHANSPEC_CTL_SB_SHIFT	8
#define WL_CHANSPEC_CTL_SB_LLL		0x0000
#define WL_CHANSPEC_CTL_SB_LLU		0x0100
#define WL_CHANSPEC_CTL_SB_LUL		0x0200
#define WL_CHANSPEC_CTL_SB_LUU		0x0300
#define WL_CHANSPEC_CTL_SB_ULL		0x0400
#define WL_CHANSPEC_CTL_SB_ULU		0x0500
#define WL_CHANSPEC_CTL_SB_UUL		0x0600
#define WL_CHANSPEC_CTL_SB_UUU		0x0700
#define WL_CHANSPEC_CTL_SB_LL		WL_CHANSPEC_CTL_SB_LLL
#define WL_CHANSPEC_CTL_SB_LU		WL_CHANSPEC_CTL_SB_LLU
#define WL_CHANSPEC_CTL_SB_UL		WL_CHANSPEC_CTL_SB_LUL
#define WL_CHANSPEC_CTL_SB_UU		WL_CHANSPEC_CTL_SB_LUU
#define WL_CHANSPEC_CTL_SB_L		WL_CHANSPEC_CTL_SB_LLL
#define WL_CHANSPEC_CTL_SB_U		WL_CHANSPEC_CTL_SB_LLU
#define WL_CHANSPEC_CTL_SB_LOWER	WL_CHANSPEC_CTL_SB_LLL
#define WL_CHANSPEC_CTL_SB_UPPER	WL_CHANSPEC_CTL_SB_LLU
#define WL_CHANSPEC_CTL_SB_NONE		WL_CHANSPEC_CTL_SB_LLL

#define WL_CHANSPEC_BW_MASK		0x3800
#define WL_CHANSPEC_BW_SHIFT		11
#define WL_CHANSPEC_BW_5		0x0000
#define WL_CHANSPEC_BW_10		0x0800
#define WL_CHANSPEC_BW_20		0x1000
#define WL_CHANSPEC_BW_40		0x1800
#define WL_CHANSPEC_BW_80		0x2000
#define WL_CHANSPEC_BW_160		0x2800
#define WL_CHANSPEC_BW_8080		0x3000
#define WL_CHANSPEC_BW_2P5		0x3800

#define WL_CHANSPEC_BAND_MASK		0xc000
#define WL_CHANSPEC_BAND_SHIFT		14
#define WL_CHANSPEC_BAND_2G		0x0000
#define WL_CHANSPEC_BAND_3G		0x4000
#define WL_CHANSPEC_BAND_4G		0x8000
#define WL_CHANSPEC_BAND_5G		0xc000
#define INVCHANSPEC			255
#define MAX_CHANSPEC				0xFFFF

#define LOWER_20_SB(channel)		(((channel) > CH_10MHZ_APART) ? \
					((channel) - CH_10MHZ_APART) : 0)
#define UPPER_20_SB(channel)		(((channel) < (MAXCHANNEL - CH_10MHZ_APART)) ? \
					((channel) + CH_10MHZ_APART) : 0)

#define LL_20_SB(channel) (((channel) > 3 * CH_10MHZ_APART) ? ((channel) - 3 * CH_10MHZ_APART) : 0)
#define UU_20_SB(channel) 	(((channel) < (MAXCHANNEL - 3 * CH_10MHZ_APART)) ? \
				((channel) + 3 * CH_10MHZ_APART) : 0)
#define LU_20_SB(channel) LOWER_20_SB(channel)
#define UL_20_SB(channel) UPPER_20_SB(channel)

#define LOWER_40_SB(channel)		((channel) - CH_20MHZ_APART)
#define UPPER_40_SB(channel)		((channel) + CH_20MHZ_APART)
#define CHSPEC_WLCBANDUNIT(chspec)	(CHSPEC_IS5G(chspec) ? BAND_5G_INDEX : BAND_2G_INDEX)
#define CH20MHZ_CHSPEC(channel)		(chanspec_t)((chanspec_t)(channel) | WL_CHANSPEC_BW_20 | \
					(((channel) <= CH_MAX_2G_CHANNEL) ? \
					WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G))
#define CH2P5MHZ_CHSPEC(channel)	(chanspec_t)((chanspec_t)(channel) | WL_CHANSPEC_BW_2P5 | \
						(((channel) <= CH_MAX_2G_CHANNEL) ? \
						WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G))
#define CH5MHZ_CHSPEC(channel)		(chanspec_t)((chanspec_t)(channel) | WL_CHANSPEC_BW_5 | \
						(((channel) <= CH_MAX_2G_CHANNEL) ? \
						WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G))
#define CH10MHZ_CHSPEC(channel)		(chanspec_t)((chanspec_t)(channel) | WL_CHANSPEC_BW_10 | \
						(((channel) <= CH_MAX_2G_CHANNEL) ? \
						WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G))
#define NEXT_20MHZ_CHAN(channel)	(((channel) < (MAXCHANNEL - CH_20MHZ_APART)) ? \
					((channel) + CH_20MHZ_APART) : 0)
#define CH40MHZ_CHSPEC(channel, ctlsb)	(chanspec_t) \
					((channel) | (ctlsb) | WL_CHANSPEC_BW_40 | \
					((channel) <= CH_MAX_2G_CHANNEL ? WL_CHANSPEC_BAND_2G : \
					WL_CHANSPEC_BAND_5G))
#define CH80MHZ_CHSPEC(channel, ctlsb)	(chanspec_t) \
					((channel) | (ctlsb) | \
					 WL_CHANSPEC_BW_80 | WL_CHANSPEC_BAND_5G)
#define CH160MHZ_CHSPEC(channel, ctlsb)	(chanspec_t) \
					((channel) | (ctlsb) | \
					 WL_CHANSPEC_BW_160 | WL_CHANSPEC_BAND_5G)
#define CHBW_CHSPEC(bw, channel)	(chanspec_t)((chanspec_t)(channel) | (bw) | \
							(((channel) <= CH_MAX_2G_CHANNEL) ? \
							WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G))

#ifdef WL11AC_80P80
#define CHSPEC_CHANNEL(chspec)	wf_chspec_channel(chspec)
#else
#define CHSPEC_CHANNEL(chspec)	((uint8)((chspec) & WL_CHANSPEC_CHAN_MASK))
#endif
#define CHSPEC_CHAN1(chspec)	((chspec) & WL_CHANSPEC_CHAN1_MASK) >> WL_CHANSPEC_CHAN1_SHIFT
#define CHSPEC_CHAN2(chspec)	((chspec) & WL_CHANSPEC_CHAN2_MASK) >> WL_CHANSPEC_CHAN2_SHIFT
#define CHSPEC_BAND(chspec)		((chspec) & WL_CHANSPEC_BAND_MASK)
#define CHSPEC_CTL_SB(chspec)	((chspec) & WL_CHANSPEC_CTL_SB_MASK)
#define CHSPEC_BW(chspec)		((chspec) & WL_CHANSPEC_BW_MASK)

#define CHSPEC_IS2P5(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_2P5)
#define CHSPEC_IS5(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_5)
#define CHSPEC_IS10(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_10)
#define CHSPEC_IS20(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_20)
#ifndef CHSPEC_IS40
#define CHSPEC_IS40(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40)
#endif
#ifndef CHSPEC_IS80
#define CHSPEC_IS80(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_80)
#endif
#ifndef CHSPEC_IS160
#define CHSPEC_IS160(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_160)
#endif
#ifndef CHSPEC_IS8080
#define CHSPEC_IS8080(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_8080)
#endif

#ifdef WL11ULB

#define BW_LE20(bw)		(((bw) == WL_CHANSPEC_BW_2P5) || \
				((bw) == WL_CHANSPEC_BW_5) || \
				((bw) == WL_CHANSPEC_BW_10) || \
				((bw) == WL_CHANSPEC_BW_20))
#define CHSPEC_ISLE20(chspec)	(CHSPEC_IS2P5(chspec) || CHSPEC_IS5(chspec) || \
				CHSPEC_IS10(chspec) || CHSPEC_IS20(chspec))
#else 
#define BW_LE20(bw)		((bw) == WL_CHANSPEC_BW_20)
#define CHSPEC_ISLE20(chspec)	(CHSPEC_IS20(chspec))
#endif 

#define BW_LE40(bw)		(BW_LE20(bw) || ((bw) == WL_CHANSPEC_BW_40))
#define BW_LE80(bw)		(BW_LE40(bw) || ((bw) == WL_CHANSPEC_BW_80))
#define BW_LE160(bw)		(BW_LE80(bw) || ((bw) == WL_CHANSPEC_BW_160))
#define CHSPEC_BW_LE20(chspec)	(BW_LE20(CHSPEC_BW(chspec)))
#define CHSPEC_IS5G(chspec)	(((chspec) & WL_CHANSPEC_BAND_MASK) == WL_CHANSPEC_BAND_5G)
#define CHSPEC_IS2G(chspec)	(((chspec) & WL_CHANSPEC_BAND_MASK) == WL_CHANSPEC_BAND_2G)
#define CHSPEC_SB_UPPER(chspec)	\
	((((chspec) & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_UPPER) && \
	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40))
#define CHSPEC_SB_LOWER(chspec)	\
	((((chspec) & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_LOWER) && \
	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40))
#define CHSPEC2WLC_BAND(chspec) (CHSPEC_IS5G(chspec) ? WLC_BAND_5G : WLC_BAND_2G)

#define CHANSPEC_STR_LEN    20

#define CHSPEC_IS_BW_160_WIDE(chspec) (CHSPEC_BW(chspec) == WL_CHANSPEC_BW_160 ||\
	CHSPEC_BW(chspec) == WL_CHANSPEC_BW_8080)

#ifdef WL11ULB
#define CHSPEC_BW_GE(chspec, bw) \
	(((CHSPEC_IS_BW_160_WIDE(chspec) &&\
	((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) ||\
	(CHSPEC_BW(chspec) >= (bw))) && \
	(!(CHSPEC_BW(chspec) == WL_CHANSPEC_BW_2P5 && (bw) != WL_CHANSPEC_BW_2P5)))
#else 
#define CHSPEC_BW_GE(chspec, bw) \
		((CHSPEC_IS_BW_160_WIDE(chspec) &&\
		((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) ||\
		(CHSPEC_BW(chspec) >= (bw)))
#endif 

#ifdef WL11ULB
#define CHSPEC_BW_LE(chspec, bw) \
	(((CHSPEC_IS_BW_160_WIDE(chspec) &&\
	((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) ||\
	(CHSPEC_BW(chspec) <= (bw))) || \
	(CHSPEC_BW(chspec) == WL_CHANSPEC_BW_2P5))
#else 
#define CHSPEC_BW_LE(chspec, bw) \
		((CHSPEC_IS_BW_160_WIDE(chspec) &&\
		((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) ||\
		(CHSPEC_BW(chspec) <= (bw)))
#endif 

#ifdef WL11ULB
#define CHSPEC_BW_GT(chspec, bw) \
	((!(CHSPEC_IS_BW_160_WIDE(chspec) &&\
	((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) &&\
	(CHSPEC_BW(chspec) > (bw))) && \
	(CHSPEC_BW(chspec) != WL_CHANSPEC_BW_2P5))
#else 
#define CHSPEC_BW_GT(chspec, bw) \
		(!(CHSPEC_IS_BW_160_WIDE(chspec) &&\
		((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) &&\
		(CHSPEC_BW(chspec) > (bw)))
#endif 

#ifdef WL11ULB
#define CHSPEC_BW_LT(chspec, bw) \
	((!(CHSPEC_IS_BW_160_WIDE(chspec) &&\
	((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) &&\
	(CHSPEC_BW(chspec) < (bw))) || \
	((CHSPEC_BW(chspec) == WL_CHANSPEC_BW_2P5 && (bw) != WL_CHANSPEC_BW_2P5)))
#else 
#define CHSPEC_BW_LT(chspec, bw) \
		(!(CHSPEC_IS_BW_160_WIDE(chspec) &&\
		((bw) == WL_CHANSPEC_BW_160 || (bw) == WL_CHANSPEC_BW_8080)) &&\
		(CHSPEC_BW(chspec) < (bw)))
#endif 

#define WL_LCHANSPEC_CHAN_MASK		0x00ff
#define WL_LCHANSPEC_CHAN_SHIFT		     0

#define WL_LCHANSPEC_CTL_SB_MASK	0x0300
#define WL_LCHANSPEC_CTL_SB_SHIFT	     8
#define WL_LCHANSPEC_CTL_SB_LOWER	0x0100
#define WL_LCHANSPEC_CTL_SB_UPPER	0x0200
#define WL_LCHANSPEC_CTL_SB_NONE	0x0300

#define WL_LCHANSPEC_BW_MASK		0x0C00
#define WL_LCHANSPEC_BW_SHIFT		    10
#define WL_LCHANSPEC_BW_10		0x0400
#define WL_LCHANSPEC_BW_20		0x0800
#define WL_LCHANSPEC_BW_40		0x0C00

#define WL_LCHANSPEC_BAND_MASK		0xf000
#define WL_LCHANSPEC_BAND_SHIFT		    12
#define WL_LCHANSPEC_BAND_5G		0x1000
#define WL_LCHANSPEC_BAND_2G		0x2000

#define LCHSPEC_CHANNEL(chspec)	((uint8)((chspec) & WL_LCHANSPEC_CHAN_MASK))
#define LCHSPEC_BAND(chspec)	((chspec) & WL_LCHANSPEC_BAND_MASK)
#define LCHSPEC_CTL_SB(chspec)	((chspec) & WL_LCHANSPEC_CTL_SB_MASK)
#define LCHSPEC_BW(chspec)	((chspec) & WL_LCHANSPEC_BW_MASK)
#define LCHSPEC_IS10(chspec)	(((chspec) & WL_LCHANSPEC_BW_MASK) == WL_LCHANSPEC_BW_10)
#define LCHSPEC_IS20(chspec)	(((chspec) & WL_LCHANSPEC_BW_MASK) == WL_LCHANSPEC_BW_20)
#define LCHSPEC_IS40(chspec)	(((chspec) & WL_LCHANSPEC_BW_MASK) == WL_LCHANSPEC_BW_40)
#define LCHSPEC_IS5G(chspec)	(((chspec) & WL_LCHANSPEC_BAND_MASK) == WL_LCHANSPEC_BAND_5G)
#define LCHSPEC_IS2G(chspec)	(((chspec) & WL_LCHANSPEC_BAND_MASK) == WL_LCHANSPEC_BAND_2G)

#define LCHSPEC_SB_UPPER(chspec)	\
	((((chspec) & WL_LCHANSPEC_CTL_SB_MASK) == WL_LCHANSPEC_CTL_SB_UPPER) && \
	(((chspec) & WL_LCHANSPEC_BW_MASK) == WL_LCHANSPEC_BW_40))
#define LCHSPEC_SB_LOWER(chspec)	\
	((((chspec) & WL_LCHANSPEC_CTL_SB_MASK) == WL_LCHANSPEC_CTL_SB_LOWER) && \
	(((chspec) & WL_LCHANSPEC_BW_MASK) == WL_LCHANSPEC_BW_40))

#define LCHSPEC_CREATE(chan, band, bw, sb)  ((uint16)((chan) | (sb) | (bw) | (band)))

#define CH20MHZ_LCHSPEC(channel) \
	(chanspec_t)((chanspec_t)(channel) | WL_LCHANSPEC_BW_20 | \
	WL_LCHANSPEC_CTL_SB_NONE | (((channel) <= CH_MAX_2G_CHANNEL) ? \
	WL_LCHANSPEC_BAND_2G : WL_LCHANSPEC_BAND_5G))

#define WF_CHAN_FACTOR_2_4_G		4814	

#define WF_CHAN_FACTOR_5_G		10000	

#define WF_CHAN_FACTOR_4_G		8000	

#define WLC_2G_25MHZ_OFFSET		5	

#define WF_NUM_SIDEBANDS_40MHZ   2
#define WF_NUM_SIDEBANDS_80MHZ   4
#define WF_NUM_SIDEBANDS_8080MHZ 4
#define WF_NUM_SIDEBANDS_160MHZ  8

extern char * wf_chspec_ntoa_ex(chanspec_t chspec, char *buf);

extern char * wf_chspec_ntoa(chanspec_t chspec, char *buf);

extern chanspec_t wf_chspec_aton(const char *a);

extern bool wf_chspec_malformed(chanspec_t chanspec);

extern bool wf_chspec_valid(chanspec_t chanspec);

extern uint8 wf_chspec_ctlchan(chanspec_t chspec);

extern chanspec_t wf_chspec_ctlchspec(chanspec_t chspec);

extern chanspec_t wf_chspec_primary40_chspec(chanspec_t chspec);

extern int wf_mhz2channel(uint freq, uint start_factor);

extern int wf_channel2mhz(uint channel, uint start_factor);

extern chanspec_t wf_chspec_80(uint8 center_channel, uint8 primary_channel);

extern uint16 wf_channel2chspec(uint ctl_ch, uint bw);

extern uint wf_channel2freq(uint channel);
extern uint wf_freq2channel(uint freq);

extern chanspec_t wf_chspec_get8080_chspec(uint8 primary_20mhz,
	uint8 chan0_80Mhz, uint8 chan1_80Mhz);

extern uint8 wf_chspec_primary80_channel(chanspec_t chanspec);

extern uint8 wf_chspec_secondary80_channel(chanspec_t chanspec);

extern chanspec_t wf_chspec_primary80_chspec(chanspec_t chspec);

#ifdef WL11AC_80P80

extern uint8 wf_chspec_channel(chanspec_t chspec);
#endif
#endif	
