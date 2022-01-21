/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/of_device.h>

#include <mali_kbase_config.h>

#include "mali_kbase_config_platform.h"

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

static struct kbase_platform_config dummy_platform_config;

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &dummy_platform_config;
}

int kbase_platform_register(void)
{
	return 0;
}

void kbase_platform_unregister(void)
{
}

#ifdef CONFIG_OF
static const struct kbase_mali_platform_functions mediatek_mt8183_data = {
	.pm_callbacks = &mt8183_pm_callbacks,
	.platform_funcs = &mt8183_platform_funcs,
};

static const struct kbase_mali_platform_functions mediatek_mt8192_data = {
	.pm_callbacks = &mt8192_pm_callbacks,
	.platform_funcs = &mt8192_platform_funcs,
};

static const struct kbase_mali_platform_functions mediatek_mt8195_data = {
	.pm_callbacks = &mt8195_pm_callbacks,
	.platform_funcs = &mt8195_platform_funcs,
};

const struct of_device_id kbase_dt_ids[] = {
	{ .compatible = "arm,malit6xx" },
	{ .compatible = "arm,mali-midgard" },
	{ .compatible = "arm,mali-bifrost" },
	{ .compatible = "arm,mali-valhall" },
	{ .compatible = "mediatek,mt8183-mali", .data = &mediatek_mt8183_data },
	{ .compatible = "mediatek,mt8192-mali", .data = &mediatek_mt8192_data },
	{ .compatible = "mediatek,mt8195-mali", .data = &mediatek_mt8195_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, kbase_dt_ids);
#endif
