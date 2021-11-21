/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __T7XX_COMMON_H__
#define __T7XX_COMMON_H__

#include <linux/types.h>

struct ccci_header {
	/* do NOT assume data[1] is data length in rx */
	u32 data[2];
	u16 channel:16;
	u16 seq_num:15;
	u16 assert_bit:1;
	u32 reserved;
} __packed;

#define CCCI_H_LEN	sizeof(struct ccci_header)
/* CCCI_H_LEN + reserved space that is used in exception flow */
#define CCCI_H_ELEN	128

/* AP<->MD messages on control or system channel */
#define CTL_ID_HS1_MSG			0x0
#define	CTL_ID_HS2_MSG			0x1
#define	CTL_ID_HS3_MSG			0x2
#define	CTL_ID_MD_RESET			0x3 /* deprecated */
#define	CTL_ID_MD_EX			0x4
#define	CCCI_DRV_VER_ERROR		0x5
#define	CTL_ID_MD_EX_REC_OK		0x6
#define	MD_EX_RESUME			0x7 /* deprecated */
#define	CTL_ID_MD_EX_PASS		0x8
#define	CTL_ID_PORT_ENUM		0x9

#define	MD_EX_CHK_ID			0x45584350
#define	MD_EX_REC_OK_CHK_ID		0x45524543

enum MD_STATE {
	INVALID = 0, /* no traffic */
	GATED, /* no traffic */
	BOOT_WAITING_FOR_HS1,
	BOOT_WAITING_FOR_HS2,
	READY,
	EXCEPTION,
	RESET, /* no traffic */
	WAITING_TO_STOP,
	STOPPED,
	MD_STATE_MAX,
}; /* for CCCI and CCMNI, broadcast by FSM */

enum HIF_STATE {
	RX_FLUSH, /* broadcast by HIF only for GRO */
	TX_IRQ, /* broadcast by HIF, only for network */
	TX_FULL, /* broadcast by HIF, only for network */
};

#endif
