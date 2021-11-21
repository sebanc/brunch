/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __MODEM_DPMA_TX_H__
#define __MODEM_DPMA_TX_H__

#include "t7xx_hif_dpmaif.h"

int dpmaif_tx_send_skb(struct md_dpmaif_ctrl *dpmaif_ctrl,
		       int qno, struct sk_buff *skb, int blocking);
void dpmaif_tx_thread_uninit(struct md_dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_tx_thread_init(struct md_dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_txq_rel(struct dpmaif_tx_queue *txq);
void dpmaif_irq_tx_done(struct md_dpmaif_ctrl *dpmaif_ctrl,
			unsigned int que_mask);
int dpmaif_txq_init(struct dpmaif_tx_queue *txq);
void dpmaif_suspend_tx_sw_stop(struct md_dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_stop_tx_sw(struct md_dpmaif_ctrl *dpmaif_ctrl);

#endif /* __MODEM_DPMA_TX_H__ */
