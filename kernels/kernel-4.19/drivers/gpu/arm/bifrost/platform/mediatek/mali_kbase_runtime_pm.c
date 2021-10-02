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

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <mali_kbase.h>
#include "mali_kbase_config_platform.h"
#include <mali_kbase_defs.h>

static struct platform_device *probe_gpu_core1_dev;
static struct platform_device *probe_gpu_core2_dev;

static const struct of_device_id mtk_gpu_corex_of_ids[] = {
	{ .compatible = "mediatek,gpu_core1", .data = "1" },
	{ .compatible = "mediatek,gpu_core2", .data = "2" },
	{}
};

static int mtk_gpu_corex_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	const char *tmp;

	match = of_match_device(mtk_gpu_corex_of_ids, dev);
	if (!match)
		return -ENODEV;
	tmp = match->data;
	if (*tmp == '1')
		probe_gpu_core1_dev = pdev;
	else
		probe_gpu_core2_dev = pdev;

	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int mtk_gpu_corex_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_gpu_corex_driver = {
	.probe  = mtk_gpu_corex_probe,
	.remove = mtk_gpu_corex_remove,
	.driver = {
		.name = "gpu_corex",
		.of_match_table = mtk_gpu_corex_of_ids,
	}
};

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int error;
	struct mfg_base *mfg = kbdev->platform_context;

	if (mfg->is_powered) {
		dev_dbg(kbdev->dev, "mali_device is already powered\n");
		return 0;
	}

	error = pm_runtime_get_sync(kbdev->dev);
	if (error < 0) {
		dev_err(kbdev->dev,
			"Power on core 0 failed (err: %d)\n", error);
		return error;
	}

	error = pm_runtime_get_sync(&mfg->gpu_core1_dev->dev);
	if (error < 0) {
		dev_err(kbdev->dev,
			"Power on core 1 failed (err: %d)\n", error);
		return error;
	}

	error = pm_runtime_get_sync(&mfg->gpu_core2_dev->dev);
	if (error < 0) {
		dev_err(kbdev->dev,
			"Power on core 2 failed (err: %d)\n", error);
		return error;
	}

	error = clk_enable(mfg->clk_main_parent);
	if (error < 0) {
		dev_err(kbdev->dev,
			"clk_main_parent clock enable failed (err: %d)\n",
			error);
		return error;
	}

	error = clk_enable(mfg->clk_mux);
	if (error < 0) {
		dev_err(kbdev->dev,
			"clk_mux clock enable failed (err: %d)\n", error);
		return error;
	}

	error = clk_enable(mfg->subsys_mfg_cg);
	if (error < 0) {
		dev_err(kbdev->dev,
			"subsys_mfg_cg clock enable failed (err: %d)\n", error);
		return error;
	}

	mfg->is_powered = true;

	return 1;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = kbdev->platform_context;
	int error;

	if (!mfg->is_powered) {
		dev_dbg(kbdev->dev, "mali_device is already powered off\n");
		return;
	}

	mfg->is_powered = false;

	clk_disable(mfg->subsys_mfg_cg);

	clk_disable(mfg->clk_mux);

	clk_disable(mfg->clk_main_parent);

	pm_runtime_mark_last_busy(&mfg->gpu_core2_dev->dev);
	error = pm_runtime_put_autosuspend(&mfg->gpu_core2_dev->dev);
	if (error < 0)
		dev_err(kbdev->dev,
		"Power off core 2 failed (err: %d)\n", error);

	pm_runtime_mark_last_busy(&mfg->gpu_core1_dev->dev);
	error = pm_runtime_put_autosuspend(&mfg->gpu_core1_dev->dev);
	if (error < 0)
		dev_err(kbdev->dev,
		"Power off core 1 failed (err: %d)\n", error);

	pm_runtime_mark_last_busy(kbdev->dev);
	error = pm_runtime_put_autosuspend(kbdev->dev);
	if (error < 0)
		dev_err(kbdev->dev,
		"Power off core 0 failed (err: %d)\n", error);
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
	struct mfg_base *mfg = kbdev->platform_context;
	int error, i;

	if (mfg->reg_is_powered) {
		dev_dbg(kbdev->dev, "GPU regulators are already power on\n");
		return 0;
	}

	for (i = 0; i < kbdev->nr_regulators; i++) {
		error = regulator_enable(kbdev->regulators[i]);
		if (error < 0) {
			dev_err(kbdev->dev,
				"Power on reg %d failed error = %d\n",
				i, error);
			return error;
		}
	}

	error = clk_prepare(mfg->clk_main_parent);
	if (error < 0) {
		dev_err(kbdev->dev,
			"clk_main_parent clock prepare failed (err: %d)\n",
			error);
		return error;
	}

	error = clk_prepare(mfg->clk_mux);
	if (error < 0) {
		dev_err(kbdev->dev,
			"clk_mux clock prepare failed (err: %d)\n", error);
		return error;
	}

	error = clk_prepare(mfg->subsys_mfg_cg);
	if (error < 0) {
		dev_err(kbdev->dev,
			"subsys_mfg_cg clock prepare failed (err: %d)\n",
			error);
		return error;
	}

	mfg->reg_is_powered = true;

	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = kbdev->platform_context;
	int error, i;

	if (!mfg->reg_is_powered) {
		dev_dbg(kbdev->dev, "GPU regulators are already power off\n");
		return;
	}

	clk_unprepare(mfg->subsys_mfg_cg);

	clk_unprepare(mfg->clk_mux);

	clk_unprepare(mfg->clk_main_parent);

	for (i = 0; i < kbdev->nr_regulators; i++) {
		error = regulator_disable(kbdev->regulators[i]);
		if (error < 0) {
			dev_err(kbdev->dev,
				"Power off reg %d failed error = %d\n",
				i, error);
		}
	}

	mfg->reg_is_powered = false;
}

static void pm_callback_resume(struct kbase_device *kbdev)
{
	pm_callback_runtime_on(kbdev);
	pm_callback_power_on(kbdev);
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	pm_callback_power_off(kbdev);
	pm_callback_runtime_off(kbdev);
}

struct kbase_pm_callback_conf pm_callbacks = {
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


int mali_mfgsys_init(struct kbase_device *kbdev, struct mfg_base *mfg)
{
	int err = 0, i;
	unsigned long volt;

	if (!probe_gpu_core1_dev || !probe_gpu_core2_dev)
		return -EPROBE_DEFER;

	for (i = 0; i < kbdev->nr_regulators; i++)
		if (kbdev->regulators[i] == NULL)
			return -EINVAL;

	mfg->gpu_core1_dev = probe_gpu_core1_dev;
	mfg->gpu_core2_dev = probe_gpu_core2_dev;

	mfg->clk_main_parent = devm_clk_get(kbdev->dev, "clk_main_parent");
	if (IS_ERR(mfg->clk_main_parent)) {
		err = PTR_ERR(mfg->clk_main_parent);
		dev_err(kbdev->dev, "devm_clk_get clk_main_parent failed\n");
		return err;
	}

	mfg->clk_sub_parent = devm_clk_get(kbdev->dev, "clk_sub_parent");
	if (IS_ERR(mfg->clk_sub_parent)) {
		err = PTR_ERR(mfg->clk_sub_parent);
		dev_err(kbdev->dev, "devm_clk_get clk_sub_parent failed\n");
		return err;
	}

	mfg->clk_mux = devm_clk_get(kbdev->dev, "clk_mux");
	if (IS_ERR(mfg->clk_mux)) {
		err = PTR_ERR(mfg->clk_mux);
		dev_err(kbdev->dev, "devm_clk_get clk_mux failed\n");
		return err;
	}

	mfg->subsys_mfg_cg = devm_clk_get(kbdev->dev, "subsys_mfg_cg");
	if (IS_ERR(mfg->subsys_mfg_cg)) {
		err = PTR_ERR(mfg->subsys_mfg_cg);
		dev_err(kbdev->dev, "devm_clk_get subsys_mfg_cg failed\n");
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
		kbdev->current_voltages[i] = volt;
	}

	mfg->is_powered = false;
	mfg->reg_is_powered = false;

	return 0;
}

static void voltage_range_check(struct kbase_device *kbdev,
				unsigned long *voltages)
{
	if (voltages[1] - voltages[0] < MIN_VOLT_BIAS ||
	    voltages[1] - voltages[0] > MAX_VOLT_BIAS)
		voltages[1] = voltages[0] + MIN_VOLT_BIAS;
	voltages[1] = clamp_t(unsigned long, voltages[1], VSRAM_GPU_MIN_VOLT,
			      VSRAM_GPU_MAX_VOLT);
}

#ifdef CONFIG_REGULATOR
static bool get_step_volt(unsigned long *step_volt, unsigned long *target_volt,
			  int nr_regulators, bool inc)
{
	int i;

	for (i = 0; i < nr_regulators; i++)
		if (step_volt[i] != target_volt[i])
			break;

	if (i == nr_regulators)
		return false;

	/* Do one round of *caterpillar move* - shrink the tail as much to the
	 * head as possible, and then step ahead as far as possible.
	 * Depending on the direction of voltage transition, a reversed
	 * sequence of extend-and-shrink may apply, which leads to the same
	 * result in the end.
	 */
	if (inc) {
		step_volt[0] = min(target_volt[0],
				   step_volt[1] - MIN_VOLT_BIAS);
		step_volt[1] = min(target_volt[1],
				   step_volt[0] + MAX_VOLT_BIAS);
	} else {
		step_volt[0] = max(target_volt[0],
				   step_volt[1] - MAX_VOLT_BIAS);
		step_volt[1] = max(target_volt[1],
				   step_volt[0] + MIN_VOLT_BIAS);
	}
	return true;
}

static int set_voltages(struct kbase_device *kbdev, unsigned long *voltages,
			bool inc)
{
	unsigned long step_volt[BASE_MAX_NR_CLOCKS_REGULATORS];
	const unsigned long reg_min_volt[BASE_MAX_NR_CLOCKS_REGULATORS] = {
		VGPU_MIN_VOLT,
		VSRAM_GPU_MIN_VOLT,
	};
	const unsigned long reg_max_volt[BASE_MAX_NR_CLOCKS_REGULATORS] = {
		VGPU_MAX_VOLT,
		VSRAM_GPU_MAX_VOLT,
	};
	int i, err;

	/* Nothing to do if the direction of voltage transition is incorrect. */
	if ((inc && kbdev->current_voltages[0] > voltages[0]) ||
	    (!inc && kbdev->current_voltages[0] < voltages[0]))
		return 0;

	for (i = 0; i < kbdev->nr_regulators; ++i)
		step_volt[i] = kbdev->current_voltages[i];

	while (get_step_volt(step_volt, voltages, kbdev->nr_regulators, inc)) {
		for (i = 0; i < kbdev->nr_regulators; i++) {
			if (kbdev->current_voltages[i] == step_volt[i])
				continue;

			/* Assuming valid max voltages are always positive. */
			if (reg_max_volt[i] > 0 &&
			    (step_volt[i] < reg_min_volt[i] ||
			     step_volt[i] > reg_max_volt[i])) {
				dev_warn(kbdev->dev, "Clamp invalid voltage: "
					 "%lu of regulator %d into [%lu, %lu]",
					 step_volt[i], i,
					 reg_min_volt[i],
					 reg_max_volt[i]);

				step_volt[i] = clamp_val(step_volt[i],
							 reg_min_volt[i],
							 reg_max_volt[i]);
			}

			err = regulator_set_voltage(kbdev->regulators[i],
						    step_volt[i],
						    step_volt[i] + VOLT_TOL);

			if (err) {
				dev_err(kbdev->dev,
					"Failed to set reg %d voltage err:(%d)\n",
					i, err);
				return err;
			}
			kbdev->current_voltages[i] = step_volt[i];
		}
	}

	return 0;
}
#endif

static int set_frequency(struct kbase_device *kbdev, unsigned long freq)
{
	int err, i;
	struct mfg_base *mfg = kbdev->platform_context;

	if (kbdev->current_freqs[0] != freq) {
		err = clk_set_parent(mfg->clk_mux, mfg->clk_sub_parent);
		if (err) {
			dev_err(kbdev->dev, "Failed to select sub clock src\n");
			return err;
		}

		for (i = 0; i < kbdev->nr_clocks; i++)
			if (kbdev->clocks[i]) {
				err = clk_set_rate(kbdev->clocks[i], freq);
				if (err) {
					dev_err(kbdev->dev,
						"Failed to set clock rate: %lu (err: %d)\n",
						freq, err);
					return err;
				}
				kbdev->current_freqs[i] = freq;
			}

		err = clk_set_parent(mfg->clk_mux, mfg->clk_main_parent);
		if (err) {
			dev_err(kbdev->dev,
				"Failed to select main clock src\n");
			return err;
		}
	}

	return 0;
}

static int platform_init(struct kbase_device *kbdev)
{
	int err;
	struct mfg_base *mfg;

	mfg = kzalloc(sizeof(*mfg), GFP_KERNEL);
	if (!mfg)
		return -ENOMEM;

	err = mali_mfgsys_init(kbdev, mfg);
	if (err)
		goto platform_init_err;

	kbdev->platform_context = mfg;
	pm_runtime_set_autosuspend_delay(kbdev->dev, 50);
	pm_runtime_use_autosuspend(kbdev->dev);
	pm_runtime_enable(kbdev->dev);

	err = clk_set_parent(mfg->clk_mux, mfg->clk_sub_parent);
	if (err) {
		dev_err(kbdev->dev, "Failed to select sub clock src\n");
		goto platform_init_err;
	}

	err = clk_set_rate(mfg->clk_main_parent, GPU_FREQ_KHZ_MAX * 1000);
	if (err) {
		dev_err(kbdev->dev, "Failed to set clock %d kHz\n",
				GPU_FREQ_KHZ_MAX);
		goto platform_init_err;
	}

	err = clk_set_parent(mfg->clk_mux, mfg->clk_main_parent);
	if (err) {
		dev_err(kbdev->dev, "Failed to select main clock src\n");
		goto platform_init_err;
	}

	kbdev->devfreq_ops.set_frequency = set_frequency;
#ifdef CONFIG_REGULATOR
	kbdev->devfreq_ops.set_voltages = set_voltages;
#endif
	kbdev->devfreq_ops.voltage_range_check = voltage_range_check;

	return 0;

platform_init_err:
	kfree(mfg);
	return err;
}

static void platform_term(struct kbase_device *kbdev)
{
	struct mfg_base *mfg = kbdev->platform_context;

	kfree(mfg);
	kbdev->platform_context = NULL;
	pm_runtime_disable(kbdev->dev);
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = platform_init,
	.platform_term_func = platform_term
};

static int __init mtk_mfg_corex(void)
{
	int ret;

	ret = platform_driver_register(&mtk_gpu_corex_driver);
	if (ret != 0)
		pr_debug("%s: Failed to register GPU core driver", __func__);

	return ret;
}

subsys_initcall(mtk_mfg_corex);
