#ifndef __BACKPORT_SKBUFF_H
#define __BACKPORT_SKBUFF_H
#include_next <linux/skbuff.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(4,4,10)
static __always_inline void
__skb_postpush_rcsum(struct sk_buff *skb, const void *start, unsigned int len,
		     unsigned int off)
{
	if (skb->ip_summed == CHECKSUM_COMPLETE)
		skb->csum = csum_block_add(skb->csum,
					   csum_partial(start, len, 0), off);
}

static inline void skb_postpush_rcsum(struct sk_buff *skb,
				      const void *start, unsigned int len)
{
	__skb_postpush_rcsum(skb, start, len, 0);
}
#endif /* LINUX_VERSION_IS_LESS(4,4,10) */

#if LINUX_VERSION_IS_LESS(4,13,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline void *backport_skb_put(struct sk_buff *skb, unsigned int len)
{
	return skb_put(skb, len);
}
#define skb_put LINUX_BACKPORT(skb_put)

static inline void *backport_skb_push(struct sk_buff *skb, unsigned int len)
{
	return skb_push(skb, len);
}
#define skb_push LINUX_BACKPORT(skb_push)

static inline void *backport___skb_push(struct sk_buff *skb, unsigned int len)
{
	return __skb_push(skb, len);
}
#define __skb_push LINUX_BACKPORT(__skb_push)

static inline void *__skb_put_zero(struct sk_buff *skb, unsigned int len)
{
	void *tmp = __skb_put(skb, len);

	memset(tmp, 0, len);
	return tmp;
}

static inline void *skb_put_zero(struct sk_buff *skb, unsigned int len)
{
	void *tmp = skb_put(skb, len);

	memset(tmp, 0, len);

	return tmp;
}

static inline void *skb_put_data(struct sk_buff *skb, const void *data,
				 unsigned int len)
{
	void *tmp = skb_put(skb, len);

	memcpy(tmp, data, len);

	return tmp;
}

static inline void skb_put_u8(struct sk_buff *skb, u8 val)
{
	*(u8 *)skb_put(skb, 1) = val;
}
#endif

#if LINUX_VERSION_IS_LESS(4,20,0)
static inline struct sk_buff *__skb_peek(const struct sk_buff_head *list_)
{
	return list_->next;
}

#if !LINUX_VERSION_IN_RANGE(4,19,10, 4,20,0) && \
    !LINUX_VERSION_IN_RANGE(4,14,217, 4,15,0)
static inline void skb_mark_not_on_list(struct sk_buff *skb)
{
	skb->next = NULL;
}
#endif /* 4.19.10 <= x < 4.20 || 4.14.217 <= x < 4.15 */

#if !LINUX_VERSION_IN_RANGE(4,19,10, 4,20,0)
static inline void skb_list_del_init(struct sk_buff *skb)
{
	__list_del_entry((struct list_head *)&skb->next);
	skb_mark_not_on_list(skb);
}
#endif /* 4.19.10 <= x < 4.20 */
#endif /* < 4.20 */

#if LINUX_VERSION_IS_LESS(4,11,0)
#define skb_mac_offset LINUX_BACKPORT(skb_mac_offset)
static inline int skb_mac_offset(const struct sk_buff *skb)
{
	return skb_mac_header(skb) - skb->data;
}
#endif

#if LINUX_VERSION_IS_LESS(5,4,0)
/**
 * skb_frag_off() - Returns the offset of a skb fragment
 * @frag: the paged fragment
 */
#define skb_frag_off LINUX_BACKPORT(skb_frag_off)
static inline unsigned int skb_frag_off(const skb_frag_t *frag)
{
	return frag->page_offset;
}

#define nf_reset_ct LINUX_BACKPORT(nf_reset_ct)
static inline void nf_reset_ct(struct sk_buff *skb)
{
	nf_reset(skb);
}
#endif

#ifndef skb_list_walk_safe
#define skb_list_walk_safe(first, skb, next_skb)				\
	for ((skb) = (first), (next_skb) = (skb) ? (skb)->next : NULL; (skb); 	\
	     (skb) = (next_skb), (next_skb) = (skb) ? (skb)->next : NULL)
#endif

#if LINUX_VERSION_IS_LESS(5,6,0) &&			\
	!LINUX_VERSION_IN_RANGE(5,4,69, 5,5,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,19,149, 4,20,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,14,200, 4,15,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,9,238, 4,10,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,4,238, 4,5,0)
/**
 *	skb_queue_len_lockless	- get queue length
 *	@list_: list to measure
 *
 *	Return the length of an &sk_buff queue.
 *	This variant can be used in lockless contexts.
 */
#define skb_queue_len_lockless LINUX_BACKPORT(skb_queue_len_lockless)
static inline __u32 skb_queue_len_lockless(const struct sk_buff_head *list_)
{
	return READ_ONCE(list_->qlen);
}
#endif /* < 5.6.0 */

#if LINUX_VERSION_IS_LESS(5,10,54)
#define skb_get_kcov_handle LINUX_BACKPORT(skb_get_kcov_handle)
static inline u64 skb_get_kcov_handle(struct sk_buff *skb)
{
	return 0;
}
#endif

#endif /* __BACKPORT_SKBUFF_H */
