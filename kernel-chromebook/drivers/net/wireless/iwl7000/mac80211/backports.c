/*
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 Intel Corporation
 * Copyright (C) 2020 Intel Corporation
 *
 * Backport functionality introduced in Linux 4.4.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "mac80211-exp.h"

#include <linux/export.h>
#include <linux/if_vlan.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <net/ip.h>
#include <asm/unaligned.h>
#include <linux/device.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0)
static unsigned int __ieee80211_get_mesh_hdrlen(u8 flags)
{
	int ae = flags & MESH_FLAGS_AE;
	/* 802.11-2012, 8.2.4.7.3 */
	switch (ae) {
	default:
	case 0:
		return 6;
	case MESH_FLAGS_AE_A4:
		return 12;
	case MESH_FLAGS_AE_A5_A6:
		return 18;
	}
}

int ieee80211_data_to_8023_exthdr(struct sk_buff *skb, struct ethhdr *ehdr,
				  const u8 *addr, enum nl80211_iftype iftype,
				  u8 data_offset)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	struct {
		u8 hdr[ETH_ALEN] __aligned(2);
		__be16 proto;
	} payload;
	struct ethhdr tmp;
	u16 hdrlen;
	u8 mesh_flags = 0;

	if (unlikely(!ieee80211_is_data_present(hdr->frame_control)))
		return -1;

	hdrlen = ieee80211_hdrlen(hdr->frame_control) + data_offset;
	if (skb->len < hdrlen + 8)
		return -1;

	/* convert IEEE 802.11 header + possible LLC headers into Ethernet
	 * header
	 * IEEE 802.11 address fields:
	 * ToDS FromDS Addr1 Addr2 Addr3 Addr4
	 *   0     0   DA    SA    BSSID n/a
	 *   0     1   DA    BSSID SA    n/a
	 *   1     0   BSSID SA    DA    n/a
	 *   1     1   RA    TA    DA    SA
	 */
	memcpy(tmp.h_dest, ieee80211_get_DA(hdr), ETH_ALEN);
	memcpy(tmp.h_source, ieee80211_get_SA(hdr), ETH_ALEN);

	if (iftype == NL80211_IFTYPE_MESH_POINT)
		skb_copy_bits(skb, hdrlen, &mesh_flags, 1);

	mesh_flags &= MESH_FLAGS_AE;

	switch (hdr->frame_control &
		cpu_to_le16(IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS)) {
	case cpu_to_le16(IEEE80211_FCTL_TODS):
		if (unlikely(iftype != NL80211_IFTYPE_AP &&
			     iftype != NL80211_IFTYPE_AP_VLAN &&
			     iftype != NL80211_IFTYPE_P2P_GO))
			return -1;
		break;
	case cpu_to_le16(IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS):
		if (unlikely(iftype != NL80211_IFTYPE_WDS &&
			     iftype != NL80211_IFTYPE_MESH_POINT &&
			     iftype != NL80211_IFTYPE_AP_VLAN &&
			     iftype != NL80211_IFTYPE_STATION))
			return -1;
		if (iftype == NL80211_IFTYPE_MESH_POINT) {
			if (mesh_flags == MESH_FLAGS_AE_A4)
				return -1;
			if (mesh_flags == MESH_FLAGS_AE_A5_A6) {
				skb_copy_bits(skb, hdrlen +
					offsetof(struct ieee80211s_hdr, eaddr1),
					tmp.h_dest, 2 * ETH_ALEN);
			}
			hdrlen += __ieee80211_get_mesh_hdrlen(mesh_flags);
		}
		break;
	case cpu_to_le16(IEEE80211_FCTL_FROMDS):
		if ((iftype != NL80211_IFTYPE_STATION &&
		     iftype != NL80211_IFTYPE_P2P_CLIENT &&
		     iftype != NL80211_IFTYPE_MESH_POINT) ||
		    (is_multicast_ether_addr(tmp.h_dest) &&
		     ether_addr_equal(tmp.h_source, addr)))
			return -1;
		if (iftype == NL80211_IFTYPE_MESH_POINT) {
			if (mesh_flags == MESH_FLAGS_AE_A5_A6)
				return -1;
			if (mesh_flags == MESH_FLAGS_AE_A4)
				skb_copy_bits(skb, hdrlen +
					offsetof(struct ieee80211s_hdr, eaddr1),
					tmp.h_source, ETH_ALEN);
			hdrlen += __ieee80211_get_mesh_hdrlen(mesh_flags);
		}
		break;
	case cpu_to_le16(0):
		if (iftype != NL80211_IFTYPE_ADHOC &&
		    iftype != NL80211_IFTYPE_STATION &&
		    !ieee80211_viftype_ocb(iftype))
				return -1;
		break;
	}

	skb_copy_bits(skb, hdrlen, &payload, sizeof(payload));
	tmp.h_proto = payload.proto;

	if (likely((ether_addr_equal(payload.hdr, rfc1042_header) &&
		    tmp.h_proto != htons(ETH_P_AARP) &&
		    tmp.h_proto != htons(ETH_P_IPX)) ||
		   ether_addr_equal(payload.hdr, bridge_tunnel_header)))
		/* remove RFC1042 or Bridge-Tunnel encapsulation and
		 * replace EtherType */
		hdrlen += ETH_ALEN + 2;
	else
		tmp.h_proto = htons(skb->len - hdrlen);

	pskb_pull(skb, hdrlen);

	if (!ehdr)
		ehdr = skb_push(skb, sizeof(struct ethhdr));
	memcpy(ehdr, &tmp, sizeof(tmp));

	return 0;
}
EXPORT_SYMBOL(/* don't auto-generate a rename */
	ieee80211_data_to_8023_exthdr);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,18,0) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
static void
__frame_add_frag(struct sk_buff *skb, struct page *page,
		 void *ptr, int len, int size)
{
	struct skb_shared_info *sh = skb_shinfo(skb);
	int page_offset;

	get_page(page);
	page_offset = ptr - page_address(page);
	skb_add_rx_frag(skb, sh->nr_frags, page, page_offset, len, size);
}

static void
__ieee80211_amsdu_copy_frag(struct sk_buff *skb, struct sk_buff *frame,
			    int offset, int len)
{
	struct skb_shared_info *sh = skb_shinfo(skb);
	const skb_frag_t *frag = &sh->frags[0];
	struct page *frag_page;
	void *frag_ptr;
	int frag_len, frag_size;
	int head_size = skb->len - skb->data_len;
	int cur_len;

	frag_page = virt_to_head_page(skb->head);
	frag_ptr = skb->data;
	frag_size = head_size;

	while (offset >= frag_size) {
		offset -= frag_size;
		frag_page = skb_frag_page(frag);
		frag_ptr = skb_frag_address(frag);
		frag_size = skb_frag_size(frag);
		frag++;
	}

	frag_ptr += offset;
	frag_len = frag_size - offset;

	cur_len = min(len, frag_len);

	__frame_add_frag(frame, frag_page, frag_ptr, cur_len, frag_size);
	len -= cur_len;

	while (len > 0) {
		frag_len = skb_frag_size(frag);
		cur_len = min(len, frag_len);
		__frame_add_frag(frame, skb_frag_page(frag),
				 skb_frag_address(frag), cur_len, frag_len);
		len -= cur_len;
		frag++;
	}
}

static struct sk_buff *
__ieee80211_amsdu_copy(struct sk_buff *skb, unsigned int hlen,
		       int offset, int len, bool reuse_frag)
{
	struct sk_buff *frame;
	int cur_len = len;

	if (skb->len - offset < len)
		return NULL;

	/*
	 * When reusing framents, copy some data to the head to simplify
	 * ethernet header handling and speed up protocol header processing
	 * in the stack later.
	 */
	if (reuse_frag)
		cur_len = min_t(int, len, 32);

	/*
	 * Allocate and reserve two bytes more for payload
	 * alignment since sizeof(struct ethhdr) is 14.
	 */
	frame = dev_alloc_skb(hlen + sizeof(struct ethhdr) + 2 + cur_len);
	if (!frame)
		return NULL;

	skb_reserve(frame, hlen + sizeof(struct ethhdr) + 2);
	skb_copy_bits(skb, offset, skb_put(frame, cur_len), cur_len);

	len -= cur_len;
	if (!len)
		return frame;

	offset += cur_len;
	__ieee80211_amsdu_copy_frag(skb, frame, offset, len);

	return frame;
}

void ieee80211_amsdu_to_8023s(struct sk_buff *skb, struct sk_buff_head *list,
			      const u8 *addr, enum nl80211_iftype iftype,
			      const unsigned int extra_headroom,
			      const u8 *check_da, const u8 *check_sa)
{
	unsigned int hlen = ALIGN(extra_headroom, 4);
	struct sk_buff *frame = NULL;
	u16 ethertype;
	u8 *payload;
	int offset = 0, remaining;
	struct ethhdr eth;
	bool reuse_frag = skb->head_frag && !skb_has_frag_list(skb);
	bool reuse_skb = false;
	bool last = false;

	while (!last) {
		unsigned int subframe_len;
		int len;
		u8 padding;

		skb_copy_bits(skb, offset, &eth, sizeof(eth));
		len = ntohs(eth.h_proto);
		subframe_len = sizeof(struct ethhdr) + len;
		padding = (4 - subframe_len) & 0x3;

		/* the last MSDU has no padding */
		remaining = skb->len - offset;
		if (subframe_len > remaining)
			goto purge;

		offset += sizeof(struct ethhdr);
		last = remaining <= subframe_len + padding;

		/* FIXME: should we really accept multicast DA? */
		if ((check_da && !is_multicast_ether_addr(eth.h_dest) &&
		     !ether_addr_equal(check_da, eth.h_dest)) ||
		    (check_sa && !ether_addr_equal(check_sa, eth.h_source))) {
			offset += len + padding;
			continue;
		}

		/* reuse skb for the last subframe */
		if (!skb_is_nonlinear(skb) && !reuse_frag && last) {
			skb_pull(skb, offset);
			frame = skb;
			reuse_skb = true;
		} else {
			frame = __ieee80211_amsdu_copy(skb, hlen, offset, len,
						       reuse_frag);
			if (!frame)
				goto purge;

			offset += len + padding;
		}

		skb_reset_network_header(frame);
		frame->dev = skb->dev;
		frame->priority = skb->priority;

		payload = frame->data;
		ethertype = (payload[6] << 8) | payload[7];
		if (likely((ether_addr_equal(payload, rfc1042_header) &&
			    ethertype != ETH_P_AARP && ethertype != ETH_P_IPX) ||
			   ether_addr_equal(payload, bridge_tunnel_header))) {
			eth.h_proto = htons(ethertype);
			skb_pull(frame, ETH_ALEN + 2);
		}

		memcpy(skb_push(frame, sizeof(eth)), &eth, sizeof(eth));
		__skb_queue_tail(list, frame);
	}

	if (!reuse_skb)
		dev_kfree_skb(skb);

	return;

 purge:
	__skb_queue_purge(list);
	dev_kfree_skb(skb);
}
EXPORT_SYMBOL(/* don't auto-generate a rename */
	ieee80211_amsdu_to_8023s);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
#include <linux/devcoredump.h>

static void devcd_free_sgtable(void *data)
{
	struct scatterlist *table = data;
	int i;
	struct page *page;
	struct scatterlist *iter;
	struct scatterlist *delete_iter;

	/* free pages */
	iter = table;
	for_each_sg(table, iter, sg_nents(table), i) {
		page = sg_page(iter);
		if (page)
			__free_page(page);
	}

	/* then free all chained tables */
	iter = table;
	delete_iter = table;	/* always points on a head of a table */
	while (!sg_is_last(iter)) {
		iter++;
		if (sg_is_chain(iter)) {
			iter = sg_chain_ptr(iter);
			kfree(delete_iter);
			delete_iter = iter;
		}
	}

	/* free the last table */
	kfree(delete_iter);
}

static ssize_t devcd_read_from_sgtable(char *buffer, loff_t offset,
				       size_t buf_len, void *data,
				       size_t data_len)
{
	struct scatterlist *table = data;

	if (offset > data_len)
		return -EINVAL;

	if (offset + buf_len > data_len)
		buf_len = data_len - offset;
	return sg_pcopy_to_buffer(table, sg_nents(table), buffer, buf_len,
				  offset);
}

void dev_coredumpsg(struct device *dev, struct scatterlist *table,
		    size_t datalen, gfp_t gfp)
{
	/*
	 * those casts are needed due to const issues, but it's fine
	 * since calling convention for const/non-const is identical
	 */
	dev_coredumpm(dev, THIS_MODULE, table, datalen, gfp,
		      (void *)devcd_read_from_sgtable,
		      (void *)devcd_free_sgtable);
}
EXPORT_SYMBOL_GPL(dev_coredumpsg);
#endif /* < 4.7.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
int kstrtobool(const char *s, bool *res)
{
	if (!s)
		return -EINVAL;

	switch (s[0]) {
	case 'y':
	case 'Y':
	case '1':
		*res = true;
		return 0;
	case 'n':
	case 'N':
	case '0':
		*res = false;
		return 0;
	case 'o':
	case 'O':
		switch (s[1]) {
		case 'n':
		case 'N':
			*res = true;
			return 0;
		case 'f':
		case 'F':
			*res = false;
			return 0;
		default:
			break;
		}
	default:
		break;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(kstrtobool);

int kstrtobool_from_user(const char __user *s, size_t count, bool *res)
{
	/* Longest string needed to differentiate, newline, terminator */
	char buf[4];

	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, s, count))
		return -EFAULT;
	buf[count] = '\0';
	return kstrtobool(buf, res);
}
EXPORT_SYMBOL_GPL(kstrtobool_from_user);
#endif /* < 4.6.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
#ifdef CONFIG_DEBUG_FS
static ssize_t _debugfs_read_file_bool(struct file *file, char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	char buf[3];
	bool *val = file->private_data;

	if (*val)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t _debugfs_write_file_bool(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	bool bv;
	bool *val = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = '\0';
	if (strtobool(buf, &bv) == 0)
		*val = bv;

	return count;
}

static const struct file_operations fops_bool = {
	.read =		_debugfs_read_file_bool,
	.write =	_debugfs_write_file_bool,
	.open =		simple_open,
	.llseek =	default_llseek,
};

struct dentry *iwl_debugfs_create_bool(const char *name, umode_t mode,
				       struct dentry *parent, bool *value)
{
	return debugfs_create_file(name, mode, parent, value, &fops_bool);
}
EXPORT_SYMBOL_GPL(iwl_debugfs_create_bool);

#endif /* CONFIG_DEBUG_FS */

/* Calculate expected number of TX descriptors */
int tso_count_descs(struct sk_buff *skb)
{
	/* The Marvell Way */
	return skb_shinfo(skb)->gso_segs * 2 + skb_shinfo(skb)->nr_frags;
}
EXPORT_SYMBOL(tso_count_descs);

void tso_build_hdr(struct sk_buff *skb, char *hdr, struct tso_t *tso,
		   int size, bool is_last)
{
	struct tcphdr *tcph;
	int hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
	int mac_hdr_len = skb_network_offset(skb);

	memcpy(hdr, skb->data, hdr_len);
	if (!tso->ipv6) {
		struct iphdr *iph = (void *)(hdr + mac_hdr_len);

		iph->id = htons(tso->ip_id);
		iph->tot_len = htons(size + hdr_len - mac_hdr_len);
		tso->ip_id++;
	} else {
		struct ipv6hdr *iph = (void *)(hdr + mac_hdr_len);

		iph->payload_len = htons(size + tcp_hdrlen(skb));
	}
	tcph = (struct tcphdr *)(hdr + skb_transport_offset(skb));
	put_unaligned_be32(tso->tcp_seq, &tcph->seq);

	if (!is_last) {
		/* Clear all special flags for not last packet */
		tcph->psh = 0;
		tcph->fin = 0;
		tcph->rst = 0;
	}
}
EXPORT_SYMBOL(tso_build_hdr);

void tso_build_data(struct sk_buff *skb, struct tso_t *tso, int size)
{
	tso->tcp_seq += size;
	tso->size -= size;
	tso->data += size;

	if ((tso->size == 0) &&
	    (tso->next_frag_idx < skb_shinfo(skb)->nr_frags)) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[tso->next_frag_idx];

		/* Move to next segment */
		tso->size = frag->size;
		tso->data = page_address(frag->page.p) + frag->page_offset;
		tso->next_frag_idx++;
	}
}
EXPORT_SYMBOL(tso_build_data);

void tso_start(struct sk_buff *skb, struct tso_t *tso)
{
	int hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);

	tso->ip_id = ntohs(ip_hdr(skb)->id);
	tso->tcp_seq = ntohl(tcp_hdr(skb)->seq);
	tso->next_frag_idx = 0;
	tso->ipv6 = vlan_get_protocol(skb) == htons(ETH_P_IPV6);

	/* Build first data */
	tso->size = skb_headlen(skb) - hdr_len;
	tso->data = skb->data + hdr_len;
	if ((tso->size == 0) &&
	    (tso->next_frag_idx < skb_shinfo(skb)->nr_frags)) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[tso->next_frag_idx];

		/* Move to next segment */
		tso->size = frag->size;
		tso->data = page_address(frag->page.p) + frag->page_offset;
		tso->next_frag_idx++;
	}
}
EXPORT_SYMBOL(tso_start);

/* This was only included in upstream v4.6, but was cherry-picked to
 * chromeos-4.4.  Since we don't expect to have a chromeos-4.5, we can
 * just check on < 4.4.
 */
/**
 * match_string - matches given string in an array
 * @array:	array of strings
 * @n:		number of strings in the array or -1 for NULL terminated arrays
 * @string:	string to match with
 *
 * Return:
 * index of a @string in the @array if matches, or %-EINVAL otherwise.
 */
int match_string(const char * const *array, size_t n, const char *string)
{
	int index;
	const char *item;

	for (index = 0; index < n; index++) {
		item = array[index];
		if (!item)
			break;
		if (!strcmp(item, string))
			return index;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(match_string);
#endif /* < 4.4.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define NETDEV_RSS_KEY_LEN 40

void netdev_rss_key_fill(void *buffer, size_t len)
{
	static u8 netdev_rss_key[NETDEV_RSS_KEY_LEN];
	static bool initialized;

	BUG_ON(len > sizeof(netdev_rss_key));
	/*
	 * A crude "do once". It's not protected and potentially racy, but we
	 * will get here only on mvm_up which is protected by the rtnl lock.
	 */
	if (!initialized) {
		get_random_bytes(netdev_rss_key, sizeof(netdev_rss_key));
		initialized = true;
	}
	memcpy(buffer, netdev_rss_key, len);
}
EXPORT_SYMBOL_GPL(netdev_rss_key_fill);
#endif /* < 3.19.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
/**
 * devm_kvasprintf - Allocate resource managed space
 *			for the formatted string.
 * @dev: Device to allocate memory for
 * @gfp: the GFP mask used in the devm_kmalloc() call when
 *       allocating memory
 * @fmt: the formatted string to duplicate
 * @ap: the list of tokens to be placed in the formatted string
 * RETURNS:
 * Pointer to allocated string on success, NULL on failure.
 */
char *devm_kvasprintf(struct device *dev, gfp_t gfp, const char *fmt,
		      va_list ap)
{
	unsigned int len;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	p = devm_kzalloc(dev, len + 1, gfp);
	if (!p)
		return NULL;

	vsnprintf(p, len + 1, fmt, ap);

	return p;
}
EXPORT_SYMBOL_GPL(devm_kvasprintf);

/**
 * devm_kasprintf - Allocate resource managed space
 *		and copy an existing formatted string into that
 * @dev: Device to allocate memory for
 * @gfp: the GFP mask used in the devm_kmalloc() call when
 *       allocating memory
 * @fmt: the string to duplicate
 * RETURNS:
 * Pointer to allocated string on success, NULL on failure.
 */
char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = devm_kvasprintf(dev, gfp, fmt, ap);
	va_end(ap);

	return p;
}
EXPORT_SYMBOL_GPL(devm_kasprintf);
#endif /* < 3.17 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#undef genl_register_family
#undef genl_unregister_family

int __backport_genl_register_family(struct genl_family *family)
{
	int i, ret;

#define __copy(_field) family->family._field = family->_field
	__copy(id);
	__copy(hdrsize);
	__copy(version);
	__copy(maxattr);
	strncpy(family->family.name, family->name, sizeof(family->family.name));
	__copy(netnsok);
	__copy(pre_doit);
	__copy(post_doit);
#if LINUX_VERSION_IS_GEQ(3,10,0)
	__copy(parallel_ops);
#endif
#if LINUX_VERSION_IS_GEQ(3,11,0)
	__copy(module);
#endif
#undef __copy

	ret = genl_register_family(&family->family);
	if (ret < 0)
		return ret;

	family->attrbuf = family->family.attrbuf;
	family->id = family->family.id;

	for (i = 0; i < family->n_ops; i++) {
		ret = genl_register_ops(&family->family, &family->ops[i]);
		if (ret < 0)
			goto error;
	}

	for (i = 0; i < family->n_mcgrps; i++) {
		ret = genl_register_mc_group(&family->family,
					     &family->mcgrps[i]);
		if (ret)
			goto error;
	}

	return 0;

 error:
	backport_genl_unregister_family(family);
	return ret;
}
EXPORT_SYMBOL_GPL(__backport_genl_register_family);

int backport_genl_unregister_family(struct genl_family *family)
{
	int err;
	err = genl_unregister_family(&family->family);
	return err;
}
EXPORT_SYMBOL_GPL(backport_genl_unregister_family);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
static size_t sg_copy_buffer(struct scatterlist *sgl, unsigned int nents,
			     void *buf, size_t buflen, off_t skip,
			     bool to_buffer)
{
	unsigned int offset = 0;
	struct sg_mapping_iter miter;
	unsigned long flags;
	unsigned int sg_flags = SG_MITER_ATOMIC;

	if (to_buffer)
		sg_flags |= SG_MITER_FROM_SG;
	else
		sg_flags |= SG_MITER_TO_SG;

	sg_miter_start(&miter, sgl, nents, sg_flags);

	/* we'd like to call sg_miter_skip here, but it doesn't exist yet ... */

	local_irq_save(flags);

	while (sg_miter_next(&miter) && offset < buflen) {
		unsigned int len;
		unsigned int inner_offs = 0;

		/* so we do some extra bookkeeping here */
		if (miter.length <= skip) {
			skip -= miter.length;
			continue;
		} else if (skip) {
			inner_offs = skip;
			skip = 0;
		}

		len = min(miter.length - inner_offs, buflen - offset);

		if (to_buffer)
			memcpy(buf + offset, miter.addr + inner_offs, len);
		else
			memcpy(miter.addr + inner_offs, buf + offset, len);

		offset += len;
	}

	sg_miter_stop(&miter);

	local_irq_restore(flags);
	return offset;
}

size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents,
			    const void *buf, size_t buflen, off_t skip)
{
	return sg_copy_buffer(sgl, nents, (void *)buf, buflen, skip, false);
}
EXPORT_SYMBOL_GPL(sg_pcopy_from_buffer);

size_t sg_pcopy_to_buffer(struct scatterlist *sgl, unsigned int nents,
			  void *buf, size_t buflen, off_t skip)
{
	return sg_copy_buffer(sgl, nents, buf, buflen, skip, true);
}
EXPORT_SYMBOL_GPL(sg_pcopy_to_buffer);
#endif /* < 3.11 */

#if CFG80211_VERSION < KERNEL_VERSION(5,8,0)
#include "mac80211/ieee80211_i.h"
#include "mac80211/driver-ops.h"

void ieee80211_mgmt_frame_register(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   u16 frame_type, bool reg)
{
        struct ieee80211_local *local = wiphy_priv(wiphy);
        struct ieee80211_sub_if_data *sdata = IEEE80211_WDEV_TO_SUB_IF(wdev);

	switch (frame_type) {
	case IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_REQ:
		if (reg) {
			local->probe_req_reg = true;
			sdata->vif.probe_req_reg = true;
		} else {
			if (local->probe_req_reg)
				local->probe_req_reg = false;

			if (sdata->vif.probe_req_reg)
				sdata->vif.probe_req_reg = false;
		}

		if (!local->open_count)
			break;

		if (ieee80211_sdata_running(sdata)) {
			if (sdata->vif.probe_req_reg == 1)
				drv_config_iface_filter(local, sdata,
							FIF_PROBE_REQ,
							FIF_PROBE_REQ);
			else if (sdata->vif.probe_req_reg == 0)
				drv_config_iface_filter(local, sdata, 0,
							FIF_PROBE_REQ);
		}

                ieee80211_configure_filter(local);
		break;
	default:
		break;
	}
}
#endif /* < 5.8 */
