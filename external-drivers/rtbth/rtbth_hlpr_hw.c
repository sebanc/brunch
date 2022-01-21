/*
*************************************************************************
* Ralink Technology Corporation
* 5F., No. 5, Taiyuan 1st St., Jhubei City,
* Hsinchu County 302,
* Taiwan, R.O.C.
*
* (c) Copyright 2012, Ralink Technology Corporation
*
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the                         *
* Free Software Foundation, Inc.,                                       *
* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
*                                                                       *
*************************************************************************/

#include "rtbt_hal.h"

VOID RtbtReadModemRegister(
	IN PRTBTH_ADAPTER pAd,
	IN UCHAR		  BbpID,
	IN PUCHAR   	  pValue)
{
	MDM_CTRL_STRUC  MdmCtrlCsr;
	MDM_RDATA_STRUC MdmRDataCsr;

	MdmCtrlCsr.word = 0;
	MdmRDataCsr.word = 0;
	
	MdmCtrlCsr.field.MdmWrite = 0;  // 0: Read
	MdmCtrlCsr.field.MdmAddr = BbpID;

	RT_IO_WRITE32(pAd, MDM_CTRL, MdmCtrlCsr.word);
	RT_IO_READ32(pAd, MDM_RDATA, &MdmRDataCsr.word);
	*(pValue) = (UCHAR)MdmRDataCsr.field.MdmRData;
}


VOID RtbtWriteModemRegister(
	IN PRTBTH_ADAPTER pAd,
	IN UCHAR		  BbpID,
	IN UCHAR		  Value)
{
	MDM_CTRL_STRUC  MdmCtrlCsr;

	MdmCtrlCsr.word = 0;

	MdmCtrlCsr.field.MdmWrite = 1; // 1: Write
	MdmCtrlCsr.field.MdmAddr = BbpID;
	MdmCtrlCsr.field.MdmData = Value;

	RT_IO_WRITE32(pAd, MDM_CTRL, MdmCtrlCsr.word);
}

VOID BthInitializeAsic(IN RTBTH_ADAPTER *pAd)
{
	ASIC_VER_ID_STRUC	AsicVerId;
	ECO_CTL_CSR_STRUC	ECOCtrCsr;
	/*UCHAR				Num;*/
	UINT8 AfhOffset;
	UINT32				Value;
	CMB_CTRL_STRUC		CmbCtrl;

	
	DebugPrint(TRACE, DBG_INIT,"-->%s()\n", __FUNCTION__);

	/* Initialize the seed of PRBS */
	RT_IO_WRITE32(pAd, PRBS_SEED, 0x00F07FFF);

	/* Set logical link and channel information table location */
	RT_IO_WRITE32(pAd, PBF_CFG, 0x00004400);

	/* AFH channel info */
	AfhOffset = (MCU_CH_INFO_ADDR - MCU_CH_INFO_BASE_ADDR) / AFH_CFG_OFFSET;
	Value = ((AfhOffset + 3 * 2) << 24) | ((AfhOffset + 2 * 2) << 16) | ((AfhOffset + 1 * 2) << 8) | ((AfhOffset + 0 * 2) << 0);
	RT_IO_WRITE32(pAd, AFH_CFG0_CSR, Value);

	Value = ((AfhOffset + 7 * 2) << 24) | ((AfhOffset + 6 * 2) << 16) | ((AfhOffset + 5 * 2) << 8) | ((AfhOffset + 4 * 2) << 0);
	RT_IO_WRITE32(pAd, AFH_CFG1_CSR, Value);

	
	AsicVerId.word = pAd->MACVersion;
	if (AsicVerId.field.ASICVer == ASIC_VER_RT3290 &&
		AsicVerId.field.ASICRev < ASIC_REV_C)
	{
		/* BBP packet dectection threshold uses lower value to avoid RX_EN issue */
		RtbtWriteModemRegister(pAd, 0x10, 0x35);

		pAd->bLESupported = FALSE;
		pAd->bHwTimerSupported = FALSE;

		pAd->hwCodecSupport = FALSE;
		DebugPrint(TRACE, DBG_INIT, "SW CVSD Codec\n");
	}
	else if (AsicVerId.field.ASICVer == ASIC_VER_RT3290 && AsicVerId.field.ASICRev < ASIC_REV_LE)
	{
		/* BBP packet dectection threshold uses the normal value */
		RtbtWriteModemRegister(pAd, 0x10, 0x37);

		/* Enable EDR CRC check for Len 0, and notify firmware */
		RT_IO_READ32(pAd, ECO_CTL_CSR, &ECOCtrCsr.word);
		ECOCtrCsr.field.EDRLen0CRCEn = 1;
		RT_IO_WRITE32(pAd, ECO_CTL_CSR, ECOCtrCsr.word);

		pAd->bLESupported = FALSE;

		pAd->bHwTimerSupported = FALSE;
		pAd->hwCodecSupport = FALSE;
		DebugPrint(TRACE, DBG_INIT, "SW CVSD Codec\n");
	}
	else
	{
		/* BBP packet dectection threshold uses the normal value */
		RtbtWriteModemRegister(pAd, 0x10, 0x37);

		/* Enable EDR CRC check for Len 0, and notify firmware */
		RT_IO_READ32(pAd, ECO_CTL_CSR, &ECOCtrCsr.word);
		ECOCtrCsr.field.EDRLen0CRCEn = 1;
		RT_IO_WRITE32(pAd, ECO_CTL_CSR, ECOCtrCsr.word);

		pAd->bLESupported = TRUE;

		/* Enable HW timer */
		CmbCtrl.word = 0;
		RT_IO_READ32(pAd, CMB_CTRL, &CmbCtrl.word);
		CmbCtrl.field.AuxOpt |= AUX_OPT_INTF_CLK_MASK;
		RT_IO_WRITE32(pAd, CMB_CTRL, CmbCtrl.word);

		pAd->bHwTimerSupported = TRUE;

		pAd->hwCodecSupport = TRUE;
		DebugPrint(TRACE, DBG_INIT, "HW CVSD Codec\n");
	}
	
	DebugPrint(TRACE, DBG_INIT,"<--%s()\n", __FUNCTION__);
}

VOID BthDisableBtFunc(IN RTBTH_ADAPTER *pAd)
{
	ULONG   	MacValue;

	// per Max's request, leave bit[2](BT_RF_EN) on, to fix Ch 13 issue
	pAd->btFunCtrl.word &= ~(0x1);
	//
	// Reset the whole bluetooth(PMDA, LC, MCU, ...) except RF
	//
	RT_IO_FORCE_READ32(pAd, BT_FUN_CTRL, &MacValue);
        MacValue &= ~(0x1);
	RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, MacValue);
	DebugPrint(TRACE, DBG_INIT, "<== BthDisableBtFunc\n");
}

VOID BthEnableBtFunc(IN RTBTH_ADAPTER *pAd)
{
	ULONG   	MacValue = pAd->btFunCtrl.word;
	ULONG   	i = 0;

	DebugPrint(TRACE, DBG_INIT, "==> BthEnableBtFunc, pAd->btFunCtrl=0x%x\n", pAd->btFunCtrl);

	if (MacValue & 0x01)
		return;
	else
	{
		MacValue |= 0x5;
		MacValue &= (~0x2);
		RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, MacValue);
		pAd->btFunCtrl.word |=0x5;
		pAd->btFunCtrl.word &= (~0x2);
	}

	do
	{
		// Check PLL stable.
		i++;
		MacValue = 0;
		RT_IO_READ32(pAd, CMB_CTRL, &MacValue);

		if (i > 1000)
			break;

		if ((MacValue&0xc00000) != 0)
			break;

	} while(TRUE);

	pAd->btFunCtrl.word |=0x7;
	RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, pAd->btFunCtrl.word);
	DebugPrint(TRACE, DBG_INIT, "<== BthEnableBtFunc %d\n", i);
}

int BthWaitForDmaIdle(IN RTBTH_ADAPTER *pAd, IN int wait_cnt)
{
	WPDMA_GLO_CFG_STRUC GloCfg;
	int cnt = 0;
	
	do
	{
		RT_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);
		if ((GloCfg.field.TxDMABusy == 0) && (GloCfg.field.RxDMABusy == 0))
			return TRUE;
		cnt++;
		KeStallExecutionProcessor(50);
	}while(cnt < wait_cnt);

	if (GloCfg.field.TxDMABusy || GloCfg.field.RxDMABusy)
	{
		DebugPrint(ERROR, DBG_INIT, "PDMA Busy(Tx=%d, Rx=%d)!\n",
				   GloCfg.field.TxDMABusy,
				   GloCfg.field.RxDMABusy);
	}
			
	return FALSE;
}

VOID BthDmaCfg(
	IN RTBTH_ADAPTER *pAd,
	IN ULONG DmaCfgMode)
{
	WPDMA_GLO_CFG_STRUC GloCfg;
	ULONG   	Count;

	GloCfg.word = 0;
	Count = 0;
	switch (DmaCfgMode)
	{
		case 0:
			// Disable Tx/Rx PDMA first
			RT_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);
			GloCfg.field.EnableRxDMA = 0;
			GloCfg.field.EnableTxDMA = 0;
			RT_IO_WRITE32(pAd, WPDMA_GLO_CFG, GloCfg.word);

			BthWaitForDmaIdle(pAd, 200);

			RT_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);
			GloCfg.word &= 0xff0;
			RT_IO_WRITE32(pAd, WPDMA_GLO_CFG, GloCfg.word);
			DebugPrint(TRACE, DBG_INIT, "Disable Dma\n");

			break;

		case 1:
			// enable
			BthWaitForDmaIdle(pAd, 300);

			RT_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);
			GloCfg.field.EnTXWriteBackDDONE = 1;
			GloCfg.field.EnableRxDMA = 1;
			GloCfg.field.EnableTxDMA = 1;
			RT_IO_WRITE32(pAd, WPDMA_GLO_CFG, GloCfg.word);

			GloCfg.word = 0;
			RT_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);
			RTBT_ASSERT((GloCfg.field.TxDMABusy == 0) && (GloCfg.field.RxDMABusy == 0));
			DebugPrint(TRACE, DBG_INIT, "Enable Dma = 0x%08x\n", GloCfg.word);

			if (GloCfg.field.TxDMABusy || GloCfg.field.RxDMABusy)
			{
				DebugPrint(ERROR, DBG_INIT, "PDMA Busy(Tx=%d, Rx=%d) !\n\n",
						   GloCfg.field.TxDMABusy,
						   GloCfg.field.RxDMABusy);
			}

			break;
		case 2:
			break;
	}
}

NTSTATUS
BthReadRFRegister(
	IN PRTBTH_ADAPTER pAd,
	IN UCHAR		  RfID,
	IN PUCHAR   	  pValue)
{
	SPI_BUSY_STRUC  	SpiBusyCsr;
	SPI_CMD_DATA_STRUC  SpiCmdDataCsr;
	SPI_RDATA_STRUC 	SpiRDataCsr;
	ULONG   			i = 0, k = 0;

	do
	{
		SpiBusyCsr.word = 0;
		RT_IO_READ32(pAd, SPI_BUSY, &SpiBusyCsr.word);
		if(SpiBusyCsr.word == 0xffffffff)
		{
			DebugPrint(TRACE, DBG_HW_ACCESS,
				"BthReadRFRegister: Card maybe not exist or bt_func not enable bt_fun =%x\n",pAd->btFunCtrl.word);
			break;
		}

		if (SpiBusyCsr.field.SPIBusy == BUSY)
		{
			continue;
		}

		//
		// SPI not busy
		//
		SpiCmdDataCsr.word = 0;
		SpiCmdDataCsr.field.Address = RfID;
		RT_IO_WRITE32(pAd, SPI_CMD_DATA, SpiCmdDataCsr.word);
		do //for (k = 0; k < MAX_BUSY_COUNT; k++)
		{
			RT_IO_READ32(pAd, SPI_BUSY, &SpiBusyCsr.word);

			if (SpiBusyCsr.field.SPIBusy == IDLE)
				break;
		} while ((++k < MAX_BUSY_COUNT) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));

		if (SpiBusyCsr.field.SPIBusy == IDLE)
		{
			break;
		}
	} while ((++i < MAX_BUSY_COUNT) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));

	if (SpiBusyCsr.field.SPIBusy == BUSY)
		return STATUS_UNSUCCESSFUL;

	SpiRDataCsr.word = 0;
	RT_IO_READ32(pAd, SPI_RDATA, &SpiRDataCsr.word);
	*pValue = (UCHAR) SpiRDataCsr.field.SPIRData;

	return STATUS_SUCCESS;
}

NTSTATUS
BthWriteRFRegister(
	IN PRTBTH_ADAPTER pAd,
	IN UCHAR		  RfID,
	IN UCHAR		  Value)
{
	SPI_BUSY_STRUC  	SpiBusyCsr;
	SPI_CMD_DATA_STRUC  SpiCmdDataCsr;
	ULONG   			i = 0;

	do
	{
		SpiBusyCsr.word = 0;
		RT_IO_READ32(pAd, SPI_BUSY, &SpiBusyCsr.word);
		if(SpiBusyCsr.word == 0xffffffff)
		{
			DebugPrint(TRACE, DBG_HW_ACCESS,
					"BthWriteRFRegister: Card maybe not exist or bt_func not enable bt_fun =%x\n",pAd->btFunCtrl.word);
			break;
		}

		if (SpiBusyCsr.field.SPIBusy == IDLE)
		{
			break;
		}

	} while ((++i < MAX_BUSY_COUNT) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));

	if ((i == MAX_BUSY_COUNT) || (RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	SpiCmdDataCsr.word = 0;
	SpiCmdDataCsr.field.RWMode = 1;
	SpiCmdDataCsr.field.RWSpiCmd = 1;
	SpiCmdDataCsr.field.Address = RfID;
	SpiCmdDataCsr.field.WriteData = Value;

	RT_IO_WRITE32(pAd, SPI_CMD_DATA, SpiCmdDataCsr.word);

	return STATUS_SUCCESS;
}

