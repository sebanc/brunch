/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 */

#ifndef __MTK_MDP_COMP_H__
#define __MTK_MDP_COMP_H__

/**
 * struct mtk_mdp_comp - the MDP's function component data
 * @node:	list node to track sibing MDP components
 * @clk:	clocks required for component
 * @dev:	component's device
 */
struct mtk_mdp_comp {
	struct list_head	node;
	struct clk		*clk[2];
	struct device		*dev;
};

int mtk_mdp_comp_init(struct mtk_mdp_comp *comp, struct device *dev);

void mtk_mdp_comp_clock_on(struct mtk_mdp_comp *comp);
void mtk_mdp_comp_clock_off(struct mtk_mdp_comp *comp);

extern struct platform_driver mtk_mdp_component_driver;

#endif /* __MTK_MDP_COMP_H__ */
