/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_MAUSB_EVENT_H__
#define __MAUSB_MAUSB_EVENT_H__

#include "ma_usb.h"

#define MAUSB_MAX_NUM_OF_MA_DEVS			15
#define MAUSB_RING_BUFFER_SIZE				1024
#define MAUSB_MAX_DATA_IN_REQ_SIZE			28

#define MAUSB_EVENT_TYPE_DEV_RESET			1u
#define MAUSB_EVENT_TYPE_USB_DEV_HANDLE			2u
#define MAUSB_EVENT_TYPE_EP_HANDLE			3u
#define MAUSB_EVENT_TYPE_EP_HANDLE_ACTIVATE		4u
#define MAUSB_EVENT_TYPE_EP_HANDLE_INACTIVATE		5u
#define MAUSB_EVENT_TYPE_EP_HANDLE_RESET		6u
#define MAUSB_EVENT_TYPE_EP_HANDLE_DELETE		7u
#define MAUSB_EVENT_TYPE_MODIFY_EP0			8u
#define MAUSB_EVENT_TYPE_SET_USB_DEV_ADDRESS		9u
#define MAUSB_EVENT_TYPE_UPDATE_DEV			10u
#define MAUSB_EVENT_TYPE_USB_DEV_DISCONNECT		11u
#define MAUSB_EVENT_TYPE_PING				12u
#define MAUSB_EVENT_TYPE_DEV_DISCONNECT			13u
#define MAUSB_EVENT_TYPE_USB_DEV_RESET			14u
#define MAUSB_EVENT_TYPE_CANCEL_TRANSFER		15u
#define MAUSB_EVENT_TYPE_CLEAR_TRANSFERS		16u

#define MAUSB_EVENT_TYPE_PORT_CHANGED			80u
#define MAUSB_EVENT_TYPE_SEND_MGMT_MSG			81u
#define MAUSB_EVENT_TYPE_SEND_DATA_MSG			82u
#define MAUSB_EVENT_TYPE_RECEIVED_MGMT_MSG		83u
#define MAUSB_EVENT_TYPE_RECEIVED_DATA_MSG		84u
#define MAUSB_EVENT_TYPE_URB_COMPLETE			85u
#define MAUSB_EVENT_TYPE_SEND_ACK			86u
#define MAUSB_EVENT_TYPE_ITERATOR_RESET			87u
#define MAUSB_EVENT_TYPE_ITERATOR_SEEK			88u
#define MAUSB_EVENT_TYPE_DELETE_DATA_TRANSFER		89u
#define MAUSB_EVENT_TYPE_DELETE_MA_DEV			90u
#define MAUSB_EVENT_TYPE_USER_FINISHED			91u
#define MAUSB_EVENT_TYPE_RELEASE_EVENT_RESOURCES	92u
#define MAUSB_EVENT_TYPE_NETWORK_DISCONNECTED		93u
#define MAUSB_EVENT_TYPE_MGMT_REQUEST_TIMED_OUT		94u

#define MAUSB_EVENT_TYPE_NONE				255u

#define MAUSB_DATA_MSG_DIRECTION_OUT			0
#define MAUSB_DATA_MSG_DIRECTION_IN			1
#define MAUSB_DATA_MSG_CONTROL MAUSB_DATA_MSG_DIRECTION_OUT

struct mausb_devhandle {
	u64 event_id;
	u32 route_string;
	u16 hub_dev_handle;
	u16 parent_hs_hub_dev_handle;
	u16 parent_hs_hub_port;
	u16 mtt;
	/* dev_handle assigned in user */
	u16 dev_handle;
	u8  device_speed;
	u8  lse;
};

struct mausb_ephandle {
	u64 event_id;
	u16 device_handle;
	u16 descriptor_size;
	/* ep_handle assigned in user */
	u16 ep_handle;
	char	 descriptor[sizeof(struct ma_usb_ephandlereq_desc_ss)];
};

struct mausb_epactivate {
	u64 event_id;
	u16 device_handle;
	u16 ep_handle;
};

struct mausb_epinactivate {
	u64 event_id;
	u16 device_handle;
	u16 ep_handle;
};

struct mausb_epreset {
	u64 event_id;
	u16 device_handle;
	u16 ep_handle;
	u8  tsp;
};

struct mausb_epdelete {
	u64 event_id;
	u16 device_handle;
	u16 ep_handle;
};

struct mausb_updatedev {
	u64 event_id;
	u16 device_handle;
	u16 max_exit_latency;
	struct ma_usb_updatedevreq_desc update_descriptor;
	u8  hub;
	u8  number_of_ports;
	u8  mtt;
	u8  ttt;
	u8  integrated_hub_latency;
};

struct mausb_usbdevreset {
	u64 event_id;
	u16 device_handle;
};

struct mausb_modifyep0 {
	u64 event_id;
	u16 device_handle;
	u16 ep_handle;
	__le16 max_packet_size;
};

struct mausb_setusbdevaddress {
	u64 event_id;
	u16 device_handle;
	u16 response_timeout;
};

struct mausb_usbdevdisconnect {
	u16 device_handle;
};

struct mausb_canceltransfer {
	u64 urb;
	u16 device_handle;
	u16 ep_handle;
};

struct mausb_cleartransfers {
	u64 event_id;
	u16 device_handle;
	u16 ep_handle;
};

struct mausb_devdisconnect {
	u64 event_id;
};

struct mausb_mgmt_hdr {
	__aligned(4) char hdr[MAUSB_MAX_MGMT_SIZE];
};

struct mausb_mgmt_req_timedout {
	u64 event_id;
};

struct mausb_delete_ma_dev {
	u64 event_id;
	u16 device_id;
};

/* TODO split mgmt_event to generic send mgmt req and specific requests */
struct mausb_mgmt_event {
	union {
		struct mausb_devhandle		dev_handle;
		struct mausb_ephandle		ep_handle;
		struct mausb_epactivate		ep_activate;
		struct mausb_epinactivate	ep_inactivate;
		struct mausb_epreset		ep_reset;
		struct mausb_epdelete		ep_delete;
		struct mausb_modifyep0		modify_ep0;
		struct mausb_setusbdevaddress	set_usb_dev_address;
		struct mausb_updatedev		update_dev;
		struct mausb_usbdevreset	usb_dev_reset;
		struct mausb_usbdevdisconnect	usb_dev_disconnect;
		struct mausb_canceltransfer	cancel_transfer;
		struct mausb_cleartransfers	clear_transfers;
		struct mausb_devdisconnect	dev_disconnect;
		struct mausb_mgmt_hdr		mgmt_hdr;
		struct mausb_mgmt_req_timedout	mgmt_req_timedout;
		struct mausb_delete_ma_dev	delete_ma_dev;
	};
};

struct mausb_data_event {
	uintptr_t urb;
	uintptr_t recv_buf;
	u32 iterator_seek_delta;
	u32 transfer_size;
	u32 rem_transfer_size;
	u32 transfer_flags;
	u32 isoch_seg_num;
	u32 req_id;
	u32 payload_size;
	s32 status;

	__aligned(4) char hdr[MAUSB_TRANSFER_HDR_SIZE];
	__aligned(4) char hdr_ack[MAUSB_TRANSFER_HDR_SIZE];

	u16 device_id;
	u16 ep_handle;
	u16 packet_size;
	u8  setup_packet;
	u8  direction;
	u8  transfer_type;
	u8  first_control_packet;
	u8  transfer_eot;
	u8  mausb_address;
	u8  mausb_ssid;
};

struct mausb_port_changed_event {
	u8 port;
	u8 dev_type;
	u8 dev_speed;
	u8 lse;
};

struct mausb_event {
	union {
		struct mausb_mgmt_event		mgmt;
		struct mausb_data_event		data;
		struct mausb_port_changed_event port_changed;
	};
	s32 status;
	u8 type;
	u8 madev_addr;
};

struct mausb_events_notification {
	u16 num_of_events;
	u16 num_of_completed_events;
	u8  madev_addr;
};

#endif /* __MAUSB_MAUSB_EVENT_H__ */
