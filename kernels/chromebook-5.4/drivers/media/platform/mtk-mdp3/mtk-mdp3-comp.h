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
	MDP_COMP_TYPE_AAL,

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
	MDP_COMP_WPEO,			/* 1 */
	MDP_COMP_WPEI2,			/* 2 */
	MDP_COMP_WPEO2,			/* 3 */
	MDP_COMP_ISP_IMGI,		/* 4 */
	MDP_COMP_ISP_IMG2O,		/* 5 */

	/* MDP */
	MDP_COMP_CAMIN,		    /* 6 */
	MDP_COMP_CAMIN2,	    /* 7 */
	MDP_COMP_RDMA0,		    /* 8 */
	MDP_COMP_RDMA1,		    /* 9 */
	MDP_COMP_HDR0,		    /* 10 */
	MDP_COMP_HDR1,		    /* 11 */
	MDP_COMP_COLOR0,	    /* 12 */
	MDP_COMP_COLOR1,	    /* 13 */
	MDP_COMP_AAL0,          /* 14 */
	MDP_COMP_AAL1,          /* 15 */
	MDP_COMP_RSZ0,		    /* 16 */
	MDP_COMP_RSZ1,		    /* 17 */
	MDP_COMP_TDSHP0,	    /* 18 */
	MDP_COMP_TDSHP1,        /* 19 */
	MDP_COMP_WROT0,		    /* 20 */
	MDP_COMP_WROT1,		    /* 21 */

	/* Dummy Engine */
	MDP_COMP_RSZ2,
	MDP_COMP_WDMA,

	MDP_MAX_COMP_COUNT	/* ALWAYS keep at the end */
};

enum mdp_comp_event {
    RDMA0_SOF,
    RDMA1_SOF,
    AAL0_SOF,
    AAL1_SOF,
    HDR0_SOF,
    HDR1_SOF,
    RSZ0_SOF,
    RSZ1_SOF,
    WROT0_SOF,
    WROT1_SOF,
    TDSHP0_SOF,
    TDSHP1_SOF,
    DL_RELAY0_SOF,
    DL_RELAY1_SOF,
    COLOR0_SOF,
    COLOR1_SOF,    
    WROT1_FRAME_DONE,
    WROT0_FRAME_DONE,
    TDSHP1_FRAME_DONE,
    TDSHP0_FRAME_DONE,
    RSZ1_FRAME_DONE,
    RSZ0_FRAME_DONE,
	RDMA1_FRAME_DONE,
    RDMA0_FRAME_DONE,
    HDR1_FRAME_DONE,
    HDR0_FRAME_DONE,
    COLOR1_FRAME_DONE,
    COLOR0_FRAME_DONE,
    AAL1_FRAME_DONE,
    AAL0_FRAME_DONE,
    STREAM_DONE_0,
    STREAM_DONE_1,
    STREAM_DONE_2,
    STREAM_DONE_3,
    STREAM_DONE_4,
    STREAM_DONE_5,
    STREAM_DONE_6,
    STREAM_DONE_7,
    STREAM_DONE_8,
    STREAM_DONE_9,
    STREAM_DONE_10,
    STREAM_DONE_11,
    STREAM_DONE_12,
    STREAM_DONE_13,
    STREAM_DONE_14,
    STREAM_DONE_15,
    WROT1_SW_RST_DONE,
    WROT0_SW_RST_DONE,
    RDMA1_SW_RST_DONE,
    RDMA0_SW_RST_DONE,

	MDP_MAX_EVENT_COUNT	/* ALWAYS keep at the end */
};

struct mdp_comp_ops;

struct mdp_comp {
	struct mdp_dev			*mdp_dev;
	void __iomem			*regs;
	phys_addr_t			reg_base;
	u16				subsys_id;
	struct clk			*clks[4];
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
void mdp_comp_clocks_on(struct device *dev, struct mdp_comp *comps, int num);
void mdp_comp_clocks_off(struct device *dev, struct mdp_comp *comps, int num);
int mdp_comp_ctx_init(struct mdp_dev *mdp, struct mdp_comp_ctx *ctx,
		      const struct img_compparam *param,
	const struct img_ipi_frameparam *frame);

#endif  /* __MTK_MDP3_COMP_H__ */

