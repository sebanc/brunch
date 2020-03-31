#ifndef __BACKPORT_SPINLOCK_H
#define __BACKPORT_SPINLOCK_H
#include_next <linux/spinlock.h>

#if LINUX_VERSION_IS_LESS(3,18,0)
#ifndef CONFIG_DEBUG_LOCK_ALLOC
#undef raw_spin_lock_nested
/*
 * Always evaluate the 'subclass' argument to avoid that the compiler
 * warns about set-but-not-used variables when building with
 * CONFIG_DEBUG_LOCK_ALLOC=n and with W=1.
 */
# define raw_spin_lock_nested(lock, subclass)		\
	_raw_spin_lock(((void)(subclass), (lock)))
# define raw_spin_lock_nest_lock(lock, nest_lock)	_raw_spin_lock(lock)
#endif
#endif /* LINUX_VERSION_IS_LESS(3,18,0) */


#if LINUX_VERSION_IS_LESS(4,16,0)
int alloc_bucket_spinlocks(spinlock_t **locks, unsigned int *lock_mask,
			   size_t max_size, unsigned int cpu_mult,
			   gfp_t gfp);

void free_bucket_spinlocks(spinlock_t *locks);
#endif /* LINUX_VERSION_IS_LESS(4,16,0) */

#if LINUX_VERSION_IS_LESS(4,19,0)
int __alloc_bucket_spinlocks(spinlock_t **locks, unsigned int *lock_mask,
			     size_t max_size, unsigned int cpu_mult,
			     gfp_t gfp, const char *name,
			     struct lock_class_key *key);

#define alloc_bucket_spinlocks(locks, lock_mask, max_size, cpu_mult, gfp)    \
	({								     \
		static struct lock_class_key key;			     \
		int ret;						     \
									     \
		ret = __alloc_bucket_spinlocks(locks, lock_mask, max_size,   \
					       cpu_mult, gfp, #locks, &key); \
		ret;							\
	})
#endif /* LINUX_VERSION_IS_LESS(4,19,0) */

#endif /* __BACKPORT_SPINLOCK_H */
