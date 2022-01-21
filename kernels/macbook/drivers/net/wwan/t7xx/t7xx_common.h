/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_COMMON_H__
#define __T7XX_COMMON_H__

#include <linux/bits.h>
#include <linux/types.h>

struct ccci_header {
	/* do not assume data[1] is data length in rx */
	u32 data[2];
	u32 status;
	u32 reserved;
};

enum txq_type {
	TXQ_NORMAL,
	TXQ_FAST,
	TXQ_TYPE_CNT
};

enum direction {
	MTK_IN,
	MTK_OUT,
	MTK_INOUT,
};

#define HDR_FLD_AST		BIT(31)
#define HDR_FLD_SEQ		GENMASK(30, 16)
#define HDR_FLD_CHN		GENMASK(15, 0)

#define CCCI_H_LEN		16
/* CCCI_H_LEN + reserved space that is used in exception flow */
#define CCCI_H_ELEN		128

#define CCCI_HEADER_NO_DATA	0xffffffff

/* Control identification numbers for AP<->MD messages  */
#define CTL_ID_HS1_MSG		0x0
#define CTL_ID_HS2_MSG		0x1
#define CTL_ID_HS3_MSG		0x2
#define CTL_ID_MD_EX		0x4
#define CTL_ID_DRV_VER_ERROR	0x5
#define CTL_ID_MD_EX_ACK	0x6
#define CTL_ID_MD_EX_PASS	0x8
#define CTL_ID_PORT_ENUM	0x9

/* Modem exception check identification number */
#define MD_EX_CHK_ID		0x45584350
/* Modem exception check acknowledge identification number */
#define MD_EX_CHK_ACK_ID	0x45524543

enum md_state {
	MD_STATE_INVALID, /* no traffic */
	MD_STATE_GATED, /* no traffic */
	MD_STATE_WAITING_FOR_HS1,
	MD_STATE_WAITING_FOR_HS2,
	MD_STATE_READY,
	MD_STATE_EXCEPTION,
	MD_STATE_RESET, /* no traffic */
	MD_STATE_WAITING_TO_STOP,
	MD_STATE_STOPPED,
};

#endif
