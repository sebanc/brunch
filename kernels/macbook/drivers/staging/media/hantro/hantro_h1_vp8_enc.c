// SPDX-License-Identifier: GPL-2.0
/*
 * Hantro VP8 encoder.
 *
 * Copyright 2019 Google LLC
 */

#include "hantro.h"
#include "hantro_h1_regs.h"
#include "hantro_v4l2.h"

/* Various parameters specific to VP8 encoder. */
#define VP8_CABAC_CTX_OFFSET			192
#define VP8_CABAC_CTX_SIZE			((55 + 96) << 3)

void hantro_vp8_enc_done(struct hantro_ctx *ctx)
{
	struct hantro_vp8_enc_ctrl_buf *ctrl_buf = ctx->vp8_enc.ctrl_buf.cpu;
	struct vb2_v4l2_buffer *dst_buf = hantro_get_dst_buf(ctx);

	/* Read length information of this run from utility buffer. */
	ctx->vp8_enc.buf_data.ext_hdr_size = ctrl_buf->ext_hdr_size;
	ctx->vp8_enc.buf_data.dct_size = ctrl_buf->dct_size;

	hantro_vp8_enc_assemble_bitstream(ctx, &dst_buf->vb2_buf);
}

static void hantro_h1_vp8_enc_set_params(struct hantro_dev *vpu,
					 struct hantro_ctx *ctx)
{
	struct hantro_h1_vp8_enc_reg_params *params;
	int i;

	params = hantro_get_ctrl(ctx, V4L2_CID_PRIVATE_HANTRO_REG_PARAMS);

	vepu_write_relaxed(vpu, params->enc_ctrl0, H1_REG_ENC_CTRL0);
	vepu_write_relaxed(vpu, params->enc_ctrl1, H1_REG_ENC_CTRL1);
	vepu_write_relaxed(vpu, params->enc_ctrl2, H1_REG_ENC_CTRL2);
	vepu_write_relaxed(vpu, params->enc_ctrl3, H1_REG_ENC_CTRL3);
	/*
	 * Controls registers 4 and 5 are purposely inverted because register
	 * numbers in upstream and parameter in structure are inverted.
	 */
	vepu_write_relaxed(vpu, params->enc_ctrl5, H1_REG_ENC_CTRL4);
	vepu_write_relaxed(vpu, params->enc_ctrl4, H1_REG_ENC_CTRL5);
	vepu_write_relaxed(vpu, params->str_hdr_rem_msb,
			   H1_REG_STR_HDR_REM_MSB);
	vepu_write_relaxed(vpu, params->str_hdr_rem_lsb,
			   H1_REG_STR_HDR_REM_LSB);
	vepu_write_relaxed(vpu, params->mad_ctrl, H1_REG_MAD_CTRL);

	for (i = 0; i < ARRAY_SIZE(params->qp_val); ++i)
		vepu_write_relaxed(vpu, params->qp_val[i],
				   H1_REG_VP8_QP_VAL(i));

	vepu_write_relaxed(vpu, params->bool_enc, H1_REG_VP8_BOOL_ENC);
	vepu_write_relaxed(vpu, params->vp8_ctrl0, H1_REG_VP8_CTRL0);
	vepu_write_relaxed(vpu, params->rlc_ctrl, H1_REG_RLC_CTRL);
	vepu_write_relaxed(vpu, params->mb_ctrl, H1_REG_MB_CTRL);

	for (i = 0; i < ARRAY_SIZE(params->rgb_yuv_coeff); ++i)
		vepu_write_relaxed(vpu, params->rgb_yuv_coeff[i],
				   H1_REG_RGB_YUV_COEFF(i));

	vepu_write_relaxed(vpu, params->rgb_mask_msb, H1_REG_RGB_MASK_MSB);
	vepu_write_relaxed(vpu, params->intra_area_ctrl,
			   H1_REG_INTRA_AREA_CTRL);
	vepu_write_relaxed(vpu, params->cir_intra_ctrl,
			   H1_REG_CIR_INTRA_CTRL);
	vepu_write_relaxed(vpu, params->first_roi_area,
			   H1_REG_FIRST_ROI_AREA);
	vepu_write_relaxed(vpu, params->second_roi_area,
			   H1_REG_SECOND_ROI_AREA);
	vepu_write_relaxed(vpu, params->mvc_ctrl, H1_REG_MVC_CTRL);

	for (i = 0; i < ARRAY_SIZE(params->intra_penalty); ++i)
		vepu_write_relaxed(vpu, params->intra_penalty[i],
				   H1_REG_VP8_INTRA_PENALTY(i));

	for (i = 0; i < ARRAY_SIZE(params->seg_qp); ++i)
		vepu_write_relaxed(vpu, params->seg_qp[i],
				   H1_REG_VP8_SEG_QP(i));

	for (i = 0; i < ARRAY_SIZE(params->dmv_4p_1p_penalty); ++i)
		vepu_write_relaxed(vpu, params->dmv_4p_1p_penalty[i],
				   H1_REG_DMV_4P_1P_PENALTY(i));

	for (i = 0; i < ARRAY_SIZE(params->dmv_qpel_penalty); ++i)
		vepu_write_relaxed(vpu, params->dmv_qpel_penalty[i],
				   H1_REG_DMV_QPEL_PENALTY(i));

	vepu_write_relaxed(vpu, params->vp8_ctrl1, H1_REG_VP8_CTRL1);
	vepu_write_relaxed(vpu, params->bit_cost_golden,
			   H1_REG_VP8_BIT_COST_GOLDEN);

	for (i = 0; i < ARRAY_SIZE(params->loop_flt_delta); ++i)
		vepu_write_relaxed(vpu, params->loop_flt_delta[i],
				   H1_REG_VP8_LOOP_FLT_DELTA(i));
}

static inline u32 enc_in_img_ctrl(struct hantro_ctx *ctx)
{
	unsigned int bytes_per_line, overfill_r, overfill_b;

	/*
	 * The hardware needs only the value for luma plane, because
	 * values of other planes are calculated internally based on
	 * format setting.
	 */
	bytes_per_line = ctx->src_fmt.plane_fmt[0].bytesperline;
	overfill_r = ctx->src_fmt.width - ctx->dst_fmt.width;
	overfill_b = ctx->src_fmt.height - ctx->dst_fmt.height;

	return H1_REG_IN_IMG_CTRL_ROW_LEN(bytes_per_line)
			| H1_REG_IN_IMG_CTRL_OVRFLR_D4(overfill_r / 4)
			| H1_REG_IN_IMG_CTRL_OVRFLB(overfill_b)
			| H1_REG_IN_IMG_CTRL_FMT(ctx->vpu_src_fmt->enc_fmt);
}

static void hantro_h1_vp8_enc_set_buffers(struct hantro_dev *vpu,
					  struct hantro_ctx *ctx)
{
	const u32 src_addr_regs[] = { H1_REG_ADDR_IN_PLANE_0,
				      H1_REG_ADDR_IN_PLANE_1,
				      H1_REG_ADDR_IN_PLANE_2 };
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct hantro_h1_vp8_enc_reg_params *params;
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;
	dma_addr_t ref_buf_dma, rec_buf_dma;
	dma_addr_t stream_dma;
	size_t rounded_size;
	dma_addr_t dst_dma;
	u32 start_offset;
	size_t dst_size;
	int i;

	src_buf = hantro_get_src_buf(ctx);
	dst_buf = hantro_get_dst_buf(ctx);

	rounded_size = hantro_rounded_luma_size(ctx->src_fmt.width,
						ctx->src_fmt.height);

	params = hantro_get_ctrl(ctx,
				 V4L2_CID_PRIVATE_HANTRO_REG_PARAMS);

	ref_buf_dma = ctx->vp8_enc.ext_buf.dma;
	rec_buf_dma = ref_buf_dma;
	if (ctx->vp8_enc.ref_rec_ptr)
		ref_buf_dma += rounded_size * 3 / 2;
	else
		rec_buf_dma += rounded_size * 3 / 2;
	ctx->vp8_enc.ref_rec_ptr ^= 1;

	dst_dma = vb2_dma_contig_plane_dma_addr(&dst_buf->vb2_buf, 0);
	dst_size = vb2_plane_size(&dst_buf->vb2_buf, 0);

	/*
	 * stream addr-->|
	 * align 64bits->|<-start offset->|
	 * |<---------header size-------->|<---dst buf---
	 */
	start_offset = (params->rlc_ctrl & H1_REG_RLC_CTRL_STR_OFFS_MASK)
					>> H1_REG_RLC_CTRL_STR_OFFS_SHIFT;
	stream_dma = dst_dma + params->hdr_len;

	/**
	 * Userspace will pass 8 bytes aligned size(round_down) to us,
	 * so we need to plus start offset to get real header size.
	 *
	 * |<-aligned size->|<-start offset->|
	 * |<----------header size---------->|
	 */
	ctx->vp8_enc.buf_data.hdr_size = params->hdr_len + (start_offset >> 3);

	if (params->enc_ctrl & H1_REG_ENC_PIC_INTRA)
		dst_buf->flags |= V4L2_BUF_FLAG_KEYFRAME;
	else
		dst_buf->flags &= ~V4L2_BUF_FLAG_KEYFRAME;

	/*
	 * We assume here that 1/10 of the buffer is enough for headers.
	 * DCT partition will be placed in remaining 9/10 of the buffer.
	 */
	ctx->vp8_enc.buf_data.dct_offset = round_up(dst_size / 10, 8);

	/* Destination buffer. */
	vepu_write_relaxed(vpu, stream_dma, H1_REG_ADDR_OUTPUT_STREAM);
	vepu_write_relaxed(vpu, dst_dma + ctx->vp8_enc.buf_data.dct_offset,
			   H1_REG_ADDR_VP8_DCT_PART(0));
	vepu_write_relaxed(vpu, dst_size - ctx->vp8_enc.buf_data.dct_offset,
			   H1_REG_STR_BUF_LIMIT);

	/* Auxiliary buffers. */
	vepu_write_relaxed(vpu, ctx->vp8_enc.ctrl_buf.dma,
			   H1_REG_ADDR_OUTPUT_CTRL);
	vepu_write_relaxed(vpu, ctx->vp8_enc.mv_buf.dma, H1_REG_ADDR_MV_OUT);
	vepu_write_relaxed(vpu, ctx->vp8_enc.priv_dst.dma,
			   H1_REG_ADDR_VP8_PROB_CNT);
	vepu_write_relaxed(vpu, ctx->vp8_enc.priv_src.dma +
			   VP8_CABAC_CTX_OFFSET,
			   H1_REG_ADDR_CABAC_TBL);
	vepu_write_relaxed(vpu, ctx->vp8_enc.priv_src.dma
			+ VP8_CABAC_CTX_OFFSET + VP8_CABAC_CTX_SIZE,
		H1_REG_ADDR_VP8_SEG_MAP);

	/* Reference buffers. */
	vepu_write_relaxed(vpu, ref_buf_dma, H1_REG_ADDR_REF_LUMA);
	vepu_write_relaxed(vpu, ref_buf_dma + rounded_size,
			   H1_REG_ADDR_REF_CHROMA);

	/* Reconstruction buffers. */
	vepu_write_relaxed(vpu, rec_buf_dma, H1_REG_ADDR_REC_LUMA);
	vepu_write_relaxed(vpu, rec_buf_dma + rounded_size,
			   H1_REG_ADDR_REC_CHROMA);

	/* Source buffer. */
	/*
	 * TODO(crbug.com/901264): The way to pass an offset within a
	 * DMA-buf is not defined in V4L2 specification, so we abuse
	 * data_offset for now. Fix it when we have the right interface,
	 * including any necessary validation and potential alignment
	 * issues.
	 */
	for (i = 0; i < src_fmt->num_planes; ++i)
		vepu_write_relaxed(vpu,
				   vb2_dma_contig_plane_dma_addr(
					&src_buf->vb2_buf, i) +
				   src_buf->vb2_buf.planes[i].data_offset,
				   src_addr_regs[i]);

	/* Source parameters. */
	vepu_write_relaxed(vpu, enc_in_img_ctrl(ctx), H1_REG_IN_IMG_CTRL);
}

int hantro_h1_vp8_enc_run(struct hantro_ctx *ctx)
{
	struct hantro_dev *vpu = ctx->dev;
	struct vb2_v4l2_buffer *dst_buf;
	u32 reg;

	hantro_start_prepare_run(ctx);

	/* prepare to run */
	memcpy(ctx->vp8_enc.buf_data.header,
	       hantro_get_ctrl(ctx, V4L2_CID_PRIVATE_HANTRO_HEADER),
	       HANTRO_VP8_HEADER_SIZE);
	memcpy(ctx->vp8_enc.priv_src.cpu,
	       hantro_get_ctrl(ctx, V4L2_CID_PRIVATE_HANTRO_HW_PARAMS),
	       HANTRO_VP8_HW_PARAMS_SIZE);

	/* The hardware expects the control buffer to be zeroed. */
	memset(ctx->vp8_enc.ctrl_buf.cpu, 0,
	       sizeof(struct hantro_vp8_enc_ctrl_buf));

	vepu_write_relaxed(vpu,
			   H1_REG_ENC_CTRL_ENC_MODE_VP8, H1_REG_ENC_CTRL);

	hantro_h1_vp8_enc_set_params(vpu, ctx);
	hantro_h1_vp8_enc_set_buffers(vpu, ctx);

	/* Make sure that all registers are written at this point. */
	wmb();

	/* Start the hardware. */
	reg = H1_REG_AXI_CTRL_OUTPUT_SWAP16
		| H1_REG_AXI_CTRL_INPUT_SWAP16
		| H1_REG_AXI_CTRL_BURST_LEN(16)
		| H1_REG_AXI_CTRL_GATE_BIT
		| H1_REG_AXI_CTRL_OUTPUT_SWAP32
		| H1_REG_AXI_CTRL_INPUT_SWAP32
		| H1_REG_AXI_CTRL_OUTPUT_SWAP8
		| H1_REG_AXI_CTRL_INPUT_SWAP8;
	vepu_write(vpu, reg, H1_REG_AXI_CTRL);

	vepu_write(vpu, 0, H1_REG_INTERRUPT);

	reg = H1_REG_ENC_CTRL_NAL_MODE_BIT
		| H1_REG_ENC_CTRL_WIDTH(MB_WIDTH(ctx->src_fmt.width))
		| H1_REG_ENC_CTRL_HEIGHT(MB_HEIGHT(ctx->src_fmt.height))
		| H1_REG_ENC_CTRL_ENC_MODE_VP8
		| H1_REG_ENC_CTRL_EN_BIT;

	dst_buf = hantro_get_dst_buf(ctx);

	if (dst_buf->flags & V4L2_BUF_FLAG_KEYFRAME)
		reg |= H1_REG_ENC_PIC_INTRA;

	hantro_end_prepare_run(ctx);

	vepu_write(vpu, reg, H1_REG_ENC_CTRL);

	return 0;
}
