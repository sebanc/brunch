# SPDX-License-Identifier: GPL-2.0-only
obj-$(CPTCFG_IWLMEI)	+= iwlmei.o
iwlmei-y += main.o
iwlmei-y += net.o
iwlmei-$(CPTCFG_IWLWIFI_DEVICE_TRACING) += trace.o
CFLAGS_trace.o := -I$(src)

ccflags-y += -I $(src)/../

ifeq ($(CPTCFG_IWLWIFI_SIMULATION),y)
UBSAN_SANITIZE := y
endif
