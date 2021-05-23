/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _RTW_IOCTL_H_
#define _RTW_IOCTL_H_

/*	00 - Success
*	11 - Error */
#define STATUS_SUCCESS				(0x00000000L)
#define STATUS_PENDING				(0x00000103L)

#define STATUS_UNSUCCESSFUL			(0xC0000001L)
#define STATUS_INSUFFICIENT_RESOURCES		(0xC000009AL)
#define STATUS_NOT_SUPPORTED			(0xC00000BBL)

#define uint_SUCCESS			((uint)STATUS_SUCCESS)
#define uint_PENDING			((uint)STATUS_PENDING)
#define uint_NOT_RECOGNIZED		((uint)0x00010001L)
#define uint_NOT_COPIED			((uint)0x00010002L)
#define uint_NOT_ACCEPTED		((uint)0x00010003L)
#define uint_CALL_ACTIVE			((uint)0x00010007L)

#define uint_FAILURE			((uint)STATUS_UNSUCCESSFUL)
#define uint_RESOURCES			((uint)STATUS_INSUFFICIENT_RESOURCES)
#define uint_CLOSING			((uint)0xC0010002L)
#define uint_BAD_VERSION			((uint)0xC0010004L)
#define uint_BAD_CHARACTERISTICS		((uint)0xC0010005L)
#define uint_ADAPTER_NOT_FOUND		((uint)0xC0010006L)
#define uint_OPEN_FAILED			((uint)0xC0010007L)
#define uint_DEVICE_FAILED		((uint)0xC0010008L)
#define uint_MULTICAST_FULL		((uint)0xC0010009L)
#define uint_MULTICAST_EXISTS		((uint)0xC001000AL)
#define uint_MULTICAST_NOT_FOUND		((uint)0xC001000BL)
#define uint_REQUEST_ABORTED		((uint)0xC001000CL)
#define uint_RESET_IN_PROGRESS		((uint)0xC001000DL)
#define uint_CLOSING_INDICATING		((uint)0xC001000EL)
#define uint_NOT_SUPPORTED		((uint)STATUS_NOT_SUPPORTED)
#define uint_INVALID_PACKET		((uint)0xC001000FL)
#define uint_OPEN_LIST_FULL		((uint)0xC0010010L)
#define uint_ADAPTER_NOT_READY		((uint)0xC0010011L)
#define uint_ADAPTER_NOT_OPEN		((uint)0xC0010012L)
#define uint_NOT_INDICATING		((uint)0xC0010013L)
#define uint_INVALID_LENGTH		((uint)0xC0010014L)
#define uint_INVALID_DATA		((uint)0xC0010015L)
#define uint_BUFFER_TOO_SHORT		((uint)0xC0010016L)
#define uint_INVALID_OID			((uint)0xC0010017L)
#define uint_ADAPTER_REMOVED		((uint)0xC0010018L)
#define uint_UNSUPPORTED_MEDIA		((uint)0xC0010019L)
#define uint_GROUP_ADDRESS_IN_USE	((uint)0xC001001AL)
#define uint_FILE_NOT_FOUND		((uint)0xC001001BL)
#define uint_ERROR_READING_FILE		((uint)0xC001001CL)
#define uint_ALREADY_MAPPED		((uint)0xC001001DL)
#define uint_RESOURCE_CONFLICT		((uint)0xC001001EL)
#define uint_NO_CABLE			((uint)0xC001001FL)

#define uint_INVALID_SAP			((uint)0xC0010020L)
#define uint_SAP_IN_USE			((uint)0xC0010021L)
#define uint_INVALID_ADDRESS		((uint)0xC0010022L)
#define uint_VC_NOT_ACTIVATED		((uint)0xC0010023L)
#define uint_DEST_OUT_OF_ORDER		((uint)0xC0010024L)  /* cause 27 */
#define uint_VC_NOT_AVAILABLE		((uint)0xC0010025L)  /* cause 35, 45 */
#define uint_CELLRATE_NOT_AVAILABLE	((uint)0xC0010026L)  /* cause 37 */
#define uint_INCOMPATABLE_QOS		((uint)0xC0010027L)  /* cause 49 */
#define uint_AAL_PARAMS_UNSUPPORTED	((uint)0xC0010028L)  /* cause 93 */
#define uint_NO_ROUTE_TO_DESTINATION	((uint)0xC0010029L)  /* cause 3 */

#ifndef OID_802_11_CAPABILITY
	#define OID_802_11_CAPABILITY                   0x0d010122
#endif

#ifndef OID_802_11_PMKID
	#define OID_802_11_PMKID                        0x0d010123
#endif


/* For DDK-defined OIDs */
#define OID_NDIS_SEG1	0x00010100
#define OID_NDIS_SEG2	0x00010200
#define OID_NDIS_SEG3	0x00020100
#define OID_NDIS_SEG4	0x01010100
#define OID_NDIS_SEG5	0x01020100
#define OID_NDIS_SEG6	0x01020200
#define OID_NDIS_SEG7	0xFD010100
#define OID_NDIS_SEG8	0x0D010100
#define OID_NDIS_SEG9	0x0D010200
#define OID_NDIS_SEG10	0x0D020200

#define SZ_OID_NDIS_SEG1		  23
#define SZ_OID_NDIS_SEG2		    3
#define SZ_OID_NDIS_SEG3		    6
#define SZ_OID_NDIS_SEG4		    6
#define SZ_OID_NDIS_SEG5		    4
#define SZ_OID_NDIS_SEG6		    8
#define SZ_OID_NDIS_SEG7		    7
#define SZ_OID_NDIS_SEG8		  36
#define SZ_OID_NDIS_SEG9		  24
#define SZ_OID_NDIS_SEG10		  19

/* For Realtek-defined OIDs */
#define OID_MP_SEG1		0xFF871100
#define OID_MP_SEG2		0xFF818000

#define OID_MP_SEG3		0xFF818700
#define OID_MP_SEG4		0xFF011100

enum oid_type {
	QUERY_OID,
	SET_OID
};

struct oid_funs_node {
	unsigned int oid_start; /* the starting number for OID */
	unsigned int oid_end; /* the ending number for OID */
	struct oid_obj_priv *node_array;
	unsigned int array_sz; /* the size of node_array */
	int query_counter; /* count the number of query hits for this segment  */
	int set_counter; /* count the number of set hits for this segment  */
};

struct oid_par_priv {
	void		*adapter_context;
	uint	oid;
	void		*information_buf;
	u32		information_buf_len;
	u32		*bytes_rw;
	u32		*bytes_needed;
	enum oid_type	type_of_oid;
	u32		dbg;
};

struct oid_obj_priv {
	unsigned char	dbg; /* 0: without OID debug message  1: with OID debug message */
	uint(*oidfuns)(struct oid_par_priv *poid_par_priv);
};

#if defined(_RTW_MP_IOCTL_C_)
static uint oid_null_function(struct oid_par_priv *poid_par_priv)
{
	return uint_SUCCESS;
}
#endif

#if defined(CONFIG_WIRELESS_EXT)
extern struct iw_handler_def  rtw_handlers_def;
#endif

extern void rtw_request_wps_pbc_event(struct adapter *adapt);

extern	uint drv_query_info(
	struct  net_device *		MiniportAdapterContext,
	uint		Oid,
	void			*InformationBuffer,
	u32			InformationBufferLength,
	u32			*BytesWritten,
	u32			*BytesNeeded
);

extern	uint	drv_set_info(
	struct  net_device *		MiniportAdapterContext,
	uint		Oid,
	void			*InformationBuffer,
	u32			InformationBufferLength,
	u32			*BytesRead,
	u32			*BytesNeeded
);

#endif /*  #ifndef __INC_CEINFO_ */
