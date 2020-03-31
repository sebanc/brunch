/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _LINUX_KSTALED_H
#define _LINUX_KSTALED_H

#include <linux/types.h>

struct page;
struct page_vma_mapped_walk;
struct pglist_data;
struct mm_struct;
struct zone;

#ifdef CONFIG_KSTALED

/*
 * Age ranges from zero to KSTALED_MAX_AGE. Zero means it's never set
 * (e.g., a page never used). A cyclic value between one and the max
 * points to a bucket on the ring described below. Age is set when
 * page is first added to a bucket, updated when accessed, and cleared
 * when deleted from the ring (i.e., not in any buckets).
 *
 * After the age of a page is updated, the page is moved into a new
 * bucket by reclaim when it scans the ring. The head is the age of
 * the hottest pages; the tail is the age of the coldest pages. If the
 * ring isn't in use, then the head is zero; and the tail must be zero
 * too in this case.
 *
 * The ring shares the same data structure, i.e., doubly linked list,
 * with inactive LRU list (kstaled doesn't use active LRU list). The
 * head is incremented after each mm walk has filled a bucket. The
 * tail is incremented before reclaim starts draining a bucket.
 *
 * Pages that are added or accessed after the second to the last mm
 * walk are hot, and the rest are cold. In other words, hot pages are
 * from the last filled bucket and the bucket that is currently being
 * filled. This means cold pages are guaranteed to be at least as old
 * as the interval between the last two mm walks, which is also the
 * time to fill the last bucket.
 *
 * When there aren't any used buckets, i.e, kstaled is not running, we
 * call the ring empty. When there is only one filled bucket, i.e., the
 * tail is right behind the head, we call it low. There are no cold
 * pages in this case, and reclaim stalls. When there aren't any empty
 * buckets, i.e., the head is right behind the tail, we call it full.
 * And mm walk stalls in this case.
 */
#define KSTALED_AGE_WIDTH	4
#define KSTALED_LRU_TYPES	2
#define KSTALED_MAX_AGE		GENMASK(KSTALED_AGE_WIDTH - 1, 0)
#define KSTALED_AGE_PGOFF	(LAST_CPUPID_PGOFF - KSTALED_AGE_WIDTH)
#define KSTALED_AGE_MASK	(KSTALED_MAX_AGE << KSTALED_AGE_PGOFF)

bool kstaled_is_enabled(void);
bool kstaled_ring_inuse(struct pglist_data *node);
void kstaled_add_mm(struct mm_struct *mm);
void kstaled_del_mm(struct mm_struct *mm);
unsigned kstaled_get_age(struct page *page);
void kstaled_set_age(struct page *page);
void kstaled_clear_age(struct page *page);
void kstaled_update_age(struct page *page);
bool kstaled_direct_aging(struct page_vma_mapped_walk *pvmw);
void kstaled_enable_throttle(void);
void kstaled_disable_throttle(void);
bool kstaled_throttle_alloc(struct zone *zone, int order, gfp_t gfp_mask);
bool kstaled_direct_reclaim(struct zone *zone, int order, gfp_t gfp_mask);
struct page *kstaled_get_page_ref(struct page *page);
void kstaled_put_page_ref(struct page *page);

#else /* CONFIG_KSTALED */

#define KSTALED_AGE_MASK	0

static inline bool kstaled_is_enabled(void)
{
	return false;
}

static inline bool kstaled_ring_inuse(struct pglist_data *node)
{
	return false;
}

static inline void kstaled_add_mm(struct mm_struct *mm)
{
}

static inline void kstaled_del_mm(struct mm_struct *mm)
{
}

static inline unsigned kstaled_get_age(struct page *page)
{
	return 0;
}

static inline void kstaled_set_age(struct page *page)
{
}

static inline void kstaled_clear_age(struct page *page)
{
}

static inline void kstaled_update_age(struct page *page)
{
}

static inline bool kstaled_direct_aging(struct page_vma_mapped_walk *pvmw)
{
	return false;
}

static inline void kstaled_enable_throttle(void)
{
}

static inline void kstaled_disable_throttle(void)
{
}

static inline bool kstaled_throttle_alloc(struct zone *zone, int order,
					  gfp_t gfp_mask)
{
	return true;
}

static inline bool kstaled_direct_reclaim(struct zone *zone, int order,
					  gfp_t gfp_mask)
{
	return false;
}

#endif /* CONFIG_KSTALED */
#endif /* _LINUX_KSTALED_H */
