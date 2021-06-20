/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <uapi/linux/wait.h>
#include <video/mipi_display.h>
#include <video/videomode.h>

#include "mtk_drm_ddp_comp.h"

#define DSI_VIDEO_FIFO_DEPTH	(1920 / 4)
#define DSI_HOST_FIFO_DEPTH	64

#define DSI_START		0x00

#define DSI_INTEN		0x08

#define DSI_INTSTA		0x0c
#define LPRX_RD_RDY_INT_FLAG		BIT(0)
#define CMD_DONE_INT_FLAG		BIT(1)
#define TE_RDY_INT_FLAG			BIT(2)
#define VM_DONE_INT_FLAG		BIT(3)
#define EXT_TE_RDY_INT_FLAG		BIT(4)
#define DSI_BUSY			BIT(31)

#define DSI_CON_CTRL		0x10
#define DSI_RESET			BIT(0)
#define DSI_EN				BIT(1)
#define DSI_DUAL			BIT(4)
#define DSI_MODE_CTRL		0x14
#define MODE				(3)
#define CMD_MODE			0
#define SYNC_PULSE_MODE			1
#define SYNC_EVENT_MODE			2
#define BURST_MODE			3
#define FRM_MODE			BIT(16)
#define MIX_MODE			BIT(17)

#define DSI_TXRX_CTRL		0x18
#define VC_NUM				(2 << 0)
#define LANE_NUM			(0xf << 2)
#define DIS_EOT				BIT(6)
#define NULL_EN				BIT(7)
#define TE_FREERUN			BIT(8)
#define EXT_TE_EN			BIT(9)
#define EXT_TE_EDGE			BIT(10)
#define MAX_RTN_SIZE			(0xf << 12)
#define HSTX_CKLP_EN			BIT(16)

#define DSI_PSCTRL		0x1c
#define DSI_PS_WC			0x3fff
#define DSI_PS_SEL			(3 << 16)
#define PACKED_PS_16BIT_RGB565		(0 << 16)
#define LOOSELY_PS_18BIT_RGB666		(1 << 16)
#define PACKED_PS_18BIT_RGB666		(2 << 16)
#define PACKED_PS_24BIT_RGB888		(3 << 16)

#define DSI_VSA_NL		0x20
#define DSI_VBP_NL		0x24
#define DSI_VFP_NL		0x28
#define DSI_VACT_NL		0x2C
#define DSI_HSA_WC		0x50
#define DSI_HBP_WC		0x54
#define DSI_HFP_WC		0x58

#define DSI_CMDQ_SIZE		0x60
#define CMDQ_SIZE		0x3f

#define DSI_HSTX_CKL_WC		0x64

#define DSI_RX_DATA0		0x74
#define DSI_RX_DATA1		0x78
#define DSI_RX_DATA2		0x7c
#define DSI_RX_DATA3		0x80

#define DSI_RACK		0x84
#define RACK				BIT(0)

#define DSI_MEM_CONTI		0x90

#define DSI_PHY_LCCON		0x104
#define LC_HS_TX_EN			BIT(0)
#define LC_ULPM_EN			BIT(1)
#define LC_WAKEUP_EN			BIT(2)

#define DSI_PHY_LD0CON		0x108
#define LD0_HS_TX_EN			BIT(0)
#define LD0_ULPM_EN			BIT(1)
#define LD0_WAKEUP_EN			BIT(2)

#define DSI_PHY_TIMECON0	0x110
#define LPX				(0xff << 0)
#define HS_PREP				(0xff << 8)
#define HS_ZERO				(0xff << 16)
#define HS_TRAIL			(0xff << 24)

#define DSI_PHY_TIMECON1	0x114
#define TA_GO				(0xff << 0)
#define TA_SURE				(0xff << 8)
#define TA_GET				(0xff << 16)
#define DA_HS_EXIT			(0xff << 24)

#define DSI_PHY_TIMECON2	0x118
#define CONT_DET			(0xff << 0)
#define CLK_ZERO			(0xff << 16)
#define CLK_TRAIL			(0xff << 24)

#define DSI_PHY_TIMECON3	0x11c
#define CLK_HS_PREP			(0xff << 0)
#define CLK_HS_POST			(0xff << 8)
#define CLK_HS_EXIT			(0xff << 16)

#define DSI_VM_CMD_CON		0x130
#define VM_CMD_EN			BIT(0)
#define TS_VFP_EN			BIT(5)

#define DSI_CMDQ0		0x200
#define CONFIG				(0xff << 0)
#define SHORT_PACKET			0
#define LONG_PACKET			2
#define BTA				BIT(2)
#define DATA_ID				(0xff << 8)
#define DATA_0				(0xff << 16)
#define DATA_1				(0xff << 24)

#define MMSYS_SW_RST_DSI_B	BIT(2)
#define MMSYS_SW_RST_DSI1_B	BIT(3)

#define T_LPX		5
#define T_HS_PREP	6
#define T_HS_TRAIL	8
#define T_HS_EXIT	7
#define T_HS_ZERO	10

#define NS_TO_CYCLE(n, c)    ((n) / (c) + (((n) % (c)) ? 1 : 0))

struct phy;

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct phy *phy;
	bool dual_dsi_mode;

	struct regmap *mmsys_sw_rst_b;
	u32 sw_rst_b;

	void __iomem *regs;
	void __iomem *regs_tx1;

	struct clk *engine_clk;
	struct clk *digital_clk;
	struct clk *engine1_clk;
	struct clk *digital1_clk;

	struct clk *hs_clk;

	u32 data_rate;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int refcount;
	bool enabled;
};

static inline struct mtk_dsi *encoder_to_dsi(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dsi, encoder);
}

static inline struct mtk_dsi *connector_to_dsi(struct drm_connector *c)
{
	return container_of(c, struct mtk_dsi, conn);
}

static inline struct mtk_dsi *host_to_dsi(struct mipi_dsi_host *h)
{
	return container_of(h, struct mtk_dsi, host);
}

static void mtk_dsi_write(struct mtk_dsi *dsi, u32 data, u32 offset)
{
	writel(data, dsi->regs + offset);
	if (dsi->dual_dsi_mode)
		writel(data, dsi->regs_tx1 + offset);
}

static void mtk_dsi_mask(struct mtk_dsi *dsi, u32 offset, u32 mask, u32 data)
{
	u32 temp = readl(dsi->regs + offset);

	mtk_dsi_write(dsi, (temp & ~mask) | (data & mask), offset);
}

static void dsi_phy_timconfig(struct mtk_dsi *dsi)
{
	u32 timcon0, timcon1, timcon2, timcon3;
	u32 ui, cycle_time;

	ui = 1000 / dsi->data_rate + 0x01;
	cycle_time = 8000 / dsi->data_rate + 0x01;

	timcon0 = T_LPX | T_HS_PREP << 8 | T_HS_ZERO << 16 | T_HS_TRAIL << 24;
	timcon1 = 4 * T_LPX | (3 * T_LPX / 2) << 8 | 5 * T_LPX << 16 |
		  T_HS_EXIT << 24;
	timcon2 = ((NS_TO_CYCLE(0x64, cycle_time) + 0xa) << 24) |
		  (NS_TO_CYCLE(0x150, cycle_time) << 16);
	timcon3 = NS_TO_CYCLE(0x40, cycle_time) | (2 * T_LPX) << 16 |
		  NS_TO_CYCLE(80 + 52 * ui, cycle_time) << 8;

	mtk_dsi_write(dsi, timcon0, DSI_PHY_TIMECON0);
	mtk_dsi_write(dsi, timcon1, DSI_PHY_TIMECON1);
	mtk_dsi_write(dsi, timcon2, DSI_PHY_TIMECON2);
	mtk_dsi_write(dsi, timcon3, DSI_PHY_TIMECON3);
}

static void mtk_dsi_enable(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_EN, DSI_EN);
}

static void mtk_dsi_disable(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_EN, 0);
}

static void mtk_dsi_reset(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_RESET, DSI_RESET);
	mtk_dsi_mask(dsi, DSI_CON_CTRL, DSI_RESET, 0);
}

static void mtk_dsi_reset_all(struct mtk_dsi *dsi)
{
	regmap_update_bits(dsi->mmsys_sw_rst_b, dsi->sw_rst_b,
		MMSYS_SW_RST_DSI_B, 0);
	regmap_update_bits(dsi->mmsys_sw_rst_b, dsi->sw_rst_b,
		MMSYS_SW_RST_DSI1_B, 0);
	usleep_range(1000, 1100);
	regmap_update_bits(dsi->mmsys_sw_rst_b, dsi->sw_rst_b,
		MMSYS_SW_RST_DSI_B, MMSYS_SW_RST_DSI_B);
	regmap_update_bits(dsi->mmsys_sw_rst_b, dsi->sw_rst_b,
		MMSYS_SW_RST_DSI1_B, MMSYS_SW_RST_DSI1_B);
}

static int mtk_dsi_poweron(struct mtk_dsi *dsi)
{
	struct device *dev = dsi->dev;
	int ret;
	u64 pixel_clock, total_bits;
	u32 htotal, htotal_bits, bit_per_pixel, overhead_cycles, overhead_bits;
	unsigned long mipi_tx_rate;

	if (++dsi->refcount != 1)
		return 0;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB565:
		bit_per_pixel = 16;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		bit_per_pixel = 18;
		break;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB888:
	default:
		bit_per_pixel = 24;
		break;
	}

	/**
	 * vm.pixelclock is in kHz, pixel_clock unit is Hz, so multiply by 1000
	 * htotal_time = htotal * byte_per_pixel / num_lanes
	 * overhead_time = lpx + hs_prepare + hs_zero + hs_trail + hs_exit
	 * mipi_ratio = (htotal_time + overhead_time) / htotal_time
	 * data_rate = pixel_clock * bit_per_pixel * mipi_ratio / num_lanes;
	 */
	pixel_clock = dsi->vm.pixelclock * 1000;
	htotal = dsi->vm.hactive + dsi->vm.hback_porch + dsi->vm.hfront_porch +
			dsi->vm.hsync_len;
	htotal_bits = htotal * bit_per_pixel;

	overhead_cycles = T_LPX + T_HS_PREP + T_HS_ZERO + T_HS_TRAIL +
			T_HS_EXIT;
	overhead_bits = overhead_cycles * dsi->lanes * 8;
	total_bits = htotal_bits + overhead_bits;

	dsi->data_rate = DIV_ROUND_UP_ULL(pixel_clock * total_bits,
					  htotal * dsi->lanes);

	mipi_tx_rate = dsi->data_rate;

	/* Store DSI data rate in MHz */
	dsi->data_rate /= 1000000;

	DRM_INFO("set mipitx's data rate: %lu Hz\n", mipi_tx_rate);
	ret = clk_set_rate(dsi->hs_clk, mipi_tx_rate);
	if (ret < 0) {
		dev_err(dev, "Failed to set data rate: %d\n", ret);
		goto err_refcount;
	}

	phy_power_on(dsi->phy);

	ret = clk_prepare_enable(dsi->engine_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable engine clock: %d\n", ret);
		goto err_phy_power_off;
	}

	ret = clk_prepare_enable(dsi->digital_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable digital clock: %d\n", ret);
		goto err_disable_engine_clk;
	}

	if (dsi->dual_dsi_mode) {
		ret = clk_prepare_enable(dsi->engine1_clk);
		if (ret < 0) {
			dev_err(dev, "Failed to enable engine1 clock: %d\n", ret);
			goto err_disable_digital_clk;
		}

		ret = clk_prepare_enable(dsi->digital1_clk);
		if (ret < 0) {
			dev_err(dev, "Failed to enable digital clock: %d\n", ret);
			goto err_disable_engine1_clk;
		}
	}

	mtk_dsi_enable(dsi);
	mtk_dsi_reset(dsi);
	dsi_phy_timconfig(dsi);

	return 0;

err_disable_engine1_clk:
	clk_disable_unprepare(dsi->engine1_clk);
err_disable_digital_clk:
	clk_disable_unprepare(dsi->digital_clk);
err_disable_engine_clk:
	clk_disable_unprepare(dsi->engine_clk);
err_phy_power_off:
	phy_power_off(dsi->phy);
err_refcount:
	dsi->refcount--;
	return ret;
}

static void dsi_clk_ulp_mode_enter(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_ULPM_EN, LC_ULPM_EN);
}

static void dsi_clk_ulp_mode_leave(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_ULPM_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_WAKEUP_EN, LC_WAKEUP_EN);
	mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_WAKEUP_EN, 0);
}

static void dsi_lane0_ulp_mode_enter(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_HS_TX_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_ULPM_EN, LD0_ULPM_EN);
}

static void dsi_lane0_ulp_mode_leave(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_ULPM_EN, 0);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_WAKEUP_EN, LD0_WAKEUP_EN);
	mtk_dsi_mask(dsi, DSI_PHY_LD0CON, LD0_WAKEUP_EN, 0);
}

static bool dsi_clk_hs_state(struct mtk_dsi *dsi)
{
	u32 tmp_reg1;

	tmp_reg1 = readl(dsi->regs + DSI_PHY_LCCON);
	return ((tmp_reg1 & LC_HS_TX_EN) == 1) ? true : false;
}

static void dsi_clk_hs_mode(struct mtk_dsi *dsi, bool enter)
{
	if (enter && !dsi_clk_hs_state(dsi))
		mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, LC_HS_TX_EN);
	else if (!enter && dsi_clk_hs_state(dsi))
		mtk_dsi_mask(dsi, DSI_PHY_LCCON, LC_HS_TX_EN, 0);
}

static void dsi_set_mode(struct mtk_dsi *dsi)
{
	u32 vid_mode = CMD_MODE;

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			vid_mode = BURST_MODE;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			vid_mode = SYNC_PULSE_MODE;
		else
			vid_mode = SYNC_EVENT_MODE;
	}

	mtk_dsi_write(dsi, vid_mode, DSI_MODE_CTRL);
}

static void mtk_dsi_set_vm_cmd(struct mtk_dsi *dsi)
{
	mtk_dsi_mask(dsi, DSI_VM_CMD_CON, VM_CMD_EN, VM_CMD_EN);
	mtk_dsi_mask(dsi, DSI_VM_CMD_CON, TS_VFP_EN, TS_VFP_EN);
}

static void dsi_ps_control_vact(struct mtk_dsi *dsi)
{
	struct videomode *vm = &dsi->vm;
	u32 dsi_buf_bpp, ps_wc;
	u32 ps_bpp_mode;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_buf_bpp = 2;
	else
		dsi_buf_bpp = 3;

	ps_wc = vm->hactive * dsi_buf_bpp;

	ps_bpp_mode = ps_wc;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		ps_bpp_mode |= PACKED_PS_24BIT_RGB888;
		break;
	case MIPI_DSI_FMT_RGB666:
		ps_bpp_mode |= PACKED_PS_18BIT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		ps_bpp_mode |= LOOSELY_PS_18BIT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB565:
		ps_bpp_mode |= PACKED_PS_16BIT_RGB565;
		break;
	}

	mtk_dsi_write(dsi, vm->vactive, DSI_VACT_NL);
	mtk_dsi_write(dsi, ps_bpp_mode, DSI_PSCTRL);
	mtk_dsi_write(dsi, ps_wc, DSI_HSTX_CKL_WC);
}

static void dsi_rxtx_control(struct mtk_dsi *dsi)
{
	u32 tmp_reg;

	switch (dsi->lanes) {
	case 1:
		tmp_reg = 1 << 2;
		break;
	case 2:
		tmp_reg = 3 << 2;
		break;
	case 3:
		tmp_reg = 7 << 2;
		break;
	case 4:
		tmp_reg = 0xf << 2;
		break;
	default:
		tmp_reg = 0xf << 2;
		break;
	}

	tmp_reg |= (dsi->mode_flags & MIPI_DSI_CLOCK_NON_CONTINUOUS) << 6;
	tmp_reg |= (dsi->mode_flags & MIPI_DSI_MODE_EOT_PACKET) >> 3;

	mtk_dsi_write(dsi, tmp_reg, DSI_TXRX_CTRL);
}

static void dsi_ps_control(struct mtk_dsi *dsi)
{
	unsigned int dsi_tmp_buf_bpp;
	u32 tmp_reg;
	unsigned int hactive;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		tmp_reg = PACKED_PS_24BIT_RGB888;
		dsi_tmp_buf_bpp = 3;
		break;
	case MIPI_DSI_FMT_RGB666:
		tmp_reg = LOOSELY_PS_18BIT_RGB666;
		dsi_tmp_buf_bpp = 3;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		tmp_reg = PACKED_PS_18BIT_RGB666;
		dsi_tmp_buf_bpp = 3;
		break;
	case MIPI_DSI_FMT_RGB565:
		tmp_reg = PACKED_PS_16BIT_RGB565;
		dsi_tmp_buf_bpp = 2;
		break;
	default:
		tmp_reg = PACKED_PS_24BIT_RGB888;
		dsi_tmp_buf_bpp = 3;
		break;
	}

	hactive = dsi->vm.hactive;
	tmp_reg += (hactive * dsi_tmp_buf_bpp) & DSI_PS_WC;
	mtk_dsi_write(dsi, tmp_reg, DSI_PSCTRL);
}

static void dsi_config_vdo_timing(struct mtk_dsi *dsi)
{
	unsigned int horizontal_sync_active_byte;
	unsigned int horizontal_backporch_byte;
	unsigned int horizontal_frontporch_byte;
	unsigned int dsi_tmp_buf_bpp;

	struct videomode *vm = &dsi->vm;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_tmp_buf_bpp = 2;
	else
		dsi_tmp_buf_bpp = 3;

	mtk_dsi_write(dsi, vm->vsync_len, DSI_VSA_NL);
	mtk_dsi_write(dsi, vm->vback_porch, DSI_VBP_NL);
	mtk_dsi_write(dsi, vm->vfront_porch, DSI_VFP_NL);
	mtk_dsi_write(dsi, vm->vactive, DSI_VACT_NL);

	horizontal_sync_active_byte = (vm->hsync_len * dsi_tmp_buf_bpp - 10);

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
		horizontal_backporch_byte =
			(vm->hback_porch * dsi_tmp_buf_bpp - 10);
	else
		horizontal_backporch_byte = ((vm->hback_porch + vm->hsync_len) *
			dsi_tmp_buf_bpp - 10);

	horizontal_frontporch_byte = (vm->hfront_porch * dsi_tmp_buf_bpp - 12);

	mtk_dsi_write(dsi, horizontal_sync_active_byte, DSI_HSA_WC);
	mtk_dsi_write(dsi, horizontal_backporch_byte, DSI_HBP_WC);
	mtk_dsi_write(dsi, horizontal_frontporch_byte, DSI_HFP_WC);

	dsi_ps_control(dsi);
}

static void mtk_dsi_start(struct mtk_dsi *dsi)
{
	mtk_dsi_write(dsi, 0, DSI_START);
	/* Only start the master DSI */
	writel(1, dsi->regs + DSI_START);
}

static void mtk_dsi_stop(struct mtk_dsi *dsi)
{
	mtk_dsi_write(dsi, 0, DSI_START);
}

static void mtk_dsi_poweroff(struct mtk_dsi *dsi)
{
	if (WARN_ON(dsi->refcount == 0))
		return;

	if (--dsi->refcount != 0)
		return;

	if (!dsi->dual_dsi_mode) {
		dsi_lane0_ulp_mode_enter(dsi);
		dsi_clk_ulp_mode_enter(dsi);

		mtk_dsi_disable(dsi);
	}

	clk_disable_unprepare(dsi->engine_clk);
	clk_disable_unprepare(dsi->digital_clk);
	if (dsi->dual_dsi_mode) {
		clk_disable_unprepare(dsi->engine1_clk);
		clk_disable_unprepare(dsi->digital1_clk);
	}
	phy_power_off(dsi->phy);
}

static void mtk_dsi_tx_cmd_type0(struct mtk_dsi *dsi, u8 cmd)
{
	u32 counter = 0;

	mtk_dsi_write(dsi, 0, DSI_START);
	mtk_dsi_write(dsi, 0x00000500 | (cmd << 16), DSI_CMDQ0);
	mtk_dsi_write(dsi, 1, DSI_CMDQ_SIZE);
	mtk_dsi_write(dsi, 0, DSI_INTSTA);
	mtk_dsi_write(dsi, 1, DSI_START);

	for (counter = 0; counter < 100; counter++) {
		if (readl(dsi->regs + DSI_INTSTA) & CMD_DONE_INT_FLAG)
			break;
		usleep_range(10, 15);
	}

	if (dsi->dual_dsi_mode) {
		for (counter = 0; counter < 100; counter++) {
			u32 intsta = readl(dsi->regs_tx1 + DSI_INTSTA);
			if (intsta & CMD_DONE_INT_FLAG)
				break;
			usleep_range(10, 15);
		}
	}
}

static void mtk_output_dsi_enable(struct mtk_dsi *dsi)
{
	int ret;

	if (dsi->enabled)
		return;

	if (!dsi->dual_dsi_mode) {
		if (dsi->panel) {
			if (drm_panel_prepare(dsi->panel)) {
				DRM_ERROR("failed to prepare the panel\n");
				return;
			}
		}
	}

	ret = mtk_dsi_poweron(dsi);
	if (ret < 0) {
		DRM_ERROR("failed to power on dsi\n");
		return;
	}

	dsi_rxtx_control(dsi);
	if (dsi->dual_dsi_mode) {
		dsi_ps_control_vact(dsi);
		mtk_dsi_set_vm_cmd(dsi);
		dsi_config_vdo_timing(dsi);
	}

	dsi_clk_ulp_mode_leave(dsi);
	dsi_lane0_ulp_mode_leave(dsi);
	dsi_clk_hs_mode(dsi, 0);

	if (!dsi->dual_dsi_mode) {
		dsi_set_mode(dsi);

		dsi_ps_control_vact(dsi);
		dsi_config_vdo_timing(dsi);
	} else {
		if (dsi->panel) {
			if (drm_panel_prepare(dsi->panel)) {
				DRM_ERROR("failed to prepare the panel\n");
				return;
			}
		}
	}

	dsi_set_mode(dsi);
	dsi_clk_hs_mode(dsi, 1);

	if (!dsi->dual_dsi_mode) {
		mtk_dsi_start(dsi);
	} else {

		mtk_dsi_write(dsi, 0, DSI_START);
		/* disable dual_dsi for send cmd */
		mtk_dsi_write(dsi, DSI_EN, DSI_CON_CTRL);

		mtk_dsi_write(dsi, CMD_MODE, DSI_MODE_CTRL);
		mtk_dsi_write(dsi, 1, DSI_START);

		mtk_dsi_tx_cmd_type0(dsi, MIPI_DCS_EXIT_SLEEP_MODE);
		mtk_dsi_tx_cmd_type0(dsi, MIPI_DCS_SET_DISPLAY_ON);

		mtk_dsi_write(dsi, 0, DSI_START);

		mtk_dsi_write(dsi, DSI_EN | DSI_DUAL, DSI_CON_CTRL);

		mtk_dsi_write(dsi, BURST_MODE, DSI_MODE_CTRL);

		mtk_dsi_start(dsi);

		if (dsi->panel) {
			if (drm_panel_enable(dsi->panel)) {
				DRM_ERROR("failed to enable the panel\n");

				if(drm_panel_unprepare(dsi->panel))
					DRM_ERROR("failed to unprepare the panel\n");

				mtk_dsi_stop(dsi);
				mtk_dsi_poweroff(dsi);
				return;
			}
		}
	}
	dsi->enabled = true;
}

static void mtk_output_dsi_disable(struct mtk_dsi *dsi)
{
	if (!dsi->enabled)
		return;

	if (dsi->panel) {
		if (drm_panel_disable(dsi->panel)) {
			DRM_ERROR("failed to disable the panel\n");
			return;
		}
	}

	if (dsi->dual_dsi_mode) {
		mtk_dsi_stop(dsi);

		if (dsi->panel)
			if (drm_panel_unprepare(dsi->panel))
				DRM_ERROR("failed to unprepare the panel\n");

		mtk_dsi_reset(dsi);
		dsi_lane0_ulp_mode_enter(dsi);
		dsi_clk_ulp_mode_enter(dsi);
		mtk_dsi_disable(dsi);
	}

	mtk_dsi_poweroff(dsi);

	dsi->enabled = false;
}

static void mtk_dsi_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs mtk_dsi_encoder_funcs = {
	.destroy = mtk_dsi_encoder_destroy,
};

static bool mtk_dsi_encoder_mode_fixup(struct drm_encoder *encoder,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_dsi_encoder_mode_set(struct drm_encoder *encoder,
				     struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	dsi->vm.pixelclock = adjusted->clock;
	dsi->vm.hactive = adjusted->hdisplay;
	dsi->vm.hback_porch = adjusted->htotal - adjusted->hsync_end;
	dsi->vm.hfront_porch = adjusted->hsync_start - adjusted->hdisplay;
	dsi->vm.hsync_len = adjusted->hsync_end - adjusted->hsync_start;

	if (dsi->dual_dsi_mode) {
		dsi->vm.pixelclock /= 2;
		dsi->vm.hactive /= 2;
		dsi->vm.hback_porch /= 2;
		dsi->vm.hfront_porch /= 2;
		dsi->vm.hsync_len /= 2;
	}
	dsi->vm.vactive = adjusted->vdisplay;
	dsi->vm.vback_porch = adjusted->vtotal - adjusted->vsync_end;
	dsi->vm.vfront_porch = adjusted->vsync_start - adjusted->vdisplay;
	dsi->vm.vsync_len = adjusted->vsync_end - adjusted->vsync_start;
}

static void mtk_dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	mtk_output_dsi_disable(dsi);
}

static void mtk_dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct mtk_dsi *dsi = encoder_to_dsi(encoder);

	mtk_output_dsi_enable(dsi);
}

static int mtk_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_dsi *dsi = connector_to_dsi(connector);

	return drm_panel_get_modes(dsi->panel);
}

static struct drm_encoder *mtk_dsi_connector_best_encoder(
		struct drm_connector *connector)
{
	struct mtk_dsi *dsi = connector_to_dsi(connector);

	return &dsi->encoder;
}

static const struct drm_encoder_helper_funcs mtk_dsi_encoder_helper_funcs = {
	.mode_fixup = mtk_dsi_encoder_mode_fixup,
	.mode_set = mtk_dsi_encoder_mode_set,
	.disable = mtk_dsi_encoder_disable,
	.enable = mtk_dsi_encoder_enable,
};

static const struct drm_connector_funcs mtk_dsi_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
	mtk_dsi_connector_helper_funcs = {
	.get_modes = mtk_dsi_connector_get_modes,
	.best_encoder = mtk_dsi_connector_best_encoder,
};

static int mtk_dsi_create_connector(struct drm_device *drm, struct mtk_dsi *dsi)
{
	int ret;

	ret = drm_connector_init(drm, &dsi->conn, &mtk_dsi_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_ERROR("Failed to connector init to drm\n");
		return ret;
	}

	drm_connector_helper_add(&dsi->conn, &mtk_dsi_connector_helper_funcs);

	dsi->conn.dpms = DRM_MODE_DPMS_OFF;
	drm_mode_connector_attach_encoder(&dsi->conn, &dsi->encoder);

	if (dsi->panel) {
		ret = drm_panel_attach(dsi->panel, &dsi->conn);
		if (ret) {
			DRM_ERROR("Failed to attach panel to drm\n");
			goto err_connector_cleanup;
		}
	}

	if (dsi->dual_dsi_mode)
		mtk_dsi_reset_all(dsi);

	return 0;

err_connector_cleanup:
	drm_connector_cleanup(&dsi->conn);
	return ret;
}

static int mtk_dsi_create_conn_enc(struct drm_device *drm, struct mtk_dsi *dsi)
{
	int ret;

	ret = drm_encoder_init(drm, &dsi->encoder, &mtk_dsi_encoder_funcs,
			       DRM_MODE_ENCODER_DSI, NULL);
	if (ret) {
		DRM_ERROR("Failed to encoder init to drm\n");
		return ret;
	}
	drm_encoder_helper_add(&dsi->encoder, &mtk_dsi_encoder_helper_funcs);

	/*
	 * Currently display data paths are statically assigned to a crtc each.
	 * crtc 0 is OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0
	 */
	dsi->encoder.possible_crtcs = 1;

	/* If there's a bridge, attach to it and let it create the connector */
	ret = drm_bridge_attach(&dsi->encoder, dsi->bridge, NULL);
	if (ret) {
		DRM_ERROR("Failed to attach bridge to drm\n");

		/* Otherwise create our own connector and attach to a panel */
		ret = mtk_dsi_create_connector(drm, dsi);
		if (ret)
			goto err_encoder_cleanup;
	}

	return 0;

err_encoder_cleanup:
	drm_encoder_cleanup(&dsi->encoder);
	return ret;
}

static void mtk_dsi_destroy_conn_enc(struct mtk_dsi *dsi)
{
	drm_encoder_cleanup(&dsi->encoder);
	/* Skip connector cleanup if creation was delegated to the bridge */
	if (dsi->conn.dev)
		drm_connector_cleanup(&dsi->conn);
	if (dsi->panel)
		drm_panel_detach(dsi->panel);
}

static void mtk_dsi_ddp_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	mtk_dsi_poweron(dsi);
}

static void mtk_dsi_ddp_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi *dsi = container_of(comp, struct mtk_dsi, ddp_comp);

	mtk_dsi_poweroff(dsi);
}

static const struct mtk_ddp_comp_funcs mtk_dsi_funcs = {
	.prepare = mtk_dsi_ddp_prepare,
	.unprepare = mtk_dsi_ddp_unprepare,
};

static int mtk_dsi_host_attach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_dsi(host);

	dsi->lanes = device->lanes;
	dsi->format = device->format;
	dsi->mode_flags = device->mode_flags;

	if (dsi->conn.dev)
		drm_helper_hpd_irq_event(dsi->conn.dev);

	return 0;
}

static int mtk_dsi_host_detach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct mtk_dsi *dsi = host_to_dsi(host);

	if (dsi->conn.dev)
		drm_helper_hpd_irq_event(dsi->conn.dev);

	return 0;
}

static const struct mipi_dsi_host_ops mtk_dsi_ops = {
	.attach = mtk_dsi_host_attach,
	.detach = mtk_dsi_host_detach,
};

static int mtk_dsi_bind(struct device *dev, struct device *master, void *data)
{
	int ret;
	struct drm_device *drm = data;
	struct mtk_dsi *dsi = dev_get_drvdata(dev);

	ret = mtk_ddp_comp_register(drm, &dsi->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	if (!dsi->dual_dsi_mode) {
		ret = mipi_dsi_host_register(&dsi->host);
		if (ret < 0) {
			dev_err(dev, "failed to register DSI host: %d\n", ret);
			goto err_ddp_comp_unregister;
		}
	}

	ret = mtk_dsi_create_conn_enc(drm, dsi);
	if (ret) {
		DRM_ERROR("Encoder create failed with %d\n", ret);
		goto err_unregister;
	}

	return 0;

err_unregister:
	if (!dsi->dual_dsi_mode)
		mipi_dsi_host_unregister(&dsi->host);
err_ddp_comp_unregister:
	mtk_ddp_comp_unregister(drm, &dsi->ddp_comp);
	return ret;
}

static void mtk_dsi_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct drm_device *drm = data;
	struct mtk_dsi *dsi = dev_get_drvdata(dev);

	mtk_dsi_destroy_conn_enc(dsi);
	mipi_dsi_host_unregister(&dsi->host);
	mtk_ddp_comp_unregister(drm, &dsi->ddp_comp);
}

static const struct component_ops mtk_dsi_component_ops = {
	.bind = mtk_dsi_bind,
	.unbind = mtk_dsi_unbind,
};

static int mtk_dsi_probe(struct platform_device *pdev)
{
	struct mtk_dsi *dsi;
	struct device *dev = &pdev->dev;
	struct device_node *remote_node, *endpoint;
	struct resource *regs;
	int comp_id;
	int ret;

	dsi = devm_kzalloc(dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;

	dsi->host.ops = &mtk_dsi_ops;
	dsi->host.dev = dev;
	dsi->dual_dsi_mode = of_property_read_bool(dev->of_node,
						   "mediatek,dual-dsi-mode");

	if (!dsi->dual_dsi_mode) {
		endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				dev_err(dev, "No panel connected\n");
				return -ENODEV;
			}

			dsi->bridge = of_drm_find_bridge(remote_node);
			dsi->panel = of_drm_find_panel(remote_node);
			of_node_put(remote_node);
			if (!dsi->bridge && !dsi->panel) {
				dev_info(dev, "Waiting for bridge or panel driver\n");
				return -EPROBE_DEFER;
			}
		}
	} else {
		dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
		dsi->format = MIPI_DSI_FMT_RGB888;
		dsi->lanes = 4;
		dsi->dev = dev;
	}

	dsi->engine_clk = devm_clk_get(dev, "engine");
	if (IS_ERR(dsi->engine_clk)) {
		ret = PTR_ERR(dsi->engine_clk);
		dev_err(dev, "Failed to get engine clock: %d\n", ret);
		return ret;
	}

	dsi->digital_clk = devm_clk_get(dev, "digital");
	if (IS_ERR(dsi->digital_clk)) {
		ret = PTR_ERR(dsi->digital_clk);
		dev_err(dev, "Failed to get digital clock: %d\n", ret);
		return ret;
	}

	dsi->hs_clk = devm_clk_get(dev, "hs");
	if (IS_ERR(dsi->hs_clk)) {
		ret = PTR_ERR(dsi->hs_clk);
		dev_err(dev, "Failed to get hs clock: %d\n", ret);
		return ret;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsi->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(dsi->regs)) {
		ret = PTR_ERR(dsi->regs);
		dev_err(dev, "Failed to ioremap memory: %d\n", ret);
		return ret;
	}

	dsi->phy = devm_phy_get(dev, "dphy");
	if (IS_ERR(dsi->phy)) {
		ret = PTR_ERR(dsi->phy);
		dev_err(dev, "Failed to get MIPI-DPHY: %d\n", ret);
		return ret;
	}

	if (dsi->dual_dsi_mode) {
		dsi->engine1_clk = devm_clk_get(dev, "engine1");
		if (IS_ERR(dsi->engine1_clk)) {
			ret = PTR_ERR(dsi->engine1_clk);
			dev_err(dev, "Failed to get engine1 clock: %d\n", ret);
			return ret;
		}

		dsi->digital1_clk = devm_clk_get(dev, "digital1");
		if (IS_ERR(dsi->digital1_clk)) {
			ret = PTR_ERR(dsi->digital1_clk);
			dev_err(dev, "Failed to get digital1 clock: %d\n", ret);
			return ret;
		}

		regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		dsi->regs_tx1 = devm_ioremap_resource(dev, regs);
		if (IS_ERR(dsi->regs_tx1)) {
			ret = PTR_ERR(dsi->regs_tx1);
			dev_err(dev, "Failed to ioremap dsi1 regs: %d\n",
				ret);
			return ret;
		}

		dsi->mmsys_sw_rst_b = syscon_regmap_lookup_by_phandle(dev->of_node,
				"mediatek,syscon-dsi");
		if (IS_ERR(dsi->mmsys_sw_rst_b)) {
			dev_err(dev, "Failed to get sys config register\n");
			return PTR_ERR(dsi->mmsys_sw_rst_b);
		}

		ret = of_property_read_u32_index(dev->of_node, "mediatek,syscon-dsi",
						 1, &dsi->sw_rst_b);
		if (ret) {
			dev_err(dev, "Failed to get reset con: %d\n", ret);
			return ret;
		}

		endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
		if (!endpoint) {
			dev_err(dev, "No endpoint found\n");
			return -ENODEV;
		}

		remote_node = of_graph_get_remote_port_parent(endpoint);
		if (!remote_node) {
			dev_err(dev, "No panel connected\n");
			return -ENODEV;
		}

		ret = mipi_dsi_host_register(&dsi->host);
		if (ret < 0) {
			dev_err(dev, "failed to register DSI host: %d\n", ret);
			return -EPROBE_DEFER;
		}

		dsi->bridge = of_drm_find_bridge(remote_node);
		dsi->panel = of_drm_find_panel(remote_node);
		of_node_put(remote_node);

		if (!dsi->bridge && !dsi->panel) {
			dev_err(dev, "Waiting for bridge or panel driver\n");
			ret = -EPROBE_DEFER;
			goto err_mipi_dsi_host_unregister;
		}
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DSI);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
			goto err_mipi_dsi_host_unregister;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dsi->ddp_comp, comp_id,
				&mtk_dsi_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto err_mipi_dsi_host_unregister;
	}

	platform_set_drvdata(pdev, dsi);

	return component_add(&pdev->dev, &mtk_dsi_component_ops);

err_mipi_dsi_host_unregister:
	if (dsi->dual_dsi_mode)
		mipi_dsi_host_unregister(&dsi->host);

	return ret;
}

static int mtk_dsi_remove(struct platform_device *pdev)
{
	struct mtk_dsi *dsi = platform_get_drvdata(pdev);

	mtk_output_dsi_disable(dsi);
	component_del(&pdev->dev, &mtk_dsi_component_ops);

	return 0;
}

static const struct of_device_id mtk_dsi_of_match[] = {
	{ .compatible = "mediatek,mt8173-dsi" },
	{ },
};

struct platform_driver mtk_dsi_driver = {
	.probe = mtk_dsi_probe,
	.remove = mtk_dsi_remove,
	.driver = {
		.name = "mtk-dsi",
		.of_match_table = mtk_dsi_of_match,
	},
};
