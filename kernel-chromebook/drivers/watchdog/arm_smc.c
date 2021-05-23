/*
 * ARM Secure Monitor Call watchdog driver
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 *
 * Julius Werner <jwerner@chromium.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on mtk_wdt.c
 */

#include <linux/arm-smccc.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <uapi/linux/psci.h>

#define DRV_NAME		"smc-watchdog"
#define DRV_VERSION		"1.0"

/* TODO: find a better Function ID if we ever want to upstream this. */
#define SMCWD_FUNC_ID		0x82003d06

enum smcwd_call {
	SMCWD_INFO		= 0,
	SMCWD_SET_TIMEOUT	= 1,
	SMCWD_ENABLE		= 2,
	SMCWD_PET		= 3,
};

static bool nowayout = WATCHDOG_NOWAYOUT;
static unsigned int timeout;

static int smcwd_call(enum smcwd_call call, unsigned long arg,
		      struct arm_smccc_res *res)
{
	struct arm_smccc_res local_res;

	if (!res)
		res = &local_res;

	arm_smccc_smc(SMCWD_FUNC_ID, call, arg, 0, 0, 0, 0, 0, res);

	if ((int)res->a0 == PSCI_RET_NOT_SUPPORTED)
		return -ENOTSUPP;
	if ((int)res->a0 == PSCI_RET_INVALID_PARAMS)
		return -EINVAL;
	if ((int)res->a0 < 0)
		return -EIO;
	return res->a0;
}

static int smcwd_ping(struct watchdog_device *wdd)
{
	return smcwd_call(SMCWD_PET, 0, NULL);
}

static int smcwd_set_timeout(struct watchdog_device *wdd,
				unsigned int timeout)
{
	return smcwd_call(SMCWD_SET_TIMEOUT, timeout, NULL);
}

static int smcwd_stop(struct watchdog_device *wdd)
{
	return smcwd_call(SMCWD_ENABLE, 0, NULL);
}

static int smcwd_start(struct watchdog_device *wdd)
{
	return smcwd_call(SMCWD_ENABLE, 1, NULL);
}

static const struct watchdog_info smcwd_info = {
	.identity	= DRV_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops smcwd_ops = {
	.owner		= THIS_MODULE,
	.start		= smcwd_start,
	.stop		= smcwd_stop,
	.ping		= smcwd_ping,
	.set_timeout	= smcwd_set_timeout,
};

static int smcwd_probe(struct platform_device *pdev)
{
	struct watchdog_device *wdd;
	int err;
	struct arm_smccc_res res;

	err = smcwd_call(SMCWD_INFO, 0, &res);
	if (err < 0)
		return err;

	wdd = devm_kzalloc(&pdev->dev, sizeof(*wdd), GFP_KERNEL);
	if (!wdd)
		return -ENOMEM;

	platform_set_drvdata(pdev, wdd);

	wdd->info = &smcwd_info;
	wdd->ops = &smcwd_ops;
	wdd->timeout = res.a2;
	wdd->max_timeout = res.a2;
	wdd->min_timeout = res.a1;
	wdd->parent = &pdev->dev;

	watchdog_set_nowayout(wdd, nowayout);
	watchdog_init_timeout(wdd, timeout, &pdev->dev);
	err = smcwd_set_timeout(wdd, wdd->timeout);
	if (err)
		return err;

	err = watchdog_register_device(wdd);
	if (err)
		return err;

	dev_info(&pdev->dev, "Watchdog enabled (timeout=%d sec, nowayout=%d)\n",
			wdd->timeout, nowayout);

	return 0;
}

static void smcwd_shutdown(struct platform_device *pdev)
{
	struct watchdog_device *wdd = platform_get_drvdata(pdev);

	if (watchdog_active(wdd))
		smcwd_stop(wdd);
}

static int smcwd_remove(struct platform_device *pdev)
{
	struct watchdog_device *wdd = platform_get_drvdata(pdev);

	watchdog_unregister_device(wdd);

	return 0;
}

static const struct of_device_id smcwd_dt_ids[] = {
	{ .compatible = "arm,smc-watchdog" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, smcwd_dt_ids);

static struct platform_driver smcwd_driver = {
	.probe		= smcwd_probe,
	.remove		= smcwd_remove,
	.shutdown	= smcwd_shutdown,
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= smcwd_dt_ids,
	},
};

module_platform_driver(smcwd_driver);

module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Julius Werner <jwerner@chromium.org>");
MODULE_DESCRIPTION("ARM Secure Monitor Call Watchdog Driver");
MODULE_VERSION(DRV_VERSION);
