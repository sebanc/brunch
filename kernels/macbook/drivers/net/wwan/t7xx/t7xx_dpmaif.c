// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>

#include "t7xx_dpmaif.h"
#include "t7xx_reg.h"

static int dpmaif_init_intr(struct dpmaif_hw_info *hw_info)
{
	struct dpmaif_isr_en_mask *isr_en_msk;
	u32 value, l2intr_en, l2intr_err_en;
	int ret;

	isr_en_msk = &hw_info->isr_en_mask;

	/* set UL interrupt */
	l2intr_en = DPMAIF_UL_INT_ERR_MSK | DPMAIF_UL_INT_QDONE_MSK;
	isr_en_msk->ap_ul_l2intr_en_msk = l2intr_en;

	iowrite32(DPMAIF_AP_ALL_L2TISAR0_MASK, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);

	/* set interrupt enable mask */
	iowrite32(l2intr_en, hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMCR0);
	iowrite32(~l2intr_en, hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMSR0);

	/* check mask status */
	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0,
					value, (value & l2intr_en) != l2intr_en,
					0, DPMAIF_CHECK_INIT_TIMEOUT_US);
	if (ret)
		return ret;

	/* set DL interrupt */
	l2intr_err_en = DPMAIF_DL_INT_PITCNT_LEN_ERR | DPMAIF_DL_INT_BATCNT_LEN_ERR;
	isr_en_msk->ap_dl_l2intr_err_en_msk = l2intr_err_en;

	l2intr_en = DPMAIF_DL_INT_DLQ0_QDONE | DPMAIF_DL_INT_DLQ0_PITCNT_LEN |
		    DPMAIF_DL_INT_DLQ1_QDONE | DPMAIF_DL_INT_DLQ1_PITCNT_LEN;
	isr_en_msk->ap_ul_l2intr_en_msk = l2intr_en;

	iowrite32(DPMAIF_AP_APDL_ALL_L2TISAR0_MASK, hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);

	/* set DL ISR PD enable mask */
	iowrite32(~l2intr_en, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);

	/* check mask status */
	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMR0,
					value, (value & l2intr_en) != l2intr_en,
					0, DPMAIF_CHECK_INIT_TIMEOUT_US);
	if (ret)
		return ret;

	isr_en_msk->ap_udl_ip_busy_en_msk = DPMAIF_UDL_IP_BUSY;

	iowrite32(DPMAIF_AP_IP_BUSY_MASK, hw_info->pcie_base + DPMAIF_AP_IP_BUSY);

	iowrite32(isr_en_msk->ap_udl_ip_busy_en_msk,
		  hw_info->pcie_base + DPMAIF_AO_AP_DLUL_IP_BUSY_MASK);

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L1TIMR0);
	value |= DPMAIF_DL_INT_Q2APTOP | DPMAIF_DL_INT_Q2TOQ1;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_UL_AP_L1TIMR0);
	iowrite32(DPMA_HPC_ALL_INT_MASK, hw_info->pcie_base + DPMAIF_HPC_INTR_MASK);

	return 0;
}

static void dpmaif_mask_ulq_interrupt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int q_num)
{
	struct dpmaif_isr_en_mask *isr_en_msk;
	struct dpmaif_hw_info *hw_info;
	u32 value, ul_int_que_done;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	isr_en_msk = &hw_info->isr_en_mask;
	ul_int_que_done = BIT(q_num + _UL_INT_DONE_OFFSET) & DPMAIF_UL_INT_QDONE_MSK;
	isr_en_msk->ap_ul_l2intr_en_msk &= ~ul_int_que_done;
	iowrite32(ul_int_que_done, hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMSR0);

	/* check mask status */
	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0,
					value, (value & ul_int_que_done) == ul_int_que_done,
					0, DPMAIF_CHECK_TIMEOUT_US);
	if (ret)
		dev_err(dpmaif_ctrl->dev,
			"Could not mask the UL interrupt. DPMAIF_AO_UL_AP_L2TIMR0 is 0x%x\n",
			value);
}

void dpmaif_unmask_ulq_interrupt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int q_num)
{
	struct dpmaif_isr_en_mask *isr_en_msk;
	struct dpmaif_hw_info *hw_info;
	u32 value, ul_int_que_done;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	isr_en_msk = &hw_info->isr_en_mask;
	ul_int_que_done = BIT(q_num + _UL_INT_DONE_OFFSET) & DPMAIF_UL_INT_QDONE_MSK;
	isr_en_msk->ap_ul_l2intr_en_msk |= ul_int_que_done;
	iowrite32(ul_int_que_done, hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMCR0);

	/* check mask status */
	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0,
					value, (value & ul_int_que_done) != ul_int_que_done,
					0, DPMAIF_CHECK_TIMEOUT_US);
	if (ret)
		dev_err(dpmaif_ctrl->dev,
			"Could not unmask the UL interrupt. DPMAIF_AO_UL_AP_L2TIMR0 is 0x%x\n",
			value);
}

static void dpmaif_mask_dl_batcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info)
{
	hw_info->isr_en_mask.ap_dl_l2intr_en_msk &= ~DPMAIF_DL_INT_BATCNT_LEN_ERR;
	iowrite32(DPMAIF_DL_INT_BATCNT_LEN_ERR,
		  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
}

void dpmaif_unmask_dl_batcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info)
{
	hw_info->isr_en_mask.ap_dl_l2intr_en_msk |= DPMAIF_DL_INT_BATCNT_LEN_ERR;
	iowrite32(DPMAIF_DL_INT_BATCNT_LEN_ERR,
		  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMCR0);
}

static void dpmaif_mask_dl_pitcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info)
{
	hw_info->isr_en_mask.ap_dl_l2intr_en_msk &= ~DPMAIF_DL_INT_PITCNT_LEN_ERR;
	iowrite32(DPMAIF_DL_INT_PITCNT_LEN_ERR,
		  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
}

void dpmaif_unmask_dl_pitcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info)
{
	hw_info->isr_en_mask.ap_dl_l2intr_en_msk |= DPMAIF_DL_INT_PITCNT_LEN_ERR;
	iowrite32(DPMAIF_DL_INT_PITCNT_LEN_ERR,
		  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMCR0);
}

static u32 update_dlq_interrupt(struct dpmaif_hw_info *hw_info, u32 q_done)
{
	u32 value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0);
	iowrite32(q_done, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);

	return value;
}

static int mask_dlq_interrupt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char qno)
{
	struct dpmaif_hw_info *hw_info;
	u32 value, q_done;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	q_done = (qno == DPF_RX_QNO0) ? DPMAIF_DL_INT_DLQ0_QDONE : DPMAIF_DL_INT_DLQ1_QDONE;
	iowrite32(q_done, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);

	/* check mask status */
	ret = read_poll_timeout_atomic(update_dlq_interrupt, value, value & q_done,
				       0, DPMAIF_CHECK_TIMEOUT_US, false, hw_info, q_done);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "mask_dl_dlq0 check timeout\n");
		return -ETIMEDOUT;
	}

	hw_info->isr_en_mask.ap_dl_l2intr_en_msk &= ~q_done;

	return 0;
}

void dpmaif_hw_dlq_unmask_rx_done(struct dpmaif_hw_info *hw_info, unsigned char qno)
{
	u32 mask;

	mask = (qno == DPF_RX_QNO0) ? DPMAIF_DL_INT_DLQ0_QDONE : DPMAIF_DL_INT_DLQ1_QDONE;
	iowrite32(mask, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMCR0);
	hw_info->isr_en_mask.ap_dl_l2intr_en_msk |= mask;
}

void dpmaif_clr_ip_busy_sts(struct dpmaif_hw_info *hw_info)
{
	u32 ip_busy_sts = ioread32(hw_info->pcie_base + DPMAIF_AP_IP_BUSY);

	iowrite32(ip_busy_sts, hw_info->pcie_base + DPMAIF_AP_IP_BUSY);
}

static void dpmaif_dlq_mask_rx_pitcnt_len_err_intr(struct dpmaif_hw_info *hw_info,
						   unsigned char qno)
{
	if (qno == DPF_RX_QNO0)
		iowrite32(DPMAIF_DL_INT_DLQ0_PITCNT_LEN,
			  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
	else
		iowrite32(DPMAIF_DL_INT_DLQ1_PITCNT_LEN,
			  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
}

void dpmaif_dlq_unmask_rx_pitcnt_len_err_intr(struct dpmaif_hw_info *hw_info, unsigned char qno)
{
	if (qno == DPF_RX_QNO0)
		iowrite32(DPMAIF_DL_INT_DLQ0_PITCNT_LEN,
			  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMCR0);
	else
		iowrite32(DPMAIF_DL_INT_DLQ1_PITCNT_LEN,
			  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMCR0);
}

void dpmaif_clr_ul_all_interrupt(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_AP_ALL_L2TISAR0_MASK, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
}

void dpmaif_clr_dl_all_interrupt(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_AP_APDL_ALL_L2TISAR0_MASK, hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);
}

/* para->intr_cnt counter is set to zero before this function is called.
 * it is not check for overflow as there is no risk of overflowing intr_types or intr_queues.
 */
static void dpmaif_hw_check_tx_interrupt(struct dpmaif_ctrl *dpmaif_ctrl,
					 unsigned int l2_txisar0,
					 struct dpmaif_hw_intr_st_para *para)
{
	unsigned int index;
	unsigned int value;

	value = l2_txisar0 & DP_UL_INT_QDONE_MSK;
	if (value) {
		unsigned long pending = (value >> DP_UL_INT_DONE_OFFSET) & DP_UL_QNUM_MSK;

		para->intr_types[para->intr_cnt] = DPF_INTR_UL_DONE;
		para->intr_queues[para->intr_cnt] = pending;

		/* mask TX queue interrupt immediately after it occurs */
		for_each_set_bit(index, &pending, DPMAIF_TXQ_NUM)
			dpmaif_mask_ulq_interrupt(dpmaif_ctrl, index);

		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_EMPTY_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_DRB_EMPTY;
		para->intr_queues[para->intr_cnt] =
			(value >> DP_UL_INT_EMPTY_OFFSET) & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_MD_NOTREADY_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_MD_NOTREADY;
		para->intr_queues[para->intr_cnt] =
			(value >> DP_UL_INT_MD_NOTRDY_OFFSET) & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_MD_PWR_NOTREADY_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_MD_PWR_NOTREADY;
		para->intr_queues[para->intr_cnt] =
			(value >> DP_UL_INT_PWR_NOTRDY_OFFSET) & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_ERR_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_LEN_ERR;
		para->intr_queues[para->intr_cnt] =
			(value >> DP_UL_INT_LEN_ERR_OFFSET) & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}
}

/* para->intr_cnt counter is set to zero before this function is called.
 * it is not check for overflow as there is no risk of overflowing intr_types or intr_queues.
 */
static void dpmaif_hw_check_rx_interrupt(struct dpmaif_ctrl *dpmaif_ctrl,
					 unsigned int *pl2_rxisar0,
					 struct dpmaif_hw_intr_st_para *para,
					 int qno)
{
	unsigned int l2_rxisar0 = *pl2_rxisar0;
	struct dpmaif_hw_info *hw_info;
	unsigned int value;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	if (qno == DPF_RX_QNO_DFT) {
		value = l2_rxisar0 & DP_DL_INT_SKB_LEN_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_SKB_LEN_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;
		}

		value = l2_rxisar0 & DP_DL_INT_BATCNT_LEN_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_BATCNT_LEN_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;

			dpmaif_mask_dl_batcnt_len_err_interrupt(hw_info);
		}

		value = l2_rxisar0 & DP_DL_INT_PITCNT_LEN_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_PITCNT_LEN_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;

			dpmaif_mask_dl_pitcnt_len_err_interrupt(hw_info);
		}

		value = l2_rxisar0 & DP_DL_INT_PKT_EMPTY_MSK;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_PKT_EMPTY_SET;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;
		}

		value = l2_rxisar0 & DP_DL_INT_FRG_EMPTY_MSK;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_FRG_EMPTY_SET;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;
		}

		value = l2_rxisar0 & DP_DL_INT_MTU_ERR_MSK;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_MTU_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;
		}

		value = l2_rxisar0 & DP_DL_INT_FRG_LENERR_MSK;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_FRGCNT_LEN_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;
		}

		value = l2_rxisar0 & DP_DL_INT_DLQ0_PITCNT_LEN_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_DL_INT_DLQ0_PITCNT_LEN_ERR;
			para->intr_queues[para->intr_cnt] = BIT(qno);
			para->intr_cnt++;

			dpmaif_dlq_mask_rx_pitcnt_len_err_intr(hw_info, qno);
		}

		value = l2_rxisar0 & DP_DL_INT_HPC_ENT_TYPE_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_DL_INT_HPC_ENT_TYPE_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;
		}

		value = l2_rxisar0 & DP_DL_INT_DLQ0_QDONE_SET;
		if (value) {
			/* mask RX done interrupt immediately after it occurs */
			if (!mask_dlq_interrupt(dpmaif_ctrl, qno)) {
				para->intr_types[para->intr_cnt] = DPF_INTR_DL_DLQ0_DONE;
				para->intr_queues[para->intr_cnt] = BIT(qno);
				para->intr_cnt++;
			} else {
				/* Unable to clear the interrupt, try again on the next one
				 * device entered low power mode or suffer exception
				 */
				*pl2_rxisar0 = l2_rxisar0 & ~DP_DL_INT_DLQ0_QDONE_SET;
			}
		}
	} else {
		value = l2_rxisar0 & DP_DL_INT_DLQ1_PITCNT_LEN_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_DL_INT_DLQ1_PITCNT_LEN_ERR;
			para->intr_queues[para->intr_cnt] = BIT(qno);
			para->intr_cnt++;

			dpmaif_dlq_mask_rx_pitcnt_len_err_intr(hw_info, qno);
		}

		value = l2_rxisar0 & DP_DL_INT_DLQ1_QDONE_SET;
		if (value) {
			/* mask RX done interrupt immediately after it occurs */
			if (!mask_dlq_interrupt(dpmaif_ctrl, qno)) {
				para->intr_types[para->intr_cnt] = DPF_INTR_DL_DLQ1_DONE;
				para->intr_queues[para->intr_cnt] = BIT(qno);
				para->intr_cnt++;
			} else {
				/* Unable to clear the interrupt, try again on the next one
				 * device entered low power mode or suffer exception
				 */
				*pl2_rxisar0 = l2_rxisar0 & ~DP_DL_INT_DLQ1_QDONE_SET;
			}
		}
	}
}

/**
 * dpmaif_hw_get_interrupt_status() - Gets interrupt status from HW
 * @dpmaif_ctrl: Pointer to struct dpmaif_ctrl
 * @para: Pointer to struct dpmaif_hw_intr_st_para
 * @qno: Queue number
 *
 * Gets RX/TX interrupt and checks DL/UL interrupt status.
 * Clears interrupt statuses if needed.
 *
 * Return: Interrupt Count
 */
int dpmaif_hw_get_interrupt_status(struct dpmaif_ctrl *dpmaif_ctrl,
				   struct dpmaif_hw_intr_st_para *para, int qno)
{
	struct dpmaif_hw_info *hw_info;
	u32 l2_ri_sar0, l2_ti_sar0 = 0;
	u32 l2_rim_r0, l2_tim_r0 = 0;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	/* get RX interrupt status from HW */
	l2_ri_sar0 = ioread32(hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);
	l2_rim_r0 = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMR0);

	/* TX interrupt status */
	if (qno == DPF_RX_QNO_DFT) {
		/* All ULQ and DLQ0 interrupts use the same source
		 * no need to check ULQ interrupts when a DLQ1
		 * interrupt has occurred.
		 */
		l2_ti_sar0 = ioread32(hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
		l2_tim_r0 = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0);
	}

	/* clear IP busy register wake up CPU case */
	dpmaif_clr_ip_busy_sts(hw_info);

	/* check UL interrupt status */
	if (qno == DPF_RX_QNO_DFT) {
		/* Do not schedule bottom half again or clear UL
		 * interrupt status when we have already masked it.
		 */
		l2_ti_sar0 &= ~l2_tim_r0;
		if (l2_ti_sar0) {
			dpmaif_hw_check_tx_interrupt(dpmaif_ctrl, l2_ti_sar0, para);

			/* clear interrupt status */
			iowrite32(l2_ti_sar0, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
		}
	}

	/* check DL interrupt status */
	if (l2_ri_sar0) {
		if (qno == DPF_RX_QNO0) {
			l2_ri_sar0 &= DP_DL_DLQ0_STATUS_MASK;

			if (l2_rim_r0 & DPMAIF_DL_INT_DLQ0_QDONE)
				/* Do not schedule bottom half again or clear DL
				 * queue done interrupt status when we have already masked it.
				 */
				l2_ri_sar0 &= ~DP_DL_INT_DLQ0_QDONE_SET;
		} else {
			l2_ri_sar0 &= DP_DL_DLQ1_STATUS_MASK;

			if (l2_rim_r0 & DPMAIF_DL_INT_DLQ1_QDONE)
				/* Do not schedule bottom half again or clear DL
				 * queue done interrupt status when we have already masked it.
				 */
				l2_ri_sar0 &= ~DP_DL_INT_DLQ1_QDONE_SET;
		}

		/* have interrupt event */
		if (l2_ri_sar0) {
			dpmaif_hw_check_rx_interrupt(dpmaif_ctrl, &l2_ri_sar0, para, qno);
			/* always clear BATCNT_ERR */
			l2_ri_sar0 |= DP_DL_INT_BATCNT_LEN_ERR;

			/* clear interrupt status */
			iowrite32(l2_ri_sar0, hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);
		}
	}

	return para->intr_cnt;
}

static int dpmaif_sram_init(struct dpmaif_hw_info *hw_info)
{
	u32 value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AP_MEM_CLR);
	value |= DPMAIF_MEM_CLR;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AP_MEM_CLR);

	return readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_AP_MEM_CLR,
					value, !(value & DPMAIF_MEM_CLR),
					0, DPMAIF_CHECK_INIT_TIMEOUT_US);
}

static void dpmaif_hw_reset(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_AP_AO_RST_BIT, hw_info->pcie_base + DPMAIF_AP_AO_RGU_ASSERT);
	udelay(2);
	iowrite32(DPMAIF_AP_RST_BIT, hw_info->pcie_base + DPMAIF_AP_RGU_ASSERT);
	udelay(2);
	iowrite32(DPMAIF_AP_AO_RST_BIT, hw_info->pcie_base + DPMAIF_AP_AO_RGU_DEASSERT);
	udelay(2);
	iowrite32(DPMAIF_AP_RST_BIT, hw_info->pcie_base + DPMAIF_AP_RGU_DEASSERT);
	udelay(2);
}

static int dpmaif_hw_config(struct dpmaif_hw_info *hw_info)
{
	unsigned int value;
	int ret;

	dpmaif_hw_reset(hw_info);
	ret = dpmaif_sram_init(hw_info);
	if (ret)
		return ret;

	/* Set DPMAIF AP port mode */
	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value |= DPMAIF_PORT_MODE_PCIE;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);

	iowrite32(DPMAIF_CG_EN, hw_info->pcie_base + DPMAIF_AP_CG_EN);

	return 0;
}

static inline void dpmaif_pcie_dpmaif_sign(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_PCIE_MODE_SET_VALUE, hw_info->pcie_base + DPMAIF_UL_RESERVE_AO_RW);
}

static void dpmaif_dl_performance(struct dpmaif_hw_info *hw_info)
{
	unsigned int value;

	/* BAT cache enable */
	value = ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
	value |= DPMAIF_DL_BAT_CACHE_PRI;
	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);

	/* PIT burst enable */
	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value |= DPMAIF_DL_BURST_PIT_EN;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
}

static void dpmaif_common_hw_init(struct dpmaif_hw_info *hw_info)
{
	dpmaif_pcie_dpmaif_sign(hw_info);
	dpmaif_dl_performance(hw_info);
}

 /* DPMAIF DL DLQ part HW setting */

static inline void dpmaif_hw_hpc_cntl_set(struct dpmaif_hw_info *hw_info)
{
	unsigned int value;

	value = DPMAIF_HPC_DLQ_PATH_MODE | DPMAIF_HPC_ADD_MODE_DF << 2;
	value |= DPMAIF_HASH_PRIME_DF << 4;
	value |= DPMAIF_HPC_TOTAL_NUM << 8;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_HPC_CNTL);
}

static inline void dpmaif_hw_agg_cfg_set(struct dpmaif_hw_info *hw_info)
{
	unsigned int value;

	value = DPMAIF_AGG_MAX_LEN_DF | DPMAIF_AGG_TBL_ENT_NUM_DF << 16;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_DLQ_AGG_CFG);
}

static inline void dpmaif_hw_hash_bit_choose_set(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_DLQ_HASH_BIT_CHOOSE_DF,
		  hw_info->pcie_base + DPMAIF_AO_DL_DLQPIT_INIT_CON5);
}

static inline void dpmaif_hw_mid_pit_timeout_thres_set(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_MID_TIMEOUT_THRES_DF, hw_info->pcie_base + DPMAIF_AO_DL_DLQPIT_TIMEOUT0);
}

static void dpmaif_hw_dlq_timeout_thres_set(struct dpmaif_hw_info *hw_info)
{
	unsigned int value, i;

	/* each registers holds two DLQ threshold timeout values */
	for (i = 0; i < DPMAIF_HPC_MAX_TOTAL_NUM / 2; i++) {
		value = FIELD_PREP(DPMAIF_DLQ_LOW_TIMEOUT_THRES_MKS, DPMAIF_DLQ_TIMEOUT_THRES_DF);
		value |= FIELD_PREP(DPMAIF_DLQ_HIGH_TIMEOUT_THRES_MSK,
				    DPMAIF_DLQ_TIMEOUT_THRES_DF);
		iowrite32(value,
			  hw_info->pcie_base + DPMAIF_AO_DL_DLQPIT_TIMEOUT1 + sizeof(u32) * i);
	}
}

static inline void dpmaif_hw_dlq_start_prs_thres_set(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_DLQ_PRS_THRES_DF, hw_info->pcie_base + DPMAIF_AO_DL_DLQPIT_TRIG_THRES);
}

static void dpmaif_dl_dlq_hpc_hw_init(struct dpmaif_hw_info *hw_info)
{
	dpmaif_hw_hpc_cntl_set(hw_info);
	dpmaif_hw_agg_cfg_set(hw_info);
	dpmaif_hw_hash_bit_choose_set(hw_info);
	dpmaif_hw_mid_pit_timeout_thres_set(hw_info);
	dpmaif_hw_dlq_timeout_thres_set(hw_info);
	dpmaif_hw_dlq_start_prs_thres_set(hw_info);
}

static int dpmaif_dl_bat_init_done(struct dpmaif_ctrl *dpmaif_ctrl,
				   unsigned char q_num, bool frg_en)
{
	struct dpmaif_hw_info *hw_info;
	u32 value, dl_bat_init = 0;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	if (frg_en)
		dl_bat_init = DPMAIF_DL_BAT_FRG_INIT;

	dl_bat_init |= DPMAIF_DL_BAT_INIT_ALLSET;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_DL_BAT_INIT,
					value, !(value & DPMAIF_DL_BAT_INIT_NOT_READY),
					0, DPMAIF_CHECK_INIT_TIMEOUT_US);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "data plane modem DL BAT is not ready\n");
		return ret;
	}

	iowrite32(dl_bat_init, hw_info->pcie_base + DPMAIF_DL_BAT_INIT);

	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_DL_BAT_INIT,
					value, !(value & DPMAIF_DL_BAT_INIT_NOT_READY),
					0, DPMAIF_CHECK_INIT_TIMEOUT_US);
	if (ret)
		dev_err(dpmaif_ctrl->dev, "data plane modem DL BAT initialization failed\n");

	return ret;
}

static void dpmaif_dl_set_bat_base_addr(struct dpmaif_hw_info *hw_info,
					unsigned char q_num, dma_addr_t addr)
{
	iowrite32(lower_32_bits(addr), hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON0);
	iowrite32(upper_32_bits(addr), hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON3);
}

static void dpmaif_dl_set_bat_size(struct dpmaif_hw_info *hw_info,
				   unsigned char q_num, unsigned int size)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
	value &= ~DPMAIF_BAT_SIZE_MSK;
	value |= size & DPMAIF_BAT_SIZE_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
}

static void dpmaif_dl_bat_en(struct dpmaif_hw_info *hw_info,
			     unsigned char q_num, bool enable)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
	if (enable)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
}

static void dpmaif_dl_set_ao_bid_maxcnt(struct dpmaif_hw_info *hw_info,
					unsigned char q_num, unsigned int cnt)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
	value &= ~DPMAIF_BAT_BID_MAXCNT_MSK;
	value |= FIELD_PREP(DPMAIF_BAT_BID_MAXCNT_MSK, cnt);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
}

static inline void dpmaif_dl_set_ao_mtu(struct dpmaif_hw_info *hw_info, unsigned int mtu_sz)
{
	iowrite32(mtu_sz, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON1);
}

static void dpmaif_dl_set_ao_pit_chknum(struct dpmaif_hw_info *hw_info, unsigned char q_num,
					unsigned int number)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
	value &= ~DPMAIF_PIT_CHK_NUM_MSK;
	value |= FIELD_PREP(DPMAIF_PIT_CHK_NUM_MSK, number);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
}

static void dpmaif_dl_set_ao_remain_minsz(struct dpmaif_hw_info *hw_info, unsigned char q_num,
					  size_t min_sz)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
	value &= ~DPMAIF_BAT_REMAIN_MINSZ_MSK;
	value |= FIELD_PREP(DPMAIF_BAT_REMAIN_MINSZ_MSK, min_sz / DPMAIF_BAT_REMAIN_SZ_BASE);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
}

static void dpmaif_dl_set_ao_bat_bufsz(struct dpmaif_hw_info *hw_info,
				       unsigned char q_num, size_t buf_sz)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
	value &= ~DPMAIF_BAT_BUF_SZ_MSK;
	value |= FIELD_PREP(DPMAIF_BAT_BUF_SZ_MSK, buf_sz / DPMAIF_BAT_BUFFER_SZ_BASE);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
}

static void dpmaif_dl_set_ao_bat_rsv_length(struct dpmaif_hw_info *hw_info, unsigned char q_num,
					    unsigned int length)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
	value &= ~DPMAIF_BAT_RSV_LEN_MSK;
	value |= length & DPMAIF_BAT_RSV_LEN_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
}

static void dpmaif_dl_set_pkt_alignment(struct dpmaif_hw_info *hw_info, unsigned char q_num,
					bool enable, unsigned int mode)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~DPMAIF_PKT_ALIGN_MSK;
	if (enable) {
		value |= DPMAIF_PKT_ALIGN_EN;
		value |= FIELD_PREP(DPMAIF_PKT_ALIGN_MSK, mode);
	}

	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
}

static void dpmaif_dl_set_pkt_checksum(struct dpmaif_hw_info *hw_info)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value |= DPMAIF_DL_PKT_CHECKSUM_EN;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
}

static void dpmaif_dl_set_ao_frg_check_thres(struct dpmaif_hw_info *hw_info, unsigned char q_num,
					     unsigned int size)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~DPMAIF_FRG_CHECK_THRES_MSK;
	value |= (size & DPMAIF_FRG_CHECK_THRES_MSK);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
}

static void dpmaif_dl_set_ao_frg_bufsz(struct dpmaif_hw_info *hw_info,
				       unsigned char q_num, unsigned int buf_sz)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~DPMAIF_FRG_BUF_SZ_MSK;
	value |= FIELD_PREP(DPMAIF_FRG_BUF_SZ_MSK, buf_sz / DPMAIF_FRG_BUFFER_SZ_BASE);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
}

static void dpmaif_dl_frg_ao_en(struct dpmaif_hw_info *hw_info,
				unsigned char q_num, bool enable)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	if (enable)
		value |= DPMAIF_FRG_EN_MSK;
	else
		value &= ~DPMAIF_FRG_EN_MSK;

	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
}

static void dpmaif_dl_set_ao_bat_check_thres(struct dpmaif_hw_info *hw_info,
					     unsigned char q_num, unsigned int size)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~DPMAIF_BAT_CHECK_THRES_MSK;
	value |= FIELD_PREP(DPMAIF_BAT_CHECK_THRES_MSK, size);
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
}

static void dpmaif_dl_set_pit_seqnum(struct dpmaif_hw_info *hw_info,
				     unsigned char q_num, unsigned int seq)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PIT_SEQ_END);
	value &= ~DPMAIF_DL_PIT_SEQ_MSK;
	value |= seq & DPMAIF_DL_PIT_SEQ_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PIT_SEQ_END);
}

static void dpmaif_dl_set_dlq_pit_base_addr(struct dpmaif_hw_info *hw_info,
					    unsigned char q_num, dma_addr_t addr)
{
	iowrite32(lower_32_bits(addr), hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON0);
	iowrite32(upper_32_bits(addr), hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON4);
}

static void dpmaif_dl_set_dlq_pit_size(struct dpmaif_hw_info *hw_info,
				       unsigned char q_num, unsigned int size)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON1);
	value &= ~DPMAIF_PIT_SIZE_MSK;
	value |= size & DPMAIF_PIT_SIZE_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON1);

	iowrite32(0, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON2);
	iowrite32(0, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON3);
	iowrite32(0, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON5);
	iowrite32(0, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON6);
}

static void dpmaif_dl_dlq_pit_en(struct dpmaif_hw_info *hw_info,
				 unsigned char q_num, bool enable)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON3);
	if (enable)
		value |= DPMAIF_DLQPIT_EN_MSK;
	else
		value &= ~DPMAIF_DLQPIT_EN_MSK;

	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON3);
}

static void dpmaif_dl_dlq_pit_init_done(struct dpmaif_ctrl *dpmaif_ctrl,
					unsigned char q_num, unsigned int pit_idx)
{
	struct dpmaif_hw_info *hw_info;
	unsigned int dl_pit_init;
	u32 value;
	int timeout;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	dl_pit_init = DPMAIF_DL_PIT_INIT_ALLSET;
	dl_pit_init |= (pit_idx << DPMAIF_DLQPIT_CHAN_OFS);
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	timeout = readx_poll_timeout_atomic(ioread32,
					    hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT,
					    value, !(value & DPMAIF_DL_PIT_INIT_NOT_READY),
					    DPMAIF_CHECK_DELAY_US, DPMAIF_CHECK_INIT_TIMEOUT_US);
	if (timeout) {
		dev_err(dpmaif_ctrl->dev, "data plane modem DL PIT is not ready\n");
		return;
	}

	iowrite32(dl_pit_init, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT);

	timeout = readx_poll_timeout_atomic(ioread32,
					    hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT,
					    value, !(value & DPMAIF_DL_PIT_INIT_NOT_READY),
					    DPMAIF_CHECK_DELAY_US, DPMAIF_CHECK_INIT_TIMEOUT_US);
	if (timeout)
		dev_err(dpmaif_ctrl->dev, "data plane modem DL PIT initialization failed\n");
}

static void dpmaif_config_dlq_pit_hw(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num,
				     struct dpmaif_dl *dl_que)
{
	struct dpmaif_hw_info *hw_info;
	unsigned int pit_idx = q_num;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	dpmaif_dl_set_dlq_pit_base_addr(hw_info, q_num, dl_que->pit_base);
	dpmaif_dl_set_dlq_pit_size(hw_info, q_num, dl_que->pit_size_cnt);
	dpmaif_dl_dlq_pit_en(hw_info, q_num, true);
	dpmaif_dl_dlq_pit_init_done(dpmaif_ctrl, q_num, pit_idx);
}

static void dpmaif_config_all_dlq_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_info *hw_info;
	int i;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	for (i = 0; i < DPMAIF_RXQ_NUM; i++)
		dpmaif_config_dlq_pit_hw(dpmaif_ctrl, i, &hw_info->dl_que[i]);
}

static void dpmaif_dl_all_queue_en(struct dpmaif_ctrl *dpmaif_ctrl, bool enable)
{
	struct dpmaif_hw_info *hw_info;
	u32 dl_bat_init, value;
	int timeout;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	value = ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);

	if (enable)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
	dl_bat_init = DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	/* update DL BAT setting to HW */
	timeout = readx_poll_timeout_atomic(ioread32, hw_info->pcie_base + DPMAIF_DL_BAT_INIT,
					    value, !(value & DPMAIF_DL_BAT_INIT_NOT_READY),
					    0, DPMAIF_CHECK_TIMEOUT_US);

	if (timeout)
		dev_err(dpmaif_ctrl->dev, "timeout updating BAT setting to HW\n");

	iowrite32(dl_bat_init, hw_info->pcie_base + DPMAIF_DL_BAT_INIT);

	/* wait for HW update */
	timeout = readx_poll_timeout_atomic(ioread32, hw_info->pcie_base + DPMAIF_DL_BAT_INIT,
					    value, !(value & DPMAIF_DL_BAT_INIT_NOT_READY),
					    0, DPMAIF_CHECK_TIMEOUT_US);

	if (timeout)
		dev_err(dpmaif_ctrl->dev, "data plane modem DL BAT is not ready\n");
}

static int dpmaif_config_dlq_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_info *hw_info;
	struct dpmaif_dl_hwq *dl_hw;
	struct dpmaif_dl *dl_que;
	unsigned int queue = 0; /* all queues share one BAT/frag BAT table */
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	dpmaif_dl_dlq_hpc_hw_init(hw_info);

	dl_que = &hw_info->dl_que[queue];
	dl_hw = &hw_info->dl_que_hw[queue];
	if (!dl_que->que_started)
		return -EBUSY;

	dpmaif_dl_set_ao_remain_minsz(hw_info, queue, dl_hw->bat_remain_size);
	dpmaif_dl_set_ao_bat_bufsz(hw_info, queue, dl_hw->bat_pkt_bufsz);
	dpmaif_dl_set_ao_frg_bufsz(hw_info, queue, dl_hw->frg_pkt_bufsz);
	dpmaif_dl_set_ao_bat_rsv_length(hw_info, queue, dl_hw->bat_rsv_length);
	dpmaif_dl_set_ao_bid_maxcnt(hw_info, queue, dl_hw->pkt_bid_max_cnt);

	if (dl_hw->pkt_alignment == 64)
		dpmaif_dl_set_pkt_alignment(hw_info, queue, true, DPMAIF_PKT_ALIGN64_MODE);
	else if (dl_hw->pkt_alignment == 128)
		dpmaif_dl_set_pkt_alignment(hw_info, queue, true, DPMAIF_PKT_ALIGN128_MODE);
	else
		dpmaif_dl_set_pkt_alignment(hw_info, queue, false, 0);

	dpmaif_dl_set_pit_seqnum(hw_info, queue, DPMAIF_DL_PIT_SEQ_VALUE);
	dpmaif_dl_set_ao_mtu(hw_info, dl_hw->mtu_size);
	dpmaif_dl_set_ao_pit_chknum(hw_info, queue, dl_hw->chk_pit_num);
	dpmaif_dl_set_ao_bat_check_thres(hw_info, queue, dl_hw->chk_bat_num);
	dpmaif_dl_set_ao_frg_check_thres(hw_info, queue, dl_hw->chk_frg_num);
	/* enable frag feature */
	dpmaif_dl_frg_ao_en(hw_info, queue, true);
	/* init frag use same BAT register */
	dpmaif_dl_set_bat_base_addr(hw_info, queue, dl_que->frg_base);
	dpmaif_dl_set_bat_size(hw_info, queue, dl_que->frg_size_cnt);
	dpmaif_dl_bat_en(hw_info, queue, true);
	ret = dpmaif_dl_bat_init_done(dpmaif_ctrl, queue, true);
	if (ret)
		return ret;

	/* normal BAT init */
	dpmaif_dl_set_bat_base_addr(hw_info, queue, dl_que->bat_base);
	dpmaif_dl_set_bat_size(hw_info, queue, dl_que->bat_size_cnt);
	/* enable BAT */
	dpmaif_dl_bat_en(hw_info, queue, false);
	/* notify HW init/setting done */
	ret = dpmaif_dl_bat_init_done(dpmaif_ctrl, queue, false);
	if (ret)
		return ret;

	/* init PIT (two PIT table) */
	dpmaif_config_all_dlq_hw(dpmaif_ctrl);
	dpmaif_dl_all_queue_en(dpmaif_ctrl, true);

	dpmaif_dl_set_pkt_checksum(hw_info);

	return ret;
}

static void dpmaif_ul_update_drb_size(struct dpmaif_hw_info *hw_info,
				      unsigned char q_num, unsigned int size)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));
	value &= ~DPMAIF_DRB_SIZE_MSK;
	value |= size & DPMAIF_DRB_SIZE_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));
}

static void dpmaif_ul_update_drb_base_addr(struct dpmaif_hw_info *hw_info,
					   unsigned char q_num, dma_addr_t addr)
{
	iowrite32(lower_32_bits(addr), hw_info->pcie_base + DPMAIF_ULQSAR_n(q_num));
	iowrite32(upper_32_bits(addr), hw_info->pcie_base + DPMAIF_UL_DRB_ADDRH_n(q_num));
}

static void dpmaif_ul_rdy_en(struct dpmaif_hw_info *hw_info,
			     unsigned char q_num, bool ready)
{
	u32 value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
	if (ready)
		value |= BIT(q_num);
	else
		value &= ~BIT(q_num);

	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
}

static void dpmaif_ul_arb_en(struct dpmaif_hw_info *hw_info,
			     unsigned char q_num, bool enable)
{
	u32 value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);

	if (enable)
		value |= BIT(q_num + 8);
	else
		value &= ~BIT(q_num + 8);

	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
}

static void dpmaif_config_ulq_hw(struct dpmaif_hw_info *hw_info)
{
	struct dpmaif_ul *ul_que;
	int i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		ul_que = &hw_info->ul_que[i];
		if (ul_que->que_started) {
			dpmaif_ul_update_drb_size(hw_info, i, ul_que->drb_size_cnt *
						  DPMAIF_UL_DRB_ENTRY_WORD);
			dpmaif_ul_update_drb_base_addr(hw_info, i, ul_que->drb_base);
			dpmaif_ul_rdy_en(hw_info, i, true);
			dpmaif_ul_arb_en(hw_info, i, true);
		} else {
			dpmaif_ul_arb_en(hw_info, i, false);
		}
	}
}

static int dpmaif_hw_init_done(struct dpmaif_hw_info *hw_info)
{
	u32 value;
	int ret;

	/* sync default value to SRAM */
	value = ioread32(hw_info->pcie_base + DPMAIF_AP_OVERWRITE_CFG);
	value |= DPMAIF_SRAM_SYNC;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AP_OVERWRITE_CFG);

	ret = readx_poll_timeout_atomic(ioread32, hw_info->pcie_base + DPMAIF_AP_OVERWRITE_CFG,
					value, !(value & DPMAIF_SRAM_SYNC),
					0, DPMAIF_CHECK_TIMEOUT_US);
	if (ret)
		return ret;

	iowrite32(DPMAIF_UL_INIT_DONE, hw_info->pcie_base + DPMAIF_AO_UL_INIT_SET);
	iowrite32(DPMAIF_DL_INIT_DONE, hw_info->pcie_base + DPMAIF_AO_DL_INIT_SET);
	return 0;
}

static int dpmaif_config_que_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_info *hw_info;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	dpmaif_common_hw_init(hw_info);
	ret = dpmaif_config_dlq_hw(dpmaif_ctrl);
	if (ret)
		return ret;

	dpmaif_config_ulq_hw(hw_info);
	return dpmaif_hw_init_done(hw_info);
}

static inline bool dpmaif_dl_idle_check(struct dpmaif_hw_info *hw_info)
{
	u32 value = ioread32(hw_info->pcie_base + DPMAIF_DL_CHK_BUSY);

	/* If total queue is idle, then all DL is idle.
	 * 0: idle, 1: busy
	 */
	return !(value & DPMAIF_DL_IDLE_STS);
}

static void dpmaif_ul_all_queue_en(struct dpmaif_hw_info *hw_info, bool enable)
{
	u32 ul_arb_en = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);

	if (enable)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;

	iowrite32(ul_arb_en, hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
}

static inline int dpmaif_ul_idle_check(struct dpmaif_hw_info *hw_info)
{
	u32 value = ioread32(hw_info->pcie_base + DPMAIF_UL_CHK_BUSY);

	/* If total queue is idle, then all UL is idle.
	 * 0: idle, 1: busy
	 */
	return !(value & DPMAIF_UL_IDLE_STS);
}

 /* DPMAIF UL Part HW setting */

int dpmaif_ul_add_wcnt(struct dpmaif_ctrl *dpmaif_ctrl,
		       unsigned char q_num, unsigned int drb_entry_cnt)
{
	struct dpmaif_hw_info *hw_info;
	u32 ul_update, value;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	ul_update = drb_entry_cnt & DPMAIF_UL_ADD_COUNT_MASK;
	ul_update |= DPMAIF_UL_ADD_UPDATE;

	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_ULQ_ADD_DESC_CH_n(q_num),
					value, !(value & DPMAIF_UL_ADD_NOT_READY),
					0, DPMAIF_CHECK_TIMEOUT_US);

	if (ret) {
		dev_err(dpmaif_ctrl->dev, "UL add is not ready\n");
		return ret;
	}

	iowrite32(ul_update, hw_info->pcie_base + DPMAIF_ULQ_ADD_DESC_CH_n(q_num));

	ret = readx_poll_timeout_atomic(ioread32,
					hw_info->pcie_base + DPMAIF_ULQ_ADD_DESC_CH_n(q_num),
					value, !(value & DPMAIF_UL_ADD_NOT_READY),
					0, DPMAIF_CHECK_TIMEOUT_US);

	if (ret) {
		dev_err(dpmaif_ctrl->dev, "timeout updating UL add\n");
		return ret;
	}

	return 0;
}

unsigned int dpmaif_ul_get_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num)
{
	unsigned int value = ioread32(hw_info->pcie_base + DPMAIF_ULQ_STA0_n(q_num));

	return value >> DPMAIF_UL_DRB_RIDX_OFFSET;
}

int dpmaif_dl_add_dlq_pit_remain_cnt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int dlq_pit_idx,
				     unsigned int pit_remain_cnt)
{
	struct dpmaif_hw_info *hw_info;
	u32 dl_update, value;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	dl_update = pit_remain_cnt & DPMAIF_PIT_REM_CNT_MSK;
	dl_update |= DPMAIF_DL_ADD_UPDATE | (dlq_pit_idx << DPMAIF_ADD_DLQ_PIT_CHAN_OFS);

	ret = readx_poll_timeout_atomic(ioread32, hw_info->pcie_base + DPMAIF_DL_DLQPIT_ADD,
					value, !(value & DPMAIF_DL_ADD_NOT_READY),
					0, DPMAIF_CHECK_TIMEOUT_US);

	if (ret) {
		dev_err(dpmaif_ctrl->dev, "data plane modem is not ready to add dlq\n");
		return ret;
	}

	iowrite32(dl_update, hw_info->pcie_base + DPMAIF_DL_DLQPIT_ADD);

	ret = readx_poll_timeout_atomic(ioread32, hw_info->pcie_base + DPMAIF_DL_DLQPIT_ADD,
					value, !(value & DPMAIF_DL_ADD_NOT_READY),
					0, DPMAIF_CHECK_TIMEOUT_US);

	if (ret) {
		dev_err(dpmaif_ctrl->dev, "data plane modem add dlq failed\n");
		return ret;
	}

	return 0;
}

unsigned int dpmaif_dl_dlq_pit_get_wridx(struct dpmaif_hw_info *hw_info, unsigned int dlq_pit_idx)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_DLQ_WRIDX +
			dlq_pit_idx * DLQ_PIT_IDX_SIZE) & DPMAIF_DL_PIT_WRIDX_MSK;
}

static bool dl_add_timedout(struct dpmaif_hw_info *hw_info)
{
	u32 value;
	int ret;

	ret = readx_poll_timeout_atomic(ioread32, hw_info->pcie_base + DPMAIF_DL_BAT_ADD,
					value, !(value & DPMAIF_DL_ADD_NOT_READY),
					0, DPMAIF_CHECK_TIMEOUT_US);

	if (ret)
		return true;

	return false;
}

int dpmaif_dl_add_bat_cnt(struct dpmaif_ctrl *dpmaif_ctrl,
			  unsigned char q_num, unsigned int bat_entry_cnt)
{
	struct dpmaif_hw_info *hw_info;
	unsigned int value;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	value = bat_entry_cnt & DPMAIF_DL_ADD_COUNT_MASK;
	value |= DPMAIF_DL_ADD_UPDATE;

	if (dl_add_timedout(hw_info)) {
		dev_err(dpmaif_ctrl->dev, "DL add BAT not ready. count: %u queue: %dn",
			bat_entry_cnt, q_num);
		return -EBUSY;
	}

	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_ADD);

	if (dl_add_timedout(hw_info)) {
		dev_err(dpmaif_ctrl->dev, "DL add BAT timeout. count: %u queue: %dn",
			bat_entry_cnt, q_num);
		return -EBUSY;
	}

	return 0;
}

unsigned int dpmaif_dl_get_bat_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_BAT_RIDX) & DPMAIF_DL_BAT_WRIDX_MSK;
}

unsigned int dpmaif_dl_get_bat_wridx(struct dpmaif_hw_info *hw_info, unsigned char q_num)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_BAT_WRIDX) & DPMAIF_DL_BAT_WRIDX_MSK;
}

int dpmaif_dl_add_frg_cnt(struct dpmaif_ctrl *dpmaif_ctrl,
			  unsigned char q_num, unsigned int frg_entry_cnt)
{
	struct dpmaif_hw_info *hw_info;
	unsigned int value;

	hw_info = &dpmaif_ctrl->hif_hw_info;
	value = frg_entry_cnt & DPMAIF_DL_ADD_COUNT_MASK;
	value |= DPMAIF_DL_FRG_ADD_UPDATE;
	value |= DPMAIF_DL_ADD_UPDATE;

	if (dl_add_timedout(hw_info)) {
		dev_err(dpmaif_ctrl->dev, "Data plane modem is not ready to add frag DLQ\n");
		return -EBUSY;
	}

	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_ADD);

	if (dl_add_timedout(hw_info)) {
		dev_err(dpmaif_ctrl->dev, "Data plane modem add frag DLQ failed");
		return -EBUSY;
	}

	return 0;
}

unsigned int dpmaif_dl_get_frg_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_FRGBAT_WRIDX) & DPMAIF_DL_FRG_WRIDX_MSK;
}

static void dpmaif_set_queue_property(struct dpmaif_hw_info *hw_info,
				      struct dpmaif_hw_params *init_para)
{
	struct dpmaif_dl_hwq *dl_hwq;
	struct dpmaif_dl *dl_que;
	struct dpmaif_ul *ul_que;
	int i;

	for (i = 0; i < DPMAIF_RXQ_NUM; i++) {
		dl_hwq = &hw_info->dl_que_hw[i];
		dl_hwq->bat_remain_size = DPMAIF_HW_BAT_REMAIN;
		dl_hwq->bat_pkt_bufsz = DPMAIF_HW_BAT_PKTBUF;
		dl_hwq->frg_pkt_bufsz = DPMAIF_HW_FRG_PKTBUF;
		dl_hwq->bat_rsv_length = DPMAIF_HW_BAT_RSVLEN;
		dl_hwq->pkt_bid_max_cnt = DPMAIF_HW_PKT_BIDCNT;
		dl_hwq->pkt_alignment = DPMAIF_HW_PKT_ALIGN;
		dl_hwq->mtu_size = DPMAIF_HW_MTU_SIZE;
		dl_hwq->chk_bat_num = DPMAIF_HW_CHK_BAT_NUM;
		dl_hwq->chk_frg_num = DPMAIF_HW_CHK_FRG_NUM;
		dl_hwq->chk_pit_num = DPMAIF_HW_CHK_PIT_NUM;

		dl_que = &hw_info->dl_que[i];
		dl_que->bat_base = init_para->pkt_bat_base_addr[i];
		dl_que->bat_size_cnt = init_para->pkt_bat_size_cnt[i];
		dl_que->pit_base = init_para->pit_base_addr[i];
		dl_que->pit_size_cnt = init_para->pit_size_cnt[i];
		dl_que->frg_base = init_para->frg_bat_base_addr[i];
		dl_que->frg_size_cnt = init_para->frg_bat_size_cnt[i];
		dl_que->que_started = true;
	}

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		ul_que = &hw_info->ul_que[i];
		ul_que->drb_base = init_para->drb_base_addr[i];
		ul_que->drb_size_cnt = init_para->drb_size_cnt[i];
		ul_que->que_started = true;
	}
}

/**
 * dpmaif_hw_stop_tx_queue() - Stop all TX queues
 * @dpmaif_ctrl: Pointer to struct dpmaif_ctrl
 *
 * Disable HW UL queue. Checks busy UL queues to go to idle
 * with an attempt count of 1000000.
 *
 * Return: 0 on success and -ETIMEDOUT upon a timeout
 */
int dpmaif_hw_stop_tx_queue(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_info *hw_info;
	int count = 0;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	/* disables HW UL arb queue and check for idle */
	dpmaif_ul_all_queue_en(hw_info, false);
	while (dpmaif_ul_idle_check(hw_info)) {
		if (++count >= DPMAIF_MAX_CHECK_COUNT) {
			dev_err(dpmaif_ctrl->dev, "Stop TX failed, 0x%x\n",
				ioread32(hw_info->pcie_base + DPMAIF_UL_CHK_BUSY));
			return -ETIMEDOUT;
		}
	}

	return 0;
}

/**
 * dpmaif_hw_stop_rx_queue() - Stop all RX queues
 * @dpmaif_ctrl: Pointer to struct dpmaif_ctrl
 *
 * Disable HW DL queue. Checks busy UL queues to go to idle
 * with an attempt count of 1000000.
 * Check that HW PIT write index equals read index with the same
 * attempt count.
 *
 * Return: 0 on success and -ETIMEDOUT upon a timeout
 */
int dpmaif_hw_stop_rx_queue(struct dpmaif_ctrl *dpmaif_ctrl)
{
	struct dpmaif_hw_info *hw_info;
	unsigned int wridx;
	unsigned int ridx;
	int count = 0;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	/* disables HW DL queue and check idle */
	dpmaif_dl_all_queue_en(dpmaif_ctrl, false);
	while (dpmaif_dl_idle_check(hw_info)) {
		if (++count >= DPMAIF_MAX_CHECK_COUNT) {
			dev_err(dpmaif_ctrl->dev, "stop RX failed, 0x%x\n",
				ioread32(hw_info->pcie_base + DPMAIF_DL_CHK_BUSY));
			return -ETIMEDOUT;
		}
	}

	/* check middle PIT sync done */
	count = 0;
	do {
		wridx = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PIT_WRIDX) &
			DPMAIF_DL_PIT_WRIDX_MSK;
		ridx = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PIT_RIDX) &
			DPMAIF_DL_PIT_WRIDX_MSK;
		if (wridx == ridx)
			return 0;

		if (++count >= DPMAIF_MAX_CHECK_COUNT) {
			dev_err(dpmaif_ctrl->dev, "check middle PIT sync fail\n");
			break;
		}
	} while (1);

	return -ETIMEDOUT;
}

void dpmaif_start_hw(struct dpmaif_ctrl *dpmaif_ctrl)
{
	dpmaif_ul_all_queue_en(&dpmaif_ctrl->hif_hw_info, true);
	dpmaif_dl_all_queue_en(dpmaif_ctrl, true);
}

/**
 * dpmaif_hw_init() - Initialize HW data path API
 * @dpmaif_ctrl: Pointer to struct dpmaif_ctrl
 * @init_param: Pointer to struct dpmaif_hw_params
 *
 * Configures port mode, clock config, HW interrupt init,
 * and HW queue config.
 *
 * Return: 0 on success and -error code upon failure
 */
int dpmaif_hw_init(struct dpmaif_ctrl *dpmaif_ctrl, struct dpmaif_hw_params *init_param)
{
	struct dpmaif_hw_info *hw_info;
	int ret;

	hw_info = &dpmaif_ctrl->hif_hw_info;

	/* port mode & clock config */
	ret = dpmaif_hw_config(hw_info);

	if (ret) {
		dev_err(dpmaif_ctrl->dev, "sram_init check fail\n");
		return ret;
	}

	/* HW interrupt init */
	ret = dpmaif_init_intr(hw_info);
	if (ret) {
		dev_err(dpmaif_ctrl->dev, "init_intr check fail\n");
		return ret;
	}

	/* HW queue config */
	dpmaif_set_queue_property(hw_info, init_param);
	ret = dpmaif_config_que_hw(dpmaif_ctrl);
	if (ret)
		dev_err(dpmaif_ctrl->dev, "DPMAIF hw_init_done check fail\n");

	return ret;
}

bool dpmaif_hw_check_clr_ul_done_status(struct dpmaif_hw_info *hw_info, unsigned char qno)
{
	u32 value = ioread32(hw_info->pcie_base + DPMAIF_AP_L2TISAR0);

	value &= BIT(DP_UL_INT_DONE_OFFSET + qno);
	if (value) {
		/* clear interrupt status */
		iowrite32(value, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
		return true;
	}

	return false;
}
