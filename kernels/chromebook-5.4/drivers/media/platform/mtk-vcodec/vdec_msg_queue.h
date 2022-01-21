// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 */

#ifndef _VDEC_MSG_QUEUE_H_
#define _VDEC_MSG_QUEUE_H_

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <media/videobuf2-v4l2.h>

#include "mtk_vcodec_util.h"

#define NUM_BUFFER_COUNT 3
#define VDEC_LAT_SLICE_HEADER_SZ    (640 * 1024)
#define VDEC_ERR_MAP_SZ_AVC         ((8192 / 16) * (4352 / 16) / 8)

struct vdec_lat_buf;
struct mtk_vcodec_ctx;
struct mtk_vcodec_dev;
typedef int (*core_decode_t)(struct vdec_lat_buf *lat_buf);

struct vdec_lat_buf {
	struct mtk_vcodec_mem wdma_err_addr;
	struct mtk_vcodec_mem slice_bc_addr;
	struct vb2_v4l2_buffer ts_info;

	void *private_data;
	struct mtk_vcodec_ctx *ctx;
	core_decode_t core_decode;
	struct list_head lat_list;
	struct list_head core_list;
};

struct vdec_msg_queue {
	struct vdec_lat_buf lat_buf[NUM_BUFFER_COUNT];

	struct mtk_vcodec_mem wdma_addr;
	uint64_t wdma_rptr_addr;
	uint64_t wdma_wptr_addr;

	wait_queue_head_t lat_read;
	struct list_head lat_queue;
	spinlock_t lat_lock;
	int num_lat;
	bool init_done;
};

/**
 * vdec_msg_queue_init - init lat buffer information.
 * @ctx: v4l2 ctx
 * @msg_queue: used to store lat buffer information
 * @core_decode: core decoder callback for each codec
 * @private_size: the size used to share with core
 */
int vdec_msg_queue_init(
	struct mtk_vcodec_ctx *ctx,
	struct vdec_msg_queue *msg_queue,
	core_decode_t core_decode,
	int private_size);

/**
 * vdec_msg_queue_get_lat_buf - get used lat buffer for core decode.
 * @msg_queue: used to store lat buffer information
 */
struct vdec_lat_buf *vdec_msg_queue_get_lat_buf(
	struct vdec_msg_queue *msg_queue);

/**
 * vdec_msg_queue_get_core_buf - get used core buffer for lat decode.
 * @dev: mtk vcodec device
 */
struct vdec_lat_buf *vdec_msg_queue_get_core_buf(
	struct mtk_vcodec_dev *dev);

/**
 * vdec_msg_queue_buf_to_core - queue buf to core for core decode.
 * @dev: mtk vcodec device
 * @buf: current lat buffer
 */
void vdec_msg_queue_buf_to_core(struct mtk_vcodec_dev *dev,
	struct vdec_lat_buf *buf);

/**
 * vdec_msg_queue_buf_to_lat - queue buf to lat for lat decode.
 * @buf: current lat buffer
 */
void vdec_msg_queue_buf_to_lat(struct vdec_lat_buf *buf);

/**
 * vdec_msg_queue_update_ube_rptr - used to updata ube read point.
 * @msg_queue: used to store lat buffer information
 * @ube_rptr: current ube read point
 */
void vdec_msg_queue_update_ube_rptr(struct vdec_msg_queue *msg_queue,
	uint64_t ube_rptr);

/**
 * vdec_msg_queue_update_ube_wptr - used to updata ube write point.
 * @msg_queue: used to store lat buffer information
 * @ube_wptr: current ube write point
 */
void vdec_msg_queue_update_ube_wptr(struct vdec_msg_queue *msg_queue,
	uint64_t ube_wptr);

/**
 * vdec_msg_queue_wait_lat_buf_full - used to check whether all lat buffer
 *                                    in lat list.
 * @msg_queue: used to store lat buffer information
 */
bool vdec_msg_queue_wait_lat_buf_full(struct vdec_msg_queue *msg_queue);

/**
 * vdec_msg_queue_deinit - deinit lat buffer information.
 * @ctx: v4l2 ctx
 * @msg_queue: used to store lat buffer information
 */
void vdec_msg_queue_deinit(
	struct mtk_vcodec_ctx *ctx,
	struct vdec_msg_queue *msg_queue);

/**
 * vdec_msg_queue_core_thead - used for core decoder.
 * @data: private data used for each codec
 */
int vdec_msg_queue_core_thead(void *data);

#endif
