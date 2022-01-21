/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_HIF_DPMA_TX_H__
#define __T7XX_HIF_DPMA_TX_H__

#include <linux/bitfield.h>
#include <linux/skbuff.h>

#include "t7xx_common.h"
#include "t7xx_hif_dpmaif.h"

/* UL DRB */
struct dpmaif_drb_pd {
	unsigned int		header;
	unsigned int		p_data_addr;
	unsigned int		data_addr_ext;
	unsigned int		reserved2;
};

/* header's fields */
#define DRB_PD_DATA_LEN		GENMASK(31, 16)
#define DRB_PD_RES		GENMASK(15, 3)
#define DRB_PD_CONT		BIT(2)
#define DRB_PD_DTYP		GENMASK(1, 0)

struct dpmaif_drb_msg {
	unsigned int		header_dw1;
	unsigned int		header_dw2;
	unsigned int		reserved4;
	unsigned int		reserved5;
};

/* first double word header fields */
#define DRB_MSG_PACKET_LEN	GENMASK(31, 16)
#define DRB_MSG_DW1_RES		GENMASK(15, 3)
#define DRB_MSG_CONT		BIT(2)
#define DRB_MSG_DTYP		GENMASK(1, 0)

/* second double word header fields */
#define DRB_MSG_DW2_RES		GENMASK(31, 30)
#define DRB_MSG_L4_CHK		BIT(29)
#define DRB_MSG_IP_CHK		BIT(28)
#define DRB_MSG_RES2		BIT(27)
#define DRB_MSG_NETWORK_TYPE	GENMASK(26, 24)
#define DRB_MSG_CHANNEL_ID	GENMASK(23, 16)
#define DRB_MSG_COUNT_L		GENMASK(15, 0)

struct dpmaif_drb_skb {
	struct sk_buff		*skb;
	dma_addr_t		bus_addr;
	unsigned short		data_len;
	u16			config;
};

#define DRB_SKB_IS_LAST		BIT(15)
#define DRB_SKB_IS_FRAG		BIT(14)
#define DRB_SKB_IS_MSG		BIT(13)
#define DRB_SKB_DRB_IDX		GENMASK(12, 0)

int dpmaif_tx_send_skb(struct dpmaif_ctrl *dpmaif_ctrl, enum txq_type txqt,
		       struct sk_buff *skb);
void dpmaif_tx_thread_release(struct dpmaif_ctrl *dpmaif_ctrl);
int dpmaif_tx_thread_init(struct dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_txq_free(struct dpmaif_tx_queue *txq);
void dpmaif_irq_tx_done(struct dpmaif_ctrl *dpmaif_ctrl, unsigned int que_mask);
int dpmaif_txq_init(struct dpmaif_tx_queue *txq);
void dpmaif_suspend_tx_sw_stop(struct dpmaif_ctrl *dpmaif_ctrl);
void dpmaif_stop_tx_sw(struct dpmaif_ctrl *dpmaif_ctrl);

#endif /* __T7XX_HIF_DPMA_TX_H__ */
