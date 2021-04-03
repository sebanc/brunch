/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_WROT_H__
#define __MDP_REG_WROT_H__

#include "mmsys_reg_base.h"

#define VIDO_CTRL                   0x000
#define VIDO_DMA_PERF               0x004
#define VIDO_MAIN_BUF_SIZE          0x008
#define VIDO_SOFT_RST               0x010
#define VIDO_SOFT_RST_STAT          0x014
#define VIDO_INT_EN                 0x018
#define VIDO_INT                    0x01c
#define VIDO_CROP_OFST              0x020
#define VIDO_TAR_SIZE               0x024
#define VIDO_BASE_ADDR              0xf00
#define VIDO_OFST_ADDR              0x02c
#define VIDO_STRIDE                 0x030
#define VIDO_BASE_ADDR_C            0xf04
#define VIDO_OFST_ADDR_C            0x038
#define VIDO_STRIDE_C               0x03c
#define VIDO_DITHER                 0x054
#define VIDO_BASE_ADDR_V            0xf08
#define VIDO_OFST_ADDR_V            0x068
#define VIDO_STRIDE_V               0x06c
#define VIDO_RSV_1                  0x070
#define VIDO_DMA_PREULTRA           0x074
#define VIDO_IN_SIZE                0x078
#define VIDO_ROT_EN                 0x07c
#define VIDO_FIFO_TEST              0x080
#define VIDO_MAT_CTRL               0x084
#define VIDO_MAT_RMY                0x088
#define VIDO_MAT_RMV                0x08c
#define VIDO_MAT_GMY                0x090
#define VIDO_MAT_BMY                0x094
#define VIDO_MAT_BMV                0x098
#define VIDO_MAT_PREADD             0x09c
#define VIDO_MAT_POSTADD            0x0a0
#define VIDO_DITHER_00              0x0a4
#define VIDO_DITHER_02              0x0ac
#define VIDO_DITHER_03              0x0b0
#define VIDO_DITHER_04              0x0b4
#define VIDO_DITHER_05              0x0b8
#define VIDO_DITHER_06              0x0bc
#define VIDO_DITHER_07              0x0c0
#define VIDO_DITHER_08              0x0c4
#define VIDO_DITHER_09              0x0c8
#define VIDO_DITHER_10              0x0cc
#define VIDO_DEBUG                  0x0d0
#define VIDO_ARB_SW_CTL             0x0d4
#define MDP_WROT_TRACK_CTL          0x0e0
#define MDP_WROT_TRACK_WINDOW       0x0e4
#define MDP_WROT_TRACK_TARGET       0x0e8
#define MDP_WROT_TRACK_STOP         0x0ec
#define MDP_WROT_TRACK_PROC_CNT0    0x0f0
#define MDP_WROT_TRACK_PROC_CNT1    0x0f4

/* MASK */
#define VIDO_CTRL_MASK                  0xf530711f
#define VIDO_DMA_PERF_MASK              0x3fffffff
#define VIDO_MAIN_BUF_SIZE_MASK         0x1fff7f77
#define VIDO_SOFT_RST_MASK              0x00000001
#define VIDO_SOFT_RST_STAT_MASK         0x00000001
#define VIDO_INT_EN_MASK                0x00003f07
#define VIDO_INT_MASK                   0x00000007
#define VIDO_CROP_OFST_MASK             0x1fff1fff
#define VIDO_TAR_SIZE_MASK              0x1fff1fff
#define VIDO_BASE_ADDR_MASK             0xffffffff
#define VIDO_OFST_ADDR_MASK             0x0fffffff
#define VIDO_STRIDE_MASK                0x0000ffff
#define VIDO_BASE_ADDR_C_MASK           0xffffffff
#define VIDO_OFST_ADDR_C_MASK           0x0fffffff
#define VIDO_STRIDE_C_MASK              0x0000ffff
#define VIDO_DITHER_MASK                0xff000001
#define VIDO_BASE_ADDR_V_MASK           0xffffffff
#define VIDO_OFST_ADDR_V_MASK           0x0fffffff
#define VIDO_STRIDE_V_MASK              0x0000ffff
#define VIDO_RSV_1_MASK                 0xffffffff
#define VIDO_DMA_PREULTRA_MASK          0x00ffffff
#define VIDO_IN_SIZE_MASK               0x1fff1fff
#define VIDO_ROT_EN_MASK                0x00000001
#define VIDO_FIFO_TEST_MASK             0x00000fff
#define VIDO_MAT_CTRL_MASK              0x000000f3
#define VIDO_MAT_RMY_MASK               0x1fff1fff
#define VIDO_MAT_RMV_MASK               0x1fff1fff
#define VIDO_MAT_GMY_MASK               0x1fff1fff
#define VIDO_MAT_BMY_MASK               0x1fff1fff
#define VIDO_MAT_BMV_MASK               0x00001fff
#define VIDO_MAT_PREADD_MASK            0x1ff7fdff
#define VIDO_MAT_POSTADD_MASK           0x1ff7fdff
#define VIDO_DITHER_00_MASK             0x0000ff3f
#define VIDO_DITHER_02_MASK             0xffff3fff
#define VIDO_DITHER_03_MASK             0x0000003f
#define VIDO_DITHER_04_MASK             0xbfffffff
#define VIDO_DITHER_05_MASK             0xffff7fff
#define VIDO_DITHER_06_MASK             0x003ff773
#define VIDO_DITHER_07_MASK             0x00007777
#define VIDO_DITHER_08_MASK             0x00007777
#define VIDO_DITHER_09_MASK             0x00007777
#define VIDO_DITHER_10_MASK             0x0001ffff
#define VIDO_DEBUG_MASK                 0xffffffff
#define VIDO_ARB_SW_CTL_MASK            0x00000007
#define MDP_WROT_TRACK_CTL_MASK         0x0000001f
#define MDP_WROT_TRACK_WINDOW_MASK      0x00000fff
#define MDP_WROT_TRACK_TARGET_MASK      0x00ffffff
#define MDP_WROT_TRACK_STOP_MASK        0x00ffffff
#define MDP_WROT_TRACK_PROC_CNT0_MASK   0xffffffff
#define MDP_WROT_TRACK_PROC_CNT1_MASK   0x00000001

#endif  // __MDP_REG_WROT_H__
