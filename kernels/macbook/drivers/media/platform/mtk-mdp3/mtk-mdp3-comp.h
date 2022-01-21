/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_COMP_H__
#define __MTK_MDP3_COMP_H__

#include <linux/soc/mediatek/mtk-mmsys.h>
#include "mtk-mdp3-cmdq.h"

#define MM_REG_WRITE_MASK(cmd, id, base, ofst, val, mask, ...) \
	cmdq_pkt_write_mask((cmd)->pkt, id, \
		(base) + (ofst), (val), (mask), ##__VA_ARGS__)
#define MM_REG_WRITE(cmd, id, base, ofst, val, mask, ...) \
	MM_REG_WRITE_MASK(cmd, id, base, ofst, val, \
		(((mask) & (ofst##_MASK)) == (ofst##_MASK)) ? \
			(0xffffffff) : (mask), ##__VA_ARGS__)

#define MM_REG_WAIT(cmd, evt) \
	cmdq_pkt_wfe((cmd)->pkt, (cmd)->event[(evt)], true)

#define MM_REG_WAIT_NO_CLEAR(cmd, evt) \
	cmdq_pkt_wfe((cmd)->pkt, (cmd)->event[(evt)], false)

#define MM_REG_CLEAR(cmd, evt) \
	cmdq_pkt_clear_event((cmd)->pkt, (cmd)->event[(evt)])

#define MM_REG_SET_EVENT(cmd, evt) \
	cmdq_pkt_set_event((cmd)->pkt, (cmd)->event[(evt)])

#define MM_REG_POLL_MASK(cmd, id, base, ofst, val, mask, ...) \
	cmdq_pkt_poll_mask((cmd)->pkt, id, \
		(base) + (ofst), (val), (mask), ##__VA_ARGS__)
#define MM_REG_POLL(cmd, id, base, ofst, val, mask, ...) \
	MM_REG_POLL_MASK((cmd), id, base, ofst, val, \
		(((mask) & (ofst##_MASK)) == (ofst##_MASK)) ? \
			(0xffffffff) : (mask), ##__VA_ARGS__)

#define MDP_CAMIN       get_comp_camin()
#define MDP_CAMIN2      get_comp_camin2()
#define MDP_RDMA0       get_comp_rdma0()
#define MDP_AAL0        get_comp_aal0()
#define MDP_CCORR0      get_comp_ccorr0()
#define MDP_RSZ0        get_comp_rsz0()
#define MDP_RSZ1        get_comp_rsz1()
#define MDP_TDSHP0      get_comp_tdshp0()
#define MDP_COLOR0      get_comp_color0()
#define MDP_WROT0       get_comp_wrot0()
#define MDP_WDMA        get_comp_wdma()
#define MDP_MERGE2      get_comp_merge2()
#define MDP_MERGE3      get_comp_merge3()

enum mdp_comp_type {
	MDP_COMP_TYPE_INVALID = 0,

	MDP_COMP_TYPE_IMGI,
	MDP_COMP_TYPE_WPEI,

	MDP_COMP_TYPE_RDMA,
	MDP_COMP_TYPE_RSZ,
	MDP_COMP_TYPE_WROT,
	MDP_COMP_TYPE_WDMA,
	MDP_COMP_TYPE_PATH1,
	MDP_COMP_TYPE_PATH2,
	MDP_COMP_TYPE_SPLIT,
	MDP_COMP_TYPE_STITCH,
	MDP_COMP_TYPE_FG,
	MDP_COMP_TYPE_OVL,
	MDP_COMP_TYPE_PAD,
	MDP_COMP_TYPE_MERGE,

	MDP_COMP_TYPE_TDSHP,
	MDP_COMP_TYPE_COLOR,
	MDP_COMP_TYPE_DRE,
	MDP_COMP_TYPE_CCORR,
	MDP_COMP_TYPE_HDR,
	MDP_COMP_TYPE_AAL,
	MDP_COMP_TYPE_TCC,

	MDP_COMP_TYPE_EXTO,	/* External path */
	MDP_COMP_TYPE_DL_PATH1, /* Direct-link path1 */
	MDP_COMP_TYPE_DL_PATH2, /* Direct-link path2 */
	MDP_COMP_TYPE_DL_PATH3, /* Direct-link path3 */
	MDP_COMP_TYPE_DL_PATH4, /* Direct-link path4 */
	MDP_COMP_TYPE_DL_PATH5, /* Direct-link path5 */
	MDP_COMP_TYPE_DL_PATH6, /* Direct-link path6 */
	MDP_COMP_TYPE_DUMMY,

	MDP_COMP_TYPE_COUNT	/* ALWAYS keep at the end */
};

enum mdp_comp_id {
	MDP_COMP_INVALID = -1,     /* Invalid engine */

	/* MT8183 Comp id */
	/* ISP */
	MT8183_MDP_COMP_WPEI = 0,
	MT8183_MDP_COMP_WPEO,           /* 1 */
	MT8183_MDP_COMP_WPEI2,          /* 2 */
	MT8183_MDP_COMP_WPEO2,          /* 3 */
	MT8183_MDP_COMP_ISP_IMGI,       /* 4 */
	MT8183_MDP_COMP_ISP_IMGO,       /* 5 */
	MT8183_MDP_COMP_ISP_IMG2O,      /* 6 */

	/* IPU */
	MT8183_MDP_COMP_IPUI,           /* 7 */
	MT8183_MDP_COMP_IPUO,           /* 8 */

	/* MDP */
	MT8183_MDP_COMP_CAMIN,          /* 9 */
	MT8183_MDP_COMP_CAMIN2,         /* 10 */
	MT8183_MDP_COMP_RDMA0,          /* 11 */
	MT8183_MDP_COMP_AAL0,           /* 12 */
	MT8183_MDP_COMP_CCORR0,         /* 13 */
	MT8183_MDP_COMP_RSZ0,           /* 14 */
	MT8183_MDP_COMP_RSZ1,           /* 15 */
	MT8183_MDP_COMP_TDSHP0,         /* 16 */
	MT8183_MDP_COMP_COLOR0,         /* 17 */
	MT8183_MDP_COMP_PATH0_SOUT,     /* 18 */
	MT8183_MDP_COMP_PATH1_SOUT,     /* 19 */
	MT8183_MDP_COMP_WROT0,          /* 20 */
	MT8183_MDP_COMP_WDMA,           /* 21 */

	/* Dummy Engine */
	MT8183_MDP_COMP_RDMA1,          /* 22 */
	MT8183_MDP_COMP_RSZ2,           /* 23 */
	MT8183_MDP_COMP_TDSHP1,         /* 24 */
	MT8183_MDP_COMP_WROT1,          /* 25 */
	MT8183_MDP_MAX_COMP_COUNT,

	MDP_MAX_COMP      /* ALWAYS keep at the end */
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
	RDMA1_SOF,
	RDMA2_SOF,
	RDMA3_SOF,
	WROT1_SOF,
	WROT2_SOF,
	WROT3_SOF,
	RDMA1_FRAME_DONE,
	RDMA2_FRAME_DONE,
	RDMA3_FRAME_DONE,
	WROT1_FRAME_DONE,
	WROT2_FRAME_DONE,
	WROT3_FRAME_DONE,

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

enum mdp_mmsys_config_id {
	CONFIG_VPP0_HW_DCM_1ST_DIS0,
	CONFIG_VPP0_DL_IRELAY_WR,
	CONFIG_VPP1_HW_DCM_1ST_DIS0,
	CONFIG_VPP1_HW_DCM_1ST_DIS1,
	CONFIG_VPP1_HW_DCM_2ND_DIS0,
	CONFIG_VPP1_HW_DCM_2ND_DIS1,
	CONFIG_SVPP2_BUF_BF_RSZ_SWITCH,
	CONFIG_SVPP3_BUF_BF_RSZ_SWITCH,
	MDP_MAX_CONFIG_COUNT
};

struct mdp_comp_list {
	enum mdp_comp_id wpei;
	enum mdp_comp_id wpeo;
	enum mdp_comp_id wpei2;
	enum mdp_comp_id wpeo2;
	enum mdp_comp_id camin;
	enum mdp_comp_id camin2;
	enum mdp_comp_id split;
	enum mdp_comp_id split2;
	enum mdp_comp_id rdma0;
	enum mdp_comp_id rdma1;
	enum mdp_comp_id rdma2;
	enum mdp_comp_id rdma3;
	enum mdp_comp_id stitch;
	enum mdp_comp_id fg0;
	enum mdp_comp_id fg1;
	enum mdp_comp_id fg2;
	enum mdp_comp_id fg3;
	enum mdp_comp_id hdr0;
	enum mdp_comp_id hdr1;
	enum mdp_comp_id hdr2;
	enum mdp_comp_id hdr3;
	enum mdp_comp_id aal0;
	enum mdp_comp_id aal1;
	enum mdp_comp_id aal2;
	enum mdp_comp_id aal3;
	enum mdp_comp_id rsz0;
	enum mdp_comp_id rsz1;
	enum mdp_comp_id rsz2;
	enum mdp_comp_id rsz3;
	enum mdp_comp_id tdshp0;
	enum mdp_comp_id tdshp1;
	enum mdp_comp_id tdshp2;
	enum mdp_comp_id tdshp3;
	enum mdp_comp_id color0;
	enum mdp_comp_id color1;
	enum mdp_comp_id color2;
	enum mdp_comp_id color3;
	enum mdp_comp_id ccorr0;
	enum mdp_comp_id ovl0;
	enum mdp_comp_id ovl1;
	enum mdp_comp_id pad0;
	enum mdp_comp_id pad1;
	enum mdp_comp_id pad2;
	enum mdp_comp_id pad3;
	enum mdp_comp_id tcc0;
	enum mdp_comp_id tcc1;
	enum mdp_comp_id wrot0;
	enum mdp_comp_id wrot1;
	enum mdp_comp_id wrot2;
	enum mdp_comp_id wrot3;
	enum mdp_comp_id merge2;
	enum mdp_comp_id merge3;
	enum mdp_comp_id wdma;
	enum mdp_comp_id vdo0dl0;
	enum mdp_comp_id vdo1dl0;
	enum mdp_comp_id vdo0dl1;
	enum mdp_comp_id vdo1dl1;
	enum mdp_comp_id path0_sout;
	enum mdp_comp_id path1_sout;
};

struct mdp_comp_match {
	enum mdp_comp_type	type;
	u32			alias_id;
	enum mtk_mdp_comp_id public_id;
};

struct mdp_mutex_info {
	u32 mmsys_id;
	u32 mod;
	u32 mod2;
};

struct mdp_comp_data {
	struct mdp_comp_match match;
	struct mdp_mutex_info mutex;
};

/* Used to describe the item order in MDP property */
struct mdp_comp_info {
	u32	clk_num;
	u32 clk_ofst;
	u32	dts_reg_ofst;
};

struct mdp_comp_ops;

struct mdp_comp {
	struct mdp_dev			*mdp_dev;
	void __iomem			*regs;
	phys_addr_t			reg_base;
	u8				subsys_id;
	struct clk			*clks[6];
	struct device			*comp_dev;
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
	int (*init_comp)(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd);
	int (*config_frame)(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd,
			    const struct v4l2_rect *compose);
	int (*config_subfrm)(struct mdp_comp_ctx *ctx,
			     struct mmsys_cmdq_cmd *cmd, u32 index);
	int (*wait_comp_event)(struct mdp_comp_ctx *ctx,
			       struct mmsys_cmdq_cmd *cmd);
	int (*advance_subfrm)(struct mdp_comp_ctx *ctx,
			      struct mmsys_cmdq_cmd *cmd, u32 index);
	int (*post_process)(struct mdp_comp_ctx *ctx, struct mmsys_cmdq_cmd *cmd);
};

struct mdp_dev;

enum mdp_comp_id get_comp_camin(void);
enum mdp_comp_id get_comp_camin2(void);
enum mdp_comp_id get_comp_rdma0(void);
enum mdp_comp_id get_comp_aal0(void);
enum mdp_comp_id get_comp_ccorr0(void);
enum mdp_comp_id get_comp_rsz0(void);
enum mdp_comp_id get_comp_rsz1(void);
enum mdp_comp_id get_comp_tdshp0(void);
enum mdp_comp_id get_comp_color0(void);
enum mdp_comp_id get_comp_wrot0(void);
enum mdp_comp_id get_comp_wdma(void);
enum mdp_comp_id get_comp_merge2(void);
enum mdp_comp_id get_comp_merge3(void);

int mdp_component_init(struct mdp_dev *mdp);
void mdp_component_deinit(struct mdp_dev *mdp);
void mdp_comp_clock_on(struct device *dev, struct mdp_comp *comp);
void mdp_comp_clock_off(struct device *dev, struct mdp_comp *comp);
void mdp_comp_clocks_on(struct device *dev, struct mdp_comp *comps, int num);
void mdp_comp_clocks_off(struct device *dev, struct mdp_comp *comps, int num);
int mdp_comp_ctx_init(struct mdp_dev *mdp, struct mdp_comp_ctx *ctx,
		      const struct img_compparam *param,
	const struct img_ipi_frameparam *frame);

int mdp_get_event_idx(struct mdp_dev *mdp, enum mdp_comp_event event);

#endif  /* __MTK_MDP3_COMP_H__ */
