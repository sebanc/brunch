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

#include "t7xx_hif_dpmaif.h"
#include "t7xx_hif_dpmaif_rx.h"
#include "t7xx_hif_dpmaif_tx.h"
#include "t7xx_dpmaif.h"
#include "t7xx_netdev.h"

unsigned int ring_buf_get_next_wrdx(unsigned int buf_len, unsigned int buf_idx)
{
	buf_idx++;

	return buf_idx < buf_len ? buf_idx : 0;
}

unsigned int ring_buf_writeable(unsigned int total_cnt,
				unsigned int rel_idx,
				unsigned int wrt_idx)
{
	unsigned int pkt_cnt;

	if (wrt_idx < rel_idx)
		pkt_cnt = rel_idx - wrt_idx - 1;
	else
		pkt_cnt = total_cnt + rel_idx - wrt_idx - 1;

	return pkt_cnt;
}

static void dpmaif_enable_irq(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_isr_para *isr_para;
	int i;

	if (atomic_cmpxchg(&dpmaif_ctrl->dpmaif_irq_enabled, 0, 1) == 0) {
		for (i = 0; i < ARRAY_SIZE(dpmaif_ctrl->isr_para); i++) {
			isr_para = &dpmaif_ctrl->isr_para[i];
			mtk_clear_set_int_type(dpmaif_ctrl->mtk_dev,
					       isr_para->pcie_int, false);
		}
	}
}

static void dpmaif_disable_irq(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_isr_para *isr_para;
	int i;

	if (atomic_cmpxchg(&dpmaif_ctrl->dpmaif_irq_enabled, 1, 0) == 1) {
		for (i = 0; i < ARRAY_SIZE(dpmaif_ctrl->isr_para); i++) {
			isr_para = &dpmaif_ctrl->isr_para[i];
			mtk_clear_set_int_type(dpmaif_ctrl->mtk_dev,
					       isr_para->pcie_int, true);
		}
	}
}

static void dpmaif_irq_cb(struct dpmaif_isr_para *isr_para)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = isr_para->dpmaif_ctrl;
	struct dpmaif_hw_intr_st_para intr_status;
	int index;
	int hw_ret;

	memset(&intr_status, 0, sizeof(struct dpmaif_hw_intr_st_para));

	/* gets HW interrupt types */
	hw_ret = dpmaif_hw_get_interrupt_status(&dpmaif_ctrl->hif_hw_info,
						(void *)&intr_status,
						isr_para->dlq_id);
	if (hw_ret < 0) {
		dev_err(dpmaif_ctrl->dev, "Get HW interrupt status failed!\n");
		return;
	}

	/* Clear level 1 interrupt status */
	/* Clear level 2 DPMAIF interrupt status firstly,
	 * then clear level 1 PCIe interrupt status
	 * to avoid empty interrupt
	 */
	mtk_clear_int_status(dpmaif_ctrl->mtk_dev, isr_para->pcie_int);

	/* handles interrupts */
	for (index = 0; index < intr_status.intr_cnt; index++) {
		switch (intr_status.intr_types[index]) {
		case DPF_INTR_UL_DONE:
			dpmaif_irq_tx_done(dpmaif_ctrl, intr_status.intr_queues[index]);
			break;

		case DPF_INTR_UL_DRB_EMPTY:
		case DPF_INTR_UL_MD_NOTREADY:
		case DPF_INTR_UL_MD_PWR_NOTREADY:
			break;

		case DPF_INTR_UL_LEN_ERR:
			pr_err_ratelimited("UL interrupt: length error!\n");
			break;

		case DPF_INTR_DL_DONE:
			dpmaif_irq_rx_done(dpmaif_ctrl,
					   intr_status.intr_queues[index]);
			break;

		case DPF_INTR_DL_SKB_LEN_ERR:
			pr_err_ratelimited("DL interrupt: skb length error!\n");
			break;

		case DPF_INTR_DL_BATCNT_LEN_ERR:
			pr_err_ratelimited("DL interrupt: packet BAT count length error!\n");
			dpmaif_unmask_dl_batcnt_len_err_interrupt(&dpmaif_ctrl->hif_hw_info);
			break;

		case DPF_INTR_DL_PITCNT_LEN_ERR:
			pr_err_ratelimited("DL interrupt: PIT count length error!\n");
			dpmaif_unmask_dl_pitcnt_len_err_interrupt(&dpmaif_ctrl->hif_hw_info);
			break;

		case DPF_INTR_DL_PKT_EMPTY_SET:
			pr_err_ratelimited("DL interrupt: packet BAT buffer empty!\n");
			break;

		case DPF_INTR_DL_FRG_EMPTY_SET:
			pr_err_ratelimited("DL interrupt: fragment BAT buffer empty!\n");
			break;

		case DPF_INTR_DL_MTU_ERR:
			pr_err_ratelimited("DL interrupt: MTU size error!\n");
			break;

		case DPF_INTR_DL_FRGCNT_LEN_ERR:
			pr_err_ratelimited("DL interrupt: fragment BAT count length error!\n");
			break;

		case DPF_DL_INT_DLQ0_PITCNT_LEN_ERR:
			pr_err_ratelimited("DL interrupt: DLQ0 PIT count length error!\n");
			dpmaif_dlq_unmask_rx_pit_cnt_len_err_intr(&dpmaif_ctrl->hif_hw_info,
								  DPF_RX_QNO_DFT);
			break;

		case DPF_DL_INT_DLQ1_PITCNT_LEN_ERR:
			pr_err_ratelimited("DL interrupt: DLQ1 PIT count length error!\n");
			dpmaif_dlq_unmask_rx_pit_cnt_len_err_intr(&dpmaif_ctrl->hif_hw_info,
								  DPF_RX_QNO1);
			break;

		case DPF_DL_INT_HPC_ENT_TYPE_ERR:
			pr_err_ratelimited("DL interrupt: HPC ENT type error!\n");
			break;

		case DPF_INTR_DL_DLQ0_DONE:
			dpmaif_irq_rx_done(dpmaif_ctrl, intr_status.intr_queues[index]);
			break;

		case DPF_INTR_DL_DLQ1_DONE:
			dpmaif_irq_rx_done(dpmaif_ctrl,
					   intr_status.intr_queues[index]);
			break;

		default:
			dev_err(dpmaif_ctrl->dev, "DL interrupt: unknown interrupt!\n");
		}
	}
}

static int dpmaif_isr(void *data)
{
	struct dpmaif_isr_para *isr_para = (struct dpmaif_isr_para *)data;
	struct md_dpmaif_ctrl *dpmaif_ctrl = isr_para->dpmaif_ctrl;

	if (dpmaif_ctrl->dpmaif_state != HIFDPMAIF_STATE_PWRON) {
		dev_err(dpmaif_ctrl->dev, "the unexpected dpmaif isr!");
		goto exit;
	}

	mtk_clear_set_int_type(dpmaif_ctrl->mtk_dev, isr_para->pcie_int, true);
	dpmaif_irq_cb(isr_para);
	mtk_clear_set_int_type(dpmaif_ctrl->mtk_dev, isr_para->pcie_int, false);
exit:
	return IRQ_HANDLED;
}

static void dpmaif_isr_parameter_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
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

static int dpmaif_platform_irq_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct mtk_pci_dev *mtk_dev = dpmaif_ctrl->mtk_dev;
	int i;
	enum pcie_int ext_int;
	struct dpmaif_isr_para *isr_para;
	enum pcie_int int_type;

	/* pcie isr parameter init */
	dpmaif_isr_parameter_init(dpmaif_ctrl);

	/* register isr */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		isr_para = &dpmaif_ctrl->isr_para[i];
		int_type = isr_para->pcie_int;
		mtk_clear_set_int_type(mtk_dev, int_type, true);
		ext_int = isr_para->pcie_int;
		mtk_clear_set_int_type(dpmaif_ctrl->mtk_dev, ext_int, true);

		dpmaif_ctrl->mtk_dev->intr_callback[ext_int] = dpmaif_isr;
		dpmaif_ctrl->mtk_dev->callback_param[ext_int] = isr_para;

		mtk_clear_int_status(mtk_dev, int_type);
		mtk_clear_set_int_type(mtk_dev, int_type, false);
	}

	return 0;
}

static void dpmaif_skb_pool_rel(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_skb_pool *pool = &dpmaif_ctrl->skb_pool;
	unsigned int index;

	flush_work(&pool->reload_work);

	if (pool->pool_reload_work_queue) {
		flush_workqueue(pool->pool_reload_work_queue);
		destroy_workqueue(pool->pool_reload_work_queue);
		pool->pool_reload_work_queue = NULL;
	}

	for (index = 0; index < DPMA_SKB_QUEUE_CNT; index++)
		dpmaif_skb_queue_rel(dpmaif_ctrl, index);
}

/* we put initializations which takes too much time here: SW init only */
static int dpmaif_sw_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rx_q;
	struct dpmaif_tx_queue *tx_q;
	int ret, i;

	/* RX normal bat table init */
	ret = dpmaif_bat_init(dpmaif_ctrl, &dpmaif_ctrl->bat_req, NORMAL_BUF);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif normal bat table init fail, %d!\n", ret);
		return ret;
	}
	spin_lock_init(&dpmaif_ctrl->bat_release_lock);

	/* RX frag bat table init */
	ret = dpmaif_bat_init(dpmaif_ctrl, &dpmaif_ctrl->bat_frag, FRAG_BUF);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif frag bat table init fail, %d!\n", ret);
		return ret;
	}
	spin_lock_init(&dpmaif_ctrl->frag_bat_release_lock);

	/* dpmaif RXQ resource init */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rx_q = &dpmaif_ctrl->rxq[i];
		rx_q->index = i;
		rx_q->dpmaif_ctrl = dpmaif_ctrl;
		dpmaif_rxq_init(rx_q);
	}

	/* dpmaif TXQ resource init */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		tx_q = &dpmaif_ctrl->txq[i];
		tx_q->index = i;
		tx_q->dpmaif_ctrl = dpmaif_ctrl;
		dpmaif_txq_init(tx_q);
	}

	/* init TX thread : send skb data to dpmaif HW */
	ret = dpmaif_tx_thread_init(dpmaif_ctrl);
	if (ret)
		return ret;

	/* Init the RX skb pool */
	ret = dpmaif_skb_pool_init(dpmaif_ctrl);
	if (ret)
		return ret;

	/* Release the bat resource workqueue */
	ret = dpmaif_bat_rel_work_init(dpmaif_ctrl);

	return ret;
}

static void dpmaif_sw_rel(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rx_q;
	struct dpmaif_tx_queue *tx_q;
	int i;

	/* release the tx thread */
	dpmaif_tx_thread_uninit(dpmaif_ctrl);

	/* release the bat release workqueue */
	dpmaif_bat_rel_work_rel(dpmaif_ctrl);

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		tx_q = &dpmaif_ctrl->txq[i];
		dpmaif_txq_rel(tx_q);
	}

	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rx_q = &dpmaif_ctrl->rxq[i];
		dpmaif_rxq_rel(rx_q);
	}

	/* release the skb pool */
	dpmaif_skb_pool_rel(dpmaif_ctrl);
}

static int dpmaif_start(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_init_para hw_init_para;
	struct dpmaif_rx_queue *rxq;
	struct dpmaif_tx_queue *txq;
	unsigned int buf_cnt;
	int i, ret = 0;

	if (!dpmaif_ctrl->dpmaif_sw_init_done) {
		dev_err(dpmaif_ctrl->dev, "dpmaif sw init fail\n");
		return -EFAULT;
	}

	if (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWRON)
		return -EFAULT;

	memset(&hw_init_para, 0, sizeof(struct dpmaif_hw_init_para));

	/* rx */
	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		rxq = &dpmaif_ctrl->rxq[i];
		rxq->que_started = true;
		rxq->index = i;
		rxq->budget = rxq->bat_req->bat_size_cnt - 1;
		rxq->check_bid_fail_cnt = 0;

		/* DPMAIF HW RX queue init parameter */
		hw_init_para.pkt_bat_base_addr[i] = rxq->bat_req->bat_bus_addr;
		hw_init_para.pkt_bat_size_cnt[i] = rxq->bat_req->bat_size_cnt;
		hw_init_para.pit_base_addr[i] = rxq->pit_bus_addr;
		hw_init_para.pit_size_cnt[i] = rxq->pit_size_cnt;
		hw_init_para.frg_bat_base_addr[i] = rxq->bat_frag->bat_bus_addr;
		hw_init_para.frg_bat_size_cnt[i] = rxq->bat_frag->bat_size_cnt;
	}

	/* rx normal bat mask init */
	memset(dpmaif_ctrl->bat_req.bat_mask, 0,
	       dpmaif_ctrl->bat_req.bat_size_cnt * sizeof(unsigned char));
	/* normal bat - skb buffer and submit bat */
	buf_cnt = dpmaif_ctrl->bat_req.bat_size_cnt - 1;
	ret = dpmaif_alloc_rx_buf(dpmaif_ctrl,
				  &dpmaif_ctrl->bat_req, 0, buf_cnt, 1, true);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif_alloc_rx_buf fail, ret:%d\n", ret);
		return ret;
	}

	/* frag bat - page buffer init */
	buf_cnt = dpmaif_ctrl->bat_frag.bat_size_cnt - 1;
	ret = dpmaif_alloc_rx_frag(dpmaif_ctrl, &dpmaif_ctrl->bat_frag,
				   0, buf_cnt, 1, true);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "dpmaif_alloc_rx_frag fail, ret:%d\n", ret);
		return ret;
	}

	/* tx */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctrl->txq[i];
		txq->que_started = true;

		/* DPMAIF HW TX queue init parameter */
		hw_init_para.drb_base_addr[i] = txq->drb_bus_addr;
		hw_init_para.drb_size_cnt[i] = txq->drb_size_cnt;
	}

	dpmaif_hw_init(&dpmaif_ctrl->hif_hw_info, (void *)&hw_init_para);

	/* notifies DPMAIF HW for available BAT count */
	dpmaif_dl_add_bat_cnt(&dpmaif_ctrl->hif_hw_info, 0, rxq->bat_req->bat_size_cnt - 1);
	dpmaif_dl_add_frg_cnt(&dpmaif_ctrl->hif_hw_info, 0, rxq->bat_frag->bat_size_cnt - 1);

	dpmaif_clr_ul_all_interrupt(&dpmaif_ctrl->hif_hw_info);
	dpmaif_clr_dl_all_interrupt(&dpmaif_ctrl->hif_hw_info);

	dpmaif_ctrl->dpmaif_state = HIFDPMAIF_STATE_PWRON;
	dpmaif_enable_irq(dpmaif_ctrl);

	/* wake up the dpmaif tx thread */
	wake_up(&dpmaif_ctrl->tx_wq);

	return 0;
}

static void dpmaif_pos_stop_hw(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_suspend_tx_sw_stop(dpmaif_ctrl);
	dpmaif_suspend_rx_sw_stop(dpmaif_ctrl);
}

static void dpmaif_stop_hw(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_hw_stop_tx_queue(&dpmaif_ctrl->hif_hw_info);
	dpmaif_hw_stop_rx_queue(&dpmaif_ctrl->hif_hw_info);
}

static int dpmaif_stop(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	if (!dpmaif_ctrl->dpmaif_sw_init_done) {
		dev_err(dpmaif_ctrl->dev, "dpmaif sw init fail\n");
		return -EFAULT;
	}

	if (dpmaif_ctrl->dpmaif_state == HIFDPMAIF_STATE_PWROFF)
		return -EFAULT;

	dpmaif_disable_irq(dpmaif_ctrl);
	dpmaif_ctrl->dpmaif_state = HIFDPMAIF_STATE_PWROFF;
	dpmaif_pos_stop_hw(dpmaif_ctrl);
	flush_work(&dpmaif_ctrl->skb_pool.reload_work);
	dpmaif_stop_tx_sw(dpmaif_ctrl);
	dpmaif_stop_rx_sw(dpmaif_ctrl);

	return 0;
}

static int dpmaif_suspend(struct mtk_pci_dev *mtk_dev, void *param)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl = (struct md_dpmaif_ctrl *)param;

	if (!dpmaif_ctrl)
		return 0;

	dpmaif_suspend_tx_sw_stop(dpmaif_ctrl);
	dpmaif_hw_stop_tx_queue(&dpmaif_ctrl->hif_hw_info);

	dpmaif_hw_stop_rx_queue(&dpmaif_ctrl->hif_hw_info);

	/* mask PCIe DPMAIF interrupt */
	dpmaif_disable_irq(dpmaif_ctrl);

	dpmaif_suspend_rx_sw_stop(dpmaif_ctrl);

	return 0;
}

static void dpmaif_unmask_dlq_interrupt(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	int qno;

	if (dpmaif_ctrl)
		for (qno = 0; qno < DPMAIF_RXQ_NUM; qno++)
			dpmaif_hw_dl_dlq_unmask_rx_done_intr
			    (&dpmaif_ctrl->hif_hw_info, qno);
}

static void dpmaif_pre_start_hw(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_rx_queue *rxq;
	struct dpmaif_tx_queue *txq;
	unsigned int que_cnt;

	if (!dpmaif_ctrl) {
		pr_err("dpmaif_ctrl is a NULL pointer!\n");
		return;
	}

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
	struct md_dpmaif_ctrl *dpmaif_ctrl = (struct md_dpmaif_ctrl *)param;

	/* start dpmaif tx/rx queue SW */
	dpmaif_pre_start_hw(dpmaif_ctrl);

	/* unmask PCIe DPMAIF interrupt */
	dpmaif_enable_irq(dpmaif_ctrl);

	dpmaif_unmask_dlq_interrupt(dpmaif_ctrl);

	dpmaif_start_hw(&dpmaif_ctrl->hif_hw_info);

	wake_up(&dpmaif_ctrl->tx_wq);

	return 0;
}

static int dpmaif_pm_entity_init(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct md_pm_entity *dpmaif_pm_entity;
	int ret;

	dpmaif_pm_entity = &dpmaif_ctrl->dpmaif_pm_entity;
	INIT_LIST_HEAD(&dpmaif_pm_entity->entity);
	dpmaif_pm_entity->suspend = &dpmaif_suspend;
	dpmaif_pm_entity->suspend_late = NULL;
	dpmaif_pm_entity->resume_early = NULL;
	dpmaif_pm_entity->resume = &dpmaif_resume;
	dpmaif_pm_entity->key = PM_ENTITY_KEY_DATA;
	dpmaif_pm_entity->entity_param = (void *)dpmaif_ctrl;

	ret = mtk_pci_pm_entity_register(dpmaif_ctrl->mtk_dev,
					 dpmaif_pm_entity);
	if (ret < 0)
		dev_err(dpmaif_ctrl->dev, "dpmaif register pm_entity fail\n");

	return ret;
}

static int dpmaif_pm_entity_uninit(struct md_dpmaif_ctrl *dpmaif_ctrl)
{
	struct md_pm_entity *dpmaif_pm_entity;
	int ret;

	dpmaif_pm_entity = &dpmaif_ctrl->dpmaif_pm_entity;
	ret = mtk_pci_pm_entity_unregister(dpmaif_ctrl->mtk_dev,
					   dpmaif_pm_entity);
	if (ret < 0)
		dev_err(dpmaif_ctrl->dev, "dpmaif register pm_entity fail\n");

	return ret;
}

int dpmaif_md_state_callback(struct md_dpmaif_ctrl *dpmaif_ctrl,
			     unsigned char state)
{
	int ret = 0;

	if (!dpmaif_ctrl) {
		pr_err("dpmaif_ctrl is a NULL pointer!\n");
		return -EINVAL;
	}

	switch (state) {
	case BOOT_WAITING_FOR_HS1:
		ret = dpmaif_start(dpmaif_ctrl);
		break;

	case EXCEPTION:
		ret = dpmaif_stop(dpmaif_ctrl);
		break;
	case STOPPED:
		ret = dpmaif_stop(dpmaif_ctrl);
		break;

	case WAITING_TO_STOP:
		dpmaif_stop_hw(dpmaif_ctrl);
		break;

	default:
		break;
	}

	return ret;
}

/* State Module part End */
int dpmaif_hif_init(struct ccmni_ctl_block *ctlb)
{
	struct mtk_pci_dev *mtk_dev = ctlb->mtk_dev;
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	int ret;

	/* allocates memory for md_dpmaif_ctrl */
	dpmaif_ctrl = kzalloc(sizeof(*dpmaif_ctrl), GFP_KERNEL);
	if (!dpmaif_ctrl)
		return -ENOMEM;

	ctlb->hif_ctrl = dpmaif_ctrl;
	dpmaif_ctrl->ctlb = ctlb;
	dpmaif_ctrl->mtk_dev = mtk_dev;
	dpmaif_ctrl->dev = &mtk_dev->pdev->dev;
	dpmaif_ctrl->dpmaif_sw_init_done = false;
	dpmaif_ctrl->napi_enable = !!(ccmni_get_nic_cap(dpmaif_ctrl->ctlb) & NIC_CAP_NAPI);

	/* gets PCIe device base address offset */
	dpmaif_ctrl->hif_hw_info.pcie_base = mtk_dev->base_addr.pcie_ext_reg_base -
		mtk_dev->base_addr.pcie_dev_reg_trsl_addr;

	dpmaif_pm_entity_init(dpmaif_ctrl);

	/* registers dpmaif irq by PCIe driver API */
	ret = dpmaif_platform_irq_init(dpmaif_ctrl);
	if (ret < 0) {
		dev_err(&mtk_dev->pdev->dev,
			"Register DPMAIF callback isr with PCIe driver fail! %d!\n", ret);
		goto err;
	}

	atomic_set(&dpmaif_ctrl->dpmaif_irq_enabled, 1);
	dpmaif_disable_irq(dpmaif_ctrl);

	/* Alloc TX/RX resource */
	ret = dpmaif_sw_init(dpmaif_ctrl);
	if (ret < 0) {
		dev_err(&mtk_dev->pdev->dev, "init the dpmaif hif SW resource fail! %d!\n", ret);
		goto err;
	}

	dpmaif_ctrl->dpmaif_sw_init_done = true;

	return 0;
err:
	kfree(dpmaif_ctrl);
	ctlb->hif_ctrl = NULL;

	return ret;
}

void dpmaif_hif_exit(struct ccmni_ctl_block *ctlb)
{
	struct md_dpmaif_ctrl *dpmaif_ctrl;

	if (!ctlb->hif_ctrl) {
		pr_err("input para is null err\n");
		return;
	}

	dpmaif_ctrl = ctlb->hif_ctrl;

	if (dpmaif_ctrl->dpmaif_sw_init_done) {
		dpmaif_stop(dpmaif_ctrl);
		dpmaif_pm_entity_uninit(dpmaif_ctrl);
		dpmaif_sw_rel(dpmaif_ctrl);
		dpmaif_ctrl->dpmaif_sw_init_done = false;
	}

	kfree(dpmaif_ctrl);
	ctlb->hif_ctrl = NULL;
}
