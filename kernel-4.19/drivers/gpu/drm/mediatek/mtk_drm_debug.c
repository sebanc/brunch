/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <video/videomode.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debug.h"

#define MTK_DRM_DBG_CHK_PTR(ptr, msg)			\
	if (unlikely(!ptr)) {				\
		mtk_drm_err("invalid %s %d\n", msg, __LINE__);	\
		return;					\
	}

#define INIT_TABLE(_name, _dump_reg, _dump_size, _pipe) {	\
	.name = _name,				\
	.dump_reg = _dump_reg,			\
	.dump_size = _dump_size,		\
	.pipe = _pipe,				\
}

#define INIT_REG(_offset, _name) {	\
	.offset = _offset,		\
	.name = _name,			\
}

struct mtk_drm_dbg_reg_def {
	u32 offset;
	const char *name;
};

struct mtk_drm_dbg_table {
	const char *name;
	void __iomem *reg_va;
	struct mtk_drm_dbg_reg_def *dump_reg;
	u32 dump_size;
	u32 reg_pa;
	u32 pipe;
};

struct mtk_drm_dbg_data {
	struct mtk_drm_dbg_table *table;
	struct platform_device *pdev;
	u32 size;
};

static struct mtk_drm_dbg_reg_def mt8173_mmsys_reg[] = {
	INIT_REG(0x40, "DISP_OVL0_MOUT_EN"),
	INIT_REG(0x44, "DISP_OVL1_MOUT_EN"),
	INIT_REG(0x48, "DISP_OD_MOUT_EN"),
	INIT_REG(0x4C, "DISP_GAMMA_MOUT_EN"),
	INIT_REG(0x50, "DISP_UFOE_MOUT_EN"),

	INIT_REG(0x84, "DISP_COLOR0_SEL_IN"),
	INIT_REG(0x88, "DISP_COLOR1_SEL_IN"),
	INIT_REG(0x8C, "DISP_AAL_SEL_IN"),
	INIT_REG(0x90, "DISP_PATH0_SEL_IN"),
	INIT_REG(0x94, "DISP_PATH1_SEL_IN"),

	INIT_REG(0xA0, "DISP_UFOE_SEL_IN"),
	INIT_REG(0xA4, "DISP_DSI0_SEL_IN"),
	INIT_REG(0xA8, "DISP_DSI1_SEL_IN"),
	INIT_REG(0xAC, "DISP_DPI_SEL_IN"),

	INIT_REG(0xB4, "DISP_RDMA0_SOUT_SEL_IN"),
	INIT_REG(0xB8, "DISP_RDMA1_SOUT_SEL_IN"),
	INIT_REG(0xBC, "DISP_RDMA2_SOUT_SEL_IN"),
	INIT_REG(0xC0, "DISP_COLOR0_SOUT_SEL_IN"),
	INIT_REG(0xC4, "DISP_COLOR1_SOUT_SEL_IN"),
	INIT_REG(0xC8, "DISP_PATH0_SOUT_SEL_IN"),
	INIT_REG(0xCC, "DISP_PATH1_SOUT_SEL_IN"),

	INIT_REG(0x100, "MMSYS_CG_CON0"),
	INIT_REG(0x110, "MMSYS_CG_CON0"),

	INIT_REG(0x8B0, "DISP_DL_VALID_0"),
	INIT_REG(0x8B4, "DISP_DL_VALID_0"),
	INIT_REG(0x8B8, "DISP_DL_READY_0"),
	INIT_REG(0x8B8, "DISP_DL_READY_1"),
};

static struct mtk_drm_dbg_reg_def mt8173_mutex_reg[] = {
	INIT_REG(0x000, "DISP_MUTEX_INT_EN"),
	INIT_REG(0x004, "DISP_MUTEX_INT_STA"),

	INIT_REG(0x020, "DISP_MUTEX0_EN"),
	INIT_REG(0x028, "DISP_MUTEX0_RST"),
	INIT_REG(0x02C, "DISP_MUTEX0_MOD"),
	INIT_REG(0x030, "DISP_MUTEX0_SOF"),

	INIT_REG(0x040, "DISP_MUTEX1_EN"),
	INIT_REG(0x048, "DISP_MUTEX1_RST"),
	INIT_REG(0x04C, "DISP_MUTEX1_MOD"),
	INIT_REG(0x050, "DISP_MUTEX1_SOF"),

	INIT_REG(0x060, "DISP_MUTEX2_EN"),
	INIT_REG(0x068, "DISP_MUTEX2_RST"),
	INIT_REG(0x06C, "DISP_MUTEX2_MOD"),
	INIT_REG(0x070, "DISP_MUTEX2_SOF"),
};

struct mtk_drm_dbg_table mt8173_dbg_table[] = {
	INIT_TABLE("mmsys", mt8173_mmsys_reg, ARRAY_SIZE(mt8173_mmsys_reg), \
		   (BIT(0) | BIT(1))),
	INIT_TABLE("disp_ovl0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_ovl1", NULL, 0, BIT(1)),
	INIT_TABLE("disp_rdma0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_rdma1", NULL, 0, BIT(1)),
	INIT_TABLE("disp_color0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_color1", NULL, 0, BIT(1)),
	INIT_TABLE("disp_aal", NULL, 0, BIT(0)),
	INIT_TABLE("disp_gamma", NULL, 0, BIT(1)),
	INIT_TABLE("disp_ufoe", NULL, 0, BIT(0)),
	INIT_TABLE("disp_dsi0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_dpi0", NULL, 0, BIT(1)),
	INIT_TABLE("disp_od", NULL, 0, BIT(0)),
	INIT_TABLE("disp_mutex", mt8173_mutex_reg, \
		   ARRAY_SIZE(mt8173_mutex_reg), (BIT(0) | BIT(1))),
};

struct mtk_drm_dbg_table mt8183_dbg_table[] = {
	INIT_TABLE("mmsys", NULL, 0, (BIT(0) | BIT(1))),
	INIT_TABLE("disp_ovl0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_ovl0_2l", NULL, 0, BIT(0)),
	INIT_TABLE("disp_ovl1_2l", NULL, 0, BIT(1)),
	INIT_TABLE("disp_rdma0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_rdma1", NULL, 0, BIT(1)),
	INIT_TABLE("disp_color0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_ccorr0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_aal0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_gamma0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_dither0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_dsi0", NULL, 0, BIT(0)),
	INIT_TABLE("disp_dpi0", NULL, 0, BIT(1)),
	INIT_TABLE("disp_mutex", NULL, 0, (BIT(0) | BIT(1))),
};

struct mtk_drm_dbg_data mt8173_dbg_data = {
	.table = mt8173_dbg_table,
	.size = ARRAY_SIZE(mt8173_dbg_table),
};

struct mtk_drm_dbg_data mt8183_dbg_data = {
	.table = mt8183_dbg_table,
	.size = ARRAY_SIZE(mt8183_dbg_table),
};

static const struct of_device_id mtk_drm_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt8173-mmsys",
	  .data = &mt8173_dbg_data},
	{ .compatible = "mediatek,mt8183-display",
	  .data = &mt8183_dbg_data},
	{ }
};

struct mtk_drm_dbg_data *dbg_data;

static void mtk_drm_dbg_dump_info(void)
{
	struct mtk_drm_dbg_data *data = dbg_data;
	struct mtk_drm_dbg_table *table;
	u32 i;

	MTK_DRM_DBG_CHK_PTR(data, "data");

	table = data->table;

	for (i = 0; i < data->size; i++) {
		if (table[i].reg_va != NULL)
			mtk_drm_dbg("i %2d %16s reg 0x%X\n",
				i,
				table[i].name,
				table[i].reg_pa);
	}
}

static void mtk_drm_dbg_get_comp_reg_base(struct device_node *node,
					  struct mtk_drm_dbg_table *comp)
{
	struct resource res;

	MTK_DRM_DBG_CHK_PTR(node, "node");

	MTK_DRM_DBG_CHK_PTR(comp, "comp");

	comp->reg_va = of_iomap(node, 0);
	of_node_put(node);

	if (of_address_to_resource(node, 0, &res) != 0) {
		pr_err("Missing reg in %s node\n", node->full_name);
		return;
	}
	comp->reg_pa = res.start;

}

static void mtk_drm_dbg_alloc_comp_reg_base(struct device *dev,
					    struct mtk_drm_dbg_table *comp)
{
	struct device_node *node;

	MTK_DRM_DBG_CHK_PTR(dev, "dev");

	MTK_DRM_DBG_CHK_PTR(comp, "comp");

	node = of_parse_phandle(dev->of_node, comp->name, 0);
	if (!node) {
		pr_err("DEV: cannot parse node name:%s\n", comp->name);
		return;
	}

	mtk_drm_dbg_get_comp_reg_base(node, comp);

	mtk_drm_dbg("node %16s pa 0x%x\n",
		    node->full_name, comp->reg_pa);

}

static void mtk_drm_dbg_init_comp_table(struct platform_device *pdev)
{
	struct mtk_drm_dbg_data *data = dbg_data;
	struct mtk_drm_dbg_table *table;
	struct mtk_drm_dbg_table *comp;
	u32 i;

	MTK_DRM_DBG_CHK_PTR(data, "data");

	table = data->table;
	MTK_DRM_DBG_CHK_PTR(table, "table");

	/* mmsys is 1st comp, it was already init done, skip it*/
	for (i = 1; i < data->size; i++) {
		comp = &table[i];

		mtk_drm_dbg_alloc_comp_reg_base(&pdev->dev, comp);
	}
}

static void mtk_drm_dbg_init_table(struct platform_device *pdev)
{
	struct mtk_drm_dbg_data *data = dbg_data;
	struct mtk_drm_dbg_table *table;
	struct mtk_drm_dbg_table *comp;
	struct device_node *node = pdev->dev.of_node;

	MTK_DRM_DBG_CHK_PTR(data, "data");

	table = data->table;
	MTK_DRM_DBG_CHK_PTR(table, "table");

	comp = &table[0];

	mtk_drm_dbg_get_comp_reg_base(node, comp);

	mtk_drm_dbg("init %s reg 0x%x\n",
		    comp->name,
		    comp->reg_pa);

	mtk_drm_dbg_init_comp_table(pdev);
}

static void mtk_drm_dbg_dump_comp_reg(struct mtk_drm_dbg_table *comp,
				      u32 len,
				      u32 offset)
{
	u32 i;
	u32 reg_pa;
	void __iomem *reg_va;

	MTK_DRM_DBG_CHK_PTR(comp, "comp");

	reg_va = comp->reg_va;
	reg_pa = comp->reg_pa;

	mtk_drm_dbg("dump comp %s len 0x%X offset 0x%X\n",
		    comp->name, len, offset);

	for (i = 0; i < len; i += 0x10) {
		mtk_drm_dbg("0x%08X | 0x%08X 0x%08X 0x%08X 0x%08X\n",
			    reg_pa + i + offset,
			    readl(reg_va + offset + i),
			    readl(reg_va + offset + i + 0x4),
			    readl(reg_va + offset + i + 0x8),
			    readl(reg_va + offset + i + 0xc));
	}
}

static void mtk_drm_dbg_dump_comp(struct mtk_drm_dbg_table *comp)
{
	u32 i;
	u32 value;
	void __iomem *base = comp->reg_va;

	mtk_drm_dbg("=== %s Status size %d ===\n", comp->name, comp->dump_size);

	for (i = 0; i < comp->dump_size; i++) {
		value = readl(base + comp->dump_reg[i].offset);
		mtk_drm_dbg("%s: 0x%08x\n", comp->dump_reg[i].name, value);
	}
}

void mtk_drm_dbg_dump_path(u32 pipe)
{
	struct mtk_drm_dbg_data *data = dbg_data;
	struct mtk_drm_dbg_table *table;
	struct mtk_drm_dbg_table *comp;
	u32 i;

	MTK_DRM_DBG_CHK_PTR(data, "data");

	table = data->table;
	MTK_DRM_DBG_CHK_PTR(table, "table");

	mtk_drm_dbg("pipe %d %d\n", pipe, data->size);

	mtk_drm_dbg_dump_info();

	mtk_drm_dbg_dump_comp_reg(&table[0], 0x040, 0x100);
	mtk_drm_dbg_dump_comp_reg(&table[0], 0x040, 0x100);
	mtk_drm_dbg_dump_comp_reg(&table[0], 0x040, 0x800);
	mtk_drm_dbg_dump_comp_reg(&table[0], 0x040, 0x800);

	for (i = 0; i < data->size; i++) {
		comp = &table[i];
		if (!(comp->pipe & (1 << pipe)))
			continue;

		mtk_drm_dbg_dump_comp_reg(comp, 0x100, 0x000);
		mtk_drm_dbg_dump_comp_reg(comp, 0x100, 0x000);
		mtk_drm_dbg_dump_comp(comp);
	}

	mtk_drm_dbg("done!\n");
}

void mtk_drm_dbg_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;

	mtk_drm_dbg("%s node %s\n", dev_name(dev),
		    dev->of_node ? dev->of_node->name : "");

	if (dev->of_node && dev->of_node->parent)
		mtk_drm_dbg("parent node %s\n", dev->of_node->parent->full_name);

	of_id = of_match_node(mtk_drm_dbg_of_ids, dev->of_node);
	if (!of_id) {
		mtk_drm_err("can not find node\n");
		return;
	}

	dbg_data = (struct mtk_drm_dbg_data *)of_id->data;

	mtk_drm_dbg_init_table(pdev);

	mtk_drm_dbg_dump_info();
}
