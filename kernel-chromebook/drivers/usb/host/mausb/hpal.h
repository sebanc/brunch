/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_HPAL_H__
#define __MAUSB_HPAL_H__

#include <linux/miscdevice.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/usb.h>

#include "ip_link.h"
#include "mausb_address.h"
#include "mausb_event.h"

#define MAUSB_CONTROL_SETUP_SIZE	8
#define MAUSB_BUSY_RETRIES_COUNT	3
#define MAUSB_HEARTBEAT_TIMEOUT_MS	1000
#define MAUSB_CLIENT_STOPPED_TIMEOUT_MS	3000

#define MAUSB_MAX_RECEIVE_FAILURES	3
#define MAUSB_MAX_MISSED_HEARTBEATS	3
#define MAUSB_TRANSFER_RESERVED		0

#define MAUSB_CHANNEL_MAP_LENGTH	4

extern struct mss mss;
extern struct mausb_hcd *mhcd;
extern struct miscdevice mausb_host_dev;

enum mausb_isoch_header_format_size {
	MAUSB_ISOCH_SHORT_FORMAT_SIZE	 = 4,
	MAUSB_ISOCH_STANDARD_FORMAT_SIZE = 8,
	MAUSB_ISOCH_LONG_FORMAT_SIZE	 = 12,
};

struct mausb_completion {
	struct list_head   list_entry;
	struct completion  *completion_event;
	struct mausb_event *mausb_event;
	u64		   event_id;
};

struct mausb_mss_rings_events {
	atomic_t	  mausb_stop_reading_ring_events;
	struct completion mausb_ring_has_events;
};

struct mss {
	bool	   deinit_in_progress;
	spinlock_t lock;	/* Protect mss structure */
	u64	   ring_buffer_id;

	struct completion empty;
	struct completion client_stopped;
	bool		  client_connected;
	struct timer_list heartbeat_timer;
	u8		  missed_heartbeats;

	struct list_head  madev_list;
	atomic_t	  num_of_transitions_to_sleep;
	struct list_head  available_ring_buffers;

	struct mausb_mss_rings_events	 rings_events;
	struct mausb_events_notification events[MAUSB_MAX_NUM_OF_MA_DEVS];
};

struct mausb_device {
	struct mausb_device_address dev_addr;
	struct net		    *net_ns;
	struct mausb_ring_buffer    *ring_buffer;
	struct list_head	    list_entry;

	struct mausb_ip_ctx *mgmt_channel;
	struct mausb_ip_ctx *ctrl_channel;
	struct mausb_ip_ctx *bulk_channel;
	struct mausb_ip_ctx *isoch_channel;
	struct mausb_ip_ctx *channel_map[MAUSB_CHANNEL_MAP_LENGTH];

	struct work_struct work;
	struct work_struct socket_disconnect_work;
	struct work_struct hcd_disconnect_work;
	struct work_struct madev_delete_work;
	struct work_struct ping_work;
	struct work_struct heartbeat_work;
	struct workqueue_struct *workq;

	struct kref refcount;
	/* Set on port change event after cap resp */
	u8 dev_type;
	u8 dev_speed;
	u8 lse;
	u8 madev_addr;
	u8 dev_connected;
	u8 port_number;
	u16 id;

	u64		event_id;
	spinlock_t	event_id_lock; /* Lock event ID increments */

	struct list_head completion_events;
	spinlock_t	 completion_events_lock; /* Lock completion events */

	struct completion user_finished_event;
	u16		  num_of_user_events;
	u16		  num_of_completed_events;

	spinlock_t	  num_of_user_events_lock; /* Lock user events count */

	struct timer_list connection_timer;
	u8		  receive_failures_num;
	spinlock_t	  connection_timer_lock; /* Lock connection timer */

	atomic_t	  unresponsive_client;

	atomic_t	  num_of_usb_devices;
};

struct mausb_urb_ctx *mausb_find_urb_in_tree(struct urb *urb);
struct mausb_urb_ctx *mausb_unlink_and_delete_urb_from_tree(struct urb *urb,
							    int status);
struct mausb_device *mausb_get_dev_from_addr_unsafe(u8 madev_addr);

static inline u64 mausb_event_id(struct mausb_device *dev)
{
	unsigned long flags;
	u64 val;

	spin_lock_irqsave(&dev->event_id_lock, flags);
	val = ++(dev->event_id);
	spin_unlock_irqrestore(&dev->event_id_lock, flags);

	return val;
}

int mausb_initiate_dev_connection(struct mausb_device_address device_address,
				  u8 madev_address);
int mausb_enqueue_event_from_user(u8 madev_addr, u16 num_of_events,
				  u16 num_of_completed);
int mausb_enqueue_event_to_user(struct mausb_device *dev,
				struct mausb_event *event);
int mausb_data_req_enqueue_event(struct mausb_device *dev, u16 ep_handle,
				 struct urb *request);
int mausb_signal_event(struct mausb_device *dev, struct mausb_event *event,
		       u64 event_id);
int mausb_insert_urb_in_tree(struct urb *urb, bool link_urb_to_ep);

static inline void mausb_insert_event(struct mausb_device *dev,
				      struct mausb_completion *event)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->completion_events_lock, flags);
	list_add_tail(&event->list_entry, &dev->completion_events);
	spin_unlock_irqrestore(&dev->completion_events_lock, flags);
}

static inline void mausb_remove_event(struct mausb_device *dev,
				      struct mausb_completion *event)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->completion_events_lock, flags);
	list_del(&event->list_entry);
	spin_unlock_irqrestore(&dev->completion_events_lock, flags);
}

void mausb_release_ma_dev_async(struct kref *kref);
void mausb_on_madev_connected(struct mausb_device *dev);
void mausb_complete_request(struct urb *urb, u32 actual_length, int status);
void mausb_complete_urb(struct mausb_event *event);
void mausb_reset_connection_timer(struct mausb_device *dev);
void mausb_reset_heartbeat_cnt(void);
void mausb_release_event_resources(struct mausb_event  *event);
void mausb_initialize_mss(void);
void mausb_deinitialize_mss(void);
int mausb_register_power_state_listener(void);
void mausb_unregister_power_state_listener(void);

void mausb_init_standard_ep_descriptor(struct ma_usb_ephandlereq_desc_std *
				       std_desc,
				       struct usb_endpoint_descriptor *
				       usb_std_desc);
void mausb_init_superspeed_ep_descriptor(struct ma_usb_ephandlereq_desc_ss *
					 ss_desc,
					 struct usb_endpoint_descriptor *
					 usb_std_desc,
					 struct usb_ss_ep_comp_descriptor *
					 usb_ss_desc);

int mausb_send_data(struct mausb_device *dev, enum mausb_channel channel_num,
		    struct mausb_kvec_data_wrapper *data);

int mausb_send_transfer_ack(struct mausb_device *dev,
			    struct mausb_event *event);

int mausb_send_data_msg(struct mausb_device *dev, struct mausb_event *event);

int mausb_receive_data_msg(struct mausb_device *dev, struct mausb_event *event);

int mausb_add_data_chunk(void *buffer, u32 buffer_size,
			 struct list_head *chunks_list);

int mausb_init_data_wrapper(struct mausb_kvec_data_wrapper *data,
			    struct list_head *chunks_list,
			    u32 num_of_data_chunks);

void mausb_cleanup_chunks_list(struct list_head *chunks_list);

static inline bool mausb_ctrl_transfer(struct ma_usb_hdr_common *hdr)
{
	return (hdr->data.t_flags & MA_USB_DATA_TFLAGS_TRANSFER_TYPE_MASK) ==
		MA_USB_DATA_TFLAGS_TRANSFER_TYPE_CTRL;
}

static inline bool mausb_isoch_transfer(struct ma_usb_hdr_common *hdr)
{
	return (hdr->data.t_flags & MA_USB_DATA_TFLAGS_TRANSFER_TYPE_MASK) ==
		MA_USB_DATA_TFLAGS_TRANSFER_TYPE_ISOCH;
}

static inline bool mausb_ctrl_data_event(struct mausb_event *event)
{
	return event->data.transfer_type ==
		MA_USB_DATA_TFLAGS_TRANSFER_TYPE_CTRL;
}

static inline bool mausb_isoch_data_event(struct mausb_event *event)
{
	return event->data.transfer_type ==
		MA_USB_DATA_TFLAGS_TRANSFER_TYPE_ISOCH;
}

/* usb to mausb transfer type */
static inline
u8 mausb_transfer_type_from_usb(struct usb_endpoint_descriptor *epd)
{
	return (u8)usb_endpoint_type(epd) << 3;
}

static inline u8 mausb_transfer_type_from_hdr(struct ma_usb_hdr_common *hdr)
{
	return hdr->data.t_flags & MA_USB_DATA_TFLAGS_TRANSFER_TYPE_MASK;
}

static inline
enum mausb_channel mausb_transfer_type_to_channel(u8 transfer_type)
{
	return transfer_type >> 3;
}

void mausb_ip_callback(void *ctx, enum mausb_channel channel,
		       enum mausb_link_action action, int status, void *data);

struct mausb_data_iter {
	u32 length;

	void *buffer;
	u32  buffer_len;
	u32  offset;

	struct scatterlist	*sg;
	struct sg_mapping_iter	sg_iter;
	bool		sg_started;
	unsigned int	num_sgs;
	unsigned int	flags;
};

struct mausb_payload_chunk {
	struct list_head list_entry;
	struct kvec	 kvec;
};

int mausb_data_iterator_read(struct mausb_data_iter *iterator,
			     u32 byte_num,
			     struct list_head *data_chunks_list,
			     u32 *data_chunks_num);

u32 mausb_data_iterator_length(struct mausb_data_iter *iterator);
u32 mausb_data_iterator_write(struct mausb_data_iter *iterator, void *buffer,
			      u32 length);

void mausb_init_data_iterator(struct mausb_data_iter *iterator,
			      void *buffer, u32 buffer_len,
			      struct scatterlist *sg, unsigned int num_sgs,
			      bool direction);
void mausb_reset_data_iterator(struct mausb_data_iter *iterator);
void mausb_uninit_data_iterator(struct mausb_data_iter *iterator);
void mausb_data_iterator_seek(struct mausb_data_iter *iterator, u32 seek_delta);

struct mausb_ring_buffer {
	atomic_t mausb_ring_events;
	atomic_t mausb_completed_user_events;

	struct mausb_event *to_user_buffer;
	int		head;
	int		tail;
	spinlock_t	lock; /* Protect ring buffer */
	u64		id;

	struct mausb_event *from_user_buffer;
	int current_from_user;

	struct list_head list_entry;
	bool buffer_full;
};

int mausb_ring_buffer_init(struct mausb_ring_buffer *ring);
int mausb_ring_buffer_put(struct mausb_ring_buffer *ring,
			  struct mausb_event *event);
int mausb_ring_buffer_move_tail(struct mausb_ring_buffer *ring, u32 count);
void mausb_ring_buffer_cleanup(struct mausb_ring_buffer *ring);
void mausb_ring_buffer_destroy(struct mausb_ring_buffer *ring);
void mausb_cleanup_ring_buffer_event(struct mausb_event *event);
void mausb_disconect_event_unsafe(struct mausb_ring_buffer *ring,
				  uint8_t madev_addr);

static inline unsigned int mausb_get_page_order(unsigned int num_of_elems,
						unsigned int elem_size)
{
	unsigned int num_of_pages = DIV_ROUND_UP(num_of_elems * elem_size,
						 PAGE_SIZE);
	unsigned int order = (unsigned int)ilog2(num_of_pages) +
					(is_power_of_2(num_of_pages) ? 0 : 1);
	return order;
}

static inline
struct mausb_event *mausb_ring_current_from_user(struct mausb_ring_buffer *ring)
{
	return ring->from_user_buffer + ring->current_from_user;
}

static inline void mausb_ring_next_from_user(struct mausb_ring_buffer *ring)
{
	ring->current_from_user = (ring->current_from_user + 1) &
				  (MAUSB_RING_BUFFER_SIZE - 1);
}

#endif /* __MAUSB_HPAL_H__ */
