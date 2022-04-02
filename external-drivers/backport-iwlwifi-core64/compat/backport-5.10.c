// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/sysfs.h>

/**
 *	dev_fetch_sw_netstats - get per-cpu network device statistics
 *	@s: place to store stats
 *	@netstats: per-cpu network stats to read from
 *
 *	Read per-cpu network statistics and populate the related fields in @s.
 */
void dev_fetch_sw_netstats(struct rtnl_link_stats64 *s,
			   const struct pcpu_sw_netstats __percpu *netstats)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		const struct pcpu_sw_netstats *stats;
		struct pcpu_sw_netstats tmp;
		unsigned int start;

		stats = per_cpu_ptr(netstats, cpu);
		do {
			start = u64_stats_fetch_begin_irq(&stats->syncp);
			tmp.rx_packets = stats->rx_packets;
			tmp.rx_bytes   = stats->rx_bytes;
			tmp.tx_packets = stats->tx_packets;
			tmp.tx_bytes   = stats->tx_bytes;
		} while (u64_stats_fetch_retry_irq(&stats->syncp, start));

		s->rx_packets += tmp.rx_packets;
		s->rx_bytes   += tmp.rx_bytes;
		s->tx_packets += tmp.tx_packets;
		s->tx_bytes   += tmp.tx_bytes;
	}
}
EXPORT_SYMBOL_GPL(dev_fetch_sw_netstats);

int netif_rx_any_context(struct sk_buff *skb)
{
	/*
	 * If invoked from contexts which do not invoke bottom half
	 * processing either at return from interrupt or when softrqs are
	 * reenabled, use netif_rx_ni() which invokes bottomhalf processing
	 * directly.
	 */
	if (in_interrupt())
		return netif_rx(skb);
	else
		return netif_rx_ni(skb);
}
EXPORT_SYMBOL(netif_rx_any_context);

/**
 *	sysfs_emit - scnprintf equivalent, aware of PAGE_SIZE buffer.
 *	@buf:	start of PAGE_SIZE buffer.
 *	@fmt:	format
 *	@...:	optional arguments to @format
 *
 *
 * Returns number of characters written to @buf.
 */
int sysfs_emit(char *buf, const char *fmt, ...)
{
	va_list args;
	int len;

	if (WARN(!buf || offset_in_page(buf),
		 "invalid sysfs_emit: buf:%p\n", buf))
		return 0;

	va_start(args, fmt);
	len = vscnprintf(buf, PAGE_SIZE, fmt, args);
	va_end(args);

	return len;
}
EXPORT_SYMBOL_GPL(sysfs_emit);
