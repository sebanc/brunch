// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Authors: Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
//          Cezary Rojewski <cezary.rojewski@intel.com>
//

#include <sound/soc.h>
#include <uapi/sound/tlv.h>
#include "avs.h"
#include "kcontrol.h"
#include "messages.h"
#include "path.h"

#define DSP_VOLUME_MAX		S32_MAX /* 0db */
#define DSP_VOLUME_STEP_MAX	30

static u32 ctlvol_to_dspvol(u32 value)
{
	if (value > DSP_VOLUME_STEP_MAX)
		value = 0;
	return DSP_VOLUME_MAX >> (DSP_VOLUME_STEP_MAX - value);
}

static u32 dspvol_to_ctlvol(u32 volume)
{
	if (volume > DSP_VOLUME_MAX)
		return DSP_VOLUME_STEP_MAX;
	return volume ? __fls(volume) : 0;
}

static int avs_kcontrol_volume_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	struct avs_kcontrol_volume_data *data = kcontrol->private_data;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = data->channels;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = DSP_VOLUME_STEP_MAX;

	return 0;
}

static int avs_kcontrol_volume_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct avs_kcontrol_volume_data *data = kcontrol->private_data;
	struct avs_path_module *active_module;
	struct avs_volume_cfg *dspvols = NULL;
	size_t num_dspvols;
	int ret, i = 0;

	/* prevent access to modules while path is being constructed */
	mutex_lock(&data->adev->path_mutex);

	active_module = data->active_module;
	if (active_module) {
		ret = avs_ipc_peakvol_get_volume(data->adev,
						 active_module->module_id,
						 active_module->instance_id,
						 &dspvols, &num_dspvols);
		if (ret) {
			mutex_unlock(&data->adev->path_mutex);
			return AVS_IPC_RET(ret);
		}

		for ( ; i < num_dspvols; i++)
			ucontrol->value.integer.value[i] =
				dspvol_to_ctlvol(dspvols[i].target_volume);
	}
	for ( ; i < data->channels; i++)
		ucontrol->value.integer.value[i] = data->volume[i];

	mutex_unlock(&data->adev->path_mutex);

	kfree(dspvols);
	return 0;
}

static int avs_kcontrol_volume_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct avs_kcontrol_volume_data *data = kcontrol->private_data;
	struct avs_path_module *active_module;
	struct avs_volume_cfg dspvol = {0};
	long *ctlvol = ucontrol->value.integer.value;
	int channels_max;
	int i, ret = 0, changed = 0;

	/* prevent access to modules while path is being constructed */
	mutex_lock(&data->adev->path_mutex);

	active_module = data->active_module;
	if (active_module)
		channels_max = active_module->template->in_fmt->num_channels;
	else
		channels_max = data->channels;

	for (i = 1; i < channels_max; i++)
		if (data->volume[i] != ctlvol[i])
			changed = 1;

	if (!changed) {
		mutex_unlock(&data->adev->path_mutex);
		return 0;
	}

	memcpy(data->volume, ctlvol, sizeof(*ctlvol) * channels_max);

	for (i = 1; i < channels_max; i++)
		if (data->volume[i] != data->volume[0])
			break;

	if (i == channels_max) {
		dspvol.channel_id = AVS_ALL_CHANNELS_MASK;
		dspvol.target_volume = ctlvol_to_dspvol(data->volume[0]);

		if (active_module)
			ret = avs_ipc_peakvol_set_volume(data->adev,
							 active_module->module_id,
							 active_module->instance_id,
							 &dspvol);
	} else {
		for (i = 0; i < channels_max; i++) {
			dspvol.channel_id = i;
			dspvol.target_volume = ctlvol_to_dspvol(data->volume[i]);

			if (active_module)
				ret = avs_ipc_peakvol_set_volume(data->adev,
								 active_module->module_id,
								 active_module->instance_id,
								 &dspvol);
			if (ret)
				break;
			memset(&dspvol, 0, sizeof(dspvol));
		}
	}

	mutex_unlock(&data->adev->path_mutex);

	return ret ? AVS_IPC_RET(ret) : 1;
}

static const SNDRV_CTL_TLVD_DECLARE_DB_SCALE(avs_kcontrol_volume_tlv, -9000, 300, 1);

static struct snd_kcontrol_new avs_kcontrol_volume_template = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = avs_kcontrol_volume_info,
	.get = avs_kcontrol_volume_get,
	.put = avs_kcontrol_volume_put,
	.tlv.p = avs_kcontrol_volume_tlv,
};

/*
 * avs_kcontrol_volume_register() - register volume kcontrol
 * @widget: Widget to which kcontrol belongs
 * @id: Which kcontrol it is
 * @count: If there is more than one
 * @max_channels: Maximum number of channels that will be in use
 */
struct snd_kcontrol *
avs_kcontrol_volume_register(struct avs_dev *adev,
			     struct snd_soc_dapm_widget *widget, int id,
			     int count, int max_channels)
{
	struct avs_kcontrol_volume_data *data;
	struct snd_kcontrol_new *kctrl_tmpl;
	struct snd_kcontrol *kctrl;
	int ret, i;

	kctrl_tmpl = kmemdup(&avs_kcontrol_volume_template, sizeof(*kctrl_tmpl), GFP_KERNEL);
	if (!kctrl_tmpl)
		return ERR_PTR(-ENOMEM);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	data->adev = adev;
	data->channels = max_channels;
	/* Set default volume to maximum, so we don't get users asking, why there is no sound */
	for (i = 0; i < max_channels; i++)
		data->volume[i] = dspvol_to_ctlvol(DSP_VOLUME_MAX);

	/*
	 * There can be one or more volume kontrols, if there is one we just name it
	 * "%s Volume", but if there is more, we need to number them properly.
	 */
	if (count == 1)
		kctrl_tmpl->name = kasprintf(GFP_KERNEL, "%s DSP Volume", widget->name);
	else
		kctrl_tmpl->name = kasprintf(GFP_KERNEL, "%s DSP Volume%d", widget->name, id);

	if (!kctrl_tmpl->name) {
		kctrl = ERR_PTR(-ENOMEM);
		goto err_kasprintf;
	}

	kctrl = snd_ctl_new1(kctrl_tmpl, data);
	if (!kctrl)
		goto err_ctl_new1;

	ret = snd_ctl_add(widget->dapm->card->snd_card, kctrl);
	if (ret) {
		kctrl = ERR_PTR(ret);
		goto err_ctl_new1;
	}

	return kctrl;

err_ctl_new1:
	kfree(kctrl_tmpl->name);
err_kasprintf:
	kfree(kctrl_tmpl);

	return kctrl;
}

/*
 * avs_kcontrol_volume_module_init_fill_cfg_data() - Fills up data for module init IPC
 * @module: module for which kcontrol is being used
 * @vols: will point to data, needs to be freed by caller
 * @vols_size: will return data size in bytes
 */
int avs_kcontrol_volume_module_init(struct avs_path_module *module,
				    struct avs_volume_cfg **vols,
				    size_t *vols_size)
{
	struct avs_kcontrol_volume_data *data = module->template->kctrl->private_data;
	struct avs_volume_cfg *cd;
	int channels_max = module->template->in_fmt->num_channels;
	int i;

	for (i = 1; i < channels_max; i++)
		if (data->volume[i] != data->volume[0])
			break;

	if (i == channels_max)
		*vols_size = sizeof(*cd);
	else
		*vols_size = sizeof(*cd) * channels_max;

	cd = kzalloc(*vols_size, GFP_KERNEL);
	if (!cd)
		return -ENOMEM;

	if (i == channels_max) {
		cd[0].channel_id = AVS_ALL_CHANNELS_MASK;
		cd[0].target_volume = ctlvol_to_dspvol(data->volume[0]);
		cd[0].curve_type = AVS_AUDIO_CURVE_NONE;
		cd[0].curve_duration = 0;
	} else {
		for (i = 0; i < channels_max; i++) {
			cd[i].channel_id = i;
			cd[i].target_volume = ctlvol_to_dspvol(data->volume[i]);
			cd[i].curve_type = AVS_AUDIO_CURVE_NONE;
			cd[i].curve_duration = 0;
		}
	}

	*vols = cd;
	data->active_module = module;

	return 0;
}

/* avs_kcontrol_volume_module_deinit() - Sets active module to null, should be
 * used before freeing pipelines, so we are in "working" state
 * @module: module for which kcontrol is being used
 */
int avs_kcontrol_volume_module_deinit(struct avs_path_module *module)
{
	struct avs_kcontrol_volume_data *data;
	struct snd_kcontrol *kcontrol = module->template->kctrl;

	if (kcontrol) {
		data = kcontrol->private_data;
		data->active_module = NULL;
	}

	return 0;
}
