/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_RESOURCES_H
#define IPTS_RESOURCES_H

#include <linux/device.h>
#include <linux/types.h>

#include "spec-device.h"

struct ipts_buffer {
	u8 *address;
	size_t size;

	dma_addr_t dma_address;
	struct device *device;
};

struct ipts_resources {
	struct ipts_buffer data[IPTS_BUFFERS];
	struct ipts_buffer feedback[IPTS_BUFFERS];

	struct ipts_buffer doorbell;
	struct ipts_buffer workqueue;
	struct ipts_buffer hid2me;

	struct ipts_buffer descriptor;

	// Buffer for synthesizing HID reports
	struct ipts_buffer report;
};

int ipts_resources_init(struct ipts_resources *res, struct device *dev, size_t ds, size_t fs);
int ipts_resources_free(struct ipts_resources *res);

#endif /* IPTS_RESOURCES_H */
