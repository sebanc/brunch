// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2005-2014, 2018-2022 Intel Corporation
 * Copyright (C) 2015-2017 Intel Deutschland GmbH
 */
#include <linux/module.h>
#include <linux/types.h>

#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "fw/img.h"
#include "iwl-config.h"
#include "iwl-phy-db.h"
#include "iwl-csr.h"
#include "xvt.h"
#include "user-infc.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"
#include "iwl-io.h"
#include "iwl-prph.h"
#include "fw/dbg.h"
#include "fw/api/rx.h"

#define DRV_DESCRIPTION	"Intel(R) xVT driver for Linux"
MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(IWLWIFI);

#define TX_QUEUE_CFG_TID (6)

static const struct iwl_op_mode_ops iwl_xvt_ops;

/*
 * module init and exit functions
 */
static int __init iwl_xvt_init(void)
{
	return iwl_opmode_register("iwlxvt", &iwl_xvt_ops);
}
module_init(iwl_xvt_init);

static void __exit iwl_xvt_exit(void)
{
	iwl_opmode_deregister("iwlxvt");
}
module_exit(iwl_xvt_exit);

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 * A warning will be triggered on violation.
 */
static const struct iwl_hcmd_names iwl_xvt_cmd_names[] = {
	HCMD_NAME(UCODE_ALIVE_NTFY),
	HCMD_NAME(INIT_COMPLETE_NOTIF),
	HCMD_NAME(TX_CMD),
	HCMD_NAME(SCD_QUEUE_CFG),
	HCMD_NAME(FW_PAGING_BLOCK_CMD),
	HCMD_NAME(PHY_CONFIGURATION_CMD),
	HCMD_NAME(CALIB_RES_NOTIF_PHY_DB),
	HCMD_NAME(NVM_ACCESS_CMD),
	HCMD_NAME(GET_SET_PHY_DB_CMD),
	HCMD_NAME(REPLY_HD_PARAMS_CMD),
	HCMD_NAME(NVM_COMMIT_COMPLETE_NOTIFICATION),
	HCMD_NAME(REPLY_RX_PHY_CMD),
	HCMD_NAME(REPLY_RX_MPDU_CMD),
	HCMD_NAME(FRAME_RELEASE),
	HCMD_NAME(REPLY_RX_DSP_EXT_INFO),
	HCMD_NAME(BA_NOTIF),
	HCMD_NAME(DTS_MEASUREMENT_NOTIFICATION),
	HCMD_NAME(REPLY_DEBUG_XVT_CMD),
	HCMD_NAME(LDBG_CONFIG_CMD),
	HCMD_NAME(DEBUG_LOG_MSG),
};

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 */
static const struct iwl_hcmd_names iwl_xvt_long_cmd_names[] = {
	HCMD_NAME(PHY_CONTEXT_CMD),
	HCMD_NAME(ADD_STA_KEY),
	HCMD_NAME(ADD_STA),
	HCMD_NAME(REMOVE_STA),
	HCMD_NAME(MAC_CONTEXT_CMD),
	HCMD_NAME(BINDING_CONTEXT_CMD),
	HCMD_NAME(LQ_CMD),
	HCMD_NAME(POWER_TABLE_CMD),
	HCMD_NAME(GET_SET_PHY_DB_CMD),
	HCMD_NAME(TX_ANT_CONFIGURATION_CMD),
	HCMD_NAME(REPLY_SF_CFG_CMD),
	HCMD_NAME(DEBUG_HOST_COMMAND),
};

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 */
static const struct iwl_hcmd_names iwl_xvt_phy_names[] = {
	HCMD_NAME(CT_KILL_NOTIFICATION),
	HCMD_NAME(DTS_MEASUREMENT_NOTIF_WIDE),
};

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 */
static const struct iwl_hcmd_names iwl_xvt_data_path_names[] = {
	HCMD_NAME(DQA_ENABLE_CMD),
};

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 */
static const struct iwl_hcmd_names iwl_xvt_regulatory_and_nvm_names[] = {
	HCMD_NAME(NVM_ACCESS_COMPLETE),
};

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 */
static const struct iwl_hcmd_names iwl_xvt_location_names[] = {
	HCMD_NAME(LOCATION_GROUP_NOTIFICATION),
	HCMD_NAME(TOF_MCSI_DEBUG_NOTIF),
	HCMD_NAME(TOF_RANGE_RESPONSE_NOTIF),
};

/* Please keep this array *SORTED* by hex value.
 * Access is done through binary search.
 */
static const struct iwl_hcmd_names iwl_xvt_system_names[] = {
	HCMD_NAME(INIT_EXTENDED_CFG_CMD),
};

static const struct iwl_hcmd_names iwl_xvt_xvt_names[] = {
	HCMD_NAME(DTS_MEASUREMENT_TRIGGER_NOTIF),
	HCMD_NAME(MPAPD_EXEC_DONE_NOTIF),
	HCMD_NAME(RUN_TIME_CALIB_DONE_NOTIF),
	HCMD_NAME(IQ_CALIB_CONFIG_NOTIF),
};

static const struct iwl_hcmd_names iwl_xvt_debug_names[] = {
	HCMD_NAME(DBGC_SUSPEND_RESUME),
	HCMD_NAME(BUFFER_ALLOCATION),
};

static const struct iwl_hcmd_arr iwl_xvt_cmd_groups[] = {
	[LEGACY_GROUP] = HCMD_ARR(iwl_xvt_cmd_names),
	[LONG_GROUP] = HCMD_ARR(iwl_xvt_long_cmd_names),
	[SYSTEM_GROUP] = HCMD_ARR(iwl_xvt_system_names),
	[PHY_OPS_GROUP] = HCMD_ARR(iwl_xvt_phy_names),
	[DATA_PATH_GROUP] = HCMD_ARR(iwl_xvt_data_path_names),
	[LOCATION_GROUP] = HCMD_ARR(iwl_xvt_location_names),
	[REGULATORY_AND_NVM_GROUP] = HCMD_ARR(iwl_xvt_regulatory_and_nvm_names),
	[XVT_GROUP] = HCMD_ARR(iwl_xvt_xvt_names),
	[DEBUG_GROUP] = HCMD_ARR(iwl_xvt_debug_names),
};

static void iwl_xvt_fwrt_dump_start(void *ctx)
{
	struct iwl_xvt *xvt = ctx;

	mutex_lock(&xvt->mutex);
}

static void iwl_xvt_fwrt_dump_end(void *ctx)
{
	struct iwl_xvt *xvt = ctx;

	mutex_unlock(&xvt->mutex);
}

static const struct iwl_fw_runtime_ops iwl_xvt_fwrt_ops = {
	.dump_start = iwl_xvt_fwrt_dump_start,
	.dump_end = iwl_xvt_fwrt_dump_end,
};

static int iwl_xvt_tm_send_hcmd(void *op_mode, struct iwl_host_cmd *host_cmd)
{
	struct iwl_xvt *xvt = (struct iwl_xvt *)op_mode;

	if (WARN_ON_ONCE(!op_mode))
		return -EINVAL;

	return iwl_xvt_send_cmd(xvt, host_cmd);
}

static struct iwl_op_mode *iwl_xvt_start(struct iwl_trans *trans,
					 const struct iwl_cfg *cfg,
					 const struct iwl_fw *fw,
					 struct dentry *dbgfs_dir)
{
	struct iwl_op_mode *op_mode;
	struct iwl_xvt *xvt;
	struct iwl_trans_config trans_cfg = {};
	static const u8 no_reclaim_cmds[] = {
		TX_CMD,
	};
	u8 i;
	int err;

	op_mode = kzalloc(sizeof(struct iwl_op_mode) +
			  sizeof(struct iwl_xvt), GFP_KERNEL);
	if (!op_mode)
		return NULL;

	op_mode->ops = &iwl_xvt_ops;

	xvt = IWL_OP_MODE_GET_XVT(op_mode);
	xvt->fw = fw;
	xvt->cfg = cfg;
	xvt->trans = trans;
	xvt->dev = trans->dev;

	iwl_fw_runtime_init(&xvt->fwrt, trans, fw, &iwl_xvt_fwrt_ops, xvt,
			    NULL, NULL, dbgfs_dir);

	mutex_init(&xvt->mutex);
	spin_lock_init(&xvt->notif_lock);

	/*
	 * Populate the state variables that the
	 * transport layer needs to know about.
	 */
	trans_cfg.op_mode = op_mode;
	trans_cfg.no_reclaim_cmds = no_reclaim_cmds;
	trans_cfg.n_no_reclaim_cmds = ARRAY_SIZE(no_reclaim_cmds);
	trans_cfg.command_groups = iwl_xvt_cmd_groups;
	trans_cfg.command_groups_size = ARRAY_SIZE(iwl_xvt_cmd_groups);
	trans_cfg.cmd_queue = IWL_MVM_DQA_CMD_QUEUE;
	IWL_DEBUG_INFO(xvt, "dqa supported\n");
	trans_cfg.cmd_fifo = IWL_MVM_TX_FIFO_CMD;
	trans_cfg.bc_table_dword =
		trans->trans_cfg->device_family < IWL_DEVICE_FAMILY_AX210;
	trans_cfg.scd_set_active = true;
	trans->wide_cmd_header = true;

	switch (iwlwifi_mod_params.amsdu_size) {
	case IWL_AMSDU_DEF:
	case IWL_AMSDU_4K:
		trans_cfg.rx_buf_size = IWL_AMSDU_4K;
		break;
	case IWL_AMSDU_8K:
		trans_cfg.rx_buf_size = IWL_AMSDU_8K;
		break;
	case IWL_AMSDU_12K:
		trans_cfg.rx_buf_size = IWL_AMSDU_12K;
		break;
	default:
		pr_err("%s: Unsupported amsdu_size: %d\n", KBUILD_MODNAME,
		       iwlwifi_mod_params.amsdu_size);
		trans_cfg.rx_buf_size = IWL_AMSDU_4K;
	}
	/* the hardware splits the A-MSDU */
	if (xvt->trans->trans_cfg->mq_rx_supported)
		trans_cfg.rx_buf_size = IWL_AMSDU_4K;

	trans->rx_mpdu_cmd_hdr_size =
		(trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210) ?
		sizeof(struct iwl_rx_mpdu_desc) : IWL_RX_DESC_SIZE_V1;

	trans_cfg.cb_data_offs = offsetof(struct iwl_xvt_skb_info, trans);

	trans_cfg.fw_reset_handshake = fw_has_capa(&xvt->fw->ucode_capa,
						   IWL_UCODE_TLV_CAPA_FW_RESET_HANDSHAKE);

	trans_cfg.queue_alloc_cmd_ver =
		iwl_fw_lookup_cmd_ver(xvt->fw,
				      WIDE_ID(DATA_PATH_GROUP,
					      SCD_QUEUE_CONFIG_CMD),
				      0);

	/* Configure transport layer */
	iwl_trans_configure(xvt->trans, &trans_cfg);
	trans->command_groups = trans_cfg.command_groups;
	trans->command_groups_size = trans_cfg.command_groups_size;

	/* set up notification wait support */
	iwl_notification_wait_init(&xvt->notif_wait);

	iwl_tm_init(trans, xvt->fw, &xvt->mutex, xvt);

	/* Init phy db */
	xvt->phy_db = iwl_phy_db_init(xvt->trans);
	if (!xvt->phy_db)
		goto out_free;

	iwl_dnt_init(xvt->trans, dbgfs_dir);

	for (i = 0; i < NUM_OF_LMACS; i++) {
		init_waitqueue_head(&xvt->tx_meta_data[i].mod_tx_wq);
		init_waitqueue_head(&xvt->tx_meta_data[i].mod_tx_done_wq);
		xvt->tx_meta_data[i].queue = -1;
		xvt->tx_meta_data[i].tx_mod_thread = NULL;
		xvt->tx_meta_data[i].txq_full = false;
	};

	for (i = 0; i < ARRAY_SIZE(xvt->reorder_bufs); i++)
		xvt->reorder_bufs[i].sta_id = IWL_XVT_INVALID_STA;

	memset(xvt->payloads, 0, sizeof(xvt->payloads));
	xvt->tx_task = NULL;
	xvt->is_enhanced_tx = false;
	xvt->send_tx_resp = false;
	xvt->send_rx_mpdu = true;
	memset(xvt->queue_data, 0, sizeof(xvt->queue_data));
	init_waitqueue_head(&xvt->tx_done_wq);

	trans->dbg.dest_tlv = xvt->fw->dbg.dest_tlv;
	trans->dbg.n_dest_reg = xvt->fw->dbg.n_dest_reg;
	memcpy(trans->dbg.conf_tlv, xvt->fw->dbg.conf_tlv,
	       sizeof(trans->dbg.conf_tlv));
	trans->dbg.trigger_tlv = xvt->fw->dbg.trigger_tlv;

	IWL_INFO(xvt, "Detected %s, REV=0x%X, xVT operation mode\n",
		 xvt->trans->name, xvt->trans->hw_rev);

	err = iwl_xvt_dbgfs_register(xvt, dbgfs_dir);
	if (err)
		IWL_ERR(xvt, "failed register xvt debugfs folder (%d)\n", err);

	return op_mode;

out_free:
	iwl_fw_runtime_free(&xvt->fwrt);
	kfree(op_mode);

	return NULL;
}

static void iwl_xvt_stop(struct iwl_op_mode *op_mode)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	int i;

	iwl_fw_cancel_timestamp(&xvt->fwrt);

	mutex_lock(&xvt->mutex);
	iwl_xvt_stop_op_mode(xvt);
	mutex_unlock(&xvt->mutex);

	for (i = 0; i < ARRAY_SIZE(xvt->reorder_bufs); i++) {
		struct iwl_xvt_reorder_buffer *buffer;

		buffer = &xvt->reorder_bufs[i];
		iwl_xvt_destroy_reorder_buffer(xvt, buffer);
	}

	iwl_fw_flush_dumps(&xvt->fwrt);
	iwl_fw_runtime_free(&xvt->fwrt);

	iwl_phy_db_free(xvt->phy_db);
	xvt->phy_db = NULL;
	iwl_dnt_free(xvt->trans);
	kfree(op_mode);
}

static void iwl_xvt_reclaim_and_free(struct iwl_xvt *xvt,
				     struct tx_meta_data *tx_data,
				     u16 txq_id, u16 ssn)
{
	struct sk_buff_head skbs;
	struct sk_buff *skb;
	struct iwl_xvt_skb_info *skb_info;

	__skb_queue_head_init(&skbs);

	iwl_trans_reclaim(xvt->trans, txq_id, ssn, &skbs);

	while (!skb_queue_empty(&skbs)) {
		skb = __skb_dequeue(&skbs);
		skb_info = (void *)skb->cb;
		if (xvt->is_enhanced_tx) {
			xvt->queue_data[txq_id].tx_counter++;
			xvt->num_of_tx_resp++;
		} else {
			tx_data->tx_counter++;
		}


		if (skb_info->dev_cmd)
			iwl_trans_free_tx_cmd(xvt->trans, skb_info->dev_cmd);
		kfree_skb(skb);
	}

	if (xvt->is_enhanced_tx &&
	    xvt->expected_tx_amount == xvt->num_of_tx_resp)
		wake_up_interruptible(&xvt->tx_done_wq);
	else if (tx_data->tot_tx == tx_data->tx_counter)
		wake_up_interruptible(&tx_data->mod_tx_done_wq);
}

static struct tx_meta_data *
iwl_xvt_rx_get_tx_meta_data(struct iwl_xvt *xvt, u16 txq_id)
{
	u8 lmac_id;

	/*
	 * in case of enhanced_tx, tx_meta_data->queue is not
	 * being set, so there's nothing to verify
	 */
	if (xvt->is_enhanced_tx)
		return &xvt->tx_meta_data[XVT_LMAC_0_ID];

	if (!iwl_xvt_is_unified_fw(xvt)) {
		lmac_id = XVT_LMAC_0_ID;
		goto verify;
	}

	if (txq_id == xvt->tx_meta_data[XVT_LMAC_1_ID].queue) {
		lmac_id = XVT_LMAC_1_ID;
		goto verify;
	}

	lmac_id = XVT_LMAC_0_ID;
verify:
	if (WARN(txq_id != xvt->tx_meta_data[lmac_id].queue,
		 "got TX_CMD from unidentified queue: (lmac %d) %d %d\n",
		 lmac_id, txq_id, xvt->tx_meta_data[lmac_id].queue))
		return NULL;

	return &xvt->tx_meta_data[lmac_id];
}

static void iwl_xvt_txpath_flush(struct iwl_xvt *xvt,
				 struct iwl_rx_packet *resp_pkt)
{
	int i;
	int num_flushed_queues;
	struct iwl_tx_path_flush_cmd_rsp *rsp;

	if (iwl_fw_lookup_notif_ver(xvt->fw, LONG_GROUP, TXPATH_FLUSH, 0) == 0)
		return;

	if (WARN_ON_ONCE(iwl_rx_packet_payload_len(resp_pkt) != sizeof(*rsp)))
		return;

	rsp = (void *)resp_pkt->data;

	num_flushed_queues = le16_to_cpu(rsp->num_flushed_queues);
	if (WARN_ONCE(num_flushed_queues > IWL_TX_FLUSH_QUEUE_RSP,
		      "num_flushed_queues %d", num_flushed_queues))
		return;

	for (i = 0; i < num_flushed_queues; i++) {
		struct iwl_flush_queue_info *queue_info = &rsp->queues[i];
		struct tx_meta_data *tx_data;
		int tid = le16_to_cpu(queue_info->tid);
		int read_before = le16_to_cpu(queue_info->read_before_flush);
		int read_after = le16_to_cpu(queue_info->read_after_flush);
		int queue_num = le16_to_cpu(queue_info->queue_num);

		if (tid == IWL_MGMT_TID)
			tid = IWL_MAX_TID_COUNT;

		IWL_DEBUG_TX_QUEUES(xvt,
				    "tid %d queue_id %d read-before %d read-after %d\n",
				    tid, queue_num, read_before, read_after);
		if (read_before != read_after &&
		    xvt->queue_data[queue_num].txq_full != 1) {
			tx_data = iwl_xvt_rx_get_tx_meta_data(xvt, queue_num);
			if (!tx_data)
				continue;

			iwl_xvt_reclaim_and_free(xvt, tx_data, queue_num, read_after);
		}
	}
}

static void iwl_xvt_rx_tx_cmd_single(struct iwl_xvt *xvt,
				     struct iwl_rx_packet *pkt)
{
	/* struct iwl_mvm_tx_resp_v3 is almost the same */
	struct iwl_mvm_tx_resp *tx_resp = (void *)pkt->data;
	int txq_id = SEQ_TO_QUEUE(le16_to_cpu(pkt->hdr.sequence));
	u16 ssn = iwl_xvt_get_scd_ssn(xvt, tx_resp);
	struct tx_meta_data *tx_data;
	u16 status = le16_to_cpu(iwl_xvt_get_agg_status(xvt, tx_resp)->status) &
				 TX_STATUS_MSK;

	tx_data = iwl_xvt_rx_get_tx_meta_data(xvt, txq_id);
	if (!tx_data)
		return;

	if (unlikely(status != TX_STATUS_SUCCESS))
		IWL_WARN(xvt, "got error TX_RSP status %#x\n", status);

	iwl_xvt_reclaim_and_free(xvt, tx_data, txq_id, ssn);
}

static void iwl_xvt_rx_tx_cmd_handler(struct iwl_xvt *xvt,
				      struct iwl_rx_packet *pkt)
{
	struct iwl_mvm_tx_resp *tx_resp = (void *)pkt->data;

	if (tx_resp->frame_count == 1)
		iwl_xvt_rx_tx_cmd_single(xvt, pkt);

	/* for aggregations - we reclaim on BA_NOTIF */
}

static void iwl_xvt_rx_ba_notif(struct iwl_xvt *xvt,
				struct iwl_rx_packet *pkt)
{
	struct iwl_mvm_ba_notif *ba_notif;
	struct tx_meta_data *tx_data;
	u16 scd_flow;
	u16 scd_ssn;

	if (iwl_xvt_is_unified_fw(xvt)) {
		struct iwl_mvm_compressed_ba_notif *ba_res = (void *)pkt->data;
		u16 queue;
		u16 tfd_idx;

		if (!le16_to_cpu(ba_res->tfd_cnt))
			goto out;

		queue = le16_to_cpu(ba_res->tfd[0].q_num);
		tfd_idx = le16_to_cpu(ba_res->tfd[0].tfd_index);

		tx_data = iwl_xvt_rx_get_tx_meta_data(xvt, queue);
		if (!tx_data)
			return;

		iwl_xvt_reclaim_and_free(xvt, tx_data, queue, tfd_idx);
out:
		IWL_DEBUG_TX_REPLY(xvt,
				   "BA_NOTIFICATION Received from sta_id = %d, flags %x, sent:%d, acked:%d\n",
				   ba_res->sta_id, le32_to_cpu(ba_res->flags),
				   le16_to_cpu(ba_res->txed),
				   le16_to_cpu(ba_res->done));
		return;
	}

	ba_notif = (void *)pkt->data;
	scd_ssn = le16_to_cpu(ba_notif->scd_ssn);
	scd_flow = le16_to_cpu(ba_notif->scd_flow);

	tx_data = iwl_xvt_rx_get_tx_meta_data(xvt, scd_flow);
	if (!tx_data)
		return;

	iwl_xvt_reclaim_and_free(xvt, tx_data, scd_flow, scd_ssn);

	IWL_DEBUG_TX_REPLY(xvt, "ba_notif from %pM, sta_id = %d\n",
			   ba_notif->sta_addr, ba_notif->sta_id);
	IWL_DEBUG_TX_REPLY(xvt,
			   "tid %d, seq %d, bitmap 0x%llx, scd flow %d, ssn %d, sent %d, acked %d\n",
			   ba_notif->tid, le16_to_cpu(ba_notif->seq_ctl),
			   (unsigned long long)le64_to_cpu(ba_notif->bitmap),
			   scd_flow, scd_ssn, ba_notif->txed,
			   ba_notif->txed_2_done);
}

static void iwl_xvt_rx_dispatch(struct iwl_op_mode *op_mode,
				struct napi_struct *napi,
				struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	union iwl_dbg_tlv_tp_data tp_data = { .fw_pkt = pkt };

	iwl_dbg_tlv_time_point(&xvt->fwrt,
			       IWL_FW_INI_TIME_POINT_FW_RSP_OR_NOTIF, &tp_data);

	spin_lock(&xvt->notif_lock);
	iwl_notification_wait_notify(&xvt->notif_wait, pkt);
	IWL_DEBUG_INFO(xvt, "rx dispatch got notification\n");

	switch (WIDE_ID(pkt->hdr.group_id, pkt->hdr.cmd)) {
	case WIDE_ID(LEGACY_GROUP, TX_CMD):
		iwl_xvt_rx_tx_cmd_handler(xvt, pkt);
		break;
	case WIDE_ID(LEGACY_GROUP, BA_NOTIF):
		iwl_xvt_rx_ba_notif(xvt, pkt);
		break;
	case WIDE_ID(LEGACY_GROUP, REPLY_RX_MPDU_CMD):
		iwl_xvt_reorder(xvt, pkt);
		break;
	case WIDE_ID(LONG_GROUP, TXPATH_FLUSH):
		iwl_xvt_txpath_flush(xvt, pkt);
		break;
	case WIDE_ID(LEGACY_GROUP, FRAME_RELEASE):
		iwl_xvt_rx_frame_release(xvt, pkt);
	}

	iwl_xvt_send_user_rx_notif(xvt, rxb);
	spin_unlock(&xvt->notif_lock);
}

static void iwl_xvt_nic_config(struct iwl_op_mode *op_mode)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	u8 radio_cfg_type, radio_cfg_step, radio_cfg_dash;
	u32 reg_val;

	radio_cfg_type = (xvt->fw->phy_config & FW_PHY_CFG_RADIO_TYPE) >>
			 FW_PHY_CFG_RADIO_TYPE_POS;
	radio_cfg_step = (xvt->fw->phy_config & FW_PHY_CFG_RADIO_STEP) >>
			 FW_PHY_CFG_RADIO_STEP_POS;
	radio_cfg_dash = (xvt->fw->phy_config & FW_PHY_CFG_RADIO_DASH) >>
			 FW_PHY_CFG_RADIO_DASH_POS;

	IWL_DEBUG_INFO(xvt, "Radio type=0x%x-0x%x-0x%x\n", radio_cfg_type,
		       radio_cfg_step, radio_cfg_dash);

	if (xvt->trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210)
		return;

	reg_val = CSR_HW_REV_STEP_DASH(xvt->trans->hw_rev);

	/* radio configuration */
	reg_val |= radio_cfg_type << CSR_HW_IF_CONFIG_REG_POS_PHY_TYPE;
	reg_val |= radio_cfg_step << CSR_HW_IF_CONFIG_REG_POS_PHY_STEP;
	reg_val |= radio_cfg_dash << CSR_HW_IF_CONFIG_REG_POS_PHY_DASH;

	WARN_ON((radio_cfg_type << CSR_HW_IF_CONFIG_REG_POS_PHY_TYPE) &
		 ~CSR_HW_IF_CONFIG_REG_MSK_PHY_TYPE);

	/*
	 * TODO: Bits 7-8 of CSR in 8000 HW family and higher set the ADC
	 * sampling, and shouldn't be set to any non-zero value.
	 * The same is supposed to be true of the other HW, but unsetting
	 * them (such as the 7260) causes automatic tests to fail on seemingly
	 * unrelated errors. Need to further investigate this, but for now
	 * we'll separate cases.
	 */
	if (xvt->trans->trans_cfg->device_family < IWL_DEVICE_FAMILY_8000)
		reg_val |= CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI;

	iwl_trans_set_bits_mask(xvt->trans, CSR_HW_IF_CONFIG_REG,
				CSR_HW_IF_CONFIG_REG_MSK_MAC_STEP_DASH |
				CSR_HW_IF_CONFIG_REG_MSK_PHY_TYPE |
				CSR_HW_IF_CONFIG_REG_MSK_PHY_STEP |
				CSR_HW_IF_CONFIG_REG_MSK_PHY_DASH |
				CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI |
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI,
				reg_val);

	/*
	 * W/A : NIC is stuck in a reset state after Early PCIe power off
	 * (PCIe power is lost before PERST# is asserted), causing ME FW
	 * to lose ownership and not being able to obtain it back.
	 */
	if (!xvt->trans->cfg->apmg_not_supported)
		iwl_set_bits_mask_prph(xvt->trans, APMG_PS_CTRL_REG,
				       APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS,
				       ~APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS);
}

static void iwl_xvt_nic_error(struct iwl_op_mode *op_mode, bool sync)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	void *p_table;
	void *p_table_umac = NULL;
	struct iwl_error_event_table_v2 table_v2;
	struct iwl_umac_error_event_table table_umac;
	int err, table_size;

	xvt->fw_error = true;
	wake_up_interruptible(&xvt->tx_meta_data[XVT_LMAC_0_ID].mod_tx_wq);

	iwl_xvt_get_nic_error_log_v2(xvt, &table_v2);
	iwl_xvt_dump_nic_error_log_v2(xvt, &table_v2);
	p_table = kmemdup(&table_v2, sizeof(table_v2), GFP_ATOMIC);
	table_size = sizeof(table_v2);

	if (xvt->trans->dbg.umac_error_event_table ||
	    (xvt->trans->dbg.error_event_table_tlv_status &
	     IWL_ERROR_EVENT_TABLE_UMAC)) {
		iwl_xvt_get_umac_error_log(xvt, &table_umac);
		iwl_xvt_dump_umac_error_log(xvt, &table_umac);
		p_table_umac = kmemdup(&table_umac, sizeof(table_umac),
				       GFP_ATOMIC);
	}

	if (p_table) {
		err = iwl_xvt_user_send_notif(xvt, IWL_XVT_CMD_SEND_NIC_ERROR,
					      (void *)p_table, table_size,
					      GFP_ATOMIC);
		if (err)
			IWL_WARN(xvt,
				 "Error %d sending NIC error notification\n",
				 err);
		kfree(p_table);
	}

	if (p_table_umac) {
		err = iwl_xvt_user_send_notif(xvt,
					      IWL_XVT_CMD_SEND_NIC_UMAC_ERROR,
					      (void *)p_table_umac,
					      sizeof(table_umac), GFP_ATOMIC);
		if (err)
			IWL_WARN(xvt,
				 "Error %d sending NIC umac error notification\n",
				 err);
		kfree(p_table_umac);
	}

	iwl_fw_error_collect(&xvt->fwrt, sync);
}

static bool iwl_xvt_set_hw_rfkill_state(struct iwl_op_mode *op_mode, bool state)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	u32 rfkill_state = state ? IWL_XVT_RFKILL_ON : IWL_XVT_RFKILL_OFF;
	int err;

	err = iwl_xvt_user_send_notif(xvt, IWL_XVT_CMD_SEND_RFKILL,
				      &rfkill_state, sizeof(rfkill_state),
				      GFP_ATOMIC);
	if (err)
		IWL_WARN(xvt, "Error %d sending RFKILL notification\n", err);

	return false;
}

static void iwl_xvt_free_skb(struct iwl_op_mode *op_mode, struct sk_buff *skb)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	struct iwl_xvt_skb_info *skb_info = (void *)skb->cb;

	iwl_trans_free_tx_cmd(xvt->trans, skb_info->dev_cmd);
	kfree_skb(skb);
}

static void iwl_xvt_stop_sw_queue(struct iwl_op_mode *op_mode, int queue)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	u8 i;

	if (xvt->queue_data[queue].allocated_queue) {
		xvt->queue_data[queue].txq_full = true;
	} else {
		for (i = 0; i < NUM_OF_LMACS; i++) {
			if (queue == xvt->tx_meta_data[i].queue) {
				xvt->tx_meta_data[i].txq_full = true;
				break;
			}
		}
	}
}

static void iwl_xvt_wake_sw_queue(struct iwl_op_mode *op_mode, int queue)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);
	u8 i;

	if (xvt->queue_data[queue].allocated_queue) {
		xvt->queue_data[queue].txq_full = false;
		wake_up_interruptible(&xvt->queue_data[queue].tx_wq);
	} else {
		for (i = 0; i < NUM_OF_LMACS; i++) {
			if (queue == xvt->tx_meta_data[i].queue) {
				xvt->tx_meta_data[i].txq_full = false;
				wake_up_interruptible(
					&xvt->tx_meta_data[i].mod_tx_wq);
				break;
			}
		}
	}
}

static void iwl_xvt_time_point(struct iwl_op_mode *op_mode,
			       enum iwl_fw_ini_time_point tp_id,
			       union iwl_dbg_tlv_tp_data *tp_data)
{
	struct iwl_xvt *xvt = IWL_OP_MODE_GET_XVT(op_mode);

	iwl_dbg_tlv_time_point(&xvt->fwrt, tp_id, tp_data);
}

static const struct iwl_op_mode_ops iwl_xvt_ops = {
	.start = iwl_xvt_start,
	.stop = iwl_xvt_stop,
	.rx = iwl_xvt_rx_dispatch,
	.nic_config = iwl_xvt_nic_config,
	.nic_error = iwl_xvt_nic_error,
	.hw_rf_kill = iwl_xvt_set_hw_rfkill_state,
	.free_skb = iwl_xvt_free_skb,
	.queue_full = iwl_xvt_stop_sw_queue,
	.queue_not_full = iwl_xvt_wake_sw_queue,
	.time_point = iwl_xvt_time_point,
	.test_ops = {
		.send_hcmd = iwl_xvt_tm_send_hcmd,
		.cmd_exec = iwl_xvt_user_cmd_execute,
	},
};

void iwl_xvt_free_tx_queue(struct iwl_xvt *xvt, u8 lmac_id)
{
	int ret = 0;
	u32 new_cmd_id = WIDE_ID(DATA_PATH_GROUP, SCD_QUEUE_CONFIG_CMD);

	if (xvt->tx_meta_data[lmac_id].queue == -1)
		return;

	if (iwl_fw_lookup_cmd_ver(xvt->fw, new_cmd_id, 0) >= 3) {
		struct iwl_scd_queue_cfg_cmd remove_cmd = {
			.operation = cpu_to_le32(IWL_SCD_QUEUE_REMOVE),
			.u.remove.sta_mask = cpu_to_le32(xvt->tx_meta_data[lmac_id].sta_msk),
			.u.remove.tid = cpu_to_le32(TX_QUEUE_CFG_TID),
		};
		ret = iwl_xvt_send_cmd_pdu(xvt, new_cmd_id, 0, sizeof(remove_cmd), &remove_cmd);
		if (ret)
			IWL_ERR(xvt, "Failed to remove queue %d with %d error\n",
				xvt->tx_meta_data[lmac_id].queue, ret);
	}

	iwl_trans_txq_free(xvt->trans, xvt->tx_meta_data[lmac_id].queue);

	xvt->tx_meta_data[lmac_id].queue = -1;
	xvt->tx_meta_data[lmac_id].sta_msk = 0;
}

int iwl_xvt_allocate_tx_queue(struct iwl_xvt *xvt, u8 sta_id,
			      u8 lmac_id)
{
	int ret = 0;
	int size = max_t(u32, IWL_DEFAULT_QUEUE_SIZE,
			 xvt->trans->cfg->min_ba_txq_size);

	if (xvt->tx_meta_data[lmac_id].sta_msk & BIT(sta_id))
		return ret;

	ret = iwl_trans_txq_alloc(xvt->trans, 0,
				  BIT(sta_id), TX_QUEUE_CFG_TID, size, 0);
	/* ret is positive when func returns the allocated the queue number */
	if (ret > 0) {
		xvt->tx_meta_data[lmac_id].queue = ret;
		xvt->tx_meta_data[lmac_id].sta_msk |= BIT(sta_id);
		ret = 0;
	} else {
		IWL_ERR(xvt, "failed to allocate queue\n");
	}

	return ret;
}

void iwl_xvt_txq_disable(struct iwl_xvt *xvt)
{
	if (!iwl_xvt_has_default_txq(xvt))
		return;
	if (iwl_xvt_is_unified_fw(xvt)) {
		iwl_xvt_free_tx_queue(xvt, XVT_LMAC_0_ID);
		iwl_xvt_free_tx_queue(xvt, XVT_LMAC_1_ID);
	} else {
		iwl_trans_txq_disable(xvt->trans,
				      IWL_XVT_DEFAULT_TX_QUEUE,
				      true);
	}
}

#ifdef CONFIG_ACPI
static int iwl_xvt_sar_geo_init(struct iwl_xvt *xvt)
{
	u32 cmd_id = WIDE_ID(PHY_OPS_GROUP, PER_CHAIN_LIMIT_OFFSET_CMD);
	union iwl_geo_tx_power_profiles_cmd cmd;
	u16 len;
	u32 n_bands;
	u32 n_profiles;
	u32 sk = 0;
	int ret;
	u8 cmd_ver = iwl_fw_lookup_cmd_ver(xvt->fw, cmd_id,
					   IWL_FW_CMD_VER_UNKNOWN);

	BUILD_BUG_ON(offsetof(struct iwl_geo_tx_power_profiles_cmd_v1, ops) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v2, ops) ||
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v2, ops) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v3, ops) ||
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v3, ops) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v4, ops) ||
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v4, ops) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v5, ops));
	/* the ops field is at the same spot for all versions, so set in v1 */
	cmd.v1.ops = cpu_to_le32(IWL_PER_CHAIN_OFFSET_SET_TABLES);

	/* Only set to South Korea if the table revision is 1 */
	if (xvt->fwrt.geo_rev == 1)
		sk = 1;

	/*
	 * Set the table_revision to South Korea (1) or not (0).  The
	 * element name is misleading, as it doesn't contain the table
	 * revision number, but whether the South Korea variation
	 * should be used.
	 * This must be done after calling iwl_sar_geo_init().
	 */
	if (cmd_ver == 5) {
		len = sizeof(cmd.v5);
		n_bands = ARRAY_SIZE(cmd.v5.table[0]);
		cmd.v5.table_revision = cpu_to_le32(sk);
		n_profiles = ACPI_NUM_GEO_PROFILES_REV3;
	} else if (cmd_ver == 4) {
		len = sizeof(cmd.v4);
		n_bands = ARRAY_SIZE(cmd.v4.table[0]);
		cmd.v4.table_revision = cpu_to_le32(sk);
		n_profiles = ACPI_NUM_GEO_PROFILES_REV3;
	} else if (cmd_ver == 3) {
		len = sizeof(cmd.v3);
		n_bands = ARRAY_SIZE(cmd.v3.table[0]);
		cmd.v3.table_revision = cpu_to_le32(sk);
		n_profiles = ACPI_NUM_GEO_PROFILES;
	} else if (fw_has_api(&xvt->fwrt.fw->ucode_capa,
			      IWL_UCODE_TLV_API_SAR_TABLE_VER)) {
		len =  sizeof(cmd.v2);
		n_bands = ARRAY_SIZE(cmd.v2.table[0]);
		cmd.v2.table_revision = cpu_to_le32(sk);
		n_profiles = ACPI_NUM_GEO_PROFILES;
	} else {
		len = sizeof(cmd.v1);
		n_bands = ARRAY_SIZE(cmd.v1.table[0]);
		n_profiles = ACPI_NUM_GEO_PROFILES;
	}

	BUILD_BUG_ON(offsetof(struct iwl_geo_tx_power_profiles_cmd_v1, table) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v2, table) ||
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v2, table) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v3, table) ||
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v3, table) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v4, table) ||
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v4, table) !=
		     offsetof(struct iwl_geo_tx_power_profiles_cmd_v5, table));
	/* the table is at the same position for all versions, so set use v1 */
	ret = iwl_sar_geo_init(&xvt->fwrt, &cmd.v1.table[0][0],
			       n_bands, n_profiles);

	/*
	 * It is a valid scenario to not support SAR, or miss wgds table,
	 * but in that case there is no need to send the command.
	 */
	if (ret)
		return 0;

	return iwl_xvt_send_cmd_pdu(xvt, cmd_id, 0, len, &cmd);
}
#else /* CONFIG_ACPI */
static int iwl_xvt_sar_geo_init(struct iwl_xvt *xvt)
{
	return 0;
}

void iwl_xvt_lari_cfg(struct iwl_xvt *xvt)
{
}

#endif /* CONFIG_ACPI */

int iwl_xvt_sar_select_profile(struct iwl_xvt *xvt, int prof_a, int prof_b)
{
	u32 cmd_id = REDUCE_TX_POWER_CMD;
	struct iwl_dev_tx_power_cmd cmd = {
		.common.set_mode = cpu_to_le32(IWL_TX_POWER_MODE_SET_CHAINS),
	};
	__le16 *per_chain;
	u16 len = 0;
	u32 n_subbands;
	u8 cmd_ver = iwl_fw_lookup_cmd_ver(xvt->fw, cmd_id,
					   IWL_FW_CMD_VER_UNKNOWN);
	if (cmd_ver == 6) {
		len = sizeof(cmd.v6);
		n_subbands = IWL_NUM_SUB_BANDS_V2;
		per_chain = cmd.v6.per_chain[0][0];
	} else if (fw_has_api(&xvt->fw->ucode_capa,
			      IWL_UCODE_TLV_API_REDUCE_TX_POWER)) {
		len = sizeof(cmd.v5);
		n_subbands = IWL_NUM_SUB_BANDS_V1;
		per_chain = cmd.v5.per_chain[0][0];
	} else if (fw_has_capa(&xvt->fw->ucode_capa,
			       IWL_UCODE_TLV_CAPA_TX_POWER_ACK)) {
		len = sizeof(cmd.v4);
		n_subbands = IWL_NUM_SUB_BANDS_V1;
		per_chain = cmd.v4.per_chain[0][0];
	} else {
		len = sizeof(cmd.v3);
		n_subbands = IWL_NUM_SUB_BANDS_V1;
		per_chain = cmd.v3.per_chain[0][0];
	}

	/* all structs have the same common part, add it */
	len += sizeof(cmd.common);

	if (iwl_sar_select_profile(&xvt->fwrt, per_chain, IWL_NUM_CHAIN_TABLES,
				   n_subbands, prof_a, prof_b))
		return -ENOENT;

	IWL_DEBUG_RADIO(xvt, "Sending REDUCE_TX_POWER_CMD per chain\n");
	return iwl_xvt_send_cmd_pdu(xvt, cmd_id, 0, len, &cmd);
}

static int iwl_xvt_sar_init(struct iwl_xvt *xvt)
{
	int ret;

	ret = iwl_sar_get_wrds_table(&xvt->fwrt);
	if (ret < 0) {
		IWL_DEBUG_RADIO(xvt,
				"WRDS SAR BIOS table invalid or unavailable. (%d)\n",
				ret);
		/*
		 * If not available, don't fail and don't bother with EWRD and
		 * WGDS */
		if (!iwl_sar_get_wgds_table(&xvt->fwrt)) {
			/*
			 * If basic SAR is not available, we check for WGDS,
			 * which should *not* be available either.  If it is
			 * available, issue an error, because we can't use SAR
			 * Geo without basic SAR.
			 */
			IWL_ERR(xvt, "BIOS contains WGDS but no WRDS\n");
		}
	} else {
		ret = iwl_sar_get_ewrd_table(&xvt->fwrt);
		/* if EWRD is not available, we can still use
		 * WRDS, so don't fail */
		if (ret < 0)
			IWL_DEBUG_RADIO(xvt,
					"EWRD SAR BIOS table invalid or unavailable. (%d)\n",
					ret);

		/* read geo SAR table */
		if (iwl_sar_geo_support(&xvt->fwrt)) {
			ret = iwl_sar_get_wgds_table(&xvt->fwrt);
			if (ret < 0)
				IWL_DEBUG_RADIO(xvt,
						"Geo SAR BIOS table invalid or unavailable. (%d)\n",
						ret);
			/* we don't fail if the table is not available */
		}
	}

	ret = iwl_xvt_sar_select_profile(xvt, 1, 1);
	/*
	 * If we don't have profile 0 from BIOS, just skip it.  This
	 * means that SAR Geo will not be enabled either, even if we
	 * have other valid profiles.
	 */
	if (ret == -ENOENT)
		return 1;

	return ret;
}

int iwl_xvt_init_sar_tables(struct iwl_xvt *xvt)
{
	int ret;

	ret = iwl_xvt_sar_init(xvt);

	if (ret == 0) {
		ret = iwl_xvt_sar_geo_init(xvt);
	} else if (ret > 0 && !iwl_sar_get_wgds_table(&xvt->fwrt)) {
		/*
		 * If basic SAR is not available, we check for WGDS,
		 * which should *not* be available either.  If it is
		 * available, issue an error, because we can't use SAR
		 * Geo without basic SAR.
		 */
		IWL_ERR(xvt, "BIOS contains WGDS but no WRDS\n");
	}

	return ret;
}

static int iwl_xvt_ppag_send_cmd(struct iwl_xvt *xvt)
{
	union iwl_ppag_table_cmd cmd;
	int ret, cmd_size;

	ret = iwl_read_ppag_table(&xvt->fwrt, &cmd, &cmd_size);
	if (ret < 0)
		return ret;

	IWL_DEBUG_RADIO(xvt, "Sending PER_PLATFORM_ANT_GAIN_CMD\n");
	ret = iwl_xvt_send_cmd_pdu(xvt, WIDE_ID(PHY_OPS_GROUP,
						PER_PLATFORM_ANT_GAIN_CMD),
				   0, cmd_size, &cmd);
	if (ret < 0)
		IWL_ERR(xvt, "failed to send PER_PLATFORM_ANT_GAIN_CMD (%d)\n",
			ret);

	return ret;
}

int iwl_xvt_init_ppag_tables(struct iwl_xvt *xvt)
{
	int ret;

	ret = iwl_acpi_get_ppag_table(&xvt->fwrt);
	if (ret < 0) {
		IWL_DEBUG_RADIO(xvt,
				"PPAG BIOS table invalid or unavailable. (%d)\n",
				ret);
	}

	if (!(iwl_acpi_is_ppag_approved(&xvt->fwrt)))
		return 0;

	return iwl_xvt_ppag_send_cmd(xvt);
}

void iwl_xvt_txpath_flush_send_cmd(struct iwl_xvt *xvt, u32 sta_id, u16 tids)
{
	int ret;
	struct iwl_tx_path_flush_cmd flush_cmd = {
		.sta_id = cpu_to_le32(sta_id),
		.tid_mask = cpu_to_le16(tids),
	};

	struct iwl_host_cmd cmd = {
		.id = TXPATH_FLUSH,
		.len = { sizeof(flush_cmd), },
		.data = { &flush_cmd, },
		.flags = CMD_WANT_SKB,
	};

	IWL_DEBUG_TX_QUEUES(xvt, "flush for sta id %d tid mask 0x%x\n", sta_id, tids);

	ret = iwl_xvt_send_cmd(xvt, &cmd);

	if (ret) {
		IWL_ERR(xvt, "Failed to send flush command (%d)\n", ret);
		return;
	}

	iwl_xvt_txpath_flush(xvt, cmd.resp_pkt);
}

void iwl_xvt_lari_cfg(struct iwl_xvt *xvt)
{
	int ret;
	u32 value;
	struct iwl_lari_config_change_cmd_v6 cmd = {};

	cmd.config_bitmap = iwl_acpi_get_lari_config_bitmap(&xvt->fwrt);

	ret = iwl_acpi_get_dsm_u32(xvt->fwrt.dev, 0,
				   DSM_FUNC_ENABLE_UNII4_CHAN,
				   &iwl_guid, &value);
	if (!ret)
		cmd.oem_unii4_allow_bitmap = cpu_to_le32(value);

	if (cmd.config_bitmap ||
	    cmd.oem_uhb_allow_bitmap ||
	    cmd.oem_unii4_allow_bitmap) {
		size_t cmd_size;
		u8 cmd_ver = iwl_fw_lookup_cmd_ver(xvt->fw,
						   WIDE_ID(REGULATORY_AND_NVM_GROUP,
							   LARI_CONFIG_CHANGE), 1);

		switch (cmd_ver) {
		case 6:
			cmd_size = sizeof(struct iwl_lari_config_change_cmd_v6);
			break;
		case 5:
			cmd_size = sizeof(struct iwl_lari_config_change_cmd_v5);
			break;
		case 4:
			cmd_size = sizeof(struct iwl_lari_config_change_cmd_v4);
			break;
		case 3:
			cmd_size = sizeof(struct iwl_lari_config_change_cmd_v3);
			break;
		case 2:
			cmd_size = sizeof(struct iwl_lari_config_change_cmd_v2);
			break;
		default:
			cmd_size = sizeof(struct iwl_lari_config_change_cmd_v1);
			break;
		}

		IWL_DEBUG_RADIO(xvt,
				"sending LARI_CONFIG_CHANGE, config_bitmap=0x%x, oem_11ax_allow_bitmap=0x%x\n",
				le32_to_cpu(cmd.config_bitmap),
				le32_to_cpu(cmd.oem_11ax_allow_bitmap));
		IWL_DEBUG_RADIO(xvt,
				"sending LARI_CONFIG_CHANGE, oem_unii4_allow_bitmap=0x%x, chan_state_active_bitmap=0x%x, cmd_ver=%d\n",
				le32_to_cpu(cmd.oem_unii4_allow_bitmap),
				le32_to_cpu(cmd.chan_state_active_bitmap),
				cmd_ver);

		ret = iwl_xvt_send_cmd_pdu(xvt,
					   WIDE_ID(REGULATORY_AND_NVM_GROUP,
						   LARI_CONFIG_CHANGE),
					0, cmd_size, &cmd);

		if (ret < 0)
			IWL_DEBUG_RADIO(xvt, "Failed to send LARI_CONFIG_CHANGE (%d)\n",
					ret);
	}
}
