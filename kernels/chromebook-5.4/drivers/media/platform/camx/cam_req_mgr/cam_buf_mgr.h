/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Spectra camera buffer manager
 *
 * Copyright 2020 Google LLC.
 */

#ifndef _CAM_BUF_MGR_H_
#define _CAM_BUF_MGR_H_

#include <linux/types.h>
#include <linux/platform_device.h>

struct dma_buf *cmm_alloc_buffer(size_t len, unsigned int flags);

void cmm_free_buffer(struct dma_buf *dmabuf);

int cam_buf_mgr_init(struct platform_device *pdev);

void cam_buf_mgr_exit(struct platform_device *pdev);

#endif /* _CAM_BUF_MGR_H_ */
