// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "resources.h"
#include "spec-hid.h"
#include "spec-mei.h"

static int ipts_resources_alloc_dma(struct ipts_dma_buffer *buffer, struct device *dev, size_t size)
{
	if (buffer->address)
		return 0;

	buffer->address = dma_alloc_coherent(dev, size, &buffer->dma_address, GFP_KERNEL);

	if (!buffer->address)
		return -ENOMEM;

	buffer->size = size;
	buffer->dma_device = dev;

	return 0;
}

static void ipts_resources_free_dma(struct ipts_dma_buffer *buffer)
{
	if (!buffer->address)
		return;

	dma_free_coherent(buffer->dma_device, buffer->size, buffer->address, buffer->dma_address);

	buffer->address = NULL;
	buffer->size = 0;

	buffer->dma_address = 0;
	buffer->dma_device = NULL;
}

static int ipts_resources_alloc_buffer(struct ipts_buffer *buffer, size_t size)
{
	if (buffer->address)
		return 0;

	buffer->size = size;
	buffer->address = kzalloc(size, GFP_KERNEL);

	if (!buffer->address)
		return -ENOMEM;

	return 0;
}

static void ipts_resources_free_buffer(struct ipts_buffer *buffer)
{
	if (!buffer->address)
		return;

	kfree(buffer->address);
	buffer->address = NULL;
	buffer->size = 0;
}

int ipts_resources_init(struct ipts_resources *resources, struct device *dev,
			struct ipts_rsp_get_device_info info)
{
	int i = 0;
	int ret = 0;

	size_t hid2me_size = min_t(size_t, info.feedback_size, IPTS_HID_2_ME_BUFFER_SIZE);

	for (i = 0; i < IPTS_MAX_BUFFERS; i++) {
		ret = ipts_resources_alloc_dma(&resources->data[i], dev, info.data_size);
		if (ret)
			goto err;
	}

	for (i = 0; i < IPTS_MAX_BUFFERS; i++) {
		ret = ipts_resources_alloc_dma(&resources->feedback[i], dev, info.feedback_size);
		if (ret)
			goto err;
	}

	ret = ipts_resources_alloc_dma(&resources->doorbell, dev, sizeof(u32));
	if (ret)
		goto err;

	ret = ipts_resources_alloc_dma(&resources->workqueue, dev, sizeof(u32));
	if (ret)
		goto err;

	ret = ipts_resources_alloc_dma(&resources->hid2me, dev, hid2me_size);
	if (ret)
		goto err;

	ret = ipts_resources_alloc_dma(&resources->descriptor, dev, info.data_size + 8);
	if (ret)
		goto err;

	ret = ipts_resources_alloc_buffer(&resources->report, IPTS_HID_REPORT_DATA_SIZE);
	if (ret)
		goto err;

	ret = ipts_resources_alloc_buffer(&resources->feature, info.data_size);
	if (ret)
		goto err;

	return 0;

err:

	ipts_resources_free(resources);
	return ret;
}

void ipts_resources_free(struct ipts_resources *resources)
{
	int i = 0;

	for (i = 0; i < IPTS_MAX_BUFFERS; i++)
		ipts_resources_free_dma(&resources->data[i]);

	for (i = 0; i < IPTS_MAX_BUFFERS; i++)
		ipts_resources_free_dma(&resources->feedback[i]);

	ipts_resources_free_dma(&resources->doorbell);
	ipts_resources_free_dma(&resources->workqueue);
	ipts_resources_free_dma(&resources->hid2me);
	ipts_resources_free_dma(&resources->descriptor);
	ipts_resources_free_buffer(&resources->report);
	ipts_resources_free_buffer(&resources->feature);
}
