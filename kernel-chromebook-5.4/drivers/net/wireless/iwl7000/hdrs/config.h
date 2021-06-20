/******************************************************************************
 *
 * Copyright(c) 2018-2021 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/
#ifndef __IWL_CHROME_CONFIG
#define __IWL_CHROME_CONFIG
/* This must match the CPTCFG_* symbols defined in the Makefile */
#define CPTCFG_MAC80211_MODULE 1
#define CPTCFG_MAC80211_LEDS 1
#define CPTCFG_MAC80211_RC_DEFAULT ""
#define CPTCFG_MAC80211_STA_HASH_MAX_SIZE 0
#define CPTCFG_IWL_TIMEOUT_FACTOR 1
#define CPTCFG_IWLWIFI_MODULE 1
#define CPTCFG_IWLMVM_MODULE 1
#define CPTCFG_IWLWIFI_OPMODE_MODULAR 1
#define CPTCFG_IWLWIFI_DEBUG 1
#define CPTCFG_IWLWIFI_NUM_CHANNELS 1
#define CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES 1
#define CPTCFG_IWLWIFI_DISALLOW_OLDER_FW 1
#define CPTCFG_IWLWIFI_NUM_STA_INTERFACES 1
#define CPTCFG_REJECT_NONUPSTREAM_NL80211 1
#define CPTCFG_IWLWIFI_ATLAS_PLATFORM_WORKAROUND 1

#ifdef CONFIG_IWL7000_DEBUGFS
#define CPTCFG_MAC80211_DEBUGFS 1
#define CPTCFG_IWLWIFI_DEBUGFS 1
#endif

#ifdef CONFIG_IWL7000_LEDS
#define CPTCFG_IWLWIFI_LEDS 1
#endif

#ifdef CONFIG_IWL7000_TRACING
#define CPTCFG_MAC80211_MESSAGE_TRACING 1
#define CPTCFG_IWLWIFI_DEVICE_TRACING 1
#endif

#ifdef CONFIG_IWL7000_TESTMODE
#define CPTCFG_IWLWIFI_DEVICE_TESTMODE 1
#define CPTCFG_NL80211_TESTMODE 1
#endif

#ifdef CONFIG_IWL7000_XVT_MODULE
#define CPTCFG_IWLXVT_MODULE 1
#endif

/* cfg80211 version specific backward compat code follows */
#ifdef CONFIG_WIRELESS_38
#define CFG80211_VERSION KERNEL_VERSION(3,8,0)
#else
#define CFG80211_VERSION LINUX_VERSION_CODE
#endif

#if CFG80211_VERSION >= KERNEL_VERSION(5, 11, 0)
#define CPTCFG_IWLWIFI_WIFI_6_SUPPORT 1
#endif /* >= 5.10.0 */

#if defined(CONFIG_IWL7000_VENDOR_CMDS) && \
	(CFG80211_VERSION >= KERNEL_VERSION(3, 14, 0))
#define CPTCFG_IWLMVM_VENDOR_CMDS 1
#endif

#endif
