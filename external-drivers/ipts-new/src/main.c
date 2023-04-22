// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/mei_cl_bus.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "mei.h"
#include "receiver.h"
#include "spec-device.h"

/*
 * The MEI client ID for IPTS functionality.
 */
#define IPTS_ID UUID_LE(0x3e8d0870, 0x271a, 0x4208, 0x8e, 0xb5, 0x9a, 0xcb, 0x94, 0x02, 0xae, 0x04)

static int ipts_set_dma_mask(struct mei_cl_device *cldev)
{
	if (!cldev)
		return -EFAULT;

	if (!dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(64)))
		return 0;

	return dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(32));
}

static int ipts_probe(struct mei_cl_device *cldev, const struct mei_cl_device_id *id)
{
	int ret = 0;
	struct ipts_context *ipts = NULL;

	if (!cldev)
		return -EFAULT;

	ret = ipts_set_dma_mask(cldev);
	if (ret) {
		dev_err(&cldev->dev, "Failed to set DMA mask for IPTS: %d\n", ret);
		return ret;
	}

	ret = mei_cldev_enable(cldev);
	if (ret) {
		dev_err(&cldev->dev, "Failed to enable MEI device: %d\n", ret);
		return ret;
	}

	ipts = devm_kzalloc(&cldev->dev, sizeof(*ipts), GFP_KERNEL);
	if (!ipts) {
		mei_cldev_disable(cldev);
		return -ENOMEM;
	}

	ret = ipts_mei_init(&ipts->mei, cldev);
	if (ret) {
		dev_err(&cldev->dev, "Failed to init MEI bus logic: %d\n", ret);
		return ret;
	}

	ipts->dev = &cldev->dev;
	ipts->mode = IPTS_MODE_EVENT;

	mutex_init(&ipts->feature_lock);
	init_completion(&ipts->feature_event);

	mei_cldev_set_drvdata(cldev, ipts);

	ret = ipts_control_start(ipts);
	if (ret) {
		dev_err(&cldev->dev, "Failed to start IPTS: %d\n", ret);
		return ret;
	}

	return 0;
}

static void ipts_remove(struct mei_cl_device *cldev)
{
	int ret = 0;
	struct ipts_context *ipts = NULL;

	if (!cldev) {
		pr_err("MEI device is NULL!");
		return;
	}

	ipts = mei_cldev_get_drvdata(cldev);

	ret = ipts_control_stop(ipts);
	if (ret)
		dev_err(&cldev->dev, "Failed to stop IPTS: %d\n", ret);

	mei_cldev_disable(cldev);
}

static struct mei_cl_device_id ipts_device_id_table[] = {
	{ .uuid = IPTS_ID, .version = MEI_CL_VERSION_ANY },
	{},
};
MODULE_DEVICE_TABLE(mei, ipts_device_id_table);

static struct mei_cl_driver ipts_driver = {
	.id_table = ipts_device_id_table,
	.name = "ipts",
	.probe = ipts_probe,
	.remove = ipts_remove,
};
module_mei_cl_driver(ipts_driver);

MODULE_DESCRIPTION("IPTS touchscreen driver");
MODULE_AUTHOR("Dorian Stoll <dorian.stoll@tmsp.io>");
MODULE_LICENSE("GPL");
