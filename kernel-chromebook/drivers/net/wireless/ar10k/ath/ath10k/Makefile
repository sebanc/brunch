obj-$(CPTCFG_ATH10K) += ath10k_core.o
ath10k_core-y += mac.o \
		 debug.o \
		 core.o \
		 htc.o \
		 htt.o \
		 htt_rx.o \
		 htt_tx.o \
		 txrx.o \
		 wmi.o \
		 wmi-tlv.o \
		 bmi.o \
		 hw.o \
		 p2p.o \
		 swap.o

ath10k_core-$(CPTCFG_ATH10K_DEBUGFS) += spectral.o
ath10k_core-$(CPTCFG_NL80211_TESTMODE) += testmode.o
ath10k_core-$(CPTCFG_ATH10K_TRACING) += trace.o
ath10k_core-$(CPTCFG_THERMAL) += thermal.o
ath10k_core-$(CPTCFG_MAC80211_DEBUGFS) += debugfs_sta.o
ath10k_core-$(CPTCFG_PM) += wow.o
ath10k_core-$(CONFIG_DEV_COREDUMP) += coredump.o

obj-$(CPTCFG_ATH10K_PCI) += ath10k_pci.o
ath10k_pci-y += pci.o \
		ce.o

ath10k_pci-$(CPTCFG_ATH10K_AHB) += ahb.o

# for tracing framework to find trace.h
CFLAGS_trace.o := -I$(src)
