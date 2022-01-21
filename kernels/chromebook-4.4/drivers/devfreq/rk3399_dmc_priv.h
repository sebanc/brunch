/*
 * Copyright (C) 2017 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RK3399_DMC_PRIV_H
#define __RK3399_DMC_PRIV_H

void rockchip_ddrclk_set_timeout_en(struct clk *clk, bool enable);
void rockchip_ddrclk_wait_set_rate(struct clk *clk);
int rockchip_ddrclk_register_sync_nb(struct clk *clk,
				     struct notifier_block *nb);
int rockchip_ddrclk_unregister_sync_nb(struct clk *clk,
				       struct notifier_block *nb);
#endif
