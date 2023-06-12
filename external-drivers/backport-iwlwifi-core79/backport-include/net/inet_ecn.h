#ifndef __BACKPORT_NET_INET_ECN_H
#define __BACKPORT_NET_INET_ECN_H
#include_next <net/inet_ecn.h>
#include <linux/version.h>
#include <linux/if_vlan.h>

#ifndef skb_vlan_tag_present
#define skb_vlan_tag_present(__skb)       ((__skb)->vlan_present)
#endif

#if LINUX_VERSION_IS_LESS(5,7,10) &&			\
	!LINUX_VERSION_IN_RANGE(4,14,212, 4,15,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,19,134, 4,20,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,4,248, 4,5,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,9,248, 4,10,0) &&	\
	!LINUX_VERSION_IN_RANGE(5,4,53, 5,5,0)
/* A getter for the SKB protocol field which will handle VLAN tags consistently
 * whether VLAN acceleration is enabled or not.
 */
static inline __be16 skb_protocol(const struct sk_buff *skb, bool skip_vlan)
{
	unsigned int offset = skb_mac_offset(skb) + sizeof(struct ethhdr);
	__be16 proto = skb->protocol;

	if (!skip_vlan)
		/* VLAN acceleration strips the VLAN header from the skb and
		 * moves it to skb->vlan_proto
		 */
		return skb_vlan_tag_present(skb) ? skb->vlan_proto : proto;

	while (eth_type_vlan(proto)) {
		struct vlan_hdr vhdr, *vh;

		vh = skb_header_pointer(skb, offset, sizeof(vhdr), &vhdr);
		if (!vh)
			break;

		proto = vh->h_vlan_encapsulated_proto;
		offset += sizeof(vhdr);
	}

	return proto;
}
#endif

#if LINUX_VERSION_IS_LESS(5,16,0)
#define skb_get_dsfield LINUX_BACKPORT(skb_get_dsfield)
static inline int skb_get_dsfield(struct sk_buff *skb)
{
	switch (skb_protocol(skb, true)) {
	case cpu_to_be16(ETH_P_IP):
		if (!pskb_network_may_pull(skb, sizeof(struct iphdr)))
			break;
		return ipv4_get_dsfield(ip_hdr(skb));

	case cpu_to_be16(ETH_P_IPV6):
		if (!pskb_network_may_pull(skb, sizeof(struct ipv6hdr)))
			break;
		return ipv6_get_dsfield(ipv6_hdr(skb));
	}

	return -1;
}
#endif

#endif /* __BACKPORT_NET_INET_ECN_H */
