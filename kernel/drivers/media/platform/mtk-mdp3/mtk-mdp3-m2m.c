// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/platform_device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>
#include "mtk-mdp3-m2m.h"

static inline struct mdp_m2m_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mdp_m2m_ctx, fh);
}

static inline struct mdp_m2m_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mdp_m2m_ctx, ctrl_handler);
}

static inline struct mdp_frame *ctx_get_frame(struct mdp_m2m_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->curr_param.output;
	else
		return &ctx->curr_param.captures[0];
}

static void mdp_m2m_ctx_set_state(struct mdp_m2m_ctx *ctx, u32 state)
{
	mutex_lock(&ctx->curr_param.state_lock);
	ctx->curr_param.state |= state;
	mutex_unlock(&ctx->curr_param.state_lock);
}

static bool mdp_m2m_ctx_is_state_set(struct mdp_m2m_ctx *ctx, u32 mask)
{
	bool ret;

	mutex_lock(&ctx->curr_param.state_lock);
	ret = (ctx->curr_param.state & mask) == mask;
	mutex_unlock(&ctx->curr_param.state_lock);
	return ret;
}

static void mdp_m2m_ctx_lock(struct vb2_queue *q)
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(q);

	mutex_lock(&ctx->mdp_dev->m2m_lock);
}

static void mdp_m2m_ctx_unlock(struct vb2_queue *q)
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(q);

	mutex_unlock(&ctx->mdp_dev->m2m_lock);
}

static void mdp_m2m_job_abort(void *priv)
{
}

static void mdp_m2m_process_done(void *priv, int vb_state)
{
	struct mdp_m2m_ctx *ctx = priv;
	struct vb2_v4l2_buffer *src_vbuf, *dst_vbuf;
	u32 valid_output_flags = V4L2_BUF_FLAG_TIMECODE |
				 V4L2_BUF_FLAG_TSTAMP_SRC_MASK |
				 V4L2_BUF_FLAG_KEYFRAME |
				 V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME;

	src_vbuf = (struct vb2_v4l2_buffer *)
			v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vbuf = (struct vb2_v4l2_buffer *)
			v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	src_vbuf->sequence = ctx->frame_count;
	dst_vbuf->sequence = src_vbuf->sequence;
	dst_vbuf->timecode = src_vbuf->timecode;
	dst_vbuf->flags &= ~valid_output_flags;
	dst_vbuf->flags |= src_vbuf->flags & valid_output_flags;

	v4l2_m2m_buf_done(src_vbuf, vb_state);
	v4l2_m2m_buf_done(dst_vbuf, vb_state);
	v4l2_m2m_job_finish(ctx->mdp_dev->m2m_dev, ctx->m2m_ctx);

	ctx->curr_param.frame_no = ctx->frame_count++;
}

static void mdp_m2m_worker(struct work_struct *work)
{
	struct mdp_m2m_ctx *ctx = container_of(work, struct mdp_m2m_ctx, work);
	struct mdp_frame *frame;
	struct vb2_v4l2_buffer *src_vb, *dst_vb;
	struct img_ipi_frameparam param = {0};
	struct mdp_cmdq_param task = {0};
	enum vb2_buffer_state vb_state = VB2_BUF_STATE_ERROR;
	int ret;

	if (mdp_m2m_ctx_is_state_set(ctx, MDP_M2M_CTX_ERROR)) {
		dev_err(&ctx->mdp_dev->pdev->dev,
			"mdp_m2m_ctx is in error state\n");
		goto worker_end;
	}

	param.frame_no = ctx->curr_param.frame_no;
	param.type = ctx->curr_param.type;
	param.num_inputs = 1;
	param.num_outputs = 1;

	frame = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	src_vb = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	mdp_set_src_config(&param.inputs[0], frame, &src_vb->vb2_buf);

	frame = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	dst_vb = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	mdp_set_dst_config(&param.outputs[0], frame, &dst_vb->vb2_buf);

	dst_vb->vb2_buf.timestamp = src_vb->vb2_buf.timestamp;
	param.timestamp = src_vb->vb2_buf.timestamp;

	ret = mdp_vpu_process(&ctx->vpu, &param);
	if (ret) {
		dev_err(&ctx->mdp_dev->pdev->dev,
			"VPU MDP process failed: %d\n", ret);
		goto worker_end;
	}

	task.config = ctx->vpu.config;
	task.param = &param;
	task.composes[0] = &frame->compose;
	task.wait = 1;
	task.cmdq_cb = NULL;
	task.cb_data = NULL;
	task.mdp_ctx = ctx;

	ret = mdp_cmdq_send(ctx->mdp_dev, &task);
	if (ret) {
		dev_err(&ctx->mdp_dev->pdev->dev,
			"CMDQ sendtask failed: %d\n", ret);
		goto worker_end;
	}

	return;

worker_end:
	mdp_m2m_process_done(ctx, vb_state);
}

static void mdp_m2m_device_run(void *priv)
{
	struct mdp_m2m_ctx *ctx = priv;

	queue_work(ctx->mdp_dev->job_wq, &ctx->work);
}

static int mdp_m2m_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(q);
	int ret;

	ret = 0;//pm_runtime_get_sync(&ctx->mdp_dev->pdev->dev);
	if (ret < 0)
		mdp_dbg(1, "[%d] pm_runtime_get_sync failed:%d", ctx->id, ret);

	ctx->frame_count = 0;

	return 0;
}

static struct vb2_v4l2_buffer *mdp_m2m_buf_remove(struct mdp_m2m_ctx *ctx,
						  unsigned int type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return (struct vb2_v4l2_buffer *)
			v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	else
		return (struct vb2_v4l2_buffer *)
			v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
}

static void mdp_m2m_stop_streaming(struct vb2_queue *q)
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb;

	vb = mdp_m2m_buf_remove(ctx, q->type);
	while (vb) {
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
		vb = mdp_m2m_buf_remove(ctx, q->type);
	}

	//pm_runtime_put(&ctx->mdp_dev->pdev->dev);
}

static int mdp_m2m_queue_setup(struct vb2_queue *q,
			       unsigned int *num_buffers,
			       unsigned int *num_planes, unsigned int sizes[],
			       struct device *alloc_devs[])
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(q);
	struct v4l2_pix_format_mplane *pix_mp;
	u32 i;

	pix_mp = &ctx_get_frame(ctx, q->type)->format.fmt.pix_mp;

	/* from VIDIOC_CREATE_BUFS */
	if (*num_planes) {
		if (*num_planes != pix_mp->num_planes)
			return -EINVAL;
		for (i = 0; i < pix_mp->num_planes; ++i)
			if (sizes[i] < pix_mp->plane_fmt[i].sizeimage)
				return -EINVAL;
	} else {/* from VIDIOC_REQBUFS */
		*num_planes = pix_mp->num_planes;
		for (i = 0; i < pix_mp->num_planes; ++i)
			sizes[i] = pix_mp->plane_fmt[i].sizeimage;
	}

	mdp_dbg(2, "[%d] type:%d, planes:%u, buffers:%u, size:%u,%u,%u",
		ctx->id, q->type, *num_planes, *num_buffers,
		sizes[0], sizes[1], sizes[2]);
	return 0;
}

static int mdp_m2m_buf_prepare(struct vb2_buffer *vb)
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_pix_format_mplane *pix_mp;
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);
	u32 i;

	v4l2_buf->field = V4L2_FIELD_NONE;

	if (!V4L2_TYPE_IS_OUTPUT(vb->type)) {
		pix_mp = &ctx_get_frame(ctx, vb->type)->format.fmt.pix_mp;
		for (i = 0; i < pix_mp->num_planes; ++i) {
			vb2_set_plane_payload(vb, i,
					      pix_mp->plane_fmt[i].sizeimage);
		}
	}
	return 0;
}

static int mdp_m2m_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	v4l2_buf->field = V4L2_FIELD_NONE;

	return 0;
}

static void mdp_m2m_buf_queue(struct vb2_buffer *vb)
{
	struct mdp_m2m_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	v4l2_buf->field = V4L2_FIELD_NONE;

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static const struct vb2_ops mdp_m2m_qops = {
	.queue_setup	= mdp_m2m_queue_setup,
	.wait_prepare	= mdp_m2m_ctx_unlock,
	.wait_finish	= mdp_m2m_ctx_lock,
	.buf_prepare	= mdp_m2m_buf_prepare,
	.start_streaming = mdp_m2m_start_streaming,
	.stop_streaming	= mdp_m2m_stop_streaming,
	.buf_queue	= mdp_m2m_buf_queue,
	.buf_out_validate = mdp_m2m_buf_out_validate,
};

static int mdp_m2m_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);

	strlcpy(cap->driver, MDP_MODULE_NAME, sizeof(cap->driver));
	strlcpy(cap->card, ctx->mdp_dev->pdev->name, sizeof(cap->card));
	strlcpy(cap->bus_info, "platform:mt8183", sizeof(cap->bus_info));
	cap->capabilities = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING |
			V4L2_CAP_DEVICE_CAPS; /* | V4L2_CAP_META_OUTPUT */
	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	return 0;
}

static int mdp_m2m_enum_fmt_mplane(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	return mdp_enum_fmt_mplane(f);
}

static int mdp_m2m_g_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mdp_frame *frame;
	struct v4l2_pix_format_mplane *pix_mp;

	frame = ctx_get_frame(ctx, f->type);
	*f = frame->format;
	pix_mp = &f->fmt.pix_mp;
	pix_mp->colorspace = ctx->curr_param.colorspace;
	pix_mp->xfer_func = ctx->curr_param.xfer_func;
	pix_mp->ycbcr_enc = ctx->curr_param.ycbcr_enc;
	pix_mp->quantization = ctx->curr_param.quant;

	mdp_dbg(2, "[%d] type:%d, frame:%ux%u colorspace=%d", ctx->id, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.colorspace);
	return 0;
}

static int mdp_m2m_s_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mdp_frame *frame = ctx_get_frame(ctx, f->type);
	struct mdp_frame *capture;
	const struct mdp_format *fmt;
	struct vb2_queue *vq;

	mdp_dbg(2, "[%d] type:%d", ctx->id, f->type);

	fmt = mdp_try_fmt_mplane(f, &ctx->curr_param, ctx->id);
	if (!fmt) {
		mdp_err("[%d] try_fmt failed, type:%d", ctx->id, f->type);
		return -EINVAL;
	}

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (vb2_is_streaming(vq)) {
		dev_info(&ctx->mdp_dev->pdev->dev, "Queue %d busy\n", f->type);
		return -EBUSY;
	}

	frame->format = *f;
	frame->mdp_fmt = fmt;
	frame->ycbcr_prof = mdp_map_ycbcr_prof_mplane(f, fmt->mdp_color);
	frame->usage = V4L2_TYPE_IS_OUTPUT(f->type) ?
		MDP_BUFFER_USAGE_HW_READ : MDP_BUFFER_USAGE_MDP;

	capture = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		capture->crop.c.left = 0;
		capture->crop.c.top = 0;
		capture->crop.c.width = f->fmt.pix_mp.width;
		capture->crop.c.height = f->fmt.pix_mp.height;
		ctx->curr_param.colorspace = f->fmt.pix_mp.colorspace;
		ctx->curr_param.ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->curr_param.quant = f->fmt.pix_mp.quantization;
		ctx->curr_param.xfer_func = f->fmt.pix_mp.xfer_func;

		mdp_m2m_ctx_set_state(ctx, MDP_M2M_SRC_FMT);
	} else {
		capture->compose.left = 0;
		capture->compose.top = 0;
		capture->compose.width = f->fmt.pix_mp.width;
		capture->compose.height = f->fmt.pix_mp.height;

		mdp_m2m_ctx_set_state(ctx, MDP_M2M_DST_FMT);
	}

	ctx->frame_count = 0;

	mdp_dbg(2, "[%d] type:%d, frame:%ux%u", ctx->id, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height);
	return 0;
}

static int mdp_m2m_try_fmt_mplane(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);

	if (!mdp_try_fmt_mplane(f, &ctx->curr_param, ctx->id))
		return -EINVAL;

	return 0;
}

static int mdp_m2m_reqbufs(struct file *file, void *fh,
			   struct v4l2_requestbuffers *reqbufs)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int mdp_m2m_streamon(struct file *file, void *fh,
			    enum v4l2_buf_type type)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);
	int ret;

	/* The source and target color formats need to be set */
	if (V4L2_TYPE_IS_OUTPUT(type)) {
		if (!mdp_m2m_ctx_is_state_set(ctx, MDP_M2M_SRC_FMT))
			return -EINVAL;
	} else {
		if (!mdp_m2m_ctx_is_state_set(ctx, MDP_M2M_DST_FMT))
			return -EINVAL;
	}

	if (!mdp_m2m_ctx_is_state_set(ctx, MDP_VPU_INIT)) {
		ret = mdp_vpu_get_locked(ctx->mdp_dev);
		if (ret)
			return ret;

		ret = mdp_vpu_ctx_init(&ctx->vpu, &ctx->mdp_dev->vpu,
				       MDP_DEV_M2M);
		if (ret) {
			dev_err(&ctx->mdp_dev->pdev->dev,
				"VPU init failed %d\n", ret);
			return -EINVAL;
		}
		mdp_m2m_ctx_set_state(ctx, MDP_VPU_INIT);
	}

	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int mdp_m2m_g_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mdp_frame *frame;
	bool valid = false;

	if (s->type <= V4L2_BUF_TYPE_META_OUTPUT) {
		if (V4L2_TYPE_IS_OUTPUT(s->type))
			valid = mdp_target_is_crop(s->target);
		else
			valid = mdp_target_is_compose(s->target);
	}

	if (!valid) {
		mdp_dbg(1, "[%d] invalid type:%u target:%u", ctx->id, s->type,
			s->target);
		return -EINVAL;
	}

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		frame = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		s->r = frame->crop.c;
		return 0;
	case V4L2_SEL_TGT_COMPOSE:
		frame = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		s->r = frame->compose;
		return 0;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		frame = ctx_get_frame(ctx, s->type);
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = frame->format.fmt.pix_mp.width;
		s->r.height = frame->format.fmt.pix_mp.height;
		return 0;
	}
	return -EINVAL;
}

static int mdp_m2m_s_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(fh);
	struct mdp_frame *frame = ctx_get_frame(ctx, s->type);
	struct mdp_frame *capture;
	struct v4l2_rect r;
	bool valid = false;
	int ret;

	if (s->type <= V4L2_BUF_TYPE_META_OUTPUT) {
		if (V4L2_TYPE_IS_OUTPUT(s->type))
			valid = (s->target == V4L2_SEL_TGT_CROP);
		else
			valid = (s->target == V4L2_SEL_TGT_COMPOSE);
	}
	if (!valid) {
		mdp_dbg(1, "[%d] invalid type:%u target:%u", ctx->id, s->type,
			s->target);
		return -EINVAL;
	}

	ret = mdp_try_crop(&r, s, frame, ctx->id);
	if (ret)
		return ret;
	capture = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	/* Check to see if scaling ratio is within supported range */
	if (mdp_m2m_ctx_is_state_set(ctx, MDP_M2M_DST_FMT | MDP_M2M_SRC_FMT)) {
		if (mdp_target_is_crop(s->target)) {
			ret = mdp_check_scaling_ratio(&r, &capture->compose,
						      capture->rotation,
						      ctx->curr_param.limit);
		} else {
			ret = mdp_check_scaling_ratio(&capture->crop.c, &r,
						      capture->rotation,
						      ctx->curr_param.limit);
		}

		if (ret) {
			dev_info(&ctx->mdp_dev->pdev->dev,
				 "Out of scaling range\n");
			return ret;
		}
	}

	if (mdp_target_is_crop(s->target))
		capture->crop.c = r;
	else
		capture->compose = r;

	s->r = r;
	memset(s->reserved, 0, sizeof(s->reserved));

	ctx->frame_count = 0;
	return 0;
}

static const struct v4l2_ioctl_ops mdp_m2m_ioctl_ops = {
	.vidioc_querycap		= mdp_m2m_querycap,
	.vidioc_enum_fmt_vid_cap	= mdp_m2m_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_out	= mdp_m2m_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= mdp_m2m_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= mdp_m2m_g_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= mdp_m2m_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= mdp_m2m_s_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= mdp_m2m_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mdp_m2m_try_fmt_mplane,
	.vidioc_reqbufs			= mdp_m2m_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf			= v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_streamon		= mdp_m2m_streamon,
	.vidioc_streamoff		= v4l2_m2m_ioctl_streamoff,
	.vidioc_g_selection		= mdp_m2m_g_selection,
	.vidioc_s_selection		= mdp_m2m_s_selection,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int mdp_m2m_queue_init(void *priv,
			      struct vb2_queue *src_vq,
			      struct vb2_queue *dst_vq)
{
	struct mdp_m2m_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->ops = &mdp_m2m_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->dev = &ctx->mdp_dev->pdev->dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->ops = &mdp_m2m_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->dev = &ctx->mdp_dev->pdev->dev;

	return vb2_queue_init(dst_vq);
}

static int mdp_m2m_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mdp_m2m_ctx *ctx = ctrl_to_ctx(ctrl);
	struct mdp_frame *capture;

	if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
		return 0;

	capture = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		capture->hflip = ctrl->val;
		break;
	case V4L2_CID_VFLIP:
		capture->vflip = ctrl->val;
		break;
	case V4L2_CID_ROTATE:
		if (mdp_m2m_ctx_is_state_set(ctx,
					     MDP_M2M_DST_FMT |
					     MDP_M2M_SRC_FMT)) {
			int ret = mdp_check_scaling_ratio(&capture->crop.c,
				&capture->compose, ctrl->val,
				ctx->curr_param.limit);

			if (ret)
				return ret;
		}
		capture->rotation = ctrl->val;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops mdp_m2m_ctrl_ops = {
	.s_ctrl	= mdp_m2m_s_ctrl,
};

static int mdp_m2m_ctrls_create(struct mdp_m2m_ctx *ctx)
{
	v4l2_ctrl_handler_init(&ctx->ctrl_handler, MDP_MAX_CTRLS);
	ctx->ctrls.hflip = v4l2_ctrl_new_std(&ctx->ctrl_handler,
					     &mdp_m2m_ctrl_ops, V4L2_CID_HFLIP,
					     0, 1, 1, 0);
	ctx->ctrls.vflip = v4l2_ctrl_new_std(&ctx->ctrl_handler,
					     &mdp_m2m_ctrl_ops, V4L2_CID_VFLIP,
					     0, 1, 1, 0);
	ctx->ctrls.rotate = v4l2_ctrl_new_std(&ctx->ctrl_handler,
					      &mdp_m2m_ctrl_ops,
					      V4L2_CID_ROTATE, 0, 270, 90, 0);

	if (ctx->ctrl_handler.error) {
		int err = ctx->ctrl_handler.error;

		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		dev_err(&ctx->mdp_dev->pdev->dev,
			"Failed to register controls\n");
		return err;
	}
	return 0;
}

static int mdp_m2m_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct mdp_dev *mdp = video_get_drvdata(vdev);
	struct mdp_m2m_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&mdp->m2m_lock)) {
		ret = -ERESTARTSYS;
		goto err_free_ctx;
	}

	ctx->id = ida_alloc(&mdp->mdp_ida, GFP_KERNEL);
	ctx->mdp_dev = mdp;

	v4l2_fh_init(&ctx->fh, vdev);
	vdev->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	file->private_data = &ctx->fh;
	ret = mdp_m2m_ctrls_create(ctx);
	if (ret)
		goto err_exit_fh;

	/* Use separate control handler per file handle */
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	v4l2_fh_add(&ctx->fh);

	ctx->m2m_ctx = v4l2_m2m_ctx_init(mdp->m2m_dev, ctx, mdp_m2m_queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		dev_err(&mdp->pdev->dev, "Failed to initialize m2m context\n");
		ret = PTR_ERR(ctx->m2m_ctx);
		goto err_release_handler;
	}
	ctx->fh.m2m_ctx = ctx->m2m_ctx;

	INIT_WORK(&ctx->work, mdp_m2m_worker);

	ret = mdp_frameparam_init(&ctx->curr_param);
	if (ret) {
		dev_err(&mdp->pdev->dev,
			"Failed to initialize mdp parameter\n");
		goto err_release_m2m_ctx;
	}

	mutex_unlock(&mdp->m2m_lock);

	mdp_dbg(1, "%s [%d]", dev_name(&mdp->pdev->dev), ctx->id);

	return 0;

err_release_m2m_ctx:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
err_release_handler:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
err_exit_fh:
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&mdp->m2m_lock);
err_free_ctx:
	kfree(ctx);

	return ret;
}

static int mdp_m2m_release(struct file *file)
{
	struct mdp_m2m_ctx *ctx = fh_to_ctx(file->private_data);
	struct mdp_dev *mdp = video_drvdata(file);

	flush_workqueue(mdp->job_wq);
	mutex_lock(&mdp->m2m_lock);
	if (mdp_m2m_ctx_is_state_set(ctx, MDP_VPU_INIT)) {
		mdp_vpu_ctx_deinit(&ctx->vpu);
		mdp_vpu_put_locked(mdp);
	}

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	ida_free(&mdp->mdp_ida, ctx->id);
	mutex_unlock(&mdp->m2m_lock);

	mdp_dbg(1, "%s [%d]", dev_name(&mdp->pdev->dev), ctx->id);
	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations mdp_m2m_fops = {
	.owner		= THIS_MODULE,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
	.open		= mdp_m2m_open,
	.release	= mdp_m2m_release,
};

static const struct v4l2_m2m_ops mdp_m2m_ops = {
	.device_run	= mdp_m2m_device_run,
	.job_abort	= mdp_m2m_job_abort,
};

int mdp_m2m_device_register(struct mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;
	int ret = 0;

	mdp->m2m_vdev = video_device_alloc();
	if (!mdp->m2m_vdev) {
		dev_err(dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_video_alloc;
	}
	//mdp->m2m_vdev->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE |
	//	V4L2_CAP_STREAMING;
	mdp->m2m_vdev->fops = &mdp_m2m_fops;
	mdp->m2m_vdev->ioctl_ops = &mdp_m2m_ioctl_ops;
	mdp->m2m_vdev->release = video_device_release;
	mdp->m2m_vdev->lock = &mdp->m2m_lock;
	mdp->m2m_vdev->vfl_dir = VFL_DIR_M2M;
	mdp->m2m_vdev->v4l2_dev = &mdp->v4l2_dev;
	snprintf(mdp->m2m_vdev->name, sizeof(mdp->m2m_vdev->name), "%s:m2m",
		 MDP_MODULE_NAME);
	video_set_drvdata(mdp->m2m_vdev, mdp);

	mdp->m2m_dev = v4l2_m2m_init(&mdp_m2m_ops);
	if (IS_ERR(mdp->m2m_dev)) {
		dev_err(dev, "Failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(mdp->m2m_dev);
		goto err_m2m_init;
	}

	ret = video_register_device(mdp->m2m_vdev, VFL_TYPE_GRABBER, 2);
	if (ret) {
		dev_err(dev, "Failed to register video device\n");
		goto err_video_register;
	}

	v4l2_info(&mdp->v4l2_dev, "Driver registered as /dev/video%d",
		  mdp->m2m_vdev->num);
	return 0;

err_video_register:
	v4l2_m2m_release(mdp->m2m_dev);
err_m2m_init:
	video_device_release(mdp->m2m_vdev);
err_video_alloc:

	return ret;
}

void mdp_m2m_device_unregister(struct mdp_dev *mdp)
{
	video_unregister_device(mdp->m2m_vdev);
	video_device_release(mdp->m2m_vdev);
	v4l2_m2m_release(mdp->m2m_dev);
}

void mdp_m2m_job_finish(struct mdp_m2m_ctx *ctx)
{
	enum vb2_buffer_state vb_state = VB2_BUF_STATE_DONE;

	mdp_m2m_process_done(ctx, vb_state);
}

