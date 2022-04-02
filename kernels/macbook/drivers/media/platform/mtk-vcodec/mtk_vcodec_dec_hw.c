// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_dec_hw.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

static int mtk_vdec_comp_bind(struct device *dev, struct device *master,
	void *data)
{
	struct mtk_vdec_comp_dev *comp_dev = dev_get_drvdata(dev);
	struct mtk_vcodec_dev *master_dev = data;
	int i;

	for (i = 0; i < MTK_VDEC_HW_MAX; i++) {
		if (dev->of_node != master_dev->component_node[i])
			continue;

		master_dev->comp_dev[i] = comp_dev;
		comp_dev->comp_idx = i;
		comp_dev->master_dev = master_dev;
		break;
	}

	if (i == MTK_VDEC_HW_MAX) {
		dev_err(dev, "Failed to get component node\n");
		return -EINVAL;
	}

	comp_dev->reg_base[VDEC_COMP_SYS] =
		master_dev->reg_base[VDEC_COMP_SYS];
	set_bit(comp_dev->comp_idx, master_dev->hardware_bitmap);

	return 0;
}

static void mtk_vdec_comp_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct mtk_vdec_comp_dev *comp_dev = dev_get_drvdata(dev);

	comp_dev->reg_base[VDEC_COMP_SYS] = NULL;
}

static const struct component_ops mtk_vdec_hw_component_ops = {
	.bind = mtk_vdec_comp_bind,
	.unbind = mtk_vdec_comp_unbind,
};

/* Wake up core context wait_queue */
static void mtk_vdec_comp_wake_up_ctx(struct mtk_vcodec_ctx *ctx,
	unsigned int hw_id)
{
	ctx->int_core_cond[hw_id] = 1;
	wake_up_interruptible(&ctx->core_queue[hw_id]);
}

static irqreturn_t mtk_vdec_comp_irq_handler(int irq, void *priv)
{
	struct mtk_vdec_comp_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	u32 cg_status;
	unsigned int dec_done_status;
	void __iomem *vdec_misc_addr = dev->reg_base[VDEC_COMP_MISC] +
					VDEC_IRQ_CFG_REG;

	ctx = mtk_vcodec_get_curr_ctx(dev->master_dev, dev->comp_idx);

	/* check if HW active or not */
	cg_status = readl(dev->reg_base[VDEC_COMP_SYS]);
	if ((cg_status & VDEC_HW_ACTIVE) != 0) {
		mtk_v4l2_err("vdec active is not 0x0 (0x%08x)",
			cg_status);
		return IRQ_HANDLED;
	}

	dec_done_status = readl(vdec_misc_addr);
	if ((dec_done_status & MTK_VDEC_IRQ_STATUS_DEC_SUCCESS) !=
		MTK_VDEC_IRQ_STATUS_DEC_SUCCESS)
		return IRQ_HANDLED;

	/* clear interrupt */
	writel((readl(vdec_misc_addr) | VDEC_IRQ_CFG), vdec_misc_addr);
	writel((readl(vdec_misc_addr) & ~VDEC_IRQ_CLR), vdec_misc_addr);

	mtk_vdec_comp_wake_up_ctx(ctx, dev->comp_idx);

	mtk_v4l2_debug(3, "wake up ctx %d, dec_done_status=%x",
		ctx->id, dec_done_status);

	return IRQ_HANDLED;
}

static int mtk_vdec_comp_init_irq(struct mtk_vdec_comp_dev *dev)
{
	struct platform_device *pdev = dev->plat_dev;
	int ret;

	dev->dec_irq = platform_get_irq_optional(pdev, 0);
	if (dev->dec_irq < 0) {
		dev_dbg(&pdev->dev, "Failed to get irq resource");
		return 0;
	}

	ret = devm_request_irq(&pdev->dev, dev->dec_irq,
				mtk_vdec_comp_irq_handler, 0, pdev->name, dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to install dev->dec_irq %d (%d)",
			dev->dec_irq, ret);
		return -ENOENT;
	}

	disable_irq(dev->dec_irq);
	return 0;
}

static int mtk_vdec_comp_probe(struct platform_device *pdev)
{
	struct mtk_vdec_comp_dev *dev;
	int ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->plat_dev = pdev;
	spin_lock_init(&dev->irqlock);

	ret = mtk_vcodec_init_dec_pm(dev->plat_dev, &dev->pm);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get mt vcodec clock source");
		return ret;
	}

	dev->reg_base[VDEC_COMP_MISC] =
		devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR((__force void *)dev->reg_base[VDEC_COMP_MISC])) {
		ret = PTR_ERR((__force void *)dev->reg_base[VDEC_COMP_MISC]);
		goto err;
	}

	if (of_get_property(pdev->dev.of_node, "dma-ranges", NULL))
		dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	ret = mtk_vdec_comp_init_irq(dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register irq handler.\n");
		goto err;
	}

	platform_set_drvdata(pdev, dev);

	ret = component_add(&pdev->dev, &mtk_vdec_hw_component_ops);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add component: %d\n", ret);
		goto err;
	}

	return 0;
err:
	mtk_vcodec_release_dec_pm(&dev->pm);
	return ret;
}

static const struct of_device_id mtk_vdec_comp_ids[] = {
	{
		.compatible = "mediatek,mtk-vcodec-lat",
	},
	{
		.compatible = "mediatek,mtk-vcodec-core",
	},
	{
		.compatible = "mediatek,mtk-vcodec-lat-soc",
	},
	{
		.compatible = "mediatek,mtk-vcodec-core1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vdec_comp_ids);

struct platform_driver mtk_vdec_comp_driver = {
	.probe	= mtk_vdec_comp_probe,
	.driver	= {
		.name	= "mtk-vdec-comp",
		.of_match_table = mtk_vdec_comp_ids,
	},
};
