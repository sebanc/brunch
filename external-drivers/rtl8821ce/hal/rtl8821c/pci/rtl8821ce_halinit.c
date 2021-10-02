/******************************************************************************
 *
 * Copyright(c) 2016 - 2017 Realtek Corporation.
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

#define _RTL8821CE_HALINIT_C_
#include <drv_types.h>          /* PADAPTER, basic_types.h and etc. */
#include <hal_data.h>		/* HAL_DATA_TYPE */
#include "../rtl8821c.h"
#include "rtl8821ce.h"

u32 InitMAC_TRXBD_8821CE(PADAPTER Adapter)
{
	u8 tmpU1b;
	u16 tmpU2b;
	u32 tmpU4b;
	int q_idx;
	struct recv_priv *precvpriv = &Adapter->recvpriv;
	struct xmit_priv *pxmitpriv = &Adapter->xmitpriv;
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	RTW_INFO("=======>InitMAC_TXBD_8821CE()\n");

	/*
	 * Set CMD TX BD (buffer descriptor) physical address(from OS API).
	 */
	rtw_write32(Adapter, REG_H2CQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[TXCMD_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_H2CQ_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE_CMD | ((RTL8821CE_SEG_NUM << 12) &
					 0x3000));

#ifdef CONFIG_64BIT_DMA
	rtw_write32(Adapter, REG_H2CQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[TXCMD_QUEUE_INX].dma) >> 32);
#endif
	/*
	 * Set TX/RX BD (buffer descriptor) physical address(from OS API).
	 */
	rtw_write32(Adapter, REG_BCNQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_MGQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VOQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VIQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BEQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));

	/* vincent sync windows */
	tmpU4b = rtw_read32(Adapter, REG_BEQ_TXBD_DESA_8821C);

	rtw_write32(Adapter, REG_BKQ_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_HI0Q_TXBD_DESA_8821C,
		    (u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma &
		    DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_RXQ_RXBD_DESA_8821C,
		    (u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma &
		    DMA_BIT_MASK(32));

#ifdef CONFIG_64BIT_DMA
	/*
	 * 2009/10/28 MH For DMA 64 bits. We need to assign the high
	 * 32 bit address for NIC HW to transmit data to correct path.
	 */
	rtw_write32(Adapter, REG_BCNQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_MGQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_VOQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_VIQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_BEQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_BKQ_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_HI0Q_TXBD_DESA_8821C + 4,
		    ((u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_RXQ_RXBD_DESA_8821C + 4,
		    ((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma) >> 32);

#if 0
	/* 2009/10/28 MH If RX descriptor address is not equal to zero.
	* We will enable DMA 64 bit functuion.
	* Note: We never saw thd consition which the descripto address are
	*	divided into 4G down and 4G upper separate area.
	*/
	if (((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma) >> 32 != 0) {
		RTW_INFO("Enable DMA64 bit\n");

		/* Check if other descriptor address is zero and
		* abnormally be in 4G lower area.
		*/
		if (((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma) >> 32)
			RTW_INFO("MGNT_QUEUE HA=0\n");

	} else
		RTW_INFO("Enable DMA32 bit\n");
#endif
	if (adapter_to_dvobj(Adapter)->bdma64)	
		PlatformEnableDMA64(Adapter);
#endif

	/* pci buffer descriptor mode: Reset the Read/Write point to 0 */
	PlatformEFIOWrite4Byte(Adapter, REG_TSFTIMER_HCI_8821C, 0x3fffffff);

	/* Reset the H2CQ R/W point index to 0 */
	tmpU4b = rtw_read32(Adapter, REG_H2CQ_CSR_8821C);
	rtw_write32(Adapter, REG_H2CQ_CSR_8821C, (tmpU4b | BIT8 | BIT16));

	tmpU1b = rtw_read8(Adapter, REG_PCIE_CTRL + 3);
	rtw_write8(Adapter, REG_PCIE_CTRL + 3, (tmpU1b | 0xF7));

	/* 20100318 Joseph: Reset interrupt migration setting
	* when initialization. Suggested by SD1
	*/

	rtw_write32(Adapter, REG_INT_MIG, 0);
	pHalData->bInterruptMigration = _FALSE;

	/* 2009.10.19. Reset H2C protection register. by tynli. */
	rtw_write32(Adapter, REG_MCUTST_I_8821C, 0x0);

#if MP_DRIVER == 1
	if (Adapter->registrypriv.mp_mode == 1) {
		rtw_write32(Adapter, REG_MACID, 0x87654321);
		rtw_write32(Adapter, 0x0700, 0x87654321);
	}
#endif

	/* pic buffer descriptor mode: */
	/* ---- tx */
	rtw_write16(Adapter, REG_MGQ_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_VOQ_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_VIQ_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_BEQ_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_BKQ_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI0Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI1Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI2Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI3Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI4Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI5Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI6Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI7Q_TXBD_NUM_8821C,
		    TX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 12) & 0x3000));


	/* rx. support 32 bits in linux */

#ifdef CONFIG_64BIT_DMA
	/* using 64bit */
	rtw_write16(Adapter, REG_RX_RXBD_NUM_8821C,
		    RX_BD_NUM_8821CE |((RTL8821CE_SEG_NUM<<13 ) & 0x6000) |0x8000);
#else
	/* using 32bit */
	rtw_write16(Adapter, REG_RX_RXBD_NUM_8821C,
		    RX_BD_NUM_8821CE | ((RTL8821CE_SEG_NUM << 13) & 0x6000));
#endif

	/* reset read/write point */
	rtw_write32(Adapter, REG_TSFTIMER_HCI_8821C, 0XFFFFFFFF);

#if 1 /* vincent windows */
	/* Start debug mode */
	{
		u8 reg0x3f3 = 0;

		reg0x3f3 = rtw_read8(Adapter, 0x3f3);
		rtw_write8(Adapter, 0x3f3, reg0x3f3 | BIT2);
	}

	{
	/* Need to disable BT coex to let MP tool Tx, this would be done in FW
	*  in the future, suggest by ChunChu, 2015.05.19
	*/

		u8 tmp1Byte;
		u16 tmp2Byte;
		u32 tmp4Byte;

		tmp2Byte = rtw_read16(Adapter, REG_SYS_FUNC_EN_8821C);
		rtw_write16(Adapter, REG_SYS_FUNC_EN_8821C, tmp2Byte | BIT10);
		tmp1Byte = rtw_read8(Adapter, REG_DIS_TXREQ_CLR_8821C);
		rtw_write8(Adapter, REG_DIS_TXREQ_CLR_8821C, tmp1Byte | BIT7);
		tmp4Byte = rtw_read32(Adapter, 0x1080);
		rtw_write32(Adapter, 0x1080, tmp4Byte | BIT16);
	}
#endif

	RTW_INFO("InitMAC_TXBD_8821CE() <====\n");

	return _SUCCESS;
}

u32 rtl8821ce_hal_init(PADAPTER padapter)
{
	u8 ok = _TRUE;
	u8 val8;
	PHAL_DATA_TYPE hal;
	struct registry_priv  *registry_par;

	hal = GET_HAL_DATA(padapter);
	registry_par = &padapter->registrypriv;

	InitMAC_TRXBD_8821CE(padapter);

	ok = rtl8821c_hal_init(padapter);
	if (ok == _FALSE)
		return _FAIL;

#if defined(USING_RX_TAG)
	/* have to init after halmac init */
	val8 = rtw_read8(padapter, REG_PCIE_CTRL_8821C + 2);
	rtw_write8(padapter, REG_PCIE_CTRL_8821C + 2, (val8 | BIT4));
	rtw_write16(padapter, REG_PCIE_CTRL_8821C, 0x8000);
#else
	rtw_write16(padapter, REG_PCIE_CTRL_8821C, 0x0000);
#endif

	rtw_write8(padapter, REG_RX_DRVINFO_SZ_8821C, 0x4);

	/* MAX AMPDU Number = 43, Reg0x4C8[21:16] = 0x2B */
	rtw_write16(padapter, REG_PROT_MODE_CTRL_8821C + 2, 0x2B2B);

	hal->pci_backdoor_ctrl = registry_par->pci_aspm_config;

	rtw_pci_aspm_config(padapter);

	return _SUCCESS;
}

void rtl8821ce_init_default_value(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;


	pHalData = GET_HAL_DATA(padapter);

	rtl8821c_init_default_value(padapter);

	/* interface related variable */
	pHalData->CurrentWirelessMode = WIRELESS_MODE_AUTO;
	pHalData->bDefaultAntenna = 1;
	pHalData->TransmitConfig = BIT_CFEND_FORMAT | BIT_WMAC_TCR_ERRSTEN_3;

	/* Set RCR-Receive Control Register .
	 * The value is set in InitializeAdapter8190Pci().
	 */
	pHalData->ReceiveConfig = (
#ifdef CONFIG_RX_PACKET_APPEND_FCS
				BIT_APP_FCS		|
#endif
				BIT_APP_MIC			|
				BIT_APP_ICV			|
				BIT_APP_PHYSTS		|
				BIT_VHT_DACK		|
				BIT_HTC_LOC_CTRL	|
				/* BIT_AMF		| */
				BIT_CBSSID_DATA		|
				BIT_CBSSID_BCN		|
				/* BIT_ACF		| */
				/* BIT_ADF		|  PS-Poll filter */
				BIT_AB				|
				BIT_AB				|
				BIT_APM				|
				0);

	/*
	 * Set default value of Interrupt Mask Register0
	 */
	pHalData->IntrMaskDefault[0] = (u32)(
				BIT(29)			| /* BIT_PSTIMEOUT */
				BIT(27)			| /* BIT_GTINT3 */
				BIT_TXBCN0ERR_MSK	|
				BIT_TXBCN0OK_MSK	|
				BIT_BCNDMAINT0_MSK	|
				BIT_HSISR_IND_ON_INT_MSK |
				BIT_C2HCMD_MSK		|
	#ifdef CONFIG_LPS_LCLK
				BIT_CPWM_MSK		|
	#endif
				BIT_HIGHDOK_MSK	|
				BIT_MGTDOK_MSK		|
				BIT_BKDOK_MSK		|
				BIT_BEDOK_MSK		|
				BIT_VIDOK_MSK		|
				BIT_VODOK_MSK		|
				BIT_RDU_MSK		|
				BIT_RXOK_MSK		|
				0);

	/*
	 * Set default value of Interrupt Mask Register1
	 */
	pHalData->IntrMaskDefault[1] = (u32)(
				BIT(9)		| /* TXFOVW */
				BIT_FOVW_MSK	|
				BIT_PRETXERR_HANDLE_IMR |
				0);

	/*
	 * Set default value of Interrupt Mask Register3
	 */
	pHalData->IntrMaskDefault[3] = (u32)(
				BIT_SETH2CDOK_MASK	| /* H2C_TX_OK */
				0);

	/* 2012/03/27 hpfan Add for win8 DTM DPC ISR test */
	pHalData->IntrMaskReg[0] = (u32)(
				BIT_RDU_MSK	|
				BIT(29)		| /* BIT_PSTIMEOUT */
				0);

	pHalData->IntrMaskReg[1] = (u32)(
					   BIT_C2HCMD_MSK	|
					   0);

	pHalData->IntrMask[0] = pHalData->IntrMaskDefault[0];
	pHalData->IntrMask[1] = pHalData->IntrMaskDefault[1];
	pHalData->IntrMask[3] = pHalData->IntrMaskDefault[3];

}

static void hal_deinit_misc(PADAPTER padapter)
{

}

u32 rtl8821ce_hal_deinit(PADAPTER padapter)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv *pobj_priv = adapter_to_dvobj(padapter);
	u8 status = _TRUE;

	RTW_INFO("==> %s\n", __func__);


	hal_deinit_misc(padapter);
	status = rtl8821c_hal_deinit(padapter);
	if (status == _FALSE) {
		RTW_INFO("%s: rtl8821c_hal_deinit fail\n", __func__);
		return _FAIL;
	}

	RTW_INFO("%s <==\n", __func__);
	return _SUCCESS;
}
