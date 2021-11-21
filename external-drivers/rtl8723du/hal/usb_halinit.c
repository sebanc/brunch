// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _USB_HALINIT_C_

#include <rtl8723d_hal.h>

static void
_ConfigChipOutEP_8723(
	struct adapter * adapt,
	u8 NumOutPipe
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	pHalData->OutEpQueueSel = 0;
	pHalData->OutEpNumber = 0;

	switch (NumOutPipe) {
	case 4:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 4;
		break;
	case 3:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 3;
		break;
	case 2:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 2;
		break;
	case 1:
		pHalData->OutEpQueueSel = TX_SELE_HQ;
		pHalData->OutEpNumber = 1;
		break;
	default:
		break;
	}
}

static bool HalUsbSetQueuePipeMapping8723DUsb(
	struct adapter * adapt,
	u8 NumInPipe,
	u8 NumOutPipe
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	bool result = false;

	_ConfigChipOutEP_8723(adapt, NumOutPipe);

	result = Hal_MappingOutPipe(adapt, NumOutPipe);

	RTW_INFO("USB NumInPipe(%u), NumOutPipe(%u/%u)\n"
		 , NumInPipe
		 , pHalData->OutEpNumber
		 , NumOutPipe);

	return result;
}

static void rtl8723du_interface_configure(
	struct adapter * adapt
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(adapt);

	if (IS_HIGH_SPEED_USB(adapt)) {
		/* HIGH SPEED */
		pHalData->UsbBulkOutSize = USB_HIGH_SPEED_BULK_SIZE; /* 512 bytes */
	} else {
		/* FULL SPEED */
		pHalData->UsbBulkOutSize = USB_FULL_SPEED_BULK_SIZE; /* 64 bytes */
	}

	pHalData->interfaceIndex = pdvobjpriv->InterfaceNumber;

	HalUsbSetQueuePipeMapping8723DUsb(adapt,
			  pdvobjpriv->RtNumInPipes, pdvobjpriv->RtNumOutPipes);
}

static u32 _InitPowerOn_8723du(struct adapter * adapt)
{
	u32 status = _SUCCESS;
	u16 value16 = 0;
	u8 value8 = 0;

	rtw_hal_get_hwreg(adapt, HW_VAR_APFM_ON_MAC, &value8);
	if (value8)
		return _SUCCESS;
	/* HW Power on sequence */
	if (!HalPwrSeqCmdParsing(adapt, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, rtl8723D_card_enable_flow))
		return _FAIL;

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC block */
	/* Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31. */
	rtw_write16(adapt, REG_CR, 0x00);  /* suggseted by zhouzhou, by page, 20111230 */

	value16 = rtw_read16(adapt, REG_CR);
	value16 |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
		    | PROTOCOL_EN | SCHEDULE_EN | ENSEC | CALTMR_EN);
	rtw_write16(adapt, REG_CR, value16);

	value8 = true;
	rtw_hal_set_hwreg(adapt, HW_VAR_APFM_ON_MAC, &value8);

	return status;
}

/*
 * -------------------------------------------------------------------------
 * LLT R/W/Init function
 * -------------------------------------------------------------------------
 */
/*
 * ---------------------------------------------------------------
 * MAC init functions
 * ---------------------------------------------------------------
 */

/*
 * USB has no hardware interrupt,
 * so no need to initialize HIMR.
 */
static void _InitInterrupt(struct adapter * adapt)
{
}

static void _InitQueueReservedPage(struct adapter * adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	u32 numHQ = 0;
	u32 numLQ = 0;
	u32 numNQ = 0;
	u32 numPubQ;
	u32 value32;
	u8 value8;
	bool bWiFiConfig = pregistrypriv->wifi_spec;

	if (pHalData->OutEpQueueSel & TX_SELE_HQ)
		numHQ = bWiFiConfig ? WMM_NORMAL_PAGE_NUM_HPQ_8723D : NORMAL_PAGE_NUM_HPQ_8723D;

	if (pHalData->OutEpQueueSel & TX_SELE_LQ)
		numLQ = bWiFiConfig ? WMM_NORMAL_PAGE_NUM_LPQ_8723D : NORMAL_PAGE_NUM_LPQ_8723D;

	/* NOTE: This step shall be proceed before writing REG_RQPN. */
	if (pHalData->OutEpQueueSel & TX_SELE_NQ)
		numNQ = bWiFiConfig ? WMM_NORMAL_PAGE_NUM_NPQ_8723D : NORMAL_PAGE_NUM_NPQ_8723D;
	value8 = (u8)_NPQ(numNQ);
	rtw_write8(adapt, REG_RQPN_NPQ, value8);

	numPubQ = TX_TOTAL_PAGE_NUMBER_8723D - numHQ - numLQ - numNQ;

	/* TX DMA */
	value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;
	rtw_write32(adapt, REG_RQPN, value32);

}

static void _InitTRxBufferBoundary(struct adapter * adapt)
{
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
#ifdef CONFIG_CONCURRENT_MODE
	u8 val8;
#endif /* CONFIG_CONCURRENT_MODE */

	/* u16	txdmactrl; */
	u8 txpktbuf_bndy;

	if (!pregistrypriv->wifi_spec)
		txpktbuf_bndy = TX_PAGE_BOUNDARY_8723D;
	else {
		/* for WMM */
		txpktbuf_bndy = WMM_NORMAL_TX_PAGE_BOUNDARY_8723D;
	}

	rtw_write8(adapt, REG_TXPKTBUF_BCNQ_BDNY_8723D, txpktbuf_bndy);
	rtw_write8(adapt, REG_TXPKTBUF_MGQ_BDNY_8723D, txpktbuf_bndy);
	rtw_write8(adapt, REG_TXPKTBUF_WMAC_LBK_BF_HD_8723D, txpktbuf_bndy);
	rtw_write8(adapt, REG_TRXFF_BNDY, txpktbuf_bndy);
	rtw_write8(adapt, REG_TDECTRL + 1, txpktbuf_bndy);

	/* RX Page Boundary */
	rtw_write16(adapt, REG_TRXFF_BNDY + 2, RX_DMA_BOUNDARY_8723D);

#ifdef CONFIG_CONCURRENT_MODE
	val8 = txpktbuf_bndy + 8;
	rtw_write8(adapt, REG_BCNQ1_BDNY, val8);
	rtw_write8(adapt, REG_DWBCN1_CTRL_8723D + 1, val8); /* BCN1_HEAD */

	val8 = rtw_read8(adapt, REG_DWBCN1_CTRL_8723D + 2);
	val8 |= BIT(1); /* BIT1- BIT_SW_BCN_SEL_EN */
	rtw_write8(adapt, REG_DWBCN1_CTRL_8723D + 2, val8);
#endif /* CONFIG_CONCURRENT_MODE */
}


static void
_InitTransferPageSize_8723du(
	struct adapter * adapt
)
{

	u8 value8;

	value8 = _PSRX(PBP_256) | _PSTX(PBP_256);

	rtw_write8(adapt, REG_PBP, value8);
}


static void
_InitNormalChipRegPriority(
	struct adapter * adapt,
	u16 beQ,
	u16 bkQ,
	u16 viQ,
	u16 voQ,
	u16 mgtQ,
	u16 hiQ
)
{
	u16 value16 = (rtw_read16(adapt, REG_TRXDMA_CTRL) & 0x7);

	value16 |= _TXDMA_BEQ_MAP(beQ) | _TXDMA_BKQ_MAP(bkQ) |
		   _TXDMA_VIQ_MAP(viQ) | _TXDMA_VOQ_MAP(voQ) |
		   _TXDMA_MGQ_MAP(mgtQ) | _TXDMA_HIQ_MAP(hiQ);

	rtw_write16(adapt, REG_TRXDMA_CTRL, value16);
}


static void
_InitNormalChipTwoOutEpPriority(
	struct adapter * adapt
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	u16 beQ, bkQ, viQ, voQ, mgtQ, hiQ;

	u16 valueHi = 0;
	u16 valueLow = 0;

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
		/* RT_ASSERT(false,("Shall not reach here!\n")); */
		break;
	}
	if (!pregistrypriv->wifi_spec) {
		beQ = valueLow;
		bkQ = valueLow;
		viQ = valueHi;
		voQ = valueHi;
		mgtQ = valueHi;
		hiQ = valueHi;
	} else { /* for WMM */
		beQ = valueLow;
		bkQ = valueHi;
		viQ = valueHi;
		voQ = valueLow;
		mgtQ = valueHi;
		hiQ = valueHi;
	}

	_InitNormalChipRegPriority(adapt, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static void
_InitNormalChipThreeOutEpPriority(
	struct adapter * adapt
)
{
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	u16 beQ, bkQ, viQ, voQ, mgtQ, hiQ;

	if (!pregistrypriv->wifi_spec) { /* typical setting */
		beQ = QUEUE_LOW;
		bkQ = QUEUE_LOW;
		viQ = QUEUE_NORMAL;
		voQ = QUEUE_HIGH;
		mgtQ = QUEUE_HIGH;
		hiQ = QUEUE_HIGH;
	} else { /* for WMM */
		beQ = QUEUE_LOW;
		bkQ = QUEUE_NORMAL;
		viQ = QUEUE_NORMAL;
		voQ = QUEUE_HIGH;
		mgtQ = QUEUE_HIGH;
		hiQ = QUEUE_HIGH;
	}
	_InitNormalChipRegPriority(adapt, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static void _InitQueuePriority(struct adapter * adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	switch (pHalData->OutEpNumber) {
	case 2:
		_InitNormalChipTwoOutEpPriority(adapt);
		break;
	case 3:
	case 4:
		_InitNormalChipThreeOutEpPriority(adapt);
		break;
	default:
		/* RT_ASSERT(false,("Shall not reach here!\n")); */
		break;
	}

}

static void
_InitHardwareDropIncorrectBulkOut(
	struct adapter * adapt
)
{
	u32 value32 = rtw_read32(adapt, REG_TXDMA_OFFSET_CHK);

	value32 |= DROP_DATA_EN;

	rtw_write32(adapt, REG_TXDMA_OFFSET_CHK, value32);
}

static void
_InitNetworkType(
	struct adapter * adapt
)
{
	u32 value32;

	value32 = rtw_read32(adapt, REG_CR);

	/* TODO: use the other function to set network type */
	value32 = (value32 & ~MASK_NETTYPE) | _NETTYPE(NT_LINK_AP);
	rtw_write32(adapt, REG_CR, value32);
}

static void
_InitDriverInfoSize(
	struct adapter * adapt,
	u8 drvInfoSize
)
{
	u8 value8;

	/* BIT_DRVINFO_SZ [3:0] */
	value8 = rtw_read8(adapt, REG_RX_DRVINFO_SZ) & 0xF8;
	value8 |= drvInfoSize;
	rtw_write8(adapt, REG_RX_DRVINFO_SZ, value8);
}

static void
_InitWMACSetting(
	struct adapter * adapt
)
{
	u16 value16;
	u32 rcr;

	rcr = RCR_APM | RCR_AM | RCR_AB | RCR_CBSSID_DATA | RCR_CBSSID_BCN | RCR_APP_ICV | RCR_AMF | RCR_HTC_LOC_CTRL | RCR_APP_MIC | RCR_APP_PHYST_RXFF;
	rtw_hal_set_hwreg(adapt, HW_VAR_RCR, (u8 *)&rcr);

	/* Accept all data frames */
	value16 = 0xFFFF;
	rtw_write16(adapt, REG_RXFLTMAP2_8723D, value16);

	/* 2010.09.08 hpfan */
	/* Since ADF is removed from RCR, ps-poll will not be indicate to driver, */
	/* RxFilterMap should mask ps-poll to gurantee AP mode can rx ps-poll. */

	value16 = 0x400;
	rtw_write16(adapt, REG_RXFLTMAP1_8723D, value16);

	/* Accept all management frames */
	value16 = 0xFFFF;
	rtw_write16(adapt, REG_RXFLTMAP0_8723D, value16);

}

static void
_InitAdaptiveCtrl(
	struct adapter * adapt
)
{
	u16 value16;
	u32 value32;

	/* Response Rate Set */
	value32 = rtw_read32(adapt, REG_RRSR);
	value32 &= ~RATE_BITMAP_ALL;
	value32 |= RATE_RRSR_CCK_ONLY_1M;
	rtw_write32(adapt, REG_RRSR, value32);

	/* CF-END Threshold */
	/* m_spIoBase->rtw_write8(REG_CFEND_TH, 0x1); */

	/* SIFS (used in NAV) */
	value16 = _SPEC_SIFS_CCK(0x10) | _SPEC_SIFS_OFDM(0x10);
	rtw_write16(adapt, REG_SPEC_SIFS, value16);

	/* Retry Limit */
	value16 = _LRL(RL_VAL_STA) | _SRL(RL_VAL_STA);
	rtw_write16(adapt, REG_RL, value16);

}

static void
_InitEDCA(
	struct adapter * adapt
)
{
	/* Set Spec SIFS (used in NAV) */
	rtw_write16(adapt, REG_SPEC_SIFS, 0x100a);
	rtw_write16(adapt, REG_MAC_SPEC_SIFS, 0x100a);

	/* Set SIFS for CCK */
	rtw_write16(adapt, REG_SIFS_CTX, 0x100a);

	/* Set SIFS for OFDM */
	rtw_write16(adapt, REG_SIFS_TRX, 0x100a);

	/* TXOP */
	rtw_write32(adapt, REG_EDCA_BE_PARAM, 0x005EA42B);
	rtw_write32(adapt, REG_EDCA_BK_PARAM, 0x0000A44F);
	rtw_write32(adapt, REG_EDCA_VI_PARAM, 0x005EA324);
	rtw_write32(adapt, REG_EDCA_VO_PARAM, 0x002FA226);

}

static void _InitHWLed(struct adapter * adapt)
{
	struct led_priv *pledpriv = &(adapt->ledpriv);

	if (pledpriv->LedStrategy != HW_LED)
		return;

	/* HW led control */
	/* to do .... */
	/* must consider cases of antenna diversity/ commbo card/solo card/mini card */

}

static void
_InitRDGSetting_8723du(
	struct adapter * adapt
)
{
	rtw_write8(adapt, REG_RD_CTRL_8723D, 0xFF);
	rtw_write16(adapt, REG_RD_NAV_NXT_8723D, 0x200);
	rtw_write8(adapt, REG_RD_RESP_PKT_TH_8723D, 0x05);
}

static void
_InitRetryFunction(
	struct adapter * adapt
)
{
	u8 value8;

	value8 = rtw_read8(adapt, REG_FWHW_TXQ_CTRL);
	value8 |= EN_AMPDU_RTY_NEW;
	rtw_write8(adapt, REG_FWHW_TXQ_CTRL, value8);

	/* Set ACK timeout */
	rtw_write8(adapt, REG_ACKTO, 0x40);
}

static void _InitBurstPktLen(struct adapter * adapt)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	u8 tmp8;


	tmp8 = rtw_read8(adapt, REG_RXDMA_MODE_CTRL_8723D);
	tmp8 &= ~(BIT(4) | BIT(5));
	switch (pHalData->UsbBulkOutSize) {
	case USB_HIGH_SPEED_BULK_SIZE:
		tmp8 |= BIT(4); /* set burst pkt len=512B */
		break;
	case USB_FULL_SPEED_BULK_SIZE:
	default:
		tmp8 |= BIT(5); /* set burst pkt len=64B */
		break;
	}
	tmp8 |= BIT(1) | BIT(2) | BIT(3);
	rtw_write8(adapt, REG_RXDMA_MODE_CTRL_8723D, tmp8);

	pHalData->bSupportUSB3 = false;

	tmp8 = rtw_read8(adapt, REG_HT_SINGLE_AMPDU_8723D);
	tmp8 |= BIT(7); /* enable single pkt ampdu */
	rtw_write8(adapt, REG_HT_SINGLE_AMPDU_8723D, tmp8);
	rtw_write16(adapt, REG_MAX_AGGR_NUM, 0x0C14);
	rtw_write8(adapt, REG_AMPDU_MAX_TIME_8723D, 0x5E);
	rtw_write32(adapt, REG_AMPDU_MAX_LENGTH_8723D, 0xffffffff);
	if (pHalData->AMPDUBurstMode)
		rtw_write8(adapt, REG_AMPDU_BURST_MODE_8723D, 0x5F);

	/* for VHT packet length 11K */
	rtw_write8(adapt, REG_RX_PKT_LIMIT, 0x18);

	rtw_write8(adapt, REG_PIFS, 0x00);
	rtw_write8(adapt, REG_FWHW_TXQ_CTRL, 0x80);
	rtw_write32(adapt, REG_FAST_EDCA_CTRL, 0x03086666);

	/* to prevent mac is reseted by bus. 20111208, by Page */
	tmp8 = rtw_read8(adapt, REG_RSV_CTRL);
	tmp8 |= BIT(5) | BIT(6);
	rtw_write8(adapt, REG_RSV_CTRL, tmp8);
}

/*-----------------------------------------------------------------------------
 * Function:	usb_AggSettingTxUpdate()
 *
 * Overview:	Separate TX/RX parameters update independent for TP detection and
 *			dynamic TX/RX aggreagtion parameters update.
 *
 * Input:			struct adapter *
 *
 * Output/Return:	NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	12/10/2010	MHC		Separate to smaller function.
 *
 *---------------------------------------------------------------------------*/
static void
usb_AggSettingTxUpdate(
	struct adapter * adapt
)
{
}   /* usb_AggSettingTxUpdate */


/*-----------------------------------------------------------------------------
 * Function:	usb_AggSettingRxUpdate()
 *
 * Overview:	Separate TX/RX parameters update independent for TP detection and
 *			dynamic TX/RX aggreagtion parameters update.
 *
 * Input:			struct adapter *
 *
 * Output/Return:	NONE
 *
 *---------------------------------------------------------------------------*/
static void
usb_AggSettingRxUpdate(struct adapter * adapt)
{
	struct hal_com_data * pHalData;
	u8 aggctrl;
	u32 aggrx;

	pHalData = GET_HAL_DATA(adapt);

	aggctrl = rtw_read8(adapt, REG_TRXDMA_CTRL);
	aggctrl &= ~RXDMA_AGG_EN;

	aggrx = rtw_read32(adapt, REG_RXDMA_AGG_PG_TH);
	aggrx &= ~BIT_USB_RXDMA_AGG_EN;
	aggrx &= ~0xFF0F; /* reset agg size and timeout */

	rtw_write8(adapt, REG_TRXDMA_CTRL, aggctrl);
	rtw_write32(adapt, REG_RXDMA_AGG_PG_TH, aggrx);
}

static void _initUsbAggregationSetting(struct adapter * adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	/* Tx aggregation setting */
	usb_AggSettingTxUpdate(adapt);

	/* Rx aggregation setting */
	usb_AggSettingRxUpdate(adapt);

	/* 201/12/10 MH Add for USB agg mode dynamic switch. */
	pHalData->UsbRxHighSpeedMode = false;
}

static void PHY_InitAntennaSelection8723D(struct adapter * adapt)
{
}

static void _InitRFType(struct adapter * Adapter)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

#if	DISABLE_BB_RF
	pHalData->rf_chip = RF_PSEUDO_11N;
	pHalData->rf_type = RF_1T1R;
	return;
#endif

	pHalData->rf_chip = RF_6052;
	pHalData->rf_type = RF_1T1R;

	RTW_INFO("Set RF Chip ID to RF_6052 and RF type to %d.\n", pHalData->rf_type);
}

/* Set CCK and OFDM Block "ON" */
static void _BBTurnOnBlock(struct adapter * adapt)
{
#if (DISABLE_BB_RF)
	return;
#endif

	phy_set_bb_reg(adapt, rFPGA0_RFMOD, bCCKEn, 0x1);
	phy_set_bb_reg(adapt, rFPGA0_RFMOD, bOFDMEn, 0x1);
}

#define MgntActSet_RF_State(...)

enum {
	Antenna_Lfet = 1,
	Antenna_Right = 2,
};

/* 2010/08/09 MH Add for power down check. */
static bool
HalDetectPwrDownMode(
	struct adapter * adapt
)
{
	u8 tmpvalue;
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	EFUSE_ShadowRead(adapt, 1, EEPROM_FEATURE_OPTION_8723D, (u32 *)&tmpvalue);

	if (tmpvalue & BIT(4) && pwrctrlpriv->reg_pdnmode)
		pHalData->pwrdown = true;
	else
		pHalData->pwrdown = false;

	RTW_INFO("%s(): PDN=%d\n", __func__, pHalData->pwrdown);
	return pHalData->pwrdown;
}   /* HalDetectPwrDownMode */


/*
 * 2010/08/26 MH Add for selective suspend mode check.
 * If Efuse 0x0e bit1 is not enabled, we can not support selective suspend for Minicard and
 * slim card.
 */
/*-----------------------------------------------------------------------------
 * Function:	HwSuspendModeEnable()
 *
 * Overview:	HW suspend mode switch.
 *
 * Input:		NONE
 *
 * Output:	NONE
 *
 * Return:	NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	08/23/2010	MHC		HW suspend mode switch test..
 *---------------------------------------------------------------------------*/
static void _InitBBRegBackup_8723du(struct adapter * adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	/* For Channel 1~11 (Default Value)*/
	pHalData->RegForRecover[0].offset = rCCK0_TxFilter2;
	pHalData->RegForRecover[0].value =
		phy_query_bb_reg(adapt,
			       pHalData->RegForRecover[0].offset, bMaskDWord);

	pHalData->RegForRecover[1].offset = rCCK0_DebugPort;
	pHalData->RegForRecover[1].value =
		phy_query_bb_reg(adapt,
			       pHalData->RegForRecover[1].offset, bMaskDWord);

	pHalData->RegForRecover[2].offset = 0xAAC;
	pHalData->RegForRecover[2].value =
		phy_query_bb_reg(adapt,
			       pHalData->RegForRecover[2].offset, bMaskDWord);
}

static u32 rtl8723du_hal_init(struct adapter * adapt)
{
	u8 value8 = 0, u1bRegCR;
	u32 status = _SUCCESS;
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	u32 NavUpper = WiFiNavUpperUs;
	unsigned long init_start_time = rtw_get_current_time();


	/* if (rtw_is_surprise_removed(adapt)) */
	/*	return RT_STATUS_FAILURE; */

	/* Check if MAC has already power on. */
	/* set usb timeout to fix read mac register fail before power on */
	rtw_write8(adapt, 0xFE4C /*REG_USB_ACCESS_TIMEOUT*/, 0x80);
	value8 = rtw_read8(adapt, REG_SYS_CLKR_8723D + 1);
	u1bRegCR = rtw_read8(adapt, REG_CR);
	RTW_INFO(" power-on :REG_SYS_CLKR 0x09=0x%02x. REG_CR 0x100=0x%02x.\n", value8, u1bRegCR);
	if ((value8 & BIT(3)) && (u1bRegCR != 0 && u1bRegCR != 0xEA))
		RTW_INFO(" MAC has already power on.\n");
	else {
		/* Set FwPSState to ALL_ON mode to prevent from the I/O be return because of 32k */
		/* state which is set before sleep under wowlan mode. 2012.01.04. by tynli. */
		RTW_INFO(" MAC has not been powered on yet.\n");
	}

	status = rtw_hal_power_on(adapt);
	if (status == _FAIL) {
		goto exit;
	}

	status = rtl8723d_InitLLTTable(adapt);
	if (status == _FAIL) {
		goto exit;
	}

	if (pHalData->bRDGEnable)
		_InitRDGSetting_8723du(adapt);


	/* Enable TX Report */
	/* Enable Tx Report Timer */
	value8 = rtw_read8(adapt, REG_TX_RPT_CTRL);
	rtw_write8(adapt, REG_TX_RPT_CTRL, value8 | BIT(1));
	/* Set MAX RPT MACID */
	rtw_write8(adapt, REG_TX_RPT_CTRL + 1, 2);
	/* Tx RPT Timer. Unit: 32us */
	rtw_write16(adapt, REG_TX_RPT_TIME, 0xCdf0);
	rtw_write8(adapt, REG_EARLY_MODE_CONTROL_8723D, 0);

	if (adapt->registrypriv.mp_mode == 0) {
		status = rtl8723d_FirmwareDownload(adapt, false);
		if (status != _SUCCESS) {
			pHalData->bFWReady = false;
			pHalData->fw_ractrl = false;
			goto exit;
		} else {
			pHalData->bFWReady = true;
			pHalData->fw_ractrl = true;
		}
	}

	if (pwrctrlpriv->reg_rfoff)
		pwrctrlpriv->rf_pwrstate = rf_off;

	/* Set RF type for BB/RF configuration */
	_InitRFType(adapt);

	HalDetectPwrDownMode(adapt);

	/* We should call the function before MAC/BB configuration. */
	PHY_InitAntennaSelection8723D(adapt);


#if (DISABLE_BB_RF == 1)
	/* fpga verification to open phy */
	rtw_write8(adapt, REG_SYS_FUNC_EN_8723D, FEN_USBA | FEN_USBD | FEN_BB_GLB_RSTn | FEN_BBRSTB);
#endif

#if (HAL_MAC_ENABLE == 1)
	status = PHY_MACConfig8723D(adapt);
	if (status == _FAIL) {
		RTW_INFO("PHY_MACConfig8723D fault !!\n");
		goto exit;
	}
#endif

	/* d. Initialize BB related configurations. */
#if (HAL_BB_ENABLE == 1)
	status = PHY_BBConfig8723D(adapt);
	if (status == _FAIL) {
		RTW_INFO("PHY_BBConfig8723D fault !!\n");
		goto exit;
	}
#endif

#if (HAL_RF_ENABLE == 1)
	status = PHY_RFConfig8723D(adapt);

	if (status == _FAIL) {
		RTW_INFO("PHY_RFConfig8723D fault !!\n");
		goto exit;
	}
	/*---- Set CCK and OFDM Block "ON"----*/
	phy_set_bb_reg(adapt, rFPGA0_RFMOD, bCCKEn, 0x1);
	phy_set_bb_reg(adapt, rFPGA0_RFMOD, bOFDMEn, 0x1);
#endif

	_InitBBRegBackup_8723du(adapt);

	_InitMacAPLLSetting_8723D(adapt);

	_InitQueueReservedPage(adapt);
	_InitTRxBufferBoundary(adapt);
	_InitQueuePriority(adapt);
	_InitTransferPageSize_8723du(adapt);


	/* Get Rx PHY status in order to report RSSI and others. */
	_InitDriverInfoSize(adapt, DRVINFO_SZ);

	_InitInterrupt(adapt);
	_InitNetworkType(adapt); /* set msr */
	_InitWMACSetting(adapt);
	_InitAdaptiveCtrl(adapt);
	_InitEDCA(adapt);
	_InitRetryFunction(adapt);
	/* _InitOperationMode(adapt);*/ /*todo */
	rtl8723d_InitBeaconParameters(adapt);
	rtl8723d_InitBeaconMaxError(adapt, true);

	_InitBurstPktLen(adapt);
	_initUsbAggregationSetting(adapt);

	_InitHardwareDropIncorrectBulkOut(adapt);

	rtw_write16(adapt, REG_PKT_VO_VI_LIFE_TIME, 0x0400); /* unit: 256us. 256ms */
	rtw_write16(adapt, REG_PKT_BE_BK_LIFE_TIME, 0x0400); /* unit: 256us. 256ms */
	_InitHWLed(adapt);

	_BBTurnOnBlock(adapt);

	rtw_hal_set_chnl_bw(adapt, adapt->registrypriv.channel,
		CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE, HAL_PRIME_CHNL_OFFSET_DONT_CARE);

	invalidate_cam_all(adapt);

	rtl8723d_InitAntenna_Selection(adapt);

	/* HW SEQ CTRL */
	/* set 0x0 to 0xFF by tynli. Default enable HW SEQ NUM. */
	rtw_write8(adapt, REG_HWSEQ_CTRL, 0xFF);

	/*
	 * Disable BAR, suggested by Scott
	 * 2010.04.09 add by hpfan
	 */
	rtw_write32(adapt, REG_BAR_MODE_CTRL, 0x0201ffff);

	if (pregistrypriv->wifi_spec)
		rtw_write16(adapt, REG_FAST_EDCA_CTRL, 0);

	rtl8723d_InitHalDm(adapt);

#if (MP_DRIVER == 1)
	if (adapt->registrypriv.mp_mode == 1) {
		adapt->mppriv.channel = pHalData->current_channel;
		MPT_InitializeAdapter(adapt, adapt->mppriv.channel);
	} else
#endif
	{
		pwrctrlpriv->rf_pwrstate = rf_on;

		if (pwrctrlpriv->rf_pwrstate == rf_on) {
			struct pwrctrl_priv *pwrpriv;
			unsigned long start_time;
			u8 restore_iqk_rst;
			u8 b2Ant;
			u8 h2cCmdBuf;

			pwrpriv = adapter_to_pwrctl(adapt);

			/*phy_lc_calibrate_8723d(&pHalData->odmpriv);*/
			halrf_lck_trigger(&pHalData->odmpriv);

			/* Inform WiFi FW that it is the beginning of IQK */
			h2cCmdBuf = 1;
			FillH2CCmd8723D(adapt, H2C_8723D_BT_WLAN_CALIBRATION, 1, &h2cCmdBuf);

			start_time = rtw_get_current_time();
			do {
				if (rtw_read8(adapt, 0x1e7) & 0x01)
					break;

				rtw_msleep_os(50);
			} while (rtw_get_passing_time_ms(start_time) <= 400);

			rtw_btcoex_IQKNotify(adapt, true);
			restore_iqk_rst = (pwrpriv->bips_processing) ? true : false;
			b2Ant = pHalData->EEPROMBluetoothAntNum == Ant_x2 ? true : false;
			halrf_iqk_trigger(&pHalData->odmpriv, false);
			/*phy_iq_calibrate_8723d(adapt, false);*/
			pHalData->bIQKInitialized = true;
			rtw_btcoex_IQKNotify(adapt, false);

			/* Inform WiFi FW that it is the finish of IQK */
			h2cCmdBuf = 0;
			FillH2CCmd8723D(adapt, H2C_8723D_BT_WLAN_CALIBRATION, 1, &h2cCmdBuf);

			odm_txpowertracking_check(&pHalData->odmpriv);
		}
	}

	/* Init BT hw config. */
	if (adapt->registrypriv.mp_mode == 1)
		rtw_btcoex_HAL_Initialize(adapt, true);
	else
		rtw_btcoex_HAL_Initialize(adapt, false);

	rtw_hal_set_hwreg(adapt, HW_VAR_NAV_UPPER, (u8 *)&NavUpper);

	/* ack for xmit mgmt frames. */
	rtw_write32(adapt, REG_FWHW_TXQ_CTRL, rtw_read32(adapt, REG_FWHW_TXQ_CTRL) | BIT(12));

	phy_set_bb_reg(adapt, rOFDM0_XAAGCCore1, bMaskByte0, 0x50);
	phy_set_bb_reg(adapt, rOFDM0_XAAGCCore1, bMaskByte0, 0x20);
	/* Enable MACTXEN/MACRXEN block */
	u1bRegCR = rtw_read8(adapt, REG_CR);
	u1bRegCR |= (MACTXEN | MACRXEN);
	rtw_write8(adapt, REG_CR, u1bRegCR);

	if (adapt->registrypriv.wifi_spec == 1)
		phy_set_bb_reg(adapt, rOFDM0_ECCAThreshold,
			       0x00ff00ff, 0x00250029);

exit:

	RTW_INFO("%s in %dms\n", __func__, rtw_get_passing_time_ms(init_start_time));


	return status;
}

static void rtl8723du_hw_power_down(struct adapter * adapt)
{
	u8 u1bTmp;

	RTW_INFO("PowerDownRTL8723U\n");


	/* 1. Run Card Disable Flow */
	/* Done before this function call. */

	/* 2. 0x04[16] = 0 */   /* reset WLON */
	u1bTmp = rtw_read8(adapt, REG_APS_FSMCO + 2);
	rtw_write8(adapt, REG_APS_FSMCO + 2, (u1bTmp & (~BIT(0))));

	/* 3. 0x04[12:11] = 2b'11 */ /* enable suspend */
	/* Done before this function call. */

	/* 4. 0x04[15] = 1 */ /* enable PDN */
	u1bTmp = rtw_read8(adapt, REG_APS_FSMCO + 1);
	rtw_write8(adapt, REG_APS_FSMCO + 1, (u1bTmp | BIT(7)));
}

/*
 * Description: RTL8723e card disable power sequence v003 which suggested by Scott.
 * First created by tynli. 2011.01.28.
 */
static void
CardDisableRTL8723du(
	struct adapter * adapt
)
{
	u8 u1bTmp;

	rtw_hal_get_hwreg(adapt, HW_VAR_APFM_ON_MAC, &u1bTmp);
	RTW_INFO(FUNC_ADPT_FMT ": bMacPwrCtrlOn=%d\n", FUNC_ADPT_ARG(adapt), u1bTmp);
	if (!u1bTmp)
		return;
	u1bTmp = false;
	rtw_hal_set_hwreg(adapt, HW_VAR_APFM_ON_MAC, &u1bTmp);

	/* Stop Tx Report Timer. 0x4EC[Bit1]=b'0 */
	u1bTmp = rtw_read8(adapt, REG_TX_RPT_CTRL);
	rtw_write8(adapt, REG_TX_RPT_CTRL, u1bTmp & (~BIT(1)));

	/* stop rx */
	rtw_write8(adapt, REG_CR, 0x0);

	/* 1. Run LPS WL RFOFF flow */
	HalPwrSeqCmdParsing(adapt, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, rtl8723D_enter_lps_flow);

	if ((rtw_read8(adapt, REG_MCUFWDL_8723D) & BIT(7)) &&
	    GET_HAL_DATA(adapt)->bFWReady) /*8051 RAM code */
		rtl8723d_FirmwareSelfReset(adapt);

	/* Reset MCU. Suggested by Filen. 2011.01.26. by tynli. */
	u1bTmp = rtw_read8(adapt, REG_SYS_FUNC_EN_8723D + 1);
	rtw_write8(adapt, REG_SYS_FUNC_EN_8723D + 1, (u1bTmp & (~BIT(2))));

	/* MCUFWDL 0x80[1:0]=0 */   /* reset MCU ready status */
	rtw_write8(adapt, REG_MCUFWDL_8723D, 0x00);

	/* Card disable power action flow */
	HalPwrSeqCmdParsing(adapt, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, rtl8723D_card_disable_flow);

	GET_HAL_DATA(adapt)->bFWReady = false;
}


static u32 rtl8723du_hal_deinit(struct adapter * adapt)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);

	RTW_INFO("==> %s\n", __func__);

	rtw_write16(adapt, REG_GPIO_MUXCFG, rtw_read16(adapt, REG_GPIO_MUXCFG) & (~BIT(12)));

	rtw_write32(adapt, REG_HISR0_8723D, 0xFFFFFFFF);
	rtw_write32(adapt, REG_HISR1_8723D, 0xFFFFFFFF);
#ifdef SUPPORT_HW_RFOFF_DETECTED
	RTW_INFO("%s: bkeepfwalive(%x)\n", __func__, pwrctl->bkeepfwalive);

	if (pwrctl->bkeepfwalive) {
		_ps_close_RF(Adapter);
		if ((pwrctl->bHWPwrPindetect) && (pwrctl->bHWPowerdown))
			rtl8723du_hw_power_down(adapt);
	} else
#endif
	{
		if (rtw_is_hw_init_completed(adapt)) {
			rtw_hal_power_off(adapt);

			if ((pwrctl->bHWPwrPindetect) && (pwrctl->bHWPowerdown))
				rtl8723du_hw_power_down(adapt);
		}
		pHalData->bMacPwrCtrlOn = false;
	}
	return _SUCCESS;
}


static unsigned int rtl8723du_inirp_init(struct adapter * adapt)
{
	u8 i;
	struct recv_buf *precvbuf;
	uint status;
	struct intf_hdl *pintfhdl = &adapt->iopriv.intf;
	struct recv_priv *precvpriv = &(adapt->recvpriv);
	u32(*_read_port)(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *pmem);

	_read_port = pintfhdl->io_ops._read_port;

	status = _SUCCESS;


	precvpriv->ff_hwaddr = RECV_BULK_IN_ADDR;

	/* issue Rx irp to receive data */
	precvbuf = (struct recv_buf *)precvpriv->precv_buf;
	for (i = 0; i < NR_RECVBUFF; i++) {
		if (!_read_port(pintfhdl, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf)) {
			status = _FAIL;
			goto exit;
		}

		precvbuf++;
		precvpriv->free_recv_buf_queue_cnt--;
	}

exit:



	return status;

}

static unsigned int rtl8723du_inirp_deinit(struct adapter * adapt)
{
	rtw_read_port_cancel(adapt);
	return _SUCCESS;
}

/*
 *-------------------------------------------------------------------
 *	EEPROM/EFUSE Content Parsing
 *-------------------------------------------------------------------
 */
static void
hal_EfuseParseLEDSetting(
	struct adapter * adapt,
	u8 *PROMContent,
	bool AutoloadFail
)
{
}

/* Read HW power down mode selection */
static void
hal_EfuseParsePowerSavingSetting(
	struct adapter * adapt,
	u8 *PROMContent,
	u8 AutoloadFail
)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);

	if (AutoloadFail) {
		pwrctl->bHWPowerdown = false;
		pwrctl->bSupportRemoteWakeup = false;
	} else {
		/*if(SUPPORT_HW_RADIO_DETECT(adapt)) */
		pwrctl->bHWPwrPindetect = adapt->registrypriv.hwpwrp_detect;
		/*else */
		/*pwrctl->bHWPwrPindetect = false; */ /*dongle not support new */


		/*hw power down mode selection , 0:rf-off / 1:power down */

		if (adapt->registrypriv.hwpdn_mode == 2)
			pwrctl->bHWPowerdown = (PROMContent[EEPROM_FEATURE_OPTION_8723D] & BIT(4));
		else
			pwrctl->bHWPowerdown = adapt->registrypriv.hwpdn_mode;

		/* decide hw if support remote wakeup function */
		/* if hw supported, 8051 (SIE) will generate WeakUP signal( D+/D- toggle) when autoresume */
		pwrctl->bSupportRemoteWakeup = (PROMContent[EEPROM_USB_OPTIONAL_FUNCTION0] & BIT(1)) ? true : false;

		/*if(SUPPORT_HW_RADIO_DETECT(adapt)) */
		/*adapt->registrypriv.usbss_enable = pwrctl->bSupportRemoteWakeup ; */

		RTW_INFO("%s...bHWPwrPindetect(%x)-bHWPowerdown(%x) ,bSupportRemoteWakeup(%x)\n", __func__,
			pwrctl->bHWPwrPindetect, pwrctl->bHWPowerdown, pwrctl->bSupportRemoteWakeup);

		RTW_INFO("### PS params=>  power_mgnt(%x),usbss_enable(%x) ###\n", adapt->registrypriv.power_mgnt, adapt->registrypriv.usbss_enable);

	}
}

static void
hal_EfuseParseIDs(
	struct adapter * adapt,
	u8 *hwinfo,
	bool AutoLoadFail
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	if (!AutoLoadFail) {
		/* VID, PID */
		pHalData->EEPROMVID = ReadLE2Byte(&hwinfo[EEPROM_VID_8723DU]);
		pHalData->EEPROMPID = ReadLE2Byte(&hwinfo[EEPROM_PID_8723DU]);

		/* Customer ID, 0x00 and 0xff are reserved for Realtek. */
		pHalData->EEPROMCustomerID = *(u8 *)&hwinfo[EEPROM_CustomID_8723D];
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;
	} else {
		pHalData->EEPROMVID = EEPROM_Default_VID;
		pHalData->EEPROMPID = EEPROM_Default_PID;

		/* Customer ID, 0x00 and 0xff are reserved for Realtek. */
		pHalData->EEPROMCustomerID = EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;
	}

	if ((pHalData->EEPROMVID == EEPROM_Default_VID)
	    && (pHalData->EEPROMPID == EEPROM_Default_PID)) {
		pHalData->CustomerID = EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;
	}

	RTW_INFO("VID = 0x%04X, PID = 0x%04X\n", pHalData->EEPROMVID, pHalData->EEPROMPID);
	RTW_INFO("Customer ID: 0x%02X, SubCustomer ID: 0x%02X\n", pHalData->EEPROMCustomerID, pHalData->EEPROMSubCustomerID);
}

static u8
InitadaptVariablesByPROM_8723du(
	struct adapter * adapt
)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	u8 *hwinfo = NULL;
	u8 ret = _FAIL;

	if (sizeof(pHalData->efuse_eeprom_data) < HWSET_MAX_SIZE_8723D)
		RTW_INFO("[WARNING] size of efuse_eeprom_data is less than HWSET_MAX_SIZE_8723D!\n");

	hwinfo = pHalData->efuse_eeprom_data;

	Hal_InitPGData(adapt, hwinfo);
	Hal_EfuseParseIDCode(adapt, hwinfo);
	Hal_EfuseParseEEPROMVer_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);

	hal_EfuseParseIDs(adapt, hwinfo, pHalData->bautoload_fail_flag);
	hal_config_macaddr(adapt, pHalData->bautoload_fail_flag);
	hal_EfuseParsePowerSavingSetting(adapt, hwinfo, pHalData->bautoload_fail_flag);

	Hal_EfuseParseTxPowerInfo_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);
	Hal_EfuseParseBoardType_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);

	Hal_EfuseParseBTCoexistInfo_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);

	Hal_EfuseParseChnlPlan_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);
	Hal_EfuseParseXtal_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);
	Hal_EfuseParseThermalMeter_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);
	Hal_EfuseParseAntennaDiversity_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);
	Hal_EfuseParseCustomerID_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag);

	hal_EfuseParseLEDSetting(adapt, hwinfo, pHalData->bautoload_fail_flag);

	/* set coex. ant info once efuse parsing is done */
	rtw_btcoex_set_ant_info(adapt);

	/* Hal_EfuseParseKFreeData_8723D(adapt, hwinfo, pHalData->bautoload_fail_flag); */
	if (hal_read_mac_hidden_rpt(adapt) != _SUCCESS)
		goto exit;
	ret = _SUCCESS;

exit:
	return ret;
}

static u8
hal_EfuseParsePROMContent(
	struct adapter * adapt
)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	u8 eeValue;
	u8 ret = _FAIL;

	eeValue = rtw_read8(adapt, REG_9346CR);
	/* To check system boot selection. */
	pHalData->EepromOrEfuse = (eeValue & BOOT_FROM_EEPROM) ? true : false;
	pHalData->bautoload_fail_flag = (eeValue & EEPROM_EN) ? false : true;

	RTW_INFO("Boot from %s, Autoload %s !\n", (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
		 (pHalData->bautoload_fail_flag ? "Fail" : "OK"));

	if (InitadaptVariablesByPROM_8723du(adapt) != _SUCCESS)
		goto exit;

	ret = _SUCCESS;

exit:
	return ret;
}

static void
_ReadRFType(struct adapter * adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

#if DISABLE_BB_RF
	pHalData->rf_chip = RF_PSEUDO_11N;
#else
	pHalData->rf_chip = RF_6052;
#endif
}

/*
 * Description:
 *    We should set Efuse cell selection to WiFi cell in default.
 * Assumption:
 *    PASSIVE_LEVEL
 */
static void
hal_EfuseCellSel(struct adapter * adapt)
{
	u32 value32;

	value32 = rtw_read32(adapt, EFUSE_TEST);
	value32 = (value32 & ~EFUSE_SEL_MASK) | EFUSE_SEL(EFUSE_WIFI_SEL_0);
	rtw_write32(adapt, EFUSE_TEST, value32);
}

static u8 rtl8723du_read_adapter_info(struct adapter * adapt)
{
	u8 ret = _FAIL;

	/* Read EEPROM size before call any EEPROM function */
	adapt->EepromAddressSize = GetEEPROMSize8723D(adapt);

	/* Efuse_InitSomeVar(adapt); */

	hal_EfuseCellSel(adapt);

	_ReadRFType(adapt);
	if (hal_EfuseParsePROMContent(adapt) != _SUCCESS)
		goto exit;

	ret = _SUCCESS;

exit:
	return ret;
}

#define GPIO_DEBUG_PORT_NUM 0
static void rtl8723du_trigger_gpio_0(struct adapter * adapt)
{

	u32 gpioctrl;

	RTW_INFO("==> trigger_gpio_0...\n");
	rtw_write16_async(adapt, REG_GPIO_PIN_CTRL, 0);
	rtw_write8_async(adapt, REG_GPIO_PIN_CTRL + 2, 0xFF);
	gpioctrl = (BIT(GPIO_DEBUG_PORT_NUM) << 24) | (BIT(GPIO_DEBUG_PORT_NUM) << 16);
	rtw_write32_async(adapt, REG_GPIO_PIN_CTRL, gpioctrl);
	gpioctrl |= (BIT(GPIO_DEBUG_PORT_NUM) << 8);
	rtw_write32_async(adapt, REG_GPIO_PIN_CTRL, gpioctrl);
	RTW_INFO("<=== trigger_gpio_0...\n");

}

/*
 * If variable not handled here,
 * some variables will be processed in SetHwReg8723A()
 */
static u8 SetHwReg8723du(struct adapter * adapt, u8 variable, u8 *val)
{
	u8 ret = _SUCCESS;

	switch (variable) {
	case HW_VAR_RXDMA_AGG_PG_TH:
		break;
	case HW_VAR_SET_RPWM:
		rtw_write8(adapt, REG_USB_HRPWM, *val);
		break;
	case HW_VAR_TRIGGER_GPIO_0:
		rtl8723du_trigger_gpio_0(adapt);
		break;
	default:
		ret = SetHwReg8723D(adapt, variable, val);
		break;
	}
	return ret;
}

/*
 * If variable not handled here,
 * some variables will be processed in GetHwReg8723A()
 */
static void GetHwReg8723du(struct adapter * adapt, u8 variable, u8 *val)
{
	switch (variable) {
	default:
		GetHwReg8723D(adapt, variable, val);
		break;
	}

}

/*
 * Description:
 * Query setting of specified variable.
 */
static u8
GetHalDefVar8723du(
	struct adapter * adapt,
	enum hal_def_variable eVariable,
	void * pValue
)
{
	u8 bResult = _SUCCESS;

	switch (eVariable) {
	case HAL_DEF_IS_SUPPORT_ANT_DIV:
		break;
	case HAL_DEF_DRVINFO_SZ:
		*((u32 *)pValue) = DRVINFO_SZ;
		break;
	case HAL_DEF_MAX_RECVBUF_SZ:
		*((u32 *)pValue) = MAX_RECVBUF_SZ;
		break;
	case HAL_DEF_RX_PACKET_OFFSET:
		*((u32 *)pValue) = RXDESC_SIZE + DRVINFO_SZ * 8;
		break;
	case HW_VAR_MAX_RX_AMPDU_FACTOR:
		*((enum ht_cap_ampdu_factor *)pValue) = MAX_AMPDU_FACTOR_64K;
		break;
	default:
		bResult = GetHalDefVar8723D(adapt, eVariable, pValue);
		break;
	}

	return bResult;
}




/*
 * Description:
 * Change default setting of specified variable.
 */
static u8
SetHalDefVar8723du(
	struct adapter * adapt,
	enum hal_def_variable eVariable,
	void * pValue
)
{
	u8 bResult = _SUCCESS;

	switch (eVariable) {
	default:
		bResult = SetHalDefVar8723D(adapt, eVariable, pValue);
		break;
	}

	return bResult;
}

static u8 rtl8723du_ps_func(struct adapter * adapt, enum hal_intf_ps_func efunc_id, u8 *val)
{
	u8 bResult = true;

	switch (efunc_id) {

#if defined(CONFIG_AUTOSUSPEND) && defined(SUPPORT_HW_RFOFF_DETECTED)
	case HAL_USB_SELECT_SUSPEND: {
		u8 bfwpoll = *((u8 *)val);

		rtl8723d_set_FwSelectSuspend_cmd(adapt, bfwpoll, 500); /*note fw to support hw power down ping detect */
	}
	break;
#endif /*CONFIG_AUTOSUSPEND && SUPPORT_HW_RFOFF_DETECTED */

	default:
		break;
	}
	return bResult;
}

void rtl8723du_set_hal_ops(struct adapter * adapt)
{
	struct hal_ops *pHalFunc = &adapt->hal_func;


	rtl8723d_set_hal_ops(pHalFunc);

	pHalFunc->hal_power_on = &_InitPowerOn_8723du;
	pHalFunc->hal_power_off = &CardDisableRTL8723du;

	pHalFunc->hal_init = &rtl8723du_hal_init;
	pHalFunc->hal_deinit = &rtl8723du_hal_deinit;

	pHalFunc->inirp_init = &rtl8723du_inirp_init;
	pHalFunc->inirp_deinit = &rtl8723du_inirp_deinit;

	pHalFunc->init_xmit_priv = &rtl8723du_init_xmit_priv;
	pHalFunc->free_xmit_priv = &rtl8723du_free_xmit_priv;

	pHalFunc->init_recv_priv = &rtl8723du_init_recv_priv;
	pHalFunc->free_recv_priv = &rtl8723du_free_recv_priv;

	pHalFunc->InitSwLeds = &rtl8723du_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8723du_DeInitSwLeds;

	pHalFunc->init_default_value = &rtl8723d_init_default_value;
	pHalFunc->intf_chip_configure = &rtl8723du_interface_configure;
	pHalFunc->read_adapter_info = &rtl8723du_read_adapter_info;

	pHalFunc->set_hw_reg_handler = &SetHwReg8723du;
	pHalFunc->GetHwRegHandler = &GetHwReg8723du;
	pHalFunc->get_hal_def_var_handler = &GetHalDefVar8723du;
	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8723du;

	pHalFunc->hal_xmit = &rtl8723du_hal_xmit;
	pHalFunc->mgnt_xmit = &rtl8723du_mgnt_xmit;
	pHalFunc->hal_xmitframe_enqueue = &rtl8723du_hal_xmitframe_enqueue;

	pHalFunc->interface_ps_func = &rtl8723du_ps_func;
}
