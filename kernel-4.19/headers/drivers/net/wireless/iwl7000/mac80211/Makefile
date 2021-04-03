# SPDX-License-Identifier: GPL-2.0
obj-$(CPTCFG_MAC80211) += iwl7000_mac80211.o

# mac80211 objects
mac80211-y := \
	main.o status.o \
	driver-ops.o \
	sta_info.o \
	wep.o \
	aead_api.o \
	wpa.o \
	scan.o offchannel.o \
	ht.o agg-tx.o agg-rx.o \
	vht.o \
	he.o \
	s1g.o \
	ibss.o \
	iface.o \
	rate.o \
	michael.o \
	tkip.o \
	aes_cmac.o \
	aes_gmac.o \
	fils_aead.o \
	cfg.o \
	ethtool.o \
	rx.o \
	spectmgmt.o \
	tx.o \
	key.o \
	util.o \
	wme.o \
	chan.o \
	trace.o mlme.o \
	tdls.o \
	ocb.o \
	airtime.o

mac80211-$(CPTCFG_MAC80211_LEDS) += led.o
mac80211-$(CPTCFG_MAC80211_DEBUGFS) += \
	debugfs.o \
	debugfs_sta.o \
	debugfs_netdev.o \
	debugfs_key.o

mac80211-$(CPTCFG_MAC80211_MESH) += \
	mesh.o \
	mesh_pathtbl.o \
	mesh_plink.o \
	mesh_hwmp.o \
	mesh_sync.o \
	mesh_ps.o

mac80211-$(CONFIG_PM) += pm.o
mac80211-y += arc4.o
mac80211-y += bucket_locks.o
mac80211-y += rhashtable.o

CFLAGS_trace.o := -I$(srctree)/$(src)

CFLAGS_tx.o += -include "hdrs/iwl-tx.h"
CFLAGS_debugfs.o += -include "hdrs/net/fq.h"

rc80211_minstrel-y := \
	rc80211_minstrel.o \
	rc80211_minstrel_ht.o

rc80211_minstrel-$(CPTCFG_MAC80211_DEBUGFS) += \
	rc80211_minstrel_debugfs.o \
	rc80211_minstrel_ht_debugfs.o

mac80211-$(CPTCFG_MAC80211_RC_MINSTREL) += $(rc80211_minstrel-y)

ccflags-y += -DDEBUG

mac80211-y += backports.o cfg-utils.o reg.o

iwl7000_mac80211-objs := $(mac80211-objs) $(mac80211-y)
