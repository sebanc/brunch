/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _RTW_MP_H_
#define _RTW_MP_H_

#define RTWPRIV_VER_INFO	1

#define MAX_MP_XMITBUF_SZ	2048
#define NR_MP_XMITFRAME		8

struct mp_xmit_frame {
	struct list_head	list;

	struct pkt_attrib attrib;

	struct sk_buff *pkt;

	int frame_tag;

	struct adapter *adapt;

	/* insert urb, irp, and irpcnt info below... */
	/* max frag_cnt = 8 */

	u8 *mem_addr;
	u32 sz[8];

	struct urb * pxmit_urb[8];

	u8 bpending[8];
	int ac_tag[8];
	int last[8];
	uint irpcnt;
	uint fragcnt;

	uint mem[(MAX_MP_XMITBUF_SZ >> 2)];
};

struct mp_wiparam {
	u32 bcompleted;
	u32 act_type;
	u32 io_offset;
	u32 io_value;
};

struct mp_tx {
	u8 stop;
	u32 count, sended;
	u8 payload;
	struct pkt_attrib attrib;
	/* struct tx_desc desc; */
	/* u8 resvdtx[7]; */
	u8 desc[TXDESC_SIZE];
	u8 *pallocated_buf;
	u8 *buf;
	u32 buf_size, write_size;
	void * PktTxThread;
};

#define MP_MAX_LINES		1000
#define MP_MAX_LINES_BYTES	256


struct rt_pmac_pkt_info {
	u8			MCS;
	u8			Nss;
	u8			Nsts;
	u32			N_sym;
	u8			SIGA2B3;
};

struct rt_pmac_tx_info {
	u8			bEnPMacTx:1;		/* 0: Disable PMac 1: Enable PMac */
	u8			Mode:3;				/* 0: Packet TX 3:Continuous TX */
	u8			Ntx:4;				/* 0-7 */
	u8			TX_RATE;			/* MPT_RATE_E */
	u8			TX_RATE_HEX;
	u8			TX_SC;
	u8			bSGI:1;
	u8			bSPreamble:1;
	u8			bSTBC:1;
	u8			bLDPC:1;
	u8			NDP_sound:1;
	u8			BandWidth:3;		/* 0: 20 1:40 2:80Mhz */
	u8			m_STBC;			/* bSTBC + 1 */
	u16			PacketPeriod;
	u32		PacketCount;
	u32		PacketLength;
	u8			PacketPattern;
	u16			SFD;
	u8			SignalField;
	u8			ServiceField;
	u16			LENGTH;
	u8			CRC16[2];
	u8			LSIG[3];
	u8			HT_SIG[6];
	u8			VHT_SIG_A[6];
	u8			VHT_SIG_B[4];
	u8			VHT_SIG_B_CRC;
	u8			VHT_Delimiter[4];
	u8			MacAddress[6];
};

typedef void (*MPT_WORK_ITEM_HANDLER)(void * Adapter);

struct mpt_context {
	/* Indicate if we have started Mass Production Test. */
	bool			bMassProdTest;

	/* Indicate if the driver is unloading or unloaded. */
	bool			bMptDrvUnload;

	struct semaphore	MPh2c_Sema;
	struct timer_list			MPh2c_timeout_timer;
	/* Event used to sync H2c for BT control */

	bool		MptH2cRspEvent;
	bool		MptBtC2hEvent;
	bool		bMPh2c_timeout;

	/* 8190 PCI does not support NDIS_WORK_ITEM. */
	/* Work Item for Mass Production Test. */
	/* NDIS_WORK_ITEM	MptWorkItem;
	*	RT_WORK_ITEM		MptWorkItem; */
	/* Event used to sync the case unloading driver and MptWorkItem is still in progress.
	*	NDIS_EVENT		MptWorkItemEvent; */
	/* To protect the following variables.
	*	NDIS_SPIN_LOCK		MptWorkItemSpinLock; */
	/* Indicate a MptWorkItem is scheduled and not yet finished. */
	bool			bMptWorkItemInProgress;
	/* An instance which implements function and context of MptWorkItem. */
	MPT_WORK_ITEM_HANDLER	CurrMptAct;

	/* 1=Start, 0=Stop from UI. */
	u32			MptTestStart;
	/* _TEST_MODE, defined in MPT_Req2.h */
	u32			MptTestItem;
	/* Variable needed in each implementation of CurrMptAct. */
	u32			MptActType;	/* Type of action performed in CurrMptAct. */
	/* The Offset of IO operation is depend of MptActType. */
	u32			MptIoOffset;
	/* The Value of IO operation is depend of MptActType. */
	u32			MptIoValue;
	/* The RfPath of IO operation is depend of MptActType. */

	u32			mpt_rf_path;


	enum wireless_mode	MptWirelessModeToSw;	/* Wireless mode to switch. */
	u8			MptChannelToSw;	/* Channel to switch. */
	u8			MptInitGainToSet;	/* Initial gain to set. */
	/* u32			bMptAntennaA;		 */ /* true if we want to use antenna A. */
	u32			MptBandWidth;		/* bandwidth to switch. */

	u32			mpt_rate_index;/* rate index. */

	/* Register value kept for Single Carrier Tx test. */
	u8			btMpCckTxPower;
	/* Register value kept for Single Carrier Tx test. */
	u8			btMpOfdmTxPower;
	/* For MP Tx Power index */
	u8			TxPwrLevel[4];	/* rf-A, rf-B*/
	u32			RegTxPwrLimit;
	/* Content of RCR Regsiter for Mass Production Test. */
	u32			MptRCR;
	/* true if we only receive packets with specific pattern. */
	bool			bMptFilterPattern;
	/* Rx OK count, statistics used in Mass Production Test. */
	u32			MptRxOkCnt;
	/* Rx CRC32 error count, statistics used in Mass Production Test. */
	u32			MptRxCrcErrCnt;

	bool			bCckContTx;	/* true if we are in CCK Continuous Tx test. */
	bool			bOfdmContTx;	/* true if we are in OFDM Continuous Tx test. */
		/* true if we have start Continuous Tx test. */
	bool			is_start_cont_tx;

	/* true if we are in Single Carrier Tx test. */
	bool			bSingleCarrier;
	/* true if we are in Carrier Suppression Tx Test. */

	bool			is_carrier_suppression;

	/* true if we are in Single Tone Tx test. */

	bool			is_single_tone;


	/* ACK counter asked by K.Y.. */
	bool			bMptEnableAckCounter;
	u32			MptAckCounter;

	/* SD3 Willis For 8192S to save 1T/2T RF table for ACUT	Only fro ACUT delete later ~~~! */
	/* s8		BufOfLines[2][MAX_LINES_HWCONFIG_TXT][MAX_BYTES_LINE_HWCONFIG_TXT]; */
	/* s8			BufOfLines[2][MP_MAX_LINES][MP_MAX_LINES_BYTES]; */
	/* int			RfReadLine[2]; */

	u8		APK_bound[2];	/* for APK	path A/path B */
	bool		bMptIndexEven;

	u8		backup0xc50;
	u8		backup0xc58;
	u8		backup0xc30;
	u8		backup0x52_RF_A;
	u8		backup0x52_RF_B;

	u32			backup0x58_RF_A;
	u32			backup0x58_RF_B;

	u8			h2cReqNum;
	u8			c2hBuf[32];

	u8          btInBuf[100];
	u32			mptOutLen;
	u8          mptOutBuf[100];
	struct rt_pmac_tx_info	PMacTxInfo;
	struct rt_pmac_pkt_info	PMacPktInfo;
	u8 HWTxmode;

	bool			bldpc;
	bool			bstbc;
};

/* #define RTPRIV_IOCTL_MP					( SIOCIWFIRSTPRIV + 0x17) */
enum {
	WRITE_REG = 1,
	READ_REG,
	WRITE_RF,
	READ_RF,
	MP_START,
	MP_STOP,
	MP_RATE,
	MP_CHANNEL,
	MP_BANDWIDTH,
	MP_TXPOWER,
	MP_ANT_TX,
	MP_ANT_RX,
	MP_CTX,
	MP_QUERY,
	MP_ARX,
	MP_PSD,
	MP_PWRTRK,
	MP_THER,
	MP_IOCTL,
	EFUSE_GET,
	EFUSE_SET,
	MP_RESET_STATS,
	MP_DUMP,
	MP_PHYPARA,
	MP_SetRFPathSwh,
	MP_QueryDrvStats,
	CTA_TEST,
	MP_DISABLE_BT_COEXIST,
	MP_PwrCtlDM,
	MP_GETVER,
	MP_MON,
	EFUSE_MASK,
	EFUSE_FILE,
	MP_TX,
	MP_RX,
	MP_IQK,
	MP_LCK,
	MP_HW_TX_MODE,
	MP_GET_TXPOWER_INX,
	MP_CUSTOMER_STR,
	MP_PWRLMT,
	MP_PWRBYRATE,
	BT_EFUSE_FILE,
	MP_SetBT,
	MP_SWRFPath,
	MP_NULL,
	MP_SD_IREAD,
	MP_SD_IWRITE,
};

struct mp_priv {
	struct adapter *papdater;

	/* Testing Flag */
	u32 mode;/* 0 for normal type packet, 1 for loopback packet (16bytes TXCMD) */

	u32 prev_fw_state;

	/* OID cmd handler */
	struct mp_wiparam workparam;
	/*	u8 act_in_progress; */

	/* Tx Section */
	u8 TID;
	u32 tx_pktcount;
	u32 pktInterval;
	u32 pktLength;
	struct mp_tx tx;

	/* Rx Section */
	u32 rx_bssidpktcount;
	u32 rx_pktcount;
	u32 rx_pktcount_filter_out;
	u32 rx_crcerrpktcount;
	u32 rx_pktloss;
	bool  rx_bindicatePkt;
	struct recv_stat rxstat;

	/* RF/BB relative */
	u8 channel;
	u8 bandwidth;
	u8 prime_channel_offset;
	u8 txpoweridx;
	u8 rateidx;
	u32 preamble;
	/*	u8 modem; */
	u32 CrystalCap;
	/*	u32 curr_crystalcap; */

	u16 antenna_tx;
	u16 antenna_rx;
	/*	u8 curr_rfpath; */

	u8 check_mp_pkt;

	u8 bSetTxPower;
	/*	uint ForcedDataRate; */
	u8 mp_dm;
	u8 mac_filter[ETH_ALEN];
	u8 bmac_filter;

	/* RF PATH Setting for WLG WLA BTG BT */
	u8 rf_path_cfg;

	struct wlan_network mp_network;
	unsigned char network_macaddr[6];

	u8 *pallocated_mp_xmitframe_buf;
	u8 *pmp_xmtframe_buf;
	struct __queue free_mp_xmitqueue;
	u32 free_mp_xmitframe_cnt;
	bool bSetRxBssid;
	bool bTxBufCkFail;
	bool bRTWSmbCfg;
	bool bloopback;
	bool bloadefusemap;
	bool bloadBTefusemap;

	struct mpt_context	mpt_ctx;


	u8		*TXradomBuffer;
};

struct rf_reg_param {
	u32 path;
	u32 offset;
	u32 value;
};

struct bb_reg_param {
	u32 offset;
	u32 value;
};

struct rt_mp_firmware {
	enum firmware_source eFWSource;
#ifdef CONFIG_EMBEDDED_FWIMG
	u8		*szFwBuffer;
#else
	u8			szFwBuffer[0x8000];
#endif
	u32		ulFwLength;
};

/* *********************************************************************** */

#define LOWER	true
#define RAISE	false

/* Hardware Registers */
#define BB_REG_BASE_ADDR		0x800

/* MP variables */
enum mp_mode {
	MP_OFF,
	MP_ON,
	MP_ERR,
	MP_CONTINUOUS_TX,
	MP_SINGLE_CARRIER_TX,
	MP_CARRIER_SUPPRISSION_TX,
	MP_SINGLE_TONE_TX,
	MP_PACKET_TX,
	MP_PACKET_RX
};

enum test_mode {
	TEST_NONE                 ,
	PACKETS_TX                ,
	PACKETS_RX                ,
	CONTINUOUS_TX             ,
	OFDM_Single_Tone_TX       ,
	CCK_Carrier_Suppression_TX
};


enum mpt_bandwidth {
	MPT_BW_20MHZ = 0,
	MPT_BW_40MHZ_DUPLICATE = 1,
	MPT_BW_40MHZ_ABOVE = 2,
	MPT_BW_40MHZ_BELOW = 3,
	MPT_BW_40MHZ = 4,
	MPT_BW_80MHZ = 5,
	MPT_BW_80MHZ_20_ABOVE = 6,
	MPT_BW_80MHZ_20_BELOW = 7,
	MPT_BW_80MHZ_20_BOTTOM = 8,
	MPT_BW_80MHZ_20_TOP = 9,
	MPT_BW_80MHZ_40_ABOVE = 10,
	MPT_BW_80MHZ_40_BELOW = 11,
};

#define MAX_RF_PATH_NUMS	RF_PATH_MAX


extern u8 mpdatarate[NumRates];

/* MP set force data rate base on the definition. */
enum mpt_rate_index {
	/* CCK rate. */
	MPT_RATE_1M = 1 ,	/* 0 */
	MPT_RATE_2M,
	MPT_RATE_55M,
	MPT_RATE_11M,	/* 3 */

	/* OFDM rate. */
	MPT_RATE_6M,	/* 4 */
	MPT_RATE_9M,
	MPT_RATE_12M,
	MPT_RATE_18M,
	MPT_RATE_24M,
	MPT_RATE_36M,
	MPT_RATE_48M,
	MPT_RATE_54M,	/* 11 */

	/* HT rate. */
	MPT_RATE_MCS0,	/* 12 */
	MPT_RATE_MCS1,
	MPT_RATE_MCS2,
	MPT_RATE_MCS3,
	MPT_RATE_MCS4,
	MPT_RATE_MCS5,
	MPT_RATE_MCS6,
	MPT_RATE_MCS7,	/* 19 */
	MPT_RATE_MCS8,
	MPT_RATE_MCS9,
	MPT_RATE_MCS10,
	MPT_RATE_MCS11,
	MPT_RATE_MCS12,
	MPT_RATE_MCS13,
	MPT_RATE_MCS14,
	MPT_RATE_MCS15,	/* 27 */
	MPT_RATE_MCS16,
	MPT_RATE_MCS17, /*  #29 */
	MPT_RATE_MCS18,
	MPT_RATE_MCS19,
	MPT_RATE_MCS20,
	MPT_RATE_MCS21,
	MPT_RATE_MCS22, /*  #34 */
	MPT_RATE_MCS23,
	MPT_RATE_MCS24,
	MPT_RATE_MCS25,
	MPT_RATE_MCS26,
	MPT_RATE_MCS27, /*  #39 */
	MPT_RATE_MCS28, /*  #40 */
	MPT_RATE_MCS29, /*  #41 */
	MPT_RATE_MCS30, /*  #42 */
	MPT_RATE_MCS31, /*  #43 */
	/* VHT rate. Total: 20*/
	MPT_RATE_VHT1SS_MCS0 = 100,/*  #44*/
	MPT_RATE_VHT1SS_MCS1, /*  # */
	MPT_RATE_VHT1SS_MCS2,
	MPT_RATE_VHT1SS_MCS3,
	MPT_RATE_VHT1SS_MCS4,
	MPT_RATE_VHT1SS_MCS5,
	MPT_RATE_VHT1SS_MCS6, /*  # */
	MPT_RATE_VHT1SS_MCS7,
	MPT_RATE_VHT1SS_MCS8,
	MPT_RATE_VHT1SS_MCS9, /* #53 */
	MPT_RATE_VHT2SS_MCS0, /* #54 */
	MPT_RATE_VHT2SS_MCS1,
	MPT_RATE_VHT2SS_MCS2,
	MPT_RATE_VHT2SS_MCS3,
	MPT_RATE_VHT2SS_MCS4,
	MPT_RATE_VHT2SS_MCS5,
	MPT_RATE_VHT2SS_MCS6,
	MPT_RATE_VHT2SS_MCS7,
	MPT_RATE_VHT2SS_MCS8,
	MPT_RATE_VHT2SS_MCS9, /* #63 */
	MPT_RATE_VHT3SS_MCS0,
	MPT_RATE_VHT3SS_MCS1,
	MPT_RATE_VHT3SS_MCS2,
	MPT_RATE_VHT3SS_MCS3,
	MPT_RATE_VHT3SS_MCS4,
	MPT_RATE_VHT3SS_MCS5,
	MPT_RATE_VHT3SS_MCS6, /*  #126 */
	MPT_RATE_VHT3SS_MCS7,
	MPT_RATE_VHT3SS_MCS8,
	MPT_RATE_VHT3SS_MCS9,
	MPT_RATE_VHT4SS_MCS0,
	MPT_RATE_VHT4SS_MCS1, /*  #131 */
	MPT_RATE_VHT4SS_MCS2,
	MPT_RATE_VHT4SS_MCS3,
	MPT_RATE_VHT4SS_MCS4,
	MPT_RATE_VHT4SS_MCS5,
	MPT_RATE_VHT4SS_MCS6, /*  #136 */
	MPT_RATE_VHT4SS_MCS7,
	MPT_RATE_VHT4SS_MCS8,
	MPT_RATE_VHT4SS_MCS9,
	MPT_RATE_LAST
};

#define MAX_TX_PWR_INDEX_N_MODE 64	/* 0x3F */

#define MPT_IS_CCK_RATE(_value)		(MPT_RATE_1M <= _value && _value <= MPT_RATE_11M)
#define MPT_IS_OFDM_RATE(_value)	(MPT_RATE_6M <= _value && _value <= MPT_RATE_54M)
#define MPT_IS_HT_RATE(_value)		(MPT_RATE_MCS0 <= _value && _value <= MPT_RATE_MCS31)
#define MPT_IS_HT_1S_RATE(_value)	(MPT_RATE_MCS0 <= _value && _value <= MPT_RATE_MCS7)
#define MPT_IS_HT_2S_RATE(_value)	(MPT_RATE_MCS8 <= _value && _value <= MPT_RATE_MCS15)
#define MPT_IS_HT_3S_RATE(_value)	(MPT_RATE_MCS16 <= _value && _value <= MPT_RATE_MCS23)
#define MPT_IS_HT_4S_RATE(_value)	(MPT_RATE_MCS24 <= _value && _value <= MPT_RATE_MCS31)

#define MPT_IS_VHT_RATE(_value)		(MPT_RATE_VHT1SS_MCS0 <= _value && _value <= MPT_RATE_VHT4SS_MCS9)
#define MPT_IS_VHT_1S_RATE(_value)	(MPT_RATE_VHT1SS_MCS0 <= _value && _value <= MPT_RATE_VHT1SS_MCS9)
#define MPT_IS_VHT_2S_RATE(_value)	(MPT_RATE_VHT2SS_MCS0 <= _value && _value <= MPT_RATE_VHT2SS_MCS9)
#define MPT_IS_VHT_3S_RATE(_value)	(MPT_RATE_VHT3SS_MCS0 <= _value && _value <= MPT_RATE_VHT3SS_MCS9)
#define MPT_IS_VHT_4S_RATE(_value)	(MPT_RATE_VHT4SS_MCS0 <= _value && _value <= MPT_RATE_VHT4SS_MCS9)

#define MPT_IS_2SS_RATE(_rate) ((MPT_RATE_MCS8 <= _rate && _rate <= MPT_RATE_MCS15) || \
	(MPT_RATE_VHT2SS_MCS0 <= _rate && _rate <= MPT_RATE_VHT2SS_MCS9))
#define MPT_IS_3SS_RATE(_rate) ((MPT_RATE_MCS16 <= _rate && _rate <= MPT_RATE_MCS23) || \
	(MPT_RATE_VHT3SS_MCS0 <= _rate && _rate <= MPT_RATE_VHT3SS_MCS9))
#define MPT_IS_4SS_RATE(_rate) ((MPT_RATE_MCS24 <= _rate && _rate <= MPT_RATE_MCS31) || \
	(MPT_RATE_VHT4SS_MCS0 <= _rate && _rate <= MPT_RATE_VHT4SS_MCS9))

enum power_mode {
	POWER_LOW = 0,
	POWER_NORMAL
};

/* The following enumeration is used to define the value of Reg0xD00[30:28] or JaguarReg0x914[18:16]. */
enum ofdm_tx_mode {
	OFDM_ALL_OFF		= 0,
	OFDM_ContinuousTx	= 1,
	OFDM_SingleCarrier	= 2,
	OFDM_SingleTone	= 4,
};


#define RX_PKT_BROADCAST	1
#define RX_PKT_DEST_ADDR	2
#define RX_PKT_PHY_MATCH	3

enum encry_ctrl_state {
	HW_CONTROL,		/* hw encryption& decryption */
	SW_CONTROL,		/* sw encryption& decryption */
	HW_ENCRY_SW_DECRY,	/* hw encryption & sw decryption */
	SW_ENCRY_HW_DECRY	/* sw encryption & hw decryption */
};

enum mpt_txpwr_def {
	MPT_CCK,
	MPT_OFDM, /* L and HT OFDM */
	MPT_OFDM_AND_HT,
	MPT_HT,
	MPT_VHT
};

#define IS_MPT_HT_RATE(_rate)			(_rate >= MPT_RATE_MCS0 && _rate <= MPT_RATE_MCS31)
#define IS_MPT_VHT_RATE(_rate)			(_rate >= MPT_RATE_VHT1SS_MCS0 && _rate <= MPT_RATE_VHT4SS_MCS9)
#define IS_MPT_CCK_RATE(_rate)			(_rate >= MPT_RATE_1M && _rate <= MPT_RATE_11M)
#define IS_MPT_OFDM_RATE(_rate)			(_rate >= MPT_RATE_6M && _rate <= MPT_RATE_54M)
/*************************************************************************/
extern int init_mp_priv(struct adapter * adapt);
extern void free_mp_priv(struct mp_priv *pmp_priv);
extern int MPT_InitializeAdapter(struct adapter * adapt, u8 Channel);
extern void MPT_DeInitAdapter(struct adapter * adapt);
extern int mp_start_test(struct adapter * adapt);
extern void mp_stop_test(struct adapter * adapt);

extern u32 _read_rfreg(struct adapter * adapt, u8 rfpath, u32 addr, u32 bitmask);
extern void _write_rfreg(struct adapter * adapt, u8 rfpath, u32 addr, u32 bitmask, u32 val);

extern u32 read_macreg(struct adapter *adapt, u32 addr, u32 sz);
extern void write_macreg(struct adapter *adapt, u32 addr, u32 val, u32 sz);
extern u32 read_bbreg(struct adapter *adapt, u32 addr, u32 bitmask);
extern void write_bbreg(struct adapter *adapt, u32 addr, u32 bitmask, u32 val);
extern u32 read_rfreg(struct adapter * adapt, u8 rfpath, u32 addr);
extern void write_rfreg(struct adapter * adapt, u8 rfpath, u32 addr, u32 val);
void	SetChannel(struct adapter * pAdapter);
void	SetBandwidth(struct adapter * pAdapter);
int	SetTxPower(struct adapter * pAdapter);
void	SetAntenna(struct adapter * pAdapter);
void	SetDataRate(struct adapter * pAdapter);
void	SetAntenna(struct adapter * pAdapter);
int	SetThermalMeter(struct adapter * pAdapter, u8 target_ther);
void	GetThermalMeter(struct adapter * pAdapter, u8 *value);
void	SetContinuousTx(struct adapter * pAdapter, u8 bStart);
void	SetSingleCarrierTx(struct adapter * pAdapter, u8 bStart);
void	SetSingleToneTx(struct adapter * pAdapter, u8 bStart);
void	SetCarrierSuppressionTx(struct adapter * pAdapter, u8 bStart);
void	PhySetTxPowerLevel(struct adapter * pAdapter);
void	fill_txdesc_for_mp(struct adapter * adapt, u8 *ptxdesc);
void	SetPacketTx(struct adapter * adapt);
void	SetPacketRx(struct adapter * pAdapter, u8 bStartRx, u8 bAB);
void	ResetPhyRxPktCount(struct adapter * pAdapter);
u32	GetPhyRxPktReceived(struct adapter * pAdapter);
u32	GetPhyRxPktCRC32Error(struct adapter * pAdapter);
int	SetPowerTracking(struct adapter * adapt, u8 enable);
void	GetPowerTracking(struct adapter * adapt, u8 *enable);
u32	mp_query_psd(struct adapter * pAdapter, u8 *data);
void	rtw_mp_trigger_iqk(struct adapter * adapt);
void	rtw_mp_trigger_lck(struct adapter * adapt);
u8 rtw_mp_mode_check(struct adapter * adapt);


void hal_mpt_SwitchRfSetting(struct adapter * pAdapter);
int hal_mpt_SetPowerTracking(struct adapter * adapt, u8 enable);
void hal_mpt_GetPowerTracking(struct adapter * adapt, u8 *enable);
void hal_mpt_CCKTxPowerAdjust(struct adapter * Adapter, bool bInCH14);
void hal_mpt_SetChannel(struct adapter * pAdapter);
void hal_mpt_SetBandwidth(struct adapter * pAdapter);
void hal_mpt_SetTxPower(struct adapter * pAdapter);
void hal_mpt_SetDataRate(struct adapter * pAdapter);
void hal_mpt_SetAntenna(struct adapter * pAdapter);
int hal_mpt_SetThermalMeter(struct adapter * pAdapter, u8 target_ther);
void hal_mpt_TriggerRFThermalMeter(struct adapter * pAdapter);
u8 hal_mpt_ReadRFThermalMeter(struct adapter * pAdapter);
void hal_mpt_GetThermalMeter(struct adapter * pAdapter, u8 *value);
void hal_mpt_SetContinuousTx(struct adapter * pAdapter, u8 bStart);
void hal_mpt_SetSingleCarrierTx(struct adapter * pAdapter, u8 bStart);
void hal_mpt_SetSingleToneTx(struct adapter * pAdapter, u8 bStart);
void hal_mpt_SetCarrierSuppressionTx(struct adapter * pAdapter, u8 bStart);
void mpt_ProSetPMacTx(struct adapter *	Adapter);
void MP_PHY_SetRFPathSwitch(struct adapter * pAdapter , bool bMain);
void mp_phy_switch_rf_path_set(struct adapter * pAdapter , u8 *pstate);
u8 MP_PHY_QueryRFPathSwitch(struct adapter * pAdapter);
u32 mpt_ProQueryCalTxPower(struct adapter *	pAdapter, u8 RfPath);
void MPT_PwrCtlDM(struct adapter * adapt, u32 bstart);
u8 mpt_to_mgnt_rate(u32	MptRateIdx);
u8 rtw_mpRateParseFunc(struct adapter * pAdapter, u8 *targetStr);
u32 mp_join(struct adapter * adapt, u8 mode);
u32 hal_mpt_query_phytxok(struct adapter *	pAdapter);

void
PMAC_Get_Pkt_Param(
	struct rt_pmac_tx_info *	pPMacTxInfo,
	struct rt_pmac_pkt_info *	pPMacPktInfo
);
void
CCK_generator(
	struct rt_pmac_tx_info *	pPMacTxInfo,
	struct rt_pmac_pkt_info *	pPMacPktInfo
);
void
PMAC_Nsym_generator(
	struct rt_pmac_tx_info *	pPMacTxInfo,
	struct rt_pmac_pkt_info *	pPMacPktInfo
);
void
L_SIG_generator(
	u32	N_SYM,		/* Max: 750*/
	struct rt_pmac_tx_info *	pPMacTxInfo,
	struct rt_pmac_pkt_info *	pPMacPktInfo
);

void HT_SIG_generator(
	struct rt_pmac_tx_info *	pPMacTxInfo,
	struct rt_pmac_pkt_info *	pPMacPktInfo);

void VHT_SIG_A_generator(
	struct rt_pmac_tx_info *	pPMacTxInfo,
	struct rt_pmac_pkt_info *	pPMacPktInfo);

void VHT_SIG_B_generator(
	struct rt_pmac_tx_info *	pPMacTxInfo);

void VHT_Delimiter_generator(
	struct rt_pmac_tx_info *	pPMacTxInfo);


int rtw_mp_write_reg(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_read_reg(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_write_rf(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_read_rf(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_start(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_stop(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_rate(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_channel(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_bandwidth(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_txpower_index(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_txpower(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_txpower(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_ant_tx(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_ant_rx(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_set_ctx_destAddr(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_ctx(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_disable_bt_coexist(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_disable_bt_coexist(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_arx(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_trx_query(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_pwrtrk(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_psd(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_thermal(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_reset_stats(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_dump(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_phypara(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_SetRFPath(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_switch_rf_path(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *wrqu, char *extra);
int rtw_mp_QueryDrv(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_PwrCtlDM(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *wrqu, char *extra);
int rtw_mp_getver(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_mon(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_pwrlmt(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_pwrbyrate(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_efuse_mask_file(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_efuse_file_map(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_bt_efuse_file_map(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_SetBT(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_pretx_proc(struct adapter * adapt, u8 bStartTest, char *extra);
int rtw_mp_tx(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_rx(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
int rtw_mp_hwtx(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra);
u8 HwRateToMPTRate(u8 rate);
int rtw_mp_iqk(struct net_device *dev,
		 struct iw_request_info *info,
		 struct iw_point *wrqu, char *extra);
int rtw_mp_lck(struct net_device *dev, 
		struct iw_request_info *info, 
		struct iw_point *wrqu, char *extra);
#endif /* _RTW_MP_H_ */
