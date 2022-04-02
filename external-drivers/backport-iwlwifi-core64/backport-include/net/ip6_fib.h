#ifndef __BACKPORT_NET_IP6_ROUTE_H
#define __BACKPORT_NET_IP6_ROUTE_H
#include_next <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <linux/route.h>
#include <linux/version.h>

/*
 * This function is avaliable with one argument since kernel 3.10, but the
 * secound one was added in 4.2.
 */
#if LINUX_VERSION_IS_LESS(4,2,0)
#define rt6_nexthop LINUX_BACKPORT(rt6_nexthop)
static inline struct in6_addr *rt6_nexthop(struct rt6_info *rt,
					   struct in6_addr *daddr)
{
	if (rt->rt6i_flags & RTF_GATEWAY)
		return &rt->rt6i_gateway;
	else if (rt->rt6i_flags & RTF_CACHE)
		return &rt->rt6i_dst.addr;
	else
		return daddr;
}
#endif /* LINUX_VERSION_IS_LESS(4,2,0) */

#endif /* __BACKPORT_NET_IP6_ROUTE_H */
