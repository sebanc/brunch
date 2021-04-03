#ifndef __IWL_CHROME
#define __IWL_CHROME
/* This file is pre-included from the Makefile (cc command line)
 *
 * ChromeOS backport definitions
 * Copyright (C) 2016-2017 Intel Deutschland GmbH
 * Copyright (C) 2018-2020 Intel Corporation
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/vmalloc.h>

/* get the CPTCFG_* preprocessor symbols */
#include <hdrs/config.h>

#include <hdrs/mac80211-exp.h>

#define LINUX_VERSION_IS_LESS(x1,x2,x3) (LINUX_VERSION_CODE < KERNEL_VERSION(x1,x2,x3))
#define LINUX_VERSION_IS_GEQ(x1,x2,x3)  (LINUX_VERSION_CODE >= KERNEL_VERSION(x1,x2,x3))
#define LINUX_VERSION_IN_RANGE(x1,x2,x3, y1,y2,y3) \
        (LINUX_VERSION_IS_GEQ(x1,x2,x3) && LINUX_VERSION_IS_LESS(y1,y2,y3))
#define LINUX_BACKPORT(sym) backport_ ## sym

/* this must be before including rhashtable.h */
#if LINUX_VERSION_IS_LESS(4,15,0)
#ifndef CONFIG_LOCKDEP
struct lockdep_map { };
#endif /* CONFIG_LOCKDEP */
#endif /* LINUX_VERSION_IS_LESS(4,15,0) */

/* include rhashtable this way to get our copy if another exists */
#include <linux/list_nulls.h>
#include "linux/rhashtable.h"

#include <net/genetlink.h>
#include <linux/crypto.h>
#include <linux/moduleparam.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <crypto/algapi.h>
#include <linux/pci.h>
#include <linux/if_vlan.h>
#include <linux/overflow.h>
#include "net/fq.h"

#if LINUX_VERSION_IS_LESS(3,20,0)
#define get_net_ns_by_fd LINUX_BACKPORT(get_net_ns_by_fd)
static inline struct net *get_net_ns_by_fd(int fd)
{
	return ERR_PTR(-EINVAL);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
static inline u64 ktime_get_ns(void)
{
	return ktime_to_ns(ktime_get());
}

static inline u64 ktime_get_real_ns(void)
{
	return ktime_to_ns(ktime_get_real());
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0) */

/*
 * Need to include these here, otherwise we get the regular kernel ones
 * pre-including them makes it work, even though later the kernel ones
 * are included again, but they (hopefully) have the same include guard
 * ifdef/define so the second time around nothing happens
 *
 * We still keep them in the correct directory so if they don't exist in
 * the kernel (e.g. bitfield.h won't) the preprocessor can find them.
 */
#include <hdrs/linux/ieee80211.h>
#include <hdrs/linux/average.h>
#include <hdrs/linux/bitfield.h>
#include <hdrs/net/ieee80211_radiotap.h>
#define IEEE80211RADIOTAP_H 1 /* older kernels used this include protection */

/* mac80211 & backport - order matters, need this inbetween */
#include <hdrs/mac80211-bp.h>

#include <hdrs/net/codel.h>
#include <hdrs/net/mac80211.h>

/* artifacts of backports - never in upstream */
#define genl_info_snd_portid(__genl_info) (__genl_info->snd_portid)
#define NETLINK_CB_PORTID(__skb) NETLINK_CB(cb->skb).portid
#define netlink_notify_portid(__notify) __notify->portid
#define __genl_const const

static inline struct netlink_ext_ack *genl_info_extack(struct genl_info *info)
{
#if LINUX_VERSION_IS_GEQ(4,12,0)
	return info->extack;
#else
	return NULL;
#endif
}

#if LINUX_VERSION_IS_LESS(5,3,0)
#define ktime_get_boottime_ns ktime_get_boot_ns
#endif

#if LINUX_VERSION_IS_GEQ(5,3,0)
/*
 * In v5.3, this function was renamed, so rename it here for v5.3+.
 * When we merge v5.3 back from upstream, the opposite should be done
 * (i.e. we will have _boottime_ and need to rename to _boot_ in <
 * v5.3 instead).
*/
#define ktime_get_boot_ns ktime_get_boottime_ns
#endif /* > 5.3.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
#define kvfree __iwl7000_kvfree
static inline void kvfree(const void *addr)
{
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}

static inline u64 ktime_get_boot_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}

/* interface name assignment types (sysfs name_assign_type attribute) */
#define NET_NAME_UNKNOWN	0	/* unknown origin (not exposed to userspace) */
#define NET_NAME_ENUM		1	/* enumerated by kernel */
#define NET_NAME_PREDICTABLE	2	/* predictably named by the kernel */
#define NET_NAME_USER		3	/* provided by user-space */
#define NET_NAME_RENAMED	4	/* renamed by user-space */

static inline struct net_device *
backport_alloc_netdev_mqs(int sizeof_priv, const char *name,
			  unsigned char name_assign_type,
			  void (*setup)(struct net_device *),
			  unsigned int txqs, unsigned int rxqs)
{
	return alloc_netdev_mqs(sizeof_priv, name, setup, txqs, rxqs);
}

#define alloc_netdev_mqs backport_alloc_netdev_mqs

#undef alloc_netdev
static inline struct net_device *
backport_alloc_netdev(int sizeof_priv, const char *name,
		      unsigned char name_assign_type,
		      void (*setup)(struct net_device *))
{
	return backport_alloc_netdev_mqs(sizeof_priv, name, name_assign_type,
					 setup, 1, 1);
}
#define alloc_netdev backport_alloc_netdev

char *devm_kvasprintf(struct device *dev, gfp_t gfp, const char *fmt,
		      va_list ap);
char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
#include <crypto/scatterwalk.h>
#include <crypto/aead.h>

static inline struct scatterlist *scatterwalk_ffwd(struct scatterlist dst[2],
					    struct scatterlist *src,
					    unsigned int len)
{
	for (;;) {
		if (!len)
			return src;

		if (src->length > len)
			break;

		len -= src->length;
		src = sg_next(src);
	}

	sg_init_table(dst, 2);
	sg_set_page(dst, sg_page(src), src->length - len, src->offset + len);
	scatterwalk_crypto_chain(dst, sg_next(src), 0, 2);

	return dst;
}



struct aead_old_request {
	struct scatterlist srcbuf[2];
	struct scatterlist dstbuf[2];
	struct aead_request subreq;
};

static inline unsigned int iwl7000_crypto_aead_reqsize(struct crypto_aead *tfm)
{
	return crypto_aead_crt(tfm)->reqsize + sizeof(struct aead_old_request);
}
#define crypto_aead_reqsize iwl7000_crypto_aead_reqsize

static inline struct aead_request *
crypto_backport_convert(struct aead_request *req)
{
	struct aead_old_request *nreq = aead_request_ctx(req);
	struct crypto_aead *aead = crypto_aead_reqtfm(req);
	struct scatterlist *src, *dst;

	src = scatterwalk_ffwd(nreq->srcbuf, req->src, req->assoclen);
	dst = req->src == req->dst ?
	      src : scatterwalk_ffwd(nreq->dstbuf, req->dst, req->assoclen);

	aead_request_set_tfm(&nreq->subreq, aead);
	aead_request_set_callback(&nreq->subreq, aead_request_flags(req),
				  req->base.complete, req->base.data);
	aead_request_set_crypt(&nreq->subreq, src, dst, req->cryptlen,
			       req->iv);
	aead_request_set_assoc(&nreq->subreq, req->src, req->assoclen);

	return &nreq->subreq;
}

static inline int iwl7000_crypto_aead_encrypt(struct aead_request *req)
{
	return crypto_aead_encrypt(crypto_backport_convert(req));
}
#define crypto_aead_encrypt iwl7000_crypto_aead_encrypt

static inline int iwl7000_crypto_aead_decrypt(struct aead_request *req)
{
	return crypto_aead_decrypt(crypto_backport_convert(req));
}
#define crypto_aead_decrypt iwl7000_crypto_aead_decrypt

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0) */

/* Note: this stuff is included in in chromeos-3.14 and 3.18.
 * Additionally, we check for <4.2, since that's when it was
 * added upstream.
 */
#if (LINUX_VERSION_CODE != KERNEL_VERSION(3,14,0)) &&	\
    (LINUX_VERSION_CODE != KERNEL_VERSION(3,18,0)) &&	\
    (LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0))
static inline void aead_request_set_ad(struct aead_request *req,
				       unsigned int assoclen)
{
	req->assoclen = assoclen;
}

static inline void kernel_param_lock(struct module *mod)
{
	__kernel_param_lock();
}

static inline void kernel_param_unlock(struct module *mod)
{
	__kernel_param_unlock();
}
#endif /* !3.14 && <4.2 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
#ifdef CONFIG_DEBUG_FS
struct dentry *iwl_debugfs_create_bool(const char *name, umode_t mode,
				       struct dentry *parent, bool *value);
#else
static inline struct dentry *
iwl_debugfs_create_bool(const char *name, umode_t mode,
			struct dentry *parent, bool *value)
{
	return ERR_PTR(-ENODEV);
}
#endif /* CONFIG_DEBUG_FS */
#define debugfs_create_bool iwl_debugfs_create_bool

#define tso_t __iwl7000_tso_t
struct tso_t {
	int next_frag_idx;
	void *data;
	size_t size;
	u16 ip_id;
	bool ipv6;
	u32 tcp_seq;
};

int tso_count_descs(struct sk_buff *skb);
void tso_build_hdr(struct sk_buff *skb, char *hdr, struct tso_t *tso,
		   int size, bool is_last);
void tso_start(struct sk_buff *skb, struct tso_t *tso);
void tso_build_data(struct sk_buff *skb, struct tso_t *tso, int size);

#endif /* < 4.4.0 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
static inline int
skb_ensure_writable(struct sk_buff *skb, int write_len)
{
	if (!pskb_may_pull(skb, write_len))
		return -ENOMEM;

	if (!skb_cloned(skb) || skb_clone_writable(skb, write_len))
		return 0;

	return pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
}
#endif

#ifndef NETIF_F_CSUM_MASK
#define NETIF_F_CSUM_MASK (NETIF_F_V4_CSUM | NETIF_F_V6_CSUM)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
static inline int
pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
		      int minvec, int maxvec)
{
	return -EOPNOTSUPP;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
void netdev_rss_key_fill(void *buffer, size_t len);
#endif

#if CFG80211_VERSION < KERNEL_VERSION(4, 1, 0) &&	\
	CFG80211_VERSION >= KERNEL_VERSION(3, 14, 0)
static inline struct sk_buff *
iwl7000_cfg80211_vendor_event_alloc(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    int approxlen, int event_idx, gfp_t gfp)
{
	return cfg80211_vendor_event_alloc(wiphy, approxlen, event_idx, gfp);
}

#define cfg80211_vendor_event_alloc iwl7000_cfg80211_vendor_event_alloc
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
int __must_check kstrtobool(const char *s, bool *res);
int __must_check kstrtobool_from_user(const char __user *s, size_t count, bool *res);
#endif /* < 4.6 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,5,0) ||	\
     LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0))
/* We don't really care much about alignment, since nl80211 isn't using
 * this for hot paths. So just implement it using nla_put_u64().
 */
static inline int nla_put_u64_64bit(struct sk_buff *skb, int attrtype,
				    u64 value, int padattr)
{
	return nla_put_u64(skb, attrtype, value);
}
#endif /* < 4.4 && > 4.5 */

#define nla_put_s64 iwl7000_nla_put_s64
static inline int nla_put_s64(struct sk_buff *skb, int attrtype, s64 value,
			      int padattr)
{
	return nla_put_u64(skb, attrtype, value);
}
void dev_coredumpsg(struct device *dev, struct scatterlist *table,
		    size_t datalen, gfp_t gfp);
#endif /* < 4.7 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
static inline bool backport_napi_complete_done(struct napi_struct *n, int work_done)
{
	if (unlikely(test_bit(NAPI_STATE_NPSVC, &n->state)))
		return false;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
	napi_complete(n);
#else
	napi_complete_done(n, work_done);
#endif /* < 3.19 */
	return true;
}

static inline bool backport_napi_complete(struct napi_struct *n)
{
	return backport_napi_complete_done(n, 0);
}
#define napi_complete_done LINUX_BACKPORT(napi_complete_done)
#define napi_complete LINUX_BACKPORT(napi_complete)

/* on earlier kernels, genl_unregister_family() modifies the struct */
#define __genl_ro_after_init
#else
#define __genl_ro_after_init __ro_after_init
#endif

#ifndef __BUILD_BUG_ON_NOT_POWER_OF_2
#define __BUILD_BUG_ON_NOT_POWER_OF_2(...)
#endif

#define ATTRIBUTE_GROUPS_BACKPORT(_name) \
static struct BP_ATTR_GRP_STRUCT _name##_dev_attrs[ARRAY_SIZE(_name##_attrs)];\
static void init_##_name##_attrs(void)				\
{									\
	int i;								\
	for (i = 0; _name##_attrs[i]; i++)				\
		_name##_dev_attrs[i] =				\
			*container_of(_name##_attrs[i],		\
				      struct BP_ATTR_GRP_STRUCT,	\
				      attr);				\
}

#ifndef __ATTRIBUTE_GROUPS
#define __ATTRIBUTE_GROUPS(_name)				\
static const struct attribute_group *_name##_groups[] = {	\
	&_name##_group,						\
	NULL,							\
}
#endif /* __ATTRIBUTE_GROUPS */

#undef ATTRIBUTE_GROUPS
#define ATTRIBUTE_GROUPS(_name)					\
static const struct attribute_group _name##_group = {		\
	.attrs = _name##_attrs,					\
};								\
static inline void init_##_name##_attrs(void) {}		\
__ATTRIBUTE_GROUPS(_name)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
#define __print_array(array, count, el_size) ""
#endif

#if LINUX_VERSION_IS_LESS(4,10,0)
static inline void *nla_memdup(const struct nlattr *src, gfp_t gfp)
{
	return kmemdup(nla_data(src), nla_len(src), gfp);
}
#endif

#if LINUX_VERSION_IS_LESS(4,15,0)
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
#endif /* LINUX_VERSION_IS_LESS(4,15,0) */

#if LINUX_VERSION_IS_LESS(4,4,0)
#define genl_notify(_fam, _skb, _info, _group, _flags)			\
	genl_notify(_fam, _skb, genl_info_net(_info),			\
		    genl_info_snd_portid(_info),			\
		    _group, _info->nlhdr, _flags)
#endif /* < 4.4 */

#if LINUX_VERSION_IS_LESS(4,10,0)
/**
 * genl_family_attrbuf - return family's attrbuf
 * @family: the family
 *
 * Return the family's attrbuf, while validating that it's
 * actually valid to access it.
 *
 * You cannot use this function with a family that has parallel_ops
 * and you can only use it within (pre/post) doit/dumpit callbacks.
 */
#define genl_family_attrbuf LINUX_BACKPORT(genl_family_attrbuf)
static inline struct nlattr **genl_family_attrbuf(struct genl_family *family)
{
	WARN_ON(family->parallel_ops);

	return family->attrbuf;
}

#define __genl_ro_after_init
#else
#define __genl_ro_after_init __ro_after_init
#endif

#ifndef GENL_UNS_ADMIN_PERM
#define GENL_UNS_ADMIN_PERM GENL_ADMIN_PERM
#endif

#if LINUX_VERSION_IS_LESS(4, 1, 0)
#define dev_of_node LINUX_BACKPORT(dev_of_node)
static inline struct device_node *dev_of_node(struct device *dev)
{
#ifndef CONFIG_OF
	return NULL;
#else
	return dev->of_node;
#endif
}
#endif /* LINUX_VERSION_IS_LESS(4, 1, 0) */

#if LINUX_VERSION_IS_LESS(4,12,0)
#include "magic.h"

static inline int nla_validate5(const struct nlattr *head,
				int len, int maxtype,
				const struct nla_policy *policy,
				struct netlink_ext_ack *extack)
{
	return nla_validate(head, len, maxtype, policy);
}
#define nla_validate4 nla_validate
#define nla_validate(...) \
	macro_dispatcher(nla_validate, __VA_ARGS__)(__VA_ARGS__)

static inline int nla_parse6(struct nlattr **tb, int maxtype,
			     const struct nlattr *head,
			     int len, const struct nla_policy *policy,
			     struct netlink_ext_ack *extack)
{
	return nla_parse(tb, maxtype, head, len, policy);
}
#define nla_parse5(...) nla_parse(__VA_ARGS__)
#define nla_parse(...) \
	macro_dispatcher(nla_parse, __VA_ARGS__)(__VA_ARGS__)

static inline int nlmsg_parse6(const struct nlmsghdr *nlh, int hdrlen,
			       struct nlattr *tb[], int maxtype,
			       const struct nla_policy *policy,
			       struct netlink_ext_ack *extack)
{
	return nlmsg_parse(nlh, hdrlen, tb, maxtype, policy);
}
#define nlmsg_parse5 nlmsg_parse
#define nlmsg_parse(...) \
	macro_dispatcher(nlmsg_parse, __VA_ARGS__)(__VA_ARGS__)

static inline int nlmsg_validate5(const struct nlmsghdr *nlh,
				  int hdrlen, int maxtype,
				  const struct nla_policy *policy,
				  struct netlink_ext_ack *extack)
{
	return nlmsg_validate(nlh, hdrlen, maxtype, policy);
}
#define nlmsg_validate4 nlmsg_validate
#define nlmsg_validate(...) \
	macro_dispatcher(nlmsg_validate, __VA_ARGS__)(__VA_ARGS__)

static inline int nla_parse_nested5(struct nlattr *tb[], int maxtype,
				    const struct nlattr *nla,
				    const struct nla_policy *policy,
				    struct netlink_ext_ack *extack)
{
	return nla_parse_nested(tb, maxtype, nla, policy);
}
#define nla_parse_nested4 nla_parse_nested
#define nla_parse_nested(...) \
	macro_dispatcher(nla_parse_nested, __VA_ARGS__)(__VA_ARGS__)

static inline int nla_validate_nested4(const struct nlattr *start, int maxtype,
				       const struct nla_policy *policy,
				       struct netlink_ext_ack *extack)
{
	return nla_validate_nested(start, maxtype, policy);
}
#define nla_validate_nested3 nla_validate_nested
#define nla_validate_nested(...) \
	macro_dispatcher(nla_validate_nested, __VA_ARGS__)(__VA_ARGS__)

#define kvmalloc LINUX_BACKPORT(kvmalloc)
static inline void *kvmalloc(size_t size, gfp_t flags)
{
	gfp_t kmalloc_flags = flags;
	void *ret;

	if ((flags & GFP_KERNEL) != GFP_KERNEL)
		return kmalloc(size, flags);

	if (size > PAGE_SIZE)
		kmalloc_flags |= __GFP_NOWARN | __GFP_NORETRY;

	ret = kmalloc(size, flags);
	if (ret || size < PAGE_SIZE)
		return ret;

	return vmalloc(size);
}

#define kvmalloc_array LINUX_BACKPORT(kvmalloc_array)
static inline void *kvmalloc_array(size_t n, size_t size, gfp_t flags)
{
	size_t bytes;

	if (unlikely(check_mul_overflow(n, size, &bytes)))
		return NULL;

	return kvmalloc(bytes, flags);
}

#define kvzalloc LINUX_BACKPORT(kvzalloc)
static inline void *kvzalloc(size_t size, gfp_t flags)
{
	return kvmalloc(size, flags | __GFP_ZERO);
}

#endif /* LINUX_VERSION_IS_LESS(4,12,0) */

#if LINUX_VERSION_IS_LESS(4,14,0)
static inline void *kvcalloc(size_t n, size_t size, gfp_t flags)
{
	return kvmalloc_array(n, size, flags | __GFP_ZERO);
}
#endif /* LINUX_VERSION_IS_LESS(4,14,0) */

/* avoid conflicts with other headers */
#ifdef is_signed_type
#undef is_signed_type
#endif

#ifndef offsetofend
/**
 * offsetofend(TYPE, MEMBER)
 *
 * @TYPE: The type of the structure
 * @MEMBER: The member within the structure to get the end offset of
 */
#define offsetofend(TYPE, MEMBER) \
	(offsetof(TYPE, MEMBER)	+ sizeof(((TYPE *)0)->MEMBER))
#endif


int __alloc_bucket_spinlocks(spinlock_t **locks, unsigned int *lock_mask,
			     size_t max_size, unsigned int cpu_mult,
			     gfp_t gfp, const char *name,
			     struct lock_class_key *key);

#define alloc_bucket_spinlocks(locks, lock_mask, max_size, cpu_mult, gfp)    \
	({								     \
		static struct lock_class_key key;			     \
		int ret;						     \
									     \
		ret = __alloc_bucket_spinlocks(locks, lock_mask, max_size,   \
					       cpu_mult, gfp, #locks, &key); \
		ret;							\
	})
void free_bucket_spinlocks(spinlock_t *locks);

#if LINUX_VERSION_IS_LESS(4,12,0)
#define GENL_SET_ERR_MSG(info, msg) do { } while (0)

static inline int genl_err_attr(struct genl_info *info, int err,
				struct nlattr *attr)
{
#if LINUX_VERSION_IS_GEQ(4,12,0)
	info->extack->bad_attr = attr;
#endif

	return err;
}
#endif

#if LINUX_VERSION_IS_LESS(4,19,0)
#ifndef atomic_fetch_add_unless
static inline int atomic_fetch_add_unless(atomic_t *v, int a, int u)
{
		return __atomic_add_unless(v, a, u);
}
#endif
#endif /* LINUX_VERSION_IS_LESS(4,19,0) */

#if LINUX_VERSION_IS_LESS(4,20,0)
static inline void rcu_head_init(struct rcu_head *rhp)
{
	rhp->func = (void *)~0L;
}

static inline bool
rcu_head_after_call_rcu(struct rcu_head *rhp, void *f)
{
	if (READ_ONCE(rhp->func) == f)
		return true;
	WARN_ON_ONCE(READ_ONCE(rhp->func) != (void *)~0L);
	return false;
}
#endif /* LINUX_VERSION_IS_LESS(4,20,0) */

#if LINUX_VERSION_IS_LESS(4,14,0)
#define skb_mark_not_on_list iwl7000_skb_mark_not_on_list
static inline void skb_mark_not_on_list(struct sk_buff *skb)
{
	skb->next = NULL;
}
#endif /* LINUX_VERSION_IS_LESS(4,14,0) */

#if LINUX_VERSION_IS_LESS(5,4,0)
#include <linux/pci-aspm.h>
#endif

#if LINUX_VERSION_IS_LESS(5,5,0)
#include <linux/debugfs.h>

#define debugfs_create_xul iwl7000_debugfs_create_xul
static inline void debugfs_create_xul(const char *name, umode_t mode,
				      struct dentry *parent,
				      unsigned long *value)
{
	if (sizeof(*value) == sizeof(u32))
		debugfs_create_x32(name, mode, parent, (u32 *)value);
	else
		debugfs_create_x64(name, mode, parent, (u64 *)value);
}
#endif

#ifndef skb_list_walk_safe
#define skb_list_walk_safe(first, skb, next_skb)				\
	for ((skb) = (first), (next_skb) = (skb) ? (skb)->next : NULL; (skb);	\
	     (skb) = (next_skb), (next_skb) = (skb) ? (skb)->next : NULL)
#endif

#if LINUX_VERSION_IS_LESS(4,13,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
#include <linux/acpi.h>
#include <linux/uuid.h>

#define guid_t uuid_le
#define uuid_t uuid_be

static inline void guid_gen(guid_t *u)
{
	return uuid_le_gen(u);
}

static inline void uuid_gen(uuid_t *u)
{
	return uuid_be_gen(u);
}

static inline void guid_copy(guid_t *dst, const guid_t *src)
{
	memcpy(dst, src, sizeof(guid_t));
}

#define GUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)	\
	UUID_LE(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)

static inline union acpi_object *
LINUX_BACKPORT(acpi_evaluate_dsm)(acpi_handle handle, const guid_t *guid,
				  u64 rev, u64 func, union acpi_object *args)
{
	return acpi_evaluate_dsm(handle, guid->b, rev, func, args);
}

#define acpi_evaluate_dsm LINUX_BACKPORT(acpi_evaluate_dsm)
#endif

#if LINUX_VERSION_IS_LESS(4,18,0)
#define firmware_request_nowarn(fw, name, device) request_firmware(fw, name, device)
#endif

#endif /* __IWL_CHROME */

#if LINUX_VERSION_IS_LESS(5,4,0)

/**
 * list_for_each_entry_rcu	-	iterate over rcu list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 * @cond...:	optional lockdep expression if called from non-RCU protection.
 *
 * This list-traversal primitive may safely run concurrently with
 * the _rcu list-mutation primitives such as list_add_rcu()
 * as long as the traversal is guarded by rcu_read_lock().
 */
#undef list_for_each_entry_rcu
#define list_for_each_entry_rcu(pos, head, member, cond...)		\
	for (pos = list_entry_rcu((head)->next, typeof(*pos), member); \
		&pos->member != (head); \
		pos = list_entry_rcu(pos->member.next, typeof(*pos), member))
#endif /* < 5.4 */
