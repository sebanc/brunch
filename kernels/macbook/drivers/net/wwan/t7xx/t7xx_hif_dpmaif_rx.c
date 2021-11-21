// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/kthread.h>
#include <linux/pm_runtime.h>

#include "t7xx_hif_dpmaif.h"
#include "t7xx_hif_dpmaif_rx.h"
#include "t7xx_dpmaif.h"
#include "t7xx_netdev.h"

#define DPMF_CUR_CPU_INVALID	(-1)

static void dpmf_get_cur_cpu(int *cpu)
{
	*cpu = get_cpu();
	put_cpu();
}

static int dpmaif_find_reload_cpu(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	int cpu;
	int i;

	for_each_online_cpu(cpu) {
		for (i = 0; i < DPMAIF_RXQ_NUM; i++)
			if (cpu == dpmaif_ctrl->rxq[i].cur_irq_cpu ||
			    cpu == dpmaif_ctrl->rxq[i].cur_rx_thrd_cpu)
				break;
		if (i == DPMAIF_RXQ_NUM)
			return cpu;
	}

	dpmf_get_cur_cpu(&cpu);

	return cpu;
}

static inline unsigned int nomal_pit_bid(struct dpmaifq_normal_pit *pit_info)
{
	return (pit_info->h_bid << 13) + pit_info->buffer_id;
}

static void dpmf_set_skb_cs_type(unsigned int cs_result, struct sk_buff *skb)
{
	if (cs_result == DPMF_CS_RESULT_PASS)
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		skb->ip_summed = CHECKSUM_NONE;
}

static unsigned int ring_buf_readable(unsigned int total_cnt,
				      unsigned int rd_idx,
				      unsigned int wrt_idx)
{
	unsigned int pkt_cnt;

	if (wrt_idx >= rd_idx)
		pkt_cnt = wrt_idx - rd_idx;
	else
		pkt_cnt = total_cnt + wrt_idx - rd_idx;

	return pkt_cnt;
}

/* RX part start */
static inline bool dpmaif_rx_push_need_reschedule(struct dpmaif_rx_queue *rxq)
{
	return (rxq->cur_rx_thrd_cpu == DPMF_CUR_CPU_INVALID ||
		rxq->cur_rx_thrd_cpu == rxq->cur_irq_cpu);
}

static int dpmaif_find_rx_push_cpu(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	int cpu;
	int i;

	for_each_online_cpu(cpu) {
		for (i = 0; i < DPMAIF_RXQ_NUM; i++)
			if (cpu == dpmaif_ctrl->rxq[i].cur_irq_cpu)
				break;
		if (i == DPMAIF_RXQ_NUM)
			return cpu;
	}

	dpmf_get_cur_cpu(&cpu);

	return cpu;
}

static inline void dpmaif_rx_push_thrd_reschedule(struct dpmaif_rx_queue *rxq)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	int cpu;

	/* set cpu affinity */
	cpu = dpmaif_find_rx_push_cpu(dpmaif_ctrl);
	if (!set_cpus_allowed_ptr(current, cpumask_of(cpu)))
		rxq->cur_rx_thrd_cpu = cpu;
	else
		dev_err(dpmaif_ctrl->dev,
			"Try to schedule rxq%d push thread(pid:%d) to cpu:%d failed!\n",
			rxq->index, current->pid, cpu);
}

static int dpmaif_net_rx_push_thread(void *arg)
{
	struct dpmaif_rx_queue *q = arg;
	struct md_dpmaif_ctrl *hif_ctrl = q->dpmaif_ctrl;
	struct sk_buff *skb;
	int count = 0;
	int netif_id;
	int ret;

	while (1) {
		if (skb_queue_empty(&q->skb_list.skb_list)) {
			ccmni_queue_state_notify(hif_ctrl->ctlb, RX_FLUSH, q->index);
			count = 0;

			ret = wait_event_interruptible(q->rx_wq,
						       !skb_queue_empty(&q->skb_list.skb_list));
			if (ret == -ERESTARTSYS)
				continue;
		}
		if (kthread_should_stop())
			break;

		if (dpmaif_rx_push_need_reschedule(q))
			dpmaif_rx_push_thrd_reschedule(q);

		skb = ccci_skb_dequeue(&q->skb_list);
		if (!skb)
			continue;

		netif_id = ((struct lhif_header *)skb->data)->netif;
		skb_pull(skb, sizeof(struct lhif_header));
		ccmni_rx_callback(hif_ctrl->ctlb, netif_id, skb, NULL);
		count++;

		/* avoid the rx thread soft lockup */
		if (need_resched())
			cond_resched();
	}
	return 0;
}

/* SW write, Update write index of ring buffer to HW */
static int dpmaifq_set_rx_bat(struct md_dpmaif_ctrl *dpmaif_ctrl,
			      unsigned char q_num, unsigned int bat_cnt)
{
	unsigned short old_sw_rl_idx, new_sw_wr_idx, old_sw_wr_idx;
	struct dpmaif_rx_queue *rxq = &dpmaif_ctrl->rxq[q_num];
	struct dpmaif_bat_request *bat_req = rxq->bat_req;
	int ret = 0;

	if (!rxq->que_started || bat_cnt >= bat_req->bat_size_cnt) {
		dev_err(dpmaif_ctrl->dev, "Rx queue started:%d BAT (%d) >= requested(%d)\n",
			rxq->que_started, bat_req->bat_size_cnt, bat_cnt);
		return -EINVAL;
	}

	old_sw_rl_idx = bat_req->bat_rel_rd_idx;
	old_sw_wr_idx = bat_req->bat_wr_idx;
	new_sw_wr_idx = old_sw_wr_idx + bat_cnt;

	if (old_sw_rl_idx > old_sw_wr_idx) {
		if (new_sw_wr_idx >= old_sw_rl_idx) {
			dev_err(dpmaif_ctrl->dev,
				"Rx BAT flow check fail\n");
			ret = -EINVAL;
		}
	} else {
		if (new_sw_wr_idx >= bat_req->bat_size_cnt) {
			new_sw_wr_idx = new_sw_wr_idx - bat_req->bat_size_cnt;
			if (new_sw_wr_idx >= old_sw_rl_idx) {
				dev_err(dpmaif_ctrl->dev,
					"Rx BAT flow check fail\n");
				ret = -EINVAL;
			}
		}
	}

	if (ret < 0)
		return ret;

	bat_req->bat_wr_idx = new_sw_wr_idx;

	return 0;
}

#define GET_SKB_BY_ENTRY(skb_entry)\
	((struct dpmaif_skb_info *)list_entry(skb_entry, struct dpmaif_skb_info, entry))

static struct dpmaif_skb_info *dpmaif_skb_dma_map(struct md_dpmaif_ctrl *dpmaif_ctrl,
						  struct sk_buff *skb)
{
	struct dpmaif_skb_info *skb_info;
	unsigned long long data_bus_addr;
	size_t data_len;

	if (!skb)
		return NULL;

	skb_info = kmalloc(sizeof(*skb_info), GFP_KERNEL);
	if (!skb_info)
		return NULL;

	/* DMA mapping */
	data_len = skb_data_size(skb);
	data_bus_addr = dma_map_single(dpmaif_ctrl->dev, skb->data, data_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(dpmaif_ctrl->dev, data_bus_addr)) {
		dev_err(dpmaif_ctrl->dev, "dma mapping fail\n");
		kfree(skb_info);
		return NULL;
	}

	INIT_LIST_HEAD(&skb_info->entry);
	skb_info->skb = skb;
	skb_info->data_len = data_len;
	skb_info->data_bus_addr = data_bus_addr;

	return skb_info;
}

static void *dpmaif_map_skb_deq(struct dpmaif_map_skb *skb_list)
{
	struct list_head *entry;

	entry = skb_list->head.next;
	if (!list_empty(&skb_list->head) && entry) {
		list_del(entry);
		skb_list->qlen--;
		return (void *)entry;
	}

	return NULL;
}

static struct dpmaif_skb_info *dpmaif_skb_dequeue(struct md_dpmaif_ctrl *dpmaif_ctrl,
						  unsigned int index)
{
	struct dpmaif_skb_info *skb_info = NULL;
	struct dpmaif_skb_queue *queue;
	struct dpmaif_skb_pool *pool;
	struct list_head *entry;
	unsigned int max_len, qlen;
	unsigned long flags;

	if (!dpmaif_ctrl || index >= DPMA_SKB_QUEUE_CNT)
		goto err;

	pool = &dpmaif_ctrl->skb_pool;
	queue = &pool->queue[index];

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	entry = dpmaif_map_skb_deq(&queue->skb_list);
	max_len = queue->max_len;
	qlen = queue->skb_list.qlen;
	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
	if (!entry)
		goto busy;

	/* reload the skb pool */
	if (qlen < max_len * DPMA_RELOAD_TH_1 / DPMA_RELOAD_TH_2) {
		queue_work_on(dpmaif_find_reload_cpu(dpmaif_ctrl),
			      pool->pool_reload_work_queue, &pool->reload_work);
	}

	skb_info = GET_SKB_BY_ENTRY(entry);

busy:
	if (!skb_info)
		queue->busy_cnt++;

	return skb_info;

err:
	return NULL;
}

static void *dpmaif_alloc_skb(struct md_dpmaif_ctrl *dpmaif_ctrl,
			      unsigned int size, int blocking)
{
	struct dpmaif_skb_info *skb_info = NULL;
	struct dpmaif_skb_queue *queue;
	struct sk_buff *skb;
	unsigned int index;

	if (size > DPMA_SKB_SIZE_MAX)
		goto from_kernel;

	for (index = 0; index < dpmaif_ctrl->skb_pool.queue_cnt; index++) {
		queue = &dpmaif_ctrl->skb_pool.queue[DPMA_SKB_QUEUE_CNT - 1 - index];
		if (size <= queue->size) {
			skb_info = dpmaif_skb_dequeue(dpmaif_ctrl,
						      DPMA_SKB_QUEUE_CNT - 1 - index);
			if (!skb_info || !skb_info->skb) {
				kfree(skb_info);
				skb_info = NULL;
				goto from_kernel;
			}

			break;
		}
	}

	return skb_info;

from_kernel:
	skb = __dev_alloc_skb(size, GFP_KERNEL);
	if (skb) {
		skb_info = dpmaif_skb_dma_map(dpmaif_ctrl, skb);
		if (!skb_info) {
			dev_err(dpmaif_ctrl->dev, "skb dma mapping fail\n");
			dev_kfree_skb_any(skb);
		}
	}

	return skb_info;
}

int dpmaif_alloc_rx_buf(struct md_dpmaif_ctrl *dpmaif_ctrl,
			struct dpmaif_bat_request *bat_req,
			unsigned char q_num, unsigned int buf_cnt,
			int blocking, bool first_time)
{
	struct dpmaif_skb_info *skb_info;
	struct sk_buff *new_skb = NULL;
	struct dpmaif_bat_skb *cur_skb;
	struct dpmaif_bat *cur_bat;

	unsigned short bat_star_idx, cur_bat_idx;
	unsigned int bat_cnt, bat_max_cnt;
	unsigned long long data_base_addr;
	unsigned int alloc_cnt, i;
	unsigned int hw_wr_idx;
	int ret, ret_hw = 0;
	size_t data_len;

	alloc_cnt = buf_cnt;
	if (alloc_cnt == 0 || alloc_cnt > bat_req->bat_size_cnt) {
		dev_err(dpmaif_ctrl->dev, "Alloc_cnt is invalid !\n");
		return 0;
	}

	/* check bat buffer space */
	bat_max_cnt = bat_req->bat_size_cnt;
	bat_cnt = ring_buf_writeable(bat_max_cnt, bat_req->bat_rel_rd_idx,
				     bat_req->bat_wr_idx);

	if (alloc_cnt > bat_cnt) {
		dev_err(dpmaif_ctrl->dev, "Alloc bat buf not enough(%d>%d)\n", alloc_cnt, bat_cnt);
		return -ENOMEM;
	}

	bat_star_idx = bat_req->bat_wr_idx;

	/* Set buffer to be used */
	for (i = 0; i < alloc_cnt; i++) {
		cur_bat_idx = bat_star_idx + i;
		if (cur_bat_idx >= bat_max_cnt)
			cur_bat_idx = cur_bat_idx - bat_max_cnt;

		/* re-init check */
		cur_skb = ((struct dpmaif_bat_skb *)bat_req->bat_skb_ptr +
			cur_bat_idx);
		if (!cur_skb->skb) {
			skb_info = dpmaif_alloc_skb(dpmaif_ctrl,
						    bat_req->pkt_buf_sz,
						    blocking);
			if (skb_info) {
				new_skb = skb_info->skb;
				data_base_addr = skb_info->data_bus_addr;
				data_len = skb_info->data_len;
				kfree(skb_info);
			} else {
				dev_err(dpmaif_ctrl->dev, "DPMAIF alloc skb fail\n");
				break;
			}

			if (!new_skb) {
				dev_err(dpmaif_ctrl->dev, "Alloc skb:%d fail on q%d!(%d/%d)\n",
					bat_req->pkt_buf_sz, q_num, cur_bat_idx, blocking);
				break;
			}

			cur_bat = (struct dpmaif_bat *)bat_req->bat_base + cur_bat_idx;

			cur_bat->buffer_addr_ext = (u32)(data_base_addr >> 32);
			cur_bat->p_buffer_addr = (u32)data_base_addr;

			cur_skb->skb = new_skb;
			cur_skb->data_bus_addr = data_base_addr;
			cur_skb->data_len = data_len;
		} else {
			data_base_addr = cur_skb->data_bus_addr;
			cur_bat = (struct dpmaif_bat *)bat_req->bat_base + cur_bat_idx;
			cur_bat->buffer_addr_ext = (u32)(data_base_addr >> 32);
			cur_bat->p_buffer_addr = (u32)data_base_addr;
		}
	}
	/* Ensure RX SKBs are allocated and BAT entries are updated done before
	 * we notify HW there are new available BAT entries
	 */
	wmb();
	if (i == 0)
		return -ENOMEM;

	ret = dpmaifq_set_rx_bat(dpmaif_ctrl, q_num, i);
	if (ret < 0)
		return ret;

	if (!first_time)
		ret_hw = dpmaif_dl_add_bat_cnt(&dpmaif_ctrl->hif_hw_info, q_num, i);

	hw_wr_idx = dpmaif_dl_get_bat_wridx(&dpmaif_ctrl->hif_hw_info, DPF_RX_QNO_DFT);

	if (!first_time &&
	    hw_wr_idx != bat_req->bat_wr_idx) {
		dev_err(dpmaif_ctrl->dev,
			"%s--err: wr_idx miss match! hw_wr_idx: %d, sw_wr_idx: %d\n",
			__func__, hw_wr_idx, bat_req->bat_wr_idx);
	}

	return ret_hw < 0 ? ret_hw : ret;
}

static int dpmaifq_rel_rx_pit_entry(struct dpmaif_rx_queue *rxq,
				    unsigned short rel_entry_num)
{
	unsigned short old_sw_rel_idx, new_sw_rel_idx, old_hw_wr_idx;
	int ret;

	if (!rxq->que_started)
		return 0;

	if (rel_entry_num >= rxq->pit_size_cnt) {
		dev_err(rxq->dpmaif_ctrl->dev, "Invalid PIT entry release index.\n");
		return -EINVAL;
	}

	old_sw_rel_idx = rxq->pit_rel_rd_idx;
	new_sw_rel_idx = old_sw_rel_idx + rel_entry_num;

	old_hw_wr_idx = rxq->pit_wr_idx;

	if (old_hw_wr_idx < old_sw_rel_idx && new_sw_rel_idx >= rxq->pit_size_cnt)
		new_sw_rel_idx = new_sw_rel_idx - rxq->pit_size_cnt;

	ret = dpmaif_dl_add_dlq_pit_remain_cnt(&rxq->dpmaif_ctrl->hif_hw_info,
					       rxq->index, rel_entry_num);

	if (!ret)
		rxq->pit_rel_rd_idx = new_sw_rel_idx;
	else
		dev_err(rxq->dpmaif_ctrl->dev,
			"PIT release failure, pit-r/w/rel-%d,%d%d; rel_entry_num = %d, ret=%d\n",
			rxq->pit_rd_idx, rxq->pit_wr_idx,
			rxq->pit_rel_rd_idx, rel_entry_num, ret);

	return ret;
}

static void dpmaif_set_frag_bat_mask(struct dpmaif_rx_queue *rxq, unsigned int bat_idx)
{
	spinlock_t *spin_lock = &rxq->dpmaif_ctrl->frag_bat_release_lock;
	unsigned long flags = 0;

	spin_lock_irqsave(spin_lock, flags);
	if (rxq->bat_frag->bat_mask[bat_idx] == 0)
		rxq->bat_frag->bat_mask[bat_idx] = 1;
	else
		dev_err(rxq->dpmaif_ctrl->dev,
			"Try to set a bat mask value which already has been 1!\n");

	spin_unlock_irqrestore(spin_lock, flags);
}

static void dpmaif_set_pkt_bat_mask(struct dpmaif_rx_queue *rxq, unsigned int bat_idx)
{
	spinlock_t *spin_lock = &rxq->dpmaif_ctrl->bat_release_lock;
	unsigned long flags;

	spin_lock_irqsave(spin_lock, flags);
	if (rxq->bat_req->bat_mask[bat_idx] == 0)
		rxq->bat_req->bat_mask[bat_idx] = 1;
	else
		dev_err(rxq->dpmaif_ctrl->dev,
			"Try to set a bat mask value which already has been 1!\n");

	spin_unlock_irqrestore(spin_lock, flags);
}

static int frag_bat_cur_bid_check(struct dpmaif_rx_queue *rxq,
				  unsigned int cur_bid)
{
	struct dpmaif_bat_request *bat_frag = rxq->bat_frag;
	struct page *page;

	if (cur_bid >= DPMAIF_DL_FRG_ENTRY_SIZE) {
		dev_err(rxq->dpmaif_ctrl->dev, "cur_bid[%d] err\n", cur_bid);
		return -EINVAL;
	}

	page = ((struct dpmaif_bat_page *)bat_frag->bat_skb_ptr +
		cur_bid)->page;
	if (!page) {
		rxq->check_bid_fail_cnt++;
		return -EINVAL;
	}

	return 0;
}

int dpmaif_alloc_rx_frag(struct md_dpmaif_ctrl *dpmaif_ctrl,
			 struct dpmaif_bat_request *bat_req,
			 unsigned char q_num, unsigned int buf_cnt,
			 int blocking, bool first_time)
{
	struct dpmaif_bat_page *cur_page;
	struct dpmaif_bat *cur_bat;
	struct page *page;

	int size = L1_CACHE_ALIGN(bat_req->pkt_buf_sz);
	unsigned long long data_base_addr;
	unsigned short cur_bat_idx;
	unsigned int buf_space;
	unsigned long offset;
	int i, ret = 0;
	void *data;

	if (buf_cnt == 0 || buf_cnt > bat_req->bat_size_cnt) {
		dev_err(dpmaif_ctrl->dev, "frag alloc_cnt is invalid !\n");
		return 0;
	}

	/* check bat buffer space */
	buf_space = ring_buf_writeable(bat_req->bat_size_cnt,
				       bat_req->bat_rel_rd_idx,
				       bat_req->bat_wr_idx);

	if (buf_cnt > buf_space) {
		dev_err(dpmaif_ctrl->dev, "alloc rx frag not enough(%d>%d)\n", buf_cnt, buf_space);
		return -ENOMEM;
	}

	cur_bat_idx = bat_req->bat_wr_idx;

	for (i = 0; i < buf_cnt; i++) {
		cur_page = ((struct dpmaif_bat_page *)bat_req->bat_skb_ptr +
			cur_bat_idx);

		if (!cur_page->page) {
			/* allocates a frag from a page for receive buffer. */
			data = netdev_alloc_frag(size);
			if (!data) {
				ret = -ENOMEM;
				break;
			}

			page = virt_to_head_page(data);
			offset = data - page_address(page);

			/* Get physical address of the RB */
			data_base_addr = dma_map_page(dpmaif_ctrl->dev,
						      page, offset,
						      bat_req->pkt_buf_sz,
						      DMA_FROM_DEVICE);
			if (dma_mapping_error(dpmaif_ctrl->dev, data_base_addr)) {
				dev_err(dpmaif_ctrl->dev, "DMA mapping fail\n");
				put_page(virt_to_head_page(data));
				ret = -ENOMEM;
				break;
			}

			/* set to dpmaif HW */
			cur_bat = ((struct dpmaif_bat *)bat_req->bat_base +
				cur_bat_idx);
			cur_bat->buffer_addr_ext = (u32)(data_base_addr >> 32);
			cur_bat->p_buffer_addr = (u32)data_base_addr;

			/* record frag information */
			cur_page->page = page;
			cur_page->data_bus_addr = data_base_addr;
			cur_page->offset = offset;
			cur_page->data_len = bat_req->pkt_buf_sz;
		} else {
			data_base_addr = cur_page->data_bus_addr;

			/* set to dpmaif HW */
			cur_bat = ((struct dpmaif_bat *)bat_req->bat_base +
				cur_bat_idx);
			cur_bat->buffer_addr_ext = (u32)(data_base_addr >> 32);
			cur_bat->p_buffer_addr = (u32)data_base_addr;
		}

		cur_bat_idx = ring_buf_get_next_wrdx(bat_req->bat_size_cnt,
						     cur_bat_idx);
	}
	/* Ensure RX page fragments are allocated and FRAG BAT entries are
	 * updated done before notifying HW about new available FRAG BAT entries
	 */
	wmb();

	bat_req->bat_wr_idx = cur_bat_idx;

	/* all rxq share one frag_bat table */
	q_num = DPF_RX_QNO_DFT;

	/* notify to HW */
	if (!first_time)
		dpmaif_dl_add_frg_cnt(&dpmaif_ctrl->hif_hw_info, q_num, i);

	return ret;
}

static int dpmaif_set_rx_frag_to_skb(struct dpmaif_rx_queue *rxq,
				     struct dpmaifq_normal_pit *pkt_inf_t,
				     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	struct dpmaif_bat_page *cur_page_info = ((struct dpmaif_bat_page *)
		rxq->bat_frag->bat_skb_ptr + nomal_pit_bid(pkt_inf_t));
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	struct sk_buff *base_skb = rx_skb_info->cur_skb;
	struct page *page = cur_page_info->page;

	unsigned long long data_bus_addr, data_base_addr;
	unsigned int data_len;
	int data_offset;

	/* rx current frag data unmapping */
	dma_unmap_page(dpmaif_ctrl->dev, cur_page_info->data_bus_addr,
		       cur_page_info->data_len, DMA_FROM_DEVICE);
	if (!page) {
		dev_err(dpmaif_ctrl->dev, "frag check fail-bid:%d",
			nomal_pit_bid(pkt_inf_t));
		return -EINVAL;
	}

	/* calculate data address && data len. */
	data_bus_addr = pkt_inf_t->data_addr_ext;
	data_bus_addr = (data_bus_addr << 32) + pkt_inf_t->p_data_addr;
	data_base_addr = (unsigned long long)cur_page_info->data_bus_addr;
	data_offset = (int)(data_bus_addr - data_base_addr);

	data_len = pkt_inf_t->data_len;

	/* record to skb for user: fragment data to nr_frags */
	skb_add_rx_frag(base_skb, skb_shinfo(base_skb)->nr_frags,
			page, cur_page_info->offset + data_offset,
			data_len, cur_page_info->data_len);

	cur_page_info->page = NULL;
	cur_page_info->offset = 0;
	cur_page_info->data_len = 0;

	return 0;
}

static int dpmaif_get_rx_frag(struct dpmaif_rx_queue *rxq,
			      int blocking, struct dpmaifq_normal_pit *pkt_inf_t,
			      struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	unsigned int cur_bid;
	int ret;

	cur_bid = nomal_pit_bid(pkt_inf_t);

	/* check frag bid */
	ret = frag_bat_cur_bid_check(rxq, cur_bid);
	if (ret < 0)
		return ret;

	/* set frag data to cur_skb skb_shinfo->frags[] */
	ret = dpmaif_set_rx_frag_to_skb(rxq, pkt_inf_t, rx_skb_info);
	if (ret < 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "dpmaif_set_rx_frag_to_skb fail, ret = %d\n", ret);
		return ret;
	}

	/* mask bat and update BAT and reallocate buffer if needed */
	dpmaif_set_frag_bat_mask(rxq, cur_bid);

	return 0;
}

static int bat_cur_bid_check(struct dpmaif_rx_queue *rxq,
			     unsigned int cur_bid)
{
	struct dpmaif_bat_request *bat_req = rxq->bat_req;
	struct sk_buff *skb =
		((struct dpmaif_bat_skb *)bat_req->bat_skb_ptr +
			cur_bid)->skb;

	if (!skb || cur_bid >= DPMAIF_DL_BAT_ENTRY_SIZE) {
		rxq->check_bid_fail_cnt++;
		if (skb) {
			dev_kfree_skb_any(skb);
			((struct dpmaif_bat_skb *)bat_req->bat_skb_ptr + cur_bid)->skb = NULL;
		}
		return -EINVAL;
	}

	return 0;
}

static int dpmaif_check_pit_seq(struct dpmaif_rx_queue *rxq,
				struct dpmaifq_normal_pit *pit)
{
	unsigned int expect_pit_seq;
	unsigned int cur_pit_seq;
	unsigned int count = 0;
	int ret = -1;

	if (!pit) {
		dev_err(rxq->dpmaif_ctrl->dev, "txq%d Invalid PIT pointer\n", rxq->index);
		goto exit;
	}

	expect_pit_seq = rxq->expect_pit_seq;

	while (count <= DPMF_POLL_PIT_CNT_MAX) {
		cur_pit_seq = pit->pit_seq;
		if (cur_pit_seq > DPMF_PIT_SEQ_MAX) {
			dev_err(rxq->dpmaif_ctrl->dev, "txq%d Invalid PIT SEQ: %u, max: %u\n",
				rxq->index, cur_pit_seq, DPMF_PIT_SEQ_MAX);
			ret = -1;
			break;
		}

		if (cur_pit_seq == expect_pit_seq) {
			/* Check result pass */
			rxq->expect_pit_seq++;
			if (rxq->expect_pit_seq >= DPMF_PIT_SEQ_MAX)
				rxq->expect_pit_seq = 0;

			rxq->pit_seq_fail_cnt = 0;
			ret = 0;

			goto exit;
		} else {
			count++;
		}

		udelay(DPMF_POLL_PIT_TIME);
	}

	rxq->pit_seq_fail_cnt++;
	if (rxq->pit_seq_fail_cnt >= DPMAIF_PIT_SEQ_CHECK_FAIL_CNT) {
		ret = -1;
		rxq->pit_seq_fail_cnt = 0;
	}

exit:
	return ret;
}

static unsigned int dpmaif_chk_avail_pkt_bat_cnt(struct dpmaif_rx_queue *rxq)
{
	spinlock_t *spin_lock = &rxq->dpmaif_ctrl->bat_release_lock;
	unsigned int count = 0;
	unsigned char mask_val;
	unsigned long flags;
	unsigned int index;
	unsigned int i;

	for (i = 0; i < rxq->bat_req->bat_size_cnt; i++) {
		index = rxq->bat_req->bat_rel_rd_idx + i;
		if (index >= rxq->bat_req->bat_size_cnt)
			index -= rxq->bat_req->bat_size_cnt;

		spin_lock_irqsave(spin_lock, flags);
		mask_val = rxq->bat_req->bat_mask[index];
		spin_unlock_irqrestore(spin_lock, flags);

		if (mask_val == 1)
			count++;
		else
			break;
	}

	if (count > rxq->bat_req->bat_size_cnt) {
		dev_err(rxq->dpmaif_ctrl->dev, "PKT_BAT count(%u) > total PKT_BAT count(%u)\n",
			count, rxq->bat_req->bat_size_cnt);
		return 0;
	}

	return count;
}

static unsigned int dpmaif_chk_avail_frag_bat_cnt(struct dpmaif_rx_queue *rxq)
{
	spinlock_t *spin_lock = &rxq->dpmaif_ctrl->frag_bat_release_lock;
	unsigned int count = 0;
	unsigned char mask_val;
	unsigned long flags;
	unsigned int index;
	unsigned int i;

	for (i = 0; i < rxq->bat_frag->bat_size_cnt; i++) {
		index = rxq->bat_frag->bat_rel_rd_idx + i;
		if (index >= rxq->bat_frag->bat_size_cnt)
			index -= rxq->bat_frag->bat_size_cnt;

		spin_lock_irqsave(spin_lock, flags);
		mask_val = rxq->bat_frag->bat_mask[index];
		spin_unlock_irqrestore(spin_lock, flags);

		if (mask_val == 1)
			count++;
		else
			break;
	}

	if (count > rxq->bat_frag->bat_size_cnt) {
		dev_err(rxq->dpmaif_ctrl->dev, "FRAG_BAT count(%u) > total FRAG_BAT count(%u)\n",
			count, rxq->bat_frag->bat_size_cnt);
		return 0;
	}

	return count;
}

static int dpmaif_rel_dl_bat_entry(struct dpmaif_rx_queue *rxq,
				   unsigned int rel_entry_num)
{
	spinlock_t *spin_lock = &rxq->dpmaif_ctrl->bat_release_lock;
	unsigned short old_sw_rel_idx, new_sw_rel_idx, hw_rd_idx;
	unsigned char q_num = rxq->index;
	unsigned long flags;
	unsigned int index;
	unsigned int i;

	if (!rxq->que_started)
		goto error;

	if (rel_entry_num >= rxq->bat_req->bat_size_cnt || rel_entry_num == 0)
		goto error;

	old_sw_rel_idx = rxq->bat_req->bat_rel_rd_idx;
	new_sw_rel_idx = old_sw_rel_idx + rel_entry_num;

	hw_rd_idx = dpmaif_dl_get_bat_ridx(&rxq->dpmaif_ctrl->hif_hw_info, q_num);
	/* queue had empty and no need to release */
	if (rxq->bat_req->bat_wr_idx == old_sw_rel_idx)
		goto error;

	if (hw_rd_idx > old_sw_rel_idx) {
		if (new_sw_rel_idx > hw_rd_idx)
			goto error;
	} else if (hw_rd_idx < old_sw_rel_idx) {
		if (new_sw_rel_idx >= rxq->bat_req->bat_size_cnt) {
			new_sw_rel_idx = new_sw_rel_idx - rxq->bat_req->bat_size_cnt;
			if (new_sw_rel_idx > hw_rd_idx)
				goto error;
		}
	}

	/* reset bat mask value */
	for (i = 0; i < rel_entry_num; i++) {
		index = rxq->bat_req->bat_rel_rd_idx + i;
		if (index >= rxq->bat_req->bat_size_cnt)
			index -= rxq->bat_req->bat_size_cnt;

		spin_lock_irqsave(spin_lock, flags);
		rxq->bat_req->bat_mask[index] = 0;
		spin_unlock_irqrestore(spin_lock, flags);
	}

	rxq->bat_req->bat_rel_rd_idx = new_sw_rel_idx;

	return rel_entry_num;

error:
	return -EINVAL;
}

static int dpmaif_rel_dl_frag_bat_entry(struct dpmaif_rx_queue *rxq,
					unsigned int rel_entry_num)
{
	spinlock_t *spin_lock = &rxq->dpmaif_ctrl->frag_bat_release_lock;
	unsigned short old_sw_rel_idx, new_sw_rel_idx, hw_rd_idx;
	unsigned char q_num = rxq->index;
	unsigned long flags;
	unsigned int index;
	unsigned int i;

	if (!rxq->que_started)
		return -EINVAL;

	if (rel_entry_num >= rxq->bat_frag->bat_size_cnt || rel_entry_num == 0)
		goto err;

	old_sw_rel_idx = rxq->bat_frag->bat_rel_rd_idx;
	new_sw_rel_idx = old_sw_rel_idx + rel_entry_num;

	hw_rd_idx = dpmaif_dl_get_frg_ridx(&rxq->dpmaif_ctrl->hif_hw_info, q_num);

	/* queue had empty and no need to release */
	if (rxq->bat_frag->bat_wr_idx == old_sw_rel_idx)
		goto err;

	if (hw_rd_idx > old_sw_rel_idx) {
		if (new_sw_rel_idx > hw_rd_idx)
			goto err;
	} else if (hw_rd_idx < old_sw_rel_idx) {
		if (new_sw_rel_idx >= rxq->bat_frag->bat_size_cnt) {
			new_sw_rel_idx = new_sw_rel_idx - rxq->bat_frag->bat_size_cnt;
			if (new_sw_rel_idx > hw_rd_idx)
				goto err;
		}
	}

	/* reset bat mask value */
	for (i = 0; i < rel_entry_num; i++) {
		index = rxq->bat_frag->bat_rel_rd_idx + i;
		if (index >= rxq->bat_frag->bat_size_cnt)
			index -= rxq->bat_frag->bat_size_cnt;

		spin_lock_irqsave(spin_lock, flags);
		rxq->bat_frag->bat_mask[index] = 0;
		spin_unlock_irqrestore(spin_lock, flags);
	}

	rxq->bat_frag->bat_rel_rd_idx = new_sw_rel_idx;
	return rel_entry_num;

err:
	return -EINVAL;
}

static int dpmaif_dl_pit_rel_and_add(struct dpmaif_rx_queue *rxq)
{
	int ret = 0;

	if (rxq->pit_remain_rel_cnt) {
		if (rxq->pit_remain_rel_cnt >= DPMF_PIT_CNT_UPDATE_THRESHOLD) {
			ret = dpmaifq_rel_rx_pit_entry(rxq,
						       rxq->pit_remain_rel_cnt);
			if (ret == 0)
				rxq->pit_remain_rel_cnt = 0;
			else
				dev_err(rxq->dpmaif_ctrl->dev,
					"release pit fail, ret:%d\n", ret);
		}
	}
	return ret;
}

static int dpmaif_dl_pkt_bat_rel_and_add(struct dpmaif_rx_queue *rxq,
					 int blocking)
{
	unsigned int bid_cnt;
	int ret = 0;

	bid_cnt = dpmaif_chk_avail_pkt_bat_cnt(rxq);
	if (bid_cnt == 0)
		goto exit;

	/* batch release: DPMF_BAT_CNT_UPDATE_THRESHOLD */
	if (bid_cnt < DPMF_BAT_CNT_UPDATE_THRESHOLD)
		goto exit;

	ret = dpmaif_rel_dl_bat_entry(rxq, bid_cnt);
	if (ret <= 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "Release PKT BAT failed!\n");
		goto exit;
	}

	ret = dpmaif_alloc_rx_buf(rxq->dpmaif_ctrl, rxq->bat_req,
				  rxq->index, bid_cnt, blocking, false);

	if (ret < 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "Allocate new RX buffer failed! ret:%d\n", ret);
		goto exit;
	}

exit:
	return ret;
}

static int dpmaif_dl_frag_bat_rel_and_add(struct dpmaif_rx_queue *rxq,
					  int blocking)
{
	unsigned int bid_cnt;
	int ret = 0;

	bid_cnt = dpmaif_chk_avail_frag_bat_cnt(rxq);
	if (bid_cnt == 0)
		goto exit;

	/* Frag BAT batch release: DPMF_BAT_CNT_UPDATE_THRESHOLD */
	if (bid_cnt < DPMF_BAT_CNT_UPDATE_THRESHOLD)
		goto exit;

	ret = dpmaif_rel_dl_frag_bat_entry(rxq, bid_cnt);
	if (ret <= 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "Release PKT BAT failed!\n");
		goto exit;
	}

	ret = dpmaif_alloc_rx_frag(rxq->dpmaif_ctrl, rxq->bat_frag,
				   rxq->index, bid_cnt, blocking, false);
	if (ret < 0)
		goto exit;

exit:
	return ret;
}

static inline void dpmaif_rx_msg_pit(struct dpmaif_rx_queue *rxq,
				     struct dpmaifq_msg_pit *msg_pit,
				     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	rx_skb_info->cur_chn_idx = msg_pit->channel_id;
	rx_skb_info->check_sum = msg_pit->check_sum;
	rx_skb_info->pit_dp = msg_pit->dp;
}

static int dpmaif_rx_set_data_to_skb(struct dpmaif_rx_queue *rxq,
				     struct dpmaifq_normal_pit *pkt_inf_t,
				     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	struct dpmaif_bat_skb *cur_bat_skb = ((struct dpmaif_bat_skb *)
		rxq->bat_req->bat_skb_ptr + nomal_pit_bid(pkt_inf_t));
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	struct sk_buff *new_skb;

	unsigned long long data_bus_addr, data_base_addr;
	int data_offset;
	unsigned int data_len;
	unsigned int *temp_u32;
	/* rx current skb data unmapping */
	dma_unmap_single(dpmaif_ctrl->dev, cur_bat_skb->data_bus_addr,
			 cur_bat_skb->data_len, DMA_FROM_DEVICE);

	/* calculate data address && data len. */
	/* cur pkt physical address(in a buffer bid pointed) */
	data_bus_addr = pkt_inf_t->data_addr_ext;
	data_bus_addr = (data_bus_addr << 32) + pkt_inf_t->p_data_addr;
	data_base_addr = (unsigned long long)cur_bat_skb->data_bus_addr;
	data_offset = (int)(data_bus_addr - data_base_addr);

	data_len = pkt_inf_t->data_len;
	/* cur pkt data len */
	/* record to skb for user: wapper, enqueue */
	/* get skb which data contained pkt data */
	new_skb = cur_bat_skb->skb;
	new_skb->len = 0;
	skb_reset_tail_pointer(new_skb);
	/* set data len, data pointer */
	skb_reserve(new_skb, data_offset);

	if ((new_skb->tail + data_len) > new_skb->end) {
		dev_err(dpmaif_ctrl->dev, "rxq%d pkt(%d/%d): len = 0x%x, offset=0x%llx-0x%llx\n",
			rxq->index, rxq->pit_rd_idx, nomal_pit_bid(pkt_inf_t),
			data_len, data_bus_addr, data_base_addr);
		dev_err(dpmaif_ctrl->dev, "skb(%p, %p, 0x%x, 0x%x)\n",
			new_skb->head, new_skb->data,
			(unsigned int)new_skb->tail,
			(unsigned int)new_skb->end);
		if (rxq->pit_rd_idx > 2) {
			temp_u32 = (unsigned int *)
				((struct dpmaifq_normal_pit *)
				rxq->pit_base + rxq->pit_rd_idx - 2);
			dev_err(dpmaif_ctrl->dev,
				"pit:%d data(%x, %x, %x, %x, %x, %x, %x, %x, %x)\n",
				rxq->pit_rd_idx - 2, temp_u32[0], temp_u32[1],
				temp_u32[2], temp_u32[3], temp_u32[4],
				temp_u32[5], temp_u32[6],
				temp_u32[7], temp_u32[8]);
		}
		return -EINVAL;
	}

	skb_put(new_skb, data_len);

	rx_skb_info->cur_skb = new_skb;
	cur_bat_skb->skb = NULL;
	return 0;
}

static int dpmaif_get_rx_pkt(struct dpmaif_rx_queue *rxq,
			     int blocking, struct dpmaifq_normal_pit *pkt_inf_t,
			     struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	unsigned int cur_bid;
	int ret;

	/* cur BID in pit info */
	cur_bid = nomal_pit_bid(pkt_inf_t);

	/* check bid */
	ret = bat_cur_bid_check(rxq, cur_bid);
	if (ret < 0)
		return ret;

	/* set data to skb->data. */
	ret = dpmaif_rx_set_data_to_skb(rxq, pkt_inf_t, rx_skb_info);
	if (ret < 0) {
		dev_err(rxq->dpmaif_ctrl->dev, "rx set data to skb failed! ret = %d\n", ret);
		return ret;
	}

	/* mask bat, update BAT and reallocate buffer if needed */
	dpmaif_set_pkt_bat_mask(rxq, cur_bid);

	return 0;
}

static int dpmaifq_rx_notify_hw(struct dpmaif_rx_queue *rxq, int blocking)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	int ret;

	/* normal bat release and add */
	queue_work_on(dpmaif_find_reload_cpu(dpmaif_ctrl),
		      dpmaif_ctrl->bat_rel_work_queue, &dpmaif_ctrl->bat_rel_work);

	ret = dpmaif_dl_pit_rel_and_add(rxq);
	if (ret < 0)
		dev_err(dpmaif_ctrl->dev, "dlq%d update pit fail\n", rxq->index);

	return ret;
}

static void dpmaif_rx_skb(struct dpmaif_rx_queue *rxq,
			  struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	struct sk_buff *new_skb = rx_skb_info->cur_skb;
	struct lhif_header *lhif_h;

	if (rx_skb_info->pit_dp) {
		dev_kfree_skb_any(new_skb);
		goto err;
	}

	dpmf_set_skb_cs_type(rx_skb_info->check_sum, new_skb);

	if (!dpmaif_ctrl->napi_enable) {
		/* md put the ccmni_index to the msg pkt,
		 * so we need push it by self. maybe no need
		 */
		lhif_h = (struct lhif_header *)(skb_push(new_skb,
				sizeof(struct lhif_header)));
		lhif_h->netif = rx_skb_info->cur_chn_idx;

		/* Add data to rx thread SKB list */
		ccci_skb_enqueue(&rxq->skb_list, new_skb);
	} else {
		ccmni_rx_callback(dpmaif_ctrl->ctlb,
				  rx_skb_info->cur_chn_idx, new_skb, NULL);
	}

err:
	rx_skb_info->cur_skb = NULL;
}

static int dpmaif_rx_start(struct dpmaif_rx_queue *rxq, unsigned short pit_cnt,
			   int blocking, unsigned long time_limit)
{
	struct dpmaif_cur_rx_skb_info *cur_rx_skb_info;
	struct dpmaifq_normal_pit *pkt_inf_t;
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	unsigned short recv_skb_cnt = 0;
	unsigned int cur_pit, pit_len;
	unsigned short rx_cnt;
	int ret = 0, ret_hw = 0;

	if (!rxq || !rxq->dpmaif_ctrl) {
		pr_err("NULL pointer!\n");
		goto error;
	}

	pit_len = rxq->pit_size_cnt;
	cur_rx_skb_info = &rxq->rx_data_info;

	dpmaif_ctrl = rxq->dpmaif_ctrl;
	cur_pit = rxq->pit_rd_idx;

	for (rx_cnt = 0; rx_cnt < pit_cnt; rx_cnt++) {
		if (!cur_rx_skb_info->msg_pit_received)
			if (!blocking && time_after_eq(jiffies, time_limit))
				break;

		pkt_inf_t = (struct dpmaifq_normal_pit *)rxq->pit_base + cur_pit;

		if (dpmaif_check_pit_seq(rxq, pkt_inf_t)) {
			pr_err_ratelimited("dlq%u checks PIT SEQ fail\n", rxq->index);
			goto error;
		}

		if (pkt_inf_t->packet_type == DES_PT_MSG) {
			if (cur_rx_skb_info->msg_pit_received)
				dev_err(dpmaif_ctrl->dev, "dlq%u receive repeat msg_pit err\n",
					rxq->index);
			cur_rx_skb_info->msg_pit_received = true;
			dpmaif_rx_msg_pit(rxq,
					  (struct dpmaifq_msg_pit *)pkt_inf_t,
					  cur_rx_skb_info);
		} else if (pkt_inf_t->packet_type == DES_PT_PD) {
			if (pkt_inf_t->buffer_type != PKT_BUF_FRAG) {
				/* skb->data: add to skb ptr && record ptr.*/
				ret = dpmaif_get_rx_pkt(rxq, blocking,
							pkt_inf_t, cur_rx_skb_info);
			} else if (!cur_rx_skb_info->cur_skb) {
				/* msg + frag pit, no data pkt received. */
				dev_err(dpmaif_ctrl->dev,
					"rxq%d skb_idx < 0 pit/bat = %d, %d; buf: %d; %d, %d\n",
					rxq->index, cur_pit, nomal_pit_bid(pkt_inf_t),
					pkt_inf_t->buffer_type, rx_cnt,
					pit_cnt);
				ret = -EINVAL;
			} else {
				/* skb->frags[]: add to frags[]*/
				ret = dpmaif_get_rx_frag(rxq, blocking,
							 pkt_inf_t, cur_rx_skb_info);
			}

			/* check rx status */
			if (ret < 0) {
				cur_rx_skb_info->err_payload = 1;
				pr_err_ratelimited("rxq%d error payload!\n", rxq->index);
			}

			if (pkt_inf_t->c_bit == 0) {
				if (cur_rx_skb_info->err_payload == 0) {
					dpmaif_rx_skb(rxq, cur_rx_skb_info);
				} else if (cur_rx_skb_info->cur_skb) {
					dev_kfree_skb_any(cur_rx_skb_info->cur_skb);
					cur_rx_skb_info->cur_skb = NULL;
				}

				/* reinit cur_rx_skb_info */
				memset(cur_rx_skb_info, 0x00,
				       sizeof(struct dpmaif_cur_rx_skb_info));
				cur_rx_skb_info->msg_pit_received = false;

				if (!dpmaif_ctrl->napi_enable) {
					recv_skb_cnt++;
					if ((recv_skb_cnt & DPMF_RX_PUSH_THRESHOLD_MASK) == 0) {
						wake_up_all(&rxq->rx_wq);
						recv_skb_cnt = 0;
					}
				}
			}
		}

		/* get next pointer to get pkt data */
		cur_pit = ring_buf_get_next_wrdx(pit_len, cur_pit);
		rxq->pit_rd_idx = cur_pit;

		/* notify HW++ */
		rxq->pit_remain_rel_cnt++;
		if (rx_cnt > 0 && (rx_cnt % DPMF_NOTIFY_RELEASE_COUNT) == 0) {
			ret_hw = dpmaifq_rx_notify_hw(rxq, blocking);
			if (ret_hw < 0)
				break;
		}
	}

	/* still need sync to SW/HW cnt */
	if (!dpmaif_ctrl->napi_enable) {
		if (recv_skb_cnt)
			wake_up_all(&rxq->rx_wq);
	}

	/* update to HW */
	if (ret_hw == 0)
		ret_hw = dpmaifq_rx_notify_hw(rxq, blocking);
	if (ret_hw < 0 && ret == 0)
		ret = ret_hw;

	return ret < 0 ? ret : rx_cnt;

error:
	return -EAGAIN;
}

static unsigned int dpmaifq_poll_rx_pit(struct dpmaif_rx_queue *rxq)
{
	unsigned short sw_rd_idx, hw_wr_idx;
	unsigned int pit_cnt;

	if (!rxq->que_started)
		return 0;

	sw_rd_idx = rxq->pit_rd_idx;
	hw_wr_idx = dpmaif_dl_dlq_pit_get_wridx(&rxq->dpmaif_ctrl->hif_hw_info, rxq->index);
	pit_cnt = ring_buf_readable(rxq->pit_size_cnt, sw_rd_idx, hw_wr_idx);
	rxq->pit_wr_idx = hw_wr_idx;
	return pit_cnt;
}

struct napi_struct *dpmaif_get_napi(struct md_dpmaif_ctrl *dpmaif_ctrl,
				    int qno)
{
	struct dpmaif_rx_queue *rxq = &dpmaif_ctrl->rxq[qno];

	if (!rxq) {
		pr_err("dpmaif queue is a NULL pointer!\n");
		goto error;
	}

	return &rxq->napi;

error:
	return NULL;
}

static int dpmaif_napi_rx_skb(struct dpmaif_rx_queue *rxq,
			      struct dpmaif_cur_rx_skb_info *rx_skb_info)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	struct sk_buff *new_skb = rx_skb_info->cur_skb;
	int ret = 0;

	if (rx_skb_info->pit_dp) {
		dev_kfree_skb_any(new_skb);
		goto err;
	}

	dpmf_set_skb_cs_type(rx_skb_info->check_sum, new_skb);
	/* upload to ccmni */
	ret = ccmni_rx_callback(dpmaif_ctrl->ctlb,
				rx_skb_info->cur_chn_idx, new_skb, &rxq->napi);

err:
	rx_skb_info->cur_skb = NULL;
	return ret;
}

static int dpmaif_napi_rx_start(struct dpmaif_rx_queue *rxq,
				unsigned short pit_cnt, int blocking,
				unsigned int budget, int *once_more)
{
	struct dpmaif_cur_rx_skb_info *cur_rx_skb_info;
	struct dpmaifq_normal_pit *pkt_inf_t;
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	unsigned int cur_pit, pit_len;
	unsigned short rx_cnt;
	unsigned int packets_cnt = 0;
	int ret = 0, ret_hw = 0;

	if (!rxq || !rxq->dpmaif_ctrl) {
		pr_err("NULL pointer!\n");
		return -EINVAL;
	}

	pit_len = rxq->pit_size_cnt;
	cur_rx_skb_info = &rxq->rx_data_info;

	dpmaif_ctrl = rxq->dpmaif_ctrl;
	cur_pit = rxq->pit_rd_idx;

	for (rx_cnt = 0; rx_cnt < pit_cnt; rx_cnt++) {
		if (!cur_rx_skb_info->msg_pit_received) {
			if (packets_cnt >= budget)
				break;
		}

		pkt_inf_t = (struct dpmaifq_normal_pit *)rxq->pit_base + cur_pit;

		if (dpmaif_check_pit_seq(rxq, pkt_inf_t)) {
			pr_err_ratelimited("dlq%u checks PIT SEQ fail\n", rxq->index);
			*once_more = 1;
			return packets_cnt;
		}

		if (pkt_inf_t->packet_type == DES_PT_MSG) {
			if (cur_rx_skb_info->msg_pit_received)
				dev_err(dpmaif_ctrl->dev, "dlq%u receive repeat msg_pit err\n",
					rxq->index);
			cur_rx_skb_info->msg_pit_received = true;
			dpmaif_rx_msg_pit(rxq,
					  (struct dpmaifq_msg_pit *)pkt_inf_t,
					  cur_rx_skb_info);
		} else if (pkt_inf_t->packet_type == DES_PT_PD) {
			if (pkt_inf_t->buffer_type != PKT_BUF_FRAG) {
				/* skb->data: add to skb ptr && record ptr.*/
				ret = dpmaif_get_rx_pkt(rxq, blocking,
							pkt_inf_t, cur_rx_skb_info);
			} else if (!cur_rx_skb_info->cur_skb) {
				/* msg+frag pit, no data pkt received. */
				dev_err(dpmaif_ctrl->dev,
					"rxq%d skb_idx < 0 pit/bat = %d, %d; buf: %d; %d, %d\n",
					rxq->index, cur_pit, nomal_pit_bid(pkt_inf_t),
					pkt_inf_t->buffer_type, rx_cnt,
					pit_cnt);
				ret = -EINVAL;
			} else {
				/* skb->frags[]: add to frags[]*/
				ret = dpmaif_get_rx_frag(rxq, blocking,
							 pkt_inf_t, cur_rx_skb_info);
			}

			if (ret < 0) {
				/* move on pit index to skip error data */
				cur_rx_skb_info->err_payload = 1;
				pr_err_ratelimited("rxq%d error payload!\n", rxq->index);
			}

			if (pkt_inf_t->c_bit == 0) {
				/* last one, not msg pit, && data had rx.*/
				if (cur_rx_skb_info->err_payload == 0) {
					ret = dpmaif_napi_rx_skb(rxq, cur_rx_skb_info);
				} else {
					if (cur_rx_skb_info->cur_skb) {
						dev_kfree_skb_any(cur_rx_skb_info->cur_skb);
						cur_rx_skb_info->cur_skb = NULL;
					}
				}
				/* reinit cur_rx_skb_info */
				memset(cur_rx_skb_info, 0x00,
				       sizeof(struct dpmaif_cur_rx_skb_info));
				cur_rx_skb_info->msg_pit_received = false;
				packets_cnt++;
			}
		}

		/* get next pointer to get pkt data */
		cur_pit = ring_buf_get_next_wrdx(pit_len, cur_pit);
		rxq->pit_rd_idx = cur_pit;

		/* notify HW++ */
		rxq->pit_remain_rel_cnt++;
		if (rx_cnt > 0 && (rx_cnt % DPMF_NOTIFY_RELEASE_COUNT) == 0) {
			ret_hw = dpmaifq_rx_notify_hw(rxq, blocking);
			if (ret_hw < 0)
				break;
		}
	}

	/* update to HW */
	if (ret_hw == 0)
		ret_hw = dpmaifq_rx_notify_hw(rxq, blocking);

	if (ret_hw < 0 && ret == 0)
		ret = ret_hw;

	return ret < 0 ? ret : packets_cnt;
}

static int dpmaif_napi_rx_data_collect(struct md_dpmaif_ctrl *hif_ctrl,
				       unsigned char q_num, int budget, int *once_more)
{
	struct dpmaif_rx_queue *rxq = &hif_ctrl->rxq[q_num];
	unsigned int cnt;
	int ret = 0;

	cnt = dpmaifq_poll_rx_pit(rxq);

	if (cnt) {
		ret = dpmaif_napi_rx_start(rxq, cnt, false, budget, once_more);

		if (ret < 0 && ret != -EAGAIN)
			dev_err(hif_ctrl->dev, "dlq%u rx ERR:%d\n", rxq->index, ret);
	}

	return ret;
}

int dpmaif_napi_rx_poll(struct napi_struct *napi, int budget)
{
	struct dpmaif_rx_queue *rxq = container_of(napi, struct dpmaif_rx_queue, napi);
	unsigned short try_cnt = 0;
	bool wait_complete;
	int once_more = 0;
	int work_done = 0;
	int each_budget;
	int rx_cnt;

	atomic_set(&rxq->rx_processing, 1);
	/* Ensure rx_processing is changed to 1 before actually begin RX flow */
	smp_mb();

	/* RPM lock */
	pm_runtime_get(rxq->dpmaif_ctrl->dev);

	mtk_pci_l_resource_lock(rxq->dpmaif_ctrl->mtk_dev);

	/* We check the wait result with one or several "try wait"
	 * for not blocking in interrupt contex
	 */
	do {
		wait_complete = mtk_pci_l_resource_try_wait_complete(rxq->dpmaif_ctrl->mtk_dev);

		if (wait_complete)
			break;

		udelay(DPMF_POLL_RX_TIME);

		try_cnt++;
	} while (try_cnt < DPMF_PCIE_LOCK_RES_TRY_WAIT_CNT); /* try wait up to 200us */

	if (!wait_complete) {
		/* do not use NAPI this time */
		napi_complete_done(napi, work_done);

		/* it's better not to try more times in interrupt context */
		/* we defer it to workqueue */
		queue_work(rxq->worker, &rxq->dpmaif_rxq_work);

		dpmaif_clr_ip_busy_sts(&rxq->dpmaif_ctrl->hif_hw_info);
	} else if (rxq->que_started) {
		while (work_done < budget) {
			each_budget = budget - work_done;
			rx_cnt = dpmaif_napi_rx_data_collect(rxq->dpmaif_ctrl,
							     rxq->index, each_budget,
							     &once_more);
			if (rx_cnt > 0)
				work_done += rx_cnt;
			else
				break;
		}

		if (once_more) {
			napi_gro_flush(napi, false);
			work_done = budget;
			dpmaif_clr_ip_busy_sts(&rxq->dpmaif_ctrl->hif_hw_info);
		} else if (work_done < budget) {
			napi_complete_done(napi, work_done);
			dpmaif_clr_ip_busy_sts(&rxq->dpmaif_ctrl->hif_hw_info);
			dpmaif_hw_dl_dlq_unmask_rx_done_intr(&rxq->dpmaif_ctrl->hif_hw_info,
							     rxq->index);
		} else {
			dpmaif_clr_ip_busy_sts(&rxq->dpmaif_ctrl->hif_hw_info);
		}
	}

	mtk_pci_l_resource_unlock(rxq->dpmaif_ctrl->mtk_dev);

	/* RPM unlock */
	pm_runtime_put(rxq->dpmaif_ctrl->dev);

	atomic_set(&rxq->rx_processing, 0);

	return work_done;
}

/* may be called from workqueue or tasklet context,
 * tasklet with blocking=false
 */
static int dpmaif_rx_data_collect(struct md_dpmaif_ctrl *dpmaif_ctrl,
				  unsigned char q_num, int budget,
				  int blocking)
{
	unsigned long time_limit = jiffies + msecs_to_jiffies(DPMF_WORKQUEUE_TIME_LIMIT);
	struct dpmaif_rx_queue *rxq = &dpmaif_ctrl->rxq[q_num];
	unsigned int cnt, rd_cnt;
	int real_cnt;

	do {
		cnt = dpmaifq_poll_rx_pit(rxq);
		if (cnt) {
			rd_cnt = (cnt > budget && !blocking) ? budget : cnt;
			real_cnt = dpmaif_rx_start(rxq, rd_cnt, blocking, time_limit);

			if (real_cnt < 0) {
				if (real_cnt != -EAGAIN)
					dev_err(dpmaif_ctrl->dev, "dlq%u rx ERROR: %d\n",
						rxq->index, real_cnt);
				return real_cnt;
			} else if (real_cnt < cnt) {
				return -EAGAIN;
			}
		}
	} while (cnt);

	return 0;
}

static void dpmaif_rxq_work(struct work_struct *work)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_rx_queue *rxq;
	int ret;

	rxq = container_of(work, struct dpmaif_rx_queue, dpmaif_rxq_work);
	dpmaif_ctrl = rxq->dpmaif_ctrl;

	atomic_set(&rxq->rx_processing, 1);
	 /* Ensure rx_processing is changed to 1 before actually begin RX flow */
	smp_mb();

	if (!rxq->que_started) {
		atomic_set(&rxq->rx_processing, 0);
		dev_err(dpmaif_ctrl->dev, "RXQ:%d has not been started.\n", rxq->index);
		return;
	}

	/* RPM lock */
	pm_runtime_get(dpmaif_ctrl->dev);

	mtk_pci_l_resource_lock(dpmaif_ctrl->mtk_dev);

	/* we can try blocking wait for lock resource here in process context */
	mtk_pci_l_resource_wait_complete(dpmaif_ctrl->mtk_dev);

	/* blocking=0:
	 * because of data performance, we want to finish the workqueue and
	 * reschedule the NAPI/tasklet mode after time_limit.
	 */
	ret = dpmaif_rx_data_collect(dpmaif_ctrl, rxq->index, rxq->budget, 0);

	if (ret == -EAGAIN) {
		if (dpmaif_ctrl->napi_enable)
			napi_schedule(&rxq->napi);
		else
			tasklet_hi_schedule(&rxq->dpmaif_rxq_task);

		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
	} else if (ret < 0) {
		/* Rx done and empty interrupt will be enabled in workqueue */
		/* maybe we can try one more time */
		queue_work(rxq->worker, &rxq->dpmaif_rxq_work);
		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
	} else {
		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
		dpmaif_hw_dl_dlq_unmask_rx_done_intr(&dpmaif_ctrl->hif_hw_info, rxq->index);
	}

	mtk_pci_l_resource_unlock(dpmaif_ctrl->mtk_dev);

	/* RPM unlock */
	pm_runtime_put(dpmaif_ctrl->dev);

	atomic_set(&rxq->rx_processing, 0);
}

static void dpmaif_rxq_tasklet(unsigned long data)
{
	struct dpmaif_rx_queue *rxq = (struct dpmaif_rx_queue *)data;
	struct md_dpmaif_ctrl *dpmaif_ctrl = rxq->dpmaif_ctrl;
	unsigned short try_cnt = 0;
	bool wait_complete;
	int ret;

	atomic_set(&rxq->rx_processing, 1);
	 /* Ensure rx_processing is changed to 1 before actually begin RX flow */
	smp_mb();

	if (!rxq->que_started) {
		atomic_set(&rxq->rx_processing, 0);
		dev_err(dpmaif_ctrl->dev, "tasklet RXQ:%d has not been started.\n", rxq->index);
		return;
	}

	/* RPM lock */
	pm_runtime_get(dpmaif_ctrl->dev);

	mtk_pci_l_resource_lock(dpmaif_ctrl->mtk_dev);

	/* We check the wait result with one or several "try wait"
	 * for not blocking in interrupt contex
	 */
	do {
		wait_complete = mtk_pci_l_resource_try_wait_complete(dpmaif_ctrl->mtk_dev);

		if (wait_complete)
			break;

		udelay(DPMF_POLL_RX_TIME);

		try_cnt++;
	} while (try_cnt < DPMF_PCIE_LOCK_RES_TRY_WAIT_CNT); /* try wait up to 200us */

	if (!wait_complete) {
		/* it's better not to try more times in interrupt context */
		/* we defer it to workqueue */
		queue_work(rxq->worker,
			   &rxq->dpmaif_rxq_work);

		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
	} else {
		ret = dpmaif_rx_data_collect(dpmaif_ctrl, rxq->index, rxq->budget, 0);

		if (ret == -EAGAIN) {
			tasklet_hi_schedule(&rxq->dpmaif_rxq_task);
			dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
		} else if (ret < 0) {
			/* Rx done and empty interrupt will be enabled in workqueue */
			queue_work(rxq->worker, &rxq->dpmaif_rxq_work);
			dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
		} else {
			dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
			dpmaif_hw_dl_dlq_unmask_rx_done_intr(&dpmaif_ctrl->hif_hw_info,
							     rxq->index);
		}
	}

	mtk_pci_l_resource_unlock(dpmaif_ctrl->mtk_dev);

	/* RPM unlock */
	pm_runtime_put(dpmaif_ctrl->dev);

	atomic_set(&rxq->rx_processing, 0);
}

void dpmaif_irq_rx_done(struct md_dpmaif_ctrl *dpmaif_ctrl,
			unsigned int que_mask)
{
	struct dpmaif_rx_queue *rxq;
	int qno;

	/* get the queue id */
	qno = ffs(que_mask) - 1;
	if (qno < 0 || qno > DPMAIF_RXQ_NUM - 1) {
		dev_err(dpmaif_ctrl->dev, "rxq err, qno:%u\n", qno);
		return;
	}

	rxq = &dpmaif_ctrl->rxq[qno];

	if (dpmaif_ctrl->napi_enable)
		napi_schedule(&rxq->napi);
	else
		tasklet_hi_schedule(&dpmaif_ctrl->rxq[qno].dpmaif_rxq_task);

	dpmf_get_cur_cpu(&dpmaif_ctrl->rxq[qno].cur_irq_cpu);
}

static void dpmaif_base_rel(struct md_dpmaif_ctrl *dpmaif_ctrl,
			    struct dpmaif_bat_request *bat_req)
{
	if (bat_req->bat_base)
		dma_free_coherent(dpmaif_ctrl->dev,
				  (bat_req->bat_size_cnt * sizeof(struct dpmaif_bat)),
				  bat_req->bat_base,
				  bat_req->bat_bus_addr);
}

int dpmaif_bat_init(struct md_dpmaif_ctrl *dpmaif_ctrl,
		    struct dpmaif_bat_request *bat_req,
		    enum bat_type buf_type)
{
	int sw_buf_size = (buf_type == FRAG_BUF) ?
		sizeof(struct dpmaif_bat_page) :
		sizeof(struct dpmaif_bat_skb);

	bat_req->bat_size_cnt = (buf_type == FRAG_BUF) ?
		DPMAIF_DL_FRG_ENTRY_SIZE :
		DPMAIF_DL_BAT_ENTRY_SIZE;

	bat_req->skb_pkt_cnt = bat_req->bat_size_cnt;
	bat_req->pkt_buf_sz = (buf_type == FRAG_BUF) ?
		DPMAIF_BUF_FRAG_SIZE :
		DPMAIF_BUF_PKT_SIZE;

	bat_req->bat_wr_idx = 0;
	bat_req->bat_rel_rd_idx = 0;

	/* alloc buffer for HW && AP SW */
	 bat_req->bat_base = dma_alloc_coherent(dpmaif_ctrl->dev,
						(bat_req->bat_size_cnt *
						 sizeof(struct dpmaif_bat)),
						&bat_req->bat_bus_addr, GFP_KERNEL);

	if (!bat_req->bat_base)
		return -ENOMEM;

	/* alloc buffer for AP SW to record skb information */
	bat_req->bat_skb_ptr =
		kzalloc((bat_req->skb_pkt_cnt * sw_buf_size), GFP_KERNEL);

	if (!bat_req->bat_skb_ptr)
		goto err_base;

	memset(bat_req->bat_base, 0, (bat_req->bat_size_cnt * sizeof(struct dpmaif_bat)));

	/* alloc buffer for BAT mask */
	bat_req->bat_mask = kcalloc(bat_req->bat_size_cnt,
				    sizeof(unsigned char), GFP_KERNEL);
	if (!bat_req->bat_mask)
		goto err_all;

	atomic_set(&bat_req->refcnt, 0);

	return 0;

err_all:
	kfree(bat_req->bat_skb_ptr);
err_base:
	dpmaif_base_rel(dpmaif_ctrl, bat_req);
	return -ENOMEM;
}

static void dpmaif_bat_rel(struct md_dpmaif_ctrl *dpmaif_ctrl,
			   struct dpmaif_bat_request *bat_req)
{
	struct dpmaif_bat_skb *cur_skb;
	unsigned int index;

	if (!bat_req || !dpmaif_ctrl)
		return;

	if (!atomic_dec_and_test(&bat_req->refcnt))
		return;

	kfree(bat_req->bat_mask);
	bat_req->bat_mask = NULL;

	if (bat_req->bat_skb_ptr) {
		for (index = 0; index < bat_req->bat_size_cnt; index++) {
			cur_skb = ((struct dpmaif_bat_skb *)
				  bat_req->bat_skb_ptr + index);

			if (cur_skb->skb) {
				dma_unmap_single(dpmaif_ctrl->dev,
						 cur_skb->data_bus_addr,
						 cur_skb->data_len,
						 DMA_FROM_DEVICE);
				kfree_skb(cur_skb->skb);
				cur_skb->skb = NULL;
			}
		}

		kfree(bat_req->bat_skb_ptr);
		bat_req->bat_skb_ptr = NULL;
	}

	dpmaif_base_rel(dpmaif_ctrl, bat_req);
}

static void dpmaif_frag_bat_rel(struct md_dpmaif_ctrl *dpmaif_ctrl,
				struct dpmaif_bat_request *bat_req)
{
	struct dpmaif_bat_page *cur_page;
	unsigned int index;
	struct page *page;

	if (!bat_req || !dpmaif_ctrl)
		return;

	if (!atomic_dec_and_test(&bat_req->refcnt))
		return;

	kfree(bat_req->bat_mask);
	bat_req->bat_mask = NULL;

	if (bat_req->bat_skb_ptr) {
		for (index = 0; index < bat_req->bat_size_cnt; index++) {
			cur_page = ((struct dpmaif_bat_page *)
				bat_req->bat_skb_ptr + index);
			page = cur_page->page;
			if (page) {
				/* rx unmapping */
				dma_unmap_page(dpmaif_ctrl->dev,
					       cur_page->data_bus_addr,
					       cur_page->data_len,
					       DMA_FROM_DEVICE);
				put_page(page);
				cur_page->page = NULL;
			}
		}
		kfree(bat_req->bat_skb_ptr);
		bat_req->bat_skb_ptr = NULL;
	}

	dpmaif_base_rel(dpmaif_ctrl, bat_req);
}

static int dpmaif_rx_buf_init(struct dpmaif_rx_queue *rxq)
{
	/* PIT buffer init */
	rxq->pit_size_cnt = DPMAIF_DL_PIT_ENTRY_SIZE;
	rxq->pit_rd_idx = 0;
	rxq->pit_wr_idx = 0;
	rxq->pit_rel_rd_idx = 0;
	rxq->expect_pit_seq = 0;
	rxq->pit_remain_rel_cnt = 0;

	rxq->cur_irq_cpu = DPMF_CUR_CPU_INVALID;
	rxq->cur_rx_thrd_cpu = DPMF_CUR_CPU_INVALID;
	rxq->pit_seq_fail_cnt = 0;

	memset(&rxq->rx_data_info, 0x00, sizeof(struct dpmaif_cur_rx_skb_info));
	rxq->rx_data_info.msg_pit_received = false;

	/* PIT init */
	rxq->pit_base = dma_alloc_coherent(rxq->dpmaif_ctrl->dev,
					   rxq->pit_size_cnt * sizeof(struct dpmaifq_normal_pit),
					   &rxq->pit_bus_addr, GFP_KERNEL);

	if (!rxq->pit_base) {
		dev_err(rxq->dpmaif_ctrl->dev, "pit request fail\n");
		return -ENOMEM;
	}

	memset(rxq->pit_base, 0, rxq->pit_size_cnt * sizeof(struct dpmaifq_normal_pit));

	/* BAT init */
	/* RXQ BAT table init */
	rxq->bat_req = &rxq->dpmaif_ctrl->bat_req;
	atomic_inc(&rxq->bat_req->refcnt);

	/* RXQ Frag BAT table init */
	rxq->bat_frag = &rxq->dpmaif_ctrl->bat_frag;
	atomic_inc(&rxq->bat_frag->refcnt);

	return 0;
}

static void dpmaif_rx_buf_rel(struct dpmaif_rx_queue *rxq)
{
	if (!rxq || !rxq->dpmaif_ctrl)
		return;

	dpmaif_bat_rel(rxq->dpmaif_ctrl, rxq->bat_req);
	dpmaif_frag_bat_rel(rxq->dpmaif_ctrl, rxq->bat_frag);

	if (rxq->pit_base)
		dma_free_coherent(rxq->dpmaif_ctrl->dev,
				  rxq->pit_size_cnt * sizeof(struct dpmaifq_normal_pit),
				  rxq->pit_base,
				  rxq->pit_bus_addr);
}

int dpmaif_rxq_init(struct dpmaif_rx_queue *queue)
{
	int ret;

	/* rxq data buffer(pit, bat...) init */
	ret = dpmaif_rx_buf_init(queue);
	if (ret < 0) {
		dev_err(&queue->dpmaif_ctrl->mtk_dev->pdev->dev, "RX buffer init fail, %d\n", ret);
		return ret;
	}

	INIT_WORK(&queue->dpmaif_rxq_work, dpmaif_rxq_work);
	queue->worker = alloc_workqueue("dpmaif_rx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI,
					1, queue->index);

	/* init none napi resources */
	if (!queue->dpmaif_ctrl->napi_enable) {
		tasklet_init(&queue->dpmaif_rxq_task, dpmaif_rxq_tasklet,
			     (unsigned long)queue);

		/* rx push thread and skb list init */
		init_waitqueue_head(&queue->rx_wq);
		ccci_skb_queue_init(&queue->skb_list, queue->bat_req->skb_pkt_cnt,
				    SKB_RX_LIST_MAX_LEN, 0);
		queue->rx_thread = kthread_run(dpmaif_net_rx_push_thread,
					       queue, "dpmaif_rx%d_push", queue->index);
	}

	return 0;
}

static void ccci_skb_queue_rel(struct ccci_skb_queue *queue)
{
	struct sk_buff *skb;

	if (!queue)
		return;

	while ((skb = skb_dequeue(&queue->skb_list)) != NULL)
		kfree_skb(skb);
}

void dpmaif_rxq_rel(struct dpmaif_rx_queue *queue)
{
	if (!queue)
		return;

	if (queue->worker) {
		flush_workqueue(queue->worker);
		destroy_workqueue(queue->worker);
	}

	if (!queue->dpmaif_ctrl->napi_enable) {
		if (queue->rx_thread)
			kthread_stop(queue->rx_thread);

		tasklet_disable(&queue->dpmaif_rxq_task);
		tasklet_kill(&queue->dpmaif_rxq_task);

		ccci_skb_queue_rel(&queue->skb_list);
	}

	dpmaif_rx_buf_rel(queue);
}

#define DPMF_MAP_SKB_QUE_EMPTY(skb_list) list_empty(&(skb_list).head)

void dpmaif_skb_queue_rel(struct md_dpmaif_ctrl *dpmaif_ctrl,
			  unsigned int index)
{
	struct dpmaif_skb_info *skb_info;
	struct dpmaif_skb_queue *queue;
	struct list_head *entry;

	if (!dpmaif_ctrl || index >= DPMA_SKB_QUEUE_CNT)
		return;

	queue = &dpmaif_ctrl->skb_pool.queue[index];
	while (!DPMF_MAP_SKB_QUE_EMPTY(queue->skb_list)) {
		entry = dpmaif_map_skb_deq(&queue->skb_list);
		if (entry) {
			skb_info = GET_SKB_BY_ENTRY(entry);
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
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_skb_info *skb_info;
	struct dpmaif_skb_queue *queue;
	struct dpmaif_skb_pool *pool;
	unsigned int max_cnt;
	struct sk_buff *skb;
	unsigned long flags;
	unsigned int index;
	unsigned int size;
	unsigned int qlen;
	unsigned int cnt;

	if (!work)
		return;

	pool = container_of(work, struct dpmaif_skb_pool, reload_work);
	dpmaif_ctrl = container_of(pool, struct md_dpmaif_ctrl, skb_pool);

	for (index = 0; index < DPMA_SKB_QUEUE_CNT; index++) {
		queue = &pool->queue[index];

		spin_lock_irqsave(&queue->skb_list.lock, flags);
		size = queue->size;
		max_cnt = queue->max_len;
		qlen = queue->skb_list.qlen;
		spin_unlock_irqrestore(&queue->skb_list.lock, flags);

		if (qlen >= max_cnt * DPMA_RELOAD_TH_1 / DPMA_RELOAD_TH_2)
			continue;
		cnt = max_cnt - qlen;
		while (cnt--) {
			skb = __dev_alloc_skb(size, GFP_KERNEL);
			if (!skb)
				continue;
			skb_info = dpmaif_skb_dma_map(dpmaif_ctrl, skb);
			if (skb_info) {
				spin_lock_irqsave(&queue->skb_list.lock, flags);
				list_add_tail(&skb_info->entry, &queue->skb_list.head);
				queue->skb_list.qlen++;
				spin_unlock_irqrestore(&queue->skb_list.lock, flags);
			} else {
				dev_kfree_skb_any(skb);
			}
		}
	}
}

static int dpmaif_skb_queue_init_struct(struct md_dpmaif_ctrl *dpmaif_ctrl,
					unsigned int index)
{
	struct dpmaif_skb_queue *queue;
	unsigned int max_cnt;
	unsigned int size;

	queue = &dpmaif_ctrl->skb_pool.queue[index];
	size = DPMA_SKB_SIZE_MAX / BIT(index);
	max_cnt = DPMA_SKB_QUEUE_SIZE / DPMA_SKB_SIZE_TRUE(size);

	if (size < DPMA_SKB_TRUE_SIZE_MIN)
		return -EINVAL;

	INIT_LIST_HEAD(&queue->skb_list.head);
	spin_lock_init(&queue->skb_list.lock);
	queue->skb_list.qlen = 0;
	queue->size = size;
	queue->max_len = max_cnt;
	queue->busy_cnt = 0;

	return 0;
}

int dpmaif_skb_pool_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_skb_pool *pool;
	int index;
	int ret;

	pool = &dpmaif_ctrl->skb_pool;
	pool->queue_cnt = DPMA_SKB_QUEUE_CNT;

	/* init the skb queue structure */
	for (index = 0; index < pool->queue_cnt; index++) {
		ret = dpmaif_skb_queue_init_struct(dpmaif_ctrl, index);
		if (ret) {
			dev_err(dpmaif_ctrl->dev, "Init skb_queue:%d fail\n", index);
			return ret;
		}
	}

	/* init pool reload work */
	pool->pool_reload_work_queue = alloc_workqueue("dpmaif_skb_pool_reload_work",
						       WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (!pool->pool_reload_work_queue) {
		dev_err(dpmaif_ctrl->dev, "Create the pool_reload_work_queue fail\n");
		return -ENOMEM;
	}

	INIT_WORK(&pool->reload_work, skb_reload_work);

	/* schedule the skb pool reload workqueue */
	queue_work_on(dpmaif_find_reload_cpu(dpmaif_ctrl),
		      pool->pool_reload_work_queue, &pool->reload_work);

	return 0;
}

static void dpmaif_bat_rel_work(struct work_struct *work)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_rx_queue *rxq;
	int blocking = 1;

	dpmaif_ctrl = container_of(work, struct md_dpmaif_ctrl, bat_rel_work);

	pm_runtime_get(dpmaif_ctrl->dev);

	mtk_pci_l_resource_lock(dpmaif_ctrl->mtk_dev);

	/* ALL RXQ use one bat table, so choose DPF_RX_QNO_DFT */
	rxq = &dpmaif_ctrl->rxq[DPF_RX_QNO_DFT];

	mtk_pci_l_resource_wait_complete(dpmaif_ctrl->mtk_dev);

	/* normal bat release and add */
	dpmaif_dl_pkt_bat_rel_and_add(rxq, blocking);

	/* frag bat release and add */
	dpmaif_dl_frag_bat_rel_and_add(rxq, blocking);

	mtk_pci_l_resource_unlock(dpmaif_ctrl->mtk_dev);

	pm_runtime_put(dpmaif_ctrl->dev);
}

int dpmaif_bat_rel_work_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_ctrl->bat_rel_work_queue =
		alloc_workqueue("dpmaif_bat_rel_work_queue", WQ_MEM_RECLAIM, 1);

	if (!dpmaif_ctrl->bat_rel_work_queue)
		return -ENOMEM;

	INIT_WORK(&dpmaif_ctrl->bat_rel_work, dpmaif_bat_rel_work);

	return 0;
}

void dpmaif_bat_rel_work_rel(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	flush_work(&dpmaif_ctrl->bat_rel_work);

	if (dpmaif_ctrl->bat_rel_work_queue) {
		flush_workqueue(dpmaif_ctrl->bat_rel_work_queue);
		destroy_workqueue(dpmaif_ctrl->bat_rel_work_queue);
		dpmaif_ctrl->bat_rel_work_queue = NULL;
	}
}

void dpmaif_suspend_rx_sw_stop(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rxq;
	unsigned int i, count;

	/* Disable DL/RX SW active */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rxq = &dpmaif_ctrl->rxq[i];

		flush_work(&rxq->dpmaif_rxq_work);

		/* for BAT add_cnt register */
		count = 0;
		do {
			/* retry handler */
			if (++count >= DPMA_CHECK_REG_TIMEOUT) {
				dev_err(dpmaif_ctrl->dev, "Stop Rx sw failed, %d\n", count);
				break;
			}
		} while (atomic_read(&rxq->rx_processing));

		/* Ensure RX processing has already been stopped before we toggle
		 * the rxq->que_started to false during RX suspend flow
		 */
		smp_mb();
		rxq->que_started = false;
	}
}

static void dpmaif_stop_rxq(struct dpmaif_rx_queue *rxq)
{
	int cnt, j = 0;

	flush_work(&rxq->dpmaif_rxq_work);

	/* reset sw */
	rxq->que_started = false;
	do {
		/* Disable HW arb and check idle */
		cnt = ring_buf_readable(rxq->pit_size_cnt,
					rxq->pit_rd_idx, rxq->pit_wr_idx);
		/* retry handler */
		if (++j >= DPMA_CHECK_REG_TIMEOUT) {
			dev_err(rxq->dpmaif_ctrl->dev, "stop Rx sw failed, %d\n", cnt);
			break;
		}
	} while (cnt);

	memset(rxq->pit_base, 0,
	       (rxq->pit_size_cnt * sizeof(struct dpmaifq_normal_pit)));
	memset(rxq->bat_req->bat_base, 0,
	       (rxq->bat_req->bat_size_cnt * sizeof(struct dpmaif_bat)));
	memset(rxq->bat_req->bat_mask, 0,
	       (rxq->bat_req->bat_size_cnt * sizeof(unsigned char)));

	memset(&rxq->rx_data_info, 0x00, sizeof(struct dpmaif_cur_rx_skb_info));
	rxq->rx_data_info.msg_pit_received = false;

	rxq->pit_rd_idx = 0;
	rxq->pit_wr_idx = 0;
	rxq->pit_rel_rd_idx = 0;

	rxq->expect_pit_seq = 0;
	rxq->pit_remain_rel_cnt = 0;
	rxq->pit_seq_fail_cnt = 0;

	rxq->bat_req->bat_rel_rd_idx = 0;
	rxq->bat_req->bat_wr_idx = 0;

	rxq->bat_frag->bat_rel_rd_idx = 0;
	rxq->bat_frag->bat_wr_idx = 0;
}

void dpmaif_stop_rx_sw(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	int i;

	for (i = 0; i < DPMAIF_RXQ_NUM; i++)
		dpmaif_stop_rxq(&dpmaif_ctrl->rxq[i]);
}

