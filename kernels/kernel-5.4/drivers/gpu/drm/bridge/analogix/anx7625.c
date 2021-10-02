// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(c) 2016, Analogix Semiconductor. All rights reserved.
 *
 */
#include <linux/extcon.h>
#include <linux/gcd.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include <video/display_timing.h>

#include "anx7625.h"

/*
 * there is a sync issue while access I2C register between AP(CPU) and
 * internal firmware(OCM), to avoid the race condition, AP should access
 * the reserved slave address before slave address occurs changes.
 */
static int i2c_access_workaround(struct anx7625_data *ctx,
				 struct i2c_client *client)
{
	u8 offset;
	struct device *dev = &client->dev;
	int ret;

	if (client == ctx->last_client)
		return 0;

	ctx->last_client = client;

	if (client == ctx->i2c.tcpc_client)
		offset = RSVD_00_ADDR;
	else if (client == ctx->i2c.tx_p0_client)
		offset = RSVD_D1_ADDR;
	else if (client == ctx->i2c.tx_p1_client)
		offset = RSVD_60_ADDR;
	else if (client == ctx->i2c.rx_p0_client)
		offset = RSVD_39_ADDR;
	else if (client == ctx->i2c.rx_p1_client)
		offset = RSVD_7F_ADDR;
	else
		offset = RSVD_00_ADDR;

	ret = i2c_smbus_write_byte_data(client, offset, 0x00);
	if (ret < 0)
		DRM_DEV_ERROR(dev,
			      "failed to access i2c id=%x\n:%x",
			      client->addr, offset);

	return ret;
}

static int anx7625_reg_read(struct anx7625_data *ctx,
			    struct i2c_client *client, u8 reg_addr)
{
	int ret;
	struct device *dev = &client->dev;

	i2c_access_workaround(ctx, client);

	ret = i2c_smbus_read_byte_data(client, reg_addr);
	if (ret < 0)
		DRM_DEV_ERROR(dev, "read i2c failed id=%x:%x\n",
			      client->addr, reg_addr);

	return ret;
}

static int anx7625_reg_block_read(struct anx7625_data *ctx,
				  struct i2c_client *client,
				  u8 reg_addr, u8 len, u8 *buf)
{
	int ret;
	struct device *dev = &client->dev;

	i2c_access_workaround(ctx, client);

	ret = i2c_smbus_read_i2c_block_data(client, reg_addr, len, buf);
	if (ret < 0)
		DRM_DEV_ERROR(dev, "read i2c block failed id=%x:%x\n",
			      client->addr, reg_addr);

	return ret;
}

static int anx7625_reg_write(struct anx7625_data *ctx,
			     struct i2c_client *client,
			     u8 reg_addr, u8 reg_val)
{
	int ret;
	struct device *dev = &client->dev;

	i2c_access_workaround(ctx, client);

	ret = i2c_smbus_write_byte_data(client, reg_addr, reg_val);

	if (ret < 0)
		DRM_DEV_ERROR(dev, "failed to write i2c id=%x\n:%x",
			      client->addr, reg_addr);

	return ret;
}

static int anx7625_write_or(struct anx7625_data *ctx,
			    struct i2c_client *client,
			    u8 offset, u8 mask)
{
	int val;

	val = anx7625_reg_read(ctx, client, offset);
	if (val < 0)
		return val;

	return anx7625_reg_write(ctx, client, offset, (val | (mask)));
}

static int anx7625_write_and(struct anx7625_data *ctx,
			     struct i2c_client *client,
			     u8 offset, u8 mask)
{
	int val;

	val = anx7625_reg_read(ctx, client, offset);
	if (val < 0)
		return val;

	return anx7625_reg_write(ctx, client, offset, (val & (mask)));
}

static int anx7625_write_and_or(struct anx7625_data *ctx,
				struct i2c_client *client,
				u8 offset, u8 and_mask, u8 or_mask)
{
	int val;

	val = anx7625_reg_read(ctx, client, offset);
	if (val < 0)
		return val;

	return anx7625_reg_write(ctx, client,
				 offset, (val & and_mask) | (or_mask));
}

static int anx7625_config_bit_matrix(struct anx7625_data *ctx)
{
	int i, ret;

	ret = anx7625_reg_write(ctx, ctx->i2c.tx_p2_client,
				AUDIO_CONTROL_REGISTER, 0x80);
	for (i = 0; i < 13; i++)
		ret |= anx7625_reg_write(ctx, ctx->i2c.tx_p2_client,
					 VIDEO_BIT_MATRIX_12 + i,
					 0x18 + i);

	return ret;
}

static int anx7625_read_ctrl_status_p0(struct anx7625_data *ctx)
{
	return anx7625_reg_read(ctx, ctx->i2c.rx_p0_client, AP_AUX_CTRL_STATUS);
}

static int wait_aux_op_finish(struct anx7625_data *ctx)
{
	int val;
	int ret;
	struct device *dev = &ctx->client->dev;

	ret = readx_poll_timeout(anx7625_read_ctrl_status_p0,
				 ctx, val,
				 (!(val & AP_AUX_CTRL_OP_EN) || (val < 0)),
				 2000,
				 2000 * 150);
	if (ret) {
		DRM_DEV_ERROR(dev, "aux operation failed!\n");
		return -EIO;
	}

	val = anx7625_reg_read(ctx, ctx->i2c.rx_p0_client,
			       AP_AUX_CTRL_STATUS);
	if (val < 0 || (val & 0x0F)) {
		DRM_DEV_ERROR(dev, "aux status %02x\n", val);
		val = -EIO;
	}

	return val;
}

static int anx7625_video_mute_control(struct anx7625_data *ctx,
				      u8 status)
{
	int ret;

	if (status) { /* mute on */
		/* set mute flag */
		ret = anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
				       AP_AV_STATUS, AP_MIPI_MUTE);
		/* clear mipi RX en */
		ret |= anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
					 AP_AV_STATUS, (u8)~AP_MIPI_RX_EN);
	} else { /* mute off */
		/* clear mute flag */
		ret = anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
					AP_AV_STATUS, (u8)~AP_MIPI_MUTE);
		/* set MIPI RX EN */
		ret |= anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
					AP_AV_STATUS, AP_MIPI_RX_EN);
	}

	return ret;
}

static int anx7625_config_audio_input(struct anx7625_data *ctx)
{
	int ret;

	/* channel num */
	ret = anx7625_reg_write(ctx, ctx->i2c.tx_p2_client,
				AUDIO_CHANNEL_STATUS_6, I2S_CH_2 << 5);
	/* layout */
	/* as I2S channel is 2, no need to set layout */

	/* FS */
	ret |= anx7625_write_and_or(ctx, ctx->i2c.tx_p2_client,
				    AUDIO_CHANNEL_STATUS_4,
				    0xf0, AUDIO_FS_48K);
	/* word length */
	ret |= anx7625_write_and_or(ctx, ctx->i2c.tx_p2_client,
				    AUDIO_CHANNEL_STATUS_5,
				    0xf0, AUDIO_W_LEN_24_24MAX);
	/* I2S */
	ret |= anx7625_write_or(ctx, ctx->i2c.tx_p2_client,
				AUDIO_CHANNEL_STATUS_6, I2S_SLAVE_MODE);
	ret |= anx7625_write_and(ctx, ctx->i2c.tx_p2_client,
				 AUDIO_CONTROL_REGISTER, ~TDM_TIMING_MODE);
	/* audio change flag */
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
				AP_AV_STATUS, AP_AUDIO_CHG);

	if (ret < 0)
		DRM_ERROR("failed to config audio.\n");

	return ret;
}

/* reduction of fraction a/b */
static void anx7625_reduction_of_a_fraction(unsigned long *a, unsigned long *b)
{
	unsigned long gcd_num;
	unsigned long tmp_a, tmp_b;
	u32 i = 1;

	gcd_num = gcd(*a, *b);
	*a /= gcd_num;
	*b /= gcd_num;

	tmp_a = *a;
	tmp_b = *b;

	while ((*a > MAX_UNSIGNED_24BIT) || (*b > MAX_UNSIGNED_24BIT)) {
		i++;
		*a = tmp_a / i;
		*b = tmp_b / i;
	}

	/*
	 * in the end, make a, b larger to have higher ODFC PLL
	 * output frequency accuracy
	 */
	while ((*a < MAX_UNSIGNED_24BIT) && (*b < MAX_UNSIGNED_24BIT)) {
		*a <<= 1;
		*b <<= 1;
	}

	*a >>= 1;
	*b >>= 1;
}

static int anx7625_calculate_m_n(u32 pixelclock,
				 unsigned long *m,
				 unsigned long *n,
				 u8 *post_divider)
{
	if (pixelclock > PLL_OUT_FREQ_ABS_MAX / POST_DIVIDER_MIN) {
		/* pixel clock frequency is too high */
		DRM_ERROR("pixelclock too high, act(%d), maximum(%lu)\n",
			  pixelclock,
			  PLL_OUT_FREQ_ABS_MAX / POST_DIVIDER_MIN);
		return -EINVAL;
	}

	if (pixelclock < PLL_OUT_FREQ_ABS_MIN / POST_DIVIDER_MAX) {
		/* pixel clock frequency is too low */
		DRM_ERROR("pixelclock too low, act(%d), maximum(%lu)\n",
			  pixelclock,
			  PLL_OUT_FREQ_ABS_MIN / POST_DIVIDER_MAX);
		return -EINVAL;
	}

	for (*post_divider = 1;
		pixelclock < (PLL_OUT_FREQ_MIN / (*post_divider));)
		*post_divider += 1;

	if (*post_divider > POST_DIVIDER_MAX) {
		for (*post_divider = 1;
			(pixelclock <
			 (PLL_OUT_FREQ_ABS_MIN / (*post_divider)));)
			*post_divider += 1;

		if (*post_divider > POST_DIVIDER_MAX) {
			DRM_ERROR("cannot find property post_divider(%d)\n",
				  *post_divider);
			return -EDOM;
		}
	}

	/* patch to improve the accuracy */
	if (*post_divider == 7) {
		/* 27,000,000 is not divisible by 7 */
		*post_divider = 8;
	} else if (*post_divider == 11) {
		/* 27,000,000 is not divisible by 11 */
		*post_divider = 12;
	} else if ((*post_divider == 13) || (*post_divider == 14)) {
		/*27,000,000 is not divisible by 13 or 14*/
		*post_divider = 15;
	}

	if (pixelclock * (*post_divider) > PLL_OUT_FREQ_ABS_MAX) {
		DRM_ERROR("act clock(%u) large than maximum(%lu)\n",
			  pixelclock * (*post_divider),
			  PLL_OUT_FREQ_ABS_MAX);
		return -EDOM;
	}

	*m = pixelclock;
	*n = XTAL_FRQ / (*post_divider);

	anx7625_reduction_of_a_fraction(m, n);

	return 0;
}

static int anx7625_odfc_config(struct anx7625_data *ctx,
			       u8 post_divider)
{
	int ret;

	/* config input reference clock frequency 27MHz/19.2MHz */
	ret = anx7625_write_and(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_16,
				~(REF_CLK_27000KHZ << MIPI_FREF_D_IND));
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_16,
				(REF_CLK_27000KHZ << MIPI_FREF_D_IND));
	/* post divider */
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p1_client,
				 MIPI_DIGITAL_PLL_8, 0x0f);
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_8,
				post_divider << 4);

	/* add patch for MIS2-125 (5pcs ANX7625 fail ATE MBIST test) */
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_7,
				 ~MIPI_PLL_VCO_TUNE_REG_VAL);

	/* reset ODFC PLL */
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_7,
				 ~MIPI_PLL_RESET_N);
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_7,
				MIPI_PLL_RESET_N);

	if (ret < 0)
		DRM_ERROR("IO error.\n");

	return ret;
}

static int anx7625_dsi_video_timing_config(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;
	unsigned long m, n;
	u16 htotal;
	int ret;
	u8 post_divider = 0;

	ret = anx7625_calculate_m_n(ctx->dt.pixelclock.min * 1000,
				    &m, &n, &post_divider);

	if (ret) {
		DRM_ERROR("cannot get property m n value.\n");
		return ret;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "compute M(%lu), N(%lu), divider(%d).\n",
			     m, n, post_divider);

	/* configure pixel clock */
	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client, PIXEL_CLOCK_L,
				(ctx->dt.pixelclock.min / 1000) & 0xFF);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client, PIXEL_CLOCK_H,
				 (ctx->dt.pixelclock.min / 1000) >> 8);
	/* lane count */
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p1_client,
			MIPI_LANE_CTRL_0, 0xfc);
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client,
				MIPI_LANE_CTRL_0, 3);

	/* Htotal */
	htotal = ctx->dt.hactive.min + ctx->dt.hfront_porch.min +
		ctx->dt.hback_porch.min + ctx->dt.hsync_len.min;
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_TOTAL_PIXELS_L, htotal & 0xFF);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_TOTAL_PIXELS_H, htotal >> 8);
	/* Hactive */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_ACTIVE_PIXELS_L, ctx->dt.hactive.min & 0xFF);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_ACTIVE_PIXELS_H, ctx->dt.hactive.min >> 8);
	/* HFP */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_FRONT_PORCH_L, ctx->dt.hfront_porch.min);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_FRONT_PORCH_H,
			ctx->dt.hfront_porch.min >> 8);
	/* HWS */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_SYNC_WIDTH_L, ctx->dt.hsync_len.min);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_SYNC_WIDTH_H, ctx->dt.hsync_len.min >> 8);
	/* HBP */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_BACK_PORCH_L, ctx->dt.hback_porch.min);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			HORIZONTAL_BACK_PORCH_H, ctx->dt.hback_porch.min >> 8);
	/* Vactive */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client, ACTIVE_LINES_L,
			ctx->dt.vactive.min);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client, ACTIVE_LINES_H,
			ctx->dt.vactive.min >> 8);
	/* VFP */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			VERTICAL_FRONT_PORCH, ctx->dt.vfront_porch.min);
	/* VWS */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			VERTICAL_SYNC_WIDTH, ctx->dt.vsync_len.min);
	/* VBP */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p2_client,
			VERTICAL_BACK_PORCH, ctx->dt.vback_porch.min);
	/* M value */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
			MIPI_PLL_M_NUM_23_16, (m >> 16) & 0xff);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
			MIPI_PLL_M_NUM_15_8, (m >> 8) & 0xff);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
			MIPI_PLL_M_NUM_7_0, (m & 0xff));
	/* N value */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
			MIPI_PLL_N_NUM_23_16, (n >> 16) & 0xff);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
			MIPI_PLL_N_NUM_15_8, (n >> 8) & 0xff);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client, MIPI_PLL_N_NUM_7_0,
			(n & 0xff));
	/* diff */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
			MIPI_DIGITAL_ADJ_1, 0x37);

	ret |= anx7625_odfc_config(ctx, post_divider - 1);

	if (ret < 0)
		DRM_ERROR("mipi dsi setup IO error.\n");

	return ret;
}

static int anx7625_swap_dsi_lane3(struct anx7625_data *ctx)
{
	int val;

	/* swap MIPI-DSI data lane 3 P and N */
	val = anx7625_reg_read(ctx, ctx->i2c.rx_p1_client, MIPI_SWAP);
	if (val < 0) {
		DRM_ERROR("IO error : access MIPI_SWAP.\n");
		return -EIO;
	}

	val |= (1 << MIPI_SWAP_CH3);
	return anx7625_reg_write(ctx, ctx->i2c.rx_p1_client, MIPI_SWAP, val);
}

static int anx7625_api_dsi_config(struct anx7625_data *ctx)

{
	int val, ret;

	/* swap MIPI-DSI data lane 3 P and N */
	ret = anx7625_swap_dsi_lane3(ctx);
	if (ret < 0) {
		DRM_ERROR("IO error : swap dsi lane 3 failed.\n");
		return ret;
	}

	/* DSI clock settings */
	val = (0 << MIPI_HS_PWD_CLK)		|
		(0 << MIPI_HS_RT_CLK)		|
		(0 << MIPI_PD_CLK)		|
		(1 << MIPI_CLK_RT_MANUAL_PD_EN)	|
		(1 << MIPI_CLK_HS_MANUAL_PD_EN)	|
		(0 << MIPI_CLK_DET_DET_BYPASS)	|
		(0 << MIPI_CLK_MISS_CTRL)	|
		(0 << MIPI_PD_LPTX_CH_MANUAL_PD_EN);
	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				MIPI_PHY_CONTROL_3, val);

	/*
	 * Decreased HS prepare timing delay from 160ns to 80ns work with
	 *     a) Dragon board 810 series (Qualcomm AP)
	 *     b) Moving Pixel DSI source (PG3A pattern generator +
	 *	P332 D-PHY Probe) default D-PHY timing
	 *	5ns/step
	 */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_TIME_HS_PRPR, 0x10);

	/* enable DSI mode*/
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_18,
				SELECT_DSI << MIPI_DPI_SELECT);

	ret |= anx7625_dsi_video_timing_config(ctx);
	if (ret < 0) {
		DRM_ERROR("dsi video timing config failed\n");
		return ret;
	}

	/* toggle m, n ready */
	ret = anx7625_write_and(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_6,
				~(MIPI_M_NUM_READY | MIPI_N_NUM_READY));
	usleep_range(1000, 1100);
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, MIPI_DIGITAL_PLL_6,
				MIPI_M_NUM_READY | MIPI_N_NUM_READY);

	/* configure integer stable register */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_VIDEO_STABLE_CNT, 0x02);
	/* power on MIPI RX */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_LANE_CTRL_10, 0x00);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_LANE_CTRL_10, 0x80);

	if (ret < 0)
		DRM_ERROR("IO error : mipi dsi enable init failed.\n");

	return ret;
}

static int anx7625_dsi_config(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;
	int ret;

	DRM_DEV_DEBUG_DRIVER(dev, "config dsi.\n");

	/* DSC disable */
	ret = anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
				R_DSC_CTRL_0, ~DSC_EN);

	ret |= anx7625_api_dsi_config(ctx);

	if (ret < 0) {
		DRM_ERROR("IO error : api dsi config error.\n");
		return ret;
	}

	/* set MIPI RX EN */
	ret = anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
			       AP_AV_STATUS, AP_MIPI_RX_EN);
	/* clear mute flag */
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
				 AP_AV_STATUS, (u8)~AP_MIPI_MUTE);
	if (ret < 0)
		DRM_ERROR("IO error : enable mipi rx failed.\n");
	else
		DRM_DEV_DEBUG_DRIVER(dev, "success to config DSI\n");

	return ret;
}

static int anx7625_api_dpi_config(struct anx7625_data *ctx)
{
	u16 freq = ctx->dt.pixelclock.min / 1000;
	int ret;

	/* configure pixel clock */
	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				PIXEL_CLOCK_L, freq & 0xFF);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 PIXEL_CLOCK_H, (freq >> 8));

	/* set DPI mode */
	/* set to DPI PLL module sel */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_DIGITAL_PLL_9, 0x20);
	/* power down MIPI */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_LANE_CTRL_10, 0x08);
	/* enable DPI mode */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p1_client,
				 MIPI_DIGITAL_PLL_18, 0x1C);
	/* set first edge */
	ret |= anx7625_reg_write(ctx, ctx->i2c.tx_p2_client,
				 VIDEO_CONTROL_0, 0x05);
	if (ret < 0)
		DRM_ERROR("IO error : dpi phy set failed.\n");

	return ret;
}

static int anx7625_dpi_config(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;
	int ret;

	DRM_DEV_DEBUG_DRIVER(dev, "config dpi\n");

	/* DSC disable */
	ret = anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
				R_DSC_CTRL_0, ~DSC_EN);
	if (ret < 0) {
		DRM_ERROR("IO error : disable dsc failed.\n");
		return ret;
	}

	ret = anx7625_config_bit_matrix(ctx);
	if (ret < 0) {
		DRM_ERROR("config bit matrix failed.\n");
		return ret;
	}

	ret = anx7625_api_dpi_config(ctx);
	if (ret < 0) {
		DRM_ERROR("mipi phy(dpi) setup failed.\n");
		return ret;
	}

	/* set MIPI RX EN */
	ret = anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
			       AP_AV_STATUS, AP_MIPI_RX_EN);
	/* clear mute flag */
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
				 AP_AV_STATUS, (u8)~AP_MIPI_MUTE);
	if (ret < 0)
		DRM_ERROR("IO error : enable mipi rx failed.\n");

	return ret;
}

static void anx7625_dp_start(struct anx7625_data *ctx)
{
	int ret;

	if (!ctx->display_timing_valid) {
		DRM_ERROR("mipi drm haven't set display timing yet.\n");
		return;
	}

	anx7625_config_audio_input(ctx);

	if (ctx->pdata.dsi_supported)
		ret = anx7625_dsi_config(ctx);
	else
		ret = anx7625_dpi_config(ctx);

	if (ret < 0)
		DRM_ERROR("MIPI phy setup error.\n");
}

static void anx7625_dp_stop(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;
	int ret;

	DRM_DEV_DEBUG_DRIVER(dev, "stop dp output\n");

	/*
	 * video disable: 0x72:08 bit 7 = 0;
	 * audio disable: 0x70:87 bit 0 = 0;
	 */
	ret = anx7625_write_and(ctx, ctx->i2c.tx_p0_client, 0x87, 0xfe);
	ret |= anx7625_write_and(ctx, ctx->i2c.tx_p2_client, 0x08, 0x7f);

	ret |= anx7625_video_mute_control(ctx, 1);
	if (ret < 0)
		DRM_ERROR("IO error : mute video failed\n");
}

static int sp_tx_rst_aux(struct anx7625_data *ctx)
{
	int ret;

	ret = anx7625_write_or(ctx, ctx->i2c.tx_p2_client, RST_CTRL2,
			       AUX_RST);
	ret |= anx7625_write_and(ctx, ctx->i2c.tx_p2_client, RST_CTRL2,
				 ~AUX_RST);
	return ret;
}

static int sp_tx_aux_wr(struct anx7625_data *ctx, u8 offset)
{
	int ret;

	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				AP_AUX_BUFF_START, offset);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 AP_AUX_COMMAND, 0x04);
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
				AP_AUX_CTRL_STATUS, AP_AUX_CTRL_OP_EN);
	return (ret | wait_aux_op_finish(ctx));
}

static int sp_tx_aux_rd(struct anx7625_data *ctx, u8 len_cmd)
{
	int ret;

	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				AP_AUX_COMMAND, len_cmd);
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p0_client,
				AP_AUX_CTRL_STATUS, AP_AUX_CTRL_OP_EN);
	return (ret | wait_aux_op_finish(ctx));
}

static int sp_tx_get_edid_block(struct anx7625_data *ctx)
{
	int c = 0;
	struct device *dev = &ctx->client->dev;

	sp_tx_aux_wr(ctx, 0x7e);
	sp_tx_aux_rd(ctx, 0x01);
	c = anx7625_reg_read(ctx, ctx->i2c.rx_p0_client, AP_AUX_BUFF_START);
	if (c < 0) {
		DRM_ERROR("IO error : access AUX BUFF.\n");
		return -EIO;
	}

	DRM_DEV_DEBUG_DRIVER(dev, " EDID Block = %d\n", c + 1);

	if (c > MAX_EDID_BLOCK)
		c = 1;

	return c;
}

static int edid_read(struct anx7625_data *ctx,
		     u8 offset, u8 *pblock_buf)
{
	int ret, cnt;
	struct device *dev = &ctx->client->dev;

	for (cnt = 0; cnt <= EDID_TRY_CNT; cnt++) {
		sp_tx_aux_wr(ctx, offset);
		/* set I2C read com 0x01 mot = 0 and read 16 bytes */
		ret = sp_tx_aux_rd(ctx, 0xf1);

		if (ret) {
			sp_tx_rst_aux(ctx);
			DRM_DEV_DEBUG_DRIVER(dev, "edid read failed, reset!\n");
		} else {
			ret = anx7625_reg_block_read(ctx, ctx->i2c.rx_p0_client,
						     AP_AUX_BUFF_START,
						     MAX_DPCD_BUFFER_SIZE,
						     pblock_buf);
			if (ret > 0)
				break;
		}
	}

	if (cnt > EDID_TRY_CNT)
		return -EIO;

	return 0;
}

static int segments_edid_read(struct anx7625_data *ctx,
			      u8 segment, u8 *buf, u8 offset)
{
	u8 cnt;
	int ret;
	struct device *dev = &ctx->client->dev;

	/* write address only */
	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				AP_AUX_ADDR_7_0, 0x30);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 AP_AUX_COMMAND, 0x04);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 AP_AUX_CTRL_STATUS,
				 AP_AUX_CTRL_ADDRONLY | AP_AUX_CTRL_OP_EN);

	ret |= wait_aux_op_finish(ctx);
	/* write segment address */
	ret |= sp_tx_aux_wr(ctx, segment);
	/* data read */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 AP_AUX_ADDR_7_0, 0x50);
	if (ret) {
		DRM_ERROR("IO error : aux initial failed.\n");
		return ret;
	}

	for (cnt = 0; cnt <= EDID_TRY_CNT; cnt++) {
		sp_tx_aux_wr(ctx, offset);
		/* set I2C read com 0x01 mot = 0 and read 16 bytes */
		ret = sp_tx_aux_rd(ctx, 0xf1);

		if (ret) {
			ret = sp_tx_rst_aux(ctx);
			DRM_DEV_ERROR(dev, "segment read failed, reset!\n");
		} else {
			ret = anx7625_reg_block_read(ctx, ctx->i2c.rx_p0_client,
						     AP_AUX_BUFF_START,
						     MAX_DPCD_BUFFER_SIZE, buf);
			if (ret > 0)
				break;
		}
	}

	if (cnt > EDID_TRY_CNT)
		return -EIO;

	return 0;
}

static int sp_tx_edid_read(struct anx7625_data *ctx,
			   u8 *pedid_blocks_buf)
{
	u8 offset, edid_pos;
	int count, blocks_num;
	u8 pblock_buf[MAX_DPCD_BUFFER_SIZE];
	u8 i, j;
	u8 g_edid_break = 0;
	int ret;
	struct device *dev = &ctx->client->dev;

	/* address initial */
	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				AP_AUX_ADDR_7_0, 0x50);
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 AP_AUX_ADDR_15_8, 0);
	ret |= anx7625_write_and(ctx, ctx->i2c.rx_p0_client,
				 AP_AUX_ADDR_19_16, 0xf0);
	if (ret < 0) {
		DRM_ERROR("access aux channel IO error.\n");
		return -EIO;
	}

	blocks_num = sp_tx_get_edid_block(ctx);
	if (blocks_num < 0)
		return blocks_num;

	count = 0;

	do {
		switch (count) {
		case 0:
		case 1:
			for (i = 0; i < 8; i++) {
				offset = (i + count * 8) * MAX_DPCD_BUFFER_SIZE;
				g_edid_break = edid_read(ctx, offset,
							 pblock_buf);

				if (g_edid_break)
					break;

				memcpy(&pedid_blocks_buf[offset],
				       pblock_buf,
				       MAX_DPCD_BUFFER_SIZE);
			}

			break;
		case 2:
			offset = 0x00;

			for (j = 0; j < 8; j++) {
				edid_pos = (j + count * 8) *
					MAX_DPCD_BUFFER_SIZE;

				if (g_edid_break == 1)
					break;

				segments_edid_read(ctx, count / 2,
						   pblock_buf, offset);
				memcpy(&pedid_blocks_buf[edid_pos],
				       pblock_buf,
				       MAX_DPCD_BUFFER_SIZE);
				offset = offset + 0x10;
			}

			break;
		case 3:
			offset = 0x80;

			for (j = 0; j < 8; j++) {
				edid_pos = (j + count * 8) *
					MAX_DPCD_BUFFER_SIZE;
				if (g_edid_break == 1)
					break;

				segments_edid_read(ctx, count / 2,
						   pblock_buf, offset);
				memcpy(&pedid_blocks_buf[edid_pos],
				       pblock_buf,
				       MAX_DPCD_BUFFER_SIZE);
				offset = offset + 0x10;
			}

			break;
		default:
			break;
		}

		count++;

	} while (blocks_num >= count);

	/* check edid data */
	if (!drm_edid_is_valid((struct edid *)pedid_blocks_buf)) {
		DRM_DEV_ERROR(dev, "WARNING! edid check failed!\n");
		return -EINVAL;
	}

	/* reset aux channel */
	sp_tx_rst_aux(ctx);

	return (blocks_num + 1);
}

static void anx7625_power_on(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;

	if (!ctx->pdata.low_power_mode) {
		DRM_DEV_DEBUG_DRIVER(dev, "Anx7625 not low power mode!\n");
		return;
	}

	/* power on pin enable */
	gpiod_set_value(ctx->pdata.gpio_p_on, 1);
	usleep_range(10000, 11000);
	/* power reset pin enable */
	gpiod_set_value(ctx->pdata.gpio_reset, 1);
	usleep_range(10000, 11000);

	DRM_DEV_DEBUG_DRIVER(dev, "Anx7625 power on !\n");
}

static void anx7625_power_standby(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;

	if (!ctx->pdata.low_power_mode) {
		DRM_DEV_DEBUG_DRIVER(dev, "anx7625 not low power mode!\n");
		return;
	}

	gpiod_set_value(ctx->pdata.gpio_reset, 0);
	usleep_range(1000, 1100);
	gpiod_set_value(ctx->pdata.gpio_p_on, 0);
	usleep_range(1000, 1100);
	DRM_DEV_DEBUG_DRIVER(dev, "anx7625 power down\n");
}

/* basic configurations of ANX7625 */
static void anx7625_config(struct anx7625_data *ctx)
{
	anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
			  XTAL_FRQ_SEL, XTAL_FRQ_27M);
}

static void anx7625_disable_pd_protocol(struct anx7625_data *ctx)
{
	struct device *dev = &ctx->client->dev;
	int ret;

	/* reset main ocm */
	ret = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client, 0x88, 0x40);
	/* Disable PD */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				 AP_AV_STATUS, AP_DISABLE_PD);
	/* release main ocm */
	ret |= anx7625_reg_write(ctx, ctx->i2c.rx_p0_client, 0x88, 0x00);

	if (ret < 0)
		DRM_DEV_DEBUG_DRIVER(dev, "disable PD feature failed.\n");
	else
		DRM_DEV_DEBUG_DRIVER(dev, "disable PD feature succeeded.\n");
}

static void anx7625_power_on_init(struct anx7625_data *ctx)
{
	int retry_count, i;
	int ret;
	struct device *dev = &ctx->client->dev;

	for (retry_count = 0; retry_count < 3; retry_count++) {
		anx7625_power_on(ctx);
		anx7625_config(ctx);

		for (i = 0; i < OCM_LOADING_TIME; i++) {
			/* check interface workable */
			ret = anx7625_reg_read(ctx, ctx->i2c.rx_p0_client,
					       FLASH_LOAD_STA);
			if (ret < 0) {
				DRM_ERROR("IO error : access flash load.\n");
				return;
			}
			if ((ret & FLASH_LOAD_STA_CHK) == FLASH_LOAD_STA_CHK) {
				anx7625_disable_pd_protocol(ctx);
				DRM_DEV_DEBUG_DRIVER(dev,
						     "Firmware ver %02x%02x,",
					anx7625_reg_read(ctx,
							 ctx->i2c.rx_p0_client,
							 OCM_FW_VERSION),
					anx7625_reg_read(ctx,
							 ctx->i2c.rx_p0_client,
							 OCM_FW_REVERSION));
				DRM_DEV_DEBUG_DRIVER(dev, "Driver version %s\n",
						     ANX7625_DRV_VERSION);

				return;
			}
			usleep_range(1000, 1100);
		}
		anx7625_power_standby(ctx);
	}
}

static int anx7625_extcon_notifier(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct anx7625_data *ctx =
		container_of(nb, struct anx7625_data, event_nb);

	schedule_work(&ctx->extcon_wq);
	return NOTIFY_DONE;
}

static void anx7625_chip_control(struct anx7625_data *ctx, int state)
{
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "before set, power_state(%d).\n",
			     atomic_read(&ctx->power_status));

	if (!ctx->pdata.low_power_mode)
		return;

	if (state) {
		atomic_inc(&ctx->power_status);
		if (atomic_read(&ctx->power_status) == 1)
			anx7625_power_on_init(ctx);
	} else {
		if (atomic_read(&ctx->power_status)) {
			atomic_dec(&ctx->power_status);

			if (atomic_read(&ctx->power_status) == 0)
				anx7625_power_standby(ctx);
		}
	}

	DRM_DEV_DEBUG_DRIVER(dev, "after set, power_state(%d).\n",
			     atomic_read(&ctx->power_status));
}

static void anx7625_extcon_work(struct work_struct *work)
{
	struct anx7625_data *ctx =
		container_of(work, struct anx7625_data, extcon_wq);
	int state = extcon_get_state(ctx->extcon, EXTCON_DISP_DP);

	mutex_lock(&ctx->lock);
	anx7625_chip_control(ctx, state);
	mutex_unlock(&ctx->lock);
}

static int anx7625_extcon_notifier_init(struct anx7625_data *ctx)
{
	int ret;
	struct device *dev = &ctx->client->dev;

	ctx->event_nb.notifier_call = anx7625_extcon_notifier;
	INIT_WORK(&ctx->extcon_wq, anx7625_extcon_work);
	ret = devm_extcon_register_notifier(&ctx->client->dev, ctx->extcon,
					    EXTCON_DISP_DP, &ctx->event_nb);
	if (ret) {
		DRM_DEV_ERROR(dev, "failed to register notifier for DP");
		return ret;
	}
	return 0;
}

static void anx7625_init_gpio(struct anx7625_data *platform)
{
	struct device *dev = &platform->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "anx7625 init gpio\n");

	/* gpio for chip power enable */
	platform->pdata.gpio_p_on =
		devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	/* gpio for chip reset */
	platform->pdata.gpio_reset =
		devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);

	if (platform->pdata.gpio_p_on && platform->pdata.gpio_reset) {
		platform->pdata.low_power_mode = 1;
		DRM_DEV_DEBUG_DRIVER(dev, "low power mode, pon %d, reset %d.\n",
				     desc_to_gpio(platform->pdata.gpio_p_on),
				     desc_to_gpio(platform->pdata.gpio_reset));
	} else {
		platform->pdata.low_power_mode = 0;
		DRM_DEV_DEBUG_DRIVER(dev, "not low power mode.\n");
	}
}

static void anx7625_stop_dp_work(struct anx7625_data *ctx)
{
	ctx->hpd_status = 0;
	ctx->hpd_high_cnt = 0;
	ctx->display_timing_valid = 0;

	ctx->slimport_edid_p.edid_block_num = -1;

	if (ctx->pdata.low_power_mode == 0)
		anx7625_disable_pd_protocol(ctx);
}

static void anx7625_start_dp_work(struct anx7625_data *ctx)
{
	int ret;
	struct device *dev = &ctx->client->dev;

	if (ctx->hpd_high_cnt >= 2) {
		DRM_DEV_DEBUG_DRIVER(dev, "anx7625 filter useless HPD\n");
		return;
	}

	ctx->hpd_high_cnt++;

	/* not support HDCP */
	ret = anx7625_write_and(ctx, ctx->i2c.rx_p1_client, 0xee, 0x9f);

	/* try auth flag */
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, 0xec, 0x10);
	/* interrupt for DRM */
	ret |= anx7625_write_or(ctx, ctx->i2c.rx_p1_client, 0xff, 0x01);
	if (ret < 0)
		return;

	ret = anx7625_reg_read(ctx, ctx->i2c.rx_p1_client, 0x86);
	if (ret < 0)
		return;

	DRM_DEV_DEBUG_DRIVER(dev, "Secure OCM version=%02x\n", ret);
}

static int anx7625_read_hpd_status_p0(struct anx7625_data *ctx)
{
	return anx7625_reg_read(ctx, ctx->i2c.rx_p0_client, SYSTEM_STSTUS);
}

static void anx7625_hpd_polling(struct anx7625_data *ctx)
{
	int ret, val;
	struct device *dev = &ctx->client->dev;

	if (atomic_read(&ctx->power_status) != 1) {
		DRM_DEV_DEBUG_DRIVER(dev, "No need to poling HPD status.\n");
		return;
	}

	ret = readx_poll_timeout(anx7625_read_hpd_status_p0,
				 ctx, val,
				 ((val & HPD_STATUS) || (val < 0)),
				 5000,
				 5000 * 100);
	if (ret) {
		DRM_DEV_ERROR(dev, "HPD polling timeout!\n");
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, "HPD raise up.\n");
		anx7625_reg_write(ctx, ctx->i2c.tcpc_client,
				  INTR_ALERT_1, 0xFF);
		anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				  INTERFACE_CHANGE_INT, 0);
	}

	anx7625_start_dp_work(ctx);
}

static void anx7625_disconnect_check(struct anx7625_data *ctx)
{
	if (atomic_read(&ctx->power_status) == 0)
		anx7625_stop_dp_work(ctx);
}

static void anx7625_low_power_mode_check(struct anx7625_data *ctx,
					 int state)
{
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "low power mode check, state(%d).\n", state);

	if (ctx->pdata.low_power_mode) {
		anx7625_chip_control(ctx, state);
		if (state)
			anx7625_hpd_polling(ctx);
		else
			anx7625_disconnect_check(ctx);
	}
}

static void dp_hpd_change_handler(struct anx7625_data *ctx, bool on)
{
	struct device *dev = &ctx->client->dev;

	/* hpd changed */
	DRM_DEV_DEBUG_DRIVER(dev, "dp_hpd_change_default_func: %d\n",
			     (u32)on);

	if (on == 0) {
		DRM_DEV_DEBUG_DRIVER(dev, " HPD low\n");
		anx7625_stop_dp_work(ctx);
	} else {
		DRM_DEV_DEBUG_DRIVER(dev, " HPD high\n");
		anx7625_start_dp_work(ctx);
	}

	ctx->hpd_status = 1;
}

static int anx7625_hpd_change_detect(struct anx7625_data *ctx)
{
	int intr_vector, status;
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "power_status=%d\n",
			     (u32)atomic_read(&ctx->power_status));

	status = anx7625_reg_write(ctx, ctx->i2c.tcpc_client,
				   INTR_ALERT_1, 0xFF);
	if (status < 0) {
		DRM_ERROR("IO error : clear alert register.\n");
		return status;
	}

	intr_vector = anx7625_reg_read(ctx, ctx->i2c.rx_p0_client,
				       INTERFACE_CHANGE_INT);
	if (intr_vector < 0) {
		DRM_ERROR("IO error : access interrupt change register.\n");
		return intr_vector;
	}
	DRM_DEV_DEBUG_DRIVER(dev, "0x7e:0x44=%x\n", intr_vector);
	status = anx7625_reg_write(ctx, ctx->i2c.rx_p0_client,
				   INTERFACE_CHANGE_INT,
				   intr_vector & (~intr_vector));
	if (status < 0) {
		DRM_ERROR("IO error : clear interrupt change register.\n");
		return status;
	}

	if (!(intr_vector & HPD_STATUS_CHANGE))
		return -ENOENT;

	status = anx7625_reg_read(ctx, ctx->i2c.rx_p0_client,
				  SYSTEM_STSTUS);
	if (status < 0) {
		DRM_ERROR("IO error : clear interrupt status.\n");
		return status;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "0x7e:0x45=%x\n", status);
	dp_hpd_change_handler(ctx, status & HPD_STATUS);

	return 0;
}

static void anx7625_work_func(struct work_struct *work)
{
	int event;
	struct anx7625_data *ctx = container_of(work,
						struct anx7625_data, work);

	mutex_lock(&ctx->lock);
	event = anx7625_hpd_change_detect(ctx);
	mutex_unlock(&ctx->lock);

	if (event < 0)
		return;

	if (ctx->bridge_attached)
		drm_helper_hpd_irq_event(ctx->connector.dev);
}

static irqreturn_t anx7625_intr_hpd_isr(int irq, void *data)
{
	struct anx7625_data *ctx = (struct anx7625_data *)data;

	if (atomic_read(&ctx->power_status) != 1)
		return IRQ_NONE;

	queue_work(ctx->workqueue, &ctx->work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int anx7625_parse_dt(struct device *dev,
			    struct anx7625_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	struct device_node *panel_node, *out_ep;

	of_property_read_u32(dev->of_node, "panel_flags",
			     &pdata->panel_flags);
	DRM_DEV_DEBUG_DRIVER(dev, "panel_flags : %d\n", pdata->panel_flags);

	if (pdata->panel_flags == 1)
		pdata->internal_panel = 1;
	else if (pdata->panel_flags == 2)
		pdata->extcon_supported = 1;
	DRM_DEV_DEBUG_DRIVER(dev, "%s support extcon, %s internal panel\n",
			     pdata->extcon_supported ? "" : "not",
			     pdata->internal_panel ? "has" : "no");

	pdata->node.mipi_dsi_host_node = of_graph_get_remote_node(np, 0, 0);
	if (pdata->node.mipi_dsi_host_node) {
		of_node_put(pdata->node.mipi_dsi_host_node);
		pdata->dsi_supported = 1;
		DRM_DEV_DEBUG_DRIVER(dev, "found dsi host node.\n");
	} else {
		pdata->node.mipi_dpi_host_node =
			of_graph_get_remote_node(np, 1, 0);

		if (pdata->node.mipi_dpi_host_node) {
			of_node_put(pdata->node.mipi_dpi_host_node);
			DRM_DEV_DEBUG_DRIVER(dev, "found dpi host node.\n");
		}
	}

	DRM_DEV_DEBUG_DRIVER(dev, "dsi_supported : %d\n", pdata->dsi_supported);

	pdata->node.panel_node = of_graph_get_port_by_id(np, 2);
	if (pdata->node.panel_node) {
		of_node_put(pdata->node.panel_node);
		out_ep = of_get_child_by_name(pdata->node.panel_node,
					      "endpoint");
		if (!out_ep) {
			DRM_DEV_DEBUG_DRIVER(dev, "cannot get endpoint.\n");
			return -EPROBE_DEFER;
		}

		panel_node = of_graph_get_remote_port_parent(out_ep);
		of_node_put(out_ep);

		pdata->panel = of_drm_find_panel(panel_node);
		DRM_DEV_DEBUG_DRIVER(dev, "get panel node.\n");
		of_node_put(panel_node);
		if (IS_ERR_OR_NULL(pdata->panel))
			return -EPROBE_DEFER;
	} else {
		if (pdata->internal_panel) {
			DRM_ERROR("failed to get internal panel.\n");
			return -EPROBE_DEFER;
		}
	}

	return 0;
}
#else
static int anx7625_parse_dt(struct device *dev,
			    struct anx7625_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static inline struct anx7625_data *connector_to_anx7625(struct drm_connector *c)
{
	return container_of(c, struct anx7625_data, connector);
}

static inline struct anx7625_data *bridge_to_anx7625(struct drm_bridge *bridge)
{
	return container_of(bridge, struct anx7625_data, bridge);
}

static void anx7625_post_disable(struct drm_bridge *bridge)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	struct device *dev = &ctx->client->dev;
	int ret;

	DRM_DEV_DEBUG_DRIVER(dev, "post disable\n");

	if (!ctx->pdata.panel)
		return;

	ret = drm_panel_unprepare(ctx->pdata.panel);
	if (ret)
		DRM_ERROR("failed to unprepare panel: %d\n", ret);
	else
		DRM_DEV_DEBUG_DRIVER(dev, "backlight unprepared.\n");

	atomic_set(&ctx->panel_power, 0);
}

static void anx7625_pre_enable(struct drm_bridge *bridge)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	struct device *dev = &ctx->client->dev;
	int ret;

	DRM_DEV_DEBUG_DRIVER(dev, "pre enable\n");

	if (!ctx->pdata.panel)
		return;

	ret = drm_panel_prepare(ctx->pdata.panel);
	if (ret < 0)
		DRM_ERROR("failed to prepare panel: %d\n", ret);
	else
		DRM_DEV_DEBUG_DRIVER(dev, "backlight prepared.\n");
	atomic_set(&ctx->panel_power, 1);
}

static int anx7625_get_modes(struct drm_connector *connector)
{
	struct anx7625_data *ctx = connector_to_anx7625(connector);
	int err, num_modes = 0;
	int turn_off_flag = 0;
	struct s_edid_data *p_edid = &ctx->slimport_edid_p;
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "drm get modes\n");

	if (ctx->slimport_edid_p.edid_block_num > 0)
		goto out;

	mutex_lock(&ctx->lock);

	if (ctx->pdata.panel && atomic_read(&ctx->panel_power) == 0) {
		turn_off_flag = 1;
		anx7625_pre_enable(&ctx->bridge);
	}

	anx7625_low_power_mode_check(ctx, 1);

	p_edid->edid_block_num = sp_tx_edid_read(ctx, p_edid->edid_raw_data);

	err = -EIO;
	if (p_edid->edid_block_num < 0) {
		DRM_ERROR("Failed to read EDID.\n");
		goto fail;
	}

	err = drm_connector_update_edid_property(connector,
						 (struct edid *)
						 &p_edid->edid_raw_data);

	if (err)
		DRM_ERROR("Failed to update EDID property: %d\n", err);

fail:
	mutex_unlock(&ctx->lock);
	anx7625_low_power_mode_check(ctx, 0);
	if (ctx->pdata.panel && turn_off_flag == 1)
		anx7625_post_disable(&ctx->bridge);

	if (err)
		return err;

out:
	num_modes = drm_add_edid_modes(connector,
				       (struct edid *)&p_edid->edid_raw_data);
	DRM_DEV_DEBUG_DRIVER(dev, "num_modes(%d)\n", num_modes);

	return num_modes;
}

static enum drm_mode_status
anx7625_connector_mode_valid(struct drm_connector *connector,
			     struct drm_display_mode *mode)
{
	struct anx7625_data *ctx = connector_to_anx7625(connector);
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "drm modes valid verify\n");

	if (mode->clock > SUPPORT_PIXEL_CLOCK)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static struct drm_connector_helper_funcs anx7625_connector_helper_funcs = {
	.get_modes = anx7625_get_modes,
	.mode_valid = anx7625_connector_mode_valid,
};

static enum drm_connector_status anx7625_detect(struct drm_connector *connector,
						bool force)
{
	struct anx7625_data *ctx = connector_to_anx7625(connector);
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "drm detect\n");

	if (ctx->pdata.internal_panel)
		return connector_status_connected;

	if (!ctx->hpd_status)
		return connector_status_disconnected;

	return connector_status_connected;
}

static const struct drm_connector_funcs anx7625_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = anx7625_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int anx7625_attach_dsi(struct anx7625_data *ctx)
{
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	struct device_node *mipi_host_node;
	struct device *dev = &ctx->client->dev;
	const struct mipi_dsi_device_info info = {
		.type = "anx7625",
		.channel = 0,
		.node = NULL,
	};

	DRM_DEV_DEBUG_DRIVER(dev, "attach dsi\n");

	if (ctx->pdata.dsi_supported)
		mipi_host_node = ctx->pdata.node.mipi_dsi_host_node;
	else
		mipi_host_node = ctx->pdata.node.mipi_dpi_host_node;

	if (!mipi_host_node) {
		DRM_ERROR("dsi host is not configured.\n");
		return -EINVAL;
	}

	host = of_find_mipi_dsi_host_by_node(mipi_host_node);
	if (!host) {
		DRM_ERROR("failed to find dsi host.\n");
		return -EINVAL;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		DRM_ERROR("failed to create dsi device.\n");
		return -EINVAL;
	}

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO	|
		MIPI_DSI_MODE_VIDEO_SYNC_PULSE	|
		MIPI_DSI_MODE_EOT_PACKET	|
		MIPI_DSI_MODE_VIDEO_HSE;

	if (mipi_dsi_attach(dsi) < 0) {
		DRM_ERROR("failed to attach dsi to host.\n");
		mipi_dsi_device_unregister(dsi);
		return -EINVAL;
	}

	ctx->dsi = dsi;

	DRM_DEV_DEBUG_DRIVER(dev, "attach dsi succeeded.\n");

	return 0;
}

static void anx7625_bridge_detach(struct drm_bridge *bridge)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);

	if (ctx->dsi) {
		mipi_dsi_detach(ctx->dsi);
		mipi_dsi_device_unregister(ctx->dsi);
	}

	if (ctx->bridge_attached)
		drm_connector_unregister(&ctx->connector);
}

static int anx7625_bridge_attach(struct drm_bridge *bridge)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	int err;
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "drm attach\n");
	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	err = drm_connector_init(bridge->dev, &ctx->connector,
				 &anx7625_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (err) {
		DRM_ERROR("Failed to initialize connector: %d\n", err);
		return err;
	}

	drm_connector_helper_add(&ctx->connector,
				 &anx7625_connector_helper_funcs);

	err = drm_connector_register(&ctx->connector);
	if (err) {
		DRM_ERROR("Failed to register connector: %d\n", err);
		return err;
	}

	ctx->connector.polled = DRM_CONNECTOR_POLL_HPD;

	err = drm_connector_attach_encoder(&ctx->connector, bridge->encoder);
	if (err) {
		DRM_ERROR("Failed to link up connector to encoder: %d\n", err);
		drm_connector_unregister(&ctx->connector);
		return err;
	}

	err = anx7625_attach_dsi(ctx);
	if (err) {
		DRM_ERROR("Failed to attach to dsi : %d\n", err);
		drm_connector_unregister(&ctx->connector);
		return err;
	}

	ctx->bridge_attached = 1;

	return 0;
}

static enum drm_mode_status
anx7625_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_mode *mode)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "drm mode checking\n");

	/* Max 1200p at 5.4 Ghz, one lane, pixel clock 300M */
	if (mode->clock > SUPPORT_PIXEL_CLOCK) {
		DRM_DEV_DEBUG_DRIVER(dev,
				     "drm mode invalid, pixelclock too high.\n");
		return MODE_CLOCK_HIGH;
	}

	DRM_DEV_DEBUG_DRIVER(dev, "drm mode valid.\n");

	return MODE_OK;
}

static void anx7625_bridge_mode_set(struct drm_bridge *bridge,
				    const struct drm_display_mode *old_mode,
				    const struct drm_display_mode *mode)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	struct device *dev = &ctx->client->dev;

	DRM_DEV_DEBUG_DRIVER(dev, "drm mode set\n");

	mutex_lock(&ctx->lock);

	ctx->dt.pixelclock.min = mode->clock;
	ctx->dt.hactive.min = mode->hdisplay;
	ctx->dt.hsync_len.min = mode->hsync_end - mode->hsync_start;
	ctx->dt.hfront_porch.min = mode->hsync_start - mode->hdisplay;
	ctx->dt.hback_porch.min = mode->htotal - mode->hsync_end;
	ctx->dt.vactive.min = mode->vdisplay;
	ctx->dt.vsync_len.min = mode->vsync_end - mode->vsync_start;
	ctx->dt.vfront_porch.min = mode->vsync_start - mode->vdisplay;
	ctx->dt.vback_porch.min = mode->vtotal - mode->vsync_end;

	ctx->display_timing_valid = 1;

	DRM_DEV_DEBUG_DRIVER(dev, "pixelclock(%d).\n", ctx->dt.pixelclock.min);
	DRM_DEV_DEBUG_DRIVER(dev, "hactive(%d), hsync(%d), hfp(%d), hbp(%d)\n",
			     ctx->dt.hactive.min,
			     ctx->dt.hsync_len.min,
			     ctx->dt.hfront_porch.min,
			     ctx->dt.hback_porch.min);
	DRM_DEV_DEBUG_DRIVER(dev, "vactive(%d), vsync(%d), vfp(%d), vbp(%d)\n",
			     ctx->dt.vactive.min,
			     ctx->dt.vsync_len.min,
			     ctx->dt.vfront_porch.min,
			     ctx->dt.vback_porch.min);
	DRM_DEV_DEBUG_DRIVER(dev, "hdisplay(%d),hsync_start(%d).\n",
			     mode->hdisplay,
			     mode->hsync_start);
	DRM_DEV_DEBUG_DRIVER(dev, "hsync_end(%d),htotal(%d).\n",
			     mode->hsync_end,
			     mode->htotal);
	DRM_DEV_DEBUG_DRIVER(dev, "vdisplay(%d),vsync_start(%d).\n",
			     mode->vdisplay,
			     mode->vsync_start);
	DRM_DEV_DEBUG_DRIVER(dev, "vsync_end(%d),vtotal(%d).\n",
			     mode->vsync_end,
			     mode->vtotal);

	mutex_unlock(&ctx->lock);
}

static void anx7625_bridge_enable(struct drm_bridge *bridge)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	struct device *dev = &ctx->client->dev;
	int ret;

	DRM_DEV_DEBUG_DRIVER(dev, "drm enable\n");

	anx7625_low_power_mode_check(ctx, 1);

	if (WARN_ON(!atomic_read(&ctx->power_status)))
		return;

	if (ctx->pdata.panel) {
		ret = drm_panel_enable(ctx->pdata.panel);
		if (ret < 0) {
			DRM_ERROR("failed to enable panel: %d.\n", ret);
			return;
		}
	}

	mutex_lock(&ctx->lock);
	anx7625_dp_start(ctx);
	mutex_unlock(&ctx->lock);
}

static void anx7625_bridge_disable(struct drm_bridge *bridge)
{
	struct anx7625_data *ctx = bridge_to_anx7625(bridge);
	struct device *dev = &ctx->client->dev;
	int ret;

	if (WARN_ON(!atomic_read(&ctx->power_status)))
		return;

	DRM_DEV_DEBUG_DRIVER(dev, "drm disable\n");

	if (ctx->pdata.panel) {
		ret = drm_panel_disable(ctx->pdata.panel);
		if (ret < 0)
			DRM_ERROR("failed to disable panel: %d.\n", ret);
	}

	DRM_DEV_DEBUG_DRIVER(dev, "drm disable\n");
	mutex_lock(&ctx->lock);

	anx7625_dp_stop(ctx);

	anx7625_low_power_mode_check(ctx, 0);

	mutex_unlock(&ctx->lock);
}

static const struct drm_bridge_funcs anx7625_bridge_funcs = {
	.attach = anx7625_bridge_attach,
	.detach = anx7625_bridge_detach,
	.disable = anx7625_bridge_disable,
	.post_disable = anx7625_post_disable,
	.pre_enable = anx7625_pre_enable,
	.mode_valid = anx7625_bridge_mode_valid,
	.mode_set = anx7625_bridge_mode_set,
	.enable = anx7625_bridge_enable,
};

static int anx7625_register_i2c_dummy_clients(struct anx7625_data *ctx,
					      struct i2c_client *client)
{
	ctx->i2c.tx_p0_client = i2c_new_dummy(client->adapter,
					      TX_P0_ADDR >> 1);
	if (!ctx->i2c.tx_p0_client)
		return -ENOMEM;

	ctx->i2c.tx_p1_client = i2c_new_dummy(client->adapter,
					      TX_P1_ADDR >> 1);
	if (!ctx->i2c.tx_p1_client)
		goto free_tx_p0;

	ctx->i2c.tx_p2_client = i2c_new_dummy(client->adapter,
					      TX_P2_ADDR >> 1);
	if (!ctx->i2c.tx_p2_client)
		goto free_tx_p1;

	ctx->i2c.rx_p0_client = i2c_new_dummy(client->adapter,
					      RX_P0_ADDR >> 1);
	if (!ctx->i2c.rx_p0_client)
		goto free_tx_p2;

	ctx->i2c.rx_p1_client = i2c_new_dummy(client->adapter,
					      RX_P1_ADDR >> 1);
	if (!ctx->i2c.rx_p1_client)
		goto free_rx_p0;

	ctx->i2c.rx_p2_client = i2c_new_dummy(client->adapter,
					      RX_P2_ADDR >> 1);
	if (!ctx->i2c.rx_p2_client)
		goto free_rx_p1;

	ctx->i2c.tcpc_client = i2c_new_dummy(client->adapter,
					     TCPC_INTERFACE_ADDR >> 1);
	if (!ctx->i2c.tcpc_client)
		goto free_rx_p2;

	return 0;

free_rx_p2:
	i2c_unregister_device(ctx->i2c.rx_p2_client);
free_rx_p1:
	i2c_unregister_device(ctx->i2c.rx_p1_client);
free_rx_p0:
	i2c_unregister_device(ctx->i2c.rx_p0_client);
free_tx_p2:
	i2c_unregister_device(ctx->i2c.tx_p2_client);
free_tx_p1:
	i2c_unregister_device(ctx->i2c.tx_p1_client);
free_tx_p0:
	i2c_unregister_device(ctx->i2c.tx_p0_client);

	return -ENOMEM;
}

static void anx7625_unregister_i2c_dummy_clients(struct anx7625_data *ctx)
{
	i2c_unregister_device(ctx->i2c.tx_p0_client);
	i2c_unregister_device(ctx->i2c.tx_p1_client);
	i2c_unregister_device(ctx->i2c.tx_p2_client);
	i2c_unregister_device(ctx->i2c.rx_p0_client);
	i2c_unregister_device(ctx->i2c.rx_p1_client);
	i2c_unregister_device(ctx->i2c.rx_p2_client);
	i2c_unregister_device(ctx->i2c.tcpc_client);
}

static int anx7625_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct anx7625_data *platform;
	struct anx7625_platform_data *pdata;
	int ret = 0;
	struct device *dev = &client->dev;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		DRM_DEV_ERROR(dev, "anx7625's i2c bus doesn't support\n");
		return -ENODEV;
	}

	platform = kzalloc(sizeof(*platform), GFP_KERNEL);
	if (!platform) {
		DRM_DEV_ERROR(dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	pdata = &platform->pdata;

	ret = anx7625_parse_dt(dev, pdata);
	if (ret) {
		DRM_DEV_ERROR(dev, "failed to parse devicetree.\n");
		goto free_platform;
	}

	platform->client = client;
	i2c_set_clientdata(client, platform);

	anx7625_init_gpio(platform);

	if (platform->pdata.extcon_supported) {
		/* get extcon device from DTS */
		ret = -EINVAL;
		platform->extcon = extcon_get_edev_by_phandle(dev, 0);
		if (PTR_ERR(platform->extcon) == -EPROBE_DEFER)
			goto free_platform;

		if (IS_ERR(platform->extcon)) {
			DRM_DEV_ERROR(dev,
				      "can not get extcon device!");
			goto free_platform;
		}

		ret = anx7625_extcon_notifier_init(platform);
		if (ret)
			goto free_platform;
	}

	atomic_set(&platform->power_status, 0);

	mutex_init(&platform->lock);

	platform->pdata.intp_irq = client->irq;
	if (platform->pdata.intp_irq) {
		INIT_WORK(&platform->work, anx7625_work_func);
		platform->workqueue = create_workqueue("anx7625_work");
		if (!platform->workqueue) {
			DRM_DEV_ERROR(dev, "failed to create work queue\n");
			ret = -ENOMEM;
			goto free_platform;
		}

		ret = devm_request_threaded_irq(dev, platform->pdata.intp_irq,
						NULL, anx7625_intr_hpd_isr,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"anx7625-intp", platform);
		if (ret) {
			DRM_DEV_ERROR(dev, "failed to request irq\n");
			goto free_wq;
		}
	}

	if (anx7625_register_i2c_dummy_clients(platform, client) != 0) {
		ret = -ENOMEM;
		DRM_ERROR("Failed to reserve I2C bus.\n");
		goto free_wq;
	}

	if (platform->pdata.low_power_mode == 0) {
		anx7625_disable_pd_protocol(platform);
		atomic_set(&platform->power_status, 1);
	}

	/* add work function */
	if (platform->pdata.intp_irq)
		queue_work(platform->workqueue, &platform->work);

	platform->bridge.funcs = &anx7625_bridge_funcs;
	if (IS_ENABLED(CONFIG_OF))
		platform->bridge.of_node = client->dev.of_node;
	drm_bridge_add(&platform->bridge);

	DRM_DEV_DEBUG_DRIVER(dev, "probe done\n");

	return 0;

free_wq:
	if (platform->workqueue)
		destroy_workqueue(platform->workqueue);

free_platform:
	kfree(platform);

	return ret;
}

static int anx7625_i2c_remove(struct i2c_client *client)
{
	struct anx7625_data *platform = i2c_get_clientdata(client);

	drm_bridge_remove(&platform->bridge);

	if (platform->pdata.intp_irq)
		destroy_workqueue(platform->workqueue);

	if (platform->pdata.extcon_supported) {
		devm_extcon_unregister_notifier(&platform->client->dev,
						platform->extcon,
						EXTCON_DISP_DP,
						&platform->event_nb);
		flush_work(&platform->extcon_wq);
	}

	anx7625_unregister_i2c_dummy_clients(platform);

	kfree(platform);
	return 0;
}

static const struct i2c_device_id anx7625_id[] = {
	{"anx7625", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, anx7625_id);

#ifdef CONFIG_OF
static const struct of_device_id anx_match_table[] = {
	{.compatible = "analogix,anx7625",},
	{},
};
#endif

static struct i2c_driver anx7625_driver = {
	.driver = {
		.name = "anx7625",
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif
	},
	.probe = anx7625_i2c_probe,
	.remove = anx7625_i2c_remove,

	.id_table = anx7625_id,
};

module_i2c_driver(anx7625_driver);

MODULE_DESCRIPTION("MIPI2DP anx7625 driver");
MODULE_AUTHOR("Xin Ji <xji@analogixsemi.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(ANX7625_DRV_VERSION);
