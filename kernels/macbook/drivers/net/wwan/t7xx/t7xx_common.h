/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021-2022, Intel Corporation.
 *
 * Authors:
 *  Haijun Liu <haijun.liu@mediatek.com>
 *  Ricardo Martinez<ricardo.martinez@linux.intel.com>
 *
 * Contributors:
 *  Andy Shevchenko <andriy.shevchenko@linux.intel.com>
 *  Chiranjeevi Rapolu <chiranjeevi.rapolu@intel.com>
 *  Eliot Lee <eliot.lee@intel.com>
 *  Moises Veleta <moises.veleta@intel.com>
 *  Sreehari Kancharla <sreehari.kancharla@intel.com>
 */

#ifndef __T7XX_COMMON_H__
#define __T7XX_COMMON_H__

#include <linux/bits.h>
#include <linux/skbuff.h>
#include <linux/types.h>

struct ccci_header {
	__le32 packet_header;
	__le32 packet_len;
	__le32 status;
	__le32 ex_msg;
};

enum mtk_txrx {
	MTK_TX,
	MTK_RX,
};

#define TX_DEFAULT_QUEUE	0

#define MTK_SKB_64K		64528		/* 63kB + CCCI header */
#define MTK_SKB_4K		3584		/* 3.5kB */
#define MTK_SKB_2K		2048
#define NET_RX_BUF		MTK_SKB_4K

#define HDR_FLD_AST		((u32)BIT(31))
#define HDR_FLD_SEQ		GENMASK(30, 16)
#define HDR_FLD_CHN		GENMASK(15, 0)

#define CCCI_H_LEN		16
/* For exception flow use CCCI_H_LEN + reserved space */
#define CCCI_H_ELEN		128

/* Coupled with HW - indicates if there is data following the CCCI header or not */
#define CCCI_HEADER_NO_DATA	0xffffffff

#define CCCI_H_AST_BIT		BIT(31)
#define CCCI_H_SEQ_FLD		GENMASK(30, 16)
#define CCCI_H_CHN_FLD		GENMASK(15, 0)

enum md_state {
	MD_STATE_INVALID,
	MD_STATE_WAITING_FOR_HS1,
	MD_STATE_WAITING_FOR_HS2,
	MD_STATE_READY,
	MD_STATE_EXCEPTION,
	MD_STATE_WAITING_TO_STOP,
	MD_STATE_STOPPED,
};

#ifdef NET_SKBUFF_DATA_USES_OFFSET
static inline unsigned int t7xx_skb_data_area_size(struct sk_buff *skb)
{
	return skb->head + skb->end - skb->data;
}
#else
static inline unsigned int t7xx_skb_data_area_size(struct sk_buff *skb)
{
	return skb->end - skb->data;
}
#endif

#endif
