// SPDX-License-Identifier: GPL-2.0+
/*
 * Rockchip rk3399 VPU codec driver
 */

#include <asm/unaligned.h>

#include "rk3399_vpu_regs.h"
#include "rockchip_vpu_common.h"
#include "rockchip_vpu_hw.h"

static void set_src_img_ctrl(struct rockchip_vpu_dev *vpu,
			     struct rockchip_vpu_ctx *ctx)
{
	struct v4l2_pix_format_mplane *pix_fmt = &ctx->src_fmt;
	struct v4l2_rect *crop = &ctx->src_crop;
	u32 overfill_r, overfill_b;
	u32 reg;

	vpu_debug_enter();

	/* The pix fmt width/height are MB round up aligned
	 * by .vidioc_s_fmt_vid_cap_mplane() callback
	 */
	overfill_r = pix_fmt->width - crop->width;
	overfill_b = pix_fmt->height - crop->height;

	reg = VEPU_REG_IN_IMG_CTRL_ROW_LEN(pix_fmt->width);
	vepu_write_relaxed(vpu, reg, VEPU_REG_INPUT_LUMA_INFO);

	reg = VEPU_REG_IN_IMG_CTRL_OVRFLR_D4(overfill_r / 4) |
	      VEPU_REG_IN_IMG_CTRL_OVRFLB(overfill_b);
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENC_OVER_FILL_STRM_OFFSET);

	reg = VEPU_REG_IN_IMG_CTRL_FMT(ctx->vpu_src_fmt->enc_fmt);
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENC_CTRL1);

	vpu_debug_leave();
}

static void rk3399_vpu_jpege_set_buffers(struct rockchip_vpu_dev *vpu,
					 struct rockchip_vpu_ctx *ctx)
{
	struct v4l2_pix_format_mplane *pix_fmt = &ctx->src_fmt;
	struct vb2_buffer *buf;
	dma_addr_t dst_dma, src_dma[3];
	u32 dst_size;
	int i;

	vpu_debug_enter();

	buf = &ctx->run.dst->b.vb2_buf;
	dst_dma = vb2_dma_contig_plane_dma_addr(buf, 0);
	dst_size = vb2_plane_size(buf, 0);

	vepu_write_relaxed(vpu, dst_dma, VEPU_REG_ADDR_OUTPUT_STREAM);
	vepu_write_relaxed(vpu, dst_size, VEPU_REG_STR_BUF_LIMIT);

	buf = &ctx->run.src->b.vb2_buf;
	for (i = 0; i < ARRAY_SIZE(src_dma); i++) {
		if (i < pix_fmt->num_planes)
			src_dma[i] = vb2_dma_contig_plane_dma_addr(buf, i) +
				ctx->run.src->b.vb2_buf.planes[i].data_offset;
		else
			src_dma[i] = src_dma[i-1];
	}
	vepu_write_relaxed(vpu, src_dma[0], VEPU_REG_ADDR_IN_LUMA);
	vepu_write_relaxed(vpu, src_dma[1], VEPU_REG_ADDR_IN_CB);
	vepu_write_relaxed(vpu, src_dma[2], VEPU_REG_ADDR_IN_CR);

	vpu_debug_leave();
}

static void rk3399_vpu_jpege_set_params(struct rockchip_vpu_dev *vpu,
					struct rockchip_vpu_ctx *ctx)
{
	u32 reg, i;
	u8 *qtable;

	vpu_debug_enter();

	set_src_img_ctrl(vpu, ctx);

	for (i = 0; i < 16; i++) {
		qtable = ctx->run.jpege.lumin_quant_tbl;
		reg = get_unaligned_be32(&qtable[i * 4]);
		vepu_write_relaxed(vpu, reg, VEPU_REG_JPEG_LUMA_QUAT(i));

		qtable = ctx->run.jpege.chroma_quant_tbl;
		reg = get_unaligned_be32(&qtable[i * 4]);

		vepu_write_relaxed(vpu, reg, VEPU_REG_JPEG_CHROMA_QUAT(i));
	}

	vpu_debug_leave();
}

void rk3399_vpu_jpege_run(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	u32 reg;

	vpu_debug_enter();

	rockchip_vpu_power_on(vpu);

	/* Switch to JPEG encoder mode before writing regsiters */
	reg = VEPU_REG_ENCODE_FORMAT(2);
	vepu_write(vpu, reg, VEPU_REG_ENCODE_START);

	rk3399_vpu_jpege_set_params(vpu, ctx);
	rk3399_vpu_jpege_set_buffers(vpu, ctx);

	/* Make sure that all registers are written at this point. */
	wmb();

	/* Start the hardware. */
	reg = VEPU_REG_OUTPUT_SWAP32
		| VEPU_REG_OUTPUT_SWAP16
		| VEPU_REG_OUTPUT_SWAP8
		| VEPU_REG_INPUT_SWAP8
		| VEPU_REG_INPUT_SWAP16
		| VEPU_REG_INPUT_SWAP32;
	vepu_write(vpu, reg, VEPU_REG_DATA_ENDIAN);

	reg = VEPU_REG_AXI_CTRL_BURST_LEN(16);
	vepu_write(vpu, reg, VEPU_REG_AXI_CTRL);

	reg = VEPU_REG_CLK_GATING_EN;
	vepu_write(vpu, reg, VEPU_REG_INTERRUPT);

	reg = VEPU_REG_MB_WIDTH(MB_WIDTH(ctx->src_fmt.width))
		| VEPU_REG_MB_HEIGHT(MB_HEIGHT(ctx->src_fmt.height))
		| VEPU_REG_PIC_TYPE(1)
		| VEPU_REG_ENCODE_FORMAT(2)
		| VEPU_REG_ENCODE_ENABLE;

	/* Set the watchdog. */
	schedule_delayed_work(&vpu->watchdog_work, msecs_to_jiffies(2000));

	vepu_write(vpu, reg, VEPU_REG_ENCODE_START);
}

void rk3399_vpu_jpege_done(struct rockchip_vpu_ctx *ctx,
			   enum vb2_buffer_state result)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	struct vb2_v4l2_buffer *vb2_dst = &ctx->run.dst->b;
	size_t encoded_size;

	vpu_debug_enter();

	encoded_size = vepu_read(vpu, VEPU_REG_STR_BUF_LIMIT) / 8;
	vb2_set_plane_payload(&vb2_dst->vb2_buf, 0, encoded_size);
	rockchip_vpu_run_done(ctx, result);

	vpu_debug_leave();
}
