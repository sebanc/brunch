/*
 * ChromeOS backport definitions
 */

#include <hdrs/mac80211-exp.h>
#include <hdrs/ath-exp.h>

#include <linux/types.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/spinlock_types.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)
#include "u64_stats_sync.h"

struct pcpu_sw_netstats {
	u64     rx_packets;
	u64     rx_bytes;
	u64     tx_packets;
	u64     tx_bytes;
	struct u64_stats_sync   syncp;
};

#define netdev_tstats(dev)	((struct pcpu_sw_netstats *)dev->ml_priv)
#define netdev_assign_tstats(dev, e)	dev->ml_priv = (e);
#else
#define netdev_tstats(dev)	dev->tstats
#define netdev_assign_tstats(dev, e)	dev->tstats = (e);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0) */

