/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __COIOMMU_H
#define __COIOMMU_H

#include <linux/rwlock.h>
#include <linux/kthread.h>
#include <linux/shrinker.h>

#define COIOMMU_INFO_NR_OBJS 64

struct dtt_page_cache {
	int nobjs;
	void *objects[COIOMMU_INFO_NR_OBJS];
};

struct coiommu_dtt {
	void *root;
	unsigned int level;
	int max_map_count;
	rwlock_t lock;
	void *zero_page;
	struct shrinker dtt_shrinker;
	spinlock_t alloc_lock;
	int cur_cache;
	struct kthread_worker *worker;
	struct kthread_work alloc_work;
	atomic_t pages;
	struct dtt_page_cache cache[2];
};

struct coiommu {
	const struct coiommu_dev_ops *dev_ops;
	void *dev;
	unsigned short *endpoints;
	int ep_count;
	struct coiommu_dtt dtt;
};

#define dtt_to_coiommu(v) container_of(v, struct coiommu, dtt)

#define COIOMMU_UPPER_LEVEL_STRIDE	9
#define COIOMMU_UPPER_LEVEL_MASK	(((u64)1 << COIOMMU_UPPER_LEVEL_STRIDE) - 1)
#define COIOMMU_PT_LEVEL_STRIDE		10
#define COIOMMU_PT_LEVEL_MASK		(((u64)1 << COIOMMU_PT_LEVEL_STRIDE) - 1)

#define DTTE_MAP_CNT_MASK	0xFFFF
#define DTTE_PINNED_FLAG	31
#define DTTE_ACCESSED_FLAG	30

struct dtt_leaf_entry {
	atomic_t dtte;
};

struct dtt_parent_entry {
	u64 val;
};

#define DTT_PRESENT	1
#define DTT_LAST_LEVEL	1

static inline bool parent_pte_present(struct dtt_parent_entry *pte)
{
	return (pte->val & DTT_PRESENT) != 0;
}

static inline u64 parent_pte_value(void *vaddr)
{
	return virt_to_phys(vaddr) | DTT_PRESENT;
}

static inline u64 parent_pte_addr(struct dtt_parent_entry *pte)
{
	return pte->val & PAGE_MASK;
}

#define coiommu_set_flag(flag, dtte) \
	atomic_or(flag, dtte)

#define coiommu_clear_flag(flag, dtte) \
	atomic_and(~(flag), dtte)

#define coiommu_test_flag(flag, dtte) \
	(atomic_read(dtte) & flag)

#endif /* __COIOMMU_H */
