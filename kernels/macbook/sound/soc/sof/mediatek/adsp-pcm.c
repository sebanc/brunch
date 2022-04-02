// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license. When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2021 Mediatek Inc. All rights reserved.
//
// Author: YC Hung <yc.hung@mediatek.com>

/*
 * PCM interface for generic Mediatek audio DSP block
 */

#include <linux/module.h>
#include "../ops.h"
#include "adsp_helper.h"
#include "adsp-pcm.h"

int mtk_adsp_stream_init(struct snd_sof_dev *sdev)
{
	struct adsp_priv *priv = sdev->pdata->hw_pdata;
	struct sof_mtk_adsp_stream *stream = priv->stream_buf;
	int i;

	for (i = 0; i < MTK_MAX_STREAM; i++, stream++) {
		stream->sdev = sdev;
		stream->active = 0;
		stream->stream_tag = i + 1;
	}

	return 0;
}
EXPORT_SYMBOL_NS(mtk_adsp_stream_init, SND_SOC_SOF_MTK_COMMON);

static int mtk_adsp_stream_put(struct snd_sof_dev *sdev,
			       struct sof_mtk_adsp_stream *mtk_stream)
{
	struct adsp_priv *priv = sdev->pdata->hw_pdata;
	struct sof_mtk_adsp_stream *stream = priv->stream_buf;
	int i;

	/* Free an active stream */
	for (i = 0; i < MTK_MAX_STREAM; i++, stream++) {
		if (stream == mtk_stream) {
			stream->active = 0;
			return 0;
		}
	}

	dev_dbg(sdev->dev, "Cannot find active stream tag %d\n", mtk_stream->stream_tag);
	return -EINVAL;
}

static struct sof_mtk_adsp_stream *mtk_adsp_stream_get(struct snd_sof_dev *sdev, int tag)
{
	struct adsp_priv *priv = sdev->pdata->hw_pdata;
	struct sof_mtk_adsp_stream *stream = priv->stream_buf;
	int i;

	for (i = 0; i < MTK_MAX_STREAM; i++, stream++) {
		if (stream->active)
			continue;

		/* return stream if tag not specified*/
		if (!tag) {
			stream->active = 1;
			return stream;
		}

		/* check if this is the requested stream tag */
		if (stream->stream_tag == tag) {
			stream->active = 1;
			return stream;
		}
	}

	dev_dbg(sdev->dev, "stream %d active or no inactive stream\n", stream->stream_tag);
	return NULL;
}

int mtk_adsp_pcm_open(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream)
{
	struct sof_mtk_adsp_stream *stream;

	stream = mtk_adsp_stream_get(sdev, 0);
	if (!stream)
		return -ENODEV;

	substream->runtime->private_data = stream;

	return 0;
}
EXPORT_SYMBOL_NS(mtk_adsp_pcm_open, SND_SOC_SOF_MTK_COMMON);

int mtk_adsp_pcm_close(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream)
{
	struct sof_mtk_adsp_stream *stream;

	stream = substream->runtime->private_data;
	if (!stream) {
		dev_err(sdev->dev, "No open stream\n");
		return -EINVAL;
	}

	substream->runtime->private_data = NULL;

	return mtk_adsp_stream_put(sdev, stream);
}
EXPORT_SYMBOL_NS(mtk_adsp_pcm_close, SND_SOC_SOF_MTK_COMMON);

snd_pcm_uframes_t mtk_adsp_pcm_pointer(struct snd_sof_dev *sdev,
				       struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *scomp = sdev->component;
	struct snd_sof_pcm *spcm;
	struct sof_ipc_stream_posn posn;
	struct snd_sof_pcm_stream *stream;
	snd_pcm_uframes_t pos;
	int ret;

	spcm = snd_sof_find_spcm_dai(scomp, rtd);
	if (!spcm) {
		dev_warn_ratelimited(sdev->dev, "warn: can't find PCM with DAI ID %d\n",
				     rtd->dai_link->id);
		return 0;
	}

	stream = &spcm->stream[substream->stream];
	ret = snd_sof_ipc_msg_data(sdev, stream->substream, &posn, sizeof(posn));
	if (ret < 0) {
		dev_warn(sdev->dev, "failed to read stream position: %d\n", ret);
		return 0;
	}

	memcpy(&stream->posn, &posn, sizeof(posn));
	pos = spcm->stream[substream->stream].posn.host_posn;
	pos = bytes_to_frames(substream->runtime, pos);

	return pos;
}
EXPORT_SYMBOL_NS(mtk_adsp_pcm_pointer, SND_SOC_SOF_MTK_COMMON);
MODULE_LICENSE("Dual BSD/GPL");
