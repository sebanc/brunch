/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_CCORR_H__
#define __MDP_REG_CCORR_H__

#include "mmsys_reg_base.h"

#define MDP_CCORR_EN                0x000
#define MDP_CCORR_RESET             0x004
#define MDP_CCORR_INTEN             0x008
#define MDP_CCORR_INTSTA            0x00c
#define MDP_CCORR_STATUS            0x010
#define MDP_CCORR_CFG               0x020
#define MDP_CCORR_INPUT_COUNT       0x024
#define MDP_CCORR_OUTPUT_COUNT      0x028
#define MDP_CCORR_CHKSUM            0x02c
#define MDP_CCORR_SIZE              0x030
#define MDP_CCORR_Y2R_00            0x034
#define MDP_CCORR_Y2R_01            0x038
#define MDP_CCORR_Y2R_02            0x03c
#define MDP_CCORR_Y2R_03            0x040
#define MDP_CCORR_Y2R_04            0x044
#define MDP_CCORR_Y2R_05            0x048
#define MDP_CCORR_R2Y_00            0x04c
#define MDP_CCORR_R2Y_01            0x050
#define MDP_CCORR_R2Y_02            0x054
#define MDP_CCORR_R2Y_03            0x058
#define MDP_CCORR_R2Y_04            0x05c
#define MDP_CCORR_R2Y_05            0x060
#define MDP_CCORR_COEF_0            0x080
#define MDP_CCORR_COEF_1            0x084
#define MDP_CCORR_COEF_2            0x088
#define MDP_CCORR_COEF_3            0x08c
#define MDP_CCORR_COEF_4            0x090
#define MDP_CCORR_SHADOW            0x0a0
#define MDP_CCORR_DUMMY_REG         0x0c0
#define MDP_CCORR_ATPG              0x0fc

/* MASK */
#define MDP_CCORR_EN_MASK           0x00000001
#define MDP_CCORR_RESET_MASK        0x00000001
#define MDP_CCORR_INTEN_MASK        0x00000003
#define MDP_CCORR_INTSTA_MASK       0x00000003
#define MDP_CCORR_STATUS_MASK       0xfffffff3
#define MDP_CCORR_CFG_MASK          0x70001317
#define MDP_CCORR_INPUT_COUNT_MASK  0x1fff1fff
#define MDP_CCORR_OUTPUT_COUNT_MASK 0x1fff1fff
#define MDP_CCORR_CHKSUM_MASK       0xffffffff
#define MDP_CCORR_SIZE_MASK         0x1fff1fff
#define MDP_CCORR_Y2R_00_MASK       0x01ff01ff
#define MDP_CCORR_Y2R_01_MASK       0x1fff01ff
#define MDP_CCORR_Y2R_02_MASK       0x1fff1fff
#define MDP_CCORR_Y2R_03_MASK       0x1fff1fff
#define MDP_CCORR_Y2R_04_MASK       0x1fff1fff
#define MDP_CCORR_Y2R_05_MASK       0x1fff1fff
#define MDP_CCORR_R2Y_00_MASK       0x01ff01ff
#define MDP_CCORR_R2Y_01_MASK       0x07ff01ff
#define MDP_CCORR_R2Y_02_MASK       0x07ff07ff
#define MDP_CCORR_R2Y_03_MASK       0x07ff07ff
#define MDP_CCORR_R2Y_04_MASK       0x07ff07ff
#define MDP_CCORR_R2Y_05_MASK       0x07ff07ff
#define MDP_CCORR_COEF_0_MASK       0x1fff1fff
#define MDP_CCORR_COEF_1_MASK       0x1fff1fff
#define MDP_CCORR_COEF_2_MASK       0x1fff1fff
#define MDP_CCORR_COEF_3_MASK       0x1fff1fff
#define MDP_CCORR_COEF_4_MASK       0x1fff1fff
#define MDP_CCORR_SHADOW_MASK       0x00000007
#define MDP_CCORR_DUMMY_REG_MASK    0xffffffff
#define MDP_CCORR_ATPG_MASK         0x00000003

#endif  // __MDP_REG_CCORR_H__
