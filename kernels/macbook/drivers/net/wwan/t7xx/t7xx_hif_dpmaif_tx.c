// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/pm_runtime.h>

#include "t7xx_hif_dpmaif.h"
#include "t7xx_hif_dpmaif_tx.h"
#include "t7xx_dpmaif.h"
#include "t7xx_netdev.h"

static unsigned int ring_buf_releasable(unsigned int total_cnt,
					unsigned int rel_idx,
					unsigned int rd_idx)
{
	unsigned int pkt_cnt;

	if (rel_idx <= rd_idx)
		pkt_cnt = rd_idx - rel_idx;
	else
		pkt_cnt = total_cnt + rd_idx - rel_idx;

	return pkt_cnt;
}

/* TX part start */
static unsigned int dpmaifq_poll_tx_drb(struct md_dpmaif_ctrl *dpmaif_ctrl,
					unsigned char q_num)
{
	struct dpmaif_tx_queue *txq;
	unsigned int drb_cnt = 0;
	unsigned short old_sw_rd_idx, new_hw_rd_idx;
	unsigned int hw_read_idx;
	unsigned long flags;

	txq = &dpmaif_ctrl->txq[q_num];
	if (!txq->que_started)
		return drb_cnt;

	old_sw_rd_idx = txq->drb_rd_idx;

	hw_read_idx = dpmaif_ul_get_ridx(&dpmaif_ctrl->hif_hw_info, q_num);

	new_hw_rd_idx = hw_read_idx / DPMAIF_UL_DRB_ENTRY_WORD;

	if (new_hw_rd_idx >= DPMAIF_UL_DRB_ENTRY_SIZE) {
		dev_err(dpmaif_ctrl->dev, "Out of range read index: %u\n", new_hw_rd_idx);
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

static unsigned short dpmaif_release_tx_buffer(struct md_dpmaif_ctrl *dpmaif_ctrl,
					       unsigned char q_num,
					       int release_cnt)
{
	unsigned int drb_entry_num, idx;
	unsigned short cur_idx;
	struct dpmaif_drb_pd *cur_drb, *drb_base =
		(struct dpmaif_drb_pd *)(dpmaif_ctrl->txq[q_num].drb_base);
	struct sk_buff *skb_free;
	struct dpmaif_tx_queue *txq = &dpmaif_ctrl->txq[q_num];
	struct dpmaif_drb_skb *cur_drb_skb;
	unsigned long flags;

	if (release_cnt <= 0)
		return 0;

	spin_lock_irqsave(&txq->tx_lock, flags);
	drb_entry_num = txq->drb_size_cnt;
	cur_idx = txq->drb_rel_rd_idx;
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	for (idx = 0; idx < release_cnt; idx++) {
		cur_drb = drb_base + cur_idx;
		if (cur_drb->dtyp == DES_DTYP_PD) {
			cur_drb_skb =
				((struct dpmaif_drb_skb *)txq->drb_skb_base +
				cur_idx);
			dma_unmap_single(dpmaif_ctrl->dev,
					 cur_drb_skb->bus_addr, cur_drb_skb->data_len,
					 DMA_TO_DEVICE);

			if (cur_drb->c_bit == 0)
				skb_free = cur_drb_skb->skb;
			if (cur_drb->c_bit == 0 && !skb_free) {
				dev_err(dpmaif_ctrl->dev,
					"txq%u: pkt(%u): drb check fail, drb-w/r/rel-%u,%u,%u)\n",
					q_num, cur_idx, txq->drb_wr_idx,
					txq->drb_rd_idx,
					txq->drb_rel_rd_idx);
				dev_err(dpmaif_ctrl->dev,
					"release_cnt = %u, this time index = %u\n",
					release_cnt, idx);
				return -EINVAL;
			}
			if (cur_drb->c_bit == 0)
				dev_kfree_skb_any(skb_free);

			cur_drb_skb->skb = NULL;
		} else {
			txq->last_ch_id =
				((struct dpmaif_drb_msg *)cur_drb)->channel_id;
		}

		spin_lock_irqsave(&txq->tx_lock, flags);
		cur_idx = ring_buf_get_next_wrdx(drb_entry_num, cur_idx);
		txq->drb_rel_rd_idx = cur_idx;
		atomic_inc(&txq->tx_budget);
		spin_unlock_irqrestore(&txq->tx_lock, flags);

		if ((ccmni_get_nic_cap(dpmaif_ctrl->ctlb) &
		     NIC_CAP_TXBUSY_STOP) && (atomic_read(&txq->tx_budget) >
					      txq->drb_size_cnt / 8))
			ccmni_queue_state_notify(dpmaif_ctrl->ctlb, TX_IRQ, txq->index);
	}
	if (cur_drb->c_bit)
		dev_err(dpmaif_ctrl->dev, "txq%d_done: last one: c_bit != 0 ???\n", q_num);

	return idx;
}

static int dpmaif_tx_release(struct md_dpmaif_ctrl *dpmaif_ctrl,
			     unsigned char q_num, unsigned short budget)
{
	unsigned int rel_cnt;
	int real_rel_cnt;
	struct dpmaif_tx_queue *txq = &dpmaif_ctrl->txq[q_num];

	/* update rd idx: from HW */
	dpmaifq_poll_tx_drb(dpmaif_ctrl, q_num);
	rel_cnt = ring_buf_releasable(txq->drb_size_cnt, txq->drb_rel_rd_idx,
				      txq->drb_rd_idx);

	if (budget && rel_cnt > budget)
		real_rel_cnt = budget;
	else
		real_rel_cnt = rel_cnt;

	/* release data buff */
	if (real_rel_cnt)
		real_rel_cnt = dpmaif_release_tx_buffer(dpmaif_ctrl, q_num,
							real_rel_cnt);

	return ((real_rel_cnt < rel_cnt) ? -EAGAIN : 0);
}

/* check if it is spurious TX done interrupt */
/* return false indicates there are remaining spurious interrupt */
static inline bool dpmaif_no_remain_spurious_tx_done_intr(struct dpmaif_tx_queue *txq)
{
	return (dpmaifq_poll_tx_drb(txq->dpmaif_ctrl, txq->index) > 0);
}

/* tx done */
static void dpmaif_tx_done(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct dpmaif_tx_queue *txq =
		container_of(dwork, struct dpmaif_tx_queue, dpmaif_tx_work);
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	int ret;

	dpmaif_ctrl = txq->dpmaif_ctrl;
	if (!dpmaif_ctrl) {
		pr_err("hif_ctrl is a NULL pointer!\n");
		goto exit;
	}

	pm_runtime_get(dpmaif_ctrl->dev);

	/* lock deep sleep as early as possible */
	mtk_pci_l_resource_lock(dpmaif_ctrl->mtk_dev);

	/* ensure that we are not in deep sleep */
	mtk_pci_l_resource_wait_complete(dpmaif_ctrl->mtk_dev);

	ret = dpmaif_tx_release(dpmaif_ctrl, txq->index, txq->drb_size_cnt);

	if (ret == -EAGAIN ||
	    (dpmaif_hw_check_clr_ul_done_status(&dpmaif_ctrl->hif_hw_info, txq->index) &&
		dpmaif_no_remain_spurious_tx_done_intr(txq))) {
		queue_delayed_work(dpmaif_ctrl->txq[txq->index].worker,
				   &dpmaif_ctrl->txq[txq->index].dpmaif_tx_work,
				   msecs_to_jiffies(DPMF_TX_REL_TIMEOUT));
		/* clear IP busy to give the device time to enter the low power state */
		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
	} else {
		dpmaif_clr_ip_busy_sts(&dpmaif_ctrl->hif_hw_info);
		dpmaif_unmask_ul_que_interrupt(&dpmaif_ctrl->hif_hw_info, txq->index);
	}

	mtk_pci_l_resource_unlock(dpmaif_ctrl->mtk_dev);

	pm_runtime_put(dpmaif_ctrl->dev);

exit:
	return;
}

static void set_drb_msg(struct md_dpmaif_ctrl *dpmaif_ctrl,
			unsigned char q_num, unsigned short cur_idx,
	unsigned int pkt_len, unsigned short count_l,
	unsigned char channel_id, __be16 network_type)
{
	struct dpmaif_drb_msg *drb =
		((struct dpmaif_drb_msg *)dpmaif_ctrl->txq[q_num].drb_base +
		cur_idx);

	drb->dtyp = DES_DTYP_MSG;
	drb->c_bit = 1;
	drb->packet_len = pkt_len;
	drb->count_l = count_l;
	drb->channel_id = channel_id;
	drb->l4_chk = 1;
	drb->network_type = 0;
}

static void set_drb_payload(struct md_dpmaif_ctrl *dpmaif_ctrl,
			    unsigned char q_num, unsigned short cur_idx,
			    unsigned long long data_addr,
			    unsigned int pkt_size, char last_one)
{
	struct dpmaif_drb_pd *drb =
		((struct dpmaif_drb_pd *)dpmaif_ctrl->txq[q_num].drb_base +
		cur_idx);

	drb->dtyp = DES_DTYP_PD;
	if (last_one)
		drb->c_bit = 0;
	else
		drb->c_bit = 1;
	drb->data_len = pkt_size;
	drb->p_data_addr = (u32)data_addr;
	drb->data_addr_ext = (u32)(data_addr >> 32);
}

static void record_drb_skb(struct md_dpmaif_ctrl *dpmaif_ctrl,
			   unsigned char q_num, unsigned short cur_idx,
			   struct sk_buff *skb, unsigned short is_msg,
			   unsigned short is_frag, unsigned short is_last_one,
			   dma_addr_t bus_addr, unsigned int data_len)
{
	struct dpmaif_drb_skb *drb_skb =
		((struct dpmaif_drb_skb *)dpmaif_ctrl->txq[q_num].drb_skb_base +
		cur_idx);

	drb_skb->skb = skb;
	drb_skb->bus_addr = bus_addr;
	drb_skb->data_len = data_len;
	drb_skb->drb_idx = cur_idx;
	drb_skb->is_msg = is_msg;
	drb_skb->is_frag = is_frag;
	drb_skb->is_last_one = is_last_one;
}

static int dpmaif_tx_send_skb_on_tx_thread(struct md_dpmaif_ctrl *dpmaif_ctrl,
					   struct dpmaif_tx_event *event)
{
	struct sk_buff *skb = event->skb;
	int qno = event->qno;
	struct dpmaif_tx_queue *txq;
	struct skb_shared_info *info;
	void *data_addr;
	unsigned int data_len;
	int ret = 0;
	unsigned short cur_idx, is_frag, is_last_one;
	struct ccci_header ccci_h;
	unsigned int wt_cnt, send_cnt, payload_cnt;
	dma_addr_t bus_addr;
	unsigned long flags;
	bool skb_pulled = false;
	int drb_wr_idx_backup = -1;

	if (!skb)
		return 0;

	txq = &dpmaif_ctrl->txq[qno];

	atomic_set(&txq->tx_processing, 1);

	 /* Ensure tx_processing is changed to 1 before actually begin TX flow */
	smp_mb();

	if (!txq->que_started || dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) {
		ret = -ENODEV;
		goto exit;
	}

	info = skb_shinfo(skb);

	if (info->frag_list)
		dev_err(dpmaif_ctrl->dev, "Attention:ulq%d skb frag_list not supported!\n", qno);

	payload_cnt = info->nr_frags + 1;
	/* nr_frags: frag cnt, 1: skb->data, 1: msg drb */
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
	/* 3.1 a msg drb first, then payload drb. */
	set_drb_msg(dpmaif_ctrl, txq->index, cur_idx, skb->len, 0,
		    ccci_h.data[0], skb->protocol);
	record_drb_skb(dpmaif_ctrl, txq->index, cur_idx, skb, 1, 0, 0, 0, 0);
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	cur_idx = ring_buf_get_next_wrdx(txq->drb_size_cnt, cur_idx);

	/* 3.2 payload drb: skb->data + frags[] */
	for (wt_cnt = 0; wt_cnt < payload_cnt; wt_cnt++) {
		/* get data_addr && data_len */
		if (wt_cnt == 0) {
			data_len = skb_headlen(skb);
			data_addr = skb->data;
			is_frag = 0;
		} else {
			skb_frag_t *frag = info->frags + wt_cnt - 1;

			data_len = skb_frag_size(frag);
			data_addr = skb_frag_address(frag);
			is_frag = 1;
		}
		if (wt_cnt == payload_cnt - 1)
			is_last_one = 1;
		else
			/* set 0~(n-1) drb, maybe none */
			is_last_one = 0;

		/* tx mapping */
		bus_addr = dma_map_single(dpmaif_ctrl->dev, data_addr, data_len, DMA_TO_DEVICE);
		if (dma_mapping_error(dpmaif_ctrl->dev, bus_addr)) {
			dev_err(dpmaif_ctrl->dev, "dma mapping fail\n");
			ret = -ENOMEM;
			goto exit;
		}

		spin_lock_irqsave(&txq->tx_lock, flags);
		set_drb_payload(dpmaif_ctrl, txq->index, cur_idx, bus_addr, data_len,
				is_last_one);
		record_drb_skb(dpmaif_ctrl, txq->index, cur_idx, skb, 0, is_frag,
			       is_last_one, bus_addr, data_len);
		spin_unlock_irqrestore(&txq->tx_lock, flags);

		cur_idx = ring_buf_get_next_wrdx(txq->drb_size_cnt, cur_idx);
	}

	atomic_sub(send_cnt, &txq->tx_budget);

exit:
	atomic_set(&txq->tx_processing, 0);

	if (ret < 0) {
		dev_err(dpmaif_ctrl->dev,
			"send fail: drb_wr_idx_backup:%d, ret:%d\n",
			drb_wr_idx_backup, ret);

		if (skb_pulled)
			skb_push(skb, sizeof(struct ccci_header));

		if (drb_wr_idx_backup >= 0) {
			spin_lock_irqsave(&txq->tx_lock, flags);
			txq->drb_wr_idx = drb_wr_idx_backup;
			spin_unlock_irqrestore(&txq->tx_lock, flags);
		}
	}

	return ret;
}

/* must be called within protection of event_lock */
static inline void dpmaif_finish_event(struct dpmaif_tx_event *event)
{
	list_del(&event->entry);
	kfree(event);
}

static bool all_tx_list_is_empty(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	int i;
	bool is_empty = true;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		if (!list_empty(&dpmaif_ctrl->txq[i].tx_event_queue)) {
			is_empty = false;
			break;
		}
	}

	return is_empty;
}

static int select_tx_queue(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	unsigned char i;
	unsigned char txq_id;
	int ret = -1;
	unsigned char txq_prio[HIF_TXQ_IN_USE] = {HIF_ACK_QUEUE, HIF_NORMAL_QUEUE};

	for (i = 0; i < HIF_TXQ_IN_USE; i++) {
		txq_id = txq_prio[dpmaif_ctrl->txq_select_times % HIF_TXQ_IN_USE];
		if (!dpmaif_ctrl->txq[txq_id].que_started)
			break;

		if (!list_empty(&dpmaif_ctrl->txq[txq_id].tx_event_queue)) {
			ret = txq_id;
			dpmaif_ctrl->txq_select_times++;
			break;
		}
		dpmaif_ctrl->txq_select_times++;
	}

	return ret;
}

static int txq_burst_send_skb(struct dpmaif_tx_queue *txq)
{
	int ret = -1;
	unsigned long flags;
	struct dpmaif_tx_event *event;
	int i;
	int drb_cnt = 0;
	int drb_remain_cnt;

	spin_lock_irqsave(&txq->tx_lock, flags);
	drb_remain_cnt = ring_buf_writeable(txq->drb_size_cnt,
					    txq->drb_rel_rd_idx, txq->drb_wr_idx);
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	/* write DRB descriptor information */
	for (i = 0; i < DPMF_SKB_TX_BURST_CNT; i++) {
		if (list_empty(&txq->tx_event_queue)) {
			ret = 0;
			break;
		}

		spin_lock_irqsave(&txq->tx_event_lock, flags);
		event = list_first_entry(&txq->tx_event_queue,
					 struct dpmaif_tx_event, entry);
		spin_unlock_irqrestore(&txq->tx_event_lock, flags);

		if (drb_remain_cnt < event->drb_cnt) {
			spin_lock_irqsave(&txq->tx_lock, flags);
			drb_remain_cnt = ring_buf_writeable(txq->drb_size_cnt,
							    txq->drb_rel_rd_idx,
							    txq->drb_wr_idx);
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

	/* check send result if success */
	if (drb_cnt > 0) {
		txq->drb_lack = false;
		ret = drb_cnt;
	} else {
		if (ret == -1) {
			txq->drb_lack = true;
			ret = -ENOMEM;
		}
	}

	return ret;
}

static bool check_all_txq_drb_lack(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	bool ret = true;
	unsigned char i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		if (!list_empty(&dpmaif_ctrl->txq[i].tx_event_queue) &&
		    !dpmaif_ctrl->txq[i].drb_lack) {
			ret = false;
			break;
		}
	}

	return ret;
}

static void do_tx_hw_push(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	bool first_time = true;
	unsigned char txq_id;
	struct dpmaif_tx_queue *txq;
	int drb_send_cnt;
	int ret;

	dpmaif_ctrl->txq_select_times = 0;
	do {
		ret = select_tx_queue(dpmaif_ctrl);
		if (ret >= 0) {
			txq_id = (unsigned char)ret;
			txq = &dpmaif_ctrl->txq[txq_id];
			ret = txq_burst_send_skb(txq);
			if (ret > 0) {
				/* wait for the pcie resource locked done */
				if (first_time)
					mtk_pci_l_resource_wait_complete(dpmaif_ctrl->mtk_dev);

				/* notify the dpmaif HW */
				drb_send_cnt = ret;
				ret = dpmaif_ul_add_wcnt(&dpmaif_ctrl->hif_hw_info,
							 txq_id,
							 drb_send_cnt * DPMAIF_UL_DRB_ENTRY_WORD);
				if (ret < 0)
					dev_err(dpmaif_ctrl->dev,
						"txq%d: dpmaif_ul_add_wcnt fail!\n", txq_id);
			} else {
				/* all txq the lack of DRB count */
				if (check_all_txq_drb_lack(dpmaif_ctrl))
					usleep_range(10, 20);
			}
		}
		first_time = false;

		/* avoid the soft lockup */
		if (need_resched())
			cond_resched();
	} while (!all_tx_list_is_empty(dpmaif_ctrl) &&
		 !kthread_should_stop() &&
		 (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWRON));
}

static int dpmaif_tx_hw_push_thread(void *arg)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = arg;
	int ret;

	while (1) {
		if (all_tx_list_is_empty(dpmaif_ctrl) ||
		    dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) {
			ret = wait_event_interruptible(dpmaif_ctrl->tx_wq,
						       !all_tx_list_is_empty(dpmaif_ctrl) &&
						       dpmaif_ctrl->dpmaif_state ==
						       HIFDPMAIF_STATE_PWRON);

			if (ret == -ERESTARTSYS)
				continue;
		}
		if (kthread_should_stop())
			break;

		ret = pm_runtime_get_sync(dpmaif_ctrl->dev);
		if (ret < 0) {
			dev_err(dpmaif_ctrl->dev, "rpm lock fail, ret:%d\n", ret);
			continue;
		}

		mtk_pci_l_resource_lock(dpmaif_ctrl->mtk_dev);
		do_tx_hw_push(dpmaif_ctrl);
		mtk_pci_l_resource_unlock(dpmaif_ctrl->mtk_dev);
		pm_runtime_put_sync(dpmaif_ctrl->dev);
	}

	return 0;
}

int dpmaif_tx_thread_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	init_waitqueue_head(&dpmaif_ctrl->tx_wq);
	dpmaif_ctrl->tx_thread = kthread_run(dpmaif_tx_hw_push_thread,
					     dpmaif_ctrl, "dpmaif_tx_hw_push");

	return 0;
}

void dpmaif_tx_thread_uninit(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	if (dpmaif_ctrl->tx_thread)
		kthread_stop(dpmaif_ctrl->tx_thread);
}

static inline unsigned char get_drb_cnt_per_skb(struct sk_buff *skb)
{
	/* normal drb (frags data + skb linear data) + msg drb */
	return (skb_shinfo(skb)->nr_frags + 1 + 1);
}

static bool check_tx_queue_drb_available(struct dpmaif_tx_queue *txq,
					 int send_drb_cnt)
{
	bool ret = true;
	unsigned long flags;
	int drb_remain_cnt;

	spin_lock_irqsave(&txq->tx_lock, flags);
	drb_remain_cnt = ring_buf_writeable(txq->drb_size_cnt,
					    txq->drb_rel_rd_idx,
					    txq->drb_wr_idx);
	spin_unlock_irqrestore(&txq->tx_lock, flags);

	/* Check tx buffer full */
	if (drb_remain_cnt < send_drb_cnt)
		ret = false;

	return ret;
}

int dpmaif_tx_send_skb(struct md_dpmaif_ctrl *dpmaif_ctrl,
		       int qno, struct sk_buff *skb, int blocking)
{
	int ret = 0;
	struct dpmaif_tx_event *event;
	unsigned long flags;
	bool tx_drb_available = true;
	struct dpmaif_tx_queue *txq;
	int send_drb_cnt = get_drb_cnt_per_skb(skb);

	if (!dpmaif_ctrl) {
		pr_err("dpmaif_ctrl is a NULL pointer!\n");
		return 0;
	}

	if (qno >= DPMAIF_TXQ_NUM) {
		dev_err(dpmaif_ctrl->dev, "txq%d > %d\n", qno, DPMAIF_TXQ_NUM);
		ret = -EINVAL;
		goto fail;
	}

	txq = &dpmaif_ctrl->txq[qno];

	/* check tx queue drb full */
	if ((txq->tx_skb_stat++ % DPMF_SKB_TX_BURST_CNT) == 0)
		tx_drb_available = check_tx_queue_drb_available(txq, send_drb_cnt);

	if (txq->tx_submit_skb_cnt < txq->tx_list_max_len && tx_drb_available) {
		event = kmalloc(sizeof(*event),
				in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
		if (!event) {
			ret = -EBUSY;
			goto fail;
		}

		INIT_LIST_HEAD(&event->entry);
		event->qno = qno;
		event->skb = skb;
		event->blocking = blocking;
		event->drb_cnt = send_drb_cnt;

		spin_lock_irqsave(&txq->tx_event_lock, flags);
		list_add_tail(&event->entry, &txq->tx_event_queue);
		txq->tx_submit_skb_cnt++;
		spin_unlock_irqrestore(&txq->tx_event_lock, flags);
	} else {
		/* notify to ccmni layer, ccmni should carry off the net device tx queue. */
		if (ccmni_get_nic_cap(dpmaif_ctrl->ctlb) & NIC_CAP_TXBUSY_STOP) {
			ccmni_queue_state_notify(dpmaif_ctrl->ctlb, TX_FULL, qno);
			dev_crit(dpmaif_ctrl->dev, "TX FULL!\n");
		}
		ret = -EBUSY;
	}

	/* wake up the tx thread */
	wake_up(&dpmaif_ctrl->tx_wq);

fail:
	return ret;
}

void dpmaif_irq_tx_done(struct md_dpmaif_ctrl *dpmaif_ctrl,
			unsigned int que_mask)
{
	int i;
	unsigned int intr_ul_que_done;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		intr_ul_que_done = que_mask & BIT(i);
		if (intr_ul_que_done) {
			queue_delayed_work(dpmaif_ctrl->txq[i].worker,
					   &dpmaif_ctrl->txq[i].dpmaif_tx_work,
					   msecs_to_jiffies(DPMF_TX_REL_TIMEOUT));
		}
	}
}

static int dpmaif_tx_buf_init(struct dpmaif_tx_queue *txq)
{
	/* DRB buffer init */
	txq->drb_size_cnt = DPMAIF_UL_DRB_ENTRY_SIZE;

	/* alloc buffer for HW && AP SW */
	txq->drb_base = dma_alloc_coherent(txq->dpmaif_ctrl->dev,
					   (txq->drb_size_cnt *
					    sizeof(struct dpmaif_drb_pd)),
					   &txq->drb_bus_addr, GFP_KERNEL);

	if (!txq->drb_base) {
		dev_err(txq->dpmaif_ctrl->dev, "drb request fail\n");
		return -ENOMEM;
	}

	memset(txq->drb_base, 0, txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd));

	/* alloc buffer for AP SW to record the skb information */
	txq->drb_skb_base =
		kzalloc((txq->drb_size_cnt * sizeof(struct dpmaif_drb_skb)),
			GFP_KERNEL);
	if (!txq->drb_skb_base)
		return -ENOMEM;

	return 0;
}

static void dpmaif_tx_buf_rel(struct dpmaif_tx_queue *txq)
{
	unsigned int index;
	struct dpmaif_drb_skb *drb_skb;

	if (!txq || !txq->dpmaif_ctrl)
		return;

	if (txq->drb_base)
		dma_free_coherent(txq->dpmaif_ctrl->dev,
				  (txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd)),
				  txq->drb_base,
				  txq->drb_bus_addr);

	if (txq->drb_skb_base) {
		for (index = 0; index < txq->drb_size_cnt; index++) {
			drb_skb = ((struct dpmaif_drb_skb *)
				txq->drb_skb_base + index);
			if (drb_skb->skb) {
				dma_unmap_single(txq->dpmaif_ctrl->dev,
						 drb_skb->bus_addr, drb_skb->data_len,
						 DMA_TO_DEVICE);
				if (drb_skb->is_last_one) {
					kfree_skb(drb_skb->skb);
					drb_skb->skb = NULL;
				}
			}
		}

		kfree(txq->drb_skb_base);
		txq->drb_skb_base = NULL;
	}
}

int dpmaif_txq_init(struct dpmaif_tx_queue *txq)
{
	int ret;

	/* TXQ tx list size init */
	spin_lock_init(&txq->tx_event_lock);
	INIT_LIST_HEAD(&txq->tx_event_queue);
	txq->tx_submit_skb_cnt = 0;
	txq->tx_skb_stat = 0;
	txq->tx_list_max_len = DPMAIF_UL_DRB_ENTRY_SIZE / 2;
	txq->drb_lack = false;

	init_waitqueue_head(&txq->req_wq);
	atomic_set(&txq->tx_budget, DPMAIF_UL_DRB_ENTRY_SIZE);

	/* init the drb dma buffer and tx skb record info buffer */
	ret = dpmaif_tx_buf_init(txq);
	if (ret) {
		dev_err(txq->dpmaif_ctrl->dev, "tx buffer init fail %d\n", ret);
		return ret;
	}

	/* init the tx delay workqueue */
	txq->worker = alloc_workqueue("md_dpmaif_tx%d_worker",
				      WQ_UNBOUND | WQ_MEM_RECLAIM |
				      (txq->index == 0 ? WQ_HIGHPRI : 0),
				      1, txq->index);
	INIT_DELAYED_WORK(&txq->dpmaif_tx_work, dpmaif_tx_done);
	spin_lock_init(&txq->tx_lock);

	return 0;
}

void dpmaif_txq_rel(struct dpmaif_tx_queue *txq)
{
	unsigned long flags;
	struct dpmaif_tx_event *event, *event_next;

	if (!txq)
		return;

	if (txq->worker) {
		flush_workqueue(txq->worker);
		destroy_workqueue(txq->worker);
	}

	spin_lock_irqsave(&txq->tx_event_lock, flags);
	list_for_each_entry_safe(event, event_next,
				 &txq->tx_event_queue, entry) {
		if (event->skb)
			dev_kfree_skb_any(event->skb);
		dpmaif_finish_event(event);
	}
	spin_unlock_irqrestore(&txq->tx_event_lock, flags);

	dpmaif_tx_buf_rel(txq);
}

void dpmaif_suspend_tx_sw_stop(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_tx_queue *txq;
	unsigned int que_cnt;
	int count;

	/* Disable UL SW active */
	for (que_cnt = 0; que_cnt < DPMAIF_TXQ_NUM; que_cnt++) {
		txq = &dpmaif_ctrl->txq[que_cnt];

		txq->que_started = false;
		/* Ensure we have toggled txq->que_started to false before we check whether
		 * the txq is stopped during TX suspend flow
		 */
		smp_mb();

		/* just confirm sw will not update drb_wcnt reg. */
		count = 0;
		do {
			if (++count >= DPMA_CHECK_REG_TIMEOUT) {
				dev_err(dpmaif_ctrl->dev, "Pool stop Tx failed\n");
				break;
			}
		} while (atomic_read(&txq->tx_processing));
	}
}

static int dpmaif_stop_txq(struct dpmaif_tx_queue *txq)
{
	int index;
	struct dpmaif_drb_skb *drb_skb;

	if (!txq->drb_base)
		return -EFAULT;

	txq->que_started = false;

	/* flush work */
	cancel_delayed_work(&txq->dpmaif_tx_work);
	flush_delayed_work(&txq->dpmaif_tx_work);

	if (txq->drb_skb_base) {
		for (index = 0; index < txq->drb_size_cnt; index++) {
			drb_skb = ((struct dpmaif_drb_skb *)
				txq->drb_skb_base + index);
			if (drb_skb->skb) {
				dma_unmap_single(txq->dpmaif_ctrl->dev,
						 drb_skb->bus_addr, drb_skb->data_len,
						 DMA_TO_DEVICE);
				if (drb_skb->is_last_one) {
					kfree_skb(drb_skb->skb);
					drb_skb->skb = NULL;
				}
			}
		}
		memset(txq->drb_skb_base, 0,
		       (txq->drb_size_cnt * sizeof(struct dpmaif_drb_skb)));
	}

	memset(txq->drb_base, 0,
	       (txq->drb_size_cnt * sizeof(struct dpmaif_drb_pd)));

	txq->drb_rd_idx = 0;
	txq->drb_wr_idx = 0;
	txq->drb_rel_rd_idx = 0;

	return 0;
}

void dpmaif_stop_tx_sw(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	int i;

	/* flush and release UL descriptor */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++)
		dpmaif_stop_txq(&dpmaif_ctrl->txq[i]);
}

