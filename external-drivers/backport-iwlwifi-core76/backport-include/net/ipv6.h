#ifndef __BACKPORT_NET_IPV6_H
#define __BACKPORT_NET_IPV6_H
#include_next <net/ipv6.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(4,5,0)
#define ipv6_addr_prefix_copy LINUX_BACKPORT(ipv6_addr_prefix_copy)
static inline void ipv6_addr_prefix_copy(struct in6_addr *addr,
					 const struct in6_addr *pfx,
					 int plen)
{
	/* caller must guarantee 0 <= plen <= 128 */
	int o = plen >> 3,
	    b = plen & 0x7;

	memcpy(addr->s6_addr, pfx, o);
	if (b != 0) {
		addr->s6_addr[o] &= ~(0xff00 >> b);
		addr->s6_addr[o] |= (pfx->s6_addr[o] & (0xff00 >> b));
	}
}
#endif

#endif /* __BACKPORT_NET_IPV6_H */
