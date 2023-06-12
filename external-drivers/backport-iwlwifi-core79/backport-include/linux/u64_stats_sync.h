#ifndef __BACKPORT_LINUX_U64_STATS_SYNC_H
#define __BACKPORT_LINUX_U64_STATS_SYNC_H

#include <linux/version.h>
#include <generated/utsrelease.h>
#include_next <linux/u64_stats_sync.h>

#if LINUX_VERSION_IS_LESS(4,16,0) && \
    !LINUX_VERSION_IN_RANGE(4,14,44, 4,15,0) && \
    !(LINUX_VERSION_IS_GEQ(4,15,18) && UTS_UBUNTU_RELEASE_ABI >= 33)
static inline unsigned long
u64_stats_update_begin_irqsave(struct u64_stats_sync *syncp)
{
	unsigned long flags = 0;

#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	local_irq_save(flags);
	write_seqcount_begin(&syncp->seq);
#endif
	return flags;
}

static inline void
u64_stats_update_end_irqrestore(struct u64_stats_sync *syncp,
				unsigned long flags)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_end(&syncp->seq);
	local_irq_restore(flags);
#endif
}
#endif /* < 4.16 */

#endif /* __BACKPORT_LINUX_U64_STATS_SYNC_H */
