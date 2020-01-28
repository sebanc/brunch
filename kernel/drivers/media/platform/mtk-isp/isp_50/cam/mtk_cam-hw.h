/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_HW_H__
#define __MTK_CAM_HW_H__

#include <linux/types.h>

#include "mtk_cam.h"
#include "mtk_cam-ipi.h"

/*
 * struct mtk_isp_p1_device - the Mediatek ISP P1 device information
 *
 * @dev: Pointer to device.
 * @scp_pdev: Pointer to SCP platform device.
 * @rproc_handle: Pointer to new remoteproc instance.
 * @cam_dev: Embedded struct cam_dev
 * @regs: Camera ISP HW base register address
 * @num_clks: The number of driver's clocks
 * @clks: The clock data array
 * @spinlock_irq: Used to protect register read/write data
 * @enqueued_frame_seq_no: Frame sequence number of enqueued frame
 * @dequeued_frame_seq_no: Frame sequence number of dequeued frame
 * @composed_frame_seq_no: Frame sequence number of composed frame
 * @timestamp: Frame timestamp in ns
 * @meta0_vb2_index: Meta0 vb2 buffer index, set when SOF
 * @meta1_vb2_index: Meta1 vb2 buffer index, set when SOF
 * @sof_count: SOF counter
 * @composer_wq: The work queue for frame request composing
 * @composer_scp_addr: SCP address of ISP composer memory
 * @composer_iova: DMA address of ISP composer memory
 * @virt_addr: Virtual address of ISP composer memory
 *
 */
struct mtk_isp_p1_device {
	struct device *dev;
	struct platform_device *scp_pdev;
	struct rproc *rproc_handle;
	struct mtk_cam_dev cam_dev;
	void __iomem *regs;
	unsigned int num_clks;
	struct clk_bulk_data *clks;
	/* Used to protect register read/write data */
	spinlock_t spinlock_irq;
	unsigned int enqueued_frame_seq_no;
	unsigned int dequeued_frame_seq_no;
	unsigned int composed_frame_seq_no;
	unsigned int meta0_vb2_index;
	unsigned int meta1_vb2_index;
	u8 sof_count;
	struct workqueue_struct *composer_wq;
	dma_addr_t composer_scp_addr;
	dma_addr_t composer_iova;
	void *composer_virt_addr;
};

int mtk_isp_hw_init(struct mtk_cam_dev *cam_dev);
int mtk_isp_hw_release(struct mtk_cam_dev *cam_dev);
void mtk_isp_hw_config(struct mtk_cam_dev *cam_dev,
		       struct p1_config_param *config_param);
void mtk_isp_stream(struct mtk_cam_dev *cam_dev, int on);
void mtk_isp_enqueue(struct mtk_cam_dev *cam_dev, unsigned int dma_port,
		     struct mtk_cam_dev_buffer *buffer);
void mtk_isp_req_enqueue(struct mtk_cam_dev *cam_dev,
			 struct mtk_cam_dev_request *req);

#endif /* __MTK_CAM_HW_H__ */
