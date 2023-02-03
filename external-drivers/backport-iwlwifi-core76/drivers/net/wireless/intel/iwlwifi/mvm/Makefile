# SPDX-License-Identifier: GPL-2.0
obj-$(CPTCFG_IWLMVM)   += iwlmvm.o
iwlmvm-y += fw.o mac80211.o nvm.o ops.o phy-ctxt.o mac-ctxt.o
iwlmvm-y += utils.o rx.o rxmq.o tx.o binding.o quota.o sta.o sf.o
iwlmvm-y += scan.o time-event.o rs.o rs-fw.o
iwlmvm-y += power.o coex.o
iwlmvm-y += tt.o offloading.o tdls.o
iwlmvm-y += ftm-responder.o ftm-initiator.o
iwlmvm-y += rfi.o
iwlmvm-y += nan.o
iwlmvm-y += time-sync.o
iwlmvm-y += mld-key.o mld-mac.o link.o mld-sta.o mld-mac80211.o
iwlmvm-$(CPTCFG_IWLWIFI_DEBUGFS) += debugfs.o debugfs-vif.o
iwlmvm-$(CPTCFG_IWLWIFI_LEDS) += led.o
iwlmvm-$(CONFIG_PM) += d3.o
iwlmvm-$(CPTCFG_IWLMEI) += vendor-cmd.o

ccflags-y += -I$(src)/../

# non-upstream things
iwlmvm-$(CPTCFG_IWLMVM_VENDOR_CMDS) += vendor-cmd.o
ifeq ($(CPTCFG_IWLMVM_VENDOR_CMDS),y)
endif

ifeq ($(CPTCFG_IWLWIFI_SIMULATION),y)
UBSAN_SANITIZE := y
endif
