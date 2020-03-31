/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_H__
#define __MTK_CAM_H__

#include <linux/device.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#include "mtk_cam-ipi.h"

#define IMG_MAX_WIDTH		5376
#define IMG_MAX_HEIGHT		4032
#define IMG_MIN_WIDTH		80
#define IMG_MIN_HEIGHT		60

/*
 * ID enum value for struct mtk_cam_dev_node_desc:id
 * or mtk_cam_video_device:id
 */
enum  {
	MTK_CAM_P1_META_IN_0 = 0,
	MTK_CAM_P1_MAIN_STREAM_OUT,
	MTK_CAM_P1_PACKED_BIN_OUT,
	MTK_CAM_P1_META_OUT_0,
	MTK_CAM_P1_META_OUT_1,
	MTK_CAM_P1_META_OUT_2,
	MTK_CAM_P1_META_OUT_3,
	MTK_CAM_P1_TOTAL_NODES
};

/* Supported image format list */
#define MTK_CAM_IMG_FMT_UNKNOWN		0x0000
#define MTK_CAM_IMG_FMT_BAYER8		0x2200
#define MTK_CAM_IMG_FMT_BAYER10		0x2201
#define MTK_CAM_IMG_FMT_BAYER12		0x2202
#define MTK_CAM_IMG_FMT_BAYER14		0x2203
#define MTK_CAM_IMG_FMT_FG_BAYER8	0x2204
#define MTK_CAM_IMG_FMT_FG_BAYER10	0x2205
#define MTK_CAM_IMG_FMT_FG_BAYER12	0x2206
#define MTK_CAM_IMG_FMT_FG_BAYER14	0x2207

/* Supported bayer pixel order */
#define MTK_CAM_RAW_PXL_ID_B		0
#define MTK_CAM_RAW_PXL_ID_GB		1
#define MTK_CAM_RAW_PXL_ID_GR		2
#define MTK_CAM_RAW_PXL_ID_R		3
#define MTK_CAM_RAW_PXL_ID_UNKNOWN	4

/*
 * struct mtk_p1_frame_param - MTK ISP P1 driver frame parameters.
 *
 * @frame_seq_no: The frame sequence of frame in driver layer.
 * @dma_bufs: The DMA buffer address information of enabled DMA nodes.
 *
 */
struct mtk_p1_frame_param {
	unsigned int frame_seq_no;
	struct dma_buffer dma_bufs[MTK_CAM_P1_TOTAL_NODES];
} __packed;

/*
 * struct mtk_cam_dev_request - MTK camera device request.
 *
 * @req: Embedded struct media request.
 * @frame_params: The frame info. & address info. of enabled DMA nodes.
 * @frame_work: work queue entry for frame transmission to SCP.
 * @list: List entry of the object for @struct mtk_cam_dev:
 *        pending_job_list or running_job_list.
 * @timestamp: Start of frame timestamp in ns
 *
 */
struct mtk_cam_dev_request {
	struct media_request req;
	struct mtk_p1_frame_param frame_params;
	struct work_struct frame_work;
	struct list_head list;
	u64 timestamp;
};

/*
 * struct mtk_cam_dev_buffer - MTK camera device buffer.
 *
 * @vbb: Embedded struct vb2_v4l2_buffer.
 * @list: List entry of the object for @struct mtk_cam_video_device:
 *        buf_list.
 * @daddr: The DMA address of this buffer.
 * @scp_addr: The SCP address of this buffer which
 *            is only supported for meta input node.
 * @node_id: The vidoe node id which this buffer belongs to.
 *
 */
struct mtk_cam_dev_buffer {
	struct vb2_v4l2_buffer vbb;
	struct list_head list;
	/* Intenal part */
	dma_addr_t daddr;
	dma_addr_t scp_addr;
	unsigned int node_id;
};

/*
 * struct mtk_cam_dev_node_desc - MTK camera device node descriptor
 *
 * @id: id of the node
 * @name: name of the node
 * @cap: supported V4L2 capabilities
 * @buf_type: supported V4L2 buffer type
 * @dma_port: the dma ports associated to the node
 * @link_flags: default media link flags
 * @smem_alloc: using the smem_dev as alloc device or not
 * @image: true for image node, false for meta node
 * @num_fmts: the number of supported node formats
 * @default_fmt_idx: default format of this node
 * @max_buf_count: maximum VB2 buffer count
 * @ioctl_ops:  mapped to v4l2_ioctl_ops
 * @fmts: supported format
 * @frmsizes: supported V4L2 frame size number
 *
 */
struct mtk_cam_dev_node_desc {
	u8 id;
	const char *name;
	u32 cap;
	u32 buf_type;
	u32 dma_port;
	u32 link_flags;
	u8 smem_alloc:1;
	u8 image:1;
	u8 num_fmts;
	u8 default_fmt_idx;
	u8 max_buf_count;
	const struct v4l2_ioctl_ops *ioctl_ops;
	const struct v4l2_format *fmts;
	const struct v4l2_frmsizeenum *frmsizes;
};

/*
 * struct mtk_cam_video_device - Mediatek video device structure
 *
 * @id: Id for index of mtk_cam_dev:vdev_nodes array
 * @enabled: Indicate the video device is enabled or not
 * @desc: The node description of video device
 * @vdev_fmt: The V4L2 format of video device
 * @vdev_pad: The media pad graph object of video device
 * @vbq: A videobuf queue of video device
 * @vdev: The video device instance
 * @vdev_lock: Serializes vb2 queue and video device operations
 * @buf_list: List for enqueue buffers
 * @buf_list_lock: Lock used to protect buffer list.
 *
 */
struct mtk_cam_video_device {
	unsigned int id;
	unsigned int enabled;
	struct mtk_cam_dev_node_desc desc;
	struct v4l2_format vdev_fmt;
	struct media_pad vdev_pad;
	struct vb2_queue vbq;
	struct video_device vdev;
	/* Serializes vb2 queue and video device operations */
	struct mutex vdev_lock;
	struct list_head buf_list;
	/* Lock used to protect buffer list */
	spinlock_t buf_list_lock;
};

/*
 * struct mtk_cam_dev - Mediatek camera device structure.
 *
 * @dev: Pointer to device.
 * @smem_pdev: Pointer to shared memory device.
 * @pipeline: Media pipeline information.
 * @media_dev: Media device instance.
 * @subdev: The V4L2 sub-device instance.
 * @v4l2_dev: The V4L2 device driver instance.
 * @notifier: The v4l2_device notifier data.
 * @subdev_pads: Pointer to the number of media pads of this sub-device.
 * @vdev_nodes: The array list of mtk_cam_video_device nodes.
 * @seninf: Pointer to the seninf sub-device.
 * @sensor: Pointer to the active sensor V4L2 sub-device when streaming on.
 * @streaming: Indicate the overall streaming status is on or off.
 * @enabled_dmas: The enabled dma port information when streaming on.
 * @enabled_count: Number of enabled video nodes
 * @stream_count: Number of streaming video nodes
 * @running_job_count: Nunber of running jobs in the HW driver.
 * @pending_job_list: List to keep the media requests before en-queue into
 *                    HW driver.
 * @pending_job_lock: Protect the pending_job_list data & running_job_count.
 * @running_job_list: List to keep the media requests after en-queue into
 *                    HW driver.
 * @running_job_lock: Protect the running_job_list data.
 * @op_lock: Serializes driver's VB2 callback operations.
 *
 */
struct mtk_cam_dev {
	struct device *dev;
	struct device *smem_dev;
	struct media_pipeline pipeline;
	struct media_device media_dev;
	struct v4l2_subdev subdev;
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_pad *subdev_pads;
	struct mtk_cam_video_device vdev_nodes[MTK_CAM_P1_TOTAL_NODES];
	struct v4l2_subdev *seninf;
	struct v4l2_subdev *sensor;
	unsigned int streaming;
	unsigned int enabled_dmas;
	unsigned int enabled_count;
	unsigned int stream_count;
	unsigned int running_job_count;
	struct list_head pending_job_list;
	/* Protect the pending_job_list data */
	spinlock_t pending_job_lock;
	struct list_head running_job_list;
	/* Protect the running_job_list data & running_job_count */
	spinlock_t running_job_lock;
	/* Serializes driver's VB2 callback operations */
	struct mutex op_lock;
};

int mtk_cam_dev_init(struct platform_device *pdev,
		     struct mtk_cam_dev *cam_dev);
void mtk_cam_dev_cleanup(struct mtk_cam_dev *cam_dev);
void mtk_cam_dev_req_try_queue(struct mtk_cam_dev *cam_dev);
void mtk_cam_dev_dequeue_frame(struct mtk_cam_dev *cam_dev,
			       unsigned int node_id, unsigned int frame_seq_no,
			       int vb2_index);
void mtk_cam_dev_dequeue_req_frame(struct mtk_cam_dev *cam_dev,
				   unsigned int frame_seq_no);
void mtk_cam_dev_event_frame_sync(struct mtk_cam_dev *cam_dev,
				  unsigned int frame_seq_no);
struct mtk_cam_dev_request *mtk_cam_dev_get_req(struct mtk_cam_dev *cam,
						unsigned int frame_seq_no);

#endif /* __MTK_CAM_H__ */
