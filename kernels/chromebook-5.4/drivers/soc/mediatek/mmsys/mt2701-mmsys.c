// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-mmsys.h>

#define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN	0x040
#define DISP_REG_CONFIG_DISP_OVL1_MOUT_EN	0x044
#define DISP_REG_CONFIG_DISP_OD_MOUT_EN		0x048
#define DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN	0x04c
#define DISP_REG_CONFIG_DISP_UFOE_MOUT_EN	0x050
#define DISP_REG_CONFIG_DISP_COLOR0_SEL_IN	0x084
#define DISP_REG_CONFIG_DISP_COLOR1_SEL_IN	0x088
#define DISP_REG_CONFIG_DSIE_SEL_IN		0x0a4
#define DISP_REG_CONFIG_DSIO_SEL_IN		0x0a8
#define DISP_REG_CONFIG_DPI_SEL_IN		0x0ac
#define DISP_REG_CONFIG_DISP_RDMA2_SOUT		0x0b8
#define DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN	0x0c4
#define DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN	0x0c8
#define DISP_REG_CONFIG_MMSYS_CG_CON0		0x100

#define DISP_REG_CONFIG_DISP_OVL_MOUT_EN	0x030
#define DISP_REG_CONFIG_OUT_SEL			0x04c
#define DISP_REG_CONFIG_DSI_SEL			0x050
#define DISP_REG_CONFIG_DPI_SEL			0x064

#define OVL0_MOUT_EN_COLOR0			0x1
#define OD_MOUT_EN_RDMA0			0x1
#define OD1_MOUT_EN_RDMA1			BIT(16)
#define UFOE_MOUT_EN_DSI0			0x1
#define COLOR0_SEL_IN_OVL0			0x1
#define OVL1_MOUT_EN_COLOR1			0x1
#define GAMMA_MOUT_EN_RDMA1			0x1
#define RDMA0_SOUT_DPI0				0x2
#define RDMA0_SOUT_DPI1				0x3
#define RDMA0_SOUT_DSI1				0x1
#define RDMA0_SOUT_DSI2				0x4
#define RDMA0_SOUT_DSI3				0x5
#define RDMA1_SOUT_DPI0				0x2
#define RDMA1_SOUT_DPI1				0x3
#define RDMA1_SOUT_DSI1				0x1
#define RDMA1_SOUT_DSI2				0x4
#define RDMA1_SOUT_DSI3				0x5
#define RDMA2_SOUT_DPI0				0x2
#define RDMA2_SOUT_DPI1				0x3
#define RDMA2_SOUT_DSI1				0x1
#define RDMA2_SOUT_DSI2				0x4
#define RDMA2_SOUT_DSI3				0x5
#define DPI0_SEL_IN_RDMA1			0x1
#define DPI0_SEL_IN_RDMA2			0x3
#define DPI1_SEL_IN_RDMA1			(0x1 << 8)
#define DPI1_SEL_IN_RDMA2			(0x3 << 8)
#define DSI0_SEL_IN_RDMA1			0x1
#define DSI0_SEL_IN_RDMA2			0x4
#define DSI1_SEL_IN_RDMA1			0x1
#define DSI1_SEL_IN_RDMA2			0x4
#define DSI2_SEL_IN_RDMA1			(0x1 << 16)
#define DSI2_SEL_IN_RDMA2			(0x4 << 16)
#define DSI3_SEL_IN_RDMA1			(0x1 << 16)
#define DSI3_SEL_IN_RDMA2			(0x4 << 16)
#define COLOR1_SEL_IN_OVL1			0x1

#define OVL_MOUT_EN_RDMA			0x1
#define BLS_TO_DSI_RDMA1_TO_DPI1		0x8
#define BLS_TO_DPI_RDMA1_TO_DSI			0x2
#define DSI_SEL_IN_BLS				0x0
#define DPI_SEL_IN_BLS				0x0
#define DSI_SEL_IN_RDMA				0x1

static void mtk_mmsys_ddp_mout_en(void __iomem *config_regs,
				  enum mtk_ddp_comp_id cur,
				  enum mtk_ddp_comp_id next,
				  bool enable)
{
	unsigned int addr, value, reg;

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_COLOR0) {
		addr = DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
		value = OVL0_MOUT_EN_COLOR0;
	} else if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_RDMA0) {
		addr = DISP_REG_CONFIG_DISP_OVL_MOUT_EN;
		value = OVL_MOUT_EN_RDMA;
	} else if (cur == DDP_COMPONENT_OD0 && next == DDP_COMPONENT_RDMA0) {
		addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD_MOUT_EN_RDMA0;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_DSI0) {
		addr = DISP_REG_CONFIG_DISP_UFOE_MOUT_EN;
		value = UFOE_MOUT_EN_DSI0;
	} else if (cur == DDP_COMPONENT_OVL1 && next == DDP_COMPONENT_COLOR1) {
		addr = DISP_REG_CONFIG_DISP_OVL1_MOUT_EN;
		value = OVL1_MOUT_EN_COLOR1;
	} else if (cur == DDP_COMPONENT_GAMMA && next == DDP_COMPONENT_RDMA1) {
		addr = DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN;
		value = GAMMA_MOUT_EN_RDMA1;
	} else if (cur == DDP_COMPONENT_OD1 && next == DDP_COMPONENT_RDMA1) {
		addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD1_MOUT_EN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI0) {
		addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DPI0;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI1) {
		addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DPI1;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI1) {
		addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DSI1;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI2) {
		addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DSI2;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI3) {
		addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DSI3;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI1) {
		addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DSI1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI2) {
		addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DSI2;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI3) {
		addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DSI3;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DPI0;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI1) {
		addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DPI1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI0) {
		addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DPI0;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI1) {
		addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DPI1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI1) {
		addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DSI1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI2) {
		addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DSI2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI3) {
		addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DSI3;
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

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_COLOR0) {
		addr = DISP_REG_CONFIG_DISP_COLOR0_SEL_IN;
		value = COLOR0_SEL_IN_OVL0;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI0_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI1) {
		addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI1_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI0) {
		addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI0_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI1) {
		addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI1_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI2) {
		addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI2_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI3) {
		addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI3_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI0) {
		addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI0_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI1) {
		addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI1_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI0) {
		addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI0_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI1) {
		addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI1_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI2) {
		addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI2_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI3) {
		addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI3_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_OVL1 && next == DDP_COMPONENT_COLOR1) {
		addr = DISP_REG_CONFIG_DISP_COLOR1_SEL_IN;
		value = COLOR1_SEL_IN_OVL1;
	} else if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DSI0) {
		addr = DISP_REG_CONFIG_DSI_SEL;
		value = DSI_SEL_IN_BLS;
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
	if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DSI0) {
		writel_relaxed(BLS_TO_DSI_RDMA1_TO_DPI1,
			       config_regs + DISP_REG_CONFIG_OUT_SEL);
	} else if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DPI0) {
		writel_relaxed(BLS_TO_DPI_RDMA1_TO_DSI,
			       config_regs + DISP_REG_CONFIG_OUT_SEL);
		writel_relaxed(DSI_SEL_IN_RDMA,
			       config_regs + DISP_REG_CONFIG_DSI_SEL);
		writel_relaxed(DPI_SEL_IN_BLS,
			       config_regs + DISP_REG_CONFIG_DPI_SEL);
	}
}

struct mtk_mmsys_conn_funcs mt2701_mmsys_funcs = {
	.mout_en = mtk_mmsys_ddp_mout_en,
	.sel_in = mtk_mmsys_ddp_sel_in,
	.sout_sel = mtk_mmsys_ddp_sout_sel,
};
