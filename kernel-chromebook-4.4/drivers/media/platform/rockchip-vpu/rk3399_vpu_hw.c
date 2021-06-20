/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2016 Rockchip Electronics Co., Ltd.
 *	Jeffy Chen <jeffy.chen@rock-chips.com>
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

#include "rk3399_vpu_regs.h"

/*
 * Supported formats.
 */

static const struct rockchip_vpu_fmt rk3399_vpu_enc_fmts[] = {
	/* Source formats. */
	{
		.name = "4:2:0 3 planes Y/Cb/Cr",
		.fourcc = V4L2_PIX_FMT_YUV420M,
		.codec_mode = RK_VPU_CODEC_NONE,
		.num_mplanes = 3,
		.num_cplanes = 3,
		.depth = { 8, 8, 8 },
		.h_subsampling = { 1, 2, 2 },
		.v_subsampling = { 1, 2, 2 },
		.enc_fmt = RK3288_VPU_ENC_FMT_YUV420P,
	},
	{
		.name = "4:2:0 2 plane Y/CbCr",
		.fourcc = V4L2_PIX_FMT_NV12M,
		.codec_mode = RK_VPU_CODEC_NONE,
		.num_mplanes = 2,
		.num_cplanes = 2,
		.depth = { 8, 16 },
		.h_subsampling = { 1, 2 },
		.v_subsampling = { 1, 2 },
		.enc_fmt = RK3288_VPU_ENC_FMT_YUV420SP,
	},
	{
		.name = "4:2:2 1 plane YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
		.codec_mode = RK_VPU_CODEC_NONE,
		.num_mplanes = 1,
		.num_cplanes = 1,
		.depth = { 16 },
		.h_subsampling = { 1 },
		.v_subsampling = { 1 },
		.enc_fmt = RK3288_VPU_ENC_FMT_YUYV422,
	},
	{
		.name = "4:2:2 1 plane UYVY",
		.fourcc = V4L2_PIX_FMT_UYVY,
		.codec_mode = RK_VPU_CODEC_NONE,
		.num_mplanes = 1,
		.num_cplanes = 1,
		.depth = { 16 },
		.h_subsampling = { 1 },
		.v_subsampling = { 1 },
		.enc_fmt = RK3288_VPU_ENC_FMT_UYVY422,
	},
	/* Destination formats. */
	{
		.name = "VP8 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_VP8,
		.codec_mode = RK_VPU_CODEC_VP8E,
		.num_mplanes = 1,
		.frmsize = {
			.min_width = 96,
			.max_width = 1920,
			.step_width = MB_DIM,
			.min_height = 96,
			.max_height = 1088,
			.step_height = MB_DIM,
		},
	},
	{
		.name = "H264 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_H264,
		.codec_mode = RK_VPU_CODEC_H264E,
		.num_mplanes = 1,
		.frmsize = {
			.min_width = 96,
			.max_width = 1920,
			.step_width = MB_DIM,
			.min_height = 96,
			.max_height = 1088,
			.step_height = MB_DIM,
		},
	},
	{
		.name = "JPEG Encoded Stream",
		.fourcc = V4L2_PIX_FMT_JPEG_RAW,
		.codec_mode = RK_VPU_CODEC_JPEGE,
		.num_mplanes = 1,
		.frmsize = {
			.min_width = 96,
			.max_width = 8192,
			.step_width = MB_DIM,
			.min_height = 32,
			.max_height = 8192,
			.step_height = MB_DIM,
		},
	},
};

static const struct rockchip_vpu_fmt rk3399_vpu_dec_fmts[] = {
	{
		.name = "4:2:0 1 plane Y/CbCr",
		.fourcc = V4L2_PIX_FMT_NV12,
		.codec_mode = RK_VPU_CODEC_NONE,
		.num_mplanes = 1,
		.num_cplanes = 2,
		.depth = { 8, 16 },
		.h_subsampling = { 1, 2 },
		.v_subsampling = { 1, 2 },
	},
	{
		.name = "Frames of VP8 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_VP8_FRAME,
		.codec_mode = RK_VPU_CODEC_VP8D,
		.num_mplanes = 1,
		.frmsize = {
			.min_width = 48,
			.max_width = 1920,
			.step_width = MB_DIM,
			.min_height = 48,
			.max_height = 1088,
			.step_height = MB_DIM,
		},
	},
};

/*
 * Interrupt handlers.
 */

static irqreturn_t rk3399_vepu_irq(int irq, void *dev_id)
{
	struct rockchip_vpu_dev *vpu = dev_id;
	u32 status = vepu_read(vpu, VEPU_REG_INTERRUPT);

	vepu_write(vpu, 0, VEPU_REG_INTERRUPT);

	if (status & VEPU_REG_INTERRUPT_BIT) {
		vepu_write(vpu, 0, VEPU_REG_AXI_CTRL);

		rockchip_vpu_irq_done(vpu);
	}

	return IRQ_HANDLED;
}

static irqreturn_t rk3399_vdpu_irq(int irq, void *dev_id)
{
	struct rockchip_vpu_dev *vpu = dev_id;
	u32 status = vdpu_read(vpu, VDPU_REG_INTERRUPT);

	vdpu_write(vpu, 0, VDPU_REG_INTERRUPT);

	vpu_debug(3, "vdpu_irq status: %08x\n", status);

	if (status & VDPU_REG_INTERRUPT_DEC_IRQ) {
		vdpu_write(vpu, 0, VDPU_REG_AXI_CTRL);

		rockchip_vpu_irq_done(vpu);
	}

	return IRQ_HANDLED;
}

/*
 * Initialization/clean-up.
 */

static int rk3399_vpu_hw_probe(struct rockchip_vpu_dev *vpu)
{
	struct resource *res;
	int irq_enc, irq_dec;
	int ret;

	vpu->aclk = devm_clk_get(vpu->dev, "aclk_vcodec");
	if (IS_ERR(vpu->aclk)) {
		dev_err(vpu->dev, "failed to get aclk\n");
		return PTR_ERR(vpu->aclk);
	}

	vpu->hclk = devm_clk_get(vpu->dev, "hclk_vcodec");
	if (IS_ERR(vpu->hclk)) {
		dev_err(vpu->dev, "failed to get hclk\n");
		return PTR_ERR(vpu->hclk);
	}

	/*
	 * Bump ACLK to max. possible freq. (400 MHz) to improve performance.
	 *
	 * VP8 encoding 1280x720@1.2Mbps 200 MHz: 39 fps, 400: MHz 77 fps
	 */
	clk_set_rate(vpu->aclk, 400 * 1000 * 1000);

	res = platform_get_resource(vpu->pdev, IORESOURCE_MEM, 0);
	vpu->base = devm_ioremap_resource(vpu->dev, res);
	if (IS_ERR(vpu->base))
		return PTR_ERR(vpu->base);

	vpu->enc_base = vpu->base + vpu->variant->enc_offset;
	vpu->dec_base = vpu->base + vpu->variant->dec_offset;

	ret = dma_set_coherent_mask(vpu->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(vpu->dev, "could not set DMA coherent mask\n");
		return ret;
	}

	irq_enc = platform_get_irq_byname(vpu->pdev, "vepu");
	if (irq_enc <= 0) {
		dev_err(vpu->dev, "could not get vepu IRQ\n");
		return -ENXIO;
	}

	ret = devm_request_threaded_irq(vpu->dev, irq_enc, NULL,
					rk3399_vepu_irq, IRQF_ONESHOT,
					dev_name(vpu->dev), vpu);
	if (ret) {
		dev_err(vpu->dev, "could not request vepu IRQ\n");
		return ret;
	}

	irq_dec = platform_get_irq_byname(vpu->pdev, "vdpu");
	if (irq_dec <= 0) {
		dev_err(vpu->dev, "could not get vdpu IRQ\n");
		return -ENXIO;
	}

	ret = devm_request_threaded_irq(vpu->dev, irq_dec, NULL,
					rk3399_vdpu_irq, IRQF_ONESHOT,
					dev_name(vpu->dev), vpu);
	if (ret) {
		dev_err(vpu->dev, "could not request vdpu IRQ\n");
		return ret;
	}

	return 0;
}

static void rk3399_vpu_clk_enable(struct rockchip_vpu_dev *vpu)
{
	clk_prepare_enable(vpu->aclk);
	clk_prepare_enable(vpu->hclk);
}

static void rk3399_vpu_clk_disable(struct rockchip_vpu_dev *vpu)
{
	clk_disable_unprepare(vpu->hclk);
	clk_disable_unprepare(vpu->aclk);
}

static void rk3399_vpu_enc_reset(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	vepu_write(vpu, VEPU_REG_INTERRUPT_DIS_BIT, VEPU_REG_INTERRUPT);
	vepu_write(vpu, 0, VEPU_REG_ENCODE_START);
	vepu_write(vpu, 0, VEPU_REG_AXI_CTRL);
}

static void rk3399_vpu_dec_reset(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	vdpu_write(vpu, VDPU_REG_INTERRUPT_DEC_IRQ_DIS, VDPU_REG_INTERRUPT);
	vdpu_write(vpu, 0, VDPU_REG_EN_FLAGS);
	vdpu_write(vpu, 1, VDPU_REG_SOFT_RESET);
}

/*
 * Supported codec ops.
 */

static const struct rockchip_vpu_codec_ops rk3399_vpu_mode_ops[] = {
	[RK_VPU_CODEC_VP8E] = {
		.init = rk3399_vpu_vp8e_init,
		.exit = rk3399_vpu_vp8e_exit,
		.run = rk3399_vpu_vp8e_run,
		.done = rk3399_vpu_vp8e_done,
		.reset = rk3399_vpu_enc_reset,
},
	[RK_VPU_CODEC_VP8D] = {
		.init = rk3399_vpu_vp8d_init,
		.exit = rk3399_vpu_vp8d_exit,
		.run = rk3399_vpu_vp8d_run,
		.done = rockchip_vpu_run_done,
		.reset = rk3399_vpu_dec_reset,
	},
	[RK_VPU_CODEC_H264E] = {
		.init = rk3399_vpu_h264e_init,
		.exit = rk3399_vpu_h264e_exit,
		.run = rk3399_vpu_h264e_run,
		.done = rk3399_vpu_h264e_done,
		.reset = rk3399_vpu_enc_reset,
	},
	[RK_VPU_CODEC_JPEGE] = {
		.run = rk3399_vpu_jpege_run,
		.done = rk3399_vpu_jpege_done,
		.reset = rk3399_vpu_enc_reset,
	},
};

/*
 * VPU variant.
 */

const struct rockchip_vpu_variant rk3399_vpu_variant = {
	.enc_offset = 0x0,
	.dec_offset = 0x400,
	.enc_fmts = rk3399_vpu_enc_fmts,
	.num_enc_fmts = ARRAY_SIZE(rk3399_vpu_enc_fmts),
	.dec_fmts = rk3399_vpu_dec_fmts,
	.num_dec_fmts = ARRAY_SIZE(rk3399_vpu_dec_fmts),
	.mode_ops = rk3399_vpu_mode_ops,
	.hw_probe = rk3399_vpu_hw_probe,
	.clk_enable = rk3399_vpu_clk_enable,
	.clk_disable = rk3399_vpu_clk_disable,
};
