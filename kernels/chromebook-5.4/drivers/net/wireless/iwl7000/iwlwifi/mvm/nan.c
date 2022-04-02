// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2015-2017 Intel Deutschland GmbH
 * Copyright (C) 2018, 2020 Intel Corporation
 */
#include <net/cfg80211.h>
#include <linux/etherdevice.h>

#include "mvm.h"
#include "fw/api/nan.h"

#define NAN_WARMUP_TIMEOUT_USEC  (120000000ULL)
#define NAN_CHANNEL_24           (6)
#define NAN_CHANNEL_52           (149)

enum srf_type {
	SRF_BF_TYPE             = BIT(0),
	SRF_INCLUDE             = BIT(1),
	SRF_BLOOM_FILTER_IDX    = BIT(2) | BIT(3),
};

static bool iwl_mvm_can_beacon(struct ieee80211_vif *vif,
			       enum nl80211_band band, u8 channel)
{
	struct wiphy *wiphy = ieee80211_vif_to_wdev(vif)->wiphy;
	int freq = ieee80211_channel_to_frequency(channel, band);
	struct ieee80211_channel *chan = ieee80211_get_channel(wiphy,
							       freq);
	struct cfg80211_chan_def def;

	if (!chan)
		return false;

	cfg80211_chandef_create(&def, chan, NL80211_CHAN_NO_HT);
	return cfg80211_reg_can_beacon(wiphy, &def, vif->type);
}

static inline bool iwl_mvm_nan_is_ver2(struct ieee80211_hw *hw)
{
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);

	return fw_has_api(&mvm->fw->ucode_capa, IWL_UCODE_TLV_API_NAN2_VER2);
}

static inline size_t iwl_mvm_nan_cfg_cmd_len(struct ieee80211_hw *hw)
{
	return iwl_mvm_nan_is_ver2(hw) ? sizeof(struct iwl_nan_cfg_cmd_v2) :
					 sizeof(struct iwl_nan_cfg_cmd);
}

static inline struct iwl_nan_umac_cfg
*iwl_mvm_nan_get_umac_cfg(struct ieee80211_hw *hw, void *nan_cfg_cmd)
{
	return iwl_mvm_nan_is_ver2(hw) ?
	       &((struct iwl_nan_cfg_cmd_v2 *)nan_cfg_cmd)->umac_cfg :
	       &((struct iwl_nan_cfg_cmd *)nan_cfg_cmd)->umac_cfg;
}

static inline struct iwl_nan_testbed_cfg
*iwl_mvm_nan_get_tb_cfg(struct ieee80211_hw *hw, void *nan_cfg_cmd)
{
	return iwl_mvm_nan_is_ver2(hw) ?
	       &((struct iwl_nan_cfg_cmd_v2 *)nan_cfg_cmd)->tb_cfg :
	       &((struct iwl_nan_cfg_cmd *)nan_cfg_cmd)->tb_cfg;
}

static inline struct iwl_nan_nan2_cfg
*iwl_mvm_nan_get_nan2_cfg(struct ieee80211_hw *hw, void *nan_cfg_cmd)
{
	return iwl_mvm_nan_is_ver2(hw) ?
	       &((struct iwl_nan_cfg_cmd_v2 *)nan_cfg_cmd)->nan2_cfg :
	       &((struct iwl_nan_cfg_cmd *)nan_cfg_cmd)->nan2_cfg;
}

int iwl_mvm_start_nan(struct ieee80211_hw *hw,
		      struct ieee80211_vif *vif,
		      struct cfg80211_nan_conf *conf)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	void *cmd;
	struct iwl_nan_umac_cfg *umac_cfg;
	struct iwl_nan_testbed_cfg *tb_cfg;
	struct iwl_nan_nan2_cfg *nan2_cfg;
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	int ret = 0;
	u16 cdw = 0;

	IWL_DEBUG_MAC80211(IWL_MAC80211_GET_MVM(hw), "Start NAN\n");

	/* 2GHz is mandatory and nl80211 should make sure it is set.
	 * Warn and add 2GHz if this happens anyway.
	 */
	if (WARN_ON(ieee80211_nan_bands(conf) && !(ieee80211_nan_has_band(conf, NL80211_BAND_2GHZ))))
		return -EINVAL;

	/* This function should not be called when using ADD_STA ver >=12 */
	WARN_ON_ONCE(iwl_fw_lookup_cmd_ver(mvm->fw, LONG_GROUP,
					   ADD_STA, 0) >= 12);

	ieee80211_nan_set_band(conf, NL80211_BAND_2GHZ);
	cmd = kzalloc(iwl_mvm_nan_cfg_cmd_len(hw), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	umac_cfg = iwl_mvm_nan_get_umac_cfg(hw, cmd);
	tb_cfg = iwl_mvm_nan_get_tb_cfg(hw, cmd);
	nan2_cfg = iwl_mvm_nan_get_nan2_cfg(hw, cmd);

	mutex_lock(&mvm->mutex);

	umac_cfg->action = cpu_to_le32(FW_CTXT_ACTION_ADD);
	umac_cfg->tsf_id = cpu_to_le32(mvmvif->tsf_id);
	umac_cfg->beacon_template_id = cpu_to_le32(mvmvif->id);

	ether_addr_copy(umac_cfg->node_addr, vif->addr);
	umac_cfg->sta_id = cpu_to_le32(mvm->aux_sta.sta_id);
	umac_cfg->master_pref = conf->master_pref;

	if (ieee80211_nan_has_band(conf, NL80211_BAND_2GHZ)) {
		if (!iwl_mvm_can_beacon(vif, NL80211_BAND_2GHZ,
					NAN_CHANNEL_24)) {
			IWL_ERR(mvm, "Can't beacon on %d\n", NAN_CHANNEL_24);
			ret = -EINVAL;
			goto out;
		}

		tb_cfg->chan24 = NAN_CHANNEL_24;
	}

	if (ieee80211_nan_has_band(conf, NL80211_BAND_5GHZ)) {
		if (!iwl_mvm_can_beacon(vif, NL80211_BAND_5GHZ,
					NAN_CHANNEL_52)) {
			IWL_ERR(mvm, "Can't beacon on %d\n", NAN_CHANNEL_52);
			ret = -EINVAL;
			goto out;
		}

		tb_cfg->chan52 = NAN_CHANNEL_52;
	}

	tb_cfg->warmup_timer = cpu_to_le32(NAN_WARMUP_TIMEOUT_USEC);
	tb_cfg->op_bands = 3;
	nan2_cfg->cdw = cpu_to_le16(cdw);

	if ((ieee80211_nan_has_band(conf, NL80211_BAND_2GHZ)) &&
	    (ieee80211_nan_has_band(conf, NL80211_BAND_5GHZ)))
		umac_cfg->dual_band = cpu_to_le32(1);

	ret = iwl_mvm_send_cmd_pdu(mvm, iwl_cmd_id(NAN_CONFIG_CMD,
						   NAN_GROUP, 0),
				   0, iwl_mvm_nan_cfg_cmd_len(hw), cmd);

	if (!ret)
		mvm->nan_vif = vif;

out:
	mutex_unlock(&mvm->mutex);
	kfree(cmd);

	return ret;
}

int iwl_mvm_stop_nan(struct ieee80211_hw *hw,
		     struct ieee80211_vif *vif)
{
	void *cmd;
	struct iwl_nan_umac_cfg *umac_cfg;
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	int ret = 0;

	IWL_DEBUG_MAC80211(IWL_MAC80211_GET_MVM(hw), "Stop NAN\n");

	cmd = kzalloc(iwl_mvm_nan_cfg_cmd_len(hw), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	umac_cfg = iwl_mvm_nan_get_umac_cfg(hw, cmd);

	mutex_lock(&mvm->mutex);
	umac_cfg->action = cpu_to_le32(FW_CTXT_ACTION_REMOVE);

	ret = iwl_mvm_send_cmd_pdu(mvm, iwl_cmd_id(NAN_CONFIG_CMD,
						   NAN_GROUP, 0),
				   0, iwl_mvm_nan_cfg_cmd_len(hw), cmd);

	if (!ret)
		mvm->nan_vif = NULL;
	mutex_unlock(&mvm->mutex);
	kfree(cmd);

	return ret;
}

static enum iwl_fw_nan_func_type
iwl_fw_nan_func_type(enum nl80211_nan_function_type type)
{
	switch (type) {
	case NL80211_NAN_FUNC_PUBLISH:
		return IWL_NAN_DE_FUNC_PUBLISH;
	case NL80211_NAN_FUNC_SUBSCRIBE:
		return IWL_NAN_DE_FUNC_SUBSCRIBE;
	case NL80211_NAN_FUNC_FOLLOW_UP:
		return IWL_NAN_DE_FUNC_FOLLOW_UP;
	default:
		return IWL_NAN_DE_FUNC_NOT_VALID;
	}
}

static u8
iwl_mvm_get_match_filter_len(struct cfg80211_nan_func_filter *filters,
			     u8 num_filters)
{
	int i;
	unsigned int len = 0;

	len += num_filters;
	for (i = 0; i < num_filters; i++)
		len += filters[i].len;

	if (WARN_ON_ONCE(len > U8_MAX))
		return 0;

	return len;
}

static void iwl_mvm_copy_filters(struct cfg80211_nan_func_filter *filters,
				 u8 num_filters, u8 *cmd_data)
{
	int i;
	u8 offset = 0;

	for (i = 0; i < num_filters; i++) {
		memcpy(cmd_data + offset, &filters[i].len,
		       sizeof(u8));
		offset++;
		if (filters[i].len > 0)
			memcpy(cmd_data + offset, filters[i].filter,
			       filters[i].len);

		offset += filters[i].len;
	}
}

static inline size_t iwl_mvm_nan_add_func_cmd_len(struct ieee80211_hw *hw)
{
	if (iwl_mvm_nan_is_ver2(hw))
		return sizeof(struct iwl_nan_add_func_cmd_v2);

	return sizeof(struct iwl_nan_add_func_cmd) -
		iwl_mvm_chan_info_padding(IWL_MAC80211_GET_MVM(hw));
}

static inline struct iwl_nan_add_func_common
*iwl_mvm_nan_get_add_func_common(struct ieee80211_hw *hw,
				 void *nan_add_func_cmd)
{
	return iwl_mvm_nan_is_ver2(hw) ?
	       &((struct iwl_nan_add_func_cmd_v2 *)nan_add_func_cmd)->cmn :
	       &((struct iwl_nan_add_func_cmd *)nan_add_func_cmd)->cmn;
}

static inline u8 *iwl_mvm_nan_get_add_func_data(struct ieee80211_hw *hw,
						void *nan_add_func_cmd)
{
	if (iwl_mvm_nan_is_ver2(hw))
		return ((struct iwl_nan_add_func_cmd_v2 *)
			nan_add_func_cmd)->data;

	return ((struct iwl_nan_add_func_cmd *)nan_add_func_cmd)->data -
		iwl_mvm_chan_info_padding(IWL_MAC80211_GET_MVM(hw));
}

int iwl_mvm_add_nan_func(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif,
			 const struct cfg80211_nan_func *nan_func)
{
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	void *cmd;
	struct iwl_nan_add_func_common *cmn;
	struct iwl_nan_add_func_common_tail *tail;
	struct iwl_host_cmd hcmd = {
		.id = iwl_cmd_id(NAN_DISCOVERY_FUNC_CMD, NAN_GROUP, 0),
		.flags = CMD_WANT_SKB,
	};
	struct iwl_nan_add_func_res *resp;
	struct iwl_rx_packet *pkt;
	u8 *cmd_data;
	u16 flags = 0;
	u8 tx_filt_len, rx_filt_len;
	size_t cmd_len;
	int ret = 0;

	IWL_DEBUG_MAC80211(IWL_MAC80211_GET_MVM(hw), "Add NAN func\n");

	mutex_lock(&mvm->mutex);

	/* We assume here that mac80211 properly validated the nan_func */
	cmd_len = iwl_mvm_nan_add_func_cmd_len(hw) +
		  ALIGN(nan_func->serv_spec_info_len, 4);
	if (nan_func->srf_bf_len)
		cmd_len += ALIGN(nan_func->srf_bf_len + 1, 4);
	else if (nan_func->srf_num_macs)
		cmd_len += ALIGN(nan_func->srf_num_macs * ETH_ALEN + 1, 4);

	rx_filt_len = iwl_mvm_get_match_filter_len(nan_func->rx_filters,
						   nan_func->num_rx_filters);

	tx_filt_len = iwl_mvm_get_match_filter_len(nan_func->tx_filters,
						   nan_func->num_tx_filters);

	cmd_len += ALIGN(rx_filt_len, 4);
	cmd_len += ALIGN(tx_filt_len, 4);

	cmd = kzalloc(cmd_len, GFP_KERNEL);

	if (!cmd) {
		ret = -ENOBUFS;
		goto unlock;
	}

	hcmd.len[0] = cmd_len;
	hcmd.data[0] = cmd;

	cmn = iwl_mvm_nan_get_add_func_common(hw, cmd);
	tail = iwl_mvm_chan_info_cmd_tail(mvm, &cmn->faw_ci);

	cmd_data = iwl_mvm_nan_get_add_func_data(hw, cmd);
	cmn->action = cpu_to_le32(FW_CTXT_ACTION_ADD);
	cmn->type = iwl_fw_nan_func_type(nan_func->type);
	cmn->instance_id = nan_func->instance_id;
	tail->dw_interval = 1;

	memcpy(&cmn->service_id, nan_func->service_id, sizeof(cmn->service_id));

	/*
	 * TODO: Currently we want all the events, however we might need to be
	 * able to unset this flag for solicited publish to disable "Replied"
	 * events.
	 */
	flags |= IWL_NAN_DE_FUNC_FLAG_RAISE_EVENTS;
	if (nan_func->subscribe_active ||
	    nan_func->publish_type == NL80211_NAN_UNSOLICITED_PUBLISH)
		flags |= IWL_NAN_DE_FUNC_FLAG_UNSOLICITED_OR_ACTIVE;

	if (nan_func->close_range)
		flags |= IWL_NAN_DE_FUNC_FLAG_CLOSE_RANGE;

	if (nan_func->type == NL80211_NAN_FUNC_FOLLOW_UP ||
	    (nan_func->type == NL80211_NAN_FUNC_PUBLISH &&
	     !nan_func->publish_bcast))
		flags |= IWL_NAN_DE_FUNC_FLAG_UNICAST;

	if (nan_func->publish_type == NL80211_NAN_SOLICITED_PUBLISH)
		flags |= IWL_NAN_DE_FUNC_FLAG_SOLICITED;

	cmn->flags = cpu_to_le16(flags);
	cmn->ttl = cpu_to_le32(nan_func->ttl);
	tail->serv_info_len = nan_func->serv_spec_info_len;
	if (nan_func->serv_spec_info_len)
		memcpy(cmd_data, nan_func->serv_spec_info,
		       nan_func->serv_spec_info_len);

	if (nan_func->type == NL80211_NAN_FUNC_FOLLOW_UP) {
		cmn->flw_up_id = nan_func->followup_id;
		cmn->flw_up_req_id = nan_func->followup_reqid;
		memcpy(cmn->flw_up_addr, nan_func->followup_dest.addr,
		       ETH_ALEN);
		cmn->ttl = cpu_to_le32(1);
	}

	cmd_data += ALIGN(tail->serv_info_len, 4);
	if (nan_func->srf_bf_len) {
		u8 srf_ctl = 0;

		srf_ctl |= SRF_BF_TYPE;
		srf_ctl |= (nan_func->srf_bf_idx << 2) & SRF_BLOOM_FILTER_IDX;
		if (nan_func->srf_include)
			srf_ctl |= SRF_INCLUDE;

		tail->srf_len = nan_func->srf_bf_len + 1;
		memcpy(cmd_data, &srf_ctl, sizeof(srf_ctl));
		memcpy(cmd_data + 1, nan_func->srf_bf, nan_func->srf_bf_len);
	} else if (nan_func->srf_num_macs) {
		u8 srf_ctl = 0;
		int i;

		if (nan_func->srf_include)
			srf_ctl |= SRF_INCLUDE;

		tail->srf_len = nan_func->srf_num_macs * ETH_ALEN + 1;
		memcpy(cmd_data, &srf_ctl, sizeof(srf_ctl));

		for (i = 0; i < nan_func->srf_num_macs; i++) {
			memcpy(cmd_data + 1 + i * ETH_ALEN,
			       nan_func->srf_macs[i].addr, ETH_ALEN);
		}
	}

	cmd_data += ALIGN(tail->srf_len, 4);

	if (rx_filt_len > 0)
		iwl_mvm_copy_filters(nan_func->rx_filters,
				     nan_func->num_rx_filters, cmd_data);

	tail->rx_filter_len = rx_filt_len;
	cmd_data += ALIGN(tail->rx_filter_len, 4);

	if (tx_filt_len > 0)
		iwl_mvm_copy_filters(nan_func->tx_filters,
				     nan_func->num_tx_filters, cmd_data);

	tail->tx_filter_len = tx_filt_len;

	ret = iwl_mvm_send_cmd(mvm, &hcmd);

	if (ret) {
		IWL_ERR(mvm, "Couldn't send NAN_DISCOVERY_FUNC_CMD: %d\n", ret);
		goto out_free;
	}

	pkt = hcmd.resp_pkt;

	if (WARN_ON(iwl_rx_packet_payload_len(pkt) != sizeof(*resp))) {
		ret = -EIO;
		goto out_free_resp;
	}

	resp = (void *)pkt->data;

	IWL_DEBUG_MAC80211(mvm,
			   "Add NAN func response status: %d, instance_id: %d\n",
			   resp->status, resp->instance_id);

	if (resp->status == IWL_NAN_DE_FUNC_STATUS_INSUFFICIENT_ENTRIES ||
	    resp->status == IWL_NAN_DE_FUNC_STATUS_INSUFFICIENT_MEMORY) {
		ret = -ENOBUFS;
		goto out_free_resp;
	}

	if (resp->status != IWL_NAN_DE_FUNC_STATUS_SUCCESSFUL) {
		ret = -EIO;
		goto out_free_resp;
	}

	if (cmn->instance_id &&
	    WARN_ON(resp->instance_id != cmn->instance_id)) {
		ret = -EIO;
		goto out_free_resp;
	}

	ret = 0;
out_free_resp:
	iwl_free_resp(&hcmd);
out_free:
	kfree(cmd);
unlock:
	mutex_unlock(&mvm->mutex);
	return ret;
}

void iwl_mvm_del_nan_func(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  u8 instance_id)
{
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	void *cmd;
	struct iwl_nan_add_func_common *cmn;
	int ret;

	IWL_DEBUG_MAC80211(IWL_MAC80211_GET_MVM(hw), "Remove NAN func\n");

	cmd = kzalloc(iwl_mvm_nan_add_func_cmd_len(hw), GFP_KERNEL);
	if (!cmd) {
		IWL_ERR(mvm,
			"Failed to allocate command to remove NAN func instance_id: %d\n",
			instance_id);
		return;
	}

	cmn = iwl_mvm_nan_get_add_func_common(hw, cmd);

	mutex_lock(&mvm->mutex);
	cmn->action = cpu_to_le32(FW_CTXT_ACTION_REMOVE);
	cmn->instance_id = instance_id;

	ret = iwl_mvm_send_cmd_pdu(mvm, iwl_cmd_id(NAN_DISCOVERY_FUNC_CMD,
						   NAN_GROUP, 0),
				   0, iwl_mvm_nan_add_func_cmd_len(hw), cmd);
	if (ret)
		IWL_ERR(mvm, "Failed to remove NAN func instance_id: %d\n",
			instance_id);

	mutex_unlock(&mvm->mutex);
	kfree(cmd);
}

static u8 iwl_cfg_nan_func_type(u8 fw_type)
{
	switch (fw_type) {
	case IWL_NAN_DE_FUNC_PUBLISH:
		return NL80211_NAN_FUNC_PUBLISH;
	case IWL_NAN_DE_FUNC_SUBSCRIBE:
		return NL80211_NAN_FUNC_SUBSCRIBE;
	case IWL_NAN_DE_FUNC_FOLLOW_UP:
		return NL80211_NAN_FUNC_FOLLOW_UP;
	default:
		return NL80211_NAN_FUNC_MAX_TYPE + 1;
	}
}

static void iwl_mvm_nan_match_v1(struct iwl_mvm *mvm,
				 struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_nan_disc_evt_notify_v1 *ev = (void *)pkt->data;
	struct cfg80211_nan_match_params match = {0};
	int len = iwl_rx_packet_payload_len(pkt);

	if (WARN_ON_ONCE(!mvm->nan_vif)) {
		IWL_ERR(mvm, "NAN vif is NULL\n");
		return;
	}

	if (WARN_ON_ONCE(len < sizeof(*ev) + ev->service_info_len)) {
		IWL_ERR(mvm,
			"Invalid NAN match event length: %d, info_len: %d\n",
			len, ev->service_info_len);
		return;
	}

	match.type = iwl_cfg_nan_func_type(ev->type);

	if (WARN_ON_ONCE(match.type > NL80211_NAN_FUNC_MAX_TYPE)) {
		IWL_ERR(mvm, "Invalid func type\n");
		return;
	}

	match.inst_id = ev->instance_id;
	match.peer_inst_id = ev->peer_instance;
	match.addr = ev->peer_mac_addr;
	match.info = ev->buf;
	match.info_len = ev->service_info_len;
	ieee80211_nan_func_match(mvm->nan_vif, &match,
				 GFP_ATOMIC);
}

static void iwl_mvm_nan_match_v2(struct iwl_mvm *mvm,
				 struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_nan_disc_evt_notify_v2 *ev = (void *)pkt->data;
	u32 len = iwl_rx_packet_payload_len(pkt);
	u32 i = 0;

	if (WARN_ONCE(!mvm->nan_vif, "NAN vif is NULL"))
		return;

	if (WARN_ONCE(len < sizeof(*ev), "Invalid NAN match event length=%u",
		      len))
		return;

	if (WARN_ONCE(len < sizeof(*ev) + le32_to_cpu(ev->match_len) +
		      le32_to_cpu(ev->avail_attrs_len),
		      "Bad NAN match event: len=%u, match=%u, attrs=%u\n",
		      len, ev->match_len, ev->avail_attrs_len))
		return;

	i = 0;
	while (i < le32_to_cpu(ev->match_len)) {
		struct cfg80211_nan_match_params match = {0};
		struct iwl_nan_disc_info *disc_info =
			(struct iwl_nan_disc_info *)(((u8 *)(ev + 1)) + i);

		match.type = iwl_cfg_nan_func_type(disc_info->type);
		match.inst_id = disc_info->instance_id;
		match.peer_inst_id = disc_info->peer_instance;
		match.addr = ev->peer_mac_addr;
		match.info = disc_info->buf;
		match.info_len = disc_info->service_info_len;
		ieee80211_nan_func_match(mvm->nan_vif, &match,
					 GFP_ATOMIC);

		i += ALIGN(sizeof(*disc_info) +
			   disc_info->service_info_len +
			   le16_to_cpu(disc_info->sdea_service_info_len) +
			   le16_to_cpu(disc_info->sec_ctxt_len), 4);
	}
}

void iwl_mvm_nan_match(struct iwl_mvm *mvm, struct iwl_rx_cmd_buffer *rxb)
{
	if (fw_has_api(&mvm->fw->ucode_capa, IWL_UCODE_TLV_API_NAN_NOTIF_V2))
		iwl_mvm_nan_match_v2(mvm, rxb);
	else
		iwl_mvm_nan_match_v1(mvm, rxb);
}

void iwl_mvm_nan_de_term_notif(struct iwl_mvm *mvm,
			       struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_nan_de_term *ev = (void *)pkt->data;
	enum nl80211_nan_func_term_reason nl_reason;

	if (WARN_ON_ONCE(!mvm->nan_vif)) {
		IWL_ERR(mvm, "NAN vif is NULL\n");
		return;
	}

	switch (ev->reason) {
	case IWL_NAN_DE_TERM_TTL_REACHED:
		nl_reason = NL80211_NAN_FUNC_TERM_REASON_TTL_EXPIRED;
		break;
	case IWL_NAN_DE_TERM_USER_REQUEST:
		nl_reason = NL80211_NAN_FUNC_TERM_REASON_USER_REQUEST;
		break;
	case IWL_NAN_DE_TERM_FAILURE:
		nl_reason = NL80211_NAN_FUNC_TERM_REASON_ERROR;
		break;
	default:
		WARN_ON_ONCE(1);
		return;
	}

	ieee80211_nan_func_terminated(mvm->nan_vif, ev->instance_id, nl_reason,
				      GFP_ATOMIC);
}

int iwl_mvm_nan_config_nan_faw_cmd(struct iwl_mvm *mvm,
				   struct cfg80211_chan_def *chandef, u8 slots)
{
	struct iwl_nan_faw_config cmd = {};
	struct iwl_nan_faw_config_tail *tail =
		iwl_mvm_chan_info_cmd_tail(mvm, &cmd.faw_ci);
	struct iwl_mvm_vif *mvmvif;
	int ret;

	if (WARN_ON(!mvm->nan_vif))
		return -EINVAL;

	mutex_lock(&mvm->mutex);

	mvmvif = iwl_mvm_vif_from_mac80211(mvm->nan_vif);

	/* Set the channel info data */
	iwl_mvm_set_chan_info_chandef(mvm, &cmd.faw_ci, chandef);

	ieee80211_chandef_to_operating_class(chandef, &tail->op_class);
	tail->slots = slots;
	tail->type = IWL_NAN_POST_NAN_ATTR_FURTHER_NAN;
	cmd.id_n_color = cpu_to_le32(FW_CMD_ID_AND_COLOR(mvmvif->id,
							 mvmvif->color));

	ret = iwl_mvm_send_cmd_pdu(mvm, iwl_cmd_id(NAN_FAW_CONFIG_CMD,
						   NAN_GROUP, 0),
				   0, sizeof(cmd), &cmd);

	mutex_unlock(&mvm->mutex);

	return ret;
}
