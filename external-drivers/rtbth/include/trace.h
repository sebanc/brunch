/*
*************************************************************************
* Ralink Technology Corporation
* 5F., No. 5, Taiyuan 1st St., Jhubei City,
* Hsinchu County 302,
* Taiwan, R.O.C.
*
* (c) Copyright 2012, Ralink Technology Corporation
*
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the                         *
* Free Software Foundation, Inc.,                                       *
* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
*                                                                       *
*************************************************************************/

#ifndef __TRACE_H
#define __TRACE_H


//
// Define debug levels
//
#define	NONE       0 //  Tracing is not on
#define	FATAL      1 // Abnormal exit or termination
#define	ERROR      2 // Severe errors that need logging
#define	WARNING    3 // Warnings such as allocation failure
#define	INFO       4 // Includes non-error cases such as Entry-Exit
#define	TRACE      5 // Detailed traces from intermediate steps
#define	LOUD       6 // Detailed trace from every step
#define	NOISY	   7 // Raw data


//
// TODO: These defines are missing in evntrace.h
// in some DDK build environments (XP).
//
#if !defined(TRACE_LEVEL_NONE)
#define TRACE_LEVEL_NONE        0   // Tracing is not on
#define TRACE_LEVEL_CRITICAL    1   // Abnormal exit or termination
#define TRACE_LEVEL_FATAL       1   // Deprecated name for Abnormal exit or termination
#define TRACE_LEVEL_ERROR       2   // Severe errors that need logging
#define TRACE_LEVEL_WARNING     3   // Warnings such as allocation failure
#define TRACE_LEVEL_INFORMATION 4   // Includes non-error cases(e.g.,Entry-Exit)
#define TRACE_LEVEL_VERBOSE     5   // Detailed traces from intermediate steps
#define TRACE_LEVEL_RESERVED6   6       // statistics of packet and queue
#define TRACE_LEVEL_RESERVED7   7
#define TRACE_LEVEL_RESERVED8   8
#define TRACE_LEVEL_RESERVED9   9
#endif


//
// Define Debug Flags
//
#define DBG_INIT		0x00000001
#define DBG_PNP			0x00000002
#define DBG_POWER		0x00000004
#define DBG_HW_ACCESS	0x00000008
#define DBG_IOCTLS		0x00000010
#define DBG_MISC		0x00000020
#define DBG_QUEUE		0x00000040
#define DBG_LM			0x00000080
#define DBG_LMP			0x00000100
#define DBG_LC_COMMAND	0x00000200
#define DBG_LC_EVENT	0x00000400
#define DBG_PDMA_DATA	0x00000800
#define DBG_SEC			0x00001000
#define DBG_LINK		0x00002000
#define DBG_THREAD		0x00004000
#define DBG_HCI_CMD		0x00008000
#define DBG_HCI_EVENT	0x00010000
#define DBG_HCI_DATA	0x00020000
#define DBG_L2CAPS		0x00040000
#define DBG_L2CAPD		0x00080000
#define DBG_BTCX		0x00100000
#define DBG_AFH			0x00200000
#define DBG_BW			0x00400000
#define DBG_LL			0x00800000
#define DBG_TMR			0x01000000
#define DBG_SLOT_OFT    0x02000000
#define DBG_FATAL_ERR   0x04000000

#endif // __TRACE_H //
