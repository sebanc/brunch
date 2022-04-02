// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2015, 2017, 2020-2021 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>

#include <mali_kbase.h>
#include <mali_kbase_config.h>
#include <backend/gpu/mali_kbase_pm_internal.h>

static struct kbase_platform_config dummy_platform_config;

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &dummy_platform_config;
}

#ifndef CONFIG_OF
int kbase_platform_register(void)
{
	return 0;
}

void kbase_platform_unregister(void)
{
}
#endif

#ifdef CONFIG_MALI_VALHALL_DVFS
#if MALI_USE_CSF
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
#else
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation, u32 util_gl_share, u32 util_cl_share[2])
#endif
{
	return 1;
}
#endif /* CONFIG_MALI_VALHALL_DVFS */

#ifdef CONFIG_OF
const struct of_device_id kbase_dt_ids[] = {
	{ .compatible = "arm,malit6xx" },
	{ .compatible = "arm,mali-midgard" },
	{ .compatible = "arm,mali-bifrost" },
	{ .compatible = "arm,mali-valhall" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, kbase_dt_ids);
#endif
