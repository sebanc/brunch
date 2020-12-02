/*
 * Copyright(c) 2015 Intel Deutschland GmbH
 *
 * Backport functionality introduced in Linux 4.4.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/if_vlan.h>
#include <linux/mm.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include <net/ip.h>
#include <net/tso.h>
#include <asm/unaligned.h>

#ifdef CONFIG_DEBUG_FS
#if LINUX_VERSION_IS_LESS(4,3,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static ssize_t debugfs_read_file_bool(struct file *file, char __user *user_buf,
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

static ssize_t debugfs_write_file_bool(struct file *file,
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
#endif /* < 4.3.0 */

static const struct file_operations fops_bool = {
	.read =		debugfs_read_file_bool,
	.write =	debugfs_write_file_bool,
	.open =		simple_open,
	.llseek =	default_llseek,
};

struct dentry *debugfs_create_bool(const char *name, umode_t mode,
				   struct dentry *parent, bool *value)
{
	return debugfs_create_file(name, mode, parent, value, &fops_bool);
}
EXPORT_SYMBOL_GPL(debugfs_create_bool);
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
#ifdef CONFIG_IPV6
		struct ipv6hdr *iph = (void *)(hdr + mac_hdr_len);

		iph->payload_len = htons(size + tcp_hdrlen(skb));
#else /* CONFIG_IPV6 */
		/* tso->ipv6 should never be set if IPV6 is not enabeld */
		WARN_ON(1);
#endif /* CONFIG_IPV6 */
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
		tso->data = page_address(skb_frag_page(frag)) + frag->page_offset;
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
		tso->data = page_address(skb_frag_page(frag)) + frag->page_offset;
		tso->next_frag_idx++;
	}
}
EXPORT_SYMBOL(tso_start);
