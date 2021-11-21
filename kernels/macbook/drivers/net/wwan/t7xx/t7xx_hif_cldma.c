// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/pm_runtime.h>

#include "t7xx_buffer_manager.h"
#include "t7xx_common.h"
#include "t7xx_dev_node.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_isr.h"
#include "t7xx_port_proxy.h"
#include "t7xx_port.h"
#include "t7xx_pci_ats.h"

#define MAX_TX_BUDGET			16
#define MAX_RX_BUDGET			16

static struct md_cd_ctrl *cldma_md_ctrl[CLDMA_NUM];

static DEFINE_MUTEX(ctl_cfg_mutex); /* protects cldma late init config */

static enum queue_type rxq_type[CLDMA_RXQ_NUM] = {SHARED_Q, SHARED_Q, SHARED_Q, SHARED_Q,
						  SHARED_Q, SHARED_Q, SHARED_Q, SHARED_Q};
static enum queue_type txq_type[CLDMA_TXQ_NUM] = {SHARED_Q, SHARED_Q, SHARED_Q, SHARED_Q,
						  SHARED_Q, SHARED_Q, SHARED_Q, SHARED_Q};
static int rxq_buff_size[CLDMA_RXQ_NUM] = {SKB_4K, SKB_4K, SKB_4K, SKB_4K,
					   SKB_4K, SKB_4K, SKB_4K, SKB_4K};
static int txq_buff_size[CLDMA_TXQ_NUM] = {SKB_4K, SKB_4K, SKB_4K, SKB_4K,
					   SKB_4K, SKB_4K, SKB_4K, SKB_4K};
static int rxq_buff_num[CLDMA_RXQ_NUM] = {MAX_RX_BUDGET, MAX_RX_BUDGET, MAX_RX_BUDGET,
					  MAX_RX_BUDGET, MAX_RX_BUDGET, MAX_RX_BUDGET,
					  MAX_RX_BUDGET, MAX_RX_BUDGET };
static int txq_buff_num[CLDMA_TXQ_NUM] = {MAX_TX_BUDGET, MAX_TX_BUDGET, MAX_TX_BUDGET,
					  MAX_TX_BUDGET, MAX_TX_BUDGET, MAX_TX_BUDGET,
					  MAX_TX_BUDGET, MAX_TX_BUDGET };

static int rxq2ring[CLDMA_RXQ_NUM] = {0, 1, 2, 3, 4, 5, 6, 7};
static int txq2ring[CLDMA_TXQ_NUM] = {0, 1, 2, 3, 4, 5, 6, 7};
static int ring2rxq[CLDMA_RXQ_NUM] = {0, 1, 2, 3, 4, 5, 6, 7};
static int ring2txq[CLDMA_TXQ_NUM] = {0, 1, 2, 3, 4, 5, 6, 7};

static struct md_cd_ctrl *md_cd_get(enum cldma_id hif_id)
{
	if (hif_id >= CLDMA_NUM) {
		pr_err("hif_id = %u\n", hif_id);
		return NULL;
	}

	return cldma_md_ctrl[hif_id];
}

static inline void md_cd_set(enum cldma_id hif_id, struct md_cd_ctrl *md_ctrl)
{
	cldma_md_ctrl[hif_id] = md_ctrl;
}

static inline void md_cd_queue_struct_reset(struct md_cd_queue *queue,
					    enum cldma_id hif_id, enum direction dir,
					    unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->hif_id = hif_id;
	queue->tr_ring = NULL;
	queue->tr_done = NULL;
	queue->tx_xmit = NULL;
	queue->busy_count = 0;
}

static inline void md_cd_queue_struct_init(struct md_cd_queue *queue,
					   enum cldma_id hif_id, enum direction dir,
					   unsigned char index)
{
	md_cd_queue_struct_reset(queue, hif_id, dir, index);
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->ring_lock);
}

static inline void cldma_tgpd_set_data_ptr(struct cldma_tgpd *tgpd,
					   dma_addr_t data_ptr)
{
	tgpd->data_buff_bd_ptr_h = (u32)(data_ptr >> 32);
	tgpd->data_buff_bd_ptr_l = (u32)data_ptr;
}

static inline void cldma_tgpd_set_next_ptr(struct cldma_tgpd *tgpd,
					   dma_addr_t next_ptr)
{
	tgpd->next_gpd_ptr_h = (u32)(next_ptr >> 32);
	tgpd->next_gpd_ptr_l = (u32)next_ptr;
}

static inline void cldma_rgpd_set_data_ptr(struct cldma_rgpd *rgpd,
					   dma_addr_t data_ptr)
{
	rgpd->data_buff_bd_ptr_h = (u32)(data_ptr >> 32);
	rgpd->data_buff_bd_ptr_l = (u32)data_ptr;
}

static inline void cldma_rgpd_set_next_ptr(struct cldma_rgpd *rgpd,
					   dma_addr_t next_ptr)
{
	rgpd->next_gpd_ptr_h = (u32)(next_ptr >> 32);
	rgpd->next_gpd_ptr_l = (u32)next_ptr;
}

static inline void cldma_tbd_set_next_ptr(struct cldma_tbd *tbd,
					  dma_addr_t next_ptr)
{
	tbd->next_bd_ptr_h = (u32)(next_ptr >> 32);
	tbd->next_bd_ptr_l = (u32)next_ptr;
}

static struct cldma_request *cldma_ring_step_forward(struct cldma_ring *ring,
						     struct cldma_request *req)
{
	struct cldma_request *next_req;

	if (req->entry.next == &ring->gpd_ring)
		next_req = list_first_entry(&ring->gpd_ring,
					    struct cldma_request, entry);
	else
		next_req = list_entry(req->entry.next,
				      struct cldma_request, entry);
	return next_req;
}

static struct cldma_request *cldma_ring_step_backward(struct cldma_ring *ring,
						      struct cldma_request *req)
{
	struct cldma_request *prev_req;

	if (req->entry.prev == &ring->gpd_ring)
		prev_req = list_last_entry(&ring->gpd_ring,
					   struct cldma_request, entry);
	else
		prev_req = list_entry(req->entry.prev,
				      struct cldma_request, entry);
	return prev_req;
}

/* may be called from workqueue or NAPI or tasklet (queue0) context,
 * only NAPI and tasklet with blocking=false
 */
static int cldma_gpd_rx_collect(struct md_cd_queue *queue, int budget, int blocking)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(queue->hif_id);
	struct cldma_hw_info *hw_info;
	struct cldma_request *req;
	struct cldma_rgpd *rgpd;
	struct sk_buff *new_skb;
	struct sk_buff *skb;
	int ret = 0, count = 0;
	unsigned char hwo_polling_count = 0;
	unsigned long long gpd_addr_h_inreg;
	unsigned long long gpd_addr_l_inreg;
	unsigned int L2RISAR0;
	unsigned long flags;
	int over_budget = 0;

	if (!md_ctrl) {
		pr_err("NULL pointer: md_ctrl %p\n", md_ctrl);
		return -EINVAL;
	}
	hw_info = &md_ctrl->hw_info;
again:
	while (1) {
		req = queue->tr_done;
		if (!req) {
			dev_err(md_ctrl->dev, "RXQ was released\n");
			return 0;
		}

		rgpd = (struct cldma_rgpd *)req->gpd;
		if ((rgpd->gpd_flags & GPD_FLAGS_HWO) || !req->skb) {
			/* check addr */
			gpd_addr_h_inreg = ioread32(hw_info->ap_ao_base +
						    REG_CLDMA_SO_CURRENT_ADDRH_0 +
						    queue->index * 8);
			gpd_addr_l_inreg = ioread32(hw_info->ap_ao_base +
						    REG_CLDMA_SO_CURRENT_ADDRL_0 +
						    queue->index * 8);

			if (gpd_addr_l_inreg == 0xFFFFFFFF && gpd_addr_h_inreg == 0xFFFFFFFF) {
				dev_err(md_ctrl->dev, "PCIe Link disconnect!\n");
				return 0;
			}

			if ((unsigned long long)queue->tr_done->gpd_addr !=
				((gpd_addr_h_inreg << 32) + gpd_addr_l_inreg) &&
				(hwo_polling_count++ < 100)) {
				udelay(1);
				continue;
			}
			hwo_polling_count = 0;
			break;
		}
		hwo_polling_count = 0;
		skb = req->skb;

		if (req->mapped_buff) {
			dma_unmap_single(md_ctrl->dev, req->mapped_buff,
					 skb_data_size(skb), DMA_FROM_DEVICE);
			req->mapped_buff = 0;
		}

		/* init skb struct */
		skb->len = 0;
		skb_reset_tail_pointer(skb);
		skb_put(skb, rgpd->data_buff_len);
		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1)
			dev_notice(md_ctrl->dev, "CLDMA_MD wakeup source:%d\n", queue->index);

		/* upload skb */
		if (queue->q_type == SHARED_Q)
			ret = proxy_dispatch_recv_skb(md_ctrl->hif_id, skb);
		else if (queue->q_type == DEDICATED_Q)
			ret = port_recv_skb_from_dedicatedQ(md_ctrl->hif_id,
							    queue->index, skb);
		new_skb = NULL;
		if (ret >= 0 || ret == -EDROPPACKET)
			new_skb = ccci_alloc_skb(queue->tr_ring->pkt_size, 1, blocking);
		if (!new_skb) {
			msleep(20);
			return -EAGAIN;
		}

		/* mark cldma_request as available */
		req->skb = NULL;
		cldma_rgpd_set_data_ptr(rgpd, 0);
		/* step forward */
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);

		req = queue->rx_refill;
		rgpd = (struct cldma_rgpd *)req->gpd;
		req->mapped_buff = dma_map_single(md_ctrl->dev, new_skb->data,
						  skb_data_size(new_skb), DMA_FROM_DEVICE);
		if (dma_mapping_error(md_ctrl->dev, req->mapped_buff)) {
			dev_err(md_ctrl->dev, "error dma mapping\n");
			req->mapped_buff = 0;
			ccci_free_skb(new_skb);
			return -EFAULT;
		}

		cldma_rgpd_set_data_ptr(rgpd, req->mapped_buff);
		rgpd->data_buff_len = 0;
		/* set HWO, no need to hold ring_lock as no racer */
		rgpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		/* mark cldma_request as available */
		req->skb = new_skb;
		queue->rx_refill = cldma_ring_step_forward(queue->tr_ring, req);

		/* check budget and if need rescheduled
		 * over_budget=1 means they can be scheduled again
		 */
		if (++count >= budget && need_resched()) {
			over_budget = 1;
			break;
		}
	}

	ret = 0;
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->rxq_active & BIT(queue->index)) {
		hw_info = &md_ctrl->hw_info;
		/* resume Rx queue */
		if (!cldma_hw_queue_status(hw_info, queue->index, true))
			cldma_hw_resume_queue(hw_info, queue->index, true);
		/* greedy mode */
		L2RISAR0 = cldma_hw_int_status(hw_info, BIT(queue->index), true);

		if (over_budget) {
			/* need be scheduled again, avoid the soft lockup */
			if (L2RISAR0) {
				cldma_hw_rx_done(hw_info, L2RISAR0);
				ret = -EAGAIN;
			} else {
				ret = 0;
			}
		} else {
			if (L2RISAR0) {
				/* clear IP busy register wake up cpu case */
				cldma_hw_rx_done(hw_info, L2RISAR0);
				spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
				goto again;
			} else {
				ret = 0;
			}
		}
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	return ret;
}

static void cldma_rx_done(struct work_struct *work)
{
	struct md_cd_queue *queue;
	struct md_cd_ctrl *md_ctrl;
	int ret;

	queue = container_of(work, struct md_cd_queue, cldma_rx_work);
	if (!queue->md) {
		pr_err("modem is NULL!\n");
		return;
	}

	md_ctrl = md_cd_get(queue->hif_id);

	if (!md_ctrl) {
		pr_err("md_ctrl not found\n");
		return;
	}
	ret = queue->tr_ring->handle_rx_done(queue, queue->budget, 1);
	if (ret) {
		if (md_ctrl->rxq_active & BIT(queue->index)) {
			queue_work(queue->worker, &queue->cldma_rx_work);
			return;
		}
	}
	/* clear IP busy register wake up cpu case */
	cldma_clear_ip_busy(&md_ctrl->hw_info);
	/* enable RX_DONE && EMPTY interrupt */
	cldma_hw_dismask_txrxirq(&md_ctrl->hw_info, queue->index, true);
	cldma_hw_dismask_eqirq(&md_ctrl->hw_info, queue->index, true);
	pm_runtime_put(md_ctrl->dev);
}

/* this function may be called from both workqueue and ISR (timer) */
static int cldma_gpd_tx_collect(struct md_cd_queue *queue, int budget, int blocking)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(queue->hif_id);
	struct cldma_request *req;
	struct sk_buff *skb_free;
	struct cldma_tgpd *tgpd;
	unsigned int dma_len;
	unsigned long flags;
	dma_addr_t dma_free;
	int count = 0;

	if (!md_ctrl) {
		pr_err("md_ctrl is NULL\n");
		return -EINVAL;
	}
	while (1) {
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = queue->tr_done;
		if (!req) {
			dev_err(md_ctrl->dev, "TXQ was released\n");
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}
		tgpd = (struct cldma_tgpd *)req->gpd;
		if ((tgpd->gpd_flags & GPD_FLAGS_HWO) || !req->skb) {
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}
		/* restore IOC setting */
		if (req->ioc_override & GPD_FLAGS_IOC) {
			if (req->ioc_override & GPD_FLAGS_HWO)
				tgpd->gpd_flags |= GPD_FLAGS_IOC;
			else
				tgpd->gpd_flags &= ~GPD_FLAGS_IOC;
			dev_notice(md_ctrl->dev,
				   "qno%u, req->ioc_override=0x%x,tgpd->gpd_flags=0x%x\n",
				   queue->index, req->ioc_override, tgpd->gpd_flags);
		}
		tgpd->debug_tx = 2;
		queue->budget++;
		/* save skb reference */
		dma_free = req->mapped_buff;
		dma_len = tgpd->data_buff_len;
		skb_free = req->skb;
		/* mark cldma_request as available */
		req->skb = NULL;
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		count++;
		dma_unmap_single(md_ctrl->dev,
				 dma_free, dma_len, DMA_TO_DEVICE);
		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1)
			dev_notice(md_ctrl->dev, "CLDMA_AP wakeup source:%d\n", queue->index);

		ccci_free_skb(skb_free);
	}
	if (count)
		wake_up_nr(&queue->req_wq, count);
	return count;
}

static void cldma_tx_queue_empty_handler(struct md_cd_queue *queue)
{
	struct cldma_hw_info *hw_info;
	struct cldma_request *req;
	struct md_cd_ctrl *md_ctrl;
	struct cldma_tgpd *tgpd;
	dma_addr_t ul_curr_addr;
	unsigned long flags;
	u32 ul_curr_addr_l;
	u32 ul_curr_addr_h;
	int pending_gpd = 0;

	md_ctrl = md_cd_get(queue->hif_id);
	if (!md_ctrl)	{
		pr_err("md_ctrl is NULL\n");
		return;
	}
	hw_info = &md_ctrl->hw_info;
	if (!(md_ctrl->txq_active & BIT(queue->index)))
		return;

	/* check if there is any pending TGPD with HWO=1 */
	spin_lock_irqsave(&queue->ring_lock, flags);
	req = cldma_ring_step_backward(queue->tr_ring, queue->tx_xmit);
	tgpd = (struct cldma_tgpd *)req->gpd;
	if ((tgpd->gpd_flags & GPD_FLAGS_HWO) && req->skb)
		pending_gpd = 1;
	spin_unlock_irqrestore(&queue->ring_lock, flags);

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (pending_gpd) {
		/* Check current processing TGPD */
		ul_curr_addr_l = ioread32(hw_info->ap_pdn_base +
					  REG_CLDMA_UL_CURRENT_ADDRL_0 +
					  queue->index * 8);
		ul_curr_addr_h = ioread32(hw_info->ap_pdn_base +
					  REG_CLDMA_UL_CURRENT_ADDRH_0 +
					  queue->index * 8);
		ul_curr_addr = ((dma_addr_t)(ul_curr_addr_h) << 32) |
				(dma_addr_t)ul_curr_addr_l;
		if (req->gpd_addr != ul_curr_addr)
			dev_err(md_ctrl->dev,
				"CLDMA%d Q%d TGPD addr, SW:%pad, HW:%llX\n",
				md_ctrl->hif_id, queue->index, &req->gpd_addr, ul_curr_addr);
		else
			/* retry */
			cldma_hw_resume_queue(&md_ctrl->hw_info, queue->index, false);
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

static void cldma_tx_done(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct md_cd_queue *queue = container_of(dwork, struct md_cd_queue, cldma_tx_work);
	struct cldma_hw_info *hw_info;
	struct md_cd_ctrl *md_ctrl;
	unsigned int L2TISAR0;
	unsigned long flags;
	int count;

	if (!queue->md) {
		pr_err("modem is NULL!\n");
		return;
	}

	md_ctrl = md_cd_get(queue->hif_id);
	if (!md_ctrl)	{
		pr_err("md_ctrl is NULL\n");
		return;
	}

	hw_info = &md_ctrl->hw_info;
	count = queue->tr_ring->handle_tx_done(queue, 0, 0);
	if (count && md_ctrl->tx_busy_warn_cnt)
		md_ctrl->tx_busy_warn_cnt = 0;
	/* greedy mode */
	L2TISAR0 = cldma_hw_int_status(hw_info,
				       BIT(queue->index) |
					(BIT(queue->index) << EQ_STA_BIT_OFFSET), false);
	if (L2TISAR0 & EMPTY_STATUS_BITMASK & (BIT(queue->index) << EQ_STA_BIT_OFFSET)) {
		cldma_hw_tx_done(hw_info, BIT(queue->index) << EQ_STA_BIT_OFFSET);
		cldma_tx_queue_empty_handler(queue);
	}
	if (L2TISAR0 & TXRX_STATUS_BITMASK & BIT(queue->index)) {
		cldma_hw_tx_done(hw_info, BIT(queue->index));
		queue_delayed_work(queue->worker,
				   &queue->cldma_tx_work, msecs_to_jiffies(0));
		return;
	}

	/* enable TX_DONE interrupt */
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->txq_active & BIT(queue->index)) {
		cldma_clear_ip_busy(hw_info);
		cldma_hw_dismask_eqirq(hw_info, queue->index, false);
		cldma_hw_dismask_txrxirq(hw_info, queue->index, false);
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	pm_runtime_put(md_ctrl->dev);
}

static void cldma_rx_ring_init(struct md_cd_ctrl *md_ctrl,
			       struct cldma_ring *ring)
{
	struct cldma_request *item, *first_item = NULL;
	struct cldma_rgpd *prev_gpd, *gpd = NULL;
	unsigned long flags;
	int i;

	if (ring->type != RING_GPD)
		return;

	for (i = 0; i < ring->length; i++) {
		item = kzalloc(sizeof(*item), GFP_KERNEL);
		if (!item)
			break;
		item->skb = ccci_alloc_skb(ring->pkt_size, 1, 1);
		if (!item->skb) {
			dev_err(md_ctrl->dev, "invalid input: skb is NULL\n");
			kfree(item);
			return;
		}
		item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool,
					   GFP_KERNEL, &item->gpd_addr);
		if (!item->gpd) {
			ccci_free_skb(item->skb);
			kfree(item);
			return;
		}
		gpd = (struct cldma_rgpd *)item->gpd;
		memset(gpd, 0, sizeof(struct cldma_rgpd));
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		item->mapped_buff = dma_map_single(md_ctrl->dev,
						   item->skb->data,
						   skb_data_size(item->skb),
						   DMA_FROM_DEVICE);
		if (dma_mapping_error(md_ctrl->dev,
				      item->mapped_buff)) {
			dev_err(md_ctrl->dev, "error dma mapping\n");
			item->mapped_buff = 0;
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock,
					       flags);
			ccci_free_skb(item->skb);
			dma_pool_free(md_ctrl->gpd_dmapool,
				      item->gpd, item->gpd_addr);
			kfree(item);
			return;
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		cldma_rgpd_set_data_ptr(gpd, item->mapped_buff);
		gpd->data_allow_len = ring->pkt_size;
		gpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		if (i == 0)
			first_item = item;
		else
			cldma_rgpd_set_next_ptr(prev_gpd, item->gpd_addr);
		INIT_LIST_HEAD(&item->entry);
		list_add_tail(&item->entry, &ring->gpd_ring);
		prev_gpd = gpd;
	}

	if (first_item)
		cldma_rgpd_set_next_ptr(gpd, first_item->gpd_addr);
}

static void cldma_tx_ring_init(struct md_cd_ctrl *md_ctrl,
			       struct cldma_ring *ring)
{
	struct cldma_request *item, *bd_item, *first_item = NULL;
	struct cldma_tbd *bd = NULL, *prev_bd = NULL;
	struct cldma_tgpd *tgpd, *prev_gpd;
	bool fbd_item;
	int i, j;

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(*item), GFP_KERNEL);
			if (!item)
				continue;
			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL,
						   &item->gpd_addr);
			if (!item->gpd) {
				kfree(item);
				continue;
			}
			tgpd = (struct cldma_tgpd *)item->gpd;
			memset(tgpd, 0, sizeof(struct cldma_tgpd));
			tgpd->gpd_flags = GPD_FLAGS_IOC;
			if (!first_item)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd, item->gpd_addr);
			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = tgpd;
		}
		if (first_item)
			cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
	}
	if (ring->type == RING_GPD_BD) {
		for (i = 0; i < ring->length; i++) {
			fbd_item = false;
			item = kzalloc(sizeof(*item), GFP_KERNEL);
			if (!item)
				continue;

			item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool,
						   GFP_KERNEL, &item->gpd_addr);
			if (!item->gpd) {
				kfree(item);
				continue;
			}
			tgpd = (struct cldma_tgpd *)item->gpd;
			memset(tgpd, 0, sizeof(struct cldma_tgpd));
			tgpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_BDP;
			if (!first_item)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd, item->gpd_addr);
			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = tgpd;
			/* add BD */
			for (j = 0; j < MAX_BD_NUM + 1; j++) {	/* extra 1 BD for skb head */
				bd_item = kzalloc(sizeof(*bd_item), GFP_KERNEL);
				if (!bd_item)
					continue;
				bd_item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL,
							      &bd_item->gpd_addr);
				if (!bd_item->gpd) {
					kfree(bd_item);
					continue;
				}
				bd = (struct cldma_tbd *)bd_item->gpd;
				memset(bd, 0, sizeof(struct cldma_tbd));
				if (!fbd_item) {
					fbd_item = true;
					cldma_tgpd_set_data_ptr(tgpd, bd_item->gpd_addr);
				} else {
					cldma_tbd_set_next_ptr(prev_bd, bd_item->gpd_addr);
				}
				INIT_LIST_HEAD(&bd_item->entry);
				list_add_tail(&bd_item->entry, &item->bd);
				prev_bd = bd;
			}
			if (bd)
				bd->bd_flags |= 0x1;	/* EOL */
		}
		if (first_item)
			cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
	}
}

static void cldma_ring_uninit(struct md_cd_ctrl *md_ctrl,
			      struct cldma_ring *ring, int dir)
{
	struct cldma_request *req_cur, *req_next;

	if (ring->type != RING_GPD)
		return;

	list_for_each_entry_safe(req_cur, req_next, &ring->gpd_ring, entry) {
		if (req_cur->mapped_buff && req_cur->skb) {
			if (dir == 1)
				dma_unmap_single(md_ctrl->dev,
						 req_cur->mapped_buff,
						 skb_data_size(req_cur->skb),
						 DMA_FROM_DEVICE);
			else
				dma_unmap_single(md_ctrl->dev,
						 req_cur->mapped_buff,
						 skb_data_size(req_cur->skb),
						 DMA_TO_DEVICE);
			req_cur->mapped_buff = 0;
		}

		if (req_cur->skb)
			ccci_free_skb(req_cur->skb);

		if (req_cur->gpd)
			dma_pool_free(md_ctrl->gpd_dmapool, req_cur->gpd,
				      req_cur->gpd_addr);

		list_del(&req_cur->entry);
		kfree_sensitive(req_cur);
	}
}

static void cldma_queue_switch_ring(struct md_cd_queue *queue)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(queue->hif_id);
	struct cldma_request *req;

	if (!md_ctrl)	{
		pr_err("md_ctrl is NULL\n");
		return;
	}
	if (queue->dir == MTK_OUT) {
		queue->tr_ring = &md_ctrl->normal_tx_ring[txq2ring[queue->index]];
		req = list_first_entry(&queue->tr_ring->gpd_ring,
				       struct cldma_request, entry);
		queue->tr_done = req;
		queue->tx_xmit = req;
		queue->budget = queue->tr_ring->length;
	} else if (queue->dir == MTK_IN) {
		queue->tr_ring = &md_ctrl->normal_rx_ring[rxq2ring[queue->index]];
		req = list_first_entry(&queue->tr_ring->gpd_ring,
				       struct cldma_request, entry);
		queue->tr_done = req;
		queue->rx_refill = req;
		queue->budget = queue->tr_ring->length;
	}
}

static void cldma_rx_queue_init(struct md_cd_queue *queue)
{
	queue->dir = MTK_IN;
	cldma_queue_switch_ring(queue);
	queue->q_type = rxq_type[queue->index];
}

static void cldma_tx_queue_init(struct md_cd_queue *queue)
{
	queue->dir = MTK_OUT;
	cldma_queue_switch_ring(queue);
	queue->q_type = txq_type[queue->index];
}

static inline void cldma_enable_irq(struct md_cd_ctrl *md_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 0, 1) == 0)
		mtk_clear_set_int_type(md_ctrl->mtk_dev,
				       md_ctrl->hw_info.phy_interrupt_id, false);
}

static inline void cldma_disable_irq(struct md_cd_ctrl *md_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 1, 0) == 1)
		mtk_clear_set_int_type(md_ctrl->mtk_dev,
				       md_ctrl->hw_info.phy_interrupt_id, true);
}

static int cldma_irq_work_cb(struct md_cd_ctrl *md_ctrl)
{
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	unsigned int L2TIMR0, L2RIMR0, L2TISAR0, L2RISAR0;
	unsigned int L3TISAR0, L3TISAR1;
	unsigned int L3RISAR0, L3RISAR1;
	int i, ret = 0;

	/* L2 raw interrupt status */
	L2TISAR0 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2TISAR0);
	L2RISAR0 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2RISAR0);
	L2TIMR0 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2TIMR0);
	L2RIMR0 = ioread32(hw_info->ap_ao_base + REG_CLDMA_L2RIMR0);

	if ((L2TISAR0 | L2RISAR0) == 0) {
		dev_err(md_ctrl->dev, "Warning: CLDMA%d false interrupt\n", md_ctrl->hif_id);
		return 0;
	}

	L2TISAR0 &= (~L2TIMR0);
	L2RISAR0 &= (~L2RIMR0);
	if (L2TISAR0) {
		if (md_ctrl->tx_busy_warn_cnt &&
		    (L2TISAR0 & TXRX_STATUS_BITMASK))
			md_ctrl->tx_busy_warn_cnt = 0;
		if (L2TISAR0 & (TQ_ERR_INT_BITMASK | TQ_ACTIVE_START_ERR_INT_BITMASK)) {
			/* read and clear L3 TX interrupt status */
			L3TISAR0 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3TISAR0);
			iowrite32(L3TISAR0,
				  hw_info->ap_pdn_base + REG_CLDMA_L3TISAR0);
			L3TISAR1 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3TISAR1);
			iowrite32(L3TISAR1, hw_info->ap_pdn_base + REG_CLDMA_L3TISAR1);
		}
		cldma_hw_tx_done(hw_info, L2TISAR0);

		if (L2TISAR0 & (TXRX_STATUS_BITMASK | EMPTY_STATUS_BITMASK)) {
			for (i = 0; i < CLDMA_TXQ_NUM; i++) {
				if (L2TISAR0 & TXRX_STATUS_BITMASK & BIT(i)) {
					pm_runtime_get(md_ctrl->dev);
					/* disable TX_DONE interrupt */
					cldma_hw_mask_eqirq(hw_info, i, false);
					cldma_hw_mask_txrxirq(hw_info, i, false);
					ret = queue_delayed_work(md_ctrl->txq[i].worker,
								 &md_ctrl->txq[i].cldma_tx_work,
								 msecs_to_jiffies(0));
				}
				if (L2TISAR0 & (EMPTY_STATUS_BITMASK &
						(BIT(i) << EQ_STA_BIT_OFFSET))) {
					cldma_tx_queue_empty_handler(&md_ctrl->txq[i]);
				}
			}
		}
	}

	if (L2RISAR0) {
		/* clear IP busy register wake up cpu case */
		if (L2RISAR0 & (RQ_ERR_INT_BITMASK | RQ_ACTIVE_START_ERR_INT_BITMASK)) {
			/* read and clear L3 RX interrupt status */
			L3RISAR0 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3RISAR0);
			iowrite32(L3RISAR0, hw_info->ap_pdn_base + REG_CLDMA_L3RISAR0);
			L3RISAR1 = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3RISAR1);
			iowrite32(L3RISAR1, hw_info->ap_pdn_base + REG_CLDMA_L3RISAR1);
		}
		cldma_hw_rx_done(hw_info, L2RISAR0);
		if (L2RISAR0 & (TXRX_STATUS_BITMASK | EMPTY_STATUS_BITMASK)) {
			for (i = 0; i < CLDMA_RXQ_NUM; i++) {
				if ((L2RISAR0 & BIT(i)) ||
				    (L2RISAR0 & (BIT(i) << EQ_STA_BIT_OFFSET))) {
					pm_runtime_get(md_ctrl->dev);
					/* disable RX_DONE and QUEUE_EMPTY interrupt */
					cldma_hw_mask_eqirq(hw_info, i, true);
					cldma_hw_mask_txrxirq(hw_info, i, true);
					/* always start work due to no napi */
					queue_work(md_ctrl->rxq[i].worker,
						   &md_ctrl->rxq[i].cldma_rx_work);
				}
			}
		}
	}
	return ret;
}

int ccci_cldma_stop(enum cldma_id hif_id)
{
	struct cldma_hw_info *hw_info;
	struct md_cd_ctrl *md_ctrl;
	unsigned int tx_active;
	unsigned int rx_active;
	int cnt;
	int i;

	md_ctrl = md_cd_get(hif_id);
	if (!md_ctrl) {
		pr_err("md_ctrl is NULL\n");
		return -EINVAL;
	}
	hw_info = &md_ctrl->hw_info;

	/* stop tx rx queue */
	md_ctrl->rxq_active = 0;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, true);
	md_ctrl->txq_active = 0;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, false);
	md_ctrl->txq_started = 0;

	/* disable L1 and L2 interrupt */
	cldma_disable_irq(md_ctrl);
	cldma_hw_stop(hw_info, true);
	cldma_hw_stop(hw_info, false);

	/* clear tx/rx/empty interrupt status */
	cldma_hw_tx_done(hw_info, 0xFFFF);
	cldma_hw_rx_done(hw_info, 0xFFFF);

	if (md_ctrl->is_late_init) {
		for (i = 0; i < CLDMA_TXQ_NUM; i++)
			flush_delayed_work(&md_ctrl->txq[i].cldma_tx_work);

		for (i = 0; i < CLDMA_RXQ_NUM; i++)
			flush_work(&md_ctrl->rxq[i].cldma_rx_work);
	}

	cnt = 0;
	do {
		tx_active = cldma_hw_queue_status(hw_info, CLDMA_ALL_Q, false);
		rx_active = cldma_hw_queue_status(hw_info, CLDMA_ALL_Q, true);
		if ((tx_active || rx_active) && tx_active != CLDMA_ALL_Q &&
		    rx_active != CLDMA_ALL_Q) {
			msleep(20);
			cnt++;
		} else {
			break;
		}
	} while (cnt < 500);

	if (tx_active || rx_active) {
		dev_err(md_ctrl->dev, "Could not stop CLDMA%d queue,tx=0x%X,rx=0x%X\n",
			md_ctrl->hif_id, tx_active, rx_active);

		return -EAGAIN;
	}

	return 0;
}

static void cldma_late_uninit(struct md_cd_ctrl *md_ctrl)
{
	unsigned long flags;
	int i;

	if (md_ctrl->is_late_init) {
		/* free all tx/rx cldma request/gdb/skb buff */
		for (i = 0; i < CLDMA_TXQ_NUM; i++)
			cldma_ring_uninit(md_ctrl,
					  &md_ctrl->normal_tx_ring[i], 0);

		for (i = 0; i < CLDMA_RXQ_NUM; i++)
			cldma_ring_uninit(md_ctrl,
					  &md_ctrl->normal_rx_ring[i], 1);

		dma_pool_destroy(md_ctrl->gpd_dmapool);
		md_ctrl->gpd_dmapool = NULL;
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		md_ctrl->is_late_init = 0;
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}
}

static int cldma_sw_reset(enum cldma_id hif_id)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);
	struct ccci_modem *md;
	unsigned long flags;
	int i;

	if (!md_ctrl) {
		pr_err("md_ctrl is NULL\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	md_ctrl->hif_id = hif_id;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	md = md_ctrl->mtk_dev->md;

	cldma_disable_irq(md_ctrl);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		md_ctrl->txq[i].md = md;
		cancel_delayed_work_sync(&md_ctrl->txq[i].cldma_tx_work);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		md_cd_queue_struct_reset(&md_ctrl->txq[i],
					 md_ctrl->hif_id, MTK_OUT, i);
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}

	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		md_ctrl->rxq[i].md = md;
		cancel_work_sync(&md_ctrl->rxq[i].cldma_rx_work);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		md_cd_queue_struct_reset(&md_ctrl->rxq[i],
					 md_ctrl->hif_id, MTK_IN, i);
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	}
	cldma_late_uninit(md_ctrl);
	md_ctrl->tx_busy_warn_cnt = 0;

	return 0;
}

void ccci_cldma_reset(void)
{
	cldma_sw_reset(ID_CLDMA1);
}

int ccci_cldma_start(enum cldma_id hif_id)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);
	struct cldma_hw_info *hw_info;
	unsigned long flags;
	int i;

	if (!md_ctrl) {
		pr_err("md_ctrl is NULL\n");
		return -EINVAL;
	}
	hw_info = &md_ctrl->hw_info;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->is_late_init) {
		cldma_enable_irq(md_ctrl);
		/* set start address */
		for (i = 0; i < CLDMA_TXQ_NUM; i++) {
			if (md_ctrl->txq[i].tr_done)
				cldma_hw_set_start_address(hw_info, i,
							   md_ctrl->txq[i].tr_done->gpd_addr, 0);
		}

		for (i = 0; i < CLDMA_RXQ_NUM; i++) {
			if (md_ctrl->rxq[i].tr_done)
				cldma_hw_set_start_address(hw_info, i,
							   md_ctrl->rxq[i].tr_done->gpd_addr, 1);
		}

		/* start all Rx queues and enable L2 interrupt */
		cldma_hw_start_queue(hw_info, 0xFF, 1);
		cldma_hw_start(hw_info);
		/* wait write done */
		wmb();
		md_ctrl->txq_started = 0;
		md_ctrl->txq_active |= TXRX_STATUS_BITMASK;
		md_ctrl->rxq_active |= TXRX_STATUS_BITMASK;
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

static void clear_txq(struct md_cd_ctrl *md_ctrl, int qnum)
{
	struct md_cd_queue *txq = &md_ctrl->txq[qnum];
	struct cldma_request *req;
	struct cldma_tgpd *tgpd;
	unsigned long flags;

	spin_lock_irqsave(&txq->ring_lock, flags);
	req = list_first_entry(&txq->tr_ring->gpd_ring,
			       struct cldma_request, entry);
	txq->tr_done = req;
	txq->tx_xmit = req;
	txq->budget = txq->tr_ring->length;
	list_for_each_entry(req, &txq->tr_ring->gpd_ring, entry) {
		tgpd = (struct cldma_tgpd *)req->gpd;
		tgpd->gpd_flags &= ~GPD_FLAGS_HWO;
		if (txq->tr_ring->type != RING_GPD_BD)
			cldma_tgpd_set_data_ptr(tgpd, 0);
		tgpd->data_buff_len = 0;
		if (req->skb) {
			ccci_free_skb(req->skb);
			req->skb = NULL;
		}
	}
	spin_unlock_irqrestore(&txq->ring_lock, flags);
}

static int clear_rxq(struct md_cd_ctrl *md_ctrl, int qnum)
{
	struct md_cd_queue *rxq = &md_ctrl->rxq[qnum];
	struct cldma_request *req;
	struct cldma_rgpd *rgpd;
	unsigned long flags;

	spin_lock_irqsave(&rxq->ring_lock, flags);
	req = list_first_entry(&rxq->tr_ring->gpd_ring,
			       struct cldma_request, entry);
	rxq->tr_done = req;
	rxq->rx_refill = req;
	list_for_each_entry(req, &rxq->tr_ring->gpd_ring, entry) {
		rgpd = (struct cldma_rgpd *)req->gpd;
		rgpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		rgpd->data_buff_len = 0;
		if (req->skb) {
			req->skb->len = 0;
			skb_reset_tail_pointer(req->skb);
		}
	}
	spin_unlock_irqrestore(&rxq->ring_lock, flags);
	list_for_each_entry(req, &rxq->tr_ring->gpd_ring, entry) {
		rgpd = (struct cldma_rgpd *)req->gpd;
		if (req->skb)
			continue;
		req->skb = ccci_alloc_skb(rxq->tr_ring->pkt_size, 1, 1);
		if (!req->skb) {
			dev_err(md_ctrl->dev, "skb not allocated\n");
			return -EFAULT;
		}
		req->mapped_buff = dma_map_single(md_ctrl->dev, req->skb->data,
						  skb_data_size(req->skb),
						  DMA_FROM_DEVICE);
		if (dma_mapping_error(md_ctrl->dev,
				      req->mapped_buff)) {
			dev_err(md_ctrl->dev, "err dma mapping\n");
			return -EFAULT;
		}
		cldma_rgpd_set_data_ptr(rgpd, req->mapped_buff);
	}
	return 0;
}

/* only allowed when cldma is stopped */
static void cldma_clear_all_queue(struct md_cd_ctrl *md_ctrl,
				  enum direction dir)
{
	int i;

	if (dir == MTK_OUT || dir == MTK_INOUT) {
		for (i = 0; i < CLDMA_TXQ_NUM; i++)
			clear_txq(md_ctrl, i);
	}
	if (dir == MTK_IN || dir == MTK_INOUT) {
		for (i = 0; i < CLDMA_RXQ_NUM; i++)
			clear_rxq(md_ctrl, i);
	}
}

static int cldma_stop_queue(struct md_cd_ctrl *md_ctrl,
			    unsigned char qno, enum direction dir)
{
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	unsigned long flags;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (dir == MTK_IN) {
		/* disable RX_DONE and QUEUE_EMPTY interrupt */
		cldma_hw_mask_eqirq(hw_info, qno, true);
		cldma_hw_mask_txrxirq(hw_info, qno, true);
		if (qno == CLDMA_ALL_Q)
			md_ctrl->rxq_active &= ~TXRX_STATUS_BITMASK;
		else
			md_ctrl->rxq_active &= ~(TXRX_STATUS_BITMASK & BIT(qno));

		cldma_hw_stop_queue(hw_info, qno, true);
	} else if (dir == MTK_OUT) {
		cldma_hw_mask_eqirq(hw_info, qno, false);
		cldma_hw_mask_txrxirq(hw_info, qno, false);
		if (qno == CLDMA_ALL_Q)
			md_ctrl->txq_active &= ~TXRX_STATUS_BITMASK;
		else
			md_ctrl->txq_active &= ~(TXRX_STATUS_BITMASK & BIT(qno));

		cldma_hw_stop_queue(hw_info, qno, false);
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

/* this is called inside queue->ring_lock */
static int cldma_gpd_handle_tx_request(struct md_cd_queue *queue,
				       struct cldma_request *tx_req, struct sk_buff *skb,
				       unsigned int ioc_override)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(queue->hif_id);
	struct cldma_tgpd *tgpd;
	unsigned long flags;

	if (!md_ctrl) {
		pr_err("md_ctrl is NULL\n");
		return -EINVAL;
	}

	tgpd = tx_req->gpd;
	/* override current IOC setting */
	if (ioc_override & GPD_FLAGS_IOC) {
		/* backup current IOC setting */
		if (tgpd->gpd_flags & GPD_FLAGS_IOC)
			tx_req->ioc_override = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		else
			tx_req->ioc_override = GPD_FLAGS_IOC;
		if (ioc_override & GPD_FLAGS_HWO)
			tgpd->gpd_flags |= GPD_FLAGS_IOC;
		else
			tgpd->gpd_flags &= ~GPD_FLAGS_IOC;
	}
	/* update GPD */
	tx_req->mapped_buff = dma_map_single(md_ctrl->dev, skb->data,
					     skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(md_ctrl->dev, tx_req->mapped_buff)) {
		dev_err(md_ctrl->dev, "error dma mapping\n");
		return -EINVAL;
	}
	cldma_tgpd_set_data_ptr(tgpd, tx_req->mapped_buff);
	tgpd->data_buff_len = skb->len;
	tgpd->debug_tx = 1;
	/* set HWO
	 * use cldma_timeout_lock to avoid race condition with cldma_stop.
	 * this lock must cover TGPD setting, as even without a resume
	 * operation, CLDMA still can start sending next HWO=1 TGPD
	 * if last TGPD was just finished.
	 */
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->txq_active & BIT(queue->index))
		tgpd->gpd_flags |= GPD_FLAGS_HWO;
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	/* mark cldma_request as available */
	tx_req->skb = skb;
	return 0;
}

static void cldma_hw_start_send(struct md_cd_ctrl *md_ctrl, u8 qno)
{
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	struct cldma_request *req;

	/* check whether the device was powered off (cldma start address is not set) */
	if (!cldma_tx_addr_is_set(hw_info, qno)) {
		cldma_hw_init(hw_info);
		req = cldma_ring_step_backward(md_ctrl->txq[qno].tr_ring,
					       md_ctrl->txq[qno].tx_xmit);
		cldma_hw_set_start_address(hw_info, qno, req->gpd_addr, false);
		md_ctrl->txq_started &= ~BIT(qno);
	}

	/* resume or start queue */
	if (!cldma_hw_queue_status(hw_info, qno, false)) {
		if (md_ctrl->txq_started & BIT(qno))
			cldma_hw_resume_queue(hw_info, qno, false);
		else
			cldma_hw_start_queue(hw_info, qno, false);
		md_ctrl->txq_started |= BIT(qno);
	}
}

int ccci_cldma_write_room(enum cldma_id hif_id, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);
	struct md_cd_queue *queue;

	if (!md_ctrl)
		return -EINVAL;

	queue = &md_ctrl->txq[qno];
	if (!queue)
		return -EINVAL;

	if (queue->budget > (MAX_TX_BUDGET - 1))
		return queue->budget;

	return 0;
}

int ccci_cldma_send_skb(enum cldma_id hif_id, int qno, struct sk_buff *skb,
			int skb_from_pool, int blocking)
{
	struct ccci_buffer_ctrl *buf_ctrl;
	struct cldma_request *tx_req;
	struct md_cd_ctrl *md_ctrl;
	struct md_cd_queue *queue;
	unsigned int ioc_override = 0;
	unsigned long flags;
	int ret = 0;

	md_ctrl = md_cd_get(hif_id);
	if (!md_ctrl) {
		pr_err("md_ctrl is NULL\n");
		return -EINVAL;
	}

	pm_runtime_get_sync(md_ctrl->dev);
	mtk_pci_l_resource_lock(md_ctrl->mtk_dev);
	if (qno >= CLDMA_TXQ_NUM) {
		ret = -EINVAL;
		goto exit;
	}

	if (skb_from_pool && skb_headroom(skb) == NET_SKB_PAD) {
		buf_ctrl = (struct ccci_buffer_ctrl *)skb_push(skb,
			sizeof(struct ccci_buffer_ctrl));
		if (buf_ctrl->head_magic == CCCI_BUF_MAGIC)
			ioc_override = buf_ctrl->ioc_override;
		skb_pull(skb, sizeof(struct ccci_buffer_ctrl));
	}

	queue = &md_ctrl->txq[qno];

	/* check if queue is active */
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (!(md_ctrl->txq_active & BIT(qno))) {
		ret = -EBUSY;
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		goto exit;
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

 retry:
	spin_lock_irqsave(&queue->ring_lock, flags);
	tx_req = queue->tx_xmit;
	if (queue->budget > 0 && !tx_req->skb) {
		queue->budget--;
		queue->tr_ring->handle_tx_request(queue, tx_req, skb, ioc_override);
		queue->tx_xmit = cldma_ring_step_forward(queue->tr_ring, tx_req);
		spin_unlock_irqrestore(&queue->ring_lock, flags);

		mtk_pci_l_resource_wait_complete(md_ctrl->mtk_dev);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		md_ctrl->tx_busy_warn_cnt = 0;
		cldma_hw_start_send(md_ctrl, qno);
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	} else {
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		mtk_pci_l_resource_wait_complete(md_ctrl->mtk_dev);

		/* check CLDMA status */
		if (cldma_hw_queue_status(&md_ctrl->hw_info, qno, false)) {
			queue->busy_count++;
			md_ctrl->tx_busy_warn_cnt = 0;
		} else {
			if (++md_ctrl->tx_busy_warn_cnt == 1000)
				dev_notice(md_ctrl->dev, "tx busy: dump CLDMA and GPD status\n");
			/* resume channel */
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
			cldma_hw_resume_queue(&md_ctrl->hw_info, qno, false);
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		}

		if (blocking) {
			ret = wait_event_interruptible_exclusive(queue->req_wq,
								 queue->budget > 0);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}

			goto retry;
		} else {
			ret = -EBUSY;
			goto exit;
		}
	}

 exit:
	mtk_pci_l_resource_unlock(md_ctrl->mtk_dev);
	pm_runtime_put_sync(md_ctrl->dev);
	return ret;
}

int ccci_cldma_txq_mtu(unsigned char qno)
{
	if (qno >= CLDMA_TXQ_NUM)
		return -EINVAL;

	return txq_buff_size[qno];
}

static inline void set_txq_buff_size(unsigned char qno, int buffer_size)
{
	txq_buff_size[qno] = buffer_size;
}

static void ccci_cldma_adjust_config(void)
{
	int qno;

	/* set back to default set, Because RXQ NUM= TXQ NUM */
	for (qno = 0; qno < CLDMA_RXQ_NUM; qno++) {
		rxq_buff_size[qno] = SKB_4K;
		set_txq_buff_size(qno, SKB_4K);
		rxq_type[qno] = SHARED_Q;
		txq_type[qno] = SHARED_Q;
	}
	rxq_buff_size[7] = SKB_64K;
}

/* this function contains longer duration initializations */
static int cldma_late_init(struct md_cd_ctrl *md_ctrl)
{
	char dma_pool_name[32];
	unsigned long flags;
	int i;

	if (md_ctrl->is_late_init == 1) {
		dev_err(md_ctrl->dev, "cldma late init was already done\n");
		return -EALREADY;
	}

	atomic_set(&md_ctrl->wakeup_src, 0);
	mutex_lock(&ctl_cfg_mutex);
	ccci_cldma_adjust_config();
	/* init ring buffers */
	sprintf(dma_pool_name, "cldma_requeset_DMA_hif%d", md_ctrl->hif_id);
	md_ctrl->gpd_dmapool = dma_pool_create(dma_pool_name,
					       md_ctrl->dev,
					       sizeof(struct cldma_tgpd), 16, 0);
	if (!md_ctrl->gpd_dmapool) {
		mutex_unlock(&ctl_cfg_mutex);
		dev_err(md_ctrl->dev, "dmapool alloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_tx_ring[i].gpd_ring);
		md_ctrl->normal_tx_ring[i].length = txq_buff_num[ring2txq[i]];
		md_ctrl->normal_tx_ring[i].type = RING_GPD;
		md_ctrl->normal_tx_ring[i].handle_tx_request = &cldma_gpd_handle_tx_request;
		md_ctrl->normal_tx_ring[i].handle_tx_done = &cldma_gpd_tx_collect;
		cldma_tx_ring_init(md_ctrl, &md_ctrl->normal_tx_ring[i]);
	}
	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_rx_ring[i].gpd_ring);
		md_ctrl->normal_rx_ring[i].length = rxq_buff_num[ring2rxq[i]];
		md_ctrl->normal_rx_ring[i].pkt_size = rxq_buff_size[ring2rxq[i]];
		md_ctrl->normal_rx_ring[i].type = RING_GPD;
		md_ctrl->normal_rx_ring[i].handle_rx_done = &cldma_gpd_rx_collect;
		cldma_rx_ring_init(md_ctrl, &md_ctrl->normal_rx_ring[i]);
	}
	/* init queue */
	for (i = 0; i < CLDMA_TXQ_NUM; i++)
		cldma_tx_queue_init(&md_ctrl->txq[i]);

	for (i = 0; i < CLDMA_RXQ_NUM; i++)
		cldma_rx_queue_init(&md_ctrl->rxq[i]);
	mutex_unlock(&ctl_cfg_mutex);

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	md_ctrl->is_late_init = 1;
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

static inline void __iomem *pcie_addr_transfer(void __iomem *addr, u32 addr_trs1, u32 phy_addr)
{
	return addr + phy_addr - addr_trs1;
}

static void hw_info_init(struct md_cd_ctrl *md_ctrl)
{
	struct mtk_addr_base *pbase = &md_ctrl->mtk_dev->base_addr;
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	u32 phy_ao_base, phy_pd_base;

	if (md_ctrl->hif_id != ID_CLDMA1)
		return;

	phy_ao_base = CLDMA1_AO_BASE;
	phy_pd_base = CLDMA1_PD_BASE;
	hw_info->phy_interrupt_id = CLDMA1_INT;

	hw_info->hw_mode = MODE_BIT_64;
	hw_info->ap_ao_base = pcie_addr_transfer(pbase->pcie_ext_reg_base,
						 pbase->pcie_dev_reg_trsl_addr,
						 phy_ao_base);
	hw_info->ap_pdn_base = pcie_addr_transfer(pbase->pcie_ext_reg_base,
						  pbase->pcie_dev_reg_trsl_addr,
						  phy_pd_base);
}

static struct md_cd_ctrl *alloc_md_ctrl(enum cldma_id hif_id, struct mtk_pci_dev *mtk_dev)
{
	struct md_cd_ctrl *md_ctrl;

	md_ctrl = kzalloc(sizeof(*md_ctrl), GFP_KERNEL);
	if (!md_ctrl)
		return NULL;

	md_ctrl->mtk_dev = mtk_dev;
	md_ctrl->dev = &mtk_dev->pdev->dev;
	md_ctrl->hif_id = hif_id;
	hw_info_init(md_ctrl);
	md_cd_set(hif_id, md_ctrl);

	return md_ctrl;
}

int ccci_cldma_alloc(struct mtk_pci_dev *mtk_dev)
{
	struct md_cd_ctrl *md_ctrl1;

	md_ctrl1 = alloc_md_ctrl(ID_CLDMA1, mtk_dev);
	if (!md_ctrl1)
		return -ENOMEM;

	return 0;
}

void ccci_cldma_exception(enum cldma_id hif_id, enum hif_ex_stage stage)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);

	switch (stage) {
	case HIF_EX_INIT:
		/* disable CLDMA Tx queues */
		cldma_stop_queue(md_ctrl, CLDMA_ALL_Q, MTK_OUT);
		/* Clear Tx queue */
		cldma_clear_all_queue(md_ctrl, MTK_OUT);
		break;
	case HIF_EX_CLEARQ_DONE:
		/* stop CLDMA, we don't want to get CLDMA IRQ when MD is
		 * resetting CLDMA after it got cleaq_ack
		 */
		cldma_stop_queue(md_ctrl, CLDMA_ALL_Q, MTK_IN);
		/* flush all (TX&RX) workqueue */
		ccci_cldma_stop(hif_id);
		if (md_ctrl->hif_id == ID_CLDMA1)
			cldma_hw_reset(md_ctrl->mtk_dev->base_addr.infracfg_ao_base);
		/* clear the RX queue */
		cldma_clear_all_queue(md_ctrl, MTK_IN);
		break;
	case HIF_EX_ALLQ_RESET:
		cldma_hw_init(&md_ctrl->hw_info);
		ccci_cldma_start(hif_id);
		break;
	default:
		break;
	}
}

static int cldma_resume_early(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)entity_param;
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	unsigned long flags;
	int qno_t;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	cldma_hw_reg_restore(hw_info);

	for (qno_t = 0; qno_t < CLDMA_TXQ_NUM; qno_t++) {
		cldma_hw_set_start_address(hw_info,
					   qno_t,
					   md_ctrl->txq[qno_t].tx_xmit->gpd_addr,
					   false);
		cldma_hw_set_start_address(hw_info,
					   qno_t,
					   md_ctrl->rxq[qno_t].tr_done->gpd_addr,
					   true);
	}

	cldma_enable_irq(md_ctrl);
	cldma_hw_start_queue(hw_info, CLDMA_ALL_Q, true);
	md_ctrl->rxq_active |= TXRX_STATUS_BITMASK;
	cldma_hw_dismask_eqirq(hw_info, CLDMA_ALL_Q, true);
	cldma_hw_dismask_txrxirq(hw_info, CLDMA_ALL_Q, true);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	return 0;
}

static int cldma_resume(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)entity_param;
	unsigned long flags;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	md_ctrl->txq_active |= TXRX_STATUS_BITMASK;
	cldma_hw_dismask_txrxirq(&md_ctrl->hw_info, CLDMA_ALL_Q, false);
	cldma_hw_dismask_eqirq(&md_ctrl->hw_info, CLDMA_ALL_Q, false);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->hif_id == ID_CLDMA1)
		mhccif_mask_clr(mtk_dev, D2H_SW_INT_MASK);

	return 0;
}

static int cldma_suspend_late(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)entity_param;
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	unsigned long flags;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	cldma_hw_mask_eqirq(hw_info, CLDMA_ALL_Q, true);
	cldma_hw_mask_txrxirq(hw_info, CLDMA_ALL_Q, true);
	md_ctrl->rxq_active &= ~TXRX_STATUS_BITMASK;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, true);
	cldma_clear_ip_busy(hw_info);
	cldma_disable_irq(md_ctrl);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

static int cldma_suspend(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct md_cd_ctrl *md_ctrl = entity_param;
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	unsigned long flags;

	if (md_ctrl->hif_id == ID_CLDMA1)
		mhccif_mask_set(mtk_dev, D2H_SW_INT_MASK);

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	cldma_hw_mask_eqirq(hw_info, CLDMA_ALL_Q, false);
	cldma_hw_mask_txrxirq(hw_info, CLDMA_ALL_Q, false);
	md_ctrl->txq_active &= ~TXRX_STATUS_BITMASK;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, false);
	md_ctrl->txq_started = 0;
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
}

static int cldma_pm_init(struct md_cd_ctrl *md_ctrl)
{
	md_ctrl->pm_entity = kzalloc(sizeof(*md_ctrl->pm_entity), GFP_KERNEL);
	if (!md_ctrl->pm_entity)
		return -ENOMEM;

	md_ctrl->pm_entity->entity_param = md_ctrl;
	if (md_ctrl->hif_id == ID_CLDMA1)
		md_ctrl->pm_entity->key = PM_ENTITY_KEY_CTRL;
	else
		md_ctrl->pm_entity->key = PM_ENTITY_KEY_CTRL2;

	md_ctrl->pm_entity->suspend = cldma_suspend;
	md_ctrl->pm_entity->suspend_late = cldma_suspend_late;
	md_ctrl->pm_entity->resume = cldma_resume;
	md_ctrl->pm_entity->resume_early = cldma_resume_early;

	return mtk_pci_pm_entity_register(md_ctrl->mtk_dev, md_ctrl->pm_entity);
}

static int cldma_pm_uninit(struct md_cd_ctrl *md_ctrl)
{
	if (!md_ctrl->pm_entity) {
		dev_err(md_ctrl->dev, "pm_entity not allocated\n");
		return -EINVAL;
	}

	mtk_pci_pm_entity_unregister(md_ctrl->mtk_dev, md_ctrl->pm_entity);
	kfree_sensitive(md_ctrl->pm_entity);
	md_ctrl->pm_entity = NULL;

	return 0;
}

void ccci_cldma_hif_hw_init(enum cldma_id hif_id)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);
	struct cldma_hw_info *hw_info;
	unsigned long flags;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	hw_info = &md_ctrl->hw_info;

	/* mask cldma interrupt */
	cldma_hw_stop(hw_info, false);
	cldma_hw_stop(hw_info, true);

	/* clear cldma interrupt */
	cldma_hw_rx_done(hw_info, EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK);
	cldma_hw_tx_done(hw_info, EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK);

	/* initialize CLDMA hardware */
	cldma_hw_init(hw_info);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

static int pcie_irq_cb_handler(void *data)
{
	struct md_cd_ctrl *md_ctrl = data;
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	int ret;

	mtk_clear_set_int_type(md_ctrl->mtk_dev, hw_info->phy_interrupt_id, true);
	ret = cldma_irq_work_cb(md_ctrl);
	mtk_clear_int_status(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);
	mtk_clear_set_int_type(md_ctrl->mtk_dev, hw_info->phy_interrupt_id, false);

	return ret;
}

int ccci_cldma_init(enum cldma_id hif_id)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);
	struct cldma_hw_info *hw_info;
	struct ccci_modem *md;
	int ret, i;

	md = md_ctrl->mtk_dev->md;
	md_ctrl->hif_id = hif_id;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	md_ctrl->is_late_init = 0;
	md_ctrl->tx_busy_warn_cnt = 0;
	hw_info = &md_ctrl->hw_info;
	ret = cldma_pm_init(md_ctrl);
	if (ret)
		return ret;

	spin_lock_init(&md_ctrl->cldma_timeout_lock);
	/* initialize HIF queue structure */
	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		md_cd_queue_struct_init(&md_ctrl->txq[i], md_ctrl->hif_id, MTK_OUT, i);
		md_ctrl->txq[i].md = md;
		md_ctrl->txq[i].worker =
			alloc_workqueue("md_hif%d_tx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM | (i == 0 ? WQ_HIGHPRI : 0),
					1, hif_id, i);
		if (!md_ctrl->txq[i].worker)
			return -ENOMEM;
		INIT_DELAYED_WORK(&md_ctrl->txq[i].cldma_tx_work, cldma_tx_done);
	}
	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		md_cd_queue_struct_init(&md_ctrl->rxq[i], md_ctrl->hif_id, MTK_IN, i);
		md_ctrl->rxq[i].md = md;
		INIT_WORK(&md_ctrl->rxq[i].cldma_rx_work, cldma_rx_done);
		md_ctrl->rxq[i].worker = alloc_workqueue("md_hif%d_rx%d_worker",
							 WQ_UNBOUND | WQ_MEM_RECLAIM,
							 1, hif_id, i);
		if (!md_ctrl->rxq[i].worker)
			return -ENOMEM;
	}

	mtk_clear_set_int_type(md_ctrl->mtk_dev, hw_info->phy_interrupt_id, true);
	atomic_set(&md_ctrl->cldma_irq_enabled, 0);

	/* registers CLDMA callback isr with PCIe driver */
	md_ctrl->mtk_dev->intr_callback[hw_info->phy_interrupt_id] = pcie_irq_cb_handler;
	md_ctrl->mtk_dev->callback_param[hw_info->phy_interrupt_id] = md_ctrl;
	mtk_clear_int_status(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);

	md_ctrl->tx_busy_warn_cnt = 0;

	return 0;
}

void ccci_cldma_switch_cfg(enum cldma_id hif_id)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);

	if (md_ctrl) {
		cldma_late_uninit(md_ctrl);
		cldma_late_init(md_ctrl);
	}
}

void ccci_cldma_exit(enum cldma_id hif_id)
{
	struct md_cd_ctrl *md_ctrl = md_cd_get(hif_id);
	int i;

	if (!md_ctrl)
		return;

	/* stop cldma work */
	ccci_cldma_stop(hif_id);
	cldma_late_uninit(md_ctrl);

	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		if (md_ctrl->txq[i].worker) {
			flush_workqueue(md_ctrl->txq[i].worker);
			destroy_workqueue(md_ctrl->txq[i].worker);
			md_ctrl->txq[i].worker = NULL;
		}
	}
	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		if (md_ctrl->rxq[i].worker) {
			flush_workqueue(md_ctrl->rxq[i].worker);
			destroy_workqueue(md_ctrl->rxq[i].worker);
			md_ctrl->rxq[i].worker = NULL;
		}
	}

	cldma_pm_uninit(md_ctrl);
	kfree_sensitive(md_ctrl);
	md_cd_set(hif_id, NULL);
}
