/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Daoyuan Huang <daoyuan.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_MDP3_DEBUG_H__
#define __MTK_MDP3_DEBUG_H__

#include <linux/soc/mediatek/mtk-cmdq.h>

#define MDP_DEBUG

struct mdp_func_struct {
	void (*mdp_dump_mmsys_config)(void);
	void (*mdp_dump_rsz)(void __iomem *base, const char *label);
	void (*mdp_dump_tdshp)(void __iomem *base, const char *label);
	uint32_t (*mdp_rdma_get_src_base_addr)(void);
	uint32_t (*mdp_wrot_get_reg_offset_dst_addr)(void);
	uint32_t (*mdp_wdma_get_reg_offset_dst_addr)(void);
};

void mdp_debug_init(struct platform_device *pDevice);
void mdp_debug_deinit(void);
struct mdp_func_struct *mdp_get_func(void);
int32_t mdp_dump_info(uint64_t comp_flag, int log_level);


#endif  /* __MTK_MDP3_DEBUG_H__ */

