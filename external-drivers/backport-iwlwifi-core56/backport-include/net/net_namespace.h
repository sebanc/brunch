#ifndef _COMPAT_NET_NET_NAMESPACE_H
#define _COMPAT_NET_NET_NAMESPACE_H 1

#include_next <net/net_namespace.h>

#if LINUX_VERSION_IS_LESS(3,20,0)
/*
 * In older kernels we simply fail this function.
 */
#define get_net_ns_by_fd	LINUX_BACKPORT(get_net_ns_by_fd)
static inline struct net *get_net_ns_by_fd(int fd)
{
	return ERR_PTR(-EINVAL);
}
#endif

#if LINUX_VERSION_IS_LESS(4,1,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
typedef struct {
#ifdef CONFIG_NET_NS
	struct net *net;
#endif
} possible_net_t;

static inline void possible_write_pnet(possible_net_t *pnet, struct net *net)
{
#ifdef CONFIG_NET_NS
	pnet->net = net;
#endif
}

static inline struct net *possible_read_pnet(const possible_net_t *pnet)
{
#ifdef CONFIG_NET_NS
	return pnet->net;
#else
	return &init_net;
#endif
}
#else
#define possible_write_pnet(pnet, net) write_pnet(pnet, net)
#define possible_read_pnet(pnet) read_pnet(pnet)
#endif /* LINUX_VERSION_IS_LESS(4,1,0) */

#endif	/* _COMPAT_NET_NET_NAMESPACE_H */
