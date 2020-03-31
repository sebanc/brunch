/*
 * Tablet mode switch sysfs trigger.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>

bool tablet_mode = 0;
struct platform_device *pdev = NULL;
static struct input_dev *idev = NULL;

void tablet_mode_notify(int value)
{
	if (tablet_mode == value)
		return;

	tablet_mode = value;
	input_report_switch(idev, SW_TABLET_MODE, tablet_mode);
	input_sync(idev);
}

static ssize_t tablet_mode_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%d\n", tablet_mode);
}

static ssize_t tablet_mode_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int value;

	if (kstrtoint(buf, 0, &value))
		return -EINVAL;

	if (value != 0 && value != 1)
		return -EINVAL;

	tablet_mode_notify(value);

       return count;
}

static const DEVICE_ATTR(tablet_mode, 0644, tablet_mode_show, tablet_mode_store);

static int __init tablet_mode_switch_init(void)
{
	char *input_device_name = "Tablet mode switch";
	int error;

	pdev = platform_device_register_simple("tablet_mode_switch", 0, NULL, 0);
	if (IS_ERR(pdev))
		return -ENODEV;

	idev = input_allocate_device();
	if (!idev)
		return -ENOMEM;

	idev->id.bustype = BUS_HOST;
	idev->id.version = 1;
	idev->id.product = 0;
	idev->name = input_device_name;
	idev->dev.parent = &pdev->dev;

	input_set_drvdata(idev, pdev);

	input_set_capability(idev, EV_SW, SW_TABLET_MODE);
	error = input_register_device(idev);
	if (error)
		goto err_free_input;

	device_create_file(idev->dev.parent, &dev_attr_tablet_mode);

	dev_info(idev->dev.parent, "started\n");

	return 0;

 err_free_input:
	input_free_device(idev);

	return error;
}

void tablet_mode_switch_exit(void)
{
	device_remove_file(idev->dev.parent, &dev_attr_tablet_mode);
	input_unregister_device(idev);
	platform_device_unregister(pdev);
}

module_init(tablet_mode_switch_init);
module_exit(tablet_mode_switch_exit);

MODULE_LICENSE("GPL");
