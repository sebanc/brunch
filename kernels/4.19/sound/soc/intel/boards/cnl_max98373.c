/*
 *  cnl_max98373.c - ASOC Machine driver for CNL
 *
 *  Copyright (C) 2018 Intel Corp
 *  Author: Harsha Priya <harshapriya.n@intel.com>
 *          Sathyanarayana Nujella <sathyanarayana.nujella@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/acpi.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>

#include "../../codecs/hdac_hdmi.h"
#include "../../codecs/max98373.h"

#define CNL_FREQ_OUT		19200000
#define MAX98373_CODEC_DAI	"max98373-aif1"
#define MAXIM_DEV0_NAME		"i2c-MX98373:00"
#define MAXIM_DEV1_NAME		"i2c-MX98373:01"
#define CNL_NAME_SIZE		32
#define CNL_MAX_HDMI		2

struct cnl_max98373_private {
	int pcm_count;
};

static int cnl_dmic_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	channels->min = channels->max = 2;

	return 0;
}

static const struct snd_kcontrol_new cnl_controls[] = {
	SOC_DAPM_PIN_SWITCH("Left Spk"),
	SOC_DAPM_PIN_SWITCH("Right Spk"),
};

static const struct snd_soc_dapm_widget cnl_widgets[] = {
	SND_SOC_DAPM_SPK("Left Spk", NULL),
	SND_SOC_DAPM_SPK("Right Spk", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};

static const struct snd_soc_dapm_route cnl_map[] = {
	/* speaker */
	{ "Left Spk", NULL, "Left BE_OUT" },
	{ "Right Spk", NULL, "Right BE_OUT" },
	{ "Left HiFi Playback", NULL, "ssp1 Tx" },
	{ "Right HiFi Playback", NULL, "ssp1 Tx" },

	{"ssp1 Tx", NULL, "codec0_out"},

	{"ssp1 Rx", NULL, "HiFi Capture"},
	{"codec0_in", NULL, "ssp1 Rx"},
	/* dmic */
	{"DMic", NULL, "SoC DMIC"},
	{"DMIC01 Rx", NULL, "DMIC AIF"},
	{"dmic01_hifi", NULL, "DMIC01 Rx"},
};

static int cnl_ssp1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *runtime = substream->private_data;
	int ret = 0, j;

	for (j = 0; j < runtime->num_codecs; j++) {
		struct snd_soc_dai *codec_dai = runtime->codec_dais[j];

		if (!strcmp(codec_dai->component->name, MAXIM_DEV0_NAME)) {
			ret = snd_soc_dai_set_tdm_slot(codec_dai, 0x30, 3, 8, 16);
			if (ret < 0) {
				dev_err(runtime->dev, "DEV0 TDM slot err:%d\n", ret);
			return ret;
			}
		}
		if (!strcmp(codec_dai->component->name, MAXIM_DEV1_NAME)) {
			ret = snd_soc_dai_set_tdm_slot(codec_dai, 0xC0, 3, 8, 16);
			if (ret < 0) {
				dev_err(runtime->dev, "DEV1 TDM slot err:%d\n", ret);
			return ret;
			}
		}
	}

	return 0;
}

static struct snd_soc_ops cnl_ssp1_ops = {
	.hw_params = cnl_ssp1_hw_params,
};

static int cnl_be_fixup(struct snd_soc_pcm_runtime *rtd,
			    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S16_LE);
	return 0;
}
static struct snd_soc_codec_conf max98373_codec_conf[] = {

	{
		.dev_name = MAXIM_DEV0_NAME,
		.name_prefix = "Right",
	},

	{
		.dev_name = MAXIM_DEV1_NAME,
		.name_prefix = "Left",
	},
};

static struct snd_soc_dai_link_component ssp0_codec_components[] = {
	{ /* Left */
		.name = MAXIM_DEV0_NAME,
		.dai_name = MAX98373_CODEC_DAI,
	},

	{  /* For Right */
		.name = MAXIM_DEV1_NAME,
		.dai_name = MAX98373_CODEC_DAI,
	},

};

struct snd_soc_dai_link cnl_dailink[] = {
	{
		.name = "dmic01",
		.id = 2,
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = "0000:00:1f.3",
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_hw_params_fixup = cnl_dmic_fixup,
	},
	{
		.name = "SSP1-Codec",
		.id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.codecs = ssp0_codec_components,
		.num_codecs = ARRAY_SIZE(ssp0_codec_components),
		.platform_name = "0000:00:1f.3",
		.be_hw_params_fixup = cnl_be_fixup,
		.ignore_pmdown_time = 1,
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_DSP_B |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &cnl_ssp1_ops,
	},
};

static int
cnl_add_dai_link(struct snd_soc_card *card, struct snd_soc_dai_link *link)
{
	struct cnl_max98373_private *ctx = snd_soc_card_get_drvdata(card);

	link->platform_name = "0000:00:1f.3";
	link->nonatomic = 1;

	ctx->pcm_count++;

	return 0;
}

/* SoC card */
static struct snd_soc_card snd_soc_card_cnl = {
	.name = "cnlmax98373",
	.owner = THIS_MODULE,
	.dai_link = cnl_dailink,
	.num_links = ARRAY_SIZE(cnl_dailink),
	.dapm_widgets = cnl_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cnl_widgets),
	.dapm_routes = cnl_map,
	.num_dapm_routes = ARRAY_SIZE(cnl_map),
	.controls = cnl_controls,
	.num_controls = ARRAY_SIZE(cnl_controls),
	.add_dai_link = cnl_add_dai_link,
	.codec_conf = max98373_codec_conf,
	.num_configs = ARRAY_SIZE(max98373_codec_conf),
	.fully_routed = true,
};

static int snd_cnl_max98373_mc_probe(struct platform_device *pdev)
{
	struct cnl_max98373_private *ctx;
	int res = 0;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->pcm_count = ARRAY_SIZE(cnl_dailink);

	snd_soc_card_cnl.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_cnl, ctx);
	res = devm_snd_soc_register_card(&pdev->dev, &snd_soc_card_cnl);
	return res;
}


static const struct platform_device_id cnl_board_ids[] = {
	{ .name = "cnl_max98373" },
	{ }
};

static struct platform_driver snd_cnl_max98373_driver = {
	.driver = {
		.name = "cnl_max98373",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_cnl_max98373_mc_probe,
	.id_table = cnl_board_ids,
};

module_platform_driver(snd_cnl_max98373_driver);

MODULE_AUTHOR("Harha Priya <harshapriya.n@intel.com>");
MODULE_AUTHOR("Sathyanarayana Nujella <sathyanarayana.nujella@intel.com>");
MODULE_AUTHOR("Sathya Prakash M R <sathya.prakash.m.r@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cnl_max98373");
