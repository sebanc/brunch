// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/extcon.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <crypto/hash.h>
#include <crypto/sha.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>

#include <sound/hdmi-codec.h>

/* Vendor option */
#define AUDIO_SELECT I2S
#define AUDIO_TYPE LPCM
#define AUDIO_SAMPLE_RATE SAMPLE_RATE_48K
#define AUDIO_CHANNEL_COUNT 2

/*
 * 0: Standard I2S
 * 1: 32bit I2S
 */
#define I2S_INPUT_FORMAT 1

/*
 * 0: Left-justified
 * 1: Right-justified
 */
#define I2S_JUSTIFIED 0

/*
 * 0: Data delay 1T correspond to WS
 * 1: No data delay correspond to WS
 */
#define I2S_DATA_DELAY 0

/*
 * 0: Left channel
 * 1: Right channel
 */
#define I2S_WS_CHANNEL 0

/*
 * 0: MSB shift first
 * 1: LSB shift first
 */
#define I2S_DATA_SEQUENCE 0

/*
 * IT6505 maximum link rate
 * RBR : 1.62 Gbps/lane
 * HBR : 2.7  Gbps/lane
 * HBR2: 5.4  Gbps/lane
 * HBR3: 8.1  Gbps/lane
 */
#define MAX_LINK_RATE HBR

/* IT6505 maximum lane count */
#define MAX_LANE_COUNT 4

#define TRAINING_LINK_RATE HBR
#define TRAINING_LANE_COUNT 4
#define ENABLE_DP_LANE_SWAP 0
#define AUX_WAIT_TIMEOUT_MS 15
#define PIXEL_CLK_DELAY 1
#define PIXEL_CLK_INVERSE 0
#define ADJUST_PHASE_THRESHOLD 80000
#define MAX_PIXEL_CLK 95000
#define DEFAULT_DRV_HOLD 0
#define DEFAULT_PWR_ON 0
#define AUX_FIFO_MAX_SIZE 0x10

/*
 * Vendor option afe settings for different platforms
 * 0: for bitland 10e, quanta zde
 * 1: for google kukui p1/p2, huaqin krane
 */

static u8 afe_setting_table[2][3] = {
	{0, 0, 0},
	{0x93, 0x2A, 0x85}
};

enum it6505_sys_state {
	SYS_UNPLUG = 0,
	SYS_HPD,
	SYS_TRAIN,
	SYS_WAIT,
	SYS_TRAINFAIL,
	SYS_HDCP,
	SYS_NOROP,
	SYS_UNKNOWN,
};

enum it6505_audio_select {
	I2S = 0,
	SPDIF,
};

enum it6505_audio_sample_rate {
	SAMPLE_RATE_24K = 0x6,
	SAMPLE_RATE_32K = 0x3,
	SAMPLE_RATE_48K = 0x2,
	SAMPLE_RATE_96K = 0xA,
	SAMPLE_RATE_192K = 0xE,
	SAMPLE_RATE_44_1K = 0x0,
	SAMPLE_RATE_88_2K = 0x8,
	SAMPLE_RATE_176_4K = 0xC,
};

enum it6505_audio_type {
	LPCM = 0,
	NLPCM,
	DSS,
};

enum it6505_audio_word_length {
	WORD_LENGTH_16BIT = 0,
	WORD_LENGTH_18BIT,
	WORD_LENGTH_20BIT,
	WORD_LENGTH_24BIT,
};

/*
 * Audio Sample Word Length
 * WORD_LENGTH_16BIT
 * WORD_LENGTH_18BIT
 * WORD_LENGTH_20BIT
 * WORD_LENGTH_24BIT
 */
#define AUDIO_WORD_LENGTH WORD_LENGTH_24BIT

enum it6505_link_rate {
	RBR,
	HBR,
	HBR2,
	HBR3,
};

struct it6505_audio_sample_rate_map {
	enum it6505_audio_sample_rate rate;
	int sample_rate_value;
};

struct it6505_platform_data {
	struct regulator *pwr18;
	struct regulator *ovdd;
	struct gpio_desc *gpiod_hpd;
	struct gpio_desc *gpiod_reset;
};

struct it6505_drm_dp_link {
	unsigned char revision;
	unsigned int rate;
	unsigned int num_lanes;
	unsigned long capabilities;
};

struct it6505 {
	struct drm_dp_aux aux;
	struct drm_bridge bridge;
	struct i2c_client *client;
	struct edid *edid;
	struct drm_connector connector;
	struct it6505_drm_dp_link link;
	struct it6505_platform_data pdata;
	struct mutex lock;
	struct mutex mode_lock;
	struct mutex aux_lock;
	struct regmap *regmap;
	struct drm_display_mode video_info;
	wait_queue_head_t edid_wait;
	struct notifier_block event_nb;
	struct extcon_dev *extcon;
	struct work_struct extcon_wq;
	struct delayed_work delayed_audio;
	enum it6505_sys_state state;
	bool hbr;
	u8 lane_count;
	bool enable_ssc;
	bool lane_swap_disabled;
	bool lane_swap;

	enum it6505_audio_select audio_select;
	enum it6505_audio_sample_rate audio_sample_rate;
	enum it6505_audio_type audio_type;
	enum it6505_audio_word_length audio_word_length;
	u8 audio_channel_count;
	u8 i2s_input_format;
	u8 i2s_justified;
	u8 i2s_data_delay;
	u8 i2s_ws_channel;
	u8 i2s_data_sequence;
	bool hdcp_flag;
	bool enable_hdcp;
	bool enable_audio;
	u8 train_count;
	u8 train_fail_hpd;
	bool train_pass;
	bool enable_auto_train;
	bool cp_capable;
	u8 sha1_input[64];
	u8 av[5][4];
	u8 bv[5][4];
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	bool powered;
	u8 dpcd_sink_count;
	/* it6505 driver hold option */
	bool enable_drv_hold;

	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	enum drm_connector_status last_connector_status;
};

struct it6505_lane_voltage_pre_emphasis {
	u8 voltage_swing[MAX_LANE_COUNT];
	u8 pre_emphasis[MAX_LANE_COUNT];
};

static const struct regmap_range it6505_bridge_volatile_ranges[] = {
	{ .range_min = 0, .range_max = 0xFF },
};

static const struct regmap_access_table it6505_bridge_volatile_table = {
	.yes_ranges = it6505_bridge_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(it6505_bridge_volatile_ranges),
};

static const struct regmap_config it6505_bridge_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &it6505_bridge_volatile_table,
	.cache_type = REGCACHE_NONE,
};

static int dptx_read(struct it6505 *it6505, unsigned int reg_addr)
{
	unsigned int value;
	int err;
	struct device *dev = &it6505->client->dev;

	err = regmap_read(it6505->regmap, reg_addr, &value);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "read failed reg[0x%x] err: %d", reg_addr,
			      err);
		return err;
	}

	return value;
}

static void dptx_debug_dump(struct it6505 *it6505)
{
	unsigned int i, j;
	u8 regs[16];
	struct device *dev = &it6505->client->dev;

	for (i = 0; i <= 0xff; i += 16) {
		for (j = 0; j < 16; j++)
			regs[j] = dptx_read(it6505, i + j);

		DRM_DEV_DEBUG_DRIVER(dev, "[0x%02x] = %16ph", i, regs);
	}
}

static int dptx_write(struct it6505 *it6505, unsigned int reg_addr,
		      unsigned int reg_val)
{
	int err;
	struct device *dev = &it6505->client->dev;

	err = regmap_write(it6505->regmap, reg_addr, reg_val);

	if (err < 0) {
		DRM_DEV_ERROR(dev, "write failed reg[0x%x] = 0x%x err = %d",
			      reg_addr, reg_val, err);
		return err;
	}

	return 0;
}

static int dptx_set_bits(struct it6505 *it6505, unsigned int reg,
			 unsigned int mask, unsigned int value)
{
	int err;
	struct device *dev = &it6505->client->dev;

	err = regmap_update_bits(it6505->regmap, reg, mask, value);
	if (err < 0) {
		DRM_DEV_ERROR(
			dev, "write reg[0x%x] = 0x%x mask = 0x%x failed err %d",
			reg, value, mask, err);
		return err;
	}

	return 0;
}

static unsigned int dpcd_read(struct it6505 *it6505, unsigned long offset)
{
	u8 value;
	int ret;
	struct device *dev = &it6505->client->dev;

	ret = drm_dp_dpcd_readb(&it6505->aux, offset, &value);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "DPCD read failed [0x%lx] ret: %d", offset,
			      ret);
		return ret;
	}
	return value;
}

static int dpcd_write(struct it6505 *it6505, unsigned long offset,
		      unsigned long datain)
{
	int ret;
	struct device *dev = &it6505->client->dev;

	ret = drm_dp_dpcd_writeb(&it6505->aux, offset, datain);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "DPCD write failed [0x%lx] ret: %d", offset,
			      ret);
		return ret;
	}
	return 0;
}

static void dptx_debug_print(struct it6505 *it6505, unsigned int reg,
			     const char *prefix)
{
	int val;
	struct device *dev = &it6505->client->dev;

	if (likely(!(drm_debug & DRM_UT_DRIVER)))
		return;

	val = dptx_read(it6505, reg);
	if (val < 0)
		DRM_DEV_DEBUG_DRIVER(dev, "%s reg[%02x] read error (%d)",
				     prefix, reg, val);
	else
		DRM_DEV_DEBUG_DRIVER(dev, "%s reg[%02x] = 0x%02x", prefix, reg,
				     val);
}

static void dpcd_debug_print(struct it6505 *it6505, unsigned int reg,
			     const char *prefix)
{
	unsigned int val;
	struct device *dev = &it6505->client->dev;

	if (likely(!(drm_debug & DRM_UT_DRIVER)))
		return;

	val = dpcd_read(it6505, reg);
	if (val < 0)
		DRM_DEV_DEBUG_DRIVER(dev, "%s reg%02x read error", prefix, reg);
	else
		DRM_DEV_DEBUG_DRIVER(dev, "%s DPCD%02x = 0x%02x", prefix, reg,
				     val);
}

static int it6505_drm_dp_link_probe(struct drm_dp_aux *aux,
				    struct it6505_drm_dp_link *link)
{
	u8 values[3];
	int err;

	memset(link, 0, sizeof(*link));

	err = drm_dp_dpcd_read(aux, DP_DPCD_REV, values, sizeof(values));
	if (err < 0)
		return err;

	link->revision = values[0];
	link->rate = drm_dp_bw_code_to_link_rate(values[1]);
	link->num_lanes = values[2] & DP_MAX_LANE_COUNT_MASK;

	if (values[2] & DP_ENHANCED_FRAME_CAP)
		link->capabilities = 1;

	return 0;
}

static int it6505_drm_dp_link_power_up(struct drm_dp_aux *aux,
				       struct it6505_drm_dp_link *link)
{
	u8 value;
	int err;

	/* DP_SET_POWER register is only available on DPCD v1.1 and later */
	if (link->revision < 0x11)
		return 0;

	err = drm_dp_dpcd_readb(aux, DP_SET_POWER, &value);
	if (err < 0)
		return err;

	value &= ~DP_SET_POWER_MASK;
	value |= DP_SET_POWER_D0;

	err = drm_dp_dpcd_writeb(aux, DP_SET_POWER, value);
	if (err < 0)
		return err;

	/*
	 * According to the DP 1.1 specification, a "Sink Device must exit the
	 * power saving state within 1 ms" (Section 2.5.3.1, Table 5-52, "Sink
	 * Control Field" (register 0x600).
	 */
	usleep_range(1000, 2000);

	return 0;
}

static int it6505_drm_dp_link_configure(struct it6505 *it6505)
{
	u8 values[2];
	int err;
	struct it6505_drm_dp_link *link = &it6505->link;
	struct drm_dp_aux *aux = &it6505->aux;

	values[0] = it6505->hbr ? DP_LINK_BW_2_7 : DP_LINK_BW_1_62;
	values[1] = link->num_lanes;

	if (link->capabilities)
		values[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN;

	err = drm_dp_dpcd_write(aux, DP_LINK_BW_SET, values, sizeof(values));
	if (err < 0)
		return err;

	return 0;
}

static inline struct it6505 *connector_to_it6505(struct drm_connector *c)
{
	return container_of(c, struct it6505, connector);
}

static inline struct it6505 *bridge_to_it6505(struct drm_bridge *bridge)
{
	return container_of(bridge, struct it6505, bridge);
}

static void it6505_init_variable(struct it6505 *it6505)
{
	it6505->audio_select = AUDIO_SELECT;
	it6505->audio_sample_rate = AUDIO_SAMPLE_RATE;
	it6505->audio_channel_count = AUDIO_CHANNEL_COUNT;
	it6505->audio_type = AUDIO_TYPE;
	it6505->i2s_input_format = I2S_INPUT_FORMAT;
	it6505->i2s_justified = I2S_JUSTIFIED;
	it6505->i2s_data_delay = I2S_DATA_DELAY;
	it6505->i2s_ws_channel = I2S_WS_CHANNEL;
	it6505->i2s_data_sequence = I2S_DATA_SEQUENCE;
	it6505->audio_word_length = AUDIO_WORD_LENGTH;
	it6505->enable_audio = false;
	it6505->enable_hdcp = true;
	it6505->lane_count = min(MAX_LANE_COUNT, TRAINING_LANE_COUNT);
	it6505->hbr = min(MAX_LINK_RATE, TRAINING_LINK_RATE) ? true : false;
	it6505->enable_ssc = true;
	it6505->lane_swap = ENABLE_DP_LANE_SWAP ? true : false;
	it6505->train_pass = false;
}

static void it6505_lane_termination_on(struct it6505 *it6505)
{
	if (dptx_read(it6505, 0xCF) == 0xF0)
		dptx_set_bits(it6505, 0x5D, 0x80, 0x00);

	if (dptx_read(it6505, 0xCF) == 0x70) {
		if (it6505->lane_swap) {
			switch (it6505->lane_count) {
			case 1:
			case 2:
				dptx_set_bits(it6505, 0x5D, 0x0C, 0x08);
				break;
			default:
				dptx_set_bits(it6505, 0x5D, 0x0C, 0x0C);
				break;
			}
		} else {
			switch (it6505->lane_count) {
			case 1:
			case 2:
				dptx_set_bits(it6505, 0x5D, 0x0C, 0x04);
				break;
			default:
				dptx_set_bits(it6505, 0x5D, 0x0C, 0x0C);
				break;
			}
		}
	}
}

static void it6505_lane_termination_off(struct it6505 *it6505)
{
	if (dptx_read(it6505, 0xCF) == 0xF0)
		dptx_set_bits(it6505, 0x5D, 0x80, 0x80);

	if (dptx_read(it6505, 0xCF) == 0x70)
		dptx_set_bits(it6505, 0x5D, 0x0C, 0x00);
}

static void it6505_lane_power_on(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0x5C, 0xF1, ((BIT(it6505->lane_count) - 1) <<
	(it6505->lane_swap ? (8 - it6505->lane_count) : 4)) | 0x01);
}

static void it6505_lane_power_off(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0x5C, 0xF0, 0x00);
}

static void it6505_lane_off(struct it6505 *it6505)
{
	it6505_lane_power_off(it6505);
	it6505_lane_termination_off(it6505);
}

static void it6505_aux_termination_on(struct it6505 *it6505)
{
	if (dptx_read(it6505, 0xCF) == 0xF0)
		it6505_lane_termination_on(it6505);

	if (dptx_read(it6505, 0xCF) == 0x70)
		dptx_set_bits(it6505, 0x5D, 0x80, 0x80);
}

static void it6505_aux_power_on(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0x5E, 0x02, 0x02);
}

static void it6505_aux_on(struct it6505 *it6505)
{
	it6505_aux_power_on(it6505);
	it6505_aux_termination_on(it6505);
}

static void it6505_aux_reset(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0x05, 0x08, 0x08);
	dptx_set_bits(it6505, 0x05, 0x08, 0x00);
}

static bool dptx_get_sink_hpd(struct it6505 *it6505)
{
	int reg0d = dptx_read(it6505, 0x0D);

	if (reg0d < 0)
		return false;

	return (reg0d & BIT(1)) == BIT(1);
}

static void dptx_select_bank(struct it6505 *it6505, unsigned int bank_id)
{
	dptx_set_bits(it6505, 0x0F, 0x01, bank_id & BIT(0));
}

static void it6505_int_mask_on(struct it6505 *it6505)
{
	dptx_write(it6505, 0x09, 0x1F);
	dptx_write(it6505, 0x0A, 0x07);
	dptx_write(it6505, 0x0B, 0x90);
}

static void it6505_int_mask_off(struct it6505 *it6505)
{
	dptx_write(it6505, 0x09, 0x00);
	dptx_write(it6505, 0x0A, 0x00);
	dptx_write(it6505, 0x0B, 0x00);
}

static bool dptx_init(struct it6505 *it6505)
{
	int err;
	struct device *dev = &it6505->client->dev;

	err = dptx_write(it6505, 0x05, 0x3B);

	if (err < 0)
		return false;

	usleep_range(1000, 2000);
	err = regmap_write(it6505->regmap, 0x05, 0x1F);

	if (err == 0) {
		DRM_DEV_ERROR(dev, "write failed reg[0x05] = 0x1F err = 0");
		return false;
	}
	usleep_range(1000, 1500);
	return true;
}

static void it6505_init_and_mask_on(struct it6505 *it6505)
{
	int err;
	struct device *dev = &it6505->client->dev;

	err = dptx_init(it6505);

	if (!err)
		DRM_DEV_ERROR(dev, "dptx_init error!");

	it6505_int_mask_on(it6505);
}

static int it6505_poweron(struct it6505 *it6505)
{
	struct it6505_platform_data *pdata = &it6505->pdata;
	int err;
	struct device *dev = &it6505->client->dev;

	if (it6505->powered) {
		DRM_DEV_DEBUG_DRIVER(dev, "it6505 already powered on");
		return 0;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "it6505 start to power on");

	err = regulator_enable(pdata->pwr18);

	if (err)
		return err;

	/* time interval between IVDD and OVDD at least be 1ms */
	usleep_range(1000, 2000);
	err = regulator_enable(pdata->ovdd);

	if (err) {
		regulator_disable(pdata->pwr18);
		return err;
	}
	/* time interval between OVDD and SYSRSTN at least be 10ms */
	usleep_range(10000, 20000);
	gpiod_set_value_cansleep(pdata->gpiod_reset, 0);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(pdata->gpiod_reset, 1);
	usleep_range(10000, 20000);

	it6505_init_and_mask_on(it6505);
	it6505->powered = true;
	return 0;
}

static int it6505_poweroff(struct it6505 *it6505)
{
	struct it6505_platform_data *pdata = &it6505->pdata;
	int err;
	struct device *dev = &it6505->client->dev;

	if (!it6505->powered) {
		DRM_DEV_DEBUG_DRIVER(dev, "power had been already off");
		return 0;
	}
	gpiod_set_value_cansleep(pdata->gpiod_reset, 0);
	err = regulator_disable(pdata->pwr18);

	if (err)
		return err;

	err = regulator_disable(pdata->ovdd);

	if (err)
		return err;

	kfree(it6505->edid);
	it6505->edid = NULL;
	it6505->powered = false;
	return 0;
}

static int dptx_read_word(struct it6505 *it6505, unsigned int reg)
{
	int val0, val1;

	val0 = dptx_read(it6505, reg);
	if (val0 < 0)
		return val0;

	val1 = dptx_read(it6505, reg + 1);
	if (val1 < 0)
		return val1;

	return (val1 << 8) | val0;
}

static void show_video_info(struct it6505 *it6505)
{
	int hsync_pol, vsync_pol, interlaced;
	int htotal, hdes, hdew, hfph, hsyncw;
	int vtotal, vdes, vdew, vfph, vsyncw;
	int rddata, i;
	int pclk, sum;

	usleep_range(10000, 15000);
	dptx_select_bank(it6505, 0);
	rddata = dptx_read(it6505, 0xA0);
	hsync_pol = rddata & BIT(0);
	vsync_pol = (rddata & BIT(2)) >> 2;
	interlaced = (rddata & BIT(4)) >> 4;

	htotal = dptx_read_word(it6505, 0xA1) & 0x1FFF;
	hdes = dptx_read_word(it6505, 0xA3) & 0x1FFF;
	hdew = dptx_read_word(it6505, 0xA5) & 0x1FFF;
	hfph = dptx_read_word(it6505, 0xA7) & 0x1FFF;
	hsyncw = dptx_read_word(it6505, 0xA9) & 0x1FFF;

	vtotal = dptx_read_word(it6505, 0xAB) & 0xFFF;
	vdes = dptx_read_word(it6505, 0xAD) & 0xFFF;
	vdew = dptx_read_word(it6505, 0xAF) & 0xFFF;
	vfph = dptx_read_word(it6505, 0xB1) & 0xFFF;
	vsyncw = dptx_read_word(it6505, 0xB3) & 0xFFF;

	sum = 0;
	for (i = 0; i < 100; i++) {
		dptx_set_bits(it6505, 0x12, 0x80, 0x80);
		usleep_range(10000, 15000);
		dptx_set_bits(it6505, 0x12, 0x80, 0x00);
		rddata = dptx_read_word(it6505, 0x13) & 0xFFF;

		sum += rddata;
	}

	sum /= 100;
	pclk = 13500 * 2048 / sum;
	it6505->video_info.clock = pclk;
	it6505->video_info.hdisplay = hdew;
	it6505->video_info.hsync_start = hdew + hfph;
	it6505->video_info.hsync_end = hdew + hfph + hsyncw;
	it6505->video_info.htotal = htotal;
	it6505->video_info.vdisplay = vdew;
	it6505->video_info.vsync_start = vdew + vfph;
	it6505->video_info.vsync_end = vdew + vfph + vsyncw;
	it6505->video_info.vtotal = vtotal;
	it6505->video_info.vrefresh = pclk / htotal / vtotal;

	DRM_DEV_DEBUG_DRIVER(&it6505->client->dev, DRM_MODE_FMT,
			     DRM_MODE_ARG(&it6505->video_info));
}

static const struct it6505_audio_sample_rate_map audio_sample_rate_map[] = {
	{SAMPLE_RATE_24K, 24000},
	{SAMPLE_RATE_32K, 32000},
	{SAMPLE_RATE_48K, 48000},
	{SAMPLE_RATE_96K, 96000},
	{SAMPLE_RATE_192K, 192000},
	{SAMPLE_RATE_44_1K, 44100},
	{SAMPLE_RATE_88_2K, 88200},
	{SAMPLE_RATE_176_4K, 176400},
};

static void show_audio_m_count(struct it6505 *it6505)
{
	unsigned int audio_n, vclk, aclk = 0, audio_m_cal, audio_m_count, i;
	struct device *dev = &it6505->client->dev;

	vclk = it6505->hbr ? 2700000 : 1620000;

	for (i = 0; i < ARRAY_SIZE(audio_sample_rate_map); i++) {
		if (it6505->audio_sample_rate ==
		    audio_sample_rate_map[i].rate) {
			aclk = audio_sample_rate_map[i].sample_rate_value / 100;
			break;
		}
	}

	if (i == ARRAY_SIZE(audio_sample_rate_map))
		aclk = 0;

	audio_n = (dptx_read(it6505, 0xE0) << 16) +
		  (dptx_read(it6505, 0xDF) << 8) + dptx_read(it6505, 0xDE);
	audio_m_cal = audio_n * aclk / vclk * 512;
	audio_m_count = (dptx_read(it6505, 0xE6) << 16) +
		  (dptx_read(it6505, 0xE5) << 8) + dptx_read(it6505, 0xE4);
	DRM_DEV_DEBUG_DRIVER(dev, "audio N:0x%06x", audio_n);
	DRM_DEV_DEBUG_DRIVER(dev, "audio M cal:0x%06x", audio_m_cal);
	DRM_DEV_DEBUG_DRIVER(dev, "audio M count:0x%06x", audio_m_count);
}

static void it6505_audio_disable(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0xD3, 0x20, 0x20);
	dptx_set_bits(it6505, 0xB8, 0x0F, 0x00);
	dptx_set_bits(it6505, 0xE8, 0x22, 0x00);
	dptx_set_bits(it6505, 0x05, 0x02, 0x02);
	it6505->enable_audio = false;
}

static const char *const state_string[] = {
	[SYS_UNPLUG] = "SYS_UNPLUG",
	[SYS_HPD] = "SYS_HPD",
	[SYS_TRAIN] = "SYS_TRAIN",
	[SYS_WAIT] = "SYS_WAIT",
	[SYS_TRAINFAIL] = "SYS_TRAINFAIL",
	[SYS_HDCP] = "SYS_HDCP",
	[SYS_NOROP] = "SYS_NOROP",
	[SYS_UNKNOWN] = "SYS_UNKNOWN",
};

static void dptx_sys_chg(struct it6505 *it6505, enum it6505_sys_state newstate)
{
	int i;
	struct device *dev = &it6505->client->dev;

	if (newstate != SYS_UNPLUG) {
		if (!dptx_get_sink_hpd(it6505))
			newstate = SYS_UNPLUG;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "sys_state change: %s -> %s",
			     state_string[it6505->state],
			     state_string[newstate]);
	it6505->state = newstate;

	switch (it6505->state) {
	case SYS_UNPLUG:
		kfree(it6505->edid);
		it6505->edid = NULL;
		DRM_DEV_DEBUG_DRIVER(dev, "Free it6505 EDID memory");
		it6505->enable_auto_train = true;

		if (it6505->enable_audio)
			it6505_audio_disable(it6505);

		if (it6505->powered) {
			it6505_init_and_mask_on(it6505);
			it6505_lane_off(it6505);
		}
		break;
	case SYS_HPD:
		it6505_aux_on(it6505);
		break;
	case SYS_TRAIN:
		break;
	case SYS_WAIT:
		break;
	case SYS_HDCP:
		break;
	case SYS_NOROP:
		it6505->enable_auto_train = true;
		for (i = 0; i < 3; i++)
			show_video_info(it6505);
		break;
	case SYS_TRAINFAIL:
		/* it6505 goes to idle */
		it6505->enable_auto_train = true;
		if (it6505->dpcd_sink_count == 0)
			dptx_set_bits(it6505, 0x16, 0x40, 0x40);
		break;
	default:
		break;
	}
}

static bool it6505_aux_op_finished(struct it6505 *it6505)
{
	int reg2b = dptx_read(it6505, 0x2B);

	if (reg2b < 0)
		return false;

	return (reg2b & BIT(5)) == 0;
}

static int dptx_aux_wait(struct it6505 *it6505)
{
	int status;
	unsigned long timeout;
	struct device *dev = &it6505->client->dev;

	timeout = jiffies + msecs_to_jiffies(AUX_WAIT_TIMEOUT_MS) + 1;

	while (!it6505_aux_op_finished(it6505)) {
		if (time_after(jiffies, timeout)) {
			DRM_DEV_ERROR(dev, "Timed out waiting AUX to finish");
			return -ETIMEDOUT;
		}
		usleep_range(1000, 2000);
	}

	status = dptx_read(it6505, 0x9F);
	if (status < 0) {
		DRM_DEV_ERROR(dev, "Failed to read AUX channel: %d", status);
		return status;
	}

	if (status & 0x03) {
		DRM_DEV_ERROR(dev, "Failed to wait for AUX (status: 0x%x)",
			      status);
		return -ETIMEDOUT;
	}

	return 0;
}

enum aux_cmd_type {
	CMD_AUX_NATIVE_READ = 0x0,
	CMD_AUX_NATIVE_WRITE = 0x5,
	CMD_AUX_I2C_EDID_READ = 0xB,
};

enum aux_cmd_reply {
	REPLY_ACK,
	REPLY_NACK,
	REPLY_DEFER,
};

static ssize_t it6505_aux_operation(struct it6505 *it6505,
				    enum aux_cmd_type cmd,
				    unsigned int address, u8 *buffer,
				    size_t size, enum aux_cmd_reply *reply)
{
	int i, status;
	bool aux_write_check = false;

	mutex_lock(&it6505->aux_lock);
aux_write_check:

	if (cmd == CMD_AUX_I2C_EDID_READ) {
		/* DP AUX EDID FIFO has maximum length of 16 bytes. */
		size = min_t(size_t, size, AUX_FIFO_MAX_SIZE);
		/* Enable AUX FIFO read back and clear FIFO */
		dptx_write(it6505, 0x23, 0xC3);
		dptx_write(it6505, 0x23, 0xC2);
	} else {
		/* The DP AUX transmit buffer has 4 bytes. */
		size = min_t(size_t, size, 4);
		dptx_write(it6505, 0x23, 0x42);
	}

	/* Start Address[7:0] */
	dptx_write(it6505, 0x24, (address >> 0) & 0xFF);
	/* Start Address[15:8] */
	dptx_write(it6505, 0x25, (address >> 8) & 0xFF);
	/* WriteNum[3:0]+StartAdr[19:16] */
	dptx_write(it6505, 0x26, ((address >> 16) & 0x0F) | ((size - 1) << 4));

	if (cmd == CMD_AUX_NATIVE_WRITE)
		regmap_bulk_write(it6505->regmap, 0x27, buffer, size);

	/* Aux Fire */
	dptx_write(it6505, 0x2B, cmd);

	status = dptx_aux_wait(it6505);
	if (status < 0)
		goto error;

	status = dptx_read(it6505, 0x9F);
	if (status < 0)
		goto error;

	switch ((status >> 6) & 0x3) {
	case 0:
		*reply = REPLY_ACK;
		break;
	case 1:
		*reply = REPLY_DEFER;
		status = -EAGAIN;
		goto error;
	case 2:
		*reply = REPLY_NACK;
		status = -EIO;
		goto error;
	case 3:
		status = -ETIMEDOUT;
		goto error;
	}

	if (cmd == CMD_AUX_NATIVE_WRITE) {
		aux_write_check = true;
		cmd = CMD_AUX_NATIVE_READ;
		goto aux_write_check;
	}

	if (cmd == CMD_AUX_I2C_EDID_READ) {
		for (i = 0; i < size; i++) {
			status = dptx_read(it6505, 0x2F);
			if (status < 0)
				goto error;
			buffer[i] = status;
		}
	} else {
		for (i = 0; i < size; i++) {
			status = dptx_read(it6505, 0x2C + i);
			if (status < 0)
				goto error;

			if (aux_write_check && buffer[size - 1 - i] != status)
				break;

			buffer[size - 1 - i] = status;
		}
	}

	dptx_write(it6505, 0x23, 0x41);
	dptx_write(it6505, 0x23, 0x40);
	mutex_unlock(&it6505->aux_lock);

	return i;

error:
	mutex_unlock(&it6505->aux_lock);
	return status;
}

static ssize_t it6505_aux_do_transfer(struct it6505 *it6505,
				      enum aux_cmd_type cmd,
				      unsigned int address, u8 *buffer,
				      size_t size, enum aux_cmd_reply *reply)
{
	int i, ret, ret_size = 0, request_size;

	for (i = 0; i < size; i += 4) {
		request_size = min((int)size - i, 4);
		ret = it6505_aux_operation(it6505, cmd, address + i,
					   buffer + i, request_size, reply);
		if (ret < 0)
			return ret;
		ret_size +=  ret;
	}

	return ret_size;
}

static ssize_t it6505_aux_transfer(struct drm_dp_aux *aux,
				   struct drm_dp_aux_msg *msg)
{
	struct it6505 *it6505 = container_of(aux, struct it6505, aux);
	u8 cmd;
	bool is_i2c = !(msg->request & DP_AUX_NATIVE_WRITE);
	int ret;
	enum aux_cmd_reply reply;

	/* IT6505 doesn't support arbitrary I2C read / write. */
	if (is_i2c)
		return -EINVAL;

	switch (msg->request) {
	case DP_AUX_NATIVE_READ:
		cmd = CMD_AUX_NATIVE_READ;
		break;
	case DP_AUX_NATIVE_WRITE:
		cmd = CMD_AUX_NATIVE_WRITE;
		break;
	default:
		return -EINVAL;
	}

	ret = it6505_aux_do_transfer(it6505, cmd, msg->address, msg->buffer,
				     msg->size, &reply);
	if (ret < 0)
		return ret;

	switch (reply) {
	case REPLY_ACK:
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;
		break;
	case REPLY_NACK:
		msg->reply = DP_AUX_NATIVE_REPLY_NACK;
		break;
	case REPLY_DEFER:
		msg->reply = DP_AUX_NATIVE_REPLY_DEFER;
		break;
	}

	return ret;
}

static int dptx_get_edidblock(void *data, u8 *buf, unsigned int blockno,
			      size_t len)
{
	struct it6505 *it6505 = data;
	int offset, ret, aux_retry = 100;
	struct device *dev = &it6505->client->dev;
	enum aux_cmd_reply reply;

	it6505_aux_reset(it6505);
	DRM_DEV_DEBUG_DRIVER(dev, "blocknum = %d", blockno);

	for (offset = 0; offset < EDID_LENGTH;) {
		ret = it6505_aux_do_transfer(it6505, CMD_AUX_I2C_EDID_READ,
					     blockno * EDID_LENGTH + offset,
					     buf + offset, 8, &reply);
		if (ret < 0 && ret != -EAGAIN)
			return ret;

		switch (reply) {
		case REPLY_ACK:
			DRM_DEV_DEBUG_DRIVER(dev, "[0x%02x]: %8ph", offset,
					     buf + offset);
			offset += 8;
			aux_retry = 80;
			break;
		case REPLY_NACK:
			return -EIO;
		case REPLY_DEFER:
			msleep(20);
			if (!(--aux_retry))
				return -EIO;
		}
	}

	return 0;
}

static void it6505_disable_hdcp(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0x38, 0x0B, 0x00);
	dptx_set_bits(it6505, 0x05, 0x10, 0x10);
}


static int it6505_get_modes(struct drm_connector *connector)
{
	struct it6505 *it6505 = connector_to_it6505(connector);
	int err, num_modes = 0, i;
	struct device *dev = &it6505->client->dev;

	it6505->train_fail_hpd = 3;
	DRM_DEV_DEBUG_DRIVER(dev, "sink_count:%d edid:%p",
			     it6505->dpcd_sink_count, it6505->edid);

	if (it6505->edid)
		return drm_add_edid_modes(connector, it6505->edid);
	mutex_lock(&it6505->mode_lock);

	for (i = 0; i < 3; i++) {
		dptx_debug_print(it6505, 0x9F, "get_modes:");
		it6505_aux_reset(it6505);
		it6505_disable_hdcp(it6505);
		it6505->edid =
		drm_do_get_edid(&it6505->connector, dptx_get_edidblock, it6505);

		if (it6505->edid)
			break;
		dptx_debug_dump(it6505);
	}
	if (!it6505->edid) {
		DRM_DEV_ERROR(dev, "Failed to read EDID");
		goto unlock;
	}

	err = drm_connector_update_edid_property(connector, it6505->edid);
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to update EDID property: %d", err);
		goto unlock;
	}

	num_modes = drm_add_edid_modes(connector, it6505->edid);

unlock:
	wake_up(&it6505->edid_wait);
	mutex_unlock(&it6505->mode_lock);

	return num_modes;
}

static const struct drm_connector_helper_funcs it6505_connector_helper_funcs = {
	.get_modes = it6505_get_modes,
};

static void it6505_update_plugged_status(struct it6505 *it6505,
					 enum drm_connector_status status)
{
	if (it6505->plugged_cb && it6505->codec_dev)
		it6505->plugged_cb(it6505->codec_dev,
				   status == connector_status_connected);
}

static enum drm_connector_status it6505_detect(struct drm_connector *connector,
					       bool force)
{
	struct it6505 *it6505 = connector_to_it6505(connector);
	enum drm_connector_status status;
	u8 dpcd600;

	mutex_lock(&it6505->mode_lock);
	if (!it6505->powered)
		goto it6505_no_powered;

	it6505_aux_on(it6505);
	dpcd600 = dpcd_read(it6505, DP_SET_POWER);
	it6505->dpcd_sink_count = dpcd_read(it6505, DP_SINK_COUNT);
	it6505->dpcd_sink_count &= 0x3F;
	dpcd_debug_print(it6505, DP_SET_POWER, "detect:");

	if (!dptx_get_sink_hpd(it6505) ||
	    (dpcd600 & DP_SET_POWER_MASK) != DP_SET_POWER_D0) {
it6505_no_powered:
		it6505->dpcd_sink_count = 0;
	}

	if (it6505->dpcd_sink_count) {
		status = connector_status_connected;
	} else {
		status = connector_status_disconnected;
		if (it6505->powered && !dptx_get_sink_hpd(it6505))
			it6505_lane_off(it6505);
	}

	DRM_DEV_DEBUG_DRIVER(&it6505->client->dev, "sink_count:%d status:%d",
			     it6505->dpcd_sink_count, status);

	if (status != it6505->last_connector_status) {
		it6505->last_connector_status = status;
		it6505_update_plugged_status(it6505, status);
	}

	mutex_unlock(&it6505->mode_lock);
	return status;
}

static const struct drm_connector_funcs it6505_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = it6505_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int it6505_extcon_notifier(struct notifier_block *self,
				  unsigned long event, void *ptr)
{
	struct it6505 *it6505 = container_of(self, struct it6505, event_nb);

	schedule_work(&it6505->extcon_wq);
	return NOTIFY_DONE;
}

static void it6505_extcon_work(struct work_struct *work)
{
	struct it6505 *it6505 = container_of(work, struct it6505, extcon_wq);
	int state = extcon_get_state(it6505->extcon, EXTCON_DISP_DP);
	unsigned int pwroffretry = 0;
	struct device *dev = &it6505->client->dev;

	if (it6505->enable_drv_hold)
		return;
	mutex_lock(&it6505->lock);
	DRM_DEV_DEBUG_DRIVER(dev, "EXTCON_DISP_DP = 0x%02x", state);
	if (state > 0) {
		DRM_DEV_DEBUG_DRIVER(dev, "start to power on");
		msleep(1000);
		it6505_poweron(it6505);
		if (!dptx_get_sink_hpd(it6505))
			it6505_lane_off(it6505);
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "start to power off");
		while (it6505_poweroff(it6505) && pwroffretry++ < 5) {
			DRM_DEV_DEBUG_DRIVER(dev, "power off fail %d times",
					     pwroffretry);
		}
		drm_helper_hpd_irq_event(it6505->connector.dev);
		DRM_DEV_DEBUG_DRIVER(dev, "power off it6505 success!");
	}
	mutex_unlock(&it6505->lock);
}

static int it6505_use_notifier_module(struct it6505 *it6505)
{
	int ret;
	struct device *dev = &it6505->client->dev;

	it6505->event_nb.notifier_call = it6505_extcon_notifier;
	INIT_WORK(&it6505->extcon_wq, it6505_extcon_work);
	ret = devm_extcon_register_notifier(&it6505->client->dev,
					    it6505->extcon, EXTCON_DISP_DP,
					    &it6505->event_nb);
	if (ret) {
		DRM_DEV_ERROR(dev, "failed to register notifier for DP");
		return ret;
	}
	schedule_work(&it6505->extcon_wq);

	return 0;
}

static void it6505_remove_notifier_module(struct it6505 *it6505)
{
	if (it6505->extcon) {
		devm_extcon_unregister_notifier(&it6505->client->dev,
						it6505->extcon,
						EXTCON_DISP_DP,
						&it6505->event_nb);
		flush_work(&it6505->extcon_wq);
	}
}

static int it6505_bridge_attach(struct drm_bridge *bridge)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);
	struct device *dev;
	int err;

	dev = &it6505->client->dev;
	if (!bridge->encoder) {
		DRM_DEV_ERROR(dev, "Parent encoder object not found");
		return -ENODEV;
	}

	err = drm_connector_init(bridge->dev, &it6505->connector,
				 &it6505_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to initialize connector: %d", err);
		return err;
	}

	drm_connector_helper_add(&it6505->connector,
				 &it6505_connector_helper_funcs);

	it6505->connector.polled = DRM_CONNECTOR_POLL_HPD;

	err = drm_connector_attach_encoder(&it6505->connector, bridge->encoder);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to link up connector to encoder: %d",
			      err);
		goto cleanup_connector;
	}

	err = drm_connector_register(&it6505->connector);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to register connector: %d", err);
		goto cleanup_connector;
	}

	err = it6505_use_notifier_module(it6505);
	if (err < 0) {
		drm_connector_unregister(&it6505->connector);
		goto unregister_connector;
	}

	return 0;

unregister_connector:
	drm_connector_unregister(&it6505->connector);
cleanup_connector:
	drm_connector_cleanup(&it6505->connector);
	return err;
}

static void it6505_bridge_detach(struct drm_bridge *bridge)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);

	it6505_remove_notifier_module(it6505);
	drm_connector_unregister(&it6505->connector);
	drm_connector_cleanup(&it6505->connector);
}

static enum drm_mode_status
it6505_bridge_mode_valid(struct drm_bridge *bridge,
			 const struct drm_display_mode *mode)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);
	struct drm_display_mode *video = &it6505->video_info;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	/* Max 1200p at 5.4 Ghz, one lane */
	if (mode->clock > MAX_PIXEL_CLK)
		return MODE_CLOCK_HIGH;
	video->clock = mode->clock;

	return MODE_OK;
}

static int it6505_send_video_infoframe(struct it6505 *it6505,
				       struct hdmi_avi_infoframe *frame)
{
	u8 buffer[HDMI_INFOFRAME_HEADER_SIZE + HDMI_AVI_INFOFRAME_SIZE];
	int err;
	struct device *dev = &it6505->client->dev;

	err = hdmi_avi_infoframe_pack(frame, buffer, sizeof(buffer));
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to pack AVI infoframe: %d\n", err);
		return err;
	}

	err = dptx_set_bits(it6505, 0xE8, 0x01, 0x00);
	if (err)
		return err;

	err = regmap_bulk_write(it6505->regmap, 0xE9,
				buffer + HDMI_INFOFRAME_HEADER_SIZE,
				frame->length);
	if (err)
		return err;

	err = dptx_set_bits(it6505, 0xE8, 0x01, 0x01);
	if (err)
		return err;

	return 0;
}

static void it6505_bridge_mode_set(struct drm_bridge *bridge,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);
	struct hdmi_avi_infoframe frame;
	int err;
	struct device *dev = &it6505->client->dev;

	mutex_lock(&it6505->mode_lock);
	err = drm_hdmi_avi_infoframe_from_display_mode(&frame, adjusted_mode,
						       false);
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to setup AVI infoframe: %d", err);
		goto unlock;
	}

	it6505->video_info.base.id = adjusted_mode->base.id;
	strlcpy(it6505->video_info.name, adjusted_mode->name,
		DRM_DISPLAY_MODE_LEN);
	it6505->video_info.type = adjusted_mode->type;
	it6505->video_info.flags = adjusted_mode->flags;
	err = it6505_send_video_infoframe(it6505, &frame);
	if (err)
		DRM_DEV_ERROR(dev, "Failed to send AVI infoframe: %d", err);

unlock:
	mutex_unlock(&it6505->mode_lock);
}

static bool it6505_audio_input(struct it6505 *it6505)
{
	int regbe = dptx_read(it6505, 0xBE);

	return regbe != 0xFF;
}

static void dptx_set_audio_format(struct it6505 *it6505)
{
	unsigned int audio_source_count;

	/* I2S MODE */
	dptx_write(it6505, 0xB9,
	   (it6505->audio_word_length << 5) | (it6505->i2s_data_sequence << 4) |
		       (it6505->i2s_ws_channel << 3) |
		       (it6505->i2s_data_delay << 2) |
		       (it6505->i2s_justified << 1) | it6505->i2s_input_format);
	if (it6505->audio_select == SPDIF) {
		dptx_write(it6505, 0xBA, 0x00);
		/* 0x30 = 128*FS */
		dptx_set_bits(it6505, 0x11, 0xF0, 0x30);
	} else {
		dptx_write(it6505, 0xBA, 0xE4);
	}

	dptx_write(it6505, 0xBB, 0x20);
	dptx_write(it6505, 0xBC, 0x00);
	audio_source_count = BIT(DIV_ROUND_UP(it6505->audio_channel_count, 2))
				 - 1;


	audio_source_count |= it6505->audio_select << 4;

	dptx_write(it6505, 0xB8, audio_source_count);
}

static void dptx_set_audio_channel_status(struct it6505 *it6505)
{
	enum it6505_audio_sample_rate sample_rate = it6505->audio_sample_rate;
	u8 audio_word_length_map[] = {0x02, 0x04, 0x03, 0x0B};

	/* Channel Status */
	dptx_write(it6505, 0xBF, it6505->audio_type << 1);
	dptx_write(it6505, 0xC0, 0x00);
	dptx_write(it6505, 0xC1, 0x00);
	dptx_write(it6505, 0xC2, sample_rate);
	dptx_write(it6505, 0xC3, (~sample_rate << 4) |
		   audio_word_length_map[it6505->audio_word_length]);
}

static void dptx_set_audio_infoframe(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;
	u8 audio_info_ca[] = {0x00, 0x00, 0x01, 0x03, 0x07, 0x0B, 0x0F, 0x1F};

	dptx_write(it6505, 0xF7, it6505->audio_channel_count - 1);
	dptx_set_bits(it6505, 0xF8, 0x03, 0x00);

	if (it6505->audio_channel_count > 0 && it6505->audio_channel_count <= 8)
		dptx_write(it6505, 0xF9,
			   audio_info_ca[it6505->audio_channel_count - 1]);
	else
		DRM_DEV_ERROR(dev, "audio channel number error: %u",
			      it6505->audio_channel_count);
	/* Enable Audio InfoFrame */
	dptx_set_bits(it6505, 0xE8, 0x22, 0x22);
}

static void it6505_set_audio(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;
	int regbe;

	it6505_audio_disable(it6505);
	/* Audio Clock Domain Reset */
	dptx_set_bits(it6505, 0x05, 0x02, 0x02);
	/* Audio mute */
	dptx_set_bits(it6505, 0xD3, 0x20, 0x20);
	/* Release Audio Clock Domain Reset */
	dptx_set_bits(it6505, 0x05, 0x02, 0x00);

	if (it6505_audio_input(it6505)) {
		regbe = dptx_read(it6505, 0xBE);
		DRM_DEV_DEBUG_DRIVER(dev, "audio input fs: %d.%d kHz",
				     6750 / regbe, 67500 % regbe);
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "no audio input");
	}

	dptx_set_audio_channel_status(it6505);
	dptx_set_audio_format(it6505);
	dptx_set_audio_infoframe(it6505);

	/* Enable Enhanced Audio TimeStmp Mode */
	dptx_set_bits(it6505, 0xD4, 0x04, 0x04);
	/* Disable Full Audio Packet */
	dptx_set_bits(it6505, 0xBB, 0x10, 0x00);

	dptx_write(it6505, 0xDE, 0x00);
	dptx_write(it6505, 0xDF, 0x80);
	dptx_write(it6505, 0xE0, 0x00);
	dptx_set_bits(it6505, 0xB8, 0x80, 0x80);
	dptx_set_bits(it6505, 0xB8, 0x80, 0x00);
	dptx_set_bits(it6505, 0xD3, 0x20, 0x00);
	it6505->enable_audio = true;
}

static int __maybe_unused it6505_audio_set_hw_params(struct it6505 *it6505,
					struct hdmi_codec_params *params)
{
	unsigned int channel_count = params->cea.channels, i = 0;
	struct device *dev = &it6505->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "setting %d Hz, %d bit, %d channels\n",
			     params->sample_rate, params->sample_width,
			     channel_count);

	if (!it6505->bridge.encoder)
		return -ENODEV;

	while (i < ARRAY_SIZE(audio_sample_rate_map) &&
	       params->sample_rate !=
		       audio_sample_rate_map[i].sample_rate_value) {
		i++;
	}
	if (i == ARRAY_SIZE(audio_sample_rate_map)) {
		DRM_DEV_DEBUG_DRIVER(dev, "sample rate: %d Hz not support",
				     params->sample_rate);
		return -EINVAL;
	}
	it6505->audio_sample_rate = audio_sample_rate_map[i].rate;

	switch (params->sample_width) {
	case 16:
		it6505->audio_word_length = WORD_LENGTH_16BIT;
		break;
	case 18:
		it6505->audio_word_length = WORD_LENGTH_18BIT;
		break;
	case 20:
		it6505->audio_word_length = WORD_LENGTH_20BIT;
		break;
	case 24:
	case 32:
		it6505->audio_word_length = WORD_LENGTH_24BIT;
		break;
	default:
		DRM_DEV_DEBUG_DRIVER(dev, "wordlength: %d bit not support",
				     params->sample_width);
		return -EINVAL;
	}

	if (channel_count == 0 || channel_count % 2) {
		DRM_DEV_DEBUG_DRIVER(dev, "channel number: %d not support",
				     channel_count);
		return -EINVAL;
	}
	it6505->audio_channel_count = channel_count;
	return 0;
}

static void it6505_delayed_audio(struct work_struct *work)
{
	struct it6505 *it6505 = container_of(work, struct it6505,
					     delayed_audio.work);

	it6505_set_audio(it6505);
}

static int it6505_audio_hw_params(struct device *dev, void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return it6505_audio_set_hw_params(it6505, params);
}

static int it6505_audio_trigger(struct device *dev, int event)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	DRM_DEV_DEBUG_DRIVER(dev, "event: %d", event);
	switch (event) {
	case HDMI_CODEC_TRIGGER_EVENT_START:
	case HDMI_CODEC_TRIGGER_EVENT_RESUME:
		queue_delayed_work(system_wq, &it6505->delayed_audio,
				   msecs_to_jiffies(180));
		break;
	case HDMI_CODEC_TRIGGER_EVENT_STOP:
	case HDMI_CODEC_TRIGGER_EVENT_SUSPEND:
		cancel_delayed_work(&it6505->delayed_audio);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void it6505_audio_shutdown(struct device *dev, void *data)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	it6505_audio_disable(it6505);
}

static int it6505_audio_hook_plugged_cb(struct device *dev, void *data,
					hdmi_codec_plugged_cb fn,
					struct device *codec_dev)
{
	struct it6505 *it6505 = data;

	it6505->plugged_cb = fn;
	it6505->codec_dev = codec_dev;
	it6505_update_plugged_status(it6505, it6505->last_connector_status);
	return 0;
}

static const struct hdmi_codec_ops it6505_audio_codec_ops = {
	.hw_params = it6505_audio_hw_params,
	.trigger = it6505_audio_trigger,
	.audio_shutdown = it6505_audio_shutdown,
	.hook_plugged_cb = it6505_audio_hook_plugged_cb,
};

static int it6505_register_audio_driver(struct device *dev)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	struct hdmi_codec_pdata codec_data = {
		.ops = &it6505_audio_codec_ops,
		.max_i2s_channels = 8,
		.i2s = 1,
		.data = it6505,
	};
	struct platform_device *pdev;

	pdev = platform_device_register_data(dev, HDMI_CODEC_DRV_NAME,
					     PLATFORM_DEVID_AUTO, &codec_data,
					     sizeof(codec_data));
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	INIT_DELAYED_WORK(&it6505->delayed_audio, it6505_delayed_audio);
	it6505->last_connector_status = connector_status_disconnected;

	DRM_DEV_DEBUG_DRIVER(dev, "bound to %s", HDMI_CODEC_DRV_NAME);
	return 0;
}

/*
 * DPCD Read
 */

static int it6505_get_dpcd(struct it6505 *it6505, int offset, u8 *dpcd, int num)
{
	int ret;
	struct device *dev = &it6505->client->dev;

	ret = drm_dp_dpcd_read(&it6505->aux, offset, dpcd, num);

	if (ret < 0)
		return ret;

	DRM_DEV_DEBUG_DRIVER(dev, "ret = %d DPCD[0x%x] = %*ph", ret, offset,
			     num, dpcd);

	return 0;
}

static void it6505_parse_dpcd(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;
	int bcaps;
	struct it6505_drm_dp_link *link = &it6505->link;

	it6505_get_dpcd(it6505, DP_DPCD_REV, it6505->dpcd,
			ARRAY_SIZE(it6505->dpcd));

	DRM_DEV_DEBUG_DRIVER(dev, "#########DPCD Rev.: %d.%d###########",
			     link->revision >> 4, link->revision & 0x0F);

	DRM_DEV_DEBUG_DRIVER(dev, "Sink maximum link rate: %d.%d Gbps per lane",
			     link->rate / 100000, link->rate / 1000 % 100);

	switch (link->rate) {
	case 162000:
		it6505->hbr =
		min3(MAX_LINK_RATE, TRAINING_LINK_RATE, RBR) ? true : false;
		if (it6505->hbr)
			DRM_DEV_DEBUG_DRIVER(
				dev, "Not support HBR Mode, will train RBR");
		else
			DRM_DEV_DEBUG_DRIVER(dev, "Training RBR");
		break;

	case 270000:
		it6505->hbr =
		min3(MAX_LINK_RATE, TRAINING_LINK_RATE, HBR) ? true : false;
		if (!it6505->hbr)
			DRM_DEV_DEBUG_DRIVER(
				dev, "Support HBR mode, will train RBR");
		else
			DRM_DEV_DEBUG_DRIVER(dev, "Training HBR");
		break;

	case 540000:
		it6505->hbr =
		min3(MAX_LINK_RATE, TRAINING_LINK_RATE, HBR2) ? true : false;
		if (it6505->hbr)
			DRM_DEV_DEBUG_DRIVER(
				dev, "Support HBR2 mode, will train HBR");
		else
			DRM_DEV_DEBUG_DRIVER(dev, "Training RBR");
		break;

	case 810000:
		it6505->hbr =
		min3(MAX_LINK_RATE, TRAINING_LINK_RATE, HBR3) ? true : false;
		if (it6505->hbr)
			DRM_DEV_DEBUG_DRIVER(
				dev, "Support HBR3 mode, will train HBR");
		else
			DRM_DEV_DEBUG_DRIVER(dev, "Training RBR");
		break;
	default:
		DRM_DEV_ERROR(dev, "Unknown Maximum Link Rate: %u",
			      link->rate);
		break;
	}

	if (link->num_lanes == 1 || link->num_lanes == 2 ||
	    link->num_lanes == 4) {
		DRM_DEV_DEBUG_DRIVER(
			dev, "Lane Count: %u lane", link->num_lanes);
		it6505->lane_count = min3(MAX_LANE_COUNT, TRAINING_LANE_COUNT,
					  (int)link->num_lanes);
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Training %u lane", it6505->lane_count);
	} else {
		DRM_DEV_ERROR(dev, "Lane Count Error: %u", link->num_lanes);
	}

	if (link->capabilities)
		DRM_DEV_DEBUG_DRIVER(dev, "Support Enhanced Framing");
	else
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Can not support Enhanced Framing Mode");

	if (it6505->dpcd[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5) {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Down-Spread: 0.5, support SSC!");
	} else {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Down-Spread: 0, No support SSC!");
		it6505->enable_ssc = false;
	}

	if (link->revision >= 0x11 &&
	    it6505->dpcd[DP_MAX_DOWNSPREAD] & DP_NO_AUX_HANDSHAKE_LINK_TRAINING)
		DRM_DEV_DEBUG_DRIVER(dev, "Support No AUX Training");
	else
		DRM_DEV_DEBUG_DRIVER(dev, "Can not support No AUX Training");

	bcaps = dpcd_read(it6505, DP_AUX_HDCP_BCAPS);
	if (bcaps & DP_BCAPS_HDCP_CAPABLE) {
		DRM_DEV_DEBUG_DRIVER(dev, "Sink support HDCP!");
		it6505->cp_capable = true;
		DRM_DEV_DEBUG_DRIVER(dev, "Downstream is %s!",
		bcaps & DP_BCAPS_REPEATER_PRESENT ? "repeater" : "receiver");
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "Sink not support HDCP!");
		it6505->cp_capable = false;
		it6505->enable_hdcp = false;
	}
}

static void it6505_enable_hdcp(struct it6505 *it6505)
{
	u8 bksvs[5], c;
	struct device *dev = &it6505->client->dev;

	/* Disable CP_Desired */
	dptx_set_bits(it6505, 0x38, 0x0B, 0x00);
	dptx_set_bits(it6505, 0x05, 0x10, 0x10);

	usleep_range(1000, 1500);
	c = dpcd_read(it6505, DP_AUX_HDCP_BCAPS);
	DRM_DEV_DEBUG_DRIVER(dev, "DPCD[0x68028]: 0x%x\n", c);
	if (!c)
		return;

	dptx_set_bits(it6505, 0x05, 0x10, 0x00);
	/* Disable CP_Desired */
	dptx_set_bits(it6505, 0x38, 0x01, 0x00);
	/* Use R0' 100ms waiting */
	dptx_set_bits(it6505, 0x38, 0x08, 0x00);
	/* clear the repeater List Chk Done and fail bit */
	dptx_set_bits(it6505, 0x39, 0x30, 0x00);

	it6505_get_dpcd(it6505, DP_AUX_HDCP_BKSV, bksvs, ARRAY_SIZE(bksvs));

	DRM_DEV_DEBUG_DRIVER(dev, "Sink BKSV = %5ph", bksvs);

	/* Select An Generator */
	dptx_set_bits(it6505, 0x3A, 0x01, 0x01);
	/* Enable An Generator */
	dptx_set_bits(it6505, 0x3A, 0x02, 0x02);
	/* delay1ms(10);*/
	usleep_range(10000, 15000);
	/* Stop An Generator */
	dptx_set_bits(it6505, 0x3A, 0x02, 0x00);

	dptx_set_bits(it6505, 0x38, 0x01, 0x01);
	dptx_set_bits(it6505, 0x39, 0x01, 0x01);
}

static void it6505_lanespeed_setup(struct it6505 *it6505)
{
	if (!it6505->hbr) {
		dptx_set_bits(it6505, 0x16, 0x01, 0x01);
		dptx_set_bits(it6505, 0x5C, 0x02, 0x00);
	} else {
		dptx_set_bits(it6505, 0x16, 0x01, 0x00);
		dptx_set_bits(it6505, 0x5C, 0x02, 0x02);
	}
}

static void it6505_lane_swap(struct it6505 *it6505)
{
	int err;
	union extcon_property_value property;
	struct device *dev = &it6505->client->dev;

	if (!it6505->lane_swap_disabled) {
		err = extcon_get_property(it6505->extcon, EXTCON_DISP_DP,
					  EXTCON_PROP_USB_TYPEC_POLARITY,
					  &property);
		if (err) {
			DRM_DEV_ERROR(dev, "get property fail!");
			return;
		}
		it6505->lane_swap = property.intval;
	}

	dptx_set_bits(it6505, 0x16, 0x08, it6505->lane_swap ? 0x08 : 0x00);
	dptx_set_bits(it6505, 0x16, 0x06, (it6505->lane_count - 1) << 1);
	DRM_DEV_DEBUG_DRIVER(dev, "it6505 lane_swap = 0x%x", it6505->lane_swap);
	it6505_lane_power_on(it6505);
	it6505_lane_termination_on(it6505);
}

static void it6505_lane_config(struct it6505 *it6505)
{
	it6505_lanespeed_setup(it6505);
	it6505_lane_swap(it6505);
}

static void it6505_set_ssc(struct it6505 *it6505)
{
	dptx_set_bits(it6505, 0x16, 0x10, it6505->enable_ssc << 4);
	if (it6505->enable_ssc) {
		dptx_select_bank(it6505, 1);
		dptx_write(it6505, 0x88, 0x9E);
		dptx_write(it6505, 0x89, 0x1C);
		dptx_write(it6505, 0x8A, 0x42);
		dptx_select_bank(it6505, 0);
		dptx_write(it6505, 0x58, 0x07);
		dptx_write(it6505, 0x59, 0x29);
		dptx_write(it6505, 0x5A, 0x03);
		/* Stamp Interrupt Step */
		dptx_set_bits(it6505, 0xD4, 0x30, 0x10);
		dpcd_write(it6505, DP_DOWNSPREAD_CTRL, DP_SPREAD_AMP_0_5);
	} else {
		dpcd_write(it6505, DP_DOWNSPREAD_CTRL, 0x00);
		dptx_set_bits(it6505, 0xD4, 0x30, 0x00);
	}
}

static void it6505_pixel_clk_phase_adjustment(struct it6505 *it6505)
{
	struct drm_display_mode *vid = &it6505->video_info;

	dptx_set_bits(it6505, 0x10, 0x03,
		vid->clock < ADJUST_PHASE_THRESHOLD ? PIXEL_CLK_DELAY : 0);
	dptx_set_bits(it6505, 0x12, 0x10, PIXEL_CLK_INVERSE << 4);
}

static void it6505_set_afe_driving(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;
	u32 afe_setting;

	if (device_property_read_u32(dev, "afe-setting", &afe_setting) < 0)
		afe_setting = 0;
	if (afe_setting >= ARRAY_SIZE(afe_setting_table)) {
		DRM_DEV_ERROR(dev, "afe setting value error and use default");
		afe_setting = 0;
	}
	if (afe_setting) {
		dptx_select_bank(it6505, 1);
		dptx_write(it6505, 0x7E, afe_setting_table[afe_setting][0]);
		dptx_write(it6505, 0x7F, afe_setting_table[afe_setting][1]);
		dptx_write(it6505, 0x81, afe_setting_table[afe_setting][2]);
		dptx_select_bank(it6505, 0);
	}
}

static void dptx_output_config(struct it6505 *it6505)
{
	/* change bank 0 */
	dptx_select_bank(it6505, 0);
	/* config CSC mapping table into RGB-to-YCbCr */
	dptx_write(it6505, 0x64, 0x10);
	dptx_write(it6505, 0x65, 0x80);
	dptx_write(it6505, 0x66, 0x10);
	dptx_write(it6505, 0x67, 0x4F);
	dptx_write(it6505, 0x68, 0x09);
	dptx_write(it6505, 0x69, 0xBA);
	dptx_write(it6505, 0x6A, 0x3B);
	dptx_write(it6505, 0x6B, 0x4B);
	dptx_write(it6505, 0x6C, 0x3E);
	dptx_write(it6505, 0x6D, 0x4F);
	dptx_write(it6505, 0x6E, 0x09);
	dptx_write(it6505, 0x6F, 0x56);
	dptx_write(it6505, 0x70, 0x0E);
	dptx_write(it6505, 0x71, 0x00);
	dptx_write(it6505, 0x72, 0x00);
	dptx_write(it6505, 0x73, 0x4F);
	dptx_write(it6505, 0x74, 0x09);
	dptx_write(it6505, 0x75, 0x00);
	dptx_write(it6505, 0x76, 0x00);
	dptx_write(it6505, 0x77, 0xE7);
	dptx_write(it6505, 0x78, 0x10);
	/* initial HPD Timer event setting */
	dptx_write(it6505, 0xE8, 0x00);
	dptx_set_bits(it6505, 0xCE, 0x70, 0x60);
	dptx_write(it6505, 0xCA, 0x4D);
	dptx_write(it6505, 0xC9, 0xF5);
	dptx_write(it6505, 0x5C, 0x02);

	it6505_drm_dp_link_power_up(&it6505->aux, &it6505->link);
	dptx_set_bits(it6505, 0x59, 0x01, 0x01);
	dptx_set_bits(it6505, 0x5A, 0x05, 0x01);
	dptx_write(it6505, 0x12, 0x01);
	dptx_write(it6505, 0xCB, 0x17);
	dptx_write(it6505, 0x11, 0x09);
	dptx_write(it6505, 0x20, 0x28);
	dptx_set_bits(it6505, 0x23, 0x30, 0x00);
	dptx_set_bits(it6505, 0x3A, 0x04, 0x04);
	dptx_set_bits(it6505, 0x15, 0x01, 0x01);
	dptx_write(it6505, 0x0C, 0x08);

	dptx_set_bits(it6505, 0x5F, 0x20, 0x00);
	it6505_lane_config(it6505);

	it6505_set_ssc(it6505);

	if (it6505->link.capabilities) {
		dptx_write(it6505, 0xD3, 0x33);
		dpcd_write(it6505, DP_LANE_COUNT_SET,
			  it6505->lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN);
	} else {
		dptx_write(it6505, 0xD3, 0x32);
	}

	/* it6505 displayport phy and logic reset */
	dptx_set_bits(it6505, 0x15, 0x02, 0x02);
	dptx_set_bits(it6505, 0x15, 0x02, 0x00);
	dptx_set_bits(it6505, 0x05, 0x03, 0x00);

	/* reg60[2] = Input DDR */
	dptx_write(it6505, 0x60, 0x44);
	/* M444B24 format */
	dptx_write(it6505, 0x62, 0x01);
	/* select RGB Bypass CSC */
	dptx_write(it6505, 0x63, 0x00);

	it6505_pixel_clk_phase_adjustment(it6505);
	dptx_set_bits(it6505, 0x61, 0x01, 0x01);
	dptx_write(it6505, 0x06, 0xFF);
	dptx_write(it6505, 0x07, 0xFF);
	dptx_write(it6505, 0x08, 0xFF);

	dptx_set_bits(it6505, 0xD3, 0x10, 0x00);
	dptx_set_bits(it6505, 0xD4, 0x41, 0x41);
	dptx_set_bits(it6505, 0xE8, 0x11, 0x11);

	it6505_set_afe_driving(it6505);
}

static void it6505_start_auto_train(struct it6505 *it6505)
{
	/* reset auto DP link training */
	dptx_write(it6505, 0x17, 0x04);
	/* start auto DP link training */
	dptx_write(it6505, 0x17, 0x01);
}

static u8 dp_link_status(const u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}

static bool it6505_use_step_train_check(struct it6505 *it6505)
{
	if (it6505->link.revision >= 0x12)
		return it6505->dpcd[DP_TRAINING_AUX_RD_INTERVAL] >= 0x01;

	return true;
}

static bool it6505_check_max_value(u8 lane_voltage_swing_pre_emphasis)
{
	return ((lane_voltage_swing_pre_emphasis & 0x03) == 0x03);
}

static bool it6505_check_max_voltage_swing_reached(u8 *lane_voltage_swing,
						   u8 lane_count)
{
	u8 i;

	for (i = 0; i < lane_count; i++) {
		if (lane_voltage_swing[i] & DP_TRAIN_MAX_SWING_REACHED)
			return true;
	}

	return false;
}

static bool it6505_step_train_lane_voltage_pre_emphasis_set(
	struct it6505 *it6505,
	struct it6505_lane_voltage_pre_emphasis *lane_voltage_pre_emphasis,
	u8 *lane_voltage_pre_emphasis_set)
{
	u8 i;

	for (i = 0; i < it6505->lane_count; i++) {
		lane_voltage_pre_emphasis->voltage_swing[i] &= 0x03;
		lane_voltage_pre_emphasis_set[i] =
			lane_voltage_pre_emphasis->voltage_swing[i];
		if (it6505_check_max_value(
			    lane_voltage_pre_emphasis->voltage_swing[i]))
			lane_voltage_pre_emphasis_set[i] |=
				DP_TRAIN_MAX_SWING_REACHED;

		lane_voltage_pre_emphasis->pre_emphasis[i] &= 0x03;
		lane_voltage_pre_emphasis_set[i] |=
			lane_voltage_pre_emphasis->pre_emphasis[i]
			<< DP_TRAIN_PRE_EMPHASIS_SHIFT;
		if (it6505_check_max_value(
			    lane_voltage_pre_emphasis->pre_emphasis[i]))
			lane_voltage_pre_emphasis_set[i] |=
				DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
		dpcd_write(it6505, DP_TRAINING_LANE0_SET + i,
			   lane_voltage_pre_emphasis_set[i]);

		if (lane_voltage_pre_emphasis_set[i] !=
		    dpcd_read(it6505, DP_TRAINING_LANE0_SET + i))
			return false;
	}

	return true;
}

static bool it6505_step_cr_train(
	struct it6505 *it6505,
	struct it6505_lane_voltage_pre_emphasis *lane_voltage_pre_emphasis)
{
	u8 loop_count = 0, i = 0, j, voltage_swing_adjust = -1;
	u8 link_status[DP_LINK_STATUS_SIZE] = { 0 }, pre_emphasis_adjust = -1;
	u8 lane_level_config[MAX_LANE_COUNT] = { 0 };

	it6505_drm_dp_link_configure(it6505);
	dpcd_write(it6505, DP_DOWNSPREAD_CTRL,
		   it6505->enable_ssc ? DP_SPREAD_AMP_0_5 : 0 << 4);
	dpcd_write(it6505, DP_TRAINING_PATTERN_SET, DP_TRAINING_PATTERN_1);

	while (loop_count < 5 && i < 10) {
		i++;
		if (!it6505_step_train_lane_voltage_pre_emphasis_set(
			    it6505, lane_voltage_pre_emphasis,
			    lane_level_config))
			continue;
		drm_dp_link_train_clock_recovery_delay(it6505->dpcd);
		drm_dp_dpcd_read_link_status(&it6505->aux, link_status);

		if (drm_dp_clock_recovery_ok(link_status, it6505->lane_count)) {
			dptx_set_bits(it6505, 0x16, BIT(5), BIT(5));
			return true;
		}
		DRM_DEV_DEBUG_DRIVER(&it6505->client->dev, "cr not done");

		if (it6505_check_max_voltage_swing_reached(lane_level_config,
							   it6505->lane_count))
			goto cr_train_fail;

		for (j = 0; j < it6505->lane_count; j++) {
			lane_voltage_pre_emphasis->voltage_swing[j] =
				drm_dp_get_adjust_request_voltage(link_status,
								  j) >>
				DP_TRAIN_VOLTAGE_SWING_SHIFT;
			lane_voltage_pre_emphasis->pre_emphasis[j] =
				drm_dp_get_adjust_request_pre_emphasis(
					link_status, j) >>
				DP_TRAIN_PRE_EMPHASIS_SHIFT;
			if ((voltage_swing_adjust ==
			     lane_voltage_pre_emphasis->voltage_swing[j]) &&
			    (pre_emphasis_adjust ==
			     lane_voltage_pre_emphasis->pre_emphasis[j])) {
				loop_count++;
				continue;
			}

			voltage_swing_adjust =
				lane_voltage_pre_emphasis->voltage_swing[j];
			pre_emphasis_adjust =
				lane_voltage_pre_emphasis->pre_emphasis[j];
			loop_count = 0;

			if (voltage_swing_adjust + pre_emphasis_adjust > 0x03)
				lane_voltage_pre_emphasis->voltage_swing[j] =
					0x03 - lane_voltage_pre_emphasis
						       ->pre_emphasis[j];
		}
	}

cr_train_fail:
	dpcd_write(it6505, DP_TRAINING_PATTERN_SET,
		   DP_TRAINING_PATTERN_DISABLE);

	return false;
}

static bool it6505_step_eq_train(
	struct it6505 *it6505,
	struct it6505_lane_voltage_pre_emphasis *lane_voltage_pre_emphasis)
{
	u8 loop_count = 0, i, link_status[DP_LINK_STATUS_SIZE] = { 0 };
	u8 lane_level_config[MAX_LANE_COUNT] = { 0 };

	dpcd_write(it6505, DP_TRAINING_PATTERN_SET, DP_TRAINING_PATTERN_2);

	while (loop_count < 6) {
		loop_count++;

		if (!it6505_step_train_lane_voltage_pre_emphasis_set(
			    it6505, lane_voltage_pre_emphasis,
			    lane_level_config))
			continue;

		drm_dp_link_train_channel_eq_delay(it6505->dpcd);
		drm_dp_dpcd_read_link_status(&it6505->aux, link_status);

		if (!drm_dp_clock_recovery_ok(link_status, it6505->lane_count))
			goto eq_train_fail;

		if (drm_dp_channel_eq_ok(link_status, it6505->lane_count)) {
			dpcd_write(it6505, DP_TRAINING_PATTERN_SET,
				   DP_TRAINING_PATTERN_DISABLE);
			dptx_set_bits(it6505, 0x16, BIT(6), BIT(6));
			return true;
		}
		DRM_DEV_DEBUG_DRIVER(&it6505->client->dev, "eq not done");

		for (i = 0; i < it6505->lane_count; i++) {
			lane_voltage_pre_emphasis->voltage_swing[i] =
				drm_dp_get_adjust_request_voltage(link_status,
								  i) >>
				DP_TRAIN_VOLTAGE_SWING_SHIFT;
			lane_voltage_pre_emphasis->pre_emphasis[i] =
				drm_dp_get_adjust_request_pre_emphasis(
					link_status, i) >>
				DP_TRAIN_PRE_EMPHASIS_SHIFT;

			if (lane_voltage_pre_emphasis->voltage_swing[i] +
				    lane_voltage_pre_emphasis->pre_emphasis[i] >
			    0x03)
				lane_voltage_pre_emphasis->voltage_swing[i] =
					0x03 - lane_voltage_pre_emphasis
						       ->pre_emphasis[i];
		}
	}

eq_train_fail:
	dpcd_write(it6505, DP_TRAINING_PATTERN_SET,
		   DP_TRAINING_PATTERN_DISABLE);
	return false;
}

static bool it6505_step_train(struct it6505 *it6505)
{
	struct it6505_lane_voltage_pre_emphasis lane_voltage_pre_emphasis = {
		.voltage_swing = { 0 },
		.pre_emphasis = { 0 },
	};

	if (!it6505_step_cr_train(it6505, &lane_voltage_pre_emphasis))
		return false;
	if (!it6505_step_eq_train(it6505, &lane_voltage_pre_emphasis))
		return false;
	return true;
}

static void dptx_process_sys_wait(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;

	dptx_debug_print(it6505, 0x9F, "");

	if (it6505->train_pass) {
		DRM_DEV_DEBUG_DRIVER(dev, "%s Training Success",
				     it6505->enable_auto_train ? "Auto" :
								 "Step");
		if (it6505->enable_auto_train)
			dptx_debug_print(it6505, 0x0E, "Auto training state:");

		if (it6505_audio_input(it6505)) {
			DRM_DEV_DEBUG_DRIVER(dev, "Enable audio!");
			it6505_set_audio(it6505);
		}
		it6505->enable_auto_train = true;

		if (it6505->hdcp_flag) {
			DRM_DEV_DEBUG_DRIVER(dev, "Enable HDCP");
			dptx_sys_chg(it6505, SYS_HDCP);
		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Disable HDCP");
			dptx_sys_chg(it6505, SYS_NOROP);
		}
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "%s Training fail %d times",
				     it6505->enable_auto_train ? "Auto" :
								 "Step",
				     it6505->train_count);
		dptx_debug_print(it6505, 0x0D, "system state");
		it6505->train_count--;
		if (it6505->train_count > 0) {
			dptx_sys_chg(it6505, SYS_TRAIN);
		} else {
			DRM_DEV_DEBUG_DRIVER(
				dev, "%s Training Fail total %d times",
				it6505->enable_auto_train ? "Auto" : "Step",
				!!it6505->dpcd_sink_count ? 3 : 1);
			DRM_DEV_DEBUG_DRIVER(dev, "Sys change to SYS_HPD");
			if (it6505->dpcd_sink_count != 0 &&
			    it6505->train_fail_hpd > 0) {
				if (it6505_use_step_train_check(it6505))
					it6505->enable_auto_train = false;
				it6505->train_fail_hpd--;
				dptx_sys_chg(it6505, SYS_HPD);
			} else {
				dptx_sys_chg(it6505, SYS_TRAINFAIL);
			}
		}
	}
}

static void dptx_sys_fsm(struct it6505 *it6505)
{
	unsigned int i;
	int reg0e, ret;
	struct device *dev = &it6505->client->dev;

	if (it6505->state != SYS_UNPLUG && !dptx_get_sink_hpd(it6505))
		dptx_sys_chg(it6505, SYS_UNPLUG);

	DRM_DEV_DEBUG_DRIVER(dev, "state: %s", state_string[it6505->state]);
	switch (it6505->state) {
	case SYS_UNPLUG:
		drm_helper_hpd_irq_event(it6505->connector.dev);
		break;

	case SYS_HPD:
		for (i = 0; i < 5; i++) {
			dptx_debug_print(it6505, 0x9F, "sys_fsm:");
			ret = it6505_drm_dp_link_probe(&it6505->aux,
						       &it6505->link);
			if (ret == 0)
				break;
			it6505_aux_reset(it6505);
			msleep(20);
		}
		if (i >= 5) {
			DRM_DEV_DEBUG_DRIVER(dev, "link probe fail");
			return;
		}
		it6505_drm_dp_link_power_up(&it6505->aux, &it6505->link);
		dpcd_debug_print(it6505, DP_SET_POWER, "sys_fsm:");
		it6505->dpcd_sink_count = dpcd_read(it6505, DP_SINK_COUNT);
		it6505->dpcd_sink_count &= 0x3F;
		DRM_DEV_DEBUG_DRIVER(dev, "sink_count:%d",
				     it6505->dpcd_sink_count);
		if (mutex_is_locked(&it6505->mode_lock))
			goto sleep;

		drm_helper_hpd_irq_event(it6505->connector.dev);

		if (it6505->dpcd_sink_count) {
sleep:
			ret = wait_event_interruptible_timeout(
				it6505->edid_wait,
				it6505->edid != NULL,
				msecs_to_jiffies(7000));
			if (!ret)
				DRM_DEV_DEBUG_DRIVER(dev, "get edid timeout");

		}

		it6505_init_variable(it6505);
		/* GETDPCD */
		it6505_parse_dpcd(it6505);

		/*
		 * training fail 3 times,
		 * then change to HPD to restart
		 */
		it6505->train_count = !!it6505->dpcd_sink_count ? 3 : 1;
		DRM_DEV_DEBUG_DRIVER(dev, "will Train %s, %d lanes",
				     it6505->hbr ? "HBR" : "RBR",
				     it6505->lane_count);
		it6505_disable_hdcp(it6505);
		it6505_audio_disable(it6505);
		dptx_output_config(it6505);
		dptx_sys_chg(it6505, SYS_TRAIN);
		break;

	case SYS_TRAIN:
		if (it6505->enable_auto_train) {
			if (it6505->dpcd_sink_count != 0)
				dptx_set_bits(it6505, 0x16, 0x40, 0x00);
			it6505_start_auto_train(it6505);
			/*
			 * waiting for training down flag
			 * because we don't know
			 * how long this step will be completed
			 * so use step 1ms to wait
			 */
			for (i = 0; i < 200; i++) {
				usleep_range(1000, 2000);
				reg0e = dptx_read(it6505, 0x0E);
				if (reg0e & BIT(4)) {
					it6505->train_pass = true;
					break;
				}
			}
		} else {
			dptx_set_bits(it6505, 0x16, 0x60, 0x00);
			dpcd_write(it6505, DP_TRAINING_PATTERN_SET,
				   DP_TRAINING_PATTERN_DISABLE);
			if (it6505_step_train(it6505))
				it6505->train_pass = true;
		}

		dptx_sys_chg(it6505, SYS_WAIT);
		break;

	case SYS_WAIT:
		dptx_process_sys_wait(it6505);
		break;

	case SYS_HDCP:
		msleep(2400);
		it6505_enable_hdcp(it6505);
		break;

	case SYS_NOROP:
		break;

	case SYS_TRAINFAIL:
		break;

	default:
		DRM_DEV_ERROR(dev, "sys_state change to unknown_state: %d",
			      it6505->state);
		break;
	}
}

static int sha1_digest(struct it6505 *it6505, u8 *sha1_input, unsigned int size,
		       u8 *output_av)
{
	struct shash_desc *desc;
	struct crypto_shash *tfm;
	int err;
	struct device *dev = &it6505->client->dev;

	tfm = crypto_alloc_shash("sha1", 0, 0);
	if (IS_ERR(tfm)) {
		DRM_DEV_ERROR(dev, "crypto_alloc_shash sha1 failed");
		return PTR_ERR(tfm);
	}
	desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (!desc) {
		crypto_free_shash(tfm);
		return -ENOMEM;
	}

	desc->tfm = tfm;
	err = crypto_shash_digest(desc, sha1_input, size, output_av);
	if (err)
		DRM_DEV_ERROR(dev, "crypto_shash_digest sha1 failed");

	crypto_free_shash(tfm);
	kfree(desc);
	return err;
}

static int it6505_makeup_sha1_input(struct it6505 *it6505)
{
	int msgcnt = 0, i;
	u8 am0[8], binfo[2];
	struct device *dev = &it6505->client->dev;

	dptx_set_bits(it6505, 0x3A, 0x20, 0x20);
	for (i = 0; i < 8; i++)
		am0[i] = dptx_read(it6505, 0x4C + i);

	DRM_DEV_DEBUG_DRIVER(dev, "read am0 = %8ph", am0);
	dptx_set_bits(it6505, 0x3A, 0x20, 0x00);

	it6505_get_dpcd(it6505, DP_AUX_HDCP_BINFO, binfo, ARRAY_SIZE(binfo));
	DRM_DEV_DEBUG_DRIVER(dev, "Attached devices: %02x", binfo[0] & 0x7F);

	DRM_DEV_DEBUG_DRIVER(dev, "%s 127 devices are attached",
			     (binfo[0] & BIT(7)) ? "over" : "under");
	DRM_DEV_DEBUG_DRIVER(dev, "depth, attached levels: %02x",
			     binfo[1] & 0x07);
	DRM_DEV_DEBUG_DRIVER(dev, "%s than seven levels cascaded",
			     (binfo[1] & BIT(3)) ? "more" : "less");

	for (i = 0; i < (binfo[0] & 0x7F); i++) {
		it6505_get_dpcd(it6505, DP_AUX_HDCP_KSV_FIFO + (i % 3) * 5,
				it6505->sha1_input + msgcnt, 5);
		msgcnt += 5;

		DRM_DEV_DEBUG_DRIVER(dev, "KSV List %d device : %5ph", i,
				     it6505->sha1_input + i * 5);
	}
	it6505->sha1_input[msgcnt++] = binfo[0];
	it6505->sha1_input[msgcnt++] = binfo[1];
	for (i = 0; i < 8; i++)
		it6505->sha1_input[msgcnt++] = am0[i];

	DRM_DEV_DEBUG_DRIVER(dev, "SHA Message Count = %d", msgcnt);
	return msgcnt;
}

static void it6505_check_sha1_result(struct it6505 *it6505)
{
	unsigned int i, sha_pass;
	struct device *dev = &it6505->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "SHA calculate complete");
	for (i = 0; i < 5; i++)
		DRM_DEV_DEBUG_DRIVER(dev, "av %d: %4ph", i, it6505->av[i]);

	sha_pass = 1;
	for (i = 0; i < 5; i++) {
		it6505_get_dpcd(it6505, DP_AUX_HDCP_V_PRIME(i), it6505->bv[i],
				4);
		DRM_DEV_DEBUG_DRIVER(dev, "bv %d: %4ph", i, it6505->bv[i]);
		if ((it6505->bv[i][3] != it6505->av[i][0]) ||
		    (it6505->bv[i][2] != it6505->av[i][1]) ||
		    (it6505->bv[i][1] != it6505->av[i][2]) ||
		    (it6505->bv[i][0] != it6505->av[i][3])) {
			sha_pass = 0;
		}
	}
	if (sha_pass) {
		DRM_DEV_DEBUG_DRIVER(dev, "SHA check result pass!");
		dptx_set_bits(it6505, 0x39, BIT(4), BIT(4));
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "SHA check result fail");
		dptx_set_bits(it6505, 0x39, BIT(5), BIT(5));
	}
}

static void to_fsm_state(struct it6505 *it6505, enum it6505_sys_state state)
{
	while (it6505->state != state && it6505->state != SYS_TRAINFAIL &&
	       it6505->state != SYS_NOROP) {
		dptx_sys_fsm(it6505);
		if (it6505->state == SYS_UNPLUG)
			return;
	}
}

static void go_on_fsm(struct it6505 *it6505);

static void hpd_irq(struct it6505 *it6505)
{
	u8 dpcd_sink_count, dpcd_irq_vector;
	u8 link_status[DP_LINK_STATUS_SIZE];
	unsigned int bstatus;
	struct device *dev = &it6505->client->dev;

	/*
	 * Some dongle may assert HPD_IRQ and delay several ms
	 * to pull down HPD signal when unplug.
	 */
	msleep(500);
	if (!dptx_get_sink_hpd(it6505)) {
		DRM_DEV_DEBUG_DRIVER(dev, "HPD_IRQ HPD low");
		return;
	}
	dpcd_sink_count = dpcd_read(it6505, DP_SINK_COUNT);
	dpcd_sink_count &= 0x3F;
	dpcd_irq_vector = dpcd_read(it6505, DP_DEVICE_SERVICE_IRQ_VECTOR);
	drm_dp_dpcd_read_link_status(&it6505->aux, link_status);

	DRM_DEV_DEBUG_DRIVER(dev, "dpcd_sink_count = 0x%x", dpcd_sink_count);
	DRM_DEV_DEBUG_DRIVER(dev, "dpcd_irq_vector = 0x%x", dpcd_irq_vector);
	DRM_DEV_DEBUG_DRIVER(dev, "link_status = %*ph",
			     (int)ARRAY_SIZE(link_status), link_status);
	DRM_DEV_DEBUG_DRIVER(dev, "it6505->dpcd_sink_count:%d, sink_hpd:%d",
			     it6505->dpcd_sink_count,
			     dptx_get_sink_hpd(it6505));
	if (dpcd_sink_count != it6505->dpcd_sink_count) {
		it6505->dpcd_sink_count = dpcd_sink_count;
		it6505_init_and_mask_on(it6505);
		return;
	}

	if (dpcd_irq_vector & DP_CP_IRQ) {
		bstatus = dpcd_read(it6505, DP_AUX_HDCP_BSTATUS);
		dptx_set_bits(it6505, 0x39, 0x02, 0x02);
		DRM_DEV_DEBUG_DRIVER(dev, "Receive CP_IRQ, bstatus = 0x%02x",
				     bstatus);
		dptx_debug_print(it6505, 0x55, "");

		if (bstatus & DP_BSTATUS_READY)
			DRM_DEV_DEBUG_DRIVER(dev, "HDCP KSV list ready");

		if (bstatus & DP_BSTATUS_LINK_FAILURE) {
			DRM_DEV_DEBUG_DRIVER(
				dev, "Link Integrity Fail, restart HDCP");
			return;
		}
		if (bstatus & DP_BSTATUS_R0_PRIME_READY) {
			DRM_DEV_DEBUG_DRIVER(dev, "HDCP R0' ready");
			go_on_fsm(it6505);
		}
	}

	if (dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED) &
	    DP_LINK_STATUS_UPDATED) {
		if (!drm_dp_channel_eq_ok(link_status,
					  it6505->lane_count)) {
			DRM_DEV_DEBUG_DRIVER(dev, "Link Re-Training");
			dptx_set_bits(it6505, 0xD3, 0x30, 0x30);
			dptx_set_bits(it6505, 0xE8, 0x33, 0x00);
			msleep(500);
			dptx_sys_chg(it6505, SYS_HPD);
			go_on_fsm(it6505);
		}
	}
}

static void go_on_fsm(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;

	if (it6505->dpcd_sink_count && it6505->cp_capable) {
		DRM_DEV_DEBUG_DRIVER(dev, "Support HDCP, cp_capable: %d",
				     it6505->cp_capable);
		if (it6505->enable_hdcp) {
			DRM_DEV_DEBUG_DRIVER(dev, "Enable HDCP");
			it6505->hdcp_flag = true;
			to_fsm_state(it6505, SYS_HDCP);
			dptx_sys_fsm(it6505);

		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Disable HDCP");
			it6505->hdcp_flag = false;
			to_fsm_state(it6505, SYS_NOROP);
		}
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "Not support HDCP");
		it6505->hdcp_flag = false;
		to_fsm_state(it6505, SYS_NOROP);
	}
}

static void it6505_check_reg06(struct it6505 *it6505, unsigned int reg06)
{
	unsigned int rddata;
	struct device *dev = &it6505->client->dev;

	if (reg06 & BIT(0)) {
		/* hpd pin state change */
		DRM_DEV_DEBUG_DRIVER(dev, "HPD Change Interrupt");
		if (dptx_get_sink_hpd(it6505)) {
			dptx_sys_chg(it6505, SYS_HPD);
			dptx_sys_fsm(it6505);

			if (it6505->edid) {
				rddata = dptx_read(it6505, 0x0D);
				if (rddata & BIT(2)) {
					go_on_fsm(it6505);
				} else {
					dptx_set_bits(it6505, 0x05, 0x01, 0x00);
					dptx_set_bits(it6505, 0x61, 0x02, 0x02);
					dptx_set_bits(it6505, 0x61, 0x02, 0x00);
				}
			} else {
				go_on_fsm(it6505);
			}
		} else {
			dptx_sys_chg(it6505, SYS_UNPLUG);
			dptx_sys_fsm(it6505);
			return;
		}
	}

	if (it6505->cp_capable) {
		if (reg06 & BIT(4)) {
			DRM_DEV_DEBUG_DRIVER(dev,
					     "HDCP encryption Done Interrupt");
			dptx_sys_chg(it6505, SYS_NOROP);
		}

		if (reg06 & BIT(3)) {
			DRM_DEV_DEBUG_DRIVER(
				dev, "HDCP encryption Fail Interrupt, retry");
			it6505_init_and_mask_on(it6505);
		}
	}

	if (reg06 & BIT(1)) {
		DRM_DEV_DEBUG_DRIVER(dev, "HPD IRQ Interrupt");
		hpd_irq(it6505);
	}

	if (reg06 & BIT(2)) {
		rddata = dptx_read(it6505, 0x0D);

		if (rddata & BIT(2)) {
			DRM_DEV_DEBUG_DRIVER(dev, "Video Stable On Interrupt");
			if (it6505->state == SYS_HPD)
				dptx_sys_chg(it6505, SYS_TRAIN);
			go_on_fsm(it6505);

		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Video Stable Off Interrupt");
		}
	}
}

static void it6505_check_reg07(struct it6505 *it6505, unsigned int reg07)
{
	struct device *dev = &it6505->client->dev;
	unsigned int len, i;
	unsigned int bstatus;

	if (it6505->state == SYS_UNPLUG)
		return;
	if (reg07 & BIT(0)) {
		DRM_DEV_DEBUG_DRIVER(dev, "AUX PC Request Fail Interrupt");
		dptx_debug_print(it6505, 0x9F, "AUX status:");
		it6505_aux_reset(it6505);
	}

	if (reg07 & BIT(1)) {
		DRM_DEV_DEBUG_DRIVER(dev, "HDCP event Interrupt");
		for (i = 0; i < 500; i++) {
			bstatus = dpcd_read(it6505, DP_AUX_HDCP_BSTATUS);
			usleep_range(2000, 3000);
			if (bstatus & DP_BSTATUS_READY)
				break;
		}

		/*
		 * Read Bstatus to determine what happened
		 */
		DRM_DEV_DEBUG_DRIVER(dev, "Bstatus: 0x%02x", bstatus);
		dptx_debug_print(it6505, 0x3B, "ar0_low");
		dptx_debug_print(it6505, 0x3C, "ar0_high");
		dptx_debug_print(it6505, 0x45, "br0_low");
		dptx_debug_print(it6505, 0x46, "br0_high");

		if (bstatus & DP_BSTATUS_READY) {
			DRM_DEV_DEBUG_DRIVER(dev, "HDCP KSV list ready");
			len = it6505_makeup_sha1_input(it6505);
			sha1_digest(it6505, it6505->sha1_input, len,
				    (u8 *)it6505->av);
			it6505_check_sha1_result(it6505);
		}
	}
	if (it6505->enable_audio && (reg07 & BIT(2))) {
		DRM_DEV_DEBUG_DRIVER(dev, "Audio FIFO OverFlow Interrupt");
		dptx_debug_print(it6505, 0xBE, "");
		dptx_set_bits(it6505, 0xD3, 0x20, 0x20);
		dptx_set_bits(it6505, 0xE8, 0x22, 0x00);
		dptx_set_bits(it6505, 0xB8, 0x80, 0x80);
		dptx_set_bits(it6505, 0xB8, 0x80, 0x00);
		it6505_set_audio(it6505);
	}
	if (it6505->enable_audio && (reg07 & BIT(5)))
		DRM_DEV_DEBUG_DRIVER(dev, "Audio infoframe packet done");
}

static void it6505_check_reg08(struct it6505 *it6505, unsigned int reg08)
{
	struct device *dev = &it6505->client->dev;

	if (it6505->state == SYS_UNPLUG)
		return;
	if (it6505->enable_audio && (reg08 & BIT(1)))
		DRM_DEV_DEBUG_DRIVER(dev, "Audio TimeStamp Packet Done!");

	if (it6505->enable_audio && (reg08 & BIT(3))) {
		DRM_DEV_DEBUG_DRIVER(dev, "Audio M Error Interrupt ...");
		show_audio_m_count(it6505);
	}
	if (reg08 & BIT(4)) {
		DRM_DEV_DEBUG_DRIVER(dev, "Link Training Fail Interrupt");
		/* restart training */
		if (it6505->dpcd_sink_count != 0)
			dptx_sys_chg(it6505, SYS_UNPLUG);
	}

	if (reg08 & BIT(7)) {
		DRM_DEV_DEBUG_DRIVER(dev, "IO Latch FIFO OverFlow Interrupt");
		dptx_set_bits(it6505, 0x61, 0x02, 0x02);
		dptx_set_bits(it6505, 0x61, 0x02, 0x00);
	}
}

static void it6505_dptx_irq(struct it6505 *it6505)
{
	int reg06, reg07, reg08;
	struct device *dev = &it6505->client->dev;

	reg06 = dptx_read(it6505, 0x06);
	reg07 = dptx_read(it6505, 0x07);
	reg08 = dptx_read(it6505, 0x08);

	dptx_write(it6505, 0x06, reg06);
	dptx_write(it6505, 0x07, reg07);
	dptx_write(it6505, 0x08, reg08);

	DRM_DEV_DEBUG_DRIVER(dev, "reg06 = 0x%02x", reg06);
	DRM_DEV_DEBUG_DRIVER(dev, "reg07 = 0x%02x", reg07);
	DRM_DEV_DEBUG_DRIVER(dev, "reg08 = 0x%02x", reg08);
	dptx_debug_print(it6505, 0x0D, "");

	if (reg06 != 0)
		it6505_check_reg06(it6505, reg06);

	if (reg07 != 0)
		it6505_check_reg07(it6505, reg07);

	if (reg08 != 0)
		it6505_check_reg08(it6505, reg08);
}

static void it6505_bridge_enable(struct drm_bridge *bridge)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);

	if (!it6505->enable_drv_hold) {
		it6505_int_mask_on(it6505);
		dptx_sys_chg(it6505, SYS_HPD);
	}
}

static void it6505_bridge_disable(struct drm_bridge *bridge)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);

	dptx_sys_chg(it6505, SYS_UNPLUG);
}

static const struct drm_bridge_funcs it6505_bridge_funcs = {
	.attach = it6505_bridge_attach,
	.detach = it6505_bridge_detach,
	.mode_valid = it6505_bridge_mode_valid,
	.mode_set = it6505_bridge_mode_set,
	.enable = it6505_bridge_enable,
	.disable = it6505_bridge_disable,
};

static void it6505_clear_int(struct it6505 *it6505)
{
	dptx_write(it6505, 0x06, 0xFF);
	dptx_write(it6505, 0x07, 0xFF);
	dptx_write(it6505, 0x08, 0xFF);
}

static irqreturn_t it6505_intp_threaded_handler(int unused, void *data)
{
	struct it6505 *it6505 = data;
	struct device *dev = &it6505->client->dev;

	msleep(150);
	mutex_lock(&it6505->lock);

	if (it6505->enable_drv_hold == 0 && it6505->powered) {
		DRM_DEV_DEBUG_DRIVER(dev, "into it6505_dptx_irq");
		it6505_dptx_irq(it6505);
	}

	mutex_unlock(&it6505->lock);
	return IRQ_HANDLED;
}

static int it6505_init_pdata(struct it6505 *it6505)
{
	struct it6505_platform_data *pdata = &it6505->pdata;
	struct device *dev = &it6505->client->dev;

	/* 1.0V digital core power regulator  */
	pdata->pwr18 = devm_regulator_get(dev, "pwr18");
	if (IS_ERR(pdata->pwr18)) {
		DRM_DEV_ERROR(dev, "pwr18 regulator not found");
		return PTR_ERR(pdata->pwr18);
	}

	pdata->ovdd = devm_regulator_get(dev, "ovdd");
	if (IS_ERR(pdata->ovdd)) {
		DRM_DEV_ERROR(dev, "ovdd regulator not found");
		return PTR_ERR(pdata->ovdd);
	}

	/* GPIO for HPD */
	pdata->gpiod_hpd = devm_gpiod_get(dev, "hpd", GPIOD_IN);
	if (IS_ERR(pdata->gpiod_hpd))
		return PTR_ERR(pdata->gpiod_hpd);

	/* GPIO for chip reset */
	pdata->gpiod_reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);

	return PTR_ERR_OR_ZERO(pdata->gpiod_reset);
}

static int it6505_bridge_resume(struct device *dev)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return it6505_poweron(it6505);
}

static int it6505_bridge_suspend(struct device *dev)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return it6505_poweroff(it6505);
}

static SIMPLE_DEV_PM_OPS(it6505_bridge_pm_ops, it6505_bridge_suspend,
			 it6505_bridge_resume);

static ssize_t enable_drv_hold_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->enable_drv_hold);
}

static ssize_t enable_drv_hold_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	unsigned int drv_hold;

	if (kstrtoint(buf, 10, &drv_hold) < 0)
		return -EINVAL;

	it6505->enable_drv_hold = drv_hold ? true : false;

	if (it6505->enable_drv_hold) {
		it6505_int_mask_off(it6505);
	} else {
		it6505_clear_int(it6505);
		it6505_int_mask_on(it6505);
	}
	return count;
}

static ssize_t print_timing_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	struct drm_display_mode *vid = &it6505->video_info;
	char *str = buf, *end = buf + PAGE_SIZE;

	str += scnprintf(str, end - str, "---video timing---\n");
	str += scnprintf(str, end - str, "PCLK:%d.%03dMHz\n", vid->clock / 1000,
			 vid->clock % 1000);
	str += scnprintf(str, end - str, "HTotal:%d\n", vid->htotal);
	str += scnprintf(str, end - str, "HActive:%d\n", vid->hdisplay);
	str += scnprintf(str, end - str, "HFrontPorch:%d\n",
			 vid->hsync_start - vid->hdisplay);
	str += scnprintf(str, end - str, "HSyncWidth:%d\n",
			 vid->hsync_end - vid->hsync_start);
	str += scnprintf(str, end - str, "HBackPorch:%d\n",
			 vid->htotal - vid->hsync_end);
	str += scnprintf(str, end - str, "VTotal:%d\n", vid->vtotal);
	str += scnprintf(str, end - str, "VActive:%d\n", vid->vdisplay);
	str += scnprintf(str, end - str, "VFrontPorch:%d\n",
			 vid->vsync_start - vid->vdisplay);
	str += scnprintf(str, end - str, "VSyncWidth:%d\n",
			 vid->vsync_end - vid->vsync_start);
	str += scnprintf(str, end - str, "VBackPorch:%d\n",
			 vid->vtotal - vid->vsync_end);

	return str - buf;
}

static ssize_t sha_debug_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int i = 0;
	char *str = buf, *end = buf + PAGE_SIZE;
	struct it6505 *it6505 = dev_get_drvdata(dev);

	str += scnprintf(str, end - str, "sha input:\n");
	for (i = 0; i < ARRAY_SIZE(it6505->sha1_input); i += 16)
		str += scnprintf(str, end - str, "%16ph\n",
				 it6505->sha1_input + i);

	str += scnprintf(str, end - str, "av:\n");
	for (i = 0; i < ARRAY_SIZE(it6505->av); i++)
		str += scnprintf(str, end - str, "%4ph\n", it6505->av[i]);

	str += scnprintf(str, end - str, "bv:\n");
	for (i = 0; i < ARRAY_SIZE(it6505->bv); i++)
		str += scnprintf(str, end - str, "%4ph\n", it6505->bv[i]);

	return end - str;
}

static ssize_t enable_hdcp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->enable_hdcp);
}

static ssize_t enable_hdcp_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	unsigned int reg3f, hdcp;

	if (kstrtoint(buf, 10, &hdcp) < 0)
		return -EINVAL;

	if (!it6505->powered || it6505->state == SYS_UNPLUG) {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "power down or unplug, can not fire HDCP");
		return -EINVAL;
	}
	it6505->enable_hdcp = hdcp ? true : false;

	if (it6505->enable_hdcp) {
		if (it6505->cp_capable) {
			dptx_sys_chg(it6505, SYS_HDCP);
			dptx_sys_fsm(it6505);
		} else {
			DRM_DEV_ERROR(dev, "sink not support HDCP");
		}
	} else {
		dptx_set_bits(it6505, 0x05, 0x10, 0x10);
		dptx_set_bits(it6505, 0x05, 0x10, 0x00);
		reg3f = dptx_read(it6505, 0x3F);
		hdcp = (reg3f & BIT(7)) >> 7;
		DRM_DEV_DEBUG_DRIVER(dev, "%s to disable hdcp",
				     hdcp ? "failed" : "succeeded");
	}
	return count;
}

static ssize_t force_pwronoff_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	int pwr;

	if (kstrtoint(buf, 10, &pwr) < 0)
		return -EINVAL;
	if (pwr)
		it6505_poweron(it6505);
	else
		it6505_poweroff(it6505);
	return count;
}

static ssize_t pwr_state_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->powered);
}

static DEVICE_ATTR_RO(print_timing);
static DEVICE_ATTR_RO(pwr_state);
static DEVICE_ATTR_RO(sha_debug);
static DEVICE_ATTR_WO(force_pwronoff);
static DEVICE_ATTR_RW(enable_drv_hold);
static DEVICE_ATTR_RW(enable_hdcp);

static const struct attribute *it6505_attrs[] = {
	&dev_attr_enable_drv_hold.attr,
	&dev_attr_print_timing.attr,
	&dev_attr_sha_debug.attr,
	&dev_attr_enable_hdcp.attr,
	&dev_attr_force_pwronoff.attr,
	&dev_attr_pwr_state.attr,
	NULL,
};

static void it6505_shutdown(struct i2c_client *client)
{
	struct it6505 *it6505 = dev_get_drvdata(&client->dev);

	dptx_sys_chg(it6505, SYS_UNPLUG);
}

static int it6505_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct it6505 *it6505;
	struct it6505_platform_data *pdata;
	struct device *dev = &client->dev;
	struct extcon_dev *extcon;
	int err, intp_irq;

	it6505 = devm_kzalloc(&client->dev, sizeof(*it6505), GFP_KERNEL);
	if (!it6505)
		return -ENOMEM;

	mutex_init(&it6505->lock);
	mutex_init(&it6505->mode_lock);
	mutex_init(&it6505->aux_lock);
	init_waitqueue_head(&it6505->edid_wait);
	pdata = &it6505->pdata;

	it6505->bridge.of_node = client->dev.of_node;
	it6505->client = client;
	i2c_set_clientdata(client, it6505);

	/* get extcon device from DTS */
	extcon = extcon_get_edev_by_phandle(dev, 0);
	if (PTR_ERR(extcon) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (IS_ERR(extcon)) {
		DRM_DEV_ERROR(dev, "can not get extcon device!");
		return -EINVAL;
	}

	it6505->extcon = extcon;

	it6505->lane_swap_disabled =
		device_property_read_bool(dev, "no-laneswap");

	err = it6505_init_pdata(it6505);
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to initialize pdata: %d", err);
		return err;
	}

	it6505->regmap =
		devm_regmap_init_i2c(client, &it6505_bridge_regmap_config);
	if (IS_ERR(it6505->regmap)) {
		DRM_DEV_ERROR(dev, "regmap i2c init failed");
		return PTR_ERR(it6505->regmap);
	}

	intp_irq = client->irq;

	if (!intp_irq) {
		DRM_DEV_ERROR(dev, "Failed to get CABLE_DET and INTP IRQ");
		return -ENODEV;
	}

	err = devm_request_threaded_irq(&client->dev, intp_irq, NULL,
					it6505_intp_threaded_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"it6505-intp", it6505);
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to request INTP threaded IRQ: %d",
			      err);
		return err;
	}

	err = it6505_register_audio_driver(dev);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to register audio driver: %d", err);
		return err;
	}

	/* Register aux channel */
	it6505->aux.name = "DP-AUX";
	it6505->aux.dev = dev;
	it6505->aux.transfer = it6505_aux_transfer;
	err = drm_dp_aux_register(&it6505->aux);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to register aux: %d", err);
		return err;
	}

	it6505->enable_drv_hold = DEFAULT_DRV_HOLD;
	it6505->enable_hdcp = true;
	it6505->enable_audio = false;
	it6505->enable_auto_train = true;
	it6505->powered = false;

	if (DEFAULT_PWR_ON)
		it6505_poweron(it6505);
	err = sysfs_create_files(&client->dev.kobj, it6505_attrs);
	if (err) {
		drm_dp_aux_unregister(&it6505->aux);
		return err;
	}

	it6505->bridge.funcs = &it6505_bridge_funcs;
	drm_bridge_add(&it6505->bridge);
	return 0;
}

static int it6505_remove(struct i2c_client *client)
{
	struct it6505 *it6505 = i2c_get_clientdata(client);

	drm_connector_unregister(&it6505->connector);
	drm_connector_cleanup(&it6505->connector);
	drm_bridge_remove(&it6505->bridge);
	sysfs_remove_files(&client->dev.kobj, it6505_attrs);
	drm_dp_aux_unregister(&it6505->aux);
	it6505_poweroff(it6505);
	return 0;
}

static const struct i2c_device_id it6505_id[] = {
	{ "it6505", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, it6505_id);

static const struct of_device_id it6505_of_match[] = {
	{ .compatible = "ite,it6505" },
	{ }
};

static struct i2c_driver it6505_i2c_driver = {
	.driver = {
		.name = "it6505_dptx",
		.of_match_table = it6505_of_match,
		.pm = &it6505_bridge_pm_ops,
	},
	.probe = it6505_i2c_probe,
	.remove = it6505_remove,
	.shutdown = it6505_shutdown,
	.id_table = it6505_id,
};

module_i2c_driver(it6505_i2c_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_DESCRIPTION("IT6505 DisplayPort Transmitter driver");
MODULE_LICENSE("GPL v2");
