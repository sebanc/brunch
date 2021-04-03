// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#include "hpal.h"

#include <linux/circ_buf.h>
#include <net/net_namespace.h>

#include "hcd.h"
#include "hpal_data.h"
#include "utils.h"

#define MAUSB_DELETE_MADEV_TIMEOUT_MS 3000

struct mss mss;

static int mausb_start_connection_timer(struct mausb_device *dev);
static int mausb_power_state_cb(struct notifier_block *nb, unsigned long action,
				void *data);
static void mausb_signal_empty_mss(void);
static void mausb_remove_madev_from_list(u8 madev_addr);
static void mausb_execute_urb_dequeue(struct work_struct *dequeue_work);
static int mausb_start_heartbeat_timer(void);

static inline struct mausb_urb_ctx *__mausb_find_urb_in_tree(struct urb *urb)
{
	struct rb_node *node = mhcd->mausb_urbs.rb_node;

	while (node) {
		struct mausb_urb_ctx *urb_ctx =
		    rb_entry(node, struct mausb_urb_ctx, rb_node);

		if (urb < urb_ctx->urb)
			node = urb_ctx->rb_node.rb_left;
		else if (urb > urb_ctx->urb)
			node = urb_ctx->rb_node.rb_right;
		else
			return urb_ctx;
	}
	return NULL;
}

struct mausb_urb_ctx *mausb_find_urb_in_tree(struct urb *urb)
{
	unsigned long flags;
	struct mausb_urb_ctx *urb_ctx;

	spin_lock_irqsave(&mhcd->lock, flags);
	urb_ctx =  __mausb_find_urb_in_tree(urb);
	spin_unlock_irqrestore(&mhcd->lock, flags);

	return urb_ctx;
}

static int mausb_insert_urb_ctx_in_tree(struct mausb_urb_ctx *urb_ctx)
{
	struct rb_node **new_node = &mhcd->mausb_urbs.rb_node;
	struct rb_node *parent = NULL;
	struct mausb_urb_ctx *current_urb = NULL;

	while (*new_node) {
		parent = *new_node;
		current_urb = rb_entry(*new_node, struct mausb_urb_ctx,
				       rb_node);

		if (urb_ctx->urb < current_urb->urb)
			new_node = &((*new_node)->rb_left);
		else if (urb_ctx->urb > current_urb->urb)
			new_node = &((*new_node)->rb_right);
		else
			return -EEXIST;
	}
	rb_link_node(&urb_ctx->rb_node, parent, new_node);
	rb_insert_color(&urb_ctx->rb_node, &mhcd->mausb_urbs);
	return 0;
}

static void mausb_delete_urb_ctx_from_tree(struct mausb_urb_ctx *urb_ctx)
{
	rb_erase(&urb_ctx->rb_node, &mhcd->mausb_urbs);
}

static struct mausb_urb_ctx *mausb_create_urb_ctx(struct urb *urb, int *status)
{
	struct mausb_urb_ctx *urb_ctx = NULL;

	if (!urb) {
		dev_err(mausb_host_dev.this_device, "Urb is NULL");
		*status = -EINVAL;
		return NULL;
	}

	urb_ctx = kzalloc(sizeof(*urb_ctx), GFP_ATOMIC);
	if (!urb_ctx) {
		*status = -ENOMEM;
		return NULL;
	}

	urb_ctx->urb = urb;
	INIT_WORK(&urb_ctx->work, mausb_execute_urb_dequeue);

	return urb_ctx;
}

int mausb_insert_urb_in_tree(struct urb *urb, bool link_urb_to_ep)
{
	unsigned long flags;
	int status = 0;

	struct mausb_urb_ctx *urb_ctx = mausb_create_urb_ctx(urb, &status);

	if (!urb_ctx)
		return status;

	spin_lock_irqsave(&mhcd->lock, flags);

	if (link_urb_to_ep) {
		status = usb_hcd_link_urb_to_ep(urb->hcpriv, urb);
		if (status) {
			spin_unlock_irqrestore(&mhcd->lock, flags);
			dev_err(mausb_host_dev.this_device, "Error %d while linking urb to hcd_endpoint",
				status);
			kfree(urb_ctx);
			return status;
		}
	}

	if (mausb_insert_urb_ctx_in_tree(urb_ctx)) {
		kfree(urb_ctx);
		if (link_urb_to_ep)
			usb_hcd_unlink_urb_from_ep(urb->hcpriv, urb);
		spin_unlock_irqrestore(&mhcd->lock, flags);
		dev_err(mausb_host_dev.this_device, "Urb_ctx insertion failed");
		return -EEXIST;
	}

	mausb_init_data_iterator(&urb_ctx->iterator, urb->transfer_buffer,
				 urb->transfer_buffer_length, urb->sg,
				 (unsigned int)urb->num_sgs,
				 usb_urb_dir_in(urb));

	spin_unlock_irqrestore(&mhcd->lock, flags);

	return 0;
}

static bool mausb_return_urb_ctx_to_tree(struct mausb_urb_ctx *urb_ctx,
					 bool link_urb_to_ep)
{
	unsigned long flags;
	int status;

	if (!urb_ctx)
		return false;

	spin_lock_irqsave(&mhcd->lock, flags);
	if (link_urb_to_ep) {
		status = usb_hcd_link_urb_to_ep(urb_ctx->urb->hcpriv,
						urb_ctx->urb);
		if (status) {
			spin_unlock_irqrestore(&mhcd->lock, flags);
			dev_err(mausb_host_dev.this_device, "Error %d while linking urb to hcd_endpoint",
				status);
			return false;
		}
	}

	if (mausb_insert_urb_ctx_in_tree(urb_ctx)) {
		if (link_urb_to_ep)
			usb_hcd_unlink_urb_from_ep(urb_ctx->urb->hcpriv,
						   urb_ctx->urb);
		spin_unlock_irqrestore(&mhcd->lock, flags);
		dev_err(mausb_host_dev.this_device, "Urb_ctx insertion failed");
		return false;
	}

	spin_unlock_irqrestore(&mhcd->lock, flags);

	return true;
}

static void mausb_complete_urbs_from_tree(void)
{
	struct mausb_urb_ctx *urb_ctx = NULL;
	struct urb	     *current_urb = NULL;
	struct rb_node	     *current_node = NULL;
	unsigned long flags;
	int status = 0;
	int ret;

	dev_dbg(mausb_host_dev.this_device, "Completing all urbs from tree");

	spin_lock_irqsave(&mhcd->lock, flags);

	while ((current_node = rb_first(&mhcd->mausb_urbs))) {
		urb_ctx = rb_entry(current_node, struct mausb_urb_ctx, rb_node);

		current_urb = urb_ctx->urb;
		mausb_delete_urb_ctx_from_tree(urb_ctx);
		mausb_uninit_data_iterator(&urb_ctx->iterator);
		kfree(urb_ctx);

		ret = usb_hcd_check_unlink_urb(current_urb->hcpriv,
					       current_urb, status);
		if (ret == -EIDRM)
			dev_warn(mausb_host_dev.this_device, "Urb=%p is already unlinked",
				 current_urb);
		else
			usb_hcd_unlink_urb_from_ep(current_urb->hcpriv,
						   current_urb);

		spin_unlock_irqrestore(&mhcd->lock, flags);

		/* Prepare urb for completion */
		dev_dbg(mausb_host_dev.this_device, "Completing urb=%p",
			current_urb);

		current_urb->status	   = -EPROTO;
		current_urb->actual_length = 0;
		atomic_dec(&current_urb->use_count);
		usb_hcd_giveback_urb(current_urb->hcpriv, current_urb,
				     current_urb->status);

		spin_lock_irqsave(&mhcd->lock, flags);
	}

	spin_unlock_irqrestore(&mhcd->lock, flags);

	dev_dbg(mausb_host_dev.this_device, "Completed all urbs from tree");
}

/*After this function call only valid thing to do with urb is to give it back*/
struct mausb_urb_ctx *mausb_unlink_and_delete_urb_from_tree(struct urb *urb,
							    int status)
{
	struct mausb_urb_ctx *urb_ctx = NULL;
	unsigned long flags;
	int ret;

	if (!urb) {
		dev_warn(mausb_host_dev.this_device, "URB is NULL");
		return NULL;
	}

	spin_lock_irqsave(&mhcd->lock, flags);

	urb_ctx = __mausb_find_urb_in_tree(urb);
	if (!urb_ctx) {
		dev_warn(mausb_host_dev.this_device, "Urb=%p not in tree", urb);
		spin_unlock_irqrestore(&mhcd->lock, flags);
		return NULL;
	}

	ret = usb_hcd_check_unlink_urb(urb->hcpriv, urb, status);
	if (ret == -EIDRM)
		dev_warn(&urb->dev->dev, "Urb=%p is already unlinked", urb);
	else
		usb_hcd_unlink_urb_from_ep(urb->hcpriv, urb);

	mausb_delete_urb_ctx_from_tree(urb_ctx);

	spin_unlock_irqrestore(&mhcd->lock, flags);

	dev_vdbg(&urb->dev->dev, "Urb=%p is removed from tree", urb);

	return urb_ctx;
}

void mausb_release_event_resources(struct mausb_event *event)
{
	struct ma_usb_hdr_common *receive_buffer = (struct ma_usb_hdr_common *)
						    event->data.recv_buf;

	kfree(receive_buffer);
}

static void mausb_iterator_reset(struct mausb_device *dev,
				 struct mausb_event *event)
{
	struct urb	     *urb = (struct urb *)event->data.urb;
	struct mausb_urb_ctx *urb_ctx;

	urb_ctx = mausb_find_urb_in_tree(urb);

	if (urb_ctx)
		mausb_reset_data_iterator(&urb_ctx->iterator);
}

static void mausb_iterator_seek(struct mausb_device *dev,
				struct mausb_event *event)
{
	struct urb	     *urb = (struct urb *)event->data.urb;
	struct mausb_urb_ctx *urb_ctx;

	urb_ctx = mausb_find_urb_in_tree(urb);

	if (urb_ctx)
		mausb_data_iterator_seek(&urb_ctx->iterator,
					 event->data.iterator_seek_delta);
}

void mausb_complete_urb(struct mausb_event *event)
{
	struct urb *urb = (struct urb *)event->data.urb;

	dev_vdbg(mausb_host_dev.this_device, "URB complete request, transfer_size=%d, rem_transfer_size=%d, status=%d",
		 event->data.transfer_size, event->data.rem_transfer_size,
		 event->status);
	mausb_complete_request(urb,
			       event->data.transfer_size -
			       event->data.rem_transfer_size,
			       event->status);
}

static void mausb_delete_ma_dev(struct mausb_device *dev,
				struct mausb_event *event)
{
	mausb_signal_event(dev, event, event->mgmt.delete_ma_dev.event_id);
}

static void mausb_process_user_finished(struct mausb_device *dev,
					struct mausb_event *event)
{
	complete(&dev->user_finished_event);
}

static int mausb_send_mgmt_msg(struct mausb_device *dev,
			       struct mausb_event *event)
{
	struct mausb_kvec_data_wrapper wrapper;
	struct kvec kvec;
	struct ma_usb_hdr_common *hdr;
	int status;

	hdr = (struct ma_usb_hdr_common *)event->mgmt.mgmt_hdr.hdr;

	dev_vdbg(mausb_host_dev.this_device, "Sending event=%d, type=%d",
		 event->type, hdr->type);

	kvec.iov_base	 = hdr;
	kvec.iov_len	 = hdr->length;
	wrapper.kvec	 = &kvec;
	wrapper.kvec_num = 1;
	wrapper.length	 = hdr->length;

	status = mausb_ip_send(dev->mgmt_channel, &wrapper);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Send failed. Disconnecting... status=%d",
			status);
		queue_work(dev->workq, &dev->socket_disconnect_work);
		queue_work(dev->workq, &dev->hcd_disconnect_work);
	}

	return status;
}

static int mausb_get_first_free_port_number(u8 *port_number)
{
	(*port_number) = 0;
	while ((mhcd->connected_ports & (1 << *port_number)) != 0 &&
	       *port_number < NUMBER_OF_PORTS)
		++(*port_number);

	if (*port_number == NUMBER_OF_PORTS)
		return -EINVAL;

	mhcd->connected_ports |= (1 << *port_number);

	return 0;
}

static inline void mausb_port_has_changed_event(struct mausb_device *dev,
						struct mausb_event *event)
{
	int status;
	u8 port_number;
	unsigned long flags;

	spin_lock_irqsave(&mhcd->lock, flags);

	status = mausb_get_first_free_port_number(&port_number);
	if (status < 0) {
		spin_unlock_irqrestore(&mhcd->lock, flags);
		dev_err(mausb_host_dev.this_device, "There is no free port, schedule delete ma_dev");
		queue_work(dev->workq, &dev->socket_disconnect_work);
		return;
	}

	spin_unlock_irqrestore(&mhcd->lock, flags);

	dev->dev_type	   = event->port_changed.dev_type;
	dev->dev_speed	   = event->port_changed.dev_speed;
	dev->lse	   = event->port_changed.lse;
	dev->dev_connected = 1;
	dev->port_number   = port_number;

	mausb_port_has_changed(event->port_changed.dev_type,
			       event->port_changed.dev_speed, dev);

	if ((enum mausb_device_type)event->port_changed.dev_type == USB30HUB)
		mausb_port_has_changed(USB20HUB, HIGH_SPEED, dev);
}

static void mausb_complete_timeout_event(struct mausb_device *dev,
					 struct mausb_event *event)
{
	dev_vdbg(mausb_host_dev.this_device, "Event type=%d, event_id=%llu",
		 event->type, event->mgmt.mgmt_req_timedout.event_id);
	mausb_signal_event(dev, event, event->mgmt.mgmt_req_timedout.event_id);
}

static void mausb_process_event(struct mausb_device *dev,
				struct mausb_event *event)
{
	dev_vdbg(mausb_host_dev.this_device, "Process event of type=%d",
		 event->type);

	switch (event->type) {
	case MAUSB_EVENT_TYPE_USB_DEV_HANDLE:
		mausb_usbdevhandle_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_EP_HANDLE:
		mausb_ephandle_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_EP_HANDLE_ACTIVATE:
		mausb_epactivate_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_EP_HANDLE_INACTIVATE:
		mausb_epinactivate_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_EP_HANDLE_RESET:
		mausb_epreset_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_EP_HANDLE_DELETE:
		mausb_epdelete_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_MODIFY_EP0:
		mausb_modifyep0_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_SET_USB_DEV_ADDRESS:
		mausb_setusbdevaddress_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_UPDATE_DEV:
		mausb_updatedev_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_USB_DEV_RESET:
		mausb_usbdevreset_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_CANCEL_TRANSFER:
		mausb_canceltransfer_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_CLEAR_TRANSFERS:
		mausb_cleartransfers_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_PORT_CHANGED:
		mausb_port_has_changed_event(dev, event);
		break;
	case MAUSB_EVENT_TYPE_DEV_DISCONNECT:
		mausb_devdisconnect_event_from_user(dev, event);
		break;
	case MAUSB_EVENT_TYPE_PING:
	case MAUSB_EVENT_TYPE_SEND_MGMT_MSG:
		mausb_send_mgmt_msg(dev, event);
		break;
	case MAUSB_EVENT_TYPE_SEND_DATA_MSG:
		mausb_send_data_msg(dev, event);
		break;
	case MAUSB_EVENT_TYPE_RECEIVED_DATA_MSG:
		mausb_receive_data_msg(dev, event);
		break;
	case MAUSB_EVENT_TYPE_URB_COMPLETE:
		mausb_complete_urb(event);
		break;
	case MAUSB_EVENT_TYPE_SEND_ACK:
		mausb_send_transfer_ack(dev, event);
		mausb_release_event_resources(event);
		break;
	case MAUSB_EVENT_TYPE_ITERATOR_RESET:
		mausb_iterator_reset(dev, event);
		break;
	case MAUSB_EVENT_TYPE_ITERATOR_SEEK:
		mausb_iterator_seek(dev, event);
		break;
	case MAUSB_EVENT_TYPE_DELETE_MA_DEV:
		mausb_delete_ma_dev(dev, event);
		break;
	case MAUSB_EVENT_TYPE_USER_FINISHED:
		mausb_process_user_finished(dev, event);
		break;
	case MAUSB_EVENT_TYPE_RELEASE_EVENT_RESOURCES:
		mausb_release_event_resources(event);
		break;
	case MAUSB_EVENT_TYPE_NONE:
		mausb_release_event_resources(event);
		break;
	case MAUSB_EVENT_TYPE_MGMT_REQUEST_TIMED_OUT:
		mausb_complete_timeout_event(dev, event);
		break;
	default:
		break;
	}

	mausb_notify_completed_user_events(dev->ring_buffer);
}

static void mausb_hpal_kernel_work(struct work_struct *work)
{
	struct mausb_device *dev = container_of(work, struct mausb_device,
						work);
	struct mausb_event *event;
	int status;
	u16 i;
	u16 events;
	u16 completed_events;
	unsigned long flags;
	struct mausb_ring_buffer *dev_mausb_ring = dev->ring_buffer;

	spin_lock_irqsave(&dev->num_of_user_events_lock, flags);
	events = dev->num_of_user_events;
	completed_events = dev->num_of_completed_events;
	dev->num_of_user_events = 0;
	dev->num_of_completed_events = 0;
	spin_unlock_irqrestore(&dev->num_of_user_events_lock, flags);

	status = mausb_ring_buffer_move_tail(dev_mausb_ring, completed_events);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Dequeue failed, status=%d",
			status);
		kref_put(&dev->refcount, mausb_release_ma_dev_async);
		return;
	}

	for (i = 0; i < events; ++i) {
		event = mausb_ring_current_from_user(dev_mausb_ring);
		mausb_ring_next_from_user(dev_mausb_ring);
		mausb_process_event(dev, event);
	}
}

static void mausb_socket_disconnect_event(struct work_struct *work)
{
	struct mausb_device *dev = container_of(work, struct mausb_device,
						socket_disconnect_work);
	struct mausb_event event;
	int status;

	dev_info(mausb_host_dev.this_device, "Disconnect madev_addr=%d",
		 dev->madev_addr);

	mausb_ip_disconnect(dev->ctrl_channel);
	mausb_destroy_ip_ctx(dev->ctrl_channel);
	dev->ctrl_channel = NULL;

	mausb_ip_disconnect(dev->bulk_channel);
	mausb_destroy_ip_ctx(dev->bulk_channel);
	dev->bulk_channel = NULL;

	mausb_ip_disconnect(dev->isoch_channel);
	mausb_destroy_ip_ctx(dev->isoch_channel);
	dev->isoch_channel = NULL;

	if (dev->mgmt_channel) {
		memset(&event, 0, sizeof(event));
		event.type = MAUSB_EVENT_TYPE_NETWORK_DISCONNECTED;
		event.data.device_id = dev->id;

		status = mausb_enqueue_event_to_user(dev, &event);

		dev_info(mausb_host_dev.this_device, "Network disconnected notification sent status=%d",
			 status);

		dev_info(mausb_host_dev.this_device, "Releasing MAUSB device ref");
		kref_put(&dev->refcount, mausb_release_ma_dev_async);
	}

	mausb_ip_disconnect(dev->mgmt_channel);
	mausb_destroy_ip_ctx(dev->mgmt_channel);
	dev->mgmt_channel = NULL;

	memset(dev->channel_map, 0, sizeof(dev->channel_map));
}

static void mausb_disconnect_ma_dev(struct mausb_device *dev)
{
	dev_info(mausb_host_dev.this_device, "Disconnecting MAUSB device madev_addr=%d",
		 dev->madev_addr);

	if (!dev->dev_connected) {
		dev_warn(mausb_host_dev.this_device, "MAUSB device is not connected");
		kref_put(&dev->refcount, mausb_release_ma_dev_async);
		return;
	}
	mausb_hcd_disconnect(dev->port_number, dev->dev_type, dev->dev_speed);

	if (dev->dev_type == USB30HUB)
		mausb_hcd_disconnect(dev->port_number, USB20HUB, HIGH_SPEED);
}

static void mausb_hcd_disconnect_event(struct work_struct *work)
{
	struct mausb_device *ma_dev = container_of(work, struct mausb_device,
						   hcd_disconnect_work);

	mausb_disconnect_ma_dev(ma_dev);
}

static void mausb_delete_madev(struct work_struct *work)
{
	struct mausb_device *dev = container_of(work, struct mausb_device,
						madev_delete_work);
	struct mausb_event	event;
	struct completion	completion;
	struct completion	*user_event;
	struct mausb_completion mausb_completion;
	long status;
	unsigned long timeout = msecs_to_jiffies(MAUSB_DELETE_MADEV_TIMEOUT_MS);

	dev_info(mausb_host_dev.this_device, "Deleting MAUSB device madev_addr=%d",
		 dev->madev_addr);

	del_timer_sync(&dev->connection_timer);

	/* Client IS responsive */
	if (!atomic_read(&dev->unresponsive_client)) {
		memset(&event, 0, sizeof(event));
		event.type = MAUSB_EVENT_TYPE_DELETE_MA_DEV;
		event.mgmt.delete_ma_dev.device_id = dev->id;
		event.mgmt.delete_ma_dev.event_id  = mausb_event_id(dev);

		init_completion(&completion);
		mausb_completion.completion_event = &completion;
		mausb_completion.event_id = event.mgmt.delete_ma_dev.event_id;
		mausb_completion.mausb_event = &event;

		mausb_insert_event(dev, &mausb_completion);

		status = mausb_enqueue_event_to_user(dev, &event);
		if (status < 0) {
			mausb_remove_event(dev, &mausb_completion);
			dev_err(mausb_host_dev.this_device, "Ring buffer full, enqueue failed");
			return;
		}

		dev_dbg(mausb_host_dev.this_device, "Deleting MAUSB device...");

		status = wait_for_completion_interruptible_timeout(&completion,
								   timeout);

		dev_dbg(mausb_host_dev.this_device, "Deleting MAUSB device event finished with %ld",
			status);

		mausb_remove_event(dev, &mausb_completion);

		user_event = &dev->user_finished_event;

		status = wait_for_completion_interruptible_timeout(user_event,
								   timeout);
		dev_info(mausb_host_dev.this_device, "User event finished with %ld",
			 status);
	}

	flush_workqueue(dev->workq);
	destroy_workqueue(dev->workq);

	mausb_clear_hcd_madev(dev->port_number);

	mausb_ring_buffer_cleanup(dev->ring_buffer);
	mausb_ring_buffer_destroy(dev->ring_buffer);

	mausb_remove_madev_from_list(dev->madev_addr);

	put_net(dev->net_ns);

	kfree(dev->ring_buffer);
	kfree(dev);
	mausb_signal_empty_mss();

	dev_dbg(mausb_host_dev.this_device, "MAUSB device deleted.");
}

static void mausb_ping_work(struct work_struct *work)
{
	struct mausb_device *dev = container_of(work, struct mausb_device,
						ping_work);

	if (mausb_start_connection_timer(dev) < 0) {
		dev_err(mausb_host_dev.this_device, "Session timeout - disconnnecting device madev_addr=%d",
			dev->madev_addr);
		queue_work(dev->workq, &dev->socket_disconnect_work);
		queue_work(dev->workq, &dev->hcd_disconnect_work);
		return;
	}

	mausb_ping_event_to_user(dev);
}

static void mausb_heartbeat_work(struct work_struct *work)
{
	struct mausb_device *dev = container_of(work, struct mausb_device,
						heartbeat_work);

	dev_err(mausb_host_dev.this_device, "App is unresponsive - disconnect device");
	atomic_set(&dev->unresponsive_client, 1);
	mausb_complete_urbs_from_tree();
	queue_work(dev->workq, &dev->socket_disconnect_work);
	queue_work(dev->workq, &dev->hcd_disconnect_work);
}

static void mausb_connection_timer_func(unsigned long data)
{
	struct mausb_device *dev = (struct mausb_device *)data;

	queue_work(dev->workq, &dev->ping_work);
}

static void mausb_heartbeat_timer_func(unsigned long data)
{
	unsigned long flags;
	struct mausb_device *dev = NULL;

	if (mausb_start_heartbeat_timer() < 0) {
		dev_err(mausb_host_dev.this_device, "App is unresponsive - disconnecting devices");
		spin_lock_irqsave(&mss.lock, flags);

		/* Reset connected clients */
		mss.client_connected = false;
		mss.missed_heartbeats = 0;

		list_for_each_entry(dev, &mss.madev_list, list_entry) {
			dev_vdbg(mausb_host_dev.this_device, "Enqueue heartbeat_work madev_addr=%x",
				 dev->madev_addr);
			queue_work(dev->workq, &dev->heartbeat_work);
		}

		complete(&mss.client_stopped);
		spin_unlock_irqrestore(&mss.lock, flags);
	}
}

static struct mausb_device *
mausb_create_madev(struct mausb_device_address dev_addr, u8 madev_address,
		   int *status)
{
	struct mausb_device *dev;
	unsigned long flags;
	char workq_name[16];
	struct workqueue_struct *workq;

	memset(workq_name, 0, sizeof(workq_name));
	sprintf(workq_name, "%x", madev_address);
	strcat(workq_name, "_madev_workq");

	dev_vdbg(mausb_host_dev.this_device, "madev_workq_name = %s",
		 workq_name);

	workq = alloc_ordered_workqueue(workq_name, WQ_MEM_RECLAIM);
	if (!workq) {
		*status = -ENOMEM;
		return NULL;
	}

	spin_lock_irqsave(&mss.lock, flags);

	if (mss.deinit_in_progress) {
		spin_unlock_irqrestore(&mss.lock, flags);
		dev_alert(mausb_host_dev.this_device, "Device creating failed - mss deinit in progress");
		flush_workqueue(workq);
		destroy_workqueue(workq);
		*status = -ESHUTDOWN;
		return NULL;
	}

	dev = mausb_get_dev_from_addr_unsafe(madev_address);
	if (dev) {
		spin_unlock_irqrestore(&mss.lock, flags);
		dev_warn(mausb_host_dev.this_device, "MAUSB device already connected, madev_address=%x",
			 madev_address);
		flush_workqueue(workq);
		destroy_workqueue(workq);
		*status = -EEXIST;
		return NULL;
	}

	dev = kzalloc(sizeof(*dev), GFP_ATOMIC);

	if (!dev) {
		spin_unlock_irqrestore(&mss.lock, flags);
		dev_alert(mausb_host_dev.this_device, "Could not allocate MAUSB device!");
		flush_workqueue(workq);
		destroy_workqueue(workq);
		*status = -ENOMEM;
		return NULL;
	}

	dev_dbg(mausb_host_dev.this_device, "Create MAUSB device.");

	dev->workq = workq;

	INIT_WORK(&dev->work, mausb_hpal_kernel_work);
	INIT_WORK(&dev->socket_disconnect_work, mausb_socket_disconnect_event);
	INIT_WORK(&dev->hcd_disconnect_work, mausb_hcd_disconnect_event);
	INIT_WORK(&dev->madev_delete_work, mausb_delete_madev);
	INIT_WORK(&dev->ping_work, mausb_ping_work);
	INIT_WORK(&dev->heartbeat_work, mausb_heartbeat_work);

	kref_init(&dev->refcount);

	dev->event_id = 0;
	spin_lock_init(&dev->event_id_lock);

	INIT_LIST_HEAD(&dev->completion_events);
	spin_lock_init(&dev->completion_events_lock);
	spin_lock_init(&dev->num_of_user_events_lock);
	spin_lock_init(&dev->connection_timer_lock);

	init_completion(&dev->user_finished_event);
	atomic_set(&dev->unresponsive_client, 0);

	setup_timer(&dev->connection_timer, mausb_connection_timer_func,
		    (unsigned long)dev);

	dev->dev_addr = dev_addr;
	dev->madev_addr = madev_address;
	dev->net_ns = get_net(current->nsproxy->net_ns);

	if (!list_empty(&mss.available_ring_buffers)) {
		dev->ring_buffer = container_of(mss.available_ring_buffers.next,
						struct mausb_ring_buffer,
						list_entry);
		list_del(mss.available_ring_buffers.next);
	} else {
		dev_alert(mausb_host_dev.this_device, "Ring buffer for mausb device is not available!");
	}

	list_add_tail(&dev->list_entry, &mss.madev_list);

	reinit_completion(&mss.empty);

	spin_unlock_irqrestore(&mss.lock, flags);

	return dev;
}

void mausb_release_ma_dev_async(struct kref *kref)
{
	struct mausb_device *dev = container_of(kref, struct mausb_device,
						refcount);

	dev_info(mausb_host_dev.this_device, "Scheduling work for MAUSB device to be deleted");

	schedule_work(&dev->madev_delete_work);
}

int mausb_initiate_dev_connection(struct mausb_device_address dev_addr,
				  u8 madev_address)
{
	int error = 0;
	struct mausb_device *dev;
	unsigned long flags;

	spin_lock_irqsave(&mss.lock, flags);
	dev = mausb_get_dev_from_addr_unsafe(madev_address);
	spin_unlock_irqrestore(&mss.lock, flags);

	if (dev) {
		dev_warn(mausb_host_dev.this_device, "MAUSB device already connected, madev_address=%x",
			 madev_address);
		return -EEXIST;
	}

	dev = mausb_create_madev(dev_addr, madev_address, &error);
	if (!dev)
		return error;

	dev_info(mausb_host_dev.this_device, "New MAUSB device created madev_addr=%d",
		 madev_address);

	error = mausb_init_ip_ctx(&dev->mgmt_channel, dev->net_ns,
				  dev->dev_addr.ip.address,
				  dev->dev_addr.ip.port.management, dev,
				  mausb_ip_callback, MAUSB_MGMT_CHANNEL);
	if (error) {
		dev_err(mausb_host_dev.this_device, "Mgmt ip context init failed: error=%d",
			error);
		kref_put(&dev->refcount, mausb_release_ma_dev_async);
		return error;
	}

	mausb_ip_connect_async(dev->mgmt_channel);

	return 0;
}

void mausb_on_madev_connected(struct mausb_device *dev)
{
	struct mausb_event mausb_event;

	mausb_dev_reset_req_event(&mausb_event);
	mausb_enqueue_event_to_user(dev, &mausb_event);
}

int mausb_enqueue_event_from_user(u8 madev_addr, u16 num_of_events,
				  u16 num_of_completed)
{
	unsigned long flags;
	struct mausb_device *dev;

	spin_lock_irqsave(&mss.lock, flags);
	dev = mausb_get_dev_from_addr_unsafe(madev_addr);

	if (!dev) {
		spin_unlock_irqrestore(&mss.lock, flags);
		return -EINVAL;
	}

	spin_lock(&dev->num_of_user_events_lock);
	dev->num_of_user_events += num_of_events;
	dev->num_of_completed_events += num_of_completed;
	spin_unlock(&dev->num_of_user_events_lock);
	queue_work(dev->workq, &dev->work);
	spin_unlock_irqrestore(&mss.lock, flags);

	return 0;
}

int mausb_enqueue_event_to_user(struct mausb_device *dev,
				struct mausb_event *event)
{
	int status;

	event->madev_addr = dev->madev_addr;
	status = mausb_ring_buffer_put(dev->ring_buffer, event);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Ring buffer operation failed");
		mausb_cleanup_ring_buffer_event(event);
		return status;
	}

	mausb_notify_ring_events(dev->ring_buffer);
	dev_vdbg(mausb_host_dev.this_device, "User-space notification sent.");

	return 0;
}

int mausb_data_req_enqueue_event(struct mausb_device *dev, u16 ep_handle,
				 struct urb *request)
{
	int status;
	struct mausb_event mausb_event;

	mausb_event.type   = MAUSB_EVENT_TYPE_SEND_DATA_MSG;
	mausb_event.status = 0;

	mausb_event.data.transfer_type =
		mausb_transfer_type_from_usb(&request->ep->desc);
	mausb_event.data.device_id	= dev->id;
	mausb_event.data.ep_handle	= ep_handle;
	mausb_event.data.urb		= (uintptr_t)request;
	mausb_event.data.setup_packet	=
		(usb_endpoint_xfer_control(&request->ep->desc) &&
			request->setup_packet);
	mausb_event.data.transfer_size	= request->transfer_buffer_length;
	mausb_event.data.direction	= (usb_urb_dir_in(request) ?
						MAUSB_DATA_MSG_DIRECTION_IN :
						MAUSB_DATA_MSG_DIRECTION_OUT);
	mausb_event.data.transfer_size +=
		((mausb_event.data.direction == MAUSB_DATA_MSG_DIRECTION_OUT &&
			mausb_event.data.setup_packet) ?
				MAUSB_CONTROL_SETUP_SIZE : 0);
	mausb_event.data.rem_transfer_size = mausb_event.data.transfer_size;
	mausb_event.data.transfer_flags	   = request->transfer_flags;
	mausb_event.data.transfer_eot	   = false;
	mausb_event.data.isoch_seg_num	   = (u32)request->number_of_packets;
	mausb_event.data.recv_buf	   = 0;
	mausb_event.data.payload_size	   =
		(usb_endpoint_xfer_isoc(&request->ep->desc) &&
		 usb_endpoint_dir_out(&request->ep->desc)) ?
		(request->iso_frame_desc[request->number_of_packets - 1]
								.offset +
		 request->iso_frame_desc[request->number_of_packets - 1]
								.length) : 0;

	if (mausb_event.data.setup_packet) {
		memcpy(mausb_event.data.hdr_ack, request->setup_packet,
		       MAUSB_CONTROL_SETUP_SIZE);
		memcpy(shift_ptr(mausb_event.data.hdr_ack,
				 MAUSB_CONTROL_SETUP_SIZE),
		       &request->dev->route, sizeof(request->dev->route));
	}

	status = mausb_enqueue_event_to_user(dev, &mausb_event);
	if (status < 0)
		dev_err(&request->dev->dev, "Failed to enqueue event to user-space ep_handle=%#x, status=%d",
			mausb_event.data.ep_handle, status);

	return status;
}

void mausb_complete_request(struct urb *urb, u32 actual_length, int status)
{
	mausb_hcd_urb_complete(urb, actual_length, status);
}

int mausb_signal_event(struct mausb_device *dev,
		       struct mausb_event *event, u64 event_id)
{
	unsigned long flags;
	struct mausb_completion *mausb_completion;

	spin_lock_irqsave(&dev->completion_events_lock, flags);
	list_for_each_entry(mausb_completion, &dev->completion_events,
			    list_entry) {
		if (mausb_completion->event_id == event_id) {
			memcpy(mausb_completion->mausb_event, event,
			       sizeof(*event));
			complete(mausb_completion->completion_event);
			spin_unlock_irqrestore(&dev->completion_events_lock,
					       flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&dev->completion_events_lock, flags);

	return -ETIMEDOUT;
}

static int mausb_start_connection_timer(struct mausb_device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->connection_timer_lock, flags);

	if (++dev->receive_failures_num > MAUSB_MAX_RECEIVE_FAILURES) {
		dev_err(mausb_host_dev.this_device, "Missed more than %d ping responses",
			MAUSB_MAX_RECEIVE_FAILURES);
		spin_unlock_irqrestore(&dev->connection_timer_lock, flags);
		return -ETIMEDOUT;
	}

	mod_timer(&dev->connection_timer, jiffies + msecs_to_jiffies(1000));

	spin_unlock_irqrestore(&dev->connection_timer_lock, flags);

	return 0;
}

void mausb_reset_connection_timer(struct mausb_device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->connection_timer_lock, flags);
	dev->receive_failures_num = 0;

	mod_timer(&dev->connection_timer, jiffies + msecs_to_jiffies(1000));

	spin_unlock_irqrestore(&dev->connection_timer_lock, flags);
}

static int mausb_start_heartbeat_timer(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mss.lock, flags);
	if (++mss.missed_heartbeats > MAUSB_MAX_MISSED_HEARTBEATS) {
		dev_err(mausb_host_dev.this_device, "Missed more than %d heartbeats",
			MAUSB_MAX_MISSED_HEARTBEATS);
		spin_unlock_irqrestore(&mss.lock, flags);
		return -ETIMEDOUT;
	}

	spin_unlock_irqrestore(&mss.lock, flags);
	mod_timer(&mss.heartbeat_timer,
		  jiffies + msecs_to_jiffies(MAUSB_HEARTBEAT_TIMEOUT_MS));

	return 0;
}

void mausb_reset_heartbeat_cnt(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mss.lock, flags);
	mss.missed_heartbeats = 0;
	spin_unlock_irqrestore(&mss.lock, flags);
}

static void mausb_execute_urb_dequeue(struct work_struct *dequeue_work)
{
	struct mausb_urb_ctx *urb_ctx =
			container_of(dequeue_work, struct mausb_urb_ctx, work);
	struct urb		  *urb = urb_ctx->urb;
	struct mausb_endpoint_ctx *ep_ctx;
	struct mausb_device	  *ma_dev;
	struct mausb_event	  mausb_event;
	int status = 0;

	ep_ctx = urb->ep->hcpriv;
	ma_dev = ep_ctx->ma_dev;

	if (atomic_read(&ma_dev->unresponsive_client)) {
		dev_err(mausb_host_dev.this_device, "Client is not responsive anymore - finish urb immediately urb=%p, ep_handle=%#x, dev_handle=%#x",
			urb, ep_ctx->ep_handle, ep_ctx->dev_handle);
		goto complete_urb;
	}

	dev_vdbg(mausb_host_dev.this_device, "urb=%p, ep_handle=%#x, dev_handle=%#x",
		 urb, ep_ctx->ep_handle, ep_ctx->dev_handle);

	if (!usb_endpoint_xfer_isoc(&urb->ep->desc)) {
		status = mausb_canceltransfer_event_to_user(ep_ctx->ma_dev,
							    ep_ctx->dev_handle,
							    ep_ctx->ep_handle,
							    (uintptr_t)urb);
		if (status < 0)
			goto complete_urb;
	}

	memset(&mausb_event, 0, sizeof(mausb_event));

	mausb_event.type   = MAUSB_EVENT_TYPE_DELETE_DATA_TRANSFER;
	mausb_event.status = 0;

	mausb_event.data.transfer_type =
				mausb_transfer_type_from_usb(&urb->ep->desc);
	mausb_event.data.device_id     = ma_dev->id;
	mausb_event.data.ep_handle     = ep_ctx->ep_handle;
	mausb_event.data.urb	       = (uintptr_t)urb;
	mausb_event.data.direction     = (usb_urb_dir_in(urb) ?
						MAUSB_DATA_MSG_DIRECTION_IN :
						MAUSB_DATA_MSG_DIRECTION_OUT);

	status = mausb_enqueue_event_to_user(ep_ctx->ma_dev, &mausb_event);
	if (status < 0) {
		dev_alert(mausb_host_dev.this_device, "Failed to enqueue event to user-space ep_handle=%#x, status=%d",
			  mausb_event.data.ep_handle, status);
		goto complete_urb;
	}

	if (!mausb_return_urb_ctx_to_tree(urb_ctx, false)) {
		dev_alert(mausb_host_dev.this_device, "Failed to insert in tree urb=%p ep_handle=%#x, status=%d",
			  urb, mausb_event.data.ep_handle, status);
		goto complete_urb;
	}

	return;

complete_urb:

	/* Deallocate urb_ctx */
	mausb_uninit_data_iterator(&urb_ctx->iterator);
	kfree(urb_ctx);

	urb->status	   = -EPROTO;
	urb->actual_length = 0;
	atomic_dec(&urb->use_count);
	usb_hcd_giveback_urb(urb->hcpriv, urb, urb->status);
}

void mausb_initialize_mss(void)
{
	spin_lock_init(&mss.lock);
	INIT_LIST_HEAD(&mss.madev_list);
	INIT_LIST_HEAD(&mss.available_ring_buffers);

	init_completion(&mss.empty);
	complete(&mss.empty);
	init_completion(&mss.rings_events.mausb_ring_has_events);
	atomic_set(&mss.rings_events.mausb_stop_reading_ring_events, 0);
	mss.deinit_in_progress	= false;
	mss.ring_buffer_id	= 0;
	mss.client_connected = false;
	mss.missed_heartbeats = 0;
	init_completion(&mss.client_stopped);
	atomic_set(&mss.num_of_transitions_to_sleep, 0);

	setup_timer(&mss.heartbeat_timer, mausb_heartbeat_timer_func, 0);
}

void mausb_deinitialize_mss(void)
{
	struct mausb_device *dev = NULL;
	unsigned long flags;
	unsigned long timeout =
			msecs_to_jiffies(MAUSB_CLIENT_STOPPED_TIMEOUT_MS);

	spin_lock_irqsave(&mss.lock, flags);

	mss.deinit_in_progress = true;

	list_for_each_entry(dev, &mss.madev_list, list_entry) {
		dev_dbg(mausb_host_dev.this_device, "Enqueue mausb_hcd_disconnect_work madev_addr=%x",
			dev->madev_addr);
		queue_work(dev->workq, &dev->hcd_disconnect_work);
	}

	spin_unlock_irqrestore(&mss.lock, flags);

	wait_for_completion(&mss.empty);
	dev_dbg(mausb_host_dev.this_device, "Waiting for completion on disconnect_event ended");
	mausb_stop_ring_events();

	timeout = wait_for_completion_timeout(&mss.client_stopped, timeout);
	if (timeout > 0)
		dev_dbg(mausb_host_dev.this_device, "Remaining time after waiting for stopping client %ld",
			timeout);
}

int mausb_register_power_state_listener(void)
{
	dev_dbg(mausb_host_dev.this_device, "Registering power states listener");

	mhcd->power_state_listener.notifier_call = mausb_power_state_cb;
	return register_pm_notifier(&mhcd->power_state_listener);
}

void mausb_unregister_power_state_listener(void)
{
	dev_dbg(mausb_host_dev.this_device, "Un-registering power states listener");

	unregister_pm_notifier(&mhcd->power_state_listener);
}

static int mausb_power_state_cb(struct notifier_block *nb, unsigned long action,
				void *data)
{
	unsigned long flags;
	struct mausb_device *dev = NULL;

	dev_info(mausb_host_dev.this_device, "Power state callback action = %ld",
		 action);
	if (action == PM_SUSPEND_PREPARE || action == PM_HIBERNATION_PREPARE) {
		/* Stop heartbeat timer */
		del_timer_sync(&mss.heartbeat_timer);
		dev_info(mausb_host_dev.this_device, "Saving state before sleep");
		spin_lock_irqsave(&mss.lock, flags);
		if (!list_empty(&mss.madev_list))
			atomic_inc(&mss.num_of_transitions_to_sleep);

		list_for_each_entry(dev, &mss.madev_list, list_entry) {
			dev_info(mausb_host_dev.this_device, "Enqueue heartbeat_work madev_addr=%x",
				 dev->madev_addr);
			queue_work(dev->workq, &dev->heartbeat_work);
		}

		spin_unlock_irqrestore(&mss.lock, flags);
	} else if (action == PM_POST_SUSPEND || action == PM_POST_HIBERNATION) {
		mausb_reset_heartbeat_cnt();
		/* Start hearbeat timer */
		mod_timer(&mss.heartbeat_timer, jiffies +
			  msecs_to_jiffies(MAUSB_HEARTBEAT_TIMEOUT_MS));
	}
	return NOTIFY_OK;
}

static void mausb_populate_standard_ep_descriptor(struct usb_ep_desc *std_desc,
						  struct usb_endpoint_descriptor
						  *usb_std_desc)
{
	std_desc->bLength	   = usb_std_desc->bLength;
	std_desc->bDescriptorType  = usb_std_desc->bDescriptorType;
	std_desc->bEndpointAddress = usb_std_desc->bEndpointAddress;
	std_desc->bmAttributes	   = usb_std_desc->bmAttributes;
	std_desc->wMaxPacketSize   = usb_std_desc->wMaxPacketSize;
	std_desc->bInterval	   = usb_std_desc->bInterval;
}

static void
mausb_populate_superspeed_ep_descriptor(struct usb_ss_ep_comp_desc *ss_desc,
					struct usb_ss_ep_comp_descriptor*
					usb_ss_desc)
{
	ss_desc->bLength	   = usb_ss_desc->bLength;
	ss_desc->bDescriptorType   = usb_ss_desc->bDescriptorType;
	ss_desc->bMaxBurst	   = usb_ss_desc->bMaxBurst;
	ss_desc->bmAttributes	   = usb_ss_desc->bmAttributes;
	ss_desc->wBytesPerInterval = usb_ss_desc->wBytesPerInterval;
}

void
mausb_init_standard_ep_descriptor(struct ma_usb_ephandlereq_desc_std *std_desc,
				  struct usb_endpoint_descriptor *usb_std_desc)
{
	mausb_populate_standard_ep_descriptor(&std_desc->usb20, usb_std_desc);
}

void
mausb_init_superspeed_ep_descriptor(struct ma_usb_ephandlereq_desc_ss *ss_desc,
				    struct usb_endpoint_descriptor *
				    usb_std_desc,
				    struct usb_ss_ep_comp_descriptor *
				    usb_ss_desc)
{
	mausb_populate_standard_ep_descriptor(&ss_desc->usb20, usb_std_desc);
	mausb_populate_superspeed_ep_descriptor(&ss_desc->usb31, usb_ss_desc);
}

struct mausb_device *mausb_get_dev_from_addr_unsafe(u8 madev_addr)
{
	struct mausb_device *dev = NULL;

	list_for_each_entry(dev, &mss.madev_list, list_entry) {
		if (dev->madev_addr == madev_addr)
			return dev;
	}

	return NULL;
}

static void mausb_remove_madev_from_list(u8 madev_addr)
{
	unsigned long flags;
	struct mausb_device *ma_dev = NULL;
	struct mausb_device *tmp = NULL;

	spin_lock_irqsave(&mss.lock, flags);

	list_for_each_entry_safe(ma_dev, tmp, &mss.madev_list, list_entry) {
		if (ma_dev->madev_addr == madev_addr) {
			list_del(&ma_dev->list_entry);
			break;
		}
	}

	if (list_empty(&mss.madev_list))
		reinit_completion(&mss.rings_events.mausb_ring_has_events);

	spin_unlock_irqrestore(&mss.lock, flags);
}

static void mausb_signal_empty_mss(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mss.lock, flags);
	if (list_empty(&mss.madev_list))
		complete(&mss.empty);
	spin_unlock_irqrestore(&mss.lock, flags);
}

static inline
struct mausb_ip_ctx *mausb_get_data_channel(struct mausb_device *ma_dev,
					    enum mausb_channel channel)
{
	if (channel >= MAUSB_CHANNEL_MAP_LENGTH)
		return NULL;

	return ma_dev->channel_map[channel];
}

int mausb_send_data(struct mausb_device *dev, enum mausb_channel channel_num,
		    struct mausb_kvec_data_wrapper *data)
{
	struct mausb_ip_ctx *channel = mausb_get_data_channel(dev, channel_num);
	int status = 0;

	if (!channel)
		return -ECHRNG;

	status = mausb_ip_send(channel, data);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Send failed. Disconnecting... status=%d",
			status);
		queue_work(dev->workq, &dev->socket_disconnect_work);
		queue_work(dev->workq, &dev->hcd_disconnect_work);
	}

	return status;
}

int mausb_send_transfer_ack(struct mausb_device *dev, struct mausb_event *event)
{
	struct ma_usb_hdr_common *ack_hdr;
	struct kvec kvec;
	struct mausb_kvec_data_wrapper data_to_send;
	enum mausb_channel channel;

	ack_hdr = (struct ma_usb_hdr_common *)(&event->data.hdr_ack);

	data_to_send.kvec	    = &kvec;
	data_to_send.kvec->iov_base = ack_hdr;
	data_to_send.kvec->iov_len  = ack_hdr->length;
	data_to_send.kvec_num	    = 1;
	data_to_send.length	    = ack_hdr->length;

	channel = mausb_transfer_type_to_channel(event->data.transfer_type);
	return mausb_send_data(dev, channel, &data_to_send);
}

int mausb_send_data_msg(struct mausb_device *dev, struct mausb_event *event)
{
	struct mausb_urb_ctx *urb_ctx;
	int status = 0;

	if (event->status != 0) {
		dev_err(mausb_host_dev.this_device, "Event %d failed with status %d",
			event->type, event->status);
		mausb_complete_urb(event);
		return event->status;
	}

	urb_ctx = mausb_find_urb_in_tree((struct urb *)event->data.urb);
	if (!urb_ctx) {
		/* Transfer will be deleted from dequeue task */
		dev_warn(mausb_host_dev.this_device, "Urb is already cancelled for event=%d",
			 event->type);
		return status;
	}

	if (mausb_isoch_data_event(event)) {
		if (event->data.direction == MAUSB_DATA_MSG_DIRECTION_IN)
			status = mausb_send_isoch_in_msg(dev, event);
		else
			status = mausb_send_isoch_out_msg(dev, event, urb_ctx);
	} else {
		if (event->data.direction == MAUSB_DATA_MSG_DIRECTION_IN)
			status = mausb_send_in_data_msg(dev, event);
		else
			status = mausb_send_out_data_msg(dev, event, urb_ctx);
	}

	return status;
}

int mausb_receive_data_msg(struct mausb_device *dev, struct mausb_event *event)
{
	int status = 0;
	struct mausb_urb_ctx *urb_ctx;

	dev_vdbg(mausb_host_dev.this_device, "Direction=%d",
		 event->data.direction);

	if (!mausb_isoch_data_event(event)) {
		status = mausb_send_transfer_ack(dev, event);
		if (status < 0) {
			dev_warn(mausb_host_dev.this_device, "Sending acknowledgment failed");
			goto cleanup;
		}
	}

	urb_ctx = mausb_find_urb_in_tree((struct urb *)event->data.urb);
	if (!urb_ctx) {
		/* Transfer will be deleted from dequeue task */
		dev_warn(mausb_host_dev.this_device, "Urb is already cancelled");
		goto cleanup;
	}

	if (mausb_isoch_data_event(event)) {
		if (event->data.direction == MAUSB_DATA_MSG_DIRECTION_IN)
			mausb_receive_isoch_in_data(dev, event, urb_ctx);
		else
			mausb_receive_isoch_out(event);
	} else {
		if (event->data.direction == MAUSB_DATA_MSG_DIRECTION_IN)
			mausb_receive_in_data(event, urb_ctx);
		else
			mausb_receive_out_data(event, urb_ctx);
	}

cleanup:
	mausb_release_event_resources(event);
	return status;
}

int mausb_add_data_chunk(void *buffer, u32 buffer_size,
			 struct list_head *chunks_list)
{
	struct mausb_payload_chunk *data_chunk;

	data_chunk = kzalloc(sizeof(*data_chunk), GFP_KERNEL);
	if (!data_chunk)
		return -ENOMEM;

	/* Initialize data chunk for MAUSB header and add it to chunks list */
	INIT_LIST_HEAD(&data_chunk->list_entry);

	data_chunk->kvec.iov_base = buffer;
	data_chunk->kvec.iov_len  = buffer_size;
	list_add_tail(&data_chunk->list_entry, chunks_list);
	return 0;
}

int mausb_init_data_wrapper(struct mausb_kvec_data_wrapper *data,
			    struct list_head *chunks_list,
			    u32 num_of_data_chunks)
{
	struct mausb_payload_chunk *data_chunk = NULL;
	struct mausb_payload_chunk *tmp = NULL;
	u32 current_kvec = 0;

	data->length = 0;
	data->kvec = kcalloc(num_of_data_chunks, sizeof(struct kvec),
			     GFP_KERNEL);
	if (!data->kvec)
		return -ENOMEM;

	list_for_each_entry_safe(data_chunk, tmp, chunks_list, list_entry) {
		data->kvec[current_kvec].iov_base =
			data_chunk->kvec.iov_base;
		data->kvec[current_kvec].iov_len =
		    data_chunk->kvec.iov_len;
		++data->kvec_num;
		data->length += data_chunk->kvec.iov_len;
		++current_kvec;
	}
	return 0;
}

void mausb_cleanup_chunks_list(struct list_head *chunks_list)
{
	struct mausb_payload_chunk *data_chunk = NULL;
	struct mausb_payload_chunk *tmp = NULL;

	list_for_each_entry_safe(data_chunk, tmp, chunks_list, list_entry) {
		list_del(&data_chunk->list_entry);
		kfree(data_chunk);
	}
}

static void mausb_init_ip_ctx_helper(struct mausb_device *dev,
				     struct mausb_ip_ctx **ip_ctx,
				     u16 port,
				     enum mausb_channel channel)
{
	int status = mausb_init_ip_ctx(ip_ctx, dev->net_ns,
				       dev->dev_addr.ip.address, port, dev,
				       mausb_ip_callback, channel);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Init ip context failed with error=%d",
			status);
		queue_work(dev->workq, &dev->socket_disconnect_work);
		return;
	}

	dev->channel_map[channel] = *ip_ctx;
	mausb_ip_connect_async(*ip_ctx);
}

static void mausb_connect_callback(struct mausb_device *dev,
				   enum mausb_channel channel, int status)
{
	struct mausb_device_address *dev_addr = &dev->dev_addr;

	dev_info(mausb_host_dev.this_device, "Connect callback for channel=%d with status=%d",
		      channel, status);

	if (status < 0) {
		queue_work(dev->workq, &dev->socket_disconnect_work);
		return;
	}

	if (channel == MAUSB_MGMT_CHANNEL) {
		if (dev_addr->ip.port.control == 0) {
			dev->channel_map[MAUSB_CTRL_CHANNEL] =
				dev->mgmt_channel;
			channel = MAUSB_CTRL_CHANNEL;
		} else {
			mausb_init_ip_ctx_helper(dev, &dev->ctrl_channel,
						 dev_addr->ip.port.control,
						 MAUSB_CTRL_CHANNEL);
			return;
		}
	}

	if (channel == MAUSB_CTRL_CHANNEL) {
		if (dev_addr->ip.port.bulk == 0) {
			dev->channel_map[MAUSB_BULK_CHANNEL] =
				dev->channel_map[MAUSB_CTRL_CHANNEL];
			channel = MAUSB_BULK_CHANNEL;
		} else {
			mausb_init_ip_ctx_helper(dev, &dev->bulk_channel,
						 dev_addr->ip.port.bulk,
						 MAUSB_BULK_CHANNEL);
			return;
		}
	}

	if (channel == MAUSB_BULK_CHANNEL) {
		if (dev_addr->ip.port.isochronous == 0) {
			/* if there is no isoch port use tcp for it */
			dev->channel_map[MAUSB_ISOCH_CHANNEL] =
				dev->channel_map[MAUSB_BULK_CHANNEL];
			channel = MAUSB_ISOCH_CHANNEL;
		} else {
			mausb_init_ip_ctx_helper(dev, &dev->isoch_channel,
						 dev_addr->ip.port.isochronous,
						 MAUSB_ISOCH_CHANNEL);
			return;
		}
	}

	if (channel == MAUSB_ISOCH_CHANNEL) {
		dev->channel_map[MAUSB_INTR_CHANNEL] =
				dev->channel_map[MAUSB_CTRL_CHANNEL];
		mausb_on_madev_connected(dev);
	}
}

static void mausb_handle_connect_event(struct mausb_device *dev,
				       enum mausb_channel channel, int status,
				       void *data)
{
	mausb_connect_callback(dev, channel, status);
}

static void mausb_handle_receive_event(struct mausb_device *dev,
				       enum mausb_channel channel, int status,
				       void *data)
{
	struct mausb_event event;

	event.madev_addr = dev->madev_addr;

	if (status <= 0) {
		dev_err(mausb_host_dev.this_device, "Receive event error status=%d",
			status);
		queue_work(dev->workq, &dev->socket_disconnect_work);
		queue_work(dev->workq, &dev->hcd_disconnect_work);
		return;
	}

	mausb_reset_connection_timer(dev);

	status = mausb_msg_received_event(&event,
					  (struct ma_usb_hdr_common *)data,
					  channel);
	if (status == 0)
		status = mausb_enqueue_event_to_user(dev, &event);

	if (status < 0)
		dev_err(mausb_host_dev.this_device, "Failed to enqueue, status=%d",
			status);
}

void mausb_ip_callback(void *ctx, enum mausb_channel channel,
		       enum mausb_link_action action, int status, void *data)
{
	struct mausb_device *dev = (struct mausb_device *)ctx;

	switch (action) {
	case MAUSB_LINK_CONNECT:
		mausb_handle_connect_event(dev, channel, status, data);
		break;
	case MAUSB_LINK_SEND:
		/*
		 * Currently there is nothing to do, as send operation is
		 * synchronous
		 */
		break;
	case MAUSB_LINK_RECV:
		mausb_handle_receive_event(dev, channel, status, data);
		break;
	case MAUSB_LINK_DISCONNECT:
		/*
		 * Currently there is nothing to do, as disconnect operation is
		 * synchronous
		 */
		break;
	default:
		dev_warn(mausb_host_dev.this_device, "Unknown network action");
	}
}

static int mausb_read_virtual_buffer(struct mausb_data_iter *iterator,
				     u32 byte_num,
				     struct list_head *data_chunks_list,
				     u32 *data_chunks_num)
{
	u32 rem_data		= 0;
	u32 bytes_to_read	= 0;
	struct mausb_payload_chunk *data_chunk = NULL;

	(*data_chunks_num) = 0;

	if (!data_chunks_list)
		return -EINVAL;

	INIT_LIST_HEAD(data_chunks_list);
	rem_data      = iterator->length - iterator->offset;
	bytes_to_read = min(byte_num, rem_data);

	if (bytes_to_read == 0)
		return 0;

	data_chunk = kzalloc(sizeof(*data_chunk), GFP_KERNEL);

	if (!data_chunk)
		return -ENOMEM;

	++(*data_chunks_num);

	data_chunk->kvec.iov_base = (u8 *)(iterator->buffer) + iterator->offset;
	data_chunk->kvec.iov_len = bytes_to_read;
	iterator->offset += bytes_to_read;

	list_add_tail(&data_chunk->list_entry, data_chunks_list);

	return 0;
}

static int mausb_read_scatterlist_buffer(struct mausb_data_iter *iterator,
					 u32 byte_num,
					 struct list_head *data_chunks_list,
					 u32 *data_chunks_num)
{
	u32 current_sg_read_num;
	struct mausb_payload_chunk *data_chunk = NULL;

	(*data_chunks_num) = 0;

	if (!data_chunks_list)
		return -EINVAL;

	INIT_LIST_HEAD(data_chunks_list);

	while (byte_num) {
		if (iterator->sg_iter.consumed == iterator->sg_iter.length) {
			if (!sg_miter_next(&iterator->sg_iter))
				break;
			iterator->sg_iter.consumed = 0;
		}

		data_chunk = kzalloc(sizeof(*data_chunk), GFP_KERNEL);
		if (!data_chunk) {
			sg_miter_stop(&iterator->sg_iter);
			return -ENOMEM;
		}

		current_sg_read_num = min((size_t)byte_num,
					  iterator->sg_iter.length -
					  iterator->sg_iter.consumed);

		data_chunk->kvec.iov_base = (u8 *)iterator->sg_iter.addr +
				iterator->sg_iter.consumed;
		data_chunk->kvec.iov_len  = current_sg_read_num;

		++(*data_chunks_num);
		list_add_tail(&data_chunk->list_entry, data_chunks_list);

		byte_num -= current_sg_read_num;
		iterator->sg_iter.consumed += current_sg_read_num;
		data_chunk = NULL;
	}

	return 0;
}

static u32 mausb_write_virtual_buffer(struct mausb_data_iter *iterator,
				      void *buffer, u32 size)
{
	u32 rem_space   = 0;
	u32 write_count = 0;

	if (!buffer || !size)
		return write_count;

	rem_space   = iterator->length - iterator->offset;
	write_count = min(size, rem_space);

	if (write_count > 0) {
		void *location = shift_ptr(iterator->buffer, iterator->offset);

		memcpy(location, buffer, write_count);
		iterator->offset += write_count;
	}

	return write_count;
}

static u32 mausb_write_scatterlist_buffer(struct mausb_data_iter *iterator,
					  void *buffer, u32 size)
{
	u32 current_sg_rem_space;
	u32 count = 0;
	u32 total_count = 0;
	void *location = NULL;

	if (!buffer || !size)
		return count;

	while (size) {
		if (iterator->sg_iter.consumed >= iterator->sg_iter.length) {
			if (!sg_miter_next(&iterator->sg_iter))
				break;
			iterator->sg_iter.consumed = 0;
		}

		current_sg_rem_space = (u32)(iterator->sg_iter.length -
			iterator->sg_iter.consumed);

		count = min(size, current_sg_rem_space);
		total_count += count;

		location = shift_ptr(iterator->sg_iter.addr,
				     iterator->sg_iter.consumed);

		memcpy(location, buffer, count);

		buffer = shift_ptr(buffer, count);
		size -= count;
		iterator->sg_iter.consumed += count;
	}

	return total_count;
}

int mausb_data_iterator_read(struct mausb_data_iter *iterator,
			     u32 byte_num,
			     struct list_head *data_chunks_list,
			     u32 *data_chunks_num)
{
	if (iterator->buffer)
		return mausb_read_virtual_buffer(iterator, byte_num,
						 data_chunks_list,
						 data_chunks_num);
	else
		return mausb_read_scatterlist_buffer(iterator, byte_num,
						     data_chunks_list,
						     data_chunks_num);
}

u32 mausb_data_iterator_write(struct mausb_data_iter *iterator, void *buffer,
			      u32 length)
{
	if (iterator->buffer)
		return mausb_write_virtual_buffer(iterator, buffer, length);
	else
		return mausb_write_scatterlist_buffer(iterator, buffer, length);
}

static inline void mausb_seek_virtual_buffer(struct mausb_data_iter *iterator,
					     u32 seek_delta)
{
	iterator->offset += min(seek_delta, iterator->length -
					    iterator->offset);
}

static void mausb_seek_scatterlist_buffer(struct mausb_data_iter *iterator,
					  u32 seek_delta)
{
	u32 rem_bytes_in_current_scatter;

	while (seek_delta) {
		rem_bytes_in_current_scatter = (u32)(iterator->sg_iter.length -
						iterator->sg_iter.consumed);
		if (rem_bytes_in_current_scatter <= seek_delta) {
			iterator->sg_iter.consumed +=
			    rem_bytes_in_current_scatter;
			seek_delta -= rem_bytes_in_current_scatter;
			if (!sg_miter_next(&iterator->sg_iter))
				break;
			iterator->sg_iter.consumed = 0;
		} else {
			iterator->sg_iter.consumed += seek_delta;
			break;
		}
	}
}

void mausb_data_iterator_seek(struct mausb_data_iter *iterator,
			      u32 seek_delta)
{
	if (iterator->buffer)
		mausb_seek_virtual_buffer(iterator, seek_delta);
	else
		mausb_seek_scatterlist_buffer(iterator, seek_delta);
}

static void mausb_calculate_buffer_length(struct mausb_data_iter *iterator)
{
	/* Calculate buffer length */
	if (iterator->buffer_len > 0) {
		/* Transfer_buffer_length is populated */
		iterator->length = iterator->buffer_len;
	} else if (iterator->sg && iterator->num_sgs != 0) {
		/* Transfer_buffer_length is not populated */
		sg_miter_start(&iterator->sg_iter, iterator->sg,
			       iterator->num_sgs, iterator->flags);
		while (sg_miter_next(&iterator->sg_iter))
			iterator->length += (u32)iterator->sg_iter.length;
		sg_miter_stop(&iterator->sg_iter);
	} else {
		iterator->length = 0;
	}
}

void mausb_init_data_iterator(struct mausb_data_iter *iterator, void *buffer,
			      u32 buffer_len, struct scatterlist *sg,
			      unsigned int num_sgs, bool direction)
{
	iterator->offset = 0;
	iterator->buffer     = buffer;
	iterator->buffer_len = buffer_len;
	iterator->length     = 0;
	iterator->sg	     = sg;
	iterator->num_sgs    = num_sgs;
	iterator->sg_started = false;

	mausb_calculate_buffer_length(iterator);

	if (!buffer && sg && num_sgs != 0) {
		/* Scatterlist provided */
		iterator->flags = direction ? SG_MITER_TO_SG : SG_MITER_FROM_SG;
		sg_miter_start(&iterator->sg_iter, sg, num_sgs,
			       iterator->flags);
		iterator->sg_started = true;
	}
}

void mausb_uninit_data_iterator(struct mausb_data_iter *iterator)
{
	iterator->offset     = 0;
	iterator->length     = 0;
	iterator->buffer     = NULL;
	iterator->buffer_len = 0;

	if (iterator->sg_started)
		sg_miter_stop(&iterator->sg_iter);

	iterator->sg_started = false;
}

void mausb_reset_data_iterator(struct mausb_data_iter *iterator)
{
	iterator->offset = 0;
	if (iterator->sg_started) {
		sg_miter_stop(&iterator->sg_iter);
		iterator->sg_started = false;
	}

	if (!iterator->buffer && iterator->sg && iterator->num_sgs != 0) {
		sg_miter_start(&iterator->sg_iter, iterator->sg,
			       iterator->num_sgs, iterator->flags);
		iterator->sg_started = true;
	}
}

u32 mausb_data_iterator_length(struct mausb_data_iter *iterator)
{
	return iterator->length;
}

static int mausb_ring_buffer_get(struct mausb_ring_buffer *ring,
				 struct mausb_event *event)
{
	unsigned long flags;

	spin_lock_irqsave(&ring->lock, flags);
	if (CIRC_CNT(ring->head, ring->tail, MAUSB_RING_BUFFER_SIZE) < 1) {
		spin_unlock_irqrestore(&ring->lock, flags);
		return -ENOSPC;
	}
	memcpy(event, ring->to_user_buffer + ring->tail, sizeof(*event));
	dev_vdbg(mausb_host_dev.this_device, "HEAD=%d, TAIL=%d", ring->head,
		 ring->tail);
	ring->tail = (ring->tail + 1) & (MAUSB_RING_BUFFER_SIZE - 1);
	dev_vdbg(mausb_host_dev.this_device, "HEAD=%d, TAIL=%d", ring->head,
		 ring->tail);
	spin_unlock_irqrestore(&ring->lock, flags);
	return 0;
}

int mausb_ring_buffer_init(struct mausb_ring_buffer *ring)
{
	unsigned int page_order =
		mausb_get_page_order(2 * MAUSB_RING_BUFFER_SIZE,
				     sizeof(struct mausb_event));
	ring->to_user_buffer =
		(struct mausb_event *)__get_free_pages(GFP_KERNEL, page_order);
	if (!ring->to_user_buffer)
		return -ENOMEM;
	ring->from_user_buffer = ring->to_user_buffer + MAUSB_RING_BUFFER_SIZE;
	ring->head = 0;
	ring->tail = 0;
	ring->current_from_user = 0;
	ring->buffer_full = false;
	spin_lock_init(&ring->lock);

	return 0;
}

int mausb_ring_buffer_put(struct mausb_ring_buffer *ring,
			  struct mausb_event *event)
{
	unsigned long flags;

	spin_lock_irqsave(&ring->lock, flags);

	if (ring->buffer_full) {
		dev_err(mausb_host_dev.this_device, "Ring buffer is full");
		spin_unlock_irqrestore(&ring->lock, flags);
		return -ENOSPC;
	}

	if (CIRC_SPACE(ring->head, ring->tail, MAUSB_RING_BUFFER_SIZE) < 2) {
		dev_err(mausb_host_dev.this_device, "Ring buffer capacity exceeded, disconnecting device");
		ring->buffer_full = true;
		mausb_disconect_event_unsafe(ring, event->madev_addr);
		ring->head = (ring->head + 1) & (MAUSB_RING_BUFFER_SIZE - 1);
		spin_unlock_irqrestore(&ring->lock, flags);
		return -ENOSPC;
	}
	memcpy(ring->to_user_buffer + ring->head, event, sizeof(*event));
	dev_vdbg(mausb_host_dev.this_device, "HEAD=%d, TAIL=%d",
		 ring->head, ring->tail);
	ring->head = (ring->head + 1) & (MAUSB_RING_BUFFER_SIZE - 1);
	dev_vdbg(mausb_host_dev.this_device, "HEAD=%d, TAIL=%d",
		 ring->head, ring->tail);
	spin_unlock_irqrestore(&ring->lock, flags);
	return 0;
}

int mausb_ring_buffer_move_tail(struct mausb_ring_buffer *ring, u32 count)
{
	unsigned long flags;

	spin_lock_irqsave(&ring->lock, flags);
	if (CIRC_CNT(ring->head, ring->tail, MAUSB_RING_BUFFER_SIZE) < count) {
		spin_unlock_irqrestore(&ring->lock, flags);
		return -ENOSPC;
	}
	dev_vdbg(mausb_host_dev.this_device, "old HEAD=%d, TAIL=%d",
		 ring->head, ring->tail);
	ring->tail = (ring->tail + (int)count) & (MAUSB_RING_BUFFER_SIZE - 1);
	dev_vdbg(mausb_host_dev.this_device, "new HEAD=%d, TAIL=%d",
		 ring->head, ring->tail);
	spin_unlock_irqrestore(&ring->lock, flags);
	return 0;
}

void mausb_ring_buffer_cleanup(struct mausb_ring_buffer *ring)
{
	struct mausb_event event;

	while (mausb_ring_buffer_get(ring, &event) == 0)
		mausb_cleanup_ring_buffer_event(&event);
}

void mausb_ring_buffer_destroy(struct mausb_ring_buffer *ring)
{
	unsigned int page_order =
		mausb_get_page_order(2 * MAUSB_RING_BUFFER_SIZE,
				     sizeof(struct mausb_event));
	if (ring && ring->to_user_buffer)
		free_pages((unsigned long)ring->to_user_buffer, page_order);
}

void mausb_cleanup_ring_buffer_event(struct mausb_event *event)
{
	dev_dbg(mausb_host_dev.this_device, "Cleanup ring buffer event=%d",
		event->type);
	switch (event->type) {
	case MAUSB_EVENT_TYPE_SEND_DATA_MSG:
		mausb_cleanup_send_data_msg_event(event);
		break;
	case MAUSB_EVENT_TYPE_RECEIVED_DATA_MSG:
		mausb_cleanup_received_data_msg_event(event);
		break;
	case MAUSB_EVENT_TYPE_DELETE_DATA_TRANSFER:
		mausb_cleanup_delete_data_transfer_event(event);
		break;
	case MAUSB_EVENT_TYPE_NONE:
		break;
	default:
		dev_warn(mausb_host_dev.this_device, "Unknown event type");
		break;
	}
}

void mausb_disconect_event_unsafe(struct mausb_ring_buffer *ring,
				  uint8_t madev_addr)
{
	struct mausb_event disconnect_event;
	struct mausb_device *dev = mausb_get_dev_from_addr_unsafe(madev_addr);

	if (!dev) {
		dev_err(mausb_host_dev.this_device, "Device not found, madev_addr=%#x",
			madev_addr);
		return;
	}

	disconnect_event.type = MAUSB_EVENT_TYPE_DEV_DISCONNECT;
	disconnect_event.status = -EINVAL;
	disconnect_event.madev_addr = madev_addr;

	memcpy(ring->to_user_buffer + ring->head, &disconnect_event,
	       sizeof(disconnect_event));

	dev_dbg(mausb_host_dev.this_device, "Adding hcd_disconnect_work to dev workq, madev_addr=%#x",
		madev_addr);
	queue_work(dev->workq, &dev->hcd_disconnect_work);
}
