// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Daoyuan Huang <daoyuan.huang@mediatek.com>
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

#include <linux/compiler_types.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include "mmsys_reg_base.h"
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-debug.h"
#include "mtk-mdp3-regs.h"

struct mdp_module_base_va {
	void __iomem *MDP_RDMA0;
	void __iomem *MDP_RSZ0;
	void __iomem *MDP_RSZ1;
	void __iomem *MDP_TDSHP;
	void __iomem *MDP_COLOR;
	void __iomem *MDP_AAL;
	void __iomem *MDP_CCORR;
	void __iomem *MDP_WROT0;
	void __iomem *MDP_WDMA;
	void __iomem *SMI_LARB0;
};

struct RegDef {
	int offset;
	const char *name;
};

struct mdp_debug_context {
	struct platform_device *mdp_device;
	struct mdp_func_struct mdp_func_pointer;
	struct mdp_module_base_va mdp_mod_base_va;
	void __iomem *mdp_mmsys_base_va;
};

static struct mdp_debug_context g_mdp_debug;

#define MMSYS_CONFIG_BASE	g_mdp_debug.mdp_mmsys_base_va
#define MDP_RDMA0_BASE		g_mdp_debug.mdp_mod_base_va.MDP_RDMA0
#define MDP_RSZ0_BASE		g_mdp_debug.mdp_mod_base_va.MDP_RSZ0
#define MDP_RSZ1_BASE		g_mdp_debug.mdp_mod_base_va.MDP_RSZ1
#define MDP_TDSHP_BASE		g_mdp_debug.mdp_mod_base_va.MDP_TDSHP
#define MDP_COLOR_BASE		g_mdp_debug.mdp_mod_base_va.MDP_COLOR
#define MDP_AAL_BASE		g_mdp_debug.mdp_mod_base_va.MDP_AAL
#define MDP_CCORR_BASE		g_mdp_debug.mdp_mod_base_va.MDP_CCORR
#define MDP_WROT0_BASE		g_mdp_debug.mdp_mod_base_va.MDP_WROT0
#define MDP_WDMA_BASE		g_mdp_debug.mdp_mod_base_va.MDP_WDMA

#define MDP_REG_GET32(addr)	(readl((void *)addr) & 0xffffffff)
#define MDP_REG_SET32(addr, val)	writel(val, addr)

static const char *mdp_get_rsz_state(const uint32_t state);

struct mdp_func_struct *mdp_get_func(void)
{
	return &g_mdp_debug.mdp_func_pointer;
}

static void __iomem *mdp_alloc_reference_VA_by_name(const char *ref_name)
{
	struct device_node *node;
	struct device *dev = &(g_mdp_debug.mdp_device->dev);
	void __iomem *VA;

	node = of_parse_phandle(dev->of_node, ref_name, 0);
	if (!node) {
		mdp_err("DEV: cannot parse node name:%s\n", ref_name);
		return 0;
	}

	VA = of_iomap(node, 0);
	of_node_put(node);
	mdp_dbg(2, "DEV: VA ref(%s): 0x%p\n", ref_name, VA);

	return VA;
}

static void mdp_free_module_base_VA(void __iomem *VA)
{
	iounmap(VA);
}

static void mdp_init_module_base_VA(void)
{
	struct mdp_module_base_va *mod_base_va = &(g_mdp_debug.mdp_mod_base_va);
	struct device_node *rdma_node = g_mdp_debug.mdp_device->dev.of_node;
	void __iomem *va;

	if (rdma_node) {
		va = of_iomap(rdma_node, 0);
		of_node_put(rdma_node);
		mod_base_va->MDP_RDMA0 = va;
		mdp_dbg(2, "MDP_RDMA va: 0x%p\n", va);
	} else
		mdp_err("%s:MDP_RDMA node missing!\n", __func__);

	mod_base_va->MDP_RSZ0 = mdp_alloc_reference_VA_by_name("mdp3-rsz0");
	mod_base_va->MDP_RSZ1 = mdp_alloc_reference_VA_by_name("mdp3-rsz1");
	mod_base_va->MDP_WROT0 = mdp_alloc_reference_VA_by_name("mdp3-wrot0");
	mod_base_va->MDP_WDMA = mdp_alloc_reference_VA_by_name("mdp3-wdma0");
	mod_base_va->MDP_TDSHP = mdp_alloc_reference_VA_by_name("mdp3-tdshp0");
	mod_base_va->MDP_COLOR = mdp_alloc_reference_VA_by_name("mdp3-color0");
	mod_base_va->MDP_AAL = mdp_alloc_reference_VA_by_name("mdp3-aal0");
	mod_base_va->MDP_CCORR = mdp_alloc_reference_VA_by_name("mdp3-ccorr0");
	mod_base_va->SMI_LARB0 =
		mdp_alloc_reference_VA_by_name("mediatek,larb");
}

static void mdp_deinit_module_base_VA(void)
{
	struct mdp_module_base_va *mod_base_va = &(g_mdp_debug.mdp_mod_base_va);

	mdp_free_module_base_VA(mod_base_va->MDP_RDMA0);
	mdp_free_module_base_VA(mod_base_va->MDP_RSZ0);
	mdp_free_module_base_VA(mod_base_va->MDP_RSZ1);
	mdp_free_module_base_VA(mod_base_va->MDP_WROT0);
	mdp_free_module_base_VA(mod_base_va->MDP_WDMA);
	mdp_free_module_base_VA(mod_base_va->MDP_TDSHP);
	mdp_free_module_base_VA(mod_base_va->MDP_COLOR);
	mdp_free_module_base_VA(mod_base_va->MDP_AAL);
	mdp_free_module_base_VA(mod_base_va->MDP_CCORR);
	mdp_free_module_base_VA(mod_base_va->SMI_LARB0);
	memset(mod_base_va, 0, sizeof(struct mdp_module_base_va));
}

static void mdp_map_mmsys_VA(void)
{
	g_mdp_debug.mdp_mmsys_base_va =
		mdp_alloc_reference_VA_by_name("mediatek,mmsys");
}

static void mdp_unmap_mmsys_VA(void)
{
	mdp_free_module_base_VA(g_mdp_debug.mdp_mmsys_base_va);
}

static uint32_t mdp_rdma_get_reg_offset_src_addr_virtual(void)
{
	return 0;
}

static uint32_t mdp_wrot_get_reg_offset_dst_addr_virtual(void)
{
	return 0;
}

static uint32_t mdp_wdma_get_reg_offset_dst_addr_virtual(void)
{
	return 0;
}

/* MDP engine dump */
static void mdp_dump_rsz_common(void __iomem *base, const char *label)
{
	uint32_t value[8];
	uint32_t request[8];
	uint32_t state;

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x004);
	value[1] = MDP_REG_GET32(base + 0x00C);
	value[2] = MDP_REG_GET32(base + 0x010);
	value[3] = MDP_REG_GET32(base + 0x014);
	value[4] = MDP_REG_GET32(base + 0x018);
	MDP_REG_SET32(base + 0x040, 0x00000001);
	value[5] = MDP_REG_GET32(base + 0x044);
	MDP_REG_SET32(base + 0x040, 0x00000002);
	value[6] = MDP_REG_GET32(base + 0x044);
	MDP_REG_SET32(base + 0x040, 0x00000003);
	value[7] = MDP_REG_GET32(base + 0x044);

	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("RSZ_CONTROL: 0x%08x, RSZ_INPUT_IMAGE: 0x%08x\n",
		 value[0], value[1]);
	mdp_err("RSZ_OUTPUT_IMAGE: 0x%08x RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		 value[2], value[3]);
	mdp_err("RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_DEBUG_1: 0x%08x\n",
		 value[4], value[5]);
	mdp_err(", RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		 value[6], value[7]);

	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[6] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	request[4] = (value[1] & 0x1FFF);	/* input_width */
	request[5] = (value[1] >> 16) & 0x1FFF;	/* input_height */
	request[6] = (value[2] & 0x1FFF);	/* output_width */
	request[7] = (value[2] >> 16) & 0x1FFF;	/* output_height */

	mdp_err("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		mdp_get_rsz_state(state));
	mdp_err("RSZ input_width,input_height,output_width,output_height:");
	mdp_err("%d,%d,%d,%d\n",
		 request[4], request[5], request[6], request[7]);
}

static void mdp_dump_tdshp_common(void __iomem *base, const char *label)
{
	uint32_t value[8];

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x114);
	value[1] = MDP_REG_GET32(base + 0x11C);
	value[2] = MDP_REG_GET32(base + 0x104);
	value[3] = MDP_REG_GET32(base + 0x108);
	value[4] = MDP_REG_GET32(base + 0x10C);
	value[5] = MDP_REG_GET32(base + 0x120);
	value[6] = MDP_REG_GET32(base + 0x128);
	value[7] = MDP_REG_GET32(base + 0x110);

	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	mdp_err("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, 0x10C: 0x%08x\n",
		value[2], value[3], value[4]);
	mdp_err("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[7], value[5], value[6]);
}

static void mdp_virtual_function_setting(void)
{
	struct mdp_func_struct *pfunc = mdp_get_func();

	pfunc->mdp_dump_rsz = mdp_dump_rsz_common;
	pfunc->mdp_dump_tdshp = mdp_dump_tdshp_common;
	pfunc->mdp_rdma_get_src_base_addr =
		mdp_rdma_get_reg_offset_src_addr_virtual;
	pfunc->mdp_wrot_get_reg_offset_dst_addr =
		mdp_wrot_get_reg_offset_dst_addr_virtual;
	pfunc->mdp_wdma_get_reg_offset_dst_addr =
		mdp_wdma_get_reg_offset_dst_addr_virtual;
}

static void mdp_dump_mmsys_config(void)
{
	int i;
	uint32_t value;
	static const struct RegDef configRegisters[] = {
		{0xF80, "ISP_MOUT_EN"},
		{0xF84, "MDP_RDMA0_MOUT_EN"},
		{0xF8C, "MDP_PRZ0_MOUT_EN"},
		{0xF90, "MDP_PRZ1_MOUT_EN"},
		{0xF94, "MDP_COLOR_MOUT_EN"},
		{0xF98, "IPU_MOUT_EN"},
		{0xFE8, "MDP_AAL_MOUT_EN"},
		/* {0x02C, "MDP_TDSHP_MOUT_EN"}, */
		{0xF00, "DISP_OVL0_MOUT_EN"},
		{0xF04, "DISP_OVL0_2L_MOUT_EN"},
		{0xF08, "DISP_OVL1_2L_MOUT_EN"},
		{0xF0C, "DISP_DITHER0_MOUT_EN"},
		{0xF10, "DISP_RSZ_MOUT_EN"},
		/* {0x040, "DISP_UFOE_MOUT_EN"}, */
		/* {0x040, "MMSYS_MOUT_RST"}, */
		{0xFA0, "DISP_TO_WROT_SOUT_SEL"},
		{0xFA4, "MDP_COLOR_IN_SOUT_SEL"},
		{0xFA8, "MDP_PATH0_SOUT_SEL"},
		{0xFAC, "MDP_PATH1_SOUT_SEL"},
		{0xFB0, "MDP_TDSHP_SOUT_SEL"},
		{0xFC0, "MDP_PRZ0_SEL_IN"},
		{0xFC4, "MDP_PRZ1_SEL_IN"},
		{0xFC8, "MDP_TDSHP_SEL_IN"},
		{0xFCC, "DISP_WDMA0_SEL_IN"},
		{0xFDC, "MDP_COLOR_SEL_IN"},
		{0xF20, "DISP_COLOR_OUT_SEL_IN"},
		{0xFD0, "MDP_WROT0_SEL_IN"},
		{0xFD4, "MDP_WDMA_SEL_IN"},
		{0xFD8, "MDP_COLOR_OUT_SEL_IN"},
		{0xFDC, "MDP_COLOR_SEL_IN "},
		/* {0xFDC, "DISP_COLOR_SEL_IN"}, */
		{0xFE0, "MDP_PATH0_SEL_IN"},
		{0xFE4, "MDP_PATH1_SEL_IN"},
		{0xFEC, "MDP_AAL_SEL_IN"},
		{0xFF0, "MDP_CCORR_SEL_IN"},
		{0xFF4, "MDP_CCORR_SOUT_SEL"},
		/* {0x070, "DISP_WDMA1_SEL_IN"}, */
		/* {0x074, "DISP_UFOE_SEL_IN"}, */
		{0xF2C, "DSI0_SEL_IN"},
		{0xF30, "DSI1_SEL_IN"},
		{0xF50, "DISP_RDMA0_SOUT_SEL_IN"},
		{0xF54, "DISP_RDMA1_SOUT_SEL_IN"},
		{0x0F0, "MMSYS_MISC"},
		/* ACK and REQ related */
		{0x8B4, "DISP_DL_VALID_0"},
		{0x8B8, "DISP_DL_VALID_1"},
		{0x8C0, "DISP_DL_READY_0"},
		{0x8C4, "DISP_DL_READY_1"},
		{0x8CC, "MDP_DL_VALID_0"},
		{0x8D0, "MDP_DL_VALID_1"},
		{0x8D4, "MDP_DL_READY_0"},
		{0x8D8, "MDP_DL_READY_1"},
		{0x8E8, "MDP_MOUT_MASK"},
		{0x948, "MDP_DL_VALID_2"},
		{0x94C, "MDP_DL_READY_2"},
		{0x950, "DISP_DL_VALID_2"},
		{0x954, "DISP_DL_READY_2"},
		{0x100, "MMSYS_CG_CON0"},
		{0x110, "MMSYS_CG_CON1"},
		/* Async DL related */
		{0x960, "TOP_RELAY_FSM_RD"},
		{0x934, "MDP_ASYNC_CFG_WD"},
		{0x938, "MDP_ASYNC_CFG_RD"},
		{0x958, "MDP_ASYNC_CFG_OUT_RD"},
		{0x95C, "MDP_ASYNC_IPU_CFG_OUT_RD"},
		{0x994, "ISP_RELAY_CFG_WD"},
		{0x998, "ISP_RELAY_CNT_RD"},
		{0x99C, "ISP_RELAY_CNT_LATCH_RD"},
		{0x9A0, "IPU_RELAY_CFG_WD"},
		{0x9A4, "IPU_RELAY_CNT_RD"},
		{0x9A8, "IPU_RELAY_CNT_LATCH_RD"}
	};

	if (!MMSYS_CONFIG_BASE) {
		mdp_err("%s:MMSYS_CONFIG_BASE=0!\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(configRegisters); i++) {
		value = MDP_REG_GET32(MMSYS_CONFIG_BASE +
			configRegisters[i].offset);
		mdp_err("%s: 0x%08x\n", configRegisters[i].name, value);
	}
}

static const char *mdp_get_rdma_state(uint32_t state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "wait sof";
	case 0x4:
		return "reg update";
	case 0x8:
		return "clear0";
	case 0x10:
		return "clear1";
	case 0x20:
		return "int0";
	case 0x40:
		return "int1";
	case 0x80:
		return "data running";
	case 0x100:
		return "wait done";
	case 0x200:
		return "warm reset";
	case 0x400:
		return "wait reset";
	default:
		return "";
	}
}

static const char *mdp_get_rsz_state(const uint32_t state)
{
	switch (state) {
	case 0x5:
		return "downstream hang";	/* 0,1,0,1 */
	case 0xa:
		return "upstream hang";	/* 1,0,1,0 */
	default:
		return "";
	}
}

static const char *mdp_get_wdma_state(uint32_t state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "clear";
	case 0x4:
		return "prepare";
	case 0x8:
		return "prepare";
	case 0x10:
		return "data running";
	case 0x20:
		return "eof wait";
	case 0x40:
		return "soft reset wait";
	case 0x80:
		return "eof done";
	case 0x100:
		return "sof reset done";
	case 0x200:
		return "frame complete";
	default:
		return "";
	}
}

static void mdp_dump_rdma_common(void __iomem *base, const char *label)
{
	uint32_t value[17];
	uint32_t state;
	uint32_t grep;

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x030);
	value[1] = MDP_REG_GET32(base +
		   mdp_get_func()->mdp_rdma_get_src_base_addr());
	value[2] = MDP_REG_GET32(base + 0x060);
	value[3] = MDP_REG_GET32(base + 0x070);
	value[4] = MDP_REG_GET32(base + 0x078);
	value[5] = MDP_REG_GET32(base + 0x080);
	value[6] = MDP_REG_GET32(base + 0x100);
	value[7] = MDP_REG_GET32(base + 0x118);
	value[8] = MDP_REG_GET32(base + 0x130);
	value[9] = MDP_REG_GET32(base + 0x400);
	value[10] = MDP_REG_GET32(base + 0x408);
	value[11] = MDP_REG_GET32(base + 0x410);
	value[12] = MDP_REG_GET32(base + 0x420);
	value[13] = MDP_REG_GET32(base + 0x430);
	value[14] = MDP_REG_GET32(base + 0x440);
	value[15] = MDP_REG_GET32(base + 0x4D0);
	value[16] = MDP_REG_GET32(base + 0x0);

	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err
	    ("RDMA_SRC_CON: 0x%08x, RDMA_SRC_BASE_0: 0x%08x\n",
	     value[0], value[1]);
	mdp_err
	    ("RDMA_MF_BKGD_SIZE_IN_BYTE: 0x%08x RDMA_MF_SRC_SIZE: 0x%08x\n",
	     value[2], value[3]);
	mdp_err("RDMA_MF_CLIP_SIZE: 0x%08x, RDMA_MF_OFFSET_1: 0x%08x\n",
		value[4], value[5]);
	mdp_err("RDMA_SRC_END_0: 0x%08x, RDMA_SRC_OFFSET_0: 0x%08x\n",
		 value[6], value[7]);
	mdp_err("RDMA_SRC_OFFSET_W_0: 0x%08x, RDMA_MON_STA_0: 0x%08x\n",
		 value[8], value[9]);
	mdp_err("RDMA_MON_STA_1: 0x%08x, RDMA_MON_STA_2: 0x%08x\n",
		 value[10], value[11]);
	mdp_err("RDMA_MON_STA_4: 0x%08x, RDMA_MON_STA_6: 0x%08x\n",
		 value[12], value[13]);
	mdp_err("RDMA_MON_STA_8: 0x%08x, RDMA_MON_STA_26: 0x%08x\n",
		 value[14], value[15]);
	mdp_err("RDMA_EN: 0x%08x\n",
		 value[16]);

	/* parse state */
	mdp_err("RDMA ack:%d req:%d\n", (value[9] & (1 << 11)) >> 11,
		 (value[9] & (1 << 10)) >> 10);
	state = (value[10] >> 8) & 0x7FF;
	grep = (value[10] >> 20) & 0x1;
	mdp_err("RDMA state: 0x%x (%s)\n", state, mdp_get_rdma_state(state));
	mdp_err("RDMA horz_cnt: %d vert_cnt:%d\n",
		value[15] & 0xFFF, (value[15] >> 16) & 0xFFF);

	mdp_err("RDMA grep:%d => suggest to ask SMI help:%d\n", grep, grep);
}

static void mdp_dump_rot_common(void __iomem *base, const char *label)
{
	uint32_t value[47];

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x000);
	value[1] = MDP_REG_GET32(base + 0x008);
	value[2] = MDP_REG_GET32(base + 0x00C);
	value[3] = MDP_REG_GET32(base + 0x024);
	value[4] = MDP_REG_GET32(base +
		   mdp_get_func()->mdp_wrot_get_reg_offset_dst_addr());
	value[5] = MDP_REG_GET32(base + 0x02C);
	value[6] = MDP_REG_GET32(base + 0x004);
	value[7] = MDP_REG_GET32(base + 0x030);
	value[8] = MDP_REG_GET32(base + 0x078);
	value[9] = MDP_REG_GET32(base + 0x070);
	MDP_REG_SET32(base + 0x018, 0x00000100);
	value[10] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000200);
	value[11] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000300);
	value[12] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000400);
	value[13] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000500);
	value[14] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000600);
	value[15] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000700);
	value[16] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000800);
	value[17] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000900);
	value[18] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000A00);
	value[19] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000B00);
	value[20] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000C00);
	value[21] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000D00);
	value[22] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000E00);
	value[23] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00000F00);
	value[24] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001000);
	value[25] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001100);
	value[26] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001200);
	value[27] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001300);
	value[28] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001400);
	value[29] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001500);
	value[30] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001600);
	value[31] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001700);
	value[32] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001800);
	value[33] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001900);
	value[34] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001A00);
	value[35] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001B00);
	value[36] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001C00);
	value[37] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001D00);
	value[38] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001E00);
	value[39] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00001F00);
	value[40] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00002000);
	value[41] = MDP_REG_GET32(base + 0x0D0);
	MDP_REG_SET32(base + 0x018, 0x00002100);
	value[42] = MDP_REG_GET32(base + 0x0D0);
	value[43] = MDP_REG_GET32(base + 0x01C);
	value[44] = MDP_REG_GET32(base + 0x07C);
	value[45] = MDP_REG_GET32(base + 0x010);
	value[46] = MDP_REG_GET32(base + 0x014);

	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("ROT_CTRL: 0x%08x, ROT_MAIN_BUF_SIZE: 0x%08x\n",
		 value[0], value[1]);
	mdp_err("ROT_SUB_BUF_SIZE: 0x%08x, ROT_TAR_SIZE: 0x%08x\n",
		 value[2], value[3]);
	mdp_err("ROT_BASE_ADDR: 0x%08x, ROT_OFST_ADDR: 0x%08x\n",
		 value[4], value[5]);
	mdp_err("ROT_DMA_PERF: 0x%08x, ROT_STRIDE: 0x%08x\n",
		 value[6], value[7]);
	mdp_err("ROT_IN_SIZE: 0x%08x, ROT_EOL: 0x%08x\n",
		 value[8], value[9]);
	mdp_err("ROT_DBUGG_1: 0x%08x, ROT_DEBUBG_2: 0x%08x\n",
		 value[10], value[11]);
	mdp_err("ROT_DBUGG_3: 0x%08x, ROT_DBUGG_4: 0x%08x\n",
		 value[12], value[13]);
	mdp_err("ROT_DEBUBG_5: 0x%08x, ROT_DBUGG_6: 0x%08x\n",
		 value[14], value[15]);
	mdp_err("ROT_DBUGG_7: 0x%08x, ROT_DEBUBG_8: 0x%08x\n",
		 value[16], value[17]);
	mdp_err("ROT_DBUGG_9: 0x%08x, ROT_DBUGG_A: 0x%08x\n",
		 value[18], value[19]);
	mdp_err("ROT_DEBUBG_B: 0x%08x, ROT_DBUGG_C: 0x%08x\n",
		 value[20], value[21]);
	mdp_err("ROT_DBUGG_D: 0x%08x, ROT_DEBUBG_E: 0x%08x\n",
		 value[22], value[23]);
	mdp_err("ROT_DBUGG_F: 0x%08x, ROT_DBUGG_10: 0x%08x\n",
		 value[24], value[25]);
	mdp_err("ROT_DEBUBG_11: 0x%08x, ROT_DEBUG_12: 0x%08x\n",
		 value[26], value[27]);
	mdp_err("ROT_DBUGG_13: 0x%08x, ROT_DBUGG_14: 0x%08x\n",
		 value[28], value[29]);
	mdp_err("ROT_DEBUG_15: 0x%08x, ROT_DBUGG_16: 0x%08x\n",
		 value[30], value[31]);
	mdp_err("ROT_DBUGG_17: 0x%08x, ROT_DEBUG_18: 0x%08x\n",
		 value[32], value[33]);
	mdp_err("ROT_DBUGG_19: 0x%08x, ROT_DBUGG_1A: 0x%08x\n",
		 value[34], value[35]);
	mdp_err("ROT_DEBUG_1B: 0x%08x, ROT_DBUGG_1C: 0x%08x\n",
		 value[36], value[37]);
	mdp_err("ROT_DBUGG_1D: 0x%08x, ROT_DEBUG_1E: 0x%08x\n",
		 value[38], value[39]);
	mdp_err("ROT_DBUGG_1F: 0x%08x, ROT_DBUGG_20: 0x%08x\n",
		 value[40], value[41]);
	mdp_err("ROT_DEBUG_21: 0x%08x\n",
		 value[42]);
	mdp_err("VIDO_INT: 0x%08x, VIDO_ROT_EN: 0x%08x\n",
		value[43], value[44]);
	mdp_err("VIDO_SOFT_RST: 0x%08x, VIDO_SOFT_RST_STAT: 0x%08x\n",
		value[45], value[46]);
}

static void mdp_dump_color_common(void __iomem *base, const char *label)
{
	uint32_t value[13];

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x400);
	value[1] = MDP_REG_GET32(base + 0x404);
	value[2] = MDP_REG_GET32(base + 0x408);
	value[3] = MDP_REG_GET32(base + 0x40C);
	value[4] = MDP_REG_GET32(base + 0x410);
	value[5] = MDP_REG_GET32(base + 0x420);
	value[6] = MDP_REG_GET32(base + 0xC00);
	value[7] = MDP_REG_GET32(base + 0xC04);
	value[8] = MDP_REG_GET32(base + 0xC08);
	value[9] = MDP_REG_GET32(base + 0xC0C);
	value[10] = MDP_REG_GET32(base + 0xC10);
	value[11] = MDP_REG_GET32(base + 0xC50);
	value[12] = MDP_REG_GET32(base + 0xC54);

	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("COLOR CFG_MAIN: 0x%08x\n", value[0]);
	mdp_err("COLOR PXL_CNT_MAIN: 0x%08x, LINE_CNT_MAIN: 0x%08x\n",
		value[1], value[2]);
	mdp_err("COLOR WIN_X_MAIN: 0x%08x, WIN_Y_MAIN: 0x%08x\n",
		value[3], value[4]);
	mdp_err("DBG_CFG_MAIN: 0x%08x, COLOR START: 0x%08x\n",
		value[5], value[6]);
	mdp_err("INTEN: 0x%08x, INTSTA: 0x%08x\n",
		value[7], value[8]);
	mdp_err("COLOR OUT_SEL: 0x%08x, FRAME_DONE_DEL: 0x%08x\n",
		value[9], value[10]);
	mdp_err
	    ("COLOR INTERNAL_IP_WIDTH: 0x%08x, INTERNAL_IP_HEIGHT: 0x%08x\n",
	     value[11], value[12]);
}

static void mdp_dump_wdma_common(void __iomem *base, const char *label)
{
	uint32_t value[56];
	uint32_t state;
	/* grep bit = 1, WDMA has sent request to SMI,
	 *and not receive done yet
	 */
	uint32_t grep;
	uint32_t isFIFOFull;	/* 1 for WDMA FIFO full */
	int i;

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x014);
	value[1] = MDP_REG_GET32(base + 0x018);
	value[2] = MDP_REG_GET32(base + 0x028);
	value[3] = MDP_REG_GET32(base +
		   mdp_get_func()->mdp_wdma_get_reg_offset_dst_addr());
	value[4] = MDP_REG_GET32(base + 0x078);
	value[5] = MDP_REG_GET32(base + 0x080);
	value[6] = MDP_REG_GET32(base + 0x0A0);
	value[7] = MDP_REG_GET32(base + 0x0A8);

	for (i = 0; i < 16; i++) {
		MDP_REG_SET32(base + 0x014, (0x10000000 * i) |
			      (value[0] & (0x0FFFFFFF)));
		value[8 + (3 * i)] = MDP_REG_GET32(base + 0x014);
		value[9 + (3 * i)] = MDP_REG_GET32(base + 0x0AC);
		value[10 + (3 * i)] = MDP_REG_GET32(base + 0x0B8);
	}

	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("[MDP]WDMA_CFG: 0x%08x, WDMA_SRC_SIZE: 0x%08x\n",
		 value[0], value[1]);
	mdp_err("WDMA_DST_W_IN_BYTE = 0x%08x, [MDP]WDMA_DST_ADDR0: 0x%08x\n",
		 value[2], value[3]);
	mdp_err
	    ("WDMA_DST_UV_PITCH: 0x%08x, WDMA_DST_ADDR_OFFSET0 = 0x%08x\n",
	     value[4], value[5]);
	mdp_err("[MDP]WDMA_STATUS: 0x%08x, WDMA_INPUT_CNT: 0x%08x\n",
		value[6], value[7]);

	/* Dump Addtional WDMA debug info */
	for (i = 0; i < 16; i++) {
		mdp_err("WDMA_DEBUG_%x 014:0x%08x, 0ac:0x%08x, 0b8:0x%08x\n",
			i, value[8 + (3 * i)], value[9 + (3 * i)],
			value[10 + (3 * i)]);
	}

	/* parse WDMA state */
	state = value[6] & 0x3FF;
	grep = (value[6] >> 13) & 0x1;
	isFIFOFull = (value[6] >> 12) & 0x1;

	mdp_err("WDMA state:0x%x (%s)\n", state, mdp_get_wdma_state(state));
	mdp_err("WDMA in_req:%d in_ack:%d\n", (value[6] >> 15) & 0x1,
		(value[6] >> 14) & 0x1);

	/* note WDMA send request(i.e command) to SMI first,
	 * then SMI takes request data from WDMA FIFO
	 */
	/* if SMI dose not process request and upstream HWs */
	/* such as MDP_RSZ send data to WDMA, WDMA FIFO will full finally */
	mdp_err("WDMA grep:%d, FIFO full:%d\n", grep, isFIFOFull);
	mdp_err("WDMA suggest: Need SMI help:%d, Need check WDMA config:%d\n",
		(grep), ((grep == 0) && (isFIFOFull == 1)));
}

static void mdp_dump_rsz(void __iomem *base, const char *label)
{
	uint32_t value[11];
	uint32_t request[4];
	uint32_t state;

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x004);
	value[1] = MDP_REG_GET32(base + 0x008);
	value[2] = MDP_REG_GET32(base + 0x010);
	value[3] = MDP_REG_GET32(base + 0x014);
	value[4] = MDP_REG_GET32(base + 0x018);
	value[5] = MDP_REG_GET32(base + 0x01C);
	MDP_REG_SET32(base + 0x044, 0x00000001);
	value[6] = MDP_REG_GET32(base + 0x048);
	MDP_REG_SET32(base + 0x044, 0x00000002);
	value[7] = MDP_REG_GET32(base + 0x048);
	MDP_REG_SET32(base + 0x044, 0x00000003);
	value[8] = MDP_REG_GET32(base + 0x048);
	value[9] = MDP_REG_GET32(base + 0x100);
	value[10] = MDP_REG_GET32(base + 0x200);
	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("RSZ_CONTROL_1: 0x%08x, RSZ_CONTROL_2: 0x%08x\n",
		 value[0], value[1]);
	mdp_err("RSZ_INPUT_IMAGE: 0x%08x, RSZ_OUTPUT_IMAGE: 0x%08x\n",
		 value[2], value[3]);
	mdp_err("RSZ_HORIZONTAL_COEFF_STEP: 0x%08x\n", value[4]);
	mdp_err("RSZ_VERTICAL_COEFF_STEP: 0x%08x\n", value[5]);
	mdp_err
	    ("RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
	     value[6], value[7], value[8]);
	mdp_err("PAT1_GEN_SET: 0x%08x, PAT2_GEN_SET: 0x%08x\n",
		value[9], value[10]);
	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[7] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	mdp_err("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		mdp_get_rsz_state(state));
}

static void mdp_dump_tdshp(void __iomem *base, const char *label)
{
	uint32_t value[10];

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x114);
	value[1] = MDP_REG_GET32(base + 0x11C);
	value[2] = MDP_REG_GET32(base + 0x104);
	value[3] = MDP_REG_GET32(base + 0x108);
	value[4] = MDP_REG_GET32(base + 0x10C);
	value[5] = MDP_REG_GET32(base + 0x110);
	value[6] = MDP_REG_GET32(base + 0x120);
	value[7] = MDP_REG_GET32(base + 0x124);
	value[8] = MDP_REG_GET32(base + 0x128);
	value[9] = MDP_REG_GET32(base + 0x12C);
	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	mdp_err("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, STATUS: 0x%08x\n",
		value[2], value[3], value[4]);
	mdp_err("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[5], value[6], value[8]);
	mdp_err("TDSHP OUTPUT_OFFSET: 0x%08x, BLANK_WIDTH: 0x%08x\n",
		value[7], value[9]);
}

static void mdp_dump_aal(void __iomem *base, const char *label)
{
	uint32_t value[9];

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x00C);    /* MDP_AAL_INTSTA       */
	value[1] = MDP_REG_GET32(base + 0x010);    /* MDP_AAL_STATUS       */
	value[2] = MDP_REG_GET32(base + 0x024);    /* MDP_AAL_INPUT_COUNT  */
	value[3] = MDP_REG_GET32(base + 0x028);    /* MDP_AAL_OUTPUT_COUNT */
	value[4] = MDP_REG_GET32(base + 0x030);    /* MDP_AAL_SIZE         */
	value[5] = MDP_REG_GET32(base + 0x034);    /* MDP_AAL_OUTPUT_SIZE  */
	value[6] = MDP_REG_GET32(base + 0x038);    /* MDP_AAL_OUTPUT_OFFSET*/
	value[7] = MDP_REG_GET32(base + 0x4EC);    /* MDP_AAL_TILE_00      */
	value[8] = MDP_REG_GET32(base + 0x4F0);    /* MDP_AAL_TILE_01      */
	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("AAL_INTSTA: 0x%08x, AAL_STATUS: 0x%08x\n",
		value[0], value[1]);
	mdp_err("AAL_INPUT_COUNT: 0x%08x, AAL_OUTPUT_COUNT: 0x%08x\n",
		value[2], value[3]);
	mdp_err("AAL_SIZE: 0x%08x\n", value[4]);
	mdp_err("AAL_OUTPUT_SIZE: 0x%08x, AAL_OUTPUT_OFFSET: 0x%08x\n",
		value[5], value[6]);
	mdp_err("AAL_TILE_00: 0x%08x, AAL_TILE_01: 0x%08x\n",
		value[7], value[8]);
}

static void mdp_dump_ccorr(void __iomem *base, const char *label)
{
	uint32_t value[5];

	if (!base) {
		mdp_err("=============== [MDP] %s Status ===============\n",
			label);
		mdp_err("%s:base=0!\n", __func__);
		return;
	}

	value[0] = MDP_REG_GET32(base + 0x00C);/* MDP_CCORR_INTSTA         */
	value[1] = MDP_REG_GET32(base + 0x010);/* MDP_CCORR_STATUS         */
	value[2] = MDP_REG_GET32(base + 0x024);/* MDP_CCORR_INPUT_COUNT    */
	value[3] = MDP_REG_GET32(base + 0x028);/* MDP_CCORR_OUTPUT_COUNT   */
	value[4] = MDP_REG_GET32(base + 0x030);/* MDP_CCORR_SIZE       */
	mdp_err("=============== [MDP] %s Status ===============\n",
		label);
	mdp_err("CCORR_INTSTA: 0x%08x, CCORR_STATUS: 0x%08x\n",
		value[0], value[1]);
	mdp_err("CCORR_INPUT_COUNT: 0x%08x, CCORR_OUTPUT_COUNT: 0x%08x\n",
		value[2], value[3]);
	mdp_err("CCORR_SIZE: 0x%08x\n",
		value[4]);
}

static uint32_t mdp_rdma_get_reg_offset_src_addr(void)
{
	return 0xF00;
}

static uint32_t mdp_wrot_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

static uint32_t mdp_wdma_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

static void mdp_platform_function_setting(void)
{
	struct mdp_func_struct *pFunc = mdp_get_func();

	pFunc->mdp_dump_mmsys_config = mdp_dump_mmsys_config;
	pFunc->mdp_dump_rsz = mdp_dump_rsz;
	pFunc->mdp_dump_tdshp = mdp_dump_tdshp;
	pFunc->mdp_rdma_get_src_base_addr = mdp_rdma_get_reg_offset_src_addr;
	pFunc->mdp_wrot_get_reg_offset_dst_addr =
		mdp_wrot_get_reg_offset_dst_addr;
	pFunc->mdp_wdma_get_reg_offset_dst_addr =
		mdp_wdma_get_reg_offset_dst_addr;
}

int32_t mdp_dump_info(uint64_t comp_flag, int log_level)
{
	if (comp_flag & (1LL << MDP_COMP_RDMA0))
		mdp_dump_rdma_common(MDP_RDMA0_BASE, "RDMA0");
	if (comp_flag & (1LL << MDP_COMP_AAL0))
		mdp_dump_aal(MDP_AAL_BASE, "AAL0");
	if (comp_flag & (1LL << MDP_COMP_CCORR0))
		mdp_dump_ccorr(MDP_CCORR_BASE, "CCORR0");
	if (comp_flag & (1LL << MDP_COMP_RSZ0))
		mdp_get_func()->mdp_dump_rsz(MDP_RSZ0_BASE, "RSZ0");
	if (comp_flag & (1LL << MDP_COMP_RSZ1))
		mdp_get_func()->mdp_dump_rsz(MDP_RSZ1_BASE, "RSZ1");
	if (comp_flag & (1LL << MDP_COMP_TDSHP0))
		mdp_get_func()->mdp_dump_tdshp(MDP_TDSHP_BASE, "TDSHP");
	if (comp_flag & (1LL << MDP_COMP_COLOR0))
		mdp_dump_color_common(MDP_COLOR_BASE, "COLOR0");
	if (comp_flag & (1LL << MDP_COMP_WROT0))
		mdp_dump_rot_common(MDP_WROT0_BASE, "WROT0");
	if (comp_flag & (1LL << MDP_COMP_WDMA))
		mdp_dump_wdma_common(MDP_WDMA_BASE, "WDMA");

	return 0;
}

void mdp_debug_init(struct platform_device *pDevice)
{
	pr_err("%s:start\n", __func__);
	g_mdp_debug.mdp_device = pDevice;

	mdp_init_module_base_VA();
	mdp_map_mmsys_VA();
	mdp_virtual_function_setting();
	mdp_platform_function_setting();

	pr_err("%s:end\n", __func__);
}

void mdp_debug_deinit(void)
{
	mdp_deinit_module_base_VA();
	mdp_unmap_mmsys_VA();
}

