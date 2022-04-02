/*
 * SPDX-License-Identifier:
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"

#define DISP_GAMMA_EN				0x0000
#define GAMMA_EN					BIT(0)
#define DISP_GAMMA_CFG				0x0020
#define GAMMA_LUT_EN					BIT(1)
#define DISP_GAMMA_SIZE				0x0030
#define DISP_GAMMA_LUT				0x0700

#define LUT_10BIT_MASK				0x03ff

struct mtk_disp_gamma_data {
	bool has_dither;
};

/**
 * struct mtk_disp_gamma - DISP_GAMMA driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_gamma {
	struct mtk_ddp_comp			ddp_comp;
	const struct mtk_disp_gamma_data	*data;
};

static inline struct mtk_disp_gamma *comp_to_gamma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_gamma, ddp_comp);
}

void mtk_gamma_set(struct mtk_ddp_comp *comp, struct drm_crtc_state *state,
		   struct cmdq_pkt *cmdq_pkt)
{
	unsigned int i, reg;
	struct drm_color_lut *lut;
	u32 lut_base;
	u32 word;

	if (state->gamma_lut) {
		reg = readl(comp->regs + DISP_GAMMA_CFG);
		reg = reg | GAMMA_LUT_EN;
		mtk_ddp_write(cmdq_pkt, reg, comp, DISP_GAMMA_CFG);
		lut_base = DISP_GAMMA_LUT;
		lut = (struct drm_color_lut *)state->gamma_lut->data;
		for (i = 0; i < MTK_LUT_SIZE; i++) {
			word = (((lut[i].red >> 6) & LUT_10BIT_MASK) << 20) +
				(((lut[i].green >> 6) & LUT_10BIT_MASK) << 10) +
				((lut[i].blue >> 6) & LUT_10BIT_MASK);
			mtk_ddp_write(cmdq_pkt, word, comp, (lut_base + i * 4));
		}
	}
}

static void mtk_gamma_config(struct mtk_ddp_comp *comp, unsigned int w,
			     unsigned int h, unsigned int vrefresh,
			     unsigned int bpc, struct cmdq_pkt *cmdq_pkt)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	mtk_ddp_write(cmdq_pkt, w << 16 | h, comp, DISP_GAMMA_SIZE);

	if (gamma->data && gamma->data->has_dither)
		mtk_dither_set(comp, bpc, DISP_GAMMA_CFG, cmdq_pkt);
}

static void mtk_gamma_start(struct mtk_ddp_comp *comp)
{
	writel(GAMMA_EN, comp->regs  + DISP_GAMMA_EN);
}

static void mtk_gamma_stop(struct mtk_ddp_comp *comp)
{
	writel_relaxed(0x0, comp->regs + DISP_GAMMA_EN);
}

static const struct mtk_ddp_comp_funcs mtk_disp_gamma_funcs = {
	.gamma_set = mtk_gamma_set,
	.config = mtk_gamma_config,
	.start = mtk_gamma_start,
	.stop = mtk_gamma_stop,
};

static int mtk_disp_gamma_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %pOF: %d\n",
			dev->of_node, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_gamma_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_gamma_component_ops = {
	.bind	= mtk_disp_gamma_bind,
	.unbind = mtk_disp_gamma_unbind,
};

static int mtk_disp_gamma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_gamma *priv;
	int comp_id;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_GAMMA);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_gamma_funcs);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to initialize component: %d\n",
				ret);

		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = component_add(dev, &mtk_disp_gamma_component_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int mtk_disp_gamma_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_gamma_component_ops);

	return 0;
}

static const struct mtk_disp_gamma_data mt8173_gamma_driver_data = {
	.has_dither = true,
};

static const struct of_device_id mtk_disp_gamma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt8173-disp-gamma",
	  .data = &mt8173_gamma_driver_data},
	{ .compatible = "mediatek,mt8183-disp-gamma"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_gamma_driver_dt_match);

struct platform_driver mtk_disp_gamma_driver = {
	.probe		= mtk_disp_gamma_probe,
	.remove		= mtk_disp_gamma_remove,
	.driver		= {
		.name	= "mediatek-disp-gamma",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_gamma_driver_dt_match,
	},
};
