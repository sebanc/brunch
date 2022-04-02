// SPDX-License-Identifier: GPL-2.0
/*
 * HID support for Vivaldi Keyboard
 *
 * Copyright 2020 Google LLC.
 * Author: Sean O'Brien <seobrien@chromium.org>
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/input/vivaldi-fmap.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>

static ssize_t function_row_physmap_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct hid_device *hdev = to_hid_device(dev);
	struct vivaldi_data *drvdata = hid_get_drvdata(hdev);

	return vivaldi_function_row_physmap_show(drvdata, buf);
}

static DEVICE_ATTR_RO(function_row_physmap);
static struct attribute *sysfs_attrs[] = {
	&dev_attr_function_row_physmap.attr,
	NULL
};

static const struct attribute_group input_attribute_group = {
	.attrs = sysfs_attrs
};

static int vivaldi_probe(struct hid_device *hdev,
			 const struct hid_device_id *id)
{
	struct vivaldi_data *drvdata;
	int ret;

	drvdata = devm_kzalloc(&hdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	hid_set_drvdata(hdev, drvdata);

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	return hid_hw_start(hdev, HID_CONNECT_DEFAULT);
}

static void vivaldi_feature_mapping(struct hid_device *hdev,
				    struct hid_field *field,
				    struct hid_usage *usage)
{
	struct vivaldi_data *drvdata = hid_get_drvdata(hdev);

	vivaldi_hid_feature_mapping(drvdata, hdev, field, usage);
}

static int vivaldi_input_configured(struct hid_device *hdev,
				    struct hid_input *hidinput)
{
	return devm_device_add_group(&hdev->dev, &input_attribute_group);
}

static const struct hid_device_id vivaldi_table[] = {
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_VIVALDI, HID_ANY_ID,
		     HID_ANY_ID) },
	{ }
};

MODULE_DEVICE_TABLE(hid, vivaldi_table);

static struct hid_driver hid_vivaldi = {
	.name = "hid-vivaldi",
	.id_table = vivaldi_table,
	.probe = vivaldi_probe,
	.feature_mapping = vivaldi_feature_mapping,
	.input_configured = vivaldi_input_configured,
};

module_hid_driver(hid_vivaldi);

MODULE_AUTHOR("Sean O'Brien");
MODULE_DESCRIPTION("HID vivaldi driver");
MODULE_LICENSE("GPL");
