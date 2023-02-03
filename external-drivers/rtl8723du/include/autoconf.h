/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/*
 * Public General Configure
 */

#define RTL871X_MODULE_NAME "8723DU"
#define DRV_NAME "rtl8723du"

/*
 * Wi-Fi Functions Configure
 */

// #define CONFIG_CONCURRENT_MODE

#ifdef CONFIG_CONCURRENT_MODE
	#define CONFIG_TSF_RESET_OFFLOAD			/* For 2 PORT TSF SYNC. */
	#define CONFIG_RUNTIME_PORT_SWITCH
#endif /* CONFIG_CONCURRENT_MODE */

/*
 * Hareware/Firmware Related Configure
 */

#define DISABLE_BB_RF	0

#define RTW_NOTCH_FILTER 0 /* 0:Disable, 1:Enable, */

#define CONFIG_80211W

/*
 * Others
 */
#define CONFIG_EMBEDDED_FWIMG

#ifdef CONFIG_EMBEDDED_FWIMG
	#define	LOAD_FW_HEADER_FROM_DRIVER
#endif
/* #define CONFIG_FILE_FWIMG */

/*
 * Auto Configure Section
 */
#define MP_DRIVER	0

/*
 * Debug Related Configure
 */
#define CONFIG_DEBUG /* DBG_871X, etc... */
#ifdef CONFIG_DEBUG
	#define DBG	1	/* for ODM & BTCOEX debug */
	#define DBG_PHYDM_MORE 0
#else /* !CONFIG_DEBUG */
	#define DBG	0	/* for ODM & BTCOEX debug */
	#define DBG_PHYDM_MORE 0
#endif /* CONFIG_DEBUG */

#define CONFIG_PROC_DEBUG
