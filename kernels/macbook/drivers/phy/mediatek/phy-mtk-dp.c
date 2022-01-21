// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 BayLibre
 * Author: Markus Schneider-Pargmann <msp@baylibre.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define PHY_OFFSET 0x1000

#define MTK_DP_PHY_DIG_PLL_CTL_1		(PHY_OFFSET + 0x014)
# define TPLL_SSC_EN				BIT(3)

#define MTK_DP_PHY_DIG_BIT_RATE			(PHY_OFFSET + 0x03C)
# define BIT_RATE_RBR				0
# define BIT_RATE_HBR				1
# define BIT_RATE_HBR2				2
# define BIT_RATE_HBR3				3

#define MTK_DP_PHY_DIG_SW_RST			(PHY_OFFSET + 0x038)
# define DP_GLB_SW_RST_PHYD			BIT(0)

#define MTK_DP_LANE0_DRIVING_PARAM_3		(PHY_OFFSET + 0x138)
#define MTK_DP_LANE1_DRIVING_PARAM_3		(PHY_OFFSET + 0x238)
#define MTK_DP_LANE2_DRIVING_PARAM_3		(PHY_OFFSET + 0x338)
#define MTK_DP_LANE3_DRIVING_PARAM_3		(PHY_OFFSET + 0x438)
# define XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT	0x10
# define XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT	(0x14 << 8)
# define XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT	(0x18 << 16)
# define XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT	(0x20 << 24)
# define DRIVING_PARAM_3_DEFAULT		(XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT)

#define MTK_DP_LANE0_DRIVING_PARAM_4		(PHY_OFFSET + 0x13C)
#define MTK_DP_LANE1_DRIVING_PARAM_4		(PHY_OFFSET + 0x23C)
#define MTK_DP_LANE2_DRIVING_PARAM_4		(PHY_OFFSET + 0x33C)
#define MTK_DP_LANE3_DRIVING_PARAM_4		(PHY_OFFSET + 0x43C)
# define XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT	0x18
# define XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT	(0x1e << 8)
# define XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT	(0x24 << 16)
# define XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT	(0x20 << 24)
# define DRIVING_PARAM_4_DEFAULT		(XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT)

#define MTK_DP_LANE0_DRIVING_PARAM_5		(PHY_OFFSET + 0x140)
#define MTK_DP_LANE1_DRIVING_PARAM_5		(PHY_OFFSET + 0x240)
#define MTK_DP_LANE2_DRIVING_PARAM_5		(PHY_OFFSET + 0x340)
#define MTK_DP_LANE3_DRIVING_PARAM_5		(PHY_OFFSET + 0x440)
# define XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT	0x28
# define XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT	(0x30 << 8)
# define DRIVING_PARAM_5_DEFAULT		(XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT | \
						 XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT)

#define MTK_DP_LANE0_DRIVING_PARAM_6		(PHY_OFFSET + 0x144)
#define MTK_DP_LANE1_DRIVING_PARAM_6		(PHY_OFFSET + 0x244)
#define MTK_DP_LANE2_DRIVING_PARAM_6		(PHY_OFFSET + 0x344)
#define MTK_DP_LANE3_DRIVING_PARAM_6		(PHY_OFFSET + 0x444)
# define XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT	0x00
# define XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT	(0x04 << 8)
# define XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT	(0x08 << 16)
# define XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT	(0x10 << 24)
# define DRIVING_PARAM_6_DEFAULT		(XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT)

#define MTK_DP_LANE0_DRIVING_PARAM_7		(PHY_OFFSET + 0x148)
#define MTK_DP_LANE1_DRIVING_PARAM_7		(PHY_OFFSET + 0x248)
#define MTK_DP_LANE2_DRIVING_PARAM_7		(PHY_OFFSET + 0x348)
#define MTK_DP_LANE3_DRIVING_PARAM_7		(PHY_OFFSET + 0x448)
# define XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT	0x00
# define XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT	(0x06 << 8)
# define XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT	(0x0c << 16)
# define XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT	(0x00 << 24)
# define DRIVING_PARAM_7_DEFAULT		(XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT)

#define MTK_DP_LANE0_DRIVING_PARAM_8		(PHY_OFFSET + 0x14C)
#define MTK_DP_LANE1_DRIVING_PARAM_8		(PHY_OFFSET + 0x24C)
#define MTK_DP_LANE2_DRIVING_PARAM_8		(PHY_OFFSET + 0x34C)
#define MTK_DP_LANE3_DRIVING_PARAM_8		(PHY_OFFSET + 0x44C)
# define XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT	0x08
# define XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT	(0x00 << 8)
# define DRIVING_PARAM_8_DEFAULT		(XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT | \
						 XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT)

struct mtk_dp_phy {
	struct regmap *regs;
};

static int mtk_dp_phy_init(struct phy *phy)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);
	u32 driving_params[] = {
		DRIVING_PARAM_3_DEFAULT,
		DRIVING_PARAM_4_DEFAULT,
		DRIVING_PARAM_5_DEFAULT,
		DRIVING_PARAM_6_DEFAULT,
		DRIVING_PARAM_7_DEFAULT,
		DRIVING_PARAM_8_DEFAULT
	};

	regmap_bulk_write(dp_phy->regs, MTK_DP_LANE0_DRIVING_PARAM_3,
			  driving_params, ARRAY_SIZE(driving_params));
	regmap_bulk_write(dp_phy->regs, MTK_DP_LANE1_DRIVING_PARAM_3,
			  driving_params, ARRAY_SIZE(driving_params));
	regmap_bulk_write(dp_phy->regs, MTK_DP_LANE2_DRIVING_PARAM_3,
			  driving_params, ARRAY_SIZE(driving_params));
	regmap_bulk_write(dp_phy->regs, MTK_DP_LANE3_DRIVING_PARAM_3,
			  driving_params, ARRAY_SIZE(driving_params));

	return 0;
}

static int mtk_dp_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);
	u32 val;

	if (opts->dp.set_rate) {
		switch (opts->dp.link_rate) {
		default:
			dev_err(&phy->dev,
				"Implementation error, unknown linkrate %x\n",
				opts->dp.link_rate);
			return -EINVAL;
		case 1620:
			val = BIT_RATE_RBR;
			break;
		case 2700:
			val = BIT_RATE_HBR;
			break;
		case 5400:
			val = BIT_RATE_HBR2;
			break;
		case 8100:
			val = BIT_RATE_HBR3;
			break;
		}
		regmap_write(dp_phy->regs, MTK_DP_PHY_DIG_BIT_RATE, val);
	}

	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_PLL_CTL_1,
			   TPLL_SSC_EN, opts->dp.ssc ? TPLL_SSC_EN : 0);

	return 0;
}

static int mtk_dp_phy_reset(struct phy *phy)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);

	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   DP_GLB_SW_RST_PHYD, 0);
	usleep_range(50, 200);
	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   DP_GLB_SW_RST_PHYD, 1);

	return 0;
}

static const struct phy_ops mtk_dp_phy_dev_ops = {
	.init = mtk_dp_phy_init,
	.configure = mtk_dp_phy_configure,
	.reset = mtk_dp_phy_reset,
	.owner = THIS_MODULE,
};

static int mtk_dp_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_phy *dp_phy;
	struct phy *phy;

	dp_phy = devm_kzalloc(dev, sizeof(*dp_phy), GFP_KERNEL);
	if (!dp_phy)
		return -ENOMEM;

	dp_phy->regs = *(struct regmap **)dev->platform_data;
	if (!dp_phy->regs) {
		dev_err(dev, "No data passed, requires struct regmap**\n");
		return -EINVAL;
	}

	phy = devm_phy_create(dev, NULL, &mtk_dp_phy_dev_ops);
	if (IS_ERR(phy)) {
		dev_err(dev, "Failed to create DP PHY: %ld\n", PTR_ERR(phy));
		return PTR_ERR(phy);
	}
	phy_set_drvdata(phy, dp_phy);

	// Set device data to the phy so that mtk-dp can get it easily
	dev_set_drvdata(dev, phy);

	return 0;
}

struct platform_driver mtk_dp_phy_driver = {
	.probe = mtk_dp_phy_probe,
	.driver = {
		.name = "mediatek-dp-phy",
	},
};
module_platform_driver(mtk_dp_phy_driver);

MODULE_AUTHOR("Markus Schneider-Pargmann <msp@baylibre.com>");
MODULE_DESCRIPTION("MediaTek DP PHY Driver");
MODULE_LICENSE("GPL v2");
