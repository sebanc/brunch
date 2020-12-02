/*
 * Copyright (c) 2013  Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * Backport functionality introduced in Linux 3.9.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <net/inet_frag.h>
#include <net/sock.h>

void __iomem *devm_ioremap_resource(struct device *dev, struct resource *res)
{
	void __iomem *dest_ptr;

	dest_ptr = devm_ioremap_resource(dev, res);
	if (!dest_ptr)
		return (void __iomem *)ERR_PTR(-ENOMEM);
	return dest_ptr;
}
EXPORT_SYMBOL_GPL(devm_ioremap_resource);

/**
 * eth_prepare_mac_addr_change - prepare for mac change
 * @dev: network device
 * @p: socket address
 */
int eth_prepare_mac_addr_change(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (!(dev->priv_flags & IFF_LIVE_ADDR_CHANGE) && netif_running(dev))
		return -EBUSY;
	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	return 0;
}
EXPORT_SYMBOL_GPL(eth_prepare_mac_addr_change);

/**
 * eth_commit_mac_addr_change - commit mac change
 * @dev: network device
 * @p: socket address
 */
void eth_commit_mac_addr_change(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
}
EXPORT_SYMBOL_GPL(eth_commit_mac_addr_change);

void __sg_page_iter_start(struct sg_page_iter *piter,
			  struct scatterlist *sglist, unsigned int nents,
			  unsigned long pgoffset)
{
	piter->__pg_advance = 0;
	piter->__nents = nents;

	piter->page = NULL;
	piter->sg = sglist;
	piter->sg_pgoffset = pgoffset;
}
EXPORT_SYMBOL_GPL(__sg_page_iter_start);

static int sg_page_count(struct scatterlist *sg)
{
	return PAGE_ALIGN(sg->offset + sg->length) >> PAGE_SHIFT;
}

bool __sg_page_iter_next(struct sg_page_iter *piter)
{
	if (!piter->__nents || !piter->sg)
		return false;

	piter->sg_pgoffset += piter->__pg_advance;
	piter->__pg_advance = 1;

	while (piter->sg_pgoffset >= sg_page_count(piter->sg)) {
		piter->sg_pgoffset -= sg_page_count(piter->sg);
		piter->sg = sg_next(piter->sg);
		if (!--piter->__nents || !piter->sg)
			return false;
	}
	piter->page = nth_page(sg_page(piter->sg), piter->sg_pgoffset);

	return true;
}
EXPORT_SYMBOL_GPL(__sg_page_iter_next);

static bool sg_miter_get_next_page(struct sg_mapping_iter *miter)
{
	if (!miter->__remaining) {
		struct scatterlist *sg;
		unsigned long pgoffset;

		if (!__sg_page_iter_next(&miter->piter))
			return false;

		sg = miter->piter.sg;
		pgoffset = miter->piter.sg_pgoffset;

		miter->__offset = pgoffset ? 0 : sg->offset;
		miter->__remaining = sg->offset + sg->length -
				(pgoffset << PAGE_SHIFT) - miter->__offset;
		miter->__remaining = min_t(unsigned long, miter->__remaining,
					   PAGE_SIZE - miter->__offset);
	}

	return true;
}

/**
 * sg_miter_start - start mapping iteration over a sg list
 * @miter: sg mapping iter to be started
 * @sgl: sg list to iterate over
 * @nents: number of sg entries
 *
 * Description:
 *   Starts mapping iterator @miter.
 *
 * Context:
 *   Don't care.
 */
void backport_sg_miter_start(struct sg_mapping_iter *miter, struct scatterlist *sgl,
		    unsigned int nents, unsigned int flags)
{
	memset(miter, 0, sizeof(struct sg_mapping_iter));

	__sg_page_iter_start(&miter->piter, sgl, nents, 0);
	WARN_ON(!(flags & (SG_MITER_TO_SG | SG_MITER_FROM_SG)));
	miter->__flags = flags;
}
EXPORT_SYMBOL_GPL(backport_sg_miter_start);

/**
 * sg_miter_next - proceed mapping iterator to the next mapping
 * @miter: sg mapping iter to proceed
 *
 * Description:
 *   Proceeds @miter to the next mapping.  @miter should have been started
 *   using sg_miter_start().  On successful return, @miter->page,
 *   @miter->addr and @miter->length point to the current mapping.
 *
 * Context:
 *   Preemption disabled if SG_MITER_ATOMIC.  Preemption must stay disabled
 *   till @miter is stopped.  May sleep if !SG_MITER_ATOMIC.
 *
 * Returns:
 *   true if @miter contains the next mapping.  false if end of sg
 *   list is reached.
 */
bool backport_sg_miter_next(struct sg_mapping_iter *miter)
{
	sg_miter_stop(miter);

	/*
	 * Get to the next page if necessary.
	 * __remaining, __offset is adjusted by sg_miter_stop
	 */
	if (!sg_miter_get_next_page(miter))
		return false;

	miter->page = sg_page_iter_page(&miter->piter);
	miter->consumed = miter->length = miter->__remaining;

	if (miter->__flags & SG_MITER_ATOMIC)
		miter->addr = kmap_atomic(miter->page) + miter->__offset;
	else
		miter->addr = kmap(miter->page) + miter->__offset;

	return true;
}
EXPORT_SYMBOL_GPL(backport_sg_miter_next);

/**
 * sg_miter_stop - stop mapping iteration
 * @miter: sg mapping iter to be stopped
 *
 * Description:
 *   Stops mapping iterator @miter.  @miter should have been started
 *   using sg_miter_start().  A stopped iteration can be resumed by
 *   calling sg_miter_next() on it.  This is useful when resources (kmap)
 *   need to be released during iteration.
 *
 * Context:
 *   Preemption disabled if the SG_MITER_ATOMIC is set.  Don't care
 *   otherwise.
 */
void backport_sg_miter_stop(struct sg_mapping_iter *miter)
{
	WARN_ON(miter->consumed > miter->length);

	/* drop resources from the last iteration */
	if (miter->addr) {
		miter->__offset += miter->consumed;
		miter->__remaining -= miter->consumed;

		if ((miter->__flags & SG_MITER_TO_SG) &&
		    !PageSlab(miter->page))
			flush_kernel_dcache_page(miter->page);

		if (miter->__flags & SG_MITER_ATOMIC) {
			WARN_ON_ONCE(preemptible());
			kunmap_atomic(miter->addr);
		} else
			kunmap(miter->page);

		miter->page = NULL;
		miter->addr = NULL;
		miter->length = 0;
		miter->consumed = 0;
	}
}
EXPORT_SYMBOL_GPL(backport_sg_miter_stop);
