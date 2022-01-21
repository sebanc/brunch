/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __HAL_COMMON_H__
#define __HAL_COMMON_H__

#include "HalVerDef.h"
#include "hal_pg.h"
#include "hal_phy.h"
#include "hal_com_reg.h"
#include "hal_com_phycfg.h"
#include "../hal/hal_com_c2h.h"

/*------------------------------ Tx Desc definition Macro ------------------------*/
/* #pragma mark -- Tx Desc related definition. -- */
/* ----------------------------------------------------------------------------
 * -----------------------------------------------------------
 *	Rate
 * -----------------------------------------------------------
 * CCK Rates, TxHT = 0 */
#define DESC_RATE1M					0x00
#define DESC_RATE2M					0x01
#define DESC_RATE5_5M				0x02
#define DESC_RATE11M				0x03

/* OFDM Rates, TxHT = 0 */
#define DESC_RATE6M					0x04
#define DESC_RATE9M					0x05
#define DESC_RATE12M				0x06
#define DESC_RATE18M				0x07
#define DESC_RATE24M				0x08
#define DESC_RATE36M				0x09
#define DESC_RATE48M				0x0a
#define DESC_RATE54M				0x0b

/* MCS Rates, TxHT = 1 */
#define DESC_RATEMCS0				0x0c
#define DESC_RATEMCS1				0x0d
#define DESC_RATEMCS2				0x0e
#define DESC_RATEMCS3				0x0f
#define DESC_RATEMCS4				0x10
#define DESC_RATEMCS5				0x11
#define DESC_RATEMCS6				0x12
#define DESC_RATEMCS7				0x13
#define DESC_RATEMCS8				0x14
#define DESC_RATEMCS9				0x15
#define DESC_RATEMCS10				0x16
#define DESC_RATEMCS11				0x17
#define DESC_RATEMCS12				0x18
#define DESC_RATEMCS13				0x19
#define DESC_RATEMCS14				0x1a
#define DESC_RATEMCS15				0x1b
#define DESC_RATEMCS16				0x1C
#define DESC_RATEMCS17				0x1D
#define DESC_RATEMCS18				0x1E
#define DESC_RATEMCS19				0x1F
#define DESC_RATEMCS20				0x20
#define DESC_RATEMCS21				0x21
#define DESC_RATEMCS22				0x22
#define DESC_RATEMCS23				0x23
#define DESC_RATEMCS24				0x24
#define DESC_RATEMCS25				0x25
#define DESC_RATEMCS26				0x26
#define DESC_RATEMCS27				0x27
#define DESC_RATEMCS28				0x28
#define DESC_RATEMCS29				0x29
#define DESC_RATEMCS30				0x2A
#define DESC_RATEMCS31				0x2B
#define DESC_RATEVHTSS1MCS0		0x2C
#define DESC_RATEVHTSS1MCS1		0x2D
#define DESC_RATEVHTSS1MCS2		0x2E
#define DESC_RATEVHTSS1MCS3		0x2F
#define DESC_RATEVHTSS1MCS4		0x30
#define DESC_RATEVHTSS1MCS5		0x31
#define DESC_RATEVHTSS1MCS6		0x32
#define DESC_RATEVHTSS1MCS7		0x33
#define DESC_RATEVHTSS1MCS8		0x34
#define DESC_RATEVHTSS1MCS9		0x35
#define DESC_RATEVHTSS2MCS0		0x36
#define DESC_RATEVHTSS2MCS1		0x37
#define DESC_RATEVHTSS2MCS2		0x38
#define DESC_RATEVHTSS2MCS3		0x39
#define DESC_RATEVHTSS2MCS4		0x3A
#define DESC_RATEVHTSS2MCS5		0x3B
#define DESC_RATEVHTSS2MCS6		0x3C
#define DESC_RATEVHTSS2MCS7		0x3D
#define DESC_RATEVHTSS2MCS8		0x3E
#define DESC_RATEVHTSS2MCS9		0x3F
#define DESC_RATEVHTSS3MCS0		0x40
#define DESC_RATEVHTSS3MCS1		0x41
#define DESC_RATEVHTSS3MCS2		0x42
#define DESC_RATEVHTSS3MCS3		0x43
#define DESC_RATEVHTSS3MCS4		0x44
#define DESC_RATEVHTSS3MCS5		0x45
#define DESC_RATEVHTSS3MCS6		0x46
#define DESC_RATEVHTSS3MCS7		0x47
#define DESC_RATEVHTSS3MCS8		0x48
#define DESC_RATEVHTSS3MCS9		0x49
#define DESC_RATEVHTSS4MCS0		0x4A
#define DESC_RATEVHTSS4MCS1		0x4B
#define DESC_RATEVHTSS4MCS2		0x4C
#define DESC_RATEVHTSS4MCS3		0x4D
#define DESC_RATEVHTSS4MCS4		0x4E
#define DESC_RATEVHTSS4MCS5		0x4F
#define DESC_RATEVHTSS4MCS6		0x50
#define DESC_RATEVHTSS4MCS7		0x51
#define DESC_RATEVHTSS4MCS8		0x52
#define DESC_RATEVHTSS4MCS9		0x53

#define HDATA_RATE(rate)\
	(rate == DESC_RATE1M) ? "CCK_1M" :\
	(rate == DESC_RATE2M) ? "CCK_2M" :\
	(rate == DESC_RATE5_5M) ? "CCK5_5M" :\
	(rate == DESC_RATE11M) ? "CCK_11M" :\
	(rate == DESC_RATE6M) ? "OFDM_6M" :\
	(rate == DESC_RATE9M) ? "OFDM_9M" :\
	(rate == DESC_RATE12M) ? "OFDM_12M" :\
	(rate == DESC_RATE18M) ? "OFDM_18M" :\
	(rate == DESC_RATE24M) ? "OFDM_24M" :\
	(rate == DESC_RATE36M) ? "OFDM_36M" :\
	(rate == DESC_RATE48M) ? "OFDM_48M" :\
	(rate == DESC_RATE54M) ? "OFDM_54M" :\
	(rate == DESC_RATEMCS0) ? "MCS0" :\
	(rate == DESC_RATEMCS1) ? "MCS1" :\
	(rate == DESC_RATEMCS2) ? "MCS2" :\
	(rate == DESC_RATEMCS3) ? "MCS3" :\
	(rate == DESC_RATEMCS4) ? "MCS4" :\
	(rate == DESC_RATEMCS5) ? "MCS5" :\
	(rate == DESC_RATEMCS6) ? "MCS6" :\
	(rate == DESC_RATEMCS7) ? "MCS7" :\
	(rate == DESC_RATEMCS8) ? "MCS8" :\
	(rate == DESC_RATEMCS9) ? "MCS9" :\
	(rate == DESC_RATEMCS10) ? "MCS10" :\
	(rate == DESC_RATEMCS11) ? "MCS11" :\
	(rate == DESC_RATEMCS12) ? "MCS12" :\
	(rate == DESC_RATEMCS13) ? "MCS13" :\
	(rate == DESC_RATEMCS14) ? "MCS14" :\
	(rate == DESC_RATEMCS15) ? "MCS15" :\
	(rate == DESC_RATEMCS16) ? "MCS16" :\
	(rate == DESC_RATEMCS17) ? "MCS17" :\
	(rate == DESC_RATEMCS18) ? "MCS18" :\
	(rate == DESC_RATEMCS19) ? "MCS19" :\
	(rate == DESC_RATEMCS20) ? "MCS20" :\
	(rate == DESC_RATEMCS21) ? "MCS21" :\
	(rate == DESC_RATEMCS22) ? "MCS22" :\
	(rate == DESC_RATEMCS23) ? "MCS23" :\
	(rate == DESC_RATEVHTSS1MCS0) ? "VHTSS1MCS0" :\
	(rate == DESC_RATEVHTSS1MCS1) ? "VHTSS1MCS1" :\
	(rate == DESC_RATEVHTSS1MCS2) ? "VHTSS1MCS2" :\
	(rate == DESC_RATEVHTSS1MCS3) ? "VHTSS1MCS3" :\
	(rate == DESC_RATEVHTSS1MCS4) ? "VHTSS1MCS4" :\
	(rate == DESC_RATEVHTSS1MCS5) ? "VHTSS1MCS5" :\
	(rate == DESC_RATEVHTSS1MCS6) ? "VHTSS1MCS6" :\
	(rate == DESC_RATEVHTSS1MCS7) ? "VHTSS1MCS7" :\
	(rate == DESC_RATEVHTSS1MCS8) ? "VHTSS1MCS8" :\
	(rate == DESC_RATEVHTSS1MCS9) ? "VHTSS1MCS9" :\
	(rate == DESC_RATEVHTSS2MCS0) ? "VHTSS2MCS0" :\
	(rate == DESC_RATEVHTSS2MCS1) ? "VHTSS2MCS1" :\
	(rate == DESC_RATEVHTSS2MCS2) ? "VHTSS2MCS2" :\
	(rate == DESC_RATEVHTSS2MCS3) ? "VHTSS2MCS3" :\
	(rate == DESC_RATEVHTSS2MCS4) ? "VHTSS2MCS4" :\
	(rate == DESC_RATEVHTSS2MCS5) ? "VHTSS2MCS5" :\
	(rate == DESC_RATEVHTSS2MCS6) ? "VHTSS2MCS6" :\
	(rate == DESC_RATEVHTSS2MCS7) ? "VHTSS2MCS7" :\
	(rate == DESC_RATEVHTSS2MCS8) ? "VHTSS2MCS8" :\
	(rate == DESC_RATEVHTSS2MCS9) ? "VHTSS2MCS9" :\
	(rate == DESC_RATEVHTSS3MCS0) ? "VHTSS3MCS0" :\
	(rate == DESC_RATEVHTSS3MCS1) ? "VHTSS3MCS1" :\
	(rate == DESC_RATEVHTSS3MCS2) ? "VHTSS3MCS2" :\
	(rate == DESC_RATEVHTSS3MCS3) ? "VHTSS3MCS3" :\
	(rate == DESC_RATEVHTSS3MCS4) ? "VHTSS3MCS4" :\
	(rate == DESC_RATEVHTSS3MCS5) ? "VHTSS3MCS5" :\
	(rate == DESC_RATEVHTSS3MCS6) ? "VHTSS3MCS6" :\
	(rate == DESC_RATEVHTSS3MCS7) ? "VHTSS3MCS7" :\
	(rate == DESC_RATEVHTSS3MCS8) ? "VHTSS3MCS8" :\
	(rate == DESC_RATEVHTSS3MCS9) ? "VHTSS3MCS9" : "UNKNOWN"

enum {
	UP_LINK,
	DOWN_LINK,
};

enum rt_media_status {
	RT_MEDIA_DISCONNECT = 0,
	RT_MEDIA_CONNECT       = 1
};

#define MAX_DLFW_PAGE_SIZE			4096	/* @ page : 4k bytes */
enum firmware_source {
	FW_SOURCE_IMG_FILE = 0,
	FW_SOURCE_HEADER_FILE = 1,		/* from header file */
};

enum ch_sw_use_case {
	CW_SW_USE_CASE_TDLS		= 0,
	CH_SW_USE_CASE_MCC		= 1
};

enum wakeup_reason {
	RX_PAIRWISEKEY					= 0x01,
	RX_GTK							= 0x02,
	RX_FOURWAY_HANDSHAKE			= 0x03,
	RX_DISASSOC						= 0x04,
	RX_DEAUTH						= 0x08,
	RX_ARP_REQUEST					= 0x09,
	FW_DECISION_DISCONNECT			= 0x10,
	RX_MAGIC_PKT					= 0x21,
	RX_UNICAST_PKT					= 0x22,
	RX_PATTERN_PKT					= 0x23,
	RTD3_SSID_MATCH					= 0x24,
	RX_REALWOW_V2_WAKEUP_PKT		= 0x30,
	RX_REALWOW_V2_ACK_LOST			= 0x31,
	ENABLE_FAIL_DMA_IDLE			= 0x40,
	ENABLE_FAIL_DMA_PAUSE			= 0x41,
	RTIME_FAIL_DMA_IDLE				= 0x42,
	RTIME_FAIL_DMA_PAUSE			= 0x43,
	RX_PNO							= 0x55,
	AP_OFFLOAD_WAKEUP				= 0x66,
	CLK_32K_UNLOCK					= 0xFD,
	CLK_32K_LOCK					= 0xFE
};

/*
 * Queue Select Value in TxDesc
 *   */
#define QSLT_BK							0x2/* 0x01 */
#define QSLT_BE							0x0
#define QSLT_VI							0x5/* 0x4 */
#define QSLT_VO							0x7/* 0x6 */
#define QSLT_BEACON						0x10
#define QSLT_HIGH						0x11
#define QSLT_MGNT						0x12
#define QSLT_CMD						0x13

/* BK, BE, VI, VO, HCCA, MANAGEMENT, COMMAND, HIGH, BEACON.
 * #define MAX_TX_QUEUE		9 */

#define TX_SELE_HQ			BIT(0)		/* High Queue */
#define TX_SELE_LQ			BIT(1)		/* Low Queue */
#define TX_SELE_NQ			BIT(2)		/* Normal Queue */
#define TX_SELE_EQ			BIT(3)		/* Extern Queue */

#define PageNum_128(_Len)		(u32)(((_Len)>>7) + ((_Len) & 0x7F ? 1 : 0))
#define PageNum_256(_Len)		(u32)(((_Len)>>8) + ((_Len) & 0xFF ? 1 : 0))
#define PageNum_512(_Len)		(u32)(((_Len)>>9) + ((_Len) & 0x1FF ? 1 : 0))
#define PageNum(_Len, _Size)		(u32)(((_Len)/(_Size)) + ((_Len)&((_Size) - 1) ? 1 : 0))

struct dbg_rx_counter {
	u32	rx_pkt_ok;
	u32	rx_pkt_crc_error;
	u32	rx_pkt_drop;
	u32	rx_ofdm_fa;
	u32	rx_cck_fa;
	u32	rx_ht_fa;
};

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	void rtw_mbid_cam_init(struct dvobj_priv *dvobj);
	void rtw_mbid_cam_deinit(struct dvobj_priv *dvobj);
	void rtw_mbid_cam_reset(struct adapter *adapter);
	u8 rtw_get_max_mbid_cam_id(struct adapter *adapter);
	u8 rtw_get_mbid_cam_entry_num(struct adapter *adapter);
	int rtw_mbid_cam_cache_dump(void *sel, const char *fun_name , struct adapter *adapter);
	int rtw_mbid_cam_dump(void *sel, const char *fun_name, struct adapter *adapter);
	void rtw_mbid_cam_restore(struct adapter *adapter);
#endif

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	void rtw_hal_set_macaddr_mbid(struct adapter *adapter, u8 *mac_addr);
	void rtw_hal_change_macaddr_mbid(struct adapter *adapter, u8 *mac_addr);
#endif

void rtw_dump_mac_rx_counters(struct adapter *adapt, struct dbg_rx_counter *rx_counter);
void rtw_dump_phy_rx_counters(struct adapter *adapt, struct dbg_rx_counter *rx_counter);
void rtw_reset_mac_rx_counters(struct adapter *adapt);
void rtw_reset_phy_rx_counters(struct adapter *adapt);
void rtw_reset_phy_trx_ok_counters(struct adapter *adapt);

void dump_chip_info(struct hal_version	ChipVersion);
void rtw_hal_config_rftype(struct adapter *  adapt);

#define BAND_CAP_2G			BIT0
#define BAND_CAP_5G			BIT1
#define BAND_CAP_BIT_NUM	2

#define BW_CAP_5M		BIT0
#define BW_CAP_10M		BIT1
#define BW_CAP_20M		BIT2
#define BW_CAP_40M		BIT3
#define BW_CAP_80M		BIT4
#define BW_CAP_160M		BIT5
#define BW_CAP_80_80M	BIT6
#define BW_CAP_BIT_NUM	7

#define PROTO_CAP_11B		BIT0
#define PROTO_CAP_11G		BIT1
#define PROTO_CAP_11N		BIT2
#define PROTO_CAP_11AC		BIT3
#define PROTO_CAP_BIT_NUM	4

#define WL_FUNC_P2P			BIT0
#define WL_FUNC_MIRACAST	BIT1
#define WL_FUNC_TDLS		BIT2
#define WL_FUNC_FTM			BIT3
#define WL_FUNC_BIT_NUM		4

#define TBTT_PROHIBIT_SETUP_TIME 0x04 /* 128us, unit is 32us */
#define TBTT_PROHIBIT_HOLD_TIME 0x80 /* 4ms, unit is 32us*/
#define TBTT_PROHIBIT_HOLD_TIME_STOP_BCN 0x64 /* 3.2ms unit is 32us*/

int hal_spec_init(struct adapter *adapter);
void dump_hal_spec(void *sel, struct adapter *adapter);

bool hal_chk_band_cap(struct adapter *adapter, u8 cap);
bool hal_chk_bw_cap(struct adapter *adapter, u8 cap);
bool hal_chk_proto_cap(struct adapter *adapter, u8 cap);
bool hal_is_band_support(struct adapter *adapter, u8 band);
bool hal_is_bw_support(struct adapter *adapter, u8 bw);
bool hal_is_wireless_mode_support(struct adapter *adapter, u8 mode);
u8 hal_largest_bw(struct adapter *adapter, u8 in_bw);

bool hal_chk_wl_func(struct adapter *adapter, u8 func);

void hal_com_config_channel_plan(
	struct adapter * adapt,
	char *hw_alpha2,
	u8 hw_chplan,
	char *sw_alpha2,
	u8 sw_chplan,
	u8 def_chplan,
	bool AutoLoadFail
);

int hal_config_macaddr(struct adapter *adapter, bool autoload_fail);

bool
HAL_IsLegalChannel(
	struct adapter *	Adapter,
	u32			Channel
);

u8	MRateToHwRate(u8 rate);

u8	hw_rate_to_m_rate(u8 rate);

void	HalSetBrateCfg(
	struct adapter *		Adapter,
	u8			*mBratesOS,
	u16			*pBrateCfg);

bool
Hal_MappingOutPipe(
	struct adapter *	pAdapter,
	u8		NumOutPipe
);

void rtw_dump_fw_info(void *sel, struct adapter *adapter);
void rtw_restore_hw_port_cfg(struct adapter *adapter);
void rtw_restore_mac_addr(struct adapter *adapter);/*set mac addr when hal_init for all iface*/
void rtw_hal_dump_macaddr(void *sel, struct adapter *adapter);

void rtw_init_hal_com_default_value(struct adapter * Adapter);

#ifdef CONFIG_FW_C2H_REG
void c2h_evt_clear(struct adapter *adapter);
int c2h_evt_read_88xx(struct adapter *adapter, u8 *buf);
#endif

void rtw_hal_c2h_pkt_pre_hdl(struct adapter *adapter, u8 *buf, u16 len);
void rtw_hal_c2h_pkt_hdl(struct adapter *adapter, u8 *buf, u16 len);

u8 rtw_get_mgntframe_raid(struct adapter *adapter, unsigned char network_type);

void rtw_hal_update_sta_wset(struct adapter *adapter, struct sta_info *psta);
s8 rtw_get_sta_rx_nss(struct adapter *adapter, struct sta_info *psta);
s8 rtw_get_sta_tx_nss(struct adapter *adapter, struct sta_info *psta);
void rtw_hal_update_sta_ra_info(struct adapter * adapt, struct sta_info *psta);

/* access HW only */
u32 rtw_sec_read_cam(struct adapter *adapter, u8 addr);
void rtw_sec_write_cam(struct adapter *adapter, u8 addr, u32 wdata);
void rtw_sec_read_cam_ent(struct adapter *adapter, u8 id, u8 *ctrl, u8 *mac, u8 *key);
void rtw_sec_write_cam_ent(struct adapter *adapter, u8 id, u16 ctrl, u8 *mac, u8 *key);
void rtw_sec_clr_cam_ent(struct adapter *adapter, u8 id);
bool rtw_sec_read_cam_is_gk(struct adapter *adapter, u8 id);

u8 rtw_hal_rcr_check(struct adapter *adapter, u32 check_bit);

u8 rtw_hal_rcr_add(struct adapter *adapter, u32 add);
u8 rtw_hal_rcr_clear(struct adapter *adapter, u32 clear);
void rtw_hal_rcr_set_chk_bssid(struct adapter *adapter, u8 self_action);

void hw_var_port_switch(struct adapter *adapter);

u8 SetHwReg(struct adapter * adapt, u8 variable, u8 *val);
void GetHwReg(struct adapter * adapt, u8 variable, u8 *val);
void rtw_hal_check_rxfifo_full(struct adapter *adapter);
void rtw_hal_reqtxrpt(struct adapter *adapt, u8 macid);

u8 SetHalDefVar(struct adapter *adapter, enum hal_def_variable variable, void *value);
u8 GetHalDefVar(struct adapter *adapter, enum hal_def_variable variable, void *value);

bool
eqNByte(
	u8	*str1,
	u8	*str2,
	u32	num
);

u32
MapCharToHexDigit(
	char	chTmp
);

bool
GetHexValueFromString(
		char			*szStr,
	u32			*pu4bVal,
	u32			*pu4bMove
);

bool
GetFractionValueFromString(
		char		*szStr,
	u8			*pInteger,
	u8			*pFraction,
	u32		*pu4bMove
);

bool
IsCommentString(
		char		*szStr
);

bool
ParseQualifiedString(
	char *In,
	u32 *Start,
	char *Out,
	char  LeftQualifier,
	char  RightQualifier
);

bool
GetU1ByteIntegerFromStringInDecimal(
		char *Str,
	u8 *pInt
);

bool
isAllSpaceOrTab(
	u8	*data,
	u8	size
);

void linked_info_dump(struct adapter *adapt, u8 benable);
void rtw_store_phy_info(struct adapter *adapt, union recv_frame *prframe);
#define		HWSET_MAX_SIZE			1024
#ifdef CONFIG_EFUSE_CONFIG_FILE
	#define		EFUSE_FILE_COLUMN_NUM		16
	u32 Hal_readPGDataFromConfigFile(struct adapter * adapt);
	u32 Hal_ReadMACAddrFromFile(struct adapter * adapt, u8 *mac_addr);
#endif /* CONFIG_EFUSE_CONFIG_FILE */

int check_phy_efuse_tx_power_info_valid(struct adapter * adapt);
int hal_efuse_macaddr_offset(struct adapter *adapter);
int Hal_GetPhyEfuseMACAddr(struct adapter * adapt, u8 *mac_addr);
void rtw_dump_cur_efuse(struct adapter * adapt);

void rtw_bb_rf_gain_offset(struct adapter *adapt);

void dm_DynamicUsbTxAgg(struct adapter *adapt, u8 from_timer);
u8 rtw_hal_busagg_qsel_check(struct adapter *adapt, u8 pre_qsel, u8 next_qsel);

u8 rtw_get_current_tx_rate(struct adapter *adapt, struct sta_info *psta);
u8 rtw_get_current_tx_sgi(struct adapter *adapt, struct sta_info *psta);

void rtw_hal_set_fw_rsvd_page(struct adapter *adapter, bool finished);
u8 rtw_hal_get_rsvd_page_num(struct adapter *adapter);

#ifdef CONFIG_TSF_RESET_OFFLOAD
int rtw_hal_reset_tsf(struct adapter *adapter, u8 reset_port);
#endif

#if defined(CONFIG_FW_MULTI_PORT_SUPPORT)
int rtw_hal_set_wifi_port_id_cmd(struct adapter *adapter);
#endif

s8 rtw_hal_ch_sw_iqk_info_search(struct adapter *adapt, u8 central_chnl, u8 bw_mode);
void rtw_hal_ch_sw_iqk_info_backup(struct adapter *adapter);
void rtw_hal_ch_sw_iqk_info_restore(struct adapter *adapt, u8 ch_sw_use_case);

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	extern char *rtw_phy_file_path;
	extern char rtw_phy_para_file_path[PATH_LENGTH_MAX];
	#define GetLineFromBuffer(buffer)   strsep(&buffer, "\r\n")
#endif

void update_IOT_info(struct adapter *adapt);

void hal_set_crystal_cap(struct adapter *adapter, u8 crystal_cap);
void rtw_hal_correct_tsf(struct adapter *adapt, u8 hw_port, u64 tsf);

void ResumeTxBeacon(struct adapter *adapt);
void StopTxBeacon(struct adapter *adapt);
#ifdef CONFIG_MI_WITH_MBSSID_CAM /*HW port0 - MBSS*/
	void hw_var_set_opmode_mbid(struct adapter *Adapter, u8 mode);
	u8 rtw_mbid_camid_alloc(struct adapter *adapter, u8 *mac_addr);
#endif

int rtw_hal_get_rsvd_page(struct adapter *adapter, u32 page_offset, u32 page_num, u8 *buffer, u32 buffer_size);
void rtw_hal_construct_beacon(struct adapter *adapt, u8 *pframe, u32 *pLength);
void rtw_hal_construct_NullFunctionData(struct adapter *, u8 *pframe, u32 *pLength,
				u8 *StaAddr, u8 bQoS, u8 AC, u8 bEosp, u8 bForcePowerSave);

void rtw_dump_phy_cap(void *sel, struct adapter *adapter);
void rtw_dump_rsvd_page(void *sel, struct adapter *adapter, u8 page_offset, u8 page_num);

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
int rtw_hal_set_default_port_id_cmd(struct adapter *adapter, u8 mac_id);
int rtw_set_default_port_id(struct adapter *adapter);
int rtw_set_ps_rsvd_page(struct adapter *adapter);
#endif

#ifdef RTW_CHANNEL_SWITCH_OFFLOAD
void rtw_hal_switch_chnl_and_set_bw_offload(struct adapter *adapter, u8 central_ch, u8 pri_ch_idx, u8 bw);
#endif

s16 translate_dbm_to_percentage(s16 signal);

#endif /* __HAL_COMMON_H__ */
