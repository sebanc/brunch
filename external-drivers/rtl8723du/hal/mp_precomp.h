/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#ifndef __MP_PRECOMP_H__
#define __MP_PRECOMP_H__

#include <drv_types.h>
#include <hal_data.h>

#define BT_TMP_BUF_SIZE	100

#define rsprintf snprintf

#define DCMD_Printf			DBG_BT_INFO

#define delay_ms(ms)		rtw_mdelay_os(ms)

#ifdef bEnable
#undef bEnable
#endif

#define WPP_SOFTWARE_TRACE 0

enum {
	COMP_COEX		= 0,
	COMP_MAX
};

extern u32 GLBtcDbgType[];

#define DBG_OFF			0
#define DBG_SEC			1
#define DBG_SERIOUS		2
#define DBG_WARNING		3
#define DBG_LOUD		4
#define DBG_TRACE		5

#define BT_SUPPORT		1
#define COEX_SUPPORT		1
#define HS_SUPPORT		1

#include "halbtcoutsrc.h"

/* for wifi only mode */
#include "hal_btcoex_wifionly.h"

#include "halbtc8723d1ant.h"
#include "halbtc8723d2ant.h"

#endif /*  __MP_PRECOMP_H__ */
