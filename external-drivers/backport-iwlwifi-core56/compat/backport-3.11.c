/*
 * Copyright (c) 2016 Intel Deutschland GmbH
 *
 * Backport functionality introduced in Linux 3.11.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/scatterlist.h>

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
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
 * sg_miter_skip - reposition mapping iterator
 * @miter: sg mapping iter to be skipped
 * @offset: number of bytes to plus the current location
 *
 * Description:
 *   Sets the offset of @miter to its current location plus @offset bytes.
 *   If mapping iterator @miter has been proceeded by sg_miter_next(), this
 *   stops @miter.
 *
 * Context:
 *   Don't care if @miter is stopped, or not proceeded yet.
 *   Otherwise, preemption disabled if the SG_MITER_ATOMIC is set.
 *
 * Returns:
 *   true if @miter contains the valid mapping.  false if end of sg
 *   list is reached.
 */
static bool sg_miter_skip(struct sg_mapping_iter *miter, off_t offset)
{
	sg_miter_stop(miter);

	while (offset) {
		off_t consumed;

		if (!sg_miter_get_next_page(miter))
			return false;

		consumed = min_t(off_t, offset, miter->__remaining);
		miter->__offset += consumed;
		miter->__remaining -= consumed;
		offset -= consumed;
	}

	return true;
}

/**
 * sg_copy_buffer - Copy data between a linear buffer and an SG list
 * @sgl:		 The SG list
 * @nents:		 Number of SG entries
 * @buf:		 Where to copy from
 * @buflen:		 The number of bytes to copy
 * @skip:		 Number of bytes to skip before copying
 * @to_buffer:		 transfer direction (true == from an sg list to a
 *			 buffer, false == from a buffer to an sg list
 *
 * Returns the number of copied bytes.
 *
 **/
size_t sg_copy_buffer(struct scatterlist *sgl, unsigned int nents, void *buf,
		      size_t buflen, off_t skip, bool to_buffer)
{
	unsigned int offset = 0;
	struct sg_mapping_iter miter;
	unsigned long flags;
	unsigned int sg_flags = SG_MITER_ATOMIC;

	if (to_buffer)
		sg_flags |= SG_MITER_FROM_SG;
	else
		sg_flags |= SG_MITER_TO_SG;

	sg_miter_start(&miter, sgl, nents, sg_flags);

	if (!sg_miter_skip(&miter, skip))
		return false;

	local_irq_save(flags);

	while (sg_miter_next(&miter) && offset < buflen) {
		unsigned int len;

		len = min(miter.length, buflen - offset);

		if (to_buffer)
			memcpy(buf + offset, miter.addr, len);
		else
			memcpy(miter.addr, buf + offset, len);

		offset += len;
	}

	sg_miter_stop(&miter);

	local_irq_restore(flags);
	return offset;
}
EXPORT_SYMBOL_GPL(sg_copy_buffer);
#endif
