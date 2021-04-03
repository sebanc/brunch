// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2005-2014, 2018-2020 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 * Copyright (C) 2015-2017 Intel Deutschland GmbH
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/pci_ids.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/limits.h>

#include "iwl-drv.h"
#include "iwl-prph.h"
#include "iwl-csr.h"
#include "iwl-io.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-phy-db.h"
#include "xvt.h"
#include "user-infc.h"
#include "iwl-dnt-cfg.h"
#include "iwl-dnt-dispatch.h"
#include "iwl-trans.h"
#include "fw/dbg.h"
#include "fw/acpi.h"
#include "fw/img.h"

#define XVT_UCODE_CALIB_TIMEOUT (CPTCFG_IWL_TIMEOUT_FACTOR * HZ)
#define XVT_SCU_BASE	(0xe6a00000)
#define XVT_SCU_SNUM1	(XVT_SCU_BASE + 0x300)
#define XVT_SCU_SNUM2	(XVT_SCU_SNUM1 + 0x4)
#define XVT_SCU_SNUM3	(XVT_SCU_SNUM2 + 0x4)
#define XVT_MAX_TX_COUNT (ULLONG_MAX)
#define XVT_LMAC_0_STA_ID (0) /* must be aligned with station id added in USC */
#define XVT_LMAC_1_STA_ID (2) /* must be aligned with station id added in USC */
#define XVT_STOP_TX (IEEE80211_SCTL_FRAG + 1)

void iwl_xvt_send_user_rx_notif(struct iwl_xvt *xvt,
				struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	void *data = pkt->data;
	u32 size = iwl_rx_packet_payload_len(pkt);

	IWL_DEBUG_INFO(xvt, "rx notification: group=0x%x, id=0x%x\n",
		       pkt->hdr.group_id, pkt->hdr.cmd);

	switch (WIDE_ID(pkt->hdr.group_id, pkt->hdr.cmd)) {
	case WIDE_ID(LONG_GROUP, GET_SET_PHY_DB_CMD):
	case WIDE_ID(XVT_GROUP, GRP_XVT_GET_SET_PHY_DB_CMD):
		iwl_xvt_user_send_notif(xvt, IWL_TM_USER_CMD_NOTIF_PHY_DB,
					data, size, GFP_ATOMIC);
		break;
	case DTS_MEASUREMENT_NOTIFICATION:
	case WIDE_ID(PHY_OPS_GROUP, DTS_MEASUREMENT_NOTIF_WIDE):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_DTS_MEASUREMENTS,
					data, size, GFP_ATOMIC);
		break;
	case REPLY_RX_DSP_EXT_INFO:
		if (!xvt->rx_hdr_enabled)
			break;

		iwl_xvt_user_send_notif(xvt, IWL_TM_USER_CMD_NOTIF_RX_HDR,
					data, size, GFP_ATOMIC);
		break;
	case APMG_PD_SV_CMD:
		if (!xvt->apmg_pd_en)
			break;

		iwl_xvt_user_send_notif(xvt, IWL_TM_USER_CMD_NOTIF_APMG_PD,
					data, size, GFP_ATOMIC);
		break;
	case REPLY_RX_MPDU_CMD:
		if (!xvt->send_rx_mpdu)
			break;

		iwl_xvt_user_send_notif(xvt, IWL_TM_USER_CMD_NOTIF_UCODE_RX_PKT,
					data, size, GFP_ATOMIC);
		break;
	case NVM_COMMIT_COMPLETE_NOTIFICATION:
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_COMMIT_STATISTICS,
					data, size, GFP_ATOMIC);
		break;
	case REPLY_HD_PARAMS_CMD:
		iwl_xvt_user_send_notif(xvt, IWL_TM_USER_CMD_NOTIF_BFE,
					data, size, GFP_ATOMIC);
		break;
	case DEBUG_LOG_MSG:
		iwl_dnt_dispatch_collect_ucode_message(xvt->trans, rxb);
		break;
	case WIDE_ID(LOCATION_GROUP, TOF_MCSI_DEBUG_NOTIF):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_LOC_MCSI,
					data, size, GFP_ATOMIC);
		break;
	case WIDE_ID(LOCATION_GROUP, TOF_RANGE_RESPONSE_NOTIF):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_LOC_RANGE,
					data, size, GFP_ATOMIC);
		break;
	case WIDE_ID(XVT_GROUP, IQ_CALIB_CONFIG_NOTIF):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_IQ_CALIB,
					data, size, GFP_ATOMIC);
		break;
	case WIDE_ID(XVT_GROUP, DTS_MEASUREMENT_TRIGGER_NOTIF):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_DTS_MEASUREMENTS_XVT,
					data, size, GFP_ATOMIC);
		break;
	case WIDE_ID(XVT_GROUP, MPAPD_EXEC_DONE_NOTIF):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_MPAPD_EXEC_DONE,
					data, size, GFP_ATOMIC);
		break;
	case WIDE_ID(XVT_GROUP, RUN_TIME_CALIB_DONE_NOTIF):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_RUN_TIME_CALIB_DONE,
					data, size, GFP_ATOMIC);
		break;
	case WIDE_ID(PHY_OPS_GROUP, CT_KILL_NOTIFICATION):
		iwl_xvt_user_send_notif(xvt,
					IWL_TM_USER_CMD_NOTIF_CT_KILL,
					data, size, GFP_ATOMIC);
		break;
	case REPLY_RX_PHY_CMD:
		IWL_DEBUG_INFO(xvt,
			       "REPLY_RX_PHY_CMD received but not handled\n");
		break;
	case INIT_COMPLETE_NOTIF:
		IWL_DEBUG_INFO(xvt, "received INIT_COMPLETE_NOTIF\n");
		break;
	case TX_CMD:
		if (xvt->send_tx_resp)
			iwl_xvt_user_send_notif(xvt, IWL_XVT_CMD_TX_CMD_RESP,
						data, size, GFP_ATOMIC);
		break;
	default:
		IWL_DEBUG_INFO(xvt, "xVT mode RX command 0x%x not handled\n",
			       pkt->hdr.cmd);
	}
}

static void iwl_xvt_led_enable(struct iwl_xvt *xvt)
{
	iwl_write32(xvt->trans, CSR_LED_REG, CSR_LED_REG_TURN_ON);
}

static void iwl_xvt_led_disable(struct iwl_xvt *xvt)
{
	iwl_write32(xvt->trans, CSR_LED_REG, CSR_LED_REG_TURN_OFF);
}

static int iwl_xvt_sdio_io_toggle(struct iwl_xvt *xvt,
				 struct iwl_tm_data *data_in,
				 struct iwl_tm_data *data_out)
{
	struct iwl_tm_sdio_io_toggle *sdio_io_toggle = data_in->data;

	return iwl_trans_test_mode_cmd(xvt->trans, sdio_io_toggle->enable);
}

/**
 * iwl_xvt_read_sv_drop - read SV drop version
 * @xvt: xvt data
 * Return: the SV drop (>= 0) or a negative error number
 */
static int iwl_xvt_read_sv_drop(struct iwl_xvt *xvt)
{
	struct xvt_debug_cmd debug_cmd = {
		.opcode = cpu_to_le32(XVT_DBG_GET_SVDROP_VER_OP),
		.dw_num = 0,
	};
	struct xvt_debug_res *debug_res;
	struct iwl_rx_packet *pkt;
	struct iwl_host_cmd host_cmd = {
		.id = REPLY_DEBUG_XVT_CMD,
		.data[0] = &debug_cmd,
		.len[0] = sizeof(debug_cmd),
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
		.flags = CMD_WANT_SKB,
	};
	int ret;

	if (xvt->state != IWL_XVT_STATE_OPERATIONAL)
		return 0;

	ret = iwl_xvt_send_cmd(xvt, &host_cmd);
	if (ret)
		return ret;

	/* Retrieve response packet */
	pkt = host_cmd.resp_pkt;

	/* Get response data */
	debug_res = (struct xvt_debug_res *)pkt->data;
	if (le32_to_cpu(debug_res->dw_num) < 1) {
		ret = -ENODATA;
		goto out;
	}
	ret = le32_to_cpu(debug_res->data[0]) & 0xFF;

out:
	iwl_free_resp(&host_cmd);
	return ret;
}

static int iwl_xvt_get_dev_info(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_tm_dev_info_req *dev_info_req;
	struct iwl_tm_dev_info *dev_info;
	const u8 driver_ver[] = BACKPORTS_GIT_TRACKED;
	int sv_step = 0x00;
	int dev_info_size;
	bool read_sv_drop = true;

	if (data_in) {
		dev_info_req = (struct iwl_tm_dev_info_req *)data_in->data;
		read_sv_drop = dev_info_req->read_sv ? true : false;
	}

	if (xvt->fwrt.cur_fw_img == IWL_UCODE_REGULAR && read_sv_drop) {
		sv_step = iwl_xvt_read_sv_drop(xvt);
		if (sv_step < 0)
			return sv_step;
	}

	dev_info_size = sizeof(struct iwl_tm_dev_info) +
			(strlen(driver_ver)+1)*sizeof(u8);
	dev_info = kzalloc(dev_info_size, GFP_KERNEL);
	if (!dev_info)
		return -ENOMEM;

	dev_info->dev_id = xvt->trans->hw_id;
	dev_info->fw_ver = xvt->fw->ucode_ver;
	dev_info->vendor_id = PCI_VENDOR_ID_INTEL;
	dev_info->build_ver = sv_step;

	/*
	 * TODO: Silicon step is retrieved by reading
	 * radio register 0x00. Simplifying implementation
	 * by reading it in user space.
	 */
	dev_info->silicon_step = 0x00;

	strcpy(dev_info->driver_ver, driver_ver);

	data_out->data = dev_info;
	data_out->len = dev_info_size;

	return 0;
}

static int iwl_xvt_set_sw_config(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_xvt_sw_cfg_request *sw_cfg =
				(struct iwl_xvt_sw_cfg_request *)data_in->data;
	struct iwl_phy_cfg_cmd_v3 *fw_calib_cmd_cfg =
				xvt->sw_stack_cfg.fw_calib_cmd_cfg;
	__le32 cfg_mask = cpu_to_le32(sw_cfg->cfg_mask),
	       fw_calib_event, fw_calib_flow,
	       event_override, flow_override;
	int usr_idx, iwl_idx;

	if (data_in->len < sizeof(struct iwl_xvt_sw_cfg_request))
		return -EINVAL;

	xvt->sw_stack_cfg.fw_dbg_flags = sw_cfg->dbg_flags;
	xvt->sw_stack_cfg.load_mask = sw_cfg->load_mask;
	xvt->sw_stack_cfg.calib_override_mask = sw_cfg->cfg_mask;

	for (usr_idx = 0; usr_idx < IWL_USER_FW_IMAGE_IDX_TYPE_MAX; usr_idx++) {
		switch (usr_idx) {
		case IWL_USER_FW_IMAGE_IDX_INIT:
			iwl_idx = IWL_UCODE_INIT;
			break;
		case IWL_USER_FW_IMAGE_IDX_REGULAR:
			iwl_idx = IWL_UCODE_REGULAR;
			break;
		case IWL_USER_FW_IMAGE_IDX_WOWLAN:
			iwl_idx = IWL_UCODE_WOWLAN;
			break;
		}
		/* TODO: Calculate PHY config according to device values */
		fw_calib_cmd_cfg[iwl_idx].phy_cfg =
			cpu_to_le32(xvt->fw->phy_config);

		/*
		 * If a cfg_mask bit is unset, take the default value
		 * from the FW. Otherwise, take the value from sw_cfg.
		 */
		fw_calib_event = xvt->fw->default_calib[iwl_idx].event_trigger;
		event_override =
			 cpu_to_le32(sw_cfg->calib_ctrl[usr_idx].event_trigger);

		fw_calib_cmd_cfg[iwl_idx].calib_control.event_trigger =
			(~cfg_mask & fw_calib_event) |
			(cfg_mask & event_override);

		fw_calib_flow = xvt->fw->default_calib[iwl_idx].flow_trigger;
		flow_override =
			cpu_to_le32(sw_cfg->calib_ctrl[usr_idx].flow_trigger);

		fw_calib_cmd_cfg[iwl_idx].calib_control.flow_trigger =
			(~cfg_mask & fw_calib_flow) |
			(cfg_mask & flow_override);
	}

	return 0;
}

static int iwl_xvt_get_sw_config(struct iwl_xvt *xvt,
				 struct iwl_tm_data *data_in,
				 struct iwl_tm_data *data_out)
{
	struct iwl_xvt_sw_cfg_request *get_cfg_req;
	struct iwl_xvt_sw_cfg_request *sw_cfg;
	struct iwl_phy_cfg_cmd_v3 *fw_calib_cmd_cfg =
				xvt->sw_stack_cfg.fw_calib_cmd_cfg;
	__le32 event_trigger, flow_trigger;
	int i, u;

	if (data_in->len < sizeof(struct iwl_xvt_sw_cfg_request))
		return -EINVAL;

	get_cfg_req = data_in->data;
	sw_cfg = kzalloc(sizeof(*sw_cfg), GFP_KERNEL);
	if (!sw_cfg)
		return -ENOMEM;

	sw_cfg->load_mask = xvt->sw_stack_cfg.load_mask;
	sw_cfg->phy_config = xvt->fw->phy_config;
	sw_cfg->cfg_mask = xvt->sw_stack_cfg.calib_override_mask;
	sw_cfg->dbg_flags = xvt->sw_stack_cfg.fw_dbg_flags;
	for (i = 0; i < IWL_UCODE_TYPE_MAX; i++) {
		switch (i) {
		case IWL_UCODE_INIT:
			u = IWL_USER_FW_IMAGE_IDX_INIT;
			break;
		case IWL_UCODE_REGULAR:
			u = IWL_USER_FW_IMAGE_IDX_REGULAR;
			break;
		case IWL_UCODE_WOWLAN:
			u = IWL_USER_FW_IMAGE_IDX_WOWLAN;
			break;
		case IWL_UCODE_REGULAR_USNIFFER:
			continue;
		}
		if (get_cfg_req->get_calib_type == IWL_XVT_GET_CALIB_TYPE_DEF) {
			event_trigger =
				xvt->fw->default_calib[i].event_trigger;
			flow_trigger =
				xvt->fw->default_calib[i].flow_trigger;
		} else {
			event_trigger =
				fw_calib_cmd_cfg[i].calib_control.event_trigger;
			flow_trigger =
				fw_calib_cmd_cfg[i].calib_control.flow_trigger;
		}
		sw_cfg->calib_ctrl[u].event_trigger =
			le32_to_cpu(event_trigger);
		sw_cfg->calib_ctrl[u].flow_trigger =
			le32_to_cpu(flow_trigger);
	}

	data_out->data = sw_cfg;
	data_out->len = sizeof(*sw_cfg);
	return 0;
}

static int iwl_xvt_send_phy_cfg_cmd(struct iwl_xvt *xvt, u32 ucode_type)
{
	struct iwl_phy_cfg_cmd_v3 *calib_cmd_cfg =
		&xvt->sw_stack_cfg.fw_calib_cmd_cfg[ucode_type];
	int err;
	size_t cmd_size;

	IWL_DEBUG_INFO(xvt, "Sending Phy CFG command: 0x%x\n",
		       calib_cmd_cfg->phy_cfg);

	/* ESL workaround - calibration is not allowed */
	if (CPTCFG_IWL_TIMEOUT_FACTOR > 20) {
		calib_cmd_cfg->calib_control.event_trigger = 0;
		calib_cmd_cfg->calib_control.flow_trigger = 0;
	}
	cmd_size = iwl_fw_lookup_cmd_ver(xvt->fw, IWL_ALWAYS_LONG_GROUP,
					 PHY_CONFIGURATION_CMD,
					 IWL_FW_CMD_VER_UNKNOWN) == 3 ?
					    sizeof(struct iwl_phy_cfg_cmd_v3) :
					    sizeof(struct iwl_phy_cfg_cmd_v1);

	/* Sending calibration configuration control data */
	err = iwl_xvt_send_cmd_pdu(xvt, PHY_CONFIGURATION_CMD, 0,
				   cmd_size, calib_cmd_cfg);
	if (err)
		IWL_ERR(xvt, "Error (%d) running INIT calibrations control\n",
			err);

	return err;
}

static int iwl_xvt_continue_init_unified(struct iwl_xvt *xvt)
{
	struct iwl_nvm_access_complete_cmd nvm_complete = {};
	struct iwl_notification_wait init_complete_wait;
	static const u16 init_complete[] = { INIT_COMPLETE_NOTIF };
	int err, ret;

	err = iwl_xvt_send_cmd_pdu(xvt,
				   WIDE_ID(REGULATORY_AND_NVM_GROUP,
					   NVM_ACCESS_COMPLETE), 0,
				   sizeof(nvm_complete), &nvm_complete);
	if (err)
		goto init_error;

	xvt->state = IWL_XVT_STATE_OPERATIONAL;

	iwl_init_notification_wait(&xvt->notif_wait,
				   &init_complete_wait,
				   init_complete,
				   sizeof(init_complete),
				   NULL,
				   NULL);

	err = iwl_xvt_send_phy_cfg_cmd(xvt, IWL_UCODE_REGULAR);
	if (err) {
		iwl_remove_notification(&xvt->notif_wait, &init_complete_wait);
		goto init_error;
	}

	err = iwl_wait_notification(&xvt->notif_wait, &init_complete_wait,
				    XVT_UCODE_CALIB_TIMEOUT);
	if (err)
		goto init_error;

	ret = iwl_xvt_init_sar_tables(xvt);
	if (ret < 0) {
		err = ret;
		goto init_error;
	}

	return err;
init_error:
	xvt->state = IWL_XVT_STATE_UNINITIALIZED;
	iwl_fw_dbg_stop_sync(&xvt->fwrt);
	iwl_trans_stop_device(xvt->trans);
	return err;
}

static int iwl_xvt_run_runtime_fw(struct iwl_xvt *xvt)
{
	int err;

	err = iwl_xvt_run_fw(xvt, IWL_UCODE_REGULAR);
	if (err)
		goto fw_error;

	xvt->state = IWL_XVT_STATE_OPERATIONAL;

	if (iwl_xvt_is_unified_fw(xvt)) {
		err = iwl_xvt_nvm_init(xvt);
		if (err) {
			IWL_ERR(xvt, "Failed to read NVM: %d\n", err);
			return err;
		}
		return iwl_xvt_continue_init_unified(xvt);
	}

	/* Send phy db control command and then phy db calibration*/
	err = iwl_send_phy_db_data(xvt->phy_db);
	if (err)
		goto phy_error;

	err = iwl_xvt_send_phy_cfg_cmd(xvt, IWL_UCODE_REGULAR);
	if (err)
		goto phy_error;

	return 0;

phy_error:
	iwl_fw_dbg_stop_sync(&xvt->fwrt);
	iwl_trans_stop_device(xvt->trans);

fw_error:
	xvt->state = IWL_XVT_STATE_UNINITIALIZED;

	return err;
}

static bool iwl_xvt_wait_phy_db_entry(struct iwl_notif_wait_data *notif_wait,
				  struct iwl_rx_packet *pkt, void *data)
{
	struct iwl_phy_db *phy_db = data;

	if (pkt->hdr.cmd != CALIB_RES_NOTIF_PHY_DB) {
		WARN_ON(pkt->hdr.cmd != INIT_COMPLETE_NOTIF);
		return true;
	}

	WARN_ON(iwl_phy_db_set_section(phy_db, pkt));

	return false;
}

/*
 * iwl_xvt_start_op_mode starts FW according to load mask,
 * waits for alive notification from device, and sends it
 * to user.
 */
static int iwl_xvt_start_op_mode(struct iwl_xvt *xvt)
{
	int err = 0;
	u32 ucode_type = IWL_UCODE_INIT;

	/*
	 * If init FW and runtime FW are both enabled,
	 * Runtime FW will be executed after "continue
	 * initialization" is done.
	 * If init FW is disabled and runtime FW is
	 * enabled, run Runtime FW. If runtime fw is
	 * disabled, do nothing.
	 */
	if (!(xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_INIT)) {
		if (xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_RUNTIME) {
			err = iwl_xvt_run_runtime_fw(xvt);
		} else {
			if (xvt->state != IWL_XVT_STATE_UNINITIALIZED) {
				xvt->fw_running = false;
				iwl_fw_dbg_stop_sync(&xvt->fwrt);
				iwl_trans_stop_device(xvt->trans);
			}
			err = iwl_trans_start_hw(xvt->trans);
			if (err) {
				IWL_ERR(xvt, "Failed to start HW\n");
			} else {
				iwl_write32(xvt->trans, CSR_RESET, 0);
				xvt->state = IWL_XVT_STATE_NO_FW;
			}
		}

		return err;
	}

	/* when fw image is unified, only regular ucode is loaded. */
	if (iwl_xvt_is_unified_fw(xvt))
		ucode_type = IWL_UCODE_REGULAR;
	err = iwl_xvt_run_fw(xvt, ucode_type);
	if (err)
		return err;

	xvt->state = IWL_XVT_STATE_INIT_STARTED;

	err = iwl_xvt_nvm_init(xvt);
	if (err)
		IWL_ERR(xvt, "Failed to read NVM: %d\n", err);

	/*
	 * The initialization flow is not yet complete.
	 * User need to execute "Continue initialization"
	 * flow in order to complete it.
	 *
	 * NOT sending ALIVE notification to user. User
	 * knows that FW is alive when "start op mode"
	 * returns without errors.
	 */

	return err;
}

static void iwl_xvt_stop_op_mode(struct iwl_xvt *xvt)
{
	if (xvt->state == IWL_XVT_STATE_UNINITIALIZED)
		return;

	if (xvt->fw_running) {
		iwl_xvt_txq_disable(xvt);
		xvt->fw_running = false;
	}
	iwl_fw_dbg_stop_sync(&xvt->fwrt);
	iwl_trans_stop_device(xvt->trans);

	iwl_free_fw_paging(&xvt->fwrt);

	xvt->state = IWL_XVT_STATE_UNINITIALIZED;
}

/*
 * iwl_xvt_continue_init get phy calibrations data from
 * device and stores them. It also runs runtime FW if it
 * is marked in the load mask.
 */
static int iwl_xvt_continue_init(struct iwl_xvt *xvt)
{
	struct iwl_notification_wait calib_wait;
	static const u16 init_complete[] = {
		INIT_COMPLETE_NOTIF,
		CALIB_RES_NOTIF_PHY_DB
	};
	int err, ret;

	if (xvt->state != IWL_XVT_STATE_INIT_STARTED)
		return -EINVAL;

	if (iwl_xvt_is_unified_fw(xvt))
		return iwl_xvt_continue_init_unified(xvt);

	iwl_init_notification_wait(&xvt->notif_wait,
				   &calib_wait,
				   init_complete,
				   ARRAY_SIZE(init_complete),
				   iwl_xvt_wait_phy_db_entry,
				   xvt->phy_db);

	err = iwl_xvt_send_phy_cfg_cmd(xvt, IWL_UCODE_INIT);
	if (err) {
		iwl_remove_notification(&xvt->notif_wait, &calib_wait);
		goto error;
	}

	/*
	 * Waiting for the calibration complete notification
	 * iwl_xvt_wait_phy_db_entry will store the calibrations
	 */
	err = iwl_wait_notification(&xvt->notif_wait, &calib_wait,
				    XVT_UCODE_CALIB_TIMEOUT);
	if (err)
		goto error;

	xvt->state = IWL_XVT_STATE_OPERATIONAL;

	iwl_dbg_tlv_time_point(&xvt->fwrt, IWL_FW_INI_TIME_POINT_POST_INIT,
			       NULL);
	iwl_dbg_tlv_time_point(&xvt->fwrt, IWL_FW_INI_TIME_POINT_PERIODIC,
			       NULL);

	if (xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_RUNTIME)
		/* Run runtime FW stops the device by itself if error occurs */
		err = iwl_xvt_run_runtime_fw(xvt);
	if (err)
		goto cont_init_end;

	ret = iwl_xvt_init_sar_tables(xvt);
	if (ret < 0) {
		err = ret;
		goto error;
	}

	goto cont_init_end;

error:
	xvt->state = IWL_XVT_STATE_UNINITIALIZED;
	iwl_xvt_txq_disable(xvt);
	iwl_fw_dbg_stop_sync(&xvt->fwrt);
	iwl_trans_stop_device(xvt->trans);

cont_init_end:

	return err;
}

static int iwl_xvt_get_phy_db(struct iwl_xvt *xvt,
			      struct iwl_tm_data *data_in,
			      struct iwl_tm_data *data_out)
{
	struct iwl_xvt_phy_db_request *phy_db_req =
				(struct iwl_xvt_phy_db_request *)data_in->data;
	struct iwl_xvt_phy_db_request *phy_db_resp;
	u8 *phy_data;
	u16 phy_size;
	u32 resp_size;
	int err;

	if ((data_in->len < sizeof(struct iwl_xvt_phy_db_request)) ||
	    (phy_db_req->size != 0))
		return -EINVAL;

	err = iwl_phy_db_get_section_data(xvt->phy_db,
					  phy_db_req->type,
					  &phy_data, &phy_size,
					  phy_db_req->chg_id);
	if (err)
		return err;

	resp_size = sizeof(*phy_db_resp) + phy_size;
	phy_db_resp = kzalloc(resp_size, GFP_KERNEL);
	if (!phy_db_resp)
		return -ENOMEM;
	phy_db_resp->chg_id = phy_db_req->chg_id;
	phy_db_resp->type = phy_db_req->type;
	phy_db_resp->size = phy_size;
	memcpy(phy_db_resp->data, phy_data, phy_size);

	data_out->data = phy_db_resp;
	data_out->len = resp_size;

	return 0;
}

static struct iwl_device_tx_cmd *iwl_xvt_init_tx_dev_cmd(struct iwl_xvt *xvt)
{
	struct iwl_device_tx_cmd *dev_cmd;

	dev_cmd = iwl_trans_alloc_tx_cmd(xvt->trans);
	if (unlikely(!dev_cmd))
		return NULL;

	dev_cmd->hdr.cmd = TX_CMD;

	return dev_cmd;
}

static u16 iwl_xvt_get_offload_assist(struct ieee80211_hdr *hdr)
{
	int hdrlen = ieee80211_hdrlen(hdr->frame_control);
	u16 offload_assist = 0;
	bool amsdu;

	amsdu = ieee80211_is_data_qos(hdr->frame_control) &&
		(*ieee80211_get_qos_ctl(hdr) &
		 IEEE80211_QOS_CTL_A_MSDU_PRESENT);

	if (amsdu)
		offload_assist |= BIT(TX_CMD_OFFLD_AMSDU);

	/*
	* padding is inserted later in transport.
	* do not align A-MSDUs to dword, as the subframe header
	* aligns the SNAP header.
	*/
	if (hdrlen % 4 && !amsdu)
		offload_assist |= BIT(TX_CMD_OFFLD_PAD);

	return offload_assist;
}

static struct iwl_device_tx_cmd *
iwl_xvt_set_tx_params_gen3(struct iwl_xvt *xvt, struct sk_buff *skb,
			   u32 rate_flags, u32 tx_flags)

{
	struct iwl_device_tx_cmd *dev_cmd;
	struct iwl_tx_cmd_gen3 *cmd;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct iwl_xvt_skb_info *skb_info = (void *)skb->cb;
	u32 header_length = ieee80211_hdrlen(hdr->frame_control);

	dev_cmd = iwl_xvt_init_tx_dev_cmd(xvt);
	if (unlikely(!dev_cmd))
		return NULL;

	cmd = (struct iwl_tx_cmd_gen3 *)dev_cmd->payload;

	cmd->offload_assist |= cpu_to_le32(iwl_xvt_get_offload_assist(hdr));

	cmd->len = cpu_to_le16((u16)skb->len);

	cmd->flags = cpu_to_le16(tx_flags);
	if (ieee80211_has_morefrags(hdr->frame_control))
		/* though this flag is not supported for gen3, it is used
		 * here for silicon feedback tests. */
		cmd->flags |= cpu_to_le16(TX_CMD_FLG_MORE_FRAG);

	cmd->rate_n_flags =  cpu_to_le32(rate_flags);

	/* Copy MAC header from skb into command buffer */
	memcpy(cmd->hdr, hdr, header_length);

	 /* Saving device command address itself in the control buffer, to be
	  * used when reclaiming the command.
	  */
	skb_info->dev_cmd = dev_cmd;

	return dev_cmd;
}

static struct iwl_device_tx_cmd *
iwl_xvt_set_tx_params_gen2(struct iwl_xvt *xvt, struct sk_buff *skb,
			   u32 rate_flags, u32 flags)
{
	struct iwl_device_tx_cmd *dev_cmd;
	struct iwl_xvt_skb_info *skb_info = (void *)skb->cb;
	struct iwl_tx_cmd_gen2 *tx_cmd;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	u32 header_length = ieee80211_hdrlen(hdr->frame_control);

	dev_cmd = iwl_xvt_init_tx_dev_cmd(xvt);
	if (unlikely(!dev_cmd))
		return NULL;

	tx_cmd = (struct iwl_tx_cmd_gen2 *)dev_cmd->payload;
	tx_cmd->len = cpu_to_le16((u16)skb->len);
	tx_cmd->offload_assist |= cpu_to_le16(iwl_xvt_get_offload_assist(hdr));
	tx_cmd->flags = cpu_to_le32(flags);
	if (ieee80211_has_morefrags(hdr->frame_control))
		/* though this flag is not supported for gen2, it is used
		 * for silicon feedback tests. */
		tx_cmd->flags |= cpu_to_le32(TX_CMD_FLG_MORE_FRAG);
	tx_cmd->rate_n_flags = cpu_to_le32(rate_flags);

	/* Copy MAC header from skb into command buffer */
	memcpy(tx_cmd->hdr, hdr, header_length);

	 /* Saving device command address itself in the
	  * control buffer, to be used when reclaiming
	  * the command. */
	skb_info->dev_cmd = dev_cmd;

	return dev_cmd;
}

/*
 * Allocates and sets the Tx cmd the driver data pointers in the skb
 */
static struct iwl_device_tx_cmd *
iwl_xvt_set_mod_tx_params(struct iwl_xvt *xvt, struct sk_buff *skb,
			  u8 sta_id, u32 rate_flags, u32 flags)
{
	struct iwl_device_tx_cmd *dev_cmd;
	struct iwl_xvt_skb_info *skb_info = (void *)skb->cb;
	struct iwl_tx_cmd *tx_cmd;

	dev_cmd = iwl_xvt_init_tx_dev_cmd(xvt);
	if (unlikely(!dev_cmd))
		return NULL;

	tx_cmd = (struct iwl_tx_cmd *)dev_cmd->payload;

	tx_cmd->len = cpu_to_le16((u16)skb->len);
	tx_cmd->life_time = cpu_to_le32(TX_CMD_LIFE_TIME_INFINITE);

	tx_cmd->sta_id = sta_id;
	tx_cmd->rate_n_flags = cpu_to_le32(rate_flags);
	tx_cmd->tx_flags = cpu_to_le32(flags);

	/* the skb should already hold the data */
	memcpy(tx_cmd->hdr, skb->data, sizeof(struct ieee80211_hdr));

	/*
	 * Saving device command address itself in the
	 * control buffer, to be used when reclaiming
	 * the command.
	 */
	skb_info->dev_cmd = dev_cmd;

	return dev_cmd;
}

static void iwl_xvt_set_seq_number(struct iwl_xvt *xvt,
				   struct tx_meta_data *meta_tx,
				   struct sk_buff *skb,
				   u8 frag_num)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	u8 *qc, tid;

	if (!ieee80211_is_data_qos(hdr->frame_control) ||
	    is_multicast_ether_addr(hdr->addr1))
		return;

	qc = ieee80211_get_qos_ctl(hdr);
	tid = *qc & IEEE80211_QOS_CTL_TID_MASK;
	if (WARN_ON(tid >= IWL_MAX_TID_COUNT))
		tid = IWL_MAX_TID_COUNT - 1;

	/* frag_num is expected to be zero in case of no fragmentation */
	hdr->seq_ctrl = cpu_to_le16(meta_tx->seq_num[tid] |
				    (frag_num & IEEE80211_SCTL_FRAG));

	if (!ieee80211_has_morefrags(hdr->frame_control))
		meta_tx->seq_num[tid] += 0x10;
}

static int iwl_xvt_send_packet(struct iwl_xvt *xvt,
			       struct iwl_tm_mod_tx_request *tx_req,
			       u32 *status, struct tx_meta_data *meta_tx)
{
	struct sk_buff *skb;
	struct iwl_device_tx_cmd *dev_cmd;
	int time_remain, err = 0;
	u32 flags = 0;
	u32 rate_flags = tx_req->rate_flags;

	if (xvt->fw_error) {
		IWL_ERR(xvt, "FW Error while sending Tx\n");
		*status = XVT_TX_DRIVER_ABORTED;
		return -ENODEV;
	}

	skb = alloc_skb(tx_req->len, GFP_KERNEL);
	if (!skb) {
		*status = XVT_TX_DRIVER_ABORTED;
		return -ENOMEM;
	}

	memcpy(skb_put(skb, tx_req->len), tx_req->data, tx_req->len);
	iwl_xvt_set_seq_number(xvt, meta_tx, skb, 0);

	flags = tx_req->no_ack ? 0 : TX_CMD_FLG_ACK;

	if (iwl_xvt_is_unified_fw(xvt)) {
		flags |= IWL_TX_FLAGS_CMD_RATE;

		if (xvt->trans->trans_cfg->device_family >=
		    IWL_DEVICE_FAMILY_AX210)
			dev_cmd = iwl_xvt_set_tx_params_gen3(xvt, skb,
							     rate_flags,
							     flags);
		else
			dev_cmd = iwl_xvt_set_tx_params_gen2(xvt, skb,
							     rate_flags,
							     flags);
	} else {
		dev_cmd = iwl_xvt_set_mod_tx_params(xvt,
						    skb,
						    tx_req->sta_id,
						    tx_req->rate_flags,
						    flags);
	}
	if (!dev_cmd) {
		kfree_skb(skb);
		*status = XVT_TX_DRIVER_ABORTED;
		return -ENOMEM;
	}

	if (tx_req->trigger_led)
		iwl_xvt_led_enable(xvt);

	/* wait until the tx queue isn't full */
	time_remain = wait_event_interruptible_timeout(meta_tx->mod_tx_wq,
						       !meta_tx->txq_full, HZ);

	if (time_remain <= 0) {
		/* This should really not happen */
		WARN_ON_ONCE(meta_tx->txq_full);
		IWL_ERR(xvt, "Error while sending Tx\n");
		*status = XVT_TX_DRIVER_QUEUE_FULL;
		err = -EIO;
		goto err;
	}

	if (xvt->fw_error) {
		WARN_ON_ONCE(meta_tx->txq_full);
		IWL_ERR(xvt, "FW Error while sending Tx\n");
		*status = XVT_TX_DRIVER_ABORTED;
		err = -ENODEV;
		goto err;
	}

	/* Assume we have one Txing thread only: the queue is not full
	 * any more - nobody could fill it up in the meantime since we
	 * were blocked.
	 */

	local_bh_disable();

	err = iwl_trans_tx(xvt->trans, skb, dev_cmd, meta_tx->queue);

	local_bh_enable();
	if (err) {
		IWL_ERR(xvt, "Tx command failed (error %d)\n", err);
		*status = XVT_TX_DRIVER_ABORTED;
		goto err;
	}

	if (tx_req->trigger_led)
		iwl_xvt_led_disable(xvt);

	return err;
err:
	iwl_trans_free_tx_cmd(xvt->trans, dev_cmd);
	kfree_skb(skb);
	return err;
}

static struct iwl_device_tx_cmd *
iwl_xvt_set_tx_params(struct iwl_xvt *xvt, struct sk_buff *skb,
		      struct iwl_xvt_tx_start *tx_start, u8 packet_index)
{
	struct iwl_device_tx_cmd *dev_cmd;
	struct iwl_xvt_skb_info *skb_info = (void *)skb->cb;
	struct iwl_tx_cmd *tx_cmd;
	/* the skb should already hold the data */
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	u32 header_length = ieee80211_hdrlen(hdr->frame_control);

	dev_cmd = iwl_xvt_init_tx_dev_cmd(xvt);
	if (unlikely(!dev_cmd))
		return NULL;

	tx_cmd = (struct iwl_tx_cmd *)dev_cmd->payload;

	/* let the fw manage the seq number for non-qos/multicast */
	if (!ieee80211_is_data_qos(hdr->frame_control) ||
	    is_multicast_ether_addr(hdr->addr1))
		tx_cmd->tx_flags |= cpu_to_le32(TX_CMD_FLG_SEQ_CTL);

	tx_cmd->len = cpu_to_le16((u16)skb->len);
	tx_cmd->offload_assist |= cpu_to_le16(iwl_xvt_get_offload_assist(hdr));
	tx_cmd->tx_flags |= cpu_to_le32(tx_start->tx_data.tx_flags);
	if (ieee80211_has_morefrags(hdr->frame_control))
		tx_cmd->tx_flags |= cpu_to_le32(TX_CMD_FLG_MORE_FRAG);
	tx_cmd->rate_n_flags = cpu_to_le32(tx_start->tx_data.rate_flags);
	tx_cmd->sta_id = tx_start->frames_data[packet_index].sta_id;
	tx_cmd->sec_ctl = tx_start->frames_data[packet_index].sec_ctl;
	tx_cmd->initial_rate_index = tx_start->tx_data.initial_rate_index;
	tx_cmd->life_time = cpu_to_le32(TX_CMD_LIFE_TIME_INFINITE);
	tx_cmd->rts_retry_limit = tx_start->tx_data.rts_retry_limit;
	tx_cmd->data_retry_limit = tx_start->tx_data.data_retry_limit;
	tx_cmd->tid_tspec = tx_start->frames_data[packet_index].tid_tspec;
	memcpy(tx_cmd->key,
	       tx_start->frames_data[packet_index].key,
	       sizeof(tx_cmd->key));

	memcpy(tx_cmd->hdr, hdr, header_length);

	/*
	 * Saving device command address itself in the control buffer,
	 * to be used when reclaiming the command.
	 */
	skb_info->dev_cmd = dev_cmd;

	return dev_cmd;
}

static struct sk_buff *iwl_xvt_set_skb(struct iwl_xvt *xvt,
				       struct ieee80211_hdr *hdr,
				       struct tx_payload *payload)
{
	struct sk_buff *skb;
	u32 header_size = ieee80211_hdrlen(hdr->frame_control);
	u32 payload_length  = payload->length;
	u32 packet_length  = payload_length + header_size;

	skb = alloc_skb(packet_length, GFP_KERNEL);
	if (!skb)
		return NULL;
	/* copy MAC header into skb */
	memcpy(skb_put(skb, header_size), hdr, header_size);
	/* copy frame payload into skb */
	memcpy(skb_put(skb, payload_length), payload->payload, payload_length);

	return skb;
}

static struct sk_buff *iwl_xvt_create_fragment_skb(struct iwl_xvt *xvt,
						   struct ieee80211_hdr *hdr,
						   struct tx_payload *payload,
						   u32 fragment_size,
						   u8 frag_num)
{
	struct sk_buff *skb;
	const __le16 morefrags = cpu_to_le16(IEEE80211_FCTL_MOREFRAGS);
	u32 header_size = ieee80211_hdrlen(hdr->frame_control);
	u32 skb_size, offset, payload_remain, payload_chunck_size;

	if (WARN(fragment_size <= header_size ||
		 !ieee80211_is_data_qos(hdr->frame_control),
		 "can't fragment, fragment_size small big or not qos data"))
		return NULL;

	payload_chunck_size = fragment_size - header_size;
	offset = payload_chunck_size * frag_num;
	if (WARN(offset >= payload->length, "invalid fragment number %d\n",
		 frag_num))
		return NULL;

	payload_remain = payload->length - offset;

	if (fragment_size < payload_remain + header_size) {
		skb_size = fragment_size;
		hdr->frame_control |= morefrags;
	} else {
		skb_size = payload_remain + header_size;
		hdr->frame_control &= ~morefrags;
		payload_chunck_size = payload_remain;
	}

	skb = alloc_skb(skb_size, GFP_KERNEL);
	if (!skb)
		return NULL;

	/* copy MAC header into skb */
	memcpy(skb_put(skb, header_size), hdr, header_size);

	/* copy frame payload into skb */
	memcpy(skb_put(skb, payload_chunck_size),
	       &payload->payload[offset],
	       payload_chunck_size);

	return skb;
}

static struct sk_buff *iwl_xvt_get_skb(struct iwl_xvt *xvt,
				       struct ieee80211_hdr *hdr,
				       struct tx_payload *payload,
				       u32 fragment_size,
				       u8 frag_num)
{
	if (fragment_size == 0)/* no framgmentation */
		return iwl_xvt_set_skb(xvt, hdr, payload);

	return iwl_xvt_create_fragment_skb(xvt, hdr, payload,
					   fragment_size, frag_num);
}

static int iwl_xvt_transmit_packet(struct iwl_xvt *xvt,
				   struct sk_buff *skb,
				   struct iwl_xvt_tx_start *tx_start,
				   u8 packet_index,
				   u8 frag_num,
				   u32 *status)
{
	struct iwl_device_tx_cmd *dev_cmd;
	int time_remain, err = 0;
	u8 queue = tx_start->frames_data[packet_index].queue;
	struct tx_queue_data *queue_data = &xvt->queue_data[queue];
	u32 rate_flags = tx_start->tx_data.rate_flags;
	u32 tx_flags = tx_start->tx_data.tx_flags;

	/* set tx number */
	iwl_xvt_set_seq_number(xvt, &xvt->tx_meta_data[XVT_LMAC_0_ID], skb,
			       frag_num);

	if (iwl_xvt_is_unified_fw(xvt)) {
		if (xvt->trans->trans_cfg->device_family >=
		    IWL_DEVICE_FAMILY_AX210)
			dev_cmd = iwl_xvt_set_tx_params_gen3(xvt, skb,
							     rate_flags,
							     tx_flags);
		else
			dev_cmd = iwl_xvt_set_tx_params_gen2(xvt, skb,
							     rate_flags,
							     tx_flags);
	} else {
		dev_cmd = iwl_xvt_set_tx_params(xvt, skb, tx_start,
						packet_index);
	}
	if (!dev_cmd) {
		kfree_skb(skb);
		*status = XVT_TX_DRIVER_ABORTED;
		return -ENOMEM;
	}
	/* wait until the tx queue isn't full */
	time_remain = wait_event_interruptible_timeout(queue_data->tx_wq,
						       !queue_data->txq_full,
						       HZ);

	if (time_remain <= 0) {
		/* This should really not happen */
		WARN_ON_ONCE(queue_data->txq_full);
		IWL_ERR(xvt, "Error while sending Tx - queue full\n");
		*status = XVT_TX_DRIVER_QUEUE_FULL;
		err = -EIO;
		goto on_err;
	}

	if (xvt->fw_error) {
		WARN_ON_ONCE(queue_data->txq_full);
		IWL_ERR(xvt, "FW Error while sending packet\n");
		*status = XVT_TX_DRIVER_ABORTED;
		err = -ENODEV;
		goto on_err;
	}
	/* Assume we have one Txing thread only: the queue is not full
	 * any more - nobody could fill it up in the meantime since we
	 * were blocked.
	 */
	local_bh_disable();
	err = iwl_trans_tx(xvt->trans, skb, dev_cmd, queue);
	local_bh_enable();
	if (err) {
		IWL_ERR(xvt, "Tx command failed (error %d)\n", err);
		*status = XVT_TX_DRIVER_ABORTED;
		goto on_err;
	}

	return 0;

on_err:
	iwl_trans_free_tx_cmd(xvt->trans, dev_cmd);
	kfree_skb(skb);
	return err;
}

static int iwl_xvt_send_tx_done_notif(struct iwl_xvt *xvt, u32 status)
{
	struct iwl_xvt_tx_done *done_notif;
	u32 i, j, done_notif_size, num_of_queues = 0;
	int err;

	for (i = 1; i < IWL_MAX_HW_QUEUES; i++) {
		if (xvt->queue_data[i].allocated_queue)
			num_of_queues++;
	}

	done_notif_size = sizeof(*done_notif) +
		num_of_queues * sizeof(struct iwl_xvt_post_tx_data);
	done_notif = kzalloc(done_notif_size, GFP_KERNEL);
	if (!done_notif)
		return -ENOMEM;

	done_notif->status = status;
	done_notif->num_of_queues = num_of_queues;

	for (i = 1, j = 0; i <= num_of_queues; i++) {
		if (!xvt->queue_data[i].allocated_queue)
			continue;
		done_notif->tx_data[j].num_of_packets =
			xvt->queue_data[i].tx_counter;
		done_notif->tx_data[j].queue = i;
		j++;
	}
	err = iwl_xvt_user_send_notif(xvt,
				      IWL_XVT_CMD_ENHANCED_TX_DONE,
				      (void *)done_notif,
				      done_notif_size, GFP_ATOMIC);
	if (err)
		IWL_ERR(xvt, "Error %d sending tx_done notification\n", err);
	kfree(done_notif);
	return err;
}

static int iwl_xvt_start_tx_handler(void *data)
{
	struct iwl_xvt_enhanced_tx_data *task_data = data;
	struct iwl_xvt_tx_start *tx_start = &task_data->tx_start_data;
	struct iwl_xvt *xvt = task_data->xvt;
	u8 num_of_frames;
	u32 status, packets_in_cycle = 0;
	int time_remain, err = 0, sent_packets = 0;
	u32 num_of_cycles = tx_start->num_of_cycles;
	u64 i, num_of_iterations;

	/* reset tx parameters */
	xvt->num_of_tx_resp = 0;
	xvt->send_tx_resp = tx_start->send_tx_resp;
	status = 0;

	for (i = 0; i < IWL_MAX_HW_QUEUES; i++)
		xvt->queue_data[i].tx_counter = 0;

	num_of_frames = tx_start->num_of_different_frames;
	for (i = 0; i < num_of_frames; i++)
		packets_in_cycle += tx_start->frames_data[i].times;
	if (WARN(packets_in_cycle == 0, "invalid packets amount to send"))
		return -EINVAL;

	if (num_of_cycles == IWL_XVT_TX_MODULATED_INFINITE)
		num_of_cycles = XVT_MAX_TX_COUNT / packets_in_cycle;
	xvt->expected_tx_amount = packets_in_cycle * num_of_cycles;
	num_of_iterations = num_of_cycles * num_of_frames;

	for (i = 0; (i < num_of_iterations) && !kthread_should_stop(); i++) {
		u16 j, times;
		u8 frame_index, payload_idx, frag_idx, frag_num;
		struct ieee80211_hdr *hdr;
		struct sk_buff *skb;
		u8 frag_size = tx_start->tx_data.fragment_size;
		struct tx_payload *payload;
		u8 frag_array_size = ARRAY_SIZE(tx_start->tx_data.frag_num);

		frame_index = i % num_of_frames;
		payload_idx = tx_start->frames_data[frame_index].payload_index;
		payload = xvt->payloads[payload_idx];
		hdr = (struct ieee80211_hdr *)
			tx_start->frames_data[frame_index].header;
		times = tx_start->frames_data[frame_index].times;
		for (j = 0; j < times; j++) {
			if (xvt->fw_error) {
				IWL_ERR(xvt, "FW Error during TX\n");
				status = XVT_TX_DRIVER_ABORTED;
				err = -ENODEV;
				goto on_exit;
			}

			frag_idx = 0;
			while (frag_idx <  frag_array_size) {
				frag_num = tx_start->tx_data.frag_num[frag_idx];

				if (frag_num == XVT_STOP_TX ||
				    (frag_size == 0 && frag_idx > 0))
					break;

				skb = iwl_xvt_get_skb(xvt, hdr, payload,
						      frag_size, frag_num);
				if (!skb) {
					IWL_ERR(xvt, "skb is NULL\n");
					status = XVT_TX_DRIVER_ABORTED;
					err = -ENOMEM;
					goto on_exit;
				}
				err = iwl_xvt_transmit_packet(xvt,
							      skb,
							      tx_start,
							      frame_index,
							      frag_num,
							      &status);
				if (err) {
					IWL_ERR(xvt, "stop due to err %d\n",
						err);
					goto on_exit;
				}

				sent_packets++;
				++frag_idx;
			}
		}
	}

on_exit:
	if (sent_packets > 0 && !xvt->fw_error) {
		time_remain = wait_event_interruptible_timeout(xvt->tx_done_wq,
					xvt->num_of_tx_resp == sent_packets,
					5 * HZ * CPTCFG_IWL_TIMEOUT_FACTOR);
		if (time_remain <= 0) {
			IWL_ERR(xvt, "Not all Tx messages were sent\n");
			if (status == 0)
				status = XVT_TX_DRIVER_TIMEOUT;
		}
	}

	err = iwl_xvt_send_tx_done_notif(xvt, status);

	xvt->is_enhanced_tx = false;
	kfree(data);
	for (i = 0; i < IWL_XVT_MAX_PAYLOADS_AMOUNT; i++) {
		kfree(xvt->payloads[i]);
		xvt->payloads[i] = NULL;
	}
	do_exit(err);
}

static int iwl_xvt_modulated_tx_handler(void *data)
{
	u64 tx_count, max_tx;
	int time_remain, num_of_packets, err = 0;
	struct iwl_xvt *xvt;
	struct iwl_xvt_tx_mod_done *done_notif;
	u32 status = XVT_TX_DRIVER_SUCCESSFUL;
	struct iwl_xvt_tx_mod_task_data *task_data =
		(struct iwl_xvt_tx_mod_task_data *)data;
	struct tx_meta_data *xvt_tx;

	xvt = task_data->xvt;
	xvt_tx = &xvt->tx_meta_data[task_data->lmac_id];
	xvt_tx->tx_task_operating = true;
	num_of_packets = task_data->tx_req.times;
	max_tx = (num_of_packets == IWL_XVT_TX_MODULATED_INFINITE) ?
		  XVT_MAX_TX_COUNT : num_of_packets;
	xvt_tx->tot_tx = num_of_packets;
	xvt_tx->tx_counter = 0;

	for (tx_count = 0;
	    (tx_count < max_tx) && (!kthread_should_stop());
	     tx_count++){
		err = iwl_xvt_send_packet(xvt, &task_data->tx_req,
					  &status, xvt_tx);
		if (err) {
			IWL_ERR(xvt, "stop send packets due to err %d\n", err);
			break;
		}
	}

	if (!err) {
		time_remain = wait_event_interruptible_timeout(
						xvt_tx->mod_tx_done_wq,
						xvt_tx->tx_counter == tx_count,
						5 * HZ);
		if (time_remain <= 0) {
			IWL_ERR(xvt, "Not all Tx messages were sent\n");
			xvt_tx->tx_task_operating = false;
			status = XVT_TX_DRIVER_TIMEOUT;
		}
	}

	done_notif = kmalloc(sizeof(*done_notif), GFP_KERNEL);
	if (!done_notif) {
		xvt_tx->tx_task_operating = false;
		kfree(data);
		return -ENOMEM;
	}
	done_notif->num_of_packets = xvt_tx->tx_counter;
	done_notif->status = status;
	done_notif->lmac_id = task_data->lmac_id;
	err = iwl_xvt_user_send_notif(xvt,
				      IWL_XVT_CMD_SEND_MOD_TX_DONE,
				      (void *)done_notif,
				      sizeof(*done_notif), GFP_ATOMIC);
	if (err) {
		IWL_ERR(xvt, "Error %d sending tx_done notification\n", err);
		kfree(done_notif);
	}

	xvt_tx->tx_task_operating = false;
	kfree(data);
	do_exit(err);
}

static int iwl_xvt_modulated_tx_infinite_stop(struct iwl_xvt *xvt,
					      struct iwl_tm_data *data_in)
{
	int err = 0;
	u32 lmac_id = ((struct iwl_xvt_tx_mod_stop *)data_in->data)->lmac_id;
	struct tx_meta_data *xvt_tx = &xvt->tx_meta_data[lmac_id];

	if (xvt_tx->tx_mod_thread && xvt_tx->tx_task_operating) {
		err = kthread_stop(xvt_tx->tx_mod_thread);
		xvt_tx->tx_mod_thread = NULL;
	}

	return err;
}

static inline int map_sta_to_lmac(struct iwl_xvt *xvt, u8 sta_id)
{
	switch (sta_id) {
	case XVT_LMAC_0_STA_ID:
		return XVT_LMAC_0_ID;
	case XVT_LMAC_1_STA_ID:
		return XVT_LMAC_1_ID;
	default:
		IWL_ERR(xvt, "wrong sta id, can't match queue\n");
		return -EINVAL;
	}
}

static int iwl_xvt_tx_queue_cfg(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in)
{
	struct iwl_xvt_tx_queue_cfg *input =
			(struct iwl_xvt_tx_queue_cfg *)data_in->data;
	u8 sta_id = input->sta_id;
	int lmac_id = map_sta_to_lmac(xvt, sta_id);

	if (lmac_id < 0)
		return lmac_id;

	switch (input->operation) {
	case TX_QUEUE_CFG_ADD:
		return iwl_xvt_allocate_tx_queue(xvt, sta_id, lmac_id);
	case TX_QUEUE_CFG_REMOVE:
		iwl_xvt_free_tx_queue(xvt, lmac_id);
		break;
	default:
		IWL_ERR(xvt, "failed in tx config - wrong operation\n");
		return -EINVAL;
	}

	return 0;
}

static int iwl_xvt_start_tx(struct iwl_xvt *xvt,
			    struct iwl_xvt_driver_command_req *req)
{
	struct iwl_xvt_enhanced_tx_data *task_data;

	if (WARN(xvt->is_enhanced_tx ||
		 xvt->tx_meta_data[XVT_LMAC_0_ID].tx_task_operating ||
		 xvt->tx_meta_data[XVT_LMAC_1_ID].tx_task_operating,
		 "TX is already in progress\n"))
		return -EINVAL;

	xvt->is_enhanced_tx = true;

	task_data = kzalloc(sizeof(*task_data), GFP_KERNEL);
	if (!task_data) {
		xvt->is_enhanced_tx = false;
		return -ENOMEM;
	}

	task_data->xvt = xvt;
	memcpy(&task_data->tx_start_data, req->input_data,
	       sizeof(struct iwl_xvt_tx_start));

	xvt->tx_task = kthread_run(iwl_xvt_start_tx_handler,
				   task_data, "start enhanced tx command");
	if (!xvt->tx_task) {
		xvt->is_enhanced_tx = true;
		kfree(task_data);
		return -ENOMEM;
	}

	return 0;
}

static int iwl_xvt_stop_tx(struct iwl_xvt *xvt)
{
	int err = 0;

	if (xvt->tx_task && xvt->is_enhanced_tx) {
		err = kthread_stop(xvt->tx_task);
		xvt->tx_task = NULL;
	}

	return err;
}

static int iwl_xvt_set_tx_payload(struct iwl_xvt *xvt,
				  struct iwl_xvt_driver_command_req *req)
{
	struct iwl_xvt_set_tx_payload *input =
		(struct iwl_xvt_set_tx_payload *)req->input_data;
	u32 size = sizeof(struct tx_payload) + input->length;
	struct tx_payload *payload_struct;

	if (WARN(input->index >= IWL_XVT_MAX_PAYLOADS_AMOUNT,
		 "invalid payload index\n"))
		return -EINVAL;

	/* First free payload in case index is already in use */
	kfree(xvt->payloads[input->index]);

	/* Allocate payload in xvt buffer */
	xvt->payloads[input->index] = kzalloc(size, GFP_KERNEL);
	if (!xvt->payloads[input->index])
		return -ENOMEM;

	payload_struct = xvt->payloads[input->index];
	payload_struct->length = input->length;
	memcpy(payload_struct->payload, input->payload, input->length);

	return 0;
}

static int iwl_xvt_modulated_tx(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in)
{
	u32 pkt_length = ((struct iwl_tm_mod_tx_request *)data_in->data)->len;
	u32 req_length = sizeof(struct iwl_tm_mod_tx_request) + pkt_length;
	u32 task_data_length =
		sizeof(struct iwl_xvt_tx_mod_task_data) + pkt_length;
	struct tx_meta_data *xvt_tx = &xvt->tx_meta_data[XVT_LMAC_0_ID];
	u8 sta_id;
	int lmac_id;
	struct iwl_xvt_tx_mod_task_data *task_data;
	int err;

	/* Verify this command was not called while tx is operating */
	if (WARN_ON(xvt->is_enhanced_tx))
		return -EINVAL;

	task_data = kzalloc(task_data_length, GFP_KERNEL);
	if (!task_data)
		return -ENOMEM;

	/*
	* no need to check whether tx already operating on lmac, since check
	* is already done in the USC
	*/
	task_data->xvt = xvt;
	memcpy(&task_data->tx_req, data_in->data, req_length);

	if (iwl_xvt_is_unified_fw(xvt)) {
		sta_id = task_data->tx_req.sta_id;
		lmac_id = map_sta_to_lmac(xvt, sta_id);
		if (lmac_id < 0) {
			err = lmac_id;
			goto out;
		}

		task_data->lmac_id = lmac_id;
		xvt_tx = &xvt->tx_meta_data[lmac_id];

		/* check if tx queue is allocated. if not - return */
		if (xvt_tx->queue < 0) {
			IWL_ERR(xvt, "failed in tx - queue is not allocated\n");
			err = -EIO;
			goto out;
		}
	}

	xvt_tx->tx_mod_thread = kthread_run(iwl_xvt_modulated_tx_handler,
					   task_data, "tx mod infinite");
	if (!xvt_tx->tx_mod_thread) {
		xvt_tx->tx_task_operating = false;
		err = -ENOMEM;
		goto out;
	}

	return 0;
out:
	kfree(task_data);
	return err;
}

static int iwl_xvt_rx_hdrs_mode(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_xvt_rx_hdrs_mode_request *rx_hdr = data_in->data;

	if (data_in->len < sizeof(struct iwl_xvt_rx_hdrs_mode_request))
		return -EINVAL;

	if (rx_hdr->mode)
		xvt->rx_hdr_enabled = true;
	else
		xvt->rx_hdr_enabled = false;

	return 0;
}

static int iwl_xvt_apmg_pd_mode(struct iwl_xvt *xvt,
				  struct iwl_tm_data *data_in)
{
	struct iwl_xvt_apmg_pd_mode_request *apmg_pd = data_in->data;

	if (apmg_pd->mode)
		xvt->apmg_pd_en = true;
	else
		xvt->apmg_pd_en = false;

	return 0;
}

static int iwl_xvt_allocate_dma(struct iwl_xvt *xvt,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_xvt_alloc_dma *dma_req = data_in->data;
	struct iwl_xvt_alloc_dma *dma_res;

	if (data_in->len < sizeof(struct iwl_xvt_alloc_dma))
		return -EINVAL;

	if (xvt->dma_cpu_addr) {
		IWL_ERR(xvt, "XVT DMA already allocated\n");
		return -EBUSY;
	}

	xvt->dma_cpu_addr = dma_alloc_coherent(xvt->trans->dev, dma_req->size,
					       &(xvt->dma_addr), GFP_KERNEL);

	if (!xvt->dma_cpu_addr) {
		return false;
	}

	dma_res = kmalloc(sizeof(*dma_res), GFP_KERNEL);
	if (!dma_res) {
		dma_free_coherent(xvt->trans->dev, dma_req->size,
				  xvt->dma_cpu_addr, xvt->dma_addr);
		xvt->dma_cpu_addr = NULL;
		xvt->dma_addr = 0;
		return -ENOMEM;
	}
	dma_res->size = dma_req->size;
	/* Casting to avoid compilation warnings when DMA address is 32bit */
	dma_res->addr = (u64)xvt->dma_addr;

	data_out->data = dma_res;
	data_out->len = sizeof(struct iwl_xvt_alloc_dma);
	xvt->dma_buffer_size = dma_req->size;

	return 0;
}

static int iwl_xvt_get_dma(struct iwl_xvt *xvt,
			   struct iwl_tm_data *data_in,
			   struct iwl_tm_data *data_out)
{
	struct iwl_xvt_get_dma *get_dma_resp;
	u32 resp_size;

	if (!xvt->dma_cpu_addr) {
		return -ENOMEM;
	}

	resp_size = sizeof(*get_dma_resp) + xvt->dma_buffer_size;
	get_dma_resp = kmalloc(resp_size, GFP_KERNEL);
	if (!get_dma_resp) {
		return -ENOMEM;
	}

	get_dma_resp->size = xvt->dma_buffer_size;
	memcpy(get_dma_resp->data, xvt->dma_cpu_addr, xvt->dma_buffer_size);
	data_out->data = get_dma_resp;
	data_out->len = resp_size;

	return 0;
}

static int iwl_xvt_free_dma(struct iwl_xvt *xvt,
			    struct iwl_tm_data *data_in)
{

	if (!xvt->dma_cpu_addr) {
		IWL_ERR(xvt, "XVT DMA was not allocated\n");
		return 0;
	}

	dma_free_coherent(xvt->trans->dev, xvt->dma_buffer_size,
			  xvt->dma_cpu_addr, xvt->dma_addr);
	xvt->dma_cpu_addr = NULL;
	xvt->dma_addr = 0;
	xvt->dma_buffer_size = 0;

	return 0;
}

static int iwl_xvt_get_chip_id(struct iwl_xvt *xvt,
			       struct iwl_tm_data *data_out)
{
	struct iwl_xvt_chip_id *chip_id;

	chip_id = kmalloc(sizeof(struct iwl_xvt_chip_id), GFP_KERNEL);
	if (!chip_id)
		return -ENOMEM;

	chip_id->registers[0] = ioread32((void __force __iomem *)XVT_SCU_SNUM1);
	chip_id->registers[1] = ioread32((void __force __iomem *)XVT_SCU_SNUM2);
	chip_id->registers[2] = ioread32((void __force __iomem *)XVT_SCU_SNUM3);


	data_out->data = chip_id;
	data_out->len = sizeof(struct iwl_xvt_chip_id);

	return 0;
}

static int iwl_xvt_get_mac_addr_info(struct iwl_xvt *xvt,
				     struct iwl_tm_data *data_out)
{
	struct iwl_xvt_mac_addr_info *mac_addr_info;
	u32 mac_addr0, mac_addr1;
	__u8 temp_mac_addr[ETH_ALEN];
	const u8 *hw_addr;

	mac_addr_info = kzalloc(sizeof(*mac_addr_info), GFP_KERNEL);
	if (!mac_addr_info)
		return -ENOMEM;

	if (xvt->cfg->nvm_type != IWL_NVM_EXT) {
		memcpy(mac_addr_info->mac_addr, xvt->nvm_hw_addr,
		       sizeof(mac_addr_info->mac_addr));
	} else {
		/* MAC address in family 8000 */
		if (xvt->is_nvm_mac_override) {
			memcpy(mac_addr_info->mac_addr, xvt->nvm_mac_addr,
			       sizeof(mac_addr_info->mac_addr));
		} else {
			/* read the mac address from WFMP registers */
			mac_addr0 = iwl_read_umac_prph_no_grab(xvt->trans,
							       WFMP_MAC_ADDR_0);
			mac_addr1 = iwl_read_umac_prph_no_grab(xvt->trans,
							       WFMP_MAC_ADDR_1);

			hw_addr = (const u8 *)&mac_addr0;
			temp_mac_addr[0] = hw_addr[3];
			temp_mac_addr[1] = hw_addr[2];
			temp_mac_addr[2] = hw_addr[1];
			temp_mac_addr[3] = hw_addr[0];

			hw_addr = (const u8 *)&mac_addr1;
			temp_mac_addr[4] = hw_addr[1];
			temp_mac_addr[5] = hw_addr[0];

			memcpy(mac_addr_info->mac_addr, temp_mac_addr,
			       sizeof(mac_addr_info->mac_addr));
		}
	}

	data_out->data = mac_addr_info;
	data_out->len = sizeof(*mac_addr_info);

	return 0;
}

static int iwl_xvt_add_txq(struct iwl_xvt *xvt,
			   struct iwl_scd_txq_cfg_cmd *cmd,
			   u16 ssn, u16 flags, int size)
{
	int queue_id = cmd->scd_queue, ret;

	if (iwl_xvt_is_unified_fw(xvt)) {
		/*TODO: add support for second lmac*/
		queue_id =
			iwl_trans_txq_alloc(xvt->trans,
					    cpu_to_le16(flags),
					    cmd->sta_id, cmd->tid,
					    SCD_QUEUE_CFG, size, 0);
		if (queue_id < 0)
			return queue_id;
	} else {
		iwl_trans_txq_enable_cfg(xvt->trans, queue_id, ssn, NULL, 0);
		ret = iwl_xvt_send_cmd_pdu(xvt, SCD_QUEUE_CFG, 0, sizeof(*cmd),
					   cmd);
		if (ret) {
			IWL_ERR(xvt, "Failed to config queue %d on FIFO %d\n",
				cmd->scd_queue, cmd->tx_fifo);
			return ret;
		}
	}

	xvt->queue_data[queue_id].allocated_queue = true;
	init_waitqueue_head(&xvt->queue_data[queue_id].tx_wq);

	return queue_id;
}

static int iwl_xvt_remove_txq(struct iwl_xvt *xvt,
			      struct iwl_scd_txq_cfg_cmd *cmd)
{
	int ret = 0;

	if (iwl_xvt_is_unified_fw(xvt)) {
		struct iwl_tx_queue_cfg_cmd queue_cfg_cmd = {
			.flags = 0,
			.sta_id = cmd->sta_id,
			.tid = cmd->tid,
		};

		ret = iwl_xvt_send_cmd_pdu(xvt, SCD_QUEUE_CFG, 0,
					   sizeof(queue_cfg_cmd),
					   &queue_cfg_cmd);
		iwl_trans_txq_free(xvt->trans, cmd->scd_queue);
	} else {
		iwl_trans_txq_disable(xvt->trans, cmd->scd_queue, false);
		ret = iwl_xvt_send_cmd_pdu(xvt, SCD_QUEUE_CFG, 0,
					   sizeof(*cmd), cmd);
	}

	if (WARN(ret, "failed to send SCD_QUEUE_CFG"))
		return ret;

	xvt->queue_data[cmd->scd_queue].allocated_queue = false;

	return 0;
}

static int iwl_xvt_config_txq(struct iwl_xvt *xvt,
			      struct iwl_xvt_driver_command_req *req,
			      struct iwl_xvt_driver_command_resp *resp)
{
	struct iwl_xvt_txq_config *conf =
		(struct iwl_xvt_txq_config *)req->input_data;
	int queue_id = conf->scd_queue, error;
	struct iwl_scd_txq_cfg_cmd cmd = {
		.sta_id = conf->sta_id,
		.tid = conf->tid,
		.scd_queue = conf->scd_queue,
		.action = conf->action,
		.aggregate = conf->aggregate,
		.tx_fifo = conf->tx_fifo,
		.window = conf->window,
		.ssn = cpu_to_le16(conf->ssn),
	};
	struct iwl_xvt_txq_config_resp txq_resp = {
		.sta_id = conf->sta_id,
		.tid = conf->tid,
	};

	if (req->max_out_length < sizeof(txq_resp))
		return -ENOBUFS;

	if (conf->action == TX_QUEUE_CFG_REMOVE) {
		error = iwl_xvt_remove_txq(xvt, &cmd);
		if (WARN(error, "failed to remove queue"))
			return error;
	} else {
		queue_id = iwl_xvt_add_txq(xvt, &cmd, conf->ssn,
					   conf->flags, conf->queue_size);
		if (queue_id < 0)
			return queue_id;
	}

	txq_resp.scd_queue = queue_id;

	memcpy(resp->resp_data, &txq_resp, sizeof(txq_resp));
	resp->length = sizeof(txq_resp);

	return 0;
}

static int
iwl_xvt_get_rx_agg_stats_cmd(struct iwl_xvt *xvt,
			     struct iwl_xvt_driver_command_req *req,
			     struct iwl_xvt_driver_command_resp *resp)
{
	struct iwl_xvt_get_rx_agg_stats *params = (void *)req->input_data;
	struct iwl_xvt_get_rx_agg_stats_resp *stats_resp =
						(void *)resp->resp_data;
	struct iwl_xvt_reorder_buffer *buffer;
	int i;

	IWL_DEBUG_INFO(xvt, "get rx agg stats: sta_id=%d, tid=%d\n",
		       params->sta_id, params->tid);

	if (req->max_out_length < sizeof(stats_resp))
		return -ENOBUFS;

	for (i = 0; i < ARRAY_SIZE(xvt->reorder_bufs); i++) {
		buffer = &xvt->reorder_bufs[i];
		if (buffer->sta_id != params->sta_id ||
		    buffer->tid != params->tid)
			continue;

		spin_lock_bh(&buffer->lock);
		stats_resp->dropped = buffer->stats.dropped;
		stats_resp->released = buffer->stats.released;
		stats_resp->skipped = buffer->stats.skipped;
		stats_resp->reordered = buffer->stats.reordered;

		/* clear statistics */
		memset(&buffer->stats, 0, sizeof(buffer->stats));
		spin_unlock_bh(&buffer->lock);

		break;
	}

	if (i == ARRAY_SIZE(xvt->reorder_bufs))
		return -ENOENT;

	resp->length = sizeof(*stats_resp);
	return 0;
}

static void iwl_xvt_config_rx_mpdu(struct iwl_xvt *xvt,
				   struct iwl_xvt_driver_command_req *req)

{
	xvt->send_rx_mpdu =
		((struct iwl_xvt_config_rx_mpdu_req *)req->input_data)->enable;
}

static int iwl_xvt_echo_notif(struct iwl_xvt *xvt)
{
	return iwl_xvt_user_send_notif(xvt, IWL_XVT_CMD_ECHO_NOTIF,
				       NULL, 0, GFP_KERNEL);
}

static int iwl_xvt_handle_driver_cmd(struct iwl_xvt *xvt,
				     struct iwl_tm_data *data_in,
				     struct iwl_tm_data *data_out)
{
	struct iwl_xvt_driver_command_req *req = data_in->data;
	struct iwl_xvt_driver_command_resp *resp = NULL;
	__u32 cmd_id = req->command_id;
	int err = 0;

	IWL_DEBUG_INFO(xvt, "handle driver command 0x%X\n", cmd_id);

	if (req->max_out_length > 0) {
		resp = kzalloc(sizeof(*resp) + req->max_out_length, GFP_KERNEL);
		if (!resp)
			return -ENOMEM;
	}

	/* resp->length and resp->resp_data should be set in command handler */
	switch (cmd_id) {
	case IWL_DRV_CMD_CONFIG_TX_QUEUE:
		err = iwl_xvt_config_txq(xvt, req, resp);
		break;
	case IWL_DRV_CMD_SET_TX_PAYLOAD:
		err = iwl_xvt_set_tx_payload(xvt, req);
		break;
	case IWL_DRV_CMD_TX_START:
		err = iwl_xvt_start_tx(xvt, req);
		break;
	case IWL_DRV_CMD_TX_STOP:
		err = iwl_xvt_stop_tx(xvt);
		break;
	case IWL_DRV_CMD_GET_RX_AGG_STATS:
		err = iwl_xvt_get_rx_agg_stats_cmd(xvt, req, resp);
		break;
	case IWL_DRV_CMD_CONFIG_RX_MPDU:
		iwl_xvt_config_rx_mpdu(xvt, req);
		break;
	case IWL_DRV_CMD_ECHO_NOTIF:
		err = iwl_xvt_echo_notif(xvt);
		break;
	default:
		IWL_ERR(xvt, "no command handler found for cmd_id[%u]\n",
			cmd_id);
		err = -EOPNOTSUPP;
	}

	if (err)
		goto out_free;

	if (req->max_out_length > 0) {
		if (WARN_ONCE(resp->length == 0,
			      "response was not set correctly\n")) {
			err = -ENODATA;
			goto out_free;
		}

		resp->command_id = cmd_id;
		data_out->len = resp->length +
			sizeof(struct iwl_xvt_driver_command_resp);
		data_out->data = resp;

		return err;
	}

out_free:
	kfree(resp);
	return err;
}

static int iwl_xvt_get_fw_tlv(struct iwl_xvt *xvt,
			      u32 img_id,
			      u32 tlv_id,
			      const u8 **out_tlv_data,
			      u32 *out_tlv_len)
{
	int err = 0;

	switch (tlv_id) {
	case IWL_UCODE_TLV_CMD_VERSIONS:
		*out_tlv_data = (const u8 *)xvt->fw->ucode_capa.cmd_versions;
		*out_tlv_len = xvt->fw->ucode_capa.n_cmd_versions *
			sizeof(struct iwl_fw_cmd_version);
		break;

	case IWL_UCODE_TLV_PHY_INTEGRATION_VERSION:
		*out_tlv_data = xvt->fw->phy_integration_ver;
		*out_tlv_len = xvt->fw->phy_integration_ver_len;
		break;

	default:
		IWL_ERR(xvt, "TLV type not supported, type = %u\n", tlv_id);

		*out_tlv_data = NULL;
		*out_tlv_len = 0;
		err = -EOPNOTSUPP;
	}

	return err;
}

static int iwl_xvt_handle_get_fw_tlv_len(struct iwl_xvt *xvt,
					 struct iwl_tm_data *data_in,
					 struct iwl_tm_data *data_out)
{
	struct iwl_xvt_get_fw_tlv_len_request *req = data_in->data;
	struct iwl_xvt_fw_tlv_len_response *resp;
	const u8 *tlv_data;
	u32 tlv_len;
	int err = 0;

	IWL_DEBUG_INFO(xvt, "handle get fw tlv len, type = %u\n",
		       req->tlv_type_id);

	err = iwl_xvt_get_fw_tlv(xvt, req->fw_img_type, req->tlv_type_id,
				 &tlv_data, &tlv_len);
	if (err)
		return err;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->bytes_len = tlv_len;

	data_out->len = sizeof(*resp);
	data_out->data = resp;

	return 0;
}

static int iwl_xvt_handle_get_fw_tlv_data(struct iwl_xvt *xvt,
					  struct iwl_tm_data *data_in,
					  struct iwl_tm_data *data_out)
{
	struct iwl_xvt_get_fw_tlv_data_request *req = data_in->data;
	struct iwl_xvt_fw_tlv_data_response *resp;
	const u8 *tlv_data;
	u32 tlv_len;
	int err;

	IWL_DEBUG_INFO(xvt, "handle get fw tlv data, type = %u\n",
		       req->tlv_type_id);

	err = iwl_xvt_get_fw_tlv(xvt, req->fw_img_type, req->tlv_type_id,
				 &tlv_data, &tlv_len);
	if (err)
		return err;

	data_out->len = sizeof(*resp) + tlv_len;
	resp = kzalloc(data_out->len, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->bytes_len = tlv_len;
	if (tlv_data)
		memcpy(resp->data, tlv_data, tlv_len);

	data_out->data = resp;

	return 0;
}

int iwl_xvt_user_cmd_execute(struct iwl_testmode *testmode, u32 cmd,
			     struct iwl_tm_data *data_in,
			     struct iwl_tm_data *data_out, bool *supported_cmd)
{
	struct iwl_xvt *xvt = testmode->op_mode;
	int ret = 0;

	*supported_cmd = true;
	if (WARN_ON_ONCE(!xvt || !data_in))
		return -EINVAL;

	IWL_DEBUG_INFO(xvt, "%s cmd=0x%X\n", __func__, cmd);
	mutex_lock(&xvt->mutex);

	switch (cmd) {

	/* Testmode custom cases */

	case IWL_TM_USER_CMD_GET_DEVICE_INFO:
		ret = iwl_xvt_get_dev_info(xvt, data_in, data_out);
		break;

	case IWL_TM_USER_CMD_SV_IO_TOGGLE:
		ret = iwl_xvt_sdio_io_toggle(xvt, data_in, data_out);
		break;

	/* xVT cases */

	case IWL_XVT_CMD_START:
		ret = iwl_xvt_start_op_mode(xvt);
		break;

	case IWL_XVT_CMD_STOP:
		iwl_xvt_stop_op_mode(xvt);
		break;

	case IWL_XVT_CMD_CONTINUE_INIT:
		ret = iwl_xvt_continue_init(xvt);
		break;

	case IWL_XVT_CMD_GET_PHY_DB_ENTRY:
		ret = iwl_xvt_get_phy_db(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_SET_CONFIG:
		ret = iwl_xvt_set_sw_config(xvt, data_in);
		break;

	case IWL_XVT_CMD_GET_CONFIG:
		ret = iwl_xvt_get_sw_config(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_MOD_TX:
		ret = iwl_xvt_modulated_tx(xvt, data_in);
		break;

	case IWL_XVT_CMD_RX_HDRS_MODE:
		ret = iwl_xvt_rx_hdrs_mode(xvt, data_in);
		break;

	case IWL_XVT_CMD_APMG_PD_MODE:
		ret = iwl_xvt_apmg_pd_mode(xvt, data_in);
		break;

	case IWL_XVT_CMD_ALLOC_DMA:
		ret = iwl_xvt_allocate_dma(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_GET_DMA:
		ret = iwl_xvt_get_dma(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_FREE_DMA:
		ret = iwl_xvt_free_dma(xvt, data_in);
		break;
	case IWL_XVT_CMD_GET_CHIP_ID:
		ret = iwl_xvt_get_chip_id(xvt, data_out);
		break;

	case IWL_XVT_CMD_GET_MAC_ADDR_INFO:
		ret = iwl_xvt_get_mac_addr_info(xvt, data_out);
		break;

	case IWL_XVT_CMD_MOD_TX_STOP:
		ret = iwl_xvt_modulated_tx_infinite_stop(xvt, data_in);
		break;

	case IWL_XVT_CMD_TX_QUEUE_CFG:
		ret = iwl_xvt_tx_queue_cfg(xvt, data_in);
		break;
	case IWL_XVT_CMD_DRIVER_CMD:
		ret = iwl_xvt_handle_driver_cmd(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_FW_TLV_GET_LEN:
		ret = iwl_xvt_handle_get_fw_tlv_len(xvt, data_in, data_out);
		break;

	case IWL_XVT_CMD_FW_TLV_GET_DATA:
		ret = iwl_xvt_handle_get_fw_tlv_data(xvt, data_in, data_out);
		break;

	default:
		*supported_cmd = false;
		ret = -EOPNOTSUPP;
		IWL_DEBUG_INFO(xvt, "%s (cmd=0x%X) Not supported by xVT\n",
			       __func__, cmd);
		break;
	}

	mutex_unlock(&xvt->mutex);

	if (ret && *supported_cmd)
		IWL_ERR(xvt, "%s (cmd=0x%X) ret=%d\n", __func__, cmd, ret);
	else
		IWL_DEBUG_INFO(xvt, "%s (cmd=0x%X) ended Ok\n", __func__, cmd);
	return ret;
}
