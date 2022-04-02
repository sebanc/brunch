/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_PAD_H__
#define __MDP_REG_PAD_H__

#define VPP_PADDING0_PADDING_CON        (0x000)
#define VPP_PADDING0_PADDING_PIC_SIZE   (0x004)
#define VPP_PADDING0_W_PADDING_SIZE     (0x008)
#define VPP_PADDING0_H_PADDING_SIZE     (0x00c)

#define VPP_PADDING0_PADDING_CON_MASK      (0x00000007)
#define VPP_PADDING0_PADDING_PIC_SIZE_MASK (0xFFFFFFFF)
#define VPP_PADDING0_W_PADDING_SIZE_MASK   (0x1FFF1FFF)
#define VPP_PADDING0_H_PADDING_SIZE_MASK   (0x1FFF1FFF)

#endif  // __MDP_REG_PAD_H__
