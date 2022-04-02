/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Authors: Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
 *          Cezary Rojewski <cezary.rojewski@intel.com>
 */

#ifndef __SOUND_SOC_INTEL_AVS_KCTRL_H
#define __SOUND_SOC_INTEL_AVS_KCTRL_H

#include <sound/pcm.h> // sound/soc-dapm.h needs it...
#include <sound/soc-dapm.h>

struct avs_kcontrol_volume_data {
	struct avs_dev *adev;
	struct avs_path_module *active_module;
	int channels;
	long volume[AVS_CHANNELS_MAX];
};

struct snd_kcontrol *
avs_kcontrol_volume_register(struct avs_dev *adev,
			     struct snd_soc_dapm_widget *widget, int id,
			     int count, int max_channels);
int avs_kcontrol_volume_module_init(struct avs_path_module *module,
				    struct avs_volume_cfg **vols,
				    size_t *vols_size);
int avs_kcontrol_volume_module_deinit(struct avs_path_module *module);

#endif
