// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/wait.h>

#include "t7xx_buffer_manager.h"

static struct ccci_skb_queue skb_pool_64k;
static struct ccci_skb_queue skb_pool_4k;
static struct ccci_skb_queue skb_pool_16;
static struct workqueue_struct *pool_reload_work_queue;

static struct sk_buff *alloc_skb_from_pool(int size)
{
	struct sk_buff *skb = NULL;

	if (size > SKB_4K)
		skb = ccci_skb_dequeue(&skb_pool_64k);
	else if (size > SKB_16)
		skb = ccci_skb_dequeue(&skb_pool_4k);
	else if (size > 0)
		skb = ccci_skb_dequeue(&skb_pool_16);

	return skb;
}

static struct sk_buff *alloc_skb_from_kernel(int size, gfp_t gfp_mask)
{
	struct sk_buff *skb = NULL;

	if (size > SKB_4K)
		skb = __dev_alloc_skb(SKB_64K, gfp_mask);
	else if (size > SKB_1_5K)
		skb = __dev_alloc_skb(SKB_4K, gfp_mask);
	else if (size > SKB_16)
		skb = __dev_alloc_skb(SKB_1_5K, gfp_mask);
	else if (size > 0)
		skb = __dev_alloc_skb(SKB_16, gfp_mask);

	return skb;
}

struct sk_buff *ccci_skb_dequeue(struct ccci_skb_queue *queue)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	result = __skb_dequeue(&queue->skb_list);
	if (queue->max_occupied < queue->max_len - queue->skb_list.qlen)
		queue->max_occupied = queue->max_len - queue->skb_list.qlen;

	queue->deq_count++;
	if (queue->pre_filled && queue->skb_list.qlen <
		queue->max_len / RELOAD_TH)
		queue_work(pool_reload_work_queue, &queue->reload_work);

	spin_unlock_irqrestore(&queue->skb_list.lock, flags);

	return result;
}

void ccci_skb_enqueue(struct ccci_skb_queue *queue, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	if (queue->skb_list.qlen < queue->max_len) {
		queue->enq_count++;
		__skb_queue_tail(&queue->skb_list, newsk);
		if (queue->skb_list.qlen > queue->max_history)
			queue->max_history = queue->skb_list.qlen;
	} else {
		dev_kfree_skb_any(newsk);
	}
	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
}

void ccci_skb_queue_init(struct ccci_skb_queue *queue,
			 unsigned int skb_size, unsigned int max_len, char fill_now)
{
	int i;

	skb_queue_head_init(&queue->skb_list);
	queue->max_len = max_len;
	if (fill_now) {
		for (i = 0; i < queue->max_len; i++) {
			struct sk_buff *skb =
				alloc_skb_from_kernel(skb_size, GFP_KERNEL);

			if (skb)
				skb_queue_tail(&queue->skb_list, skb);
		}
		queue->pre_filled = 1;
	} else {
		queue->pre_filled = 0;
	}
	queue->max_history = 0;
}

/* Caller should check for NULL return value
 */
struct sk_buff *ccci_alloc_skb(int size,
			       unsigned char from_pool, unsigned char blocking)
{
	int count = 0;
	struct sk_buff *skb;
	struct ccci_buffer_ctrl *buf_ctrl;

	if (size > SKB_64K || size < 0)
		return NULL;

	if (from_pool) {
 slow_retry:
		skb = alloc_skb_from_pool(size);
		if (!skb && blocking) {
			pr_err("from %ps skb pool is empty! size=%d retry count=%d\n",
			       __builtin_return_address(0), size, count++);
			msleep(100);
			goto slow_retry;
		}
		if (skb && skb_headroom(skb) == NET_SKB_PAD) {
			buf_ctrl = (struct ccci_buffer_ctrl *)
				   skb_push(skb, sizeof(struct ccci_buffer_ctrl));
			buf_ctrl->head_magic = CCCI_BUF_MAGIC;
			buf_ctrl->policy = RECYCLE;
			buf_ctrl->ioc_override = 0x0;
			skb_pull(skb, sizeof(struct ccci_buffer_ctrl));
		} else {
			pr_err("skb %p: fill headroom fail!\n", skb);
		}
	} else {
		if (blocking) {
			skb = alloc_skb_from_kernel(size, GFP_KERNEL);
		} else {
 fast_retry:
			skb = alloc_skb_from_kernel(size, GFP_ATOMIC);
			if (!skb && count++ < 20)
				goto fast_retry;
		}
	}

	return skb;
}

void ccci_free_skb(struct sk_buff *skb)
{
	struct ccci_buffer_ctrl *buf_ctrl;
	enum data_policy policy = FREE;

	buf_ctrl = (struct ccci_buffer_ctrl *)(skb->head
		+ NET_SKB_PAD - sizeof(struct ccci_buffer_ctrl));

	if (buf_ctrl->head_magic == CCCI_BUF_MAGIC) {
		policy = buf_ctrl->policy;
		memset(buf_ctrl, 0, sizeof(*buf_ctrl));
	}

	if (policy != RECYCLE || skb->dev ||
	    skb_size(skb) < NET_SKB_PAD + SKB_16)
		policy = FREE;

	switch (policy) {
	case RECYCLE:
		/* reset sk_buff (take __alloc_skb as ref.) */
		skb->data = skb->head;
		skb->len = 0;
		skb_reset_tail_pointer(skb);
		/* reserve memory as netdev_alloc_skb */
		skb_reserve(skb, NET_SKB_PAD);
		/* enqueue */
		if (skb_size(skb) < SKB_4K)
			ccci_skb_enqueue(&skb_pool_16, skb);
		else if (skb_size(skb) < SKB_64K)
			ccci_skb_enqueue(&skb_pool_4k, skb);
		else
			ccci_skb_enqueue(&skb_pool_64k, skb);
		break;
	case FREE:
		dev_kfree_skb_any(skb);
		break;
	}
}

static void reload_work_64k(struct work_struct *work)
{
	struct sk_buff *skb;

	while (skb_pool_64k.skb_list.qlen < SKB_POOL_SIZE_64K) {
		skb = alloc_skb_from_kernel(SKB_64K, GFP_KERNEL);
		if (skb)
			skb_queue_tail(&skb_pool_64k.skb_list, skb);
		else
			pr_err("fail to reload 64KB pool\n");
	}
}

static void reload_work_4k(struct work_struct *work)
{
	struct sk_buff *skb;

	while (skb_pool_4k.skb_list.qlen < SKB_POOL_SIZE_4K) {
		skb = alloc_skb_from_kernel(SKB_4K, GFP_KERNEL);
		if (skb)
			skb_queue_tail(&skb_pool_4k.skb_list, skb);
		else
			pr_err("fail to reload 4KB pool\n");
	}
}

static void reload_work_16(struct work_struct *work)
{
	struct sk_buff *skb;

	while (skb_pool_16.skb_list.qlen < SKB_POOL_SIZE_16) {
		skb = alloc_skb_from_kernel(SKB_16, GFP_KERNEL);
		if (skb)
			skb_queue_tail(&skb_pool_16.skb_list, skb);
		else
			pr_err("fail to reload 16B pool\n");
	}
}

void ccci_subsys_bm_init(void)
{
	/* init skb pool */
	ccci_skb_queue_init(&skb_pool_64k, SKB_64K, SKB_POOL_SIZE_64K, 1);
	ccci_skb_queue_init(&skb_pool_4k, SKB_4K, SKB_POOL_SIZE_4K, 1);
	ccci_skb_queue_init(&skb_pool_16, SKB_16, SKB_POOL_SIZE_16, 1);
	/* init pool reload work */
	pool_reload_work_queue = alloc_workqueue("pool_reload_work",
						 WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&skb_pool_64k.reload_work, reload_work_64k);
	INIT_WORK(&skb_pool_4k.reload_work, reload_work_4k);
	INIT_WORK(&skb_pool_16.reload_work, reload_work_16);
}

void ccci_subsys_bm_uninit(void)
{
	struct ccci_skb_queue *queue;
	struct sk_buff *newsk;

	flush_work(&skb_pool_64k.reload_work);
	flush_work(&skb_pool_4k.reload_work);
	flush_work(&skb_pool_16.reload_work);

	if (pool_reload_work_queue) {
		flush_workqueue(pool_reload_work_queue);
		destroy_workqueue(pool_reload_work_queue);
	}
	queue = &skb_pool_64k;
	while ((newsk = skb_dequeue(&queue->skb_list)) != NULL)
		dev_kfree_skb_any(newsk);

	queue = &skb_pool_4k;
	while ((newsk = skb_dequeue(&queue->skb_list)) != NULL)
		dev_kfree_skb_any(newsk);

	queue = &skb_pool_16;

	while ((newsk = skb_dequeue(&queue->skb_list)) != NULL)
		dev_kfree_skb_any(newsk);
}
