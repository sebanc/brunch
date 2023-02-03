#ifndef __BACKPORT_NET_GENETLINK_H
#define __BACKPORT_NET_GENETLINK_H
#include_next <net/genetlink.h>
#include <linux/version.h>

static inline void __bp_genl_info_userhdr_set(struct genl_info *info,
					      void *userhdr)
{
	info->userhdr = userhdr;
}

static inline void *__bp_genl_info_userhdr(struct genl_info *info)
{
	return info->userhdr;
}

#if LINUX_VERSION_IS_LESS(4,12,0)
#define GENL_SET_ERR_MSG(info, msg) NL_SET_ERR_MSG(genl_info_extack(info), msg)

static inline int genl_err_attr(struct genl_info *info, int err,
				struct nlattr *attr)
{
	return err;
}
#endif /* < 4.12 */

/* this is for patches we apply */
static inline struct netlink_ext_ack *genl_info_extack(struct genl_info *info)
{
#if LINUX_VERSION_IS_GEQ(4,12,0)
	return info->extack;
#else
	return info->userhdr;
#endif
}

/* this is for patches we apply */
static inline struct netlink_ext_ack *genl_callback_extack(struct netlink_callback *cb)
{
#if LINUX_VERSION_IS_GEQ(4,20,0)
	return cb->extack;
#else
	return NULL;
#endif
}

/* this gets put in place of info->userhdr, since we use that above */
static inline void *genl_info_userhdr(struct genl_info *info)
{
	return (u8 *)info->genlhdr + GENL_HDRLEN;
}

#if LINUX_VERSION_IS_LESS(4,10,0)
#define __genl_ro_after_init
#else
#define __genl_ro_after_init __ro_after_init
#endif

#if LINUX_VERSION_IS_LESS(4,15,0)
#define genlmsg_nlhdr LINUX_BACKPORT(genlmsg_nlhdr)
static inline struct nlmsghdr *genlmsg_nlhdr(void *user_hdr)
{
	return (struct nlmsghdr *)((char *)user_hdr -
				   GENL_HDRLEN -
				   NLMSG_HDRLEN);
}

#ifndef genl_dump_check_consistent
static inline
void backport_genl_dump_check_consistent(struct netlink_callback *cb,
					 void *user_hdr)
{
	struct genl_family dummy_family = {
		.hdrsize = 0,
	};

	genl_dump_check_consistent(cb, user_hdr, &dummy_family);
}
#define genl_dump_check_consistent LINUX_BACKPORT(genl_dump_check_consistent)
#endif
#endif /* LINUX_VERSION_IS_LESS(4,15,0) */

#if LINUX_VERSION_IS_LESS(5,2,0)
enum genl_validate_flags {
	GENL_DONT_VALIDATE_STRICT		= BIT(0),
	GENL_DONT_VALIDATE_DUMP			= BIT(1),
	GENL_DONT_VALIDATE_DUMP_STRICT		= BIT(2),
};

struct backport_genl_ops {
	void			*__dummy_was_policy_must_be_null;
	int		       (*doit)(struct sk_buff *skb,
				       struct genl_info *info);
#if LINUX_VERSION_IS_GEQ(4,5,0) || \
    LINUX_VERSION_IN_RANGE(4,4,104, 4,5,0)
	int		       (*start)(struct netlink_callback *cb);
#endif
	int		       (*dumpit)(struct sk_buff *skb,
					 struct netlink_callback *cb);
	int		       (*done)(struct netlink_callback *cb);
	u8			cmd;
	u8			internal_flags;
	u8			flags;
	u8			validate;
};

static inline int
__real_backport_genl_register_family(struct genl_family *family)
{
#define OPS_VALIDATE(f) \
	BUILD_BUG_ON(offsetof(struct genl_ops, f) != \
		     offsetof(struct backport_genl_ops, f))
	OPS_VALIDATE(doit);
#if LINUX_VERSION_IS_GEQ(4,5,0) || \
    LINUX_VERSION_IN_RANGE(4,4,104, 4,5,0)
	OPS_VALIDATE(start);
#endif
	OPS_VALIDATE(dumpit);
	OPS_VALIDATE(done);
	OPS_VALIDATE(cmd);
	OPS_VALIDATE(internal_flags);
	OPS_VALIDATE(flags);

	return genl_register_family(family);
}
#define genl_ops backport_genl_ops

static inline int
__real_backport_genl_unregister_family(struct genl_family *family)
{
	return genl_unregister_family(family);
}

struct backport_genl_family {
	struct genl_family	family;
	const struct genl_ops *	copy_ops;

	/* copied */
	int			id;		/* private */
	unsigned int		hdrsize;
	char			name[GENL_NAMSIZ];
	unsigned int		version;
	unsigned int		maxattr;
	bool			netnsok;
	bool			parallel_ops;
	const struct nla_policy *policy;
	int			(*pre_doit)(const struct genl_ops *ops,
					    struct sk_buff *skb,
					    struct genl_info *info);
	void			(*post_doit)(const struct genl_ops *ops,
					     struct sk_buff *skb,
					     struct genl_info *info);
/*
 * unsupported!
	int			(*mcast_bind)(struct net *net, int group);
	void			(*mcast_unbind)(struct net *net, int group);
 */
	struct nlattr **	attrbuf;	/* private */
	const struct genl_ops *	ops;
	const struct genl_multicast_group *mcgrps;
	unsigned int		n_ops;
	unsigned int		n_mcgrps;
	struct module		*module;
};
#undef genl_family
#define genl_family backport_genl_family

#define genl_register_family backport_genl_register_family
int genl_register_family(struct genl_family *family);

#define genl_unregister_family backport_genl_unregister_family
int backport_genl_unregister_family(struct genl_family *family);

#define genl_notify LINUX_BACKPORT(genl_notify)
void genl_notify(const struct genl_family *family, struct sk_buff *skb,
		 struct genl_info *info, u32 group, gfp_t flags);

#define genlmsg_put LINUX_BACKPORT(genlmsg_put)
void *genlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
		  const struct genl_family *family, int flags, u8 cmd);

#define genlmsg_put_reply LINUX_BACKPORT(genlmsg_put_reply)
void *genlmsg_put_reply(struct sk_buff *skb,
			struct genl_info *info,
			const struct genl_family *family,
			int flags, u8 cmd);

#define genlmsg_multicast_netns LINUX_BACKPORT(genlmsg_multicast_netns)
int genlmsg_multicast_netns(const struct genl_family *family,
			    struct net *net, struct sk_buff *skb,
			    u32 portid, unsigned int group,
			    gfp_t flags);

#define genlmsg_multicast LINUX_BACKPORT(genlmsg_multicast)
int genlmsg_multicast(const struct genl_family *family,
		      struct sk_buff *skb, u32 portid,
		      unsigned int group, gfp_t flags);

#define genlmsg_multicast_allns LINUX_BACKPORT(genlmsg_multicast_allns)
int backport_genlmsg_multicast_allns(const struct genl_family *family,
				     struct sk_buff *skb, u32 portid,
				     unsigned int group, gfp_t flags);

#define genl_family_attrbuf LINUX_BACKPORT(genl_family_attrbuf)
static inline struct nlattr **genl_family_attrbuf(struct genl_family *family)
{
	WARN_ON(family->parallel_ops);

	return family->attrbuf;
}

#define genlmsg_parse LINUX_BACKPORT(genlmsg_parse)
static inline int genlmsg_parse(const struct nlmsghdr *nlh,
				const struct genl_family *family,
				struct nlattr *tb[], int maxtype,
				const struct nla_policy *policy,
				struct netlink_ext_ack *extack)
{
	return __nlmsg_parse(nlh, family->hdrsize + GENL_HDRLEN, tb, maxtype,
			     policy, NL_VALIDATE_STRICT, extack);
}
#endif /* LINUX_VERSION_IS_LESS(5,2,0) */

#endif /* __BACKPORT_NET_GENETLINK_H */
