/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#ifndef __HAL_BTCOEX_H__
#define __HAL_BTCOEX_H__

#include <drv_types.h>

/* Some variables can't get from outsrc BT-Coex,
 * so we need to save here */
struct bt_coexist {
	u8 bBtExist;
	u8 btTotalAntNum;
	u8 btChipType;
	u8 bInitlized;
	u8 btAntisolation;
};

void DBG_BT_INFO(u8 *dbgmsg);

void hal_btcoex_SetBTCoexist(struct adapter * adapt, u8 bBtExist);
u8 hal_btcoex_IsBtExist(struct adapter * adapt);
u8 hal_btcoex_IsBtDisabled(struct adapter *);
void hal_btcoex_SetChipType(struct adapter * adapt, u8 chipType);
void hal_btcoex_SetPgAntNum(struct adapter * adapt, u8 antNum);

u8 hal_btcoex_Initialize(struct adapter * adapt);
void hal_btcoex_PowerOnSetting(struct adapter * adapt);
void hal_btcoex_AntInfoSetting(struct adapter * adapt);
void hal_btcoex_PowerOffSetting(struct adapter * adapt);
void hal_btcoex_PreLoadFirmware(struct adapter * adapt);
void hal_btcoex_InitHwConfig(struct adapter * adapt, u8 bWifiOnly);

void hal_btcoex_IpsNotify(struct adapter * adapt, u8 type);
void hal_btcoex_LpsNotify(struct adapter * adapt, u8 type);
void hal_btcoex_ScanNotify(struct adapter * adapt, u8 type);
void hal_btcoex_ConnectNotify(struct adapter * adapt, u8 action);
void hal_btcoex_MediaStatusNotify(struct adapter * adapt, u8 mediaStatus);
void hal_btcoex_SpecialPacketNotify(struct adapter * adapt, u8 pktType);
void hal_btcoex_IQKNotify(struct adapter * adapt, u8 state);
void hal_btcoex_BtInfoNotify(struct adapter * adapt, u8 length, u8 *tmpBuf);
void hal_btcoex_BtMpRptNotify(struct adapter * adapt, u8 length, u8 *tmpBuf);
void hal_btcoex_SuspendNotify(struct adapter * adapt, u8 state);
void hal_btcoex_HaltNotify(struct adapter * adapt, u8 do_halt);
void hal_btcoex_SwitchBtTRxMask(struct adapter * adapt);

void hal_btcoex_Hanlder(struct adapter * adapt);

int hal_btcoex_IsBTCoexRejectAMPDU(struct adapter * adapt);
int hal_btcoex_IsBTCoexCtrlAMPDUSize(struct adapter * adapt);
u32 hal_btcoex_GetAMPDUSize(struct adapter * adapt);
void hal_btcoex_SetManualControl(struct adapter * adapt, u8 bmanual);
u8 hal_btcoex_1Ant(struct adapter * adapt);
u8 hal_btcoex_IsBtControlLps(struct adapter *);
u8 hal_btcoex_IsLpsOn(struct adapter *);
u8 hal_btcoex_RpwmVal(struct adapter *);
u8 hal_btcoex_LpsVal(struct adapter *);
u32 hal_btcoex_GetRaMask(struct adapter *);
void hal_btcoex_RecordPwrMode(struct adapter * adapt, u8 *pCmdBuf, u8 cmdLen);
void hal_btcoex_DisplayBtCoexInfo(struct adapter *, u8 *pbuf, u32 bufsize);
void hal_btcoex_SetDBG(struct adapter *, u32 *pDbgModule);
u32 hal_btcoex_GetDBG(struct adapter *, u8 *pStrBuf, u32 bufSize);
u8 hal_btcoex_IncreaseScanDeviceNum(struct adapter *);
u8 hal_btcoex_IsBtLinkExist(struct adapter *);
void hal_btcoex_SetBtPatchVersion(struct adapter *, u16 btHciVer, u16 btPatchVer);
void hal_btcoex_SetHciVersion(struct adapter *, u16 hciVersion);
void hal_btcoex_SendScanNotify(struct adapter *, u8 type);
void hal_btcoex_StackUpdateProfileInfo(void);
void hal_btcoex_pta_off_on_notify(struct adapter * adapt, u8 bBTON);
void hal_btcoex_SetAntIsolationType(struct adapter * adapt, u8 anttype);
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	int hal_btcoex_AntIsolationConfig_ParaFile(struct adapter *	Adapter, char *pFileName);
	int hal_btcoex_ParseAntIsolationConfigFile(struct adapter * Adapter, char	*buffer);
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */
u16 hal_btcoex_btreg_read(struct adapter * adapt, u8 type, u16 addr, u32 *data);
u16 hal_btcoex_btreg_write(struct adapter * adapt, u8 type, u16 addr, u16 val);
void hal_btcoex_set_rfe_type(u8 type);
void hal_btcoex_switchband_notify(u8 under_scan, u8 band_type);
void hal_btcoex_WlFwDbgInfoNotify(struct adapter * adapt, u8* tmpBuf, u8 length);
void hal_btcoex_rx_rate_change_notify(struct adapter * adapt, bool is_data_frame, u8 rate_id);

#endif /* !__HAL_BTCOEX_H__ */
