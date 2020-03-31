#ifndef __BACKPORT_NETDEVICE_H
#define __BACKPORT_NETDEVICE_H
#include_next <linux/netdevice.h>
#include <linux/netdev_features.h>
#include <linux/version.h>
#include <backport/magic.h>

/*
 * This is declared implicitly in newer kernels by netdevice.h using
 * this pointer in struct net_device, but declare it here anyway so
 * pointers to it are accepted as function arguments without warning.
 */
struct inet6_dev;

/* older kernels don't include this here, we need it */
#include <linux/ethtool.h>
#include <linux/rculist.h>
/*
 * new kernels include <net/netprio_cgroup.h> which
 * has this ... and some drivers rely on it :-(
 */
#include <linux/hardirq.h>

#if LINUX_VERSION_IS_LESS(3,14,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
/*
 * Backports note: if in-kernel support is provided we could then just
 * take the kernel's implementation of __dev_kfree_skb_irq() as it requires
 * raise_softirq_irqoff() which is not exported. For the backport case we
 * just use slightly less optimized version and we don't get the ability
 * to distinguish the two different reasons to free the skb -- whether it
 * was consumed or dropped.
 *
 * The upstream documentation for this:
 *
 * It is not allowed to call kfree_skb() or consume_skb() from hardware
 * interrupt context or with hardware interrupts being disabled.
 * (in_irq() || irqs_disabled())
 *
 * We provide four helpers that can be used in following contexts :
 *
 * dev_kfree_skb_irq(skb) when caller drops a packet from irq context,
 *  replacing kfree_skb(skb)
 *
 * dev_consume_skb_irq(skb) when caller consumes a packet from irq context.
 *  Typically used in place of consume_skb(skb) in TX completion path
 *
 * dev_kfree_skb_any(skb) when caller doesn't know its current irq context,
 *  replacing kfree_skb(skb)
 *
 * dev_consume_skb_any(skb) when caller doesn't know its current irq context,
 *  and consumed a packet. Used in place of consume_skb(skb)
 */
#define skb_free_reason LINUX_BACKPORT(skb_free_reason)
enum skb_free_reason {
	SKB_REASON_CONSUMED,
	SKB_REASON_DROPPED,
};

#define __dev_kfree_skb_irq LINUX_BACKPORT(__dev_kfree_skb_irq)
static inline void __dev_kfree_skb_irq(struct sk_buff *skb,
				       enum skb_free_reason reason)
{
	dev_kfree_skb_irq(skb);
}

#define __dev_kfree_skb_any LINUX_BACKPORT(__dev_kfree_skb_any)
static inline void __dev_kfree_skb_any(struct sk_buff *skb,
				       enum skb_free_reason reason)
{
	dev_kfree_skb_any(skb);
}

#define dev_consume_skb_irq LINUX_BACKPORT(dev_consume_skb_irq)
static inline void dev_consume_skb_irq(struct sk_buff *skb)
{
	dev_kfree_skb_irq(skb);
}

#define dev_consume_skb_any LINUX_BACKPORT(dev_consume_skb_any)
static inline void dev_consume_skb_any(struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
}

#if (LINUX_VERSION_CODE != KERNEL_VERSION(3,13,11) || UTS_UBUNTU_RELEASE_ABI < 24)
struct pcpu_sw_netstats {
	u64     rx_packets;
	u64     rx_bytes;
	u64     tx_packets;
	u64     tx_bytes;
	struct u64_stats_sync   syncp;
};
#endif

#define netdev_tstats(dev)	((struct pcpu_sw_netstats *)dev->ml_priv)
#define netdev_assign_tstats(dev, e)	dev->ml_priv = (e);
#else
#define netdev_tstats(dev)	dev->tstats
#define netdev_assign_tstats(dev, e)	dev->tstats = (e);
#endif /* LINUX_VERSION_IS_LESS(3,14,0) */

#if LINUX_VERSION_IS_LESS(3,7,8)
#define netdev_set_default_ethtool_ops LINUX_BACKPORT(netdev_set_default_ethtool_ops)
extern void netdev_set_default_ethtool_ops(struct net_device *dev,
					   const struct ethtool_ops *ops);
#endif

#if LINUX_VERSION_IS_LESS(3,3,0)
/*
 * BQL was added as of v3.3 but some Linux distributions
 * have backported BQL to their v3.2 kernels or older. To
 * address this we assume that they also enabled CONFIG_BQL
 * and test for that here and simply avoid adding the static
 * inlines if it was defined
 */
#ifndef CONFIG_BQL
#define netdev_tx_sent_queue LINUX_BACKPORT(netdev_tx_sent_queue)
static inline void netdev_tx_sent_queue(struct netdev_queue *dev_queue,
					unsigned int bytes)
{
}

#define netdev_sent_queue LINUX_BACKPORT(netdev_sent_queue)
static inline void netdev_sent_queue(struct net_device *dev, unsigned int bytes)
{
}

#define netdev_tx_completed_queue LINUX_BACKPORT(netdev_tx_completed_queue)
static inline void netdev_tx_completed_queue(struct netdev_queue *dev_queue,
					     unsigned pkts, unsigned bytes)
{
}

#define netdev_completed_queue LINUX_BACKPORT(netdev_completed_queue)
static inline void netdev_completed_queue(struct net_device *dev,
					  unsigned pkts, unsigned bytes)
{
}

#define netdev_tx_reset_queue LINUX_BACKPORT(netdev_tx_reset_queue)
static inline void netdev_tx_reset_queue(struct netdev_queue *q)
{
}

#define netdev_reset_queue LINUX_BACKPORT(netdev_reset_queue)
static inline void netdev_reset_queue(struct net_device *dev_queue)
{
}
#endif /* CONFIG_BQL */
#endif /* < 3.3 */

#ifndef NETDEV_PRE_UP
#define NETDEV_PRE_UP		0x000D
#endif

#if LINUX_VERSION_IS_LESS(3,11,0)
#define netdev_notifier_info_to_dev(ndev) ndev
#endif

#if LINUX_VERSION_IS_LESS(3,7,0)
#define netdev_notify_peers(dev) netif_notify_peers(dev)
#define napi_gro_flush(napi, old) napi_gro_flush(napi)
#endif

#ifndef IFF_LIVE_ADDR_CHANGE
#define IFF_LIVE_ADDR_CHANGE 0x100000
#endif

#ifndef IFF_SUPP_NOFCS
#define IFF_SUPP_NOFCS	0x80000		/* device supports sending custom FCS */
#endif

#ifndef IFF_UNICAST_FLT
#define IFF_UNICAST_FLT	0x20000		/* Supports unicast filtering	*/
#endif

#ifndef QUEUE_STATE_ANY_XOFF
#define __QUEUE_STATE_DRV_XOFF __QUEUE_STATE_XOFF
#define __QUEUE_STATE_STACK_XOFF __QUEUE_STATE_XOFF
#endif

#if LINUX_VERSION_IS_LESS(3,17,0)
#ifndef NET_NAME_UNKNOWN
#define NET_NAME_UNKNOWN	0
#endif
#ifndef NET_NAME_ENUM
#define NET_NAME_ENUM		1
#endif
#ifndef NET_NAME_PREDICTABLE
#define NET_NAME_PREDICTABLE	2
#endif
#ifndef NET_NAME_USER
#define NET_NAME_USER		3
#endif
#ifndef NET_NAME_RENAMED
#define NET_NAME_RENAMED	4
#endif

#define alloc_netdev_mqs(sizeof_priv, name, name_assign_type, setup, txqs, rxqs) \
	alloc_netdev_mqs(sizeof_priv, name, setup, txqs, rxqs)

#undef alloc_netdev
#define alloc_netdev(sizeof_priv, name, name_assign_type, setup) \
	alloc_netdev_mqs(sizeof_priv, name, name_assign_type, setup, 1, 1)

#undef alloc_netdev_mq
#define alloc_netdev_mq(sizeof_priv, name, name_assign_type, setup, count) \
	alloc_netdev_mqs(sizeof_priv, name, name_assign_type, setup, count, \
			 count)
#endif /* LINUX_VERSION_IS_LESS(3,17,0) */

/*
 * This backports this commit from upstream:
 * commit 87757a917b0b3c0787e0563c679762152be81312
 * net: force a list_del() in unregister_netdevice_many()
 */
#if (!(LINUX_VERSION_IS_GEQ(3,10,45) && \
       LINUX_VERSION_IS_LESS(3,11,0)) && \
     !(LINUX_VERSION_IS_GEQ(3,12,23) && \
       LINUX_VERSION_IS_LESS(3,13,0)) && \
     !(LINUX_VERSION_IS_GEQ(3,14,9) && \
       LINUX_VERSION_IS_LESS(3,15,0)) && \
     !(LINUX_VERSION_IS_GEQ(3,15,2) && \
       LINUX_VERSION_IS_LESS(3,16,0)) && \
     LINUX_VERSION_IS_LESS(3,16,0))
static inline void backport_unregister_netdevice_many(struct list_head *head)
{
	unregister_netdevice_many(head);

	if (!(head->next == LIST_POISON1 && head->prev == LIST_POISON2))
		list_del(head);
}
#define unregister_netdevice_many LINUX_BACKPORT(unregister_netdevice_many)
#endif

#if LINUX_VERSION_IS_LESS(3,19,0)
#define napi_alloc_frag(fragsz) netdev_alloc_frag(fragsz)
#endif

#if LINUX_VERSION_IS_LESS(3,19,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
/* RSS keys are 40 or 52 bytes long */
#define NETDEV_RSS_KEY_LEN 52
#define netdev_rss_key_fill LINUX_BACKPORT(netdev_rss_key_fill)
void netdev_rss_key_fill(void *buffer, size_t len);
#endif /* LINUX_VERSION_IS_LESS(3,19,0) */

#if LINUX_VERSION_IS_LESS(3,19,0)
#define napi_alloc_skb LINUX_BACKPORT(napi_alloc_skb)
static inline struct sk_buff *napi_alloc_skb(struct napi_struct *napi,
					     unsigned int length)
{
	return netdev_alloc_skb_ip_align(napi->dev, length);
}
#endif /* LINUX_VERSION_IS_LESS(3,19,0) */

#ifndef IFF_TX_SKB_SHARING
#define IFF_TX_SKB_SHARING 0
#endif

#if LINUX_VERSION_IS_LESS(4,1,0)
netdev_features_t passthru_features_check(struct sk_buff *skb,
					  struct net_device *dev,
					  netdev_features_t features);
#endif /* LINUX_VERSION_IS_LESS(4,1,0) */

#if LINUX_VERSION_IS_LESS(4,2,0)
#undef u64_stats_init
static inline void u64_stats_init(struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG == 32 && defined(CONFIG_SMP)
	seqcount_init(&syncp->seq);
#endif
}
#endif /* LINUX_VERSION_IS_LESS(4,2,0) */

#ifndef netdev_alloc_pcpu_stats
#define netdev_alloc_pcpu_stats(type)				\
({								\
	typeof(type) __percpu *pcpu_stats = alloc_percpu(type); \
	if (pcpu_stats)	{					\
		int i;						\
		for_each_possible_cpu(i) {			\
			typeof(type) *stat;			\
			stat = per_cpu_ptr(pcpu_stats, i);	\
			u64_stats_init(&stat->syncp);		\
		}						\
	}							\
	pcpu_stats;						\
})
#endif /* netdev_alloc_pcpu_stats */

#if LINUX_VERSION_IS_LESS(4,10,0)
static inline bool backport_napi_complete_done(struct napi_struct *n, int work_done)
{
	if (unlikely(test_bit(NAPI_STATE_NPSVC, &n->state)))
		return false;

#if LINUX_VERSION_IS_LESS(3,19,0)
	napi_complete(n);
#else
	napi_complete_done(n, work_done);
#endif /* < 3.19 */
	return true;
}
#define napi_complete_done LINUX_BACKPORT(napi_complete_done)
#endif /* < 4.10 */

#if LINUX_VERSION_IS_LESS(4,5,0)
#define netif_tx_napi_add LINUX_BACKPORT(netif_tx_napi_add)
/**
 *	netif_tx_napi_add - initialize a napi context
 *	@dev:  network device
 *	@napi: napi context
 *	@poll: polling function
 *	@weight: default weight
 *
 * This variant of netif_napi_add() should be used from drivers using NAPI
 * to exclusively poll a TX queue.
 * This will avoid we add it into napi_hash[], thus polluting this hash table.
 */
static inline void netif_tx_napi_add(struct net_device *dev,
				     struct napi_struct *napi,
				     int (*poll)(struct napi_struct *, int),
				     int weight)
{
	netif_napi_add(dev, napi, poll, weight);
}
#endif /* < 4.5 */

#ifndef NETIF_F_CSUM_MASK
#define NETIF_F_CSUM_MASK NETIF_F_ALL_CSUM
#endif

#if LINUX_VERSION_IS_LESS(4,7,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
#define netif_trans_update LINUX_BACKPORT(netif_trans_update)
static inline void netif_trans_update(struct net_device *dev)
{
	dev->trans_start = jiffies;
}
#endif

#if LINUX_VERSION_IS_LESS(4,11,9)
#define netdev_set_priv_destructor(_dev, _destructor) \
	(_dev)->destructor = __ ## _destructor
#define netdev_set_def_destructor(_dev) \
	(_dev)->destructor = free_netdev
#else
#define netdev_set_priv_destructor(_dev, _destructor) \
	(_dev)->needs_free_netdev = true; \
	(_dev)->priv_destructor = (_destructor);
#define netdev_set_def_destructor(_dev) \
	(_dev)->needs_free_netdev = true;
#endif

#if LINUX_VERSION_IS_LESS(4,15,0)
static inline int _bp_netdev_upper_dev_link(struct net_device *dev,
					    struct net_device *upper_dev)
{
#if LINUX_VERSION_IS_LESS(3,9,0)
	return 0;
#else
	return netdev_upper_dev_link(dev, upper_dev);
#endif
}
#define netdev_upper_dev_link3(dev, upper, extack) \
	netdev_upper_dev_link(dev, upper)
#define netdev_upper_dev_link2(dev, upper) \
	netdev_upper_dev_link(dev, upper)
#define netdev_upper_dev_link(...) \
	macro_dispatcher(netdev_upper_dev_link, __VA_ARGS__)(__VA_ARGS__)
#endif

#if LINUX_VERSION_IS_LESS(5,0,0)
static inline int backport_dev_open(struct net_device *dev, struct netlink_ext_ack *extack)
{
	return dev_open(dev);
}
#define dev_open LINUX_BACKPORT(dev_open)
#endif

#endif /* __BACKPORT_NETDEVICE_H */
