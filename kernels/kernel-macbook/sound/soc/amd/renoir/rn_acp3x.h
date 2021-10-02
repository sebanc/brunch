/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * AMD ALSA SoC PDM Driver
 *
 * Copyright 2020 Advanced Micro Devices, Inc.
 */

#include "rn_chip_offset_byte.h"
#include <sound/pcm.h>

#define ACP_DEVS		5
#define ACP_PHY_BASE_ADDRESS 0x1240000
#define	ACP_REG_START	0x1240000
#define	ACP_REG_END	0x1250200
#define ACP_I2STDM_REG_START	0x1242400
#define ACP_I2STDM_REG_END	0x1242410
#define ACP_BT_TDM_REG_START	0x1242800
#define ACP_BT_TDM_REG_END	0x1242810

#define ACP_DEVICE_ID 0x15E2
#define ACP_POWER_ON 0x00
#define ACP_POWER_ON_IN_PROGRESS 0x01
#define ACP_POWER_OFF 0x02
#define ACP_POWER_OFF_IN_PROGRESS 0x03
#define ACP_SOFT_RESET_SOFTRESET_AUDDONE_MASK	0x00010001

#define ACP_PGFSM_CNTL_POWER_ON_MASK    0x01
#define ACP_PGFSM_CNTL_POWER_OFF_MASK   0x00
#define ACP_PGFSM_STATUS_MASK           0x03
#define ACP_POWERED_ON                  0x00
#define ACP_POWER_ON_IN_PROGRESS        0x01
#define ACP_POWERED_OFF                 0x02
#define ACP_POWER_OFF_IN_PROGRESS       0x03

#define ACP_ERROR_MASK 0x20000000
#define ACP_EXT_INTR_STAT_CLEAR_MASK 0xFFFFFFFF
#define PDM_DMA_STAT 0x10
#define PDM_DMA_INTR_MASK  0x10000
#define ACP_ERROR_STAT 29
#define PDM_DECIMATION_FACTOR 0x2
#define ACP_PDM_CLK_FREQ_MASK 0x07
#define ACP_WOV_MISC_CTRL_MASK 0x10
#define ACP_PDM_ENABLE 0x01
#define ACP_PDM_DISABLE 0x00
#define ACP_PDM_DMA_EN_STATUS 0x02
#define TWO_CH 0x02
#define FOUR_CH 0x04
#define DELAY_US 5
#define ACP_COUNTER 20000
/* time in ms for runtime suspend delay */
#define ACP_SUSPEND_DELAY_MS	2000

#define ACP_SRAM_PTE_OFFSET	0x02050000
#define PAGE_SIZE_4K_ENABLE     0x2
#define MEM_WINDOW_START	0x4000000

#define CAPTURE_MIN_NUM_PERIODS     4
#define CAPTURE_MAX_NUM_PERIODS     4
#define CAPTURE_MAX_PERIOD_SIZE     8192
#define CAPTURE_MIN_PERIOD_SIZE     4096

#define MAX_BUFFER (CAPTURE_MAX_PERIOD_SIZE * CAPTURE_MAX_NUM_PERIODS)
#define MIN_BUFFER MAX_BUFFER
#define	ACP_DMIC_AUTO   -1
#define PDM_OFFSET 0x8000
#define I2S_SP_INSTANCE                 0x01
#define I2S_BT_INSTANCE                 0x02

#define TDM_ENABLE 1
#define TDM_DISABLE 0

#define ACP_I2S_MODE 0x00
#define I2S_MODE	0x04
#define	I2S_RX_THRESHOLD	27
#define	I2S_TX_THRESHOLD	28
#define	BT_TX_THRESHOLD 26
#define	BT_RX_THRESHOLD 25

#define BT_RX_INTR_MASK 0x2000000
#define BT_TX_INTR_MASK 0x4000000
#define I2S_RX_INTR_MASK 0x8000000
#define I2S_TX_INTR_MASK 0x10000000

#define ACP_SRAM_SP_PB_PTE_OFFSET	0x60
#define ACP_SRAM_SP_CP_PTE_OFFSET	0x100
#define ACP_SRAM_BT_PB_PTE_OFFSET	0x200
#define ACP_SRAM_BT_CP_PTE_OFFSET	0x300
#define PAGE_SIZE_4K_ENABLE 0x2
#define I2S_SP_TX_MEM_WINDOW_START	0x400C000
#define I2S_SP_RX_MEM_WINDOW_START	0x4020000
#define I2S_BT_TX_MEM_WINDOW_START	0x4040000
#define I2S_BT_RX_MEM_WINDOW_START	0x4060000

#define SP_PB_FIFO_ADDR_OFFSET		0x500
#define SP_CAPT_FIFO_ADDR_OFFSET	0x700
#define BT_PB_FIFO_ADDR_OFFSET		0x900
#define BT_CAPT_FIFO_ADDR_OFFSET	0xB00

#define I2S_PLAYBACK_MIN_NUM_PERIODS    2
#define I2S_PLAYBACK_MAX_NUM_PERIODS    8
#define I2S_PLAYBACK_MAX_PERIOD_SIZE    8192
#define I2S_PLAYBACK_MIN_PERIOD_SIZE    1024
#define I2S_CAPTURE_MIN_NUM_PERIODS     2
#define I2S_CAPTURE_MAX_NUM_PERIODS     8
#define I2S_CAPTURE_MAX_PERIOD_SIZE     8192
#define I2S_CAPTURE_MIN_PERIOD_SIZE     1024

#define I2S_MAX_BUFFER (I2S_PLAYBACK_MAX_PERIOD_SIZE * I2S_PLAYBACK_MAX_NUM_PERIODS)
#define I2S_MIN_BUFFER I2S_MAX_BUFFER
#define FIFO_SIZE 0x100
#define DMA_SIZE 0x40
#define FRM_LEN 0x100

#define SLOT_WIDTH_8 0x08
#define SLOT_WIDTH_16 0x10
#define SLOT_WIDTH_24 0x18
#define SLOT_WIDTH_32 0x20
#define ACP3x_ITER_IRER_SAMP_LEN_MASK	0x38

#define PDM_PTE_OFFSET 0x00
#define PDM_MEM_WINDOW_START	0x4000000

struct acp3x_platform_info {
	u16 play_i2s_instance;
	u16 cap_i2s_instance;
	u16 capture_channel;
};

struct i2s_dev_data {
	bool tdm_mode;
	unsigned int i2s_irq;
	u16 i2s_instance;
	u32 tdm_fmt;
	u32 substream_type;
	void __iomem *acp_base;
	struct snd_pcm_substream *play_stream;
	struct snd_pcm_substream *capture_stream;
	struct snd_pcm_substream *i2ssp_play_stream;
	struct snd_pcm_substream *i2ssp_capture_stream;
};

struct i2s_stream_instance {
	u16 num_pages;
	u16 i2s_instance;
	u16 capture_channel;
	u16 direction;
	u16 channels;
	u32 xfer_resolution;
	u32 val;
	dma_addr_t dma_addr;
	u64 bytescount;
	void __iomem *acp_base;
};

struct pdm_dev_data {
	u32 pdm_irq;
	void __iomem *acp_base;
	struct snd_pcm_substream *capture_stream;
};

struct pdm_stream_instance {
	u16 num_pages;
	u16 channels;
	dma_addr_t dma_addr;
	u64 bytescount;
	void __iomem *acp_base;
};

union acp_pdm_dma_count {
	struct {
	u32 low;
	u32 high;
	} bcount;
	u64 bytescount;
};

static inline u32 rn_readl(void __iomem *base_addr)
{
	return readl(base_addr - ACP_PHY_BASE_ADDRESS);
}

static inline void rn_writel(u32 val, void __iomem *base_addr)
{
	writel(val, base_addr - ACP_PHY_BASE_ADDRESS);
}

static inline u64 acp_get_byte_count(struct i2s_stream_instance *rtd,
				     int direction)
{
	u64 byte_count;

	if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (rtd->i2s_instance) {
		case I2S_BT_INSTANCE:
			byte_count = rn_readl(rtd->acp_base +
					ACP_BT_TX_LINEARPOSITIONCNTR_HIGH);
			byte_count |= rn_readl(rtd->acp_base +
					ACP_BT_TX_LINEARPOSITIONCNTR_LOW);
			break;
		case I2S_SP_INSTANCE:
		default:
			byte_count = rn_readl(rtd->acp_base +
					ACP_I2S_TX_LINEARPOSITIONCNTR_HIGH);
			byte_count |= rn_readl(rtd->acp_base +
					ACP_I2S_TX_LINEARPOSITIONCNTR_LOW);
		}
	} else {
		switch (rtd->i2s_instance) {
		case I2S_BT_INSTANCE:
			byte_count = rn_readl(rtd->acp_base +
					ACP_BT_RX_LINEARPOSITIONCNTR_HIGH);
			byte_count |= rn_readl(rtd->acp_base +
					ACP_BT_RX_LINEARPOSITIONCNTR_LOW);
			break;
		case I2S_SP_INSTANCE:
		default:
			byte_count = rn_readl(rtd->acp_base +
					ACP_I2S_RX_LINEARPOSITIONCNTR_HIGH);
			byte_count |= rn_readl(rtd->acp_base +
					ACP_I2S_RX_LINEARPOSITIONCNTR_LOW);
		}
	}
	return byte_count;
}

void i2sdma_wq(struct i2s_dev_data *dev_data);
void pdmdma_wq(struct pdm_dev_data *dev_data);
