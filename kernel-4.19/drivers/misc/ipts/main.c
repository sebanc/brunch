/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/dma-mapping.h>
#include <linux/intel-ipts.h>
#include <linux/module.h>

#include "msgs.h"

static ssize_t device_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ipts_info_t *ipts;

	ipts = dev_get_drvdata(dev);
	return sprintf(buf,	"hardware_id = %s\n"
				"vendor_id = 0x%04hX\n"
				"device_id = 0x%04hX\n"
				"hw_rev = %d\n"
				"fw_rev = %d\n"
				"frame_size = %d\n"
				"feedback_size = %d\n"
				"sensor_mode = %d\n"
				"max_touch_points = %d\n"
				"spi_frequency = %d\n"
				"spi_io_mode = %d\n"
				"sensor_minor_eds_rev = %d\n"
				"sensor_major_eds_rev = %d\n"
				"me_minor_eds_rev = %d\n"
				"me_major_eds_rev = %d\n"
				"sensor_eds_intf_rev = %d\n"
				"me_eds_intf_rev = %d\n"
				"kernel_compat_ver = %d\n",
				ipts->hardware_id,
				ipts->device_info.vendor_id,
				ipts->device_info.device_id,
				ipts->device_info.hw_rev,
				ipts->device_info.fw_rev,
				ipts->device_info.frame_size,
				ipts->device_info.feedback_size,
				ipts->device_info.sensor_mode,
				ipts->device_info.max_touch_points,
				ipts->device_info.spi_frequency,
				ipts->device_info.spi_io_mode,
				ipts->device_info.sensor_minor_eds_rev,
				ipts->device_info.sensor_major_eds_rev,
				ipts->device_info.me_minor_eds_rev,
				ipts->device_info.me_major_eds_rev,
				ipts->device_info.sensor_eds_intf_rev,
				ipts->device_info.me_eds_intf_rev,
				ipts->device_info.kernel_compat_ver);
}
static DEVICE_ATTR_RO(device_info);

static struct attribute *ipts_attrs[] = {
	&dev_attr_device_info.attr,
	NULL
};

static const struct attribute_group ipts_grp = {
	.attrs = ipts_attrs,
};

static void mei_ipts_rx(struct mei_cl_device *cldev)
{
	ipts_info_t *ipts = mei_cldev_get_drvdata(cldev);

	ssize_t msg_len;
	touch_sensor_msg_m2h_t m2h_msg;

	if (ipts->state != IPTS_STA_STOPPING) {
		msg_len = mei_cldev_recv(cldev, (u8*)&m2h_msg, sizeof(m2h_msg));
		if (msg_len <= 0) {
			dev_err(&cldev->dev, "error in reading m2h msg\n");
			return;
		}

		if (ipts_handle_resp(ipts, &m2h_msg, msg_len) != 0) {
			dev_err(&cldev->dev, "error in handling resp msg\n");
		}
	}
}

static int ipts_mei_cl_probe(struct mei_cl_device *cldev, const struct mei_cl_device_id *id)
{
	int ret = 0;
	ipts_info_t *ipts;

	dev_info(&cldev->dev, "IPTS started");

	if (dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(64)) == 0) {
		dev_dbg(&cldev->dev, "Using DMA_BIT_MASK(64)\n");
	} else if (dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(32)) == 0) {
		dev_dbg(&cldev->dev, "Using DMA_BIT_MASK(32)\n");
	} else {
		dev_err(&cldev->dev, "No suitable DMA available\n");
		return -EFAULT;
	}

	ipts = devm_kzalloc(&cldev->dev, sizeof(ipts_info_t), GFP_KERNEL);
	if (ipts == NULL) {
		dev_err(&cldev->dev, "Cannot allocate IPTS object\n");
		ret = -ENOMEM;
		return ret;
	}

	ipts->cldev = cldev;

	mei_cldev_set_drvdata(cldev, ipts);

	ret = mei_cldev_enable(cldev);
	if (ret < 0) {
		dev_err(&cldev->dev, "Cannot enable mei device\n");
		return ret;
	}

	ret = mei_cldev_register_rx_cb(cldev, mei_ipts_rx);
	if (ret) {
		dev_err(&cldev->dev, "Could not reg rx event ret=%d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&ipts->cldev->dev.kobj, &ipts_grp);
	if (ret)
		dev_err(&cldev->dev, "Cannot create sysfs for IPTS\n");

	ret = ipts_start(ipts);
	if (ret) {
		dev_err(&cldev->dev, "Cannot start IPTS\n");
		return ret;
	}

	return 0;
}

static int ipts_mei_cl_remove(struct mei_cl_device *cldev)
{
	ipts_info_t *ipts = mei_cldev_get_drvdata(cldev);

	ipts_stop(ipts);
	sysfs_remove_group(&cldev->dev.kobj, &ipts_grp);
	mei_cldev_disable(cldev);

	dev_info(&cldev->dev, "IPTS removed");

	return 0;
}

static struct mei_cl_device_id ipts_mei_cl_tbl[] = {
	{ .uuid = UUID_LE(0x3e8d0870, 0x271a, 0x4208, 0x8e, 0xb5, 0x9a, 0xcb, 0x94, 0x02, 0xae, 0x04), .version = MEI_CL_VERSION_ANY },
	{ }
};
MODULE_DEVICE_TABLE(mei, ipts_mei_cl_tbl);

static struct mei_cl_driver ipts_mei_cl_driver = {
	.id_table = ipts_mei_cl_tbl,
	.name = KBUILD_MODNAME,
	.probe = ipts_mei_cl_probe,
	.remove = ipts_mei_cl_remove,
};

module_mei_cl_driver(ipts_mei_cl_driver);

MODULE_DESCRIPTION
	("Intel(R) Management Engine Interface Client Driver for "\
	"Intel Precision Touch and Stylus");
MODULE_LICENSE("GPL");
