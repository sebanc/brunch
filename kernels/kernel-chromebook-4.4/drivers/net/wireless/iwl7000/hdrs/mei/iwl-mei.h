// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2021 Intel Corporation
 */

#ifndef __iwl_mei_h__
#define __iwl_mei_h__

#include <linux/if_ether.h>

enum iwl_mei_nvm_caps {
	MEI_NVM_CAPS_LARI_SUPPORT	= BIT(0),
	MEI_NVM_CAPS_11AX_SUPPORT	= BIT(1),
};

struct iwl_mei_nvm {
	u8 hw_addr[ETH_ALEN];
	u8 n_hw_addrs;
	u8 reserved;
	u32 radio_cfg;
	u32 caps;
	u32 nvm_version;
	u32 channels[110];
};

struct iwl_mei_conn_info {
	u8 lp_state;
	u8 auth_mode;
	u8 ssid_len;
	u8 channel;
	u8 band;
	u8 ucast_cipher;
	u8 bssid[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
};

struct iwl_mei_colloc_info {
	u8 channel;
	u8 bssid[ETH_ALEN];
};

struct iwl_mei_ops {
	void (*me_conn_status)(void *priv,
			       const struct iwl_mei_conn_info *conn_info);
	void (*rfkill)(void *priv, bool blocked);
	void (*roaming_forbidden)(void *priv, bool forbidden);
	void (*sap_connected)(void *priv);
	void (*nic_stolen)(void *priv);
};

static inline bool iwl_mei_is_connected(void)
{ return false; }

static inline struct iwl_mei_nvm *iwl_mei_get_nvm(void)
{ return NULL; }

static inline int iwl_mei_get_ownership(void)
{ return 0; }

static inline void iwl_mei_set_rfkill_state(bool hw_rfkill, bool sw_rfkill)
{}

static inline void iwl_mei_set_nic_info(const u8 *mac_address, const u8 *nvm_address)
{}

static inline void iwl_mei_set_country_code(u16 mcc)
{}

static inline void iwl_mei_set_power_limit(const __le16 *power_limit)
{}

static inline int iwl_mei_register(void *priv,
				   const struct iwl_mei_ops *ops)
{ return 0; }

static inline void iwl_mei_start_unregister(void)
{}

static inline void iwl_mei_unregister_complete(void)
{}

static inline void iwl_mei_set_netdev(struct net_device *netdev)
{}

static inline void iwl_mei_tx_copy_to_csme(struct sk_buff *skb,
					   unsigned int ivlen)
{}

static inline void iwl_mei_host_associated(const struct iwl_mei_conn_info *conn_info,
					   const struct iwl_mei_colloc_info *colloc_info)
{}

static inline void iwl_mei_host_disassociated(u8 type)
{}

static inline void iwl_mei_device_down(void)
{}

#endif /* __iwl_mei_h__ */
