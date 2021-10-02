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

#include "rk3399_vdec_regs.h"

/*
 * Supported formats.
 */

static const struct rockchip_vpu_fmt rk3399_vdec_fmts[] = {
	{
		.name = "4:2:0 1 plane Y/CbCr",
		.fourcc = V4L2_PIX_FMT_NV12,
		.codec_mode = RK_VPU_CODEC_NONE,
		.num_mplanes = 1,
		.num_cplanes = 2,
		.depth = { 8, 16 },
		.v_subsampling = { 1, 2 },
		.h_subsampling = { 1, 2 },
	},
	{
		.name = "Slices of H264 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_H264_SLICE,
		.codec_mode = RK_VPU_CODEC_H264D,
		.num_mplanes = 1,
		.frmsize = {
			.min_width = 48,
			.max_width = 3840,
			.step_width = MB_DIM,
			.min_height = 48,
			.max_height = 2160,
			.step_height = MB_DIM,
		},
	},
	{
		.name = "Frames of VP9 Encoded Stream",
		.fourcc = V4L2_PIX_FMT_VP9_FRAME,
		.codec_mode = RK_VPU_CODEC_VP9D,
		.num_mplanes = 1,
		.frmsize = {
			.min_width = 48,
			.max_width = 3840,
			.step_width = SB_DIM,
			.min_height = 48,
			.max_height = 2176,
			.step_height = SB_DIM,
		},
	},
};

/*
 * Interrupt handlers.
 */

static irqreturn_t rk3399_vdec_irq(int irq, void *dev_id)
{
	struct rockchip_vpu_dev *vpu = dev_id;
	u32 status = vdpu_read(vpu, RKVDEC_REG_INTERRUPT);

	vpu_debug(5, "dec status %x\n", status);

	vdpu_write(vpu, 0, RKVDEC_REG_INTERRUPT);

	if (status & RKVDEC_IRQ)
		rockchip_vpu_irq_done(vpu);

	return IRQ_HANDLED;
}

/*
 * Initialization/clean-up.
 */

static int rk3399_vdec_hw_probe(struct rockchip_vpu_dev *vpu)
{
	struct resource *res;
	int irq_dec;
	int ret;

	vpu->aclk = devm_clk_get(vpu->dev, "aclk_vdu");
	if (IS_ERR(vpu->aclk)) {
		dev_err(vpu->dev, "failed to get aclk\n");
		return PTR_ERR(vpu->aclk);
	}

	vpu->hclk = devm_clk_get(vpu->dev, "hclk_vdu");
	if (IS_ERR(vpu->hclk)) {
		dev_err(vpu->dev, "failed to get hclk\n");
		return PTR_ERR(vpu->hclk);
	}

	vpu->sclk_cabac = devm_clk_get(vpu->dev, "sclk_cabac");
	if (IS_ERR(vpu->sclk_cabac)) {
		dev_err(vpu->dev, "failed to get sclk_cabac\n");
		return PTR_ERR(vpu->sclk_cabac);
	}

	vpu->sclk_core = devm_clk_get(vpu->dev, "sclk_core");
	if (IS_ERR(vpu->sclk_core)) {
		dev_err(vpu->dev, "failed to get sclk_core\n");
		return PTR_ERR(vpu->sclk_core);
	}

	/*
	 * Bump ACLK to max. possible freq. (500 MHz) to improve performance
	 * When 4k video playback.
	 */
	clk_set_rate(vpu->aclk, 500 * 1000 * 1000);

	res = platform_get_resource(vpu->pdev, IORESOURCE_MEM, 0);
	vpu->base = devm_ioremap_resource(vpu->dev, res);
	if (IS_ERR(vpu->base))
		return PTR_ERR(vpu->base);

	vpu->dec_base = vpu->base + vpu->variant->dec_offset;

	ret = dma_set_coherent_mask(vpu->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(vpu->dev, "could not set DMA coherent mask\n");
		return ret;
	}

	irq_dec = platform_get_irq(vpu->pdev, 0);
	if (irq_dec <= 0) {
		dev_err(vpu->dev, "could not get vdec IRQ\n");
		return -ENXIO;
	}

	ret = devm_request_threaded_irq(vpu->dev, irq_dec, NULL,
					rk3399_vdec_irq, IRQF_ONESHOT,
					dev_name(vpu->dev), vpu);
	if (ret) {
		dev_err(vpu->dev, "could not request vdec IRQ\n");
		return ret;
	}

	return 0;
}

static void rk3399_vdec_clk_enable(struct rockchip_vpu_dev *vpu)
{
	clk_prepare_enable(vpu->aclk);
	clk_prepare_enable(vpu->hclk);
	clk_prepare_enable(vpu->sclk_cabac);
	clk_prepare_enable(vpu->sclk_core);
}

static void rk3399_vdec_clk_disable(struct rockchip_vpu_dev *vpu)
{
	clk_disable_unprepare(vpu->sclk_core);
	clk_disable_unprepare(vpu->sclk_cabac);
	clk_disable_unprepare(vpu->hclk);
	clk_disable_unprepare(vpu->aclk);
}

static void rk3399_vdec_reset(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	vdpu_write(vpu, RKVDEC_IRQ_DIS, RKVDEC_REG_INTERRUPT);
	vdpu_write(vpu, 0, RKVDEC_REG_SYSCTRL);
}

/*
 * Supported codec ops.
 */

static const struct rockchip_vpu_codec_ops rk3399_vdec_mode_ops[] = {
	[RK_VPU_CODEC_H264D] = {
		.init = rk3399_vdec_h264d_init,
		.exit = rk3399_vdec_h264d_exit,
		.run = rk3399_vdec_h264d_run,
		.done = rockchip_vpu_run_done,
		.reset = rk3399_vdec_reset,
	},
	[RK_VPU_CODEC_VP9D] = {
		.init = rk3399_vdec_vp9d_init,
		.exit = rk3399_vdec_vp9d_exit,
		.run = rk3399_vdec_vp9d_run,
		.done = rk3399_vdec_vp9d_done,
		.reset = rk3399_vdec_reset,
	},
};

/*
 * VPU variant.
 */

const struct rockchip_vpu_variant rk3399_vdec_variant = {
	.dec_offset = 0x0,
	.dec_fmts = rk3399_vdec_fmts,
	.num_dec_fmts = ARRAY_SIZE(rk3399_vdec_fmts),
	.mode_ops = rk3399_vdec_mode_ops,
	.hw_probe = rk3399_vdec_hw_probe,
	.clk_enable = rk3399_vdec_clk_enable,
	.clk_disable = rk3399_vdec_clk_disable,
};
