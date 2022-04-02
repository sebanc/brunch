/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __WLAN_BSSDEF_H__
#define __WLAN_BSSDEF_H__


#define MAX_IE_SZ	768

#define NDIS_802_11_LENGTH_SSID         32
#define NDIS_802_11_LENGTH_RATES        8
#define NDIS_802_11_LENGTH_RATES_EX     16

typedef unsigned char   NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];        /* Set of 8 data rates */
typedef unsigned char   NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];  /* Set of 16 data rates */


struct ndis_802_11_ssid {
	u32  SsidLength;
	u8  Ssid[32];
};

enum ndis_802_11_network_type {
	Ndis802_11FH,
	Ndis802_11DS,
	Ndis802_11OFDM5,
	Ndis802_11OFDM24,
	Ndis802_11NetworkTypeMax    /* not a real type, defined as an upper bound */
};

struct ndis_802_11_configuration_fh {
	u32           Length;             /* Length of structure */
	u32           HopPattern;         /* As defined by 802.11, MSB set */
	u32           HopSet;             /* to one if non-802.11 */
	u32           DwellTime;          /* units are Kusec */
};

/*
	FW will only save the channel number in DSConfig.
	ODI Handler will convert the channel number to freq. number.
*/
struct ndis_802_11_configuration {
	u32           Length;             /* Length of structure */
	u32           BeaconPeriod;       /* units are Kusec */
	u32           ATIMWindow;         /* units are Kusec */
	u32           DSConfig;           /* channel number */
	struct ndis_802_11_configuration_fh    FHConfig;
};



enum ndis_802_11_network_infrastructure {
	Ndis802_11IBSS,
	Ndis802_11Infrastructure,
	Ndis802_11AutoUnknown,
	Ndis802_11InfrastructureMax,     /* Not a real value, defined as upper bound */
	Ndis802_11APMode,
	Ndis802_11Monitor,
};

struct ndis_802_11_fixed_ies {
	u8  Timestamp[8];
	u16  BeaconInterval;
	u16  Capabilities;
};

struct ndis_802_11_variable_ies {
	u8  ElementID;
	u8  Length;
	u8  data[1];
};

/*
Length is the 4 bytes multiples of the sume of
	sizeof (NDIS_802_11_MAC_ADDRESS) + 2 + sizeof (struct ndis_802_11_ssid) + sizeof (u32)
+   sizeof (long) + sizeof (enum ndis_802_11_network_type) + sizeof (struct ndis_802_11_configuration)
+   sizeof (NDIS_802_11_RATES_EX) + IELength

Except the IELength, all other fields are fixed length. Therefore, we can define a marco to present the
partial sum.

*/
enum ndis_802_11_authentication_mode {
	Ndis802_11AuthModeOpen,
	Ndis802_11AuthModeShared,
	Ndis802_11AuthModeAutoSwitch,
	Ndis802_11AuthModeWPA,
	Ndis802_11AuthModeWPAPSK,
	Ndis802_11AuthModeWPANone,
	Ndis802_11AuthModeWAPI,
	Ndis802_11AuthModeMax               /* Not a real mode, defined as upper bound */
}; 

enum ndis_802_11_wep_status {
	Ndis802_11WEPEnabled,
	Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
	Ndis802_11WEPDisabled,
	Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
	Ndis802_11WEPKeyAbsent,
	Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
	Ndis802_11WEPNotSupported,
	Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
	Ndis802_11Encryption2Enabled,
	Ndis802_11Encryption2KeyAbsent,
	Ndis802_11Encryption3Enabled,
	Ndis802_11Encryption3KeyAbsent,
	Ndis802_11_EncrypteionWAPI
};

#define NDIS_802_11_AI_REFQI_CAPABILITIES      1
#define NDIS_802_11_AI_REFQI_LISTENINTERVAL    2
#define NDIS_802_11_AI_REFQI_CURRENTAPADDRESS  4

#define NDIS_802_11_AI_RESET_CAPABILITIES      1
#define NDIS_802_11_AI_RESET_STATUSCODE        2
#define NDIS_802_11_AI_RESET_ASSOCIATIONID     4

struct ndis_802_11_ai_refqi {
	u16 Capabilities;
	u16 ListenInterval;
	unsigned char  CurrentAPAddress[6];
};

struct ndis_802_11_ai_reset {
	u16 Capabilities;
	u16 StatusCode;
	u16 AssociationId;
};

struct ndis_802_11_assoc_info {
	u32                   Length;
	u16                  AvailableRequestFixedIEs;
	struct ndis_802_11_ai_refqi    RequestFixedIEs;
	u32                   RequestIELength;
	u32                   OffsetRequestIEs;
	u16                  AvailableResponseFixedIEs;
	struct ndis_802_11_ai_reset    ResponseFixedIEs;
	u32                   ResponseIELength;
	u32                   OffsetResponseIEs;
};

enum ndis_802_11_reload_defaults {
	Ndis802_11ReloadWEPKeys
};

/* Key mapping keys require a BSSID */
struct ndis_802_11_key {
	u32           Length;             /* Length of this structure */
	u32           KeyIndex;
	u32           KeyLength;          /* length of key in bytes */
	unsigned char BSSID[6];
	unsigned long long KeyRSC;
	u8           KeyMaterial[32];     /* variable length depending on above field */
};

struct ndis_802_11_remove_key {
	u32                   Length;        /* Length of this structure */
	u32                   KeyIndex;
	unsigned char BSSID[6];
};

struct ndis_802_11_wep {
	u32     Length;        /* Length of this structure */
	u32     KeyIndex;      /* 0 is the per-client key, 1-N are the global keys */
	u32     KeyLength;     /* length of key in bytes */
	u8     KeyMaterial[16];/* variable length depending on above field */
};

struct ndis_801_11_authentication_request {
	u32 Length;            /* Length of structure */
	unsigned char Bssid[6];
	u32 Flags;
};

enum ndis_802_11_status_type {
	Ndis802_11StatusType_Authentication,
	Ndis802_11StatusType_MediaStreamMode,
	Ndis802_11StatusType_PMKID_CandidateList,
	Ndis802_11StatusTypeMax    /* not a real type, defined as an upper bound */
};

struct ndis_802_11_status_indication {
	enum ndis_802_11_status_type StatusType;
};

/* mask for authentication/integrity fields */
#define NDIS_802_11_AUTH_REQUEST_AUTH_FIELDS        0x0f
#define NDIS_802_11_AUTH_REQUEST_REAUTH			0x01
#define NDIS_802_11_AUTH_REQUEST_KEYUPDATE		0x02
#define NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR		0x06
#define NDIS_802_11_AUTH_REQUEST_GROUP_ERROR		0x0E

/* MIC check time, 60 seconds. */
#define MIC_CHECK_TIME	60000000

struct ndis_802_11_authentication_event {
	struct ndis_802_11_status_indication       Status;
	struct ndis_801_11_authentication_request  Request[1];
};

struct ndis_802_11_test {
	u32 Length;
	u32 Type;
	union {
		struct ndis_802_11_authentication_event AuthenticationEvent;
		long RssiTrigger;
	} tt;
};

#ifndef Ndis802_11APMode
#define Ndis802_11APMode (Ndis802_11InfrastructureMax+1)
#endif

struct wlan_phy_info {
	u8	SignalStrength;/* (in percentage) */
	u8	SignalQuality;/* (in percentage) */
	u8	Optimum_antenna;  /* for Antenna diversity */
	u8	is_cck_rate;	/* 1:cck_rate */
	s8	rx_snr[4];
};

struct wlan_bcn_info {
	/* these infor get from rtw_get_encrypt_info when
	 *	 * translate scan to UI */
	u8 encryp_protocol;/* OPEN/WEP/WPA/WPA2/WAPI */
	int group_cipher; /* WPA/WPA2 group cipher */
	int pairwise_cipher;/* //WPA/WPA2/WEP pairwise cipher */
	int is_8021x;

	/* bwmode 20/40 and ch_offset UP/LOW */
	unsigned short	ht_cap_info;
	unsigned char	ht_info_infos_0;
};

enum bss_type {
	BSS_TYPE_UNDEF,
	BSS_TYPE_BCN = 1,
	BSS_TYPE_PROB_REQ = 2,
	BSS_TYPE_PROB_RSP = 3,
};

struct wlan_bssid_ex {
	u32  Length;
	unsigned char MacAddress[6];
	u8  Reserved[2];/* [0]: IS beacon frame , bss_type*/
	struct ndis_802_11_ssid  Ssid;
	u32  Privacy;
	long  Rssi;/* (in dBM,raw data ,get from PHY) */
	enum ndis_802_11_network_type  NetworkTypeInUse;
	struct ndis_802_11_configuration  Configuration;
	enum ndis_802_11_network_infrastructure  InfrastructureMode;
	NDIS_802_11_RATES_EX  SupportedRates;
	struct wlan_phy_info	PhyInfo;
	u32  IELength;
	u8  IEs[MAX_IE_SZ];	/* (timestamp, beacon interval, and capability information) */
} __attribute__((packed));

#define BSS_EX_IES(bss_ex) ((bss_ex)->IEs)
#define BSS_EX_IES_LEN(bss_ex) ((bss_ex)->IELength)
#define BSS_EX_FIXED_IE_OFFSET(bss_ex) ((bss_ex)->Reserved[0] == BSS_TYPE_PROB_REQ ? 0 : 12)
#define BSS_EX_TLV_IES(bss_ex) (BSS_EX_IES((bss_ex)) + BSS_EX_FIXED_IE_OFFSET((bss_ex)))
#define BSS_EX_TLV_IES_LEN(bss_ex) (BSS_EX_IES_LEN((bss_ex)) - BSS_EX_FIXED_IE_OFFSET((bss_ex)))

inline  static uint get_wlan_bssid_ex_sz(struct wlan_bssid_ex *bss)
{
	return sizeof(struct wlan_bssid_ex) - MAX_IE_SZ + bss->IELength;
}

struct	wlan_network {
	struct list_head	list;
	int	network_type;	/* refer to ieee80211.h for WIRELESS_11A/B/G */
	int	fixed;			/* set to fixed when not to be removed as site-surveying */
	unsigned long last_scanned; /* timestamp for the network */
	int	aid;			/* will only be valid when a BSS is joinned. */
	int	join_res;
	struct wlan_bssid_ex	network; /* must be the last item */
	struct wlan_bcn_info	BcnInfo;
};

enum VRTL_CARRIER_SENSE {
	DISABLE_VCS,
	ENABLE_VCS,
	AUTO_VCS
};

enum VCS_TYPE {
	NONE_VCS,
	RTS_CTS,
	CTS_TO_SELF
};




#define PWR_CAM 0
#define PWR_MINPS 1
#define PWR_MAXPS 2
#define PWR_UAPSD 3
#define PWR_VOIP 4


enum UAPSD_MAX_SP {
	NO_LIMIT,
	TWO_MSDU,
	FOUR_MSDU,
	SIX_MSDU
};

#define NUM_PRE_AUTH_KEY 16
#define NUM_PMKID_CACHE NUM_PRE_AUTH_KEY

/*
*	WPA2
*/

struct pmkid_candidate {
	unsigned char BSSID[6];
	u32 Flags;
};

struct ndis_802_11_pmkid_candidate_list {
	u32 Version;       /* Version of the structure */
	u32 NumCandidates; /* No. of pmkid candidates */
	struct pmkid_candidate CandidateList[1];
};

struct ndis_802_11_authentification_encrypt {
	enum ndis_802_11_authentication_mode AuthModeSupported;
	enum ndis_802_11_wep_status EncryptStatusSupported;

};

struct ndis_802_11_capability {
	u32  Length;
	u32  Version;
	u32  NoOfPMKIDs;
	u32  NoOfAuthEncryptPairsSupported;
	struct ndis_802_11_authentification_encrypt AuthenticationEncryptionSupported[1];

};

#endif /* #ifndef WLAN_BSSDEF_H_ */
