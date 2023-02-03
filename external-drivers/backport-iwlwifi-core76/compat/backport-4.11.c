// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/netdevice.h>

struct rtnl_link_stats64 *
bp_dev_get_tstats64(struct net_device *dev, struct rtnl_link_stats64 *s)
{
	dev_get_tstats64(dev, s);
	return s;
}
EXPORT_SYMBOL_GPL(bp_dev_get_tstats64);
