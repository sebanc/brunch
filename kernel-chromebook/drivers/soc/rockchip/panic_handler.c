/*
 * RK3399 panic handler
 *
 * Based on arch/arm/mach-rockchip/rockchip.c
 * Copyright (c) 2013 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>

static void __iomem *rockchip_cpu_debug[6];

#define RK3399_DEBUG_PA			0xfe400000
#define RK3399_DEBUG_PA_LITTLE(x)	(RK3399_DEBUG_PA + 0x030000 + \
					 ((x) * 0x002000))
#define RK3399_DEBUG_PA_BIG(x)		(RK3399_DEBUG_PA + 0x210000 + \
					 ((x) * 0x100000))

#define CPU_EDPCSR_LO			0x0a0
#define CPU_EDPCSR_HI			0x0ac
#define CPU_EDLAR			0xfb0
#define CPU_EDLAR_UNLOCK		0xc5acce55

#define NUM_CPU_SAMPLES			100
#define NUM_SAMPLES_TO_PRINT		32

int rockchip_panic_notify(struct notifier_block *nb, unsigned long event,
			  void *p)
{
	unsigned long dbgpcsr;
	int i, j;
	void *pc = NULL;
	void *prev_pc = NULL;
	int printed = 0;

	/*
	 * The panic handler will try to shut down the other CPUs.
	 * If any of them are still online at this point, this loop attempts
	 * to determine the program counter value.  If there are no wedged
	 * CPUs, this loop will do nothing.
	 */
	for_each_online_cpu(i) {
		/* No need to print something in rockchip_panic_notify() */
		if (smp_processor_id() == i)
			continue;

		/* Unlock EDLSR.SLK so that EDPCSRhi gets populated */
		writel_relaxed(CPU_EDLAR_UNLOCK,
			       rockchip_cpu_debug[i] + CPU_EDLAR);

		/* Try to read a bunch of times if CPU is actually running */
		for (j = 0; j < NUM_CPU_SAMPLES &&
			    printed < NUM_SAMPLES_TO_PRINT; j++) {

			dbgpcsr = ((u64)readl_relaxed(rockchip_cpu_debug[i] +
						      CPU_EDPCSR_LO)) |
				  ((u64)readl_relaxed(rockchip_cpu_debug[i] +
						      CPU_EDPCSR_HI) << 32);

			/* NOTE: no offset on ARMv8; see DBGDEVID1.PCSROffset */
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

struct notifier_block rockchip_panic_nb = {
	.notifier_call = rockchip_panic_notify,
	.priority = INT_MAX,
};

static int __init rockchip_panic_init(void)
{
	int i;

	if (of_find_compatible_node(NULL, NULL, "rockchip,rk3399") == NULL)
		return 0;

	for (i = 0; i < 4; i++) {
		rockchip_cpu_debug[i] =
			ioremap(RK3399_DEBUG_PA_LITTLE(i), SZ_4K);
	}
	for (i = 0; i < 2; i++) {
		rockchip_cpu_debug[4 + i] =
			ioremap(RK3399_DEBUG_PA_BIG(i), SZ_4K);
	}

	atomic_notifier_chain_register(&panic_notifier_list,
				       &rockchip_panic_nb);
	return 0;
}
arch_initcall(rockchip_panic_init);
