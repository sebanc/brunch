/*
 * (C) COPYRIGHT 2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 */

#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>

#include <linux/pm_runtime.h>
#include <linux/suspend.h>

#include "mali_kbase_rk.h"

static int kbase_pm_notifier(struct notifier_block *nb, unsigned long action,
		void *data)
{
	struct rk_context *platform = container_of(nb, struct rk_context, pm_nb);
	struct device *dev = platform->kbdev->dev;

	switch (action) {
	case PM_SUSPEND_PREPARE:
		return pm_runtime_get_sync(dev);
	case PM_POST_SUSPEND:
		return pm_runtime_put(dev);
	}

	return 0;
}

static int kbase_platform_rk_init(struct kbase_device *kbdev)
{
	struct rk_context *platform;

	platform = kzalloc(sizeof(*platform), GFP_KERNEL);
	if (!platform)
		return -ENOMEM;

	platform->is_powered = false;

	kbdev->platform_context = platform;
	platform->kbdev = kbdev;

	return 0;
}

static void kbase_platform_rk_term(struct kbase_device *kbdev)
{
	struct rk_context *platform = kbdev->platform_context;

	kfree(platform);
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_rk_init,
	.platform_term_func = &kbase_platform_rk_term,
};

/* TODO */
static int rk_pm_callback_runtime_on(struct kbase_device *kbdev)
{
	return 0;
}

static void rk_pm_callback_runtime_off(struct kbase_device *kbdev)
{
}

static int rk_pm_enable_regulator(struct kbase_device *kbdev)
{
	int ret = 0;

	if (!(kbdev->regulator)) {
		dev_dbg(kbdev->dev, "Continuing without Mali regulator control\n");
		/* Allow probe to continue without regulator */
		return ret;
	}

	dev_dbg(kbdev->dev, "Enabling regulator.");

	ret = regulator_enable(kbdev->regulator);
	if (ret)
		dev_err(kbdev->dev, "fail to enable regulator, ret=%d\n", ret);

	return ret;
}

static void rk_pm_disable_regulator(struct kbase_device *kbdev)
{
	if (!(kbdev->regulator)) {
		dev_dbg(kbdev->dev, "Continuing without Mali regulator control\n");
		/* Allow probe to continue without regulator */
		return;
	}

	regulator_disable(kbdev->regulator);
}

static int rk_pm_enable_clk(struct kbase_device *kbdev)
{
	int ret = 0;

	if (!(kbdev->clock)) {
		dev_dbg(kbdev->dev, "Continuing without Mali clock control\n");
		/* Allow probe to continue without clock. */
	} else {
		ret = clk_enable(kbdev->clock);
		if (ret)
			dev_err(kbdev->dev, "failed to enable clk: %d\n", ret);
	}

	return ret;
}

static void rk_pm_disable_clk(struct kbase_device *kbdev)
{
	if (!(kbdev->clock))
		dev_dbg(kbdev->dev, "Continuing without Mali clock control\n");
		/* Allow probe to continue without clock. */
	else
		clk_disable(kbdev->clock);
}

static int rk_pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret = 1; /* Assume GPU has been powered off */
	int err = 0;
	struct rk_context *platform;

	platform = kbdev->platform_context;
	if (platform->is_powered) {
		dev_dbg(kbdev->dev, "mali_device is already powered\n");
		return 0;
	}

	dev_dbg(kbdev->dev, "powering on.");

	/* we must enable vdd_gpu before pd_gpu_in_chip. */
	err = rk_pm_enable_regulator(kbdev);
	if (err) {
		dev_err(kbdev->dev, "fail to enable regulator, err=%d\n", err);
		return err;
	}

	err = pm_runtime_get_sync(kbdev->dev);
	if (err < 0 && err != -EACCES) {
		dev_err(kbdev->dev,
			"failed to runtime resume device: %d\n", err);
		pm_runtime_put_sync(kbdev->dev);
		rk_pm_disable_regulator(kbdev);
		return err;
	} else if (err == 1) {
		/*
		 * Let core know that the chip has not been
		 * powered off, so we can save on re-initialization.
		 */
		ret = 0;
	}

	err = rk_pm_enable_clk(kbdev); /* clk is not relative to pd */
	if (err) {
		dev_err(kbdev->dev, "failed to enable clk: %d\n", err);
		pm_runtime_put_sync(kbdev->dev);
		rk_pm_disable_clk(kbdev);
		return err;
	}

	platform->is_powered = true;

	return ret;
}

static void rk_pm_callback_power_off(struct kbase_device *kbdev)
{
	struct rk_context *platform = kbdev->platform_context;

	if (!platform->is_powered) {
		dev_dbg(kbdev->dev, "mali_dev is already powered off\n");
		return;
	}

	dev_dbg(kbdev->dev, "powering off\n");

	platform->is_powered = false;

	rk_pm_disable_clk(kbdev);

	pm_runtime_mark_last_busy(kbdev->dev);
	pm_runtime_put_autosuspend(kbdev->dev);

	rk_pm_disable_regulator(kbdev);
}

int rk_kbase_device_runtime_init(struct kbase_device *kbdev)
{
	struct rk_context *platform = kbdev->platform_context;
	int err;

	pm_runtime_set_autosuspend_delay(kbdev->dev, 200);
	pm_runtime_use_autosuspend(kbdev->dev);

	platform->pm_nb.notifier_call = kbase_pm_notifier;
	platform->pm_nb.priority = 0;
	err = register_pm_notifier(&platform->pm_nb);
	if (err) {
		dev_err(kbdev->dev, "Couldn't register pm notifier\n");
		return -ENODEV;
	}

	/* no need to call pm_runtime_set_active here. */
	pm_runtime_enable(kbdev->dev);

	return 0;
}

void rk_kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	struct rk_context *platform = kbdev->platform_context;

	pm_runtime_disable(kbdev->dev);
	unregister_pm_notifier(&platform->pm_nb);
}

static void rk_pm_suspend_callback(struct kbase_device *kbdev)
{
	/*
	 * Depending on power policy, the GPU might not be powered off at this
	 * point. We have to call rk_pm_callback_power_off() here to make
	 * sure it is before turning off the regulator. The function can be
	 * called safely even if the GPU is already powered off.
	 */
	rk_pm_callback_power_off(kbdev);
	rk_pm_disable_regulator(kbdev);
}

static void rk_pm_resume_callback(struct kbase_device *kbdev)
{
	rk_pm_enable_regulator(kbdev);
	/*
	 * Core will call rk_pm_enable_regulator() itself before attempting
	 * to access the GPU, so no need to do it here.
	 */
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = rk_pm_callback_power_on,
	.power_off_callback = rk_pm_callback_power_off,
	.power_suspend_callback = rk_pm_suspend_callback,
	.power_resume_callback = rk_pm_resume_callback,
#ifdef CONFIG_PM
	.power_runtime_init_callback = rk_kbase_device_runtime_init,
	.power_runtime_term_callback = rk_kbase_device_runtime_disable,
	.power_runtime_on_callback = rk_pm_callback_runtime_on,
	.power_runtime_off_callback = rk_pm_callback_runtime_off,
#else				/* CONFIG_PM */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* CONFIG_PM */
};

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}
