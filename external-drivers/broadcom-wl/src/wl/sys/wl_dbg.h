/*
 * Minimal debug/trace/assert driver definitions for
 * Broadcom 802.11 Networking Adapter.
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
 * $Id: wl_dbg.h 405851 2013-06-05 00:56:21Z $
 */

#ifndef _wl_dbg_h_
#define _wl_dbg_h_

extern uint32 wl_msg_level;
extern uint32 wl_msg_level2;

#if defined(BCMDBG) && !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
extern char* wlc_dbg_get_hw_timestamp(void);

#define WL_TIMESTAMP() 		do { if (wl_msg_level2 & WL_TIMESTAMP_VAL) {\
	                            printf(wlc_dbg_get_hw_timestamp()); }\
	                        } while (0)
#else
#define WL_TIMESTAMP()
#endif 

#if 0 && (VERSION_MAJOR > 9)
extern int osl_printf(const char *fmt, ...);
#include <IOKit/apple80211/IO8Log.h>
#define WL_PRINT(args)		do { osl_printf args; } while (0)
#define RELEASE_PRINT(args)	do { WL_PRINT(args); IO8Log args; } while (0)
#else
#define WL_PRINT(args)		do { WL_TIMESTAMP(); printf args; } while (0)
#endif

#ifdef BCMDBG

#define	WL_NONE(args)		do {if (wl_msg_level & 0) WL_PRINT(args);} while (0)

#define	WL_ERROR(args)		do {if (wl_msg_level & WL_ERROR_VAL) WL_PRINT(args);} while (0)
#define	WL_TRACE(args)		do {if (wl_msg_level & WL_TRACE_VAL) WL_PRINT(args);} while (0)

#else	

#define WL_NONE(args)

#ifdef BCMDBG_ERR
#define	WL_ERROR(args)		WL_PRINT(args)
#else
#define	WL_ERROR(args)
#endif 
#define	WL_TRACE(args)
#define WL_APSTA_UPDN(args)
#define WL_APSTA_RX(args)
#define WL_WSEC(args)
#define WL_WSEC_DUMP(args)
#define WL_PCIE(args)		do {if (wl_msg_level2 & WL_PCIE_VAL) WL_PRINT(args);} while (0)
#define WL_PCIE_ON()		(wl_msg_level2 & WL_PCIE_VAL)
#endif 

extern uint32 wl_msg_level;
extern uint32 wl_msg_level2;
#endif 
