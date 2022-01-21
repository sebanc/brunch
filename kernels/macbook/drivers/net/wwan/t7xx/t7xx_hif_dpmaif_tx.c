// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>

#include "t7xx_common.h"
#include "t7xx_dpmaif.h"
#include "t7xx_hif_dpmaif.h"
#include "t7xx_hif_dpmaif_tx.h"

#define DPMAIF_SKB_TX_BURST_CNT	5
#define DPMAIF_DRB_ENTRY_SIZE	6144

/* DRB dtype */
#define DES_DTYP_PD		0
#define DES_DTYP_MSG		1

static unsigned int dpmaifq_poll_tx_drb(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num)
{
	unsigned short old_sw_rd_idx, new_hw_rd_idx;
	struct dpmaif_tx_queue *txq;
	unsigned int hw_read_idx;
	unsigned int drb_cnt = 0;
	unsigned long flags;

	txq = &dpmaif_ctrl->txq[q_num];
	if (!txq->que_started)
		return drb_cnt;

	old_sw_rd_idx = txq->drb_rd_idx;

	hw_read_idx = dpmaif_ul_get_ridx(&dpmaif_ctrl->hif_hw_info, q_num);

	new_hw_rd_idx = hw_read_idx / DPMAIF_UL_DRB_ENTRY_WORD;

	if (new_hw_rd_idx >= DPMAIF_DRB_ENTRY_SIZE) {
		dev_err(dpmaif_ctrl->dev, "out of range read index: %u\n", new_hw_rd_idx);
		return 0;
	}

	if (old_sw_rd_idx <= new_hw_rd_idx)
		drb_cnt = new_hw_rd_idx - old_sw_rd_idx;
	else
		drb_cnt = txq->drb_size_cnt - old_sw_rd_idx + new_hw_rd_idx;

	spin_lock_irqsave(&txq->tx_lock, flags);
	txq->drb_rd_idx = new_hw_rd_idx;
	spin_unlock_irqrestore(&txq->tx_lock, flags);
	return drb_cnt;
}

static unsigned short dpmaif_release_tx_buffer(struct dpmaif_ctrl *dpmaif_ctrl,
					       unsigned char q_num, unsigned int release_cnt)
{
	struct dpmaif_drb_skb *cur_drb_skb, *drb_skb_base;
	struct dpmaif_drb_pd *cur_drb, *drb_base;
	struct dpmaif_tx_queue *txq;
	struct dpmaif_callbacks *cb;
	unsigned int drb_cnt, i;
	unsigned short cur_idx;
	unsigned long flags;

	if (!release_cnt)
		return 0;

	txq = &dpmaif_ctrl->txq[q_num];
	drb_skb_base = txq->drb_skb_base;
	drb_base = txq->drb_base;
	cb = dpmaif_ctrl->callbacks;

	spin_lock_irqsave(&txq->tx_lock, flags);
	drb_cnt = txq->drb_size_cnt;
	cur_idx = txq->drb_release_rd_idx;
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	for (i = 0; i < release_cnt; i++) {
		cur_drb = drb_base + cur_idx;
		if (FIELD_GET(DRB_PD_DTYP, cur_drb->header) == DES_DTYP_PD) {
			cur_drb_skb = drb_skb_base + cur_idx;
			if (!(FIELD_GET(DRB_SKB_IS_MSG, cur_drb_skb->config))) {
				dma_unmap_single(dpmaif_ctrl->dev, cur_drb_skb->bus_addr,
						 cur_drb_skb->data_len, DMA_TO_DEVICE);
			}

			if (!FIELD_GET(DRB_PD_CONT, cur_drb->header)) {
				if (!cur_drb_skb->skb) {
					dev_err(dpmaif_ctrl->dev,
						"txq%u: DRB check fail, invalid skb\n", q_num);
					continue;
				}

				dev_kfree_skb_any(cur_drb_skb->skb);
			}

			cur_drb_skb->skb = NULL;
		} else {
			txq->last_ch_id = FIELD_GET(DRB_MSG_CHANNEL_ID,
						    ((struct dpmaif_drb_msg *)cur_drb)->header_dw2);
		}

		spin_lock_irqsave(&txq->tx_lock, flags);
		cur_idx = ring_buf_get_next_wrdx(drb_cnt, cur_idx);
		txq->drb_release_rd_idx = cur_idx;
		spin_unlock_irqrestore(&txq->tx_lock, flags);

		if (atomic_inc_return(&txq->tx_budget) > txq->drb_size_cnt / 8)
			cb->state_notify(dpmaif_ctrl->mtk_dev,
					 DMPAIF_TXQ_STATE_IRQ, txq->index);
	}

	if (FIELD_GET(DRB_PD_CONT, cur_drb->header))
		dev_err(dpmaif_ctrl->dev, "txq%u: DRB not marked as the last one\n", q_num);

	return i;
}

static int dpmaif_tx_release(struct dpmaif_ctrl *dpmaif_ctrl,
			     unsigned char q_num, unsigned int budget)
{
	unsigned int rel_cnt, real_rel_cnt;
	struct dpmaif_tx_queue *txq;

	txq = &dpmaif_ctrl->txq[q_num];

	/* update rd idx: from HW */
	dpmaifq_poll_tx_drb(dpmaif_ctrl, q_num);
	/* release the readable pkts */
	rel_cnt = ring_buf_read_write_count(txq->drb_size_cnt, txq->drb_release_rd_idx,
					    txq->drb_rd_idx, true);

	real_rel_cnt = min_not_zero(budget, rel_cnt);

	/* release data buff */
	if (real_rel_cnt)
		real_rel_cnt = dpmaif_release_tx_buffer(dpmaif_ctrl, q_num, real_rel_cnt);

	return ((real_rel_cnt < rel_cnt) ? -EAGAIN : 0);
}

/* return false indicates there are remaining spurious interrupts */
static inline bool dpmaif_no_remain_spurious_tx_done_intr(struct dpmaif_tx_queue *txq)
{
	return (dpmaifq_poll_tx_drb(txq->dpmaif_ctrl, txq->index) > 0);
}

static void dpmaif_tx_done(struct work_struct *work)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_tx_queue *txq;
	int ret;

	txq = container_of(work, struct dpmaif_tx_queue, dpmaif_tx_work);
	dpmaif_ctrl = txq->dpmaif_ctrl;

	ret = pm_runtime_resume_and_get(dpmaif_ctrl->dev);
	if (ret < 0 && ret != -EACCES)
		return;

	/* The device may be in low power state. disable sleep if needed */
	mtk_pci_disable_sleep(dpmaif_ctrl->mtk_dev);

	/* ensure that we are not in deep sleep */
	if (mtk_pci_sleep_disable_complete(dpmaif_ctrl->mtk_dev)) {
		ret = dpmaif_tx_release(dpmaif_ctrl, txq->index, txq->drb_size_cnt);
		if (ret == -EAGAIN ||
		    (dpmaif_hw_check_clr_ul_done_status(&dpmaif_ctrl->hif_hw_info, txq->index) &&
		     dpmaif_no_remain_spurious_tx_done_intr(txq))) {
			queue_work(dpmaif_ctrl->txq[txq->index].worker,
				   &dpmaif_ctrl->txq[txq->index].dpmaif_tx_work);
			/* clear IP busy to give the device time to enter the low power state */
			dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
		} else {
			dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
			dpmaif_unmask_ulq_interrupt(dpmaif_ctrl, txq->index);
		}
	}

	mtk_pci_enable_sleep(dpmaif_ctrl->mtk_dev);
	pm_runtime_mark_last_busy(dpmaif_ctrl->dev);
	pm_runtime_put_autosuspend(dpmaif_ctrl->dev);
}

static void set_drb_msg(struct dpmaif_ctrl *dpmaif_ctrl,
			unsigned char q_num, unsigned short cur_idx,
			unsigned int pkt_len, unsigned short count_l,
			unsigned char channel_id, __be16 network_type)
{
	struct dpmaif_drb_msg *drb_base;
	struct dpmaif_drb_msg *drb;

	drb_base = dpmaif_ctrl->txq[q_num].drb_base;
	drb = drb_base + cur_idx;

	drb->header_dw1 = FIELD_PREP(DRB_MSG_DTYP, DES_DTYP_MSG);
	drb->header_dw1 |= FIELD_PREP(DRB_MSG_CONT, 1);
	drb->header_dw1 |= FIELD_PREP(DRB_MSG_PACKET_LEN, pkt_len);

	drb->header_dw2 = FIELD_PREP(DRB_MSG_COUNT_L, count_l);
	drb->header_dw2 |= FIELD_PREP(DRB_MSG_CHANNEL_ID, channel_id);
	drb->header_dw2 |= FIELD_PREP(DRB_MSG_L4_CHK, 1);
}

static void set_drb_payload(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num,
			    unsigned short cur_idx, dma_addr_t data_addr,
			    unsigned int pkt_size, char last_one)
{
	struct dpmaif_drb_pd *drb_base;
	struct dpmaif_drb_pd *drb;

	drb_base = dpmaif_ctrl->txq[q_num].drb_base;
	drb = drb_base + cur_idx;

	drb->header &= ~DRB_PD_DTYP;
	drb->header |= FIELD_PREP(DRB_PD_DTYP, DES_DTYP_PD);

	drb->header &= ~DRB_PD_CONT;
	if (!last_one)
		drb->header |= FIELD_PREP(DRB_PD_CONT, 1);

	drb->header &= ~DRB_PD_DATA_LEN;
	drb->header |= FIELD_PREP(DRB_PD_DATA_LEN, pkt_size);
	drb->p_data_addr = lower_32_bits(data_addr);
	drb->data_addr_ext = upper_32_bits(data_addr);
}

static void record_drb_skb(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num,
			   unsigned short cur_idx, struct sk_buff *skb, unsigned short is_msg,
			   bool is_frag, bool is_last_one, dma_addr_t bus_addr,
			   unsigned int data_len)
{
	struct dpmaif_drb_skb *drb_skb_base;
	struct dpmaif_drb_skb *drb_skb;

	drb_skb_base = dpmaif_ctrl->txq[q_num].drb_skb_base;
	drb_skb = drb_skb_base + cur_idx;

	drb_skb->skb = skb;
	drb_skb->bus_addr = bus_addr;
	drb_skb->data_len = data_len;
	drb_skb->config = FIELD_PREP(DRB_SKB_DRB_IDX, cur_idx);
	drb_skb->config |= FIELD_PREP(DRB_SKB_IS_MSG, is_msg);
	drb_skb->config |= FIELD_PREP(DRB_SKB_IS_FRAG, is_frag);
	drb_skb->config |= FIELD_PREP(DRB_SKB_IS_LAST, is_last_one);
}

static int dpmaif_tx_send_skb_on_tx_thread(struct dpmaif_ctrl *dpmaif_ctrl,
					   struct dpmaif_tx_event *event)
{
	unsigned int wt_cnt, send_cnt, payload_cnt;
	struct skb_shared_info *info;
	struct dpmaif_tx_queue *txq;
	int drb_wr_idx_backup = -1;
	struct ccci_header ccci_h;
	bool is_frag, is_last_one;
	bool skb_pulled = false;
	unsigned short cur_idx;
	unsigned int data_len;
	int qno = event->qno;
	dma_addr_t bus_addr;
	unsigned long flags;
	struct sk_buff *skb;
	void *data_addr;
	int ret = 0;

	skb = event->skb;
	txq = &dpmaif_ctrl->txq[qno];
	if (!txq->que_started || dpmaif_ctrl->state != DPMAIF_STATE_PWRON)
		return -ENODEV;

	atomic_set(&txq->tx_processing, 1);

	 /* Ensure tx_processing is changed to 1 before actually begin TX flow */
	smp_mb();

	info = skb_shinfo(skb);

	if (info->frag_list)
		dev_err(dpmaif_ctrl->dev, "ulq%d skb frag_list not supported!\n", qno);

	payload_cnt = info->nr_frags + 1;
	/* nr_frags: frag cnt, 1: skb->data, 1: msg DRB */
	send_cnt = payload_cnt + 1;

	ccci_h = *(struct ccci_header *)skb->data;
	skb_pull(skb, sizeof(struct ccci_header));
	skb_pulled = true;

	spin_lock_irqsave(&txq->tx_lock, flags);
	/* update the drb_wr_idx */
	cur_idx = txq->drb_wr_idx;
	drb_wr_idx_backup = cur_idx;
	txq->drb_wr_idx += send_cnt;
	if (txq->drb_wr_idx >= txq->drb_size_cnt)
		txq->drb_wr_idx -= txq->drb_size_cnt;

	/* 3 send data. */
	/* 3.1 a msg drb first, then payload DRB. */
	set_drb_msg(dpmaif_ctrl, txq->index, cur_idx, skb->len, 0, ccci_h.data[0], skb->protocol);
	record_drb_skb(dpmaif_ctrl, txq->index, cur_idx, skb, 1, 0, 0, 0, 0);
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	cur_idx = ring_buf_get_next_wrdx(txq->drb_size_cnt, cur_idx);

	/* 3.2 DRB payload: skb->data + frags[] */
	for (wt_cnt = 0; wt_cnt < payload_cnt; wt_cnt++) {
		/* get data_addr && data_len */
		if (!wt_cnt) {
			data_len = skb_headlen(skb);
			data_addr = skb->data;
			is_frag = false;
		} else {
			skb_frag_t *frag = info->frags + wt_cnt - 1;

			data_len = skb_frag_size(frag);
			data_addr = skb_frag_address(frag);
			is_frag = true;
		}

		if (wt_cnt == payload_cnt - 1)
			is_last_one = true;
		else
			/* set 0~(n-1) DRB, maybe none */
			is_last_one = false;

		/* tx mapping */
		bus_addr = dma_map_single(dpmaif_ctrl->dev, data_addr, data_len, DMA_TO_DEVICE);
		if (dma_mapping_error(dpmaif_ctrl->dev, bus_addr)) {
			dev_err(dpmaif_ctrl->dev, "DMA mapping fail\n");
			ret = -ENOMEM;
			break;
		}

		spin_lock_irqsave(&txq->tx_lock, flags);
		set_drb_payload(dpmaif_ctrl, txq->index, cur_idx, bus_addr, data_len,
				is_last_one);
		record_drb_skb(dpmaif_ctrl, txq->index, cur_idx, skb, 0, is_frag,
			       is_last_one, bus_addr, data_len);
		spin_unlock_irqrestore(&txq->tx_lock, flags);

		cur_idx = ring_buf_get_next_wrdx(txq->drb_size_cnt, cur_idx);
	}

	if (ret < 0) {
		atomic_set(&txq->tx_processing, 0);
		dev_err(dpmaif_ctrl->dev,
			"send fail: drb_wr_idx_backup:%d, ret:%d\n", drb_wr_idx_backup, ret);

		if (skb_pulled)
			skb_push(skb, sizeof(struct ccci_header));

		if (drb_wr_idx_backup >= 0) {
			spin_lock_irqsave(&txq->tx_lock, flags);
			txq->drb_wr_idx = drb_wr_idx_backup;
			spin_unlock_irqrestore(&txq->tx_lock, flags);
		}
	} else {
		atomic_sub(send_cnt, &txq->tx_budget);
		atomic_set(&txq->tx_processing, 0);
	}

	return ret;
}

/* must be called within protection of event_lock */
static inline void dpmaif_finish_event(struct dpmaif_tx_event *event)
{
	list_del(&event->entry);
	kfree(event);
}

static bool tx_lists_are_all_empty(const struct dpmaif_ctrl *dpmaif_ctrl)
{
	int i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		if (!list_empty(&dpmaif_ctrl->txq[i].tx_event_queue))
			return false;
	}

	return true;
}

static int select_tx_queue(struct dpmaif_ctrl *dpmaif_ctrl)
{
	unsigned char txq_prio[TXQ_TYPE_CNT] = {TXQ_FAST, TXQ_NORMAL};
	unsigned char txq_id, i;

	for (i = 0; i < TXQ_TYPE_CNT; i++) {
		txq_id = txq_prio[dpmaif_ctrl->txq_select_times % TXQ_TYPE_CNT];
		if (!dpmaif_ctrl->txq[txq_id].que_started)
			break;

		if (!list_empty(&dpmaif_ctrl->txq[txq_id].tx_event_queue)) {
			dpmaif_ctrl->txq_select_times++;
			return txq_id;
		}

		dpmaif_ctrl->txq_select_times++;
	}

	return -EAGAIN;
}

static int txq_burst_send_skb(struct dpmaif_tx_queue *txq)
{
	int drb_remain_cnt, i;
	unsigned long flags;
	int drb_cnt = 0;
	int ret = 0;

	spin_lock_irqsave(&txq->tx_lock, flags);
	drb_remain_cnt = ring_buf_read_write_count(txq->drb_size_cnt, txq->drb_release_rd_idx,
						   txq->drb_wr_idx, false);
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	/* write DRB descriptor information */
	for (i = 0; i < DPMAIF_SKB_TX_BURST_CNT; i++) {
		struct dpmaif_tx_event *event;

		spin_lock_irqsave(&txq->tx_event_lock, flags);
		if (list_empty(&txq->tx_event_queue)) {
			spin_unlock_irqrestore(&txq->tx_event_lock, flags);
			break;
		}
		event = list_first_entry(&txq->tx_event_queue, struct dpmaif_tx_event, entry);
		spin_unlock_irqrestore(&txq->tx_event_lock, flags);

		if (drb_remain_cnt < event->drb_cnt) {
			spin_lock_irqsave(&txq->tx_lock, flags);
			drb_remain_cnt = ring_buf_read_write_count(txq->drb_size_cnt,
								   txq->drb_release_rd_idx,
								   txq->drb_wr_idx,
								   false);
			spin_unlock_irqrestore(&txq->tx_lock, flags);
			continue;
		}

		drb_remain_cnt -= event->drb_cnt;

		ret = dpmaif_tx_send_skb_on_tx_thread(txq->dpmaif_ctrl, event);

		if (ret < 0) {
			dev_crit(txq->dpmaif_ctrl->dev,
				 "txq%d send skb fail, ret=%d\n", event->qno, ret);
			break;
		}

		drb_cnt += event->drb_cnt;
		spin_lock_irqsave(&txq->tx_event_lock, flags);
		dpmaif_finish_event(event);
		txq->tx_submit_skb_cnt--;
		spin_unlock_irqrestore(&txq->tx_event_lock, flags);
	}

	if (drb_cnt > 0) {
		txq->drb_lack = false;
		ret = drb_cnt;
	} else if (ret == -ENOMEM) {
		txq->drb_lack = true;
	}

	return ret;
}

static bool check_all_txq_drb_lack(const struct dpmaif_ctrl *dpmaif_ctrl)
{
	unsigned char i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++)
		if (!list_empty(&dpmaif_ctrl->txq[i].tx_event_queue) &&
		    !dpmaif_ctrl->txq[i].drb_lack)
			return false;

	return true;
}

static void do_tx_hw_push(struct dpmaif_ctrl *dpmaif_ctrl)
{
	bool first_time = true;

	dpmaif_ctrl->txq_select_times = 0;
	do {
		int txq_id;

		txq_id = select_tx_queue(dpmaif_ctrl);
		if (txq_id >= 0) {
			struct dpmaif_tx_queue *txq;
			int ret;

			txq = &dpmaif_ctrl->txq[txq_id];
			ret = txq_burst_send_skb(txq);
			if (ret > 0) {
				int drb_send_cnt = ret;

				/* wait for the PCIe resource locked done */
				if (first_time &&
				    !mtk_pci_sleep_disable_complete(dpmaif_ctrl->mtk_dev))
					return;

				/* notify the dpmaif HW */
				ret = dpmaif_ul_add_wcnt(dpmaif_ctrl, (unsigned char)txq_id,
							 drb_send_cnt * DPMAIF_UL_DRB_ENTRY_WORD);
				if (ret < 0)
					dev_err(dpmaif_ctrl->dev,
						"txq%d: dpmaif_ul_add_wcnt fail.\n", txq_id);
			} else {
				/* all txq the lack of DRB count */
				if (check_all_txq_drb_lack(dpmaif_ctrl))
					usleep_range(10, 20);
			}
		}

		first_time = false;
		cond_resched();

	} while (!tx_lists_are_all_empty(dpmaif_ctrl) && !kthread_should_stop() &&
		 (dpmaif_ctrl->state == DPMAIF_STATE_PWRON));
}

static int dpmaif_tx_hw_push_thread(void *arg)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	int ret;

	dpmaif_ctrl = arg;
	while (!kthread_should_stop()) {
		if (tx_lists_are_all_empty(dpmaif_ctrl) ||
		    dpmaif_ctrl->state != DPMAIF_STATE_PWRON) {
			if (wait_event_interruptible(dpmaif_ctrl->tx_wq,
						     (!tx_lists_are_all_empty(dpmaif_ctrl) &&
						     dpmaif_ctrl->state == DPMAIF_STATE_PWRON) ||
						     kthread_should_stop()))
				continue;
		}

		if (kthread_should_stop())
			break;

		ret = pm_runtime_resume_and_get(dpmaif_ctrl->dev);
		if (ret < 0 && ret != -EACCES)
			return ret;

		mtk_pci_disable_sleep(dpmaif_ctrl->mtk_dev);
		do_tx_hw_push(dpmaif_ctrl);
		mtk_pci_enable_sleep(dpmaif_ctrl->mtk_dev);
		pm_runtime_mark_last_busy(dpmaif_ctrl->dev);
		pm_runtime_put_autosuspend(dpmaif_ctrl->dev);
	}

	return 0;
}

int dpmaif_tx_thread_init(struct dpmaif_ctrl *dpmaif_ctrl)
{
	init_waitqueue_head(&dpmaif_ctrl->tx_wq);
	dpmaif_ctrl->tx_thread = kthread_run(dpmaif_tx_hw_push_thread,
					     dpmaif_ctrl, "dpmaif_tx_hw_push");
	if (IS_ERR(dpmaif_ctrl->tx_thread)) {
		dev_err(dpmaif_ctrl->dev, "failed to start tx thread\n");
		return PTR_ERR(dpmaif_ctrl->tx_thread);
	}

	return 0;
}

void dpmaif_tx_thread_release(struct dpmaif_ctrl *dpmaif_ctrl)
{
	if (dpmaif_ctrl->tx_thread)
		kthread_stop(dpmaif_ctrl->tx_thread);
}

static inline unsigned char get_drb_cnt_per_skb(struct sk_buff *skb)
{
	/* normal DRB (frags data + skb linear data) + msg DRB */
	return (skb_shinfo(skb)->nr_frags + 1 + 1);
}

static bool check_tx_queue_drb_available(struct dpmaif_tx_queue *txq, unsigned int send_drb_cnt)
{
	unsigned int drb_remain_cnt;
	unsigned long flags;

	spin_lock_irqsave(&txq->tx_lock, flags);
	drb_remain_cnt = ring_buf_read_write_count(txq->drb_size_cnt, txq->drb_release_rd_idx,
						   txq->drb_wr_idx, false);
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	return drb_remain_cnt >= send_drb_cnt;
}

/**
 * dpmaif_tx_send_skb() - Add SKB to the xmit queue
 * @dpmaif_ctrl: Pointer to struct dpmaif_ctrl
 * @txqt: Queue type to xmit on (normal or fast)
 * @skb: Pointer to the SKB to xmit
 *
 * Add the SKB to the queue of the SKBs to be xmit.
 * Wake up the thread that push the SKBs from the queue to the HW.
 *
 * Return: Zero on success and negative errno on failure
 */
int dpmaif_tx_send_skb(struct dpmaif_ctrl *dpmaif_ctrl, enum txq_type txqt, struct sk_buff *skb)
{
	bool tx_drb_available = true;
	struct dpmaif_tx_queue *txq;
	struct dpmaif_callbacks *cb;
	unsigned int send_drb_cnt;

	send_drb_cnt = get_drb_cnt_per_skb(skb);
	txq = &dpmaif_ctrl->txq[txqt];

	/* check tx queue DRB full */
	if (!(txq->tx_skb_stat++ % DPMAIF_SKB_TX_BURST_CNT))
		tx_drb_available = check_tx_queue_drb_available(txq, send_drb_cnt);

	if (txq->tx_submit_skb_cnt < txq->tx_list_max_len && tx_drb_available) {
		struct dpmaif_tx_event *event;
		unsigned long flags;

		event = kmalloc(sizeof(*event), GFP_ATOMIC);
		if (!event)
			return -ENOMEM;

		INIT_LIST_HEAD(&event->entry);
		event->qno = txqt;
		event->skb = skb;
		event->drb_cnt = send_drb_cnt;

		spin_lock_irqsave(&txq->tx_event_lock, flags);
		list_add_tail(&event->entry, &txq->tx_event_queue);
		txq->tx_submit_skb_cnt++;
		spin_unlock_irqrestore(&txq->tx_event_lock, flags);
		wake_up(&dpmaif_ctrl->tx_wq);

		return 0;
	}

	cb = dpmaif_ctrl->callbacks;
	cb->state_notify(dpmaif_ctrl->mtk_dev, DMPAIF_TXQ_STATE_FULL, txqt);

	return -EBUSY;
}

void dpmaif_irq_tx_done(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int que_mask)
{
	int i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		if (que_mask & BIT(i))
			queue_work(dpmaif_ctrl->txq[i].worker, &dpmaif_ctrl->txq[i].dpmaif_tx_work);
	}
}

static int dpmaif_tx_buf_init(struct dpmaif_tx_queue *txq)
{
	size_t brb_skb_size;
	size_t brb_pd_size;

	brb_pd_size = DPMAIF_DRB_ENTRY_SIZE * sizeof(struct dpmaif_drb_pd);
	brb_skb_size = DPMAIF_DRB_ENTRY_SIZE * sizeof(struct dpmaif_drb_skb);

	txq->drb_size_cnt = DPMAIF_DRB_ENTRY_SIZE;

	/* alloc buffer for HW && AP SW */
	txq->drb_base = dma_alloc_coherent(txq->dpmaif_ctrl->dev, brb_pd_size,
					   &txq->drb_bus_addr, GFP_KERNEL | __GFP_ZERO);
	if (!txq->drb_base)
		return -ENOMEM;

	/* alloc buffer for AP SW to record the skb information */
	txq->drb_skb_base = devm_kzalloc(txq->dpmaif_ctrl->dev, brb_skb_size, GFP_KERNEL);
	if (!txq->drb_skb_base) {
		dma_free_coherent(txq->dpmaif_ctrl->dev, brb_pd_size,
				  txq->drb_base, txq->drb_bus_addr);
		return -ENOMEM;
	}

	return 0;
}

static void dpmaif_tx_buf_rel(struct dpmaif_tx_queue *txq)
{
	if (txq->drb_base)
		dma_free_coherent(txq->dpmaif_ctrl->dev,
				  txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd),
				  txq->drb_base, txq->drb_bus_addr);

	if (txq->drb_skb_base) {
		struct dpmaif_drb_skb *drb_skb, *drb_skb_base = txq->drb_skb_base;
		unsigned int i;

		for (i = 0; i < txq->drb_size_cnt; i++) {
			drb_skb = drb_skb_base + i;
			if (drb_skb->skb) {
				if (!(FIELD_GET(DRB_SKB_IS_MSG, drb_skb->config))) {
					dma_unmap_single(txq->dpmaif_ctrl->dev, drb_skb->bus_addr,
							drb_skb->data_len, DMA_TO_DEVICE);
				}
				if (FIELD_GET(DRB_SKB_IS_LAST, drb_skb->config)) {
					kfree_skb(drb_skb->skb);
					drb_skb->skb = NULL;
				}
			}
		}
	}
}

/**
 * dpmaif_txq_init() - Initialize TX queue
 * @txq: Pointer to struct dpmaif_tx_queue
 *
 * Initialize the TX queue data structure and allocate memory for it to use.
 *
 * Return: Zero on success and negative errno on failure
 */
int dpmaif_txq_init(struct dpmaif_tx_queue *txq)
{
	int ret;

	spin_lock_init(&txq->tx_event_lock);
	INIT_LIST_HEAD(&txq->tx_event_queue);
	txq->tx_submit_skb_cnt = 0;
	txq->tx_skb_stat = 0;
	txq->tx_list_max_len = DPMAIF_DRB_ENTRY_SIZE / 2;
	txq->drb_lack = false;

	init_waitqueue_head(&txq->req_wq);
	atomic_set(&txq->tx_budget, DPMAIF_DRB_ENTRY_SIZE);

	/* init the DRB DMA buffer and tx skb record info buffer */
	ret = dpmaif_tx_buf_init(txq);
	if (ret) {
		dev_err(txq->dpmaif_ctrl->dev, "tx buffer init fail %d\n", ret);
		return ret;
	}

	txq->worker = alloc_workqueue("md_dpmaif_tx%d_worker", WQ_UNBOUND | WQ_MEM_RECLAIM |
				      (txq->index ? 0 : WQ_HIGHPRI), 1, txq->index);
	if (!txq->worker)
		return -ENOMEM;

	INIT_WORK(&txq->dpmaif_tx_work, dpmaif_tx_done);
	spin_lock_init(&txq->tx_lock);
	return 0;
}

void dpmaif_txq_free(struct dpmaif_tx_queue *txq)
{
	struct dpmaif_tx_event *event, *event_next;
	unsigned long flags;

	if (txq->worker)
		destroy_workqueue(txq->worker);

	spin_lock_irqsave(&txq->tx_event_lock, flags);
	list_for_each_entry_safe(event, event_next, &txq->tx_event_queue, entry) {
		if (event->skb)
			dev_kfree_skb_any(event->skb);

		dpmaif_finish_event(event);
	}

	spin_unlock_irqrestore(&txq->tx_event_lock, flags);
	dpmaif_tx_buf_rel(txq);
}

void dpmaif_suspend_tx_sw_stop(struct dpmaif_ctrl *dpmaif_ctrl)
{
	int i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		struct dpmaif_tx_queue *txq;
		int count;

		txq = &dpmaif_ctrl->txq[i];

		txq->que_started = false;
		/* Ensure tx_processing is changed to 1 before actually begin TX flow */
		smp_mb();

		/* Confirm that SW will not transmit */
		count = 0;
		do {
			if (++count >= DPMAIF_MAX_CHECK_COUNT) {
				dev_err(dpmaif_ctrl->dev, "tx queue stop failed\n");
				break;
			}
		} while (atomic_read(&txq->tx_processing));
	}
}

static void dpmaif_stop_txq(struct dpmaif_tx_queue *txq)
{
	txq->que_started = false;

	cancel_work_sync(&txq->dpmaif_tx_work);
	flush_work(&txq->dpmaif_tx_work);

	if (txq->drb_skb_base) {
		struct dpmaif_drb_skb *drb_skb, *drb_skb_base = txq->drb_skb_base;
		unsigned int i;

		for (i = 0; i < txq->drb_size_cnt; i++) {
			drb_skb = drb_skb_base + i;
			if (drb_skb->skb) {
				if (!(FIELD_GET(DRB_SKB_IS_MSG, drb_skb->config))) {
					dma_unmap_single(txq->dpmaif_ctrl->dev, drb_skb->bus_addr,
							 drb_skb->data_len, DMA_TO_DEVICE);
				}
				if (FIELD_GET(DRB_SKB_IS_LAST, drb_skb->config)) {
					kfree_skb(drb_skb->skb);
					drb_skb->skb = NULL;
				}
			}
		}
	}

	txq->drb_rd_idx = 0;
	txq->drb_wr_idx = 0;
	txq->drb_release_rd_idx = 0;
}

void dpmaif_stop_tx_sw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	int i;

	/* flush and release UL descriptor */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++)
		dpmaif_stop_txq(&dpmaif_ctrl->txq[i]);
}
