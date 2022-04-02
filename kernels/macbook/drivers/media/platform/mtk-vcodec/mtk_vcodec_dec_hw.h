// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 */

#ifndef _MTK_VCODEC_DEC_HW_H_
#define _MTK_VCODEC_DEC_HW_H_

#include <linux/component.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include "mtk_vcodec_drv.h"

/**
 * enum mtk_comp_hw_reg_idx - component register base index
 */
enum mtk_comp_hw_reg_idx {
	VDEC_COMP_SYS,
	VDEC_COMP_MISC,
	NUM_MAX_COMP_VCODEC_REG_BASE
};

/**
 * struct mtk_vdec_comp_dev - component framwork driver data
 * @plat_dev: platform device
 * @master_dev: master device
 * @irqlock: protect data access by irq handler and work thread
 * @reg_base: Mapped address of MTK Vcodec registers.
 *
 * @curr_ctx: The context that is waiting for codec hardware
 *
 * @dec_irq: decoder irq resource
 * @pm: power management control
 * @comp_idx: component index
 */
struct mtk_vdec_comp_dev {
	struct platform_device *plat_dev;
	struct mtk_vcodec_dev *master_dev;
	spinlock_t irqlock;
	void __iomem *reg_base[NUM_MAX_COMP_VCODEC_REG_BASE];

	struct mtk_vcodec_ctx *curr_ctx;

	int dec_irq;
	struct mtk_vcodec_pm pm;
	int comp_idx;
};

#endif /* _MTK_VCODEC_DEC_HW_H_ */
