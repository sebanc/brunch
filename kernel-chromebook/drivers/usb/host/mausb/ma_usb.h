/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_MA_USB_H__
#define __MAUSB_MA_USB_H__

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <types.h>
#endif /* __KERNEL__ */

#define MA_USB_SET_FIELD_(_m_, _v) __mausb_set_field(MA_USB_##_m_##_MASK, _v)
#define MA_USB_GET_FIELD_(_m_, _v) __mausb_get_field(MA_USB_##_m_##_MASK, _v)
#define MA_USB_SET_FIELD(_m_, _v) __mausb_set_field(MA_USB_##_m_##_MASK, \
						    MA_USB_##_v)
#define MA_USB_GET_FIELD(_m_, _v) __mausb_get_field(MA_USB_##_m_##_MASK, \
						    MA_USB_##_v)

#define MA_USB_MGMT_TOKEN_RESERVED  0
#define MA_USB_MGMT_TOKEN_MIN       1
#define MA_USB_MGMT_TOKEN_MAX       ((1 << 10) - 1)

#define MA_USB_DATA_EPS_UNASSIGNED  0
#define MA_USB_DATA_EPS_ACTIVE      1
#define MA_USB_DATA_EPS_INACTIVE    2
#define MA_USB_DATA_EPS_HALTED      3

#define MA_USB_DATA_TFLAGS_ARQ      1
#define MA_USB_DATA_TFLAGS_NEG      2
#define MA_USB_DATA_TFLAGS_EOT      4
#define MA_USB_DATA_TFLAGS_TRANSFER_TYPE_CTRL   0
#define MA_USB_DATA_TFLAGS_TRANSFER_TYPE_ISOCH  8
#define MA_USB_DATA_TFLAGS_TRANSFER_TYPE_BULK   16
#define MA_USB_DATA_TFLAGS_TRANSFER_TYPE_INTR   24

#define MA_USB_DATA_TFLAGS_TRANSFER_TYPE_MASK   0x18

#define MA_USB_DATA_IFLAGS_MTD_VALID      1
#define MA_USB_DATA_IFLAGS_HDR_FMT_SHORT  0
#define MA_USB_DATA_IFLAGS_HDR_FMT_STD    2
#define MA_USB_DATA_IFLAGS_HDR_FMT_LONG   4
#define MA_USB_DATA_IFLAGS_IRS_FMT_STD    0
#define MA_USB_DATA_IFLAGS_IRS_FMT_LONG   2
#define MA_USB_DATA_IFLAGS_ASAP           8

#define MA_USB_DATA_IFLAGS_FMT_MASK       0x6

/* version */

#define MA_USB_HDR_VERSION_1_0      0

/* flags */

#define MA_USB_HDR_FLAGS_HOST       1
#define MA_USB_HDR_FLAGS_RETRY      2
#define MA_USB_HDR_FLAGS_TIMESTAMP  4
#define MA_USB_HDR_FLAGS_RESERVED   8
#define MA_USB_HDR_FLAG(_f) MA_USB_HDR_FLAGS_##_f

/* type and subtype */

#define MA_USB_HDR_TYPE_TYPE_MASK     0xC0
#define MA_USB_HDR_TYPE_SUBTYPE_MASK  0x3F

#define MA_USB_HDR_TYPE_TYPE_MANAGEMENT 0
#define MA_USB_HDR_TYPE_TYPE_CONTROL    1
#define MA_USB_HDR_TYPE_TYPE_DATA       2

/* Management subtypes */

#define _MA_USB_HDR_TYPE_MANAGEMENT_REQ(_s) \
	MA_USB_SET_FIELD_(HDR_TYPE_SUBTYPE, (_s) * 2)
#define _MA_USB_HDR_TYPE_MANAGEMENT_RESP(_s) \
	MA_USB_SET_FIELD_(HDR_TYPE_SUBTYPE, (_s) * 2 + 1)

#define MA_USB_HDR_TYPE_MANAGEMENT_REQ(_s) \
	_MA_USB_HDR_TYPE_MANAGEMENT_REQ(MA_USB_HDR_TYPE_SUBTYPE_##_s)
#define MA_USB_HDR_TYPE_MANAGEMENT_RESP(_s) \
	_MA_USB_HDR_TYPE_MANAGEMENT_RESP(MA_USB_HDR_TYPE_SUBTYPE_##_s)

#define MA_USB_HDR_TYPE_SUBTYPE_CAP               0
#define MA_USB_HDR_TYPE_SUBTYPE_USBDEVHANDLE      1
#define MA_USB_HDR_TYPE_SUBTYPE_EPHANDLE          2
#define MA_USB_HDR_TYPE_SUBTYPE_EPACTIVATE        3
#define MA_USB_HDR_TYPE_SUBTYPE_EPINACTIVATE      4
#define MA_USB_HDR_TYPE_SUBTYPE_EPRESET           5
#define MA_USB_HDR_TYPE_SUBTYPE_CLEARTRANSFERS    6
#define MA_USB_HDR_TYPE_SUBTYPE_EPHANDLEDELETE    7
#define MA_USB_HDR_TYPE_SUBTYPE_DEVRESET          8
#define MA_USB_HDR_TYPE_SUBTYPE_MODIFYEP0         9
#define MA_USB_HDR_TYPE_SUBTYPE_SETUSBDEVADDR     10
#define MA_USB_HDR_TYPE_SUBTYPE_UPDATEDEV         11
#define MA_USB_HDR_TYPE_SUBTYPE_USBDEVDISCONNECT  12
#define MA_USB_HDR_TYPE_SUBTYPE_USBSUSPEND        13
#define MA_USB_HDR_TYPE_SUBTYPE_USBRESUME         14
#define MA_USB_HDR_TYPE_SUBTYPE_REMOTEWAKE        15
#define MA_USB_HDR_TYPE_SUBTYPE_PING              16
#define MA_USB_HDR_TYPE_SUBTYPE_DEVDISCONNECT     17
#define MA_USB_HDR_TYPE_SUBTYPE_DEVINITDISCONNECT 18
#define MA_USB_HDR_TYPE_SUBTYPE_SYNCH             19
#define MA_USB_HDR_TYPE_SUBTYPE_CANCELTRANSFER    20
#define MA_USB_HDR_TYPE_SUBTYPE_EPOPENSTREAM      21
#define MA_USB_HDR_TYPE_SUBTYPE_EPCLOSESTREAM     22
#define MA_USB_HDR_TYPE_SUBTYPE_USBDEVRESET       23
#define MA_USB_HDR_TYPE_SUBTYPE_DEVNOTIFICATION   24
#define MA_USB_HDR_TYPE_SUBTYPE_EPSETKEEPALIVE    25
#define MA_USB_HDR_TYPE_SUBTYPE_GETPORTBW         26
#define MA_USB_HDR_TYPE_SUBTYPE_SLEEP             27
#define MA_USB_HDR_TYPE_SUBTYPE_WAKE              28
#define MA_USB_HDR_TYPE_SUBTYPE_VENDORSPECIFIC    31 /* Reserved */

/* Data subtypes */

#define _MA_USB_HDR_TYPE_DATA_REQ(_s) ({ \
	typeof(_s) (s) = (_s); \
	MA_USB_SET_FIELD(HDR_TYPE_TYPE, HDR_TYPE_TYPE_DATA) | \
	MA_USB_SET_FIELD_(HDR_TYPE_SUBTYPE, (s) * 2 \
	+ ((s) > 0 ? 1 : 0)); })
#define _MA_USB_HDR_TYPE_DATA_RESP(_s) ({ \
	typeof(_s) (s) = (_s); \
	MA_USB_SET_FIELD(HDR_TYPE_TYPE, HDR_TYPE_TYPE_DATA) | \
	MA_USB_SET_FIELD_(HDR_TYPE_SUBTYPE, (s) * 2 + 1 \
	+ ((s) > 0 ? 1 : 0)); })
#define _MA_USB_HDR_TYPE_DATA_ACK(_s) ( \
	MA_USB_SET_FIELD(HDR_TYPE_TYPE, HDR_TYPE_TYPE_DATA) | \
	MA_USB_SET_FIELD_(HDR_TYPE_SUBTYPE, (_s) * 2 + 2))

#define MA_USB_HDR_TYPE_DATA_REQ(_s) \
	_MA_USB_HDR_TYPE_DATA_REQ(MA_USB_HDR_TYPE_SUBTYPE_##_s)
#define MA_USB_HDR_TYPE_DATA_RESP(_s) \
	_MA_USB_HDR_TYPE_DATA_RESP(MA_USB_HDR_TYPE_SUBTYPE_##_s)
#define MA_USB_HDR_TYPE_DATA_ACK(_s) \
	_MA_USB_HDR_TYPE_DATA_ACK(MA_USB_HDR_TYPE_SUBTYPE_##_s)

#define MA_USB_HDR_TYPE_SUBTYPE_TRANSFER          0
#define MA_USB_HDR_TYPE_SUBTYPE_ISOCHTRANSFER     1

/* EP/Device Handle */

#define MA_USB_DEVICE_HANDLE_RESERVED   0

#define MA_USB_EP_HANDLE_D_MASK     0x0001
#define MA_USB_EP_HANDLE_EP_N_MASK  0x001e
#define MA_USB_EP_HANDLE_ADDR_MASK  0x0fe0
#define MA_USB_EP_HANDLE_BUS_N_MASK 0xf000

#define MA_USB_EP_HANDLE(_b, _a, _e, _d) ( \
	MA_USB_SET_FIELD_(EP_HANDLE_BUS_N, _b)  | \
	MA_USB_SET_FIELD_(EP_HANDLE_ADDR, _a)   | \
	MA_USB_SET_FIELD_(EP_HANDLE_EP_N, _e)   | \
	MA_USB_SET_FIELD_(EP_HANDLE_D, _d))

#define MA_USB_EP_HANDLE_BUS_N_VIRTUAL  15
#define MA_USB_EP_HANDLE_ADDR_DEFAULT   0
#define MA_USB_EP_HANDLE_EP_N_DEFAULT   0
#define MA_USB_EP_HANDLE_D_OUT          0	/* See USB2.0 9.3, Table 9-2 */
#define MA_USB_EP_HANDLE_D_IN           1	/* See USB2.0 9.3, Table 9-2 */

/* Status codes */

#define MA_USB_HDR_STATUS_UNSUCCESSFUL                  -128
#define MA_USB_HDR_STATUS_INVALID_MA_USB_SESSION_STATE  -127
#define MA_USB_HDR_STATUS_INVALID_DEVICE_HANDLE         -126
#define MA_USB_HDR_STATUS_INVALID_EP_HANDLE             -125
#define MA_USB_HDR_STATUS_INVALID_EP_HANDLE_STATE       -124
#define MA_USB_HDR_STATUS_INVALID_REQUEST               -123
#define MA_USB_HDR_STATUS_MISSING_SEQUENCE_NUMBER       -122
#define MA_USB_HDR_STATUS_TRANSFER_PENDING              -121
#define MA_USB_HDR_STATUS_TRANSFER_EP_STALL             -120
#define MA_USB_HDR_STATUS_TRANSFER_SIZE_ERROR           -119
#define MA_USB_HDR_STATUS_TRANSFER_DATA_BUFFER_ERROR    -118
#define MA_USB_HDR_STATUS_TRANSFER_BABBLE_DETECTED      -117
#define MA_USB_HDR_STATUS_TRANSFER_TRANSACTION_ERROR    -116
#define MA_USB_HDR_STATUS_TRANSFER_SHORT_TRANSFER       -115
#define MA_USB_HDR_STATUS_TRANSFER_CANCELED             -114
#define MA_USB_HDR_STATUS_INSUFICIENT_RESOURCES         -113
#define MA_USB_HDR_STATUS_NOT_SUFFICIENT_BANDWIDTH      -112
#define MA_USB_HDR_STATUS_INTERNAL_ERROR                -111
#define MA_USB_HDR_STATUS_DATA_OVERRUN                  -110
#define MA_USB_HDR_STATUS_DEVICE_NOT_ACCESSED           -109
#define MA_USB_HDR_STATUS_BUFFER_OVERRUN                -108
#define MA_USB_HDR_STATUS_BUSY                          -107
#define MA_USB_HDR_STATUS_DROPPED_PACKET                -106
#define MA_USB_HDR_STATUS_ISOCH_TIME_EXPIRED            -105
#define MA_USB_HDR_STATUS_ISOCH_TIME_INVALID            -104
#define MA_USB_HDR_STATUS_NO_USB_PING_RESPONSE          -103
#define MA_USB_HDR_STATUS_NOT_SUPPORTED                 -102
#define MA_USB_HDR_STATUS_REQUEST_DENIED                -101
#define MA_USB_HDR_STATUS_MISSING_REQUEST_ID            -100
#define MA_USB_HDR_STATUS_SUCCESS                       0	/* Reserved */
#define MA_USB_HDR_STATUS_NO_ERROR MA_USB_HDR_STATUS_SUCCESS	/* Reserved */

/* Speed values */

#define MA_USB_SPEED_LOW_SPEED         0
#define MA_USB_SPEED_FULL_SPEED        1
#define MA_USB_SPEED_HIGH_SPEED        2
#define MA_USB_SPEED_SUPER_SPEED       3
#define MA_USB_SPEED_SUPER_SPEED_PLUS  4

/* capreq extra hdr */

#define MA_USB_CAPREQ_DESC_SYNCHRONIZATION_LENGTH\
	(sizeof(struct ma_usb_desc) +\
	sizeof(struct ma_usb_capreq_desc_synchronization))
#define MA_USB_CAPREQ_DESC_LINK_SLEEP_LENGTH\
	(sizeof(struct ma_usb_desc) +\
	sizeof(struct ma_usb_capreq_desc_link_sleep))

#define MA_USB_CAPREQ_LENGTH\
	(sizeof(struct ma_usb_hdr_common) +\
	sizeof(struct ma_usb_hdr_capreq) +\
	MA_USB_CAPREQ_DESC_SYNCHRONIZATION_LENGTH +\
	MA_USB_CAPREQ_DESC_LINK_SLEEP_LENGTH)

/* capreq desc types */

#define MA_USB_CAPREQ_DESC_TYPE_SYNCHRONIZATION 3
#define MA_USB_CAPREQ_DESC_TYPE_LINK_SLEEP      5

/* capresp descriptors */

#define MA_USB_CAPRESP_DESC_TYPE_SPEED            0
#define MA_USB_CAPRESP_DESC_TYPE_P_MANAGED_OUT    1
#define MA_USB_CAPRESP_DESC_TYPE_ISOCHRONOUS      2
#define MA_USB_CAPRESP_DESC_TYPE_SYNCHRONIZATION  3
#define MA_USB_CAPRESP_DESC_TYPE_CONTAINER_ID     4
#define MA_USB_CAPRESP_DESC_TYPE_LINK_SLEEP       5
#define MA_USB_CAPRESP_DESC_TYPE_HUB_LATENCY      6

/* Request ID and sequence number values */

#define MA_USB_TRANSFER_RESERVED      0
#define MA_USB_TRANSFER_REQID_MIN     0
#define MA_USB_TRANSFER_REQID_MAX     ((1 <<  8) - 1)
#define MA_USB_TRANSFER_SEQN_MIN      0
#define MA_USB_TRANSFER_SEQN_MAX      ((1 << 24) - 2)
#define MA_USB_TRANSFER_SEQN_INVALID  ((1 << 24) - 1)

#define MA_USB_ISOCH_SFLAGS_FRAGMENT      0x1
#define MA_USB_ISOCH_SFLAGS_LAST_FRAGMENT 0x2

#define MAUSB_MAX_MGMT_SIZE 50

#define MAUSB_TRANSFER_HDR_SIZE (u32)(sizeof(struct ma_usb_hdr_common) +\
				      sizeof(struct ma_usb_hdr_transfer))

#define MAUSB_ISOCH_TRANSFER_HDR_SIZE (u16)(sizeof(struct ma_usb_hdr_common) +\
			sizeof(struct ma_usb_hdr_isochtransfer) +\
			sizeof(struct ma_usb_hdr_isochtransfer_optional))

#define MAX_ISOCH_ASAP_PACKET_SIZE (150000 /* Network MTU */ -\
	MAUSB_ISOCH_TRANSFER_HDR_SIZE - 20 /* IP header size */ -\
	8 /* UDP header size*/)

#define shift_ptr(ptr, offset) ((u8 *)(ptr) + (offset))

/* USB descriptor */
struct ma_usb_desc {
	u8 length;
	u8 type;
	u8 value[0];
} __packed;

struct ma_usb_ep_handle {
	u16 d		:1,
	    ep_n	:4,
	    addr	:7,
	    bus_n	:4;
};

struct ma_usb_hdr_mgmt {
	u32 status	:8,
	    token	:10,  /* requestor originator allocated */
	    reserved	:14;
} __aligned(4);

struct ma_usb_hdr_ctrl {	/* used in all req/resp/conf operations */
	s8 status;
	u8 link_type;
	union {
		u8 tid;	/* ieee 802.11 */
	} connection_id;
} __aligned(4);

struct ma_usb_hdr_data {
	s8 status;
	u8 eps		:2,
	   t_flags	:6;
	union {
		u16 stream_id;
		struct {
			u16 headers	:12,
			    i_flags	:4;
		};
	};
} __aligned(4);

struct ma_usb_hdr_common {
	u8 version	:4,
	   flags	:4;
	u8  type;
	u16 length;
	union {
		u16 dev;
		u16 epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u8 dev_addr;
	u8 ssid;
	union {
		s8 status;
		struct ma_usb_hdr_mgmt mgmt;
		struct ma_usb_hdr_ctrl ctrl;
		struct ma_usb_hdr_data data;
	};
} __aligned(4);

/* capreq extra hdr */

struct ma_usb_hdr_capreq {
	u32 out_mgmt_reqs	:12,
	    reserved		:20;
} __aligned(4);

struct ma_usb_capreq_desc_synchronization {
	u8 media_time_available	:1,
	   reserved		:7;
} __packed;

struct ma_usb_capreq_desc_link_sleep {
	u8 link_sleep_capable	:1,
	   reserved		:7;
} __packed;

/* capresp extra hdr */

struct ma_usb_hdr_capresp {
	u16 endpoints;
	u8 devices;
	u8 streams		:5,
	   dev_type		:3;
	u32 descs		:8,
	    descs_length	:24;
	u16 out_transfer_reqs;
	u16 out_mgmt_reqs	:12,
	    reserved		:4;
} __aligned(4);

struct ma_usb_capresp_desc_speed {
	u8 reserved1		:4,
		speed		:4;
	u8 reserved2		:4,
	   lse			:2,	/* USB3.1 8.5.6.7, Table 8-22 */
	   reserved3		:2;
} __packed;

struct ma_usb_capresp_desc_p_managed_out {
	u8 elastic_buffer		:1,
	   drop_notification		:1,
	   reserved			:6;
} __packed;

struct ma_usb_capresp_desc_isochronous {
	u8 payload_dword_aligned	:1,
	   reserved			:7;
} __packed;

struct ma_usb_capresp_desc_synchronization {
	u8 media_time_available	:1,
	   time_stamp_required	:1,/* hubs need this set */
	   reserved		:6;
} __packed;

struct ma_usb_capresp_desc_container_id {
	u8 container_id[16];	/* UUID IETF RFC 4122 */
} __packed;

struct ma_usb_capresp_desc_link_sleep {
	u8 link_sleep_capable	:1,
	   reserved		:7;
} __packed;

struct ma_usb_capresp_desc_hub_latency {
	u16 latency;		/* USB3.1 */
} __packed;

/* usbdevhandlereq extra hdr */
struct ma_usb_hdr_usbdevhandlereq {
	u32 route_string	:20,
	    speed		:4,
	    reserved1		:8;
	u16 hub_dev_handle;
	u16 reserved2;
	u16 parent_hs_hub_dev_handle;
	u16 parent_hs_hub_port		:4,
	    mtt				:1,	/* USB2.0 11.14, 11.14.1.3 */
	    lse				:2,	/* USB3.1 8.5.6.7, Table 8-22 */
	    reserved3			:9;
} __aligned(4);

/* usbdevhandleresp extra hdr */
struct ma_usb_hdr_usbdevhandleresp {
	u16 dev_handle;
	u16 reserved;
} __aligned(4);

/* ephandlereq extra hdr */
struct ma_usb_hdr_ephandlereq {
	u32 ep_descs		:5,
	    ep_desc_size	:6,
	    reserved		:21;
} __aligned(4);

/*
 * Restricted USB2.0 ep desc limited to 6 bytes, isolating further changes.
 * See USB2.0 9.6.6, Table 9-13
 */
struct usb_ep_desc {
	u8 bLength;
	/* USB2.0 9.4, Table 9-5 (5) usb/ch9.h: USB_DT_ENDPOINT */
	u8 bDescriptorType;
	u8  bEndpointAddress;
	u8  bmAttributes;
	__le16 wMaxPacketSize;
	u8  bInterval;
} __packed;

/*
 * Restricted USB3.1 ep comp desc isolating further changes in usb/ch9.h
 * See USB3.1 9.6.7, Table 9-26
 */
struct usb_ss_ep_comp_desc {
	u8 bLength;
	/* USB3.1 9.4, Table 9-6 (48) usb/ch9.h: USB_DT_SS_ENDPOINT_COMP */
	u8  bDescriptorType;
	u8  bMaxBurst;
	u8  bmAttributes;
	__le16 wBytesPerInterval;
} __packed;

/*
 * USB3.1 ss_plus_isoch_ep_comp_desc
 * See USB3.1 9.6.8, Table 9-27
 */
struct usb_ss_plus_isoch_ep_comp_desc {
	u8 bLength;
	/* USB3.1 9.4, Table 9-6 (49) usb/ch9.h: not yet defined! */
	u8 bDescriptorType;
	u16 wReserved;
	u32 dwBytesPerInterval;
} __packed;

struct ma_usb_ephandlereq_desc_std {
	struct usb_ep_desc usb20;
} __aligned(4);

struct ma_usb_ephandlereq_desc_ss {
	struct usb_ep_desc	   usb20;
	struct usb_ss_ep_comp_desc usb31;
} __aligned(4);

struct ma_usb_ephandlereq_desc_ss_plus {
	struct usb_ep_desc		      usb20;
	struct usb_ss_ep_comp_desc	      usb31;
	struct usb_ss_plus_isoch_ep_comp_desc usb31_isoch;
} __aligned(4);

struct ma_usb_dev_context {
	struct usb_ep_desc usb;
};

/* ephandleresp extra hdr */
struct ma_usb_hdr_ephandleresp {
	u32 ep_descs :5,
	    reserved :27;
} __aligned(4);

/* ephandleresp descriptor */
struct ma_usb_ephandleresp_desc {
	union {
		struct ma_usb_ep_handle eph;
		u16		epv;
	} ep_handle;
	u16 d		:1,	/* non-control or non-OUT */
	    isoch	:1,
	    l_managed	:1,	/* control or non-isoch OUT */
	    invalid	:1,
	    reserved1	:12;
	u16 ccu;		/* control or non-isoch OUT */
	u16 reserved2;
	u32 buffer_size;	/* control or OUT */
	u16 isoch_prog_delay;	/* in us. */
	u16 isoch_resp_delay;	/* in us. */
} __aligned(4);

/* epactivatereq extra hdr */
struct ma_usb_hdr_epactivatereq {
	u32 ep_handles	:5,
	    reserved	:27;
	union {
		u16		epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* epactivateresp extra hdr */
struct ma_usb_hdr_epactivateresp {
	u32 ep_errors	:5,
	    reserved	:27;
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* epinactivatereq extra hdr */
struct ma_usb_hdr_epinactivatereq {
	u32 ep_handles	:5,
	    suspend	:1,
	    reserved	:26;
	union {
		u16		epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* epinactivateresp extra hdr */
struct ma_usb_hdr_epinactivateresp {
	u32 ep_errors	:5,
	    reserved	:27;
	union {
		u16		epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* epresetreq extra hdr */
struct ma_usb_hdr_epresetreq {
	u32 ep_reset_blocks	:5,
	    reserved		:27;
} __aligned(4);

/* epresetreq reset block */
struct ma_usb_epresetreq_block {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u16 tsp		:1,
	    reserved	:15;
} __aligned(4);

/* epresetresp extra hdr */
struct ma_usb_hdr_epresetresp {
	u32 ep_errors	:5,
	    reserved	:27;
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* cleartransfersreq extra hdr */
struct ma_usb_hdr_cleartransfersreq {
	u32 info_blocks	:8,
	    reserved	:24;
} __aligned(4);

/* cleartransfersreq info block */
struct ma_usb_cleartransfersreq_block {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u16 stream_id; /* ss stream eps only */
	u32 start_req_id	:8,
	    reserved		:24;
} __aligned(4);

/* cleartransfersresp extra hdr */
struct ma_usb_hdr_cleartransfersresp {
	u32 status_blocks	:8,
	    reserved		:24;
} __aligned(4);

/* cleartransfersresp status block */
struct ma_usb_cleartransfersresp_block {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u16 stream_id;	/* ss stream eps only */
	u32 cancel_success	:1,
	    partial_delivery	:1,
	    reserved		:30;
	u32 last_req_id		:8,
	    delivered_seq_n	:24;	/* OUT w/partial_delivery only */
	u32 delivered_byte_offset;	/* OUT w/partial_delivery only */
} __aligned(4);

/* ephandledeletereq extra hdr */
struct ma_usb_hdr_ephandledeletereq {
	u32 ep_handles	:5,
	    reserved	:27;
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* ephandledeleteresp extra hdr */
struct ma_usb_hdr_ephandledeleteresp {
	u32 ep_errors	:5,
	    reserved	:27;
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle[0];
} __aligned(4);

/* modifyep0req extra hdr */
struct ma_usb_hdr_modifyep0req {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u16 max_packet_size;
} __aligned(4);

/*
 * modifyep0resp extra hdr
 * Only if req ep0 handle addr was 0 and req dev is in the addressed state
 * or  if req ep0 handle addr != 0 and req dev is in default state
 */
struct ma_usb_hdr_modifyep0resp {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;

	u16 reserved;
} __aligned(4);

/* setusbdevaddrreq extra hdr */
struct ma_usb_hdr_setusbdevaddrreq {
	u16 response_timeout;	/* in ms. */
	u16 reserved;
} __aligned(4);

/* setusbdevaddrresp extra hdr */
struct ma_usb_hdr_setusbdevaddrresp {
	u32 addr	:7,
	    reserved	:25;
} __aligned(4);

/* updatedevreq extra hdr */
struct ma_usb_hdr_updatedevreq {
	u16 max_exit_latency;	/* hubs only */
	u8 hub		:1,
	   ports	:4,
	   mtt		:1,
	   ttt		:2;
	u8 integrated_hub_latency	:1,
	   reserved			:7;
} __aligned(4);

/*
 * USB2.0 dev desc, isolating further changes in usb/ch9.h
 * See USB2.0 9.6.6, Table 9-13
 */
struct usb_dev_desc {
	u8 bLength;
	/*
	 * USB2.0 9.4, Table 9-5 (1)
	 * usb/ch9.h: USB_DT_DEVICE
	 */
	u8 bDescriptorType;
	__le16 bcdUSB;
	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;
	__le16 idVendor;
	__le16 idProduct;
	__le16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;
	u8  iSerialNumber;
	u8  bNumConfigurations;
} __packed;

struct ma_usb_updatedevreq_desc {
	struct usb_dev_desc usb20;
} __aligned(4);

/* remotewakereq extra hdr */
struct ma_usb_hdr_remotewakereq {
	u32 resumed  :1,
		 reserved :31;
} __aligned(4);

/* synchreq/resp extra hdr */
struct ma_usb_hdr_synch {
	u32 mtd_valid		:1,	/* MA-USB1.0b 6.5.1.8, Table 66 */
	    resp_required	:1,
	    reserved		:30;
	union {
		u32 timestamp;		/* MA-USB1.0b 6.5.1.11 */
		struct {
			u32 delta		:13,
			    bus_interval	:19;
		};			/* MA-USB1.0b 6.6.1, Table 69 */
	};
	u32 mtd;			/* MA-USB1.0b 6.5.1.12 */
} __aligned(4);

/* canceltransferreq extra hdr */
struct ma_usb_hdr_canceltransferreq {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u16 stream_id;
	u32 req_id	  :8,
		 reserved :24;
} __aligned(4);

/* canceltransferresp extra hdr */
struct ma_usb_hdr_canceltransferresp {
	union {
		u16			epv;
		struct ma_usb_ep_handle eph;
	} handle;
	u16 stream_id;
	u32 req_id		:8,
	    cancel_status	:3,
	    reserved1		:21;
	u32 delivered_seq_n	:24,
	    reserved2		:8;
	u32 delivered_byte_offset;
} __aligned(4);

/* transferreq/resp/ack extra hdr */
struct ma_usb_hdr_transfer {
	u32 seq_n	:24,
	    req_id	:8;
	union {
		u32 rem_size_credit;
		/* ISOCH aliased fields added for convenience. */
		struct {
			u32 presentation_time	:20,
			    segments		:12;
		};
	};
} __aligned(4);

/* isochtransferreq/resp extra hdr */
struct ma_usb_hdr_isochtransfer {
	u32 seq_n		:24,
	    req_id		:8;
	u32 presentation_time	:20,
	    segments		:12;
} __aligned(4);

/* isochtransferreq/resp optional hdr */
struct ma_usb_hdr_isochtransfer_optional {
	union {
		u32 timestamp;	/* MA-USB1.0b 6.5.1.11 */
		struct {
			u32 delta		:13,
			    bus_interval	:19;
		};		/* MA-USB1.0b 6.6.1, Table 69 */
	};
	u32 mtd;		/* MA-USB1.0b 6.5.1.12 */
} __aligned(4);

/* isochdatablock hdrs */

struct ma_usb_hdr_isochdatablock_short {
	u16 block_length;
	u16 segment_number	:12,
	    s_flags		:4;
} __aligned(4);

struct ma_usb_hdr_isochdatablock_std {
	u16 block_length;
	u16 segment_number	:12,
	    s_flags		:4;
	u16 segment_length;
	u16 fragment_offset;
} __aligned(4);

struct ma_usb_hdr_isochdatablock_long {
	u16 block_length;
	u16 segment_number	:12,
	    s_flags		:4;
	u32 segment_length;
	u32 fragment_offset;
} __aligned(4);

/* isochreadsizeblock hdrs */

struct ma_usb_hdr_isochreadsizeblock_std {
	u32 service_intervals		:12,
	    max_segment_length		:20;
} __aligned(4);

struct ma_usb_hdr_isochreadsizeblock_long {
	u32 service_intervals		:12,
	    reserved			:20;
	u32 max_segment_length;
} __aligned(4);

static inline int __mausb_set_field(int m, int v)
{
	return ((~((m) - 1) & (m)) * (v)) & (m);
}

static inline int __mausb_get_field(int m, int v)
{
	return ((v) & (m)) / (~((m) - 1) & (m));
}

static inline bool mausb_is_management_hdr_type(int hdr_type)
{
	return MA_USB_GET_FIELD_(HDR_TYPE_TYPE, hdr_type)
			== MA_USB_HDR_TYPE_TYPE_MANAGEMENT;
}

static inline bool mausb_is_data_hdr_type(int hdr_type)
{
	return MA_USB_GET_FIELD_(HDR_TYPE_TYPE, hdr_type)
			== MA_USB_HDR_TYPE_TYPE_DATA;
}

static inline bool mausb_is_management_resp_hdr_type(int hdr_resp_type)
{
	return mausb_is_management_hdr_type(hdr_resp_type) &&
			(MA_USB_GET_FIELD_(HDR_TYPE_SUBTYPE, hdr_resp_type) & 1)
			!= 0;
}

static inline
struct ma_usb_hdr_transfer *
mausb_get_data_transfer_hdr(struct ma_usb_hdr_common *hdr)
{
	return (struct ma_usb_hdr_transfer *)shift_ptr(hdr, sizeof(*hdr));
}

static inline
struct ma_usb_hdr_isochtransfer *
mausb_get_isochtransfer_hdr(struct ma_usb_hdr_common *hdr)
{
	return (struct ma_usb_hdr_isochtransfer *)shift_ptr(hdr, sizeof(*hdr));
}

static inline
struct ma_usb_hdr_isochtransfer_optional *
mausb_hdr_isochtransfer_optional_hdr(struct ma_usb_hdr_common *hdr)
{
	return (struct ma_usb_hdr_isochtransfer_optional *)
			shift_ptr(hdr, sizeof(struct ma_usb_hdr_common) +
				       sizeof(struct ma_usb_hdr_isochtransfer));
}

#endif	/* __MAUSB_MA_USB_H__ */
