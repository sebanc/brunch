// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Driver for onboard USB hubs
 *
 * Copyright (c) 2020, Google LLC
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/suspend.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

/************************** Platform driver **************************/

struct udev_node {
	struct usb_device *udev;
	struct list_head list;
};

struct onboard_hub {
	struct regulator *vdd;
	struct device *dev;
	bool always_powered_in_suspend;
	bool is_powered_on;
	bool going_away;
	struct list_head udev_list;
	struct mutex lock;
};

static int onboard_hub_power_on(struct onboard_hub *hub)
{
	int err;

	err = regulator_enable(hub->vdd);
	if (err) {
		dev_err(hub->dev, "failed to enable regulator: %d\n", err);
		return err;
	}

	hub->is_powered_on = true;

	return 0;
}

static int onboard_hub_power_off(struct onboard_hub *hub)
{
	int err;

	err = regulator_disable(hub->vdd);
	if (err) {
		dev_err(hub->dev, "failed to disable regulator: %d\n", err);
		return err;
	}

	hub->is_powered_on = false;

	return 0;
}

static int __maybe_unused onboard_hub_suspend(struct device *dev)
{
	struct onboard_hub *hub = dev_get_drvdata(dev);
	struct udev_node *node;
	bool power_off;
	int rc = 0;

	if (hub->always_powered_in_suspend)
		return 0;

	power_off = true;

	mutex_lock(&hub->lock);

	list_for_each_entry(node, &hub->udev_list, list) {
		if (!device_may_wakeup(node->udev->bus->controller))
			continue;

		if (usb_wakeup_enabled_descendants(node->udev)) {
			power_off = false;
			break;
		}
	}

	mutex_unlock(&hub->lock);

	if (power_off)
		rc = onboard_hub_power_off(hub);

	return rc;
}

static int __maybe_unused onboard_hub_resume(struct device *dev)
{
	struct onboard_hub *hub = dev_get_drvdata(dev);
	int rc = 0;

	if (!hub->is_powered_on)
		rc = onboard_hub_power_on(hub);

	return rc;
}

static int onboard_hub_add_usbdev(struct onboard_hub *hub, struct usb_device *udev)
{
	struct udev_node *node;
	int ret = 0;

	mutex_lock(&hub->lock);

	if (hub->going_away) {
		ret = -EINVAL;
		goto unlock;
	}

	node = devm_kzalloc(hub->dev, sizeof(*node), GFP_KERNEL);
	if (!node) {
		ret = -ENOMEM;
		goto unlock;
	}

	node->udev = udev;

	list_add(&node->list, &hub->udev_list);

unlock:
	mutex_unlock(&hub->lock);

	return ret;
}

static void onboard_hub_remove_usbdev(struct onboard_hub *hub, struct usb_device *udev)
{
	struct udev_node *node;

	mutex_lock(&hub->lock);

	list_for_each_entry(node, &hub->udev_list, list) {
		if (node->udev == udev) {
			list_del(&node->list);
			break;
		}
	}

	mutex_unlock(&hub->lock);
}

static ssize_t always_powered_in_suspend_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct onboard_hub *hub = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", hub->always_powered_in_suspend);
}

static ssize_t always_powered_in_suspend_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct onboard_hub *hub = dev_get_drvdata(dev);
	bool val;
	int ret;

	ret = kstrtobool(buf, &val);
	if (ret < 0)
		return ret;

	hub->always_powered_in_suspend = val;

	return count;
}
static DEVICE_ATTR_RW(always_powered_in_suspend);

static struct attribute *onboard_hub_sysfs_entries[] = {
	&dev_attr_always_powered_in_suspend.attr,
	NULL,
};

static const struct attribute_group onboard_hub_sysfs_group = {
	.attrs = onboard_hub_sysfs_entries,
};

static int onboard_hub_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct onboard_hub *hub;
	int err;

	hub = devm_kzalloc(dev, sizeof(*hub), GFP_KERNEL);
	if (!hub)
		return -ENOMEM;

	hub->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(hub->vdd))
		return PTR_ERR(hub->vdd);

	hub->dev = dev;
	mutex_init(&hub->lock);
	INIT_LIST_HEAD(&hub->udev_list);

	dev_set_drvdata(dev, hub);

	err = devm_device_add_group(dev, &onboard_hub_sysfs_group);
	if (err) {
		dev_err(dev, "failed to create sysfs entries: %d\n", err);
		return err;
	}

	return onboard_hub_power_on(hub);
}

static int onboard_hub_remove(struct platform_device *pdev)
{
	struct onboard_hub *hub = dev_get_drvdata(&pdev->dev);
	struct udev_node *node;
	struct usb_device *udev;

	hub->going_away = true;

	mutex_lock(&hub->lock);

	/* unbind the USB devices to avoid dangling references to this device */
	while (!list_empty(&hub->udev_list)) {
		node = list_first_entry(&hub->udev_list, struct udev_node, list);
		udev = node->udev;

		/*
		 * Unbinding the driver will call onboard_hub_remove_usbdev(),
		 * which acquires hub->lock.  We must release the lock first.
		 */
		get_device(&udev->dev);
		mutex_unlock(&hub->lock);
		device_release_driver(&udev->dev);
		put_device(&udev->dev);
		mutex_lock(&hub->lock);
	}

	mutex_unlock(&hub->lock);

	return onboard_hub_power_off(hub);
}

static const struct of_device_id onboard_hub_match[] = {
	{ .compatible = "onboard-usb-hub" },
	{}
};
MODULE_DEVICE_TABLE(of, onboard_hub_match);

static const struct dev_pm_ops __maybe_unused onboard_hub_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(onboard_hub_suspend, onboard_hub_resume)
};

static struct platform_driver onboard_hub_driver = {
	.probe = onboard_hub_probe,
	.remove = onboard_hub_remove,

	.driver = {
		.name = "onboard-usb-hub",
		.of_match_table = onboard_hub_match,
		.pm = pm_ptr(&onboard_hub_pm_ops),
	},
};

/************************** USB driver **************************/

#define VENDOR_ID_REALTEK	0x0bda

static struct onboard_hub *_find_onboard_hub(struct device *dev)
{
	phandle ph;
	struct device_node *np;
	struct platform_device *pdev;

	if (of_property_read_u32(dev->of_node, "hub", &ph)) {
		dev_err(dev, "failed to read 'hub' property\n");
		return ERR_PTR(-EINVAL);
	}

	np = of_find_node_by_phandle(ph);
	if (!np) {
		dev_err(dev, "failed find device node for onboard hub\n");
		return ERR_PTR(-EINVAL);
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return ERR_PTR(-EPROBE_DEFER);

	put_device(&pdev->dev);

	return dev_get_drvdata(&pdev->dev);
}

static int onboard_hub_usbdev_probe(struct usb_device *udev)
{
	struct device *dev = &udev->dev;
	struct onboard_hub *hub;

	/* ignore supported hubs without device tree node */
	if (!dev->of_node)
		return -ENODEV;

	hub = _find_onboard_hub(dev);
	if (IS_ERR(hub))
		return PTR_ERR(hub);

	dev_set_drvdata(dev, hub);

	return onboard_hub_add_usbdev(hub, udev);
}

static void onboard_hub_usbdev_disconnect(struct usb_device *udev)
{
	struct onboard_hub *hub = dev_get_drvdata(&udev->dev);

	onboard_hub_remove_usbdev(hub, udev);
}

static const struct usb_device_id onboard_hub_id_table[] = {
	{ USB_DEVICE(VENDOR_ID_REALTEK, 0x0411) }, /* RTS5411 USB 3.0 */
	{ USB_DEVICE(VENDOR_ID_REALTEK, 0x5411) }, /* RTS5411 USB 2.0 */
	{},
};

MODULE_DEVICE_TABLE(usb, onboard_hub_id_table);

static struct usb_device_driver onboard_hub_usbdev_driver = {

	.name = "onboard-usb-hub",
	.probe = onboard_hub_usbdev_probe,
	.disconnect = onboard_hub_usbdev_disconnect,
	.generic_subclass = 1,
	.supports_autosuspend =	1,
	.id_table = onboard_hub_id_table,
};

/************************** Driver (de)registration **************************/

static int __init onboard_hub_init(void)
{
	int ret;

	ret = platform_driver_register(&onboard_hub_driver);
	if (ret)
		return ret;

	ret = usb_register_device_driver(&onboard_hub_usbdev_driver, THIS_MODULE);
	if (ret)
		platform_driver_unregister(&onboard_hub_driver);

	return ret;
}
module_init(onboard_hub_init);

static void __exit onboard_hub_exit(void)
{
	usb_deregister_device_driver(&onboard_hub_usbdev_driver);
	platform_driver_unregister(&onboard_hub_driver);
}
module_exit(onboard_hub_exit);

MODULE_AUTHOR("Matthias Kaehlcke <mka@chromium.org>");
MODULE_DESCRIPTION("Driver for discrete onboard USB hubs");
MODULE_LICENSE("GPL v2");
