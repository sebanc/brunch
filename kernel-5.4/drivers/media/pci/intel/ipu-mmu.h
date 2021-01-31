/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2020 Intel Corporation */

#ifndef IPU_MMU_H
#define IPU_MMU_H

#include <linux/dma-mapping.h>

#include "ipu.h"
#include "ipu-pdata.h"

#define ISYS_MMID 1
#define PSYS_MMID 0

/*
 * @pgtbl: virtual address of the l1 page table (one page)
 */
struct ipu_mmu_info {
	u32 __iomem *pgtbl;
	dma_addr_t aperture_start;
	dma_addr_t aperture_end;
	unsigned long pgsize_bitmap;

	spinlock_t lock;	/* Serialize access to users */
	struct ipu_dma_mapping *dmap;
	u32 dummy_l2_tbl;
	u32 dummy_page;
};

/*
 * @pgtbl: physical address of the l1 page table
 */
struct ipu_mmu {
	struct list_head node;

	struct ipu_mmu_hw *mmu_hw;
	unsigned int nr_mmus;
	int mmid;

	phys_addr_t pgtbl;
	struct device *dev;

	struct ipu_dma_mapping *dmap;
	struct list_head vma_list;

	struct page *trash_page;
	dma_addr_t iova_addr_trash;

	bool ready;
	spinlock_t ready_lock;	/* Serialize access to bool ready */

	void (*tlb_invalidate)(struct ipu_mmu *mmu);
};

struct ipu_mmu *ipu_mmu_init(struct device *dev,
			     void __iomem *base, int mmid,
			     const struct ipu_hw_variants *hw);
void ipu_mmu_cleanup(struct ipu_mmu *mmu);
int ipu_mmu_hw_init(struct ipu_mmu *mmu);
int ipu_mmu_hw_cleanup(struct ipu_mmu *mmu);
int ipu_mmu_map(struct ipu_mmu_info *mmu_info, unsigned long iova,
		phys_addr_t paddr, size_t size);
size_t ipu_mmu_unmap(struct ipu_mmu_info *mmu_info, unsigned long iova,
		     size_t size);
phys_addr_t ipu_mmu_iova_to_phys(struct ipu_mmu_info *mmu_info,
				 dma_addr_t iova);
#endif
