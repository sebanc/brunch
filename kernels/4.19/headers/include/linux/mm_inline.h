/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

#include <linux/huge_mm.h>
#include <linux/swap.h>

/**
 * page_is_file_cache - should the page be on a file LRU or anon LRU?
 * @page: the page to test
 *
 * Returns 1 if @page is page cache page backed by a regular filesystem,
 * or 0 if @page is anonymous, tmpfs or otherwise ram or swap backed.
 * Used by functions that manipulate the LRU lists, to sort a page
 * onto the right LRU list.
 *
 * We would like to get this info without a page flag, but the state
 * needs to survive until the page is last deleted from the LRU, which
 * could be as far down as __page_cache_release.
 */
static inline int page_is_file_cache(struct page *page)
{
	return !PageSwapBacked(page);
}

static __always_inline void update_lru_size(struct lruvec *lruvec,
				enum lru_list lru, enum zone_type zid,
				int nr_pages)
{
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	__mod_node_page_state(pgdat, NR_LRU_BASE + lru, nr_pages);
	__mod_zone_page_state(&pgdat->node_zones[zid],
				NR_ZONE_LRU_BASE + lru, nr_pages);
#ifdef CONFIG_MEMCG
	mem_cgroup_update_lru_size(lruvec, lru, zid, nr_pages);
#endif
}

/**
 * __clear_page_lru_flags - clear page lru flags before releasing a page
 * @page: the page that was on lru and now has a zero reference
 */
static __always_inline void __clear_page_lru_flags(struct page *page)
{
	VM_BUG_ON_PAGE(!PageLRU(page), page);

	__ClearPageLRU(page);

	/* this shouldn't happen, so leave the flags to bad_page() */
	if (PageActive(page) && PageUnevictable(page))
		return;

	__ClearPageActive(page);
	__ClearPageUnevictable(page);
}

/**
 * page_lru - which LRU list should a page be on?
 * @page: the page to test
 *
 * Returns the LRU list a page should be on, as an index
 * into the array of LRU lists.
 */
static __always_inline enum lru_list page_lru(struct page *page)
{
	enum lru_list lru;

	VM_BUG_ON_PAGE(PageActive(page) && PageUnevictable(page), page);

	if (PageUnevictable(page))
		return LRU_UNEVICTABLE;

	lru = page_is_file_cache(page) ? LRU_INACTIVE_FILE : LRU_INACTIVE_ANON;
	if (PageActive(page))
		lru += LRU_ACTIVE;

	return lru;
}

#define lru_to_page(head) (list_entry((head)->prev, struct page, lru))

#ifdef CONFIG_LRU_GEN

#ifdef CONFIG_LRU_GEN_ENABLED
DECLARE_STATIC_KEY_TRUE(lru_gen_static_key);

static inline bool lru_gen_enabled(void)
{
	return static_branch_likely(&lru_gen_static_key);
}
#else
DECLARE_STATIC_KEY_FALSE(lru_gen_static_key);

static inline bool lru_gen_enabled(void)
{
	return static_branch_unlikely(&lru_gen_static_key);
}
#endif

/* We track at most MAX_NR_GENS generations using the sliding window technique. */
static inline int lru_gen_from_seq(unsigned long seq)
{
	return seq % MAX_NR_GENS;
}

/* Convert the level of usage to a tier. See the comment on MAX_NR_TIERS. */
static inline int lru_tier_from_usage(int usage)
{
	return order_base_2(usage + 1);
}

/* Return a proper index regardless whether we keep a full history of stats. */
static inline int hist_from_seq_or_gen(int seq_or_gen)
{
	return seq_or_gen % NR_STAT_GENS;
}

/* The youngest and the second youngest generations are counted as active. */
static inline bool lru_gen_is_active(struct lruvec *lruvec, int gen)
{
	unsigned long max_seq = READ_ONCE(lruvec->evictable.max_seq);

	VM_BUG_ON(!max_seq);
	VM_BUG_ON(gen >= MAX_NR_GENS);

	return gen == lru_gen_from_seq(max_seq) || gen == lru_gen_from_seq(max_seq - 1);
}

/* Update the sizes of the multigenerational lru lists. */
static inline void lru_gen_update_size(struct page *page, struct lruvec *lruvec,
				       int old_gen, int new_gen)
{
	int type = page_is_file_cache(page);
	int zone = page_zonenum(page);
	int delta = hpage_nr_pages(page);
	enum lru_list lru = type * LRU_FILE;
	struct lrugen *lrugen = &lruvec->evictable;

	lockdep_assert_held(&lruvec_pgdat(lruvec)->lru_lock);
	VM_BUG_ON(old_gen != -1 && old_gen >= MAX_NR_GENS);
	VM_BUG_ON(new_gen != -1 && new_gen >= MAX_NR_GENS);
	VM_BUG_ON(old_gen == -1 && new_gen == -1);

	if (old_gen >= 0)
		WRITE_ONCE(lrugen->sizes[old_gen][type][zone],
			   lrugen->sizes[old_gen][type][zone] - delta);
	if (new_gen >= 0)
		WRITE_ONCE(lrugen->sizes[new_gen][type][zone],
			   lrugen->sizes[new_gen][type][zone] + delta);

	if (old_gen < 0) {
		if (lru_gen_is_active(lruvec, new_gen))
			lru += LRU_ACTIVE;
		update_lru_size(lruvec, lru, zone, delta);
		return;
	}

	if (new_gen < 0) {
		if (lru_gen_is_active(lruvec, old_gen))
			lru += LRU_ACTIVE;
		update_lru_size(lruvec, lru, zone, -delta);
		return;
	}

	if (!lru_gen_is_active(lruvec, old_gen) && lru_gen_is_active(lruvec, new_gen)) {
		update_lru_size(lruvec, lru, zone, -delta);
		update_lru_size(lruvec, lru + LRU_ACTIVE, zone, delta);
	}

	VM_BUG_ON(lru_gen_is_active(lruvec, old_gen) && !lru_gen_is_active(lruvec, new_gen));
}

/* Add a page to one of the multigenerational lru lists. Return true on success. */
static inline bool lru_gen_addition(struct page *page, struct lruvec *lruvec, bool front)
{
	int gen;
	unsigned long old_flags, new_flags;
	int type = page_is_file_cache(page);
	int zone = page_zonenum(page);
	struct lrugen *lrugen = &lruvec->evictable;

	if (PageUnevictable(page) || !lrugen->enabled[type])
		return false;
	/*
	 * If a page is being faulted in, add it to the youngest generation.
	 * try_walk_mm_list() may look at the size of the youngest generation to
	 * determine if the aging is due.
	 *
	 * If a page can't be evicted immediately, i.e., an anon page not in
	 * swap cache, a dirty file page under reclaim, or a page rejected by
	 * evict_pages() due to races, dirty buffer heads, etc., add it to the
	 * second oldest generation.
	 *
	 * If a page could be evicted immediately, i.e., a clean file page, add
	 * it to the oldest generation.
	 */
	if (PageActive(page))
		gen = lru_gen_from_seq(lrugen->max_seq);
	else if ((!type && !PageSwapCache(page)) ||
		 (PageReclaim(page) && (PageDirty(page) || PageWriteback(page))) ||
		 (!PageReferenced(page) && PageWorkingset(page)))
		gen = lru_gen_from_seq(lrugen->min_seq[type] + 1);
	else
		gen = lru_gen_from_seq(lrugen->min_seq[type]);

	do {
		old_flags = READ_ONCE(page->flags);
		VM_BUG_ON_PAGE(old_flags & LRU_GEN_MASK, page);

		new_flags = (old_flags & ~(LRU_GEN_MASK | BIT(PG_active))) |
			    ((gen + 1UL) << LRU_GEN_PGOFF);
		/* see the comment in evict_pages() */
		if (!(old_flags & BIT(PG_referenced)))
			new_flags &= ~(LRU_USAGE_MASK | LRU_TIER_FLAGS);
	} while (cmpxchg(&page->flags, old_flags, new_flags) != old_flags);

	lru_gen_update_size(page, lruvec, -1, gen);
	if (front)
		list_add(&page->lru, &lrugen->lists[gen][type][zone]);
	else
		list_add_tail(&page->lru, &lrugen->lists[gen][type][zone]);

	return true;
}

/* Delete a page from one of the multigenerational lru lists. Return true on success. */
static inline bool lru_gen_deletion(struct page *page, struct lruvec *lruvec)
{
	int gen;
	unsigned long old_flags, new_flags;

	do {
		old_flags = READ_ONCE(page->flags);
		if (!(old_flags & LRU_GEN_MASK))
			return false;

		VM_BUG_ON_PAGE(PageActive(page), page);
		VM_BUG_ON_PAGE(PageUnevictable(page), page);

		gen = ((old_flags & LRU_GEN_MASK) >> LRU_GEN_PGOFF) - 1;

		new_flags = old_flags & ~LRU_GEN_MASK;
		/* mark page active accordingly */
		if (lru_gen_is_active(lruvec, gen))
			new_flags |= BIT(PG_active);
	} while (cmpxchg(&page->flags, old_flags, new_flags) != old_flags);

	lru_gen_update_size(page, lruvec, gen, -1);
	list_del(&page->lru);

	return true;
}

/* Return the level of usage of a page. See the comment on MAX_NR_TIERS. */
static inline int page_tier_usage(struct page *page)
{
	unsigned long flags = READ_ONCE(page->flags);

	return flags & BIT(PG_workingset) ?
	       ((flags & LRU_USAGE_MASK) >> LRU_USAGE_PGOFF) + 1 : 0;
}

/* Increment the usage counter after a page is accessed via file descriptors. */
static inline void page_inc_usage(struct page *page)
{
	unsigned long usage;
	unsigned long old_flags, new_flags;

	do {
		old_flags = READ_ONCE(page->flags);

		if (!(old_flags & BIT(PG_workingset))) {
			new_flags = old_flags | BIT(PG_workingset);
			continue;
		}

		usage = (old_flags & LRU_USAGE_MASK) + BIT(LRU_USAGE_PGOFF);

		new_flags = (old_flags & ~LRU_USAGE_MASK) | min(usage, LRU_USAGE_MASK);
	} while (new_flags != old_flags &&
		 cmpxchg(&page->flags, old_flags, new_flags) != old_flags);
}

#else /* CONFIG_LRU_GEN */

static inline bool lru_gen_enabled(void)
{
	return false;
}

static inline bool lru_gen_addition(struct page *page, struct lruvec *lruvec, bool front)
{
	return false;
}

static inline bool lru_gen_deletion(struct page *page, struct lruvec *lruvec)
{
	return false;
}

static inline void page_inc_usage(struct page *page)
{
}

#endif /* CONFIG_LRU_GEN */

static __always_inline void add_page_to_lru_list(struct page *page,
				struct lruvec *lruvec)
{
	enum lru_list lru = page_lru(page);

	if (lru_gen_addition(page, lruvec, true))
		return;

	update_lru_size(lruvec, lru, page_zonenum(page), hpage_nr_pages(page));
	list_add(&page->lru, &lruvec->lists[lru]);
}

static __always_inline void add_page_to_lru_list_tail(struct page *page,
				struct lruvec *lruvec)
{
	enum lru_list lru = page_lru(page);

	if (lru_gen_addition(page, lruvec, false))
		return;

	update_lru_size(lruvec, lru, page_zonenum(page), hpage_nr_pages(page));
	list_add_tail(&page->lru, &lruvec->lists[lru]);
}

static __always_inline void del_page_from_lru_list(struct page *page,
				struct lruvec *lruvec)
{
	if (lru_gen_deletion(page, lruvec))
		return;

	list_del(&page->lru);
	update_lru_size(lruvec, page_lru(page), page_zonenum(page),
			-hpage_nr_pages(page));
}
#endif
