// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/netdevice.h>

/**
 *	dev_get_tstats64 - ndo_get_stats64 implementation
 *	@dev: device to get statistics from
 *	@s: place to store stats
 *
 *	Populate @s from dev->stats and dev->tstats. Can be used as
 *	ndo_get_stats64() callback.
 */
void dev_get_tstats64(struct net_device *dev, struct rtnl_link_stats64 *s)
{
	netdev_stats_to_stats64(s, &dev->stats);
	dev_fetch_sw_netstats(s, dev->tstats);
}
EXPORT_SYMBOL_GPL(dev_get_tstats64);
