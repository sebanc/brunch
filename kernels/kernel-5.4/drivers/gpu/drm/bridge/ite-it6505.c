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
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>

#include <crypto/hash.h>
#include <crypto/sha.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include <sound/hdmi-codec.h>

#define AX 0
#define BX 1
#define AUDSEL I2S
#define AUDTYPE LPCM
#define AUDFS AUD48K
#define AUDCH 2
/* 0: Standard I2S;1: 32bit I2S */
#define I2SINPUTFMT 1
/* 0: Left-justified;1: Right-justified */
#define I2SJUSTIFIED 0
/* 0: Data delay 1T correspond to WS;1: No data delay correspond to WS */
#define I2SDATADELAY 0
/* 0: is left channel;1: is right channel */
#define I2SWSCHANNEL 0
/* 0: MSB shift first;1: LSB shift first */
#define I2SDATASEQ 0

#define LANESWAP 0
#define LANE 4
#define _HBR 1
#define ENSSC 1
#define FLAGTRAINDOWN 150
#define POLLINGKSVLIST 400
#define TRAINFAILCNT 5
#define TRAINFAILHPD 3
#define AUX_WAIT_TIMEOUT_MS 15
#define PCLK_DELAY 1
#define PCLK_INV 0
#define EDIDRETRYTIME 5
#define SHOWVIDEOTIMING 2
#define PWROFFRETRYTIME 5
#define MAXPCLK 80000
#define DEFAULTHDCP 1
#define DEFAULTAUDIO 0
#define DEFAULTPWRONOFF 1
#define DEFAULTDRVHOLD 0
#define DEFAULTPWRON 0
#define AUX_FIFO_MAX_SIZE 0x10

/* AX or BX */
#define CHIP_VERSION BX

/*
 * 0: for bitland
 * 1: for google kukui p2, huaqin
 */
#define AFE_SETTING 1

static u8 afe_setting_table[2][3] = {
	{0, 0, 0},
	{0x93, 0x2a, 0x85}
};

enum sys_status {
	SYS_UNPLUG = 0,
	SYS_HPD,
	SYS_AUTOTRAIN,
	SYS_WAIT,
	SYS_TRAINFAIL,
	SYS_ReHDCP,
	SYS_NOROP,
	SYS_Unknown,
};

enum it6505_aud_sel {
	I2S = 0,
	SPDIF,
};

enum it6505_aud_fs {
	AUD24K = 0x6,
	AUD32K = 0x3,
	AUD48K = 0x2,
	AUD96K = 0xA,
	AUD192K = 0xE,
	AUD44P1K = 0x0,
	AUD88P2K = 0x8,
	AUD176P4K = 0xC,
};

enum it6505_aud_type {
	LPCM = 0,
	NLPCM,
	DSS,
	HBR,
};

enum aud_word_length {
	AUD16BIT = 0,
	AUD18BIT,
	AUD20BIT,
	AUD24BIT,
};

/* Audio Sample Word Length: AUD16BIT, AUD18BIT, AUD20BIT, AUD24BIT */
#define AUDWORDLENGTH AUD24BIT

struct it6505_platform_data {
	struct regulator *pwr18;
	struct regulator *ovdd;
	struct gpio_desc *gpiod_hpd;
	struct gpio_desc *gpiod_reset;
};

struct it6505 {
	struct drm_dp_aux aux;
	struct drm_bridge bridge;
	struct i2c_client *client;
	struct edid *edid;
	struct drm_connector connector;
	struct drm_dp_link link;
	struct it6505_platform_data pdata;
	struct mutex lock;
	struct mutex mode_lock;
	struct regmap *regmap;
	struct drm_display_mode vid_info;

	struct notifier_block event_nb;
	struct extcon_dev *extcon;
	struct work_struct extcon_wq;
	struct delayed_work delayed_audio;
	enum sys_status status;
	bool hbr;
	u8 en_ssc;
	bool laneswap_disabled;
	bool laneswap;

	enum it6505_aud_sel aud_sel;
	enum it6505_aud_fs aud_fs;
	enum it6505_aud_type aud_type;
	u8 aud_ch;
	u8 i2s_input_fmt;
	u8 i2s_justified;
	u8 i2s_data_delay;
	u8 i2s_ws_channel;
	u8 i2s_data_seq;
	u8 vidstable_done;
	enum aud_word_length audwordlength;
	unsigned int en_hdcp;
	unsigned int en_pwronoff;
	unsigned int en_audio;
	u8 cntfsm;
	u8 train_fail_hpd;
	bool cp_capable;
	bool cp_done;
	u8 downstream_repeater;
	u8 shainput[64];
	u8 av[5][4];
	u8 bv[5][4];
	bool powered;
	/* it6505 driver hold option */
	unsigned int drv_hold;

	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	enum drm_connector_status last_connector_status;
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

static int dptxrd(struct it6505 *it6505, unsigned int reg_addr,
		  unsigned int *value)
{
	int err;
	struct device *dev = &it6505->client->dev;

	err = regmap_read(it6505->regmap, reg_addr, value);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "read failed reg[0x%x] err: %d", reg_addr,
			      err);
		return err;
	}

	return 0;
}

static void it6505_dump(struct it6505 *it6505)
{
	unsigned int temp[16], i, j;
	u8 value[16];
	struct device *dev = &it6505->client->dev;

	for (i = 0; i <= 0xff; i += 16) {
		for (j = 0; j < 16; j++) {
			dptxrd(it6505, i + j, temp + j);
			value[j] = temp[j];
		}
		DRM_DEV_DEBUG_DRIVER(dev, "[0x%02x] = %16ph", i, value);
	}
}

static int dptxwr(struct it6505 *it6505, unsigned int reg_addr,
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

static int dptxset(struct it6505 *it6505, unsigned int reg, unsigned int mask,
		   unsigned int value)
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

static void dptx_debug_print(struct it6505 *it6505, unsigned int reg,
			     const char *prefix)
{
	unsigned int val;
	int err;
	struct device *dev = &it6505->client->dev;

	if (likely(!(drm_debug & DRM_UT_DRIVER)))
		return;

	err = dptxrd(it6505, reg, &val);
	if (err < 0)
		DRM_DEV_DEBUG_DRIVER(dev, "%s reg%02x read error", prefix, reg);
	else
		DRM_DEV_DEBUG_DRIVER(dev, "%s reg%02x = 0x%02x", prefix, reg,
				     val);
}

static inline struct it6505 *connector_to_it6505(struct drm_connector *c)
{
	return container_of(c, struct it6505, connector);
}

static inline struct it6505 *bridge_to_it6505(struct drm_bridge *bridge)
{
	return container_of(bridge, struct it6505, bridge);
}

static void it6505_init_fsm(struct it6505 *it6505)
{
	it6505->aud_sel = AUDSEL;
	it6505->aud_fs = AUDFS;
	it6505->aud_ch = AUDCH;
	it6505->aud_type = AUDTYPE;
	it6505->i2s_input_fmt = I2SINPUTFMT;
	it6505->i2s_justified = I2SJUSTIFIED;
	it6505->i2s_data_delay = I2SDATADELAY;
	it6505->i2s_ws_channel = I2SWSCHANNEL;
	it6505->i2s_data_seq = I2SDATASEQ;
	it6505->audwordlength = AUDWORDLENGTH;
	it6505->en_audio = DEFAULTAUDIO;
	it6505->en_hdcp = DEFAULTHDCP;

	it6505->hbr = _HBR;
	it6505->en_ssc = ENSSC;
	it6505->laneswap = LANESWAP;
	it6505->vidstable_done = 0;
}

static void it6505_termination_on(struct it6505 *it6505)
{
#if (CHIP_VERSION == BX)
	dptxset(it6505, 0x5D, 0x80, 0x00);
	dptxset(it6505, 0x5E, 0x02, 0x02);
#endif
}

static void it6505_termination_off(struct it6505 *it6505)
{
#if (CHIP_VERSION == BX)
	dptxset(it6505, 0x5D, 0x80, 0x80);
	dptxset(it6505, 0x5E, 0x02, 0x00);
	dptxset(it6505, 0x5C, 0xF0, 0x00);
#endif
}

static bool dptx_getsinkhpd(struct it6505 *it6505)
{
	unsigned int value;
	int ret;

	ret = dptxrd(it6505, 0x0D, &value);

	if (ret < 0)
		return false;

	return (value & BIT(1)) == BIT(1);
}

static void dptx_chgbank(struct it6505 *it6505, unsigned int bank_id)
{
	dptxset(it6505, 0x0F, 0x01, bank_id & BIT(0));
}

static void it6505_int_mask_on(struct it6505 *it6505)
{
	dptxwr(it6505, 0x09, 0x1F);
	dptxwr(it6505, 0x0A, 0x07);
	dptxwr(it6505, 0x0B, 0x90);
}

static void it6505_int_mask_off(struct it6505 *it6505)
{
	dptxwr(it6505, 0x09, 0x00);
	dptxwr(it6505, 0x0A, 0x00);
	dptxwr(it6505, 0x0B, 0x00);
}

static void dptx_init(struct it6505 *it6505)
{
	dptxwr(it6505, 0x05, 0x3B);
	usleep_range(1000, 2000);
	dptxwr(it6505, 0x05, 0x1F);
	usleep_range(1000, 1500);
}

static void it6505_init_and_mask_on(struct it6505 *it6505)
{
	dptx_init(it6505);
	it6505_int_mask_on(it6505);
}

static int it6505_poweron(struct it6505 *it6505)
{
	struct it6505_platform_data *pdata = &it6505->pdata;
	int err = 0;
	struct device *dev = &it6505->client->dev;

	if (it6505->powered) {
		DRM_DEV_DEBUG_DRIVER(dev, "it6505 already powered on");
		return 0;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "it6505 start to power on");

	err = regulator_enable(pdata->pwr18);
	DRM_DEV_DEBUG_DRIVER(dev, "%s to enable pwr18 regulator",
			     err ? "Failed" : "Succeeded");
	if (err)
		return err;
	/* time interval between IVDD and OVDD at least be 1ms */
	usleep_range(1000, 2000);
	err = regulator_enable(pdata->ovdd);
	DRM_DEV_DEBUG_DRIVER(dev, "%s to enable ovdd regulator",
			     err ? "Failed" : "Succeeded");
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
	int err = 0;
	struct device *dev = &it6505->client->dev;

	if (!it6505->powered) {
		DRM_DEV_DEBUG_DRIVER(dev, "power had been already off");
		return 0;
	}
	gpiod_set_value_cansleep(pdata->gpiod_reset, 0);
	err = regulator_disable(pdata->pwr18);
	DRM_DEV_DEBUG_DRIVER(dev, "%s to disable pwr18 regulator",
			     err ? "Failed" : "Succeeded");
	if (err)
		return err;
	err = regulator_disable(pdata->ovdd);
	DRM_DEV_DEBUG_DRIVER(dev, "%s to disable ovdd regulator",
			     err ? "Failed" : "Succeeded");
	if (err)
		return err;

	kfree(it6505->edid);
	it6505->edid = NULL;
	it6505->powered = false;
	return 0;
}

static void show_vid_info(struct it6505 *it6505)
{
	int hsync_pol, vsync_pol, interlaced;
	int htotal, hdes, hdew, hfph, hsyncw;
	int vtotal, vdes, vdew, vfph, vsyncw;
	int rddata, rddata1, i;
	int pclk, sum;

	usleep_range(10000, 15000);
	dptx_chgbank(it6505, 0);
	dptxrd(it6505, 0xa0, &rddata);
	hsync_pol = rddata & BIT(0);
	vsync_pol = (rddata & BIT(2)) >> 2;
	interlaced = (rddata & BIT(4)) >> 4;

	dptxrd(it6505, 0xa1, &rddata);
	dptxrd(it6505, 0xa2, &rddata1);
	htotal = ((rddata1 & 0x1F) << 8) + rddata;

	dptxrd(it6505, 0xa3, &rddata);
	dptxrd(it6505, 0xa4, &rddata1);

	hdes = ((rddata1 & 0x1F) << 8) + rddata;

	dptxrd(it6505, 0xa5, &rddata);
	dptxrd(it6505, 0xa6, &rddata1);

	hdew = ((rddata1 & 0x1F) << 8) + rddata;

	dptxrd(it6505, 0xa7, &rddata);
	dptxrd(it6505, 0xa8, &rddata1);

	hfph = ((rddata1 & 0x1F) << 8) + rddata;

	dptxrd(it6505, 0xa9, &rddata);
	dptxrd(it6505, 0xaa, &rddata1);

	hsyncw = ((rddata1 & 0x1F) << 8) + rddata;

	dptxrd(it6505, 0xab, &rddata);
	dptxrd(it6505, 0xac, &rddata1);
	vtotal = ((rddata1 & 0x0F) << 8) + rddata;

	dptxrd(it6505, 0xad, &rddata);
	dptxrd(it6505, 0xae, &rddata1);
	vdes = ((rddata1 & 0x0F) << 8) + rddata;

	dptxrd(it6505, 0xaf, &rddata);
	dptxrd(it6505, 0xb0, &rddata1);
	vdew = ((rddata1 & 0x0F) << 8) + rddata;

	dptxrd(it6505, 0xb1, &rddata);
	dptxrd(it6505, 0xb2, &rddata1);
	vfph = ((rddata1 & 0x0F) << 8) + rddata;

	dptxrd(it6505, 0xb3, &rddata);
	dptxrd(it6505, 0xb4, &rddata1);
	vsyncw = ((rddata1 & 0x0F) << 8) + rddata;

	sum = 0;
	for (i = 0; i < 100; i++) {
		dptxset(it6505, 0x12, 0x80, 0x80);
		usleep_range(10000, 15000);
		dptxset(it6505, 0x12, 0x80, 0x00);

		dptxrd(it6505, 0x13, &rddata);
		dptxrd(it6505, 0x14, &rddata1);
		rddata = ((rddata1 & 0x0F) << 8) + rddata;

		sum += rddata;
	}

	sum /= 100;
	pclk = 13500 * 2048 / sum;
	it6505->vid_info.clock = pclk;
	it6505->vid_info.hdisplay = hdew;
	it6505->vid_info.hsync_start = hdew + hfph;
	it6505->vid_info.hsync_end = hdew + hfph + hsyncw;
	it6505->vid_info.htotal = htotal;
	it6505->vid_info.vdisplay = vdew;
	it6505->vid_info.vsync_start = vdew + vfph;
	it6505->vid_info.vsync_end = vdew + vfph + vsyncw;
	it6505->vid_info.vtotal = vtotal;
	it6505->vid_info.vrefresh = pclk / htotal / vtotal;

	DRM_DEV_DEBUG_DRIVER(&it6505->client->dev, DRM_MODE_FMT,
			     DRM_MODE_ARG(&it6505->vid_info));
}

static void show_aud_mcnt(struct it6505 *it6505)
{
	unsigned int audn, regde, regdf, rege0, vclk, aclk, audmcal, audmcnt;
	struct device *dev = &it6505->client->dev;

	dptxrd(it6505, 0xde, &regde);
	dptxrd(it6505, 0xdf, &regdf);
	dptxrd(it6505, 0xe0, &rege0);
	audn = regde + (regdf << 8) + (rege0 << 16);
	if (it6505->hbr)
		vclk = 2700000;
	else
		vclk = 1620000;
	switch (it6505->aud_fs) {
	case AUD32K:
		aclk = 320;
		break;
	case AUD48K:
		aclk = 480;
		break;
	case AUD96K:
		aclk = 960;
		break;
	case AUD192K:
		aclk = 1920;
		break;
	case AUD44P1K:
		aclk = 441;
		break;
	case AUD88P2K:
		aclk = 882;
		break;
	case AUD176P4K:
		aclk = 1764;
		break;
	default:
		aclk = 0;
		break;
	}
	audmcal = audn * aclk / vclk * 512;
	dptxrd(it6505, 0xe4, &regde);
	dptxrd(it6505, 0xe5, &regdf);
	dptxrd(it6505, 0xe6, &rege0);
	audmcnt = (rege0 << 16) + (regdf << 8) + regde;
	DRM_DEV_DEBUG_DRIVER(dev, "audio N:0x%06x", audn);
	DRM_DEV_DEBUG_DRIVER(dev, "audio Mcal:0x%06x", audmcal);
	DRM_DEV_DEBUG_DRIVER(dev, "audio Mcnt:0x%06x", audmcnt);
}

static const char *const state_string[] = {
	[SYS_UNPLUG] = "SYS_UNPLUG",
	[SYS_HPD] = "SYS_HPD",
	[SYS_AUTOTRAIN] = "SYS_AUTOTRAIN",
	[SYS_WAIT] = "SYS_WAIT",
	[SYS_TRAINFAIL] = "SYS_TRAINFAIL",
	[SYS_ReHDCP] = "SYS_ReHDCP",
	[SYS_NOROP] = "SYS_NOROP",
	[SYS_Unknown] = "SYS_Unknown",
};

static void dptx_sys_chg(struct it6505 *it6505, enum sys_status newstate)
{
	int i = 0;
	struct device *dev = &it6505->client->dev;

	if (newstate != SYS_UNPLUG) {
		if (!dptx_getsinkhpd(it6505))
			newstate = SYS_UNPLUG;
	}
	if (it6505->status == newstate)
		return;

	DRM_DEV_DEBUG_DRIVER(dev, "sys_state change: %s -> %s",
			     state_string[it6505->status],
			     state_string[newstate]);
	it6505->status = newstate;

	switch (it6505->status) {
	case SYS_UNPLUG:
		kfree(it6505->edid);
		it6505->edid = NULL;
		DRM_DEV_DEBUG_DRIVER(dev, "Free it6505 EDID memory");
		it6505_init_and_mask_on(it6505);
		it6505_termination_off(it6505);
		break;
	case SYS_HPD:
		it6505_termination_on(it6505);
		break;
	case SYS_AUTOTRAIN:
		break;
	case SYS_WAIT:
		break;
	case SYS_ReHDCP:
		break;
	case SYS_NOROP:
		for (i = 0; i < SHOWVIDEOTIMING; i++)
			show_vid_info(it6505);
		break;
	case SYS_TRAINFAIL:
		/* it6505 goes to idle */
		break;
	default:
		break;
	}
}

static bool it6505_aux_op_finished(struct it6505 *it6505)
{
	unsigned int value;
	int err;

	err = regmap_read(it6505->regmap, 0x2b, &value);
	if (err < 0)
		return false;

	return (value & BIT(5)) == 0;
}

static int dptx_auxwait(struct it6505 *it6505)
{
	unsigned int status;
	unsigned long timeout;
	int err;
	struct device *dev = &it6505->client->dev;

	timeout = jiffies + msecs_to_jiffies(AUX_WAIT_TIMEOUT_MS) + 1;

	while (!it6505_aux_op_finished(it6505)) {
		if (time_after(jiffies, timeout)) {
			DRM_DEV_ERROR(dev, "Timed out waiting AUX to finish");
			return -ETIMEDOUT;
		}
		usleep_range(1000, 2000);
	}

	err = dptxrd(it6505, 0x9f, &status);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to read AUX channel: %d", err);
		return err;
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

static ssize_t it6505_aux_do_transfer(struct it6505 *it6505,
				      enum aux_cmd_type cmd,
				      unsigned int address, u8 *buffer,
				      size_t size, enum aux_cmd_reply *reply)
{
	int ret;
	int i;
	int status, val;

	if (cmd == CMD_AUX_I2C_EDID_READ) {
		/* DP AUX EDID FIFO has maximum length of 16 bytes. */
		size = min_t(size_t, size, AUX_FIFO_MAX_SIZE);
		/* Enable AUX FIFO read back and clear FIFO */
		dptxwr(it6505, 0x23, 0xC3);
		dptxwr(it6505, 0x23, 0xC2);
	} else {
		/* The DP AUX transmit buffer has 4 bytes. */
		size = min_t(size_t, size, 4);
		dptxwr(it6505, 0x23, 0x42);
	}

	/* Start Address[7:0] */
	dptxwr(it6505, 0x24, (address >> 0) & 0xFF);
	/* Start Address[15:8] */
	dptxwr(it6505, 0x25, (address >> 8) & 0xFF);
	/* WriteNum[3:0]+StartAdr[19:16] */
	dptxwr(it6505, 0x26, ((address >> 16) & 0x0F) | ((size - 1) << 4));

	if (cmd == CMD_AUX_NATIVE_WRITE)
		regmap_bulk_write(it6505->regmap, 0x27, buffer, size);

	/* Aux Fire */
	dptxwr(it6505, 0x2B, cmd);

	ret = dptx_auxwait(it6505);
	if (ret < 0)
		return ret;

	ret = dptxrd(it6505, 0x9F, &status);
	if (ret < 0)
		return ret;

	switch ((status >> 6) & 0x3) {
	case 0:
		*reply = REPLY_ACK;
		break;
	case 1:
		*reply = REPLY_NACK;
		return 0;
	case 2:
		*reply = REPLY_DEFER;
		return 0;
	case 3:
		return -ETIMEDOUT;
	}

	if (cmd == CMD_AUX_NATIVE_WRITE)
		goto out;

	if (cmd == CMD_AUX_I2C_EDID_READ) {
		for (i = 0; i < size; i++) {
			ret = dptxrd(it6505, 0x2F, &val);
			if (ret < 0)
				return ret;
			buffer[i] = val;
		}
	} else {
		for (i = 0; i < size; i++) {
			ret = dptxrd(it6505, 0x2C + i, &val);
			if (ret < 0)
				return ret;
			buffer[size - 1 - i] = val;
		}
	}

out:
	dptxwr(it6505, 0x23, 0x40);
	return size;
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
	int offset, ret;
	struct device *dev = &it6505->client->dev;
	enum aux_cmd_reply reply;

	dptxset(it6505, 0x05, 0x08, 0x08);
	dptxset(it6505, 0x05, 0x08, 0x00);
	DRM_DEV_DEBUG_DRIVER(dev, "blocknum = %d", blockno);

	for (offset = 0; offset < EDID_LENGTH; offset += 8) {
		ret = it6505_aux_do_transfer(it6505, CMD_AUX_I2C_EDID_READ,
					     blockno * EDID_LENGTH + offset,
					     buf + offset, 8, &reply);
		if (ret < 0)
			return ret;
		if (reply != REPLY_ACK)
			return -EIO;

		DRM_DEV_DEBUG_DRIVER(dev, "[0x%02x]: %8ph", offset,
				     buf + offset);
	}

	return 0;
}

static int it6505_get_modes(struct drm_connector *connector)
{
	struct it6505 *it6505 = connector_to_it6505(connector);
	int err, num_modes = 0;
	struct device *dev = &it6505->client->dev;

	it6505->train_fail_hpd = TRAINFAILHPD;
	if (it6505->edid)
		return drm_add_edid_modes(connector, it6505->edid);
	mutex_lock(&it6505->mode_lock);

	it6505_dump(it6505);
	dptx_debug_print(it6505, 0x9F, "aux status");
	it6505->edid =
		drm_do_get_edid(&it6505->connector, dptx_get_edidblock, it6505);
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

	if (gpiod_get_value(it6505->pdata.gpiod_hpd))
		status = connector_status_disconnected;
	else
		status = connector_status_connected;

	if (status != it6505->last_connector_status) {
		it6505->last_connector_status = status;
		it6505_update_plugged_status(it6505, status);
	}

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

	if (it6505->drv_hold)
		return;
	mutex_lock(&it6505->lock);
	DRM_DEV_DEBUG_DRIVER(dev, "EXTCON_DISP_DP = 0x%02x", state);
	if (state > 0) {
		DRM_DEV_DEBUG_DRIVER(dev, "start to power on");
		msleep(1000);
		it6505_poweron(it6505);
		if (!dptx_getsinkhpd(it6505))
			it6505_termination_off(it6505);
	} else {
		if (it6505->en_pwronoff) {
			DRM_DEV_DEBUG_DRIVER(dev, "start to power off");
			while (it6505_poweroff(it6505) &&
			       pwroffretry++ < PWROFFRETRYTIME) {
				DRM_DEV_DEBUG_DRIVER(dev,
						     "power off fail %d times",
						     pwroffretry);
			}
			drm_helper_hpd_irq_event(it6505->connector.dev);
			DRM_DEV_DEBUG_DRIVER(dev, "power off it6505 success!");
		}
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
	return 0;
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
		return err;
	}

	err = drm_connector_register(&it6505->connector);
	if (err < 0) {
		DRM_DEV_ERROR(dev, "Failed to register connector: %d", err);
		return err;
	}

	err = it6505_use_notifier_module(it6505);
	if (err < 0) {
		drm_connector_unregister(&it6505->connector);
		return err;
	}
	schedule_work(&it6505->extcon_wq);

	return 0;
}

static void it6505_bridge_detach(struct drm_bridge *bridge)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);

	devm_extcon_unregister_notifier(&it6505->client->dev, it6505->extcon,
					EXTCON_DISP_DP, &it6505->event_nb);
	flush_work(&it6505->extcon_wq);
	drm_connector_unregister(&it6505->connector);
}

static enum drm_mode_status
it6505_bridge_mode_valid(struct drm_bridge *bridge,
			 const struct drm_display_mode *mode)
{
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	/* Max 1200p at 5.4 Ghz, one lane */
	if (mode->clock > MAXPCLK)
		return MODE_CLOCK_HIGH;

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

	err = dptxset(it6505, 0xe8, 0x01, 0x00);
	if (err)
		return err;

	err = regmap_bulk_write(it6505->regmap, 0xe9,
				buffer + HDMI_INFOFRAME_HEADER_SIZE,
				frame->length);
	if (err)
		return err;

	err = dptxset(it6505, 0xe8, 0x01, 0x01);
	if (err)
		return err;

	return 0;
}

static void it6505_bridge_mode_set(struct drm_bridge *bridge,
				   const struct drm_display_mode *mode,
				   const struct drm_display_mode *adjusted_mode)
{
	struct it6505 *it6505 = bridge_to_it6505(bridge);
	struct hdmi_avi_infoframe frame;
	int err;
	struct device *dev = &it6505->client->dev;

	mutex_lock(&it6505->mode_lock);
	err = drm_hdmi_avi_infoframe_from_display_mode(&frame,
						       &it6505->connector,
						       adjusted_mode);
	if (err) {
		DRM_DEV_ERROR(dev, "Failed to setup AVI infoframe: %d", err);
		goto unlock;
	}
	strlcpy(it6505->vid_info.name, adjusted_mode->name,
		DRM_DISPLAY_MODE_LEN);
	it6505->vid_info.type = adjusted_mode->type;
	it6505->vid_info.flags = adjusted_mode->flags;
	err = it6505_send_video_infoframe(it6505, &frame);
	if (err)
		DRM_DEV_ERROR(dev, "Failed to send AVI infoframe: %d", err);

unlock:
	mutex_unlock(&it6505->mode_lock);
}

static void dptx_set_aud_fmt(struct it6505 *it6505)
{
	unsigned int audsrc;
	/* I2S MODE */
	dptxwr(it6505, 0xB9,
	       (it6505->audwordlength << 5) | (it6505->i2s_data_seq << 4) |
		       (it6505->i2s_ws_channel << 3) |
		       (it6505->i2s_data_delay << 2) |
		       (it6505->i2s_justified << 1) | it6505->i2s_input_fmt);
	if (it6505->aud_sel == SPDIF) {
		dptxwr(it6505, 0xBA, 0x00);
		/* 0x30 = 128*FS */
		dptxset(it6505, 0x11, 0xF0, 0x30);
	} else {
		dptxwr(it6505, 0xBA, 0xe4);
	}

	dptxwr(it6505, 0xBB, 0x20);
	dptxwr(it6505, 0xBC, 0x00);
	audsrc = 1;

	if (it6505->aud_ch > 2)
		audsrc |= 2;

	if (it6505->aud_ch > 4)
		audsrc |= 4;

	if (it6505->aud_ch > 6)
		audsrc |= 8;

	audsrc |= it6505->aud_sel << 4;

	dptxwr(it6505, 0xB8, audsrc);
}

static void dptx_set_aud_chsts(struct it6505 *it6505)
{
	enum it6505_aud_fs audfs = it6505->aud_fs;

	/* Channel Status */
	dptxwr(it6505, 0xBF, it6505->aud_type << 1);
	dptxwr(it6505, 0xC0, 0x00);
	dptxwr(it6505, 0xC1, 0x00);
	dptxwr(it6505, 0xC2, audfs);
	switch (it6505->audwordlength) {
	case AUD16BIT:
		dptxwr(it6505, 0xC3, (~audfs << 4) + 0x02);
		break;

	case AUD18BIT:
		dptxwr(it6505, 0xC3, (~audfs << 4) + 0x04);
		break;

	case AUD20BIT:
		dptxwr(it6505, 0xC3, (~audfs << 4) + 0x03);
		break;

	case AUD24BIT:
		dptxwr(it6505, 0xC3, (~audfs << 4) + 0x0B);
		break;
	}
}

static void dptx_set_audio_infoframe(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;

	dptxwr(it6505, 0xf7, it6505->aud_ch - 1);

	switch (it6505->aud_ch) {
	case 2:
		dptxwr(it6505, 0xF9, 0x00);
		break;
	case 3:
		dptxwr(it6505, 0xF9, 0x01);
		break;
	case 4:
		dptxwr(it6505, 0xF9, 0x03);
		break;
	case 5:
		dptxwr(it6505, 0xF9, 0x07);
		break;
	case 6:
		dptxwr(it6505, 0xF9, 0x0B);
		break;
	case 7:
		dptxwr(it6505, 0xF9, 0x0F);
		break;
	case 8:
		dptxwr(it6505, 0xF9, 0x1F);
		break;
	default:
		DRM_DEV_ERROR(dev, "audio channel number error: %u",
			      it6505->aud_ch);
	}
	/* Enable Audio InfoFrame */
	dptxset(it6505, 0xE8, 0x22, 0x22);
}

static void it6505_set_audio(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;
	unsigned int regbe;

	/* Audio Clock Domain Reset */
	dptxset(it6505, 0x05, 0x02, 0x02);
	/* Audio mute */
	dptxset(it6505, 0xD3, 0x20, 0x20);
	/* Release Audio Clock Domain Reset */
	dptxset(it6505, 0x05, 0x02, 0x00);

	dptxrd(it6505, 0xBE, &regbe);
	if (regbe == 0xFF)
		DRM_DEV_DEBUG_DRIVER(dev, "no audio input");
	else
		DRM_DEV_DEBUG_DRIVER(dev, "audio input fs: %d.%d kHz",
				     6750 / regbe, 67500 % regbe);
	dptx_set_aud_fmt(it6505);
	dptx_set_aud_chsts(it6505);
	dptx_set_audio_infoframe(it6505);

	/* Enable Enhanced Audio TimeStmp Mode */
	dptxset(it6505, 0xD4, 0x04, 0x04);
	/* Disable Full Audio Packet */
	dptxset(it6505, 0xBB, 0x10, 0x00);

	dptxwr(it6505, 0xDE, 0x00);
	dptxwr(it6505, 0xDF, 0x80);
	dptxwr(it6505, 0xE0, 0x00);
	dptxset(it6505, 0xB8, 0x80, 0x80);
	dptxset(it6505, 0xB8, 0x80, 0x00);
	dptxset(it6505, 0xD3, 0x20, 0x00);
}

static void it6505_delayed_audio(struct work_struct *work)
{
	struct it6505 *it6505 = container_of(work, struct it6505,
					     delayed_audio.work);

	it6505_set_audio(it6505);
	it6505->en_audio = 1;
}

static int it6505_audio_hw_params(struct device *dev, void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	unsigned int channel_num = params->cea.channels;

	DRM_DEV_DEBUG_DRIVER(dev, "setting %d Hz, %d bit, %d channels\n",
			     params->sample_rate, params->sample_width,
			     channel_num);

	if (!it6505->bridge.encoder)
		return -ENODEV;

	switch (params->sample_rate) {
	case 24000:
		it6505->aud_fs = AUD24K;
		break;
	case 32000:
		it6505->aud_fs = AUD32K;
		break;
	case 44100:
		it6505->aud_fs = AUD44P1K;
		break;
	case 48000:
		it6505->aud_fs = AUD48K;
		break;
	case 88200:
		it6505->aud_fs = AUD88P2K;
		break;
	case 96000:
		it6505->aud_fs = AUD96K;
		break;
	case 176400:
		it6505->aud_fs = AUD176P4K;
		break;
	case 192000:
		it6505->aud_fs = AUD192K;
		break;
	default:
		DRM_DEV_DEBUG_DRIVER(dev, "sample rate: %d Hz not support",
				     params->sample_rate);
		return -EINVAL;
	}

	switch (params->sample_width) {
	case 16:
		it6505->audwordlength = AUD16BIT;
		break;
	case 18:
		it6505->audwordlength = AUD18BIT;
		break;
	case 20:
		it6505->audwordlength = AUD20BIT;
		break;
	case 24:
	case 32:
		it6505->audwordlength = AUD24BIT;
		break;
	default:
		DRM_DEV_DEBUG_DRIVER(dev, "wordlength: %d bit not support",
				     params->sample_width);
		return -EINVAL;
	}

	if (channel_num == 0 || channel_num % 2) {
		DRM_DEV_DEBUG_DRIVER(dev, "channel number: %d not support",
				     channel_num);
		return -EINVAL;
	}
	it6505->aud_ch = channel_num;

	return 0;
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

	dptxset(it6505, 0xD3, 0x20, 0x20);
	dptxset(it6505, 0xB8, 0x0F, 0x00);
	dptxset(it6505, 0xE8, 0x22, 0x00);
	dptxset(it6505, 0x05, 0x02, 0x02);
	it6505->en_audio = 0;
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

/***************************************************************************
 * DPCD Read and EDID
 ***************************************************************************/

static unsigned int dptx_dpcdrd(struct it6505 *it6505, unsigned long offset)
{
	u8 value;
	int ret;
	struct device *dev = &it6505->client->dev;

	ret = drm_dp_dpcd_readb(&it6505->aux, offset, &value);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "DPCD read failed [0x%lx] ret: %d", offset,
			      ret);
		return 0;
	}
	return value;
}

static int dptx_dpcdwr(struct it6505 *it6505, unsigned long offset,
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

static int it6505_get_dpcd(struct it6505 *it6505, int offset, u8 *dpcd, int num)
{
	int i, ret;
	struct device *dev = &it6505->client->dev;

	for (i = 0; i < num; i += 4) {
		ret = drm_dp_dpcd_read(&it6505->aux, offset + i, dpcd + i,
				       min(num - i, 4));
		if (ret < 0)
			return ret;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "DPCD[0x%x] = %*ph", offset, num, dpcd);
	return 0;
}

static void it6505_parse_dpcd(struct it6505 *it6505)
{
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	struct device *dev = &it6505->client->dev;
	int bcaps;
	struct drm_dp_link *link = &it6505->link;

	drm_dp_link_probe(&it6505->aux, link);
	it6505_get_dpcd(it6505, DP_DPCD_REV, dpcd, ARRAY_SIZE(dpcd));

	DRM_DEV_DEBUG_DRIVER(dev, "#########DPCD Rev.: %d.%d###########",
			     link->revision >> 4, link->revision & 0x0F);

	switch (link->rate) {
	case 162000:
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Link Rate: 1.62Gbps per lane");
		if (it6505->hbr) {
			DRM_DEV_DEBUG_DRIVER(
				dev, "Not support HBR Mode, will train LBR");
			it6505->hbr = false;
		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Training LBR");
		}
		break;

	case 270000:
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Link Rate: 2.7Gbps per lane");
		if (!it6505->hbr) {
			DRM_DEV_DEBUG_DRIVER(
				dev, "Support HBR Mode, will train LBR");
			it6505->hbr = false;
		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Training HBR");
		}
		break;

	case 540000:
		/* TODO(pihsun): Check if this is correct. HBR here? */
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Link Rate: 2.7Gbps per lane");
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
		if (link->num_lanes > LANE)
			link->num_lanes = LANE;
		DRM_DEV_DEBUG_DRIVER(dev, "Training %u lane", link->num_lanes);
	} else {
		DRM_DEV_ERROR(dev, "Lane Count Error: %u", link->num_lanes);
	}

	if (link->capabilities & DP_LINK_CAP_ENHANCED_FRAMING)
		DRM_DEV_DEBUG_DRIVER(dev, "Support Enhanced Framing");
	else
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Can not support Enhanced Framing Mode");

	if (dpcd[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5) {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Down-Spread: 0.5, support SSC!");
	} else {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "Maximum Down-Spread: 0, No support SSC!");
		it6505->en_ssc = 0;
	}

	if (link->revision >= 0x11 &&
	    dpcd[DP_MAX_DOWNSPREAD] & DP_NO_AUX_HANDSHAKE_LINK_TRAINING)
		DRM_DEV_DEBUG_DRIVER(dev, "Support No AUX Training");
	else
		DRM_DEV_DEBUG_DRIVER(dev, "Can not support No AUX Training");

	bcaps = dptx_dpcdrd(it6505, DP_AUX_HDCP_BCAPS);
	if (bcaps & DP_BCAPS_HDCP_CAPABLE) {
		DRM_DEV_DEBUG_DRIVER(dev, "Sink support HDCP!");
		it6505->cp_capable = true;
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "Sink not support HDCP!");
		it6505->cp_capable = false;
		it6505->en_hdcp = 0;
	}

	if (bcaps & DP_BCAPS_REPEATER_PRESENT) {
		DRM_DEV_DEBUG_DRIVER(dev, "Downstream is repeater!!");
		it6505->downstream_repeater = true;
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "Downstream is receiver!!");
		it6505->downstream_repeater = false;
	}
}

static void it6505_enable_hdcp(struct it6505 *it6505)
{
	u8 bksvs[5], c;
	struct device *dev = &it6505->client->dev;

	/* Disable CP_Desired */
	dptxset(it6505, 0x38, 0x0B, 0x00);
	dptxset(it6505, 0x05, 0x10, 0x10);

	usleep_range(1000, 1500);
	c = dptx_dpcdrd(it6505, DP_AUX_HDCP_BCAPS);
	DRM_DEV_DEBUG_DRIVER(dev, "DPCD[0x68028]: 0x%x\n", c);
	if (!c)
		return;

	dptxset(it6505, 0x05, 0x10, 0x00);
	/* Disable CP_Desired */
	dptxset(it6505, 0x38, 0x01, 0x00);
	/* Use R0' 100ms waiting */
	dptxset(it6505, 0x38, 0x08, 0x00);
	/* clear the repeater List Chk Done and fail bit */
	dptxset(it6505, 0x39, 0x30, 0x00);

	it6505_get_dpcd(it6505, DP_AUX_HDCP_BKSV, bksvs, ARRAY_SIZE(bksvs));

	DRM_DEV_DEBUG_DRIVER(dev, "Sink BKSV = %5ph", bksvs);

	/* Select An Generator */
	dptxset(it6505, 0x3A, 0x01, 0x01);
	/* Enable An Generator */
	dptxset(it6505, 0x3A, 0x02, 0x02);
	/* delay1ms(10);*/
	usleep_range(10000, 15000);
	/* Stop An Generator */
	dptxset(it6505, 0x3A, 0x02, 0x00);

	dptxset(it6505, 0x38, 0x01, 0x01);
	dptxset(it6505, 0x39, 0x01, 0x01);
}

static void it6505_lanespeed_setup(struct it6505 *it6505)
{
	if (!it6505->hbr) {
		dptxset(it6505, 0x16, 0x01, 0x01);
		dptxset(it6505, 0x5C, 0x02, 0x00);
	} else {
		dptxset(it6505, 0x16, 0x01, 0x00);
		dptxset(it6505, 0x5C, 0x02, 0x02);
	}
}

static void it6505_lane_swap(struct it6505 *it6505)
{
	int err;
	union extcon_property_value property;
	struct device *dev = &it6505->client->dev;

	if (!it6505->laneswap_disabled) {
		err = extcon_get_property(it6505->extcon, EXTCON_DISP_DP,
					  EXTCON_PROP_USB_TYPEC_POLARITY,
					  &property);
		if (err) {
			DRM_DEV_ERROR(dev, "get property fail!");
			return;
		}
		it6505->laneswap = property.intval;
	}

	dptxset(it6505, 0x16, 0x08, it6505->laneswap ? 0x08 : 0x00);
	dptxset(it6505, 0x16, 0x06, (it6505->link.num_lanes - 1) << 1);
	DRM_DEV_DEBUG_DRIVER(dev, "it6505->laneswap = 0x%x", it6505->laneswap);

	if (it6505->laneswap) {
		switch (it6505->link.num_lanes) {
		case 1:
			dptxset(it6505, 0x5C, 0xF1, 0x81);
			break;
		case 2:
			dptxset(it6505, 0x5C, 0xF1, 0xC1);
			break;
		default:
			dptxset(it6505, 0x5C, 0xF1, 0xF1);
			break;
		}
	} else {
		switch (it6505->link.num_lanes) {
		case 1:
			dptxset(it6505, 0x5C, 0xF1, 0x11);
			break;
		case 2:
			dptxset(it6505, 0x5C, 0xF1, 0x31);
			break;
		default:
			dptxset(it6505, 0x5C, 0xF1, 0xF1);
			break;
		}
	}
}

static void it6505_lane_config(struct it6505 *it6505)
{
	it6505_lanespeed_setup(it6505);
	it6505_lane_swap(it6505);
}

static void it6505_set_ssc(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;

	dptxset(it6505, 0x16, 0x10, it6505->en_ssc << 4);
	if (it6505->en_ssc) {
		DRM_DEV_DEBUG_DRIVER(dev, "Enable 27M 4463 PPM SSC");
		dptx_chgbank(it6505, 1);
		dptxwr(it6505, 0x88, 0x9e);
		dptxwr(it6505, 0x89, 0x1c);
		dptxwr(it6505, 0x8A, 0x42);
		dptx_chgbank(it6505, 0);
		dptxwr(it6505, 0x58, 0x07);
		dptxwr(it6505, 0x59, 0x29);
		dptxwr(it6505, 0x5A, 0x03);
		/* Stamp Interrupt Step */
		dptxset(it6505, 0xD4, 0x30, 0x10);
		dptx_dpcdwr(it6505, DP_DOWNSPREAD_CTRL, DP_SPREAD_AMP_0_5);
	} else {
		dptx_dpcdwr(it6505, DP_DOWNSPREAD_CTRL, 0x00);
		dptxset(it6505, 0xD4, 0x30, 0x00);
	}
}

static void pclk_phase(struct it6505 *it6505)
{
	dptxset(it6505, 0x10, 0x03, PCLK_DELAY);
	dptxset(it6505, 0x12, 0x10, PCLK_INV << 4);
}

static void afe_driving_setting(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;
	unsigned int afe_setting;

	afe_setting = AFE_SETTING;
	if (afe_setting >= ARRAY_SIZE(afe_setting_table)) {
		DRM_DEV_ERROR(dev, "afe setting value error and use default");
		afe_setting = 0;
	}
	if (afe_setting) {
		dptx_chgbank(it6505, 1);
		dptxwr(it6505, 0x7E, afe_setting_table[afe_setting][0]);
		dptxwr(it6505, 0x7F, afe_setting_table[afe_setting][1]);
		dptxwr(it6505, 0x81, afe_setting_table[afe_setting][2]);
		dptx_chgbank(it6505, 0);
	}
}

static void dptx_output(struct it6505 *it6505)
{
	/* change bank 0 */
	dptx_chgbank(it6505, 0);
	dptxwr(it6505, 0x64, 0x10);
	dptxwr(it6505, 0x65, 0x80);
	dptxwr(it6505, 0x66, 0x10);
	dptxwr(it6505, 0x67, 0x4F);
	dptxwr(it6505, 0x68, 0x09);
	dptxwr(it6505, 0x69, 0xBA);
	dptxwr(it6505, 0x6A, 0x3B);
	dptxwr(it6505, 0x6B, 0x4B);
	dptxwr(it6505, 0x6C, 0x3E);
	dptxwr(it6505, 0x6D, 0x4F);
	dptxwr(it6505, 0x6E, 0x09);
	dptxwr(it6505, 0x6F, 0x56);
	dptxwr(it6505, 0x70, 0x0E);
	dptxwr(it6505, 0x71, 0x00);
	dptxwr(it6505, 0x72, 0x00);
	dptxwr(it6505, 0x73, 0x4F);
	dptxwr(it6505, 0x74, 0x09);
	dptxwr(it6505, 0x75, 0x00);
	dptxwr(it6505, 0x76, 0x00);
	dptxwr(it6505, 0x77, 0xE7);
	dptxwr(it6505, 0x78, 0x10);
	dptxwr(it6505, 0xE8, 0x00);
	dptxset(it6505, 0xCE, 0x70, 0x60);
	dptxwr(it6505, 0xCA, 0x4D);
	dptxwr(it6505, 0xC9, 0xF5);
	dptxwr(it6505, 0x5C, 0x02);

	drm_dp_link_power_up(&it6505->aux, &it6505->link);
	dptxset(it6505, 0x59, 0x01, 0x01);
	dptxset(it6505, 0x5A, 0x05, 0x01);
	dptxwr(it6505, 0x12, 0x01);
	dptxwr(it6505, 0xCB, 0x17);
	dptxwr(it6505, 0x11, 0x09);
	dptxwr(it6505, 0x20, 0x28);
	dptxset(it6505, 0x23, 0x30, 0x00);
	dptxset(it6505, 0x3A, 0x04, 0x04);
	dptxset(it6505, 0x15, 0x01, 0x01);
	dptxwr(it6505, 0x0C, 0x08);

	dptxset(it6505, 0x5F, 0x20, 0x00);
	it6505_lane_config(it6505);

	it6505_set_ssc(it6505);

	if (it6505->link.capabilities & DP_LINK_CAP_ENHANCED_FRAMING) {
		dptxwr(it6505, 0xD3, 0x33);
		dptx_dpcdwr(it6505, DP_LANE_COUNT_SET,
			    DP_LANE_COUNT_ENHANCED_FRAME_EN);
	} else {
		dptxwr(it6505, 0xD3, 0x32);
	}

	dptxset(it6505, 0x15, 0x02, 0x02);
	dptxset(it6505, 0x15, 0x02, 0x00);
	dptxset(it6505, 0x05, 0x03, 0x02);
	dptxset(it6505, 0x05, 0x03, 0x00);

	/* reg60[2] = InDDR */
	dptxwr(it6505, 0x60, 0x44);
	/* M444B24 format */
	dptxwr(it6505, 0x62, 0x01);
	/* select RGB Bypass CSC */
	dptxwr(it6505, 0x63, 0x00);

	pclk_phase(it6505);
	dptxset(it6505, 0x61, 0x01, 0x01);
	dptxwr(it6505, 0x06, 0xFF);
	dptxwr(it6505, 0x07, 0xFF);
	dptxwr(it6505, 0x08, 0xFF);

	dptxset(it6505, 0xd3, 0x30, 0x00);
	dptxset(it6505, 0xd4, 0x41, 0x41);
	dptxset(it6505, 0xe8, 0x11, 0x11);

	afe_driving_setting(it6505);
	dptxwr(it6505, 0x17, 0x04);
	dptxwr(it6505, 0x17, 0x01);
}

static void dptx_process_sys_wait(struct it6505 *it6505)
{
	int reg0e;
	struct device *dev = &it6505->client->dev;

	dptxrd(it6505, 0x0E, &reg0e);
	DRM_DEV_DEBUG_DRIVER(dev, "SYS_WAIT state reg0e=0x%02x", reg0e);

	if (reg0e & BIT(4)) {
		DRM_DEV_DEBUG_DRIVER(dev, "Auto Link Training Success...");
		DRM_DEV_DEBUG_DRIVER(dev, "Link State: 0x%x", reg0e & 0x1F);
		if (it6505->en_audio) {
			DRM_DEV_DEBUG_DRIVER(dev, "Enable audio!");
			it6505_set_audio(it6505);
		}
		DRM_DEV_DEBUG_DRIVER(dev, "it6505->VidStable_Done = %02x",
				     it6505->vidstable_done);
		if (it6505->cp_capable) {
			DRM_DEV_DEBUG_DRIVER(dev, "Support HDCP");
			if (it6505->en_hdcp) {
				DRM_DEV_DEBUG_DRIVER(dev, "Enable HDCP");
				dptx_sys_chg(it6505, SYS_ReHDCP);
			} else {
				DRM_DEV_DEBUG_DRIVER(dev, "Disable HDCP");
				dptx_sys_chg(it6505, SYS_NOROP);
			}
		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Not support HDCP");
			dptx_sys_chg(it6505, SYS_NOROP);
		}
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "Auto Link Training fail step %d",
				     it6505->cntfsm);
		dptx_debug_print(it6505, 0x0D, "system status");
		if (it6505->cntfsm > 0) {
			it6505->cntfsm--;
			dptx_sys_chg(it6505, SYS_AUTOTRAIN);
		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Auto Training Fail %d times",
					     TRAINFAILCNT);
			DRM_DEV_DEBUG_DRIVER(dev, "Sys change to SYS_HPD");
			dptx_dpcdwr(it6505, DP_TRAINING_PATTERN_SET, 0x00);
			if (it6505->train_fail_hpd > 0) {
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
	int reg0e;
	struct device *dev = &it6505->client->dev;

	if (it6505->status != SYS_UNPLUG && !dptx_getsinkhpd(it6505))
		dptx_sys_chg(it6505, SYS_UNPLUG);

	DRM_DEV_DEBUG_DRIVER(dev, "state: %s", state_string[it6505->status]);
	switch (it6505->status) {
	case SYS_UNPLUG:
		drm_helper_hpd_irq_event(it6505->connector.dev);
		break;

	case SYS_HPD:
		dptx_debug_print(it6505, 0x9F, "aux status");
		drm_helper_hpd_irq_event(it6505->connector.dev);
		it6505_init_fsm(it6505);
		/* GETDPCD */
		it6505_parse_dpcd(it6505);

		/*
		 * training fail TRAINFAILCNT times,
		 * then change to HPD to restart
		 */
		it6505->cntfsm = TRAINFAILCNT;
		DRM_DEV_DEBUG_DRIVER(dev, "will Train %s %d lanes",
				     it6505->hbr ? "HBR" : "LBR",
				     it6505->link.num_lanes);
		dptx_sys_chg(it6505, SYS_AUTOTRAIN);
		break;

	case SYS_AUTOTRAIN:
		dptx_output(it6505);

		/*
		 * waiting for training down flag
		 * because we don't know
		 * how long this step will be completed
		 * so use step 1ms to wait
		 */
		for (i = 0; i < FLAGTRAINDOWN; i++) {
			usleep_range(1000, 2000);
			dptxrd(it6505, 0x0E, &reg0e);
			if (reg0e & BIT(4))
				break;
		}

		dptx_sys_chg(it6505, SYS_WAIT);
		break;

	case SYS_WAIT:
		dptx_process_sys_wait(it6505);
		break;

	case SYS_ReHDCP:
		msleep(2400);
		dptx_debug_print(it6505, 0x3B, "ar0_low");
		dptx_debug_print(it6505, 0x3C, "ar0_high");
		dptx_debug_print(it6505, 0x45, "br0_low");
		dptx_debug_print(it6505, 0x46, "br0_high");
		it6505_enable_hdcp(it6505);
		DRM_DEV_DEBUG_DRIVER(dev, "SYS_ReHDCP end");
		break;

	case SYS_NOROP:
		break;

	case SYS_TRAINFAIL:
		break;

	default:
		DRM_DEV_ERROR(dev, "sys_state change to unknown_state: %d",
			      it6505->status);
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
	unsigned int value;
	u8 am0[8], binfo[2];
	struct device *dev = &it6505->client->dev;

	dptxset(it6505, 0x3A, 0x20, 0x20);
	for (i = 0; i < 8; i++) {
		dptxrd(it6505, 0x4C + i, &value);
		am0[i] = value;
	}
	DRM_DEV_DEBUG_DRIVER(dev, "read am0 = %8ph", am0);
	dptxset(it6505, 0x3A, 0x20, 0x00);

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
				it6505->shainput + msgcnt, 5);
		msgcnt += 5;

		DRM_DEV_DEBUG_DRIVER(dev, "KSV List %d device : %5ph", i,
				     it6505->shainput + i * 5);
	}
	it6505->shainput[msgcnt++] = binfo[0];
	it6505->shainput[msgcnt++] = binfo[1];
	for (i = 0; i < 8; i++)
		it6505->shainput[msgcnt++] = am0[i];

	DRM_DEV_DEBUG_DRIVER(dev, "SHA Message Count = %d", msgcnt);
	return msgcnt;
}

static void it6505_check_sha1_result(struct it6505 *it6505)
{
	unsigned int i, shapass;
	struct device *dev = &it6505->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "SHA calculate complete");
	for (i = 0; i < 5; i++)
		DRM_DEV_DEBUG_DRIVER(dev, "av %d: %4ph", i, it6505->av[i]);

	shapass = 1;
	for (i = 0; i < 5; i++) {
		it6505_get_dpcd(it6505, DP_AUX_HDCP_V_PRIME(i), it6505->bv[i],
				4);
		DRM_DEV_DEBUG_DRIVER(dev, "bv %d: %4ph", i, it6505->bv[i]);
		if ((it6505->bv[i][3] != it6505->av[i][0]) ||
		    (it6505->bv[i][2] != it6505->av[i][1]) ||
		    (it6505->bv[i][1] != it6505->av[i][2]) ||
		    (it6505->bv[i][0] != it6505->av[i][3])) {
			shapass = 0;
		}
	}
	if (shapass) {
		DRM_DEV_DEBUG_DRIVER(dev, "SHA check result pass!");
		dptxset(it6505, 0x39, BIT(4), BIT(4));
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "SHA check result fail");
		dptxset(it6505, 0x39, BIT(5), BIT(5));
	}
}

static void to_fsm_status(struct it6505 *it6505, enum sys_status status)
{
	while (it6505->status != status && it6505->status != SYS_TRAINFAIL) {
		dptx_sys_fsm(it6505);
		if (it6505->status == SYS_UNPLUG)
			return;
	}
}

static void go_on_fsm(struct it6505 *it6505);

static u8 dp_link_status(const u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}

static void hpd_irq(struct it6505 *it6505)
{
	u8 dpcd_sink_count, dpcd_irq_vector;
	u8 link_status[DP_LINK_STATUS_SIZE];
	unsigned int bstatus;
	struct device *dev = &it6505->client->dev;

	dpcd_sink_count = dptx_dpcdrd(it6505, DP_SINK_COUNT);
	dpcd_irq_vector = dptx_dpcdrd(it6505, DP_DEVICE_SERVICE_IRQ_VECTOR);
	drm_dp_dpcd_read_link_status(&it6505->aux, link_status);

	DRM_DEV_DEBUG_DRIVER(dev, "dpcd_sink_count = 0x%x", dpcd_sink_count);
	DRM_DEV_DEBUG_DRIVER(dev, "dpcd_irq_vector = 0x%x", dpcd_irq_vector);
	DRM_DEV_DEBUG_DRIVER(dev, "link_status = %*ph", (int)ARRAY_SIZE(link_status),
			     link_status);

	if (dpcd_irq_vector & DP_CP_IRQ) {
		bstatus = dptx_dpcdrd(it6505, DP_AUX_HDCP_BSTATUS);
		dptxset(it6505, 0x39, 0x02, 0x02);
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
		if (bstatus & DP_BSTATUS_R0_PRIME_READY)
			DRM_DEV_DEBUG_DRIVER(dev, "HDCP R0' ready");
	}

	if (dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED) &
	    DP_LINK_STATUS_UPDATED) {
		if (!drm_dp_channel_eq_ok(link_status,
					  it6505->link.num_lanes)) {
			DRM_DEV_DEBUG_DRIVER(dev, "Link Re-Training");
			dptxset(it6505, 0xD3, 0x30, 0x30);
			dptxset(it6505, 0xE8, 0x33, 0x00);
			msleep(500);
			dptx_sys_chg(it6505, SYS_HPD);
			go_on_fsm(it6505);
		}
	}
}

static void go_on_fsm(struct it6505 *it6505)
{
	struct device *dev = &it6505->client->dev;

	if (it6505->cp_capable) {
		DRM_DEV_DEBUG_DRIVER(dev, "Support HDCP, cp_capable: %d",
				     it6505->cp_capable);
		if (it6505->en_hdcp) {
			DRM_DEV_DEBUG_DRIVER(dev, "Enable HDCP");
			to_fsm_status(it6505, SYS_ReHDCP);
			dptx_sys_fsm(it6505);

		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Disable HDCP");
			to_fsm_status(it6505, SYS_NOROP);
		}
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "Not support HDCP");
		to_fsm_status(it6505, SYS_NOROP);
	}
}

static void it6505_check_reg06(struct it6505 *it6505, unsigned int reg06)
{
	unsigned int rddata;
	struct device *dev = &it6505->client->dev;

	if (reg06 & BIT(0)) {
		/* hpd pin status change */
		DRM_DEV_DEBUG_DRIVER(dev, "HPD Change Interrupt");
		if (dptx_getsinkhpd(it6505)) {
			dptx_sys_chg(it6505, SYS_HPD);
			dptx_sys_fsm(it6505);
			dptxrd(it6505, 0x0D, &rddata);
			if (rddata & BIT(2)) {
				go_on_fsm(it6505);
			} else {
				dptxwr(it6505, 0x05, 0x00);
				dptxset(it6505, 0x61, 0x02, 0x02);
				dptxset(it6505, 0x61, 0x02, 0x00);
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
			it6505->cp_done = 1;
			dptx_sys_chg(it6505, SYS_NOROP);
		}

		if (reg06 & BIT(3)) {
			DRM_DEV_DEBUG_DRIVER(
				dev, "HDCP encryption Fail Interrupt, retry");
			it6505->cp_done = 0;
			it6505_init_and_mask_on(it6505);
		}
	}

	if (reg06 & BIT(1)) {
		DRM_DEV_DEBUG_DRIVER(dev, "HPD IRQ Interrupt");
		hpd_irq(it6505);
	}

	if (reg06 & BIT(2)) {
		dptxrd(it6505, 0x0D, &rddata);

		if (rddata & BIT(2)) {
			DRM_DEV_DEBUG_DRIVER(dev, "Video Stable On Interrupt");
			it6505->vidstable_done = 1;
			dptx_sys_chg(it6505, SYS_AUTOTRAIN);
			go_on_fsm(it6505);
		} else {
			DRM_DEV_DEBUG_DRIVER(dev, "Video Stable Off Interrupt");
			it6505->vidstable_done = 0;
		}
	}
}

static void it6505_check_reg07(struct it6505 *it6505, unsigned int reg07)
{
	struct device *dev = &it6505->client->dev;
	unsigned int len, i;
	unsigned int bstatus;

	if (it6505->status == SYS_UNPLUG)
		return;
	if (reg07 & BIT(0))
		DRM_DEV_DEBUG_DRIVER(dev, "AUX PC Request Fail Interrupt");

	if (reg07 & BIT(1)) {
		DRM_DEV_DEBUG_DRIVER(dev, "HDCP event Interrupt");
		for (i = 0; i < POLLINGKSVLIST; i++) {
			bstatus = dptx_dpcdrd(it6505, DP_AUX_HDCP_BSTATUS);
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
			sha1_digest(it6505, it6505->shainput, len,
				    (u8 *)it6505->av);
			it6505_check_sha1_result(it6505);
		}
	}
	if (it6505->en_audio && (reg07 & BIT(2))) {
		DRM_DEV_DEBUG_DRIVER(dev, "Audio FIFO OverFlow Interrupt");
		dptx_debug_print(it6505, 0xBE, "");
		dptxset(it6505, 0xD3, 0x20, 0x20);
		dptxset(it6505, 0xE8, 0x22, 0x00);
		dptxset(it6505, 0xB8, 0x80, 0x80);
		dptxset(it6505, 0xB8, 0x80, 0x00);
		it6505_set_audio(it6505);
	}
	if (it6505->en_audio && (reg07 & BIT(5)))
		DRM_DEV_DEBUG_DRIVER(dev, "Audio infoframe packet done");
}

static void it6505_check_reg08(struct it6505 *it6505, unsigned int reg08)
{
	struct device *dev = &it6505->client->dev;

	if (it6505->status == SYS_UNPLUG)
		return;
	if (it6505->en_audio && (reg08 & BIT(1)))
		DRM_DEV_DEBUG_DRIVER(dev, "Audio TimeStamp Packet Done!");

	if (it6505->en_audio && (reg08 & BIT(3))) {
		DRM_DEV_DEBUG_DRIVER(dev, "Audio M Error Interrupt ...");
		show_aud_mcnt(it6505);
	}
	if (reg08 & BIT(4)) {
		DRM_DEV_DEBUG_DRIVER(dev, "Link Training Fail Interrupt");
		/* restart training */
		dptx_sys_chg(it6505, SYS_AUTOTRAIN);
		go_on_fsm(it6505);
	}

	if (reg08 & BIT(7)) {
		DRM_DEV_DEBUG_DRIVER(dev, "IO Latch FIFO OverFlow Interrupt");
		dptxset(it6505, 0x61, 0x02, 0x02);
		dptxset(it6505, 0x61, 0x02, 0x00);
	}
}

static void it6505_dptx_irq(struct it6505 *it6505)
{
	unsigned int reg06, reg07, reg08, reg0d;
	struct device *dev = &it6505->client->dev;

	dptxrd(it6505, 0x06, &reg06);
	dptxrd(it6505, 0x07, &reg07);
	dptxrd(it6505, 0x08, &reg08);
	dptxrd(it6505, 0x0D, &reg0d);

	dptxwr(it6505, 0x06, reg06);
	dptxwr(it6505, 0x07, reg07);
	dptxwr(it6505, 0x08, reg08);

	DRM_DEV_DEBUG_DRIVER(dev, "reg06 = 0x%02x", reg06);
	DRM_DEV_DEBUG_DRIVER(dev, "reg07 = 0x%02x", reg07);
	DRM_DEV_DEBUG_DRIVER(dev, "reg08 = 0x%02x", reg08);
	DRM_DEV_DEBUG_DRIVER(dev, "reg0d = 0x%02x", reg0d);

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

	if (!it6505->drv_hold) {
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
	dptxwr(it6505, 0x06, 0xFF);
	dptxwr(it6505, 0x07, 0xFF);
	dptxwr(it6505, 0x08, 0xFF);
}

static irqreturn_t it6505_intp_threaded_handler(int unused, void *data)
{
	struct it6505 *it6505 = data;
	struct device *dev = &it6505->client->dev;

	msleep(150);
	mutex_lock(&it6505->lock);

	if (it6505->drv_hold == 0 && it6505->powered) {
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

static ssize_t drv_hold_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->drv_hold);
}

static ssize_t drv_hold_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &it6505->drv_hold) < 0)
		return -EINVAL;

	if (it6505->drv_hold) {
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
	struct drm_display_mode *vid = &it6505->vid_info;
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
	for (i = 0; i < ARRAY_SIZE(it6505->shainput); i += 16)
		str += scnprintf(str, end - str, "%16ph\n",
				 it6505->shainput + i);

	str += scnprintf(str, end - str, "av:\n");
	for (i = 0; i < ARRAY_SIZE(it6505->av); i++)
		str += scnprintf(str, end - str, "%4ph\n", it6505->av[i]);

	str += scnprintf(str, end - str, "bv:\n");
	for (i = 0; i < ARRAY_SIZE(it6505->bv); i++)
		str += scnprintf(str, end - str, "%4ph\n", it6505->bv[i]);

	return end - str;
}

static ssize_t en_hdcp_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->en_hdcp);
}

static ssize_t en_hdcp_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);
	unsigned int reg3f, hdcp;

	if (kstrtoint(buf, 10, &it6505->en_hdcp) < 0)
		return -EINVAL;

	if (!it6505->powered || it6505->status == SYS_UNPLUG) {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "power down or unplug, can not fire HDCP");
		return -EINVAL;
	}

	if (it6505->en_hdcp) {
		if (it6505->cp_capable) {
			dptx_sys_chg(it6505, SYS_ReHDCP);
			dptx_sys_fsm(it6505);
		} else {
			DRM_DEV_ERROR(dev, "sink not support HDCP");
			it6505->cp_done = 0;
		}
	} else {
		dptxset(it6505, 0x05, 0x10, 0x10);
		dptxset(it6505, 0x05, 0x10, 0x00);
		dptxrd(it6505, 0x3F, &reg3f);
		hdcp = (reg3f & BIT(7)) >> 7;
		DRM_DEV_DEBUG_DRIVER(dev, "%s to disable hdcp",
				     hdcp ? "failed" : "succeeded");
		it6505->cp_done = hdcp;
	}
	return count;
}

static ssize_t en_pwronoff_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->en_pwronoff);
}

static ssize_t en_pwronoff_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &it6505->en_pwronoff) < 0)
		return -EINVAL;
	return count;
}

static ssize_t en_audio_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->en_audio);
}

static ssize_t en_audio_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &it6505->en_audio) < 0)
		return -EINVAL;
	if (!it6505->powered || it6505->status == SYS_UNPLUG) {
		DRM_DEV_DEBUG_DRIVER(
			dev, "power down or unplug, can not output audio");
		return -EINVAL;
	}

	if (it6505->en_audio) {
		it6505_set_audio(it6505);
	} else {
		dptxset(it6505, 0x05, 0x02, 0x02);
		dptxset(it6505, 0x05, 0x02, 0x00);
	}
	DRM_DEV_DEBUG_DRIVER(dev, "%s audio",
			     it6505->en_audio ? "Enable" : "Disable");
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

static ssize_t pwr_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct it6505 *it6505 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", it6505->powered);
}

static DEVICE_ATTR_RO(print_timing);
static DEVICE_ATTR_RO(pwr_status);
static DEVICE_ATTR_RO(sha_debug);
static DEVICE_ATTR_WO(force_pwronoff);
static DEVICE_ATTR_RW(drv_hold);
static DEVICE_ATTR_RW(en_hdcp);
static DEVICE_ATTR_RW(en_pwronoff);
static DEVICE_ATTR_RW(en_audio);

static const struct attribute *it6505_attrs[] = {
	&dev_attr_drv_hold.attr,
	&dev_attr_print_timing.attr,
	&dev_attr_sha_debug.attr,
	&dev_attr_en_hdcp.attr,
	&dev_attr_en_pwronoff.attr,
	&dev_attr_en_audio.attr,
	&dev_attr_force_pwronoff.attr,
	&dev_attr_pwr_status.attr,
	NULL,
};

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

	it6505->laneswap_disabled =
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

	it6505->en_pwronoff = DEFAULTPWRONOFF;
	it6505->drv_hold = DEFAULTDRVHOLD;
	it6505->en_hdcp = DEFAULTHDCP;
	it6505->en_audio = DEFAULTAUDIO;
	it6505->powered = 0;

	if (DEFAULTPWRON)
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

	drm_bridge_remove(&it6505->bridge);
	sysfs_remove_files(&client->dev.kobj, it6505_attrs);
	kfree(it6505->edid);
	it6505->edid = NULL;
	drm_connector_unregister(&it6505->connector);
	drm_dp_aux_unregister(&it6505->aux);
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
	},
	.probe = it6505_i2c_probe,
	.remove = it6505_remove,
	.id_table = it6505_id,
};

module_i2c_driver(it6505_i2c_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_DESCRIPTION("IT6505 DisplayPort Transmitter driver");
MODULE_LICENSE("GPL v2");
