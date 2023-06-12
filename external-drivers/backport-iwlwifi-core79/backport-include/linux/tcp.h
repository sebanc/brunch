#ifndef __BACKPORT_LINUX_TCP_H
#define __BACKPORT_LINUX_TCP_H

#include_next <linux/tcp.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(6,0,0)
static inline int skb_tcp_all_headers(const struct sk_buff *skb)
{
	return skb_transport_offset(skb) + tcp_hdrlen(skb);
}
#endif

#endif
