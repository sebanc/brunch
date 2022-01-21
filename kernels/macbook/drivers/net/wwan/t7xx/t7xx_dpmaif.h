/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_DPMAIF_H__
#define __T7XX_DPMAIF_H__

#include <linux/bits.h>
#include <linux/types.h>

#include "t7xx_hif_dpmaif.h"

#define DPMAIF_DL_PIT_SEQ_VALUE		251
#define DPMAIF_UL_DRB_BYTE_SIZE		16
#define DPMAIF_UL_DRB_ENTRY_WORD	(DPMAIF_UL_DRB_BYTE_SIZE >> 2)

#define DPMAIF_MAX_CHECK_COUNT		1000000
#define DPMAIF_CHECK_TIMEOUT_US		10000
#define DPMAIF_CHECK_INIT_TIMEOUT_US	100000
#define DPMAIF_CHECK_DELAY_US		10

/* DPMAIF HW Initialization parameter structure */
struct dpmaif_hw_params {
	/* UL part */
	dma_addr_t	drb_base_addr[DPMAIF_TXQ_NUM];
	unsigned int	drb_size_cnt[DPMAIF_TXQ_NUM];

	/* DL part */
	dma_addr_t	pkt_bat_base_addr[DPMAIF_RXQ_NUM];
	unsigned int	pkt_bat_size_cnt[DPMAIF_RXQ_NUM];
	dma_addr_t	frg_bat_base_addr[DPMAIF_RXQ_NUM];
	unsigned int	frg_bat_size_cnt[DPMAIF_RXQ_NUM];
	dma_addr_t	pit_base_addr[DPMAIF_RXQ_NUM];
	unsigned int	pit_size_cnt[DPMAIF_RXQ_NUM];
};

enum dpmaif_hw_intr_type {
	DPF_INTR_INVALID_MIN = 0,

	DPF_INTR_UL_DONE,
	DPF_INTR_UL_DRB_EMPTY,
	DPF_INTR_UL_MD_NOTREADY,
	DPF_INTR_UL_MD_PWR_NOTREADY,
	DPF_INTR_UL_LEN_ERR,

	DPF_INTR_DL_DONE,
	DPF_INTR_DL_SKB_LEN_ERR,
	DPF_INTR_DL_BATCNT_LEN_ERR,
	DPF_INTR_DL_PITCNT_LEN_ERR,
	DPF_INTR_DL_PKT_EMPTY_SET,
	DPF_INTR_DL_FRG_EMPTY_SET,
	DPF_INTR_DL_MTU_ERR,
	DPF_INTR_DL_FRGCNT_LEN_ERR,

	DPF_DL_INT_DLQ0_PITCNT_LEN_ERR,
	DPF_DL_INT_DLQ1_PITCNT_LEN_ERR,
	DPF_DL_INT_HPC_ENT_TYPE_ERR,
	DPF_INTR_DL_DLQ0_DONE,
	DPF_INTR_DL_DLQ1_DONE,

	DPF_INTR_INVALID_MAX
};

#define DPF_RX_QNO0			0
#define DPF_RX_QNO1			1
#define DPF_RX_QNO_DFT			DPF_RX_QNO0

struct dpmaif_hw_intr_st_para {
	unsigned int intr_cnt;
	enum dpmaif_hw_intr_type intr_types[DPF_INTR_INVALID_MAX - 1];
	unsigned int intr_queues[DPF_INTR_INVALID_MAX - 1];
};

#define DPMAIF_HW_BAT_REMAIN		64
#define DPMAIF_HW_BAT_PKTBUF		(128 * 28)
#define DPMAIF_HW_FRG_PKTBUF		128
#define DPMAIF_HW_BAT_RSVLEN		64
#define DPMAIF_HW_PKT_BIDCNT		1
#define DPMAIF_HW_PKT_ALIGN		64
#define DPMAIF_HW_MTU_SIZE		(3 * 1024 + 8)

#define DPMAIF_HW_CHK_BAT_NUM		62
#define DPMAIF_HW_CHK_FRG_NUM		3
#define DPMAIF_HW_CHK_PIT_NUM		(2 * DPMAIF_HW_CHK_BAT_NUM)

/* tx interrupt mask */
#define DP_UL_INT_DONE_OFFSET		0
#define DP_UL_INT_EMPTY_OFFSET		5
#define DP_UL_INT_MD_NOTRDY_OFFSET	10
#define DP_UL_INT_PWR_NOTRDY_OFFSET	15
#define DP_UL_INT_LEN_ERR_OFFSET	20
#define DP_UL_QNUM_MSK			0x1F

#define DP_UL_INT_DONE(q_num)		BIT((q_num) + DP_UL_INT_DONE_OFFSET)
#define DP_UL_INT_EMPTY(q_num)		BIT((q_num) + DP_UL_INT_EMPTY_OFFSET)
#define DP_UL_INT_MD_NOTRDY(q_num)	BIT((q_num) + DP_UL_INT_MD_NOTRDY_OFFSET)
#define DP_UL_INT_PWR_NOTRDY(q_num)	BIT((q_num) + DP_UL_INT_PWR_NOTRDY_OFFSET)
#define DP_UL_INT_LEN_ERR(q_num)	BIT((q_num) + DP_UL_INT_LEN_ERR_OFFSET)

#define DP_UL_INT_QDONE_MSK		(DP_UL_QNUM_MSK << DP_UL_INT_DONE_OFFSET)
#define DP_UL_INT_EMPTY_MSK		(DP_UL_QNUM_MSK << DP_UL_INT_EMPTY_OFFSET)
#define DP_UL_INT_MD_NOTREADY_MSK	(DP_UL_QNUM_MSK << DP_UL_INT_MD_NOTRDY_OFFSET)
#define DP_UL_INT_MD_PWR_NOTREADY_MSK	(DP_UL_QNUM_MSK << DP_UL_INT_PWR_NOTRDY_OFFSET)
#define DP_UL_INT_ERR_MSK		(DP_UL_QNUM_MSK << DP_UL_INT_LEN_ERR_OFFSET)

/* rx interrupt mask */
#define DP_DL_INT_QDONE_MSK		BIT(0)
#define DP_DL_INT_SKB_LEN_ERR		BIT(1)
#define DP_DL_INT_BATCNT_LEN_ERR	BIT(2)
#define DP_DL_INT_PITCNT_LEN_ERR	BIT(3)
#define DP_DL_INT_PKT_EMPTY_MSK		BIT(4)
#define DP_DL_INT_FRG_EMPTY_MSK		BIT(5)
#define DP_DL_INT_MTU_ERR_MSK		BIT(6)
#define DP_DL_INT_FRG_LENERR_MSK	BIT(7)
#define DP_DL_INT_DLQ0_PITCNT_LEN_ERR	BIT(8)
#define DP_DL_INT_DLQ1_PITCNT_LEN_ERR	BIT(9)
#define DP_DL_INT_HPC_ENT_TYPE_ERR	BIT(10)
#define DP_DL_INT_DLQ0_QDONE_SET	BIT(13)
#define DP_DL_INT_DLQ1_QDONE_SET	BIT(14)

#define DP_DL_DLQ0_STATUS_MASK \
	(DP_DL_INT_DLQ0_PITCNT_LEN_ERR | DP_DL_INT_DLQ0_QDONE_SET)

#define DP_DL_DLQ1_STATUS_MASK \
	(DP_DL_INT_DLQ1_PITCNT_LEN_ERR | DP_DL_INT_DLQ1_QDONE_SET)

/* APIs exposed to HIF Layer */
int dpmaif_hw_init(struct dpmaif_ctrl *dpmaif_ctrl, struct dpmaif_hw_params *init_param);
int dpmaif_hw_stop_tx_queue(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_hw_stop_rx_queue(struct dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_start_hw(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_hw_get_interrupt_status(struct dpmaif_ctrl *dpmaif_ctrl,
				   struct dpmaif_hw_intr_st_para *para, int qno);
void dpmaif_unmask_ulq_interrupt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int q_num);
int dpmaif_ul_add_wcnt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num,
		       unsigned int drb_entry_cnt);
int dpmaif_dl_add_bat_cnt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num,
			  unsigned int bat_entry_cnt);
int dpmaif_dl_add_frg_cnt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned char q_num,
			  unsigned int frg_entry_cnt);
int dpmaif_dl_add_dlq_pit_remain_cnt(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int dlq_pit_idx,
				     unsigned int pit_remain_cnt);
void dpmaif_dlq_unmask_rx_pitcnt_len_err_intr(struct dpmaif_hw_info *hw_info, unsigned char qno);
void dpmaif_hw_dlq_unmask_rx_done(struct dpmaif_hw_info *hw_info, unsigned char qno);
bool dpmaif_hw_check_clr_ul_done_status(struct dpmaif_hw_info *hw_info, unsigned char qno);
unsigned int dpmaif_ul_get_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num);
void dpmaif_clr_ul_all_interrupt(struct dpmaif_hw_info *hw_info);
void dpmaif_clr_dl_all_interrupt(struct dpmaif_hw_info *hw_info);
void dpmaif_clr_ip_busy_sts(struct dpmaif_hw_info *hw_info);
void dpmaif_unmask_dl_batcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info);
void dpmaif_unmask_dl_pitcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info);
unsigned int dpmaif_dl_get_bat_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num);
unsigned int dpmaif_dl_get_bat_wridx(struct dpmaif_hw_info *hw_info, unsigned char q_num);
unsigned int dpmaif_dl_get_frg_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num);
unsigned int dpmaif_dl_dlq_pit_get_wridx(struct dpmaif_hw_info *hw_info,
					 unsigned int dlq_pit_idx);

#endif /* __T7XX_DPMAIF_H__ */
