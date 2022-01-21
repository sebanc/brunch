// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "t7xx_common.h"
#include "t7xx_dpmaif.h"
#include "t7xx_hif_dpmaif.h"
#include "t7xx_hif_dpmaif_rx.h"
#include "t7xx_hif_dpmaif_tx.h"
#include "t7xx_pcie_mac.h"

unsigned int ring_buf_get_next_wrdx(unsigned int buf_len, unsigned int buf_idx)
{
	buf_idx++;

	return buf_idx < buf_len ? buf_idx : 0;
}

unsigned int ring_buf_read_write_count(unsigned int total_cnt, unsigned int rd_idx,
				       unsigned int wrt_idx, bool rd_wrt)
{
	int pkt_cnt;

	if (rd_wrt)
		pkt_cnt = wrt_idx - rd_idx;
	else
		pkt_cnt = rd_idx - wrt_idx - 1;

	if (pkt_cnt < 0)
		pkt_cnt += total_cnt;

	return (unsigned int)pkt_cnt;
}

static void dpmaif_enable_irq(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_isr_para *isr_para;
	int i;

	for (i = 0; i < ARRAY_SIZE(dpmaif_ctrl->isr_para); i++) {
		isr_para = &dpmaif_ctrl->isr_para[i];
		mtk_pcie_mac_set_int(dpmaif_ctrl->mtk_dev, isr_para->pcie_int);
	}
}

static void dpmaif_disable_irq(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_isr_para *isr_para;
	int i;

	for (i = 0; i < ARRAY_SIZE(dpmaif_ctrl->isr_para); i++) {
		isr_para = &dpmaif_ctrl->isr_para[i];
		mtk_pcie_mac_clear_int(dpmaif_ctrl->mtk_dev, isr_para->pcie_int);
	}
}

static void dpmaif_irq_cb(struct dpmaif_isr_para *isr_para)
{
	struct dpmaif_hw_intr_st_para intr_status;
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct device *dev;
	int i;

	dpmaif_ctrl = isr_para->dpmaif_ctrl;
	dev = dpmaif_ctrl->dev;
	memset(&intr_status, 0, sizeof(intr_status));

	/* gets HW interrupt types */
	if (dpmaif_hw_get_interrupt_status(dpmaif_ctrl, &intr_status, isr_para->dlq_id) < 0) {
		dev_err(dev, "Get HW interrupt status failed!\n");
		return;
	}

	/* Clear level 1 interrupt status */
	/* Clear level 2 DPMAIF interrupt status first,
	 * then clear level 1 PCIe interrupt status
	 * to avoid an empty interrupt.
	 */
	mtk_pcie_mac_clear_int_status(dpmaif_ctrl->mtk_dev, isr_para->pcie_int);

	/* handles interrupts */
	for (i = 0; i < intr_status.intr_cnt; i++) {
		switch (intr_status.intr_types[i]) {
		case DPF_INTR_UL_DONE:
			dpmaif_irq_tx_done(dpmaif_ctrl, intr_status.intr_queues[i]);
			break;

		case DPF_INTR_UL_DRB_EMPTY:
		case DPF_INTR_UL_MD_NOTREADY:
		case DPF_INTR_UL_MD_PWR_NOTREADY:
			/* no need to log an error for these cases. */
			break;

		case DPF_INTR_DL_BATCNT_LEN_ERR:
			dev_err_ratelimited(dev, "DL interrupt: packet BAT count length error!\n");
			dpmaif_unmask_dl_batcnt_len_err_interrupt(&dpmaif_ctrl->hif_hw_info);
			break;

		case DPF_INTR_DL_PITCNT_LEN_ERR:
			dev_err_ratelimited(dev, "DL interrupt: PIT count length error!\n");
			dpmaif_unmask_dl_pitcnt_len_err_interrupt(&dpmaif_ctrl->hif_hw_info);
			break;

		case DPF_DL_INT_DLQ0_PITCNT_LEN_ERR:
			dev_err_ratelimited(dev, "DL interrupt: DLQ0 PIT count length error!\n");
			dpmaif_dlq_unmask_rx_pitcnt_len_err_intr(&dpmaif_ctrl->hif_hw_info,
								 DPF_RX_QNO_DFT);
			break;

		case DPF_DL_INT_DLQ1_PITCNT_LEN_ERR:
			dev_err_ratelimited(dev, "DL interrupt: DLQ1 PIT count length error!\n");
			dpmaif_dlq_unmask_rx_pitcnt_len_err_intr(&dpmaif_ctrl->hif_hw_info,
								 DPF_RX_QNO1);
			break;

		case DPF_INTR_DL_DONE:
		case DPF_INTR_DL_DLQ0_DONE:
		case DPF_INTR_DL_DLQ1_DONE:
			dpmaif_irq_rx_done(dpmaif_ctrl, intr_status.intr_queues[i]);
			break;

		default:
			dev_err_ratelimited(dev, "DL interrupt error: type : %d\n",
					    intr_status.intr_types[i]);
		}
	}
}

static irqreturn_t dpmaif_isr_handler(int irq, void *data)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	struct dpmaif_isr_para *isr_para;

	isr_para = data;
	dpmaif_ctrl = isr_para->dpmaif_ctrl;

	if (dpmaif_ctrl->state != DPMAIF_STATE_PWRON) {
		dev_err(dpmaif_ctrl->dev, "interrupt received before initializing DPMAIF\n");
		return IRQ_HANDLED;
	}

	mtk_pcie_mac_clear_int(dpmaif_ctrl->mtk_dev, isr_para->pcie_int);
	dpmaif_irq_cb(isr_para);
	mtk_pcie_mac_set_int(dpmaif_ctrl->mtk_dev, isr_para->pcie_int);
	return IRQ_HANDLED;
}

static void dpmaif_isr_parameter_init(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_isr_para *isr_para;
	unsigned char i;

	/* set up the RXQ and isr relation */
	dpmaif_ctrl->rxq_int_mapping[DPF_RX_QNO0] = DPMAIF_INT;
	dpmaif_ctrl->rxq_int_mapping[DPF_RX_QNO1] = DPMAIF2_INT;

	/* init the isr parameter */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		isr_para = &dpmaif_ctrl->isr_para[i];
		isr_para->dpmaif_ctrl = dpmaif_ctrl;
		isr_para->dlq_id = i;
		isr_para->pcie_int = dpmaif_ctrl->rxq_int_mapping[i];
	}
}

static void dpmaif_platform_irq_init(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_isr_para *isr_para;
	struct mtk_pci_dev *mtk_dev;
	enum pcie_int int_type;
	int i;

	mtk_dev = dpmaif_ctrl->mtk_dev;
	/* PCIe isr parameter init */
	dpmaif_isr_parameter_init(dpmaif_ctrl);

	/* register isr */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		isr_para = &dpmaif_ctrl->isr_para[i];
		int_type = isr_para->pcie_int;
		mtk_pcie_mac_clear_int(mtk_dev, int_type);

		mtk_dev->intr_handler[int_type] = dpmaif_isr_handler;
		mtk_dev->intr_thread[int_type] = NULL;
		mtk_dev->callback_param[int_type] = isr_para;

		mtk_pcie_mac_clear_int_status(mtk_dev, int_type);
		mtk_pcie_mac_set_int(mtk_dev, int_type);
	}
}

static void dpmaif_skb_pool_free(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_skb_pool *pool;
	unsigned int i;

	pool = &dpmaif_ctrl->skb_pool;
	flush_work(&pool->reload_work);

	if (pool->reload_work_queue) {
		destroy_workqueue(pool->reload_work_queue);
		pool->reload_work_queue = NULL;
	}

	for (i = 0; i < DPMA_SKB_QUEUE_CNT; i++)
		dpmaif_skb_queue_free(dpmaif_ctrl, i);
}

/* we put initializations which takes too much time here: SW init only */
static int dpmaif_sw_init(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rx_q;
	struct dpmaif_tx_queue *tx_q;
	int ret, i, j;

	/* RX normal BAT table init */
	ret = dpmaif_bat_alloc(dpmaif_ctrl, &dpmaif_ctrl->bat_req, BAT_TYPE_NORMAL);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "normal BAT table init fail, %d!\n", ret);
		return ret;
	}

	/* RX frag BAT table init */
	ret = dpmaif_bat_alloc(dpmaif_ctrl, &dpmaif_ctrl->bat_frag, BAT_TYPE_FRAG);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "frag BAT table init fail, %d!\n", ret);
		goto bat_frag_err;
	}

	/* dpmaif RXQ resource init */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rx_q = &dpmaif_ctrl->rxq[i];
		rx_q->index = i;
		rx_q->dpmaif_ctrl = dpmaif_ctrl;
		ret = dpmaif_rxq_alloc(rx_q);
		if (ret)
			goto rxq_init_err;
	}

	/* dpmaif TXQ resource init */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		tx_q = &dpmaif_ctrl->txq[i];
		tx_q->index = i;
		tx_q->dpmaif_ctrl = dpmaif_ctrl;
		ret = dpmaif_txq_init(tx_q);
		if (ret)
			goto txq_init_err;
	}

	/* Init TX thread: send skb data to dpmaif HW */
	ret = dpmaif_tx_thread_init(dpmaif_ctrl);
	if (ret)
		goto tx_thread_err;

	/* Init the RX skb pool */
	ret = dpmaif_skb_pool_init(dpmaif_ctrl);
	if (ret)
		goto pool_init_err;

	/* Init BAT rel workqueue */
	ret = dpmaif_bat_release_work_alloc(dpmaif_ctrl);
	if (ret)
		goto bat_work_init_err;

	return 0;

bat_work_init_err:
	dpmaif_skb_pool_free(dpmaif_ctrl);
pool_init_err:
	dpmaif_tx_thread_release(dpmaif_ctrl);
tx_thread_err:
	i = DPMAIF_TXQ_NUM;
txq_init_err:
	for (j = 0; j < i; j++) {
		tx_q = &dpmaif_ctrl->txq[j];
		dpmaif_txq_free(tx_q);
	}

	i = DPMAIF_RXQ_NUM;
rxq_init_err:
	for (j = 0; j < i; j++) {
		rx_q = &dpmaif_ctrl->rxq[j];
		dpmaif_rxq_free(rx_q);
	}

	dpmaif_bat_free(dpmaif_ctrl, &dpmaif_ctrl->bat_frag);
bat_frag_err:
	dpmaif_bat_free(dpmaif_ctrl, &dpmaif_ctrl->bat_req);

	return ret;
}

static void dpmaif_sw_release(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rx_q;
	struct dpmaif_tx_queue *tx_q;
	int i;

	/* release the tx thread */
	dpmaif_tx_thread_release(dpmaif_ctrl);

	/* release the BAT release workqueue */
	dpmaif_bat_release_work_free(dpmaif_ctrl);

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		tx_q = &dpmaif_ctrl->txq[i];
		dpmaif_txq_free(tx_q);
	}

	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rx_q = &dpmaif_ctrl->rxq[i];
		dpmaif_rxq_free(rx_q);
	}

	/* release the skb pool */
	dpmaif_skb_pool_free(dpmaif_ctrl);
}

static int dpmaif_start(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_params hw_init_para;
	struct dpmaif_rx_queue *rxq;
	struct dpmaif_tx_queue *txq;
	unsigned int buf_cnt;
	int i, ret = 0;

	if (dpmaif_ctrl->state == DPMAIF_STATE_PWRON)
		return -EFAULT;

	memset(&hw_init_para, 0, sizeof(hw_init_para));

	/* rx */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rxq = &dpmaif_ctrl->rxq[i];
		rxq->que_started = true;
		rxq->index = i;
		rxq->budget = rxq->bat_req->bat_size_cnt - 1;

		/* DPMAIF HW RX queue init parameter */
		hw_init_para.pkt_bat_base_addr[i] = rxq->bat_req->bat_bus_addr;
		hw_init_para.pkt_bat_size_cnt[i] = rxq->bat_req->bat_size_cnt;
		hw_init_para.pit_base_addr[i] = rxq->pit_bus_addr;
		hw_init_para.pit_size_cnt[i] = rxq->pit_size_cnt;
		hw_init_para.frg_bat_base_addr[i] = rxq->bat_frag->bat_bus_addr;
		hw_init_para.frg_bat_size_cnt[i] = rxq->bat_frag->bat_size_cnt;
	}

	/* rx normal BAT mask init */
	memset(dpmaif_ctrl->bat_req.bat_mask, 0,
	       dpmaif_ctrl->bat_req.bat_size_cnt * sizeof(unsigned char));
	/* normal BAT - skb buffer and submit BAT */
	buf_cnt = dpmaif_ctrl->bat_req.bat_size_cnt - 1;
	ret = dpmaif_rx_buf_alloc(dpmaif_ctrl, &dpmaif_ctrl->bat_req, 0, buf_cnt, true);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif_rx_buf_alloc fail, ret:%d\n", ret);
		return ret;
	}

	/* frag BAT - page buffer init */
	buf_cnt = dpmaif_ctrl->bat_frag.bat_size_cnt - 1;
	ret = dpmaif_rx_frag_alloc(dpmaif_ctrl, &dpmaif_ctrl->bat_frag, 0, buf_cnt, true);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif_rx_frag_alloc fail, ret:%d\n", ret);
		goto err_bat;
	}

	/* tx */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctrl->txq[i];
		txq->que_started = true;

		/* DPMAIF HW TX queue init parameter */
		hw_init_para.drb_base_addr[i] = txq->drb_bus_addr;
		hw_init_para.drb_size_cnt[i] = txq->drb_size_cnt;
	}

	ret = dpmaif_hw_init(dpmaif_ctrl, &hw_init_para);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif_hw_init fail, ret:%d\n", ret);
		goto err_frag;
	}

	/* notifies DPMAIF HW for available BAT count */
	ret = dpmaif_dl_add_bat_cnt(dpmaif_ctrl, 0, rxq->bat_req->bat_size_cnt - 1);
	if (ret)
		goto err_frag;

	ret = dpmaif_dl_add_frg_cnt(dpmaif_ctrl, 0, rxq->bat_frag->bat_size_cnt - 1);
	if (ret)
		goto err_frag;

	dpmaif_clr_ul_all_interrupt(&dpmaif_ctrl->hif_hw_info);
	dpmaif_clr_dl_all_interrupt(&dpmaif_ctrl->hif_hw_info);

	dpmaif_ctrl->state = DPMAIF_STATE_PWRON;
	dpmaif_enable_irq(dpmaif_ctrl);

	/* wake up the dpmaif tx thread */
	wake_up(&dpmaif_ctrl->tx_wq);
	return 0;
err_frag:
	dpmaif_bat_free(rxq->dpmaif_ctrl, rxq->bat_frag);
err_bat:
	dpmaif_bat_free(rxq->dpmaif_ctrl, rxq->bat_req);
	return ret;
}

static void dpmaif_pos_stop_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_suspend_tx_sw_stop(dpmaif_ctrl);
	dpmaif_suspend_rx_sw_stop(dpmaif_ctrl);
}

static void dpmaif_stop_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_hw_stop_tx_queue(dpmaif_ctrl);
	dpmaif_hw_stop_rx_queue(dpmaif_ctrl);
}

static int dpmaif_stop(struct dpmaif_ctrl *dpmaif_ctrl)
{
	if (!dpmaif_ctrl->dpmaif_sw_init_done) {
		dev_err(dpmaif_ctrl->dev, "dpmaif SW init fail\n");
		return -EFAULT;
	}

	if (dpmaif_ctrl->state == DPMAIF_STATE_PWROFF)
		return -EFAULT;

	dpmaif_disable_irq(dpmaif_ctrl);
	dpmaif_ctrl->state = DPMAIF_STATE_PWROFF;
	dpmaif_pos_stop_hw(dpmaif_ctrl);
	flush_work(&dpmaif_ctrl->skb_pool.reload_work);
	dpmaif_stop_tx_sw(dpmaif_ctrl);
	dpmaif_stop_rx_sw(dpmaif_ctrl);
	return 0;
}

static int dpmaif_suspend(struct mtk_pci_dev *mtk_dev, void *param)
{
	struct dpmaif_ctrl *dpmaif_ctrl;

	dpmaif_ctrl = param;
	dpmaif_suspend_tx_sw_stop(dpmaif_ctrl);
	dpmaif_hw_stop_tx_queue(dpmaif_ctrl);
	dpmaif_hw_stop_rx_queue(dpmaif_ctrl);
	dpmaif_disable_irq(dpmaif_ctrl);
	dpmaif_suspend_rx_sw_stop(dpmaif_ctrl);
	return 0;
}

static void dpmaif_unmask_dlq_interrupt(struct dpmaif_ctrl *dpmaif_ctrl)
{
	int qno;

	for (qno = 0; qno < DPMAIF_RXQ_NUM; qno++)
		dpmaif_hw_dlq_unmask_rx_done(&dpmaif_ctrl->hif_hw_info, qno);
}

static void dpmaif_pre_start_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rxq;
	struct dpmaif_tx_queue *txq;
	unsigned int que_cnt;

	/* Enable UL SW active */
	for (que_cnt = 0; que_cnt < DPMAIF_TXQ_NUM; que_cnt++) {
		txq = &dpmaif_ctrl->txq[que_cnt];
		txq->que_started = true;
	}

	/* Enable DL/RX SW active */
	for (que_cnt = 0; que_cnt < DPMAIF_RXQ_NUM; que_cnt++) {
		rxq = &dpmaif_ctrl->rxq[que_cnt];
		rxq->que_started = true;
	}
}

static int dpmaif_resume(struct mtk_pci_dev *mtk_dev, void *param)
{
	struct dpmaif_ctrl *dpmaif_ctrl;

	dpmaif_ctrl = param;
	if (!dpmaif_ctrl)
		return 0;

	/* start dpmaif tx/rx queue SW */
	dpmaif_pre_start_hw(dpmaif_ctrl);
	/* unmask PCIe DPMAIF interrupt */
	dpmaif_enable_irq(dpmaif_ctrl);
	dpmaif_unmask_dlq_interrupt(dpmaif_ctrl);
	dpmaif_start_hw(dpmaif_ctrl);
	wake_up(&dpmaif_ctrl->tx_wq);
	return 0;
}

static int dpmaif_pm_entity_init(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct md_pm_entity *dpmaif_pm_entity;
	int ret;

	dpmaif_pm_entity = &dpmaif_ctrl->dpmaif_pm_entity;
	INIT_LIST_HEAD(&dpmaif_pm_entity->entity);
	dpmaif_pm_entity->suspend = &dpmaif_suspend;
	dpmaif_pm_entity->suspend_late = NULL;
	dpmaif_pm_entity->resume_early = NULL;
	dpmaif_pm_entity->resume = &dpmaif_resume;
	dpmaif_pm_entity->id = PM_ENTITY_ID_DATA;
	dpmaif_pm_entity->entity_param = dpmaif_ctrl;

	ret = mtk_pci_pm_entity_register(dpmaif_ctrl->mtk_dev, dpmaif_pm_entity);
	if (ret)
		dev_err(dpmaif_ctrl->dev, "dpmaif register pm_entity fail\n");

	return ret;
}

static int dpmaif_pm_entity_release(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct md_pm_entity *dpmaif_pm_entity;
	int ret;

	dpmaif_pm_entity = &dpmaif_ctrl->dpmaif_pm_entity;
	ret = mtk_pci_pm_entity_unregister(dpmaif_ctrl->mtk_dev, dpmaif_pm_entity);
	if (ret < 0)
		dev_err(dpmaif_ctrl->dev, "dpmaif register pm_entity fail\n");

	return ret;
}

int dpmaif_md_state_callback(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char state)
{
	int ret = 0;

	switch (state) {
	case MD_STATE_WAITING_FOR_HS1:
		ret = dpmaif_start(dpmaif_ctrl);
		break;

	case MD_STATE_EXCEPTION:
		ret = dpmaif_stop(dpmaif_ctrl);
		break;

	case MD_STATE_STOPPED:
		ret = dpmaif_stop(dpmaif_ctrl);
		break;

	case MD_STATE_WAITING_TO_STOP:
		dpmaif_stop_hw(dpmaif_ctrl);
		break;

	default:
		break;
	}

	return ret;
}

/**
 * dpmaif_hif_init() - Initialize data path
 * @mtk_dev: MTK context structure
 * @callbacks: Callbacks implemented by the network layer to handle RX skb and
 *	       event notifications
 *
 * Allocate and initialize datapath control block
 * Register datapath ISR, TX and RX resources
 *
 * Return: pointer to DPMAIF context structure or NULL in case of error
 */
struct dpmaif_ctrl *dpmaif_hif_init(struct mtk_pci_dev *mtk_dev,
				    struct dpmaif_callbacks *callbacks)
{
	struct dpmaif_ctrl *dpmaif_ctrl;
	int ret;

	if (!callbacks)
		return NULL;

	dpmaif_ctrl = devm_kzalloc(&mtk_dev->pdev->dev, sizeof(*dpmaif_ctrl), GFP_KERNEL);
	if (!dpmaif_ctrl)
		return NULL;

	dpmaif_ctrl->mtk_dev = mtk_dev;
	dpmaif_ctrl->callbacks = callbacks;
	dpmaif_ctrl->dev = &mtk_dev->pdev->dev;
	dpmaif_ctrl->dpmaif_sw_init_done = false;
	dpmaif_ctrl->hif_hw_info.pcie_base = mtk_dev->base_addr.pcie_ext_reg_base -
					     mtk_dev->base_addr.pcie_dev_reg_trsl_addr;

	ret = dpmaif_pm_entity_init(dpmaif_ctrl);
	if (ret)
		return NULL;

	/* registers dpmaif irq by PCIe driver API */
	dpmaif_platform_irq_init(dpmaif_ctrl);
	dpmaif_disable_irq(dpmaif_ctrl);

	/* Alloc TX/RX resource */
	ret = dpmaif_sw_init(dpmaif_ctrl);
	if (ret) {
		dev_err(&mtk_dev->pdev->dev, "DPMAIF SW initialization fail! %d\n", ret);
		dpmaif_pm_entity_release(dpmaif_ctrl);
		return NULL;
	}

	dpmaif_ctrl->dpmaif_sw_init_done = true;
	return dpmaif_ctrl;
}

void dpmaif_hif_exit(struct dpmaif_ctrl *dpmaif_ctrl)
{
	if (dpmaif_ctrl->dpmaif_sw_init_done) {
		dpmaif_stop(dpmaif_ctrl);
		dpmaif_pm_entity_release(dpmaif_ctrl);
		dpmaif_sw_release(dpmaif_ctrl);
		dpmaif_ctrl->dpmaif_sw_init_done = false;
	}
}
