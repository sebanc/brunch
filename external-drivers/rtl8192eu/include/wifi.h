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
#ifndef _WIFI_H_
#define _WIFI_H_


#ifndef BIT
#define BIT(x)	(1 << (x))
#endif


#define WLAN_ETHHDR_LEN		14
#define WLAN_ETHADDR_LEN	6
#define WLAN_IEEE_OUI_LEN	3
#define WLAN_ADDR_LEN		6
#define WLAN_CRC_LEN		4
#define WLAN_BSSID_LEN		6
#define WLAN_BSS_TS_LEN		8
#define WLAN_HDR_A3_LEN		24
#define WLAN_HDR_A4_LEN		30
#define WLAN_HDR_A3_QOS_LEN	26
#define WLAN_HDR_A4_QOS_LEN	32
#define WLAN_SSID_MAXLEN	32
#define WLAN_DATA_MAXLEN	2312

#define WLAN_A3_PN_OFFSET	24
#define WLAN_A4_PN_OFFSET	30

#define WLAN_MIN_ETHFRM_LEN	60
#define WLAN_MAX_ETHFRM_LEN	1514
#define WLAN_ETHHDR_LEN		14
#define WLAN_WMM_LEN		24
#define VENDOR_NAME_LEN		20

#ifdef CONFIG_APPEND_VENDOR_IE_ENABLE
#define WLAN_MAX_VENDOR_IE_LEN 255
#define WLAN_MAX_VENDOR_IE_NUM 5
#define WIFI_BEACON_VENDOR_IE_BIT BIT(0)
#define WIFI_PROBEREQ_VENDOR_IE_BIT BIT(1)
#define WIFI_PROBERESP_VENDOR_IE_BIT BIT(2)
#define WIFI_ASSOCREQ_VENDOR_IE_BIT BIT(3)
#define WIFI_ASSOCRESP_VENDOR_IE_BIT BIT(4)
#ifdef CONFIG_P2P
#define WIFI_P2P_PROBEREQ_VENDOR_IE_BIT BIT(5)
#define WIFI_P2P_PROBERESP_VENDOR_IE_BIT BIT(6)
#define WLAN_MAX_VENDOR_IE_MASK_MAX 7
#else
#define WLAN_MAX_VENDOR_IE_MASK_MAX 5
#endif
#endif

#define P80211CAPTURE_VERSION	0x80211001

/* This value is tested by WiFi 11n Test Plan 5.2.3.
 * This test verifies the WLAN NIC can update the NAV through sending the CTS with large duration. */
#define	WiFiNavUpperUs				30000	/* 30 ms */

#ifdef GREEN_HILL
#pragma pack(1)
#endif

/*ieee80211 management */
#define WIFI_ACTION_NOACK (BIT(7) | BIT(6) | BIT(5) | IEEE80211_FTYPE_MGMT)
/* below is for control frame */
#define WIFI_NDPA (BIT(6) | BIT(4) | IEEE80211_FTYPE_CTL)

/* Reason codes (IEEE 802.11-2007, 7.3.1.7, Table 7-22) */
enum {
	_STATS_REFUSED_TEMPORARILY_ = 30, /* Code-22-65535 reserved */
};
/* IEEE 802.11r */
#define WLAN_STATUS_INVALID_PMKID 53

#define SetToDs(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_TODS); \
	} while (0)

#define GetToDs(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_TODS)) != 0)

#define SetFrDs(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_FROMDS); \
	} while (0)

#define GetFrDs(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_FROMDS)) != 0)

#define get_tofr_ds(pframe)	((GetToDs(pframe) << 1) | GetFrDs(pframe))


#define SetMFrag(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_MOREFRAGS); \
	} while (0)

#define GetMFrag(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_MOREFRAGS)) != 0)

#define ClearMFrag(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) &= (~cpu_to_le16(IEEE80211_FCTL_MOREFRAGS)); \
	} while (0)

#define GetRetry(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_RETRY)) != 0)

#define ClearRetry(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) &= (~cpu_to_le16(IEEE80211_FCTL_RETRY)); \
	} while (0)

#define SetPwrMgt(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_PM); \
	} while (0)

#define GetPwrMgt(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_PM)) != 0)

#define ClearPwrMgt(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) &= (~cpu_to_le16(IEEE80211_FCTL_PM)); \
	} while (0)

#define SetMData(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_MOREDATA); \
	} while (0)

#define GetMData(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_MOREDATA)) != 0)

#define ClearMData(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) &= (~cpu_to_le16(IEEE80211_FCTL_MOREDATA)); \
	} while (0)

#define SetPrivacy(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_PROTECTED); \
	} while (0)

#define GetPrivacy(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_PROTECTED)) != 0)

#define GetOrder(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_ORDER)) != 0)

#define GetFrameType(pbuf)	(le16_to_cpu(*(unsigned short *)(pbuf)) & (BIT(3) | BIT(2)))

#define SetFrameType(pbuf, type)	\
	do {	\
		*(unsigned short *)(pbuf) &= __constant_cpu_to_le16(~(BIT(3) | BIT(2))); \
		*(unsigned short *)(pbuf) |= __constant_cpu_to_le16(type); \
	} while (0)

#define get_frame_sub_type(pbuf)	(cpu_to_le16(*(unsigned short *)(pbuf)) & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2)))


#define set_frame_sub_type(pbuf, type) \
	do {    \
		*(unsigned short *)(pbuf) &= cpu_to_le16(~(BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2))); \
		*(unsigned short *)(pbuf) |= cpu_to_le16(type); \
	} while (0)


#define GetSequence(pbuf)	(cpu_to_le16(*(unsigned short *)((SIZE_PTR)(pbuf) + 22)) >> 4)

#define GetFragNum(pbuf)	(cpu_to_le16(*(unsigned short *)((SIZE_PTR)(pbuf) + 22)) & 0x0f)

#define GetTupleCache(pbuf)	(cpu_to_le16(*(unsigned short *)((SIZE_PTR)(pbuf) + 22)))

#define SetFragNum(pbuf, num) \
	do {    \
		*(unsigned short *)((SIZE_PTR)(pbuf) + 22) = \
			((*(unsigned short *)((SIZE_PTR)(pbuf) + 22)) & le16_to_cpu(~(0x000f))) | \
				cpu_to_le16(0x0f & (num));     \
	} while (0)

#define SetSeqNum(pbuf, num) \
	do {    \
		*(unsigned short *)((SIZE_PTR)(pbuf) + 22) = \
			((*(unsigned short *)((SIZE_PTR)(pbuf) + 22)) & le16_to_cpu((unsigned short)~0xfff0)) | \
			le16_to_cpu((unsigned short)(0xfff0 & (num << 4))); \
	} while (0)

#define set_duration(pbuf, dur) \
	do {    \
		*(unsigned short *)((SIZE_PTR)(pbuf) + 2) = cpu_to_le16(0xffff & (dur)); \
	} while (0)


/* QoS control field */
#define SetPriority(qc, tid)	SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 0, 4, tid)
#define SetEOSP(qc, eosp)		SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 4, 1, eosp)
#define SetAckpolicy(qc, ack)	SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 5, 2, ack)
#define SetAMsdu(qc, amsdu)		SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 7, 1, amsdu)

#define GetPriority(qc)		LE_BITS_TO_2BYTE(((u8 *)(qc)), 0, 4)
#define GetEOSP(qc)			LE_BITS_TO_2BYTE(((u8 *)(qc)), 4, 1)
#define GetAckpolicy(qc)	LE_BITS_TO_2BYTE(((u8 *)(qc)), 5, 2)
#define GetAMsdu(qc)		LE_BITS_TO_2BYTE(((u8 *)(qc)), 7, 1)

/* QoS control field (MSTA only) */
#define set_mctrl_present(qc, p)	SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 8, 1, p)
#define set_mps_lv(qc, lv)			SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 9, 1, lv)
#define set_rspi(qc, rspi)			SET_BITS_TO_LE_2BYTE(((u8 *)(qc)), 10, 1, rspi)

#define get_mctrl_present(qc)	LE_BITS_TO_2BYTE(((u8 *)(qc)), 8, 1)
#define get_mps_lv(qc)			LE_BITS_TO_2BYTE(((u8 *)(qc)), 9, 1)
#define get_rspi(qc)			LE_BITS_TO_2BYTE(((u8 *)(qc)), 10, 1)


#define GetAid(pbuf)	(cpu_to_le16(*(unsigned short *)((SIZE_PTR)(pbuf) + 2)) & 0x3fff)

#define GetTid(pbuf)	(cpu_to_le16(*(unsigned short *)((SIZE_PTR)(pbuf) + (((GetToDs(pbuf)<<1) | GetFrDs(pbuf)) == 3 ? 30 : 24))) & 0x000f)

#define GetAddr1Ptr(pbuf)	((unsigned char *)((SIZE_PTR)(pbuf) + 4))

#define get_addr2_ptr(pbuf)	((unsigned char *)((SIZE_PTR)(pbuf) + 10))

#define GetAddr3Ptr(pbuf)	((unsigned char *)((SIZE_PTR)(pbuf) + 16))

#define GetAddr4Ptr(pbuf)	((unsigned char *)((SIZE_PTR)(pbuf) + 24))

__inline static unsigned char *get_ra(unsigned char *pframe)
{
	unsigned char	*ra;
	ra = GetAddr1Ptr(pframe);
	return ra;
}
__inline static unsigned char *get_ta(unsigned char *pframe)
{
	unsigned char	*ta;
	ta = get_addr2_ptr(pframe);
	return ta;
}

/* can't apply to mesh mode */
__inline static unsigned char *get_hdr_bssid(unsigned char *pframe)
{
	unsigned char	*sa = NULL;
	unsigned int	to_fr_ds	= (GetToDs(pframe) << 1) | GetFrDs(pframe);

	switch (to_fr_ds) {
	case 0x00:	/* ToDs=0, FromDs=0 */
		sa = GetAddr3Ptr(pframe);
		break;
	case 0x01:	/* ToDs=0, FromDs=1 */
		sa = get_addr2_ptr(pframe);
		break;
	case 0x02:	/* ToDs=1, FromDs=0 */
		sa = GetAddr1Ptr(pframe);
		break;
	case 0x03:	/* ToDs=1, FromDs=1 */
		sa = GetAddr1Ptr(pframe);
		break;
	}

	return sa;
}


__inline static int IsFrameTypeCtrl(unsigned char *pframe)
{
	if (IEEE80211_FTYPE_CTL == GetFrameType(pframe))
		return _TRUE;
	else
		return _FALSE;
}
static inline int IsFrameTypeMgnt(unsigned char *pframe)
{
	if (GetFrameType(pframe) == IEEE80211_FTYPE_MGMT)
		return _TRUE;
	else
		return _FALSE;
}
static inline int IsFrameTypeData(unsigned char *pframe)
{
	if (GetFrameType(pframe) == IEEE80211_FTYPE_DATA)
		return _TRUE;
	else
		return _FALSE;
}


/*-----------------------------------------------------------------------------
			Below is for the security related definition
------------------------------------------------------------------------------*/
#define _RESERVED_FRAME_TYPE_	0
#define _SKB_FRAME_TYPE_		2
#define _PRE_ALLOCMEM_			1
#define _PRE_ALLOCHDR_			3
#define _PRE_ALLOCLLCHDR_		4
#define _PRE_ALLOCICVHDR_		5
#define _PRE_ALLOCMICHDR_		6

#define _SIFSTIME_				((priv->pmib->dot11BssType.net_work_type&WIRELESS_11A) ? 16 : 10)
#define _ACKCTSLNG_				14	/* 14 bytes long, including crclng */
#define _CRCLNG_				4

#define _ASOCREQ_IE_OFFSET_		4	/* excluding wlan_hdr */
#define	_ASOCRSP_IE_OFFSET_		6
#define _REASOCREQ_IE_OFFSET_	10
#define _REASOCRSP_IE_OFFSET_	6
#define _PROBEREQ_IE_OFFSET_	0
#define	_PROBERSP_IE_OFFSET_	12
#define _AUTH_IE_OFFSET_		6
#define _DEAUTH_IE_OFFSET_		0
#define _BEACON_IE_OFFSET_		12
#define _PUBLIC_ACTION_IE_OFFSET_	8

#define _FIXED_IE_LENGTH_			_BEACON_IE_OFFSET_

/* ---------------------------------------------------------------------------
					Below is the fixed elements...
-----------------------------------------------------------------------------*/
#define _AUTH_ALGM_NUM_			2
#define _AUTH_SEQ_NUM_			2
#define _BEACON_ITERVAL_		2
#define _CAPABILITY_			2
#define _CURRENT_APADDR_		6
#define _LISTEN_INTERVAL_		2
#define _RSON_CODE_				2
#define _ASOC_ID_				2
#define _STATUS_CODE_			2
#define _TIMESTAMP_				8

#define AUTH_ODD_TO				0
#define AUTH_EVEN_TO			1

#define WLAN_ETHCONV_ENCAP		1
#define WLAN_ETHCONV_RFC1042	2
#define WLAN_ETHCONV_8021h		3

#define cap_ESS BIT(0)
#define cap_IBSS BIT(1)
#define cap_CFPollable BIT(2)
#define cap_CFRequest BIT(3)
#define cap_Privacy BIT(4)
#define cap_ShortPremble BIT(5)
#define cap_PBCC	BIT(6)
#define cap_ChAgility	BIT(7)
#define cap_SpecMgmt	BIT(8)
#define cap_QoS	BIT(9)
#define cap_ShortSlot	BIT(10)

/*-----------------------------------------------------------------------------
				Below is the definition for 802.11i / 802.1x
------------------------------------------------------------------------------*/
#define _IEEE8021X_MGT_			1		/* WPA */
#define _IEEE8021X_PSK_			2		/* WPA with pre-shared key */

#if 0
#define _NO_PRIVACY_			0
#define _WEP_40_PRIVACY_		1
#define _TKIP_PRIVACY_			2
#define _WRAP_PRIVACY_			3
#define _CCMP_PRIVACY_			4
#define _WEP_104_PRIVACY_		5
#define _WEP_WPA_MIXED_PRIVACY_ 6	/*  WEP + WPA */
#endif

#define _MME_IE_LENGTH_  18

/*-----------------------------------------------------------------------------
				Below is the definition for WMM
------------------------------------------------------------------------------*/
#define _WMM_IE_Length_				7  /* for WMM STA */
#define _WMM_Para_Element_Length_		24


/*-----------------------------------------------------------------------------
				Below is the definition for 802.11n
------------------------------------------------------------------------------*/

/* #ifdef CONFIG_80211N_HT */

#define set_order_bit(pbuf)	\
		do	{	\
			*(unsigned short *)(pbuf) |= cpu_to_le16(IEEE80211_FCTL_ORDER); \
		} while (0)



#define GetOrderBit(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(IEEE80211_FCTL_ORDER)) != 0)

#define ACT_CAT_VENDOR				0x7F/* 127 */

/**
 * struct ieee80211_ht_cap - HT additional information
 *
 * This structure refers to "HT information element" as
 * described in 802.11n draft section 7.3.2.53
 */
#ifndef CONFIG_IEEE80211_HT_ADDT_INFO
struct ieee80211_ht_addt_info {
	unsigned char	control_chan;
	unsigned char		ht_param;
	unsigned short	operation_mode;
	unsigned short	stbc_param;
	unsigned char		basic_set[16];
} __attribute__((packed));
#endif

struct HT_caps_element {
	union {
		struct {
			unsigned short	HT_caps_info;
			unsigned char	AMPDU_para;
			unsigned char	MCS_rate[16];
			unsigned short	HT_ext_caps;
			unsigned int	Beamforming_caps;
			unsigned char	ASEL_caps;
		} HT_cap_element;
		unsigned char HT_cap[26];
	} u;
} __attribute__((packed));

struct HT_info_element {
	unsigned char	primary_channel;
	unsigned char	infos[5];
	unsigned char	MCS_rate[16];
}  __attribute__((packed));

struct AC_param {
	unsigned char		ACI_AIFSN;
	unsigned char		CW;
	unsigned short	TXOP_limit;
}  __attribute__((packed));

struct WMM_para_element {
	unsigned char		QoS_info;
	unsigned char		reserved;
	struct AC_param	ac_param[4];
}  __attribute__((packed));

struct ADDBA_request {
	unsigned char		dialog_token;
	unsigned short	BA_para_set;
	unsigned short	BA_timeout_value;
	unsigned short	BA_starting_seqctrl;
}  __attribute__((packed));



/* 802.11n HT capabilities masks */
#define IEEE80211_HT_CAP_RX_STBC_1R 0x0100

/*
 * A-PMDU buffer sizes
 * According to IEEE802.11n spec size varies from 8K to 64K (in powers of 2)
 */
#ifndef IEEE80211_MIN_AMPDU_BUF
    #define IEEE80211_MIN_AMPDU_BUF 0x8
#endif
#ifndef IEEE80211_MAX_AMPDU_BUF
    #define IEEE80211_MAX_AMPDU_BUF 0x40
#endif

#define HT_INFO_OPERATION_MODE_OP_MODE_MASK	\
	((u16) (0x0001 | 0x0002))

/* #endif */

/*	===============WPS Section=============== */
/*	For WPSv1.0 */
#define WPSOUI							0x0050f204
/*	WPS attribute ID */
#define WPS_ATTR_VER1					0x104A
#define WPS_ATTR_SIMPLE_CONF_STATE	0x1044
#define WPS_ATTR_RESP_TYPE			0x103B
#define WPS_ATTR_UUID_E				0x1047
#define WPS_ATTR_MANUFACTURER		0x1021
#define WPS_ATTR_MODEL_NAME			0x1023
#define WPS_ATTR_MODEL_NUMBER		0x1024
#define WPS_ATTR_SERIAL_NUMBER		0x1042
#define WPS_ATTR_PRIMARY_DEV_TYPE	0x1054
#define WPS_ATTR_SEC_DEV_TYPE_LIST	0x1055
#define WPS_ATTR_DEVICE_NAME			0x1011
#define WPS_ATTR_CONF_METHOD			0x1008
#define WPS_ATTR_RF_BANDS				0x103C
#define WPS_ATTR_DEVICE_PWID			0x1012
#define WPS_ATTR_REQUEST_TYPE			0x103A
#define WPS_ATTR_ASSOCIATION_STATE	0x1002
#define WPS_ATTR_CONFIG_ERROR			0x1009
#define WPS_ATTR_VENDOR_EXT			0x1049
#define WPS_ATTR_SELECTED_REGISTRAR	0x1041

/*	Value of WPS attribute "WPS_ATTR_DEVICE_NAME */
#define WPS_MAX_DEVICE_NAME_LEN		32

/*	Value of WPS Request Type Attribute */
#define WPS_REQ_TYPE_ENROLLEE_INFO_ONLY			0x00
#define WPS_REQ_TYPE_ENROLLEE_OPEN_8021X		0x01
#define WPS_REQ_TYPE_REGISTRAR					0x02
#define WPS_REQ_TYPE_WLAN_MANAGER_REGISTRAR	0x03

/*	Value of WPS Response Type Attribute */
#define WPS_RESPONSE_TYPE_INFO_ONLY	0x00
#define WPS_RESPONSE_TYPE_8021X		0x01
#define WPS_RESPONSE_TYPE_REGISTRAR	0x02
#define WPS_RESPONSE_TYPE_AP			0x03

/*	Value of WPS WiFi Simple Configuration State Attribute */
#define WPS_WSC_STATE_NOT_CONFIG	0x01
#define WPS_WSC_STATE_CONFIG			0x02

/*	Value of WPS Version Attribute */
#define WPS_VERSION_1					0x10

/*	Value of WPS Configuration Method Attribute */
#define WPS_CONFIG_METHOD_FLASH		0x0001
#define WPS_CONFIG_METHOD_ETHERNET	0x0002
#define WPS_CONFIG_METHOD_LABEL		0x0004
#define WPS_CONFIG_METHOD_DISPLAY	0x0008
#define WPS_CONFIG_METHOD_E_NFC		0x0010
#define WPS_CONFIG_METHOD_I_NFC		0x0020
#define WPS_CONFIG_METHOD_NFC		0x0040
#define WPS_CONFIG_METHOD_PBC		0x0080
#define WPS_CONFIG_METHOD_KEYPAD	0x0100
#define WPS_CONFIG_METHOD_VPBC		0x0280
#define WPS_CONFIG_METHOD_PPBC		0x0480
#define WPS_CONFIG_METHOD_VDISPLAY	0x2008
#define WPS_CONFIG_METHOD_PDISPLAY	0x4008

/*	Value of Category ID of WPS Primary Device Type Attribute */
#define WPS_PDT_CID_DISPLAYS			0x0007
#define WPS_PDT_CID_MULIT_MEDIA		0x0008
#define WPS_PDT_CID_RTK_WIDI			WPS_PDT_CID_MULIT_MEDIA

/*	Value of Sub Category ID of WPS Primary Device Type Attribute */
#define WPS_PDT_SCID_MEDIA_SERVER	0x0005
#define WPS_PDT_SCID_RTK_DMP			WPS_PDT_SCID_MEDIA_SERVER

/*	Value of Device Password ID */
#define WPS_DPID_PIN					0x0000
#define WPS_DPID_USER_SPEC			0x0001
#define WPS_DPID_MACHINE_SPEC			0x0002
#define WPS_DPID_REKEY					0x0003
#define WPS_DPID_PBC					0x0004
#define WPS_DPID_REGISTRAR_SPEC		0x0005

/*	Value of WPS RF Bands Attribute */
#define WPS_RF_BANDS_2_4_GHZ		0x01
#define WPS_RF_BANDS_5_GHZ		0x02

/*	Value of WPS Association State Attribute */
#define WPS_ASSOC_STATE_NOT_ASSOCIATED			0x00
#define WPS_ASSOC_STATE_CONNECTION_SUCCESS		0x01
#define WPS_ASSOC_STATE_CONFIGURATION_FAILURE	0x02
#define WPS_ASSOC_STATE_ASSOCIATION_FAILURE		0x03
#define WPS_ASSOC_STATE_IP_FAILURE				0x04

/*	=====================P2P Section===================== */
/*	For P2P */
#define	P2POUI							0x506F9A09

/*	P2P Attribute ID */
#define	P2P_ATTR_STATUS					0x00
#define	P2P_ATTR_MINOR_REASON_CODE		0x01
#define	P2P_ATTR_CAPABILITY				0x02
#define	P2P_ATTR_DEVICE_ID				0x03
#define	P2P_ATTR_GO_INTENT				0x04
#define	P2P_ATTR_CONF_TIMEOUT			0x05
#define	P2P_ATTR_LISTEN_CH				0x06
#define	P2P_ATTR_GROUP_BSSID				0x07
#define	P2P_ATTR_EX_LISTEN_TIMING		0x08
#define	P2P_ATTR_INTENDED_IF_ADDR		0x09
#define	P2P_ATTR_MANAGEABILITY			0x0A
#define	P2P_ATTR_CH_LIST					0x0B
#define	P2P_ATTR_NOA						0x0C
#define	P2P_ATTR_DEVICE_INFO				0x0D
#define	P2P_ATTR_GROUP_INFO				0x0E
#define	P2P_ATTR_GROUP_ID					0x0F
#define	P2P_ATTR_INTERFACE				0x10
#define	P2P_ATTR_OPERATING_CH			0x11
#define	P2P_ATTR_INVITATION_FLAGS		0x12

/*	Value of Status Attribute */
#define	P2P_STATUS_SUCCESS						0x00
#define	P2P_STATUS_FAIL_INFO_UNAVAILABLE		0x01
#define	P2P_STATUS_FAIL_INCOMPATIBLE_PARAM		0x02
#define	P2P_STATUS_FAIL_LIMIT_REACHED			0x03
#define	P2P_STATUS_FAIL_INVALID_PARAM			0x04
#define	P2P_STATUS_FAIL_REQUEST_UNABLE			0x05
#define	P2P_STATUS_FAIL_PREVOUS_PROTO_ERR		0x06
#define	P2P_STATUS_FAIL_NO_COMMON_CH			0x07
#define	P2P_STATUS_FAIL_UNKNOWN_P2PGROUP		0x08
#define	P2P_STATUS_FAIL_BOTH_GOINTENT_15		0x09
#define	P2P_STATUS_FAIL_INCOMPATIBLE_PROVSION	0x0A
#define	P2P_STATUS_FAIL_USER_REJECT				0x0B

/*	Value of Inviation Flags Attribute */
#define	P2P_INVITATION_FLAGS_PERSISTENT			BIT(0)

#define	DMP_P2P_DEVCAP_SUPPORT	(P2P_DEVCAP_SERVICE_DISCOVERY | \
				 P2P_DEVCAP_CLIENT_DISCOVERABILITY | \
				 P2P_DEVCAP_CONCURRENT_OPERATION | \
				 P2P_DEVCAP_INVITATION_PROC)

#define	DMP_P2P_GRPCAP_SUPPORT	(P2P_GRPCAP_INTRABSS)

/*	Value of Device Capability Bitmap */
#define	P2P_DEVCAP_SERVICE_DISCOVERY		BIT(0)
#define	P2P_DEVCAP_CLIENT_DISCOVERABILITY	BIT(1)
#define	P2P_DEVCAP_CONCURRENT_OPERATION	BIT(2)
#define	P2P_DEVCAP_INFRA_MANAGED			BIT(3)
#define	P2P_DEVCAP_DEVICE_LIMIT				BIT(4)
#define	P2P_DEVCAP_INVITATION_PROC			BIT(5)

/*	Value of Group Capability Bitmap */
#define	P2P_GRPCAP_GO							BIT(0)
#define	P2P_GRPCAP_PERSISTENT_GROUP			BIT(1)
#define	P2P_GRPCAP_GROUP_LIMIT				BIT(2)
#define	P2P_GRPCAP_INTRABSS					BIT(3)
#define	P2P_GRPCAP_CROSS_CONN				BIT(4)
#define	P2P_GRPCAP_PERSISTENT_RECONN		BIT(5)
#define	P2P_GRPCAP_GROUP_FORMATION			BIT(6)

/*	P2P Public Action Frame ( Management Frame ) */
#define	P2P_PUB_ACTION_ACTION				0x09

/*	P2P Public Action Frame Type */
#define	P2P_GO_NEGO_REQ						0
#define	P2P_GO_NEGO_RESP						1
#define	P2P_GO_NEGO_CONF						2
#define	P2P_INVIT_REQ							3
#define	P2P_INVIT_RESP							4
#define	P2P_DEVDISC_REQ						5
#define	P2P_DEVDISC_RESP						6
#define	P2P_PROVISION_DISC_REQ				7
#define	P2P_PROVISION_DISC_RESP				8

/*	P2P Action Frame Type */
#define	P2P_NOTICE_OF_ABSENCE	0
#define	P2P_PRESENCE_REQUEST		1
#define	P2P_PRESENCE_RESPONSE	2
#define	P2P_GO_DISC_REQUEST		3


#define	P2P_MAX_PERSISTENT_GROUP_NUM		10

#define	P2P_PROVISIONING_SCAN_CNT			3

#define	P2P_WILDCARD_SSID_LEN				7

#define	P2P_FINDPHASE_EX_NONE				0	/* default value, used when: (1)p2p disabed or (2)p2p enabled but only do 1 scan phase */
#define	P2P_FINDPHASE_EX_FULL				1	/* used when p2p enabled and want to do 1 scan phase and P2P_FINDPHASE_EX_MAX-1 find phase */
#define	P2P_FINDPHASE_EX_SOCIAL_FIRST		(P2P_FINDPHASE_EX_FULL+1)
#define	P2P_FINDPHASE_EX_MAX					4
#define	P2P_FINDPHASE_EX_SOCIAL_LAST		P2P_FINDPHASE_EX_MAX

#define	P2P_PROVISION_TIMEOUT				5000	/*	5 seconds timeout for sending the provision discovery request */
#define	P2P_CONCURRENT_PROVISION_TIMEOUT	3000	/*	3 seconds timeout for sending the provision discovery request under concurrent mode */
#define	P2P_GO_NEGO_TIMEOUT					5000	/*	5 seconds timeout for receiving the group negotation response */
#define	P2P_CONCURRENT_GO_NEGO_TIMEOUT		3000	/*	3 seconds timeout for sending the negotiation request under concurrent mode */
#define	P2P_TX_PRESCAN_TIMEOUT				100		/*	100ms */
#define	P2P_INVITE_TIMEOUT					5000	/*	5 seconds timeout for sending the invitation request */
#define	P2P_CONCURRENT_INVITE_TIMEOUT		3000	/*	3 seconds timeout for sending the invitation request under concurrent mode */
#define	P2P_RESET_SCAN_CH						25000	/*	25 seconds timeout to reset the scan channel (based on channel plan) */
#define	P2P_MAX_INTENT						15

#define	P2P_MAX_NOA_NUM						2

/*	WPS Configuration Method */
#define	WPS_CM_NONE							0x0000
#define	WPS_CM_LABEL							0x0004
#define	WPS_CM_DISPLYA						0x0008
#define	WPS_CM_EXTERNAL_NFC_TOKEN			0x0010
#define	WPS_CM_INTEGRATED_NFC_TOKEN		0x0020
#define	WPS_CM_NFC_INTERFACE					0x0040
#define	WPS_CM_PUSH_BUTTON					0x0080
#define	WPS_CM_KEYPAD						0x0100
#define	WPS_CM_SW_PUHS_BUTTON				0x0280
#define	WPS_CM_HW_PUHS_BUTTON				0x0480
#define	WPS_CM_SW_DISPLAY_PIN				0x2008
#define	WPS_CM_LCD_DISPLAY_PIN				0x4008

enum P2P_ROLE {
	P2P_ROLE_DISABLE = 0,
	P2P_ROLE_DEVICE = 1,
	P2P_ROLE_CLIENT = 2,
	P2P_ROLE_GO = 3
};

enum P2P_STATE {
	P2P_STATE_NONE = 0,							/*	P2P disable */
	P2P_STATE_IDLE = 1,								/*	P2P had enabled and do nothing ,  buddy adapters is linked */
	P2P_STATE_LISTEN = 2,							/*	In pure listen state */
	P2P_STATE_SCAN = 3,							/*	In scan phase */
	P2P_STATE_FIND_PHASE_LISTEN = 4,				/*	In the listen state of find phase */
	P2P_STATE_FIND_PHASE_SEARCH = 5,				/*	In the search state of find phase */
	P2P_STATE_TX_PROVISION_DIS_REQ = 6,			/*	In P2P provisioning discovery */
	P2P_STATE_RX_PROVISION_DIS_RSP = 7,
	P2P_STATE_RX_PROVISION_DIS_REQ = 8,
	P2P_STATE_GONEGO_ING = 9,						/*	Doing the group owner negoitation handshake */
	P2P_STATE_GONEGO_OK = 10,						/*	finish the group negoitation handshake with success */
	P2P_STATE_GONEGO_FAIL = 11,					/*	finish the group negoitation handshake with failure */
	P2P_STATE_RECV_INVITE_REQ_MATCH = 12,		/*	receiving the P2P Inviation request and match with the profile. */
	P2P_STATE_PROVISIONING_ING = 13,				/*	Doing the P2P WPS */
	P2P_STATE_PROVISIONING_DONE = 14,			/*	Finish the P2P WPS */
	P2P_STATE_TX_INVITE_REQ = 15,					/*	Transmit the P2P Invitation request */
	P2P_STATE_RX_INVITE_RESP_OK = 16,				/*	Receiving the P2P Invitation response */
	P2P_STATE_RECV_INVITE_REQ_DISMATCH = 17,	/*	receiving the P2P Inviation request and dismatch with the profile. */
	P2P_STATE_RECV_INVITE_REQ_GO = 18,			/*	receiving the P2P Inviation request and this wifi is GO. */
	P2P_STATE_RECV_INVITE_REQ_JOIN = 19,			/*	receiving the P2P Inviation request to join an existing P2P Group. */
	P2P_STATE_RX_INVITE_RESP_FAIL = 20,			/*	recveing the P2P Inviation response with failure */
	P2P_STATE_RX_INFOR_NOREADY = 21,			/* receiving p2p negoitation response with information is not available */
	P2P_STATE_TX_INFOR_NOREADY = 22,			/* sending p2p negoitation response with information is not available */
};

enum P2P_WPSINFO {
	P2P_NO_WPSINFO						= 0,
	P2P_GOT_WPSINFO_PEER_DISPLAY_PIN	= 1,
	P2P_GOT_WPSINFO_SELF_DISPLAY_PIN	= 2,
	P2P_GOT_WPSINFO_PBC					= 3,
};

#define	P2P_PRIVATE_IOCTL_SET_LEN		64

enum P2P_PROTO_WK_ID {
	P2P_FIND_PHASE_WK = 0,
	P2P_RESTORE_STATE_WK = 1,
	P2P_PRE_TX_PROVDISC_PROCESS_WK = 2,
	P2P_PRE_TX_NEGOREQ_PROCESS_WK = 3,
	P2P_PRE_TX_INVITEREQ_PROCESS_WK = 4,
	P2P_AP_P2P_CH_SWITCH_PROCESS_WK = 5,
	P2P_RO_CH_WK = 6,
	P2P_CANCEL_RO_CH_WK = 7,
};

#ifdef CONFIG_P2P_PS
enum P2P_PS_STATE {
	P2P_PS_DISABLE = 0,
	P2P_PS_ENABLE = 1,
	P2P_PS_SCAN = 2,
	P2P_PS_SCAN_DONE = 3,
	P2P_PS_ALLSTASLEEP = 4, /* for P2P GO */
};

enum P2P_PS_MODE {
	P2P_PS_NONE = 0,
	P2P_PS_CTWINDOW = 1,
	P2P_PS_NOA	 = 2,
	P2P_PS_MIX = 3, /* CTWindow and NoA */
};
#endif /* CONFIG_P2P_PS */

/*	=====================WFD Section=====================
 *	For Wi-Fi Display */
#define	WFD_ATTR_DEVICE_INFO			0x00
#define	WFD_ATTR_ASSOC_BSSID			0x01
#define	WFD_ATTR_COUPLED_SINK_INFO	0x06
#define	WFD_ATTR_LOCAL_IP_ADDR		0x08
#define	WFD_ATTR_SESSION_INFO		0x09
#define	WFD_ATTR_ALTER_MAC			0x0a

/*	For WFD Device Information Attribute */
#define	WFD_DEVINFO_SOURCE					0x0000
#define	WFD_DEVINFO_PSINK					0x0001
#define	WFD_DEVINFO_SSINK					0x0002
#define	WFD_DEVINFO_DUAL					0x0003

#define	WFD_DEVINFO_SESSION_AVAIL			0x0010
#define	WFD_DEVINFO_WSD						0x0040
#define	WFD_DEVINFO_PC_TDLS					0x0080
#define	WFD_DEVINFO_HDCP_SUPPORT			0x0100

#define IP_MCAST_MAC(mac)		((mac[0] == 0x01) && (mac[1] == 0x00) && (mac[2] == 0x5e))
#define ICMPV6_MCAST_MAC(mac)	((mac[0] == 0x33) && (mac[1] == 0x33) && (mac[2] != 0xff))

#ifdef CONFIG_IOCTL_CFG80211
/* Regulatroy Domain */
struct regd_pair_mapping {
	u16 reg_dmnenum;
	u16 reg_5ghz_ctl;
	u16 reg_2ghz_ctl;
};

struct rtw_regulatory {
	char alpha2[2];
	u16 country_code;
	u16 max_power_level;
	u32 tp_scale;
	u16 current_rd;
	u16 current_rd_ext;
	int16_t power_limit;
	struct regd_pair_mapping *regpair;
};
#endif

#ifdef CONFIG_WAPI_SUPPORT
#ifndef IW_AUTH_WAPI_VERSION_1
#define IW_AUTH_WAPI_VERSION_1		0x00000008
#endif
#ifndef IW_AUTH_KEY_MGMT_WAPI_PSK
#define IW_AUTH_KEY_MGMT_WAPI_PSK	0x04
#endif
#ifndef IW_AUTH_WAPI_ENABLED
#define IW_AUTH_WAPI_ENABLED		0x20
#endif
#ifndef IW_ENCODE_ALG_SM4
#define IW_ENCODE_ALG_SM4			0x20
#endif
#endif

#endif /* _WIFI_H_ */
