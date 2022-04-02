// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk-mmsys.h"
#include "mt8167-mmsys.h"
#include "mt8183-mmsys.h"
#include "mt8192-mmsys.h"
#include "mt8195-mmsys.h"
#include "mt8365-mmsys.h"

#define MMSYS_SW_RESET_PER_REG 32

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.clk_driver = "clk-mt2701-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.clk_driver = "clk-mt2712-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt6779_mmsys_driver_data = {
	.clk_driver = "clk-mt6779-mm",
};

static const struct mtk_mmsys_driver_data mt6797_mmsys_driver_data = {
	.clk_driver = "clk-mt6797-mm",
};

static const struct mtk_mmsys_driver_data mt8167_mmsys_driver_data = {
	.clk_driver = "clk-mt8167-mm",
	.routes = mt8167_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt8167_mmsys_routing_table),
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.clk_driver = "clk-mt8173-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
	.sw_reset_start = MMSYS_SW0_RST_B,
	.num_resets = 32,
};

static const struct mtk_mmsys_driver_data mt8183_mmsys_driver_data = {
	.clk_driver = "clk-mt8183-mm",
	.routes = mmsys_mt8183_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8183_routing_table),
	.sw_reset_start = MMSYS_SW0_RST_B,
	.num_resets = 32,
	.mdp_isp_ctrl = mmsys_mt8183_mdp_isp_ctrl_table,
	.has_gce_client_reg = true,
};

static const struct mtk_mmsys_driver_data mt8192_mmsys_driver_data = {
	.clk_driver = "clk-mt8192-mm",
	.routes = mmsys_mt8192_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8192_routing_table),
};

static const struct mtk_mmsys_driver_data mt8195_vdosys0_driver_data = {
	.clk_driver = "clk-mt8195-vdo0",
	.routes = mmsys_mt8195_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8195_routing_table),
};

static const struct mtk_mmsys_driver_data mt8195_vdosys1_driver_data = {
	.clk_driver = "clk-mt8195-vdo1",
	.routes = mmsys_mt8195_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8195_routing_table),
	.config = mmsys_mt8195_config_table,
	.num_configs = ARRAY_SIZE(mmsys_mt8195_config_table),
	.sw_reset_start = MT8195_VDO1_SW0_RST_B,
	.num_resets = 64,
	.has_gce_client_reg = true,
};

static const struct mtk_mmsys_driver_data mt8365_mmsys_driver_data = {
	.clk_driver = "clk-mt8365-mm",
	.routes = mt8365_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt8365_mmsys_routing_table),
};

static const struct mtk_mmsys_driver_data mt8195_vppsys0_driver_data = {
	.clk_driver = "clk-mt8195-vpp0",
	.mdp_mmsys_configs = mmsys_mt8195_mdp_vppsys_config_table,
	.mdp_num_mmsys_configs = ARRAY_SIZE(mmsys_mt8195_mdp_vppsys_config_table),
	.vppsys = true,
	.has_gce_client_reg = true,
};

static const struct mtk_mmsys_driver_data mt8195_vppsys1_driver_data = {
	.clk_driver = "clk-mt8195-vpp1",
	.mdp_mmsys_configs = mmsys_mt8195_mdp_vppsys_config_table,
	.mdp_num_mmsys_configs = ARRAY_SIZE(mmsys_mt8195_mdp_vppsys_config_table),
	.vppsys = true,
	.has_gce_client_reg = true,
};

struct mtk_mmsys {
	void __iomem *regs;
	const struct mtk_mmsys_driver_data *data;
	spinlock_t lock; /* protects mmsys_sw_rst_b reg */
	struct reset_controller_dev rcdev;
	struct cmdq_client_reg cmdq_base;
	phys_addr_t addr;
};

void mtk_mmsys_ddp_connect(struct device *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	u32 reg;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp) {
			reg = readl_relaxed(mmsys->regs + routes[i].addr);
			reg &= ~routes[i].mask;
			reg |= routes[i].val;
			writel_relaxed(reg, mmsys->regs + routes[i].addr);
		}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_connect);

void mtk_mmsys_ddp_disconnect(struct device *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	u32 reg;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp) {
			reg = readl_relaxed(mmsys->regs + routes[i].addr);
			reg &= ~routes[i].mask;
			writel_relaxed(reg, mmsys->regs + routes[i].addr);
		}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_disconnect);

void mtk_mmsys_mdp_isp_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			    enum mtk_mdp_comp_id id)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const unsigned int *isp_ctrl = mmsys->data->mdp_isp_ctrl;
	u32 reg;

	/* Direct link */
	if (id == MDP_COMP_CAMIN) {
		/* Reset MDP_DL_ASYNC_TX */
		if (isp_ctrl[ISP_REG_MMSYS_SW0_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW0_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX]);
		}

		/* Reset MDP_DL_ASYNC_RX */
		if (isp_ctrl[ISP_REG_MMSYS_SW1_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW1_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX]);
		}

		/* Enable sof mode */
		if (isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_NO_SOF_MODE]);
		}
	}

	if (id == MDP_COMP_CAMIN2) {
		/* Reset MDP_DL_ASYNC2_TX */
		if (isp_ctrl[ISP_REG_MMSYS_SW0_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW0_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX2]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX2],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX2]);
		}

		/* Reset MDP_DL_ASYNC2_RX */
		if (isp_ctrl[ISP_REG_MMSYS_SW1_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW1_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX2]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX2],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX2]);
		}

		/* Enable sof mode */
		if (isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_NO_SOF_MODE]);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_isp_ctrl);

void mtk_mmsys_mdp_camin_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id id, u32 camin_w, u32 camin_h)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const unsigned int *isp_ctrl = mmsys->data->mdp_isp_ctrl;
	u32 reg;

	/* Config for direct link */
	if (id == MDP_COMP_CAMIN) {
		if (isp_ctrl[ISP_REG_MDP_ASYNC_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MDP_ASYNC_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}

		if (isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}
	}
	if (id == MDP_COMP_CAMIN2) {
		if (isp_ctrl[ISP_REG_MDP_ASYNC_IPU_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MDP_ASYNC_IPU_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}
		if (isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_camin_ctrl);

static int mtk_mmsys_reset_update(struct reset_controller_dev *rcdev, unsigned long id,
				  bool assert)
{
	struct mtk_mmsys *mmsys = container_of(rcdev, struct mtk_mmsys, rcdev);
	unsigned long flags;
	u32 offset;
	u32 reg;

	offset = (id / MMSYS_SW_RESET_PER_REG) * sizeof(u32);
	id = id % MMSYS_SW_RESET_PER_REG;

	spin_lock_irqsave(&mmsys->lock, flags);

	reg = readl_relaxed(mmsys->regs + mmsys->data->sw_reset_start + offset);

	if (assert)
		reg &= ~BIT(id);
	else
		reg |= BIT(id);

	writel_relaxed(reg, mmsys->regs + mmsys->data->sw_reset_start + offset);

	spin_unlock_irqrestore(&mmsys->lock, flags);

	return 0;
}

static int mtk_mmsys_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return mtk_mmsys_reset_update(rcdev, id, true);
}

static int mtk_mmsys_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return mtk_mmsys_reset_update(rcdev, id, false);
}

static int mtk_mmsys_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	int ret;

	ret = mtk_mmsys_reset_assert(rcdev, id);
	if (ret)
		return ret;

	usleep_range(1000, 1100);

	return mtk_mmsys_reset_deassert(rcdev, id);
}

static const struct reset_control_ops mtk_mmsys_reset_ops = {
	.assert = mtk_mmsys_reset_assert,
	.deassert = mtk_mmsys_reset_deassert,
	.reset = mtk_mmsys_reset,
};

void mtk_mmsys_ddp_config(struct device *dev, enum mtk_mmsys_config_type config,
			  u32 id, u32 val, struct cmdq_pkt *cmdq_pkt)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_config *mmsys_config = mmsys->data->config;
	u32 reg_val;
	u32 mask;
	u32 offset;
	u32 tmp;
	int i;

	if (!mmsys->data->num_configs)
		return;

	for (i = 0; i < mmsys->data->num_configs; i++)
		if (config == mmsys_config[i].config && id == mmsys_config[i].id)
			break;

	if (i == mmsys->data->num_configs)
		return;

	offset = mmsys_config[i].addr;
	mask = mmsys_config[i].mask;
	reg_val = val << mmsys_config[i].shift;

	if (cmdq_pkt && mmsys->cmdq_base.size) {
		cmdq_pkt_write_mask(cmdq_pkt, mmsys->cmdq_base.subsys,
				    mmsys->cmdq_base.offset + offset, reg_val,
				    mask);
	} else {
		tmp = readl(mmsys->regs + offset);

		tmp = (tmp & ~mask) | reg_val;
		writel(tmp, mmsys->regs + offset);
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_config);

void mtk_mmsys_mdp_write_config(struct device *dev,
			 struct mmsys_cmdq_cmd *cmd,
			 u32 alias_id, u32 value, u32 mask)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const u32 *configs = mmsys->data->mdp_mmsys_configs;

	cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys,
			    mmsys->addr + configs[alias_id], value, mask);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_write_config);

void mtk_mmsys_write_reg_by_cmdq(struct device *dev,
			 struct mmsys_cmdq_cmd *cmd,
			 u32 offset, u32 value, u32 mask)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);

	cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys,
			    mmsys->addr + offset, value, mask);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_write_reg_by_cmdq);

static int mtk_mmsys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *clks;
	struct platform_device *drm;
	struct mtk_mmsys *mmsys;
	struct resource res;
	int ret;

	mmsys = devm_kzalloc(dev, sizeof(*mmsys), GFP_KERNEL);
	if (!mmsys)
		return -ENOMEM;

	mmsys->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mmsys->regs)) {
		ret = PTR_ERR(mmsys->regs);
		dev_err(dev, "Failed to ioremap mmsys registers: %d\n", ret);
		return ret;
	}

	mmsys->data = of_device_get_match_data(&pdev->dev);
	if (!mmsys->data) {
		dev_err(dev, "Couldn't get match driver data\n");
		return -EINVAL;
	}

	spin_lock_init(&mmsys->lock);

	mmsys->rcdev.owner = THIS_MODULE;
	mmsys->rcdev.nr_resets = mmsys->data->num_resets;
	mmsys->rcdev.ops = &mtk_mmsys_reset_ops;
	mmsys->rcdev.of_node = pdev->dev.of_node;
	ret = devm_reset_controller_register(&pdev->dev, &mmsys->rcdev);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't register mmsys reset controller: %d\n", ret);
		return ret;
	}

	if (of_address_to_resource(dev->of_node, 0, &res) < 0)
		mmsys->addr = 0L;
	else
		mmsys->addr = res.start;

	if (mmsys->data->has_gce_client_reg) {
		ret = cmdq_dev_get_client_reg(dev, &mmsys->cmdq_base, 0);
		if (ret) {
			dev_err(dev, "No mediatek,gce-client-reg!\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, mmsys);

	clks = platform_device_register_data(&pdev->dev, mmsys->data->clk_driver,
					     PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(clks))
		return PTR_ERR(clks);

	if (mmsys->data->vppsys)
		goto EXIT;

	drm = platform_device_register_data(&pdev->dev, "mediatek-drm",
					    PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(drm)) {
		platform_device_unregister(clks);
		return PTR_ERR(drm);
	}

EXIT:
	return 0;
}

static const struct of_device_id of_match_mtk_mmsys[] = {
	{
		.compatible = "mediatek,mt2701-mmsys",
		.data = &mt2701_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt2712-mmsys",
		.data = &mt2712_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt6779-mmsys",
		.data = &mt6779_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt6797-mmsys",
		.data = &mt6797_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8167-mmsys",
		.data = &mt8167_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8173-mmsys",
		.data = &mt8173_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8183-mmsys",
		.data = &mt8183_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8192-mmsys",
		.data = &mt8192_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vppsys0",
		.data = &mt8195_vppsys0_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vppsys1",
		.data = &mt8195_vppsys1_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vdosys0",
		.data = &mt8195_vdosys0_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vdosys1",
		.data = &mt8195_vdosys1_driver_data,
	},
	{
		.compatible = "mediatek,mt8365-mmsys",
		.data = &mt8365_mmsys_driver_data,
	},
	{ }
};

static struct platform_driver mtk_mmsys_drv = {
	.driver = {
		.name = "mtk-mmsys",
		.of_match_table = of_match_mtk_mmsys,
	},
	.probe = mtk_mmsys_probe,
};

builtin_platform_driver(mtk_mmsys_drv);
