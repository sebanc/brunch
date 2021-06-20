/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Virtio GPU interfaces for sharing data.
 *
 * Copyright (C) 2018 Google, Inc.
 */

#ifndef _VIRTIO_DRM_H
#define _VIRTIO_DRM_H

#include <linux/dma-buf.h>
#include <linux/types.h>

/*
 * The following methods are to share dma bufs with a host via the
 * virtio Wayland (virtwl) device.
 */

/*
 * Converts the given dma_buf to the virtio-gpu specific resource handle
 * backing the dma_buf, waiting for creation to be confirmed by the host
 * if necessary.
 */
extern int virtio_gpu_dma_buf_to_handle(struct dma_buf *buf, bool no_wait,
					uint32_t *handle);

#endif /* _VIRTIO_DRM_H */
