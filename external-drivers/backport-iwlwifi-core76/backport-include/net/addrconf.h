#ifndef _BACKPORT_NET_ADDRCONF_H
#define _BACKPORT_NET_ADDRCONF_H 1

#include_next <net/addrconf.h>

#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,1,0)
static inline int backport_ipv6_mc_check_mld(struct sk_buff *skb)
{
	return ipv6_mc_check_mld(skb, NULL);
}
#define ipv6_mc_check_mld LINUX_BACKPORT(ipv6_mc_check_mld)
#endif

#endif	/* _BACKPORT_NET_ADDRCONF_H */
