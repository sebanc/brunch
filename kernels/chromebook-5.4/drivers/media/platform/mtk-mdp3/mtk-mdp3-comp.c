// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-regs.h"

#include "mdp-platform.h"
#include "mmsys_config.h"
#include "mdp_reg_rdma.h"
#include "mdp_reg_rsz.h"
#include "mdp_reg_wrot.h"
#include "mdp_reg_aal.h"
#include "mdp_reg_tdshp.h"
#include "mdp_reg_color.h"
#include "mdp_reg_hdr.h"
#include "isp_reg.h"

static s64 get_comp_flag(const struct mdp_comp_ctx *ctx)
{
#if RDMA0_RSZ1_SRAM_SHARING
	if (ctx->comp->id == MDP_RDMA0)
		return (1 << MDP_RDMA0) | (1 << MDP_SCL1);
#endif
	return 1 << ctx->comp->id;
}

static int init_rdma(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;
#if RDMA0_RSZ1_SRAM_SHARING
	struct mdp_comp *prz1 = ctx->comp->mdp_dev->comp[MDP_SCL1];

	/* Disable RSZ1 */
	if (ctx->comp->id == MDP_RDMA0 && prz1)
		MM_REG_WRITE_S(cmd, subsys_id, prz1->reg_base, PRZ_ENABLE,
			     0x00000000, 0x00000001);
#endif

	/* Reset RDMA */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_RESET, 0x00000001,
		     0x00000001);
	MM_REG_POLL_S(cmd, subsys_id, base, MDP_RDMA_MON_STA_1, 0x00600100,
		    0x00600100);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_RESET, 0x00000000,
		     0x00000001);
	return 0;
}

static int config_rdma_frame(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd,
			     const struct v4l2_rect *compose)
{
	const struct mdp_rdma_data *rdma = &ctx->param->rdma;
	u32 colorformat = ctx->input->buffer.format.colorformat;
	bool block10bit = MDP_COLOR_IS_10BIT_PACKED(colorformat);
	bool en_ufo = MDP_COLOR_IS_UFP(colorformat);
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

#if RDMA_SUPPORT_10BIT
	if (block10bit)
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_RESV_DUMMY_0,
			     0x00000007, 0x00000007);
	else
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_RESV_DUMMY_0,
			     0x00000000, 0x00000007);
#endif
	/* Setup smi control */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_GMCIF_CON,
		     (1 <<  0) +
		     (7 <<  4) + //burst type to 8
		     (1 << 16),  //enable pre-ultra
		     0x00030071);
	/* Setup source frame info */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_CON,  rdma->src_ctrl,
		     0x03C8FE0F);
#if RDMA_SUPPORT_10BIT
	if (en_ufo) {
		/* Setup source buffer base */
		MM_REG_WRITE_S(cmd, subsys_id,
			     base, MDP_RDMA_UFO_DEC_LENGTH_BASE_Y,
			     rdma->ufo_dec_y, 0xFFFFFFFF);
		MM_REG_WRITE_S(cmd, subsys_id,
			     base, MDP_RDMA_UFO_DEC_LENGTH_BASE_C,
			     rdma->ufo_dec_c, 0xFFFFFFFF);
		/* Set 10bit source frame pitch */
		if (block10bit)
			MM_REG_WRITE_S(cmd, subsys_id,
				     base, MDP_RDMA_MF_BKGD_SIZE_IN_PXL,
				     rdma->mf_bkgd_in_pxl, 0x001FFFFF);
	}
#endif
    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_CON, rdma->control,
             0x00001110);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_COMP_CON, rdma->comp_ctrl,
		     rdma->comp_ctrl);
	/* Setup source buffer base */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_BASE_0, rdma->iova[0],
		     0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_BASE_1, rdma->iova[1],
		     0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_BASE_2, rdma->iova[2],
		     0xFFFFFFFF);
	/* Setup source buffer end */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_END_0,
		     rdma->iova_end[0], 0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_END_1,
		     rdma->iova_end[1], 0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_END_2,
		     rdma->iova_end[2], 0xFFFFFFFF);
	/* Setup source frame pitch */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_MF_BKGD_SIZE_IN_BYTE,
		     rdma->mf_bkgd, 0x001FFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SF_BKGD_SIZE_IN_BYTE,
		     rdma->sf_bkgd, 0x001FFFFF);
	/* Setup color transform */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_TRANSFORM_0,
		     rdma->transform, 0x0F110000);

	return 0;
}

static int config_rdma_subfrm(struct mdp_comp_ctx *ctx,
			      struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_rdma_subfrm *subfrm = &ctx->param->rdma.subfrms[index];
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	u32 colorformat = ctx->input->buffer.format.colorformat;
	bool block10bit = MDP_COLOR_IS_10BIT_PACKED(colorformat);
	bool en_ufo = MDP_COLOR_IS_UFP(colorformat);
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	/* Enable RDMA */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_EN, 0x00000001,
		     0x00000001);

	/* Set Y pixel offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_0,
		     subfrm->offset[0], 0xFFFFFFFF);
#if RDMA_SUPPORT_10BIT
	/* Set 10bit UFO mode */
	if (block10bit && en_ufo)
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_0_P,
			     subfrm->offset_0_p, 0xFFFFFFFF);
#endif
	/* Set U pixel offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_1,
		     subfrm->offset[1], 0xFFFFFFFF);
	/* Set V pixel offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_SRC_OFFSET_2,
		     subfrm->offset[2], 0xFFFFFFFF);
	/* Set source size */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_MF_SRC_SIZE, subfrm->src,
		     0x1FFF1FFF);
	/* Set target size */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_MF_CLIP_SIZE,
		     subfrm->clip, 0x1FFF1FFF);
	/* Set crop offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_MF_OFFSET_1,
		     subfrm->clip_ofst, 0x003F001F);

#if RDMA_UPSAMPLE_REPEAT_ONLY
	if ((csf->in.right - csf->in.left + 1) > 320)
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_RESV_DUMMY_0,
			     0x00000004, 0x00000004);
#endif

	return 0;
}

static int wait_rdma_event(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	if (ctx->comp->alias_id == 0)
		MM_REG_WAIT(cmd, RDMA0_FRAME_DONE);
	else if (ctx->comp->alias_id == 1)
		MM_REG_WAIT(cmd, RDMA1_FRAME_DONE);
	else
		pr_err("Do not support RDMA%d_DONE event\n", ctx->comp->alias_id);

	/* Disable RDMA */
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_RDMA_EN, 0x00000000,
		     0x00000001);
	return 0;
}

static const struct mdp_comp_ops rdma_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_rdma,
	.config_frame = config_rdma_frame,
	.config_subfrm = config_rdma_subfrm,
	/* .reconfig_frame = reconfig_rdma_frame, */
	/* .reconfig_subfrms = reconfig_rdma_subfrms, */
	.wait_comp_event = wait_rdma_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_rsz(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	/* Reset RSZ */
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_ENABLE, 0x00010000,
		     0x00010000);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_ENABLE, 0x00000000,
		     0x00010000);
	/* Enable RSZ */
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_ENABLE, 0x00000001,
		     0x00000001);
	return 0;
}

static int config_rsz_frame(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd,
			    const struct v4l2_rect *compose)
{
	const struct mdp_rsz_data *rsz = &ctx->param->rsz;
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

#ifdef RSZ_ETC_CONTROL
	MM_REG_WRITE_S(cmd, subsys_id, base, RSZ_ETC_CONTROL, 0,
		     0xFFFFFFFF);
#endif
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CONTROL_1, rsz->control1,
		     0x03FFFDF3);
	if (ctx->param->frame.bypass) {
		/* Disable RSZ */
		MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_ENABLE, 0x00000000,
			     0x00000001);
		return 0;
	}

	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CONTROL_1, rsz->control1,
		     0x07FF01F3);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CONTROL_2, rsz->control2,
		     0x0FFFC290);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_HORIZONTAL_COEFF_STEP,
		     rsz->coeff_step_x, 0x007FFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_VERTICAL_COEFF_STEP,
		     rsz->coeff_step_y, 0x007FFFFF);
	return 0;
}

static int config_rsz_subfrm(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_rsz_subfrm *subfrm = &ctx->param->rsz.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CONTROL_2, subfrm->control2,
		     0x00003800);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_INPUT_IMAGE, subfrm->src,
		     0xFFFFFFFF);
#if RSZ_DISABLE_DCM_SMALL_TILE
	if ((csf->in.right - csf->in.left + 1) <= 16)
		MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CONTROL_1, 1 << 27,
			     1 << 27);
#endif
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_LUMA_HORIZONTAL_INTEGER_OFFSET,
		     subfrm->luma_h_int_ofst, 0x0000FFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET,
		     subfrm->luma_h_sub_ofst, 0x001FFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_LUMA_VERTICAL_INTEGER_OFFSET,
		     subfrm->luma_v_int_ofst, 0x0000FFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_LUMA_VERTICAL_SUBPIXEL_OFFSET,
		     subfrm->luma_v_int_ofst, 0x001FFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CHROMA_HORIZONTAL_INTEGER_OFFSET,
		     subfrm->chroma_h_int_ofst, 0x001FFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET,
		     subfrm->chroma_h_sub_ofst, 0x001FFFFF);

	MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_OUTPUT_IMAGE, subfrm->clip,
		     0xFFFFFFFF);

	return 0;
}

static int advance_rsz_subfrm(struct mdp_comp_ctx *ctx,
			      struct mdp_cmd *cmd, u32 index)
{
#if RSZ_DISABLE_DCM_SMALL_TILE
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	if ((csf->in.right - csf->in.left + 1) <= 16)
		MM_REG_WRITE_S(cmd, subsys_id, base, PRZ_CONTROL_1, 0, 1 << 27);
#endif
	return 0;
}

static const struct mdp_comp_ops rsz_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_rsz,
	.config_frame = config_rsz_frame,
	.config_subfrm = config_rsz_subfrm,
	/* .reconfig_frame = NULL, */
	/* .reconfig_subfrms = NULL, */
	.wait_comp_event = NULL,
	.advance_subfrm = advance_rsz_subfrm,
	.post_process = NULL,
};

static int init_aal(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	/* Always set MDP_AAL enable to 1 */
    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_EN, 0x1, 0x1);

	return 0;
}

static int config_aal_frame(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd,
			    const struct v4l2_rect *compose)
{
	const struct mdp_aal_data *aal = &ctx->param->aal;
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_CFG, 0x1, 0x1);
	/* 10 bit format */
	if (aal->format10bits)
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_CFG_MAIN, 0 << 7, 0x80);
	else
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_CFG_MAIN, 1 << 7, 0x80);

	return 0;
}

static int config_aal_subfrm(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_aal_subfrm *subfrm = &ctx->param->aal.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_SIZE,
			subfrm->src, 0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_OUTPUT_OFFSET,
			subfrm->clip_ofst, 0x00FF00FF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_AAL_OUTPUT_SIZE,
			subfrm->clip, 0xFFFFFFFF);

	return 0;
}

static const struct mdp_comp_ops aal_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_aal,
	.config_frame = config_aal_frame,
	.config_subfrm = config_aal_subfrm,
	/* .reconfig_frame = NULL, */
	/* .reconfig_subfrms = NULL, */
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_hdr(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	// Always set MDP_HDR enable to 1
    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_TOP, 1, 0x1);
    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_RELAY, 1, 0x1);

	return 0;
}

static int config_hdr_subfrm(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_hdr_subfrm *subfrm = &ctx->param->hdr.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_TILE_POS,
			subfrm->win_size, MDP_HDR_TILE_POS_MASK);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_SIZE_0,
			subfrm->src, 0x1FFF1FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_SIZE_1,
			subfrm->clip_ofst0, 0x1FFF1FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_SIZE_2,
			subfrm->clip_ofst1, 0x1FFF1FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_HIST_CTRL_0,
			subfrm->hist_ctrl_0, 0x00001FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_HIST_CTRL_1,
			subfrm->hist_ctrl_1, 0x00001FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_TOP,
			subfrm->hdr_top, 0x00000060);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HDR_HIST_ADDR,
			subfrm->hist_addr, 0x00000200);

	return 0;
}

static const struct mdp_comp_ops hdr_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_hdr,
	.config_frame = NULL,
	.config_subfrm = config_hdr_subfrm,
	/* .reconfig_frame = NULL, */
	/* .reconfig_subfrms = NULL, */
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_tdshp(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_TDSHP_CTRL, 0x00000001,
            0x00000001);
    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HFG_CTRL, 0x00000000,
            0x00000303);
    MM_REG_WRITE_S(cmd, subsys_id, base, MDP_TDSHP_CFG, 0x00000001,
            0x00000001);

	return 0;
}

static int config_tdshp_subfrm(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_tdshp_subfrm *subfrm = &ctx->param->tdshp.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_TDSHP_INPUT_SIZE,
			subfrm->src, 0xFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_TDSHP_OUTPUT_OFFSET,
			subfrm->clip_ofst, 0xFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_TDSHP_OUTPUT_SIZE,
			subfrm->clip, 0xFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HIST_CFG_00,
			subfrm->hist_cfg_0, 0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, MDP_HIST_CFG_00,
			subfrm->hist_cfg_1, 0xFFFFFFFF);

	return 0;
}

static const struct mdp_comp_ops tdshp_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_tdshp,
	.config_frame = NULL,
	.config_subfrm = config_tdshp_subfrm,
	/* .reconfig_frame = NULL, */
	/* .reconfig_subfrms = NULL, */
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_color(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, DISP_COLOR_START, 0x3, 0x3);
	MM_REG_WRITE_S(cmd, subsys_id, base, DISP_COLOR_CFG_MAIN, 0x7, 0x7);
	MM_REG_WRITE_S(cmd, subsys_id, base, DISP_COLOR_CM1_EN, 0x0, 0x1);
	MM_REG_WRITE_S(cmd, subsys_id, base, DISP_COLOR_CM2_EN, 0x0, 0x1);

	return 0;
}

static int config_color_subfrm(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_color_subfrm *subfrm = &ctx->param->color.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_S(cmd, subsys_id, base, DISP_COLOR_INTERNAL_IP_WIDTH,
			subfrm->src>>16, 0x3FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, DISP_COLOR_INTERNAL_IP_HEIGHT,
			subfrm->src&0xFFFF, 0x3FFF);

	return 0;
}

static const struct mdp_comp_ops color_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_color,
	.config_frame = NULL,
	.config_subfrm = config_color_subfrm,
	/* .reconfig_frame = NULL, */
	/* .reconfig_subfrms = NULL, */
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_wrot(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

#if WROT_FILTER_CONSTRAINT
	/* Wait WROT SRAM shared to DISP RDMA */
	if (ctx->comp->alias_id == 0)
		pr_err("Do not support WROT0_SRAM_READY event\n");
	else
		pr_err("Do not support WROT1_SRAM_READY event\n");
#endif
	/* Reset WROT */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_SOFT_RST, 0x01, 0x00000001);
	MM_REG_POLL_S(cmd, subsys_id, base, VIDO_SOFT_RST_STAT, 0x01,
		    0x00000001);
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_SOFT_RST, 0x00, 0x00000001);
	MM_REG_POLL_S(cmd, subsys_id, base, VIDO_SOFT_RST_STAT, 0x00,
		    0x00000001);
	return 0;
}

static int config_wrot_frame(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd,
			     const struct v4l2_rect *compose)
{
	const struct mdp_wrot_data *wrot = &ctx->param->wrot;
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	/* Write frame base address */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_BASE_ADDR, wrot->iova[0],
		     0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_BASE_ADDR_C, wrot->iova[1],
		     0xFFFFFFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_BASE_ADDR_V, wrot->iova[2],
		     0xFFFFFFFF);
	/* Write frame related registers */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_CTRL, wrot->control,
		     0xF131510F);
	/* Write frame Y pitch */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_STRIDE, wrot->stride[0],
		     0x0000FFFF);
	/* Write frame UV pitch */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_STRIDE_C, wrot->stride[1],
		     0x0000FFFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_STRIDE_V, wrot->stride[2],
		     0x0000FFFF);
	/* Write matrix control */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_MAT_CTRL, wrot->mat_ctrl,
		     0x000000F3);

	/* Set the fixed ALPHA as 0xFF */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_DITHER, 0xFF000000,
		     0xFF000000);
	/* Set VIDO_EOL_SEL */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_RSV_1, 0x80000000,
		     0x80000000);
	/* Set VIDO_FIFO_TEST */
	if (wrot->fifo_test != 0)
		MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_FIFO_TEST,
			     wrot->fifo_test, 0x00000FFF);

#if WROT_FILTER_CONSTRAINT
	/* Filter enable */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_MAIN_BUF_SIZE, wrot->filter,
		     0x00000077);
#endif

	return 0;
}

static int config_wrot_subfrm(struct mdp_comp_ctx *ctx,
			      struct mdp_cmd *cmd, u32 index)
{
	const struct mdp_wrot_subfrm *subfrm = &ctx->param->wrot.subfrms[index];
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	/* Write Y pixel offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_OFST_ADDR,
		     subfrm->offset[0], 0x0FFFFFFF);
	/* Write U pixel offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_OFST_ADDR_C,
		     subfrm->offset[1], 0x0FFFFFFF);
	/* Write V pixel offset */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_OFST_ADDR_V,
		     subfrm->offset[2], 0x0FFFFFFF);
	/* Write source size */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_IN_SIZE, subfrm->src,
		     0x1FFF1FFF);
	/* Write target size */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_TAR_SIZE, subfrm->clip,
		     0x1FFF1FFF);
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_CROP_OFST, subfrm->clip_ofst,
		     0x1FFF1FFF);

	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_MAIN_BUF_SIZE,
		     subfrm->main_buf, 0x1FFF7F00);

	/* Enable WROT */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_ROT_EN, 0x01, 0x00000001);

	return 0;
}

static int wait_wrot_event(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	if (ctx->comp->alias_id == 0)
		MM_REG_WAIT(cmd, WROT0_FRAME_DONE);
	else if (ctx->comp->alias_id == 1)
		MM_REG_WAIT(cmd, WROT1_FRAME_DONE);
	else
		pr_err("Do not support WROT%d_DONE event\n", ctx->comp->alias_id);
#if WROT_FILTER_CONSTRAINT
	/* Filter disable */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_MAIN_BUF_SIZE,
		     (0 << 4) +
		(0 << 0),
		0x00000077);
#endif
	/* Disable WROT */
	MM_REG_WRITE_S(cmd, subsys_id, base, VIDO_ROT_EN, 0x00, 0x00000001);

	return 0;
}

static const struct mdp_comp_ops wrot_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_wrot,
	.config_frame = config_wrot_frame,
	.config_subfrm = config_wrot_subfrm,
	/* .reconfig_frame = reconfig_wrot_frame, */
	/* .reconfig_subfrms = reconfig_wrot_subfrms, */
	.wait_comp_event = wait_wrot_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int init_isp(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	const struct isp_data *isp = &ctx->param->isp;
	phys_addr_t mmsys = ctx->comp->mdp_dev->mmsys.reg_base;
	u16 subsys_id = ctx->comp->mdp_dev->mmsys.subsys_id;

	/* Direct link */
	if (isp->dl_flags & (1 << MDP_CAMIN)) {
		mdp_dbg(2, "SW_RST ASYNC");
		/* Reset MDP_DL_ASYNC_TX */
		/* Bit  3: MDP_DL_ASYNC_TX / MDP_RELAY */
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW0_RST_B, 0x0,
			     0x00000008);
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW0_RST_B, 1 << 3,
			     0x00000008);
		/* Reset MDP_DL_ASYNC_RX */
		/* Bit  10: MDP_DL_ASYNC_RX */
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW1_RST_B, 0x0,
			     0x00000400);
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW1_RST_B, 1 << 10,
			     0x00000400);

		/* Enable sof mode */
		//MM_REG_WRITE_S(cmd, subsys_id, mmsys, ISP_RELAY_CFG_WD, 0 << 31,
		//	     0x80000000);
	}

	if (isp->dl_flags & (1 << MDP_CAMIN2)) {
		mdp_dbg(2, "SW_RST ASYNC2");
		/* Reset MDP_DL_ASYNC2_TX */
		/* Bit  4: MDP_DL_ASYNC2_TX / MDP_RELAY2 */
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW0_RST_B, 0x0,
			     0x00000010);
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW0_RST_B, 1 << 4,
			     0x00000010);
		/* Reset MDP_DL_ASYNC2_RX */
		/* Bit  11: MDP_DL_ASYNC2_RX */
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW1_RST_B, 0x0,
			     0x00000800);
		MM_REG_WRITE_S(cmd, subsys_id, mmsys, MMSYS_SW1_RST_B, 1 << 11,
			     0x00000800);

		/* Enable sof mode */
		//MM_REG_WRITE_S(cmd, subsys_id, mmsys, IPU_RELAY_CFG_WD, 0 << 31,
		//	     0x80000000);
	}

	return 0;
}

static int config_isp_frame(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd,
			    const struct v4l2_rect *compose)
{
	const struct isp_data *isp = &ctx->param->isp;
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

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
	case ISP_DRV_DIP_CQ_THRE12:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2298,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE13:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x22A4,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	case ISP_DRV_DIP_CQ_THRE14:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x22B0,
				  isp->cq_iova, 0xFFFFFFFF);
		break;
	/* From CQ15 to CQ18, these do not connect to GCE */
	default:
		mdp_err("Do not support this cq (%d)", isp->cq_idx);
		return -EINVAL;
	}

	return 0;
}

static int config_isp_subfrm(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index)
{
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2304,
			  ctx->param->isp.tpipe_iova[index], 0xFFFFFFFF);
	return 0;
}

static int wait_isp_event(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd)
{
	const struct isp_data *isp = &ctx->param->isp;
	phys_addr_t base = ctx->comp->reg_base;
	u16 subsys_id = ctx->comp->subsys_id;

	/* MDP_DL_SEL: select MDP_CROP */
	if (isp->dl_flags & (1 << MDP_CAMIN))
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x0030, 0x00000000,
				  0x00000200);
	/* MDP2_DL_SEL: select MDP_CROP2 */
	if (isp->dl_flags & (1 << MDP_CAMIN2))
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x0030, 0x00000000,
				  0x00000C00);

	switch (isp->cq_idx) {
	case ISP_DRV_DIP_CQ_THRE0:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0001,
				  0x00000001);
		//MM_REG_WAIT(cmd, ISP_P2_0_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE1:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0002,
				  0x00000002);
		//MM_REG_WAIT(cmd, ISP_P2_1_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE2:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0004,
				  0x00000004);
		//MM_REG_WAIT(cmd, ISP_P2_2_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE3:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0008,
				  0x00000008);
		//MM_REG_WAIT(cmd, ISP_P2_3_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE4:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0010,
				  0x00000010);
		//MM_REG_WAIT(cmd, ISP_P2_4_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE5:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0020,
				  0x00000020);
		//MM_REG_WAIT(cmd, ISP_P2_5_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE6:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0040,
				  0x00000040);
		///MM_REG_WAIT(cmd, ISP_P2_6_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE7:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0080,
				  0x00000080);
		//MM_REG_WAIT(cmd, ISP_P2_7_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE8:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0100,
				  0x00000100);
		//MM_REG_WAIT(cmd, ISP_P2_8_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE9:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0200,
				  0x00000200);
		//MM_REG_WAIT(cmd, ISP_P2_9_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE10:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0400,
				  0x00000400);
		//MM_REG_WAIT(cmd, ISP_P2_10_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE11:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x0800,
				  0x00000800);
		//MM_REG_WAIT(cmd, ISP_P2_11_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE12:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x1000,
				  0x00001000);
		//MM_REG_WAIT(cmd, ISP_P2_12_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE13:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x2000,
				  0x00002000);
		//MM_REG_WAIT(cmd, ISP_P2_13_DONE);
		break;
	case ISP_DRV_DIP_CQ_THRE14:
		MM_REG_WRITE_MASK(cmd, subsys_id, base, 0x2000, 0x4000,
				  0x00004000);
		//MM_REG_WAIT(cmd, ISP_P2_14_DONE);
		break;
	/* From CQ15 to CQ18, these do not connect to GCE */
	default:
		mdp_err("Do not support this cq (%d)", isp->cq_idx);
		return -EINVAL;
	}

	return 0;
}

static const struct mdp_comp_ops imgi_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = init_isp,
	.config_frame = config_isp_frame,
	.config_subfrm = config_isp_subfrm,
	/* .reconfig_frame = reconfig_isp_frame, */
	/* .reconfig_subfrms = reconfig_isp_subfrms, */
	.wait_comp_event = wait_isp_event,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static int config_camin_subfrm(struct mdp_comp_ctx *ctx,
			       struct mdp_cmd *cmd, u32 index)
{
	const struct img_comp_subfrm *csf = &ctx->param->subfrms[index];
	//phys_addr_t base = ctx->comp->reg_base;
	//u8 subsys_id = ctx->comp->subsys_id;
	u32 isp_dl_w, isp_dl_h;

	isp_dl_w = csf->in.right - csf->in.left + 1;
	isp_dl_h = csf->in.bottom - csf->in.top + 1;

	/* Config for direct link */
	if (ctx->comp->alias_id == 0) {
/*#ifdef MDP_ASYNC_CFG_WD
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_ASYNC_CFG_WD,
			     (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
#ifdef ISP_RELAY_CFG_WD
		MM_REG_WRITE_S(cmd, subsys_id, base, ISP_RELAY_CFG_WD,
			     (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
	} else {
#ifdef MDP_ASYNC_IPU_CFG_WD
		MM_REG_WRITE_S(cmd, subsys_id, base, MDP_ASYNC_IPU_CFG_WD,
			     (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
#ifdef IPU_RELAY_CFG_WD
		MM_REG_WRITE_S(cmd, subsys_id, base, IPU_RELAY_CFG_WD,
			     (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif*/
	}

	return 0;
}

static const struct mdp_comp_ops camin_ops = {
	.get_comp_flag = get_comp_flag,
	.init_comp = NULL,
	.config_frame = NULL,
	.config_subfrm = config_camin_subfrm,
	/* .reconfig_frame = NULL, */
	/* .reconfig_subfrms = NULL, */
	.wait_comp_event = NULL,
	.advance_subfrm = NULL,
	.post_process = NULL,
};

static const struct mdp_comp_ops *mdp_comp_ops[MDP_COMP_TYPE_COUNT] = {
	[MDP_COMP_TYPE_RDMA] =		&rdma_ops,
	[MDP_COMP_TYPE_RSZ] =		&rsz_ops,
	[MDP_COMP_TYPE_AAL] =		&aal_ops,
	[MDP_COMP_TYPE_HDR] =		&hdr_ops,
	[MDP_COMP_TYPE_TDSHP] =	&tdshp_ops,
	[MDP_COMP_TYPE_COLOR] =	&color_ops,
	[MDP_COMP_TYPE_WROT] =		&wrot_ops,
	[MDP_COMP_TYPE_WDMA] =		NULL,
	[MDP_COMP_TYPE_PATH] =		NULL,
	[MDP_COMP_TYPE_CCORR] =	NULL,
	[MDP_COMP_TYPE_IMGI] =		&imgi_ops,
	[MDP_COMP_TYPE_EXTO] =		NULL,
	[MDP_COMP_TYPE_DL_PATH] =	&camin_ops,
};

struct mdp_comp_match {
	enum mdp_comp_type	type;
	u32			alias_id;
};

static const struct mdp_comp_match mdp_comp_matches[MDP_MAX_COMP_COUNT] = {
	[MDP_COMP_WPEI] =	{ MDP_COMP_TYPE_WPEI, 0 },
	[MDP_COMP_WPEO] =	{ MDP_COMP_TYPE_EXTO, 2 },
	[MDP_COMP_WPEI2] =	{ MDP_COMP_TYPE_WPEI, 1 },
	[MDP_COMP_WPEO2] =	{ MDP_COMP_TYPE_EXTO, 3 },
	[MDP_COMP_ISP_IMGI] =	{ MDP_COMP_TYPE_IMGI, 0 },
	[MDP_COMP_ISP_IMG2O] =	{ MDP_COMP_TYPE_EXTO, 1 },
	[MDP_COMP_CAMIN] =	{ MDP_COMP_TYPE_DL_PATH, 0 },
	[MDP_COMP_CAMIN2] =	{ MDP_COMP_TYPE_DL_PATH, 1 },
	[MDP_COMP_RDMA0] =	{ MDP_COMP_TYPE_RDMA, 0 },
	[MDP_COMP_RDMA1] =	{ MDP_COMP_TYPE_RDMA, 1 },
	[MDP_COMP_HDR0] =	{ MDP_COMP_TYPE_HDR, 0 },
	[MDP_COMP_HDR1] =	{ MDP_COMP_TYPE_HDR, 1 },
	[MDP_COMP_COLOR0] =	{ MDP_COMP_TYPE_COLOR, 0 },
	[MDP_COMP_COLOR1] =	{ MDP_COMP_TYPE_COLOR, 1 },
	[MDP_COMP_AAL0] =	{ MDP_COMP_TYPE_AAL, 0 },
	[MDP_COMP_AAL1] =	{ MDP_COMP_TYPE_AAL, 1 },
	[MDP_COMP_RSZ0] =	{ MDP_COMP_TYPE_RSZ, 0 },
	[MDP_COMP_RSZ1] =	{ MDP_COMP_TYPE_RSZ, 1 },
	[MDP_COMP_TDSHP0] =	{ MDP_COMP_TYPE_TDSHP, 0 },
	[MDP_COMP_TDSHP1] =	{ MDP_COMP_TYPE_TDSHP, 1 },
	[MDP_COMP_WROT0] =	{ MDP_COMP_TYPE_WROT, 0 },
	[MDP_COMP_WROT1] =	{ MDP_COMP_TYPE_WROT, 1 },
};

static const char * const gce_event_names[MDP_MAX_EVENT_COUNT] = {
    [RDMA0_SOF]				= "RDMA0_SOF",
    [RDMA1_SOF]				= "RDMA1_SOF",
    [AAL0_SOF]				= "AAL0_SOF",
    [AAL1_SOF]				= "AAL1_SOF",
    [HDR0_SOF]				= "HDR0_SOF",
    [HDR1_SOF]				= "HDR1_SOF",
    [RSZ0_SOF]				= "RSZ0_SOF",
    [RSZ1_SOF]				= "RSZ1_SOF",
    [WROT0_SOF]				= "WROT0_SOF",
    [WROT1_SOF]				= "WROT1_SOF",
    [TDSHP0_SOF]			= "TDSHP0_SOF",
    [TDSHP1_SOF]			= "TDSHP1_SOF",
    [DL_RELAY0_SOF]		= "DL_RELAY0_SOF",
    [DL_RELAY1_SOF]		= "DL_RELAY1_SOF",
    [COLOR0_SOF]			= "COLOR0_SOF",
    [COLOR1_SOF]			= "COLOR1_SOF",
    [WROT1_FRAME_DONE]		= "WROT1_FRAME_DONE",
    [WROT0_FRAME_DONE]		= "WROT0_FRAME_DONE",
    [TDSHP1_FRAME_DONE]	= "TDSHP1_FRAME_DONE",
    [TDSHP0_FRAME_DONE]	= "TDSHP0_FRAME_DONE",
    [RSZ1_FRAME_DONE]		= "RSZ1_FRAME_DONE",
    [RSZ0_FRAME_DONE]		= "RSZ0_FRAME_DONE",
    [RDMA1_FRAME_DONE]		= "RDMA1_FRAME_DONE",
    [RDMA0_FRAME_DONE]		= "RDMA0_FRAME_DONE",
    [HDR1_FRAME_DONE]		= "HDR1_FRAME_DONE",
    [HDR0_FRAME_DONE]		= "HDR0_FRAME_DONE",
    [COLOR1_FRAME_DONE]	= "COLOR1_FRAME_DONE",
    [COLOR0_FRAME_DONE]	= "COLOR0_FRAME_DONE",
    [AAL1_FRAME_DONE]		= "AAL1_FRAME_DONE",
    [AAL0_FRAME_DONE]		= "AAL0_FRAME_DONE",
    [STREAM_DONE_0]		= "STREAM_DONE_0",
    [STREAM_DONE_1]		= "STREAM_DONE_1",
    [STREAM_DONE_2]		= "STREAM_DONE_2",
    [STREAM_DONE_3]		= "STREAM_DONE_3",
    [STREAM_DONE_4]		= "STREAM_DONE_4",
    [STREAM_DONE_5]		= "STREAM_DONE_5",
    [STREAM_DONE_6]		= "STREAM_DONE_6",
    [STREAM_DONE_7]		= "STREAM_DONE_7",
    [STREAM_DONE_8]		= "STREAM_DONE_8",
    [STREAM_DONE_9]		= "STREAM_DONE_9",
    [STREAM_DONE_10]		= "STREAM_DONE_10",
    [STREAM_DONE_11]		= "STREAM_DONE_11",
    [STREAM_DONE_12]		= "STREAM_DONE_12",
    [STREAM_DONE_13]		= "STREAM_DONE_13",
    [STREAM_DONE_14]		= "STREAM_DONE_14",
    [STREAM_DONE_15]		= "STREAM_DONE_15",
    [WROT1_SW_RST_DONE]	= "WROT1_SW_RST_DONE",
    [WROT0_SW_RST_DONE]	= "WROT0_SW_RST_DONE",
    [RDMA1_SW_RST_DONE]	= "RDMA1_SW_RST_DONE",
    [RDMA0_SW_RST_DONE]	= "RDMA0_SW_RST_DONE",
};

static const struct of_device_id mdp_comp_dt_ids[] = {
	{
		.compatible = "mediatek,mt8192-mdp-rdma",
		.data = (void *)MDP_COMP_TYPE_RDMA,
	}, {
		.compatible = "mediatek,mt8192-mdp-color",
		.data = (void *)MDP_COMP_TYPE_COLOR,
	}, {
	    .compatible = "mediatek,mt8192-mdp-aal",
		.data = (void *)MDP_COMP_TYPE_AAL,
	}, {
	    .compatible = "mediatek,mt8192-mdp-hdr",
		.data = (void *)MDP_COMP_TYPE_HDR,
	}, {
		.compatible = "mediatek,mt8192-mdp-rsz",
		.data = (void *)MDP_COMP_TYPE_RSZ,
	}, {
		.compatible = "mediatek,mt8192-mdp-wrot",
		.data = (void *)MDP_COMP_TYPE_WROT,
	}, {
		.compatible = "mediatek,mt8192-mdp-tdshp",
		.data = (void *)MDP_COMP_TYPE_TDSHP,
	}, {
		.compatible = "mediatek,mt8192-mdp-dl",
		.data = (void *)MDP_COMP_TYPE_DL_PATH,
	},
	{}
};

static int mdp_comp_get_id(struct device *dev, struct device_node *node,
			   enum mdp_comp_type type)
{
	u32 alias_id;
	int i, ret;

	ret = of_property_read_u32(node, "mediatek,mdp-id", &alias_id);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(mdp_comp_matches); i++)
		if (mdp_comp_matches[i].type == type &&
		    mdp_comp_matches[i].alias_id == alias_id)
			return i;

	dev_err(dev, "Failed to get id. type: %d, alias: %d\n", type, alias_id);
	return -EINVAL;
}

void mdp_comp_clock_on(struct device *dev, struct mdp_comp *comp)
{
	int i, err;

	if (comp->larb_dev) {
	    err = pm_runtime_get_sync(comp->larb_dev);
	    if (err < 0)
			dev_err(dev,
				"Failed to try get larb once, err %d. type:%d id:%d\n",
				err, comp->type, comp->id);
		err = pm_runtime_get_sync(comp->larb_dev);
		if (err < 0)
			dev_err(dev,
				"Failed to get larb, err %d. type:%d id:%d\n",
				err, comp->type, comp->id);
	}

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		err = clk_prepare_enable(comp->clks[i]);
		if (err)
			dev_err(dev,
				"Failed to enable clock %d, err %d. type:%d id:%d\n",
				i, err, comp->type, comp->id);
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

	if (comp->larb_dev)
		pm_runtime_put(comp->larb_dev);
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

static int mdp_get_subsys_id(struct device *dev, struct device_node *node,
			     struct mdp_comp *comp)
{
	struct platform_device *comp_pdev;
	struct cmdq_client_reg  *cmdq_reg;
	int ret = 0;

	if (!dev || !node || !comp)
		return -EINVAL;

	comp_pdev = of_find_device_by_node(node);

	if (!comp_pdev) {
		dev_err(dev, "get comp_pdev fail! comp id=%d type=%d\n",
			comp->id, comp->type);
		return -ENODEV;
	}

	cmdq_reg = kzalloc(sizeof(*cmdq_reg), GFP_KERNEL);
	if (!cmdq_reg)
		return -ENOMEM;

	ret = cmdq_dev_get_client_reg(&comp_pdev->dev, cmdq_reg, 0);
	if (ret != 0) {
		dev_err(&comp_pdev->dev,
			"cmdq_dev_get_subsys fail!\n");
		kfree(cmdq_reg);
		return -EINVAL;
	}

	comp->subsys_id = cmdq_reg->subsys;
	dev_err(&comp_pdev->dev, "subsys id=%d\n", cmdq_reg->subsys);

	kfree(cmdq_reg);

	return 0;
}

static void __mdp_comp_init(struct mdp_dev *mdp, struct device_node *node,
			    struct mdp_comp *comp)
{
	struct resource res;
	phys_addr_t base;
	int i;

	if (of_address_to_resource(node, 0, &res) < 0)
		base = 0L;
	else
		base = 0L | res.start;

	comp->mdp_dev = mdp;
	/* comp->dev_node = of_node_get(node); */
	comp->regs = of_iomap(node, 0);
	comp->reg_base = base;
    for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
        comp->clks[i] = of_clk_get(node, i);
        if (IS_ERR(comp->clks[i]))
            break;
    }
}

static int mdp_mm_init(struct mdp_dev *mdp,
		       struct mdp_comp *comp, const char *ref_name)
{
	struct device_node *node;
	struct device *dev = &mdp->pdev->dev;

	node = of_parse_phandle(dev->of_node, ref_name, 0);
	if (!node) {
		dev_err(dev, "Failed to parse dt %s\n", ref_name);
		return -EINVAL;
	}

	__mdp_comp_init(mdp, node, comp);
	mdp_get_subsys_id(dev, node, comp);
	of_node_put(node);
	if (!comp->reg_base) {
		dev_err(dev, "Failed to init %s base\n", ref_name);
		return -EINVAL;
	}
	return 0;
}

static int mdp_comp_init(struct device *dev, struct mdp_dev *mdp,
			 struct device_node *node, struct mdp_comp *comp,
			 enum mdp_comp_id id)
{
	struct device_node *larb_node;
	struct platform_device *larb_pdev;
	int i;

	if (id < 0 || id >= MDP_MAX_COMP_COUNT) {
		dev_err(dev, "Invalid component id %d\n", id);
		return -EINVAL;
	}

	__mdp_comp_init(mdp, node, comp);
	comp->type = mdp_comp_matches[id].type;
	comp->id = id;
	comp->alias_id = mdp_comp_matches[id].alias_id;
	comp->ops = mdp_comp_ops[comp->type];

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		comp->clks[i] = of_clk_get(node, i);
		if (IS_ERR(comp->clks[i]))
			break;
	}

	mdp_get_subsys_id(dev, node, comp);

    /* Only DMA capable components need the LARB property */
    comp->larb_dev = NULL;
    if (comp->type != MDP_COMP_TYPE_RDMA &&
        comp->type != MDP_COMP_TYPE_WROT &&
        comp->type != MDP_COMP_TYPE_WDMA)
        return 0;

    larb_node = of_parse_phandle(node, "mediatek,larb", 0);
    if (!larb_node) {
        dev_err(dev, "Missing mediatek,larb phandle in %pOF node\n",
            node);
        return -EINVAL;
    }

    larb_pdev = of_find_device_by_node(larb_node);
    if (!larb_pdev) {
        dev_warn(dev, "Waiting for larb device %pOF\n", larb_node);
        of_node_put(larb_node);
        return -EPROBE_DEFER;
    }
    of_node_put(larb_node);

    comp->larb_dev = &larb_pdev->dev;

	return 0;
}

static void mdp_comp_deinit(struct mdp_comp *comp)
{
	iounmap(comp->regs);
	/* of_node_put(comp->dev_node); */
}

void mdp_component_deinit(struct mdp_dev *mdp)
{
	int i;

	mdp_comp_deinit(&mdp->mmsys);
	mdp_comp_deinit(&mdp->mm_mutex);
	for (i = 0; i < ARRAY_SIZE(mdp->comp); i++) {
		if (mdp->comp[i]) {
			mdp_comp_deinit(mdp->comp[i]);
			kfree(mdp->comp[i]);
		}
	}
}

int mdp_component_init(struct mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;
	struct device_node *node, *parent;
	int i, ret;

	for (i = RDMA0_SOF; i < MDP_MAX_EVENT_COUNT; i++) {
		s32 event_id;

		if (!dev)
			return -EINVAL;
		if (of_property_read_u32_index(dev->of_node,
					       "mediatek,gce-events",
					       i, &event_id)) {
			dev_err(dev, "can't parse gce-events property");

			return -ENODEV;
		}
		mdp->event[i] = (event_id < 0) ? -i : event_id;
		dev_info(dev, "Get event %s id:%d\n",
			 gce_event_names[i], mdp->event[i]);
	}

	ret = mdp_mm_init(mdp, &mdp->mmsys, "mediatek,mmsys");
	if (ret)
		goto err_init_mm;

	ret = mdp_mm_init(mdp, &mdp->mm_mutex, "mediatek,mm-mutex");
	if (ret)
		goto err_init_mm;

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
			dev_info(dev, "Skipping disabled component %pOF\n",
				 node);
			continue;
		}

		type = (enum mdp_comp_type)of_id->data;
		id = mdp_comp_get_id(dev, node, type);
		if (id < 0) {
			dev_warn(dev, "Skipping unknown component %pOF\n",
				 node);
			continue;
		}

		comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
		if (!comp) {
			ret = -ENOMEM;
			goto err_init_comps;
		}
		mdp->comp[id] = comp;

		ret = mdp_comp_init(dev, mdp, node, comp, id);
		if (ret)
			goto err_init_comps;

		dev_info(dev, "%s type:%d alias:%d id:%d base:%#x regs:%p\n",
			 of_id->compatible, type, comp->alias_id, id,
			(u32)comp->reg_base, comp->regs);
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
	int i;

	if (param->type < 0 || param->type >= MDP_MAX_COMP_COUNT) {
		mdp_err("Invalid component id %d", param->type);
		return -EINVAL;
	}

	ctx->comp = mdp->comp[param->type];
	if (!ctx->comp) {
		mdp_err("Uninit component id %d", param->type);
		return -EINVAL;
	}

	ctx->param = param;
	ctx->input = &frame->inputs[param->input];
	for (i = 0; i < param->num_outputs; i++)
		ctx->outputs[i] = &frame->outputs[param->outputs[i]];
	return 0;
}

