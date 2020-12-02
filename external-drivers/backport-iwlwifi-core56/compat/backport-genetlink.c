/*
 * Copyright 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 Intel Corporation
 *
 * Backport functionality introduced in Linux 4.20.
 * Much of this is based on upstream lib/nlattr.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <net/genetlink.h>
#include <net/netlink.h>
#include <net/sock.h>

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static const struct genl_family *find_family_real_ops(__genl_const struct genl_ops **ops)
{
	const struct genl_family *family;
	__genl_const struct genl_ops *tmp_ops = *ops;

	/* find the family ... */
	while (tmp_ops->doit || tmp_ops->dumpit)
		tmp_ops++;
	family = (void *)tmp_ops->done;

	/* cast to suppress const warning */
	*ops = (void *)(family->ops + (*ops - family->copy_ops));

	return family;
}

#if LINUX_VERSION_IS_LESS(4,12,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
enum nlmsgerr_attrs {
	NLMSGERR_ATTR_UNUSED,
	NLMSGERR_ATTR_MSG,
	NLMSGERR_ATTR_OFFS,
	NLMSGERR_ATTR_COOKIE,
	__NLMSGERR_ATTR_MAX,
	NLMSGERR_ATTR_MAX = __NLMSGERR_ATTR_MAX - 1
};

#define NLM_F_CAPPED	0x100	/* request was capped */
#define NLM_F_ACK_TLVS	0x200	/* extended ACK TVLs were included */

static void extack_netlink_ack(struct sk_buff *in_skb, struct nlmsghdr *nlh,
			       int err, const struct netlink_ext_ack *extack)
{
	struct sk_buff *skb;
	struct nlmsghdr *rep;
	struct nlmsgerr *errmsg;
	size_t payload = sizeof(*errmsg);
	size_t tlvlen = 0;
	unsigned int flags = 0;
	/* backports assumes everyone supports this - libnl does so it's true */
	bool nlk_has_extack = true;

	/* Error messages get the original request appened, unless the user
	 * requests to cap the error message, and get extra error data if
	 * requested.
	 * (ignored in backports)
	 */
	if (nlk_has_extack && extack && extack->_msg)
		tlvlen += nla_total_size(strlen(extack->_msg) + 1);

	if (err) {
		if (1)
			payload += nlmsg_len(nlh);
		else
			flags |= NLM_F_CAPPED;
		if (nlk_has_extack && extack && extack->bad_attr)
			tlvlen += nla_total_size(sizeof(u32));
	} else {
		flags |= NLM_F_CAPPED;

		if (nlk_has_extack && extack && extack->cookie_len)
			tlvlen += nla_total_size(extack->cookie_len);
	}

	if (tlvlen)
		flags |= NLM_F_ACK_TLVS;

	skb = nlmsg_new(payload + tlvlen, GFP_KERNEL);
	if (!skb) {
		NETLINK_CB(in_skb).sk->sk_err = ENOBUFS;
		NETLINK_CB(in_skb).sk->sk_error_report(NETLINK_CB(in_skb).sk);
		return;
	}

	rep = __nlmsg_put(skb, NETLINK_CB_PORTID(in_skb), nlh->nlmsg_seq,
			  NLMSG_ERROR, payload, flags);
	errmsg = nlmsg_data(rep);
	errmsg->error = err;
	memcpy(&errmsg->msg, nlh, payload > sizeof(*errmsg) ? nlh->nlmsg_len : sizeof(*nlh));

	if (nlk_has_extack && extack) {
		if (extack->_msg) {
			WARN_ON(nla_put_string(skb, NLMSGERR_ATTR_MSG,
					       extack->_msg));
		}
		if (err) {
			if (extack->bad_attr &&
			    !WARN_ON((u8 *)extack->bad_attr < in_skb->data ||
				     (u8 *)extack->bad_attr >= in_skb->data +
							       in_skb->len))
				WARN_ON(nla_put_u32(skb, NLMSGERR_ATTR_OFFS,
						    (u8 *)extack->bad_attr -
						    in_skb->data));
		} else {
			if (extack->cookie_len)
				WARN_ON(nla_put(skb, NLMSGERR_ATTR_COOKIE,
						extack->cookie_len,
						extack->cookie));
		}
	}

	nlmsg_end(skb, rep);

	netlink_unicast(in_skb->sk, skb, NETLINK_CB_PORTID(in_skb),
			MSG_DONTWAIT);
}

static int extack_doit(struct sk_buff *skb, struct genl_info *info)
{
	int (*doit)(struct sk_buff *, struct genl_info *);
	int err;

	doit = genl_info_extack(info)->__bp_doit;

	/* signal from our pre_doit to not do anything */
	if (!doit)
		return 0;

	err = doit(skb, info);

	if (err == -EINTR)
		return err;

	if (info->nlhdr->nlmsg_flags & NLM_F_ACK || err)
		extack_netlink_ack(skb, info->nlhdr, err,
				   genl_info_extack(info));

	/* suppress sending ACK from normal netlink code */
	info->nlhdr->nlmsg_flags &= ~NLM_F_ACK;
	return 0;
}
#endif /* LINUX_VERSION_IS_LESS(4,12,0) */

static int backport_pre_doit(__genl_const struct genl_ops *ops,
			     struct sk_buff *skb,
			     struct genl_info *info)
{
	const struct genl_family *family = find_family_real_ops(&ops);
	int err;
#if LINUX_VERSION_IS_LESS(4,12,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
	struct netlink_ext_ack *extack = kzalloc(sizeof(*extack), GFP_KERNEL);

	if (!extack)
		return -ENOMEM;

	__bp_genl_info_userhdr_set(info, extack);

	extack->__bp_doit = ops->doit;
#else
	struct netlink_ext_ack *extack = genl_info_extack(info);
#endif

	if (ops->validate & GENL_DONT_VALIDATE_STRICT)
		err = nlmsg_validate_deprecated(info->nlhdr,
						GENL_HDRLEN + family->hdrsize,
						family->maxattr, family->policy,
						extack);
	else
		err = nlmsg_validate(info->nlhdr, GENL_HDRLEN + family->hdrsize,
				     family->maxattr, family->policy, extack);

	if (!err && family->pre_doit)
		err = family->pre_doit(ops, skb, info);

#if LINUX_VERSION_IS_LESS(4,12,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
	if (err) {
		/* signal to do nothing */
		extack->__bp_doit = NULL;

		extack_netlink_ack(skb, info->nlhdr, err, extack);

		/* suppress sending ACK from normal netlink code */
		info->nlhdr->nlmsg_flags &= ~NLM_F_ACK;

		/* extack will be freed in post_doit as usual */

		return 0;
	}
#endif

	return err;
}

static void backport_post_doit(__genl_const struct genl_ops *ops,
			       struct sk_buff *skb,
			       struct genl_info *info)
{
	const struct genl_family *family = find_family_real_ops(&ops);

#if LINUX_VERSION_IS_LESS(4,12,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
	if (genl_info_extack(info)->__bp_doit)
#else
	if (1)
#endif
		if (family->post_doit)
			family->post_doit(ops, skb, info);

#if LINUX_VERSION_IS_LESS(4,12,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
	kfree(__bp_genl_info_userhdr(info));
#endif
}

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
int backport_genl_register_family(struct genl_family *family)
{
	struct genl_ops *ops;
	int err, i;

#define COPY(memb)	family->family.memb = family->memb
#define BACK(memb)	family->memb = family->family.memb

	/* we append one entry to the ops to find our family pointer ... */
	ops = kzalloc(sizeof(*ops) * (family->n_ops + 1), GFP_KERNEL);
	if (!ops)
		return -ENOMEM;

	memcpy(ops, family->ops, sizeof(*ops) * family->n_ops);

	/*
	 * Remove policy to skip validation as the struct nla_policy
	 * memory layout isn't compatible with the old version
	 */
	for (i = 0; i < family->n_ops; i++) {
#if LINUX_VERSION_IS_LESS(4,12,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
		if (ops[i].doit)
			ops[i].doit = extack_doit;
#endif
/*
 * TODO: add dumpit redirect (like extack_doit) that will
 *       make this code honor !GENL_DONT_VALIDATE_DUMP and
 *       actually validate in this case ...
 */
	}
	/* keep doit/dumpit NULL - that's invalid */
	ops[family->n_ops].done = (void *)family;

	COPY(id);
	memcpy(family->family.name, family->name, sizeof(family->name));
	COPY(hdrsize);
	COPY(version);
	COPY(maxattr);
	COPY(netnsok);
#if LINUX_VERSION_IS_GEQ(3,10,0)
	COPY(parallel_ops);
#endif
	/* The casts are OK - we checked everything is the same offset in genl_ops */
	family->family.pre_doit = (void *)backport_pre_doit;
	family->family.post_doit = (void *)backport_post_doit;
	/* attrbuf is output only */
	family->copy_ops = (void *)ops;
#if LINUX_VERSION_IS_GEQ(3,13,0)
	family->family.ops = (void *)ops;
	COPY(mcgrps);
	COPY(n_ops);
	COPY(n_mcgrps);
#endif
#if LINUX_VERSION_IS_GEQ(3,11,0)
	COPY(module);
#endif

	err = __real_backport_genl_register_family(&family->family);

	BACK(id);
	BACK(attrbuf);

	if (err)
		return err;

#if LINUX_VERSION_IS_GEQ(3,13,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,0)
	return 0;
#else
	for (i = 0; i < family->n_ops; i++) {
		err = genl_register_ops(&family->family, ops + i);
		if (err < 0)
			goto error;
	}

	for (i = 0; i < family->n_mcgrps; i++) {
		err = genl_register_mc_group(&family->family,
					     &family->mcgrps[i]);
		if (err)
			goto error;
	}

	return 0;
 error:
	genl_unregister_family(family);
	return err;
#endif /* LINUX_VERSION_IS_GEQ(3,13,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,0) */
}
EXPORT_SYMBOL_GPL(backport_genl_register_family);

int backport_genl_unregister_family(struct genl_family *family)
{
	return __real_backport_genl_unregister_family(&family->family);
}
EXPORT_SYMBOL_GPL(backport_genl_unregister_family);
#endif

#define INVALID_GROUP	0xffffffff

static u32 __backport_genl_group(const struct genl_family *family,
				 u32 group)
{
	if (WARN_ON_ONCE(group >= family->n_mcgrps))
		return INVALID_GROUP;
#if LINUX_VERSION_IS_LESS(3,13,0)
	return family->mcgrps[group].id;
#else
	return family->family.mcgrp_offset + group;
#endif
}

void genl_notify(const struct genl_family *family, struct sk_buff *skb,
		 struct genl_info *info, u32 group, gfp_t flags)
{
	struct net *net = genl_info_net(info);
	struct sock *sk = net->genl_sock;
	int report = 0;

	if (info->nlhdr)
		report = nlmsg_report(info->nlhdr);

	group = __backport_genl_group(family, group);
	if (group == INVALID_GROUP)
		return;
	nlmsg_notify(sk, skb, genl_info_snd_portid(info), group, report,
		     flags);
}
EXPORT_SYMBOL_GPL(genl_notify);

void *genlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
		  const struct genl_family *family, int flags, u8 cmd)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *hdr;

	nlh = nlmsg_put(skb, portid, seq, family->id, GENL_HDRLEN +
			family->hdrsize, flags);
	if (nlh == NULL)
		return NULL;

	hdr = nlmsg_data(nlh);
	hdr->cmd = cmd;
	hdr->version = family->version;
	hdr->reserved = 0;

	return (char *) hdr + GENL_HDRLEN;
}
EXPORT_SYMBOL_GPL(genlmsg_put);

void *genlmsg_put_reply(struct sk_buff *skb,
			struct genl_info *info,
			const struct genl_family *family,
			int flags, u8 cmd)
{
	return genlmsg_put(skb, genl_info_snd_portid(info), info->snd_seq,
			   family,
			   flags, cmd);
}
EXPORT_SYMBOL_GPL(genlmsg_put_reply);

int genlmsg_multicast_netns(const struct genl_family *family,
			    struct net *net, struct sk_buff *skb,
			    u32 portid, unsigned int group,
			    gfp_t flags)
{
	group = __backport_genl_group(family, group);
	if (group == INVALID_GROUP)
		return -EINVAL;
	return nlmsg_multicast(net->genl_sock, skb, portid, group, flags);
}
EXPORT_SYMBOL_GPL(genlmsg_multicast_netns);

int genlmsg_multicast(const struct genl_family *family,
		      struct sk_buff *skb, u32 portid,
		      unsigned int group, gfp_t flags)
{
	return genlmsg_multicast_netns(family, &init_net, skb,
				       portid, group, flags);
}
EXPORT_SYMBOL_GPL(genlmsg_multicast);

static int genlmsg_mcast(struct sk_buff *skb, u32 portid, unsigned long group,
			 gfp_t flags)
{
	struct sk_buff *tmp;
	struct net *net, *prev = NULL;
	bool delivered = false;
	int err;

	for_each_net_rcu(net) {
		if (prev) {
			tmp = skb_clone(skb, flags);
			if (!tmp) {
				err = -ENOMEM;
				goto error;
			}
			err = nlmsg_multicast(prev->genl_sock, tmp,
					      portid, group, flags);
			if (!err)
				delivered = true;
			else if (err != -ESRCH)
				goto error;
		}

		prev = net;
	}

	err = nlmsg_multicast(prev->genl_sock, skb, portid, group, flags);
	if (!err)
		delivered = true;
	else if (err != -ESRCH)
		return err;
	return delivered ? 0 : -ESRCH;
 error:
	kfree_skb(skb);
	return err;
}

int backport_genlmsg_multicast_allns(const struct genl_family *family,
				     struct sk_buff *skb, u32 portid,
				     unsigned int group, gfp_t flags)
{
	group = __backport_genl_group(family, group);
	if (group == INVALID_GROUP)
		return -EINVAL;
	return genlmsg_mcast(skb, portid, group, flags);
}
EXPORT_SYMBOL_GPL(backport_genlmsg_multicast_allns);
#endif /* RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6) */
