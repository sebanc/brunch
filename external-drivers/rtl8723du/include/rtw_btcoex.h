/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#ifndef __RTW_BTCOEX_H__
#define __RTW_BTCOEX_H__

#include <drv_types.h>

/* For H2C: H2C_BT_MP_OPER. Return status definition to the user layer */
enum bt_ctrl_status {
	BT_STATUS_SUCCESS								= 0x00, /* Success */
	BT_STATUS_BT_OP_SUCCESS							= 0x01, /* bt fw op execution success */
	BT_STATUS_H2C_SUCCESS							= 0x02, /* H2c success */
	BT_STATUS_H2C_FAIL								= 0x03, /* H2c fail */
	BT_STATUS_H2C_LENGTH_EXCEEDED					= 0x04, /* H2c command length exceeded */
	BT_STATUS_H2C_TIMTOUT							= 0x05, /* H2c timeout */
	BT_STATUS_H2C_BT_NO_RSP							= 0x06, /* H2c sent, bt no rsp */
	BT_STATUS_C2H_SUCCESS							= 0x07, /* C2h success */
	BT_STATUS_C2H_REQNUM_MISMATCH					= 0x08, /* bt fw wrong rsp */
	BT_STATUS_OPCODE_U_VERSION_MISMATCH				= 0x08, /* Upper layer OP code version mismatch. */
	BT_STATUS_OPCODE_L_VERSION_MISMATCH				= 0x0a, /* Lower layer OP code version mismatch. */
	BT_STATUS_UNKNOWN_OPCODE_U						= 0x0b, /* Unknown Upper layer OP code */
	BT_STATUS_UNKNOWN_OPCODE_L						= 0x0c, /* Unknown Lower layer OP code */
	BT_STATUS_PARAMETER_FORMAT_ERROR_U				= 0x0d, /* Wrong parameters sent by upper layer. */
	BT_STATUS_PARAMETER_FORMAT_ERROR_L				= 0x0e, /* bt fw parameter format is not consistency */
	BT_STATUS_PARAMETER_OUT_OF_RANGE_U				= 0x0f, /* uppery layer parameter value is out of range */
	BT_STATUS_PARAMETER_OUT_OF_RANGE_L				= 0x10, /* bt fw parameter value is out of range */
	BT_STATUS_UNKNOWN_STATUS_L						= 0x11, /* bt returned an defined status code */
	BT_STATUS_UNKNOWN_STATUS_H						= 0x12, /* driver need to do error handle or not handle-well. */
	BT_STATUS_WRONG_LEVEL							= 0x13, /* should be under passive level */
	BT_STATUS_NOT_IMPLEMENT						= 0x14, /* op code not implemented yet */
	BT_STATUS_BT_STACK_OP_SUCCESS					= 0x15, /* bt stack op execution success */
	BT_STATUS_BT_STACK_NOT_SUPPORT					= 0x16, /* stack version not support this. */
	BT_STATUS_BT_STACK_SEND_HCI_EVENT_FAIL			= 0x17, /* send hci event fail */
	BT_STATUS_BT_STACK_NOT_BIND						= 0x18, /* stack not bind wifi driver */
	BT_STATUS_BT_STACK_NO_RSP						= 0x19, /* stack doesn't have any rsp. */
	BT_STATUS_MAX
};

enum btcoex_suspend_state {
	BTCOEX_SUSPEND_STATE_RESUME					= 0x0,
	BTCOEX_SUSPEND_STATE_SUSPEND				= 0x1,
	BTCOEX_SUSPEND_STATE_SUSPEND_KEEP_ANT		= 0x2,
	BTCOEX_SUSPEND_STATE_MAX
};

#define SET_BT_MP_OPER_RET(OpCode, StatusCode)						((OpCode << 8) | StatusCode)
#define GET_OP_CODE_FROM_BT_MP_OPER_RET(RetCode)					((RetCode & 0xF0) >> 8)
#define GET_STATUS_CODE_FROM_BT_MP_OPER_RET(RetCode)				(RetCode & 0x0F)
#define CHECK_STATUS_CODE_FROM_BT_MP_OPER_RET(RetCode, StatusCode)	(GET_STATUS_CODE_FROM_BT_MP_OPER_RET(RetCode) == StatusCode)

#define	PACKET_NORMAL			0
#define	PACKET_DHCP				1
#define	PACKET_ARP				2
#define	PACKET_EAPOL			3

void rtw_btcoex_Initialize(struct adapter *);
void rtw_btcoex_PowerOnSetting(struct adapter * adapt);
void rtw_btcoex_AntInfoSetting(struct adapter * adapt);
void rtw_btcoex_PowerOffSetting(struct adapter * adapt);
void rtw_btcoex_PreLoadFirmware(struct adapter * adapt);
void rtw_btcoex_HAL_Initialize(struct adapter * adapt, u8 bWifiOnly);
void rtw_btcoex_IpsNotify(struct adapter *, u8 type);
void rtw_btcoex_LpsNotify(struct adapter *, u8 type);
void rtw_btcoex_ScanNotify(struct adapter *, u8 type);
void rtw_btcoex_ConnectNotify(struct adapter *, u8 action);
void rtw_btcoex_MediaStatusNotify(struct adapter *, u8 mediaStatus);
void rtw_btcoex_SpecialPacketNotify(struct adapter *, u8 pktType);
void rtw_btcoex_IQKNotify(struct adapter * adapt, u8 state);
void rtw_btcoex_BtInfoNotify(struct adapter *, u8 length, u8 *tmpBuf);
void rtw_btcoex_BtMpRptNotify(struct adapter *, u8 length, u8 *tmpBuf);
void rtw_btcoex_SuspendNotify(struct adapter *, u8 state);
void rtw_btcoex_HaltNotify(struct adapter *);
void rtw_btcoex_switchband_notify(u8 under_scan, u8 band_type);
void rtw_btcoex_WlFwDbgInfoNotify(struct adapter * adapt, u8* tmpBuf, u8 length);
void rtw_btcoex_rx_rate_change_notify(struct adapter * adapt, u8 is_data_frame, u8 rate_id);
void rtw_btcoex_SwitchBtTRxMask(struct adapter *);
void rtw_btcoex_Switch(struct adapter *, u8 enable);
u8 rtw_btcoex_IsBtDisabled(struct adapter *);
void rtw_btcoex_Handler(struct adapter *);
int rtw_btcoex_IsBTCoexRejectAMPDU(struct adapter * adapt);
int rtw_btcoex_IsBTCoexCtrlAMPDUSize(struct adapter *);
u32 rtw_btcoex_GetAMPDUSize(struct adapter *);
void rtw_btcoex_SetManualControl(struct adapter *, u8 bmanual);
u8 rtw_btcoex_1Ant(struct adapter *);
u8 rtw_btcoex_IsBtControlLps(struct adapter *);
u8 rtw_btcoex_IsLpsOn(struct adapter *);
u8 rtw_btcoex_RpwmVal(struct adapter *);
u8 rtw_btcoex_LpsVal(struct adapter *);
u32 rtw_btcoex_GetRaMask(struct adapter *);
void rtw_btcoex_RecordPwrMode(struct adapter *, u8 *pCmdBuf, u8 cmdLen);
void rtw_btcoex_DisplayBtCoexInfo(struct adapter *, u8 *pbuf, u32 bufsize);
void rtw_btcoex_SetDBG(struct adapter *, u32 *pDbgModule);
u32 rtw_btcoex_GetDBG(struct adapter *, u8 *pStrBuf, u32 bufSize);
u8 rtw_btcoex_IncreaseScanDeviceNum(struct adapter *);
u8 rtw_btcoex_IsBtLinkExist(struct adapter *);
void rtw_btcoex_pta_off_on_notify(struct adapter * adapt, u8 bBTON);

u16 rtw_btcoex_btreg_read(struct adapter * adapt, u8 type, u16 addr, u32 *data);
u16 rtw_btcoex_btreg_write(struct adapter * adapt, u8 type, u16 addr, u16 val);
u8 rtw_btcoex_get_bt_coexist(struct adapter * adapt);
u8 rtw_btcoex_get_chip_type(struct adapter * adapt);
u8 rtw_btcoex_get_pg_ant_num(struct adapter * adapt);
u8 rtw_btcoex_get_pg_single_ant_path(struct adapter * adapt);
u8 rtw_btcoex_get_pg_rfe_type(struct adapter * adapt);
u8 rtw_btcoex_is_tfbga_package_type(struct adapter * adapt);
u8 rtw_btcoex_get_ant_div_cfg(struct adapter * adapt);

/* ==================================================
 * Below Functions are called by BT-Coex
 * ================================================== */
void rtw_btcoex_rx_ampdu_apply(struct adapter * adapt);
void rtw_btcoex_LPS_Enter(struct adapter * adapt);
u8 rtw_btcoex_LPS_Leave(struct adapter * adapt);
void rtw_btcoex_set_ant_info(struct adapter * adapt);

#endif /* __RTW_BTCOEX_H__ */

