// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 */

#include <linux/freezer.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>

#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_drv.h"
#include "vdec_msg_queue.h"

static int vde_msg_queue_get_trans_size(int width, int height)
{
	if (width > 1920 || height > 1088)
		return (30 * 1024 * 1024);
	else
		return 6 * 1024 * 1024;
}

int vdec_msg_queue_init(
	struct mtk_vcodec_ctx *ctx,
	struct vdec_msg_queue *msg_queue,
	core_decode_t core_decode,
	int private_size)
{
	struct vdec_lat_buf *lat_buf;
	int i, err;

	init_waitqueue_head(&msg_queue->lat_read);
	INIT_LIST_HEAD(&msg_queue->lat_queue);
	spin_lock_init(&msg_queue->lat_lock);
	msg_queue->num_lat = 0;

	msg_queue->wdma_addr.size = vde_msg_queue_get_trans_size(
		ctx->picinfo.buf_w, ctx->picinfo.buf_h);

	err = mtk_vcodec_mem_alloc(ctx, &msg_queue->wdma_addr);
	if (err) {
		mtk_v4l2_err("failed to allocate wdma_addr buf");
		return -ENOMEM;
	}
	msg_queue->wdma_rptr_addr = msg_queue->wdma_addr.dma_addr;
	msg_queue->wdma_wptr_addr = msg_queue->wdma_addr.dma_addr;

	for (i = 0; i < NUM_BUFFER_COUNT; i++) {
		lat_buf = &msg_queue->lat_buf[i];

		lat_buf->wdma_err_addr.size = VDEC_ERR_MAP_SZ_AVC;
		err = mtk_vcodec_mem_alloc(ctx, &lat_buf->wdma_err_addr);
		if (err) {
			mtk_v4l2_err("failed to allocate wdma_err_addr buf[%d]", i);
			return -ENOMEM;
		}

		lat_buf->slice_bc_addr.size = VDEC_LAT_SLICE_HEADER_SZ;
		err = mtk_vcodec_mem_alloc(ctx, &lat_buf->slice_bc_addr);
		if (err) {
			mtk_v4l2_err("failed to allocate wdma_addr buf[%d]", i);
			return -ENOMEM;
		}

		lat_buf->private_data = kzalloc(private_size, GFP_KERNEL);
		if (!lat_buf->private_data) {
			mtk_v4l2_err("failed to allocate private_data[%d]", i);
			return -ENOMEM;
		}

		lat_buf->ctx = ctx;
		lat_buf->core_decode = core_decode;
		vdec_msg_queue_buf_to_lat(lat_buf);
	}

	msg_queue->init_done = true;
	return 0;
}

struct vdec_lat_buf *vdec_msg_queue_get_lat_buf(
	struct vdec_msg_queue *msg_queue)
{
	struct vdec_lat_buf *buf;
	long timeout_jiff;
	int ret;

	if (list_empty(&msg_queue->lat_queue)) {
		mtk_v4l2_debug(3, "lat queue is NULL, num_lat = %d", msg_queue->num_lat);
		timeout_jiff = msecs_to_jiffies(1500);
		ret = wait_event_timeout(msg_queue->lat_read,
			!list_empty(&msg_queue->lat_queue), timeout_jiff);
		if (!ret)
			return NULL;
	}

	spin_lock(&msg_queue->lat_lock);
	buf = list_first_entry(&msg_queue->lat_queue, struct vdec_lat_buf,
		lat_list);
	list_del(&buf->lat_list);
	msg_queue->num_lat--;

	mtk_v4l2_debug(4, "lat num in msg queue = %d", msg_queue->num_lat);
	mtk_v4l2_debug(3, "get lat(0x%p) trans(0x%llx) err:0x%llx slice(0x%llx)\n",
		buf, msg_queue->wdma_addr.dma_addr,
		buf->wdma_err_addr.dma_addr,
		buf->slice_bc_addr.dma_addr);

	spin_unlock(&msg_queue->lat_lock);
	return buf;
}

struct vdec_lat_buf *vdec_msg_queue_get_core_buf(
	struct mtk_vcodec_dev *dev)
{
	struct vdec_lat_buf *buf;
	int ret;

	if (list_empty(&dev->core_queue)) {
		mtk_v4l2_debug(3, "core queue is NULL, num_core = %d", dev->num_core);
		ret = wait_event_freezable(dev->core_read,
			!list_empty(&dev->core_queue));
		if (ret)
			return NULL;
	}

	spin_lock(&dev->core_lock);
	buf = list_first_entry(&dev->core_queue, struct vdec_lat_buf,
		core_list);
	mtk_v4l2_debug(3, "get core buf addr: (0x%p)", buf);
	list_del(&buf->core_list);
	dev->num_core--;
	spin_unlock(&dev->core_lock);
	return buf;
}

void vdec_msg_queue_buf_to_lat(struct vdec_lat_buf *buf)
{
	struct vdec_msg_queue *msg_queue = &buf->ctx->msg_queue;

	spin_lock(&msg_queue->lat_lock);
	list_add_tail(&buf->lat_list, &msg_queue->lat_queue);
	msg_queue->num_lat++;
	wake_up_all(&msg_queue->lat_read);
	mtk_v4l2_debug(3, "queue buf addr: (0x%p) lat num = %d",
		buf, msg_queue->num_lat);
	spin_unlock(&msg_queue->lat_lock);
}

void vdec_msg_queue_buf_to_core(struct mtk_vcodec_dev *dev,
	struct vdec_lat_buf *buf)
{
	spin_lock(&dev->core_lock);
	list_add_tail(&buf->core_list, &dev->core_queue);
	dev->num_core++;
	wake_up_all(&dev->core_read);
	mtk_v4l2_debug(3, "queu buf addr: (0x%p)", buf);
	spin_unlock(&dev->core_lock);
}

void vdec_msg_queue_update_ube_rptr(struct vdec_msg_queue *msg_queue,
	uint64_t ube_rptr)
{
	spin_lock(&msg_queue->lat_lock);
	msg_queue->wdma_rptr_addr = ube_rptr;
	mtk_v4l2_debug(3, "update ube rprt (0x%llx)", ube_rptr);
	spin_unlock(&msg_queue->lat_lock);
}

void vdec_msg_queue_update_ube_wptr(struct vdec_msg_queue *msg_queue,
	uint64_t ube_wptr)
{
	spin_lock(&msg_queue->lat_lock);
	msg_queue->wdma_wptr_addr = ube_wptr;
	mtk_v4l2_debug(3, "update ube wprt: (0x%llx 0x%llx) offset: 0x%llx",
		msg_queue->wdma_rptr_addr, msg_queue->wdma_wptr_addr, ube_wptr);
	spin_unlock(&msg_queue->lat_lock);
}

bool vdec_msg_queue_wait_lat_buf_full(struct vdec_msg_queue *msg_queue)
{
	long timeout_jiff;
	int ret, i;

	for (i = 0; i < NUM_BUFFER_COUNT + 2; i++) {
		timeout_jiff = msecs_to_jiffies(1000);
		ret = wait_event_timeout(msg_queue->lat_read,
			msg_queue->num_lat == NUM_BUFFER_COUNT, timeout_jiff);
		if (ret) {
			mtk_v4l2_debug(3, "success to get lat buf: %d",
				msg_queue->num_lat);
			return true;
		}
	}

	mtk_v4l2_err("failed with lat buf isn't full: %d",
		msg_queue->num_lat);
	return false;
}

void vdec_msg_queue_deinit(
	struct mtk_vcodec_ctx *ctx,
	struct vdec_msg_queue *msg_queue)
{
	struct vdec_lat_buf *lat_buf;
	struct mtk_vcodec_mem *mem;
	int i;

	mem = &msg_queue->wdma_addr;
	if (mem->va)
		mtk_vcodec_mem_free(ctx, mem);
	for (i = 0; i < NUM_BUFFER_COUNT; i++) {
		lat_buf = &msg_queue->lat_buf[i];

		mem = &lat_buf->wdma_err_addr;
		if (mem->va)
			mtk_vcodec_mem_free(ctx, mem);

		mem = &lat_buf->slice_bc_addr;
		if (mem->va)
			mtk_vcodec_mem_free(ctx, mem);

		if (lat_buf->private_data)
			kfree(lat_buf->private_data);
	}

	msg_queue->init_done = false;
}

int vdec_msg_queue_core_thead(void *data)
{
	struct mtk_vcodec_dev *dev = data;
	struct vdec_lat_buf *lat_buf;
	struct mtk_vcodec_ctx *ctx;

	set_freezable();
	for (;;) {
		try_to_freeze();
		if (kthread_should_stop())
			break;

		lat_buf = vdec_msg_queue_get_core_buf(dev);
		if (!lat_buf)
			continue;

		ctx = lat_buf->ctx;
		mtk_vcodec_dec_enable_hardware(ctx, MTK_VDEC_CORE);
		mtk_vcodec_set_curr_ctx(dev, ctx, MTK_VDEC_CORE);

		if (!lat_buf->core_decode)
			mtk_v4l2_err("Core decode callback func is NULL");
		else
			lat_buf->core_decode(lat_buf);

		mtk_vcodec_set_curr_ctx(dev, NULL, MTK_VDEC_CORE);
		mtk_vcodec_dec_disable_hardware(ctx, MTK_VDEC_CORE);
		vdec_msg_queue_buf_to_lat(lat_buf);
	}

	mtk_v4l2_debug(3, "Video Capture Thread End");
	return 0;
}
