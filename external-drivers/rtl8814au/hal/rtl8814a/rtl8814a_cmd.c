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
#define _RTL8814A_CMD_C_

/* #include <drv_types.h> */
#include <rtl8814a_hal.h>

#define CONFIG_H2C_EF

#define RTL8814_MAX_H2C_BOX_NUMS	4
#define RTL8814_MAX_CMD_LEN	7
#define RTL8814_MESSAGE_BOX_SIZE		4
#define RTL8814_EX_MESSAGE_BOX_SIZE	4


static u8 _is_fw_read_cmd_down(_adapter *padapter, u8 msgbox_num)
{
	u8	read_down = _FALSE;
	int	retry_cnts = 100;

	u8 valid;

	/* RTW_INFO(" _is_fw_read_cmd_down ,reg_1cc(%x),msg_box(%d)...\n",rtw_read8(padapter,REG_HMETFR),msgbox_num); */

	do {
		valid = rtw_read8(padapter, REG_HMETFR) & BIT(msgbox_num);
		if (0 == valid)
			read_down = _TRUE;
		else
			rtw_msleep_os(1);
	} while ((!read_down) && (retry_cnts--));

	return read_down;

}


/*****************************************
* H2C Msg format :
* 0x1DF - 0x1D0
*| 31 - 8	| 7-5	 4 - 0	|
*| h2c_msg	|Class_ID CMD_ID	|
*
* Extend 0x1FF - 0x1F0
*|31 - 0	  |
*|ext_msg|
******************************************/
s32 FillH2CCmd_8814(PADAPTER padapter, u8 ElementID, u32 CmdLen, u8 *pCmdBuffer)
{
	u8 h2c_box_num;
	u32	msgbox_addr;
	u32 msgbox_ex_addr = 0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 cmd_idx ;
	u32	h2c_cmd = 0;
	u32	h2c_cmd_ex = 0;
	s32 ret = _FAIL;


	padapter = GET_PRIMARY_ADAPTER(padapter);
	pHalData = GET_HAL_DATA(padapter);


	if (pHalData->bFWReady == _FALSE) {
		/* RTW_INFO("FillH2CCmd_8814(): return H2C cmd because fw is not ready\n"); */
		return ret;
	}

	_enter_critical_mutex(&(adapter_to_dvobj(padapter)->h2c_fwcmd_mutex), NULL);


	if (!pCmdBuffer)
		goto exit;

	if (CmdLen > RTL8814_MAX_CMD_LEN) {
		RTW_WARN("%s, CmdLen: %d > %d, return!\n", __func__, CmdLen, RTL8814_MAX_CMD_LEN);
		goto exit;
	}

	if (rtw_is_surprise_removed(padapter))
		goto exit;

	/* pay attention to if  race condition happened in  H2C cmd setting. */
	do {
		h2c_box_num = pHalData->LastHMEBoxNum;

		if (!_is_fw_read_cmd_down(padapter, h2c_box_num)) {
			RTW_INFO(" fw read cmd failed...\n");
			goto exit;
		}

		*(u8 *)(&h2c_cmd) = ElementID;

		if (CmdLen <= 3) {
			_rtw_memcpy((u8 *)(&h2c_cmd) + 1, pCmdBuffer, CmdLen);
			h2c_cmd_ex = 0;
		} else {
			_rtw_memcpy((u8 *)(&h2c_cmd) + 1, pCmdBuffer, 3);
			_rtw_memcpy((u8 *)(&h2c_cmd_ex), pCmdBuffer + 3, CmdLen - 3);
		}

		/* Write Ext command */
		msgbox_ex_addr = REG_HMEBOX_EXT0_8814A + (h2c_box_num * RTL8814_EX_MESSAGE_BOX_SIZE);
#ifdef CONFIG_H2C_EF
		for (cmd_idx = 0; cmd_idx < RTL8814_MESSAGE_BOX_SIZE; cmd_idx++)
			rtw_write8(padapter, msgbox_ex_addr + cmd_idx, *((u8 *)(&h2c_cmd_ex) + cmd_idx));
#else
		h2c_cmd_ex = le32_to_cpu(h2c_cmd_ex);
		rtw_write32(padapter, msgbox_ex_addr, h2c_cmd_ex);
#endif

		/* Write command */
		msgbox_addr = REG_HMEBOX_0 + (h2c_box_num * RTL8814_MESSAGE_BOX_SIZE);
#ifdef CONFIG_H2C_EF
		for (cmd_idx = 0; cmd_idx < RTL8814_MESSAGE_BOX_SIZE; cmd_idx++)
			rtw_write8(padapter, msgbox_addr + cmd_idx, *((u8 *)(&h2c_cmd) + cmd_idx));
#else
		h2c_cmd = le32_to_cpu(h2c_cmd);
		rtw_write32(padapter, msgbox_addr, h2c_cmd);
#endif

		/* RTW_INFO("MSG_BOX:%d,CmdLen(%d), reg:0x%x =>h2c_cmd:0x%x, reg:0x%x =>h2c_cmd_ex:0x%x ..\n" */
		/* ,pHalData->LastHMEBoxNum ,CmdLen,msgbox_addr,h2c_cmd,msgbox_ex_addr,h2c_cmd_ex); */

		pHalData->LastHMEBoxNum = (h2c_box_num + 1) % RTL8814_MAX_H2C_BOX_NUMS;

	} while (0);

	ret = _SUCCESS;

exit:

	_exit_critical_mutex(&(adapter_to_dvobj(padapter)->h2c_fwcmd_mutex), NULL);


	return ret;
}
#ifdef CONFIG_FW_CORRECT_BCN
void rtl8814_fw_update_beacon_cmd(_adapter *padapter)
{
	u8	param[2] = {0};
	u16	txpktbuf_bndy;



	SET_8814A_H2CCMD_BCNHWSEQ_EN(param, 1);
	SET_8814A_H2CCMD_BCNHWSEQ_BCN_NUMBER(param, 0);
	SET_8814A_H2CCMD_BCNHWSEQ_HWSEQ(param, 1);
	SET_8814A_H2CCMD_BCNHWSEQ_EXHWSEQ(param, 0);
	SET_8814A_H2CCMD_BCNHWSEQ_PAGE(param, 0);
	if (GET_HAL_DATA(padapter)->firmware_version < 23)
		/* FW v21, v22 use H2C_BCNHWSEQ = 0xC2 */
		FillH2CCmd_8814(padapter, 0xC2, 2, param);
	else
		FillH2CCmd_8814(padapter, H2C_BCNHWSEQ, 2, param);

	/*RTW_INFO("%s, %d, correct beacon HW sequence, FirmwareVersion=%d, H2C_BCNHWSEQ=%d\n", __func__, __LINE__, GET_HAL_DATA(padapter)->firmware_version, H2C_BCNHWSEQ);*/

}
#endif
u8	Get_VHT_ENI(
	u32		IOTAction,
	u32		WirelessMode,
	u32		ratr_bitmap
)
{
	u8	Ret = 0;

	if (WirelessMode == WIRELESS_11_24AC) {
		if (ratr_bitmap & 0xfff00000)	/* Mix , 2SS */
			Ret = 3;
		else 					/* Mix, 1SS */
			Ret = 2;
	} else if (WirelessMode == WIRELESS_11_5AC) {
		Ret = 1;					/* VHT */
	}

	return Ret << 4;
}

BOOLEAN
Get_RA_ShortGI_8814(
	PADAPTER			Adapter,
	struct sta_info		*psta,
	u8					shortGIrate,
	u32					ratr_bitmap
)
{
	BOOLEAN		bShortGI;
	struct mlme_ext_priv	*pmlmeext = &Adapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	bShortGI = shortGIrate;

#ifdef CONFIG_80211AC_VHT
	if (bShortGI &&
	    is_supported_vht(psta->wireless_mode) &&
	    (pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_REALTEK_JAGUAR_CCUTAP) &&
	    TEST_FLAG(psta->vhtpriv.ldpc_cap, LDPC_VHT_ENABLE_TX)
	   ) {
		if (ratr_bitmap & 0xC0000000)
			bShortGI = _FALSE;
	}
#endif /* CONFIG_80211AC_VHT */

	return bShortGI;
}


void
Set_RA_LDPC_8814(
	struct sta_info	*psta,
	BOOLEAN			bLDPC
)
{
	if (psta == NULL)
		return;
#ifdef CONFIG_80211AC_VHT
	if (psta->wireless_mode == WIRELESS_11_5AC) {
		if (bLDPC && TEST_FLAG(psta->vhtpriv.ldpc_cap, LDPC_VHT_CAP_TX))
			SET_FLAG(psta->vhtpriv.ldpc_cap, LDPC_VHT_ENABLE_TX);
		else
			CLEAR_FLAG(psta->vhtpriv.ldpc_cap, LDPC_VHT_ENABLE_TX);
	} else if (is_supported_ht(psta->wireless_mode) || is_supported_vht(psta->wireless_mode)) {
		if (bLDPC && TEST_FLAG(psta->htpriv.ldpc_cap, LDPC_HT_CAP_TX))
			SET_FLAG(psta->htpriv.ldpc_cap, LDPC_HT_ENABLE_TX);
		else
			CLEAR_FLAG(psta->htpriv.ldpc_cap, LDPC_HT_ENABLE_TX);
	}

	update_ldpc_stbc_cap(psta);
#endif /* CONFIG_80211AC_VHT */

	/* RTW_INFO("MacId %d bLDPC %d\n", psta->cmn.mac_id, bLDPC); */
}

void rtl8814_req_txrpt_cmd(PADAPTER padapter, u8 macid)
{
	u8 u1H2CApReqRptParm[H2C_AP_REQ_TXRPT_LEN] = {0};

	SET_8814A_H2CCMD_TXREP_PARM_STA1(u1H2CApReqRptParm, macid);
	SET_8814A_H2CCMD_TXREP_PARM_STA2(u1H2CApReqRptParm, 0xff);
	SET_8814A_H2CCMD_TXREP_PARM_RTY(u1H2CApReqRptParm, 0x00);
	FillH2CCmd_8814(padapter, H2C_AP_REQ_TXRPT, H2C_AP_REQ_TXRPT_LEN, u1H2CApReqRptParm);
	SET_8814A_H2CCMD_TXREP_PARM_RTY(u1H2CApReqRptParm, 0x02);
	FillH2CCmd_8814(padapter, H2C_AP_REQ_TXRPT, H2C_AP_REQ_TXRPT_LEN, u1H2CApReqRptParm);
}

void rtl8814_set_FwPwrMode_cmd(PADAPTER padapter, u8 PSMode)
{
	u8	u1H2CSetPwrMode[H2C_PWRMODE_LEN] = {0};
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
	u8	Mode = 0, RLBM = 0, PowerState = 0, LPSAwakeIntvl = 2;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 allQueueUAPSD = 0;

	RTW_INFO("%s: Mode=%d SmartPS=%d\n", __FUNCTION__,	PSMode, pwrpriv->smart_ps);

	switch (PSMode) {
	case PS_MODE_ACTIVE:
		Mode = 0;
		break;
	case PS_MODE_MIN:
		Mode = 1;
		break;
	case PS_MODE_MAX:
		RLBM = 1;
		Mode = 1;
		break;
	case PS_MODE_DTIM:
		RLBM = 2;
		Mode = 1;
		break;
	case PS_MODE_UAPSD_WMM:
		Mode = 2;
		break;
	default:
		Mode = 0;
		break;
	}

	if (Mode > PS_MODE_ACTIVE) {
#ifdef CONFIG_BT_COEXIST
		if ((rtw_btcoex_IsBtControlLps(padapter) == _TRUE)  && (_TRUE == pHalData->EEPROMBluetoothCoexist))
			PowerState = rtw_btcoex_RpwmVal(padapter);
		else
#endif /* CONFIG_BT_COEXIST */
			PowerState = 0x00;/* AllON(0x0C), RFON(0x04), RFOFF(0x00) */

#ifdef CONFIG_EXT_CLK
		Mode |= BIT(7);/* supporting 26M XTAL CLK_Request feature. */
#endif /* CONFIG_EXT_CLK */
	} else
		PowerState = 0x0C;/* AllON(0x0C), RFON(0x04), RFOFF(0x00) */

	/* 0: Active, 1: LPS, 2: WMMPS */
	SET_8814A_H2CCMD_PWRMODE_PARM_MODE(u1H2CSetPwrMode, Mode);

	/* 0:Min, 1:Max , 2:User define */
	SET_8814A_H2CCMD_PWRMODE_PARM_RLBM(u1H2CSetPwrMode, RLBM);

	/* (LPS) smart_ps:  0: PS_Poll, 1: PS_Poll , 2: NullData */
	/* (WMM)smart_ps: 0:PS_Poll, 1:NullData */
	SET_8814A_H2CCMD_PWRMODE_PARM_SMART_PS(u1H2CSetPwrMode, pwrpriv->smart_ps);

	/* AwakeInterval: Unit is beacon interval, this field is only valid in PS_DTIM mode */
	SET_8814A_H2CCMD_PWRMODE_PARM_BCN_PASS_TIME(u1H2CSetPwrMode, LPSAwakeIntvl);

	/* (WMM only)bAllQueueUAPSD */
	SET_8814A_H2CCMD_PWRMODE_PARM_ALL_QUEUE_UAPSD(u1H2CSetPwrMode, allQueueUAPSD);

	/* AllON(0x0C), RFON(0x04), RFOFF(0x00) */
	SET_8814A_H2CCMD_PWRMODE_PARM_PWR_STATE(u1H2CSetPwrMode, PowerState);

#ifdef CONFIG_BT_COEXIST
	if (_TRUE == pHalData->EEPROMBluetoothCoexist)
		rtw_btcoex_RecordPwrMode(padapter, u1H2CSetPwrMode, sizeof(u1H2CSetPwrMode));
#endif /* CONFIG_BT_COEXIST */
	/* RTW_INFO("u1H2CSetPwrMode="MAC_FMT"\n", MAC_ARG(u1H2CSetPwrMode)); */
	FillH2CCmd_8814(padapter, H2C_SET_PWR_MODE, sizeof(u1H2CSetPwrMode), u1H2CSetPwrMode);

}

#ifdef CONFIG_TDLS
#ifdef CONFIG_TDLS_CH_SW
void rtl8814_set_BcnEarly_C2H_Rpt_cmd(PADAPTER padapter, u8 enable)
{
	u8	u1H2CSetPwrMode[H2C_PWRMODE_LEN] = {0};

	SET_8814A_H2CCMD_PWRMODE_PARM_MODE(u1H2CSetPwrMode, 1);
	SET_8814A_H2CCMD_PWRMODE_PARM_RLBM(u1H2CSetPwrMode, 1);
	SET_8814A_H2CCMD_PWRMODE_PARM_SMART_PS(u1H2CSetPwrMode, 0);
	SET_8814A_H2CCMD_PWRMODE_PARM_BCN_PASS_TIME(u1H2CSetPwrMode, 0);
	SET_8814A_H2CCMD_PWRMODE_PARM_ALL_QUEUE_UAPSD(u1H2CSetPwrMode, 0);
	SET_8814A_H2CCMD_PWRMODE_PARM_BCN_EARLY_C2H_RPT(u1H2CSetPwrMode, enable);
	SET_8814A_H2CCMD_PWRMODE_PARM_PWR_STATE(u1H2CSetPwrMode, 0x0C);
	FillH2CCmd_8814(padapter, H2C_SET_PWR_MODE, sizeof(u1H2CSetPwrMode), u1H2CSetPwrMode);
}
#endif
#endif

void rtl8814a_set_FwPwrModeInIPS_cmd(PADAPTER padapter, u8 cmd_param)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
	u8 parm[H2C_INACTIVE_PS_LEN] = {0};

	/* u8 cmd_param; */ /* BIT0:enable, BIT1:NoConnect32k */
	if (cmd_param) {

#ifdef CONFIG_BT_COEXIST
		rtw_btcoex_IpsNotify(padapter, pwrpriv->ips_mode_req);
#endif
		/* Enter IPS */
		RTW_INFO("%s: issue H2C to FW when entering IPS\n", __func__);

		parm[0] = 0x1;/* suggest by Isaac.Hsu*/
		
		rtw_hal_fill_h2c_cmd(padapter, /* H2C_FWLPS_IN_IPS_, */
				     H2C_INACTIVE_PS_,
				     H2C_INACTIVE_PS_LEN, parm);
	} else {
		/* Leave IPS */
		RTW_INFO("%s: Leaving IPS in FWLPS state\n", __func__);

		parm[0] = 0x0;
		parm[1] = 0x0;
		parm[2] = 0x0;
		rtw_hal_fill_h2c_cmd(padapter, H2C_INACTIVE_PS_,
				     H2C_INACTIVE_PS_LEN, parm);
#ifdef CONFIG_BT_COEXIST
		rtw_btcoex_IpsNotify(padapter, IPS_NONE);
#endif
	}
}

/*
 * Description: Get the reserved page number in Tx packet buffer.
 * Retrun value: the page number.
 * 2012.08.09, by tynli.
 *   */
u8
GetTxBufferRsvdPageNum8814(_adapter *Adapter, bool bWoWLANBoundary)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8	RsvdPageNum = 0;
	u16	TxPageBndy = LAST_ENTRY_OF_TX_PKT_BUFFER_8814A; /* default reseved 1 page for the IC type which is undefined. */

	if (bWoWLANBoundary)
		rtw_hal_get_def_var(Adapter, HAL_DEF_TX_PAGE_BOUNDARY_WOWLAN, (u8 *)&TxPageBndy);
	else
		rtw_hal_get_def_var(Adapter, HAL_DEF_TX_PAGE_BOUNDARY, (u8 *)&TxPageBndy);

	RsvdPageNum = LAST_ENTRY_OF_TX_PKT_BUFFER_8814A - TxPageBndy + 1;

	return RsvdPageNum;
}

void rtl8814_download_rsvd_page(PADAPTER padapter, u8 mstatus)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	BOOLEAN		bSendBeacon = _FALSE;
	BOOLEAN		bcn_valid = _FALSE;
	u8	DLBcnCount = 0;
	u32 poll = 0;
	u8 RegFwHwTxQCtrl;

	RTW_INFO("%s mstatus(%x)\n", __FUNCTION__, mstatus);

	if (mstatus == 1) {
		u8 bcn_ctrl = rtw_read8(padapter, REG_BCN_CTRL);

		/* We should set AID, correct TSF, HW seq enable before set JoinBssReport to Fw in 88/92C. */
		/* Suggested by filen. Added by tynli. */
		rtw_write16(padapter, REG_BCN_PSR_RPT, (0xC000 | pmlmeinfo->aid));
		/* Hw sequende enable by dedault. 2010.06.23. by tynli. */
		/* rtw_write16(padapter, REG_NQOS_SEQ, ((pmlmeext->mgnt_seq+100)&0xFFF)); */
		/* rtw_write8(padapter, REG_HWSEQ_CTRL, 0xFF); */

		/* Set REG_CR bit 8. DMA beacon by SW. */
		rtw_write8(padapter,  REG_CR + 1,
			rtw_read8(padapter,  REG_CR + 1) | BIT0);

		/*RTW_INFO("%s-%d: enable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(padapter, REG_CR));*/

		/* Disable Hw protection for a time which revserd for Hw sending beacon. */
		/* Fix download reserved page packet fail that access collision with the protection time. */
		/* 2010.05.11. Added by tynli. */
		rtw_write8(padapter, REG_BCN_CTRL, (bcn_ctrl & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);

		RegFwHwTxQCtrl = rtw_read8(padapter, REG_FWHW_TXQ_CTRL + 2);
		if (RegFwHwTxQCtrl & BIT6) {
			RTW_INFO("HalDownloadRSVDPage(): There is an Adapter is sending beacon.\n");
			bSendBeacon = _TRUE;
		}

		/* Set FWHW_TXQ_CTRL 0x422[6]=0 to tell Hw the packet is not a real beacon frame. */
		RegFwHwTxQCtrl &= (~BIT6);
		rtw_write8(padapter, REG_FWHW_TXQ_CTRL + 2, RegFwHwTxQCtrl);

		/* Clear beacon valid check bit. */
		rtw_hal_set_hwreg(padapter, HW_VAR_BCN_VALID, NULL);
		DLBcnCount = 0;
		poll = 0;
		do {
			/* download rsvd page. */
			rtw_hal_set_fw_rsvd_page(padapter, _FALSE);
			DLBcnCount++;
			do {
				rtw_yield_os();
				/* rtw_mdelay_os(10); */
				/* check rsvd page download OK. */
				rtw_hal_get_hwreg(padapter, HW_VAR_BCN_VALID, (u8 *)(&bcn_valid));
				poll++;
			} while (!bcn_valid && (poll % 10) != 0 && !RTW_CANNOT_RUN(padapter));

		} while (!bcn_valid && DLBcnCount <= 100 && !RTW_CANNOT_RUN(padapter));

		/* RT_ASSERT(bcn_valid, ("HalDownloadRSVDPage88ES(): 1 Download RSVD page failed!\n")); */
		if (RTW_CANNOT_RUN(padapter))
			;
		else if (!bcn_valid)
			RTW_ERR(ADPT_FMT": 1 DL RSVD page failed! DLBcnCount:%u, poll:%u\n",
				 ADPT_ARG(padapter) , DLBcnCount, poll);
		else {
			struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);

			pwrctl->fw_psmode_iface_id = padapter->iface_id;
			rtw_hal_set_fw_rsvd_page(padapter, _TRUE);
			RTW_INFO(ADPT_FMT": 1 DL RSVD page success! DLBcnCount:%u, poll:%u\n",
				 ADPT_ARG(padapter), DLBcnCount, poll);
		}

		/* restore bcn_ctrl */
		rtw_write8(padapter, REG_BCN_CTRL, bcn_ctrl);

		/* To make sure that if there exists an adapter which would like to send beacon. */
		/* If exists, the origianl value of 0x422[6] will be 1, we should check this to */
		/* prevent from setting 0x422[6] to 0 after download reserved page, or it will cause */
		/* the beacon cannot be sent by HW. */
		/* 2010.06.23. Added by tynli. */
		if (bSendBeacon) {
			RegFwHwTxQCtrl |= BIT6;
			rtw_write8(padapter, REG_FWHW_TXQ_CTRL + 2, RegFwHwTxQCtrl);
		}

		/*  */
		/* Update RSVD page location H2C to Fw. */
		/*  */
		if (bcn_valid) {
			rtw_hal_set_hwreg(padapter, HW_VAR_BCN_VALID, NULL);
			RTW_INFO("Set RSVD page location to Fw.\n");
			/* FillH2CCmd88E(Adapter, H2C_88E_RSVDPAGE, H2C_RSVDPAGE_LOC_LENGTH, pMgntInfo->u1RsvdPageLoc); */
		}

		/* Do not enable HW DMA BCN or it will cause Pcie interface hang by timing issue. 2011.11.24. by tynli. */
		/* if(!padapter->bEnterPnpSleep) */
#ifndef CONFIG_PCI_HCI
		{
#ifndef RTL8814AE_SW_BCN
			/* Clear CR[8] or beacon packet will not be send to TxBuf anymore. */
			rtw_write8(padapter, REG_CR + 1,
				rtw_read8(padapter, REG_CR + 1) & (~BIT0));

			/*RTW_INFO("%s-%d: disable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(padapter, REG_CR));*/
#endif
		}
#endif /* !CONFIG_PCI_HCI */
	}
}

void rtl8814_set_FwJoinBssReport_cmd(PADAPTER padapter, u8 mstatus)
{
	if (mstatus == 1)
		rtl8814_download_rsvd_page(padapter, RT_MEDIA_CONNECT);
}

#ifdef CONFIG_P2P_PS
void rtl8814_set_p2p_ps_offload_cmd(_adapter *padapter, u8 p2p_ps_state)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(padapter);
	struct wifidirect_info	*pwdinfo = &(padapter->wdinfo);
	u8	*p2p_ps_offload = (u8 *)&pHalData->p2p_ps_offload;
	u8	i;


#if 1
	switch (p2p_ps_state) {
	case P2P_PS_DISABLE:
		RTW_INFO("P2P_PS_DISABLE\n");
		_rtw_memset(p2p_ps_offload, 0, 1);
		break;
	case P2P_PS_ENABLE:
		RTW_INFO("P2P_PS_ENABLE\n");
		/* update CTWindow value. */
		if (pwdinfo->ctwindow > 0) {
			SET_8814A_H2CCMD_P2P_PS_OFFLOAD_CTWINDOW_EN(p2p_ps_offload, 1);
			rtw_write8(padapter, REG_P2P_CTWIN, pwdinfo->ctwindow);
		}

		/* hw only support 2 set of NoA */
		for (i = 0 ; i < pwdinfo->noa_num ; i++) {
			/* To control the register setting for which NOA */
			rtw_write8(padapter, REG_NOA_DESC_SEL, (i << 4));
			if (i == 0)
				SET_8814A_H2CCMD_P2P_PS_OFFLOAD_NOA0_EN(p2p_ps_offload, 1);
			else
				SET_8814A_H2CCMD_P2P_PS_OFFLOAD_NOA1_EN(p2p_ps_offload, 1);

			/* config P2P NoA Descriptor Register */
			/* RTW_INFO("%s(): noa_duration = %x\n",__FUNCTION__,pwdinfo->noa_duration[i]); */
			rtw_write32(padapter, REG_NOA_DESC_DURATION, pwdinfo->noa_duration[i]);

			/* RTW_INFO("%s(): noa_interval = %x\n",__FUNCTION__,pwdinfo->noa_interval[i]); */
			rtw_write32(padapter, REG_NOA_DESC_INTERVAL, pwdinfo->noa_interval[i]);

			/* RTW_INFO("%s(): start_time = %x\n",__FUNCTION__,pwdinfo->noa_start_time[i]); */
			rtw_write32(padapter, REG_NOA_DESC_START, pwdinfo->noa_start_time[i]);

			/* RTW_INFO("%s(): noa_count = %x\n",__FUNCTION__,pwdinfo->noa_count[i]); */
			rtw_write8(padapter, REG_NOA_DESC_COUNT, pwdinfo->noa_count[i]);
		}

		if ((pwdinfo->opp_ps == 1) || (pwdinfo->noa_num > 0)) {
			/* rst p2p circuit: reg 0x5F0 */
			rtw_write8(padapter, REG_P2P_RST_8814A, BIT(0)); /* rst p2p 0 circuit NOA 0 */

			SET_8814A_H2CCMD_P2P_PS_OFFLOAD_ENABLE(p2p_ps_offload, 1);

			if (pwdinfo->role == P2P_ROLE_GO) {
				/* 1: Owner, 0: Client */
				SET_8814A_H2CCMD_P2P_PS_OFFLOAD_ROLE(p2p_ps_offload, 1);
				SET_8814A_H2CCMD_P2P_PS_OFFLOAD_ALLSTASLEEP(p2p_ps_offload, 0);
			} else {
				/* 1: Owner, 0: Client */
				SET_8814A_H2CCMD_P2P_PS_OFFLOAD_ROLE(p2p_ps_offload, 0);
			}

			SET_8814A_H2CCMD_P2P_PS_OFFLOAD_DISCOVERY(p2p_ps_offload, 0);
		}
		break;
	case P2P_PS_SCAN:
		RTW_INFO("P2P_PS_SCAN\n");
		SET_8814A_H2CCMD_P2P_PS_OFFLOAD_DISCOVERY(p2p_ps_offload, 1);
		break;
	case P2P_PS_SCAN_DONE:
		RTW_INFO("P2P_PS_SCAN_DONE\n");
		SET_8814A_H2CCMD_P2P_PS_OFFLOAD_DISCOVERY(p2p_ps_offload, 0);
		pwdinfo->p2p_ps_state = P2P_PS_ENABLE;
		break;
	default:
		break;
	}

	RTW_INFO("P2P_PS_OFFLOAD : %x\n", p2p_ps_offload[0]);
	FillH2CCmd_8814(padapter, H2C_P2P_PS_OFFLOAD, 1, p2p_ps_offload);
#endif


}
#endif /* CONFIG_P2P */



static void
C2HTxBeamformingHandler_8814(
		PADAPTER		Adapter,
		u8				*CmdBuf,
		u8				CmdLen
)
{
#ifdef CONFIG_BEAMFORMING /*PHYDM_BF - (BEAMFORMING_SUPPORT == 1)*/
	u8	status = CmdBuf[0] & BIT0;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*pDM_Odm = &pHalData->odmpriv;
	/*beamforming_check_sounding_success(Adapter, status);*/
	phydm_beamforming_end_sw(pDM_Odm, status);
#endif /*CONFIG_BEAMFORMING*/
}

static void
C2HTxFeedbackHandler_8814(
		PADAPTER	Adapter,
		u8			*CmdBuf,
		u8			CmdLen
)
{
#ifdef CONFIG_XMIT_ACK
	if (GET_8814A_C2H_TX_RPT_RETRY_OVER(CmdBuf) | GET_8814A_C2H_TX_RPT_LIFE_TIME_OVER(CmdBuf))
		rtw_ack_tx_done(&Adapter->xmitpriv, RTW_SCTX_DONE_CCX_PKT_FAIL);
	else
		rtw_ack_tx_done(&Adapter->xmitpriv, RTW_SCTX_DONE_SUCCESS);
#endif
}

static void
C2HTxRPTHandler_8814(
		PADAPTER	Adapter,
		u8			*CmdBuf,
		u8			CmdLen
)
{
	_irqL	 irqL;
	u8 macid = 0, IniRate = 0;
	u16 TxOK = 0, TxFail = 0;
	struct sta_priv	*pstapriv = &(GET_PRIMARY_ADAPTER(Adapter))->stapriv, *pstapriv_original = NULL;
	struct sta_info *psta = NULL;
	struct sta_info *pbcmc_stainfo = rtw_get_bcmc_stainfo(Adapter);
	_list	*plist, *phead;
	PADAPTER	adapter_ognl = NULL;

	if(!pstapriv->gotc2h) {
		RTW_WARN("%s,%d: No gotc2h!\n", __FUNCTION__, __LINE__);
		return;
	}
	
	adapter_ognl = rtw_get_iface_by_id(GET_PRIMARY_ADAPTER(Adapter), pstapriv->c2h_adapter_id);
	if(!adapter_ognl) {
		RTW_WARN("%s: No adapter!\n", __FUNCTION__);
		return;
	}

	psta = rtw_get_stainfo(&adapter_ognl->stapriv, pstapriv->c2h_sta_mac);
	if (!psta) {
		RTW_WARN("%s: No corresponding sta_info!\n", __FUNCTION__);
		return;
	}
	
	macid = GET_8814A_C2H_TC2H_APREQ_TXRPT_MACID1(CmdBuf);
	TxOK = GET_8814A_C2H_TC2H_APREQ_TXRPT_TXOK1(CmdBuf);
	TxFail = GET_8814A_C2H_TC2H_APREQ_TXRPT_TXFAIL1(CmdBuf);
	IniRate = GET_8814A_C2H_TC2H_APREQ_TXRPT_INIRATE1(CmdBuf);
	psta->sta_stats.tx_ok_cnt = TxOK;
	psta->sta_stats.tx_fail_cnt = TxFail;

}

static void
C2HSPC_STAT_8814(
		PADAPTER	Adapter,
		u8			*CmdBuf,
		u8			CmdLen
)
{
	_irqL	 irqL;
	struct sta_priv	*pstapriv = &(GET_PRIMARY_ADAPTER(Adapter))->stapriv;
	struct sta_info *psta = NULL;
	_list	*plist, *phead;
	PADAPTER	adapter_ognl = NULL;

	if(!pstapriv->gotc2h) {
		RTW_WARN("%s, %d: No gotc2h!\n", __FUNCTION__, __LINE__);
		return;
	}
	
	adapter_ognl = rtw_get_iface_by_id(GET_PRIMARY_ADAPTER(Adapter), pstapriv->c2h_adapter_id);
	if(!adapter_ognl) {
		RTW_WARN("%s: No adapter!\n", __FUNCTION__);
		return;
	}

	psta = rtw_get_stainfo(&adapter_ognl->stapriv, pstapriv->c2h_sta_mac);
	if (!psta) {
		RTW_WARN("%s: No corresponding sta_info!\n", __FUNCTION__);
		return;
	}
	psta->sta_stats.tx_retry_cnt = GET_8814A_C2H_SPC_STAT_TYPEB_RETRY1(CmdBuf);
	rtw_sctx_done(&pstapriv->gotc2h);
}

s32 c2h_handler_8814a(_adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	s32 ret = _SUCCESS;

	switch (id) {
	case C2H_TXBF:
		RTW_INFO("[C2H], C2H_TXBF!!\n");
		C2HTxBeamformingHandler_8814(adapter, payload, plen);
		break;
	case C2H_CCX_TX_RPT:
		C2HTxFeedbackHandler_8814(adapter, payload, plen);
		break;
	case C2H_AP_REQ_TXRPT:
		C2HTxRPTHandler_8814(adapter, payload, plen);
		break;
	case C2H_SPC_STAT:
		C2HSPC_STAT_8814(adapter, payload, plen);
		break;
	default:
		ret = _FAIL;
		break;
	}

	return ret;
}

#ifdef CONFIG_BT_COEXIST
#if 0
static void rtl8814_set_FwRsvdPage_cmd(PADAPTER padapter, PRSVDPAGE_LOC rsvdpageloc)
{
	u8 u1H2CRsvdPageParm[H2C_RSVDPAGE_LOC_LEN] = {0};

	RTW_INFO("8812au/8821/8811 RsvdPageLoc: ProbeRsp=%d PsPoll=%d Null=%d QoSNull=%d BTNull=%d\n",
		 rsvdpageloc->LocProbeRsp, rsvdpageloc->LocPsPoll,
		 rsvdpageloc->LocNullData, rsvdpageloc->LocQosNull,
		 rsvdpageloc->LocBTQosNull);

	SET_H2CCMD_RSVDPAGE_LOC_PROBE_RSP(u1H2CRsvdPageParm, rsvdpageloc->LocProbeRsp);
	SET_H2CCMD_RSVDPAGE_LOC_PSPOLL(u1H2CRsvdPageParm, rsvdpageloc->LocPsPoll);
	SET_H2CCMD_RSVDPAGE_LOC_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocNullData);
	SET_H2CCMD_RSVDPAGE_LOC_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocQosNull);
	SET_H2CCMD_RSVDPAGE_LOC_BT_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocBTQosNull);

	RTW_DBG_DUMP("u1H2CRsvdPageParm:", u1H2CRsvdPageParm, H2C_RSVDPAGE_LOC_LEN);
	FillH2CCmd_8814(padapter, H2C_RSVD_PAGE, H2C_RSVDPAGE_LOC_LEN, u1H2CRsvdPageParm);
}

#ifdef CONFIG_WOWLAN
static void rtl8814_set_FwAoacRsvdPage_cmd(PADAPTER padapter, PRSVDPAGE_LOC rsvdpageloc)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	u8	res = 0, count = 0;
#ifdef CONFIG_WOWLAN
	u8 u1H2CAoacRsvdPageParm[H2C_AOAC_RSVDPAGE_LOC_LEN] = {0};

	RTW_INFO("rtl8814_set_FwAoacRsvdPage_cmd: RWC=%d ArpRsp=%d NbrAdv=%d GtkRsp=%d GtkInfo=%d ProbeReq=%d NetworkList=%d\n",
		 rsvdpageloc->LocRemoteCtrlInfo, rsvdpageloc->LocArpRsp,
		 rsvdpageloc->LocNbrAdv, rsvdpageloc->LocGTKRsp,
		 rsvdpageloc->LocGTKInfo, rsvdpageloc->LocProbeReq,
		 rsvdpageloc->LocNetList);

#ifdef CONFIG_PNO_SUPPORT
	RTW_INFO("NLO_INFO=%d\n", rsvdpageloc->LocPNOInfo);
#endif
	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
		SET_H2CCMD_AOAC_RSVDPAGE_LOC_REMOTE_WAKE_CTRL_INFO(u1H2CAoacRsvdPageParm, rsvdpageloc->LocRemoteCtrlInfo);
		SET_H2CCMD_AOAC_RSVDPAGE_LOC_ARP_RSP(u1H2CAoacRsvdPageParm, rsvdpageloc->LocArpRsp);
		/* SET_H2CCMD_AOAC_RSVDPAGE_LOC_NEIGHBOR_ADV(u1H2CAoacRsvdPageParm, rsvdpageloc->LocNbrAdv); */
		SET_H2CCMD_AOAC_RSVDPAGE_LOC_GTK_RSP(u1H2CAoacRsvdPageParm, rsvdpageloc->LocGTKRsp);
		SET_H2CCMD_AOAC_RSVDPAGE_LOC_GTK_INFO(u1H2CAoacRsvdPageParm, rsvdpageloc->LocGTKInfo);
#ifdef CONFIG_GTK_OL
		SET_H2CCMD_AOAC_RSVDPAGE_LOC_GTK_EXT_MEM(u1H2CAoacRsvdPageParm, rsvdpageloc->LocGTKEXTMEM);
#endif /* CONFIG_GTK_OL */
	} else {
#ifdef CONFIG_PNO_SUPPORT
		if (!pwrpriv->wowlan_in_resume)
			SET_H2CCMD_AOAC_RSVDPAGE_LOC_NLO_INFO(u1H2CAoacRsvdPageParm, rsvdpageloc->LocPNOInfo);
#endif
	}

	RTW_DBG_DUMP("u1H2CAoacRsvdPageParm:", u1H2CAoacRsvdPageParm, H2C_AOAC_RSVDPAGE_LOC_LEN);
	FillH2CCmd_8814(padapter, H2C_AOAC_RSVD_PAGE, H2C_AOAC_RSVDPAGE_LOC_LEN, u1H2CAoacRsvdPageParm);

#ifdef CONFIG_PNO_SUPPORT
	if (!MLME_IS_AP(padapter) && !MLME_IS_MESH(padapter) &&
	    !check_fwstate(pmlmepriv, _FW_LINKED) &&
	    pwrpriv->wowlan_in_resume == _FALSE) {

		res = rtw_read8(padapter, 0x1b8);
		while (res == 0 && count < 25) {
			RTW_INFO("[%d] FW loc_NLOInfo: %d\n", count, res);
			res = rtw_read8(padapter, 0x1b8);
			count++;
			rtw_msleep_os(2);
		}
	}
#endif /* CONFIG_PNO_SUPPORT */
#endif /* CONFIG_WOWLAN */
}
#endif

static void SetFwRsvdPagePkt_BTCoex(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	struct xmit_frame	*pcmdframe;
	struct pkt_attrib	*pattrib;
	struct xmit_priv	*pxmitpriv;
	struct pwrctrl_priv *pwrctl;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	u32	BeaconLength = 0;
	u32	NullDataLength = 0, QosNullLength = 0, BTQosNullLength = 0;
	u32	ProbeReqLength = 0;
	u8	*ReservedPagePacket;
	u8	TxDescLen = TXDESC_SIZE, TxDescOffset = TXDESC_OFFSET;
	u8	TotalPageNum = 0, CurtPktPageNum = 0, RsvdPageNum = 0;
	u16	BufIndex, PageSize = PAGE_SIZE_TX_8814;
	u32	TotalPacketLen, MaxRsvdPageBufSize = 0;
	RSVDPAGE_LOC	RsvdPageLoc;

	pHalData = GET_HAL_DATA(padapter);

	pxmitpriv = &padapter->xmitpriv;
	pwrctl = adapter_to_pwrctl(padapter);

	/* RsvdPageNum = BCNQ_PAGE_NUM_8723B + WOWLAN_PAGE_NUM_8723B; */

	RsvdPageNum = BCNQ_PAGE_NUM_8814;
	MaxRsvdPageBufSize = RsvdPageNum * PageSize;

	pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);
	if (pcmdframe == NULL) {
		RTW_INFO("%s: alloc ReservedPagePacket fail!\n", __FUNCTION__);
		return;
	}

	ReservedPagePacket = pcmdframe->buf_addr;
	_rtw_memset(&RsvdPageLoc, 0, sizeof(RSVDPAGE_LOC));

	/* 3 (1) beacon */
	BufIndex = TxDescOffset;
	rtw_hal_construct_beacon(padapter,
		&ReservedPagePacket[BufIndex], &BeaconLength);

	/* When we count the first page size, we need to reserve description size for the RSVD */
	/* packet, it will be filled in front of the packet in TXPKTBUF. */
	CurtPktPageNum = (u8)PageNum(TxDescLen + BeaconLength, PageSize);

	/* If we don't add 1 more page, the WOWLAN function has a problem. Baron thinks it's a bug of firmware */
	if (CurtPktPageNum == 1)
		CurtPktPageNum += 1;
	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum * PageSize);

	/* Jump to lastest page */
	if (BufIndex < (MaxRsvdPageBufSize - PageSize)) {
		BufIndex = TxDescOffset + (MaxRsvdPageBufSize - PageSize);
		TotalPageNum = BCNQ_PAGE_NUM_8814 - 1;

	}

	/* 3 (6) BT Qos null data */
	RsvdPageLoc.LocBTQosNull = TotalPageNum;
	rtw_hal_construct_NullFunctionData(
		padapter,
		&ReservedPagePacket[BufIndex],
		&BTQosNullLength,
		_TRUE, 0, 0, _FALSE);
	rtl8814a_fill_fake_txdesc(padapter, &ReservedPagePacket[BufIndex - TxDescLen], BTQosNullLength, _FALSE, _TRUE,  _FALSE);

	/* RTW_INFO("%s(): HW_VAR_SET_TX_CMD: BT QOS NULL DATA %p %d\n",  */
	/*	__FUNCTION__, &ReservedPagePacket[BufIndex-TxDescLen], (BTQosNullLength+TxDescLen)); */

	CurtPktPageNum = (u8)PageNum(TxDescLen + BTQosNullLength, PageSize);

	TotalPageNum += CurtPktPageNum;

	TotalPacketLen = BufIndex + BTQosNullLength;
	if (TotalPacketLen > MaxRsvdPageBufSize) {
		RTW_INFO("%s(): ERROR: The rsvd page size is not enough!!TotalPacketLen %d, MaxRsvdPageBufSize %d\n", __FUNCTION__,
			 TotalPacketLen, MaxRsvdPageBufSize);
		goto error;
	} else {
		/* update attribute */
		pattrib = &pcmdframe->attrib;
		update_mgntframe_attrib(padapter, pattrib);
		pattrib->qsel = QSLT_BEACON;
		pattrib->pktlen = pattrib->last_txcmdsz = TotalPacketLen - TxDescOffset;
#ifdef CONFIG_PCI_HCI
		dump_mgntframe(padapter, pcmdframe);
#else
		dump_mgntframe_and_wait(padapter, pcmdframe, 100);
#endif
	}

	RTW_INFO("%s: Set RSVD page location to Fw ,TotalPacketLen(%d), TotalPageNum(%d)\n", __FUNCTION__, TotalPacketLen, TotalPageNum);
	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
		rtl8814_set_FwRsvdPage_cmd(padapter, &RsvdPageLoc);
#ifdef CONFIG_WOWLAN
		rtl8814_set_FwAoacRsvdPage_cmd(padapter, &RsvdPageLoc);
#endif
	}

	return;

error:

	rtw_free_xmitframe(pxmitpriv, pcmdframe);
}
#endif

void rtl8814a_download_BTCoex_AP_mode_rsvd_page(PADAPTER padapter)
{
	rtl8814_download_rsvd_page(padapter, RT_MEDIA_CONNECT);
}

#endif /* CONFIG_BT_COEXIST */
