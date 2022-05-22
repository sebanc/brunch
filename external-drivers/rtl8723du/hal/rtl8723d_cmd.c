// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTL8723D_CMD_C_

#include <rtl8723d_hal.h>
#include "hal_com_h2c.h"

#define MAX_H2C_BOX_NUMS	4
#define MESSAGE_BOX_SIZE		4

#define RTL8723D_MAX_CMD_LEN	7
#define RTL8723D_EX_MESSAGE_BOX_SIZE	4

static u8 _is_fw_read_cmd_down(struct adapter *adapt, u8 msgbox_num)
{
	u8	read_down = false;
	int	retry_cnts = 100;

	u8 valid;

	/* RTW_INFO(" _is_fw_read_cmd_down ,reg_1cc(%x),msg_box(%d)...\n",rtw_read8(adapt,REG_HMETFR),msgbox_num); */

	do {
		valid = rtw_read8(adapt, REG_HMETFR) & BIT(msgbox_num);
		if (0 == valid)
			read_down = true;
		else
			rtw_msleep_os(1);
	} while ((!read_down) && (retry_cnts--));

	return read_down;

}


/*****************************************
* H2C Msg format :
*| 31 - 8		|7-5	| 4 - 0	|
*| h2c_msg	|Class	|CMD_ID	|
*| 31-0						|
*| Ext msg					|
*
******************************************/
int FillH2CCmd8723D(struct adapter * adapt, u8 ElementID, u32 CmdLen, u8 *pCmdBuffer)
{
	u8 h2c_box_num;
	u8 h2c[RTL8723D_MAX_CMD_LEN + 1] = {0};
	u32	msgbox_addr;
	u32 msgbox_ex_addr = 0;
	u32	h2c_cmd = 0;
	u32	h2c_cmd_ex = 0;
	int ret = _FAIL;
	struct hal_com_data * pHalData;		
	__le32 le_tmp;

	adapt = GET_PRIMARY_ADAPTER(adapt);
	pHalData = GET_HAL_DATA(adapt);
	_enter_critical_mutex(&(adapter_to_dvobj(adapt)->h2c_fwcmd_mutex), NULL);

	if (!pCmdBuffer)
		goto exit;
	if (CmdLen > RTL8723D_MAX_CMD_LEN)
		goto exit;
	if (rtw_is_surprise_removed(adapt))
		goto exit;

	h2c[0] = ElementID;
	memcpy(h2c + 1, pCmdBuffer, CmdLen);
	
	/* pay attention to if  race condition happened in  H2C cmd setting. */
	do {
		h2c_box_num = pHalData->LastHMEBoxNum;

		if (!_is_fw_read_cmd_down(adapt, h2c_box_num)) {
			RTW_INFO(" fw read cmd failed...\n");
			goto exit;
		}
		/* Write Ext command (byte 4~7) */
		msgbox_ex_addr = REG_HMEBOX_EXT0_8723D + (h2c_box_num * RTL8723D_EX_MESSAGE_BOX_SIZE);
		memcpy((u8 *)(&le_tmp), h2c + 4, RTL8723D_EX_MESSAGE_BOX_SIZE);
		h2c_cmd_ex = le32_to_cpu(le_tmp);
		rtw_write32(adapt, msgbox_ex_addr, h2c_cmd_ex);
		/* Write command (byte 0~3) */
		msgbox_addr = REG_HMEBOX_0_8723D + (h2c_box_num * MESSAGE_BOX_SIZE);
		memcpy((u8 *)(&le_tmp), h2c, 4);
		h2c_cmd = le32_to_cpu(le_tmp);
		rtw_write32(adapt, msgbox_addr, h2c_cmd);

		/* RTW_INFO("MSG_BOX:%d, CmdLen(%d), CmdID(0x%x), reg:0x%x =>h2c_cmd:0x%.8x, reg:0x%x =>h2c_cmd_ex:0x%.8x\n" */
		/*	,pHalData->LastHMEBoxNum , CmdLen, ElementID, msgbox_addr, h2c_cmd, msgbox_ex_addr, h2c_cmd_ex); */

		/* update last msg box number */
		pHalData->LastHMEBoxNum = (h2c_box_num + 1) % MAX_H2C_BOX_NUMS;

	} while (0);

	ret = _SUCCESS;

exit:

	_exit_critical_mutex(&(adapter_to_dvobj(adapt)->h2c_fwcmd_mutex), NULL);


	return ret;
}

/*
 * Description: Get the reserved page number in Tx packet buffer.
 * Retrun value: the page number.
 * 2012.08.09, by tynli.
 *   */
u8 GetTxBufferRsvdPageNum8723D(struct adapter *adapt, bool wowlan)
{
	u8	RsvdPageNum = 0;
	/* default reseved 1 page for the IC type which is undefined. */
	u8	TxPageBndy = LAST_ENTRY_OF_TX_PKT_BUFFER_8723D;

	rtw_hal_get_def_var(adapt, HAL_DEF_TX_PAGE_BOUNDARY, (u8 *)&TxPageBndy);

	RsvdPageNum = LAST_ENTRY_OF_TX_PKT_BUFFER_8723D - TxPageBndy + 1;

	return RsvdPageNum;
}

static void rtl8723d_set_FwRsvdPage_cmd(struct adapter * adapt, struct rsvd_page * rsvdpageloc)
{
	u8 u1H2CRsvdPageParm[H2C_RSVDPAGE_LOC_LEN] = {0};

	RTW_INFO("8723DRsvdPageLoc: ProbeRsp=%d PsPoll=%d Null=%d QoSNull=%d BTNull=%d\n",
		 rsvdpageloc->LocProbeRsp, rsvdpageloc->LocPsPoll,
		 rsvdpageloc->LocNullData, rsvdpageloc->LocQosNull,
		 rsvdpageloc->LocBTQosNull);

	SET_H2CCMD_RSVD_PAGE_PROBE_RSP(u1H2CRsvdPageParm, rsvdpageloc->LocProbeRsp);
	SET_H2CCMD_RSVD_PAGE_PSPOLL(u1H2CRsvdPageParm, rsvdpageloc->LocPsPoll);
	SET_H2CCMD_RSVD_PAGE_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocNullData);
	SET_H2CCMD_RSVD_PAGE_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocQosNull);
	SET_H2CCMD_RSVD_PAGE_BT_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocBTQosNull);

	RTW_DBG_DUMP("u1H2CRsvdPageParm:",
		     u1H2CRsvdPageParm, H2C_RSVDPAGE_LOC_LEN);

	FillH2CCmd8723D(adapt, H2C_8723D_RSVD_PAGE, H2C_RSVDPAGE_LOC_LEN, u1H2CRsvdPageParm);
}

static void rtl8723d_set_FwAoacRsvdPage_cmd(struct adapter * adapt, struct rsvd_page * rsvdpageloc)
{
}

void rtl8723d_set_FwPwrMode_cmd(struct adapter * adapt, u8 psmode)
{
	u8 smart_ps = 0;
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	u8 u1H2CPwrModeParm[H2C_PWRMODE_LEN] = {0};
	u8 PowerState = 0, awake_intvl = 1, byte5 = 0, rlbm = 0;
	struct wifidirect_info *wdinfo = &(adapt->wdinfo);
	u8 allQueueUAPSD = 0;

	if (pwrpriv->dtim > 0)
		RTW_INFO("%s(): FW LPS mode = %d, SmartPS=%d, dtim=%d\n", __func__, psmode, pwrpriv->smart_ps, pwrpriv->dtim);
	else
		RTW_INFO("%s(): FW LPS mode = %d, SmartPS=%d\n", __func__, psmode, pwrpriv->smart_ps);

	if (psmode == PS_MODE_MIN) {
		rlbm = 0;
		awake_intvl = 2;
		smart_ps = pwrpriv->smart_ps;
	} else if (psmode == PS_MODE_MAX) {
		rlbm = 1;
		awake_intvl = 2;
		smart_ps = pwrpriv->smart_ps;
	} else if (psmode == PS_MODE_DTIM) { /* For WOWLAN LPS, DTIM = (awake_intvl - 1) */
		if (pwrpriv->dtim > 0 && pwrpriv->dtim < 16)
			awake_intvl = pwrpriv->dtim + 1; /* DTIM = (awake_intvl - 1) */
		else
			awake_intvl = 4;/* DTIM=3 */


		rlbm = 2;
		smart_ps = pwrpriv->smart_ps;
	} else {
		rlbm = 2;
		awake_intvl = 4;
		smart_ps = pwrpriv->smart_ps;
	}

	if (!rtw_p2p_chk_state(wdinfo, P2P_STATE_NONE)) {
		awake_intvl = 2;
		rlbm = 1;
	}

	if (adapt->registrypriv.wifi_spec == 1) {
		awake_intvl = 2;
		rlbm = 1;
	}

	if (psmode > 0) {
		if (rtw_btcoex_IsBtControlLps(adapt)) {
			PowerState = rtw_btcoex_RpwmVal(adapt);
			byte5 = rtw_btcoex_LpsVal(adapt);

			if ((rlbm == 2) && (byte5 & BIT(4))) {
				/* Keep awake interval to 1 to prevent from */
				/* decreasing coex performance */
				awake_intvl = 2;
				rlbm = 2;
			}
		} else {
			PowerState = 0x00;/* AllON(0x0C), RFON(0x04), RFOFF(0x00) */
			byte5 = 0x40;
		}
	} else {
		PowerState = 0x0C;/* AllON(0x0C), RFON(0x04), RFOFF(0x00) */
		byte5 = 0x40;
	}

	SET_8723D_H2CCMD_PWRMODE_PARM_MODE(u1H2CPwrModeParm, (psmode > 0) ? 1 : 0);
	SET_8723D_H2CCMD_PWRMODE_PARM_SMART_PS(u1H2CPwrModeParm, smart_ps);
	SET_8723D_H2CCMD_PWRMODE_PARM_RLBM(u1H2CPwrModeParm, rlbm);
	SET_8723D_H2CCMD_PWRMODE_PARM_BCN_PASS_TIME(u1H2CPwrModeParm, awake_intvl);
	SET_8723D_H2CCMD_PWRMODE_PARM_ALL_QUEUE_UAPSD(u1H2CPwrModeParm, allQueueUAPSD);
	SET_8723D_H2CCMD_PWRMODE_PARM_PWR_STATE(u1H2CPwrModeParm, PowerState);
	SET_8723D_H2CCMD_PWRMODE_PARM_BYTE5(u1H2CPwrModeParm, byte5);
	rtw_btcoex_RecordPwrMode(adapt, u1H2CPwrModeParm, H2C_PWRMODE_LEN);

	RTW_DBG_DUMP("u1H2CPwrModeParm:",
		     u1H2CPwrModeParm, H2C_PWRMODE_LEN);

	FillH2CCmd8723D(adapt, H2C_8723D_SET_PWR_MODE, H2C_PWRMODE_LEN, u1H2CPwrModeParm);
}

void rtl8723d_set_FwPsTuneParam_cmd(struct adapter * adapt)
{
	u8 u1H2CPsTuneParm[H2C_PSTUNEPARAM_LEN] = {0};
	u8 bcn_to_limit = 10; /* 10 * 100 * awakeinterval (ms) */
	u8 dtim_timeout = 5; /* ms //wait broadcast data timer */
	u8 ps_timeout = 20;  /* ms //Keep awake when tx */
	u8 dtim_period = 3;

	/* RTW_INFO("%s(): FW LPS mode = %d\n", __func__, psmode); */

	SET_8723D_H2CCMD_PSTUNE_PARM_BCN_TO_LIMIT(u1H2CPsTuneParm, bcn_to_limit);
	SET_8723D_H2CCMD_PSTUNE_PARM_DTIM_TIMEOUT(u1H2CPsTuneParm, dtim_timeout);
	SET_8723D_H2CCMD_PSTUNE_PARM_PS_TIMEOUT(u1H2CPsTuneParm, ps_timeout);
	SET_8723D_H2CCMD_PSTUNE_PARM_ADOPT(u1H2CPsTuneParm, 1);
	SET_8723D_H2CCMD_PSTUNE_PARM_DTIM_PERIOD(u1H2CPsTuneParm, dtim_period);

	RTW_DBG_DUMP("u1H2CPsTuneParm:",
		     u1H2CPsTuneParm, H2C_PSTUNEPARAM_LEN);

	FillH2CCmd8723D(adapt, H2C_8723D_PS_TUNING_PARA, H2C_PSTUNEPARAM_LEN, u1H2CPsTuneParm);
}

void rtl8723d_download_rsvd_page(struct adapter * adapt, u8 mstatus)
{
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	bool		bcn_valid = false;
	u8	DLBcnCount = 0;
	u32 poll = 0;
	u8 val8, RegFwHwTxQCtrl;


	RTW_INFO("+" FUNC_ADPT_FMT ": hw_port=%d mstatus(%x)\n",
		 FUNC_ADPT_ARG(adapt), get_hw_port(adapt), mstatus);

	if (mstatus == RT_MEDIA_CONNECT) {
		bool bRecover = false;
		u8 v8;

		/* We should set AID, correct TSF, HW seq enable before set JoinBssReport to Fw in 88/92C. */
		/* Suggested by filen. Added by tynli. */
		rtw_write16(adapt, REG_BCN_PSR_RPT, (0xC000 | pmlmeinfo->aid));

		/* set REG_CR bit 8 */
		v8 = rtw_read8(adapt, REG_CR + 1);
		v8 |= BIT(0); /* ENSWBCN */
		rtw_write8(adapt,  REG_CR + 1, v8);

		/* Disable Hw protection for a time which revserd for Hw sending beacon. */
		/* Fix download reserved page packet fail that access collision with the protection time. */
		/* 2010.05.11. Added by tynli. */
		val8 = rtw_read8(adapt, REG_BCN_CTRL);
		val8 &= ~EN_BCN_FUNCTION;
		val8 |= DIS_TSF_UDT;
		rtw_write8(adapt, REG_BCN_CTRL, val8);

		/* Set FWHW_TXQ_CTRL 0x422[6]=0 to tell Hw the packet is not a real beacon frame. */
		RegFwHwTxQCtrl = rtw_read8(adapt, REG_FWHW_TXQ_CTRL + 2);
		if (RegFwHwTxQCtrl & BIT(6))
			bRecover = true;

		/* To tell Hw the packet is not a real beacon frame. */
		RegFwHwTxQCtrl &= ~BIT(6);
		rtw_write8(adapt, REG_FWHW_TXQ_CTRL + 2, RegFwHwTxQCtrl);

		/* Clear beacon valid check bit. */
		rtw_hal_set_hwreg(adapt, HW_VAR_BCN_VALID, NULL);
		rtw_hal_set_hwreg(adapt, HW_VAR_DL_BCN_SEL, NULL);

		DLBcnCount = 0;
		poll = 0;
		do {
			/* download rsvd page. */
			rtw_hal_set_fw_rsvd_page(adapt, 0);
			DLBcnCount++;
			do {
				rtw_yield_os();
				/* rtw_mdelay_os(10); */
				/* check rsvd page download OK. */
				rtw_hal_get_hwreg(adapt, HW_VAR_BCN_VALID, (u8 *)(&bcn_valid));
				poll++;
			} while (!bcn_valid && (poll % 10) != 0 && !RTW_CANNOT_RUN(adapt));

		} while (!bcn_valid && DLBcnCount <= 100 && !RTW_CANNOT_RUN(adapt));

		if (RTW_CANNOT_RUN(adapt))
			;
		else if (!bcn_valid)
			RTW_INFO(ADPT_FMT": 1 DL RSVD page failed! DLBcnCount:%u, poll:%u\n",
				 ADPT_ARG(adapt) , DLBcnCount, poll);
		else {
			struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);

			pwrctl->fw_psmode_iface_id = adapt->iface_id;
			RTW_INFO(ADPT_FMT": 1 DL RSVD page success! DLBcnCount:%u, poll:%u\n",
				 ADPT_ARG(adapt), DLBcnCount, poll);
		}

		/* 2010.05.11. Added by tynli. */
		val8 = rtw_read8(adapt, REG_BCN_CTRL);
		val8 |= EN_BCN_FUNCTION;
		val8 &= ~DIS_TSF_UDT;
		rtw_write8(adapt, REG_BCN_CTRL, val8);

		/* To make sure that if there exists an adapter which would like to send beacon. */
		/* If exists, the origianl value of 0x422[6] will be 1, we should check this to */
		/* prevent from setting 0x422[6] to 0 after download reserved page, or it will cause */
		/* the beacon cannot be sent by HW. */
		/* 2010.06.23. Added by tynli. */
		if (bRecover) {
			RegFwHwTxQCtrl |= BIT(6);
			rtw_write8(adapt, REG_FWHW_TXQ_CTRL + 2, RegFwHwTxQCtrl);
		}

		/* Clear CR[8] or beacon packet will not be send to TxBuf anymore. */
		v8 = rtw_read8(adapt, REG_CR + 1);
		v8 &= ~BIT(0); /* ~ENSWBCN */
		rtw_write8(adapt, REG_CR + 1, v8);
	}
}

void rtl8723d_set_FwJoinBssRpt_cmd(struct adapter *adapt, u8 mstatus)
{
	if (mstatus == 1)
		rtl8723d_download_rsvd_page(adapt, RT_MEDIA_CONNECT);
}

static void SetFwRsvdPagePkt_BTCoex(struct adapter * adapt)
{
	struct hal_com_data * pHalData;
	struct xmit_frame *pcmdframe;
	struct pkt_attrib *pattrib;
	struct xmit_priv *pxmitpriv;
	struct mlme_ext_priv *pmlmeext;
	struct mlme_ext_info *pmlmeinfo;
	u32	BeaconLength = 0;
	u32	BTQosNullLength = 0;
	u8 *ReservedPagePacket;
	u8 TxDescLen, TxDescOffset;
	u8 TotalPageNum = 0, CurtPktPageNum = 0, RsvdPageNum = 0;
	u16	BufIndex, PageSize;
	u32	TotalPacketLen, MaxRsvdPageBufSize = 0;
	struct rsvd_page RsvdPageLoc;

	pHalData = GET_HAL_DATA(adapt);
	pxmitpriv = &adapt->xmitpriv;
	pmlmeext = &adapt->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;
	TxDescLen = TXDESC_SIZE;
	TxDescOffset = TXDESC_OFFSET;
	PageSize = PAGE___kernel_size_tX_8723D;

	RsvdPageNum = BCNQ_PAGE_NUM_8723D;
	MaxRsvdPageBufSize = RsvdPageNum * PageSize;

	pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);
	if (!pcmdframe) {
		RTW_INFO("%s: alloc ReservedPagePacket fail!\n", __func__);
		return;
	}

	ReservedPagePacket = pcmdframe->buf_addr;
	memset(&RsvdPageLoc, 0, sizeof(struct rsvd_page));

	/* 3 (1) beacon */
	BufIndex = TxDescOffset;
	rtw_hal_construct_beacon(adapt,
		&ReservedPagePacket[BufIndex], &BeaconLength);

	/* When we count the first page size, we need to reserve description size for the RSVD */
	/* packet, it will be filled in front of the packet in TXPKTBUF. */
	CurtPktPageNum = (u8)PageNum_128(TxDescLen + BeaconLength);
	/* If we don't add 1 more page, the WOWLAN function has a problem. Baron thinks it's a bug of firmware */
	if (CurtPktPageNum == 1)
		CurtPktPageNum += 1;
	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum * PageSize);

	/* Jump to lastest page */
	if (BufIndex < (MaxRsvdPageBufSize - PageSize)) {
		BufIndex = TxDescOffset + (MaxRsvdPageBufSize - PageSize);
		TotalPageNum = BCNQ_PAGE_NUM_8723D - 1;
	}

	/* 3 (6) BT Qos null data */
	RsvdPageLoc.LocBTQosNull = TotalPageNum;
	rtw_hal_construct_NullFunctionData(
		adapt,
		&ReservedPagePacket[BufIndex],
		&BTQosNullLength,
		get_my_bssid(&pmlmeinfo->network),
		true, 0, 0, false);
	rtl8723d_fill_fake_txdesc(adapt, &ReservedPagePacket[BufIndex - TxDescLen], BTQosNullLength, false, true, false);

	CurtPktPageNum = (u8)PageNum_128(TxDescLen + BTQosNullLength);

	TotalPageNum += CurtPktPageNum;

	TotalPacketLen = BufIndex + BTQosNullLength;
	if (TotalPacketLen > MaxRsvdPageBufSize) {
		RTW_INFO(FUNC_ADPT_FMT ": ERROR: The rsvd page size is not enough!!TotalPacketLen %d, MaxRsvdPageBufSize %d\n",
			FUNC_ADPT_ARG(adapt), TotalPacketLen, MaxRsvdPageBufSize);
		goto error;
	}

	/* update attribute */
	pattrib = &pcmdframe->attrib;
	update_mgntframe_attrib(adapt, pattrib);
	pattrib->qsel = QSLT_BEACON;
	pattrib->pktlen = pattrib->last_txcmdsz = TotalPacketLen - TxDescOffset;
	dump_mgntframe_and_wait(adapt, pcmdframe, 100);

	/*	RTW_INFO(FUNC_ADPT_FMT ": Set RSVD page location to Fw, TotalPacketLen(%d), TotalPageNum(%d)\n",
	 *		FUNC_ADPT_ARG(adapt), TotalPacketLen, TotalPageNum); */
	rtl8723d_set_FwRsvdPage_cmd(adapt, &RsvdPageLoc);
	rtl8723d_set_FwAoacRsvdPage_cmd(adapt, &RsvdPageLoc);

	return;

error:
	rtw_free_xmitframe(pxmitpriv, pcmdframe);
}

void rtl8723d_download_BTCoex_AP_mode_rsvd_page(struct adapter * adapt)
{
	struct hal_com_data * pHalData;
	struct mlme_ext_priv *pmlmeext;
	struct mlme_ext_info *pmlmeinfo;
	u8 bRecover = false;
	u8 bcn_valid = false;
	u8 DLBcnCount = 0;
	u32 poll = 0;
	u8 val8, RegFwHwTxQCtrl;


	RTW_INFO("+" FUNC_ADPT_FMT ": hw_port=%d fw_state=0x%08X\n",
		FUNC_ADPT_ARG(adapt), get_hw_port(adapt), get_fwstate(&adapt->mlmepriv));

#ifdef CONFIG_RTW_DEBUG
	if (!check_fwstate(&adapt->mlmepriv, WIFI_AP_STATE)) {
		RTW_INFO(FUNC_ADPT_FMT ": [WARNING] not in AP mode!!\n",
			 FUNC_ADPT_ARG(adapt));
	}
#endif /* CONFIG_RTW_DEBUG */

	pHalData = GET_HAL_DATA(adapt);
	pmlmeext = &adapt->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;

	/* We should set AID, correct TSF, HW seq enable before set JoinBssReport to Fw in 88/92C. */
	/* Suggested by filen. Added by tynli. */
	rtw_write16(adapt, REG_BCN_PSR_RPT, (0xC000 | pmlmeinfo->aid));

	/* set REG_CR bit 8 */
	val8 = rtw_read8(adapt, REG_CR + 1);
	val8 |= BIT(0); /* ENSWBCN */
	rtw_write8(adapt,  REG_CR + 1, val8);

	/* Disable Hw protection for a time which revserd for Hw sending beacon. */
	/* Fix download reserved page packet fail that access collision with the protection time. */
	/* 2010.05.11. Added by tynli. */
	val8 = rtw_read8(adapt, REG_BCN_CTRL);
	val8 &= ~EN_BCN_FUNCTION;
	val8 |= DIS_TSF_UDT;
	rtw_write8(adapt, REG_BCN_CTRL, val8);

	/* Set FWHW_TXQ_CTRL 0x422[6]=0 to tell Hw the packet is not a real beacon frame. */
	RegFwHwTxQCtrl = rtw_read8(adapt, REG_FWHW_TXQ_CTRL + 2);
	if (RegFwHwTxQCtrl & BIT(6))
		bRecover = true;

	/* To tell Hw the packet is not a real beacon frame. */
	RegFwHwTxQCtrl &= ~BIT(6);
	rtw_write8(adapt, REG_FWHW_TXQ_CTRL + 2, RegFwHwTxQCtrl);

	/* Clear beacon valid check bit. */
	rtw_hal_set_hwreg(adapt, HW_VAR_BCN_VALID, NULL);
	rtw_hal_set_hwreg(adapt, HW_VAR_DL_BCN_SEL, NULL);

	DLBcnCount = 0;
	poll = 0;
	do {
		SetFwRsvdPagePkt_BTCoex(adapt);
		DLBcnCount++;
		do {
			rtw_yield_os();
			/*			rtw_mdelay_os(10); */
			/* check rsvd page download OK. */
			rtw_hal_get_hwreg(adapt, HW_VAR_BCN_VALID, &bcn_valid);
			poll++;
		} while (!bcn_valid && (poll % 10) != 0 && !RTW_CANNOT_RUN(adapt));
	} while (!bcn_valid && (DLBcnCount <= 100) && !RTW_CANNOT_RUN(adapt));

	if (bcn_valid) {
		struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);

		pwrctl->fw_psmode_iface_id = adapt->iface_id;
		RTW_INFO(ADPT_FMT": DL RSVD page success! DLBcnCount:%d, poll:%d\n",
			 ADPT_ARG(adapt), DLBcnCount, poll);
	} else {
		RTW_INFO(ADPT_FMT": DL RSVD page fail! DLBcnCount:%d, poll:%d\n",
			 ADPT_ARG(adapt), DLBcnCount, poll);
		RTW_INFO(ADPT_FMT": DL RSVD page fail! bSurpriseRemoved=%s\n",
			ADPT_ARG(adapt), rtw_is_surprise_removed(adapt) ? "True" : "False");
		RTW_INFO(ADPT_FMT": DL RSVD page fail! bDriverStopped=%s\n",
			ADPT_ARG(adapt), rtw_is_drv_stopped(adapt) ? "True" : "False");
	}

	/* 2010.05.11. Added by tynli. */
	val8 = rtw_read8(adapt, REG_BCN_CTRL);
	val8 |= EN_BCN_FUNCTION;
	val8 &= ~DIS_TSF_UDT;
	rtw_write8(adapt, REG_BCN_CTRL, val8);

	/* To make sure that if there exists an adapter which would like to send beacon. */
	/* If exists, the origianl value of 0x422[6] will be 1, we should check this to */
	/* prevent from setting 0x422[6] to 0 after download reserved page, or it will cause */
	/* the beacon cannot be sent by HW. */
	/* 2010.06.23. Added by tynli. */
	if (bRecover) {
		RegFwHwTxQCtrl |= BIT(6);
		rtw_write8(adapt, REG_FWHW_TXQ_CTRL + 2, RegFwHwTxQCtrl);
	}

	/* Clear CR[8] or beacon packet will not be send to TxBuf anymore. */
	val8 = rtw_read8(adapt, REG_CR + 1);
	val8 &= ~BIT(0); /* ~ENSWBCN */
	rtw_write8(adapt, REG_CR + 1, val8);
}

void rtl8723d_set_p2p_ps_offload_cmd(struct adapter *adapt, u8 p2p_ps_state)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);
	struct P2P_PS_Offload_t	*p2p_ps_offload = (struct P2P_PS_Offload_t *)(&pHalData->p2p_ps_offload);
	u8	i;

	switch (p2p_ps_state) {
	case P2P_PS_DISABLE:
		RTW_INFO("P2P_PS_DISABLE\n");
		memset(p2p_ps_offload, 0 , 1);
		break;
	case P2P_PS_ENABLE:
		RTW_INFO("P2P_PS_ENABLE\n");
		/* update CTWindow value. */
		if (pwdinfo->ctwindow > 0) {
			p2p_ps_offload->CTWindow_En = 1;
			rtw_write8(adapt, REG_P2P_CTWIN, pwdinfo->ctwindow);
		}

		/* hw only support 2 set of NoA */
		for (i = 0 ; i < pwdinfo->noa_num ; i++) {
			/* To control the register setting for which NOA */
			rtw_write8(adapt, REG_NOA_DESC_SEL, (i << 4));
			if (i == 0)
				p2p_ps_offload->NoA0_En = 1;
			else
				p2p_ps_offload->NoA1_En = 1;

			/* config P2P NoA Descriptor Register */
			rtw_write32(adapt, REG_NOA_DESC_DURATION, pwdinfo->noa_duration[i]);

			rtw_write32(adapt, REG_NOA_DESC_INTERVAL, pwdinfo->noa_interval[i]);

			rtw_write32(adapt, REG_NOA_DESC_START, pwdinfo->noa_start_time[i]);

			rtw_write8(adapt, REG_NOA_DESC_COUNT, pwdinfo->noa_count[i]);
		}

		if ((pwdinfo->opp_ps == 1) || (pwdinfo->noa_num > 0)) {
			/* rst p2p circuit */
			rtw_write8(adapt, REG_DUAL_TSF_RST, BIT(4));

			p2p_ps_offload->Offload_En = 1;

			if (pwdinfo->role == P2P_ROLE_GO) {
				p2p_ps_offload->role = 1;
				p2p_ps_offload->AllStaSleep = 0;
			} else
				p2p_ps_offload->role = 0;

			p2p_ps_offload->discovery = 0;
		}
		break;
	case P2P_PS_SCAN:
		RTW_INFO("P2P_PS_SCAN\n");
		p2p_ps_offload->discovery = 1;
		break;
	case P2P_PS_SCAN_DONE:
		RTW_INFO("P2P_PS_SCAN_DONE\n");
		p2p_ps_offload->discovery = 0;
		pwdinfo->p2p_ps_state = P2P_PS_ENABLE;
		break;
	default:
		break;
	}
	FillH2CCmd8723D(adapt, H2C_8723D_P2P_PS_OFFLOAD, 1, (u8 *)p2p_ps_offload);
}
