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
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

#define VDEC_HW_ACTIVE	0x10
#define VDEC_IRQ_CFG	0x11
#define VDEC_IRQ_CLR	0x10
#define VDEC_IRQ_CFG_REG	0xa4

typedef irqreturn_t (*vdec_irq_handler)(int irq, void *priv);

static int mtk_vdec_hw_bind(struct device *dev, struct device *master,
	void *data)
{
	struct mtk_vcodec_dev *comp_priv = dev_get_drvdata(dev);
	struct mtk_vcodec_dev *master_priv = data;
	int i;

	for (i = 0; i < MTK_VDEC_HW_MAX; i++) {
		if (dev->of_node != master_priv->component_node[i])
			continue;

		master_priv->hw_dev[i] = comp_priv;
		comp_priv->comp_idx = i;
		break;
	}

	if (i == MTK_VDEC_HW_MAX) {
		dev_err(dev, "Failed to get component node\n");
		return -EINVAL;
	}

	comp_priv->reg_base[VDEC_SYS] = master_priv->reg_base[VDEC_SYS];
	master_priv->reg_base[VDEC_MISC] = comp_priv->reg_base[VDEC_MISC];
	return 0;
}

static void mtk_vdec_hw_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct mtk_vcodec_dev *comp_priv = dev_get_drvdata(dev);

	comp_priv->reg_base[VDEC_SYS] = NULL;
}

static const struct component_ops mtk_vdec_hw_component_ops = {
	.bind = mtk_vdec_hw_bind,
	.unbind = mtk_vdec_hw_unbind,
};

/* Wake up context wait_queue */
static void mtk_vdec_hw_wake_up_ctx(struct mtk_vcodec_ctx *ctx)
{
	ctx->int_cond = 1;
	wake_up_interruptible(&ctx->queue);
}

/* Wake up core context wait_queue */
static void mtk_vdec_hw_wake_up_core_ctx(struct mtk_vcodec_ctx *ctx)
{
	ctx->int_core_cond = 1;
	wake_up_interruptible(&ctx->core_queue);
}

static irqreturn_t mtk_vdec_hw_irq_handler(int irq, void *priv)
{
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	u32 cg_status;
	unsigned int dec_done_status;
	void __iomem *vdec_misc_addr = dev->reg_base[VDEC_MISC] +
					VDEC_IRQ_CFG_REG;

	ctx = mtk_vcodec_get_curr_ctx(dev);

	/* check if HW active or not */
	cg_status = readl(dev->reg_base[VDEC_SYS]);
	if ((cg_status & VDEC_HW_ACTIVE) != 0) {
		mtk_v4l2_err("Failed DEC ISR, VDEC active is not 0x0 (0x%08x)",
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

	mtk_vdec_hw_wake_up_ctx(ctx);

	mtk_v4l2_debug(3, "wake up ctx %d, dec_done_status=%x",	ctx->id,
		dec_done_status);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_vdec_hw_core_irq_handler(int irq, void *priv)
{
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned int dec_done_status;
	void __iomem *vdec_misc_addr = dev->reg_base[VDEC_MISC] +
					VDEC_IRQ_CFG_REG;

	ctx = mtk_vcodec_get_curr_ctx(dev);

	dec_done_status = readl(vdec_misc_addr);
	if ((dec_done_status & MTK_VDEC_IRQ_STATUS_DEC_SUCCESS) !=
		MTK_VDEC_IRQ_STATUS_DEC_SUCCESS)
		return IRQ_HANDLED;

	/* clear interrupt */
	writel((readl(vdec_misc_addr) | VDEC_IRQ_CFG), vdec_misc_addr);
	writel((readl(vdec_misc_addr) & ~VDEC_IRQ_CLR), vdec_misc_addr);

	mtk_vdec_hw_wake_up_core_ctx(ctx);

	mtk_v4l2_debug(3, "wake up core ctx %d, dec_done_status=%x", ctx->id,
		dec_done_status);

	return IRQ_HANDLED;
}

static int mtk_vdec_hw_init_irq(struct mtk_vcodec_dev *dev,
	vdec_irq_handler irq_handler)
{
	struct platform_device *pdev = dev->plat_dev;
	int ret;

	dev->dec_irq = platform_get_irq(pdev, 0);
	if (dev->dec_irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq resource");
		return dev->dec_irq;
	}

	ret = devm_request_irq(&pdev->dev, dev->dec_irq,
				irq_handler, 0, pdev->name, dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to install dev->dec_irq %d (%d)",
			dev->dec_irq, ret);
		return -ENOENT;
	}

	disable_irq(dev->dec_irq);
	return 0;
}

static int mtk_vdec_hw_probe(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev;
	vdec_irq_handler irq_handler;
	int ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->plat_dev = pdev;
	spin_lock_init(&dev->irqlock);
	mutex_init(&dev->dev_mutex);

	ret = mtk_vcodec_init_dec_pm(dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get mt vcodec clock source");
		return ret;
	}

	dev->reg_base[VDEC_MISC] = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR((__force void *)dev->reg_base[VDEC_MISC])) {
		ret = PTR_ERR((__force void *)dev->reg_base[VDEC_MISC]);
		goto err;
	}

	irq_handler = of_device_get_match_data(&pdev->dev);
	if (!irq_handler) {
		dev_err(&pdev->dev, "Failed to get match data.\n");
		goto err;
	}

	ret = mtk_vdec_hw_init_irq(dev, irq_handler);
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
	mtk_vcodec_release_dec_pm(dev);
	return ret;
}

static const struct of_device_id mtk_vdec_hw_ids[] = {
	{
		.compatible = "mediatek,mtk-vcodec-lat",
		.data = mtk_vdec_hw_irq_handler,
	},
	{
		.compatible = "mediatek,mtk-vcodec-core",
		.data = mtk_vdec_hw_core_irq_handler,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vdec_hw_ids);

struct platform_driver mtk_vdec_hw_driver = {
	.probe	= mtk_vdec_hw_probe,
	.driver	= {
		.name	= "mtk-vdec-hw",
		.of_match_table = mtk_vdec_hw_ids,
	},
};
