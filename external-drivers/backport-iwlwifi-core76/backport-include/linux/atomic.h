#ifndef __BP_ATOMIC_H
#define __BP_ATOMIC_H
#include_next <linux/atomic.h>

/* atomic_cmpxchg_relaxed */
#ifndef atomic_cmpxchg_relaxed
#define  atomic_cmpxchg_relaxed		atomic_cmpxchg
#define  atomic_cmpxchg_acquire		atomic_cmpxchg
#define  atomic_cmpxchg_release		atomic_cmpxchg

#else /* atomic_cmpxchg_relaxed */

#ifndef atomic_cmpxchg_acquire
#define  atomic_cmpxchg_acquire(...)					\
	__atomic_op_acquire(atomic_cmpxchg, __VA_ARGS__)
#endif

#ifndef atomic_cmpxchg_release
#define  atomic_cmpxchg_release(...)					\
	__atomic_op_release(atomic_cmpxchg, __VA_ARGS__)
#endif

#ifndef atomic_cmpxchg
#define  atomic_cmpxchg(...)						\
	__atomic_op_fence(atomic_cmpxchg, __VA_ARGS__)
#endif
#endif /* atomic_cmpxchg_relaxed */

/* these were introduced together, so just a single check is enough */
#ifndef atomic_try_cmpxchg_acquire
#ifndef atomic_try_cmpxchg
#define __atomic_try_cmpxchg(type, _p, _po, _n)				\
({									\
	typeof(_po) __po = (_po);					\
	typeof(*(_po)) __r, __o = *__po;				\
	__r = atomic_cmpxchg##type((_p), __o, (_n));			\
	if (unlikely(__r != __o))					\
		*__po = __r;						\
	likely(__r == __o);						\
})

#define atomic_try_cmpxchg(_p, _po, _n)		__atomic_try_cmpxchg(, _p, _po, _n)
#define atomic_try_cmpxchg_relaxed(_p, _po, _n)	__atomic_try_cmpxchg(_relaxed, _p, _po, _n)
#define atomic_try_cmpxchg_acquire(_p, _po, _n)	__atomic_try_cmpxchg(_acquire, _p, _po, _n)
#define atomic_try_cmpxchg_release(_p, _po, _n)	__atomic_try_cmpxchg(_release, _p, _po, _n)
#else /* atomic_try_cmpxchg */
#define atomic_try_cmpxchg_relaxed	atomic_try_cmpxchg
#define atomic_try_cmpxchg_acquire	atomic_try_cmpxchg
#define atomic_try_cmpxchg_release	atomic_try_cmpxchg
#endif /* atomic_try_cmpxchg */

#endif /* atomic_try_cmpxchg_acquire */

#if LINUX_VERSION_IS_LESS(4,19,0)
#ifndef atomic_fetch_add_unless
static inline int atomic_fetch_add_unless(atomic_t *v, int a, int u)
{
	return __atomic_add_unless(v, a, u);
}
#endif
#endif

#ifndef __atomic_pre_full_fence
#define __atomic_pre_full_fence         smp_mb__before_atomic
#endif

#ifndef __atomic_post_full_fence
#define __atomic_post_full_fence        smp_mb__after_atomic
#endif

#if LINUX_VERSION_IS_LESS(4,8,0)
static inline int
atomic_fetch_add(int i, atomic_t *v)
{
	return atomic_add_return(i, v) - i;
}

static inline int
atomic_fetch_sub(int i, atomic_t *v)
{
	return atomic_sub_return(i, v) + i;
}
#endif

#ifndef atomic_fetch_add_relaxed
#define atomic_fetch_add_relaxed atomic_fetch_add
#endif

#ifndef atomic_fetch_sub_relaxed
#define atomic_fetch_sub_acquire atomic_fetch_sub
#define atomic_fetch_sub_release atomic_fetch_sub
#define atomic_fetch_sub_relaxed atomic_fetch_sub
#endif

#endif /* __BP_ATOMIC_H */
