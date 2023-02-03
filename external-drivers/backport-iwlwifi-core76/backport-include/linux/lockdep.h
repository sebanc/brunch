#ifndef __BACKPORT_LINUX_LOCKDEP_H
#define __BACKPORT_LINUX_LOCKDEP_H
#include_next <linux/lockdep.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,15,0)
#ifndef CONFIG_LOCKDEP
struct lockdep_map { };
#endif /* CONFIG_LOCKDEP */
#endif /* LINUX_VERSION_IS_LESS(4,15,0) */

#if LINUX_VERSION_IS_LESS(5,11,0)
#if !defined(CONFIG_LOCKDEP) && !defined(lockdep_is_held)
/* upstream since 5.11 in this exact same way - calls compile away */
int lockdep_is_held(const void *);
#endif
#endif

#ifndef lockdep_assert_not_held
#ifdef CONFIG_LOCKDEP
#ifndef LOCK_STATE_HELD
#define LOCK_STATE_HELD		1
#endif /* LOCK_STATE_HELD */
#define lockdep_assert_not_held(l)	do {				\
		WARN_ON(debug_locks &&					\
			lockdep_is_held(l) == LOCK_STATE_HELD);		\
	} while (0)
#else
#define lockdep_assert_not_held(l)		do { (void)(l); } while (0)
#endif /* CONFIG_LOCKDEP */
#endif /* lockdep_assert_not_held */

#endif /* __BACKPORT_LINUX_LOCKDEP_H */
