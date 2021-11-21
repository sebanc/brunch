/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __T7XX_DPMAIF_H__
#define __T7XX_DPMAIF_H__

#include "t7xx_hif_dpmaif.h"

/* DPMAIF HW Initialization parameter structure */
struct dpmaif_hw_init_para {
	/* UL part */
	dma_addr_t drb_base_addr[MAIFQ_MAX_ULQ_NUM];
	unsigned int drb_size_cnt[MAIFQ_MAX_ULQ_NUM];

	/* DL part */
	dma_addr_t pkt_bat_base_addr[MAIFQ_MAX_DLQ_NUM];
	unsigned int pkt_bat_size_cnt[MAIFQ_MAX_DLQ_NUM];
	dma_addr_t frg_bat_base_addr[MAIFQ_MAX_DLQ_NUM];
	unsigned int frg_bat_size_cnt[MAIFQ_MAX_DLQ_NUM];
	dma_addr_t pit_base_addr[MAIFQ_MAX_DLQ_NUM];
	unsigned int pit_size_cnt[MAIFQ_MAX_DLQ_NUM];
};

enum dpmaif_hw_intr_type {
	DPF_INTR_INVALID_MIN = 0,

	/* uplink part */
	DPF_INTR_UL_DONE,
	DPF_INTR_UL_DRB_EMPTY,
	DPF_INTR_UL_MD_NOTREADY,
	DPF_INTR_UL_MD_PWR_NOTREADY,
	DPF_INTR_UL_LEN_ERR,

	/* downlink part */
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

enum dpmaif_hw_notify_type {
	DPF_NOTIFY_INVALID_MIN = 0,

	/* uplink part */
	DPF_NOTIFY_UL_ENABLE_INTR,

	/* downlink part */
	DPF_NOTIFY_DL_ENABLE_INTR,
	DPF_NOTIFY_DL_ADD_PKT_BAT_CNT,
	DPF_NOTIFY_DL_ADD_FRG_BAT_CNT,
	DPF_NOTIFY_DL_ADD_PIT_RMN_CNT,

	/* others */
	DPF_NOTIFY_CLEAR_IP_BUSY,

	DPF_NOTIFY_INVALID_MAX = DPF_NOTIFY_CLEAR_IP_BUSY
};

#define DPF_RX_QNO0			DPMAIF_DLQ0_NUM
#define DPF_RX_QNO1			DPMAIF_DLQ1_NUM
#define DPF_RX_QNO_DFT			DPF_RX_QNO0

struct dpmaif_hw_intr_st_para {
	unsigned int intr_cnt;
	enum dpmaif_hw_intr_type intr_types[DPF_INTR_INVALID_MAX - 1];
	unsigned int intr_queues[DPF_INTR_INVALID_MAX - 1];
};

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

/* RX interrupt mask */
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

/******************************************************************************
 *		APIs exposed to HIF Layer
 ******************************************************************************/
void dpmaif_hw_init(struct dpmaif_hw_info *hw_info, void *para);
void dpmaif_hw_stop_tx_queue(struct dpmaif_hw_info *hw_info);
void dpmaif_hw_stop_rx_queue(struct dpmaif_hw_info *hw_info);
void dpmaif_start_hw(struct dpmaif_hw_info *hw_info);
void dpmaif_dlq_unmask_rx_pit_cnt_len_err_intr(struct dpmaif_hw_info *hw_info,
					       unsigned char qno);
void dpmaif_hw_dl_dlq_unmask_rx_done_intr(struct dpmaif_hw_info *hw_info,
					  unsigned char qno);
int dpmaif_hw_get_interrupt_status(struct dpmaif_hw_info *hw_info,
				   void *para, int qno);
void dpmaif_unmask_ul_que_interrupt(struct dpmaif_hw_info *hw_info,
				    unsigned int q_num);
bool dpmaif_hw_check_clr_ul_done_status(struct dpmaif_hw_info *hw_info,
					unsigned char qno);
unsigned int dpmaif_ul_get_ridx(struct dpmaif_hw_info *hw_info, unsigned char q_num);
int dpmaif_ul_add_wcnt(struct dpmaif_hw_info *hw_info,
		       unsigned char q_num, unsigned int drb_entry_cnt);
void dpmaif_clr_ul_all_interrupt(struct dpmaif_hw_info *hw_info);
void dpmaif_clr_dl_all_interrupt(struct dpmaif_hw_info *hw_info);
void dpmaif_clr_ip_busy_sts(struct dpmaif_hw_info *hw_info);
void dpmaif_unmask_dl_batcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info);
void dpmaif_unmask_dl_pitcnt_len_err_interrupt(struct dpmaif_hw_info *hw_info);
int dpmaif_dl_add_bat_cnt(struct dpmaif_hw_info *hw_info,
			  unsigned char q_num,
			  unsigned int bat_entry_cnt);
unsigned int dpmaif_dl_get_bat_ridx(struct dpmaif_hw_info *hw_info,
				    unsigned char q_num);
unsigned int dpmaif_dl_get_bat_wridx(struct dpmaif_hw_info *hw_info,
				     unsigned char q_num);
void dpmaif_dl_add_frg_cnt(struct dpmaif_hw_info *hw_info,
			   unsigned char q_num,
			   unsigned int frg_entry_cnt);
unsigned int dpmaif_dl_get_frg_ridx(struct dpmaif_hw_info *hw_info,
				    unsigned char q_num);
int dpmaif_dl_add_dlq_pit_remain_cnt(struct dpmaif_hw_info *hw_info,
				     unsigned int dlq_pit_idx,
				     unsigned int pit_remain_cnt);
unsigned int dpmaif_dl_dlq_pit_get_wridx(struct dpmaif_hw_info *hw_info,
					 unsigned int dlq_pit_idx);

#endif /* __T7XX_DPMAIF_H__ */
