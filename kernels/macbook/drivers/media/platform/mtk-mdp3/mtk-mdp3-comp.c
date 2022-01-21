// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-regs.h"

#include "mdp_reg_rdma.h"
#include "mdp_reg_ccorr.h"
#include "mdp_reg_rsz.h"
#include "mdp_reg_wrot.h"
#include "mdp_reg_wdma.h"
#include "mdp_reg_isp.h"

static struct mdp_comp_list comp_list;

enum mdp_comp_id get_comp_camin(void)
{
	return comp_list.camin;
}

enum mdp_comp_id get_comp_camin2(void)
{
	return comp_list.camin2;
}

enum mdp_comp_id get_comp_rdma0(void)
{
	return comp_list.rdma0;
}

enum mdp_comp_id get_comp_aal0(void)
{
	return comp_list.aal0;
}

enum mdp_comp_id get_comp_ccorr0(void)
{
	return comp_list.ccorr0;
}

enum mdp_comp_id get_comp_rsz0(void)
{
	return comp_list.rsz0;
}

enum mdp_comp_id get_comp_rsz1(void)
{
	return comp_list.rsz1;
}

enum mdp_comp_id get_comp_tdshp0(void)
{
	return comp_list.tdshp0;
}

enum mdp_comp_id get_comp_color0(void)
{
	return comp_list.color0;
}

enum mdp_comp_id get_comp_wrot0(void)
{
	return comp_list.wrot0;
}

enum mdp_comp_id get_comp_wdma(void)
{
	return comp_list.wdma;
}

enum mdp_comp_id get_comp_merge2(void)
{
	return comp_list.merge2;
}

enum mdp_comp_id get_comp_merge3(void)
{
	return comp_list.merge3;
}

static const struct mdp_platform_config *__get_plat_cfg(const struct mdp_comp_ctx *ctx)
{
	if (!ctx)
		return NULL;

	return ctx->comp->mdp_dev->mdp_data->mdp_cfg;
}

static s64 get_comp_flag(const struct mdp_comp_ctx *ctx)
{
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);

	if (mdp_cfg && mdp_cfg->rdma_rsz1_sram_sharing)
		if (ctx->comp->id == MDP_RDMA0)
			return BIT(MDP_RDMA0) | BIT(MDP_RSZ1);

	return BIT(ctx->comp->id);
}

static int init_rdma(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	if (mdp_cfg && mdp_cfg->rdma_support_10bit) {
		struct mdp_comp *prz1 = ctx->comp->mdp_dev->comp[MDP_RSZ1];

		/* Disable RSZ1 */
		if (ctx->comp->id == MDP_RDMA0 && prz1)
			MM_REG_WRITE(cmd, subsys_id, prz1->reg_base, PRZ_ENABLE,
				     0x0, BIT(0));
	}

	/* Reset RDMA */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_RESET, BIT(0), BIT(0));
	MM_REG_POLL(cmd, subsys_id, base, MDP_RDMA_MON_STA_1, BIT(8), BIT(8));
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_RESET, 0x0, BIT(0));
	return 0;
}

static int config_rdma_frame(struct mdp_comp_ctx *ctx,
			     struct mmsys_cmdq_cmd *cmd,
			     const struct v4l2_rect *compose)
{
	const struct mdp_rdma_data *rdma = &ctx->param->rdma;
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);
	u32 colorformat = ctx->input->buffer.format.colorformat;
	bool block10bit = MDP_COLOR_IS_10BIT_PACKED(colorformat);
	bool en_ufo = MDP_COLOR_IS_UFP(colorformat);
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	if (mdp_cfg && mdp_cfg->rdma_support_10bit) {
		if (block10bit)
			MM_REG_WRITE(cmd, subsys_id, base,
				     MDP_RDMA_RESV_DUMMY_0, 0x7, 0x7);
		else
			MM_REG_WRITE(cmd, subsys_id, base,
				     MDP_RDMA_RESV_DUMMY_0, 0x0, 0x7);
	}

	/* Setup smi control */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_GMCIF_CON,
		     (7 <<  4) + //burst type to 8
		     (1 << 16),  //enable pre-ultra
		     0x00030071);

	/* Setup source frame info */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_CON, rdma->src_ctrl,
		     0x03C8FE0F);

	if (mdp_cfg)
		if (mdp_cfg->rdma_support_10bit && en_ufo) {
			/* Setup source buffer base */
			MM_REG_WRITE(cmd, subsys_id,
				     base, MDP_RDMA_UFO_DEC_LENGTH_BASE_Y,
				     rdma->ufo_dec_y, 0xFFFFFFFF);
			MM_REG_WRITE(cmd, subsys_id,
				     base, MDP_RDMA_UFO_DEC_LENGTH_BASE_C,
				     rdma->ufo_dec_c, 0xFFFFFFFF);
			/* Set 10bit source frame pitch */
			if (block10bit)
				MM_REG_WRITE(cmd, subsys_id,
					     base, MDP_RDMA_MF_BKGD_SIZE_IN_PXL,
					     rdma->mf_bkgd_in_pxl, 0x001FFFFF);
		}

	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_CON, rdma->control,
		     0x1110);
	/* Setup source buffer base */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_BASE_0, rdma->iova[0],
		     0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_BASE_1, rdma->iova[1],
		     0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_BASE_2, rdma->iova[2],
		     0xFFFFFFFF);
	/* Setup source buffer end */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_END_0,
		     rdma->iova_end[0], 0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_END_1,
		     rdma->iova_end[1], 0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_END_2,
		     rdma->iova_end[2], 0xFFFFFFFF);
	/* Setup source frame pitch */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_MF_BKGD_SIZE_IN_BYTE,
		     rdma->mf_bkgd, 0x001FFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SF_BKGD_SIZE_IN_BYTE,
		     rdma->sf_bkgd, 0x001FFFFF);
	/* Setup color transform */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_TRANSFORM_0,
		     rdma->transform, 0x0F110000);

	return 0;
}

static int config_rdma_subfrm(struct mdp_comp_ctx *ctx,
			      struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct mdp_rdma_subfrm *subfrm = &ctx->param->rdma.subfrms[index];
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);
	u32 colorformat = ctx->input->buffer.format.colorformat;
	bool block10bit = MDP_COLOR_IS_10BIT_PACKED(colorformat);
	bool en_ufo = MDP_COLOR_IS_UFP(colorformat);
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Enable RDMA */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_EN, BIT(0), BIT(0));

	/* Set Y pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_0,
		     subfrm->offset[0], 0xFFFFFFFF);

	/* Set 10bit UFO mode */
	if (mdp_cfg)
		if (mdp_cfg->rdma_support_10bit && block10bit && en_ufo)
			MM_REG_WRITE(cmd, subsys_id, base,
				     MDP_RDMA_SRC_OFFSET_0_P,
				     subfrm->offset_0_p, 0xFFFFFFFF);

	/* Set U pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_1,
		     subfrm->offset[1], 0xFFFFFFFF);
	/* Set V pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_2,
		     subfrm->offset[2], 0xFFFFFFFF);
	/* Set source size */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_MF_SRC_SIZE, subfrm->src,
		     0x1FFF1FFF);
	/* Set target size */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_MF_CLIP_SIZE,
		     subfrm->clip, 0x1FFF1FFF);
	/* Set crop offset */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_MF_OFFSET_1,
		     subfrm->clip_ofst, 0x003F001F);

	if (mdp_cfg && mdp_cfg->rdma_upsample_repeat_only)
		if ((csf->in.right - csf->in.left + 1) > 320)
			MM_REG_WRITE(cmd, subsys_id, base,
				     MDP_RDMA_RESV_DUMMY_0, BIT(2), BIT(2));

	return 0;
}

static int wait_rdma_event(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	struct device *dev = &ctx->comp->mdp_dev->pdev->dev;
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;
	int evt = -1;

	if (ctx->comp->alias_id == 0)
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, RDMA0_DONE);
	else
		dev_err(dev, "Do not support RDMA1_DONE event\n");

	if (evt > 0)
		MM_REG_WAIT(cmd, evt);
	/* Disable RDMA */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_RDMA_EN, 0x0, BIT(0));
	return 0;
}

static const struct mdp_comp_ops rdma_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_rdma,
	.config_frame = config_rdma_frame,
	.config_subfrm = config_rdma_subfrm,
	.wait_comp_event = wait_rdma_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_rsz(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Reset RSZ */
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_ENABLE, 0x10000, BIT(16));
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_ENABLE, 0x0, BIT(16));
	/* Enable RSZ */
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_ENABLE, BIT(0), BIT(0));
	return 0;
}

static int config_rsz_frame(struct mdp_comp_ctx *ctx,
			    struct mmsys_cmdq_cmd *cmd,
			    const struct v4l2_rect *compose)
{
	const struct mdp_rsz_data *rsz = &ctx->param->rsz;
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	if (ctx->param->frame.bypass) {
		/* Disable RSZ */
		MM_REG_WRITE(cmd, subsys_id, base, PRZ_ENABLE, 0x0, BIT(0));
		return 0;
	}

	MM_REG_WRITE(cmd, subsys_id, base, PRZ_CONTROL_1, rsz->control1,
		     0x03FFFDF3);
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_CONTROL_2, rsz->control2,
		     0x0FFFC290);
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_HORIZONTAL_COEFF_STEP,
		     rsz->coeff_step_x, 0x007FFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_VERTICAL_COEFF_STEP,
		     rsz->coeff_step_y, 0x007FFFFF);
	return 0;
}

static int config_rsz_subfrm(struct mdp_comp_ctx *ctx,
			     struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct mdp_rsz_subfrm *subfrm = &ctx->param->rsz.subfrms[index];
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE(cmd, subsys_id, base, PRZ_CONTROL_2, subfrm->control2,
		     0x00003800);
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_INPUT_IMAGE, subfrm->src,
		     0xFFFFFFFF);

	if (mdp_cfg && mdp_cfg->rsz_disable_dcm_small_sample)
		if ((csf->in.right - csf->in.left + 1) <= 16)
			MM_REG_WRITE(cmd, subsys_id, base, PRZ_CONTROL_1,
				     BIT(27), BIT(27));

	MM_REG_WRITE(cmd, subsys_id, base, PRZ_LUMA_HORIZONTAL_INTEGER_OFFSET,
		     csf->luma.left, 0xFFFF);
	MM_REG_WRITE(cmd, subsys_id,
		     base, PRZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET,
		     csf->luma.left_subpix, 0x1FFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_LUMA_VERTICAL_INTEGER_OFFSET,
		     csf->luma.top, 0xFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, PRZ_LUMA_VERTICAL_SUBPIXEL_OFFSET,
		     csf->luma.top_subpix, 0x1FFFFF);
	MM_REG_WRITE(cmd, subsys_id,
		     base, PRZ_CHROMA_HORIZONTAL_INTEGER_OFFSET,
		     csf->chroma.left, 0xFFFF);
	MM_REG_WRITE(cmd, subsys_id,
		     base, PRZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET,
		     csf->chroma.left_subpix, 0x1FFFFF);

	MM_REG_WRITE(cmd, subsys_id, base, PRZ_OUTPUT_IMAGE, subfrm->clip,
		     0xFFFFFFFF);

	return 0;
}

static int advance_rsz_subfrm(struct mdp_comp_ctx *ctx,
			      struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);

	if (mdp_cfg && mdp_cfg->rsz_disable_dcm_small_sample) {
		const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
		phys_addr_t base = ctx->comp->reg_base;
		u8 subsys_id = ctx->comp->subsys_id;

		if ((csf->in.right - csf->in.left + 1) <= 16)
			MM_REG_WRITE(cmd, subsys_id, base, PRZ_CONTROL_1, 0x0,
				     BIT(27));
	}

	return 0;
}

static const struct mdp_comp_ops rsz_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_rsz,
	.config_frame = config_rsz_frame,
	.config_subfrm = config_rsz_subfrm,
	.wait_comp_event = NULL,
	.advance_subfrm = advance_rsz_subfrm,
	.post_process = NULL,
};

static int init_wrot(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Reset WROT */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_SOFT_RST, BIT(0), BIT(0));
	MM_REG_POLL(cmd, subsys_id, base, VIDO_SOFT_RST_STAT, BIT(0), BIT(0));
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_SOFT_RST, 0x0, BIT(0));
	MM_REG_POLL(cmd, subsys_id, base, VIDO_SOFT_RST_STAT, 0x0, BIT(0));
	return 0;
}

static int config_wrot_frame(struct mdp_comp_ctx *ctx,
			     struct mmsys_cmdq_cmd *cmd,
			     const struct v4l2_rect *compose)
{
	const struct mdp_wrot_data *wrot = &ctx->param->wrot;
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Write frame base address */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_BASE_ADDR, wrot->iova[0],
		     0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_BASE_ADDR_C, wrot->iova[1],
		     0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_BASE_ADDR_V, wrot->iova[2],
		     0xFFFFFFFF);
	/* Write frame related registers */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_CTRL, wrot->control,
		     0xF131510F);
	/* Write frame Y pitch */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_STRIDE, wrot->stride[0],
		     0x0000FFFF);
	/* Write frame UV pitch */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_STRIDE_C, wrot->stride[1],
		     0xFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_STRIDE_V, wrot->stride[2],
		     0xFFFF);
	/* Write matrix control */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_MAT_CTRL, wrot->mat_ctrl, 0xF3);

	/* Set the fixed ALPHA as 0xFF */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_DITHER, 0xFF000000,
		     0xFF000000);
	/* Set VIDO_EOL_SEL */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_RSV_1, BIT(31), BIT(31));
	/* Set VIDO_FIFO_TEST */
	if (wrot->fifo_test != 0)
		MM_REG_WRITE(cmd, subsys_id, base, VIDO_FIFO_TEST,
			     wrot->fifo_test, 0xFFF);
	/* Filter enable */
	if (mdp_cfg && mdp_cfg->wrot_filter_constraint)
		MM_REG_WRITE(cmd, subsys_id, base, VIDO_MAIN_BUF_SIZE,
			     wrot->filter, 0x77);

	return 0;
}

static int config_wrot_subfrm(struct mdp_comp_ctx *ctx,
			      struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct mdp_wrot_subfrm *subfrm = &ctx->param->wrot.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Write Y pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_OFST_ADDR,
		     subfrm->offset[0], 0x0FFFFFFF);
	/* Write U pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_OFST_ADDR_C,
		     subfrm->offset[1], 0x0FFFFFFF);
	/* Write V pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_OFST_ADDR_V,
		     subfrm->offset[2], 0x0FFFFFFF);
	/* Write source size */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_IN_SIZE, subfrm->src,
		     0x1FFF1FFF);
	/* Write target size */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_TAR_SIZE, subfrm->clip,
		     0x1FFF1FFF);
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_CROP_OFST, subfrm->clip_ofst,
		     0x1FFF1FFF);

	MM_REG_WRITE(cmd, subsys_id, base, VIDO_MAIN_BUF_SIZE,
		     subfrm->main_buf, 0x1FFF7F00);

	/* Enable WROT */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_ROT_EN, BIT(0), BIT(0));

	return 0;
}

static int wait_wrot_event(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	const struct mdp_platform_config *mdp_cfg = __get_plat_cfg(ctx);
	struct device *dev = &ctx->comp->mdp_dev->pdev->dev;
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;
	int evt = -1;

	if (ctx->comp->alias_id == 0)
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, WROT0_DONE);
	else
		dev_err(dev, "Do not support WROT1_DONE event\n");

	if (evt > 0)
		MM_REG_WAIT(cmd, evt);

	if (mdp_cfg && mdp_cfg->wrot_filter_constraint)
		MM_REG_WRITE(cmd, subsys_id, base, VIDO_MAIN_BUF_SIZE, 0x0,
			     0x77);

	/* Disable WROT */
	MM_REG_WRITE(cmd, subsys_id, base, VIDO_ROT_EN, 0x0, BIT(0));

	return 0;
}

static const struct mdp_comp_ops wrot_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_wrot,
	.config_frame = config_wrot_frame,
	.config_subfrm = config_wrot_subfrm,
	.wait_comp_event = wait_wrot_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_wdma(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Reset WDMA */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_RST, BIT(0), BIT(0));
	MM_REG_POLL(cmd, subsys_id, base, WDMA_FLOW_CTRL_DBG, BIT(0), BIT(0));
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_RST, 0x0, BIT(0));
	return 0;
}

static int config_wdma_frame(struct mdp_comp_ctx *ctx,
			     struct mmsys_cmdq_cmd *cmd,
			     const struct v4l2_rect *compose)
{
	const struct mdp_wdma_data *wdma = &ctx->param->wdma;
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE(cmd, subsys_id, base, WDMA_BUF_CON2, 0x10101050,
		     0xFFFFFFFF);

	/* Setup frame information */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_CFG, wdma->wdma_cfg,
		     0x0F01B8F0);
	/* Setup frame base address */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_ADDR,   wdma->iova[0],
		     0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_U_ADDR, wdma->iova[1],
		     0xFFFFFFFF);
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_V_ADDR, wdma->iova[2],
		     0xFFFFFFFF);
	/* Setup Y pitch */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_W_IN_BYTE,
		     wdma->w_in_byte, 0x0000FFFF);
	/* Setup UV pitch */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_UV_PITCH,
		     wdma->uv_stride, 0x0000FFFF);
	/* Set the fixed ALPHA as 0xFF */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_ALPHA, 0x800000FF,
		     0x800000FF);

	return 0;
}

static int config_wdma_subfrm(struct mdp_comp_ctx *ctx,
			      struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct mdp_wdma_subfrm *subfrm = &ctx->param->wdma.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* Write Y pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_ADDR_OFFSET,
		     subfrm->offset[0], 0x0FFFFFFF);
	/* Write U pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_U_ADDR_OFFSET,
		     subfrm->offset[1], 0x0FFFFFFF);
	/* Write V pixel offset */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_DST_V_ADDR_OFFSET,
		     subfrm->offset[2], 0x0FFFFFFF);
	/* Write source size */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_SRC_SIZE, subfrm->src,
		     0x3FFF3FFF);
	/* Write target size */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_CLIP_SIZE, subfrm->clip,
		     0x3FFF3FFF);
	/* Write clip offset */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_CLIP_COORD, subfrm->clip_ofst,
		     0x3FFF3FFF);

	/* Enable WDMA */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_EN, BIT(0), BIT(0));

	return 0;
}

static int wait_wdma_event(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;
	int evt;

	evt = mdp_get_event_idx(ctx->comp->mdp_dev, WDMA0_DONE);
	if (evt > 0)
		MM_REG_WAIT(cmd, evt);
	/* Disable WDMA */
	MM_REG_WRITE(cmd, subsys_id, base, WDMA_EN, 0x0, BIT(0));
	return 0;
}

static const struct mdp_comp_ops wdma_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_wdma,
	.config_frame = config_wdma_frame,
	.config_subfrm = config_wdma_subfrm,
	.wait_comp_event = wait_wdma_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_ccorr(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* CCORR enable */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_CCORR_EN, BIT(0), BIT(0));
	/* Relay mode */
	MM_REG_WRITE(cmd, subsys_id, base, MDP_CCORR_CFG, BIT(0), BIT(0));
	return 0;
}

static int config_ccorr_frame(struct mdp_comp_ctx *ctx,
			      struct mmsys_cmdq_cmd *cmd,
			      const struct v4l2_rect *compose)
{
	/* Disabled function */
	return 0;
}

static int config_ccorr_subfrm(struct mdp_comp_ctx *ctx,
			       struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;
	u32 hsize, vsize;

	hsize = csf->in.right - csf->in.left + 1;
	vsize = csf->in.bottom - csf->in.top + 1;
	MM_REG_WRITE(cmd, subsys_id, base, MDP_CCORR_SIZE,
		     (hsize << 16) + (vsize <<  0), 0x1FFF1FFF);
	return 0;
}

static const struct mdp_comp_ops ccorr_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_ccorr,
	.config_frame = config_ccorr_frame,
	.config_subfrm = config_ccorr_subfrm,
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_isp(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	struct device *dev = ctx->comp->mdp_dev->mdp_mmsys;
	const struct isp_data *isp = &ctx->param->isp;

	/* Direct link */
	if (isp->dl_flags & BIT(MDP_CAMIN)) {
		dev_dbg(dev, "SW_RST ASYNC");
		mtk_mmsys_mdp_isp_ctrl(dev, cmd, MDP_COMP_CAMIN);
	}

	if (isp->dl_flags & BIT(MDP_CAMIN2)) {
		dev_dbg(dev, "SW_RST ASYNC2");
		mtk_mmsys_mdp_isp_ctrl(dev, cmd, MDP_COMP_CAMIN2);
	}

	return 0;
}

static int config_isp_frame(struct mdp_comp_ctx *ctx,
			    struct mmsys_cmdq_cmd *cmd,
			    const struct v4l2_rect *compose)
{
	struct device *dev = &ctx->comp->mdp_dev->pdev->dev;
	const struct isp_data *isp = &ctx->param->isp;
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	/* DIP_X_SMX1I_BASE_ADDR, DIP_X_SMX1O_BASE_ADDR */
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2890, isp->smxi_iova[0],
			  0xFFFFFFFF);
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x27D0, isp->smxi_iova[0],
			  0xFFFFFFFF);
	/* DIP_X_SMX2I_BASE_ADDR, DIP_X_SMX2O_BASE_ADDR */
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x28C0, isp->smxi_iova[1],
			  0xFFFFFFFF);
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2800, isp->smxi_iova[1],
			  0xFFFFFFFF);
	/* DIP_X_SMX3I_BASE_ADDR, DIP_X_SMX3O_BASE_ADDR */
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x28F0, isp->smxi_iova[2],
			  0xFFFFFFFF);
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2830, isp->smxi_iova[2],
			  0xFFFFFFFF);
	/* DIP_X_SMX4I_BASE_ADDR, DIP_X_SMX4O_BASE_ADDR */
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2920, isp->smxi_iova[3],
			  0xFFFFFFFF);
	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2860, isp->smxi_iova[3],
			  0xFFFFFFFF);

	switch (isp->cq_idx) {
	case ISP_DRV_DIP_CQ_THRE0:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2208,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE1:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2214,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE2:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2220,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE3:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x222C,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE4:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2238,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE5:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2244,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE6:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2250,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE7:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x225C,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE8:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2268,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE9:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2274,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE10:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2280,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE11:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x228C,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	default:
		dev_err(dev, "Do not support this cq (%d)", isp->cq_idx);
		return -EINVAL;
	}

	return 0;
}

static int config_isp_subfrm(struct mdp_comp_ctx *ctx,
			     struct mmsys_cmdq_cmd *cmd, u32 index)
{
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2304,
			  ctx->param->isp.tpipe_iova[index], 0xFFFFFFFF);
	return 0;
}

static int wait_isp_event(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd)
{
	const struct isp_data *isp = &ctx->param->isp;
	struct device *dev = &ctx->comp->mdp_dev->pdev->dev;
	phys_addr_t base = ctx->comp->reg_base;
	u8 subsys_id = ctx->comp->subsys_id;
	int evt;

	/* MDP_DL_SEL: select MDP_CROP */
	if (isp->dl_flags & BIT(MDP_CAMIN))
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x30, 0x0, BIT(9));
	/* MDP2_DL_SEL: select MDP_CROP2 */
	if (isp->dl_flags & BIT(MDP_CAMIN2))
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x30, 0x0,
				  BIT(10) | BIT(11));

	switch (isp->cq_idx) {
	case ISP_DRV_DIP_CQ_THRE0:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(0), BIT(0));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_0_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE1:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(1), BIT(1));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_1_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE2:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(2), BIT(2));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_2_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE3:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(3), BIT(3));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_3_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE4:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(4), BIT(4));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_4_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE5:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(5), BIT(5));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_5_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE6:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(6), BIT(6));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_6_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE7:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(7), BIT(7));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_7_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE8:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(8), BIT(8));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_8_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE9:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(9), BIT(9));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_9_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE10:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(10), BIT(10));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_10_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE11:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, BIT(11), BIT(11));
		evt = mdp_get_event_idx(ctx->comp->mdp_dev, ISP_P2_11_DONE);
		break;
	default:
		dev_err(dev, "Do not support this cq (%d)", isp->cq_idx);
		return -EINVAL;
	}

	MM_REG_WAIT(cmd, evt);

	return 0;
}

static const struct mdp_comp_ops imgi_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_isp,
	.config_frame = config_isp_frame,
	.config_subfrm = config_isp_subfrm,
	.wait_comp_event = wait_isp_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int config_camin_subfrm(struct mdp_comp_ctx *ctx,
			       struct mmsys_cmdq_cmd *cmd, u32 index)
{
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	struct device *dev = ctx->comp->mdp_dev->mdp_mmsys;
	u32 camin_w, camin_h;

	camin_w = csf->in.right - csf->in.left + 1;
	camin_h = csf->in.bottom - csf->in.top + 1;

	/* Config for direct link */
	if (ctx->comp->alias_id == 0)
		mtk_mmsys_mdp_camin_ctrl(dev, cmd, MDP_COMP_CAMIN,
					 camin_w, camin_h);
	if (ctx->comp->alias_id == 1)
		mtk_mmsys_mdp_camin_ctrl(dev, cmd, MDP_COMP_CAMIN2,
					 camin_w, camin_h);

	return 0;
}

static const struct mdp_comp_ops camin_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = NULL,
	.config_frame = NULL,
	.config_subfrm = config_camin_subfrm,
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static const struct mdp_comp_ops *mdp_comp_ops[MDP_COMP_TYPE_COUNT] = {
	[MDP_COMP_TYPE_RDMA] =		&rdma_ops,
	[MDP_COMP_TYPE_RSZ] =		&rsz_ops,
	[MDP_COMP_TYPE_WROT] =		&wrot_ops,
	[MDP_COMP_TYPE_WDMA] =		&wdma_ops,
	[MDP_COMP_TYPE_PATH1] =		NULL,
	[MDP_COMP_TYPE_PATH2] =		NULL,
	[MDP_COMP_TYPE_CCORR] =		&ccorr_ops,
	[MDP_COMP_TYPE_IMGI] =		&imgi_ops,
	[MDP_COMP_TYPE_EXTO] =		NULL,
	[MDP_COMP_TYPE_DL_PATH1] =	&camin_ops,
	[MDP_COMP_TYPE_DL_PATH2] =	&camin_ops,
};

static const struct of_device_id mdp_comp_dt_ids[] = {
	{
		.compatible = "mediatek,mt8183-mdp3-rdma",
		.data = (void *)MDP_COMP_TYPE_RDMA,
	}, {
		.compatible = "mediatek,mt8183-mdp3-ccorr",
		.data = (void *)MDP_COMP_TYPE_CCORR,
	}, {
		.compatible = "mediatek,mt8183-mdp3-rsz",
		.data = (void *)MDP_COMP_TYPE_RSZ,
	}, {
		.compatible = "mediatek,mt8183-mdp3-wrot",
		.data = (void *)MDP_COMP_TYPE_WROT,
	}, {
		.compatible = "mediatek,mt8183-mdp3-wdma",
		.data = (void *)MDP_COMP_TYPE_WDMA,
	},
	{}
};

static const struct of_device_id mdp_sub_comp_dt_ids[] = {
	{
		.compatible = "mediatek,mt8183-mdp3-path1",
		.data = (void *)MDP_COMP_TYPE_PATH1,
	}, {
		.compatible = "mediatek,mt8183-mdp3-path2",
		.data = (void *)MDP_COMP_TYPE_PATH2,
	}, {
		.compatible = "mediatek,mt8183-mdp3-imgi",
		.data = (void *)MDP_COMP_TYPE_IMGI,
	}, {
		.compatible = "mediatek,mt8183-mdp3-exto",
		.data = (void *)MDP_COMP_TYPE_EXTO,
	}, {
		.compatible = "mediatek,mt8183-mdp3-dl1",
		.data = (void *)MDP_COMP_TYPE_DL_PATH1,
	}, {
		.compatible = "mediatek,mt8183-mdp3-dl2",
		.data = (void *)MDP_COMP_TYPE_DL_PATH2,
	},
	{}
};

static int mdp_comp_get_id(struct mdp_dev *mdp, enum mdp_comp_type type, u32 alias_id)
{
	int i;

	for (i = 0; i < mdp->mdp_data->comp_data_len; i++)
		if (mdp->mdp_data->comp_data[i].match.type == type &&
		    mdp->mdp_data->comp_data[i].match.alias_id == alias_id)
			return i;
	return -ENODEV;
}

void mdp_comp_clock_on(struct device *dev, struct mdp_comp *comp)
{
	int i, err;

	if (comp->comp_dev) {
		err = pm_runtime_get_sync(comp->comp_dev);
		if (err < 0)
			dev_err(dev,
				"Failed to get power, err %d. type:%d id:%d\n",
				err, comp->type, comp->id);
	}

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		err = clk_prepare_enable(comp->clks[i]);
		if (err)
			dev_err(dev,
				"Failed to enable clk %d. type:%d id:%d\n",
				i, comp->type, comp->id);
	}
}

void mdp_comp_clock_off(struct device *dev, struct mdp_comp *comp)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_disable_unprepare(comp->clks[i]);
	}

	if (comp->comp_dev)
		pm_runtime_put(comp->comp_dev);
}

void mdp_comp_clocks_on(struct device *dev, struct mdp_comp *comps, int num)
{
	int i;

	for (i = 0; i < num; i++)
		mdp_comp_clock_on(dev, &comps[i]);
}

void mdp_comp_clocks_off(struct device *dev, struct mdp_comp *comps, int num)
{
	int i;

	for (i = 0; i < num; i++)
		mdp_comp_clock_off(dev, &comps[i]);
}

static int mdp_get_subsys_id(struct mdp_dev *mdp, struct device *dev, struct device_node *node,
			     struct mdp_comp *comp)
{
	struct platform_device *comp_pdev;
	struct cmdq_client_reg  cmdq_reg;
	int ret = 0;
	int index = 0;

	if (!dev || !node || !comp)
		return -EINVAL;

	comp_pdev = of_find_device_by_node(node);

	if (!comp_pdev) {
		dev_err(dev, "get comp_pdev fail! comp id=%d type=%d\n",
			comp->id, comp->type);
		return -ENODEV;
	}

	index = mdp->mdp_data->comp_info[comp->type].dts_reg_ofst;
	ret = cmdq_dev_get_client_reg(&comp_pdev->dev, &cmdq_reg, index);
	if (ret != 0) {
		dev_err(&comp_pdev->dev, "cmdq_dev_get_subsys fail!\n");
		return -EINVAL;
	}

	comp->subsys_id = cmdq_reg.subsys;
	dev_dbg(&comp_pdev->dev, "subsys id=%d\n", cmdq_reg.subsys);

	return 0;
}

static void __mdp_comp_init(struct mdp_dev *mdp, struct device_node *node,
			    struct mdp_comp *comp)
{
	struct resource res;
	phys_addr_t base;
	int index = mdp->mdp_data->comp_info[comp->type].dts_reg_ofst;

	if (of_address_to_resource(node, index, &res) < 0)
		base = 0L;
	else
		base = res.start;

	comp->mdp_dev = mdp;
	comp->regs = of_iomap(node, 0);
	comp->reg_base = base;
}

static int mdp_comp_init(struct mdp_dev *mdp, struct device_node *node,
			 struct mdp_comp *comp, enum mdp_comp_id id)
{
	struct device *dev = &mdp->pdev->dev;
	int clk_num;
	int clk_ofst;
	int i;

	if (id < 0 || id >= mdp->mdp_data->comp_data_len) {
		dev_err(dev, "Invalid component id %d\n", id);
		return -EINVAL;
	}

	comp->type = mdp->mdp_data->comp_data[id].match.type;
	comp->id = id;
	comp->alias_id = mdp->mdp_data->comp_data[id].match.alias_id;
	comp->ops = mdp_comp_ops[comp->type];
	__mdp_comp_init(mdp, node, comp);

	clk_num = mdp->mdp_data->comp_info[comp->type].clk_num;
	clk_ofst = mdp->mdp_data->comp_info[comp->type].clk_ofst;

	for (i = 0; i < clk_num; i++) {
		comp->clks[i] = of_clk_get(node, i + clk_ofst);
		if (IS_ERR(comp->clks[i]))
			break;
	}

	mdp_get_subsys_id(mdp, dev, node, comp);

	return 0;
}

static struct mdp_comp *mdp_comp_create(struct mdp_dev *mdp,
					struct device_node *node,
					enum mdp_comp_id id)
{
	struct device *dev = &mdp->pdev->dev;
	struct mdp_comp *comp;
	int ret;

	if (mdp->comp[id])
		return ERR_PTR(-EEXIST);

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return ERR_PTR(-ENOMEM);

	ret = mdp_comp_init(mdp, node, comp, id);
	if (ret) {
		kfree(comp);
		return ERR_PTR(ret);
	}
	mdp->comp[id] = comp;
	mdp->comp[id]->mdp_dev = mdp;

	dev_info(dev, "%s type:%d alias:%d id:%d base:%#x regs:%p\n",
		 dev->of_node->name, comp->type, comp->alias_id, id,
		 (u32)comp->reg_base, comp->regs);
	return comp;
}

static int mdp_sub_comps_create(struct mdp_dev *mdp, struct device_node *node)
{
	struct device *dev = &mdp->pdev->dev;
	struct property *prop;
	const char *name;
	int index = 0;

	of_property_for_each_string(node, "mediatek,mdp3-comps", prop, name) {
		const struct of_device_id *matches = mdp_sub_comp_dt_ids;
		enum mdp_comp_type type = MDP_COMP_TYPE_INVALID;
		u32 alias_id;
		int id, ret;
		struct mdp_comp *comp;

		for (; matches->compatible[0]; matches++) {
			if (of_compat_cmp(name, matches->compatible,
					  strlen(matches->compatible)) == 0) {
				type = (enum mdp_comp_type)matches->data;
				break;
			}
		}

		ret = of_property_read_u32_index(node, "mediatek,mdp3-comp-ids",
						 index, &alias_id);
		if (ret) {
			dev_warn(dev, "Skipping unknown component %s\n", name);
			return ret;
		}

		id = mdp_comp_get_id(mdp, type, alias_id);
		if (id < 0) {
			dev_err(dev, "Failed to get comp id: %s (%d, %d)\n",
				name, type, alias_id);
			return -ENODEV;
		}

		comp = mdp_comp_create(mdp, node, id);
		if (IS_ERR(comp))
			return PTR_ERR(comp);

		index++;
	}
	return 0;
}

static int mdp_mm_init(struct mdp_dev *mdp, struct mdp_comp *comp,
		       const char *ref_name)
{
	struct device_node *node;
	struct device *dev = &mdp->pdev->dev;
	phys_addr_t base;
	struct resource res;
	struct platform_device *comp_pdev;
	struct cmdq_client_reg  cmdq_reg;
	int ret = 0;

	node = of_parse_phandle(dev->of_node, ref_name, 0);
	if (!node) {
		dev_err(dev, "Failed to parse dt %s\n", ref_name);
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, &res) < 0)
		base = 0L;
	else
		base = res.start;

	comp->mdp_dev = mdp;
	comp->regs = of_iomap(node, 0);
	comp->reg_base = base;

	comp_pdev = of_find_device_by_node(node);
	if (!comp_pdev) {
		dev_err(dev, "get comp_pdev fail! comp id=%d type=%d\n",
			comp->id, comp->type);
		return -ENODEV;
	}

	ret = cmdq_dev_get_client_reg(&comp_pdev->dev, &cmdq_reg, 0);
	if (ret != 0) {
		dev_err(&comp_pdev->dev, "cmdq_dev_get_subsys fail!\n");
		return -EINVAL;
	}
	comp->subsys_id = cmdq_reg.subsys;
	dev_dbg(&comp_pdev->dev, "subsys id=%d\n", cmdq_reg.subsys);

	of_node_put(node);
	if (!comp->reg_base) {
		dev_err(dev, "Failed to init %s base\n", ref_name);
		of_node_put(node);
		return -EINVAL;
	}

	return 0;
}

static void mdp_comp_deinit(struct mdp_comp *comp)
{
	if (!comp)
		return;

	if (comp->regs)
		iounmap(comp->regs);
}

void mdp_component_deinit(struct mdp_dev *mdp)
{
	int i;

	mdp_comp_deinit(&mdp->mmsys);
	for (i = 0; i < mdp->mdp_data->pipe_info_len; i++)
		mtk_mutex_put(mdp->mdp_mutex[mdp->mdp_data->pipe_info[i].pipe_id]);

	for (i = 0; i < ARRAY_SIZE(mdp->comp); i++) {
		if (mdp->comp[i]) {
			pm_runtime_disable(mdp->comp[i]->comp_dev);
			mdp_comp_deinit(mdp->comp[i]);
			kfree(mdp->comp[i]);
		}
	}
}

int mdp_component_init(struct mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;
	struct device_node *node, *parent;
	struct platform_device *pdev;
	u32 alias_id;
	int ret;

	ret = mdp_mm_init(mdp, &mdp->mmsys, "mediatek,mmsys");
	if (ret)
		goto err_init_mm;

	memcpy(&comp_list, mdp->mdp_data->comp_list, sizeof(struct mdp_comp_list));
	parent = dev->of_node->parent;
	/* Iterate over sibling MDP function blocks */
	for_each_child_of_node(parent, node) {
		const struct of_device_id *of_id;
		enum mdp_comp_type type;
		int id;
		struct mdp_comp *comp;

		of_id = of_match_node(mdp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_info(dev, "Skipping disabled component %pOF\n", node);
			continue;
		}

		type = (enum mdp_comp_type)of_id->data;
		ret = of_property_read_u32(node, "mediatek,mdp3-id", &alias_id);
		if (ret) {
			dev_warn(dev, "Skipping unknown component %pOF\n", node);
			continue;
		}
		id = mdp_comp_get_id(mdp, type, alias_id);
		if (id < 0) {
			dev_err(dev,
				"Fail to get component id: type %d alias %d\n",
				type, alias_id);
			continue;
		}

		comp = mdp_comp_create(mdp, node, id);
		if (IS_ERR(comp))
			goto err_init_comps;

		ret = mdp_sub_comps_create(mdp, node);
		if (ret)
			goto err_init_comps;

		/* Only DMA capable components need the pm control */
		comp->comp_dev = NULL;
		if (comp->type != MDP_COMP_TYPE_RDMA &&
		    comp->type != MDP_COMP_TYPE_WROT &&
		    comp->type != MDP_COMP_TYPE_WDMA)
			continue;

		pdev = of_find_device_by_node(node);
		if (!pdev) {
			dev_warn(dev, "can't find platform device of node:%s\n",
				 node->name);
			return -ENODEV;
		}

		comp->comp_dev = &pdev->dev;
		pm_runtime_enable(comp->comp_dev);
	}
	return 0;

err_init_comps:
	mdp_component_deinit(mdp);
err_init_mm:
	return ret;
}

int mdp_comp_ctx_init(struct mdp_dev *mdp, struct mdp_comp_ctx *ctx,
		      const struct img_compparam *param,
	const struct img_ipi_frameparam *frame)
{
	struct device *dev = &mdp->pdev->dev;
	int i;

	if (param->type < 0 || param->type >= MDP_MAX_COMP_COUNT) {
		dev_err(dev, "Invalid component id %d", param->type);
		return -EINVAL;
	}

	ctx->comp = mdp->comp[param->type];
	if (!ctx->comp) {
		dev_err(dev, "Uninit component id %d", param->type);
		return -EINVAL;
	}

	ctx->param = param;
	ctx->input = &frame->inputs[param->input];
	for (i = 0; i < param->num_outputs; i++)
		ctx->outputs[i] = &frame->outputs[param->outputs[i]];
	return 0;
}
