// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_cam.h"
#include "mtk_cam-hw.h"

#define R_IMGO		BIT(0)
#define R_RRZO		BIT(1)
#define R_AAO		BIT(3)
#define R_AFO		BIT(4)
#define R_LCSO		BIT(5)
#define R_LMVO		BIT(7)
#define R_FLKO		BIT(8)
#define R_PSO		BIT(10)

#define MTK_ISP_ONE_PIXEL_MODE		1
#define MTK_ISP_MIN_RESIZE_RATIO	6
#define MTK_ISP_MAX_RUNNING_JOBS	3

#define MTK_CAM_CIO_PAD_SRC		4
#define MTK_CAM_CIO_PAD_SINK		11

static inline struct mtk_cam_video_device *
file_to_mtk_cam_node(struct file *__file)
{
	return container_of(video_devdata(__file),
		struct mtk_cam_video_device, vdev);
}

static inline struct mtk_cam_video_device *
mtk_cam_vbq_to_vdev(struct vb2_queue *__vq)
{
	return container_of(__vq, struct mtk_cam_video_device, vbq);
}

static inline struct mtk_cam_dev_request *
mtk_cam_req_to_dev_req(struct media_request *__req)
{
	return container_of(__req, struct mtk_cam_dev_request, req);
}

static inline struct mtk_cam_dev_buffer *
mtk_cam_vb2_buf_to_dev_buf(struct vb2_buffer *__vb)
{
	return container_of(__vb, struct mtk_cam_dev_buffer, vbb.vb2_buf);
}

static void mtk_cam_dev_job_done(struct mtk_cam_dev *cam,
				 struct mtk_cam_dev_request *req,
				 enum vb2_buffer_state state)
{
	struct media_request_object *obj, *obj_prev;
	unsigned long flags;
	u64 ts_eof = ktime_get_boot_ns();

	if (!cam->streaming)
		return;

	dev_dbg(cam->dev, "job done request:%s frame_seq:%d state:%d\n",
		req->req.debug_str, req->frame_params.frame_seq_no, state);

	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_dev_buffer *buf;
		struct mtk_cam_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		spin_lock_irqsave(&node->buf_list_lock, flags);
		list_del(&buf->list);
		spin_unlock_irqrestore(&node->buf_list_lock, flags);
		buf->vbb.sequence = req->frame_params.frame_seq_no;
		if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
			vb->timestamp = ts_eof;
		else
			vb->timestamp = req->timestamp;
		vb2_buffer_done(&buf->vbb.vb2_buf, state);
	}
}

void mtk_cam_dev_dequeue_frame(struct mtk_cam_dev *cam,
			       unsigned int node_id, unsigned int frame_seq_no,
			       int vb2_index)
{
	struct device *dev = cam->dev;
	struct mtk_cam_video_device *node = &cam->vdev_nodes[node_id];
	struct mtk_cam_dev_buffer *buf, *buf_prev;
	struct vb2_buffer *vb;
	unsigned long flags;

	if (!cam->vdev_nodes[node_id].enabled || !cam->streaming)
		return;

	spin_lock_irqsave(&node->buf_list_lock, flags);
	list_for_each_entry_safe(buf, buf_prev, &node->buf_list, list) {
		vb = &buf->vbb.vb2_buf;
		if (!vb->vb2_queue->uses_requests &&
		    vb->index == vb2_index) {
			dev_dbg(dev, "%s:%d:%d", __func__, node_id, vb2_index);
			vb->timestamp = ktime_get_boot_ns();
			/* AFO seq. num usage */
			buf->vbb.sequence = frame_seq_no;
			list_del(&buf->list);
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			break;
		}
	}
	spin_unlock_irqrestore(&node->buf_list_lock, flags);
}

struct mtk_cam_dev_request *mtk_cam_dev_get_req(struct mtk_cam_dev *cam,
						unsigned int frame_seq_no)
{
	struct mtk_cam_dev_request *req, *req_prev;
	unsigned long flags;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		dev_dbg(cam->dev, "frame_seq:%d, get frame_seq:%d\n",
			req->frame_params.frame_seq_no, frame_seq_no);

		/* Match by the en-queued request number */
		if (req->frame_params.frame_seq_no == frame_seq_no) {
			spin_unlock_irqrestore(&cam->running_job_lock, flags);
			return req;
		}
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);

	return NULL;
}
void mtk_cam_dev_dequeue_req_frame(struct mtk_cam_dev *cam,
				   unsigned int frame_seq_no)
{
	struct mtk_cam_dev_request *req, *req_prev;
	unsigned long flags;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		dev_dbg(cam->dev, "frame_seq:%d, de-queue frame_seq:%d\n",
			req->frame_params.frame_seq_no, frame_seq_no);

		/* Match by the en-queued request number */
		if (req->frame_params.frame_seq_no == frame_seq_no) {
			cam->running_job_count--;
			/* Pass to user space */
			mtk_cam_dev_job_done(cam, req, VB2_BUF_STATE_DONE);
			list_del(&req->list);
			break;
		} else if (req->frame_params.frame_seq_no < frame_seq_no) {
			cam->running_job_count--;
			/* Pass to user space for frame drop */
			mtk_cam_dev_job_done(cam, req, VB2_BUF_STATE_ERROR);
			dev_warn(cam->dev, "frame_seq:%d drop\n",
				 req->frame_params.frame_seq_no);
			list_del(&req->list);
		} else {
			break;
		}
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
}

static void mtk_cam_dev_req_cleanup(struct mtk_cam_dev *cam)
{
	struct mtk_cam_dev_request *req, *req_prev;
	unsigned long flags;

	dev_dbg(cam->dev, "%s\n", __func__);

	spin_lock_irqsave(&cam->pending_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list)
		list_del(&req->list);
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list)
		list_del(&req->list);
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
}

void mtk_cam_dev_req_try_queue(struct mtk_cam_dev *cam)
{
	struct mtk_cam_dev_request *req, *req_prev;
	unsigned long flags;

	if (!cam->streaming) {
		dev_dbg(cam->dev, "stream is off\n");
		return;
	}

	spin_lock_irqsave(&cam->pending_job_lock, flags);
	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list) {
		if (cam->running_job_count >= MTK_ISP_MAX_RUNNING_JOBS) {
			dev_dbg(cam->dev, "jobs are full\n");
			break;
		}
		cam->running_job_count++;
		list_del(&req->list);
		list_add_tail(&req->list, &cam->running_job_list);
		mtk_isp_req_enqueue(cam, req);
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_dev_request *cam_dev_req;

	cam_dev_req = kzalloc(sizeof(*cam_dev_req), GFP_KERNEL);

	return &cam_dev_req->req;
}

static void mtk_cam_req_free(struct media_request *req)
{
	struct mtk_cam_dev_request *cam_dev_req = mtk_cam_req_to_dev_req(req);

	kfree(cam_dev_req);
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_dev_request *cam_req = mtk_cam_req_to_dev_req(req);
	struct mtk_cam_dev *cam = container_of(req->mdev, struct mtk_cam_dev,
					       media_dev);
	unsigned long flags;

	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	/* add to pending job list */
	spin_lock_irqsave(&cam->pending_job_lock, flags);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);

	mtk_cam_dev_req_try_queue(cam);
}

static unsigned int get_pixel_bits(unsigned int pix_fmt)
{
	switch (pix_fmt) {
	case V4L2_PIX_FMT_MTISP_SBGGR8:
	case V4L2_PIX_FMT_MTISP_SGBRG8:
	case V4L2_PIX_FMT_MTISP_SGRBG8:
	case V4L2_PIX_FMT_MTISP_SRGGB8:
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		return 8;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		return 10;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		return 12;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		return 14;
	default:
		return 0;
	}
}

static void cal_image_pix_mp(struct mtk_cam_dev *cam, unsigned int node_id,
			     struct v4l2_pix_format_mplane *mp)
{
	unsigned int bpl, ppl;
	unsigned int pixel_bits = get_pixel_bits(mp->pixelformat);
	unsigned int width = mp->width;

	bpl = 0;
	if (node_id == MTK_CAM_P1_MAIN_STREAM_OUT) {
		/* Bayer encoding format & 2 bytes alignment */
		bpl = ALIGN(DIV_ROUND_UP(width * pixel_bits, 8), 2);
	} else if (node_id == MTK_CAM_P1_PACKED_BIN_OUT) {
		/*
		 * The FULL-G encoding format
		 * 1 G component per pixel
		 * 1 R component per 4 pixel
		 * 1 B component per 4 pixel
		 * Total 4G/1R/1B in 4 pixel (pixel per line:ppl)
		 */
		ppl = DIV_ROUND_UP(width * 6, 4);
		bpl = DIV_ROUND_UP(ppl * pixel_bits, 8);

		/* 4 bytes alignment for 10 bit & others are 8 bytes */
		if (pixel_bits == 10)
			bpl = ALIGN(bpl, 4);
		else
			bpl = ALIGN(bpl, 8);
	}
	/*
	 * This image output buffer will be input buffer of MTK CAM DIP HW
	 * For MTK CAM DIP HW constrained, it needs 4 bytes alignment
	 */
	bpl = ALIGN(bpl, 4);

	mp->plane_fmt[0].bytesperline = bpl;
	mp->plane_fmt[0].sizeimage = bpl * mp->height;

	dev_dbg(cam->dev, "node:%d width:%d bytesperline:%d sizeimage:%d\n",
		node_id, width, bpl, mp->plane_fmt[0].sizeimage);
}

static const struct v4l2_format *
mtk_cam_dev_find_fmt(struct mtk_cam_dev_node_desc *desc, u32 format)
{
	int i;
	const struct v4l2_format *dev_fmt;

	for (i = 0; i < desc->num_fmts; i++) {
		dev_fmt = &desc->fmts[i];
		if (dev_fmt->fmt.pix_mp.pixelformat == format)
			return dev_fmt;
	}

	return NULL;
}

/* Get the default format setting */
static void
mtk_cam_dev_load_default_fmt(struct mtk_cam_dev *cam,
			     struct mtk_cam_dev_node_desc *queue_desc,
			     struct v4l2_format *dest)
{
	const struct v4l2_format *default_fmt =
		&queue_desc->fmts[queue_desc->default_fmt_idx];

	dest->type = queue_desc->buf_type;

	/* Configure default format based on node type */
	if (!queue_desc->image) {
		dest->fmt.meta.dataformat = default_fmt->fmt.meta.dataformat;
		dest->fmt.meta.buffersize = default_fmt->fmt.meta.buffersize;
		return;
	}

	dest->fmt.pix_mp.pixelformat = default_fmt->fmt.pix_mp.pixelformat;
	dest->fmt.pix_mp.width = default_fmt->fmt.pix_mp.width;
	dest->fmt.pix_mp.height = default_fmt->fmt.pix_mp.height;
	/* bytesperline & sizeimage calculation */
	cal_image_pix_mp(cam, queue_desc->id, &dest->fmt.pix_mp);
	dest->fmt.pix_mp.num_planes = 1;

	dest->fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	dest->fmt.pix_mp.field = V4L2_FIELD_NONE;
	dest->fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	dest->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	dest->fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;
}

/* Utility functions */
static unsigned int get_sensor_pixel_id(unsigned int fmt)
{
	switch (fmt) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		return MTK_CAM_RAW_PXL_ID_B;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		return MTK_CAM_RAW_PXL_ID_GB;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		return MTK_CAM_RAW_PXL_ID_GR;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTK_CAM_RAW_PXL_ID_R;
	default:
		return MTK_CAM_RAW_PXL_ID_UNKNOWN;
	}
}

static unsigned int get_sensor_fmt(unsigned int fmt)
{
	switch (fmt) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return MTK_CAM_IMG_FMT_BAYER8;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return MTK_CAM_IMG_FMT_BAYER10;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return MTK_CAM_IMG_FMT_BAYER12;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTK_CAM_IMG_FMT_BAYER14;
	default:
		return MTK_CAM_IMG_FMT_UNKNOWN;
	}
}

static unsigned int get_img_fmt(unsigned int fourcc)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_MTISP_SBGGR8:
	case V4L2_PIX_FMT_MTISP_SGBRG8:
	case V4L2_PIX_FMT_MTISP_SGRBG8:
	case V4L2_PIX_FMT_MTISP_SRGGB8:
		return MTK_CAM_IMG_FMT_BAYER8;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		return MTK_CAM_IMG_FMT_FG_BAYER8;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		return MTK_CAM_IMG_FMT_BAYER10;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		return MTK_CAM_IMG_FMT_FG_BAYER10;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		return MTK_CAM_IMG_FMT_BAYER12;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		return MTK_CAM_IMG_FMT_FG_BAYER12;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		return MTK_CAM_IMG_FMT_BAYER14;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		return MTK_CAM_IMG_FMT_FG_BAYER14;
	default:
		return MTK_CAM_IMG_FMT_UNKNOWN;
	}
}

static int config_img_fmt(struct mtk_cam_dev *cam, unsigned int node_id,
			  struct p1_img_output *out_fmt, int sd_width,
			  int sd_height)
{
	const struct v4l2_format *cfg_fmt = &cam->vdev_nodes[node_id].vdev_fmt;

	/* Check output & input image size dimension */
	if (cfg_fmt->fmt.pix_mp.width > sd_width ||
	    cfg_fmt->fmt.pix_mp.height > sd_height) {
		dev_err(cam->dev, "node:%d cfg size is larger than sensor\n",
			node_id);
		return -EINVAL;
	}

	/* Check resize ratio for resize out stream due to HW constraint */
	if (((cfg_fmt->fmt.pix_mp.width * 100 / sd_width) <
	    MTK_ISP_MIN_RESIZE_RATIO) ||
	    ((cfg_fmt->fmt.pix_mp.height * 100 / sd_height) <
	    MTK_ISP_MIN_RESIZE_RATIO)) {
		dev_err(cam->dev, "node:%d resize ratio is less than %d%%\n",
			node_id, MTK_ISP_MIN_RESIZE_RATIO);
		return -EINVAL;
	}

	out_fmt->img_fmt = get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	out_fmt->pixel_bits = get_pixel_bits(cfg_fmt->fmt.pix_mp.pixelformat);
	if (out_fmt->img_fmt == MTK_CAM_IMG_FMT_UNKNOWN ||
	    !out_fmt->pixel_bits) {
		dev_err(cam->dev, "node:%d unknown pixel fmt:%d\n",
			node_id, cfg_fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	dev_dbg(cam->dev, "node:%d pixel_bits:%d img_fmt:0x%x\n",
		node_id, out_fmt->pixel_bits, out_fmt->img_fmt);

	out_fmt->size.w = cfg_fmt->fmt.pix_mp.width;
	out_fmt->size.h = cfg_fmt->fmt.pix_mp.height;
	out_fmt->size.stride = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
	out_fmt->size.xsize = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

	out_fmt->crop.left = 0;
	out_fmt->crop.top = 0;
	out_fmt->crop.width = sd_width;
	out_fmt->crop.height = sd_height;

	dev_dbg(cam->dev,
		"node:%d size=%0dx%0d, stride:%d, xsize:%d, crop=%0dx%0d\n",
		node_id, out_fmt->size.w, out_fmt->size.h,
		out_fmt->size.stride, out_fmt->size.xsize,
		out_fmt->crop.width, out_fmt->crop.height);

	return 0;
}

static void mtk_cam_dev_init_stream(struct mtk_cam_dev *cam)
{
	int i;

	cam->enabled_count = 0;
	cam->enabled_dmas = 0;
	cam->stream_count = 0;
	cam->running_job_count = 0;

	/* Get the enabled meta DMA ports */
	for (i = 0; i < MTK_CAM_P1_TOTAL_NODES; i++) {
		if (!cam->vdev_nodes[i].enabled)
			continue;
		cam->enabled_count++;
		cam->enabled_dmas |= cam->vdev_nodes[i].desc.dma_port;
	}

	dev_dbg(cam->dev, "%s:%d:0x%x\n", __func__, cam->enabled_count,
		cam->enabled_dmas);
}

static int mtk_cam_dev_isp_config(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	struct p1_config_param config_param;
	struct cfg_in_param *cfg_in_param;
	struct v4l2_subdev_format sd_fmt;
	int sd_width, sd_height, sd_code;
	unsigned int enabled_dma_ports = cam->enabled_dmas;
	int ret;

	/* Get sensor format configuration */
	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(cam->sensor, pad, get_fmt, NULL, &sd_fmt);
	if (ret) {
		dev_dbg(dev, "sensor g_fmt failed:%d\n", ret);
		return ret;
	}
	sd_width = sd_fmt.format.width;
	sd_height = sd_fmt.format.height;
	sd_code = sd_fmt.format.code;
	dev_dbg(dev, "sd fmt w*h=%d*%d, code=0x%x\n", sd_width, sd_height,
		sd_code);

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.cfg_in_param;
	cfg_in_param->continuous = true;
	/* Fix to one pixel mode in default */
	cfg_in_param->pixel_mode = MTK_ISP_ONE_PIXEL_MODE;
	cfg_in_param->crop.width = sd_width;
	cfg_in_param->crop.height = sd_height;
	cfg_in_param->raw_pixel_id = get_sensor_pixel_id(sd_code);
	cfg_in_param->img_fmt = get_sensor_fmt(sd_code);
	if (cfg_in_param->img_fmt == MTK_CAM_IMG_FMT_UNKNOWN ||
	    cfg_in_param->raw_pixel_id == MTK_CAM_RAW_PXL_ID_UNKNOWN) {
		dev_err(dev, "unknown sd code:%d\n", sd_code);
		return -EINVAL;
	}

	/* Update cfg_main_param */
	config_param.cfg_main_param.pure_raw = true;
	config_param.cfg_main_param.pure_raw_pack = true;
	ret = config_img_fmt(cam, MTK_CAM_P1_MAIN_STREAM_OUT,
			     &config_param.cfg_main_param.output,
			     sd_width, sd_height);
	if (ret)
		return ret;

	/* Update cfg_resize_param */
	if (enabled_dma_ports & R_RRZO) {
		ret = config_img_fmt(cam, MTK_CAM_P1_PACKED_BIN_OUT,
				     &config_param.cfg_resize_param.output,
				     sd_width, sd_height);
		if (ret)
			return ret;
	} else {
		config_param.cfg_resize_param.bypass = true;
	}

	/* Update enabled_dmas */
	config_param.enabled_dmas = enabled_dma_ports;
	mtk_isp_hw_config(cam, &config_param);
	dev_dbg(dev, "%s done\n", __func__);

	return 0;
}

void mtk_cam_dev_event_frame_sync(struct mtk_cam_dev *cam,
				  unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};

	v4l2_event_queue(cam->subdev.devnode, &event);
}

static struct v4l2_subdev *
mtk_cam_cio_get_active_sensor(struct mtk_cam_dev *cam)
{
	struct media_device *mdev = cam->seninf->entity.graph_obj.mdev;
	struct device *dev = cam->dev;
	struct media_entity *entity;
	struct v4l2_subdev *sensor;

	sensor = NULL;
	media_device_for_each_entity(entity, mdev) {
		dev_dbg(dev, "media entity: %s:0x%x:%d\n",
			entity->name, entity->function, entity->stream_count);
		if (entity->function == MEDIA_ENT_F_CAM_SENSOR &&
		    entity->stream_count) {
			sensor = media_entity_to_v4l2_subdev(entity);
			dev_dbg(dev, "sensor found: %s\n", entity->name);
			break;
		}
	}

	if (!sensor)
		dev_err(dev, "no seninf connected\n");

	return sensor;
}

static int mtk_cam_cio_stream_on(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	int ret;

	if (!cam->seninf) {
		dev_err(dev, "no seninf connected\n");
		return -ENODEV;
	}

	/* Get active sensor from graph topology */
	cam->sensor = mtk_cam_cio_get_active_sensor(cam);
	if (!cam->sensor)
		return -ENODEV;

	/* Seninf must stream on first */
	ret = v4l2_subdev_call(cam->seninf, video, s_stream, 1);
	if (ret) {
		dev_err(dev, "failed to stream on %s:%d\n",
			cam->seninf->entity.name, ret);
		return ret;
	}

	ret = v4l2_subdev_call(cam->sensor, video, s_stream, 1);
	if (ret) {
		dev_err(dev, "failed to stream on %s:%d\n",
			cam->sensor->entity.name, ret);
		goto fail_seninf_off;
	}

	ret = mtk_cam_dev_isp_config(cam);
	if (ret)
		goto fail_sensor_off;

	cam->streaming = true;
	mtk_isp_stream(cam, 1);
	mtk_cam_dev_req_try_queue(cam);
	dev_dbg(dev, "streamed on Pass 1\n");

	return 0;

fail_sensor_off:
	v4l2_subdev_call(cam->sensor, video, s_stream, 0);
fail_seninf_off:
	v4l2_subdev_call(cam->seninf, video, s_stream, 0);

	return ret;
}

static int mtk_cam_cio_stream_off(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	int ret;

	ret = v4l2_subdev_call(cam->sensor, video, s_stream, 0);
	if (ret) {
		dev_err(dev, "failed to stream off %s:%d\n",
			cam->sensor->entity.name, ret);
		return -EPERM;
	}

	ret = v4l2_subdev_call(cam->seninf, video, s_stream, 0);
	if (ret) {
		dev_err(dev, "failed to stream off %s:%d\n",
			cam->seninf->entity.name, ret);
		return -EPERM;
	}

	cam->streaming = false;
	mtk_isp_stream(cam, 0);
	mtk_isp_hw_release(cam);

	dev_dbg(dev, "streamed off Pass 1\n");

	return 0;
}

static int mtk_cam_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_cam_dev *cam = container_of(sd, struct mtk_cam_dev, subdev);

	if (enable) {
		/* Align vb2_core_streamon design */
		if (cam->streaming) {
			dev_warn(cam->dev, "already streaming on\n");
			return 0;
		}
		return mtk_cam_cio_stream_on(cam);
	}

	if (!cam->streaming) {
		dev_warn(cam->dev, "already streaming off\n");
		return 0;
	}
	return mtk_cam_cio_stream_off(cam);
}

static int mtk_cam_sd_subscribe_event(struct v4l2_subdev *subdev,
				      struct v4l2_fh *fh,
				      struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return -EINVAL;
	}
}

static int mtk_cam_media_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	struct mtk_cam_dev *cam =
		container_of(entity, struct mtk_cam_dev, subdev.entity);
	u32 pad = local->index;

	dev_dbg(cam->dev, "%s: %d->%d flags:0x%x\n",
		__func__, pad, remote->index, flags);

	/*
	 * The video nodes exposed by the driver have pads indexes
	 * from 0 to MTK_CAM_P1_TOTAL_NODES - 1.
	 */
	if (pad < MTK_CAM_P1_TOTAL_NODES)
		cam->vdev_nodes[pad].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	return 0;
}

static void mtk_cam_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_cam_dev_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_dev_request *req = mtk_cam_req_to_dev_req(vb->request);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct device *dev = cam->dev;
	unsigned long flags;

	dev_dbg(dev, "%s: node:%d fd:%d idx:%d\n", __func__,
		node->id, buf->vbb.request_fd, buf->vbb.vb2_buf.index);

	/* added the buffer into the tracking list */
	spin_lock_irqsave(&node->buf_list_lock, flags);
	list_add_tail(&buf->list, &node->buf_list);
	spin_unlock_irqrestore(&node->buf_list_lock, flags);

	/*
	 * TODO(b/140397121): Remove non-request mode support when the HAL
	 * is fixed to use the Request API only.
	 *
	 * For request buffers en-queue, handled in mtk_cam_req_try_queue
	 */
	if (!vb->vb2_queue->uses_requests) {
		mutex_lock(&cam->op_lock);
		/* If node is not streame on, re-queued when stream on */
		if (vb->vb2_queue->streaming)
			mtk_isp_enqueue(cam, node->desc.dma_port, buf);
		mutex_unlock(&cam->op_lock);
		return;
	}

	/* update buffer internal address */
	req->frame_params.dma_bufs[buf->node_id].iova = buf->daddr;
	req->frame_params.dma_bufs[buf->node_id].scp_addr = buf->scp_addr;
}

static int mtk_cam_vb2_buf_init(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct device *dev = cam->dev;
	struct mtk_cam_dev_buffer *buf;
	dma_addr_t addr;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	buf->node_id = node->id;
	buf->daddr = vb2_dma_contig_plane_dma_addr(vb, 0);
	buf->scp_addr = 0;

	/* SCP address is only valid for meta input buffer */
	if (!node->desc.smem_alloc)
		return 0;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	/* Use coherent address to get iova address */
	addr = dma_map_resource(dev, buf->daddr, vb->planes[0].length,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(dev, addr)) {
		dev_err(dev, "failed to map meta addr:%pad\n", &buf->daddr);
		return -EFAULT;
	}
	buf->scp_addr = buf->daddr;
	buf->daddr = addr;

	return 0;
}

static int mtk_cam_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);
	const struct v4l2_format *fmt = &node->vdev_fmt;
	unsigned int size;

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_META_OUTPUT ||
	    vb->vb2_queue->type == V4L2_BUF_TYPE_META_CAPTURE)
		size = fmt->fmt.meta.buffersize;
	else
		size = fmt->fmt.pix_mp.plane_fmt[0].sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		dev_dbg(cam->dev, "plane size is too small:%lu<%u\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if (vb2_get_plane_payload(vb, 0) != size) {
			dev_dbg(cam->dev, "plane payload is mismatch:%lu:%u\n",
				vb2_get_plane_payload(vb, 0), size);
			return -EINVAL;
		}
		return 0;
	}

	v4l2_buf->field = V4L2_FIELD_NONE;
	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void mtk_cam_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_cam_dev_buffer *buf;
	struct device *dev = cam->dev;

	if (!node->desc.smem_alloc)
		return;

	buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	dma_unmap_page_attrs(dev, buf->daddr,
			     vb->planes[0].length,
			     DMA_BIDIRECTIONAL,
			     DMA_ATTR_SKIP_CPU_SYNC);
}

static void mtk_cam_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vb->vb2_queue);

	dev_dbg(cam->dev, "%s\n", __func__);
}

static int mtk_cam_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	unsigned int max_buffer_count = node->desc.max_buf_count;
	const struct v4l2_format *fmt = &node->vdev_fmt;
	unsigned int size;

	/* Check the limitation of buffer size */
	if (max_buffer_count)
		*num_buffers = clamp_val(*num_buffers, 1, max_buffer_count);

	vq->dma_attrs |= DMA_ATTR_NON_CONSISTENT;

	if (vq->type == V4L2_BUF_TYPE_META_OUTPUT ||
	    vq->type == V4L2_BUF_TYPE_META_CAPTURE)
		size = fmt->fmt.meta.buffersize;
	else
		size = fmt->fmt.pix_mp.plane_fmt[0].sizeimage;

	/* Add for q.create_bufs with fmt.g_sizeimage(p) / 2 test */
	if (*num_planes) {
		if (sizes[0] < size || *num_planes != 1)
			return -EINVAL;
	} else {
		*num_planes = 1;
		sizes[0] = size;
	}

	return 0;
}

static void mtk_cam_vb2_return_all_buffers(struct mtk_cam_dev *cam,
					   struct mtk_cam_video_device *node,
					   enum vb2_buffer_state state)
{
	struct mtk_cam_dev_buffer *buf, *buf_prev;
	unsigned long flags;

	spin_lock_irqsave(&node->buf_list_lock, flags);
	list_for_each_entry_safe(buf, buf_prev, &node->buf_list, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vbb.vb2_buf, state);
	}
	spin_unlock_irqrestore(&node->buf_list_lock, flags);
}

static int mtk_cam_vb2_start_streaming(struct vb2_queue *vq,
				       unsigned int count)
{
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct mtk_cam_dev_buffer *buf, *buf_prev;
	struct device *dev = cam->dev;
	int ret;

	if (!node->enabled) {
		dev_err(dev, "Node:%d is not enabled\n", node->id);
		ret = -ENOLINK;
		goto fail_ret_buf;
	}

	mutex_lock(&cam->op_lock);
	/* Start streaming of the whole pipeline now*/
	if (!cam->pipeline.streaming_count) {
		ret = media_pipeline_start(&node->vdev.entity, &cam->pipeline);
		if (ret) {
			dev_err(dev, "failed to start pipeline:%d\n", ret);
			goto fail_unlock;
		}
		mtk_cam_dev_init_stream(cam);
		ret = mtk_isp_hw_init(cam);
		if (ret) {
			dev_err(dev, "failed to init HW:%d\n", ret);
			goto fail_stop_pipeline;
		}
	}

	/*
	 * TODO(b/140397121): Remove non-request mode support when the HAL
	 * is fixed to use the Request API only.
	 *
	 * Need to make sure HW is initialized.
	 */
	if (!vq->uses_requests)
		list_for_each_entry_safe(buf, buf_prev, &node->buf_list, list)
			mtk_isp_enqueue(cam, node->desc.dma_port, buf);

	/* Media links are fixed after media_pipeline_start */
	cam->stream_count++;
	dev_dbg(dev, "%s: count info:%d:%d\n", __func__, cam->stream_count,
		cam->enabled_count);
	if (cam->stream_count < cam->enabled_count) {
		mutex_unlock(&cam->op_lock);
		return 0;
	}

	/* Stream on sub-devices node */
	ret = v4l2_subdev_call(&cam->subdev, video, s_stream, 1);
	if (ret)
		goto fail_no_stream;
	mutex_unlock(&cam->op_lock);

	return 0;

fail_no_stream:
	cam->stream_count--;
fail_stop_pipeline:
	if (cam->stream_count == 0)
		media_pipeline_stop(&node->vdev.entity);
fail_unlock:
	mutex_unlock(&cam->op_lock);
fail_ret_buf:
	mtk_cam_vb2_return_all_buffers(cam, node, VB2_BUF_STATE_QUEUED);

	return ret;
}

static void mtk_cam_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_cam_dev *cam = vb2_get_drv_priv(vq);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vq);
	struct device *dev = cam->dev;

	mutex_lock(&cam->op_lock);
	dev_dbg(dev, "%s node:%d count info:%d\n", __func__, node->id,
		cam->stream_count);
	/* Check the first node to stream-off & disable DMA */
	if (cam->stream_count == cam->enabled_count) {
		v4l2_subdev_call(&cam->subdev, video, s_stream, 0);
		cam->enabled_dmas = 0;
	}

	mtk_cam_vb2_return_all_buffers(cam, node, VB2_BUF_STATE_ERROR);
	cam->stream_count--;
	if (cam->stream_count) {
		mutex_unlock(&cam->op_lock);
		return;
	}
	mutex_unlock(&cam->op_lock);

	mtk_cam_dev_req_cleanup(cam);
	media_pipeline_stop(&node->vdev.entity);
}

static int mtk_cam_vidioc_querycap(struct file *file, void *fh,
				   struct v4l2_capability *cap)
{
	struct mtk_cam_dev *cam = video_drvdata(file);

	strscpy(cap->driver, dev_driver_string(cam->dev), sizeof(cap->driver));
	strscpy(cap->card, dev_driver_string(cam->dev), sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(cam->dev));

	return 0;
}

static int mtk_cam_vidioc_enum_fmt(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index >= node->desc.num_fmts)
		return -EINVAL;

	/* f->description is filled in v4l_fill_fmtdesc function */
	f->pixelformat = node->desc.fmts[f->index].fmt.pix_mp.pixelformat;
	f->flags = 0;

	return 0;
}

static int mtk_cam_vidioc_g_fmt(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	f->fmt = node->vdev_fmt.fmt;

	return 0;
}

static int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mtk_cam_dev *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);
	struct device *dev = cam->dev;
	const struct v4l2_format *dev_fmt;
	struct v4l2_format try_fmt;

	memset(&try_fmt, 0, sizeof(try_fmt));
	try_fmt.type = f->type;

	/* Validate pixelformat */
	dev_fmt = mtk_cam_dev_find_fmt(&node->desc, f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_dbg(dev, "unknown fmt:%d\n", f->fmt.pix_mp.pixelformat);
		dev_fmt = &node->desc.fmts[node->desc.default_fmt_idx];
	}
	try_fmt.fmt.pix_mp.pixelformat = dev_fmt->fmt.pix_mp.pixelformat;

	/* Validate image width & height range */
	try_fmt.fmt.pix_mp.width = clamp_val(f->fmt.pix_mp.width,
					     IMG_MIN_WIDTH, IMG_MAX_WIDTH);
	try_fmt.fmt.pix_mp.height = clamp_val(f->fmt.pix_mp.height,
					      IMG_MIN_HEIGHT, IMG_MAX_HEIGHT);
	/* 4 bytes alignment for width */
	try_fmt.fmt.pix_mp.width = ALIGN(try_fmt.fmt.pix_mp.width, 4);

	/* Only support one plane */
	try_fmt.fmt.pix_mp.num_planes = 1;

	/* bytesperline & sizeimage calculation */
	cal_image_pix_mp(cam, node->id, &try_fmt.fmt.pix_mp);

	/* Constant format fields */
	try_fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	try_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	try_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	try_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	try_fmt.fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	*f = try_fmt;

	return 0;
}

static int mtk_cam_vidioc_s_fmt(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_cam_dev *cam = video_drvdata(file);
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (vb2_is_busy(node->vdev.queue)) {
		dev_dbg(cam->dev, "%s: queue is busy\n", __func__);
		return -EBUSY;
	}

	/* Get the valid format */
	mtk_cam_vidioc_try_fmt(file, fh, f);
	/* Configure to video device */
	node->vdev_fmt = *f;

	return 0;
}

static int mtk_cam_vidioc_enum_framesizes(struct file *filp, void *priv,
					  struct v4l2_frmsizeenum *sizes)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(filp);
	const struct v4l2_format *dev_fmt;

	dev_fmt = mtk_cam_dev_find_fmt(&node->desc, sizes->pixel_format);
	if (!dev_fmt || sizes->index)
		return -EINVAL;

	sizes->type = node->desc.frmsizes->type;
	memcpy(&sizes->stepwise, &node->desc.frmsizes->stepwise,
	       sizeof(sizes->stepwise));

	return 0;
}

static int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	if (f->index)
		return -EINVAL;

	/* f->description is filled in v4l_fill_fmtdesc function */
	f->pixelformat = node->vdev_fmt.fmt.meta.dataformat;
	f->flags = 0;

	return 0;
}

static int mtk_cam_vidioc_g_meta_fmt(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct mtk_cam_video_device *node = file_to_mtk_cam_node(file);

	f->fmt.meta.dataformat = node->vdev_fmt.fmt.meta.dataformat;
	f->fmt.meta.buffersize = node->vdev_fmt.fmt.meta.buffersize;

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_cam_subdev_core_ops = {
	.subscribe_event = mtk_cam_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_cam_subdev_video_ops = {
	.s_stream =  mtk_cam_sd_s_stream,
};

static const struct v4l2_subdev_ops mtk_cam_subdev_ops = {
	.core = &mtk_cam_subdev_core_ops,
	.video = &mtk_cam_subdev_video_ops,
};

static const struct media_entity_operations mtk_cam_media_entity_ops = {
	.link_setup = mtk_cam_media_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct vb2_ops mtk_cam_vb2_ops = {
	.queue_setup = mtk_cam_vb2_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_init = mtk_cam_vb2_buf_init,
	.buf_prepare = mtk_cam_vb2_buf_prepare,
	.start_streaming = mtk_cam_vb2_start_streaming,
	.stop_streaming = mtk_cam_vb2_stop_streaming,
	.buf_queue = mtk_cam_vb2_buf_queue,
	.buf_cleanup = mtk_cam_vb2_buf_cleanup,
	.buf_request_complete = mtk_cam_vb2_request_complete,
};

static const struct v4l2_file_operations mtk_cam_v4l2_fops = {
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
};

static const struct media_device_ops mtk_cam_media_ops = {
	.req_alloc = mtk_cam_req_alloc,
	.req_free = mtk_cam_req_free,
	.req_validate = vb2_request_validate,
	.req_queue = mtk_cam_req_queue,
};

static int mtk_cam_media_register(struct mtk_cam_dev *cam,
				  struct media_device *media_dev)
{
	/* Reserved MTK_CAM_CIO_PAD_SINK + 1 pads to use */
	unsigned int num_pads = MTK_CAM_CIO_PAD_SINK + 1;
	struct device *dev = cam->dev;
	int i, ret;

	media_dev->dev = cam->dev;
	strscpy(media_dev->model, dev_driver_string(dev),
		sizeof(media_dev->model));
	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "platform:%s", dev_name(dev));
	media_dev->hw_revision = 0;
	media_device_init(media_dev);
	media_dev->ops = &mtk_cam_media_ops;

	ret = media_device_register(media_dev);
	if (ret) {
		dev_err(dev, "failed to register media device:%d\n", ret);
		return ret;
	}

	/* Initialize subdev pads */
	cam->subdev_pads = devm_kcalloc(dev, num_pads,
					sizeof(*cam->subdev_pads),
					GFP_KERNEL);
	if (!cam->subdev_pads) {
		dev_err(dev, "failed to allocate subdev_pads\n");
		ret = -ENOMEM;
		goto fail_media_unreg;
	}

	ret = media_entity_pads_init(&cam->subdev.entity, num_pads,
				     cam->subdev_pads);
	if (ret) {
		dev_err(dev, "failed to initialize media pads:%d\n", ret);
		goto fail_media_unreg;
	}

	/* Initialize all pads with MEDIA_PAD_FL_SOURCE */
	for (i = 0; i < num_pads; i++)
		cam->subdev_pads[i].flags = MEDIA_PAD_FL_SOURCE;

	/* Customize the last one pad as CIO sink pad. */
	cam->subdev_pads[MTK_CAM_CIO_PAD_SINK].flags = MEDIA_PAD_FL_SINK;

	return 0;

fail_media_unreg:
	media_device_unregister(&cam->media_dev);
	media_device_cleanup(&cam->media_dev);

	return ret;
}

static int
mtk_cam_video_register_device(struct mtk_cam_dev *cam,
			      struct mtk_cam_video_device *node)
{
	struct device *dev = cam->dev;
	struct video_device *vdev = &node->vdev;
	struct vb2_queue *vbq = &node->vbq;
	unsigned int output = V4L2_TYPE_IS_OUTPUT(node->desc.buf_type);
	unsigned int link_flags = node->desc.link_flags;
	int ret;

	/* Initialize mtk_cam_video_device */
	if (link_flags & MEDIA_LNK_FL_IMMUTABLE)
		node->enabled = true;
	else
		node->enabled = false;
	mtk_cam_dev_load_default_fmt(cam, &node->desc, &node->vdev_fmt);

	cam->subdev_pads[node->id].flags = output ?
		MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;

	/* Initialize media entities */
	ret = media_entity_pads_init(&vdev->entity, 1, &node->vdev_pad);
	if (ret) {
		dev_err(dev, "failed to initialize media pad:%d\n", ret);
		return ret;
	}
	node->vdev_pad.flags = output ? MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;

	/* Initialize vbq */
	vbq->type = node->desc.buf_type;
	if (vbq->type == V4L2_BUF_TYPE_META_OUTPUT)
		vbq->io_modes = VB2_MMAP;
	else
		vbq->io_modes = VB2_MMAP | VB2_DMABUF;

	if (node->desc.smem_alloc) {
		vbq->bidirectional = 1;
		vbq->dev = cam->smem_dev;
	} else {
		vbq->dev = dev;
	}
	vbq->ops = &mtk_cam_vb2_ops;
	vbq->mem_ops = &vb2_dma_contig_memops;
	vbq->buf_struct_size = sizeof(struct mtk_cam_dev_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_BOOTIME;
	if (output)
		vbq->timestamp_flags |= V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
	else
		vbq->timestamp_flags |= V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	/* No minimum buffers limitation */
	vbq->min_buffers_needed = 0;
	vbq->drv_priv = cam;
	vbq->lock = &node->vdev_lock;
	vbq->supports_requests = true;
	/*
	 * TODO(b/140397121): Require requests when the HAL
	 * is fixed to use the Request API only.
	 */
	// vbq->requires_requests = true;

	ret = vb2_queue_init(vbq);
	if (ret) {
		dev_err(dev, "failed to init. vb2 queue:%d\n", ret);
		goto fail_media_clean;
	}

	/* Initialize vdev */
	snprintf(vdev->name, sizeof(vdev->name), "%s %s",
		 dev_driver_string(dev), node->desc.name);
	/* set cap/type/ioctl_ops of the video device */
	vdev->device_caps = node->desc.cap | V4L2_CAP_STREAMING;
	vdev->ioctl_ops = node->desc.ioctl_ops;
	vdev->fops = &mtk_cam_v4l2_fops;
	vdev->release = video_device_release_empty;
	vdev->lock = &node->vdev_lock;
	vdev->v4l2_dev = &cam->v4l2_dev;
	vdev->queue = &node->vbq;
	vdev->vfl_dir = output ? VFL_DIR_TX : VFL_DIR_RX;
	vdev->entity.function = MEDIA_ENT_F_IO_V4L;
	vdev->entity.ops = NULL;
	video_set_drvdata(vdev, cam);
	dev_dbg(dev, "registered vdev:%d:%s\n", node->id, vdev->name);

	/* Initialize miscellaneous variables */
	mutex_init(&node->vdev_lock);
	INIT_LIST_HEAD(&node->buf_list);
	spin_lock_init(&node->buf_list_lock);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret) {
		dev_err(dev, "failed to register vde:%d\n", ret);
		goto fail_vb2_rel;
	}

	/* Create link between video node and the subdev pad */
	if (output) {
		ret = media_create_pad_link(&vdev->entity, 0,
					    &cam->subdev.entity,
					    node->id, link_flags);
	} else {
		ret = media_create_pad_link(&cam->subdev.entity,
					    node->id, &vdev->entity, 0,
					    link_flags);
	}
	if (ret)
		goto fail_vdev_ureg;

	return 0;

fail_vdev_ureg:
	video_unregister_device(vdev);
fail_vb2_rel:
	mutex_destroy(&node->vdev_lock);
	vb2_queue_release(vbq);
fail_media_clean:
	media_entity_cleanup(&vdev->entity);

	return ret;
}

static void
mtk_cam_video_unregister_device(struct mtk_cam_video_device *node)
{
	video_unregister_device(&node->vdev);
	media_entity_cleanup(&node->vdev.entity);
	mutex_destroy(&node->vdev_lock);
}

static int mtk_cam_v4l2_register(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	int i, ret;

	/* Set up media device & pads */
	ret = mtk_cam_media_register(cam, &cam->media_dev);
	if (ret)
		return ret;
	dev_info(dev, "Registered media%d\n", cam->media_dev.devnode->minor);

	/* Set up v4l2 device */
	cam->v4l2_dev.mdev = &cam->media_dev;
	ret = v4l2_device_register(dev, &cam->v4l2_dev);
	if (ret) {
		dev_err(dev, "failed to register V4L2 device:%d\n", ret);
		goto fail_media_unreg;
	}
	dev_info(dev, "Registered %s\n", cam->v4l2_dev.name);

	/* Initialize subdev */
	v4l2_subdev_init(&cam->subdev, &mtk_cam_subdev_ops);
	cam->subdev.entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	cam->subdev.entity.ops = &mtk_cam_media_entity_ops;
	cam->subdev.flags = V4L2_SUBDEV_FL_HAS_DEVNODE |
				V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(cam->subdev.name, sizeof(cam->subdev.name),
		 "%s", dev_driver_string(dev));
	v4l2_set_subdevdata(&cam->subdev, cam);

	ret = v4l2_device_register_subdev(&cam->v4l2_dev, &cam->subdev);
	if (ret) {
		dev_err(dev, "failed to initialize subdev:%d\n", ret);
		goto fail_clean_media_entiy;
	}
	dev_dbg(dev, "registered %s\n", cam->subdev.name);

	/* Create video nodes and links */
	for (i = 0; i < MTK_CAM_P1_TOTAL_NODES; i++) {
		struct mtk_cam_video_device *node = &cam->vdev_nodes[i];

		node->id = node->desc.id;
		ret = mtk_cam_video_register_device(cam, node);
		if (ret)
			goto fail_vdev_unreg;
	}
	vb2_dma_contig_set_max_seg_size(dev, DMA_BIT_MASK(32));

	return 0;

fail_vdev_unreg:
	for (i--; i >= 0; i--)
		mtk_cam_video_unregister_device(&cam->vdev_nodes[i]);
fail_clean_media_entiy:
	media_entity_cleanup(&cam->subdev.entity);
	v4l2_device_unregister(&cam->v4l2_dev);
fail_media_unreg:
	media_device_unregister(&cam->media_dev);
	media_device_cleanup(&cam->media_dev);

	return ret;
}

static int mtk_cam_v4l2_unregister(struct mtk_cam_dev *cam)
{
	int i;

	for (i = 0; i < MTK_CAM_P1_TOTAL_NODES; i++)
		mtk_cam_video_unregister_device(&cam->vdev_nodes[i]);

	vb2_dma_contig_clear_max_seg_size(cam->dev);
	v4l2_device_unregister_subdev(&cam->subdev);
	v4l2_device_unregister(&cam->v4l2_dev);
	media_entity_cleanup(&cam->subdev.entity);
	media_device_unregister(&cam->media_dev);
	media_device_cleanup(&cam->media_dev);

	return 0;
}

static int mtk_cam_dev_notifier_bound(struct v4l2_async_notifier *notifier,
				      struct v4l2_subdev *sd,
				      struct v4l2_async_subdev *asd)
{
	struct mtk_cam_dev *cam =
		container_of(notifier, struct mtk_cam_dev, notifier);

	if (!(sd->entity.function & MEDIA_ENT_F_VID_IF_BRIDGE)) {
		dev_dbg(cam->dev, "no MEDIA_ENT_F_VID_IF_BRIDGE function\n");
		return -ENODEV;
	}

	cam->seninf = sd;
	dev_dbg(cam->dev, "%s is bound\n", sd->entity.name);

	return 0;
}

static void mtk_cam_dev_notifier_unbind(struct v4l2_async_notifier *notifier,
					struct v4l2_subdev *sd,
					struct v4l2_async_subdev *asd)
{
	struct mtk_cam_dev *cam =
		container_of(notifier, struct mtk_cam_dev, notifier);

	cam->seninf = NULL;
	dev_dbg(cam->dev, "%s is unbound\n", sd->entity.name);
}

static int mtk_cam_dev_notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct mtk_cam_dev *cam =
		container_of(notifier, struct mtk_cam_dev, notifier);
	struct device *dev = cam->dev;
	int ret;

	if (!cam->seninf) {
		dev_err(dev, "No seninf subdev\n");
		return -ENODEV;
	}

	ret = media_create_pad_link(&cam->seninf->entity, MTK_CAM_CIO_PAD_SRC,
				    &cam->subdev.entity, MTK_CAM_CIO_PAD_SINK,
				    MEDIA_LNK_FL_IMMUTABLE |
				    MEDIA_LNK_FL_ENABLED);
	if (ret) {
		dev_err(dev, "failed to create pad link %s %s err:%d\n",
			cam->seninf->entity.name, cam->subdev.entity.name,
			ret);
		return ret;
	}

	ret = v4l2_device_register_subdev_nodes(&cam->v4l2_dev);
	if (ret) {
		dev_err(dev, "failed to initialize subdev nodes:%d\n", ret);
		return ret;
	}

	return ret;
}

static const struct v4l2_async_notifier_operations mtk_cam_v4l2_async_ops = {
	.bound = mtk_cam_dev_notifier_bound,
	.unbind = mtk_cam_dev_notifier_unbind,
	.complete = mtk_cam_dev_notifier_complete,
};

static int mtk_cam_v4l2_async_register(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	int ret;

	v4l2_async_notifier_init(&cam->notifier);
	ret = v4l2_async_notifier_parse_fwnode_endpoints(dev,
		&cam->notifier, sizeof(struct v4l2_async_subdev), NULL);
	if (ret) {
		dev_err(dev, "failed to parse fwnode endpoints:%d\n", ret);
		return ret;
	}

	cam->notifier.ops = &mtk_cam_v4l2_async_ops;
	dev_dbg(dev, "mtk_cam v4l2_async_notifier_register\n");
	ret = v4l2_async_notifier_register(&cam->v4l2_dev, &cam->notifier);
	if (ret) {
		dev_err(dev, "failed to register async notifier : %d\n", ret);
		v4l2_async_notifier_cleanup(&cam->notifier);
	}

	return ret;
}

static void mtk_cam_v4l2_async_unregister(struct mtk_cam_dev *cam)
{
	v4l2_async_notifier_unregister(&cam->notifier);
	v4l2_async_notifier_cleanup(&cam->notifier);
}

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vcap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_cap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_cap = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_out = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_format meta_fmts[] = {
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_PARAMS,
			.buffersize = 512 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_3A,
			.buffersize = 1200 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_AF,
			.buffersize = 640 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LCS,
			.buffersize = 288 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LMV,
			.buffersize = 256,
		},
	},
};

static const struct v4l2_format stream_out_fmts[] = {
	/* This is a default image format */
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
		},
	},
};

static const struct v4l2_format bin_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14F,
		},
	},
};

static const struct
mtk_cam_dev_node_desc output_queues[] = {
	{
		.id = MTK_CAM_P1_META_IN_0,
		.name = "meta input",
		.cap = V4L2_CAP_META_OUTPUT,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.link_flags = 0,
		.image = false,
		.smem_alloc = true,
		.fmts = meta_fmts,
		.default_fmt_idx = 0,
		.max_buf_count = 10,
		.ioctl_ops = &mtk_cam_v4l2_meta_out_ioctl_ops,
	},
};

static const struct
mtk_cam_dev_node_desc capture_queues[] = {
	{
		.id = MTK_CAM_P1_MAIN_STREAM_OUT,
		.name = "main stream",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED,
		.image = true,
		.smem_alloc = false,
		.dma_port = R_IMGO,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_CAM_P1_PACKED_BIN_OUT,
		.name = "packed out",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = R_RRZO,
		.fmts = bin_out_fmts,
		.num_fmts = ARRAY_SIZE(bin_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_CAM_P1_META_OUT_0,
		.name = "partial meta 0",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = R_AAO | R_FLKO | R_PSO,
		.fmts = meta_fmts,
		.default_fmt_idx = 1,
		.max_buf_count = 5,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_CAM_P1_META_OUT_1,
		.name = "partial meta 1",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = R_AFO,
		.fmts = meta_fmts,
		.default_fmt_idx = 2,
		.max_buf_count = 5,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_CAM_P1_META_OUT_2,
		.name = "partial meta 2",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = R_LCSO,
		.fmts = meta_fmts,
		.default_fmt_idx = 3,
		.max_buf_count = 10,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_CAM_P1_META_OUT_3,
		.name = "partial meta 3",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = R_LMVO,
		.fmts = meta_fmts,
		.default_fmt_idx = 4,
		.max_buf_count = 10,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
};

/* The helper to configure the device context */
static void mtk_cam_dev_queue_setup(struct mtk_cam_dev *cam)
{
	unsigned int node_idx;
	int i;

	node_idx = 0;
	/* Setup the output queue */
	for (i = 0; i < ARRAY_SIZE(output_queues); i++)
		cam->vdev_nodes[node_idx++].desc = output_queues[i];

	/* Setup the capture queue */
	for (i = 0; i < ARRAY_SIZE(capture_queues); i++)
		cam->vdev_nodes[node_idx++].desc = capture_queues[i];
}

int mtk_cam_dev_init(struct platform_device *pdev,
		     struct mtk_cam_dev *cam)
{
	int ret;

	cam->dev = &pdev->dev;
	mtk_cam_dev_queue_setup(cam);

	spin_lock_init(&cam->pending_job_lock);
	spin_lock_init(&cam->running_job_lock);
	INIT_LIST_HEAD(&cam->pending_job_list);
	INIT_LIST_HEAD(&cam->running_job_list);
	mutex_init(&cam->op_lock);

	/* v4l2 sub-device registration */
	ret = mtk_cam_v4l2_register(cam);
	if (ret)
		return ret;

	ret = mtk_cam_v4l2_async_register(cam);
	if (ret)
		goto fail_v4l2_unreg;

	return 0;

fail_v4l2_unreg:
	mutex_destroy(&cam->op_lock);
	mtk_cam_v4l2_unregister(cam);

	return ret;
}

void mtk_cam_dev_cleanup(struct mtk_cam_dev *cam)
{
	mtk_cam_v4l2_async_unregister(cam);
	mtk_cam_v4l2_unregister(cam);
	mutex_destroy(&cam->op_lock);
}

