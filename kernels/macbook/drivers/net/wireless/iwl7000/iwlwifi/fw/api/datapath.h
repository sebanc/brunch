/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2012-2014, 2018-2021 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 * Copyright (C) 2016-2017 Intel Deutschland GmbH
 */
#ifndef __iwl_fw_api_datapath_h__
#define __iwl_fw_api_datapath_h__

/**
 * enum iwl_data_path_subcmd_ids - data path group commands
 */
enum iwl_data_path_subcmd_ids {
	/**
	 * @DQA_ENABLE_CMD: &struct iwl_dqa_enable_cmd
	 */
	DQA_ENABLE_CMD = 0x0,

	/**
	 * @UPDATE_MU_GROUPS_CMD: &struct iwl_mu_group_mgmt_cmd
	 */
	UPDATE_MU_GROUPS_CMD = 0x1,

	/**
	 * @TRIGGER_RX_QUEUES_NOTIF_CMD: &struct iwl_rxq_sync_cmd
	 */
	TRIGGER_RX_QUEUES_NOTIF_CMD = 0x2,

	/**
	 * @WNM_80211V_TIMING_MEASUREMENT_CONFIG_CMD: &struct iwl_time_sync_cfg_cmd
	 */
	WNM_80211V_TIMING_MEASUREMENT_CONFIG_CMD = 0x4,

	/**
	 * @STA_HE_CTXT_CMD: &struct iwl_he_sta_context_cmd
	 */
	STA_HE_CTXT_CMD = 0x7,

	/**
	 * @AX_SOFTAP_CLIENT_TESTMODE: &struct ax_softap_client_testmode_cmd
	 */
	AX_SOFTAP_CLIENT_TESTMODE = 0xC,

	/**
	 * @RFH_QUEUE_CONFIG_CMD: &struct iwl_rfh_queue_config
	 */
	RFH_QUEUE_CONFIG_CMD = 0xD,

	/**
	 * @TLC_MNG_CONFIG_CMD: &struct iwl_tlc_config_cmd
	 */
	TLC_MNG_CONFIG_CMD = 0xF,

	/**
	 * @HE_AIR_SNIFFER_CONFIG_CMD: &struct iwl_he_monitor_cmd
	 */
	HE_AIR_SNIFFER_CONFIG_CMD = 0x13,

	/**
	 * @CHEST_COLLECTOR_FILTER_CONFIG_CMD: Configure the CSI
	 *	matrix collection, uses &struct iwl_channel_estimation_cfg_v1
	 *	or &struct iwl_channel_estimation_cfg
	 */
	CHEST_COLLECTOR_FILTER_CONFIG_CMD = 0x14,

	/**
	 * @MONITOR_NOTIF: Datapath monitoring notification, using
	 *	&struct iwl_datapath_monitor_notif
	 */
	MONITOR_NOTIF = 0xF4,

	/**
	 * @RX_NO_DATA_NOTIF: &struct iwl_rx_no_data
	 */
	RX_NO_DATA_NOTIF = 0xF5,

	/**
	 * @THERMAL_DUAL_CHAIN_REQUEST: firmware request for SMPS mode,
	 *	&struct iwl_thermal_dual_chain_request
	 */
	THERMAL_DUAL_CHAIN_REQUEST = 0xF6,

	/**
	 * @TLC_MNG_UPDATE_NOTIF: &struct iwl_tlc_update_notif
	 */
	TLC_MNG_UPDATE_NOTIF = 0xF7,

	/**
	 * @STA_PM_NOTIF: &struct iwl_mvm_pm_state_notification
	 */
	STA_PM_NOTIF = 0xFD,

	/**
	 * @MU_GROUP_MGMT_NOTIF: &struct iwl_mu_group_mgmt_notif
	 */
	MU_GROUP_MGMT_NOTIF = 0xFE,

	/**
	 * @RX_QUEUES_NOTIFICATION: &struct iwl_rxq_sync_notification
	 */
	RX_QUEUES_NOTIFICATION = 0xFF,
};

/**
 * struct iwl_mu_group_mgmt_cmd - VHT MU-MIMO group configuration
 *
 * @reserved: reserved
 * @membership_status: a bitmap of MU groups
 * @user_position:the position of station in a group. If the station is in the
 *	group then bits (group * 2) is the position -1
 */
struct iwl_mu_group_mgmt_cmd {
	__le32 reserved;
	__le32 membership_status[2];
	__le32 user_position[4];
} __packed; /* MU_GROUP_ID_MNG_TABLE_API_S_VER_1 */

/**
 * struct iwl_mu_group_mgmt_notif - VHT MU-MIMO group id notification
 *
 * @membership_status: a bitmap of MU groups
 * @user_position: the position of station in a group. If the station is in the
 *	group then bits (group * 2) is the position -1
 */
struct iwl_mu_group_mgmt_notif {
	__le32 membership_status[2];
	__le32 user_position[4];
} __packed; /* MU_GROUP_MNG_NTFY_API_S_VER_1 */

enum iwl_channel_estimation_flags {
	IWL_CHANNEL_ESTIMATION_ENABLE	= BIT(0),
	IWL_CHANNEL_ESTIMATION_TIMER	= BIT(1),
	IWL_CHANNEL_ESTIMATION_COUNTER	= BIT(2),
	/* starting from v2: */
	IWL_CHANNEL_ESTIMATION_INTERVAL	= BIT(3),
};

enum iwl_time_sync_protocol_type {
	IWL_TIME_SYNC_PROTOCOL_TM	= BIT(0),
	IWL_TIME_SYNC_PROTOCOL_FTM	= BIT(1),
}; /* WNM_TIMING_ENABLED_PROTOCOL_API_E_VER_1 */

/**
 * struct iwl_time_sync_cfg_cmd - TM/FTM time sync measurement configuration
 *
 * @protocols: The type of frames to raise notifications for. A bitmap
 *	of @iwl_time_sync_protocol_type
 * @peer_addr: peer address with which TM/FTM measurements are required
 * @reserved: for alignment
 */
struct iwl_time_sync_cfg_cmd {
	__le32 protocols;
	u8 peer_addr[ETH_ALEN];
	u8 reserved[2];
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_CONFIG_CMD_API_S_VER_1 */

/**
 * struct iwl_time_msmt_cfm_notify - Time Sync measurement confirmation
 * notification for TM/FTM. Sent on receipt of 802.11 Ack from peer for the
 * Tx'ed TM/FTM measurement action frame.
 *
 * @peer_addr: peer address
 * @reserved: for alignment
 * @dialog_token: measurement flow dialog token number
 * @t1_hi: high dword of t1-time of the Tx'ed action frame departure on
 *	sender side in units of 10 nano seconds
 * @t1_lo: low dword of t1-time of the Tx'ed action frame departure on
 *	sender side in units of 10 nano seconds
 * @t1_max_err: maximum t1-time error in units of 10 nano seconds
 * @t4_hi: high dword of t4-time of the Rx'ed action frame's Ack arrival on
 *	sender side in units of 10 nano seconds
 * @t4_lo: low dword of t4-time of the Rx'ed action frame's Ack arrival on
 *	sender side in units of 10 nano seconds
 * @t4_max_err: maximum t4-time error in units of 10 nano seconds
 */
struct iwl_time_msmt_cfm_notify {
	u8 peer_addr[ETH_ALEN];
	u8 reserved[2];
	__le32 dialog_token;
	__le32 t1_hi;
	__le32 t1_lo;
	__le32 t1_max_err;
	__le32 t4_hi;
	__le32 t4_lo;
	__le32 t4_max_err;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_CONFIRM_NTFY_API_S_VER_1 */

/* PTP_CTX_MAX_DATA_SIZE_IN_API_D_VER_1 */
#define PTP_CTX_MAX_DATA_SIZE   128

/**
 * struct iwl_time_msmt_ptp_ctx - Vendor specific information element
 * to allow a space for flexibility for the userspace App
 *
 * @element_id: element id of vendor specific ie
 * @length: length of vendor specific ie
 * @reserved: for alignment
 * @data: vendor specific data blob
 */
struct iwl_time_msmt_ptp_ctx {
	/* Differentiate between FTM and TM specific Vendor IEs */
	union {
		struct {
			u8 element_id;
			u8 length;
			__le16 reserved;
			u8 data[PTP_CTX_MAX_DATA_SIZE];
		} ftm; /* FTM specific vendor IE */
		struct {
			u8 element_id;
			u8 length;
			u8 data[PTP_CTX_MAX_DATA_SIZE];
		} tm; /* TM specific vendor IE */
	};
} __packed /* PTP_CTX_VER_1 */;

/**
 * struct iwl_time_msmt_notify - Time Sync measurement notification
 * for TM/FTM, along with additional meta data.
 *
 * @peer_addr: peer address
 * @reserved: for alignment
 * @dialog_token: measurement flow dialog token number
 * @followup_dialog_token: Measurement flow previous dialog token number
 * @t1_hi: high dword of t1-time of the Tx'ed action frame departure on
 *	sender side in units of 10 nano seconds
 * @t1_lo: low dword of t1-time of the Tx'ed action frame departure on
 *	sender side in units of 10 nano seconds
 * @t1_max_err: maximum t1-time error in units of 10 nano seconds
 * @t4_hi: high dword of t4-time of the Rx'ed action frame's Ack arrival on
 *	sender side in units of 10 nano seconds
 * @t4_lo: low dword of t4-time of the Rx'ed action frame's Ack arrival on
 *	sender side in units of 10 nano seconds
 * @t4_max_err: maximum t4-time error in units of 10 nano seconds
 * @t2_hi: high dword of t2-time of the Rx'ed action frame arrival on
 *	receiver side in units of 10 nano seconds
 * @t2_lo: low dword of t2-time of the Rx'ed action frame arrival on
 *	receiver side in units of 10 nano seconds
 * @t2_max_err: maximum t2-time error in units of 10 nano seconds
 * @t3_hi: high dword of t3-time of the Tx'ed action frame's Ack departure on
 *	receiver side in units of 10 nano seconds
 * @t3_lo: low dword of t3-time of the Tx'ed action frame's Ack departure on
 *	receiver side in units of 10 nano seconds
 * @t3_max_err: maximum t3-time error in units of 10 nano seconds
 * @ptp: vendor specific information element
 */
struct iwl_time_msmt_notify {
	u8 peer_addr[ETH_ALEN];
	u8 reserved[2];
	__le32 dialog_token;
	__le32 followup_dialog_token;
	__le32 t1_hi;
	__le32 t1_lo;
	__le32 t1_max_err;
	__le32 t4_hi;
	__le32 t4_lo;
	__le32 t4_max_err;
	__le32 t2_hi;
	__le32 t2_lo;
	__le32 t2_max_err;
	__le32 t3_hi;
	__le32 t3_lo;
	__le32 t3_max_err;
	struct iwl_time_msmt_ptp_ctx ptp;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_NTFY_API_S_VER_1 */

/**
 * struct iwl_channel_estimation_cfg_v1 - channel estimation reporting config
 */
struct iwl_channel_estimation_cfg_v1 {
	/**
	 * @flags: flags, see &enum iwl_channel_estimation_flags
	 */
	__le32 flags;
	/**
	 * @timer: if enabled via flags, automatically disable after this many
	 *	microseconds
	 */
	__le32 timer;
	/**
	 * @count: if enabled via flags, automatically disable after this many
	 *	frames with channel estimation matrix were captured
	 */
	__le32 count;
	/**
	 * @rate_n_flags_mask: only try to record the channel estimation matrix
	 *	if the rate_n_flags value for the received frame (let's call
	 *	that rx_rnf) matches the mask/value given here like this:
	 *	(rx_rnf & rate_n_flags_mask) == rate_n_flags_val.
	 */
	__le32 rate_n_flags_mask;
	/**
	 * @rate_n_flags_val: see @rate_n_flags_mask
	 */
	__le32 rate_n_flags_val;
	/**
	 * @reserved: reserved (for alignment)
	 */
	__le32 reserved;
	/**
	 * @frame_types: bitmap of frame types to capture, the received frame's
	 *	subtype|type takes 6 bits in the frame and the corresponding bit
	 *	in this field must be set to 1 to capture channel estimation for
	 *	that frame type. Set to all-ones to enable capturing for all
	 *	frame types.
	 */
	__le64 frame_types;
} __packed; /* CHEST_COLLECTOR_FILTER_CMD_API_S_VER_1 */

/* CHEST_MAX_MAC_ADDR_FILTERED_IN_API_D_VER_1 */
#define IWL_NUM_CHANNEL_ESTIMATION_FILTER_ADDRS 20

/**
 * struct iwl_channel_estimation_cfg - channel estimation reporting config
 */
struct iwl_channel_estimation_cfg {
	/**
	 * @flags: flags, see &enum iwl_channel_estimation_flags
	 */
	__le32 flags;
	/**
	 * @timer: if enabled via flags, automatically disable after this many
	 *	microseconds
	 */
	__le32 timer;
	/**
	 * @count: if enabled via flags, automatically disable after this many
	 *	frames with channel estimation matrix were captured
	 */
	__le32 count;
	/**
	 * @rate_n_flags_mask: only try to record the channel estimation matrix
	 *	if the rate_n_flags value for the received frame (let's call
	 *	that rx_rnf) matches the mask/value given here like this:
	 *	(rx_rnf & rate_n_flags_mask) == rate_n_flags_val.
	 */
	__le32 rate_n_flags_mask;
	/**
	 * @rate_n_flags_val: see @rate_n_flags_mask
	 */
	__le32 rate_n_flags_val;
	/**
	 * @min_time_between_collection: minimum time between collecting data
	 */
	__le32 min_time_between_collection;
	/**
	 * @frame_types: bitmap of frame types to capture, the received frame's
	 *	subtype|type takes 6 bits in the frame and the corresponding bit
	 *	in this field must be set to 1 to capture channel estimation for
	 *	that frame type. Set to all-ones to enable capturing for all
	 *	frame types.
	 */
	__le64 frame_types;
	/**
	 * @num_filter_addrs: number of MAC address filters configured, if 0
	 *	no filters are applied
	 */
	__le32 num_filter_addrs;
	/**
	 * @filter_addrs: MAC address filters, used length in @num_filter_addrs
	 */
	struct {
		u8 addr[ETH_ALEN];
		__le16 reserved;
	} filter_addrs[IWL_NUM_CHANNEL_ESTIMATION_FILTER_ADDRS];
} __packed; /* CHEST_COLLECTOR_FILTER_CMD_API_S_VER_2 */

enum iwl_datapath_monitor_notif_type {
	IWL_DP_MON_NOTIF_TYPE_EXT_CCA,
};

struct iwl_datapath_monitor_notif {
	__le32 type;
	u8 mac_id;
	u8 reserved[3];
} __packed; /* MONITOR_NTF_API_S_VER_1 */

/**
 * enum iwl_thermal_dual_chain_req_events - firmware SMPS request event
 * @THERMAL_DUAL_CHAIN_REQ_ENABLE: (re-)enable dual-chain operation
 *	(subject to other constraints)
 * @THERMAL_DUAL_CHAIN_REQ_DISABLE: disable dual-chain operation
 *	(static SMPS)
 */
enum iwl_thermal_dual_chain_req_events {
	THERMAL_DUAL_CHAIN_REQ_ENABLE,
	THERMAL_DUAL_CHAIN_REQ_DISABLE,
}; /* THERMAL_DUAL_CHAIN_DISABLE_STATE_API_E_VER_1 */

/**
 * struct iwl_thermal_dual_chain_request - SMPS request
 * @event: the type of request, see &enum iwl_thermal_dual_chain_req_events
 */
struct iwl_thermal_dual_chain_request {
	__le32 event;
} __packed; /* THERMAL_DUAL_CHAIN_DISABLE_REQ_NTFY_API_S_VER_1 */

#endif /* __iwl_fw_api_datapath_h__ */
