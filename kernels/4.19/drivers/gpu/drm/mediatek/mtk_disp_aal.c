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

#define DISP_AAL_EN				0x0000
#define AAL_EN					BIT(0)
#define DISP_AAL_SIZE				0x0030

struct mtk_disp_aal_data {
	bool has_gamma;
};

/**
 * struct mtk_disp_aal - DISP_AAL driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_aal {
	struct mtk_ddp_comp			ddp_comp;
	const struct mtk_disp_aal_data	*data;
};

static inline struct mtk_disp_aal *comp_to_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_aal, ddp_comp);
}

static void mtk_aal_config(struct mtk_ddp_comp *comp, unsigned int w,
			   unsigned int h, unsigned int vrefresh,
			   unsigned int bpc, struct cmdq_pkt *cmdq_pkt)
{
	mtk_ddp_write(cmdq_pkt, h << 16 | w, comp, DISP_AAL_SIZE);
}

static void mtk_aal_start(struct mtk_ddp_comp *comp)
{
	writel(AAL_EN, comp->regs + DISP_AAL_EN);
}

static void mtk_aal_stop(struct mtk_ddp_comp *comp)
{
	writel_relaxed(0x0, comp->regs + DISP_AAL_EN);
}

static void mtk_aal_gamma_set(struct mtk_ddp_comp *comp, struct drm_crtc_state *state,
		   struct cmdq_pkt *cmdq_pkt)
{
	struct mtk_disp_aal *aal = comp_to_aal(comp);


	if (aal->data && aal->data->has_gamma)
		mtk_gamma_set(comp,state, cmdq_pkt);
}

static const struct mtk_ddp_comp_funcs mtk_disp_aal_funcs = {
	.gamma_set = mtk_aal_gamma_set,
	.config = mtk_aal_config,
	.start = mtk_aal_start,
	.stop = mtk_aal_stop,
};

static int mtk_disp_aal_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
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

static void mtk_disp_aal_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_aal_component_ops = {
	.bind	= mtk_disp_aal_bind,
	.unbind = mtk_disp_aal_unbind,
};

static int mtk_disp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_aal *priv;
	int comp_id;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_AAL);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_aal_funcs);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to initialize component: %d\n",
				ret);

		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = component_add(dev, &mtk_disp_aal_component_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int mtk_disp_aal_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_aal_component_ops);

	return 0;
}

static const struct mtk_disp_aal_data mt8173_aal_driver_data = {
	.has_gamma = true,
};

static const struct of_device_id mtk_disp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt8173-disp-aal",
	  .data = &mt8173_aal_driver_data},
	{ .compatible = "mediatek,mt8183-disp-aal"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_aal_driver_dt_match);

struct platform_driver mtk_disp_aal_driver = {
	.probe		= mtk_disp_aal_probe,
	.remove		= mtk_disp_aal_remove,
	.driver		= {
		.name	= "mediatek-disp-aal",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_aal_driver_dt_match,
	},
};
