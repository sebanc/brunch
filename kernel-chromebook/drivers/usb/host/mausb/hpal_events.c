// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#include "hpal_events.h"

#include "hcd.h"
#include "utils.h"

void mausb_dev_reset_req_event(struct mausb_event *event)
{
	event->type = MAUSB_EVENT_TYPE_DEV_RESET;
}

static int mausb_mgmt_msg_received_event(struct mausb_event *event,
					 struct ma_usb_hdr_common *hdr,
					 enum mausb_channel channel)
{
	int status = 0;

	dev_vdbg(mausb_host_dev.this_device, "channel=%d, type=%d", channel,
		 hdr->type);
	if (hdr->length <= MAUSB_MAX_MGMT_SIZE) {
		event->type = MAUSB_EVENT_TYPE_RECEIVED_MGMT_MSG;
		memcpy(event->mgmt.mgmt_hdr.hdr, hdr, hdr->length);
	} else {
		dev_err(mausb_host_dev.this_device, "MGMT message to long, failed to copy");
		status = -EINVAL;
	}

	kfree(hdr);
	return status;
}

static int mausb_data_msg_received_event(struct mausb_event *event,
					 struct ma_usb_hdr_common *hdr,
					 enum mausb_channel channel)
{
	event->type		  = MAUSB_EVENT_TYPE_RECEIVED_DATA_MSG;
	event->data.transfer_type = mausb_transfer_type_from_hdr(hdr);
	event->data.device_id	  = (u16)((hdr->ssid << 8) | hdr->dev_addr);
	event->data.ep_handle	  = hdr->handle.epv;
	event->data.recv_buf	  = (uintptr_t)hdr;

	memcpy(event->data.hdr, hdr, MAUSB_TRANSFER_HDR_SIZE);

	if (mausb_ctrl_transfer(hdr) &&
	    hdr->length <= 2 * MAUSB_TRANSFER_HDR_SIZE) {
		memcpy(event->data.hdr_ack,
		       shift_ptr(hdr, MAUSB_TRANSFER_HDR_SIZE),
		       (size_t)(hdr->length - MAUSB_TRANSFER_HDR_SIZE));
	}

	return 0;
}

static int mausb_isoch_msg_received_event(struct mausb_event *event,
					  struct ma_usb_hdr_common *hdr,
					  enum mausb_channel channel)
{
	event->type		  = MAUSB_EVENT_TYPE_RECEIVED_DATA_MSG;
	event->data.transfer_type = mausb_transfer_type_from_hdr(hdr);
	event->data.device_id	  = (u16)((hdr->ssid << 8) | hdr->dev_addr);
	event->data.ep_handle	  = hdr->handle.epv;
	event->data.recv_buf	  = (uintptr_t)hdr;

	memcpy(event->data.hdr, hdr, MAUSB_TRANSFER_HDR_SIZE);

	return 0;
}

int mausb_msg_received_event(struct mausb_event *event,
			     struct ma_usb_hdr_common *hdr,
			     enum mausb_channel channel)
{
	dev_vdbg(mausb_host_dev.this_device, "channel=%d, type=%d", channel,
		 hdr->type);
	if (mausb_is_management_hdr_type(hdr->type))
		return mausb_mgmt_msg_received_event(event, hdr, channel);
	else if (hdr->type == MA_USB_HDR_TYPE_DATA_RESP(TRANSFER))
		return mausb_data_msg_received_event(event, hdr, channel);
	else if (hdr->type == MA_USB_HDR_TYPE_DATA_RESP(ISOCHTRANSFER))
		return mausb_isoch_msg_received_event(event, hdr, channel);

	dev_warn(mausb_host_dev.this_device, "Unknown event type event=%d",
		 hdr->type);
	kfree(hdr);
	return -EBADR;
}

static void mausb_prepare_completion(struct mausb_completion *mausb_completion,
				     struct completion *completion,
				     struct mausb_event *event, u64 event_id)
{
	init_completion(completion);

	mausb_completion->completion_event = completion;
	mausb_completion->event_id	   = event_id;
	mausb_completion->mausb_event	   = event;
}

static int mausb_wait_for_completion(struct mausb_event *event, u64 event_id,
				     struct mausb_device *dev)
{
	struct completion	completion;
	struct mausb_completion mausb_completion;
	long status;
	unsigned long timeout;

	mausb_prepare_completion(&mausb_completion, &completion, event,
				 event_id);
	mausb_insert_event(dev, &mausb_completion);

	status = mausb_enqueue_event_to_user(dev, event);
	if (status < 0) {
		mausb_remove_event(dev, &mausb_completion);
		dev_err(mausb_host_dev.this_device, "Ring buffer full, event_id=%lld",
			event_id);
		return (int)status;
	}

	timeout = msecs_to_jiffies(MANAGEMENT_EVENT_TIMEOUT);
	status = wait_for_completion_interruptible_timeout(&completion,
							   timeout);

	mausb_remove_event(dev, &mausb_completion);

	if (status == 0) {
		queue_work(dev->workq, &dev->socket_disconnect_work);
		queue_work(dev->workq, &dev->hcd_disconnect_work);
		return -ETIMEDOUT;
	}

	return 0;
}

int mausb_usbdevhandle_event_to_user(struct mausb_device *dev,
				     u8 device_speed,
				     u32 route_string,
				     u16 hub_dev_handle,
				     u16 parent_hs_hub_dev_handle,
				     u16 parent_hs_hub_port, u16 mtt,
				     u8 lse, s32 *usb_dev_handle)
{
	struct mausb_event event;
	int status;
	u64 event_id = mausb_event_id(dev);

	event.type = MAUSB_EVENT_TYPE_USB_DEV_HANDLE;
	event.mgmt.dev_handle.device_speed	 = device_speed;
	event.mgmt.dev_handle.route_string	 = route_string;
	event.mgmt.dev_handle.hub_dev_handle	 = hub_dev_handle;
	event.mgmt.dev_handle.parent_hs_hub_port = parent_hs_hub_port;
	event.mgmt.dev_handle.mtt		 = mtt;
	event.mgmt.dev_handle.lse		 = lse;
	event.mgmt.dev_handle.event_id		 = event_id;
	event.madev_addr			 = dev->madev_addr;
	event.mgmt.dev_handle.parent_hs_hub_dev_handle =
						   parent_hs_hub_dev_handle;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "USBDevHandle failed, dev_handle=%#x, event_id=%lld, status=%d",
			 event.mgmt.dev_handle.dev_handle, event_id, status);
		return status;
	}

	if (event.status < 0)
		return event.status;

	*usb_dev_handle = event.mgmt.dev_handle.dev_handle;

	return 0;
}

int mausb_usbdevhandle_event_from_user(struct mausb_device *dev,
				       struct mausb_event *event)
{
	return mausb_signal_event(dev, event, event->mgmt.dev_handle.event_id);
}

int mausb_ephandle_event_to_user(struct mausb_device *dev,
				 u16 device_handle,
				 u16 descriptor_size, void *descriptor,
				 u16 *ep_handle)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type			     = MAUSB_EVENT_TYPE_EP_HANDLE;
	event.mgmt.ep_handle.device_handle   = device_handle;
	event.mgmt.ep_handle.descriptor_size = descriptor_size;
	event.mgmt.ep_handle.event_id	     = event_id;
	event.madev_addr		     = dev->madev_addr;

	memcpy(event.mgmt.ep_handle.descriptor, descriptor, descriptor_size);

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "EPHandle failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			 event.mgmt.ep_handle.ep_handle, device_handle,
			 event_id, status);
		return status;
	}

	if (event.status < 0)
		return event.status;

	*ep_handle = event.mgmt.ep_handle.ep_handle;

	return 0;
}

int mausb_ephandle_event_from_user(struct mausb_device *dev,
				   struct mausb_event *event)
{
	return mausb_signal_event(dev, event, event->mgmt.ep_handle.event_id);
}

int mausb_epactivate_event_to_user(struct mausb_device *dev,
				   u16 device_handle, u16 ep_handle)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type = MAUSB_EVENT_TYPE_EP_HANDLE_ACTIVATE;
	event.mgmt.ep_activate.device_handle = device_handle;
	event.mgmt.ep_activate.ep_handle     = ep_handle;
	event.mgmt.ep_activate.event_id	     = event_id;
	event.madev_addr		     = dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "Epactivate failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			event.mgmt.ep_activate.ep_handle, device_handle,
			event_id, status);
		return status;
	}

	return event.status;
}

int mausb_epactivate_event_from_user(struct mausb_device *dev,
				     struct mausb_event *event)
{
	return mausb_signal_event(dev, event,
		event->mgmt.ep_activate.event_id);
}

int mausb_epinactivate_event_to_user(struct mausb_device *dev,
				     u16 device_handle, u16 ep_handle)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type = MAUSB_EVENT_TYPE_EP_HANDLE_INACTIVATE;
	event.mgmt.ep_inactivate.device_handle	= device_handle;
	event.mgmt.ep_inactivate.ep_handle	= ep_handle;
	event.mgmt.ep_inactivate.event_id	= event_id;
	event.madev_addr			= dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "EPInactivate failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			event.mgmt.ep_inactivate.ep_handle, device_handle,
			event_id, status);
		return status;
	}

	return event.status;
}

int mausb_epinactivate_event_from_user(struct mausb_device *dev,
				       struct mausb_event *event)
{
	return mausb_signal_event(dev, event,
				  event->mgmt.ep_inactivate.event_id);
}

int mausb_epreset_event_to_user(struct mausb_device *dev,
				u16 device_handle, u16 ep_handle,
				u8 tsp_flag)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type			  = MAUSB_EVENT_TYPE_EP_HANDLE_RESET;
	event.mgmt.ep_reset.device_handle = device_handle;
	event.mgmt.ep_reset.ep_handle	  = ep_handle;
	event.mgmt.ep_reset.tsp		  = tsp_flag;
	event.mgmt.ep_reset.event_id	  = event_id;
	event.madev_addr		  = dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "EPReset failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			 event.mgmt.ep_reset.ep_handle, device_handle, event_id,
			 status);
		return status;
	}

	return event.status;
}

int mausb_epreset_event_from_user(struct mausb_device *dev,
				  struct mausb_event *event)
{
	return mausb_signal_event(dev, event, event->mgmt.ep_reset.event_id);
}

int mausb_epdelete_event_to_user(struct mausb_device *dev,
				 u16 device_handle, u16 ep_handle)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type			   = MAUSB_EVENT_TYPE_EP_HANDLE_DELETE;
	event.mgmt.ep_delete.device_handle = device_handle;
	event.mgmt.ep_delete.ep_handle	   = ep_handle;
	event.mgmt.ep_delete.event_id	   = event_id;
	event.madev_addr		   = dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "EPDelete failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			 event.mgmt.ep_delete.ep_handle, device_handle,
			 event_id, status);
		return status;
	}

	return event.status;
}

int mausb_epdelete_event_from_user(struct mausb_device *dev,
				   struct mausb_event *event)
{
	return mausb_signal_event(dev, event, event->mgmt.ep_delete.event_id);
}

int mausb_modifyep0_event_to_user(struct mausb_device *dev,
				  u16 device_handle, u16 *ep_handle,
				  __le16 max_packet_size)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type				= MAUSB_EVENT_TYPE_MODIFY_EP0;
	event.mgmt.modify_ep0.device_handle	= device_handle;
	event.mgmt.modify_ep0.ep_handle		= *ep_handle;
	event.mgmt.modify_ep0.max_packet_size	= max_packet_size;
	event.mgmt.modify_ep0.event_id		= event_id;
	event.madev_addr			= dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "ModifyEP0 failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			 event.mgmt.modify_ep0.ep_handle, device_handle,
			 event_id, status);
		return status;
	}

	if (event.status < 0)
		return event.status;

	*ep_handle = event.mgmt.modify_ep0.ep_handle;

	return 0;
}

int mausb_modifyep0_event_from_user(struct mausb_device *dev,
				    struct mausb_event *event)
{
	return mausb_signal_event(dev, event, event->mgmt.modify_ep0.event_id);
}

int mausb_setusbdevaddress_event_to_user(struct mausb_device *dev,
					 u16 device_handle,
					 u16 response_timeout)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type = MAUSB_EVENT_TYPE_SET_USB_DEV_ADDRESS;
	event.mgmt.set_usb_dev_address.device_handle	= device_handle;
	event.mgmt.set_usb_dev_address.response_timeout	= response_timeout;
	event.mgmt.set_usb_dev_address.event_id		= event_id;
	event.madev_addr				= dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "SetUSBDevAddress failed, dev_handle=%#x, event_id=%lld, status=%d",
			 device_handle, event_id, status);
		return status;
	}

	return event.status;
}

int mausb_setusbdevaddress_event_from_user(struct mausb_device *dev,
					   struct mausb_event *event)
{
	return mausb_signal_event(dev, event,
		event->mgmt.set_usb_dev_address.event_id);
}

static void
mausb_init_device_descriptor(struct ma_usb_updatedevreq_desc *update_descriptor,
			     struct usb_device_descriptor *device_descriptor)
{
	update_descriptor->usb20.bLength = device_descriptor->bLength;
	update_descriptor->usb20.bDescriptorType =
					device_descriptor->bDescriptorType;
	update_descriptor->usb20.bcdUSB = device_descriptor->bcdUSB;
	update_descriptor->usb20.bDeviceClass =
					device_descriptor->bDeviceClass;
	update_descriptor->usb20.bDeviceSubClass =
					device_descriptor->bDeviceSubClass;
	update_descriptor->usb20.bDeviceProtocol =
					device_descriptor->bDeviceProtocol;
	update_descriptor->usb20.bMaxPacketSize0 =
					device_descriptor->bMaxPacketSize0;
	update_descriptor->usb20.idVendor  = device_descriptor->idVendor;
	update_descriptor->usb20.idProduct = device_descriptor->idProduct;
	update_descriptor->usb20.bcdDevice = device_descriptor->bcdDevice;
	update_descriptor->usb20.iManufacturer =
					device_descriptor->iManufacturer;
	update_descriptor->usb20.iProduct  = device_descriptor->iProduct;
	update_descriptor->usb20.iSerialNumber =
					device_descriptor->iSerialNumber;
	update_descriptor->usb20.bNumConfigurations =
					device_descriptor->bNumConfigurations;
}

int mausb_updatedev_event_to_user(struct mausb_device *dev,
				  u16 device_handle,
				  u16 max_exit_latency, u8 hub,
				  u8 number_of_ports, u8 mtt,
				  u8 ttt, u8 integrated_hub_latency,
				  struct usb_device_descriptor *dev_descriptor)
{
	struct mausb_event event;
	int status;
	u64 event_id = mausb_event_id(dev);

	event.type = MAUSB_EVENT_TYPE_UPDATE_DEV;
	event.mgmt.update_dev.device_handle	     = device_handle;
	event.mgmt.update_dev.max_exit_latency	     = max_exit_latency;
	event.mgmt.update_dev.hub		     = hub;
	event.mgmt.update_dev.number_of_ports	     = number_of_ports;
	event.mgmt.update_dev.mtt		     = mtt;
	event.mgmt.update_dev.ttt		     = ttt;
	event.mgmt.update_dev.integrated_hub_latency = integrated_hub_latency;
	event.mgmt.update_dev.event_id		     = event_id;
	event.madev_addr			     = dev->madev_addr;

	mausb_init_device_descriptor(&event.mgmt.update_dev.update_descriptor,
				     dev_descriptor);

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "UpdateDev failed, dev_handle=%#x, event_id=%lld, status=%d",
			 device_handle, event_id, status);
		return status;
	}

	return event.status;
}

int mausb_updatedev_event_from_user(struct mausb_device *dev,
				    struct mausb_event *event)
{
	return mausb_signal_event(dev, event, event->mgmt.update_dev.event_id);
}

int mausb_usbdevdisconnect_event_to_user(struct mausb_device *dev,
					 u16 dev_handle)
{
	struct mausb_event event;
	int status;

	event.type = MAUSB_EVENT_TYPE_USB_DEV_DISCONNECT;
	event.mgmt.usb_dev_disconnect.device_handle = dev_handle;
	event.madev_addr			    = dev->madev_addr;

	status = mausb_enqueue_event_to_user(dev, &event);
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "USBDevDisconnect failed, dev_handle=%#x, status=%d",
			 dev_handle, status);

	return status;
}

int mausb_ping_event_to_user(struct mausb_device *dev)
{
	struct mausb_event event;
	int status;

	event.type	 = MAUSB_EVENT_TYPE_PING;
	event.madev_addr = dev->madev_addr;

	status = mausb_enqueue_event_to_user(dev, &event);
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "Ping failed, status=%d",
			 status);

	return status;
}

int mausb_devdisconnect_event_to_user(struct mausb_device *dev)
{
	struct mausb_event event;
	int status;
	u64 event_id = mausb_event_id(dev);

	event.type				= MAUSB_EVENT_TYPE_DEV_DISCONNECT;
	event.mgmt.dev_disconnect.event_id	= event_id;
	event.madev_addr			= dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "DevDisconnect failed, event_id=%lld, status=%d",
			 event_id, status);
		return status;
	}

	return event.status;
}

int mausb_devdisconnect_event_from_user(struct mausb_device *dev,
					struct mausb_event *event)
{
       return mausb_signal_event(dev, event,
				 event->mgmt.dev_disconnect.event_id);
}

int mausb_usbdevreset_event_to_user(struct mausb_device *dev,
				    u16 device_handle)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type			       = MAUSB_EVENT_TYPE_USB_DEV_RESET;
	event.mgmt.usb_dev_reset.device_handle = device_handle;
	event.mgmt.usb_dev_reset.event_id      = event_id;
	event.madev_addr		       = dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "UsbDevReset failed, dev_handle=%#x, event_id=%lld, status=%d",
			 device_handle, event_id, status);
		return status;
	}

	return event.status;
}

int mausb_usbdevreset_event_from_user(struct mausb_device *dev,
				      struct mausb_event *event)
{
	return mausb_signal_event(dev, event,
				  event->mgmt.usb_dev_reset.event_id);
}

int mausb_canceltransfer_event_to_user(struct mausb_device *dev,
				       u16 device_handle, u16 ep_handle,
				       uintptr_t urb)
{
	struct mausb_event event;
	int status;

	event.type = MAUSB_EVENT_TYPE_CANCEL_TRANSFER;
	event.mgmt.cancel_transfer.device_handle = device_handle;
	event.mgmt.cancel_transfer.ep_handle	 = ep_handle;
	event.mgmt.cancel_transfer.urb		 = urb;
	event.madev_addr			 = dev->madev_addr;

	status = mausb_enqueue_event_to_user(dev, &event);
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "CancelTransfer failed, ep_handle=%#x, dev_handle=%#x, status=%d",
			 event.mgmt.modify_ep0.ep_handle, device_handle,
			 status);
	return status;
}

int mausb_canceltransfer_event_from_user(struct mausb_device *dev,
					 struct mausb_event *event)
{
	return 0;
}

int mausb_cleartransfers_event_to_user(struct mausb_device *dev,
				       u16 device_handle, u16 ep_handle)
{
	struct mausb_event event;
	int  status;
	u64 event_id = mausb_event_id(dev);

	event.type = MAUSB_EVENT_TYPE_CLEAR_TRANSFERS;
	event.mgmt.clear_transfers.device_handle	= device_handle;
	event.mgmt.clear_transfers.ep_handle		= ep_handle;
	event.mgmt.clear_transfers.event_id		= event_id;
	event.madev_addr				= dev->madev_addr;

	status = mausb_wait_for_completion(&event, event_id, dev);
	if (status < 0) {
		dev_warn(mausb_host_dev.this_device, "ClearTransfers failed, ep_handle=%#x, dev_handle=%#x, event_id=%lld, status=%d",
			 event.mgmt.clear_transfers.ep_handle, device_handle,
			 event_id, status);
		return status;
	}

	return event.status;
}

int mausb_cleartransfers_event_from_user(struct mausb_device *dev,
					 struct mausb_event *event)
{
	return mausb_signal_event(dev, event,
				  event->mgmt.clear_transfers.event_id);
}

void mausb_cleanup_send_data_msg_event(struct mausb_event *event)
{
	mausb_complete_urb(event);
}

void mausb_cleanup_received_data_msg_event(struct mausb_event *event)
{
	mausb_release_event_resources(event);
}

void mausb_cleanup_delete_data_transfer_event(struct mausb_event *event)
{
	struct urb *urb = (struct urb *)event->data.urb;
	struct mausb_urb_ctx *urb_ctx;
	int status = 0;

	urb_ctx = mausb_unlink_and_delete_urb_from_tree(urb, status);
	if (!urb_ctx) {
		dev_warn(mausb_host_dev.this_device, "Urb=%p is not in tree",
			 urb);
		return;
	}

	/* Deallocate urb_ctx */
	mausb_uninit_data_iterator(&urb_ctx->iterator);
	kfree(urb_ctx);

	urb->status = -EPROTO;
	urb->actual_length = 0;
	atomic_dec(&urb->use_count);
	usb_hcd_giveback_urb(urb->hcpriv, urb, urb->status);
}
