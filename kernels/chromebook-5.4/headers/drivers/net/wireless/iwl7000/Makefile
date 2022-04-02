export CPTCFG_MAC80211=m
export CPTCFG_MAC80211_LEDS=y
export CPTCFG_IWLWIFI=m
export CPTCFG_IWLMVM=m
export CPTCFG_IWLWIFI_OPMODE_MODULAR=y
export CPTCFG_IWLWIFI_DEBUG=y
export CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES=y

ifeq ($(CONFIG_IWL7000_TESTMODE),y)
export CPTCFG_IWLWIFI_DEVICE_TESTMODE=y
endif

ifeq ($(CONFIG_IWL7000_VENDOR_CMDS),y)
export CPTCFG_IWLMVM_VENDOR_CMDS=y
endif

ifeq ($(CONFIG_IWL7000_DEBUGFS),y)
export CPTCFG_IWLWIFI_DEBUGFS=y
export CPTCFG_MAC80211_DEBUGFS=y
endif

ifeq ($(CONFIG_IWL7000_TRACING),y)
export CPTCFG_IWLWIFI_DEVICE_TRACING=y
export CPTCFG_MAC80211_MESSAGE_TRACING=y
endif

ifeq ($(CONFIG_IWL7000_LEDS),y)
export CPTCFG_IWLWIFI_LEDS=y
endif

ifeq ($(CONFIG_IWL7000_XVT),m)
export CPTCFG_IWLXVT=m
endif

subdir-ccflags-y := -I$(srctree)/$(src) -include "hdrs/iwl-chrome.h"
subdir-ccflags-y += -I$(srctree)/$(src)/hdrs/
subdir-ccflags-y += $(call cc-option,-Wno-error=date-time)
subdir-ccflags-y += $(call cc-option,-Wno-date-time)
# otherwise ieee80211_band vs. nl80211_band warnings come up
subdir-ccflags-y += $(call cc-option,-Wno-enum-compare)
subdir-ccflags-y += $(call cc-option,-Wno-enum-conversion)
subdir-ccflags-y += $(call cc-option,-Wno-int-in-bool-context)
# cfg80211 compatibility porting mechanism sometimes causes these
subdir-ccflags-y += $(call cc-option,-Wno-unused-const-variable)
subdir-ccflags-y += $(call cc-option,-Wno-unused-variable)

obj-$(CONFIG_IWL7000) += mac80211/
obj-$(CONFIG_IWL7000) += iwlwifi/
