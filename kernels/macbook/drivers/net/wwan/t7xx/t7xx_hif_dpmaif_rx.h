/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __MODEM_DPMA_RX_H__
#define __MODEM_DPMA_RX_H__

#include "t7xx_hif_dpmaif.h"

struct lhif_header {
	u16 pdcp_count;
	u8 flow:4;
	u8 f:3;
	u8 reserved:1;
	u8 netif:5;
	u8 nw_type:3;
} __packed;

int dpmaif_rxq_init(struct dpmaif_rx_queue *queue);
void dpmaif_stop_rx_sw(struct md_dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_bat_rel_work_init(struct md_dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_alloc_rx_buf(struct md_dpmaif_ctrl *dpmaif_ctrl,
			struct dpmaif_bat_request *bat_req,
			unsigned char q_num, unsigned int buf_cnt,
int blocking, bool first_time);
void dpmaif_skb_queue_rel(struct md_dpmaif_ctrl *dpmaif_ctrl,
			  unsigned int index);
int dpmaif_skb_pool_init(struct md_dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_alloc_rx_frag(struct md_dpmaif_ctrl *dpmaif_ctrl,
			 struct dpmaif_bat_request *bat_req,
			 unsigned char q_num, unsigned int buf_cnt,
			 int blocking, bool first_time);
void dpmaif_suspend_rx_sw_stop(struct md_dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_irq_rx_done(struct md_dpmaif_ctrl *dpmaif_ctrl,
			unsigned int que_mask);
void dpmaif_rxq_rel(struct dpmaif_rx_queue *queue);
void dpmaif_bat_rel_work_rel(struct md_dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_bat_init(struct md_dpmaif_ctrl *dpmaif_ctrl,
		    struct dpmaif_bat_request *bat_req,
		    enum bat_type buf_type);
int dpmaif_napi_rx_poll(struct napi_struct *napi, int budget);
struct napi_struct *dpmaif_get_napi(struct md_dpmaif_ctrl *dpmaif_ctrl, int qno);

#endif /* __MODEM_DPMA_RX_H__ */
