// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-mmsys.h>

#define MMSYS_OVL_MOUT_EN		0xf04
#define DISP_OVL0_GO_BLEND			BIT(0)
#define DISP_OVL0_GO_BG				BIT(1)
#define DISP_OVL0_2L_GO_BLEND			BIT(2)
#define DISP_OVL0_2L_GO_BG			BIT(3)
#define DISP_OVL1_2L_MOUT_EN		0xf08
#define OVL1_2L_MOUT_EN_RDMA1			BIT(4)
#define DISP_OVL0_2L_MOUT_EN		0xf18
#define DISP_OVL0_MOUT_EN		0xf1c
#define OVL0_MOUT_EN_DISP_RDMA0			BIT(0)
#define OVL0_MOUT_EN_OVL0_2L			BIT(4)
#define DISP_RDMA0_SEL_IN		0xf2c
#define RDMA0_SEL_IN_OVL0_2L			0x3
#define DISP_RDMA0_SOUT_SEL		0xf30
#define RDMA0_SOUT_COLOR0			0x1
#define DISP_CCORR0_SOUT_SEL		0xf34
#define CCORR0_SOUT_AAL0			0x1
#define DISP_AAL0_SEL_IN		0xf38
#define AAL0_SEL_IN_CCORR0			0x1
#define DISP_DITHER0_MOUT_EN		0xf3c
#define DITHER0_MOUT_DSI0			BIT(0)
#define DISP_DSI0_SEL_IN		0xf40
#define DSI0_SEL_IN_DITHER0			0x1
#define DISP_OVL2_2L_MOUT_EN		0xf4c
#define OVL2_2L_MOUT_RDMA4			BIT(0)

static void mtk_mmsys_ddp_mout_en(void __iomem *config_regs,
				  enum mtk_ddp_comp_id cur,
				  enum mtk_ddp_comp_id next,
				  bool enable)
{
	unsigned int addr, value, reg;

	if (cur == DDP_COMPONENT_OVL_2L0 && next == DDP_COMPONENT_RDMA0) {
		addr = DISP_OVL0_2L_MOUT_EN;
		value = OVL0_MOUT_EN_DISP_RDMA0;
	} else if (cur == DDP_COMPONENT_OVL_2L2 && next == DDP_COMPONENT_RDMA4) {
		addr = DISP_OVL2_2L_MOUT_EN;
		value = OVL2_2L_MOUT_RDMA4;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_DSI0) {
		addr = DISP_DITHER0_MOUT_EN;
		value = DITHER0_MOUT_DSI0;
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
		addr = DISP_RDMA0_SEL_IN;
		value = RDMA0_SEL_IN_OVL0_2L;
	} else if (cur == DDP_COMPONENT_CCORR && next == DDP_COMPONENT_AAL0) {
		addr = DISP_AAL0_SEL_IN;
		value = AAL0_SEL_IN_CCORR0;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_DSI0) {
		addr = DISP_DSI0_SEL_IN;
		value = DSI0_SEL_IN_DITHER0;
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
		writel_relaxed(RDMA0_SOUT_COLOR0, config_regs + DISP_RDMA0_SOUT_SEL);
	} else if (cur == DDP_COMPONENT_CCORR && next == DDP_COMPONENT_AAL0) {
		writel_relaxed(CCORR0_SOUT_AAL0, config_regs + DISP_CCORR0_SOUT_SEL);
	}
}

static void mtk_mmsys_ovl_mout_en(void __iomem *config_regs,
				  enum mtk_ddp_comp_id cur,
				  enum mtk_ddp_comp_id next,
				  bool enable)
{
	unsigned int addr, value, reg;

	addr = MMSYS_OVL_MOUT_EN;

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_OVL_2L0)
		value = DISP_OVL0_GO_BG;
	else if (cur == DDP_COMPONENT_OVL_2L0 && next == DDP_COMPONENT_OVL0)
		value = DISP_OVL0_2L_GO_BG;
	else if (cur == DDP_COMPONENT_OVL0)
		value = DISP_OVL0_GO_BLEND;
	else if (cur == DDP_COMPONENT_OVL_2L0)
		value = DISP_OVL0_2L_GO_BLEND;
	else
		value = 0;

	if (value) {
		reg = readl_relaxed(config_regs + addr);

		if (enable)
			reg |= value;
		else
			reg &= ~value;

		writel_relaxed(reg, config_regs + addr);
	}
}

struct mtk_mmsys_conn_funcs mt8192_mmsys_funcs = {
	.mout_en = mtk_mmsys_ddp_mout_en,
	.ovl_mout_en = mtk_mmsys_ovl_mout_en,
	.sel_in = mtk_mmsys_ddp_sel_in,
	.sout_sel = mtk_mmsys_ddp_sout_sel,
};
