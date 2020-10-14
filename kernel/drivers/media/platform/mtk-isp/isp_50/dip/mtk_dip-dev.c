// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-event.h>
#include "mtk_dip-dev.h"
#include "mtk-mdp3-regs.h"
#include "mtk-img-ipi.h"

int mtk_dip_pipe_init(struct mtk_dip_dev *dip_dev, struct mtk_dip_pipe *pipe,
		      const struct mtk_dip_pipe_desc *setting)
{
	int ret, i;

	pipe->dip_dev = dip_dev;
	pipe->desc = setting;
	pipe->nodes_enabled = 0;
	pipe->nodes_streaming = 0;

	atomic_set(&pipe->pipe_job_sequence, 0);
	INIT_LIST_HEAD(&pipe->pipe_job_running_list);
	INIT_LIST_HEAD(&pipe->pipe_job_pending_list);
	spin_lock_init(&pipe->job_lock);
	mutex_init(&pipe->lock);

	for (i = 0; i < pipe->desc->total_queues; i++) {
		pipe->nodes[i].desc = &pipe->desc->queue_descs[i];
		pipe->nodes[i].flags = pipe->nodes[i].desc->flags;
		spin_lock_init(&pipe->nodes[i].buf_list_lock);
		INIT_LIST_HEAD(&pipe->nodes[i].buf_list);

		if (pipe->nodes[i].flags & MEDIA_LNK_FL_ENABLED)
			pipe->nodes_enabled |= 1 << i;

		pipe->nodes[i].crop.left = 0;
		pipe->nodes[i].crop.top = 0;
		pipe->nodes[i].crop.width =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_width;
		pipe->nodes[i].crop.height =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_height;
		pipe->nodes[i].compose.left = 0;
		pipe->nodes[i].compose.top = 0;
		pipe->nodes[i].compose.width =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_width;
		pipe->nodes[i].compose.height =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_height;
	}

	ret = mtk_dip_pipe_v4l2_register(pipe, &dip_dev->mdev,
					 &dip_dev->v4l2_dev);
	if (ret) {
		dev_err(pipe->dip_dev->dev,
			"%s: failed(%d) to create V4L2 devices\n",
			pipe->desc->name, ret);

		goto err_destroy_pipe_lock;
	}

	return 0;

err_destroy_pipe_lock:
	mutex_destroy(&pipe->lock);

	return ret;
}

int mtk_dip_pipe_release(struct mtk_dip_pipe *pipe)
{
	mtk_dip_pipe_v4l2_unregister(pipe);
	mutex_destroy(&pipe->lock);

	return 0;
}

int mtk_dip_pipe_next_job_id(struct mtk_dip_pipe *pipe)
{
	int global_job_id = atomic_inc_return(&pipe->pipe_job_sequence);

	return (global_job_id & 0x0000ffff) | (pipe->desc->id << 16);
}

struct mtk_dip_request *mtk_dip_pipe_get_running_job(struct mtk_dip_pipe *pipe,
						     int id)
{
	struct mtk_dip_request *req;
	unsigned long flags;

	spin_lock_irqsave(&pipe->job_lock, flags);
	list_for_each_entry(req,
			    &pipe->pipe_job_running_list, list) {
		if (req->id == id) {
			spin_unlock_irqrestore(&pipe->job_lock, flags);
			return req;
		}
	}
	spin_unlock_irqrestore(&pipe->job_lock, flags);

	return NULL;
}

void mtk_dip_pipe_remove_job(struct mtk_dip_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&req->dip_pipe->job_lock, flags);
	list_del(&req->list);
	req->dip_pipe->num_jobs = req->dip_pipe->num_jobs - 1;
	spin_unlock_irqrestore(&req->dip_pipe->job_lock, flags);
}

void mtk_dip_pipe_job_finish(struct mtk_dip_request *req,
			     enum vb2_buffer_state vbf_state)
{
	struct mtk_dip_pipe *pipe = req->dip_pipe;
	struct mtk_dip_dev_buffer *in_buf;
	int i;
	unsigned long flags;

	in_buf = req->buf_map[MTK_DIP_VIDEO_NODE_ID_RAW_OUT];

	for (i = 0; i < pipe->desc->total_queues; i++) {
		struct mtk_dip_dev_buffer *dev_buf = req->buf_map[i];
		struct mtk_dip_video_device *node;

		if (!dev_buf)
			continue;

		if (dev_buf != in_buf)
			dev_buf->vbb.vb2_buf.timestamp =
				in_buf->vbb.vb2_buf.timestamp;

		vb2_buffer_done(&dev_buf->vbb.vb2_buf, vbf_state);

		node = mtk_dip_vbq_to_node(dev_buf->vbb.vb2_buf.vb2_queue);
		spin_lock_irqsave(&node->buf_list_lock, flags);
		list_del(&dev_buf->list);
		spin_unlock_irqrestore(&node->buf_list_lock, flags);
	}
}

static u32 dip_pass1_fmt_get_stride(struct v4l2_pix_format_mplane *mfmt,
				    const struct mtk_dip_dev_format *fmt,
				    unsigned int plane)
{
	unsigned int width = ALIGN(mfmt->width, 4);
	unsigned int pixel_bits = fmt->row_depth[0];
	unsigned int bpl, ppl;

	ppl = DIV_ROUND_UP(width * fmt->depth[0], fmt->row_depth[0]);
	bpl = DIV_ROUND_UP(ppl * pixel_bits, 8);

	return ALIGN(bpl, fmt->pass_1_align);
}

/* Stride that is accepted by MDP HW */
static u32 dip_mdp_fmt_get_stride(struct v4l2_pix_format_mplane *mfmt,
				  const struct mtk_dip_dev_format *fmt,
				  unsigned int plane)
{
	enum mdp_color c = fmt->mdp_color;
	u32 bytesperline = (mfmt->width * fmt->row_depth[plane]) / 8;
	u32 stride = (bytesperline * DIP_MCOLOR_BITS_PER_PIXEL(c))
		/ fmt->row_depth[0];

	if (plane == 0)
		return stride;

	if (plane < DIP_MCOLOR_GET_PLANE_COUNT(c)) {
		if (DIP_MCOLOR_IS_BLOCK_MODE(c))
			stride = stride / 2;
		return stride;
	}

	return 0;
}

/* Stride that is accepted by MDP HW of format with contiguous planes */
static u32
dip_mdp_fmt_get_stride_contig(const struct mtk_dip_dev_format *fmt,
			      u32 pix_stride, unsigned int plane)
{
	enum mdp_color c = fmt->mdp_color;
	u32 stride = pix_stride;

	if (plane == 0)
		return stride;

	if (plane < DIP_MCOLOR_GET_PLANE_COUNT(c)) {
		stride = stride >> DIP_MCOLOR_GET_H_SUBSAMPLE(c);
		if (DIP_MCOLOR_IS_UV_COPLANE(c) && !DIP_MCOLOR_IS_BLOCK_MODE(c))
			stride = stride * 2;
		return stride;
	}

	return 0;
}

/* Plane size that is accepted by MDP HW */
static u32
dip_mdp_fmt_get_plane_size(const struct mtk_dip_dev_format *fmt,
			   u32 stride, u32 height, unsigned int plane)
{
	enum mdp_color c = fmt->mdp_color;
	u32 bytesperline;

	bytesperline = (stride * fmt->row_depth[0])
		/ DIP_MCOLOR_BITS_PER_PIXEL(c);

	if (plane == 0)
		return bytesperline * height;
	if (plane < DIP_MCOLOR_GET_PLANE_COUNT(c)) {
		height = height >> DIP_MCOLOR_GET_V_SUBSAMPLE(c);
		if (DIP_MCOLOR_IS_BLOCK_MODE(c))
			bytesperline = bytesperline * 2;
		return bytesperline * height;
	}

	return 0;
}

static int mtk_dip_pipe_get_stride(struct mtk_dip_pipe *pipe,
				   struct v4l2_pix_format_mplane *mfmt,
				   const struct mtk_dip_dev_format *dfmt,
				   int plane, const char *buf_name)
{
	int bpl;

	if (dfmt->pass_1_align)
		bpl = dip_pass1_fmt_get_stride(mfmt, dfmt, plane);
	else
		bpl = dip_mdp_fmt_get_stride(mfmt, dfmt, plane);

	return bpl;
}

void mtk_dip_pipe_try_fmt(struct mtk_dip_pipe *pipe,
			  struct mtk_dip_video_device *node,
			  struct v4l2_format *fmt,
			  const struct v4l2_format *ufmt,
			  const struct mtk_dip_dev_format *dfmt)
{
	int i;

	fmt->type = ufmt->type;
	fmt->fmt.pix_mp.width =
		clamp_val(ufmt->fmt.pix_mp.width,
			  node->desc->frmsizeenum->stepwise.min_width,
			  node->desc->frmsizeenum->stepwise.max_width);
	fmt->fmt.pix_mp.height =
		clamp_val(ufmt->fmt.pix_mp.height,
			  node->desc->frmsizeenum->stepwise.min_height,
			  node->desc->frmsizeenum->stepwise.max_height);
	fmt->fmt.pix_mp.num_planes = ufmt->fmt.pix_mp.num_planes;
	fmt->fmt.pix_mp.field = V4L2_FIELD_NONE;

	if (ufmt->fmt.pix_mp.quantization != V4L2_QUANTIZATION_FULL_RANGE)
		fmt->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	else
		fmt->fmt.pix_mp.quantization =  ufmt->fmt.pix_mp.quantization;

	if (ufmt->fmt.pix_mp.colorspace < 0xFF)
		fmt->fmt.pix_mp.colorspace = ufmt->fmt.pix_mp.colorspace;
	else
		fmt->fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;

	/* Only MDP 0 and MDP 1 allow the color space change */
	if (node->desc->id != MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE &&
	    node->desc->id != MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE) {
		fmt->fmt.pix_mp.quantization = V4L2_QUANTIZATION_FULL_RANGE;
		fmt->fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
	}

	fmt->fmt.pix_mp.pixelformat = dfmt->format;
	fmt->fmt.pix_mp.num_planes = dfmt->num_planes;
	fmt->fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->fmt.pix_mp.field = V4L2_FIELD_NONE;

	for (i = 0; i < fmt->fmt.pix_mp.num_planes; ++i) {
		unsigned int stride;
		unsigned int sizeimage;

		stride = mtk_dip_pipe_get_stride(pipe, &fmt->fmt.pix_mp,
						 dfmt, i, node->desc->name);

		/*
		 * Check whether user prepare buffer with larger stride
		 * than HW required.
		 */
		if (stride < ufmt->fmt.pix_mp.plane_fmt[i].bytesperline)
			stride = ufmt->fmt.pix_mp.plane_fmt[i].bytesperline;

		if (dfmt->pass_1_align)
			sizeimage = stride * fmt->fmt.pix_mp.height;
		else
			sizeimage = DIV_ROUND_UP(stride *
						 fmt->fmt.pix_mp.height *
						 dfmt->depth[i],
						 dfmt->row_depth[i]);

		fmt->fmt.pix_mp.plane_fmt[i].sizeimage = sizeimage;
		fmt->fmt.pix_mp.plane_fmt[i].bytesperline = stride;
	}
}

static void set_meta_fmt(struct mtk_dip_pipe *pipe,
			 struct v4l2_meta_format *fmt,
			 const struct mtk_dip_dev_format *dev_fmt)
{
	fmt->dataformat = dev_fmt->format;

	if (dev_fmt->buffer_size <= 0)
		fmt->buffersize =
			MTK_DIP_DEV_META_BUF_DEFAULT_SIZE;
	else
		fmt->buffersize = dev_fmt->buffer_size;
}

void mtk_dip_pipe_load_default_fmt(struct mtk_dip_pipe *pipe,
				   struct mtk_dip_video_device *node,
				   struct v4l2_format *fmt)
{
	int idx = node->desc->default_fmt_idx;

	if (idx >= node->desc->num_fmts) {
		dev_err(pipe->dip_dev->dev,
			"%s:%s: invalid idx(%d), must < num_fmts(%d)\n",
			__func__, node->desc->name, idx, node->desc->num_fmts);

		idx = 0;
	}

	fmt->type = node->desc->buf_type;
	if (mtk_dip_buf_is_meta(node->desc->buf_type)) {
		set_meta_fmt(pipe, &fmt->fmt.meta,
			     &node->desc->fmts[idx]);
	} else {
		fmt->fmt.pix_mp.width = node->desc->default_width;
		fmt->fmt.pix_mp.height = node->desc->default_height;
		mtk_dip_pipe_try_fmt(pipe, node, fmt, fmt,
				     &node->desc->fmts[idx]);
	}
}

const struct mtk_dip_dev_format*
mtk_dip_pipe_find_fmt(struct mtk_dip_pipe *pipe,
		      struct mtk_dip_video_device *node,
		      u32 format)
{
	int i;

	for (i = 0; i < node->desc->num_fmts; i++) {
		if (node->desc->fmts[i].format == format)
			return &node->desc->fmts[i];
	}

	return NULL;
}

static enum mdp_ycbcr_profile
mtk_dip_map_ycbcr_prof_mplane(struct v4l2_pix_format_mplane *pix_mp,
			      u32 mdp_color)
{
	switch (pix_mp->colorspace) {
	case V4L2_COLORSPACE_DEFAULT:
		if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
			return MDP_YCBCR_PROFILE_FULL_BT601;
		return MDP_YCBCR_PROFILE_BT601;
	case V4L2_COLORSPACE_JPEG:
		return MDP_YCBCR_PROFILE_JPEG;
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_DCI_P3:
		if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
			return MDP_YCBCR_PROFILE_FULL_BT709;
		return MDP_YCBCR_PROFILE_BT709;
	case V4L2_COLORSPACE_BT2020:
		if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
			return MDP_YCBCR_PROFILE_FULL_BT2020;
		return MDP_YCBCR_PROFILE_BT2020;
	}

	if (DIP_MCOLOR_IS_RGB(mdp_color))
		return MDP_YCBCR_PROFILE_FULL_BT601;

	if (pix_mp->quantization == V4L2_QUANTIZATION_FULL_RANGE)
		return MDP_YCBCR_PROFILE_FULL_BT601;

	return MDP_YCBCR_PROFILE_BT601;
}

static inline int
mtk_dip_is_contig_mp_buffer(struct mtk_dip_dev_buffer *dev_buf)
{
	return !(dev_buf->dev_fmt->num_cplanes == 1);
}

static void mtk_dip_fill_ipi_img_param_mp(struct mtk_dip_pipe *pipe,
					  struct img_image_buffer *b,
					  struct mtk_dip_dev_buffer *dev_buf,
					  char *buf_name)
{
	struct v4l2_pix_format_mplane *pix_mp = &dev_buf->fmt.fmt.pix_mp;
	const struct mtk_dip_dev_format *mdp_fmt = dev_buf->dev_fmt;
	unsigned int i;
	unsigned int total_plane_size = 0;

	b->usage = dev_buf->dma_port;
	b->format.colorformat = dev_buf->dev_fmt->mdp_color;
	b->format.width = dev_buf->fmt.fmt.pix_mp.width;
	b->format.height = dev_buf->fmt.fmt.pix_mp.height;
	b->format.ycbcr_prof =
		mtk_dip_map_ycbcr_prof_mplane(pix_mp,
					      dev_buf->dev_fmt->mdp_color);

	for (i = 0; i < pix_mp->num_planes; ++i) {
		u32 stride = pix_mp->plane_fmt[i].bytesperline;

		/*
		 * We use dip_mdp_fmt_get_plane_size() to get the plane sizes of
		 * non-M multiple plane image buffers. In this case,
		 * pix_mp->plane_fmt[0].sizeimage is the total size of all
		 * b->format.plane_fmt[i].
		 */
		b->format.plane_fmt[i].stride = stride;
		b->format.plane_fmt[i].size =
			dip_mdp_fmt_get_plane_size(mdp_fmt, stride,
						   pix_mp->height, i);
		b->iova[i] = dev_buf->isp_daddr[i];
		total_plane_size += b->format.plane_fmt[i].size;
	}

	if (!mtk_dip_is_contig_mp_buffer(dev_buf))
		return;

	for (; i < DIP_MCOLOR_GET_PLANE_COUNT(b->format.colorformat); ++i) {
		u32 stride = dip_mdp_fmt_get_stride_contig
				(mdp_fmt, b->format.plane_fmt[0].stride, i);

		b->format.plane_fmt[i].stride = stride;
		b->format.plane_fmt[i].size =
			dip_mdp_fmt_get_plane_size(mdp_fmt, stride,
						   pix_mp->height, i);
		b->iova[i] = b->iova[i - 1] + b->format.plane_fmt[i - 1].size;
		total_plane_size += b->format.plane_fmt[i].size;
	}
}

static void mtk_dip_fill_input_ipi_param(struct mtk_dip_pipe *pipe,
					 struct img_input *iin,
					 struct mtk_dip_dev_buffer *dev_buf,
					 char *buf_name)
{
	mtk_dip_fill_ipi_img_param_mp(pipe, &iin->buffer, dev_buf, buf_name);
}

static void mtk_dip_fill_output_ipi_param(struct mtk_dip_pipe *pipe,
					  struct img_output *iout,
					  struct mtk_dip_dev_buffer *buf_out,
					  struct mtk_dip_dev_buffer *buf_in,
					  char *buf_name)
{
	mtk_dip_fill_ipi_img_param_mp(pipe, &iout->buffer, buf_out, buf_name);
	iout->crop.left = 0;
	iout->crop.top = 0;
	iout->crop.width = buf_in->fmt.fmt.pix_mp.width;
	iout->crop.height = buf_in->fmt.fmt.pix_mp.height;
	iout->crop.left_subpix = 0;
	iout->crop.top_subpix = 0;
	iout->crop.width_subpix = 0;
	iout->crop.height_subpix = 0;
	iout->rotation = 0;
}

static u32 mtk_dip_to_fixed(u32 *r, struct v4l2_fract *f)
{
	u32 q;

	if (f->denominator == 0) {
		*r = 0;
		return 0;
	}

	q = f->numerator / f->denominator;
	*r = div_u64(((u64)f->numerator - q * f->denominator) <<
		     IMG_SUBPIXEL_SHIFT, f->denominator);
	return q;
}

static void mtk_dip_set_src_crop(struct img_crop *c, struct mtk_dip_crop *crop)
{
	c->left = crop->c.left
		+ mtk_dip_to_fixed(&c->left_subpix, &crop->left_subpix);
	c->top = crop->c.top
		+ mtk_dip_to_fixed(&c->top_subpix, &crop->top_subpix);
	c->width = crop->c.width
		+ mtk_dip_to_fixed(&c->width_subpix, &crop->width_subpix);
	c->height = crop->c.height
		+ mtk_dip_to_fixed(&c->height_subpix, &crop->height_subpix);
}

static void mtk_dip_set_orientation(struct img_output *out,
				    s32 rotation, bool hflip, bool vflip)
{
	u8 flip = 0;

	if (hflip)
		flip ^= 1;
	if (vflip) {
		/*
		 * A vertical flip is equivalent to
		 * a 180-degree rotation with a horizontal flip
		 */
		rotation += 180;
		flip ^= 1;
	}

	out->rotation = rotation % 360;
	if (flip != 0)
		out->flags |= IMG_CTRL_FLAG_HFLIP;
	else
		out->flags &= ~IMG_CTRL_FLAG_HFLIP;
}

static void mtk_dip_set_crop_config(struct mtk_dip_dev *dip_dev,
				    struct mtk_dip_dev_buffer *dev_buf_out,
				    struct img_output *iout, char *buf_name)
{
	iout->buffer.format.width = dev_buf_out->compose.width;
	iout->buffer.format.height = dev_buf_out->compose.height;

	mtk_dip_set_src_crop(&iout->crop, &dev_buf_out->crop);
}

static void mtk_dip_set_rotate_config(struct mtk_dip_dev *dip_dev,
				      struct mtk_dip_dev_buffer *dev_buf_in,
				  struct mtk_dip_dev_buffer *dev_buf_out,
				  struct img_output *iout, char *buf_name)
{
	mtk_dip_set_orientation(iout, dev_buf_out->rotation,
				dev_buf_out->hflip, dev_buf_out->vflip);
}

void mtk_dip_pipe_ipi_params_config(struct mtk_dip_request *req)
{
	struct mtk_dip_pipe *pipe = req->dip_pipe;
	int buf_out_idx;
	int buf_in_idx;
	struct img_ipi_frameparam *dip_param = &req->img_fparam.frameparam;
	struct mtk_dip_dev_buffer *buf_in;
	struct mtk_dip_dev_buffer *buf_out;
	struct mtk_dip_dev_buffer *buf_tuning;
	int i = 0;
	int mdp_ids[2] = {MTK_DIP_VIDEO_NODE_ID_MDP0_CAPTURE,
			  MTK_DIP_VIDEO_NODE_ID_MDP1_CAPTURE};
	char *mdp_names[2] = {"mdp0", "mdp1"};

	memset(dip_param, 0, sizeof(*dip_param));
	dip_param->index = req->id;
	dip_param->type = STREAM_ISP_IC;

	/* Tuning buffer */
	buf_tuning =
		req->buf_map[MTK_DIP_VIDEO_NODE_ID_TUNING_OUT];
	if (buf_tuning) {
		dip_param->tuning_data.pa =
			(uint32_t)buf_tuning->scp_daddr[0];
		dip_param->tuning_data.present = true;
		dip_param->tuning_data.iova =
			(uint32_t)buf_tuning->isp_daddr[0];
	}

	buf_in_idx = 0;

	/*
	 * Raw-in buffer
	 * The input buffer is guaranteed by .request_validate()
	 */
	buf_in = req->buf_map[MTK_DIP_VIDEO_NODE_ID_RAW_OUT];
	mtk_dip_fill_input_ipi_param
		(pipe, &dip_param->inputs[buf_in_idx++],
		 buf_in, "RAW");

	/* NR buffer (optional input) */
	if (req->buf_map[MTK_DIP_VIDEO_NODE_ID_NR_OUT])
		mtk_dip_fill_input_ipi_param
			(pipe, &dip_param->inputs[buf_in_idx++],
			 req->buf_map[MTK_DIP_VIDEO_NODE_ID_NR_OUT],
			 "NR");

	/* Shading buffer (optional input)*/
	if (req->buf_map[MTK_DIP_VIDEO_NODE_ID_SHADING_OUT])
		mtk_dip_fill_input_ipi_param
			(pipe, &dip_param->inputs[buf_in_idx++],
			 req->buf_map[MTK_DIP_VIDEO_NODE_ID_SHADING_OUT],
			 "Shading");

	/* MDP buffers */
	buf_out_idx = 0;

	for (i = 0; i < ARRAY_SIZE(mdp_ids); i++) {
		buf_out =
			req->buf_map[mdp_ids[i]];
		if (buf_out) {
			struct img_output *iout =
				&dip_param->outputs[buf_out_idx++];

			mtk_dip_fill_output_ipi_param(pipe, iout, buf_out,
						      buf_in, mdp_names[i]);
			mtk_dip_set_crop_config(pipe->dip_dev, buf_out,
						iout, mdp_names[i]);

			/* MDP 0 support rotation */
			if (i == 0)
				mtk_dip_set_rotate_config(pipe->dip_dev,
							  buf_in, buf_out, iout,
							  mdp_names[i]);
		}
	}

	/* IMG2O buffer */
	buf_out = req->buf_map[MTK_DIP_VIDEO_NODE_ID_IMG2_CAPTURE];
	if (req->buf_map[MTK_DIP_VIDEO_NODE_ID_IMG2_CAPTURE])
		mtk_dip_fill_output_ipi_param
			(pipe, &dip_param->outputs[buf_out_idx++],
			 buf_out, buf_in, "IMG2O");

	/* IMG3O buffer */
	buf_out = req->buf_map[MTK_DIP_VIDEO_NODE_ID_IMG3_CAPTURE];
	if (buf_out)
		mtk_dip_fill_output_ipi_param
			(pipe, &dip_param->outputs[buf_out_idx++],
			 buf_out, buf_in, "IMG3O");

	dip_param->num_outputs = buf_out_idx;
	dip_param->num_inputs = buf_in_idx;
}

void mtk_dip_pipe_try_enqueue(struct mtk_dip_pipe *pipe)
{
	struct mtk_dip_request *req, *tmp_req;
	unsigned long flags;

	if (!pipe->streaming)
		return;

	spin_lock_irqsave(&pipe->job_lock, flags);
	list_for_each_entry_safe(req, tmp_req,
				 &pipe->pipe_job_pending_list, list) {
		list_del(&req->list);
		pipe->num_pending_jobs--;
		list_add_tail(&req->list,
			      &pipe->pipe_job_running_list);
		pipe->num_jobs++;
		mtk_dip_hw_enqueue(pipe->dip_dev, req);
	}
	spin_unlock_irqrestore(&pipe->job_lock, flags);
}
