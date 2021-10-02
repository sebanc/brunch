// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for throttling triggered by events from the Chrome OS Embedded
 * Controller.
 *
 * Copyright (C) 2018 Google, Inc.
 */

#include <linux/kernel.h>
#include <linux/mfd/cros_ec.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/throttler.h>

#define nb_to_ce_thr(nb) container_of(nb, struct cros_ec_throttler, nb)

struct cros_ec_throttler {
	struct cros_ec_device *ec;
	struct throttler *throttler;
	struct notifier_block nb;
};

static int cros_ec_throttler_event(struct notifier_block *nb,
	unsigned long queued_during_suspend, void *_notify)
{
	struct cros_ec_throttler *ce_thr = nb_to_ce_thr(nb);
	u32 host_event;

	host_event = cros_ec_get_host_event(ce_thr->ec);
	if (host_event & EC_HOST_EVENT_MASK(EC_HOST_EVENT_THROTTLE_START)) {
		throttler_set_level(ce_thr->throttler, 1);

		return NOTIFY_OK;
	} else if (host_event &
		   EC_HOST_EVENT_MASK(EC_HOST_EVENT_THROTTLE_STOP)) {
		throttler_set_level(ce_thr->throttler, 0);

		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static int cros_ec_throttler_probe(struct platform_device *pdev)
{
	struct cros_ec_throttler *ce_thr;
	struct device *dev = &pdev->dev;
	struct cros_ec_dev *ec;
	int ret;

	ce_thr = devm_kzalloc(dev, sizeof(*ce_thr), GFP_KERNEL);
	if (!ce_thr)
		return -ENOMEM;

	ec = dev_get_drvdata(pdev->dev.parent);
	ce_thr->ec = ec->ec_dev;

	/*
	 * The core code uses the DT node of the throttler to identify its
	 * throttling devices and rates. The CrOS EC throttler is a sub-device
	 * of the CrOS EC MFD device and doesn't have its own device node. Use
	 * the node of the MFD device instead.
	 */
	dev->of_node = ce_thr->ec->dev->of_node;

	ce_thr->throttler = throttler_setup(dev);
	if (IS_ERR(ce_thr->throttler))
		return PTR_ERR(ce_thr->throttler);

	dev_set_drvdata(dev, ce_thr);

	ce_thr->nb.notifier_call = cros_ec_throttler_event;
	ret = blocking_notifier_chain_register(&ce_thr->ec->event_notifier,
					       &ce_thr->nb);
	if (ret < 0) {
		dev_err(dev, "failed to register notifier\n");
		throttler_teardown(ce_thr->throttler);
		return ret;
	}

	return 0;
}

static int cros_ec_throttler_remove(struct platform_device *pdev)
{
	struct cros_ec_throttler *ce_thr = platform_get_drvdata(pdev);

	blocking_notifier_chain_unregister(&ce_thr->ec->event_notifier,
					   &ce_thr->nb);

	throttler_teardown(ce_thr->throttler);

	return 0;
}

static struct platform_driver cros_ec_throttler_driver = {
	.driver = {
		.name = "cros-ec-throttler",
	},
	.probe		= cros_ec_throttler_probe,
	.remove		= cros_ec_throttler_remove,
};

module_platform_driver(cros_ec_throttler_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Matthias Kaehlcke <mka@chromium.org>");
MODULE_DESCRIPTION("Chrome OS EC Throttler");
