// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2021 Intel Corporation.

/*
 * Intel SOF Machine Driver with speaker amplifier and no headphone codec
 */
#include <linux/module.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include "../../codecs/hdac_hdmi.h"
#include "hda_dsp_common.h"
#include "sof_cirrus_common.h"

#define NAME_SIZE 32

#define SOF_SSP_AMP_SHIFT		0
#define SOF_SSP_AMP_MASK		(GENMASK(2, 0))
#define SOF_SSP_AMP(quirk)	\
	(((quirk) << SOF_SSP_AMP_SHIFT) & SOF_SSP_AMP_MASK)
#define SOF_SSP_BT_SHIFT		3
#define SOF_SSP_BT_MASK			(GENMASK(5, 3))
#define SOF_SSP_BT(quirk)	\
	(((quirk) << SOF_SSP_BT_SHIFT) & SOF_SSP_BT_MASK)
#define SOF_NUM_HDMI_SHIFT		6
#define SOF_NUM_HDMI_MASK		(GENMASK(8, 6))
#define SOF_NUM_HDMI(quirk)	\
	(((quirk) << SOF_NUM_HDMI_SHIFT) & SOF_NUM_HDMI_MASK)
#define SOF_TYPE_AMP_SHIFT		9
#define SOF_TYPE_AMP_MASK		(GENMASK(11, 9))
#define SOF_TYPE_AMP(quirk)	\
	(((quirk) << SOF_TYPE_AMP_SHIFT) & SOF_TYPE_AMP_MASK)

#define SOF_BT_OFFLOAD_PRESENT		BIT(12)
#define SOF_SPEAKER_AMP_PRESENT		BIT(13)

enum {
	AMP_CS35L41 = 1,
};

/* Default: SSP1 */
static unsigned long sof_amp_only_quirk = SOF_SSP_AMP(1);

struct sof_hdmi_pcm {
	struct list_head head;
	struct snd_soc_dai *codec_dai;
	struct snd_soc_jack hdmi_jack;
	int device;
};

struct sof_card_private {
	struct list_head hdmi_pcm_list;
	bool common_hdmi_codec_drv;
};

/* DMIC */
static const struct snd_soc_dapm_widget dmic_widgets[] = {
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};

static const struct snd_soc_dapm_route dmic_routes[] = {
	/* digital mics */
	{"DMic", NULL, "SoC DMIC"},
};

static struct snd_soc_dai_link_component dmic_component[] = {
	{
		.name = "dmic-codec",
		.dai_name = "dmic-hifi",
	}
};

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

	ret = snd_soc_dapm_add_routes(&card->dapm, dmic_routes,
				      ARRAY_SIZE(dmic_routes));

	if (ret)
		dev_err(card->dev, "DMic map addition failed: %d\n", ret);

	return ret;
}

/* HDMI */
static int sof_hdmi_init(struct snd_soc_pcm_runtime *rtd)
{
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *dai = asoc_rtd_to_codec(rtd, 0);
	struct sof_hdmi_pcm *pcm;

	pcm = devm_kzalloc(rtd->card->dev, sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	/* dai_link id is 1:1 mapped to the PCM device */
	pcm->device = rtd->dai_link->id;
	pcm->codec_dai = dai;

	list_add_tail(&pcm->head, &ctx->hdmi_pcm_list);

	return 0;
}

/* BT offload */
static struct snd_soc_dai_link_component dummy_component[] = {
	{
		.name = "snd-soc-dummy",
		.dai_name = "snd-soc-dummy-dai",
	}
};

/* soundcard */
static int sof_card_late_probe(struct snd_soc_card *card)
{
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(card);
	struct snd_soc_component *component = NULL;
	char jack_name[NAME_SIZE];
	struct sof_hdmi_pcm *pcm;
	int err;

	if (list_empty(&ctx->hdmi_pcm_list))
		return -EINVAL;

	if (ctx->common_hdmi_codec_drv) {
		pcm = list_first_entry(&ctx->hdmi_pcm_list, struct sof_hdmi_pcm,
				       head);
		component = pcm->codec_dai->component;
		return hda_dsp_hdmi_build_controls(card, component);
	}

	list_for_each_entry(pcm, &ctx->hdmi_pcm_list, head) {
		component = pcm->codec_dai->component;
		snprintf(jack_name, sizeof(jack_name), "HDMI/DP, pcm=%d Jack",
			 pcm->device);
		err = snd_soc_card_jack_new(card, jack_name, SND_JACK_AVOUT,
					    &pcm->hdmi_jack, NULL, 0);
		if (err)
			return err;

		err = hdac_hdmi_jack_init(pcm->codec_dai, pcm->device,
					  &pcm->hdmi_jack);
		if (err < 0)
			return err;
	}

	return hdac_hdmi_jack_port_init(component, &card->dapm);
}

static struct snd_soc_card sof_amp_only_audio_card = {
	.name = "amp_only", /* the sof- prefix is added by the core */
	.owner = THIS_MODULE,
	.fully_routed = true,
	.late_probe = sof_card_late_probe,
};

/* probe */
static struct snd_soc_dai_link_component platform_component[] = {
	{
		/* name might be overridden during probe */
		.name = "0000:00:1f.3"
	}
};

static struct snd_soc_dai_link *sof_card_dai_links_create(struct device *dev,
							  int dmic_be_num,
							  int hdmi_num)
{
	struct snd_soc_dai_link_component *cpus, *idisps;
	struct snd_soc_dai_link *links;
	int i, id = 0;

	links = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link) *
			     sof_amp_only_audio_card.num_links, GFP_KERNEL);
	cpus = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link_component) *
			     sof_amp_only_audio_card.num_links, GFP_KERNEL);
	if (!links || !cpus)
		goto devm_err;

	/* dmic */
	if (dmic_be_num > 0) {
		/* at least we have dmic01 */
		links[id].name = "dmic01";

		links[id].cpus = &cpus[id];
		links[id].cpus->dai_name = "DMIC01 Pin";

		links[id].init = dmic_init;
	}
	if (dmic_be_num > 1) {
		/* set up 2 BE links at most */
		dmic_be_num = 2;

		links[id + 1].name = "dmic16k";

		links[id + 1].cpus = &cpus[id + 1];
		links[id + 1].cpus->dai_name = "DMIC16k Pin";
	}

	for (i = 0; i < dmic_be_num; i++) {
		links[id].id = id;

		links[id].num_cpus = 1;

		links[id].codecs = dmic_component;
		links[id].num_codecs = ARRAY_SIZE(dmic_component);
		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].dpcm_capture = 1;
		links[id].no_pcm = 1;
		links[id].ignore_suspend = 1;
		id++;
	}

	/* HDMI */
	if (hdmi_num > 0) {
		idisps = devm_kzalloc(dev,
			 sizeof(struct snd_soc_dai_link_component) * hdmi_num,
			 GFP_KERNEL);
		if (!idisps)
			goto devm_err;
	}
	for (i = 1; i <= hdmi_num; i++) {
		links[id].id = id;
		links[id].name = devm_kasprintf(dev, GFP_KERNEL, "iDisp%d", i);
		if (!links[id].name)
			goto devm_err;

		links[id].cpus = &cpus[id];
		links[id].num_cpus = 1;
		links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
							  "iDisp%d Pin", i);
		if (!links[id].cpus->dai_name)
			goto devm_err;

		links[id].codecs = &idisps[i - 1];
		links[id].num_codecs = 1;
		idisps[i - 1].name = "ehdaudio0D2";
		idisps[i - 1].dai_name = devm_kasprintf(dev, GFP_KERNEL,
							"intel-hdmi-hifi%d", i);
		if (!idisps[i - 1].dai_name)
			goto devm_err;

		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].init = sof_hdmi_init;
		links[id].dpcm_playback = 1;
		links[id].no_pcm = 1;
		id++;
	}

	/* speaker amp */
	if (sof_amp_only_quirk & SOF_SPEAKER_AMP_PRESENT) {
		int ssp_amp = (sof_amp_only_quirk & SOF_SSP_AMP_MASK) >>
				SOF_SSP_AMP_SHIFT;
		int type_amp = (sof_amp_only_quirk & SOF_TYPE_AMP_MASK) >>
				SOF_TYPE_AMP_SHIFT;

		links[id].id = id;
		links[id].name = devm_kasprintf(dev, GFP_KERNEL, "SSP%d-Codec",
						ssp_amp);
		if (!links[id].name)
			goto devm_err;

		links[id].cpus = &cpus[id];
		links[id].num_cpus = 1;
		links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
							  "SSP%d Pin", ssp_amp);
		if (!links[id].cpus->dai_name)
			goto devm_err;

		switch (type_amp) {
		case AMP_CS35L41:
			cs35l41_set_dai_link(&links[id]);
			break;
		default:
			dev_err(dev, "no amp defined\n");
			goto devm_err;
		}

		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].dpcm_playback = 1;
		links[id].no_pcm = 1;
		id++;
	}

	/* BT audio offload */
	if (sof_amp_only_quirk & SOF_BT_OFFLOAD_PRESENT) {
		int ssp_bt = (sof_amp_only_quirk & SOF_SSP_BT_MASK) >>
				SOF_SSP_BT_SHIFT;

		links[id].id = id;
		links[id].name = devm_kasprintf(dev, GFP_KERNEL, "SSP%d-BT",
						ssp_bt);
		if (!links[id].name)
			goto devm_err;

		links[id].cpus = &cpus[id];
		links[id].num_cpus = 1;
		links[id].cpus->dai_name = devm_kasprintf(dev, GFP_KERNEL,
							  "SSP%d Pin", ssp_bt);
		if (!links[id].cpus->dai_name)
			goto devm_err;

		links[id].codecs = dummy_component;
		links[id].num_codecs = ARRAY_SIZE(dummy_component);
		links[id].platforms = platform_component;
		links[id].num_platforms = ARRAY_SIZE(platform_component);
		links[id].dpcm_playback = 1;
		links[id].dpcm_capture = 1;
		links[id].no_pcm = 1;
		id++;
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
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (pdev->id_entry && pdev->id_entry->driver_data)
		sof_amp_only_quirk = (unsigned long)pdev->id_entry->driver_data;

	mach = pdev->dev.platform_data;

	dmic_be_num = 2;
	hdmi_num = (sof_amp_only_quirk & SOF_NUM_HDMI_MASK) >>
			SOF_NUM_HDMI_SHIFT;
	/* default number of HDMI DAI's */
	if (!hdmi_num)
		hdmi_num = 3;

	dev_dbg(&pdev->dev, "sof_amp_only_quirk = %lx\n", sof_amp_only_quirk);

	/* update codec conf */
	if (sof_amp_only_quirk & SOF_SPEAKER_AMP_PRESENT) {
		int type_amp = (sof_amp_only_quirk & SOF_TYPE_AMP_MASK) >>
				SOF_TYPE_AMP_SHIFT;

		switch (type_amp) {
		case AMP_CS35L41:
			cs35l41_set_codec_conf(&sof_amp_only_audio_card);
			break;
		default:
			break;
		}
	}

	/* compute number of dai links */
	sof_amp_only_audio_card.num_links = dmic_be_num + hdmi_num;

	if (sof_amp_only_quirk & SOF_SPEAKER_AMP_PRESENT)
		sof_amp_only_audio_card.num_links++;
	if (sof_amp_only_quirk & SOF_BT_OFFLOAD_PRESENT)
		sof_amp_only_audio_card.num_links++;

	/* create dai links */
	dai_links = sof_card_dai_links_create(&pdev->dev, dmic_be_num, hdmi_num);
	if (!dai_links)
		return -ENOMEM;

	sof_amp_only_audio_card.dai_link = dai_links;
	sof_amp_only_audio_card.dev = &pdev->dev;

	/* set platform name for each dailink */
	ret = snd_soc_fixup_dai_links_platform_name(&sof_amp_only_audio_card,
						    mach->mach_params.platform);
	if (ret)
		return ret;

	/* initialize private data */
	INIT_LIST_HEAD(&ctx->hdmi_pcm_list);
	ctx->common_hdmi_codec_drv = mach->mach_params.common_hdmi_codec_drv;
	snd_soc_card_set_drvdata(&sof_amp_only_audio_card, ctx);

	return devm_snd_soc_register_card(&pdev->dev,
					  &sof_amp_only_audio_card);
}

static const struct platform_device_id board_ids[] = {
	{
		.name = "sof_amp_only",
	},
	{
		.name = "adl_cs35l41",
		.driver_data = (kernel_ulong_t)(SOF_SSP_AMP(1) |
					SOF_SSP_BT(2) |
					SOF_NUM_HDMI(4) |
					SOF_TYPE_AMP(AMP_CS35L41) |
					SOF_BT_OFFLOAD_PRESENT |
					SOF_SPEAKER_AMP_PRESENT),
	},
	{}
};
MODULE_DEVICE_TABLE(platform, board_ids);

static struct platform_driver sof_amp_only_audio_driver = {
	.probe = sof_audio_probe,
	.driver = {
		.name = "sof_amp_only",
		.pm = &snd_soc_pm_ops,
	},
	.id_table = board_ids,
};
module_platform_driver(sof_amp_only_audio_driver)

/* Module information */
MODULE_DESCRIPTION("SOF Audio Machine driver for amplifier only boards");
MODULE_AUTHOR("Brent Lu <brent.lu@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(SND_SOC_INTEL_HDA_DSP_COMMON);
MODULE_IMPORT_NS(SND_SOC_INTEL_SOF_CIRRUS_COMMON);
