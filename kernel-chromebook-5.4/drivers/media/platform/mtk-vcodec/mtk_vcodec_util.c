// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*	Tiffany Lin <tiffany.lin@mediatek.com>
*/

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"

/* For encoder, this will enable logs in venc/*/
bool mtk_vcodec_dbg;
EXPORT_SYMBOL(mtk_vcodec_dbg);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under mtk-vcodec/.
 */
int mtk_v4l2_dbg_level;
EXPORT_SYMBOL(mtk_v4l2_dbg_level);

void __iomem *mtk_vcodec_get_reg_addr(struct mtk_vcodec_ctx *data,
					unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VCODEC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->reg_base[reg_idx];
}
EXPORT_SYMBOL(mtk_vcodec_get_reg_addr);

int mtk_vcodec_mem_alloc(struct mtk_vcodec_ctx *data,
			struct mtk_vcodec_mem *mem)
{
	unsigned long size = mem->size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

	mem->va = dma_alloc_coherent(dev, size, &mem->dma_addr, GFP_KERNEL);
	if (!mem->va) {
		mtk_v4l2_err("%s dma_alloc size=%ld failed!", dev_name(dev),
			     size);
		return -ENOMEM;
	}

	mtk_v4l2_debug(3, "[%d]  - va      = %p", ctx->id, mem->va);
	mtk_v4l2_debug(3, "[%d]  - dma     = 0x%lx", ctx->id,
		       (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(3, "[%d]    size = 0x%lx", ctx->id, size);

	return 0;
}
EXPORT_SYMBOL(mtk_vcodec_mem_alloc);

void mtk_vcodec_mem_free(struct mtk_vcodec_ctx *data,
			struct mtk_vcodec_mem *mem)
{
	unsigned long size = mem->size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

	if (!mem->va) {
		mtk_v4l2_err("%s dma_free size=%ld failed!", dev_name(dev),
			     size);
		return;
	}

	mtk_v4l2_debug(3, "[%d]  - va      = %p", ctx->id, mem->va);
	mtk_v4l2_debug(3, "[%d]  - dma     = 0x%lx", ctx->id,
		       (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(3, "[%d]    size = 0x%lx", ctx->id, size);

	dma_free_coherent(dev, size, mem->va, mem->dma_addr);
	mem->va = NULL;
	mem->dma_addr = 0;
	mem->size = 0;
}
EXPORT_SYMBOL(mtk_vcodec_mem_free);

struct mtk_vcodec_dev *mtk_vcodec_get_hw_dev(struct mtk_vcodec_dev *dev, int comp_idx)
{
	struct platform_device *hw_pdev;
	struct device_node *node;
	struct mtk_vcodec_dev *hw_dev;

	if (comp_idx >= MTK_VDEC_HW_MAX || comp_idx < 0) {
		mtk_v4l2_err("Comp idx is out of range:%d", comp_idx);
		return NULL;
	}

	if (dev->hw_dev[comp_idx])
		return dev->hw_dev[comp_idx];

	node = dev->component_node[comp_idx];
	if (!node) {
		mtk_v4l2_err("Get lat node fail:%d", comp_idx);
		return NULL;
	}

	hw_pdev = of_find_device_by_node(node);
	of_node_put(node);

	if (WARN_ON(!hw_pdev)) {
		mtk_v4l2_err("Get hw id(%d) node fail", comp_idx);
		return NULL;
	}

	hw_dev = platform_get_drvdata(hw_pdev);
	if (!hw_dev) {
		mtk_v4l2_err("Get hw id(%d) pdev fail", comp_idx);
		return NULL;
	}

	dev->hw_dev[hw_dev->comp_idx] = hw_dev;
	return hw_dev;
}
EXPORT_SYMBOL(mtk_vcodec_get_hw_dev);

void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *vdec_dev,
	struct mtk_vcodec_ctx *ctx, int comp_idx)
{
	unsigned long flags;
	struct mtk_vcodec_dev *comp_dev;

	spin_lock_irqsave(&vdec_dev->irqlock, flags);
	comp_dev = mtk_vcodec_get_hw_dev(vdec_dev, comp_idx);
	if (!comp_dev) {
		mtk_v4l2_err("Failed to get hw dev");
		spin_unlock_irqrestore(&vdec_dev->irqlock, flags);
		return;
	}
	comp_dev->curr_ctx = ctx;
	spin_unlock_irqrestore(&vdec_dev->irqlock, flags);
}
EXPORT_SYMBOL(mtk_vcodec_set_curr_ctx);

struct mtk_vcodec_ctx *mtk_vcodec_get_curr_ctx(struct mtk_vcodec_dev *vdec_dev)
{
	unsigned long flags;
	struct mtk_vcodec_ctx *ctx;

	spin_lock_irqsave(&vdec_dev->irqlock, flags);
	ctx = vdec_dev->curr_ctx;
	spin_unlock_irqrestore(&vdec_dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL(mtk_vcodec_get_curr_ctx);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek video codec driver");
