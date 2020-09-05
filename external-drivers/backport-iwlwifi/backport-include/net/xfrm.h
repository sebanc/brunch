#ifndef __BACKPORT_NET_XFRM_H
#define __BACKPORT_NET_XFRM_H
#include_next <net/xfrm.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,4,0)
#define skb_ext_reset LINUX_BACKPORT(skb_ext_reset)
static inline void skb_ext_reset(struct sk_buff *skb)
{
	secpath_reset(skb);
}
#endif

#endif /* __BACKPORT_NET_XFRM_H */
