/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifndef _RTW_IOCTL_H_
#define _RTW_IOCTL_H_

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
	NDIS_OID	oid;
	void		*information_buf;
	u32		information_buf_len;
	u32		*bytes_rw;
	u32		*bytes_needed;
	enum oid_type	type_of_oid;
	u32		dbg;
};

struct oid_obj_priv {
	unsigned char	dbg; /* 0: without OID debug message  1: with OID debug message */
	NDIS_STATUS(*oidfuns)(struct oid_par_priv *poid_par_priv);
};

#if defined(PLATFORM_LINUX) && defined(CONFIG_WIRELESS_EXT)
extern struct iw_handler_def  rtw_handlers_def;
#endif

extern void rtw_request_wps_pbc_event(_adapter *padapter);

extern	NDIS_STATUS drv_query_info(
	IN	_nic_hdl		MiniportAdapterContext,
	IN	NDIS_OID		Oid,
	IN	void			*InformationBuffer,
	IN	u32			InformationBufferLength,
	OUT	u32			*BytesWritten,
	OUT	u32			*BytesNeeded
);

extern	NDIS_STATUS	drv_set_info(
	IN	_nic_hdl		MiniportAdapterContext,
	IN	NDIS_OID		Oid,
	IN	void			*InformationBuffer,
	IN	u32			InformationBufferLength,
	OUT	u32			*BytesRead,
	OUT	u32			*BytesNeeded
);

#ifdef CONFIG_APPEND_VENDOR_IE_ENABLE
extern int rtw_vendor_ie_get_raw_data(struct net_device *, u32, char *, u32);
extern int rtw_vendor_ie_get_data(struct net_device*, int , char*);
extern int rtw_vendor_ie_get(struct net_device *, struct iw_request_info *, union iwreq_data *, char *);
extern int rtw_vendor_ie_set(struct net_device*, struct iw_request_info*, union iwreq_data*, char*);
#endif

#endif /*  #ifndef __INC_CEINFO_ */
