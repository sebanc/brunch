#ifndef __BACKPORT_NET_IPV6_H
#define __BACKPORT_NET_IPV6_H
#include_next <net/ipv6.h>
#include <linux/version.h>
#include <net/addrconf.h>
#include <net/inet_frag.h>

#if LINUX_VERSION_IS_LESS(3,7,0)
/*
 *	Equivalent of ipv4 struct ip
 */
struct frag_queue {
	struct inet_frag_queue  q;

	__be32                  id;             /* fragment id          */
	u32                     user;
	struct in6_addr         saddr;
	struct in6_addr         daddr;

	int                     iif;
	unsigned int            csum;
	__u16                   nhoffset;
};
#endif /* LINUX_VERSION_IS_LESS(3,7,0) */

#if LINUX_VERSION_IS_LESS(3,6,0)
#define ipv6_addr_hash LINUX_BACKPORT(ipv6_addr_hash)
static inline u32 ipv6_addr_hash(const struct in6_addr *a)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
	const unsigned long *ul = (const unsigned long *)a;
	unsigned long x = ul[0] ^ ul[1];

	return (u32)(x ^ (x >> 32));
#else
	return (__force u32)(a->s6_addr32[0] ^ a->s6_addr32[1] ^
			     a->s6_addr32[2] ^ a->s6_addr32[3]);
#endif
}
#endif

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
