// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#include "hpal_data.h"

#include <linux/slab.h>
#include <linux/uio.h>

#include "hcd.h"
#include "hpal.h"
#include "hpal_events.h"
#include "utils.h"

int mausb_send_in_data_msg(struct mausb_device *dev, struct mausb_event *event)
{
	struct mausb_kvec_data_wrapper data_to_send;
	struct kvec kvec[2];
	struct urb *urb   = (struct urb *)(event->data.urb);
	bool setup_packet = (usb_endpoint_xfer_control(&urb->ep->desc) &&
			     urb->setup_packet);
	u32 kvec_num = setup_packet ? 2 : 1;
	enum mausb_channel channel;

	data_to_send.kvec_num	= kvec_num;
	data_to_send.length	= MAUSB_TRANSFER_HDR_SIZE +
			(setup_packet ? MAUSB_CONTROL_SETUP_SIZE : 0);

	/* Prepare transfer header kvec */
	kvec[0].iov_base = event->data.hdr;
	kvec[0].iov_len  = MAUSB_TRANSFER_HDR_SIZE;

	/* Prepare setup packet kvec */
	if (setup_packet) {
		kvec[1].iov_base = urb->setup_packet;
		kvec[1].iov_len  = MAUSB_CONTROL_SETUP_SIZE;
	}
	data_to_send.kvec = kvec;

	channel = mausb_transfer_type_to_channel(event->data.transfer_type);
	return mausb_send_data(dev, channel, &data_to_send);
}

void mausb_receive_in_data(struct mausb_event *event,
			   struct mausb_urb_ctx *urb_ctx)
{
	struct urb *urb = urb_ctx->urb;
	struct mausb_data_iter *iterator     = &urb_ctx->iterator;
	struct ma_usb_hdr_common *common_hdr =
			(struct ma_usb_hdr_common *)event->data.recv_buf;
	void *buffer;
	u32 payload_size = common_hdr->length - MAUSB_TRANSFER_HDR_SIZE;
	u32 data_written = 0;

	buffer = shift_ptr(common_hdr, MAUSB_TRANSFER_HDR_SIZE);
	data_written = mausb_data_iterator_write(iterator, buffer,
						 payload_size);

	dev_vdbg(mausb_host_dev.this_device, "data_written=%d, payload_size=%d",
		 data_written, payload_size);

	event->data.rem_transfer_size -= data_written;

	if (event->data.transfer_eot) {
		dev_vdbg(mausb_host_dev.this_device, "transfer_size=%d, rem_transfer_size=%d, status=%d",
			 event->data.transfer_size,
			 event->data.rem_transfer_size, event->status);
		mausb_complete_request(urb, event->data.transfer_size -
				       event->data.rem_transfer_size,
				       event->status);
	}
}

static int
mausb_init_data_out_header_chunk(struct ma_usb_hdr_common *common_hdr,
				 struct list_head *chunks_list,
				 u32 *num_of_data_chunks)
{
	int status = mausb_add_data_chunk(common_hdr, MAUSB_TRANSFER_HDR_SIZE,
					  chunks_list);
	if (!status)
		++(*num_of_data_chunks);

	return status;
}

static int mausb_init_control_data_chunk(struct mausb_event *event,
					 struct list_head *chunks_list,
					 u32 *num_of_data_chunks)
{
	int status;
	void *buffer = ((struct urb *)event->data.urb)->setup_packet;

	if (!event->data.first_control_packet)
		return 0;

	status = mausb_add_data_chunk(buffer, MAUSB_CONTROL_SETUP_SIZE,
				      chunks_list);
	if (!status)
		++(*num_of_data_chunks);

	return status;
}

static int
mausb_prepare_transfer_packet(struct mausb_kvec_data_wrapper *wrapper,
			      struct mausb_event *event,
			      struct mausb_data_iter *iterator)
{
	u32 num_of_data_chunks		= 0;
	u32 num_of_payload_data_chunks	= 0;
	u32 payload_data_size		= 0;
	int status = 0;
	struct list_head chunks_list;
	struct list_head payload_data_chunks;
	struct ma_usb_hdr_common *data_hdr = (struct ma_usb_hdr_common *)
			event->data.hdr;

	INIT_LIST_HEAD(&chunks_list);

	/* Initialize data chunk for MAUSB header and add it to chunks list */
	if (mausb_init_data_out_header_chunk(data_hdr, &chunks_list,
					     &num_of_data_chunks) < 0) {
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

	/*
	 * Initialize data chunk for MAUSB control setup packet and
	 * add it to chunks list
	 */
	if (mausb_init_control_data_chunk(event, &chunks_list,
					  &num_of_data_chunks) < 0) {
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

	/* Get data chunks for data payload to send */
	INIT_LIST_HEAD(&payload_data_chunks);
	payload_data_size =
			((struct ma_usb_hdr_common *)event->data.hdr)->length -
			 MAUSB_TRANSFER_HDR_SIZE -
			 (event->data.first_control_packet ?
			  MAUSB_CONTROL_SETUP_SIZE : 0);

	if (mausb_data_iterator_read(iterator, payload_data_size,
				     &payload_data_chunks,
				     &num_of_payload_data_chunks) < 0) {
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

	list_splice_tail(&payload_data_chunks, &chunks_list);
	num_of_data_chunks += num_of_payload_data_chunks;

	/* Map all data chunks to data wrapper */
	if (mausb_init_data_wrapper(wrapper, &chunks_list,
				    num_of_data_chunks) < 0) {
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

cleanup_data_chunks: /* Cleanup all allocated data chunks */
	mausb_cleanup_chunks_list(&chunks_list);
	return status;
}

int mausb_send_out_data_msg(struct mausb_device *dev, struct mausb_event *event,
			    struct mausb_urb_ctx *urb_ctx)
{
	int status;
	struct mausb_kvec_data_wrapper data;
	enum mausb_channel channel;

	status = mausb_prepare_transfer_packet(&data, event,
					       &urb_ctx->iterator);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Failed to prepare transfer packet");
		return status;
	}

	channel = mausb_transfer_type_to_channel(event->data.transfer_type);
	status = mausb_send_data(dev, channel, &data);

	kfree(data.kvec);

	return status;
}

void mausb_receive_out_data(struct mausb_event *event,
			    struct mausb_urb_ctx *urb_ctx)
{
	struct urb *urb = urb_ctx->urb;

	dev_vdbg(mausb_host_dev.this_device, "transfer_size=%d, rem_transfer_size=%d, status=%d",
		 event->data.transfer_size, event->data.rem_transfer_size,
		 event->status);

	if (event->data.transfer_eot) {
		mausb_complete_request(urb, urb->transfer_buffer_length -
				       event->data.rem_transfer_size,
				       event->status);
	}
}

static inline u32
__mausb_isoch_prepare_read_size_block(struct ma_usb_hdr_isochreadsizeblock_std *
				      isoch_readsize_block, struct urb *urb)
{
	u32 i;
	u32 number_of_packets = (u32)urb->number_of_packets;

	if (number_of_packets == 0)
		return 0;

	isoch_readsize_block->service_intervals  = number_of_packets;
	isoch_readsize_block->max_segment_length =
					(u32)urb->iso_frame_desc[0].length;

	for (i = 0; i < number_of_packets; ++i) {
		urb->iso_frame_desc[i].status = 0;
		urb->iso_frame_desc[i].actual_length = 0;
	}

	return sizeof(struct ma_usb_hdr_isochreadsizeblock_std);
}

int mausb_send_isoch_in_msg(struct mausb_device *dev, struct mausb_event *event)
{
	u32 read_size_block_length = 0;
	struct mausb_kvec_data_wrapper data_to_send;
	struct kvec kvec[MAUSB_ISOCH_IN_KVEC_NUM];
	struct ma_usb_hdr_isochtransfer_optional opt_isoch_hdr;
	struct ma_usb_hdr_isochreadsizeblock_std isoch_readsize_block;
	struct ma_usb_hdr_common *hdr =
				(struct ma_usb_hdr_common *)event->data.hdr;
	struct urb *urb = (struct urb *)event->data.urb;
	enum mausb_channel channel;

	data_to_send.kvec_num	= 0;
	data_to_send.length	= 0;

	/* Prepare transfer header kvec */
	kvec[0].iov_base     = event->data.hdr;
	kvec[0].iov_len	     = MAUSB_TRANSFER_HDR_SIZE;
	data_to_send.length += (u32)kvec[0].iov_len;
	data_to_send.kvec_num++;

	/* Prepare optional header kvec */
	opt_isoch_hdr.timestamp = MA_USB_TRANSFER_RESERVED;
	opt_isoch_hdr.mtd	= MA_USB_TRANSFER_RESERVED;

	kvec[1].iov_base     = &opt_isoch_hdr;
	kvec[1].iov_len	     = sizeof(struct ma_usb_hdr_isochtransfer_optional);
	data_to_send.length += (u32)kvec[1].iov_len;
	data_to_send.kvec_num++;

	/* Prepare read size blocks */
	read_size_block_length =
		__mausb_isoch_prepare_read_size_block(&isoch_readsize_block,
						      urb);
	if (read_size_block_length > 0) {
		kvec[2].iov_base     = &isoch_readsize_block;
		kvec[2].iov_len	     = read_size_block_length;
		data_to_send.length += (u32)kvec[2].iov_len;
		data_to_send.kvec_num++;
	}

	hdr->length = (u16)data_to_send.length;
	data_to_send.kvec = kvec;

	channel = mausb_transfer_type_to_channel(event->data.transfer_type);
	return mausb_send_data(dev, channel, &data_to_send);
}

static void __mausb_process_in_isoch_short_resp(struct mausb_event *event,
						struct ma_usb_hdr_common *hdr,
						struct mausb_urb_ctx *urb_ctx)
{
	u8 opt_hdr_shift = (hdr->flags & MA_USB_HDR_FLAGS_TIMESTAMP) ?
			   sizeof(struct ma_usb_hdr_isochtransfer_optional) : 0;
	struct ma_usb_hdr_isochdatablock_short *data_block_hdr =
			(struct ma_usb_hdr_isochdatablock_short *)
			shift_ptr(mausb_hdr_isochtransfer_optional_hdr(hdr),
				  opt_hdr_shift);
	u8 *isoch_data = shift_ptr(data_block_hdr, hdr->data.headers *
				   sizeof(*data_block_hdr));
	u8 *end_of_packet = shift_ptr(hdr, hdr->length);
	struct urb *urb = urb_ctx->urb;
	int i;

	if (isoch_data >= end_of_packet) {
		dev_err(mausb_host_dev.this_device, "Bad header data. Data start pointer after end of packet: ep_handle=%#x",
			event->data.ep_handle);
		return;
	}

	for (i = 0; i < hdr->data.headers; ++i) {
		u16 seg_num  = data_block_hdr[i].segment_number;
		u16 seg_size = data_block_hdr[i].block_length;

		if (seg_num >= urb->number_of_packets) {
			dev_err(mausb_host_dev.this_device, "Too many segments: ep_handle=%#x, seg_num=%d, urb.number_of_packets=%d",
				event->data.ep_handle, seg_num,
				urb->number_of_packets);
			break;
		}

		if (seg_size > urb->iso_frame_desc[seg_num].length) {
			dev_err(mausb_host_dev.this_device, "Block to long for segment: ep_handle=%#x",
				event->data.ep_handle);
			break;
		}

		if (shift_ptr(isoch_data, seg_size) > end_of_packet) {
			dev_err(mausb_host_dev.this_device, "End of segment after enf of packet: ep_handle=%#x",
				event->data.ep_handle);
			break;
		}

		mausb_reset_data_iterator(&urb_ctx->iterator);
		mausb_data_iterator_seek(&urb_ctx->iterator,
					 urb->iso_frame_desc[seg_num].offset);
		mausb_data_iterator_write(&urb_ctx->iterator, isoch_data,
					  seg_size);

		isoch_data = shift_ptr(isoch_data, seg_size);

		urb->iso_frame_desc[seg_num].actual_length = seg_size;
		urb->iso_frame_desc[seg_num].status = 0;
	}
}

static void __mausb_process_in_isoch_std_resp(struct mausb_event *event,
					      struct ma_usb_hdr_common *hdr,
					      struct mausb_urb_ctx *urb_ctx)
{
	u8 opt_hdr_shift = (hdr->flags & MA_USB_HDR_FLAGS_TIMESTAMP) ?
			   sizeof(struct ma_usb_hdr_isochtransfer_optional) : 0;
	struct ma_usb_hdr_isochdatablock_std *data_block_hdr =
		(struct ma_usb_hdr_isochdatablock_std *)
		shift_ptr(mausb_hdr_isochtransfer_optional_hdr(hdr),
			  opt_hdr_shift);
	u8 *isoch_data =
		shift_ptr(data_block_hdr, hdr->data.headers *
			  sizeof(struct ma_usb_hdr_isochdatablock_std));
	u8 *end_of_packet = shift_ptr(hdr, hdr->length);
	struct urb *urb = (struct urb *)event->data.urb;
	int i;

	if (isoch_data >= end_of_packet) {
		dev_err(mausb_host_dev.this_device, "Bad header data. Data start pointer after end of packet: ep_handle=%#x",
			event->data.ep_handle);
		return;
	}

	for (i = 0; i < hdr->data.headers; ++i) {
		u16 seg_num   = data_block_hdr[i].segment_number;
		u16 seg_len   = data_block_hdr[i].segment_length;
		u16 block_len = data_block_hdr[i].block_length;

		if (seg_num >= urb->number_of_packets) {
			dev_err(mausb_host_dev.this_device, "Too many segments: ep_handle=%#x, seg_num=%d, number_of_packets=%d",
				event->data.ep_handle, seg_num,
				urb->number_of_packets);
			break;
		}

		if (block_len > urb->iso_frame_desc[seg_num].length -
			     urb->iso_frame_desc[seg_num].actual_length) {
			dev_err(mausb_host_dev.this_device, "Block too long for segment: ep_handle=%#x",
				event->data.ep_handle);
			break;
		}

		if (shift_ptr(isoch_data, block_len) >
				       end_of_packet) {
			dev_err(mausb_host_dev.this_device, "End of fragment after end of packet: ep_handle=%#x",
				event->data.ep_handle);
			break;
		}

		mausb_reset_data_iterator(&urb_ctx->iterator);
		mausb_data_iterator_seek(&urb_ctx->iterator,
					 urb->iso_frame_desc[seg_num].offset +
					 data_block_hdr[i].fragment_offset);
		mausb_data_iterator_write(&urb_ctx->iterator,
					  isoch_data, block_len);
		isoch_data = shift_ptr(isoch_data, block_len);

		urb->iso_frame_desc[seg_num].actual_length += block_len;

		if (urb->iso_frame_desc[seg_num].actual_length == seg_len)
			urb->iso_frame_desc[seg_num].status = 0;
	}
}

void mausb_receive_isoch_in_data(struct mausb_device *dev,
				 struct mausb_event *event,
				 struct mausb_urb_ctx *urb_ctx)
{
	struct ma_usb_hdr_common *common_hdr =
			(struct ma_usb_hdr_common *)event->data.recv_buf;
	struct ma_usb_hdr_transfer *transfer_hdr =
					mausb_get_data_transfer_hdr(common_hdr);

	if (!(common_hdr->data.i_flags & MA_USB_DATA_IFLAGS_FMT_MASK)) {
		/* Short ISO headers response */
		__mausb_process_in_isoch_short_resp(event, common_hdr, urb_ctx);
	} else if ((common_hdr->data.i_flags & MA_USB_DATA_IFLAGS_FMT_MASK) &
		MA_USB_DATA_IFLAGS_HDR_FMT_STD) {
		/* Standard ISO headers response */
		__mausb_process_in_isoch_std_resp(event, common_hdr, urb_ctx);
	} else if ((common_hdr->data.i_flags & MA_USB_DATA_IFLAGS_FMT_MASK) &
		MA_USB_DATA_IFLAGS_HDR_FMT_LONG) {
		/* Long ISO headers response */
		dev_warn(mausb_host_dev.this_device, "Long isoc headers in response: ep_handle=%#x, req_id=%#x",
			 event->data.ep_handle, transfer_hdr->req_id);
	} else {
		/* Error */
		dev_err(mausb_host_dev.this_device, "Isoc header error in response: ep_handle=%#x, req_id=%#x",
			event->data.ep_handle, transfer_hdr->req_id);
	}
}

static inline u32
__mausb_calculate_isoch_common_header_size(u32 num_of_segments)
{
	return MAUSB_ISOCH_TRANSFER_HDR_SIZE +
			MAUSB_ISOCH_STANDARD_FORMAT_SIZE * num_of_segments;
}

static struct ma_usb_hdr_common *
__mausb_create_isoch_out_transfer_packet(struct mausb_event *event,
					 struct mausb_urb_ctx *urb_ctx,
					 u16 payload_size, u32 seq_n,
					 u32 start_of_segments,
					 u32 number_of_segments)
{
	struct ma_usb_hdr_common		 *hdr;
	struct ma_usb_hdr_isochtransfer		 *hdr_isochtransfer;
	struct ma_usb_hdr_isochdatablock_std	 *isoc_header_std;
	struct ma_usb_hdr_isochtransfer_optional *hdr_opt_isochtransfer;
	struct urb *urb = (struct urb *)event->data.urb;
	void *isoc_headers = NULL;
	u32 length;
	u16 i;
	unsigned long block_length;
	u32 number_of_packets = (u32)event->data.isoch_seg_num;
	u32 size_of_request =
		__mausb_calculate_isoch_common_header_size(number_of_segments);

	hdr = kzalloc(size_of_request, GFP_KERNEL);
	if (!hdr)
		return NULL;

	hdr->version	  = MA_USB_HDR_VERSION_1_0;
	hdr->ssid	  = event->data.mausb_ssid;
	hdr->flags	  = MA_USB_HDR_FLAGS_HOST;
	hdr->dev_addr	  = event->data.mausb_address;
	hdr->handle.epv	  = event->data.ep_handle;
	hdr->data.status  = MA_USB_HDR_STATUS_NO_ERROR;
	hdr->data.eps	  = MAUSB_TRANSFER_RESERVED;
	hdr->data.t_flags = (u8)(usb_endpoint_type(&urb->ep->desc) << 3);

	isoc_headers = shift_ptr(hdr, MAUSB_ISOCH_TRANSFER_HDR_SIZE);

	for (i = (u16)start_of_segments;
	     i < number_of_segments + start_of_segments; ++i) {
		block_length = i < number_of_packets - 1 ?
			urb->iso_frame_desc[i + 1].offset -
			urb->iso_frame_desc[i].offset :
			mausb_data_iterator_length(&urb_ctx->iterator) -
			urb->iso_frame_desc[i].offset;

		urb->iso_frame_desc[i].status = MA_USB_HDR_STATUS_UNSUCCESSFUL;
		isoc_header_std = (struct ma_usb_hdr_isochdatablock_std *)
			shift_ptr(isoc_headers,
				  (u64)MAUSB_ISOCH_STANDARD_FORMAT_SIZE *
				  (i - start_of_segments));
		isoc_header_std->block_length	 = (u16)block_length;
		isoc_header_std->segment_number  = i;
		isoc_header_std->s_flags	 = 0;
		isoc_header_std->segment_length  = (u16)block_length;
		isoc_header_std->fragment_offset = 0;
	}

	length = __mausb_calculate_isoch_common_header_size(number_of_segments);

	hdr->flags		|= MA_USB_HDR_FLAGS_TIMESTAMP;
	hdr->type		 = (u8)MA_USB_HDR_TYPE_DATA_REQ(ISOCHTRANSFER);
	hdr->data.headers	 = (u16)number_of_segments;
	hdr->data.i_flags	 = MA_USB_DATA_IFLAGS_HDR_FMT_STD |
				      MA_USB_DATA_IFLAGS_ASAP;
	hdr_opt_isochtransfer	    = mausb_hdr_isochtransfer_optional_hdr(hdr);
	hdr_isochtransfer	    = mausb_get_isochtransfer_hdr(hdr);
	hdr_isochtransfer->req_id   = event->data.req_id;
	hdr_isochtransfer->seq_n    = seq_n;
	hdr_isochtransfer->segments = number_of_packets;

	hdr_isochtransfer->presentation_time = MA_USB_TRANSFER_RESERVED;

	hdr_opt_isochtransfer->timestamp = MA_USB_TRANSFER_RESERVED;
	hdr_opt_isochtransfer->mtd	 = MA_USB_TRANSFER_RESERVED;

	hdr->length = (u16)length + payload_size;

	return hdr;
}

static int
mausb_init_isoch_out_header_chunk(struct ma_usb_hdr_common *common_hdr,
				  struct list_head *chunks_list,
				  u32 *num_of_data_chunks,
				  u32 num_of_packets)
{
	u32 header_size =
		__mausb_calculate_isoch_common_header_size(num_of_packets);
	int status = mausb_add_data_chunk(common_hdr, header_size, chunks_list);

	if (!status)
		++(*num_of_data_chunks);

	return status;
}

static
int mausb_prepare_isoch_out_transfer_packet(struct ma_usb_hdr_common *hdr,
					    struct mausb_event *event,
					    struct mausb_urb_ctx *urb_ctx,
					    struct mausb_kvec_data_wrapper *
					    result_data_wrapper)
{
	u32 num_of_data_chunks	       = 0;
	u32 num_of_payload_data_chunks = 0;
	u32 segment_number	       = event->data.isoch_seg_num;
	u32 payload_data_size;
	struct list_head chunks_list;
	struct list_head payload_data_chunks;
	int status = 0;

	INIT_LIST_HEAD(&chunks_list);

	/* Initialize data chunk for MAUSB header and add it to chunks list */
	if (mausb_init_isoch_out_header_chunk(hdr, &chunks_list,
					      &num_of_data_chunks,
					      segment_number) < 0) {
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

	/* Get data chunks for data payload to send */
	INIT_LIST_HEAD(&payload_data_chunks);
	payload_data_size = hdr->length -
		__mausb_calculate_isoch_common_header_size(segment_number);

	if (mausb_data_iterator_read(&urb_ctx->iterator, payload_data_size,
				     &payload_data_chunks,
				     &num_of_payload_data_chunks) < 0) {
		dev_err(mausb_host_dev.this_device, "Data iterator read failed");
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

	list_splice_tail(&payload_data_chunks, &chunks_list);
	num_of_data_chunks += num_of_payload_data_chunks;

	/* Map all data chunks to data wrapper */
	if (mausb_init_data_wrapper(result_data_wrapper, &chunks_list,
				    num_of_data_chunks) < 0) {
		dev_err(mausb_host_dev.this_device, "Data wrapper init failed");
		status = -ENOMEM;
		goto cleanup_data_chunks;
	}

cleanup_data_chunks:
	mausb_cleanup_chunks_list(&chunks_list);
	return status;
}

static int mausb_create_and_send_isoch_transfer_req(struct mausb_device *dev,
						    struct mausb_event *event,
						    struct mausb_urb_ctx
						    *urb_ctx, u32 *seq_n,
						    u32 payload_size,
						    u32 start_of_segments,
						    u32 number_of_segments)
{
	struct ma_usb_hdr_common *hdr;
	struct mausb_kvec_data_wrapper data_to_send;
	int status;
	enum mausb_channel channel;

	hdr = __mausb_create_isoch_out_transfer_packet(event, urb_ctx,
						       (u16)payload_size,
						       *seq_n,
						       start_of_segments,
						       number_of_segments);
	if (!hdr) {
		dev_alert(mausb_host_dev.this_device, "Isoch transfer packet alloc failed");
		return -ENOMEM;
	}
	*seq_n = (*seq_n + 1) % (MA_USB_TRANSFER_SEQN_MAX + 1);

	status = mausb_prepare_isoch_out_transfer_packet(hdr, event, urb_ctx,
							 &data_to_send);
	if (status < 0) {
		dev_alert(mausb_host_dev.this_device, "Failed to prepare transfer packet");
		kfree(hdr);
		return status;
	}

	channel = mausb_transfer_type_to_channel(event->data.transfer_type);
	status = mausb_send_data(dev, channel, &data_to_send);

	kfree(hdr);
	kfree(data_to_send.kvec);

	return status;
}

static inline int __mausb_send_isoch_out_packet(struct mausb_device *dev,
						struct mausb_event *event,
						struct mausb_urb_ctx *urb_ctx,
						u32 *seq_n,
						u32 *starting_segments,
						u32 *rem_transfer_buf,
						u32 *payload_size, u32 index)
{
	int status = mausb_create_and_send_isoch_transfer_req(dev, event,
					urb_ctx, seq_n, *payload_size,
					*starting_segments,
					index - *starting_segments);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "ISOCH transfer request create and send failed");
		return status;
	}
	*starting_segments = index;
	*rem_transfer_buf  = MAX_ISOCH_ASAP_PACKET_SIZE;
	*payload_size	   = 0;

	return 0;
}

int mausb_send_isoch_out_msg(struct mausb_device *ma_dev,
			     struct mausb_event *mausb_event,
			     struct mausb_urb_ctx *urb_ctx)
{
	u32   starting_segments = 0;
	u32   rem_transfer_buf  = MAX_ISOCH_ASAP_PACKET_SIZE;
	struct urb *urb = (struct urb *)mausb_event->data.urb;
	u32 number_of_packets = (u32)urb->number_of_packets;
	u32 payload_size   = 0;
	u32 chunk_size;
	u32 seq_n	   = 0;
	int status;
	u32 i;

	for (i = 0; i < number_of_packets; ++i) {
		if (i < number_of_packets - 1)
			chunk_size = urb->iso_frame_desc[i + 1].offset -
					urb->iso_frame_desc[i].offset;
		else
			chunk_size =
				mausb_data_iterator_length(&urb_ctx->iterator) -
						urb->iso_frame_desc[i].offset;

		if (chunk_size + MAUSB_ISOCH_STANDARD_FORMAT_SIZE >
		    rem_transfer_buf) {
			if (payload_size == 0) {
				dev_warn(mausb_host_dev.this_device, "Fragmented");
			} else {
				status = __mausb_send_isoch_out_packet
						(ma_dev, mausb_event, urb_ctx,
						 &seq_n, &starting_segments,
						 &rem_transfer_buf,
						 &payload_size, i);
				if (status < 0)
					return status;
				i--;
				continue;
			}
		} else {
			rem_transfer_buf -=
				chunk_size + MAUSB_ISOCH_STANDARD_FORMAT_SIZE;
			payload_size += chunk_size;
		}

		if (i == number_of_packets - 1 || rem_transfer_buf == 0) {
			status = __mausb_send_isoch_out_packet
					(ma_dev, mausb_event, urb_ctx, &seq_n,
					 &starting_segments, &rem_transfer_buf,
					 &payload_size, i + 1);
			if (status < 0)
				return status;
		}
	}
	return 0;
}

void mausb_receive_isoch_out(struct mausb_event *event)
{
	struct urb *urb = (struct urb *)event->data.urb;
	u16 i;

	dev_vdbg(mausb_host_dev.this_device, "transfer_size=%d, rem_transfer_size=%d, status=%d",
		 event->data.transfer_size, event->data.rem_transfer_size,
		 event->status);

	for (i = 0; i < urb->number_of_packets; ++i)
		urb->iso_frame_desc[i].status = event->status;

	mausb_complete_request(urb, event->data.payload_size, event->status);
}
