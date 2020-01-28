// SPDX-License-Identifier: GPL-2.0+

#include <linux/backing-dev.h>
#include <linux/ctype.h>
#include <linux/freezer.h>
#include <linux/hugetlb.h>
#include <linux/kstaled.h>
#include <linux/kthread.h>
#include <linux/mm_inline.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/swap.h>
#include <linux/compaction.h>

#include "internal.h"

#include <trace/events/vmscan.h>

/* ref counts from page table owner and kstaled */
#define KSTALED_MAX_REF		2
#define KSTALED_REF_MASK	GENMASK(PAGE_TYPE_NEXT + ilog2(KSTALED_MAX_REF), PAGE_TYPE_NEXT)

#define kstaled_lru(file)	((file) ? LRU_INACTIVE_FILE : LRU_INACTIVE_ANON)
#define kstaled_flags(file)	((file) ? BIT(PG_lru) : BIT(PG_lru) | BIT(PG_swapbacked))
#define kstaled_node(kstaled)	container_of(kstaled, struct pglist_data, kstaled)
#define kstaled_of_page(page)	(&page_pgdat(page)->kstaled)
#define kstaled_of_zone(zone)	(&(zone)->zone_pgdat->kstaled)

/*
 * Controls how proactively kstaled works. Larger values make kstaled
 * more proactive but also consume more CPU. Zero means disabled.
 */
static unsigned kstaled_ratio __read_mostly;
static DECLARE_WAIT_QUEUE_HEAD(kstaled_wait);

unsigned long node_shrink_list(struct pglist_data *node, struct list_head *list,
			       unsigned long isolated, bool file, gfp_t gfp_mask);
unsigned long node_shrink_slab(struct pglist_data *node, unsigned long scanned,
			       unsigned long total, gfp_t gfp_mask);

static inline unsigned kstaled_next_age(unsigned long age)
{
	BUILD_BUG_ON(KSTALED_AGE_WIDTH < 2);
	BUILD_BUG_ON(KSTALED_AGE_PGOFF < NR_PAGEFLAGS);

	VM_BUG_ON(age > KSTALED_MAX_AGE);

	return age < KSTALED_MAX_AGE ? age + 1 : 1;
}

static inline unsigned kstaled_xchg_age(struct page *page, unsigned long age)
{
	unsigned long old, new;

	VM_BUG_ON(age > KSTALED_MAX_AGE);

	do {
		old = READ_ONCE(page->flags);
		new = (old & ~KSTALED_AGE_MASK) | (age << KSTALED_AGE_PGOFF);
		if (old == new)
			return age;
	} while (cmpxchg(&page->flags, old, new) != old);

	return (old & KSTALED_AGE_MASK) >> KSTALED_AGE_PGOFF;
}

static inline void kstaled_init_ring(struct kstaled_struct *kstaled)
{
	int i, j;
	struct pglist_data *node = kstaled_node(kstaled);

	VM_BUG_ON(kstaled->head);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		VM_BUG_ON(kstaled->tail[i]);

		for (j = 0; j < KSTALED_MAX_AGE; j++) {
			struct page *bucket = &kstaled->ring[i][j];

			VM_BUG_ON_PAGE(page_count(bucket), bucket);

			page_mapcount_reset(bucket);
			bucket->flags = kstaled_flags(i);
			INIT_LIST_HEAD(&bucket->lru);
		}
	}

	trace_kstaled_ring(node->node_id, "init",
			   kstaled->head, kstaled->tail[0], kstaled->tail[1]);
}

static inline void kstaled_start_ring(struct kstaled_struct *kstaled)
{
	int i, j;
	struct pglist_data *node = kstaled_node(kstaled);

	VM_BUG_ON(kstaled->head);
	lockdep_assert_held(&node->lru_lock);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		VM_BUG_ON(kstaled->tail[i]);

		for (j = 0; j < KSTALED_MAX_AGE; j++) {
			struct page *bucket = &kstaled->ring[i][j];

			VM_BUG_ON_PAGE(page_count(bucket), bucket);
			VM_BUG_ON_PAGE(page_mapped(bucket), bucket);
			VM_BUG_ON_PAGE(bucket->flags != kstaled_flags(i), bucket);
			VM_BUG_ON_PAGE(!list_empty(&bucket->lru), bucket);
		}

		kstaled->tail[i] = KSTALED_MAX_AGE;
	}

	trace_kstaled_ring(node->node_id, "start",
			   kstaled->head, kstaled->tail[0], kstaled->tail[1]);
}

static inline void kstaled_stop_ring(struct kstaled_struct *kstaled)
{
	int i, j;
	struct pglist_data *node = kstaled_node(kstaled);

	VM_BUG_ON(kstaled->head > KSTALED_MAX_AGE);

	spin_lock_irq(&node->lru_lock);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		VM_BUG_ON(kstaled->tail[i] > KSTALED_MAX_AGE);

		for (j = 0; j < KSTALED_MAX_AGE; j++) {
			struct page *bucket = &kstaled->ring[i][j];

			VM_BUG_ON_PAGE(page_count(bucket), bucket);
			VM_BUG_ON_PAGE(page_mapped(bucket), bucket);
			VM_BUG_ON_PAGE(bucket->flags != kstaled_flags(i), bucket);
			VM_BUG_ON_PAGE(list_empty(&bucket->lru) ==
				       ((kstaled->head > kstaled->tail[i] &&
					 (j + 1 <= kstaled->head &&
					  j + 1 > kstaled->tail[i])) ||
					(kstaled->head < kstaled->tail[i] &&
					 (j + 1 <= kstaled->head ||
					  j + 1 > kstaled->tail[i]))), bucket);

			list_del_init(&bucket->lru);
		}

		kstaled->tail[i] = 0;
	}

	kstaled->head = 0;
	kstaled->peeked = false;

	trace_kstaled_ring(node->node_id, "stop",
			   kstaled->head, kstaled->tail[0], kstaled->tail[1]);

	spin_unlock_irq(&node->lru_lock);
}

static inline bool kstaled_ring_full(struct kstaled_struct *kstaled)
{
	int i;

	VM_BUG_ON(!kstaled->head);
	VM_BUG_ON(kstaled->head > KSTALED_MAX_AGE);
	lockdep_assert_held(&kstaled_node(kstaled)->lru_lock);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		VM_BUG_ON(!kstaled->tail[i]);
		VM_BUG_ON(kstaled->tail[i] > KSTALED_MAX_AGE);
		VM_BUG_ON(kstaled->head == kstaled->tail[i]);

		if (kstaled_next_age(kstaled->head) == kstaled->tail[i])
			return true;
	}

	return false;
}

static inline bool kstaled_ring_empty(struct kstaled_struct *kstaled)
{
	int i;

	VM_BUG_ON(kstaled->head > KSTALED_MAX_AGE);
	VM_BUG_ON(!current_is_kswapd() &&
		  !lockdep_is_held_type(&kstaled_node(kstaled)->lru_lock, -1));

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		VM_BUG_ON(kstaled->tail[i] > KSTALED_MAX_AGE);
		VM_BUG_ON(kstaled->head && !kstaled->tail[i]);
		VM_BUG_ON(!kstaled->head && kstaled->tail[i]);
		VM_BUG_ON(kstaled->head && kstaled->head == kstaled->tail[i]);
	}

	return !kstaled->head;
}

static inline bool kstaled_ring_low(struct kstaled_struct *kstaled, bool file)
{
	unsigned tail = READ_ONCE(kstaled->tail[file]);

	if (kstaled_ring_empty(kstaled))
		return true;

	return kstaled_next_age(tail) == kstaled->head;
}

static inline unsigned kstaled_ring_span(struct kstaled_struct *kstaled, bool file)
{
	unsigned head = kstaled->head;
	unsigned tail = READ_ONCE(kstaled->tail[file]);

	if (kstaled_ring_empty(kstaled))
		return 0;

	return tail < head ? head - tail : head + KSTALED_MAX_AGE - tail;
}

static inline void kstaled_advance_head(struct kstaled_struct *kstaled)
{
	int i;
	unsigned head;
	struct pglist_data *node = kstaled_node(kstaled);
	struct lruvec *lruvec = node_lruvec(node);

	spin_lock_irq(&node->lru_lock);

	if (kstaled_ring_empty(kstaled))
		kstaled_start_ring(kstaled);
	else if (kstaled_ring_full(kstaled)) {
		__inc_node_state(node, KSTALED_AGING_STALLS);
		goto unlock;
	}

	head = kstaled_next_age(kstaled->head);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		struct page *bucket = &kstaled->ring[i][head - 1];

		VM_BUG_ON_PAGE(page_count(bucket), bucket);
		VM_BUG_ON_PAGE(page_mapped(bucket), bucket);
		VM_BUG_ON_PAGE(bucket->flags != kstaled_flags(i), bucket);
		VM_BUG_ON_PAGE(!list_empty(&bucket->lru), bucket);

		list_add(&bucket->lru, lruvec->lists + kstaled_lru(i));
	}

	kstaled->head = head;
	kstaled->peeked = false;

	trace_kstaled_ring(node->node_id, "advance",
			   kstaled->head, kstaled->tail[0], kstaled->tail[1]);
unlock:
	spin_unlock_irq(&node->lru_lock);
}

static inline bool kstaled_consume_tail(struct kstaled_struct *kstaled,
					struct page *bucket, bool file)
{
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned long tail = bucket - kstaled->ring[file] + 1;

	lockdep_assert_held(&node->lru_lock);

	if (!tail || tail > KSTALED_MAX_AGE)
		return false;

	VM_BUG_ON_PAGE(page_count(bucket), bucket);
	VM_BUG_ON_PAGE(page_mapped(bucket), bucket);
	VM_BUG_ON_PAGE(bucket->flags != kstaled_flags(file), bucket);
	VM_BUG_ON_PAGE(kstaled_next_age(kstaled->tail[file]) != tail, bucket);

	list_del_init(&bucket->lru);
	kstaled->tail[file] = tail;

	trace_kstaled_ring(node->node_id, "consume",
			   kstaled->head, kstaled->tail[0], kstaled->tail[1]);

	return true;
}

static inline bool kstaled_sort_page(struct kstaled_struct *kstaled,
				     struct page *page, bool file)
{
	struct page *bucket;
	unsigned age = kstaled_get_age(page);

	lockdep_assert_held(&kstaled_node(kstaled)->lru_lock);

	/* valid age: page never used or the coldest */
	if (!age || age == kstaled->tail[file])
		return false;

	bucket = &kstaled->ring[file][age - 1];

	VM_BUG_ON_PAGE(page_count(bucket), bucket);
	VM_BUG_ON_PAGE(page_mapped(bucket), bucket);
	VM_BUG_ON_PAGE(bucket->flags != kstaled_flags(file), bucket);

	/* invalid age: e.g., switched to kswapd and back */
	if (list_empty(&bucket->lru))
		return false;

	/* valid age: not the coldest; sort it based the age */
	list_move_tail(&page->lru, &bucket->lru);

	return true;
}

static inline void kstaled_init_ref(struct page *page)
{
	BUILD_BUG_ON(KSTALED_REF_MASK & PAGE_TYPE_BASE);

	VM_BUG_ON_PAGE(~page->page_type >> PAGE_TYPE_NEXT, page);

	page->page_type &= ~BIT(PAGE_TYPE_NEXT);
}

static inline bool kstaled_has_ref(struct page *page)
{
	unsigned cnt;

	cnt = ~page->page_type >> PAGE_TYPE_NEXT;

	VM_BUG_ON_PAGE(cnt > KSTALED_MAX_REF, page);

	return cnt;
}

static inline bool kstaled_get_ref(struct page *page)
{
	unsigned cnt;
	unsigned old, new;

	do {
		old = READ_ONCE(page->page_type);
		cnt = ~old >> PAGE_TYPE_NEXT;
		if (!cnt)
			break;

		VM_BUG_ON_PAGE(cnt >= KSTALED_MAX_REF, page);

		new = old & ~KSTALED_REF_MASK;
		new |= ~(++cnt << PAGE_TYPE_NEXT) & KSTALED_REF_MASK;
	} while (cmpxchg(&page->page_type, old, new) != old);

	return cnt;
}

static inline bool kstaled_put_ref(struct page *page)
{
	unsigned cnt;
	unsigned old, new;

	do {
		old = READ_ONCE(page->page_type);
		cnt = ~old >> PAGE_TYPE_NEXT;

		VM_BUG_ON_PAGE(!cnt, page);
		VM_BUG_ON_PAGE(cnt > KSTALED_MAX_REF, page);

		new = old & ~KSTALED_REF_MASK;
		new |= ~(--cnt << PAGE_TYPE_NEXT) & KSTALED_REF_MASK;
	} while (cmpxchg(&page->page_type, old, new) != old);

	return cnt;
}

static int kstaled_check_pte(pte_t *ptep, unsigned head, int *hot)
{
	int i;
	struct page *page;
	unsigned long npages;
	unsigned age;
	pte_t pte = *ptep;
	int accessed = 0;

	lockdep_assert_held(ptlock_ptr(virt_to_page(ptep)));

	if (!pte_present(pte) || pte_special(pte) || pte_devmap(pte) ||
	    !pte_young(pte))
		return 1;

	VM_BUG_ON(!pfn_valid(pte_pfn(pte)));

	page = compound_head(pte_page(pte));
	if (!PageLRU(page) && !PageReclaim(page))
		return 1;

	npages = hpage_nr_pages(page);

	VM_BUG_ON_PAGE(PageHuge(page), page);
	VM_BUG_ON_PAGE(PageTail(page), page);
	VM_BUG_ON_PAGE(!page_count(page), page);
	VM_BUG_ON_PAGE(!page_mapped(page), page);
	VM_BUG_ON_PAGE(!PageAnon(page) && !page->mapping, page);
	VM_BUG_ON_PAGE(pte_page(pte) - page >= npages, page);

	npages -= pte_page(pte) - page;

	if (PageUnevictable(page) || PageMlocked(page))
		return npages;

	if (!PageAnon(page) && mapping_unevictable(page->mapping))
		return npages;

	for (i = 0; i < npages; i++)
		accessed += ptep_test_and_clear_young(NULL, 0, ptep + i);

	if (!accessed)
		return npages;

	age = kstaled_xchg_age(page, head);
	if (head && age != head && PageLRU(page))
		*hot += hpage_nr_pages(page);

	return npages;
}

static int kstaled_scan_ptep(struct page *page, unsigned head)
{
	int i = 0;
	pte_t *ptep = page_address(page);
	spinlock_t *ptl = ptlock_ptr(page);
	int hot = 0;

	VM_BUG_ON(!rcu_read_lock_sched_held());
	VM_BUG_ON_PAGE(!PageTable(page), page);
	VM_BUG_ON_PAGE(!kstaled_has_ref(page), page);

	spin_lock(ptl);
	while (i < PTRS_PER_PTE)
		i += kstaled_check_pte(ptep + i, head, &hot);
	spin_unlock(ptl);

	VM_BUG_ON(i != PTRS_PER_PTE);

	rcu_read_unlock_sched();
	cond_resched();
	rcu_read_lock_sched();

	return hot;
}

static int kstaled_check_ptep(pmd_t *pmdp, unsigned head)
{
	int hot;
	struct page *page;
	pmd_t pmd = READ_ONCE(*pmdp);

	VM_BUG_ON(!rcu_read_lock_sched_held());

	if (!pmd_present(pmd) || is_huge_zero_pmd(pmd) || pmd_devmap(pmd) ||
	    !pmd_young(pmd) || pmd_trans_huge(pmd) || pmd_huge(pmd))
		return 0;

	VM_BUG_ON(!pfn_valid(pmd_pfn(pmd)));

	page = pmd_page(pmd);
	if (!kstaled_get_ref(page))
		return 0;

	hot = kstaled_scan_ptep(page, head);
	kstaled_put_ptep(page);

	return hot;
}

static int kstaled_check_pmd(pmd_t *pmdp, unsigned head)
{
	struct page *page;
	unsigned age;
	pmd_t pmd = *pmdp;

	lockdep_assert_held(ptlock_ptr(virt_to_page(pmdp)));

	if (!pmd_present(pmd) || is_huge_zero_pmd(pmd) || pmd_devmap(pmd) ||
	    !pmd_young(pmd))
		return 0;

	VM_BUG_ON(!pfn_valid(pmd_pfn(pmd)));

	if (!pmd_trans_huge(pmd) && !pmd_huge(pmd)) {
		pmdp_test_and_clear_young(NULL, 0, pmdp);
		return 0;
	}

	page = pmd_page(pmd);
	if (!PageLRU(page) && !PageReclaim(page))
		return 0;

	VM_BUG_ON_PAGE(PageHuge(page), page);
	VM_BUG_ON_PAGE(!PageTransHuge(page), page);
	VM_BUG_ON_PAGE(!page_count(page), page);
	VM_BUG_ON_PAGE(!page_mapped(page), page);
	VM_BUG_ON_PAGE(!PageAnon(page) && !page->mapping, page);
	VM_BUG_ON_PAGE(!PageAnon(page) && !PageSwapBacked(page), page);

	if (PageUnevictable(page) || PageMlocked(page))
		return 0;

	if (!PageAnon(page) && mapping_unevictable(page->mapping))
		return 0;

	if (!pmdp_test_and_clear_young(NULL, 0, pmdp))
		return 0;

	age = kstaled_xchg_age(page, head);
	if (!head || age == head || !PageLRU(page))
		return 0;

	return hpage_nr_pages(page);
}

static int kstaled_scan_pmdp(struct page *page, unsigned head)
{
	int i;
	pmd_t *pmdp = page_address(page);
	spinlock_t *ptl = ptlock_ptr(page);
	int hot = 0;

	VM_BUG_ON(!rcu_read_lock_sched_held());
	VM_BUG_ON_PAGE(!kstaled_has_ref(page), page);

	for (i = 0; i < PTRS_PER_PMD; i++)
		hot += kstaled_check_ptep(pmdp + i, head);

	spin_lock(ptl);
	for (i = 0; i < PTRS_PER_PMD; i++)
		hot += kstaled_check_pmd(pmdp + i, head);
	spin_unlock(ptl);

	rcu_read_unlock_sched();
	cond_resched();
	rcu_read_lock_sched();

	return hot;
}

static void kstaled_walk_pmdp(struct kstaled_struct *kstaled)
{
	struct page *page;
	unsigned head = kstaled->head;
	unsigned long hot = 0;
	struct pglist_data *node = kstaled_node(kstaled);

	VM_BUG_ON(!current_is_kswapd());

	rcu_read_lock_sched();

	list_for_each_entry_rcu(page, &kstaled->pmdp_list, pmdp_list) {
		if (!kstaled_get_ref(page))
			continue;

		hot += kstaled_scan_pmdp(page, head);
		kstaled_put_pmdp(page);
	}

	rcu_read_unlock_sched();

	inc_node_state(node, KSTALED_BACKGROUND_AGING);
	if (hot)
		mod_node_page_state(node, KSTALED_BACKGROUND_HOT, hot);

	trace_kstaled_aging(node->node_id, true, hot);
}

static void kstaled_drain_active(struct kstaled_struct *kstaled, enum lru_list lru)
{
	int i;
	struct page *page, *prev;
	struct pglist_data *node = kstaled_node(kstaled);
	struct lruvec *lruvec = node_lruvec(node);
	unsigned long moved = 0;
	unsigned long zone_moved[MAX_NR_ZONES] = {};

	if (list_empty(lruvec->lists + lru))
		return;

	spin_lock_irq(&node->lru_lock);

	list_for_each_entry_safe_reverse(page, prev, lruvec->lists + lru, lru) {
		int npages = hpage_nr_pages(page);

		VM_BUG_ON_PAGE(!PageLRU(page), page);
		VM_BUG_ON_PAGE(PageHuge(page), page);
		VM_BUG_ON_PAGE(PageTail(page), page);
		VM_BUG_ON_PAGE(!PageActive(page), page);
		VM_BUG_ON_PAGE(PageUnevictable(page), page);
		VM_BUG_ON_PAGE(PageSwapBacked(page) == is_file_lru(lru), page);

		if (&prev->lru != lruvec->lists + lru)
			prefetchw(prev);

		ClearPageActive(page);
		list_move(&page->lru, lruvec->lists + lru - LRU_ACTIVE);
		zone_moved[page_zonenum(page)] += npages;
		moved += npages;
	}

	for (i = 0; i < MAX_NR_ZONES; i++) {
		__update_lru_size(lruvec, lru, i, -zone_moved[i]);
		__update_lru_size(lruvec, lru - LRU_ACTIVE, i, zone_moved[i]);
	}
	__count_vm_events(PGDEACTIVATE, moved);

	spin_unlock_irq(&node->lru_lock);

	cond_resched();
}

static void kstaled_trim_tail(struct kstaled_struct *kstaled)
{
	int i;
	enum lru_list lru;
	struct page *page, *prev;
	struct pglist_data *node = kstaled_node(kstaled);
	struct lruvec *lruvec = node_lruvec(node);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		lru = kstaled_lru(i);

		page = list_entry(READ_ONCE(lruvec->lists[lru].prev),
				  struct page, lru);
		if (page - kstaled->ring[i] < KSTALED_MAX_AGE)
			break;
	}

	if (i == KSTALED_LRU_TYPES)
		return;

	spin_lock_irq(&node->lru_lock);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		lru = kstaled_lru(i);

		list_for_each_entry_safe_reverse(page, prev,
						 lruvec->lists + lru, lru) {
			if (!kstaled_consume_tail(kstaled, page, i))
				break;
		}
	}

	spin_unlock_irq(&node->lru_lock);
}

static void kstaled_prepare_lru(struct kstaled_struct *kstaled)
{
	enum lru_list lru;

	lru_add_drain_all();

	/* we only use inactive lists; empty active lists */
	for_each_evictable_lru(lru) {
		if (is_active_lru(lru))
			kstaled_drain_active(kstaled, lru);
	}

	/* deallocations could leave empty buckets behind */
	kstaled_trim_tail(kstaled);
}

static bool kstaled_lru_reclaimable(bool file)
{
	struct backing_dev_info *bdi;
	bool can_writeback = false;

	if (!file) {
		if (get_nr_swap_pages() > SWAP_BATCH)
			return true;

		if (READ_ONCE(total_swap_pages))
			pr_warn_ratelimited("kstaled: swapfile full\n");

		return false;
	}

	rcu_read_lock();

	list_for_each_entry_rcu(bdi, &bdi_list, bdi_list) {
		if (bdi->owner && bdi->owner->parent &&
		    !bdi_write_congested(bdi)) {
			can_writeback = true;
			break;
		}
	}

	rcu_read_unlock();

	if (!can_writeback)
		pr_warn_ratelimited("kstaled: writeback congested\n");

	return can_writeback;
}

static unsigned long kstaled_reclaim_lru(struct kstaled_struct *kstaled,
					 bool file, gfp_t gfp_mask,
					 unsigned long *attempts)
{
	int i;
	LIST_HEAD(list);
	unsigned span;
	bool clean_only;
	struct page *page, *prev;
	unsigned long batch = 0;
	unsigned long scanned = 0;
	unsigned long isolated = 0;
	unsigned long reclaimed = 0;
	struct pglist_data *node = kstaled_node(kstaled);
	struct lruvec *lruvec = node_lruvec(node);
	enum lru_list lru = kstaled_lru(file);
	unsigned long zone_isolated[MAX_NR_ZONES] = {};

	clean_only = file ? !current_is_kswapd() : !(gfp_mask & __GFP_IO);

	spin_lock_irq(&node->lru_lock);

	kstaled->peeked = true;
	span = kstaled_ring_span(kstaled, file);

	if (kstaled_ring_low(kstaled, file)) {
		__inc_node_state(node, KSTALED_RECLAIM_STALLS);
		goto unlock;
	}

	list_for_each_entry_safe_reverse(page, prev, lruvec->lists + lru, lru) {
		int npages = hpage_nr_pages(page);

		VM_BUG_ON_PAGE(!PageLRU(page), page);
		VM_BUG_ON_PAGE(PageHuge(page), page);
		VM_BUG_ON_PAGE(PageTail(page), page);
		VM_BUG_ON_PAGE(PageActive(page), page);
		VM_BUG_ON_PAGE(PageUnevictable(page), page);
		VM_BUG_ON_PAGE(PageSwapBacked(page) == file, page);

		if (&prev->lru != lruvec->lists + lru)
			prefetchw(prev);

		if (kstaled_consume_tail(kstaled, page, file))
			continue;

		if (kstaled_ring_low(kstaled, file))
			break;

		scanned++;

		if (kstaled_sort_page(kstaled, page, file))
			continue;

		if (batch++ == SWAP_BATCH)
			break;

		if (clean_only && kstaled_get_age(page) &&
		    (PageDirty(page) ||
		     (!file && PageAnon(page) && !PageSwapCache(page))))
			break;

		if (!get_page_unless_zero(page))
			continue;

		set_mask_bits(&page->flags, KSTALED_AGE_MASK | BIT(PG_lru),
			      BIT(PG_reclaim));
		list_move(&page->lru, &list);
		zone_isolated[page_zonenum(page)] += npages;
		isolated += npages;
	}

	for (i = 0; i < MAX_NR_ZONES; i++)
		__update_lru_size(lruvec, lru, i, -zone_isolated[i]);
	__mod_node_page_state(node, NR_ISOLATED_ANON + file, isolated);

	if (current_is_kswapd())
		__count_vm_events(PGSCAN_KSWAPD, scanned);
	else
		__count_vm_events(PGSCAN_DIRECT, scanned);
unlock:
	spin_unlock_irq(&node->lru_lock);

	*attempts += batch;
	if (scanned)
		atomic_long_add(scanned, &kstaled->scanned);
	if (isolated)
		reclaimed = node_shrink_list(node, &list, isolated, file, gfp_mask);
	if (reclaimed)
		wake_up_all(&kstaled->throttled);

	trace_kstaled_reclaim(node->node_id, file, clean_only, span, scanned,
			      scanned - batch, isolated, reclaimed);

	return reclaimed;
}

static bool kstaled_balance_lru(struct kstaled_struct *kstaled,
				bool file_only, unsigned long *reclaimed)
{
	int i;
	bool file[KSTALED_LRU_TYPES];

	if (kstaled_ring_empty(kstaled))
		return false;

	/*
	 * We pick the list that has the coldest pages. It's most likely
	 * the file list because direct reclaim always tries the anon list
	 * first. If both lists have the same age, we choose the anon list
	 * because going through fs is usually slower.
	 */
	file[0] = kstaled_ring_span(kstaled, true) >
		  kstaled_ring_span(kstaled, false);
	file[1] = !file[0];

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		unsigned long attempts = 0;

		if (file_only && !file[i])
			break;

		if (!kstaled_lru_reclaimable(file[i]))
			continue;

		*reclaimed += kstaled_reclaim_lru(kstaled, file[i], GFP_KERNEL,
						  &attempts);
		if (attempts)
			return true;
	}

	return false;
}

static unsigned long kstaled_shrink_slab(struct kstaled_struct *kstaled,
					 gfp_t gfp_mask)
{
	int i;
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned long scanned = atomic_long_xchg(&kstaled->scanned, 0);
	unsigned long total = 0;

	if (!scanned)
		return 0;

	for (i = 0; i < KSTALED_LRU_TYPES; i++)
		total += node_page_state(node, NR_LRU_BASE + kstaled_lru(i));

	/* we don't age slab pages; use the same heuristic as kswapd */
	return node_shrink_slab(node, scanned, total, gfp_mask);
}

static void kstaled_wakeup_kcompactd(struct kstaled_struct *kstaled,
				     unsigned long reclaimed)
{
	int i;
	int order = 0;
	struct pglist_data *node = kstaled_node(kstaled);

	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = node->node_zones + i;

		if (!managed_zone(zone) || !zone->compact_defer_shift)
			continue;

		order = max(order, zone->compact_order_failed);
	}

	/* try compaction if we've freed enough pages for migration */
	order = min(order, MAX_ORDER - 1);
	if (order && compact_gap(order) < reclaimed)
		wakeup_kcompactd(node, order, MAX_NR_ZONES);
}

struct kstaled_context {
	/* to track how fast free pages drop */
	unsigned long free;
	/* to track how fast hot pages grow */
	unsigned long hot;
	unsigned long nr_to_reclaim;
	unsigned ratio;
	bool walk_pmdp;
};

static void kstaled_estimate_demands(struct kstaled_struct *kstaled,
				     struct kstaled_context *ctx)
{
	int i;
	unsigned long hot, drop, growth;
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned long total = 0, free = 0, low = 0, high = 0;

	for (i = 0; i < KSTALED_LRU_TYPES; i++)
		total += node_page_state(node, NR_LRU_BASE + kstaled_lru(i));

	/* assume all zones are subject to reclaim for now */
	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = node->node_zones + i;

		if (!managed_zone(zone))
			continue;

		free += zone_page_state_snapshot(zone, NR_FREE_PAGES);
		low += low_wmark_pages(zone);
		high += high_wmark_pages(zone);
	}

	if (low > high)
		pr_err_ratelimited("kstaled: invalid water marks (low %lu > high %lu)\n",
				   low, high);

	/*
	 * Two things we need to estimate: demands for free pages and
	 * cold pages. Our heuristics are based on how fast free pages
	 * drop and hot pages grow. For the former, the ratio is
	 * estimated-to-measured; for the latter, it's cold-to-hot.
	 */
	drop = ctx->free > free ? ctx->free - free : 0;
	drop *= ctx->ratio;
	ctx->free = free;

	hot = node_page_state(node, KSTALED_DIRECT_HOT);
	growth = hot > ctx->hot ? hot - ctx->hot : 0;

	/*
	 * First, estimate if we need to do anything at all. This has to
	 * be most proactive because underestimate can stall reclaim. If
	 * reclaim was attempted last time, it might be needed again.
	 */
	if (!ctx->nr_to_reclaim && free >= high + drop) {
		ctx->walk_pmdp = false;
		ctx->nr_to_reclaim = 0;
		goto done;
	}

	if (kstaled_ring_empty(kstaled)) {
		ctx->walk_pmdp = true;
		ctx->nr_to_reclaim = 0;
		goto done;
	}

	/*
	 * Second, estimate if we need to walk PMD. This is fairly
	 * proactive because we can fall back to direct aging as long
	 * as the ring is not running low.
	 */
	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		if (kstaled_ring_low(kstaled, i)) {
			ctx->walk_pmdp = true;
			goto may_reclaim;
		}
	}

	/*
	 * We want to pace the PMD walk and avoid consecutive walks
	 * between which reclaim wasn't even attempted.
	 */
	if (ctx->walk_pmdp && !READ_ONCE(kstaled->peeked)) {
		ctx->walk_pmdp = false;
		goto may_reclaim;
	}

	/*
	 * We want to have same number of pages in all buckets. For
	 * cold-to-hot ratio R, we need R+1 buckets. And we create a new
	 * bucket when hot pages reach 1/(R+1) of total pages.
	 */
	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		if (kstaled_ring_span(kstaled, i) <= ctx->ratio) {
			ctx->walk_pmdp = true;
			goto may_reclaim;
		}
	}

	if (growth * (ctx->ratio + 1) > total) {
		ctx->walk_pmdp = true;
		goto may_reclaim;
	}

	ctx->walk_pmdp = false;
may_reclaim:
	/*
	 * Finally, estimate if we also need to reclaim. This is least
	 * proactive because we can fall back to direct reclaim.
	 */
	if (free >= min(high, low + drop)) {
		ctx->nr_to_reclaim = 0;
		goto done;
	}

	if (ctx->nr_to_reclaim)
		count_vm_event(KSWAPD_HIGH_WMARK_HIT_QUICKLY);
	if (free <= low)
		count_vm_event(KSWAPD_LOW_WMARK_HIT_QUICKLY);
	count_vm_event(PAGEOUTRUN);

	ctx->nr_to_reclaim = min(high, high + drop - free);
done:
	trace_kstaled_estimate(node->node_id, total, free, drop, growth,
			       kstaled_ring_span(kstaled, false),
			       kstaled_ring_span(kstaled, true),
			       ctx->walk_pmdp, ctx->nr_to_reclaim);
}

static int kstaled_node_worker(void *arg)
{
	DEFINE_WAIT(wait);
	struct kstaled_struct *kstaled = arg;
	struct pglist_data *node = kstaled_node(kstaled);
	struct kstaled_context ctx = {};

	set_freezable();
	current->flags |= PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD;

	while (!kthread_should_stop()) {
		long timeout = HZ;
		unsigned long reclaimed = 0;

		ctx.ratio = READ_ONCE(kstaled_ratio);

		if (try_to_freeze())
			continue;

		if (!ctx.ratio) {
			kstaled_stop_ring(kstaled);
			memset(&ctx, 0, sizeof(ctx));
			atomic_long_set(&kstaled->scanned, 0);
			timeout = MAX_SCHEDULE_TIMEOUT;
			goto sleep;
		}

		timeout += jiffies;

		kstaled_prepare_lru(kstaled);

		kstaled_estimate_demands(kstaled, &ctx);

		if (ctx.walk_pmdp) {
			kstaled_walk_pmdp(kstaled);
			kstaled_advance_head(kstaled);
			wake_up_all(&kstaled->throttled);
			ctx.hot = node_page_state(node, KSTALED_DIRECT_HOT);
		}

		while (kstaled_balance_lru(kstaled,
		       reclaimed >= ctx.nr_to_reclaim, &reclaimed))
			;

		reclaimed += kstaled_shrink_slab(kstaled, GFP_KERNEL);

		if (reclaimed)
			kstaled_wakeup_kcompactd(kstaled, reclaimed);

		timeout -= jiffies;
		if (timeout < 0) {
			mod_node_page_state(node, KSTALED_TIMEOUT, -timeout);
			continue;
		}
sleep:
		prepare_to_wait(&kstaled_wait, &wait, TASK_INTERRUPTIBLE);
		schedule_timeout(timeout);
		finish_wait(&kstaled_wait, &wait);
	}

	return 0;
}

static ssize_t ratio_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", kstaled_ratio);
}

static ssize_t ratio_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t len)
{
	int err;
	unsigned new, old = kstaled_ratio;

	err = kstrtouint(buf, 10, &new);
	if (err)
		return err;

	if (new > (KSTALED_MAX_AGE >> 1)) {
		pr_warn_ratelimited("kstaled: invalid ratio %u\n", new);
		return -EINVAL;
	}

	pr_info_ratelimited("kstaled: set ratio to %u\n", new);

	kstaled_ratio = new;

	if (!old && new)
		wake_up_interruptible_all(&kstaled_wait);

	return len;
}

static struct kobj_attribute kstaled_ratio_attr = __ATTR_RW(ratio);

static struct attribute *kstaled_attrs[] = {
	&kstaled_ratio_attr.attr,
	NULL
};

static struct attribute_group kstaled_attr_group = {
	.name = "kstaled",
	.attrs = kstaled_attrs,
};

static int __init kstaled_module_init(void)
{
	int err;
	struct task_struct *task;
	struct pglist_data *node = NODE_DATA(first_memory_node);
	struct kstaled_struct *kstaled = &node->kstaled;

	INIT_LIST_HEAD(&kstaled->pmdp_list);
	spin_lock_init(&kstaled->pmdp_list_lock);
	init_waitqueue_head(&kstaled->throttled);
	kstaled_init_ring(kstaled);

	err = sysfs_create_group(mm_kobj, &kstaled_attr_group);
	if (err) {
		pr_err("kstaled: failed to create sysfs group\n");
		return err;
	}

	task = kthread_run(kstaled_node_worker, kstaled, "kstaled");
	err = PTR_ERR_OR_ZERO(task);
	if (err) {
		pr_err("kstaled: failed to start worker thread\n");
		sysfs_remove_group(mm_kobj, &kstaled_attr_group);
		return err;
	}

	return 0;
}

late_initcall(kstaled_module_init);

inline bool kstaled_is_enabled(void)
{
	return READ_ONCE(kstaled_ratio);
}

inline bool kstaled_ring_inuse(struct pglist_data *node)
{
	return !kstaled_ring_empty(&node->kstaled);
}

static void kstaled_free_ptep(struct rcu_head *rcu)
{
	struct page *page = container_of(rcu, struct page, rcu_head);

	__free_page(page);
}

static void kstaled_free_pmdp(struct rcu_head *rcu)
{
	struct page *page = container_of(rcu, struct page, rcu_head);

	/* clear mapping because it overlaps with pmdp_list */
	page->mapping = NULL;
	__free_page(page);
}

bool kstaled_put_ptep(struct page *page)
{
	VM_BUG_ON_PAGE(!PageTable(page), page);

	if (!kstaled_has_ref(page))
		return false;

	if (kstaled_put_ref(page))
		return true;

	pgtable_page_dtor(page);
	call_rcu_sched(&page->rcu_head, kstaled_free_ptep);

	return true;
}

bool kstaled_put_pmdp(struct page *page)
{
	struct kstaled_struct *kstaled;

	if (!kstaled_has_ref(page))
		return false;

	if (kstaled_put_ref(page))
		return true;

	kstaled = kstaled_of_page(page);
	spin_lock(&kstaled->pmdp_list_lock);
	list_del_rcu(&page->pmdp_list);
	spin_unlock(&kstaled->pmdp_list_lock);

	pgtable_pmd_page_dtor(page);
	call_rcu_sched(&page->rcu_head, kstaled_free_pmdp);

	return true;
}

void kstaled_init_ptep(pmd_t *pmdp, struct page *page)
{
	VM_BUG_ON_PAGE(!PageTable(page), page);

	/* ignore temporary pmd_t used during thp split */
	if (object_is_on_stack(pmdp))
		return;

	lockdep_assert_held(ptlock_ptr(virt_to_page(pmdp)));

	if (kstaled_has_ref(virt_to_page(pmdp)) && !kstaled_has_ref(page))
		kstaled_init_ref(page);
}

void kstaled_init_pmdp(struct mm_struct *mm, pmd_t *pmdp)
{
	struct page *page;
	struct kstaled_struct *kstaled;

	/* ignore static mm_struct like init_mm, etc. */
	if (core_kernel_data((unsigned long)mm))
		return;

	lockdep_assert_held(&mm->page_table_lock);

	page = virt_to_page(pmdp);
	if (kstaled_has_ref(page))
		return;

	kstaled_init_ref(page);

	VM_BUG_ON_PAGE(page->mapping, page);

	kstaled = kstaled_of_page(page);
	spin_lock(&kstaled->pmdp_list_lock);
	list_add_tail_rcu(&page->pmdp_list, &kstaled->pmdp_list);
	spin_unlock(&kstaled->pmdp_list_lock);
}

inline unsigned kstaled_get_age(struct page *page)
{
	return (READ_ONCE(page->flags) & KSTALED_AGE_MASK) >> KSTALED_AGE_PGOFF;
}

inline void kstaled_set_age(struct page *page)
{
	struct kstaled_struct *kstaled = kstaled_of_page(page);
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned head = READ_ONCE(kstaled->head);
	int npages = hpage_nr_pages(page);

	VM_BUG_ON_PAGE(!PageLRU(page), page);
	VM_BUG_ON_PAGE(PageHuge(page), page);
	VM_BUG_ON_PAGE(PageTail(page), page);
	VM_BUG_ON_PAGE(!page_count(page), page);
	lockdep_assert_held(&node->lru_lock);

	if (PageUnevictable(page))
		return;

	kstaled_xchg_age(page, head);
	if (head)
		__mod_node_page_state(node, KSTALED_DIRECT_HOT, npages);
}

inline void kstaled_clear_age(struct page *page)
{
	struct kstaled_struct *kstaled = kstaled_of_page(page);
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned age, head = READ_ONCE(kstaled->head);
	int npages = hpage_nr_pages(page);

	VM_BUG_ON_PAGE(!PageLRU(page), page);
	VM_BUG_ON_PAGE(PageHuge(page), page);
	VM_BUG_ON_PAGE(PageTail(page), page);
	lockdep_assert_held(&node->lru_lock);

	if (PageUnevictable(page))
		return;

	age = kstaled_xchg_age(page, 0);
	if (head && age == head)
		__mod_node_page_state(node, KSTALED_DIRECT_HOT, -npages);
}

inline void kstaled_update_age(struct page *page)
{
	struct kstaled_struct *kstaled = kstaled_of_page(page);
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned age, head = READ_ONCE(kstaled->head);
	int npages = hpage_nr_pages(page);

	if (!PageLRU(page) && !PageReclaim(page))
		return;

	VM_BUG_ON_PAGE(PageHuge(page), page);
	VM_BUG_ON_PAGE(PageTail(page), page);
	VM_BUG_ON_PAGE(!page_count(page), page);

	if (PageUnevictable(page) || PageMlocked(page))
		return;

	if (!PageAnon(page) && mapping_unevictable(page->mapping))
		return;

	age = kstaled_xchg_age(page, head);
	if (head && age != head && PageLRU(page))
		mod_node_page_state(node, KSTALED_DIRECT_HOT, npages);
}

void kstaled_direct_aging(struct page_vma_mapped_walk *pvmw)
{
	int i = 0;
	struct kstaled_struct *kstaled = kstaled_of_page(pvmw->page);
	struct pglist_data *node = kstaled_node(kstaled);
	unsigned head = READ_ONCE(kstaled->head);
	struct page *page = virt_to_page(pvmw->pte);
	pte_t *ptep = page_address(page);
	pmd_t *pmdp = pvmw->pmd;
	spinlock_t *ptl = ptlock_ptr(virt_to_page(pmdp));
	int hot = 0;

	VM_BUG_ON_PAGE(!PageTable(page), page);
	VM_BUG_ON_PAGE(ptlock_ptr(page) != pvmw->ptl, page);
	lockdep_assert_held(ptlock_ptr(page));

	while (i < PTRS_PER_PTE)
		i += kstaled_check_pte(ptep + i, head, &hot);

	VM_BUG_ON(i != PTRS_PER_PTE);

	page_vma_mapped_walk_done(pvmw);

	/* we've unlocked the ptep, so it's safe to lock the pmdp */
	spin_lock(ptl);
	if (pmd_page(*pmdp) == page)
		pmdp_test_and_clear_young(NULL, 0, pmdp);
	spin_unlock(ptl);

	inc_node_state(node, KSTALED_DIRECT_AGING);
	if (hot)
		mod_node_page_state(node, KSTALED_DIRECT_HOT, hot);

	trace_kstaled_aging(node->node_id, false, hot);
}

inline void kstaled_enable_throttle(void)
{
	if (current->mm)
		atomic_dec(&current->mm->throttle_disabled);
}

inline void kstaled_disable_throttle(void)
{
	VM_BUG_ON(!current->mm);

	if (current->mm)
		atomic_inc(&current->mm->throttle_disabled);
}

static inline bool kstaled_throttle_disabled(struct task_struct *task)
{
	return (task->flags & PF_KTHREAD) ||
	       (task->mm && atomic_read(&task->mm->throttle_disabled));
}

bool kstaled_throttle_alloc(struct zone *zone, int order, gfp_t gfp_mask)
{
	long timeout;
	DEFINE_WAIT(wait);
	struct kstaled_struct *kstaled = kstaled_of_zone(zone);

	if (kstaled_throttle_disabled(current)) {
		pr_warn_ratelimited("kstaled: %s (order %d, mask %pGg) not throttled\n",
				    current->comm, order, &gfp_mask);
		return true;
	}

	count_vm_event(PGSCAN_DIRECT_THROTTLE);

	prepare_to_wait_exclusive(&kstaled->throttled, &wait, TASK_KILLABLE);
	timeout = schedule_timeout(HZ);
	finish_wait(&kstaled->throttled, &wait);

	pr_warn_ratelimited("kstaled: %s (order %d, mask %pGg) throttled for %u ms\n",
			    current->comm, order, &gfp_mask,
			    jiffies_to_msecs(HZ - timeout));

	return timeout;
}

bool kstaled_direct_reclaim(struct zone *zone, int order, gfp_t gfp_mask)
{
	int i;
	bool interrupted;
	unsigned long attempts = 0;
	struct kstaled_struct *kstaled = kstaled_of_zone(zone);

	VM_BUG_ON(current_is_kswapd());

	if (fatal_signal_pending(current))
		return false;

	__count_zid_vm_events(ALLOCSTALL, zone_idx(zone), 1);

	for (i = 0; i < KSTALED_LRU_TYPES; i++) {
		if (kstaled_lru_reclaimable(i) &&
		    kstaled_reclaim_lru(kstaled, i, gfp_mask, &attempts))
			return true;
	}

	if (kstaled_shrink_slab(kstaled, gfp_mask))
		return true;

	/* unlimited retries if the thread has disabled throttle */
	if (kstaled_throttle_disabled(current))
		return true;

	/* limited retries in case we are not making any progress */
	if (attempts)
		return false;

	/* throttle before retry because cold pages have run out */
	interrupted = kstaled_throttle_alloc(zone, order, gfp_mask);

	return fatal_signal_pending(current) || interrupted;
}
