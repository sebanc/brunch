/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_WDMA_H__
#define __MDP_REG_WDMA_H__

#include "mmsys_reg_base.h"

#define WDMA_INTEN              0x000
#define WDMA_INTSTA             0x004
#define WDMA_EN                 0x008
#define WDMA_RST                0x00c
#define WDMA_SMI_CON            0x010
#define WDMA_CFG                0x014
#define WDMA_SRC_SIZE           0x018
#define WDMA_CLIP_SIZE          0x01c
#define WDMA_CLIP_COORD         0x020
#define WDMA_DST_ADDR           0xf00
#define WDMA_DST_W_IN_BYTE      0x028
#define WDMA_ALPHA              0x02c
#define WDMA_BUF_CON1           0x038
#define WDMA_BUF_CON2           0x03c
#define WDMA_C00                0x040
#define WDMA_C02                0x044
#define WDMA_C10                0x048
#define WDMA_C12                0x04c
#define WDMA_C20                0x050
#define WDMA_C22                0x054
#define WDMA_PRE_ADD0           0x058
#define WDMA_PRE_ADD2           0x05c
#define WDMA_POST_ADD0          0x060
#define WDMA_POST_ADD2          0x064
#define WDMA_DST_U_ADDR         0xf04
#define WDMA_DST_V_ADDR         0xf08
#define WDMA_DST_UV_PITCH       0x078
#define WDMA_DST_ADDR_OFFSET    0x080
#define WDMA_DST_U_ADDR_OFFSET  0x084
#define WDMA_DST_V_ADDR_OFFSET  0x088
#define PROC_TRACK_CON_0        0x090
#define PROC_TRACK_CON_1        0x094
#define PROC_TRACK_CON_2        0x098
#define WDMA_FLOW_CTRL_DBG      0x0a0
#define WDMA_EXEC_DBG           0x0a4
#define WDMA_CT_DBG             0x0a8
#define WDMA_SMI_TRAFFIC_DBG    0x0ac
#define WDMA_PROC_TRACK_DBG_0   0x0b0
#define WDMA_PROC_TRACK_DBG_1   0x0b4
#define WDMA_DEBUG              0x0b8
#define WDMA_DUMMY              0x100
#define WDMA_DITHER_0           0xe00
#define WDMA_DITHER_5           0xe14
#define WDMA_DITHER_6           0xe18
#define WDMA_DITHER_7           0xe1c
#define WDMA_DITHER_8           0xe20
#define WDMA_DITHER_9           0xe24
#define WDMA_DITHER_10          0xe28
#define WDMA_DITHER_11          0xe2c
#define WDMA_DITHER_12          0xe30
#define WDMA_DITHER_13          0xe34
#define WDMA_DITHER_14          0xe38
#define WDMA_DITHER_15          0xe3c
#define WDMA_DITHER_16          0xe40
#define WDMA_DITHER_17          0xe44

/* MASK */
#define WDMA_INTEN_MASK             0x00000003
#define WDMA_INTSTA_MASK            0x00000003
#define WDMA_EN_MASK                0x00000001
#define WDMA_RST_MASK               0x00000001
#define WDMA_SMI_CON_MASK           0x0fffffff
#define WDMA_CFG_MASK               0xff03bff0
#define WDMA_SRC_SIZE_MASK          0x3fff3fff
#define WDMA_CLIP_SIZE_MASK         0x3fff3fff
#define WDMA_CLIP_COORD_MASK        0x3fff3fff
#define WDMA_DST_ADDR_MASK          0xffffffff
#define WDMA_DST_W_IN_BYTE_MASK     0x0000ffff
#define WDMA_ALPHA_MASK             0x800000ff
#define WDMA_BUF_CON1_MASK          0xd1ff01ff
#define WDMA_BUF_CON2_MASK          0xffffffff
#define WDMA_C00_MASK               0x1fff1fff
#define WDMA_C02_MASK               0x00001fff
#define WDMA_C10_MASK               0x1fff1fff
#define WDMA_C12_MASK               0x00001fff
#define WDMA_C20_MASK               0x1fff1fff
#define WDMA_C22_MASK               0x00001fff
#define WDMA_PRE_ADD0_MASK          0x01ff01ff
#define WDMA_PRE_ADD2_MASK          0x000001ff
#define WDMA_POST_ADD0_MASK         0x01ff01ff
#define WDMA_POST_ADD2_MASK         0x000001ff
#define WDMA_DST_U_ADDR_MASK        0xffffffff
#define WDMA_DST_V_ADDR_MASK        0xffffffff
#define WDMA_DST_UV_PITCH_MASK      0x0000ffff
#define WDMA_DST_ADDR_OFFSET_MASK   0x0fffffff
#define WDMA_DST_U_ADDR_OFFSET_MASK 0x0fffffff
#define WDMA_DST_V_ADDR_OFFSET_MASK 0x0fffffff
#define PROC_TRACK_CON_0_MASK       0x70000fff
#define PROC_TRACK_CON_1_MASK       0x00ffffff
#define PROC_TRACK_CON_2_MASK       0x00ffffff
#define WDMA_FLOW_CTRL_DBG_MASK     0x0000f3ff
#define WDMA_EXEC_DBG_MASK          0x003f003f
#define WDMA_CT_DBG_MASK            0x3fff3fff
#define WDMA_SMI_TRAFFIC_DBG_MASK   0xffffffff
#define WDMA_PROC_TRACK_DBG_0_MASK  0xffffffff
#define WDMA_PROC_TRACK_DBG_1_MASK  0xffffffff
#define WDMA_DEBUG_MASK             0xffffffff
#define WDMA_DUMMY_MASK             0xffffffff
#define WDMA_DITHER_0_MASK          0x0111ff11
#define WDMA_DITHER_5_MASK          0x0000ffff
#define WDMA_DITHER_6_MASK          0x0001f3ff
#define WDMA_DITHER_7_MASK          0x00000333
#define WDMA_DITHER_8_MASK          0x03ff0001
#define WDMA_DITHER_9_MASK          0x03ff03ff
#define WDMA_DITHER_10_MASK         0x00000733
#define WDMA_DITHER_11_MASK         0x00003331
#define WDMA_DITHER_12_MASK         0xffff0031
#define WDMA_DITHER_13_MASK         0x00000777
#define WDMA_DITHER_14_MASK         0x00000371
#define WDMA_DITHER_15_MASK         0x77770001
#define WDMA_DITHER_16_MASK         0x77777777
#define WDMA_DITHER_17_MASK         0x0001ffff

#endif  // __MDP_REG_WDMA_H__
