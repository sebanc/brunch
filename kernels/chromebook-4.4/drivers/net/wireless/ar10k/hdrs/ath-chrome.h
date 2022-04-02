#ifndef __ATH_CHROME
#define __ATH_CHROME
/* This file is pre-included from the Makefile (cc command line) */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/vmalloc.h>
#include <net/genetlink.h>

/* get the CPTCFG_* preprocessor symbols */
#include <hdrs/config.h>

#ifdef CONFIG_NL80211_TESTMODE
#define CPTCFG_NL80211_TESTMODE 1
#endif

#ifdef CONFIG_PM
#define CPTCFG_PM 1
#endif

/* mac80211 & backport */
#include <hdrs/mac80211-exp.h>
#include <hdrs/ieee80211.h>
#include <hdrs/mac80211-bp.h>
/* need to include mac80211 here, otherwise we get the regular kernel one */
#include <hdrs/mac80211.h>
/* ar10k_ath exports */
#include <hdrs/ath-exp.h>

#endif /* __ATH_CHROME */
