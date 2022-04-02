// SPDX-License-Identifier: GPL-2.0
/*
 * cros_ec_pd_sysfs - expose the Chrome OS EC PD update through sysfs
 *
 * Copyright 2019 Google, Inc.
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_pd_update.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/uaccess.h>

/*
 * Driver loaded on top of the EC object.
 *
 * It exposes a sysfs interface, but most importantly, set global cros_ec_pd_ec
 * to let the real driver knows which cros_ec_pd_ec device to talk to.
 */
#define DRV_NAME "cros-ec-pd-sysfs"


static umode_t cros_ec_pd_attrs_are_visible(struct kobject *kobj,
					    struct attribute *a, int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct cros_ec_dev *ec = container_of(dev, struct cros_ec_dev,
					      class_dev);
	struct ec_params_usb_pd_rw_hash_entry hash_entry;
	struct ec_params_usb_pd_discovery_entry discovery_entry;

	/* Check if a PD MCU is present */
	if (cros_ec_pd_get_status(dev,
				  ec,
				  0,
				  &hash_entry,
				  &discovery_entry) == EC_RES_SUCCESS) {
		/*
		 * Save our ec pointer so we can conduct transactions.
		 * TODO(shawnn): Find a better way to access the ec pointer.
		 */
		if (!cros_ec_pd_ec)
			cros_ec_pd_ec = ec;
		return a->mode;
	}

	return 0;
}

static ssize_t firmware_images_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int size = 0;
	int i;

	for (i = 0; cros_ec_pd_firmware_images[i].rw_image_size > 0; i++) {
		if (cros_ec_pd_firmware_images[i].filename == NULL)
			size += scnprintf(
				buf + size, PAGE_SIZE,
				"%d: %d.%d NONE\n", i,
				cros_ec_pd_firmware_images[i].id_major,
				cros_ec_pd_firmware_images[i].id_minor);
		else
			size += scnprintf(
				buf + size, PAGE_SIZE,
				"%d: %d.%d %s\n", i,
				cros_ec_pd_firmware_images[i].id_major,
				cros_ec_pd_firmware_images[i].id_minor,
				cros_ec_pd_firmware_images[i].filename);
	}

	return size;
}

static DEVICE_ATTR_RO(firmware_images);

static struct attribute *__pd_attrs[] = {
	&dev_attr_firmware_images.attr,
	NULL,
};

static struct attribute_group cros_ec_pd_attr_group = {
	.name = "pd_update",
	.attrs = __pd_attrs,
	.is_visible = cros_ec_pd_attrs_are_visible,
};


static int cros_ec_pd_sysfs_probe(struct platform_device *pd)
{
	struct cros_ec_dev *ec_dev = dev_get_drvdata(pd->dev.parent);
	struct device *dev = &pd->dev;
	int ret;

	ret = sysfs_create_group(&ec_dev->class_dev.kobj,
			&cros_ec_pd_attr_group);
	if (ret < 0)
		dev_err(dev, "failed to create attributes. err=%d\n", ret);

	return ret;
}

static int cros_ec_pd_sysfs_remove(struct platform_device *pd)
{
	struct cros_ec_dev *ec_dev = dev_get_drvdata(pd->dev.parent);

	sysfs_remove_group(&ec_dev->class_dev.kobj, &cros_ec_pd_attr_group);

	return 0;
}

static struct platform_driver cros_ec_pd_sysfs_driver = {
	.driver = {
		.name = DRV_NAME,
	},
	.probe = cros_ec_pd_sysfs_probe,
	.remove = cros_ec_pd_sysfs_remove,
};

module_platform_driver(cros_ec_pd_sysfs_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC PD update sysfs driver");
MODULE_ALIAS("platform:" DRV_NAME);
