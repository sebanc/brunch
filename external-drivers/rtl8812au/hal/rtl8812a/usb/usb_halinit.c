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
#include <rtl8812a_hal.h>

#ifndef CONFIG_USB_HCI

	#error "CONFIG_USB_HCI shall be on!\n"

#endif


static void _dbg_dump_macreg(_adapter *padapter)
{
	u32 offset = 0;
	u32 val32 = 0;
	u32 index = 0 ;
	for (index = 0; index < 64; index++) {
		offset = index * 4;
		val32 = rtw_read32(padapter, offset);
		RTW_INFO("offset : 0x%02x ,val:0x%08x\n", offset, val32);
	}
}

static VOID
_ConfigChipOutEP_8812(
	IN	PADAPTER	pAdapter,
	IN	u8		NumOutPipe
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(pAdapter);


	pHalData->OutEpQueueSel = 0;
	pHalData->OutEpNumber = 0;

	switch (NumOutPipe) {
	case	4:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ | TX_SELE_EQ;
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

static VOID _FourOutPipeMapping88212AU(
	IN	PADAPTER	pAdapter,
	IN	BOOLEAN		bWIFICfg
)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if (bWIFICfg) { /* for WMM */

		/* 0:H, 1:N, 2:L ,3:E */

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];/* VO */
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];/* VI */
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];/* BE */
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];/* BK */

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];/* MGT */
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[3];/* HIGH */
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];/* TXCMD */

	} else { /* typical setting */

		/* 0:H, 1:N, 2:L, 3:E */

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[1];/* VO */
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];/* VI */
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];/* BE */
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[2];/* BK */

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[3];/* MGT */
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];/* HIGH */
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[3];/* TXCMD */
	}

}

static BOOLEAN HalUsbSetQueuePipeMapping8812AUsb(
	IN	PADAPTER	pAdapter,
	IN	u8		NumInPipe,
	IN	u8		NumOutPipe
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(pAdapter);
	BOOLEAN			result		= _FALSE;
	struct registry_priv *pregistrypriv = &pAdapter->registrypriv;
	BOOLEAN	 bWIFICfg = (pregistrypriv->wifi_spec) ? _TRUE : _FALSE;

	_ConfigChipOutEP_8812(pAdapter, NumOutPipe);

	/* Normal chip with one IN and one OUT doesn't have interrupt IN EP. */
	if (1 == pHalData->OutEpNumber) {
		if (1 != NumInPipe)
			return result;
	}

	/* All config other than above support one Bulk IN and one Interrupt IN. */
	/* if(2 != NumInPipe){ */
	/*	return result; */
	/* } */

	if (NumOutPipe == 4) {
		result = _TRUE;
		_FourOutPipeMapping88212AU(pAdapter, bWIFICfg);
	} else
	result = Hal_MappingOutPipe(pAdapter, NumOutPipe);

	return result;

}

void rtl8812au_interface_configure(_adapter *padapter)
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

	pHalData->interfaceIndex = pdvobjpriv->InterfaceNumber;

#ifdef CONFIG_USB_TX_AGGREGATION
	pHalData->UsbTxAggMode		= 1;
	pHalData->UsbTxAggDescNum	= 6;	/* only 4 bits */

	if (IS_HARDWARE_TYPE_8812AU(padapter))   /* page added for Jaguar */
		pHalData->UsbTxAggDescNum	= 0x01 ; /* adjust value for OQT  Overflow issue */ /* 0x3;	 */ /* only 4 bits */
#endif

#ifdef CONFIG_USB_RX_AGGREGATION
	if (IS_HARDWARE_TYPE_8812AU(padapter))
		pHalData->rxagg_mode = RX_AGG_USB;
	else
		pHalData->rxagg_mode = RX_AGG_USB; /* todo: change to USB_RX_AGG_DMA */
	pHalData->rxagg_usb_size	= 8; /* unit: 512b */
	pHalData->rxagg_usb_timeout	= 0x6;
	pHalData->rxagg_dma_size	= 16; /* uint: 128b, 0x0A = 10 = MAX_RX_DMA_BUFFER_SIZE/2/pHalData->UsbBulkOutSize */
	pHalData->rxagg_dma_timeout = 0x6; /* 6, absolute time = 34ms/(2^6) */

	if (IS_SUPER_SPEED_USB(padapter)) {
		pHalData->rxagg_usb_size = 0x7;
		pHalData->rxagg_usb_timeout = 0x1a;
	} else {
		/* the setting to reduce RX FIFO overflow on USB2.0 and increase rx throughput */

#ifdef CONFIG_PREALLOC_RX_SKB_BUFFER
		u32 remainder = 0;
		u8 quotient = 0;

		remainder = MAX_RECVBUF_SZ % (4 * 1024);
		quotient = (u8)(MAX_RECVBUF_SZ >> 12);

		if (quotient > 5) {
			pHalData->rxagg_usb_size = 0x5;
			pHalData->rxagg_usb_timeout = 0x20;
		} else {
			if (remainder >= 2048) {
				pHalData->rxagg_usb_size = quotient;
				pHalData->rxagg_usb_timeout = 0x10;
			} else {
				pHalData->rxagg_usb_size = (quotient - 1);
				pHalData->rxagg_usb_timeout = 0x10;
			}
		}
#else /* !CONFIG_PREALLOC_RX_SKB_BUFFER */
		pHalData->rxagg_usb_size = 0x5;
		pHalData->rxagg_usb_timeout = 0x20;
#endif /* CONFIG_PREALLOC_RX_SKB_BUFFER */

	}
#endif /* CONFIG_USB_RX_AGGREGATION */

	HalUsbSetQueuePipeMapping8812AUsb(padapter,
			  pdvobjpriv->RtNumInPipes, pdvobjpriv->RtNumOutPipes);

}

static VOID
_InitBurstPktLen(IN PADAPTER Adapter)
{
	u1Byte speedvalue, provalue, temp;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);


	/* rtw_write16(Adapter, REG_TRXDMA_CTRL_8195, 0xf5b0); */
	/* rtw_write16(Adapter, REG_TRXDMA_CTRL_8812, 0xf5b4); */
	rtw_write8(Adapter, 0xf050, 0x01);  /* usb3 rx interval */
	rtw_write16(Adapter, REG_RXDMA_STATUS, 0x7400);  /* burset lenght=4, set 0x3400 for burset length=2 */
	rtw_write8(Adapter, 0x289, 0xf5);				/* for rxdma control */
	/* rtw_write8(Adapter, 0x3a, 0x46); */

	/* 0x456 = 0x70, sugguested by Zhilin */
	if (IS_HARDWARE_TYPE_8821U(Adapter))
		rtw_write8(Adapter, REG_AMPDU_MAX_TIME_8812, 0x5e);
	else
		rtw_write8(Adapter, REG_AMPDU_MAX_TIME_8812, 0x70);

	rtw_write32(Adapter, REG_AMPDU_MAX_LENGTH_8812, 0xffffffff);
	rtw_write8(Adapter, REG_USTIME_TSF, 0x50);
	rtw_write8(Adapter, REG_USTIME_EDCA, 0x50);

	if (IS_HARDWARE_TYPE_8821U(Adapter))
		speedvalue = BIT7;
	else
		speedvalue = rtw_read8(Adapter, 0xff); /* check device operation speed: SS 0xff bit7 */

	if (speedvalue & BIT7) { /* USB2/1.1 Mode */
		temp = rtw_read8(Adapter, 0xfe17);
		if (((temp >> 4) & 0x03) == 0) {
			pHalData->UsbBulkOutSize = USB_HIGH_SPEED_BULK_SIZE;
			provalue = rtw_read8(Adapter, REG_RXDMA_PRO_8812);
			rtw_write8(Adapter, REG_RXDMA_PRO_8812, ((provalue | BIT(4) | BIT(3) | BIT(2) | BIT(1)) & (~BIT(5)))); /* set burst pkt len=512B */
		} else {
			pHalData->UsbBulkOutSize = 64;
			provalue = rtw_read8(Adapter, REG_RXDMA_PRO_8812);
			rtw_write8(Adapter, REG_RXDMA_PRO_8812, ((provalue | BIT(5) | BIT(3) | BIT(2) | BIT(1)) & (~BIT(4)))); /* set burst pkt len=64B */
		}

		/* rtw_write8(Adapter, 0x10c, 0xb4); */
		/* hal_UphyUpdate8812AU(Adapter); */

		pHalData->bSupportUSB3 = _FALSE;
	} else { /* USB3 Mode */
		pHalData->UsbBulkOutSize = USB_SUPER_SPEED_BULK_SIZE;
		provalue = rtw_read8(Adapter, REG_RXDMA_PRO_8812);
		rtw_write8(Adapter, REG_RXDMA_PRO_8812, ((provalue | BIT(3) | BIT(2) | BIT(1)) & (~(BIT5 | BIT4)))); /* set burst pkt len=1k */
		/* PlatformEFIOWrite2Byte(Adapter, REG_RXDMA_AGG_PG_TH,0x0a05); */ /* dmc agg th 20K */
		pHalData->bSupportUSB3 = _TRUE;

		/* set Reg 0xf008[3:4] to 2'00 to disable U1/U2 Mode to avoid 2.5G spur in USB3.0. added by page, 20120712 */
		rtw_write8(Adapter, 0xf008, rtw_read8(Adapter, 0xf008) & 0xE7);
	}

#ifdef CONFIG_USB_TX_AGGREGATION
	/* rtw_write8(Adapter, REG_TDECTRL_8195, 0x30); */
#else
	rtw_write8(Adapter, REG_TDECTRL, 0x10);
#endif

	temp = rtw_read8(Adapter, REG_SYS_FUNC_EN);
	rtw_write8(Adapter, REG_SYS_FUNC_EN, temp & (~BIT(10))); /* reset 8051 */

	rtw_write8(Adapter, REG_HT_SINGLE_AMPDU_8812, rtw_read8(Adapter, REG_HT_SINGLE_AMPDU_8812) | BIT(7)); /* enable single pkt ampdu */
	rtw_write8(Adapter, REG_RX_PKT_LIMIT, 0x18);		/* for VHT packet length 11K */

	rtw_write8(Adapter, REG_PIFS, 0x00);

	if (IS_HARDWARE_TYPE_8821U(Adapter) && (Adapter->registrypriv.wifi_spec == _FALSE)) {
		/* 0x0a0a too small , it can't pass AC logo. change to 0x1f1f */
		rtw_write16(Adapter, REG_MAX_AGGR_NUM, 0x1f1f);
		rtw_write8(Adapter, REG_FWHW_TXQ_CTRL, 0x80);
		rtw_write32(Adapter, REG_FAST_EDCA_CTRL, 0x03087777);
	} else {
		rtw_write16(Adapter, REG_MAX_AGGR_NUM, 0x1f1f);
		rtw_write8(Adapter, REG_FWHW_TXQ_CTRL, rtw_read8(Adapter, REG_FWHW_TXQ_CTRL) & (~BIT(7)));
	}

	if (pHalData->AMPDUBurstMode)
		rtw_write8(Adapter, REG_AMPDU_BURST_MODE_8812,  0x5F);

	rtw_write8(Adapter, 0x1c, rtw_read8(Adapter, 0x1c) | BIT(5) | BIT(6)); /* to prevent mac is reseted by bus. 20111208, by Page */

	/* ARFB table 9 for 11ac 5G 2SS */
	rtw_write32(Adapter, REG_ARFR0_8812, 0x00000010);
	rtw_write32(Adapter, REG_ARFR0_8812 + 4, 0xfffff000);

	/* ARFB table 10 for 11ac 5G 1SS */
	rtw_write32(Adapter, REG_ARFR1_8812, 0x00000010);
	rtw_write32(Adapter, REG_ARFR1_8812 + 4, 0x003ff000);

	/* ARFB table 11 for 11ac 24G 1SS */
	rtw_write32(Adapter, REG_ARFR2_8812, 0x00000015);
	rtw_write32(Adapter, REG_ARFR2_8812 + 4, 0x003ff000);
	/* ARFB table 12 for 11ac 24G 2SS */
	rtw_write32(Adapter, REG_ARFR3_8812, 0x00000015);
	rtw_write32(Adapter, REG_ARFR3_8812 + 4, 0xffcff000);
}

static u32 _InitPowerOn_8812AU(_adapter *padapter)
{
	u16	u2btmp = 0;
	u8	u1btmp = 0;
	u8	bMacPwrCtrlOn = _FALSE;
	/* HW Power on sequence */

	rtw_hal_get_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if (bMacPwrCtrlOn == _TRUE)
		return _SUCCESS;

	if (IS_VENDOR_8821A_MP_CHIP(padapter)) {
		/* HW Power on sequence */
		if (!HalPwrSeqCmdParsing(padapter, PWR_CUT_A_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8821A_NIC_ENABLE_FLOW)) {
			RTW_ERR("%s: run power on flow fail\n", __func__);
			return _FAIL;
		}
	} else if (IS_HARDWARE_TYPE_8821U(padapter)) {
		if (!HalPwrSeqCmdParsing(padapter, PWR_CUT_TESTCHIP_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8821A_NIC_ENABLE_FLOW)) {
			RTW_ERR("%s: run power on flow fail\n", __func__);
			return _FAIL;
		}
	} else {
		if (!HalPwrSeqCmdParsing(padapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8812_NIC_ENABLE_FLOW)) {
			RTW_ERR("%s: run power on flow fail\n", __func__);
			return _FAIL;
		}
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC block */
	/* Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31. */
	rtw_write16(padapter, REG_CR, 0x00);  /* suggseted by zhouzhou, by page, 20111230 */
	u2btmp = rtw_read16(padapter, REG_CR);
	u2btmp |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
		   | PROTOCOL_EN | SCHEDULE_EN | ENSEC | CALTMR_EN);
	rtw_write16(padapter, REG_CR, u2btmp);

	/* Need remove below furture, suggest by Jackie. */
	/* if 0xF0[24] =1 (LDO), need to set the 0x7C[6] to 1. */
	if (IS_HARDWARE_TYPE_8821U(padapter)) {
		u1btmp = rtw_read8(padapter, REG_SYS_CFG + 3);
		if (u1btmp & BIT0) { /* LDO mode. */
			u1btmp = rtw_read8(padapter, 0x7c);
			rtw_write8(padapter, 0x7c, u1btmp | BIT6);
		}
	}
	bMacPwrCtrlOn = _TRUE;
	rtw_hal_set_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);

	return _SUCCESS;
}





/* ---------------------------------------------------------------
 *
 *	MAC init functions
 *
 * --------------------------------------------------------------- */

/* Shall USB interface init this? */
static VOID
_InitInterrupt_8812AU(
	IN  PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);
	u8 usb_opt;
#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	struct dvobj_priv *pdev = adapter_to_dvobj(Adapter);
#endif

	/* HIMR */
	rtw_write32(Adapter, REG_HIMR0_8812, pHalData->IntrMask[0] & 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HIMR1_8812, pHalData->IntrMask[1] & 0xFFFFFFFF);

#ifdef CONFIG_SUPPORT_USB_INT
	/* REG_USB_SPECIAL_OPTION - BIT(4)*/
	/* 0; Use interrupt endpoint to upload interrupt pkt*/
	/* 1; Use bulk endpoint to upload interrupt pkt,*/
	usb_opt = rtw_read8(Adapter, REG_USB_SPECIAL_OPTION);

	if ((IS_FULL_SPEED_USB(Adapter))
#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	    || (pdev->RtInPipe[REALTEK_USB_IN_INT_EP_IDX] == 0x07)
#endif
	   )
		usb_opt = usb_opt & (~INT_BULK_SEL);
	else
		usb_opt = usb_opt | (INT_BULK_SEL);

	rtw_write8(Adapter, REG_USB_SPECIAL_OPTION, usb_opt);

#endif/*CONFIG_SUPPORT_USB_INT*/
}

static VOID
_InitQueueReservedPage_8821AUsb(
	IN  PADAPTER Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct registry_priv	*pregistrypriv = &Adapter->registrypriv;
	u32			numHQ		= 0;
	u32			numLQ		= 0;
	u32			numNQ		= 0;
	u32			numEQ		= 0;
	u32			numPubQ	= 0;
	u32			value32;
	u32			value32_npq;
	u8			value8;
	BOOLEAN			bWiFiConfig	= pregistrypriv->wifi_spec;
#ifdef CONFIG_MCC_MODE
	u8 en_mcc = pregistrypriv->en_mcc;
#else /* !CONFIG_MCC_MODE */
	u8 en_mcc = _FALSE;
#endif /* CONFIG_MCC_MODE */

	if (bWiFiConfig) {
		/* wifi test case */
		if (pHalData->OutEpQueueSel & TX_SELE_HQ)
			numHQ = WMM_NORMAL_PAGE_NUM_HPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_LQ)
			numLQ = WMM_NORMAL_PAGE_NUM_LPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_NQ)
			numNQ = WMM_NORMAL_PAGE_NUM_NPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_EQ)
		 	numEQ = NORMAL_PAGE_NUM_EPQ_8821;
	} else if (en_mcc) {
		/* mcc case */
		if (pHalData->OutEpQueueSel & TX_SELE_HQ)
			numHQ = MCC_NORMAL_PAGE_NUM_HPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_LQ)
			numLQ = MCC_NORMAL_PAGE_NUM_LPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_NQ)
			numNQ = MCC_NORMAL_PAGE_NUM_NPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_EQ)
		 	numEQ = NORMAL_PAGE_NUM_EPQ_8821;
	} else {
		/* narmal case */
		if (pHalData->OutEpQueueSel & TX_SELE_HQ)
			numHQ = NORMAL_PAGE_NUM_HPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_LQ)
			numLQ = NORMAL_PAGE_NUM_LPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_NQ)
			numNQ = NORMAL_PAGE_NUM_NPQ_8821;
		if (pHalData->OutEpQueueSel & TX_SELE_EQ)
			numEQ = NORMAL_PAGE_NUM_EPQ_8821;

	}

	numPubQ = TX_TOTAL_PAGE_NUMBER_8821 - numHQ - numLQ - numNQ - numEQ;
	value32_npq = _NPQ(numNQ) | _EPQ(numEQ);
	rtw_write32(Adapter, REG_RQPN_NPQ, value32_npq);

	/* TX DMA RQPN */
	value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;
	rtw_write32(Adapter, REG_RQPN, value32);
}

static VOID
_InitQueueReservedPage_8812AUsb(
	IN  PADAPTER Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct registry_priv	*pregistrypriv = &Adapter->registrypriv;
	u32			numHQ		= 0;
	u32			numLQ		= 0;
	u32			numNQ		= 0;
	u32			numPubQ	= 0;
	u32			value32;
	u8			value8;
	BOOLEAN			bWiFiConfig	= pregistrypriv->wifi_spec;

	if (!bWiFiConfig) {
		if (pHalData->OutEpQueueSel & TX_SELE_HQ)
			numHQ = NORMAL_PAGE_NUM_HPQ_8812;

		if (pHalData->OutEpQueueSel & TX_SELE_LQ)
			numLQ = NORMAL_PAGE_NUM_LPQ_8812;

		/* NOTE: This step shall be proceed before writting REG_RQPN.		 */
		if (pHalData->OutEpQueueSel & TX_SELE_NQ)
			numNQ = NORMAL_PAGE_NUM_NPQ_8812;
	} else {
		/* WMM		 */
		if (pHalData->OutEpQueueSel & TX_SELE_HQ)
			numHQ = WMM_NORMAL_PAGE_NUM_HPQ_8812;

		if (pHalData->OutEpQueueSel & TX_SELE_LQ)
			numLQ = WMM_NORMAL_PAGE_NUM_LPQ_8812;

		/* NOTE: This step shall be proceed before writting REG_RQPN.		 */
		if (pHalData->OutEpQueueSel & TX_SELE_NQ)
			numNQ = WMM_NORMAL_PAGE_NUM_NPQ_8812;
	}

	numPubQ = TX_TOTAL_PAGE_NUMBER_8812 - numHQ - numLQ - numNQ;

	value8 = (u8)_NPQ(numNQ);
	rtw_write8(Adapter, REG_RQPN_NPQ, value8);

	/* TX DMA */
	value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;
	rtw_write32(Adapter, REG_RQPN, value32);
}

static VOID
_InitTxBufferBoundary_8821AUsb(
	IN PADAPTER Adapter
)
{
	struct registry_priv *pregistrypriv = &Adapter->registrypriv;
	u8	txpktbuf_bndy;

	if (!pregistrypriv->wifi_spec)
		txpktbuf_bndy = TX_PAGE_BOUNDARY_8821;
	else {
		/* for WMM */
		txpktbuf_bndy = WMM_NORMAL_TX_PAGE_BOUNDARY_8821;
	}

	rtw_write8(Adapter, REG_BCNQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_MGQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_WMAC_LBK_BF_HD, txpktbuf_bndy);
	rtw_write8(Adapter, REG_TRXFF_BNDY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_TDECTRL + 1, txpktbuf_bndy);

#ifdef CONFIG_CONCURRENT_MODE
	rtw_write8(Adapter, REG_BCNQ1_BDNY, txpktbuf_bndy + 8);
	rtw_write8(Adapter, REG_DWBCN1_CTRL_8812 + 1, txpktbuf_bndy + 8); /* BCN1_HEAD */
	/* BIT1- BIT_SW_BCN_SEL_EN */
	rtw_write8(Adapter, REG_DWBCN1_CTRL_8812 + 2, rtw_read8(Adapter, REG_DWBCN1_CTRL_8812 + 2) | BIT1);
#endif

}

static VOID
_InitTxBufferBoundary_8812AUsb(
	IN PADAPTER Adapter
)
{
	struct registry_priv *pregistrypriv = &Adapter->registrypriv;
	u8	txpktbuf_bndy;

	if (!pregistrypriv->wifi_spec)
		txpktbuf_bndy = TX_PAGE_BOUNDARY_8812;
	else {
		/* for WMM */
		txpktbuf_bndy = WMM_NORMAL_TX_PAGE_BOUNDARY_8812;
	}

	rtw_write8(Adapter, REG_BCNQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_MGQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_WMAC_LBK_BF_HD, txpktbuf_bndy);
	rtw_write8(Adapter, REG_TRXFF_BNDY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_TDECTRL + 1, txpktbuf_bndy);

}

static VOID
_InitPageBoundary_8812AUsb(
	IN  PADAPTER Adapter
)
{
	/* u2Byte 			rxff_bndy; */
	/* u2Byte			Offset; */
	/* BOOLEAN			bSupportRemoteWakeUp; */

	/* Adapter->HalFunc.get_hal_def_var_handler(Adapter, HAL_DEF_WOWLAN , &bSupportRemoteWakeUp); */
	/* RX Page Boundary */
	/* srand(static_cast<unsigned int>(time(NULL)) ); */

	/*	Offset = MAX_RX_DMA_BUFFER_SIZE_8812/256;
	 *	rxff_bndy = (Offset*256)-1; */

	if (IS_HARDWARE_TYPE_8812(Adapter))
		rtw_write16(Adapter, (REG_TRXFF_BNDY + 2), RX_DMA_BOUNDARY_8812);
	else
		rtw_write16(Adapter, (REG_TRXFF_BNDY + 2), RX_DMA_BOUNDARY_8821);

}


static VOID
_InitNormalChipRegPriority_8812AUsb(
	IN	PADAPTER	Adapter,
	IN	u16		beQ,
	IN	u16		bkQ,
	IN	u16		viQ,
	IN	u16		voQ,
	IN	u16		mgtQ,
	IN	u16		hiQ
)
{
	u16 value16	= (rtw_read16(Adapter, REG_TRXDMA_CTRL) & 0x7);

	value16 |=	_TXDMA_BEQ_MAP(beQ)	| _TXDMA_BKQ_MAP(bkQ) |
			_TXDMA_VIQ_MAP(viQ)	| _TXDMA_VOQ_MAP(voQ) |
			_TXDMA_MGQ_MAP(mgtQ) | _TXDMA_HIQ_MAP(hiQ);

	rtw_write16(Adapter, REG_TRXDMA_CTRL, value16);
}

static VOID
_InitNormalChipTwoOutEpPriority_8812AUsb(
	IN	PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);
	struct registry_priv *pregistrypriv = &Adapter->registrypriv;
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

	_InitNormalChipRegPriority_8812AUsb(Adapter, beQ, bkQ, viQ, voQ, mgtQ, hiQ);

}

static VOID
_InitNormalChipThreeOutEpPriority_8812AUsb(
	IN	PADAPTER Adapter
)
{
	struct registry_priv *pregistrypriv = &Adapter->registrypriv;
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
	_InitNormalChipRegPriority_8812AUsb(Adapter, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static VOID
init_hi_queue_config_8812a_usb(
	IN	PADAPTER Adapter
)
{
	/* Packet in Hi Queue Tx immediately (No constraint for ATIM Period)*/
	rtw_write8(Adapter, REG_HIQ_NO_LMT_EN, 0xFF);
}

static VOID
_InitNormalChipFourOutEpPriority_8812AUsb(
	IN	PADAPTER Adapter
)
{
	struct registry_priv *pregistrypriv = &Adapter->registrypriv;
	u16			beQ, bkQ, viQ, voQ, mgtQ, hiQ;

	if (!pregistrypriv->wifi_spec) { /* typical setting */
		beQ		= QUEUE_LOW;
		bkQ		= QUEUE_LOW;
		viQ		= QUEUE_NORMAL;
		voQ		= QUEUE_NORMAL;
		mgtQ	= QUEUE_EXTRA;
		hiQ		= QUEUE_HIGH;
	} else { /* for WMM */
		beQ		= QUEUE_LOW;
		bkQ		= QUEUE_NORMAL;
		viQ		= QUEUE_NORMAL;
		voQ		= QUEUE_HIGH;
		mgtQ	= QUEUE_HIGH;
		hiQ		= QUEUE_HIGH;
	}
	_InitNormalChipRegPriority_8812AUsb(Adapter, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
	init_hi_queue_config_8812a_usb(Adapter);
}


static VOID
_InitQueuePriority_8812AUsb(
	IN	PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	switch (pHalData->OutEpNumber) {
	case 2:
		_InitNormalChipTwoOutEpPriority_8812AUsb(Adapter);
		break;
	case 3:
		_InitNormalChipThreeOutEpPriority_8812AUsb(Adapter);
		break;
	case 4:
		_InitNormalChipFourOutEpPriority_8812AUsb(Adapter);
		break;
	default:
		RTW_INFO("_InitQueuePriority_8812AUsb(): Shall not reach here!\n");
		break;
	}
}



static VOID
_InitHardwareDropIncorrectBulkOut_8812A(
	IN  PADAPTER Adapter
)
{
#ifdef ENABLE_USB_DROP_INCORRECT_OUT
	u32	value32 = rtw_read32(Adapter, REG_TXDMA_OFFSET_CHK);
	value32 |= DROP_DATA_EN;
	rtw_write32(Adapter, REG_TXDMA_OFFSET_CHK, value32);
#endif
}

static VOID
_InitNetworkType_8812A(
	IN  PADAPTER Adapter
)
{
	u32	value32;

	value32 = rtw_read32(Adapter, REG_CR);
	/* TODO: use the other function to set network type */
	value32 = (value32 & ~MASK_NETTYPE) | _NETTYPE(NT_LINK_AP);

	rtw_write32(Adapter, REG_CR, value32);
}

static VOID
_InitTransferPageSize_8812AUsb(
	IN  PADAPTER Adapter
)
{

	u8	value8;
	value8 = _PSTX(PBP_512);

	PlatformEFIOWrite1Byte(Adapter, REG_PBP, value8);
}

static VOID
_InitDriverInfoSize_8812A(
	IN  PADAPTER	Adapter,
	IN	u8		drvInfoSize
)
{
	rtw_write8(Adapter, REG_RX_DRVINFO_SZ, drvInfoSize);
}

static VOID
_InitWMACSetting_8812A(
	IN  PADAPTER Adapter
)
{
	/* u4Byte			value32; */
	u16			value16;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32 rcr;

	/* rcr = AAP | APM | AM | AB | APP_ICV | ADF | AMF | APP_FCS | HTC_LOC_CTRL | APP_MIC | APP_PHYSTS; */
	rcr =
		RCR_APM | RCR_AM | RCR_AB | RCR_CBSSID_DATA | RCR_CBSSID_BCN | RCR_APP_ICV | RCR_AMF | RCR_HTC_LOC_CTRL | RCR_APP_MIC | RCR_APP_PHYST_RXFF;

#if (1 == RTL8812A_RX_PACKET_INCLUDE_CRC)
	rcr |= ACRC32;
#endif

#ifdef CONFIG_RX_PACKET_APPEND_FCS
	rcr |= RCR_APPFCS;
#endif
	rcr |= FORCEACK;
	rtw_hal_set_hwreg(Adapter, HW_VAR_RCR, (u8 *)&rcr);

	/* Accept all multicast address */
	rtw_write32(Adapter, REG_MAR, 0xFFFFFFFF);
	rtw_write32(Adapter, REG_MAR + 4, 0xFFFFFFFF);


	/* Accept all data frames */
	/* value16 = 0xFFFF; */
	/* rtw_write16(Adapter, REG_RXFLTMAP2, value16); */

	/* 2010.09.08 hpfan */
	/* Since ADF is removed from RCR, ps-poll will not be indicate to driver, */
	/* RxFilterMap should mask ps-poll to gurantee AP mode can rx ps-poll. */
	value16 = BIT10;
#ifdef CONFIG_BEAMFORMING
	/* NDPA packet subtype is 0x0101 */
	value16 |= BIT5;
#endif/*CONFIG_BEAMFORMING*/
	rtw_write16(Adapter, REG_RXFLTMAP1, value16);

	/* Accept all management frames */
	/* value16 = 0xFFFF; */
	/* rtw_write16(Adapter, REG_RXFLTMAP0, value16); */

	/* enable RX_SHIFT bits */
	/* rtw_write8(Adapter, REG_TRXDMA_CTRL, rtw_read8(Adapter, REG_TRXDMA_CTRL)|BIT(1));	 */

}

static VOID
_InitAdaptiveCtrl_8812AUsb(
	IN  PADAPTER Adapter
)
{
	u16	value16;
	u32	value32;

	/* Response Rate Set */
	value32 = rtw_read32(Adapter, REG_RRSR);
	value32 &= ~RATE_BITMAP_ALL;

	if (Adapter->registrypriv.wireless_mode & WIRELESS_11B)
		value32 |= RATE_RRSR_CCK_ONLY_1M;
	else
		value32 |= RATE_RRSR_WITHOUT_CCK;

	value32 |= RATE_RRSR_CCK_ONLY_1M;
	rtw_write32(Adapter, REG_RRSR, value32);

	/* CF-END Threshold */
	/* m_spIoBase->rtw_write8(REG_CFEND_TH, 0x1); */

	/* SIFS (used in NAV) */
	value16 = _SPEC_SIFS_CCK(0x10) | _SPEC_SIFS_OFDM(0x10);
	rtw_write16(Adapter, REG_SPEC_SIFS, value16);

	/* Retry Limit */
	value16 = BIT_LRL(RL_VAL_STA) | BIT_SRL(RL_VAL_STA);
	rtw_write16(Adapter, REG_RETRY_LIMIT, value16);

}

static VOID
_InitEDCA_8812AUsb(
	IN  PADAPTER Adapter
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
	rtw_write8(Adapter, REG_USTIME_TSF, 0x50);
	rtw_write8(Adapter, REG_USTIME_EDCA, 0x50);
}


static VOID
_InitBeaconMaxError_8812A(
	IN  PADAPTER	Adapter,
	IN	BOOLEAN		InfraMode
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

static VOID
_InitRDGSetting_8812A(
	IN	PADAPTER Adapter
)
{
	rtw_write8(Adapter, REG_RD_CTRL, 0xFF);
	rtw_write16(Adapter, REG_RD_NAV_NXT, 0x200);
	rtw_write8(Adapter, REG_RD_RESP_PKT_TH, 0x05);
}

static VOID
_InitRetryFunction_8812A(
	IN  PADAPTER Adapter
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
static VOID
usb_AggSettingTxUpdate_8812A(
	IN	PADAPTER			Adapter
)
{
#ifdef CONFIG_USB_TX_AGGREGATION
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32			value32;

	if (Adapter->registrypriv.wifi_spec)
		pHalData->UsbTxAggMode = _FALSE;

	if (pHalData->UsbTxAggMode) {
		value32 = rtw_read32(Adapter, REG_TDECTRL);
		value32 = value32 & ~(BLK_DESC_NUM_MASK << BLK_DESC_NUM_SHIFT);
		value32 |= ((pHalData->UsbTxAggDescNum & BLK_DESC_NUM_MASK) << BLK_DESC_NUM_SHIFT);

		rtw_write32(Adapter, REG_DWBCN0_CTRL_8812, value32);
		if (IS_HARDWARE_TYPE_8821U(Adapter))   /* page added for Jaguar */
			rtw_write8(Adapter, REG_DWBCN1_CTRL_8812, pHalData->UsbTxAggDescNum << 1);
	}

#endif
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
static VOID
usb_AggSettingRxUpdate_8812A(
	IN	PADAPTER			Adapter
)
{
#ifdef CONFIG_USB_RX_AGGREGATION
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			valueDMA;
	u8			valueUSB;

	valueDMA = rtw_read8(Adapter, REG_TRXDMA_CTRL);
	switch (pHalData->rxagg_mode) {
	case RX_AGG_DMA:
		valueDMA |= RXDMA_AGG_EN;
		/* 2012/10/26 MH For TX through start rate temp fix. */
		{
			u16 temp;

			/* Adjust DMA page and thresh. */
			temp = pHalData->rxagg_dma_size | (pHalData->rxagg_dma_timeout << 8);
			rtw_write16(Adapter, REG_RXDMA_AGG_PG_TH, temp);
			rtw_write8(Adapter, REG_RXDMA_AGG_PG_TH + 3, BIT(7)); /* for dma agg , 0x280[31]GBIT_RXDMA_AGG_OLD_MOD, set 1 */
		}
		break;
	case RX_AGG_USB:
		valueDMA |= RXDMA_AGG_EN;
		{
			u16 temp;

			/* Adjust DMA page and thresh. */
			temp = pHalData->rxagg_usb_size | (pHalData->rxagg_usb_timeout << 8);
			rtw_write16(Adapter, REG_RXDMA_AGG_PG_TH, temp);
		}
		break;
	case RX_AGG_MIX:
	case RX_AGG_DISABLE:
	default:
		/* TODO: */
		break;
	}

	rtw_write8(Adapter, REG_TRXDMA_CTRL, valueDMA);
#endif
}	/* usb_AggSettingRxUpdate */

static VOID
init_UsbAggregationSetting_8812A(
	IN  PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	/* Tx aggregation setting */
	usb_AggSettingTxUpdate_8812A(Adapter);

	/* Rx aggregation setting */
	usb_AggSettingRxUpdate_8812A(Adapter);

	/* 201/12/10 MH Add for USB agg mode dynamic switch. */
	pHalData->UsbRxHighSpeedMode = _FALSE;
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
VOID
USB_AggModeSwitch(
	IN	PADAPTER			Adapter
)
{
#if 0
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);

	/* pHalData->UsbRxHighSpeedMode = FALSE; */
	/* How to measure the RX speed? We assume that when traffic is more than */
	if (pMgntInfo->bRegAggDMEnable == FALSE) {
		return;	/* Inf not support. */
	}


	if (pMgntInfo->LinkDetectInfo.bHigherBusyRxTraffic == TRUE &&
	    pHalData->UsbRxHighSpeedMode == FALSE) {
		pHalData->UsbRxHighSpeedMode = TRUE;
	} else if (pMgntInfo->LinkDetectInfo.bHigherBusyRxTraffic == FALSE &&
		   pHalData->UsbRxHighSpeedMode == TRUE) {
		pHalData->UsbRxHighSpeedMode = FALSE;
	} else
		return;


#if USB_RX_AGGREGATION_92C
	if (pHalData->UsbRxHighSpeedMode == TRUE) {
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



/* Set CCK and OFDM Block "ON" */
static VOID _BBTurnOnBlock(
	IN	PADAPTER		Adapter
)
{
#if (DISABLE_BB_RF)
	return;
#endif

	phy_set_bb_reg(Adapter, rFPGA0_RFMOD, bCCKEn, 0x1);
	phy_set_bb_reg(Adapter, rFPGA0_RFMOD, bOFDMEn, 0x1);
}

static VOID _RfPowerSave(
	IN	PADAPTER		Adapter
)
{
#if 0
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo	= &(Adapter->MgntInfo);
	enum rf_path			eRFPath;

#if (DISABLE_BB_RF)
	return;
#endif

	if (pMgntInfo->RegRfOff == TRUE) { /* User disable RF via registry. */
		MgntActSet_RF_State(Adapter, eRfOff, RF_CHANGE_BY_SW);
		/* Those action will be discard in MgntActSet_RF_State because off the same state */
		for (eRFPath = 0; eRFPath < pHalData->NumTotalRFPath; eRFPath++)
			phy_set_rf_reg(Adapter, eRFPath, 0x4, 0xC00, 0x0);
	} else if (pMgntInfo->RfOffReason > RF_CHANGE_BY_PS) { /* H/W or S/W RF OFF before sleep. */
		MgntActSet_RF_State(Adapter, eRfOff, pMgntInfo->RfOffReason);
	} else {
		pHalData->eRFPowerState = eRfOn;
		pMgntInfo->RfOffReason = 0;
		if (Adapter->bInSetPower || Adapter->bResetInProgress)
			PlatformUsbEnableInPipes(Adapter);
	}
#endif
}

enum {
	Antenna_Lfet = 1,
	Antenna_Right = 2,
};

/*
 * 2010/08/26 MH Add for selective suspend mode check.
 * If Efuse 0x0e bit1 is not enabled, we can not support selective suspend for Minicard and
 * slim card.
 *   */
static VOID
HalDetectSelectiveSuspendMode(
	IN PADAPTER				Adapter
)
{
#if 0
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
#endif
}	/* HalDetectSelectiveSuspendMode */

rt_rf_power_state RfOnOffDetect(IN	PADAPTER pAdapter)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(pAdapter);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(pAdapter);
	u8	val8;
	rt_rf_power_state rfpowerstate = rf_off;

	if (pwrctl->bHWPowerdown) {
		val8 = rtw_read8(pAdapter, REG_HSISR);
		RTW_INFO("pwrdown, 0x5c(BIT7)=%02x\n", val8);
		rfpowerstate = (val8 & BIT7) ? rf_off : rf_on;
	} else { /* rf on/off */
		rtw_write8(pAdapter, REG_MAC_PINMUX_CFG, rtw_read8(pAdapter, REG_MAC_PINMUX_CFG) & ~(BIT3));
		val8 = rtw_read8(pAdapter, REG_GPIO_IO_SEL);
		RTW_INFO("GPIO_IN=%02x\n", val8);
		rfpowerstate = (val8 & BIT3) ? rf_on : rf_off;
	}
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
static void rtl8812au_hw_reset(_adapter *Adapter)
{
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
}

u32 rtl8812au_hal_init(PADAPTER Adapter)
{
	u8	value8 = 0, u1bRegCR;
	u16  value16;
	u8	txpktbuf_bndy;
	u32	status = _SUCCESS;
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
#else
#define HAL_INIT_PROFILE_TAG(stage) do {} while (0)
#endif /* DBG_HAL_INIT_PROFILING */




	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_BEGIN);
	if (pwrctrlpriv->bkeepfwalive) {
		_ps_open_RF(Adapter);

		if (pHalData->bIQKInitialized) {
			/* PHY_IQCalibrate_8812A(Adapter,_TRUE); */
		} else {
			/* PHY_IQCalibrate_8812A(Adapter,_FALSE); */
			/* pHalData->bIQKInitialized = _TRUE; */
		}

		/* odm_txpowertracking_check(&pHalData->odmpriv ); */
		/* PHY_LCCalibrate_8812A(Adapter); */

		goto exit;
	}

	/* Check if MAC has already power on. by tynli. 2011.05.27. */
	value8 = rtw_read8(Adapter, REG_SYS_CLKR + 1);
	u1bRegCR = PlatformEFIORead1Byte(Adapter, REG_CR);
	RTW_INFO(" power-on :REG_SYS_CLKR 0x09=0x%02x. REG_CR 0x100=0x%02x.\n", value8, u1bRegCR);
	if ((value8 & BIT3)  && (u1bRegCR != 0 && u1bRegCR != 0xEA)) {
		/* pHalData->bMACFuncEnable = TRUE; */
		RTW_INFO(" MAC has already power on.\n");
	} else {
		/* pHalData->bMACFuncEnable = FALSE; */
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
	if (!IS_HARDWARE_TYPE_8821(Adapter)) {
		rtw_write8(Adapter, REG_RF_CTRL, 5);
		rtw_write8(Adapter, REG_RF_CTRL, 7);
		rtw_write8(Adapter, REG_RF_B_CTRL_8812, 5);
		rtw_write8(Adapter, REG_RF_B_CTRL_8812, 7);
	}

	/*
		If HW didn't go through a complete de-initial procedure,
		it probably occurs some problem for double initial procedure.
		Like "CONFIG_DEINIT_BEFORE_INIT" in 92du chip
	*/
	rtl8812au_hw_reset(Adapter);



	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_PW_ON);
	status = rtw_hal_power_on(Adapter);
	if (status == _FAIL) {
		goto exit;
	}

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_LLTT);
	if (!pregistrypriv->wifi_spec) {
		if (IS_HARDWARE_TYPE_8812(Adapter))
			txpktbuf_bndy = TX_PAGE_BOUNDARY_8812;
		else
			txpktbuf_bndy = TX_PAGE_BOUNDARY_8821;
	} else {
		/* for WMM */
		if (IS_HARDWARE_TYPE_8812(Adapter))
			txpktbuf_bndy = WMM_NORMAL_TX_PAGE_BOUNDARY_8812;
		else
			txpktbuf_bndy = WMM_NORMAL_TX_PAGE_BOUNDARY_8821;
	}

	status =  InitLLTTable8812A(Adapter, txpktbuf_bndy);
	if (status == _FAIL) {
		goto exit;
	}

	_InitHardwareDropIncorrectBulkOut_8812A(Adapter);

	if (pHalData->bRDGEnable)
		_InitRDGSetting_8812A(Adapter);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_DOWNLOAD_FW);
	if (Adapter->registrypriv.mp_mode == 0) {
		status = FirmwareDownload8812(Adapter, _FALSE);
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
	status = PHY_MACConfig8812(Adapter);
	if (status == _FAIL)
		goto exit;
#endif

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC01);
	if (IS_HARDWARE_TYPE_8812(Adapter)) {
		_InitQueueReservedPage_8812AUsb(Adapter);
		_InitTxBufferBoundary_8812AUsb(Adapter);
	} else if (IS_HARDWARE_TYPE_8821(Adapter)) {
		_InitQueueReservedPage_8821AUsb(Adapter);
		_InitTxBufferBoundary_8821AUsb(Adapter);
	}

	_InitQueuePriority_8812AUsb(Adapter);
	_InitPageBoundary_8812AUsb(Adapter);

	if (IS_HARDWARE_TYPE_8812(Adapter))
		_InitTransferPageSize_8812AUsb(Adapter);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC02);
	/* Get Rx PHY status in order to report RSSI and others. */
	_InitDriverInfoSize_8812A(Adapter, DRVINFO_SZ);

	_InitInterrupt_8812AU(Adapter);
	_InitNetworkType_8812A(Adapter);/* set msr	 */
	_InitWMACSetting_8812A(Adapter);
	_InitAdaptiveCtrl_8812AUsb(Adapter);
	_InitEDCA_8812AUsb(Adapter);

	_InitRetryFunction_8812A(Adapter);
	init_UsbAggregationSetting_8812A(Adapter);

	_InitBeaconParameters_8812A(Adapter);
	_InitBeaconMaxError_8812A(Adapter, _TRUE);

	_InitBurstPktLen(Adapter);  /* added by page. 20110919 */

	/*  */
	/* Init CR MACTXEN, MACRXEN after setting RxFF boundary REG_TRXFF_BNDY to patch */
	/* Hw bug which Hw initials RxFF boundry size to a value which is larger than the real Rx buffer size in 88E. */
	/* 2011.08.05. by tynli. */
	/*  */
	value8 = rtw_read8(Adapter, REG_CR);
	rtw_write8(Adapter, REG_CR, (value8 | MACTXEN | MACRXEN));

#if defined(CONFIG_CONCURRENT_MODE) || defined(CONFIG_TX_MCAST2UNI)

#ifdef CONFIG_CHECK_AC_LIFETIME
	/* Enable lifetime check for the four ACs */
	rtw_write8(Adapter, REG_LIFETIME_CTRL, rtw_read8(Adapter, REG_LIFETIME_CTRL) | 0x0f);
#endif /* CONFIG_CHECK_AC_LIFETIME */

#ifdef CONFIG_TX_MCAST2UNI
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x0400);	/* unit: 256us. 256ms */
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x0400);	/* unit: 256us. 256ms */
#else	/* CONFIG_TX_MCAST2UNI */
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x3000);	/* unit: 256us. 3s */
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x3000);	/* unit: 256us. 3s */
#endif /* CONFIG_TX_MCAST2UNI */
#endif /* CONFIG_CONCURRENT_MODE || CONFIG_TX_MCAST2UNI */


#ifdef CONFIG_RTW_LED
	_InitHWLed(Adapter);
#endif /* CONFIG_RTW_LED */

	/*  */
	/* d. Initialize BB related configurations. */
	/*  */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_BB);
#if (HAL_BB_ENABLE == 1)
	status = PHY_BBConfig8812(Adapter);
	if (status == _FAIL)
		goto exit;
#endif

	/* 92CU use 3-wire to r/w RF */
	/* pHalData->Rf_Mode = RF_OP_By_SW_3wire; */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_RF);
#if (HAL_RF_ENABLE == 1)
	status = PHY_RFConfig8812(Adapter);
	if (status == _FAIL)
		goto exit;

	if (pHalData->rf_type == RF_1T1R && IS_HARDWARE_TYPE_8812AU(Adapter))
		PHY_BB8812_Config_1T(Adapter);
	if (Adapter->registrypriv.rf_config == RF_1T2R && IS_HARDWARE_TYPE_8812AU(Adapter))
		phy_set_bb_reg(Adapter, rTxPath_Jaguar, bMaskLWord, 0x1111);
#endif

	if (Adapter->registrypriv.channel <= 14)
		PHY_SwitchWirelessBand8812(Adapter, BAND_ON_2_4G);
	else
		PHY_SwitchWirelessBand8812(Adapter, BAND_ON_5G);

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

	if (pregistrypriv->wifi_spec)
		rtw_write16(Adapter, REG_FAST_EDCA_CTRL , 0);
	/* adjust EDCCA to avoid collision */
	if (pregistrypriv->wifi_spec) {
		if (IS_HARDWARE_TYPE_8821(Adapter))
			if (Adapter->registrypriv.adaptivity_en == 0) {
				Adapter->registrypriv.adaptivity_en = 1;
				Adapter->registrypriv.adaptivity_mode = 0;
			}
	}
	/* Nav limit , suggest by scott */
	rtw_write8(Adapter, 0x652, 0x0);

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_HAL_DM);
	rtl8812_InitHalDm(Adapter);

#if (MP_DRIVER == 1)
	if (Adapter->registrypriv.mp_mode == 1) {
		Adapter->mppriv.channel = pHalData->current_channel;
		MPT_InitializeAdapter(Adapter, Adapter->mppriv.channel);
	} else
#endif  /* #if (MP_DRIVER == 1) */
	{
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

		/* 0x4c6[3] 1: RTS BW = Data BW */
		/* 0: RTS BW depends on CCA / secondary CCA result. */
		rtw_write8(Adapter, REG_QUEUE_CTRL, rtw_read8(Adapter, REG_QUEUE_CTRL) & 0xF7);

		/* enable Tx report. */
		rtw_write8(Adapter,  REG_FWHW_TXQ_CTRL + 1, 0x0F);

		/* Suggested by SD1 pisa. Added by tynli. 2011.10.21. */
		rtw_write8(Adapter, REG_EARLY_MODE_CONTROL_8812 + 3, 0x01); /* Pretx_en, for WEP/TKIP SEC */

		/* tynli_test_tx_report. */
		rtw_write16(Adapter, REG_TX_RPT_TIME, 0x3DF0);

		/* Reset USB mode switch setting */
		rtw_write8(Adapter, REG_SDIO_CTRL_8812, 0x0);
		rtw_write8(Adapter, REG_ACLK_MON, 0x0);


		HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_IQK);
		/* 2010/08/26 MH Merge from 8192CE. */
		if (pwrctrlpriv->rf_pwrstate == rf_on) {
			/*		if(IS_HARDWARE_TYPE_8812AU(Adapter))
					{
			#if (RTL8812A_SUPPORT == 1)
						pHalData->bNeedIQK = _TRUE;
						if(pHalData->bIQKInitialized)
							PHY_IQCalibrate_8812A(Adapter, _TRUE);
						else
						{
							PHY_IQCalibrate_8812A(Adapter, _FALSE);
							pHalData->bIQKInitialized = _TRUE;
						}
			#endif
					}*/

			HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_PW_TRACK);

			/* odm_txpowertracking_check(&pHalData->odmpriv ); */


			HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_LCK);
			/* PHY_LCCalibrate_8812A(Adapter); */
		}
	}

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC21);


	/* HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_INIT_PABIAS);
	 *	_InitPABias(Adapter); */

#ifdef CONFIG_BT_COEXIST
	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_BT_COEXIST);
	/* _InitBTCoexist(Adapter); */
	/* 2010/08/23 MH According to Alfred's suggestion, we need to to prevent HW enter */
	/* suspend mode automatically. */
	/* HwSuspendModeEnable92Cu(Adapter, _FALSE); */

	if (_TRUE == pHalData->EEPROMBluetoothCoexist) {
		/* Init BT hw config. */
		rtw_btcoex_HAL_Initialize(Adapter, _FALSE);
	} else {
		/* In combo card run wifi only , must setting some hardware reg. */
		rtl8812a_combo_card_WifiOnlyHwInit(Adapter);
	}
#endif /* CONFIG_BT_COEXIST */

	HAL_INIT_PROFILE_TAG(HAL_INIT_STAGES_MISC31);

	rtw_write8(Adapter, REG_USB_HRPWM, 0);

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

VOID
hal_poweroff_8812au(
	IN	PADAPTER			Adapter
)
{
	u8	u1bTmp;
	u8 bMacPwrCtrlOn = _FALSE;
	u16	utemp, ori_fsmc0;

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if (bMacPwrCtrlOn == _FALSE)
		return ;

	ori_fsmc0 = utemp = rtw_read16(Adapter, REG_APS_FSMCO);
	rtw_write16(Adapter, REG_APS_FSMCO, utemp & ~0x8000);
	RTW_INFO(" %s\n", __FUNCTION__);

	/* Stop Tx Report Timer. 0x4EC[Bit1]=b'0 */
	u1bTmp = rtw_read8(Adapter, REG_TX_RPT_CTRL);
	rtw_write8(Adapter, REG_TX_RPT_CTRL, u1bTmp & (~BIT1));

	/* stop rx */
	rtw_write8(Adapter, REG_CR, 0x0);

	/* Run LPS WL RFOFF flow */
	if (IS_HARDWARE_TYPE_8821U(Adapter))
		HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8821A_NIC_LPS_ENTER_FLOW);
	else
		HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8812_NIC_LPS_ENTER_FLOW);

	if ((rtw_read8(Adapter, REG_MCUFWDL) & RAM_DL_SEL) &&
	    GET_HAL_DATA(Adapter)->bFWReady) /* 8051 RAM code */
		_8051Reset8812(Adapter);

	/* Reset MCU. Suggested by Filen. 2011.01.26. by tynli. */
	u1bTmp = rtw_read8(Adapter, REG_SYS_FUNC_EN + 1);
	rtw_write8(Adapter, REG_SYS_FUNC_EN + 1, (u1bTmp & (~BIT2)));

	/* MCUFWDL 0x80[1:0]=0				 */ /* reset MCU ready status */
	rtw_write8(Adapter, REG_MCUFWDL, 0x00);

	/* Card disable power action flow */
	if (IS_HARDWARE_TYPE_8821U(Adapter))
		HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8821A_NIC_DISABLE_FLOW);
	else
		HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK, Rtl8812_NIC_DISABLE_FLOW);

	bMacPwrCtrlOn = _FALSE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);

	GET_HAL_DATA(Adapter)->bFWReady = _FALSE;

	if (ori_fsmc0 & 0x8000) {
		utemp = rtw_read16(Adapter, REG_APS_FSMCO);
		rtw_write16(Adapter, REG_APS_FSMCO, utemp | 0x8000);
	}
}

static void rtl8812au_hw_power_down(_adapter *padapter)
{
	/* 2010/-8/09 MH For power down module, we need to enable register block contrl reg at 0x1c. */
	/* Then enable power down control bit of register 0x04 BIT4 and BIT15 as 1. */

	/* Enable register area 0x0-0xc. */
	rtw_write8(padapter, REG_RSV_CTRL, 0x0);
	rtw_write16(padapter, REG_APS_FSMCO, 0x8812);
}

u32 rtl8812au_hal_deinit(PADAPTER Adapter)
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

	rtw_write32(Adapter, REG_HISR0_8812, 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HISR1_8812, 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HIMR0_8812, IMR_DISABLED_8812);
	rtw_write32(Adapter, REG_HIMR1_8812, IMR_DISABLED_8812);

#ifdef SUPPORT_HW_RFOFF_DETECTED
	RTW_INFO("bkeepfwalive(%x)\n", pwrctl->bkeepfwalive);
	if (pwrctl->bkeepfwalive) {
		_ps_close_RF(Adapter);
		if ((pwrctl->bHWPwrPindetect) && (pwrctl->bHWPowerdown))
			rtl8812au_hw_power_down(Adapter);
	} else
#endif
	{
		if (rtw_is_hw_init_completed(Adapter)) {
			rtw_hal_power_off(Adapter);

			if ((pwrctl->bHWPwrPindetect) && (pwrctl->bHWPowerdown))
				rtl8812au_hw_power_down(Adapter);
		}
	}
	return _SUCCESS;
}


unsigned int rtl8812au_inirp_init(PADAPTER Adapter)
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
	if (pdev->RtInPipe[REALTEK_USB_IN_INT_EP_IDX] != 0x07) {
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

unsigned int rtl8812au_inirp_deinit(PADAPTER Adapter)
{

	rtw_read_port_cancel(Adapter);


	return _SUCCESS;
}

/* -------------------------------------------------------------------
 *
 *	EEPROM/EFUSE Content Parsing
 *
 * ------------------------------------------------------------------- */
VOID
hal_ReadIDs_8812AU(
	IN	PADAPTER	Adapter,
	IN	pu1Byte		PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		/* VID, PID */
		if (IS_HARDWARE_TYPE_8812AU(Adapter)) {
			pHalData->EEPROMVID = ReadLE2Byte(&PROMContent[EEPROM_VID_8812AU]);
			pHalData->EEPROMPID = ReadLE2Byte(&PROMContent[EEPROM_PID_8812AU]);
		} else if (IS_HARDWARE_TYPE_8821U(Adapter))

		{
			pHalData->EEPROMVID = ReadLE2Byte(&PROMContent[EEPROM_VID_8821AU]);
			pHalData->EEPROMPID = ReadLE2Byte(&PROMContent[EEPROM_PID_8821AU]);
		}


		/* Customer ID, 0x00 and 0xff are reserved for Realtek.		 */
		pHalData->EEPROMCustomerID = *(u8 *)&PROMContent[EEPROM_CustomID_8812];
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;

	} else {
		pHalData->EEPROMVID			= EEPROM_Default_VID;
		pHalData->EEPROMPID			= EEPROM_Default_PID;

		/* Customer ID, 0x00 and 0xff are reserved for Realtek.		 */
		pHalData->EEPROMCustomerID		= EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID	= EEPROM_Default_SubCustomerID;

	}

	if ((pHalData->EEPROMVID == 0x050D) && (pHalData->EEPROMPID == 0x1106)) /* SerComm for Belkin. */
		pHalData->CustomerID = RT_CID_819x_Sercomm_Belkin;
	else if ((pHalData->EEPROMVID == 0x0846) && (pHalData->EEPROMPID == 0x9051)) /* SerComm for Netgear. */
		pHalData->CustomerID = RT_CID_819x_Sercomm_Netgear;
	else if ((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x330e)) /* add by ylb 20121012 for customer led for alpha */
		pHalData->CustomerID = RT_CID_819x_ALPHA_Dlink;
	else if ((pHalData->EEPROMVID == 0x0B05) && (pHalData->EEPROMPID == 0x17D2)) /* Edimax for ASUS */
		pHalData->CustomerID = RT_CID_819x_Edimax_ASUS;
	else if ((pHalData->EEPROMVID == 0x0846) && (pHalData->EEPROMPID == 0x9052))
		pHalData->CustomerID = RT_CID_NETGEAR;
	else if ((pHalData->EEPROMVID == 0x0411) && ((pHalData->EEPROMPID == 0x0242) || (pHalData->EEPROMPID == 0x025D)))
		pHalData->CustomerID = RT_CID_DNI_BUFFALO;
	else if (((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3314)) ||
		((pHalData->EEPROMVID == 0x20F4) && (pHalData->EEPROMPID == 0x804B)) ||
		((pHalData->EEPROMVID == 0x20F4) && (pHalData->EEPROMPID == 0x805B)) ||
		((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3315)) ||
		((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3316)))
		pHalData->CustomerID = RT_CID_DLINK;

	RTW_INFO("VID = 0x%04X, PID = 0x%04X\n", pHalData->EEPROMVID, pHalData->EEPROMPID);
	RTW_INFO("Customer ID: 0x%02X, SubCustomer ID: 0x%02X\n", pHalData->EEPROMCustomerID, pHalData->EEPROMSubCustomerID);
}

VOID
hal_InitPGData_8812A(
	IN	PADAPTER		padapter,
	IN	OUT	u8			*PROMContent
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u32			i;
	u16			value16;

	if (_FALSE == pHalData->bautoload_fail_flag) {
		/* autoload OK. */
		if (is_boot_from_eeprom(padapter)) {
			/* Read all Content from EEPROM or EFUSE. */
			for (i = 0; i < HWSET_MAX_SIZE_JAGUAR; i += 2) {
				/* value16 = EF2Byte(ReadEEprom(pAdapter, (u2Byte) (i>>1))); */
				/* *((u16*)(&PROMContent[i])) = value16; */
			}
		} else {
			/*  */
			/* 2013/03/08 MH Add for 8812A HW limitation, ROM code can only */
			/*  */
			if (IS_HARDWARE_TYPE_8812AU(padapter)) {
				u8	efuse_content[4];
				efuse_OneByteRead(padapter, 0x200, &efuse_content[0], _FALSE);
				efuse_OneByteRead(padapter, 0x202, &efuse_content[1], _FALSE);
				efuse_OneByteRead(padapter, 0x204, &efuse_content[2], _FALSE);
				efuse_OneByteRead(padapter, 0x210, &efuse_content[3], _FALSE);
				if (efuse_content[0] != 0xFF ||
				    efuse_content[1] != 0xFF ||
				    efuse_content[2] != 0xFF ||
				    efuse_content[3] != 0xFF) {
					/* DbgPrint("Disable FW ofl load\n"); */
					/* pMgntInfo->RegFWOffload = FALSE; */
				}
			}

			/* Read EFUSE real map to shadow. */
			EFUSE_ShadowMapUpdate(padapter, EFUSE_WIFI, _FALSE);
		}
	} else {
		/* autoload fail */
		/* pHalData->AutoloadFailFlag = _TRUE; */
		/*  */
		/* 2013/03/08 MH Add for 8812A HW limitation, ROM code can only */
		/*  */
		if (IS_HARDWARE_TYPE_8812AU(padapter)) {
			u8	efuse_content[4];
			efuse_OneByteRead(padapter, 0x200, &efuse_content[0], _FALSE);
			efuse_OneByteRead(padapter, 0x202, &efuse_content[1], _FALSE);
			efuse_OneByteRead(padapter, 0x204, &efuse_content[2], _FALSE);
			efuse_OneByteRead(padapter, 0x210, &efuse_content[3], _FALSE);
			if (efuse_content[0] != 0xFF ||
			    efuse_content[1] != 0xFF ||
			    efuse_content[2] != 0xFF ||
			    efuse_content[3] != 0xFF) {
				/* DbgPrint("Disable FW ofl load\n"); */
				/* pMgntInfo->RegFWOffload = FALSE; */
				pHalData->bautoload_fail_flag = _FALSE;
			} else {
				/* DbgPrint("EFUSE_Read1Byte(pAdapter, (u2Byte)512) = %x\n", EFUSE_Read1Byte(pAdapter, (u2Byte)512)); */
			}
		}

		/* update to default value 0xFF */
		if (!is_boot_from_eeprom(padapter))
			EFUSE_ShadowMapUpdate(padapter, EFUSE_WIFI, _FALSE);
	}

#ifdef CONFIG_EFUSE_CONFIG_FILE
	if (check_phy_efuse_tx_power_info_valid(padapter) == _FALSE) {
		if (Hal_readPGDataFromConfigFile(padapter) != _SUCCESS)
			RTW_ERR("invalid phy efuse and read from file fail, will use driver default!!\n");
	}
#endif

}

VOID
hal_CustomizedBehavior_8812AU(
	IN	PADAPTER	Adapter
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

	case RT_CID_819x_HP:
		pledpriv->LedStrategy = SW_LED_MODE6; /* Customize Led mode */
		break;

	case RT_CID_819x_Sercomm_Belkin:
		pledpriv->LedStrategy = SW_LED_MODE9;
		break;

	case RT_CID_819x_Sercomm_Netgear:
		pledpriv->LedStrategy = SW_LED_MODE10;
		break;

	case RT_CID_819x_ALPHA_Dlink: /* add by ylb 20121012 for customer led for alpha */
		pledpriv->LedStrategy = SW_LED_MODE1;
		break;

	case RT_CID_819x_Edimax_ASUS:
		pledpriv->LedStrategy = SW_LED_MODE11;
		break;

	case RT_CID_WNC_NEC:
		pledpriv->LedStrategy = SW_LED_MODE12;
		break;

	case RT_CID_NETGEAR:
		pledpriv->LedStrategy = SW_LED_MODE13;
		break;

	case RT_CID_DNI_BUFFALO:
		pledpriv->LedStrategy = SW_LED_MODE14;
		break;

	case RT_CID_DLINK:
		pledpriv->LedStrategy = SW_LED_MODE15;
		break;

	default:
		pledpriv->LedStrategy = SW_LED_MODE9;
		break;
	}

	pHalData->bLedOpenDrain = _TRUE;/* Support Open-drain arrangement for controlling the LED. Added by Roger, 2009.10.16. */
#endif
}

static void
hal_CustomizeByCustomerID_8812AU(
	IN	PADAPTER		pAdapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	/* For customized behavior. */
	if ((pHalData->EEPROMVID == 0x103C) && (pHalData->EEPROMPID == 0x1629)) /* HP Lite-On for RTL8188CUS Slim Combo. */
		pHalData->CustomerID = RT_CID_819x_HP;
	else if ((pHalData->EEPROMVID == 0x9846) && (pHalData->EEPROMPID == 0x9041))
		pHalData->CustomerID = RT_CID_NETGEAR;
	else if ((pHalData->EEPROMVID == 0x2019) && (pHalData->EEPROMPID == 0x1201))
		pHalData->CustomerID = RT_CID_PLANEX;
	else if ((pHalData->EEPROMVID == 0x0BDA) && (pHalData->EEPROMPID == 0x5088))
		pHalData->CustomerID = RT_CID_CC_C;
	else if ((pHalData->EEPROMVID == 0x0411) && ((pHalData->EEPROMPID == 0x0242) || (pHalData->EEPROMPID == 0x025D)))
		pHalData->CustomerID = RT_CID_DNI_BUFFALO;
	else if (((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3314)) ||
		((pHalData->EEPROMVID == 0x20F4) && (pHalData->EEPROMPID == 0x804B)) ||
		((pHalData->EEPROMVID == 0x20F4) && (pHalData->EEPROMPID == 0x805B)) ||
		((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3315)) ||
		((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3316)))
		pHalData->CustomerID = RT_CID_DLINK;

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
		else if ((pHalData->EEPROMVID == 0x0BFF) && (pHalData->EEPROMPID == 0x8160)) {
			/* pHalData->bAutoConnectEnable = _FALSE; */
			pHalData->CustomerID = RT_CID_CHINA_MOBILE;
		} else if ((pHalData->EEPROMVID == 0x0BDA) &&	(pHalData->EEPROMPID == 0x5088))
			pHalData->CustomerID = RT_CID_CC_C;
		else if ((pHalData->EEPROMVID == 0x0846) && (pHalData->EEPROMPID == 0x9052))
			pHalData->CustomerID = RT_CID_NETGEAR;
		else if ((pHalData->EEPROMVID == 0x0411) && ((pHalData->EEPROMPID == 0x0242) || (pHalData->EEPROMPID == 0x025D)))
			pHalData->CustomerID = RT_CID_DNI_BUFFALO;
		else if (((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3314)) ||
			((pHalData->EEPROMVID == 0x20F4) && (pHalData->EEPROMPID == 0x804B)) ||
			((pHalData->EEPROMVID == 0x20F4) && (pHalData->EEPROMPID == 0x805B)) ||
			((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3315)) ||
			((pHalData->EEPROMVID == 0x2001) && (pHalData->EEPROMPID == 0x3316)))
			pHalData->CustomerID = RT_CID_DLINK;
		RTW_INFO("PID= 0x%x, VID=  %x\n", pHalData->EEPROMPID, pHalData->EEPROMVID);
		break;
	case EEPROM_CID_WHQL:
		/* padapter->bInHctTest = TRUE; */

		/* pMgntInfo->bSupportTurboMode = FALSE; */
		/* pMgntInfo->bAutoTurboBy8186 = FALSE; */

		/* pMgntInfo->PowerSaveControl.bInactivePs = FALSE; */
		/* pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE; */
		/* pMgntInfo->PowerSaveControl.bLeisurePs = FALSE; */
		/* pMgntInfo->PowerSaveControl.bLeisurePsModeBackup = FALSE; */
		/* pMgntInfo->keepAliveLevel = 0; */

		/* padapter->bUnloadDriverwhenS3S4 = FALSE; */
		break;
	default:
		pHalData->CustomerID = RT_CID_DEFAULT;
		break;

	}
	RTW_INFO("Customer ID: 0x%2x\n", pHalData->CustomerID);

	hal_CustomizedBehavior_8812AU(pAdapter);
}

VOID
hal_ReadUsbModeSwitch_8812AU(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);

	if (AutoloadFail)
		pHalData->EEPROMUsbSwitch = _FALSE;
	else
		/* check efuse 0x08 bit2 */
		pHalData->EEPROMUsbSwitch = (PROMContent[EEPROM_USB_MODE_8812] & BIT1) >> 1;

	RTW_INFO("Usb Switch: %d\n", pHalData->EEPROMUsbSwitch);
}

static VOID
ReadLEDSetting_8812AU(
	IN	PADAPTER	Adapter,
	IN	u8		*PROMContent,
	IN	BOOLEAN		AutoloadFail
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

VOID
InitAdapterVariablesByPROM_8812AU(
	IN	PADAPTER	Adapter
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);

	hal_InitPGData_8812A(Adapter, pHalData->efuse_eeprom_data);

	Hal_EfuseParseIDCode8812A(Adapter, pHalData->efuse_eeprom_data);

	Hal_ReadPROMVersion8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadIDs_8812AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_config_macaddr(Adapter, pHalData->bautoload_fail_flag);
	Hal_ReadTxPowerInfo8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadBoardType8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	/*  */
	/* Read Bluetooth co-exist and initialize */
	/*  */
	Hal_EfuseParseBTCoexistInfo8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	Hal_ReadChannelPlan8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseXtal_8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadThermalMeter_8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadRemoteWakeup_8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadAntennaDiversity8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	if (IS_HARDWARE_TYPE_8821U(Adapter))
		Hal_ReadPAType_8821A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	else {
		Hal_ReadAmplifierType_8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
		Hal_ReadRFEType_8812A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	}

	hal_ReadUsbModeSwitch_8812AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_CustomizeByCustomerID_8812AU(Adapter);

	ReadLEDSetting_8812AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	/* 2013/04/15 MH Add for different board type recognize. */
	hal_ReadUsbType_8812AU(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	if (IS_HARDWARE_TYPE_8821U(Adapter))
		Hal_EfuseParseKFreeData_8821A(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	/* set coex. ant info once efuse parsing is done */
	rtw_btcoex_set_ant_info(Adapter);
}

static void Hal_ReadPROMContent_8812A(
	IN PADAPTER		Adapter
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

	InitAdapterVariablesByPROM_8812AU(Adapter);
}

u8
ReadAdapterInfo8812AU(
	IN PADAPTER			Adapter
)
{
	/* Read all content in Efuse/EEPROM. */
	Hal_ReadPROMContent_8812A(Adapter);

	/* We need to define the RF type after all PROM value is recognized. */
	ReadRFType8812A(Adapter);

	return _SUCCESS;
}

void UpdateInterruptMask8812AU(PADAPTER padapter, u8 bHIMR0 , u32 AddMSR, u32 RemoveMSR)
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
		rtw_write32(padapter, REG_HIMR0_8812, *himr);
	else
		rtw_write32(padapter, REG_HIMR1_8812, *himr);

}

u8 SetHwReg8812AU(PADAPTER Adapter, u8 variable, u8 *val)
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

			/*rpwm value only use BIT0(clock bit) ,BIT6(Ack bit), and BIT7(Toggle bit) for 88e.
			BIT0 value - 1: 32k, 0:40MHz.
			BIT6 value - 1: report cpwm value after success set, 0:do not report.
			BIT7 value - Toggle bit change.
			modify by Thomas. 2012/4/2.*/
			ps_state = ps_state & 0xC1;
			/*RTW_INFO("##### Change RPWM value to = %x for switch clk #####\n", ps_state);*/
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
		ret = SetHwReg8812A(Adapter, variable, val);
		break;
	}

	return ret;
}

void GetHwReg8812AU(PADAPTER Adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	switch (variable) {
	case HW_VAR_CPWM:
#ifdef CONFIG_LPS_LCLK
		*val = rtw_read8(Adapter, REG_USB_HCPWM);
		/*
			RTW_INFO("##### REG_USB_HCPWM(0x%02x) = 0x%02x #####\n",
			REG_USB_HCPWM, *val);
		*/
#endif /* CONFIG_LPS_LCLK */
		break;
	case HW_VAR_RPWM_TOG:
#ifdef CONFIG_LPS_LCLK
		*val = rtw_read8(Adapter, REG_USB_HRPWM) & BIT7;
#endif /* CONFIG_LPS_LCLK */
		break;
	default:
		GetHwReg8812A(Adapter, variable, val);
		break;
	}

}

/*
 *	Description:
 *		Change default setting of specified variable.
 *   */
u8
SetHalDefVar8812AUsb(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch (eVariable) {
	default:
		SetHalDefVar8812A(Adapter, eVariable, pValue);
		break;
	}

	return bResult;
}

/*
 *	Description:
 *		Query setting of specified variable.
 *   */
u8
GetHalDefVar8812AUsb(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch (eVariable) {
	default:
		GetHalDefVar8812A(Adapter, eVariable, pValue);
		break;
	}

	return bResult;
}

void _update_response_rate(_adapter *padapter, unsigned int mask)
{
	u8	RateIndex = 0;
	/* Set RRSR rate table. */
	rtw_write8(padapter, REG_RRSR, mask & 0xff);
	rtw_write8(padapter, REG_RRSR + 1, (mask >> 8) & 0xff);

	/* Set RTS initial rate */
	while (mask > 0x1) {
		mask = (mask >> 1);
		RateIndex++;
	}
	rtw_write8(padapter, REG_INIRTS_RATE_SEL, RateIndex);
}

static void rtl8812au_init_default_value(_adapter *padapter)
{
	PHAL_DATA_TYPE pHalData;

	pHalData = GET_HAL_DATA(padapter);

	InitDefaultValue8821A(padapter);

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

static u8 rtl8812au_ps_func(PADAPTER Adapter, HAL_INTF_PS_FUNC efunc_id, u8 *val)
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

void rtl8812au_set_hal_ops(_adapter *padapter)
{
	struct hal_ops	*pHalFunc = &padapter->hal_func;


	pHalFunc->hal_power_on = _InitPowerOn_8812AU;
	pHalFunc->hal_power_off = hal_poweroff_8812au;

	pHalFunc->hal_init = &rtl8812au_hal_init;
	pHalFunc->hal_deinit = &rtl8812au_hal_deinit;

	pHalFunc->inirp_init = &rtl8812au_inirp_init;
	pHalFunc->inirp_deinit = &rtl8812au_inirp_deinit;

	pHalFunc->init_xmit_priv = &rtl8812au_init_xmit_priv;
	pHalFunc->free_xmit_priv = &rtl8812au_free_xmit_priv;

	pHalFunc->init_recv_priv = &rtl8812au_init_recv_priv;
	pHalFunc->free_recv_priv = &rtl8812au_free_recv_priv;
#ifdef CONFIG_RTW_SW_LED
	pHalFunc->InitSwLeds = &rtl8812au_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8812au_DeInitSwLeds;
#endif/* CONFIG_RTW_SW_LED */

	pHalFunc->init_default_value = &rtl8812au_init_default_value;
	pHalFunc->intf_chip_configure = &rtl8812au_interface_configure;
	pHalFunc->read_adapter_info = &ReadAdapterInfo8812AU;

	pHalFunc->set_hw_reg_handler = &SetHwReg8812AU;
	pHalFunc->GetHwRegHandler = &GetHwReg8812AU;
	pHalFunc->get_hal_def_var_handler = &GetHalDefVar8812AUsb;
	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8812AUsb;

	pHalFunc->hal_xmit = &rtl8812au_hal_xmit;
	pHalFunc->mgnt_xmit = &rtl8812au_mgnt_xmit;
	pHalFunc->hal_xmitframe_enqueue = &rtl8812au_hal_xmitframe_enqueue;

#ifdef CONFIG_HOSTAPD_MLME
	pHalFunc->hostap_mgnt_xmit_entry = &rtl8812au_hostap_mgnt_xmit_entry;
#endif
	pHalFunc->interface_ps_func = &rtl8812au_ps_func;
#ifdef CONFIG_XMIT_THREAD_MODE
	pHalFunc->xmit_thread_handler = &rtl8812au_xmit_buf_handler;
#endif
#ifdef CONFIG_SUPPORT_USB_INT
	pHalFunc->interrupt_handler = interrupt_handler_8812au;
#endif

	rtl8812_set_hal_ops(pHalFunc);

}
