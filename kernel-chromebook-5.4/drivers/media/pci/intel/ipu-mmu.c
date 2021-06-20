// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2020 Intel Corporation

#include <asm/cacheflush.h>

#include <linux/device.h>
#include <linux/iova.h>
#include <linux/module.h>
#include <linux/sizes.h>

#include "ipu.h"
#include "ipu-platform.h"
#include "ipu-dma.h"
#include "ipu-mmu.h"
#include "ipu-platform-regs.h"

#define ISP_PAGE_SHIFT		12
#define ISP_PAGE_SIZE		BIT(ISP_PAGE_SHIFT)
#define ISP_PAGE_MASK		(~(ISP_PAGE_SIZE - 1))

#define ISP_L1PT_SHIFT		22
#define ISP_L1PT_MASK		(~((1U << ISP_L1PT_SHIFT) - 1))

#define ISP_L2PT_SHIFT		12
#define ISP_L2PT_MASK		(~(ISP_L1PT_MASK | (~(ISP_PAGE_MASK))))

#define ISP_L1PT_PTES           1024
#define ISP_L2PT_PTES           1024

#define ISP_PADDR_SHIFT		12

#define REG_TLB_INVALIDATE	0x0000

#define REG_L1_PHYS		0x0004	/* 27-bit pfn */
#define REG_INFO		0x0008

/* The range of stream ID i in L1 cache is from 0 to 15 */
#define MMUV2_REG_L1_STREAMID(i)	(0x0c + ((i) * 4))

/* The range of stream ID i in L2 cache is from 0 to 15 */
#define MMUV2_REG_L2_STREAMID(i)	(0x4c + ((i) * 4))

/* ZLW Enable for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_EN_SID(i)		(0x100 + ((i) * 0x20))

/* ZLW 1D mode Enable for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_1DMODE_SID(i)	(0x100 + ((i) * 0x20) + 0x0004)

/* Set ZLW insertion N pages ahead per stream 1D where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_INS_N_AHEAD_SID(i)	(0x100 + ((i) * 0x20) + 0x0008)

/* ZLW 2D mode Enable for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_2DMODE_SID(i)	(0x100 + ((i) * 0x20) + 0x0010)

/* ZLW Insertion for each stream in L1 MMU AT where i : 0..15 */
#define MMUV2_AT_REG_L1_ZLW_INSERTION(i)	(0x100 + ((i) * 0x20) + 0x000c)

#define MMUV2_AT_REG_L1_FW_ZLW_FIFO		(0x100 + \
			(IPU_MMU_MAX_TLB_L1_STREAMS * 0x20) + 0x003c)

/* FW ZLW has prioty - needed for ZLW invalidations */
#define MMUV2_AT_REG_L1_FW_ZLW_PRIO		(0x100 + \
			(IPU_MMU_MAX_TLB_L1_STREAMS * 0x20))

#define TBL_PHYS_ADDR(a)	((phys_addr_t)(a) << ISP_PADDR_SHIFT)
#define TBL_VIRT_ADDR(a)	phys_to_virt(TBL_PHYS_ADDR(a))

static void zlw_invalidate(struct ipu_mmu *mmu, struct ipu_mmu_hw *mmu_hw)
{
	unsigned int retry = 0;
	unsigned int i, j;
	int ret;

	for (i = 0; i < mmu_hw->nr_l1streams; i++) {
		/* We need to invalidate only the zlw enabled stream IDs */
		if (mmu_hw->l1_zlw_en[i]) {
			/*
			 * Maximum 16 blocks per L1 stream
			 * Write trash buffer iova offset to the FW_ZLW
			 * register. This will trigger pre-fetching of next 16
			 * pages from the page table. So we need to increment
			 * iova address by 16 * 4K to trigger the next 16 pages.
			 * Once this loop is completed, the L1 cache will be
			 * filled with trash buffer translation.
			 *
			 * TODO: Instead of maximum 16 blocks, use the allocated
			 * block size
			 */
			for (j = 0; j < mmu_hw->l1_block_sz[i]; j++)
				writel(mmu->iova_addr_trash +
					   j * MMUV2_TRASH_L1_BLOCK_OFFSET,
					   mmu_hw->base +
					   MMUV2_AT_REG_L1_ZLW_INSERTION(i));

			/*
			 * Now we need to fill the L2 cache entry. L2 cache
			 * entries will be automatically updated, based on the
			 * L1 entry. The above loop for L1 will update only one
			 * of the two entries in L2 as the L1 is under 4MB
			 * range. To force the other entry in L2 to update, we
			 * just need to trigger another pre-fetch which is
			 * outside the above 4MB range.
			 */
			writel(mmu->iova_addr_trash +
				   MMUV2_TRASH_L2_BLOCK_OFFSET,
				   mmu_hw->base +
				   MMUV2_AT_REG_L1_ZLW_INSERTION(0));
		}
	}

	/*
	 * Wait until AT is ready. FIFO read should return 2 when AT is ready.
	 * Retry value of 1000 is just by guess work to avoid the forever loop.
	 */
	do {
		if (retry > 1000) {
			dev_err(mmu->dev, "zlw invalidation failed\n");
			return;
		}
		ret = readl(mmu_hw->base + MMUV2_AT_REG_L1_FW_ZLW_FIFO);
		retry++;
	} while (ret != 2);
}

static void tlb_invalidate(struct ipu_mmu *mmu)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&mmu->ready_lock, flags);
	if (!mmu->ready) {
		spin_unlock_irqrestore(&mmu->ready_lock, flags);
		return;
	}

	for (i = 0; i < mmu->nr_mmus; i++) {
		/*
		 * To avoid the HW bug induced dead lock in some of the IPU4
		 * MMUs on successive invalidate calls, we need to first do a
		 * read to the page table base before writing the invalidate
		 * register. MMUs which need to implement this WA, will have
		 * the insert_read_before_invalidate flasg set as true.
		 * Disregard the return value of the read.
		 */
		if (mmu->mmu_hw[i].insert_read_before_invalidate)
			readl(mmu->mmu_hw[i].base + REG_L1_PHYS);

		/* Normal invalidate or zlw invalidate */
		if (mmu->mmu_hw[i].zlw_invalidate) {
			/* trash buffer must be mapped by now, just in case! */
			WARN_ON(!mmu->iova_addr_trash);

			zlw_invalidate(mmu, &mmu->mmu_hw[i]);
		} else {
			writel(0xffffffff, mmu->mmu_hw[i].base +
				   REG_TLB_INVALIDATE);
		}
	}
	spin_unlock_irqrestore(&mmu->ready_lock, flags);
}

#ifdef DEBUG
static void page_table_dump(struct ipu_mmu_info *mmu_info)
{
	u32 l1_idx;

	pr_debug("begin IOMMU page table dump\n");

	for (l1_idx = 0; l1_idx < ISP_L1PT_PTES; l1_idx++) {
		u32 l2_idx;
		u32 iova = (phys_addr_t)l1_idx << ISP_L1PT_SHIFT;

		if (mmu_info->pgtbl[l1_idx] == mmu_info->dummy_l2_tbl)
			continue;
		pr_debug("l1 entry %u; iovas 0x%8.8x--0x%8.8x, at %p\n",
			 l1_idx, iova, iova + ISP_PAGE_SIZE,
			 (void *)TBL_PHYS_ADDR(mmu_info->pgtbl[l1_idx]));

		for (l2_idx = 0; l2_idx < ISP_L2PT_PTES; l2_idx++) {
			u32 *l2_pt = TBL_VIRT_ADDR(mmu_info->pgtbl[l1_idx]);
			u32 iova2 = iova + (l2_idx << ISP_L2PT_SHIFT);

			if (l2_pt[l2_idx] == mmu_info->dummy_page)
				continue;

			pr_debug("\tl2 entry %u; iova 0x%8.8x, phys %p\n",
				 l2_idx, iova2,
				 (void *)TBL_PHYS_ADDR(l2_pt[l2_idx]));
		}
	}

	pr_debug("end IOMMU page table dump\n");
}
#endif /* DEBUG */

static u32 *alloc_page_table(struct ipu_mmu_info *mmu_info, bool l1)
{
	u32 *pt = (u32 *)get_zeroed_page(GFP_ATOMIC | GFP_DMA32);
	int i;

	if (!pt)
		return NULL;

	pr_debug("get_zeroed_page() == %p\n", pt);

	for (i = 0; i < ISP_L1PT_PTES; i++)
		pt[i] = l1 ? mmu_info->dummy_l2_tbl : mmu_info->dummy_page;

	return pt;
}

static int l2_map(struct ipu_mmu_info *mmu_info, unsigned long iova,
		  phys_addr_t paddr, size_t size)
{
	u32 l1_idx = iova >> ISP_L1PT_SHIFT;
	u32 l1_entry = mmu_info->pgtbl[l1_idx];
	u32 *l2_pt;
	u32 iova_start = iova;
	unsigned int l2_idx;
	unsigned long flags;

	pr_debug("mapping l2 page table for l1 index %u (iova %8.8x)\n",
		 l1_idx, (u32)iova);

	spin_lock_irqsave(&mmu_info->lock, flags);
	if (l1_entry == mmu_info->dummy_l2_tbl) {
		u32 *l2_virt = alloc_page_table(mmu_info, false);

		if (!l2_virt) {
			spin_unlock_irqrestore(&mmu_info->lock, flags);
			return -ENOMEM;
		}

		l1_entry = virt_to_phys(l2_virt) >> ISP_PADDR_SHIFT;
		pr_debug("allocated page for l1_idx %u\n", l1_idx);

		if (mmu_info->pgtbl[l1_idx] == mmu_info->dummy_l2_tbl) {
			mmu_info->pgtbl[l1_idx] = l1_entry;
#ifdef CONFIG_X86
			clflush_cache_range(&mmu_info->pgtbl[l1_idx],
					    sizeof(mmu_info->pgtbl[l1_idx]));
#endif /* CONFIG_X86 */
		} else {
			free_page((unsigned long)TBL_VIRT_ADDR(l1_entry));
		}
	}

	l2_pt = TBL_VIRT_ADDR(mmu_info->pgtbl[l1_idx]);

	pr_debug("l2_pt at %p\n", l2_pt);

	paddr = ALIGN(paddr, ISP_PAGE_SIZE);

	l2_idx = (iova_start & ISP_L2PT_MASK) >> ISP_L2PT_SHIFT;

	pr_debug("l2_idx %u, phys 0x%8.8x\n", l2_idx, l2_pt[l2_idx]);
	if (l2_pt[l2_idx] != mmu_info->dummy_page) {
		spin_unlock_irqrestore(&mmu_info->lock, flags);
		return -EBUSY;
	}

	l2_pt[l2_idx] = paddr >> ISP_PADDR_SHIFT;

#ifdef CONFIG_X86
	clflush_cache_range(&l2_pt[l2_idx], sizeof(l2_pt[l2_idx]));
#endif /* CONFIG_X86 */
	spin_unlock_irqrestore(&mmu_info->lock, flags);

	pr_debug("l2 index %u mapped as 0x%8.8x\n", l2_idx, l2_pt[l2_idx]);

	return 0;
}

static int __ipu_mmu_map(struct ipu_mmu_info *mmu_info, unsigned long iova,
			 phys_addr_t paddr, size_t size)
{
	u32 iova_start = round_down(iova, ISP_PAGE_SIZE);
	u32 iova_end = ALIGN(iova + size, ISP_PAGE_SIZE);

	pr_debug
	    ("mapping iova 0x%8.8x--0x%8.8x, size %zu at paddr 0x%10.10llx\n",
	     iova_start, iova_end, size, paddr);

	return l2_map(mmu_info, iova_start, paddr, size);
}

static size_t l2_unmap(struct ipu_mmu_info *mmu_info, unsigned long iova,
		       phys_addr_t dummy, size_t size)
{
	u32 l1_idx = iova >> ISP_L1PT_SHIFT;
	u32 *l2_pt = TBL_VIRT_ADDR(mmu_info->pgtbl[l1_idx]);
	u32 iova_start = iova;
	unsigned int l2_idx;
	size_t unmapped = 0;

	pr_debug("unmapping l2 page table for l1 index %u (iova 0x%8.8lx)\n",
		 l1_idx, iova);

	if (mmu_info->pgtbl[l1_idx] == mmu_info->dummy_l2_tbl)
		return -EINVAL;

	pr_debug("l2_pt at %p\n", l2_pt);

	for (l2_idx = (iova_start & ISP_L2PT_MASK) >> ISP_L2PT_SHIFT;
	     (iova_start & ISP_L1PT_MASK) + (l2_idx << ISP_PAGE_SHIFT)
	     < iova_start + size && l2_idx < ISP_L2PT_PTES; l2_idx++) {
		unsigned long flags;

		pr_debug("l2 index %u unmapped, was 0x%10.10llx\n",
			 l2_idx, TBL_PHYS_ADDR(l2_pt[l2_idx]));
		spin_lock_irqsave(&mmu_info->lock, flags);
		l2_pt[l2_idx] = mmu_info->dummy_page;
		spin_unlock_irqrestore(&mmu_info->lock, flags);
#ifdef CONFIG_X86
		clflush_cache_range(&l2_pt[l2_idx], sizeof(l2_pt[l2_idx]));
#endif /* CONFIG_X86 */
		unmapped++;
	}

	return unmapped << ISP_PAGE_SHIFT;
}

static size_t __ipu_mmu_unmap(struct ipu_mmu_info *mmu_info,
			      unsigned long iova, size_t size)
{
	return l2_unmap(mmu_info, iova, 0, size);
}

static int allocate_trash_buffer(struct ipu_mmu *mmu)
{
	unsigned int n_pages = PAGE_ALIGN(IPU_MMUV2_TRASH_RANGE) >> PAGE_SHIFT;
	struct iova *iova;
	u32 iova_addr;
	unsigned int i;
	int ret;

	/* Allocate 8MB in iova range */
	iova = alloc_iova(&mmu->dmap->iovad, n_pages,
			  mmu->dmap->mmu_info->aperture_end >> PAGE_SHIFT, 0);
	if (!iova) {
		dev_err(mmu->dev, "cannot allocate iova range for trash\n");
		return -ENOMEM;
	}

	/*
	 * Map the 8MB iova address range to the same physical trash page
	 * mmu->trash_page which is already reserved at the probe
	 */
	iova_addr = iova->pfn_lo;
	for (i = 0; i < n_pages; i++) {
		ret = ipu_mmu_map(mmu->dmap->mmu_info, iova_addr << PAGE_SHIFT,
				  page_to_phys(mmu->trash_page), PAGE_SIZE);
		if (ret) {
			dev_err(mmu->dev,
				"mapping trash buffer range failed\n");
			goto out_unmap;
		}

		iova_addr++;
	}

	/* save the address for the ZLW invalidation */
	mmu->iova_addr_trash = iova->pfn_lo << PAGE_SHIFT;
	dev_dbg(mmu->dev, "iova trash buffer for MMUID: %d is %u\n",
		mmu->mmid, (unsigned int)mmu->iova_addr_trash);
	return 0;

out_unmap:
	ipu_mmu_unmap(mmu->dmap->mmu_info, iova->pfn_lo << PAGE_SHIFT,
		      (iova->pfn_hi - iova->pfn_lo + 1) << PAGE_SHIFT);
	__free_iova(&mmu->dmap->iovad, iova);
	return ret;
}

int ipu_mmu_hw_init(struct ipu_mmu *mmu)
{
	unsigned int i;
	unsigned long flags;
	struct ipu_mmu_info *mmu_info;

	dev_dbg(mmu->dev, "mmu hw init\n");

	mmu_info = mmu->dmap->mmu_info;

	/* Initialise the each MMU HW block */
	for (i = 0; i < mmu->nr_mmus; i++) {
		struct ipu_mmu_hw *mmu_hw = &mmu->mmu_hw[i];
		unsigned int j;
		u16 block_addr;

		/* Write page table address per MMU */
		writel((phys_addr_t)virt_to_phys(mmu_info->pgtbl)
			   >> ISP_PADDR_SHIFT,
			   mmu->mmu_hw[i].base + REG_L1_PHYS);

		/* Set info bits per MMU */
		writel(mmu->mmu_hw[i].info_bits,
		       mmu->mmu_hw[i].base + REG_INFO);

		/* Configure MMU TLB stream configuration for L1 */
		for (j = 0, block_addr = 0; j < mmu_hw->nr_l1streams;
		     block_addr += mmu->mmu_hw[i].l1_block_sz[j], j++) {
			if (block_addr > IPU_MAX_LI_BLOCK_ADDR) {
				dev_err(mmu->dev, "invalid L1 configuration\n");
				return -EINVAL;
			}

			/* Write block start address for each streams */
			writel(block_addr, mmu_hw->base +
				   mmu_hw->l1_stream_id_reg_offset + 4 * j);
		}

		/* Configure MMU TLB stream configuration for L2 */
		for (j = 0, block_addr = 0; j < mmu_hw->nr_l2streams;
		     block_addr += mmu->mmu_hw[i].l2_block_sz[j], j++) {
			if (block_addr > IPU_MAX_L2_BLOCK_ADDR) {
				dev_err(mmu->dev, "invalid L2 configuration\n");
				return -EINVAL;
			}

			writel(block_addr, mmu_hw->base +
				   mmu_hw->l2_stream_id_reg_offset + 4 * j);
		}
	}

	/*
	 * Allocate 1 page of physical memory for the trash buffer.
	 */
	if (!mmu->trash_page) {
		mmu->trash_page = alloc_page(GFP_KERNEL);
		if (!mmu->trash_page) {
			dev_err(mmu->dev, "insufficient memory for trash buffer\n");
			return -ENOMEM;
		}
	}

	/* Allocate trash buffer, if not allocated. Only once per MMU */
	if (!mmu->iova_addr_trash) {
		int ret;

		ret = allocate_trash_buffer(mmu);
		if (ret) {
			__free_page(mmu->trash_page);
			mmu->trash_page = NULL;
			dev_err(mmu->dev, "trash buffer allocation failed\n");
			return ret;
		}
	}

	spin_lock_irqsave(&mmu->ready_lock, flags);
	mmu->ready = true;
	spin_unlock_irqrestore(&mmu->ready_lock, flags);

	return 0;
}
EXPORT_SYMBOL(ipu_mmu_hw_init);

static struct ipu_mmu_info *ipu_mmu_alloc(struct ipu_device *isp)
{
	struct ipu_mmu_info *mmu_info;
	void *ptr;

	mmu_info = kzalloc(sizeof(*mmu_info), GFP_KERNEL);
	if (!mmu_info)
		return NULL;

	mmu_info->aperture_start = 0;
	mmu_info->aperture_end = DMA_BIT_MASK(isp->secure_mode ?
				      IPU_MMU_ADDRESS_BITS :
				      IPU_MMU_ADDRESS_BITS_NON_SECURE);
	mmu_info->pgsize_bitmap = SZ_4K;

	ptr = (void *)get_zeroed_page(GFP_KERNEL | GFP_DMA32);
	if (!ptr)
		goto err_mem;

	mmu_info->dummy_page = virt_to_phys(ptr) >> ISP_PAGE_SHIFT;

	ptr = alloc_page_table(mmu_info, false);
	if (!ptr)
		goto err;

	mmu_info->dummy_l2_tbl = virt_to_phys(ptr) >> ISP_PAGE_SHIFT;

	/*
	 * We always map the L1 page table (a single page as well as
	 * the L2 page tables).
	 */
	mmu_info->pgtbl = alloc_page_table(mmu_info, true);
	if (!mmu_info->pgtbl)
		goto err;

	spin_lock_init(&mmu_info->lock);

	pr_debug("domain initialised\n");

	return mmu_info;

err:
	free_page((unsigned long)TBL_VIRT_ADDR(mmu_info->dummy_page));
	free_page((unsigned long)TBL_VIRT_ADDR(mmu_info->dummy_l2_tbl));
err_mem:
	kfree(mmu_info);

	return NULL;
}

int ipu_mmu_hw_cleanup(struct ipu_mmu *mmu)
{
	unsigned long flags;

	spin_lock_irqsave(&mmu->ready_lock, flags);
	mmu->ready = false;
	spin_unlock_irqrestore(&mmu->ready_lock, flags);

	return 0;
}
EXPORT_SYMBOL(ipu_mmu_hw_cleanup);

static struct ipu_dma_mapping *alloc_dma_mapping(struct ipu_device *isp)
{
	struct ipu_dma_mapping *dmap;

	dmap = kzalloc(sizeof(*dmap), GFP_KERNEL);
	if (!dmap)
		return NULL;

	dmap->mmu_info = ipu_mmu_alloc(isp);
	if (!dmap->mmu_info) {
		kfree(dmap);
		return NULL;
	}
	init_iova_domain(&dmap->iovad, SZ_4K, 1);
	dmap->mmu_info->dmap = dmap;

	kref_init(&dmap->ref);

	pr_debug("alloc mapping\n");

	iova_cache_get();

	return dmap;
}

phys_addr_t ipu_mmu_iova_to_phys(struct ipu_mmu_info *mmu_info,
				 dma_addr_t iova)
{
	u32 *l2_pt = TBL_VIRT_ADDR(mmu_info->pgtbl[iova >> ISP_L1PT_SHIFT]);

	return (phys_addr_t)l2_pt[(iova & ISP_L2PT_MASK) >> ISP_L2PT_SHIFT]
	    << ISP_PAGE_SHIFT;
}

/*
 * The following four functions are implemented based on iommu.c
 * drivers/iommu/iommu.c/iommu_pgsize().
 */
static size_t ipu_mmu_pgsize(unsigned long pgsize_bitmap,
			     unsigned long addr_merge, size_t size)
{
	unsigned int pgsize_idx;
	size_t pgsize;

	/* Max page size that still fits into 'size' */
	pgsize_idx = __fls(size);

	/* need to consider alignment requirements ? */
	if (likely(addr_merge)) {
		/* Max page size allowed by address */
		unsigned int align_pgsize_idx = __ffs(addr_merge);

		pgsize_idx = min(pgsize_idx, align_pgsize_idx);
	}

	/* build a mask of acceptable page sizes */
	pgsize = (1UL << (pgsize_idx + 1)) - 1;

	/* throw away page sizes not supported by the hardware */
	pgsize &= pgsize_bitmap;

	/* make sure we're still sane */
	WARN_ON(!pgsize);

	/* pick the biggest page */
	pgsize_idx = __fls(pgsize);
	pgsize = 1UL << pgsize_idx;

	return pgsize;
}

/* drivers/iommu/iommu.c/iommu_unmap() */
size_t ipu_mmu_unmap(struct ipu_mmu_info *mmu_info, unsigned long iova,
		     size_t size)
{
	size_t unmapped_page, unmapped = 0;
	unsigned int min_pagesz;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(mmu_info->pgsize_bitmap);

	/*
	 * The virtual address, as well as the size of the mapping, must be
	 * aligned (at least) to the size of the smallest page supported
	 * by the hardware
	 */
	if (!IS_ALIGNED(iova | size, min_pagesz)) {
		dev_err(NULL, "unaligned: iova 0x%lx size 0x%zx min_pagesz 0x%x\n",
			iova, size, min_pagesz);
		return -EINVAL;
	}

	/*
	 * Keep iterating until we either unmap 'size' bytes (or more)
	 * or we hit an area that isn't mapped.
	 */
	while (unmapped < size) {
		size_t pgsize = ipu_mmu_pgsize(mmu_info->pgsize_bitmap,
						iova, size - unmapped);

		unmapped_page = __ipu_mmu_unmap(mmu_info, iova, pgsize);
		if (!unmapped_page)
			break;

		dev_dbg(NULL, "unmapped: iova 0x%lx size 0x%zx\n",
			iova, unmapped_page);

		iova += unmapped_page;
		unmapped += unmapped_page;
	}

	return unmapped;
}

/* drivers/iommu/iommu.c/iommu_map() */
int ipu_mmu_map(struct ipu_mmu_info *mmu_info, unsigned long iova,
		phys_addr_t paddr, size_t size)
{
	unsigned long orig_iova = iova;
	unsigned int min_pagesz;
	size_t orig_size = size;
	int ret = 0;

	if (mmu_info->pgsize_bitmap == 0UL)
		return -ENODEV;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(mmu_info->pgsize_bitmap);

	/*
	 * both the virtual address and the physical one, as well as
	 * the size of the mapping, must be aligned (at least) to the
	 * size of the smallest page supported by the hardware
	 */
	if (!IS_ALIGNED(iova | paddr | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx pa %pa size 0x%zx min_pagesz 0x%x\n",
		       iova, &paddr, size, min_pagesz);
		return -EINVAL;
	}

	pr_debug("map: iova 0x%lx pa %pa size 0x%zx\n", iova, &paddr, size);

	while (size) {
		size_t pgsize = ipu_mmu_pgsize(mmu_info->pgsize_bitmap,
					       iova | paddr, size);

		pr_debug("mapping: iova 0x%lx pa %pa pgsize 0x%zx\n",
			 iova, &paddr, pgsize);

		ret = __ipu_mmu_map(mmu_info, iova, paddr, pgsize);
		if (ret)
			break;

		iova += pgsize;
		paddr += pgsize;
		size -= pgsize;
	}

	/* unroll mapping in case something went wrong */
	if (ret)
		ipu_mmu_unmap(mmu_info, orig_iova, orig_size - size);

	return ret;
}

static void ipu_mmu_destroy(struct ipu_mmu *mmu)
{
	struct ipu_dma_mapping *dmap = mmu->dmap;
	struct ipu_mmu_info *mmu_info = dmap->mmu_info;
	struct iova *iova;
	u32 l1_idx;

	if (mmu->iova_addr_trash) {
		iova = find_iova(&dmap->iovad,
				 mmu->iova_addr_trash >> PAGE_SHIFT);
		if (iova) {
			/* unmap and free the trash buffer iova */
			ipu_mmu_unmap(mmu_info, iova->pfn_lo << PAGE_SHIFT,
				      (iova->pfn_hi - iova->pfn_lo + 1) <<
				      PAGE_SHIFT);
			__free_iova(&dmap->iovad, iova);
		} else {
			dev_err(mmu->dev, "trash buffer iova not found.\n");
		}

		mmu->iova_addr_trash = 0;
	}

	if (mmu->trash_page)
		__free_page(mmu->trash_page);

	for (l1_idx = 0; l1_idx < ISP_L1PT_PTES; l1_idx++)
		if (mmu_info->pgtbl[l1_idx] != mmu_info->dummy_l2_tbl)
			free_page((unsigned long)
				  TBL_VIRT_ADDR(mmu_info->pgtbl[l1_idx]));

	free_page((unsigned long)TBL_VIRT_ADDR(mmu_info->dummy_page));
	free_page((unsigned long)TBL_VIRT_ADDR(mmu_info->dummy_l2_tbl));
	free_page((unsigned long)mmu_info->pgtbl);
	kfree(mmu_info);
}

struct ipu_mmu *ipu_mmu_init(struct device *dev,
			     void __iomem *base, int mmid,
			     const struct ipu_hw_variants *hw)
{
	struct ipu_mmu *mmu;
	struct ipu_mmu_pdata *pdata;
	struct ipu_device *isp = pci_get_drvdata(to_pci_dev(dev));
	unsigned int i;

	if (hw->nr_mmus > IPU_MMU_MAX_DEVICES)
		return ERR_PTR(-EINVAL);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < hw->nr_mmus; i++) {
		struct ipu_mmu_hw *pdata_mmu = &pdata->mmu_hw[i];
		const struct ipu_mmu_hw *src_mmu = &hw->mmu_hw[i];

		if (src_mmu->nr_l1streams > IPU_MMU_MAX_TLB_L1_STREAMS ||
		    src_mmu->nr_l2streams > IPU_MMU_MAX_TLB_L2_STREAMS)
			return ERR_PTR(-EINVAL);

		*pdata_mmu = *src_mmu;
		pdata_mmu->base = base + src_mmu->offset;
	}

	mmu = devm_kzalloc(dev, sizeof(*mmu), GFP_KERNEL);
	if (!mmu)
		return ERR_PTR(-ENOMEM);

	mmu->mmid = mmid;
	mmu->mmu_hw = pdata->mmu_hw;
	mmu->nr_mmus = hw->nr_mmus;
	mmu->tlb_invalidate = tlb_invalidate;
	mmu->ready = false;
	INIT_LIST_HEAD(&mmu->vma_list);
	spin_lock_init(&mmu->ready_lock);

	mmu->dmap = alloc_dma_mapping(isp);
	if (!mmu->dmap) {
		dev_err(dev, "can't alloc dma mapping\n");
		return ERR_PTR(-ENOMEM);
	}

	return mmu;
}
EXPORT_SYMBOL(ipu_mmu_init);

void ipu_mmu_cleanup(struct ipu_mmu *mmu)
{
	struct ipu_dma_mapping *dmap = mmu->dmap;

	ipu_mmu_destroy(mmu);
	mmu->dmap = NULL;
	iova_cache_put();
	put_iova_domain(&dmap->iovad);
	kfree(dmap);
}
EXPORT_SYMBOL(ipu_mmu_cleanup);

MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu mmu driver");
