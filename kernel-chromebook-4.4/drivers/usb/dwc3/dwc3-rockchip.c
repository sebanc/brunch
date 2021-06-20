/**
 * dwc3-rockchip.c - Rockchip Specific Glue layer
 *
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 *
 * Authors: William Wu <william.wu@rock-chips.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/extcon.h>
#include <linux/freezer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/usb.h>

struct dwc3_rockchip {
	int			num_clocks;
	bool			populated;
	bool			suspended;
	struct device		*dev;
	struct clk		**clks;
	struct reset_control	*otg_rst;
	struct extcon_dev	*edev;
	struct notifier_block	host_nb;
	struct work_struct	otg_work;
	struct mutex		lock;
};

static int dwc3_rockchip_of_populate(struct dwc3_rockchip *rockchip)
{
	struct device		*dev = rockchip->dev;
	struct device_node	*np = dev->of_node;
	int			ret;

	/* Allocate and initialize the core */
	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed to populate dwc3 core\n");
		return ret;
	}
	rockchip->populated = true;
	return 0;
}

static void dwc3_rockchip_of_depopulate(struct dwc3_rockchip *rockchip)
{
	if (rockchip->populated) {
		of_platform_depopulate(rockchip->dev);
		rockchip->populated = false;
	}
}

static int dwc3_rockchip_host_notifier(struct notifier_block *nb,
				       unsigned long event, void *ptr)
{
	struct dwc3_rockchip *rockchip =
		container_of(nb, struct dwc3_rockchip, host_nb);

	if (!rockchip->suspended)
		schedule_work(&rockchip->otg_work);

	return NOTIFY_DONE;
}

static void dwc3_rockchip_otg_extcon_evt_work(struct work_struct *work)
{
	struct dwc3_rockchip	*rockchip =
		container_of(work, struct dwc3_rockchip, otg_work);
	struct extcon_dev	*edev = rockchip->edev;
	int err;

	mutex_lock(&rockchip->lock);

	if (extcon_get_cable_state_(edev, EXTCON_USB_HOST) > 0) {
		if (rockchip->populated)
			goto out;

		/*
		 * We have to reset the OTG after the USB cable has been
		 * inserted before we can initialize it.
		 * Without this reset, USB3 devices connected to various
		 * docking stations such as Anker Premium USB-C hub
		 * may not be discovered at all.
		 *
		 * Give the port some time to settle before resetting it.
		 * Non-scientific experiments suggest that this additional
		 * delay helps to avoid 'port not accepting address' errors
		 * (error -71). Note that a delay of 50 mS was found to be
		 * insufficient.
		 */
		msleep(100);
		reset_control_assert(rockchip->otg_rst);
		udelay(1);
		reset_control_deassert(rockchip->otg_rst);

		/*
		 * The following sleep helps to ensure that some special
		 * USB3 devices are discovered.
		 * 1. USB3 Ethernet devices which are already inserted
		 *    when booting need to sleep at least 10ms.
		 * 2. USB3 Apple USB-C Dock with USB3 Transcend Flash
		 *    Drive 16GB already inserted need to sleep at least
		 *    300ms to ensure that it is discovered successfully.
		 */
		msleep(300);

		/* Don't abort on errors. Just report it. */
		err = dwc3_rockchip_of_populate(rockchip);
		if (err)
			dev_err(rockchip->dev,
				"Failed to connect USB devices: %d\n", err);
		else
			dev_info(rockchip->dev, "USB HOST connected\n");
	} else {
		if (!rockchip->populated)
			goto out;

#ifdef CONFIG_FREEZER
		/*
		 * usb_remove_hcd() may call usb_disconnect() to remove
		 * a block device plugged in before. Unfortunately, the
		 * block layer suspend/resume path is fundamentally
		 * broken due to freezable kthreads and workqueue and
		 * may deadlock if a block device gets removed while
		 * resume is in progress.
		 *
		 * So we need to add a ugly hack to avoid removing
		 * hcd and kicking off device removal while freezer
		 * is active. This is a joke but does avoid this
		 * particular deadlock when test with USB-C HUB and
		 * USB2/3 flash drive.
		 */
		while (pm_freezing)
			usleep_range(10000, 20000);
#endif

		dwc3_rockchip_of_depopulate(rockchip);
		dev_info(rockchip->dev, "USB HOST disconnected\n");
	}
out:
	mutex_unlock(&rockchip->lock);
}

static int dwc3_rockchip_extcon_register(struct dwc3_rockchip *rockchip)
{
	int			ret;
	struct device		*dev = rockchip->dev;
	struct extcon_dev	*edev;

	if (device_property_read_bool(dev, "extcon")) {
		edev = extcon_get_edev_by_phandle(dev, 0);
		if (IS_ERR(edev)) {
			if (PTR_ERR(edev) != -EPROBE_DEFER)
				dev_err(dev, "couldn't get extcon device\n");
			return PTR_ERR(edev);
		}

		INIT_WORK(&rockchip->otg_work,
			  dwc3_rockchip_otg_extcon_evt_work);

		rockchip->host_nb.notifier_call =
				dwc3_rockchip_host_notifier;
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
					       &rockchip->host_nb);
		if (ret < 0) {
			dev_err(dev, "failed to register notifier for USB HOST\n");
			return ret;
		}

		rockchip->edev = edev;
	}

	return 0;
}

static void dwc3_rockchip_extcon_unregister(struct dwc3_rockchip *rockchip)
{
	if (!rockchip->edev)
		return;

	extcon_unregister_notifier(rockchip->edev, EXTCON_USB_HOST,
				   &rockchip->host_nb);
	cancel_work_sync(&rockchip->otg_work);
}

static int dwc3_rockchip_clk_init(struct dwc3_rockchip *rockchip, int count)
{
	struct device		*dev = rockchip->dev;
	struct device_node	*np = dev->of_node;
	int			get_idx, prep_idx;
	int			ret;

	rockchip->num_clocks = count;

	if (!count)
		return 0;

	rockchip->clks = devm_kcalloc(dev, rockchip->num_clocks,
				      sizeof(struct clk *), GFP_KERNEL);
	if (!rockchip->clks)
		return -ENOMEM;

	for (get_idx = 0; get_idx < rockchip->num_clocks; get_idx++) {
		struct clk *clk;

		clk = of_clk_get(np, get_idx);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto err0;
		}
		rockchip->clks[get_idx] = clk;
	}

	for (prep_idx = 0; prep_idx < rockchip->num_clocks; prep_idx++) {
		ret = clk_prepare_enable(rockchip->clks[prep_idx]);
		if (ret < 0)
			goto err1;
	}
	return 0;

err1:
	while (--prep_idx >= 0)
		clk_disable_unprepare(rockchip->clks[prep_idx]);
err0:
	while (--get_idx >= 0)
		clk_put(rockchip->clks[get_idx]);
	return ret;
}

static int dwc3_rockchip_probe(struct platform_device *pdev)
{
	struct dwc3_rockchip	*rockchip;
	struct device		*dev = &pdev->dev;
	struct device_node	*np = dev->of_node;
	int			ret;
	int			i;

	rockchip = devm_kzalloc(dev, sizeof(*rockchip), GFP_KERNEL);
	if (!rockchip)
		return -ENOMEM;

	mutex_init(&rockchip->lock);

	rockchip->otg_rst = devm_reset_control_get(dev, "usb3-otg");
	if (IS_ERR(rockchip->otg_rst)) {
		if (PTR_ERR(rockchip->otg_rst) != -EPROBE_DEFER)
			dev_err(dev, "could not get reset controller\n");
		return PTR_ERR(rockchip->otg_rst);
	}

	platform_set_drvdata(pdev, rockchip);
	rockchip->dev = dev;

	ret = dwc3_rockchip_clk_init(rockchip, of_clk_get_parent_count(np));
	if (ret)
		return ret;

	mutex_lock(&rockchip->lock);

	ret = dwc3_rockchip_extcon_register(rockchip);
	if (ret < 0)
		goto err1;

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	if (!rockchip->edev) {
		ret = dwc3_rockchip_of_populate(rockchip);
		if (ret)
			goto err2;
	} else {
		/*
		 * Populate only if cable is connected.
		 * Populate through worker to avoid race conditions against
		 * action scheduled through notifier.
		 */
		if (extcon_get_cable_state_(rockchip->edev,
					    EXTCON_USB_HOST) > 0)
			schedule_work(&rockchip->otg_work);
	}

	device_enable_async_suspend(dev);

	mutex_unlock(&rockchip->lock);
	return 0;

err2:
	pm_runtime_disable(dev);
err1:
	if (ret != -EPROBE_DEFER)
		dev_err(rockchip->dev, "Bailing out, error %d\n", ret);

	for (i = 0; i < rockchip->num_clocks; i++) {
		clk_disable_unprepare(rockchip->clks[i]);
		clk_put(rockchip->clks[i]);
	}

	mutex_unlock(&rockchip->lock);
	return ret;
}

static int dwc3_rockchip_remove(struct platform_device *pdev)
{
	struct dwc3_rockchip	*rockchip = platform_get_drvdata(pdev);
	struct device		*dev = &pdev->dev;
	int			i;

	dwc3_rockchip_extcon_unregister(rockchip);
	dwc3_rockchip_of_depopulate(rockchip);

	pm_runtime_disable(dev);

	for (i = 0; i < rockchip->num_clocks; i++) {
		if (!pm_runtime_status_suspended(dev))
			clk_disable(rockchip->clks[i]);
		clk_unprepare(rockchip->clks[i]);
		clk_put(rockchip->clks[i]);
	}

	return 0;
}

#ifdef CONFIG_PM
static int dwc3_rockchip_runtime_suspend(struct device *dev)
{
	struct dwc3_rockchip	*rockchip = dev_get_drvdata(dev);
	int			i;

	for (i = 0; i < rockchip->num_clocks; i++)
		clk_disable(rockchip->clks[i]);

	return 0;
}

static int dwc3_rockchip_runtime_resume(struct device *dev)
{
	struct dwc3_rockchip	*rockchip = dev_get_drvdata(dev);
	int			ret;
	int			i;

	for (i = 0; i < rockchip->num_clocks; i++) {
		ret = clk_enable(rockchip->clks[i]);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	while (--i >= 0)
		clk_disable(rockchip->clks[i]);
	return ret;
}

static int dwc3_rockchip_suspend(struct device *dev)
{
	struct dwc3_rockchip *rockchip = dev_get_drvdata(dev);

	rockchip->suspended = true;
	cancel_work_sync(&rockchip->otg_work);

	return 0;
}

static int dwc3_rockchip_resume(struct device *dev)
{
	struct dwc3_rockchip *rockchip = dev_get_drvdata(dev);

	reset_control_assert(rockchip->otg_rst);
	udelay(1);
	reset_control_deassert(rockchip->otg_rst);

	rockchip->suspended = false;

	if (rockchip->edev)
		schedule_work(&rockchip->otg_work);

	return 0;
}

static const struct dev_pm_ops dwc3_rockchip_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_rockchip_suspend, dwc3_rockchip_resume)
	SET_RUNTIME_PM_OPS(dwc3_rockchip_runtime_suspend,
			   dwc3_rockchip_runtime_resume, NULL)
};

#define DEV_PM_OPS      (&dwc3_rockchip_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM */

static const struct of_device_id rockchip_dwc3_match[] = {
	{ .compatible = "rockchip,rk3399-dwc3" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, rockchip_dwc3_match);

static struct platform_driver dwc3_rockchip_driver = {
	.probe		= dwc3_rockchip_probe,
	.remove		= dwc3_rockchip_remove,
	.driver		= {
		.name	= "rockchip-dwc3",
		.of_match_table = rockchip_dwc3_match,
		.pm	= DEV_PM_OPS,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

module_platform_driver(dwc3_rockchip_driver);

MODULE_ALIAS("platform:rockchip-dwc3");
MODULE_AUTHOR("William Wu <william.wu@rock-chips.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 ROCKCHIP Glue Layer");
