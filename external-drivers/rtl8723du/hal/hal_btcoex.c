// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#define __HAL_BTCOEX_C__

#include <hal_data.h>
#include <hal_btcoex.h>
#include "mp_precomp.h"

/* ************************************
 *		Global variables
 * ************************************ */
static const char *const BtProfileString[] = {
	"NONE",
	"A2DP",
	"PAN",
	"HID",
	"SCO",
};

static const char *const BtSpecString[] = {
	"1.0b",
	"1.1",
	"1.2",
	"2.0+EDR",
	"2.1+EDR",
	"3.0+HS",
	"4.0",
};

static const char *const BtLinkRoleString[] = {
	"Master",
	"Slave",
};

static const char *const h2cStaString[] = {
	"successful",
	"h2c busy",
	"rf off",
	"fw not read",
};

static const char *const ioStaString[] = {
	"success",
	"can not IO",
	"rf off",
	"fw not read",
	"wait io timeout",
	"invalid len",
	"idle Q empty",
	"insert waitQ fail",
	"unknown fail",
	"wrong level",
	"h2c stopped",
};

static const char *const GLBtcWifiBwString[] = {
	"11bg",
	"HT20",
	"HT40",
	"HT80",
	"HT160"
};

static const char *const GLBtcWifiFreqString[] = {
	"2.4G",
	"5G"
};

static const char *const GLBtcIotPeerString[] = {
	"UNKNOWN",
	"REALTEK",
	"REALTEK_92SE",
	"BROADCOM",
	"RALINK",
	"ATHEROS",
	"CISCO",
	"MERU",
	"MARVELL",
	"REALTEK_SOFTAP", /* peer is RealTek SOFT_AP, by Bohn, 2009.12.17 */
	"SELF_SOFTAP", /* Self is SoftAP */
	"AIRGO",
	"INTEL",
	"RTK_APCLIENT",
	"REALTEK_81XX",
	"REALTEK_WOW",
	"REALTEK_JAGUAR_BCUTAP",
	"REALTEK_JAGUAR_CCUTAP"
};

static const char *const coexOpcodeString[] = {
	"Wifi status notify",
	"Wifi progress",
	"Wifi info",
	"Power state",
	"Set Control",
	"Get Control"
};

static const char *const coexIndTypeString[] = {
	"bt info",
	"pstdma",
	"limited tx/rx",
	"coex table",
	"request"
};

static const char *const coexH2cResultString[] = {
	"ok",
	"unknown",
	"un opcode",
	"opVer MM",
	"par Err",
	"par OoR",
	"reqNum MM",
	"halMac Fail",
	"h2c TimeOut",
	"Invalid c2h Len",
	"data overflow"
};

#define HALBTCOUTSRC_AGG_CHK_WINDOW_IN_MS	8000

struct btc_coexist GLBtCoexist;
static struct gl_coex_offload coex_offload;
static u8 GLBtcWiFiInScanState;
static u8 GLBtcWiFiInIQKState;
static u8 GLBtcWiFiInIPS;
static u8 GLBtcWiFiInLPS;
static u8 GLBtcBtCoexAliveRegistered;

/*
 * BT control H2C/C2H
 */
/* EXT_EID */
enum {
	C2H_WIFI_FW_ACTIVE_RSP	= 0,
	C2H_TRIG_BY_BT_FW
};

/* C2H_STATUS */
enum {
	BT_STATUS_OK = 0,
	BT_STATUS_VERSION_MISMATCH,
	BT_STATUS_UNKNOWN_OPCODE,
	BT_STATUS_ERROR_PARAMETER
};

/* C2H BT OP CODES */
enum {
	BT_OP_GET_BT_VERSION				= 0x00,
	BT_OP_WRITE_REG_ADDR				= 0x0c,
	BT_OP_WRITE_REG_VALUE				= 0x0d,

	BT_OP_READ_REG					= 0x11,

	BT_LO_OP_GET_AFH_MAP_L				= 0x1e,
	BT_LO_OP_GET_AFH_MAP_M				= 0x1f,
	BT_LO_OP_GET_AFH_MAP_H				= 0x20,

	BT_OP_GET_BT_COEX_SUPPORTED_FEATURE		= 0x2a,
	BT_OP_GET_BT_COEX_SUPPORTED_VERSION		= 0x2b,
	BT_OP_GET_BT_ANT_DET_VAL			= 0x2c,
	BT_OP_GET_BT_BLE_SCAN_PARA			= 0x2d,
	BT_OP_GET_BT_BLE_SCAN_TYPE			= 0x2e,
	BT_OP_GET_BT_DEVICE_INFO			= 0x30,
	BT_OP_GET_BT_FORBIDDEN_SLOT_VAL			= 0x31,
	BT_OP_MAX
};

#define BTC_MPOPER_TIMEOUT	50	/* unit: ms */

#define C2H_MAX_SIZE		16
static u8 GLBtcBtMpOperSeq;
static _mutex GLBtcBtMpOperLock;
static struct timer_list GLBtcBtMpOperTimer;
static struct semaphore GLBtcBtMpRptSema;
static u8 GLBtcBtMpRptSeq;
static u8 GLBtcBtMpRptStatus;
static u8 GLBtcBtMpRptRsp[C2H_MAX_SIZE];
static u8 GLBtcBtMpRptRspSize;
static u8 GLBtcBtMpRptWait;
static u8 GLBtcBtMpRptWiFiOK;
static u8 GLBtcBtMpRptBTOK;

/*
 * Debug
 */
u32 GLBtcDbgType[COMP_MAX];
static u8 GLBtcDbgBuf[BT_TMP_BUF_SIZE];  /* TODO - Remove Global */
u8	gl_btc_trace_buf[BT_TMP_BUF_SIZE];

struct btcoexdbginfo {
	u8 *info;
	u32 size; /* buffer total size */
	u32 len; /* now used length */
};

static struct btcoexdbginfo GLBtcDbgInfo; /* TODO - Remove global */

#define	BT_Operation(Adapter)						false

static void DBG_BT_INFO_INIT(struct btcoexdbginfo * pinfo, u8 *pbuf, u32 size)
{
	if (!pinfo)
		return;

	memset(pinfo, 0, sizeof(struct btcoexdbginfo));

	if (pbuf && size) {
		pinfo->info = pbuf;
		pinfo->size = size;
	}
}

void DBG_BT_INFO(u8 *dbgmsg)
{
	struct btcoexdbginfo * pinfo;
	u32 msglen;
	u8 *pbuf;

	pinfo = &GLBtcDbgInfo;

	if (!pinfo->info)
		return;

	msglen = strlen(dbgmsg);
	if (pinfo->len + msglen > pinfo->size)
		return;

	pbuf = pinfo->info + pinfo->len;
	memcpy(pbuf, dbgmsg, msglen);
	pinfo->len += msglen;
}

/* ************************************
 *		Debug related function
 * ************************************ */
static u8 halbtcoutsrc_IsBtCoexistAvailable(struct btc_coexist * pBtCoexist)
{
	if (!pBtCoexist->bBinded ||
	    !pBtCoexist->Adapter)
		return false;
	return true;
}

static void halbtcoutsrc_DbgInit(void)
{
	u8	i;

	for (i = 0; i < COMP_MAX; i++)
		GLBtcDbgType[i] = 0;
}

static void halbtcoutsrc_EnterPwrLock(struct btc_coexist * pBtCoexist)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj((struct adapter *)pBtCoexist->Adapter);
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);

	_enter_pwrlock(&pwrpriv->lock);
}

static void halbtcoutsrc_ExitPwrLock(struct btc_coexist * pBtCoexist)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj((struct adapter *)pBtCoexist->Adapter);
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);

	up(&pwrpriv->lock);
}

static u8 halbtcoutsrc_IsHwMailboxExist(struct btc_coexist * pBtCoexist)
{
	if (pBtCoexist->board_info.bt_chip_type == BTC_CHIP_CSR_BC4
	    || pBtCoexist->board_info.bt_chip_type == BTC_CHIP_CSR_BC8
	   )
		return false;
	return true;
}

static u8 halbtcoutsrc_LeaveLps(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;


	adapt = pBtCoexist->Adapter;

	pBtCoexist->bt_info.bt_ctrl_lps = true;
	pBtCoexist->bt_info.bt_lps_on = false;

	return rtw_btcoex_LPS_Leave(adapt);
}

static void halbtcoutsrc_EnterLps(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;


	adapt = pBtCoexist->Adapter;

	if (!pBtCoexist->bdontenterLPS) {
		pBtCoexist->bt_info.bt_ctrl_lps = true;
		pBtCoexist->bt_info.bt_lps_on = true;

		rtw_btcoex_LPS_Enter(adapt);
	}
}

static void halbtcoutsrc_NormalLps(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;



	adapt = pBtCoexist->Adapter;

	if (pBtCoexist->bt_info.bt_ctrl_lps) {
		pBtCoexist->bt_info.bt_lps_on = false;
		rtw_btcoex_LPS_Leave(adapt);
		pBtCoexist->bt_info.bt_ctrl_lps = false;
	}
}

static void halbtcoutsrc_Pre_NormalLps(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;

	adapt = pBtCoexist->Adapter;

	if (pBtCoexist->bt_info.bt_ctrl_lps) {
		pBtCoexist->bt_info.bt_lps_on = false;
		rtw_btcoex_LPS_Leave(adapt);
	}
}

static void halbtcoutsrc_Post_NormalLps(struct btc_coexist * pBtCoexist)
{
	if (pBtCoexist->bt_info.bt_ctrl_lps)
		pBtCoexist->bt_info.bt_ctrl_lps = false;
}

/*
 *  Constraint:
 *	   1. this function will request pwrctrl->lock
 */
static void halbtcoutsrc_LeaveLowPower(struct btc_coexist * pBtCoexist)
{
}

/*
 *  Constraint:
 *	   1. this function will request pwrctrl->lock
 */
static void halbtcoutsrc_NormalLowPower(struct btc_coexist * pBtCoexist)
{
}

static void halbtcoutsrc_DisableLowPower(struct btc_coexist * pBtCoexist, u8 bLowPwrDisable)
{
	pBtCoexist->bt_info.bt_disable_low_pwr = bLowPwrDisable;
	if (bLowPwrDisable)
		halbtcoutsrc_LeaveLowPower(pBtCoexist);		/* leave 32k low power. */
	else
		halbtcoutsrc_NormalLowPower(pBtCoexist);	/* original 32k low power behavior. */
}

static void halbtcoutsrc_AggregationCheck(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;
	bool bNeedToAct = false;
	static u32 preTime = 0;
	u32 curTime = 0;

	adapt = pBtCoexist->Adapter;

	/* ===================================== */
	/* To void continuous deleteBA=>addBA=>deleteBA=>addBA */
	/* This function is not allowed to continuous called. */
	/* It can only be called after 8 seconds. */
	/* ===================================== */

	curTime = rtw_systime_to_ms(rtw_get_current_time());
	if ((curTime - preTime) < HALBTCOUTSRC_AGG_CHK_WINDOW_IN_MS)	/* over 8 seconds you can execute this function again. */
		return;
	else
		preTime = curTime;

	if (pBtCoexist->bt_info.reject_agg_pkt) {
		bNeedToAct = true;
		pBtCoexist->bt_info.pre_reject_agg_pkt = pBtCoexist->bt_info.reject_agg_pkt;
	} else {
		if (pBtCoexist->bt_info.pre_reject_agg_pkt) {
			bNeedToAct = true;
			pBtCoexist->bt_info.pre_reject_agg_pkt = pBtCoexist->bt_info.reject_agg_pkt;
		}

		if (pBtCoexist->bt_info.pre_bt_ctrl_agg_buf_size !=
		    pBtCoexist->bt_info.bt_ctrl_agg_buf_size) {
			bNeedToAct = true;
			pBtCoexist->bt_info.pre_bt_ctrl_agg_buf_size = pBtCoexist->bt_info.bt_ctrl_agg_buf_size;
		}

		if (pBtCoexist->bt_info.bt_ctrl_agg_buf_size) {
			if (pBtCoexist->bt_info.pre_agg_buf_size !=
			    pBtCoexist->bt_info.agg_buf_size)
				bNeedToAct = true;
			pBtCoexist->bt_info.pre_agg_buf_size = pBtCoexist->bt_info.agg_buf_size;
		}
	}

	if (bNeedToAct)
		rtw_btcoex_rx_ampdu_apply(adapt);
}

static u8 halbtcoutsrc_is_autoload_fail(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;
	struct hal_com_data * pHalData;

	adapt = pBtCoexist->Adapter;
	pHalData = GET_HAL_DATA(adapt);

	return pHalData->bautoload_fail_flag;
}

static u8 halbtcoutsrc_is_fw_ready(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;

	adapt = pBtCoexist->Adapter;

	return GET_HAL_DATA(adapt)->bFWReady;
}

static u8 halbtcoutsrc_IsDualBandConnected(struct adapter * adapt)
{
	u8 ret = BTC_MULTIPORT_SCC;

	return ret;
}

static u8 halbtcoutsrc_IsWifiBusy(struct adapter * adapt)
{
	if (rtw_mi_check_status(adapt, MI_AP_ASSOC))
		return true;
	if (rtw_mi_busy_traffic_check(adapt, false))
		return true;

	return false;
}

static u32 _halbtcoutsrc_GetWifiLinkStatus(struct adapter * adapt)
{
	struct mlme_priv *pmlmepriv;
	u8 bp2p;
	u32 portConnectedStatus;


	pmlmepriv = &adapt->mlmepriv;
	bp2p = false;
	portConnectedStatus = 0;

	if (!rtw_p2p_chk_state(&adapt->wdinfo, P2P_STATE_NONE))
		bp2p = true;

	if (check_fwstate(pmlmepriv, WIFI_ASOC_STATE)) {
		if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
			if (bp2p)
				portConnectedStatus |= WIFI_P2P_GO_CONNECTED;
			else
				portConnectedStatus |= WIFI_AP_CONNECTED;
		} else {
			if (bp2p)
				portConnectedStatus |= WIFI_P2P_GC_CONNECTED;
			else
				portConnectedStatus |= WIFI_STA_CONNECTED;
		}
	}

	return portConnectedStatus;
}

static u32 halbtcoutsrc_GetWifiLinkStatus(struct btc_coexist * pBtCoexist)
{
	/* ================================= */
	/* return value: */
	/* [31:16]=> connected port number */
	/* [15:0]=> port connected bit define */
	/* ================================ */

	struct adapter * adapt;
	u32 retVal;
	u32 portConnectedStatus, numOfConnectedPort;
	struct dvobj_priv *dvobj;
	struct adapter *iface;
	int i;

	adapt = pBtCoexist->Adapter;
	retVal = 0;
	portConnectedStatus = 0;
	numOfConnectedPort = 0;
	dvobj = adapter_to_dvobj(adapt);

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		if ((iface) && rtw_is_adapter_up(iface)) {
			retVal = _halbtcoutsrc_GetWifiLinkStatus(iface);
			if (retVal) {
				portConnectedStatus |= retVal;
				numOfConnectedPort++;
			}
		}
	}
	retVal = (numOfConnectedPort << 16) | portConnectedStatus;

	return retVal;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void _btmpoper_timer_hdl(void *p)
#else
static void _btmpoper_timer_hdl(struct timer_list *t)
#endif
{
	if (GLBtcBtMpRptWait) {
		GLBtcBtMpRptWait = false;
		up(&GLBtcBtMpRptSema);
	}
}

/*
 * !IMPORTANT!
 *	Before call this function, caller should acquire "GLBtcBtMpOperLock"!
 *	Othrewise there will be racing problem and something may go wrong.
 */
static u8 _btmpoper_cmd(struct btc_coexist * pBtCoexist, u8 opcode, u8 opcodever, u8 *cmd, u8 size)
{
	struct adapter * adapt;
	u8 buf[H2C_BTMP_OPER_LEN] = {0};
	u8 buflen;
	u8 seq;
	int ret;


	if (!cmd && size)
		size = 0;
	if ((size + 2) > H2C_BTMP_OPER_LEN)
		return BT_STATUS_H2C_LENGTH_EXCEEDED;
	buflen = size + 2;

	seq = GLBtcBtMpOperSeq & 0xF;
	GLBtcBtMpOperSeq++;

	buf[0] = (opcodever & 0xF) | (seq << 4);
	buf[1] = opcode;
	if (cmd && size)
		memcpy(buf + 2, cmd, size);

	GLBtcBtMpRptWait = true;
	GLBtcBtMpRptWiFiOK = false;
	GLBtcBtMpRptBTOK = false;
	GLBtcBtMpRptStatus = 0;
	adapt = pBtCoexist->Adapter;
	_set_timer(&GLBtcBtMpOperTimer, BTC_MPOPER_TIMEOUT);
	if (rtw_hal_fill_h2c_cmd(adapt, H2C_BT_MP_OPER, buflen, buf) == _FAIL) {
		_cancel_timer_ex(&GLBtcBtMpOperTimer);
		ret = BT_STATUS_H2C_FAIL;
		goto exit;
	}

	_rtw_down_sema(&GLBtcBtMpRptSema);
	/* GLBtcBtMpRptWait should be false here*/

	if (!GLBtcBtMpRptWiFiOK) {
		RTW_ERR("%s: Didn't get H2C Rsp Event!\n", __func__);
		ret = BT_STATUS_H2C_TIMTOUT;
		goto exit;
	}
	if (!GLBtcBtMpRptBTOK) {
		RTW_DBG("%s: Didn't get BT response!\n", __func__);
		ret = BT_STATUS_H2C_BT_NO_RSP;
		goto exit;
	}

	if (seq != GLBtcBtMpRptSeq) {
		RTW_ERR("%s: Sequence number not match!(%d!=%d)!\n",
			 __func__, seq, GLBtcBtMpRptSeq);
		ret = BT_STATUS_C2H_REQNUM_MISMATCH;
		goto exit;
	}

	switch (GLBtcBtMpRptStatus) {
	/* Examine the status reported from C2H */
	case BT_STATUS_OK:
		ret = BT_STATUS_BT_OP_SUCCESS;
		RTW_DBG("%s: C2H status = BT_STATUS_BT_OP_SUCCESS\n", __func__);
		break;
	case BT_STATUS_VERSION_MISMATCH:
		ret = BT_STATUS_OPCODE_L_VERSION_MISMATCH;
		RTW_DBG("%s: C2H status = BT_STATUS_OPCODE_L_VERSION_MISMATCH\n", __func__);
		break;
	case BT_STATUS_UNKNOWN_OPCODE:
		ret = BT_STATUS_UNKNOWN_OPCODE_L;
		RTW_DBG("%s: C2H status = MP_BT_STATUS_UNKNOWN_OPCODE_L\n", __func__);
		break;
	case BT_STATUS_ERROR_PARAMETER:
		ret = BT_STATUS_PARAMETER_FORMAT_ERROR_L;
		RTW_DBG("%s: C2H status = MP_BT_STATUS_PARAMETER_FORMAT_ERROR_L\n", __func__);
		break;
	default:
		ret = BT_STATUS_UNKNOWN_STATUS_L;
		RTW_DBG("%s: C2H status = MP_BT_STATUS_UNKNOWN_STATUS_L\n", __func__);
		break;
	}

exit:
	return ret;
}

static u32 halbtcoutsrc_GetBtPatchVer(struct btc_coexist * pBtCoexist)
{
	if (pBtCoexist->bt_info.get_bt_fw_ver_cnt <= 5) {
		if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
			unsigned long irqL;
			u8 ret;

			_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

			ret = _btmpoper_cmd(pBtCoexist, BT_OP_GET_BT_VERSION, 0, NULL, 0);
			if (BT_STATUS_BT_OP_SUCCESS == ret) {
				pBtCoexist->bt_info.bt_real_fw_ver = le16_to_cpu(*(__le16 *)GLBtcBtMpRptRsp);
				pBtCoexist->bt_info.bt_fw_ver = *(GLBtcBtMpRptRsp + 2);
				pBtCoexist->bt_info.get_bt_fw_ver_cnt++;
			}

			_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);
		}
	}
	return pBtCoexist->bt_info.bt_real_fw_ver;
}

static int halbtcoutsrc_GetWifiRssi(struct adapter * adapt)
{
	return rtw_phydm_get_min_rssi(adapt);
}

static u32 halbtcoutsrc_GetBtCoexSupportedFeature(void *pBtcContext)
{
	struct btc_coexist * pBtCoexist;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;
	u32 data = 0;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_GET_BT_COEX_SUPPORTED_FEATURE;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			data = le16_to_cpu(*(__le16 *)GLBtcBtMpRptRsp);
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return data;
}

static u32 halbtcoutsrc_GetBtCoexSupportedVersion(void *pBtcContext)
{
	struct btc_coexist * pBtCoexist;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;
	u32 data = 0xFFFF;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_GET_BT_COEX_SUPPORTED_VERSION;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			data = le16_to_cpu(*(__le16 *)GLBtcBtMpRptRsp);
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return data;
}

static u32 halbtcoutsrc_GetBtDeviceInfo(void *pBtcContext)
{
	struct btc_coexist * pBtCoexist;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;
	u32 btDeviceInfo = 0;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_GET_BT_DEVICE_INFO;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			btDeviceInfo = le32_to_cpu(*(__le32 *)GLBtcBtMpRptRsp);
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return btDeviceInfo;
}

static u32 halbtcoutsrc_GetBtForbiddenSlotVal(void *pBtcContext)
{
	struct btc_coexist * pBtCoexist;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;
	u32 btForbiddenSlotVal = 0;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_GET_BT_FORBIDDEN_SLOT_VAL;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			btForbiddenSlotVal = le32_to_cpu(*(__le32 *)GLBtcBtMpRptRsp);
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return btForbiddenSlotVal;
}

static u8 halbtcoutsrc_GetWifiScanAPNum(struct adapter * adapt)
{
	struct mlme_priv *pmlmepriv;
	struct mlme_ext_priv *pmlmeext;
	static u8 scan_AP_num = 0;


	pmlmepriv = &adapt->mlmepriv;
	pmlmeext = &adapt->mlmeextpriv;

	if (!GLBtcWiFiInScanState) {
		if (pmlmepriv->num_of_scanned > 0xFF)
			scan_AP_num = 0xFF;
		else
			scan_AP_num = (u8)pmlmepriv->num_of_scanned;
	}

	return scan_AP_num;
}

static u8 halbtcoutsrc_Get(void *pBtcContext, u8 getType, void *pOutBuf)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;
	struct hal_com_data * pHalData;
	struct mlme_ext_priv *mlmeext;
	u8 bSoftApExist, bVwifiExist;
	u8 *pu8;
	int *pS4Tmp;
	u32 *pU4Tmp;
	u8 *pU1Tmp;
	u16 *pU2Tmp;
	u8 ret;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return false;

	adapt = pBtCoexist->Adapter;
	pHalData = GET_HAL_DATA(adapt);
	mlmeext = &adapt->mlmeextpriv;
	bSoftApExist = false;
	bVwifiExist = false;
	pu8 = (u8 *)pOutBuf;
	pS4Tmp = (int *)pOutBuf;
	pU4Tmp = (u32 *)pOutBuf;
	pU1Tmp = (u8 *)pOutBuf;
	pU2Tmp = (u16*)pOutBuf;
	ret = true;

	switch (getType) {
	case BTC_GET_BL_HS_OPERATION:
		*pu8 = false;
		ret = false;
		break;

	case BTC_GET_BL_HS_CONNECTING:
		*pu8 = false;
		ret = false;
		break;

	case BTC_GET_BL_WIFI_FW_READY:
		*pu8 = halbtcoutsrc_is_fw_ready(pBtCoexist);
		break;

	case BTC_GET_BL_WIFI_CONNECTED:
		*pu8 = (rtw_mi_check_status(adapt, MI_LINKED)) ? true : false;
		break;

	case BTC_GET_BL_WIFI_DUAL_BAND_CONNECTED:
		*pu8 = halbtcoutsrc_IsDualBandConnected(adapt);
		break;

	case BTC_GET_BL_WIFI_BUSY:
		*pu8 = halbtcoutsrc_IsWifiBusy(adapt);
		break;

	case BTC_GET_BL_WIFI_SCAN:
		/* Use the value of the new variable GLBtcWiFiInScanState to judge whether WiFi is in scan state or not, since the originally used flag
			WIFI_SITE_MONITOR in fwstate may not be cleared in time */
		*pu8 = GLBtcWiFiInScanState;
		break;

	case BTC_GET_BL_WIFI_LINK:
		*pu8 = (rtw_mi_check_status(adapt, MI_STA_LINKING)) ? true : false;
		break;

	case BTC_GET_BL_WIFI_ROAM:
		*pu8 = (rtw_mi_check_status(adapt, MI_STA_LINKING)) ? true : false;
		break;

	case BTC_GET_BL_WIFI_4_WAY_PROGRESS:
		*pu8 = false;
		break;

	case BTC_GET_BL_WIFI_UNDER_5G:
		*pu8 = (pHalData->current_band_type == BAND_ON_5G) ? true : false;
		break;

	case BTC_GET_BL_WIFI_AP_MODE_ENABLE:
		*pu8 = (rtw_mi_check_status(adapt, MI_AP_MODE)) ? true : false;
		break;

	case BTC_GET_BL_WIFI_ENABLE_ENCRYPTION:
		*pu8 = adapt->securitypriv.dot11PrivacyAlgrthm == 0 ? false : true;
		break;

	case BTC_GET_BL_WIFI_UNDER_B_MODE:
		if (mlmeext->cur_wireless_mode == WIRELESS_11B)
			*pu8 = true;
		else
			*pu8 = false;
		break;

	case BTC_GET_BL_WIFI_IS_IN_MP_MODE:
		if (adapt->registrypriv.mp_mode == 0)
			*pu8 = false;
		else
			*pu8 = true;
		break;

	case BTC_GET_BL_EXT_SWITCH:
		*pu8 = false;
		break;
	case BTC_GET_BL_IS_ASUS_8723B:
		/* Always return false in linux driver since this case is added only for windows driver */
		*pu8 = false;
		break;

	case BTC_GET_BL_RF4CE_CONNECTED:
		*pu8 = false;
		break;
	case BTC_GET_S4_WIFI_RSSI:
		*pS4Tmp = halbtcoutsrc_GetWifiRssi(adapt);
		break;

	case BTC_GET_S4_HS_RSSI:
		*pS4Tmp = 0;
		ret = false;
		break;

	case BTC_GET_U4_WIFI_BW:
		if (IsLegacyOnly(mlmeext->cur_wireless_mode))
			*pU4Tmp = BTC_WIFI_BW_LEGACY;
		else {
			switch (pHalData->current_channel_bw) {
			case CHANNEL_WIDTH_20:
				*pU4Tmp = BTC_WIFI_BW_HT20;
				break;
			case CHANNEL_WIDTH_40:
				*pU4Tmp = BTC_WIFI_BW_HT40;
				break;
			case CHANNEL_WIDTH_80:
				*pU4Tmp = BTC_WIFI_BW_HT80;
				break;
			case CHANNEL_WIDTH_160:
				*pU4Tmp = BTC_WIFI_BW_HT160;
				break;
			default:
				RTW_INFO("[BTCOEX] unknown bandwidth(%d)\n", pHalData->current_channel_bw);
				*pU4Tmp = BTC_WIFI_BW_HT40;
				break;
			}

		}
		break;

	case BTC_GET_U4_WIFI_TRAFFIC_DIRECTION: {
		struct rt_link_detect *plinkinfo;
		plinkinfo = &adapt->mlmepriv.LinkDetectInfo;

		if (plinkinfo->NumTxOkInPeriod > plinkinfo->NumRxOkInPeriod)
			*pU4Tmp = BTC_WIFI_TRAFFIC_TX;
		else
			*pU4Tmp = BTC_WIFI_TRAFFIC_RX;
	}
		break;

	case BTC_GET_U4_WIFI_FW_VER:
		*pU4Tmp = pHalData->firmware_version << 16;
		*pU4Tmp |= pHalData->firmware_sub_version;
		break;

	case BTC_GET_U4_WIFI_LINK_STATUS:
		*pU4Tmp = halbtcoutsrc_GetWifiLinkStatus(pBtCoexist);
		break;

	case BTC_GET_U4_BT_PATCH_VER:
		*pU4Tmp = halbtcoutsrc_GetBtPatchVer(pBtCoexist);
		break;

	case BTC_GET_U4_VENDOR:
		*pU4Tmp = BTC_VENDOR_OTHER;
		break;

	case BTC_GET_U4_SUPPORTED_VERSION:
		*pU4Tmp = halbtcoutsrc_GetBtCoexSupportedVersion(pBtCoexist);
		break;
	case BTC_GET_U4_SUPPORTED_FEATURE:
		*pU4Tmp = halbtcoutsrc_GetBtCoexSupportedFeature(pBtCoexist);
		break;

	case BTC_GET_U4_BT_DEVICE_INFO:
		*pU4Tmp = halbtcoutsrc_GetBtDeviceInfo(pBtCoexist);
		break;

	case BTC_GET_U4_BT_FORBIDDEN_SLOT_VAL:
		*pU4Tmp = halbtcoutsrc_GetBtForbiddenSlotVal(pBtCoexist);
		break;

	case BTC_GET_U4_WIFI_IQK_TOTAL:
		*pU4Tmp = pHalData->odmpriv.n_iqk_cnt;
		break;

	case BTC_GET_U4_WIFI_IQK_OK:
		*pU4Tmp = pHalData->odmpriv.n_iqk_ok_cnt;
		break;

	case BTC_GET_U4_WIFI_IQK_FAIL:
		*pU4Tmp = pHalData->odmpriv.n_iqk_fail_cnt;
		break;

	case BTC_GET_U1_WIFI_DOT11_CHNL:
		*pU1Tmp = adapt->mlmeextpriv.cur_channel;
		break;

	case BTC_GET_U1_WIFI_CENTRAL_CHNL:
		*pU1Tmp = pHalData->current_channel;
		break;

	case BTC_GET_U1_WIFI_HS_CHNL:
		*pU1Tmp = 0;
		ret = false;
		break;

	case BTC_GET_U1_WIFI_P2P_CHNL:
		{
			struct wifidirect_info *pwdinfo = &(adapt->wdinfo);
			
			*pU1Tmp = pwdinfo->operating_channel;
		}
		break;

	case BTC_GET_U1_MAC_PHY_MODE:
		/*			*pU1Tmp = BTC_SMSP;
		 *			*pU1Tmp = BTC_DMSP;
		 *			*pU1Tmp = BTC_DMDP;
		 *			*pU1Tmp = BTC_MP_UNKNOWN; */
		break;

	case BTC_GET_U1_AP_NUM:
		*pU1Tmp = halbtcoutsrc_GetWifiScanAPNum(adapt);
		break;
	case BTC_GET_U1_ANT_TYPE:
		switch (pHalData->bt_coexist.btAntisolation) {
		case 0:
			*pU1Tmp = (u8)BTC_ANT_TYPE_0;
			pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_0;
			break;
		case 1:
			*pU1Tmp = (u8)BTC_ANT_TYPE_1;
			pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_1;
			break;
		case 2:
			*pU1Tmp = (u8)BTC_ANT_TYPE_2;
			pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_2;
			break;
		case 3:
			*pU1Tmp = (u8)BTC_ANT_TYPE_3;
			pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_3;
			break;
		case 4:
			*pU1Tmp = (u8)BTC_ANT_TYPE_4;
			pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_4;
			break;
		}
		break;
	case BTC_GET_U1_IOT_PEER:
		*pU1Tmp = mlmeext->mlmext_info.assoc_AP_vendor;
		break;

	/* =======1Ant=========== */
	case BTC_GET_U1_LPS_MODE:
		*pU1Tmp = adapt->dvobj->pwrctl_priv.pwr_mode;
		break;

	case BTC_GET_U2_BEACON_PERIOD:
		*pU2Tmp = mlmeext->mlmext_info.bcn_interval;
		break;

	default:
		ret = false;
		break;
	}

	return ret;
}

static u8 halbtcoutsrc_Set(void *pBtcContext, u8 setType, void *pInBuf)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;
	struct hal_com_data * pHalData;
	u8 *pu8;
	u8 *pU1Tmp;
	u32	*pU4Tmp;
	u8 ret;
	u8 result = true;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;
	pHalData = GET_HAL_DATA(adapt);
	pu8 = (u8 *)pInBuf;
	pU1Tmp = (u8 *)pInBuf;
	pU4Tmp = (u32 *)pInBuf;
	ret = true;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return false;

	switch (setType) {
	/* set some u8 type variables. */
	case BTC_SET_BL_BT_DISABLE:
		pBtCoexist->bt_info.bt_disabled = *pu8;
		break;

	case BTC_SET_BL_BT_ENABLE_DISABLE_CHANGE:
		pBtCoexist->bt_info.bt_enable_disable_change = *pu8;
		break;

	case BTC_SET_BL_BT_TRAFFIC_BUSY:
		pBtCoexist->bt_info.bt_busy = *pu8;
		break;

	case BTC_SET_BL_BT_LIMITED_DIG:
		pBtCoexist->bt_info.limited_dig = *pu8;
		break;

	case BTC_SET_BL_FORCE_TO_ROAM:
		pBtCoexist->bt_info.force_to_roam = *pu8;
		break;

	case BTC_SET_BL_TO_REJ_AP_AGG_PKT:
		pBtCoexist->bt_info.reject_agg_pkt = *pu8;
		break;

	case BTC_SET_BL_BT_CTRL_AGG_SIZE:
		pBtCoexist->bt_info.bt_ctrl_agg_buf_size = *pu8;
		break;

	case BTC_SET_BL_INC_SCAN_DEV_NUM:
		pBtCoexist->bt_info.increase_scan_dev_num = *pu8;
		break;

	case BTC_SET_BL_BT_TX_RX_MASK:
		pBtCoexist->bt_info.bt_tx_rx_mask = *pu8;
		break;

	case BTC_SET_BL_MIRACAST_PLUS_BT:
		pBtCoexist->bt_info.miracast_plus_bt = *pu8;
		break;

	/* set some u8 type variables. */
	case BTC_SET_U1_RSSI_ADJ_VAL_FOR_AGC_TABLE_ON:
		pBtCoexist->bt_info.rssi_adjust_for_agc_table_on = *pU1Tmp;
		break;

	case BTC_SET_U1_AGG_BUF_SIZE:
		pBtCoexist->bt_info.agg_buf_size = *pU1Tmp;
		break;

	/* the following are some action which will be triggered */
	case BTC_SET_ACT_GET_BT_RSSI:
		ret = false;
		break;

	case BTC_SET_ACT_AGGREGATE_CTRL:
		halbtcoutsrc_AggregationCheck(pBtCoexist);
		break;

	/* =======1Ant=========== */
	/* set some u8 type variables. */
	case BTC_SET_U1_RSSI_ADJ_VAL_FOR_1ANT_COEX_TYPE:
		pBtCoexist->bt_info.rssi_adjust_for_1ant_coex_type = *pU1Tmp;
		break;

	case BTC_SET_U1_LPS_VAL:
		pBtCoexist->bt_info.lps_val = *pU1Tmp;
		break;

	case BTC_SET_U1_RPWM_VAL:
		pBtCoexist->bt_info.rpwm_val = *pU1Tmp;
		break;

	/* the following are some action which will be triggered */
	case BTC_SET_ACT_LEAVE_LPS:
		result = halbtcoutsrc_LeaveLps(pBtCoexist);
		break;

	case BTC_SET_ACT_ENTER_LPS:
		halbtcoutsrc_EnterLps(pBtCoexist);
		break;

	case BTC_SET_ACT_NORMAL_LPS:
		halbtcoutsrc_NormalLps(pBtCoexist);
		break;

	case BTC_SET_ACT_PRE_NORMAL_LPS:
		halbtcoutsrc_Pre_NormalLps(pBtCoexist);
		break;

	case BTC_SET_ACT_POST_NORMAL_LPS:
		halbtcoutsrc_Post_NormalLps(pBtCoexist);
		break;

	case BTC_SET_ACT_DISABLE_LOW_POWER:
		halbtcoutsrc_DisableLowPower(pBtCoexist, *pu8);
		break;

	case BTC_SET_ACT_UPDATE_RAMASK:
		/*
		pBtCoexist->bt_info.ra_mask = *pU4Tmp;

		if (check_fwstate(&adapt->mlmepriv, WIFI_ASOC_STATE)) {
			struct sta_info *psta;
			struct wlan_bssid_ex * cur_network;

			cur_network = &adapt->mlmeextpriv.mlmext_info.network;
			psta = rtw_get_stainfo(&adapt->stapriv, cur_network->MacAddress);
			rtw_hal_update_ra_mask(psta);
		}
		*/
		break;

	case BTC_SET_ACT_SEND_MIMO_PS: {
		u8 newMimoPsMode = 3;
		struct mlme_ext_priv *pmlmeext = &(adapt->mlmeextpriv);
		struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);

		/* *pU1Tmp = 0 use SM_PS static type */
		/* *pU1Tmp = 1 disable SM_PS */
		if (*pU1Tmp == 0)
			newMimoPsMode = WLAN_HT_CAP_SM_PS_STATIC;
		else if (*pU1Tmp == 1)
			newMimoPsMode = WLAN_HT_CAP_SM_PS_DISABLED;

		if (check_fwstate(&adapt->mlmepriv , WIFI_ASOC_STATE)) {
			/* issue_action_SM_PS(adapt, get_my_bssid(&(pmlmeinfo->network)), newMimoPsMode); */
			issue_action_SM_PS_wait_ack(adapt , get_my_bssid(&(pmlmeinfo->network)) , newMimoPsMode, 3 , 1);
		}
	}
	break;

	case BTC_SET_ACT_CTRL_BT_INFO:
		ret = false;
		break;
	case BTC_SET_ACT_CTRL_BT_COEX:
		ret = false;
		break;
	case BTC_SET_ACT_CTRL_8723B_ANT:
		ret = false;
		break;
	/* ===================== */
	default:
		ret = false;
		break;
	}

	return result;
}

static u8 halbtcoutsrc_UnderIps(struct btc_coexist * pBtCoexist)
{
	struct adapter * adapt;
	struct pwrctrl_priv *pwrpriv;
	u8 bMacPwrCtrlOn;

	adapt = pBtCoexist->Adapter;
	pwrpriv = &adapt->dvobj->pwrctl_priv;
	bMacPwrCtrlOn = false;

	if ((pwrpriv->bips_processing)
	    && (IPS_NONE != pwrpriv->ips_mode_req)
	   )
		return true;

	if (rf_off == pwrpriv->rf_pwrstate)
		return true;

	rtw_hal_get_hwreg(adapt, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if (false == bMacPwrCtrlOn)
		return true;

	return false;
}

static u8 halbtcoutsrc_UnderLps(struct btc_coexist * pBtCoexist)
{
	return GLBtcWiFiInLPS;
}

static u8 halbtcoutsrc_Under32K(struct btc_coexist * pBtCoexist)
{
	/* todo: the method to check whether wifi is under 32K or not */
	return false;
}

static void halbtcoutsrc_DisplayCoexStatistics(struct btc_coexist * pBtCoexist)
{
	struct adapter *		adapt = pBtCoexist->Adapter;
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapt);
	u8				*cliBuf = pBtCoexist->cli_buf;

	if (pHalData->EEPROMBluetoothCoexist == 1) {
		CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s", "============[Coex Status]============");
		CL_PRINTF(cliBuf);
		CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %d ", "IsBtDisabled", rtw_btcoex_IsBtDisabled(adapt));
		CL_PRINTF(cliBuf);
		CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %d ", "IsBtControlLps", rtw_btcoex_IsBtControlLps(adapt));
		CL_PRINTF(cliBuf);
	}
}

static void halbtcoutsrc_DisplayBtLinkInfo(struct btc_coexist * pBtCoexist)
{
}

static void halbtcoutsrc_DisplayWifiStatus(struct btc_coexist * pBtCoexist)
{
	struct adapter *	adapt = pBtCoexist->Adapter;
	u8			*cliBuf = pBtCoexist->cli_buf;
	int			wifiRssi = 0;
	bool	bScan = false, bLink = false, bRoam = false, bWifiBusy = false, bWifiUnderBMode = false;
	u32			wifiBw = BTC_WIFI_BW_HT20, wifiTrafficDir = BTC_WIFI_TRAFFIC_TX, wifiFreq = BTC_FREQ_2_4G;
	u32			wifiLinkStatus = 0x0;
	u8			wifiChnl = 0, wifiP2PChnl = 0, nScanAPNum = 0;
	u32			iqk_cnt_total = 0, iqk_cnt_ok = 0, iqk_cnt_fail = 0;
	u16			wifiBcnInterval = 0;

	wifiLinkStatus = halbtcoutsrc_GetWifiLinkStatus(pBtCoexist);
	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %d/ %d/ %d/ %d/ %d (mcc+2band = %d)", "STA/vWifi/HS/p2pGo/p2pGc",
		((wifiLinkStatus & WIFI_STA_CONNECTED) ? 1 : 0), ((wifiLinkStatus & WIFI_AP_CONNECTED) ? 1 : 0),
		((wifiLinkStatus & WIFI_HS_CONNECTED) ? 1 : 0), ((wifiLinkStatus & WIFI_P2P_GO_CONNECTED) ? 1 : 0),
		((wifiLinkStatus & WIFI_P2P_GC_CONNECTED) ? 1 : 0),
		halbtcoutsrc_IsDualBandConnected(adapt) ? 1 : 0);
	CL_PRINTF(cliBuf);

	pBtCoexist->btc_get(pBtCoexist, BTC_GET_BL_WIFI_SCAN, &bScan);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_BL_WIFI_LINK, &bLink);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_BL_WIFI_ROAM, &bRoam);
	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %d/ %d/ %d ", "Link/ Roam/ Scan",
		bLink, bRoam, bScan);
	CL_PRINTF(cliBuf);	

	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U4_WIFI_IQK_TOTAL, &iqk_cnt_total);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U4_WIFI_IQK_OK, &iqk_cnt_ok);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U4_WIFI_IQK_FAIL, &iqk_cnt_fail);
	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %d/ %d/ %d %s %s",
		"IQK All/ OK/ Fail/AutoLoad/FWDL", iqk_cnt_total, iqk_cnt_ok, iqk_cnt_fail,
		((halbtcoutsrc_is_autoload_fail(pBtCoexist)) ? "fail":"ok"), ((halbtcoutsrc_is_fw_ready(pBtCoexist)) ? "ok":"fail"));
	CL_PRINTF(cliBuf);
	
	if (wifiLinkStatus & WIFI_STA_CONNECTED) {
		CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %s", "IOT Peer", GLBtcIotPeerString[adapt->mlmeextpriv.mlmext_info.assoc_AP_vendor]);
		CL_PRINTF(cliBuf);
	}

	pBtCoexist->btc_get(pBtCoexist, BTC_GET_S4_WIFI_RSSI, &wifiRssi);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U1_WIFI_DOT11_CHNL, &wifiChnl);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U2_BEACON_PERIOD, &wifiBcnInterval);
	if ((wifiLinkStatus & WIFI_P2P_GO_CONNECTED) || (wifiLinkStatus & WIFI_P2P_GC_CONNECTED)) 
		pBtCoexist->btc_get(pBtCoexist, BTC_GET_U1_WIFI_P2P_CHNL, &wifiP2PChnl);	

	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %d dBm/ %d/ %d/ %d", "RSSI/ STA_Chnl/ P2P_Chnl/ BI",
		wifiRssi-100, wifiChnl, wifiP2PChnl, wifiBcnInterval);
	CL_PRINTF(cliBuf);

	pBtCoexist->btc_get(pBtCoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifiFreq);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U4_WIFI_BW, &wifiBw);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U4_WIFI_TRAFFIC_DIRECTION, &wifiTrafficDir);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_BL_WIFI_UNDER_B_MODE, &bWifiUnderBMode);
	pBtCoexist->btc_get(pBtCoexist, BTC_GET_U1_AP_NUM, &nScanAPNum);
	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %s / %s/ %s/ %d ", "Band/ BW/ Traffic/ APCnt",
		GLBtcWifiFreqString[wifiFreq], ((bWifiUnderBMode) ? "11b" : GLBtcWifiBwString[wifiBw]),
		((!bWifiBusy) ? "idle" : ((BTC_WIFI_TRAFFIC_TX == wifiTrafficDir) ? "uplink" : "downlink")),
		   nScanAPNum);
	CL_PRINTF(cliBuf);

	/* power status */
	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %s%s%s", "Power Status", \
		((halbtcoutsrc_UnderIps(pBtCoexist)) ? "IPS ON" : "IPS OFF"),
		((halbtcoutsrc_UnderLps(pBtCoexist)) ? ", LPS ON" : ", LPS OFF"),
		((halbtcoutsrc_Under32K(pBtCoexist)) ? ", 32k" : ""));
	CL_PRINTF(cliBuf);

	CL_SPRINTF(cliBuf, BT_TMP_BUF_SIZE, "\r\n %-35s = %02x %02x %02x %02x %02x %02x (0x%x/0x%x)", "Power mode cmd(lps/rpwm)",
		   pBtCoexist->pwrModeVal[0], pBtCoexist->pwrModeVal[1],
		   pBtCoexist->pwrModeVal[2], pBtCoexist->pwrModeVal[3],
		   pBtCoexist->pwrModeVal[4], pBtCoexist->pwrModeVal[5],
		   pBtCoexist->bt_info.lps_val,
		   pBtCoexist->bt_info.rpwm_val);
	CL_PRINTF(cliBuf);
}

static void halbtcoutsrc_DisplayDbgMsg(void *pBtcContext, u8 dispType)
{
	struct btc_coexist * pBtCoexist;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	switch (dispType) {
	case BTC_DBG_DISP_COEX_STATISTICS:
		halbtcoutsrc_DisplayCoexStatistics(pBtCoexist);
		break;
	case BTC_DBG_DISP_BT_LINK_INFO:
		halbtcoutsrc_DisplayBtLinkInfo(pBtCoexist);
		break;
	case BTC_DBG_DISP_WIFI_STATUS:
		halbtcoutsrc_DisplayWifiStatus(pBtCoexist);
		break;
	default:
		break;
	}
}

/* ************************************
 *		IO related function
 * ************************************ */
static u8 halbtcoutsrc_Read1Byte(void *pBtcContext, u32 RegAddr)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	return rtw_read8(adapt, RegAddr);
}

static u16 halbtcoutsrc_Read2Byte(void *pBtcContext, u32 RegAddr)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	return	rtw_read16(adapt, RegAddr);
}

static u32 halbtcoutsrc_Read4Byte(void *pBtcContext, u32 RegAddr)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	return	rtw_read32(adapt, RegAddr);
}

static void halbtcoutsrc_Write1Byte(void *pBtcContext, u32 RegAddr, u8 Data)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	rtw_write8(adapt, RegAddr, Data);
}

static void halbtcoutsrc_BitMaskWrite1Byte(void *pBtcContext, u32 regAddr, u8 bitMask, u8 data1b)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;
	u8 originalValue, bitShift;
	u8 i;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;
	originalValue = 0;
	bitShift = 0;

	if (bitMask != 0xff) {
		originalValue = rtw_read8(adapt, regAddr);

		for (i = 0; i <= 7; i++) {
			if ((bitMask >> i) & 0x1)
				break;
		}
		bitShift = i;

		data1b = (originalValue & ~bitMask) | ((data1b << bitShift) & bitMask);
	}

	rtw_write8(adapt, regAddr, data1b);
}

static void halbtcoutsrc_Write2Byte(void *pBtcContext, u32 RegAddr, u16 Data)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	rtw_write16(adapt, RegAddr, Data);
}

static void halbtcoutsrc_Write4Byte(void *pBtcContext, u32 RegAddr, u32 Data)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	rtw_write32(adapt, RegAddr, Data);
}

static void halbtcoutsrc_WriteLocalReg1Byte(void *pBtcContext, u32 RegAddr, u8 Data)
{
	struct btc_coexist *		pBtCoexist = (struct btc_coexist *)pBtcContext;
	struct adapter *			Adapter = pBtCoexist->Adapter;

	if (BTC_INTF_SDIO == pBtCoexist->chip_interface)
		rtw_write8(Adapter, SDIO_LOCAL_BASE | RegAddr, Data);
	else
		rtw_write8(Adapter, RegAddr, Data);
}

static void halbtcoutsrc_SetBbReg(void *pBtcContext, u32 RegAddr, u32 BitMask, u32 Data)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	phy_set_bb_reg(adapt, RegAddr, BitMask, Data);
}


static u32 halbtcoutsrc_GetBbReg(void *pBtcContext, u32 RegAddr, u32 BitMask)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	return phy_query_bb_reg(adapt, RegAddr, BitMask);
}

static void halbtcoutsrc_SetRfReg(void *pBtcContext, enum rf_path eRFPath, u32 RegAddr, u32 BitMask, u32 Data)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	phy_set_rf_reg(adapt, eRFPath, RegAddr, BitMask, Data);
}

static u32 halbtcoutsrc_GetRfReg(void *pBtcContext, enum rf_path eRFPath, u32 RegAddr, u32 BitMask)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	return phy_query_rf_reg(adapt, eRFPath, RegAddr, BitMask);
}

static u16 halbtcoutsrc_SetBtReg(void *pBtcContext, u8 RegType, u32 RegAddr, u32 Data)
{
	struct btc_coexist * pBtCoexist;
	u16 ret = BT_STATUS_BT_OP_SUCCESS;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;
		__le32 le_tmp;

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		le_tmp = cpu_to_le32(Data);
		op_code = BT_OP_WRITE_REG_VALUE;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, (u8 *)&le_tmp, 3);
		if (status != BT_STATUS_BT_OP_SUCCESS) {
			ret = SET_BT_MP_OPER_RET(op_code, status);
		} else {
			buf[0] = RegType;
			*(__le16 *)(buf + 1) = cpu_to_le16((u16)RegAddr);
			op_code = BT_OP_WRITE_REG_ADDR;
			status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 3);
			if (status != BT_STATUS_BT_OP_SUCCESS)
				ret = SET_BT_MP_OPER_RET(op_code, status);
		}

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);
	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return ret;
}

static bool halbtcoutsrc_SetBtAntDetection(void *pBtcContext, u8 txTime, u8 btChnl)
{
	/* Always return false since we don't implement this yet */
	return false;
}

static bool
halbtcoutsrc_SetBtTRXMASK(
	void *			pBtcContext,
	u8			bt_trx_mask
	)
{
	/* Always return false since we don't implement this yet */
	return false;
}

static u16 halbtcoutsrc_GetBtReg_with_status(void *pBtcContext, u8 RegType, u32 RegAddr, u32 *data)
{
	struct btc_coexist * pBtCoexist;
	u16 ret = BT_STATUS_BT_OP_SUCCESS;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;

		buf[0] = RegType;
		*(__le16 *)(buf + 1) = cpu_to_le16((u16)RegAddr);

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_READ_REG;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 3);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			*data = le16_to_cpu(*(__le16 *)GLBtcBtMpRptRsp);
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return ret;
}

static u32 halbtcoutsrc_GetBtReg(void *pBtcContext, u8 RegType, u32 RegAddr)
{
	u32 regVal;
	
	return (BT_STATUS_BT_OP_SUCCESS == halbtcoutsrc_GetBtReg_with_status(pBtcContext, RegType, RegAddr, &regVal)) ? regVal : 0xffffffff;
}

static void halbtcoutsrc_FillH2cCmd(void *pBtcContext, u8 elementId, u32 cmdLen, u8 *pCmdBuffer)
{
	struct btc_coexist * pBtCoexist;
	struct adapter * adapt;


	pBtCoexist = (struct btc_coexist *)pBtcContext;
	adapt = pBtCoexist->Adapter;

	rtw_hal_fill_h2c_cmd(adapt, elementId, cmdLen, pCmdBuffer);
}

static void halbtcoutsrc_coex_offload_init(void)
{
	u8	i;

	coex_offload.h2c_req_num = 0;
	coex_offload.cnt_h2c_sent = 0;
	coex_offload.cnt_c2h_ack = 0;
	coex_offload.cnt_c2h_ind = 0;

	for (i = 0; i < COL_MAX_H2C_REQ_NUM; i++)
		init_completion(&coex_offload.c2h_event[i]);
}

static enum col_h2c_status halbtcoutsrc_send_h2c(struct adapter * Adapter, struct COL_H2C * pcol_h2c, u16 h2c_cmd_len)
{
	enum col_h2c_status		h2c_status = COL_STATUS_C2H_OK;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	reinit_completion(&coex_offload.c2h_event[pcol_h2c->req_num]);		/* set event to un signaled state */
#else
	INIT_COMPLETION(coex_offload.c2h_event[pcol_h2c->req_num]);
#endif
	return h2c_status;
}

static enum col_h2c_status halbtcoutsrc_check_c2h_ack(struct adapter * Adapter, struct col_single_h2c_record * pH2cRecord)
{
	enum col_h2c_status	c2h_status = COL_STATUS_C2H_OK;
	struct COL_H2C *		p_h2c_cmd = (struct COL_H2C *)&pH2cRecord->h2c_buf[0];
	u8			req_num = p_h2c_cmd->req_num;
	struct col_c2h_ack *	p_c2h_ack = (struct col_c2h_ack *)&coex_offload.c2h_ack_buf[req_num];


	if ((COL_C2H_ACK_HDR_LEN + p_c2h_ack->ret_len) > coex_offload.c2h_ack_len[req_num]) {
		c2h_status = COL_STATUS_COEX_DATA_OVERFLOW;
		return c2h_status;
	}
	/* else */
	{
		memmove(&pH2cRecord->c2h_ack_buf[0], &coex_offload.c2h_ack_buf[req_num], coex_offload.c2h_ack_len[req_num]);
		pH2cRecord->c2h_ack_len = coex_offload.c2h_ack_len[req_num];
	}


	if (p_c2h_ack->req_num != p_h2c_cmd->req_num) {
		c2h_status = COL_STATUS_C2H_REQ_NUM_MISMATCH;
	} else if (p_c2h_ack->opcode_ver != p_h2c_cmd->opcode_ver) {
		c2h_status = COL_STATUS_C2H_OPCODE_VER_MISMATCH;
	} else {
		c2h_status = p_c2h_ack->status;
	}

	return c2h_status;
}

static enum col_h2c_status halbtcoutsrc_CoexH2cProcess(void *pBtCoexist,
		u8 opcode, u8 opcode_ver, u8 *ph2c_par, u8 h2c_par_len)
{
	struct adapter *			Adapter = ((struct btc_coexist *)pBtCoexist)->Adapter;
	u8				H2C_Parameter[BTC_TMP_BUF_SHORT] = {0};
	struct COL_H2C *			pcol_h2c = (struct COL_H2C *)&H2C_Parameter[0];
	enum col_h2c_status		h2c_status = COL_STATUS_C2H_OK, c2h_status = COL_STATUS_C2H_OK;
	enum col_h2c_status		ret_status = COL_STATUS_C2H_OK;
	u16				col_h2c_len = 0;

	pcol_h2c->opcode = opcode;
	pcol_h2c->opcode_ver = opcode_ver;
	pcol_h2c->req_num = coex_offload.h2c_req_num;
	coex_offload.h2c_req_num++;
	coex_offload.h2c_req_num %= 16;

	memmove(&pcol_h2c->buf[0], ph2c_par, h2c_par_len);


	col_h2c_len = h2c_par_len + 2;	/* 2=sizeof(OPCode, OPCode_version and  Request number) */
	BT_PrintData(Adapter, "[COL], H2C cmd: ", col_h2c_len, H2C_Parameter);

	coex_offload.cnt_h2c_sent++;

	coex_offload.h2c_record[opcode].count++;
	coex_offload.h2c_record[opcode].h2c_len = col_h2c_len;
	memmove((void *)&coex_offload.h2c_record[opcode].h2c_buf[0], (void *)pcol_h2c, col_h2c_len);

	h2c_status = halbtcoutsrc_send_h2c(Adapter, pcol_h2c, col_h2c_len);

	coex_offload.h2c_record[opcode].c2h_ack_len = 0;

	if (COL_STATUS_C2H_OK == h2c_status) {
		/* if reach here, it means H2C get the correct c2h response, */
		c2h_status = halbtcoutsrc_check_c2h_ack(Adapter, &coex_offload.h2c_record[opcode]);
		ret_status = c2h_status;
	} else {
		/* check h2c status error, return error status code to upper layer. */
		ret_status = h2c_status;
	}
	coex_offload.h2c_record[opcode].status[ret_status]++;
	coex_offload.status[ret_status]++;

	return ret_status;
}

static u8 halbtcoutsrc_GetAntDetValFromBt(void *pBtcContext)
{
	/* Always return 0 since we don't implement this yet */
	return 0;
}

static u8 halbtcoutsrc_GetBleScanTypeFromBt(void *pBtcContext)
{
	struct btc_coexist * pBtCoexist;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;
	u8 data = 0;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;


		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_GET_BT_BLE_SCAN_TYPE;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			data = *(u8 *)GLBtcBtMpRptRsp;
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return data;
}

static u32 halbtcoutsrc_GetBleScanParaFromBt(void *pBtcContext, u8 scanType)
{
	struct btc_coexist * pBtCoexist;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;
	u32 data = 0;

	pBtCoexist = (struct btc_coexist *)pBtcContext;

	if (halbtcoutsrc_IsHwMailboxExist(pBtCoexist)) {
		u8 buf[3] = {0};
		unsigned long irqL;
		u8 op_code;
		u8 status;
		

		_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

		op_code = BT_OP_GET_BT_BLE_SCAN_PARA;
		status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
		if (status == BT_STATUS_BT_OP_SUCCESS)
			data = le32_to_cpu(*(__le32 *)GLBtcBtMpRptRsp);
		else
			ret = SET_BT_MP_OPER_RET(op_code, status);

		_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	} else
		ret = BT_STATUS_NOT_IMPLEMENT;

	return data;
}

static u8 halbtcoutsrc_GetBtAFHMapFromBt(void *pBtcContext, u8 mapType, u8 *afhMap)
{
	struct btc_coexist *pBtCoexist = (struct btc_coexist *)pBtcContext;
	u8 buf[2] = {0};
	unsigned long irqL;
	u8 op_code;
	u32 *AfhMapL = (u32 *)&(afhMap[0]);
	u32 *AfhMapM = (u32 *)&(afhMap[4]);
	u16 *AfhMapH = (u16 *)&(afhMap[8]);
	u8 status;
	u32 ret = BT_STATUS_BT_OP_SUCCESS;

	if (!halbtcoutsrc_IsHwMailboxExist(pBtCoexist))
		return false;

	buf[0] = 0;
	buf[1] = mapType;

	_enter_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	op_code = BT_LO_OP_GET_AFH_MAP_L;
	status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
	if (status == BT_STATUS_BT_OP_SUCCESS)
		*AfhMapL = le32_to_cpu(*(__le32 *)GLBtcBtMpRptRsp);
	else {
		ret = SET_BT_MP_OPER_RET(op_code, status);
		goto exit;
	}

	op_code = BT_LO_OP_GET_AFH_MAP_M;
	status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
	if (status == BT_STATUS_BT_OP_SUCCESS) {
		*AfhMapM = le32_to_cpu(*(__le32 *)GLBtcBtMpRptRsp);
	} else {
		ret = SET_BT_MP_OPER_RET(op_code, status);
		goto exit;
	}

	op_code = BT_LO_OP_GET_AFH_MAP_H;
	status = _btmpoper_cmd(pBtCoexist, op_code, 0, buf, 0);
	if (status == BT_STATUS_BT_OP_SUCCESS)
		*AfhMapH = le16_to_cpu(*(__le16 *)GLBtcBtMpRptRsp);
	else {
		ret = SET_BT_MP_OPER_RET(op_code, status);
		goto exit;
	}

exit:

	_exit_critical_mutex(&GLBtcBtMpOperLock, &irqL);

	return (ret == BT_STATUS_BT_OP_SUCCESS) ? true : false;
}

static u32 halbtcoutsrc_GetPhydmVersion(void *pBtcContext)
{
	return RELEASE_VERSION_8723D;
}

static void halbtcoutsrc_phydm_modify_AntDiv_HwSw(void *pBtcContext, u8 is_hw)
{
	/* empty function since we don't need it */
}

static void halbtcoutsrc_phydm_modify_RA_PCR_threshold(void *pBtcContext, u8 RA_offset_direction, u8 RA_threshold_offset)
{
	struct btc_coexist *pBtCoexist = (struct btc_coexist *)pBtcContext;

	phydm_modify_RA_PCR_threshold(pBtCoexist->odm_priv, RA_offset_direction, RA_threshold_offset);
}

static u32 halbtcoutsrc_phydm_query_PHY_counter(void *pBtcContext, u8 info_type)
{
	struct btc_coexist *pBtCoexist = (struct btc_coexist *)pBtcContext;

	return phydm_cmn_info_query((struct PHY_DM_STRUCT *)pBtCoexist->odm_priv, (enum phydm_info_query_e)info_type);
}

/* ************************************
 *		Extern functions called by other module
 * ************************************ */
static u8 EXhalbtcoutsrc_BindBtCoexWithAdapter(void *adapt)
{
	struct btc_coexist *		pBtCoexist = &GLBtCoexist;
	struct hal_com_data	*pHalData = GET_HAL_DATA((struct adapter *)adapt);

	if (pBtCoexist->bBinded)
		return false;
	else
		pBtCoexist->bBinded = true;

	pBtCoexist->statistics.cnt_bind++;

	pBtCoexist->Adapter = adapt;
	pBtCoexist->odm_priv = (void *)&(pHalData->odmpriv);

	pBtCoexist->stack_info.profile_notified = false;

	pBtCoexist->bt_info.bt_ctrl_agg_buf_size = false;
	pBtCoexist->bt_info.agg_buf_size = 5;

	pBtCoexist->bt_info.increase_scan_dev_num = false;
	pBtCoexist->bt_info.miracast_plus_bt = false;

	return true;
}

static void EXhalbtcoutsrc_AntInfoSetting(void *adapt)
{
	struct btc_coexist *		pBtCoexist = &GLBtCoexist;
	u8	antNum = 1, singleAntPath = 0;

	antNum = rtw_btcoex_get_pg_ant_num((struct adapter *)adapt);
	EXhalbtcoutsrc_SetAntNum(BT_COEX_ANT_TYPE_PG, antNum);

	if (antNum == 1) {
		singleAntPath = rtw_btcoex_get_pg_single_ant_path((struct adapter *)adapt);
		EXhalbtcoutsrc_SetSingleAntPath(singleAntPath);
	}

	pBtCoexist->board_info.customerID = RT_CID_DEFAULT;

	/* set default antenna position to main  port */
	pBtCoexist->board_info.btdm_ant_pos = BTC_ANTENNA_AT_MAIN_PORT;

	pBtCoexist->board_info.btdm_ant_det_finish = false;
	pBtCoexist->board_info.btdm_ant_num_by_ant_det = 1;

	pBtCoexist->board_info.tfbga_package = rtw_btcoex_is_tfbga_package_type((struct adapter *)adapt);

	pBtCoexist->board_info.rfe_type = rtw_btcoex_get_pg_rfe_type((struct adapter *)adapt);

	pBtCoexist->board_info.ant_div_cfg = rtw_btcoex_get_ant_div_cfg((struct adapter *)adapt);

}

bool EXhalbtcoutsrc_InitlizeVariables(void *adapt)
{
	struct btc_coexist * pBtCoexist = &GLBtCoexist;

	/* pBtCoexist->statistics.cntBind++; */

	halbtcoutsrc_DbgInit();

	halbtcoutsrc_coex_offload_init();

	pBtCoexist->chip_interface = BTC_INTF_USB;
	EXhalbtcoutsrc_BindBtCoexWithAdapter(adapt);

	pBtCoexist->btc_read_1byte = halbtcoutsrc_Read1Byte;
	pBtCoexist->btc_write_1byte = halbtcoutsrc_Write1Byte;
	pBtCoexist->btc_write_1byte_bitmask = halbtcoutsrc_BitMaskWrite1Byte;
	pBtCoexist->btc_read_2byte = halbtcoutsrc_Read2Byte;
	pBtCoexist->btc_write_2byte = halbtcoutsrc_Write2Byte;
	pBtCoexist->btc_read_4byte = halbtcoutsrc_Read4Byte;
	pBtCoexist->btc_write_4byte = halbtcoutsrc_Write4Byte;
	pBtCoexist->btc_write_local_reg_1byte = halbtcoutsrc_WriteLocalReg1Byte;

	pBtCoexist->btc_set_bb_reg = halbtcoutsrc_SetBbReg;
	pBtCoexist->btc_get_bb_reg = halbtcoutsrc_GetBbReg;

	pBtCoexist->btc_set_rf_reg = halbtcoutsrc_SetRfReg;
	pBtCoexist->btc_get_rf_reg = halbtcoutsrc_GetRfReg;

	pBtCoexist->btc_fill_h2c = halbtcoutsrc_FillH2cCmd;
	pBtCoexist->btc_disp_dbg_msg = halbtcoutsrc_DisplayDbgMsg;

	pBtCoexist->btc_get = halbtcoutsrc_Get;
	pBtCoexist->btc_set = halbtcoutsrc_Set;
	pBtCoexist->btc_get_bt_reg = halbtcoutsrc_GetBtReg;
	pBtCoexist->btc_set_bt_reg = halbtcoutsrc_SetBtReg;
	pBtCoexist->btc_set_bt_ant_detection = halbtcoutsrc_SetBtAntDetection;
	pBtCoexist->btc_set_bt_trx_mask = halbtcoutsrc_SetBtTRXMASK;
	pBtCoexist->btc_coex_h2c_process = halbtcoutsrc_CoexH2cProcess;
	pBtCoexist->btc_get_bt_coex_supported_feature = halbtcoutsrc_GetBtCoexSupportedFeature;
	pBtCoexist->btc_get_bt_coex_supported_version= halbtcoutsrc_GetBtCoexSupportedVersion;
	pBtCoexist->btc_get_ant_det_val_from_bt = halbtcoutsrc_GetAntDetValFromBt;
	pBtCoexist->btc_get_ble_scan_type_from_bt = halbtcoutsrc_GetBleScanTypeFromBt;
	pBtCoexist->btc_get_ble_scan_para_from_bt = halbtcoutsrc_GetBleScanParaFromBt;
	pBtCoexist->btc_get_bt_afh_map_from_bt = halbtcoutsrc_GetBtAFHMapFromBt;
	pBtCoexist->btc_get_bt_phydm_version = halbtcoutsrc_GetPhydmVersion;
	pBtCoexist->btc_phydm_modify_RA_PCR_threshold = halbtcoutsrc_phydm_modify_RA_PCR_threshold;
	pBtCoexist->btc_phydm_query_PHY_counter = halbtcoutsrc_phydm_query_PHY_counter;
	pBtCoexist->btc_phydm_modify_ANTDIV_HwSw = halbtcoutsrc_phydm_modify_AntDiv_HwSw;

	pBtCoexist->cli_buf = &GLBtcDbgBuf[0];

	GLBtcWiFiInScanState = false;

	GLBtcWiFiInIQKState = false;

	GLBtcWiFiInIPS = false;

	GLBtcWiFiInLPS = false;

	GLBtcBtCoexAliveRegistered = false;

	/* BT Control H2C/C2H*/
	GLBtcBtMpOperSeq = 0;
	_rtw_mutex_init(&GLBtcBtMpOperLock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	rtw_init_timer(&GLBtcBtMpOperTimer, adapt, _btmpoper_timer_hdl, pBtCoexist);
#else
	timer_setup(&GLBtcBtMpOperTimer, _btmpoper_timer_hdl, 0);
#endif
	sema_init(&GLBtcBtMpRptSema, 0);
	GLBtcBtMpRptSeq = 0;
	GLBtcBtMpRptStatus = 0;
	memset(GLBtcBtMpRptRsp, 0, C2H_MAX_SIZE);
	GLBtcBtMpRptRspSize = 0;
	GLBtcBtMpRptWait = false;
	GLBtcBtMpRptWiFiOK = false;
	GLBtcBtMpRptBTOK = false;

	return true;
}

void EXhalbtcoutsrc_PowerOnSetting(struct btc_coexist * pBtCoexist)
{
	struct hal_com_data	*pHalData = NULL;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pHalData = GET_HAL_DATA((struct adapter *)pBtCoexist->Adapter);

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_power_on_setting(pBtCoexist);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_power_on_setting(pBtCoexist);
}

void EXhalbtcoutsrc_PreLoadFirmware(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_pre_load_firmware++;

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_pre_load_firmware(pBtCoexist);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_pre_load_firmware(pBtCoexist);
}

static void EXhalbtcoutsrc_init_hw_config(struct btc_coexist * pBtCoexist, u8 bWifiOnly)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_init_hw_config++;

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_init_hw_config(pBtCoexist, bWifiOnly);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_init_hw_config(pBtCoexist, bWifiOnly);
}

static void EXhalbtcoutsrc_init_coex_dm(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_init_coex_dm++;
	pBtCoexist->initilized = true;
	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_init_coex_dm(pBtCoexist);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_init_coex_dm(pBtCoexist);
}

static void EXhalbtcoutsrc_ips_notify(struct btc_coexist * pBtCoexist, u8 type)
{
	u8	ipsType;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_ips_notify++;
	if (pBtCoexist->manual_control)
		return;

	if (IPS_NONE == type) {
		ipsType = BTC_IPS_LEAVE;
		GLBtcWiFiInIPS = false;
	} else {
		ipsType = BTC_IPS_ENTER;
		GLBtcWiFiInIPS = true;
	}

	/* All notify is called in cmd thread, don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_ips_notify(pBtCoexist, ipsType);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_ips_notify(pBtCoexist, ipsType);
}

static void EXhalbtcoutsrc_lps_notify(struct btc_coexist * pBtCoexist, u8 type)
{
	u8 lpsType;


	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_lps_notify++;
	if (pBtCoexist->manual_control)
		return;

	if (PS_MODE_ACTIVE == type) {
		lpsType = BTC_LPS_DISABLE;
		GLBtcWiFiInLPS = false;
	} else {
		lpsType = BTC_LPS_ENABLE;
		GLBtcWiFiInLPS = true;
	}

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_lps_notify(pBtCoexist, lpsType);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_lps_notify(pBtCoexist, lpsType);
}

static void EXhalbtcoutsrc_scan_notify(struct btc_coexist * pBtCoexist, u8 type)
{
	u8	scanType;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;
	pBtCoexist->statistics.cnt_scan_notify++;
	if (pBtCoexist->manual_control)
		return;

	if (type) {
		scanType = BTC_SCAN_START;
		GLBtcWiFiInScanState = true;
	} else {
		scanType = BTC_SCAN_FINISH;
		GLBtcWiFiInScanState = false;
	}

	/* All notify is called in cmd thread, don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_scan_notify(pBtCoexist, scanType);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_scan_notify(pBtCoexist, scanType);
}

void EXhalbtcoutsrc_SetAntennaPathNotify(struct btc_coexist * pBtCoexist, u8 type)
{
}

static void EXhalbtcoutsrc_connect_notify(struct btc_coexist * pBtCoexist, u8 assoType)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;
	pBtCoexist->statistics.cnt_connect_notify++;
	if (pBtCoexist->manual_control)
		return;
	
	/* All notify is called in cmd thread, don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */
	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_connect_notify(pBtCoexist, assoType);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_connect_notify(pBtCoexist, assoType);
}

static void EXhalbtcoutsrc_media_status_notify(struct btc_coexist * pBtCoexist, enum rt_media_status mediaStatus)
{
	u8 mStatus;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_media_status_notify++;
	if (pBtCoexist->manual_control)
		return;

	if (RT_MEDIA_CONNECT == mediaStatus)
		mStatus = BTC_MEDIA_CONNECT;
	else
		mStatus = BTC_MEDIA_DISCONNECT;

	/* All notify is called in cmd thread, don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */
	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_media_status_notify(pBtCoexist, mStatus);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_media_status_notify(pBtCoexist, mStatus);
}

static void EXhalbtcoutsrc_specific_packet_notify(struct btc_coexist * pBtCoexist, u8 pktType)
{
	u8	packetType;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;
	pBtCoexist->statistics.cnt_specific_packet_notify++;
	if (pBtCoexist->manual_control)
		return;

	if (PACKET_DHCP == pktType)
		packetType = BTC_PACKET_DHCP;
	else if (PACKET_EAPOL == pktType)
		packetType = BTC_PACKET_EAPOL;
	else if (PACKET_ARP == pktType)
		packetType = BTC_PACKET_ARP;
	else {
		packetType = BTC_PACKET_UNKNOWN;
		return;
	}

	/* All notify is called in cmd thread, don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */
	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_specific_packet_notify(pBtCoexist, packetType);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_specific_packet_notify(pBtCoexist, packetType);
}

static void EXhalbtcoutsrc_bt_info_notify(struct btc_coexist * pBtCoexist, u8 *tmpBuf, u8 length)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_bt_info_notify++;

	/* All notify is called in cmd thread, don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */
	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_bt_info_notify(pBtCoexist, tmpBuf, length);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_bt_info_notify(pBtCoexist, tmpBuf, length);
}

void EXhalbtcoutsrc_WlFwDbgInfoNotify(struct btc_coexist * pBtCoexist, u8* tmpBuf, u8 length)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_wl_fwdbginfo_notify(pBtCoexist, tmpBuf, length);
	else if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_wl_fwdbginfo_notify(pBtCoexist, tmpBuf, length);
}

void EXhalbtcoutsrc_rx_rate_change_notify(struct btc_coexist * pBtCoexist, bool is_data_frame, u8 btc_rate_id)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->statistics.cnt_rate_id_notify++;

	if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_rx_rate_change_notify(pBtCoexist, is_data_frame, btc_rate_id);
	else if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_rx_rate_change_notify(pBtCoexist, is_data_frame, btc_rate_id);
}

void
EXhalbtcoutsrc_RfStatusNotify(
	struct btc_coexist *		pBtCoexist,
	u8				type
)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;
	pBtCoexist->statistics.cnt_rf_status_notify++;

	if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_rf_status_notify(pBtCoexist, type);
}

void EXhalbtcoutsrc_StackOperationNotify(struct btc_coexist * pBtCoexist, u8 type)
{
}

static void EXhalbtcoutsrc_halt_notify(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_halt_notify(pBtCoexist);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_halt_notify(pBtCoexist);
}

static void EXhalbtcoutsrc_SwitchBtTRxMask(struct btc_coexist * pBtCoexist)
{
}

static void EXhalbtcoutsrc_pnp_notify(struct btc_coexist * pBtCoexist, u8 pnpState)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	/*  */
	/* currently only 1ant we have to do the notification, */
	/* once pnp is notified to sleep state, we have to leave LPS that we can sleep normally. */
	/*  */
	if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_pnp_notify(pBtCoexist, pnpState);
	else if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_pnp_notify(pBtCoexist, pnpState);
}

void EXhalbtcoutsrc_CoexDmSwitch(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;
	pBtCoexist->statistics.cnt_coex_dm_switch++;

	halbtcoutsrc_LeaveLowPower(pBtCoexist);

	if (pBtCoexist->board_info.btdm_ant_num == 1) {
		pBtCoexist->stop_coex_dm = true;
		ex_halbtc8723d1ant_coex_dm_reset(pBtCoexist);
		EXhalbtcoutsrc_SetAntNum(BT_COEX_ANT_TYPE_DETECTED, 2);
		ex_halbtc8723d2ant_init_hw_config(pBtCoexist, false);
		ex_halbtc8723d2ant_init_coex_dm(pBtCoexist);
		pBtCoexist->stop_coex_dm = false;
	}
	halbtcoutsrc_NormalLowPower(pBtCoexist);
}

static void EXhalbtcoutsrc_periodical(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;
	pBtCoexist->statistics.cnt_periodical++;

	/* Periodical should be called in cmd thread, */
	/* don't need to leave low power again
	*	halbtcoutsrc_LeaveLowPower(pBtCoexist); */
	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_periodical(pBtCoexist);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_periodical(pBtCoexist);
}

void EXhalbtcoutsrc_StackUpdateProfileInfo(void)
{
}

void EXhalbtcoutsrc_UpdateMinBtRssi(s8 btRssi)
{
	struct btc_coexist * pBtCoexist = &GLBtCoexist;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->stack_info.min_bt_rssi = btRssi;
}

void EXhalbtcoutsrc_SetHciVersion(u16 hciVersion)
{
	struct btc_coexist * pBtCoexist = &GLBtCoexist;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->stack_info.hci_version = hciVersion;
}

void EXhalbtcoutsrc_SetBtPatchVersion(u16 btHciVersion, u16 btPatchVersion)
{
	struct btc_coexist * pBtCoexist = &GLBtCoexist;

	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	pBtCoexist->bt_info.bt_real_fw_ver = btPatchVersion;
	pBtCoexist->bt_info.bt_hci_ver = btHciVersion;
}

void EXhalbtcoutsrc_SetChipType(u8 chipType)
{
	switch (chipType) {
	default:
	case BT_2WIRE:
	case BT_ISSC_3WIRE:
	case BT_ACCEL:
	case BT_RTL8756:
		GLBtCoexist.board_info.bt_chip_type = BTC_CHIP_UNDEF;
		break;
	case BT_CSR_BC4:
		GLBtCoexist.board_info.bt_chip_type = BTC_CHIP_CSR_BC4;
		break;
	case BT_CSR_BC8:
		GLBtCoexist.board_info.bt_chip_type = BTC_CHIP_CSR_BC8;
		break;
	case BT_RTL8723A:
		GLBtCoexist.board_info.bt_chip_type = BTC_CHIP_RTL8723A;
		break;
	case BT_RTL8821:
		GLBtCoexist.board_info.bt_chip_type = BTC_CHIP_RTL8821;
		break;
	case BT_RTL8723B:
		GLBtCoexist.board_info.bt_chip_type = BTC_CHIP_RTL8723B;
		break;
	}
}

void EXhalbtcoutsrc_SetAntNum(u8 type, u8 antNum)
{
	if (BT_COEX_ANT_TYPE_PG == type) {
		GLBtCoexist.board_info.pg_ant_num = antNum;
		GLBtCoexist.board_info.btdm_ant_num = antNum;
	} else if (BT_COEX_ANT_TYPE_ANTDIV == type) {
		GLBtCoexist.board_info.btdm_ant_num = antNum;
		/* GLBtCoexist.boardInfo.btdmAntPos = BTC_ANTENNA_AT_MAIN_PORT;	 */
	} else if (BT_COEX_ANT_TYPE_DETECTED == type) {
		GLBtCoexist.board_info.btdm_ant_num = antNum;
		/* GLBtCoexist.boardInfo.btdmAntPos = BTC_ANTENNA_AT_MAIN_PORT; */
	}
}

/*
 * Currently used by 8723b only, S0 or S1
 *   */
void EXhalbtcoutsrc_SetSingleAntPath(u8 singleAntPath)
{
	GLBtCoexist.board_info.single_ant_path = singleAntPath;
}

void EXhalbtcoutsrc_DisplayBtCoexInfo(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	halbtcoutsrc_LeaveLowPower(pBtCoexist);

	/* To prevent the racing with IPS enter */
	halbtcoutsrc_EnterPwrLock(pBtCoexist);

	if (pBtCoexist->board_info.btdm_ant_num == 2)
		ex_halbtc8723d2ant_display_coex_info(pBtCoexist);
	else if (pBtCoexist->board_info.btdm_ant_num == 1)
		ex_halbtc8723d1ant_display_coex_info(pBtCoexist);
	halbtcoutsrc_ExitPwrLock(pBtCoexist);

	halbtcoutsrc_NormalLowPower(pBtCoexist);
}

void EXhalbtcoutsrc_DisplayAntDetection(struct btc_coexist * pBtCoexist)
{
	if (!halbtcoutsrc_IsBtCoexistAvailable(pBtCoexist))
		return;

	halbtcoutsrc_LeaveLowPower(pBtCoexist);

	halbtcoutsrc_NormalLowPower(pBtCoexist);
}

static void ex_halbtcoutsrc_pta_off_on_notify(struct btc_coexist * pBtCoexist, u8 bBTON)
{
}

static void EXhalbtcoutsrc_set_rfe_type(u8 type)
{
	GLBtCoexist.board_info.rfe_type= type;
}

static void EXhalbtcoutsrc_switchband_notify(struct btc_coexist *pBtCoexist, u8 type)
{
}

static u8 EXhalbtcoutsrc_rate_id_to_btc_rate_id(u8 rate_id)
{
	u8 btc_rate_id = BTC_UNKNOWN;

	switch (rate_id) {
		/* CCK rates */
		case DESC_RATE1M:
			btc_rate_id = BTC_CCK_1;
			break;
		case DESC_RATE2M:
			btc_rate_id = BTC_CCK_2;
			break;
		case DESC_RATE5_5M:
			btc_rate_id = BTC_CCK_5_5;
			break;
		case DESC_RATE11M:
			btc_rate_id = BTC_CCK_11;
			break;

		/* OFDM rates */
		case DESC_RATE6M:
			btc_rate_id = BTC_OFDM_6;
			break;
		case DESC_RATE9M:
			btc_rate_id = BTC_OFDM_9;
			break;
		case DESC_RATE12M:
			btc_rate_id = BTC_OFDM_12;
			break;
		case DESC_RATE18M:
			btc_rate_id = BTC_OFDM_18;
			break;
		case DESC_RATE24M:
			btc_rate_id = BTC_OFDM_24;
			break;
		case DESC_RATE36M:
			btc_rate_id = BTC_OFDM_36;
			break;
		case DESC_RATE48M:
			btc_rate_id = BTC_OFDM_48;
			break;
		case DESC_RATE54M:
			btc_rate_id = BTC_OFDM_54;
			break;

		/* MCS rates */
		case DESC_RATEMCS0:
			btc_rate_id = BTC_MCS_0;
			break;
		case DESC_RATEMCS1:
			btc_rate_id = BTC_MCS_1;
			break;
		case DESC_RATEMCS2:
			btc_rate_id = BTC_MCS_2;
			break;
		case DESC_RATEMCS3:
			btc_rate_id = BTC_MCS_3;
			break;
		case DESC_RATEMCS4:
			btc_rate_id = BTC_MCS_4;
			break;
		case DESC_RATEMCS5:
			btc_rate_id = BTC_MCS_5;
			break;
		case DESC_RATEMCS6:
			btc_rate_id = BTC_MCS_6;
			break;
		case DESC_RATEMCS7:
			btc_rate_id = BTC_MCS_7;
			break;
		case DESC_RATEMCS8:
			btc_rate_id = BTC_MCS_8;
			break;
		case DESC_RATEMCS9:
			btc_rate_id = BTC_MCS_9;
			break;
		case DESC_RATEMCS10:
			btc_rate_id = BTC_MCS_10;
			break;
		case DESC_RATEMCS11:
			btc_rate_id = BTC_MCS_11;
			break;
		case DESC_RATEMCS12:
			btc_rate_id = BTC_MCS_12;
			break;
		case DESC_RATEMCS13:
			btc_rate_id = BTC_MCS_13;
			break;
		case DESC_RATEMCS14:
			btc_rate_id = BTC_MCS_14;
			break;
		case DESC_RATEMCS15:
			btc_rate_id = BTC_MCS_15;
			break;
		case DESC_RATEMCS16:
			btc_rate_id = BTC_MCS_16;
			break;
		case DESC_RATEMCS17:
			btc_rate_id = BTC_MCS_17;
			break;
		case DESC_RATEMCS18:
			btc_rate_id = BTC_MCS_18;
			break;
		case DESC_RATEMCS19:
			btc_rate_id = BTC_MCS_19;
			break;
		case DESC_RATEMCS20:
			btc_rate_id = BTC_MCS_20;
			break;
		case DESC_RATEMCS21:
			btc_rate_id = BTC_MCS_21;
			break;
		case DESC_RATEMCS22:
			btc_rate_id = BTC_MCS_22;
			break;
		case DESC_RATEMCS23:
			btc_rate_id = BTC_MCS_23;
			break;
		case DESC_RATEMCS24:
			btc_rate_id = BTC_MCS_24;
			break;
		case DESC_RATEMCS25:
			btc_rate_id = BTC_MCS_25;
			break;
		case DESC_RATEMCS26:
			btc_rate_id = BTC_MCS_26;
			break;
		case DESC_RATEMCS27:
			btc_rate_id = BTC_MCS_27;
			break;
		case DESC_RATEMCS28:
			btc_rate_id = BTC_MCS_28;
			break;
		case DESC_RATEMCS29:
			btc_rate_id = BTC_MCS_29;
			break;
		case DESC_RATEMCS30:
			btc_rate_id = BTC_MCS_30;
			break;
		case DESC_RATEMCS31:
			btc_rate_id = BTC_MCS_31;
			break;
			
		case DESC_RATEVHTSS1MCS0:
			btc_rate_id = BTC_VHT_1SS_MCS_0;
			break;
		case DESC_RATEVHTSS1MCS1:
			btc_rate_id = BTC_VHT_1SS_MCS_1;
			break;
		case DESC_RATEVHTSS1MCS2:
			btc_rate_id = BTC_VHT_1SS_MCS_2;
			break;
		case DESC_RATEVHTSS1MCS3:
			btc_rate_id = BTC_VHT_1SS_MCS_3;
			break;
		case DESC_RATEVHTSS1MCS4:
			btc_rate_id = BTC_VHT_1SS_MCS_4;
			break;
		case DESC_RATEVHTSS1MCS5:
			btc_rate_id = BTC_VHT_1SS_MCS_5;
			break;
		case DESC_RATEVHTSS1MCS6:
			btc_rate_id = BTC_VHT_1SS_MCS_6;
			break;
		case DESC_RATEVHTSS1MCS7:
			btc_rate_id = BTC_VHT_1SS_MCS_7;
			break;
		case DESC_RATEVHTSS1MCS8:
			btc_rate_id = BTC_VHT_1SS_MCS_8;
			break;
		case DESC_RATEVHTSS1MCS9:
			btc_rate_id = BTC_VHT_1SS_MCS_9;
			break;

		case DESC_RATEVHTSS2MCS0:
			btc_rate_id = BTC_VHT_2SS_MCS_0;
			break;
		case DESC_RATEVHTSS2MCS1:
			btc_rate_id = BTC_VHT_2SS_MCS_1;
			break;
		case DESC_RATEVHTSS2MCS2:
			btc_rate_id = BTC_VHT_2SS_MCS_2;
			break;
		case DESC_RATEVHTSS2MCS3:
			btc_rate_id = BTC_VHT_2SS_MCS_3;
			break;
		case DESC_RATEVHTSS2MCS4:
			btc_rate_id = BTC_VHT_2SS_MCS_4;
			break;
		case DESC_RATEVHTSS2MCS5:
			btc_rate_id = BTC_VHT_2SS_MCS_5;
			break;
		case DESC_RATEVHTSS2MCS6:
			btc_rate_id = BTC_VHT_2SS_MCS_6;
			break;
		case DESC_RATEVHTSS2MCS7:
			btc_rate_id = BTC_VHT_2SS_MCS_7;
			break;
		case DESC_RATEVHTSS2MCS8:
			btc_rate_id = BTC_VHT_2SS_MCS_8;
			break;
		case DESC_RATEVHTSS2MCS9:
			btc_rate_id = BTC_VHT_2SS_MCS_9;
			break;

		case DESC_RATEVHTSS3MCS0:
			btc_rate_id = BTC_VHT_3SS_MCS_0;
			break;
		case DESC_RATEVHTSS3MCS1:
			btc_rate_id = BTC_VHT_3SS_MCS_1;
			break;
		case DESC_RATEVHTSS3MCS2:
			btc_rate_id = BTC_VHT_3SS_MCS_2;
			break;
		case DESC_RATEVHTSS3MCS3:
			btc_rate_id = BTC_VHT_3SS_MCS_3;
			break;
		case DESC_RATEVHTSS3MCS4:
			btc_rate_id = BTC_VHT_3SS_MCS_4;
			break;
		case DESC_RATEVHTSS3MCS5:
			btc_rate_id = BTC_VHT_3SS_MCS_5;
			break;
		case DESC_RATEVHTSS3MCS6:
			btc_rate_id = BTC_VHT_3SS_MCS_6;
			break;
		case DESC_RATEVHTSS3MCS7:
			btc_rate_id = BTC_VHT_3SS_MCS_7;
			break;
		case DESC_RATEVHTSS3MCS8:
			btc_rate_id = BTC_VHT_3SS_MCS_8;
			break;
		case DESC_RATEVHTSS3MCS9:
			btc_rate_id = BTC_VHT_3SS_MCS_9;
			break;

		case DESC_RATEVHTSS4MCS0:
			btc_rate_id = BTC_VHT_4SS_MCS_0;
			break;
		case DESC_RATEVHTSS4MCS1:
			btc_rate_id = BTC_VHT_4SS_MCS_1;
			break;
		case DESC_RATEVHTSS4MCS2:
			btc_rate_id = BTC_VHT_4SS_MCS_2;
			break;
		case DESC_RATEVHTSS4MCS3:
			btc_rate_id = BTC_VHT_4SS_MCS_3;
			break;
		case DESC_RATEVHTSS4MCS4:
			btc_rate_id = BTC_VHT_4SS_MCS_4;
			break;
		case DESC_RATEVHTSS4MCS5:
			btc_rate_id = BTC_VHT_4SS_MCS_5;
			break;
		case DESC_RATEVHTSS4MCS6:
			btc_rate_id = BTC_VHT_4SS_MCS_6;
			break;
		case DESC_RATEVHTSS4MCS7:
			btc_rate_id = BTC_VHT_4SS_MCS_7;
			break;
		case DESC_RATEVHTSS4MCS8:
			btc_rate_id = BTC_VHT_4SS_MCS_8;
			break;
		case DESC_RATEVHTSS4MCS9:
			btc_rate_id = BTC_VHT_4SS_MCS_9;
			break;
	}
	
	return btc_rate_id;
}

/*
 * Description:
 *	Run BT-Coexist mechansim or not
 *
 */
void hal_btcoex_SetBTCoexist(struct adapter * adapt, u8 bBtExist)
{
	struct hal_com_data *	pHalData;


	pHalData = GET_HAL_DATA(adapt);
	pHalData->bt_coexist.bBtExist = bBtExist;
}

/*
 * Dewcription:
 *	Check is co-exist mechanism enabled or not
 *
 * Return:
 *	true	Enable BT co-exist mechanism
 *	false	Disable BT co-exist mechanism
 */
u8 hal_btcoex_IsBtExist(struct adapter * adapt)
{
	struct hal_com_data *	pHalData;


	pHalData = GET_HAL_DATA(adapt);
	return pHalData->bt_coexist.bBtExist;
}

u8 hal_btcoex_IsBtDisabled(struct adapter * adapt)
{
	if (!hal_btcoex_IsBtExist(adapt))
		return true;

	if (GLBtCoexist.bt_info.bt_disabled)
		return true;
	else
		return false;
}

void hal_btcoex_SetChipType(struct adapter * adapt, u8 chipType)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);
	pHalData->bt_coexist.btChipType = chipType;
}

void hal_btcoex_SetPgAntNum(struct adapter * adapt, u8 antNum)
{
	struct hal_com_data *	pHalData;

	pHalData = GET_HAL_DATA(adapt);

	pHalData->bt_coexist.btTotalAntNum = antNum;
}

u8 hal_btcoex_Initialize(struct adapter * adapt)
{
	memset(&GLBtCoexist, 0, sizeof(GLBtCoexist));

	return EXhalbtcoutsrc_InitlizeVariables((void *)adapt);
}

void hal_btcoex_PowerOnSetting(struct adapter * adapt)
{
	EXhalbtcoutsrc_PowerOnSetting(&GLBtCoexist);
}

void hal_btcoex_AntInfoSetting(struct adapter * adapt)
{
	hal_btcoex_SetBTCoexist(adapt, rtw_btcoex_get_bt_coexist(adapt));
	hal_btcoex_SetChipType(adapt, rtw_btcoex_get_chip_type(adapt));
	hal_btcoex_SetPgAntNum(adapt, rtw_btcoex_get_pg_ant_num(adapt));

	EXhalbtcoutsrc_AntInfoSetting(adapt);
}

void hal_btcoex_PowerOffSetting(struct adapter * adapt)
{
	/* Clear the WiFi on/off bit in scoreboard reg. if necessary */
	rtw_write16(adapt, 0xaa, 0x8000);
}

void hal_btcoex_PreLoadFirmware(struct adapter * adapt)
{
	EXhalbtcoutsrc_PreLoadFirmware(&GLBtCoexist);
}

void hal_btcoex_InitHwConfig(struct adapter * adapt, u8 bWifiOnly)
{
	if (!hal_btcoex_IsBtExist(adapt))
		return;

	EXhalbtcoutsrc_init_hw_config(&GLBtCoexist, bWifiOnly);
	EXhalbtcoutsrc_init_coex_dm(&GLBtCoexist);
}

void hal_btcoex_IpsNotify(struct adapter * adapt, u8 type)
{
	EXhalbtcoutsrc_ips_notify(&GLBtCoexist, type);
}

void hal_btcoex_LpsNotify(struct adapter * adapt, u8 type)
{
	EXhalbtcoutsrc_lps_notify(&GLBtCoexist, type);
}

void hal_btcoex_ScanNotify(struct adapter * adapt, u8 type)
{
	EXhalbtcoutsrc_scan_notify(&GLBtCoexist, type);
}

void hal_btcoex_ConnectNotify(struct adapter * adapt, u8 action)
{
	u8 assoType = 0;
	u8 is_5g_band = false;

	is_5g_band = (adapt->mlmeextpriv.cur_channel > 14) ? true : false;

	if (action) {
		if (is_5g_band)
			assoType = BTC_ASSOCIATE_5G_START;
		else
			assoType = BTC_ASSOCIATE_START;
	}
	else {
		if (is_5g_band)
			assoType = BTC_ASSOCIATE_5G_FINISH;
		else
			assoType = BTC_ASSOCIATE_FINISH;
	}
	
	EXhalbtcoutsrc_connect_notify(&GLBtCoexist, assoType);
}

void hal_btcoex_MediaStatusNotify(struct adapter * adapt, u8 mediaStatus)
{
	EXhalbtcoutsrc_media_status_notify(&GLBtCoexist, mediaStatus);
}

void hal_btcoex_SpecialPacketNotify(struct adapter * adapt, u8 pktType)
{
	EXhalbtcoutsrc_specific_packet_notify(&GLBtCoexist, pktType);
}

void hal_btcoex_IQKNotify(struct adapter * adapt, u8 state)
{
	GLBtcWiFiInIQKState = state;
}

void hal_btcoex_BtInfoNotify(struct adapter * adapt, u8 length, u8 *tmpBuf)
{
	if (GLBtcWiFiInIQKState)
		return;

	EXhalbtcoutsrc_bt_info_notify(&GLBtCoexist, tmpBuf, length);
}

void hal_btcoex_BtMpRptNotify(struct adapter * adapt, u8 length, u8 *tmpBuf)
{
	u8 extid, status, len, seq;


	if (!GLBtcBtMpRptWait)
		return;

	if ((length < 3) || (!tmpBuf))
		return;

	extid = tmpBuf[0];
	/* not response from BT FW then exit*/
	switch (extid) {
	case C2H_WIFI_FW_ACTIVE_RSP:
		GLBtcBtMpRptWiFiOK = true;
		break;

	case C2H_TRIG_BY_BT_FW:
		GLBtcBtMpRptBTOK = true;

		status = tmpBuf[1] & 0xF;
		len = length - 3;
		seq = tmpBuf[2] >> 4;

		GLBtcBtMpRptSeq = seq;
		GLBtcBtMpRptStatus = status;
		memcpy(GLBtcBtMpRptRsp, tmpBuf + 3, len);
		GLBtcBtMpRptRspSize = len;

		break;

	default:
		return;
	}

	if ((GLBtcBtMpRptWiFiOK) && (GLBtcBtMpRptBTOK)) {
		GLBtcBtMpRptWait = false;
		_cancel_timer_ex(&GLBtcBtMpOperTimer);
		up(&GLBtcBtMpRptSema);
	}
}

void hal_btcoex_SuspendNotify(struct adapter * adapt, u8 state)
{
	switch (state) {
	case BTCOEX_SUSPEND_STATE_SUSPEND:
		EXhalbtcoutsrc_pnp_notify(&GLBtCoexist, BTC_WIFI_PNP_SLEEP);
		break;
	case BTCOEX_SUSPEND_STATE_SUSPEND_KEEP_ANT:
		EXhalbtcoutsrc_pnp_notify(&GLBtCoexist, BTC_WIFI_PNP_SLEEP);
		EXhalbtcoutsrc_pnp_notify(&GLBtCoexist, BTC_WIFI_PNP_SLEEP_KEEP_ANT);
		break;
	case BTCOEX_SUSPEND_STATE_RESUME:
#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
		/* re-download FW after resume, inform WL FW port number */
		rtw_hal_set_wifi_port_id_cmd(GLBtCoexist.Adapter);
#endif
		EXhalbtcoutsrc_pnp_notify(&GLBtCoexist, BTC_WIFI_PNP_WAKE_UP);
		break;
	}
}

void hal_btcoex_HaltNotify(struct adapter * adapt, u8 do_halt)
{
	if (do_halt == 1)
		EXhalbtcoutsrc_halt_notify(&GLBtCoexist);

	GLBtCoexist.bBinded = false;
	GLBtCoexist.Adapter = NULL;
}

void hal_btcoex_SwitchBtTRxMask(struct adapter * adapt)
{
	EXhalbtcoutsrc_SwitchBtTRxMask(&GLBtCoexist);
}

void hal_btcoex_Hanlder(struct adapter * adapt)
{
	u32	bt_patch_ver;

	EXhalbtcoutsrc_periodical(&GLBtCoexist);

	if (GLBtCoexist.bt_info.bt_get_fw_ver == 0) {
		GLBtCoexist.btc_get(&GLBtCoexist, BTC_GET_U4_BT_PATCH_VER, &bt_patch_ver);
		GLBtCoexist.bt_info.bt_get_fw_ver = bt_patch_ver;
	}
}

int hal_btcoex_IsBTCoexRejectAMPDU(struct adapter * adapt)
{
	return (int)GLBtCoexist.bt_info.reject_agg_pkt;
}

int hal_btcoex_IsBTCoexCtrlAMPDUSize(struct adapter * adapt)
{
	return (int)GLBtCoexist.bt_info.bt_ctrl_agg_buf_size;
}

u32 hal_btcoex_GetAMPDUSize(struct adapter * adapt)
{
	return (u32)GLBtCoexist.bt_info.agg_buf_size;
}

void hal_btcoex_SetManualControl(struct adapter * adapt, u8 bmanual)
{
	GLBtCoexist.manual_control = bmanual;
}

u8 hal_btcoex_1Ant(struct adapter * adapt)
{
	if (!hal_btcoex_IsBtExist(adapt))
		return false;

	if (GLBtCoexist.board_info.btdm_ant_num == 1)
		return true;

	return false;
}

u8 hal_btcoex_IsBtControlLps(struct adapter * adapt)
{
	if (GLBtCoexist.bdontenterLPS)
		return true;
	
	if (!hal_btcoex_IsBtExist(adapt))
		return false;

	if (GLBtCoexist.bt_info.bt_disabled)
		return false;

	if (GLBtCoexist.bt_info.bt_ctrl_lps)
		return true;

	return false;
}

u8 hal_btcoex_IsLpsOn(struct adapter * adapt)
{
	if (GLBtCoexist.bdontenterLPS)
		return false;
	
	if (!hal_btcoex_IsBtExist(adapt))
		return false;

	if (GLBtCoexist.bt_info.bt_disabled)
		return false;

	if (GLBtCoexist.bt_info.bt_lps_on)
		return true;

	return false;
}

u8 hal_btcoex_RpwmVal(struct adapter * adapt)
{
	return GLBtCoexist.bt_info.rpwm_val;
}

u8 hal_btcoex_LpsVal(struct adapter * adapt)
{
	return GLBtCoexist.bt_info.lps_val;
}

u32 hal_btcoex_GetRaMask(struct adapter * adapt)
{
	if (!hal_btcoex_IsBtExist(adapt))
		return 0;

	if (GLBtCoexist.bt_info.bt_disabled)
		return 0;

	/* Modify by YiWei , suggest by Cosa and Jenyu
	 * Remove the limit antenna number , because 2 antenna case (ex: 8192eu)also want to get BT coex report rate mask.
	 */
	/*if (GLBtCoexist.board_info.btdm_ant_num != 1)
		return 0;*/

	return GLBtCoexist.bt_info.ra_mask;
}

void hal_btcoex_RecordPwrMode(struct adapter * adapt, u8 *pCmdBuf, u8 cmdLen)
{

	memcpy(GLBtCoexist.pwrModeVal, pCmdBuf, cmdLen);
}

void hal_btcoex_DisplayBtCoexInfo(struct adapter * adapt, u8 *pbuf, u32 bufsize)
{
	struct btcoexdbginfo * pinfo;


	pinfo = &GLBtcDbgInfo;
	DBG_BT_INFO_INIT(pinfo, pbuf, bufsize);
	EXhalbtcoutsrc_DisplayBtCoexInfo(&GLBtCoexist);
	DBG_BT_INFO_INIT(pinfo, NULL, 0);
}

void hal_btcoex_SetDBG(struct adapter * adapt, u32 *pDbgModule)
{
	u32 i;


	if (!pDbgModule)
		return;

	for (i = 0; i < COMP_MAX; i++)
		GLBtcDbgType[i] = pDbgModule[i];
}

u32 hal_btcoex_GetDBG(struct adapter * adapt, u8 *pStrBuf, u32 bufSize)
{
	int count;
	u8 *pstr;
	u32 leftSize;


	if ((!pStrBuf) || (0 == bufSize))
		return 0;

	count = 0;
	pstr = pStrBuf;
	leftSize = bufSize;
	/*	RTW_INFO(FUNC_ADPT_FMT ": bufsize=%d\n", FUNC_ADPT_ARG(adapt), bufSize); */

	count = rtw_sprintf(pstr, leftSize, "#define DBG\t%d\n", DBG);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

	count = rtw_sprintf(pstr, leftSize, "BTCOEX Debug Setting:\n");
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

	count = rtw_sprintf(pstr, leftSize,
			    "COMP_COEX: 0x%08X\n\n",
			    GLBtcDbgType[COMP_COEX]);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

exit:
	count = pstr - pStrBuf;
	/*	RTW_INFO(FUNC_ADPT_FMT ": usedsize=%d\n", FUNC_ADPT_ARG(adapt), count); */

	return count;
}

u8 hal_btcoex_IncreaseScanDeviceNum(struct adapter * adapt)
{
	if (!hal_btcoex_IsBtExist(adapt))
		return false;

	if (GLBtCoexist.bt_info.increase_scan_dev_num)
		return true;

	return false;
}

u8 hal_btcoex_IsBtLinkExist(struct adapter * adapt)
{
	if (GLBtCoexist.bt_link_info.bt_link_exist)
		return true;

	return false;
}

void hal_btcoex_SetBtPatchVersion(struct adapter * adapt, u16 btHciVer, u16 btPatchVer)
{
	EXhalbtcoutsrc_SetBtPatchVersion(btHciVer, btPatchVer);
}

void hal_btcoex_SetHciVersion(struct adapter * adapt, u16 hciVersion)
{
	EXhalbtcoutsrc_SetHciVersion(hciVersion);
}

void hal_btcoex_StackUpdateProfileInfo(void)
{
	EXhalbtcoutsrc_StackUpdateProfileInfo();
}

void hal_btcoex_pta_off_on_notify(struct adapter * adapt, u8 bBTON)
{
	ex_halbtcoutsrc_pta_off_on_notify(&GLBtCoexist, bBTON);
}

/*
 *	Description:
 *	Setting BT coex antenna isolation type .
 *	coex mechanisn/ spital stream/ best throughput
 *	anttype = 0	,	PSTDMA	/	2SS	/	0.5T	,	bad isolation , WiFi/BT ANT Distance<15cm , (<20dB) for 2,3 antenna
 *	anttype = 1	,	PSTDMA	/	1SS	/	0.5T	,	normal isolaiton , 50cm>WiFi/BT ANT Distance>15cm , (>20dB) for 2 antenna
 *	anttype = 2	,	TDMA	/	2SS	/	T ,		normal isolaiton , 50cm>WiFi/BT ANT Distance>15cm , (>20dB) for 3 antenna
 *	anttype = 3	,	no TDMA	/	1SS	/	0.5T	,	good isolation , WiFi/BT ANT Distance >50cm , (>40dB) for 2 antenna
 *	anttype = 4	,	no TDMA	/	2SS	/	T ,		good isolation , WiFi/BT ANT Distance >50cm , (>40dB) for 3 antenna
 *	wifi only throughput ~ T
 *	wifi/BT share one antenna with SPDT
 */
void hal_btcoex_SetAntIsolationType(struct adapter * adapt, u8 anttype)
{
	struct hal_com_data * pHalData;
	struct btc_coexist *	pBtCoexist = &GLBtCoexist;

	/*RTW_INFO("####%s , anttype = %d  , %d\n" , __func__ , anttype , __LINE__); */
	pHalData = GET_HAL_DATA(adapt);


	pHalData->bt_coexist.btAntisolation = anttype;

	switch (pHalData->bt_coexist.btAntisolation) {
	case 0:
		pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_0;
		break;
	case 1:
		pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_1;
		break;
	case 2:
		pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_2;
		break;
	case 3:
		pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_3;
		break;
	case 4:
		pBtCoexist->board_info.ant_type = (u8)BTC_ANT_TYPE_4;
		break;
	}

}

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
int
hal_btcoex_ParseAntIsolationConfigFile(
	struct adapter *		Adapter,
	char			*buffer
)
{
	u32	i = 0 , j = 0;
	char	*szLine , *ptmp;
	int rtStatus = _SUCCESS;
	char param_value_string[10];
	u8 anttype = 4;
	u8 ant_num = 3 , ant_distance = 50 , rfe_type = 1;
	struct ant_isolation {
		char *param_name;  /* antenna isolation config parameter name */
		u8 *value; /* antenna isolation config parameter value */
	};
	struct ant_isolation ant_isolation_param[] = {
		{"ANT_NUMBER" , &ant_num},
		{"ANT_DISTANCE" , &ant_distance},
		{"RFE_TYPE" , &rfe_type},
		{NULL , NULL}
	};

	ptmp = buffer;
	for (szLine = GetLineFromBuffer(ptmp) ; szLine; szLine = GetLineFromBuffer(ptmp)) {
		/* skip comment */
		if (IsCommentString(szLine))
			continue;

		/* RTW_INFO("%s : szLine = %s , strlen(szLine) = %d\n" , __func__ , szLine , strlen(szLine));*/
		for (j = 0 ; ant_isolation_param[j].param_name ; j++) {
			if (strstr(szLine , ant_isolation_param[j].param_name)) {
				i = 0;
				while (i < strlen(szLine)) {
					if (szLine[i] != '"')
						++i;
					else {
						/* skip only has one " */
						if (strpbrk(szLine , "\"") == strrchr(szLine , '"')) {
							RTW_INFO("Fail to parse parameters , format error!\n");
							break;
						}
						memset((void *)param_value_string , 0 , 10);
						if (!ParseQualifiedString(szLine , &i , param_value_string , '"' , '"')) {
							RTW_INFO("Fail to parse parameters\n");
							return _FAIL;
						} else if (!GetU1ByteIntegerFromStringInDecimal(param_value_string , ant_isolation_param[j].value))
							RTW_INFO("Fail to GetU1ByteIntegerFromStringInDecimal\n");

						break;
					}
				}
			}
		}
	}

	/* YiWei 20140716 , for BT coex antenna isolation control */
	/* rfe_type = 0 was SPDT , rfe_type = 1 was coupler */
	if (ant_num == 3 && ant_distance >= 50)
		anttype = 3;
	else if (ant_num == 2 && ant_distance >= 50 && rfe_type == 1)
		anttype = 2;
	else if (ant_num == 3 && ant_distance >= 15 && ant_distance < 50)
		anttype = 2;
	else if (ant_num == 2 && ant_distance >= 15 && ant_distance < 50 && rfe_type == 1)
		anttype = 2;
	else if ((ant_num == 2 && ant_distance < 15 && rfe_type == 1) || (ant_num == 3 && ant_distance < 15))
		anttype = 1;
	else if (ant_num == 2 && rfe_type == 0)
		anttype = 0;
	else
		anttype = 0;

	hal_btcoex_SetAntIsolationType(Adapter, anttype);

	RTW_INFO("%s : ant_num = %d\n" , __func__ , ant_num);
	RTW_INFO("%s : ant_distance = %d\n" , __func__ , ant_distance);
	RTW_INFO("%s : rfe_type = %d\n" , __func__ , rfe_type);
	/* RTW_INFO("<===Hal_ParseAntIsolationConfigFile()\n"); */
	return rtStatus;
}


int
hal_btcoex_AntIsolationConfig_ParaFile(
	struct adapter *	Adapter,
	char		*pFileName
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0 , rtStatus = _FAIL;

	memset(pHalData->para_file_buf , 0 , MAX_PARA_FILE_BUF_LEN);

	rtw_get_phy_file_path(Adapter, pFileName);
	if (rtw_is_file_readable(rtw_phy_para_file_path)) {
		rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
		if (rlen > 0)
			rtStatus = _SUCCESS;
	}


	if (rtStatus == _SUCCESS) {
		/*RTW_INFO("%s(): read %s ok\n", __func__ , pFileName);*/
		rtStatus = hal_btcoex_ParseAntIsolationConfigFile(Adapter , pHalData->para_file_buf);
	} else
		RTW_INFO("%s(): No File %s, Load from *** Array!\n" , __func__ , pFileName);

	return rtStatus;
}
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */

u16 hal_btcoex_btreg_read(struct adapter * adapt, u8 type, u16 addr, u32 *data)
{
	u16 ret = 0;

	halbtcoutsrc_LeaveLowPower(&GLBtCoexist);

	ret = halbtcoutsrc_GetBtReg_with_status(&GLBtCoexist, type, addr, data);

	halbtcoutsrc_NormalLowPower(&GLBtCoexist);

	return ret;
}

u16 hal_btcoex_btreg_write(struct adapter * adapt, u8 type, u16 addr, u16 val)
{
	u16 ret = 0;

	halbtcoutsrc_LeaveLowPower(&GLBtCoexist);

	ret = halbtcoutsrc_SetBtReg(&GLBtCoexist, type, addr, val);

	halbtcoutsrc_NormalLowPower(&GLBtCoexist);

	return ret;
}

void hal_btcoex_set_rfe_type(u8 type)
{
	EXhalbtcoutsrc_set_rfe_type(type);
}

void hal_btcoex_switchband_notify(u8 under_scan, u8 band_type)
{
	switch (band_type) {
	case BAND_ON_2_4G:
		if (under_scan)
			EXhalbtcoutsrc_switchband_notify(&GLBtCoexist, BTC_SWITCH_TO_24G);
		else
			EXhalbtcoutsrc_switchband_notify(&GLBtCoexist, BTC_SWITCH_TO_24G_NOFORSCAN);
		break;
	case BAND_ON_5G:
		EXhalbtcoutsrc_switchband_notify(&GLBtCoexist, BTC_SWITCH_TO_5G);
		break;
	default:
		RTW_INFO("[BTCOEX] unkown switch band type\n");
		break;
	}
}

void hal_btcoex_WlFwDbgInfoNotify(struct adapter * adapt, u8* tmpBuf, u8 length)
{
	EXhalbtcoutsrc_WlFwDbgInfoNotify(&GLBtCoexist, tmpBuf, length);
}

void hal_btcoex_rx_rate_change_notify(struct adapter * adapt, bool is_data_frame, u8 rate_id)
{
	EXhalbtcoutsrc_rx_rate_change_notify(&GLBtCoexist, is_data_frame, EXhalbtcoutsrc_rate_id_to_btc_rate_id(rate_id));
}
