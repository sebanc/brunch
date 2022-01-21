/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_SKB_UTIL_H__
#define __T7XX_SKB_UTIL_H__

#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/wwan.h>

#define MTK_SKB_64K	64528		/* 63kB + CCCI header */
#define MTK_SKB_4K	3584		/* 3.5kB */
#define MTK_SKB_2K  	2048
#define MTK_SKB_1_5K	(1500 + 16)	/* net MTU + CCCI_H, for network packet */
#define MTK_SKB_16	16		/* for struct ccci_header */
#define NET_RX_BUF	MTK_SKB_4K

#define CCCI_BUF_MAGIC	0xFECDBA89

#define MTK_SKB_WAIT_FOR_POOLS_RELOAD_MS	20

struct ccci_skb_queue {
	struct sk_buff_head	skb_list;
	unsigned int		max_len;
	struct work_struct	reload_work;
	bool			pre_filled;
	unsigned int		max_history;
	unsigned int		max_occupied;
	unsigned int		enq_count;
	unsigned int		deq_count;
};

struct skb_pools {
	struct ccci_skb_queue skb_pool_64k;
	struct ccci_skb_queue skb_pool_4k;
	struct ccci_skb_queue skb_pool_16;
	struct workqueue_struct *reload_work_queue;
};

/**
 * enum data_policy - tells the request free routine how to handle the skb
 * @FREE: free the skb
 * @RECYCLE: put the skb back into our pool
 *
 * The driver request structure will always be recycled, but its skb
 * can have a different policy. The driver request can work as a wrapper
 * because the network subsys will handle the skb itself.
 * TX: policy is determined by sender
 * RX: policy is determined by receiver
 */
enum data_policy {
	MTK_SKB_POLICY_FREE = 0,
	MTK_SKB_POLICY_RECYCLE,
};

/* Get free skb flags */
#define	GFS_NONE_BLOCKING	0
#define	GFS_BLOCKING		1

/* CCCI buffer control structure. Must be less than NET_SKB_PAD */
struct ccci_buffer_ctrl {
	unsigned int		head_magic;
	enum data_policy	policy;
	unsigned char		ioc_override;
	unsigned char		blocking;
};

#ifdef NET_SKBUFF_DATA_USES_OFFSET
static inline unsigned int skb_size(struct sk_buff *skb)
{
	return skb->end;
}

static inline unsigned int skb_data_size(struct sk_buff *skb)
{
	return skb->head + skb->end - skb->data;
}
#else
static inline unsigned int skb_size(struct sk_buff *skb)
{
	return skb->end - skb->head;
}

static inline unsigned int skb_data_size(struct sk_buff *skb)
{
	return skb->end - skb->data;
}
#endif

int ccci_skb_pool_alloc(struct skb_pools *pools);
void ccci_skb_pool_free(struct skb_pools *pools);
struct sk_buff *ccci_alloc_skb_from_pool(struct skb_pools *pools, size_t size, bool blocking);
struct sk_buff *ccci_alloc_skb(size_t size, bool blocking);
void ccci_free_skb(struct skb_pools *pools, struct sk_buff *skb);
struct sk_buff *ccci_skb_dequeue(struct workqueue_struct *reload_work_queue,
				 struct ccci_skb_queue *queue);
void ccci_skb_enqueue(struct ccci_skb_queue *queue, struct sk_buff *skb);
int ccci_skb_queue_alloc(struct ccci_skb_queue *queue, size_t skb_size, size_t max_len, bool fill);

#endif /* __T7XX_SKB_UTIL_H__ */
