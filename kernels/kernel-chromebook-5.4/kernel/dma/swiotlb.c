// SPDX-License-Identifier: GPL-2.0-only
/*
 * Dynamic DMA mapping support.
 *
 * This implementation is a fallback for platforms that do not support
 * I/O TLBs (aka DMA address translation hardware).
 * Copyright (C) 2000 Asit Mallick <Asit.K.Mallick@intel.com>
 * Copyright (C) 2000 Goutham Rao <goutham.rao@intel.com>
 * Copyright (C) 2000, 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 03/05/07 davidm	Switch from PCI-DMA to generic device DMA API.
 * 00/12/13 davidm	Rename to swiotlb.c and add mark_clean() to avoid
 *			unnecessary i-cache flushing.
 * 04/07/.. ak		Better overflow handling. Assorted fixes.
 * 05/09/10 linville	Add support for syncing ranges, support syncing for
 *			DMA_BIDIRECTIONAL mappings, miscellaneous cleanup.
 * 08/12/11 beckyb	Add highmem support
 */

#define pr_fmt(fmt) "software IO TLB: " fmt

#include <linux/cache.h>
#include <linux/dma-direct.h>
#include <linux/mm.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/swiotlb.h>
#include <linux/pfn.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/scatterlist.h>
#include <linux/mem_encrypt.h>
#include <linux/set_memory.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#ifdef CONFIG_DMA_RESTRICTED_POOL
#include <linux/device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#endif

#include <asm/io.h>
#include <asm/dma.h>

#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/iommu-helper.h>

#define CREATE_TRACE_POINTS
#include <trace/events/swiotlb.h>

#define OFFSET(val,align) ((unsigned long)	\
	                   ( (val) & ( (align) - 1)))

#define SLABS_PER_PAGE (1 << (PAGE_SHIFT - IO_TLB_SHIFT))

/*
 * Minimum IO TLB size to bother booting with.  Systems with mainly
 * 64bit capable cards will only lightly use the swiotlb.  If we can't
 * allocate a contiguous 1MB, we're probably in trouble anyway.
 */
#define IO_TLB_MIN_SLABS ((1<<20) >> IO_TLB_SHIFT)
#define INVALID_PHYS_ADDR (~(phys_addr_t)0)

enum swiotlb_force swiotlb_force;

/*
 * struct swiotlb - Software IO TLB Memory Pool Descriptor
 *
 * @start:      The start address of the swiotlb memory pool. Used to do a quick
 *              range check to see if the memory was in fact allocated by this
 *              API.
 * @end:        The end address of the swiotlb memory pool. Used to do a quick
 *              range check to see if the memory was in fact allocated by this
 *              API.
 * @nslabs:     The number of IO TLB blocks (in groups of 64) between @start and
 *              @end. For default swiotlb, this is command line adjustable via
 *              setup_io_tlb_npages.
 * @used:       The number of used IO TLB block.
 * @list:       The free list describing the number of free entries available
 *              from each index.
 * @index:      The index to start searching in the next round.
 * @orig_addr:  The original address corresponding to a mapped entry for the
 *              sync operations.
 * @lock:       The lock to protect the above data structures in the map and
 *              unmap calls.
 * @debugfs:    The dentry to debugfs.
 */
struct swiotlb {
	phys_addr_t start;
	phys_addr_t end;
	unsigned long nslabs;
	unsigned long used;
	unsigned int *list;
	unsigned int index;
	phys_addr_t *orig_addr;
	spinlock_t lock;
	struct dentry *debugfs;
};
static struct swiotlb default_swiotlb;

static inline struct swiotlb *get_swiotlb(struct device *dev)
{
#ifdef CONFIG_DMA_RESTRICTED_POOL
	if (dev && dev->dev_swiotlb)
		return dev->dev_swiotlb;
#endif
	return &default_swiotlb;
}

/*
 * Max segment that we can provide which (if pages are contingous) will
 * not be bounced (unless SWIOTLB_FORCE is set).
 */
unsigned int max_segment;

static int late_alloc;

static int __init
setup_io_tlb_npages(char *str)
{
	struct swiotlb *swiotlb = &default_swiotlb;

	if (isdigit(*str)) {
		swiotlb->nslabs = simple_strtoul(str, &str, 0);
		/* avoid tail segment of size < IO_TLB_SEGSIZE */
		swiotlb->nslabs = ALIGN(swiotlb->nslabs, IO_TLB_SEGSIZE);
	}
	if (*str == ',')
		++str;
	if (!strcmp(str, "force")) {
		swiotlb_force = SWIOTLB_FORCE;
	} else if (!strcmp(str, "noforce")) {
		swiotlb_force = SWIOTLB_NO_FORCE;
		swiotlb->nslabs = 1;
	}

	return 0;
}
early_param("swiotlb", setup_io_tlb_npages);

static bool no_iotlb_memory;

unsigned long swiotlb_nr_tbl(void)
{
	return unlikely(no_iotlb_memory) ? 0 : default_swiotlb.nslabs;
}
EXPORT_SYMBOL_GPL(swiotlb_nr_tbl);

unsigned int swiotlb_max_segment(void)
{
	return unlikely(no_iotlb_memory) ? 0 : max_segment;
}
EXPORT_SYMBOL_GPL(swiotlb_max_segment);

void swiotlb_set_max_segment(unsigned int val)
{
	if (swiotlb_force == SWIOTLB_FORCE)
		max_segment = 1;
	else
		max_segment = rounddown(val, PAGE_SIZE);
}

/* default to 64MB */
#define IO_TLB_DEFAULT_SIZE (64UL<<20)
unsigned long swiotlb_size_or_default(void)
{
	unsigned long size;

	size = default_swiotlb.nslabs << IO_TLB_SHIFT;

	return size ? size : (IO_TLB_DEFAULT_SIZE);
}

void swiotlb_print_info(void)
{
	struct swiotlb *swiotlb = &default_swiotlb;
	unsigned long bytes = swiotlb->nslabs << IO_TLB_SHIFT;

	if (no_iotlb_memory) {
		pr_warn("No low mem\n");
		return;
	}

	pr_info("mapped [mem %pa-%pa] (%luMB)\n", &swiotlb->start, &swiotlb->end,
	       bytes >> 20);
}

/*
 * Early SWIOTLB allocation may be too early to allow an architecture to
 * perform the desired operations.  This function allows the architecture to
 * call SWIOTLB when the operations are possible.  It needs to be called
 * before the SWIOTLB memory is used.
 */
void __init swiotlb_update_mem_attributes(void)
{
	struct swiotlb *swiotlb = &default_swiotlb;
	void *vaddr;
	unsigned long bytes;

	if (no_iotlb_memory || late_alloc)
		return;

	vaddr = phys_to_virt(swiotlb->start);
	bytes = PAGE_ALIGN(swiotlb->nslabs << IO_TLB_SHIFT);
	set_memory_decrypted((unsigned long)vaddr, bytes >> PAGE_SHIFT);
	memset(vaddr, 0, bytes);
}

int __init swiotlb_init_with_tbl(char *tlb, unsigned long nslabs, int verbose)
{
	struct swiotlb *swiotlb = &default_swiotlb;
	unsigned long i, bytes;
	size_t alloc_size;

	bytes = nslabs << IO_TLB_SHIFT;

	swiotlb->nslabs = nslabs;
	swiotlb->start = __pa(tlb);
	swiotlb->end = swiotlb->start + bytes;

	/*
	 * Allocate and initialize the free list array.  This array is used
	 * to find contiguous free memory regions of size up to IO_TLB_SEGSIZE
	 * between swiotlb->start and swiotlb->end.
	 */
	alloc_size = PAGE_ALIGN(swiotlb->nslabs * sizeof(int));
	swiotlb->list = memblock_alloc(alloc_size, PAGE_SIZE);
	if (!swiotlb->list)
		panic("%s: Failed to allocate %zu bytes align=0x%lx\n",
		      __func__, alloc_size, PAGE_SIZE);

	alloc_size = PAGE_ALIGN(swiotlb->nslabs * sizeof(phys_addr_t));
	swiotlb->orig_addr = memblock_alloc(alloc_size, PAGE_SIZE);
	if (!swiotlb->orig_addr)
		panic("%s: Failed to allocate %zu bytes align=0x%lx\n",
		      __func__, alloc_size, PAGE_SIZE);

	for (i = 0; i < swiotlb->nslabs; i++) {
		swiotlb->list[i] = IO_TLB_SEGSIZE - OFFSET(i, IO_TLB_SEGSIZE);
		swiotlb->orig_addr[i] = INVALID_PHYS_ADDR;
	}
	swiotlb->index = 0;
	no_iotlb_memory = false;

	if (verbose)
		swiotlb_print_info();

	swiotlb_set_max_segment(swiotlb->nslabs << IO_TLB_SHIFT);
	spin_lock_init(&swiotlb->lock);

	return 0;
}

/*
 * Statically reserve bounce buffer space and initialize bounce buffer data
 * structures for the software IO TLB used to implement the DMA API.
 */
void  __init
swiotlb_init(int verbose)
{
	struct swiotlb *swiotlb = &default_swiotlb;
	size_t default_size = IO_TLB_DEFAULT_SIZE;
	unsigned char *vstart;
	unsigned long bytes;

	if (!swiotlb->nslabs) {
		swiotlb->nslabs = (default_size >> IO_TLB_SHIFT);
		swiotlb->nslabs = ALIGN(swiotlb->nslabs, IO_TLB_SEGSIZE);
	}

	bytes = swiotlb->nslabs << IO_TLB_SHIFT;

	/* Get IO TLB memory from the low pages */
	vstart = memblock_alloc_low(PAGE_ALIGN(bytes), PAGE_SIZE);
	if (vstart && !swiotlb_init_with_tbl(vstart, swiotlb->nslabs, verbose))
		return;

	if (swiotlb->start) {
		memblock_free_early(swiotlb->start,
				    PAGE_ALIGN(swiotlb->nslabs << IO_TLB_SHIFT));
		swiotlb->start = 0;
	}
	pr_warn("Cannot allocate buffer");
	no_iotlb_memory = true;
}

/*
 * Systems with larger DMA zones (those that don't support ISA) can
 * initialize the swiotlb later using the slab allocator if needed.
 * This should be just like above, but with some error catching.
 */
int
swiotlb_late_init_with_default_size(size_t default_size)
{
	struct swiotlb *swiotlb = &default_swiotlb;
	unsigned long bytes, req_nslabs = swiotlb->nslabs;
	unsigned char *vstart = NULL;
	unsigned int order;
	int rc = 0;

	if (!swiotlb->nslabs) {
		swiotlb->nslabs = (default_size >> IO_TLB_SHIFT);
		swiotlb->nslabs = ALIGN(swiotlb->nslabs, IO_TLB_SEGSIZE);
	}

	/*
	 * Get IO TLB memory from the low pages
	 */
	order = get_order(swiotlb->nslabs << IO_TLB_SHIFT);
	swiotlb->nslabs = SLABS_PER_PAGE << order;
	bytes = swiotlb->nslabs << IO_TLB_SHIFT;

	while ((SLABS_PER_PAGE << order) > IO_TLB_MIN_SLABS) {
		vstart = (void *)__get_free_pages(GFP_DMA | __GFP_NOWARN,
						  order);
		if (vstart)
			break;
		order--;
	}

	if (!vstart) {
		swiotlb->nslabs = req_nslabs;
		return -ENOMEM;
	}
	if (order != get_order(bytes)) {
		pr_warn("only able to allocate %ld MB\n",
			(PAGE_SIZE << order) >> 20);
		swiotlb->nslabs = SLABS_PER_PAGE << order;
	}
	rc = swiotlb_late_init_with_tbl(vstart, swiotlb->nslabs);
	if (rc)
		free_pages((unsigned long)vstart, order);

	return rc;
}

static void swiotlb_cleanup(void)
{
	struct swiotlb *swiotlb = &default_swiotlb;

	swiotlb->end = 0;
	swiotlb->start = 0;
	swiotlb->nslabs = 0;
	max_segment = 0;
}

static int swiotlb_init_tlb_pool(struct swiotlb *swiotlb, phys_addr_t start,
				size_t size)
{
	unsigned long i;
	void *vaddr = phys_to_virt(start);

	size = ALIGN(size, 1 << IO_TLB_SHIFT);
	swiotlb->nslabs = size >> IO_TLB_SHIFT;
	swiotlb->nslabs = ALIGN(swiotlb->nslabs, IO_TLB_SEGSIZE);

	swiotlb->start = start;
	swiotlb->end = swiotlb->start + size;

	set_memory_decrypted((unsigned long)vaddr, size >> PAGE_SHIFT);
	memset(vaddr, 0, size);

	/*
	 * Allocate and initialize the free list array.  This array is used
	 * to find contiguous free memory regions of size up to IO_TLB_SEGSIZE
	 * between swiotlb->start and swiotlb->end.
	 */
	swiotlb->list = (unsigned int *)__get_free_pages(GFP_KERNEL,
	                              get_order(swiotlb->nslabs * sizeof(int)));
	if (!swiotlb->list)
		goto cleanup3;

	swiotlb->orig_addr = (phys_addr_t *)
		__get_free_pages(GFP_KERNEL,
				 get_order(swiotlb->nslabs *
					   sizeof(phys_addr_t)));
	if (!swiotlb->orig_addr)
		goto cleanup4;

	for (i = 0; i < swiotlb->nslabs; i++) {
		swiotlb->list[i] = IO_TLB_SEGSIZE - OFFSET(i, IO_TLB_SEGSIZE);
		swiotlb->orig_addr[i] = INVALID_PHYS_ADDR;
	}
	swiotlb->index = 0;

	spin_lock_init(&swiotlb->lock);

	return 0;

cleanup4:
	free_pages((unsigned long)swiotlb->list,
		   get_order(swiotlb->nslabs * sizeof(int)));
	swiotlb->list = NULL;
cleanup3:
	swiotlb_cleanup();
	return -ENOMEM;
}

int swiotlb_late_init_with_tbl(char *tlb, unsigned long nslabs)
{
	struct swiotlb *swiotlb = &default_swiotlb;
	unsigned long bytes = nslabs << IO_TLB_SHIFT;
	int ret;

	ret = swiotlb_init_tlb_pool(swiotlb, virt_to_phys(tlb), bytes);
	if (ret)
		return ret;

	no_iotlb_memory = false;

	swiotlb_print_info();

	late_alloc = 1;

	swiotlb_set_max_segment(bytes);

	return 0;
}

void __init swiotlb_exit(void)
{
	struct swiotlb *swiotlb = &default_swiotlb;

	if (!swiotlb->orig_addr)
		return;

	if (late_alloc) {
		free_pages((unsigned long)swiotlb->orig_addr,
			   get_order(swiotlb->nslabs * sizeof(phys_addr_t)));
		free_pages((unsigned long)swiotlb->list,
			   get_order(swiotlb->nslabs * sizeof(int)));
		free_pages((unsigned long)phys_to_virt(swiotlb->start),
			   get_order(swiotlb->nslabs << IO_TLB_SHIFT));
	} else {
		memblock_free_late(__pa(swiotlb->orig_addr),
				   PAGE_ALIGN(swiotlb->nslabs * sizeof(phys_addr_t)));
		memblock_free_late(__pa(swiotlb->list),
				   PAGE_ALIGN(swiotlb->nslabs * sizeof(int)));
		memblock_free_late(swiotlb->start,
				   PAGE_ALIGN(swiotlb->nslabs << IO_TLB_SHIFT));
	}
	swiotlb_cleanup();
}

/*
 * Bounce: copy the swiotlb buffer from or back to the original dma location
 */
static void swiotlb_bounce(phys_addr_t orig_addr, phys_addr_t tlb_addr,
			   size_t size, enum dma_data_direction dir)
{
	unsigned long pfn = PFN_DOWN(orig_addr);
	unsigned char *vaddr = phys_to_virt(tlb_addr);

	if (PageHighMem(pfn_to_page(pfn))) {
		/* The buffer does not have a mapping.  Map it in and copy */
		unsigned int offset = orig_addr & ~PAGE_MASK;
		char *buffer;
		unsigned int sz = 0;
		unsigned long flags;

		while (size) {
			sz = min_t(size_t, PAGE_SIZE - offset, size);

			local_irq_save(flags);
			buffer = kmap_atomic(pfn_to_page(pfn));
			if (dir == DMA_TO_DEVICE)
				memcpy(vaddr, buffer + offset, sz);
			else
				memcpy(buffer + offset, vaddr, sz);
			kunmap_atomic(buffer);
			local_irq_restore(flags);

			size -= sz;
			pfn++;
			vaddr += sz;
			offset = 0;
		}
	} else if (dir == DMA_TO_DEVICE) {
		memcpy(vaddr, phys_to_virt(orig_addr), size);
	} else {
		memcpy(phys_to_virt(orig_addr), vaddr, size);
	}
}

static int swiotlb_tbl_find_free_region(struct device *hwdev,
					dma_addr_t tbl_dma_addr,
					size_t alloc_size, unsigned long attrs)
{
	struct swiotlb *swiotlb = get_swiotlb(hwdev);
	unsigned long flags;
	unsigned int nslots, stride, index, wrap;
	int i;
	unsigned long mask;
	unsigned long offset_slots;
	unsigned long max_slots;
	unsigned long tmp_io_tlb_used;

#ifdef CONFIG_DMA_RESTRICTED_POOL
	if (no_iotlb_memory && !hwdev->dev_swiotlb)
#else
	if (no_iotlb_memory)
#endif
		panic("Can not allocate SWIOTLB buffer earlier and can't now provide you with the DMA bounce buffer");

	mask = dma_get_seg_boundary(hwdev);

	tbl_dma_addr &= mask;

	offset_slots = ALIGN(tbl_dma_addr, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;

	/*
	 * Carefully handle integer overflow which can occur when mask == ~0UL.
	 */
	max_slots = mask + 1
		    ? ALIGN(mask + 1, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT
		    : 1UL << (BITS_PER_LONG - IO_TLB_SHIFT);

	/*
	 * For mappings greater than or equal to a page, we limit the stride
	 * (and hence alignment) to a page size.
	 */
	nslots = ALIGN(alloc_size, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;
	if (alloc_size >= PAGE_SIZE)
		stride = (1 << (PAGE_SHIFT - IO_TLB_SHIFT));
	else
		stride = 1;

	BUG_ON(!nslots);

	/*
	 * Find suitable number of IO TLB entries size that will fit this
	 * request and allocate a buffer from that IO TLB pool.
	 */
	spin_lock_irqsave(&swiotlb->lock, flags);

	if (unlikely(nslots > swiotlb->nslabs - swiotlb->used))
		goto not_found;

	index = ALIGN(swiotlb->index, stride);
	if (index >= swiotlb->nslabs)
		index = 0;
	wrap = index;

	do {
		while (iommu_is_span_boundary(index, nslots, offset_slots,
					      max_slots)) {
			index += stride;
			if (index >= swiotlb->nslabs)
				index = 0;
			if (index == wrap)
				goto not_found;
		}

		/*
		 * If we find a slot that indicates we have 'nslots' number of
		 * contiguous buffers, we allocate the buffers from that slot
		 * and mark the entries as '0' indicating unavailable.
		 */
		if (swiotlb->list[index] >= nslots) {
			int count = 0;

			for (i = index; i < (int) (index + nslots); i++)
				swiotlb->list[i] = 0;
			for (i = index - 1; (OFFSET(i, IO_TLB_SEGSIZE) != IO_TLB_SEGSIZE - 1) && swiotlb->list[i]; i--)
				swiotlb->list[i] = ++count;

			/*
			 * Update the indices to avoid searching in the next
			 * round.
			 */
			swiotlb->index = ((index + nslots) < swiotlb->nslabs
				      ? (index + nslots) : 0);

			goto found;
		}
		index += stride;
		if (index >= swiotlb->nslabs)
			index = 0;
	} while (index != wrap);

not_found:
	tmp_io_tlb_used = swiotlb->used;

	spin_unlock_irqrestore(&swiotlb->lock, flags);
	if (!(attrs & DMA_ATTR_NO_WARN) && printk_ratelimit())
		dev_warn(hwdev, "swiotlb buffer is full (sz: %zd bytes), total %lu (slots), used %lu (slots)\n",
			 alloc_size, swiotlb->nslabs, tmp_io_tlb_used);
	return -ENOMEM;

found:
	swiotlb->used += nslots;
	spin_unlock_irqrestore(&swiotlb->lock, flags);

	return index;
}

static void swiotlb_tbl_release_region(struct device *hwdev, int index, size_t size)
{
	struct swiotlb *swiotlb = get_swiotlb(hwdev);
	unsigned long flags;
	int i, count, nslots = ALIGN(size, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;

	/*
	 * Return the buffer to the free list by setting the corresponding
	 * entries to indicate the number of contiguous entries available.
	 * While returning the entries to the free list, we merge the entries
	 * with slots below and above the pool being returned.
	 */
	spin_lock_irqsave(&swiotlb->lock, flags);
	{
		count = ((index + nslots) < ALIGN(index + 1, IO_TLB_SEGSIZE) ?
			 swiotlb->list[index + nslots] : 0);
		/*
		 * Step 1: return the slots to the free list, merging the
		 * slots with superceeding slots
		 */
		for (i = index + nslots - 1; i >= index; i--) {
			swiotlb->list[i] = ++count;
			swiotlb->orig_addr[i] = INVALID_PHYS_ADDR;
		}
		/*
		 * Step 2: merge the returned slots with the preceding slots,
		 * if available (non zero)
		 */
		for (i = index - 1; (OFFSET(i, IO_TLB_SEGSIZE) != IO_TLB_SEGSIZE -1) && swiotlb->list[i]; i--)
			swiotlb->list[i] = ++count;

		swiotlb->used -= nslots;
	}
	spin_unlock_irqrestore(&swiotlb->lock, flags);
}

phys_addr_t swiotlb_tbl_map_single(struct device *hwdev,
				   dma_addr_t tbl_dma_addr,
				   phys_addr_t orig_addr,
				   size_t mapping_size,
				   size_t alloc_size,
				   enum dma_data_direction dir,
				   unsigned long attrs)
{
	struct swiotlb *swiotlb = get_swiotlb(hwdev);
	phys_addr_t tlb_addr;
	int nslots, index;
	int i;

	if (mem_encrypt_active())
		pr_warn_once("Memory encryption is active and system is using DMA bounce buffers\n");

	if (mapping_size > alloc_size) {
		dev_warn_once(hwdev, "Invalid sizes (mapping: %zd bytes, alloc: %zd bytes)",
			      mapping_size, alloc_size);
		return (phys_addr_t)DMA_MAPPING_ERROR;
	}

	index = swiotlb_tbl_find_free_region(hwdev, tbl_dma_addr, alloc_size, attrs);
	if (index < 0)
		return (phys_addr_t)DMA_MAPPING_ERROR;

	tlb_addr = swiotlb->start + (index << IO_TLB_SHIFT);

	/*
	 * Save away the mapping from the original address to the DMA address.
	 * This is needed when we sync the memory.  Then we sync the buffer if
	 * needed.
	 */
	nslots = ALIGN(alloc_size, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;
	for (i = 0; i < nslots; i++)
		swiotlb->orig_addr[index + i] = orig_addr + (i << IO_TLB_SHIFT);
	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC) &&
	    (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL))
		swiotlb_bounce(orig_addr, tlb_addr, mapping_size, DMA_TO_DEVICE);

	return tlb_addr;
}

/*
 * tlb_addr is the physical address of the bounce buffer to unmap.
 */
void swiotlb_tbl_unmap_single(struct device *hwdev, phys_addr_t tlb_addr,
			      size_t mapping_size, size_t alloc_size,
			      enum dma_data_direction dir, unsigned long attrs)
{
	struct swiotlb *swiotlb = get_swiotlb(hwdev);
	int index = (tlb_addr - swiotlb->start) >> IO_TLB_SHIFT;
	phys_addr_t orig_addr = swiotlb->orig_addr[index];

	/*
	 * First, sync the memory before unmapping the entry
	 */
	if (orig_addr != INVALID_PHYS_ADDR &&
	    !(attrs & DMA_ATTR_SKIP_CPU_SYNC) &&
	    ((dir == DMA_FROM_DEVICE) || (dir == DMA_BIDIRECTIONAL)))
		swiotlb_bounce(orig_addr, tlb_addr, mapping_size, DMA_FROM_DEVICE);

	swiotlb_tbl_release_region(hwdev, index, alloc_size);
}

void swiotlb_tbl_sync_single(struct device *hwdev, phys_addr_t tlb_addr,
			     size_t size, enum dma_data_direction dir,
			     enum dma_sync_target target)
{
	struct swiotlb *swiotlb = get_swiotlb(hwdev);
	int index = (tlb_addr - swiotlb->start) >> IO_TLB_SHIFT;
	phys_addr_t orig_addr = swiotlb->orig_addr[index];

	if (orig_addr == INVALID_PHYS_ADDR)
		return;
	orig_addr += (unsigned long)tlb_addr & ((1 << IO_TLB_SHIFT) - 1);

	switch (target) {
	case SYNC_FOR_CPU:
		if (likely(dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL))
			swiotlb_bounce(orig_addr, tlb_addr,
				       size, DMA_FROM_DEVICE);
		else
			BUG_ON(dir != DMA_TO_DEVICE);
		break;
	case SYNC_FOR_DEVICE:
		if (likely(dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL))
			swiotlb_bounce(orig_addr, tlb_addr,
				       size, DMA_TO_DEVICE);
		else
			BUG_ON(dir != DMA_FROM_DEVICE);
		break;
	default:
		BUG();
	}
}

/*
 * Create a swiotlb mapping for the buffer at @phys, and in case of DMAing
 * to the device copy the data into it as well.
 */
bool swiotlb_map(struct device *dev, phys_addr_t *phys, dma_addr_t *dma_addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	struct swiotlb *swiotlb = get_swiotlb(dev);

	trace_swiotlb_bounced(dev, *dma_addr, size, swiotlb_force);

	if (unlikely(swiotlb_force == SWIOTLB_NO_FORCE)) {
		dev_warn_ratelimited(dev,
			"Cannot do DMA to address %pa\n", phys);
		return false;
	}

	/* Oh well, have to allocate and map a bounce buffer. */
	*phys = swiotlb_tbl_map_single(dev, __phys_to_dma(dev, swiotlb->start),
			*phys, size, size, dir, attrs);
	if (*phys == (phys_addr_t)DMA_MAPPING_ERROR)
		return false;

	/* Ensure that the address returned is DMA'ble */
	*dma_addr = __phys_to_dma(dev, *phys);
	if (unlikely(!dma_capable(dev, *dma_addr, size))) {
		swiotlb_tbl_unmap_single(dev, *phys, size, size, dir,
			attrs | DMA_ATTR_SKIP_CPU_SYNC);
		return false;
	}

	return true;
}

size_t swiotlb_max_mapping_size(struct device *dev)
{
	return ((size_t)1 << IO_TLB_SHIFT) * IO_TLB_SEGSIZE;
}

bool is_swiotlb_active(struct device *dev)
{
	struct swiotlb *swiotlb = get_swiotlb(dev);

	/*
	 * When SWIOTLB is initialized, even if swiotlb->start points to
	 * physical address zero, swiotlb->end surely doesn't.
	 */
	return swiotlb->end != 0;
}

bool is_swiotlb_buffer(struct device *dev, phys_addr_t paddr)
{
	struct swiotlb *swiotlb = get_swiotlb(dev);

	return paddr >= swiotlb->start && paddr < swiotlb->end;
}

phys_addr_t get_swiotlb_start(struct device *dev)
{
	struct swiotlb *swiotlb = get_swiotlb(dev);

	return swiotlb->start;
}

#ifdef CONFIG_DEBUG_FS
static void swiotlb_create_debugfs(struct swiotlb *swiotlb, const char *name,
				   struct dentry *node)
{
	swiotlb->debugfs = debugfs_create_dir(name, node);
	debugfs_create_ulong("io_tlb_nslabs", 0400, swiotlb->debugfs, &swiotlb->nslabs);
	debugfs_create_ulong("io_tlb_used", 0400, swiotlb->debugfs, &swiotlb->used);
}

static int __init swiotlb_create_default_debugfs(void)
{
	swiotlb_create_debugfs(&default_swiotlb, "swiotlb", NULL);

	return 0;
}

late_initcall(swiotlb_create_default_debugfs);
#endif

#ifdef CONFIG_DMA_RESTRICTED_POOL
struct page *dev_swiotlb_alloc(struct device *dev, size_t size)
{
	struct swiotlb *swiotlb;
	phys_addr_t tlb_addr;
	int index;

	if (!dev->dev_swiotlb)
		return NULL;

	swiotlb = dev->dev_swiotlb;
	index = swiotlb_tbl_find_free_region(dev, swiotlb->start, size, 0);
	if (index < 0)
		return NULL;

	tlb_addr = swiotlb->start + (index << IO_TLB_SHIFT);

	return pfn_to_page(PFN_DOWN(tlb_addr));
}

bool dev_swiotlb_free(struct device *dev, struct page *page, size_t size)
{
	unsigned int index;
	phys_addr_t tlb_addr = page_to_phys(page);

	if (!is_swiotlb_buffer(dev, tlb_addr))
		return false;

	index = (tlb_addr - dev->dev_swiotlb->start) >> IO_TLB_SHIFT;
	swiotlb_tbl_release_region(dev, index, size);

	return true;
}

bool is_swiotlb_force(struct device *dev)
{
	return unlikely(swiotlb_force == SWIOTLB_FORCE) || dev->dev_swiotlb;
}

bool is_dev_swiotlb_force(struct device *dev)
{
	return dev->dev_swiotlb;
}

static int rmem_swiotlb_device_init(struct reserved_mem *rmem,
				    struct device *dev)
{
	struct swiotlb *swiotlb = rmem->priv;
	int ret;

	if (dev->dev_swiotlb)
		return -EBUSY;

	/* Since multiple devices can share the same pool, the private data,
	 * swiotlb struct, will be initialized by the first device attached
	 * to it.
	 */
	if (!swiotlb) {
		swiotlb = kzalloc(sizeof(*swiotlb), GFP_KERNEL);
		if (!swiotlb)
			return -ENOMEM;
#ifdef CONFIG_ARM
		if (!PageHighMem(pfn_to_page(PHYS_PFN(rmem->base)))) {
			ret = -EINVAL;
			goto cleanup;
		}
#endif /* CONFIG_ARM */

		ret = swiotlb_init_tlb_pool(swiotlb, rmem->base, rmem->size);
		if (ret)
			goto cleanup;

		rmem->priv = swiotlb;
	}

#ifdef CONFIG_DEBUG_FS
	swiotlb_create_debugfs(swiotlb, rmem->name, default_swiotlb.debugfs);
#endif /* CONFIG_DEBUG_FS */

	dev->dev_swiotlb = swiotlb;

	return 0;

cleanup:
	kfree(swiotlb);

	return ret;
}

static void rmem_swiotlb_device_release(struct reserved_mem *rmem,
					struct device *dev)
{
	if (!dev)
		return;

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(dev->dev_swiotlb->debugfs);
#endif /* CONFIG_DEBUG_FS */
	dev->dev_swiotlb = NULL;
}

static const struct reserved_mem_ops rmem_swiotlb_ops = {
	.device_init = rmem_swiotlb_device_init,
	.device_release = rmem_swiotlb_device_release,
};

static int __init rmem_swiotlb_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;

	if (of_get_flat_dt_prop(node, "reusable", NULL) ||
	    of_get_flat_dt_prop(node, "linux,cma-default", NULL) ||
	    of_get_flat_dt_prop(node, "linux,dma-default", NULL) ||
	    of_get_flat_dt_prop(node, "no-map", NULL))
		return -EINVAL;

	rmem->ops = &rmem_swiotlb_ops;
	pr_info("Reserved memory: created device swiotlb memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

RESERVEDMEM_OF_DECLARE(dma, "restricted-dma-pool", rmem_swiotlb_setup);
#endif /* CONFIG_DMA_RESTRICTED_POOL */
