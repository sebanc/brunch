/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#ifndef __MEDIATEK_ADSP_PCM_H__
#define __MEDIATEK_ADSP_PCM_H__

#include "../sof-audio.h"

int mtk_adsp_stream_init(struct snd_sof_dev *sdev);
int mtk_adsp_pcm_open(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream);
int mtk_adsp_pcm_close(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream);
snd_pcm_uframes_t mtk_adsp_pcm_pointer(struct snd_sof_dev *sdev,
				       struct snd_pcm_substream *substream);
#endif
