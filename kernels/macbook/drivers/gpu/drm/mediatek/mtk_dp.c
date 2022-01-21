// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (c) 2021 BayLibre
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/hdmi-codec.h>
#include <video/videomode.h>

#include "mtk_dp_reg.h"

#define MTK_DP_AUX_WAIT_REPLY_COUNT 20
#define MTK_DP_CHECK_SINK_CAP_TIMEOUT_COUNT 3

#define MTK_DP_MAX_LANES 4
#define MTK_DP_MAX_LINK_RATE MTK_DP_LINKRATE_HBR3

#define MTK_DP_TBC_BUF_READ_START_ADDR 0x08

#define MTK_DP_TRAIN_RETRY_LIMIT 8
#define MTK_DP_TRAIN_MAX_ITERATIONS 5

#define MTK_DP_AUX_WRITE_READ_WAIT_TIME_US 20

#define MTK_DP_DP_VERSION_11 0x11

enum mtk_dp_state {
	MTK_DP_STATE_INITIAL,
	MTK_DP_STATE_IDLE,
	MTK_DP_STATE_PREPARE,
	MTK_DP_STATE_NORMAL,
};

enum mtk_dp_train_state {
	MTK_DP_TRAIN_STATE_STARTUP = 0,
	MTK_DP_TRAIN_STATE_CHECKCAP,
	MTK_DP_TRAIN_STATE_CHECKEDID,
	MTK_DP_TRAIN_STATE_TRAINING_PRE,
	MTK_DP_TRAIN_STATE_TRAINING,
	MTK_DP_TRAIN_STATE_CHECKTIMING,
	MTK_DP_TRAIN_STATE_NORMAL,
	MTK_DP_TRAIN_STATE_POWERSAVE,
	MTK_DP_TRAIN_STATE_DPIDLE,
};

struct mtk_dp_timings {
	struct videomode vm;

	u16 htotal;
	u16 vtotal;
	u8 frame_rate;
	u32 pix_rate_khz;
};

struct mtk_dp_train_info {
	bool tps3;
	bool tps4;
	bool sink_ssc;
	bool cable_plugged_in;
	bool cable_state_change;
	bool cr_done;
	bool eq_done;

	// link_rate is in multiple of 0.27Gbps
	int link_rate;
	int lane_count;

	int irq_status;
	int check_cap_count;
};

// Same values as used by the DP Spec for MISC0 bits 1 and 2
enum mtk_dp_color_format {
	MTK_DP_COLOR_FORMAT_RGB_444 = 0,
	MTK_DP_COLOR_FORMAT_YUV_422 = 1,
	MTK_DP_COLOR_FORMAT_YUV_444 = 2,
	MTK_DP_COLOR_FORMAT_YUV_420 = 3,
	MTK_DP_COLOR_FORMAT_YONLY = 4,
	MTK_DP_COLOR_FORMAT_RAW = 5,
	MTK_DP_COLOR_FORMAT_RESERVED = 6,
	MTK_DP_COLOR_FORMAT_DEFAULT = MTK_DP_COLOR_FORMAT_RGB_444,
	MTK_DP_COLOR_FORMAT_UNKNOWN = 15,
};

// Multiple of 0.27Gbps
enum mtk_dp_linkrate {
	MTK_DP_LINKRATE_RBR = 0x6,
	MTK_DP_LINKRATE_HBR = 0xA,
	MTK_DP_LINKRATE_HBR2 = 0x14,
	MTK_DP_LINKRATE_HBR25 = 0x19,
	MTK_DP_LINKRATE_HBR3 = 0x1E,
};

// Same values as used for DP Spec MISC0 bits 5,6,7
enum mtk_dp_color_depth {
	MTK_DP_COLOR_DEPTH_6BIT = 0,
	MTK_DP_COLOR_DEPTH_8BIT = 1,
	MTK_DP_COLOR_DEPTH_10BIT = 2,
	MTK_DP_COLOR_DEPTH_12BIT = 3,
	MTK_DP_COLOR_DEPTH_16BIT = 4,
	MTK_DP_COLOR_DEPTH_UNKNOWN = 5,
};

struct mtk_dp_audio_cfg {
	int sample_rate;
	int word_length_bits;
	int channels;
};

struct mtk_dp_info {
	enum mtk_dp_color_depth depth;
	enum mtk_dp_color_format format;
	struct mtk_dp_audio_cfg audio_caps;
	struct mtk_dp_timings timings;
};

struct dp_cal_data {
	unsigned int glb_bias_trim;
	unsigned int clktx_impse;

	unsigned int ln0_tx_impsel_pmos;
	unsigned int ln0_tx_impsel_nmos;
	unsigned int ln1_tx_impsel_pmos;
	unsigned int ln1_tx_impsel_nmos;
	unsigned int ln2_tx_impsel_pmos;
	unsigned int ln2_tx_impsel_nmos;
	unsigned int ln3_tx_impsel_pmos;
	unsigned int ln3_tx_impsel_nmos;
};

struct mtk_dp {
	struct device *dev;
	struct platform_device *phy_dev;
	struct phy *phy;
	struct dp_cal_data cal_data;

	struct drm_device *drm_dev;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_dp_aux aux;

	struct mutex edid_lock;
	struct edid *edid;

	u8 rx_cap[DP_RECEIVER_CAP_SIZE];

	struct mtk_dp_info info;
	enum mtk_dp_state state;

	struct mtk_dp_train_info train_info;
	enum mtk_dp_train_state train_state;
	unsigned int input_fmt;

	struct regmap *regs;
	struct clk *dp_tx_clk;

	bool enabled;
	bool audio_enable;

	bool has_fec;
	struct mutex dp_lock;

	struct mutex update_plugged_status_lock;

	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	u8 connector_eld[MAX_ELD_BYTES];
};

enum mtk_dp_sdp_type {
	MTK_DP_SDP_NONE = 0x00,
	MTK_DP_SDP_ACM = 0x01,
	MTK_DP_SDP_ISRC = 0x02,
	MTK_DP_SDP_AVI = 0x03,
	MTK_DP_SDP_AUI = 0x04,
	MTK_DP_SDP_SPD = 0x05,
	MTK_DP_SDP_MPEG = 0x06,
	MTK_DP_SDP_NTSC = 0x07,
	MTK_DP_SDP_VSP = 0x08,
	MTK_DP_SDP_VSC = 0x09,
	MTK_DP_SDP_EXT = 0x0A,
	MTK_DP_SDP_PPS0 = 0x0B,
	MTK_DP_SDP_PPS1 = 0x0C,
	MTK_DP_SDP_PPS2 = 0x0D,
	MTK_DP_SDP_PPS3 = 0x0E,
	MTK_DP_SDP_DRM = 0x10,
	MTK_DP_SDP_MAX_NUM
};

struct mtk_dp_sdp_packet {
	enum mtk_dp_sdp_type type;
	struct dp_sdp sdp;
};

#define MTK_DP_IEC_CHANNEL_STATUS_LEN 5
union mtk_dp_audio_channel_status {
	struct {
		u8 rev : 1;
		u8 is_lpcm : 1;
		u8 copy_right : 1;
		u8 additional_format_info : 3;
		u8 channel_status_mode : 2;
		u8 category_code;
		u8 src_num : 4;
		u8 channel_num : 4;
		u8 sampling_freq : 4;
		u8 clk_accuracy : 2;
		u8 rev2 : 2;
		u8 word_len : 4;
		u8 original_sampling_freq : 4;
	} iec;

	u8 buf[MTK_DP_IEC_CHANNEL_STATUS_LEN];
};

static struct regmap_config mtk_dp_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = SEC_OFFSET + 0x90,
	.name = "mtk-dp-registers",
};

static bool mtk_dp_is_edp(struct mtk_dp *mtk_dp)
{
	return mtk_dp->next_bridge != NULL;
}

static struct mtk_dp *mtk_dp_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dp, bridge);
}

static u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val;
	int ret;

	ret = regmap_read(mtk_dp->regs, offset, &read_val);
	if (ret) {
		dev_err(mtk_dp->dev, "Failed to read register 0x%x: %d\n",
			offset, ret);
		return 0;
	}

	return read_val;
}

static void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	int ret;

	ret = regmap_write(mtk_dp->regs, offset, val);
	if (ret)
		dev_err(mtk_dp->dev,
			"Failed to write register 0x%x with value 0x%x: %d\n",
			offset, val, ret);
}

static void mtk_dp_update_bits(struct mtk_dp *mtk_dp, u32 offset, u32 val,
			       u32 mask)
{
	int ret;

	ret = regmap_update_bits(mtk_dp->regs, offset, mask, val);
	if (ret)
		dev_err(mtk_dp->dev,
			"Failed to update register 0x%x with value 0x%x, mask 0x%x: %d\n",
			offset, val, mask, ret);
}

static void mtk_dp_bulk_16bit_write(struct mtk_dp *mtk_dp, u32 offset, u8 *buf,
				    size_t length)
{
	int i;
	int num_regs = (length + 1) / 2;

	// 2 bytes per register
	for (i = 0; i < num_regs; i++) {
		u32 val = buf[i * 2] |
			  (i * 2 + 1 < length ? buf[i * 2 + 1] << 8 : 0);

		mtk_dp_write(mtk_dp, offset + i * 4, val);
	}
}

static unsigned long mtk_dp_sip_atf_call(unsigned int cmd, unsigned int para)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_DP_SIP_CONTROL_AARCH32, cmd, para, 0, 0, 0, 0, 0,
		      &res);

	pr_debug("[DPTX]%s cmd 0x%x, p1 0x%x, ret 0x%lx-0x%lx", __func__, cmd,
		 para, res.a0, res.a1);
	return res.a1;
}

static void mtk_dp_msa_bypass_disable(struct mtk_dp *mtk_dp)
{
	const u16 bits_to_set =
		BIT(HTOTAL_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VTOTAL_SEL_DP_ENC0_P0_SHIFT) |
		BIT(HSTART_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VSTART_SEL_DP_ENC0_P0_SHIFT) |
		BIT(HWIDTH_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VHEIGHT_SEL_DP_ENC0_P0_SHIFT) |
		BIT(HSP_SEL_DP_ENC0_P0_SHIFT) | BIT(HSW_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VSP_SEL_DP_ENC0_P0_SHIFT) | BIT(VSW_SEL_DP_ENC0_P0_SHIFT);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3030, bits_to_set,
			   bits_to_set);
}

static void mtk_dp_set_msa(struct mtk_dp *mtk_dp)
{
	struct mtk_dp_timings *timings = &mtk_dp->info.timings;

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3010, timings->htotal,
			   HTOTAL_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3018,
			   timings->vm.hsync_len + timings->vm.hback_porch,
			   HSTART_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3028,
			   timings->vm.hsync_len << HSW_SW_DP_ENC0_P0_SHIFT,
			   HSW_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3028, 0,
			   HSP_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3020, timings->vm.hactive,
			   HWIDTH_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3014, timings->vtotal,
			   VTOTAL_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_301C,
			   timings->vm.vsync_len + timings->vm.vback_porch,
			   VSTART_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_302C,
			   timings->vm.vsync_len << VSW_SW_DP_ENC0_P0_SHIFT,
			   VSW_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_302C, 0,
			   VSP_SW_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3024, timings->vm.vactive,
			   VHEIGHT_SW_DP_ENC0_P0_MASK);

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3064, timings->vm.hactive,
			   HDE_NUM_LAST_DP_ENC0_P0_MASK);

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3154, timings->htotal,
			   PGEN_HTOTAL_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3158,
			   timings->vm.hfront_porch,
			   PGEN_HSYNC_RISING_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_315C, timings->vm.hsync_len,
			   PGEN_HSYNC_PULSE_WIDTH_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3160,
			   timings->vm.hback_porch + timings->vm.hsync_len,
			   PGEN_HFDE_START_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3164, timings->vm.hactive,
			   PGEN_HFDE_ACTIVE_WIDTH_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3168, timings->vtotal,
			   PGEN_VTOTAL_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_316C,
			   timings->vm.vfront_porch,
			   PGEN_VSYNC_RISING_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3170, timings->vm.vsync_len,
			   PGEN_VSYNC_PULSE_WIDTH_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3174,
			   timings->vm.vback_porch + timings->vm.vsync_len,
			   PGEN_VFDE_START_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3178, timings->vm.vactive,
			   PGEN_VFDE_ACTIVE_WIDTH_DP_ENC0_P0_MASK);
}

static void mtk_dp_set_color_format(struct mtk_dp *mtk_dp,
				    enum mtk_dp_color_format color_format)
{
	u32 val;

	mtk_dp->info.format = color_format;

	// Update MISC0
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3034,
			   color_format << DP_TEST_COLOR_FORMAT_SHIFT,
			   DP_TEST_COLOR_FORMAT_MASK);

	switch (color_format) {
	case MTK_DP_COLOR_FORMAT_YUV_422:
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR422;
		break;
	case MTK_DP_COLOR_FORMAT_YUV_420:
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR420;
		break;
	case MTK_DP_COLOR_FORMAT_YONLY:
	case MTK_DP_COLOR_FORMAT_RAW:
	case MTK_DP_COLOR_FORMAT_RESERVED:
	case MTK_DP_COLOR_FORMAT_UNKNOWN:
		drm_warn(mtk_dp->drm_dev, "Unsupported color format: %d\n",
			 color_format);
		fallthrough;
	case MTK_DP_COLOR_FORMAT_RGB_444:
	case MTK_DP_COLOR_FORMAT_YUV_444:
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_RGB;
		break;
	}

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_303C, val,
			   PIXEL_ENCODE_FORMAT_DP_ENC0_P0_MASK);
}

static void mtk_dp_set_color_depth(struct mtk_dp *mtk_dp,
				   enum mtk_dp_color_depth color_depth)
{
	u32 val;

	mtk_dp->info.depth = color_depth;

	// Update MISC0
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3034,
			   color_depth << DP_TEST_BIT_DEPTH_SHIFT,
			   DP_TEST_BIT_DEPTH_MASK);

	switch (color_depth) {
	case MTK_DP_COLOR_DEPTH_6BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_6BIT;
		break;
	case MTK_DP_COLOR_DEPTH_8BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_8BIT;
		break;
	case MTK_DP_COLOR_DEPTH_10BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_10BIT;
		break;
	case MTK_DP_COLOR_DEPTH_12BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_12BIT;
		break;
	case MTK_DP_COLOR_DEPTH_16BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_16BIT;
		break;
	case MTK_DP_COLOR_DEPTH_UNKNOWN:
		drm_warn(mtk_dp->drm_dev, "Unsupported color depth %d\n",
			 color_depth);
		return;
	}

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_303C, val,
			   VIDEO_COLOR_DEPTH_DP_ENC0_P0_MASK);
}

static void mtk_dp_mn_overwrite_disable(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3004, 0,
			   VIDEO_M_CODE_SEL_DP_ENC0_P0_MASK);
}

static void mtk_dp_set_sram_read_start(struct mtk_dp *mtk_dp, u32 val)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_303C,
			   val << SRAM_START_READ_THRD_DP_ENC0_P0_SHIFT,
			   SRAM_START_READ_THRD_DP_ENC0_P0_MASK);
}

static void mtk_dp_setup_encoder(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_303C,
			   BIT(VIDEO_MN_GEN_EN_DP_ENC0_P0_SHIFT),
			   VIDEO_MN_GEN_EN_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3040,
			   0x20 << SDP_DOWN_CNT_INIT_DP_ENC0_P0_SHIFT,
			   SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3364,
			   0x20 << SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_SHIFT,
			   SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3300,
			   2 << VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_SHIFT,
			   VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3364,
			   4 << FIFO_READ_START_POINT_DP_ENC1_P0_SHIFT,
			   FIFO_READ_START_POINT_DP_ENC1_P0_MASK);
	mtk_dp_write(mtk_dp, MTK_DP_ENC1_P0_3368,
		     1 << VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENC1_P0_SHIFT |
			     1 << VIDEO_STABLE_CNT_THRD_DP_ENC1_P0_SHIFT |
			     BIT(SDP_DP13_EN_DP_ENC1_P0_SHIFT) |
			     1 << BS2BS_MODE_DP_ENC1_P0_SHIFT);
}

static void mtk_dp_pg_disable(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3038, 0,
			   VIDEO_SOURCE_SEL_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_31B0,
			   4 << PGEN_PATTERN_SEL_SHIFT, PGEN_PATTERN_SEL_MASK);
}

static void mtk_dp_audio_setup_channels(struct mtk_dp *mtk_dp,
					struct mtk_dp_audio_cfg *cfg)
{
	u32 channel_enable_bits;

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3324,
			   AUDIO_SOURCE_MUX_DP_ENC1_P0_DPRX,
			   AUDIO_SOURCE_MUX_DP_ENC1_P0_MASK);

	//audio channel count change reset
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_33F4, BIT(9), BIT(9));

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3304,
			   AU_PRTY_REGEN_DP_ENC1_P0_MASK,
			   AU_PRTY_REGEN_DP_ENC1_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3304,
			   AU_CH_STS_REGEN_DP_ENC1_P0_MASK,
			   AU_CH_STS_REGEN_DP_ENC1_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3304,
			   AUDIO_SAMPLE_PRSENT_REGEN_DP_ENC1_P0_MASK,
			   AUDIO_SAMPLE_PRSENT_REGEN_DP_ENC1_P0_MASK);

	switch (cfg->channels) {
	case 2:
		channel_enable_bits = AUDIO_2CH_SEL_DP_ENC0_P0_MASK |
				      AUDIO_2CH_EN_DP_ENC0_P0_MASK;
		break;
	case 8:
	default:
		channel_enable_bits = AUDIO_8CH_SEL_DP_ENC0_P0_MASK |
				      AUDIO_8CH_EN_DP_ENC0_P0_MASK;
		break;
	}
	mtk_dp_update_bits(
		mtk_dp, MTK_DP_ENC0_P0_3088,
		channel_enable_bits | AU_EN_DP_ENC0_P0_MASK,
		AUDIO_2CH_SEL_DP_ENC0_P0_MASK | AUDIO_2CH_EN_DP_ENC0_P0_MASK |
			AUDIO_8CH_SEL_DP_ENC0_P0_MASK |
			AUDIO_8CH_EN_DP_ENC0_P0_MASK | AU_EN_DP_ENC0_P0_MASK);

	//audio channel count change reset
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_33F4, 0, BIT(9));

	//enable audio reset
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_33F4, BIT(0), BIT(0));
}

static void mtk_dp_audio_channel_status_set(struct mtk_dp *mtk_dp,
					    struct mtk_dp_audio_cfg *cfg)
{
	union mtk_dp_audio_channel_status channel_status;

	memset(&channel_status, 0, sizeof(channel_status));

	switch (cfg->sample_rate) {
	case 32000:
		channel_status.iec.sampling_freq = 3;
		break;
	case 44100:
		channel_status.iec.sampling_freq = 0;
		break;
	case 48000:
		channel_status.iec.sampling_freq = 2;
		break;
	case 88200:
		channel_status.iec.sampling_freq = 8;
		break;
	case 96000:
		channel_status.iec.sampling_freq = 0xA;
		break;
	case 192000:
		channel_status.iec.sampling_freq = 0xE;
		break;
	default:
		channel_status.iec.sampling_freq = 0x1;
		break;
	}

	switch (cfg->word_length_bits) {
	case 16:
		channel_status.iec.word_len = 0x02;
		break;
	case 20:
		channel_status.iec.word_len = 0x03;
		break;
	case 24:
		channel_status.iec.word_len = 0x0B;
		break;
	}

	// IEC 60958 consumer channel status bits
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_308C,
			   channel_status.buf[1] << 8 | channel_status.buf[0],
			   CH_STATUS_0_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3090,
			   channel_status.buf[3] << 8 | channel_status.buf[2],
			   CH_STATUS_1_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3094, channel_status.buf[4],
			   CH_STATUS_2_DP_ENC0_P0_MASK);
}

static void mtk_dp_audio_sdp_asp_set_channels(struct mtk_dp *mtk_dp,
					      int channels)
{
	if (channels != 2 && channels != 8)
		channels = 8;

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_312C,
			   (channels - 1) << ASP_HB3_DP_ENC0_P0_SHIFT,
			   ASP_HB2_DP_ENC0_P0_MASK | ASP_HB3_DP_ENC0_P0_MASK);
}

static void mtk_dp_audio_set_divider(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30BC,
			   AUDIO_M_CODE_MULT_DIV_SEL_DP_ENC0_P0_DIV_2,
			   AUDIO_M_CODE_MULT_DIV_SEL_DP_ENC0_P0_MASK);
}

static bool mtk_dp_plug_state(struct mtk_dp *mtk_dp)
{
	return !!(mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_3414) &
		  HPD_DB_DP_TRANS_P0_MASK);
}

static void mtk_dp_sdp_trigger_packet(struct mtk_dp *mtk_dp,
				      enum mtk_dp_sdp_type type)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3280, type,
			   SDP_PACKET_TYPE_DP_ENC1_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3280, SDP_PACKET_W_DP_ENC1_P0,
			   SDP_PACKET_W_DP_ENC1_P0);
}

static void mtk_dp_sdp_set_data(struct mtk_dp *mtk_dp, u8 *data_bytes)
{
	mtk_dp_bulk_16bit_write(mtk_dp, MTK_DP_ENC1_P0_3200, data_bytes, 0x10);
}

static void mtk_dp_sdp_set_header(struct mtk_dp *mtk_dp,
				  enum mtk_dp_sdp_type type,
				  struct dp_sdp_header *header)
{
	u32 db_addr;

	switch (type) {
	case MTK_DP_SDP_DRM:
		db_addr = MTK_DP_ENC0_P0_3138;
		break;
	case MTK_DP_SDP_PPS0:
	case MTK_DP_SDP_PPS1:
	case MTK_DP_SDP_PPS2:
	case MTK_DP_SDP_PPS3:
		db_addr = MTK_DP_ENC0_P0_3130;
		break;
	default:
		db_addr = MTK_DP_ENC0_P0_30D8 + (type - MTK_DP_SDP_ACM) * 8;
	}

	mtk_dp_bulk_16bit_write(mtk_dp, db_addr, (u8 *)header, 4);
}

static const u32 mtk_dp_sdp_type_to_reg[MTK_DP_SDP_MAX_NUM] = {
	/* MTK_DP_SDP_NONE => */ 0x0,
	/* MTK_DP_SDP_ACM  => */ MTK_DP_ENC0_P0_30B4,
	/* MTK_DP_SDP_ISRC => */ MTK_DP_ENC0_P0_30B4 + 1,
	/* MTK_DP_SDP_AVI  => */ MTK_DP_ENC0_P0_30A4 + 1,
	/* MTK_DP_SDP_AUI  => */ MTK_DP_ENC0_P0_30A8,
	/* MTK_DP_SDP_SPD  => */ MTK_DP_ENC0_P0_30A8 + 1,
	/* MTK_DP_SDP_MPEG => */ MTK_DP_ENC0_P0_30AC,
	/* MTK_DP_SDP_NTSC => */ MTK_DP_ENC0_P0_30AC + 1,
	/* MTK_DP_SDP_VSP  => */ MTK_DP_ENC0_P0_30B0,
	/* MTK_DP_SDP_VSC  => */ MTK_DP_ENC0_P0_30B8,
	/* MTK_DP_SDP_EXT  => */ MTK_DP_ENC0_P0_30B0 + 1,
	/* MTK_DP_SDP_PPS0 => */ MTK_DP_ENC0_P0_31E8,
	/* MTK_DP_SDP_PPS1 => */ MTK_DP_ENC0_P0_31E8,
	/* MTK_DP_SDP_PPS2 => */ MTK_DP_ENC0_P0_31E8,
	/* MTK_DP_SDP_PPS3 => */ MTK_DP_ENC0_P0_31E8,
	/* MTK_DP_SDP_DRM  => */ MTK_DP_ENC0_P0_31DC,
};

static void mtk_dp_disable_sdp(struct mtk_dp *mtk_dp, enum mtk_dp_sdp_type type)
{
	if (type == MTK_DP_SDP_NONE)
		return;

	// Disable periodic send
	mtk_dp_update_bits(mtk_dp, mtk_dp_sdp_type_to_reg[type] & 0xfffc, 0,
			   0xFF << ((mtk_dp_sdp_type_to_reg[type] & 3) * 8));
}

static void mtk_dp_setup_sdp(struct mtk_dp *mtk_dp,
			     struct mtk_dp_sdp_packet *packet)
{
	mtk_dp_sdp_set_data(mtk_dp, packet->sdp.db);
	mtk_dp_sdp_set_header(mtk_dp, packet->type, &packet->sdp.sdp_header);

	mtk_dp_disable_sdp(mtk_dp, packet->type);

	switch (packet->type) {
	case MTK_DP_SDP_NONE:
		break;
	case MTK_DP_SDP_ISRC:
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_31EC,
				   0x1C << ISRC1_HB3_DP_ENC0_P0_SHIFT,
				   ISRC1_HB3_DP_ENC0_P0_MASK);
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3280, MTK_DP_SDP_ISRC,
				   SDP_PACKET_TYPE_DP_ENC1_P0_MASK);

		if (packet->sdp.sdp_header.HB3 & BIT(2))
			mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30BC,
					   BIT(ISRC_CONT_DP_ENC0_P0_SHIFT),
					   ISRC_CONT_DP_ENC0_P0_MASK);
		else
			mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30BC, 0,
					   ISRC_CONT_DP_ENC0_P0_MASK);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3280,
				   SDP_PACKET_W_DP_ENC1_P0,
				   SDP_PACKET_W_DP_ENC1_P0);
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30B4,
				   5 << ISRC_CFG_DP_ENC0_P0_SHIFT,
				   ISRC_CFG_DP_ENC0_P0_MASK);
		break;
	case MTK_DP_SDP_DRM:
		mtk_dp_sdp_trigger_packet(mtk_dp, packet->type);
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_31DC,
				   5 << HDR0_CFG_DP_ENC0_P0_SHIFT,
				   HDR0_CFG_DP_ENC0_P0_MASK);
		break;
	case MTK_DP_SDP_ACM:
	case MTK_DP_SDP_AVI:
	case MTK_DP_SDP_AUI:
	case MTK_DP_SDP_SPD:
	case MTK_DP_SDP_MPEG:
	case MTK_DP_SDP_NTSC:
	case MTK_DP_SDP_VSP:
	case MTK_DP_SDP_VSC:
	case MTK_DP_SDP_EXT:
	case MTK_DP_SDP_PPS0:
	case MTK_DP_SDP_PPS1:
	case MTK_DP_SDP_PPS2:
	case MTK_DP_SDP_PPS3:
		mtk_dp_sdp_trigger_packet(mtk_dp, packet->type);
		// Enable periodic sending
		mtk_dp_update_bits(
			mtk_dp, mtk_dp_sdp_type_to_reg[packet->type] & 0xfffc,
			0x05 << ((mtk_dp_sdp_type_to_reg[packet->type] & 3) *
				 8),
			0xff << ((mtk_dp_sdp_type_to_reg[packet->type] & 3) *
				 8));
		break;
	default:
		break;
	}
}

static void mtk_dp_sdp_vsc_ext_disable(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30A0, 0,
			   BIT(7) | BIT(8) | BIT(12));
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_328C, 0, BIT(7));
}

static void mtk_dp_aux_irq_clear(struct mtk_dp *mtk_dp)
{
	mtk_dp_write(
		mtk_dp, MTK_DP_AUX_P0_3640,
		BIT(AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_SHIFT) |
			BIT(AUX_RX_DATA_RECV_IRQ_AUX_TX_P0_SHIFT) |
			BIT(AUX_RX_ADDR_RECV_IRQ_AUX_TX_P0_SHIFT) |
			BIT(AUX_RX_CMD_RECV_IRQ_AUX_TX_P0_SHIFT) |
			BIT(AUX_RX_MCCS_RECV_COMPLETE_IRQ_AUX_TX_P0_SHIFT) |
			BIT(AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_SHIFT) |
			BIT(AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0_SHIFT));
}

static void mtk_dp_aux_set_cmd(struct mtk_dp *mtk_dp, u8 cmd, u32 addr)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3644, cmd,
			   MCU_REQUEST_COMMAND_AUX_TX_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3648, addr,
			   MCU_REQUEST_ADDRESS_LSB_AUX_TX_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_364C, addr >> 16,
			   MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_MASK);
}

static void mtk_dp_aux_cmd_complete(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3650,
			   BIT(MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_SHIFT),
			   MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_MASK |
				   PHY_FIFO_RST_AUX_TX_P0_MASK |
				   MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
}

static void mtk_dp_aux_request_ready(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3630,
			   BIT(AUX_TX_REQUEST_READY_AUX_TX_P0_SHIFT),
			   AUX_TX_REQUEST_READY_AUX_TX_P0_MASK);
}

static void mtk_dp_aux_fill_write_fifo(struct mtk_dp *mtk_dp, u8 *buf,
				       size_t length)
{
	mtk_dp_bulk_16bit_write(mtk_dp, MTK_DP_AUX_P0_3708, buf, length);
}

static void mtk_dp_aux_read_rx_fifo(struct mtk_dp *mtk_dp, u8 *buf,
				    size_t length, int read_delay)
{
	int read_pos;

	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3620, 0,
			   AUX_RD_MODE_AUX_TX_P0_MASK);

	for (read_pos = 0; read_pos < length; read_pos++) {
		mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3620,
				   BIT(AUX_RX_FIFO_R_PULSE_TX_P0_SHIFT),
				   AUX_RX_FIFO_READ_PULSE_TX_P0_MASK);
		usleep_range(read_delay, read_delay * 2);
		buf[read_pos] =
			(u8)(mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3620) &
			     AUX_RX_FIFO_READ_DATA_AUX_TX_P0_MASK >>
				     AUX_RX_FIFO_READ_DATA_AUX_TX_P0_SHIFT);
	}
}

static void mtk_dp_aux_set_length(struct mtk_dp *mtk_dp, size_t length)
{
	if (length > 0) {
		mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3650,
				   (length - 1)
					   << MCU_REQ_DATA_NUM_AUX_TX_P0_SHIFT,
				   MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
		mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_362C, 0,
				   AUX_NO_LENGTH_AUX_TX_P0_MASK |
					   AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
					   AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	} else {
		mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_362C,
				   BIT(AUX_NO_LENGTH_AUX_TX_P0_SHIFT),
				   AUX_NO_LENGTH_AUX_TX_P0_MASK |
					   AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
					   AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	}
}

static int mtk_dp_aux_wait_for_completion(struct mtk_dp *mtk_dp, bool is_read)
{
	int wait_reply = MTK_DP_AUX_WAIT_REPLY_COUNT;

	while (--wait_reply) {
		u32 aux_irq_status;

		if (is_read) {
			u32 fifo_status =
				mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3618);

			if (fifo_status &
			    (AUX_RX_FIFO_WRITE_POINTER_AUX_TX_P0_MASK |
			     AUX_RX_FIFO_FULL_AUX_TX_P0_MASK)) {
				return 0;
			}
		}

		aux_irq_status = mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3640);
		if (aux_irq_status & AUX_RX_RECV_COMPLETE_IRQ_TX_P0_MASK)
			return 0;

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_MASK)
			return -ETIMEDOUT;

		usleep_range(100, 500);
	}

	return -ETIMEDOUT;
}

static int mtk_dp_aux_do_transfer(struct mtk_dp *mtk_dp, bool is_read, u8 cmd,
				  u32 addr, u8 *buf, size_t length)
{
	int ret;
	u32 reply_cmd;

	dev_dbg(mtk_dp->dev, "AUX transfer is_read(%d) cmd(%d) addr(0x%x) length(%zu)\n",
		is_read, cmd, addr, length);
	if (is_read && (length > DP_AUX_MAX_PAYLOAD_BYTES ||
			(cmd == DP_AUX_NATIVE_READ && !length))) {
		dev_err(mtk_dp->dev, "AUX err: read lenth > 16 or length = 0\n");
		return -EINVAL;
	}

	if (!is_read)
		mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3704,
				   BIT(AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_SHIFT),
				   AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_MASK);

	mtk_dp_aux_cmd_complete(mtk_dp);
	mtk_dp_aux_irq_clear(mtk_dp);
	usleep_range(MTK_DP_AUX_WRITE_READ_WAIT_TIME_US,
		     MTK_DP_AUX_WRITE_READ_WAIT_TIME_US * 2);

	mtk_dp_aux_set_cmd(mtk_dp, cmd, addr);
	mtk_dp_aux_set_length(mtk_dp, length);

	if (!is_read) {
		if (length)
			mtk_dp_aux_fill_write_fifo(mtk_dp, buf, length);

		mtk_dp_update_bits(
			mtk_dp, MTK_DP_AUX_P0_3704,
			AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_MASK,
			AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_MASK);
	}

	mtk_dp_aux_request_ready(mtk_dp);

	ret = mtk_dp_aux_wait_for_completion(mtk_dp, is_read);

	reply_cmd = mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3624) &
		    AUX_RX_REPLY_COMMAND_AUX_TX_P0_MASK;

	if (ret || reply_cmd) {
		u32 phy_status = mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3628) &
				 AUX_RX_PHY_STATE_AUX_TX_P0_MASK;
		if (phy_status != AUX_RX_PHY_STATE_AUX_TX_P0_RX_IDLE) {
			drm_err(mtk_dp->drm_dev,
				"AUX Rx Aux hang, need SW reset\n");
			ret = -EIO;
		}

		mtk_dp_aux_cmd_complete(mtk_dp);
		mtk_dp_aux_irq_clear(mtk_dp);

		usleep_range(MTK_DP_AUX_WRITE_READ_WAIT_TIME_US,
			     MTK_DP_AUX_WRITE_READ_WAIT_TIME_US * 2);
		dev_err(mtk_dp->dev, "AUX err: wait completion timeout\n");
		return -ETIMEDOUT;
	}

	if (!length) {
		mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_362C, 0,
				   AUX_NO_LENGTH_AUX_TX_P0_MASK |
					   AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
					   AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	} else if (is_read) {
		int read_delay;

		if (cmd == (DP_AUX_I2C_READ | DP_AUX_I2C_MOT) ||
		    cmd == DP_AUX_I2C_READ)
			read_delay = 500;
		else
			read_delay = 100;

		mtk_dp_aux_read_rx_fifo(mtk_dp, buf, length, read_delay);
	}

	mtk_dp_aux_cmd_complete(mtk_dp);
	mtk_dp_aux_irq_clear(mtk_dp);
	usleep_range(MTK_DP_AUX_WRITE_READ_WAIT_TIME_US,
		     MTK_DP_AUX_WRITE_READ_WAIT_TIME_US * 2);

	return 0;
}

static bool mtk_dp_set_swing_pre_emphasis(struct mtk_dp *mtk_dp, int lane_num,
					  int swing_val, int preemphasis)
{
	u32 lane_shift = lane_num * DP_TX1_VOLT_SWING_SHIFT;

	if (lane_num < 0 || lane_num > 3)
		return false;

	dev_dbg(mtk_dp->dev,
		"link training swing_val= 0x%x, preemphasis = 0x%x\n",
		swing_val, preemphasis);

	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_SWING_EMP,
			   swing_val << (DP_TX0_VOLT_SWING_SHIFT + lane_shift),
			   DP_TX0_VOLT_SWING_MASK << lane_shift);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_SWING_EMP,
			   preemphasis << (DP_TX0_PRE_EMPH_SHIFT + lane_shift),
			   DP_TX0_PRE_EMPH_MASK << lane_shift);

	return true;
}

static void mtk_dp_reset_swing_pre_emphasis(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_SWING_EMP, 0,
			   DP_TX0_VOLT_SWING_MASK | DP_TX1_VOLT_SWING_MASK |
				   DP_TX2_VOLT_SWING_MASK |
				   DP_TX3_VOLT_SWING_MASK |
				   DP_TX0_PRE_EMPH_MASK | DP_TX1_PRE_EMPH_MASK |
				   DP_TX2_PRE_EMPH_MASK | DP_TX3_PRE_EMPH_MASK);
}

static void mtk_dp_fec_enable(struct mtk_dp *mtk_dp, bool enable)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3540,
			   enable ? BIT(FEC_EN_DP_TRANS_P0_SHIFT) : 0,
			   FEC_EN_DP_TRANS_P0_MASK);
}

static u32 mtk_dp_swirq_get_clear(struct mtk_dp *mtk_dp)
{
	u32 irq_status = mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_35D0) &
			 SW_IRQ_FINAL_STATUS_DP_TRANS_P0_MASK;

	if (irq_status) {
		mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_35C8, irq_status,
				   SW_IRQ_CLR_DP_TRANS_P0_MASK);
		mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_35C8, 0,
				   SW_IRQ_CLR_DP_TRANS_P0_MASK);
	}

	return irq_status;
}

static u32 mtk_dp_hwirq_get_clear(struct mtk_dp *mtk_dp)
{
	u8 irq_status = (mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_3418) &
			 IRQ_STATUS_DP_TRANS_P0_MASK) >>
			IRQ_STATUS_DP_TRANS_P0_SHIFT;

	if (irq_status) {
		mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3418, irq_status,
				   IRQ_CLR_DP_TRANS_P0_MASK);
		mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3418, 0,
				   IRQ_CLR_DP_TRANS_P0_MASK);
	}

	return irq_status;
}

static void mtk_dp_hwirq_enable(struct mtk_dp *mtk_dp, bool enable)
{
	u32 val = 0;

	if (!enable)
		val = IRQ_MASK_DP_TRANS_P0_DISC_IRQ |
		      IRQ_MASK_DP_TRANS_P0_CONN_IRQ |
		      IRQ_MASK_DP_TRANS_P0_INT_IRQ;
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3418, val,
			   IRQ_MASK_DP_TRANS_P0_MASK);
}

static void mtk_dp_initialize_settings(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_342C,
			   XTAL_FREQ_DP_TRANS_P0_DEFAULT,
			   XTAL_FREQ_DP_TRANS_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3540,
			   BIT(FEC_CLOCK_EN_MODE_DP_TRANS_P0_SHIFT),
			   FEC_CLOCK_EN_MODE_DP_TRANS_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_31EC,
			   BIT(AUDIO_CH_SRC_SEL_DP_ENC0_P0_SHIFT),
			   AUDIO_CH_SRC_SEL_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_304C, 0,
			   SDP_VSYNC_RISING_MASK_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_IRQ_MASK, IRQ_MASK_AUX_TOP_IRQ,
			   IRQ_MASK_AUX_TOP_IRQ);
}

static void mtk_dp_initialize_hpd_detect_settings(struct mtk_dp *mtk_dp)
{
	// Debounce threshold
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3410,
			   8 << HPD_DEB_THD_DP_TRANS_P0_SHIFT,
			   HPD_DEB_THD_DP_TRANS_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3410,
			   (HPD_INT_THD_DP_TRANS_P0_LOWER_500US |
			    HPD_INT_THD_DP_TRANS_P0_UPPER_1100US)
				   << HPD_INT_THD_DP_TRANS_P0_SHIFT,
			   HPD_INT_THD_DP_TRANS_P0_MASK);

	// Connect threshold 1.5ms + 5 x 0.1ms = 2ms
	// Disconnect threshold 1.5ms + 5 x 0.1ms = 2ms
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3410,
			   (5 << HPD_DISC_THD_DP_TRANS_P0_SHIFT) |
				   (5 << HPD_CONN_THD_DP_TRANS_P0_SHIFT),
			   HPD_DISC_THD_DP_TRANS_P0_MASK |
				   HPD_CONN_THD_DP_TRANS_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3430,
			   HPD_INT_THD_ECO_DP_TRANS_P0_HIGH_BOUND_EXT,
			   HPD_INT_THD_ECO_DP_TRANS_P0_MASK);
}

static void mtk_dp_initialize_aux_settings(struct mtk_dp *mtk_dp)
{
	// modify timeout threshold = 1595 [12 : 8]
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_360C, 0x1595,
			   AUX_TIMEOUT_THR_AUX_TX_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3658, 0,
			   AUX_TX_OV_EN_AUX_TX_P0_MASK);
	// 25 for 26M
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3634,
			   25 << AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_SHIFT,
			   AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_MASK);
	// 13 for 26M
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3614,
			   13 << AUX_RX_UI_CNT_THR_AUX_TX_P0_SHIFT,
			   AUX_RX_UI_CNT_THR_AUX_TX_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_37C8,
			   BIT(MTK_ATOP_EN_AUX_TX_P0_SHIFT),
			   MTK_ATOP_EN_AUX_TX_P0_MASK);
}

static void mtk_dp_initialize_digital_settings(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_304C, 0,
			   VBID_VIDEO_MUTE_DP_ENC0_P0_MASK);
	mtk_dp_set_color_format(mtk_dp, MTK_DP_COLOR_FORMAT_RGB_444);
	mtk_dp_set_color_depth(mtk_dp, MTK_DP_COLOR_DEPTH_8BIT);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3368,
			   1 << BS2BS_MODE_DP_ENC1_P0_SHIFT,
			   BS2BS_MODE_DP_ENC1_P0_MASK);

	// dp tx encoder reset all sw
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3004,
			   BIT(DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_SHIFT),
			   DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_MASK);
	usleep_range(1000, 5000);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3004, 0,
			   DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_MASK);
}

static void mtk_dp_digital_sw_reset(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_340C,
			   BIT(DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_SHIFT),
			   DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_MASK);
	usleep_range(1000, 5000);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_340C, 0,
			   DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_MASK);
}

static void mtk_dp_set_lanes(struct mtk_dp *mtk_dp, int lanes)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_35F0,
			   lanes == 0 ? 0 : BIT(3), BIT(3) | BIT(2));
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3000, lanes,
			   LANE_NUM_DP_ENC0_P0_MASK);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_34A4,
			   lanes << LANE_NUM_DP_TRANS_P0_SHIFT,
			   LANE_NUM_DP_TRANS_P0_MASK);
}

static int link_rate_to_mb_per_s(struct mtk_dp *mtk_dp,
				 enum mtk_dp_linkrate linkrate)
{
	switch (linkrate) {
	default:
		drm_err(mtk_dp->drm_dev,
			"Implementation error, unknown linkrate %d\n",
			linkrate);
		fallthrough;
	case MTK_DP_LINKRATE_RBR:
		return 1620;
	case MTK_DP_LINKRATE_HBR:
		return 2700;
	case MTK_DP_LINKRATE_HBR2:
		return 5400;
	case MTK_DP_LINKRATE_HBR3:
		return 8100;
	}
}

static u32 check_cal_data_valid(u32 min, u32 max, u32 val, u32 default_val)
{
	if (val < min || val > max)
		return default_val;

	return val;
}

static int mtk_dp_get_calibration_data(struct mtk_dp *mtk_dp)
{
	struct dp_cal_data *cal_data = &mtk_dp->cal_data;
	struct device *dev = mtk_dp->dev;
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len;

	cell = nvmem_cell_get(dev, "dp_calibration_data");
	if (IS_ERR(cell)) {
		dev_err(dev,
			"Error: Failed to get nvmem cell dp_calibration_data\n");
		return PTR_ERR(cell);
	}

	buf = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf) || ((len / sizeof(u32)) != 4)) {
		dev_err(dev,
			"Error: Failed to read nvmem_cell_read fail len %ld\n",
			(len / sizeof(u32)));
		return PTR_ERR(buf);
	}

	if (mtk_dp_is_edp(mtk_dp)) {
		cal_data->glb_bias_trim = check_cal_data_valid(
			1, 0x1e, (buf[3] >> 27) & 0x1f, 0xf);
		cal_data->clktx_impse =
			check_cal_data_valid(1, 0xe, (buf[0] >> 9) & 0xf, 0x8);
		cal_data->ln0_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 28) & 0xf, 0x8);
		cal_data->ln0_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 24) & 0xf, 0x8);
		cal_data->ln1_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 20) & 0xf, 0x8);
		cal_data->ln1_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 16) & 0xf, 0x8);
		cal_data->ln2_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 12) & 0xf, 0x8);
		cal_data->ln2_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 8) & 0xf, 0x8);
		cal_data->ln3_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[2] >> 4) & 0xf, 0x8);
		cal_data->ln3_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, buf[2] & 0xf, 0x8);
	} else {
		cal_data->glb_bias_trim = check_cal_data_valid(
			1, 0x1e, (buf[0] >> 27) & 0x1f, 0xf);
		cal_data->clktx_impse =
			check_cal_data_valid(1, 0xe, (buf[0] >> 13) & 0xf, 0x8);
		cal_data->ln0_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 28) & 0xf, 0x8);
		cal_data->ln0_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 24) & 0xf, 0x8);
		cal_data->ln1_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 20) & 0xf, 0x8);
		cal_data->ln1_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 16) & 0xf, 0x8);
		cal_data->ln2_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 12) & 0xf, 0x8);
		cal_data->ln2_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 8) & 0xf, 0x8);
		cal_data->ln3_tx_impsel_pmos =
			check_cal_data_valid(1, 0xe, (buf[1] >> 4) & 0xf, 0x8);
		cal_data->ln3_tx_impsel_nmos =
			check_cal_data_valid(1, 0xe, buf[1] & 0xf, 0x8);
	}

	kfree(buf);

	return 0;
}

static void mtk_dp_set_cal_data(struct mtk_dp *mtk_dp)
{
	struct dp_cal_data *cal_data = &mtk_dp->cal_data;

	mtk_dp_update_bits(mtk_dp, DP_PHY_GLB_DPAUX_TX,
			   cal_data->clktx_impse << 20, RG_CKM_PT0_CKTX_IMPSEL);
	mtk_dp_update_bits(mtk_dp, DP_PHY_GLB_BIAS_GEN_00,
			   cal_data->glb_bias_trim << 16,
			   RG_XTP_GLB_BIAS_INTR_CTRL);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_0,
			   cal_data->ln0_tx_impsel_pmos << 12,
			   RG_XTP_LN0_TX_IMPSEL_PMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_0,
			   cal_data->ln0_tx_impsel_nmos << 16,
			   RG_XTP_LN0_TX_IMPSEL_NMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_1,
			   cal_data->ln1_tx_impsel_pmos << 12,
			   RG_XTP_LN1_TX_IMPSEL_PMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_1,
			   cal_data->ln1_tx_impsel_nmos << 16,
			   RG_XTP_LN1_TX_IMPSEL_NMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_2,
			   cal_data->ln2_tx_impsel_pmos << 12,
			   RG_XTP_LN2_TX_IMPSEL_PMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_2,
			   cal_data->ln2_tx_impsel_nmos << 16,
			   RG_XTP_LN2_TX_IMPSEL_NMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_3,
			   cal_data->ln3_tx_impsel_pmos << 12,
			   RG_XTP_LN3_TX_IMPSEL_PMOS);
	mtk_dp_update_bits(mtk_dp, DP_PHY_LANE_TX_3,
			   cal_data->ln3_tx_impsel_nmos << 16,
			   RG_XTP_LN3_TX_IMPSEL_NMOS);
}

static int mtk_dp_phy_configure(struct mtk_dp *mtk_dp,
				enum mtk_dp_linkrate link_rate, int lane_count)
{
	int ret;
	union phy_configure_opts
		phy_opts = { .dp = {
				     .link_rate = link_rate_to_mb_per_s(
					     mtk_dp, link_rate),
				     .set_rate = 1,
				     .lanes = lane_count,
				     .set_lanes = 1,
				     .ssc = mtk_dp->train_info.sink_ssc,
			     } };

	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP,
			   DP_PWR_STATE_MASK);

	ret = phy_configure(mtk_dp->phy, &phy_opts);

	mtk_dp_set_cal_data(mtk_dp);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL_LANE, DP_PWR_STATE_MASK);
	return ret;
}

static void mtk_dp_set_idle_pattern(struct mtk_dp *mtk_dp, bool enable)
{
	const u32 val = POST_MISC_DATA_LANE0_OV_DP_TRANS_P0_MASK |
			POST_MISC_DATA_LANE1_OV_DP_TRANS_P0_MASK |
			POST_MISC_DATA_LANE2_OV_DP_TRANS_P0_MASK |
			POST_MISC_DATA_LANE3_OV_DP_TRANS_P0_MASK;
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3580, enable ? val : 0, val);
}

static void mtk_dp_train_set_pattern(struct mtk_dp *mtk_dp, int pattern)
{
	if (pattern < 0 || pattern > 4) {
		drm_err(mtk_dp->drm_dev,
			"Implementation error, no such pattern %d\n", pattern);
		return;
	}

	if (pattern == 1) // TPS1
		mtk_dp_set_idle_pattern(mtk_dp, false);

	mtk_dp_update_bits(
		mtk_dp, MTK_DP_TRANS_P0_3400,
		pattern ? BIT(pattern - 1) << PATTERN1_EN_DP_TRANS_P0_SHIFT : 0,
		PATTERN1_EN_DP_TRANS_P0_MASK | PATTERN2_EN_DP_TRANS_P0_MASK |
			PATTERN3_EN_DP_TRANS_P0_MASK |
			PATTERN4_EN_DP_TRANS_P0_MASK);
}

static void mtk_dp_set_enhanced_frame_mode(struct mtk_dp *mtk_dp, bool enable)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3000,
			   enable ? BIT(ENHANCED_FRAME_EN_DP_ENC0_P0_SHIFT) : 0,
			   ENHANCED_FRAME_EN_DP_ENC0_P0_MASK);
}

static void mtk_dp_training_set_scramble(struct mtk_dp *mtk_dp, bool enable)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TRANS_P0_3404,
			   enable ? DP_SCR_EN_DP_TRANS_P0_MASK : 0,
			   DP_SCR_EN_DP_TRANS_P0_MASK);
}

static void mtk_dp_video_mute(struct mtk_dp *mtk_dp, bool enable)
{
	u32 val = BIT(VIDEO_MUTE_SEL_DP_ENC0_P0_SHIFT);

	if (enable)
		val |= BIT(VIDEO_MUTE_SW_DP_ENC0_P0_SHIFT);
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3000, val,
			   VIDEO_MUTE_SEL_DP_ENC0_P0_MASK |
				   VIDEO_MUTE_SW_DP_ENC0_P0_MASK);

	if (mtk_dp_is_edp(mtk_dp))
		mtk_dp_sip_atf_call(MTK_DP_SIP_ATF_EDP_VIDEO_UNMUTE, enable);
	else
		mtk_dp_sip_atf_call(MTK_DP_SIP_ATF_VIDEO_UNMUTE, enable);
}

static void mtk_dp_audio_mute(struct mtk_dp *mtk_dp, bool mute)
{
	if (mute) {
		mtk_dp_update_bits(
			mtk_dp, MTK_DP_ENC0_P0_3030,
			BIT(VBID_AUDIO_MUTE_SW_DP_ENC0_P0_SHIFT) |
				BIT(VBID_AUDIO_MUTE_SEL_DP_ENC0_P0_SHIFT),
			VBID_AUDIO_MUTE_FLAG_SW_DP_ENC0_P0_MASK |
				VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0_MASK);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3088, 0,
				   AU_EN_DP_ENC0_P0_MASK);
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30A4, 0,
				   AU_TS_CFG_DP_ENC0_P0_MASK);

	} else {
		mtk_dp_update_bits(
			mtk_dp, MTK_DP_ENC0_P0_3030, 0,
			VBID_AUDIO_MUTE_FLAG_SW_DP_ENC0_P0_MASK |
				VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0_MASK);

		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3088,
				   BIT(AU_EN_DP_ENC0_P0_SHIFT),
				   AU_EN_DP_ENC0_P0_MASK);
		// Send one every two frames
		mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_30A4, 0x0F,
				   AU_TS_CFG_DP_ENC0_P0_MASK);
	}
}

static void mtk_dp_power_enable(struct mtk_dp *mtk_dp)
{
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_RESET_AND_PROBE, 0,
			   SW_RST_B_PHYD);
	usleep_range(10, 200);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_RESET_AND_PROBE, SW_RST_B_PHYD,
			   SW_RST_B_PHYD);
	mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL,
			   DP_PWR_STATE_MASK);
}

static void mtk_dp_power_disable(struct mtk_dp *mtk_dp)
{
	mtk_dp_write(mtk_dp, MTK_DP_TOP_PWR_STATE, 0);
	usleep_range(10, 200);
	mtk_dp_write(mtk_dp, MTK_DP_0034,
		     DA_CKM_CKTX0_EN_FORCE_EN | DA_CKM_BIAS_LPF_EN_FORCE_VAL |
			     DA_CKM_BIAS_EN_FORCE_VAL |
			     DA_XTP_GLB_LDO_EN_FORCE_VAL |
			     DA_XTP_GLB_AVD10_ON_FORCE_VAL);
	// Disable RX
	mtk_dp_write(mtk_dp, MTK_DP_1040, 0);
	mtk_dp_write(mtk_dp, MTK_DP_TOP_MEM_PD,
		     0x550 | BIT(FUSE_SEL_SHIFT) | BIT(MEM_ISO_EN_SHIFT));
}

static void mtk_dp_initialize_priv_data(struct mtk_dp *mtk_dp)
{
	mtk_dp->train_info.link_rate = MTK_DP_LINKRATE_HBR2;
	mtk_dp->train_info.lane_count = MTK_DP_MAX_LANES;
	mtk_dp->train_info.irq_status = 0;
	mtk_dp->train_info.cable_plugged_in = false;
	mtk_dp->train_info.cable_state_change = false;
	mtk_dp->train_state = MTK_DP_TRAIN_STATE_STARTUP;
	mtk_dp->state = MTK_DP_STATE_INITIAL;

	mtk_dp->info.format = MTK_DP_COLOR_FORMAT_RGB_444;
	mtk_dp->info.depth = MTK_DP_COLOR_DEPTH_8BIT;
	memset(&mtk_dp->info.timings, 0, sizeof(struct mtk_dp_timings));
	mtk_dp->info.timings.frame_rate = 60;

	mtk_dp->has_fec = false;
	mtk_dp->audio_enable = false;
}

static void mtk_dp_sdp_set_down_cnt_init(struct mtk_dp *mtk_dp,
					 u32 sram_read_start)
{
	u32 sdp_down_cnt_init = 0;
	u32 dc_offset;

	if (mtk_dp->info.timings.pix_rate_khz > 0)
		sdp_down_cnt_init = sram_read_start *
				    mtk_dp->train_info.link_rate * 2700 * 8 /
				    (mtk_dp->info.timings.pix_rate_khz * 4);

	switch (mtk_dp->train_info.lane_count) {
	case 1:
		sdp_down_cnt_init = sdp_down_cnt_init > 0x1A ?
						  sdp_down_cnt_init :
						  0x1A; //26
		break;
	case 2:
		// case for LowResolution && High Audio Sample Rate
		dc_offset = mtk_dp->info.timings.vtotal <= 525 ? 0x04 : 0x00;
		sdp_down_cnt_init = sdp_down_cnt_init > 0x10 ?
						  sdp_down_cnt_init :
						  0x10 + dc_offset; //20 or 16
		break;
	case 4:
	default:
		sdp_down_cnt_init =
			sdp_down_cnt_init > 0x06 ? sdp_down_cnt_init : 0x06; //6
		break;
	}

	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC0_P0_3040,
			   sdp_down_cnt_init
				   << SDP_DOWN_CNT_INIT_DP_ENC0_P0_SHIFT,
			   SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
}

static void mtk_dp_sdp_set_down_cnt_init_in_hblank(struct mtk_dp *mtk_dp)
{
	int pix_clk_mhz;
	u32 dc_offset;
	u32 spd_down_cnt_init = 0;

	pix_clk_mhz = mtk_dp->info.format == MTK_DP_COLOR_FORMAT_YUV_420 ?
				    mtk_dp->info.timings.pix_rate_khz / 2000 :
				    mtk_dp->info.timings.pix_rate_khz / 1000;

	switch (mtk_dp->train_info.lane_count) {
	case 1:
		spd_down_cnt_init = 0x20;
		break;
	case 2:
		dc_offset = (mtk_dp->info.timings.vtotal <= 525) ? 0x14 : 0x00;
		spd_down_cnt_init = 0x18 + dc_offset;
		break;
	case 4:
	default:
		dc_offset = (mtk_dp->info.timings.vtotal <= 525) ? 0x08 : 0x00;
		if (pix_clk_mhz > mtk_dp->train_info.link_rate * 27)
			spd_down_cnt_init = 0x8;
		else
			spd_down_cnt_init = 0x10 + dc_offset;
		break;
	}
	mtk_dp_update_bits(mtk_dp, MTK_DP_ENC1_P0_3364, spd_down_cnt_init,
			   SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
}

static void mtk_dp_setup_tu(struct mtk_dp *mtk_dp)
{
	u32 sram_read_start = MTK_DP_TBC_BUF_READ_START_ADDR;

	if (mtk_dp->train_info.lane_count > 0) {
		sram_read_start = min_t(
			u32, MTK_DP_TBC_BUF_READ_START_ADDR,
			mtk_dp->info.timings.vm.hactive /
				(mtk_dp->train_info.lane_count * 4 * 2 * 2));
		mtk_dp_set_sram_read_start(mtk_dp, sram_read_start);
	}

	mtk_dp_setup_encoder(mtk_dp);
	mtk_dp_sdp_set_down_cnt_init_in_hblank(mtk_dp);
	mtk_dp_sdp_set_down_cnt_init(mtk_dp, sram_read_start);
}

static void mtk_dp_calculate_pixrate(struct mtk_dp *mtk_dp)
{
	int target_frame_rate = 60;
	int target_pixel_clk;

	if (mtk_dp->info.timings.frame_rate > 0) {
		target_frame_rate = mtk_dp->info.timings.frame_rate;
		target_pixel_clk = (int)mtk_dp->info.timings.htotal *
				   (int)mtk_dp->info.timings.vtotal *
				   target_frame_rate;
	} else if (mtk_dp->info.timings.pix_rate_khz > 0) {
		target_pixel_clk = mtk_dp->info.timings.pix_rate_khz * 1000;
	} else {
		target_pixel_clk = (int)mtk_dp->info.timings.htotal *
				   (int)mtk_dp->info.timings.vtotal *
				   target_frame_rate;
	}

	if (target_pixel_clk > 0)
		mtk_dp->info.timings.pix_rate_khz = target_pixel_clk / 1000;
}

static void mtk_dp_set_tx_out(struct mtk_dp *mtk_dp)
{
	mtk_dp_msa_bypass_disable(mtk_dp);
	mtk_dp_calculate_pixrate(mtk_dp);
	mtk_dp_pg_disable(mtk_dp);
	mtk_dp_setup_tu(mtk_dp);
}

static void mtk_dp_edid_free(struct mtk_dp *mtk_dp)
{
	mutex_lock(&mtk_dp->edid_lock);
	kfree(mtk_dp->edid);
	mtk_dp->edid = NULL;
	mutex_unlock(&mtk_dp->edid_lock);
}

static void mtk_dp_hpd_sink_event(struct mtk_dp *mtk_dp)
{
	ssize_t ret;
	u8 sink_count;
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	u32 sink_count_reg;
	u32 link_status_reg;
	bool locked;

	sink_count_reg = DP_SINK_COUNT_ESI;
	link_status_reg = DP_LANE0_1_STATUS;

	ret = drm_dp_dpcd_readb(&mtk_dp->aux, sink_count_reg, &sink_count);
	if (ret < 0) {
		drm_info(mtk_dp->drm_dev, "Read sink count failed: %ld\n", ret);
		return;
	}

	ret = drm_dp_dpcd_readb(&mtk_dp->aux, DP_SINK_COUNT, &sink_count);
	if (ret < 0) {
		drm_info(mtk_dp->drm_dev,
			 "Read DP_SINK_COUNT_ESI failed: %ld\n", ret);
		return;
	}

	ret = drm_dp_dpcd_read(&mtk_dp->aux, link_status_reg, link_status,
			       sizeof(link_status));
	if (!ret) {
		drm_info(mtk_dp->drm_dev, "Read link status failed: %ld\n",
			 ret);
		return;
	}

	locked = drm_dp_channel_eq_ok(link_status,
				      mtk_dp->train_info.lane_count);
	dev_dbg(mtk_dp->dev, "Read link status locked: 0x%x\n", locked);
	if (!locked && mtk_dp->train_state > MTK_DP_TRAIN_STATE_TRAINING_PRE)
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_TRAINING_PRE;

	if (link_status[1] & DP_REMOTE_CONTROL_COMMAND_PENDING)
		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
				   DP_REMOTE_CONTROL_COMMAND_PENDING);

	if (DP_GET_SINK_COUNT(sink_count) &&
	    (link_status[2] & DP_DOWNSTREAM_PORT_STATUS_CHANGED)) {
		mtk_dp_edid_free(mtk_dp);
		mtk_dp->train_info.check_cap_count = 0;
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_CHECKEDID;
		msleep(20);
	}
}

static void mtk_dp_sdp_stop_sending(struct mtk_dp *mtk_dp)
{
	u8 packet_type;

	for (packet_type = MTK_DP_SDP_ACM; packet_type < MTK_DP_SDP_MAX_NUM;
	     packet_type++)
		mtk_dp_disable_sdp(mtk_dp, packet_type);

	mtk_dp_sdp_vsc_ext_disable(mtk_dp);
}

static void mtk_dp_train_update_swing_pre(struct mtk_dp *mtk_dp, int lanes,
					  u8 dpcd_adjust_req[2])
{
	int lane;

	for (lane = 0; lane < lanes; ++lane) {
		u8 val;
		u8 swing;
		u8 preemphasis;
		int index = lane / 2;
		int shift = lane % 2 ? DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT : 0;

		swing = (dpcd_adjust_req[index] >> shift) &
			DP_ADJUST_VOLTAGE_SWING_LANE0_MASK;
		preemphasis = ((dpcd_adjust_req[index] >> shift) &
			       DP_ADJUST_PRE_EMPHASIS_LANE0_MASK) >>
			      DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT;
		val = swing << DP_TRAIN_VOLTAGE_SWING_SHIFT |
		      preemphasis << DP_TRAIN_PRE_EMPHASIS_SHIFT;

		if (swing == DP_TRAIN_VOLTAGE_SWING_LEVEL_3)
			val |= DP_TRAIN_MAX_SWING_REACHED;
		if (preemphasis == 3)
			val |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		mtk_dp_set_swing_pre_emphasis(mtk_dp, lane, swing, preemphasis);
		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_TRAINING_LANE0_SET + lane,
				   val);
	}

	// Wait for the signal to be stable enough
	usleep_range(2000, 5000);
}

static void mtk_dp_read_link_status(struct mtk_dp *mtk_dp,
				    u8 link_status[DP_LINK_STATUS_SIZE])
{
	drm_dp_dpcd_read(&mtk_dp->aux, DP_LANE0_1_STATUS, link_status,
			 DP_LINK_STATUS_SIZE);
}

static int mtk_dp_train_flow(struct mtk_dp *mtk_dp, int target_link_rate,
			     int target_lane_count)
{
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	u8 lane_adjust[2] = {};
	bool pass_tps1 = false;
	bool pass_tps2_3 = false;
	int train_retries;
	int status_control;
	int iteration_count;
	u8 prev_lane_adjust;
	u8 val;

	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_LINK_BW_SET, target_link_rate);
	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_LANE_COUNT_SET,
			   target_lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN);

	if (mtk_dp->train_info.sink_ssc)
		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_DOWNSPREAD_CTRL,
				   DP_SPREAD_AMP_0_5);

	train_retries = 0;
	status_control = 0;
	iteration_count = 1;
	prev_lane_adjust = 0xFF;

	mtk_dp_set_lanes(mtk_dp, target_lane_count / 2);
	mtk_dp_phy_configure(mtk_dp, target_link_rate, target_lane_count);

	dev_dbg(mtk_dp->dev,
		"Link train target_link_rate = 0x%x, target_lane_count = 0x%x\n",
		target_link_rate, target_lane_count);

	do {
		train_retries++;
		if (!mtk_dp->train_info.cable_plugged_in ||
		    ((mtk_dp->train_info.irq_status & MTK_DP_HPD_DISCONNECT) !=
		     0x0)) {
			return -ENODEV;
		}

		if (mtk_dp->train_state < MTK_DP_TRAIN_STATE_TRAINING)
			return -EAGAIN;

		if (!pass_tps1) {
			mtk_dp_training_set_scramble(mtk_dp, false);

			if (status_control == 0) {
				status_control = 1;
				mtk_dp_train_set_pattern(mtk_dp, 1);
				val = DP_LINK_SCRAMBLING_DISABLE |
				      DP_TRAINING_PATTERN_1;
				drm_dp_dpcd_writeb(
					&mtk_dp->aux, DP_TRAINING_PATTERN_SET,
					DP_LINK_SCRAMBLING_DISABLE |
						DP_TRAINING_PATTERN_1);
				drm_dp_dpcd_read(&mtk_dp->aux,
						 DP_ADJUST_REQUEST_LANE0_1,
						 lane_adjust,
						 sizeof(lane_adjust));
				iteration_count++;

				mtk_dp_train_update_swing_pre(
					mtk_dp, target_lane_count, lane_adjust);
			}

			drm_dp_link_train_clock_recovery_delay(&mtk_dp->aux,
							       mtk_dp->rx_cap);
			mtk_dp_read_link_status(mtk_dp, link_status);

			if (drm_dp_clock_recovery_ok(link_status,
						     target_lane_count)) {
				mtk_dp->train_info.cr_done = true;
				pass_tps1 = true;
				train_retries = 0;
				iteration_count = 1;
				dev_dbg(mtk_dp->dev, "Link train CR pass\n");
			} else if (prev_lane_adjust == link_status[4]) {
				iteration_count++;
				dev_dbg(mtk_dp->dev, "Link train CQ fail\n");
				if (prev_lane_adjust &
				    DP_ADJUST_VOLTAGE_SWING_LANE0_MASK)
					break;
			} else {
				dev_dbg(mtk_dp->dev, "Link train CQ fail\n");
				prev_lane_adjust = link_status[4];
			}
		} else if (pass_tps1 && !pass_tps2_3) {
			if (status_control == 1) {
				status_control = 2;
				if (mtk_dp->train_info.tps4) {
					mtk_dp_train_set_pattern(mtk_dp, 4);
					val = DP_TRAINING_PATTERN_4;
				} else if (mtk_dp->train_info.tps3) {
					mtk_dp_train_set_pattern(mtk_dp, 3);
					val = DP_LINK_SCRAMBLING_DISABLE |
					      DP_TRAINING_PATTERN_3;
				} else {
					mtk_dp_train_set_pattern(mtk_dp, 2);
					val = DP_LINK_SCRAMBLING_DISABLE |
					      DP_TRAINING_PATTERN_2;
				}
				drm_dp_dpcd_writeb(&mtk_dp->aux,
						   DP_TRAINING_PATTERN_SET,
						   val);

				drm_dp_dpcd_read(&mtk_dp->aux,
						 DP_ADJUST_REQUEST_LANE0_1,
						 lane_adjust,
						 sizeof(lane_adjust));

				iteration_count++;
				mtk_dp_train_update_swing_pre(
					mtk_dp, target_lane_count, lane_adjust);
			}

			drm_dp_link_train_channel_eq_delay(&mtk_dp->aux,
							   mtk_dp->rx_cap);

			mtk_dp_read_link_status(mtk_dp, link_status);

			if (!drm_dp_clock_recovery_ok(link_status,
						      target_lane_count)) {
				mtk_dp->train_info.cr_done = false;
				mtk_dp->train_info.eq_done = false;
				dev_dbg(mtk_dp->dev, "Link train EQ fail\n");
				break;
			}

			if (drm_dp_channel_eq_ok(link_status,
						 target_lane_count)) {
				mtk_dp->train_info.eq_done = true;
				pass_tps2_3 = true;
				dev_dbg(mtk_dp->dev, "Link train EQ pass\n");
				break;
			}

			if (prev_lane_adjust == link_status[4])
				iteration_count++;
			else
				prev_lane_adjust = link_status[4];
		}

		drm_dp_dpcd_read(&mtk_dp->aux, DP_ADJUST_REQUEST_LANE0_1,
				 lane_adjust, sizeof(lane_adjust));
		mtk_dp_train_update_swing_pre(mtk_dp, target_lane_count,
					      lane_adjust);
	} while (train_retries < MTK_DP_TRAIN_RETRY_LIMIT &&
		 iteration_count < MTK_DP_TRAIN_MAX_ITERATIONS);

	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
	mtk_dp_train_set_pattern(mtk_dp, 0);

	if (pass_tps2_3) {
		mtk_dp->train_info.link_rate = target_link_rate;
		mtk_dp->train_info.lane_count = target_lane_count;

		mtk_dp_training_set_scramble(mtk_dp, true);

		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_LANE_COUNT_SET,
				   target_lane_count |
					   DP_LANE_COUNT_ENHANCED_FRAME_EN);
		mtk_dp_set_enhanced_frame_mode(mtk_dp, true);

		return 0;
	}

	return -ETIMEDOUT;
}

static bool mtk_dp_parse_capabilities(struct mtk_dp *mtk_dp)
{
	u8 buf[DP_RECEIVER_CAP_SIZE] = {};
	u8 val;
	struct mtk_dp_train_info *train_info = &mtk_dp->train_info;

	if (!mtk_dp_plug_state(mtk_dp))
		return false;

	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_SET_POWER, DP_SET_POWER_D0);
	usleep_range(2000, 5000);

	drm_dp_read_dpcd_caps(&mtk_dp->aux, buf);

	memcpy(mtk_dp->rx_cap, buf, min(sizeof(mtk_dp->rx_cap), sizeof(buf)));
	mtk_dp->rx_cap[DP_TRAINING_AUX_RD_INTERVAL] &= DP_TRAINING_AUX_RD_MASK;

	train_info->link_rate =
		min_t(int, MTK_DP_MAX_LINK_RATE, buf[DP_MAX_LINK_RATE]);
	train_info->lane_count =
		min_t(int, MTK_DP_MAX_LANES, drm_dp_max_lane_count(buf));

	train_info->tps3 = drm_dp_tps3_supported(buf);
	train_info->tps4 = drm_dp_tps4_supported(buf);

	train_info->sink_ssc =
		!!(buf[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5);

	train_info->sink_ssc = false;

	drm_dp_dpcd_readb(&mtk_dp->aux, DP_MSTM_CAP, &val);
	if (val & DP_MST_CAP) {
		// Clear DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0
		drm_dp_dpcd_readb(&mtk_dp->aux,
				  DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0, &val);
		if (val)
			drm_dp_dpcd_writeb(&mtk_dp->aux,
					   DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
					   val);
	}

	return true;
}

static int mtk_dp_edid_parse_audio_capabilities(struct mtk_dp *mtk_dp,
						struct mtk_dp_audio_cfg *cfg)
{
	struct cea_sad *sads;
	int sad_count;
	int i;
	int ret = 0;

	mutex_lock(&mtk_dp->edid_lock);
	if (!mtk_dp->edid) {
		mutex_unlock(&mtk_dp->edid_lock);
		dev_err(mtk_dp->dev, "EDID not found!\n");
		return -EINVAL;
	}

	sad_count = drm_edid_to_sad(mtk_dp->edid, &sads);
	mutex_unlock(&mtk_dp->edid_lock);
	if (sad_count <= 0) {
		drm_info(mtk_dp->drm_dev, "The SADs is NULL\n");
		return 0;
	}

	for (i = 0; i < sad_count; i++) {
		int sample_rate;
		int word_length;
		// Only PCM supported at the moment
		if (sads[i].format != HDMI_AUDIO_CODING_TYPE_PCM)
			continue;

		sample_rate = drm_cea_sad_get_sample_rate(&sads[i]);
		word_length =
			drm_cea_sad_get_uncompressed_word_length(&sads[i]);
		if (sample_rate <= 0 || word_length <= 0)
			continue;

		cfg->channels = sads[i].channels;
		cfg->word_length_bits = word_length;
		cfg->sample_rate = sample_rate;
		ret = 1;
		break;
	}
	kfree(sads);

	return ret;
}

static void mtk_dp_train_change_mode(struct mtk_dp *mtk_dp)
{
	phy_reset(mtk_dp->phy);
	mtk_dp_reset_swing_pre_emphasis(mtk_dp);

	usleep_range(2000, 5000);
}

static int mtk_dp_train_start(struct mtk_dp *mtk_dp)
{
	int ret = 0;
	int lane_count;
	int link_rate;
	int train_limit;
	int max_link_rate;
	int plug_wait;

	for (plug_wait = 7; !mtk_dp_plug_state(mtk_dp) && plug_wait > 0;
	     --plug_wait)
		usleep_range(1000, 5000);
	if (plug_wait == 0) {
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_DPIDLE;
		return -ENODEV;
	}

	link_rate = mtk_dp->rx_cap[1];
	lane_count = mtk_dp->rx_cap[2] & 0x1F;

	mtk_dp->train_info.link_rate = min(MTK_DP_MAX_LINK_RATE, link_rate);
	mtk_dp->train_info.lane_count = min(MTK_DP_MAX_LANES, lane_count);
	link_rate = mtk_dp->train_info.link_rate;
	lane_count = mtk_dp->train_info.lane_count;

	switch (link_rate) {
	case MTK_DP_LINKRATE_RBR:
	case MTK_DP_LINKRATE_HBR:
	case MTK_DP_LINKRATE_HBR2:
	case MTK_DP_LINKRATE_HBR25:
	case MTK_DP_LINKRATE_HBR3:
		break;
	default:
		mtk_dp->train_info.link_rate = MTK_DP_LINKRATE_HBR3;
		break;
	};

	max_link_rate = link_rate;
	for (train_limit = 0; train_limit <= 6; ++train_limit) {
		mtk_dp->train_info.cr_done = false;
		mtk_dp->train_info.eq_done = false;

		mtk_dp_train_change_mode(mtk_dp);
		ret = mtk_dp_train_flow(mtk_dp, link_rate, lane_count);
		if (ret == -ENODEV || ret == -EAGAIN)
			return ret;

		if (!mtk_dp->train_info.cr_done) {
			switch (link_rate) {
			case MTK_DP_LINKRATE_RBR:
				lane_count = lane_count / 2;
				link_rate = max_link_rate;
				if (lane_count == 0x0) {
					mtk_dp->train_state =
						MTK_DP_TRAIN_STATE_DPIDLE;
					return -EIO;
				}
				break;
			case MTK_DP_LINKRATE_HBR:
				link_rate = MTK_DP_LINKRATE_RBR;
				break;
			case MTK_DP_LINKRATE_HBR2:
				link_rate = MTK_DP_LINKRATE_HBR;
				break;
			case MTK_DP_LINKRATE_HBR3:
				link_rate = MTK_DP_LINKRATE_HBR2;
				break;
			default:
				return -EINVAL;
			};
		} else if (!mtk_dp->train_info.eq_done) {
			lane_count /= 2;
			if (lane_count == 0)
				return -EIO;
		} else {
			return 0;
		}
	}

	return -ETIMEDOUT;
}

static int mtk_dp_train_handler(struct mtk_dp *mtk_dp)
{
	int ret = 0;

	if (mtk_dp->train_state == MTK_DP_TRAIN_STATE_NORMAL)
		return ret;

	switch (mtk_dp->train_state) {
	case MTK_DP_TRAIN_STATE_STARTUP:
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_CHECKCAP;
		break;

	case MTK_DP_TRAIN_STATE_CHECKCAP:
		if (mtk_dp_parse_capabilities(mtk_dp)) {
			mtk_dp->train_info.check_cap_count = 0;
			mtk_dp->train_state = MTK_DP_TRAIN_STATE_CHECKEDID;
		} else {
			mtk_dp->train_info.check_cap_count++;

			if (mtk_dp->train_info.check_cap_count >
			    MTK_DP_CHECK_SINK_CAP_TIMEOUT_COUNT) {
				mtk_dp->train_info.check_cap_count = 0;
				mtk_dp->train_state = MTK_DP_TRAIN_STATE_DPIDLE;
				ret = -ETIMEDOUT;
			}
		}
		break;

	case MTK_DP_TRAIN_STATE_CHECKEDID: {
		int caps_found = mtk_dp_edid_parse_audio_capabilities(
			mtk_dp, &mtk_dp->info.audio_caps);
		mtk_dp->audio_enable = caps_found > 0;
		if (!mtk_dp->audio_enable)
			memset(&mtk_dp->info.audio_caps, 0,
			       sizeof(mtk_dp->info.audio_caps));
	}

		mtk_dp->train_state = MTK_DP_TRAIN_STATE_TRAINING_PRE;
		break;

	case MTK_DP_TRAIN_STATE_TRAINING_PRE:
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_TRAINING;
		break;

	case MTK_DP_TRAIN_STATE_TRAINING:
		ret = mtk_dp_train_start(mtk_dp);
		if (!ret) {
			mtk_dp_video_mute(mtk_dp, true);
			mtk_dp_audio_mute(mtk_dp, true);
			mtk_dp->train_state = MTK_DP_TRAIN_STATE_CHECKTIMING;
			mtk_dp_fec_enable(mtk_dp, mtk_dp->has_fec);
		} else if (ret != -EAGAIN) {
			mtk_dp->train_state = MTK_DP_TRAIN_STATE_DPIDLE;
		}

		ret = 0;
		break;

	case MTK_DP_TRAIN_STATE_CHECKTIMING:
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_NORMAL;
		break;
	case MTK_DP_TRAIN_STATE_NORMAL:
		break;
	case MTK_DP_TRAIN_STATE_POWERSAVE:
		break;
	case MTK_DP_TRAIN_STATE_DPIDLE:
		break;
	default:
		break;
	}

	return ret;
}

static void mtk_dp_video_enable(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable) {
		mtk_dp_set_tx_out(mtk_dp);
		mtk_dp_video_mute(mtk_dp, false);
	} else {
		mtk_dp_video_mute(mtk_dp, true);
	}
}

static void mtk_dp_audio_sdp_setup(struct mtk_dp *mtk_dp,
				   struct mtk_dp_audio_cfg *cfg)
{
	struct mtk_dp_sdp_packet packet;
	struct hdmi_audio_infoframe frame;

	hdmi_audio_infoframe_init(&frame);
	frame.coding_type = HDMI_AUDIO_CODING_TYPE_PCM;
	frame.channels = cfg->channels;
	frame.sample_frequency = cfg->sample_rate;

	switch (cfg->word_length_bits) {
	case 16:
		frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_16;
		break;
	case 20:
		frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_20;
		break;
	case 24:
	default:
		frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_24;
		break;
	}

	packet.type = MTK_DP_SDP_AUI;
	hdmi_audio_infoframe_pack_for_dp(&frame, &packet.sdp,
					 MTK_DP_DP_VERSION_11);

	mtk_dp_audio_sdp_asp_set_channels(mtk_dp, cfg->channels);
	mtk_dp_setup_sdp(mtk_dp, &packet);
}

static void mtk_dp_audio_setup(struct mtk_dp *mtk_dp,
			       struct mtk_dp_audio_cfg *cfg)
{
	mtk_dp_audio_sdp_setup(mtk_dp, cfg);
	mtk_dp_audio_channel_status_set(mtk_dp, cfg);

	mtk_dp_audio_setup_channels(mtk_dp, cfg);
	mtk_dp_audio_set_divider(mtk_dp);
}

static void mtk_dp_video_config(struct mtk_dp *mtk_dp)
{
	mtk_dp_mn_overwrite_disable(mtk_dp);

	mtk_dp_set_msa(mtk_dp);

	mtk_dp_set_color_depth(mtk_dp, mtk_dp->info.depth);
	mtk_dp_set_color_format(mtk_dp, mtk_dp->info.format);
}

static int mtk_dp_state_handler(struct mtk_dp *mtk_dp)
{
	int ret = 0;

	switch (mtk_dp->state) {
	case MTK_DP_STATE_INITIAL:
		mtk_dp_video_mute(mtk_dp, true);
		mtk_dp_audio_mute(mtk_dp, true);
		mtk_dp->state = MTK_DP_STATE_IDLE;
		break;

	case MTK_DP_STATE_IDLE:
		if (mtk_dp->train_state == MTK_DP_TRAIN_STATE_NORMAL)
			mtk_dp->state = MTK_DP_STATE_PREPARE;
		break;

	case MTK_DP_STATE_PREPARE:
		mtk_dp_video_config(mtk_dp);
		mtk_dp_video_enable(mtk_dp, true);

		if (mtk_dp->audio_enable) {
			mtk_dp_audio_setup(mtk_dp, &mtk_dp->info.audio_caps);
			mtk_dp_audio_mute(mtk_dp, false);
		}

		mtk_dp->state = MTK_DP_STATE_NORMAL;
		break;

	case MTK_DP_STATE_NORMAL:
		if (mtk_dp->train_state != MTK_DP_TRAIN_STATE_NORMAL) {
			mtk_dp_video_mute(mtk_dp, true);
			mtk_dp_audio_mute(mtk_dp, true);
			mtk_dp_sdp_stop_sending(mtk_dp);
			mtk_dp->state = MTK_DP_STATE_IDLE;
		}
		break;

	default:
		break;
	}

	return ret;
}

static void mtk_dp_init_port(struct mtk_dp *mtk_dp)
{
	mtk_dp_set_idle_pattern(mtk_dp, true);
	mtk_dp_initialize_priv_data(mtk_dp);

	mtk_dp_initialize_settings(mtk_dp);
	mtk_dp_initialize_aux_settings(mtk_dp);
	mtk_dp_initialize_digital_settings(mtk_dp);
	mtk_dp_update_bits(mtk_dp, MTK_DP_AUX_P0_3690,
			   BIT(RX_REPLY_COMPLETE_MODE_AUX_TX_P0_SHIFT),
			   RX_REPLY_COMPLETE_MODE_AUX_TX_P0_MASK);
	mtk_dp_initialize_hpd_detect_settings(mtk_dp);

	mtk_dp_digital_sw_reset(mtk_dp);
}

static irqreturn_t mtk_dp_hpd_event_thread(int hpd, void *dev)
{
	struct mtk_dp *mtk_dp = dev;
	int event;
	u8 buf[DP_RECEIVER_CAP_SIZE] = {};

	event = mtk_dp_plug_state(mtk_dp) ? connector_status_connected :
						  connector_status_disconnected;

	if (event < 0)
		return IRQ_HANDLED;

	if (mtk_dp->drm_dev) {
		dev_info(mtk_dp->dev, "drm_helper_hpd_irq_event\n");
		drm_helper_hpd_irq_event(mtk_dp->bridge.dev);
	}

	if (mtk_dp->train_info.cable_state_change) {
		mtk_dp->train_info.cable_state_change = false;

		mtk_dp->train_state = MTK_DP_TRAIN_STATE_STARTUP;

		if (!mtk_dp->train_info.cable_plugged_in ||
		    !mtk_dp_plug_state(mtk_dp)) {
			mtk_dp_video_mute(mtk_dp, true);
			mtk_dp_audio_mute(mtk_dp, true);

			mtk_dp_initialize_priv_data(mtk_dp);
			mtk_dp_set_idle_pattern(mtk_dp, true);
			if (mtk_dp->has_fec)
				mtk_dp_fec_enable(mtk_dp, false);
			mtk_dp_sdp_stop_sending(mtk_dp);

			mtk_dp_edid_free(mtk_dp);
			mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
					   DP_PWR_STATE_BANDGAP_TPLL,
					   DP_PWR_STATE_MASK);
		} else {
			mtk_dp_update_bits(mtk_dp, MTK_DP_TOP_PWR_STATE,
					   DP_PWR_STATE_BANDGAP_TPLL_LANE,
					   DP_PWR_STATE_MASK);
			drm_dp_read_dpcd_caps(&mtk_dp->aux, buf);
			mtk_dp->train_info.link_rate =
				min_t(int, MTK_DP_MAX_LINK_RATE,
				      buf[DP_MAX_LINK_RATE]);
			mtk_dp->train_info.lane_count =
				min_t(int, MTK_DP_MAX_LANES,
				      drm_dp_max_lane_count(buf));
		}
	}

	if (mtk_dp->train_info.irq_status & MTK_DP_HPD_INTERRUPT) {
		dev_info(mtk_dp->dev, "MTK_DP_HPD_INTERRUPT\n");
		mtk_dp->train_info.irq_status &= ~MTK_DP_HPD_INTERRUPT;
		mtk_dp_hpd_sink_event(mtk_dp);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mtk_dp_hpd_isr_handler(struct mtk_dp *mtk_dp)
{
	bool connected;
	u16 swirq_status = mtk_dp_swirq_get_clear(mtk_dp);
	u8 hwirq_status = mtk_dp_hwirq_get_clear(mtk_dp);
	struct mtk_dp_train_info *train_info = &mtk_dp->train_info;

	train_info->irq_status |= hwirq_status | swirq_status;

	dev_dbg(mtk_dp->dev, "hpd isr HWIRQ status(0x%x) SWIRQ status(0x%x)\n",
		hwirq_status, swirq_status);
	if (!train_info->irq_status)
		return IRQ_HANDLED;

	connected = mtk_dp_plug_state(mtk_dp);
	if (connected || !train_info->cable_plugged_in)
		train_info->irq_status &= ~MTK_DP_HPD_DISCONNECT;
	else if (!connected || train_info->cable_plugged_in)
		train_info->irq_status &= ~MTK_DP_HPD_CONNECT;

	if (!(train_info->irq_status &
	      (MTK_DP_HPD_CONNECT | MTK_DP_HPD_DISCONNECT)))
		return IRQ_HANDLED;

	if (train_info->irq_status & MTK_DP_HPD_CONNECT) {
		train_info->irq_status &= ~MTK_DP_HPD_CONNECT;
		train_info->cable_plugged_in = true;
	} else {
		train_info->irq_status &= ~MTK_DP_HPD_DISCONNECT;
		train_info->cable_plugged_in = false;
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_STARTUP;
	}
	train_info->cable_state_change = true;
	return IRQ_WAKE_THREAD;
}

static irqreturn_t mtk_dp_hpd_event(int hpd, void *dev)
{
	struct mtk_dp *mtk_dp = dev;
	u32 irq_status;

	irq_status = mtk_dp_read(mtk_dp, MTK_DP_TOP_IRQ_STATUS);

	if (!irq_status)
		return IRQ_HANDLED;

	if (irq_status & RGS_IRQ_STATUS_TRANSMITTER)
		return mtk_dp_hpd_isr_handler(mtk_dp);

	return IRQ_HANDLED;
}

static int mtk_dp_dt_parse_pdata(struct mtk_dp *mtk_dp,
				 struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	void __iomem *base;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	mtk_dp->regs = devm_regmap_init_mmio(dev, base, &mtk_dp_regmap_config);
	if (IS_ERR(mtk_dp->regs))
		return PTR_ERR(mtk_dp->regs);

	mtk_dp->dp_tx_clk = devm_clk_get(dev, "faxi");
	if (IS_ERR(mtk_dp->dp_tx_clk)) {
		ret = PTR_ERR(mtk_dp->dp_tx_clk);
		dev_err(dev, "Failed to get dptx clock: %d\n", ret);
		mtk_dp->dp_tx_clk = NULL;
	}

	return 0;
}

static void mtk_dp_update_plugged_status(struct mtk_dp *mtk_dp)
{
	bool connected, has_audio;

	mutex_lock(&mtk_dp->update_plugged_status_lock);
	connected = mtk_dp_plug_state(mtk_dp);
	has_audio = drm_detect_monitor_audio(mtk_dp->edid);
	if (mtk_dp->plugged_cb && mtk_dp->codec_dev)
		mtk_dp->plugged_cb(mtk_dp->codec_dev, connected & has_audio);
	mutex_unlock(&mtk_dp->update_plugged_status_lock);
}

static enum drm_connector_status mtk_dp_bdg_detect(struct drm_bridge *bridge)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	enum drm_connector_status ret = connector_status_disconnected;
	u8 sink_count = 0;

	if (mtk_dp_is_edp(mtk_dp))
		return connector_status_connected;

	if (mtk_dp_plug_state(mtk_dp)) {
		drm_dp_dpcd_readb(&mtk_dp->aux, DP_SINK_COUNT, &sink_count);
		if (DP_GET_SINK_COUNT(sink_count))
			ret = connector_status_connected;
	}

	mtk_dp_update_plugged_status(mtk_dp);
	return ret;
}

static struct edid *mtk_dp_get_edid(struct drm_bridge *bridge,
				    struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	bool enabled = mtk_dp->enabled;
	struct edid *new_edid = NULL;

	if (!enabled)
		drm_bridge_chain_pre_enable(bridge);

	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_SET_POWER, DP_SET_POWER_D0);
	usleep_range(2000, 5000);

	if (mtk_dp_plug_state(mtk_dp))
		new_edid = drm_get_edid(connector, &mtk_dp->aux.ddc);

	if (!enabled)
		drm_bridge_chain_post_disable(bridge);

	mutex_lock(&mtk_dp->edid_lock);
	kfree(mtk_dp->edid);
	mtk_dp->edid = NULL;

	if (!new_edid) {
		mutex_unlock(&mtk_dp->edid_lock);
		return NULL;
	}

	mtk_dp->edid = drm_edid_duplicate(new_edid);
	mutex_unlock(&mtk_dp->edid_lock);

	return new_edid;
}

static ssize_t mtk_dp_aux_transfer(struct drm_dp_aux *mtk_aux,
				   struct drm_dp_aux_msg *msg)
{
	ssize_t err = -EAGAIN;
	struct mtk_dp *mtk_dp;
	bool is_read;
	u8 request;
	size_t accessed_bytes = 0;
	int retry = 3, ret = 0;

	mtk_dp = container_of(mtk_aux, struct mtk_dp, aux);

	if (!mtk_dp->train_info.cable_plugged_in ||
	    mtk_dp->train_info.irq_status & MTK_DP_HPD_DISCONNECT) {
		mtk_dp->train_state = MTK_DP_TRAIN_STATE_CHECKCAP;
		dev_err(mtk_dp->dev, "AUX err: DP noconnected\n");
		err = -EAGAIN;
		goto err;
	}

	switch (msg->request) {
	case DP_AUX_I2C_MOT:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE | DP_AUX_I2C_MOT:
		request = msg->request & ~DP_AUX_I2C_WRITE_STATUS_UPDATE;
		is_read = false;
		break;
	case DP_AUX_I2C_READ:
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ | DP_AUX_I2C_MOT:
		request = msg->request;
		is_read = true;
		break;
	default:
		drm_err(mtk_aux->drm_dev, "invalid aux cmd = %d\n",
			msg->request);
		err = -EINVAL;
		goto err;
	}

	if (msg->size == 0) {
		mtk_dp_aux_do_transfer(mtk_dp, is_read, request,
				       msg->address + accessed_bytes,
				       msg->buffer + accessed_bytes, 0);
	} else {
		while (accessed_bytes < msg->size) {
			size_t to_access =
				min_t(size_t, DP_AUX_MAX_PAYLOAD_BYTES,
				      msg->size - accessed_bytes);
			retry = 3;
			while (retry--) {
				ret = mtk_dp_aux_do_transfer(
					mtk_dp, is_read, request,
					msg->address + accessed_bytes,
					msg->buffer + accessed_bytes,
					to_access);
				if (ret == 0)
					break;
				udelay(50);
			}
			if (!retry && ret) {
				drm_info(mtk_dp->drm_dev,
					 "Failed to do AUX transfer: %d\n",
					 ret);
				break;
			}
			accessed_bytes += to_access;
		}
	}
err:
	if (!ret) {
		msg->reply = DP_AUX_NATIVE_REPLY_ACK | DP_AUX_I2C_REPLY_ACK;
		ret = msg->size;
	} else {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		return err;
	}

	msg->reply = DP_AUX_NATIVE_REPLY_ACK | DP_AUX_I2C_REPLY_ACK;
	return msg->size;
}

static void mtk_dp_aux_init(struct mtk_dp *mtk_dp)
{
	drm_dp_aux_init(&mtk_dp->aux);
	mtk_dp->aux.name = "aux_mtk_dp";
	mtk_dp->aux.transfer = mtk_dp_aux_transfer;
}

static void mtk_dp_poweroff(struct mtk_dp *mtk_dp)
{
	mutex_lock(&mtk_dp->dp_lock);

	mtk_dp_hwirq_enable(mtk_dp, false);
	mtk_dp_power_disable(mtk_dp);
	phy_exit(mtk_dp->phy);
	clk_disable_unprepare(mtk_dp->dp_tx_clk);

	mutex_unlock(&mtk_dp->dp_lock);
}

static int mtk_dp_poweron(struct mtk_dp *mtk_dp)
{
	int ret = 0;

	mutex_lock(&mtk_dp->dp_lock);

	ret = clk_prepare_enable(mtk_dp->dp_tx_clk);
	if (ret < 0) {
		dev_err(mtk_dp->dev, "Fail to enable clock: %d\n", ret);
		goto err;
	}
	ret = phy_init(mtk_dp->phy);
	if (ret) {
		dev_err(mtk_dp->dev, "Failed to initialize phy: %d\n", ret);
		goto err_phy_init;
	}
	ret = mtk_dp_phy_configure(mtk_dp, MTK_DP_LINKRATE_RBR, 1);
	if (ret) {
		dev_err(mtk_dp->dev, "Failed to configure phy: %d\n", ret);
		goto err_phy_config;
	}

	mtk_dp_init_port(mtk_dp);
	mtk_dp_power_enable(mtk_dp);
	mtk_dp_hwirq_enable(mtk_dp, true);

err_phy_config:
	phy_exit(mtk_dp->phy);
err_phy_init:
	clk_disable_unprepare(mtk_dp->dp_tx_clk);
err:
	mutex_unlock(&mtk_dp->dp_lock);
	return ret;
}

static int mtk_dp_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	int ret;

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		dev_err(mtk_dp->dev, "Driver does not provide a connector!");
		return -EINVAL;
	}

	ret = mtk_dp_poweron(mtk_dp);
	if (ret)
		return ret;

	if (mtk_dp->next_bridge) {
		ret = drm_bridge_attach(bridge->encoder, mtk_dp->next_bridge,
					&mtk_dp->bridge, flags);
		if (ret) {
			drm_warn(mtk_dp->drm_dev,
				 "Failed to attach external bridge: %d\n", ret);
			goto err_bridge_attach;
		}
	}

	mtk_dp->drm_dev = bridge->dev;

	return 0;

err_bridge_attach:
	mtk_dp_poweroff(mtk_dp);
	return ret;
}

static void mtk_dp_bridge_detach(struct drm_bridge *bridge)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);

	mtk_dp->drm_dev = NULL;

	mtk_dp_poweroff(mtk_dp);
}

static void mtk_dp_bridge_atomic_disable(struct drm_bridge *bridge,
					 struct drm_bridge_state *old_state)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);

	mtk_dp_video_mute(mtk_dp, true);
	mtk_dp_audio_mute(mtk_dp, true);
	mtk_dp->state = MTK_DP_STATE_IDLE;
	mtk_dp->train_state = MTK_DP_TRAIN_STATE_STARTUP;

	mtk_dp->enabled = false;
	msleep(100);
}

static void mtk_dp_parse_drm_mode_timings(struct mtk_dp *mtk_dp,
					  struct drm_display_mode *mode)
{
	struct mtk_dp_timings *timings = &mtk_dp->info.timings;

	drm_display_mode_to_videomode(mode, &timings->vm);
	timings->frame_rate = mode->clock * 1000 / mode->htotal / mode->vtotal;
	timings->htotal = mode->htotal;
	timings->vtotal = mode->vtotal;
}

static void mtk_dp_bridge_atomic_enable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_state)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int ret = 0;
	int i;

	conn = drm_atomic_get_new_connector_for_encoder(old_state->base.state,
							bridge->encoder);
	if (!conn) {
		drm_err(mtk_dp->drm_dev,
			"Can't enable bridge as connector is missing\n");
		return;
	}

	memcpy(mtk_dp->connector_eld, conn->eld, MAX_ELD_BYTES);

	conn_state =
		drm_atomic_get_new_connector_state(old_state->base.state, conn);
	if (!conn_state) {
		drm_err(mtk_dp->drm_dev,
			"Can't enable bridge as connector state is missing\n");
		return;
	}

	crtc = conn_state->crtc;
	if (!crtc) {
		drm_err(mtk_dp->drm_dev,
			"Can't enable bridge as connector state doesn't have a crtc\n");
		return;
	}

	crtc_state = drm_atomic_get_new_crtc_state(old_state->base.state, crtc);
	if (!crtc_state) {
		drm_err(mtk_dp->drm_dev,
			"Can't enable bridge as crtc state is missing\n");
		return;
	}

	mtk_dp_parse_drm_mode_timings(mtk_dp, &crtc_state->adjusted_mode);
	if (!mtk_dp_parse_capabilities(mtk_dp)) {
		drm_err(mtk_dp->drm_dev,
			"Can't enable bridge as nothing is plugged in\n");
		return;
	}

	//training
	for (i = 0; i < 50; i++) {
		ret = mtk_dp_train_handler(mtk_dp);
		if (ret) {
			drm_err(mtk_dp->drm_dev, "Train handler failed %d\n",
				ret);
			return;
		}

		ret = mtk_dp_state_handler(mtk_dp);
		if (ret) {
			drm_err(mtk_dp->drm_dev, "State handler failed %d\n",
				ret);
			return;
		}
	}

	mtk_dp->enabled = true;
	mtk_dp_update_plugged_status(mtk_dp);

	dev_dbg(mtk_dp->dev, "DPTX calc pixel clock = %d MHz\n",
		mtk_dp_read(mtk_dp, 0x33c8) * mtk_dp->train_info.link_rate * 27 / 0x8000);
}

static enum drm_mode_status
mtk_dp_bridge_mode_valid(struct drm_bridge *bridge,
			 const struct drm_display_info *info,
			 const struct drm_display_mode *mode)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	u32 rx_linkrate;
	u32 bpp;

	bpp = info->color_formats & DRM_COLOR_FORMAT_YCRCB422 ? 16 : 24;
	rx_linkrate = (u32)mtk_dp->train_info.link_rate * 27000;
	if (rx_linkrate * mtk_dp->train_info.lane_count < mode->clock * bpp / 8)
		return MODE_CLOCK_HIGH;

	if (mode->clock > 600000)
		return MODE_CLOCK_HIGH;

	if ((mode->clock * 1000) / (mode->htotal * mode->vtotal) > 62)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static u32 *mtk_dp_bridge_atomic_get_output_bus_fmts(
	struct drm_bridge *bridge, struct drm_bridge_state *bridge_state,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state, unsigned int *num_output_fmts)
{
	u32 *output_fmts;

	*num_output_fmts = 0;
	output_fmts = kcalloc(1, sizeof(*output_fmts), GFP_KERNEL);
	if (!output_fmts)
		return NULL;
	*num_output_fmts = 1;
	output_fmts[0] = MEDIA_BUS_FMT_FIXED;
	return output_fmts;
}

static const u32 mt8195_input_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static u32 *mtk_dp_bridge_atomic_get_input_bus_fmts(
	struct drm_bridge *bridge, struct drm_bridge_state *bridge_state,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state, u32 output_fmt,
	unsigned int *num_input_fmts)
{
	u32 *input_fmts;
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	struct drm_display_info *display_info =
		&conn_state->connector->display_info;
	u32 rx_linkrate;

	rx_linkrate = (u32)mtk_dp->train_info.link_rate * 27000;
	*num_input_fmts = 0;
	input_fmts = kcalloc(ARRAY_SIZE(mt8195_input_fmts), sizeof(*input_fmts),
			     GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	*num_input_fmts = ARRAY_SIZE(mt8195_input_fmts);

	memcpy(input_fmts, mt8195_input_fmts,
	       sizeof(*input_fmts) * ARRAY_SIZE(mt8195_input_fmts));

	if (((rx_linkrate * mtk_dp->train_info.lane_count) <
	     (mode->clock * 24 / 8)) &&
	    ((rx_linkrate * mtk_dp->train_info.lane_count) >
	     (mode->clock * 16 / 8)) &&
	    (display_info->color_formats & DRM_COLOR_FORMAT_YCRCB422)) {
		kfree(input_fmts);
		input_fmts = kcalloc(1, sizeof(*input_fmts), GFP_KERNEL);
		*num_input_fmts = 1;
		input_fmts[0] = MEDIA_BUS_FMT_YUYV8_1X16;
	}

	return input_fmts;
}

static int mtk_dp_bridge_atomic_check(struct drm_bridge *bridge,
				      struct drm_bridge_state *bridge_state,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct mtk_dp *mtk_dp = mtk_dp_from_bridge(bridge);
	unsigned int input_bus_format;

	input_bus_format = bridge_state->input_bus_cfg.format;

	dev_info(mtk_dp->dev, "input format 0x%04x, output format 0x%04x\n",
		 bridge_state->input_bus_cfg.format,
		 bridge_state->output_bus_cfg.format);

	mtk_dp->input_fmt = input_bus_format;
	if (mtk_dp->input_fmt == MEDIA_BUS_FMT_YUYV8_1X16)
		mtk_dp->info.format = MTK_DP_COLOR_FORMAT_YUV_422;
	else
		mtk_dp->info.format = MTK_DP_COLOR_FORMAT_RGB_444;

	return 0;
}

static const struct drm_bridge_funcs mtk_dp_bridge_funcs = {
	.atomic_check = mtk_dp_bridge_atomic_check,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_get_output_bus_fmts = mtk_dp_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dp_bridge_atomic_get_input_bus_fmts,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.attach = mtk_dp_bridge_attach,
	.detach = mtk_dp_bridge_detach,
	.atomic_enable = mtk_dp_bridge_atomic_enable,
	.atomic_disable = mtk_dp_bridge_atomic_disable,
	.mode_valid = mtk_dp_bridge_mode_valid,
	.get_edid = mtk_dp_get_edid,
	.detect = mtk_dp_bdg_detect,
};

/*
 * HDMI audio codec callbacks
 */
static int mtk_dp_audio_hw_params(struct device *dev, void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	struct mtk_dp_audio_cfg cfg;

	if (!mtk_dp->enabled) {
		pr_err("%s, DP is not ready!\n", __func__);
		return -ENODEV;
	}

	cfg.channels = params->cea.channels;
	cfg.sample_rate = params->sample_rate;
	cfg.word_length_bits = 24;

	mtk_dp_audio_setup(mtk_dp, &cfg);

	return 0;
}

static int mtk_dp_audio_startup(struct device *dev, void *data)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	mtk_dp_audio_mute(mtk_dp, false);

	return 0;
}

static void mtk_dp_audio_shutdown(struct device *dev, void *data)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	mtk_dp_audio_mute(mtk_dp, true);
}

static int mtk_dp_audio_get_eld(struct device *dev, void *data, uint8_t *buf,
				size_t len)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	if (mtk_dp->enabled)
		memcpy(buf, mtk_dp->connector_eld, len);
	else
		memset(buf, 0, len);

	dev_dbg(mtk_dp->dev, "get eld: %*ph\n", (int)len, buf);

	return 0;
}

static int mtk_dp_audio_hook_plugged_cb(struct device *dev, void *data,
					hdmi_codec_plugged_cb fn,
					struct device *codec_dev)
{
	struct mtk_dp *mtk_dp = data;

	mutex_lock(&mtk_dp->update_plugged_status_lock);
	mtk_dp->plugged_cb = fn;
	mtk_dp->codec_dev = codec_dev;
	mutex_unlock(&mtk_dp->update_plugged_status_lock);

	mtk_dp_update_plugged_status(mtk_dp);

	return 0;
}

static const struct hdmi_codec_ops mtk_dp_audio_codec_ops = {
	.hw_params = mtk_dp_audio_hw_params,
	.audio_startup = mtk_dp_audio_startup,
	.audio_shutdown = mtk_dp_audio_shutdown,
	.get_eld = mtk_dp_audio_get_eld,
	.hook_plugged_cb = mtk_dp_audio_hook_plugged_cb,
	.no_capture_mute = 1,
};

static int mtk_dp_register_audio_driver(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	struct hdmi_codec_pdata codec_data = {
		.ops = &mtk_dp_audio_codec_ops,
		.max_i2s_channels = 8,
		.i2s = 1,
		.data = mtk_dp,
	};
	struct platform_device *pdev;

	pdev = platform_device_register_data(dev, HDMI_CODEC_DRV_NAME,
					     PLATFORM_DEVID_AUTO, &codec_data,
					     sizeof(codec_data));
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	return 0;
}

static int mtk_dp_probe(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp;
	struct device *dev = &pdev->dev;
	int ret;
	int irq_num = 0;
	struct drm_panel *panel = NULL;

	mtk_dp = devm_kzalloc(dev, sizeof(*mtk_dp), GFP_KERNEL);
	if (!mtk_dp)
		return -ENOMEM;

	mtk_dp->dev = dev;

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(dev, "failed to request dp irq resource\n");
		return -EPROBE_DEFER;
	}

	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0, &panel,
					  &mtk_dp->next_bridge);
	if (ret == -ENODEV) {
		dev_info(
			dev,
			"No panel connected in devicetree, continuing as external DP\n");
		mtk_dp->next_bridge = NULL;
	} else if (ret) {
		dev_err(dev, "Failed to find panel or bridge: %d\n", ret);
		return ret;
	}

	if (panel) {
		mtk_dp->next_bridge = devm_drm_panel_bridge_add(dev, panel);
		if (IS_ERR(mtk_dp->next_bridge)) {
			ret = PTR_ERR(mtk_dp->next_bridge);
			dev_err(dev, "Failed to create bridge: %d\n", ret);
			return -EPROBE_DEFER;
		}
	}

	ret = mtk_dp_dt_parse_pdata(mtk_dp, pdev);
	if (ret)
		return ret;

	mtk_dp_aux_init(mtk_dp);

	ret = devm_request_threaded_irq(dev, irq_num, mtk_dp_hpd_event,
					mtk_dp_hpd_event_thread,
					IRQ_TYPE_LEVEL_HIGH, dev_name(dev),
					mtk_dp);
	if (ret) {
		dev_err(dev, "failed to request mediatek dptx irq\n");
		return -EPROBE_DEFER;
	}

	mutex_init(&mtk_dp->dp_lock);
	mutex_init(&mtk_dp->edid_lock);

	platform_set_drvdata(pdev, mtk_dp);

	if (!mtk_dp_is_edp(mtk_dp)) {
		mutex_init(&mtk_dp->update_plugged_status_lock);
		ret = mtk_dp_register_audio_driver(dev);
		if (ret) {
			dev_err(dev, "Failed to register audio driver: %d\n",
				ret);
			return ret;
		}
	}

	mtk_dp->phy_dev = platform_device_register_data(dev, "mediatek-dp-phy",
							PLATFORM_DEVID_AUTO,
							&mtk_dp->regs,
							sizeof(&mtk_dp->regs));
	if (IS_ERR(mtk_dp->phy_dev)) {
		dev_err(dev, "Failed to create device mediatek-dp-phy: %ld\n",
			PTR_ERR(mtk_dp->phy_dev));
		return PTR_ERR(mtk_dp->phy_dev);
	}

	mtk_dp_get_calibration_data(mtk_dp);

	mtk_dp->phy = dev_get_drvdata(&mtk_dp->phy_dev->dev);
	if (IS_ERR(mtk_dp->phy)) {
		dev_err(dev, "Failed to get phy: %ld\n", PTR_ERR(mtk_dp->phy));
		platform_device_unregister(mtk_dp->phy_dev);
		return PTR_ERR(mtk_dp->phy);
	}

	mtk_dp->bridge.funcs = &mtk_dp_bridge_funcs;
	mtk_dp->bridge.of_node = dev->of_node;
	if (mtk_dp_is_edp(mtk_dp))
		mtk_dp->bridge.type = DRM_MODE_CONNECTOR_eDP;
	else
		mtk_dp->bridge.type = DRM_MODE_CONNECTOR_DisplayPort;

	mtk_dp->bridge.ops =
		DRM_BRIDGE_OP_DETECT | DRM_BRIDGE_OP_EDID | DRM_BRIDGE_OP_HPD;
	drm_bridge_add(&mtk_dp->bridge);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	return 0;
}

static int mtk_dp_remove(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp = platform_get_drvdata(pdev);

	platform_device_unregister(mtk_dp->phy_dev);

	mtk_dp_video_mute(mtk_dp, true);
	mtk_dp_audio_mute(mtk_dp, true);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_dp_suspend(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	if (mtk_dp_plug_state(mtk_dp)) {
		drm_dp_dpcd_writeb(&mtk_dp->aux, DP_SET_POWER, DP_SET_POWER_D3);
		usleep_range(2000, 3000);
	}

	mtk_dp_power_disable(mtk_dp);
	mtk_dp_hwirq_enable(mtk_dp, false);

	pm_runtime_put_sync(dev);

	return 0;
}

static int mtk_dp_resume(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	pm_runtime_get_sync(dev);

	mtk_dp_init_port(mtk_dp);
	mtk_dp_power_enable(mtk_dp);
	mtk_dp_hwirq_enable(mtk_dp, true);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_dp_pm_ops, mtk_dp_suspend, mtk_dp_resume);

static const struct of_device_id mtk_dp_of_match[] = {
	{
		.compatible = "mediatek,mt8195-dp-tx",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dp_of_match);

struct platform_driver mtk_dp_driver = {
	.probe = mtk_dp_probe,
	.remove = mtk_dp_remove,
	.driver = {
		.name = "mediatek-drm-dp",
		.of_match_table = mtk_dp_of_match,
		.pm = &mtk_dp_pm_ops,
	},
};

MODULE_AUTHOR("Jason-JH.Lin <jason-jh.lin@mediatek.com>");
MODULE_AUTHOR("Markus Schneider-Pargmann <msp@baylibre.com>");
MODULE_DESCRIPTION("MediaTek DisplayPort Driver");
MODULE_LICENSE("GPL v2");
