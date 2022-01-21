// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/skbuff.h>

#include "t7xx_pci.h"
#include "t7xx_skb_util.h"

/* define the pool size of each different skb length */
#define SKB_64K_POOL_SIZE	50
#define SKB_4K_POOL_SIZE	256
#define SKB_16_POOL_SIZE	64

/* reload pool if pool size dropped below 1/RELOAD_TH */
#define RELOAD_TH		3

#define ALLOC_SKB_RETRY		20

static struct sk_buff *alloc_skb_from_pool(struct skb_pools *pools, size_t size)
{
	if (size > MTK_SKB_4K)
		return ccci_skb_dequeue(pools->reload_work_queue, &pools->skb_pool_64k);
	else if (size > MTK_SKB_16)
		return ccci_skb_dequeue(pools->reload_work_queue, &pools->skb_pool_4k);
	else if (size > 0)
		return ccci_skb_dequeue(pools->reload_work_queue, &pools->skb_pool_16);

	return NULL;
}

static struct sk_buff *alloc_skb_from_kernel(size_t size, gfp_t gfp_mask)
{
	if (size > MTK_SKB_4K)
		return __dev_alloc_skb(MTK_SKB_64K, gfp_mask);
	else if (size > MTK_SKB_1_5K)
		return __dev_alloc_skb(MTK_SKB_4K, gfp_mask);
	else if (size > MTK_SKB_16)
		return __dev_alloc_skb(MTK_SKB_1_5K, gfp_mask);
	else if (size > 0)
		return __dev_alloc_skb(MTK_SKB_16, gfp_mask);

	return NULL;
}

struct sk_buff *ccci_skb_dequeue(struct workqueue_struct *reload_work_queue,
				 struct ccci_skb_queue *queue)
{
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	skb = __skb_dequeue(&queue->skb_list);
	if (queue->max_occupied < queue->max_len - queue->skb_list.qlen)
		queue->max_occupied = queue->max_len - queue->skb_list.qlen;

	queue->deq_count++;
	if (queue->pre_filled && queue->skb_list.qlen < queue->max_len / RELOAD_TH)
		queue_work(reload_work_queue, &queue->reload_work);

	spin_unlock_irqrestore(&queue->skb_list.lock, flags);

	return skb;
}

void ccci_skb_enqueue(struct ccci_skb_queue *queue, struct sk_buff *skb)
{
	unsigned long flags;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	if (queue->skb_list.qlen < queue->max_len) {
		queue->enq_count++;
		__skb_queue_tail(&queue->skb_list, skb);
		if (queue->skb_list.qlen > queue->max_history)
			queue->max_history = queue->skb_list.qlen;
	} else {
		dev_kfree_skb_any(skb);
	}

	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
}

int ccci_skb_queue_alloc(struct ccci_skb_queue *queue, size_t skb_size, size_t max_len, bool fill)
{
	skb_queue_head_init(&queue->skb_list);
	queue->max_len = max_len;
	if (fill) {
		unsigned int i;

		for (i = 0; i < queue->max_len; i++) {
			struct sk_buff *skb;

			skb = alloc_skb_from_kernel(skb_size, GFP_KERNEL);

			if (!skb) {
				while ((skb = skb_dequeue(&queue->skb_list)))
					dev_kfree_skb_any(skb);
				return -ENOMEM;
			}

			skb_queue_tail(&queue->skb_list, skb);
		}

		queue->pre_filled = true;
	} else {
		queue->pre_filled = false;
	}

	queue->max_history = 0;

	return 0;
}

/**
 * ccci_alloc_skb_from_pool() - allocate memory for skb from pre-allocated pools
 * @pools: skb pools
 * @size: allocation size
 * @blocking : true for blocking operation
 *
 * Returns pointer to skb on success, NULL on failure.
 */
struct sk_buff *ccci_alloc_skb_from_pool(struct skb_pools *pools, size_t size, bool blocking)
{
	struct ccci_buffer_ctrl *buf_ctrl;
	struct sk_buff *skb;
	int count;

	if (!size || size > MTK_SKB_64K)
		return NULL;

	for (count = 0; count < ALLOC_SKB_RETRY; count++) {
		skb = alloc_skb_from_pool(pools, size);
		if (skb || !blocking)
			break;

		/* no memory at the pool.
		 * waiting for pools reload_work which will allocate new memory from kernel
		 */
		might_sleep();
		msleep(MTK_SKB_WAIT_FOR_POOLS_RELOAD_MS);
	}

	if (skb && skb_headroom(skb) == NET_SKB_PAD) {
		buf_ctrl = skb_push(skb, sizeof(*buf_ctrl));
		buf_ctrl->head_magic = CCCI_BUF_MAGIC;
		buf_ctrl->policy = MTK_SKB_POLICY_RECYCLE;
		buf_ctrl->ioc_override = 0;
		skb_pull(skb, sizeof(struct ccci_buffer_ctrl));
	}

	return skb;
}

/**
 * ccci_alloc_skb() - allocate memory for skb from the kernel
 * @size: allocation size
 * @blocking : true for blocking operation
 *
 * Returns pointer to skb on success, NULL on failure.
 */
struct sk_buff *ccci_alloc_skb(size_t size, bool blocking)
{
	struct sk_buff *skb;
	int count;

	if (!size || size > MTK_SKB_64K)
		return NULL;

	if (blocking) {
		might_sleep();
		skb = alloc_skb_from_kernel(size, GFP_KERNEL);
	} else {
		for (count = 0; count < ALLOC_SKB_RETRY; count++) {
			skb = alloc_skb_from_kernel(size, GFP_ATOMIC);
			if (skb)
				return skb;
		}
	}

	return skb;
}

static void free_skb_from_pool(struct skb_pools *pools, struct sk_buff *skb)
{
	if (skb_size(skb) < MTK_SKB_4K)
		ccci_skb_enqueue(&pools->skb_pool_16, skb);
	else if (skb_size(skb) < MTK_SKB_64K)
		ccci_skb_enqueue(&pools->skb_pool_4k, skb);
	else
		ccci_skb_enqueue(&pools->skb_pool_64k, skb);
}

void ccci_free_skb(struct skb_pools *pools, struct sk_buff *skb)
{
	struct ccci_buffer_ctrl *buf_ctrl;
	enum data_policy policy;
	int offset;

	if (!skb)
		return;

	offset = NET_SKB_PAD - sizeof(*buf_ctrl);
	buf_ctrl = (struct ccci_buffer_ctrl *)(skb->head + offset);
	policy = MTK_SKB_POLICY_FREE;

	if (buf_ctrl->head_magic == CCCI_BUF_MAGIC) {
		policy = buf_ctrl->policy;
		memset(buf_ctrl, 0, sizeof(*buf_ctrl));
	}

	if (policy != MTK_SKB_POLICY_RECYCLE || skb->dev ||
	    skb_size(skb) < NET_SKB_PAD + MTK_SKB_16)
		policy = MTK_SKB_POLICY_FREE;

	switch (policy) {
	case MTK_SKB_POLICY_RECYCLE:
		skb->data = skb->head;
		skb->len = 0;
		skb_reset_tail_pointer(skb);
		/* reserve memory as netdev_alloc_skb */
		skb_reserve(skb, NET_SKB_PAD);
		/* enqueue */
		free_skb_from_pool(pools, skb);
		break;

	case MTK_SKB_POLICY_FREE:
		dev_kfree_skb_any(skb);
		break;

	default:
		break;
	}
}

static void reload_work_64k(struct work_struct *work)
{
	struct ccci_skb_queue *queue;
	struct sk_buff *skb;

	queue = container_of(work, struct ccci_skb_queue, reload_work);

	while (queue->skb_list.qlen < SKB_64K_POOL_SIZE) {
		skb = alloc_skb_from_kernel(MTK_SKB_64K, GFP_KERNEL);
		if (skb)
			skb_queue_tail(&queue->skb_list, skb);
	}
}

static void reload_work_4k(struct work_struct *work)
{
	struct ccci_skb_queue *queue;
	struct sk_buff *skb;

	queue = container_of(work, struct ccci_skb_queue, reload_work);

	while (queue->skb_list.qlen < SKB_4K_POOL_SIZE) {
		skb = alloc_skb_from_kernel(MTK_SKB_4K, GFP_KERNEL);
		if (skb)
			skb_queue_tail(&queue->skb_list, skb);
	}
}

static void reload_work_16(struct work_struct *work)
{
	struct ccci_skb_queue *queue;
	struct sk_buff *skb;

	queue = container_of(work, struct ccci_skb_queue, reload_work);

	while (queue->skb_list.qlen < SKB_16_POOL_SIZE) {
		skb = alloc_skb_from_kernel(MTK_SKB_16, GFP_KERNEL);
		if (skb)
			skb_queue_tail(&queue->skb_list, skb);
	}
}

int ccci_skb_pool_alloc(struct skb_pools *pools)
{
	struct sk_buff *skb;
	int ret;

	ret = ccci_skb_queue_alloc(&pools->skb_pool_64k,
				   MTK_SKB_64K, SKB_64K_POOL_SIZE, true);
	if (ret)
		return ret;

	ret = ccci_skb_queue_alloc(&pools->skb_pool_4k,
				   MTK_SKB_4K, SKB_4K_POOL_SIZE, true);
	if (ret)
		goto err_pool_4k;

	ret = ccci_skb_queue_alloc(&pools->skb_pool_16,
				   MTK_SKB_16, SKB_16_POOL_SIZE, true);
	if (ret)
		goto err_pool_16k;

	pools->reload_work_queue = alloc_workqueue("pool_reload_work",
						   WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI,
						   1);
	if (!pools->reload_work_queue) {
		ret = -ENOMEM;
		goto err_wq;
	}

	INIT_WORK(&pools->skb_pool_64k.reload_work, reload_work_64k);
	INIT_WORK(&pools->skb_pool_4k.reload_work, reload_work_4k);
	INIT_WORK(&pools->skb_pool_16.reload_work, reload_work_16);

	return ret;

err_wq:
	while ((skb = skb_dequeue(&pools->skb_pool_16.skb_list)))
		dev_kfree_skb_any(skb);
err_pool_16k:
	while ((skb = skb_dequeue(&pools->skb_pool_4k.skb_list)))
		dev_kfree_skb_any(skb);
err_pool_4k:
	while ((skb = skb_dequeue(&pools->skb_pool_64k.skb_list)))
		dev_kfree_skb_any(skb);

	return ret;
}

void ccci_skb_pool_free(struct skb_pools *pools)
{
	struct ccci_skb_queue *queue;
	struct sk_buff *newsk;

	flush_work(&pools->skb_pool_64k.reload_work);
	flush_work(&pools->skb_pool_4k.reload_work);
	flush_work(&pools->skb_pool_16.reload_work);

	if (pools->reload_work_queue)
		destroy_workqueue(pools->reload_work_queue);

	queue = &pools->skb_pool_64k;
	while ((newsk = skb_dequeue(&queue->skb_list)) != NULL)
		dev_kfree_skb_any(newsk);

	queue = &pools->skb_pool_4k;
	while ((newsk = skb_dequeue(&queue->skb_list)) != NULL)
		dev_kfree_skb_any(newsk);

	queue = &pools->skb_pool_16;
	while ((newsk = skb_dequeue(&queue->skb_list)) != NULL)
		dev_kfree_skb_any(newsk);
}
