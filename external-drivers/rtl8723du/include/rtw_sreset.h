/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _RTW_SRESET_H_
#define _RTW_SRESET_H_

/* #include <drv_types.h> */

enum {
	SRESET_TGP_NULL = 0,
	SRESET_TGP_XMIT_STATUS = 1,
	SRESET_TGP_LINK_STATUS = 2,
};

struct sreset_priv {
	_mutex	silentreset_mutex;
	u8	silent_reset_inprogress;
	u8	Wifi_Error_Status;
	unsigned long last_tx_time;
	unsigned long last_tx_complete_time;

	int dbg_trigger_point;
	u64 self_dect_tx_cnt;
	u64 self_dect_rx_cnt;
	u64 self_dect_fw_cnt;
	u64 self_dect_scan_cnt;
	u64 txbuf_empty_cnt;
	u64 tx_dma_status_cnt;
	u64 rx_dma_status_cnt;
	u8 rx_cnt;
	u8 self_dect_fw;
	u8 self_dect_scan;
	u8 is_txbuf_empty;
	u8 self_dect_case;
	u8 dbg_sreset_ctrl;
};



#define	WIFI_STATUS_SUCCESS		0
#define	USB_VEN_REQ_CMD_FAIL	BIT0
#define	USB_READ_PORT_FAIL		BIT1
#define	USB_WRITE_PORT_FAIL		BIT2
#define	WIFI_MAC_TXDMA_ERROR	BIT3
#define   WIFI_TX_HANG				BIT4
#define	WIFI_RX_HANG				BIT5
#define	WIFI_IF_NOT_EXIST			BIT6

void sreset_init_value(struct adapter *adapt);
void sreset_reset_value(struct adapter *adapt);
u8 sreset_get_wifi_status(struct adapter *adapt);
void sreset_set_wifi_error_status(struct adapter *adapt, u32 status);
void sreset_set_trigger_point(struct adapter *adapt, int tgp);
bool sreset_inprogress(struct adapter *adapt);
void sreset_reset(struct adapter *adapt);

#endif
