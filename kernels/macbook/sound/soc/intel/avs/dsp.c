// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Authors: Cezary Rojewski <cezary.rojewski@intel.com>
//          Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
//

#include <sound/hdaudio_ext.h>
#include "avs.h"
#include "registers.h"
#include "trace.h"

#define AVS_ADSPCS_INTERVAL_MS		500
#define AVS_ADSPCS_TIMEOUT_MS		50000

int avs_dsp_core_power(struct avs_dev *adev, u32 core_mask, bool active)
{
	u32 value, mask, reg;

	value = snd_hdac_adsp_readl(adev, AZX_ADSP_REG_ADSPCS);
	trace_avs_dsp_core_op(value, core_mask, "power", active);

	mask = AZX_ADSPCS_SPA_MASK(core_mask);
	value = active ? mask : 0;

	snd_hdac_adsp_updatel(adev, AZX_ADSP_REG_ADSPCS, mask, value);

	value = active ? AZX_ADSPCS_CPA_MASK(core_mask) : 0;

	return snd_hdac_adsp_readl_poll(adev, AZX_ADSP_REG_ADSPCS,
					reg, (reg & value) == value,
					AVS_ADSPCS_INTERVAL_MS,
					AVS_ADSPCS_TIMEOUT_MS);
}

int avs_dsp_core_reset(struct avs_dev *adev, u32 core_mask, bool reset)
{
	u32 value, mask, reg;

	value = snd_hdac_adsp_readl(adev, AZX_ADSP_REG_ADSPCS);
	trace_avs_dsp_core_op(value, core_mask, "reset", reset);

	mask = AZX_ADSPCS_CRST_MASK(core_mask);
	value = reset ? mask : 0;

	snd_hdac_adsp_updatel(adev, AZX_ADSP_REG_ADSPCS, mask, value);

	return snd_hdac_adsp_readl_poll(adev, AZX_ADSP_REG_ADSPCS,
					reg, (reg & value) == value,
					AVS_ADSPCS_INTERVAL_MS,
					AVS_ADSPCS_TIMEOUT_MS);
}

int avs_dsp_core_stall(struct avs_dev *adev, u32 core_mask, bool stall)
{
	u32 value, mask, reg;

	value = snd_hdac_adsp_readl(adev, AZX_ADSP_REG_ADSPCS);
	trace_avs_dsp_core_op(value, core_mask, "stall", stall);

	mask = AZX_ADSPCS_CSTALL_MASK(core_mask);
	value = stall ? mask : 0;

	snd_hdac_adsp_updatel(adev, AZX_ADSP_REG_ADSPCS, mask, value);

	return snd_hdac_adsp_readl_poll(adev, AZX_ADSP_REG_ADSPCS,
					reg, (reg & value) == value,
					AVS_ADSPCS_INTERVAL_MS,
					AVS_ADSPCS_TIMEOUT_MS);
}

int avs_dsp_core_enable(struct avs_dev *adev, u32 core_mask)
{
	int ret;

	ret = avs_dsp_op(adev, power, core_mask, true);
	if (ret)
		dev_warn(adev->dev, "core_mask %d power failed: %d\n",
			 core_mask, ret);

	ret = avs_dsp_op(adev, reset, core_mask, false);
	if (ret)
		dev_warn(adev->dev, "core_mask %d reset failed: %d\n",
			 core_mask, ret);

	return avs_dsp_op(adev, stall, core_mask, false);
}

int avs_dsp_core_disable(struct avs_dev *adev, u32 core_mask)
{
	int ret;

	ret = avs_dsp_op(adev, stall, core_mask, true);
	if (ret)
		dev_warn(adev->dev, "core_mask %d stall failed: %d\n",
			core_mask, ret);

	ret = avs_dsp_op(adev, reset, core_mask, true);
	if (ret)
		dev_warn(adev->dev, "core_mask %d reset failed: %d\n",
			core_mask, ret);

	return avs_dsp_op(adev, power, core_mask, false);
}

int avs_dsp_enable(struct avs_dev *adev, u32 core_mask)
{
	const struct avs_spec *const spec = adev->spec;
	u32 mask;
	int ret;

	ret = avs_dsp_core_enable(adev, core_mask);
	if (ret < 0) {
		dev_err(adev->dev, "core_mask %08x enable failed: %d\n",
			core_mask, ret);
		return ret;
	}

	mask = core_mask & ~(spec->master_mask);
	if (!mask)
		/*
		 * without master core, fw is dead anyway
		 * so setting D0 for it is futile.
		 */
		return 0;

	ret = avs_ipc_set_dx(adev, mask, true);
	if (ret) {
		dev_err(adev->dev, "set d0 failed: %d\n", ret);
		return AVS_IPC_RET(ret);
	}

	return 0;
}

int avs_dsp_disable(struct avs_dev *adev, u32 core_mask)
{
	int ret;

	ret = avs_ipc_set_dx(adev, core_mask, false);
	if (ret) {
		dev_err(adev->dev, "set d3 failed: %d\n", ret);
		return AVS_IPC_RET(ret);
	}

	ret = avs_dsp_core_disable(adev, core_mask);
	if (ret < 0) {
		dev_err(adev->dev, "core_mask %08x disable failed: %d\n",
			core_mask, ret);
		return ret;
	}

	return 0;
}

int avs_dsp_get_core(struct avs_dev *adev, u32 core_id)
{
	const struct avs_spec *const spec = adev->spec;
	atomic_t *ref;
	u32 mask;
	int ret;

	mask = BIT_MASK(core_id);
	if (mask == spec->master_mask)
		/* nothing to do for master */
		return 0;
	if (core_id >= adev->hw_cfg.dsp_cores)
		return -EINVAL;

	ref = &adev->core_refs[core_id];
	if (atomic_add_return(1, ref) == 1) {
		/*
		 * No cores other than master_mask can be running for DSP
		 * to achieve d0ix. Conscious SET_D0IX IPC failure is permitted,
		 * simply d0ix power state will no longer be attempted.
		 */
		ret = avs_dsp_disable_d0ix(adev);
		if (ret && ret != -AVS_EIPC)
			goto err;

		ret = avs_dsp_enable(adev, mask);
		if (ret) {
			avs_dsp_enable_d0ix(adev);
			goto err;
		}
	}

	return 0;
err:
	atomic_dec(ref);
	return ret;
}

int avs_dsp_put_core(struct avs_dev *adev, u32 core_id)
{
	const struct avs_spec *const spec = adev->spec;
	atomic_t *ref;
	u32 mask;
	int ret;

	mask = BIT_MASK(core_id);
	if (mask == spec->master_mask)
		/* nothing to do for master */
		return 0;
	if (core_id >= adev->hw_cfg.dsp_cores)
		return -EINVAL;

	ref = &adev->core_refs[core_id];
	if (atomic_dec_and_test(ref)) {
		ret = avs_dsp_disable(adev, mask);
		if (ret)
			return ret;

		/* Match disable_d0ix in avs_dsp_get_core(). */
		avs_dsp_enable_d0ix(adev);
	}

	return 0;
}

int avs_dsp_init_module(struct avs_dev *adev, u16 module_id, u8 ppl_instance_id,
			u8 core_id, u8 domain, void *param, u32 param_size,
			u16 *instance_id)
{
	struct avs_module_entry mentry;
	bool was_loaded = false;
	int ret, id;

	id = avs_module_id_alloc(adev, module_id);
	if (id < 0)
		return id;

	ret = avs_get_module_id_entry(adev, module_id, &mentry);

	ret = avs_dsp_get_core(adev, core_id);
	if (ret) {
		dev_err(adev->dev, "get core failed: %d\n", ret);
		goto err_core;
	}

	/* Load code into memory if this is the first instance. */
	if (!id && !avs_module_entry_is_loaded(&mentry)) {
		ret = avs_dsp_op(adev, transfer_mods, true, &mentry, 1);
		if (ret) {
			dev_err(adev->dev, "load modules failed: %d\n", ret);
			goto err_core;
		}
		was_loaded = true;
	}

	ret = avs_ipc_init_instance(adev, module_id, id, ppl_instance_id,
				    core_id, domain, param, param_size);
	if (ret) {
		dev_err(adev->dev, "init instance failed: %d\n", ret);
		ret = AVS_IPC_RET(ret);
		goto err_ipc;
	}

	*instance_id = id;
	return 0;

err_ipc:
	if (was_loaded)
		avs_dsp_op(adev, transfer_mods, false, &mentry, 1);
	avs_dsp_put_core(adev, core_id);
err_core:
	avs_module_id_free(adev, module_id, id);
	return ret;
}

void avs_dsp_delete_module(struct avs_dev *adev, u16 module_id, u16 instance_id,
			   u8 ppl_instance_id, u8 core_id)
{
	struct avs_module_entry mentry;
	int ret;

	/* Unowned modules need their resources freed explicitly. */
	if (ppl_instance_id == INVALID_PIPELINE_ID)
		avs_ipc_delete_instance(adev, module_id, instance_id);

	avs_module_id_free(adev, module_id, instance_id);

	ret = avs_get_module_id_entry(adev, module_id, &mentry);
	/* Unload occupied memory if this was the last instance. */
	if (!ret && mentry.type.load_type == AVS_MODULE_LOAD_TYPE_LOADABLE) {
		if (avs_is_module_ida_empty(adev, module_id)) {
			ret = avs_dsp_op(adev, transfer_mods, false, &mentry, 1);
			if (ret)
				dev_err(adev->dev, "unload modules failed: %d\n", ret);
		}
	}

	ret = avs_dsp_put_core(adev, core_id);
	if (ret)
		dev_err(adev->dev, "put core failed: %d\n", ret);
}

int avs_dsp_create_pipeline(struct avs_dev *adev, u16 req_size, u8 priority,
			    bool lp, u16 attributes, u8 *instance_id)
{
	struct avs_fw_cfg *fw_cfg = &adev->fw_cfg;
	int ret, id;

	id = ida_alloc_max(&adev->ppl_ida, fw_cfg->max_ppl_count - 1, GFP_KERNEL);
	if (id < 0)
		return id;

	ret = avs_ipc_create_pipeline(adev, req_size, priority, id, lp,
				      attributes);
	if (ret) {
		dev_err(adev->dev, "create pipeline failed: %d\n", ret);
		ida_free(&adev->ppl_ida, id);
		return AVS_IPC_RET(ret);
	}

	*instance_id = id;
	return 0;
}

int avs_dsp_delete_pipeline(struct avs_dev *adev, u8 instance_id)
{
	int ret;

	ret = avs_ipc_delete_pipeline(adev, instance_id);
	if (ret) {
		dev_err(adev->dev, "delete pipeline failed: %d\n", ret);
		ret = AVS_IPC_RET(ret);
	}

	ida_free(&adev->ppl_ida, instance_id);
	return ret;
}
