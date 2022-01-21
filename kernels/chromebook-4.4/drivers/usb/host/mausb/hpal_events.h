/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_HPAL_EVENTS_H__
#define __MAUSB_HPAL_EVENTS_H__

#include "hpal.h"

#define MANAGEMENT_EVENT_TIMEOUT 3000

int mausb_msg_received_event(struct mausb_event *event,
			     struct ma_usb_hdr_common *hdr,
			     enum mausb_channel channel);
int mausb_usbdevhandle_event_to_user(struct mausb_device *dev,
				     u8 device_speed,
				     u32 route_string,
				     u16 hub_dev_handle,
				     u16 parent_hs_hub_dev_handle,
				     u16 parent_hs_hub_port, u16 mtt,
				     u8 lse, s32 *usb_dev_handle);
int mausb_usbdevhandle_event_from_user(struct mausb_device *dev,
				       struct mausb_event *event);
int mausb_ephandle_event_to_user(struct mausb_device *dev, u16 device_handle,
				 u16 descriptor_size, void *descriptor,
				 u16 *ep_handle);
int mausb_ephandle_event_from_user(struct mausb_device *dev,
				   struct mausb_event *event);
int mausb_epactivate_event_to_user(struct mausb_device *dev,
				   u16 device_handle, u16 ep_handle);
int mausb_epactivate_event_from_user(struct mausb_device *dev,
				     struct mausb_event *event);
int mausb_epinactivate_event_to_user(struct mausb_device *dev,
				     u16 device_handle,
				     u16 ep_handle);
int mausb_epinactivate_event_from_user(struct mausb_device *dev,
				       struct mausb_event *event);
int mausb_epreset_event_to_user(struct mausb_device *dev,
				u16 device_handle, u16 ep_handle,
				u8 tsp_flag);
int mausb_epreset_event_from_user(struct mausb_device *dev,
				  struct mausb_event *event);
int mausb_epdelete_event_to_user(struct mausb_device *dev,
				 u16 device_handle, u16 ep_handle);
int mausb_epdelete_event_from_user(struct mausb_device *dev,
				   struct mausb_event *event);
int mausb_modifyep0_event_to_user(struct mausb_device *dev,
				  u16 device_handle, u16 *ep_handle,
				  __le16 max_packet_size);
int mausb_modifyep0_event_from_user(struct mausb_device *dev,
				    struct mausb_event *event);
int mausb_setusbdevaddress_event_to_user(struct mausb_device *dev,
					 u16 device_handle,
					 u16 response_timeout);
int mausb_setusbdevaddress_event_from_user(struct mausb_device *dev,
					   struct mausb_event *event);
int mausb_updatedev_event_to_user(struct mausb_device *dev,
				  u16 device_handle,
				  u16 max_exit_latency, u8 hub,
				  u8 number_of_ports, u8 mtt,
				  u8 ttt, u8 integrated_hub_latency,
				  struct usb_device_descriptor *dev_descriptor);
int mausb_updatedev_event_from_user(struct mausb_device *dev,
				    struct mausb_event *event);
int mausb_usbdevdisconnect_event_to_user(struct mausb_device *dev,
					 u16 dev_handle);
int mausb_ping_event_to_user(struct mausb_device *dev);
int mausb_devdisconnect_event_to_user(struct mausb_device *dev);
int mausb_devdisconnect_event_from_user(struct mausb_device *dev,
					struct mausb_event *event);
int mausb_usbdevreset_event_to_user(struct mausb_device *dev,
				    u16 device_handle);
int mausb_usbdevreset_event_from_user(struct mausb_device *dev,
				      struct mausb_event *event);
int mausb_canceltransfer_event_to_user(struct mausb_device *dev,
				       u16 device_handle, u16 ep_handle,
				       uintptr_t urb);
int mausb_canceltransfer_event_from_user(struct mausb_device *dev,
					 struct mausb_event *event);
int mausb_cleartransfers_event_to_user(struct mausb_device *dev,
				       u16 device_handle,
				       u16 ep_handle);
int mausb_cleartransfers_event_from_user(struct mausb_device *dev,
					 struct mausb_event *event);

void mausb_dev_reset_req_event(struct mausb_event *event);
int mausb_canceltransfer_event_from_user(struct mausb_device *dev,
					 struct mausb_event *event);
void mausb_cleanup_send_data_msg_event(struct mausb_event *event);
void mausb_cleanup_received_data_msg_event(struct mausb_event *event);
void mausb_cleanup_delete_data_transfer_event(struct mausb_event *event);

#endif /* __MAUSB_HPAL_EVENTS_H__ */
