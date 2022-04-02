/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2017-2022 Google, Inc.
 */

#ifndef __SOC_RK3399_DDRCLK_H
#define __SOC_RK3399_DDRCLK_H

struct clk;
struct notifier_block;

void rockchip_ddrclk_set_timeout_en(struct clk *clk, bool enable);
void rockchip_ddrclk_wait_set_rate(struct clk *clk);
int rockchip_ddrclk_register_sync_nb(struct clk *clk,
				     struct notifier_block *nb);
int rockchip_ddrclk_unregister_sync_nb(struct clk *clk,
				       struct notifier_block *nb);
#endif
