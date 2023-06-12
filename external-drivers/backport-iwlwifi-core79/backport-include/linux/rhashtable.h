#ifndef __BACKPORT_RHASHTABLE_H
#define __BACKPORT_RHASHTABLE_H
#include_next <linux/rhashtable.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,12,0)
/**
 * rhashtable_lookup_get_insert_fast - lookup and insert object into hash table
 * @ht:		hash table
 * @obj:	pointer to hash head inside object
 * @params:	hash table parameters
 *
 * Just like rhashtable_lookup_insert_fast(), but this function returns the
 * object if it exists, NULL if it did not and the insertion was successful,
 * and an ERR_PTR otherwise.
 */
#define rhashtable_lookup_get_insert_fast LINUX_BACKPORT(rhashtable_lookup_get_insert_fast)
static inline void *rhashtable_lookup_get_insert_fast(
	struct rhashtable *ht, struct rhash_head *obj,
	const struct rhashtable_params params)
{
	const char *key = rht_obj(ht, obj);

	BUG_ON(ht->p.obj_hashfn);

	return __rhashtable_insert_fast(ht, key + ht->p.key_offset, obj, params,
					false);
}
#endif

#endif /* __BACKPORT_RHASHTABLE_H */
