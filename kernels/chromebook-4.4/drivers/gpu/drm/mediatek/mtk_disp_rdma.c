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
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <soc/mediatek/cmdq.h>

#include "mtk_drm_ddp_comp.h"

static phys_addr_t addr_va2pa(void __iomem *va)
{
	struct page *pg = vmalloc_to_page(va);

	return (page_to_pfn(pg) << PAGE_SHIFT) + ((unsigned long)va & 0xfff);
}

#define cmdq_write(handle, val, reg) \
		cmdq_rec_write((handle), (val), addr_va2pa(reg))
#define cmdq_write_mask(handle, val, reg, mask) \
		cmdq_rec_write_mask((handle), (val), addr_va2pa(reg), (mask))

#define DISP_REG_RDMA_INT_ENABLE		0x0000
#define DISP_REG_RDMA_INT_STATUS		0x0004
#define RDMA_TARGET_LINE_INT				BIT(5)
#define RDMA_FIFO_UNDERFLOW_INT				BIT(4)
#define RDMA_EOF_ABNORMAL_INT				BIT(3)
#define RDMA_FRAME_END_INT				BIT(2)
#define RDMA_FRAME_START_INT				BIT(1)
#define RDMA_REG_UPDATE_INT				BIT(0)
#define DISP_REG_RDMA_GLOBAL_CON		0x0010
#define RDMA_ENGINE_EN					BIT(0)
#define DISP_REG_RDMA_SIZE_CON_0		0x0014
#define DISP_REG_RDMA_SIZE_CON_1		0x0018
#define DISP_REG_RDMA_TARGET_LINE		0x001c
#define DISP_REG_RDMA_FIFO_CON			0x0040
#define RDMA_FIFO_UNDERFLOW_EN				BIT(31)
#define RDMA_FIFO_PSEUDO_SIZE(bytes)			(((bytes) / 16) << 16)
#define RDMA_OUTPUT_VALID_FIFO_THRESHOLD(bytes) (((bytes) / 16) & 0x3ff)

/**
 * struct mtk_disp_rdma - DISP_RDMA driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_rdma {
	struct mtk_ddp_comp		ddp_comp;
};

static void mtk_rdma_start(struct mtk_ddp_comp *comp, struct cmdq_rec *handle)
{
	cmdq_write_mask(handle, RDMA_ENGINE_EN,
			comp->regs + DISP_REG_RDMA_GLOBAL_CON, RDMA_ENGINE_EN);
}

static void mtk_rdma_stop(struct mtk_ddp_comp *comp, struct cmdq_rec *handle)
{
	cmdq_write_mask(handle, 0,
			comp->regs + DISP_REG_RDMA_GLOBAL_CON, RDMA_ENGINE_EN);
}

static void mtk_rdma_config(struct mtk_ddp_comp *comp, unsigned int width,
			    unsigned int height, unsigned int vrefresh,
			    unsigned int bpc, struct cmdq_rec *handle)
{
	unsigned long long threshold;
	unsigned int reg;

	cmdq_write_mask(handle, width, comp->regs + DISP_REG_RDMA_SIZE_CON_0,
			0x1fff);
	cmdq_write_mask(handle, height, comp->regs + DISP_REG_RDMA_SIZE_CON_1,
			0xfffff);

	/*
	 * Enable FIFO underflow since DSI and DPI can't be blocked.
	 * Keep the FIFO pseudo size reset default of 8 KiB. Set the
	 * output threshold to 6 microseconds with 7/6 overhead to
	 * account for blanking, and with a pixel depth of 4 bytes:
	 */
	threshold = div_u64((unsigned long long)width * height * vrefresh *
			    4 * 7, 1000000);
	reg = RDMA_FIFO_UNDERFLOW_EN |
	      RDMA_FIFO_PSEUDO_SIZE(SZ_8K) |
	      RDMA_OUTPUT_VALID_FIFO_THRESHOLD(threshold);
	cmdq_write(handle, reg, comp->regs + DISP_REG_RDMA_FIFO_CON);
}

static const struct mtk_ddp_comp_funcs mtk_disp_rdma_funcs = {
	.config = mtk_rdma_config,
	.start = mtk_rdma_start,
	.stop = mtk_rdma_stop,
};

static int mtk_disp_rdma_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct mtk_disp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;

}

static void mtk_disp_rdma_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_rdma_component_ops = {
	.bind	= mtk_disp_rdma_bind,
	.unbind = mtk_disp_rdma_unbind,
};

static int mtk_disp_rdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_rdma *priv;
	int comp_id;
	int irq;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_RDMA);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_rdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	ret = component_add(dev, &mtk_disp_rdma_component_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int mtk_disp_rdma_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_rdma_component_ops);

	return 0;
}

static const struct of_device_id mtk_disp_rdma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt8173-disp-rdma", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_rdma_driver_dt_match);

struct platform_driver mtk_disp_rdma_driver = {
	.probe		= mtk_disp_rdma_probe,
	.remove		= mtk_disp_rdma_remove,
	.driver		= {
		.name	= "mediatek-disp-rdma",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_rdma_driver_dt_match,
	},
};
