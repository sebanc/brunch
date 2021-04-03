/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_RHASHTABLE
#include_next <linux/rhashtable.h>
#else
#undef lockdep_rht_mutex_is_held
#define lockdep_rht_mutex_is_held LINUX_BACKPORT(lockdep_rht_mutex_is_held)
#undef lockdep_rht_bucket_is_held
#define lockdep_rht_bucket_is_held LINUX_BACKPORT(lockdep_rht_bucket_is_held)
#undef rhashtable_insert_slow
#define rhashtable_insert_slow LINUX_BACKPORT(rhashtable_insert_slow)
#undef rhashtable_walk_enter
#define rhashtable_walk_enter LINUX_BACKPORT(rhashtable_walk_enter)
#undef rhashtable_walk_exit
#define rhashtable_walk_exit LINUX_BACKPORT(rhashtable_walk_exit)
#undef rhashtable_walk_start_check
#define rhashtable_walk_start_check LINUX_BACKPORT(rhashtable_walk_start_check)
#undef rhashtable_walk_next
#define rhashtable_walk_next LINUX_BACKPORT(rhashtable_walk_next)
#undef rhashtable_walk_peek
#define rhashtable_walk_peek LINUX_BACKPORT(rhashtable_walk_peek)
#undef rhashtable_walk_stop
#define rhashtable_walk_stop LINUX_BACKPORT(rhashtable_walk_stop)
#undef rhashtable_init
#define rhashtable_init LINUX_BACKPORT(rhashtable_init)
#undef rhltable_init
#define rhltable_init LINUX_BACKPORT(rhltable_init)
#undef rhashtable_free_and_destroy
#define rhashtable_free_and_destroy LINUX_BACKPORT(rhashtable_free_and_destroy)
#undef rhashtable_destroy
#define rhashtable_destroy LINUX_BACKPORT(rhashtable_destroy)
#undef __rht_bucket_nested
#define __rht_bucket_nested LINUX_BACKPORT(__rht_bucket_nested)
#undef rht_bucket_nested
#define rht_bucket_nested LINUX_BACKPORT(rht_bucket_nested)
#undef rht_bucket_nested_insert
#define rht_bucket_nested_insert LINUX_BACKPORT(rht_bucket_nested_insert)
#include <linux/backport-rhashtable.h>
#endif /* CPTCFG_BPAUTO_RHASHTABLE */
