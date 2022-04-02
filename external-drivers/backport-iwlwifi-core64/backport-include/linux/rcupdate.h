#ifndef __BACKPORT_LINUX_RCUPDATE_H
#define __BACKPORT_LINUX_RCUPDATE_H
#include_next <linux/rcupdate.h>

/*
 * This adds a nested function everywhere kfree_rcu() was called. This
 * function frees the memory and is given as a function to call_rcu().
 * The rcu callback could happen every time also after the module was
 * unloaded and this will cause problems. To address that problem, we
 * put rcu_barrier() into each module_exit() in module.h.
 */
#if !defined(kfree_rcu)
#define kfree_rcu(data, rcuhead)		do {			\
		void __kfree_rcu_fn(struct rcu_head *rcu_head)		\
		{							\
			void *___ptr;					\
			___ptr = container_of(rcu_head, typeof(*(data)), rcuhead);\
			kfree(___ptr);					\
		}							\
		call_rcu(&(data)->rcuhead, __kfree_rcu_fn);		\
	} while (0)
#endif

#ifndef RCU_INIT_POINTER
#define RCU_INIT_POINTER(p, v) \
		p = (typeof(*v) __force __rcu *)(v)
#endif

#ifndef rcu_dereference_check
#define rcu_dereference_check(p, c) rcu_dereference(p)
#endif

#ifndef rcu_dereference_protected
#define rcu_dereference_protected(p, c) (p)
#endif
#ifndef rcu_access_pointer
#define rcu_access_pointer(p)   ACCESS_ONCE(p)
#endif

#ifndef rcu_dereference_raw
#define rcu_dereference_raw(p)	rcu_dereference(p)
#endif

#if LINUX_VERSION_IS_LESS(4,20,0)
typedef void (*rcu_callback_t)(struct rcu_head *head);

static inline void rcu_head_init(struct rcu_head *rhp)
{
        rhp->func = (rcu_callback_t)~0L;
}

static inline bool
rcu_head_after_call_rcu(struct rcu_head *rhp, rcu_callback_t f)
{
        if (READ_ONCE(rhp->func) == f)
                return true;
        WARN_ON_ONCE(READ_ONCE(rhp->func) != (rcu_callback_t)~0L);
        return false;
}
#endif /* < 4.20 */

#ifndef rcu_swap_protected
/**
 * rcu_swap_protected() - swap an RCU and a regular pointer
 * @rcu_ptr: RCU pointer
 * @ptr: regular pointer
 * @c: the conditions under which the dereference will take place
 *
 * Perform swap(@rcu_ptr, @ptr) where @rcu_ptr is an RCU-annotated pointer and
 * @c is the argument that is passed to the rcu_dereference_protected() call
 * used to read that pointer.
 */
#define rcu_swap_protected(rcu_ptr, ptr, c) do {			\
	typeof(ptr) __tmp = rcu_dereference_protected((rcu_ptr), (c));	\
	rcu_assign_pointer((rcu_ptr), (ptr));				\
	(ptr) = __tmp;							\
} while (0)
#endif

#ifndef rcu_replace_pointer
/**
 * rcu_replace_pointer() - replace an RCU pointer, returning its old value
 * @rcu_ptr: RCU pointer, whose old value is returned
 * @ptr: regular pointer
 * @c: the lockdep conditions under which the dereference will take place
 *
 * Perform a replacement, where @rcu_ptr is an RCU-annotated
 * pointer and @c is the lockdep argument that is passed to the
 * rcu_dereference_protected() call used to read that pointer.  The old
 * value of @rcu_ptr is returned, and @rcu_ptr is set to @ptr.
 */
#define rcu_replace_pointer(rcu_ptr, ptr, c)				\
({									\
	typeof(ptr) __tmp = rcu_dereference_protected((rcu_ptr), (c));	\
	rcu_assign_pointer((rcu_ptr), (ptr));				\
	__tmp;								\
})
#endif

#endif /* __BACKPORT_LINUX_RCUPDATE_H */
