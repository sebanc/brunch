/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_HIF_DPMA_RX_H__
#define __T7XX_HIF_DPMA_RX_H__

#include <linux/bitfield.h>
#include <linux/types.h>

#include "t7xx_hif_dpmaif.h"

/* lhif header feilds */
#define LHIF_HEADER_NW_TYPE	GENMASK(31, 29)
#define LHIF_HEADER_NETIF	GENMASK(28, 24)
#define LHIF_HEADER_F		GENMASK(22, 20)
#define LHIF_HEADER_FLOW	GENMASK(19, 16)

/* Structure of DL PIT */
struct dpmaif_normal_pit {
	unsigned int	pit_header;
	unsigned int	p_data_addr;
	unsigned int	data_addr_ext;
	unsigned int	pit_footer;
};

/* pit_header fields */
#define NORMAL_PIT_DATA_LEN	GENMASK(31, 16)
#define NORMAL_PIT_BUFFER_ID	GENMASK(15, 3)
#define NORMAL_PIT_BUFFER_TYPE	BIT(2)
#define NORMAL_PIT_CONT		BIT(1)
#define NORMAL_PIT_PACKET_TYPE	BIT(0)
/* pit_footer fields */
#define NORMAL_PIT_DLQ_DONE	GENMASK(31, 30)
#define NORMAL_PIT_ULQ_DONE	GENMASK(29, 24)
#define NORMAL_PIT_HEADER_OFFSET GENMASK(23, 19)
#define NORMAL_PIT_BI_F		GENMASK(18, 17)
#define NORMAL_PIT_IG		BIT(16)
#define NORMAL_PIT_RES		GENMASK(15, 11)
#define NORMAL_PIT_H_BID	GENMASK(10, 8)
#define NORMAL_PIT_PIT_SEQ	GENMASK(7, 0)

struct dpmaif_msg_pit {
	unsigned int	dword1;
	unsigned int	dword2;
	unsigned int	dword3;
	unsigned int	dword4;
};

/* double word 1 */
#define MSG_PIT_DP		BIT(31)
#define MSG_PIT_RES		GENMASK(30, 27)
#define MSG_PIT_NETWORK_TYPE	GENMASK(26, 24)
#define MSG_PIT_CHANNEL_ID	GENMASK(23, 16)
#define MSG_PIT_RES2		GENMASK(15, 12)
#define MSG_PIT_HPC_IDX		GENMASK(11, 8)
#define MSG_PIT_SRC_QID		GENMASK(7, 5)
#define MSG_PIT_ERROR_BIT	BIT(4)
#define MSG_PIT_CHECKSUM	GENMASK(3, 2)
#define MSG_PIT_CONT		BIT(1)
#define MSG_PIT_PACKET_TYPE	BIT(0)

/* double word 2 */
#define MSG_PIT_HP_IDX		GENMASK(31, 27)
#define MSG_PIT_CMD		GENMASK(26, 24)
#define MSG_PIT_RES3		GENMASK(23, 21)
#define MSG_PIT_FLOW		GENMASK(20, 16)
#define MSG_PIT_COUNT		GENMASK(15, 0)

/* double word 3 */
#define MSG_PIT_HASH		GENMASK(31, 24)
#define MSG_PIT_RES4		GENMASK(23, 18)
#define MSG_PIT_PRO		GENMASK(17, 16)
#define MSG_PIT_VBID		GENMASK(15, 3)
#define MSG_PIT_RES5		GENMASK(2, 0)

/* dwourd 4 */
#define MSG_PIT_DLQ_DONE	GENMASK(31, 30)
#define MSG_PIT_ULQ_DONE	GENMASK(29, 24)
#define MSG_PIT_IP		BIT(23)
#define MSG_PIT_RES6		BIT(22)
#define MSG_PIT_MR		GENMASK(21, 20)
#define MSG_PIT_RES7		GENMASK(19, 17)
#define MSG_PIT_IG		BIT(16)
#define MSG_PIT_RES8		GENMASK(15, 11)
#define MSG_PIT_H_BID		GENMASK(10, 8)
#define MSG_PIT_PIT_SEQ		GENMASK(7, 0)

int dpmaif_rxq_alloc(struct dpmaif_rx_queue *queue);
void dpmaif_stop_rx_sw(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_bat_release_work_alloc(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_rx_buf_alloc(struct dpmaif_ctrl *dpmaif_ctrl,
			const struct dpmaif_bat_request *bat_req, const unsigned char q_num,
			const unsigned int buf_cnt, const bool first_time);
void dpmaif_skb_queue_free(struct dpmaif_ctrl *dpmaif_ctrl, const unsigned int index);
int dpmaif_skb_pool_init(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_rx_frag_alloc(struct dpmaif_ctrl *dpmaif_ctrl, struct dpmaif_bat_request *bat_req,
			 unsigned char q_num, const unsigned int buf_cnt, const bool first_time);
void dpmaif_suspend_rx_sw_stop(struct dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_irq_rx_done(struct dpmaif_ctrl *dpmaif_ctrl, const unsigned int que_mask);
void dpmaif_rxq_free(struct dpmaif_rx_queue *queue);
void dpmaif_bat_release_work_free(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_bat_alloc(const struct dpmaif_ctrl *dpmaif_ctrl, struct dpmaif_bat_request *bat_req,
		     const enum bat_type buf_type);
void dpmaif_bat_free(const struct dpmaif_ctrl *dpmaif_ctrl,
		     struct dpmaif_bat_request *bat_req);

#endif /* __T7XX_HIF_DPMA_RX_H__ */
