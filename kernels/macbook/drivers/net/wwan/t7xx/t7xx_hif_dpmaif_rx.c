// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/pm_runtime.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "t7xx_dpmaif.h"
#include "t7xx_hif_dpmaif.h"
#include "t7xx_hif_dpmaif_rx.h"

#define DPMAIF_BAT_COUNT		8192
#define DPMAIF_FRG_COUNT		4814
#define DPMAIF_PIT_COUNT		(DPMAIF_BAT_COUNT * 2)

#define DPMAIF_BAT_CNT_THRESHOLD	30
#define DPMAIF_PIT_CNT_THRESHOLD	60
#define DPMAIF_RX_PUSH_THRESHOLD_MASK	0x7
#define DPMAIF_NOTIFY_RELEASE_COUNT	128
#define DPMAIF_POLL_PIT_TIME_US		20
#define DPMAIF_POLL_RX_TIME_US		10
#define DPMAIF_POLL_RX_TIMEOUT_US	200
#define DPMAIF_POLL_PIT_MAX_TIME_US	2000
#define DPMAIF_WQ_TIME_LIMIT_MS		2
#define DPMAIF_CS_RESULT_PASS		0

#define DPMAIF_SKB_OVER_HEAD		SKB_DATA_ALIGN(sizeof(struct skb_shared_info))
#define DPMAIF_SKB_SIZE_EXTRA		SKB_DATA_ALIGN(NET_SKB_PAD + DPMAIF_SKB_OVER_HEAD)
#define DPMAIF_SKB_SIZE(s)		((s) + DPMAIF_SKB_SIZE_EXTRA)
#define DPMAIF_SKB_Q_SIZE		(DPMAIF_BAT_COUNT * DPMAIF_SKB_SIZE(DPMAIF_HW_BAT_PKTBUF))
#define DPMAIF_SKB_SIZE_MIN		32
#define DPMAIF_RELOAD_TH_1		4
#define DPMAIF_RELOAD_TH_2		5

/* packet_type */
#define DES_PT_PD			0x00
#define DES_PT_MSG			0x01
/* buffer_type */
#define PKT_BUF_FRAG			0x01

static inline unsigned int normal_pit_bid(const struct dpmaif_normal_pit *pit_info)
{
	return (FIELD_GET(NORMAL_PIT_H_BID, pit_info->pit_footer) << 13) +
		FIELD_GET(NORMAL_PIT_BUFFER_ID, pit_info->pit_header);
}

static void dpmaif_set_skb_cs_type(const unsigned int cs_result, struct sk_buff *skb)
{
	if (cs_result == DPMAIF_CS_RESULT_PASS)
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		skb->ip_summed = CHECKSUM_NONE;
}

static int dpmaif_net_rx_push_thread(void *arg)
{
	struct dpmaif_ctrl *hif_ctrl;
	struct dpmaif_callbacks *cb;
	struct dpmaif_rx_queue *q;
	struct sk_buff *skb;
	u32 *lhif_header;
	int netif_id;

	q = arg;
	hif_ctrl = q->dpmaif_ctrl;
	cb = hif_ctrl->callbacks;
	while (!kthread_should_stop()) {
		if (skb_queue_empty(&q->skb_queue.skb_list)) {
			if (wait_event_interruptible(q->rx_wq,
						     !skb_queue_empty(&q->skb_queue.skb_list) ||
						     kthread_should_stop()))
				continue;
		}

		if (kthread_should_stop())
			break;

		skb = ccci_skb_dequeue(hif_ctrl->mtk_dev->pools.reload_work_queue,
				       &q->skb_queue);
		if (!skb)
			continue;

		lhif_header = (u32 *)skb->data;
		netif_id = FIELD_GET(LHIF_HEADER_NETIF, *lhif_header);
		skb_pull(skb, sizeof(*lhif_header));
		cb->recv_skb(hif_ctrl->mtk_dev, netif_id, skb);

		cond_resched();
	}

	return 0;
}

static int dpmaif_update_bat_wr_idx(struct dpmaif_ctrl *dpmaif_ctrl,
				    const unsigned char q_num, const unsigned int bat_cnt)
{
	unsigned short old_rl_idx, new_wr_idx, old_wr_idx;
	struct dpmaif_bat_request *bat_req;
	struct dpmaif_rx_queue *rxq;

	rxq = &dpmaif_ctrl->rxq[q_num];
	bat_req = rxq->bat_req;

	if (!rxq->que_started) {
		dev_err(dpmaif_ctrl->dev, "RX queue %d has not been started\n", rxq->index);
		return -EINVAL;
	}

	old_rl_idx = bat_req->bat_release_rd_idx;
	old_wr_idx = bat_req->bat_wr_idx;
	new_wr_idx = old_wr_idx + bat_cnt;

	if (old_rl_idx > old_wr_idx) {
		if (new_wr_idx >= old_rl_idx) {
			dev_err(dpmaif_ctrl->dev, "RX BAT flow check fail\n");
			return -EINVAL;
		}
	} else if (new_wr_idx >= bat_req->bat_size_cnt) {
		new_wr_idx -= bat_req->bat_size_cnt;
		if (new_wr_idx >= old_rl_idx) {
			dev_err(dpmaif_ctrl->dev, "RX BAT flow check fail\n");
			return -EINVAL;
		}
	}

	bat_req->bat_wr_idx = new_wr_idx;
	return 0;
}

#define GET_SKB_BY_ENTRY(skb_entry)\
	((struct dpmaif_skb_info *)list_entry(skb_entry, struct dpmaif_skb_info, entry))

static struct dpmaif_skb_info *alloc_and_map_skb_info(const struct dpmaif_ctrl *dpmaif_ctrl,
						      struct sk_buff *skb)
{
	struct dpmaif_skb_info *skb_info;
	dma_addr_t data_bus_addr;
	size_t data_len;

	skb_info = kmalloc(sizeof(*skb_info), GFP_KERNEL);
	if (!skb_info)
		return NULL;

	/* DMA mapping */
	data_len = skb_data_size(skb);
	data_bus_addr = dma_map_single(dpmaif_ctrl->dev, skb->data, data_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(dpmaif_ctrl->dev, data_bus_addr)) {
		dev_err_ratelimited(dpmaif_ctrl->dev, "DMA mapping error\n");
		kfree(skb_info);
		return NULL;
	}

	INIT_LIST_HEAD(&skb_info->entry);
	skb_info->skb = skb;
	skb_info->data_len = data_len;
	skb_info->data_bus_addr = data_bus_addr;
	return skb_info;
}

static struct list_head *dpmaif_map_skb_deq(struct dpmaif_map_skb *skb_list)
{
	struct list_head *entry;

	entry = skb_list->head.next;
	if (!list_empty(&skb_list->head) && entry) {
		list_del(entry);
		skb_list->qlen--;
		return entry;
	}

	return NULL;
}

static struct dpmaif_skb_info *dpmaif_skb_dequeue(struct dpmaif_skb_pool *pool,
						  struct dpmaif_skb_queue *queue)
{
	unsigned int max_len, qlen;
	struct list_head *entry;
	unsigned long flags;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	entry = dpmaif_map_skb_deq(&queue->skb_list);
	max_len = queue->max_len;
	qlen = queue->skb_list.qlen;
	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
	if (!entry)
		return NULL;

	if (qlen < max_len * DPMAIF_RELOAD_TH_1 / DPMAIF_RELOAD_TH_2)
		queue_work(pool->reload_work_queue, &pool->reload_work);

	return GET_SKB_BY_ENTRY(entry);
}

static struct dpmaif_skb_info *dpmaif_dev_alloc_skb(struct dpmaif_ctrl *dpmaif_ctrl,
						    const unsigned int size)
{
	struct dpmaif_skb_info *skb_info;
	struct sk_buff *skb;

	skb = __dev_alloc_skb(size, GFP_KERNEL);
	if (!skb)
		return NULL;

	skb_info = alloc_and_map_skb_info(dpmaif_ctrl, skb);
	if (!skb_info)
		dev_kfree_skb_any(skb);

	return skb_info;
}

static struct dpmaif_skb_info *dpmaif_alloc_skb(struct dpmaif_ctrl *dpmaif_ctrl,
						const unsigned int size)
{
	unsigned int i;

	if (size > DPMAIF_HW_BAT_PKTBUF)
		return dpmaif_dev_alloc_skb(dpmaif_ctrl, size);

	for (i = 0; i < dpmaif_ctrl->skb_pool.queue_cnt; i++) {
		struct dpmaif_skb_info *skb_info;
		struct dpmaif_skb_queue *queue;

		queue = &dpmaif_ctrl->skb_pool.queue[DPMA_SKB_QUEUE_CNT - 1 - i];

		if (size <= queue->size) {
			skb_info = dpmaif_skb_dequeue(&dpmaif_ctrl->skb_pool, queue);
			if (skb_info && skb_info->skb)
				return skb_info;

			kfree(skb_info);
			return dpmaif_dev_alloc_skb(dpmaif_ctrl, size);
		}
	}

	return NULL;
}

/**
 * dpmaif_rx_buf_alloc() - Allocates buffers for the BAT ring
 * @dpmaif_ctrl: Pointer to DPMAIF context structure
 * @bat_req: Pointer to BAT request structure
 * @q_num: Queue number
 * @buf_cnt: Number of buffers to allocate
 * @first_time: Indicates if the ring is being populated for the first time
 *
 * Allocates skb and store the start address of the data buffer into the BAT ring.
 * If the this is not the initial call then notify the HW about the new entries.
 *
 * Return: 0 on success, a negative error code on failure
 */
int dpmaif_rx_buf_alloc(struct dpmaif_ctrl *dpmaif_ctrl,
			const struct dpmaif_bat_request *bat_req,
			const unsigned char q_num, const unsigned int buf_cnt,
			const bool first_time)
{
	unsigned int alloc_cnt, i, hw_wr_idx;
	unsigned int bat_cnt, bat_max_cnt;
	unsigned short bat_start_idx;
	int ret;

	alloc_cnt = buf_cnt;
	if (!alloc_cnt || alloc_cnt > bat_req->bat_size_cnt)
		return -EINVAL;

	/* check BAT buffer space */
	bat_max_cnt = bat_req->bat_size_cnt;
	bat_cnt = ring_buf_read_write_count(bat_max_cnt, bat_req->bat_release_rd_idx,
					    bat_req->bat_wr_idx, false);

	if (alloc_cnt > bat_cnt)
		return -ENOMEM;

	bat_start_idx = bat_req->bat_wr_idx;

	/* Set buffer to be used */
	for (i = 0; i < alloc_cnt; i++) {
		unsigned short cur_bat_idx = bat_start_idx + i;
		struct dpmaif_bat_skb *cur_skb;
		struct dpmaif_bat *cur_bat;

		if (cur_bat_idx >= bat_max_cnt)
			cur_bat_idx -= bat_max_cnt;

		cur_skb = (struct dpmaif_bat_skb *)bat_req->bat_skb_ptr + cur_bat_idx;
		if (!cur_skb->skb) {
			struct dpmaif_skb_info *skb_info;

			skb_info = dpmaif_alloc_skb(dpmaif_ctrl, bat_req->pkt_buf_sz);
			if (!skb_info)
				break;

			cur_skb->skb = skb_info->skb;
			cur_skb->data_bus_addr = skb_info->data_bus_addr;
			cur_skb->data_len = skb_info->data_len;
			kfree(skb_info);
		}

		cur_bat = (struct dpmaif_bat *)bat_req->bat_base + cur_bat_idx;
		cur_bat->buffer_addr_ext = upper_32_bits(cur_skb->data_bus_addr);
		cur_bat->p_buffer_addr = lower_32_bits(cur_skb->data_bus_addr);
	}

	if (!i)
		return -ENOMEM;

	ret = dpmaif_update_bat_wr_idx(dpmaif_ctrl, q_num, i);
	if (ret)
		return ret;

	if (!first_time) {
		ret = dpmaif_dl_add_bat_cnt(dpmaif_ctrl, q_num, i);
		if (ret)
			return ret;

		hw_wr_idx = dpmaif_dl_get_bat_wridx(&dpmaif_ctrl->hif_hw_info, DPF_RX_QNO_DFT);
		if (hw_wr_idx != bat_req->bat_wr_idx) {
			ret = -EFAULT;
			dev_err(dpmaif_ctrl->dev, "Write index mismatch in Rx ring\n");
		}
	}

	return ret;
}

static int dpmaifq_release_rx_pit_entry(struct dpmaif_rx_queue *rxq,
					const unsigned short rel_entry_num)
{
	unsigned short old_sw_rel_idx, new_sw_rel_idx, old_hw_wr_idx;
	int ret;

	if (!rxq->que_started)
		return 0;

	if (rel_entry_num >= rxq->pit_size_cnt) {
		dev_err(rxq->dpmaif_ctrl->dev, "Invalid PIT entry release index.\n");
		return -EINVAL;
	}

	old_sw_rel_idx = rxq->pit_release_rd_idx;
	new_sw_rel_idx = old_sw_rel_idx + rel_entry_num;

	old_hw_wr_idx = rxq->pit_wr_idx;

	if (old_hw_wr_idx < old_sw_rel_idx && new_sw_rel_idx >= rxq->pit_size_cnt)
		new_sw_rel_idx = new_sw_rel_idx - rxq->pit_size_cnt;

	ret = dpmaif_dl_add_dlq_pit_remain_cnt(rxq->dpmaif_ctrl, rxq->index, rel_entry_num);

	if (ret) {
		dev_err(rxq->dpmaif_ctrl->dev,
			"PIT release failure, PIT-r/w/rel-%d,%d%d; rel_entry_num = %d, ret=%d\n",
			rxq->pit_rd_idx, rxq->pit_wr_idx,
			rxq->pit_release_rd_idx, rel_entry_num, ret);
		return ret;
	}
	rxq->pit_release_rd_idx = new_sw_rel_idx;
	return 0;
}

static void dpmaif_set_bat_mask(struct device *dev,
				struct dpmaif_bat_request *bat_req, unsigned int idx)
{
	unsigned long flags;

	spin_lock_irqsave(&bat_req->mask_lock, flags);
	if (!bat_req->bat_mask[idx])
		bat_req->bat_mask[idx] = 1;
	else
		dev_err(dev, "%s BAT mask was already set\n",
			bat_req->type == BAT_TYPE_NORMAL ? "Normal" : "Frag");

	spin_unlock_irqrestore(&bat_req->mask_lock, flags);
}

static int frag_bat_cur_bid_check(struct dpmaif_rx_queue *rxq,
				  const unsigned int cur_bid)
{
	struct dpmaif_bat_request *bat_frag;
	struct page *page;

	bat_frag = rxq->bat_frag;

	if (cur_bid >= DPMAIF_FRG_COUNT) {
		dev_err(rxq->dpmaif_ctrl->dev, "frag BAT cur_bid[%d] err\n", cur_bid);
		return -EINVAL;
	}

	page = ((struct dpmaif_bat_page *)bat_frag->bat_skb_ptr + cur_bid)->page;
	if (!page)
		return -EINVAL;

	return 0;
}

/**
 * dpmaif_rx_frag_alloc() - Allocates buffers for the Fragment BAT ring
 * @dpmaif_ctrl: Pointer to DPMAIF context structure
 * @bat_req: Pointer to BAT request structure
 * @q_num: Queue number
 * @buf_cnt: Number of buffers to allocate
 * @first_time: Indicates if the ring is being populated for the first time
 *
 * Fragment BAT is used when the received packet won't fit in a regular BAT entry.
 * This function allocates a page fragment and store the start address of the page
 * into the Fragment BAT ring.
 * If the this is not the initial call then notify the HW about the new entries.
 *
 * Return: 0 on success, a negative error code on failure
 */
int dpmaif_rx_frag_alloc(struct dpmaif_ctrl *dpmaif_ctrl, struct dpmaif_bat_request *bat_req,
			 unsigned char q_num, const unsigned int buf_cnt, const bool first_time)
{
	unsigned short cur_bat_idx;
	unsigned int buf_space;
	int i, ret = 0;

	if (!buf_cnt || buf_cnt > bat_req->bat_size_cnt)
		return -EINVAL;

	buf_space = ring_buf_read_write_count(bat_req->bat_size_cnt, bat_req->bat_release_rd_idx,
					      bat_req->bat_wr_idx, false);

	if (buf_cnt > buf_space) {
		dev_err(dpmaif_ctrl->dev,
			"Requested more buffers than the space available in Rx frag ring\n");
		return -EINVAL;
	}

	cur_bat_idx = bat_req->bat_wr_idx;

	for (i = 0; i < buf_cnt; i++) {
		struct dpmaif_bat_page *cur_page;
		struct dpmaif_bat *cur_bat;
		dma_addr_t data_base_addr;

		cur_page = (struct dpmaif_bat_page *)bat_req->bat_skb_ptr + cur_bat_idx;
		if (!cur_page->page) {
			unsigned long offset;
			struct page *page;
			void *data;

			data = netdev_alloc_frag(bat_req->pkt_buf_sz);
			if (!data)
				break;

			page = virt_to_head_page(data);
			offset = data - page_address(page);
			data_base_addr = dma_map_page(dpmaif_ctrl->dev, page, offset,
						      bat_req->pkt_buf_sz, DMA_FROM_DEVICE);

			if (dma_mapping_error(dpmaif_ctrl->dev, data_base_addr)) {
				dev_err(dpmaif_ctrl->dev, "DMA mapping fail\n");
				put_page(virt_to_head_page(data));
				break;
			}

			/* record frag information */
			cur_page->page = page;
			cur_page->data_bus_addr = data_base_addr;
			cur_page->offset = offset;
			cur_page->data_len = bat_req->pkt_buf_sz;
		}

		/* set to dpmaif HW */
		data_base_addr = cur_page->data_bus_addr;
		cur_bat = (struct dpmaif_bat *)bat_req->bat_base + cur_bat_idx;
		cur_bat->buffer_addr_ext = upper_32_bits(data_base_addr);
		cur_bat->p_buffer_addr = lower_32_bits(data_base_addr);

		cur_bat_idx = ring_buf_get_next_wrdx(bat_req->bat_size_cnt, cur_bat_idx);
	}

	if (i < buf_cnt)
		return -ENOMEM;

	bat_req->bat_wr_idx = cur_bat_idx;

	/* all rxq share one frag_bat table */
	q_num = DPF_RX_QNO_DFT;

	/* notify to HW */
	if (!first_time)
		dpmaif_dl_add_frg_cnt(dpmaif_ctrl, q_num, i);

	return ret;
}

static int dpmaif_set_rx_frag_to_skb(const struct dpmaif_rx_queue *rxq,
				     const struct dpmaif_normal_pit *pkt_info,
				     const struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	unsigned long long data_bus_addr, data_base_addr;
	struct dpmaif_bat_page *cur_page_info;
	struct sk_buff *base_skb;
	unsigned int data_len;
	struct device *dev;
	struct page *page;
	int data_offset;

	base_skb = rx_skb_info->cur_skb;
	dev = rxq->dpmaif_ctrl->dev;
	cur_page_info = rxq->bat_frag->bat_skb_ptr;
	cur_page_info += normal_pit_bid(pkt_info);
	page = cur_page_info->page;

	/* rx current frag data unmapping */
	dma_unmap_page(dev, cur_page_info->data_bus_addr,
		       cur_page_info->data_len, DMA_FROM_DEVICE);
	if (!page) {
		dev_err(dev, "frag check fail, bid:%d", normal_pit_bid(pkt_info));
		return -EINVAL;
	}

	/* calculate data address && data len. */
	data_bus_addr = pkt_info->data_addr_ext;
	data_bus_addr = (data_bus_addr << 32) + pkt_info->p_data_addr;
	data_base_addr = (unsigned long long)cur_page_info->data_bus_addr;
	data_offset = (int)(data_bus_addr - data_base_addr);

	data_len = FIELD_GET(NORMAL_PIT_DATA_LEN, pkt_info->pit_header);

	/* record to skb for user: fragment data to nr_frags */
	skb_add_rx_frag(base_skb, skb_shinfo(base_skb)->nr_frags, page,
			cur_page_info->offset + data_offset, data_len, cur_page_info->data_len);

	cur_page_info->page = NULL;
	cur_page_info->offset = 0;
	cur_page_info->data_len = 0;
	return 0;
}

static int dpmaif_get_rx_frag(struct dpmaif_rx_queue *rxq,
			      const struct dpmaif_normal_pit *pkt_info,
			      const struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	unsigned int cur_bid;
	int ret;

	cur_bid = normal_pit_bid(pkt_info);

	/* check frag bid */
	ret = frag_bat_cur_bid_check(rxq, cur_bid);
	if (ret < 0)
		return ret;

	/* set frag data to cur_skb skb_shinfo->frags[] */
	ret = dpmaif_set_rx_frag_to_skb(rxq, pkt_info, rx_skb_info);
	if (ret < 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "dpmaif_set_rx_frag_to_skb fail, ret = %d\n", ret);
		return ret;
	}

	dpmaif_set_bat_mask(rxq->dpmaif_ctrl->dev, rxq->bat_frag, cur_bid);
	return 0;
}

static inline int bat_cur_bid_check(struct dpmaif_rx_queue *rxq,
				    const unsigned int cur_bid)
{
	struct dpmaif_bat_skb *bat_req;
	struct dpmaif_bat_skb *bat_skb;

	bat_req = rxq->bat_req->bat_skb_ptr;
	bat_skb = bat_req + cur_bid;

	if (cur_bid >= DPMAIF_BAT_COUNT || !bat_skb->skb)
		return -EINVAL;

	return 0;
}

static int dpmaif_check_read_pit_seq(const struct dpmaif_normal_pit *pit)
{
	return FIELD_GET(NORMAL_PIT_PIT_SEQ, pit->pit_footer);
}

static int dpmaif_check_pit_seq(struct dpmaif_rx_queue *rxq,
				const struct dpmaif_normal_pit *pit)
{
	unsigned int cur_pit_seq, expect_pit_seq = rxq->expect_pit_seq;

	/* Running in soft interrupt therefore cannot sleep */
	if (read_poll_timeout_atomic(dpmaif_check_read_pit_seq, cur_pit_seq,
				     cur_pit_seq == expect_pit_seq, DPMAIF_POLL_PIT_TIME_US,
				     DPMAIF_POLL_PIT_MAX_TIME_US, false, pit))
		return -EFAULT;

	rxq->expect_pit_seq++;
	if (rxq->expect_pit_seq >= DPMAIF_DL_PIT_SEQ_VALUE)
		rxq->expect_pit_seq = 0;

	return 0;
}

static unsigned int dpmaif_avail_pkt_bat_cnt(struct dpmaif_bat_request *bat_req)
{
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&bat_req->mask_lock, flags);
	for (i = 0; i < bat_req->bat_size_cnt; i++) {
		unsigned int index = bat_req->bat_release_rd_idx + i;

		if (index >= bat_req->bat_size_cnt)
			index -= bat_req->bat_size_cnt;

		if (!bat_req->bat_mask[index])
			break;
	}

	spin_unlock_irqrestore(&bat_req->mask_lock, flags);

	return i;
}

static int dpmaif_release_dl_bat_entry(const struct dpmaif_rx_queue *rxq,
				       const unsigned int rel_entry_num,
				       const enum bat_type buf_type)
{
	unsigned short old_sw_rel_idx, new_sw_rel_idx, hw_rd_idx;
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_bat_request *bat;
	unsigned long flags;
	unsigned int i;

	dpmaif_ctrl = rxq->dpmaif_ctrl;

	if (!rxq->que_started)
		return -EINVAL;

	if (buf_type == BAT_TYPE_FRAG) {
		bat = rxq->bat_frag;
		hw_rd_idx = dpmaif_dl_get_frg_ridx(&dpmaif_ctrl->hif_hw_info, rxq->index);
	} else {
		bat = rxq->bat_req;
		hw_rd_idx = dpmaif_dl_get_bat_ridx(&dpmaif_ctrl->hif_hw_info, rxq->index);
	}

	if (rel_entry_num >= bat->bat_size_cnt || !rel_entry_num)
		return -EINVAL;

	old_sw_rel_idx = bat->bat_release_rd_idx;
	new_sw_rel_idx = old_sw_rel_idx + rel_entry_num;

	/* queue had empty and no need to release */
	if (bat->bat_wr_idx == old_sw_rel_idx)
		return 0;

	if (hw_rd_idx >= old_sw_rel_idx) {
		if (new_sw_rel_idx > hw_rd_idx)
			return -EINVAL;
	} else if (new_sw_rel_idx >= bat->bat_size_cnt) {
		new_sw_rel_idx -= bat->bat_size_cnt;
		if (new_sw_rel_idx > hw_rd_idx)
			return -EINVAL;
	}

	/* reset BAT mask value */
	spin_lock_irqsave(&bat->mask_lock, flags);
	for (i = 0; i < rel_entry_num; i++) {
		unsigned int index = bat->bat_release_rd_idx + i;

		if (index >= bat->bat_size_cnt)
			index -= bat->bat_size_cnt;

		bat->bat_mask[index] = 0;
	}

	spin_unlock_irqrestore(&bat->mask_lock, flags);
	bat->bat_release_rd_idx = new_sw_rel_idx;

	return rel_entry_num;
}

static int dpmaif_dl_pit_release_and_add(struct dpmaif_rx_queue *rxq)
{
	int ret;

	if (rxq->pit_remain_release_cnt < DPMAIF_PIT_CNT_THRESHOLD)
		return 0;

	ret = dpmaifq_release_rx_pit_entry(rxq, rxq->pit_remain_release_cnt);
	if (ret)
		dev_err(rxq->dpmaif_ctrl->dev, "release PIT fail\n");
	else
		rxq->pit_remain_release_cnt = 0;

	return ret;
}

static int dpmaif_dl_pkt_bat_release_and_add(const struct dpmaif_rx_queue *rxq)
{
	unsigned int bid_cnt;
	int ret;

	bid_cnt = dpmaif_avail_pkt_bat_cnt(rxq->bat_req);

	if (bid_cnt < DPMAIF_BAT_CNT_THRESHOLD)
		return 0;

	ret = dpmaif_release_dl_bat_entry(rxq, bid_cnt, BAT_TYPE_NORMAL);
	if (ret <= 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "release PKT BAT failed, ret:%d\n", ret);
		return ret;
	}

	ret = dpmaif_rx_buf_alloc(rxq->dpmaif_ctrl, rxq->bat_req, rxq->index, bid_cnt, false);

	if (ret < 0)
		dev_err(rxq->dpmaif_ctrl->dev, "allocate new RX buffer failed, ret:%d\n", ret);

	return ret;
}

static int dpmaif_dl_frag_bat_release_and_add(const struct dpmaif_rx_queue *rxq)
{
	unsigned int bid_cnt;
	int ret;

	bid_cnt = dpmaif_avail_pkt_bat_cnt(rxq->bat_frag);

	if (bid_cnt < DPMAIF_BAT_CNT_THRESHOLD)
		return 0;

	ret = dpmaif_release_dl_bat_entry(rxq, bid_cnt, BAT_TYPE_FRAG);
	if (ret <= 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "release PKT BAT failed, ret:%d\n", ret);
		return ret;
	}

	return dpmaif_rx_frag_alloc(rxq->dpmaif_ctrl, rxq->bat_frag, rxq->index, bid_cnt, false);
}

static inline void dpmaif_rx_msg_pit(const struct dpmaif_rx_queue *rxq,
				     const struct dpmaif_msg_pit *msg_pit,
				     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	rx_skb_info->cur_chn_idx = FIELD_GET(MSG_PIT_CHANNEL_ID, msg_pit->dword1);
	rx_skb_info->check_sum = FIELD_GET(MSG_PIT_CHECKSUM, msg_pit->dword1);
	rx_skb_info->pit_dp = FIELD_GET(MSG_PIT_DP, msg_pit->dword1);
}

static int dpmaif_rx_set_data_to_skb(const struct dpmaif_rx_queue *rxq,
				     const struct dpmaif_normal_pit *pkt_info,
				     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	unsigned long long data_bus_addr, data_base_addr;
	struct dpmaif_bat_skb *cur_bat_skb;
	struct sk_buff *new_skb;
	unsigned int data_len;
	struct device *dev;
	int data_offset;

	dev = rxq->dpmaif_ctrl->dev;
	cur_bat_skb = rxq->bat_req->bat_skb_ptr;
	cur_bat_skb += normal_pit_bid(pkt_info);
	dma_unmap_single(dev, cur_bat_skb->data_bus_addr, cur_bat_skb->data_len, DMA_FROM_DEVICE);

	/* calculate data address && data len. */
	/* cur pkt physical address(in a buffer bid pointed) */
	data_bus_addr = pkt_info->data_addr_ext;
	data_bus_addr = (data_bus_addr << 32) + pkt_info->p_data_addr;
	data_base_addr = (unsigned long long)cur_bat_skb->data_bus_addr;
	data_offset = (int)(data_bus_addr - data_base_addr);

	data_len = FIELD_GET(NORMAL_PIT_DATA_LEN, pkt_info->pit_header);
	/* cur pkt data len */
	/* record to skb for user: wapper, enqueue */
	/* get skb which data contained pkt data */
	new_skb = cur_bat_skb->skb;
	new_skb->len = 0;
	skb_reset_tail_pointer(new_skb);
	/* set data len, data pointer */
	skb_reserve(new_skb, data_offset);

	if (new_skb->tail + data_len > new_skb->end) {
		dev_err(dev, "No buffer space available\n");
		return -ENOBUFS;
	}

	skb_put(new_skb, data_len);

	rx_skb_info->cur_skb = new_skb;
	cur_bat_skb->skb = NULL;
	return 0;
}

static int dpmaif_get_rx_pkt(struct dpmaif_rx_queue *rxq,
			     const struct dpmaif_normal_pit *pkt_info,
			     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	unsigned int cur_bid;
	int ret;

	cur_bid = normal_pit_bid(pkt_info);
	ret = bat_cur_bid_check(rxq, cur_bid);
	if (ret < 0)
		return ret;

	ret = dpmaif_rx_set_data_to_skb(rxq, pkt_info, rx_skb_info);
	if (ret < 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "rx set data to skb failed, ret = %d\n", ret);
		return ret;
	}

	dpmaif_set_bat_mask(rxq->dpmaif_ctrl->dev, rxq->bat_req, cur_bid);
	return 0;
}

static int dpmaifq_rx_notify_hw(struct dpmaif_rx_queue *rxq)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	int ret;

	dpmaif_ctrl = rxq->dpmaif_ctrl;

	/* normal BAT release and add */
	queue_work(dpmaif_ctrl->bat_release_work_queue, &dpmaif_ctrl->bat_release_work);

	ret = dpmaif_dl_pit_release_and_add(rxq);
	if (ret < 0)
		dev_err(dpmaif_ctrl->dev, "dlq%d update PIT fail\n", rxq->index);

	return ret;
}

static void dpmaif_rx_skb(struct dpmaif_rx_queue *rxq, struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	struct sk_buff *new_skb;
	u32 *lhif_header;

	new_skb = rx_skb_info->cur_skb;
	rx_skb_info->cur_skb = NULL;

	if (rx_skb_info->pit_dp) {
		dev_kfree_skb_any(new_skb);
		return;
	}

	dpmaif_set_skb_cs_type(rx_skb_info->check_sum, new_skb);

	/* MD put the ccmni_index to the msg pkt,
	 * so we need push it alone. Maybe not needed.
	 */
	lhif_header = skb_push(new_skb, sizeof(*lhif_header));
	*lhif_header &= ~LHIF_HEADER_NETIF;
	*lhif_header |= FIELD_PREP(LHIF_HEADER_NETIF, rx_skb_info->cur_chn_idx);

	/* add data to rx thread skb list */
	ccci_skb_enqueue(&rxq->skb_queue, new_skb);
}

static int dpmaif_rx_start(struct dpmaif_rx_queue *rxq, const unsigned short pit_cnt,
			   const unsigned long timeout)
{
	struct dpmaif_cur_rx_skb_info *cur_rx_skb_info;
	unsigned short rx_cnt, recv_skb_cnt = 0;
	unsigned int cur_pit, pit_len;
	int ret = 0, ret_hw = 0;
	struct device *dev;

	dev = rxq->dpmaif_ctrl->dev;
	if (!rxq->pit_base)
		return -EAGAIN;

	pit_len = rxq->pit_size_cnt;
	cur_rx_skb_info = &rxq->rx_data_info;
	cur_pit = rxq->pit_rd_idx;

	for (rx_cnt = 0; rx_cnt < pit_cnt; rx_cnt++) {
		struct dpmaif_normal_pit *pkt_info;

		if (!cur_rx_skb_info->msg_pit_received && time_after_eq(jiffies, timeout))
			break;

		pkt_info = (struct dpmaif_normal_pit *)rxq->pit_base + cur_pit;

		if (dpmaif_check_pit_seq(rxq, pkt_info)) {
			dev_err_ratelimited(dev, "dlq%u checks PIT SEQ fail\n", rxq->index);
			return -EAGAIN;
		}

		if (FIELD_GET(NORMAL_PIT_PACKET_TYPE, pkt_info->pit_header) == DES_PT_MSG) {
			if (cur_rx_skb_info->msg_pit_received)
				dev_err(dev, "dlq%u receive repeat msg_pit err\n", rxq->index);
			cur_rx_skb_info->msg_pit_received = true;
			dpmaif_rx_msg_pit(rxq, (struct dpmaif_msg_pit *)pkt_info,
					  cur_rx_skb_info);
		} else { /* DES_PT_PD */
			if (FIELD_GET(NORMAL_PIT_BUFFER_TYPE, pkt_info->pit_header) !=
			    PKT_BUF_FRAG) {
				/* skb->data: add to skb ptr && record ptr */
				ret = dpmaif_get_rx_pkt(rxq, pkt_info, cur_rx_skb_info);
			} else if (!cur_rx_skb_info->cur_skb) {
				/* msg + frag PIT, no data pkt received */
				dev_err(dev,
					"rxq%d skb_idx < 0 PIT/BAT = %d, %d; buf: %ld; %d, %d\n",
					rxq->index, cur_pit, normal_pit_bid(pkt_info),
					FIELD_GET(NORMAL_PIT_BUFFER_TYPE, pkt_info->pit_header),
					rx_cnt, pit_cnt);
				ret = -EINVAL;
			} else {
				/* skb->frags[]: add to frags[] */
				ret = dpmaif_get_rx_frag(rxq, pkt_info, cur_rx_skb_info);
			}

			/* check rx status */
			if (ret < 0) {
				cur_rx_skb_info->err_payload = 1;
				dev_err_ratelimited(dev, "rxq%d error payload\n", rxq->index);
			}

			if (!FIELD_GET(NORMAL_PIT_CONT, pkt_info->pit_header)) {
				if (!cur_rx_skb_info->err_payload) {
					dpmaif_rx_skb(rxq, cur_rx_skb_info);
				} else if (cur_rx_skb_info->cur_skb) {
					dev_kfree_skb_any(cur_rx_skb_info->cur_skb);
					cur_rx_skb_info->cur_skb = NULL;
				}

				/* reinit cur_rx_skb_info */
				memset(cur_rx_skb_info, 0, sizeof(*cur_rx_skb_info));
				recv_skb_cnt++;
				if (!(recv_skb_cnt & DPMAIF_RX_PUSH_THRESHOLD_MASK)) {
					wake_up_all(&rxq->rx_wq);
					recv_skb_cnt = 0;
				}
			}
		}

		/* get next pointer to get pkt data */
		cur_pit = ring_buf_get_next_wrdx(pit_len, cur_pit);
		rxq->pit_rd_idx = cur_pit;

		/* notify HW++ */
		rxq->pit_remain_release_cnt++;
		if (rx_cnt > 0 && !(rx_cnt % DPMAIF_NOTIFY_RELEASE_COUNT)) {
			ret_hw = dpmaifq_rx_notify_hw(rxq);
			if (ret_hw < 0)
				break;
		}
	}

	/* still need sync to SW/HW cnt */
	if (recv_skb_cnt)
		wake_up_all(&rxq->rx_wq);

	/* update to HW */
	if (!ret_hw)
		ret_hw = dpmaifq_rx_notify_hw(rxq);

	if (ret_hw < 0 && !ret)
		ret = ret_hw;

	return ret < 0 ? ret : rx_cnt;
}

static unsigned int dpmaifq_poll_rx_pit(struct dpmaif_rx_queue *rxq)
{
	unsigned short hw_wr_idx;
	unsigned int pit_cnt;

	if (!rxq->que_started)
		return 0;

	hw_wr_idx = dpmaif_dl_dlq_pit_get_wridx(&rxq->dpmaif_ctrl->hif_hw_info, rxq->index);
	pit_cnt = ring_buf_read_write_count(rxq->pit_size_cnt, rxq->pit_rd_idx, hw_wr_idx, true);
	rxq->pit_wr_idx = hw_wr_idx;
	return pit_cnt;
}

static int dpmaif_rx_data_collect(struct dpmaif_ctrl *dpmaif_ctrl,
				  const unsigned char q_num, const int budget)
{
	struct dpmaif_rx_queue *rxq;
	unsigned long time_limit;
	unsigned int cnt;

	time_limit = jiffies + msecs_to_jiffies(DPMAIF_WQ_TIME_LIMIT_MS);
	rxq = &dpmaif_ctrl->rxq[q_num];

	do {
		cnt = dpmaifq_poll_rx_pit(rxq);
		if (cnt) {
			unsigned int rd_cnt = (cnt > budget) ? budget : cnt;
			int real_cnt = dpmaif_rx_start(rxq, rd_cnt, time_limit);

			if (real_cnt < 0) {
				if (real_cnt != -EAGAIN)
					dev_err(dpmaif_ctrl->dev, "dlq%u rx err: %d\n",
						rxq->index, real_cnt);
				return real_cnt;
			} else if (real_cnt < cnt) {
				return -EAGAIN;
			}
		}
	} while (cnt);

	return 0;
}

/* call after mtk_pci_disable_sleep */
static void dpmaif_do_rx(struct dpmaif_ctrl *dpmaif_ctrl, struct dpmaif_rx_queue *rxq)
{
	int ret;

	ret = dpmaif_rx_data_collect(dpmaif_ctrl, rxq->index, rxq->budget);
	if (ret < 0) {
		/* Rx done and empty interrupt will be enabled in workqueue */
		/* Try one more time */
		queue_work(rxq->worker, &rxq->dpmaif_rxq_work);
		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
	} else {
		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
		dpmaif_hw_dlq_unmask_rx_done(&dpmaif_ctrl->hif_hw_info, rxq->index);
	}
}

static void dpmaif_rxq_work(struct work_struct *work)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_rx_queue *rxq;
	int ret;

	rxq = container_of(work, struct dpmaif_rx_queue, dpmaif_rxq_work);
	dpmaif_ctrl = rxq->dpmaif_ctrl;

	atomic_set(&rxq->rx_processing, 1);
	 /* Ensure rx_processing is changed to 1 before actually begin RX flow */
	smp_mb();

	if (!rxq->que_started) {
		atomic_set(&rxq->rx_processing, 0);
		dev_err(dpmaif_ctrl->dev, "work RXQ: %d has not been started\n", rxq->index);
		return;
	}

	ret = pm_runtime_resume_and_get(dpmaif_ctrl->dev);
	if (ret < 0 && ret != -EACCES)
		return;

	mtk_pci_disable_sleep(dpmaif_ctrl->mtk_dev);

	/* we can try blocking wait for lock resource here in process context */
	if (mtk_pci_sleep_disable_complete(dpmaif_ctrl->mtk_dev))
		dpmaif_do_rx(dpmaif_ctrl, rxq);

	mtk_pci_enable_sleep(dpmaif_ctrl->mtk_dev);

	pm_runtime_mark_last_busy(dpmaif_ctrl->dev);
	pm_runtime_put_autosuspend(dpmaif_ctrl->dev);
	atomic_set(&rxq->rx_processing, 0);
}

void dpmaif_irq_rx_done(struct dpmaif_ctrl *dpmaif_ctrl, const unsigned int que_mask)
{
	struct dpmaif_rx_queue *rxq;
	int qno;

	/* get the queue id */
	qno = ffs(que_mask) - 1;
	if (qno < 0 || qno > DPMAIF_RXQ_NUM - 1) {
		dev_err(dpmaif_ctrl->dev, "rxq number err, qno:%u\n", qno);
		return;
	}

	rxq = &dpmaif_ctrl->rxq[qno];
	queue_work(rxq->worker, &rxq->dpmaif_rxq_work);
}

static void dpmaif_base_free(const struct dpmaif_ctrl *dpmaif_ctrl,
			     const struct dpmaif_bat_request *bat_req)
{
	if (bat_req->bat_base)
		dma_free_coherent(dpmaif_ctrl->dev,
				  bat_req->bat_size_cnt * sizeof(struct dpmaif_bat),
				  bat_req->bat_base, bat_req->bat_bus_addr);
}

/**
 * dpmaif_bat_alloc() - Allocate the BAT ring buffer
 * @dpmaif_ctrl: Pointer to DPMAIF context structure
 * @bat_req: Pointer to BAT request structure
 * @buf_type: BAT ring type
 *
 * This function allocates the BAT ring buffer shared with the HW device, also allocates
 * a buffer used to store information about the BAT skbs for further release.
 *
 * Return: 0 on success, a negative error code on failure
 */
int dpmaif_bat_alloc(const struct dpmaif_ctrl *dpmaif_ctrl,
		     struct dpmaif_bat_request *bat_req,
		     const enum bat_type buf_type)
{
	int sw_buf_size;

	sw_buf_size = (buf_type == BAT_TYPE_FRAG) ?
		sizeof(struct dpmaif_bat_page) : sizeof(struct dpmaif_bat_skb);

	bat_req->bat_size_cnt = (buf_type == BAT_TYPE_FRAG) ? DPMAIF_FRG_COUNT : DPMAIF_BAT_COUNT;

	bat_req->skb_pkt_cnt = bat_req->bat_size_cnt;
	bat_req->pkt_buf_sz = (buf_type == BAT_TYPE_FRAG) ? DPMAIF_HW_FRG_PKTBUF : NET_RX_BUF;

	bat_req->type = buf_type;
	bat_req->bat_wr_idx = 0;
	bat_req->bat_release_rd_idx = 0;

	/* alloc buffer for HW && AP SW */
	bat_req->bat_base = dma_alloc_coherent(dpmaif_ctrl->dev,
					       bat_req->bat_size_cnt * sizeof(struct dpmaif_bat),
					       &bat_req->bat_bus_addr, GFP_KERNEL | __GFP_ZERO);

	if (!bat_req->bat_base)
		return -ENOMEM;

	/* alloc buffer for AP SW to record skb information */
	bat_req->bat_skb_ptr = devm_kzalloc(dpmaif_ctrl->dev, bat_req->skb_pkt_cnt * sw_buf_size,
					    GFP_KERNEL);

	if (!bat_req->bat_skb_ptr)
		goto err_base;

	/* alloc buffer for BAT mask */
	bat_req->bat_mask = kcalloc(bat_req->bat_size_cnt, sizeof(unsigned char), GFP_KERNEL);
	if (!bat_req->bat_mask)
		goto err_base;

	spin_lock_init(&bat_req->mask_lock);
	atomic_set(&bat_req->refcnt, 0);
	return 0;

err_base:
	dpmaif_base_free(dpmaif_ctrl, bat_req);

	return -ENOMEM;
}

static void unmap_bat_skb(struct device *dev, struct dpmaif_bat_skb *bat_skb_base,
			  unsigned int index)
{
	struct dpmaif_bat_skb *bat_skb;

	bat_skb = bat_skb_base + index;

	if (bat_skb->skb) {
		dma_unmap_single(dev, bat_skb->data_bus_addr, bat_skb->data_len,
				 DMA_FROM_DEVICE);
		kfree_skb(bat_skb->skb);
		bat_skb->skb = NULL;
	}
}

static void unmap_bat_page(struct device *dev, struct dpmaif_bat_page *bat_page_base,
			   unsigned int index)
{
	struct dpmaif_bat_page *bat_page;

	bat_page = bat_page_base + index;

	if (bat_page->page) {
		dma_unmap_page(dev, bat_page->data_bus_addr, bat_page->data_len,
			       DMA_FROM_DEVICE);
		put_page(bat_page->page);
		bat_page->page = NULL;
	}
}

void dpmaif_bat_free(const struct dpmaif_ctrl *dpmaif_ctrl,
		     struct dpmaif_bat_request *bat_req)
{
	if (!bat_req)
		return;

	if (!atomic_dec_and_test(&bat_req->refcnt))
		return;

	kfree(bat_req->bat_mask);
	bat_req->bat_mask = NULL;

	if (bat_req->bat_skb_ptr) {
		unsigned int i;

		for (i = 0; i < bat_req->bat_size_cnt; i++) {
			if (bat_req->type == BAT_TYPE_FRAG)
				unmap_bat_page(dpmaif_ctrl->dev, bat_req->bat_skb_ptr, i);
			else
				unmap_bat_skb(dpmaif_ctrl->dev, bat_req->bat_skb_ptr, i);
		}
	}

	dpmaif_base_free(dpmaif_ctrl, bat_req);
}

static int dpmaif_rx_alloc(struct dpmaif_rx_queue *rxq)
{
	/* PIT buffer init */
	rxq->pit_size_cnt = DPMAIF_PIT_COUNT;
	rxq->pit_rd_idx = 0;
	rxq->pit_wr_idx = 0;
	rxq->pit_release_rd_idx = 0;
	rxq->expect_pit_seq = 0;
	rxq->pit_remain_release_cnt = 0;

	memset(&rxq->rx_data_info, 0, sizeof(rxq->rx_data_info));

	/* PIT allocation */
	rxq->pit_base = dma_alloc_coherent(rxq->dpmaif_ctrl->dev,
					   rxq->pit_size_cnt * sizeof(struct dpmaif_normal_pit),
					   &rxq->pit_bus_addr, GFP_KERNEL | __GFP_ZERO);

	if (!rxq->pit_base)
		return -ENOMEM;

	/* RXQ BAT table init */
	rxq->bat_req = &rxq->dpmaif_ctrl->bat_req;
	atomic_inc(&rxq->bat_req->refcnt);

	/* RXQ Frag BAT table init */
	rxq->bat_frag = &rxq->dpmaif_ctrl->bat_frag;
	atomic_inc(&rxq->bat_frag->refcnt);
	return 0;
}

static void dpmaif_rx_buf_free(const struct dpmaif_rx_queue *rxq)
{
	if (!rxq->dpmaif_ctrl)
		return;

	dpmaif_bat_free(rxq->dpmaif_ctrl, rxq->bat_req);
	dpmaif_bat_free(rxq->dpmaif_ctrl, rxq->bat_frag);

	if (rxq->pit_base)
		dma_free_coherent(rxq->dpmaif_ctrl->dev,
				  rxq->pit_size_cnt * sizeof(struct dpmaif_normal_pit),
				  rxq->pit_base, rxq->pit_bus_addr);
}

int dpmaif_rxq_alloc(struct dpmaif_rx_queue *queue)
{
	int ret;

	/* rxq data (PIT, BAT...) allocation */
	ret = dpmaif_rx_alloc(queue);
	if (ret < 0) {
		dev_err(queue->dpmaif_ctrl->dev, "rx buffer alloc fail, %d\n", ret);
		return ret;
	}

	INIT_WORK(&queue->dpmaif_rxq_work, dpmaif_rxq_work);
	queue->worker = alloc_workqueue("dpmaif_rx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI,
					1, queue->index);
	if (!queue->worker)
		return -ENOMEM;

	/* rx push thread and skb list init */
	init_waitqueue_head(&queue->rx_wq);
	ccci_skb_queue_alloc(&queue->skb_queue, queue->bat_req->skb_pkt_cnt,
			     queue->bat_req->pkt_buf_sz, false);
	queue->rx_thread = kthread_run(dpmaif_net_rx_push_thread,
				       queue, "dpmaif_rx%d_push", queue->index);
	if (IS_ERR(queue->rx_thread)) {
		dev_err(queue->dpmaif_ctrl->dev, "failed to start rx thread\n");
		return PTR_ERR(queue->rx_thread);
	}

	return 0;
}

void dpmaif_rxq_free(struct dpmaif_rx_queue *queue)
{
	struct sk_buff *skb;

	if (queue->worker)
		destroy_workqueue(queue->worker);

	if (queue->rx_thread)
		kthread_stop(queue->rx_thread);

	while ((skb = skb_dequeue(&queue->skb_queue.skb_list)))
		kfree_skb(skb);

	dpmaif_rx_buf_free(queue);
}

void dpmaif_skb_queue_free(struct dpmaif_ctrl *dpmaif_ctrl, const unsigned int index)
{
	struct dpmaif_skb_queue *queue;

	queue = &dpmaif_ctrl->skb_pool.queue[index];
	while (!list_empty(&queue->skb_list.head)) {
		struct list_head *entry = dpmaif_map_skb_deq(&queue->skb_list);

		if (entry) {
			struct dpmaif_skb_info *skb_info = GET_SKB_BY_ENTRY(entry);

			if (skb_info) {
				if (skb_info->skb) {
					dev_kfree_skb_any(skb_info->skb);
					skb_info->skb = NULL;
				}

				kfree(skb_info);
			}
		}
	}
}

static void skb_reload_work(struct work_struct *work)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_skb_pool *pool;
	unsigned int i;

	pool = container_of(work, struct dpmaif_skb_pool, reload_work);
	dpmaif_ctrl = container_of(pool, struct dpmaif_ctrl, skb_pool);

	for (i = 0; i < DPMA_SKB_QUEUE_CNT; i++) {
		struct dpmaif_skb_queue *queue = &pool->queue[i];
		unsigned int cnt, qlen, size, max_cnt;
		unsigned long flags;

		spin_lock_irqsave(&queue->skb_list.lock, flags);
		size = queue->size;
		max_cnt = queue->max_len;
		qlen = queue->skb_list.qlen;
		spin_unlock_irqrestore(&queue->skb_list.lock, flags);

		if (qlen >= max_cnt * DPMAIF_RELOAD_TH_1 / DPMAIF_RELOAD_TH_2)
			continue;

		cnt = max_cnt - qlen;
		while (cnt--) {
			struct dpmaif_skb_info *skb_info;

			skb_info = dpmaif_dev_alloc_skb(dpmaif_ctrl, size);
			if (!skb_info)
				return;

			spin_lock_irqsave(&queue->skb_list.lock, flags);
			list_add_tail(&skb_info->entry, &queue->skb_list.head);
			queue->skb_list.qlen++;
			spin_unlock_irqrestore(&queue->skb_list.lock, flags);
		}
	}
}

static int dpmaif_skb_queue_init_struct(struct dpmaif_ctrl *dpmaif_ctrl,
					const unsigned int index)
{
	struct dpmaif_skb_queue *queue;
	unsigned int max_cnt, size;

	queue = &dpmaif_ctrl->skb_pool.queue[index];
	size = DPMAIF_HW_BAT_PKTBUF / BIT(index);
	max_cnt = DPMAIF_SKB_Q_SIZE / DPMAIF_SKB_SIZE(size);

	if (size < DPMAIF_SKB_SIZE_MIN)
		return -EINVAL;

	INIT_LIST_HEAD(&queue->skb_list.head);
	spin_lock_init(&queue->skb_list.lock);
	queue->skb_list.qlen = 0;
	queue->size = size;
	queue->max_len = max_cnt;
	return 0;
}

/**
 * dpmaif_skb_pool_init() - Initialize DPMAIF SKB pool
 * @dpmaif_ctrl: Pointer to data path control struct dpmaif_ctrl
 *
 * Init the DPMAIF SKB queue structures that includes SKB pool, work and workqueue.
 *
 * Return: Zero on success and negative errno on failure
 */
int dpmaif_skb_pool_init(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_skb_pool *pool;
	int i;

	pool = &dpmaif_ctrl->skb_pool;
	pool->queue_cnt = DPMA_SKB_QUEUE_CNT;

	/* init the skb queue structure */
	for (i = 0; i < pool->queue_cnt; i++) {
		int ret = dpmaif_skb_queue_init_struct(dpmaif_ctrl, i);

		if (ret) {
			dev_err(dpmaif_ctrl->dev, "Init skb_queue:%d fail\n", i);
			return ret;
		}
	}

	/* init pool reload work */
	pool->reload_work_queue = alloc_workqueue("dpmaif_skb_pool_reload_work",
						  WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (!pool->reload_work_queue) {
		dev_err(dpmaif_ctrl->dev, "Create the reload_work_queue fail\n");
		return -ENOMEM;
	}

	INIT_WORK(&pool->reload_work, skb_reload_work);
	queue_work(pool->reload_work_queue, &pool->reload_work);

	return 0;
}

static void dpmaif_bat_release_work(struct work_struct *work)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_rx_queue *rxq;
	int ret;

	dpmaif_ctrl = container_of(work, struct dpmaif_ctrl, bat_release_work);

	ret = pm_runtime_resume_and_get(dpmaif_ctrl->dev);
	if (ret < 0 && ret != -EACCES)
		return;

	mtk_pci_disable_sleep(dpmaif_ctrl->mtk_dev);

	/* ALL RXQ use one BAT table, so choose DPF_RX_QNO_DFT */
	rxq = &dpmaif_ctrl->rxq[DPF_RX_QNO_DFT];

	if (mtk_pci_sleep_disable_complete(dpmaif_ctrl->mtk_dev)) {
		/* normal BAT release and add */
		dpmaif_dl_pkt_bat_release_and_add(rxq);
		/* frag BAT release and add */
		dpmaif_dl_frag_bat_release_and_add(rxq);
	}

	mtk_pci_enable_sleep(dpmaif_ctrl->mtk_dev);
	pm_runtime_mark_last_busy(dpmaif_ctrl->dev);
	pm_runtime_put_autosuspend(dpmaif_ctrl->dev);
}

int dpmaif_bat_release_work_alloc(struct dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_ctrl->bat_release_work_queue =
		alloc_workqueue("dpmaif_bat_release_work_queue", WQ_MEM_RECLAIM, 1);

	if (!dpmaif_ctrl->bat_release_work_queue)
		return -ENOMEM;

	INIT_WORK(&dpmaif_ctrl->bat_release_work, dpmaif_bat_release_work);

	return 0;
}

void dpmaif_bat_release_work_free(struct dpmaif_ctrl *dpmaif_ctrl)
{
	flush_work(&dpmaif_ctrl->bat_release_work);

	if (dpmaif_ctrl->bat_release_work_queue) {
		destroy_workqueue(dpmaif_ctrl->bat_release_work_queue);
		dpmaif_ctrl->bat_release_work_queue = NULL;
	}
}

/**
 * dpmaif_suspend_rx_sw_stop() - Suspend RX flow
 * @dpmaif_ctrl: Pointer to data path control struct dpmaif_ctrl
 *
 * Wait for all the RX work to finish executing and mark the RX queue as paused
 *
 */
void dpmaif_suspend_rx_sw_stop(struct dpmaif_ctrl *dpmaif_ctrl)
{
	unsigned int i;

	/* Disable DL/RX SW active */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		struct dpmaif_rx_queue *rxq;
		int timeout, value;

		rxq = &dpmaif_ctrl->rxq[i];
		flush_work(&rxq->dpmaif_rxq_work);

		timeout = readx_poll_timeout_atomic(atomic_read, &rxq->rx_processing, value,
						    !value, 0, DPMAIF_CHECK_INIT_TIMEOUT_US);

		if (timeout)
			dev_err(dpmaif_ctrl->dev, "Stop RX SW failed\n");

		/* Ensure RX processing has already been stopped before we toggle
		 * the rxq->que_started to false during RX suspend flow.
		 */
		smp_mb();
		rxq->que_started = false;
	}
}

static void dpmaif_stop_rxq(struct dpmaif_rx_queue *rxq)
{
	int cnt, j = 0;

	flush_work(&rxq->dpmaif_rxq_work);

	/* reset SW */
	rxq->que_started = false;
	do {
		/* disable HW arb and check idle */
		cnt = ring_buf_read_write_count(rxq->pit_size_cnt, rxq->pit_rd_idx,
						rxq->pit_wr_idx, true);
		/* retry handler */
		if (++j >= DPMAIF_MAX_CHECK_COUNT) {
			dev_err(rxq->dpmaif_ctrl->dev, "stop RX SW failed, %d\n", cnt);
			break;
		}
	} while (cnt);

	memset(rxq->pit_base, 0, rxq->pit_size_cnt * sizeof(struct dpmaif_normal_pit));
	memset(rxq->bat_req->bat_base, 0, rxq->bat_req->bat_size_cnt * sizeof(struct dpmaif_bat));
	memset(rxq->bat_req->bat_mask, 0, rxq->bat_req->bat_size_cnt * sizeof(unsigned char));
	memset(&rxq->rx_data_info, 0, sizeof(rxq->rx_data_info));

	rxq->pit_rd_idx = 0;
	rxq->pit_wr_idx = 0;
	rxq->pit_release_rd_idx = 0;

	rxq->expect_pit_seq = 0;
	rxq->pit_remain_release_cnt = 0;

	rxq->bat_req->bat_release_rd_idx = 0;
	rxq->bat_req->bat_wr_idx = 0;

	rxq->bat_frag->bat_release_rd_idx = 0;
	rxq->bat_frag->bat_wr_idx = 0;
}

void dpmaif_stop_rx_sw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	int i;

	for (i = 0; i < DPMAIF_RXQ_NUM; i++)
		dpmaif_stop_rxq(&dpmaif_ctrl->rxq[i]);
}
