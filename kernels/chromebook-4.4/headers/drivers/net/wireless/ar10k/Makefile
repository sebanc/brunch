
export CPTCFG_ATH_COMMON=m

export CPTCFG_ATH10K=m
export CPTCFG_ATH10K_PCI=m
export CPTCFG_ATH10K_TRACING=y
export CPTCFG_ATH10K_DEBUGFS=y

export CPTCFG_MAC80211=m
export CPTCFG_MAC80211_LEDS=y
export CPTCFG_MAC80211_DEBUGFS=y
export CPTCFG_MAC80211_RC_MINSTREL=y
export CPTCFG_MAC80211_RC_MINSTREL_HT=y

export CPTCFG_NL80211_TESTMODE=$(CONFIG_NL80211_TESTMODE)
export CPTCFG_PM=$(CONFIG_PM)

subdir-ccflags-y += -I$(src) -include "hdrs/ath-chrome.h"
subdir-ccflags-y += -I$(src)/hdrs/

obj-$(CONFIG_AR10K) += ath/
obj-$(CONFIG_AR10K) += mac80211/
