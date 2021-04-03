/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2014 Rockchip Electronics Co., Ltd.
 *	Alpha Lin <Alpha.Lin@rock-chips.com>
 *	Jeffy Chen <jeffy.chen@rock-chips.com>
 *
 * Copyright (C) 2014 Google, Inc.
 *	Tomasz Figa <tfiga@chromium.org>
 *
 * Based on s5p-mfc driver by Samsung Electronics Co., Ltd.
 *
 * Copyright (C) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "rockchip_vpu_common.h"

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <media/v4l2-event.h>
#include <linux/workqueue.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-sg.h>

#include "rockchip_vpu_enc.h"
#include "rockchip_vpu_hw.h"

#define V4L2_CID_PRIVATE_ROCKCHIP_HEADER	(V4L2_CID_CUSTOM_BASE + 0)
#define V4L2_CID_PRIVATE_ROCKCHIP_REG_PARAMS	(V4L2_CID_CUSTOM_BASE + 1)
#define V4L2_CID_PRIVATE_ROCKCHIP_HW_PARAMS	(V4L2_CID_CUSTOM_BASE + 2)
#define V4L2_CID_PRIVATE_ROCKCHIP_RET_PARAMS	(V4L2_CID_CUSTOM_BASE + 3)

static const struct rockchip_vpu_fmt *find_format(struct rockchip_vpu_dev *dev,
						  u32 fourcc, bool bitstream)
{
	const struct rockchip_vpu_fmt *formats = dev->variant->enc_fmts;
	unsigned int i;

	vpu_debug_enter();

	for (i = 0; i < dev->variant->num_enc_fmts; i++) {
		if (formats[i].fourcc != fourcc)
			continue;
		if (bitstream && formats[i].codec_mode != RK_VPU_CODEC_NONE)
			return &formats[i];
		if (!bitstream && formats[i].codec_mode == RK_VPU_CODEC_NONE)
			return &formats[i];
	}

	return NULL;
}

static const struct rockchip_vpu_fmt *get_def_fmt(struct rockchip_vpu_dev *dev,
						  bool bitstream)
{
	const struct rockchip_vpu_fmt *formats = dev->variant->enc_fmts;
	unsigned int i;

	for (i = 0; i < dev->variant->num_enc_fmts; i++) {
		if (bitstream == (formats[i].codec_mode != RK_VPU_CODEC_NONE))
			return &formats[i];
	}

	/* There must be at least one raw and one coded format in the array. */
	BUG_ON(i >= dev->variant->num_enc_fmts);
	return NULL;
}

/*
 * Indices of controls that need to be accessed directly, i.e. through
 * p_cur.p pointer of their v4l2_ctrl structs.
 */
enum {
	ROCKCHIP_VPU_ENC_CTRL_HEADER,
	ROCKCHIP_VPU_ENC_CTRL_REG_PARAMS,
	ROCKCHIP_VPU_ENC_CTRL_HW_PARAMS,
	ROCKCHIP_VPU_ENC_CTRL_RET_PARAMS,
	ROCKCHIP_VPU_ENC_CTRL_Y_QUANT_TBL,
	ROCKCHIP_VPU_ENC_CTRL_C_QUANT_TBL,
};

static struct rockchip_vpu_control controls[] = {
	/* Private, per-frame controls. */
	[ROCKCHIP_VPU_ENC_CTRL_HEADER] = {
		.id = V4L2_CID_PRIVATE_ROCKCHIP_HEADER,
		.type = V4L2_CTRL_TYPE_PRIVATE,
		.name = "Rockchip Private Header",
		.elem_size = ROCKCHIP_HEADER_SIZE,
		.max_stores = VIDEO_MAX_FRAME,
		.can_store = true,
	},
	[ROCKCHIP_VPU_ENC_CTRL_REG_PARAMS] = {
		.id = V4L2_CID_PRIVATE_ROCKCHIP_REG_PARAMS,
		.type = V4L2_CTRL_TYPE_PRIVATE,
		.name = "Rockchip Private Reg Params",
		.elem_size = sizeof(struct rockchip_reg_params),
		.max_stores = VIDEO_MAX_FRAME,
		.can_store = true,
	},
	[ROCKCHIP_VPU_ENC_CTRL_HW_PARAMS] = {
		.id = V4L2_CID_PRIVATE_ROCKCHIP_HW_PARAMS,
		.type = V4L2_CTRL_TYPE_PRIVATE,
		.name = "Rockchip Private Hw Params",
		.elem_size = ROCKCHIP_HW_PARAMS_SIZE,
		.max_stores = VIDEO_MAX_FRAME,
		.can_store = true,
	},
	[ROCKCHIP_VPU_ENC_CTRL_RET_PARAMS] = {
		.id = V4L2_CID_PRIVATE_ROCKCHIP_RET_PARAMS,
		.type = V4L2_CTRL_TYPE_PRIVATE,
		.name = "Rockchip Private Ret Params",
		.is_volatile = true,
		.is_read_only = true,
		.max_stores = VIDEO_MAX_FRAME,
		.elem_size = ROCKCHIP_RET_PARAMS_SIZE,
	},
	[ROCKCHIP_VPU_ENC_CTRL_Y_QUANT_TBL] = {
		.id = V4L2_CID_JPEG_LUMA_QUANTIZATION,
		.type = V4L2_CTRL_TYPE_U8,
		.minimum = 0,
		.maximum = 255,
		.step = 1,
		.dims = { 8, 8 }
	},
	[ROCKCHIP_VPU_ENC_CTRL_C_QUANT_TBL] = {
		.id = V4L2_CID_JPEG_CHROMA_QUANTIZATION,
		.type = V4L2_CTRL_TYPE_U8,
		.minimum = 0,
		.maximum = 255,
		.step = 1,
		.dims = { 8, 8 }
	},
	/* Generic controls. (currently ignored) */
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 150,
		.step = 1,
		.default_value = 30,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 10000,
		.maximum = 288000000,
		.step = 1,
		.default_value = 2097152,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.menu_skip_mask = ~(
				(1 << V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
				(1 << V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
				(1 << V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)
			),
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_4_1,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 30,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 18,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 288000,
		.step = 1,
		.default_value = 30000,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
		.type = V4L2_CTRL_TYPE_BUTTON,
	},
	/*
	 * This hardware does not support features provided by controls
	 * below, but they are required for compatibility with certain
	 * userspace software.
	 */
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Rate Control Reaction Coeff.",
		.minimum = 1,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.maximum = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		.default_value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Fixed Target Bit Enable",
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
};

static inline const void *get_ctrl_ptr(struct rockchip_vpu_ctx *ctx,
				       unsigned id)
{
	struct v4l2_ctrl *ctrl = ctx->ctrls[id];

	return ctrl->p_cur.p;
}

static const char *const *rockchip_vpu_enc_get_menu(u32 id)
{
	static const char *const vpu_video_frame_skip[] = {
		"Disabled",
		"Level Limit",
		"VBV/CPB Limit",
		NULL,
	};

	switch (id) {
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE:
		return vpu_video_frame_skip;
	}

	return NULL;
}

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct rockchip_vpu_dev *vpu = video_drvdata(file);

	vpu_debug_enter();

	strlcpy(cap->driver, vpu->dev->driver->name, sizeof(cap->driver));
	strlcpy(cap->card, vpu->vfd_enc->name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform: %s",
		 vpu->dev->driver->name);

	/*
	 * This is only a mem-to-mem video device.
	 */
	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	vpu_debug_leave();

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *prov,
				  struct v4l2_frmsizeenum *fsize)
{
	struct rockchip_vpu_dev *dev = video_drvdata(file);
	const struct rockchip_vpu_fmt *fmt;

	if (fsize->index != 0) {
		vpu_debug(0, "invalid frame size index (expected 0, got %d)\n",
				fsize->index);
		return -EINVAL;
	}

	fmt = find_format(dev, fsize->pixel_format, true);
	if (!fmt) {
		vpu_debug(0, "unsupported bitstream format (%08x)\n",
				fsize->pixel_format);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise = fmt->frmsize;

	return 0;
}

static int vidioc_enum_fmt(struct file *file, struct v4l2_fmtdesc *f, bool out)
{
	struct rockchip_vpu_dev *dev = video_drvdata(file);
	const struct rockchip_vpu_fmt *fmt;
	const struct rockchip_vpu_fmt *formats = dev->variant->enc_fmts;
	int i, j = 0;

	vpu_debug_enter();

	for (i = 0; i < dev->variant->num_enc_fmts; i++) {
		if (out && formats[i].codec_mode != RK_VPU_CODEC_NONE)
			continue;
		else if (!out && formats[i].codec_mode == RK_VPU_CODEC_NONE)
			continue;

		if (j == f->index) {
			fmt = &formats[i];
			strlcpy(f->description, fmt->name,
				sizeof(f->description));
			f->pixelformat = fmt->fourcc;

			f->flags = 0;
			if (formats[i].codec_mode != RK_VPU_CODEC_NONE)
				f->flags |= V4L2_FMT_FLAG_COMPRESSED;

			vpu_debug_leave();

			return 0;
		}

		++j;
	}

	vpu_debug_leave();

	return -EINVAL;
}

static int vidioc_enum_fmt_vid_cap_mplane(struct file *file, void *priv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(file, f, false);
}

static int vidioc_enum_fmt_vid_out_mplane(struct file *file, void *priv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(file, f, true);
}

static int vidioc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);

	vpu_debug_enter();

	vpu_debug(4, "f->type = %d\n", f->type);

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		f->fmt.pix_mp = ctx->dst_fmt;
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		f->fmt.pix_mp = ctx->src_fmt;
		break;

	default:
		vpu_err("invalid buf type\n");
		return -EINVAL;
	}

	vpu_debug_leave();

	return 0;
}

static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct rockchip_vpu_dev *dev = video_drvdata(file);
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	const struct rockchip_vpu_fmt *fmt;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	char str[5];
	unsigned long dma_align;
	int i;
	bool need_alignment;

	vpu_debug_enter();

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vpu_debug(4, "%s\n", fmt2str(f->fmt.pix_mp.pixelformat, str));

		fmt = find_format(dev, pix_fmt_mp->pixelformat, true);
		if (!fmt) {
			fmt = ctx->vpu_dst_fmt;
			pix_fmt_mp->pixelformat = fmt->fourcc;
		}

		if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
			vpu_err("must be set encoding output size\n");
			return -EINVAL;
		}

		pix_fmt_mp->plane_fmt[0].bytesperline = 0;
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vpu_debug(4, "%s\n", fmt2str(f->fmt.pix_mp.pixelformat, str));

		fmt = find_format(dev, pix_fmt_mp->pixelformat, false);
		if (!fmt) {
			fmt = ctx->vpu_src_fmt;
			pix_fmt_mp->pixelformat = fmt->fourcc;
		}

		pix_fmt_mp->num_planes = fmt->num_mplanes;

		/* Limit to hardware min/max. */
		pix_fmt_mp->width = clamp(pix_fmt_mp->width,
				ctx->vpu_dst_fmt->frmsize.min_width,
				ctx->vpu_dst_fmt->frmsize.max_width);
		pix_fmt_mp->height = clamp(pix_fmt_mp->height,
				ctx->vpu_dst_fmt->frmsize.min_height,
				ctx->vpu_dst_fmt->frmsize.max_height);
		/* Round up to macroblocks. */
		pix_fmt_mp->width = round_up(pix_fmt_mp->width, MB_DIM);
		pix_fmt_mp->height = round_up(pix_fmt_mp->height, MB_DIM);

		vpu_debug(0, "OUTPUT codec mode: %d\n", fmt->codec_mode);
		vpu_debug(0, "fmt - w: %d, h: %d, mb - w: %d, h: %d\n",
			  pix_fmt_mp->width, pix_fmt_mp->height,
			  MB_WIDTH(pix_fmt_mp->width),
			  MB_HEIGHT(pix_fmt_mp->height));

		/* Fill in remaining fields. */
		rockchip_vpu_update_planes(fmt, pix_fmt_mp);

		/* TODO(crbug.com/824662): handle sizeimage in Chromium.
		 * Align plane size to full cache line by adjusting the height.
		 * Considering the I420 format, the width is multiple of MB_DIM,
		 * the size of uv plane is 1/4 of y plane. So the height should
		 * be multiple of (cache * 4 / MB_DIM).
		 */
		dma_align = dma_get_cache_alignment();
		need_alignment = false;
		for (i = 0; i < fmt->num_mplanes; i++) {
			if (!IS_ALIGNED(pix_fmt_mp->plane_fmt[i].sizeimage,
					dma_align)) {
				need_alignment = true;
				break;
			}
		}
		if (need_alignment) {
			pix_fmt_mp->height = round_up(pix_fmt_mp->height,
						      dma_align * 4 / MB_DIM);
			if (pix_fmt_mp->height >
			    ctx->vpu_dst_fmt->frmsize.max_height) {
				vpu_err("Aligned height higher than maximum.\n");
				return -EINVAL;
			}
			/* Fill in remaining fields again. */
			rockchip_vpu_update_planes(fmt, pix_fmt_mp);
		}
		break;

	default:
		vpu_err("invalid buf type\n");
		return -EINVAL;
	}

	vpu_debug_leave();

	return 0;
}

static void reset_src_fmt(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	const struct rockchip_vpu_fmt *vpu_dst_fmt = ctx->vpu_dst_fmt;
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;

	ctx->vpu_src_fmt = get_def_fmt(vpu, false);

	memset(src_fmt, 0, sizeof(*src_fmt));

	src_fmt->width = vpu_dst_fmt->frmsize.min_width;
	src_fmt->height = vpu_dst_fmt->frmsize.min_height;
	src_fmt->pixelformat = ctx->vpu_src_fmt->fourcc;
	src_fmt->num_planes = ctx->vpu_src_fmt->num_mplanes;

	rockchip_vpu_update_planes(ctx->vpu_src_fmt, src_fmt);
}

static int vidioc_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	struct rockchip_vpu_dev *dev = ctx->dev;
	int ret = 0;

	vpu_debug_enter();

	/* Change not allowed if any queue is streaming. */
	if (vb2_is_streaming(&ctx->vq_src) || vb2_is_streaming(&ctx->vq_dst)) {
		ret = -EBUSY;
		goto out;
	}

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		/*
		 * Pixel format change is not allowed when the other queue has
		 * buffers allocated.
		 */
		if (vb2_is_busy(&ctx->vq_src)
		    && pix_fmt_mp->pixelformat != ctx->dst_fmt.pixelformat) {
			ret = -EBUSY;
			goto out;
		}

		ret = vidioc_try_fmt(file, priv, f);
		if (ret)
			goto out;

		ctx->vpu_dst_fmt = find_format(dev, pix_fmt_mp->pixelformat,
					       true);
		ctx->dst_fmt = *pix_fmt_mp;

		/*
		 * Current raw format might have become invalid with newly
		 * selected codec, so reset it to default just to be safe and
		 * keep internal driver state sane. User is mandated to set
		 * the raw format again after we return, so we don't need
		 * anything smarter.
		 */
		reset_src_fmt(ctx);
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		/*
		 * Pixel format change is not allowed when the other queue has
		 * buffers allocated.
		 */
		if (vb2_is_busy(&ctx->vq_dst)
		    && pix_fmt_mp->pixelformat != ctx->src_fmt.pixelformat) {
			ret = -EBUSY;
			goto out;
		}

		ret = vidioc_try_fmt(file, priv, f);
		if (ret)
			goto out;

		ctx->vpu_src_fmt = find_format(dev, pix_fmt_mp->pixelformat,
					       false);

		/* Reset crop rectangle. */
		ctx->src_crop.width = pix_fmt_mp->width;
		ctx->src_crop.height = pix_fmt_mp->height;

		ctx->src_fmt = *pix_fmt_mp;
		break;

	default:
		vpu_err("invalid buf type\n");
		ret = -EINVAL;
	}

out:
	vpu_debug_leave();

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *reqbufs)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;

	vpu_debug_enter();

	switch (reqbufs->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vpu_debug(4, "\n");

		ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
		if (ret != 0) {
			vpu_err("error in vb2_reqbufs() for E(D)\n");
			goto out;
		}
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vpu_debug(4, "memory type %d\n", reqbufs->memory);

		ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
		if (ret != 0) {
			vpu_err("error in vb2_reqbufs() for E(S)\n");
			goto out;
		}
		break;

	default:
		vpu_err("invalid buf type\n");
		ret = -EINVAL;
		goto out;
	}

out:
	vpu_debug_leave();

	return ret;
}

static int vidioc_querybuf(struct file *file, void *priv,
			   struct v4l2_buffer *buf)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;

	vpu_debug_enter();

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_querybuf(&ctx->vq_dst, buf);
		if (ret != 0) {
			vpu_err("error in vb2_querybuf() for E(D)\n");
			goto out;
		}

		buf->m.planes[0].m.mem_offset += DST_QUEUE_OFF_BASE;
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = vb2_querybuf(&ctx->vq_src, buf);
		if (ret != 0) {
			vpu_err("error in vb2_querybuf() for E(S)\n");
			goto out;
		}
		break;

	default:
		vpu_err("invalid buf type\n");
		ret = -EINVAL;
		goto out;
	}

out:
	vpu_debug_leave();

	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;
	int i;

	vpu_debug_enter();

	for (i = 0; i < buf->length; i++) {
		vpu_debug(4, "plane[%d]->length %d bytesused %d\n",
			  i, buf->m.planes[i].length,
			  buf->m.planes[i].bytesused);
	}

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = vb2_qbuf(&ctx->vq_src, buf);
		vpu_debug(4, "vb2_qbuf return %d\n", ret);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_qbuf(&ctx->vq_dst, buf);
		vpu_debug(4, "vb2_qbuf return %d\n", ret);
		break;

	default:
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;

	vpu_debug_enter();

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = vb2_dqbuf(&ctx->vq_src, buf, file->f_flags & O_NONBLOCK);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_dqbuf(&ctx->vq_dst, buf, file->f_flags & O_NONBLOCK);
		if (!ret)
			buf->m.planes[0].m.mem_offset += DST_QUEUE_OFF_BASE;
		break;

	default:
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int vidioc_expbuf(struct file *file, void *priv,
			 struct v4l2_exportbuffer *eb)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;

	vpu_debug_enter();

	switch (eb->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = vb2_expbuf(&ctx->vq_src, eb);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_expbuf(&ctx->vq_dst, eb);
		break;

	default:
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;

	vpu_debug_enter();

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = vb2_streamon(&ctx->vq_src, type);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_streamon(&ctx->vq_dst, type);
		break;

	default:
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret;

	vpu_debug_enter();

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = vb2_streamoff(&ctx->vq_src, type);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_streamoff(&ctx->vq_dst, type);
		break;

	default:
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_enc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct rockchip_vpu_ctx *ctx = ctrl_to_ctx(ctrl);
	struct rockchip_vpu_dev *dev = ctx->dev;
	int ret = 0;

	vpu_debug_enter();

	vpu_debug(4, "ctrl id %d\n", ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
	case V4L2_CID_MPEG_VIDEO_BITRATE:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
	case V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE:
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
	case V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF:
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
	case V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT:
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR:
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		/* Ignore these controls for now. (FIXME?) */
		break;

	case V4L2_CID_PRIVATE_ROCKCHIP_HEADER:
	case V4L2_CID_PRIVATE_ROCKCHIP_REG_PARAMS:
	case V4L2_CID_PRIVATE_ROCKCHIP_HW_PARAMS:
	case V4L2_CID_JPEG_LUMA_QUANTIZATION:
	case V4L2_CID_JPEG_CHROMA_QUANTIZATION:
		/* Nothing to do here. The control is used directly. */
		break;

	default:
		v4l2_err(&dev->v4l2_dev, "Invalid control, id=%d, val=%d\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_enc_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct rockchip_vpu_ctx *ctx = ctrl_to_ctx(ctrl);
	struct rockchip_vpu_dev *dev = ctx->dev;
	int ret = 0;

	vpu_debug_enter();

	vpu_debug(4, "ctrl id %d\n", ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_ROCKCHIP_RET_PARAMS:
		memcpy(ctrl->p_new.p, ctx->run.priv_dst.cpu,
			ROCKCHIP_RET_PARAMS_SIZE);
		break;

	default:
		v4l2_err(&dev->v4l2_dev, "Invalid control, id=%d, val=%d\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static const struct v4l2_ctrl_ops rockchip_vpu_enc_ctrl_ops = {
	.s_ctrl = rockchip_vpu_enc_s_ctrl,
	.g_volatile_ctrl = rockchip_vpu_enc_g_volatile_ctrl,
};

static int vidioc_cropcap(struct file *file, void *priv,
			  struct v4l2_cropcap *cap)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *fmt = &ctx->src_fmt;
	int ret = 0;

	vpu_debug_enter();

	/* Crop only supported on source. */
	if (cap->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = -EINVAL;
		goto out;
	}

	cap->bounds.left = 0;
	cap->bounds.top = 0;
	cap->bounds.width = fmt->width;
	cap->bounds.height = fmt->height;
	cap->defrect = cap->bounds;
	cap->pixelaspect.numerator = 1;
	cap->pixelaspect.denominator = 1;

out:
	vpu_debug_leave();

	return ret;
}

static int vidioc_g_crop(struct file *file, void *priv, struct v4l2_crop *crop)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	int ret = 0;

	vpu_debug_enter();

	/* Crop only supported on source. */
	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = -EINVAL;
		goto out;
	}

	crop->c = ctx->src_crop;

out:
	vpu_debug_leave();

	return ret;
}

static int vidioc_s_crop(struct file *file, void *priv,
			 const struct v4l2_crop *crop)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *fmt = &ctx->src_fmt;
	const struct v4l2_rect *rect = &crop->c;
	int ret = 0;

	vpu_debug_enter();

	/* Crop only supported on source. */
	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = -EINVAL;
		goto out;
	}

	/* Change not allowed if the queue is streaming. */
	if (vb2_is_streaming(&ctx->vq_src)) {
		ret = -EBUSY;
		goto out;
	}

	/* We do not support offsets. */
	if (rect->left != 0 || rect->top != 0)
		goto fallback;

	/* We can crop only inside right- or bottom-most macroblocks. */
	if (round_up(rect->width, MB_DIM) != fmt->width
	    || round_up(rect->height, MB_DIM) != fmt->height)
		goto fallback;

	/* We support widths aligned to 4 pixels and arbitrary heights. */
	ctx->src_crop.width = round_up(rect->width, 4);
	ctx->src_crop.height = rect->height;

	vpu_debug_leave();

	return 0;

fallback:
	/* Default to full frame for incorrect settings. */
	ctx->src_crop.width = fmt->width;
	ctx->src_crop.height = fmt->height;

out:
	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_encoder_cmd(struct rockchip_vpu_ctx *ctx,
				    struct v4l2_encoder_cmd *cmd, bool try)
{
	struct rockchip_vpu_dev *dev = ctx->dev;
	unsigned long flags;
	int ret = 0;

	switch (cmd->cmd) {
	case V4L2_ENC_CMD_STOP:
	case V4L2_ENC_CMD_START:
		if (cmd->flags != 0) {
			vpu_err("Invalid flags for encoder command (%u)",
				cmd->flags);
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	switch (cmd->cmd) {
	case V4L2_ENC_CMD_STOP:
		if (!list_empty(&ctx->flush_buf.list)) {
			vpu_err("Cannot stop while flush already in progress");
			ret = -EBUSY;
			break;
		}

		if (!ctx->stopped && !try)
			list_add_tail(&ctx->flush_buf.list, &ctx->src_queue);
		break;

	case V4L2_ENC_CMD_START:
		if (!list_empty(&ctx->flush_buf.list)
		    || (ctx->stopped && !ctx->vq_dst.last_buffer_dequeued)) {
			vpu_err("Cannot restart with flush still in progress");
			ret = -EBUSY;
			break;
		}

		if (!vb2_is_streaming(&ctx->vq_src)) {
			vpu_err("Cannot start with OUTPUT queue not streaming");
			ret = -EINVAL;
			break;
		}

		if (!try) {
			vb2_clear_last_buffer_dequeued(&ctx->vq_dst);
			ctx->stopped = false;
		}
		break;
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (!try)
		rockchip_vpu_try_context(dev, ctx);

	return ret;
}

static int vidioc_try_encoder_cmd(struct file *file, void *priv,
				 struct v4l2_encoder_cmd *cmd)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);

	return rockchip_vpu_encoder_cmd(ctx, cmd, true);
}

static int vidioc_encoder_cmd(struct file *file, void *priv,
			      struct v4l2_encoder_cmd *cmd)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(priv);

	return rockchip_vpu_encoder_cmd(ctx, cmd, false);
}

static const struct v4l2_ioctl_ops rockchip_vpu_enc_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = vidioc_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = vidioc_s_fmt,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_expbuf = vidioc_expbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_cropcap = vidioc_cropcap,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_s_crop = vidioc_s_crop,
	.vidioc_try_encoder_cmd = vidioc_try_encoder_cmd,
	.vidioc_encoder_cmd = vidioc_encoder_cmd,
};

static int rockchip_vpu_queue_setup(struct vb2_queue *vq,
				  const void *parg,
				  unsigned int *buf_count,
				  unsigned int *plane_count,
				  unsigned int psize[], void *allocators[])
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(vq->drv_priv);
	int ret = 0;
	int i;

	vpu_debug_enter();

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*plane_count = ctx->vpu_dst_fmt->num_mplanes;

		if (*buf_count < 1)
			*buf_count = 1;

		if (*buf_count > VIDEO_MAX_FRAME)
			*buf_count = VIDEO_MAX_FRAME;

		psize[0] = ctx->dst_fmt.plane_fmt[0].sizeimage;
		/* Kernel mapping necessary for bitstream post processing. */
		allocators[0] = ctx->dev->alloc_ctx_vm;
		vpu_debug(0, "capture psize[%d]: %d\n", 0, psize[0]);
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*plane_count = ctx->vpu_src_fmt->num_mplanes;

		if (*buf_count < 1)
			*buf_count = 1;

		if (*buf_count > VIDEO_MAX_FRAME)
			*buf_count = VIDEO_MAX_FRAME;

		for (i = 0; i < ctx->vpu_src_fmt->num_mplanes; ++i) {
			psize[i] = ctx->src_fmt.plane_fmt[i].sizeimage;
			vpu_debug(0, "output psize[%d]: %d\n", i, psize[i]);
			allocators[i] = ctx->dev->alloc_ctx;
		}
		break;

	default:
		vpu_err("invalid queue type: %d\n", vq->type);
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(vq->drv_priv);
	int ret = 0;
	int i;

	vpu_debug_enter();

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vpu_debug(4, "plane size: %ld, dst size: %d\n",
				vb2_plane_size(vb, 0),
				ctx->dst_fmt.plane_fmt[0].sizeimage);

		if (vb2_plane_size(vb, 0)
		    < ctx->dst_fmt.plane_fmt[0].sizeimage) {
			vpu_err("plane size is too small for capture\n");
			ret = -EINVAL;
		}
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		for (i = 0; i < ctx->vpu_src_fmt->num_mplanes; ++i) {
			vpu_debug(4, "plane %d size: %ld, sizeimage: %u\n", i,
					vb2_plane_size(vb, i),
					ctx->src_fmt.plane_fmt[i].sizeimage);

			if (vb2_plane_size(vb, i)
			    < ctx->src_fmt.plane_fmt[i].sizeimage) {
				vpu_err("size of plane %d is too small for output\n",
					i);
				break;
			}
		}

		if (i != ctx->vpu_src_fmt->num_mplanes)
			ret = -EINVAL;
		break;

	default:
		vpu_err("invalid queue type: %d\n", vq->type);
		ret = -EINVAL;
	}

	vpu_debug_leave();

	return ret;
}

/*
 * FIXME: The operation below should be done in the userspace.
 * This driver is going to be replaced by a proper upstream driver
 * in the future, so we will just keep this as is for the time being.
 */

#define VP8_KEY_FRAME_HDR_SIZE			10
#define VP8_INTER_FRAME_HDR_SIZE		3

#define VP8_FRAME_TAG_KEY_FRAME_BIT		BIT(0)
#define VP8_FRAME_TAG_LENGTH_SHIFT		5
#define VP8_FRAME_TAG_LENGTH_MASK		(0x7ffff << 5)

/*
 * The hardware takes care only of ext hdr and dct partition. The software
 * must take care of frame header.
 *
 * Buffer layout as received from hardware:
 *   |<--gap-->|<--ext hdr-->|<-gap->|<---dct part---
 *   |<-------dct part offset------->|
 *
 * Required buffer layout:
 *   |<--hdr-->|<--ext hdr-->|<---dct part---
 */
static void
rockchip_vpu_vp8e_assemble_bitstream(struct rockchip_vpu_ctx *ctx,
				     struct rockchip_vpu_buf *dst_buf)
{
	struct vb2_v4l2_buffer *vb2_dst = &dst_buf->b;
	size_t ext_hdr_size = dst_buf->vp8e.ext_hdr_size;
	size_t dct_size = dst_buf->vp8e.dct_size;
	size_t hdr_size = dst_buf->vp8e.hdr_size;
	size_t dst_size;
	size_t tag_size;
	void *dst;
	u32 *tag;

	dst_size = vb2_plane_size(&vb2_dst->vb2_buf, 0);
	dst = vb2_plane_vaddr(&vb2_dst->vb2_buf, 0);
	tag = dst; /* To access frame tag words. */

	if (WARN_ON(hdr_size + ext_hdr_size + dct_size > dst_size))
		return;
	if (WARN_ON(dst_buf->vp8e.dct_offset + dct_size > dst_size))
		return;

	vpu_debug(1, "%s: hdr_size = %zu, ext_hdr_size = %zu, dct_size = %zu\n",
			__func__, hdr_size, ext_hdr_size, dct_size);

	memmove(dst + hdr_size + ext_hdr_size,
		dst + dst_buf->vp8e.dct_offset, dct_size);
	memcpy(dst, dst_buf->vp8e.header, hdr_size);

	/* Patch frame tag at first 32-bit word of the frame. */
	if (vb2_dst->flags & V4L2_BUF_FLAG_KEYFRAME) {
		tag_size = VP8_KEY_FRAME_HDR_SIZE;
		tag[0] &= ~VP8_FRAME_TAG_KEY_FRAME_BIT;
	} else {
		tag_size = VP8_INTER_FRAME_HDR_SIZE;
		tag[0] |= VP8_FRAME_TAG_KEY_FRAME_BIT;
	}

	tag[0] &= ~VP8_FRAME_TAG_LENGTH_MASK;
	tag[0] |= (hdr_size + ext_hdr_size - tag_size)
						<< VP8_FRAME_TAG_LENGTH_SHIFT;

	vb2_set_plane_payload(&vb2_dst->vb2_buf, 0,
				hdr_size + ext_hdr_size + dct_size);
}

static void rockchip_vpu_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(vq->drv_priv);
	struct rockchip_vpu_buf *buf = vb_to_buf(vb);
	bool flush_buf;

	vpu_debug_enter();

	/* Zero-size buffer with V4L2_BUF_FLAG_LAST means the flush is done. */
	flush_buf = (buf->b.flags & V4L2_BUF_FLAG_LAST
		     && vb2_get_plane_payload(vb, 0) == 0);
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
	    && vb->state == VB2_BUF_STATE_DONE
	    && ctx->vpu_dst_fmt->fourcc == V4L2_PIX_FMT_VP8
	    && !flush_buf) {
		/*
		 * TODO(akahuang): This function is not only used for RK3388.
		 * Rename it (or maybe move it to the VP8 plugin).
		 */
		rockchip_vpu_vp8e_assemble_bitstream(ctx, buf);
	}
	vpu_debug_leave();
}

static int rockchip_vpu_start_streaming(struct vb2_queue *q, unsigned int count)
{
	int ret = 0;
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(q->drv_priv);
	struct rockchip_vpu_dev *dev = ctx->dev;
	bool ready = false;

	vpu_debug_enter();

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = rockchip_vpu_init(ctx);
		if (ret < 0) {
			vpu_err("rockchip_vpu_init failed\n");
			return ret;
		}

		vb2_clear_last_buffer_dequeued(&ctx->vq_dst);
		ctx->stopped = false;

		ready = vb2_is_streaming(&ctx->vq_dst);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ready = vb2_is_streaming(&ctx->vq_src);
	}

	if (ready)
		rockchip_vpu_try_context(dev, ctx);

	vpu_debug_leave();

	return 0;
}

static void rockchip_vpu_stop_streaming(struct vb2_queue *q)
{
	unsigned long flags;
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(q->drv_priv);
	struct rockchip_vpu_dev *dev = ctx->dev;
	struct rockchip_vpu_buf *b;
	LIST_HEAD(queue);
	int i;

	vpu_debug_enter();

	spin_lock_irqsave(&dev->irqlock, flags);

	list_del_init(&ctx->list);

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		list_splice_init(&ctx->dst_queue, &queue);
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		list_del_init(&ctx->flush_buf.list);
		list_splice_init(&ctx->src_queue, &queue);
		break;

	default:
		break;
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	wait_event(dev->run_wq, dev->current_ctx != ctx);

	while (!list_empty(&queue)) {
		b = list_first_entry(&queue, struct rockchip_vpu_buf, list);
		for (i = 0; i < b->b.vb2_buf.num_planes; i++)
			vb2_set_plane_payload(&b->b.vb2_buf, i, 0);
		vb2_buffer_done(&b->b.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del(&b->list);
	}

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		rockchip_vpu_deinit(ctx);

	vpu_debug_leave();
}

static void rockchip_vpu_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(vq->drv_priv);
	struct rockchip_vpu_dev *dev = ctx->dev;
	struct rockchip_vpu_buf *vpu_buf;
	unsigned long flags;

	vpu_debug_enter();

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vpu_buf = vb_to_buf(vb);

		/* Mark destination as available for use by VPU */
		spin_lock_irqsave(&dev->irqlock, flags);

		list_add_tail(&vpu_buf->list, &ctx->dst_queue);

		spin_unlock_irqrestore(&dev->irqlock, flags);
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vpu_buf = vb_to_buf(vb);

		spin_lock_irqsave(&dev->irqlock, flags);

		list_add_tail(&vpu_buf->list, &ctx->src_queue);

		spin_unlock_irqrestore(&dev->irqlock, flags);
		break;

	default:
		vpu_err("unsupported buffer type (%d)\n", vq->type);
	}

	if (vb2_is_streaming(&ctx->vq_src) && vb2_is_streaming(&ctx->vq_dst))
		rockchip_vpu_try_context(dev, ctx);

	vpu_debug_leave();
}

static struct vb2_ops rockchip_vpu_enc_qops = {
	.queue_setup = rockchip_vpu_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_prepare = rockchip_vpu_buf_prepare,
	.buf_finish = rockchip_vpu_buf_finish,
	.start_streaming = rockchip_vpu_start_streaming,
	.stop_streaming = rockchip_vpu_stop_streaming,
	.buf_queue = rockchip_vpu_buf_queue,
};

struct vb2_ops *rockchip_get_enc_queue_ops(void)
{
	return &rockchip_vpu_enc_qops;
}

const struct v4l2_ioctl_ops *rockchip_get_enc_v4l2_ioctl_ops(void)
{
	return &rockchip_vpu_enc_ioctl_ops;
}

static void rockchip_vpu_enc_prepare_run(struct rockchip_vpu_ctx *ctx)
{
	struct vb2_v4l2_buffer *vb2_src = &ctx->run.src->b;
	unsigned config_store = vb2_src->config_store;

	v4l2_ctrl_apply_store(&ctx->ctrl_handler, config_store);

	if (ctx->vpu_dst_fmt->fourcc == V4L2_PIX_FMT_VP8) {
		memcpy(ctx->run.dst->vp8e.header,
			get_ctrl_ptr(ctx, ROCKCHIP_VPU_ENC_CTRL_HEADER),
			ROCKCHIP_HEADER_SIZE);
		ctx->run.vp8e.reg_params = get_ctrl_ptr(ctx,
			ROCKCHIP_VPU_ENC_CTRL_REG_PARAMS);
		memcpy(ctx->run.priv_src.cpu,
			get_ctrl_ptr(ctx, ROCKCHIP_VPU_ENC_CTRL_HW_PARAMS),
			ROCKCHIP_HW_PARAMS_SIZE);
	} else if (ctx->vpu_dst_fmt->fourcc == V4L2_PIX_FMT_H264) {
		ctx->run.h264e.reg_params = get_ctrl_ptr(ctx,
			ROCKCHIP_VPU_ENC_CTRL_REG_PARAMS);
	} else if (ctx->vpu_dst_fmt->fourcc == V4L2_PIX_FMT_JPEG_RAW) {
		memcpy(ctx->run.jpege.lumin_quant_tbl,
			get_ctrl_ptr(ctx, ROCKCHIP_VPU_ENC_CTRL_Y_QUANT_TBL),
			ROCKCHIP_JPEG_QUANT_ELE_SIZE);
		memcpy(ctx->run.jpege.chroma_quant_tbl,
			get_ctrl_ptr(ctx, ROCKCHIP_VPU_ENC_CTRL_C_QUANT_TBL),
			ROCKCHIP_JPEG_QUANT_ELE_SIZE);
	}
}

static const struct rockchip_vpu_run_ops rockchip_vpu_enc_run_ops = {
	.prepare_run = rockchip_vpu_enc_prepare_run,
};

int rockchip_vpu_enc_init(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	int ret;

	ctx->vpu_dst_fmt = get_def_fmt(vpu, true);
	reset_src_fmt(ctx);

	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->run.priv_src,
					ROCKCHIP_HW_PARAMS_SIZE);
	if (ret) {
		vpu_err("Failed to allocate private source buffer.\n");
		return ret;
	}


	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->run.priv_dst,
					ROCKCHIP_RET_PARAMS_SIZE);
	if (ret) {
		vpu_err("Failed to allocate private destination buffer.\n");
		goto err_priv_src;
	}

	ret = rockchip_vpu_ctrls_setup(ctx, &rockchip_vpu_enc_ctrl_ops,
					controls, ARRAY_SIZE(controls),
					rockchip_vpu_enc_get_menu);
	if (ret) {
		vpu_err("Failed to set up controls\n");
		goto err_priv_dst;
	}

	ctx->run_ops = &rockchip_vpu_enc_run_ops;

	return 0;

err_priv_dst:
	rockchip_vpu_aux_buf_free(vpu, &ctx->run.priv_dst);
err_priv_src:
	rockchip_vpu_aux_buf_free(vpu, &ctx->run.priv_src);

	return ret;
}

void rockchip_vpu_enc_exit(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	rockchip_vpu_ctrls_delete(ctx);

	rockchip_vpu_aux_buf_free(vpu, &ctx->run.priv_dst);
	rockchip_vpu_aux_buf_free(vpu, &ctx->run.priv_src);
};
