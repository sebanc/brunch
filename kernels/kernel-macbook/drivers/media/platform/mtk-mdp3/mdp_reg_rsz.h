/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_RSZ_H__
#define __MDP_REG_RSZ_H__

#include "mmsys_reg_base.h"

#define PRZ_ENABLE                                        0x000
#define PRZ_CONTROL_1                                     0x004
#define PRZ_CONTROL_2                                     0x008
#define PRZ_INT_FLAG                                      0x00c
#define PRZ_INPUT_IMAGE                                   0x010
#define PRZ_OUTPUT_IMAGE                                  0x014
#define PRZ_HORIZONTAL_COEFF_STEP                         0x018
#define PRZ_VERTICAL_COEFF_STEP                           0x01c
#define PRZ_LUMA_HORIZONTAL_INTEGER_OFFSET                0x020
#define PRZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET               0x024
#define PRZ_LUMA_VERTICAL_INTEGER_OFFSET                  0x028
#define PRZ_LUMA_VERTICAL_SUBPIXEL_OFFSET                 0x02c
#define PRZ_CHROMA_HORIZONTAL_INTEGER_OFFSET              0x030
#define PRZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET             0x034
#define PRZ_RSV                                           0x040
#define PRZ_DEBUG_SEL                                     0x044
#define PRZ_DEBUG                                         0x048
#define PRZ_TAP_ADAPT                                     0x04c
#define PRZ_IBSE_SOFTCLIP                                 0x050
#define PRZ_IBSE_YLEVEL_1                                 0x054
#define PRZ_IBSE_YLEVEL_2                                 0x058
#define PRZ_IBSE_YLEVEL_3                                 0x05c
#define PRZ_IBSE_YLEVEL_4                                 0x060
#define PRZ_IBSE_YLEVEL_5                                 0x064
#define PRZ_IBSE_GAINCONTROL_1                            0x068
#define PRZ_IBSE_GAINCONTROL_2                            0x06c
#define PRZ_DEMO_IN_HMASK                                 0x070
#define PRZ_DEMO_IN_VMASK                                 0x074
#define PRZ_DEMO_OUT_HMASK                                0x078
#define PRZ_DEMO_OUT_VMASK                                0x07c
#define PRZ_ATPG                                          0x0fc
#define PRZ_PAT1_GEN_SET                                  0x100
#define PRZ_PAT1_GEN_FRM_SIZE                             0x104
#define PRZ_PAT1_GEN_COLOR0                               0x108
#define PRZ_PAT1_GEN_COLOR1                               0x10c
#define PRZ_PAT1_GEN_COLOR2                               0x110
#define PRZ_PAT1_GEN_POS                                  0x114
#define PRZ_PAT1_GEN_TILE_POS                             0x124
#define PRZ_PAT1_GEN_TILE_OV                              0x128
#define PRZ_PAT2_GEN_SET                                  0x200
#define PRZ_PAT2_GEN_COLOR0                               0x208
#define PRZ_PAT2_GEN_COLOR1                               0x20c
#define PRZ_PAT2_GEN_POS                                  0x214
#define PRZ_PAT2_GEN_CURSOR_RB0                           0x218
#define PRZ_PAT2_GEN_CURSOR_RB1                           0x21c
#define PRZ_PAT2_GEN_TILE_POS                             0x224
#define PRZ_PAT2_GEN_TILE_OV                              0x228

/* MASK */
#define PRZ_ENABLE_MASK                                   0x00010001
#define PRZ_CONTROL_1_MASK                                0xfffffff3
#define PRZ_CONTROL_2_MASK                                0x0ffffaff
#define PRZ_INT_FLAG_MASK                                 0x00000033
#define PRZ_INPUT_IMAGE_MASK                              0xffffffff
#define PRZ_OUTPUT_IMAGE_MASK                             0xffffffff
#define PRZ_HORIZONTAL_COEFF_STEP_MASK                    0x007fffff
#define PRZ_VERTICAL_COEFF_STEP_MASK                      0x007fffff
#define PRZ_LUMA_HORIZONTAL_INTEGER_OFFSET_MASK           0x0000ffff
#define PRZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET_MASK          0x001fffff
#define PRZ_LUMA_VERTICAL_INTEGER_OFFSET_MASK             0x0000ffff
#define PRZ_LUMA_VERTICAL_SUBPIXEL_OFFSET_MASK            0x001fffff
#define PRZ_CHROMA_HORIZONTAL_INTEGER_OFFSET_MASK         0x0000ffff
#define PRZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET_MASK        0x001fffff
#define PRZ_RSV_MASK                                      0xffffffff
#define PRZ_DEBUG_SEL_MASK                                0x0000000f
#define PRZ_DEBUG_MASK                                    0xffffffff
#define PRZ_TAP_ADAPT_MASK                                0x03ffffff
#define PRZ_IBSE_SOFTCLIP_MASK                            0x000fffff
#define PRZ_IBSE_YLEVEL_1_MASK                            0xffffffff
#define PRZ_IBSE_YLEVEL_2_MASK                            0xffffffff
#define PRZ_IBSE_YLEVEL_3_MASK                            0xffffffff
#define PRZ_IBSE_YLEVEL_4_MASK                            0xffffffff
#define PRZ_IBSE_YLEVEL_5_MASK                            0x0000ff3f
#define PRZ_IBSE_GAINCONTROL_1_MASK                       0xffffffff
#define PRZ_IBSE_GAINCONTROL_2_MASK                       0x0fffff0f
#define PRZ_DEMO_IN_HMASK_MASK                            0xffffffff
#define PRZ_DEMO_IN_VMASK_MASK                            0xffffffff
#define PRZ_DEMO_OUT_HMASK_MASK                           0xffffffff
#define PRZ_DEMO_OUT_VMASK_MASK                           0xffffffff
#define PRZ_ATPG_MASK                                     0x00000003
#define PRZ_PAT1_GEN_SET_MASK                             0x00ff00fd
#define PRZ_PAT1_GEN_FRM_SIZE_MASK                        0x1fff1fff
#define PRZ_PAT1_GEN_COLOR0_MASK                          0x00ff00ff
#define PRZ_PAT1_GEN_COLOR1_MASK                          0x00ff00ff
#define PRZ_PAT1_GEN_COLOR2_MASK                          0x00ff00ff
#define PRZ_PAT1_GEN_POS_MASK                             0x1fff1fff
#define PRZ_PAT1_GEN_TILE_POS_MASK                        0x1fff1fff
#define PRZ_PAT1_GEN_TILE_OV_MASK                         0x0000ffff
#define PRZ_PAT2_GEN_SET_MASK                             0x00ff0003
#define PRZ_PAT2_GEN_COLOR0_MASK                          0x00ff00ff
#define PRZ_PAT2_GEN_COLOR1_MASK                          0x000000ff
#define PRZ_PAT2_GEN_POS_MASK                             0x1fff1fff
#define PRZ_PAT2_GEN_CURSOR_RB0_MASK                      0x00ff00ff
#define PRZ_PAT2_GEN_CURSOR_RB1_MASK                      0x000000ff
#define PRZ_PAT2_GEN_TILE_POS_MASK                        0x1fff1fff
#define PRZ_PAT2_GEN_TILE_OV_MASK                         0x0000ffff

#endif // __MDP_REG_RSZ_H__
