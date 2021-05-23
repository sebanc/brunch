/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTL8723D_RECV_H__
#define __RTL8723D_RECV_H__

#define RECV_BLK_SZ 512
#define RECV_BLK_CNT 16
#define RECV_BLK_TH RECV_BLK_CNT

#ifndef MAX_RECVBUF_SZ
	#define MAX_RECVBUF_SZ (4000) /* about 4K */
#endif /* !MAX_RECVBUF_SZ */

/* Rx smooth factor */
#define	Rx_Smooth_Factor (20)

/*-----------------------------------------------------------------*/
/*	RTL8723D RX BUFFER DESC                                      */
/*-----------------------------------------------------------------*/
/*DWORD 0*/
#define SET_RX_BUFFER_DESC_DATA_LENGTH_8723D(__pRxStatusDesc, __Value)		SET_BITS_TO_LE_4BYTE(__pRxStatusDesc, 0, 14, __Value)
#define SET_RX_BUFFER_DESC_LS_8723D(__pRxStatusDesc, __Value)	SET_BITS_TO_LE_4BYTE(__pRxStatusDesc, 15, 1, __Value)
#define SET_RX_BUFFER_DESC_FS_8723D(__pRxStatusDesc, __Value)		SET_BITS_TO_LE_4BYTE(__pRxStatusDesc, 16, 1, __Value)
#define SET_RX_BUFFER_DESC_TOTAL_LENGTH_8723D(__pRxStatusDesc, __Value)		SET_BITS_TO_LE_4BYTE(__pRxStatusDesc, 16, 15, __Value)

#define GET_RX_BUFFER_DESC_OWN_8723D(__pRxStatusDesc)		LE_BITS_TO_4BYTE(__pRxStatusDesc, 31, 1)
#define GET_RX_BUFFER_DESC_LS_8723D(__pRxStatusDesc)		LE_BITS_TO_4BYTE(__pRxStatusDesc, 15, 1)
#define GET_RX_BUFFER_DESC_FS_8723D(__pRxStatusDesc)		LE_BITS_TO_4BYTE(__pRxStatusDesc, 16, 1)
#ifdef USING_RX_TAG
	#define GET_RX_BUFFER_DESC_RX_TAG_8723D(__pRxStatusDesc)		LE_BITS_TO_4BYTE(__pRxStatusDesc, 16, 13)
#else
	#define GET_RX_BUFFER_DESC_TOTAL_LENGTH_8723D(__pRxStatusDesc)		LE_BITS_TO_4BYTE(__pRxStatusDesc, 16, 15)
#endif

/*DWORD 1*/
#define SET_RX_BUFFER_PHYSICAL_LOW_8723D(__pRxStatusDesc, __Value)		SET_BITS_TO_LE_4BYTE(__pRxStatusDesc+4, 0, 32, __Value)

/*DWORD 2*/
#ifdef CONFIG_64BIT_DMA
	#define SET_RX_BUFFER_PHYSICAL_HIGH_8723D(__pRxStatusDesc, __Value)		SET_BITS_TO_LE_4BYTE(__pRxStatusDesc+8, 0, 32, __Value)
#else
	#define SET_RX_BUFFER_PHYSICAL_HIGH_8723D(__pRxStatusDesc, __Value)
#endif

int rtl8723du_init_recv_priv(struct adapter *adapt);
void rtl8723du_free_recv_priv(struct adapter *adapt);
void rtl8723du_init_recvbuf(struct adapter *adapt, struct recv_buf *precvbuf);

void rtl8723d_query_rx_desc_status(union recv_frame *precvframe, u8 *pdesc);

#endif /* __RTL8723D_RECV_H__ */
