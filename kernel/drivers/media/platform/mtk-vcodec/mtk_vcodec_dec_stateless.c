// SPDX-License-Identifier: GPL-2.0

#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <linux/module.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_drv_if.h"
#include "mtk_vcodec_dec_pm.h"

/**
 * struct mtk_stateless_control  - CID control type
 * @id: CID control id
 * @v4l2_ctrl_type: CID control type
 * @codec_type codec type (V4L2 pixel format) for CID control type
 */
struct mtk_stateless_control {
	u32 id;
	enum v4l2_ctrl_type type;
	int codec_type;
};

static const struct mtk_stateless_control mtk_stateless_controls[] = {
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SPS,
		.type = V4L2_CTRL_TYPE_H264_SPS,
		.codec_type = V4L2_PIX_FMT_H264_SLICE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PPS,
		.type = V4L2_CTRL_TYPE_H264_PPS,
		.codec_type = V4L2_PIX_FMT_H264_SLICE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SCALING_MATRIX,
		.type = V4L2_CTRL_TYPE_H264_SCALING_MATRIX,
		.codec_type = V4L2_PIX_FMT_H264_SLICE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SLICE_PARAMS,
		.type = V4L2_CTRL_TYPE_H264_SLICE_PARAMS,
		.codec_type = V4L2_PIX_FMT_H264_SLICE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_DECODE_PARAMS,
		.type = V4L2_CTRL_TYPE_H264_DECODE_PARAMS,
		.codec_type = V4L2_PIX_FMT_H264_SLICE,
	},
};

#define NUM_CTRLS ARRAY_SIZE(mtk_stateless_controls)

static const struct mtk_video_fmt mtk_video_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_H264_SLICE,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MM21,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
};

#define NUM_FORMATS ARRAY_SIZE(mtk_video_formats)
#define DEFAULT_OUT_FMT_IDX    0
#define DEFAULT_CAP_FMT_IDX    1

static const struct mtk_codec_framesizes mtk_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264_SLICE,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(mtk_vdec_framesizes)

static void mtk_vdec_stateless_out_to_done(struct mtk_vcodec_ctx *ctx,
					   struct mtk_vcodec_mem *bs, int error)
{
	struct mtk_video_dec_buf *out_buf;
	struct vb2_v4l2_buffer *vb;

	if (bs == NULL) {
		mtk_v4l2_err("Free bitstream buffer fail.");
		return;
	}
	out_buf = container_of(bs, struct mtk_video_dec_buf, bs_buffer);
	vb = &out_buf->m2m_buf.vb;

	if (out_buf->used) {
		mtk_v4l2_debug(2,
			"Free bitsteam buffer id = %d to done_list",
			vb->vb2_buf.index);

		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (error) {
			v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
			if (error == -EIO)
				out_buf->error = true;
		} else
			v4l2_m2m_buf_done(vb, VB2_BUF_STATE_DONE);
		out_buf->used = false;
	} else
		mtk_v4l2_err("Free bitsteam buffer id = %d not used",
				vb->vb2_buf.index);
}

static void mtk_vdec_stateless_cap_to_disp(struct mtk_vcodec_ctx *ctx,
					   struct vdec_fb *fb, int error)
{
	struct mtk_video_dec_buf *vdec_frame_buf;
	struct vb2_v4l2_buffer *vb;
	unsigned int cap_y_size = 0, cap_c_size = 0;

	if (fb == NULL) {
		mtk_v4l2_err("Free frame buffer fail.");
		return;
	}
	vdec_frame_buf = container_of(fb, struct mtk_video_dec_buf,
				      frame_buffer);
	vb = &vdec_frame_buf->m2m_buf.vb;
	if (error == 1) {
		cap_y_size = 0;
		cap_c_size = 0;
	} else {
		cap_y_size = ctx->q_data[MTK_Q_DATA_DST].sizeimage[0];
		cap_c_size = ctx->q_data[MTK_Q_DATA_DST].sizeimage[1];
	}

	if (vdec_frame_buf->used) {
		v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

		vb2_set_plane_payload(&vb->vb2_buf, 0,
			cap_y_size);
		if (ctx->q_data[MTK_Q_DATA_DST].fmt->num_planes == 2)
			vb2_set_plane_payload(&vb->vb2_buf, 1,
				cap_c_size);

		mtk_v4l2_debug(2,
			"Free frame buffer id = %d to done_list",
			vb->vb2_buf.index);
		if (error == 1)
			vb->flags |= V4L2_BUF_FLAG_LAST;
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_DONE);
		vdec_frame_buf->used = false;
	} else
		mtk_v4l2_err("Free frame buffer id = %d not used",
				vb->vb2_buf.index);
}

static struct vdec_fb *vdec_get_cap_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_video_dec_buf *framebuf;
	struct vb2_v4l2_buffer *vb2_v4l2;
	struct vb2_buffer *dst_buf;
	struct vdec_fb *pfb;

	vb2_v4l2 = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (vb2_v4l2 == NULL) {
		mtk_v4l2_debug(1, "[%d] dst_buf empty!!", ctx->id);
		return NULL;
	}

	dst_buf = &vb2_v4l2->vb2_buf;
	framebuf = container_of(vb2_v4l2, struct mtk_video_dec_buf, m2m_buf.vb);

	pfb = &framebuf->frame_buffer;
	pfb->base_y.va = vb2_plane_vaddr(dst_buf, 0);
	pfb->base_y.dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	pfb->base_y.size = ctx->q_data[MTK_Q_DATA_DST].sizeimage[0];

	if (ctx->q_data[MTK_Q_DATA_DST].fmt->num_planes == 2) {
		pfb->base_c.va = vb2_plane_vaddr(dst_buf, 1);
		pfb->base_c.dma_addr =
			vb2_dma_contig_plane_dma_addr(dst_buf, 1);
		pfb->base_c.size = ctx->q_data[MTK_Q_DATA_DST].sizeimage[1];
	}
	mtk_v4l2_debug(1,
		"id=%d Framebuf  pfb=%p VA=%p Y_DMA=%pad C_DMA=%pad Size=%zx frame_count = %d",
		dst_buf->index, pfb,
		pfb->base_y.va, &pfb->base_y.dma_addr,
		&pfb->base_c.dma_addr, pfb->base_y.size,
		ctx->decoded_frame_cnt);

	return pfb;
}

static void vb2ops_vdec_buf_request_complete(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req, &ctx->ctrl_hdl);
}

static int fops_media_request_validate(struct media_request *mreq)
{
	struct mtk_vcodec_ctx *ctx = NULL;
	struct media_request_object *req_obj;
	struct v4l2_ctrl_handler *parent_hdl, *hdl;
	struct v4l2_ctrl *ctrl_test;
	unsigned int buffer_cnt;
	unsigned int i;

	buffer_cnt = vb2_request_buffer_cnt(mreq);
	if (!buffer_cnt) {
		mtk_v4l2_err("Request count is zero");
		return -ENOENT;
	} else if (buffer_cnt > 1) {
		mtk_v4l2_err("Request count is too much %d", buffer_cnt);
		return -EINVAL;
	}

	list_for_each_entry(req_obj, &mreq->objects, list) {
		struct vb2_buffer *vb;

		if (vb2_request_object_is_buffer(req_obj)) {
			vb = container_of(req_obj, struct vb2_buffer, req_obj);
			ctx = vb2_get_drv_priv(vb->vb2_queue);
			break;
		}
	}

	if (!ctx)
		return -ENOENT;

	parent_hdl = &ctx->ctrl_hdl;

	hdl = v4l2_ctrl_request_hdl_find(mreq, parent_hdl);
	if (!hdl) {
		mtk_v4l2_err("Missing codec control(s)\n");
		return -ENOENT;
	}

	for (i = 0; i < NUM_CTRLS; i++) {
		if (mtk_stateless_controls[i].codec_type != ctx->current_codec)
			continue;

		ctrl_test = v4l2_ctrl_request_hdl_ctrl_find(hdl,
					  mtk_stateless_controls[i].id);
		if (!ctrl_test) {
			mtk_v4l2_err("Missing required codec control\n");
			return -ENOENT;
		}
	}

	v4l2_ctrl_request_hdl_put(hdl);

	return vb2_request_validate(mreq);
}

static void mtk_vdec_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx =
		container_of(work, struct mtk_vcodec_ctx, decode_work);
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vb2_buffer *src_buf;
	struct vdec_fb *dst_buf;
	struct mtk_vcodec_mem *buf;
	struct mtk_video_dec_buf *src_buf_info;
	struct vb2_v4l2_buffer *src_vb2_v4l2;
	struct media_request *src_buf_req;
	bool res_chg = false;
	int ret;

	src_vb2_v4l2 = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_vb2_v4l2 == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] src_buf empty!!", ctx->id);
		return;
	}

	src_buf = &src_vb2_v4l2->vb2_buf;
	src_buf_info = container_of(src_vb2_v4l2, struct mtk_video_dec_buf,
				    m2m_buf.vb);

	mtk_v4l2_debug(3, "[%d] (%d) id=%d, vb=%p buf_info = %p",
			ctx->id, src_buf->vb2_queue->type,
			src_buf->index, src_buf, src_buf_info);

	buf = &src_buf_info->bs_buffer;
	buf->va = vb2_plane_vaddr(src_buf, 0);
	buf->dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	buf->size = (size_t)src_buf->planes[0].bytesused;
	if (!buf->va) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_err("[%d] id=%d src_addr is NULL!!",
				ctx->id, src_buf->index);
		return;
	}

	mtk_v4l2_debug(3, "[%d] Bitstream VA=%p DMA=%pad Size=%zx vb=%p",
			ctx->id, buf->va, &buf->dma_addr, buf->size, src_buf);
	/* Apply request controls. */
	src_buf_req = src_vb2_v4l2->vb2_buf.req_obj.req;
	if (src_buf_req)
		v4l2_ctrl_request_setup(src_buf_req, &ctx->ctrl_hdl);
	else
		mtk_v4l2_err("vb2 buffer media request is NULL");

	dst_buf = vdec_get_cap_buffer(ctx);

	v4l2_m2m_buf_copy_metadata(src_vb2_v4l2,
				   v4l2_m2m_next_dst_buf(ctx->m2m_ctx), true);
	ret = vdec_if_decode(ctx, buf, dst_buf, &res_chg);
	if (ret) {
		mtk_v4l2_err(
			" <===[%d], src_buf[%d] sz=0x%zx pts=%llu vdec_if_decode() ret=%d res_chg=%d===>",
			ctx->id,
			src_buf->index,
			buf->size,
			src_buf->timestamp,
			ret, res_chg);
		if (ret == -EIO) {
			mutex_lock(&ctx->lock);
			src_buf_info->error = true;
			mutex_unlock(&ctx->lock);
		}
	}

	mtk_vdec_stateless_out_to_done(ctx, buf, ret);
	if (!ret)
		mtk_vdec_stateless_cap_to_disp(ctx, dst_buf, 0);

	v4l2_ctrl_request_complete(src_buf_req, &ctx->ctrl_hdl);

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
}

static void vb2ops_vdec_stateless_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct mtk_video_dec_buf *dst_buf = NULL;
	struct mtk_video_dec_buf *src_buf = NULL;

	mtk_v4l2_debug(3, "[%d] (%d) id=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb);

	/*
	 * check if this buffer is ready to be used after decode
	 */
	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		dst_buf = container_of(vb2_v4l2, struct mtk_video_dec_buf,
				       m2m_buf.vb);
		mutex_lock(&ctx->lock);
		if (dst_buf->used)
			mtk_v4l2_err("Capture buffer in used (%d).", vb->index);
		else {
			dst_buf->used = true;
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
		}
		mutex_unlock(&ctx->lock);
		return;
	}

	src_buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, m2m_buf.vb);
	mutex_lock(&ctx->lock);
	if (src_buf->used)
		mtk_v4l2_err("Output buffer still in used (%d).", vb->index);
	else {
		src_buf->used = true;
		v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
	}
	mutex_unlock(&ctx->lock);

	mtk_v4l2_debug(3, "(%d) id=%d, bs=%p used = %d",
		vb->vb2_queue->type, vb->index, src_buf, src_buf->used);

	if (ctx->state == MTK_STATE_INIT) {
		ctx->state = MTK_STATE_HEADER;
		mtk_v4l2_debug(1, "Init driver from init to header.");
	} else
		mtk_v4l2_debug(3, "[%d] already init driver %d",
				ctx->id, ctx->state);

}

static int mtk_vdec_flush_decoder(struct mtk_vcodec_ctx *ctx)
{
	bool res_chg;

	return vdec_if_decode(ctx, NULL, NULL, &res_chg);
}

static const struct v4l2_ctrl_ops mtk_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = mtk_vdec_g_v_ctrl,
};

static int mtk_vcodec_dec_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;
	unsigned int i;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, NUM_CTRLS);
	if (ctx->ctrl_hdl.error) {
		mtk_v4l2_err("v4l2_ctrl_handler_init failed\n");
		return ctx->ctrl_hdl.error;
	}

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				0, 32, 1, 1);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	/*
	 * H264. Baseline / Extended decoding is not supported.
	 */
	v4l2_ctrl_new_std_menu(&ctx->ctrl_hdl,
			&mtk_vcodec_dec_ctrl_ops,
			V4L2_CID_MPEG_VIDEO_H264_PROFILE,
			V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
			BIT(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
			BIT(V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED),
			V4L2_MPEG_VIDEO_H264_PROFILE_MAIN);
	if (ctx->ctrl_hdl.error) {
		mtk_v4l2_err("Adding control failed %d",
				ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	for (i = 0; i < NUM_CTRLS; i++) {
		struct v4l2_ctrl_config cfg = { 0 };

		cfg.ops = &mtk_vcodec_dec_ctrl_ops;
		cfg.id = mtk_stateless_controls[i].id;
		cfg.type = mtk_stateless_controls[i].type;

		v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &cfg, NULL);
		if (ctx->ctrl_hdl.error) {
			mtk_v4l2_err("Adding control failed %d",
					ctx->ctrl_hdl.error);
			return ctx->ctrl_hdl.error;
		}
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

	return 0;
}

const struct media_device_ops mtk_vcodec_media_ops = {
	.req_validate	= fops_media_request_validate,
	.req_queue	= v4l2_m2m_request_queue,
};

static void mtk_init_vdec_params(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_queue *src_vq;

	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				 V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

	/* Support request api for output plane */
	src_vq->supports_requests = true;
	src_vq->requires_requests = true;
}

static int vb2ops_vdec_out_buf_validate(struct vb2_buffer *vb)
{
	return 0;
}

static struct vb2_ops mtk_vdec_request_vb2_ops = {
	.queue_setup	= vb2ops_vdec_queue_setup,
	.buf_prepare	= vb2ops_vdec_buf_prepare,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.start_streaming	= vb2ops_vdec_start_streaming,

	.buf_queue	= vb2ops_vdec_stateless_buf_queue,
	.buf_out_validate = vb2ops_vdec_out_buf_validate,
	.buf_init	= vb2ops_vdec_buf_init,
	.buf_finish	= vb2ops_vdec_buf_finish,
	.stop_streaming	= vb2ops_vdec_stop_streaming,
	.buf_request_complete = vb2ops_vdec_buf_request_complete,
};


const struct mtk_vcodec_dec_pdata mtk_req_8183_pdata = {
	.chip = MTK_MT8183,
	.init_vdec_params = mtk_init_vdec_params,
	.ctrls_setup = mtk_vcodec_dec_ctrls_setup,
	.vdec_vb2_ops = &mtk_vdec_request_vb2_ops,
	.vdec_formats = mtk_video_formats,
	.num_formats = NUM_FORMATS,
	.default_out_fmt = &mtk_video_formats[DEFAULT_OUT_FMT_IDX],
	.default_cap_fmt = &mtk_video_formats[DEFAULT_CAP_FMT_IDX],
	.vdec_framesizes = mtk_vdec_framesizes,
	.num_framesizes = NUM_SUPPORTED_FRAMESIZE,
	.uses_stateless_api = true,
	.worker = mtk_vdec_worker,
	.flush_decoder = mtk_vdec_flush_decoder,
};
