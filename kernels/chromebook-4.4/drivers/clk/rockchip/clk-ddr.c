/*
 * Copyright (c) 2016 Rockchip Electronics Co. Ltd.
 * Author: Lin Huang <hl@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <soc/rockchip/rk3399_dmc.h>
#include <soc/rockchip/rockchip_sip.h>
#include "clk.h"

struct rockchip_ddrclk {
	struct clk_hw			hw;
	void __iomem			*reg_base;
	int				mux_offset;
	int				mux_shift;
	int				mux_width;
	int				div_shift;
	int				div_width;
	int				ddr_flag;
	unsigned long			cached_rate;
	struct work_struct		set_rate_work;
	struct mutex			lock;
	struct raw_notifier_head	sync_chain;
	bool				timeout_en;
};

#define to_rockchip_ddrclk_hw(hw) container_of(hw, struct rockchip_ddrclk, hw)
#define DMC_DEFAULT_TIMEOUT_NS		NSEC_PER_SEC
#define DDRCLK_SET_RATE_MAX_RETRIES	3

static void rockchip_ddrclk_set_rate_func(struct work_struct *work)
{
	struct rockchip_ddrclk *ddrclk = container_of(work,
			struct rockchip_ddrclk, set_rate_work);
	ktime_t timeout = ktime_add_ns(ktime_get(), DMC_DEFAULT_TIMEOUT_NS);
	struct arm_smccc_res res;
	int ret, i;

	mutex_lock(&ddrclk->lock);
	for (i = 0; i < DDRCLK_SET_RATE_MAX_RETRIES; i++) {
		ret = raw_notifier_call_chain(&ddrclk->sync_chain, 0,
					      &timeout);
		if (ddrclk->timeout_en && ret == NOTIFY_BAD)
			goto out;

		/*
		* Check the timeout with irqs disabled. This is so we don't get
		* preempted after checking the timeout. That could cause things
		* like garbage values for the display if we change the ddr rate
		* at the wrong time.
		*/
		local_irq_disable();
		if (ddrclk->timeout_en &&
		    ktime_after(ktime_add_ns(ktime_get(), DMC_MIN_SET_RATE_NS),
				timeout)) {
			local_irq_enable();
			continue;
		}

		arm_smccc_smc(ROCKCHIP_SIP_DRAM_FREQ, ddrclk->cached_rate, 0,
			      ROCKCHIP_SIP_CONFIG_DRAM_SET_RATE,
			      0, 0, 0, 0, &res);
		local_irq_enable();
		break;
	}
out:
	mutex_unlock(&ddrclk->lock);
}

void rockchip_ddrclk_set_timeout_en(struct clk *clk, bool enable)
{
	struct clk_hw *hw = __clk_get_hw(clk);
	struct rockchip_ddrclk *ddrclk = to_rockchip_ddrclk_hw(hw);

	mutex_lock(&ddrclk->lock);
	ddrclk->timeout_en = enable;
	mutex_unlock(&ddrclk->lock);
}
EXPORT_SYMBOL(rockchip_ddrclk_set_timeout_en);

int rockchip_ddrclk_register_sync_nb(struct clk *clk, struct notifier_block *nb)
{
	struct clk_hw *hw = __clk_get_hw(clk);
	struct rockchip_ddrclk *ddrclk;
	int ret;

	if (!hw || !nb)
		return -EINVAL;

	ddrclk = to_rockchip_ddrclk_hw(hw);
	if (!ddrclk)
		return -EINVAL;

	mutex_lock(&ddrclk->lock);
	ret = raw_notifier_chain_register(&ddrclk->sync_chain, nb);
	mutex_unlock(&ddrclk->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_ddrclk_register_sync_nb);

int rockchip_ddrclk_unregister_sync_nb(struct clk *clk,
				       struct notifier_block *nb)
{
	struct clk_hw *hw = __clk_get_hw(clk);
	struct rockchip_ddrclk *ddrclk;
	int ret;

	if (!hw || !nb)
		return -EINVAL;

	ddrclk = to_rockchip_ddrclk_hw(hw);
	if (!ddrclk)
		return -EINVAL;

	mutex_lock(&ddrclk->lock);
	ret = raw_notifier_chain_unregister(&ddrclk->sync_chain, nb);
	mutex_unlock(&ddrclk->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_ddrclk_unregister_sync_nb);

void rockchip_ddrclk_wait_set_rate(struct clk *clk)
{
	struct clk_hw *hw = __clk_get_hw(clk);
	struct rockchip_ddrclk *ddrclk = to_rockchip_ddrclk_hw(hw);

	flush_work(&ddrclk->set_rate_work);
}
EXPORT_SYMBOL_GPL(rockchip_ddrclk_wait_set_rate);

static long rockchip_ddrclk_sip_round_rate(struct clk_hw *hw,
					   unsigned long rate,
					   unsigned long *prate)
{
	struct arm_smccc_res res;

	arm_smccc_smc(ROCKCHIP_SIP_DRAM_FREQ, rate, 0,
		      ROCKCHIP_SIP_CONFIG_DRAM_ROUND_RATE,
		      0, 0, 0, 0, &res);

	return res.a0;
}

static int rockchip_ddrclk_sip_set_rate(struct clk_hw *hw, unsigned long drate,
					unsigned long prate)
{
	struct rockchip_ddrclk *ddrclk = to_rockchip_ddrclk_hw(hw);
	long rate;

	rate = rockchip_ddrclk_sip_round_rate(hw, drate, &prate);
	if (rate < 0)
		return rate;

	ddrclk->cached_rate = rate;
	queue_work(system_highpri_wq, &ddrclk->set_rate_work);
	return 0;
}

static unsigned long
rockchip_ddrclk_sip_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct rockchip_ddrclk *ddrclk = to_rockchip_ddrclk_hw(hw);
	struct arm_smccc_res res;

	/*
	 * This is racy, but it doesn't matter. Returning the cached_rate allows
	 * us to report what the likely value is based on the async work. We
	 * still need to check if the set rate work timed out in the dmcfreq
	 * driver, though. That will flush the work before checking the rate,
	 * and the NOCACHE flag is set for this clk.
	 */
	if (work_busy(&ddrclk->set_rate_work))
		return ddrclk->cached_rate;

	arm_smccc_smc(ROCKCHIP_SIP_DRAM_FREQ, 0, 0,
		      ROCKCHIP_SIP_CONFIG_DRAM_GET_RATE,
		      0, 0, 0, 0, &res);

	return res.a0;
}

static u8 rockchip_ddrclk_get_parent(struct clk_hw *hw)
{
	struct rockchip_ddrclk *ddrclk = to_rockchip_ddrclk_hw(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 val;

	val = clk_readl(ddrclk->reg_base +
			ddrclk->mux_offset) >> ddrclk->mux_shift;
	val &= GENMASK(ddrclk->mux_width - 1, 0);

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static const struct clk_ops rockchip_ddrclk_sip_ops = {
	.recalc_rate = rockchip_ddrclk_sip_recalc_rate,
	.set_rate = rockchip_ddrclk_sip_set_rate,
	.round_rate = rockchip_ddrclk_sip_round_rate,
	.get_parent = rockchip_ddrclk_get_parent,
};

struct clk *rockchip_clk_register_ddrclk(const char *name, int flags,
					 const char *const *parent_names,
					 u8 num_parents, int mux_offset,
					 int mux_shift, int mux_width,
					 int div_shift, int div_width,
					 int ddr_flag, void __iomem *reg_base)
{
	struct rockchip_ddrclk *ddrclk;
	struct clk_init_data init;
	struct clk *clk;
	struct arm_smccc_res res;

	ddrclk = kzalloc(sizeof(*ddrclk), GFP_KERNEL);
	if (!ddrclk)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	init.flags = flags;
	init.flags |= CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE;

	switch (ddr_flag) {
	case ROCKCHIP_DDRCLK_SIP:
		init.ops = &rockchip_ddrclk_sip_ops;
		break;
	default:
		pr_err("%s: unsupported ddrclk type %d\n", __func__, ddr_flag);
		kfree(ddrclk);
		return ERR_PTR(-EINVAL);
	}

	ddrclk->reg_base = reg_base;
	ddrclk->hw.init = &init;
	ddrclk->mux_offset = mux_offset;
	ddrclk->mux_shift = mux_shift;
	ddrclk->mux_width = mux_width;
	ddrclk->div_shift = div_shift;
	ddrclk->div_width = div_width;
	ddrclk->ddr_flag = ddr_flag;
	ddrclk->timeout_en = true;
	mutex_init(&ddrclk->lock);
	INIT_WORK(&ddrclk->set_rate_work, rockchip_ddrclk_set_rate_func);
	RAW_INIT_NOTIFIER_HEAD(&ddrclk->sync_chain);

	arm_smccc_smc(ROCKCHIP_SIP_DRAM_FREQ, 0, 0,
		      ROCKCHIP_SIP_CONFIG_DRAM_GET_RATE,
		      0, 0, 0, 0, &res);
	ddrclk->cached_rate = res.a0;

	clk = clk_register(NULL, &ddrclk->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: could not register ddrclk %s\n", __func__,	name);
		kfree(ddrclk);
		return NULL;
	}

	return clk;
}
