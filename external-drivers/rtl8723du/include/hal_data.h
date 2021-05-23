/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __HAL_DATA_H__
#define __HAL_DATA_H__

#include "../hal/phydm/phydm_precomp.h"
#include <hal_btcoex.h>

/*
 * <Roger_Notes> For RTL8723 WiFi/BT/GPS multi-function configuration. 2010.10.06.
 *   */
enum RT_MULTI_FUNC {
	RT_MULTI_FUNC_NONE	= 0x00,
	RT_MULTI_FUNC_WIFI	= 0x01,
	RT_MULTI_FUNC_BT		= 0x02,
	RT_MULTI_FUNC_GPS	= 0x04,
};
/*
 * <Roger_Notes> For RTL8723 WiFi PDn/GPIO polarity control configuration. 2010.10.08.
 *   */
enum RT_POLARITY_CTL {
	RT_POLARITY_LOW_ACT	= 0,
	RT_POLARITY_HIGH_ACT	= 1,
};

/* For RTL8723 regulator mode. by tynli. 2011.01.14. */
enum RT_REGULATOR_MODE {
	RT_SWITCHING_REGULATOR	= 0,
	RT_LDO_REGULATOR			= 1,
};

/*
 * Interface type.
 *   */
enum INTERFACE_SELECT_PCIE {
	INTF_SEL0_SOLO_MINICARD			= 0,		/* WiFi solo-mCard */
	INTF_SEL1_BT_COMBO_MINICARD		= 1,		/* WiFi+BT combo-mCard */
	INTF_SEL2_PCIe						= 2,		/* PCIe Card */
};

enum INTERFACE_SELECT_USB {
	INTF_SEL0_USB 				= 0,		/* USB */
	INTF_SEL1_USB_High_Power  	= 1,		/* USB with high power PA */
	INTF_SEL2_MINICARD		  	= 2,		/* Minicard */
	INTF_SEL3_USB_Solo 		= 3,		/* USB solo-Slim module */
	INTF_SEL4_USB_Combo		= 4,		/* USB Combo-Slim module */
	INTF_SEL5_USB_Combo_MF	= 5,		/* USB WiFi+BT Multi-Function Combo, i.e., Proprietary layout(AS-VAU) which is the same as SDIO card */
};

enum RT_AMPDU_BURST {
	RT_AMPDU_BURST_NONE	= 0,
	RT_AMPDU_BURST_92D	= 1,
	RT_AMPDU_BURST_88E	= 2,
	RT_AMPDU_BURST_8812_4	= 3,
	RT_AMPDU_BURST_8812_8	= 4,
	RT_AMPDU_BURST_8812_12	= 5,
	RT_AMPDU_BURST_8812_15	= 6,
	RT_AMPDU_BURST_8723B	= 7,
};

/* Tx Power Limit Table Size */
#define MAX_REGULATION_NUM						4
#define MAX_RF_PATH_NUM_IN_POWER_LIMIT_TABLE	4
#define MAX_2_4G_BANDWIDTH_NUM					2
#define MAX_RATE_SECTION_NUM						10
#define MAX_5G_BANDWIDTH_NUM						4

#define MAX_BASE_NUM_IN_PHY_REG_PG_2_4G			10 /* CCK:1, OFDM:1, HT:4, VHT:4 */
#define MAX_BASE_NUM_IN_PHY_REG_PG_5G			9 /* OFDM:1, HT:4, VHT:4 */


/* ###### duplicate code,will move to ODM ######### */
/* #define IQK_MAC_REG_NUM		4 */
/* #define IQK_ADDA_REG_NUM		16 */

/* #define IQK_BB_REG_NUM			10 */
#define IQK_BB_REG_NUM_92C	9
#define IQK_BB_REG_NUM_92D	10
#define IQK_BB_REG_NUM_test	6

#define IQK_Matrix_Settings_NUM_92D	(1+24+21)

/* #define HP_THERMAL_NUM		8 */
/* ###### duplicate code,will move to ODM ######### */

#ifdef RTW_RX_AGGREGATION
enum RX_AGG_MODE {
	RX_AGG_DISABLE,
	RX_AGG_DMA,
	RX_AGG_USB,
	RX_AGG_MIX
};

/* #define MAX_RX_DMA_BUFFER_SIZE	10240 */		/* 10K for 8192C RX DMA buffer */

#endif /* RTW_RX_AGGREGATION */

/* E-Fuse */
#define EFUSE_MAP_SIZE	512

#define EFUSE_MAX_SIZE	512
/* end of E-Fuse */

#define Mac_OFDM_OK			0x00000000
#define Mac_OFDM_Fail		0x10000000
#define Mac_OFDM_FasleAlarm	0x20000000
#define Mac_CCK_OK			0x30000000
#define Mac_CCK_Fail		0x40000000
#define Mac_CCK_FasleAlarm	0x50000000
#define Mac_HT_OK			0x60000000
#define Mac_HT_Fail			0x70000000
#define Mac_HT_FasleAlarm	0x90000000
#define Mac_DropPacket		0xA0000000

#define REG_RF_BB_GAIN_OFFSET	0x55
#define RF_GAIN_OFFSET_MASK		0xfffff

/* For store initial value of BB register */
struct bb_init_register {
	u16	offset;
	u32	value;
};

#define PAGE_SIZE_128	128
#define PAGE_SIZE_256	256
#define PAGE_SIZE_512	512

#define HCI_SUS_ENTER		0
#define HCI_SUS_LEAVING		1
#define HCI_SUS_LEAVE		2
#define HCI_SUS_ENTERING	3
#define HCI_SUS_ERR			4

#define EFUSE_FILE_UNUSED 0
#define EFUSE_FILE_FAILED 1
#define EFUSE_FILE_LOADED 2

#define MACADDR_FILE_UNUSED 0
#define MACADDR_FILE_FAILED 1
#define MACADDR_FILE_LOADED 2

#define KFREE_FLAG_ON				BIT(0)
#define KFREE_FLAG_THERMAL_K_ON		BIT(1)

#define MAX_IQK_INFO_BACKUP_CHNL_NUM	5
#define MAX_IQK_INFO_BACKUP_REG_NUM		10

struct kfree_data_t {
	u8 flag;
	s8 bb_gain[BB_GAIN_NUM][RF_PATH_MAX];
	s8 thermal;
};

bool kfree_data_is_bb_gain_empty(struct kfree_data_t *data);

struct hal_spec_t {
	char *ic_name;
	u8 macid_num;

	u8 sec_cam_ent_num;
	u8 sec_cap;

	u8 rfpath_num_2g:4;	/* used for tx power index path */
	u8 rfpath_num_5g:4;	/* used for tx power index path */

	u8 max_tx_cnt;
	u8 tx_nss_num:4;
	u8 rx_nss_num:4;
	u8 band_cap;	/* value of BAND_CAP_XXX */
	u8 bw_cap;		/* value of BW_CAP_XXX */
	u8 port_num;
	u8 proto_cap;	/* value of PROTO_CAP_XXX */
	u8 wl_func;		/* value of WL_FUNC_XXX */
	u8 hci_type;	/* value of HCI Type */
	u8 bt_ft_ver;	/*bt FT version*/
};

#define HAL_SPEC_CHK_RF_PATH_2G(_spec, _path) ((_spec)->rfpath_num_2g > (_path))
#define HAL_SPEC_CHK_RF_PATH_5G(_spec, _path) ((_spec)->rfpath_num_5g > (_path))
#define HAL_SPEC_CHK_RF_PATH(_spec, _band, _path) ( \
	_band == BAND_ON_2_4G ? HAL_SPEC_CHK_RF_PATH_2G(_spec, _path) : \
	_band == BAND_ON_5G ? HAL_SPEC_CHK_RF_PATH_5G(_spec, _path) : 0)

#define HAL_SPEC_CHK_TX_CNT(_spec, _cnt_idx) ((_spec)->max_tx_cnt > (_cnt_idx))

#ifdef CONFIG_PHY_CAPABILITY_QUERY
struct phy_spec_t {
	u32 trx_cap;
	u32 stbc_cap;
	u32 ldpc_cap;
	u32 txbf_param;
	u32 txbf_cap;
};
#endif
struct hal_iqk_reg_backup {
	u8 central_chnl;
	u8 bw_mode;
	u32 reg_backup[MAX_RF_PATH][MAX_IQK_INFO_BACKUP_REG_NUM];
};


struct hal_p2p_ps_para {
	/*DW0*/
	u8  offload_en:1;
	u8  role:1;
	u8  ctwindow_en:1;
	u8  noa_en:1;
	u8  noa_sel:1;
	u8  all_sta_sleep:1;
	u8  discovery:1;
	u8  rsvd2:1;
	u8  p2p_port_id;
	u8  p2p_group;
	u8  p2p_macid;

	/*DW1*/
	u8 ctwindow_length;
	u8 rsvd3;
	u8 rsvd4;
	u8 rsvd5;

	/*DW2*/
	u32 noa_duration_para;

	/*DW3*/
	u32 noa_interval_para;

	/*DW4*/
	u32 noa_start_time_para;

	/*DW5*/
	u32 noa_count_para;
};

#define TXPWR_LMT_RS_CCK	0
#define TXPWR_LMT_RS_OFDM	1
#define TXPWR_LMT_RS_HT		2
#define TXPWR_LMT_RS_VHT	3
#define TXPWR_LMT_RS_NUM	4

#define TXPWR_LMT_RS_NUM_2G	4 /* CCK, OFDM, HT, VHT */
#define TXPWR_LMT_RS_NUM_5G	3 /* OFDM, HT, VHT */

#ifdef CONFIG_TXPWR_LIMIT
extern const char *const _txpwr_lmt_rs_str[];
#define txpwr_lmt_rs_str(rs) (((rs) >= TXPWR_LMT_RS_NUM) ? _txpwr_lmt_rs_str[TXPWR_LMT_RS_NUM] : _txpwr_lmt_rs_str[(rs)])

struct txpwr_lmt_ent {
	struct list_head list;

	s8 lmt_2g[MAX_2_4G_BANDWIDTH_NUM]
		[TXPWR_LMT_RS_NUM_2G]
		[CENTER_CH_2G_NUM]
		[MAX_TX_COUNT];

	char regd_name[0];
};
#endif /* CONFIG_TXPWR_LIMIT */

struct hal_com_data {
	struct hal_version			version_id;
	enum RT_MULTI_FUNC		MultiFunc; /* For multi-function consideration. */
	enum RT_POLARITY_CTL		PolarityCtl; /* For Wifi PDn Polarity control. */
	enum RT_REGULATOR_MODE	RegulatorMode; /* switching regulator or LDO */
	u8	hw_init_completed;
	/****** FW related ******/
	u32 firmware_size;
	u16 firmware_version;
	u16	FirmwareVersionRev;
	u16 firmware_sub_version;
	u16	FirmwareSignature;
	u8	RegFWOffload;
	u8	bFWReady;
	u8	bBTFWReady;
	u8	fw_ractrl;
	u8	LastHMEBoxNum;	/* H2C - for host message to fw */

	/****** current WIFI_PHY values ******/
	enum wireless_mode CurrentWirelessMode;
	enum channel_width current_channel_bw;
	enum BAND_TYPE		current_band_type;	/* 0:2.4G, 1:5G */
	enum BAND_TYPE		BandSet;
	u8				current_channel;
	u8				cch_20;
	u8				cch_40;
	u8				cch_80;
	u8				CurrentCenterFrequencyIndex1;
	u8				nCur40MhzPrimeSC;	/* Control channel sub-carrier */
	u8				nCur80MhzPrimeSC;   /* used for primary 40MHz of 80MHz mode */
	bool		bSwChnlAndSetBWInProgress;
	u8				bDisableSWChannelPlan; /* flag of disable software change channel plan	 */
	u16				BasicRateSet;
	u32				ReceiveConfig;
	u32				rcr_backup; /* used for switching back from monitor mode */
	u8				rx_tsf_addr_filter_config; /* for 8822B/8821C USE */
	bool			bSwChnl;
	bool			bSetChnlBW;
	bool			bSWToBW40M;
	bool			bSWToBW80M;
	bool			bChnlBWInitialized;
	u32				BackUp_BB_REG_4_2nd_CCA[3];

#ifdef CONFIG_BCN_RECOVERY
	u8 issue_bcn_fail;
#endif /*CONFIG_BCN_RECOVERY*/

	/****** rf_ctrl *****/
	u8	rf_chip;
	u8	rf_type;	/*enum rf_type*/
	u8	PackageType;
	u8	NumTotalRFPath;
	u8	antenna_test;

	/****** Debug ******/
	u16	ForcedDataRate;	/* Force Data Rate. 0: Auto, 0x02: 1M ~ 0x6C: 54M. */
	u8	bDumpRxPkt;
	u8	bDumpTxPkt;
	u8	bDisableTXPowerTraining;
	u8	dis_turboedca;


	/****** EEPROM setting.******/
	bool	bautoload_fail_flag;
	u8	efuse_file_status;
	u8	macaddr_file_status;
	u8	EepromOrEfuse;
	u8	efuse_eeprom_data[EEPROM_MAX_SIZE]; /*92C:256bytes, 88E:512bytes, we use union set (512bytes)*/
	u8	InterfaceSel; /* board type kept in eFuse */
	u16	CustomerID;

	u16	EEPROMVID;
	u16	EEPROMSVID;
	u8	EEPROMUsbSwitch;
	u16	EEPROMPID;
	u16	EEPROMSDID;

	u8	EEPROMCustomerID;
	u8	EEPROMSubCustomerID;
	u8	EEPROMVersion;
	u8	EEPROMRegulatory;
	u8	eeprom_thermal_meter;
	u8	EEPROMBluetoothCoexist;
	u8	EEPROMBluetoothType;
	u8	EEPROMBluetoothAntNum;
	u8	EEPROMBluetoothAntIsolation;
	u8	EEPROMBluetoothRadioShared;
	u8	EEPROMMACAddr[ETH_ALEN];
	u8	tx_bbswing_24G;
	u8	tx_bbswing_5G;
	u8	efuse0x3d7;	/* efuse[0x3D7] */
	u8	efuse0x3d8;	/* efuse[0x3D8] */

	u8	EEPROMRFGainOffset;
	u8	EEPROMRFGainVal;
	struct kfree_data_t kfree_data;

	u8      adjuseVoltageVal;
	u8      need_restore;
	u8	EfuseUsedPercentage;
	u16	EfuseUsedBytes;
	struct efuse_hal	EfuseHal;

	/*---------------------------------------------------------------------------------*/
	/* 2.4G TX power info for target TX power*/
	u8	Index24G_CCK_Base[MAX_RF_PATH][CENTER_CH_2G_NUM];
	u8	Index24G_BW40_Base[MAX_RF_PATH][CENTER_CH_2G_NUM];
	s8	CCK_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	OFDM_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW20_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW40_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];

	u8 txpwr_by_rate_undefined_band_path[TX_PWR_BY_RATE_NUM_BAND]
		[TX_PWR_BY_RATE_NUM_RF];

	s8	TxPwrByRateOffset[TX_PWR_BY_RATE_NUM_BAND]
		[TX_PWR_BY_RATE_NUM_RF]
		[TX_PWR_BY_RATE_NUM_RATE];

	/* Store the original power by rate value of the base rate for each rate section and rf path */
	u8	TxPwrByRateBase2_4G[TX_PWR_BY_RATE_NUM_RF]
		[MAX_BASE_NUM_IN_PHY_REG_PG_2_4G];
	u8	TxPwrByRateBase5G[TX_PWR_BY_RATE_NUM_RF]
		[MAX_BASE_NUM_IN_PHY_REG_PG_5G];

	u8	txpwr_by_rate_loaded:1;
	u8	txpwr_by_rate_from_file:1;
	u8	txpwr_limit_loaded:1;
	u8	txpwr_limit_from_file:1;
	u8	rf_power_tracking_type;

	/* Read/write are allow for following hardware information variables	 */
	u8	crystal_cap;

	u8	PAType_2G;
	u8	PAType_5G;
	u8	LNAType_2G;
	u8	LNAType_5G;
	u8	ExternalPA_2G;
	u8	ExternalLNA_2G;
	u8	external_pa_5g;
	u8	external_lna_5g;
	u16	TypeGLNA;
	u16	TypeGPA;
	u16	TypeALNA;
	u16	TypeAPA;
	u16	rfe_type;

	u8	bLedOpenDrain; /* Support Open-drain arrangement for controlling the LED. Added by Roger, 2009.10.16. */
	u32	ac_param_be; /* Original parameter for BE, use for EDCA turbo.	*/
	u8	is_turbo_edca;
	u8	prv_traffic_idx;
	struct bb_register_definition	PHYRegDef[MAX_RF_PATH];	/* Radio A/B/C/D */

	u32	RfRegChnlVal[MAX_RF_PATH];

	/* RDG enable */
	bool	 bRDGEnable;

	u16 RegRRSR;
	/****** antenna diversity ******/
	u8	AntDivCfg;
	u8	with_extenal_ant_switch;
	u8	b_fix_tx_ant;
	u8	AntDetection;
	u8	TRxAntDivType;
	u8	ant_path; /* for 8723B s0/s1 selection	 */
	u32	antenna_tx_path;					/* Antenna path Tx */
	u32	AntennaRxPath;					/* Antenna path Rx */
	u8 sw_antdiv_bl_state;

	/******** PHY DM & DM Section **********/
	spinlock_t		IQKSpinLock;
	u8			INIDATA_RATE[MACID_NUM_SW_LIMIT];

	struct PHY_DM_STRUCT	 odmpriv;
	u64			bk_rf_ability;
	u8			bIQKInitialized;
	u8			bNeedIQK;
	u8			IQK_MP_Switch;
	u8			bScanInProcess;
	/******** PHY DM & DM Section **********/



	/* 2010/08/09 MH Add CU power down mode. */
	bool		pwrdown;

	/* Add for dual MAC  0--Mac0 1--Mac1 */
	u32	interfaceIndex;
	u8	p2p_ps_offload;
	/* Auto FSM to Turn On, include clock, isolation, power control for MAC only */
	u8	bMacPwrCtrlOn;
	u8 hci_sus_state;
	u8	RegIQKFWOffload;
	struct submit_ctx	iqk_sctx;
	u8 ch_switch_offload;
	struct submit_ctx chsw_sctx;
	enum RT_AMPDU_BURST AMPDUBurstMode; /* 92C maybe not use, but for compile successfully */
	u8	OutEpQueueSel;
	u8	OutEpNumber;

#ifdef RTW_RX_AGGREGATION
	RX_AGG_MODE rxagg_mode;

	/* For RX Aggregation DMA Mode */
	u8 rxagg_dma_size;
	u8 rxagg_dma_timeout;
#endif /* RTW_RX_AGGREGATION */

	/* 2010/12/10 MH Add for USB aggreation mode dynamic shceme. */
	bool		UsbRxHighSpeedMode;
	bool		UsbTxVeryHighSpeedMode;
	u32			UsbBulkOutSize;
	bool		bSupportUSB3;
	u8			usb_intf_start;

	/* Interrupt relatd register information. */
	u32			IntArray[3];/* HISR0,HISR1,HSISR */
	u32			IntrMask[3];

	/* For bluetooth co-existance */
	struct bt_coexist		bt_coexist;

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	char	para_file_buf[MAX_PARA_FILE_BUF_LEN];
	char *mac_reg;
	u32	mac_reg_len;
	char *bb_phy_reg;
	u32	bb_phy_reg_len;
	char *bb_agc_tab;
	u32	bb_agc_tab_len;
	char *bb_phy_reg_pg;
	u32	bb_phy_reg_pg_len;
	char *bb_phy_reg_mp;
	u32	bb_phy_reg_mp_len;
	char *rf_radio_a;
	u32	rf_radio_a_len;
	char *rf_radio_b;
	u32	rf_radio_b_len;
	char *rf_tx_pwr_track;
	u32	rf_tx_pwr_track_len;
	char *rf_tx_pwr_lmt;
	u32	rf_tx_pwr_lmt_len;
#endif

	struct hal_spec_t hal_spec;
#ifdef CONFIG_PHY_CAPABILITY_QUERY
	struct phy_spec_t phy_spec;
#endif
	u8	RfKFreeEnable;
	u8	RfKFree_ch_group;
	bool				bCCKinCH14;
	struct bb_init_register	RegForRecover[5];

	u32 RxGainOffset[4]; /*{2G, 5G_Low, 5G_Middle, G_High}*/
	u8 BackUp_IG_REG_4_Chnl_Section[4]; /*{A,B,C,D}*/

	struct hal_iqk_reg_backup iqk_reg_backup[MAX_IQK_INFO_BACKUP_CHNL_NUM];

	u8 not_xmitframe_fw_dl; /*not use xmitframe to download fw*/
	u8 phydm_op_mode;

	u8 in_cta_test;
};

extern unsigned char RTW_WPA_OUI[];
extern unsigned char WPA_TKIP_CIPHER[4];
extern unsigned char RSN_TKIP_CIPHER[4];

struct hal_com_data;

#define GET_HAL_DATA(__pAdapter)			((struct hal_com_data *)((__pAdapter)->HalData))
#define GET_HAL_SPEC(__pAdapter)			(&(GET_HAL_DATA((__pAdapter))->hal_spec))

#define GET_HAL_RFPATH_NUM(__pAdapter)		(((struct hal_com_data *)((__pAdapter)->HalData))->NumTotalRFPath)
#define RT_GetInterfaceSelection(_Adapter)		(GET_HAL_DATA(_Adapter)->InterfaceSel)
#define GET_RF_TYPE(__pAdapter)				(GET_HAL_DATA(__pAdapter)->rf_type)
#define GET_KFREE_DATA(__ADAPTER) (&(GET_HAL_DATA((__ADAPTER))->kfree_data))

#define	SUPPORT_HW_RADIO_DETECT(Adapter)	(RT_GetInterfaceSelection(Adapter) == INTF_SEL2_MINICARD || \
		RT_GetInterfaceSelection(Adapter) == INTF_SEL3_USB_Solo || \
		RT_GetInterfaceSelection(Adapter) == INTF_SEL4_USB_Combo)

#define get_hal_mac_addr(adapter)				(GET_HAL_DATA(adapter)->EEPROMMACAddr)
#define is_boot_from_eeprom(adapter)			(GET_HAL_DATA(adapter)->EepromOrEfuse)
#define rtw_get_hw_init_completed(adapter)		(GET_HAL_DATA(adapter)->hw_init_completed)
#define rtw_is_hw_init_completed(adapter)		(GET_HAL_DATA(adapter)->hw_init_completed)

int recvbuf2recvframe(struct adapter * adapt, void *ptr);
extern int new_bcn_max;

/* alias for phydm coding style */
#define REG_OFDM_0_XA_TX_IQ_IMBALANCE	rOFDM0_XATxIQImbalance
#define REG_OFDM_0_ECCA_THRESHOLD		rOFDM0_ECCAThreshold
#define REG_FPGA0_XB_LSSI_READ_BACK		rFPGA0_XB_LSSIReadBack
#define REG_FPGA0_TX_GAIN_STAGE			rFPGA0_TxGainStage
#define REG_OFDM_0_XA_AGC_CORE1			rOFDM0_XAAGCCore1
#define REG_OFDM_0_XB_AGC_CORE1			rOFDM0_XBAGCCore1

#define REG_FPGA0_XAB_RF_INTERFACE_SW	rFPGA0_XAB_RFInterfaceSW
#define REG_FPGA0_XAB_RF_PARAMETER	rFPGA0_XAB_RFParameter
#define REG_FPGA0_XA_HSSI_PARAMETER1	rFPGA0_XA_HSSIParameter1
#define REG_FPGA0_XA_LSSI_PARAMETER	rFPGA0_XA_LSSIParameter
#define REG_FPGA0_XA_RF_INTERFACE_OE	rFPGA0_XA_RFInterfaceOE
#define REG_FPGA0_XB_HSSI_PARAMETER1	rFPGA0_XB_HSSIParameter1
#define REG_FPGA0_XB_LSSI_PARAMETER	rFPGA0_XB_LSSIParameter
#define REG_FPGA0_XB_LSSI_READ_BACK	rFPGA0_XB_LSSIReadBack
#define REG_FPGA0_XB_RF_INTERFACE_OE	rFPGA0_XB_RFInterfaceOE
#define REG_FPGA0_XCD_RF_INTERFACE_SW	rFPGA0_XCD_RFInterfaceSW
#define REG_FPGA0_XCD_SWITCH_CONTROL	rFPGA0_XCD_SwitchControl
#define REG_FPGA1_TX_BLOCK	rFPGA1_TxBlock
#define REG_FPGA1_TX_INFO	rFPGA1_TxInfo
#define REG_IQK_AGC_CONT	rIQK_AGC_Cont
#define REG_IQK_AGC_PTS	rIQK_AGC_Pts
#define REG_IQK_AGC_RSP	rIQK_AGC_Rsp
#define REG_OFDM_0_AGC_RSSI_TABLE	rOFDM0_AGCRSSITable
#define REG_OFDM_0_ECCA_THRESHOLD	rOFDM0_ECCAThreshold
#define REG_OFDM_0_RX_IQ_EXT_ANTA	rOFDM0_RxIQExtAnta
#define REG_OFDM_0_TR_MUX_PAR	rOFDM0_TRMuxPar
#define REG_OFDM_0_TRX_PATH_ENABLE	rOFDM0_TRxPathEnable
#define REG_OFDM_0_XA_AGC_CORE1	rOFDM0_XAAGCCore1
#define REG_OFDM_0_XA_RX_IQ_IMBALANCE	rOFDM0_XARxIQImbalance
#define REG_OFDM_0_XA_TX_IQ_IMBALANCE	rOFDM0_XATxIQImbalance
#define REG_OFDM_0_XB_AGC_CORE1	rOFDM0_XBAGCCore1
#define REG_OFDM_0_XB_RX_IQ_IMBALANCE	rOFDM0_XBRxIQImbalance
#define REG_OFDM_0_XB_TX_IQ_IMBALANCE	rOFDM0_XBTxIQImbalance
#define REG_OFDM_0_XC_TX_AFE	rOFDM0_XCTxAFE
#define REG_OFDM_0_XD_TX_AFE	rOFDM0_XDTxAFE

#define REG_BLUE_TOOTH	rBlue_Tooth
#define REG_CCK_0_AFE_SETTING	rCCK0_AFESetting
#define REG_CONFIG_ANT_A	rConfig_AntA
#define REG_CONFIG_ANT_B	rConfig_AntB
#define REG_CONFIG_PMPD_ANT_A	rConfig_Pmpd_AntA
#define REG_CONFIG_PMPD_ANT_B	rConfig_Pmpd_AntB
#define REG_DPDT_CONTROL	rDPDT_control
#define REG_FPGA0_ANALOG_PARAMETER4	rFPGA0_AnalogParameter4
#define REG_FPGA0_IQK	rFPGA0_IQK
#define REG_FPGA0_PSD_FUNCTION	rFPGA0_PSDFunction
#define REG_FPGA0_PSD_REPORT	rFPGA0_PSDReport
#define REG_FPGA0_RFMOD	rFPGA0_RFMOD
#define REG_FPGA0_TX_GAIN_STAGE	rFPGA0_TxGainStage
#define REG_FPGA0_XAB_RF_INTERFACE_SW	rFPGA0_XAB_RFInterfaceSW
#define REG_FPGA0_XAB_RF_PARAMETER	rFPGA0_XAB_RFParameter
#define REG_FPGA0_XA_HSSI_PARAMETER1	rFPGA0_XA_HSSIParameter1
#define REG_FPGA0_XA_LSSI_PARAMETER	rFPGA0_XA_LSSIParameter
#define REG_FPGA0_XA_RF_INTERFACE_OE	rFPGA0_XA_RFInterfaceOE
#define REG_FPGA0_XB_HSSI_PARAMETER1	rFPGA0_XB_HSSIParameter1
#define REG_FPGA0_XB_LSSI_PARAMETER	rFPGA0_XB_LSSIParameter
#define REG_FPGA0_XB_LSSI_READ_BACK	rFPGA0_XB_LSSIReadBack
#define REG_FPGA0_XB_RF_INTERFACE_OE	rFPGA0_XB_RFInterfaceOE
#define REG_FPGA0_XCD_RF_INTERFACE_SW	rFPGA0_XCD_RFInterfaceSW
#define REG_FPGA0_XCD_SWITCH_CONTROL	rFPGA0_XCD_SwitchControl
#define REG_FPGA1_TX_BLOCK	rFPGA1_TxBlock
#define REG_FPGA1_TX_INFO	rFPGA1_TxInfo
#define REG_IQK_AGC_CONT	rIQK_AGC_Cont
#define REG_IQK_AGC_PTS	rIQK_AGC_Pts
#define REG_IQK_AGC_RSP	rIQK_AGC_Rsp
#define REG_OFDM_0_AGC_RSSI_TABLE	rOFDM0_AGCRSSITable
#define REG_OFDM_0_ECCA_THRESHOLD	rOFDM0_ECCAThreshold
#define REG_OFDM_0_RX_IQ_EXT_ANTA	rOFDM0_RxIQExtAnta
#define REG_OFDM_0_TR_MUX_PAR	rOFDM0_TRMuxPar
#define REG_OFDM_0_TRX_PATH_ENABLE	rOFDM0_TRxPathEnable
#define REG_OFDM_0_XA_AGC_CORE1	rOFDM0_XAAGCCore1
#define REG_OFDM_0_XA_RX_IQ_IMBALANCE	rOFDM0_XARxIQImbalance
#define REG_OFDM_0_XA_TX_IQ_IMBALANCE	rOFDM0_XATxIQImbalance
#define REG_OFDM_0_XB_AGC_CORE1	rOFDM0_XBAGCCore1
#define REG_OFDM_0_XB_RX_IQ_IMBALANCE	rOFDM0_XBRxIQImbalance
#define REG_OFDM_0_XB_TX_IQ_IMBALANCE	rOFDM0_XBTxIQImbalance
#define REG_OFDM_0_XC_TX_AFE	rOFDM0_XCTxAFE
#define REG_OFDM_0_XD_TX_AFE	rOFDM0_XDTxAFE
#define REG_PMPD_ANAEN	rPMPD_ANAEN
#define REG_PDP_ANT_A	rPdp_AntA
#define REG_PDP_ANT_A_4	rPdp_AntA_4
#define REG_PDP_ANT_B	rPdp_AntB
#define REG_PDP_ANT_B_4	rPdp_AntB_4
#define REG_RX_CCK	rRx_CCK
#define REG_RX_IQK	rRx_IQK
#define REG_RX_IQK_PI_A	rRx_IQK_PI_A
#define REG_RX_IQK_PI_B	rRx_IQK_PI_B
#define REG_RX_IQK_TONE_A	rRx_IQK_Tone_A
#define REG_RX_IQK_TONE_B	rRx_IQK_Tone_B
#define REG_RX_OFDM	rRx_OFDM
#define REG_RX_POWER_AFTER_IQK_A_2	rRx_Power_After_IQK_A_2
#define REG_RX_POWER_AFTER_IQK_B_2	rRx_Power_After_IQK_B_2
#define REG_RX_POWER_BEFORE_IQK_A_2	rRx_Power_Before_IQK_A_2
#define REG_RX_POWER_BEFORE_IQK_B_2	rRx_Power_Before_IQK_B_2
#define REG_RX_TO_RX	rRx_TO_Rx
#define REG_RX_WAIT_CCA	rRx_Wait_CCA
#define REG_RX_WAIT_RIFS	rRx_Wait_RIFS
#define REG_S0_S1_PATH_SWITCH	rS0S1_PathSwitch
#define REG_SYM_WLBT_PAPE_SEL	rSYM_WLBT_PAPE_SEL
#define REG_SLEEP	rSleep
#define REG_STANDBY	rStandby
#define REG_TX_AGC_A_CCK_1_MCS32	rTxAGC_A_CCK1_Mcint
#define REG_TX_AGC_A_MCS03_MCS00	rTxAGC_A_Mcs03_Mcs00
#define REG_TX_AGC_A_MCS07_MCS04	rTxAGC_A_Mcs07_Mcs04
#define REG_TX_AGC_A_MCS11_MCS08	rTxAGC_A_Mcs11_Mcs08
#define REG_TX_AGC_A_MCS15_MCS12	rTxAGC_A_Mcs15_Mcs12
#define REG_TX_AGC_A_RATE18_06	rTxAGC_A_Rate18_06
#define REG_TX_AGC_A_RATE54_24	rTxAGC_A_Rate54_24
#define REG_TX_AGC_B_CCK_11_A_CCK_2_11	rTxAGC_B_CCK11_A_CCK2_11
#define REG_TX_AGC_B_CCK_1_55_MCS32	rTxAGC_B_CCK1_55_Mcint
#define REG_TX_AGC_B_MCS03_MCS00	rTxAGC_B_Mcs03_Mcs00
#define REG_TX_AGC_B_MCS07_MCS04	rTxAGC_B_Mcs07_Mcs04
#define REG_TX_AGC_B_MCS11_MCS08	rTxAGC_B_Mcs11_Mcs08
#define REG_TX_AGC_B_MCS15_MCS12	rTxAGC_B_Mcs15_Mcs12
#define REG_TX_AGC_B_RATE18_06	rTxAGC_B_Rate18_06
#define REG_TX_AGC_B_RATE54_24	rTxAGC_B_Rate54_24
#define REG_TX_CCK_BBON	rTx_CCK_BBON
#define REG_TX_CCK_RFON	rTx_CCK_RFON
#define REG_TX_IQK	rTx_IQK
#define REG_TX_IQK_PI_A	rTx_IQK_PI_A
#define REG_TX_IQK_PI_B	rTx_IQK_PI_B
#define REG_TX_IQK_TONE_A	rTx_IQK_Tone_A
#define REG_TX_IQK_TONE_B	rTx_IQK_Tone_B
#define REG_TX_OFDM_BBON	rTx_OFDM_BBON
#define REG_TX_OFDM_RFON	rTx_OFDM_RFON
#define REG_TX_POWER_AFTER_IQK_A	rTx_Power_After_IQK_A
#define REG_TX_POWER_AFTER_IQK_B	rTx_Power_After_IQK_B
#define REG_TX_POWER_BEFORE_IQK_A	rTx_Power_Before_IQK_A
#define REG_TX_POWER_BEFORE_IQK_B	rTx_Power_Before_IQK_B
#define REG_TX_TO_RX	rTx_To_Rx
#define REG_TX_TO_TX	rTx_To_Tx
#define REG_APK	rAPK

#define rf_welut_jaguar	RF_WeLut_Jaguar
#define rf_mode_table_addr	RF_ModeTableAddr
#define rf_mode_table_data0	RF_ModeTableData0
#define rf_mode_table_data1	RF_ModeTableData1






#define RX_SMOOTH_FACTOR	Rx_Smooth_Factor

#endif /* __HAL_DATA_H__ */
