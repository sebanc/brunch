#ifndef __BACKPORT_LINUX_U64_STATS_SYNC_H
#define __BACKPORT_LINUX_U64_STATS_SYNC_H

#include <linux/version.h>
#include <generated/utsrelease.h>
#include_next <linux/u64_stats_sync.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0) && \
    !(LINUX_VERSION_CODE == KERNEL_VERSION(3,13,11) && UTS_UBUNTU_RELEASE_ABI > 30)
static inline unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if BITS_PER_LONG==32
	local_irq_disable();
#endif
	return 0;
#endif
}

static inline bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if BITS_PER_LONG==32
	local_irq_enable();
#endif
	return false;
#endif
}

#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
#if BITS_PER_LONG == 32 && defined(CONFIG_SMP)
# define u64_stats_init(syncp)	seqcount_init(syncp.seq)
#else
# define u64_stats_init(syncp)	do { } while (0)
#endif
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0) */

#endif /* __BACKPORT_LINUX_U64_STATS_SYNC_H */
