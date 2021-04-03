// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2012-2014, 2018-2020 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 */
#include "iwl-trans.h"
#include "iwl-tm-infc.h"
#include "iwl-drv.h"
#include "iwl-prph.h"
#include "iwl-io.h"

static int iwl_tm_send_hcmd(struct iwl_testmode *testmode,
			    struct iwl_tm_data *data_in,
			    struct iwl_tm_data *data_out)
{
	struct iwl_tm_cmd_request *hcmd_req = data_in->data;
	struct iwl_tm_cmd_request *cmd_resp;
	u32 reply_len, resp_size;
	struct iwl_rx_packet *pkt;
	struct iwl_host_cmd host_cmd = {
		.id = hcmd_req->id,
		.data[0] = hcmd_req->data,
		.len[0] = hcmd_req->len,
		.dataflags[0] = IWL_HCMD_DFL_NOCOPY,
	};
	int ret;

	if (!testmode->send_hcmd)
		return -EOPNOTSUPP;

	if (hcmd_req->want_resp)
		host_cmd.flags |= CMD_WANT_SKB;

	mutex_lock(testmode->mutex);
	ret = testmode->send_hcmd(testmode->op_mode, &host_cmd);
	mutex_unlock(testmode->mutex);
	if (ret)
		return ret;
	/* if no reply is required, we are done */
	if (!(host_cmd.flags & CMD_WANT_SKB))
		return 0;

	/* Retrieve response packet */
	pkt = host_cmd.resp_pkt;
	reply_len = iwl_rx_packet_len(pkt);

	/* Set response data */
	resp_size = sizeof(struct iwl_tm_cmd_request) + reply_len;
	cmd_resp = kzalloc(resp_size, GFP_KERNEL);
	if (!cmd_resp) {
		ret = -ENOMEM;
		goto out;
	}
	cmd_resp->id = hcmd_req->id;
	cmd_resp->len = reply_len;
	memcpy(cmd_resp->data, &(pkt->hdr), reply_len);

	data_out->data = cmd_resp;
	data_out->len = resp_size;
	ret = 0;

out:
	iwl_free_resp(&host_cmd);
	return ret;
}

static void iwl_tm_execute_reg_ops(struct iwl_testmode *testmode,
				   struct iwl_tm_regs_request *request,
				   struct iwl_tm_regs_request *result)
{
	struct iwl_tm_reg_op *cur_op;
	u32 idx, read_idx;

	for (idx = 0, read_idx = 0; idx < request->num; idx++) {
		cur_op = &request->reg_ops[idx];

		if  (cur_op->op_type == IWL_TM_REG_OP_READ) {
			cur_op->value = iwl_read32(testmode->trans,
						   cur_op->address);
			memcpy(&result->reg_ops[read_idx], cur_op,
			       sizeof(*cur_op));
			read_idx++;
		} else {
			/* IWL_TM_REG_OP_WRITE is the only possible option */
			iwl_write32(testmode->trans, cur_op->address,
				    cur_op->value);
		}
	}
}

static int iwl_tm_reg_ops(struct iwl_testmode *testmode,
			  struct iwl_tm_data *data_in,
			  struct iwl_tm_data *data_out)
{
	struct iwl_tm_reg_op *cur_op;
	struct iwl_tm_regs_request *request = data_in->data;
	struct iwl_tm_regs_request *result;
	u32 result_size;
	u32 idx, read_idx;
	bool is_grab_nic_access_required = true;

	/* Calculate result size (result is returned only for read ops) */
	for (idx = 0, read_idx = 0; idx < request->num; idx++) {
		if (request->reg_ops[idx].op_type == IWL_TM_REG_OP_READ)
			read_idx++;
		/* check if there is an operation that it is not */
		/* in the CSR range (0x00000000 - 0x000003FF)    */
		/* and not in the AL range			 */
		cur_op = &request->reg_ops[idx];
		if (IS_AL_ADDR(cur_op->address) || cur_op->address < HBUS_BASE)
			is_grab_nic_access_required = false;
	}
	result_size = sizeof(struct iwl_tm_regs_request) +
		      read_idx * sizeof(struct iwl_tm_reg_op);

	result = kzalloc(result_size, GFP_KERNEL);
	if (!result)
		return -ENOMEM;
	result->num = read_idx;
	if (is_grab_nic_access_required) {
		if (!iwl_trans_grab_nic_access(testmode->trans)) {
			kfree(result);
			return -EBUSY;
		}
		iwl_tm_execute_reg_ops(testmode, request, result);
		iwl_trans_release_nic_access(testmode->trans);
	} else {
		iwl_tm_execute_reg_ops(testmode, request, result);
	}

	data_out->data = result;
	data_out->len = result_size;

	return 0;
}

static int iwl_tm_get_dev_info(struct iwl_testmode *testmode,
			       struct iwl_tm_data *data_out)
{
	struct iwl_tm_dev_info *dev_info;
	const u8 driver_ver[] = BACKPORTS_GIT_TRACKED;

	dev_info = kzalloc(sizeof(*dev_info) + (strlen(driver_ver) + 1) *
			   sizeof(u8), GFP_KERNEL);
	if (!dev_info)
		return -ENOMEM;

	dev_info->dev_id = testmode->trans->hw_id;
	dev_info->fw_ver = testmode->fw->ucode_ver;
	dev_info->vendor_id = PCI_VENDOR_ID_INTEL;
	dev_info->silicon_step = CSR_HW_REV_STEP(testmode->trans->hw_rev);

	/* TODO: Assign real value when feature is implemented */
	dev_info->build_ver = 0x00;

	strcpy(dev_info->driver_ver, driver_ver);

	data_out->data = dev_info;
	data_out->len = sizeof(*dev_info);

	return 0;
}

static int iwl_tm_indirect_read(struct iwl_testmode *testmode,
				struct iwl_tm_data *data_in,
				struct iwl_tm_data *data_out)
{
	struct iwl_trans *trans = testmode->trans;
	struct iwl_tm_sram_read_request *cmd_in = data_in->data;
	u32 addr = cmd_in->offset;
	u32 size = cmd_in->length;
	u32 *buf32, size32, i;

	if (size & (sizeof(u32) - 1))
		return -EINVAL;

	data_out->data = kmalloc(size, GFP_KERNEL);
	if (!data_out->data)
		return -ENOMEM;

	data_out->len = size;

	size32 = size / sizeof(u32);
	buf32 = data_out->data;

	mutex_lock(testmode->mutex);

	/* Hard-coded periphery absolute address */
	if (addr >= IWL_ABS_PRPH_START &&
	    addr < IWL_ABS_PRPH_START + PRPH_END) {
		if (!iwl_trans_grab_nic_access(trans)) {
			mutex_unlock(testmode->mutex);
			return -EBUSY;
		}
		for (i = 0; i < size32; i++)
			buf32[i] = iwl_trans_read_prph(trans,
						       addr + i * sizeof(u32));
		iwl_trans_release_nic_access(trans);
	} else {
		/* target memory (SRAM) */
		iwl_trans_read_mem(trans, addr, buf32, size32);
	}

	mutex_unlock(testmode->mutex);
	return 0;
}

static int iwl_tm_indirect_write(struct iwl_testmode *testmode,
				 struct iwl_tm_data *data_in)
{
	struct iwl_trans *trans = testmode->trans;
	struct iwl_tm_sram_write_request *cmd_in = data_in->data;
	u32 addr = cmd_in->offset;
	u32 size = cmd_in->len;
	u8 *buf = cmd_in->buffer;
	u32 *buf32 = (u32 *)buf, size32 = size / sizeof(u32);
	u32 val, i;

	mutex_lock(testmode->mutex);
	if (addr >= IWL_ABS_PRPH_START &&
	    addr < IWL_ABS_PRPH_START + PRPH_END) {
		/* Periphery writes can be 1-3 bytes long, or DWORDs */
		if (size < 4) {
			memcpy(&val, buf, size);
			if (!iwl_trans_grab_nic_access(trans)) {
				mutex_unlock(testmode->mutex);
				return -EBUSY;
			}
			iwl_write32(trans, HBUS_TARG_PRPH_WADDR,
				    (addr & 0x000FFFFF) | ((size - 1) << 24));
			iwl_write32(trans, HBUS_TARG_PRPH_WDAT, val);
			iwl_trans_release_nic_access(trans);
		} else {
			if (size % sizeof(u32)) {
				mutex_unlock(testmode->mutex);
				return -EINVAL;
			}

			for (i = 0; i < size32; i++)
				iwl_write_prph(trans, addr + i * sizeof(u32),
					       buf32[i]);
		}
	} else {
		iwl_trans_write_mem(trans, addr, buf32, size32);
	}
	mutex_unlock(testmode->mutex);

	return 0;
}

static int iwl_tm_get_fw_info(struct iwl_testmode *testmode,
			      struct iwl_tm_data *data_out)
{
	struct iwl_tm_get_fw_info *fw_info;
	u32 api_len, capa_len;
	u32 *bitmap;
	int i;

	if (!testmode->fw_major_ver || !testmode->fw_minor_ver)
		return -EOPNOTSUPP;

	api_len = 4 * DIV_ROUND_UP(NUM_IWL_UCODE_TLV_API, 32);
	capa_len = 4 * DIV_ROUND_UP(NUM_IWL_UCODE_TLV_CAPA, 32);

	fw_info = kzalloc(sizeof(*fw_info) + api_len + capa_len, GFP_KERNEL);
	if (!fw_info)
		return -ENOMEM;

	fw_info->fw_major_ver = testmode->fw_major_ver;
	fw_info->fw_minor_ver = testmode->fw_minor_ver;
	fw_info->fw_capa_api_len = api_len;
	fw_info->fw_capa_flags = testmode->fw->ucode_capa.flags;
	fw_info->fw_capa_len = capa_len;

	bitmap = (u32 *)fw_info->data;
	for (i = 0; i < NUM_IWL_UCODE_TLV_API; i++) {
		if (fw_has_api(&testmode->fw->ucode_capa,
			       (__force iwl_ucode_tlv_api_t)i))
			bitmap[i / 32] |= BIT(i % 32);
	}

	bitmap = (u32 *)(fw_info->data + api_len);
	for (i = 0; i < NUM_IWL_UCODE_TLV_CAPA; i++) {
		if (fw_has_capa(&testmode->fw->ucode_capa,
				(__force iwl_ucode_tlv_capa_t)i))
			bitmap[i / 32] |= BIT(i % 32);
	}

	data_out->data = fw_info;
	data_out->len = sizeof(*fw_info) + api_len + capa_len;

	return 0;
}

/**
 * iwl_tm_execute_cmd - Implementation of test command executor
 * @testmode:	  iwl_testmode, holds all relevant data for execution
 * @cmd:	  User space command's index
 * @data_in:	  Input data. "data" field is to be casted to relevant
 *		  data structure. All verification must be done in the
 *		  caller function, therefor assuming that input data
 *		  length is valid.
 * @data_out:	  Will be allocated inside, freeing is in the caller's
 *		  responsibility
 */
int iwl_tm_execute_cmd(struct iwl_testmode *testmode, u32 cmd,
		       struct iwl_tm_data *data_in,
		       struct iwl_tm_data *data_out)
{
	const struct iwl_test_ops *test_ops;
	bool cmd_supported = false;
	int ret;

	if (!testmode->trans->op_mode) {
		IWL_ERR(testmode->trans, "No op_mode!\n");
		return -ENODEV;
	}
	if (WARN_ON_ONCE(!testmode->op_mode || !data_in))
		return -EINVAL;

	test_ops = &testmode->trans->op_mode->ops->test_ops;

	if (test_ops->cmd_exec)
		ret = test_ops->cmd_exec(testmode, cmd, data_in, data_out,
					    &cmd_supported);

	if (cmd_supported)
		goto out;

	switch (cmd) {
	case IWL_TM_USER_CMD_HCMD:
		ret = iwl_tm_send_hcmd(testmode, data_in, data_out);
		break;
	case IWL_TM_USER_CMD_REG_ACCESS:
		ret = iwl_tm_reg_ops(testmode, data_in, data_out);
		break;
	case IWL_TM_USER_CMD_SRAM_WRITE:
		ret = iwl_tm_indirect_write(testmode, data_in);
		break;
	case IWL_TM_USER_CMD_SRAM_READ:
		ret = iwl_tm_indirect_read(testmode, data_in, data_out);
		break;
	case IWL_TM_USER_CMD_GET_DEVICE_INFO:
		ret = iwl_tm_get_dev_info(testmode, data_out);
		break;
	case IWL_TM_USER_CMD_GET_FW_INFO:
		ret = iwl_tm_get_fw_info(testmode, data_out);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

out:
	return ret;
}

void iwl_tm_init(struct iwl_trans *trans, const struct iwl_fw *fw,
		 struct mutex *mutex, void *op_mode)
{
	struct iwl_testmode *testmode = &trans->testmode;

	testmode->trans = trans;
	testmode->fw = fw;
	testmode->mutex = mutex;
	testmode->op_mode = op_mode;

	if (trans->op_mode->ops->test_ops.send_hcmd)
		testmode->send_hcmd = trans->op_mode->ops->test_ops.send_hcmd;
}
IWL_EXPORT_SYMBOL(iwl_tm_init);

void iwl_tm_set_fw_ver(struct iwl_trans *trans, u32 fw_major_ver,
		       u32 fw_minor_var)
{
	struct iwl_testmode *testmode = &trans->testmode;

	testmode->fw_major_ver = fw_major_ver;
	testmode->fw_minor_ver = fw_minor_var;
}
IWL_EXPORT_SYMBOL(iwl_tm_set_fw_ver);
