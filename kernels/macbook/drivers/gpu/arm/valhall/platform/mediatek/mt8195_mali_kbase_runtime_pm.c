/*
 * Copyright (C) 2021 MediaTek Inc.
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

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <mali_kbase.h>
#include "mali_kbase_config_platform.h"
#include <mali_kbase_defs.h>


/* Definition for PMIC regulators */
#define VSRAM_GPU_MAX_VOLT (750000)	/* uV */
#define VSRAM_GPU_MIN_VOLT (750000)	/* uV */
#define VGPU_MAX_VOLT (750000)	/* uV */
#define VGPU_MIN_VOLT (625000)	/* uV */

#define MIN_VOLT_BIAS (0)	/* uV */
#define MAX_VOLT_BIAS (250000)	/* uV */
#define VOLT_TOL (125)	/* uV */

#define GPU_CORE_NUM 5

/* Definition for MFG registers */
#define MFG_QCHANNEL_CON 0xb4
#define MFG_DEBUG_SEL 0x170
#define MFG_DEBUG_TOP 0x178
#define BUS_IDLE_BIT 0x4
#define MFG_TIMESTAMP 0x130
#define TOP_TSVALUEB_EN 0x00000001

/**
 * Maximum frequency GPU will be clocked at. Given in kHz.
 * This must be specified as there is no default value.
 *
 * Attached value: number in kHz
 * Default value: NA
 */
#define GPU_FREQ_KHZ_MAX (880000)
/**
 * Minimum frequency GPU will be clocked at. Given in kHz.
 * This must be specified as there is no default value.
 *
 * Attached value: number in kHz
 * Default value: NA
 */
#define GPU_FREQ_KHZ_MIN (390000)
/**
 * Autosuspend delay
 *
 * The delay time (in milliseconds) to be used for autosuspend
 */
#define AUTO_SUSPEND_DELAY (50)

enum gpu_clk_idx {mux, pll, main, sub, cg};
/* list of clocks required by GPU */
static const char * const gpu_clocks[] = {
	"clk_mux",
	"clk_pll_src",
	"clk_main_parent",
	"clk_sub_parent",
	"subsys_bg3d",
};

static void pm_domain_term(struct kbase_device *kbdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(kbdev->pm_domain_devs); i++) {
		if (kbdev->pm_domain_devs[i])
			dev_pm_domain_detach(kbdev->pm_domain_devs[i], true);
	}
}

static int pm_domain_init(struct kbase_device *kbdev)
{
	int err;
	int i, num_domains, num_domain_names;
	const char *pd_names[GPU_CORE_NUM];

	num_domains = of_count_phandle_with_args(kbdev->dev->of_node,
						 "power-domains",
						 "#power-domain-cells");

	num_domain_names = of_property_count_strings(kbdev->dev->of_node,
					"power-domain-names");

	/*
	 * Single domain is handled by the core, and, if only a single power
	 * the power domain is requested, the property is optional.
	 */
	if (num_domains < 2 && kbdev->num_pm_domains < 2)
		return 0;

	if (num_domains != num_domain_names) {
		dev_err(kbdev->dev,
			"Device tree power domains are not match: PD %d, PD names %d\n",
			num_domains, num_domain_names);
		return -EINVAL;
	}

	if (num_domains != kbdev->num_pm_domains) {
		dev_err(kbdev->dev,
			"Incorrect number of power domains: %d provided, %d needed\n",
			num_domains, kbdev->num_pm_domains);
		return -EINVAL;
	}

	if (WARN(num_domains > ARRAY_SIZE(kbdev->pm_domain_devs),
			"Too many supplies in compatible structure.\n"))
		return -EINVAL;

	err = of_property_read_string_array(kbdev->dev->of_node,
					    "power-domain-names",
					    pd_names,
					    num_domain_names);

	if (err < 0) {
		dev_err(kbdev->dev, "Error reading supply-names: %d\n", err);
		return err;
	}

	for (i = 0; i < num_domains; i++) {
		kbdev->pm_domain_devs[i] =
			dev_pm_domain_attach_by_name(kbdev->dev,
					pd_names[i]);
		if (IS_ERR_OR_NULL(kbdev->pm_domain_devs[i])) {
			err = PTR_ERR(kbdev->pm_domain_devs[i]) ? : -ENODATA;
			kbdev->pm_domain_devs[i] = NULL;
			if (err == -EPROBE_DEFER) {
				dev_dbg(kbdev->dev,
					"Probe deferral for pm-domain %s(%d)\n",
					pd_names[i], i);
			} else {
				dev_err(kbdev->dev,
					"failed to get pm-domain %s(%d): %d\n",
					pd_names[i], i, err);
			}
			goto err;
		}
	}

	return 0;

err:
	pm_domain_term(kbdev);
	return err;
}

struct mfg_base {
	struct clk_bulk_data *clks;
	int num_clks;
	void __iomem *g_mfg_base;
	bool is_powered;
};

static void check_bus_idle(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = kbdev->platform_context;
	u32 val;

	/* MFG_QCHANNEL_CON (0x13fb_f0b4) bit [1:0] = 0x1 */
	writel(0x00000001, mfg->g_mfg_base + MFG_QCHANNEL_CON);

	/* set register MFG_DEBUG_SEL (0x13fb_f170) bit [7:0] = 0x03 */
	writel(0x00000003, mfg->g_mfg_base + MFG_DEBUG_SEL);

	/* polling register MFG_DEBUG_TOP (0x13fb_f178) bit 2 = 0x1 */
	/* => 1 for bus idle, 0 for bus non-idle */
	do {
		val = readl(mfg->g_mfg_base + MFG_DEBUG_TOP);
	} while ((val & BUS_IDLE_BIT) != BUS_IDLE_BIT);
}

static void enable_timestamp_register(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = kbdev->platform_context;

	/* MFG_TIMESTAMP (0x13fb_f130) bit [0:0]: TOP_TSVALUEB_EN,
	 * write 1 into 0x13fb_f130 bit 0 to enable timestamp register
	 */
	writel(TOP_TSVALUEB_EN, mfg->g_mfg_base + MFG_TIMESTAMP);
}

static void *get_mfg_base(const char *node_name)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		return of_iomap(node, 0);

	return NULL;
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int error, err, r_idx, p_idx;
	struct mfg_base *mfg = kbdev->platform_context;

	if (mfg->is_powered) {
		dev_dbg(kbdev->dev, "mali_device is already powered\n");
		return 0;
	}

	for (r_idx = 0; r_idx < kbdev->nr_regulators; r_idx++) {
		error = regulator_enable(kbdev->regulators[r_idx]);
		if (error < 0) {
			dev_err(kbdev->dev,
				"Power on reg %d failed error = %d\n",
				r_idx, error);
			goto reg_err;
		}
	}

	for (p_idx = 0; p_idx < kbdev->num_pm_domains; p_idx++) {
		error = pm_runtime_get_sync(kbdev->pm_domain_devs[p_idx]);
		if (error < 0) {
			dev_err(kbdev->dev,
				"Power on core %d failed (err: %d)\n",
				p_idx+1, error);
			goto pm_err;
		}
	}

	error = clk_bulk_prepare_enable(mfg->num_clks, mfg->clks);
	if (error < 0) {
		dev_err(kbdev->dev,
			"gpu clock enable failed (err: %d)\n",
			error);
		goto clk_err;
	}

	mfg->is_powered = true;

	enable_timestamp_register(kbdev);

	return 1;

clk_err:
	clk_bulk_disable_unprepare(mfg->num_clks, mfg->clks);

pm_err:
	if (p_idx >= kbdev->num_pm_domains)
		p_idx = kbdev->num_pm_domains - 1;
	for (; p_idx >= 0; p_idx--) {
		pm_runtime_mark_last_busy(kbdev->pm_domain_devs[p_idx]);
		err = pm_runtime_put_autosuspend(kbdev->pm_domain_devs[p_idx]);
		if (err < 0)
			dev_err(kbdev->dev,
				"Power off core %d failed (err: %d)\n",
				p_idx+1, err);
	}

reg_err:
	if (r_idx >= kbdev->nr_regulators)
		r_idx = kbdev->nr_regulators - 1;
	for (; r_idx >= 0; r_idx--) {
		err = regulator_disable(kbdev->regulators[r_idx]);
		if (err < 0)
			dev_err(kbdev->dev,
				"Power off reg %d failed error = %d\n",
				r_idx, err);
	}

	return error;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = kbdev->platform_context;
	int error, i;

	if (!mfg->is_powered) {
		dev_dbg(kbdev->dev, "mali_device is already powered off\n");
		return;
	}

	mfg->is_powered = false;

	check_bus_idle(kbdev);

	clk_bulk_disable_unprepare(mfg->num_clks, mfg->clks);

	for (i = kbdev->num_pm_domains - 1; i >= 0; i--) {
		pm_runtime_mark_last_busy(kbdev->pm_domain_devs[i]);
		error = pm_runtime_put_autosuspend(kbdev->pm_domain_devs[i]);
		if (error < 0)
			dev_err(kbdev->dev,
				"Power off core %d failed (err: %d)\n",
				i+1, error);
	}

	for (i = kbdev->nr_regulators - 1; i >= 0; i--) {
		error = regulator_disable(kbdev->regulators[i]);
		if (error < 0)
			dev_err(kbdev->dev,
				"Power off reg %d failed error = %d\n",
				i, error);
	}
}

static int kbase_device_runtime_init(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "%s\n", __func__);

	return 0;
}

static void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "%s\n", __func__);
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
}

static void pm_callback_resume(struct kbase_device *kbdev)
{
	pm_callback_power_on(kbdev);
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	pm_callback_power_off(kbdev);
}

struct kbase_pm_callback_conf mt8195_pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};


static int mali_mfgsys_init(struct kbase_device *kbdev, struct mfg_base *mfg)
{
	int err, i;
	unsigned long volt;

	kbdev->num_pm_domains = GPU_CORE_NUM;

	err = pm_domain_init(kbdev);
	if (err < 0)
		return err;

	for (i = 0; i < kbdev->nr_regulators; i++)
		if (kbdev->regulators[i] == NULL)
			return -EINVAL;

	mfg->num_clks = ARRAY_SIZE(gpu_clocks);
	mfg->clks = devm_kcalloc(kbdev->dev, mfg->num_clks,
				     sizeof(*mfg->clks), GFP_KERNEL);

	if (!mfg->clks)
		return -ENOMEM;

	for (i = 0; i < mfg->num_clks; ++i)
		mfg->clks[i].id = gpu_clocks[i];

	err = devm_clk_bulk_get(kbdev->dev, mfg->num_clks, mfg->clks);
	if (err != 0) {
		dev_err(kbdev->dev,
			"clk_bulk_get error: %d\n",
			err);
		return err;
	}

	for (i = 0; i < kbdev->nr_regulators; i++) {
		volt = (i == 0) ? VGPU_MAX_VOLT : VSRAM_GPU_MAX_VOLT;
		err = regulator_set_voltage(kbdev->regulators[i],
			volt, volt + VOLT_TOL);
		if (err < 0) {
			dev_err(kbdev->dev,
				"Regulator %d set voltage failed: %d\n",
				i, err);
			return err;
		}
#ifdef CONFIG_MALI_VALHALL_DEVFREQ
		kbdev->current_voltages[i] = volt;
#endif
	}

	mfg->g_mfg_base = get_mfg_base("mediatek,mt8195-mfgcfg");
	if (!mfg->g_mfg_base) {
		dev_err(kbdev->dev, "Cannot find mfgcfg node\n");
		return -ENODEV;
	}

	mfg->is_powered = false;

	return 0;
}

#ifdef CONFIG_MALI_VALHALL_DEVFREQ
static void voltage_range_check(struct kbase_device *kbdev,
				unsigned long *voltages)
{
	if (voltages[1] - voltages[0] < MIN_VOLT_BIAS ||
	    voltages[1] - voltages[0] > MAX_VOLT_BIAS)
		voltages[1] = voltages[0] + MIN_VOLT_BIAS;
	voltages[1] = clamp_t(unsigned long, voltages[1], VSRAM_GPU_MIN_VOLT,
			      VSRAM_GPU_MAX_VOLT);
}

static int set_frequency(struct kbase_device *kbdev, unsigned long freq)
{
	int err;
	struct mfg_base *mfg = kbdev->platform_context;

	if (kbdev->current_freqs[0] != freq) {
		err = clk_set_parent(mfg->clks[mux].clk, mfg->clks[sub].clk);
		if (err) {
			dev_err(kbdev->dev, "Failed to select sub clock src\n");
			return err;
		}

		err = clk_set_rate(mfg->clks[pll].clk, freq);
		if (err) {
			dev_err(kbdev->dev,
				"Failed to set clock rate: %lu (err: %d)\n",
				freq, err);
			return err;
		}
		kbdev->current_freqs[0] = freq;

		err = clk_set_parent(mfg->clks[mux].clk, mfg->clks[main].clk);
		if (err) {
			dev_err(kbdev->dev, "Failed to select main clock src\n");
			return err;
		}
	}

	return 0;
}
#endif

static int platform_init(struct kbase_device *kbdev)
{
	int err, i;
	struct mfg_base *mfg;

	mfg = devm_kzalloc(kbdev->dev, sizeof(*mfg), GFP_KERNEL);
	if (!mfg)
		return -ENOMEM;

	err = mali_mfgsys_init(kbdev, mfg);
	if (err)
		return err;

	kbdev->platform_context = mfg;
	for (i = 0; i < kbdev->num_pm_domains; i++) {
		pm_runtime_set_autosuspend_delay(kbdev->pm_domain_devs[i],
						AUTO_SUSPEND_DELAY);
		pm_runtime_use_autosuspend(kbdev->pm_domain_devs[i]);
	}

	err = clk_set_parent(mfg->clks[mux].clk, mfg->clks[sub].clk);
	if (err) {
		dev_err(kbdev->dev, "Failed to select sub clock src\n");
		return err;
	}

	err = clk_set_rate(mfg->clks[pll].clk, GPU_FREQ_KHZ_MAX * 1000);
	if (err) {
		dev_err(kbdev->dev, "Failed to set clock %d kHz\n",
			GPU_FREQ_KHZ_MAX);
		return err;
	}

	err = clk_set_parent(mfg->clks[mux].clk, mfg->clks[main].clk);
	if (err) {
		dev_err(kbdev->dev, "Failed to select main clock src\n");
		return err;
	}

#ifdef CONFIG_MALI_VALHALL_DEVFREQ
	kbdev->devfreq_ops.set_frequency = set_frequency;
	kbdev->devfreq_ops.voltage_range_check = voltage_range_check;
#endif

	return 0;
}

static void platform_term(struct kbase_device *kbdev)
{
	kbdev->platform_context = NULL;
	pm_domain_term(kbdev);
}

struct kbase_platform_funcs_conf mt8195_platform_funcs = {
	.platform_init_func = platform_init,
	.platform_term_func = platform_term
};
