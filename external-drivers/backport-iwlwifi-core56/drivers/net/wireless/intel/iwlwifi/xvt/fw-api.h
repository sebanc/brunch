/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef __fw_api_h__
#define __fw_api_h__

#include "fw/api/rs.h"
#include "fw/api/txq.h"
#include "fw/api/location.h"
#include "fw/api/alive.h"
#include "fw/api/phy.h"
#include "fw/api/nvm-reg.h"
#include "fw/api/commands.h"
#include "fw/api/tx.h"
#include "fw/api/config.h"
#include "fw/api/datapath.h"

#define IWL_XVT_DEFAULT_TX_QUEUE	1
#define IWL_XVT_DEFAULT_TX_FIFO		3

#define IWL_XVT_TX_STA_ID_DEFAULT	0

#define XVT_LMAC_0_ID	0
#define XVT_LMAC_1_ID	1

enum {
	APMG_PD_SV_CMD = 0x43,

	/* ToF */
	LOCATION_GROUP_NOTIFICATION = 0x11,

	NVM_COMMIT_COMPLETE_NOTIFICATION = 0xad,
	GET_SET_PHY_DB_CMD = 0x8f,

	/* BFE */
	REPLY_HD_PARAMS_CMD = 0xa6,

	REPLY_RX_DSP_EXT_INFO = 0xc4,

	REPLY_DEBUG_XVT_CMD = 0xf3,
};

struct xvt_alive_resp_ver2 {
	__le16 status;
	__le16 flags;
	u8 ucode_minor;
	u8 ucode_major;
	__le16 id;
	u8 api_minor;
	u8 api_major;
	u8 ver_subtype;
	u8 ver_type;
	u8 mac;
	u8 opt;
	__le16 reserved2;
	__le32 timestamp;
	__le32 error_event_table_ptr;   /* SRAM address for error log */
	__le32 log_event_table_ptr;     /* SRAM address for LMAC event log */
	__le32 cpu_register_ptr;
	__le32 dbgm_config_ptr;
	__le32 alive_counter_ptr;
	__le32 scd_base_ptr;            /* SRAM address for SCD */
	__le32 st_fwrd_addr;            /* pointer to Store and forward */
	__le32 st_fwrd_size;
	u8 umac_minor;                  /* UMAC version: minor */
	u8 umac_major;                  /* UMAC version: major */
	__le16 umac_id;                 /* UMAC version: id */
	__le32 error_info_addr;         /* SRAM address for UMAC error log */
	__le32 dbg_print_buff_addr;
} __packed; /* ALIVE_RES_API_S_VER_2 */

enum {
	XVT_DBG_GET_SVDROP_VER_OP = 0x01,
};

struct xvt_debug_cmd {
	__le32 opcode;
	__le32 dw_num;
}; /* DEBUG_XVT_CMD_API_S_VER_1 */

struct xvt_debug_res {
	__le32 dw_num;
	__le32 data[0];
}; /* DEBUG_XVT_RES_API_S_VER_1 */

#endif /* __fw_api_h__ */
