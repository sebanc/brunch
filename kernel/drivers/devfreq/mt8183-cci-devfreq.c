// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.

 * Author: Andrew-sh.Cheng <andrew-sh.cheng@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>

#include "governor.h"

#define MAX_VOLT_LIMIT		(1150000)

struct cci_devfreq {
	struct devfreq *devfreq;
	struct regulator *proc_reg;
	struct clk *cci_clk;
	int old_vproc;
	unsigned long old_freq;
};

static int mtk_cci_set_voltage(struct cci_devfreq *cci_df, int vproc)
{
	int ret;

	ret = regulator_set_voltage(cci_df->proc_reg, vproc,
						MAX_VOLT_LIMIT);
	if (!ret)
		cci_df->old_vproc = vproc;
	return ret;
}

static int mtk_cci_devfreq_target(struct device *dev, unsigned long *freq,
				  u32 flags)
{
	int ret;
	struct cci_devfreq *cci_df = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long opp_rate, opp_voltage, old_voltage;

	if (!cci_df)
		return -EINVAL;

	if (cci_df->old_freq == *freq)
		return 0;

	opp_rate = *freq;
	opp = dev_pm_opp_find_freq_floor(dev, &opp_rate);
	opp_voltage = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	old_voltage = cci_df->old_vproc;
	if (old_voltage == 0)
		old_voltage = regulator_get_voltage(cci_df->proc_reg);

	// scale up: set voltage first then freq
	if (opp_voltage > old_voltage) {
		ret = mtk_cci_set_voltage(cci_df, opp_voltage);
		if (ret) {
			pr_err("cci: failed to scale up voltage\n");
			return ret;
		}
	}

	ret = clk_set_rate(cci_df->cci_clk, *freq);
	if (ret) {
		pr_err("%s: failed cci to set rate: %d\n", __func__,
		       ret);
		mtk_cci_set_voltage(cci_df, old_voltage);
		return ret;
	}

	// scale down: set freq first then voltage
	if (opp_voltage < old_voltage) {
		ret = mtk_cci_set_voltage(cci_df, opp_voltage);
		if (ret) {
			pr_err("cci: failed to scale down voltage\n");
			clk_set_rate(cci_df->cci_clk, cci_df->old_freq);
			return ret;
		}
	}

	cci_df->old_freq = *freq;

	return 0;
}

static struct devfreq_dev_profile cci_devfreq_profile = {
	.target = mtk_cci_devfreq_target,
};

static int mtk_cci_devfreq_probe(struct platform_device *pdev)
{
	struct device *cci_dev = &pdev->dev;
	struct cci_devfreq *cci_df;
	struct devfreq_passive_data *passive_data;
	int ret;

	cci_df = devm_kzalloc(cci_dev, sizeof(*cci_df), GFP_KERNEL);
	if (!cci_df)
		return -ENOMEM;

	cci_df->cci_clk = devm_clk_get(cci_dev, "cci_clock");
	ret = PTR_ERR_OR_ZERO(cci_df->cci_clk);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(cci_dev, "failed to get clock for CCI: %d\n",
				ret);
		return ret;
	}
	cci_df->proc_reg = devm_regulator_get_optional(cci_dev, "proc");
	ret = PTR_ERR_OR_ZERO(cci_df->proc_reg);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(cci_dev, "failed to get regulator for CCI: %d\n",
				ret);
		return ret;
	}

	ret = dev_pm_opp_of_add_table(cci_dev);
	if (ret) {
		dev_err(cci_dev, "Fail to init CCI OPP table: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, cci_df);

	passive_data = devm_kzalloc(cci_dev, sizeof(*passive_data), GFP_KERNEL);
	if (!passive_data)
		return -ENOMEM;

	passive_data->parent_type = CPUFREQ_PARENT_DEV;

	cci_df->devfreq = devm_devfreq_add_device(cci_dev,
						  &cci_devfreq_profile,
						  DEVFREQ_GOV_PASSIVE,
						  passive_data);
	if (IS_ERR(cci_df->devfreq)) {
		ret = PTR_ERR(cci_df->devfreq);
		dev_err(cci_dev, "cannot create cci devfreq device:%d\n", ret);
		dev_pm_opp_of_remove_table(cci_dev);
		return ret;
	}

	return 0;
}

static const __maybe_unused struct of_device_id
	mediatek_cci_devfreq_of_match[] = {
	{ .compatible = "mediatek,mt8183-cci" },
	{ },
};
MODULE_DEVICE_TABLE(of, mediatek_cci_devfreq_of_match);

static struct platform_driver cci_devfreq_driver = {
	.probe	= mtk_cci_devfreq_probe,
	.driver = {
		.name = "mediatek-cci-devfreq",
		.of_match_table = of_match_ptr(mediatek_cci_devfreq_of_match),
	},
};

static int __init mtk_cci_devfreq_init(void)
{
	return platform_driver_register(&cci_devfreq_driver);
}
module_init(mtk_cci_devfreq_init)

static void __exit mtk_cci_devfreq_exit(void)
{
	platform_driver_unregister(&cci_devfreq_driver);
}
module_exit(mtk_cci_devfreq_exit)

MODULE_DESCRIPTION("Mediatek CCI devfreq driver");
MODULE_AUTHOR("Andrew-sh.Cheng <andrew-sh.cheng@mediatek.com>");
MODULE_LICENSE("GPL v2");
