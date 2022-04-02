// SPDX-License-Identifier: GPL-2.0
// Copyright(c) 2019-2020 Intel Corporation.

/*
 * Intel SOF Machine Driver with Realtek rt5682 Codec
 * and speaker codec MAX98357A or RT1015.
 */
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/dmi.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/rt5682.h>
#include <sound/rt5682s.h>
#include <sound/soc-acpi.h>
#include "../../codecs/rt1015.h"
#include "../../codecs/rt5682.h"
#include "../../codecs/rt5682s.h"
#include "../../codecs/hdac_hdmi.h"
#include "../common/soc-intel-quirks.h"

#define NAME_SIZE 32

#define SOF_RT5682_SSP_CODEC(quirk)		((quirk) & GENMASK(2, 0))
#define SOF_RT5682_SSP_CODEC_MASK			(GENMASK(2, 0))
#define SOF_RT5682_MCLK_EN			BIT(3)
#define SOF_RT5682_MCLK_24MHZ			BIT(4)
#define SOF_SPEAKER_AMP_PRESENT		BIT(5)
#define SOF_RT5682_SSP_AMP_SHIFT		6
#define SOF_RT5682_SSP_AMP_MASK                 (GENMASK(8, 6))
#define SOF_RT5682_SSP_AMP(quirk)	\
	(((quirk) << SOF_RT5682_SSP_AMP_SHIFT) & SOF_RT5682_SSP_AMP_MASK)
#define SOF_RT5682_MCLK_BYTCHT_EN		BIT(9)
#define SOF_RT5682_NUM_HDMIDEV_SHIFT		10
#define SOF_RT5682_NUM_HDMIDEV_MASK		(GENMASK(12, 10))
#define SOF_RT5682_NUM_HDMIDEV(quirk)	\
	((quirk << SOF_RT5682_NUM_HDMIDEV_SHIFT) & SOF_RT5682_NUM_HDMIDEV_MASK)
#define SOF_RT1015_SPEAKER_AMP_PRESENT		BIT(13)
#define SOF_RT1015_SPEAKER_AMP_100FS		BIT(14)
#define SOF_RT5682S_HEADPHONE_CODEC_PRESENT	BIT(23)

/* Default: MCLK on, MCLK 19.2M, SSP0  */
static unsigned long sof_rt5682_quirk = SOF_RT5682_MCLK_EN |
					SOF_RT5682_SSP_CODEC(0);

static int is_legacy_cpu;

static struct snd_soc_jack sof_hdmi[3];

struct sof_hdmi_pcm {
	struct list_head head;
	struct snd_soc_dai *codec_dai;
	int device;
};

struct sof_card_private {
	struct clk *mclk;
	struct snd_soc_jack sof_headset;
	struct list_head hdmi_pcm_list;
};

static int sof_rt5682_quirk_cb(const struct dmi_system_id *id)
{
	sof_rt5682_quirk = (unsigned long)id->driver_data;
	return 1;
}

static const struct dmi_system_id sof_rt5682_quirk_table[] = {
	{
		.callback = sof_rt5682_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Circuitco"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Minnowboard Max"),
		},
		.driver_data = (void *)(SOF_RT5682_SSP_CODEC(2)),
	},
	{
		.callback = sof_rt5682_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "AAEON"),
			DMI_MATCH(DMI_PRODUCT_NAME, "UP-CHT01"),
		},
		.driver_data = (void *)(SOF_RT5682_SSP_CODEC(2)),
	},
	{
		.callback = sof_rt5682_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "WhiskeyLake Client"),
		},
		.driver_data = (void *)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_MCLK_24MHZ |
					SOF_RT5682_SSP_CODEC(1)),
	},
	{
		.callback = sof_rt5682_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Dooly"),
		},
		.driver_data = (void *)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_MCLK_24MHZ |
					SOF_RT5682_SSP_CODEC(0) |
					SOF_SPEAKER_AMP_PRESENT |
					SOF_RT1015_SPEAKER_AMP_PRESENT |
					SOF_RT1015_SPEAKER_AMP_100FS |
					SOF_RT5682_SSP_AMP(1)),
	},
	{
		.callback = sof_rt5682_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_PRODUCT_FAMILY, "Google_Hatch"),
		},
		.driver_data = (void *)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_MCLK_24MHZ |
					SOF_RT5682_SSP_CODEC(0) |
					SOF_SPEAKER_AMP_PRESENT |
					SOF_RT5682_SSP_AMP(1)),
	},
	{
		.callback = sof_rt5682_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Ice Lake Client"),
		},
		.driver_data = (void *)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_SSP_CODEC(0)),
	},
	{}
};

static int sof_hdmi_init(struct snd_soc_pcm_runtime *rtd)
{
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *dai = rtd->codec_dai;
	struct snd_soc_dai_link *dai_link;
	struct sof_hdmi_pcm *pcm;
	int hdmi_id;
	char hdmi_name[16];

	pcm = devm_kzalloc(rtd->card->dev, sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	if (sscanf(rtd->dai_link->name, "iDisp%d", &hdmi_id) != 1) {
		dev_err(rtd->dev, "failed to get hdmi id\n");
		return -ENODEV;
	}

	/* dai_link id is 1:1 mapped to the PCM device */
	snprintf(hdmi_name, sizeof(hdmi_name), "HDMI%d", hdmi_id);

	for_each_card_links(rtd->card, dai_link) {
		if (dai_link->name && !strcmp(hdmi_name, dai_link->name)) {
			pcm->device = dai_link->id;
			pcm->codec_dai = dai;

			list_add_tail(&pcm->head, &ctx->hdmi_pcm_list);
			return 0;
		}
	}

	dev_err(rtd->dev, "failed to find dai link of HDMI%d\n", hdmi_id);
	return -ENODEV;
}

static int sof_rt5682_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_component *component = rtd->codec_dai->component;
	struct snd_soc_jack *jack;
	int ret;

	/* need to enable ASRC function for 24MHz mclk rate */
	if ((sof_rt5682_quirk & SOF_RT5682_MCLK_EN) &&
	    (sof_rt5682_quirk & SOF_RT5682_MCLK_24MHZ)) {
		if (sof_rt5682_quirk & SOF_RT5682S_HEADPHONE_CODEC_PRESENT)
			rt5682s_sel_asrc_clk_src(component,
						 RT5682S_DA_STEREO1_FILTER |
						 RT5682S_AD_STEREO1_FILTER,
						 RT5682S_CLK_SEL_I2S1_ASRC);
		else
			rt5682_sel_asrc_clk_src(component,
						RT5682_DA_STEREO1_FILTER |
						RT5682_AD_STEREO1_FILTER,
						RT5682_CLK_SEL_I2S1_ASRC);
	}

	if (sof_rt5682_quirk & SOF_RT5682_MCLK_BYTCHT_EN) {
		/*
		 * The firmware might enable the clock at
		 * boot (this information may or may not
		 * be reflected in the enable clock register).
		 * To change the rate we must disable the clock
		 * first to cover these cases. Due to common
		 * clock framework restrictions that do not allow
		 * to disable a clock that has not been enabled,
		 * we need to enable the clock first.
		 */
		ret = clk_prepare_enable(ctx->mclk);
		if (!ret)
			clk_disable_unprepare(ctx->mclk);

		ret = clk_set_rate(ctx->mclk, 19200000);

		if (ret)
			dev_err(rtd->dev, "unable to set MCLK rate\n");
	}

	/*
	 * Headset buttons map to the google Reference headset.
	 * These can be configured by userspace.
	 */
	ret = snd_soc_card_jack_new(rtd->card, "Headset Jack",
				    SND_JACK_HEADSET | SND_JACK_BTN_0 |
				    SND_JACK_BTN_1 | SND_JACK_BTN_2 |
				    SND_JACK_BTN_3,
				    &ctx->sof_headset, NULL, 0);
	if (ret) {
		dev_err(rtd->dev, "Headset Jack creation failed: %d\n", ret);
		return ret;
	}

	jack = &ctx->sof_headset;

	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);
	ret = snd_soc_component_set_jack(component, jack, NULL);

	if (ret) {
		dev_err(rtd->dev, "Headset Jack call-back failed: %d\n", ret);
		return ret;
	}

	return ret;
};

static int sof_rt5682_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int pll_id, pll_source, pll_in, pll_out, clk_id, ret;

	if (sof_rt5682_quirk & SOF_RT5682_MCLK_EN) {
		if (sof_rt5682_quirk & SOF_RT5682_MCLK_BYTCHT_EN) {
			ret = clk_prepare_enable(ctx->mclk);
			if (ret < 0) {
				dev_err(rtd->dev,
					"could not configure MCLK state");
				return ret;
			}
		}

		if (sof_rt5682_quirk & SOF_RT5682S_HEADPHONE_CODEC_PRESENT)
			pll_source = RT5682S_PLL_S_MCLK;
		else
			pll_source = RT5682_PLL1_S_MCLK;

		if (sof_rt5682_quirk & SOF_RT5682_MCLK_24MHZ)
			pll_in = 24000000;
		else
			pll_in = 19200000;
	} else {
		if (sof_rt5682_quirk & SOF_RT5682S_HEADPHONE_CODEC_PRESENT)
			pll_source = RT5682S_PLL_S_BCLK1;
		else
			pll_source = RT5682_PLL1_S_BCLK1;

		pll_in = params_rate(params) * 50;
	}

	if (sof_rt5682_quirk & SOF_RT5682S_HEADPHONE_CODEC_PRESENT) {
		pll_id = RT5682S_PLL2;
		clk_id = RT5682S_SCLK_S_PLL2;
	} else {
		pll_id = RT5682_PLL1;
		clk_id = RT5682_SCLK_S_PLL1;
	}

	pll_out = params_rate(params) * 512;

	/* Configure pll for codec */
	ret = snd_soc_dai_set_pll(codec_dai, pll_id, pll_source, pll_in,
				  pll_out);
	if (ret < 0)
		dev_err(rtd->dev, "snd_soc_dai_set_pll err = %d\n", ret);

	/* Configure sysclk for codec */
	ret = snd_soc_dai_set_sysclk(codec_dai, clk_id,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(rtd->dev, "snd_soc_dai_set_sysclk err = %d\n", ret);

	/*
	 * slot_width should equal or large than data length, set them
	 * be the same
	 */
	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0x0, 0x0, 2,
				       params_width(params));
	if (ret < 0) {
		dev_err(rtd->dev, "set TDM slot err:%d\n", ret);
		return ret;
	}

	return ret;
}

static struct snd_soc_ops sof_rt5682_ops = {
	.hw_params = sof_rt5682_hw_params,
};

static int sof_rt1015_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai;
	int i, fs, ret;

	if (!snd_soc_card_get_codec_dai(card, "rt1015-aif"))
		return 0;

	if (sof_rt5682_quirk & SOF_RT1015_SPEAKER_AMP_100FS)
		fs = 100;
	else
		fs = 64;

	for_each_rtd_codec_dai(rtd, i, codec_dai) {
		ret = snd_soc_dai_set_pll(codec_dai, 0, RT1015_PLL_S_BCLK,
					  params_rate(params) * fs,
					  params_rate(params) * 256);
		if (ret < 0) {
			dev_err(card->dev, "failed to set pll\n");
			return ret;
		}
		/* Configure sysclk for codec */
		ret = snd_soc_dai_set_sysclk(codec_dai, RT1015_SCLK_S_PLL,
					     params_rate(params) * 256,
					     SND_SOC_CLOCK_IN);
		if (ret < 0) {
			dev_err(card->dev, "failed to set sysclk\n");
			return ret;
		}

		if (sof_rt5682_quirk & SOF_RT1015_SPEAKER_AMP_100FS) {
			if (!strcmp(codec_dai->component->name, "i2c-10EC1015:00")) {
				ret = snd_soc_dai_set_tdm_slot(codec_dai,
							       0x0, 0x1, 4, 24);
				if (ret < 0) {
					dev_err(card->dev, "failed to set tdm slot\n");
					return ret;
				}
			}

			if (!strcmp(codec_dai->component->name, "i2c-10EC1015:01")) {
				ret = snd_soc_dai_set_tdm_slot(codec_dai,
							       0x0, 0x2, 4, 24);
				if (ret < 0) {
					dev_err(card->dev, "failed to set tdm slot\n");
					return ret;
				}
			}
		}
	}

	return 0;
}

static struct snd_soc_ops sof_rt1015_ops = {
	.hw_params = sof_rt1015_hw_params,
};

static struct snd_soc_dai_link_component platform_component[] = {
	{
		/* name might be overridden during probe */
		.name = "0000:00:1f.3"
	}
};

static int sof_card_late_probe(struct snd_soc_card *card)
{
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(card);
	struct snd_soc_component *component = NULL;
	char jack_name[NAME_SIZE];
	struct sof_hdmi_pcm *pcm;
	int err = 0;
	int i = 0;

	/* HDMI is not supported by SOF on Baytrail/CherryTrail */
	if (is_legacy_cpu)
		return 0;

	list_for_each_entry(pcm, &ctx->hdmi_pcm_list, head) {
		component = pcm->codec_dai->component;
		snprintf(jack_name, sizeof(jack_name),
			 "HDMI/DP, pcm=%d Jack", pcm->device);
		err = snd_soc_card_jack_new(card, jack_name,
					    SND_JACK_AVOUT, &sof_hdmi[i],
					    NULL, 0);

		if (err)
			return err;

		err = hdac_hdmi_jack_init(pcm->codec_dai, pcm->device,
					  &sof_hdmi[i]);
		if (err < 0)
			return err;

		i++;
	}
	if (!component)
		return -EINVAL;

	return hdac_hdmi_jack_port_init(component, &card->dapm);
}

static const struct snd_kcontrol_new sof_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Spk"),
	SOC_DAPM_PIN_SWITCH("Left Spk"),
	SOC_DAPM_PIN_SWITCH("Right Spk"),

};

static const struct snd_soc_dapm_widget sof_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_SPK("Spk", NULL),
	SND_SOC_DAPM_SPK("Left Spk", NULL),
	SND_SOC_DAPM_SPK("Right Spk", NULL),
};

static const struct snd_soc_dapm_widget dmic_widgets[] = {
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};

static const struct snd_soc_dapm_route sof_map[] = {
	/* HP jack connectors - unknown if we have jack detection */
	{ "Headphone Jack", NULL, "HPOL" },
	{ "Headphone Jack", NULL, "HPOR" },

	/* other jacks */
	{ "IN1P", NULL, "Headset Mic" },
};

static const struct snd_soc_dapm_route speaker_map[] = {
	/* speaker */
	{ "Spk", NULL, "Speaker" },
};

static const struct snd_soc_dapm_route speaker_map_lr[] = {
	{ "Left Spk", NULL, "Left SPO" },
	{ "Right Spk", NULL, "Right SPO" },
};

static const struct snd_soc_dapm_route dmic_map[] = {
	/* digital mics */
	{"DMic", NULL, "SoC DMIC"},
};

static int speaker_codec_init_lr(struct snd_soc_pcm_runtime *rtd)
{
	return snd_soc_dapm_add_routes(&rtd->card->dapm, speaker_map_lr,
				       ARRAY_SIZE(speaker_map_lr));
}

static int speaker_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int ret;

	ret = snd_soc_dapm_add_routes(&card->dapm, speaker_map,
				      ARRAY_SIZE(speaker_map));

	if (ret)
		dev_err(rtd->dev, "Speaker map addition failed: %d\n", ret);
	return ret;
}

static int dmic_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int ret;

	ret = snd_soc_dapm_new_controls(&card->dapm, dmic_widgets,
					ARRAY_SIZE(dmic_widgets));
	if (ret) {
		dev_err(card->dev, "DMic widget addition failed: %d\n", ret);
		/* Don't need to add routes if widget addition failed */
		return ret;
	}

	ret = snd_soc_dapm_add_routes(&card->dapm, dmic_map,
				      ARRAY_SIZE(dmic_map));

	if (ret)
		dev_err(card->dev, "DMic map addition failed: %d\n", ret);

	return ret;
}

static struct snd_soc_codec_conf rt1015_amp_conf[] = {
	{
		.dev_name = "i2c-10EC1015:00",
		.name_prefix = "Left",
	},
	{
		.dev_name = "i2c-10EC1015:01",
		.name_prefix = "Right",
	},
};

/* sof audio machine driver for rt5682 codec */
static struct snd_soc_card sof_audio_card_rt5682 = {
	.name = "sof_rt5682",
	.owner = THIS_MODULE,
	.controls = sof_controls,
	.num_controls = ARRAY_SIZE(sof_controls),
	.dapm_widgets = sof_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sof_widgets),
	.dapm_routes = sof_map,
	.num_dapm_routes = ARRAY_SIZE(sof_map),
	.fully_routed = true,
	.late_probe = sof_card_late_probe,
};

static struct snd_soc_dai_link_component rt5682_component[] = {
	{
		.name = "i2c-10EC5682:00",
		.dai_name = "rt5682-aif1",
	}
};

static struct snd_soc_dai_link_component rt5682s_component[] = {
	{
		.name = "i2c-RTL5682:00",
		.dai_name = "rt5682s-aif1",
	}
};

static struct snd_soc_dai_link_component dmic_component[] = {
	{
		.name = "dmic-codec",
		.dai_name = "dmic-hifi",
	}
};

static struct snd_soc_dai_link_component max98357a_component[] = {
	{
		.name = "MX98357A:00",
		.dai_name = "HiFi",
	}
};

static struct snd_soc_dai_link_component rt1015_components[] = {
	{
		.name = "i2c-10EC1015:00",
		.dai_name = "rt1015-aif",
	},
	{
		.name = "i2c-10EC1015:01",
		.dai_name = "rt1015-aif",
	},
};

static struct snd_soc_dai_link *sof_card_dai_links_create(struct device *dev,
							  int ssp_codec,
							  int ssp_amp,
							  int dmic_be_num,
							  int hdmi_num)
{
	struct snd_soc_dai_link_component *idisp_components;
	struct snd_soc_dai_link_component *cpus;
	struct snd_soc_dai_link *links;
	int i, id = 0;

	links = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link) *
			     sof_audio_card_rt5682.num_links, GFP_KERNEL);
	cpus = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link_component) *
			     sof_audio_card_rt5682.num_links, GFP_KERNEL);
	if (!links || !cpus)
		goto devm_err;

	/* codec SSP */
	links[id].name = devm_kasprintf(dev, GFP_KERNEL,
					"SSP%d-Codec", ssp_codec);
	if (!links[id].name)
		goto devm_err;

	links[id].id = id;
	if (sof_rt5682_quirk & SOF_RT5682S_HEADPHONE_CODEC_PRESENT) {
		links[id].codecs = rt5682s_component;
		links[id].num_codecs = ARRAY_SIZE(rt5682s_component);
	} else {
		links[id].codecs = rt5682_component;
		links[id].num_codecs = ARRAY_SIZE(rt5682_component);
	}
	links[id].platforms = platform_component;
	links[id].num_platforms = ARRAY_SIZE(platform_component);
	links[id].init = sof_rt5682_codec_init;
	links[id].ops = &sof_rt5682_ops;
	links[id].nonatomic = true;
	links[id].dpcm_playback = 1;
	links[id].dpcm_capture = 1;
	links[id].no_pcm = 1;
	links[id].cpus = &cpus[id];
	links[id].num_cpus = 1;
	if (is_legacy_cpu) {
		links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
							  "ssp%d-port",
							  ssp_codec);
		if (!links[id].cpus->dai_name)
			goto devm_err;
	} else {
		/*
		 * Currently, On SKL+ platforms MCLK will be turned off in sof
		 * runtime suspended, and it will go into runtime suspended
		 * right after playback is stop. However, rt5682 will output
		 * static noise if sysclk turns off during playback. Set
		 * ignore_pmdown_time to power down rt5682 immediately and
		 * avoid the noise.
		 * It can be removed once we can control MCLK by driver.
		 */
		links[id].ignore_pmdown_time = 1;
		links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
							  "SSP%d Pin",
							  ssp_codec);
		if (!links[id].cpus->dai_name)
			goto devm_err;
	}
	id++;

	/* dmic */
	if (dmic_be_num > 0) {
		/* at least we have dmic01 */
		links[id].name = "dmic01";
		links[id].cpus = &cpus[id];
		links[id].cpus->dai_name = "DMIC01 Pin";
		links[id].init = dmic_init;
		if (dmic_be_num > 1) {
			/* set up 2 BE links at most */
			links[id + 1].name = "dmic16k";
			links[id + 1].cpus = &cpus[id + 1];
			links[id + 1].cpus->dai_name = "DMIC16k Pin";
			dmic_be_num = 2;
		}
	}

	for (i = 0; i < dmic_be_num; i++) {
		links[id].id = id;
		links[id].num_cpus = 1;
		links[id].codecs = dmic_component;
		links[id].num_codecs = ARRAY_SIZE(dmic_component);
		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].ignore_suspend = 1;
		links[id].dpcm_capture = 1;
		links[id].no_pcm = 1;
		id++;
	}

	/* HDMI */
	if (hdmi_num > 0) {
		idisp_components = devm_kzalloc(dev,
				   sizeof(struct snd_soc_dai_link_component) *
				   hdmi_num, GFP_KERNEL);
		if (!idisp_components)
			goto devm_err;
	}
	for (i = 1; i <= hdmi_num; i++) {
		links[id].name = devm_kasprintf(dev, GFP_KERNEL,
						"iDisp%d", i);
		if (!links[id].name)
			goto devm_err;

		links[id].id = id;
		links[id].cpus = &cpus[id];
		links[id].num_cpus = 1;
		links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
							  "iDisp%d Pin", i);
		if (!links[id].cpus->dai_name)
			goto devm_err;

		idisp_components[i - 1].name = "ehdaudio0D2";
		idisp_components[i - 1].dai_name = devm_kasprintf(dev,
								  GFP_KERNEL,
								  "intel-hdmi-hifi%d",
								  i);
		if (!idisp_components[i - 1].dai_name)
			goto devm_err;

		links[id].codecs = &idisp_components[i - 1];
		links[id].num_codecs = 1;
		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].init = sof_hdmi_init;
		links[id].dpcm_playback = 1;
		links[id].no_pcm = 1;
		id++;
	}

	/* speaker amp */
	if (sof_rt5682_quirk & SOF_SPEAKER_AMP_PRESENT) {
		links[id].name = devm_kasprintf(dev, GFP_KERNEL,
						"SSP%d-Codec", ssp_amp);
		if (!links[id].name)
			goto devm_err;

		links[id].id = id;
		if (sof_rt5682_quirk & SOF_RT1015_SPEAKER_AMP_PRESENT) {
			links[id].codecs = rt1015_components;
			links[id].num_codecs = ARRAY_SIZE(rt1015_components);
			links[id].init = speaker_codec_init_lr;
			links[id].ops = &sof_rt1015_ops;
		} else {
			links[id].codecs = max98357a_component;
			links[id].num_codecs = ARRAY_SIZE(max98357a_component);
			links[id].init = speaker_codec_init;
		}
		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].nonatomic = true;
		links[id].dpcm_playback = 1;
		links[id].no_pcm = 1;
		links[id].cpus = &cpus[id];
		links[id].num_cpus = 1;
		if (is_legacy_cpu) {
			links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
								  "ssp%d-port",
								  ssp_amp);
			if (!links[id].cpus->dai_name)
				goto devm_err;

		} else {
			links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
								  "SSP%d Pin",
								  ssp_amp);
			if (!links[id].cpus->dai_name)
				goto devm_err;
		}
	}

	return links;
devm_err:
	return NULL;
}

static int sof_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_dai_link *dai_links;
	struct snd_soc_acpi_mach *mach;
	struct sof_card_private *ctx;
	int dmic_be_num, hdmi_num;
	int ret, ssp_amp, ssp_codec;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (pdev->id_entry && pdev->id_entry->driver_data)
		sof_rt5682_quirk = (unsigned long)pdev->id_entry->driver_data;

	dmi_check_system(sof_rt5682_quirk_table);

	if (soc_intel_is_byt() || soc_intel_is_cht()) {
		is_legacy_cpu = 1;
		dmic_be_num = 0;
		hdmi_num = 0;
		/* default quirk for legacy cpu */
		sof_rt5682_quirk = SOF_RT5682_MCLK_EN |
						SOF_RT5682_MCLK_BYTCHT_EN |
						SOF_RT5682_SSP_CODEC(2);
	} else {
		dmic_be_num = 2;
		hdmi_num = (sof_rt5682_quirk & SOF_RT5682_NUM_HDMIDEV_MASK) >>
			 SOF_RT5682_NUM_HDMIDEV_SHIFT;
		/* default number of HDMI DAI's */
		if (!hdmi_num)
			hdmi_num = 3;
	}

	mach = (&pdev->dev)->platform_data;

	/* A speaker amp might not be present when the quirk claims one is.
	 * Detect this via whether the machine driver match includes quirk_data.
	 */
	if ((sof_rt5682_quirk & SOF_SPEAKER_AMP_PRESENT) && !mach->quirk_data)
		sof_rt5682_quirk &= ~SOF_SPEAKER_AMP_PRESENT;

	/* Detect the headset codec variant to support machines in DMI quirk */
	if (acpi_dev_present("RTL5682", NULL, -1))
		sof_rt5682_quirk |= SOF_RT5682S_HEADPHONE_CODEC_PRESENT;

	/* need to get main clock from pmc */
	if (sof_rt5682_quirk & SOF_RT5682_MCLK_BYTCHT_EN) {
		ctx->mclk = devm_clk_get(&pdev->dev, "pmc_plt_clk_3");
		ret = clk_prepare_enable(ctx->mclk);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"could not configure MCLK state");
			return ret;
		}
	}

	dev_dbg(&pdev->dev, "sof_rt5682_quirk = %lx\n", sof_rt5682_quirk);

	ssp_amp = (sof_rt5682_quirk & SOF_RT5682_SSP_AMP_MASK) >>
			SOF_RT5682_SSP_AMP_SHIFT;

	ssp_codec = sof_rt5682_quirk & SOF_RT5682_SSP_CODEC_MASK;

	/* compute number of dai links */
	sof_audio_card_rt5682.num_links = 1 + dmic_be_num + hdmi_num;

	if (sof_rt5682_quirk & SOF_SPEAKER_AMP_PRESENT)
		sof_audio_card_rt5682.num_links++;

	dai_links = sof_card_dai_links_create(&pdev->dev, ssp_codec, ssp_amp,
					      dmic_be_num, hdmi_num);
	if (!dai_links)
		return -ENOMEM;

	sof_audio_card_rt5682.dai_link = dai_links;

	if (sof_rt5682_quirk & SOF_RT1015_SPEAKER_AMP_PRESENT) {
		sof_audio_card_rt5682.codec_conf = rt1015_amp_conf;
		sof_audio_card_rt5682.num_configs = ARRAY_SIZE(rt1015_amp_conf);
	}

	INIT_LIST_HEAD(&ctx->hdmi_pcm_list);

	sof_audio_card_rt5682.dev = &pdev->dev;

	/* set platform name for each dailink */
	ret = snd_soc_fixup_dai_links_platform_name(&sof_audio_card_rt5682,
						    mach->mach_params.platform);
	if (ret)
		return ret;

	snd_soc_card_set_drvdata(&sof_audio_card_rt5682, ctx);

	return devm_snd_soc_register_card(&pdev->dev,
					  &sof_audio_card_rt5682);
}

static int sof_rt5682_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct snd_soc_component *component = NULL;

	for_each_card_components(card, component) {
		if (!strcmp(component->name, rt5682_component[0].name)) {
			snd_soc_component_set_jack(component, NULL, NULL);
			break;
		}
	}

	return 0;
}

static const struct platform_device_id board_ids[] = {
	{
		.name = "sof_rt5682",
	},
	{
		.name = "tgl_max98357a_rt5682",
		.driver_data = (kernel_ulong_t)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_SSP_CODEC(0) |
					SOF_SPEAKER_AMP_PRESENT |
					SOF_RT5682_SSP_AMP(1) |
					SOF_RT5682_NUM_HDMIDEV(4)),
	},
	{
		.name = "jsl_rt5682_rt1015",
		.driver_data = (kernel_ulong_t)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_MCLK_24MHZ |
					SOF_RT5682_SSP_CODEC(0) |
					SOF_SPEAKER_AMP_PRESENT |
					SOF_RT1015_SPEAKER_AMP_PRESENT |
					SOF_RT5682_SSP_AMP(1)),
	},
	{
		.name = "cml_rt1015_rt5682",
		.driver_data = (kernel_ulong_t)(SOF_RT5682_MCLK_EN |
					SOF_RT5682_MCLK_24MHZ |
					SOF_RT5682_SSP_CODEC(0) |
					SOF_SPEAKER_AMP_PRESENT |
					SOF_RT1015_SPEAKER_AMP_PRESENT |
					SOF_RT1015_SPEAKER_AMP_100FS |
					SOF_RT5682_SSP_AMP(1)),
	},
	{ }
};

static struct platform_driver sof_audio = {
	.probe = sof_audio_probe,
	.remove = sof_rt5682_remove,
	.driver = {
		.name = "sof_rt5682",
		.pm = &snd_soc_pm_ops,
	},
	.id_table = board_ids,
};
module_platform_driver(sof_audio)

/* Module information */
MODULE_DESCRIPTION("SOF Audio Machine driver");
MODULE_AUTHOR("Bard Liao <bard.liao@intel.com>");
MODULE_AUTHOR("Sathya Prakash M R <sathya.prakash.m.r@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sof_rt5682");
MODULE_ALIAS("platform:tgl_max98357a_rt5682");
MODULE_ALIAS("platform:jsl_rt5682_rt1015");
MODULE_ALIAS("platform:cml_rt1015_rt5682");
