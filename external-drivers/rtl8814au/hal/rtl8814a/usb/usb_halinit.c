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
#define _HCI_HAL_INIT_C_

/* #include <drv_types.h> */
#include <rtl8814a_hal.h>

#ifndef CONFIG_USB_HCI

	#error "CONFIG_USB_HCI shall be on!\n"

#endif

static void
_ConfigChipOutEP_8814(
		PADAPTER	pAdapter,
		u8		NumOutPipe
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(pAdapter);


	pHalData->OutEpQueueSel = 0;
	pHalData->OutEpNumber = 0;

	switch (NumOutPipe) {
	case	4:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 4;
		break;
	case	3:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 3;
		break;
	case	2:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 2;
		break;
	case	1:
		pHalData->OutEpQueueSel = TX_SELE_HQ;
		pHalData->OutEpNumber = 1;
		break;
	default:
		break;

	}
	RTW_INFO("%s OutEpQueueSel(0x%02x), OutEpNumber(%d)\n", __FUNCTION__, pHalData->OutEpQueueSel, pHalData->OutEpNumber);

}

static BOOLEAN HalUsbSetQueuePipeMapping8814AUsb(
		PADAPTER	pAdapter,
		u8		NumInPipe,
		u8		NumOutPipe
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(pAdapter);
	BOOLEAN			result		= _FALSE;

	_ConfigChipOutEP_8814(pAdapter, NumOutPipe);

	/* Normal chip with one IN and one OUT doesn't have interrupt IN EP. */
	if (1 == pHalData->OutEpNumber) {
		if (1 != NumInPipe)
			return result;
	}

	/* All config other than above support one Bulk IN and one Interrupt IN. */
	/* if(2 != NumInPipe){ */
	/*	return result; */
	/* } */

	result = Hal_MappingOutPipe(pAdapter, NumOutPipe);

	return result;

}

void rtl8814au_interface_configure(_adapter *padapter)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(padapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	if (IS_SUPER_SPEED_USB(padapter)) {
		pHalData->UsbBulkOutSize = USB_SUPER_SPEED_BULK_SIZE;/* 1024 bytes */
	} else if (IS_HIGH_SPEED_USB(padapter)) {
		pHalData->UsbBulkOutSize = USB_HIGH_SPEED_BULK_SIZE;/* 512 bytes */
	} else {
		pHalData->UsbBulkOutSize = USB_FULL_SPEED_BULK_SIZE;/* 64 bytes */
	}

#ifdef CONFIG_USB_TX_AGGREGATION
	pHalData->UsbTxAggMode		= 1;
	pHalData->UsbTxAggDescNum	= 3;	/* only 4 bits */
#endif /* CONFIG_USB_TX_AGGREGATION */

#ifdef CONFIG_USB_RX_AGGREGATION
	pHalData->rxagg_mode = RX_AGG_DMA;
	pHalData->rxagg_usb_size = 8; /* unit: 512b */
	pHalData->rxagg_usb_timeout = 0x6;
	pHalData->rxagg_dma_size = 16; /* uint: 128b, 0x0A = 10 = MAX_RX_DMA_BUFFER_SIZE/2/pHalData->UsbBulkOutSize */
	pHalData->rxagg_dma_timeout = 0x6; /* 6, absolute time = 34ms/(2^6) */
#endif /* CONFIG_USB_RX_AGGREGATION */

	HalUsbSetQueuePipeMapping8814AUsb(padapter,
			  pdvobjpriv->RtNumInPipes, pdvobjpriv->RtNumOutPipes);

}

static void
_InitBurstPktLen(PADAPTER Adapter)
{
	u8			u1bTmp;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	/* yx_qi 131128 move to 0x1448, 144c */
	rtw_write32(Adapter, REG_FAST_EDCA_VOVI_SETTING_8814A, 0x08070807); /* yx_qi 131128 */
	rtw_write32(Adapter, REG_FAST_EDCA_BEBK_SETTING_8814A, 0x08070807); /* yx_qi 131128 */

	u1bTmp = rtw_read8(Adapter, 0xff); /* check device operation speed: SS 0xff bit7 */

	if (u1bTmp & BIT7)  /* USB2/1.1 Mode */
		pHalData->bSupportUSB3 = FALSE;
	else  /* USB3 Mode */
		pHalData->bSupportUSB3 = TRUE;

	if (pHalData->bSupportUSB3 == _FALSE) { /* USB2/1.1 Mode */
		if (pHalData->UsbBulkOutSize == 512) {
			/* set burst pkt len=512B */
			rtw_write8(Adapter, REG_RXDMA_MODE_8814A, 0x1e);
		} else {
			/* set burst pkt len=64B */
			rtw_write8(Adapter, REG_RXDMA_MODE_8814A, 0x2e);
		}

		rtw_write16(Adapter, REG_RXDMA_AGG_PG_TH_8814A, 0x2005); /* dmc agg th 20K */
	} else { /* USB3 Mode */
		/* set burst pkt len=1k */
		rtw_write8(Adapter, REG_RXDMA_MODE_8814A, 0x0e);
		rtw_write16(Adapter, REG_RXDMA_AGG_PG_TH_8814A, 0x0a05); /* dmc agg th 20K */

		/* set Reg 0xf008[3:4] to 2'00 to disable U1/U2 Mode to avoid 2.5G spur in USB3.0. added by page, 20120712 */
		rtw_write8(Adapter, 0xf008, rtw_read8(Adapter, 0xf008) & 0xE7);
		/* to avoid usb 3.0 H2C fail */
		rtw_write16(Adapter, 0xf002, 0);

		rtw_write8(Adapter, REG_SW_AMPDU_BURST_MODE_CTRL_8814A, rtw_read8(Adapter, REG_SW_AMPDU_BURST_MODE_CTRL_8814A) & ~BIT(6));
		RTW_INFO("turn off the LDPC pre-TX\n");

	}

	if (pHalData->AMPDUBurstMode)
		rtw_write8(Adapter, REG_SW_AMPDU_BURST_MODE_CTRL_8814A,  0x5F);
}


void
_InitQueueReservedPage_8814AUsb(
		PADAPTER	Adapter
)
{
	struct registry_priv	*pregistrypriv = &Adapter->registrypriv;
	u16		txpktbuf_bndy;

	RTW_INFO("===>_InitQueueReservedPage_8814AUsb()\n");

	/* ---- Set Fifo page for each Queue under Mac Direct LPBK nonsec mode ------------ */
	rtw_write32(Adapter, REG_FIFOPAGE_INFO_1_8814A, HPQ_PGNUM_8814A);
	rtw_write32(Adapter, REG_FIFOPAGE_INFO_2_8814A, LPQ_PGNUM_8814A);
	rtw_write32(Adapter, REG_FIFOPAGE_INFO_3_8814A, NPQ_PGNUM_8814A);
	rtw_write32(Adapter, REG_FIFOPAGE_INFO_4_8814A, EPQ_PGNUM_8814A);

	rtw_write32(Adapter, REG_FIFOPAGE_INFO_5_8814A, PUB_PGNUM_8814A);

	rtw_write32(Adapter, REG_RQPN_CTRL_2_8814A, 0x80000000);

	if (!pregistrypriv->wifi_spec)
		txpktbuf_bndy = TX_PAGE_BOUNDARY_8814A;
	else		/* for WMM */
		txpktbuf_bndy = WMM_NORMAL_TX_PAGE_BOUNDARY_8814A;

	/* Set page boundary and header */
	rtw_write16(Adapter, REG_TXPKTBUF_BCNQ_BDNY_8814A, txpktbuf_bndy);
	rtw_write16(Adapter, REG_TXPKTBUF_BCNQ1_BDNY_8814A, txpktbuf_bndy);
	rtw_write16(Adapter, REG_MGQ_PGBNDY_8814A, txpktbuf_bndy);

	/* Set The head page of packet of Bcnq */
	rtw_write16(Adapter, REG_FIFOPAGE_CTRL_2_8814A, txpktbuf_bndy);
	/* The head page of packet of Bcnq1 */
	rtw_write16(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 2, txpktbuf_bndy);

	RTW_INFO("<===_InitQueueReservedPage_8814AUsb()\n");
}


static u32 _InitPowerOn_8814AU(_adapter *padapter)
{
	int		status = _SUCCESS;
	u16			u2btmp = 0;

	/* YX sugguested 2014.06.03 */
	u8	u1btmp = rtw_read8(padapter, 0x10C2);
	rtw_write8(padapter, 0x10C2, (u1btmp | BIT1));

	if (!HalPwrSeqCmdParsing(padapter, (u8)(~PWR_CUT_TESTCHIP_MSK), PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8814A_NIC_ENABLE_FLOW))
		return _FAIL;


	/* Enable MAC DMA/WMAC/SCHEDULE/SEC block */
	/* Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31. */
	rtw_write16(padapter, REG_CR_8814A, 0x00);  /* suggseted by zhouzhou, by page, 20111230 */
	u2btmp = PlatformEFIORead2Byte(padapter, REG_CR_8814A);
	u2btmp |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
		   | PROTOCOL_EN | SCHEDULE_EN | ENSEC | CALTMR_EN);
	rtw_write16(padapter, REG_CR_8814A, u2btmp);

	_InitQueueReservedPage_8814AUsb(padapter);
	return status;
}





/* ---------------------------------------------------------------
 *
 *	MAC init functions
 *
 * --------------------------------------------------------------- */

/* Shall USB interface init this? */
static void
_InitInterrupt_8814AU(
		PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	/* HIMR */
	rtw_write32(Adapter, REG_HIMR0_8814A, pHalData->IntrMask[0] & 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HIMR1_8814A, pHalData->IntrMask[1] & 0xFFFFFFFF);
}

static void
_InitPageBoundary_8814AUsb(
		PADAPTER Adapter
)
{
	/* 20130416 KaiYuan modified for 8814 */
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	rtw_write16(Adapter, REG_RXFF_PTR_8814A, RX_DMA_BOUNDARY_8814A); /*yx_qi 20140331*/

}


static void
_InitNormalChipRegPriority_8814AUsb(
		PADAPTER	Adapter,
		u16		beQ,
		u16		bkQ,
		u16		viQ,
		u16		voQ,
		u16		mgtQ,
		u16		hiQ
)
{
	u16 value16		= (PlatformEFIORead2Byte(Adapter, REG_TRXDMA_CTRL_8814A) & 0x7);

	value16 |=	_TXDMA_BEQ_MAP(beQ)	| _TXDMA_BKQ_MAP(bkQ) |
			_TXDMA_VIQ_MAP(viQ)	| _TXDMA_VOQ_MAP(voQ) |
			_TXDMA_MGQ_MAP(mgtQ) | _TXDMA_HIQ_MAP(hiQ) | BIT2;

	rtw_write16(Adapter, REG_TRXDMA_CTRL_8814A, value16);
}

static void
_InitNormalChipTwoOutEpPriority_8814AUsb(
		PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);
	struct registry_priv	*pregistrypriv = &Adapter->registrypriv;
	u16			beQ, bkQ, viQ, voQ, mgtQ, hiQ;


	u16	valueHi = 0;
	u16	valueLow = 0;

	switch (pHalData->OutEpQueueSel) {
	case (TX_SELE_HQ | TX_SELE_LQ):
		valueHi = QUEUE_HIGH;
		valueLow = QUEUE_LOW;
		break;
	case (TX_SELE_NQ | TX_SELE_LQ):
		valueHi = QUEUE_NORMAL;
		valueLow = QUEUE_LOW;
		break;
	case (TX_SELE_HQ | TX_SELE_NQ):
		valueHi = QUEUE_HIGH;
		valueLow = QUEUE_NORMAL;
		break;
	default:
		valueHi = QUEUE_HIGH;
		valueLow = QUEUE_NORMAL;
		break;
	}

	if (!pregistrypriv->wifi_spec) {
		beQ		= valueLow;
		bkQ		= valueLow;
		viQ		= valueHi;
		voQ		= valueHi;
		mgtQ	= valueHi;
		hiQ		= valueHi;
	} else { /* for WMM ,CONFIG_OUT_EP_WIFI_MODE */
		beQ		= valueLow;
		bkQ		= valueHi;
		viQ		= valueHi;
		voQ		= valueLow;
		mgtQ	= valueHi;
		hiQ		= valueHi;
	}

	_InitNormalChipRegPriority_8814AUsb(Adapter, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static void
_InitNormalChipThreeOutEpPriority_8814AUsb(
		PADAPTER Adapter
)
{
	struct registry_priv	*pregistrypriv = &Adapter->registrypriv;
	u16			beQ, bkQ, viQ, voQ, mgtQ, hiQ;

	if (!pregistrypriv->wifi_spec) { /* typical setting */
		beQ		= QUEUE_LOW;
		bkQ		= QUEUE_LOW;
		viQ		= QUEUE_NORMAL;
		voQ		= QUEUE_HIGH;
		mgtQ	= QUEUE_HIGH;
		hiQ		= QUEUE_HIGH;
	} else { /* for WMM */
		beQ		= QUEUE_LOW;
		bkQ		= QUEUE_NORMAL;
		viQ		= QUEUE_NORMAL;
		voQ		= QUEUE_HIGH;
		mgtQ	= QUEUE_HIGH;
		hiQ		= QUEUE_HIGH;
	}
	_InitNormalChipRegPriority_8814AUsb(Adapter, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static void
_InitQueuePriority_8814AUsb(
		PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	switch (pHalData->OutEpNumber) {
	case 2:
		_InitNormalChipTwoOutEpPriority_8814AUsb(Adapter);
		break;
	case 3:
	case 4:
		_InitNormalChipThreeOutEpPriority_8814AUsb(Adapter);
		break;
	default:
		RTW_INFO("_InitQueuePriority_8812AUsb(): Shall not reach here!\n");
		break;
	}
}



static void
_InitHardwareDropIncorrectBulkOut_8814A(
		PADAPTER Adapter
)
{
#ifdef ENABLE_USB_DROP_INCORRECT_OUT
	u32	value32 = rtw_read32(Adapter, REG_TXDMA_OFFSET_CHK);
	value32 |= DROP_DATA_EN;
	rtw_write32(Adapter, REG_TXDMA_OFFSET_CHK, value32);
#endif /* ENABLE_USB_DROP_INCORRECT_OUT */
}

static void
_InitNetworkType_8814A(
		PADAPTER Adapter
)
{
	u32	value32;

	value32 = rtw_read32(Adapter, REG_CR);
	/* TODO: use the other function to set network type */
	value32 = (value32 & ~MASK_NETTYPE) | _NETTYPE(NT_LINK_AP);

	rtw_write32(Adapter, REG_CR, value32);
}

static void
_InitTransferPageSize_8814AUsb(
		PADAPTER Adapter
)
{
	/* 8814 doesn't need this by Alex */
}

static void
_InitDriverInfoSize_8814A(
		PADAPTER	Adapter,
		u8		drvInfoSize
)
{
	rtw_write8(Adapter, REG_RX_DRVINFO_SZ, drvInfoSize);
}
#if 0
/* static void
 * _InitWMACSetting_8814A( */
/*   PADAPTER Adapter */
/* ) */
{
	/* u32			value32; */
	u16			value16;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32 rcr;

	/* rcr = AAP | APM | AM | AB | APP_ICV | ADF | AMF | APP_FCS | HTC_LOC_CTRL | APP_MIC | APP_PHYSTS; */
	rcr =
		RCR_APM | RCR_AM | RCR_AB | RCR_CBSSID_DATA | RCR_CBSSID_BCN | RCR_APP_ICV | RCR_AMF | RCR_HTC_LOC_CTRL | RCR_APP_MIC | RCR_APP_PHYST_RXFF;

#if (1 == RTL8812A_RX_PACKET_INCLUDE_CRC)
	rcr |= ACRC32;
#endif /* (1 == RTL8812A_RX_PACKET_INCLUDE_CRC) */

#ifdef CONFIG_RX_PACKET_APPEND_FCS
	rcr |= RCR_APPFCS;
#endif /* CONFIG_RX_PACKET_APPEND_FCS */

	rcr |= FORCEACK;

	rtw_hal_set_hwreg(Adapter, HW_VAR_RCR, (u8 *)&rcr);

	/*  Accept all multicast address */
	rtw_write32(Adapter, REG_MAR, 0xFFFFFFFF);
	rtw_write32(Adapter, REG_MAR + 4, 0xFFFFFFFF);


	/*  Accept all data frames */
	/* value16 = 0xFFFF; */
	/* rtw_write16(Adapter, REG_RXFLTMAP2, value16); */

	/*  2010.09.08 hpfan */
	/*  Since ADF is removed from RCR, ps-poll will not be indicate to driver, */
	/*  RxFilterMap should mask ps-poll to gurantee AP mode can rx ps-poll. */
	value16 = BIT10;
#ifdef CONFIG_BEAMFORMING
	/*  NDPA packet subtype is 0x0101 */
	value16 |= BIT5;
#endif
	rtw_write16(Adapter, REG_RXFLTMAP1, value16);

	/*  Accept all management frames */
	/* value16 = 0xFFFF; */
	/* rtw_write16(Adapter, REG_RXFLTMAP0, value16); */

	/* enable RX_SHIFT bits */
	/* rtw_write8(Adapter, REG_TRXDMA_CTRL, rtw_read8(Adapter, REG_TRXDMA_CTRL)|BIT(1));	 */

}
#endif

/* old _InitWMACSetting_8812A + _InitAdaptiveCtrl_8812AUsb = new _InitMacConfigure_8814A */
static void
_InitMacConfigure_8814A(
		PADAPTER			Adapter
)
{
	u16			value16;
	u32			regRRSR;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32 rcr;

	switch (Adapter->registrypriv.wireless_mode) {
	case WIRELESS_11B:
		regRRSR = RATE_ALL_CCK;
		break;

	case WIRELESS_11G:
	case WIRELESS_11A:
	case WIRELESS_11_5N:
	case WIRELESS_11A_5N: /* Todo: no basic rate for ofdm ? */
	case WIRELESS_11_5AC:
		regRRSR = RATE_ALL_OFDM_AG;
		break;

	case WIRELESS_11BG:
	case WIRELESS_11G_24N:
	case WIRELESS_11_24N:
	case WIRELESS_11BG_24N:
	default:
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;

	}

	/* Init value for RRSR. */
	rtw_phydm_set_rrsr(Adapter, regRRSR, TRUE);


	/* Retry Limit */
	value16 = BIT_LRL(RL_VAL_STA) | BIT_SRL(RL_VAL_STA);
	rtw_write16(Adapter, REG_RETRY_LIMIT, value16);

	rcr = RCR_APM | RCR_AM | RCR_AB | RCR_CBSSID_DATA | RCR_CBSSID_BCN | RCR_APP_ICV | RCR_AMF | RCR_HTC_LOC_CTRL | RCR_APP_MIC | RCR_APP_PHYST_RXFF;
	rcr |= FORCEACK;
#if (1 == RTL8812A_RX_PACKET_INCLUDE_CRC)
	rcr |= ACRC32;
#endif /* (1 == RTL8812A_RX_PACKET_INCLUDE_CRC) */

#ifdef CONFIG_RX_PACKET_APPEND_FCS
	rcr |= RCR_APPFCS;
#endif /* CONFIG_RX_PACKET_APPEND_FCS */
	rtw_hal_set_hwreg(Adapter, HW_VAR_RCR, (u8 *)&rcr);

	/* 2010.09.08 hpfan */
	/* Since ADF is removed from RCR, ps-poll will not be indicate to driver, */
	/* RxFilterMap should mask ps-poll to gurantee AP mode can rx ps-poll. */
	value16 = BIT10;
#ifdef CONFIG_BEAMFORMING
	/* NDPA packet subtype is 0x0101 */
	value16 |= BIT5;
#endif /*CONFIG_BEAMFORMING*/
	rtw_write16(Adapter, REG_RXFLTMAP1, value16);

	/* 201409/25 MH When RA is enabled, we need to reduce the value. */
	rtw_write8(Adapter, REG_MAX_AGGR_NUM_8814A, 0x36);
	rtw_write8(Adapter, REG_RTS_MAX_AGGR_NUM_8814A, 0x36);

}

static void
_InitEDCA_8814AUsb(
		PADAPTER Adapter
)
{
	/* Set Spec SIFS (used in NAV) */
	rtw_write16(Adapter, REG_SPEC_SIFS, 0x100a);
	rtw_write16(Adapter, REG_MAC_SPEC_SIFS, 0x100a);

	/* Set SIFS for CCK */
	rtw_write16(Adapter, REG_SIFS_CTX, 0x100a);

	/* Set SIFS for OFDM */
	rtw_write16(Adapter, REG_SIFS_TRX, 0x100a);

	/* TXOP */
	rtw_write32(Adapter, REG_EDCA_BE_PARAM, 0x005EA42B);
	rtw_write32(Adapter, REG_EDCA_BK_PARAM, 0x0000A44F);
	rtw_write32(Adapter, REG_EDCA_VI_PARAM, 0x005EA324);
	rtw_write32(Adapter, REG_EDCA_VO_PARAM, 0x002FA226);

	/* 0x50 for 80MHz clock */
	/* rtw_write8(Adapter, REG_USTIME_TSF, 0x50); */
	/* rtw_write8(Adapter, REG_USTIME_EDCA, 0x50); */
}


static void
_InitBeaconMaxError_8814A(
		PADAPTER	Adapter,
		BOOLEAN		InfraMode
)
{
#ifdef CONFIG_ADHOC_WORKAROUND_SETTING
	rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xFF);
#else
	/* rtw_write8(Adapter, REG_BCN_MAX_ERR, (InfraMode ? 0xFF : 0x10));	 */
#endif
}


#ifdef CONFIG_RTW_LED
static void _InitHWLed(PADAPTER Adapter)
{
	struct led_priv *pledpriv = adapter_to_led(Adapter);

	if (pledpriv->LedStrategy != HW_LED)
		return;

	/* HW led control
	 * to do ....
	 * must consider cases of antenna diversity/ commbo card/solo card/mini card */

}
#endif /* CONFIG_RTW_LED */

/*
static void
_InitRDGSetting_8812A(
		PADAPTER Adapter
	)
{
	rtw_write8(Adapter,REG_RD_CTRL,0xFF);
	rtw_write16(Adapter, REG_RD_NAV_NXT, 0x200);
	rtw_write8(Adapter,REG_RD_RESP_PKT_TH,0x05);
}*/

static void
_InitRetryFunction_8814A(
		PADAPTER Adapter
)
{
	u8	value8;

	value8 = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL);
	value8 |= EN_AMPDU_RTY_NEW;
	rtw_write8(Adapter, REG_FWHW_TXQ_CTRL, value8);

	/* Set ACK timeout */
	/* rtw_write8(Adapter, REG_ACKTO, 0x40);  */ /* masked by page for BCM IOT issue temporally */
	rtw_write8(Adapter, REG_ACKTO, 0x80);
}

/*-----------------------------------------------------------------------------
 * Function:	usb_AggSettingTxUpdate()
 *
 * Overview:	Seperate TX/RX parameters update independent for TP detection and
 *			dynamic TX/RX aggreagtion parameters update.
 *
 * Input:			PADAPTER
 *
 * Output/Return:	NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	12/10/2010	MHC		Seperate to smaller function.
 *
 *---------------------------------------------------------------------------*/
static void
usb_AggSettingTxUpdate_8814A(
		PADAPTER			Adapter
)
{
#ifdef CONFIG_USB_TX_AGGREGATION
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32			value32;

	if (Adapter->registrypriv.wifi_spec)
		pHalData->UsbTxAggDescNum = 1;

	if (pHalData->UsbTxAggMode) {
		value32 = rtw_read32(Adapter, REG_TDECTRL);
		value32 = value32 & ~(BLK_DESC_NUM_MASK << BLK_DESC_NUM_SHIFT);
		value32 |= ((pHalData->UsbTxAggDescNum & BLK_DESC_NUM_MASK) << BLK_DESC_NUM_SHIFT);

		rtw_write32(Adapter, REG_TDECTRL, value32);
		rtw_write8(Adapter, REG_TDECTRL + 3, pHalData->UsbTxAggDescNum << 1);
	}

#endif /* CONFIG_USB_TX_AGGREGATION */
}	/* usb_AggSettingTxUpdate */


/*-----------------------------------------------------------------------------
 * Function:	usb_AggSettingRxUpdate()
 *
 * Overview:	Seperate TX/RX parameters update independent for TP detection and
 *			dynamic TX/RX aggreagtion parameters update.
 *
 * Input:			PADAPTER
 *
 * Output/Return:	NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	12/10/2010	MHC		Seperate to smaller function.
 *
 *---------------------------------------------------------------------------*/
static void
usb_AggSettingRxUpdate_8814A(
		PADAPTER			Adapter
)
{
#ifdef CONFIG_USB_RX_AGGREGATION
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			valueDMA;
	u8			valueUSB;

	valueDMA = rtw_read8(Adapter, REG_TRXDMA_CTRL_8814A);
	valueUSB = rtw_read8(Adapter, REG_RXDMA_AGG_PG_TH_8814A + 3);
	switch (pHalData->rxagg_mode) {
	case RX_AGG_DMA:
		valueDMA |= RXDMA_AGG_EN;
		valueUSB &= ~USB_AGG_EN_8814A;
		break;
	case RX_AGG_USB:
		valueDMA &= ~RXDMA_AGG_EN;
		valueUSB |= USB_AGG_EN_8814A;
		break;
	case RX_AGG_MIX:
		valueDMA |= RXDMA_AGG_EN;
		valueUSB |= USB_AGG_EN_8814A;
		break;
	case RX_AGG_DISABLE:
	default:
		valueDMA &= ~RXDMA_AGG_EN;
		valueUSB &= ~USB_AGG_EN_8814A;
		break;
	}

	rtw_write8(Adapter, REG_TRXDMA_CTRL_8814A, valueDMA);
	rtw_write8(Adapter, REG_RXDMA_AGG_PG_TH_8814A + 3, valueUSB); /* yx_qi 131128 */
#endif /* CONFIG_USB_RX_AGGREGATION */
}	/* usb_AggSettingRxUpdate */

static void
init_UsbAggregationSetting_8814A(
		PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	/* Tx aggregation setting */
	usb_AggSettingTxUpdate_8814A(Adapter);

	/* Rx aggregation setting */
	usb_AggSettingRxUpdate_8814A(Adapter);

	/* 201/12/10 MH Add for USB agg mode dynamic switch. */
	pHalData->UsbRxHighSpeedMode = _FALSE;
	pHalData->UsbTxVeryHighSpeedMode = _FALSE;
}

/*-----------------------------------------------------------------------------
 * Function:	USB_AggModeSwitch()
 *
 * Overview:	When RX traffic is more than 40M, we need to adjust some parameters to increase
 *			RX speed by increasing batch indication size. This will decrease TCP ACK speed, we
 *			need to monitor the influence of FTP/network share.
 *			For TX mode, we are still ubder investigation.
 *
 * Input:		PADAPTER
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	12/10/2010	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
void
USB_AggModeSwitch(
		PADAPTER			Adapter
)
{
#if 0
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);

	/* pHalData->UsbRxHighSpeedMode = _FALSE; */
	/* How to measure the RX speed? We assume that when traffic is more than */
	if (pMgntInfo->bRegAggDMEnable == _FALSE) {
		return;	/* Inf not support. */
	}


	if (pMgntInfo->LinkDetectInfo.bHigherBusyRxTraffic == _TRUE &&
	    pHalData->UsbRxHighSpeedMode == _FALSE) {
		pHalData->UsbRxHighSpeedMode = _TRUE;
	} else if (pMgntInfo->LinkDetectInfo.bHigherBusyRxTraffic == _FALSE &&
		   pHalData->UsbRxHighSpeedMode == _TRUE) {
		pHalData->UsbRxHighSpeedMode = _FALSE;
	} else
		return;


#if USB_RX_AGGREGATION_92C
	if (pHalData->UsbRxHighSpeedMode == _TRUE) {
		/* 2010/12/10 MH The parameter is tested by SD1 engineer and SD3 channel emulator. */
		/* USB mode */
#if (RT_PLATFORM == PLATFORM_LINUX)
		if (pMgntInfo->LinkDetectInfo.bTxBusyTraffic) {
			pHalData->RxAggBlockCount	= 16;
			pHalData->RxAggBlockTimeout	= 7;
		} else
#endif
		{
			pHalData->RxAggBlockCount	= 40;
			pHalData->RxAggBlockTimeout	= 5;
		}
		/* Mix mode */
		pHalData->RxAggPageCount	= 72;
		pHalData->RxAggPageTimeout	= 6;
	} else {
		/* USB mode */
		pHalData->RxAggBlockCount	= pMgntInfo->RegRxAggBlockCount;
		pHalData->RxAggBlockTimeout	= pMgntInfo->RegRxAggBlockTimeout;
		/* Mix mode */
		pHalData->RxAggPageCount		= pMgntInfo->RegRxAggPageCount;
		pHalData->RxAggPageTimeout	= pMgntInfo->RegRxAggPageTimeout;
	}

	if (pHalData->RxAggBlockCount > MAX_RX_AGG_BLKCNT)
		pHalData->RxAggBlockCount = MAX_RX_AGG_BLKCNT;
#if (OS_WIN_FROM_VISTA(OS_VERSION)) || (RT_PLATFORM == PLATFORM_LINUX)	/* do not support WINXP to prevent usbehci.sys BSOD */
	if (IS_WIRELESS_MODE_N_24G(Adapter) || IS_WIRELESS_MODE_N_5G(Adapter)) {
		/*  */
		/* 2010/12/24 MH According to V1012 QC IOT test, XP BSOD happen when running chariot test */
		/* with the aggregation dynamic change!! We need to disable the function to prevent it is broken */
		/* in usbehci.sys. */
		/*  */
		usb_AggSettingRxUpdate_8188E(Adapter);

		/* 2010/12/27 MH According to designer's suggstion, we can only modify Timeout value. Otheriwse */
		/* there might many HW incorrect behavior, the XP BSOD at usbehci.sys may be relative to the */
		/* issue. Base on the newest test, we can not enable block cnt > 30, otherwise XP usbehci.sys may */
		/* BSOD. */
	}
#endif

#endif
#endif
}	/* USB_AggModeSwitch */


#if 0
/* static void _BBTurnOnBlock( */
/* 	PADAPTER		Adapter */
/* ) */
{
#if (DISABLE_BB_RF)
	return;
#endif

	phy_set_bb_reg(Adapter, rFPGA0_RFMOD, bCCKEn, 0x1);
	phy_set_bb_reg(Adapter, rFPGA0_RFMOD, bOFDMEn, 0x1);
}
#endif


/*
 * 2010/08/26 MH Add for selective suspend mode check.
 * If Efuse 0x0e bit1 is not enabled, we can not support selective suspend for Minicard and
 * slim card.
 *   */
#if 0
static void
HalDetectSelectiveSuspendMode(
		PADAPTER				Adapter
)
{
	u8	tmpvalue;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);

	/* If support HW radio detect, we need to enable WOL ability, otherwise, we */
	/* can not use FW to notify host the power state switch. */

	EFUSE_ShadowRead(Adapter, 1, EEPROM_USB_OPTIONAL1, (u32 *)&tmpvalue);

	RTW_INFO("HalDetectSelectiveSuspendMode(): SS ");
	if (tmpvalue & BIT1)
		RTW_INFO("Enable\n");
	else {
		RTW_INFO("Disable\n");
		pdvobjpriv->RegUsbSS = _FALSE;
	}

	/* 2010/09/01 MH According to Dongle Selective Suspend INF. We can switch SS mode. */
	if (pdvobjpriv->RegUsbSS && !SUPPORT_HW_RADIO_DETECT(pHalData)) {
		/* PMGNT_INFO				pMgntInfo = &(Adapter->MgntInfo); */

		/* if (!pMgntInfo->bRegDongleSS)	 */
		/* { */
		pdvobjpriv->RegUsbSS = _FALSE;
		/* } */
	}
}	/* HalDetectSelectiveSuspendMode */
#endif

rt_rf_power_state RfOnOffDetect(PADAPTER pAdapter)
{
	rt_rf_power_state rfpowerstate = rf_on;

	return rfpowerstate;
}	/* HalDetectPwrDownMode */

void _ps_open_RF(_adapter *padapter)
{
	/* here call with bRegSSPwrLvl 1, bRegSSPwrLvl 2 needs to be verified */
	/* phy_SsPwrSwitch92CU(padapter, rf_on, 1); */
}

void _ps_close_RF(_adapter *padapter)
{
	/* here call with bRegSSPwrLvl 1, bRegSSPwrLvl 2 needs to be verified */
	/* phy_SsPwrSwitch92CU(padapter, rf_off, 1); */
}


/*	A lightweight deinit function	*/
static void rtl8814au_hw_reset(_adapter *Adapter)
{
#if 0
	u8 reg_val = 0;
	if (rtw_read8(Adapter, REG_MCUFWDL) & BIT7) {
		_8051Reset8812(Adapter);
		rtw_write8(Adapter, REG_MCUFWDL, 0x00);
		/* before BB reset should do clock gated */
		rtw_write32(Adapter, rFPGA0_XCD_RFPara,
			    rtw_read32(Adapter, rFPGA0_XCD_RFPara) | (BIT6));
		/* reset BB */
		reg_val = rtw_read8(Adapter, REG_SYS_FUNC_EN);
		reg_val &= ~(BIT(0) | BIT(1));
		rtw_write8(Adapter, REG_SYS_FUNC_EN, reg_val);
		/* reset RF */
		rtw_write8(Adapter, REG_RF_CTRL, 0);
		/* reset TRX path */
		rtw_write16(Adapter, REG_CR, 0);
		/* reset MAC */
		reg_val = rtw_read8(Adapter, REG_APS_FSMCO + 1);
		reg_val |= BIT(1);
		reg_val = rtw_write8(Adapter, REG_APS_FSMCO + 1, reg_val);     /* reg0x5[1] ,auto FSM off */

		reg_val = rtw_read8(Adapter, REG_APS_FSMCO + 1);

		/* check if   reg0x5[1] auto cleared */
		while (reg_val & BIT(1)) {
			rtw_udelay_os(1);
			reg_val = rtw_read8(Adapter, REG_APS_FSMCO + 1);
		}
		reg_val |= BIT(0);
		reg_val = rtw_write8(Adapter, REG_APS_FSMCO + 1, reg_val);   /* reg0x5[0] ,auto FSM on */

		reg_val = rtw_read8(Adapter, REG_SYS_FUNC_EN + 1);
		reg_val &= ~(BIT(4) | BIT(7));
		rtw_write8(Adapter, REG_SYS_FUNC_EN + 1, reg_val);
		reg_val = rtw_read8(Adapter, REG_SYS_FUNC_EN + 1);
		reg_val |= BIT(4) | BIT(7);
		rtw_write8(Adapter, REG_SYS_FUNC_EN + 1, reg_val);
	}
#endif /* 0 */
}

u32 rtl8814au_hal_init(PADAPTER Adapter)
{
	u8	value8 = 0, u1bRegCR;
	u16  value16;
	u8	txpktbuf_bndy;
	u32	status = _SUCCESS;
	u32	NavUpper = WiFiNavUpperUs;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrctrlpriv = adapter_to_pwrctl(Adapter);
	struct registry_priv	*pregistrypriv = &Adapter->registrypriv;

	rt_rf_power_state		eRfPowerStateToSet;

	systime init_start_time = rtw_get_current_time();


#ifdef DBG_HAL_INIT_PROFILING

	enum HAL_INIT_STAGES {
		HAL_INIT_STAGES_BEGIN = 0,
		HAL_INIT_STAGES_INIT_PW_ON,
		HAL_INIT_STAGES_INIT_LLTT,
		HAL_INIT_STAGES_DOWNLOAD_FW,
		HAL_INIT_STAGES_MAC,
		HAL_INIT_STAGES_MISC01,
		HAL_INIT_STAGES_MISC02,
		HAL_INIT_STAGES_BB,
		HAL_INIT_STAGES_RF,
		HAL_INIT_STAGES_TURN_ON_BLOCK,
		HAL_INIT_STAGES_INIT_SECURITY,
		HAL_INIT_STAGES_MISC11,
		HAL_INIT_STAGES_INIT_HAL_DM,
		/* HAL_INIT_STAGES_RF_PS, */
		HAL_INIT_STAGES_IQK,
		HAL_INIT_STAGES_PW_TRACK,
		HAL_INIT_STAGES_LCK,
		HAL_INIT_STAGES_MISC21,
		/* HAL_INIT_STAGES_INIT_PABIAS, */
#ifdef CONFIG_BT_COEXIST
		HAL_INIT_STAGES_BT_COEXIST,
#endif
		/* HAL_INIT_STAGES_ANTENNA_SEL, */
		HAL_INIT_STAGES_MISC31,
		HAL_INIT_STAGES_END,
		HAL_INIT_STAGES_NUM
	};

	char *hal_init_stages_str[] = {
		"HAL_INIT_STAGES_BEGIN",
		"HAL_INIT_STAGES_INIT_PW_ON",
		"HAL_INIT_STAGES_INIT_LLTT",
		"HAL_INIT_STAGES_DOWNLOAD_FW",
		"HAL_INIT_STAGES_MAC",
		"HAL_INIT_STAGES_MISC01",
		"HAL_INIT_STAGES_MISC02",
		"HAL_INIT_STAGES_BB",
		"HAL_INIT_STAGES_RF",
		"HAL_INIT_STAGES_TURN_ON_BLOCK",
		"HAL_INIT_STAGES_INIT_SECURITY",
		"HAL_INIT_STAGES_MISC11",
		"HAL_INIT_STAGES_INIT_HAL_DM",
		/* "HAL_INIT_STAGES_RF_PS", */
		"HAL_INIT_STAGES_IQK",
		"HAL_INIT_STAGES_PW_TRACK",
		"HAL_INIT_STAGES_LCK",
		"HAL_INIT_STAGES_MISC21",
#ifdef CONFIG_BT_COEXIST
		"HAL_INIT_STAGES_BT_COEXIST",
#endif
		/* "HAL_INIT_STAGES_ANTENNA_SEL", */
		"HAL_INIT_STAGES_MISC31",
		"HAL_INIT_STAGES_END",
	};

	int hal_init_profiling_i;
	systime hal_init_stages_timestamp[HAL_INIT_STAGES_NUM]; /* used to record the time of each stage's starting point */

	for (hal_init_profiling_i = 0; hal_init_profiling_i < HAL_INIT_STAGES_NUM; hal_init_profiling_i++)
		hal_init_stages_timestamp[hal_init_profiling_i] = 0;

#define HAL_INIT_PROFILE_TAG(stage) do { hal_init_stages_timestamp[(stage)] = rtw_get_current_time(); } while (0)
#else /* DBG_HAL_INIT_PROFILING */
#define HAL_INIT_PROFILE_TAG(stage) do {} while (0)
#endif /* DBG_HAL_INIT_PROFILING */




	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_BEGIN);
	if (pwrctrlpriv->bkeepfwalive) {
		_ps_open_RF(Adapter);

		if (pHalData->bIQKInitialized) {
			/* phy_iq_calibrate_8812a(Adapter,_TRUE); */
		} else {
			/* phy_iq_calibrate_8812a(Adapter,_FALSE); */
			/* pHalData->bIQKInitialized = _TRUE; */
		}

		/* odm_txpowertracking_check(&pHalData->odmpriv ); */
		/* phy_lc_calibrate_8812a(Adapter); */

		goto exit;
	}

	/* Check if MAC has already power on. by tynli. 2011.05.27. */
	value8 = rtw_read8(Adapter, REG_SYS_CLKR + 1);
	u1bRegCR = rtw_read8(Adapter, REG_CR);
	RTW_INFO(" power-on :REG_SYS_CLKR 0x09=0x%02x. REG_CR 0x100=0x%02x.\n", value8, u1bRegCR);
	if ((value8 & BIT3)  && (u1bRegCR != 0 && u1bRegCR != 0xEA)) {
		/* pHalData->bMACFuncEnable = _TRUE; */
		RTW_INFO(" MAC has already power on.\n");
	} else {
		/* pHalData->bMACFuncEnable = _FALSE; */
		/* Set FwPSState to ALL_ON mode to prevent from the I/O be return because of 32k */
		/* state which is set before sleep under wowlan mode. 2012.01.04. by tynli. */
		/* pHalData->FwPSState = FW_PS_STATE_ALL_ON_88E; */
		RTW_INFO(" MAC has not been powered on yet.\n");
	}

	/*  */
	/* 2012/11/13 MH Revise for U2/U3 switch we can not update RF-A/B reset. */
	/* After discuss with BB team YN, reset after MAC power on to prevent RF */
	/* R/W error. Is it a right method? */
	/*  */
	/*if(!IS_HARDWARE_TYPE_8821(Adapter))
	{
		rtw_write8(Adapter, REG_RF_CTRL, 5);
		rtw_write8(Adapter, REG_RF_CTRL, 7);
		rtw_write8(Adapter, REG_RF_B_CTRL_8812, 5);
		rtw_write8(Adapter, REG_RF_B_CTRL_8812, 7);
	}*/

	/*
		If HW didn't go through a complete de-initial procedure,
		it probably occurs some problem for double initial procedure.
		Like "CONFIG_DEINIT_BEFORE_INIT" in 92du chip
	*/
	rtl8814au_hw_reset(Adapter); /* todo */



	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_PW_ON);
	status = rtw_hal_power_on(Adapter);
	if (status == _FAIL) {
		RTW_INFO("Failed to init power on!\n");
		goto exit;
	}

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_LLTT);

	status =  InitLLTTable8814A(Adapter);
	if (status == _FAIL) {
		RTW_INFO("Failed to init LLT table\n");
		goto exit;
	}

	_InitHardwareDropIncorrectBulkOut_8814A(Adapter);

	/*if(pHalData->bRDGEnable){
		_InitRDGSetting_8812A(Adapter);
	}*/

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_DOWNLOAD_FW);
	if (Adapter->registrypriv.mp_mode == 0) {
		status = FirmwareDownload8814A(Adapter, _FALSE);
		if (status != _SUCCESS) {
			RTW_INFO("%s: Download Firmware failed!!\n", __FUNCTION__);
			pHalData->bFWReady = _FALSE;
			pHalData->fw_ractrl = _FALSE;
			/* return status; */
		} else {
			RTW_INFO("%s: Download Firmware Success!!\n", __FUNCTION__);
			pHalData->bFWReady = _TRUE;
			pHalData->fw_ractrl = _TRUE;
		}
	}

	if (pwrctrlpriv->reg_rfoff == _TRUE)
		pwrctrlpriv->rf_pwrstate = rf_off;

	/* 2010/08/09 MH We need to check if we need to turnon or off RF after detecting */
	/* HW GPIO pin. Before PHY_RFConfig8192C. */
	/* HalDetectPwrDownMode(Adapter); */
	/* 2010/08/26 MH If Efuse does not support sective suspend then disable the function. */
	/* HalDetectSelectiveSuspendMode(Adapter); */

	/* Save target channel */
	/* <Roger_Notes> Current Channel will be updated again later. */
	pHalData->current_channel = 0;/* set 0 to trigger switch correct channel */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MAC);
#if (HAL_MAC_ENABLE == 1)
	status = PHY_MACConfig8814(Adapter);
	if (status == _FAIL)
		goto exit;
#endif /* HAL_MAC_ENABLE */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC01);

	_InitQueuePriority_8814AUsb(Adapter);
	_InitPageBoundary_8814AUsb(Adapter);

	_InitTransferPageSize_8814AUsb(Adapter);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC02);
	/* Get Rx PHY status in order to report RSSI and others. */
	_InitDriverInfoSize_8814A(Adapter, DRVINFO_SZ);

	_InitInterrupt_8814AU(Adapter);
	_InitNetworkType_8814A(Adapter);/* set msr	 */
	_InitMacConfigure_8814A(Adapter);
	/* _InitWMACSetting_8814A(Adapter); */
	/* _InitAdaptiveCtrl_8814AUsb(Adapter); */
	_InitEDCA_8814AUsb(Adapter);

	_InitRetryFunction_8814A(Adapter);
	init_UsbAggregationSetting_8814A(Adapter);

	_InitBeaconParameters_8814A(Adapter);
	_InitBeaconMaxError_8814A(Adapter, _TRUE);

	_InitBurstPktLen(Adapter);  /* added by page. 20110919 */

	/*  */
	/* Init CR MACTXEN, MACRXEN after setting RxFF boundary REG_TRXFF_BNDY to patch */
	/* Hw bug which Hw initials RxFF boundry size to a value which is larger than the real Rx buffer size in 88E. */
	/* 2011.08.05. by tynli. */
	/*  */
	value8 = rtw_read8(Adapter, REG_CR);
	rtw_write8(Adapter, REG_CR, (value8 | MACTXEN | MACRXEN));

#ifdef CONFIG_RTW_LED
	_InitHWLed(Adapter);
#endif /* CONFIG_RTW_LED */

	/*  */
	/* d. Initialize BB related configurations. */
	/*  */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_BB);
#if (HAL_BB_ENABLE == 1)
	status = PHY_BBConfig8814(Adapter);
	if (status == _FAIL)
		goto exit;
#endif /* HAL_BB_ENABLE */

	/* 92CU use 3-wire to r/w RF */
	/* pHalData->Rf_Mode = RF_OP_By_SW_3wire; */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_RF);
#if (HAL_RF_ENABLE == 1)
	status = PHY_RFConfig8814A(Adapter);
	if (status == _FAIL)
		goto exit;

	/* todo: */
	/* if(pHalData->rf_type == RF_1T1R && IS_HARDWARE_TYPE_8812AU(Adapter)) */
	/* PHY_BB8812_Config_1T(Adapter); */
#endif

	PHY_ConfigBB_8814A(Adapter);

	if (Adapter->registrypriv.channel <= 14)
		PHY_SwitchWirelessBand8814A(Adapter, BAND_ON_2_4G);
	else
		PHY_SwitchWirelessBand8814A(Adapter, BAND_ON_5G);

	rtw_hal_set_chnl_bw(Adapter, Adapter->registrypriv.channel,
		CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE, HAL_PRIME_CHNL_OFFSET_DONT_CARE);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_TURN_ON_BLOCK);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_SECURITY);
	invalidate_cam_all(Adapter);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC11);

	/* HW SEQ CTRL */
	/* set 0x0 to 0xFF by tynli. Default enable HW SEQ NUM. */
	rtw_write8(Adapter, REG_HWSEQ_CTRL, 0xFF);

	/*  */
	/* Disable BAR, suggested by Scott */
	/* 2010.04.09 add by hpfan */
	/*  */
	rtw_write32(Adapter, REG_BAR_MODE_CTRL, 0x0201ffff);

	rtw_write8(Adapter, REG_SECONDARY_CCA_CTRL_8814A, 0x03);

	if (pregistrypriv->wifi_spec)
		rtw_write16(Adapter, REG_FAST_EDCA_CTRL , 0);
	/* adjust EDCCA to avoid collision */
	/*if(pregistrypriv->wifi_spec)
	{
		rtw_write16(Adapter, rEDCCA_Jaguar, 0xfe01);
	}*/
	/* Nav limit , suggest by scott */
	rtw_write8(Adapter, 0x652, 0x0);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_HAL_DM);
	rtl8814_InitHalDm(Adapter);

	/*  */
	/* 2010/08/11 MH Merge from 8192SE for Minicard init. We need to confirm current radio status */
	/* and then decide to enable RF or not.!!!??? For Selective suspend mode. We may not */
	/* call init_adapter. May cause some problem?? */
	/*  */
	/* Fix the bug that Hw/Sw radio off before S3/S4, the RF off action will not be executed */
	/* in MgntActSet_RF_State() after wake up, because the value of pHalData->eRFPowerState */
	/* is the same as eRfOff, we should change it to eRfOn after we config RF parameters. */
	/* Added by tynli. 2010.03.30. */
	pwrctrlpriv->rf_pwrstate = rf_on;

	/*phy_iq_calibrate_8814a_init(&pHalData->odmpriv);*/

#if (HAL_BB_ENABLE == 1)
	PHY_SetRFEReg8814A(Adapter, _TRUE, pHalData->current_band_type);
#endif /* HAL_BB_ENABLE */

	/* 0x4c6[3] 1: RTS BW = Data BW */
	/* 0: RTS BW depends on CCA / secondary CCA result. */
	rtw_write8(Adapter, REG_QUEUE_CTRL, rtw_read8(Adapter, REG_QUEUE_CTRL) & 0xF7);

	rtw_hal_set_hwreg(Adapter, HW_VAR_NAV_UPPER, ((u8 *)&NavUpper));

	/* enable Tx report. */
	rtw_write8(Adapter,  REG_FWHW_TXQ_CTRL + 1, 0x0F);

	/* Suggested by SD1 pisa. Added by tynli. 2011.10.21. */
	/* rtw_write8(Adapter, REG_EARLY_MODE_CONTROL_8812+3, 0x01); */ /* Pretx_en, for WEP/TKIP SEC */

	/* tynli_test_tx_report. */
	/* rtw_write16(Adapter, REG_TX_RPT_TIME, 0x3DF0); */

	/* Reset USB mode switch setting */
	rtw_write8(Adapter, REG_SDIO_CTRL_8814A, 0x0);
	rtw_write8(Adapter, REG_ACLK_MON, 0x0);


	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_IQK);
	/* 2010/08/26 MH Merge from 8192CE. */
	if (pwrctrlpriv->rf_pwrstate == rf_on) {
		/*		if(IS_HARDWARE_TYPE_8812AU(Adapter))
				{
		#if (RTL8812A_SUPPORT == 1)
					pHalData->bNeedIQK = _TRUE;
					if(pHalData->bIQKInitialized)
						phy_iq_calibrate_8812a(Adapter, _TRUE);
					else
					{
						phy_iq_calibrate_8812a(Adapter, _FALSE);
						pHalData->bIQKInitialized = _TRUE;
					}
		#endif
				}*/
		/* this should be done by rf team using phydm code */
		/* phy_iq_calibrate_8814a(&pHalData->odmpriv, _FALSE); */
		HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_PW_TRACK);

		/* odm_txpowertracking_check(&pHalData->odmpriv ); */


		HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_LCK);
		/* phy_lc_calibrate_8812a(Adapter); */
	}


	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC21);

#if (MP_DRIVER == 1)
	if (Adapter->registrypriv.mp_mode == 1) {
		Adapter->mppriv.channel = pHalData->current_channel;
		MPT_InitializeAdapter(Adapter, Adapter->mppriv.channel);
	}
#endif	/* #if (MP_DRIVER == 1) */

#ifdef CONFIG_BT_COEXIST
	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_BT_COEXIST);
	/* _InitBTCoexist(Adapter); */

	if (_TRUE == pHalData->EEPROMBluetoothCoexist) {
		/* Init BT hw config. */
		rtw_btcoex_HAL_Initialize(Adapter, _FALSE);
	} else {
		/* In combo card run wifi only , must setting some hardware reg. */
		rtl8812a_combo_card_WifiOnlyHwInit(Adapter);
	}
#endif /* CONFIG_BT_COEXIST */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC31);

	/* rtw_write8(Adapter, REG_USB_HRPWM, 0); */

#ifdef CONFIG_XMIT_ACK
	/* ack for xmit mgmt frames. */
	rtw_write32(Adapter, REG_FWHW_TXQ_CTRL, rtw_read32(Adapter, REG_FWHW_TXQ_CTRL) | BIT(12));
#endif /* CONFIG_XMIT_ACK */


exit:
	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_END);

	RTW_INFO("%s in %dms\n", __FUNCTION__, rtw_get_passing_time_ms(init_start_time));

#ifdef DBG_HAL_INIT_PROFILING
	hal_init_stages_timestamp[HAL_INIT_STAGES_END] = rtw_get_current_time();

	for (hal_init_profiling_i = 0; hal_init_profiling_i < HAL_INIT_STAGES_NUM - 1; hal_init_profiling_i++) {
		RTW_INFO("DBG_HAL_INIT_PROFILING: %35s, %u, %5u, %5u\n"
			 , hal_init_stages_str[hal_init_profiling_i]
			 , hal_init_stages_timestamp[hal_init_profiling_i]
			, (hal_init_stages_timestamp[hal_init_profiling_i + 1] - hal_init_stages_timestamp[hal_init_profiling_i])
			, rtw_get_time_interval_ms(hal_init_stages_timestamp[hal_init_profiling_i], hal_init_stages_timestamp[hal_init_profiling_i + 1])
			);
	}
#endif



	return status;
}

void
hal_carddisable_8814(
		PADAPTER			Adapter
)
{
	u8	u1bTmp;

	RTW_INFO("CardDisableRTL8814AU\n");

	/* stop rx */
	rtw_write8(Adapter, REG_CR_8814A, 0x0);

	/* Card disable power action flow */
	HalPwrSeqCmdParsing(Adapter, (u8)(~PWR_CUT_TESTCHIP_MSK), PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8814A_NIC_DISABLE_FLOW);

	GET_HAL_DATA(Adapter)->bFWReady = _FALSE;

}

static void rtl8814au_hw_power_down(_adapter *padapter)
{
	/* 2010/-8/09 MH For power down module, we need to enable register block contrl reg at 0x1c. */
	/* Then enable power down control bit of register 0x04 BIT4 and BIT15 as 1. */

	/* Enable register area 0x0-0xc. */
	rtw_write8(padapter, REG_RSV_CTRL, 0x0);
	rtw_write16(padapter, REG_APS_FSMCO, 0x8812);
}

u32 rtl8814au_hal_deinit(PADAPTER Adapter)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(Adapter);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	RTW_INFO("==> %s\n", __FUNCTION__);

#ifdef CONFIG_BT_COEXIST
	if (hal_btcoex_IsBtExist(Adapter)) {
		RTW_INFO("BT module enable SIC\n");
		/* Only under WIN7 we can support selective suspend and enter D3 state when system call halt adapter. */

		/* rtw_write16(Adapter, REG_GPIO_MUXCFG, rtw_read16(Adapter, REG_GPIO_MUXCFG)|BIT12); */
		/* 2010/10/13 MH If we enable SIC in the position and then call _ResetDigitalProcedure1. in XP, */
		/* the system will hang due to 8051 reset fail. */
	} else
#endif /* CONFIG_BT_COEXIST */
	{
		rtw_write16(Adapter, REG_GPIO_MUXCFG, rtw_read16(Adapter, REG_GPIO_MUXCFG) & (~BIT12));
	}

	if (pHalData->bSupportUSB3 == _TRUE) {
		/* set Reg 0xf008[3:4] to 2'11 to eable U1/U2 Mode in USB3.0. added by page, 20120712 */
		rtw_write8(Adapter, 0xf008, rtw_read8(Adapter, 0xf008) | 0x18);
	}

	rtw_write32(Adapter, REG_HISR, 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HISRE, 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HIMR, 0);
	rtw_write32(Adapter, REG_HIMRE, 0);

#ifdef SUPPORT_HW_RFOFF_DETECTED
	RTW_INFO("bkeepfwalive(%x)\n", pwrctl->bkeepfwalive);
	if (pwrctl->bkeepfwalive) {
		_ps_close_RF(Adapter);
		if ((pwrctl->bHWPwrPindetect) && (pwrctl->bHWPowerdown))
			rtl8814au_hw_power_down(Adapter);
	} else
#endif
	{
		if (rtw_is_hw_init_completed(Adapter)) {
			rtw_hal_power_off(Adapter);

			if ((pwrctl->bHWPwrPindetect) && (pwrctl->bHWPowerdown))
				rtl8814au_hw_power_down(Adapter);
		}
	}
	return _SUCCESS;
}


unsigned int rtl8814au_inirp_init(PADAPTER Adapter)
{
	u8 i;
	struct recv_buf *precvbuf;
	uint	status;
	struct dvobj_priv *pdev = adapter_to_dvobj(Adapter);
	struct intf_hdl *pintfhdl = &Adapter->iopriv.intf;
	struct recv_priv *precvpriv = &(Adapter->recvpriv);
	u32(*_read_port)(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *pmem);
#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	u32(*_read_interrupt)(struct intf_hdl *pintfhdl, u32 addr);
#endif


	_read_port = pintfhdl->io_ops._read_port;

	status = _SUCCESS;


	precvpriv->ff_hwaddr = RECV_BULK_IN_ADDR;

	/* issue Rx irp to receive data	 */
	precvbuf = (struct recv_buf *)precvpriv->precv_buf;
	for (i = 0; i < NR_RECVBUFF; i++) {
		if (_read_port(pintfhdl, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf) == _FALSE) {
			status = _FAIL;
			goto exit;
		}

		precvbuf++;
		precvpriv->free_recv_buf_queue_cnt--;
	}

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	if (pdev->RtInPipe[REALTEK_USB_IN_INT_EP_IDX] != 0x05) {
		status = _FAIL;
		RTW_INFO("%s =>Warning !! Have not USB Int-IN pipe, RtIntInPipe(%d)!!!\n", __func__, pdev->RtInPipe[REALTEK_USB_IN_INT_EP_IDX]);
		goto exit;
	}
	_read_interrupt = pintfhdl->io_ops._read_interrupt;
	if (_read_interrupt(pintfhdl, RECV_INT_IN_ADDR) == _FALSE) {
		status = _FAIL;
	}
#endif

exit:



	return status;

}

unsigned int rtl8814au_inirp_deinit(PADAPTER Adapter)
{

	rtw_read_port_cancel(Adapter);


	return _SUCCESS;
}

/* -------------------------------------------------------------------
 *
 *	EEPROM/EFUSE Content Parsing
 *
 * ------------------------------------------------------------------- */
void
hal_ReadIDs_8814AU(
		PADAPTER	Adapter,
		u8 			*PROMContent,
		BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		pHalData->EEPROMVID = ReadLE2Byte(&PROMContent[EEPROM_VID_8814AU]);
		pHalData->EEPROMPID = ReadLE2Byte(&PROMContent[EEPROM_PID_8814AU]);

		/* Customer ID, 0x00 and 0xff are reserved for Realtek.		 */
		pHalData->EEPROMCustomerID = *(u8 *)&PROMContent[EEPROM_CustomID_8814];
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;
	} else {
		pHalData->EEPROMVID			= EEPROM_Default_VID;
		pHalData->EEPROMPID			= EEPROM_Default_PID;

		/* Customer ID, 0x00 and 0xff are reserved for Realtek.		 */
		pHalData->EEPROMCustomerID		= EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID	= EEPROM_Default_SubCustomerID;
	}

	RTW_INFO("VID = 0x%04X, PID = 0x%04X\n", pHalData->EEPROMVID, pHalData->EEPROMPID);
	RTW_INFO("Customer ID: 0x%02X, SubCustomer ID: 0x%02X\n", pHalData->EEPROMCustomerID, pHalData->EEPROMSubCustomerID);
}


void
hal_CustomizedBehavior_8814AU(
		PADAPTER	Adapter
)
{
#ifdef CONFIG_RTW_SW_LED
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*pledpriv = adapter_to_led(Adapter);


	/* Led mode */
	switch (pHalData->CustomerID) {
	case RT_CID_DEFAULT:
		pledpriv->LedStrategy = SW_LED_MODE9;
		pledpriv->bRegUseLed = _TRUE;
		break;

	default:
		pledpriv->LedStrategy = SW_LED_MODE9;
		break;
	}
#endif
}

static void
hal_CustomizeByCustomerID_8814AU(
		PADAPTER		pAdapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	RTW_INFO("PID= 0x%x, VID=  %x\n", pHalData->EEPROMPID, pHalData->EEPROMVID);

	/*	Decide CustomerID according to VID/DID or EEPROM */
	switch (pHalData->EEPROMCustomerID) {
	case EEPROM_CID_DEFAULT:
		if ((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3308))
			pHalData->CustomerID = RT_CID_DLINK;
		else if ((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3309))
			pHalData->CustomerID = RT_CID_DLINK;
		else if ((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x330a))
			pHalData->CustomerID = RT_CID_DLINK;
		else if ((pHalData->EEPROMVID == 0x0BFF) && (pHalData->EEPROMPID == 0x8160))
			pHalData->CustomerID = RT_CID_CHINA_MOBILE;
		else if ((pHalData->EEPROMVID == 0x0BDA) &&	(pHalData->EEPROMPID == 0x5088))
			pHalData->CustomerID = RT_CID_CC_C;

		break;
	case EEPROM_CID_WHQL:
		/* padapter->bInHctTest = _TRUE; */

		/* pMgntInfo->bSupportTurboMode = _FALSE; */
		/* pMgntInfo->bAutoTurboBy8186 = _FALSE; */

		/* pMgntInfo->PowerSaveControl.bInactivePs = _FALSE; */
		/* pMgntInfo->PowerSaveControl.bIPSModeBackup = _FALSE; */
		/* pMgntInfo->PowerSaveControl.bLeisurePs = _FALSE; */
		/* pMgntInfo->PowerSaveControl.bLeisurePsModeBackup = _FALSE; */
		/* pMgntInfo->keepAliveLevel = 0; */

		/* padapter->bUnloadDriverwhenS3S4 = _FALSE; */
		break;
	default:
		pHalData->CustomerID = RT_CID_DEFAULT;
		break;

	}
	RTW_INFO("Customer ID: 0x%2x\n", pHalData->CustomerID);

	hal_CustomizedBehavior_8814AU(pAdapter);
}

void
hal_ReadUsbModeSwitch_8814AU(
		PADAPTER	Adapter,
		u8			*PROMContent,
		BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);

	if (AutoloadFail)
		pHalData->EEPROMUsbSwitch = _FALSE;
	else
		/* check efuse 0x0e bit4 */
		pHalData->EEPROMUsbSwitch = (PROMContent[EEPROM_USB_MODE_8814A] & BIT4) >> 4;

	RTW_INFO("Usb Switch: %d\n", pHalData->EEPROMUsbSwitch);
}

static void
ReadLEDSetting_8814AU(
		PADAPTER	Adapter,
		u8		*PROMContent,
		BOOLEAN		AutoloadFail
)
{
#ifdef CONFIG_RTW_LED
	struct led_priv *pledpriv = adapter_to_led(Adapter);

#ifdef CONFIG_RTW_SW_LED
	pledpriv->bRegUseLed = _TRUE;
#else /* HW LED */
	pledpriv->LedStrategy = HW_LED;
#endif /* CONFIG_RTW_SW_LED */
#endif
}

void
InitAdapterVariablesByPROM_8814AU(
		PADAPTER	Adapter
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);

	hal_InitPGData_8814A(Adapter, pHalData->efuse_eeprom_data);

	/* Hal_EfuseParseIDCode8812A(Adapter, pHalData->efuse_eeprom_data); */
	hal_ReadPROMVersion8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadIDs_8814AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_config_macaddr(Adapter, pHalData->bautoload_fail_flag);
	hal_ReadTxPowerInfo8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadBoardType8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_Read_TRX_antenna_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	/*  */
	/* Read Bluetooth co-exist and initialize */
	/*  */
	hal_EfuseParseBTCoexistInfo8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	hal_ReadChannelPlan8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_EfuseParseXtal_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadThermalMeter_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadRemoteWakeup_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadAntennaDiversity8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadRFEType_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadPowerTrackingType_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	ReadLEDSetting_8814AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	hal_ReadUsbModeSwitch_8814AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_CustomizeByCustomerID_8814AU(Adapter);

	hal_GetRxGainOffset_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	Hal_EfuseParseKFreeData_8814A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	/* set coex. ant info once efuse parsing is done */
	rtw_btcoex_set_ant_info(Adapter);
}

static void hal_ReadPROMContent_8814A(
		PADAPTER		Adapter
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
	u8			eeValue;

	/* check system boot selection */
	eeValue = rtw_read8(Adapter, REG_9346CR);
	pHalData->EepromOrEfuse		= (eeValue & BOOT_FROM_EEPROM) ? _TRUE : _FALSE;
	pHalData->bautoload_fail_flag	= (eeValue & EEPROM_EN) ? _FALSE : _TRUE;

	RTW_INFO("Boot from %s, Autoload %s !\n", (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
		 (pHalData->bautoload_fail_flag ? "Fail" : "OK"));

	/* pHalData->EEType = IS_BOOT_FROM_EEPROM(Adapter) ? EEPROM_93C46 : EEPROM_BOOT_EFUSE; */

	InitAdapterVariablesByPROM_8814AU(Adapter);
}

u8
ReadAdapterInfo8814AU(
		PADAPTER			Adapter
)
{
	Hal_InitEfuseVars_8814A(Adapter);

	/* Read all content in Efuse/EEPROM. */
	hal_ReadPROMContent_8814A(Adapter);

	/* We need to define the RF type after all PROM value is recognized. */
	ReadRFType8814A(Adapter);

	return _SUCCESS;
}

void UpdateInterruptMask8814AU(PADAPTER padapter, u8 bHIMR0 , u32 AddMSR, u32 RemoveMSR)
{
	HAL_DATA_TYPE *pHalData;

	u32 *himr;
	pHalData = GET_HAL_DATA(padapter);

	if (bHIMR0)
		himr = &(pHalData->IntrMask[0]);
	else
		himr = &(pHalData->IntrMask[1]);

	if (AddMSR)
		*himr |= AddMSR;

	if (RemoveMSR)
		*himr &= (~RemoveMSR);

	if (bHIMR0)
		rtw_write32(padapter, REG_HIMR0_8814A, *himr);
	else
		rtw_write32(padapter, REG_HIMR1_8814A, *himr);

}

u8 SetHwReg8814AU(PADAPTER Adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(Adapter);
	struct registry_priv *registry_par = &Adapter->registrypriv;
	u8 ret = _SUCCESS;

	switch (variable) {
	case HW_VAR_RXDMA_AGG_PG_TH:
#ifdef CONFIG_USB_RX_AGGREGATION
		{
			/*u8	threshold = *((u8 *)val);
			if( threshold == 0)
			{
				threshold = pHalData->UsbRxAggPageCount;
			}
			rtw_write8(Adapter, REG_RXDMA_AGG_PG_TH, threshold);*/
		}
#endif
		break;
	case HW_VAR_SET_RPWM:
#ifdef CONFIG_LPS_LCLK
		{
			u8	ps_state = *((u8 *)val);
			/* rpwm value only use BIT0(clock bit) ,BIT6(Ack bit), and BIT7(Toggle bit) for 88e. */
			/* BIT0 value - 1: 32k, 0:40MHz. */
			/* BIT6 value - 1: report cpwm value after success set, 0:do not report. */
			/* BIT7 value - Toggle bit change. */
			/* modify by Thomas. 2012/4/2. */
			ps_state = ps_state & 0xC1;
			/* RTW_INFO("##### Change RPWM value to = %x for switch clk #####\n",ps_state); */
			rtw_write8(Adapter, REG_USB_HRPWM, ps_state);
		}
#endif
#ifdef CONFIG_AP_WOWLAN
		if (pwrctl->wowlan_ap_mode == _TRUE) {
			u8	ps_state = *((u8 *)val);

			RTW_INFO("%s, RPWM\n", __func__);
			ps_state = ps_state & 0xC1;
			rtw_write8(Adapter, REG_USB_HRPWM, ps_state);
		}
#endif
		break;
	case HW_VAR_USB_MODE:
		/* U2 to U3 */
		if (registry_par->switch_usb_mode == 1) {
			if (IS_HIGH_SPEED_USB(Adapter)) {
				if ((rtw_read8(Adapter, 0x74) & (BIT(2) | BIT(3))) != BIT(3)) {
					rtw_write8(Adapter, 0x74, 0x8);
					rtw_write8(Adapter, 0x70, 0x2);
					rtw_write8(Adapter, 0x3e, 0x1);
					rtw_write8(Adapter, 0x3d, 0x3);
					/* usb disconnect */
					rtw_write8(Adapter, 0x5, 0x80);
					*val = _TRUE;
				}
			} else if (IS_SUPER_SPEED_USB(Adapter)) {
				rtw_write8(Adapter, 0x70, rtw_read8(Adapter, 0x70) & (~BIT(1)));
				rtw_write8(Adapter, 0x3e, rtw_read8(Adapter, 0x3e) & (~BIT(0)));
			}
		} else if (registry_par->switch_usb_mode == 2) {
			/* U3 to U2 */
			if (IS_SUPER_SPEED_USB(Adapter)) {
				if ((rtw_read8(Adapter, 0x74) & (BIT(2) | BIT(3))) != BIT(2)) {
					rtw_write8(Adapter, 0x74, 0x4);
					rtw_write8(Adapter, 0x70, 0x2);
					rtw_write8(Adapter, 0x3e, 0x1);
					rtw_write8(Adapter, 0x3d, 0x3);
					/* usb disconnect */
					rtw_write8(Adapter, 0x5, 0x80);
					*val = _TRUE;
				}
			} else if (IS_HIGH_SPEED_USB(Adapter)) {
				rtw_write8(Adapter, 0x70, rtw_read8(Adapter, 0x70) & (~BIT(1)));
				rtw_write8(Adapter, 0x3e, rtw_read8(Adapter, 0x3e) & (~BIT(0)));
			}
		}
		break;
	default:
		ret = SetHwReg8814A(Adapter, variable, val);
		break;
	}

	return ret;
}

void GetHwReg8814AU(PADAPTER Adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	switch (variable) {
	default:
		GetHwReg8814A(Adapter, variable, val);
		break;
	}

}

/*
 *	Description:
 *		Change default setting of specified variable.
 *   */
u8
SetHalDefVar8814AUsb(
		PADAPTER				Adapter,
		HAL_DEF_VARIABLE		eVariable,
		void						*pValue
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch (eVariable) {
	default:
		SetHalDefVar8814A(Adapter, eVariable, pValue);
		break;
	}

	return bResult;
}

/*
 *	Description:
 *		Query setting of specified variable.
 *   */
u8
GetHalDefVar8814AUsb(
		PADAPTER				Adapter,
		HAL_DEF_VARIABLE		eVariable,
		void						*pValue
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch (eVariable) {
	default:
		GetHalDefVar8814A(Adapter, eVariable, pValue);
		break;
	}

	return bResult;
}

static void rtl8814au_init_default_value(_adapter *padapter)
{
	PHAL_DATA_TYPE pHalData;

	pHalData = GET_HAL_DATA(padapter);

	InitDefaultValue8814A(padapter);

	pHalData->IntrMask[0]	= (u32)(\
					/* IMR_ROK 		| */
					/* IMR_RDU			| */
					/* IMR_VODOK		| */
					/* IMR_VIDOK		| */
					/* IMR_BEDOK		| */
					/* IMR_BKDOK		| */
					/* IMR_MGNTDOK		| */
					/* IMR_HIGHDOK		| */
					/* IMR_CPWM		| */
					/* IMR_CPWM2		| */
					/* IMR_C2HCMD		| */
					/* IMR_HISR1_IND_INT	| */
					/* IMR_ATIMEND		| */
					/* IMR_BCNDMAINT_E	| */
					/* IMR_HSISR_IND_ON_INT	| */
					/* IMR_BCNDOK0		| */
					/* IMR_BCNDMAINT0	| */
					/* IMR_TSF_BIT32_TOGGLE	| */
					/* IMR_TXBCN0OK	| */
					/* IMR_TXBCN0ERR	| */
					/* IMR_GTINT3		| */
					/* IMR_GTINT4		| */
					/* IMR_TXCCK		| */
					0);

	pHalData->IntrMask[1]	= (u32)(\
					/* IMR_RXFOVW		| */
					/* IMR_TXFOVW		| */
					/* IMR_RXERR		| */
					/* IMR_TXERR		| */
					/* IMR_ATIMEND_E	| */
					/* IMR_BCNDOK1		| */
					/* IMR_BCNDOK2		| */
					/* IMR_BCNDOK3		| */
					/* IMR_BCNDOK4		| */
					/* IMR_BCNDOK5		| */
					/* IMR_BCNDOK6		| */
					/* IMR_BCNDOK7		| */
					/* IMR_BCNDMAINT1	| */
					/* IMR_BCNDMAINT2	| */
					/* IMR_BCNDMAINT3	| */
					/* IMR_BCNDMAINT4	| */
					/* IMR_BCNDMAINT5	| */
					/* IMR_BCNDMAINT6	| */
					/* IMR_BCNDMAINT7	| */
					0);
}

static u8 rtl8814au_ps_func(PADAPTER Adapter, HAL_INTF_PS_FUNC efunc_id, u8 *val)
{
	u8 bResult = _TRUE;
	switch (efunc_id) {

#if defined(CONFIG_AUTOSUSPEND) && defined(SUPPORT_HW_RFOFF_DETECTED)
	case HAL_USB_SELECT_SUSPEND: {
		u8 bfwpoll = *((u8 *)val);
		/* rtl8188e_set_FwSelectSuspend_cmd(Adapter,bfwpoll ,500); */ /* note fw to support hw power down ping detect */
	}
	break;
#endif /* CONFIG_AUTOSUSPEND && SUPPORT_HW_RFOFF_DETECTED */

	default:
		break;
	}
	return bResult;
}

void rtl8814au_set_hal_ops(_adapter *padapter)
{
	struct hal_ops	*pHalFunc = &padapter->hal_func;


	pHalFunc->hal_power_on = _InitPowerOn_8814AU;
	pHalFunc->hal_power_off = hal_carddisable_8814;

	pHalFunc->hal_init = &rtl8814au_hal_init;
	pHalFunc->hal_deinit = &rtl8814au_hal_deinit;

	pHalFunc->inirp_init = &rtl8814au_inirp_init;
	pHalFunc->inirp_deinit = &rtl8814au_inirp_deinit;

	pHalFunc->init_xmit_priv = &rtl8814au_init_xmit_priv;
	pHalFunc->free_xmit_priv = &rtl8814au_free_xmit_priv;

	pHalFunc->init_recv_priv = &rtl8814au_init_recv_priv;
	pHalFunc->free_recv_priv = &rtl8814au_free_recv_priv;
#ifdef CONFIG_RTW_SW_LED
	pHalFunc->InitSwLeds = &rtl8814au_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8814au_DeInitSwLeds;
#endif/* CONFIG_RTW_SW_LED */

	pHalFunc->init_default_value = &rtl8814au_init_default_value;
	pHalFunc->intf_chip_configure = &rtl8814au_interface_configure;
	pHalFunc->read_adapter_info = &ReadAdapterInfo8814AU;

	pHalFunc->set_hw_reg_handler = &SetHwReg8814AU;
	pHalFunc->GetHwRegHandler = &GetHwReg8814AU;
	pHalFunc->get_hal_def_var_handler = &GetHalDefVar8814AUsb;
	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8814AUsb;


	pHalFunc->hal_xmit = &rtl8814au_hal_xmit;
	pHalFunc->mgnt_xmit = &rtl8814au_mgnt_xmit;
	pHalFunc->hal_xmitframe_enqueue = &rtl8814au_hal_xmitframe_enqueue;

#ifdef CONFIG_HOSTAPD_MLME
	pHalFunc->hostap_mgnt_xmit_entry = &rtl8812au_hostap_mgnt_xmit_entry;
#endif
	pHalFunc->interface_ps_func = &rtl8814au_ps_func;
#ifdef CONFIG_XMIT_THREAD_MODE
	pHalFunc->xmit_thread_handler = &rtl8814au_xmit_buf_handler;
#endif
#ifdef CONFIG_SUPPORT_USB_INT
	pHalFunc->interrupt_handler = interrupt_handler_8814au;
#endif
#ifdef CONFIG_FW_CORRECT_BCN
	pHalFunc->fw_correct_bcn = &rtl8814_fw_update_beacon_cmd;
#endif
	rtl8814_set_hal_ops(pHalFunc);

}
