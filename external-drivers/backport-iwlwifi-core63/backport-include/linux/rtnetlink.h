#ifndef __BACKPORT_LINUX_RTNETLINK_H
#define __BACKPORT_LINUX_RTNETLINK_H
#include_next <linux/rtnetlink.h>

#ifndef rtnl_dereference
#define rtnl_dereference(p)                                     \
        rcu_dereference_protected(p, lockdep_rtnl_is_held())
#endif

#ifndef rcu_dereference_rtnl
#define rcu_dereference_rtnl(p)					\
	rcu_dereference_check(p, rcu_read_lock_held() ||	\
				 lockdep_rtnl_is_held())
#endif

#if LINUX_VERSION_IS_LESS(3,19,0)
#define ndo_dflt_fdb_add(ndm, tb, dev, addr, vid, flags) \
	ndo_dflt_fdb_add(ndm, tb, dev, addr, flags)
#endif

#if LINUX_VERSION_IS_LESS(3,13,0) &&		\
	!defined(CONFIG_PROVE_LOCKING) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline bool lockdep_rtnl_is_held(void)
{
	return true;
}
#endif

#endif /* __BACKPORT_LINUX_RTNETLINK_H */
