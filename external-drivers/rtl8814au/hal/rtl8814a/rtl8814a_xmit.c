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
#define _RTL8814A_XMIT_C_

/* #include <drv_types.h> */
#include <rtl8814a_hal.h>

void _dbg_dump_tx_info(_adapter	*padapter, int frame_tag, u8 *ptxdesc)
{
	u8 bDumpTxPkt;
	u8 bDumpTxDesc = _FALSE;
	rtw_hal_get_def_var(padapter, HAL_DEF_DBG_DUMP_TXPKT, &(bDumpTxPkt));

	if (bDumpTxPkt == 1) { /* dump txdesc for data frame */
		RTW_INFO("dump tx_desc for data frame\n");
		if ((frame_tag & 0x0f) == DATA_FRAMETAG)
			bDumpTxDesc = _TRUE;
	} else if (bDumpTxPkt == 2) { /* dump txdesc for mgnt frame */
		RTW_INFO("dump tx_desc for mgnt frame\n");
		if ((frame_tag & 0x0f) == MGNT_FRAMETAG)
			bDumpTxDesc = _TRUE;
	} else if (bDumpTxPkt == 3) { /* dump early info */
	}

	if (bDumpTxDesc) {
		/* ptxdesc->txdw4 = cpu_to_le32(0x00001006); */ /* RTS Rate=24M */
		/*	ptxdesc->txdw6 = 0x6666f800; */
		RTW_INFO("=====================================\n");
		RTW_INFO("Offset00(0x%08x)\n", *((u32 *)(ptxdesc)));
		RTW_INFO("Offset04(0x%08x)\n", *((u32 *)(ptxdesc + 4)));
		RTW_INFO("Offset08(0x%08x)\n", *((u32 *)(ptxdesc + 8)));
		RTW_INFO("Offset12(0x%08x)\n", *((u32 *)(ptxdesc + 12)));
		RTW_INFO("Offset16(0x%08x)\n", *((u32 *)(ptxdesc + 16)));
		RTW_INFO("Offset20(0x%08x)\n", *((u32 *)(ptxdesc + 20)));
		RTW_INFO("Offset24(0x%08x)\n", *((u32 *)(ptxdesc + 24)));
		RTW_INFO("Offset28(0x%08x)\n", *((u32 *)(ptxdesc + 28)));
		RTW_INFO("Offset32(0x%08x)\n", *((u32 *)(ptxdesc + 32)));
		RTW_INFO("Offset36(0x%08x)\n", *((u32 *)(ptxdesc + 36)));
		RTW_INFO("=====================================\n");
	}

}

/*
 * Description:
 *	Aggregation packets and send to hardware
 *
 * Return:
 *	0	Success
 *	-1	Hardware resource(TX FIFO) not ready
 *	-2	Software resource(xmitbuf) not ready
 */
#ifdef CONFIG_TX_EARLY_MODE

/* #define DBG_EMINFO */

#if RTL8188E_EARLY_MODE_PKT_NUM_10 == 1
	#define EARLY_MODE_MAX_PKT_NUM	10
#else
	#define EARLY_MODE_MAX_PKT_NUM	5
#endif


struct EMInfo {
	u8	EMPktNum;
	u16  EMPktLen[EARLY_MODE_MAX_PKT_NUM];
};


void
InsertEMContent_8814(
	struct EMInfo *pEMInfo,
	u8 *VirtualAddress)
{

#if RTL8188E_EARLY_MODE_PKT_NUM_10 == 1
	u8 index = 0;
	u32	dwtmp = 0;
#endif

	_rtw_memset(VirtualAddress, 0, EARLY_MODE_INFO_SIZE);
	if (pEMInfo->EMPktNum == 0)
		return;

#ifdef DBG_EMINFO
	{
		int i;
		RTW_INFO("\n%s ==> pEMInfo->EMPktNum =%d\n", __FUNCTION__, pEMInfo->EMPktNum);
		for (i = 0; i < EARLY_MODE_MAX_PKT_NUM; i++)
			RTW_INFO("%s ==> pEMInfo->EMPktLen[%d] =%d\n", __FUNCTION__, i, pEMInfo->EMPktLen[i]);

	}
#endif

#if RTL8188E_EARLY_MODE_PKT_NUM_10 == 1
	SET_EARLYMODE_PKTNUM(VirtualAddress, pEMInfo->EMPktNum);

	if (pEMInfo->EMPktNum == 1)
		dwtmp = pEMInfo->EMPktLen[0];
	else {
		dwtmp = pEMInfo->EMPktLen[0];
		dwtmp += ((dwtmp % 4) ? (4 - dwtmp % 4) : 0) + 4;
		dwtmp += pEMInfo->EMPktLen[1];
	}
	SET_EARLYMODE_LEN0(VirtualAddress, dwtmp);
	if (pEMInfo->EMPktNum <= 3)
		dwtmp = pEMInfo->EMPktLen[2];
	else {
		dwtmp = pEMInfo->EMPktLen[2];
		dwtmp += ((dwtmp % 4) ? (4 - dwtmp % 4) : 0) + 4;
		dwtmp += pEMInfo->EMPktLen[3];
	}
	SET_EARLYMODE_LEN1(VirtualAddress, dwtmp);
	if (pEMInfo->EMPktNum <= 5)
		dwtmp = pEMInfo->EMPktLen[4];
	else {
		dwtmp = pEMInfo->EMPktLen[4];
		dwtmp += ((dwtmp % 4) ? (4 - dwtmp % 4) : 0) + 4;
		dwtmp += pEMInfo->EMPktLen[5];
	}
	SET_EARLYMODE_LEN2_1(VirtualAddress, dwtmp & 0xF);
	SET_EARLYMODE_LEN2_2(VirtualAddress, dwtmp >> 4);
	if (pEMInfo->EMPktNum <= 7)
		dwtmp = pEMInfo->EMPktLen[6];
	else {
		dwtmp = pEMInfo->EMPktLen[6];
		dwtmp += ((dwtmp % 4) ? (4 - dwtmp % 4) : 0) + 4;
		dwtmp += pEMInfo->EMPktLen[7];
	}
	SET_EARLYMODE_LEN3(VirtualAddress, dwtmp);
	if (pEMInfo->EMPktNum <= 9)
		dwtmp = pEMInfo->EMPktLen[8];
	else {
		dwtmp = pEMInfo->EMPktLen[8];
		dwtmp += ((dwtmp % 4) ? (4 - dwtmp % 4) : 0) + 4;
		dwtmp += pEMInfo->EMPktLen[9];
	}
	SET_EARLYMODE_LEN4(VirtualAddress, dwtmp);
#else
	SET_EARLYMODE_PKTNUM(VirtualAddress, pEMInfo->EMPktNum);
	SET_EARLYMODE_LEN0(VirtualAddress, pEMInfo->EMPktLen[0]);
	SET_EARLYMODE_LEN1(VirtualAddress, pEMInfo->EMPktLen[1]);
	SET_EARLYMODE_LEN2_1(VirtualAddress, pEMInfo->EMPktLen[2] & 0xF);
	SET_EARLYMODE_LEN2_2(VirtualAddress, pEMInfo->EMPktLen[2] >> 4);
	SET_EARLYMODE_LEN3(VirtualAddress, pEMInfo->EMPktLen[3]);
	SET_EARLYMODE_LEN4(VirtualAddress, pEMInfo->EMPktLen[4]);
#endif

}



void UpdateEarlyModeInfo8814(struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{
	/* _adapter *padapter, struct xmit_frame *pxmitframe,struct tx_servq	*ptxservq */
	int index, j;
	u16 offset, pktlen;
	PTXDESC_8814 ptxdesc;

	u8 *pmem, *pEMInfo_mem;
	s8 node_num_0 = 0, node_num_1 = 0;
	struct EMInfo eminfo;
	struct agg_pkt_info *paggpkt;
	struct xmit_frame *pframe = (struct xmit_frame *)pxmitbuf->priv_data;
	pmem = pframe->buf_addr;

#ifdef DBG_EMINFO
	RTW_INFO("\n%s ==> agg_num:%d\n", __FUNCTION__, pframe->agg_num);
	for (index = 0; index < pframe->agg_num; index++) {
		offset =	pxmitpriv->agg_pkt[index].offset;
		pktlen = pxmitpriv->agg_pkt[index].pkt_len;
		RTW_INFO("%s ==> agg_pkt[%d].offset=%d\n", __FUNCTION__, index, offset);
		RTW_INFO("%s ==> agg_pkt[%d].pkt_len=%d\n", __FUNCTION__, index, pktlen);
	}
#endif

	if (pframe->agg_num > EARLY_MODE_MAX_PKT_NUM) {
		node_num_0 = pframe->agg_num;
		node_num_1 = EARLY_MODE_MAX_PKT_NUM - 1;
	}

	for (index = 0; index < pframe->agg_num; index++) {

		offset = pxmitpriv->agg_pkt[index].offset;
		pktlen = pxmitpriv->agg_pkt[index].pkt_len;

		_rtw_memset(&eminfo, 0, sizeof(struct EMInfo));
		if (pframe->agg_num > EARLY_MODE_MAX_PKT_NUM) {
			if (node_num_0 > EARLY_MODE_MAX_PKT_NUM) {
				eminfo.EMPktNum = EARLY_MODE_MAX_PKT_NUM;
				node_num_0--;
			} else {
				eminfo.EMPktNum = node_num_1;
				node_num_1--;
			}
		} else
			eminfo.EMPktNum = pframe->agg_num - (index + 1);
		for (j = 0; j < eminfo.EMPktNum ; j++) {
			eminfo.EMPktLen[j] = pxmitpriv->agg_pkt[index + 1 + j].pkt_len + 4; /* 4 bytes CRC */
		}

		if (pmem) {
			if (index == 0) {
				ptxdesc = (PTXDESC_8814)(pmem);
				pEMInfo_mem = ((u8 *)ptxdesc) + TXDESC_SIZE;
			} else {
				pmem = pmem + pxmitpriv->agg_pkt[index - 1].offset;
				ptxdesc = (PTXDESC_8814)(pmem);
				pEMInfo_mem = ((u8 *)ptxdesc) + TXDESC_SIZE;
			}

#ifdef DBG_EMINFO
			RTW_INFO("%s ==> desc.pkt_len=%d\n", __FUNCTION__, ptxdesc->pktlen);
#endif
			InsertEMContent_8814(&eminfo, pEMInfo_mem);
		}


	}
	_rtw_memset(pxmitpriv->agg_pkt, 0, sizeof(struct agg_pkt_info) * MAX_AGG_PKT_NUM);

}
#endif

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
void rtl8814a_cal_txdesc_chksum(u8 *ptxdesc)
{
	u16	*usPtr;
	u32 count;
	u32 index;
	u16 checksum = 0;


	usPtr = (u16 *)ptxdesc;
	/* checksume is always calculated by first 32 bytes, */
	/* and it doesn't depend on TX DESC length. */
	/* Thomas,Lucas@SD4,20130515 */
	count = 16;

	/* Clear first */
	SET_TX_DESC_TX_DESC_CHECKSUM_8814A(ptxdesc, 0);

	for (index = 0 ; index < count ; index++)
		checksum = checksum ^ le16_to_cpu(*(usPtr + index));

	SET_TX_DESC_TX_DESC_CHECKSUM_8814A(ptxdesc, checksum);
}
#endif

/*
 * Description: In normal chip, we should send some packet to Hw which will be used by Fw
 *			in FW LPS mode. The function is to fill the Tx descriptor of this packets, then
 *			Fw can tell Hw to send these packet derectly.
 *   */
void rtl8814a_fill_fake_txdesc(
	PADAPTER	padapter,
	u8			*pDesc,
	u32			BufferLen,
	u8			IsPsPoll,
	u8			IsBTQosNull,
	u8			bDataFrame)
{
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;


	/* Clear all status */
	_rtw_memset(pDesc, 0, TXDESC_SIZE);

	SET_TX_DESC_LAST_SEG_8814A(pDesc, 1);

	SET_TX_DESC_OFFSET_8814A(pDesc, TXDESC_SIZE);

	SET_TX_DESC_PKT_SIZE_8814A(pDesc, BufferLen);

	SET_TX_DESC_QUEUE_SEL_8814A(pDesc,  QSLT_MGNT);

	if (pmlmeext->cur_wireless_mode & WIRELESS_11B)
		SET_TX_DESC_RATE_ID_8814A(pDesc, RATEID_IDX_B);
	else
		SET_TX_DESC_RATE_ID_8814A(pDesc, RATEID_IDX_G);

	/* Set NAVUSEHDR to prevent Ps-poll AId filed to be changed to error vlaue by Hw. */
	if (IsPsPoll)
		SET_TX_DESC_NAV_USE_HDR_8814A(pDesc, 1);
	else {
		SET_TX_DESC_HWSEQ_EN_8814A(pDesc, 1); /* Hw set sequence number */
	}
#if 0 /* todo */
	if (IsBTQosNull)
		SET_TX_DESC_BT_INT_8812(pDesc, 1);
#endif /* 0 */

	SET_TX_DESC_USE_RATE_8814A(pDesc, 1);

	/* 8814 no OWN bit? */
	/* SET_TX_DESC_OWN_8812(pDesc, 1); */

	/*  */
	/* Encrypt the data frame if under security mode excepct null data. Suggested by CCW. */
	/*  */
	if (_TRUE == bDataFrame) {
		u32 EncAlg;

		EncAlg = padapter->securitypriv.dot11PrivacyAlgrthm;
		switch (EncAlg) {
		case _NO_PRIVACY_:
			SET_TX_DESC_SEC_TYPE_8814A(pDesc, 0x0);
			break;
		case _WEP40_:
		case _WEP104_:
		case _TKIP_:
			SET_TX_DESC_SEC_TYPE_8814A(pDesc, 0x1);
			break;
		case _SMS4_:
			SET_TX_DESC_SEC_TYPE_8814A(pDesc, 0x2);
			break;
		case _AES_:
			SET_TX_DESC_SEC_TYPE_8814A(pDesc, 0x3);
			break;
		default:
			SET_TX_DESC_SEC_TYPE_8814A(pDesc, 0x0);
			break;
		}
	}
	SET_TX_DESC_TX_RATE_8814A(pDesc, MRateToHwRate(pmlmeext->tx_rate));

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	/* USB interface drop packet if the checksum of descriptor isn't correct. */
	/* Using this checksum can let hardware recovery from packet bulk out error (e.g. Cancel URC, Bulk out error.). */
	rtl8814a_cal_txdesc_chksum(pDesc);
#endif
}

#if defined(CONFIG_CONCURRENT_MODE)
void fill_txdesc_force_bmc_camid(struct pkt_attrib *pattrib, u8 *ptxdesc)
{
	if ((pattrib->encrypt > 0) && (!pattrib->bswenc)
	    && (pattrib->bmc_camid != INVALID_SEC_MAC_CAM_ID)) {

		SET_TX_DESC_EN_DESC_ID_8814A(ptxdesc, 1);
		SET_TX_DESC_MACID_8814A(ptxdesc, pattrib->bmc_camid);
	}
}
#endif
void fill_txdesc_bmc_tx_rate(struct pkt_attrib *pattrib, u8 *ptxdesc)
{
	SET_TX_DESC_USE_RATE_8814A(ptxdesc, 1);
	SET_TX_DESC_TX_RATE_8814A(ptxdesc, MRateToHwRate(pattrib->rate));
	SET_TX_DESC_DISABLE_FB_8814A(ptxdesc, 1);
}

void rtl8814a_fill_txdesc_sectype(struct pkt_attrib *pattrib, u8 *ptxdesc)
{
	if ((pattrib->encrypt > 0) && !pattrib->bswenc) {
		switch (pattrib->encrypt) {
		/* SEC_TYPE : 0:NO_ENC,1:WEP40/TKIP,2:WAPI,3:AES */
		case _WEP40_:
		case _WEP104_:
		case _TKIP_:
		case _TKIP_WTMIC_:
			SET_TX_DESC_SEC_TYPE_8814A(ptxdesc, 0x1);
			break;
#ifdef CONFIG_WAPI_SUPPORT
		case _SMS4_:
			SET_TX_DESC_SEC_TYPE_8814A(ptxdesc, 0x2);
			break;
#endif
		case _AES_:
			SET_TX_DESC_SEC_TYPE_8814A(ptxdesc, 0x3);
			break;
		case _NO_PRIVACY_:
		default:
			SET_TX_DESC_SEC_TYPE_8814A(ptxdesc, 0x0);
			break;

		}

	}

}

void rtl8814a_fill_txdesc_vcs(PADAPTER padapter, struct pkt_attrib *pattrib, u8 *ptxdesc)
{
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);

	/* RTW_INFO("vcs_mode=%d\n", pattrib->vcs_mode); */

	if (pattrib->vcs_mode) {

		switch (pattrib->vcs_mode) {
		case RTS_CTS:
			SET_TX_DESC_RTS_ENABLE_8814A(ptxdesc, 1);
			break;
		case CTS_TO_SELF:
			SET_TX_DESC_CTS2SELF_8814A(ptxdesc, 1);
			break;
		case NONE_VCS:
		default:
			break;
		}

		if (pmlmeinfo->preamble_mode == PREAMBLE_SHORT)
			SET_TX_DESC_RTS_SHORT_8814A(ptxdesc, 1);

		/* RTS Rate=24M */
		SET_TX_DESC_RTS_RATE_8814A(ptxdesc, 0x8);

		/* compatibility for MCC consideration, use pmlmeext->cur_channel */
		if (pmlmeext->cur_channel > 14)
			/* RTS retry to rate OFDM 6M for 5G */
			SET_TX_DESC_RTS_RATE_FB_LIMIT_8814A(ptxdesc, 4);
		else
			/* RTS retry to rate CCK 1M for 2.4G */
			SET_TX_DESC_RTS_RATE_FB_LIMIT_8814A(ptxdesc, 0);
	}
}


u8
BWMapping_8814(
		PADAPTER		Adapter,
		struct pkt_attrib	*pattrib
)
{
	u8	BWSettingOfDesc = 0;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);

	/* RTW_INFO("BWMapping pHalData->current_channel_bw %d, pattrib->bwmode %d\n",pHalData->current_channel_bw,pattrib->bwmode); */

	if (pHalData->current_channel_bw == CHANNEL_WIDTH_80) {
		if (pattrib->bwmode == CHANNEL_WIDTH_80)
			BWSettingOfDesc = 2;
		else if (pattrib->bwmode == CHANNEL_WIDTH_40)
			BWSettingOfDesc = 1;
		else
			BWSettingOfDesc = 0;
	} else if (pHalData->current_channel_bw == CHANNEL_WIDTH_40) {
		if ((pattrib->bwmode == CHANNEL_WIDTH_40) || (pattrib->bwmode == CHANNEL_WIDTH_80))
			BWSettingOfDesc = 1;
		else
			BWSettingOfDesc = 0;
	} else
		BWSettingOfDesc = 0;

	return BWSettingOfDesc;
}

u8
SCMapping_8814(
		PADAPTER		Adapter,
		struct pkt_attrib	*pattrib
)
{
	u8	SCSettingOfDesc = 0;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	/* RTW_INFO("SCMapping: pHalData->current_channel_bw %d, pHalData->nCur80MhzPrimeSC %d, pHalData->nCur40MhzPrimeSC %d\n",pHalData->current_channel_bw,pHalData->nCur80MhzPrimeSC,pHalData->nCur40MhzPrimeSC); */

	if (pHalData->current_channel_bw == CHANNEL_WIDTH_80) {
		if (pattrib->bwmode == CHANNEL_WIDTH_80)
			SCSettingOfDesc = VHT_DATA_SC_DONOT_CARE;
		else if (pattrib->bwmode == CHANNEL_WIDTH_40) {
			if (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
				SCSettingOfDesc = VHT_DATA_SC_40_LOWER_OF_80MHZ;
			else if (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
				SCSettingOfDesc = VHT_DATA_SC_40_UPPER_OF_80MHZ;
			else
				RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
		} else {
			if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER))
				SCSettingOfDesc = VHT_DATA_SC_20_LOWEST_OF_80MHZ;
			else if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER))
				SCSettingOfDesc = VHT_DATA_SC_20_LOWER_OF_80MHZ;
			else if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER))
				SCSettingOfDesc = VHT_DATA_SC_20_UPPER_OF_80MHZ;
			else if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER))
				SCSettingOfDesc = VHT_DATA_SC_20_UPPERST_OF_80MHZ;
			else
				RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
		}
	} else if (pHalData->current_channel_bw == CHANNEL_WIDTH_40) {
		/* RTW_INFO("SCMapping: HT Case: pHalData->current_channel_bw %d, pHalData->nCur40MhzPrimeSC %d\n",pHalData->current_channel_bw,pHalData->nCur40MhzPrimeSC); */

		if (pattrib->bwmode == CHANNEL_WIDTH_40)
			SCSettingOfDesc = VHT_DATA_SC_DONOT_CARE;
		else if (pattrib->bwmode == CHANNEL_WIDTH_20) {
			if (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
				SCSettingOfDesc = VHT_DATA_SC_20_UPPER_OF_80MHZ;
			else if (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
				SCSettingOfDesc = VHT_DATA_SC_20_LOWER_OF_80MHZ;
			else
				SCSettingOfDesc = VHT_DATA_SC_DONOT_CARE;

		}
	} else
		SCSettingOfDesc = VHT_DATA_SC_DONOT_CARE;

	return SCSettingOfDesc;
}


void rtl8814a_fill_txdesc_phy(PADAPTER padapter, struct pkt_attrib *pattrib, u8 *ptxdesc)
{
	/* RTW_INFO("bwmode=%d, ch_off=%d\n", pattrib->bwmode, pattrib->ch_offset); */

	if (pattrib->ht_en) {
		/* Set Bandwidth and sub-channel settings. */
		SET_TX_DESC_DATA_BW_8814A(ptxdesc, BWMapping_8814(padapter, pattrib));

		SET_TX_DESC_DATA_SC_8814A(ptxdesc, SCMapping_8814(padapter, pattrib));
	}
}
