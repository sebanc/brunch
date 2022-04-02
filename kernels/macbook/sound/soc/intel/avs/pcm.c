// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Authors: Cezary Rojewski <cezary.rojewski@intel.com>
//          Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
//

#include <linux/debugfs.h>
#include <linux/device.h>
#include <sound/hda_register.h>
#include <sound/hdaudio_ext.h>
#include <sound/pcm_params.h>
#include <sound/soc-acpi.h>
#include <sound/soc-acpi-intel-match.h>
#include <sound/soc-component.h>
#include "avs.h"
#include "path.h"
#include "topology.h"

struct avs_pcm_dma_data {
	struct avs_tplg_path_template *template;
	struct avs_path *path;
	/*
	 * link stream is stored within substream's runtime
	 * private_data to fulfill the needs of codec BE path
	 *
	 * host stream assigned
	 */
	struct hdac_ext_stream *stream;

	bool suspended; /* used to differentiate suspend flow from setup flow */
	bool is_fe;	/* used to maintain order of operations in suspend/resume flow */
	/* data needed to perform resume */
	struct snd_pcm_substream *substream;
	const struct snd_soc_dai_ops *ops;
	struct snd_pcm_hw_params *hw_params;
};

static struct avs_tplg_path_template *
avs_pcm_find_path_template(struct snd_soc_dai *dai,
			   struct snd_soc_component *component,
			   bool fe, int direction)
{
	struct snd_soc_dapm_widget *dw;
	struct snd_soc_dapm_path *dp;
	enum snd_soc_dapm_direction dir;

	if (direction == SNDRV_PCM_STREAM_CAPTURE) {
		dw = dai->capture_widget;
		dir = fe ? SND_SOC_DAPM_DIR_OUT : SND_SOC_DAPM_DIR_IN;
	} else {
		dw = dai->playback_widget;
		dir = fe ? SND_SOC_DAPM_DIR_IN : SND_SOC_DAPM_DIR_OUT;
	}

	dp = list_first_entry_or_null(&dw->edges[dir], typeof(*dp), list_node[dir]);
	if (!dp)
		return NULL;

	/* Get the other widget, with actual path template data */
	dw = (dp->source == dw) ? dp->sink : dp->source;

	return dw->priv;
}

static unsigned int adsp_get_dpib_pos(struct hdac_bus *bus,
				      unsigned char index)
{
	return readl(bus->remap_addr + AZX_REG_VS_SDXDPIB_XBASE +
		  (AZX_REG_VS_SDXDPIB_XINTERVAL * index));
}

static int avs_pcm_hw_params(struct avs_dev *adev,
			     struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *fe_hw_params,
			     struct snd_pcm_hw_params *be_hw_params,
			     struct snd_soc_dai *dai, int dma_id)
{
	struct avs_pcm_dma_data *data;
	struct avs_path *path;
	int ret;

	dev_dbg(dai->dev, "%s FE hw_params str %p rtd %p",
		__func__, substream, substream->runtime);
	dev_dbg(dai->dev, "rate %d chn %d vbd %d bd %d\n",
		params_rate(fe_hw_params), params_channels(fe_hw_params),
		params_width(fe_hw_params),
		params_physical_width(fe_hw_params));

	dev_dbg(dai->dev, "%s BE hw_params str %p rtd %p",
		__func__, substream, substream->runtime);
	dev_dbg(dai->dev, "rate %d chn %d vbd %d bd %d\n",
		params_rate(be_hw_params), params_channels(be_hw_params),
		params_width(be_hw_params),
		params_physical_width(be_hw_params));

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (data->path)
		return 0;

	path = avs_path_create(adev, dma_id, data->template, fe_hw_params, be_hw_params);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		dev_err(dai->dev, "create BE path failed: %d\n", ret);
		return ret;
	}

	data->path = path;
	return 0;
}

static int avs_pcm_fe_hw_params(struct avs_dev *adev,
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *fe_hw_params,
				struct snd_soc_dai *dai, int dma_id)
{
	struct snd_pcm_hw_params *be_hw_params = NULL;
	struct snd_soc_pcm_runtime *fe, *be;
	struct snd_soc_dpcm *dpcm;

	fe = asoc_substream_to_rtd(substream);
	for_each_dpcm_be(fe, substream->stream, dpcm) {
		be = dpcm->be;
		be_hw_params = &be->dpcm[substream->stream].hw_params;
	}

	return avs_pcm_hw_params(adev, substream, fe_hw_params, be_hw_params,
				 dai, dma_id);
}

static int avs_pcm_be_hw_params(struct avs_dev *adev,
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *be_hw_params,
				struct snd_soc_dai *dai, int dma_id)
{
	struct snd_pcm_hw_params *fe_hw_params = NULL;
	struct snd_soc_pcm_runtime *fe, *be;
	struct snd_soc_dpcm *dpcm;

	be = asoc_substream_to_rtd(substream);
	for_each_dpcm_fe(be, substream->stream, dpcm) {
		fe = dpcm->fe;
		fe_hw_params = &fe->dpcm[substream->stream].hw_params;
	}

	return avs_pcm_hw_params(adev, substream, fe_hw_params, be_hw_params,
				 dai, dma_id);
}

static int avs_dai_prepare(struct avs_dev *adev,
			   struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;
	int ret;

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (!data->path)
		return 0;

	ret = avs_path_reset(data->path);
	if (ret < 0) {
		dev_err(dai->dev, "reset path failed: %d\n", ret);
		return ret;
	}

	ret = avs_path_pause(data->path);
	if (ret < 0)
		dev_err(dai->dev, "pause path failed: %d\n", ret);
	return ret;
}

static int avs_dai_nonhda_be_startup(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_component *component;
	struct avs_tplg_path_template *template;
	struct avs_pcm_dma_data *data;
	int i;

	for_each_rtd_components(rtd, i, component)
		if (strstr(component->name, "-platform"))
			break;

	if (i == rtd->num_components)
		return -EINVAL;

	template = avs_pcm_find_path_template(dai, component, false, substream->stream);
	if (!template) {
		dev_err(dai->dev, "no %s path for dai %s, invalid tplg?\n",
			snd_pcm_stream_str(substream), dai->name);
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->template = template;
	snd_soc_dai_set_dma_data(dai, substream, data);

	return 0;
}

static void avs_dai_nonhda_be_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;

	data = snd_soc_dai_get_dma_data(dai, substream);

	snd_soc_dai_set_dma_data(dai, substream, NULL);
	kfree(data);
}

static const struct snd_soc_dai_ops avs_dai_nonhda_be_ops;

static int avs_dai_nonhda_be_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params,
				       struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (data->path)
		return 0;

	if (!data->suspended) {
		data->is_fe = false;
		data->substream = substream;
		data->ops = &avs_dai_nonhda_be_ops;
		data->hw_params = kmemdup(hw_params, sizeof(struct snd_pcm_hw_params), GFP_KERNEL);
	}

	/* Actual port-id comes from topology. */
	return avs_pcm_be_hw_params(to_avs_dev(dai->dev), substream, hw_params, dai, 0);
}

static int avs_dai_nonhda_be_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;

	dev_info(dai->dev, "%s be HW_FREE str %p rtd %p",
		__func__, substream, substream->runtime);

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (data->path) {
		avs_path_free(data->path);
		data->path = NULL;
	}

	return 0;
}

static int avs_dai_nonhda_be_prepare(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	return avs_dai_prepare(to_avs_dev(dai->dev), substream, dai);
}

static int avs_dai_nonhda_be_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct avs_pcm_dma_data *data;
	int ret = 0;

	data = snd_soc_dai_get_dma_data(dai, substream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		if (rtd->dai_link->ignore_suspend)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = avs_path_pause(data->path);
		if (ret < 0) {
			dev_err(dai->dev, "pause BE path failed: %d\n", ret);
			break;
		}

		ret = avs_path_run(data->path, AVS_TPLG_TRIGGER_AUTO);
		if (ret < 0)
			dev_err(dai->dev, "run BE path failed: %d\n", ret);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (rtd->dai_link->ignore_suspend)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		ret = avs_path_pause(data->path);
		if (ret < 0)
			dev_err(dai->dev, "pause BE path failed: %d\n", ret);

		ret = avs_path_reset(data->path);
		if (ret < 0)
			dev_err(dai->dev, "reset FE path failed: %d\n", ret);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops avs_dai_nonhda_be_ops = {
	.startup = avs_dai_nonhda_be_startup,
	.shutdown = avs_dai_nonhda_be_shutdown,
	.hw_params = avs_dai_nonhda_be_hw_params,
	.hw_free = avs_dai_nonhda_be_hw_free,
	.prepare = avs_dai_nonhda_be_prepare,
	.trigger = avs_dai_nonhda_be_trigger,
};

static const struct snd_soc_dai_ops avs_dai_hda_be_ops;

static int avs_dai_hda_be_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params,
				    struct snd_soc_dai *dai)
{
	struct hdac_ext_stream *hstream;
	struct avs_dev *adev = to_avs_dev(dai->dev);
	struct avs_pcm_dma_data *data;

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (data->path)
		return 0;

	if (!data->suspended) {
		data->is_fe = false;
		data->substream = substream;
		data->ops = &avs_dai_hda_be_ops;
		data->hw_params = kmemdup(hw_params, sizeof(struct snd_pcm_hw_params), GFP_KERNEL);
	}

	hstream = substream->runtime->private_data;

	return avs_pcm_be_hw_params(adev, substream, hw_params, dai,
				    hdac_stream(hstream)->stream_tag - 1);
}

static int avs_dai_hda_be_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct hdac_ext_stream *stream;
	struct hdac_ext_link *link;
	struct hda_codec *codec;
	int ret;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	ret = avs_dai_nonhda_be_hw_free(substream, dai);
	if (ret)
		dev_err(dai->dev, "DSP hw_free failed: %d\n", ret);

	/* clear link to stream mapping */
	codec = dev_to_hda_codec(asoc_rtd_to_codec(rtd, 0)->dev);
	link = snd_hdac_ext_bus_link_at(&codec->bus->core, codec->core.addr);
	if (!link)
		return -EINVAL;

	stream = substream->runtime->private_data;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct hdac_stream *hstream = hdac_stream(stream);

		snd_hdac_ext_link_clear_stream_id(link, hstream->stream_tag);
	}

	stream->link_prepared = 0;
	return 0;
}

static int avs_dai_hda_be_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hdac_ext_stream *stream = runtime->private_data;
	struct hdac_ext_link *link;
	struct hda_codec *codec;
	struct hdac_bus *bus;
	unsigned int format_val;
	int ret;

	if (stream->link_prepared)
		return 0;

	codec = dev_to_hda_codec(asoc_rtd_to_codec(rtd, 0)->dev);
	bus = &codec->bus->core;
	format_val = snd_hdac_calc_stream_format(runtime->rate,
						 runtime->channels,
						 runtime->format,
						 runtime->sample_bits, 0);

	snd_hdac_ext_stream_decouple(bus, stream, true);
	snd_hdac_ext_link_stream_reset(stream);
	snd_hdac_ext_link_stream_setup(stream, format_val);

	link = snd_hdac_ext_bus_link_at(bus, codec->core.addr);
	if (!link)
		return -EINVAL;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct hdac_stream *hstream = hdac_stream(stream);

		snd_hdac_ext_link_set_stream_id(link, hstream->stream_tag);
	}

	ret = avs_dai_nonhda_be_prepare(substream, dai);
	if (ret)
		return ret;

	stream->link_prepared = 1;
	return 0;
}

static int avs_dai_hda_be_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct hdac_ext_stream *stream;
	struct avs_pcm_dma_data *data;
	int ret = 0;

	dev_dbg(dai->dev, "entry %s cmd=%d\n", __func__, cmd);

	stream = substream->runtime->private_data;
	if (!stream->link_prepared)
		return -EPIPE;

	data = snd_soc_dai_get_dma_data(dai, substream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		if (rtd->dai_link->ignore_suspend)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		snd_hdac_ext_link_stream_start(stream);

		ret = avs_path_pause(data->path);
		if (ret < 0) {
			dev_err(dai->dev, "pause FE path failed: %d\n", ret);
			break;
		}

		ret = avs_path_run(data->path, AVS_TPLG_TRIGGER_AUTO);
		if (ret < 0)
			dev_err(dai->dev, "run FE path failed: %d\n", ret);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (rtd->dai_link->ignore_suspend)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		ret = avs_path_pause(data->path);
		if (ret < 0)
			dev_err(dai->dev, "pause FE path failed: %d\n", ret);

		snd_hdac_ext_link_stream_clear(stream);

		ret = avs_path_reset(data->path);
		if (ret < 0)
			dev_err(dai->dev, "reset FE path failed: %d\n", ret);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops avs_dai_hda_be_ops = {
	.startup = avs_dai_nonhda_be_startup,
	.shutdown = avs_dai_nonhda_be_shutdown,
	.hw_params = avs_dai_hda_be_hw_params,
	.hw_free = avs_dai_hda_be_hw_free,
	.prepare = avs_dai_hda_be_prepare,
	.trigger = avs_dai_hda_be_trigger,
};

static const unsigned int rates[] = {
	8000, 11025, 12000, 16000,
	22050, 24000, 32000, 44100,
	48000, 64000, 88200, 96000,
	128000, 176400, 192000,
};

static const struct snd_pcm_hw_constraint_list hw_rates = {
	.count = ARRAY_SIZE(rates),
	.list = rates,
	.mask = 0,
};

static int avs_dai_fe_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct avs_dev *adev = to_avs_dev(dai->dev);
	struct avs_tplg_path_template *template;
	struct avs_pcm_dma_data *data;
	struct hdac_bus *bus = &adev->bus.core;
	struct hdac_ext_stream *stream;

	template = avs_pcm_find_path_template(dai, dai->component, true,
					      substream->stream);
	if (!template) {
		dev_err(dai->dev, "no %s path for dai %s, invalid tplg?\n",
			snd_pcm_stream_str(substream), dai->name);
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	stream = snd_hdac_ext_stream_assign(bus, substream,
					    HDAC_EXT_STREAM_TYPE_HOST);
	if (!stream) {
		kfree(data);
		return -EBUSY;
	}

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	/* avoid wrap-around with wall-clock */
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_TIME,
				     20, 178000000);
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				   &hw_rates);

	dev_info(dai->dev, "%s fe STARTUP tag %d str %p",
		__func__, hdac_stream(stream)->stream_tag, substream);

	data->stream = stream;
	data->template = template;
	snd_soc_dai_set_dma_data(dai, substream, data);
	snd_pcm_set_sync(substream);

	return 0;
}

static void avs_dai_fe_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;

	data = snd_soc_dai_get_dma_data(dai, substream);

	snd_soc_dai_set_dma_data(dai, substream, NULL);
	snd_hdac_ext_stream_release(data->stream, HDAC_EXT_STREAM_TYPE_HOST);
	kfree(data);
}

const struct snd_soc_dai_ops avs_dai_fe_ops;

static int avs_dai_fe_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *hw_params,
				struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;
	struct avs_dev *adev = to_avs_dev(dai->dev);
	struct hdac_stream *hstream;
	int ret;

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (data->path)
		return 0;

	if (!data->suspended) {
		data->is_fe = true;
		data->substream = substream;
		data->ops = &avs_dai_fe_ops;
		data->hw_params = kmemdup(hw_params, sizeof(struct snd_pcm_hw_params), GFP_KERNEL);
	}

	hstream = hdac_stream(data->stream);
	hstream->bufsize = 0;
	hstream->period_bytes = 0;
	hstream->format_val = 0;

	ret = avs_pcm_fe_hw_params(adev, substream, hw_params, dai,
				   hstream->stream_tag - 1);
	if (ret)
		goto create_err;

	ret = avs_path_bind(data->path);
	if (ret < 0) {
		dev_err(dai->dev, "bind FE <-> BE failed: %d\n", ret);
		goto bind_err;
	}

	return 0;

bind_err:
	avs_path_free(data->path);
	data->path = NULL;
create_err:
	snd_pcm_lib_free_pages(substream);
	return ret;
}

static int avs_dai_fe_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct avs_pcm_dma_data *data;
	struct hdac_stream *hstream;
	int ret;

	dev_info(dai->dev, "%s fe HW_FREE str %p rtd %p",
		__func__, substream, substream->runtime);

	data = snd_soc_dai_get_dma_data(dai, substream);
	if (!data->path)
		return 0;

	hstream = hdac_stream(data->stream);

	ret = avs_path_unbind(data->path);
	if (ret < 0)
		dev_err(dai->dev, "unbind FE <-> BE failed: %d\n", ret);

	avs_path_free(data->path);
	data->path = NULL;
	snd_hdac_stream_cleanup(hstream);
	hstream->prepared = false;

	if (!data->suspended) {
		ret = snd_pcm_lib_free_pages(substream);
		if (ret < 0)
			dev_dbg(dai->dev, "Failed to free pages!\n");
	}

	return ret;
}

static int avs_dai_fe_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct avs_pcm_dma_data *data;
	struct avs_dev *adev = to_avs_dev(dai->dev);
	struct hdac_stream *hstream;
	struct hdac_bus *bus;
	unsigned int format_val;
	int ret;

	data = snd_soc_dai_get_dma_data(dai, substream);
	hstream = hdac_stream(data->stream);
	bus = hstream->bus;

	snd_hdac_ext_stream_decouple(bus, data->stream, true);
	snd_hdac_stream_reset(hstream);

	format_val = snd_hdac_calc_stream_format(runtime->rate,
						 runtime->channels,
						 runtime->format,
						 runtime->sample_bits, 0);

	ret = snd_hdac_stream_set_params(hstream, format_val);
	if (ret < 0)
		return ret;

	ret = snd_hdac_stream_setup(hstream);
	if (ret < 0)
		return ret;

	ret = avs_dai_prepare(adev, substream, dai);
	if (ret)
		return ret;

	hstream->prepared = true;
	return 0;
}

static int avs_dai_fe_trigger(struct snd_pcm_substream *substream,
			      int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct avs_pcm_dma_data *data;
	struct hdac_ext_stream *stream;
	struct hdac_stream *hstream;
	struct hdac_bus *bus;
	unsigned long flags;
	int ret = 0;

	data = snd_soc_dai_get_dma_data(dai, substream);
	stream = data->stream;
	hstream = hdac_stream(stream);

	if (!hstream->prepared)
		return -EPIPE;

	bus = hstream->bus;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		if (rtd->dai_link->ignore_suspend)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spin_lock_irqsave(&bus->reg_lock, flags);
		snd_hdac_stream_start(hstream, true);
		spin_unlock_irqrestore(&bus->reg_lock, flags);

		ret = avs_path_pause(data->path);
		if (ret < 0) {
			dev_err(dai->dev, "pause FE path failed: %d\n", ret);
			break;
		}

		ret = avs_path_run(data->path, AVS_TPLG_TRIGGER_AUTO);
		if (ret < 0)
			dev_err(dai->dev, "run FE path failed: %d\n", ret);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (rtd->dai_link->ignore_suspend)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		ret = avs_path_pause(data->path);
		if (ret < 0)
			dev_err(dai->dev, "pause FE path failed: %d\n", ret);

		spin_lock_irqsave(&bus->reg_lock, flags);
		snd_hdac_stream_stop(hstream);
		spin_unlock_irqrestore(&bus->reg_lock, flags);

		ret = avs_path_reset(data->path);
		if (ret < 0)
			dev_err(dai->dev, "reset FE path failed: %d\n", ret);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

const struct snd_soc_dai_ops avs_dai_fe_ops = {
	.startup = avs_dai_fe_startup,
	.shutdown = avs_dai_fe_shutdown,
	.hw_params = avs_dai_fe_hw_params,
	.hw_free = avs_dai_fe_hw_free,
	.prepare = avs_dai_fe_prepare,
	.trigger = avs_dai_fe_trigger,
};

static ssize_t topology_name_read(struct file *file, char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct snd_soc_component *component = file->private_data;
	struct snd_soc_card *card = component->card;
	struct snd_soc_acpi_mach *mach = dev_get_platdata(card->dev);
	char buf[64];
	size_t len;

	len = snprintf(buf, sizeof(buf), "%s/%s\n",
		       component->driver->topology_name_prefix,
		       mach->fw_filename);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations topology_name_fops = {
	.open = simple_open,
	.read = topology_name_read,
	.llseek = default_llseek,
};

static int avs_component_load_libraries(struct avs_soc_component *acomp)
{
	struct avs_tplg *tplg = acomp->tplg;
	struct avs_dev *adev = to_avs_dev(acomp->base.dev);
	int ret;

	if (!tplg->num_libs)
		return 0;

	/* Parent device may be asleep and library loading involves IPCs. */
	pm_runtime_get_sync(adev->dev);

	avs_hda_clock_gating_enable(adev, false);
	avs_hda_l1sen_enable(adev, false);

	ret = avs_dsp_load_libraries(adev, tplg->libs, tplg->num_libs);

	avs_hda_l1sen_enable(adev, true);
	avs_hda_clock_gating_enable(adev, true);

	if (!ret)
		ret = avs_module_info_init(adev, false);

	pm_runtime_mark_last_busy(adev->dev);
	pm_runtime_put_autosuspend(adev->dev);

	return ret;
}

static int avs_component_probe(struct snd_soc_component *component)
{
	struct snd_soc_card *card = component->card;
	struct snd_soc_acpi_mach *mach;
	struct avs_soc_component *acomp;
	struct avs_dev *adev;
	char *filename;
	int ret;

	dev_info(card->dev, "probing %s card %s\n", component->name, card->name);
	mach = dev_get_platdata(card->dev);
	acomp = to_avs_soc_component(component);
	adev = to_avs_dev(component->dev);

	acomp->tplg = avs_tplg_new(component);
	if (!acomp->tplg)
		return -ENOMEM;

	if (!mach->fw_filename)
		goto finalize;

	/* Load specified topology and create sysfs for it. */
	filename = kasprintf(GFP_KERNEL, "%s/%s",
			     component->driver->topology_name_prefix,
			     mach->fw_filename);
	if (!filename)
		return -ENOMEM;

	ret = avs_load_topology(component, filename);
	kfree(filename);
	if (ret < 0)
		return ret;

	acomp->kobj = kobject_create_and_add(acomp->tplg->name, &component->dev->kobj);
	if (!acomp->kobj) {
		ret = -ENOMEM;
		goto err_kobj_create;
	}

	ret = avs_component_load_libraries(acomp);
	if (ret < 0) {
		dev_err(card->dev, "libraries loading failed: %d\n", ret);
		goto err_load_libs;
	}

finalize:
	debugfs_create_file("topology_name", 0444, component->debugfs_root,
			    component, &topology_name_fops);

	spin_lock(&adev->comp_list_lock);
	list_add_tail(&acomp->node, &adev->comp_list);
	spin_unlock(&adev->comp_list_lock);

	return 0;

err_load_libs:
	kobject_put(acomp->kobj);
err_kobj_create:
	avs_remove_topology(component);
	return ret;
}

static void avs_component_remove(struct snd_soc_component *component)
{
	struct avs_soc_component *acomp = to_avs_soc_component(component);
	struct snd_soc_acpi_mach *mach;
	struct avs_dev *adev = to_avs_dev(component->dev);
	int ret;

	mach = dev_get_platdata(component->card->dev);

	kobject_put(acomp->kobj);

	spin_lock(&adev->comp_list_lock);
	list_del(&acomp->node);
	spin_unlock(&adev->comp_list_lock);

	if (mach->fw_filename) {
		ret = avs_remove_topology(component);
		if (ret < 0)
			dev_err(component->dev, "unload topology failed: %d\n", ret);
	}
}

static int avs_component_open(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_pcm_hardware hwparams;
	int ret;

	/* nothing to do for BE */
	if (rtd->dai_link->no_pcm)
		return 0;

	hwparams.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_RESUME |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP;

	hwparams.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S32_LE;
	hwparams.period_bytes_min = 128;
	hwparams.period_bytes_max = AZX_MAX_BUF_SIZE / 2;
	hwparams.periods_min = 2;
	hwparams.periods_max = AZX_MAX_FRAG;
	hwparams.buffer_bytes_max = AZX_MAX_BUF_SIZE;
	hwparams.fifo_size = 0;

	ret = snd_soc_set_runtime_hwparams(substream, &hwparams);

	return ret;
}

static snd_pcm_uframes_t
avs_component_pointer(struct snd_soc_component *component,
		      struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct avs_pcm_dma_data *data;
	struct hdac_stream *hstream;
	struct hdac_bus *bus;
	unsigned int pos;

	data = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);
	if (!data->stream)
		return 0;

	hstream = hdac_stream(data->stream);
	bus = hstream->bus;

	/* TODO: Address the inaccurancy below. */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pos = adsp_get_dpib_pos(bus, hstream->index);
	} else {
		usleep_range(20, 21);
		adsp_get_dpib_pos(bus, hstream->index);
		pos = snd_hdac_stream_get_pos_posbuf(hstream);
	}

	if (pos >= hstream->bufsize)
		pos = 0;

	return bytes_to_frames(substream->runtime, pos);
}

static int avs_component_mmap(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream,
			      struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

#define MAX_PREALLOC_SIZE	(32 * 1024 * 1024)

static int avs_component_construct(struct snd_soc_component *component,
				   struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_pcm *pcm = rtd->pcm;

	if (dai->driver->playback.channels_min)
		snd_pcm_set_managed_buffer(pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream,
					   SNDRV_DMA_TYPE_DEV_SG, component->dev,
					   0, MAX_PREALLOC_SIZE);

	if (dai->driver->capture.channels_min)
		snd_pcm_set_managed_buffer(pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream,
					   SNDRV_DMA_TYPE_DEV_SG, component->dev,
					   0, MAX_PREALLOC_SIZE);

	return 0;
}

static int avs_comp_dai_hw_params(struct snd_soc_dai *dai, struct avs_pcm_dma_data *data, bool fe)
{
	int ret = 0;

	if (data->suspended && !(fe ^ data->is_fe)) {
		ret = data->ops->hw_params(data->substream, data->hw_params, dai);
		if (ret < 0)
			dev_err(dai->dev, "hw_params on resume failed: %d\n", ret);
	}

	return ret;
}

static int avs_comp_dai_prepare(struct snd_soc_dai *dai, struct avs_pcm_dma_data *data, bool fe)
{
	int ret = 0;

	if (data->suspended && !(fe ^ data->is_fe)) {
		ret = data->ops->prepare(data->substream, dai);
		if (ret < 0) {
			dev_err(dai->dev, "prepare on resume failed: %d\n", ret);
			return ret;
		}
		data->suspended = false;
	}

	return ret;
}

static int avs_comp_dai_hw_free(struct snd_soc_dai *dai, struct avs_pcm_dma_data *data, bool fe)
{
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	rtd = snd_pcm_substream_chip(data->substream);

	if (data->path && !(fe ^ data->is_fe) && !rtd->dai_link->ignore_suspend) {
		data->suspended = true;
		ret = data->ops->hw_free(data->substream, dai);
		if (ret < 0)
			dev_err(dai->dev, "hw_free on suspend failed: %d\n", ret);
	}

	return ret;
}

static int avs_component_resume_hw_params(struct snd_soc_component *component, bool fe)
{
	struct snd_soc_dai *dai;

	for_each_component_dais(component, dai) {
		if (dai->playback_dma_data)
			avs_comp_dai_hw_params(dai, dai->playback_dma_data, fe);

		if (dai->capture_dma_data)
			avs_comp_dai_hw_params(dai, dai->capture_dma_data, fe);
	}

	return 0;
}

static int avs_component_resume_prepare(struct snd_soc_component *component, bool fe)
{
	struct snd_soc_dai *dai;

	for_each_component_dais(component, dai) {
		if (dai->playback_dma_data)
			avs_comp_dai_prepare(dai, dai->playback_dma_data, fe);

		if (dai->capture_dma_data)
			avs_comp_dai_prepare(dai, dai->capture_dma_data, fe);
	}

	return 0;
}

static int avs_component_suspend_hw_free(struct snd_soc_component *component, bool fe)
{
	struct snd_soc_dai *dai;

	for_each_component_dais(component, dai) {
		if (dai->playback_dma_data)
			avs_comp_dai_hw_free(dai, dai->playback_dma_data, fe);

		if (dai->capture_dma_data)
			avs_comp_dai_hw_free(dai, dai->capture_dma_data, fe);
	}

	return 0;
}

static int avs_component_suspend(struct snd_soc_component *component)
{
	int ret;

	/*
	 * When freeing paths, FEs need to be freed _first_ as they perform
	 * path unbinding.
	 */

	ret = avs_component_suspend_hw_free(component, true);
	if (ret)
		return ret;

	ret = avs_component_suspend_hw_free(component, false);
	return ret;
}


static int avs_component_resume(struct snd_soc_component *component)
{
	int ret;

	/*
	 * When creating paths, FEs need to be _last_ as they perform
	 * path binding.
	 */

	ret = avs_component_resume_hw_params(component, false);
	if (ret)
		return ret;

	ret = avs_component_resume_hw_params(component, true);
	if (ret)
		return ret;

	/*
	 * Prepare callback is called by ASoC for FE _first_ and BE _last_, so
	 * recreate behavior even though order shouldn't matter in this case.
	 */
	ret = avs_component_resume_prepare(component, true);
	if (ret)
		return ret;

	ret = avs_component_resume_prepare(component, false);
	if (ret)
		return ret;

	return ret;
}

static const struct snd_soc_component_driver avs_component_driver = {
	.name			= "avs-pcm",
	.probe			= avs_component_probe,
	.remove			= avs_component_remove,
	.suspend		= avs_component_suspend,
	.resume			= avs_component_resume,
	.open			= avs_component_open,
	.pointer		= avs_component_pointer,
	.mmap			= avs_component_mmap,
	.pcm_construct		= avs_component_construct,
	.module_get_upon_open	= 1, /* increment refcount when a pcm is opened */
	.topology_name_prefix	= "intel/avs",
	.non_legacy_dai_naming	= true,
};

static int avs_soc_component_register(struct device *dev, const char *name,
				      const struct snd_soc_component_driver *drv,
				      struct snd_soc_dai_driver *cpu_dais,
				      int num_cpu_dais)
{
	struct avs_soc_component *acomp;
	int ret;

	acomp = devm_kzalloc(dev, sizeof(*acomp), GFP_KERNEL);
	if (!acomp)
		return -ENOMEM;

	ret = snd_soc_component_initialize(&acomp->base, drv, dev);
	if (ret < 0)
		return ret;

	/* force name change after ASoC is done with its init */
	acomp->base.name = name;
	INIT_LIST_HEAD(&acomp->node);

	return snd_soc_add_component(&acomp->base, cpu_dais, num_cpu_dais);
}

static struct snd_soc_dai_driver dmic_cpu_dais[] = {
{
	.name = "DMIC Pin",
	.ops = &avs_dai_nonhda_be_ops,
	.capture = {
		.stream_name	= "DMIC Rx",
		.channels_min	= 1,
		.channels_max	= 4,
		.rates		= SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000,
		.formats	= SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
},
{
	.name = "DMIC WoV Pin",
	.ops = &avs_dai_nonhda_be_ops,
	.capture = {
		.stream_name	= "DMIC WoV Rx",
		.channels_min	= 1,
		.channels_max	= 4,
		.rates		= SNDRV_PCM_RATE_16000,
		.formats	= SNDRV_PCM_FMTBIT_S16_LE,
	},
},
};

int avs_dmic_platform_register(struct avs_dev *adev, const char *name)
{
	return avs_soc_component_register(adev->dev, name, &avs_component_driver,
					  dmic_cpu_dais,
					  ARRAY_SIZE(dmic_cpu_dais));
}

static const struct snd_soc_dai_driver ssp_dai_template = {
	.ops = &avs_dai_nonhda_be_ops,
	.playback = {
		.channels_min	= 1,
		.channels_max	= 8,
		.rates		= SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT,
		.formats	= SNDRV_PCM_FMTBIT_U32_LE,
	},
	.capture = {
		.channels_min	= 1,
		.channels_max	= 8,
		.rates		= SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT,
		.formats	= SNDRV_PCM_FMTBIT_U32_LE,
	},
};

int avs_ssp_platform_register(struct avs_dev *adev, const char *name,
			      unsigned long port_mask, unsigned long *tdms)
{
	struct snd_soc_dai_driver *cpus, *dai;
	size_t ssp_count, cpu_count;
	int i, j;

	ssp_count = adev->hw_cfg.i2s_caps.ctrl_count;
	cpu_count = hweight_long(port_mask);
	if (tdms)
		for_each_set_bit(i, &port_mask, ssp_count)
			cpu_count += hweight_long(tdms[i]);

	cpus = devm_kzalloc(adev->dev, sizeof(*cpus) * cpu_count, GFP_KERNEL);
	if (!cpus)
		return -ENOMEM;

	dai = cpus;
	for_each_set_bit(i, &port_mask, ssp_count) {
		memcpy(dai, &ssp_dai_template, sizeof(*dai));

		dai->name =
			devm_kasprintf(adev->dev, GFP_KERNEL, "SSP%d Pin", i);
		dai->playback.stream_name =
			devm_kasprintf(adev->dev, GFP_KERNEL, "ssp%d Tx", i);
		dai->capture.stream_name =
			devm_kasprintf(adev->dev, GFP_KERNEL, "ssp%d Rx", i);

		if (!dai->name || !dai->playback.stream_name ||
		    !dai->capture.stream_name)
			return -ENOMEM;
		dai++;
	}

	if (!tdms)
		goto plat_register;

	for_each_set_bit(i, &port_mask, ssp_count) {
		for_each_set_bit(j, &tdms[i], ssp_count) {
			memcpy(dai, &ssp_dai_template, sizeof(*dai));

			dai->name =
				devm_kasprintf(adev->dev, GFP_KERNEL, "SSP%d:%d Pin", i, j);
			dai->playback.stream_name =
				devm_kasprintf(adev->dev, GFP_KERNEL, "ssp%d:%d Tx", i, j);
			dai->capture.stream_name =
				devm_kasprintf(adev->dev, GFP_KERNEL, "ssp%d:%d Rx", i, j);

			if (!dai->name || !dai->playback.stream_name ||
			    !dai->capture.stream_name)
				return -ENOMEM;
			dai++;
		}
	}

plat_register:
	return avs_soc_component_register(adev->dev, name, &avs_component_driver,
					  cpus, cpu_count);
}

/* HD-Audio CPU DAI template */
static const struct snd_soc_dai_driver hda_cpu_dai = {
	.ops = &avs_dai_hda_be_ops,
	.playback = {
		.channels_min	= 1,
		.channels_max	= 8,
		.rates		= SNDRV_PCM_RATE_8000_192000,
		.formats	= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.channels_min	= 1,
		.channels_max	= 8,
		.rates		= SNDRV_PCM_RATE_8000_192000,
		.formats	= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	},
};

static void avs_component_hda_unregister_dais(struct snd_soc_component *component)
{
	struct snd_soc_acpi_mach *mach;
	struct snd_soc_dai *dai, *save;
	struct hda_codec *codec;
	char name[32];

	mach = dev_get_platdata(component->card->dev);
	codec = mach->pdata;
	sprintf(name, "%s-cpu", dev_name(&codec->core.dev));

	for_each_component_dais_safe(component, dai, save) {
		if (!strstr(dai->driver->name, name))
			continue;

		if (dai->playback_widget)
			snd_soc_dapm_free_widget(dai->playback_widget);
		if (dai->capture_widget)
			snd_soc_dapm_free_widget(dai->capture_widget);
		snd_soc_unregister_dai(dai);
	}
}

static int avs_component_hda_probe(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dai_driver *dais;
	struct snd_soc_acpi_mach *mach;
	struct hda_codec *codec;
	struct hda_pcm *pcm;
	const char *cname;
	int pcm_count = 0, ret, i;

	mach = dev_get_platdata(component->card->dev);
	if (!mach)
		return -EINVAL;

	codec = mach->pdata;
	if (list_empty(&codec->pcm_list_head))
		return -EINVAL;
	list_for_each_entry(pcm, &codec->pcm_list_head, list)
		pcm_count++;

	dais = devm_kcalloc(component->dev, pcm_count, sizeof(*dais),
			    GFP_KERNEL);
	if (!dais)
		return -ENOMEM;

	cname = dev_name(&codec->core.dev);
	dapm = snd_soc_component_get_dapm(component);
	pcm = list_first_entry(&codec->pcm_list_head, struct hda_pcm, list);

	for (i = 0; i < pcm_count; i++, pcm = list_next_entry(pcm, list)) {
		struct snd_soc_dai *dai;

		memcpy(&dais[i], &hda_cpu_dai, sizeof(*dais));
		dais[i].id = i;
		dais[i].name = devm_kasprintf(component->dev, GFP_KERNEL,
					      "%s-cpu%d", cname, i);
		if (!dais[i].name) {
			ret = -ENOMEM;
			goto exit;
		}

		if (pcm->stream[0].substreams) {
			dais[i].playback.stream_name =
				devm_kasprintf(component->dev, GFP_KERNEL,
					       "%s-cpu%d Tx", cname, i);
			if (!dais[i].playback.stream_name) {
				ret = -ENOMEM;
				goto exit;
			}
		}

		if (pcm->stream[1].substreams) {
			dais[i].capture.stream_name =
				devm_kasprintf(component->dev, GFP_KERNEL,
					       "%s-cpu%d Rx", cname, i);
			if (!dais[i].capture.stream_name) {
				ret = -ENOMEM;
				goto exit;
			}
		}

		dai = snd_soc_register_dai(component, &dais[i], false);
		if (!dai) {
			dev_err(component->dev, "register dai for %s failed\n",
				pcm->name);
			ret = -EINVAL;
			goto exit;
		}

		ret = snd_soc_dapm_new_dai_widgets(dapm, dai);
		if (ret < 0) {
			dev_err(component->dev, "create widgets failed: %d\n",
				ret);
			goto exit;
		}
	}

	ret = avs_component_probe(component);
exit:
	if (ret)
		avs_component_hda_unregister_dais(component);

	return ret;
}

static void avs_component_hda_remove(struct snd_soc_component *component)
{
	avs_component_hda_unregister_dais(component);
	avs_component_remove(component);
}

static int avs_component_hda_open(struct snd_soc_component *component,
				  struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct hdac_ext_stream *stream;
	struct hda_codec *codec;

	/* handling no BE paths here */
	if (!rtd->dai_link->no_pcm)
		return avs_component_open(component, substream);

	codec = dev_to_hda_codec(asoc_rtd_to_codec(rtd, 0)->dev);
	stream = snd_hdac_ext_stream_assign(&codec->bus->core, substream,
					    HDAC_EXT_STREAM_TYPE_LINK);
	if (!stream)
		return -EBUSY;

	substream->runtime->private_data = stream;
	return 0;
}

static int avs_component_hda_close(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct hdac_ext_stream *stream;

	/* handling no BE paths here */
	if (!rtd->dai_link->no_pcm)
		return 0;

	stream = substream->runtime->private_data;
	snd_hdac_ext_stream_release(stream, HDAC_EXT_STREAM_TYPE_LINK);
	substream->runtime->private_data = NULL;

	return 0;
}

static const struct snd_soc_component_driver avs_hda_component_driver = {
	.name			= "avs-hda-pcm",
	.probe			= avs_component_hda_probe,
	.remove			= avs_component_hda_remove,
	.suspend		= avs_component_suspend,
	.resume			= avs_component_resume,
	.open			= avs_component_hda_open,
	.close			= avs_component_hda_close,
	.pointer		= avs_component_pointer,
	.mmap			= avs_component_mmap,
	.pcm_construct		= avs_component_construct,
	/*
	 * hda platform component's probe() is dependent on
	 * codec->pcm_list_head, it needs to be initialized after codec
	 * component. remove_order is here for completeness sake
	 */
	.probe_order		= SND_SOC_COMP_ORDER_LATE,
	.remove_order		= SND_SOC_COMP_ORDER_EARLY,
	.module_get_upon_open	= 1,
	.topology_name_prefix	= "intel/avs",
	.non_legacy_dai_naming	= true,
};

int avs_hda_platform_register(struct avs_dev *adev, const char *name)
{
	return avs_soc_component_register(adev->dev, name,
					  &avs_hda_component_driver, NULL, 0);
}
