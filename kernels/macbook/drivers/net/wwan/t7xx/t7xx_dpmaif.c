// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/delay.h>

#include "t7xx_reg.h"
#include "t7xx_dpmaif.h"

static void dpmaif_init_intr(struct dpmaif_hw_info *hw_info)
{
	struct dpmaif_isr_en_mask *isr_en_msk = &hw_info->isr_en_mask;
	int count = 0;
	u32 cfg;

	/* Set UL interrupt */
	isr_en_msk->ap_ul_l2intr_en_msk = DPMAIF_UL_INT_ERR_MSK |
		DPMAIF_UL_INT_QDONE_MSK;

	/* clear dummy sts */
	iowrite32(0xFFFFFFFF, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);

	/* set interrupt enable mask */
	iowrite32(isr_en_msk->ap_ul_l2intr_en_msk,
		  hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMCR0);

	iowrite32(~isr_en_msk->ap_ul_l2intr_en_msk,
		  hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMSR0);

	/* check mask sts */
	while ((ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0) &
		isr_en_msk->ap_ul_l2intr_en_msk) == isr_en_msk->ap_ul_l2intr_en_msk) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("init_intr-1 check fail\n");
			return;
		}
	}

	/* Set DL interrupt */
	isr_en_msk->ap_dl_l2intr_err_en_msk = DPMAIF_DL_INT_PITCNT_LEN_ERR |
					      DPMAIF_DL_INT_BATCNT_LEN_ERR;

	isr_en_msk->ap_dl_l2intr_en_msk = DPMAIF_DL_INT_DLQ0_QDONE |
					  DPMAIF_DL_INT_DLQ1_QDONE |
					  DPMAIF_DL_INT_DLQ0_PITCNT_LEN |
					  DPMAIF_DL_INT_DLQ1_PITCNT_LEN;

	/* clear dummy sts */
	iowrite32(0xFFFFFFFF, hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);

	/* set interrupt enable mask. DL ISR PD part */
	iowrite32(~isr_en_msk->ap_dl_l2intr_en_msk,
		  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);

	/* check mask sts */
	count = 0;
	while ((ioread32(hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMR0) &
		isr_en_msk->ap_dl_l2intr_en_msk) == isr_en_msk->ap_dl_l2intr_en_msk) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("init_intr-2 check fail\n");
			return;
		}
	}

	/* Set AP IP busy */
	isr_en_msk->ap_udl_ip_busy_en_msk = DPMAIF_UDL_IP_BUSY;

	/* clear dummy sts */
	iowrite32(0xFFFFFFFF, hw_info->pcie_base + DPMAIF_AP_IP_BUSY);

	/* set IP busy unmask */
	iowrite32(isr_en_msk->ap_udl_ip_busy_en_msk,
		  hw_info->pcie_base + DPMAIF_AO_AP_DLUL_IP_BUSY_MASK);

	cfg = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L1TIMR0);
	cfg |= DPMAIF_DL_INT_Q2APTOP | DPMAIF_DL_INT_Q2TOQ1;
	iowrite32(cfg, hw_info->pcie_base + DPMAIF_AO_UL_AP_L1TIMR0);
	iowrite32(0xffff, hw_info->pcie_base + DPMAIF_HPC_INTR_MASK);

	count = 0;
	while (count < MAIFQ_MAX_DLQ_NUM) {
		hw_info->dlq_mask_fail_cnt[count] = 0;
		count++;
	}
}

static void dpmaif_mask_ul_que_interrupt(struct dpmaif_hw_info *hw_info, unsigned int q_num)
{
	struct dpmaif_isr_en_mask *isr_en_msk = &hw_info->isr_en_mask;
	u32 ui_que_done_mask;
	int count = 0;

	ui_que_done_mask = BIT(q_num + _UL_INT_DONE_OFFSET) & DPMAIF_UL_INT_QDONE_MSK;

	isr_en_msk->ap_ul_l2intr_en_msk &= ~ui_que_done_mask;
	iowrite32(ui_que_done_mask, hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMSR0);

	/* check mask status */
	while ((ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0) &
		ui_que_done_mask) != ui_que_done_mask) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("mask_ul check fail\n");
			return;
		}
	}
}

void dpmaif_unmask_ul_que_interrupt(struct dpmaif_hw_info *hw_info, unsigned int q_num)
{
	struct dpmaif_isr_en_mask *isr_en_msk;
	unsigned int ui_que_done_mask;
	int count = 0;

	isr_en_msk = &hw_info->isr_en_mask;

	ui_que_done_mask = BIT(q_num + _UL_INT_DONE_OFFSET) & DPMAIF_UL_INT_QDONE_MSK;
	isr_en_msk->ap_ul_l2intr_en_msk |= ui_que_done_mask;
	iowrite32(ui_que_done_mask, hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMCR0);

	/* check mask status */
	while ((ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0) &
		ui_que_done_mask) == ui_que_done_mask) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("unmask_ul check fail\n");
			return;
		}
	}
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

static int mask_dlq_interrupt(struct dpmaif_hw_info *hw_info, unsigned char qno)
{
	u32 q_done = (qno == DPF_RX_QNO0) ? DPMAIF_DL_INT_DLQ0_QDONE : DPMAIF_DL_INT_DLQ1_QDONE;
	int count = 0;

	iowrite32(q_done, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);

	/* check mask sts */
	while (!(ioread32(hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMR0) & q_done)) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("mask_dl_dlq0 check fail\n");
			return -EBUSY;
		}

		iowrite32(q_done, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
	}

	hw_info->isr_en_mask.ap_dl_l2intr_en_msk &= ~q_done;
	hw_info->dlq_mask_fail_cnt[qno] = 0;

	return 0;
}

void dpmaif_hw_dl_dlq_unmask_rx_done_intr(struct dpmaif_hw_info *hw_info, unsigned char qno)
{
	u32 mask = (qno == DPF_RX_QNO0) ? DPMAIF_DL_INT_DLQ0_QDONE : DPMAIF_DL_INT_DLQ1_QDONE;

	iowrite32(mask, hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMCR0);
	hw_info->isr_en_mask.ap_dl_l2intr_en_msk |= mask;
}

void dpmaif_clr_ip_busy_sts(struct dpmaif_hw_info *hw_info)
{
	u32 ip_busy_sts = ioread32(hw_info->pcie_base + DPMAIF_AP_IP_BUSY);

	iowrite32(ip_busy_sts, hw_info->pcie_base + DPMAIF_AP_IP_BUSY);
}

static void dpmaif_dlq_mask_rx_pit_cnt_len_err_intr(struct dpmaif_hw_info *hw_info,
						    unsigned char qno)
{
	if (qno == DPF_RX_QNO0)
		iowrite32(DPMAIF_DL_INT_DLQ0_PITCNT_LEN,
			  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
	else
		iowrite32(DPMAIF_DL_INT_DLQ1_PITCNT_LEN,
			  hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMSR0);
}

void dpmaif_dlq_unmask_rx_pit_cnt_len_err_intr(struct dpmaif_hw_info *hw_info,
					       unsigned char qno)
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
	iowrite32(0xFFFFFFFF, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
}

void dpmaif_clr_dl_all_interrupt(struct dpmaif_hw_info *hw_info)
{
	iowrite32(0xFFFFFFFF, hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);
}

static void dpmaif_hw_ul_mask_multiple_tx_done_intr(struct dpmaif_hw_info *hw_info,
						    unsigned char que_mask)
{
	unsigned int index;

	for (index = 0; index < MAIFQ_MAX_ULQ_NUM; index++) {
		if (que_mask & BIT(index))
			dpmaif_mask_ul_que_interrupt(hw_info, index);
	}
}

static void dpmaif_hw_check_tx_interrupt(struct dpmaif_hw_info *hw_info,
					 unsigned int l2_txisar0,
					 struct dpmaif_hw_intr_st_para *para)
{
	unsigned int value;

	value = l2_txisar0 & DP_UL_INT_QDONE_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_DONE;
		para->intr_queues[para->intr_cnt] =
			value >> DP_UL_INT_DONE_OFFSET & DP_UL_QNUM_MSK;

		/* we mask TX queue interrupt immediately after it occurs */
		dpmaif_hw_ul_mask_multiple_tx_done_intr(hw_info,
							para->intr_queues[para->intr_cnt]);

		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_EMPTY_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_DRB_EMPTY;
		para->intr_queues[para->intr_cnt] =
			value >> DP_UL_INT_EMPTY_OFFSET & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_MD_NOTREADY_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_MD_NOTREADY;
		para->intr_queues[para->intr_cnt] =
			value >> DP_UL_INT_MD_NOTRDY_OFFSET & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_MD_PWR_NOTREADY_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_MD_PWR_NOTREADY;
		para->intr_queues[para->intr_cnt] =
			value >> DP_UL_INT_PWR_NOTRDY_OFFSET & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}

	value = l2_txisar0 & DP_UL_INT_ERR_MSK;
	if (value) {
		para->intr_types[para->intr_cnt] = DPF_INTR_UL_LEN_ERR;
		para->intr_queues[para->intr_cnt] =
			value >> DP_UL_INT_LEN_ERR_OFFSET & DP_UL_QNUM_MSK;
		para->intr_cnt++;
	}
}

static void dpmaif_hw_check_rx_interrupt(struct dpmaif_hw_info *hw_info,
					 unsigned int *pl2_rxisar0,
					 struct dpmaif_hw_intr_st_para *para,
					 int qno)
{
	unsigned int l2_rxisar0 = *pl2_rxisar0;
	unsigned int value;

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

			/* mask this interrupt */
			dpmaif_mask_dl_batcnt_len_err_interrupt(hw_info);
		}

		value = l2_rxisar0 & DP_DL_INT_PITCNT_LEN_ERR;
		if (value) {
			para->intr_types[para->intr_cnt] = DPF_INTR_DL_PITCNT_LEN_ERR;
			para->intr_queues[para->intr_cnt] = DPF_RX_QNO_DFT;
			para->intr_cnt++;

			/* mask this interrupt */
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

			/* mask this interrupt */
			dpmaif_dlq_mask_rx_pit_cnt_len_err_intr(hw_info, qno);
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
			if (!mask_dlq_interrupt(hw_info, qno)) {
				para->intr_types[para->intr_cnt] = DPF_INTR_DL_DLQ0_DONE;
				para->intr_queues[para->intr_cnt] = BIT(qno);
				para->intr_cnt++;
			} else {
				/* mask failed, try again in next interrupt
				 * as we cannot clear it now
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

			/* mask this interrupt */
			dpmaif_dlq_mask_rx_pit_cnt_len_err_intr(hw_info, qno);
		}

		value = l2_rxisar0 & DP_DL_INT_DLQ1_QDONE_SET;
		if (value) {
			/* mask RX done interrupt immediately after it occurs */
			if (!mask_dlq_interrupt(hw_info, qno)) {
				para->intr_types[para->intr_cnt] = DPF_INTR_DL_DLQ1_DONE;
				para->intr_queues[para->intr_cnt] = BIT(qno);
				para->intr_cnt++;
			} else {
				/* mask failed, try again in next interrupt
				 * as we cannot clear it now
				 */
				*pl2_rxisar0 = l2_rxisar0 & ~DP_DL_INT_DLQ1_QDONE_SET;
			}
		}
	}
}

int dpmaif_hw_get_interrupt_status(struct dpmaif_hw_info *hw_info,
				   void *para, int qno)
{
	struct dpmaif_hw_intr_st_para *intr_para;
	u32 L2RISAR0, L2TISAR0 = 0;
	u32 L2RIMR0, L2TIMR0 = 0;

	if (!para) {
		pr_err("Invalid inputs: NULL pointer!\n");
		return -EINVAL;
	}

	intr_para = (struct dpmaif_hw_intr_st_para *)para;

	memset(intr_para, 0, sizeof(struct dpmaif_hw_intr_st_para));

	/* gets interrupt status from HW */
	/* RX interrupt status */
	L2RISAR0 = ioread32(hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);
	L2RIMR0 = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_APDL_L2TIMR0);

	/* TX interrupt status */
	if (qno == DPF_RX_QNO_DFT) {
		/* all ULQ and DLQ0 interrupts use the same source
		 * no need to check ULQ interrupts when a DLQ1
		 * interrupt has occurred
		 */
		L2TISAR0 = ioread32(hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
		L2TIMR0 = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_AP_L2TIMR0);
	}

	/* clear IP busy register wake up cpu case */
	dpmaif_clr_ip_busy_sts(hw_info);

	/* check UL interrupt status */
	if (qno == DPF_RX_QNO_DFT) {
		/* don't schedule bottom half again or clear UL
		 * interrupt status when we have already masked it
		 */
		L2TISAR0 &= ~L2TIMR0;
		if (L2TISAR0) {
			dpmaif_hw_check_tx_interrupt(hw_info, L2TISAR0, intr_para);

			/* clear interrupt status */
			iowrite32(L2TISAR0, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
		}
	}

	/* check DL interrupt status */
	if (L2RISAR0) {
		if (qno == DPF_RX_QNO0) {
			L2RISAR0 &= DP_DL_DLQ0_STATUS_MASK;

			if (L2RIMR0 & DPMAIF_DL_INT_DLQ0_QDONE) {
				/* for debug: DLQ0 isr occur, but DLQ0 irq has been masked. */
				if (!L2TISAR0 && !(L2RISAR0 & DP_DL_INT_DLQ0_PITCNT_LEN_ERR) &&
				    L2RISAR0 & DP_DL_INT_DLQ0_QDONE_SET)
					hw_info->dlq_mask_fail_cnt[qno]++;
				/* don't schedule bottom half again or clear DL
				 * queue done interrupt status when we have already masked it
				 */
				L2RISAR0 &= ~DP_DL_INT_DLQ0_QDONE_SET;
			}
		} else {
			L2RISAR0 &= DP_DL_DLQ1_STATUS_MASK;

			if (L2RIMR0 & DPMAIF_DL_INT_DLQ1_QDONE) {
				/* for debug: DLQ1 isr occur, but DLQ1 irq has been masked. */
				if (!(L2RISAR0 & DP_DL_INT_DLQ1_PITCNT_LEN_ERR) &&
				    L2RISAR0 & DP_DL_INT_DLQ1_QDONE_SET)
					hw_info->dlq_mask_fail_cnt[qno]++;
				/* don't schedule bottom half again or clear DL
				 * queue done interrupt status when we have already masked it
				 */
				L2RISAR0 &= ~DP_DL_INT_DLQ1_QDONE_SET;
			}
		}

		/* have interrupt event */
		if (L2RISAR0) {
			dpmaif_hw_check_rx_interrupt(hw_info, &L2RISAR0, intr_para, qno);
			/* always clear BATCNT_ERR */
			L2RISAR0 |= DP_DL_INT_BATCNT_LEN_ERR;

			/* clear interrupt status */
			iowrite32(L2RISAR0,
				  hw_info->pcie_base + DPMAIF_AP_APDL_L2TISAR0);
		}
	}

	return intr_para->intr_cnt;
}

static void dpmaif_sram_init(struct dpmaif_hw_info *hw_info)
{
	unsigned int count = 0;
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AP_MEM_CLR);
	value |= DPMAIF_MEM_CLR;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AP_MEM_CLR);

	while (ioread32(hw_info->pcie_base + DPMAIF_AP_MEM_CLR) &
		DPMAIF_MEM_CLR) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("sram_init check fail\n");
			break;
		}
	}
}

static void dpmaif_hw_reset(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_AP_AO_RST_BIT, hw_info->pcie_base + DPMAIF_AP_AO_RGU_ASSERT);
	iowrite32(DPMAIF_AP_RST_BIT, hw_info->pcie_base + DPMAIF_AP_RGU_ASSERT);
	udelay(2);
	iowrite32(DPMAIF_AP_AO_RST_BIT, hw_info->pcie_base + DPMAIF_AP_AO_RGU_DEASSERT);
	iowrite32(DPMAIF_AP_RST_BIT, hw_info->pcie_base + DPMAIF_AP_RGU_DEASSERT);
	usleep_range(10, 20);
}

static void dpmaif_hw_config(struct dpmaif_hw_info *hw_info)
{
	unsigned int value;

	dpmaif_hw_reset(hw_info);
	dpmaif_sram_init(hw_info);

	/* Set DPMAIF AP port mode */
	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value |= DPMAIF_PORT_MODE_PCIE;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);

	/* Set CG en */
	iowrite32(0x7f, hw_info->pcie_base + DPMAIF_AP_CG_EN);
}

static inline void dpmaif_pcie_dpmaif_sign(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_PCIE_MODE_SET_VALUE,
		  hw_info->pcie_base + DPMAIF_UL_RESERVE_AO_RW);
}

static void dpmaif_dl_performance(struct dpmaif_hw_info *hw_info)
{
	unsigned int tmp;

	/* BAT cache enable */
	tmp = ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
	tmp |= DPMAIF_DL_BAT_CACHE_PRI;
	iowrite32(tmp, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);

	/* PIT burst en */
	tmp = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	tmp |= DPMAIF_DL_BURST_PIT_EN;
	iowrite32(tmp, hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
}

static void dpmaif_common_hw_init(struct dpmaif_hw_info *hw_info)
{
	dpmaif_pcie_dpmaif_sign(hw_info);
	dpmaif_dl_performance(hw_info);
}

/***********************************************************************
 *	DPMAIF DL DLQ part HW setting
 *
 ***********************************************************************/
static inline void dpmaif_hw_hpc_cntl_set(struct dpmaif_hw_info *hw_info)
{
	unsigned int cfg;

	cfg = (DPMAIF_HPC_DLQ_PATH_MODE & 0x3) << 0;
	cfg |= (DPMAIF_HPC_ADD_MODE_DF & 0x3) << 2;
	cfg |= (DPMAIF_HASH_PRIME_DF & 0xf) << 4;
	cfg |= (DPMAIF_HPC_TOTAL_NUM & 0xff) << 8;
	iowrite32(cfg, hw_info->pcie_base + DPMAIF_AO_DL_HPC_CNTL);
}

static inline void dpmaif_hw_agg_cfg_set(struct dpmaif_hw_info *hw_info)
{
	unsigned int cfg;

	cfg = (DPMAIF_AGG_MAX_LEN_DF & 0xffff) << 0;
	cfg |= (DPMAIF_AGG_TBL_ENT_NUM_DF & 0xffff) << 16;

	iowrite32(cfg, hw_info->pcie_base + DPMAIF_AO_DL_DLQ_AGG_CFG);
}

static inline void dpmaif_hw_hash_bit_choose_set(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_DLQ_HASH_BIT_CHOOSE_DF,
		  hw_info->pcie_base + DPMAIF_AO_DL_DLQPIT_INIT_CON5);
}

static inline void dpmaif_hw_mid_pit_timeout_thres_set(struct dpmaif_hw_info *hw_info)
{
	iowrite32(DPMAIF_MID_TIMEOUT_THRES_DF,
		  hw_info->pcie_base + DPMAIF_AO_DL_DLQPIT_TIMEOUT0);
}

static void dpmaif_hw_dlq_timeout_thres_set(struct dpmaif_hw_info *hw_info)
{
	unsigned int tmp, idx;

	for (idx = 0; idx < DPMAIF_HPC_MAX_TOTAL_NUM; idx++) {
		tmp = ioread32(hw_info->pcie_base +
			       DPMAIF_AO_DL_DLQPIT_TIMEOUT1 + 4 * (idx / 2));

		if (idx % 2)
			tmp = (tmp & 0xFFFF) | (DPMAIF_DLQ_TIMEOUT_THRES_DF << 16);
		else
			tmp = (tmp & 0xFFFF0000) | DPMAIF_DLQ_TIMEOUT_THRES_DF;

		iowrite32(tmp, hw_info->pcie_base +
			  DPMAIF_AO_DL_DLQPIT_TIMEOUT1 + 4 * (idx / 2));
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

/***********************************************************************
 *	DPMAIF DL Part HW setting
 *
 ***********************************************************************/
static void dpmaif_dl_bat_init_done(struct dpmaif_hw_info *hw_info,
				    unsigned char q_num, bool frg_en)
{
	unsigned int dl_bat_init = 0;
	unsigned int count = 0;

	if (frg_en)
		dl_bat_init |= DPMAIF_DL_BAT_FRG_INIT;

	dl_bat_init |= DPMAIF_DL_BAT_INIT_ALLSET;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	while (1) {
		if ((ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT) &
			DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			iowrite32(dl_bat_init,
				  hw_info->pcie_base + DPMAIF_DL_BAT_INIT);
			break;
		}
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_bat_init_done check fail-1\n");
			break;
		}
	}

	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT) &
	       DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_bat_init_done check fail-2\n");
			break;
		}
	}
}

static void dpmaif_dl_set_bat_base_addr(struct dpmaif_hw_info *hw_info,
					unsigned char q_num, unsigned long addr)
{
	u32 lb_addr;
	u32 hb_addr;

#ifdef CONFIG_64BIT
	lb_addr = (u32)(addr & 0xFFFFFFFF);
	hb_addr = (u32)(addr >> 32);
#else
	lb_addr = (u32)addr;
	hb_addr = 0;
#endif
	iowrite32(lb_addr, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON0);
	iowrite32(hb_addr, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON3);
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
	value |= (cnt << 16) & DPMAIF_BAT_BID_MAXCNT_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
}

static inline void dpmaif_dl_set_ao_mtu(struct dpmaif_hw_info *hw_info,
					unsigned int mtu_sz)
{
	iowrite32(mtu_sz, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON1);
}

static void dpmaif_dl_set_ao_pit_chknum(struct dpmaif_hw_info *hw_info,
					unsigned char q_num,
					unsigned int number)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
	value &= ~DPMAIF_PIT_CHK_NUM_MSK;
	value |= (number << 24) & DPMAIF_PIT_CHK_NUM_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
}

static void dpmaif_dl_set_ao_remain_minsz(struct dpmaif_hw_info *hw_info,
					  unsigned char q_num,
					  unsigned int sz)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
	value &= ~DPMAIF_BAT_REMAIN_MINSZ_MSK;
	value |= ((sz / DPMAIF_BAT_REMAIN_SZ_BASE) << 8) &
		 DPMAIF_BAT_REMAIN_MINSZ_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON0);
}

static void dpmaif_dl_set_ao_bat_bufsz(struct dpmaif_hw_info *hw_info,
				       unsigned char q_num, unsigned int buf_sz)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
	value &= ~DPMAIF_BAT_BUF_SZ_MSK;
	value |= ((buf_sz / DPMAIF_BAT_BUFFER_SZ_BASE) << 8) &
		 DPMAIF_BAT_BUF_SZ_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
}

static void dpmaif_dl_set_ao_bat_rsv_length(struct dpmaif_hw_info *hw_info,
					    unsigned char q_num,
					    unsigned int length)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
	value &= ~DPMAIF_BAT_RSV_LEN_MSK;
	value |= length & DPMAIF_BAT_RSV_LEN_MSK;
	iowrite32(value, hw_info->pcie_base + DPMAIF_AO_DL_PKTINFO_CON2);
}

static void dpmaif_dl_set_pkt_alignment(struct dpmaif_hw_info *hw_info,
					unsigned char q_num,
					bool enable, unsigned int mode)
{
	unsigned int value;

	value = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~DPMAIF_PKT_ALIGN_MSK;
	if (enable) {
		value |= DPMAIF_PKT_ALIGN_EN;
		value |= (mode << 22) & DPMAIF_PKT_ALIGN_MSK;
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

static void dpmaif_dl_set_ao_frg_check_thres(struct dpmaif_hw_info *hw_info,
					     unsigned char q_num,
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
	value |= ((buf_sz / DPMAIF_FRG_BUFFER_SZ_BASE) << 8) &
		 DPMAIF_FRG_BUF_SZ_MSK;
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
	value |= (size << 16) & DPMAIF_BAT_CHECK_THRES_MSK;
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
					    unsigned char q_num, unsigned long addr)
{
	u32 lb_addr;
	u32 hb_addr;

#ifdef CONFIG_64BIT
	lb_addr = (u32)(addr & 0xFFFFFFFF);
	hb_addr = (u32)(addr >> 32);
#else
	lb_addr = (u32)addr;
	hb_addr = 0;
#endif
	iowrite32(lb_addr, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON0);
	iowrite32(hb_addr, hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT_CON4);
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

static void dpmaif_dl_dlq_pit_init_done(struct dpmaif_hw_info *hw_info,
					unsigned char q_num, unsigned int pit_idx)
{
	unsigned int dl_pit_init;
	int count = 0;

	dl_pit_init = DPMAIF_DL_PIT_INIT_ALLSET;
	dl_pit_init |= (pit_idx << DPMAIF_DLQPIT_CHAN_OFS);
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	while (1) {
		if ((ioread32(hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT) &
		      DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
			iowrite32(dl_pit_init,
				  hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT);
			break;
		}
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_dlq_pit_init_done check fail-1");
			break;
		}
	}

	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_DLQPIT_INIT) &
	       DPMAIF_DL_PIT_INIT_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_dlq_pit_init_done check fail-2");
			break;
		}
	}
}

static void dpmaif_config_dlq_pit_hw(struct dpmaif_hw_info *hw_info,
				     unsigned char q_num,
				     struct dpmaif_dl_st *dl_que)
{
	unsigned int pit_idx = q_num;

	dpmaif_dl_set_dlq_pit_base_addr(hw_info, q_num, dl_que->pit_base);
	dpmaif_dl_set_dlq_pit_size(hw_info, q_num, dl_que->pit_size_cnt);
	dpmaif_dl_dlq_pit_en(hw_info, q_num, true);
	dpmaif_dl_dlq_pit_init_done(hw_info, q_num, pit_idx);
}

static void dpmaif_config_all_dlq_hw(struct dpmaif_hw_info *hw_info)
{
	int i;

	for (i = 0; i < MAIFQ_MAX_DLQ_NUM; i++)
		dpmaif_config_dlq_pit_hw(hw_info, i, &hw_info->dl_que[i]);
}

static void dpmaif_dl_all_queue_en(struct dpmaif_hw_info *hw_info,
				   bool enable)
{
	unsigned int dl_bat_init;
	unsigned int value;
	unsigned int count = 0;

	value = ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);

	if (enable)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	iowrite32(value, hw_info->pcie_base + DPMAIF_DL_BAT_INIT_CON1);
	dl_bat_init = DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	/* update DL bat setting to HW */
	while (1) {
		if ((ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT) &
			DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			iowrite32(dl_bat_init,
				  hw_info->pcie_base + DPMAIF_DL_BAT_INIT);
			break;
		}
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("Could not set BAT queue - Max retry for HW to be ready\n");
			break;
		}
	}
	/* wait for HW update */
	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_INIT) &
	       DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("BAT_INIT_NOT_READY did not change to not ready state\n");
			break;
		}
	}
}

static void dpmaif_config_dlq_hw(struct dpmaif_hw_info *hw_info)
{
	struct dpmaif_dl_st *dl_que;
	struct dpmaif_dl_hwq *dl_hw;
	unsigned int queue = 0; /* all queues share one bat/frag bat table */

	dpmaif_dl_dlq_hpc_hw_init(hw_info);

	dl_que = &hw_info->dl_que[queue];
	dl_hw = &hw_info->dl_que_hw[queue];
	if (!dl_que->que_started)
		return;

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
	/* Enable Frg feature */
	dpmaif_dl_frg_ao_en(hw_info, queue, true);
	/* init frg use same BAT register */
	dpmaif_dl_set_bat_base_addr(hw_info, queue, dl_que->frg_base);
	dpmaif_dl_set_bat_size(hw_info, queue, dl_que->frg_size_cnt);
	dpmaif_dl_bat_en(hw_info, queue, true);
	dpmaif_dl_bat_init_done(hw_info, queue, true);
	/* normal bat init */
	dpmaif_dl_set_bat_base_addr(hw_info, queue, dl_que->bat_base);
	dpmaif_dl_set_bat_size(hw_info, queue, dl_que->bat_size_cnt);
	/* Enable BAT */
	dpmaif_dl_bat_en(hw_info, queue, false);
	/* notify HW init/setting done */
	dpmaif_dl_bat_init_done(hw_info, queue, false);
	/* init pit (two pit table) */
	dpmaif_config_all_dlq_hw(hw_info);
	dpmaif_dl_all_queue_en(hw_info, true);

	dpmaif_dl_set_pkt_checksum(hw_info);
}

static void dpmaif_ul_update_drb_size(struct dpmaif_hw_info *hw_info,
				      unsigned char q_num, unsigned int size)
{
	unsigned int old_size;

	old_size = ioread32(hw_info->pcie_base + DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));
	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= size & DPMAIF_DRB_SIZE_MSK;
	iowrite32(old_size, hw_info->pcie_base + DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));
}

static void dpmaif_ul_update_drb_base_addr(struct dpmaif_hw_info *hw_info,
					   unsigned char q_num, unsigned long addr)
{
	u32 lb_addr;
	u32 hb_addr;

#ifdef CONFIG_64BIT
	lb_addr = (u32)((u64)addr & 0xFFFFFFFF);
	hb_addr = (u32)((u64)addr >> 32);
#else
	lb_addr = (u32)addr;
	hb_addr = 0;
#endif

	iowrite32(lb_addr, hw_info->pcie_base + DPMAIF_ULQSAR_n(q_num));
	iowrite32(hb_addr, hw_info->pcie_base + DPMAIF_UL_DRB_ADDRH_n(q_num));
}

static void dpmaif_ul_rdy_en(struct dpmaif_hw_info *hw_info,
			     unsigned char q_num, bool ready)
{
	u32 ul_rdy_en;

	ul_rdy_en = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
	if (ready)
		ul_rdy_en |= BIT(q_num);
	else
		ul_rdy_en &= ~BIT(q_num);

	iowrite32(ul_rdy_en, hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
}

static void dpmaif_ul_arb_en(struct dpmaif_hw_info *hw_info,
			     unsigned char q_num, bool enable)
{
	u32 ul_arb_en;

	ul_arb_en = ioread32(hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);

	if (enable)
		ul_arb_en |= BIT(q_num + 8);
	else
		ul_arb_en &= ~BIT(q_num + 8);

	iowrite32(ul_arb_en, hw_info->pcie_base + DPMAIF_AO_UL_CHNL_ARB0);
}

static void dpmaif_config_ulq_hw(struct dpmaif_hw_info *hw_info)
{
	unsigned int que_cnt;
	struct dpmaif_ul_st *ul_que;

	for (que_cnt = 0; que_cnt < MAIFQ_MAX_ULQ_NUM; que_cnt++) {
		ul_que = &hw_info->ul_que[que_cnt];
		if (ul_que->que_started) {
			dpmaif_ul_update_drb_size(hw_info, que_cnt,
						  ul_que->drb_size_cnt *
						  DPMAIF_UL_DRB_ENTRY_WORD);
			dpmaif_ul_update_drb_base_addr(hw_info, que_cnt,
						       ul_que->drb_base);
			dpmaif_ul_rdy_en(hw_info, que_cnt, true);
			dpmaif_ul_arb_en(hw_info, que_cnt, true);
		} else {
			dpmaif_ul_arb_en(hw_info, que_cnt, false);
		}
	}
}

static void dpmaif_hw_init_done(struct dpmaif_hw_info *hw_info)
{
	unsigned int count = 0;
	u32 reg_value;

	/* sync default value to SRAM */
	reg_value = ioread32(hw_info->pcie_base + DPMAIF_AP_OVERWRITE_CFG);
	reg_value |= DPMAIF_SRAM_SYNC;
	iowrite32(reg_value, hw_info->pcie_base + DPMAIF_AP_OVERWRITE_CFG);

	/* polling status */
	while (ioread32(hw_info->pcie_base + DPMAIF_AP_OVERWRITE_CFG) &
	       DPMAIF_SRAM_SYNC) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dpmaif hw_init_done check fail\n");
			break;
		}
	}

	/* UL cfg done */
	iowrite32(DPMAIF_UL_INIT_DONE, hw_info->pcie_base + DPMAIF_AO_UL_INIT_SET);
	/* DL cfg done */
	iowrite32(DPMAIF_DL_INIT_DONE, hw_info->pcie_base + DPMAIF_AO_DL_INIT_SET);
}

static bool dpmaif_config_que_hw(struct dpmaif_hw_info *hw_info)
{
	dpmaif_common_hw_init(hw_info);
	dpmaif_config_dlq_hw(hw_info);
	dpmaif_config_ulq_hw(hw_info);
	dpmaif_hw_init_done(hw_info);
	return true;
}

static inline int dpmaif_dl_idle_check(struct dpmaif_hw_info *hw_info)
{
	u32 val = ioread32(hw_info->pcie_base + DPMAIF_DL_DBG_STA1);

	/* if total queue is idle, all DL is idle
	 * 0: idle, 1: busy
	 */
	return !(val & DPMAIF_DL_IDLE_STS);
}

static void dpmaif_ul_all_queue_en(struct dpmaif_hw_info *hw_info,
				   bool enable)
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
	u32 val = ioread32(hw_info->pcie_base + DPMAIF_UL_DBG_STA2);

	/* if total queue is idle, all UL is idle
	 * 0: idle, 1: busy
	 */
	return !(val & DPMAIF_UL_IDLE_STS);
}

/***********************************************************************
 *	DPMAIF UL Part HW setting
 *
 ***********************************************************************/
int dpmaif_ul_add_wcnt(struct dpmaif_hw_info *hw_info,
		       unsigned char q_num, unsigned int drb_entry_cnt)
{
	unsigned int ul_update;
	unsigned int count = 0;

	ul_update = (drb_entry_cnt & 0x0000ffff);
	ul_update |= DPMAIF_UL_ADD_UPDATE;

	while (ioread32(hw_info->pcie_base + DPMAIF_ULQ_ADD_DESC_CH_n(q_num)) &
	       DPMAIF_UL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("UL add not ready. wcnt: %u queue: %d reg: 0x%x\n",
			       drb_entry_cnt, q_num,
			       ioread32(hw_info->pcie_base +
					DPMAIF_ULQ_ADD_DESC_CH_n(q_num)));
			return -EBUSY;
		}
	}
	iowrite32(ul_update, hw_info->pcie_base + DPMAIF_ULQ_ADD_DESC_CH_n(q_num));

	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_ULQ_ADD_DESC_CH_n(q_num)) &
	       DPMAIF_UL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("UL add timeout. wcnt: %u queue: %d reg: 0x%x\n",
			       drb_entry_cnt, q_num,
			       ioread32(hw_info->pcie_base +
					DPMAIF_ULQ_ADD_DESC_CH_n(q_num)));
			return -EBUSY;
		}
	}

	return 0;
}

unsigned int dpmaif_ul_get_ridx(struct dpmaif_hw_info *hw_info,
				unsigned char q_num)
{
	unsigned int ridx = ioread32(hw_info->pcie_base + DPMAIF_ULQ_STA0_n(q_num));

	return ridx >> DPMAIF_UL_DRB_RIDX_OFFSET;
}

int dpmaif_dl_add_dlq_pit_remain_cnt(struct dpmaif_hw_info *hw_info,
				     unsigned int dlq_pit_idx,
				     unsigned int pit_remain_cnt)
{
	int count = 0;
	u32 dl_update;

	dl_update = pit_remain_cnt & DPMAIF_PIT_REM_CNT_MSK;
	dl_update |= DPMAIF_DL_ADD_UPDATE |
		(dlq_pit_idx << DPMAIF_ADD_DLQ_PIT_CHAN_OFS);

	while (ioread32(hw_info->pcie_base + DPMAIF_DL_DLQPIT_ADD) &
	       DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_add_dlq_pit_remain_cnt check failed-1\n");
			return -EBUSY;
		}
	}
	iowrite32(dl_update, hw_info->pcie_base + DPMAIF_DL_DLQPIT_ADD);

	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_DLQPIT_ADD) &
	       DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_add_dlq_pit_remain_cnt check failed-2\n");
			return -EBUSY;
		}
	}

	return 0;
}

unsigned int dpmaif_dl_dlq_pit_get_wridx(struct dpmaif_hw_info *hw_info,
					 unsigned int dlq_pit_idx)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_DLQ_STA5 +
			dlq_pit_idx * DLQ_PIT_IDX_SIZE) & DPMAIF_DL_PIT_WRIDX_MSK;
}

int dpmaif_dl_add_bat_cnt(struct dpmaif_hw_info *hw_info,
			  unsigned char q_num, unsigned int bat_entry_cnt)
{
	unsigned int dl_bat_update;
	unsigned int count = 0;

	dl_bat_update = (bat_entry_cnt & 0xffff);
	dl_bat_update |= DPMAIF_DL_ADD_UPDATE;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_ADD) &
	       DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("DL BAT add not ready. count: %u queue: %dn",
			       bat_entry_cnt, q_num);
			return -EBUSY;
		}
	}
	iowrite32(dl_bat_update, hw_info->pcie_base + DPMAIF_DL_BAT_ADD);

	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_ADD) &
		DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("DL BAT add timeout. count: %u queue: %dn",
			       bat_entry_cnt, q_num);
			return -EBUSY;
		}
	}

	return 0;
}

unsigned int dpmaif_dl_get_bat_ridx(struct dpmaif_hw_info *hw_info,
				    unsigned char q_num)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_BAT_STA2) &
			DPMAIF_DL_BAT_WRIDX_MSK;
}

unsigned int dpmaif_dl_get_bat_wridx(struct dpmaif_hw_info *hw_info,
				     unsigned char q_num)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_BAT_STA3) &
			DPMAIF_DL_BAT_WRIDX_MSK;
}

void dpmaif_dl_add_frg_cnt(struct dpmaif_hw_info *hw_info,
			   unsigned char q_num, unsigned int frg_entry_cnt)
{
	unsigned int dl_frg_update;
	unsigned int count = 0;

	dl_frg_update = (frg_entry_cnt & 0xffff);
	dl_frg_update |= DPMAIF_DL_FRG_ADD_UPDATE;
	dl_frg_update |= DPMAIF_DL_ADD_UPDATE;

	while (ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_ADD) &
	       DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_add_frg_cnt check fail-1\n");
			break;
		}
	}
	iowrite32(dl_frg_update, hw_info->pcie_base + DPMAIF_DL_BAT_ADD);

	count = 0;
	while (ioread32(hw_info->pcie_base + DPMAIF_DL_BAT_ADD) &
	       DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("dl_add_frg_cnt check fail-2");
			break;
		}
	}
}

unsigned int dpmaif_dl_get_frg_ridx(struct dpmaif_hw_info *hw_info,
				    unsigned char q_num)
{
	return ioread32(hw_info->pcie_base + DPMAIF_AO_DL_FRGBAT_STA2) &
			DPMAIF_DL_FRG_WRIDX_MSK;
}

static void dpmaif_set_queue_property(struct dpmaif_hw_info *hw_info,
				      struct dpmaif_hw_init_para *init_para)
{
	struct dpmaif_dl_hwq *dl_hwq;
	struct dpmaif_dl_st *dl_que;
	struct dpmaif_ul_st *ul_que;
	unsigned int que_cnt;

	for (que_cnt = 0; que_cnt < MAIFQ_MAX_DLQ_NUM; que_cnt++) {
		dl_hwq = &hw_info->dl_que_hw[que_cnt];
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

		dl_que = &hw_info->dl_que[que_cnt];
		dl_que->bat_base = init_para->pkt_bat_base_addr[que_cnt];
		dl_que->bat_size_cnt = init_para->pkt_bat_size_cnt[que_cnt];
		dl_que->pit_base = init_para->pit_base_addr[que_cnt];
		dl_que->pit_size_cnt = init_para->pit_size_cnt[que_cnt];
		dl_que->frg_base = init_para->frg_bat_base_addr[que_cnt];
		dl_que->frg_size_cnt = init_para->frg_bat_size_cnt[que_cnt];
		dl_que->que_started = true;
	}

	for (que_cnt = 0; que_cnt < MAIFQ_MAX_ULQ_NUM; que_cnt++) {
		ul_que = &hw_info->ul_que[que_cnt];
		ul_que->drb_base = init_para->drb_base_addr[que_cnt];
		ul_que->drb_size_cnt = init_para->drb_size_cnt[que_cnt];
		ul_que->que_started = true;
	}
}

void dpmaif_hw_stop_tx_queue(struct dpmaif_hw_info *hw_info)
{
	int count = 0;

	/* Disable HW arb and check idle */
	dpmaif_ul_all_queue_en(hw_info, false);
	while (dpmaif_ul_idle_check(hw_info)) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("Stop TX failed, 0x%x\n",
			       ioread32(hw_info->pcie_base + DPMAIF_UL_DBG_STA2));
			break;
		}
	}
}

void dpmaif_hw_stop_rx_queue(struct dpmaif_hw_info *hw_info)
{
	unsigned int wridx;
	unsigned int ridx;
	int count = 0;

	/* Disable HW arb and check idle */
	dpmaif_dl_all_queue_en(hw_info, false);
	while (dpmaif_dl_idle_check(hw_info)) {
		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("Stop RX failed, 0x%x\n",
			       ioread32(hw_info->pcie_base + DPMAIF_DL_DBG_STA1));
			break;
		}
	}

	/* check middle pit sync done */
	count = 0;
	do {
		wridx = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PIT_STA3) &
			DPMAIF_DL_PIT_WRIDX_MSK;
		ridx = ioread32(hw_info->pcie_base + DPMAIF_AO_DL_PIT_STA2) &
			DPMAIF_DL_PIT_WRIDX_MSK;
		if (wridx == ridx)
			break;

		if (++count >= DPMA_CHECK_REG_TIMEOUT) {
			pr_err("check middle pit sync fail\n");
			break;
		}
	} while (1);
}

void dpmaif_start_hw(struct dpmaif_hw_info *hw_info)
{
	dpmaif_ul_all_queue_en(hw_info, true);
	dpmaif_dl_all_queue_en(hw_info, true);
}

void dpmaif_hw_init(struct dpmaif_hw_info *hw_info, void *para)
{
	struct dpmaif_hw_init_para *init_para;

	if (!para) {
		pr_err("Invalid inputs: NULL pointer!\n");
		goto exit;
	}

	init_para = (struct dpmaif_hw_init_para *)para;

	/* port mode & clock config */
	dpmaif_hw_config(hw_info);

	/* HW interrupt init */
	dpmaif_init_intr(hw_info);

	/* HW queue config */
	dpmaif_set_queue_property(hw_info, init_para);
	dpmaif_config_que_hw(hw_info);

exit:
	return;
}

bool dpmaif_hw_check_clr_ul_done_status(struct dpmaif_hw_info *hw_info, unsigned char qno)
{
	u32 intr = ioread32(hw_info->pcie_base + DPMAIF_AP_L2TISAR0);

	intr &= BIT(DP_UL_INT_DONE_OFFSET + qno);
	if (intr) {
		/* clear interrupt status */
		iowrite32(intr, hw_info->pcie_base + DPMAIF_AP_L2TISAR0);
		return true;
	}

	return false;
}
