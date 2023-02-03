#ifndef __BACKPORT_LINUX_CLK_H
#define __BACKPORT_LINUX_CLK_H
#include_next <linux/clk.h>

#include <linux/version.h>
#if LINUX_VERSION_IS_LESS(5,1,0)

static inline
struct clk *devm_clk_get_optional(struct device *dev, const char *id)
{
	struct clk *clk = devm_clk_get(dev, id);

	if (clk == ERR_PTR(-ENOENT))
		return NULL;

	return clk;
}

#endif /* < 5.1 */
#endif /* __BACKPORT_LINUX_CLK_H */
