// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/skbuff.h>

#include "t7xx_common.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_mhccif.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_pcie_mac.h"
#include "t7xx_skb_util.h"

#define MAX_TX_BUDGET			16
#define MAX_RX_BUDGET			16

#define CHECK_Q_STOP_TIMEOUT_US		1000000
#define CHECK_Q_STOP_STEP_US		10000

static struct cldma_ctrl *cldma_md_ctrl[CLDMA_NUM];

static DEFINE_MUTEX(ctl_cfg_mutex); /* protects CLDMA late init config */

static enum cldma_queue_type rxq_type[CLDMA_RXQ_NUM];
static enum cldma_queue_type txq_type[CLDMA_TXQ_NUM];
static int rxq_buff_size[CLDMA_RXQ_NUM];
static int txq_buff_size[CLDMA_TXQ_NUM];
static int rxq_buff_num[CLDMA_RXQ_NUM];
static int txq_buff_num[CLDMA_TXQ_NUM];
extern bool da_down_stage_flag;

static struct cldma_ctrl *md_cd_get(enum cldma_id hif_id)
{
	return cldma_md_ctrl[hif_id];
}

static inline void md_cd_set(enum cldma_id hif_id, struct cldma_ctrl *md_ctrl)
{
	cldma_md_ctrl[hif_id] = md_ctrl;
}

static inline void md_cd_queue_struct_reset(struct cldma_queue *queue, enum cldma_id hif_id,
					    enum direction dir, unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->hif_id = hif_id;
	queue->tr_ring = NULL;
	queue->tr_done = NULL;
	queue->tx_xmit = NULL;
}

static inline void md_cd_queue_struct_init(struct cldma_queue *queue, enum cldma_id hif_id,
					   enum direction dir, unsigned char index)
{
	md_cd_queue_struct_reset(queue, hif_id, dir, index);
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->ring_lock);
}

static inline void cldma_tgpd_set_data_ptr(struct cldma_tgpd *tgpd, dma_addr_t data_ptr)
{
	tgpd->data_buff_bd_ptr_h = upper_32_bits(data_ptr);
	tgpd->data_buff_bd_ptr_l = lower_32_bits(data_ptr);
}

static inline void cldma_tgpd_set_next_ptr(struct cldma_tgpd *tgpd, dma_addr_t next_ptr)
{
	tgpd->next_gpd_ptr_h = upper_32_bits(next_ptr);
	tgpd->next_gpd_ptr_l = lower_32_bits(next_ptr);
}

static inline void cldma_rgpd_set_data_ptr(struct cldma_rgpd *rgpd, dma_addr_t data_ptr)
{
	rgpd->data_buff_bd_ptr_h = upper_32_bits(data_ptr);
	rgpd->data_buff_bd_ptr_l = lower_32_bits(data_ptr);
}

static inline void cldma_rgpd_set_next_ptr(struct cldma_rgpd *rgpd, dma_addr_t next_ptr)
{
	rgpd->next_gpd_ptr_h = upper_32_bits(next_ptr);
	rgpd->next_gpd_ptr_l = lower_32_bits(next_ptr);
}

static struct cldma_request *cldma_ring_step_forward(struct cldma_ring *ring,
						     struct cldma_request *req)
{
	struct cldma_request *next_req;

	if (req->entry.next == &ring->gpd_ring)
		next_req = list_first_entry(&ring->gpd_ring, struct cldma_request, entry);
	else
		next_req = list_entry(req->entry.next, struct cldma_request, entry);

	return next_req;
}

static struct cldma_request *cldma_ring_step_backward(struct cldma_ring *ring,
						      struct cldma_request *req)
{
	struct cldma_request *prev_req;

	if (req->entry.prev == &ring->gpd_ring)
		prev_req = list_last_entry(&ring->gpd_ring, struct cldma_request, entry);
	else
		prev_req = list_entry(req->entry.prev, struct cldma_request, entry);

	return prev_req;
}

static int cldma_gpd_rx_from_queue(struct cldma_queue *queue, int budget, bool *over_budget)
{
	unsigned char hwo_polling_count = 0;
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	struct cldma_request *req;
	struct cldma_rgpd *rgpd;
	struct sk_buff *new_skb;
	bool rx_done = false;
	struct sk_buff *skb;
	int count = 0;
	int ret = 0;

	md_ctrl = md_cd_get(queue->hif_id);
	hw_info = &md_ctrl->hw_info;

	while (!rx_done) {
		req = queue->tr_done;
		if (!req) {
			dev_err(md_ctrl->dev, "RXQ was released\n");
			return -ENODATA;
		}

		rgpd = req->gpd;
		if ((rgpd->gpd_flags & GPD_FLAGS_HWO) || !req->skb) {
			u64 gpd_addr;

			/* current 64 bit address is in a table by Q index */
			gpd_addr = ioread64(hw_info->ap_pdn_base +
					    REG_CLDMA_DL_CURRENT_ADDRL_0 +
					    queue->index * sizeof(u64));
			if (gpd_addr == GENMASK_ULL(63, 0)) {
				dev_err(md_ctrl->dev, "PCIe Link disconnected\n");
				return -ENODATA;
			}

			if ((u64)queue->tr_done->gpd_addr != gpd_addr &&
			    hwo_polling_count++ < 100) {
				udelay(1);
				continue;
			}

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

		/* consume skb */
		if (md_ctrl->recv_skb) {
			ret = md_ctrl->recv_skb(queue, skb);
		} else {
			ccci_free_skb(&md_ctrl->mtk_dev->pools, skb);
			ret = -ENETDOWN;
		}

		new_skb = NULL;
		if (ret >= 0 || ret == -ENETDOWN)
			new_skb = ccci_alloc_skb_from_pool(&md_ctrl->mtk_dev->pools,
							   queue->tr_ring->pkt_size,
							   GFS_BLOCKING);

		if (!new_skb) {
			/* either the port was busy or the skb pool was empty */
			usleep_range(5000, 10000);
			return -EAGAIN;
		}

		/* mark cldma_request as available */
		req->skb = NULL;
		cldma_rgpd_set_data_ptr(rgpd, 0);
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);

		req = queue->rx_refill;
		rgpd = req->gpd;
		req->mapped_buff = dma_map_single(md_ctrl->dev, new_skb->data,
						  skb_data_size(new_skb), DMA_FROM_DEVICE);
		if (dma_mapping_error(md_ctrl->dev, req->mapped_buff)) {
			dev_err(md_ctrl->dev, "DMA mapping failed\n");
			req->mapped_buff = 0;
			ccci_free_skb(&md_ctrl->mtk_dev->pools, new_skb);
			return -ENOMEM;
		}

		cldma_rgpd_set_data_ptr(rgpd, req->mapped_buff);
		rgpd->data_buff_len = 0;
		/* set HWO, no need to hold ring_lock */
		rgpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		/* mark cldma_request as available */
		req->skb = new_skb;
		queue->rx_refill = cldma_ring_step_forward(queue->tr_ring, req);

		if (++count >= budget && need_resched()) {
			*over_budget = true;
			rx_done = true;
		}
	}

	return 0;
}

static int cldma_gpd_rx_collect(struct cldma_queue *queue, int budget)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	bool over_budget = false;
	bool rx_not_done = true;
	unsigned int l2_rx_int;
	unsigned long flags;
	int ret;

	md_ctrl = md_cd_get(queue->hif_id);
	hw_info = &md_ctrl->hw_info;

	while (rx_not_done) {
		rx_not_done = false;
		ret = cldma_gpd_rx_from_queue(queue, budget, &over_budget);
		if (ret == -ENODATA)
			return 0;

		if (ret)
			return ret;

		spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
		if (md_ctrl->rxq_active & BIT(queue->index)) {
			/* resume Rx queue */
			if (!cldma_hw_queue_status(hw_info, queue->index, true))
				cldma_hw_resume_queue(hw_info, queue->index, true);

			/* greedy mode */
			l2_rx_int = cldma_hw_int_status(hw_info, BIT(queue->index), true);

			if (l2_rx_int) {
				/* need be scheduled again, avoid the soft lockup */
				cldma_hw_rx_done(hw_info, l2_rx_int);
				if (over_budget) {
					spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
					return -EAGAIN;
				}

				/* clear IP busy register wake up CPU case */
				rx_not_done = true;
			}
		}

		spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	}

	return 0;
}

static void cldma_rx_done(struct work_struct *work)
{
	struct cldma_ctrl *md_ctrl;
	struct cldma_queue *queue;
	int value;

	queue = container_of(work, struct cldma_queue, cldma_rx_work);
	md_ctrl = md_cd_get(queue->hif_id);
	value = queue->tr_ring->handle_rx_done(queue, queue->budget);

	if (value && md_ctrl->rxq_active & BIT(queue->index)) {
		queue_work(queue->worker, &queue->cldma_rx_work);
		return;
	}

	/* clear IP busy register wake up CPU case */
	cldma_clear_ip_busy(&md_ctrl->hw_info);
	/* enable RX_DONE && EMPTY interrupt */
	cldma_hw_dismask_txrxirq(&md_ctrl->hw_info, queue->index, true);
	cldma_hw_dismask_eqirq(&md_ctrl->hw_info, queue->index, true);
	pm_runtime_mark_last_busy(md_ctrl->dev);
	pm_runtime_put_autosuspend(md_ctrl->dev);
}

static int cldma_gpd_tx_collect(struct cldma_queue *queue)
{
	struct cldma_ctrl *md_ctrl;
	struct cldma_request *req;
	struct sk_buff *skb_free;
	struct cldma_tgpd *tgpd;
	unsigned int dma_len;
	unsigned long flags;
	dma_addr_t dma_free;
	int count = 0;

	md_ctrl = md_cd_get(queue->hif_id);

	while (!kthread_should_stop()) {
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = queue->tr_done;
		if (!req) {
			dev_err(md_ctrl->dev, "TXQ was released\n");
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}

		tgpd = req->gpd;
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
		dma_unmap_single(md_ctrl->dev, dma_free, dma_len, DMA_TO_DEVICE);

		ccci_free_skb(&md_ctrl->mtk_dev->pools, skb_free);
	}

	if (count)
		wake_up_nr(&queue->req_wq, count);

	return count;
}

static void cldma_tx_queue_empty_handler(struct cldma_queue *queue)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	struct cldma_request *req;
	struct cldma_tgpd *tgpd;
	dma_addr_t ul_curr_addr;
	unsigned long flags;
	bool pending_gpd;

	md_ctrl = md_cd_get(queue->hif_id);
	hw_info = &md_ctrl->hw_info;
	if (!(md_ctrl->txq_active & BIT(queue->index)))
		return;

	/* check if there is any pending TGPD with HWO=1 */
	spin_lock_irqsave(&queue->ring_lock, flags);
	req = cldma_ring_step_backward(queue->tr_ring, queue->tx_xmit);
	tgpd = req->gpd;
	pending_gpd = (tgpd->gpd_flags & GPD_FLAGS_HWO) && req->skb;

	spin_unlock_irqrestore(&queue->ring_lock, flags);

	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	if (pending_gpd) {
		/* Check current processing TGPD
		 * current 64-bit address is in a table by Q index.
		 */
		ul_curr_addr = ioread64(hw_info->ap_pdn_base +
					REG_CLDMA_UL_CURRENT_ADDRL_0 +
					queue->index * sizeof(u64));
		if (req->gpd_addr != ul_curr_addr)
			dev_err(md_ctrl->dev,
				"CLDMA%d Q%d TGPD addr, SW:%pad, HW:%pad\n", md_ctrl->hif_id,
				queue->index, &req->gpd_addr, &ul_curr_addr);
		else
			/* retry */
			cldma_hw_resume_queue(&md_ctrl->hw_info, queue->index, false);
	}

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
}

static void cldma_tx_done(struct work_struct *work)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	struct cldma_queue *queue;
	unsigned int l2_tx_int;
	unsigned long flags;

	queue = container_of(work, struct cldma_queue, cldma_tx_work);
	md_ctrl = md_cd_get(queue->hif_id);
	hw_info = &md_ctrl->hw_info;
	queue->tr_ring->handle_tx_done(queue);

	/* greedy mode */
	l2_tx_int = cldma_hw_int_status(hw_info, BIT(queue->index) | EQ_STA_BIT(queue->index),
					false);
	if (l2_tx_int & EQ_STA_BIT(queue->index)) {
		cldma_hw_tx_done(hw_info, EQ_STA_BIT(queue->index));
		cldma_tx_queue_empty_handler(queue);
	}

	if (l2_tx_int & BIT(queue->index)) {
		cldma_hw_tx_done(hw_info, BIT(queue->index));
		queue_work(queue->worker, &queue->cldma_tx_work);
		return;
	}

	/* enable TX_DONE interrupt */
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	if (md_ctrl->txq_active & BIT(queue->index)) {
		cldma_clear_ip_busy(hw_info);
		cldma_hw_dismask_eqirq(hw_info, queue->index, false);
		cldma_hw_dismask_txrxirq(hw_info, queue->index, false);
	}

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	pm_runtime_mark_last_busy(md_ctrl->dev);
	pm_runtime_put_autosuspend(md_ctrl->dev);
}

static void cldma_ring_free(struct cldma_ctrl *md_ctrl,
			    struct cldma_ring *ring, enum dma_data_direction dir)
{
	struct cldma_request *req_cur, *req_next;

	list_for_each_entry_safe(req_cur, req_next, &ring->gpd_ring, entry) {
		if (req_cur->mapped_buff && req_cur->skb) {
			dma_unmap_single(md_ctrl->dev, req_cur->mapped_buff,
					 skb_data_size(req_cur->skb), dir);
			req_cur->mapped_buff = 0;
		}

		if (req_cur->skb)
			ccci_free_skb(&md_ctrl->mtk_dev->pools, req_cur->skb);

		if (req_cur->gpd)
			dma_pool_free(md_ctrl->gpd_dmapool, req_cur->gpd,
				      req_cur->gpd_addr);

		list_del(&req_cur->entry);
		kfree_sensitive(req_cur);
	}
}

static struct cldma_request *alloc_rx_request(struct cldma_ctrl *md_ctrl, size_t pkt_size)
{
	struct cldma_request *item;
	unsigned long flags;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return NULL;

	item->skb = ccci_alloc_skb_from_pool(&md_ctrl->mtk_dev->pools, pkt_size, GFS_BLOCKING);
	if (!item->skb)
		goto err_skb_alloc;

	item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL | __GFP_ZERO,
				   &item->gpd_addr);
	if (!item->gpd)
		goto err_gpd_alloc;

	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	item->mapped_buff = dma_map_single(md_ctrl->dev, item->skb->data,
					   skb_data_size(item->skb), DMA_FROM_DEVICE);
	if (dma_mapping_error(md_ctrl->dev, item->mapped_buff)) {
		dev_err(md_ctrl->dev, "DMA mapping failed\n");
		item->mapped_buff = 0;
		spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
		goto err_dma_map;
	}

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	return item;

err_dma_map:
	dma_pool_free(md_ctrl->gpd_dmapool, item->gpd, item->gpd_addr);
err_gpd_alloc:
	ccci_free_skb(&md_ctrl->mtk_dev->pools, item->skb);
err_skb_alloc:
	kfree(item);
	return NULL;
}

static int cldma_rx_ring_init(struct cldma_ctrl *md_ctrl, struct cldma_ring *ring)
{
	struct cldma_request *item, *first_item = NULL;
	struct cldma_rgpd *prev_gpd, *gpd = NULL;
	int i;

	for (i = 0; i < ring->length; i++) {
		item = alloc_rx_request(md_ctrl, ring->pkt_size);
		if (!item) {
			cldma_ring_free(md_ctrl, ring, DMA_FROM_DEVICE);
			return -ENOMEM;
		}

		gpd = (struct cldma_rgpd *)item->gpd;
		cldma_rgpd_set_data_ptr(gpd, item->mapped_buff);
		gpd->data_allow_len = ring->pkt_size;
		gpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		if (!i)
			first_item = item;
		else
			cldma_rgpd_set_next_ptr(prev_gpd, item->gpd_addr);

		INIT_LIST_HEAD(&item->entry);
		list_add_tail(&item->entry, &ring->gpd_ring);
		prev_gpd = gpd;
	}

	if (first_item)
		cldma_rgpd_set_next_ptr(gpd, first_item->gpd_addr);

	return 0;
}

static struct cldma_request *alloc_tx_request(struct cldma_ctrl *md_ctrl)
{
	struct cldma_request *item;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return NULL;

	item->gpd = dma_pool_alloc(md_ctrl->gpd_dmapool, GFP_KERNEL | __GFP_ZERO,
				   &item->gpd_addr);
	if (!item->gpd) {
		kfree(item);
		return NULL;
	}

	return item;
}

static int cldma_tx_ring_init(struct cldma_ctrl *md_ctrl, struct cldma_ring *ring)
{
	struct cldma_request *item, *first_item = NULL;
	struct cldma_tgpd *tgpd, *prev_gpd;
	int i;

	for (i = 0; i < ring->length; i++) {
		item = alloc_tx_request(md_ctrl);
		if (!item) {
			cldma_ring_free(md_ctrl, ring, DMA_TO_DEVICE);
			return -ENOMEM;
		}

		tgpd = item->gpd;
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

	return 0;
}

static void cldma_queue_switch_ring(struct cldma_queue *queue)
{
	struct cldma_ctrl *md_ctrl;
	struct cldma_request *req;

	md_ctrl = md_cd_get(queue->hif_id);

	if (queue->dir == MTK_OUT) {
		queue->tr_ring = &md_ctrl->tx_ring[queue->index];
		req = list_first_entry(&queue->tr_ring->gpd_ring, struct cldma_request, entry);
		queue->tr_done = req;
		queue->tx_xmit = req;
		queue->budget = queue->tr_ring->length;
	} else if (queue->dir == MTK_IN) {
		queue->tr_ring = &md_ctrl->rx_ring[queue->index];
		req = list_first_entry(&queue->tr_ring->gpd_ring, struct cldma_request, entry);
		queue->tr_done = req;
		queue->rx_refill = req;
		queue->budget = queue->tr_ring->length;
	}
}

static void cldma_rx_queue_init(struct cldma_queue *queue)
{
	queue->dir = MTK_IN;
	cldma_queue_switch_ring(queue);
	queue->q_type = rxq_type[queue->index];
}

static void cldma_tx_queue_init(struct cldma_queue *queue)
{
	queue->dir = MTK_OUT;
	cldma_queue_switch_ring(queue);
	queue->q_type = txq_type[queue->index];
}

static inline void cldma_enable_irq(struct cldma_ctrl *md_ctrl)
{
	mtk_pcie_mac_set_int(md_ctrl->mtk_dev, md_ctrl->hw_info.phy_interrupt_id);
}

static inline void cldma_disable_irq(struct cldma_ctrl *md_ctrl)
{
	mtk_pcie_mac_clear_int(md_ctrl->mtk_dev, md_ctrl->hw_info.phy_interrupt_id);
}

static void cldma_irq_work_cb(struct cldma_ctrl *md_ctrl)
{
	u32 l2_tx_int_msk, l2_rx_int_msk, l2_tx_int, l2_rx_int, val;
	struct cldma_hw_info *hw_info = &md_ctrl->hw_info;
	int i;

	/* L2 raw interrupt status */
	l2_tx_int = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2TISAR0);
	l2_rx_int = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2RISAR0);
	l2_tx_int_msk = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2TIMR0);
	l2_rx_int_msk = ioread32(hw_info->ap_ao_base + REG_CLDMA_L2RIMR0);

	l2_tx_int &= ~l2_tx_int_msk;
	l2_rx_int &= ~l2_rx_int_msk;

	if (l2_tx_int) {
		if (l2_tx_int & (TQ_ERR_INT_BITMASK | TQ_ACTIVE_START_ERR_INT_BITMASK)) {
			/* read and clear L3 TX interrupt status */
			val = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3TISAR0);
			iowrite32(val, hw_info->ap_pdn_base + REG_CLDMA_L3TISAR0);
			val = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3TISAR1);
			iowrite32(val, hw_info->ap_pdn_base + REG_CLDMA_L3TISAR1);
		}

		cldma_hw_tx_done(hw_info, l2_tx_int);

		if (l2_tx_int & (TXRX_STATUS_BITMASK | EMPTY_STATUS_BITMASK)) {
			for (i = 0; i < CLDMA_TXQ_NUM; i++) {
				if (l2_tx_int & BIT(i)) {
					pm_runtime_get(md_ctrl->dev);
					/* disable TX_DONE interrupt */
					cldma_hw_mask_eqirq(hw_info, i, false);
					cldma_hw_mask_txrxirq(hw_info, i, false);
					queue_work(md_ctrl->txq[i].worker,
						   &md_ctrl->txq[i].cldma_tx_work);
				}

				if (l2_tx_int & EQ_STA_BIT(i))
					cldma_tx_queue_empty_handler(&md_ctrl->txq[i]);
			}
		}
	}

	if (l2_rx_int) {
		/* clear IP busy register wake up CPU case */
		if (l2_rx_int & (RQ_ERR_INT_BITMASK | RQ_ACTIVE_START_ERR_INT_BITMASK)) {
			/* read and clear L3 RX interrupt status */
			val = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3RISAR0);
			iowrite32(val, hw_info->ap_pdn_base + REG_CLDMA_L3RISAR0);
			val = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L3RISAR1);
			iowrite32(val, hw_info->ap_pdn_base + REG_CLDMA_L3RISAR1);
		}

		cldma_hw_rx_done(hw_info, l2_rx_int);

		if (l2_rx_int & (TXRX_STATUS_BITMASK | EMPTY_STATUS_BITMASK)) {
			for (i = 0; i < CLDMA_RXQ_NUM; i++) {
				if (l2_rx_int & (BIT(i) | EQ_STA_BIT(i))) {
					pm_runtime_get(md_ctrl->dev);
					/* disable RX_DONE and QUEUE_EMPTY interrupt */
					cldma_hw_mask_eqirq(hw_info, i, true);
					cldma_hw_mask_txrxirq(hw_info, i, true);
					queue_work(md_ctrl->rxq[i].worker,
						   &md_ctrl->rxq[i].cldma_rx_work);
				}
			}
		}
	}
}

static bool queues_active(struct cldma_hw_info *hw_info)
{
	unsigned int tx_active;
	unsigned int rx_active;

	tx_active = cldma_hw_queue_status(hw_info, CLDMA_ALL_Q, false);
	rx_active = cldma_hw_queue_status(hw_info, CLDMA_ALL_Q, true);

	return ((tx_active || rx_active) && tx_active != CLDMA_ALL_Q && rx_active != CLDMA_ALL_Q);
}

/**
 * cldma_stop() - Stop CLDMA
 * @hif_id: CLDMA ID (ID_CLDMA0 or ID_CLDMA1)
 *
 * stop TX RX queue
 * disable L1 and L2 interrupt
 * clear TX/RX empty interrupt status
 *
 * Return: 0 on success, a negative error code on failure
 */
int cldma_stop(enum cldma_id hif_id)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	bool active;
	int i;

	md_ctrl = md_cd_get(hif_id);
	if (!md_ctrl)
		return -EINVAL;

	hw_info = &md_ctrl->hw_info;

	/* stop TX/RX queue */
	md_ctrl->rxq_active = 0;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, true);
	md_ctrl->txq_active = 0;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, false);
	md_ctrl->txq_started = 0;

	/* disable L1 and L2 interrupt */
	cldma_disable_irq(md_ctrl);
	cldma_hw_stop(hw_info, true);
	cldma_hw_stop(hw_info, false);

	/* clear TX/RX empty interrupt status */
	cldma_hw_tx_done(hw_info, CLDMA_L2TISAR0_ALL_INT_MASK);
	cldma_hw_rx_done(hw_info, CLDMA_L2RISAR0_ALL_INT_MASK);

	if (md_ctrl->is_late_init) {
		for (i = 0; i < CLDMA_TXQ_NUM; i++)
			flush_work(&md_ctrl->txq[i].cldma_tx_work);

		for (i = 0; i < CLDMA_RXQ_NUM; i++)
			flush_work(&md_ctrl->rxq[i].cldma_rx_work);
	}

	if (read_poll_timeout(queues_active, active, !active, CHECK_Q_STOP_STEP_US,
			      CHECK_Q_STOP_TIMEOUT_US, true, hw_info)) {
		dev_err(md_ctrl->dev, "Could not stop CLDMA%d queues", hif_id);
		return -EAGAIN;
	}

	return 0;
}

static void cldma_late_release(struct cldma_ctrl *md_ctrl)
{
	int i;

	if (md_ctrl->is_late_init) {
		/* free all TX/RX CLDMA request/GBD/skb buffer */
		for (i = 0; i < CLDMA_TXQ_NUM; i++)
			cldma_ring_free(md_ctrl, &md_ctrl->tx_ring[i], DMA_TO_DEVICE);

		for (i = 0; i < CLDMA_RXQ_NUM; i++)
			cldma_ring_free(md_ctrl, &md_ctrl->rx_ring[i], DMA_FROM_DEVICE);

		dma_pool_destroy(md_ctrl->gpd_dmapool);
		md_ctrl->gpd_dmapool = NULL;
		md_ctrl->is_late_init = false;
	}
}

void cldma_reset(enum cldma_id hif_id)
{
	struct cldma_ctrl *md_ctrl;
	struct mtk_modem *md;
	unsigned long flags;
	int i;

	md_ctrl = md_cd_get(hif_id);
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	md_ctrl->hif_id = hif_id;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	md = md_ctrl->mtk_dev->md;

	cldma_disable_irq(md_ctrl);
	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);

	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		md_ctrl->txq[i].md = md;
		cancel_work_sync(&md_ctrl->txq[i].cldma_tx_work);
		spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
		md_cd_queue_struct_reset(&md_ctrl->txq[i], md_ctrl->hif_id, MTK_OUT, i);
		spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	}

	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		md_ctrl->rxq[i].md = md;
		cancel_work_sync(&md_ctrl->rxq[i].cldma_rx_work);
		spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
		md_cd_queue_struct_reset(&md_ctrl->rxq[i], md_ctrl->hif_id, MTK_IN, i);
		spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	}

	cldma_late_release(md_ctrl);
}

/**
 * cldma_start() - Start CLDMA
 * @hif_id: CLDMA ID (ID_CLDMA0 or ID_CLDMA1)
 *
 * set TX/RX start address
 * start all RX queues and enable L2 interrupt
 *
 */
void cldma_start(enum cldma_id hif_id)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	unsigned long flags;

	md_ctrl = md_cd_get(hif_id);
	hw_info = &md_ctrl->hw_info;

	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	if (md_ctrl->is_late_init) {
		int i;

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

		/* start all RX queues and enable L2 interrupt */
		cldma_hw_start_queue(hw_info, 0xff, 1);
		cldma_hw_start(hw_info);
		/* wait write done */
		wmb();
		md_ctrl->txq_started = 0;
		md_ctrl->txq_active |= TXRX_STATUS_BITMASK;
		md_ctrl->rxq_active |= TXRX_STATUS_BITMASK;
	}

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
}

static void clear_txq(struct cldma_ctrl *md_ctrl, int qnum)
{
	struct cldma_request *req;
	struct cldma_queue *txq;
	struct cldma_tgpd *tgpd;
	unsigned long flags;

	txq = &md_ctrl->txq[qnum];
	spin_lock_irqsave(&txq->ring_lock, flags);
	req = list_first_entry(&txq->tr_ring->gpd_ring, struct cldma_request, entry);
	txq->tr_done = req;
	txq->tx_xmit = req;
	txq->budget = txq->tr_ring->length;
	list_for_each_entry(req, &txq->tr_ring->gpd_ring, entry) {
		tgpd = req->gpd;
		tgpd->gpd_flags &= ~GPD_FLAGS_HWO;
		cldma_tgpd_set_data_ptr(tgpd, 0);
		tgpd->data_buff_len = 0;
		if (req->skb) {
			ccci_free_skb(&md_ctrl->mtk_dev->pools, req->skb);
			req->skb = NULL;
		}
	}

	spin_unlock_irqrestore(&txq->ring_lock, flags);
}

static int clear_rxq(struct cldma_ctrl *md_ctrl, int qnum)
{
	struct cldma_request *req;
	struct cldma_queue *rxq;
	struct cldma_rgpd *rgpd;
	unsigned long flags;

	rxq = &md_ctrl->rxq[qnum];
	spin_lock_irqsave(&rxq->ring_lock, flags);
	req = list_first_entry(&rxq->tr_ring->gpd_ring, struct cldma_request, entry);
	rxq->tr_done = req;
	rxq->rx_refill = req;
	list_for_each_entry(req, &rxq->tr_ring->gpd_ring, entry) {
		rgpd = req->gpd;
		rgpd->gpd_flags = GPD_FLAGS_IOC | GPD_FLAGS_HWO;
		rgpd->data_buff_len = 0;
		if (req->skb) {
			req->skb->len = 0;
			skb_reset_tail_pointer(req->skb);
		}
	}

	spin_unlock_irqrestore(&rxq->ring_lock, flags);
	list_for_each_entry(req, &rxq->tr_ring->gpd_ring, entry) {
		rgpd = req->gpd;
		if (req->skb)
			continue;

		req->skb = ccci_alloc_skb_from_pool(&md_ctrl->mtk_dev->pools,
						    rxq->tr_ring->pkt_size, GFS_BLOCKING);
		if (!req->skb) {
			dev_err(md_ctrl->dev, "skb not allocated\n");
			return -ENOMEM;
		}

		req->mapped_buff = dma_map_single(md_ctrl->dev, req->skb->data,
						  skb_data_size(req->skb), DMA_FROM_DEVICE);
		if (dma_mapping_error(md_ctrl->dev, req->mapped_buff)) {
			dev_err(md_ctrl->dev, "DMA mapping failed\n");
			return -ENOMEM;
		}

		cldma_rgpd_set_data_ptr(rgpd, req->mapped_buff);
	}

	return 0;
}

/* only allowed when CLDMA is stopped */
static void cldma_clear_all_queue(struct cldma_ctrl *md_ctrl, enum direction dir)
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

static void cldma_stop_queue(struct cldma_ctrl *md_ctrl, unsigned char qno, enum direction dir)
{
	struct cldma_hw_info *hw_info;
	unsigned long flags;

	hw_info = &md_ctrl->hw_info;
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
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

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
}

/* this is called inside queue->ring_lock */
static int cldma_gpd_handle_tx_request(struct cldma_queue *queue, struct cldma_request *tx_req,
				       struct sk_buff *skb, unsigned int ioc_override)
{
	struct cldma_ctrl *md_ctrl;
	struct cldma_tgpd *tgpd;
	unsigned long flags;

	md_ctrl = md_cd_get(queue->hif_id);
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
		dev_err(md_ctrl->dev, "DMA mapping failed\n");
		return -ENOMEM;
	}

	cldma_tgpd_set_data_ptr(tgpd, tx_req->mapped_buff);
	tgpd->data_buff_len = skb->len;

	/* Set HWO. Use cldma_lock to avoid race condition
	 * with cldma_stop. This lock must cover TGPD setting,
	 * as even without a resume operation.
	 * CLDMA still can start sending next HWO=1,
	 * if last TGPD was just finished.
	 */
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	if (md_ctrl->txq_active & BIT(queue->index))
		tgpd->gpd_flags |= GPD_FLAGS_HWO;

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	/* mark cldma_request as available */
	tx_req->skb = skb;

	return 0;
}

static void cldma_hw_start_send(struct cldma_ctrl *md_ctrl, u8 qno)
{
	struct cldma_hw_info *hw_info;
	struct cldma_request *req;

	hw_info = &md_ctrl->hw_info;

	/* check whether the device was powered off (CLDMA start address is not set) */
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

int cldma_write_room(enum cldma_id hif_id, unsigned char qno)
{
	struct cldma_ctrl *md_ctrl;
	struct cldma_queue *queue;

	md_ctrl = md_cd_get(hif_id);
	queue = &md_ctrl->txq[qno];
	if (!queue)
		return -EINVAL;

	if (likely(!da_down_stage_flag))
		return queue->budget; 
	else {
		if (queue->budget > (MAX_TX_BUDGET - 1))
			return queue->budget;
	}

	return 0;
}

/**
 * cldma_set_recv_skb() - Set the callback to handle RX packets
 * @hif_id: CLDMA ID (ID_CLDMA0 or ID_CLDMA1)
 * @recv_skb: processing callback
 */
void cldma_set_recv_skb(enum cldma_id hif_id,
			int (*recv_skb)(struct cldma_queue *queue, struct sk_buff *skb))
{
	struct cldma_ctrl *md_ctrl = md_cd_get(hif_id);

	md_ctrl->recv_skb = recv_skb;
}

/**
 * cldma_send_skb() - Send control data to modem
 * @hif_id: CLDMA ID (ID_CLDMA0 or ID_CLDMA1)
 * @qno: queue number
 * @skb: socket buffer
 * @skb_from_pool: set to true to reuse skb from the pool
 * @blocking: true for blocking operation
 *
 * Send control packet to modem using a ring buffer.
 * If blocking is set, it will wait for completion.
 *
 * Return: 0 on success, -ENOMEM on allocation failure, -EINVAL on invalid queue request, or
 *         -EBUSY on resource lock failure.
 */
int cldma_send_skb(enum cldma_id hif_id, int qno, struct sk_buff *skb, bool skb_from_pool,
		   bool blocking)
{
	unsigned int ioc_override = 0;
	struct cldma_request *tx_req;
	struct cldma_ctrl *md_ctrl;
	struct cldma_queue *queue;
	unsigned long flags;
	int ret = 0;
	int val;

	md_ctrl = md_cd_get(hif_id);
	val = pm_runtime_resume_and_get(md_ctrl->dev);
	if (val < 0 && val != -EACCES)
		return val;

	mtk_pci_disable_sleep(md_ctrl->mtk_dev);
	if (qno >= CLDMA_TXQ_NUM) {
		ret = -EINVAL;
		goto exit;
	}

	if (skb_from_pool && skb_headroom(skb) == NET_SKB_PAD) {
		struct ccci_buffer_ctrl *buf_ctrl;

		buf_ctrl = skb_push(skb, sizeof(*buf_ctrl));
		if (buf_ctrl->head_magic == CCCI_BUF_MAGIC)
			ioc_override = buf_ctrl->ioc_override;
		skb_pull(skb, sizeof(*buf_ctrl));
	}

	queue = &md_ctrl->txq[qno];

	/* check if queue is active */
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	if (!(md_ctrl->txq_active & BIT(qno))) {
		ret = -EBUSY;
		spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
		goto exit;
	}

	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);

	do {
		spin_lock_irqsave(&queue->ring_lock, flags);
		tx_req = queue->tx_xmit;
		if (queue->budget > 0 && !tx_req->skb) {
			queue->budget--;
			queue->tr_ring->handle_tx_request(queue, tx_req, skb, ioc_override);
			queue->tx_xmit = cldma_ring_step_forward(queue->tr_ring, tx_req);
			spin_unlock_irqrestore(&queue->ring_lock, flags);

			if (!mtk_pci_sleep_disable_complete(md_ctrl->mtk_dev)) {
				ret = -EBUSY;
				break;
			}

			spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
			cldma_hw_start_send(md_ctrl, qno);
			spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
			break;
		}

		spin_unlock_irqrestore(&queue->ring_lock, flags);
		if (!mtk_pci_sleep_disable_complete(md_ctrl->mtk_dev)) {
			ret = -EBUSY;
			break;
		}

		/* check CLDMA status */
		if (!cldma_hw_queue_status(&md_ctrl->hw_info, qno, false)) {
			/* resume channel */
			spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
			cldma_hw_resume_queue(&md_ctrl->hw_info, qno, false);
			spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
		}

		if (!blocking) {
			ret = -EBUSY;
			break;
		}

		ret = wait_event_interruptible_exclusive(queue->req_wq, queue->budget > 0);
		if (ret == -ERESTARTSYS)
			ret = -EINTR;

	} while (!ret);

exit:
	mtk_pci_enable_sleep(md_ctrl->mtk_dev);
	pm_runtime_mark_last_busy(md_ctrl->dev);
	pm_runtime_put_autosuspend(md_ctrl->dev);
	return ret;
}

int cldma_txq_mtu(unsigned char qno)
{
	if (qno >= CLDMA_TXQ_NUM)
		return -EINVAL;

	return txq_buff_size[qno];
}

static void ccci_cldma_adjust_config(unsigned char cfg_id)
{
	int qno;

	/* set default config */
	for (qno = 0; qno < CLDMA_RXQ_NUM; qno++) {
		rxq_buff_size[qno] = MTK_SKB_4K;
		rxq_type[qno] = CLDMA_SHARED_Q;
		rxq_buff_num[qno] = MAX_RX_BUDGET;
	}

	rxq_buff_size[CLDMA_RXQ_NUM - 1] = MTK_SKB_64K;

	for (qno = 0; qno < CLDMA_TXQ_NUM; qno++) {
		txq_buff_size[qno] = MTK_SKB_4K;
		txq_type[qno] = CLDMA_SHARED_Q;
		txq_buff_num[qno] = MAX_TX_BUDGET;
	}

	switch (cfg_id) {
	case HIF_CFG_DEF:
		break;
	case HIF_CFG1:
		rxq_buff_size[7] = MTK_SKB_64K;
		break;
	case HIF_CFG2:
		/*Download Port Configuration*/
		rxq_type[0] = CLDMA_DEDICATED_Q;
		txq_type[0] = CLDMA_DEDICATED_Q;
		txq_buff_size[0] = MTK_SKB_2K;
		rxq_buff_size[0] = MTK_SKB_2K;
		/*Postdump Port Configuration*/
		rxq_type[1] = CLDMA_DEDICATED_Q;
		txq_type[1] = CLDMA_DEDICATED_Q;
		txq_buff_size[1] = MTK_SKB_2K;
		rxq_buff_size[1] = MTK_SKB_2K;
		break;
	default:
		break;
	}
}

/* this function contains longer duration initializations */
static int cldma_late_init(struct cldma_ctrl *md_ctrl, unsigned int cfg_id)
{
	char dma_pool_name[32];
	int i, ret;

	if (md_ctrl->is_late_init) {
		dev_err(md_ctrl->dev, "CLDMA late init was already done\n");
		return -EALREADY;
	}

	mutex_lock(&ctl_cfg_mutex);
	ccci_cldma_adjust_config(cfg_id);
	/* init ring buffers */
	snprintf(dma_pool_name, 32, "cldma_request_dma_hif%d", md_ctrl->hif_id);
	md_ctrl->gpd_dmapool = dma_pool_create(dma_pool_name, md_ctrl->dev,
					       sizeof(struct cldma_tgpd), 16, 0);
	if (!md_ctrl->gpd_dmapool) {
		mutex_unlock(&ctl_cfg_mutex);
		dev_err(md_ctrl->dev, "DMA pool alloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->tx_ring[i].gpd_ring);
		md_ctrl->tx_ring[i].length = txq_buff_num[i];
		md_ctrl->tx_ring[i].handle_tx_request = &cldma_gpd_handle_tx_request;
		md_ctrl->tx_ring[i].handle_tx_done = &cldma_gpd_tx_collect;
		ret = cldma_tx_ring_init(md_ctrl, &md_ctrl->tx_ring[i]);
		if (ret) {
			dev_err(md_ctrl->dev, "control TX ring init fail\n");
			goto err_tx_ring;
		}
	}

	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->rx_ring[i].gpd_ring);
		md_ctrl->rx_ring[i].length = rxq_buff_num[i];
		md_ctrl->rx_ring[i].pkt_size = rxq_buff_size[i];
		md_ctrl->rx_ring[i].handle_rx_done = &cldma_gpd_rx_collect;
		ret = cldma_rx_ring_init(md_ctrl, &md_ctrl->rx_ring[i]);
		if (ret) {
			dev_err(md_ctrl->dev, "control RX ring init fail\n");
			goto err_rx_ring;
		}
	}

	/* init queue */
	for (i = 0; i < CLDMA_TXQ_NUM; i++)
		cldma_tx_queue_init(&md_ctrl->txq[i]);

	for (i = 0; i < CLDMA_RXQ_NUM; i++)
		cldma_rx_queue_init(&md_ctrl->rxq[i]);
	mutex_unlock(&ctl_cfg_mutex);

	md_ctrl->is_late_init = true;
	return 0;

err_rx_ring:
	while (i--)
		cldma_ring_free(md_ctrl, &md_ctrl->rx_ring[i], DMA_FROM_DEVICE);

	i = CLDMA_TXQ_NUM;
err_tx_ring:
	while (i--)
		cldma_ring_free(md_ctrl, &md_ctrl->tx_ring[i], DMA_TO_DEVICE);

	mutex_unlock(&ctl_cfg_mutex);
	return ret;
}

static inline void __iomem *pcie_addr_transfer(void __iomem *addr, u32 addr_trs1, u32 phy_addr)
{
	return addr + phy_addr - addr_trs1;
}

static void hw_info_init(struct cldma_ctrl *md_ctrl)
{
	struct cldma_hw_info *hw_info;
	u32 phy_ao_base = 0, phy_pd_base = 0;
	struct mtk_addr_base *pbase;

	pbase = &md_ctrl->mtk_dev->base_addr;
	hw_info = &md_ctrl->hw_info;

	if (md_ctrl->hif_id == ID_CLDMA1) {
		phy_ao_base = CLDMA1_AO_BASE;
		phy_pd_base = CLDMA1_PD_BASE;
		hw_info->phy_interrupt_id = CLDMA1_INT;
	} else if (md_ctrl->hif_id == ID_CLDMA0) {
		phy_ao_base = CLDMA0_AO_BASE;
		phy_pd_base = CLDMA0_PD_BASE;
		hw_info->phy_interrupt_id = CLDMA0_INT;
	} else {
		dev_err(md_ctrl->dev, "incorrect CLDMA id\n");
 		return;
	}

	hw_info->hw_mode = MODE_BIT_64;
	hw_info->ap_ao_base = pcie_addr_transfer(pbase->pcie_ext_reg_base,
						 pbase->pcie_dev_reg_trsl_addr,
						 phy_ao_base);
	hw_info->ap_pdn_base = pcie_addr_transfer(pbase->pcie_ext_reg_base,
						  pbase->pcie_dev_reg_trsl_addr,
						  phy_pd_base);
}

int cldma_alloc(enum cldma_id hif_id, struct mtk_pci_dev *mtk_dev)
{
	struct cldma_ctrl *md_ctrl;

	md_ctrl = devm_kzalloc(&mtk_dev->pdev->dev, sizeof(*md_ctrl), GFP_KERNEL);
	if (!md_ctrl)
		return -ENOMEM;

	md_ctrl->mtk_dev = mtk_dev;
	md_ctrl->dev = &mtk_dev->pdev->dev;
	md_ctrl->hif_id = hif_id;
	hw_info_init(md_ctrl);
	md_cd_set(hif_id, md_ctrl);
	return 0;
}

/**
 * cldma_exception() - CLDMA exception handler
 * @hif_id: CLDMA ID (ID_CLDMA0 or ID_CLDMA1)
 * @stage: exception stage
 *
 * disable/flush/stop/start CLDMA/queues based on exception stage.
 *
 */
void cldma_exception(enum cldma_id hif_id, enum hif_ex_stage stage)
{
	struct cldma_ctrl *md_ctrl;

	md_ctrl = md_cd_get(hif_id);
	switch (stage) {
	case HIF_EX_INIT:
		/* disable CLDMA TX queues */
		cldma_stop_queue(md_ctrl, CLDMA_ALL_Q, MTK_OUT);
		/* Clear TX queue */
		cldma_clear_all_queue(md_ctrl, MTK_OUT);
		break;

	case HIF_EX_CLEARQ_DONE:
		/* Stop CLDMA, we do not want to get CLDMA IRQ when MD is
		 * resetting CLDMA after it got clearq_ack.
		 */
		cldma_stop_queue(md_ctrl, CLDMA_ALL_Q, MTK_IN);
		/* flush all TX&RX workqueues */
		cldma_stop(hif_id);
		if (md_ctrl->hif_id == ID_CLDMA1)
			cldma_hw_reset(md_ctrl->mtk_dev->base_addr.infracfg_ao_base);

		/* clear the RX queue */
		cldma_clear_all_queue(md_ctrl, MTK_IN);
		break;

	case HIF_EX_ALLQ_RESET:
		cldma_hw_init(&md_ctrl->hw_info);
		cldma_start(hif_id);
		break;

	default:
		break;
	}
}

static void cldma_resume_early(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	unsigned long flags;
	int qno_t;

	md_ctrl = entity_param;
	hw_info = &md_ctrl->hw_info;
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	cldma_hw_restore(hw_info);

	for (qno_t = 0; qno_t < CLDMA_TXQ_NUM; qno_t++) {
		cldma_hw_set_start_address(hw_info, qno_t, md_ctrl->txq[qno_t].tx_xmit->gpd_addr,
					   false);
		cldma_hw_set_start_address(hw_info, qno_t, md_ctrl->rxq[qno_t].tr_done->gpd_addr,
					   true);
	}

	cldma_enable_irq(md_ctrl);
	cldma_hw_start_queue(hw_info, CLDMA_ALL_Q, true);
	md_ctrl->rxq_active |= TXRX_STATUS_BITMASK;
	cldma_hw_dismask_eqirq(hw_info, CLDMA_ALL_Q, true);
	cldma_hw_dismask_txrxirq(hw_info, CLDMA_ALL_Q, true);
	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
}

static int cldma_resume(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct cldma_ctrl *md_ctrl;
	unsigned long flags;

	md_ctrl = entity_param;
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	md_ctrl->txq_active |= TXRX_STATUS_BITMASK;
	cldma_hw_dismask_txrxirq(&md_ctrl->hw_info, CLDMA_ALL_Q, false);
	cldma_hw_dismask_eqirq(&md_ctrl->hw_info, CLDMA_ALL_Q, false);
	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	mhccif_mask_clr(mtk_dev, D2H_SW_INT_MASK);

	return 0;
}

static void cldma_suspend_late(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	unsigned long flags;

	md_ctrl = entity_param;
	hw_info = &md_ctrl->hw_info;
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	cldma_hw_mask_eqirq(hw_info, CLDMA_ALL_Q, true);
	cldma_hw_mask_txrxirq(hw_info, CLDMA_ALL_Q, true);
	md_ctrl->rxq_active &= ~TXRX_STATUS_BITMASK;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, true);
	cldma_clear_ip_busy(hw_info);
	cldma_disable_irq(md_ctrl);
	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
}

static int cldma_suspend(struct mtk_pci_dev *mtk_dev, void *entity_param)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	unsigned long flags;

	md_ctrl = entity_param;
	hw_info = &md_ctrl->hw_info;

	mhccif_mask_set(mtk_dev, D2H_SW_INT_MASK);

	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	cldma_hw_mask_eqirq(hw_info, CLDMA_ALL_Q, false);
	cldma_hw_mask_txrxirq(hw_info, CLDMA_ALL_Q, false);
	md_ctrl->txq_active &= ~TXRX_STATUS_BITMASK;
	cldma_hw_stop_queue(hw_info, CLDMA_ALL_Q, false);
	md_ctrl->txq_started = 0;
	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
	return 0;
}

static int cldma_pm_init(struct cldma_ctrl *md_ctrl)
{
	md_ctrl->pm_entity = kzalloc(sizeof(*md_ctrl->pm_entity), GFP_KERNEL);
	if (!md_ctrl->pm_entity)
		return -ENOMEM;

	md_ctrl->pm_entity->entity_param = md_ctrl;
	if (md_ctrl->hif_id == ID_CLDMA1)
		md_ctrl->pm_entity->id = PM_ENTITY_ID_CTRL1;
	else
		md_ctrl->pm_entity->id = PM_ENTITY_ID_CTRL2;

	md_ctrl->pm_entity->suspend = cldma_suspend;
	md_ctrl->pm_entity->suspend_late = cldma_suspend_late;
	md_ctrl->pm_entity->resume = cldma_resume;
	md_ctrl->pm_entity->resume_early = cldma_resume_early;

	return mtk_pci_pm_entity_register(md_ctrl->mtk_dev, md_ctrl->pm_entity);
}

static int cldma_pm_uninit(struct cldma_ctrl *md_ctrl)
{
	if (!md_ctrl->pm_entity)
		return -EINVAL;

	mtk_pci_pm_entity_unregister(md_ctrl->mtk_dev, md_ctrl->pm_entity);
	kfree_sensitive(md_ctrl->pm_entity);
	md_ctrl->pm_entity = NULL;
	return 0;
}

void cldma_hif_hw_init(enum cldma_id hif_id)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	unsigned long flags;

	md_ctrl = md_cd_get(hif_id);
	spin_lock_irqsave(&md_ctrl->cldma_lock, flags);
	hw_info = &md_ctrl->hw_info;

	/* mask CLDMA interrupt */
	cldma_hw_stop(hw_info, false);
	cldma_hw_stop(hw_info, true);

	/* clear CLDMA interrupt */
	cldma_hw_rx_done(hw_info, EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK);
	cldma_hw_tx_done(hw_info, EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK);

	/* initialize CLDMA hardware */
	cldma_hw_init(hw_info);
	spin_unlock_irqrestore(&md_ctrl->cldma_lock, flags);
}

static irqreturn_t cldma_isr_handler(int irq, void *data)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;

	md_ctrl = data;
	hw_info = &md_ctrl->hw_info;

	mtk_pcie_mac_clear_int(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);
	cldma_irq_work_cb(md_ctrl);
	mtk_pcie_mac_clear_int_status(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);
	mtk_pcie_mac_set_int(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);
	return IRQ_HANDLED;
}

/**
 * cldma_init() - Initialize CLDMA
 * @hif_id: CLDMA ID (ID_CLDMA0 or ID_CLDMA1)
 *
 * allocate and initialize device power management entity
 * initialize HIF TX/RX queue structure
 * register CLDMA callback isr with PCIe driver
 *
 * Return: 0 on success, a negative error code on failure
 */
int cldma_init(enum cldma_id hif_id)
{
	struct cldma_hw_info *hw_info;
	struct cldma_ctrl *md_ctrl;
	struct mtk_modem *md;
	int ret, i;

	md_ctrl = md_cd_get(hif_id);
	md = md_ctrl->mtk_dev->md;
	md_ctrl->hif_id = hif_id;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	md_ctrl->is_late_init = false;
	hw_info = &md_ctrl->hw_info;
	ret = cldma_pm_init(md_ctrl);
	if (ret)
		return ret;

	spin_lock_init(&md_ctrl->cldma_lock);
	/* initialize HIF queue structure */
	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		md_cd_queue_struct_init(&md_ctrl->txq[i], md_ctrl->hif_id, MTK_OUT, i);
		md_ctrl->txq[i].md = md;
		md_ctrl->txq[i].worker =
			alloc_workqueue("md_hif%d_tx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM | (!i ? WQ_HIGHPRI : 0),
					1, hif_id, i);
		if (!md_ctrl->txq[i].worker)
			return -ENOMEM;

		INIT_WORK(&md_ctrl->txq[i].cldma_tx_work, cldma_tx_done);
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

	mtk_pcie_mac_clear_int(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);

	/* registers CLDMA callback ISR with PCIe driver */
	md_ctrl->mtk_dev->intr_handler[hw_info->phy_interrupt_id] = cldma_isr_handler;
	md_ctrl->mtk_dev->intr_thread[hw_info->phy_interrupt_id] = NULL;
	md_ctrl->mtk_dev->callback_param[hw_info->phy_interrupt_id] = md_ctrl;
	mtk_pcie_mac_clear_int_status(md_ctrl->mtk_dev, hw_info->phy_interrupt_id);
	return 0;
}

void cldma_switch_cfg(enum cldma_id hif_id, unsigned int cfg_id)
{
	struct cldma_ctrl *md_ctrl;

	md_ctrl = md_cd_get(hif_id);
	if (md_ctrl) {
		cldma_late_release(md_ctrl);
		cldma_late_init(md_ctrl, cfg_id);
	}
}

void cldma_exit(enum cldma_id hif_id)
{
	struct cldma_ctrl *md_ctrl;
	int i;

	md_ctrl = md_cd_get(hif_id);
	if (!md_ctrl)
		return;

	/* stop CLDMA work */
	cldma_stop(hif_id);
	cldma_late_release(md_ctrl);

	for (i = 0; i < CLDMA_TXQ_NUM; i++) {
		if (md_ctrl->txq[i].worker) {
			destroy_workqueue(md_ctrl->txq[i].worker);
			md_ctrl->txq[i].worker = NULL;
		}
	}

	for (i = 0; i < CLDMA_RXQ_NUM; i++) {
		if (md_ctrl->rxq[i].worker) {
			destroy_workqueue(md_ctrl->rxq[i].worker);
			md_ctrl->rxq[i].worker = NULL;
		}
	}

	cldma_pm_uninit(md_ctrl);
}
