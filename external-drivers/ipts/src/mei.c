// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
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
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "receiver.h"
#include "spec-device.h"

/*
 * The MEI client ID for IPTS functionality.
 */
#define IPTS_MEI_UUID                                                                              \
	UUID_LE(0x3e8d0870, 0x271a, 0x4208, 0x8e, 0xb5, 0x9a, 0xcb, 0x94, 0x02, 0xae, 0x04)

static int ipts_mei_set_dma_mask(struct mei_cl_device *cldev)
{
	if (!dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(64)))
		return 0;

	return dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(32));
}

static int ipts_mei_probe(struct mei_cl_device *cldev, const struct mei_cl_device_id *id)
{
	int ret;
	struct ipts_context *ipts;

	ret = ipts_mei_set_dma_mask(cldev);
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

	ipts->cldev = cldev;
	ipts->dev = &cldev->dev;
	ipts->mode = IPTS_MODE_EVENT;

	mei_cldev_set_drvdata(cldev, ipts);

	ret = ipts_control_start(ipts);
	if (ret) {
		dev_err(&cldev->dev, "Failed to start IPTS: %d\n", ret);
		return ret;
	}

	return 0;
}

static void ipts_mei_remove(struct mei_cl_device *cldev)
{
	int ret;
	struct ipts_context *ipts = mei_cldev_get_drvdata(cldev);

	ret = ipts_control_stop(ipts);
	if (ret)
		dev_err(&cldev->dev, "Failed to stop IPTS: %d\n", ret);

	mei_cldev_disable(cldev);
}

static struct mei_cl_device_id ipts_mei_device_id_table[] = {
	{ .uuid = IPTS_MEI_UUID, .version = MEI_CL_VERSION_ANY },
	{},
};
MODULE_DEVICE_TABLE(mei, ipts_mei_device_id_table);

static struct mei_cl_driver ipts_mei_driver = {
	.id_table = ipts_mei_device_id_table,
	.name = "ipts",
	.probe = ipts_mei_probe,
	.remove = ipts_mei_remove,
};
module_mei_cl_driver(ipts_mei_driver);

MODULE_DESCRIPTION("IPTS touchscreen driver");
MODULE_AUTHOR("Dorian Stoll <dorian.stoll@tmsp.io>");
MODULE_LICENSE("GPL");
