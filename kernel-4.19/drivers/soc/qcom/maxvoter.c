// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, The Linux Foundation. All rights reserved. */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>

static const char * const pd_names[] = {
	"mx",
	"cx"
};

static int maxvoter_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pd_names); i++) {
		struct device *dev;

		dev = dev_pm_domain_attach_by_name(&pdev->dev, pd_names[i]);
		if (IS_ERR(dev))
			return PTR_ERR(dev);
		if (!device_link_add(&pdev->dev, dev, DL_FLAG_STATELESS |
				     DL_FLAG_PM_RUNTIME))
			return -EINVAL;
		dev_pm_genpd_set_performance_state(dev, INT_MAX);
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	return 0;
}

static const struct of_device_id maxvoter_match_table[] = {
	{ .compatible = "qcom,maxvoter" },
	{ },
};

static struct platform_driver maxvoter_driver = {
	.probe  = maxvoter_probe,
	.driver = {
		   .name = "maxvoter",
		   .of_match_table = maxvoter_match_table,
	},
};

static int __init maxvoter_init(void)
{
	return platform_driver_register(&maxvoter_driver);
}
arch_initcall(maxvoter_init);
