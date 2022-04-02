/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_REG_MERGE_H__
#define __MDP_REG_MERGE_H__

#define VPP_MERGE_ENABLE	(0x000)
#define VPP_MERGE_CFG_0		(0x010)
#define VPP_MERGE_CFG_4		(0x020)
#define VPP_MERGE_CFG_12	(0x040)
#define VPP_MERGE_CFG_24	(0x070)
#define VPP_MERGE_CFG_25	(0x074)

#define VPP_MERGE_ENABLE_MASK	(0xFFFFFFFF)
#define VPP_MERGE_CFG_0_MASK	(0xFFFFFFFF)
#define VPP_MERGE_CFG_4_MASK	(0xFFFFFFFF)
#define VPP_MERGE_CFG_12_MASK	(0xFFFFFFFF)
#define VPP_MERGE_CFG_24_MASK	(0xFFFFFFFF)
#define VPP_MERGE_CFG_25_MASK	(0xFFFFFFFF)
#endif
