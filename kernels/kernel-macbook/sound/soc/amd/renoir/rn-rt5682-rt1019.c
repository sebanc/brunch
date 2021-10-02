// SPDX-License-Identifier: GPL-2.0+
//
// Machine driver for AMD ACP Audio engine using RT5682 & RT1015P codec.
//
//Copyright 2021 Advanced Micro Devices, Inc.

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/acpi.h>
#include <linux/dmi.h>

#include "rn_acp3x.h"
#include "../../codecs/rt5682.h"
#include "../../codecs/rt1019.h"

#define PCO_PLAT_CLK 48000000
#define RT5682_PLL_FREQ (48000 * 512)
#define DUAL_CHANNEL		2
#define FOUR_CHANNEL		4

#define RT1019_DEV0_DEV1	1
#define RT1019_DEV1_DEV2	2

static struct snd_soc_jack pco_jack;
static struct clk *rt5682_dai_wclk;
static struct clk *rt5682_dai_bclk;

void *acp_rn_soc_is_rltk(struct device *dev);

static const struct dmi_system_id rn_quirk_table[] = {
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Google"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Guybrush"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "rev1"),
		},
	},
	{},
};

static int acp3x_rn_5682_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_component *component = codec_dai->component;

	dev_info(rtd->dev, "codec dai name = %s\n", codec_dai->name);

	/* set rt5682 dai fmt */
	ret =  snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(rtd->card->dev,
			"Failed to set rt5682 dai fmt: %d\n", ret);
		return ret;
	}

	/* set codec PLL */
	ret = snd_soc_dai_set_pll(codec_dai, RT5682_PLL2, RT5682_PLL2_S_MCLK,
				  PCO_PLAT_CLK, RT5682_PLL_FREQ);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set rt5682 PLL: %d\n", ret);
		return ret;
	}

	/* Set codec sysclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5682_SCLK_S_PLL2,
				     RT5682_PLL_FREQ, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(rtd->dev,
			"Failed to set rt5682 SYSCLK: %d\n", ret);
		return ret;
	}

	/* Set tdm/i2s1 master bclk ratio */
	ret = snd_soc_dai_set_bclk_ratio(codec_dai, 64);
	if (ret < 0) {
		dev_err(rtd->dev,
			"Failed to set rt5682 tdm bclk ratio: %d\n", ret);
		return ret;
	}

	rt5682_dai_wclk = clk_get(component->dev, "rt5682-dai-wclk");
	rt5682_dai_bclk = clk_get(component->dev, "rt5682-dai-bclk");

	ret = snd_soc_card_jack_new(card, "Headset Jack",
				    SND_JACK_HEADSET | SND_JACK_LINEOUT |
				    SND_JACK_BTN_0 | SND_JACK_BTN_1 |
				    SND_JACK_BTN_2 | SND_JACK_BTN_3,
				    &pco_jack, NULL, 0);
	if (ret) {
		dev_err(card->dev, "HP jack creation failed %d\n", ret);
		return ret;
	}

	snd_jack_set_key(pco_jack.jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(pco_jack.jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(pco_jack.jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(pco_jack.jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	ret = snd_soc_component_set_jack(component, &pco_jack, NULL);
	if (ret) {
		dev_err(rtd->dev, "Headset Jack call-back failed: %d\n", ret);
		return ret;
	}

	return ret;
}

static int rn_rt5682_clk_enable(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);

	/* RT5682 will support only 48K output with 48M mclk */
	clk_set_rate(rt5682_dai_wclk, 48000);
	clk_set_rate(rt5682_dai_bclk, 48000 * 64);
	ret = clk_prepare_enable(rt5682_dai_wclk);
	if (ret < 0) {
		dev_err(rtd->dev, "can't enable wclk %d\n", ret);
		return ret;
	}

	return ret;
}

static int acp3x_rn_1019_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai;
	int srate, i, ret;

	ret = 0;
	srate = params_rate(params);

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		if (strcmp(codec_dai->name, "rt1019-aif"))
			continue;

		ret = snd_soc_dai_set_pll(codec_dai, 0, RT1019_PLL_S_BCLK,
					  64 * srate, 256 * srate);
		if (ret < 0)
			return ret;
		ret = snd_soc_dai_set_sysclk(codec_dai, RT1019_SCLK_S_PLL,
					     256 * srate, SND_SOC_CLOCK_IN);
		if (ret < 0)
			return ret;
	}
	return ret;
}

static void rn_rt5682_clk_disable(void)
{
	clk_disable_unprepare(rt5682_dai_wclk);
}

static const unsigned int channels[] = {
	DUAL_CHANNEL,
};

static const unsigned int rates[] = {
	48000,
};

static const struct snd_pcm_hw_constraint_list constraints_rates = {
	.count = ARRAY_SIZE(rates),
	.list  = rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_channels = {
	.count = ARRAY_SIZE(channels),
	.list = channels,
	.mask = 0,
};

static const unsigned int dmic_channels[] = {
	DUAL_CHANNEL, FOUR_CHANNEL,
};

static const struct snd_pcm_hw_constraint_list dmic_constraints_channels = {
	.count = ARRAY_SIZE(dmic_channels),
	.list = dmic_channels,
	.mask = 0,
};

static int acp3x_rn_5682_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_card *card = rtd->card;
	struct acp3x_platform_info *machine = snd_soc_card_get_drvdata(card);

	machine->play_i2s_instance = I2S_SP_INSTANCE;
	machine->cap_i2s_instance = I2S_SP_INSTANCE;

	runtime->hw.channels_max = DUAL_CHANNEL;
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
				   &constraints_channels);
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				   &constraints_rates);
	return rn_rt5682_clk_enable(substream);
}

static int acp3x_rn_rt1019_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_card *card = rtd->card;
	struct acp3x_platform_info *machine = snd_soc_card_get_drvdata(card);

	machine->play_i2s_instance = I2S_SP_INSTANCE;

	runtime->hw.channels_max = DUAL_CHANNEL;
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
				   &constraints_channels);
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				   &constraints_rates);
	return rn_rt5682_clk_enable(substream);
}

static int acp3x_rn_dmic_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_card *card = rtd->card;

	if (!strcmp(card->name, "acp3xalc56821019"))
		runtime->hw.channels_max = FOUR_CHANNEL;
	else
		runtime->hw.channels_max = DUAL_CHANNEL;
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
				   &dmic_constraints_channels);
	return 0;
}

static void rn_rt5682_shutdown(struct snd_pcm_substream *substream)
{
	rn_rt5682_clk_disable();
}

static const struct snd_soc_ops acp3x_rn_5682_ops = {
	.startup = acp3x_rn_5682_startup,
	.shutdown = rn_rt5682_shutdown,
};

static const struct snd_soc_ops acp3x_rn_rt1019_play_ops = {
	.startup = acp3x_rn_rt1019_startup,
	.shutdown = rn_rt5682_shutdown,
	.hw_params = acp3x_rn_1019_hw_params,
};

static const struct snd_soc_ops acp3x_rn_dmic_ops = {
	.startup = acp3x_rn_dmic_startup,
};

SND_SOC_DAILINK_DEF(acp_pdm,
		    DAILINK_COMP_ARRAY(COMP_CPU("acp_rn_pdm_dma.1")));

SND_SOC_DAILINK_DEF(dmic_codec,
		    DAILINK_COMP_ARRAY(COMP_CODEC("dmic-codec.0",
						  "dmic-hifi")));

SND_SOC_DAILINK_DEF(pdm_platform,
		    DAILINK_COMP_ARRAY(COMP_PLATFORM("acp_rn_pdm_dma.1")));

SND_SOC_DAILINK_DEF(acp3x_i2s,
		    DAILINK_COMP_ARRAY(COMP_CPU("acp_i2s_playcap.0")));

SND_SOC_DAILINK_DEF(i2s_platform,
		    DAILINK_COMP_ARRAY(COMP_PLATFORM("acp_rn_i2s_dma.0")));

SND_SOC_DAILINK_DEF(rt5682,
		    DAILINK_COMP_ARRAY(COMP_CODEC("i2c-10EC5682:00", "rt5682-aif1")));

SND_SOC_DAILINK_DEF(rt1019,
	DAILINK_COMP_ARRAY(COMP_CODEC("i2c-10EC1019:00", "rt1019-aif"),
			COMP_CODEC("i2c-10EC1019:01", "rt1019-aif")));

SND_SOC_DAILINK_DEF(rt1019_1,
	DAILINK_COMP_ARRAY(COMP_CODEC("i2c-10EC1019:01", "rt1019-aif"),
			COMP_CODEC("i2c-10EC1019:02", "rt1019-aif")));

static struct snd_soc_codec_conf rt1019_conf[] = {
	{
		.dlc = COMP_CODEC_CONF("i2c-10EC1019:00"),
		.name_prefix = "Left",
	},
	{
		.dlc = COMP_CODEC_CONF("i2c-10EC1019:01"),
		.name_prefix = "Right",
	},
};

static struct snd_soc_codec_conf rt1019_conf_1[] = {
	{
		.dlc = COMP_CODEC_CONF("i2c-10EC1019:01"),
		.name_prefix = "Left",
	},
	{
		.dlc = COMP_CODEC_CONF("i2c-10EC1019:02"),
		.name_prefix = "Right",
	},
};

static const struct snd_soc_dapm_widget acp3x_rn_1019_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_SPK("Left Spk", NULL),
	SND_SOC_DAPM_SPK("Right Spk", NULL),
};

static const struct snd_soc_dapm_route acp3x_rn_1019_route[] = {
	{"Headphone Jack", NULL, "HPOL"},
	{"Headphone Jack", NULL, "HPOR"},
	{"IN1P", NULL, "Headset Mic"},
	/* speaker */
	{"Left Spk", NULL, "Left SPO"},
	{"Right Spk", NULL, "Right SPO"},
};

static const struct snd_kcontrol_new acp3x_rn_mc_1019_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Left Spk"),
	SOC_DAPM_PIN_SWITCH("Right Spk"),
};

static struct snd_soc_card acp3x_rn_5682_1019 = {
	.name = "acp3xalc56821019",
	.owner = THIS_MODULE,
	.dapm_widgets = acp3x_rn_1019_widgets,
	.num_dapm_widgets = ARRAY_SIZE(acp3x_rn_1019_widgets),
	.dapm_routes = acp3x_rn_1019_route,
	.num_dapm_routes = ARRAY_SIZE(acp3x_rn_1019_route),
	.controls = acp3x_rn_mc_1019_controls,
	.num_controls = ARRAY_SIZE(acp3x_rn_mc_1019_controls),
};

static struct snd_soc_card acp3x_rn_dmic_5682_1019 = {
	.name = "acp3xsingledmic56821019",
	.owner = THIS_MODULE,
	.dapm_widgets = acp3x_rn_1019_widgets,
	.num_dapm_widgets = ARRAY_SIZE(acp3x_rn_1019_widgets),
	.dapm_routes = acp3x_rn_1019_route,
	.num_dapm_routes = ARRAY_SIZE(acp3x_rn_1019_route),
	.controls = acp3x_rn_mc_1019_controls,
	.num_controls = ARRAY_SIZE(acp3x_rn_mc_1019_controls),
};


static struct snd_soc_dai_link acp3x_rn_5682_dai[] = {
	{
		.name = "acp3x-5682-play",
		.stream_name = "Playback",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM,
		.init = acp3x_rn_5682_init,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &acp3x_rn_5682_ops,
		SND_SOC_DAILINK_REG(acp3x_i2s, rt5682, i2s_platform),
	},
};

static const struct snd_soc_dapm_widget acp3x_rn_5682_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
};

static const struct snd_soc_dapm_route acp3x_rn_5682_route[] = {
	{"Headphone Jack", NULL, "HPOL"},
	{"Headphone Jack", NULL, "HPOR"},
	{"IN1P", NULL, "Headset Mic"},
};

static const struct snd_kcontrol_new acp3x_rn_5682_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static struct snd_soc_card acp3x_rn_5682 = {
	.name = "acp3xalc5682",
	.owner = THIS_MODULE,
	.dai_link = acp3x_rn_5682_dai,
	.num_links = ARRAY_SIZE(acp3x_rn_5682_dai),
	.dapm_widgets = acp3x_rn_5682_widgets,
	.num_dapm_widgets = ARRAY_SIZE(acp3x_rn_5682_widgets),
	.dapm_routes = acp3x_rn_5682_route,
	.num_dapm_routes = ARRAY_SIZE(acp3x_rn_5682_route),
	.controls = acp3x_rn_5682_controls,
	.num_controls = ARRAY_SIZE(acp3x_rn_5682_controls),
};

void *acp_rn_soc_is_rltk(struct device *dev)
{
	const struct acpi_device_id *match;

	match = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!match)
		return NULL;
	return (void *)match->driver_data;
}

static int acp_card_dai_links_create(struct snd_soc_card *card)
{
	struct snd_soc_dai_link *links;
	struct device *dev = card->dev;
	int num_links = 3;

	links = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link) *
			     num_links, GFP_KERNEL);

	/* Initialize Headset codec dai link if defined */
	links[0].name = "acp3x-5682-play";
	links[0].stream_name = "Playback";
	links[0].dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM;
	links[0].codecs = rt5682;
	links[0].num_codecs = ARRAY_SIZE(rt5682);
	links[0].cpus = acp3x_i2s;
	links[0].num_cpus = ARRAY_SIZE(acp3x_i2s);
	links[0].platforms = i2s_platform;
	links[0].num_platforms = ARRAY_SIZE(i2s_platform);
	links[0].ops = &acp3x_rn_5682_ops;
	links[0].init = acp3x_rn_5682_init;
	links[0].dpcm_playback = 1;
	links[0].dpcm_capture = 1;

	/* Initialize Amp codec dai link if defined */
	links[1].name = "acp3x-rt1019-play";
	links[1].stream_name = "HiFi Playback";
	links[1].dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS;
	if (dmi_check_system(rn_quirk_table)) {
		links[1].codecs = rt1019;
		links[1].num_codecs = ARRAY_SIZE(rt1019);
	} else {
		links[1].codecs = rt1019_1;
		links[1].num_codecs = ARRAY_SIZE(rt1019_1);
	}
	links[1].cpus = acp3x_i2s;
	links[1].num_cpus = ARRAY_SIZE(acp3x_i2s);
	links[1].platforms = i2s_platform;
	links[1].num_platforms = ARRAY_SIZE(i2s_platform);
	links[1].ops = &acp3x_rn_rt1019_play_ops;
	links[1].dpcm_playback = 1;

	/* Initialize Dmic codec dai link if defined */
	links[2].name = "acp3x-dmic-capture";
	links[2].stream_name = "DMIC capture";
	links[2].cpus = acp_pdm;
	links[2].num_cpus = ARRAY_SIZE(acp_pdm);
	links[2].codecs = dmic_codec;
	links[2].num_codecs = ARRAY_SIZE(dmic_codec);
	links[2].platforms = pdm_platform;
	links[2].num_platforms = ARRAY_SIZE(pdm_platform);
	links[2].capture_only = 1;
	links[2].ops = &acp3x_rn_dmic_ops;

	if (dmi_check_system(rn_quirk_table)) {
		card->codec_conf = rt1019_conf;
		card->num_configs = ARRAY_SIZE(rt1019_conf);
	} else {
		card->codec_conf = rt1019_conf_1;
		card->num_configs = ARRAY_SIZE(rt1019_conf_1);
	}

	card->dai_link = links;
	card->num_links = num_links;

	return 0;
}

static int acp3x_rn_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card;
	struct acp3x_platform_info *machine;
	struct device *dev = &pdev->dev;

	card = (struct snd_soc_card *)acp_rn_soc_is_rltk(dev);
	if (!card)
		return -ENODEV;

	machine = devm_kzalloc(&pdev->dev, sizeof(*machine), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;
	pr_err("%s entry\n", __func__);
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	ret = acp_card_dai_links_create(card);
	if (ret)
		return -EINVAL;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"devm_snd_soc_register_card(%s) failed: %d\n",
				card->name, ret);
		else
			dev_dbg(&pdev->dev,
				"devm_snd_soc_register_card(%s) probe deferred: %d\n",
				card->name, ret);
	}
	pr_err("%s exit\n", __func__);
	return ret;
}

static const struct acpi_device_id acp3x_rn_audio_acpi_match[] = {
	{ "AMDI1019", (unsigned long)&acp3x_rn_5682_1019},
	{ "10021019", (unsigned long)&acp3x_rn_dmic_5682_1019},
	{ "10025682", (unsigned long)&acp3x_rn_5682},
	{},
};
MODULE_DEVICE_TABLE(acpi, acp3x_rn_audio_acpi_match);
static struct platform_driver acp3x_rn_audio = {
	.driver = {
		.name = "acp3x-alc5682-rt1019",
		.acpi_match_table = ACPI_PTR(acp3x_rn_audio_acpi_match),
		.pm = &snd_soc_pm_ops,
	},
	.probe = acp3x_rn_probe,
};

module_platform_driver(acp3x_rn_audio);

MODULE_AUTHOR("Vijendar.Mukunda@amd.com");
MODULE_DESCRIPTION("ALC5682, ALC1015P & DMIC audio support");
MODULE_LICENSE("GPL v2");
