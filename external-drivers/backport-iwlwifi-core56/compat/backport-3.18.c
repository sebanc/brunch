/*
 * Copyright (c) 2014  Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Backport functionality introduced in Linux 3.18.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <scsi/fc/fc_fcoe.h>
#include <linux/skbuff.h>
#include <linux/errqueue.h>
#include <linux/wait.h>
#include <linux/of.h>
#include <linux/string.h>

/**
 * eth_get_headlen - determine the the length of header for an ethernet frame
 * @data: pointer to start of frame
 * @len: total length of frame
 *
 * Make a best effort attempt to pull the length for all of the headers for
 * a given frame in a linear buffer.
 */
int eth_get_headlen(unsigned char *data, unsigned int max_len)
{
	union {
		unsigned char *network;
		/* l2 headers */
		struct ethhdr *eth;
		struct vlan_hdr *vlan;
		/* l3 headers */
		struct iphdr *ipv4;
		struct ipv6hdr *ipv6;
	} hdr;
	__be16 protocol;
	u8 nexthdr = 0;	/* default to not TCP */
	u8 hlen;

	/* this should never happen, but better safe than sorry */
	if (max_len < ETH_HLEN)
		return max_len;

	/* initialize network frame pointer */
	hdr.network = data;

	/* set first protocol and move network header forward */
	protocol = hdr.eth->h_proto;
	hdr.network += ETH_HLEN;

	/* handle any vlan tag if present */
	if (protocol == htons(ETH_P_8021Q)) {
		if ((hdr.network - data) > (max_len - VLAN_HLEN))
			return max_len;

		protocol = hdr.vlan->h_vlan_encapsulated_proto;
		hdr.network += VLAN_HLEN;
	}

	/* handle L3 protocols */
	if (protocol == htons(ETH_P_IP)) {
		if ((hdr.network - data) > (max_len - sizeof(struct iphdr)))
			return max_len;

		/* access ihl as a u8 to avoid unaligned access on ia64 */
		hlen = (hdr.network[0] & 0x0F) << 2;

		/* verify hlen meets minimum size requirements */
		if (hlen < sizeof(struct iphdr))
			return hdr.network - data;

		/* record next protocol if header is present */
		if (!(hdr.ipv4->frag_off & htons(IP_OFFSET)))
			nexthdr = hdr.ipv4->protocol;
	} else if (protocol == htons(ETH_P_IPV6)) {
		if ((hdr.network - data) > (max_len - sizeof(struct ipv6hdr)))
			return max_len;

		/* record next protocol */
		nexthdr = hdr.ipv6->nexthdr;
		hlen = sizeof(struct ipv6hdr);
	} else if (protocol == htons(ETH_P_FCOE)) {
		if ((hdr.network - data) > (max_len - FCOE_HEADER_LEN))
			return max_len;
		hlen = FCOE_HEADER_LEN;
	} else {
		return hdr.network - data;
	}

	/* relocate pointer to start of L4 header */
	hdr.network += hlen;

	/* finally sort out TCP/UDP */
	if (nexthdr == IPPROTO_TCP) {
		if ((hdr.network - data) > (max_len - sizeof(struct tcphdr)))
			return max_len;

		/* access doff as a u8 to avoid unaligned access on ia64 */
		hlen = (hdr.network[12] & 0xF0) >> 2;

		/* verify hlen meets minimum size requirements */
		if (hlen < sizeof(struct tcphdr))
			return hdr.network - data;

		hdr.network += hlen;
	} else if (nexthdr == IPPROTO_UDP) {
		if ((hdr.network - data) > (max_len - sizeof(struct udphdr)))
			return max_len;

		hdr.network += sizeof(struct udphdr);
	}

	/*
	 * If everything has gone correctly hdr.network should be the
	 * data section of the packet and will be the end of the header.
	 * If not then it probably represents the end of the last recognized
	 * header.
	 */
	if ((hdr.network - data) < max_len)
		return hdr.network - data;
	else
		return max_len;
}
EXPORT_SYMBOL_GPL(eth_get_headlen);

#define sock_efree LINUX_BACKPORT(sock_efree)
static void sock_efree(struct sk_buff *skb)
{
	sock_put(skb->sk);
}

/**
 * skb_clone_sk - create clone of skb, and take reference to socket
 * @skb: the skb to clone
 *
 * This function creates a clone of a buffer that holds a reference on
 * sk_refcnt.  Buffers created via this function are meant to be
 * returned using sock_queue_err_skb, or free via kfree_skb.
 *
 * When passing buffers allocated with this function to sock_queue_err_skb
 * it is necessary to wrap the call with sock_hold/sock_put in order to
 * prevent the socket from being released prior to being enqueued on
 * the sk_error_queue.
 */
struct sk_buff *skb_clone_sk(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct sk_buff *clone;

	if (!sk || !atomic_inc_not_zero(&sk->sk_refcnt))
		return NULL;

	clone = skb_clone(skb, GFP_ATOMIC);
	if (!clone) {
		sock_put(sk);
		return NULL;
	}

	clone->sk = sk;
	clone->destructor = sock_efree;

	return clone;
}
EXPORT_SYMBOL_GPL(skb_clone_sk);

#if LINUX_VERSION_IS_GEQ(3,3,0)
/*
 * skb_complete_wifi_ack() needs to get backported, because the version from
 * 3.18 added the sock_hold() and sock_put() calles missing in older versions.
 */
void skb_complete_wifi_ack(struct sk_buff *skb, bool acked)
{
	struct sock *sk = skb->sk;
	struct sock_exterr_skb *serr;
	int err;

#if LINUX_VERSION_IS_GEQ(3,3,0)
	skb->wifi_acked_valid = 1;
	skb->wifi_acked = acked;
#endif

	serr = SKB_EXT_ERR(skb);
	memset(serr, 0, sizeof(*serr));
	serr->ee.ee_errno = ENOMSG;
	serr->ee.ee_origin = SO_EE_ORIGIN_TXSTATUS;

	/* take a reference to prevent skb_orphan() from freeing the socket */
	sock_hold(sk);

	err = sock_queue_err_skb(sk, skb);
	if (err)
		kfree_skb(skb);

	sock_put(sk);
}
EXPORT_SYMBOL_GPL(skb_complete_wifi_ack);
#endif

#if LINUX_VERSION_IS_GEQ(3,17,0)
int __sched out_of_line_wait_on_bit_timeout(
	void *word, int bit, wait_bit_action_f *action,
	unsigned mode, unsigned long timeout)
{
	wait_queue_head_t *wq = bit_waitqueue(word, bit);
	DEFINE_WAIT_BIT(wait, word, bit);

	wait.key.private = jiffies + timeout;
	return __wait_on_bit(wq, &wait, action, mode);
}
EXPORT_SYMBOL_GPL(out_of_line_wait_on_bit_timeout);

__sched int bit_wait_timeout(struct wait_bit_key *word)
{
	unsigned long now = ACCESS_ONCE(jiffies);
	if (signal_pending_state(current->state, current))
		return 1;
	if (time_after_eq(now, word->private))
		return -EAGAIN;
	schedule_timeout(word->private - now);
	return 0;
}
EXPORT_SYMBOL_GPL(bit_wait_timeout);
#endif

#ifdef CONFIG_OF
/**
 * of_find_property_value_of_size
 *
 * @np:		device node from which the property value is to be read.
 * @propname:	name of the property to be searched.
 * @len:	requested length of property value
 *
 * Search for a property in a device node and valid the requested size.
 * Returns the property value on success, -EINVAL if the property does not
 *  exist, -ENODATA if property does not have a value, and -EOVERFLOW if the
 * property data isn't large enough.
 *
 */
void *of_find_property_value_of_size(const struct device_node *np,
				     const char *propname, u32 len)
{
	struct property *prop = of_find_property(np, propname, NULL);

	if (!prop)
		return ERR_PTR(-EINVAL);
	if (!prop->value)
		return ERR_PTR(-ENODATA);
	if (len > prop->length)
		return ERR_PTR(-EOVERFLOW);

	return prop->value;
}

/**
 * of_property_read_u64_array - Find and read an array of 64 bit integers
 * from a property.
 *
 * @np:		device node from which the property value is to be read.
 * @propname:	name of the property to be searched.
 * @out_values:	pointer to return value, modified only if return value is 0.
 * @sz:		number of array elements to read
 *
 * Search for a property in a device node and read 64-bit value(s) from
 * it. Returns 0 on success, -EINVAL if the property does not exist,
 * -ENODATA if property does not have a value, and -EOVERFLOW if the
 * property data isn't large enough.
 *
 * The out_values is modified only if a valid u64 value can be decoded.
 */
int of_property_read_u64_array(const struct device_node *np,
			       const char *propname, u64 *out_values,
			       size_t sz)
{
	const __be32 *val = of_find_property_value_of_size(np, propname,
						(sz * sizeof(*out_values)));

	if (IS_ERR(val))
		return PTR_ERR(val);

	while (sz--) {
		*out_values++ = of_read_number(val, 2);
		val += 2;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(of_property_read_u64_array);
#endif /* CONFIG_OF */

#if !(LINUX_VERSION_IS_GEQ(3,17,3) || \
      (LINUX_VERSION_IS_GEQ(3,14,24) && \
      LINUX_VERSION_IS_LESS(3,15,0)) || \
      (LINUX_VERSION_IS_GEQ(3,12,33) && \
      LINUX_VERSION_IS_LESS(3,13,0)) || \
      (LINUX_VERSION_IS_GEQ(3,10,60) && \
      LINUX_VERSION_IS_LESS(3,11,0)) || \
      (LINUX_VERSION_IS_GEQ(3,4,106) && \
      LINUX_VERSION_IS_LESS(3,5,0)) || \
      (LINUX_VERSION_IS_GEQ(3,2,65) && \
      LINUX_VERSION_IS_LESS(3,3,0)))
/**
 * memzero_explicit - Fill a region of memory (e.g. sensitive
 *		      keying data) with 0s.
 * @s: Pointer to the start of the area.
 * @count: The size of the area.
 *
 * Note: usually using memset() is just fine (!), but in cases
 * where clearing out _local_ data at the end of a scope is
 * necessary, memzero_explicit() should be used instead in
 * order to prevent the compiler from optimising away zeroing.
 *
 * memzero_explicit() doesn't need an arch-specific version as
 * it just invokes the one of memset() implicitly.
 */
void memzero_explicit(void *s, size_t count)
{
	memset(s, 0, count);
	barrier_data(s);
}
EXPORT_SYMBOL_GPL(memzero_explicit);
#endif

char *bin2hex(char *dst, const void *src, size_t count)
{
	const unsigned char *_src = src;

	while (count--)
		dst = hex_byte_pack(dst, *_src++);
	return dst;
}
EXPORT_SYMBOL(bin2hex);
