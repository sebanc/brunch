// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license. When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2021 Advanced Micro Devices, Inc.
//
// Authors: Ajit Kumar Pandey <AjitKumar.Pandey@amd.com>

/*
 * PCM interface for generic AMD audio ACP DSP block
 */
#include <sound/pcm_params.h>

#include "../ops.h"
#include "acp.h"
#include "acp-dsp-offset.h"
#include "../sof-audio.h"

int acp_pcm_hw_params(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream,
		      struct snd_pcm_hw_params *params, struct sof_ipc_stream_params *ipc_params)
{
	struct acp_dsp_stream *stream = substream->runtime->private_data;
	unsigned int buf_offset, index;
	u32 size;
	int ret;

	size = ipc_params->buffer.size;
	stream->num_pages = ipc_params->buffer.pages;
	stream->dmab = substream->runtime->dma_buffer_p;

	ret = acp_dsp_stream_config(sdev, stream);
	if (ret < 0) {
		dev_err(sdev->dev, "stream configuration failed\n");
		return ret;
	}

	ipc_params->buffer.phy_addr = stream->reg_offset;
	ipc_params->stream_tag = stream->stream_tag;

	/* write buffer size of stream in scratch memory */

	buf_offset = offsetof(struct scratch_reg_conf, buf_size);
	index = stream->stream_tag - 1;
	buf_offset = buf_offset + index * 4;

	snd_sof_dsp_write(sdev, ACP_DSP_BAR, ACP_SCRATCH_REG_0 + buf_offset, size);

	return 0;
}
EXPORT_SYMBOL_NS(acp_pcm_hw_params, SND_SOC_SOF_AMD_COMMON);

int acp_pcm_open(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream)
{
	struct acp_dsp_stream *stream;

	stream = acp_dsp_stream_get(sdev, 0);
	if (!stream)
		return -ENODEV;

	substream->runtime->private_data = stream;
	stream->substream = substream;

	return 0;
}
EXPORT_SYMBOL_NS(acp_pcm_open, SND_SOC_SOF_AMD_COMMON);

int acp_pcm_close(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream)
{
	struct acp_dsp_stream *stream;

	stream = substream->runtime->private_data;
	if (!stream) {
		dev_err(sdev->dev, "No open stream\n");
		return -EINVAL;
	}

	stream->substream = NULL;
	substream->runtime->private_data = NULL;

	return acp_dsp_stream_put(sdev, stream);
}
EXPORT_SYMBOL_NS(acp_pcm_close, SND_SOC_SOF_AMD_COMMON);

static const struct acp_pcm_table pcm_dev[] = {
	{I2S_BT, "I2SBT"},
	{I2S_SP, "I2SSP"},
	{PDM_DMIC, "DMIC"},
};

static enum acp_pcm_types get_id_from_list(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcm_dev); i++) {
		if (!strcmp(name, pcm_dev[i].pcm_name))
			return pcm_dev[i].pcm_index;
	}

	return PCM_NONE;
}

static u64 acp_get_byte_count(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream,
				enum acp_pcm_types pcm_id)
{
	u64 bytescount, low = 0, high = 0;
	u32 reg1 = 0, reg2 = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (pcm_id) {
		case I2S_BT:
			reg1 = ACP_BT_TX_LINEARPOSITIONCNTR_HIGH;
			reg2 = ACP_BT_TX_LINEARPOSITIONCNTR_LOW;
			break;
		case I2S_SP:
			reg1 = ACP_I2S_TX_LINEARPOSITIONCNTR_HIGH;
			reg2 = ACP_I2S_TX_LINEARPOSITIONCNTR_LOW;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (pcm_id) {
		case I2S_BT:
			reg1 = ACP_BT_RX_LINEARPOSITIONCNTR_HIGH;
			reg2 = ACP_BT_RX_LINEARPOSITIONCNTR_LOW;
			break;
		case I2S_SP:
			reg1 = ACP_I2S_RX_LINEARPOSITIONCNTR_HIGH;
			reg2 = ACP_I2S_RX_LINEARPOSITIONCNTR_LOW;
			break;
		case PDM_DMIC:
			reg1 = ACP_WOV_RX_LINEARPOSITIONCNTR_HIGH;
			reg2 = ACP_WOV_RX_LINEARPOSITIONCNTR_LOW;
			break;
		default:
			return -EINVAL;
		}
	}

	high = snd_sof_dsp_read(sdev, ACP_DSP_BAR, reg1);
	low = snd_sof_dsp_read(sdev, ACP_DSP_BAR, reg2);

	bytescount = (high << 32) | low;
	return bytescount;
}

snd_pcm_uframes_t acp_pcm_pointer(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *scomp = sdev->component;
	enum acp_pcm_types pcm_id = PCM_NONE;
	struct snd_sof_pcm *spcm;
	u32 pos, buffersize;
	u64 bytescount;

	spcm = snd_sof_find_spcm_dai(scomp, rtd);
	pcm_id = get_id_from_list(spcm->pcm.pcm_name);

	bytescount = acp_get_byte_count(sdev, substream, pcm_id);
	if (bytescount < 0)
		return -EINVAL;
	buffersize = frames_to_bytes(substream->runtime, substream->runtime->buffer_size);
	pos = do_div(bytescount, buffersize);

	return bytes_to_frames(substream->runtime, pos);
}
EXPORT_SYMBOL_NS(acp_pcm_pointer, SND_SOC_SOF_AMD_COMMON);
