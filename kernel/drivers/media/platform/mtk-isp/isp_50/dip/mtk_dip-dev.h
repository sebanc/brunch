/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_DEV_H_
#define _MTK_DIP_DEV_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include "mtk_dip-hw.h"

#define MTK_DIP_PIPE_ID_PREVIEW			0
#define MTK_DIP_PIPE_ID_CAPTURE			1
#define MTK_DIP_PIPE_ID_REPROCESS		2
#define MTK_DIP_PIPE_ID_TOTAL_NUM		3

#define MTK_DIP_VIDEO_NODE_ID_RAW_OUT		0
#define MTK_DIP_VIDEO_NODE_ID_TUNING_OUT	1
#define MTK_DIP_VIDEO_NODE_ID_NR_OUT		2
#define MTK_DIP_VIDEO_NODE_ID_SHADING_OUT	3
#define MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE	4
#define MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE	5
#define MTK_DIP_VIDEO_NODE_ID_IMG2_CAPTURE	6
#define MTK_DIP_VIDEO_NODE_ID_IMG3_CAPTURE	7
#define MTK_DIP_VIDEO_NODE_ID_TOTAL_NUM		8

#define MTK_DIP_OUTPUT_MIN_WIDTH		2U
#define MTK_DIP_OUTPUT_MIN_HEIGHT		2U
#define MTK_DIP_OUTPUT_MAX_WIDTH		5376U
#define MTK_DIP_OUTPUT_MAX_HEIGHT		4032U
#define MTK_DIP_CAPTURE_MIN_WIDTH		2U
#define MTK_DIP_CAPTURE_MIN_HEIGHT		2U
#define MTK_DIP_CAPTURE_MAX_WIDTH		5376U
#define MTK_DIP_CAPTURE_MAX_HEIGHT		4032U

#define MTK_DIP_DEV_META_BUF_DEFAULT_SIZE	(1024 * 128)
#define MTK_DIP_DEV_META_BUF_POOL_MAX_SIZE	(1024 * 1024 * 16)

/**
 * struct mtk_dip_dev_format - DIP buffer format
 * @format:	the corresponding V4L2 pixel format
 * @mdp_color:	color information setting
 * @depth:	bytes per pixel of the format
 * @row_depth:	bits per pixel
 * @num_planes:	the number of planes
 * @num_cplanes:	the number of color planes carried in a
 *			single v4l2 plane
 * @flags:	flags setting which will be passed to media_create_pad_link()
 * @buffer_size:	the buffer size of the buffer. It is used when it is
 *			a meta type format.
 * @pass_1_align:	the alignment setting of the stride .If pass_1_align
 *			is greater than 0, the stride must be align with it.
 *
 * The structure defines the DIP internal buffer format information. The fields
 * is used in V4L2 try/set format calculation flow.
 */
struct mtk_dip_dev_format {
	u32 format;
	u32 mdp_color;
	u8 depth[VIDEO_MAX_PLANES];
	u8 row_depth[VIDEO_MAX_PLANES];
	u8 num_planes;
	u8 num_cplanes;
	u32 flags;
	u32 buffer_size;
	u8 pass_1_align;
};

/**
 * struct mtk_dip_crop - DIP crop setting
 * @c:	crop region
 * @left_subpix: the left subpixel part of the corp region
 * @top_subpix: the top subpixel part of the corp region
 * @width_subpix: the width subpixel part of the corp region
 * @height_subpix: the height subpixel part of the corp region
 *
 * The structure includes the crop setting which can be passed to
 * DIP hardware.
 */
struct mtk_dip_crop {
	struct v4l2_rect	c;
	struct v4l2_fract	left_subpix;
	struct v4l2_fract	top_subpix;
	struct v4l2_fract	width_subpix;
	struct v4l2_fract	height_subpix;
};

/**
 * struct mtk_dip_dev_buffer - Buffer information used by DIP
 * @vbb:	the vb2 buffer associated
 * @fmt:	the corresponding v4l2 format
 * @dev_fmt:	the DIP internal format
 * @pipe_job_id: the global id of the request owned the buffer
 * @isp_daddr:	the address which can be used by ISP hardware
 * @scp_daddr:	the address which can be used in coprocessor
 * @dma_port:	dma port id of the buffer
 * @crop:	corp setting of the buffer
 * @compose:	compose setting of the buffer
 * @rotation:	rotation degree of the buffer
 * @hflip:	need horizontal flip or not
 * @vflip:	need vertical flip or not
 *
 * The structure includes the Buffer setting which can be used by
 * DIP hardware.
 */
struct mtk_dip_dev_buffer {
	struct vb2_v4l2_buffer vbb;
	struct v4l2_format fmt;
	const struct mtk_dip_dev_format *dev_fmt;
	int pipe_job_id;
	dma_addr_t isp_daddr[VB2_MAX_PLANES];
	dma_addr_t scp_daddr[VB2_MAX_PLANES];
	unsigned int dma_port;
	struct mtk_dip_crop crop;
	struct v4l2_rect compose;
	int rotation;
	int hflip;
	int vflip;
	struct list_head list;
};

/**
 * struct mtk_dip_pipe_desc - dip pipe descriptor
 * @name:	name of the pipe, which will be used as a part of the video
 *		device and sub device name
 * @id:		the id of the pipe
 * @queue_descs:	the setting of the queues belong to this pipe
 * @total_queues: the number of queue/video nodes supported by this pipe
 *
 * The structure describes the pipe of DIP. A pipe may contains a raw output
 * video device and at least one MDP capture device.
 */
struct mtk_dip_pipe_desc {
	const char *name;
	int id;
	const struct mtk_dip_video_device_desc *queue_descs;
	int total_queues;
};

/**
 * struct mtk_dip_video_device_desc - video device descriptor
 * @name:	name of the video device
 * @id:		the id of the video device
 * @buf_type:	buffer type of the video device
 * @cap:	device capabilities
 * @smem_alloc:	use the co-processor and CPU shared memory allocator or not
 * @supports_ctrls: support ctrl handler or not. If it is true, The DIP driver
 *		    initialized the ctrl handler for this video node.
 * @fmts:	buffer format supported by this video device
 * @num_fmts:	total number of buffer format supported by this video device
 * @description: description of the video device. It will be returned when
 *		 V4L2 enum_fmt calls
 * @dma_port:	dma port id associated to this video device
 * @frmsizeenum: frame size supported
 * @ops:	v4l2_ioctl_ops pointer used by this video device
 * @vb2_ops:	vb2_ops pointer used by this video device
 * @flags:	flags used in media_create_intf_link()
 * @default_fmt_idx: indeciate the default format with index of @fmts
 *
 * The structure describes the video device setting of DIP, which are used to
 * register the video devices and support the related V4L2 and VB2 operations.
 */
struct mtk_dip_video_device_desc {
	const char *name;
	int id;
	u32 buf_type;
	u32 cap;
	int smem_alloc;
	int supports_ctrls;
	const struct mtk_dip_dev_format *fmts;
	int num_fmts;
	const char *description;
	int default_width;
	int default_height;
	unsigned int dma_port;
	const struct v4l2_frmsizeenum *frmsizeenum;
	const struct v4l2_ioctl_ops *ops;
	const struct vb2_ops *vb2_ops;
	u32 flags;
	int default_fmt_idx;
};

/**
 * struct mtk_dip_dev_queue - dip's video device queue
 * @vbq:	vb2_queue of the dip's video device
 * @lock:		serializes vb2 queue and video device operations.
 * @dev_fmt:	buffer format of the video device
 *
 * The structure supports a vb2_queue of dip's video device with the DIP's
 * internal buffer format.
 */
struct mtk_dip_dev_queue {
	struct vb2_queue vbq;
	/* Serializes vb2 queue and video device operations */
	struct mutex lock;
	const struct mtk_dip_dev_format *dev_fmt;
};

/**
 * struct mtk_dip_video_device - DIP's video device
 * @vdev:	video_device of the dip's video device
 * @dev_q:	the mtk_dip_dev_queue providing vb2 device queue for the
 *		video device
 * @vdev_fmt:	the current v4l2 format of the video device
 * @vdev_pad:	the pad connected to the dip sub device of the pipe
 * @pad_fmt:	the pad format of vdev_pad
 * @ctrl_handler: the v4l2_ctrl_handler of the video device. Only the video
 *		  device supporting rotation initialized the handler.
 * @flags:	the flags recording the link status between the video device
 *		and the sub device of the pipe
 * @desc:	setting of the video device. The driver initialize the video
 *		device according to the settings.
 * @buf_list:	the list of vb2 buffers enqueued through this video device
 * @buf_list_lock: protect the in-device buffer list
 * @crop:	crop setting of the video device
 * @compose:	compose setting the video device
 * @rotation:	rotation setting of the video device
 *
 * The structure extends video_device and integrates the vb2 queue, a media_pad
 * connected to DIP's sub device, and a v4l2_ctrl_handler to handling ctrl.
 */
struct mtk_dip_video_device {
	struct video_device vdev;
	struct mtk_dip_dev_queue dev_q;
	struct v4l2_format vdev_fmt;
	struct media_pad vdev_pad;
	struct v4l2_mbus_framefmt pad_fmt;
	struct v4l2_ctrl_handler ctrl_handler;
	u32 flags;
	const struct mtk_dip_video_device_desc *desc;
	struct list_head buf_list;
	/* the list of vb2 buffers enqueued through this video device */
	spinlock_t buf_list_lock;
	struct v4l2_rect crop;
	struct v4l2_rect compose;
	int rotation;
};

/**
 * struct mtk_dip_pipe - DIP's pipe
 * @dip_dev:	the dip driver device instance
 * @mtk_dip_video_device: the video devices of the pipe. The entry must be NULL
 *			  if there is no video devices for the ID
 * @nodes_streaming:	bitmask records the video devices which are streaming
 * @nodes_enabled:	bitmask records the video devices which are enabled
 * @streaming:		true if the pipe is streaming
 * @subdev:		sub device connected to the output and capture video
 *			device named as the pipe's name
 * @pipe_job_sequence:	the last sequence number of the pipe jobs
 * @pipe_job_pending_list: the list saving jobs before it has been put into
 *			   running state by mtk_dip_pipe_try_enqueue().
 * @num_pending_jobs:	number of jobs in pipe_job_pending_list
 * @pipe_job_running_list: the list saving jobs already scheduled into DIP
 * @num_jobs:		number of jobs in pipe_job_running_list
 * @lock:		serializes pipe's stream on/off and buffers enqueue
 *			operations
 * @job_lock:		protect the pipe job list
 * @desc:		the settings of the pipe
 *
 * The structure represents a DIP pipe. A pipe may contains a raw output
 * video device and at least one MDP capture device.
 */
struct mtk_dip_pipe {
	struct mtk_dip_dev *dip_dev;
	struct mtk_dip_video_device nodes[MTK_DIP_VIDEO_NODE_ID_TOTAL_NUM];
	unsigned int nodes_streaming;
	unsigned int nodes_enabled;
	int streaming;
	struct media_pad *subdev_pads;
	struct media_pipeline pipeline;
	struct v4l2_subdev subdev;
	atomic_t pipe_job_sequence;
	struct list_head pipe_job_pending_list;
	int num_pending_jobs;
	struct list_head pipe_job_running_list;
	int num_jobs;
	/*serializes pipe's stream on/off and buffers enqueue operations*/
	struct mutex lock;
	spinlock_t job_lock; /* protect the pipe job list */
	const struct mtk_dip_pipe_desc *desc;
};

/**
 * struct mtk_dip_dev - DIP's device instance
 * @dev:	the device structure of DIP
 * @mdev:	media device of DIP. All video devices and sub devices of
 *		DIP are registered to the media device.
 * @v4l2_dev:	v4l2_device representing the device-level status of DIP
 * @dip_pipe:	pipes of the DIP device. For example, capture, preview and
 *		reprocessing pipes.
 * @clks:	clocks required by DIP hardware
 * @num_clks:	the total number of clocks of DIP hardware
 * @composer_wq:	The work queue for jobs which are going to be sent to
 *			coprocessor.
 * @num_composing: number of jobs in SCP
 * @mdp_wq:	the work queue for jobs which are going to be sent to MDP
 *		driver and GCE hardware.
 *
 * @flushing_waitq:	the waiting queue to keep the process which are
 *			waiting for the jobs in SCP to be finished.
 * @mdpcb_wq:	the work queue for jobs with abnormal status back from MDP/GCE
 *		, it need to pass to SCP to check the error status instead of
 *		returning the buffer to user directly.
 * @mdp_pdev:	mdp platform device which handling the MDP part jobs and
 *		pass the task to GCE hardware.
 * @scp_pdev:	SCP platform device which handling the commands to and from
 *		coprocessor
 * @rproc_handle:	remote processor handle to control SCP
 * @dip_freebufferlist:	free working buffer list
 * @working_buf_mem_scp_daddr:	the SCP caddress of the memory area of working
 *				buffers
 * @working_buf_mem_vaddr:	the cpu address of the memory area of working
 *				buffers
 * @working_buf_mem_isp_daddr:	the isp dma address of the memory area of
 *				working buffers
 * @working_buf_mem_size:	total size in bytes of the memory area of
 *				working buffers
 * @working_buf:	the working buffers of DIP
 *
 * @dip_enqueue_cnt:	it is used to create the sequence number of the job
 *			which is already enqueued to DIP.
 * @dip_stream_cnt:	the number of streaming pipe in DIP
 * @hw_op_lock:		serialize request operation to DIP coprocessor and
 *			hardware
 * @sem:		To restrict the max number of request be send to SCP.
 *
 * The structure maintains DIP's device level status.
 */
struct mtk_dip_dev {
	struct device *dev;
	struct media_device mdev;
	struct v4l2_device v4l2_dev;
	struct mtk_dip_pipe dip_pipe[MTK_DIP_PIPE_ID_TOTAL_NUM];
	struct clk_bulk_data clks[MTK_DIP_CLK_NUM];
	int num_clks;
	struct workqueue_struct *composer_wq;
	struct workqueue_struct *mdp_wq;
	wait_queue_head_t flushing_waitq;
	atomic_t num_composing;
	struct workqueue_struct *mdpcb_wq;
	struct platform_device *mdp_pdev;
	struct platform_device *scp_pdev;
	struct rproc *rproc_handle;
	struct mtk_dip_hw_working_buf_list dip_freebufferlist;
	dma_addr_t working_buf_mem_scp_daddr;
	void *working_buf_mem_vaddr;
	dma_addr_t working_buf_mem_isp_daddr;
	int working_buf_mem_size;
	struct mtk_dip_hw_subframe working_buf[DIP_SUB_FRM_DATA_NUM];
	atomic_t dip_enqueue_cnt;
	int dip_stream_cnt;
	/* To serialize request opertion to DIP co-procrosser and hadrware */
	struct mutex hw_op_lock;
	/* To restrict the max number of request be send to SCP */
	struct semaphore sem;
};

/**
 * struct mtk_dip_request - DIP's request
 * @req:	the media_request object of the request
 * @dip_pipe:	the pipe owning of the request; a request can only belongs one
 *		DIP pipe
 * @id:		the unique job id in DIP
 * @buf_map:	the buffers of the request. The entry should be NULL if the
 *		corresponding video device doesn't enqueue the buffer.
 * @img_fparam:	frame related parameters which will be passed to coprocessor
 * @fw_work:	work_struct used to be sent to composer_wq of mtk_dip_dev
 * @mdp_work:	work_struct used to be sent to mdp_wq of mtk_dip_dev
 * @mdpcb_work:	work_struct used to be sent to mdpcb_wq of mtk_dip_dev.
 *		It is used only in error handling flow.
 * @working_buf: working buffer of the request
 * @buf_count:	the number of buffers in the request
 *
 * The structure extends media_request and integrates a map of the buffers,
 * and the working buffer pointers. It is the job instance used in DIP's
 * drivers.
 */
struct mtk_dip_request {
	struct media_request req;
	struct mtk_dip_pipe *dip_pipe;
	int id;
	struct mtk_dip_dev_buffer *buf_map[MTK_DIP_VIDEO_NODE_ID_TOTAL_NUM];
	struct img_frameparam img_fparam;
	struct work_struct fw_work;
	struct work_struct mdp_work;
	struct work_struct mdpcb_work;
	struct mtk_dip_hw_subframe *working_buf;
	atomic_t buf_count;
	struct list_head list;
};

int mtk_dip_dev_media_register(struct device *dev,
			       struct media_device *media_dev);

void mtk_dip_dev_v4l2_release(struct mtk_dip_dev *dip_dev);

int mtk_dip_dev_v4l2_register(struct device *dev,
			      struct media_device *media_dev,
			      struct v4l2_device *v4l2_dev);

int mtk_dip_pipe_v4l2_register(struct mtk_dip_pipe *pipe,
			       struct media_device *media_dev,
			       struct v4l2_device *v4l2_dev);

void mtk_dip_pipe_v4l2_unregister(struct mtk_dip_pipe *pipe);

int mtk_dip_pipe_queue_buffers(struct media_request *req, int initial);

int mtk_dip_pipe_init(struct mtk_dip_dev *dip_dev, struct mtk_dip_pipe *pipe,
		      const struct mtk_dip_pipe_desc *setting);

void mtk_dip_pipe_ipi_params_config(struct mtk_dip_request *req);

int mtk_dip_pipe_release(struct mtk_dip_pipe *pipe);

struct mtk_dip_request *
mtk_dip_pipe_get_running_job(struct mtk_dip_pipe *pipe,
			     int id);

void mtk_dip_pipe_remove_job(struct mtk_dip_request *req);

int mtk_dip_pipe_next_job_id(struct mtk_dip_pipe *pipe);

void mtk_dip_pipe_debug_job(struct mtk_dip_pipe *pipe,
			    struct mtk_dip_request *req);

void mtk_dip_pipe_job_finish(struct mtk_dip_request *req,
			     enum vb2_buffer_state vbf_state);

const struct mtk_dip_dev_format *
mtk_dip_pipe_find_fmt(struct mtk_dip_pipe *pipe,
		      struct mtk_dip_video_device *node,
		      u32 format);

void mtk_dip_pipe_try_fmt(struct mtk_dip_pipe *pipe,
			  struct mtk_dip_video_device *node,
			  struct v4l2_format *fmt,
			  const struct v4l2_format *ufmt,
			  const struct mtk_dip_dev_format *dfmt);

void mtk_dip_pipe_load_default_fmt(struct mtk_dip_pipe *pipe,
				   struct mtk_dip_video_device *node,
				   struct v4l2_format *fmt_to_fill);

void mtk_dip_pipe_try_enqueue(struct mtk_dip_pipe *pipe);

void mtk_dip_hw_enqueue(struct mtk_dip_dev *dip_dev,
			struct mtk_dip_request *req);

int mtk_dip_hw_streamoff(struct mtk_dip_pipe *pipe);

int mtk_dip_hw_streamon(struct mtk_dip_pipe *pipe);

int mtk_dip_hw_working_buf_pool_init(struct mtk_dip_dev *dip_dev);

void mtk_dip_hw_working_buf_pool_release(struct mtk_dip_dev *dip_dev);

static inline struct mtk_dip_pipe*
mtk_dip_dev_get_pipe(struct mtk_dip_dev *dip_dev, unsigned int pipe_id)
{
	if (pipe_id < 0 && pipe_id >= MTK_DIP_PIPE_ID_TOTAL_NUM)
		return NULL;

	return &dip_dev->dip_pipe[pipe_id];
}

static inline struct mtk_dip_video_device*
mtk_dip_file_to_node(struct file *file)
{
	return container_of(video_devdata(file),
			    struct mtk_dip_video_device, vdev);
}

static inline struct mtk_dip_pipe*
mtk_dip_subdev_to_pipe(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mtk_dip_pipe, subdev);
}

static inline struct mtk_dip_dev*
mtk_dip_mdev_to_dev(struct media_device *mdev)
{
	return container_of(mdev, struct mtk_dip_dev, mdev);
}

static inline struct mtk_dip_video_device*
mtk_dip_vbq_to_node(struct vb2_queue *vq)
{
	return container_of(vq, struct mtk_dip_video_device, dev_q.vbq);
}

static inline struct mtk_dip_dev_buffer*
mtk_dip_vb2_buf_to_dev_buf(struct vb2_buffer *vb)
{
	return container_of(vb, struct mtk_dip_dev_buffer, vbb.vb2_buf);
}

static inline struct mtk_dip_request*
mtk_dip_media_req_to_dip_req(struct media_request *req)
{
	return container_of(req, struct mtk_dip_request, req);
}

static inline struct mtk_dip_request*
mtk_dip_hw_fw_work_to_req(struct work_struct *fw_work)
{
	return container_of(fw_work, struct mtk_dip_request, fw_work);
}

static inline struct mtk_dip_request*
mtk_dip_hw_mdp_work_to_req(struct work_struct *mdp_work)
{
	return container_of(mdp_work, struct mtk_dip_request, mdp_work);
}

static inline struct mtk_dip_request *
mtk_dip_hw_mdpcb_work_to_req(struct work_struct *mdpcb_work)
{
	return container_of(mdpcb_work, struct mtk_dip_request, mdpcb_work);
}

static inline int mtk_dip_buf_is_meta(u32 type)
{
	return type == V4L2_BUF_TYPE_META_CAPTURE ||
		type == V4L2_BUF_TYPE_META_OUTPUT;
}

static inline int mtk_dip_pipe_get_pipe_from_job_id(int id)
{
	return (id >> 16) & 0x0000ffff;
}

static inline void
mtk_dip_wbuf_to_ipi_img_sw_addr(struct img_sw_addr *ipi_addr,
				struct mtk_dip_hw_working_buf *wbuf)
{
	ipi_addr->pa = (u32)wbuf->scp_daddr;
}

static inline void
mtk_dip_wbuf_to_ipi_img_addr(struct img_addr *ipi_addr,
			     struct mtk_dip_hw_working_buf *wbuf)
{
	ipi_addr->pa = (u32)wbuf->scp_daddr;
	ipi_addr->iova = (u32)wbuf->isp_daddr;
}

static inline void
mtk_dip_wbuf_to_ipi_tuning_addr(struct tuning_addr *ipi_addr,
				struct mtk_dip_hw_working_buf *wbuf)
{
	ipi_addr->pa = (u32)wbuf->scp_daddr;
	ipi_addr->iova = (u32)wbuf->isp_daddr;
}

#endif /* _MTK_DIP_DEV_H_ */
