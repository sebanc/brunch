// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/mei_cl_bus.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "context.h"
#include "control.h"
#include "linux/device.h"
#include "protocol.h"
#include "receiver.h"
#include "uapi.h"

static int ipts_mei_set_dma_mask(struct mei_cl_device *cldev)
{
	int ret;

	ret = dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(64));
	if (!ret)
		return 0;

	return dma_coerce_mask_and_coherent(&cldev->dev, DMA_BIT_MASK(32));
}

static int ipts_mei_probe(struct mei_cl_device *cldev,
			  const struct mei_cl_device_id *id)
{
	int ret;
	struct ipts_context *ipts;

	if (ipts_mei_set_dma_mask(cldev)) {
		dev_err(&cldev->dev, "Failed to set DMA mask for IPTS\n");
		return -EFAULT;
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
	ipts->status = IPTS_HOST_STATUS_STOPPED;

	mei_cldev_set_drvdata(cldev, ipts);
	mei_cldev_register_rx_cb(cldev, ipts_receiver_callback);

	return ipts_control_start(ipts);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
static int ipts_mei_remove(struct mei_cl_device *cldev)
{
	int i;
	struct ipts_context *ipts = mei_cldev_get_drvdata(cldev);

	ipts_control_stop(ipts);

	/*
	 * We need to flush all buffers and clear their addresses to prevent
	 * issues during or after suspend. Delay the removal until all of that
	 * is done, but not for more than 500ms.
	 */
	for (i = 0; i < 20; i++) {
		if (ipts->status == IPTS_HOST_STATUS_STOPPED)
			break;

		msleep(25);
	}

	mei_cldev_disable(cldev);
	
	return 0;
}
#else
static void ipts_mei_remove(struct mei_cl_device *cldev)
{
	int i;
	struct ipts_context *ipts = mei_cldev_get_drvdata(cldev);

	ipts_control_stop(ipts);

	/*
	 * We need to flush all buffers and clear their addresses to prevent
	 * issues during or after suspend. Delay the removal until all of that
	 * is done, but not for more than 500ms.
	 */
	for (i = 0; i < 20; i++) {
		if (ipts->status == IPTS_HOST_STATUS_STOPPED)
			break;

		msleep(25);
	}

	mei_cldev_disable(cldev);
}
#endif

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

static int __init ipts_mei_init(void)
{
	int ret;

	ret = ipts_uapi_init();
	if (ret)
		return ret;

	ret = mei_cldev_driver_register(&ipts_mei_driver);
	if (ret) {
		ipts_uapi_free();
		return ret;
	}

	return 0;
}

static void __exit ipts_mei_exit(void)
{
	mei_cldev_driver_unregister(&ipts_mei_driver);
	ipts_uapi_free();
}

MODULE_DESCRIPTION("IPTS touchscreen driver");
MODULE_AUTHOR("Dorian Stoll <dorian.stoll@tmsp.io>");
MODULE_LICENSE("GPL");

module_init(ipts_mei_init);
module_exit(ipts_mei_exit);
