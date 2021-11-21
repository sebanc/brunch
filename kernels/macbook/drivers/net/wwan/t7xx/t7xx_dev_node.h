/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __T7XX_DEV_NODE_H__
#define __T7XX_DEV_NODE_H__

#include <linux/device.h>

#define CCCI_MAGIC_NUM 0xFFFFFFFF
/* port->minor is configured in-sequence, but when we use it in code
 * it should be unique among all ports for addressing.
 */
#define CCCI_IPC_MINOR_BASE 100
#define CCCI_TTY_MINOR_BASE 250

/* ==================================================================== */
/* common structures */
/* ==================================================================== */
enum direction {
	MTK_IN = 0,
	MTK_OUT = 1,
	MTK_INOUT = 2,
};

/* ======================================================================= */
/* CCCI Channel ID and Message ID definitions */
/* ======================================================================= */
/* the channel num consists of peer_id(15:12) , channel_id(11:0)
 * peer_id:
 * 0:reserved, 1: to sAP, 2: to MD
 */
enum ccci_ch {
	/* to MD */
	CCCI_CONTROL_RX = 0x2000,
	CCCI_CONTROL_TX = 0x2001,
	CCCI_SYSTEM_RX = 0x2002,
	CCCI_SYSTEM_TX = 0x2003,
	CCCI_UART1_RX = 0x2006, /* META */
	CCCI_UART1_RX_ACK = 0x2007,
	CCCI_UART1_TX = 0x2008,
	CCCI_UART1_TX_ACK = 0x2009,
	CCCI_UART2_RX = 0x200A, /* AT  */
	CCCI_UART2_RX_ACK = 0x200B,
	CCCI_UART2_TX = 0x200C,
	CCCI_UART2_TX_ACK = 0x200D,
	CCCI_MD_LOG_RX = 0x202A, /* mdlogging */
	CCCI_MD_LOG_TX = 0x202B,
	CCCI_LB_IT_RX = 0x203E, /* loop back test */
	CCCI_LB_IT_TX = 0x203F,
	CCCI_STATUS_RX = 0x2043, /* status polling */
	CCCI_STATUS_TX = 0x2044,
	CCCI_MIPC_RX = 0x20CE, /* MIPC */
	CCCI_MIPC_TX = 0x20CF,
	CCCI_MBIM_RX = 0x20D0,
	CCCI_MBIM_TX = 0x20D1,
	CCCI_DSS0_RX = 0x20D2,
	CCCI_DSS0_TX = 0x20D3,
	CCCI_DSS1_RX = 0x20D4,
	CCCI_DSS1_TX = 0x20D5,
	CCCI_DSS2_RX = 0x20D6,
	CCCI_DSS2_TX = 0x20D7,
	CCCI_DSS3_RX = 0x20D8,
	CCCI_DSS3_TX = 0x20D9,
	CCCI_DSS4_RX = 0x20DA,
	CCCI_DSS4_TX = 0x20DB,
	CCCI_DSS5_RX = 0x20DC,
	CCCI_DSS5_TX = 0x20DD,
	CCCI_DSS6_RX = 0x20DE,
	CCCI_DSS6_TX = 0x20DF,
	CCCI_DSS7_RX = 0x20E0,
	CCCI_DSS7_TX = 0x20E1,
	CCCI_MAX_CH_NUM,
	CCCI_MONITOR_CH_ID = 0xf0000000, /* for mdinit */
	CCCI_INVALID_CH_ID = 0xffff,
};

#define CCCI_MAX_CH_ID 0xFF /* RX channel ID should NOT be >= this!! */
#define CCCI_CH_ID_MASK 0xFF

int ccci_register_dev_node(const char *name, int major_id, int minor);
void ccci_unregister_dev_node(int major_id, int minor);
int ccci_init(void);
void ccci_uninit(void);

#endif	/* __T7XX_DEV_NODE_H__ */
