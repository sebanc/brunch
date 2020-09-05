/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_COMP_H__
#define __MTK_MDP3_COMP_H__

#include "mtk-mdp3-cmdq.h"

enum mdp_comp_type {
	MDP_COMP_TYPE_INVALID = 0,

	MDP_COMP_TYPE_RDMA,
	MDP_COMP_TYPE_RSZ,
	MDP_COMP_TYPE_WROT,
	MDP_COMP_TYPE_WDMA,
	MDP_COMP_TYPE_PATH,

	MDP_COMP_TYPE_TDSHP,
	MDP_COMP_TYPE_COLOR,
	MDP_COMP_TYPE_DRE,
	MDP_COMP_TYPE_CCORR,
	MDP_COMP_TYPE_HDR,

	MDP_COMP_TYPE_IMGI,
	MDP_COMP_TYPE_WPEI,
	MDP_COMP_TYPE_EXTO,	/* External path */
	MDP_COMP_TYPE_DL_PATH,	/* Direct-link path */

	MDP_COMP_TYPE_COUNT	/* ALWAYS keep at the end */
};

enum mdp_comp_id {
	MDP_COMP_NONE = -1,	/* Invalid engine */

	/* ISP */
	MDP_COMP_WPEI = 0,
	MDP_COMP_WPEO,		/* 1 */
	MDP_COMP_WPEI2,		/* 2 */
	MDP_COMP_WPEO2,		/* 3 */
	MDP_COMP_ISP_IMGI,	/* 4 */
	MDP_COMP_ISP_IMGO,	/* 5 */
	MDP_COMP_ISP_IMG2O,	/* 6 */

	/* IPU */
	MDP_COMP_IPUI,		/* 7 */
	MDP_COMP_IPUO,		/* 8 */

	/* MDP */
	MDP_COMP_CAMIN,		/* 9 */
	MDP_COMP_CAMIN2,	/* 10 */
	MDP_COMP_RDMA0,		/* 11 */
	MDP_COMP_AAL0,		/* 12 */
	MDP_COMP_CCORR0,	/* 13 */
	MDP_COMP_RSZ0,		/* 14 */
	MDP_COMP_RSZ1,		/* 15 */
	MDP_COMP_TDSHP0,	/* 16 */
	MDP_COMP_COLOR0,	/* 17 */
	MDP_COMP_PATH0_SOUT,	/* 18 */
	MDP_COMP_PATH1_SOUT,	/* 19 */
	MDP_COMP_WROT0,		/* 20 */
	MDP_COMP_WDMA,		/* 21 */

	/* Dummy Engine */
	MDP_COMP_RDMA1,		/* 22 */
	MDP_COMP_RSZ2,		/* 23 */
	MDP_COMP_TDSHP1,	/* 24 */
	MDP_COMP_WROT1,		/* 25 */

	MDP_MAX_COMP_COUNT	/* ALWAYS keep at the end */
};

enum mdp_comp_event {
	RDMA0_SOF,
	RDMA0_DONE,
	RSZ0_SOF,
	RSZ1_SOF,
	TDSHP0_SOF,
	WROT0_SOF,
	WROT0_DONE,
	WDMA0_SOF,
	WDMA0_DONE,

	ISP_P2_0_DONE,
	ISP_P2_1_DONE,
	ISP_P2_2_DONE,
	ISP_P2_3_DONE,
	ISP_P2_4_DONE,
	ISP_P2_5_DONE,
	ISP_P2_6_DONE,
	ISP_P2_7_DONE,
	ISP_P2_8_DONE,
	ISP_P2_9_DONE,
	ISP_P2_10_DONE,
	ISP_P2_11_DONE,
	ISP_P2_12_DONE,
	ISP_P2_13_DONE,
	ISP_P2_14_DONE,

	WPE_DONE,
	WPE_B_DONE,

	MDP_MAX_EVENT_COUNT	/* ALWAYS keep at the end */
};

struct mdp_comp_ops;

struct mdp_comp {
	struct mdp_dev			*mdp_dev;
	void __iomem			*regs;
	phys_addr_t			reg_base;
	u8				subsys_id;
	struct clk			*clks[2];
	struct device			*larb_dev;
	enum mdp_comp_type		type;
	enum mdp_comp_id		id;
	u32				alias_id;
	const struct mdp_comp_ops	*ops;
};

struct mdp_comp_ctx {
	struct mdp_comp			*comp;
	const struct img_compparam	*param;
	const struct img_input		*input;
	const struct img_output		*outputs[IMG_MAX_HW_OUTPUTS];
};

struct mdp_comp_ops {
	s64 (*get_comp_flag)(const struct mdp_comp_ctx *ctx);
	int (*init_comp)(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd);
	int (*config_frame)(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd,
			    const struct v4l2_rect *compose);
	int (*config_subfrm)(struct mdp_comp_ctx *ctx,
			     struct mdp_cmd *cmd, u32 index);
	int (*wait_comp_event)(struct mdp_comp_ctx *ctx,
			       struct mdp_cmd *cmd);
	int (*advance_subfrm)(struct mdp_comp_ctx *ctx,
			      struct mdp_cmd *cmd, u32 index);
	int (*post_process)(struct mdp_comp_ctx *ctx, struct mdp_cmd *cmd);
};

struct mdp_dev;

int mdp_component_init(struct mdp_dev *mdp);
void mdp_component_deinit(struct mdp_dev *mdp);
void mdp_comp_clock_on(struct device *dev, struct mdp_comp *comp);
void mdp_comp_clock_off(struct device *dev, struct mdp_comp *comp);
int mdp_comp_ctx_init(struct mdp_dev *mdp, struct mdp_comp_ctx *ctx,
		      const struct img_compparam *param,
	const struct img_ipi_frameparam *frame);

#endif  /* __MTK_MDP3_COMP_H__ */

