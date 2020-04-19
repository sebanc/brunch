/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MMSYS_MUTEX_H__
#define __MMSYS_MUTEX_H__

#include "mmsys_reg_base.h"
#include "mdp-platform.h"

#define MM_MUTEX_INTEN              0x00
#define MM_MUTEX_INTSTA             0x04
#define MM_MUTEX_CFG                0x08

#define MM_MUTEX_EN                 (0x20 + mutex_id * 0x20)
#define MM_MUTEX_GET                (0x24 + mutex_id * 0x20)
#define MM_MUTEX_RST                (0x28 + mutex_id * 0x20)
#define MM_MUTEX_MOD                (MM_MUTEX_MOD_OFFSET + mutex_id * 0x20)
#define MM_MUTEX_SOF                (MM_MUTEX_SOF_OFFSET + mutex_id * 0x20)

// MASK
#define MM_MUTEX_INTEN_MASK         0x0fff
#define MM_MUTEX_INTSTA_MASK        0x0fff
#define MM_MUTEX_DEBUG_OUT_SEL_MASK 0x03
#define MM_MUTEX_CFG_MASK           0x01

#define MM_MUTEX_EN_MASK            0x01
#define MM_MUTEX_GET_MASK           0x03
#define MM_MUTEX_RST_MASK           0x01
#define MM_MUTEX_MOD_MASK           0x07ffffff
#define MM_MUTEX_SOF_MASK           0x0f

#endif  // __MMSYS_MUTEX_H__
