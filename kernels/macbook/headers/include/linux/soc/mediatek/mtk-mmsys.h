/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __MTK_MMSYS_H
#define __MTK_MMSYS_H

#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

enum mtk_ddp_comp_id;
enum mtk_mdp_comp_id;
struct device;

struct mmsys_cmdq_cmd {
	struct cmdq_pkt *pkt;
	s32 *event;
};

enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_CCORR,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_DITHER,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DP_INTF0,
	DDP_COMPONENT_DP_INTF1,
	DDP_COMPONENT_DSC0,
	DDP_COMPONENT_DSC1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_DSI2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_MERGE0,
	DDP_COMPONENT_MERGE1,
	DDP_COMPONENT_MERGE2,
	DDP_COMPONENT_MERGE3,
	DDP_COMPONENT_MERGE4,
	DDP_COMPONENT_MERGE5,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL_2L0,
	DDP_COMPONENT_OVL_2L1,
	DDP_COMPONENT_OVL_2L2,
	DDP_COMPONENT_OVL_ADAPTOR,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_RDMA4,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_ID_MAX,
};

enum mtk_mmsys_config_type {
	MMSYS_CONFIG_MERGE_ASYNC_WIDTH,
	MMSYS_CONFIG_MERGE_ASYNC_HEIGHT,
	MMSYS_CONFIG_HDR_BE_ASYNC_WIDTH,
	MMSYS_CONFIG_HDR_BE_ASYNC_HEIGHT,
	MMSYS_CONFIG_HDR_ALPHA_SEL,
	MMSYS_CONFIG_MIXER_IN_ALPHA_ODD,
	MMSYS_CONFIG_MIXER_IN_ALPHA_EVEN,
	MMSYS_CONFIG_MIXER_IN_CH_SWAP,
	MMSYS_CONFIG_MIXER_IN_MODE,
	MMSYS_CONFIG_MIXER_IN_BIWIDTH,
};

enum mtk_mdp_comp_id {
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
	MDP_COMP_SPLIT,           /* 11 */
	MDP_COMP_SPLIT2,          /* 12 */
	MDP_COMP_RDMA0,           /* 13 */
	MDP_COMP_RDMA1,           /* 14 */
	MDP_COMP_RDMA2,           /* 15 */
	MDP_COMP_RDMA3,           /* 16 */
	MDP_COMP_STITCH,          /* 17 */
	MDP_COMP_FG0,             /* 18 */
	MDP_COMP_FG1,             /* 19 */
	MDP_COMP_FG2,             /* 20 */
	MDP_COMP_FG3,             /* 21 */
	MDP_COMP_TO_SVPP2MOUT,    /* 22 */
	MDP_COMP_TO_SVPP3MOUT,    /* 23 */
	MDP_COMP_TO_WARP0MOUT,    /* 24 */
	MDP_COMP_TO_WARP1MOUT,    /* 25 */
	MDP_COMP_VPP0_SOUT,       /* 26 */
	MDP_COMP_VPP1_SOUT,       /* 27 */
	MDP_COMP_PQ0_SOUT,        /* 28 */
	MDP_COMP_PQ1_SOUT,        /* 29 */
	MDP_COMP_HDR0,            /* 30 */
	MDP_COMP_HDR1,            /* 31 */
	MDP_COMP_HDR2,            /* 32 */
	MDP_COMP_HDR3,            /* 33 */
	MDP_COMP_AAL0,            /* 34 */
	MDP_COMP_AAL1,            /* 35 */
	MDP_COMP_AAL2,            /* 36 */
	MDP_COMP_AAL3,            /* 37 */
	MDP_COMP_CCORR0,          /* 38 */
	MDP_COMP_RSZ0,            /* 39 */
	MDP_COMP_RSZ1,            /* 40 */
	MDP_COMP_RSZ2,            /* 41 */
	MDP_COMP_RSZ3,            /* 42 */
	MDP_COMP_TDSHP0,          /* 43 */
	MDP_COMP_TDSHP1,          /* 44 */
	MDP_COMP_TDSHP2,          /* 45 */
	MDP_COMP_TDSHP3,          /* 46 */
	MDP_COMP_COLOR0,          /* 47 */
	MDP_COMP_COLOR1,          /* 48 */
	MDP_COMP_COLOR2,          /* 49 */
	MDP_COMP_COLOR3,          /* 50 */
	MDP_COMP_OVL0,            /* 51 */
	MDP_COMP_OVL1,            /* 52 */
	MDP_COMP_PAD0,            /* 53 */
	MDP_COMP_PAD1,            /* 54 */
	MDP_COMP_PAD2,            /* 55 */
	MDP_COMP_PAD3,            /* 56 */
	MDP_COMP_TCC0,            /* 56 */
	MDP_COMP_TCC1,            /* 57 */
	MDP_COMP_WROT0,           /* 58 */
	MDP_COMP_WROT1,           /* 59 */
	MDP_COMP_WROT2,           /* 60 */
	MDP_COMP_WROT3,           /* 61 */
	MDP_COMP_WDMA,            /* 62 */
	MDP_COMP_MERGE2,          /* 63 */
	MDP_COMP_MERGE3,          /* 64 */
	MDP_COMP_PATH0_SOUT,      /* 65 */
	MDP_COMP_PATH1_SOUT,      /* 66 */
	MDP_COMP_VDO0DL0,         /* 67 */
	MDP_COMP_VDO1DL0,         /* 68 */
	MDP_COMP_VDO0DL1,         /* 69 */
	MDP_COMP_VDO1DL1,         /* 70 */

	MDP_MAX_COMP_COUNT	/* ALWAYS keep at the end */
};

enum mtk_mdp_pipe_id {
	MDP_PIPE_NONE = -1,
	MDP_PIPE_RDMA0,
	MDP_PIPE_IMGI,
	MDP_PIPE_WPEI,
	MDP_PIPE_WPEI2,
	MDP_PIPE_RDMA1,
	MDP_PIPE_RDMA2,
	MDP_PIPE_RDMA3,
	MDP_PIPE_SPLIT,
	MDP_PIPE_SPLIT2,
	MDP_PIPE_VPP0_SOUT,
	MDP_PIPE_VPP1_SOUT,
	MDP_PIPE_MAX,
};

enum mtk_isp_ctrl {
	ISP_REG_MMSYS_SW0_RST_B,
	ISP_REG_MMSYS_SW1_RST_B,
	ISP_REG_MDP_ASYNC_CFG_WD,
	ISP_REG_MDP_ASYNC_IPU_CFG_WD,
	ISP_REG_ISP_RELAY_CFG_WD,
	ISP_REG_IPU_RELAY_CFG_WD,
	ISP_BIT_MDP_DL_ASYNC_TX,
	ISP_BIT_MDP_DL_ASYNC_TX2,
	ISP_BIT_MDP_DL_ASYNC_RX,
	ISP_BIT_MDP_DL_ASYNC_RX2,
	ISP_BIT_NO_SOF_MODE,
	ISP_CTRL_MAX
};

void mtk_mmsys_ddp_connect(struct device *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next);

void mtk_mmsys_ddp_disconnect(struct device *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next);

void mtk_mmsys_ddp_config(struct device *dev, enum mtk_mmsys_config_type config,
			  u32 id, u32 val, struct cmdq_pkt *cmdq_pkt);

void mtk_mmsys_mdp_isp_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			    enum mtk_mdp_comp_id id);

void mtk_mmsys_mdp_camin_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id id,
			      u32 camin_w, u32 camin_h);

void mtk_mmsys_mdp_write_config(struct device *dev,
			 struct mmsys_cmdq_cmd *cmd,
			 u32 alias_id, u32 value, u32 mask);

void mtk_mmsys_write_reg_by_cmdq(struct device *dev,
			 struct mmsys_cmdq_cmd *cmd,
			 u32 alias_id, u32 value, u32 mask);

#endif /* __MTK_MMSYS_H */
