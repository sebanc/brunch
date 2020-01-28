// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <linux/videodev2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include "mtk_dip-dev.h"
#include "mtk_dip-hw.h"
#include "mtk-mdp3-cmdq.h"

static int mtk_dip_subdev_open(struct v4l2_subdev *sd,
			       struct v4l2_subdev_fh *fh)
{
	int i;
	struct mtk_dip_pipe *pipe = mtk_dip_subdev_to_pipe(sd);

	for (i = 0; i < pipe->desc->total_queues; i++) {
		*v4l2_subdev_get_try_format(&pipe->subdev, fh->pad, i) =
			pipe->nodes[i].pad_fmt;
		*v4l2_subdev_get_try_crop(&pipe->subdev, fh->pad, i) =
			pipe->nodes[i].crop;
	}

	return 0;
}

static int mtk_dip_subdev_s_stream(struct v4l2_subdev *sd,
				   int enable)
{
	struct mtk_dip_pipe *pipe = mtk_dip_subdev_to_pipe(sd);
	int ret;

	if (enable) {
		ret = mtk_dip_hw_streamon(pipe);
		if (ret)
			dev_err(pipe->dip_dev->dev,
				"%s:%s: pipe(%d) streamon failed\n",
				__func__, pipe->desc->name, pipe->desc->id);
	} else {
		ret = mtk_dip_hw_streamoff(pipe);
		if (ret)
			dev_err(pipe->dip_dev->dev,
				"%s:%s: pipe(%d) streamon off with errors\n",
				__func__, pipe->desc->name, pipe->desc->id);
	}

	return ret;
}

static int mtk_dip_subdev_get_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_format *fmt)
{
	struct mtk_dip_pipe *dip_pipe = mtk_dip_subdev_to_pipe(sd);
	struct v4l2_mbus_framefmt *mf;
	u32 pad = fmt->pad;

	if (pad == MTK_DIP_VIDEO_NODE_ID_TUNING_OUT)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		fmt->format = dip_pipe->nodes[pad].pad_fmt;
	} else {
		mf = v4l2_subdev_get_try_format(sd, cfg, pad);
		fmt->format = *mf;
	}

	return 0;
}

static int mtk_dip_subdev_set_fmt(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_format *fmt)
{
	struct mtk_dip_pipe *dip_pipe = mtk_dip_subdev_to_pipe(sd);
	struct v4l2_mbus_framefmt *mf;
	u32 pad = fmt->pad;
	struct mtk_dip_video_device *node = &dip_pipe->nodes[pad];

	if (pad == MTK_DIP_VIDEO_NODE_ID_TUNING_OUT)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		mf = v4l2_subdev_get_try_format(sd, cfg, pad);
	else
		mf = &dip_pipe->nodes[pad].pad_fmt;

	fmt->format.code = mf->code;
	fmt->format.width =
		clamp_val(fmt->format.width,
			  node->desc->frmsizeenum->stepwise.min_width,
			  node->desc->frmsizeenum->stepwise.max_width);
	fmt->format.height =
		clamp_val(fmt->format.height,
			  node->desc->frmsizeenum->stepwise.min_height,
			  node->desc->frmsizeenum->stepwise.max_height);

	*mf = fmt->format;

	return 0;
}

static int mtk_dip_subdev_get_selection(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_selection *sel)
{
	struct v4l2_rect *try_sel, *r;
	struct mtk_dip_pipe *dip_pipe = mtk_dip_subdev_to_pipe(sd);

	if (sel->pad != MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE &&
	    sel->pad != MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE) {
		dev_dbg(dip_pipe->dip_dev->dev,
			"g_select failed(%s:%d):not support\n",
			dip_pipe->nodes[sel->pad].desc->name, sel->pad);
		return -EINVAL;
	}

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		try_sel = v4l2_subdev_get_try_crop(sd, cfg, sel->pad);
		r = &dip_pipe->nodes[sel->pad].crop;  /* effective resolution */
		break;
	default:
		dev_dbg(dip_pipe->dip_dev->dev,
			"s_select failed(%s:%d):target(%d) not support\n",
			dip_pipe->nodes[sel->pad].desc->name, sel->pad,
			sel->target);
		return -EINVAL;
	}

	if (sel->which == V4L2_SUBDEV_FORMAT_TRY)
		sel->r = *try_sel;
	else
		sel->r = *r;

	return 0;
}

static int mtk_dip_subdev_set_selection(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_selection *sel)
{
	struct v4l2_rect *rect, *try_sel;
	struct mtk_dip_pipe *dip_pipe = mtk_dip_subdev_to_pipe(sd);

	if (sel->pad != MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE &&
	    sel->pad != MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE) {
		dev_dbg(dip_pipe->dip_dev->dev,
			"g_select failed(%s:%d):not support\n",
			dip_pipe->nodes[sel->pad].desc->name, sel->pad);
		return -EINVAL;
	}

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		try_sel = v4l2_subdev_get_try_crop(sd, cfg, sel->pad);
		rect = &dip_pipe->nodes[sel->pad].crop;
		break;
	default:
		dev_dbg(dip_pipe->dip_dev->dev,
			"s_select failed(%s:%d):target(%d) not support\n",
			dip_pipe->nodes[sel->pad].desc->name, sel->pad,
			sel->target);
		return -EINVAL;
	}

	if (sel->which == V4L2_SUBDEV_FORMAT_TRY)
		*try_sel = sel->r;
	else
		*rect = sel->r;

	return 0;
}

static int mtk_dip_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote,
			      u32 flags)
{
	struct mtk_dip_pipe *pipe =
		container_of(entity, struct mtk_dip_pipe, subdev.entity);
	u32 pad = local->index;

	WARN_ON(entity->obj_type != MEDIA_ENTITY_TYPE_V4L2_SUBDEV);
	WARN_ON(pad >= pipe->desc->total_queues);

	mutex_lock(&pipe->lock);

	if (flags & MEDIA_LNK_FL_ENABLED)
		pipe->nodes_enabled |= 1 << pad;
	else
		pipe->nodes_enabled &= ~(1 << pad);

	pipe->nodes[pad].flags &= ~MEDIA_LNK_FL_ENABLED;
	pipe->nodes[pad].flags |= flags & MEDIA_LNK_FL_ENABLED;

	mutex_unlock(&pipe->lock);

	return 0;
}

static int mtk_dip_vb2_meta_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vb->vb2_queue);
	struct device *dev = pipe->dip_dev->dev;
	const struct v4l2_format *fmt = &node->vdev_fmt;

	if (vb->planes[0].length < fmt->fmt.meta.buffersize) {
		dev_dbg(dev,
			"%s:%s:%s: size error(user:%d, required:%d)\n",
			__func__, pipe->desc->name, node->desc->name,
			vb->planes[0].length, fmt->fmt.meta.buffersize);
		return -EINVAL;
	}

	if (vb->planes[0].bytesused != fmt->fmt.meta.buffersize) {
		dev_err(dev,
			"%s:%s:%s: bytesused(%d) must be %d\n",
			__func__, pipe->desc->name, node->desc->name,
			vb->planes[0].bytesused,
			fmt->fmt.meta.buffersize);
		return -EINVAL;
	}

	return 0;
}

static int mtk_dip_vb2_video_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vb->vb2_queue);
	struct device *dev = pipe->dip_dev->dev;
	const struct v4l2_format *fmt = &node->vdev_fmt;
	unsigned int size;
	int i;

	for (i = 0; i < vb->num_planes; i++) {
		size = fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
		if (vb->planes[i].length < size) {
			dev_dbg(dev,
				"%s:%s:%s: size error(user:%d, max:%d)\n",
				__func__, pipe->desc->name, node->desc->name,
				vb->planes[i].length, size);
			return -EINVAL;
		}
	}

	return 0;
}

static int mtk_dip_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (v4l2_buf->field == V4L2_FIELD_ANY)
		v4l2_buf->field = V4L2_FIELD_NONE;

	if (v4l2_buf->field != V4L2_FIELD_NONE)
		return -EINVAL;

	return 0;
}

static int mtk_dip_vb2_meta_buf_init(struct vb2_buffer *vb)
{
	struct mtk_dip_dev_buffer *dev_buf = mtk_dip_vb2_buf_to_dev_buf(vb);
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vb->vb2_queue);
	phys_addr_t buf_paddr;

	dev_buf->scp_daddr[0] = vb2_dma_contig_plane_dma_addr(vb, 0);
	buf_paddr = dev_buf->scp_daddr[0];
	dev_buf->isp_daddr[0] =	dma_map_resource(pipe->dip_dev->dev,
						 buf_paddr,
						 vb->planes[0].length,
						 DMA_BIDIRECTIONAL,
						 DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(pipe->dip_dev->dev,
			      dev_buf->isp_daddr[0])) {
		dev_err(pipe->dip_dev->dev,
			"%s:%s: failed to map buffer: s_daddr(%pad)\n",
			pipe->desc->name, node->desc->name,
			&dev_buf->scp_daddr[0]);
		return -EINVAL;
	}

	return 0;
}

static int mtk_dip_vb2_video_buf_init(struct vb2_buffer *vb)
{
	struct mtk_dip_dev_buffer *dev_buf = mtk_dip_vb2_buf_to_dev_buf(vb);
	int i;

	for (i = 0; i < vb->num_planes; i++) {
		dev_buf->scp_daddr[i] = 0;
		dev_buf->isp_daddr[i] =	vb2_dma_contig_plane_dma_addr(vb, i);
	}

	return 0;
}

static void mtk_dip_vb2_queue_meta_buf_cleanup(struct vb2_buffer *vb)
{
	struct mtk_dip_dev_buffer *dev_buf = mtk_dip_vb2_buf_to_dev_buf(vb);
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);

	dma_unmap_resource(pipe->dip_dev->dev, dev_buf->isp_daddr[0],
			   vb->planes[0].length, DMA_BIDIRECTIONAL,
			   DMA_ATTR_SKIP_CPU_SYNC);
}

static void mtk_dip_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_dip_dev_buffer *dev_buf = mtk_dip_vb2_buf_to_dev_buf(vb);
	struct mtk_dip_request *req = mtk_dip_media_req_to_dip_req(vb->request);
	struct mtk_dip_video_device *node =
		mtk_dip_vbq_to_node(vb->vb2_queue);
	struct mtk_dip_dev *dip_dev = dip_dev;
	int buf_count;

	dev_buf->fmt = node->vdev_fmt;
	dev_buf->dev_fmt = node->dev_q.dev_fmt;
	dev_buf->dma_port = node->desc->dma_port;
	dev_buf->rotation = node->rotation;
	dev_buf->crop.c = node->crop;
	dev_buf->compose = node->compose;

	spin_lock(&node->buf_list_lock);
	list_add_tail(&dev_buf->list, &node->buf_list);
	spin_unlock(&node->buf_list_lock);

	buf_count = atomic_dec_return(&req->buf_count);
	if (!buf_count) {
		mutex_lock(&req->dip_pipe->lock);
		mtk_dip_pipe_try_enqueue(req->dip_pipe);
		mutex_unlock(&req->dip_pipe->lock);
	}
}

static int mtk_dip_vb2_meta_queue_setup(struct vb2_queue *vq,
					unsigned int *num_buffers,
					unsigned int *num_planes,
					unsigned int sizes[],
					struct device *alloc_devs[])
{
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vq);
	const struct v4l2_format *fmt = &node->vdev_fmt;

	if (!*num_planes)
		*num_planes = 1;

	if (sizes[0] <= 0)
		sizes[0] = fmt->fmt.meta.buffersize;

	*num_buffers = clamp_val(*num_buffers, 1, VB2_MAX_FRAME);

	return 0;
}

static int mtk_dip_vb2_video_queue_setup(struct vb2_queue *vq,
					 unsigned int *num_buffers,
					 unsigned int *num_planes,
					 unsigned int sizes[],
					 struct device *alloc_devs[])
{
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vq);
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vq);
	const struct v4l2_format *fmt = &node->vdev_fmt;
	int i;

	if (!*num_planes)
		*num_planes = 1;

	for (i = 0; i < *num_planes; i++) {
		if (sizes[i] <= 0) {
			dev_dbg(pipe->dip_dev->dev,
				"%s:%s:%s: invalid buf: %u < %u\n",
				__func__, pipe->desc->name,
				node->desc->name, sizes[i],
				fmt->fmt.pix_mp.plane_fmt[i].sizeimage);
			sizes[i] = fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
		}

		*num_buffers = clamp_val(*num_buffers, 1, VB2_MAX_FRAME);
	}

	return 0;
}

static void mtk_dip_return_all_buffers(struct mtk_dip_pipe *pipe,
				       struct mtk_dip_video_device *node,
				       enum vb2_buffer_state state)
{
	struct mtk_dip_dev_buffer *b, *b0;

	spin_lock(&node->buf_list_lock);
	list_for_each_entry_safe(b, b0, &node->buf_list, list) {
		list_del(&b->list);
		vb2_buffer_done(&b->vbb.vb2_buf, state);
	}
	spin_unlock(&node->buf_list_lock);
}

static int mtk_dip_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vq);
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vq);
	int ret;

	mutex_lock(&pipe->lock);
	if (!pipe->nodes_streaming) {
		ret = media_pipeline_start(&node->vdev.entity, &pipe->pipeline);
		if (ret < 0) {
			dev_err(pipe->dip_dev->dev,
				"%s:%s: media_pipeline_start failed(%d)\n",
				pipe->desc->name, node->desc->name, ret);
			goto fail_return_bufs;
		}
	}

	if (!(node->flags & MEDIA_LNK_FL_ENABLED)) {
		dev_err(pipe->dip_dev->dev,
			"%s:%s: stream on failed, node is not enabled\n",
			pipe->desc->name, node->desc->name);

		ret = -ENOLINK;
		goto fail_stop_pipeline;
	}

	pipe->nodes_streaming |= 1 << node->desc->id;
	if (pipe->nodes_streaming == pipe->nodes_enabled) {
		/* Start streaming of the whole pipeline */
		ret = v4l2_subdev_call(&pipe->subdev, video, s_stream, 1);
		if (ret < 0) {
			dev_err(pipe->dip_dev->dev,
				"%s:%s: sub dev s_stream(1) failed(%d)\n",
				pipe->desc->name, node->desc->name, ret);

			goto fail_stop_pipeline;
		}
	}

	mutex_unlock(&pipe->lock);

	return 0;

fail_stop_pipeline:
	media_pipeline_stop(&node->vdev.entity);

fail_return_bufs:
	mtk_dip_return_all_buffers(pipe, node, VB2_BUF_STATE_QUEUED);
	mutex_unlock(&pipe->lock);

	return ret;
}

static void mtk_dip_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_dip_pipe *pipe = vb2_get_drv_priv(vq);
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vq);
	int ret;

	mutex_lock(&pipe->lock);

	if (pipe->streaming) {
		ret = v4l2_subdev_call(&pipe->subdev, video, s_stream, 0);
		if (ret)
			dev_err(pipe->dip_dev->dev,
				"%s:%s: sub dev s_stream(0) failed(%d)\n",
				pipe->desc->name, node->desc->name, ret);
	}

	pipe->nodes_streaming &= ~(1 << node->desc->id);
	if (!pipe->nodes_streaming)
		media_pipeline_stop(&node->vdev.entity);

	mtk_dip_return_all_buffers(pipe, node, VB2_BUF_STATE_ERROR);

	mutex_unlock(&pipe->lock);
}

static void mtk_dip_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_dip_video_device *node = mtk_dip_vbq_to_node(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req,
				   &node->ctrl_handler);
}

static int mtk_dip_videoc_querycap(struct file *file, void *fh,
				   struct v4l2_capability *cap)
{
	struct mtk_dip_pipe *pipe = video_drvdata(file);

	snprintf(cap->driver, sizeof(cap->driver), "%s %s",
		 dev_driver_string(pipe->dip_dev->dev), pipe->desc->name);
	snprintf(cap->card, sizeof(cap->card), "%s %s",
		 dev_driver_string(pipe->dip_dev->dev), pipe->desc->name);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev_name(pipe->dip_dev->mdev.dev));

	return 0;
}

static int mtk_dip_videoc_try_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mtk_dip_pipe *pipe = video_drvdata(file);
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);
	const struct mtk_dip_dev_format *dev_fmt;
	struct v4l2_format try_fmt;

	memset(&try_fmt, 0, sizeof(try_fmt));

	dev_fmt = mtk_dip_pipe_find_fmt(pipe, node,
					f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_fmt = &node->desc->fmts[node->desc->default_fmt_idx];
		dev_dbg(pipe->dip_dev->dev,
			"%s:%s:%s: dev_fmt(%d) not found, use default(%d)\n",
			__func__, pipe->desc->name, node->desc->name,
			f->fmt.pix_mp.pixelformat, dev_fmt->format);
	}

	mtk_dip_pipe_try_fmt(pipe, node, &try_fmt, f, dev_fmt);
	*f = try_fmt;

	return 0;
}

static int mtk_dip_videoc_g_fmt(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);

	*f = node->vdev_fmt;

	return 0;
}

static int mtk_dip_videoc_s_fmt(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);
	struct mtk_dip_pipe *pipe = video_drvdata(file);
	const struct mtk_dip_dev_format *dev_fmt;

	if (pipe->streaming || vb2_is_busy(&node->dev_q.vbq))
		return -EBUSY;

	dev_fmt = mtk_dip_pipe_find_fmt(pipe, node,
					f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_fmt = &node->desc->fmts[node->desc->default_fmt_idx];
		dev_dbg(pipe->dip_dev->dev,
			"%s:%s:%s: dev_fmt(%d) not found, use default(%d)\n",
			__func__, pipe->desc->name, node->desc->name,
			f->fmt.pix_mp.pixelformat, dev_fmt->format);
	}

	memset(&node->vdev_fmt, 0, sizeof(node->vdev_fmt));

	mtk_dip_pipe_try_fmt(pipe, node, &node->vdev_fmt, f, dev_fmt);
	*f = node->vdev_fmt;

	node->dev_q.dev_fmt = dev_fmt;
	node->vdev_fmt = *f;
	node->crop.left = 0; /* reset crop setting of nodes */
	node->crop.top = 0;
	node->crop.width = f->fmt.pix_mp.width;
	node->crop.height = f->fmt.pix_mp.height;
	node->compose.left = 0;
	node->compose.top = 0;
	node->compose.width = f->fmt.pix_mp.width;
	node->compose.height = f->fmt.pix_mp.height;

	return 0;
}

static int mtk_dip_videoc_enum_framesizes(struct file *file, void *priv,
					  struct v4l2_frmsizeenum *sizes)
{
	struct mtk_dip_pipe *pipe = video_drvdata(file);
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);
	const struct mtk_dip_dev_format *dev_fmt;

	dev_fmt = mtk_dip_pipe_find_fmt(pipe, node, sizes->pixel_format);

	if (!dev_fmt || sizes->index)
		return -EINVAL;

	sizes->type = node->desc->frmsizeenum->type;
	sizes->stepwise.max_width =
		node->desc->frmsizeenum->stepwise.max_width;
	sizes->stepwise.min_width =
		node->desc->frmsizeenum->stepwise.min_width;
	sizes->stepwise.max_height =
		node->desc->frmsizeenum->stepwise.max_height;
	sizes->stepwise.min_height =
		node->desc->frmsizeenum->stepwise.min_height;
	sizes->stepwise.step_height =
		node->desc->frmsizeenum->stepwise.step_height;
	sizes->stepwise.step_width =
		node->desc->frmsizeenum->stepwise.step_width;

	return 0;
}

static int mtk_dip_videoc_enum_fmt(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);

	if (f->index >= node->desc->num_fmts)
		return -EINVAL;

	strscpy(f->description, node->desc->description,
		sizeof(f->description));
	f->pixelformat = node->desc->fmts[f->index].format;
	f->flags = 0;

	return 0;
}

static int mtk_dip_meta_enum_format(struct file *file, void *fh,
				    struct v4l2_fmtdesc *f)
{
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);

	if (f->index > 0)
		return -EINVAL;

	strscpy(f->description, node->desc->description,
		sizeof(f->description));

	f->pixelformat = node->vdev_fmt.fmt.meta.dataformat;
	f->flags = 0;

	return 0;
}

static int mtk_dip_videoc_g_meta_fmt(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct mtk_dip_video_device *node = mtk_dip_file_to_node(file);
	*f = node->vdev_fmt;

	return 0;
}

static int mtk_dip_video_device_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_dip_video_device *node =
		container_of(ctrl->handler,
			     struct mtk_dip_video_device, ctrl_handler);

	if (ctrl->id != V4L2_CID_ROTATE) {
		pr_debug("[%s] doesn't support ctrl(%d)\n",
			 node->desc->name, ctrl->id);
		return -EINVAL;
	}

	node->rotation = ctrl->val;

	return 0;
}

/******************** function pointers ********************/

static const struct v4l2_subdev_internal_ops mtk_dip_subdev_internal_ops = {
	.open = mtk_dip_subdev_open,
};

static const struct v4l2_subdev_video_ops mtk_dip_subdev_video_ops = {
	.s_stream = mtk_dip_subdev_s_stream,
};

static const struct v4l2_subdev_pad_ops mtk_dip_subdev_pad_ops = {
	.link_validate = v4l2_subdev_link_validate_default,
	.get_fmt = mtk_dip_subdev_get_fmt,
	.set_fmt = mtk_dip_subdev_set_fmt,
	.get_selection = mtk_dip_subdev_get_selection,
	.set_selection = mtk_dip_subdev_set_selection,
};

static const struct v4l2_subdev_ops mtk_dip_subdev_ops = {
	.video = &mtk_dip_subdev_video_ops,
	.pad = &mtk_dip_subdev_pad_ops,
};

static const struct media_entity_operations mtk_dip_media_ops = {
	.link_setup = mtk_dip_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static struct media_request *mtk_dip_request_alloc(struct media_device *mdev)
{
	struct mtk_dip_request *dip_req;

	dip_req = kzalloc(sizeof(*dip_req), GFP_KERNEL);

	return &dip_req->req;
}

static void mtk_dip_request_free(struct media_request *req)
{
	struct mtk_dip_request *dip_req = mtk_dip_media_req_to_dip_req(req);

	kfree(dip_req);
}

static int mtk_dip_vb2_request_validate(struct media_request *req)
{
	struct media_request_object *obj;
	struct mtk_dip_dev *dip_dev = mtk_dip_mdev_to_dev(req->mdev);
	struct mtk_dip_request *dip_req = mtk_dip_media_req_to_dip_req(req);
	struct mtk_dip_pipe *pipe = NULL;
	struct mtk_dip_pipe *pipe_prev = NULL;
	struct mtk_dip_dev_buffer **map = dip_req->buf_map;
	int buf_count = 0;

	memset(map, 0, sizeof(dip_req->buf_map));

	list_for_each_entry(obj, &req->objects, list) {
		struct vb2_buffer *vb;
		struct mtk_dip_dev_buffer *dev_buf;
		struct mtk_dip_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;

		vb = container_of(obj, struct vb2_buffer, req_obj);
		node = mtk_dip_vbq_to_node(vb->vb2_queue);
		pipe = vb2_get_drv_priv(vb->vb2_queue);
		if (pipe_prev && pipe != pipe_prev) {
			dev_dbg(dip_dev->dev,
				"%s:%s:%s:found buf of different pipes(%p,%p)\n",
				__func__, node->desc->name,
				req->debug_str, pipe, pipe_prev);
			return -EINVAL;
		}

		pipe_prev = pipe;
		dev_buf = mtk_dip_vb2_buf_to_dev_buf(vb);
		dip_req->buf_map[node->desc->id] = dev_buf;
		buf_count++;
	}

	if (!pipe) {
		dev_dbg(dip_dev->dev,
			"%s: no buffer in the request(%p)\n",
			req->debug_str, req);

		return -EINVAL;
	}

	if (!map[MTK_DIP_VIDEO_NODE_ID_RAW_OUT] ||
	    (!map[MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE] &&
	     !map[MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE] &&
	     !map[MTK_DIP_VIDEO_NODE_ID_IMG3_CAPTURE])) {
		dev_dbg(dip_dev->dev,
			"won't trigger hw job: raw(%p), mdp0(%p), mdp1(%p), img3(%p)\n",
			map[MTK_DIP_VIDEO_NODE_ID_RAW_OUT],
			map[MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE],
			map[MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE],
			map[MTK_DIP_VIDEO_NODE_ID_IMG3_CAPTURE]);
		return -EINVAL;
	}

	atomic_set(&dip_req->buf_count, buf_count);
	dip_req->id = mtk_dip_pipe_next_job_id(pipe);
	dip_req->dip_pipe = pipe;

	return vb2_request_validate(req);
}

static void mtk_dip_vb2_request_queue(struct media_request *req)
{
	struct mtk_dip_request *dip_req = mtk_dip_media_req_to_dip_req(req);
	struct mtk_dip_pipe *pipe = dip_req->dip_pipe;

	spin_lock(&pipe->job_lock);
	list_add_tail(&dip_req->list, &pipe->pipe_job_pending_list);
	pipe->num_pending_jobs++;
	spin_unlock(&pipe->job_lock);

	vb2_request_queue(req);
}

static const struct media_device_ops mtk_dip_media_req_ops = {
	.req_validate = mtk_dip_vb2_request_validate,
	.req_queue = mtk_dip_vb2_request_queue,
	.req_alloc = mtk_dip_request_alloc,
	.req_free = mtk_dip_request_free,
};

static const struct v4l2_ctrl_ops mtk_dip_video_device_ctrl_ops = {
	.s_ctrl = mtk_dip_video_device_s_ctrl,
};

static const struct vb2_ops mtk_dip_vb2_meta_ops = {
	.buf_queue = mtk_dip_vb2_buf_queue,
	.queue_setup = mtk_dip_vb2_meta_queue_setup,
	.buf_init = mtk_dip_vb2_meta_buf_init,
	.buf_prepare  = mtk_dip_vb2_meta_buf_prepare,
	.buf_out_validate = mtk_dip_vb2_buf_out_validate,
	.buf_cleanup = mtk_dip_vb2_queue_meta_buf_cleanup,
	.start_streaming = mtk_dip_vb2_start_streaming,
	.stop_streaming = mtk_dip_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_dip_vb2_request_complete,
};

static const struct vb2_ops mtk_dip_vb2_video_ops = {
	.buf_queue = mtk_dip_vb2_buf_queue,
	.queue_setup = mtk_dip_vb2_video_queue_setup,
	.buf_init = mtk_dip_vb2_video_buf_init,
	.buf_prepare  = mtk_dip_vb2_video_buf_prepare,
	.buf_out_validate = mtk_dip_vb2_buf_out_validate,
	.start_streaming = mtk_dip_vb2_start_streaming,
	.stop_streaming = mtk_dip_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_dip_vb2_request_complete,
};

static const struct v4l2_file_operations mtk_dip_v4l2_fops = {
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
};

int mtk_dip_dev_media_register(struct device *dev,
			       struct media_device *media_dev)
{
	int ret;

	media_dev->dev = dev;
	strlcpy(media_dev->model, dev_driver_string(dev),
		sizeof(media_dev->model));
	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "platform:%s", dev_name(dev));
	media_dev->hw_revision = 0;
	media_dev->ops = &mtk_dip_media_req_ops;
	media_device_init(media_dev);

	ret = media_device_register(media_dev);
	if (ret) {
		dev_err(dev, "failed to register media device (%d)\n", ret);
		media_device_unregister(media_dev);
		media_device_cleanup(media_dev);
		return ret;
	}

	return 0;
}

static int mtk_dip_video_device_v4l2_register(struct mtk_dip_pipe *pipe,
					      struct mtk_dip_video_device *node)
{
	struct vb2_queue *vbq = &node->dev_q.vbq;
	struct video_device *vdev = &node->vdev;
	struct media_link *link;
	int ret;

	mutex_init(&node->dev_q.lock);

	vdev->device_caps = node->desc->cap;
	vdev->ioctl_ops = node->desc->ops;
	node->vdev_fmt.type = node->desc->buf_type;
	mtk_dip_pipe_load_default_fmt(pipe, node, &node->vdev_fmt);

	node->pad_fmt.width = node->vdev_fmt.fmt.pix_mp.width;
	node->pad_fmt.height = node->vdev_fmt.fmt.pix_mp.height;
	node->pad_fmt.code = MEDIA_BUS_FMT_FIXED;
	node->pad_fmt.field = node->vdev_fmt.fmt.pix_mp.field;
	node->pad_fmt.colorspace = node->vdev_fmt.fmt.pix_mp.colorspace;
	node->pad_fmt.quantization = node->vdev_fmt.fmt.pix_mp.quantization;
	node->crop.left = 0;
	node->crop.top = 0;
	node->crop.width = node->vdev_fmt.fmt.pix_mp.width;
	node->crop.height = node->vdev_fmt.fmt.pix_mp.height;
	node->compose.left = 0;
	node->compose.top = 0;
	node->compose.width = node->vdev_fmt.fmt.pix_mp.width;
	node->compose.height = node->vdev_fmt.fmt.pix_mp.height;

	ret = media_entity_pads_init(&vdev->entity, 1, &node->vdev_pad);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"failed initialize media entity (%d)\n", ret);
		goto err_mutex_destroy;
	}

	node->vdev_pad.flags = V4L2_TYPE_IS_OUTPUT(node->desc->buf_type) ?
		MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;

	vbq->type = node->vdev_fmt.type;
	vbq->io_modes = VB2_MMAP | VB2_DMABUF;
	vbq->ops = node->desc->vb2_ops;
	vbq->mem_ops = &vb2_dma_contig_memops;
	vbq->supports_requests = true;
	vbq->buf_struct_size = sizeof(struct mtk_dip_dev_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	vbq->min_buffers_needed = 0;
	vbq->drv_priv = pipe;
	vbq->lock = &node->dev_q.lock;

	ret = vb2_queue_init(vbq);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"%s:%s:%s: failed to init vb2 queue(%d)\n",
			__func__, pipe->desc->name, node->desc->name,
			ret);
		goto err_media_entity_cleanup;
	}

	snprintf(vdev->name, sizeof(vdev->name), "%s %s %s",
		 dev_driver_string(pipe->dip_dev->dev), pipe->desc->name,
		 node->desc->name);
	vdev->entity.name = vdev->name;
	vdev->entity.function = MEDIA_ENT_F_IO_V4L;
	vdev->entity.ops = NULL;
	vdev->release = video_device_release_empty;
	vdev->fops = &mtk_dip_v4l2_fops;
	vdev->lock = &node->dev_q.lock;
	if (node->desc->supports_ctrls)
		vdev->ctrl_handler = &node->ctrl_handler;
	else
		vdev->ctrl_handler = NULL;
	vdev->v4l2_dev = &pipe->dip_dev->v4l2_dev;
	vdev->queue = &node->dev_q.vbq;
	vdev->vfl_dir = V4L2_TYPE_IS_OUTPUT(node->desc->buf_type) ?
		VFL_DIR_TX : VFL_DIR_RX;

	if (node->desc->smem_alloc)
		vdev->queue->dev = &pipe->dip_dev->scp_pdev->dev;
	else
		vdev->queue->dev = pipe->dip_dev->dev;

	video_set_drvdata(vdev, pipe);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"failed to register video device (%d)\n", ret);
		goto err_vb2_queue_release;
	}

	if (V4L2_TYPE_IS_OUTPUT(node->desc->buf_type))
		ret = media_create_pad_link(&vdev->entity, 0,
					    &pipe->subdev.entity,
					    node->desc->id, node->flags);
	else
		ret = media_create_pad_link(&pipe->subdev.entity,
					    node->desc->id, &vdev->entity,
					    0, node->flags);
	if (ret)
		goto err_video_unregister_device;

	vdev->intf_devnode = media_devnode_create(&pipe->dip_dev->mdev,
						  MEDIA_INTF_T_V4L_VIDEO, 0,
						  VIDEO_MAJOR, vdev->minor);
	if (!vdev->intf_devnode) {
		ret = -ENOMEM;
		goto err_rm_links;
	}

	link = media_create_intf_link(&vdev->entity,
				      &vdev->intf_devnode->intf,
				      node->flags);
	if (!link) {
		ret = -ENOMEM;
		goto err_rm_devnode;
	}

	return 0;

err_rm_devnode:
	media_devnode_remove(vdev->intf_devnode);

err_rm_links:
	media_entity_remove_links(&vdev->entity);

err_video_unregister_device:
	video_unregister_device(vdev);

err_vb2_queue_release:
	vb2_queue_release(&node->dev_q.vbq);

err_media_entity_cleanup:
	media_entity_cleanup(&node->vdev.entity);

err_mutex_destroy:
	mutex_destroy(&node->dev_q.lock);

	return ret;
}

static int mtk_dip_pipe_v4l2_ctrl_init(struct mtk_dip_pipe *dip_pipe)
{
	int i, ret;
	struct mtk_dip_video_device *ctrl_node;

	for (i = 0; i < MTK_DIP_VIDEO_NODE_ID_TOTAL_NUM; i++) {
		ctrl_node = &dip_pipe->nodes[i];
		if (!ctrl_node->desc->supports_ctrls)
			continue;

		v4l2_ctrl_handler_init(&ctrl_node->ctrl_handler, 1);
		v4l2_ctrl_new_std(&ctrl_node->ctrl_handler,
				  &mtk_dip_video_device_ctrl_ops,
				  V4L2_CID_ROTATE, 0, 270, 90, 0);
		ret = ctrl_node->ctrl_handler.error;
		if (ret) {
			dev_err(dip_pipe->dip_dev->dev,
				"%s create rotate ctrl failed:(%d)",
				ctrl_node->desc->name, ret);
			goto err_free_ctrl_handlers;
		}
	}

	return 0;

err_free_ctrl_handlers:
	for (; i >= 0; i--) {
		ctrl_node = &dip_pipe->nodes[i];
		if (!ctrl_node->desc->supports_ctrls)
			continue;
		v4l2_ctrl_handler_free(&ctrl_node->ctrl_handler);
	}

	return ret;
}

static void mtk_dip_pipe_v4l2_ctrl_release(struct mtk_dip_pipe *dip_pipe)
{
	struct mtk_dip_video_device *ctrl_node =
		&dip_pipe->nodes[MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE];

	v4l2_ctrl_handler_free(&ctrl_node->ctrl_handler);
}

int mtk_dip_pipe_v4l2_register(struct mtk_dip_pipe *pipe,
			       struct media_device *media_dev,
			       struct v4l2_device *v4l2_dev)
{
	int i, ret;

	ret = mtk_dip_pipe_v4l2_ctrl_init(pipe);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"%s: failed(%d) to initialize ctrls\n",
			pipe->desc->name, ret);

		return ret;
	}

	pipe->streaming = 0;

	/* Initialize subdev media entity */
	pipe->subdev_pads = devm_kcalloc(pipe->dip_dev->dev,
					 pipe->desc->total_queues,
					 sizeof(*pipe->subdev_pads),
					 GFP_KERNEL);
	if (!pipe->subdev_pads) {
		dev_err(pipe->dip_dev->dev,
			"failed to alloc pipe->subdev_pads (%d)\n", ret);
		ret = -ENOMEM;
		goto err_release_ctrl;
	}
	ret = media_entity_pads_init(&pipe->subdev.entity,
				     pipe->desc->total_queues,
				     pipe->subdev_pads);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"failed initialize subdev media entity (%d)\n", ret);
		goto err_free_subdev_pads;
	}

	/* Initialize subdev */
	v4l2_subdev_init(&pipe->subdev, &mtk_dip_subdev_ops);

	pipe->subdev.entity.function =
		MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	pipe->subdev.entity.ops = &mtk_dip_media_ops;
	pipe->subdev.flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	pipe->subdev.ctrl_handler = NULL;
	pipe->subdev.internal_ops = &mtk_dip_subdev_internal_ops;

	for (i = 0; i < pipe->desc->total_queues; i++)
		pipe->subdev_pads[i].flags =
			V4L2_TYPE_IS_OUTPUT(pipe->nodes[i].desc->buf_type) ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;

	snprintf(pipe->subdev.name, sizeof(pipe->subdev.name),
		 "%s %s", dev_driver_string(pipe->dip_dev->dev),
		 pipe->desc->name);
	v4l2_set_subdevdata(&pipe->subdev, pipe);

	ret = v4l2_device_register_subdev(&pipe->dip_dev->v4l2_dev,
					  &pipe->subdev);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"failed initialize subdev (%d)\n", ret);
		goto err_media_entity_cleanup;
	}

	/* Create video nodes and links */
	for (i = 0; i < pipe->desc->total_queues; i++) {
		ret = mtk_dip_video_device_v4l2_register(pipe,
							 &pipe->nodes[i]);
		if (ret)
			goto err_unregister_subdev;
	}

	return 0;

err_unregister_subdev:
	v4l2_device_unregister_subdev(&pipe->subdev);

err_media_entity_cleanup:
	media_entity_cleanup(&pipe->subdev.entity);

err_free_subdev_pads:
	devm_kfree(pipe->dip_dev->dev, pipe->subdev_pads);

err_release_ctrl:
	mtk_dip_pipe_v4l2_ctrl_release(pipe);

	return ret;
}

void mtk_dip_pipe_v4l2_unregister(struct mtk_dip_pipe *pipe)
{
	unsigned int i;

	for (i = 0; i < pipe->desc->total_queues; i++) {
		video_unregister_device(&pipe->nodes[i].vdev);
		vb2_queue_release(&pipe->nodes[i].dev_q.vbq);
		media_entity_cleanup(&pipe->nodes[i].vdev.entity);
		mutex_destroy(&pipe->nodes[i].dev_q.lock);
	}

	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
	mtk_dip_pipe_v4l2_ctrl_release(pipe);
}

/********************************************
 * MTK DIP V4L2 Settings *
 ********************************************/

static const struct v4l2_ioctl_ops mtk_dip_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_dip_videoc_querycap,

	.vidioc_enum_framesizes = mtk_dip_videoc_enum_framesizes,
	.vidioc_enum_fmt_vid_out = mtk_dip_videoc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = mtk_dip_videoc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_dip_videoc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_dip_videoc_try_fmt,

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

static const struct v4l2_ioctl_ops mtk_dip_v4l2_video_cap_ioctl_ops = {
	.vidioc_querycap = mtk_dip_videoc_querycap,

	.vidioc_enum_framesizes = mtk_dip_videoc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_dip_videoc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_dip_videoc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_dip_videoc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_dip_videoc_try_fmt,

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

static const struct v4l2_ioctl_ops mtk_dip_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_dip_videoc_querycap,

	.vidioc_enum_fmt_meta_out = mtk_dip_meta_enum_format,
	.vidioc_g_fmt_meta_out = mtk_dip_videoc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_dip_videoc_g_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_dip_videoc_g_meta_fmt,

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

static const struct mtk_dip_dev_format fw_param_fmts[] = {
	{
		.format = V4L2_META_FMT_MTISP_PARAMS,
		.buffer_size = 1024 * (128 + 288),
	},
};

static const struct mtk_dip_dev_format lcei_fmts[] = {
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR8,
		.mdp_color = DIP_MCOLOR_BAYER8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
};

static const struct mtk_dip_dev_format in_fmts[] = {
	{
		.format = V4L2_PIX_FMT_VYUY,
		.mdp_color = DIP_MCOLOR_VYUY,
		.depth	 = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.mdp_color = DIP_MCOLOR_YUYV,
		.depth	 = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.mdp_color = DIP_MCOLOR_YVYU,
		.depth	 = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_NV12,
		.mdp_color = DIP_MCOLOR_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR8,
		.mdp_color = DIP_MCOLOR_BAYER8_BGGR,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG8,
		.mdp_color = DIP_MCOLOR_BAYER8_GBRG,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG8,
		.mdp_color = DIP_MCOLOR_BAYER8_GRBG,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB8,
		.mdp_color = DIP_MCOLOR_BAYER8_RGGB,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR8F,
		.mdp_color = DIP_MCOLOR_FULLG8_BGGR,
		.depth = { 12 },
		.row_depth = { 8},
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG8F,
		.mdp_color = DIP_MCOLOR_FULLG8_GBRG,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG8F,
		.mdp_color = DIP_MCOLOR_FULLG8_GRBG,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB8F,
		.mdp_color = DIP_MCOLOR_FULLG8_RGGB,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR10,
		.mdp_color = DIP_MCOLOR_BAYER10_BGGR,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG10,
		.mdp_color = DIP_MCOLOR_BAYER10_GBRG,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG10,
		.mdp_color = DIP_MCOLOR_BAYER10_GRBG,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB10,
		.mdp_color = DIP_MCOLOR_BAYER10_RGGB,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR10F,
		.mdp_color = DIP_MCOLOR_FULLG10_BGGR,
		.depth = { 15 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG10F,
		.mdp_color = DIP_MCOLOR_FULLG10_GBRG,
		.depth = { 15 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG10F,
		.mdp_color = DIP_MCOLOR_FULLG10_GRBG,
		.depth = { 15 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB10F,
		.mdp_color = DIP_MCOLOR_FULLG10_RGGB,
		.depth = { 15 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR12,
		.mdp_color = DIP_MCOLOR_BAYER12_BGGR,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG12,
		.mdp_color = DIP_MCOLOR_BAYER12_GBRG,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG12,
		.mdp_color = DIP_MCOLOR_BAYER12_GRBG,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB12,
		.mdp_color = DIP_MCOLOR_BAYER12_RGGB,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR12F,
		.mdp_color = DIP_MCOLOR_FULLG12_BGGR,
		.depth = { 18 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG12F,
		.mdp_color = DIP_MCOLOR_FULLG12_GBRG,
		.depth = { 18 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG12F,
		.mdp_color = DIP_MCOLOR_FULLG12_GRBG,
		.depth = { 18 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB12F,
		.mdp_color = DIP_MCOLOR_FULLG12_RGGB,
		.depth = { 18 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR14F,
		.mdp_color = DIP_MCOLOR_FULLG14_BGGR,
		.depth = { 21 },
		.row_depth = { 14 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG14F,
		.mdp_color = DIP_MCOLOR_FULLG14_GBRG,
		.depth = { 21 },
		.row_depth = { 14 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG14F,
		.mdp_color = DIP_MCOLOR_FULLG14_GRBG,
		.depth = { 21 },
		.row_depth = { 14 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB14F,
		.mdp_color = DIP_MCOLOR_FULLG14_RGGB,
		.depth = { 21 },
		.row_depth = { 14 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 8,
	},
	{
		.format	= V4L2_PIX_FMT_YUV420M,
		.mdp_color	= DIP_MCOLOR_I420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420M,
		.mdp_color	= DIP_MCOLOR_YV12,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_NV12M,
		.mdp_color	= DIP_MCOLOR_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.num_cplanes = 1,
	},
};

static const struct mtk_dip_dev_format mdp_fmts[] = {
	{
		.format = V4L2_PIX_FMT_VYUY,
		.mdp_color = DIP_MCOLOR_VYUY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.mdp_color = DIP_MCOLOR_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.mdp_color = DIP_MCOLOR_YVYU,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVU420,
		.mdp_color = DIP_MCOLOR_YV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 3,
	},
	{
		.format = V4L2_PIX_FMT_NV12,
		.mdp_color = DIP_MCOLOR_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format	= V4L2_PIX_FMT_YUV420M,
		.mdp_color	= DIP_MCOLOR_I420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420M,
		.mdp_color	= DIP_MCOLOR_YV12,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_NV12M,
		.mdp_color	= DIP_MCOLOR_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.num_cplanes = 1,
	}
};

static const struct mtk_dip_dev_format img2_fmts[] = {
	{
		.format = V4L2_PIX_FMT_YUYV,
		.mdp_color = DIP_MCOLOR_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
};

static const struct mtk_dip_dev_format img3_fmts[] = {
	{
		.format = V4L2_PIX_FMT_VYUY,
		.mdp_color = DIP_MCOLOR_VYUY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.mdp_color = DIP_MCOLOR_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.mdp_color = DIP_MCOLOR_YVYU,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVU420,
		.mdp_color = DIP_MCOLOR_YV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 3,
	},
	{
		.format = V4L2_PIX_FMT_NV12,
		.mdp_color = DIP_MCOLOR_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	}
};

static const struct v4l2_frmsizeenum in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_dip_video_device_desc
queues_setting[MTK_DIP_VIDEO_NODE_ID_TOTAL_NUM] = {
	{
		.id = MTK_DIP_VIDEO_NODE_ID_RAW_OUT,
		.name = "Raw Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_ENABLED,
		.fmts = in_fmts,
		.num_fmts = ARRAY_SIZE(in_fmts),
		.default_fmt_idx = 4,
		.default_width = MTK_DIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &in_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "Main image source",
	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_TUNING_OUT,
		.name = "Tuning",
		.cap = V4L2_CAP_META_OUTPUT | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.smem_alloc = 1,
		.flags = 0,
		.fmts = fw_param_fmts,
		.num_fmts = ARRAY_SIZE(fw_param_fmts),
		.default_fmt_idx = 0,
		.dma_port = 0,
		.frmsizeenum = &in_frmsizeenum,
		.ops = &mtk_dip_v4l2_meta_out_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_meta_ops,
		.description = "Tuning data",
	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_NR_OUT,
		.name = "NR Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = img3_fmts,
		.num_fmts = ARRAY_SIZE(img3_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.frmsizeenum = &in_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "NR image source",
	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_SHADING_OUT,
		.name = "Shading",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = lcei_fmts,
		.num_fmts = ARRAY_SIZE(lcei_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 2,
		.frmsizeenum = &in_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "Shading image source",
	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE,
		.name = "MDP0",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = mdp_fmts,
		.num_fmts = ARRAY_SIZE(mdp_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &out_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "Output quality enhanced image",
	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE,
		.name = "MDP1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = mdp_fmts,
		.num_fmts = ARRAY_SIZE(mdp_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &out_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "Output quality enhanced image",

	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_IMG2_CAPTURE,
		.name = "IMG2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = img2_fmts,
		.num_fmts = ARRAY_SIZE(img2_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_WIDTH,
		.dma_port = 1,
		.frmsizeenum = &out_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "Output quality enhanced image",
	},
	{
		.id = MTK_DIP_VIDEO_NODE_ID_IMG3_CAPTURE,
		.name = "IMG3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = img3_fmts,
		.num_fmts = ARRAY_SIZE(img3_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_WIDTH,
		.dma_port = 2,
		.frmsizeenum = &out_frmsizeenum,
		.ops = &mtk_dip_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_dip_vb2_video_ops,
		.description = "Output quality enhanced image",

	},

};

static const struct mtk_dip_pipe_desc
pipe_settings[MTK_DIP_PIPE_ID_TOTAL_NUM] = {
	{
		.name = "preview",
		.id = MTK_DIP_PIPE_ID_PREVIEW,
		.queue_descs = queues_setting,
		.total_queues = ARRAY_SIZE(queues_setting),
	},
	{
		.name = "capture",
		.id = MTK_DIP_PIPE_ID_CAPTURE,
		.queue_descs = queues_setting,
		.total_queues = ARRAY_SIZE(queues_setting),

	},
	{
		.name = "reprocess",
		.id = MTK_DIP_PIPE_ID_REPROCESS,
		.queue_descs = queues_setting,
		.total_queues = ARRAY_SIZE(queues_setting),
	},
};

static void mtk_dip_dev_media_unregister(struct mtk_dip_dev *dip_dev)
{
	media_device_unregister(&dip_dev->mdev);
	media_device_cleanup(&dip_dev->mdev);
}

static int mtk_dip_dev_v4l2_init(struct mtk_dip_dev *dip_dev)
{
	struct media_device *media_dev = &dip_dev->mdev;
	struct v4l2_device *v4l2_dev = &dip_dev->v4l2_dev;
	int i;
	int ret;

	ret = mtk_dip_dev_media_register(dip_dev->dev, media_dev);
	if (ret) {
		dev_err(dip_dev->dev,
			"%s: media device register failed(%d)\n",
			__func__, ret);
		return ret;
	}

	v4l2_dev->mdev = media_dev;
	v4l2_dev->ctrl_handler = NULL;

	ret = v4l2_device_register(dip_dev->dev, v4l2_dev);
	if (ret) {
		dev_err(dip_dev->dev,
			"%s: v4l2 device register failed(%d)\n",
			__func__, ret);
		goto err_release_media_device;
	}

	for (i = 0; i < MTK_DIP_PIPE_ID_TOTAL_NUM; i++) {
		ret = mtk_dip_pipe_init(dip_dev, &dip_dev->dip_pipe[i],
					&pipe_settings[i]);
		if (ret) {
			dev_err(dip_dev->dev,
				"%s: Pipe id(%d) init failed(%d)\n",
				dip_dev->dip_pipe[i].desc->name,
				i, ret);
			goto err_release_pipe;
		}
	}

	ret = v4l2_device_register_subdev_nodes(&dip_dev->v4l2_dev);
	if (ret) {
		dev_err(dip_dev->dev,
			"failed to register subdevs (%d)\n", ret);
		goto err_release_pipe;
	}

	return 0;

err_release_pipe:
	for (i--; i >= 0; i--)
		mtk_dip_pipe_release(&dip_dev->dip_pipe[i]);

	v4l2_device_unregister(v4l2_dev);

err_release_media_device:
	mtk_dip_dev_media_unregister(dip_dev);

	return ret;
}

void mtk_dip_dev_v4l2_release(struct mtk_dip_dev *dip_dev)
{
	int i;

	for (i = 0; i < MTK_DIP_PIPE_ID_TOTAL_NUM; i++)
		mtk_dip_pipe_release(&dip_dev->dip_pipe[i]);

	v4l2_device_unregister(&dip_dev->v4l2_dev);
	media_device_unregister(&dip_dev->mdev);
	media_device_cleanup(&dip_dev->mdev);
}

static int mtk_dip_res_init(struct platform_device *pdev,
			    struct mtk_dip_dev *dip_dev)
{
	int ret;

	dip_dev->mdp_pdev = mdp_get_plat_device(pdev);
	if (!dip_dev->mdp_pdev) {
		dev_err(dip_dev->dev,
			"%s: failed to get MDP device\n",
			__func__);
		return -EINVAL;
	}

	dip_dev->mdpcb_wq =
		alloc_ordered_workqueue("%s",
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE,
					"mdp_callback");
	if (!dip_dev->mdpcb_wq) {
		dev_err(dip_dev->dev,
			"%s: unable to alloc mdpcb workqueue\n", __func__);
		ret = -ENOMEM;
		goto destroy_mdpcb_wq;
	}

	dip_dev->composer_wq =
		alloc_ordered_workqueue("%s",
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE,
					"dip_composer");
	if (!dip_dev->composer_wq) {
		dev_err(dip_dev->dev,
			"%s: unable to alloc composer workqueue\n", __func__);
		ret = -ENOMEM;
		goto destroy_dip_composer_wq;
	}

	dip_dev->mdp_wq =
		alloc_ordered_workqueue("%s",
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE,
					"dip_runner");
	if (!dip_dev->mdp_wq) {
		dev_err(dip_dev->dev,
			"%s: unable to alloc dip_runner\n", __func__);
		ret = -ENOMEM;
		goto destroy_dip_runner_wq;
	}

	init_waitqueue_head(&dip_dev->flushing_waitq);

	return 0;

destroy_dip_runner_wq:
	destroy_workqueue(dip_dev->mdp_wq);

destroy_dip_composer_wq:
	destroy_workqueue(dip_dev->composer_wq);

destroy_mdpcb_wq:
	destroy_workqueue(dip_dev->mdpcb_wq);

	return ret;
}

static void mtk_dip_res_release(struct mtk_dip_dev *dip_dev)
{
	flush_workqueue(dip_dev->mdp_wq);
	destroy_workqueue(dip_dev->mdp_wq);
	dip_dev->mdp_wq = NULL;

	flush_workqueue(dip_dev->mdpcb_wq);
	destroy_workqueue(dip_dev->mdpcb_wq);
	dip_dev->mdpcb_wq = NULL;

	flush_workqueue(dip_dev->composer_wq);
	destroy_workqueue(dip_dev->composer_wq);
	dip_dev->composer_wq = NULL;

	atomic_set(&dip_dev->num_composing, 0);
	atomic_set(&dip_dev->dip_enqueue_cnt, 0);
}

static int mtk_dip_probe(struct platform_device *pdev)
{
	struct mtk_dip_dev *dip_dev;
	phandle rproc_phandle;
	int ret;

	dip_dev = devm_kzalloc(&pdev->dev, sizeof(*dip_dev), GFP_KERNEL);
	if (!dip_dev)
		return -ENOMEM;

	dip_dev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, dip_dev);
	dip_dev->dip_stream_cnt = 0;
	dip_dev->clks[0].id = "larb5";
	dip_dev->clks[1].id = "dip";
	dip_dev->num_clks = ARRAY_SIZE(dip_dev->clks);
	ret = devm_clk_bulk_get(&pdev->dev, dip_dev->num_clks, dip_dev->clks);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get LARB5 and DIP clks:%d\n",
			ret);
		return ret;
	}

	dip_dev->scp_pdev = scp_get_pdev(pdev);
	if (!dip_dev->scp_pdev) {
		dev_err(dip_dev->dev,
			"%s: failed to get scp device\n",
			__func__);
		return -EINVAL;
	}

	if (of_property_read_u32(dip_dev->dev->of_node, "mediatek,scp",
				 &rproc_phandle)) {
		dev_err(dip_dev->dev,
			"%s: could not get scp device\n",
			__func__);
		return -EINVAL;
	}

	dip_dev->rproc_handle = rproc_get_by_phandle(rproc_phandle);
	if (!dip_dev->rproc_handle) {
		dev_err(dip_dev->dev,
			"%s: could not get DIP's rproc_handle\n",
			__func__);
		return -EINVAL;
	}

	atomic_set(&dip_dev->dip_enqueue_cnt, 0);
	atomic_set(&dip_dev->num_composing, 0);
	mutex_init(&dip_dev->hw_op_lock);
	/* Limited by the co-processor side's stack size */
	sema_init(&dip_dev->sem, DIP_COMPOSING_MAX_NUM);

	ret = mtk_dip_hw_working_buf_pool_init(dip_dev);
	if (ret) {
		dev_err(&pdev->dev, "working buffer init failed(%d)\n", ret);
		return ret;
	}

	ret = mtk_dip_dev_v4l2_init(dip_dev);
	if (ret) {
		mtk_dip_hw_working_buf_pool_release(dip_dev);
		dev_err(&pdev->dev, "v4l2 init failed(%d)\n", ret);

		goto err_release_working_buf_pool;
	}

	ret = mtk_dip_res_init(pdev, dip_dev);
	if (ret) {
		dev_err(dip_dev->dev,
			"%s: mtk_dip_res_init failed(%d)\n", __func__, ret);

		ret = -EBUSY;
		goto err_release_deinit_v4l2;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000 / 30 *
					 DIP_COMPOSING_MAX_NUM * 3 *
					 USEC_PER_MSEC);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

err_release_deinit_v4l2:
	mtk_dip_dev_v4l2_release(dip_dev);
err_release_working_buf_pool:
	mtk_dip_hw_working_buf_pool_release(dip_dev);

	return ret;
}

static int mtk_dip_remove(struct platform_device *pdev)
{
	struct mtk_dip_dev *dip_dev = dev_get_drvdata(&pdev->dev);

	mtk_dip_res_release(dip_dev);
	pm_runtime_disable(&pdev->dev);
	mtk_dip_dev_v4l2_release(dip_dev);
	mtk_dip_hw_working_buf_pool_release(dip_dev);
	mutex_destroy(&dip_dev->hw_op_lock);

	return 0;
}

static int __maybe_unused mtk_dip_runtime_suspend(struct device *dev)
{
	struct mtk_dip_dev *dip_dev = dev_get_drvdata(dev);

	rproc_shutdown(dip_dev->rproc_handle);
	clk_bulk_disable_unprepare(dip_dev->num_clks,
				   dip_dev->clks);

	return 0;
}

static int __maybe_unused mtk_dip_runtime_resume(struct device *dev)
{
	struct mtk_dip_dev *dip_dev = dev_get_drvdata(dev);
	int ret;

	ret = clk_bulk_prepare_enable(dip_dev->num_clks,
				      dip_dev->clks);
	if (ret) {
		dev_err(dip_dev->dev,
			"%s: failed to enable dip clks(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = rproc_boot(dip_dev->rproc_handle);
	if (ret) {
		dev_err(dev, "%s: FW load failed(rproc:%p):%d\n",
			__func__, dip_dev->rproc_handle,	ret);
		clk_bulk_disable_unprepare(dip_dev->num_clks,
					   dip_dev->clks);

		return ret;
	}

	return 0;
}

static int __maybe_unused mtk_dip_pm_suspend(struct device *dev)
{
	struct mtk_dip_dev *dip_dev = dev_get_drvdata(dev);
	int ret, num;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = wait_event_timeout
		(dip_dev->flushing_waitq,
		 !(num = atomic_read(&dip_dev->num_composing)),
		 msecs_to_jiffies(1000 / 30 * DIP_COMPOSING_MAX_NUM * 3));
	if (!ret && num) {
		dev_err(dev, "%s: flushing SCP job timeout, num(%d)\n",
			__func__, num);

		return -EBUSY;
	}

	ret = pm_runtime_force_suspend(dev);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused mtk_dip_pm_resume(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	return 0;
}

static const struct dev_pm_ops mtk_dip_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_dip_pm_suspend, mtk_dip_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_dip_runtime_suspend, mtk_dip_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_dip_of_match[] = {
	{ .compatible = "mediatek,mt8183-dip", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_dip_of_match);

static struct platform_driver mtk_dip_driver = {
	.probe   = mtk_dip_probe,
	.remove  = mtk_dip_remove,
	.driver  = {
		.name = "mtk-cam-dip",
		.pm = &mtk_dip_pm_ops,
		.of_match_table = of_match_ptr(mtk_dip_of_match),
	}
};

module_platform_driver(mtk_dip_driver);

MODULE_AUTHOR("Frederic Chen <frederic.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek DIP driver");

