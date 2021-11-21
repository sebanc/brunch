/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTL8723D_HAL_H__
#define __RTL8723D_HAL_H__

#include "hal_data.h"

#include "rtl8723d_spec.h"
#include "rtl8723d_dm.h"
#include "rtl8723d_recv.h"
#include "rtl8723d_xmit.h"
#include "rtl8723d_cmd.h"
#include "rtl8723d_led.h"
#include "Hal8723DPwrSeq.h"
#include "Hal8723DPhyReg.h"
#include "Hal8723DPhyCfg.h"

#define FW_8723D_SIZE		0x8000
#define FW_8723D_START_ADDRESS	0x1000
#define FW_8723D_END_ADDRESS	0x1FFF /* 0x5FFF */

#define IS_FW_HEADER_EXIST_8723D(_pFwHdr)\
	((le16_to_cpu(_pFwHdr->Signature) & 0xFFF0) == 0x23D0)

struct rt_firmware {
	enum firmware_source	eFWSource;
#ifdef CONFIG_EMBEDDED_FWIMG
	u8			*szFwBuffer;
#else
	u8			szFwBuffer[FW_8723D_SIZE];
#endif
	u32			ulFwLength;
};

/*
 * This structure must be cared byte-ordering
 *
 * Added by tynli. 2009.12.04. */
struct rt_8723d_firmware_hdr {
	/* 8-byte alinment required */

	/* --- LONG WORD 0 ---- */
	__le16		Signature;	/* 92C0: test chip; 92C, 88C0: test chip; 88C1: MP A-cut; 92C1: MP A-cut */
	u8		Category;	/* AP/NIC and USB/PCI */
	u8		Function;	/* Reserved for different FW function indcation, for further use when driver needs to download different FW in different conditions */
	__le16		Version;		/* FW Version */
	__le16		Subversion;	/* FW Subversion, default 0x00 */

	/* --- LONG WORD 1 ---- */
	u8		Month;	/* Release time Month field */
	u8		Date;	/* Release time Date field */
	u8		Hour;	/* Release time Hour field */
	u8		Minute;	/* Release time Minute field */
	__le16		RamCodeSize;	/* The size of RAM code */
	__le16		Rsvd2;

	/* --- LONG WORD 2 ---- */
	__le32		SvnIdx;	/* The SVN entry index */
	__le32		Rsvd3;

	/* --- LONG WORD 3 ---- */
	__le32		Rsvd4;
	__le32		Rsvd5;
};

#define DRIVER_EARLY_INT_TIME_8723D		0x05
#define BCN_DMA_ATIME_INT_TIME_8723D		0x02

/* for 8723D
 * TX 32K, RX 16K, Page size 128B for TX, 8B for RX */
#define PAGE___kernel_size_tX_8723D			128
#define PAGE_SIZE_RX_8723D			8

#define TX_DMA_SIZE_8723D			0x8000	/* 32K(TX) */
#define RX_DMA_SIZE_8723D			0x4000	/* 16K(RX) */

#define RESV_FMWF	0

#ifdef CONFIG_FW_C2H_DEBUG
	#define RX_DMA_RESERVED_SIZE_8723D	0x100	/* 256B, reserved for c2h debug message */
#else
	#define RX_DMA_RESERVED_SIZE_8723D	0x80	/* 128B, reserved for tx report */
#endif
#define RX_DMA_BOUNDARY_8723D\
	(RX_DMA_SIZE_8723D - RX_DMA_RESERVED_SIZE_8723D - 1)


/* Note: We will divide number of page equally for each queue other than public queue! */

/* For General Reserved Page Number(Beacon Queue is reserved page)
 * Beacon:2, PS-Poll:1, Null Data:1,Qos Null Data:1,BT Qos Null Data:1 */
#define BCNQ_PAGE_NUM_8723D		0x08
#ifdef CONFIG_CONCURRENT_MODE
	#define BCNQ1_PAGE_NUM_8723D		0x08 /* 0x04 */
#else
	#define BCNQ1_PAGE_NUM_8723D		0x00
#endif

/* For WoWLan , more reserved page
 * ARP Rsp:1, RWC:1, GTK Info:1,GTK RSP:2,GTK EXT MEM:2, AOAC rpt 1, PNO: 6
 * NS offload: 2 NDP info: 1
 */
#define WOWLAN_PAGE_NUM_8723D	0x00

#define TX_TOTAL_PAGE_NUMBER_8723D\
	(0xFF - BCNQ_PAGE_NUM_8723D - BCNQ1_PAGE_NUM_8723D - WOWLAN_PAGE_NUM_8723D)
#define TX_PAGE_BOUNDARY_8723D		(TX_TOTAL_PAGE_NUMBER_8723D + 1)

#define WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_8723D	TX_TOTAL_PAGE_NUMBER_8723D
#define WMM_NORMAL_TX_PAGE_BOUNDARY_8723D\
	(WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_8723D + 1)

/* For Normal Chip Setting
 * (HPQ + LPQ + NPQ + PUBQ) shall be TX_TOTAL_PAGE_NUMBER_8723D */
#define NORMAL_PAGE_NUM_HPQ_8723D		0x0C
#define NORMAL_PAGE_NUM_LPQ_8723D		0x02
#define NORMAL_PAGE_NUM_NPQ_8723D		0x02
#define NORMAL_PAGE_NUM_EPQ_8723D		0x04

/* Note: For Normal Chip Setting, modify later */
#define WMM_NORMAL_PAGE_NUM_HPQ_8723D		0x30
#define WMM_NORMAL_PAGE_NUM_LPQ_8723D		0x20
#define WMM_NORMAL_PAGE_NUM_NPQ_8723D		0x20
#define WMM_NORMAL_PAGE_NUM_EPQ_8723D		0x00


#include "HalVerDef.h"
#include "hal_com.h"

#define EFUSE_OOB_PROTECT_BYTES (96 + 1)

#define HAL_EFUSE_MEMORY
#define HWSET_MAX_SIZE_8723D                512
#define EFUSE_REAL_CONTENT_LEN_8723D        512
#define EFUSE_MAP_LEN_8723D                 512
#define EFUSE_MAX_SECTION_8723D             64

/* For some inferiority IC purpose. added by Roger, 2009.09.02.*/
#define EFUSE_IC_ID_OFFSET			506
#define AVAILABLE_EFUSE_ADDR(addr)	(addr < EFUSE_REAL_CONTENT_LEN_8723D)

#define EFUSE_ACCESS_ON		0x69
#define EFUSE_ACCESS_OFF	0x00

/* ********************************************************
 *			EFUSE for BT definition
 * ******************************************************** */
#define BANK_NUM			1
#define EFUSE_BT_REAL_BANK_CONTENT_LEN	128
#define EFUSE_BT_REAL_CONTENT_LEN	\
	(EFUSE_BT_REAL_BANK_CONTENT_LEN * BANK_NUM)
#define EFUSE_BT_MAP_LEN		1024	/* 1k bytes */
#define EFUSE_BT_MAX_SECTION		(EFUSE_BT_MAP_LEN / 8)
#define EFUSE_PROTECT_BYTES_BANK	16

enum {
	PACKAGE_DEFAULT,
	PACKAGE_QFN68,
	PACKAGE_TFBGA90,
	PACKAGE_TFBGA80,
	PACKAGE_TFBGA79
};

#define INCLUDE_MULTI_FUNC_BT(_Adapter) \
	(GET_HAL_DATA(_Adapter)->MultiFunc & RT_MULTI_FUNC_BT)
#define INCLUDE_MULTI_FUNC_GPS(_Adapter) \
	(GET_HAL_DATA(_Adapter)->MultiFunc & RT_MULTI_FUNC_GPS)

#ifdef CONFIG_FILE_FWIMG
	extern char *rtw_fw_file_path;
	extern char *rtw_fw_wow_file_path;
#endif /* CONFIG_FILE_FWIMG */

/* rtl8723d_hal_init.c */
int rtl8723d_FirmwareDownload(struct adapter * adapt, bool  bUsedWoWLANFw);
void rtl8723d_FirmwareSelfReset(struct adapter * adapt);
void rtl8723d_InitializeFirmwareVars(struct adapter * adapt);

void rtl8723d_InitAntenna_Selection(struct adapter * adapt);
void rtl8723d_DeinitAntenna_Selection(struct adapter * adapt);
void rtl8723d_CheckAntenna_Selection(struct adapter * adapt);
void rtl8723d_init_default_value(struct adapter * adapt);

int rtl8723d_InitLLTTable(struct adapter * adapt);

int CardDisableHWSM(struct adapter * adapt, u8 resetMCU);
int CardDisableWithoutHWSM(struct adapter * adapt);

/* EFuse */
u8 GetEEPROMSize8723D(struct adapter * adapt);
void Hal_InitPGData(struct adapter * adapt, u8 *PROMContent);
void Hal_EfuseParseIDCode(struct adapter * adapt, u8 *hwinfo);
void Hal_EfuseParseTxPowerInfo_8723D(struct adapter * adapt,
				     u8 *PROMContent, bool AutoLoadFail);
void Hal_EfuseParseBTCoexistInfo_8723D(struct adapter * adapt,
				       u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseEEPROMVer_8723D(struct adapter * adapt,
				   u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParsePackageType_8723D(struct adapter * pAdapter,
				     u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseChnlPlan_8723D(struct adapter * adapt,
				  u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseCustomerID_8723D(struct adapter * adapt,
				    u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseAntennaDiversity_8723D(struct adapter * adapt,
		u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseXtal_8723D(struct adapter * pAdapter,
			      u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseThermalMeter_8723D(struct adapter * adapt,
				      u8 *hwinfo, bool AutoLoadFail);
void Hal_EfuseParseVoltage_8723D(struct adapter * pAdapter,
				 u8 *hwinfo, bool	AutoLoadFail);
void Hal_EfuseParseBoardType_8723D(struct adapter * Adapter,
				   u8	*PROMContent, bool AutoloadFail);

void rtl8723d_set_hal_ops(struct hal_ops *pHalFunc);
void init_hal_spec_8723d(struct adapter *adapter);
u8 SetHwReg8723D(struct adapter * adapt, u8 variable, u8 *val);
void GetHwReg8723D(struct adapter * adapt, u8 variable, u8 *val);
u8 SetHalDefVar8723D(struct adapter * adapt, enum hal_def_variable variable, void *pval);
u8 GetHalDefVar8723D(struct adapter * adapt, enum hal_def_variable variable, void *pval);

/* register */
void rtl8723d_InitBeaconParameters(struct adapter * adapt);
void rtl8723d_InitBeaconMaxError(struct adapter * adapt, u8 InfraMode);
void _InitMacAPLLSetting_8723D(struct adapter * Adapter);
void _8051Reset8723(struct adapter * adapt);

void rtl8723d_start_thread(struct adapter *adapt);
void rtl8723d_stop_thread(struct adapter *adapt);

#if defined(CONFIG_CHECK_BT_HANG)
	void rtl8723ds_init_checkbthang_workqueue(struct adapter *adapter);
	void rtl8723ds_free_checkbthang_workqueue(struct adapter *adapter);
	void rtl8723ds_cancle_checkbthang_workqueue(struct adapter *adapter);
	void rtl8723ds_hal_check_bt_hang(struct adapter *adapter);
#endif

void CCX_FwC2HTxRpt_8723d(struct adapter * adapt, u8 *pdata, u8 len);

u8 MRateToHwRate8723D(u8 rate);
u8 HwRateToMRate8723D(u8 rate);

void Hal_ReadRFGainOffset(struct adapter * pAdapter, u8 *hwinfo, bool AutoLoadFail);

#if defined(CONFIG_CHECK_BT_HANG)
	void check_bt_status_work(void *data);
#endif

void rtl8723d_cal_txdesc_chksum(struct tx_desc *ptxdesc);
int PHY_RF6052_Config8723D(struct adapter * pdapter);

void PHY_RF6052SetBandwidth8723D(struct adapter * Adapter, enum channel_width Bandwidth);

#endif
