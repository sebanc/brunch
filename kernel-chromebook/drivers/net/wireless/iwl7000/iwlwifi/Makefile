# SPDX-License-Identifier: GPL-2.0
# common
obj-$(CPTCFG_IWLWIFI)	+= iwlwifi.o
iwlwifi-objs		+= iwl-io.o
iwlwifi-objs		+= iwl-drv.o
iwlwifi-objs		+= iwl-debug.o
iwlwifi-objs		+= iwl-eeprom-read.o iwl-eeprom-parse.o
iwlwifi-objs		+= iwl-phy-db.o iwl-nvm-parse.o
iwlwifi-objs		+= iwl-dbg-tlv.o
iwlwifi-objs		+= iwl-trans.o
iwlwifi-objs		+= queue/tx.o

iwlwifi-objs		+= fw/img.o fw/notif-wait.o
iwlwifi-objs		+= fw/dbg.o fw/pnvm.o
iwlwifi-$(CPTCFG_IWLMVM)	+= fw/paging.o fw/smem.o fw/init.o
iwlwifi-$(CPTCFG_IWLXVT)	+= fw/init.o
iwlwifi-$(CPTCFG_IWLWIFI_DEBUGFS) += fw/debugfs.o

iwlwifi-$(CONFIG_ACPI)	+= fw/acpi.o

# Bus
iwlwifi-$(CONFIG_PCI) += pcie/drv.o pcie/rx.o pcie/tx.o pcie/trans.o
iwlwifi-$(CONFIG_PCI) += pcie/ctxt-info.o pcie/ctxt-info-gen3.o pcie/trans-gen2.o pcie/tx-gen2.o

iwlwifi-$(CPTCFG_IWLMVM) += cfg/7000.o cfg/8000.o
iwlwifi-$(CPTCFG_IWLMVM) += cfg/9000.o cfg/22000.o

iwlwifi-$(CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES) += iwl-dbg-cfg.o


iwlwifi-objs += $(iwlwifi-m)
iwlwifi-objs += $(iwlwifi-y)

iwlwifi-$(CPTCFG_IWLWIFI_DEVICE_TRACING) += iwl-devtrace.o
iwlwifi-$(CPTCFG_IWLWIFI_DEVICE_TESTMODE) += iwl-tm-gnl.o iwl-dnt-cfg.o iwl-dnt-dispatch.o
iwlwifi-$(CPTCFG_IWLWIFI_DEVICE_TESTMODE) += iwl-dnt-dev-if.o fw/testmode.o

ccflags-y += -I$(srctree)/$(src)

obj-$(CPTCFG_IWLMVM)	+= mvm/
obj-$(CPTCFG_IWLXVT)	+= xvt/

CFLAGS_iwl-devtrace.o := -I$(srctree)/$(src)
