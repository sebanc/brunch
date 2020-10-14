/*
 * Copyright (c) 2013  Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * Backport compatibility file for Linux for kernels 3.6.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/export.h>
#include <linux/clk.h>

/* whoopsie ! */
#ifndef CONFIG_COMMON_CLK
int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL_GPL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL_GPL(clk_disable);
#endif
