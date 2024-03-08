/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_RESOURCES_H
#define IPTS_RESOURCES_H

#include <linux/device.h>
#include <linux/types.h>

#include "spec-mei.h"

struct ipts_buffer {
	u8 *address;
	size_t size;
};

struct ipts_dma_buffer {
	u8 *address;
	size_t size;

	dma_addr_t dma_address;
	struct device *dma_device;
};

/**
 * struct ipts_resources - The IPTS resource manager.
 *
 * This object is responsible for allocating, storing and freeing all buffers that are needed to
 * communicate with the ME and process the data that it produces.
 *
 * Every buffer that owns the data it points to should go in here. Buffers that don't own the
 * data they point to should go into &struct ipts_context.
 *
 * @data:
 *     The data buffers for ME to host DMA data transfer. The size of these buffers is determined
 *     by &struct ipts_device_info->data_size.
 *
 * @feedback:
 *     The feedback buffers for host to ME DMA data transfer. The size of these buffers is
 *     determined by &struct ipts_device_info->feedback_size.
 *
 * @doorbell:
 *     The doorbell buffer. The doorbell is an unsigned 32-bit integer that the ME will increment
 *     when new data is available.
 *
 * @workqueue:
 *     The buffer that holds the workqueue offset. The offset is a 32-bit integer that is only
 *     required when using GuC submission with vendor provided OpenCL kernels.
 *
 * @hid2me:
 *     The buffer for HID2ME feedback, a special feedback buffer intended for passing HID feature
 *     and output reports to the ME.
 *
 * @descriptor:
 *     The buffer for querying the native HID descriptor on EDS v2 devices. The size of the buffer
 *     should &struct ipts_device_info->data_size + 8.
 *
 * @report:
 *     A buffer that is used to synthesize HID reports on EDS v1 devices that don't natively support
 *     HID. The size of this buffer should be %IPTS_HID_REPORT_DATA_SIZE.
 *
 * @feature:
 *     A buffer that is used to cache the answer to a GET_FEATURES request, so that the data buffer
 *     containing it can be safely refilled by the ME. The size of this buffer must be
 *     &struct ipts_device_info->data_size
 */
struct ipts_resources {
	struct ipts_dma_buffer data[IPTS_MAX_BUFFERS];
	struct ipts_dma_buffer feedback[IPTS_MAX_BUFFERS];

	struct ipts_dma_buffer doorbell;
	struct ipts_dma_buffer workqueue;
	struct ipts_dma_buffer hid2me;
	struct ipts_dma_buffer descriptor;

	struct ipts_buffer report;
	struct ipts_buffer feature;
};

int ipts_resources_init(struct ipts_resources *resources, struct device *dev,
			struct ipts_rsp_get_device_info info);
void ipts_resources_free(struct ipts_resources *resources);

#endif /* IPTS_RESOURCES_H */
