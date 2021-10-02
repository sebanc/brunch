// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-mmsys.h>

#define DISP_OVL0_MOUT_EN		0xf00
#define DISP_OVL0_2L_MOUT_EN		0xf04
#define DISP_OVL1_2L_MOUT_EN		0xf08
#define DISP_DITHER0_MOUT_EN		0xf0c
#define DISP_PATH0_SEL_IN		0xf24
#define DISP_DSI0_SEL_IN		0xf2c
#define DISP_DPI0_SEL_IN		0xf30
#define DISP_RDMA0_SOUT_SEL_IN		0xf50
#define DISP_RDMA1_SOUT_SEL_IN		0xf54

#define OVL0_MOUT_EN_OVL0_2L			BIT(4)
#define OVL0_2L_MOUT_EN_DISP_PATH0		BIT(0)
#define OVL1_2L_MOUT_EN_RDMA1			BIT(4)
#define DITHER0_MOUT_IN_DSI0			BIT(0)
#define DISP_PATH0_SEL_IN_OVL0_2L		0x1
#define DSI0_SEL_IN_RDMA0			0x1
#define DSI0_SEL_IN_RDMA1			0x3
#define DPI0_SEL_IN_RDMA0			0x1
#define DPI0_SEL_IN_RDMA1			0x2
#define RDMA0_SOUT_COLOR0			0x1
#define RDMA1_SOUT_DSI0				0x1

static void mtk_mmsys_ddp_mout_en(void __iomem *config_regs,
				  enum mtk_ddp_comp_id cur,
				  enum mtk_ddp_comp_id next,
				  bool enable)
{
	unsigned int addr, value, reg;

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_OVL_2L0) {
		addr = DISP_OVL0_MOUT_EN;
		value = OVL0_MOUT_EN_OVL0_2L;
	} else if (cur == DDP_COMPONENT_OVL_2L0 && next == DDP_COMPONENT_RDMA0) {
		addr = DISP_OVL0_2L_MOUT_EN;
		value = OVL0_2L_MOUT_EN_DISP_PATH0;
	} else if (cur == DDP_COMPONENT_OVL_2L1 && next == DDP_COMPONENT_RDMA1) {
		addr = DISP_OVL1_2L_MOUT_EN;
		value = OVL1_2L_MOUT_EN_RDMA1;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_DSI0) {
		addr = DISP_DITHER0_MOUT_EN;
		value = DITHER0_MOUT_IN_DSI0;
	} else {
		value = 0;
	}

	if (value) {
		reg = readl_relaxed(config_regs + addr);

		if (enable)
			reg |= value;
		else
			reg &= ~value;

		writel_relaxed(reg, config_regs + addr);
	}
}

static void mtk_mmsys_ddp_sel_in(void __iomem *config_regs,
				 enum mtk_ddp_comp_id cur,
				 enum mtk_ddp_comp_id next,
				 bool enable)
{
	unsigned int addr, value, reg;

	if (cur == DDP_COMPONENT_OVL_2L0 && next == DDP_COMPONENT_RDMA0) {
		addr = DISP_PATH0_SEL_IN;
		value = DISP_PATH0_SEL_IN_OVL0_2L;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		addr = DISP_DPI0_SEL_IN;
		value = DPI0_SEL_IN_RDMA1;
	} else {
		value = 0;
	}

	if (value) {
		reg = readl_relaxed(config_regs + addr);

		if (enable)
			reg |= value;
		else
			reg &= ~value;

		writel_relaxed(reg, config_regs + addr);
	}
}

static void mtk_mmsys_ddp_sout_sel(void __iomem *config_regs,
				   enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next)
{
	if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_COLOR0) {
		writel_relaxed(RDMA0_SOUT_COLOR0, config_regs + DISP_RDMA0_SOUT_SEL_IN);
	}
}

struct mtk_mmsys_conn_funcs mt8183_mmsys_funcs = {
	.mout_en = mtk_mmsys_ddp_mout_en,
	.sel_in = mtk_mmsys_ddp_sel_in,
	.sout_sel = mtk_mmsys_ddp_sout_sel,
};
