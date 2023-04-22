// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/dma-mapping.h>
#include <linux/types.h>

#include "desc.h"
#include "resources.h"
#include "spec-device.h"

static int ipts_resources_alloc_buffer(struct ipts_buffer *buffer, struct device *dev, size_t size)
{
	if (!buffer)
		return -EFAULT;

	if (buffer->address)
		return 0;

	buffer->address = dma_alloc_coherent(dev, size, &buffer->dma_address, GFP_KERNEL);

	if (!buffer->address)
		return -ENOMEM;

	buffer->size = size;
	buffer->device = dev;

	return 0;
}

static void ipts_resources_free_buffer(struct ipts_buffer *buffer)
{
	if (!buffer->address)
		return;

	dma_free_coherent(buffer->device, buffer->size, buffer->address, buffer->dma_address);

	buffer->address = NULL;
	buffer->size = 0;

	buffer->dma_address = 0;
	buffer->device = NULL;
}

int ipts_resources_init(struct ipts_resources *res, struct device *dev, size_t ds, size_t fs)
{
	int ret = 0;

	/*
	 * Some compilers (AOSP clang) complain about a redefined
	 * variable when this is declared inside of the for loop.
	 */
	int i = 0;

	if (!res)
		return -EFAULT;

	for (i = 0; i < IPTS_BUFFERS; i++) {
		ret = ipts_resources_alloc_buffer(&res->data[i], dev, ds);
		if (ret)
			goto err;
	}

	for (i = 0; i < IPTS_BUFFERS; i++) {
		ret = ipts_resources_alloc_buffer(&res->feedback[i], dev, fs);
		if (ret)
			goto err;
	}

	ret = ipts_resources_alloc_buffer(&res->doorbell, dev, sizeof(u32));
	if (ret)
		goto err;

	ret = ipts_resources_alloc_buffer(&res->workqueue, dev, sizeof(u32));
	if (ret)
		goto err;

	ret = ipts_resources_alloc_buffer(&res->hid2me, dev, fs);
	if (ret)
		goto err;

	ret = ipts_resources_alloc_buffer(&res->descriptor, dev, ds + 8);
	if (ret)
		goto err;

	if (!res->report.address) {
		res->report.size = IPTS_HID_REPORT_DATA_SIZE;
		res->report.address = kzalloc(res->report.size, GFP_KERNEL);

		if (!res->report.address) {
			ret = -ENOMEM;
			goto err;
		}
	}

	return 0;

err:

	ipts_resources_free(res);
	return ret;
}

int ipts_resources_free(struct ipts_resources *res)
{
	/*
	 * Some compilers (AOSP clang) complain about a redefined
	 * variable when this is declared inside of the for loop.
	 */
	int i = 0;

	if (!res)
		return -EFAULT;

	for (i = 0; i < IPTS_BUFFERS; i++)
		ipts_resources_free_buffer(&res->data[i]);

	for (i = 0; i < IPTS_BUFFERS; i++)
		ipts_resources_free_buffer(&res->feedback[i]);

	ipts_resources_free_buffer(&res->doorbell);
	ipts_resources_free_buffer(&res->workqueue);
	ipts_resources_free_buffer(&res->hid2me);
	ipts_resources_free_buffer(&res->descriptor);

	if (res->report.address) {
		kfree(res->report.address);

		res->report.address = NULL;
		res->report.size = 0;
	}

	return 0;
}
