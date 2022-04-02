// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <media/videobuf2-dma-contig.h>
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-m2m.h"
#include "mtk-mdp3-debug.h"

/* MDP debug log level (0-3). 3 shows all the logs. */
int mtk_mdp_debug;
EXPORT_SYMBOL(mtk_mdp_debug);
module_param_named(debug, mtk_mdp_debug, int, 0644);

static const struct mdp_platform_config mt8183_plat_cfg = {
	.rdma_support_10bit             = true,
	.rdma_rsz1_sram_sharing         = true,
	.rdma_upsample_repeat_only      = true,
	.rsz_disable_dcm_small_sample   = false,
	.wrot_filter_constraint         = false,
	.gce_event_offset               = 2,
};

static const struct mdp_platform_config mt8195_plat_cfg = {
	.rdma_support_10bit             = true,
	.rdma_support_extend_ufo        = true,
	.rdma_support_hyfbc             = true,
	.rdma_support_afbc              = true,
	.rdma_esl_setting               = true,
	.rdma_rsz1_sram_sharing         = false,
	.rdma_upsample_repeat_only      = false,
	.rsz_disable_dcm_small_sample   = false,
	.rsz_etc_control                = true,
	.wrot_filter_constraint         = false,
	.tdshp_1_1                      = true,
	.tdshp_dyn_contrast_version     = 2,
	.mdp_version_8195               = true,
	.mdp_version_6885               = true,
	.support_dual_pipe              = true,
	.gce_event_offset               = 0,
};

static const struct mdp_comp_list mt8183_comp_list = {
	.wpei		= MT8183_MDP_COMP_WPEI,
	.wpeo		= MT8183_MDP_COMP_WPEO,
	.wpei2		= MT8183_MDP_COMP_WPEI2,
	.wpeo2		= MT8183_MDP_COMP_WPEO2,
	.camin		= MT8183_MDP_COMP_CAMIN,
	.camin2		= MT8183_MDP_COMP_CAMIN2,
	.split		= MDP_COMP_INVALID,
	.split2		= MDP_COMP_INVALID,
	.rdma0		= MT8183_MDP_COMP_RDMA0,
	.rdma1		= MDP_COMP_INVALID,
	.rdma2		= MDP_COMP_INVALID,
	.rdma3		= MDP_COMP_INVALID,
	.stitch		= MDP_COMP_INVALID,
	.fg0		= MDP_COMP_INVALID,
	.fg1		= MDP_COMP_INVALID,
	.fg2		= MDP_COMP_INVALID,
	.fg3		= MDP_COMP_INVALID,
	.hdr0		= MDP_COMP_INVALID,
	.hdr1		= MDP_COMP_INVALID,
	.hdr2		= MDP_COMP_INVALID,
	.hdr3		= MDP_COMP_INVALID,
	.aal0		= MT8183_MDP_COMP_AAL0,
	.aal1		= MDP_COMP_INVALID,
	.aal2		= MDP_COMP_INVALID,
	.aal3		= MDP_COMP_INVALID,
	.rsz0		= MT8183_MDP_COMP_RSZ0,
	.rsz1		= MT8183_MDP_COMP_RSZ1,
	.rsz2		= MDP_COMP_INVALID,
	.rsz3		= MDP_COMP_INVALID,
	.tdshp0		= MT8183_MDP_COMP_TDSHP0,
	.tdshp1		= MDP_COMP_INVALID,
	.tdshp2		= MDP_COMP_INVALID,
	.tdshp3		= MDP_COMP_INVALID,
	.color0		= MT8183_MDP_COMP_COLOR0,
	.color1		= MDP_COMP_INVALID,
	.color2		= MDP_COMP_INVALID,
	.color3		= MDP_COMP_INVALID,
	.ccorr0		= MT8183_MDP_COMP_CCORR0,
	.ovl0		= MDP_COMP_INVALID,
	.ovl1		= MDP_COMP_INVALID,
	.pad0		= MDP_COMP_INVALID,
	.pad1		= MDP_COMP_INVALID,
	.pad2		= MDP_COMP_INVALID,
	.pad3		= MDP_COMP_INVALID,
	.tcc0		= MDP_COMP_INVALID,
	.tcc1		= MDP_COMP_INVALID,
	.wrot0		= MT8183_MDP_COMP_WROT0,
	.wrot1		= MDP_COMP_INVALID,
	.wrot2		= MDP_COMP_INVALID,
	.wrot3		= MDP_COMP_INVALID,
	.merge2		= MDP_COMP_INVALID,
	.merge3		= MDP_COMP_INVALID,
	.wdma		= MT8183_MDP_COMP_WDMA,
	.vdo0dl0	= MDP_COMP_INVALID,
	.vdo1dl0	= MDP_COMP_INVALID,
	.vdo0dl1	= MDP_COMP_INVALID,
	.vdo1dl1	= MDP_COMP_INVALID,
	.path0_sout	= MT8183_MDP_COMP_PATH0_SOUT,
	.path1_sout	= MT8183_MDP_COMP_PATH1_SOUT,
};

static const struct mdp_comp_list mt8195_comp_list = {
	.wpei		= MT8195_MDP_COMP_WPEI,
	.wpeo		= MT8195_MDP_COMP_WPEO,
	.wpei2		= MT8195_MDP_COMP_WPEI2,
	.wpeo2		= MT8195_MDP_COMP_WPEO2,
	.camin		= MT8195_MDP_COMP_CAMIN,
	.camin2		= MT8195_MDP_COMP_CAMIN2,
	.split		= MT8195_MDP_COMP_SPLIT,
	.split2		= MT8195_MDP_COMP_SPLIT2,
	.rdma0		= MT8195_MDP_COMP_RDMA0,
	.rdma1		= MT8195_MDP_COMP_RDMA1,
	.rdma2		= MT8195_MDP_COMP_RDMA2,
	.rdma3		= MT8195_MDP_COMP_RDMA3,
	.stitch		= MT8195_MDP_COMP_STITCH,
	.fg0		= MT8195_MDP_COMP_FG0,
	.fg1		= MT8195_MDP_COMP_FG1,
	.fg2		= MT8195_MDP_COMP_FG2,
	.fg3		= MT8195_MDP_COMP_FG3,
	.hdr0		= MT8195_MDP_COMP_HDR0,
	.hdr1		= MT8195_MDP_COMP_HDR1,
	.hdr2		= MT8195_MDP_COMP_HDR2,
	.hdr3		= MT8195_MDP_COMP_HDR3,
	.aal0		= MT8195_MDP_COMP_AAL0,
	.aal1		= MT8195_MDP_COMP_AAL1,
	.aal2		= MT8195_MDP_COMP_AAL2,
	.aal3		= MT8195_MDP_COMP_AAL3,
	.rsz0		= MT8195_MDP_COMP_RSZ0,
	.rsz1		= MT8195_MDP_COMP_RSZ1,
	.rsz2		= MT8195_MDP_COMP_RSZ2,
	.rsz3		= MT8195_MDP_COMP_RSZ3,
	.tdshp0		= MT8195_MDP_COMP_TDSHP0,
	.tdshp1		= MT8195_MDP_COMP_TDSHP1,
	.tdshp2		= MT8195_MDP_COMP_TDSHP2,
	.tdshp3		= MT8195_MDP_COMP_TDSHP3,
	.color0		= MT8195_MDP_COMP_COLOR0,
	.color1		= MT8195_MDP_COMP_COLOR1,
	.color2		= MT8195_MDP_COMP_COLOR2,
	.color3		= MT8195_MDP_COMP_COLOR3,
	.ccorr0		= MDP_COMP_INVALID,
	.ovl0		= MT8195_MDP_COMP_OVL0,
	.ovl1		= MT8195_MDP_COMP_OVL1,
	.pad0		= MT8195_MDP_COMP_PAD0,
	.pad1		= MT8195_MDP_COMP_PAD1,
	.pad2		= MT8195_MDP_COMP_PAD2,
	.pad3		= MT8195_MDP_COMP_PAD3,
	.tcc0		= MT8195_MDP_COMP_TCC0,
	.tcc1		= MT8195_MDP_COMP_TCC1,
	.wrot0		= MT8195_MDP_COMP_WROT0,
	.wrot1		= MT8195_MDP_COMP_WROT1,
	.wrot2		= MT8195_MDP_COMP_WROT2,
	.wrot3		= MT8195_MDP_COMP_WROT3,
	.merge2		= MT8195_MDP_COMP_MERGE2,
	.merge3		= MT8195_MDP_COMP_MERGE3,
	.wdma		= MDP_COMP_INVALID,
	.vdo0dl0	= MT8195_MDP_COMP_VDO0DL0,
	.vdo1dl0	= MT8195_MDP_COMP_VDO1DL0,
	.vdo0dl1	= MT8195_MDP_COMP_VDO0DL1,
	.vdo1dl1	= MT8195_MDP_COMP_VDO1DL1,
	.path0_sout	= MDP_COMP_INVALID,
	.path1_sout	= MDP_COMP_INVALID,
};

static const struct mdp_comp_data mt8183_mdp_comp_data[MT8183_MDP_MAX_COMP_COUNT] = {
	[MT8183_MDP_COMP_WPEI] = { {MDP_COMP_TYPE_WPEI, 0, MDP_COMP_WPEI}, {0, 0, 0} },
	[MT8183_MDP_COMP_WPEO] = { {MDP_COMP_TYPE_EXTO, 2, MDP_COMP_WPEO}, {0, 0, 0} },
	[MT8183_MDP_COMP_WPEI2] = { {MDP_COMP_TYPE_WPEI, 1, MDP_COMP_WPEI2}, {0, 0, 0} },
	[MT8183_MDP_COMP_WPEO2] = { {MDP_COMP_TYPE_EXTO, 3, MDP_COMP_WPEO2}, {0, 0, 0} },
	[MT8183_MDP_COMP_ISP_IMGI] = { {MDP_COMP_TYPE_IMGI, 0, MDP_COMP_ISP_IMGI}, {0, 0, 0} },
	[MT8183_MDP_COMP_ISP_IMGO] = { {MDP_COMP_TYPE_EXTO, 0, MDP_COMP_ISP_IMGO}, {0, 0, 0} },
	[MT8183_MDP_COMP_ISP_IMG2O] = { {MDP_COMP_TYPE_EXTO, 1, MDP_COMP_ISP_IMG2O}, {0, 0, 0} },

	[MT8183_MDP_COMP_CAMIN] = { {MDP_COMP_TYPE_DL_PATH1, 0, MDP_COMP_CAMIN}, {0, 0, 0} },
	[MT8183_MDP_COMP_CAMIN2] = { {MDP_COMP_TYPE_DL_PATH2, 1, MDP_COMP_CAMIN2}, {0, 0, 0} },
	[MT8183_MDP_COMP_RDMA0] = { {MDP_COMP_TYPE_RDMA, 0, MDP_COMP_RDMA0}, {0, BIT(2), 0} },
	[MT8183_MDP_COMP_AAL0] = { {MDP_COMP_TYPE_AAL, 0, MDP_COMP_AAL0}, {0, BIT(23), 0} },
	[MT8183_MDP_COMP_CCORR0] = { {MDP_COMP_TYPE_CCORR, 0, MDP_COMP_CCORR0}, {0, BIT(24), 0} },
	[MT8183_MDP_COMP_RSZ0] = { {MDP_COMP_TYPE_RSZ, 0, MDP_COMP_RSZ0}, {0, BIT(4), 0} },
	[MT8183_MDP_COMP_RSZ1] = { {MDP_COMP_TYPE_RSZ, 1, MDP_COMP_RSZ1}, {0, BIT(5), 0} },
	[MT8183_MDP_COMP_TDSHP0] = { {MDP_COMP_TYPE_TDSHP, 0, MDP_COMP_TDSHP0}, {0, BIT(6), 0} },
	[MT8183_MDP_COMP_PATH0_SOUT] = { {MDP_COMP_TYPE_PATH1, 0, MDP_COMP_PATH0_SOUT}, {0, 0, 0} },
	[MT8183_MDP_COMP_PATH1_SOUT] = { {MDP_COMP_TYPE_PATH2, 1, MDP_COMP_PATH1_SOUT}, {0, 0, 0} },
	[MT8183_MDP_COMP_WROT0] = { {MDP_COMP_TYPE_WROT, 0, MDP_COMP_WROT0}, {0, BIT(7), 0} },
	[MT8183_MDP_COMP_WDMA] = { {MDP_COMP_TYPE_WDMA, 0, MDP_COMP_WDMA}, {0, BIT(8), 0} },
};

static const struct mdp_comp_data mt8195_mdp_comp_data[MT8195_MDP_MAX_COMP_COUNT] = {
	[MT8195_MDP_COMP_WPEI] = {
		{MDP_COMP_TYPE_WPEI, 0, MDP_COMP_WPEI},
		{0, BIT(13), 0}
	},
	[MT8195_MDP_COMP_WPEO] = {
		{MDP_COMP_TYPE_EXTO, 2, MDP_COMP_WPEO},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_WPEI2] = {
		{MDP_COMP_TYPE_WPEI, 1, MDP_COMP_WPEI2},
		{0, BIT(14), 0}
	},
	[MT8195_MDP_COMP_WPEO2] = {
		{MDP_COMP_TYPE_EXTO, 3, MDP_COMP_WPEO2},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_CAMIN] = {
		{MDP_COMP_TYPE_DL_PATH1, 0, MDP_COMP_CAMIN},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_CAMIN2] = {
		{MDP_COMP_TYPE_DL_PATH2, 1, MDP_COMP_CAMIN2},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_SPLIT] = {
		{MDP_COMP_TYPE_SPLIT, 0, MDP_COMP_SPLIT},
		{1, 0, BIT(2)}
	},
	[MT8195_MDP_COMP_SPLIT2] = {
		{MDP_COMP_TYPE_SPLIT, 1, MDP_COMP_SPLIT2},
		{1, BIT(2), 0}
	},
	[MT8195_MDP_COMP_RDMA0] = {
		{MDP_COMP_TYPE_RDMA, 0, MDP_COMP_RDMA0},
		{0, BIT(0), 0}
	},
	[MT8195_MDP_COMP_RDMA1] = {
		{MDP_COMP_TYPE_RDMA, 1, MDP_COMP_RDMA1},
		{1, BIT(4), 0}
	},
	[MT8195_MDP_COMP_RDMA2] = {
		{MDP_COMP_TYPE_RDMA, 2, MDP_COMP_RDMA2},
		{1, BIT(5), 0}
	},
	[MT8195_MDP_COMP_RDMA3] = {
		{MDP_COMP_TYPE_RDMA, 3, MDP_COMP_RDMA3},
		{1, BIT(6), 0}
	},
	[MT8195_MDP_COMP_STITCH] = {
		{MDP_COMP_TYPE_STITCH, 0, MDP_COMP_STITCH},
		{0, BIT(2), 0}
	},
	[MT8195_MDP_COMP_FG0] = {
		{MDP_COMP_TYPE_FG, 0, MDP_COMP_FG0},
		{0, BIT(1), 0}
	},
	[MT8195_MDP_COMP_FG1] = {
		{MDP_COMP_TYPE_FG, 1, MDP_COMP_FG1},
		{1, BIT(7), 0}
	},
	[MT8195_MDP_COMP_FG2] = {
		{MDP_COMP_TYPE_FG, 2, MDP_COMP_FG2},
		{1, BIT(8), 0}
	},
	[MT8195_MDP_COMP_FG3] = {
		{MDP_COMP_TYPE_FG, 3, MDP_COMP_FG3},
		{1, BIT(9), 0}
	},
	[MT8195_MDP_COMP_HDR0] = {
		{MDP_COMP_TYPE_HDR, 0, MDP_COMP_HDR0},
		{0, BIT(3), 0}
	},
	[MT8195_MDP_COMP_HDR1] = {
		{MDP_COMP_TYPE_HDR, 1, MDP_COMP_HDR1},
		{1, BIT(10), 0}
	},
	[MT8195_MDP_COMP_HDR2] = {
		{MDP_COMP_TYPE_HDR, 2, MDP_COMP_HDR2},
		{1, BIT(11), 0}
	},
	[MT8195_MDP_COMP_HDR3] = {
		{MDP_COMP_TYPE_HDR, 3, MDP_COMP_HDR3},
		{1, BIT(12), 0}
	},
	[MT8195_MDP_COMP_AAL0] = {
		{MDP_COMP_TYPE_AAL, 0, MDP_COMP_AAL0},
		{0, BIT(4), 0}
	},
	[MT8195_MDP_COMP_AAL1] = {
		{MDP_COMP_TYPE_AAL, 1, MDP_COMP_AAL1},
		{1, BIT(13), 0}
	},
	[MT8195_MDP_COMP_AAL2] = {
		{MDP_COMP_TYPE_AAL, 2, MDP_COMP_AAL2},
		{1, BIT(14), 0}
	},
	[MT8195_MDP_COMP_AAL3] = {
		{MDP_COMP_TYPE_AAL, 3, MDP_COMP_AAL3},
		{1, BIT(15), 0}
	},
	[MT8195_MDP_COMP_RSZ0] = {
		{MDP_COMP_TYPE_RSZ, 0, MDP_COMP_RSZ0},
		{0, BIT(5), 0}
	},
	[MT8195_MDP_COMP_RSZ1] = {
		{MDP_COMP_TYPE_RSZ, 1, MDP_COMP_RSZ1},
		{1, BIT(16), 0}
	},
	[MT8195_MDP_COMP_RSZ2] = {
		{MDP_COMP_TYPE_RSZ, 2, MDP_COMP_RSZ2},
		{1, BIT(17) | BIT(22), 0}
	},
	[MT8195_MDP_COMP_RSZ3] = {
		{MDP_COMP_TYPE_RSZ, 3, MDP_COMP_RSZ3},
		{1, BIT(18) | BIT(23), 0}
	},
	[MT8195_MDP_COMP_TDSHP0] = {
		{MDP_COMP_TYPE_TDSHP, 0, MDP_COMP_TDSHP0},
		{0, BIT(6), 0}
	},
	[MT8195_MDP_COMP_TDSHP1] = {
		{MDP_COMP_TYPE_TDSHP, 1, MDP_COMP_TDSHP1},
		{1, BIT(19), 0}
	},
	[MT8195_MDP_COMP_TDSHP2] = {
		{MDP_COMP_TYPE_TDSHP, 2, MDP_COMP_TDSHP2},
		{1, BIT(20), 0}
	},
	[MT8195_MDP_COMP_TDSHP3] = {
		{MDP_COMP_TYPE_TDSHP, 3, MDP_COMP_TDSHP3},
		{1, BIT(21), 0}
	},
	[MT8195_MDP_COMP_COLOR0] = {
		{MDP_COMP_TYPE_COLOR, 0, MDP_COMP_COLOR0},
		{0, BIT(7), 0}
	},
	[MT8195_MDP_COMP_COLOR1] = {
		{MDP_COMP_TYPE_COLOR, 1, MDP_COMP_COLOR1},
		{1, BIT(24), 0}
	},
	[MT8195_MDP_COMP_COLOR2] = {
		{MDP_COMP_TYPE_COLOR, 2, MDP_COMP_COLOR2},
		{1, BIT(25), 0}
	},
	[MT8195_MDP_COMP_COLOR3] = {
		{MDP_COMP_TYPE_COLOR, 3, MDP_COMP_COLOR3},
		{1, BIT(26), 0}
	},
	[MT8195_MDP_COMP_OVL0] = {
		{MDP_COMP_TYPE_OVL, 0, MDP_COMP_OVL0},
		{0, BIT(8), 0}
	},
	[MT8195_MDP_COMP_OVL1] = {
		{MDP_COMP_TYPE_OVL, 1, MDP_COMP_OVL1},
		{1, BIT(27), 0}
	},
	[MT8195_MDP_COMP_PAD0] = {
		{MDP_COMP_TYPE_PAD, 0, MDP_COMP_PAD0},
		{0, BIT(9), 0}
	},
	[MT8195_MDP_COMP_PAD1] = {
		{MDP_COMP_TYPE_PAD, 1, MDP_COMP_PAD1},
		{1, BIT(28), 0}
	},
	[MT8195_MDP_COMP_PAD2] = {
		{MDP_COMP_TYPE_PAD, 2, MDP_COMP_PAD2},
		{1, BIT(29), 0}
	},
	[MT8195_MDP_COMP_PAD3] = {
		{MDP_COMP_TYPE_PAD, 3, MDP_COMP_PAD3},
		{1, BIT(30), 0}
	},
	[MT8195_MDP_COMP_TCC0] = {
		{MDP_COMP_TYPE_TCC, 0, MDP_COMP_TCC0},
		{0, BIT(10), 0}
	},
	[MT8195_MDP_COMP_TCC1] = {
		{MDP_COMP_TYPE_TCC, 1, MDP_COMP_TCC1},
		{1, BIT(3), 0}
	},
	[MT8195_MDP_COMP_WROT0] = {
		{MDP_COMP_TYPE_WROT, 0, MDP_COMP_WROT0},
		{0, BIT(11), 0}
	},
	[MT8195_MDP_COMP_WROT1] = {
		{MDP_COMP_TYPE_WROT, 1, MDP_COMP_WROT1},
		{1, BIT(31), 0}
	},
	[MT8195_MDP_COMP_WROT2] = {
		{MDP_COMP_TYPE_WROT, 2, MDP_COMP_WROT2},
		{1, 0, BIT(0)}
	},
	[MT8195_MDP_COMP_WROT3] = {
		{MDP_COMP_TYPE_WROT, 3, MDP_COMP_WROT3},
		{1, 0, BIT(1)}
	},
	[MT8195_MDP_COMP_MERGE2] = {
		{MDP_COMP_TYPE_MERGE, 2, MDP_COMP_MERGE2},
		{1, 0, 0}
	},
	[MT8195_MDP_COMP_MERGE3] = {
		{MDP_COMP_TYPE_MERGE, 3, MDP_COMP_MERGE2},
		{1, 0, 0}
	},
	[MT8195_MDP_COMP_PQ0_SOUT] = {
		{MDP_COMP_TYPE_DUMMY, 0, MDP_COMP_PQ0_SOUT},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_PQ1_SOUT] = {
		{MDP_COMP_TYPE_DUMMY, 1, MDP_COMP_PQ1_SOUT},
		{1, 0, 0}
	},
	[MT8195_MDP_COMP_TO_WARP0MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 2, MDP_COMP_TO_WARP0MOUT},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_TO_WARP1MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 3, MDP_COMP_TO_WARP1MOUT},
		{0, 0, 0}
	},
	[MT8195_MDP_COMP_TO_SVPP2MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 4, MDP_COMP_TO_SVPP2MOUT},
		{1, 0, 0}
	},
	[MT8195_MDP_COMP_TO_SVPP3MOUT] = {
		{MDP_COMP_TYPE_DUMMY, 5, MDP_COMP_TO_SVPP3MOUT},
		{1, 0, 0}
	},
	[MT8195_MDP_COMP_VPP0_SOUT] = {
		{MDP_COMP_TYPE_PATH1, 0, MDP_COMP_VPP0_SOUT},
		{0, BIT(15), BIT(2)}
	},
	[MT8195_MDP_COMP_VPP1_SOUT] = {
		{MDP_COMP_TYPE_PATH2, 1, MDP_COMP_VPP1_SOUT},
		{1, BIT(16), BIT(3)}
	},
	[MT8195_MDP_COMP_VDO0DL0] = {
		{MDP_COMP_TYPE_DL_PATH3, 0, MDP_COMP_VDO0DL0},
		{1, 0, BIT(4)}
	},
	[MT8195_MDP_COMP_VDO1DL0] = {
		{MDP_COMP_TYPE_DL_PATH4, 0, MDP_COMP_VDO1DL0},
		{1, 0, BIT(6)}
	},
	[MT8195_MDP_COMP_VDO0DL1] = {
		{MDP_COMP_TYPE_DL_PATH5, 0, MDP_COMP_VDO0DL1},
		{1, 0, BIT(5)}
	},
	[MT8195_MDP_COMP_VDO1DL1] = {
		{MDP_COMP_TYPE_DL_PATH6, 0, MDP_COMP_VDO1DL1},
		{1, 0, BIT(7)}
	},
};

static const enum mdp_comp_event mt8183_mdp_event[] = {
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
	WPE_B_DONE
};

static const enum mdp_comp_event mt8195_mdp_event[] = {
	RDMA0_SOF,
	WROT0_SOF,
	RDMA0_DONE,
	WROT0_DONE,
	RDMA1_SOF,
	RDMA2_SOF,
	RDMA3_SOF,
	WROT1_SOF,
	WROT2_SOF,
	WROT3_SOF,
	RDMA1_DONE,
	RDMA2_DONE,
	RDMA3_DONE,
	WROT1_DONE,
	WROT2_DONE,
	WROT3_DONE
};

static const struct mdp_comp_info mt8183_comp_dt_info[] = {
	[MDP_COMP_TYPE_RDMA]		= {2, 0, 0},
	[MDP_COMP_TYPE_RSZ]			= {1, 0, 0},
	[MDP_COMP_TYPE_WROT]		= {1, 0, 0},
	[MDP_COMP_TYPE_WDMA]		= {1, 0, 0},
	[MDP_COMP_TYPE_PATH1]		= {0, 0, 2},
	[MDP_COMP_TYPE_PATH2]		= {0, 0, 3},
	[MDP_COMP_TYPE_CCORR]		= {1, 0, 0},
	[MDP_COMP_TYPE_IMGI]		= {0, 0, 4},
	[MDP_COMP_TYPE_EXTO]		= {0, 0, 4},
	[MDP_COMP_TYPE_DL_PATH1]	= {2, 2, 1},
	[MDP_COMP_TYPE_DL_PATH2]	= {2, 4, 1},
};

static const struct mdp_comp_info mt8195_comp_dt_info[] = {
	[MDP_COMP_TYPE_SPLIT]		= {7, 0, 0},
	[MDP_COMP_TYPE_STITCH]		= {1, 0, 0},
	[MDP_COMP_TYPE_RDMA]		= {3, 0, 0},
	[MDP_COMP_TYPE_FG]		= {1, 0, 0},
	[MDP_COMP_TYPE_HDR]		= {1, 0, 0},
	[MDP_COMP_TYPE_AAL]		= {1, 0, 0},
	[MDP_COMP_TYPE_RSZ]		= {2, 0, 0},
	[MDP_COMP_TYPE_TDSHP]		= {1, 0, 0},
	[MDP_COMP_TYPE_COLOR]		= {1, 0, 0},
	[MDP_COMP_TYPE_OVL]		= {1, 0, 0},
	[MDP_COMP_TYPE_PAD]		= {1, 0, 0},
	[MDP_COMP_TYPE_TCC]		= {1, 0, 0},
	[MDP_COMP_TYPE_WROT]		= {1, 0, 0},
	[MDP_COMP_TYPE_MERGE]		= {1, 0, 0},
	[MDP_COMP_TYPE_PATH1]		= {4, 9, 0},
	[MDP_COMP_TYPE_PATH2]		= {2, 13, 0},
	[MDP_COMP_TYPE_DL_PATH1]	= {3, 3, 0},
	[MDP_COMP_TYPE_DL_PATH2]	= {3, 6, 0},
	[MDP_COMP_TYPE_DL_PATH3]	= {1, 15, 0},
	[MDP_COMP_TYPE_DL_PATH4]	= {1, 16, 0},
	[MDP_COMP_TYPE_DL_PATH5]	= {1, 17, 0},
	[MDP_COMP_TYPE_DL_PATH6]	= {1, 18, 0},
};

static const struct mdp_pipe_info mt8183_pipe_info[] = {
	{MDP_PIPE_IMGI, 0, 0},
	{MDP_PIPE_RDMA0, 0, 1},
	{MDP_PIPE_WPEI, 0, 2},
	{MDP_PIPE_WPEI2, 0, 3}
};

static const struct mdp_pipe_info mt8195_pipe_info[] = {
	{MDP_PIPE_WPEI, 0, 0},
	{MDP_PIPE_WPEI2, 0, 1},
	{MDP_PIPE_RDMA0, 0, 2},
	{MDP_PIPE_VPP1_SOUT, 0, 3},
	{MDP_PIPE_SPLIT, 1, 2, 0x387},
	{MDP_PIPE_SPLIT2, 1, 3, 0x387},
	{MDP_PIPE_RDMA1, 1, 1},
	{MDP_PIPE_RDMA2, 1, 2},
	{MDP_PIPE_RDMA3, 1, 3},
	{MDP_PIPE_VPP0_SOUT, 1, 4},
};

static const struct mdp_format mt8183_formats[] = {
	{
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.mdp_color	= MDP_COLOR_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565X,
		.mdp_color	= MDP_COLOR_RGB565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.mdp_color	= MDP_COLOR_BGR565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.mdp_color	= MDP_COLOR_RGB888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.mdp_color	= MDP_COLOR_BGR888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.mdp_color	= MDP_COLOR_BGRA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.mdp_color	= MDP_COLOR_ARGB8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.mdp_color	= MDP_COLOR_UYVY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.mdp_color	= MDP_COLOR_VYUY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mdp_color	= MDP_COLOR_YUYV,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.mdp_color	= MDP_COLOR_YVYU,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.mdp_color	= MDP_COLOR_I420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.mdp_color	= MDP_COLOR_YV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.mdp_color	= MDP_COLOR_NV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.mdp_color	= MDP_COLOR_NV21,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.mdp_color	= MDP_COLOR_NV16,
		.depth		= { 16 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.mdp_color	= MDP_COLOR_NV61,
		.depth		= { 16 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV24,
		.mdp_color	= MDP_COLOR_NV24,
		.depth		= { 24 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV42,
		.mdp_color	= MDP_COLOR_NV42,
		.depth		= { 24 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MT21C,
		.mdp_color	= MDP_COLOR_420_BLKP_UFO,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 4,
		.halign		= 5,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MM21,
		.mdp_color	= MDP_COLOR_420_BLKP,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 4,
		.halign		= 5,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.mdp_color	= MDP_COLOR_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.mdp_color	= MDP_COLOR_NV21,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.mdp_color	= MDP_COLOR_NV16,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.mdp_color	= MDP_COLOR_NV61,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.mdp_color	= MDP_COLOR_I420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.mdp_color	= MDP_COLOR_YV12,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}
};

static const struct mdp_format mt8195_formats[] = {
	{
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.mdp_color	= MDP_COLOR_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565X,
		.mdp_color	= MDP_COLOR_RGB565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.mdp_color	= MDP_COLOR_BGR565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.mdp_color	= MDP_COLOR_RGB888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.mdp_color	= MDP_COLOR_BGR888,
		.depth		= { 24 },
		.row_depth	= { 24 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.mdp_color	= MDP_COLOR_BGRA8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.mdp_color	= MDP_COLOR_ARGB8888,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.mdp_color	= MDP_COLOR_UYVY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.mdp_color	= MDP_COLOR_VYUY,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mdp_color	= MDP_COLOR_YUYV,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.mdp_color	= MDP_COLOR_YVYU,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.mdp_color	= MDP_COLOR_I420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.mdp_color	= MDP_COLOR_YV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.mdp_color	= MDP_COLOR_NV12,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.mdp_color	= MDP_COLOR_NV21,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.mdp_color	= MDP_COLOR_NV16,
		.depth		= { 16 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.mdp_color	= MDP_COLOR_NV61,
		.depth		= { 16 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV24,
		.mdp_color	= MDP_COLOR_NV24,
		.depth		= { 24 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV42,
		.mdp_color	= MDP_COLOR_NV42,
		.depth		= { 24 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MT21C,
		.mdp_color	= MDP_COLOR_NV12_HYFBC,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 1,
		.walign		= 4,
		.halign		= 4,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_MM21,
		.mdp_color	= MDP_COLOR_420_BLKP,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 4,
		.halign		= 5,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.mdp_color	= MDP_COLOR_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.mdp_color	= MDP_COLOR_NV21,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.mdp_color	= MDP_COLOR_NV16,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.mdp_color	= MDP_COLOR_NV61,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.walign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.mdp_color	= MDP_COLOR_I420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.mdp_color	= MDP_COLOR_YV12,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.walign		= 1,
		.halign		= 1,
		.flags		= MDP_FMT_FLAG_OUTPUT | MDP_FMT_FLAG_CAPTURE,
	}
};

static const u32 mt8195_mdp_mmsys_config_table[] = {
	[CONFIG_VPP0_HW_DCM_1ST_DIS0]    = 0,
	[CONFIG_VPP0_DL_IRELAY_WR]       = 1,
	[CONFIG_VPP1_HW_DCM_1ST_DIS0]    = 2,
	[CONFIG_VPP1_HW_DCM_1ST_DIS1]    = 3,
	[CONFIG_VPP1_HW_DCM_2ND_DIS0]    = 4,
	[CONFIG_VPP1_HW_DCM_2ND_DIS1]    = 5,
	[CONFIG_SVPP2_BUF_BF_RSZ_SWITCH] = 6,
	[CONFIG_SVPP3_BUF_BF_RSZ_SWITCH] = 7,
};

static const struct mtk_mdp_driver_data mt8183_mdp_driver_data = {
	.mdp_cfg = &mt8183_plat_cfg,
	.event = mt8183_mdp_event,
	.event_len = ARRAY_SIZE(mt8183_mdp_event),
	.comp_list = &mt8183_comp_list,
	.comp_data = mt8183_mdp_comp_data,
	.comp_data_len = ARRAY_SIZE(mt8183_mdp_comp_data),
	.comp_info = mt8183_comp_dt_info,
	.comp_info_len = ARRAY_SIZE(mt8183_comp_dt_info),
	.pipe_info = mt8183_pipe_info,
	.pipe_info_len = ARRAY_SIZE(mt8183_pipe_info),
	.format = mt8183_formats,
	.format_len = ARRAY_SIZE(mt8183_formats),
};

static const struct mtk_mdp_driver_data mt8195_mdp_driver_data = {
	.mdp_cfg = &mt8195_plat_cfg,
	.event = mt8195_mdp_event,
	.event_len = ARRAY_SIZE(mt8195_mdp_event),
	.comp_list = &mt8195_comp_list,
	.comp_data = mt8195_mdp_comp_data,
	.comp_data_len = ARRAY_SIZE(mt8195_mdp_comp_data),
	.comp_info = mt8195_comp_dt_info,
	.comp_info_len = ARRAY_SIZE(mt8195_comp_dt_info),
	.pipe_info = mt8195_pipe_info,
	.pipe_info_len = ARRAY_SIZE(mt8195_pipe_info),
	.format = mt8195_formats,
	.format_len = ARRAY_SIZE(mt8195_formats),
	.config_table = mt8195_mdp_mmsys_config_table,
};

static const struct of_device_id mdp_of_ids[] = {
	{ .compatible = "mediatek,mt8183-mdp3",
	  .data = &mt8183_mdp_driver_data,
	},
	{ .compatible = "mediatek,mt8195-mdp3",
	  .data = &mt8195_mdp_driver_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mdp_of_ids);

static struct platform_device *__get_pdev_by_name(struct platform_device *pdev,
						  char *ref_name)
{
	struct device *dev = &pdev->dev;
	struct device_node *mdp_node;
	struct platform_device *mdp_pdev;

	mdp_node = of_parse_phandle(dev->of_node, ref_name, 0);
	if (!mdp_node) {
		dev_err(dev, "can't get node %s\n", ref_name);
		return NULL;
	}

	mdp_pdev = of_find_device_by_node(mdp_node);
	of_node_put(mdp_node);
	if (WARN_ON(!mdp_pdev)) {
		dev_err(dev, "find %s pdev failed\n", ref_name);
		return NULL;
	}

	return mdp_pdev;
}

struct platform_device *mdp_get_plat_device(struct platform_device *pdev)
{
	if (!pdev)
		return NULL;

	return __get_pdev_by_name(pdev, "mediatek,mdp3");
}
EXPORT_SYMBOL_GPL(mdp_get_plat_device);

int mdp_vpu_get_locked(struct mdp_dev *mdp)
{
	int ret = 0;

	mutex_lock(&mdp->vpu_lock);
	if (mdp->vpu_count++ == 0) {
		ret = rproc_boot(mdp->rproc_handle);
		if (ret) {
			dev_err(&mdp->pdev->dev,
				"vpu_load_firmware failed %d\n", ret);
			goto err_load_vpu;
		}
		ret = mdp_vpu_register(mdp);
		if (ret) {
			dev_err(&mdp->pdev->dev,
				"mdp_vpu register failed %d\n", ret);
			goto err_reg_vpu;
		}
		ret = mdp_vpu_dev_init(&mdp->vpu, mdp->scp, &mdp->vpu_lock);
		if (ret) {
			dev_err(&mdp->pdev->dev,
				"mdp_vpu device init failed %d\n", ret);
			goto err_init_vpu;
		}
	}
	mutex_unlock(&mdp->vpu_lock);
	return 0;

err_init_vpu:
	mdp_vpu_unregister(mdp);
err_reg_vpu:
err_load_vpu:
	mdp->vpu_count--;
	return ret;
}

void mdp_vpu_put_locked(struct mdp_dev *mdp)
{
	mutex_lock(&mdp->vpu_lock);
	if (--mdp->vpu_count == 0) {
		mdp_vpu_dev_deinit(&mdp->vpu);
		mdp_vpu_unregister(mdp);
	}
	mutex_unlock(&mdp->vpu_lock);
}

void mdp_video_device_release(struct video_device *vdev)
{
	struct mdp_dev *mdp = (struct mdp_dev *)video_get_drvdata(vdev);

	scp_put(mdp->scp);

	destroy_workqueue(mdp->job_wq);
	destroy_workqueue(mdp->clock_wq);

	pm_runtime_disable(&mdp->pdev->dev);

	vb2_dma_contig_clear_max_seg_size(&mdp->pdev->dev);
	mdp_component_deinit(mdp);

#ifdef MDP_DEBUG
	mdp_debug_deinit();
#endif

	mdp_vpu_shared_mem_free(&mdp->vpu);
	v4l2_m2m_release(mdp->m2m_dev);
	kfree(mdp);
}

static int mdp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdp_dev *mdp;
	struct device_node *mdp_node;
	struct platform_device *mm_pdev, *mm_pdev2;
	u32 event_ofst;
	int ret, i, mutex_id, id;

	mdp = kzalloc(sizeof(*mdp), GFP_KERNEL);
	if (!mdp) {
		ret = -ENOMEM;
		goto err_return;
	}

	mdp->pdev = pdev;
	mdp->mdp_data = of_device_get_match_data(&pdev->dev);

	if (of_get_property(pdev->dev.of_node, "dma-ranges", NULL))
		dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	ret = of_property_read_u32(dev->of_node, "mediatek,mdp3-id", &id);
	if (ret) {
		dev_err(dev, "Failed to get mdp-id\n");
		goto err_return;
	}

	if (id != MDP_RDMA0_NODE) {
		platform_set_drvdata(pdev, mdp);
		goto success_return;
	}

	mm_pdev = __get_pdev_by_name(pdev, "mediatek,mmsys");
	if (!mm_pdev) {
		ret = -ENODEV;
		goto err_return;
	}
	mdp->mdp_mmsys = &mm_pdev->dev;

	mm_pdev2 = __get_pdev_by_name(pdev, "mediatek,mmsys2");
	if (!mm_pdev2)
		dev_err(dev, "Failed to get mdp mmsys2\n");
	else
		mdp->mdp_mmsys2 = &mm_pdev2->dev;

	mdp_node = of_parse_phandle(pdev->dev.of_node, "mediatek,mm-mutex", 0);
	if (!mdp_node) {
		ret = -ENODEV;
		goto err_return;
	}

	event_ofst = mdp->mdp_data->mdp_cfg->gce_event_offset;
	for (i = 0; i < mdp->mdp_data->event_len; i++) {
		s32 event_id;

		if (!dev)
			return -EINVAL;

		if (of_property_read_u32_index(mdp_node,
					       "mediatek,gce-events",
					       i + event_ofst, &event_id)) {
			dev_err(dev, "can't parse gce-events property");

			return -ENODEV;
		}
		mdp->event[i] = (event_id < 0) ? -i : event_id;
		dev_dbg(dev, "Get event id:%d\n", mdp->event[i]);
	}

	mm_pdev = of_find_device_by_node(mdp_node);
	of_node_put(mdp_node);
	if (WARN_ON(!mm_pdev)) {
		ret = -ENODEV;
		goto err_return;
	}

	mdp_node = of_parse_phandle(pdev->dev.of_node, "mediatek,mm-mutex2", 0);
	if (!mdp_node) {
		dev_err(dev, "Failed to get mdp mm-mutex2\n");
	} else {
		mm_pdev2 = of_find_device_by_node(mdp_node);
		of_node_put(mdp_node);
		if (WARN_ON(!mm_pdev2)) {
			ret = -ENODEV;
			goto err_return;
		}
	}

	for (i = 0; i < mdp->mdp_data->pipe_info_len; i++) {
		mutex_id = mdp->mdp_data->pipe_info[i].mutex_id;

		if (mdp->mdp_data->pipe_info[i].mmsys_id != 0) {
			if (mdp->mdp_mutex2[mutex_id])
				continue;
			mdp->mdp_mutex2[mutex_id] =
				mtk_mutex_mdp_get(&mm_pdev2->dev,
						  mdp->mdp_data->pipe_info[i].pipe_id);

			if (!mdp->mdp_mutex2[mutex_id]) {
				ret = -ENODEV;
				goto err_return;
			}
		} else {
			if (mdp->mdp_mutex[mutex_id])
				continue;
			mdp->mdp_mutex[mutex_id] =
				mtk_mutex_mdp_get(&mm_pdev->dev,
						  mdp->mdp_data->pipe_info[i].pipe_id);

			if (!mdp->mdp_mutex[mutex_id]) {
				ret = -ENODEV;
				goto err_return;
			}
		}
	}

	ret = mdp_component_init(mdp);
	if (ret) {
		dev_err(dev, "Failed to initialize mdp components\n");
		goto err_return;
	}

	mdp->job_wq = alloc_workqueue(MDP_MODULE_NAME, WQ_FREEZABLE, 0);
	if (!mdp->job_wq) {
		dev_err(dev, "Unable to create job workqueue\n");
		ret = -ENOMEM;
		goto err_deinit_comp;
	}

	mdp->clock_wq = alloc_workqueue(MDP_MODULE_NAME "-clock", WQ_FREEZABLE,
					0);
	if (!mdp->clock_wq) {
		dev_err(dev, "Unable to create clock workqueue\n");
		ret = -ENOMEM;
		goto err_destroy_job_wq;
	}

	mdp->scp = scp_get(pdev);
	if (!mdp->scp) {
		dev_err(&pdev->dev, "Could not get scp device\n");
		ret = -ENODEV;
		goto err_destroy_clock_wq;
	}

	mdp->rproc_handle = scp_get_rproc(mdp->scp);
	dev_dbg(&pdev->dev, "MDP rproc_handle: %pK", mdp->rproc_handle);

	mutex_init(&mdp->vpu_lock);
	mutex_init(&mdp->m2m_lock);

	mdp->cmdq_clt[0] = cmdq_mbox_create(dev, 0);
	if (IS_ERR(mdp->cmdq_clt[0])) {
		ret = PTR_ERR(mdp->cmdq_clt[0]);
		goto err_put_scp;
	}

	if (mdp->mdp_data->mdp_cfg->support_dual_pipe) {
		mdp->cmdq_clt[1] = cmdq_mbox_create(dev, 1);
		if (IS_ERR(mdp->cmdq_clt[1])) {
			ret = PTR_ERR(mdp->cmdq_clt[1]);
			goto err_mbox_destroy;
		}
	}

	init_waitqueue_head(&mdp->callback_wq);
	ida_init(&mdp->mdp_ida);
	platform_set_drvdata(pdev, mdp);
#ifdef MDP_DEBUG
	mdp_debug_init(pdev);
#endif

	vb2_dma_contig_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));

	ret = v4l2_device_register(dev, &mdp->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_dual_mbox_destroy;
	}

	ret = mdp_m2m_device_register(mdp);
	if (ret) {
		v4l2_err(&mdp->v4l2_dev, "Failed to register m2m device\n");
		goto err_unregister_device;
	}

success_return:
	dev_dbg(dev, "mdp-%d registered successfully\n", pdev->id);
	return 0;

err_unregister_device:
	v4l2_device_unregister(&mdp->v4l2_dev);
err_dual_mbox_destroy:
	if (mdp->mdp_data->mdp_cfg->support_dual_pipe)
		cmdq_mbox_destroy(mdp->cmdq_clt[1]);
err_mbox_destroy:
	cmdq_mbox_destroy(mdp->cmdq_clt[0]);
err_put_scp:
	scp_put(mdp->scp);
err_destroy_clock_wq:
	destroy_workqueue(mdp->clock_wq);
err_destroy_job_wq:
	destroy_workqueue(mdp->job_wq);
err_deinit_comp:
	mdp_component_deinit(mdp);
err_return:
	kfree(mdp);
	dev_dbg(dev, "Errno %d\n", ret);
	return ret;
}

static int mdp_remove(struct platform_device *pdev)
{
	struct mdp_dev *mdp = platform_get_drvdata(pdev);

	v4l2_device_unregister(&mdp->v4l2_dev);

	dev_dbg(&pdev->dev, "%s driver unloaded\n", pdev->name);
	return 0;
}

static int __maybe_unused mdp_suspend(struct device *dev)
{
	struct mdp_dev *mdp = dev_get_drvdata(dev);
	int ret;

	if (MDP_STAGE_ERROR(mdp->stage_flag[0]) || MDP_STAGE_ERROR(mdp->stage_flag[1]))
		dev_err(dev, "suspend enter : stage flag 1st %x, 2nd %x\n", mdp->stage_flag[0], mdp->stage_flag[1]);
	atomic_set(&mdp->suspended, 1);

	if (MDP_STAGE_ERROR(mdp->stage_flag[0]) || MDP_STAGE_ERROR(mdp->stage_flag[1]))
		dev_err(dev, "suspend check job : stage flag 1st %x, 2nd %x\n", mdp->stage_flag[0], mdp->stage_flag[1]);
	if (atomic_read(&mdp->job_count)) {
		ret = wait_event_timeout(mdp->callback_wq,
					 !atomic_read(&mdp->job_count),
					 2 * HZ);
		if (ret == 0) {
			dev_err(dev,
				"%s:flushed cmdq task incomplete, count=%d\n",
				__func__, atomic_read(&mdp->job_count));
			if (MDP_STAGE_ERROR(mdp->stage_flag[0]) || MDP_STAGE_ERROR(mdp->stage_flag[1]))
				dev_err(dev, "suspend timeout : stage flag 1st %x, 2nd %x\n", mdp->stage_flag[0], mdp->stage_flag[1]);
			return -EBUSY;
		}
	}

	if (MDP_STAGE_ERROR(mdp->stage_flag[0]) || MDP_STAGE_ERROR(mdp->stage_flag[1]))
		dev_err(dev, "suspend done : stage flag 1st %x, 2nd %x\n", mdp->stage_flag[0], mdp->stage_flag[1]);

	return 0;
}

static int __maybe_unused mdp_resume(struct device *dev)
{
	struct mdp_dev *mdp = dev_get_drvdata(dev);

	if (MDP_STAGE_ERROR(mdp->stage_flag[0]) || MDP_STAGE_ERROR(mdp->stage_flag[1]))
		dev_err(dev, "resume enter : stage flag 1st %x, 2nd %x\n", mdp->stage_flag[0], mdp->stage_flag[1]);
	atomic_set(&mdp->suspended, 0);

	if (MDP_STAGE_ERROR(mdp->stage_flag[0]) || MDP_STAGE_ERROR(mdp->stage_flag[1]))
		dev_err(dev, "resume done : stage flag 1st %x, 2nd %x\n", mdp->stage_flag[0], mdp->stage_flag[1]);

	return 0;
}

static const struct dev_pm_ops mdp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mdp_suspend, mdp_resume)
};

static struct platform_driver mdp_driver = {
	.probe		= mdp_probe,
	.remove		= mdp_remove,
	.driver = {
		.name	= MDP_MODULE_NAME,
		.pm	= &mdp_pm_ops,
		.of_match_table = of_match_ptr(mdp_of_ids),
	},
};

module_platform_driver(mdp_driver);

MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek image processor 3 driver");
MODULE_LICENSE("GPL v2");
