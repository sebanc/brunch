/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __T7XX_BUFFER_MANAGER_H__
#define __T7XX_BUFFER_MANAGER_H__

#include <linux/skbuff.h>

#define CCCI_NET_MTU        1500
#define SKB_POOL_SIZE_64K   50
#define SKB_POOL_SIZE_4K    256
#define SKB_POOL_SIZE_16    64
#define SKB_64K 64528 /* 63kB + sizeof(ccci_header) */
#define SKB_4K  3584 /* 3.5kB */
#define SKB_1_5K (CCCI_NET_MTU + 16)	/* net MTU+CCCI_H, for network packet */
#define SKB_16 16			/* for struct ccci_header */
#define NET_RX_BUF SKB_4K

/* reload pool if pool size dropped below 1/RELOAD_TH */
#define RELOAD_TH           3

#define CCCI_BUF_MAGIC 0xFECDBA89

/* This tells the request free routine how to handle the skb.
 * The CCCI request structure will always be recycled, but its skb
 * can have a different policy. The CCCI request can work as a wrapper
 * because the network subsys will handle the skb itself.
 * Tx: policy is determined by sender
 * Rx: policy is determined by receiver
 */
enum data_policy {
	FREE = 0,		/* free the skb */
	RECYCLE,		/* put the skb back into our pool */
};

/* ccci buffer control structure. Must be less than NET_SKB_PAD */
struct ccci_buffer_ctrl {
	unsigned int head_magic;
	enum data_policy policy;
	/* bit7: override or not; bit0: IOC setting */
	unsigned char ioc_override;
	unsigned char blocking;
};

struct ccci_skb_queue {
	struct sk_buff_head skb_list;
	unsigned int max_len;
	struct work_struct reload_work;
	unsigned char pre_filled;
	unsigned int max_history;
	unsigned int max_occupied;
	unsigned int enq_count;
	unsigned int deq_count;
};

#ifdef NET_SKBUFF_DATA_USES_OFFSET
static inline unsigned int skb_size(struct sk_buff *x)
{
	return x->end;
}

static inline unsigned int skb_data_size(struct sk_buff *x)
{
	return x->head + x->end - x->data;
}
#else
static inline unsigned int skb_size(struct sk_buff *x)
{
	return x->end - x->head;
}

static inline unsigned int skb_data_size(struct sk_buff *x)
{
	return x->end - x->data;
}
#endif

void ccci_subsys_bm_init(void);
void ccci_subsys_bm_uninit(void);

struct sk_buff *ccci_alloc_skb(int size,
			       unsigned char from_pool, unsigned char blocking);
void ccci_free_skb(struct sk_buff *skb);

struct sk_buff *ccci_skb_dequeue(struct ccci_skb_queue *queue);
void ccci_skb_enqueue(struct ccci_skb_queue *queue, struct sk_buff *newsk);
void ccci_skb_queue_init(struct ccci_skb_queue *queue,
			 unsigned int skb_size, unsigned int max_len, char fill_now);

#endif /* __T7XX_BUFFER_MANAGER_H__ */
