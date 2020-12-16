/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2020 Intel Corporation
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
 * Copyright(c) 2010 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2020 Intel Corporation
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
#ifndef __iwl_tm_infc__
#define __iwl_tm_infc__

#include <linux/types.h>
#include <linux/if_ether.h>

/*
 * Testmode GNL family command.
 * There is only one NL command, not to be
 * confused with testmode commands
 */
enum iwl_tm_gnl_cmd_t {
	IWL_TM_GNL_CMD_EXECUTE = 0,
	IWL_TM_GNL_CMD_SUBSCRIBE_EVENTS,
};


/* uCode trace buffer */
#define TRACE_BUFF_SIZE_MAX	0x200000
#define TRACE_BUFF_SIZE_MIN	0x1000
#define TRACE_BUFF_SIZE_DEF	0x20000

#define TM_CMD_BASE		0x100
#define TM_CMD_NOTIF_BASE	0x200
#define XVT_CMD_BASE		0x300
#define XVT_CMD_NOTIF_BASE	0x400
#define XVT_BUS_TESTER_BASE	0x500

/**
 * signifies iwl_tm_mod_tx_request is set to infinite mode,
 * when iwl_tm_mod_tx_request.times ==  IWL_XVT_TX_MODULATED_INFINITE
 */
#define IWL_XVT_TX_MODULATED_INFINITE (0)

#define IWL_XVT_MAX_MAC_HEADER_LENGTH (36)
#define IWL_XVT_MAX_NUM_OF_FRAMES (32)

/*
 * Periphery registers absolute lower bound. This is used in order to
 * differentiate registery access through HBUS_TARG_PRPH_.* and
 * HBUS_TARG_MEM_* accesses.
 */
#define IWL_ABS_PRPH_START (0xA00000)

/* User-Driver interface commands */
enum {
	IWL_TM_USER_CMD_HCMD = TM_CMD_BASE,
	IWL_TM_USER_CMD_REG_ACCESS,
	IWL_TM_USER_CMD_SRAM_WRITE,
	IWL_TM_USER_CMD_SRAM_READ,
	IWL_TM_USER_CMD_GET_DEVICE_INFO,
	IWL_TM_USER_CMD_GET_DEVICE_STATUS,
	IWL_TM_USER_CMD_BEGIN_TRACE,
	IWL_TM_USER_CMD_END_TRACE,
	IWL_TM_USER_CMD_TRACE_DUMP,
	IWL_TM_USER_CMD_NOTIFICATIONS,
	IWL_TM_USER_CMD_SWITCH_OP_MODE,
	IWL_TM_USER_CMD_GET_SIL_STEP,
	IWL_TM_USER_CMD_GET_DRIVER_BUILD_INFO,
	IWL_TM_USER_CMD_GET_FW_INFO,
	IWL_TM_USER_CMD_BUS_DATA_ACCESS,
	IWL_TM_USER_CMD_GET_SIL_TYPE,
	IWL_TM_USER_CMD_GET_RFID,

	IWL_TM_USER_CMD_NOTIF_UCODE_RX_PKT = TM_CMD_NOTIF_BASE,
	IWL_TM_USER_CMD_NOTIF_DRIVER,
	IWL_TM_USER_CMD_NOTIF_RX_HDR,
	IWL_TM_USER_CMD_NOTIF_COMMIT_STATISTICS,
	IWL_TM_USER_CMD_NOTIF_PHY_DB,
	IWL_TM_USER_CMD_NOTIF_DTS_MEASUREMENTS,
	IWL_TM_USER_CMD_NOTIF_MONITOR_DATA,
	IWL_TM_USER_CMD_NOTIF_UCODE_MSGS_DATA,
	IWL_TM_USER_CMD_NOTIF_APMG_PD,
	IWL_TM_USER_CMD_NOTIF_RETRIEVE_MONITOR,
	IWL_TM_USER_CMD_NOTIF_CRASH_DATA,
	IWL_TM_USER_CMD_NOTIF_BFE,
	IWL_TM_USER_CMD_NOTIF_LOC_MCSI,
	IWL_TM_USER_CMD_NOTIF_LOC_RANGE,
	IWL_TM_USER_CMD_NOTIF_IQ_CALIB,
	IWL_TM_USER_CMD_NOTIF_CT_KILL,
	IWL_TM_USER_CMD_NOTIF_CCA_EVENT,
	IWL_TM_USER_CMD_NOTIF_RUN_TIME_CALIB_DONE,
	IWL_TM_USER_CMD_NOTIF_MPAPD_EXEC_DONE,
};

/*
 * xVT commands indeces start where common
 * testmode commands indeces end
 */
enum {
	IWL_XVT_CMD_START = XVT_CMD_BASE,
	IWL_XVT_CMD_STOP,
	IWL_XVT_CMD_CONTINUE_INIT,
	IWL_XVT_CMD_GET_PHY_DB_ENTRY,
	IWL_XVT_CMD_SET_CONFIG,
	IWL_XVT_CMD_GET_CONFIG,
	IWL_XVT_CMD_MOD_TX,
	IWL_XVT_CMD_RX_HDRS_MODE,
	IWL_XVT_CMD_ALLOC_DMA,
	IWL_XVT_CMD_GET_DMA,
	IWL_XVT_CMD_FREE_DMA,
	IWL_XVT_CMD_GET_CHIP_ID,
	IWL_XVT_CMD_APMG_PD_MODE,
	IWL_XVT_CMD_GET_MAC_ADDR_INFO,
	IWL_XVT_CMD_MOD_TX_STOP,
	IWL_XVT_CMD_TX_QUEUE_CFG,
	IWL_XVT_CMD_DRIVER_CMD,

	/* Driver notifications */
	IWL_XVT_CMD_SEND_REPLY_ALIVE = XVT_CMD_NOTIF_BASE,
	IWL_XVT_CMD_SEND_RFKILL,
	IWL_XVT_CMD_SEND_NIC_ERROR,
	IWL_XVT_CMD_SEND_NIC_UMAC_ERROR,
	IWL_XVT_CMD_SEND_MOD_TX_DONE,
	IWL_XVT_CMD_ENHANCED_TX_DONE,
	IWL_XVT_CMD_TX_CMD_RESP,
	IWL_XVT_CMD_ECHO_NOTIF,

	/* Bus Tester Commands*/
	IWL_TM_USER_CMD_SV_BUS_CONFIG = XVT_BUS_TESTER_BASE,
	IWL_TM_USER_CMD_SV_BUS_RESET,
	IWL_TM_USER_CMD_SV_IO_TOGGLE,
	IWL_TM_USER_CMD_SV_GET_STATUS,
	IWL_TM_USER_CMD_SV_RD_WR_UINT8,
	IWL_TM_USER_CMD_SV_RD_WR_UINT32,
	IWL_TM_USER_CMD_SV_RD_WR_BUFFER,
};

/**
 * User space - driver interface command. These commands will be sent as
 * sub-commands through IWL_XVT_CMD_DRIVER_CMD.
 */
enum {
	IWL_DRV_CMD_CONFIG_TX_QUEUE = 0,
	IWL_DRV_CMD_SET_TX_PAYLOAD,
	IWL_DRV_CMD_TX_START,
	IWL_DRV_CMD_TX_STOP,
	IWL_DRV_CMD_GET_RX_AGG_STATS,
	IWL_DRV_CMD_CONFIG_RX_MPDU,
	IWL_DRV_CMD_ECHO_NOTIF,
};

enum {
	NOTIFICATIONS_DISABLE = 0,
	NOTIFICATIONS_ENABLE = 1,
};

/**
 * struct iwl_tm_cmd_request - Host command request
 * @id:		Command ID
 * @want_resp:	True value if response is required, else false (0)
 * @len:	Data length
 * @data:	For command in, casted to iwl_host_cmd
 *		rx packet when structure is used for command response.
 */
struct iwl_tm_cmd_request {
	__u32 id;
	__u32 want_resp;
	__u32 len;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_svt_sdio_enable - SV Tester SDIO bus enable command
 * @enable:	Function enable/disable 1/0
 */
struct iwl_tm_sdio_io_toggle {
	__u32 enable;
} __packed __aligned(4);

/* Register operations - Operation type */
enum {
	IWL_TM_REG_OP_READ = 0,
	IWL_TM_REG_OP_WRITE,
	IWL_TM_REG_OP_MAX
};

/**
 * struct iwl_tm_reg_op - Single register operation data
 * @op_type:	READ/WRITE
 * @address:	Register address
 * @value:	Write value, or read result
 */
struct iwl_tm_reg_op {
	__u32 op_type;
	__u32 address;
	__u32 value;
} __packed __aligned(4);

/**
 * struct iwl_tm_regs_request - Register operation request data
 * @num:	number of operations in struct
 * @reg_ops:	Array of register operations
 */
struct iwl_tm_regs_request {
	__u32 num;
	struct iwl_tm_reg_op reg_ops[];
} __packed __aligned(4);

/**
 * struct iwl_tm_trace_request - Data for trace begin requests
 * @size:	Requested size of trace buffer
 * @addr:	Resulting DMA address of trace buffer LSB
 */
struct iwl_tm_trace_request {
	__u64 addr;
	__u32 size;
} __packed __aligned(4);

/**
 * struct iwl_tm_sram_write_request
 * @offest:	Address offset
 * @length:	input data length
 * @buffer:	input data
 */
struct iwl_tm_sram_write_request {
	__u32 offset;
	__u32 len;
	__u8 buffer[];
} __packed __aligned(4);

/**
 * struct iwl_tm_sram_read_request
 * @offest:	Address offset
 * @length:	data length
 */
struct iwl_tm_sram_read_request {
	__u32 offset;
	__u32 length;
} __packed __aligned(4);

/**
 * struct iwl_tm_dev_info_req - Request data for get info request
 * @read_sv: rather or not read sv_srop
 */
struct iwl_tm_dev_info_req {
	__u32 read_sv;
} __packed __aligned(4);

/**
 * struct iwl_tm_dev_info - Result data for get info request
 * @dev_id:
 * @vendor_id:
 * @silicon:
 * @fw_ver:
 * @driver_ver:
 * @build_ver:
 */
struct iwl_tm_dev_info {
	__u32 dev_id;
	__u32 vendor_id;
	__u32 silicon_step;
	__u32 fw_ver;
	__u32 build_ver;
	__u8 driver_ver[];
} __packed __aligned(4);

/*
 * struct iwl_tm_thrshld_md - tx packet metadata that crosses a thrshld
 *
 * @monitor_collec_wind: the size of the window to collect the logs
 * @seq: packet sequence
 * @pkt_start: start time of triggering pkt
 * @pkt_end: end time of triggering pkt
 * @msrmnt: the tx latency of the pkt
 * @tid: tid of the pkt
 * @mode: recording mode (internal buffer or continuous recording).
 */
struct iwl_tm_thrshld_md {
	__u16 monitor_collec_wind;
	__u16 seq;
	__u32 pkt_start;
	__u32 pkt_end;
	__u32 msrmnt;
	__u16 tid;
	__u8 mode;
} __packed __aligned(4);

#define MAX_OP_MODE_LENGTH	16
/**
 * struct iwl_switch_op_mode - switch op_mode
 * @new_op_mode:	size of data
 */
struct iwl_switch_op_mode {
	__u8 new_op_mode[MAX_OP_MODE_LENGTH];
} __packed __aligned(4);

/**
 * struct iwl_sil_step - holds the silicon step
 * @silicon_step: the device silicon step
 */
struct iwl_sil_step {
	__u32 silicon_step;
} __packed __aligned(4);

/**
 * struct iwl_sil_type - holds the silicon type
 * @silicon_type: the device silicon type
 */
struct iwl_tm_sil_type {
	__u32 silicon_type;
} __packed __aligned(4);

/**
 * struct iwl_tm_rfid - Currently connected RF device info
 * @flavor:	- RFID flavor
 * @dash:	- RFID dash
 * @step:	- RFID step
 * @type:	- RFID type
 */
struct iwl_tm_rfid {
	__u32 flavor;
	__u32 dash;
	__u32 step;
	__u32 type;
} __packed __aligned(4);

#define MAX_DRIVER_VERSION_LEN	256
#define MAX_BUILD_DATE_LEN	32
/**
 * struct iwl_tm_build_info - Result data for get driver build info request
 * @driver_version: driver version in tree:branch:build:sha1
 * @branch_time: branch creation time
 * @build_time: build time
 */
struct iwl_tm_build_info {
	__u8 driver_version[MAX_DRIVER_VERSION_LEN];
	__u8 branch_time[MAX_BUILD_DATE_LEN];
	__u8 build_time[MAX_BUILD_DATE_LEN];
} __packed __aligned(4);

/**
 * struct iwl_tm_get_fw_info - get the FW info
 * @fw_major_ver: the fw major version
 * @fw_minor_ver: the fw minor version
 * @fw_capa_flags: the fw capabilities flags
 * @fw_capa_api_len: the fw capabilities api length in data
 * @fw_capa_len: the fw capabilities length in data
 * @data: fw_capa_api and fa_capa data (length defined by fw_capa_api_len
 *	+ fw_capa_len)
 */
struct iwl_tm_get_fw_info {
	__u32 fw_major_ver;
	__u32 fw_minor_ver;
	__u32 fw_capa_flags;
	__u32 fw_capa_api_len;
	__u32 fw_capa_len;
	__u8 data[];
} __packed __aligned(4);

/* xVT definitions */

#define IWL_XVT_RFKILL_OFF	0
#define IWL_XVT_RFKILL_ON	1

struct iwl_xvt_user_calib_ctrl {
	__u32 flow_trigger;
	__u32 event_trigger;
} __packed __aligned(4);

#define IWL_USER_FW_IMAGE_IDX_INIT	0
#define IWL_USER_FW_IMAGE_IDX_REGULAR	1
#define IWL_USER_FW_IMAGE_IDX_WOWLAN	2
#define IWL_USER_FW_IMAGE_IDX_TYPE_MAX	3

/**
 * iwl_xvt_sw_cfg_request - Data for set SW stack configuration request
 * @load_mask: bit[0] = Init FW
 *             bit[1] = Runtime FW
 * @cfg_mask:  Mask for which calibrations to regard
 * @phy_config: PHY_CONFIGURATION command paramter
 *              Used only for "Get SW CFG"
 * @get_calib_type: Used only in "Get SW CFG"
 *                  0: Get FW original calib ctrl
 *                  1: Get actual calib ctrl
 * @calib_ctrl: Calibration control for each FW
 */
enum {
	IWL_XVT_GET_CALIB_TYPE_DEF = 0,
	IWL_XVT_GET_CALIB_TYPE_RUNTIME
};

struct iwl_xvt_sw_cfg_request {
	__u32 load_mask;
	__u32 cfg_mask;
	__u32 phy_config;
	__u32 get_calib_type;
	__u32 dbg_flags;
	struct iwl_xvt_user_calib_ctrl calib_ctrl[IWL_UCODE_TYPE_MAX];
} __packed __aligned(4);

/**
 * iwl_xvt_sw_cfg_request - Data for set SW stack configuration request
 * @type:	Type of DB section
 * @chg_id:	Channel Group ID, relevant only when
 *		type is CHG PAPD or CHG TXP calibrations
 * @size:	Size of result data, in bytes
 * @data:	Result entry data
 */
struct iwl_xvt_phy_db_request {
	__u32 type;
	__u32 chg_id;
	__u32 size;
	__u8 data[];
} __packed __aligned(4);

#define IWL_TM_STATION_COUNT	16

/**
 * struct iwl_tm_tx_request - Data transmission request
 * @times:	  Number of times to transmit the data.
 * @delay_us:	  Delay between frames
 * @pa_detect_en: Flag. When True, enable PA detector
 * @trigger_led:  Flag. When true, light led when transmitting
 * @len:	  Size of data buffer
 * @rate_flags:	  Tx Configuration rate flags
 * @no_ack:	  Flag. When true, FW will not send ack
 * @sta_id:	  Station ID
 * @data:	  Data to transmit
 */
struct iwl_tm_mod_tx_request {
	__u32 times;
	__u32 delay_us;
	__u32 pa_detect_en;
	__u32 trigger_led;
	__u32 len;
	__u32 rate_flags;
	__u32 no_ack;
	__u8 sta_id;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_xvt_tx_mod_task_data - Data for modulated tx task handler
 * @lmac_id:	lmac index
 * @xvt:	pointer to the xvt op mode
 * @tx_req:	pointer to data of transmission request
 */
struct iwl_xvt_tx_mod_task_data {
	__u32 lmac_id;
	struct iwl_xvt *xvt;
	struct iwl_tm_mod_tx_request tx_req;
} __packed __aligned(4);

/**
 * error status for status parameter in struct iwl_xvt_tx_mod_done
 */
enum {
	XVT_TX_DRIVER_SUCCESSFUL = 0,
	XVT_TX_DRIVER_QUEUE_FULL,
	XVT_TX_DRIVER_TIMEOUT,
	XVT_TX_DRIVER_ABORTED
};

/**
 * struct iwl_xvt_tx_mod_done - Notification data for modulated tx
 * @num_of_packets: number of sent packets
 * @status: tx task handler error status
 * @lmac_id: lmac index
 */
struct iwl_xvt_tx_mod_done {
	__u64 num_of_packets;
	__u32 status;
	__u32 lmac_id;
} __packed __aligned(4);

/**
 * struct iwl_xvt_tx_mod_stop - input for stop modulated tx
 * @lmac_id: which lmac id to stop
 */
struct iwl_xvt_tx_mod_stop {
	__u32 lmac_id;
} __packed __aligned(4);

/**
 * struct iwl_xvt_rx_hdrs_mode_request - Start/Stop gathering headers info.
 * @mode: 0 - stop
 *        1 - start
 */
struct iwl_xvt_rx_hdrs_mode_request {
	__u32 mode;
} __packed __aligned(4);

/**
 * struct iwl_xvt_apmg_pd_mode_request - Start/Stop gathering apmg_pd info.
 * @mode: 0 - stop
 *        1 - start
 */
struct iwl_xvt_apmg_pd_mode_request {
	__u32 mode;
} __packed __aligned(4);

/**
 * struct iwl_xvt_alloc_dma - Data for alloc dma requests
 * @addr:	Resulting DMA address of trace buffer LSB
 * @size:	Requested size of dma buffer
 */
struct iwl_xvt_alloc_dma {
	__u64 addr;
	__u32 size;
} __packed __aligned(4);

/**
 * struct iwl_xvt_alloc_dma - Data for alloc dma requests
 * @size:	size of data
 * @data:	Data to transmit
 */
struct iwl_xvt_get_dma {
	__u32 size;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_xvt_chip_id - get the chip id from SCU
 * @registers:	an array of registers to hold the chip id data
 */
struct iwl_xvt_chip_id {
	__u32 registers[3];
} __packed __aligned(4);

/**
 * struct iwl_tm_crash_data - Notifications containing crash data
 * @data_type:	type of the data
 * @size:	data size
 * @data:	data
 */
struct iwl_tm_crash_data {
	__u32 size;
	__u8 data[];
} __packed __aligned(4);

/**
 * struct iwl_xvt_curr_mac_addr_info - Current mac address data
 * @curr_mac_addr:	the current mac address
 */
struct iwl_xvt_mac_addr_info {
	__u8 mac_addr[ETH_ALEN];
} __packed __aligned(4);

enum {
	TX_QUEUE_CFG_REMOVE,
	TX_QUEUE_CFG_ADD,
};

/**
 * iwl_xvt_tx_queue_cfg - add/remove tx queue
 * @ sta_id: station ID associated with queue
 * @ flags: 0 - remove queue, 1 - add queue
 */
struct iwl_xvt_tx_queue_cfg {
	__u8 sta_id;
	__u8 operation;
} __packed __aligned(4);

/**
 * iwl_xvt_driver_command_req - wrapper for general driver command that are sent
 * by IWL_XVT_CMD_DRIVER_CMD
 * @ command_id: sub comamnd ID
 * @ max_out_length: max size in bytes of the sub command's expected response
 * @ input_data: place holder for the sub command's input structure
 */
struct iwl_xvt_driver_command_req {
	__u32 command_id;
	__u32 max_out_length;
	__u8 input_data[0];
} __packed __aligned(4);

/**
 * iwl_xvt_driver_command_resp - response of IWL_XVT_CMD_DRIVER_CMD
 * @ command_id: sub command ID
 * @ length: resp_data length in bytes
 * @ resp_data: place holder for the sub command's rseponse data
 */
struct iwl_xvt_driver_command_resp {
	__u32 command_id;
	__u32 length;
	__u8 resp_data[0];
} __packed __aligned(4);

/**
 * iwl_xvt_txq_config - add/remove tx queue. IWL_DRV_CMD_CONFIG_TX_QUEUE input.
 * @sta_id: station id
 * @tid: TID
 * @scd_queue: scheduler queue to configure
 * @action: 1 queue enable, 0 queue disable, 2 change txq's tid owner
 *	Value is one of &enum iwl_scd_cfg_actions options
 * @aggregate: 1 aggregated queue, 0 otherwise
 * @tx_fifo: &enum iwl_mvm_tx_fifo
 * @window: BA window size
 * @reserved: reserved
 * @ssn: SSN for the BA agreement
 * @flags: flags for iwl_tx_queue_cfg_cmd. see &enum iwl_tx_queue_cfg_actions
 * @reserved2: reserved
 * @queue_size: size of configured queue
 */
struct iwl_xvt_txq_config {
	u8 sta_id;
	u8 tid;
	u8 scd_queue;
	u8 action;
	u8 aggregate;
	u8 tx_fifo;
	u8 window;
	u8 reserved;
	u16 ssn;
	u16 flags;
	u16 reserved2;
	int queue_size;
} __packed __aligned(4);

/**
 * iwl_xvt_txq_config_resp - response from IWL_DRV_CMD_CONFIG_TX_QUEUE
 * @sta_id: taken from command
 * @tid: taken from command
 * @scd_queue: queue number assigned to this RA -TID
 * @reserved: for alignment
 */
struct iwl_xvt_txq_config_resp {
	u8 sta_id;
	u8 tid;
	u8 scd_queue;
	u8 reserved;
} __packed __aligned(4);

/**
 * struct iwl_xvt_set_tx_payload - input for TX payload configuration
 * @index: payload's index in 'payloads' array in struct iwl_xvt
 * @length: payload length in bytes
 * @payload: buffer containing payload
*/
struct iwl_xvt_set_tx_payload {
	u16 index;
	u16 length;
	u8 payload[];
} __packed __aligned(4);

/**
 * struct tx_cmd_commom_data - Data shared between all queues for TX
 * command configuration
 * @rate_flags: Tx Configuration rate flags
 * @tx_flags: TX flags configuration
 * @initial_rate_index: Start index in TLC table
 * @rts_retry_limit: Max retry for RTS transmit
 * @data_retry_limit: Max retry for data transmit
 * @fragment_size: 0 - no fragnentation else - max fragment size
 * @frag_num: Array of fragments numbers
 */
struct tx_cmd_commom_data {
	u32 rate_flags;
	u32 tx_flags;
	u8 initial_rate_index;
	u8 rts_retry_limit;
	u8 data_retry_limit;
	u8 fragment_size;
	u8 frag_num[32];
} __packed __aligned(4);

/**
 * struct tx_cmd_frame - frame specific transmission data
 * @times: Number of subsequent times to transmit tx command to queue
 * @sta_id: Station index
 * @queue: Transmission queue
 * @tid_tspec: TID tspec
 * @sec_ctl: security control
 * @payload_index: payload buffer index in 'payloads' array in struct iwl_xvt
 * @key: security key
 * @header: MAC header
 */
struct tx_cmd_frame_data {
	u16 times;
	u8 sta_id;
	u8 queue;
	u8 tid_tspec;
	u8 sec_ctl;
	u8 payload_index;
	u8 reserved;
	u8 key[16];
	u8 header[IWL_XVT_MAX_MAC_HEADER_LENGTH];
} __packed __aligned(4);

/**
 * struct iwl_xvt_tx_start - Data for transmission. IWL_DRV_CMD_TX_START input.
 * @num_of_cycles: Number of times to go over frames_data. When set to zero -
 *  infinite transmit (max amount is ULLONG_MAX) - go over frames_data and sent
 *  tx until stop command is received.
 * @num_of_different_frames: actual number of entries in frames_data
 * @send_tx_resp: Whether to send FW's tx response to user
 * @reserved1: for alignment
 * @reserved2: for alignment
 * @tx_data: Tx command configuration shared data
 * @frames_data: array of specific frame data for each queue
*/
struct iwl_xvt_tx_start {
	u16 num_of_cycles;
	u16 num_of_different_frames;
	u8 send_tx_resp;
	u8 reserved1;
	u16 reserved2;
	struct tx_cmd_commom_data tx_data;
	struct tx_cmd_frame_data frames_data[IWL_XVT_MAX_NUM_OF_FRAMES];
} __packed __aligned(4);

/**
 * struct iwl_xvt_enhanced_tx_data - Data for enhanced TX task
 * @xvt: pointer to the xvt op mode
 * @tx_start_data: IWL_DRV_CMD_TX_START command's input
*/
struct iwl_xvt_enhanced_tx_data {
	struct iwl_xvt *xvt;
	struct iwl_xvt_tx_start tx_start_data;
} __packed __aligned(4);

/**
 * struct iwl_xvt_post_tx_data - transmission data per queue
 * @num_of_packets: number of sent packets
 * @queue: queue packets were sent on
 */
struct iwl_xvt_post_tx_data {
	u64 num_of_packets;
	u16 queue;
	u16 reserved;
} __packed __aligned(4);

/**
 * struct iwl_xvt_tx_done - Notification data for sent tx
 * @status: tx task handler error status
 * @num_of_queues: total number of queues tx was sent from. Equals to number of
 * entries in tx_data
 * @tx_data: data of sent frames for each queue
*/
struct iwl_xvt_tx_done {
	u32 status;
	u32 num_of_queues;
	struct iwl_xvt_post_tx_data tx_data[];
} __packed __aligned(4);

/*
 * struct iwl_xvt_get_rx_agg_stats - get rx aggregation statistics
 * @sta_id: station id of relevant ba
 * @tid: tid of relevant ba
 * @reserved: reserved
 */
struct iwl_xvt_get_rx_agg_stats {
	u8 sta_id;
	u8 tid;
	u16 reserved;
} __packed __aligned(4);

/*
 * struct iwl_xvt_get_rx_agg_stats_resp - rx aggregation statistics response
 * @dropped: number of frames dropped (e.g. too old)
 * @released: total number of frames released (either in-order or
 *	out of order (after passing the reorder buffer)
 * @skipped: number of frames skipped the reorder buffer (in-order)
 * @reordered: number of frames gone through the reorder buffer (unordered)
 */
struct iwl_xvt_get_rx_agg_stats_resp {
	u32 dropped;
	u32 released;
	u32 skipped;
	u32 reordered;
} __packed __aligned(4);

/* struct iwl_xvt_config_rx_mpdu - Whether to send RX MPDU notifications to user
 * @enable: 0 - disable. don't send rx mpdu to user 1 - send rx mpdu to user.
 * @reserved: reserved
 */
struct iwl_xvt_config_rx_mpdu_req {
	u8 enable;
	u8 reserved[3];
} __packed __aligned(4);

#endif
