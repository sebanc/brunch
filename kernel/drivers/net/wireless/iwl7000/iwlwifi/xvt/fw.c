/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2007 - 2014, 2018 - 2020 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2005 - 2014, 2018 - 2020 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "fw/img.h"
#include "iwl-csr.h"

#include "xvt.h"
#include "iwl-dnt-cfg.h"
#include "fw/dbg.h"
#include "fw/testmode.h"
#include "fw/api/power.h"

#define XVT_UCODE_ALIVE_TIMEOUT	(HZ * CPTCFG_IWL_TIMEOUT_FACTOR)

struct iwl_xvt_alive_data {
	bool valid;
	u32 scd_base_addr;
};

static int iwl_xvt_send_dqa_cmd(struct iwl_xvt *xvt)
{
	struct iwl_dqa_enable_cmd dqa_cmd = {
		.cmd_queue = cpu_to_le32(IWL_MVM_DQA_CMD_QUEUE),
	};
	u32 cmd_id = iwl_cmd_id(DQA_ENABLE_CMD, DATA_PATH_GROUP, 0);
	int ret;

	ret = iwl_xvt_send_cmd_pdu(xvt, cmd_id, 0, sizeof(dqa_cmd), &dqa_cmd);
	if (ret)
		IWL_ERR(xvt, "Failed to send DQA enabling command: %d\n", ret);
	else
		IWL_DEBUG_FW(xvt, "Working in DQA mode\n");

	return ret;
}

static bool iwl_alive_fn(struct iwl_notif_wait_data *notif_wait,
			 struct iwl_rx_packet *pkt,
			 void *data)
{
	struct iwl_xvt *xvt =
		container_of(notif_wait, struct iwl_xvt, notif_wait);
	struct iwl_xvt_alive_data *alive_data = data;
	struct xvt_alive_resp_ver2 *palive2;
	struct mvm_alive_resp_v3 *palive3;
	struct mvm_alive_resp *palive4;
	struct iwl_lmac_alive *lmac1, *lmac2;
	struct iwl_umac_alive *umac;
	u32 rx_packet_payload_size = iwl_rx_packet_payload_len(pkt);
	u16 status, flags;
	u32 lmac_error_event_table, umac_error_event_table;

	if (rx_packet_payload_size == sizeof(*palive2)) {

		palive2 = (void *)pkt->data;

		lmac_error_event_table =
			le32_to_cpu(palive2->error_event_table_ptr);
		alive_data->scd_base_addr = le32_to_cpu(palive2->scd_base_ptr);

		alive_data->valid = le16_to_cpu(palive2->status) ==
				    IWL_ALIVE_STATUS_OK;
		iwl_tm_set_fw_ver(xvt->trans, palive2->ucode_major,
				  palive2->ucode_minor);
		umac_error_event_table =
			le32_to_cpu(palive2->error_info_addr);

		IWL_DEBUG_FW(xvt,
			     "Alive VER2 ucode status 0x%04x revision 0x%01X "
			     "0x%01X flags 0x%01X\n",
			     le16_to_cpu(palive2->status), palive2->ver_type,
			     palive2->ver_subtype, palive2->flags);

		IWL_DEBUG_FW(xvt,
			     "UMAC version: Major - 0x%x, Minor - 0x%x\n",
			     palive2->umac_major, palive2->umac_minor);
	} else {
		if (rx_packet_payload_size == sizeof(*palive3)) {
			palive3 = (void *)pkt->data;
			status = le16_to_cpu(palive3->status);
			flags = le16_to_cpu(palive3->flags);
			lmac1 = &palive3->lmac_data;
			umac = &palive3->umac_data;

			IWL_DEBUG_FW(xvt, "Alive VER3\n");
		} else if (rx_packet_payload_size == sizeof(*palive4)) {
			__le32 lmac2_err_ptr;

			palive4 = (void *)pkt->data;
			status = le16_to_cpu(palive4->status);
			flags = le16_to_cpu(palive4->flags);
			lmac1 = &palive4->lmac_data[0];
			lmac2 = &palive4->lmac_data[1];
			umac = &palive4->umac_data;
			lmac2_err_ptr = lmac2->dbg_ptrs.error_event_table_ptr;
			xvt->trans->dbg.lmac_error_event_table[1] =
				le32_to_cpu(lmac2_err_ptr);

			IWL_DEBUG_FW(xvt, "Alive VER4 CDB\n");
		} else {
			IWL_ERR(xvt, "unrecognized alive notificatio\n");
			return false;
		}

		alive_data->valid = status == IWL_ALIVE_STATUS_OK;
		lmac_error_event_table =
			le32_to_cpu(lmac1->dbg_ptrs.error_event_table_ptr);
		alive_data->scd_base_addr =
			le32_to_cpu(lmac1->dbg_ptrs.scd_base_ptr);
		iwl_tm_set_fw_ver(xvt->trans, le32_to_cpu(lmac1->ucode_major),
				  le32_to_cpu(lmac1->ucode_minor));
		umac_error_event_table =
			le32_to_cpu(umac->dbg_ptrs.error_info_addr);

		IWL_DEBUG_FW(xvt,
			     "status 0x%04x rev 0x%01X 0x%01X flags 0x%01X\n",
			     status, lmac1->ver_type, lmac1->ver_subtype,
			     flags);
		IWL_DEBUG_FW(xvt,
			     "UMAC version: Major - 0x%x, Minor - 0x%x\n",
			     umac->umac_major, umac->umac_minor);
	}

	iwl_fw_lmac1_set_alive_err_table(xvt->trans, lmac_error_event_table);
	if (umac_error_event_table)
		iwl_fw_umac_set_alive_err_table(xvt->trans,
						umac_error_event_table);

	return true;
}

static int iwl_xvt_load_ucode_wait_alive(struct iwl_xvt *xvt,
					 enum iwl_ucode_type ucode_type)
{
	struct iwl_notification_wait alive_wait;
	struct iwl_xvt_alive_data alive_data;
	const struct fw_img *fw;
	int ret;
	enum iwl_ucode_type old_type = xvt->fwrt.cur_fw_img;
	static const u16 alive_cmd[] = { MVM_ALIVE };
	struct iwl_scd_txq_cfg_cmd cmd = {
				.scd_queue = IWL_XVT_DEFAULT_TX_QUEUE,
				.action = SCD_CFG_ENABLE_QUEUE,
				.window = IWL_FRAME_LIMIT,
				.sta_id = IWL_XVT_TX_STA_ID_DEFAULT,
				.ssn = 0,
				.tx_fifo = IWL_XVT_DEFAULT_TX_FIFO,
				.aggregate = false,
				.tid = IWL_MAX_TID_COUNT,
			};

	iwl_fw_set_current_image(&xvt->fwrt, ucode_type);
	fw = iwl_get_ucode_image(xvt->fw, ucode_type);

	if (!fw)
		return -EINVAL;

	iwl_init_notification_wait(&xvt->notif_wait, &alive_wait,
				   alive_cmd, ARRAY_SIZE(alive_cmd),
				   iwl_alive_fn, &alive_data);

	ret = iwl_trans_start_fw_dbg(xvt->trans, fw,
				     ucode_type == IWL_UCODE_INIT,
				     (xvt->sw_stack_cfg.fw_dbg_flags &
				     ~IWL_XVT_DBG_FLAGS_NO_DEFAULT_TXQ));
	if (ret) {
		iwl_fw_set_current_image(&xvt->fwrt, old_type);
		iwl_remove_notification(&xvt->notif_wait, &alive_wait);
		return ret;
	}

	/*
	 * Some things may run in the background now, but we
	 * just wait for the ALIVE notification here.
	 */
	ret = iwl_wait_notification(&xvt->notif_wait, &alive_wait,
				    XVT_UCODE_ALIVE_TIMEOUT);
	if (ret) {
		iwl_fw_set_current_image(&xvt->fwrt, old_type);
		return ret;
	}

	if (!alive_data.valid) {
		IWL_ERR(xvt, "Loaded ucode is not valid!\n");
		iwl_fw_set_current_image(&xvt->fwrt, old_type);
		return -EIO;
	}

	/* fresh firmware was loaded */
	xvt->fw_error = false;

	iwl_trans_fw_alive(xvt->trans, alive_data.scd_base_addr);

	ret = iwl_init_paging(&xvt->fwrt, ucode_type);
	if (ret)
		return ret;

	if (ucode_type == IWL_UCODE_REGULAR &&
	    fw_has_capa(&xvt->fw->ucode_capa, IWL_UCODE_TLV_CAPA_DQA_SUPPORT)) {
		ret = iwl_xvt_send_dqa_cmd(xvt);
		if (ret)
			return ret;
	}
	/*
	 * Starting from 22000 tx queue allocation must be done after add
	 * station, so it is not part of the init flow.
	 */
	if (!iwl_xvt_is_unified_fw(xvt) &&
	    iwl_xvt_has_default_txq(xvt) &&
	    ucode_type != IWL_UCODE_INIT) {
		iwl_trans_txq_enable_cfg(xvt->trans, IWL_XVT_DEFAULT_TX_QUEUE,
					 0, NULL, 0);

		WARN(iwl_xvt_send_cmd_pdu(xvt, SCD_QUEUE_CFG, 0, sizeof(cmd),
					  &cmd),
		     "Failed to configure queue %d on FIFO %d\n",
		     IWL_XVT_DEFAULT_TX_QUEUE, IWL_XVT_DEFAULT_TX_FIFO);
		xvt->tx_meta_data[XVT_LMAC_0_ID].queue =
					IWL_XVT_DEFAULT_TX_QUEUE;
	}

	xvt->fw_running = true;
#ifdef CPTCFG_IWLWIFI_DEBUGFS
	iwl_fw_set_dbg_rec_on(&xvt->fwrt);
#endif

	return 0;
}

static int iwl_xvt_send_extended_config(struct iwl_xvt *xvt)
{
	/*
	 * TODO: once WRT will be implemented in xVT, IWL_INIT_DEBUG_CFG
	 * flag will not always be set
	 */
	struct iwl_init_extended_cfg_cmd ext_cfg = {
		.init_flags = cpu_to_le32(BIT(IWL_INIT_NVM) |
					  BIT(IWL_INIT_DEBUG_CFG)),

	};

	if (xvt->sw_stack_cfg.load_mask & IWL_XVT_LOAD_MASK_RUNTIME)
		ext_cfg.init_flags |= cpu_to_le32(BIT(IWL_INIT_PHY));

	return iwl_xvt_send_cmd_pdu(xvt, WIDE_ID(SYSTEM_GROUP,
						 INIT_EXTENDED_CFG_CMD), 0,
				    sizeof(ext_cfg), &ext_cfg);
}

static int iwl_xvt_config_ltr(struct iwl_xvt *xvt)
{
	struct iwl_ltr_config_cmd cmd = {
		.flags = cpu_to_le32(LTR_CFG_FLAG_FEATURE_ENABLE),
	};

	if (!xvt->trans->ltr_enabled)
		return 0;

	return iwl_xvt_send_cmd_pdu(xvt, LTR_CONFIG, 0, sizeof(cmd), &cmd);
}

int iwl_xvt_run_fw(struct iwl_xvt *xvt, u32 ucode_type)
{
	int ret;

	if (ucode_type >= IWL_UCODE_TYPE_MAX)
		return -EINVAL;

	lockdep_assert_held(&xvt->mutex);

	if (xvt->state != IWL_XVT_STATE_UNINITIALIZED) {
		if (xvt->fw_running) {
			xvt->fw_running = false;
			if (xvt->fwrt.cur_fw_img == IWL_UCODE_REGULAR)
				iwl_xvt_txq_disable(xvt);
		}
		iwl_fw_dbg_stop_sync(&xvt->fwrt);
		iwl_trans_stop_device(xvt->trans);
	}

	ret = iwl_trans_start_hw(xvt->trans);
	if (ret) {
		IWL_ERR(xvt, "Failed to start HW\n");
		return ret;
	}

	iwl_trans_set_bits_mask(xvt->trans,
				CSR_HW_IF_CONFIG_REG,
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI,
				CSR_HW_IF_CONFIG_REG_BIT_MAC_SI);

	iwl_dbg_tlv_time_point(&xvt->fwrt, IWL_FW_INI_TIME_POINT_EARLY, NULL);

	/* Will also start the device */
	ret = iwl_xvt_load_ucode_wait_alive(xvt, ucode_type);
	if (ret) {
		IWL_ERR(xvt, "Failed to start ucode: %d\n", ret);
		iwl_fw_dbg_stop_sync(&xvt->fwrt);
		iwl_trans_stop_device(xvt->trans);
	}

	iwl_dbg_tlv_time_point(&xvt->fwrt, IWL_FW_INI_TIME_POINT_AFTER_ALIVE,
			       NULL);

	iwl_get_shared_mem_conf(&xvt->fwrt);

	if (iwl_xvt_is_unified_fw(xvt)) {
		ret = iwl_xvt_send_extended_config(xvt);
		if (ret) {
			IWL_ERR(xvt, "Failed to send extended_config: %d\n",
				ret);
			iwl_fw_dbg_stop_sync(&xvt->fwrt);
			iwl_trans_stop_device(xvt->trans);
			return ret;
		}
	}
	iwl_dnt_start(xvt->trans);

	if (xvt->fwrt.cur_fw_img == IWL_UCODE_REGULAR &&
	    (!fw_has_capa(&xvt->fw->ucode_capa,
			  IWL_UCODE_TLV_CAPA_SET_LTR_GEN2)))
		WARN_ON(iwl_xvt_config_ltr(xvt));

	if (!iwl_trans_dbg_ini_valid(xvt->trans)) {
		xvt->fwrt.dump.conf = FW_DBG_INVALID;
		/* if we have a destination, assume EARLY START */
		if (xvt->fw->dbg.dest_tlv)
			xvt->fwrt.dump.conf = FW_DBG_START_FROM_ALIVE;
		iwl_fw_start_dbg_conf(&xvt->fwrt, FW_DBG_START_FROM_ALIVE);
	}

	return ret;
}
