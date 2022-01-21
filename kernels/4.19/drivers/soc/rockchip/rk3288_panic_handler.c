// SPDX-License-Identifier: GPL-2.0
/*
 * RK3288 panic handler
 *
 * Copyright (c) 2013 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
 *
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <asm/io.h>

#define RK3288_DEBUG_PA_CPU(x)		(0xffbb0000 + (x * 0x2000))
#define CPU_DBGPCSR			0xa0
#define NUM_CPU_SAMPLES			100
#define NUM_SAMPLES_TO_PRINT		32
#define RK3288_NUM_CORES		4

static void __iomem *rk3288_cpu_debug[RK3288_NUM_CORES];
static struct clk *pclk_dbg;
static struct clk *pclk_core_niu;

static int rk3288_panic_notify(struct notifier_block *nb, unsigned long event,
			       void *p)
{
	unsigned long dbgpcsr;
	int i, j;
	void *pc;
	void *prev_pc = NULL;
	int printed = 0;

	clk_enable(pclk_dbg);
	clk_enable(pclk_core_niu);

	for_each_online_cpu(i) {
		/* No need to print something in rk3288_panic_notify() */
		if (smp_processor_id() == i)
			continue;

		/* Try to read a bunch of times if CPU is actually running */
		for (j = 0; j < NUM_CPU_SAMPLES &&
			    printed < NUM_SAMPLES_TO_PRINT; j++) {
			dbgpcsr = readl_relaxed(rk3288_cpu_debug[i] +
						CPU_DBGPCSR);

			/* NOTE: no offset on A17; see DBGDEVID1.PCSROffset */
			pc = (void *)(dbgpcsr & ~1);

			if (pc != prev_pc) {
				pr_err("CPU%d PC: <%p> %pF\n", i, pc, pc);
				printed++;
			}

			prev_pc = pc;
		}
	}
	return NOTIFY_OK;
}

struct notifier_block rk3288_panic_nb = {
	.notifier_call = rk3288_panic_notify,
	.priority = INT_MAX,
};

static int __init rk3288_panic_init(void)
{
	int i;

	if (of_find_compatible_node(NULL, NULL, "rockchip,rk3288") == NULL)
		return 0;

	/* These two clocks appear to be needed to access regs */
	pclk_dbg = clk_get(NULL, "pclk_dbg");
	if (WARN_ON(IS_ERR(pclk_dbg)))
		return PTR_ERR(pclk_dbg);

	pclk_core_niu = clk_get(NULL, "pclk_core_niu");
	if (WARN_ON(IS_ERR(pclk_core_niu)))
		return PTR_ERR(pclk_core_niu);

	clk_prepare(pclk_dbg);
	clk_prepare(pclk_core_niu);

	for (i = 0; i < RK3288_NUM_CORES; i++)
		rk3288_cpu_debug[i] = ioremap(RK3288_DEBUG_PA_CPU(i), SZ_4K);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &rk3288_panic_nb);

	return 0;
}

arch_initcall(rk3288_panic_init);
