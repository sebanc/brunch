/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __MTK_MMSYS_H
#define __MTK_MMSYS_H

enum mtk_ddp_comp_id;
struct device;

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
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_DSI2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL_2L0,
	DDP_COMPONENT_OVL_2L1,
	DDP_COMPONENT_OVL_2L2,
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

struct mtk_mmsys_conn_funcs {
	void (*mout_en)(void __iomem *config_regs,
			enum mtk_ddp_comp_id cur,
			enum mtk_ddp_comp_id next,
			bool enable);
	void (*ovl_mout_en)(void __iomem *config_regs,
			    enum mtk_ddp_comp_id cur,
			    enum mtk_ddp_comp_id next,
			    bool enable);
	void (*sel_in)(void __iomem *config_regs,
		       enum mtk_ddp_comp_id cur,
		       enum mtk_ddp_comp_id next,
		       bool enable);
	void (*sout_sel)(void __iomem *config_regs,
			 enum mtk_ddp_comp_id cur,
			 enum mtk_ddp_comp_id next);
};

extern struct mtk_mmsys_conn_funcs mt2701_mmsys_funcs;
extern struct mtk_mmsys_conn_funcs mt8183_mmsys_funcs;
extern struct mtk_mmsys_conn_funcs mt8192_mmsys_funcs;

void mtk_mmsys_ddp_connect(struct device *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next);

void mtk_mmsys_ddp_disconnect(struct device *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next);

#endif /* __MTK_MMSYS_H */
