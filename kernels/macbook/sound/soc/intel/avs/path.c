// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Authors: Cezary Rojewski <cezary.rojewski@intel.com>
//          Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
//

#include <sound/intel-nhlt.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "avs.h"
#include "path.h"
#include "topology.h"
#include "kcontrol.h"

static struct avs_tplg *
avs_path_find_tplg(struct avs_dev *adev, const char *name)
{
	struct avs_soc_component *acomp;

	list_for_each_entry(acomp, &adev->comp_list, node)
		if (!strcmp(acomp->tplg->name, name))
			return acomp->tplg;
	return NULL;
}

static struct avs_path_module *
avs_path_find_module(struct avs_path_pipeline *ppl, u32 template_id)
{
	struct avs_path_module *mod;

	list_for_each_entry(mod, &ppl->mod_list, node)
		if (mod->template->id == template_id)
			return mod;
	return NULL;
}

static struct avs_path_pipeline *
avs_path_find_pipeline(struct avs_path *path, u32 template_id)
{
	struct avs_path_pipeline *ppl;

	list_for_each_entry(ppl, &path->ppl_list, node)
		if (ppl->template->id == template_id)
			return ppl;
	return NULL;
}

static struct avs_path *
avs_path_find_path(struct avs_dev *adev, const char *name, u32 template_id)
{
	struct avs_tplg_path_template *pos, *template = NULL;
	struct avs_tplg *tplg;
	struct avs_path *path;

	tplg = avs_path_find_tplg(adev, name);
	if (!tplg)
		return NULL;

	list_for_each_entry(pos, &tplg->path_tmpl_list, node) {
		if (pos->id == template_id) {
			template = pos;
			break;
		}
	}
	if (!template)
		return NULL;

	spin_lock(&adev->path_list_lock);
	/* Only one variant of given path template may be instantiated at a time. */
	list_for_each_entry(path, &adev->path_list, node) {
		if (path->template->owner == template) {
			spin_unlock(&adev->path_list_lock);
			return path;
		}
	}

	spin_unlock(&adev->path_list_lock);
	return NULL;
}

static bool avs_test_hw_params(struct snd_pcm_hw_params *params,
			       struct avs_audio_format *fmt)
{
	return (params_rate(params) == fmt->sampling_freq &&
		params_channels(params) == fmt->num_channels &&
		params_physical_width(params) == fmt->bit_depth &&
		params_width(params) == fmt->valid_bit_depth);
}

static struct avs_tplg_path *
avs_path_find_variant(struct avs_dev *adev,
		      struct avs_tplg_path_template *template,
		      struct snd_pcm_hw_params *fe_params,
		      struct snd_pcm_hw_params *be_params)
{
	struct avs_tplg_path *variant;

	list_for_each_entry(variant, &template->path_list, node) {
		dev_dbg(adev->dev, "check FE rate %d chn %d vbd %d bd %d\n",
			variant->fe_fmt->sampling_freq, variant->fe_fmt->num_channels,
			variant->fe_fmt->valid_bit_depth, variant->fe_fmt->bit_depth);
		dev_dbg(adev->dev, "check BE rate %d chn %d vbd %d bd %d\n",
			variant->be_fmt->sampling_freq, variant->be_fmt->num_channels,
			variant->be_fmt->valid_bit_depth, variant->be_fmt->bit_depth);

		if (variant->fe_fmt && avs_test_hw_params(fe_params, variant->fe_fmt) &&
		    variant->be_fmt && avs_test_hw_params(be_params, variant->be_fmt))
			return variant;
	}

	return NULL;
}

static struct avs_tplg_path *
avs_condpath_find_variant(struct avs_dev *adev,
			  struct avs_tplg_path_template *template,
			  struct avs_path *source, struct avs_path *sink)
{
	struct avs_tplg_path *variant;

	list_for_each_entry(variant, &template->path_list, node) {
		if (variant->source_path_id == source->template->id &&
		    variant->sink_path_id == sink->template->id)
			return variant;
	}

	return NULL;
}

static bool avs_tplg_path_template_id_equal(struct avs_tplg_path_template_id *id,
					    struct avs_tplg_path_template_id *id2)
{
	return id->id == id2->id && !strcmp(id->tplg_name, id2->tplg_name);
}

static struct avs_path *
avs_condpath_find_match(struct avs_dev *adev,
			struct avs_tplg_path_template *template,
			struct avs_path *path, int dir)
{
	struct avs_tplg_path_template_id *id, *id2;

	if (dir) {
		id = &template->source;
		id2 = &template->sink;
	} else {
		id = &template->sink;
		id2 = &template->source;
	}

	/* Check whether this path is either source or sink of condpath template. */
	if (id->id != path->template->owner->id ||
	    strcmp(id->tplg_name, path->template->owner->owner->name))
		return NULL;

	/* Unidirectional condpaths are allowed. */
	if (avs_tplg_path_template_id_equal(id, id2))
		return path;

	/* Now find the counterpart */
	return avs_path_find_path(adev, id2->tplg_name, id2->id);
}

static struct avs_path *
avs_condpath_find_conflict(struct avs_dev *adev, u32 cond_type,
			   struct avs_path *path, int dir)
{
	struct avs_path *pos, *target;

	if (cond_type != AVS_COND_TYPE_NONE) {
		spin_lock(&adev->path_list_lock);
		list_for_each_entry(pos, &adev->path_list, node) {
			if (pos->template->owner->cond_type != cond_type)
				continue;
			target = dir ? pos->source : pos->sink;
			if (path == target) {
				spin_unlock(&adev->path_list_lock);
				return pos;
			}
		}
		spin_unlock(&adev->path_list_lock);
	}

	return NULL;
}

__maybe_unused
static bool avs_dma_type_is_host(u32 dma_type)
{
	return dma_type == AVS_DMA_HDA_HOST_OUTPUT ||
	       dma_type == AVS_DMA_HDA_HOST_INPUT;
}

__maybe_unused
static bool avs_dma_type_is_link(u32 dma_type)
{
	return !avs_dma_type_is_host(dma_type);
}

__maybe_unused
static bool avs_dma_type_is_output(u32 dma_type)
{
	return dma_type == AVS_DMA_HDA_HOST_OUTPUT ||
	       dma_type == AVS_DMA_HDA_LINK_OUTPUT ||
	       dma_type == AVS_DMA_I2S_LINK_OUTPUT;
}

__maybe_unused
static bool avs_dma_type_is_input(u32 dma_type)
{
	return !avs_dma_type_is_output(dma_type);
}

static int avs_copier_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct acpi_table_nhlt *nhlt = adev->nhlt;
	struct avs_tplg_module *t = mod->template;
	struct avs_copier_cfg *cfg;
	struct nhlt_specific_cfg *ep_blob;
	union avs_connector_node_id node_id = {0};
	size_t cfg_size, data_size = 0;
	void *data = NULL;
	u32 dma_type;
	int ret;

	dma_type = t->cfg_ext->copier.dma_type;
	node_id.dma_type = dma_type;

	switch (dma_type) {
		struct avs_audio_format *fmt;
		int direction;

	case AVS_DMA_I2S_LINK_OUTPUT:
	case AVS_DMA_I2S_LINK_INPUT:
		if (avs_dma_type_is_input(dma_type))
			direction = SNDRV_PCM_STREAM_CAPTURE;
		else
			direction = SNDRV_PCM_STREAM_PLAYBACK;

		if (t->cfg_ext->copier.blob_fmt)
			fmt = t->cfg_ext->copier.blob_fmt;
		else if (direction == SNDRV_PCM_STREAM_CAPTURE)
			fmt = t->in_fmt;
		else
			fmt = t->cfg_ext->copier.out_fmt;

		ep_blob = intel_nhlt_get_endpoint_blob(adev->dev,
			nhlt, t->cfg_ext->copier.vindex.i2s.instance,
			NHLT_LINK_SSP, fmt->valid_bit_depth, fmt->bit_depth,
			fmt->num_channels, fmt->sampling_freq, direction,
			NHLT_DEVICE_I2S);
		if (!ep_blob) {
			dev_err(adev->dev, "no ep_blob found\n");
			return -ENOENT;
		}

		data = ep_blob->caps;
		data_size = ep_blob->size;
		/* I2S gateway's vindex is statically assigned in topology */
		node_id.vindex = t->cfg_ext->copier.vindex.val;

		break;

	case AVS_DMA_DMIC_LINK_INPUT:
		direction = SNDRV_PCM_STREAM_CAPTURE;

		if (t->cfg_ext->copier.blob_fmt)
			fmt = t->cfg_ext->copier.blob_fmt;
		else
			fmt = t->in_fmt;

		ep_blob = intel_nhlt_get_endpoint_blob(adev->dev, nhlt, 0,
				NHLT_LINK_DMIC, fmt->valid_bit_depth,
				fmt->bit_depth, fmt->num_channels,
				fmt->sampling_freq, direction, NHLT_DEVICE_DMIC);
		if (!ep_blob) {
			dev_err(adev->dev, "no ep_blob found\n");
			return -ENOENT;
		}

		data = ep_blob->caps;
		data_size = ep_blob->size;
		/* DMIC gateway's vindex is statically assigned in topology */
		node_id.vindex = t->cfg_ext->copier.vindex.val;

		break;

	case AVS_DMA_HDA_HOST_OUTPUT:
	case AVS_DMA_HDA_HOST_INPUT:
		/* HOST gateway's vindex is dynamically assigned with DMA id */
		node_id.vindex = mod->owner->owner->dma_id;
		break;

	case AVS_DMA_HDA_LINK_OUTPUT:
	case AVS_DMA_HDA_LINK_INPUT:
		node_id.vindex = t->cfg_ext->copier.vindex.val |
				 mod->owner->owner->dma_id;
		break;

	case INVALID_OBJECT_ID:
	default:
		node_id = INVALID_NODE_ID;
		break;
	}

	cfg_size = sizeof(*cfg);
	if (data_size) {
		/* Gateway attributes are path of every config-BLOB. */
		cfg_size -= sizeof(cfg->gtw_cfg.config.attrs);
		cfg_size += data_size;
	}

	cfg = kzalloc(cfg_size, GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->base.cpc = t->cfg_base->cpc;
	cfg->base.ibs = t->cfg_base->ibs;
	cfg->base.obs = t->cfg_base->obs;
	cfg->base.is_pages = t->cfg_base->is_pages;
	cfg->base.audio_fmt = *t->in_fmt;
	cfg->out_fmt = *t->cfg_ext->copier.out_fmt;
	cfg->feature_mask = t->cfg_ext->copier.feature_mask;
	cfg->gtw_cfg.node_id = node_id;
	cfg->gtw_cfg.dma_buffer_size = t->cfg_ext->copier.dma_buffer_size;
	if (data) {
		/* config_length in DWORDs */
		memcpy(&cfg->gtw_cfg.config, data, data_size);
		cfg->gtw_cfg.config_length = data_size / 4;
	}

	mod->gtw_attrs = cfg->gtw_cfg.config.attrs;

	ret = avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				  t->core_id, t->domain, cfg, cfg_size,
				  &mod->instance_id);
	kfree(cfg);
	return ret;
}

static int avs_peakvol_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_peakvol_cfg *cfg;
	struct avs_volume_cfg *vols;
	size_t data_size;
	int ret;

	ret = avs_kcontrol_volume_module_init(mod, &vols, &data_size);
	if (ret)
		return ret;

	cfg = kzalloc(sizeof(*cfg) + data_size, GFP_KERNEL);
	if (!cfg) {
		avs_kcontrol_volume_module_deinit(mod);
		kfree(vols);
		return -ENOMEM;
	}

	cfg->base.cpc = t->cfg_base->cpc;
	cfg->base.ibs = t->cfg_base->ibs;
	cfg->base.obs = t->cfg_base->obs;
	cfg->base.is_pages = t->cfg_base->is_pages;
	cfg->base.audio_fmt = *t->in_fmt;

	memcpy(cfg->vols, vols, data_size);

	ret = avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				  t->core_id, t->domain, cfg,
				  sizeof(*cfg) + data_size, &mod->instance_id);
	if (ret)
		avs_kcontrol_volume_module_deinit(mod);

	kfree(cfg);
	kfree(vols);

	return ret;
}

static int avs_updown_mix_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_updown_mixer_cfg cfg;
	int i;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.out_channel_config = t->cfg_ext->updown_mix.out_channel_config;
	cfg.coefficients_select = t->cfg_ext->updown_mix.coefficients_select;
	for (i = 0; i < AVS_CHANNELS_MAX; i++)
		cfg.coefficients[i] = t->cfg_ext->updown_mix.coefficients[i];
	cfg.channel_map = t->cfg_ext->updown_mix.channel_map;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_src_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_src_cfg cfg;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.out_freq = t->cfg_ext->src.out_freq;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_asrc_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_asrc_cfg cfg;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.out_freq = t->cfg_ext->asrc.out_freq;
	cfg.mode = t->cfg_ext->asrc.mode;
	cfg.disable_jitter_buffer = t->cfg_ext->asrc.disable_jitter_buffer;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_aec_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_aec_cfg cfg;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.ref_fmt = *t->cfg_ext->aec.ref_fmt;
	cfg.out_fmt = *t->cfg_ext->aec.out_fmt;
	cfg.cpc_lp_mode = t->cfg_ext->aec.cpc_lp_mode;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_mux_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_aec_cfg cfg;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.ref_fmt = *t->cfg_ext->mux.ref_fmt;
	cfg.out_fmt = *t->cfg_ext->mux.out_fmt;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_wov_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_wov_cfg cfg;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.cpc_lp_mode = t->cfg_ext->wov.cpc_lp_mode;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_micsel_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_micsel_cfg cfg;

	cfg.base.cpc = t->cfg_base->cpc;
	cfg.base.ibs = t->cfg_base->ibs;
	cfg.base.obs = t->cfg_base->obs;
	cfg.base.is_pages = t->cfg_base->is_pages;
	cfg.base.audio_fmt = *t->in_fmt;
	cfg.out_fmt = *t->cfg_ext->micsel.out_fmt;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_modbase_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_modcfg_base cfg;

	cfg.cpc = t->cfg_base->cpc;
	cfg.ibs = t->cfg_base->ibs;
	cfg.obs = t->cfg_base->obs;
	cfg.is_pages = t->cfg_base->is_pages;
	cfg.audio_fmt = *t->in_fmt;

	return avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				   t->core_id, t->domain, &cfg, sizeof(cfg),
				   &mod->instance_id);
}

static int avs_modext_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	struct avs_tplg_module *t = mod->template;
	struct avs_tplg_modcfg_ext *tcfg = t->cfg_ext;
	struct avs_modcfg_ext *cfg;
	size_t cfg_size, num_pins;
	int ret, i;

	num_pins = tcfg->generic.num_input_pins + tcfg->generic.num_output_pins;
	cfg_size = sizeof(*cfg) + sizeof(*cfg->pin_fmts) * num_pins;

	cfg = kzalloc(cfg_size, GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->base.cpc = t->cfg_base->cpc;
	cfg->base.ibs = t->cfg_base->ibs;
	cfg->base.obs = t->cfg_base->obs;
	cfg->base.is_pages = t->cfg_base->is_pages;
	cfg->base.audio_fmt = *t->in_fmt;
	cfg->num_input_pins = tcfg->generic.num_input_pins;
	cfg->num_output_pins = tcfg->generic.num_output_pins;

	/* configure pin formats */
	for (i = 0; i < num_pins; i++) {
		struct avs_tplg_pin_format *tpin = &tcfg->generic.pin_fmts[i];
		struct avs_pin_format *pin = &cfg->pin_fmts[i];

		pin->pin_index = tpin->pin_index;
		pin->iobs = tpin->iobs;
		pin->audio_fmt = *tpin->fmt;
	}

	ret = avs_dsp_init_module(adev, mod->module_id, mod->owner->instance_id,
				  t->core_id, t->domain, cfg, cfg_size,
				  &mod->instance_id);
	kfree(cfg);
	return ret;
}

static int avs_path_module_type_create(struct avs_dev *adev, struct avs_path_module *mod)
{
	const guid_t *type = &mod->template->cfg_ext->type;

	if (guid_equal(type, &AVS_MIXIN_MOD_UUID) ||
	    guid_equal(type, &AVS_MIXOUT_MOD_UUID) ||
	    guid_equal(type, &AVS_KPBUFF_MOD_UUID))
		return avs_modbase_create(adev, mod);
	if (guid_equal(type, &AVS_COPIER_MOD_UUID))
		return avs_copier_create(adev, mod);
	if (guid_equal(type, &AVS_PEAKVOL_MOD_UUID) ||
	    guid_equal(type, &AVS_GAIN_MOD_UUID))
		return avs_peakvol_create(adev, mod);
	if (guid_equal(type, &AVS_MICSEL_MOD_UUID))
		return avs_micsel_create(adev, mod);
	if (guid_equal(type, &AVS_MUX_MOD_UUID))
		return avs_mux_create(adev, mod);
	if (guid_equal(type, &AVS_UPDWMIX_MOD_UUID))
		return avs_updown_mix_create(adev, mod);
	if (guid_equal(type, &AVS_SRCINTC_MOD_UUID))
		return avs_src_create(adev, mod);
	if (guid_equal(type, &AVS_AEC_MOD_UUID))
		return avs_aec_create(adev, mod);
	if (guid_equal(type, &AVS_ASRC_MOD_UUID))
		return avs_asrc_create(adev, mod);
	if (guid_equal(type, &AVS_INTELWOV_MOD_UUID))
		return avs_wov_create(adev, mod);

	if (guid_equal(type, &AVS_PROBE_MOD_UUID)) {
		dev_err(adev->dev, "Probe module can't be instantiated by topology");
		return -EINVAL;
	}

	return avs_modext_create(adev, mod);
}

static void avs_path_module_free(struct avs_dev *adev, struct avs_path_module *mod)
{
	kobject_put(&mod->kobj);
}

static void avs_path_module_release(struct kobject *kobj)
{
	struct avs_path_module *mod = container_of(kobj, struct avs_path_module, kobj);

	kfree(mod);
}

static ssize_t avs_path_module_fw_id_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct avs_path_module *mod = container_of(kobj, struct avs_path_module, kobj);

	return sprintf(buf, "%d\n", mod->module_id);
}

static struct kobj_attribute avs_path_module_fw_id =
		__ATTR(fw_id, 0444, avs_path_module_fw_id_show, NULL);

static ssize_t avs_path_module_fw_instance_show(struct kobject *kobj,
						struct kobj_attribute *attr, char *buf)
{
	struct avs_path_module *mod = container_of(kobj, struct avs_path_module, kobj);

	return sprintf(buf, "%d\n", mod->instance_id);
}

static struct kobj_attribute avs_path_module_fw_instance =
		__ATTR(fw_instance, 0444, avs_path_module_fw_instance_show, NULL);

static struct attribute *avs_path_module_attrs[] = {
	&avs_path_module_fw_id.attr,
	&avs_path_module_fw_instance.attr,
	NULL,
};

static struct kobj_type avs_path_module_ktype = {
	.release = avs_path_module_release,
	.default_attrs = avs_path_module_attrs,
	.sysfs_ops = &kobj_sysfs_ops,
};

static struct avs_path_module *
avs_path_module_create(struct avs_dev *adev,
		       struct avs_path_pipeline *owner,
		       struct avs_tplg_module *template)
{
	struct avs_path_module *mod;
	int module_id, ret;

	module_id = avs_get_module_id(adev, &template->cfg_ext->type);
	if (module_id < 0)
		return ERR_PTR(module_id);

	mod = kzalloc(sizeof(*mod), GFP_KERNEL);
	if (!mod)
		return ERR_PTR(-ENOMEM);

	mod->template = template;
	mod->module_id = module_id;
	mod->owner = owner;
	INIT_LIST_HEAD(&mod->node);

	ret = avs_path_module_type_create(adev, mod);
	if (ret) {
		dev_err(adev->dev, "module-type create failed: %d\n", ret);
		goto err;
	}

	ret = kobject_init_and_add(&mod->kobj, &avs_path_module_ktype,
				   &owner->kobj, "%d", template->id);
	if (ret) {
		avs_kcontrol_volume_module_deinit(mod);
		goto err;
	}

	return mod;

err:
	kfree(mod);
	return ERR_PTR(ret);
}

static void avs_path_binding_free(struct avs_dev *adev, struct avs_path_binding *binding)
{
	kfree(binding);
}

static struct avs_path_binding *avs_path_binding_create(struct avs_dev *adev,
							struct avs_path_pipeline *owner,
							struct avs_tplg_binding *t)
{
	struct avs_path_binding *binding;

	binding = kzalloc(sizeof(*binding), GFP_KERNEL);
	if (!binding)
		return ERR_PTR(-ENOMEM);

	binding->template = t;
	binding->owner = owner;
	INIT_LIST_HEAD(&binding->node);

	return binding;
}

static int avs_path_binding_arm(struct avs_dev *adev, struct avs_path_binding *binding)
{
	struct avs_path_module *this_mod, *target_mod;
	struct avs_path_pipeline *target_ppl;
	struct avs_path *target_path;
	struct avs_tplg_binding *t;

	t = binding->template;
	this_mod = avs_path_find_module(binding->owner,
					t->mod_id);
	if (!this_mod) {
		dev_err(adev->dev, "path mod %d not found\n", t->mod_id);
		return -EINVAL;
	}

	/* update with target_tplg_name too */
	target_path = avs_path_find_path(adev, t->target_tplg_name,
					 t->target_path_tmpl_id);
	if (!target_path) {
		dev_err(adev->dev, "target path %s:%d not found\n",
			t->target_tplg_name, t->target_path_tmpl_id);
		return -EINVAL;
	}

	target_ppl = avs_path_find_pipeline(target_path,
					    t->target_ppl_id);
	if (!target_ppl) {
		dev_err(adev->dev, "target ppl %d not found\n", t->target_ppl_id);
		return -EINVAL;
	}

	target_mod = avs_path_find_module(target_ppl, t->target_mod_id);
	if (!target_mod) {
		dev_err(adev->dev, "target mod %d not found\n", t->target_mod_id);
		return -EINVAL;
	}

	if (t->is_sink) {
		binding->sink = this_mod;
		binding->sink_pin = t->mod_pin;
		binding->source = target_mod;
		binding->source_pin = t->target_mod_pin;
	} else {
		binding->sink = target_mod;
		binding->sink_pin = t->target_mod_pin;
		binding->source = this_mod;
		binding->source_pin = t->mod_pin;
	}

	return 0;
}

static void avs_path_pipeline_free(struct avs_dev *adev,
				   struct avs_path_pipeline *ppl)
{
	struct avs_path_binding *binding, *bsave;
	struct avs_path_module *mod, *save;

	/* unlink kcontrols from active modules, before we start deleting pipelines */
	list_for_each_entry(mod, &ppl->mod_list, node)
		avs_kcontrol_volume_module_deinit(mod);

	list_for_each_entry_safe(binding, bsave, &ppl->binding_list, node) {
		list_del(&binding->node);
		avs_path_binding_free(adev, binding);
	}

	avs_dsp_delete_pipeline(adev, ppl->instance_id);

	/* Unload resources occupied by owned modules */
	list_for_each_entry_safe(mod, save, &ppl->mod_list, node) {
		avs_dsp_delete_module(adev, mod->module_id, mod->instance_id,
				      mod->owner->instance_id,
				      mod->template->core_id);
		avs_path_module_free(adev, mod);
	}

	list_del(&ppl->node);
	kobject_put(&ppl->kobj);
}

static int avs_path_pipeline_arm(struct avs_dev *adev,
				 struct avs_path_pipeline *ppl)
{
	struct avs_path_module *mod;

	list_for_each_entry(mod, &ppl->mod_list, node) {
		struct avs_path_module *source, *sink;
		int ret;

		/*
		 * Only one module (so it's implicitly last) or it is the last
		 * one, either way we don't have next module to bind it to.
		 */
		if (mod == list_last_entry(&ppl->mod_list,
					   struct avs_path_module, node))
			break;

		/* bind current module to next module on list */
		source = mod;
		sink = list_next_entry(mod, node);
		if (!source || !sink)
			return -EINVAL;

		ret = avs_ipc_bind(adev, source->module_id, source->instance_id,
				   sink->module_id, sink->instance_id, 0, 0);
		if (ret)
			return AVS_IPC_RET(ret);
	}

	return 0;
}

static void avs_path_pipeline_release(struct kobject *kobj)
{
	struct avs_path_pipeline *ppl = container_of(kobj, struct avs_path_pipeline, kobj);

	kfree(ppl);
}

static ssize_t avs_path_pipeline_state_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	struct avs_path_pipeline *ppl = container_of(kobj, struct avs_path_pipeline, kobj);
	struct avs_dev *adev = ppl->owner->owner;
	enum avs_pipeline_state state;
	int ret;

	ret = avs_ipc_get_pipeline_state(adev, ppl->instance_id, &state);
	if (ret) {
		dev_err(adev->dev, "read pipeline state failed: %d\n", ret);
		return AVS_IPC_RET(ret);
	}

	switch (state) {
	case AVS_PPL_STATE_INVALID:
		return sprintf(buf, "invalid\n");
	case AVS_PPL_STATE_UNINITIALIZED:
		return sprintf(buf, "uninitialized\n");
	case AVS_PPL_STATE_RESET:
		return sprintf(buf, "reset\n");
	case AVS_PPL_STATE_PAUSED:
		return sprintf(buf, "paused\n");
	case AVS_PPL_STATE_RUNNING:
		return sprintf(buf, "running\n");
	case AVS_PPL_STATE_EOS:
		return sprintf(buf, "eos\n");
	case AVS_PPL_STATE_ERROR_STOP:
		return sprintf(buf, "error_stop\n");
	case AVS_PPL_STATE_SAVED:
		return sprintf(buf, "saved\n");
	case AVS_PPL_STATE_RESTORED:
		return sprintf(buf, "restored\n");
	default:
		return -EIO;
	}
}

static ssize_t avs_path_pipeline_state_store(struct kobject *kobj,
					     struct kobj_attribute *attr,
					     const char *buf, size_t len)
{
	struct avs_path_pipeline *ppl = container_of(kobj, struct avs_path_pipeline, kobj);
	struct avs_dev *adev = ppl->owner->owner;
	enum avs_pipeline_state state;
	int ret;

	if (sysfs_streq(buf, "reset"))
		state = AVS_PPL_STATE_RESET;
	else if (sysfs_streq(buf, "paused"))
		state = AVS_PPL_STATE_PAUSED;
	else if (sysfs_streq(buf, "running"))
		state = AVS_PPL_STATE_RUNNING;
	else
		return -EINVAL;

	ret = avs_ipc_set_pipeline_state(adev, ppl->instance_id, state);
	if (ret) {
		dev_err(adev->dev, "write pipeline state failed: %d\n", ret);
		return AVS_IPC_RET(ret);
	}

	return len;
}

static struct kobj_attribute avs_path_pipeline_state =
	__ATTR(state, 0664, avs_path_pipeline_state_show, avs_path_pipeline_state_store);

static struct attribute *avs_path_pipeline_attrs[] = {
	&avs_path_pipeline_state.attr,
	NULL,
};

static struct kobj_type avs_path_pipeline_ktype = {
	.release = avs_path_pipeline_release,
	.default_attrs = avs_path_pipeline_attrs,
	.sysfs_ops = &kobj_sysfs_ops,
};

static struct avs_path_pipeline *
avs_path_pipeline_create(struct avs_dev *adev,
			 struct avs_path *owner,
			 struct avs_tplg_pipeline *template)
{
	struct avs_path_pipeline *ppl;
	struct avs_tplg_pplcfg *cfg = template->cfg;
	struct avs_tplg_module *tmod;
	int ret, i;

	ppl = kzalloc(sizeof(*ppl), GFP_KERNEL);
	if (!ppl)
		return ERR_PTR(-ENOMEM);

	ppl->template = template;
	ppl->owner = owner;
	INIT_LIST_HEAD(&ppl->binding_list);
	INIT_LIST_HEAD(&ppl->mod_list);
	INIT_LIST_HEAD(&ppl->node);

	ret = avs_dsp_create_pipeline(adev, cfg->req_size, cfg->priority,
				      cfg->lp, cfg->attributes,
				      &ppl->instance_id);
	if (ret) {
		dev_err(adev->dev, "error creating pipeline %d\n", ret);
		kfree(ppl);
		return ERR_PTR(ret);
	}

	ret = kobject_init_and_add(&ppl->kobj, &avs_path_pipeline_ktype,
				   &owner->kobj, "%d", ppl->template->id);
	if (ret) {
		avs_dsp_delete_pipeline(adev, ppl->instance_id);
		kfree(ppl);
		return ERR_PTR(ret);
	}

	list_for_each_entry(tmod, &template->mod_list, node) {
		struct avs_path_module *mod;

		mod = avs_path_module_create(adev, ppl, tmod);
		if (IS_ERR(mod)) {
			ret = PTR_ERR(mod);
			dev_err(adev->dev, "error creating module %d\n", ret);
			goto init_err;
		}

		list_add_tail(&mod->node, &ppl->mod_list);
	}

	for (i = 0; i < template->num_bindings; i++) {
		struct avs_path_binding *binding;

		binding = avs_path_binding_create(adev, ppl, template->bindings[i]);
		if (IS_ERR(binding)) {
			ret = PTR_ERR(binding);
			dev_err(adev->dev, "error creating binding %d\n", ret);
			goto init_err;
		}

		list_add_tail(&binding->node, &ppl->binding_list);
	}

	return ppl;

init_err:
	avs_path_pipeline_free(adev, ppl);
	return ERR_PTR(ret);
}

static void avs_path_release(struct kobject *kobj)
{
	struct avs_path *path = container_of(kobj, struct avs_path, kobj);

	kfree(path);
}

static struct kobj_type avs_path_ktype = {
	.release = avs_path_release,
};

static int avs_path_init(struct avs_dev *adev, struct avs_path *path,
			 struct avs_tplg_path *template, u32 dma_id)
{
	struct avs_tplg_pipeline *tppl;

	path->owner = adev;
	path->template = template;
	path->dma_id = dma_id;
	INIT_LIST_HEAD(&path->ppl_list);
	INIT_LIST_HEAD(&path->node);
	INIT_LIST_HEAD(&path->source_list);
	INIT_LIST_HEAD(&path->sink_list);
	INIT_LIST_HEAD(&path->source_node);
	INIT_LIST_HEAD(&path->sink_node);

	/* create all the pipelines */
	list_for_each_entry(tppl, &template->ppl_list, node) {
		struct avs_path_pipeline *ppl;

		ppl = avs_path_pipeline_create(adev, path, tppl);
		if (IS_ERR(ppl))
			return PTR_ERR(ppl);

		list_add_tail(&ppl->node, &path->ppl_list);
	}

	spin_lock(&adev->path_list_lock);
	list_add_tail(&path->node, &adev->path_list);
	spin_unlock(&adev->path_list_lock);

	return 0;
}

static int avs_path_arm(struct avs_dev *adev, struct avs_path *path)
{
	struct avs_path_pipeline *ppl;
	struct avs_path_binding *binding;
	int ret;

	list_for_each_entry(ppl, &path->ppl_list, node) {
		/*
		 * Arm all ppl bindings before binding internal modules
		 * as it costs no IPCs which isn't true for the latter.
		 */
		list_for_each_entry(binding, &ppl->binding_list, node) {
			ret = avs_path_binding_arm(adev, binding);
			if (ret < 0)
				return ret;
		}

		ret = avs_path_pipeline_arm(adev, ppl);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void avs_path_free_unlocked(struct avs_path *path)
{
	struct avs_path_pipeline *ppl, *save;

	spin_lock(&path->owner->path_list_lock);
	list_del(&path->node);
	spin_unlock(&path->owner->path_list_lock);

	list_for_each_entry_safe(ppl, save, &path->ppl_list, node)
		avs_path_pipeline_free(path->owner, ppl);

	kobject_put(&path->kobj);
}

static struct avs_path *avs_path_create_unlocked(struct avs_dev *adev, u32 dma_id,
						 struct avs_tplg_path *template,
						 const char *prefix)
{
	struct avs_soc_component *acomp;
	struct avs_path *path;
	int ret;

	acomp = to_avs_soc_component(template->owner->owner->comp);

	path = kzalloc(sizeof(*path), GFP_KERNEL);
	if (!path)
		return ERR_PTR(-ENOMEM);

	ret = kobject_init_and_add(&path->kobj, &avs_path_ktype, acomp->kobj, "%s%d:%d",
				   prefix, template->owner->id, template->id);
	if (ret) {
		kfree(path);
		return ERR_PTR(ret);
	}

	ret = avs_path_init(adev, path, template, dma_id);
	if (ret < 0)
		goto err;

	ret = avs_path_arm(adev, path);
	if (ret < 0)
		goto err;

	path->state = AVS_PPL_STATE_INVALID;
	return path;
err:
	avs_path_free_unlocked(path);
	return ERR_PTR(ret);
}

static void avs_condpath_free(struct avs_dev *adev, struct avs_path *path)
{
	int ret;

	list_del(&path->source_node);
	list_del(&path->sink_node);

	ret = avs_path_reset(path);
	if (ret < 0)
		dev_err(adev->dev, "reset condpath failed: %d\n", ret);

	ret = avs_path_unbind(path);
	if (ret < 0)
		dev_err(adev->dev, "unbind condpath failed: %d\n", ret);

	avs_path_free_unlocked(path);
}

static struct avs_path *avs_condpath_create(struct avs_dev *adev, u32 dma_id,
					    struct avs_tplg_path *template,
					    struct avs_path *source,
					    struct avs_path *sink)
{
	struct avs_path *path;
	int ret;

	/* condpath sysfs files must differ from standard paths. */
	path = avs_path_create_unlocked(adev, dma_id, template, "c");
	if (IS_ERR(path))
		return path;

	ret = avs_path_bind(path);
	if (ret)
		goto err;

	ret = avs_path_reset(path);
	if (ret)
		goto err;

	path->source = source;
	path->sink = sink;
	list_add_tail(&path->source_node, &source->source_list);
	list_add_tail(&path->sink_node, &sink->sink_list);

	return path;
err:
	avs_path_free_unlocked(path);
	return ERR_PTR(ret);
}

static int avs_condpath_walk(struct avs_dev *adev, struct avs_path *path, int dir)
{
	struct avs_tplg_path_template *template;
	struct avs_soc_component *acomp;
	struct avs_tplg_path *variant;
	struct avs_path **other, *conflict;
	struct avs_path *source, *sink;
	struct avs_path *cpath;
	unsigned long type, types = 0;
	int max, i;

	if (dir) {
		source = path;
		other = &sink;
	} else {
		sink = path;
		other = &source;
	}

	/* First create all non-conflicting condpaths. */
	list_for_each_entry(acomp, &adev->comp_list, node) {
		for (i = 0; i < acomp->tplg->num_condpath_tmpls; i++) {
			template = &acomp->tplg->condpath_tmpls[i];

			/* Do not create unidirectional condpaths twice */
			if (avs_tplg_path_template_id_equal(&template->source,
							    &template->sink) && dir)
				continue;

			if (template->cond_type != AVS_COND_TYPE_NONE) {
				/* Save conflicting types to check later on. */
				types |= BIT(template->cond_type);
				continue;
			}

			*other = avs_condpath_find_match(adev, template, path, dir);
			if (!*other)
				continue;
			variant = avs_condpath_find_variant(adev, template, source, sink);
			if (!variant)
				continue;

			cpath = avs_condpath_create(adev, 0, variant, source, sink);
			if (IS_ERR(cpath))
				return PTR_ERR(cpath);
		}
	}
	/* Now deal with exclusive condpaths. */
	for_each_set_bit(type, &types, 32) {
		variant = NULL;
		*other = NULL;

		conflict = avs_condpath_find_conflict(adev, type, path, dir);
		if (conflict) {
			/* Does existing conflict allow for override? */
			if (!conflict->template->owner->overridable)
				continue;
			max = conflict->template->owner->priority;
		} else {
			max = -1;
		}

		/* Find best match - with highest priority. */
		list_for_each_entry(acomp, &adev->comp_list, node) {
			for (i = 0; i < acomp->tplg->num_condpath_tmpls; i++) {
				template = &acomp->tplg->condpath_tmpls[i];

				/* Do not create unidirectional condpaths twice */
				if (avs_tplg_path_template_id_equal(&template->source,
								    &template->sink) && dir)
					continue;

				if (template->cond_type != type || template->priority <= max)
					continue;

				*other = avs_condpath_find_match(adev, template, path, dir);
				if (!*other)
					continue;
				variant = avs_condpath_find_variant(adev, template, source,
								    sink);
				if (variant)
					max = template->priority;
			}
		}

		if (variant) {
			cpath = avs_condpath_create(adev, 0, variant, source, sink);
			if (IS_ERR(cpath))
				return PTR_ERR(cpath);
		}
	}

	return 0;
}

/* caller responsible for holding adev->path_mutex */
static int avs_condpath_walk_all(struct avs_dev *adev, struct avs_path *path)
{
	int ret;

	ret = avs_condpath_walk(adev, path, 1);
	if (ret)
		return ret;

	return avs_condpath_walk(adev, path, 0);
}

struct avs_path *avs_path_create(struct avs_dev *adev, u32 dma_id,
				 struct avs_tplg_path_template *template,
				 struct snd_pcm_hw_params *fe_params,
				 struct snd_pcm_hw_params *be_params)
{
	struct avs_tplg_path *variant;
	struct avs_path *path;
	int ret;

	variant = avs_path_find_variant(adev, template, fe_params, be_params);
	if (!variant) {
		dev_err(adev->dev, "no matching variant found\n");
		return ERR_PTR(-ENOENT);
	}

	mutex_lock(&adev->path_mutex);
	path = avs_path_create_unlocked(adev, dma_id, variant, "");
	if (IS_ERR(path))
		goto exit;

	ret = avs_condpath_walk_all(adev, path);
	if (ret) {
		avs_path_free_unlocked(path);
		path = ERR_PTR(ret);
	}

exit:
	mutex_unlock(&adev->path_mutex);

	return path;
}

void avs_path_free(struct avs_path *path)
{
	struct avs_path *cpath, *csave;
	struct avs_dev *adev = path->owner;

	mutex_lock(&adev->path_mutex);

	/* Free all condpaths this path spawned. */
	list_for_each_entry_safe(cpath, csave, &path->source_list, source_node)
		avs_condpath_free(path->owner, cpath);
	list_for_each_entry_safe(cpath, csave, &path->sink_list, sink_node)
		avs_condpath_free(path->owner, cpath);

	avs_path_free_unlocked(path);

	mutex_unlock(&adev->path_mutex);
}

static int avs_path_bind_prepare(struct avs_dev *adev,
				 struct avs_path_binding *binding)
{
	const struct avs_audio_format *src_fmt, *sink_fmt;
	struct avs_tplg_module *tsource = binding->source->template;
	struct avs_path_module *source = binding->source;
	int ret;

	/*
	 * only copier modules about to be bound
	 * to output pin other than 0 need preparation
	 */
	if (!binding->source_pin)
		return 0;
	if (!guid_equal(&tsource->cfg_ext->type, &AVS_COPIER_MOD_UUID))
		return 0;

	src_fmt = tsource->in_fmt;
	sink_fmt = binding->sink->template->in_fmt;

	ret = avs_ipc_copier_set_sink_format(adev, source->module_id,
					     source->instance_id, binding->source_pin,
					     src_fmt, sink_fmt);
	if (ret) {
		dev_err(adev->dev, "config copier failed: %d\n", ret);
		return AVS_IPC_RET(ret);
	}

	return 0;
}

int avs_path_bind(struct avs_path *path)
{
	struct avs_path_pipeline *ppl;
	struct avs_dev *adev = path->owner;
	int ret;

	list_for_each_entry(ppl, &path->ppl_list, node) {
		struct avs_path_binding *binding;

		list_for_each_entry(binding, &ppl->binding_list, node) {
			struct avs_path_module *source, *sink;

			source = binding->source;
			sink = binding->sink;

			ret = avs_path_bind_prepare(adev, binding);
			if (ret < 0)
				return ret;

			ret = avs_ipc_bind(adev, source->module_id,
					   source->instance_id, sink->module_id,
					   sink->instance_id, binding->sink_pin,
					   binding->source_pin);
			if (ret) {
				dev_err(adev->dev, "bind path failed: %d\n", ret);
				return AVS_IPC_RET(ret);
			}
		}
	}

	return 0;
}

int avs_path_unbind(struct avs_path *path)
{
	struct avs_path_pipeline *ppl;
	struct avs_dev *adev = path->owner;
	int ret;

	list_for_each_entry(ppl, &path->ppl_list, node) {
		struct avs_path_binding *binding;

		list_for_each_entry(binding, &ppl->binding_list, node) {
			struct avs_path_module *source, *sink;

			source = binding->source;
			sink = binding->sink;

			ret = avs_ipc_unbind(adev, source->module_id,
					     source->instance_id, sink->module_id,
					     sink->instance_id, binding->sink_pin,
					     binding->source_pin);
			if (ret) {
				dev_err(adev->dev, "unbind path failed: %d\n", ret);
				return AVS_IPC_RET(ret);
			}
		}
	}

	return 0;
}

int avs_path_reset(struct avs_path *path)
{
	struct avs_path_pipeline *ppl;
	struct avs_dev *adev = path->owner;
	int ret;

	if (path->state == AVS_PPL_STATE_RESET)
		return 0;

	list_for_each_entry(ppl, &path->ppl_list, node) {
		ret = avs_ipc_set_pipeline_state(adev, ppl->instance_id,
						 AVS_PPL_STATE_RESET);
		if (ret) {
			dev_err(adev->dev, "reset path failed: %d\n", ret);
			path->state = AVS_PPL_STATE_INVALID;
			return AVS_IPC_RET(ret);
		}
	}

	path->state = AVS_PPL_STATE_RESET;
	return 0;
}

int avs_path_pause(struct avs_path *path)
{
	struct avs_path_pipeline *ppl;
	struct avs_path *cpath;
	struct avs_dev *adev = path->owner;
	int ret;

	if (path->state == AVS_PPL_STATE_PAUSED)
		return 0;

	/* if either source or sink stops, so do attached conditional paths */
	list_for_each_entry(cpath, &path->source_list, source_node)
		avs_path_pause(cpath);
	list_for_each_entry(cpath, &path->sink_list, sink_node)
		avs_path_pause(cpath);

	list_for_each_entry_reverse(ppl, &path->ppl_list, node) {
		ret = avs_ipc_set_pipeline_state(adev, ppl->instance_id,
						 AVS_PPL_STATE_PAUSED);
		if (ret) {
			dev_err(adev->dev, "pause path failed: %d\n", ret);
			path->state = AVS_PPL_STATE_INVALID;
			return AVS_IPC_RET(ret);
		}
	}

	path->state = AVS_PPL_STATE_PAUSED;
	return 0;
}

int avs_path_run(struct avs_path *path, int trigger)
{
	struct avs_path_pipeline *ppl;
	struct avs_path *cpath;
	struct avs_dev *adev = path->owner;
	int ret;

	if (path->state == AVS_PPL_STATE_RUNNING && trigger == AVS_TPLG_TRIGGER_AUTO)
		return 0;

	list_for_each_entry(ppl, &path->ppl_list, node) {
		if (ppl->template->cfg->trigger != trigger)
			continue;

		ret = avs_ipc_set_pipeline_state(adev, ppl->instance_id,
						 AVS_PPL_STATE_RUNNING);
		if (ret) {
			dev_err(adev->dev, "run path failed: %d\n", ret);
			path->state = AVS_PPL_STATE_INVALID;
			return AVS_IPC_RET(ret);
		}
	}

	path->state = AVS_PPL_STATE_RUNNING;

	/* granular pipeline triggering not intended for conditional paths */
	if (trigger == AVS_TPLG_TRIGGER_AUTO) {
		/* run conditional paths only if source and sink are both running */
		list_for_each_entry(cpath, &path->source_list, source_node)
			if (cpath->source->state == AVS_PPL_STATE_RUNNING &&
			    cpath->sink->state == AVS_PPL_STATE_RUNNING)
				avs_path_run(cpath, trigger);

		list_for_each_entry(cpath, &path->sink_list, sink_node)
			if (cpath->source->state == AVS_PPL_STATE_RUNNING &&
			    cpath->sink->state == AVS_PPL_STATE_RUNNING)
				avs_path_run(cpath, trigger);
	}

	return 0;
}
