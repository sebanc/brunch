/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MMSYS_CONFIG_H__
#define __MMSYS_CONFIG_H__

#include "mmsys_reg_base.h"

#define MMSYS_INTEN                   0x000
#define MMSYS_INTSTA                  0x004
#define MJC_APB_TX_CON                0x00c

#define ISP_MOUT_EN                   0xf80
#define MDP_RDMA0_MOUT_EN             0xf84
#define MDP_RDMA1_MOUT_EN             0xf88
#define MDP_PRZ0_MOUT_EN              0xf8c
#define MDP_PRZ1_MOUT_EN              0xf90
#define MDP_COLOR_MOUT_EN             0xf94
#define IPU_MOUT_EN                   0xf98
#define DISP_TO_WROT_SOUT_SEL         0xfa0
#define MDP_COLOR_IN_SOUT_SEL         0xfa4
#define MDP_PATH0_SOUT_SEL            0xfa8
#define MDP_PATH1_SOUT_SEL            0xfac
#define MDP_TDSHP_SOUT_SEL            0xfb0

#define DISP_OVL0_MOUT_EN             0xf00
#define DISP_OVL0_2L_MOUT_EN          0xf04
#define DISP_OVL1_2L_MOUT_EN          0xf08
#define DISP_DITHER0_MOUT_EN          0xf0c
#define DISP_RSZ_MOUT_EN              0xf10

#define MMSYS_MOUT_RST                0x048
#define MDP_PRZ0_SEL_IN               0xfc0
#define MDP_PRZ1_SEL_IN               0xfc4
#define MDP_TDSHP_SEL_IN              0xfc8
#define DISP_WDMA0_SEL_IN             0xfcc
#define MDP_WROT0_SEL_IN              0xfd0
#define MDP_WDMA_SEL_IN               0xfd4
#define MDP_COLOR_OUT_SEL_IN          0xfd8
#define MDP_COLOR_SEL_IN              0xfdc
#define MDP_PATH0_SEL_IN              0xfe0
#define MDP_PATH1_SEL_IN              0xfe4

#define DISP_COLOR_OUT_SEL_IN         0xf20
#define DISP_PATH0_SEL_IN             0xf24
#define DISP_WDMA0_PRE_SEL_IN         0xf28
#define DSI0_SEL_IN                   0xf2c
#define DSI1_SEL_IN                   0xf30
#define DISP_OVL0_SEL_IN              0xf34
#define DISP_OVL0_2L_SEL_IN           0xf38
#define OVL_TO_RSZ_SEL_IN             0xf3c
#define OVL_TO_WDMA_SEL_IN            0xf40
#define OVL_TO_WROT_SEL_IN            0xf44
#define DISP_RSZ_SEL_IN               0xf48
#define DISP_RDMA0_SOUT_SEL_IN        0xf50
#define DISP_RDMA1_SOUT_SEL_IN        0xf54
#define MDP_TO_DISP0_SOUT_SEL_IN      0xf58
#define MDP_TO_DISP1_SOUT_SEL_IN      0xf5c
#define DISP_RDMA0_RSZ_IN_SOUT_SEL_IN 0xf60
#define DISP_RDMA0_RSZ_OUT_SEL_IN     0xf64
#define MDP_AAL_MOUT_EN               0xfe8
#define MDP_AAL_SEL_IN                0xfec
#define MDP_CCORR_SEL_IN              0xff0
#define MDP_CCORR_SOUT_SEL            0xff4

#define MMSYS_MISC                    0x0f0
#define MMSYS_SMI_LARB_SEL            0x0f4
#define MMSYS_SODI_REQ_MASK           0x0f8
#define MMSYS_CG_CON0                 0x100
#define MMSYS_CG_SET0                 0x104
#define MMSYS_CG_CLR0                 0x108
#define MMSYS_CG_CON1                 0x110
#define MMSYS_CG_SET1                 0x114
#define MMSYS_CG_CLR1                 0x118
#define MMSYS_HW_DCM_DIS0             0x120
#define MMSYS_HW_DCM_DIS_SET0         0x124
#define MMSYS_HW_DCM_DIS_CLR0         0x128
#define MMSYS_HW_DCM_DIS1             0x130
#define MMSYS_HW_DCM_DIS_SET1         0x134
#define MMSYS_HW_DCM_DIS_CLR1         0x138
#define MMSYS_HW_DCM_EVENT_CTL1       0x13c
#define MMSYS_SW0_RST_B               0x140
#define MMSYS_SW1_RST_B               0x144
#define MMSYS_LCM_RST_B               0x150
#define LARB6_AXI_ASIF_CFG_WD         0x180
#define LARB6_AXI_ASIF_CFG_RD         0x184
#define PROC_TRACK_EMI_BUSY_CON       0x190
#define DISP_FAKE_ENG_EN              0x200
#define DISP_FAKE_ENG_RST             0x204
#define DISP_FAKE_ENG_CON0            0x208
#define DISP_FAKE_ENG_CON1            0x20c
#define DISP_FAKE_ENG_RD_ADDR         0x210
#define DISP_FAKE_ENG_WR_ADDR         0x214
#define DISP_FAKE_ENG_STATE           0x218
#define DISP_FAKE_ENG2_EN             0x220
#define DISP_FAKE_ENG2_RST            0x224
#define DISP_FAKE_ENG2_CON0           0x228
#define DISP_FAKE_ENG2_CON1           0x22c
#define DISP_FAKE_ENG2_RD_ADDR        0x230
#define DISP_FAKE_ENG2_WR_ADDR        0x234
#define DISP_FAKE_ENG2_STATE          0x238
#define MMSYS_MBIST_CON               0x800
#define MMSYS_MBIST_DONE              0x804
#define MMSYS_MBIST_HOLDB             0x808
#define MMSYS_MBIST_MODE              0x80c
#define MMSYS_MBIST_FAIL0             0x810
#define MMSYS_MBIST_FAIL1             0x814
#define MMSYS_MBIST_FAIL2             0x818
#define MMSYS_MBIST_DEBUG             0x820
#define MMSYS_MBIST_DIAG_SCANOUT      0x824
#define MMSYS_MBIST_PRE_FUSE          0x828
#define MMSYS_MBIST_BSEL0             0x82c
#define MMSYS_MBIST_BSEL1             0x830
#define MMSYS_MBIST_BSEL2             0x834
#define MMSYS_MBIST_BSEL3             0x838
#define MMSYS_MBIST_HDEN              0x83c
#define MDP_RDMA0_MEM_DELSEL          0x840
#define MDP_RDMA1_MEM_DELSEL          0x844
#define MDP_RSZ_MEM_DELSEL            0x848
#define MDP_TDSHP_MEM_DELSEL          0x84c
#define MDP_AAL_MEM_DELSEL            0x850

#define MDP_WROT0_MEM_DELSEL          0x854
#define MDP_WDMA_MEM_DELSEL           0x858
#define DISP_OVL_MEM_DELSEL           0x85c
#define DISP_OVL_2L_MEM_DELSEL        0x860
#define DISP_RDMA_MEM_DELSEL          0x864
#define DISP_WDMA0_MEM_DELSEL         0x868
#define DISP_GAMMA_MEM_DELSEL         0x870
#define DSI_MEM_DELSEL                0x874
#define DISP_SPLIT_MEM_DELSEL         0x878
#define DISP_DSC_MEM_DELSEL           0x87c
#define MMSYS_DEBUG_OUT_SEL           0x88c
#define MMSYS_MBIST_RP_RST_B          0x890
#define MMSYS_MBIST_RP_FAIL0          0x894
#define MMSYS_MBIST_RP_FAIL1          0x898
#define MMSYS_MBIST_RP_OK0            0x89c
#define MMSYS_MBIST_RP_OK1            0x8a0
#define MMSYS_DUMMY0                  0x8a4
#define MMSYS_DUMMY1                  0x8a8
#define MMSYS_DUMMY2                  0x8ac
#define MMSYS_DUMMY3                  0x8b0
#define DISP_DL_VALID_0               0x8b4
#define DISP_DL_VALID_1               0x8b8
#define DISP_DL_VALID_2               0x8bc
#define DISP_DL_READY_0               0x8c0
#define DISP_DL_READY_1               0x8c4
#define DISP_DL_READY_2               0x8C8
#define MDP_DL_VALID_0                0x8cc
#define MDP_DL_VALID_1                0x8d0
#define MDP_DL_READY_0                0x8d4
#define MDP_DL_READY_1                0x8d8
#define SMI_LARB0_GREQ                0x8dc
#define DISP_MOUT_MASK                0x8e0
#define DISP_MOUT_MASK1               0x8e4
#define MDP_MOUT_MASK                 0x8e8
#define MMSYS_POWER_READ              0x8ec
#define TOP_RELAY_FSM_RD              0x960
#define MDP_ASYNC_CFG_WD              0x934
#define MDP_ASYNC_CFG_RD              0x938
#define MDP_ASYNC_IPU_CFG_WD          0x93C
#define MDP_ASYNC_CFG_IPU_RD          0x940
#define MDP_ASYNC_CFG_OUT_RD          0x958
#define MDP_ASYNC_IPU_CFG_OUT_RD      0x95C
#define ISP_RELAY_CFG_WD              0x994
#define ISP_RELAY_CNT_RD              0x998
#define ISP_RELAY_CNT_LATCH_RD        0x99c
#define IPU_RELAY_CFG_WD              0x9a0
#define IPU_RELAY_CNT_RD              0x9a4
#define IPU_RELAY_CNT_LATCH_RD        0x9a8

/* MASK */
#define MMSYS_SW0_RST_B_MASK          0xffffffff
#define MMSYS_SW1_RST_B_MASK          0xffffffff
#define MDP_COLOR_IN_SOUT_SEL_MASK    0x0000000f
#define DISP_COLOR_OUT_SEL_IN_MASK    0xffffffff
#define MDP_ASYNC_CFG_WD_MASK         0xffffffff
#define MDP_ASYNC_IPU_CFG_WD_MASK     0xffffffff
#define MMSYS_HW_DCM_DIS0_MASK        0xffffffff
#define MMSYS_HW_DCM_DIS1_MASK        0xffffffff
#define MDP_ASYNC_CFG_WD_MASK         0xffffffff
#define ISP_RELAY_CFG_WD_MASK         0xffffffff
#define IPU_RELAY_CFG_WD_MASK         0xffffffff

#endif  // __MMSYS_CONFIG_H__
