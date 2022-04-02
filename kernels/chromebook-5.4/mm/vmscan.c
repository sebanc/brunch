// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/mm/vmscan.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Swap reorganised 29.12.95, Stephen Tweedie.
 *  kswapd added: 7.1.96  sct
 *  Removed kswapd_ctl limits, and swap out as many pages as needed
 *  to bring the system back to freepages.high: 2.4.97, Rik van Riel.
 *  Zone aware kswapd started 02/00, Kanoj Sarcar (kanoj@sgi.com).
 *  Multiqueue VM started 5.8.00, Rik van Riel.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/vmpressure.h>
#include <linux/vmstat.h>
#include <linux/file.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* for try_to_release_page(),
					buffer_heads_over_limit */
#include <linux/mm_inline.h>
#include <linux/backing-dev.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/compaction.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memcontrol.h>
#include <linux/delayacct.h>
#include <linux/sysctl.h>
#include <linux/oom.h>
#include <linux/pagevec.h>
#include <linux/prefetch.h>
#include <linux/printk.h>
#include <linux/dax.h>
#include <linux/psi.h>
#include <linux/memory.h>
#include <linux/pagewalk.h>
#include <linux/shmem_fs.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/mmu_notifier.h>

#include <asm/tlbflush.h>
#include <asm/div64.h>

#include <linux/swapops.h>
#include <linux/balloon_compaction.h>

#include "internal.h"

#define CREATE_TRACE_POINTS
#include <trace/events/vmscan.h>

struct scan_control {
	/* How many pages shrink_list() should reclaim */
	unsigned long nr_to_reclaim;

	/*
	 * Nodemask of nodes allowed by the caller. If NULL, all nodes
	 * are scanned.
	 */
	nodemask_t	*nodemask;

	/*
	 * The memory cgroup that hit its limit and as a result is the
	 * primary target of this reclaim invocation.
	 */
	struct mem_cgroup *target_mem_cgroup;

	/* Writepage batching in laptop mode; RECLAIM_WRITE */
	unsigned int may_writepage:1;

	/* Can mapped pages be reclaimed? */
	unsigned int may_unmap:1;

	/* Can pages be swapped as part of reclaim? */
	unsigned int may_swap:1;

	/*
	 * Cgroup memory below memory.low is protected as long as we
	 * don't threaten to OOM. If any cgroup is reclaimed at
	 * reduced force or passed over entirely due to its memory.low
	 * setting (memcg_low_skipped), and nothing is reclaimed as a
	 * result, then go back for one more cycle that reclaims the protected
	 * memory (memcg_low_reclaim) to avert OOM.
	 */
	unsigned int memcg_low_reclaim:1;
	unsigned int memcg_low_skipped:1;

	unsigned int hibernation_mode:1;

	/* One of the zones is ready for compaction */
	unsigned int compaction_ready:1;

	/* Allocation order */
	s8 order;

	/* Scan (total_size >> priority) pages at once */
	s8 priority;

	/* The highest zone to isolate pages for reclaim from */
	s8 reclaim_idx;

	/* This context's GFP mask */
	gfp_t gfp_mask;

	/* Incremented by the number of inactive pages that were scanned */
	unsigned long nr_scanned;

	/* Number of pages freed so far during a call to shrink_zones() */
	unsigned long nr_reclaimed;

	struct {
		unsigned int dirty;
		unsigned int unqueued_dirty;
		unsigned int congested;
		unsigned int writeback;
		unsigned int immediate;
		unsigned int file_taken;
		unsigned int taken;
	} nr;

	/* for recording the reclaimed slab by now */
	struct reclaim_state reclaim_state;
};

#ifdef ARCH_HAS_PREFETCH
#define prefetch_prev_lru_page(_page, _base, _field)			\
	do {								\
		if ((_page)->lru.prev != _base) {			\
			struct page *prev;				\
									\
			prev = lru_to_page(&(_page->lru));		\
			prefetch(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetch_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

#ifdef ARCH_HAS_PREFETCHW
#define prefetchw_prev_lru_page(_page, _base, _field)			\
	do {								\
		if ((_page)->lru.prev != _base) {			\
			struct page *prev;				\
									\
			prev = lru_to_page(&(_page->lru));		\
			prefetchw(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetchw_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

/*
 * From 0 .. 100.  Higher means more swappy.
 */
int vm_swappiness = 60;
/*
 * The total number of pages which are beyond the high watermark within all
 * zones.
 */
unsigned long vm_total_pages;

static void set_task_reclaim_state(struct task_struct *task,
				   struct reclaim_state *rs)
{
	/* Check for an overwrite */
	WARN_ON_ONCE(rs && task->reclaim_state);

	/* Check for the nulling of an already-nulled member */
	WARN_ON_ONCE(!rs && !task->reclaim_state);

	task->reclaim_state = rs;
}

int min_filelist_kbytes_handler(struct ctl_table *table, int write,
				void __user *buf, size_t *len, loff_t *pos)
{
	size_t written;

	if (!lru_gen_enabled() || write)
		return proc_dointvec(table, write, buf, len, pos);

	if (!*len || *pos) {
		*len = 0;
		return 0;
	}

	written = min_t(size_t, 2, *len);
	if (copy_to_user(buf, "0\n", written))
		return -EFAULT;

	*len = written;
	*pos = written;

	return 0;
}

static LIST_HEAD(shrinker_list);
static DECLARE_RWSEM(shrinker_rwsem);

#ifdef CONFIG_MEMCG
/*
 * We allow subsystems to populate their shrinker-related
 * LRU lists before register_shrinker_prepared() is called
 * for the shrinker, since we don't want to impose
 * restrictions on their internal registration order.
 * In this case shrink_slab_memcg() may find corresponding
 * bit is set in the shrinkers map.
 *
 * This value is used by the function to detect registering
 * shrinkers and to skip do_shrink_slab() calls for them.
 */
#define SHRINKER_REGISTERING ((struct shrinker *)~0UL)

static DEFINE_IDR(shrinker_idr);
static int shrinker_nr_max;

static int prealloc_memcg_shrinker(struct shrinker *shrinker)
{
	int id, ret = -ENOMEM;

	down_write(&shrinker_rwsem);
	/* This may call shrinker, so it must use down_read_trylock() */
	id = idr_alloc(&shrinker_idr, SHRINKER_REGISTERING, 0, 0, GFP_KERNEL);
	if (id < 0)
		goto unlock;

	if (id >= shrinker_nr_max) {
		if (memcg_expand_shrinker_maps(id)) {
			idr_remove(&shrinker_idr, id);
			goto unlock;
		}

		shrinker_nr_max = id + 1;
	}
	shrinker->id = id;
	ret = 0;
unlock:
	up_write(&shrinker_rwsem);
	return ret;
}

static void unregister_memcg_shrinker(struct shrinker *shrinker)
{
	int id = shrinker->id;

	BUG_ON(id < 0);

	down_write(&shrinker_rwsem);
	idr_remove(&shrinker_idr, id);
	up_write(&shrinker_rwsem);
}

static bool global_reclaim(struct scan_control *sc)
{
	return !sc->target_mem_cgroup;
}

/**
 * sane_reclaim - is the usual dirty throttling mechanism operational?
 * @sc: scan_control in question
 *
 * The normal page dirty throttling mechanism in balance_dirty_pages() is
 * completely broken with the legacy memcg and direct stalling in
 * shrink_page_list() is used for throttling instead, which lacks all the
 * niceties such as fairness, adaptive pausing, bandwidth proportional
 * allocation and configurability.
 *
 * This function tests whether the vmscan currently in progress can assume
 * that the normal dirty throttling mechanism is operational.
 */
static bool sane_reclaim(struct scan_control *sc)
{
	struct mem_cgroup *memcg = sc->target_mem_cgroup;

	if (!memcg)
		return true;
#ifdef CONFIG_CGROUP_WRITEBACK
	if (cgroup_subsys_on_dfl(memory_cgrp_subsys))
		return true;
#endif
	return false;
}

static void set_memcg_congestion(pg_data_t *pgdat,
				struct mem_cgroup *memcg,
				bool congested)
{
	struct mem_cgroup_per_node *mn;

	if (!memcg)
		return;

	mn = mem_cgroup_nodeinfo(memcg, pgdat->node_id);
	WRITE_ONCE(mn->congested, congested);
}

static bool memcg_congested(pg_data_t *pgdat,
			struct mem_cgroup *memcg)
{
	struct mem_cgroup_per_node *mn;

	mn = mem_cgroup_nodeinfo(memcg, pgdat->node_id);
	return READ_ONCE(mn->congested);

}
#else
static int prealloc_memcg_shrinker(struct shrinker *shrinker)
{
	return 0;
}

static void unregister_memcg_shrinker(struct shrinker *shrinker)
{
}

static bool global_reclaim(struct scan_control *sc)
{
	return true;
}

static bool sane_reclaim(struct scan_control *sc)
{
	return true;
}

static inline void set_memcg_congestion(struct pglist_data *pgdat,
				struct mem_cgroup *memcg, bool congested)
{
}

static inline bool memcg_congested(struct pglist_data *pgdat,
			struct mem_cgroup *memcg)
{
	return false;

}
#endif

/*
 * This misses isolated pages which are not accounted for to save counters.
 * As the data only determines if reclaim or compaction continues, it is
 * not expected that isolated pages will be a dominating factor.
 */
unsigned long zone_reclaimable_pages(struct zone *zone)
{
	unsigned long nr;

	nr = zone_page_state_snapshot(zone, NR_ZONE_INACTIVE_FILE) +
		zone_page_state_snapshot(zone, NR_ZONE_ACTIVE_FILE);
	if (get_nr_swap_pages() > 0)
		nr += zone_page_state_snapshot(zone, NR_ZONE_INACTIVE_ANON) +
			zone_page_state_snapshot(zone, NR_ZONE_ACTIVE_ANON);

	return nr;
}

/**
 * lruvec_lru_size -  Returns the number of pages on the given LRU list.
 * @lruvec: lru vector
 * @lru: lru to use
 * @zone_idx: zones to consider (use MAX_NR_ZONES for the whole LRU list)
 */
unsigned long lruvec_lru_size(struct lruvec *lruvec, enum lru_list lru, int zone_idx)
{
	unsigned long lru_size = 0;
	int zid;

	if (!mem_cgroup_disabled()) {
		for (zid = 0; zid < MAX_NR_ZONES; zid++)
			lru_size += mem_cgroup_get_zone_lru_size(lruvec, lru, zid);
	} else
		lru_size = node_page_state(lruvec_pgdat(lruvec), NR_LRU_BASE + lru);

	for (zid = zone_idx + 1; zid < MAX_NR_ZONES; zid++) {
		struct zone *zone = &lruvec_pgdat(lruvec)->node_zones[zid];
		unsigned long size;

		if (!managed_zone(zone))
			continue;

		if (!mem_cgroup_disabled())
			size = mem_cgroup_get_zone_lru_size(lruvec, lru, zid);
		else
			size = zone_page_state(&lruvec_pgdat(lruvec)->node_zones[zid],
				       NR_ZONE_LRU_BASE + lru);
		lru_size -= min(size, lru_size);
	}

	return lru_size;

}

/*
 * Add a shrinker callback to be called from the vm.
 */
int prealloc_shrinker(struct shrinker *shrinker)
{
	unsigned int size = sizeof(*shrinker->nr_deferred);

	if (shrinker->flags & SHRINKER_NUMA_AWARE)
		size *= nr_node_ids;

	shrinker->nr_deferred = kzalloc(size, GFP_KERNEL);
	if (!shrinker->nr_deferred)
		return -ENOMEM;

	if (shrinker->flags & SHRINKER_MEMCG_AWARE) {
		if (prealloc_memcg_shrinker(shrinker))
			goto free_deferred;
	}

	return 0;

free_deferred:
	kfree(shrinker->nr_deferred);
	shrinker->nr_deferred = NULL;
	return -ENOMEM;
}

void free_prealloced_shrinker(struct shrinker *shrinker)
{
	if (!shrinker->nr_deferred)
		return;

	if (shrinker->flags & SHRINKER_MEMCG_AWARE)
		unregister_memcg_shrinker(shrinker);

	kfree(shrinker->nr_deferred);
	shrinker->nr_deferred = NULL;
}

void register_shrinker_prepared(struct shrinker *shrinker)
{
	down_write(&shrinker_rwsem);
	list_add_tail(&shrinker->list, &shrinker_list);
#ifdef CONFIG_MEMCG
	if (shrinker->flags & SHRINKER_MEMCG_AWARE)
		idr_replace(&shrinker_idr, shrinker, shrinker->id);
#endif
	up_write(&shrinker_rwsem);
}

int register_shrinker(struct shrinker *shrinker)
{
	int err = prealloc_shrinker(shrinker);

	if (err)
		return err;
	register_shrinker_prepared(shrinker);
	return 0;
}
EXPORT_SYMBOL(register_shrinker);

/*
 * Remove one
 */
void unregister_shrinker(struct shrinker *shrinker)
{
	if (!shrinker->nr_deferred)
		return;
	if (shrinker->flags & SHRINKER_MEMCG_AWARE)
		unregister_memcg_shrinker(shrinker);
	down_write(&shrinker_rwsem);
	list_del(&shrinker->list);
	up_write(&shrinker_rwsem);
	kfree(shrinker->nr_deferred);
	shrinker->nr_deferred = NULL;
}
EXPORT_SYMBOL(unregister_shrinker);

#define SHRINK_BATCH 128

static unsigned long do_shrink_slab(struct shrink_control *shrinkctl,
				    struct shrinker *shrinker, int priority)
{
	unsigned long freed = 0;
	unsigned long long delta;
	long total_scan;
	long freeable;
	long nr;
	long new_nr;
	int nid = shrinkctl->nid;
	long batch_size = shrinker->batch ? shrinker->batch
					  : SHRINK_BATCH;
	long scanned = 0, next_deferred;

	if (!(shrinker->flags & SHRINKER_NUMA_AWARE))
		nid = 0;

	freeable = shrinker->count_objects(shrinker, shrinkctl);
	if (freeable == 0 || freeable == SHRINK_EMPTY)
		return freeable;

	/*
	 * copy the current shrinker scan count into a local variable
	 * and zero it so that other concurrent shrinker invocations
	 * don't also do this scanning work.
	 */
	nr = atomic_long_xchg(&shrinker->nr_deferred[nid], 0);

	total_scan = nr;
	if (shrinker->seeks) {
		delta = freeable >> priority;
		delta *= 4;
		do_div(delta, shrinker->seeks);
	} else {
		/*
		 * These objects don't require any IO to create. Trim
		 * them aggressively under memory pressure to keep
		 * them from causing refetches in the IO caches.
		 */
		delta = freeable / 2;
	}

	total_scan += delta;
	if (total_scan < 0) {
		pr_err("shrink_slab: %pS negative objects to delete nr=%ld\n",
		       shrinker->scan_objects, total_scan);
		total_scan = freeable;
		next_deferred = nr;
	} else
		next_deferred = total_scan;

	/*
	 * We need to avoid excessive windup on filesystem shrinkers
	 * due to large numbers of GFP_NOFS allocations causing the
	 * shrinkers to return -1 all the time. This results in a large
	 * nr being built up so when a shrink that can do some work
	 * comes along it empties the entire cache due to nr >>>
	 * freeable. This is bad for sustaining a working set in
	 * memory.
	 *
	 * Hence only allow the shrinker to scan the entire cache when
	 * a large delta change is calculated directly.
	 */
	if (delta < freeable / 4)
		total_scan = min(total_scan, freeable / 2);

	/*
	 * Avoid risking looping forever due to too large nr value:
	 * never try to free more than twice the estimate number of
	 * freeable entries.
	 */
	if (total_scan > freeable * 2)
		total_scan = freeable * 2;

	trace_mm_shrink_slab_start(shrinker, shrinkctl, nr,
				   freeable, delta, total_scan, priority);

	/*
	 * Normally, we should not scan less than batch_size objects in one
	 * pass to avoid too frequent shrinker calls, but if the slab has less
	 * than batch_size objects in total and we are really tight on memory,
	 * we will try to reclaim all available objects, otherwise we can end
	 * up failing allocations although there are plenty of reclaimable
	 * objects spread over several slabs with usage less than the
	 * batch_size.
	 *
	 * We detect the "tight on memory" situations by looking at the total
	 * number of objects we want to scan (total_scan). If it is greater
	 * than the total number of objects on slab (freeable), we must be
	 * scanning at high prio and therefore should try to reclaim as much as
	 * possible.
	 */
	while (total_scan >= batch_size ||
	       total_scan >= freeable) {
		unsigned long ret;
		unsigned long nr_to_scan = min(batch_size, total_scan);

		shrinkctl->nr_to_scan = nr_to_scan;
		shrinkctl->nr_scanned = nr_to_scan;
		ret = shrinker->scan_objects(shrinker, shrinkctl);
		if (ret == SHRINK_STOP)
			break;
		freed += ret;

		count_vm_events(SLABS_SCANNED, shrinkctl->nr_scanned);
		total_scan -= shrinkctl->nr_scanned;
		scanned += shrinkctl->nr_scanned;

		cond_resched();
	}

	if (next_deferred >= scanned)
		next_deferred -= scanned;
	else
		next_deferred = 0;
	/*
	 * move the unused scan count back into the shrinker in a
	 * manner that handles concurrent updates. If we exhausted the
	 * scan, there is no need to do an update.
	 */
	if (next_deferred > 0)
		new_nr = atomic_long_add_return(next_deferred,
						&shrinker->nr_deferred[nid]);
	else
		new_nr = atomic_long_read(&shrinker->nr_deferred[nid]);

	trace_mm_shrink_slab_end(shrinker, nid, freed, nr, new_nr, total_scan);
	return freed;
}

#ifdef CONFIG_MEMCG
static unsigned long shrink_slab_memcg(gfp_t gfp_mask, int nid,
			struct mem_cgroup *memcg, int priority)
{
	struct memcg_shrinker_map *map;
	unsigned long ret, freed = 0;
	int i;

	if (!mem_cgroup_online(memcg))
		return 0;

	if (!down_read_trylock(&shrinker_rwsem))
		return 0;

	map = rcu_dereference_protected(memcg->nodeinfo[nid]->shrinker_map,
					true);
	if (unlikely(!map))
		goto unlock;

	for_each_set_bit(i, map->map, shrinker_nr_max) {
		struct shrink_control sc = {
			.gfp_mask = gfp_mask,
			.nid = nid,
			.memcg = memcg,
		};
		struct shrinker *shrinker;

		shrinker = idr_find(&shrinker_idr, i);
		if (unlikely(!shrinker || shrinker == SHRINKER_REGISTERING)) {
			if (!shrinker)
				clear_bit(i, map->map);
			continue;
		}

		/* Call non-slab shrinkers even though kmem is disabled */
		if (!memcg_kmem_enabled() &&
		    !(shrinker->flags & SHRINKER_NONSLAB))
			continue;

		ret = do_shrink_slab(&sc, shrinker, priority);
		if (ret == SHRINK_EMPTY) {
			clear_bit(i, map->map);
			/*
			 * After the shrinker reported that it had no objects to
			 * free, but before we cleared the corresponding bit in
			 * the memcg shrinker map, a new object might have been
			 * added. To make sure, we have the bit set in this
			 * case, we invoke the shrinker one more time and reset
			 * the bit if it reports that it is not empty anymore.
			 * The memory barrier here pairs with the barrier in
			 * memcg_set_shrinker_bit():
			 *
			 * list_lru_add()     shrink_slab_memcg()
			 *   list_add_tail()    clear_bit()
			 *   <MB>               <MB>
			 *   set_bit()          do_shrink_slab()
			 */
			smp_mb__after_atomic();
			ret = do_shrink_slab(&sc, shrinker, priority);
			if (ret == SHRINK_EMPTY)
				ret = 0;
			else
				memcg_set_shrinker_bit(memcg, nid, i);
		}
		freed += ret;

		if (rwsem_is_contended(&shrinker_rwsem)) {
			freed = freed ? : 1;
			break;
		}
	}
unlock:
	up_read(&shrinker_rwsem);
	return freed;
}
#else /* CONFIG_MEMCG */
static unsigned long shrink_slab_memcg(gfp_t gfp_mask, int nid,
			struct mem_cgroup *memcg, int priority)
{
	return 0;
}
#endif /* CONFIG_MEMCG */

/**
 * shrink_slab - shrink slab caches
 * @gfp_mask: allocation context
 * @nid: node whose slab caches to target
 * @memcg: memory cgroup whose slab caches to target
 * @priority: the reclaim priority
 *
 * Call the shrink functions to age shrinkable caches.
 *
 * @nid is passed along to shrinkers with SHRINKER_NUMA_AWARE set,
 * unaware shrinkers will receive a node id of 0 instead.
 *
 * @memcg specifies the memory cgroup to target. Unaware shrinkers
 * are called only if it is the root cgroup.
 *
 * @priority is sc->priority, we take the number of objects and >> by priority
 * in order to get the scan target.
 *
 * Returns the number of reclaimed slab objects.
 */
static unsigned long shrink_slab(gfp_t gfp_mask, int nid,
				 struct mem_cgroup *memcg,
				 int priority)
{
	unsigned long ret, freed = 0;
	struct shrinker *shrinker;

	/*
	 * The root memcg might be allocated even though memcg is disabled
	 * via "cgroup_disable=memory" boot parameter.  This could make
	 * mem_cgroup_is_root() return false, then just run memcg slab
	 * shrink, but skip global shrink.  This may result in premature
	 * oom.
	 */
	if (!mem_cgroup_disabled() && !mem_cgroup_is_root(memcg))
		return shrink_slab_memcg(gfp_mask, nid, memcg, priority);

	if (!down_read_trylock(&shrinker_rwsem))
		goto out;

	list_for_each_entry(shrinker, &shrinker_list, list) {
		struct shrink_control sc = {
			.gfp_mask = gfp_mask,
			.nid = nid,
			.memcg = memcg,
		};

		ret = do_shrink_slab(&sc, shrinker, priority);
		if (ret == SHRINK_EMPTY)
			ret = 0;
		freed += ret;
		/*
		 * Bail out if someone want to register a new shrinker to
		 * prevent the regsitration from being stalled for long periods
		 * by parallel ongoing shrinking.
		 */
		if (rwsem_is_contended(&shrinker_rwsem)) {
			freed = freed ? : 1;
			break;
		}
	}

	up_read(&shrinker_rwsem);
out:
	cond_resched();
	return freed;
}

void drop_slab_node(int nid)
{
	unsigned long freed;

	do {
		struct mem_cgroup *memcg = NULL;

		freed = 0;
		memcg = mem_cgroup_iter(NULL, NULL, NULL);
		do {
			freed += shrink_slab(GFP_KERNEL, nid, memcg, 0);
		} while ((memcg = mem_cgroup_iter(NULL, memcg, NULL)) != NULL);
	} while (freed > 10);
}

void drop_slab(void)
{
	int nid;

	for_each_online_node(nid)
		drop_slab_node(nid);
}

static inline int is_page_cache_freeable(struct page *page)
{
	/*
	 * A freeable page cache page is referenced only by the caller
	 * that isolated the page, the page cache and optional buffer
	 * heads at page->private.
	 */
	int page_cache_pins = PageTransHuge(page) && PageSwapCache(page) ?
		HPAGE_PMD_NR : 1;
	return page_count(page) - page_has_private(page) == 1 + page_cache_pins;
}

static int may_write_to_inode(struct inode *inode, struct scan_control *sc)
{
	if (current->flags & PF_SWAPWRITE)
		return 1;
	if (!inode_write_congested(inode))
		return 1;
	if (inode_to_bdi(inode) == current->backing_dev_info)
		return 1;
	return 0;
}

/*
 * We detected a synchronous write error writing a page out.  Probably
 * -ENOSPC.  We need to propagate that into the address_space for a subsequent
 * fsync(), msync() or close().
 *
 * The tricky part is that after writepage we cannot touch the mapping: nothing
 * prevents it from being freed up.  But we have a ref on the page and once
 * that page is locked, the mapping is pinned.
 *
 * We're allowed to run sleeping lock_page() here because we know the caller has
 * __GFP_FS.
 */
static void handle_write_error(struct address_space *mapping,
				struct page *page, int error)
{
	lock_page(page);
	if (page_mapping(page) == mapping)
		mapping_set_error(mapping, error);
	unlock_page(page);
}

/* possible outcome of pageout() */
typedef enum {
	/* failed to write page out, page is locked */
	PAGE_KEEP,
	/* move page to the active list, page is locked */
	PAGE_ACTIVATE,
	/* page has been sent to the disk successfully, page is unlocked */
	PAGE_SUCCESS,
	/* page is clean and locked */
	PAGE_CLEAN,
} pageout_t;

/*
 * pageout is called by shrink_page_list() for each dirty page.
 * Calls ->writepage().
 */
static pageout_t pageout(struct page *page, struct address_space *mapping,
			 struct scan_control *sc)
{
	/*
	 * If the page is dirty, only perform writeback if that write
	 * will be non-blocking.  To prevent this allocation from being
	 * stalled by pagecache activity.  But note that there may be
	 * stalls if we need to run get_block().  We could test
	 * PagePrivate for that.
	 *
	 * If this process is currently in __generic_file_write_iter() against
	 * this page's queue, we can perform writeback even if that
	 * will block.
	 *
	 * If the page is swapcache, write it back even if that would
	 * block, for some throttling. This happens by accident, because
	 * swap_backing_dev_info is bust: it doesn't reflect the
	 * congestion state of the swapdevs.  Easy to fix, if needed.
	 */
	if (!is_page_cache_freeable(page))
		return PAGE_KEEP;
	if (!mapping) {
		/*
		 * Some data journaling orphaned pages can have
		 * page->mapping == NULL while being dirty with clean buffers.
		 */
		if (page_has_private(page)) {
			if (try_to_free_buffers(page)) {
				ClearPageDirty(page);
				pr_info("%s: orphaned page\n", __func__);
				return PAGE_CLEAN;
			}
		}
		return PAGE_KEEP;
	}
	if (mapping->a_ops->writepage == NULL)
		return PAGE_ACTIVATE;
	if (!may_write_to_inode(mapping->host, sc))
		return PAGE_KEEP;

	if (clear_page_dirty_for_io(page)) {
		int res;
		struct writeback_control wbc = {
			.sync_mode = WB_SYNC_NONE,
			.nr_to_write = SWAP_CLUSTER_MAX,
			.range_start = 0,
			.range_end = LLONG_MAX,
			.for_reclaim = 1,
		};

		SetPageReclaim(page);
		res = mapping->a_ops->writepage(page, &wbc);
		if (res < 0)
			handle_write_error(mapping, page, res);
		if (res == AOP_WRITEPAGE_ACTIVATE) {
			ClearPageReclaim(page);
			return PAGE_ACTIVATE;
		}

		if (!PageWriteback(page)) {
			/* synchronous write or broken a_ops? */
			ClearPageReclaim(page);
		}
		trace_mm_vmscan_writepage(page);
		inc_node_page_state(page, NR_VMSCAN_WRITE);
		return PAGE_SUCCESS;
	}

	return PAGE_CLEAN;
}

/*
 * Same as remove_mapping, but if the page is removed from the mapping, it
 * gets returned with a refcount of 0.
 */
static int __remove_mapping(struct address_space *mapping, struct page *page,
			    bool reclaimed)
{
	unsigned long flags;
	int refcount;
	void *shadow = NULL;

	BUG_ON(!PageLocked(page));
	BUG_ON(mapping != page_mapping(page));

	xa_lock_irqsave(&mapping->i_pages, flags);
	/*
	 * The non racy check for a busy page.
	 *
	 * Must be careful with the order of the tests. When someone has
	 * a ref to the page, it may be possible that they dirty it then
	 * drop the reference. So if PageDirty is tested before page_count
	 * here, then the following race may occur:
	 *
	 * get_user_pages(&page);
	 * [user mapping goes away]
	 * write_to(page);
	 *				!PageDirty(page)    [good]
	 * SetPageDirty(page);
	 * put_page(page);
	 *				!page_count(page)   [good, discard it]
	 *
	 * [oops, our write_to data is lost]
	 *
	 * Reversing the order of the tests ensures such a situation cannot
	 * escape unnoticed. The smp_rmb is needed to ensure the page->flags
	 * load is not satisfied before that of page->_refcount.
	 *
	 * Note that if SetPageDirty is always performed via set_page_dirty,
	 * and thus under the i_pages lock, then this ordering is not required.
	 */
	refcount = 1 + compound_nr(page);
	if (!page_ref_freeze(page, refcount))
		goto cannot_free;
	/* note: atomic_cmpxchg in page_ref_freeze provides the smp_rmb */
	if (unlikely(PageDirty(page))) {
		page_ref_unfreeze(page, refcount);
		goto cannot_free;
	}

	if (PageSwapCache(page)) {
		swp_entry_t swap = { .val = page_private(page) };

		if (lru_gen_enabled())
			shadow = lru_gen_eviction(page);
		mem_cgroup_swapout(page, swap);
		__delete_from_swap_cache(page, swap, shadow);
		xa_unlock_irqrestore(&mapping->i_pages, flags);
		put_swap_page(page, swap);
	} else {
		void (*freepage)(struct page *);

		freepage = mapping->a_ops->freepage;
		/*
		 * Remember a shadow entry for reclaimed file cache in
		 * order to detect refaults, thus thrashing, later on.
		 *
		 * But don't store shadows in an address space that is
		 * already exiting.  This is not just an optizimation,
		 * inode reclaim needs to empty out the radix tree or
		 * the nodes are lost.  Don't plant shadows behind its
		 * back.
		 *
		 * We also don't store shadows for DAX mappings because the
		 * only page cache pages found in these are zero pages
		 * covering holes, and because we don't want to mix DAX
		 * exceptional entries and shadow exceptional entries in the
		 * same address_space.
		 */
		if (reclaimed && page_is_file_cache(page) &&
		    !mapping_exiting(mapping) && !dax_mapping(mapping))
			shadow = workingset_eviction(page);
		__delete_from_page_cache(page, shadow);
		xa_unlock_irqrestore(&mapping->i_pages, flags);

		if (freepage != NULL)
			freepage(page);
	}

	return 1;

cannot_free:
	xa_unlock_irqrestore(&mapping->i_pages, flags);
	return 0;
}

/*
 * Attempt to detach a locked page from its ->mapping.  If it is dirty or if
 * someone else has a ref on the page, abort and return 0.  If it was
 * successfully detached, return 1.  Assumes the caller has a single ref on
 * this page.
 */
int remove_mapping(struct address_space *mapping, struct page *page)
{
	if (__remove_mapping(mapping, page, false)) {
		/*
		 * Unfreezing the refcount with 1 rather than 2 effectively
		 * drops the pagecache ref for us without requiring another
		 * atomic operation.
		 */
		page_ref_unfreeze(page, 1);
		return 1;
	}
	return 0;
}

/**
 * putback_lru_page - put previously isolated page onto appropriate LRU list
 * @page: page to be put back to appropriate lru list
 *
 * Add previously isolated @page to appropriate LRU list.
 * Page may still be unevictable for other reasons.
 *
 * lru_lock must not be held, interrupts must be enabled.
 */
void putback_lru_page(struct page *page)
{
	lru_cache_add(page);
	put_page(page);		/* drop ref from isolate */
}

enum page_references {
	PAGEREF_RECLAIM,
	PAGEREF_RECLAIM_CLEAN,
	PAGEREF_KEEP,
	PAGEREF_ACTIVATE,
};

static enum page_references page_check_references(struct page *page,
						  struct scan_control *sc)
{
	int referenced_ptes, referenced_page;
	unsigned long vm_flags;

	referenced_ptes = page_referenced(page, 1, sc->target_mem_cgroup,
					  &vm_flags);
	referenced_page = TestClearPageReferenced(page);

	/*
	 * Mlock lost the isolation race with us.  Let try_to_unmap()
	 * move the page to the unevictable list.
	 */
	if (vm_flags & VM_LOCKED)
		return PAGEREF_RECLAIM;

	if (referenced_ptes) {
		if (PageSwapBacked(page))
			return PAGEREF_ACTIVATE;
		/*
		 * All mapped pages start out with page table
		 * references from the instantiating fault, so we need
		 * to look twice if a mapped file page is used more
		 * than once.
		 *
		 * Mark it and spare it for another trip around the
		 * inactive list.  Another page table reference will
		 * lead to its activation.
		 *
		 * Note: the mark is set for activated pages as well
		 * so that recently deactivated but used pages are
		 * quickly recovered.
		 */
		SetPageReferenced(page);

		if (referenced_page || referenced_ptes > 1)
			return PAGEREF_ACTIVATE;

		/*
		 * Activate file-backed executable pages after first usage.
		 */
		if (vm_flags & VM_EXEC)
			return PAGEREF_ACTIVATE;

		return PAGEREF_KEEP;
	}

	/* Reclaim if clean, defer dirty pages to writeback */
	if (referenced_page && !PageSwapBacked(page))
		return PAGEREF_RECLAIM_CLEAN;

	return PAGEREF_RECLAIM;
}

/* Check if a page is dirty or under writeback */
static void page_check_dirty_writeback(struct page *page,
				       bool *dirty, bool *writeback)
{
	struct address_space *mapping;

	/*
	 * Anonymous pages are not handled by flushers and must be written
	 * from reclaim context. Do not stall reclaim based on them
	 */
	if (!page_is_file_cache(page) ||
	    (PageAnon(page) && !PageSwapBacked(page))) {
		*dirty = false;
		*writeback = false;
		return;
	}

	/* By default assume that the page flags are accurate */
	*dirty = PageDirty(page);
	*writeback = PageWriteback(page);

	/* Verify dirty/writeback state if the filesystem supports it */
	if (!page_has_private(page))
		return;

	mapping = page_mapping(page);
	if (mapping && mapping->a_ops->is_dirty_writeback)
		mapping->a_ops->is_dirty_writeback(page, dirty, writeback);
}

/*
 * shrink_page_list() returns the number of reclaimed pages
 */
static unsigned long shrink_page_list(struct list_head *page_list,
				      struct pglist_data *pgdat,
				      struct scan_control *sc,
				      enum ttu_flags ttu_flags,
				      struct reclaim_stat *stat,
				      bool ignore_references)
{
	LIST_HEAD(ret_pages);
	LIST_HEAD(free_pages);
	unsigned nr_reclaimed = 0;
	unsigned pgactivate = 0;

	memset(stat, 0, sizeof(*stat));
	cond_resched();

	while (!list_empty(page_list)) {
		struct address_space *mapping;
		struct page *page;
		int may_enter_fs;
		enum page_references references = PAGEREF_RECLAIM;
		bool dirty, writeback;
		unsigned int nr_pages;

		cond_resched();

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if (!trylock_page(page))
			goto keep;

		VM_BUG_ON_PAGE(PageActive(page), page);

		nr_pages = compound_nr(page);

		/* Account the number of base pages even though THP */
		sc->nr_scanned += nr_pages;

		if (unlikely(!page_evictable(page)))
			goto activate_locked;

		if (!sc->may_unmap && page_mapped(page))
			goto keep_locked;

		/* in case the page was found accessed by lru_gen_scan_around() */
		if (lru_gen_enabled() && !ignore_references &&
		    page_mapped(page) && PageReferenced(page))
			goto keep_locked;

		may_enter_fs = (sc->gfp_mask & __GFP_FS) ||
			(PageSwapCache(page) && (sc->gfp_mask & __GFP_IO));

		/*
		 * The number of dirty pages determines if a node is marked
		 * reclaim_congested which affects wait_iff_congested. kswapd
		 * will stall and start writing pages if the tail of the LRU
		 * is all dirty unqueued pages.
		 */
		page_check_dirty_writeback(page, &dirty, &writeback);
		if (dirty || writeback)
			stat->nr_dirty++;

		if (dirty && !writeback)
			stat->nr_unqueued_dirty++;

		/*
		 * Treat this page as congested if the underlying BDI is or if
		 * pages are cycling through the LRU so quickly that the
		 * pages marked for immediate reclaim are making it to the
		 * end of the LRU a second time.
		 */
		mapping = page_mapping(page);
		if (((dirty || writeback) && mapping &&
		     inode_write_congested(mapping->host)) ||
		    (writeback && PageReclaim(page)))
			stat->nr_congested++;

		/*
		 * If a page at the tail of the LRU is under writeback, there
		 * are three cases to consider.
		 *
		 * 1) If reclaim is encountering an excessive number of pages
		 *    under writeback and this page is both under writeback and
		 *    PageReclaim then it indicates that pages are being queued
		 *    for IO but are being recycled through the LRU before the
		 *    IO can complete. Waiting on the page itself risks an
		 *    indefinite stall if it is impossible to writeback the
		 *    page due to IO error or disconnected storage so instead
		 *    note that the LRU is being scanned too quickly and the
		 *    caller can stall after page list has been processed.
		 *
		 * 2) Global or new memcg reclaim encounters a page that is
		 *    not marked for immediate reclaim, or the caller does not
		 *    have __GFP_FS (or __GFP_IO if it's simply going to swap,
		 *    not to fs). In this case mark the page for immediate
		 *    reclaim and continue scanning.
		 *
		 *    Require may_enter_fs because we would wait on fs, which
		 *    may not have submitted IO yet. And the loop driver might
		 *    enter reclaim, and deadlock if it waits on a page for
		 *    which it is needed to do the write (loop masks off
		 *    __GFP_IO|__GFP_FS for this reason); but more thought
		 *    would probably show more reasons.
		 *
		 * 3) Legacy memcg encounters a page that is already marked
		 *    PageReclaim. memcg does not have any dirty pages
		 *    throttling so we could easily OOM just because too many
		 *    pages are in writeback and there is nothing else to
		 *    reclaim. Wait for the writeback to complete.
		 *
		 * In cases 1) and 2) we activate the pages to get them out of
		 * the way while we continue scanning for clean pages on the
		 * inactive list and refilling from the active list. The
		 * observation here is that waiting for disk writes is more
		 * expensive than potentially causing reloads down the line.
		 * Since they're marked for immediate reclaim, they won't put
		 * memory pressure on the cache working set any longer than it
		 * takes to write them to disk.
		 */
		if (PageWriteback(page)) {
			/* Case 1 above */
			if (current_is_kswapd() &&
			    PageReclaim(page) &&
			    test_bit(PGDAT_WRITEBACK, &pgdat->flags)) {
				stat->nr_immediate++;
				goto activate_locked;

			/* Case 2 above */
			} else if (sane_reclaim(sc) ||
			    !PageReclaim(page) || !may_enter_fs) {
				/*
				 * This is slightly racy - end_page_writeback()
				 * might have just cleared PageReclaim, then
				 * setting PageReclaim here end up interpreted
				 * as PageReadahead - but that does not matter
				 * enough to care.  What we do want is for this
				 * page to have PageReclaim set next time memcg
				 * reclaim reaches the tests above, so it will
				 * then wait_on_page_writeback() to avoid OOM;
				 * and it's also appropriate in global reclaim.
				 */
				SetPageReclaim(page);
				stat->nr_writeback++;
				goto activate_locked;

			/* Case 3 above */
			} else {
				unlock_page(page);
				wait_on_page_writeback(page);
				/* then go back and try same page again */
				list_add_tail(&page->lru, page_list);
				continue;
			}
		}

		if (!ignore_references)
			references = page_check_references(page, sc);

		switch (references) {
		case PAGEREF_ACTIVATE:
			goto activate_locked;
		case PAGEREF_KEEP:
			stat->nr_ref_keep += nr_pages;
			goto keep_locked;
		case PAGEREF_RECLAIM:
		case PAGEREF_RECLAIM_CLEAN:
			; /* try to reclaim the page below */
		}

		/*
		 * Anonymous process memory has backing store?
		 * Try to allocate it some swap space here.
		 * Lazyfree page could be freed directly
		 */
		if (PageAnon(page) && PageSwapBacked(page)) {
			if (!PageSwapCache(page)) {
				if (!(sc->gfp_mask & __GFP_IO))
					goto keep_locked;
				if (PageTransHuge(page)) {
					/* cannot split THP, skip it */
					if (!can_split_huge_page(page, NULL))
						goto activate_locked;
					/*
					 * Split pages without a PMD map right
					 * away. Chances are some or all of the
					 * tail pages can be freed without IO.
					 */
					if (!compound_mapcount(page) &&
					    split_huge_page_to_list(page,
								    page_list))
						goto activate_locked;
				}
				if (!add_to_swap(page)) {
					if (!PageTransHuge(page))
						goto activate_locked_split;
					/* Fallback to swap normal pages */
					if (split_huge_page_to_list(page,
								    page_list))
						goto activate_locked;
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
					count_vm_event(THP_SWPOUT_FALLBACK);
#endif
					if (!add_to_swap(page))
						goto activate_locked_split;
				}

				may_enter_fs = 1;

				/* Adding to swap updated mapping */
				mapping = page_mapping(page);
			}
		} else if (unlikely(PageTransHuge(page))) {
			/* Split file THP */
			if (split_huge_page_to_list(page, page_list))
				goto keep_locked;
		}

		/*
		 * THP may get split above, need minus tail pages and update
		 * nr_pages to avoid accounting tail pages twice.
		 *
		 * The tail pages that are added into swap cache successfully
		 * reach here.
		 */
		if ((nr_pages > 1) && !PageTransHuge(page)) {
			sc->nr_scanned -= (nr_pages - 1);
			nr_pages = 1;
		}

		/*
		 * The page is mapped into the page tables of one or more
		 * processes. Try to unmap it here.
		 */
		if (page_mapped(page)) {
			enum ttu_flags flags = ttu_flags | TTU_BATCH_FLUSH;

			if (unlikely(PageTransHuge(page)))
				flags |= TTU_SPLIT_HUGE_PMD;
			if (!try_to_unmap(page, flags)) {
				stat->nr_unmap_fail += nr_pages;
				goto activate_locked;
			}
		}

		if (PageDirty(page)) {
			/*
			 * Only kswapd can writeback filesystem pages
			 * to avoid risk of stack overflow. But avoid
			 * injecting inefficient single-page IO into
			 * flusher writeback as much as possible: only
			 * write pages when we've encountered many
			 * dirty pages, and when we've already scanned
			 * the rest of the LRU for clean pages and see
			 * the same dirty pages again (PageReclaim).
			 */
			if (page_is_file_cache(page) &&
			    (!current_is_kswapd() || !PageReclaim(page) ||
			     !test_bit(PGDAT_DIRTY, &pgdat->flags))) {
				/*
				 * Immediately reclaim when written back.
				 * Similar in principal to deactivate_page()
				 * except we already have the page isolated
				 * and know it's dirty
				 */
				inc_node_page_state(page, NR_VMSCAN_IMMEDIATE);
				SetPageReclaim(page);

				goto activate_locked;
			}

			if (references == PAGEREF_RECLAIM_CLEAN)
				goto keep_locked;
			if (!may_enter_fs)
				goto keep_locked;
			if (!sc->may_writepage)
				goto keep_locked;

			/*
			 * Page is dirty. Flush the TLB if a writable entry
			 * potentially exists to avoid CPU writes after IO
			 * starts and then write it out here.
			 */
			try_to_unmap_flush_dirty();
			switch (pageout(page, mapping, sc)) {
			case PAGE_KEEP:
				goto keep_locked;
			case PAGE_ACTIVATE:
				goto activate_locked;
			case PAGE_SUCCESS:
				if (PageWriteback(page))
					goto keep;
				if (PageDirty(page))
					goto keep;

				/*
				 * A synchronous write - probably a ramdisk.  Go
				 * ahead and try to reclaim the page.
				 */
				if (!trylock_page(page))
					goto keep;
				if (PageDirty(page) || PageWriteback(page))
					goto keep_locked;
				mapping = page_mapping(page);
			case PAGE_CLEAN:
				; /* try to free the page below */
			}
		}

		/*
		 * If the page has buffers, try to free the buffer mappings
		 * associated with this page. If we succeed we try to free
		 * the page as well.
		 *
		 * We do this even if the page is PageDirty().
		 * try_to_release_page() does not perform I/O, but it is
		 * possible for a page to have PageDirty set, but it is actually
		 * clean (all its buffers are clean).  This happens if the
		 * buffers were written out directly, with submit_bh(). ext3
		 * will do this, as well as the blockdev mapping.
		 * try_to_release_page() will discover that cleanness and will
		 * drop the buffers and mark the page clean - it can be freed.
		 *
		 * Rarely, pages can have buffers and no ->mapping.  These are
		 * the pages which were not successfully invalidated in
		 * truncate_complete_page().  We try to drop those buffers here
		 * and if that worked, and the page is no longer mapped into
		 * process address space (page_count == 1) it can be freed.
		 * Otherwise, leave the page on the LRU so it is swappable.
		 */
		if (page_has_private(page)) {
			if (!try_to_release_page(page, sc->gfp_mask))
				goto activate_locked;
			if (!mapping && page_count(page) == 1) {
				unlock_page(page);
				if (put_page_testzero(page))
					goto free_it;
				else {
					/*
					 * rare race with speculative reference.
					 * the speculative reference will free
					 * this page shortly, so we may
					 * increment nr_reclaimed here (and
					 * leave it off the LRU).
					 */
					nr_reclaimed++;
					continue;
				}
			}
		}

		if (PageAnon(page) && !PageSwapBacked(page)) {
			/* follow __remove_mapping for reference */
			if (!page_ref_freeze(page, 1))
				goto keep_locked;
			if (PageDirty(page)) {
				page_ref_unfreeze(page, 1);
				goto keep_locked;
			}

			count_vm_event(PGLAZYFREED);
			count_memcg_page_event(page, PGLAZYFREED);
		} else if (!mapping || !__remove_mapping(mapping, page, true))
			goto keep_locked;

		unlock_page(page);
free_it:
		/*
		 * THP may get swapped out in a whole, need account
		 * all base pages.
		 */
		nr_reclaimed += nr_pages;

		/*
		 * Is there need to periodically free_page_list? It would
		 * appear not as the counts should be low
		 */
		if (unlikely(PageTransHuge(page)))
			(*get_compound_page_dtor(page))(page);
		else
			list_add(&page->lru, &free_pages);
		continue;

activate_locked_split:
		/*
		 * The tail pages that are failed to add into swap cache
		 * reach here.  Fixup nr_scanned and nr_pages.
		 */
		if (nr_pages > 1) {
			sc->nr_scanned -= (nr_pages - 1);
			nr_pages = 1;
		}
activate_locked:
		/* Not a candidate for swapping, so reclaim swap space. */
		if (PageSwapCache(page) && (mem_cgroup_swap_full(page) ||
						PageMlocked(page)))
			try_to_free_swap(page);
		VM_BUG_ON_PAGE(PageActive(page), page);
		if (!PageMlocked(page)) {
			int type = page_is_file_cache(page);
			SetPageActive(page);
			stat->nr_activate[type] += nr_pages;
			count_memcg_page_event(page, PGACTIVATE);
		}
keep_locked:
		unlock_page(page);
keep:
		list_add(&page->lru, &ret_pages);
		VM_BUG_ON_PAGE(PageLRU(page) || PageUnevictable(page), page);
	}

	pgactivate = stat->nr_activate[0] + stat->nr_activate[1];

	mem_cgroup_uncharge_list(&free_pages);
	try_to_unmap_flush();
	free_unref_page_list(&free_pages);

	list_splice(&ret_pages, page_list);
	count_vm_events(PGACTIVATE, pgactivate);

	return nr_reclaimed;
}

unsigned long reclaim_clean_pages_from_list(struct zone *zone,
					    struct list_head *page_list)
{
	struct scan_control sc = {
		.gfp_mask = GFP_KERNEL,
		.priority = DEF_PRIORITY,
		.may_unmap = 1,
	};
	struct reclaim_stat dummy_stat;
	unsigned long ret;
	struct page *page, *next;
	LIST_HEAD(clean_pages);

	list_for_each_entry_safe(page, next, page_list, lru) {
		if (page_is_file_cache(page) && !PageDirty(page) &&
		    !__PageMovable(page) && !PageUnevictable(page)) {
			ClearPageActive(page);
			list_move(&page->lru, &clean_pages);
		}
	}

	ret = shrink_page_list(&clean_pages, zone->zone_pgdat, &sc,
			TTU_IGNORE_ACCESS, &dummy_stat, true);
	list_splice(&clean_pages, page_list);
	mod_node_page_state(zone->zone_pgdat, NR_ISOLATED_FILE, -ret);
	return ret;
}

/*
 * Attempt to remove the specified page from its LRU.  Only take this page
 * if it is of the appropriate PageActive status.  Pages which are being
 * freed elsewhere are also ignored.
 *
 * page:	page to consider
 * mode:	one of the LRU isolation modes defined above
 *
 * returns 0 on success, -ve errno on failure.
 */
int __isolate_lru_page(struct page *page, isolate_mode_t mode)
{
	int ret = -EINVAL;

	/* Only take pages on the LRU. */
	if (!PageLRU(page))
		return ret;

	/* Compaction should not handle unevictable pages but CMA can do so */
	if (PageUnevictable(page) && !(mode & ISOLATE_UNEVICTABLE))
		return ret;

	ret = -EBUSY;

	/*
	 * To minimise LRU disruption, the caller can indicate that it only
	 * wants to isolate pages it will be able to operate on without
	 * blocking - clean pages for the most part.
	 *
	 * ISOLATE_ASYNC_MIGRATE is used to indicate that it only wants to pages
	 * that it is possible to migrate without blocking
	 */
	if (mode & ISOLATE_ASYNC_MIGRATE) {
		/* All the caller can do on PageWriteback is block */
		if (PageWriteback(page))
			return ret;

		if (PageDirty(page)) {
			struct address_space *mapping;
			bool migrate_dirty;

			/*
			 * Only pages without mappings or that have a
			 * ->migratepage callback are possible to migrate
			 * without blocking. However, we can be racing with
			 * truncation so it's necessary to lock the page
			 * to stabilise the mapping as truncation holds
			 * the page lock until after the page is removed
			 * from the page cache.
			 */
			if (!trylock_page(page))
				return ret;

			mapping = page_mapping(page);
			migrate_dirty = !mapping || mapping->a_ops->migratepage;
			unlock_page(page);
			if (!migrate_dirty)
				return ret;
		}
	}

	if ((mode & ISOLATE_UNMAPPED) && page_mapped(page))
		return ret;

	if (likely(get_page_unless_zero(page))) {
		/*
		 * Be careful not to clear PageLRU until after we're
		 * sure the page is not being freed elsewhere -- the
		 * page release code relies on it.
		 */
		ClearPageLRU(page);
		ret = 0;
	}

	return ret;
}


/*
 * Update LRU sizes after isolating pages. The LRU size updates must
 * be complete before mem_cgroup_update_lru_size due to a santity check.
 */
static __always_inline void update_lru_sizes(struct lruvec *lruvec,
			enum lru_list lru, unsigned long *nr_zone_taken)
{
	int zid;

	for (zid = 0; zid < MAX_NR_ZONES; zid++) {
		if (!nr_zone_taken[zid])
			continue;

		update_lru_size(lruvec, lru, zid, -nr_zone_taken[zid]);
	}

}

/**
 * pgdat->lru_lock is heavily contended.  Some of the functions that
 * shrink the lists perform better by taking out a batch of pages
 * and working on them outside the LRU lock.
 *
 * For pagecache intensive workloads, this function is the hottest
 * spot in the kernel (apart from copy_*_user functions).
 *
 * Appropriate locks must be held before calling this function.
 *
 * @nr_to_scan:	The number of eligible pages to look through on the list.
 * @lruvec:	The LRU vector to pull pages from.
 * @dst:	The temp list to put pages on to.
 * @nr_scanned:	The number of pages that were scanned.
 * @sc:		The scan_control struct for this reclaim session
 * @mode:	One of the LRU isolation modes
 * @lru:	LRU list id for isolating
 *
 * returns how many pages were moved onto *@dst.
 */
static unsigned long isolate_lru_pages(unsigned long nr_to_scan,
		struct lruvec *lruvec, struct list_head *dst,
		unsigned long *nr_scanned, struct scan_control *sc,
		enum lru_list lru)
{
	struct list_head *src = &lruvec->lists[lru];
	unsigned long nr_taken = 0;
	unsigned long nr_zone_taken[MAX_NR_ZONES] = { 0 };
	unsigned long nr_skipped[MAX_NR_ZONES] = { 0, };
	unsigned long skipped = 0;
	unsigned long scan, total_scan, nr_pages;
	LIST_HEAD(pages_skipped);
	isolate_mode_t mode = (sc->may_unmap ? 0 : ISOLATE_UNMAPPED);

	total_scan = 0;
	scan = 0;
	while (scan < nr_to_scan && !list_empty(src)) {
		struct page *page;

		page = lru_to_page(src);
		prefetchw_prev_lru_page(page, src, flags);

		VM_BUG_ON_PAGE(!PageLRU(page), page);

		nr_pages = compound_nr(page);
		total_scan += nr_pages;

		if (page_zonenum(page) > sc->reclaim_idx) {
			list_move(&page->lru, &pages_skipped);
			nr_skipped[page_zonenum(page)] += nr_pages;
			continue;
		}

		/*
		 * Do not count skipped pages because that makes the function
		 * return with no isolated pages if the LRU mostly contains
		 * ineligible pages.  This causes the VM to not reclaim any
		 * pages, triggering a premature OOM.
		 *
		 * Account all tail pages of THP.  This would not cause
		 * premature OOM since __isolate_lru_page() returns -EBUSY
		 * only when the page is being freed somewhere else.
		 */
		scan += nr_pages;
		switch (__isolate_lru_page(page, mode)) {
		case 0:
			nr_taken += nr_pages;
			nr_zone_taken[page_zonenum(page)] += nr_pages;
			list_move(&page->lru, dst);
			break;

		case -EBUSY:
			/* else it is being freed elsewhere */
			list_move(&page->lru, src);
			continue;

		default:
			BUG();
		}
	}

	/*
	 * Splice any skipped pages to the start of the LRU list. Note that
	 * this disrupts the LRU order when reclaiming for lower zones but
	 * we cannot splice to the tail. If we did then the SWAP_CLUSTER_MAX
	 * scanning would soon rescan the same pages to skip and put the
	 * system at risk of premature OOM.
	 */
	if (!list_empty(&pages_skipped)) {
		int zid;

		list_splice(&pages_skipped, src);
		for (zid = 0; zid < MAX_NR_ZONES; zid++) {
			if (!nr_skipped[zid])
				continue;

			__count_zid_vm_events(PGSCAN_SKIP, zid, nr_skipped[zid]);
			skipped += nr_skipped[zid];
		}
	}
	*nr_scanned = total_scan;
	trace_mm_vmscan_lru_isolate(sc->reclaim_idx, sc->order, nr_to_scan,
				    total_scan, skipped, nr_taken, mode, lru);
	update_lru_sizes(lruvec, lru, nr_zone_taken);
	return nr_taken;
}

/**
 * isolate_lru_page - tries to isolate a page from its LRU list
 * @page: page to isolate from its LRU list
 *
 * Isolates a @page from an LRU list, clears PageLRU and adjusts the
 * vmstat statistic corresponding to whatever LRU list the page was on.
 *
 * Returns 0 if the page was removed from an LRU list.
 * Returns -EBUSY if the page was not on an LRU list.
 *
 * The returned page will have PageLRU() cleared.  If it was found on
 * the active list, it will have PageActive set.  If it was found on
 * the unevictable list, it will have the PageUnevictable bit set. That flag
 * may need to be cleared by the caller before letting the page go.
 *
 * The vmstat statistic corresponding to the list on which the page was
 * found will be decremented.
 *
 * Restrictions:
 *
 * (1) Must be called with an elevated refcount on the page. This is a
 *     fundamentnal difference from isolate_lru_pages (which is called
 *     without a stable reference).
 * (2) the lru_lock must not be held.
 * (3) interrupts must be enabled.
 */
int isolate_lru_page(struct page *page)
{
	int ret = -EBUSY;

	VM_BUG_ON_PAGE(!page_count(page), page);
	WARN_RATELIMIT(PageTail(page), "trying to isolate tail page");

	if (PageLRU(page)) {
		pg_data_t *pgdat = page_pgdat(page);
		struct lruvec *lruvec;

		spin_lock_irq(&pgdat->lru_lock);
		lruvec = mem_cgroup_page_lruvec(page, pgdat);
		if (PageLRU(page)) {
			get_page(page);
			ClearPageLRU(page);
			del_page_from_lru_list(page, lruvec);
			ret = 0;
		}
		spin_unlock_irq(&pgdat->lru_lock);
	}
	return ret;
}

/*
 * A direct reclaimer may isolate SWAP_CLUSTER_MAX pages from the LRU list and
 * then get resheduled. When there are massive number of tasks doing page
 * allocation, such sleeping direct reclaimers may keep piling up on each CPU,
 * the LRU list will go small and be scanned faster than necessary, leading to
 * unnecessary swapping, thrashing and OOM.
 */
static int too_many_isolated(struct pglist_data *pgdat, int file,
		struct scan_control *sc)
{
	unsigned long inactive, isolated;

	if (current_is_kswapd())
		return 0;

	if (!sane_reclaim(sc))
		return 0;

	if (file) {
		inactive = node_page_state(pgdat, NR_INACTIVE_FILE);
		isolated = node_page_state(pgdat, NR_ISOLATED_FILE);
	} else {
		inactive = node_page_state(pgdat, NR_INACTIVE_ANON);
		isolated = node_page_state(pgdat, NR_ISOLATED_ANON);
	}

	/*
	 * GFP_NOIO/GFP_NOFS callers are allowed to isolate more pages, so they
	 * won't get blocked by normal direct-reclaimers, forming a circular
	 * deadlock.
	 */
	if ((sc->gfp_mask & (__GFP_IO | __GFP_FS)) == (__GFP_IO | __GFP_FS))
		inactive >>= 3;

	return isolated > inactive;
}

/*
 * This moves pages from @list to corresponding LRU list.
 *
 * We move them the other way if the page is referenced by one or more
 * processes, from rmap.
 *
 * If the pages are mostly unmapped, the processing is fast and it is
 * appropriate to hold zone_lru_lock across the whole operation.  But if
 * the pages are mapped, the processing is slow (page_referenced()) so we
 * should drop zone_lru_lock around each page.  It's impossible to balance
 * this, so instead we remove the pages from the LRU while processing them.
 * It is safe to rely on PG_active against the non-LRU pages in here because
 * nobody will play with that bit on a non-LRU page.
 *
 * The downside is that we have to touch page->_refcount against each page.
 * But we had to alter page->flags anyway.
 *
 * Returns the number of pages moved to the given lruvec.
 */

static unsigned noinline_for_stack move_pages_to_lru(struct lruvec *lruvec,
						     struct list_head *list)
{
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	int nr_moved = 0;
	LIST_HEAD(pages_to_free);
	struct page *page;

	while (!list_empty(list)) {
		page = lru_to_page(list);
		VM_BUG_ON_PAGE(PageLRU(page), page);
		list_del(&page->lru);
		if (unlikely(!page_evictable(page))) {
			spin_unlock_irq(&pgdat->lru_lock);
			putback_lru_page(page);
			spin_lock_irq(&pgdat->lru_lock);
			continue;
		}
		lruvec = mem_cgroup_page_lruvec(page, pgdat);

		SetPageLRU(page);
		add_page_to_lru_list(page, lruvec);

		if (put_page_testzero(page)) {
			del_page_from_lru_list(page, lruvec);
			__clear_page_lru_flags(page);

			if (unlikely(PageCompound(page))) {
				spin_unlock_irq(&pgdat->lru_lock);
				(*get_compound_page_dtor(page))(page);
				spin_lock_irq(&pgdat->lru_lock);
			} else
				list_add(&page->lru, &pages_to_free);
		} else {
			nr_moved += hpage_nr_pages(page);
		}
	}

	/*
	 * To save our caller's stack, now use input list for pages to free.
	 */
	list_splice(&pages_to_free, list);

	return nr_moved;
}

/*
 * If a kernel thread (such as nfsd for loop-back mounts) services
 * a backing device by writing to the page cache it sets PF_LESS_THROTTLE.
 * In that case we should only throttle if the backing device it is
 * writing to is congested.  In other cases it is safe to throttle.
 */
static int current_may_throttle(void)
{
	return !(current->flags & PF_LESS_THROTTLE) ||
		current->backing_dev_info == NULL ||
		bdi_write_congested(current->backing_dev_info);
}

/*
 * shrink_inactive_list() is a helper for shrink_node().  It returns the number
 * of reclaimed pages
 */
static noinline_for_stack unsigned long
shrink_inactive_list(unsigned long nr_to_scan, struct lruvec *lruvec,
		     struct scan_control *sc, enum lru_list lru)
{
	LIST_HEAD(page_list);
	unsigned long nr_scanned;
	unsigned long nr_reclaimed = 0;
	unsigned long nr_taken;
	struct reclaim_stat stat;
	int file = is_file_lru(lru);
	enum vm_event_item item;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	struct zone_reclaim_stat *reclaim_stat = &lruvec->reclaim_stat;
	bool stalled = false;

	while (unlikely(too_many_isolated(pgdat, file, sc))) {
		if (stalled)
			return 0;

		/* wait a bit for the reclaimer. */
		msleep(100);
		stalled = true;

		/* We are about to die and free our memory. Return now. */
		if (fatal_signal_pending(current))
			return SWAP_CLUSTER_MAX;
	}

	lru_add_drain();

	spin_lock_irq(&pgdat->lru_lock);

	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, &page_list,
				     &nr_scanned, sc, lru);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, nr_taken);
	reclaim_stat->recent_scanned[file] += nr_taken;

	item = current_is_kswapd() ? PGSCAN_KSWAPD : PGSCAN_DIRECT;
	if (global_reclaim(sc))
		__count_vm_events(item, nr_scanned);
	__count_memcg_events(lruvec_memcg(lruvec), item, nr_scanned);
	spin_unlock_irq(&pgdat->lru_lock);

	if (nr_taken == 0)
		return 0;

	nr_reclaimed = shrink_page_list(&page_list, pgdat, sc, 0,
				&stat, false);

	spin_lock_irq(&pgdat->lru_lock);

	item = current_is_kswapd() ? PGSTEAL_KSWAPD : PGSTEAL_DIRECT;
	if (global_reclaim(sc))
		__count_vm_events(item, nr_reclaimed);
	__count_memcg_events(lruvec_memcg(lruvec), item, nr_reclaimed);
	reclaim_stat->recent_rotated[0] += stat.nr_activate[0];
	reclaim_stat->recent_rotated[1] += stat.nr_activate[1];

	move_pages_to_lru(lruvec, &page_list);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);

	spin_unlock_irq(&pgdat->lru_lock);

	mem_cgroup_uncharge_list(&page_list);
	free_unref_page_list(&page_list);

	/*
	 * If dirty pages are scanned that are not queued for IO, it
	 * implies that flushers are not doing their job. This can
	 * happen when memory pressure pushes dirty pages to the end of
	 * the LRU before the dirty limits are breached and the dirty
	 * data has expired. It can also happen when the proportion of
	 * dirty pages grows not through writes but through memory
	 * pressure reclaiming all the clean cache. And in some cases,
	 * the flushers simply cannot keep up with the allocation
	 * rate. Nudge the flusher threads in case they are asleep.
	 */
	if (stat.nr_unqueued_dirty == nr_taken)
		wakeup_flusher_threads(WB_REASON_VMSCAN);

	sc->nr.dirty += stat.nr_dirty;
	sc->nr.congested += stat.nr_congested;
	sc->nr.unqueued_dirty += stat.nr_unqueued_dirty;
	sc->nr.writeback += stat.nr_writeback;
	sc->nr.immediate += stat.nr_immediate;
	sc->nr.taken += nr_taken;
	if (file)
		sc->nr.file_taken += nr_taken;

	trace_mm_vmscan_lru_shrink_inactive(pgdat->node_id,
			nr_scanned, nr_reclaimed, &stat, sc->priority, file);
	return nr_reclaimed;
}

static void shrink_active_list(unsigned long nr_to_scan,
			       struct lruvec *lruvec,
			       struct scan_control *sc,
			       enum lru_list lru)
{
	unsigned long nr_taken;
	unsigned long nr_scanned;
	unsigned long vm_flags;
	LIST_HEAD(l_hold);	/* The pages which were snipped off */
	LIST_HEAD(l_active);
	LIST_HEAD(l_inactive);
	struct page *page;
	struct zone_reclaim_stat *reclaim_stat = &lruvec->reclaim_stat;
	unsigned nr_deactivate, nr_activate;
	unsigned nr_rotated = 0;
	int file = is_file_lru(lru);
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	lru_add_drain();

	spin_lock_irq(&pgdat->lru_lock);

	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, &l_hold,
				     &nr_scanned, sc, lru);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, nr_taken);
	reclaim_stat->recent_scanned[file] += nr_taken;

	__count_vm_events(PGREFILL, nr_scanned);
	__count_memcg_events(lruvec_memcg(lruvec), PGREFILL, nr_scanned);

	spin_unlock_irq(&pgdat->lru_lock);

	while (!list_empty(&l_hold)) {
		cond_resched();
		page = lru_to_page(&l_hold);
		list_del(&page->lru);

		if (unlikely(!page_evictable(page))) {
			putback_lru_page(page);
			continue;
		}

		if (unlikely(buffer_heads_over_limit)) {
			if (page_has_private(page) && trylock_page(page)) {
				if (page_has_private(page))
					try_to_release_page(page, 0);
				unlock_page(page);
			}
		}

		if (page_referenced(page, 0, sc->target_mem_cgroup,
				    &vm_flags)) {
			nr_rotated += hpage_nr_pages(page);
			/*
			 * Identify referenced, file-backed active pages and
			 * give them one more trip around the active list. So
			 * that executable code get better chances to stay in
			 * memory under moderate memory pressure.  Anon pages
			 * are not likely to be evicted by use-once streaming
			 * IO, plus JVM can create lots of anon VM_EXEC pages,
			 * so we ignore them here.
			 */
			if ((vm_flags & VM_EXEC) && page_is_file_cache(page)) {
				list_add(&page->lru, &l_active);
				continue;
			}
		}

		ClearPageActive(page);	/* we are de-activating */
		SetPageWorkingset(page);
		list_add(&page->lru, &l_inactive);
	}

	/*
	 * Move pages back to the lru list.
	 */
	spin_lock_irq(&pgdat->lru_lock);
	/*
	 * Count referenced pages from currently used mappings as rotated,
	 * even though only some of them are actually re-activated.  This
	 * helps balance scan pressure between file and anonymous pages in
	 * get_scan_count.
	 */
	reclaim_stat->recent_rotated[file] += nr_rotated;

	nr_activate = move_pages_to_lru(lruvec, &l_active);
	nr_deactivate = move_pages_to_lru(lruvec, &l_inactive);
	/* Keep all free pages in l_active list */
	list_splice(&l_inactive, &l_active);

	__count_vm_events(PGDEACTIVATE, nr_deactivate);
	__count_memcg_events(lruvec_memcg(lruvec), PGDEACTIVATE, nr_deactivate);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
	spin_unlock_irq(&pgdat->lru_lock);

	mem_cgroup_uncharge_list(&l_active);
	free_unref_page_list(&l_active);
	trace_mm_vmscan_lru_shrink_active(pgdat->node_id, nr_taken, nr_activate,
			nr_deactivate, nr_rotated, sc->priority, file);
}

unsigned long reclaim_pages(struct list_head *page_list)
{
	int nid = -1;
	unsigned long nr_reclaimed = 0;
	LIST_HEAD(node_page_list);
	struct reclaim_stat dummy_stat;
	struct page *page;
	struct scan_control sc = {
		.gfp_mask = GFP_KERNEL,
		.priority = DEF_PRIORITY,
		.may_writepage = 1,
		.may_unmap = 1,
		.may_swap = 1,
	};

	while (!list_empty(page_list)) {
		page = lru_to_page(page_list);
		if (nid == -1) {
			nid = page_to_nid(page);
			INIT_LIST_HEAD(&node_page_list);
		}

		if (nid == page_to_nid(page)) {
			ClearPageActive(page);
			list_move(&page->lru, &node_page_list);
			continue;
		}

		nr_reclaimed += shrink_page_list(&node_page_list,
						NODE_DATA(nid),
						&sc, 0,
						&dummy_stat, false);
		while (!list_empty(&node_page_list)) {
			page = lru_to_page(&node_page_list);
			list_del(&page->lru);
			putback_lru_page(page);
		}

		nid = -1;
	}

	if (!list_empty(&node_page_list)) {
		nr_reclaimed += shrink_page_list(&node_page_list,
						NODE_DATA(nid),
						&sc, 0,
						&dummy_stat, false);
		while (!list_empty(&node_page_list)) {
			page = lru_to_page(&node_page_list);
			list_del(&page->lru);
			putback_lru_page(page);
		}
	}

	return nr_reclaimed;
}

/*
 * The inactive anon list should be small enough that the VM never has
 * to do too much work.
 *
 * The inactive file list should be small enough to leave most memory
 * to the established workingset on the scan-resistant active list,
 * but large enough to avoid thrashing the aggregate readahead window.
 *
 * Both inactive lists should also be large enough that each inactive
 * page has a chance to be referenced again before it is reclaimed.
 *
 * If that fails and refaulting is observed, the inactive list grows.
 *
 * The inactive_ratio is the target ratio of ACTIVE to INACTIVE pages
 * on this LRU, maintained by the pageout code. An inactive_ratio
 * of 3 means 3:1 or 25% of the pages are kept on the inactive list.
 *
 * total     target    max
 * memory    ratio     inactive
 * -------------------------------------
 *   10MB       1         5MB
 *  100MB       1        50MB
 *    1GB       3       250MB
 *   10GB      10       0.9GB
 *  100GB      31         3GB
 *    1TB     101        10GB
 *   10TB     320        32GB
 */
static bool inactive_list_is_low(struct lruvec *lruvec, bool file,
				 struct scan_control *sc, bool trace)
{
	enum lru_list active_lru = file * LRU_FILE + LRU_ACTIVE;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	enum lru_list inactive_lru = file * LRU_FILE;
	unsigned long inactive, active;
	unsigned long inactive_ratio;
	unsigned long refaults;
	unsigned long gb;

	/*
	 * If we don't have swap space, anonymous page deactivation
	 * is pointless.
	 */
	if (!file && !total_swap_pages)
		return false;

	inactive = lruvec_lru_size(lruvec, inactive_lru, sc->reclaim_idx);
	active = lruvec_lru_size(lruvec, active_lru, sc->reclaim_idx);

	/*
	 * When refaults are being observed, it means a new workingset
	 * is being established. Disable active list protection to get
	 * rid of the stale workingset quickly.
	 */
	refaults = lruvec_page_state_local(lruvec, WORKINGSET_ACTIVATE);
	if (file && lruvec->refaults != refaults) {
		inactive_ratio = 0;
	} else {
		gb = (inactive + active) >> (30 - PAGE_SHIFT);
		if (gb)
			inactive_ratio = int_sqrt(10 * gb);
		else
			inactive_ratio = 1;
	}

	if (trace)
		trace_mm_vmscan_inactive_list_is_low(pgdat->node_id, sc->reclaim_idx,
			lruvec_lru_size(lruvec, inactive_lru, MAX_NR_ZONES), inactive,
			lruvec_lru_size(lruvec, active_lru, MAX_NR_ZONES), active,
			inactive_ratio, file);

	return inactive * inactive_ratio < active;
}

static unsigned long shrink_list(enum lru_list lru, unsigned long nr_to_scan,
				 struct lruvec *lruvec, struct scan_control *sc)
{
	if (is_active_lru(lru)) {
		if (inactive_list_is_low(lruvec, is_file_lru(lru), sc, true))
			shrink_active_list(nr_to_scan, lruvec, sc, lru);
		return 0;
	}

	return shrink_inactive_list(nr_to_scan, lruvec, sc, lru);
}

enum scan_balance {
	SCAN_EQUAL,
	SCAN_FRACT,
	SCAN_ANON,
	SCAN_FILE,
};

/*
 * Low watermark used to prevent fscache thrashing during low memory.
 */
int min_filelist_kbytes;

/*
 * Check low watermark used to prevent fscache thrashing during low memory.
 */
static int file_is_low(struct lruvec *lruvec)
{
	unsigned long size;

	if (!mem_cgroup_disabled())
		return false;

	size = node_page_state(lruvec_pgdat(lruvec), NR_ACTIVE_FILE);
	size += node_page_state(lruvec_pgdat(lruvec), NR_INACTIVE_FILE);
	size <<= (PAGE_SHIFT - 10);

	return size < min_filelist_kbytes;
}

/*
 * Determine how aggressively the anon and file LRU lists should be
 * scanned.  The relative value of each set of LRU lists is determined
 * by looking at the fraction of the pages scanned we did rotate back
 * onto the active list instead of evict.
 *
 * nr[0] = anon inactive pages to scan; nr[1] = anon active pages to scan
 * nr[2] = file inactive pages to scan; nr[3] = file active pages to scan
 */
static void get_scan_count(struct lruvec *lruvec, struct mem_cgroup *memcg,
			   struct scan_control *sc, unsigned long *nr,
			   unsigned long *lru_pages)
{
	int swappiness = mem_cgroup_swappiness(memcg);
	struct zone_reclaim_stat *reclaim_stat = &lruvec->reclaim_stat;
	u64 fraction[ANON_AND_FILE];
	u64 denominator = 0;	/* gcc */
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	unsigned long anon_prio, file_prio;
	enum scan_balance scan_balance;
	unsigned long anon, file;
	unsigned long ap, fp;
	enum lru_list lru;

	/* If we have no swap space, do not bother scanning anon pages. */
	if (!sc->may_swap || mem_cgroup_get_nr_swap_pages(memcg) <= 0) {
		scan_balance = SCAN_FILE;
		goto out;
	}

	/*
	 * Global reclaim will swap to prevent OOM even with no
	 * swappiness, but memcg users want to use this knob to
	 * disable swapping for individual groups completely when
	 * using the memory controller's swap limit feature would be
	 * too expensive.
	 */
	if (!global_reclaim(sc) && !swappiness) {
		scan_balance = SCAN_FILE;
		goto out;
	}

	/*
	 * Do not scan file pages when swap is allowed by __GFP_IO and
	 * file page count is low.
	 */
	if ((sc->gfp_mask & __GFP_IO) && file_is_low(lruvec)) {
		scan_balance = SCAN_ANON;
		goto out;
	}

	/*
	 * Do not apply any pressure balancing cleverness when the
	 * system is close to OOM, scan both anon and file equally
	 * (unless the swappiness setting disagrees with swapping).
	 */
	if (!sc->priority && swappiness) {
		scan_balance = SCAN_EQUAL;
		goto out;
	}

	/*
	 * Prevent the reclaimer from falling into the cache trap: as
	 * cache pages start out inactive, every cache fault will tip
	 * the scan balance towards the file LRU.  And as the file LRU
	 * shrinks, so does the window for rotation from references.
	 * This means we have a runaway feedback loop where a tiny
	 * thrashing file LRU becomes infinitely more attractive than
	 * anon pages.  Try to detect this based on file LRU size.
	 */
	if (global_reclaim(sc)) {
		unsigned long pgdatfile;
		unsigned long pgdatfree;
		int z;
		unsigned long total_high_wmark = 0;

		pgdatfree = sum_zone_node_page_state(pgdat->node_id, NR_FREE_PAGES);
		pgdatfile = node_page_state(pgdat, NR_ACTIVE_FILE) +
			   node_page_state(pgdat, NR_INACTIVE_FILE);

		for (z = 0; z < MAX_NR_ZONES; z++) {
			struct zone *zone = &pgdat->node_zones[z];
			if (!managed_zone(zone))
				continue;

			total_high_wmark += high_wmark_pages(zone);
		}

		if (unlikely(pgdatfile + pgdatfree <= total_high_wmark)) {
			/*
			 * Force SCAN_ANON if there are enough inactive
			 * anonymous pages on the LRU in eligible zones.
			 * Otherwise, the small LRU gets thrashed.
			 */
			if (!inactive_list_is_low(lruvec, false, sc, false) &&
			    lruvec_lru_size(lruvec, LRU_INACTIVE_ANON, sc->reclaim_idx)
					>> sc->priority) {
				scan_balance = SCAN_ANON;
				goto out;
			}
		}
	}

	/*
	 * If there is enough inactive page cache, i.e. if the size of the
	 * inactive list is greater than that of the active list *and* the
	 * inactive list actually has some pages to scan on this priority, we
	 * do not reclaim anything from the anonymous working set right now.
	 * Without the second condition we could end up never scanning an
	 * lruvec even if it has plenty of old anonymous pages unless the
	 * system is under heavy pressure.
	 */
	if (!inactive_list_is_low(lruvec, true, sc, false) &&
	    lruvec_lru_size(lruvec, LRU_INACTIVE_FILE, sc->reclaim_idx) >> sc->priority) {
		scan_balance = SCAN_FILE;
		goto out;
	}

	scan_balance = SCAN_FRACT;

	/*
	 * With swappiness at 100, anonymous and file have the same priority.
	 * This scanning priority is essentially the inverse of IO cost.
	 */
	anon_prio = swappiness;
	file_prio = 200 - anon_prio;

	/*
	 * OK, so we have swap space and a fair amount of page cache
	 * pages.  We use the recently rotated / recently scanned
	 * ratios to determine how valuable each cache is.
	 *
	 * Because workloads change over time (and to avoid overflow)
	 * we keep these statistics as a floating average, which ends
	 * up weighing recent references more than old ones.
	 */

	anon  = lruvec_lru_size(lruvec, LRU_ACTIVE_ANON, MAX_NR_ZONES) +
		lruvec_lru_size(lruvec, LRU_INACTIVE_ANON, MAX_NR_ZONES);
	file  = lruvec_lru_size(lruvec, LRU_ACTIVE_FILE, MAX_NR_ZONES) +
		lruvec_lru_size(lruvec, LRU_INACTIVE_FILE, MAX_NR_ZONES);

	spin_lock_irq(&pgdat->lru_lock);
	if (unlikely(reclaim_stat->recent_scanned[0] > anon / 4)) {
		reclaim_stat->recent_scanned[0] /= 2;
		reclaim_stat->recent_rotated[0] /= 2;
	}

	if (unlikely(reclaim_stat->recent_scanned[1] > file / 4)) {
		reclaim_stat->recent_scanned[1] /= 2;
		reclaim_stat->recent_rotated[1] /= 2;
	}

	/*
	 * The amount of pressure on anon vs file pages is inversely
	 * proportional to the fraction of recently scanned pages on
	 * each list that were recently referenced and in active use.
	 */
	ap = anon_prio * (reclaim_stat->recent_scanned[0] + 1);
	ap /= reclaim_stat->recent_rotated[0] + 1;

	fp = file_prio * (reclaim_stat->recent_scanned[1] + 1);
	fp /= reclaim_stat->recent_rotated[1] + 1;
	spin_unlock_irq(&pgdat->lru_lock);

	fraction[0] = ap;
	fraction[1] = fp;
	denominator = ap + fp + 1;
out:
	*lru_pages = 0;
	for_each_evictable_lru(lru) {
		int file = is_file_lru(lru);
		unsigned long lruvec_size;
		unsigned long low, min;
		unsigned long scan;

		lruvec_size = lruvec_lru_size(lruvec, lru, sc->reclaim_idx);
		mem_cgroup_protection(sc->target_mem_cgroup, memcg,
				      &min, &low);

		if (min || low) {
			/*
			 * Scale a cgroup's reclaim pressure by proportioning
			 * its current usage to its memory.low or memory.min
			 * setting.
			 *
			 * This is important, as otherwise scanning aggression
			 * becomes extremely binary -- from nothing as we
			 * approach the memory protection threshold, to totally
			 * nominal as we exceed it.  This results in requiring
			 * setting extremely liberal protection thresholds. It
			 * also means we simply get no protection at all if we
			 * set it too low, which is not ideal.
			 *
			 * If there is any protection in place, we reduce scan
			 * pressure by how much of the total memory used is
			 * within protection thresholds.
			 *
			 * There is one special case: in the first reclaim pass,
			 * we skip over all groups that are within their low
			 * protection. If that fails to reclaim enough pages to
			 * satisfy the reclaim goal, we come back and override
			 * the best-effort low protection. However, we still
			 * ideally want to honor how well-behaved groups are in
			 * that case instead of simply punishing them all
			 * equally. As such, we reclaim them based on how much
			 * memory they are using, reducing the scan pressure
			 * again by how much of the total memory used is under
			 * hard protection.
			 */
			unsigned long cgroup_size = mem_cgroup_size(memcg);
			unsigned long protection;

			/* memory.low scaling, make sure we retry before OOM */
			if (!sc->memcg_low_reclaim && low > min) {
				protection = low;
				sc->memcg_low_skipped = 1;
			} else {
				protection = min;
			}

			/* Avoid TOCTOU with earlier protection check */
			cgroup_size = max(cgroup_size, protection);

			scan = lruvec_size - lruvec_size * protection /
				(cgroup_size + 1);

			/*
			 * Minimally target SWAP_CLUSTER_MAX pages to keep
			 * reclaim moving forwards, avoiding decremeting
			 * sc->priority further than desirable.
			 */
			scan = max(scan, SWAP_CLUSTER_MAX);
		} else {
			scan = lruvec_size;
		}

		scan >>= sc->priority;

		/*
		 * If the cgroup's already been deleted, make sure to
		 * scrape out the remaining cache.
		 */
		if (!scan && !mem_cgroup_online(memcg))
			scan = min(lruvec_size, SWAP_CLUSTER_MAX);

		switch (scan_balance) {
		case SCAN_EQUAL:
			/* Scan lists relative to size */
			break;
		case SCAN_FRACT:
			/*
			 * Scan types proportional to swappiness and
			 * their relative recent reclaim efficiency.
			 * Make sure we don't miss the last page on
			 * the offlined memory cgroups because of a
			 * round-off error.
			 */
			scan = mem_cgroup_online(memcg) ?
			       div64_u64(scan * fraction[file], denominator) :
			       DIV64_U64_ROUND_UP(scan * fraction[file],
						  denominator);
			break;
		case SCAN_FILE:
		case SCAN_ANON:
			/* Scan one type exclusively */
			if ((scan_balance == SCAN_FILE) != file) {
				lruvec_size = 0;
				scan = 0;
			}
			break;
		default:
			/* Look ma, no brain */
			BUG();
		}

		*lru_pages += lruvec_size;
		nr[lru] = scan;
	}
}

#ifdef CONFIG_LRU_GEN

/*
 * After pages are faulted in, the aging must scan them twice before the
 * eviction can consider them. The first scan clears the accessed bit set during
 * initial faults. And the second scan makes sure they haven't been used since
 * the first scan.
 */
#define MIN_NR_GENS	2

#define MAX_BATCH_SIZE	8192

/******************************************************************************
 *                          shorthand helpers
 ******************************************************************************/

#define DEFINE_MAX_SEQ()						\
	unsigned long max_seq = READ_ONCE(lruvec->evictable.max_seq)

#define DEFINE_MIN_SEQ()						\
	unsigned long min_seq[ANON_AND_FILE] = {			\
		READ_ONCE(lruvec->evictable.min_seq[0]),		\
		READ_ONCE(lruvec->evictable.min_seq[1]),		\
	}

#define for_each_type_zone(type, zone)					\
	for ((type) = 0; (type) < ANON_AND_FILE; (type)++)		\
		for ((zone) = 0; (zone) < MAX_NR_ZONES; (zone)++)

#define for_each_gen_type_zone(gen, type, zone)				\
	for ((gen) = 0; (gen) < MAX_NR_GENS; (gen)++)			\
		for ((type) = 0; (type) < ANON_AND_FILE; (type)++)	\
			for ((zone) = 0; (zone) < MAX_NR_ZONES; (zone)++)

static int page_lru_gen(struct page *page)
{
	return ((page->flags & LRU_GEN_MASK) >> LRU_GEN_PGOFF) - 1;
}

static int get_lo_wmark(unsigned long max_seq, unsigned long *min_seq, int swappiness)
{
	return max_seq - max(min_seq[!swappiness], min_seq[1]) + 1;
}

static int get_hi_wmark(unsigned long max_seq, unsigned long *min_seq, int swappiness)
{
	return max_seq - min(min_seq[!swappiness], min_seq[1]) + 1;
}

static int get_nr_gens(struct lruvec *lruvec, int type)
{
	return lruvec->evictable.max_seq - lruvec->evictable.min_seq[type] + 1;
}

static bool __maybe_unused seq_is_valid(struct lruvec *lruvec)
{
	lockdep_assert_held(&lruvec_pgdat(lruvec)->lru_lock);

	return get_nr_gens(lruvec, 0) >= MIN_NR_GENS &&
	       get_nr_gens(lruvec, 0) <= MAX_NR_GENS &&
	       get_nr_gens(lruvec, 1) >= MIN_NR_GENS &&
	       get_nr_gens(lruvec, 1) <= MAX_NR_GENS;
}

/******************************************************************************
 *                          refault feedback loop
 ******************************************************************************/

/*
 * A feedback loop modeled after the PID controller. Currently supports the
 * proportional (P) and the integral (I) terms; the derivative (D) term can be
 * added if necessary. The setpoint (SP) is the desired position; the process
 * variable (PV) is the measured position. The error is the difference between
 * the SP and the PV. A positive error results in a positive control output
 * correction, which, in our case, is to allow eviction.
 *
 * The P term is the current refault rate refaulted/(evicted+activated), which
 * has a weight of 1. The I term is the arithmetic mean of the last N refault
 * rates, weighted by geometric series 1/2, 1/4, ..., 1/(1<<N).
 *
 * Our goal is to make sure upper tiers have similar refault rates as the base
 * tier. That is we try to be fair to all tiers by maintaining similar refault
 * rates across them.
 */
struct controller_pos {
	unsigned long refaulted;
	unsigned long total;
	int gain;
};

static void read_controller_pos(struct controller_pos *pos, struct lruvec *lruvec,
				int type, int tier, int gain)
{
	struct lrugen *lrugen = &lruvec->evictable;
	int hist = hist_from_seq_or_gen(lrugen->min_seq[type]);

	pos->refaulted = lrugen->avg_refaulted[type][tier] +
			 atomic_long_read(&lrugen->refaulted[hist][type][tier]);
	pos->total = lrugen->avg_total[type][tier] +
		     atomic_long_read(&lrugen->evicted[hist][type][tier]);
	if (tier)
		pos->total += lrugen->activated[hist][type][tier - 1];
	pos->gain = gain;
}

static void reset_controller_pos(struct lruvec *lruvec, int gen, int type)
{
	int tier;
	int hist = hist_from_seq_or_gen(gen);
	struct lrugen *lrugen = &lruvec->evictable;
	bool carryover = gen == lru_gen_from_seq(lrugen->min_seq[type]);

	if (!carryover && NR_STAT_GENS == 1)
		return;

	for (tier = 0; tier < MAX_NR_TIERS; tier++) {
		if (carryover) {
			unsigned long sum;

			sum = lrugen->avg_refaulted[type][tier] +
			      atomic_long_read(&lrugen->refaulted[hist][type][tier]);
			WRITE_ONCE(lrugen->avg_refaulted[type][tier], sum / 2);

			sum = lrugen->avg_total[type][tier] +
			      atomic_long_read(&lrugen->evicted[hist][type][tier]);
			if (tier)
				sum += lrugen->activated[hist][type][tier - 1];
			WRITE_ONCE(lrugen->avg_total[type][tier], sum / 2);

			if (NR_STAT_GENS > 1)
				continue;
		}

		atomic_long_set(&lrugen->refaulted[hist][type][tier], 0);
		atomic_long_set(&lrugen->evicted[hist][type][tier], 0);
		if (tier)
			WRITE_ONCE(lrugen->activated[hist][type][tier - 1], 0);
	}
}

static bool positive_ctrl_err(struct controller_pos *sp, struct controller_pos *pv)
{
	/*
	 * Allow eviction if the PV has a limited number of refaulted pages or a
	 * lower refault rate than the SP.
	 */
	return pv->refaulted < SWAP_CLUSTER_MAX ||
	       pv->refaulted * max(sp->total, 1UL) * sp->gain <=
	       sp->refaulted * max(pv->total, 1UL) * pv->gain;
}

/******************************************************************************
 *                          mm_struct list
 ******************************************************************************/

enum {
	MM_SCHED_ACTIVE,	/* running processes */
	MM_SCHED_INACTIVE,	/* sleeping processes */
	MM_LOCK_CONTENTION,	/* lock contentions */
	MM_VMA_INTERVAL,	/* VMAs within the range of each PUD/PMD/PTE */
	MM_LEAF_OTHER_NODE,	/* entries not from the node under reclaim */
	MM_LEAF_OTHER_MEMCG,	/* entries not from the memcg under reclaim */
	MM_LEAF_OLD,		/* old entries */
	MM_LEAF_YOUNG,		/* young entries */
	MM_LEAF_DIRTY,		/* dirty entries */
	MM_LEAF_HOLE,		/* non-present entries */
	MM_NONLEAF_OLD,		/* old non-leaf PMD entries */
	MM_NONLEAF_YOUNG,	/* young non-leaf PMD entries */
	NR_MM_STATS
};

/* mnemonic codes for the stats above */
#define MM_STAT_CODES		"aicvnmoydhlu"

struct lru_gen_mm_list {
	/* the head of a global or per-memcg mm_struct list */
	struct list_head head;
	/* protects the list */
	spinlock_t lock;
	struct {
		/* set to max_seq after each round of walk */
		unsigned long cur_seq;
		/* the next mm on the list to walk */
		struct list_head *iter;
		/* to wait for the last worker to finish */
		struct wait_queue_head wait;
		/* the number of concurrent workers */
		int nr_workers;
		/* stats for debugging */
		unsigned long stats[NR_STAT_GENS][NR_MM_STATS];
	} nodes[0];
};

static struct lru_gen_mm_list *global_mm_list;

static struct lru_gen_mm_list *alloc_mm_list(void)
{
	int nid;
	struct lru_gen_mm_list *mm_list;

	mm_list = kzalloc(struct_size(mm_list, nodes, nr_node_ids), GFP_KERNEL);
	if (!mm_list)
		return NULL;

	INIT_LIST_HEAD(&mm_list->head);
	spin_lock_init(&mm_list->lock);

	for_each_node(nid) {
		mm_list->nodes[nid].cur_seq = MIN_NR_GENS;
		mm_list->nodes[nid].iter = &mm_list->head;
		init_waitqueue_head(&mm_list->nodes[nid].wait);
	}

	return mm_list;
}

static struct lru_gen_mm_list *get_mm_list(struct mem_cgroup *memcg)
{
#ifdef CONFIG_MEMCG
	if (!mem_cgroup_disabled())
		return memcg ? memcg->mm_list : root_mem_cgroup->mm_list;
#endif
	VM_BUG_ON(memcg);

	return global_mm_list;
}

void lru_gen_init_mm(struct mm_struct *mm)
{
	INIT_LIST_HEAD(&mm->lrugen.list);
#ifdef CONFIG_MEMCG
	mm->lrugen.memcg = NULL;
#endif
#ifndef CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH
	atomic_set(&mm->lrugen.nr_cpus, 0);
#endif
	nodes_clear(mm->lrugen.nodes);
}

void lru_gen_add_mm(struct mm_struct *mm)
{
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(mm);
	struct lru_gen_mm_list *mm_list = get_mm_list(memcg);

	VM_BUG_ON_MM(!list_empty(&mm->lrugen.list), mm);
#ifdef CONFIG_MEMCG
	VM_BUG_ON_MM(mm->lrugen.memcg, mm);
	WRITE_ONCE(mm->lrugen.memcg, memcg);
#endif
	spin_lock(&mm_list->lock);
	list_add_tail(&mm->lrugen.list, &mm_list->head);
	spin_unlock(&mm_list->lock);
}

void lru_gen_del_mm(struct mm_struct *mm)
{
	int nid;
#ifdef CONFIG_MEMCG
	struct lru_gen_mm_list *mm_list = get_mm_list(mm->lrugen.memcg);
#else
	struct lru_gen_mm_list *mm_list = get_mm_list(NULL);
#endif

	spin_lock(&mm_list->lock);

	for_each_node(nid) {
		if (mm_list->nodes[nid].iter != &mm->lrugen.list)
			continue;

		mm_list->nodes[nid].iter = mm_list->nodes[nid].iter->next;
		if (mm_list->nodes[nid].iter == &mm_list->head)
			WRITE_ONCE(mm_list->nodes[nid].cur_seq,
				   mm_list->nodes[nid].cur_seq + 1);
	}

	list_del_init(&mm->lrugen.list);

	spin_unlock(&mm_list->lock);

#ifdef CONFIG_MEMCG
	mem_cgroup_put(mm->lrugen.memcg);
	WRITE_ONCE(mm->lrugen.memcg, NULL);
#endif
}

#ifdef CONFIG_MEMCG
int lru_gen_alloc_mm_list(struct mem_cgroup *memcg)
{
	if (mem_cgroup_disabled())
		return 0;

	memcg->mm_list = alloc_mm_list();

	return memcg->mm_list ? 0 : -ENOMEM;
}

void lru_gen_free_mm_list(struct mem_cgroup *memcg)
{
	kfree(memcg->mm_list);
	memcg->mm_list = NULL;
}

void lru_gen_migrate_mm(struct mm_struct *mm)
{
	struct mem_cgroup *memcg;

	lockdep_assert_held(&mm->owner->alloc_lock);

	if (mem_cgroup_disabled())
		return;

	rcu_read_lock();
	memcg = mem_cgroup_from_task(mm->owner);
	rcu_read_unlock();
	if (memcg == mm->lrugen.memcg)
		return;

	VM_BUG_ON_MM(!mm->lrugen.memcg, mm);
	VM_BUG_ON_MM(list_empty(&mm->lrugen.list), mm);

	lru_gen_del_mm(mm);
	lru_gen_add_mm(mm);
}

static bool mm_has_migrated(struct mm_struct *mm, struct mem_cgroup *memcg)
{
	return READ_ONCE(mm->lrugen.memcg) != memcg;
}
#else
static bool mm_has_migrated(struct mm_struct *mm, struct mem_cgroup *memcg)
{
	return false;
}
#endif

struct mm_walk_args {
	struct mem_cgroup *memcg;
	unsigned long max_seq;
	unsigned long start_pfn;
	unsigned long end_pfn;
	unsigned long next_addr;
	int node_id;
	int swappiness;
	int batch_size;
	int nr_pages[MAX_NR_GENS][ANON_AND_FILE][MAX_NR_ZONES];
	int mm_stats[NR_MM_STATS];
	unsigned long bitmap[0];
};

static int size_of_mm_walk_args(void)
{
	int size = sizeof(struct mm_walk_args);

	if (IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE) ||
	    IS_ENABLED(CONFIG_HAVE_ARCH_PARENT_PMD_YOUNG))
		size += sizeof(unsigned long) * BITS_TO_LONGS(PTRS_PER_PMD);

	return size;
}

static void reset_mm_stats(struct lru_gen_mm_list *mm_list, bool last,
			   struct mm_walk_args *args)
{
	int i;
	int nid = args->node_id;
	int hist = hist_from_seq_or_gen(args->max_seq);

	lockdep_assert_held(&mm_list->lock);

	for (i = 0; i < NR_MM_STATS; i++) {
		WRITE_ONCE(mm_list->nodes[nid].stats[hist][i],
			   mm_list->nodes[nid].stats[hist][i] + args->mm_stats[i]);
		args->mm_stats[i] = 0;
	}

	if (!last || NR_STAT_GENS == 1)
		return;

	hist = hist_from_seq_or_gen(args->max_seq + 1);
	for (i = 0; i < NR_MM_STATS; i++)
		WRITE_ONCE(mm_list->nodes[nid].stats[hist][i], 0);
}

static bool should_skip_mm(struct mm_struct *mm, struct mm_walk_args *args)
{
	int type;
	unsigned long size = 0;

	if (!lru_gen_mm_is_active(mm) && !node_isset(args->node_id, mm->lrugen.nodes))
		return true;

	if (mm_is_oom_victim(mm))
		return true;

	for (type = !args->swappiness; type < ANON_AND_FILE; type++) {
		size += type ? get_mm_counter(mm, MM_FILEPAGES) :
			       get_mm_counter(mm, MM_ANONPAGES) +
			       get_mm_counter(mm, MM_SHMEMPAGES);
	}

	/* leave the legwork to the rmap if mappings are too sparse */
	if (size < max(SWAP_CLUSTER_MAX, mm_pgtables_bytes(mm) / PAGE_SIZE))
		return true;

	return !mmget_not_zero(mm);
}

/* To support multiple workers that concurrently walk an mm_struct list. */
static bool get_next_mm(struct mm_walk_args *args, struct mm_struct **iter)
{
	bool last = true;
	struct mm_struct *mm = NULL;
	int nid = args->node_id;
	struct lru_gen_mm_list *mm_list = get_mm_list(args->memcg);

	if (*iter)
		mmput_async(*iter);
	else if (args->max_seq <= READ_ONCE(mm_list->nodes[nid].cur_seq))
		return false;

	spin_lock(&mm_list->lock);

	VM_BUG_ON(args->max_seq > mm_list->nodes[nid].cur_seq + 1);
	VM_BUG_ON(*iter && args->max_seq < mm_list->nodes[nid].cur_seq);
	VM_BUG_ON(*iter && !mm_list->nodes[nid].nr_workers);

	if (args->max_seq <= mm_list->nodes[nid].cur_seq) {
		last = *iter;
		goto done;
	}

	if (mm_list->nodes[nid].iter == &mm_list->head) {
		VM_BUG_ON(*iter || mm_list->nodes[nid].nr_workers);
		mm_list->nodes[nid].iter = mm_list->nodes[nid].iter->next;
	}

	while (!mm && mm_list->nodes[nid].iter != &mm_list->head) {
		mm = list_entry(mm_list->nodes[nid].iter, struct mm_struct, lrugen.list);
		mm_list->nodes[nid].iter = mm_list->nodes[nid].iter->next;
		if (should_skip_mm(mm, args))
			mm = NULL;

		args->mm_stats[mm ? MM_SCHED_ACTIVE : MM_SCHED_INACTIVE]++;
	}

	if (mm_list->nodes[nid].iter == &mm_list->head)
		WRITE_ONCE(mm_list->nodes[nid].cur_seq,
			   mm_list->nodes[nid].cur_seq + 1);
done:
	if (*iter && !mm)
		mm_list->nodes[nid].nr_workers--;
	if (!*iter && mm)
		mm_list->nodes[nid].nr_workers++;

	last = last && !mm_list->nodes[nid].nr_workers &&
	       mm_list->nodes[nid].iter == &mm_list->head;

	reset_mm_stats(mm_list, last, args);

	spin_unlock(&mm_list->lock);

	*iter = mm;
	if (mm)
		node_clear(nid, mm->lrugen.nodes);

	return last;
}

/******************************************************************************
 *                          the aging
 ******************************************************************************/

static void update_batch_size(struct page *page, int old_gen, int new_gen,
			      struct mm_walk_args *args)
{
	int type = page_is_file_cache(page);
	int zone = page_zonenum(page);
	int delta = hpage_nr_pages(page);

	VM_BUG_ON(old_gen >= MAX_NR_GENS);
	VM_BUG_ON(new_gen >= MAX_NR_GENS);

	args->batch_size++;

	args->nr_pages[old_gen][type][zone] -= delta;
	args->nr_pages[new_gen][type][zone] += delta;
}

static void reset_batch_size(struct lruvec *lruvec, struct mm_walk_args *args)
{
	int gen, type, zone;
	struct lrugen *lrugen = &lruvec->evictable;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	if (!args->batch_size)
		return;

	args->batch_size = 0;

	spin_lock_irq(&pgdat->lru_lock);

	for_each_gen_type_zone(gen, type, zone) {
		enum lru_list lru = type * LRU_FILE;
		int total = args->nr_pages[gen][type][zone];

		if (!total)
			continue;

		args->nr_pages[gen][type][zone] = 0;
		WRITE_ONCE(lrugen->sizes[gen][type][zone],
			   lrugen->sizes[gen][type][zone] + total);

		if (lru_gen_is_active(lruvec, gen))
			lru += LRU_ACTIVE;
		update_lru_size(lruvec, lru, zone, total);
	}

	spin_unlock_irq(&pgdat->lru_lock);
}

static int page_update_gen(struct page *page, int new_gen)
{
	int old_gen;
	unsigned long old_flags, new_flags;

	VM_BUG_ON(new_gen >= MAX_NR_GENS);

	do {
		old_flags = READ_ONCE(page->flags);

		old_gen = ((old_flags & LRU_GEN_MASK) >> LRU_GEN_PGOFF) - 1;
		if (old_gen < 0) {
			new_flags = old_flags | BIT(PG_referenced);
			continue;
		}

		new_flags = (old_flags & ~(LRU_GEN_MASK | LRU_USAGE_MASK | LRU_TIER_FLAGS)) |
			    ((new_gen + 1UL) << LRU_GEN_PGOFF);
	} while (new_flags != old_flags &&
		 cmpxchg(&page->flags, old_flags, new_flags) != old_flags);

	return old_gen;
}

static int should_skip_vma(unsigned long start, unsigned long end, struct mm_walk *walk)
{
	struct address_space *mapping;
	struct vm_area_struct *vma = walk->vma;
	struct mm_walk_args *args = walk->private;

	if (is_vm_hugetlb_page(vma) || (vma->vm_flags & (VM_LOCKED | VM_SPECIAL)) ||
	    !(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))
		return true;

	if (vma_is_anonymous(vma))
		return !args->swappiness;

	if (WARN_ON_ONCE(!vma->vm_file || !vma->vm_file->f_mapping))
		return true;

	mapping = vma->vm_file->f_mapping;
	if (!mapping->a_ops->writepage)
		return true;

	return (shmem_mapping(mapping) && !args->swappiness) || mapping_unevictable(mapping);
}

/*
 * Some userspace memory allocators create many single-page VMAs. So instead of
 * returning back to the PGD table for each of such VMAs, we finish at least an
 * entire PMD table and therefore avoid many zigzags. This optimizes page table
 * walks for workloads that have large numbers of tiny VMAs.
 *
 * We scan PMD tables in two passes. The first pass reaches to PTE tables and
 * doesn't take the PMD lock. The second pass clears the accessed bit on PMD
 * entries and needs to take the PMD lock. The second pass is only done on the
 * PMD entries that first pass has found the accessed bit is set, namely
 *   1) leaf entries mapping huge pages from the node under reclaim, and
 *   2) non-leaf entries whose leaf entries only map pages from the node under
 *   reclaim, when CONFIG_HAVE_ARCH_PARENT_PMD_YOUNG=y.
 */
static bool get_next_vma(struct mm_walk *walk, unsigned long mask, unsigned long size,
			 unsigned long *start, unsigned long *end)
{
	unsigned long next = round_up(*end, size);
	struct mm_walk_args *args = walk->private;

	VM_BUG_ON(mask & size);
	VM_BUG_ON(*start >= *end);
	VM_BUG_ON((next & mask) != (*start & mask));

	while (walk->vma) {
		if (next >= walk->vma->vm_end) {
			walk->vma = walk->vma->vm_next;
			continue;
		}

		if ((next & mask) != (walk->vma->vm_start & mask))
			return false;

		if (should_skip_vma(walk->vma->vm_start, walk->vma->vm_end, walk)) {
			walk->vma = walk->vma->vm_next;
			continue;
		}

		*start = max(next, walk->vma->vm_start);
		next = (next | ~mask) + 1;
		/* rounded-up boundaries can wrap to 0 */
		*end = next && next < walk->vma->vm_end ? next : walk->vma->vm_end;

		args->mm_stats[MM_VMA_INTERVAL]++;

		return true;
	}

	return false;
}

static bool walk_pte_range(pmd_t *pmd, unsigned long start, unsigned long end,
			   struct mm_walk *walk)
{
	int i;
	pte_t *pte;
	spinlock_t *ptl;
	unsigned long addr;
	int remote = 0;
	struct mm_walk_args *args = walk->private;
	int old_gen, new_gen = lru_gen_from_seq(args->max_seq);

	VM_BUG_ON(pmd_trans_huge(*pmd) || pmd_devmap(*pmd));

	pte = pte_offset_map_lock(walk->mm, pmd, start & PMD_MASK, &ptl);
	arch_enter_lazy_mmu_mode();
restart:
	i = (start >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);
	for (addr = start; addr != end; i++, addr += PAGE_SIZE) {
		struct page *page;
		unsigned long pfn = pte_pfn(pte[i]);

		if (!pte_present(pte[i]) || is_zero_pfn(pfn)) {
			args->mm_stats[MM_LEAF_HOLE]++;
			continue;
		}

		if (WARN_ON_ONCE(pte_devmap(pte[i]) || pte_special(pte[i])))
			continue;

		if (!pte_young(pte[i])) {
			args->mm_stats[MM_LEAF_OLD]++;
			continue;
		}

		VM_BUG_ON(!pfn_valid(pfn));
		if (pfn < args->start_pfn || pfn >= args->end_pfn) {
			args->mm_stats[MM_LEAF_OTHER_NODE]++;
			remote++;
			continue;
		}

		page = compound_head(pfn_to_page(pfn));
		if (page_to_nid(page) != args->node_id) {
			args->mm_stats[MM_LEAF_OTHER_NODE]++;
			remote++;
			continue;
		}

		if (page_memcg_rcu(page) != args->memcg) {
			args->mm_stats[MM_LEAF_OTHER_MEMCG]++;
			continue;
		}

		VM_BUG_ON(addr < walk->vma->vm_start || addr >= walk->vma->vm_end);
		if (!ptep_test_and_clear_young(walk->vma, addr, pte + i))
			continue;

		if (pte_dirty(pte[i]) && !PageDirty(page) &&
		    !(PageAnon(page) && PageSwapBacked(page) && !PageSwapCache(page))) {
			set_page_dirty(page);
			args->mm_stats[MM_LEAF_DIRTY]++;
		}

		old_gen = page_update_gen(page, new_gen);
		if (old_gen >= 0 && old_gen != new_gen)
			update_batch_size(page, old_gen, new_gen, args);
		args->mm_stats[MM_LEAF_YOUNG]++;
	}

	if (i < PTRS_PER_PTE && get_next_vma(walk, PMD_MASK, PAGE_SIZE, &start, &end))
		goto restart;

	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(pte, ptl);

	return IS_ENABLED(CONFIG_HAVE_ARCH_PARENT_PMD_YOUNG) && !remote;
}

#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || defined(CONFIG_HAVE_ARCH_PARENT_PMD_YOUNG)
static void __walk_pmd_range(pud_t *pud, unsigned long start,
			     struct vm_area_struct *vma, struct mm_walk *walk)
{
	int i;
	pmd_t *pmd;
	spinlock_t *ptl;
	struct mm_walk_args *args = walk->private;
	int old_gen, new_gen = lru_gen_from_seq(args->max_seq);

	VM_BUG_ON(pud_trans_huge(*pud) || pud_devmap(*pud));

	start &= PUD_MASK;
	pmd = pmd_offset(pud, start);
	ptl = pmd_lock(walk->mm, pmd);
	arch_enter_lazy_mmu_mode();

	for_each_set_bit(i, args->bitmap, PTRS_PER_PMD) {
		struct page *page;
		unsigned long pfn = pmd_pfn(pmd[i]);
		unsigned long addr = start + i * PMD_SIZE;

		if (!pmd_present(pmd[i]) || is_huge_zero_pmd(pmd[i])) {
			args->mm_stats[MM_LEAF_HOLE]++;
			continue;
		}

		if (WARN_ON_ONCE(pmd_devmap(pmd[i])))
			continue;

		if (!pmd_young(pmd[i])) {
			args->mm_stats[MM_LEAF_OLD]++;
			continue;
		}

		if (!pmd_trans_huge(pmd[i])) {
			if (IS_ENABLED(CONFIG_HAVE_ARCH_PARENT_PMD_YOUNG) &&
			    pmdp_test_and_clear_young(vma, addr, pmd + i))
				args->mm_stats[MM_NONLEAF_YOUNG]++;
			continue;
		}

		VM_BUG_ON(!pfn_valid(pfn));
		if (pfn < args->start_pfn || pfn >= args->end_pfn) {
			args->mm_stats[MM_LEAF_OTHER_NODE]++;
			continue;
		}

		page = pfn_to_page(pfn);
		VM_BUG_ON_PAGE(PageTail(page), page);
		if (page_to_nid(page) != args->node_id) {
			args->mm_stats[MM_LEAF_OTHER_NODE]++;
			continue;
		}

		if (page_memcg_rcu(page) != args->memcg) {
			args->mm_stats[MM_LEAF_OTHER_MEMCG]++;
			continue;
		}

		VM_BUG_ON(addr < vma->vm_start || addr >= vma->vm_end);
		if (!pmdp_test_and_clear_young(vma, addr, pmd + i))
			continue;

		if (pmd_dirty(pmd[i]) && !PageDirty(page) &&
		    !(PageAnon(page) && PageSwapBacked(page) && !PageSwapCache(page))) {
			set_page_dirty(page);
			args->mm_stats[MM_LEAF_DIRTY]++;
		}

		old_gen = page_update_gen(page, new_gen);
		if (old_gen >= 0 && old_gen != new_gen)
			update_batch_size(page, old_gen, new_gen, args);
		args->mm_stats[MM_LEAF_YOUNG]++;
	}

	arch_leave_lazy_mmu_mode();
	spin_unlock(ptl);

	bitmap_zero(args->bitmap, PTRS_PER_PMD);
}
#else
static void __walk_pmd_range(pud_t *pud, unsigned long start,
			     struct vm_area_struct *vma, struct mm_walk *walk)
{
}
#endif

static void walk_pmd_range(pud_t *pud, unsigned long start, unsigned long end,
			   struct mm_walk *walk)
{
	int i;
	pmd_t *pmd;
	unsigned long next;
	unsigned long addr;
	struct vm_area_struct *vma;
	int leaf = 0;
	int nonleaf = 0;
	struct mm_walk_args *args = walk->private;

	VM_BUG_ON(pud_trans_huge(*pud) || pud_devmap(*pud));

	pmd = pmd_offset(pud, start & PUD_MASK);
restart:
	vma = walk->vma;
	i = (start >> PMD_SHIFT) & (PTRS_PER_PMD - 1);
	for (addr = start; addr != end; i++, addr = next) {
		pmd_t val = pmd_read_atomic(pmd + i);

		/* for pmd_read_atomic() */
		barrier();

		next = pmd_addr_end(addr, end);

		if (!pmd_present(val)) {
			args->mm_stats[MM_LEAF_HOLE]++;
			continue;
		}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		if (pmd_trans_huge(val)) {
			unsigned long pfn = pmd_pfn(val);

			if (is_huge_zero_pmd(val)) {
				args->mm_stats[MM_LEAF_HOLE]++;
				continue;
			}

			if (!pmd_young(val)) {
				args->mm_stats[MM_LEAF_OLD]++;
				continue;
			}

			if (pfn < args->start_pfn || pfn >= args->end_pfn) {
				args->mm_stats[MM_LEAF_OTHER_NODE]++;
				continue;
			}

			__set_bit(i, args->bitmap);
			leaf++;
			continue;
		}
#endif

#ifdef CONFIG_HAVE_ARCH_PARENT_PMD_YOUNG
		if (!pmd_young(val)) {
			args->mm_stats[MM_NONLEAF_OLD]++;
			continue;
		}
#endif
		if (walk_pte_range(&val, addr, next, walk)) {
			__set_bit(i, args->bitmap);
			nonleaf++;
		}
	}

	if (leaf) {
		__walk_pmd_range(pud, start, vma, walk);
		leaf = nonleaf = 0;
	}

	if (i < PTRS_PER_PMD && get_next_vma(walk, PUD_MASK, PMD_SIZE, &start, &end))
		goto restart;

	if (nonleaf)
		__walk_pmd_range(pud, start, vma, walk);
}

static int walk_pud_range(p4d_t *p4d, unsigned long start, unsigned long end,
			  struct mm_walk *walk)
{
	int i;
	pud_t *pud;
	unsigned long addr;
	unsigned long next;
	struct mm_walk_args *args = walk->private;

	pud = pud_offset(p4d, start & P4D_MASK);
restart:
	i = (start >> PUD_SHIFT) & (PTRS_PER_PUD - 1);
	for (addr = start; addr != end; i++, addr = next) {
		pud_t val = READ_ONCE(pud[i]);

		next = pud_addr_end(addr, end);

		if (!pud_present(val) || WARN_ON_ONCE(pud_trans_huge(val) || pud_devmap(val)))
			continue;

		walk_pmd_range(&val, addr, next, walk);

		if (args->batch_size >= MAX_BATCH_SIZE) {
			end = (addr | ~PUD_MASK) + 1;
			goto done;
		}
	}

	if (i < PTRS_PER_PUD && get_next_vma(walk, P4D_MASK, PUD_SIZE, &start, &end))
		goto restart;

	end = round_up(end, P4D_SIZE);
done:
	/* rounded-up boundaries can wrap to 0 */
	args->next_addr = end && walk->vma ? max(end, walk->vma->vm_start) : 0;

	return -EAGAIN;
}

static void walk_mm(struct mm_walk_args *args, struct mm_struct *mm)
{
	static const struct mm_walk_ops mm_walk_ops = {
		.test_walk = should_skip_vma,
		.p4d_entry = walk_pud_range,
	};

	int err;
	struct mem_cgroup *memcg = args->memcg;
	struct lruvec *lruvec = mem_cgroup_lruvec(NODE_DATA(args->node_id), memcg);

	args->next_addr = FIRST_USER_ADDRESS;

	do {
		unsigned long start = args->next_addr;
		unsigned long end = mm->highest_vm_end;

		err = -EBUSY;

		rcu_read_lock();
#ifdef CONFIG_MEMCG
		if (memcg && atomic_read(&memcg->moving_account)) {
			args->mm_stats[MM_LOCK_CONTENTION]++;
			goto contended;
		}
#endif
		if (!down_read_trylock(&mm->mmap_sem)) {
			args->mm_stats[MM_LOCK_CONTENTION]++;
			goto contended;
		}

		err = walk_page_range(mm, start, end, &mm_walk_ops, args);

		up_read(&mm->mmap_sem);

		reset_batch_size(lruvec, args);
contended:
		rcu_read_unlock();

		cond_resched();
	} while (err == -EAGAIN && args->next_addr &&
		 !mm_is_oom_victim(mm) && !mm_has_migrated(mm, memcg));

}

static bool mmu_notifier_start_batch(struct mm_struct *mm, void *priv)
{
	struct mm_walk_args *args = priv;
	struct mem_cgroup *memcg = args->memcg;

	VM_BUG_ON(!rcu_read_lock_held());

#ifdef CONFIG_MEMCG
	if (memcg && atomic_read(&memcg->moving_account)) {
		args->mm_stats[MM_LOCK_CONTENTION]++;
		return false;
	}
#endif
	return !mm_is_oom_victim(mm) && !mm_has_migrated(mm, memcg);
}

static bool mmu_notifier_end_batch(void *priv, bool last)
{
	struct lruvec *lruvec;
	struct mm_walk_args *args = priv;

	VM_BUG_ON(!rcu_read_lock_held());

	if (!last && args->batch_size < MAX_BATCH_SIZE)
		return false;

	lruvec = mem_cgroup_lruvec(NODE_DATA(args->node_id), args->memcg);
	reset_batch_size(lruvec, args);

	return true;
}

static struct page *mmu_notifier_get_page(void *priv, unsigned long pfn, bool young)
{
	struct page *page;
	struct mm_walk_args *args = priv;

	if (pfn == -1 || is_zero_pfn(pfn)) {
		args->mm_stats[MM_LEAF_HOLE]++;
		return NULL;
	}

	if (!young) {
		args->mm_stats[MM_LEAF_OLD]++;
		return NULL;
	}

	VM_BUG_ON(!pfn_valid(pfn));
	if (pfn < args->start_pfn || pfn >= args->end_pfn) {
		args->mm_stats[MM_LEAF_OTHER_NODE]++;
		return NULL;
	}

	page = compound_head(pfn_to_page(pfn));
	if (page_to_nid(page) != args->node_id) {
		args->mm_stats[MM_LEAF_OTHER_NODE]++;
		return NULL;
	}

	if (page_memcg_rcu(page) != args->memcg) {
		args->mm_stats[MM_LEAF_OTHER_MEMCG]++;
		return NULL;
	}

	if (!PageLRU(page)) {
		args->mm_stats[MM_LEAF_HOLE]++;
		return NULL;
	}

	return get_page_unless_zero(page) ? page : NULL;
}

static void mmu_notifier_update_page(void *priv, struct page *page)
{
	struct mm_walk_args *args = priv;
	int old_gen, new_gen = lru_gen_from_seq(args->max_seq);

	if (page_memcg_rcu(page) != args->memcg) {
		args->mm_stats[MM_LEAF_OTHER_MEMCG]++;
		return;
	}

	if (!PageLRU(page)) {
		args->mm_stats[MM_LEAF_HOLE]++;
		return;
	}

	old_gen = page_update_gen(page, new_gen);
	if (old_gen >= 0 && old_gen != new_gen)
		update_batch_size(page, old_gen, new_gen, args);
	args->mm_stats[MM_LEAF_YOUNG]++;
}

static void call_mmu_notifier(struct mm_walk_args *args, struct mm_struct *mm)
{
	struct mmu_notifier_walk walk = {
		.start_batch = mmu_notifier_start_batch,
		.end_batch = mmu_notifier_end_batch,
		.get_page = mmu_notifier_get_page,
		.update_page = mmu_notifier_update_page,
		.private = args,
	};

	mmu_notifier_clear_young_walk(mm, &walk);
}

static void page_inc_gen(struct page *page, struct lruvec *lruvec, bool front)
{
	int old_gen, new_gen;
	unsigned long old_flags, new_flags;
	int type = page_is_file_cache(page);
	int zone = page_zonenum(page);
	struct lrugen *lrugen = &lruvec->evictable;

	old_gen = lru_gen_from_seq(lrugen->min_seq[type]);

	do {
		old_flags = READ_ONCE(page->flags);

		/* in case the aging has updated old_gen */
		new_gen = ((old_flags & LRU_GEN_MASK) >> LRU_GEN_PGOFF) - 1;
		VM_BUG_ON_PAGE(new_gen < 0, page);
		if (new_gen >= 0 && new_gen != old_gen)
			goto sort;

		new_gen = (old_gen + 1) % MAX_NR_GENS;

		new_flags = (old_flags & ~(LRU_GEN_MASK | LRU_USAGE_MASK | LRU_TIER_FLAGS)) |
			    ((new_gen + 1UL) << LRU_GEN_PGOFF);
		/* mark the page for reclaim if it's pending writeback */
		if (front)
			new_flags |= BIT(PG_reclaim);
	} while (cmpxchg(&page->flags, old_flags, new_flags) != old_flags);

	lru_gen_update_size(page, lruvec, old_gen, new_gen);
sort:
	if (front)
		list_move(&page->lru, &lrugen->lists[new_gen][type][zone]);
	else
		list_move_tail(&page->lru, &lrugen->lists[new_gen][type][zone]);
}

static bool try_inc_min_seq(struct lruvec *lruvec, int type)
{
	int gen, zone;
	bool success = false;
	struct lrugen *lrugen = &lruvec->evictable;

	VM_BUG_ON(!seq_is_valid(lruvec));

	while (get_nr_gens(lruvec, type) > MIN_NR_GENS) {
		gen = lru_gen_from_seq(lrugen->min_seq[type]);

		for (zone = 0; zone < MAX_NR_ZONES; zone++) {
			if (!list_empty(&lrugen->lists[gen][type][zone]))
				return success;
		}

		reset_controller_pos(lruvec, gen, type);
		WRITE_ONCE(lrugen->min_seq[type], lrugen->min_seq[type] + 1);

		success = true;
	}

	return success;
}

static bool inc_min_seq(struct lruvec *lruvec, int type)
{
	int gen, zone;
	int batch_size = 0;
	struct lrugen *lrugen = &lruvec->evictable;

	VM_BUG_ON(!seq_is_valid(lruvec));

	if (get_nr_gens(lruvec, type) != MAX_NR_GENS)
		return true;

	gen = lru_gen_from_seq(lrugen->min_seq[type]);

	for (zone = 0; zone < MAX_NR_ZONES; zone++) {
		struct list_head *head = &lrugen->lists[gen][type][zone];

		while (!list_empty(head)) {
			struct page *page = lru_to_page(head);

			VM_BUG_ON_PAGE(PageTail(page), page);
			VM_BUG_ON_PAGE(PageUnevictable(page), page);
			VM_BUG_ON_PAGE(PageActive(page), page);
			VM_BUG_ON_PAGE(page_is_file_cache(page) != type, page);
			VM_BUG_ON_PAGE(page_zonenum(page) != zone, page);

			prefetchw_prev_lru_page(page, head, flags);

			page_inc_gen(page, lruvec, false);

			if (++batch_size == MAX_BATCH_SIZE)
				return false;
		}

		VM_BUG_ON(lrugen->sizes[gen][type][zone]);
	}

	reset_controller_pos(lruvec, gen, type);
	WRITE_ONCE(lrugen->min_seq[type], lrugen->min_seq[type] + 1);

	return true;
}

static void inc_max_seq(struct lruvec *lruvec, unsigned long max_seq)
{
	int gen, type, zone;
	struct lrugen *lrugen = &lruvec->evictable;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	spin_lock_irq(&pgdat->lru_lock);

	VM_BUG_ON(!seq_is_valid(lruvec));

	if (lrugen->max_seq != max_seq)
		goto unlock;

	for (type = 0; type < ANON_AND_FILE; type++) {
		if (try_inc_min_seq(lruvec, type))
			continue;

		while (!inc_min_seq(lruvec, type)) {
			spin_unlock_irq(&pgdat->lru_lock);
			cond_resched();
			spin_lock_irq(&pgdat->lru_lock);
		}
	}

	gen = lru_gen_from_seq(lrugen->max_seq - 1);
	for_each_type_zone(type, zone) {
		enum lru_list lru = type * LRU_FILE;
		long total = lrugen->sizes[gen][type][zone];

		if (!total)
			continue;

		WARN_ON_ONCE(total != (int)total);

		update_lru_size(lruvec, lru, zone, total);
		update_lru_size(lruvec, lru + LRU_ACTIVE, zone, -total);
	}

	gen = lru_gen_from_seq(lrugen->max_seq + 1);
	for_each_type_zone(type, zone) {
		VM_BUG_ON(lrugen->sizes[gen][type][zone]);
		VM_BUG_ON(!list_empty(&lrugen->lists[gen][type][zone]));
	}

	for (type = 0; type < ANON_AND_FILE; type++)
		reset_controller_pos(lruvec, gen, type);

	WRITE_ONCE(lrugen->timestamps[gen], jiffies);
	/* make sure all preceding modifications appear first */
	smp_store_release(&lrugen->max_seq, lrugen->max_seq + 1);
unlock:
	spin_unlock_irq(&pgdat->lru_lock);
}

/* Main function used by the foreground, the background and the user-triggered aging. */
static bool walk_mm_list(struct lruvec *lruvec, unsigned long max_seq,
			 struct scan_control *sc, int swappiness, struct mm_walk_args *args)
{
	bool last;
	bool alloc = !args;
	struct mm_struct *mm = NULL;
	struct lrugen *lrugen = &lruvec->evictable;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	int nid = pgdat->node_id;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	struct lru_gen_mm_list *mm_list = get_mm_list(memcg);

	VM_BUG_ON(max_seq > READ_ONCE(lrugen->max_seq));

	if (alloc) {
		args = kvzalloc_node(size_of_mm_walk_args(), GFP_KERNEL, nid);
		if (WARN_ON_ONCE(!args))
			return false;
	}

	args->memcg = memcg;
	args->max_seq = max_seq;
	args->start_pfn = pgdat->node_start_pfn;
	args->end_pfn = pgdat_end_pfn(pgdat);
	args->node_id = nid;
	args->swappiness = swappiness;

	do {
		last = get_next_mm(args, &mm);
		if (mm) {
			walk_mm(args, mm);
			call_mmu_notifier(args, mm);
		}

		cond_resched();
	} while (mm);

	if (alloc)
		kvfree(args);

	if (!last) {
		/* the foreground aging prefers not to wait */
		if (!current_is_kswapd() && sc->priority < DEF_PRIORITY - 2)
			wait_event_killable(mm_list->nodes[nid].wait,
					    max_seq < READ_ONCE(lrugen->max_seq));

		return max_seq < READ_ONCE(lrugen->max_seq);
	}

	VM_BUG_ON(max_seq != READ_ONCE(lrugen->max_seq));

	inc_max_seq(lruvec, max_seq);
	/* either we see any waiters or they will see updated max_seq */
	if (wq_has_sleeper(&mm_list->nodes[nid].wait))
		wake_up_all(&mm_list->nodes[nid].wait);

	wakeup_flusher_threads(WB_REASON_VMSCAN);

	return true;
}

void lru_gen_scan_around(struct page_vma_mapped_walk *pvmw)
{
	int i;
	pte_t *pte;
	int old_gen, new_gen;
	unsigned long start;
	unsigned long end;
	unsigned long addr;
	struct lruvec *lruvec;
	struct mem_cgroup *memcg;
	struct pglist_data *pgdat = page_pgdat(pvmw->page);
	unsigned long bitmap[BITS_TO_LONGS(SWAP_CLUSTER_MAX * 2)] = {};

	lockdep_assert_held(pvmw->ptl);
	VM_BUG_ON_PAGE(PageTail(pvmw->page), pvmw->page);

	start = max(pvmw->address & PMD_MASK, pvmw->vma->vm_start);
	end = pmd_addr_end(pvmw->address, pvmw->vma->vm_end);

	if (end - start > SWAP_CLUSTER_MAX * 2 * PAGE_SIZE) {
		if (pvmw->address - start < SWAP_CLUSTER_MAX * PAGE_SIZE)
			end = start + SWAP_CLUSTER_MAX * 2 * PAGE_SIZE;
		else if (end - pvmw->address < SWAP_CLUSTER_MAX * PAGE_SIZE)
			start = end - SWAP_CLUSTER_MAX * 2 * PAGE_SIZE;
		else {
			start = pvmw->address - SWAP_CLUSTER_MAX * PAGE_SIZE;
			end = pvmw->address + SWAP_CLUSTER_MAX * PAGE_SIZE;
		}
	}

	pte = pvmw->pte - (pvmw->address - start) / PAGE_SIZE;

	arch_enter_lazy_mmu_mode();

	memcg = lock_page_memcg(pvmw->page);
	if (WARN_ON_ONCE(!mem_cgroup_disabled() && !memcg))
		goto out;

	spin_lock_irq(&pgdat->lru_lock);

	lruvec = mem_cgroup_lruvec(pgdat, memcg);
	new_gen = lru_gen_from_seq(lruvec->evictable.max_seq);

	for (i = 0, addr = start; addr != end; i++, addr += PAGE_SIZE) {
		struct page *page;
		unsigned long pfn = pte_pfn(pte[i]);

		if (!pte_present(pte[i]) || is_zero_pfn(pfn))
			continue;

		if (WARN_ON_ONCE(pte_devmap(pte[i]) || pte_special(pte[i])))
			continue;

		if (!pte_young(pte[i]))
			continue;

		VM_BUG_ON(!pfn_valid(pfn));
		if (pfn < pgdat->node_start_pfn || pfn >= pgdat_end_pfn(pgdat))
			continue;

		page = compound_head(pfn_to_page(pfn));
		if (page_to_nid(page) != pgdat->node_id)
			continue;

		if (page_memcg_rcu(page) != memcg)
			continue;

		VM_BUG_ON(addr < pvmw->vma->vm_start || addr >= pvmw->vma->vm_end);
		if (!ptep_test_and_clear_young(pvmw->vma, addr, pte + i))
			continue;

		if (pte_dirty(pte[i]) && !PageDirty(page) &&
		    !(PageAnon(page) && PageSwapBacked(page) && !PageSwapCache(page)))
			__set_bit(i, bitmap);

		old_gen = page_update_gen(page, new_gen);
		if (old_gen >= 0 && old_gen != new_gen)
			lru_gen_update_size(page, lruvec, old_gen, new_gen);
	}

	spin_unlock_irq(&pgdat->lru_lock);
	unlock_page_memcg(pvmw->page);
out:
	arch_leave_lazy_mmu_mode();

	for_each_set_bit(i, bitmap, SWAP_CLUSTER_MAX * 2)
		set_page_dirty(pte_page(pte[i]));
}

/******************************************************************************
 *                          the eviction
 ******************************************************************************/

static bool should_skip_page(struct page *page, struct scan_control *sc)
{
	if (!sc->may_unmap && page_mapped(page))
		return true;

	if (!(sc->may_writepage && (sc->gfp_mask & __GFP_IO)) &&
	    (PageDirty(page) || (PageAnon(page) && !PageSwapCache(page))))
		return true;

	if (!get_page_unless_zero(page))
		return true;

	return false;
}

static bool sort_page(struct page *page, struct lruvec *lruvec, int tier_to_isolate)
{
	bool success;
	int gen = page_lru_gen(page);
	int type = page_is_file_cache(page);
	int zone = page_zonenum(page);
	int tier = lru_tier_from_usage(page_tier_usage(page));
	struct lrugen *lrugen = &lruvec->evictable;

	VM_BUG_ON_PAGE(gen == -1, page);
	VM_BUG_ON_PAGE(tier_to_isolate < 0, page);

	/* a lazy-free page that has been written into? */
	if (type && PageDirty(page) && PageAnon(page)) {
		success = lru_gen_deletion(page, lruvec);
		VM_BUG_ON_PAGE(!success, page);
		SetPageSwapBacked(page);
		add_page_to_lru_list_tail(page, lruvec);
		return true;
	}

	/* page_update_gen() has updated the gen #? */
	if (gen != lru_gen_from_seq(lrugen->min_seq[type])) {
		list_move(&page->lru, &lrugen->lists[gen][type][zone]);
		return true;
	}

	/* activate this page if its tier has a higher refault rate */
	if (tier_to_isolate < tier) {
		int hist = hist_from_seq_or_gen(gen);

		page_inc_gen(page, lruvec, false);
		WRITE_ONCE(lrugen->activated[hist][type][tier - 1],
			   lrugen->activated[hist][type][tier - 1] + hpage_nr_pages(page));
		inc_lruvec_state(lruvec, WORKINGSET_ACTIVATE);
		return true;
	}

	/* mark this page for reclaim if it's pending writeback */
	if (PageWriteback(page) || (type && PageDirty(page))) {
		page_inc_gen(page, lruvec, true);
		return true;
	}

	return false;
}

static void isolate_page(struct page *page, struct lruvec *lruvec)
{
	bool success;

	success = lru_gen_deletion(page, lruvec);
	VM_BUG_ON_PAGE(!success, page);

	ClearPageLRU(page);

	if (PageActive(page)) {
		ClearPageActive(page);
		/* make sure shrink_page_list() rejects this page */
		SetPageReferenced(page);
		return;
	}

	/* make sure shrink_page_list() doesn't try to write this page */
	ClearPageReclaim(page);
	/* make sure shrink_page_list() doesn't reject this page */
	ClearPageReferenced(page);
}

static int scan_pages(struct lruvec *lruvec, struct scan_control *sc, long *nr_to_scan,
		      int type, int tier, struct list_head *list)
{
	bool success;
	int gen, zone;
	enum vm_event_item item;
	int sorted = 0;
	int scanned = 0;
	int isolated = 0;
	int batch_size = 0;
	struct lrugen *lrugen = &lruvec->evictable;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);

	VM_BUG_ON(!list_empty(list));

	if (get_nr_gens(lruvec, type) == MIN_NR_GENS)
		return -ENOENT;

	gen = lru_gen_from_seq(lrugen->min_seq[type]);

	for (zone = sc->reclaim_idx; zone >= 0; zone--) {
		LIST_HEAD(moved);
		int skipped = 0;
		struct list_head *head = &lrugen->lists[gen][type][zone];

		while (!list_empty(head)) {
			struct page *page = lru_to_page(head);
			int delta = hpage_nr_pages(page);

			VM_BUG_ON_PAGE(PageTail(page), page);
			VM_BUG_ON_PAGE(PageUnevictable(page), page);
			VM_BUG_ON_PAGE(PageActive(page), page);
			VM_BUG_ON_PAGE(page_is_file_cache(page) != type, page);
			VM_BUG_ON_PAGE(page_zonenum(page) != zone, page);

			prefetchw_prev_lru_page(page, head, flags);

			scanned += delta;

			if (sort_page(page, lruvec, tier))
				sorted += delta;
			else if (should_skip_page(page, sc)) {
				list_move(&page->lru, &moved);
				skipped += delta;
			} else {
				isolate_page(page, lruvec);
				list_add(&page->lru, list);
				isolated += delta;
			}

			if (scanned >= *nr_to_scan || isolated >= SWAP_CLUSTER_MAX ||
			    ++batch_size == MAX_BATCH_SIZE)
				break;
		}

		list_splice(&moved, head);
		__count_zid_vm_events(PGSCAN_SKIP, zone, skipped);

		if (scanned >= *nr_to_scan || isolated >= SWAP_CLUSTER_MAX ||
		    batch_size == MAX_BATCH_SIZE)
			break;
	}

	success = try_inc_min_seq(lruvec, type);

	item = current_is_kswapd() ? PGSCAN_KSWAPD : PGSCAN_DIRECT;
	if (global_reclaim(sc)) {
		__count_vm_events(item, isolated);
		__count_vm_events(PGREFILL, sorted);
	}
	__count_memcg_events(memcg, item, isolated);
	__count_memcg_events(memcg, PGREFILL, sorted);

	*nr_to_scan -= scanned;

	if (*nr_to_scan <= 0 || success || isolated)
		return isolated;
	/*
	 * We may have trouble finding eligible pages due to reclaim_idx,
	 * may_unmap and may_writepage. The following check makes sure we won't
	 * be stuck if we aren't making enough progress.
	 */
	return batch_size == MAX_BATCH_SIZE && sorted >= SWAP_CLUSTER_MAX ? 0 : -ENOENT;
}

static int get_tier_to_isolate(struct lruvec *lruvec, int type)
{
	int tier;
	struct controller_pos sp, pv;

	/*
	 * Ideally we don't want to evict upper tiers that have higher refault
	 * rates. However, we need to leave a margin for the fluctuations in
	 * refault rates. So we use a larger gain factor to make sure upper
	 * tiers are indeed more active. We choose 2 because the lowest upper
	 * tier would have twice of the refault rate of the base tier, according
	 * to their numbers of accesses.
	 */
	read_controller_pos(&sp, lruvec, type, 0, 1);
	for (tier = 1; tier < MAX_NR_TIERS; tier++) {
		read_controller_pos(&pv, lruvec, type, tier, 2);
		if (!positive_ctrl_err(&sp, &pv))
			break;
	}

	return tier - 1;
}

static int get_type_to_scan(struct lruvec *lruvec, int swappiness, int *tier_to_isolate)
{
	int type, tier;
	struct controller_pos sp, pv;
	int gain[ANON_AND_FILE] = { swappiness, 200 - swappiness };

	/*
	 * Compare the refault rates between the base tiers of anon and file to
	 * determine which type to evict. Also need to compare the refault rates
	 * of the upper tiers of the selected type with that of the base tier of
	 * the other type to determine which tier of the selected type to evict.
	 */
	read_controller_pos(&sp, lruvec, 0, 0, gain[0]);
	read_controller_pos(&pv, lruvec, 1, 0, gain[1]);
	type = positive_ctrl_err(&sp, &pv);

	read_controller_pos(&sp, lruvec, !type, 0, gain[!type]);
	for (tier = 1; tier < MAX_NR_TIERS; tier++) {
		read_controller_pos(&pv, lruvec, type, tier, gain[type]);
		if (!positive_ctrl_err(&sp, &pv))
			break;
	}

	*tier_to_isolate = tier - 1;

	return type;
}

static int isolate_pages(struct lruvec *lruvec, struct scan_control *sc, int swappiness,
			 long *nr_to_scan, int *type_to_scan, struct list_head *list)
{
	int i;
	int type;
	int isolated;
	int tier = -1;
	DEFINE_MAX_SEQ();
	DEFINE_MIN_SEQ();

	VM_BUG_ON(!seq_is_valid(lruvec));

	if (get_hi_wmark(max_seq, min_seq, swappiness) == MIN_NR_GENS)
		return 0;
	/*
	 * Try to select a type based on generations and swappiness, and if that
	 * fails, fall back to get_type_to_scan(). When anon and file are both
	 * available from the same generation, swappiness 200 is interpreted as
	 * anon first and swappiness 1 is interpreted as file first.
	 */
	type = !swappiness || min_seq[0] > min_seq[1] ||
	       (min_seq[0] == min_seq[1] && swappiness != 200 &&
		(swappiness == 1 || get_type_to_scan(lruvec, swappiness, &tier)));

	if (tier == -1)
		tier = get_tier_to_isolate(lruvec, type);

	for (i = !swappiness; i < ANON_AND_FILE; i++) {
		isolated = scan_pages(lruvec, sc, nr_to_scan, type, tier, list);
		if (isolated >= 0)
			break;

		type = !type;
		tier = get_tier_to_isolate(lruvec, type);
	}

	if (isolated < 0)
		isolated = *nr_to_scan = 0;

	*type_to_scan = type;

	return isolated;
}

/* Main function used by the foreground, the background and the user-triggered eviction. */
static bool evict_pages(struct lruvec *lruvec, struct scan_control *sc, int swappiness,
			long *nr_to_scan)
{
	int type;
	int isolated;
	int reclaimed;
	LIST_HEAD(list);
	struct page *page;
	enum vm_event_item item;
	struct reclaim_stat stat;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	spin_lock_irq(&pgdat->lru_lock);

	isolated = isolate_pages(lruvec, sc, swappiness, nr_to_scan, &type, &list);
	VM_BUG_ON(list_empty(&list) == !!isolated);

	if (isolated)
		__mod_node_page_state(pgdat, NR_ISOLATED_ANON + type, isolated);

	spin_unlock_irq(&pgdat->lru_lock);

	if (!isolated)
		goto done;

	reclaimed = shrink_page_list(&list, pgdat, sc, 0, &stat, false);
	/*
	 * We need to prevent rejected pages from being added back to the same
	 * lists they were isolated from. Otherwise we may risk looping on them
	 * forever. We use PageActive() or !PageReferenced() && PageWorkingset()
	 * to tell lru_gen_addition() not to add them to the oldest generation.
	 */
	list_for_each_entry(page, &list, lru) {
		if (PageMlocked(page))
			continue;

		if (page_mapped(page) && PageReferenced(page))
			SetPageActive(page);
		else if (!PageActive(page))
			SetPageWorkingset(page);
		ClearPageReferenced(page);
	}

	spin_lock_irq(&pgdat->lru_lock);

	move_pages_to_lru(lruvec, &list);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + type, -isolated);

	item = current_is_kswapd() ? PGSTEAL_KSWAPD : PGSTEAL_DIRECT;
	if (global_reclaim(sc))
		__count_vm_events(item, reclaimed);
	__count_memcg_events(lruvec_memcg(lruvec), item, reclaimed);

	spin_unlock_irq(&pgdat->lru_lock);

	mem_cgroup_uncharge_list(&list);
	free_unref_page_list(&list);

	sc->nr_reclaimed += reclaimed;
done:
	return *nr_to_scan > 0 && sc->nr_reclaimed < sc->nr_to_reclaim;
}

/******************************************************************************
 *                          page reclaim
 ******************************************************************************/

static int get_swappiness(struct lruvec *lruvec)
{
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	int swappiness = mem_cgroup_get_nr_swap_pages(memcg) >= (long)SWAP_CLUSTER_MAX ?
			 mem_cgroup_swappiness(memcg) : 0;

	VM_BUG_ON(swappiness > 200U);

	return swappiness;
}

static long get_nr_to_scan(struct lruvec *lruvec, struct scan_control *sc, int swappiness)
{
	int gen, type, zone;
	long nr_to_scan = 0;
	struct lrugen *lrugen = &lruvec->evictable;
	DEFINE_MAX_SEQ();
	DEFINE_MIN_SEQ();

	lru_add_drain();

	for (type = !swappiness; type < ANON_AND_FILE; type++) {
		unsigned long seq;

		for (seq = min_seq[type]; seq <= max_seq; seq++) {
			gen = lru_gen_from_seq(seq);

			for (zone = 0; zone <= sc->reclaim_idx; zone++)
				nr_to_scan += READ_ONCE(lrugen->sizes[gen][type][zone]);
		}
	}

	nr_to_scan = max(nr_to_scan, 0L);
	nr_to_scan = round_up(nr_to_scan >> sc->priority, SWAP_CLUSTER_MAX);

	if (get_hi_wmark(max_seq, min_seq, swappiness) > MIN_NR_GENS)
		return nr_to_scan;

	if (!arch_has_hw_pte_young()) {
		inc_max_seq(lruvec, max_seq);
		return nr_to_scan;
	}

	/* kswapd uses lru_gen_age_node() */
	if (current_is_kswapd())
		return 0;

	return walk_mm_list(lruvec, max_seq, sc, swappiness, NULL) ? nr_to_scan : 0;
}

static void lru_gen_shrink_lruvec(struct lruvec *lruvec, struct scan_control *sc)
{
	struct blk_plug plug;
	long scanned = 0;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	enum mem_cgroup_protection prot = mem_cgroup_protected(sc->target_mem_cgroup, memcg);

	blk_start_plug(&plug);

	while (true) {
		long nr_to_scan;
		int swappiness = sc->may_swap ? get_swappiness(lruvec) : 0;

		nr_to_scan = get_nr_to_scan(lruvec, sc, swappiness) - scanned;
		if (nr_to_scan < (long)SWAP_CLUSTER_MAX)
			break;

		scanned += nr_to_scan;

		if (!evict_pages(lruvec, sc, swappiness, &nr_to_scan))
			break;

		scanned -= nr_to_scan;

		if (prot == MEMCG_PROT_MIN || (prot == MEMCG_PROT_LOW && !sc->memcg_low_reclaim))
			break;

		cond_resched();
	}

	blk_finish_plug(&plug);
}

/******************************************************************************
 *                          the background aging
 ******************************************************************************/

static int lru_gen_spread = MIN_NR_GENS;

static void try_walk_mm_list(struct lruvec *lruvec, struct scan_control *sc)
{
	int gen, type, zone;
	long old_and_young[2] = {};
	int spread = READ_ONCE(lru_gen_spread);
	int swappiness = get_swappiness(lruvec);
	struct lrugen *lrugen = &lruvec->evictable;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	DEFINE_MAX_SEQ();
	DEFINE_MIN_SEQ();

	lru_add_drain();

	for (type = !swappiness; type < ANON_AND_FILE; type++) {
		unsigned long seq;

		for (seq = min_seq[type]; seq <= max_seq; seq++) {
			gen = lru_gen_from_seq(seq);

			for (zone = 0; zone < MAX_NR_ZONES; zone++)
				old_and_young[seq == max_seq] +=
					READ_ONCE(lrugen->sizes[gen][type][zone]);
		}
	}

	old_and_young[0] = max(old_and_young[0], 0L);
	old_and_young[1] = max(old_and_young[1], 0L);

	/* try to spread pages out across spread+1 generations */
	if (old_and_young[0] >= old_and_young[1] * spread &&
	    get_lo_wmark(max_seq, min_seq, swappiness) > max(spread, MIN_NR_GENS))
		return;

	walk_mm_list(lruvec, max_seq, sc, swappiness, pgdat->mm_walk_args);
}

static void lru_gen_age_node(struct pglist_data *pgdat, struct scan_control *sc)
{
	struct mem_cgroup *memcg;

	VM_BUG_ON(!current_is_kswapd());

	if (!arch_has_hw_pte_young())
		return;

	memcg = mem_cgroup_iter(NULL, NULL, NULL);
	do {
		struct lruvec *lruvec = mem_cgroup_lruvec(pgdat, memcg);
		enum mem_cgroup_protection prot = mem_cgroup_protected(sc->target_mem_cgroup,
								       memcg);

		if (prot != MEMCG_PROT_MIN && (prot != MEMCG_PROT_LOW || sc->memcg_low_reclaim))
			try_walk_mm_list(lruvec, sc);

		cond_resched();
	} while ((memcg = mem_cgroup_iter(NULL, memcg, NULL)));
}

/******************************************************************************
 *                          state change
 ******************************************************************************/

#ifdef CONFIG_LRU_GEN_ENABLED
DEFINE_STATIC_KEY_TRUE(lru_gen_static_key);
#else
DEFINE_STATIC_KEY_FALSE(lru_gen_static_key);
#endif

static DEFINE_MUTEX(lru_gen_state_mutex);
static int lru_gen_nr_swapfiles __read_mostly;

static bool __maybe_unused state_is_valid(struct lruvec *lruvec)
{
	int gen, type, zone;
	enum lru_list lru;
	struct lrugen *lrugen = &lruvec->evictable;

	for_each_evictable_lru(lru) {
		type = is_file_lru(lru);

		if (lrugen->enabled[type] && !list_empty(&lruvec->lists[lru]))
			return false;
	}

	for_each_gen_type_zone(gen, type, zone) {
		if (!lrugen->enabled[type] && !list_empty(&lrugen->lists[gen][type][zone]))
			return false;

		VM_WARN_ON_ONCE(!lrugen->enabled[type] && lrugen->sizes[gen][type][zone]);
	}

	return true;
}

static bool fill_lru_gen_lists(struct lruvec *lruvec)
{
	enum lru_list lru;
	int batch_size = 0;

	for_each_evictable_lru(lru) {
		int type = is_file_lru(lru);
		bool active = is_active_lru(lru);
		struct list_head *head = &lruvec->lists[lru];

		if (!lruvec->evictable.enabled[type])
			continue;

		while (!list_empty(head)) {
			bool success;
			struct page *page = lru_to_page(head);

			VM_BUG_ON_PAGE(PageTail(page), page);
			VM_BUG_ON_PAGE(PageUnevictable(page), page);
			VM_BUG_ON_PAGE(PageActive(page) != active, page);
			VM_BUG_ON_PAGE(page_lru_gen(page) != -1, page);
			VM_BUG_ON_PAGE(page_is_file_cache(page) != type, page);

			prefetchw_prev_lru_page(page, head, flags);

			del_page_from_lru_list(page, lruvec);
			success = lru_gen_addition(page, lruvec, true);
			VM_BUG_ON(!success);

			if (++batch_size == MAX_BATCH_SIZE)
				return false;
		}
	}

	return true;
}

static bool drain_lru_gen_lists(struct lruvec *lruvec)
{
	int gen, type, zone;
	int batch_size = 0;

	for_each_gen_type_zone(gen, type, zone) {
		struct list_head *head = &lruvec->evictable.lists[gen][type][zone];

		if (lruvec->evictable.enabled[type])
			continue;

		while (!list_empty(head)) {
			bool success;
			struct page *page = lru_to_page(head);

			VM_BUG_ON_PAGE(PageTail(page), page);
			VM_BUG_ON_PAGE(PageUnevictable(page), page);
			VM_BUG_ON_PAGE(PageActive(page), page);
			VM_BUG_ON_PAGE(page_is_file_cache(page) != type, page);
			VM_BUG_ON_PAGE(page_zonenum(page) != zone, page);

			prefetchw_prev_lru_page(page, head, flags);

			success = lru_gen_deletion(page, lruvec);
			VM_BUG_ON(!success);
			add_page_to_lru_list(page, lruvec);

			if (++batch_size == MAX_BATCH_SIZE)
				return false;
		}
	}

	return true;
}

/*
 * For file page tracking, we enable/disable it according to the main switch.
 * For anon page tracking, we only enabled it when the main switch is on and
 * there is at least one swapfile; we disable it when there are no swapfiles
 * regardless of the value of the main switch. Otherwise, we will eventually
 * reach the max size of the sliding window and have to call inc_min_seq(),
 * which brings an unnecessary overhead.
 */
void lru_gen_set_state(bool enable, bool main, bool swap)
{
	struct mem_cgroup *memcg;

	mem_hotplug_begin();
	mutex_lock(&lru_gen_state_mutex);
	cgroup_lock();

	main = main && enable != lru_gen_enabled();
	swap = swap && !(enable ? lru_gen_nr_swapfiles++ : --lru_gen_nr_swapfiles);
	swap = swap && lru_gen_enabled();
	if (!main && !swap)
		goto unlock;

	if (main) {
		if (enable)
			static_branch_enable(&lru_gen_static_key);
		else
			static_branch_disable(&lru_gen_static_key);
	}

	memcg = mem_cgroup_iter(NULL, NULL, NULL);
	do {
		int nid;

		for_each_node_state(nid, N_MEMORY) {
			struct pglist_data *pgdat = NODE_DATA(nid);
			struct lruvec *lruvec = mem_cgroup_lruvec(pgdat, memcg);
			struct lrugen *lrugen = &lruvec->evictable;

			spin_lock_irq(&pgdat->lru_lock);

			VM_BUG_ON(!seq_is_valid(lruvec));
			VM_BUG_ON(!state_is_valid(lruvec));

			WRITE_ONCE(lrugen->enabled[0], lru_gen_enabled() && lru_gen_nr_swapfiles);
			WRITE_ONCE(lrugen->enabled[1], lru_gen_enabled());

			while (!(enable ? fill_lru_gen_lists(lruvec) :
					  drain_lru_gen_lists(lruvec))) {
				spin_unlock_irq(&pgdat->lru_lock);
				cond_resched();
				spin_lock_irq(&pgdat->lru_lock);
			}

			spin_unlock_irq(&pgdat->lru_lock);
		}

		cond_resched();
	} while ((memcg = mem_cgroup_iter(NULL, memcg, NULL)));
unlock:
	cgroup_unlock();
	mutex_unlock(&lru_gen_state_mutex);
	mem_hotplug_done();
}

static int __meminit __maybe_unused lru_gen_online_mem(struct notifier_block *self,
						       unsigned long action, void *arg)
{
	struct mem_cgroup *memcg;
	struct memory_notify *mnb = arg;
	int nid = mnb->status_change_nid;

	if (action != MEM_GOING_ONLINE || nid == NUMA_NO_NODE)
		return NOTIFY_DONE;

	mutex_lock(&lru_gen_state_mutex);
	cgroup_lock();

	memcg = mem_cgroup_iter(NULL, NULL, NULL);
	do {
		struct lruvec *lruvec = mem_cgroup_lruvec(NODE_DATA(nid), memcg);
		struct lrugen *lrugen = &lruvec->evictable;

		VM_BUG_ON(!seq_is_valid(lruvec));
		VM_BUG_ON(!state_is_valid(lruvec));

		WRITE_ONCE(lrugen->enabled[0], lru_gen_enabled() && lru_gen_nr_swapfiles);
		WRITE_ONCE(lrugen->enabled[1], lru_gen_enabled());
	} while ((memcg = mem_cgroup_iter(NULL, memcg, NULL)));

	cgroup_unlock();
	mutex_unlock(&lru_gen_state_mutex);

	return NOTIFY_DONE;
}

static void lru_gen_start_kswapd(int nid)
{
	struct pglist_data *pgdat = NODE_DATA(nid);

	pgdat->mm_walk_args = kvzalloc_node(size_of_mm_walk_args(), GFP_KERNEL, nid);
	WARN_ON_ONCE(!pgdat->mm_walk_args);
}

static void lru_gen_stop_kswapd(int nid)
{
	struct pglist_data *pgdat = NODE_DATA(nid);

	kvfree(pgdat->mm_walk_args);
}

/******************************************************************************
 *                          sysfs interface
 ******************************************************************************/

static ssize_t show_lru_gen_spread(struct kobject *kobj, struct kobj_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%d\n", READ_ONCE(lru_gen_spread));
}

static ssize_t store_lru_gen_spread(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t len)
{
	int spread;

	if (kstrtoint(buf, 10, &spread) || spread >= MAX_NR_GENS)
		return -EINVAL;

	WRITE_ONCE(lru_gen_spread, spread);

	return len;
}

static struct kobj_attribute lru_gen_spread_attr = __ATTR(
	spread, 0644, show_lru_gen_spread, store_lru_gen_spread
);

static ssize_t show_lru_gen_enabled(struct kobject *kobj, struct kobj_attribute *attr,
				    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lru_gen_enabled());
}

static ssize_t store_lru_gen_enabled(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t len)
{
	int enable;

	if (kstrtoint(buf, 10, &enable))
		return -EINVAL;

	lru_gen_set_state(enable, true, false);

	return len;
}

static struct kobj_attribute lru_gen_enabled_attr = __ATTR(
	enabled, 0644, show_lru_gen_enabled, store_lru_gen_enabled
);

static struct attribute *lru_gen_attrs[] = {
	&lru_gen_spread_attr.attr,
	&lru_gen_enabled_attr.attr,
	NULL
};

static struct attribute_group lru_gen_attr_group = {
	.name = "lru_gen",
	.attrs = lru_gen_attrs,
};

/******************************************************************************
 *                          debugfs interface
 ******************************************************************************/

static void *lru_gen_seq_start(struct seq_file *m, loff_t *pos)
{
	struct mem_cgroup *memcg;
	loff_t nr_to_skip = *pos;

	m->private = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!m->private)
		return ERR_PTR(-ENOMEM);

	memcg = mem_cgroup_iter(NULL, NULL, NULL);
	do {
		int nid;

		for_each_node_state(nid, N_MEMORY) {
			if (!nr_to_skip--)
				return mem_cgroup_lruvec(NODE_DATA(nid), memcg);
		}
	} while ((memcg = mem_cgroup_iter(NULL, memcg, NULL)));

	return NULL;
}

static void lru_gen_seq_stop(struct seq_file *m, void *v)
{
	if (!IS_ERR_OR_NULL(v))
		mem_cgroup_iter_break(NULL, lruvec_memcg(v));

	kfree(m->private);
	m->private = NULL;
}

static void *lru_gen_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	int nid = lruvec_pgdat(v)->node_id;
	struct mem_cgroup *memcg = lruvec_memcg(v);

	++*pos;

	nid = next_memory_node(nid);
	if (nid == MAX_NUMNODES) {
		memcg = mem_cgroup_iter(NULL, memcg, NULL);
		if (!memcg)
			return NULL;

		nid = first_memory_node;
	}

	return mem_cgroup_lruvec(NODE_DATA(nid), memcg);
}

static void lru_gen_seq_show_full(struct seq_file *m, struct lruvec *lruvec,
				  unsigned long max_seq, unsigned long *min_seq,
				  unsigned long seq)
{
	int i;
	int type, tier;
	int hist = hist_from_seq_or_gen(seq);
	struct lrugen *lrugen = &lruvec->evictable;
	int nid = lruvec_pgdat(lruvec)->node_id;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	struct lru_gen_mm_list *mm_list = get_mm_list(memcg);

	for (tier = 0; tier < MAX_NR_TIERS; tier++) {
		seq_printf(m, "            %10d", tier);
		for (type = 0; type < ANON_AND_FILE; type++) {
			unsigned long n[3] = {};

			if (seq == max_seq) {
				n[0] = READ_ONCE(lrugen->avg_refaulted[type][tier]);
				n[1] = READ_ONCE(lrugen->avg_total[type][tier]);

				seq_printf(m, " %10luR %10luT %10lu ", n[0], n[1], n[2]);
			} else if (seq == min_seq[type] || NR_STAT_GENS > 1) {
				n[0] = atomic_long_read(&lrugen->refaulted[hist][type][tier]);
				n[1] = atomic_long_read(&lrugen->evicted[hist][type][tier]);
				if (tier)
					n[2] = READ_ONCE(lrugen->activated[hist][type][tier - 1]);

				seq_printf(m, " %10lur %10lue %10lua", n[0], n[1], n[2]);
			} else
				seq_puts(m, "          0           0           0 ");
		}
		seq_putc(m, '\n');
	}

	seq_puts(m, "                      ");
	for (i = 0; i < NR_MM_STATS; i++) {
		if (seq == max_seq && NR_STAT_GENS == 1)
			seq_printf(m, " %10lu%c", READ_ONCE(mm_list->nodes[nid].stats[hist][i]),
				   toupper(MM_STAT_CODES[i]));
		else if (seq != max_seq && NR_STAT_GENS > 1)
			seq_printf(m, " %10lu%c", READ_ONCE(mm_list->nodes[nid].stats[hist][i]),
				   MM_STAT_CODES[i]);
		else
			seq_puts(m, "          0 ");
	}
	seq_putc(m, '\n');
}

static int lru_gen_seq_show(struct seq_file *m, void *v)
{
	unsigned long seq;
	bool full = !debugfs_real_fops(m->file)->write;
	struct lruvec *lruvec = v;
	struct lrugen *lrugen = &lruvec->evictable;
	int nid = lruvec_pgdat(lruvec)->node_id;
	struct mem_cgroup *memcg = lruvec_memcg(lruvec);
	DEFINE_MAX_SEQ();
	DEFINE_MIN_SEQ();

	if (nid == first_memory_node) {
#ifdef CONFIG_MEMCG
		if (memcg)
			cgroup_path(memcg->css.cgroup, m->private, PATH_MAX);
#endif
		seq_printf(m, "memcg %5hu %s\n", mem_cgroup_id(memcg), (char *)m->private);
	}

	seq_printf(m, " node %5d\n", nid);

	seq = full ? (max_seq < MAX_NR_GENS ? 0 : max_seq - MAX_NR_GENS + 1) :
		     min(min_seq[0], min_seq[1]);

	for (; seq <= max_seq; seq++) {
		int gen, type, zone;
		unsigned int msecs;

		gen = lru_gen_from_seq(seq);
		msecs = jiffies_to_msecs(jiffies - READ_ONCE(lrugen->timestamps[gen]));

		seq_printf(m, " %10lu %10u", seq, msecs);

		for (type = 0; type < ANON_AND_FILE; type++) {
			long size = 0;

			if (seq < min_seq[type]) {
				seq_puts(m, "         -0 ");
				continue;
			}

			for (zone = 0; zone < MAX_NR_ZONES; zone++)
				size += READ_ONCE(lrugen->sizes[gen][type][zone]);

			seq_printf(m, " %10lu ", max(size, 0L));
		}

		seq_putc(m, '\n');

		if (full)
			lru_gen_seq_show_full(m, lruvec, max_seq, min_seq, seq);
	}

	return 0;
}

static const struct seq_operations lru_gen_seq_ops = {
	.start = lru_gen_seq_start,
	.stop = lru_gen_seq_stop,
	.next = lru_gen_seq_next,
	.show = lru_gen_seq_show,
};

static int advance_max_seq(struct lruvec *lruvec, unsigned long seq, int swappiness)
{
	struct scan_control sc = {
		.target_mem_cgroup = lruvec_memcg(lruvec),
	};
	DEFINE_MAX_SEQ();

	if (seq == max_seq)
		walk_mm_list(lruvec, max_seq, &sc, swappiness, NULL);

	return seq > max_seq ? -EINVAL : 0;
}

static int advance_min_seq(struct lruvec *lruvec, unsigned long seq, int swappiness,
			   unsigned long nr_to_reclaim)
{
	struct blk_plug plug;
	int err = -EINTR;
	long nr_to_scan = LONG_MAX;
	struct scan_control sc = {
		.nr_to_reclaim = nr_to_reclaim,
		.target_mem_cgroup = lruvec_memcg(lruvec),
		.may_writepage = 1,
		.may_unmap = 1,
		.may_swap = 1,
		.reclaim_idx = MAX_NR_ZONES - 1,
		.gfp_mask = GFP_KERNEL,
	};
	DEFINE_MAX_SEQ();

	if (seq >= max_seq - 1)
		return -EINVAL;

	blk_start_plug(&plug);

	while (!signal_pending(current)) {
		DEFINE_MIN_SEQ();

		if (seq < min(min_seq[!swappiness], min_seq[swappiness < 200]) ||
		    !evict_pages(lruvec, &sc, swappiness, &nr_to_scan)) {
			err = 0;
			break;
		}

		cond_resched();
	}

	blk_finish_plug(&plug);

	return err;
}

static int advance_seq(char cmd, int memcg_id, int nid, unsigned long seq,
		       int swappiness, unsigned long nr_to_reclaim)
{
	struct lruvec *lruvec;
	int err = -EINVAL;
	struct mem_cgroup *memcg = NULL;

	if (!mem_cgroup_disabled()) {
		rcu_read_lock();
		memcg = mem_cgroup_from_id(memcg_id);
#ifdef CONFIG_MEMCG
		if (memcg && !css_tryget(&memcg->css))
			memcg = NULL;
#endif
		rcu_read_unlock();

		if (!memcg)
			goto done;
	}
	if (memcg_id != mem_cgroup_id(memcg))
		goto done;

	if (nid < 0 || nid >= MAX_NUMNODES || !node_state(nid, N_MEMORY))
		goto done;

	lruvec = mem_cgroup_lruvec(NODE_DATA(nid), memcg);

	if (swappiness == -1)
		swappiness = get_swappiness(lruvec);
	else if (swappiness > 200U)
		goto done;

	switch (cmd) {
	case '+':
		err = advance_max_seq(lruvec, seq, swappiness);
		break;
	case '-':
		err = advance_min_seq(lruvec, seq, swappiness, nr_to_reclaim);
		break;
	}
done:
	mem_cgroup_put(memcg);

	return err;
}

static ssize_t lru_gen_seq_write(struct file *file, const char __user *src,
				 size_t len, loff_t *pos)
{
	void *buf;
	char *cur, *next;
	int err = 0;

	buf = kvmalloc(len + 1, GFP_USER);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, src, len)) {
		kvfree(buf);
		return -EFAULT;
	}

	next = buf;
	next[len] = '\0';

	while ((cur = strsep(&next, ",;\n"))) {
		int n;
		int end;
		char cmd;
		unsigned int memcg_id;
		unsigned int nid;
		unsigned long seq;
		unsigned int swappiness = -1;
		unsigned long nr_to_reclaim = -1;

		cur = skip_spaces(cur);
		if (!*cur)
			continue;

		n = sscanf(cur, "%c %u %u %lu %n %u %n %lu %n", &cmd, &memcg_id, &nid,
			   &seq, &end, &swappiness, &end, &nr_to_reclaim, &end);
		if (n < 4 || cur[end]) {
			err = -EINVAL;
			break;
		}

		err = advance_seq(cmd, memcg_id, nid, seq, swappiness, nr_to_reclaim);
		if (err)
			break;
	}

	kvfree(buf);

	return err ? : len;
}

static int lru_gen_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &lru_gen_seq_ops);
}

static const struct file_operations lru_gen_rw_fops = {
	.open = lru_gen_seq_open,
	.read = seq_read,
	.write = lru_gen_seq_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct file_operations lru_gen_ro_fops = {
	.open = lru_gen_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/******************************************************************************
 *                          initialization
 ******************************************************************************/

void lru_gen_init_lruvec(struct lruvec *lruvec)
{
	int i;
	int gen, type, zone;
	struct lrugen *lrugen = &lruvec->evictable;

	lrugen->max_seq = MIN_NR_GENS + 1;
	lrugen->enabled[0] = lru_gen_enabled() && lru_gen_nr_swapfiles;
	lrugen->enabled[1] = lru_gen_enabled();

	for (i = 0; i <= MIN_NR_GENS + 1; i++)
		lrugen->timestamps[i] = jiffies;

	for_each_gen_type_zone(gen, type, zone)
		INIT_LIST_HEAD(&lrugen->lists[gen][type][zone]);
}

static int __init init_lru_gen(void)
{
	BUILD_BUG_ON(MIN_NR_GENS + 1 >= MAX_NR_GENS);
	BUILD_BUG_ON(BIT(LRU_GEN_WIDTH) <= MAX_NR_GENS);
	BUILD_BUG_ON(sizeof(MM_STAT_CODES) != NR_MM_STATS + 1);

	VM_BUG_ON(PMD_SIZE / PAGE_SIZE != PTRS_PER_PTE);
	VM_BUG_ON(PUD_SIZE / PMD_SIZE != PTRS_PER_PMD);
	VM_BUG_ON(P4D_SIZE / PUD_SIZE != PTRS_PER_PUD);

	if (mem_cgroup_disabled()) {
		global_mm_list = alloc_mm_list();
		if (WARN_ON_ONCE(!global_mm_list))
			return -ENOMEM;
	}

	if (hotplug_memory_notifier(lru_gen_online_mem, 0))
		pr_err("lru_gen: failed to subscribe hotplug notifications\n");

	if (sysfs_create_group(mm_kobj, &lru_gen_attr_group))
		pr_err("lru_gen: failed to create sysfs group\n");

	debugfs_create_file("lru_gen", 0644, NULL, NULL, &lru_gen_rw_fops);
	debugfs_create_file("lru_gen_full", 0444, NULL, NULL, &lru_gen_ro_fops);

	return 0;
};
/*
 * We want to run as early as possible because debug code may call mm_alloc()
 * and mmput(). Our only dependency mm_kobj is initialized one stage earlier.
 */
arch_initcall(init_lru_gen);

#else /* CONFIG_LRU_GEN */

static void lru_gen_shrink_lruvec(struct lruvec *lruvec, struct scan_control *sc)
{
}

static void lru_gen_age_node(struct pglist_data *pgdat, struct scan_control *sc)
{
}

static void lru_gen_start_kswapd(int nid)
{
}

static void lru_gen_stop_kswapd(int nid)
{
}

#endif /* CONFIG_LRU_GEN */

/*
 * This is a basic per-node page freer.  Used by both kswapd and direct reclaim.
 */
static void shrink_node_memcg(struct pglist_data *pgdat, struct mem_cgroup *memcg,
			      struct scan_control *sc, unsigned long *lru_pages)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(pgdat, memcg);
	unsigned long nr[NR_LRU_LISTS];
	unsigned long targets[NR_LRU_LISTS];
	unsigned long nr_to_scan;
	enum lru_list lru;
	unsigned long nr_reclaimed = 0;
	unsigned long nr_to_reclaim = sc->nr_to_reclaim;
	struct blk_plug plug;
	bool scan_adjusted;

	if (lru_gen_enabled()) {
		*lru_pages = 0;
		lru_gen_shrink_lruvec(lruvec, sc);
		return;
	}

	get_scan_count(lruvec, memcg, sc, nr, lru_pages);

	/* Record the original scan target for proportional adjustments later */
	memcpy(targets, nr, sizeof(nr));

	/*
	 * Global reclaiming within direct reclaim at DEF_PRIORITY is a normal
	 * event that can occur when there is little memory pressure e.g.
	 * multiple streaming readers/writers. Hence, we do not abort scanning
	 * when the requested number of pages are reclaimed when scanning at
	 * DEF_PRIORITY on the assumption that the fact we are direct
	 * reclaiming implies that kswapd is not keeping up and it is best to
	 * do a batch of work at once. For memcg reclaim one check is made to
	 * abort proportional reclaim if either the file or anon lru has already
	 * dropped to zero at the first pass.
	 */
	scan_adjusted = (global_reclaim(sc) && !current_is_kswapd() &&
			 sc->priority == DEF_PRIORITY);

	blk_start_plug(&plug);
	while (nr[LRU_INACTIVE_ANON] || nr[LRU_ACTIVE_FILE] ||
					nr[LRU_INACTIVE_FILE]) {
		unsigned long nr_anon, nr_file, percentage;
		unsigned long nr_scanned;

		for_each_evictable_lru(lru) {
			if (nr[lru]) {
				nr_to_scan = min(nr[lru], SWAP_CLUSTER_MAX);
				nr[lru] -= nr_to_scan;

				nr_reclaimed += shrink_list(lru, nr_to_scan,
							    lruvec, sc);
			}
		}

		cond_resched();

		if (nr_reclaimed < nr_to_reclaim || scan_adjusted)
			continue;

		/*
		 * For kswapd and memcg, reclaim at least the number of pages
		 * requested. Ensure that the anon and file LRUs are scanned
		 * proportionally what was requested by get_scan_count(). We
		 * stop reclaiming one LRU and reduce the amount scanning
		 * proportional to the original scan target.
		 */
		nr_file = nr[LRU_INACTIVE_FILE] + nr[LRU_ACTIVE_FILE];
		nr_anon = nr[LRU_INACTIVE_ANON] + nr[LRU_ACTIVE_ANON];

		/*
		 * It's just vindictive to attack the larger once the smaller
		 * has gone to zero.  And given the way we stop scanning the
		 * smaller below, this makes sure that we only make one nudge
		 * towards proportionality once we've got nr_to_reclaim.
		 */
		if (!nr_file || !nr_anon)
			break;

		if (nr_file > nr_anon) {
			unsigned long scan_target = targets[LRU_INACTIVE_ANON] +
						targets[LRU_ACTIVE_ANON] + 1;
			lru = LRU_BASE;
			percentage = nr_anon * 100 / scan_target;
		} else {
			unsigned long scan_target = targets[LRU_INACTIVE_FILE] +
						targets[LRU_ACTIVE_FILE] + 1;
			lru = LRU_FILE;
			percentage = nr_file * 100 / scan_target;
		}

		/* Stop scanning the smaller of the LRU */
		nr[lru] = 0;
		nr[lru + LRU_ACTIVE] = 0;

		/*
		 * Recalculate the other LRU scan count based on its original
		 * scan target and the percentage scanning already complete
		 */
		lru = (lru == LRU_FILE) ? LRU_BASE : LRU_FILE;
		nr_scanned = targets[lru] - nr[lru];
		nr[lru] = targets[lru] * (100 - percentage) / 100;
		nr[lru] -= min(nr[lru], nr_scanned);

		lru += LRU_ACTIVE;
		nr_scanned = targets[lru] - nr[lru];
		nr[lru] = targets[lru] * (100 - percentage) / 100;
		nr[lru] -= min(nr[lru], nr_scanned);

		scan_adjusted = true;
	}
	blk_finish_plug(&plug);
	sc->nr_reclaimed += nr_reclaimed;

	/*
	 * Even if we did not try to evict anon pages at all, we want to
	 * rebalance the anon lru active/inactive ratio.
	 */
	if (inactive_list_is_low(lruvec, false, sc, true))
		shrink_active_list(SWAP_CLUSTER_MAX, lruvec,
				   sc, LRU_ACTIVE_ANON);
}

/* Use reclaim/compaction for costly allocs or under memory pressure */
static bool in_reclaim_compaction(struct scan_control *sc)
{
	if (IS_ENABLED(CONFIG_COMPACTION) && sc->order &&
			(sc->order > PAGE_ALLOC_COSTLY_ORDER ||
			 sc->priority < DEF_PRIORITY - 2))
		return true;

	return false;
}

/*
 * Reclaim/compaction is used for high-order allocation requests. It reclaims
 * order-0 pages before compacting the zone. should_continue_reclaim() returns
 * true if more pages should be reclaimed such that when the page allocator
 * calls try_to_compact_zone() that it will have enough free pages to succeed.
 * It will give up earlier than that if there is difficulty reclaiming pages.
 */
static inline bool should_continue_reclaim(struct pglist_data *pgdat,
					unsigned long nr_reclaimed,
					struct scan_control *sc)
{
	unsigned long pages_for_compaction;
	unsigned long inactive_lru_pages;
	int z;

	/* If not in reclaim/compaction mode, stop */
	if (!in_reclaim_compaction(sc))
		return false;

	/*
	 * Stop if we failed to reclaim any pages from the last SWAP_CLUSTER_MAX
	 * number of pages that were scanned. This will return to the caller
	 * with the risk reclaim/compaction and the resulting allocation attempt
	 * fails. In the past we have tried harder for __GFP_RETRY_MAYFAIL
	 * allocations through requiring that the full LRU list has been scanned
	 * first, by assuming that zero delta of sc->nr_scanned means full LRU
	 * scan, but that approximation was wrong, and there were corner cases
	 * where always a non-zero amount of pages were scanned.
	 */
	if (!nr_reclaimed)
		return false;

	/* If compaction would go ahead or the allocation would succeed, stop */
	for (z = 0; z <= sc->reclaim_idx; z++) {
		struct zone *zone = &pgdat->node_zones[z];
		if (!managed_zone(zone))
			continue;

		switch (compaction_suitable(zone, sc->order, 0, sc->reclaim_idx)) {
		case COMPACT_SUCCESS:
		case COMPACT_CONTINUE:
			return false;
		default:
			/* check next zone */
			;
		}
	}

	/*
	 * If we have not reclaimed enough pages for compaction and the
	 * inactive lists are large enough, continue reclaiming
	 */
	pages_for_compaction = compact_gap(sc->order);
	inactive_lru_pages = node_page_state(pgdat, NR_INACTIVE_FILE);
	if (get_nr_swap_pages() > 0)
		inactive_lru_pages += node_page_state(pgdat, NR_INACTIVE_ANON);

	return inactive_lru_pages > pages_for_compaction;
}

static bool pgdat_memcg_congested(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	return test_bit(PGDAT_CONGESTED, &pgdat->flags) ||
		(memcg && memcg_congested(pgdat, memcg));
}

static bool shrink_node(pg_data_t *pgdat, struct scan_control *sc)
{
	struct reclaim_state *reclaim_state = current->reclaim_state;
	unsigned long nr_reclaimed, nr_scanned;
	bool reclaimable = false;

	do {
		struct mem_cgroup *root = sc->target_mem_cgroup;
		unsigned long node_lru_pages = 0;
		struct mem_cgroup *memcg;

		memset(&sc->nr, 0, sizeof(sc->nr));

		nr_reclaimed = sc->nr_reclaimed;
		nr_scanned = sc->nr_scanned;

		memcg = mem_cgroup_iter(root, NULL, NULL);
		do {
			unsigned long lru_pages;
			unsigned long reclaimed;
			unsigned long scanned;

			/*
			 * This loop can become CPU-bound when target memcgs
			 * aren't eligible for reclaim - either because they
			 * don't have any reclaimable pages, or because their
			 * memory is explicitly protected. Avoid soft lockups.
			 */
			cond_resched();

			switch (mem_cgroup_protected(root, memcg)) {
			case MEMCG_PROT_MIN:
				/*
				 * Hard protection.
				 * If there is no reclaimable memory, OOM.
				 */
				continue;
			case MEMCG_PROT_LOW:
				/*
				 * Soft protection.
				 * Respect the protection only as long as
				 * there is an unprotected supply
				 * of reclaimable memory from other cgroups.
				 */
				if (!sc->memcg_low_reclaim) {
					sc->memcg_low_skipped = 1;
					continue;
				}
				memcg_memory_event(memcg, MEMCG_LOW);
				break;
			case MEMCG_PROT_NONE:
				/*
				 * All protection thresholds breached. We may
				 * still choose to vary the scan pressure
				 * applied based on by how much the cgroup in
				 * question has exceeded its protection
				 * thresholds (see get_scan_count).
				 */
				break;
			}

			reclaimed = sc->nr_reclaimed;
			scanned = sc->nr_scanned;
			shrink_node_memcg(pgdat, memcg, sc, &lru_pages);
			node_lru_pages += lru_pages;

			shrink_slab(sc->gfp_mask, pgdat->node_id, memcg,
					sc->priority);

			/* Record the group's reclaim efficiency */
			vmpressure(sc->gfp_mask, memcg, false,
				   sc->nr_scanned - scanned,
				   sc->nr_reclaimed - reclaimed);

		} while ((memcg = mem_cgroup_iter(root, memcg, NULL)));

		if (reclaim_state) {
			sc->nr_reclaimed += reclaim_state->reclaimed_slab;
			reclaim_state->reclaimed_slab = 0;
		}

		/* Record the subtree's reclaim efficiency */
		vmpressure(sc->gfp_mask, sc->target_mem_cgroup, true,
			   sc->nr_scanned - nr_scanned,
			   sc->nr_reclaimed - nr_reclaimed);

		if (sc->nr_reclaimed - nr_reclaimed)
			reclaimable = true;

		if (current_is_kswapd()) {
			/*
			 * If reclaim is isolating dirty pages under writeback,
			 * it implies that the long-lived page allocation rate
			 * is exceeding the page laundering rate. Either the
			 * global limits are not being effective at throttling
			 * processes due to the page distribution throughout
			 * zones or there is heavy usage of a slow backing
			 * device. The only option is to throttle from reclaim
			 * context which is not ideal as there is no guarantee
			 * the dirtying process is throttled in the same way
			 * balance_dirty_pages() manages.
			 *
			 * Once a node is flagged PGDAT_WRITEBACK, kswapd will
			 * count the number of pages under pages flagged for
			 * immediate reclaim and stall if any are encountered
			 * in the nr_immediate check below.
			 */
			if (sc->nr.writeback && sc->nr.writeback == sc->nr.taken)
				set_bit(PGDAT_WRITEBACK, &pgdat->flags);

			/*
			 * Tag a node as congested if all the dirty pages
			 * scanned were backed by a congested BDI and
			 * wait_iff_congested will stall.
			 */
			if (sc->nr.dirty && sc->nr.dirty == sc->nr.congested)
				set_bit(PGDAT_CONGESTED, &pgdat->flags);

			/* Allow kswapd to start writing pages during reclaim.*/
			if (sc->nr.unqueued_dirty == sc->nr.file_taken)
				set_bit(PGDAT_DIRTY, &pgdat->flags);

			/*
			 * If kswapd scans pages marked marked for immediate
			 * reclaim and under writeback (nr_immediate), it
			 * implies that pages are cycling through the LRU
			 * faster than they are written so also forcibly stall.
			 */
			if (sc->nr.immediate)
				congestion_wait(BLK_RW_ASYNC, HZ/10);
		}

		/*
		 * Legacy memcg will stall in page writeback so avoid forcibly
		 * stalling in wait_iff_congested().
		 */
		if (!global_reclaim(sc) && sane_reclaim(sc) &&
		    sc->nr.dirty && sc->nr.dirty == sc->nr.congested)
			set_memcg_congestion(pgdat, root, true);

		/*
		 * Stall direct reclaim for IO completions if underlying BDIs
		 * and node is congested. Allow kswapd to continue until it
		 * starts encountering unqueued dirty pages or cycling through
		 * the LRU too quickly.
		 */
		if (!sc->hibernation_mode && !current_is_kswapd() &&
		   current_may_throttle() && pgdat_memcg_congested(pgdat, root))
			wait_iff_congested(BLK_RW_ASYNC, HZ/10);

	} while (should_continue_reclaim(pgdat, sc->nr_reclaimed - nr_reclaimed,
					 sc));

	/*
	 * Kswapd gives up on balancing particular nodes after too
	 * many failures to reclaim anything from them and goes to
	 * sleep. On reclaim progress, reset the failure counter. A
	 * successful direct reclaim run will revive a dormant kswapd.
	 */
	if (reclaimable)
		pgdat->kswapd_failures = 0;

	return reclaimable;
}

/*
 * Returns true if compaction should go ahead for a costly-order request, or
 * the allocation would already succeed without compaction. Return false if we
 * should reclaim first.
 */
static inline bool compaction_ready(struct zone *zone, struct scan_control *sc)
{
	unsigned long watermark;
	enum compact_result suitable;

	suitable = compaction_suitable(zone, sc->order, 0, sc->reclaim_idx);
	if (suitable == COMPACT_SUCCESS)
		/* Allocation should succeed already. Don't reclaim. */
		return true;
	if (suitable == COMPACT_SKIPPED)
		/* Compaction cannot yet proceed. Do reclaim. */
		return false;

	/*
	 * Compaction is already possible, but it takes time to run and there
	 * are potentially other callers using the pages just freed. So proceed
	 * with reclaim to make a buffer of free pages available to give
	 * compaction a reasonable chance of completing and allocating the page.
	 * Note that we won't actually reclaim the whole buffer in one attempt
	 * as the target watermark in should_continue_reclaim() is lower. But if
	 * we are already above the high+gap watermark, don't reclaim at all.
	 */
	watermark = high_wmark_pages(zone) + compact_gap(sc->order);

	return zone_watermark_ok_safe(zone, 0, watermark, sc->reclaim_idx);
}

/*
 * This is the direct reclaim path, for page-allocating processes.  We only
 * try to reclaim pages from zones which will satisfy the caller's allocation
 * request.
 *
 * If a zone is deemed to be full of pinned pages then just give it a light
 * scan then give up on it.
 */
static void shrink_zones(struct zonelist *zonelist, struct scan_control *sc)
{
	struct zoneref *z;
	struct zone *zone;
	unsigned long nr_soft_reclaimed;
	unsigned long nr_soft_scanned;
	gfp_t orig_mask;
	pg_data_t *last_pgdat = NULL;

	/*
	 * If the number of buffer_heads in the machine exceeds the maximum
	 * allowed level, force direct reclaim to scan the highmem zone as
	 * highmem pages could be pinning lowmem pages storing buffer_heads
	 */
	orig_mask = sc->gfp_mask;
	if (buffer_heads_over_limit) {
		sc->gfp_mask |= __GFP_HIGHMEM;
		sc->reclaim_idx = gfp_zone(sc->gfp_mask);
	}

	for_each_zone_zonelist_nodemask(zone, z, zonelist,
					sc->reclaim_idx, sc->nodemask) {
		/*
		 * Take care memory controller reclaiming has small influence
		 * to global LRU.
		 */
		if (global_reclaim(sc)) {
			if (!cpuset_zone_allowed(zone,
						 GFP_KERNEL | __GFP_HARDWALL))
				continue;

			/*
			 * If we already have plenty of memory free for
			 * compaction in this zone, don't free any more.
			 * Even though compaction is invoked for any
			 * non-zero order, only frequent costly order
			 * reclamation is disruptive enough to become a
			 * noticeable problem, like transparent huge
			 * page allocations.
			 */
			if (IS_ENABLED(CONFIG_COMPACTION) &&
			    sc->order > PAGE_ALLOC_COSTLY_ORDER &&
			    compaction_ready(zone, sc)) {
				sc->compaction_ready = true;
				continue;
			}

			/*
			 * Shrink each node in the zonelist once. If the
			 * zonelist is ordered by zone (not the default) then a
			 * node may be shrunk multiple times but in that case
			 * the user prefers lower zones being preserved.
			 */
			if (zone->zone_pgdat == last_pgdat)
				continue;

			/*
			 * This steals pages from memory cgroups over softlimit
			 * and returns the number of reclaimed pages and
			 * scanned pages. This works for global memory pressure
			 * and balancing, not for a memcg's limit.
			 */
			nr_soft_scanned = 0;
			nr_soft_reclaimed = mem_cgroup_soft_limit_reclaim(zone->zone_pgdat,
						sc->order, sc->gfp_mask,
						&nr_soft_scanned);
			sc->nr_reclaimed += nr_soft_reclaimed;
			sc->nr_scanned += nr_soft_scanned;
			/* need some check for avoid more shrink_zone() */
		}

		/* See comment about same check for global reclaim above */
		if (zone->zone_pgdat == last_pgdat)
			continue;
		last_pgdat = zone->zone_pgdat;
		shrink_node(zone->zone_pgdat, sc);
	}

	/*
	 * Restore to original mask to avoid the impact on the caller if we
	 * promoted it to __GFP_HIGHMEM.
	 */
	sc->gfp_mask = orig_mask;
}

static void snapshot_refaults(struct mem_cgroup *root_memcg, pg_data_t *pgdat)
{
	struct mem_cgroup *memcg;

	if (lru_gen_enabled())
		return;

	memcg = mem_cgroup_iter(root_memcg, NULL, NULL);
	do {
		unsigned long refaults;
		struct lruvec *lruvec;

		lruvec = mem_cgroup_lruvec(pgdat, memcg);
		refaults = lruvec_page_state_local(lruvec, WORKINGSET_ACTIVATE);
		lruvec->refaults = refaults;
	} while ((memcg = mem_cgroup_iter(root_memcg, memcg, NULL)));
}

/*
 * This is the main entry point to direct page reclaim.
 *
 * If a full scan of the inactive list fails to free enough memory then we
 * are "out of memory" and something needs to be killed.
 *
 * If the caller is !__GFP_FS then the probability of a failure is reasonably
 * high - the zone may be full of dirty or under-writeback pages, which this
 * caller can't do much about.  We kick the writeback threads and take explicit
 * naps in the hope that some of these pages can be written.  But if the
 * allocating task holds filesystem locks which prevent writeout this might not
 * work, and the allocation attempt will fail.
 *
 * returns:	0, if no pages reclaimed
 * 		else, the number of pages reclaimed
 */
static unsigned long do_try_to_free_pages(struct zonelist *zonelist,
					  struct scan_control *sc)
{
	int initial_priority = sc->priority;
	pg_data_t *last_pgdat;
	struct zoneref *z;
	struct zone *zone;
retry:
	delayacct_freepages_start();

	if (global_reclaim(sc))
		__count_zid_vm_events(ALLOCSTALL, sc->reclaim_idx, 1);

	do {
		vmpressure_prio(sc->gfp_mask, sc->target_mem_cgroup,
				sc->priority);
		sc->nr_scanned = 0;
		shrink_zones(zonelist, sc);

		if (sc->nr_reclaimed >= sc->nr_to_reclaim)
			break;

		if (sc->compaction_ready)
			break;

		/*
		 * If we're getting trouble reclaiming, start doing
		 * writepage even in laptop mode.
		 */
		if (sc->priority < DEF_PRIORITY - 2)
			sc->may_writepage = 1;
	} while (--sc->priority >= 0);

	last_pgdat = NULL;
	for_each_zone_zonelist_nodemask(zone, z, zonelist, sc->reclaim_idx,
					sc->nodemask) {
		if (zone->zone_pgdat == last_pgdat)
			continue;
		last_pgdat = zone->zone_pgdat;
		snapshot_refaults(sc->target_mem_cgroup, zone->zone_pgdat);
		set_memcg_congestion(last_pgdat, sc->target_mem_cgroup, false);
	}

	delayacct_freepages_end();

	if (sc->nr_reclaimed)
		return sc->nr_reclaimed;

	/* Aborted reclaim to try compaction? don't OOM, then */
	if (sc->compaction_ready)
		return 1;

	/* Untapped cgroup reserves?  Don't OOM, retry. */
	if (sc->memcg_low_skipped) {
		sc->priority = initial_priority;
		sc->memcg_low_reclaim = 1;
		sc->memcg_low_skipped = 0;
		goto retry;
	}

	return 0;
}

static bool allow_direct_reclaim(pg_data_t *pgdat)
{
	struct zone *zone;
	unsigned long pfmemalloc_reserve = 0;
	unsigned long free_pages = 0;
	int i;
	bool wmark_ok;

	if (pgdat->kswapd_failures >= MAX_RECLAIM_RETRIES)
		return true;

	for (i = 0; i <= ZONE_NORMAL; i++) {
		zone = &pgdat->node_zones[i];
		if (!managed_zone(zone))
			continue;

		if (!zone_reclaimable_pages(zone))
			continue;

		pfmemalloc_reserve += min_wmark_pages(zone);
		free_pages += zone_page_state(zone, NR_FREE_PAGES);
	}

	/* If there are no reserves (unexpected config) then do not throttle */
	if (!pfmemalloc_reserve)
		return true;

	wmark_ok = free_pages > pfmemalloc_reserve / 2;

	/* kswapd must be awake if processes are being throttled */
	if (!wmark_ok && waitqueue_active(&pgdat->kswapd_wait)) {
		if (READ_ONCE(pgdat->kswapd_classzone_idx) > ZONE_NORMAL)
			WRITE_ONCE(pgdat->kswapd_classzone_idx, ZONE_NORMAL);

		wake_up_interruptible(&pgdat->kswapd_wait);
	}

	return wmark_ok;
}

/*
 * Throttle direct reclaimers if backing storage is backed by the network
 * and the PFMEMALLOC reserve for the preferred node is getting dangerously
 * depleted. kswapd will continue to make progress and wake the processes
 * when the low watermark is reached.
 *
 * Returns true if a fatal signal was delivered during throttling. If this
 * happens, the page allocator should not consider triggering the OOM killer.
 */
static bool throttle_direct_reclaim(gfp_t gfp_mask, struct zonelist *zonelist,
					nodemask_t *nodemask)
{
	struct zoneref *z;
	struct zone *zone;
	pg_data_t *pgdat = NULL;

	/*
	 * Kernel threads should not be throttled as they may be indirectly
	 * responsible for cleaning pages necessary for reclaim to make forward
	 * progress. kjournald for example may enter direct reclaim while
	 * committing a transaction where throttling it could forcing other
	 * processes to block on log_wait_commit().
	 */
	if (current->flags & PF_KTHREAD)
		goto out;

	/*
	 * If a fatal signal is pending, this process should not throttle.
	 * It should return quickly so it can exit and free its memory
	 */
	if (fatal_signal_pending(current))
		goto out;

	/*
	 * Check if the pfmemalloc reserves are ok by finding the first node
	 * with a usable ZONE_NORMAL or lower zone. The expectation is that
	 * GFP_KERNEL will be required for allocating network buffers when
	 * swapping over the network so ZONE_HIGHMEM is unusable.
	 *
	 * Throttling is based on the first usable node and throttled processes
	 * wait on a queue until kswapd makes progress and wakes them. There
	 * is an affinity then between processes waking up and where reclaim
	 * progress has been made assuming the process wakes on the same node.
	 * More importantly, processes running on remote nodes will not compete
	 * for remote pfmemalloc reserves and processes on different nodes
	 * should make reasonable progress.
	 */
	for_each_zone_zonelist_nodemask(zone, z, zonelist,
					gfp_zone(gfp_mask), nodemask) {
		if (zone_idx(zone) > ZONE_NORMAL)
			continue;

		/* Throttle based on the first usable node */
		pgdat = zone->zone_pgdat;
		if (allow_direct_reclaim(pgdat))
			goto out;
		break;
	}

	/* If no zone was usable by the allocation flags then do not throttle */
	if (!pgdat)
		goto out;

	/* Account for the throttling */
	count_vm_event(PGSCAN_DIRECT_THROTTLE);

	/*
	 * If the caller cannot enter the filesystem, it's possible that it
	 * is due to the caller holding an FS lock or performing a journal
	 * transaction in the case of a filesystem like ext[3|4]. In this case,
	 * it is not safe to block on pfmemalloc_wait as kswapd could be
	 * blocked waiting on the same lock. Instead, throttle for up to a
	 * second before continuing.
	 */
	if (!(gfp_mask & __GFP_FS)) {
		wait_event_interruptible_timeout(pgdat->pfmemalloc_wait,
			allow_direct_reclaim(pgdat), HZ);

		goto check_pending;
	}

	/* Throttle until kswapd wakes the process */
	wait_event_killable(zone->zone_pgdat->pfmemalloc_wait,
		allow_direct_reclaim(pgdat));

check_pending:
	if (fatal_signal_pending(current))
		return true;

out:
	return false;
}

unsigned long try_to_free_pages(struct zonelist *zonelist, int order,
				gfp_t gfp_mask, nodemask_t *nodemask)
{
	unsigned long nr_reclaimed;
	struct scan_control sc = {
		.nr_to_reclaim = SWAP_CLUSTER_MAX,
		.gfp_mask = current_gfp_context(gfp_mask),
		.reclaim_idx = gfp_zone(gfp_mask),
		.order = order,
		.nodemask = nodemask,
		.priority = DEF_PRIORITY,
		.may_writepage = !laptop_mode,
		.may_unmap = 1,
		.may_swap = 1,
	};

	/*
	 * scan_control uses s8 fields for order, priority, and reclaim_idx.
	 * Confirm they are large enough for max values.
	 */
	BUILD_BUG_ON(MAX_ORDER > S8_MAX);
	BUILD_BUG_ON(DEF_PRIORITY > S8_MAX);
	BUILD_BUG_ON(MAX_NR_ZONES > S8_MAX);

	/*
	 * Do not enter reclaim if fatal signal was delivered while throttled.
	 * 1 is returned so that the page allocator does not OOM kill at this
	 * point.
	 */
	if (throttle_direct_reclaim(sc.gfp_mask, zonelist, nodemask))
		return 1;

	set_task_reclaim_state(current, &sc.reclaim_state);
	trace_mm_vmscan_direct_reclaim_begin(order, sc.gfp_mask);

	nr_reclaimed = do_try_to_free_pages(zonelist, &sc);

	trace_mm_vmscan_direct_reclaim_end(nr_reclaimed);
	set_task_reclaim_state(current, NULL);

	return nr_reclaimed;
}

#ifdef CONFIG_MEMCG

/* Only used by soft limit reclaim. Do not reuse for anything else. */
unsigned long mem_cgroup_shrink_node(struct mem_cgroup *memcg,
						gfp_t gfp_mask, bool noswap,
						pg_data_t *pgdat,
						unsigned long *nr_scanned)
{
	struct scan_control sc = {
		.nr_to_reclaim = SWAP_CLUSTER_MAX,
		.target_mem_cgroup = memcg,
		.may_writepage = !laptop_mode,
		.may_unmap = 1,
		.reclaim_idx = MAX_NR_ZONES - 1,
		.may_swap = !noswap,
	};
	unsigned long lru_pages;

	WARN_ON_ONCE(!current->reclaim_state);

	sc.gfp_mask = (gfp_mask & GFP_RECLAIM_MASK) |
			(GFP_HIGHUSER_MOVABLE & ~GFP_RECLAIM_MASK);

	trace_mm_vmscan_memcg_softlimit_reclaim_begin(sc.order,
						      sc.gfp_mask);

	/*
	 * NOTE: Although we can get the priority field, using it
	 * here is not a good idea, since it limits the pages we can scan.
	 * if we don't reclaim here, the shrink_node from balance_pgdat
	 * will pick up pages from other mem cgroup's as well. We hack
	 * the priority and make it zero.
	 */
	shrink_node_memcg(pgdat, memcg, &sc, &lru_pages);

	trace_mm_vmscan_memcg_softlimit_reclaim_end(sc.nr_reclaimed);

	*nr_scanned = sc.nr_scanned;

	return sc.nr_reclaimed;
}

unsigned long try_to_free_mem_cgroup_pages(struct mem_cgroup *memcg,
					   unsigned long nr_pages,
					   gfp_t gfp_mask,
					   bool may_swap)
{
	struct zonelist *zonelist;
	unsigned long nr_reclaimed;
	unsigned long pflags;
	int nid;
	unsigned int noreclaim_flag;
	struct scan_control sc = {
		.nr_to_reclaim = max(nr_pages, SWAP_CLUSTER_MAX),
		.gfp_mask = (current_gfp_context(gfp_mask) & GFP_RECLAIM_MASK) |
				(GFP_HIGHUSER_MOVABLE & ~GFP_RECLAIM_MASK),
		.reclaim_idx = MAX_NR_ZONES - 1,
		.target_mem_cgroup = memcg,
		.priority = DEF_PRIORITY,
		.may_writepage = !laptop_mode,
		.may_unmap = 1,
		.may_swap = may_swap,
	};

	set_task_reclaim_state(current, &sc.reclaim_state);
	/*
	 * Unlike direct reclaim via alloc_pages(), memcg's reclaim doesn't
	 * take care of from where we get pages. So the node where we start the
	 * scan does not need to be the current node.
	 */
	nid = mem_cgroup_select_victim_node(memcg);

	zonelist = &NODE_DATA(nid)->node_zonelists[ZONELIST_FALLBACK];

	trace_mm_vmscan_memcg_reclaim_begin(0, sc.gfp_mask);

	psi_memstall_enter(&pflags);
	noreclaim_flag = memalloc_noreclaim_save();

	nr_reclaimed = do_try_to_free_pages(zonelist, &sc);

	memalloc_noreclaim_restore(noreclaim_flag);
	psi_memstall_leave(&pflags);

	trace_mm_vmscan_memcg_reclaim_end(nr_reclaimed);
	set_task_reclaim_state(current, NULL);

	return nr_reclaimed;
}
#endif

static void age_active_anon(struct pglist_data *pgdat,
				struct scan_control *sc)
{
	struct mem_cgroup *memcg;

	if (lru_gen_enabled()) {
		lru_gen_age_node(pgdat, sc);
		return;
	}

	if (!total_swap_pages)
		return;

	memcg = mem_cgroup_iter(NULL, NULL, NULL);
	do {
		struct lruvec *lruvec = mem_cgroup_lruvec(pgdat, memcg);

		if (inactive_list_is_low(lruvec, false, sc, true))
			shrink_active_list(SWAP_CLUSTER_MAX, lruvec,
					   sc, LRU_ACTIVE_ANON);

		memcg = mem_cgroup_iter(NULL, memcg, NULL);
	} while (memcg);
}

static bool pgdat_watermark_boosted(pg_data_t *pgdat, int classzone_idx)
{
	int i;
	struct zone *zone;

	/*
	 * Check for watermark boosts top-down as the higher zones
	 * are more likely to be boosted. Both watermarks and boosts
	 * should not be checked at the time time as reclaim would
	 * start prematurely when there is no boosting and a lower
	 * zone is balanced.
	 */
	for (i = classzone_idx; i >= 0; i--) {
		zone = pgdat->node_zones + i;
		if (!managed_zone(zone))
			continue;

		if (zone->watermark_boost)
			return true;
	}

	return false;
}

/*
 * Returns true if there is an eligible zone balanced for the request order
 * and classzone_idx
 */
static bool pgdat_balanced(pg_data_t *pgdat, int order, int classzone_idx)
{
	int i;
	unsigned long mark = -1;
	struct zone *zone;

	/*
	 * Check watermarks bottom-up as lower zones are more likely to
	 * meet watermarks.
	 */
	for (i = 0; i <= classzone_idx; i++) {
		zone = pgdat->node_zones + i;

		if (!managed_zone(zone))
			continue;

		mark = high_wmark_pages(zone);
		if (zone_watermark_ok_safe(zone, order, mark, classzone_idx))
			return true;
	}

	/*
	 * If a node has no populated zone within classzone_idx, it does not
	 * need balancing by definition. This can happen if a zone-restricted
	 * allocation tries to wake a remote kswapd.
	 */
	if (mark == -1)
		return true;

	return false;
}

/* Clear pgdat state for congested, dirty or under writeback. */
static void clear_pgdat_congested(pg_data_t *pgdat)
{
	clear_bit(PGDAT_CONGESTED, &pgdat->flags);
	clear_bit(PGDAT_DIRTY, &pgdat->flags);
	clear_bit(PGDAT_WRITEBACK, &pgdat->flags);
}

/*
 * Prepare kswapd for sleeping. This verifies that there are no processes
 * waiting in throttle_direct_reclaim() and that watermarks have been met.
 *
 * Returns true if kswapd is ready to sleep
 */
static bool prepare_kswapd_sleep(pg_data_t *pgdat, int order, int classzone_idx)
{
	/*
	 * The throttled processes are normally woken up in balance_pgdat() as
	 * soon as allow_direct_reclaim() is true. But there is a potential
	 * race between when kswapd checks the watermarks and a process gets
	 * throttled. There is also a potential race if processes get
	 * throttled, kswapd wakes, a large process exits thereby balancing the
	 * zones, which causes kswapd to exit balance_pgdat() before reaching
	 * the wake up checks. If kswapd is going to sleep, no process should
	 * be sleeping on pfmemalloc_wait, so wake them now if necessary. If
	 * the wake up is premature, processes will wake kswapd and get
	 * throttled again. The difference from wake ups in balance_pgdat() is
	 * that here we are under prepare_to_wait().
	 */
	if (waitqueue_active(&pgdat->pfmemalloc_wait))
		wake_up_all(&pgdat->pfmemalloc_wait);

	/* Hopeless node, leave it to direct reclaim */
	if (pgdat->kswapd_failures >= MAX_RECLAIM_RETRIES)
		return true;

	if (pgdat_balanced(pgdat, order, classzone_idx)) {
		clear_pgdat_congested(pgdat);
		return true;
	}

	return false;
}

/*
 * kswapd shrinks a node of pages that are at or below the highest usable
 * zone that is currently unbalanced.
 *
 * Returns true if kswapd scanned at least the requested number of pages to
 * reclaim or if the lack of progress was due to pages under writeback.
 * This is used to determine if the scanning priority needs to be raised.
 */
static bool kswapd_shrink_node(pg_data_t *pgdat,
			       struct scan_control *sc)
{
	struct zone *zone;
	int z;

	/* Reclaim a number of pages proportional to the number of zones */
	sc->nr_to_reclaim = 0;
	for (z = 0; z <= sc->reclaim_idx; z++) {
		zone = pgdat->node_zones + z;
		if (!managed_zone(zone))
			continue;

		sc->nr_to_reclaim += max(high_wmark_pages(zone), SWAP_CLUSTER_MAX);
	}

	/*
	 * Historically care was taken to put equal pressure on all zones but
	 * now pressure is applied based on node LRU order.
	 */
	shrink_node(pgdat, sc);

	/*
	 * Fragmentation may mean that the system cannot be rebalanced for
	 * high-order allocations. If twice the allocation size has been
	 * reclaimed then recheck watermarks only at order-0 to prevent
	 * excessive reclaim. Assume that a process requested a high-order
	 * can direct reclaim/compact.
	 */
	if (sc->order && sc->nr_reclaimed >= compact_gap(sc->order))
		sc->order = 0;

	return sc->nr_scanned >= sc->nr_to_reclaim;
}

/*
 * For kswapd, balance_pgdat() will reclaim pages across a node from zones
 * that are eligible for use by the caller until at least one zone is
 * balanced.
 *
 * Returns the order kswapd finished reclaiming at.
 *
 * kswapd scans the zones in the highmem->normal->dma direction.  It skips
 * zones which have free_pages > high_wmark_pages(zone), but once a zone is
 * found to have free_pages <= high_wmark_pages(zone), any page in that zone
 * or lower is eligible for reclaim until at least one usable zone is
 * balanced.
 */
static int balance_pgdat(pg_data_t *pgdat, int order, int classzone_idx)
{
	int i;
	unsigned long nr_soft_reclaimed;
	unsigned long nr_soft_scanned;
	unsigned long pflags;
	unsigned long nr_boost_reclaim;
	unsigned long zone_boosts[MAX_NR_ZONES] = { 0, };
	bool boosted;
	struct zone *zone;
	struct scan_control sc = {
		.gfp_mask = GFP_KERNEL,
		.order = order,
		.may_unmap = 1,
	};

	set_task_reclaim_state(current, &sc.reclaim_state);
	psi_memstall_enter(&pflags);
	__fs_reclaim_acquire();

	count_vm_event(PAGEOUTRUN);

	/*
	 * Account for the reclaim boost. Note that the zone boost is left in
	 * place so that parallel allocations that are near the watermark will
	 * stall or direct reclaim until kswapd is finished.
	 */
	nr_boost_reclaim = 0;
	for (i = 0; i <= classzone_idx; i++) {
		zone = pgdat->node_zones + i;
		if (!managed_zone(zone))
			continue;

		nr_boost_reclaim += zone->watermark_boost;
		zone_boosts[i] = zone->watermark_boost;
	}
	boosted = nr_boost_reclaim;

restart:
	sc.priority = DEF_PRIORITY;
	do {
		unsigned long nr_reclaimed = sc.nr_reclaimed;
		bool raise_priority = true;
		bool balanced;
		bool ret;

		sc.reclaim_idx = classzone_idx;

		/*
		 * If the number of buffer_heads exceeds the maximum allowed
		 * then consider reclaiming from all zones. This has a dual
		 * purpose -- on 64-bit systems it is expected that
		 * buffer_heads are stripped during active rotation. On 32-bit
		 * systems, highmem pages can pin lowmem memory and shrinking
		 * buffers can relieve lowmem pressure. Reclaim may still not
		 * go ahead if all eligible zones for the original allocation
		 * request are balanced to avoid excessive reclaim from kswapd.
		 */
		if (buffer_heads_over_limit) {
			for (i = MAX_NR_ZONES - 1; i >= 0; i--) {
				zone = pgdat->node_zones + i;
				if (!managed_zone(zone))
					continue;

				sc.reclaim_idx = i;
				break;
			}
		}

		/*
		 * If the pgdat is imbalanced then ignore boosting and preserve
		 * the watermarks for a later time and restart. Note that the
		 * zone watermarks will be still reset at the end of balancing
		 * on the grounds that the normal reclaim should be enough to
		 * re-evaluate if boosting is required when kswapd next wakes.
		 */
		balanced = pgdat_balanced(pgdat, sc.order, classzone_idx);
		if (!balanced && nr_boost_reclaim) {
			nr_boost_reclaim = 0;
			goto restart;
		}

		/*
		 * If boosting is not active then only reclaim if there are no
		 * eligible zones. Note that sc.reclaim_idx is not used as
		 * buffer_heads_over_limit may have adjusted it.
		 */
		if (!nr_boost_reclaim && balanced)
			goto out;

		/* Limit the priority of boosting to avoid reclaim writeback */
		if (nr_boost_reclaim && sc.priority == DEF_PRIORITY - 2)
			raise_priority = false;

		/*
		 * Do not writeback or swap pages for boosted reclaim. The
		 * intent is to relieve pressure not issue sub-optimal IO
		 * from reclaim context. If no pages are reclaimed, the
		 * reclaim will be aborted.
		 */
		sc.may_writepage = !laptop_mode && !nr_boost_reclaim;
		sc.may_swap = !nr_boost_reclaim;

		/*
		 * Do some background aging of the anon list, to give
		 * pages a chance to be referenced before reclaiming. All
		 * pages are rotated regardless of classzone as this is
		 * about consistent aging.
		 */
		age_active_anon(pgdat, &sc);

		/*
		 * If we're getting trouble reclaiming, start doing writepage
		 * even in laptop mode.
		 */
		if (sc.priority < DEF_PRIORITY - 2)
			sc.may_writepage = 1;

		/* Call soft limit reclaim before calling shrink_node. */
		sc.nr_scanned = 0;
		nr_soft_scanned = 0;
		nr_soft_reclaimed = mem_cgroup_soft_limit_reclaim(pgdat, sc.order,
						sc.gfp_mask, &nr_soft_scanned);
		sc.nr_reclaimed += nr_soft_reclaimed;

		/*
		 * There should be no need to raise the scanning priority if
		 * enough pages are already being scanned that that high
		 * watermark would be met at 100% efficiency.
		 */
		if (kswapd_shrink_node(pgdat, &sc))
			raise_priority = false;

		/*
		 * If the low watermark is met there is no need for processes
		 * to be throttled on pfmemalloc_wait as they should not be
		 * able to safely make forward progress. Wake them
		 */
		if (waitqueue_active(&pgdat->pfmemalloc_wait) &&
				allow_direct_reclaim(pgdat))
			wake_up_all(&pgdat->pfmemalloc_wait);

		/* Check if kswapd should be suspending */
		__fs_reclaim_release();
		ret = try_to_freeze();
		__fs_reclaim_acquire();
		if (ret || kthread_should_stop())
			break;

		/*
		 * Raise priority if scanning rate is too low or there was no
		 * progress in reclaiming pages
		 */
		nr_reclaimed = sc.nr_reclaimed - nr_reclaimed;
		nr_boost_reclaim -= min(nr_boost_reclaim, nr_reclaimed);

		/*
		 * If reclaim made no progress for a boost, stop reclaim as
		 * IO cannot be queued and it could be an infinite loop in
		 * extreme circumstances.
		 */
		if (nr_boost_reclaim && !nr_reclaimed)
			break;

		if (raise_priority || !nr_reclaimed)
			sc.priority--;
	} while (sc.priority >= 1);

	if (!sc.nr_reclaimed)
		pgdat->kswapd_failures++;

out:
	/* If reclaim was boosted, account for the reclaim done in this pass */
	if (boosted) {
		unsigned long flags;

		for (i = 0; i <= classzone_idx; i++) {
			if (!zone_boosts[i])
				continue;

			/* Increments are under the zone lock */
			zone = pgdat->node_zones + i;
			spin_lock_irqsave(&zone->lock, flags);
			zone->watermark_boost -= min(zone->watermark_boost, zone_boosts[i]);
			spin_unlock_irqrestore(&zone->lock, flags);
		}

		/*
		 * As there is now likely space, wakeup kcompact to defragment
		 * pageblocks.
		 */
		wakeup_kcompactd(pgdat, pageblock_order, classzone_idx);
	}

	snapshot_refaults(NULL, pgdat);
	__fs_reclaim_release();
	psi_memstall_leave(&pflags);
	set_task_reclaim_state(current, NULL);

	/*
	 * Return the order kswapd stopped reclaiming at as
	 * prepare_kswapd_sleep() takes it into account. If another caller
	 * entered the allocator slow path while kswapd was awake, order will
	 * remain at the higher level.
	 */
	return sc.order;
}

/*
 * The pgdat->kswapd_classzone_idx is used to pass the highest zone index to be
 * reclaimed by kswapd from the waker. If the value is MAX_NR_ZONES which is not
 * a valid index then either kswapd runs for first time or kswapd couldn't sleep
 * after previous reclaim attempt (node is still unbalanced). In that case
 * return the zone index of the previous kswapd reclaim cycle.
 */
static enum zone_type kswapd_classzone_idx(pg_data_t *pgdat,
					   enum zone_type prev_classzone_idx)
{
	enum zone_type curr_idx = READ_ONCE(pgdat->kswapd_classzone_idx);

	return curr_idx == MAX_NR_ZONES ? prev_classzone_idx : curr_idx;
}

static void kswapd_try_to_sleep(pg_data_t *pgdat, int alloc_order, int reclaim_order,
				unsigned int classzone_idx)
{
	long remaining = 0;
	DEFINE_WAIT(wait);

	if (freezing(current) || kthread_should_stop())
		return;

	prepare_to_wait(&pgdat->kswapd_wait, &wait, TASK_INTERRUPTIBLE);

	/*
	 * Try to sleep for a short interval. Note that kcompactd will only be
	 * woken if it is possible to sleep for a short interval. This is
	 * deliberate on the assumption that if reclaim cannot keep an
	 * eligible zone balanced that it's also unlikely that compaction will
	 * succeed.
	 */
	if (prepare_kswapd_sleep(pgdat, reclaim_order, classzone_idx)) {
		/*
		 * Compaction records what page blocks it recently failed to
		 * isolate pages from and skips them in the future scanning.
		 * When kswapd is going to sleep, it is reasonable to assume
		 * that pages and compaction may succeed so reset the cache.
		 */
		reset_isolation_suitable(pgdat);

		/*
		 * We have freed the memory, now we should compact it to make
		 * allocation of the requested order possible.
		 */
		wakeup_kcompactd(pgdat, alloc_order, classzone_idx);

		remaining = schedule_timeout(HZ/10);

		/*
		 * If woken prematurely then reset kswapd_classzone_idx and
		 * order. The values will either be from a wakeup request or
		 * the previous request that slept prematurely.
		 */
		if (remaining) {
			WRITE_ONCE(pgdat->kswapd_classzone_idx,
				   kswapd_classzone_idx(pgdat, classzone_idx));

			if (READ_ONCE(pgdat->kswapd_order) < reclaim_order)
				WRITE_ONCE(pgdat->kswapd_order, reclaim_order);
		}

		finish_wait(&pgdat->kswapd_wait, &wait);
		prepare_to_wait(&pgdat->kswapd_wait, &wait, TASK_INTERRUPTIBLE);
	}

	/*
	 * After a short sleep, check if it was a premature sleep. If not, then
	 * go fully to sleep until explicitly woken up.
	 */
	if (!remaining &&
	    prepare_kswapd_sleep(pgdat, reclaim_order, classzone_idx)) {
		trace_mm_vmscan_kswapd_sleep(pgdat->node_id);

		/*
		 * vmstat counters are not perfectly accurate and the estimated
		 * value for counters such as NR_FREE_PAGES can deviate from the
		 * true value by nr_online_cpus * threshold. To avoid the zone
		 * watermarks being breached while under pressure, we reduce the
		 * per-cpu vmstat threshold while kswapd is awake and restore
		 * them before going back to sleep.
		 */
		set_pgdat_percpu_threshold(pgdat, calculate_normal_threshold);

		if (!kthread_should_stop())
			schedule();

		set_pgdat_percpu_threshold(pgdat, calculate_pressure_threshold);
	} else {
		if (remaining)
			count_vm_event(KSWAPD_LOW_WMARK_HIT_QUICKLY);
		else
			count_vm_event(KSWAPD_HIGH_WMARK_HIT_QUICKLY);
	}
	finish_wait(&pgdat->kswapd_wait, &wait);
}

/*
 * The background pageout daemon, started as a kernel thread
 * from the init process.
 *
 * This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 *
 * If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
 */
static int kswapd(void *p)
{
	unsigned int alloc_order, reclaim_order;
	unsigned int classzone_idx = MAX_NR_ZONES - 1;
	pg_data_t *pgdat = (pg_data_t*)p;
	struct task_struct *tsk = current;
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(tsk, cpumask);

	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	tsk->flags |= PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD;
	set_freezable();

	WRITE_ONCE(pgdat->kswapd_order, 0);
	WRITE_ONCE(pgdat->kswapd_classzone_idx, MAX_NR_ZONES);
	for ( ; ; ) {
		bool ret;

		alloc_order = reclaim_order = READ_ONCE(pgdat->kswapd_order);
		classzone_idx = kswapd_classzone_idx(pgdat, classzone_idx);

kswapd_try_sleep:
		kswapd_try_to_sleep(pgdat, alloc_order, reclaim_order,
					classzone_idx);

		/* Read the new order and classzone_idx */
		alloc_order = reclaim_order = READ_ONCE(pgdat->kswapd_order);
		classzone_idx = kswapd_classzone_idx(pgdat, classzone_idx);
		WRITE_ONCE(pgdat->kswapd_order, 0);
		WRITE_ONCE(pgdat->kswapd_classzone_idx, MAX_NR_ZONES);

		ret = try_to_freeze();
		if (kthread_should_stop())
			break;

		/*
		 * We can speed up thawing tasks if we don't call balance_pgdat
		 * after returning from the refrigerator
		 */
		if (ret)
			continue;

		/*
		 * Reclaim begins at the requested order but if a high-order
		 * reclaim fails then kswapd falls back to reclaiming for
		 * order-0. If that happens, kswapd will consider sleeping
		 * for the order it finished reclaiming at (reclaim_order)
		 * but kcompactd is woken to compact for the original
		 * request (alloc_order).
		 */
		trace_mm_vmscan_kswapd_wake(pgdat->node_id, classzone_idx,
						alloc_order);
		reclaim_order = balance_pgdat(pgdat, alloc_order, classzone_idx);
		if (reclaim_order < alloc_order)
			goto kswapd_try_sleep;
	}

	tsk->flags &= ~(PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD);

	return 0;
}

/*
 * A zone is low on free memory or too fragmented for high-order memory.  If
 * kswapd should reclaim (direct reclaim is deferred), wake it up for the zone's
 * pgdat.  It will wake up kcompactd after reclaiming memory.  If kswapd reclaim
 * has failed or is not needed, still wake up kcompactd if only compaction is
 * needed.
 */
void wakeup_kswapd(struct zone *zone, gfp_t gfp_flags, int order,
		   enum zone_type classzone_idx)
{
	pg_data_t *pgdat;
	enum zone_type curr_idx;

	if (!managed_zone(zone))
		return;

	if (!cpuset_zone_allowed(zone, gfp_flags))
		return;

	pgdat = zone->zone_pgdat;
	curr_idx = READ_ONCE(pgdat->kswapd_classzone_idx);

	if (curr_idx == MAX_NR_ZONES || curr_idx < classzone_idx)
		WRITE_ONCE(pgdat->kswapd_classzone_idx, classzone_idx);

	if (READ_ONCE(pgdat->kswapd_order) < order)
		WRITE_ONCE(pgdat->kswapd_order, order);

	if (!waitqueue_active(&pgdat->kswapd_wait))
		return;

	/* Hopeless node, leave it to direct reclaim if possible */
	if (pgdat->kswapd_failures >= MAX_RECLAIM_RETRIES ||
	    (pgdat_balanced(pgdat, order, classzone_idx) &&
	     !pgdat_watermark_boosted(pgdat, classzone_idx))) {
		/*
		 * There may be plenty of free memory available, but it's too
		 * fragmented for high-order allocations.  Wake up kcompactd
		 * and rely on compaction_suitable() to determine if it's
		 * needed.  If it fails, it will defer subsequent attempts to
		 * ratelimit its work.
		 */
		if (!(gfp_flags & __GFP_DIRECT_RECLAIM))
			wakeup_kcompactd(pgdat, order, classzone_idx);
		return;
	}

	trace_mm_vmscan_wakeup_kswapd(pgdat->node_id, classzone_idx, order,
				      gfp_flags);
	wake_up_interruptible(&pgdat->kswapd_wait);
}

#ifdef CONFIG_HIBERNATION
/*
 * Try to free `nr_to_reclaim' of memory, system-wide, and return the number of
 * freed pages.
 *
 * Rather than trying to age LRUs the aim is to preserve the overall
 * LRU order by reclaiming preferentially
 * inactive > active > active referenced > active mapped
 */
unsigned long shrink_all_memory(unsigned long nr_to_reclaim)
{
	struct scan_control sc = {
		.nr_to_reclaim = nr_to_reclaim,
		.gfp_mask = GFP_HIGHUSER_MOVABLE,
		.reclaim_idx = MAX_NR_ZONES - 1,
		.priority = DEF_PRIORITY,
		.may_writepage = 1,
		.may_unmap = 1,
		.may_swap = 1,
		.hibernation_mode = 1,
	};
	struct zonelist *zonelist = node_zonelist(numa_node_id(), sc.gfp_mask);
	unsigned long nr_reclaimed;
	unsigned int noreclaim_flag;

	fs_reclaim_acquire(sc.gfp_mask);
	noreclaim_flag = memalloc_noreclaim_save();
	set_task_reclaim_state(current, &sc.reclaim_state);

	nr_reclaimed = do_try_to_free_pages(zonelist, &sc);

	set_task_reclaim_state(current, NULL);
	memalloc_noreclaim_restore(noreclaim_flag);
	fs_reclaim_release(sc.gfp_mask);

	return nr_reclaimed;
}
#endif /* CONFIG_HIBERNATION */

/* It's optimal to keep kswapds on the same CPUs as their memory, but
   not required for correctness.  So if the last cpu in a node goes
   away, we get changed to run anywhere: as the first one comes back,
   restore their cpu bindings. */
static int kswapd_cpu_online(unsigned int cpu)
{
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		const struct cpumask *mask;

		mask = cpumask_of_node(pgdat->node_id);

		if (cpumask_any_and(cpu_online_mask, mask) < nr_cpu_ids)
			/* One of our CPUs online: restore mask */
			set_cpus_allowed_ptr(pgdat->kswapd, mask);
	}
	return 0;
}

/*
 * This kswapd start function will be called by init and node-hot-add.
 * On node-hot-add, kswapd will moved to proper cpus if cpus are hot-added.
 */
int kswapd_run(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	int ret = 0;

	if (pgdat->kswapd)
		return 0;

	lru_gen_start_kswapd(nid);

	pgdat->kswapd = kthread_run(kswapd, pgdat, "kswapd%d", nid);
	if (IS_ERR(pgdat->kswapd)) {
		/* failure at boot is fatal */
		BUG_ON(system_state < SYSTEM_RUNNING);
		pr_err("Failed to start kswapd on node %d\n", nid);
		ret = PTR_ERR(pgdat->kswapd);
		pgdat->kswapd = NULL;
	}
	return ret;
}

/*
 * Called by memory hotplug when all memory in a node is offlined.  Caller must
 * hold mem_hotplug_begin/end().
 */
void kswapd_stop(int nid)
{
	struct task_struct *kswapd = NODE_DATA(nid)->kswapd;

	if (kswapd) {
		kthread_stop(kswapd);
		NODE_DATA(nid)->kswapd = NULL;
		lru_gen_stop_kswapd(nid);
	}
}

static int __init kswapd_init(void)
{
	int nid, ret;

	swap_setup();
	for_each_node_state(nid, N_MEMORY)
 		kswapd_run(nid);
	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
					"mm/vmscan:online", kswapd_cpu_online,
					NULL);
	WARN_ON(ret < 0);
	return 0;
}

module_init(kswapd_init)

#ifdef CONFIG_NUMA
/*
 * Node reclaim mode
 *
 * If non-zero call node_reclaim when the number of free pages falls below
 * the watermarks.
 */
int node_reclaim_mode __read_mostly;

#define RECLAIM_OFF 0
#define RECLAIM_ZONE (1<<0)	/* Run shrink_inactive_list on the zone */
#define RECLAIM_WRITE (1<<1)	/* Writeout pages during reclaim */
#define RECLAIM_UNMAP (1<<2)	/* Unmap pages during reclaim */

/*
 * Priority for NODE_RECLAIM. This determines the fraction of pages
 * of a node considered for each zone_reclaim. 4 scans 1/16th of
 * a zone.
 */
#define NODE_RECLAIM_PRIORITY 4

/*
 * Percentage of pages in a zone that must be unmapped for node_reclaim to
 * occur.
 */
int sysctl_min_unmapped_ratio = 1;

/*
 * If the number of slab pages in a zone grows beyond this percentage then
 * slab reclaim needs to occur.
 */
int sysctl_min_slab_ratio = 5;

static inline unsigned long node_unmapped_file_pages(struct pglist_data *pgdat)
{
	unsigned long file_mapped = node_page_state(pgdat, NR_FILE_MAPPED);
	unsigned long file_lru = node_page_state(pgdat, NR_INACTIVE_FILE) +
		node_page_state(pgdat, NR_ACTIVE_FILE);

	/*
	 * It's possible for there to be more file mapped pages than
	 * accounted for by the pages on the file LRU lists because
	 * tmpfs pages accounted for as ANON can also be FILE_MAPPED
	 */
	return (file_lru > file_mapped) ? (file_lru - file_mapped) : 0;
}

/* Work out how many page cache pages we can reclaim in this reclaim_mode */
static unsigned long node_pagecache_reclaimable(struct pglist_data *pgdat)
{
	unsigned long nr_pagecache_reclaimable;
	unsigned long delta = 0;

	/*
	 * If RECLAIM_UNMAP is set, then all file pages are considered
	 * potentially reclaimable. Otherwise, we have to worry about
	 * pages like swapcache and node_unmapped_file_pages() provides
	 * a better estimate
	 */
	if (node_reclaim_mode & RECLAIM_UNMAP)
		nr_pagecache_reclaimable = node_page_state(pgdat, NR_FILE_PAGES);
	else
		nr_pagecache_reclaimable = node_unmapped_file_pages(pgdat);

	/* If we can't clean pages, remove dirty pages from consideration */
	if (!(node_reclaim_mode & RECLAIM_WRITE))
		delta += node_page_state(pgdat, NR_FILE_DIRTY);

	/* Watch for any possible underflows due to delta */
	if (unlikely(delta > nr_pagecache_reclaimable))
		delta = nr_pagecache_reclaimable;

	return nr_pagecache_reclaimable - delta;
}

/*
 * Try to free up some pages from this node through reclaim.
 */
static int __node_reclaim(struct pglist_data *pgdat, gfp_t gfp_mask, unsigned int order)
{
	/* Minimum pages needed in order to stay on node */
	const unsigned long nr_pages = 1 << order;
	struct task_struct *p = current;
	unsigned int noreclaim_flag;
	struct scan_control sc = {
		.nr_to_reclaim = max(nr_pages, SWAP_CLUSTER_MAX),
		.gfp_mask = current_gfp_context(gfp_mask),
		.order = order,
		.priority = NODE_RECLAIM_PRIORITY,
		.may_writepage = !!(node_reclaim_mode & RECLAIM_WRITE),
		.may_unmap = !!(node_reclaim_mode & RECLAIM_UNMAP),
		.may_swap = 1,
		.reclaim_idx = gfp_zone(gfp_mask),
	};

	trace_mm_vmscan_node_reclaim_begin(pgdat->node_id, order,
					   sc.gfp_mask);

	cond_resched();
	fs_reclaim_acquire(sc.gfp_mask);
	/*
	 * We need to be able to allocate from the reserves for RECLAIM_UNMAP
	 * and we also need to be able to write out pages for RECLAIM_WRITE
	 * and RECLAIM_UNMAP.
	 */
	noreclaim_flag = memalloc_noreclaim_save();
	p->flags |= PF_SWAPWRITE;
	set_task_reclaim_state(p, &sc.reclaim_state);

	if (node_pagecache_reclaimable(pgdat) > pgdat->min_unmapped_pages) {
		/*
		 * Free memory by calling shrink node with increasing
		 * priorities until we have enough memory freed.
		 */
		do {
			shrink_node(pgdat, &sc);
		} while (sc.nr_reclaimed < nr_pages && --sc.priority >= 0);
	}

	set_task_reclaim_state(p, NULL);
	current->flags &= ~PF_SWAPWRITE;
	memalloc_noreclaim_restore(noreclaim_flag);
	fs_reclaim_release(sc.gfp_mask);

	trace_mm_vmscan_node_reclaim_end(sc.nr_reclaimed);

	return sc.nr_reclaimed >= nr_pages;
}

int node_reclaim(struct pglist_data *pgdat, gfp_t gfp_mask, unsigned int order)
{
	int ret;

	/*
	 * Node reclaim reclaims unmapped file backed pages and
	 * slab pages if we are over the defined limits.
	 *
	 * A small portion of unmapped file backed pages is needed for
	 * file I/O otherwise pages read by file I/O will be immediately
	 * thrown out if the node is overallocated. So we do not reclaim
	 * if less than a specified percentage of the node is used by
	 * unmapped file backed pages.
	 */
	if (node_pagecache_reclaimable(pgdat) <= pgdat->min_unmapped_pages &&
	    node_page_state(pgdat, NR_SLAB_RECLAIMABLE) <= pgdat->min_slab_pages)
		return NODE_RECLAIM_FULL;

	/*
	 * Do not scan if the allocation should not be delayed.
	 */
	if (!gfpflags_allow_blocking(gfp_mask) || (current->flags & PF_MEMALLOC))
		return NODE_RECLAIM_NOSCAN;

	/*
	 * Only run node reclaim on the local node or on nodes that do not
	 * have associated processors. This will favor the local processor
	 * over remote processors and spread off node memory allocations
	 * as wide as possible.
	 */
	if (node_state(pgdat->node_id, N_CPU) && pgdat->node_id != numa_node_id())
		return NODE_RECLAIM_NOSCAN;

	if (test_and_set_bit(PGDAT_RECLAIM_LOCKED, &pgdat->flags))
		return NODE_RECLAIM_NOSCAN;

	ret = __node_reclaim(pgdat, gfp_mask, order);
	clear_bit(PGDAT_RECLAIM_LOCKED, &pgdat->flags);

	if (!ret)
		count_vm_event(PGSCAN_ZONE_RECLAIM_FAILED);

	return ret;
}
#endif

/*
 * page_evictable - test whether a page is evictable
 * @page: the page to test
 *
 * Test whether page is evictable--i.e., should be placed on active/inactive
 * lists vs unevictable list.
 *
 * Reasons page might not be evictable:
 * (1) page's mapping marked unevictable
 * (2) page is part of an mlocked VMA
 *
 */
int page_evictable(struct page *page)
{
	int ret;

	/* Prevent address_space of inode and swap cache from being freed */
	rcu_read_lock();
	ret = !mapping_unevictable(page_mapping(page)) && !PageMlocked(page);
	rcu_read_unlock();
	return ret;
}

/**
 * check_move_unevictable_pages - check pages for evictability and move to
 * appropriate zone lru list
 * @pvec: pagevec with lru pages to check
 *
 * Checks pages for evictability, if an evictable page is in the unevictable
 * lru list, moves it to the appropriate evictable lru list. This function
 * should be only used for lru pages.
 */
void check_move_unevictable_pages(struct pagevec *pvec)
{
	struct lruvec *lruvec;
	struct pglist_data *pgdat = NULL;
	int pgscanned = 0;
	int pgrescued = 0;
	int i;

	for (i = 0; i < pvec->nr; i++) {
		struct page *page = pvec->pages[i];
		struct pglist_data *pagepgdat = page_pgdat(page);

		pgscanned++;
		if (pagepgdat != pgdat) {
			if (pgdat)
				spin_unlock_irq(&pgdat->lru_lock);
			pgdat = pagepgdat;
			spin_lock_irq(&pgdat->lru_lock);
		}
		lruvec = mem_cgroup_page_lruvec(page, pgdat);

		if (!PageLRU(page) || !PageUnevictable(page))
			continue;

		if (page_evictable(page)) {
			del_page_from_lru_list(page, lruvec);
			ClearPageUnevictable(page);
			add_page_to_lru_list(page, lruvec);
			pgrescued++;
		}
	}

	if (pgdat) {
		__count_vm_events(UNEVICTABLE_PGRESCUED, pgrescued);
		__count_vm_events(UNEVICTABLE_PGSCANNED, pgscanned);
		spin_unlock_irq(&pgdat->lru_lock);
	}
}
EXPORT_SYMBOL_GPL(check_move_unevictable_pages);
