/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_HAL_INIT_C_

#include <rtl8723d_hal.h>


/* For Two MAC FPGA verify we must disable all MAC/BB/RF setting */
#define FPGA_UNKNOWN		0
#define FPGA_2MAC			1
#define FPGA_PHY			2
#define ASIC					3
#define BOARD_TYPE			ASIC

#if BOARD_TYPE == FPGA_2MAC
#else /* FPGA_PHY and ASIC */
	#define FPGA_RF_UNKNOWN	0
	#define FPGA_RF_8225		1
	#define FPGA_RF_0222D		2
	#define FPGA_RF				FPGA_RF_0222D
#endif

/*
 *	Description:
 *		ePHY Read operation on RTL8723DE
 *
 *	Created by Roger, 2013.05.08.
 */
static u2Byte hal_MDIORead_8723DE(
	IN	PADAPTER	Adapter,
	IN	u1Byte		Addr)
{
	u2Byte ret = 0;
	u1Byte tmpU1b = 0, count = 0;

	rtw_write8(Adapter, REG_PCIE_MIX_CFG_8723D, Addr | BIT(6));
	tmpU1b = rtw_read8(Adapter, REG_PCIE_MIX_CFG_8723D) & BIT(6);
	count = 0;
	while (tmpU1b && count < 20) {
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_PCIE_MIX_CFG_8723D) & BIT(6);
		count++;
	}
	if (0 == tmpU1b) {
		/* */
		ret = rtw_read16(Adapter, REG_MDIO_V1_8723D + 2);
	}

	return ret;
}

/*
 *	Description:
 *		ePHY Write operation on RTL8723DE
 *
 *	Created by Roger, 2013.05.08.
 */
static VOID hal_MDIOWrite_8723DE(
	IN	PADAPTER	Adapter,
	IN	u1Byte		Addr,
	IN	u2Byte		Data)
{
	u1Byte tmpU1b = 0, count = 0;

	rtw_write16(Adapter, REG_MDIO_V1_8723D, Data);
	rtw_write8(Adapter, REG_PCIE_MIX_CFG_8723D, Addr | BIT(5));
	tmpU1b = rtw_read8(Adapter, REG_PCIE_MIX_CFG_8723D) & BIT(5);
	count = 0;
	while (tmpU1b && count < 20) {
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_PCIE_MIX_CFG_8723D) & BIT(5);
		count++;
	}
}

/* ------------------------------------------------------------------- */
/* */
/*	EEPROM Content Parsing */
/* */
/* ------------------------------------------------------------------- */

static VOID
Hal_ReadPROMContent_BT_8723DE(
	IN PADAPTER		Adapter
)
{

#if MP_DRIVER == 1
	if (Adapter->registrypriv.mp_mode == 1)
		EFUSE_ShadowMapUpdate(Adapter, EFUSE_BT, _FALSE);
#endif

}

static VOID
hal_ReadIDs_8723DE(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		/* VID, DID */
		pHalData->EEPROMVID = ReadLE2Byte(&PROMContent[EEPROM_VID_8723DE]);
		pHalData->EEPROMDID = ReadLE2Byte(&PROMContent[EEPROM_DID_8723DE]);
		pHalData->EEPROMSVID = ReadLE2Byte(&PROMContent[EEPROM_SVID_8723DE]);
		pHalData->EEPROMSMID = ReadLE2Byte(&PROMContent[EEPROM_SMID_8723DE]);

		/* Customer ID, 0x00 and 0xff are reserved for Realtek. */
		pHalData->EEPROMCustomerID = *(u8 *)&PROMContent[EEPROM_CustomID_8723D];
		if (pHalData->EEPROMCustomerID == 0xFF)
			pHalData->EEPROMCustomerID = EEPROM_Default_CustomerID_8188E;
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;
	} else {
		pHalData->EEPROMVID			= 0;
		pHalData->EEPROMDID			= 0;
		pHalData->EEPROMSVID		= 0;
		pHalData->EEPROMSMID		= 0;

		/* Customer ID, 0x00 and 0xff are reserved for Realtek. */
		pHalData->EEPROMCustomerID	= EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID	= EEPROM_Default_SubCustomerID;
	}

	RTW_INFO("VID = 0x%04X, DID = 0x%04X\n", pHalData->EEPROMVID, pHalData->EEPROMDID);
	RTW_INFO("SVID = 0x%04X, SMID = 0x%04X\n", pHalData->EEPROMSVID, pHalData->EEPROMSMID);
}

static VOID
Hal_ReadEfusePCIeCap8723DE(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	u8	AspmOscSupport = RT_PCI_ASPM_OSC_IGNORE;
	u16	PCIeCap = 0;

	if (!AutoloadFail) {
		PCIeCap = PROMContent[EEPROM_PCIE_DEV_CAP_01] |
			  (PROMContent[EEPROM_PCIE_DEV_CAP_02] << 8);

		RTW_INFO("Hal_ReadEfusePCIeCap8723DE(): PCIeCap = %#x\n", PCIeCap);

		/* */
		/* <Roger_Notes> We shall take L0S/L1 accept latency into consideration for ASPM Configuration policy, 2013.03.27. */
		/* L1 accept Latency: 0x8d from PCI CFG space offset 0x75 */
		/* L0S accept Latency: 0x80 from PCI CFG space offset 0x74 */
		/* */
		if (PCIeCap == 0x8d80)
			AspmOscSupport |= RT_PCI_ASPM_OSC_ENABLE;
		else
			AspmOscSupport |= RT_PCI_ASPM_OSC_DISABLE;
	}

	rtw_hal_set_def_var(Adapter, HAL_DEF_PCI_ASPM_OSC, (u8 *)&AspmOscSupport);
	RTW_INFO("Hal_ReadEfusePCIeCap8723DE(): AspmOscSupport = %d\n", AspmOscSupport);
}

static VOID
hal_CustomizedBehavior_8723DE(
	PADAPTER			Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*pledpriv = &(Adapter->ledpriv);

	pledpriv->LedStrategy = SW_LED_MODE7; /* Default LED strategy. */
	pHalData->bLedOpenDrain = _TRUE;/* Support Open-drain arrangement for controlling the LED. Added by Roger, 2009.10.16. */

	switch (pHalData->CustomerID) {
	case RT_CID_DEFAULT:
		break;

	case RT_CID_CCX:
		/* pMgntInfo->IndicateByDeauth = _TRUE; */
		break;

	case RT_CID_WHQL:
		/* Adapter->bInHctTest = _TRUE; */
		break;

	default:
		/* RTW_INFO("Unknown hardware Type\n"); */
		break;
	}
	RTW_INFO("hal_CustomizedBehavior_8723DE(): RT Customized ID: 0x%02X\n", pHalData->CustomerID);

#if 0
	if (Adapter->bInHctTest) {
		pMgntInfo->PowerSaveControl.bInactivePs = FALSE;
		pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE;
		pMgntInfo->PowerSaveControl.bLeisurePs = FALSE;
		pMgntInfo->PowerSaveControl.bLeisurePsModeBackup = FALSE;
		pMgntInfo->keepAliveLevel = 0;
	}
#endif
}

static VOID
hal_CustomizeByCustomerID_8723DE(
	IN	PADAPTER		pAdapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	/* If the customer ID had been changed by registry, do not cover up by EEPROM. */
	if (pHalData->CustomerID == RT_CID_DEFAULT) {
		switch (pHalData->EEPROMCustomerID) {
		default:
			pHalData->CustomerID = RT_CID_DEFAULT;
			break;

		}
	}
	RTW_INFO("MGNT Customer ID: 0x%2x\n", pHalData->CustomerID);

	hal_CustomizedBehavior_8723DE(pAdapter);
}


static VOID
InitAdapterVariablesByPROM_8723DE(
	IN	PADAPTER	Adapter
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
	/* RTW_INFO("InitAdapterVariablesByPROM_8723DE()!!\n"); */

	Hal_InitPGData(Adapter, pHalData->efuse_eeprom_data);
	Hal_EfuseParseIDCode(Adapter, pHalData->efuse_eeprom_data);

	Hal_EfuseParseEEPROMVer_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadIDs_8723DE(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_config_macaddr(Adapter, pHalData->bautoload_fail_flag);
	Hal_EfuseParseTxPowerInfo_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadEfusePCIeCap8723DE(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	/* */
	/* Read Bluetooth co-exist and initialize */
	/* */
	Hal_EfuseParseBTCoexistInfo_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	Hal_EfuseParseChnlPlan_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseXtal_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParsePackageType_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseThermalMeter_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseAntennaDiversity_8723D(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_CustomizeByCustomerID_8723DE(Adapter);

#ifdef CONFIG_RF_POWER_TRIM
	Hal_ReadRFGainOffset(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
#endif	/* CONFIG_RF_GAIN_OFFSET */

#ifdef CONFIG_RTW_MAC_HIDDEN_RPT
	hal_read_mac_hidden_rpt(Adapter);
#endif
}

static VOID
Hal_ReadPROMContent_8723DE(
	IN PADAPTER			Adapter
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
	u8			eeValue;


	eeValue = rtw_read8(Adapter, REG_9346CR);
	/* To check system boot selection. */
	pHalData->EepromOrEfuse		= (eeValue & BOOT_FROM_EEPROM) ? _TRUE : _FALSE;
	pHalData->bautoload_fail_flag	= (eeValue & EEPROM_EN) ? _FALSE : _TRUE;

	RTW_INFO("Boot from %s, Autoload %s !\n", (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
		 (pHalData->bautoload_fail_flag ? "Fail" : "OK"));

	/* pHalData->EEType = IS_BOOT_FROM_EEPROM(Adapter) ? EEPROM_93C46 : EEPROM_BOOT_EFUSE; */

	InitAdapterVariablesByPROM_8723DE(Adapter);
}

static void ReadRFType8723DE(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

#if DISABLE_BB_RF
	pHalData->rf_chip = RF_PSEUDO_11N;
	pHalData->rf_type = RF_1T1R;
#else
	pHalData->rf_chip = RF_6052;
	pHalData->rf_type = RF_1T1R;
#endif
}

static void ReadAdapterInfo8723DE(PADAPTER Adapter)
{
	/* Read EEPROM size before call any EEPROM function */
	Adapter->EepromAddressSize = GetEEPROMSize8723D(Adapter);

	/* For debug test now!!!!! */
	PHY_RFShadowRefresh(Adapter);

	/* Read all content in Efuse/EEPROM.	*/
	Hal_ReadPROMContent_8723DE(Adapter);

	Hal_ReadPROMContent_BT_8723DE(Adapter);

	/* We need to define the RF type after all PROM value is recognized. */
	ReadRFType8723DE(Adapter);
}


void rtl8723de_interface_configure(PADAPTER Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
	struct pwrctrl_priv	*pwrpriv = adapter_to_pwrctl(Adapter);
	u16 val_16 = 0;
	u8 ClkDelayCount = 4;	/* default: 4, range: 0~15 */

	/* /close ASPM for AMD defaultly */
	pdvobjpriv->const_amdpci_aspm = 0;

	/* / ASPM PS mode. */
	/* / 0 - Disable ASPM, 1 - Enable ASPM without Clock Req, */
	/* / 2 - Enable ASPM with Clock Req, 3- Alwyas Enable ASPM with Clock Req, */
	/* / 4-  Always Enable ASPM without Clock Req. */
	/* / set default to rtl8188ee:3 RTL8192E:2 */
	pdvobjpriv->const_pci_aspm = 0;

	/* / Setting for PCI-E device */
	pdvobjpriv->const_devicepci_aspm_setting = 0x03;

	/* / Setting for PCI-E bridge */
	pdvobjpriv->const_hostpci_aspm_setting = 0x03;

	/* / In Hw/Sw Radio Off situation. */
	/* / 0 - Default, 1 - From ASPM setting without low Mac Pwr, */
	/* / 2 - From ASPM setting with low Mac Pwr, 3 - Bus D3 */
	/* / set default to RTL8192CE:0 RTL8192SE:2 */
	pdvobjpriv->const_hwsw_rfoff_d3 = 0;

	/* / This setting works for those device with backdoor ASPM setting such as EPHY setting. */
	/* / 0: Not support ASPM, 1: Support ASPM, 2: According to chipset. */
	pdvobjpriv->const_support_pciaspm = 1;

	pwrpriv->reg_rfoff = 0;
	pwrpriv->rfoff_reason = 0;

	pHalData->bL1OffSupport = _FALSE;
	
	if(!IS_A_CUT(pHalData->VersionID) && !IS_B_CUT(pHalData->VersionID)) {
		val_16 = hal_MDIORead_8723DE(Adapter, 0x9);
		hal_MDIOWrite_8723DE(Adapter, 0x9, val_16 & (~BIT14));
	}

	/*test 0x8[15:12] = 2*/
	val_16 = hal_MDIORead_8723DE(Adapter, 0x8);
	val_16 &= 0x0FFF;
	val_16 |= (ClkDelayCount << 12);
	/*val_16 |=  0x2000;*/
	/*val_16 &= 0x2fff;*/
	hal_MDIOWrite_8723DE(Adapter, 0x8, val_16);

}

VOID
DisableInterrupt8723DE(
	IN PADAPTER			Adapter
)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);

	rtw_write32(Adapter, REG_HIMR0_8723D, IMR_DISABLED_8723D);

	rtw_write32(Adapter, REG_HIMR1_8723D, IMR_DISABLED_8723D);	/* by tynli */

	pdvobjpriv->irq_enabled = 0;
}

VOID
ClearInterrupt8723DE(
	IN PADAPTER			Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32	tmp = 0;

	tmp = rtw_read32(Adapter, REG_HISR0_8723D);
	rtw_write32(Adapter, REG_HISR0_8723D, tmp);
	pHalData->IntArray[0] = 0;

	tmp = rtw_read32(Adapter, REG_HISR1_8723D);
	rtw_write32(Adapter, REG_HISR1_8723D, tmp);
	pHalData->IntArray[1] = 0;

	tmp = rtw_read32(Adapter, REG_HSISR_8723D);
	rtw_write32(Adapter, REG_HSISR_8723D, tmp);
	pHalData->SysIntArray[0] = 0;
}


VOID
EnableInterrupt8723DE(
	IN PADAPTER			Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);

	pdvobjpriv->irq_enabled = 1;

	rtw_write32(Adapter, REG_HIMR0_8723D, pHalData->IntrMask[0] & 0xFFFFFFFF);

	rtw_write32(Adapter, REG_HIMR1_8723D, pHalData->IntrMask[1] & 0xFFFFFFFF);

	rtw_write32(Adapter, REG_HSISR_8723D, pHalData->SysIntrMask[0] & 0xFFFFFFFF);
}

BOOLEAN
InterruptRecognized8723DE(
	IN	PADAPTER			Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	BOOLEAN			bRecognized = _FALSE;

	/* 2013.11.18 Glayrainx suggests that turn off IMR and */
	/* restore after cleaning ISR. */
	rtw_write32(Adapter, REG_HIMR0_8723D, 0);
	rtw_write32(Adapter, REG_HIMR1_8723D, 0);

	pHalData->IntArray[0] = rtw_read32(Adapter, REG_HISR0_8723D);
	pHalData->IntArray[0] &= pHalData->IntrMask[0];
	rtw_write32(Adapter, REG_HISR0_8723D, pHalData->IntArray[0]);

	/* For HISR extension. Added by tynli. 2009.10.07. */
	pHalData->IntArray[1] = rtw_read32(Adapter, REG_HISR1_8723D);
	pHalData->IntArray[1] &= pHalData->IntrMask[1];
	rtw_write32(Adapter, REG_HISR1_8723D, pHalData->IntArray[1]);

	if (((pHalData->IntArray[0]) & pHalData->IntrMask[0]) != 0 ||
	    ((pHalData->IntArray[1]) & pHalData->IntrMask[1]) != 0)
		bRecognized = _TRUE;

	rtw_write32(Adapter, REG_HIMR0_8723D, pHalData->IntrMask[0] & 0xFFFFFFFF);
	rtw_write32(Adapter, REG_HIMR1_8723D, pHalData->IntrMask[1] & 0xFFFFFFFF);

	return bRecognized;
}

VOID
UpdateInterruptMask8723DE(
	IN	PADAPTER		Adapter,
	IN	u32		AddMSR,		u32		AddMSR1,
	IN	u32		RemoveMSR, u32		RemoveMSR1
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	DisableInterrupt8723DE(Adapter);

	if (AddMSR)
		pHalData->IntrMask[0] |= AddMSR;

	if (AddMSR1)
		pHalData->IntrMask[1] |= AddMSR1;

	if (RemoveMSR)
		pHalData->IntrMask[0] &= (~RemoveMSR);

	if (RemoveMSR1)
		pHalData->IntrMask[1] &= (~RemoveMSR1);

	EnableInterrupt8723DE(Adapter);
}

static VOID
HwConfigureRTL8723DE(
	IN	PADAPTER			Adapter
)
{

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32	regRRSR = 0;


	/* 1 This part need to modified according to the rate set we filtered!! */
	/* */
	/* Set RRSR, RATR, and BW_OPMODE registers */
	/* */
	switch (pHalData->CurrentWirelessMode) {
	case WIRELESS_MODE_B:
		regRRSR = RATE_ALL_CCK;
		break;
	case WIRELESS_MODE_G:
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_AUTO:
	case WIRELESS_MODE_N_24G:
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	default:
		break;
	}

	/* Init value for RRSR. */
	rtw_write32(Adapter, REG_RRSR, regRRSR);

	/* ARFB table 9 for 11ac 5G 2SS */
	rtw_write32(Adapter, REG_ARFR0_8723D, 0x00000010);
	rtw_write32(Adapter, REG_ARFR0_8723D + 4, 0x3e0ff000);

	/* ARFB table 10 for 11ac 5G 1SS */
	rtw_write32(Adapter, REG_ARFR1_8723D, 0x00000010);
	rtw_write32(Adapter, REG_ARFR1_8723D + 4, 0x000ff000);

	/* Set SLOT time Reg518 0x9 */
	rtw_write8(Adapter, REG_SLOT_8723D, 0x09);

	/* CF-End setting. */
	rtw_write16(Adapter, REG_FWHW_TXQ_CTRL_8723D, 0x1F00);

	/* Set retry limit */
	/* 3vivi, 20100928, especially for DTM, performance_ext, To avoid asoc too long to another AP more than 4.1 seconds. */
	/* 3we find retry 7 times maybe not enough, so we retry more than 7 times to pass DTM. */
	/* if(pMgntInfo->bPSPXlinkMode) */
	/* { */
	/*	pHalData->ShortRetryLimit = 3; */
	/*	pHalData->LongRetryLimit = 3; */
	/* Set retry limit */
	/*	rtw_write16(Adapter, REG_RL, 0x0303); */
	/* } */
	/* else */
	rtw_write16(Adapter, REG_RL, 0x0707);

	/* BAR settings */
	rtw_write32(Adapter, REG_BAR_MODE_CTRL_8723D, 0x0201ffff);

	/* Set Data / Response auto rate fallack retry count */
	rtw_write32(Adapter, REG_DARFRC, 0x01000000);
	rtw_write32(Adapter, REG_DARFRC + 4, 0x07060504);
	rtw_write32(Adapter, REG_RARFRC, 0x01000000);
	rtw_write32(Adapter, REG_RARFRC + 4, 0x07060504);

	/* Beacon related, for rate adaptive */
	rtw_write8(Adapter, REG_ATIMWND_8723D, 0x2);

	rtw_write8(Adapter, REG_BCN_MAX_ERR_8723D, 0xff);

	/* 20100211 Joseph: Change original setting of BCN_CTRL(0x550) from */
	/* 0x1e(0x2c for test chip) ro 0x1f(0x2d for test chip). Set BIT0 of this register disable ATIM */
	/* function. Since we do not use HIGH_QUEUE anymore, ATIM function is no longer used. */
	/* Also, enable ATIM function may invoke HW Tx stop operation. This may cause ping failed */
	/* sometimes in long run test. So just disable it now. */
	/* PlatformAtomicExchange((pu4Byte)(&pHalData->RegBcnCtrlVal), 0x1d); */
	pHalData->RegBcnCtrlVal = 0x1d;
#ifdef CONFIG_CONCURRENT_MODE
	rtw_write16(Adapter, REG_BCN_CTRL, 0x1818);	/* For 2 PORT TSF SYNC */
#else
	rtw_write8(Adapter, REG_BCN_CTRL, (u8)(pHalData->RegBcnCtrlVal));
#endif

	/* BCN_CTRL1 Reg551 0x10 */

	/* TBTT prohibit hold time. Suggested by designer TimChen. */
	rtw_write8(Adapter, REG_TBTT_PROHIBIT + 1, 0xff); /* 8 ms */

	/* 3 Note */
	rtw_write8(Adapter, REG_PIFS_8723D, 0);
	rtw_write8(Adapter, REG_AGGR_BREAK_TIME_8723D, 0x16);

	rtw_write16(Adapter, REG_NAV_PROT_LEN_8723D, 0x0040);
	rtw_write16(Adapter, REG_PROT_MODE_CTRL_8723D, 0x08ff);

	if (!Adapter->registrypriv.wifi_spec) {
		/* For Rx TP. Suggested by SD1 Richard. Added by tynli. 2010.04.12. */
		rtw_write32(Adapter, REG_FAST_EDCA_CTRL, 0x03086666);
	} else {
		/* For WiFi WMM. suggested by timchen. Added by tynli. */
		rtw_write32(Adapter, REG_FAST_EDCA_CTRL, 0x0);
	}

	/* ACKTO for IOT issue. */
	rtw_write8(Adapter, REG_ACKTO_8723D, 0x40);


	/* Set Spec SIFS (used in NAV) */
	/* 92d all set these register 0x1010, check it later */
	rtw_write16(Adapter, REG_SPEC_SIFS_8723D, 0x100a);
	rtw_write16(Adapter, REG_MAC_SPEC_SIFS_8723D, 0x100a);

	/* Set SIFS for CCK */
	rtw_write16(Adapter, REG_SIFS_CTX_8723D, 0x100a);

	/* Set SIFS for OFDM */
	rtw_write16(Adapter, REG_SIFS_TRX_8723D, 0x100a);



	/* 3 Note Data sheet don't define */
	rtw_write8(Adapter, 0x4C7, 0x80);

	rtw_write8(Adapter, REG_RX_PKT_LIMIT_8723D, 0x17);

	rtw_write8(Adapter, REG_MAX_AGGR_NUM_8723D, 0x0A);
	rtw_write8(Adapter, REG_AMPDU_MAX_TIME_8723D, 0x1C);

	/* Set Multicast Address. 2009.01.07. by tynli. */
	rtw_write32(Adapter, REG_MAR_8723D, 0xffffffff);
	rtw_write32(Adapter, REG_MAR_8723D + 4, 0xffffffff);

	/* end of win drv */

#if 1

	/* TXOP */
	rtw_write32(Adapter, REG_EDCA_BE_PARAM, 0x005EA42B);
	rtw_write32(Adapter, REG_EDCA_BK_PARAM, 0x0000A44F);
	rtw_write32(Adapter, REG_EDCA_VI_PARAM, 0x005EA324);
	rtw_write32(Adapter, REG_EDCA_VO_PARAM, 0x002FA226);
#endif

	/*rtw_write8(Adapter, REG_HT_SINGLE_AMPDU_8723D, 0x80);*/

	/*#if (OMNIPEEK_SNIFFER_ENABLED == 1)*/
	/* For sniffer parsing legacy rate bandwidth information
	PHY_SetBBReg(Adapter, 0x834, BIT(26), 0x0); */
	/*#endif*/

#if (HAL_MAC_ENABLE == 0)
	rtw_write32(Adapter, REG_MAR, 0xffffffff);
	rtw_write32(Adapter, REG_MAR + 4, 0xffffffff);
#endif
}

static u32 _InitPowerOn_8723DE(PADAPTER Adapter)
{
#ifdef CONFIG_BT_COEXIST
	u8 value8;
	u16 value16;
	u32 value32;
#endif
	u32	status = _SUCCESS;
	u8	tmpU1b;
	u16	tmpU2b;
	u8 bMacPwrCtrlOn = _FALSE;
	u32 tmp4Byte;

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if (bMacPwrCtrlOn == _TRUE)
		return _SUCCESS;

	rtw_write8(Adapter, REG_RSV_CTRL, 0x0);

	tmp4Byte = rtw_read32(Adapter, REG_SYS_CFG);

	if (tmp4Byte & BIT(24)) /* LDO */
		rtw_write8(Adapter, 0x7C, 0xC3);
	else /* SPS */
		rtw_write8(Adapter, 0x7C, 0x83);

	/* HW Power on sequence */
	if (!HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8723D_card_enable_flow))
		return _FAIL;

	/* Release MAC IO register reset */
	tmpU1b = rtw_read8(Adapter, REG_CR);
	tmpU1b = 0xff;
	rtw_write8(Adapter, REG_CR, tmpU1b);
	rtw_mdelay_os(2);

	tmpU1b = 0x7f;
	rtw_write8(Adapter, REG_HWSEQ_CTRL, tmpU1b);
	rtw_mdelay_os(2);

	/* Add for wakeup online */
	tmpU1b = rtw_read8(Adapter, REG_SYS_CLKR);
	rtw_write8(Adapter, REG_SYS_CLKR, (tmpU1b | BIT(3)));
	tmpU1b = rtw_read8(Adapter, REG_GPIO_MUXCFG + 1);
	rtw_write8(Adapter, REG_GPIO_MUXCFG + 1, (tmpU1b & ~BIT(4)));

	/* Release MAC IO register reset */
	/* 9.	CR 0x100[7:0]	= 0xFF; */
	/* 10.	CR 0x101[1]	= 0x01; */ /* Enable SEC block */
#if 0
	rtw_write16(Adapter, REG_CR, 0x00);
	tmpU2b = rtw_read16(Adapter, REG_CR);
	tmpU2b |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
		   | PROTOCOL_EN | SCHEDULE_EN | ENSEC | CALTMR_EN
		   | MACTXEN | MACRXEN);
	rtw_write16(Adapter, REG_CR, tmpU2b);
#else
	rtw_write16(Adapter, REG_CR, 0x2ff);
#endif

	bMacPwrCtrlOn = _TRUE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);

	return status;
}

static u8
_RQPN_Init_8723DE(
	IN  PADAPTER	Adapter,
	OUT u8		*pNPQRQPNVaule,
	OUT u32		*pRQPNValue
)
{
	u8			i, numNQ = 0, numPubQ = 0, numHQ = 0, numLQ = 0;
	/* u8*			numPageArray; */

	if (Adapter->registrypriv.wifi_spec) {
		numHQ = WMM_NORMAL_PAGE_NUM_HPQ_8723D;	/* 0x30 */
		numLQ = WMM_NORMAL_PAGE_NUM_LPQ_8723D;	/* 0x20 */
		numNQ = WMM_NORMAL_PAGE_NUM_NPQ_8723D;	/* 0x20 */
	} else {
		numHQ = NORMAL_PAGE_NUM_HPQ_8723D;	/* 0x0c */
		numLQ = NORMAL_PAGE_NUM_LPQ_8723D;	/* 0x02 */
		numNQ = NORMAL_PAGE_NUM_NPQ_8723D;	/* 0x02 */
	}

	numPubQ = TX_TOTAL_PAGE_NUMBER_8723D - numHQ - numLQ - numNQ;

	*pNPQRQPNVaule = _NPQ(numNQ);
	*pRQPNValue = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;

	if (Adapter->registrypriv.wifi_spec)
		return WMM_NORMAL_TX_PAGE_BOUNDARY_8723D;
	else
		return TX_PAGE_BOUNDARY_8723D;
}

static s32
_LLTWrite_8723DE(
	IN	PADAPTER	Adapter,
	IN	u32			address,
	IN	u32			data
)
{
	s32	status = _SUCCESS;
	s32	count = 0;
	u32	value = _LLT_INIT_ADDR(address) | _LLT_INIT_DATA(data) | _LLT_OP(_LLT_WRITE_ACCESS);

	rtw_write32(Adapter, REG_LLT_INIT, value);

	/* polling */
	do {
		value = rtw_read32(Adapter, REG_LLT_INIT);
		if (_LLT_NO_ACTIVE == _LLT_OP_VALUE(value))
			break;

		if (count > POLLING_LLT_THRESHOLD) {
			status = _FAIL;
			break;
		}
	} while (count++);

	return status;
}

static s32
LLT_table_init_8723DE(
	IN	PADAPTER	Adapter,
	IN	u8			txpktbuf_bndy,
	IN	u32			RQPN,
	IN	u8			RQPN_NPQ
)
{
	u16	i, maxPage = 255;
	s32	status = _SUCCESS;

	/* 12.	TXRKTBUG_PG_BNDY 0x114[31:0] = 0x27FF00F6	/TXRKTBUG_PG_BNDY */
	rtw_write8(Adapter, REG_TRXFF_BNDY, txpktbuf_bndy);
	rtw_write16(Adapter, REG_TRXFF_BNDY + 2, RX_DMA_BOUNDARY_8723D);

	/* 13.	0x208[15:8] 0x209[7:0] = 0xF6				/ Beacon Head for TXDMA */
	rtw_write8(Adapter, REG_DWBCN0_CTRL_8723D + 1, txpktbuf_bndy);

	/* 14.	BCNQ_PGBNDY 0x424[7:0] =  0xF6				/BCNQ_PGBNDY */
	/* 2009/12/03 Why do we set so large boundary. confilct with document V11. */
	rtw_write8(Adapter, REG_BCNQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter, REG_MGQ_BDNY, txpktbuf_bndy);

#ifdef CONFIG_CONCURRENT_MODE
	rtw_write8(Adapter, REG_BCNQ1_BDNY, txpktbuf_bndy + 8);
	rtw_write8(Adapter, REG_DWBCN1_CTRL_8723D + 1, txpktbuf_bndy + 8); /* BCN1_HEAD */
	/* BIT1- BIT_SW_BCN_SEL_EN */
	rtw_write8(Adapter, REG_DWBCN1_CTRL_8723D + 2, rtw_read8(Adapter, REG_DWBCN1_CTRL_8723D + 2) | BIT(1));
#endif

	/* 15.	WMAC_LBK_BF_HD 0x45D[7:0] =  0xF6			/WMAC_LBK_BF_HD */
	rtw_write8(Adapter, REG_WMAC_LBK_BF_HD, txpktbuf_bndy);

	/* 16.	DRV_INFO_SZ = 0x04 */
	rtw_write8(Adapter, REG_RX_DRVINFO_SZ, 0x4);

	/* auto init LLT */
	if (rtl8723d_InitLLTTable(Adapter) == _FAIL) {
		return _FAIL;
	}

	/* Set reserved page for each queue */
	/* 11.	RQPN 0x200[31:0]	= 0x80BD1C1C				/ load RQPN */
	rtw_write32(Adapter, REG_RQPN, RQPN);/* 0x80cb1010);/ 0x80711010);/0x80cb1010); */

	if (RQPN_NPQ != 0)
		rtw_write8(Adapter, REG_RQPN_NPQ, RQPN_NPQ);

	return _SUCCESS;
}

static  u32
InitMAC_8723DE(
	IN	PADAPTER	Adapter
)
{
	u8	tmpU1b;
	u16	tmpU2b;
	struct recv_priv	*precvpriv = &Adapter->recvpriv;
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	RTW_INFO("=======>InitMAC_8723D()\n");

	if (rtw_hal_power_on(Adapter) == _FAIL) {
		RTW_INFO("_InitPowerOn8723DE fail\n");
		return _FAIL;
	}

	/* if(!pHalData->bMACFuncEnable) */
	{
		/* System init */
		/* 18.	LLT_table_init(Adapter); */
		u32	RQPN = 0x80EB0808;
		u8	RQPN_NPQ = 0;
		u8	TX_PAGE_BOUNDARY =
			_RQPN_Init_8723DE(Adapter, &RQPN_NPQ, &RQPN);

		if (LLT_table_init_8723DE(Adapter, TX_PAGE_BOUNDARY, RQPN, RQPN_NPQ) == _FAIL)
			return _FAIL;
	}

	/* Enable Host Interrupt */
	rtw_write32(Adapter, REG_HISR0_8723D, 0xffffffff);
	rtw_write32(Adapter, REG_HISR1_8723D, 0xffffffff);


	tmpU2b = rtw_read16(Adapter, REG_TRXDMA_CTRL);
	tmpU2b &= 0xf;
	if (Adapter->registrypriv.wifi_spec)
		tmpU2b |= 0xF9B1;
	else
		tmpU2b |= 0xF5B1;
	rtw_write16(Adapter, REG_TRXDMA_CTRL, tmpU2b);


	/* Reported Tx status from HW for rate adaptive. */
	/* 2009/12/03 MH This should be realtive to power on step 14. But in document V11 */
	/* still not contain the description.!!! */
	rtw_write8(Adapter, REG_FWHW_TXQ_CTRL + 1, 0x1F);

	/* Set RCR register */
	rtw_write32(Adapter, REG_RCR, pHalData->ReceiveConfig);
	rtw_write16(Adapter, REG_RXFLTMAP0, 0xFFFF);
	rtw_write16(Adapter, REG_RXFLTMAP1, 0x400);
	rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);
	/* Set TCR register */
	rtw_write32(Adapter, REG_TCR, pHalData->TransmitConfig);

	/* */
	/* Set TX/RX descriptor physical address(from OS API). */
	/* */
	rtw_write32(Adapter, REG_BCNQ_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_MGQ_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VOQ_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VIQ_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BEQ_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BKQ_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_HI0Q_TXBD_DESA_8723D, (u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_RXQ_RXBD_DESA_8723D, (u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma & DMA_BIT_MASK(32));

#ifdef CONFIG_64BIT_DMA
	/* 2009/10/28 MH For DMA 64 bits. We need to assign the high 32 bit address */
	/* for NIC HW to transmit data to correct path. */
	rtw_write32(Adapter, REG_BCNQ_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_MGQ_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_VOQ_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_VIQ_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_BEQ_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_BKQ_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_HI0Q_TXBD_DESA_8723D + 4,
		    ((u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma) >> 32);
	rtw_write32(Adapter, REG_RXQ_RXBD_DESA_8723D + 4,
		    ((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma) >> 32);

#if 1
	/* Don't check dma addr, just depends on PCI-host setting */
	if (Adapter->dvobj->bdma64) {
		RTW_INFO("Enable DMA64 bit\n");

		PlatformEnableDMA64(Adapter);
	}
#else
	/* check dma addr, and it may fallback to 32bit mode */

	/* 2009/10/28 MH If RX descriptor address is not equal to zero. We will enable */
	/* DMA 64 bit functuion. */
	/* Note: We never saw thd consition which the descripto address are divided into */
	/* 4G down and 4G upper separate area. */
	if (((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma) >> 32 != 0) {
		/* RTW_INFO("RX_DESC_HA=%08lx\n", ((u64)priv->rx_ring_dma[RX_MPDU_QUEUE])>>32); */
		RTW_INFO("Enable DMA64 bit\n");

		/* Check if other descriptor address is zero and abnormally be in 4G lower area. */
		if (((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma) >> 32) {
			/* */
			RTW_INFO("MGNT_QUEUE HA=0\n");
		}

		PlatformEnableDMA64(Adapter);
	} else {
		struct pci_dev *pdev = Adapter->dvobj->ppcidev;

		RTW_INFO("Enable DMA32 bit (fallback)\n");

		if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
			if (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
				RTW_INFO(KERN_ERR "Unable to obtain 32bit DMA for consistent allocations\n");
		}
	}
#endif
#endif

	/* Reset the Read/Write point to 0 */
	rtw_write32(Adapter, REG_BD_RW_PTR_CLR_8723D, 0X3FFFFFFF);

	tmpU1b = rtw_read8(Adapter, REG_PCIE_CTRL_REG_8723D + 3);
	rtw_write8(Adapter, REG_PCIE_CTRL_REG_8723D + 3, (tmpU1b | 0xF7));

	/* 20100318 Joseph: Reset interrupt migration setting when initialization. Suggested by SD1. */
	rtw_write32(Adapter, REG_INT_MIG_8723D, 0);
	pHalData->bInterruptMigration = _FALSE;

	/* 2009.10.19. Reset H2C protection register. by tynli. */
	rtw_write32(Adapter, REG_MCUTST_1_8723D, 0x0);

#if MP_DRIVER == 1
	if (Adapter->registrypriv.mp_mode == 1) {
		rtw_write32(Adapter, REG_MACID, 0x87654321);
		rtw_write32(Adapter, 0x0700, 0x87654321);
	}
#endif

	/* pic buffer descriptor mode: */
	/* ---- tx */
	rtw_write16(Adapter, REG_MGQ_TXBD_NUM_8723D, (pxmitpriv->txringcount[MGT_QUEUE_INX] & 0x0FFF)  | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_VOQ_TXBD_NUM_8723D, (pxmitpriv->txringcount[VO_QUEUE_INX]  & 0x0FFF)  | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_VIQ_TXBD_NUM_8723D, (pxmitpriv->txringcount[VI_QUEUE_INX]  & 0x0FFF)  | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_BEQ_TXBD_NUM_8723D, (pxmitpriv->txringcount[BE_QUEUE_INX]  & 0x0FFF)  | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_BKQ_TXBD_NUM_8723D, (pxmitpriv->txringcount[BK_QUEUE_INX]  & 0x0FFF)  | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI0Q_TXBD_NUM_8723D, (pxmitpriv->txringcount[HIGH_QUEUE_INX] & 0x0FFF) | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
#if 0
	rtw_write16(Adapter, REG_HI1Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI2Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI3Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI4Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI5Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI6Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
	rtw_write16(Adapter, REG_HI7Q_TXBD_NUM_8723D, TX_BD_NUM | ((TX_BUFFER_SEG_NUM << 12) & 0x3000));
#endif
	/* ---- rx. support 32 bits in linux */
#ifdef CONFIG_64BIT_DMA
	rtw_write16(Adapter, REG_RX_RXBD_NUM_8723D, (precvpriv->rxringcount & 0x0FFF) | ((TX_BUFFER_SEG_NUM << 13) & 0x6000) | 0x8000); /*using 64bit */
#else
	rtw_write16(Adapter, REG_RX_RXBD_NUM_8723D, (precvpriv->rxringcount & 0x0FFF) | ((TX_BUFFER_SEG_NUM << 13) & 0x6000)); /*using 32bit */
#endif

	/* ---- reset read/write point */
	rtw_write32(Adapter, REG_BD_RW_PTR_CLR_8723D, 0XFFFFFFFF); /*Reset read/write point*/

	pHalData->RxTag = 0;
	/* Release Pcie Interface Tx DMA. */
#if defined(USING_RX_TAG)
	rtw_write16(Adapter, REG_PCIE_CTRL_REG_8723D, 0x8000);
#else
	rtw_write16(Adapter, REG_PCIE_CTRL_REG_8723D, 0x0000);
#endif

	RTW_INFO("InitMAC_8723D() <====\n");

	return _SUCCESS;
}

/*
 *	Description:
 *		PCI configuration space read operation on RTL8723DE
 *
 *	Created by Roger, 2013.05.08.
 */
u1Byte
hal_DBIRead_8723DE(
	IN	PADAPTER	Adapter,
	IN	u2Byte		Addr
)
{
	u2Byte ReadAddr = Addr & 0xfffc;
	u1Byte ret = 0, tmpU1b = 0, count = 0;

	rtw_write16(Adapter, REG_DBI_FLAG_V1_8723D, ReadAddr);
	rtw_write8(Adapter, REG_DBI_FLAG_V1_8723D + 2, 0x2);
	tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_V1_8723D + 2);
	count = 0;
	while (tmpU1b && count < 20) {
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_V1_8723D + 2);
		count++;
	}
	if (0 == tmpU1b) {
		ReadAddr = REG_DBI_RDATA_V1_8723D + Addr % 4;
		ret = rtw_read8(Adapter, ReadAddr);
	}

	return ret;
}

/*
 *	Description:
 *		PCI configuration space write operation on RTL8723DE
 *
 *	Created by Roger, 2013.05.08.
 */
VOID
hal_DBIWrite_8723DE(
	IN	PADAPTER	Adapter,
	IN	u2Byte		Addr,
	IN	u1Byte		Data
)
{
	u1Byte tmpU1b = 0, count = 0;
	u2Byte WriteAddr = 0, Remainder = Addr % 4;

	/* Write DBI 1Byte Data */
	WriteAddr = REG_DBI_WDATA_V1_8723D + Remainder;
	rtw_write8(Adapter, WriteAddr, Data);

	/* Write DBI 2Byte Address & Write Enable */
	WriteAddr = (Addr & 0xfffc) | (BIT(0) << (Remainder + 12));
	rtw_write16(Adapter, REG_DBI_FLAG_V1_8723D, WriteAddr);

	/* Write DBI Write Flag */
	rtw_write8(Adapter, REG_DBI_FLAG_V1_8723D + 2, 0x1);

	tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_V1_8723D + 2);
	count = 0;
	while (tmpU1b && count < 20) {
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_V1_8723D + 2);
		count++;
	}
}

VOID
EnableAspmBackDoor_8723DE(IN	PADAPTER Adapter)
{

	u1Byte tmp1byte = 0;
	u2Byte tmp2byte = 0;


	/* */
	/* <Roger_Notes> Overwrite following ePHY parameter for some platform compatibility issue, */
	/* especially when CLKReq is enabled, 2012.11.09. */
	/* */
	tmp2byte = hal_MDIORead_8723DE(Adapter, 0x0b);
	if (tmp2byte != 0x70)
		hal_MDIOWrite_8723DE(Adapter, 0x0b, 0x70);

	/* Configuration Space offset 0x70f BIT7 is used to control L0S */
	tmp1byte = hal_DBIRead_8723DE(Adapter, 0x70f);
	hal_DBIWrite_8723DE(Adapter, 0x70f, tmp1byte | BIT(7));

	/* Configuration Space offset 0x719 Bit3 is for L1 BIT4 is for clock request */
	tmp1byte = hal_DBIRead_8723DE(Adapter, 0x719);
	hal_DBIWrite_8723DE(Adapter, 0x719, tmp1byte | BIT(3) | BIT(4));

}

VOID
EnableL1Off_8723DE(IN	PADAPTER Adapter)
{
	u8	tmpU1b  = 0;
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	if (pHalData->bL1OffSupport == _FALSE)
		return;

	hal_MDIOWrite_8723DE(Adapter, 0x1b, (hal_MDIORead_8723DE(Adapter, 0x1b) | BIT(4)));

	tmpU1b  = hal_DBIRead_8723DE(Adapter, 0x718);
	hal_DBIWrite_8723DE(Adapter, 0x718, tmpU1b | BIT(5));

	tmpU1b  = hal_DBIRead_8723DE(Adapter, 0x160);
	hal_DBIWrite_8723DE(Adapter, 0x160, tmpU1b | 0xf);
}

VOID
SetPciLtr_8723DE(
	IN	PADAPTER	Adapter
	)
{
	u4Byte			tmpU4b		= 0;

	/* Add workaround for 11n Wifi spec, set LTR idle latency to 144us for throughput test
	 * by Hana, 2015/04/17
	 */
	if (Adapter->registrypriv.wifi_spec == 1) {
		PlatformEFIOWrite4Byte(Adapter, REG_LTR_IDLE_LATENCY_V1_8723D, 0x88908890);
	} else {
		/* LTR idle latency, 0x90 for 3ms */
		PlatformEFIOWrite4Byte(Adapter, REG_LTR_IDLE_LATENCY_V1_8723D, 0x883C883C);
	}

	/* LTR active latency, 0x3c for 64us */
	PlatformEFIOWrite4Byte(Adapter, REG_LTR_ACTIVE_LATENCY_V1_8723D, 0x880B880B);

	/* for HP 8723DE device loss, suggest by Tim lin 20170322 */
	PlatformEFIOWrite4Byte(Adapter, REG_LTR_CTRL_BASIC_8723D, 0xCB004010);
	PlatformEFIOWrite4Byte(Adapter, (REG_LTR_CTRL_BASIC_8723D + 4), 0x01233425);
}

/*
 * Description: Check if PCIe DMA Tx/Rx hang.
 * The flow is provided by SD1 Alan. Added by tynli. 2013.05.03.
 */
BOOLEAN
CheckPcieDMAHang8723DE(
	IN	PADAPTER	Adapter
)
{
	u1Byte  u1bTmp;

	/* write reg 0x350 Bit[26]=1. Enable debug port. */
	u1bTmp = PlatformEFIORead1Byte(Adapter, REG_DBI_FLAG_V1_8723D + 3);
	if (!(u1bTmp & BIT(2))) {
		PlatformEFIOWrite1Byte(Adapter, REG_DBI_FLAG_V1_8723D + 3, (u1bTmp | BIT(2)));
		rtw_mdelay_os(100); /* Suggested by DD Justin_tsai. */
	}

	/* read reg 0x350 Bit[25] if 1 : RX hang */
	/* read reg 0x350 Bit[24] if 1 : TX hang */
	u1bTmp = PlatformEFIORead1Byte(Adapter, REG_DBI_FLAG_V1_8723D + 3);
	if ((u1bTmp & BIT(0)) || (u1bTmp & BIT(1))) {
		return TRUE;
	}

	return FALSE;
}

/* */
/* Description: Reset Pcie interface DMA. */
/* */
VOID
ResetPcieInterfaceDMA8723DE(
	IN PADAPTER		Adapter,
	IN BOOLEAN		bInMACPowerOn
)
{
	u1Byte	u1Tmp;
	BOOLEAN	bReleaseMACRxPause;
	u1Byte	BackUpPcieDMAPause;


	/* Revise Note: Follow the document "PCIe RX DMA Hang Reset Flow_v03" released by SD1 Alan. */
	/* 2013.05.07, by tynli. */

	/* 1. disable register write lock */
	/*		write 0x1C bit[1:0] = 2'h0 */
	/*		write 0xCC bit[2] = 1'b1 */
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_RSV_CTRL_8723D);
	u1Tmp &= ~(BIT(1) | BIT(0));
	PlatformEFIOWrite1Byte(Adapter, REG_RSV_CTRL_8723D, u1Tmp);
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_PMC_DBG_CTRL2_8723D);
	u1Tmp |= BIT(2);
	PlatformEFIOWrite1Byte(Adapter, REG_PMC_DBG_CTRL2_8723D, u1Tmp);

	/* 2. Check and pause TRX DMA */
	/*		write 0x284 bit[18] = 1'b1 */
	/*		write 0x301 = 0xFF */
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_RXDMA_CONTROL_8723D);
	if (u1Tmp & BIT(2)) {
		/* Already pause before the function for another purpose. */
		bReleaseMACRxPause = FALSE;
	} else {
		PlatformEFIOWrite1Byte(Adapter, REG_RXDMA_CONTROL_8723D, (u1Tmp | BIT(2)));
		bReleaseMACRxPause = TRUE;
	}

	BackUpPcieDMAPause = PlatformEFIORead1Byte(Adapter,	REG_PCIE_CTRL_REG_8723D + 1);
	if (BackUpPcieDMAPause != 0xFF)
		PlatformEFIOWrite1Byte(Adapter, REG_PCIE_CTRL_REG_8723D + 1, 0xFF);

	if (bInMACPowerOn) {
		/* 3. reset TRX function */
		/*		write 0x100 = 0x00 */
		PlatformEFIOWrite1Byte(Adapter, REG_CR, 0);
	}

	/* 4. Reset PCIe DMA */
	/*		write 0x003 bit[0] = 0 */
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_SYS_FUNC_EN + 1);
	u1Tmp &= ~(BIT(0));
	PlatformEFIOWrite1Byte(Adapter, REG_SYS_FUNC_EN + 1, u1Tmp);

	/* 5. Enable PCIe DMA */
	/*		write 0x003 bit[0] = 1 */
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_SYS_FUNC_EN + 1);
	u1Tmp |= BIT(0);
	PlatformEFIOWrite1Byte(Adapter, REG_SYS_FUNC_EN + 1, u1Tmp);

	if (bInMACPowerOn) {
		/* 6. enable TRX function */
		/* write 0x100 = 0xFF */
		PlatformEFIOWrite1Byte(Adapter, REG_CR, 0xFF);

		/* We should init LLT & RQPN and prepare Tx/Rx descrptor address later */
		/* because MAC function is reset. */
	}

	/* 7. Restore PCIe autoload down bit */
	/* write 0xF8 bit[17] = 1'b1 */
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_MAC_PHY_CTRL_NORMAL + 2);
	u1Tmp |= BIT(1);
	PlatformEFIOWrite1Byte(Adapter, REG_MAC_PHY_CTRL_NORMAL + 2, u1Tmp);

	/* In MAC power on state, BB and RF maybe in ON state, if we release TRx DMA here */
	/* it will cause packets to be started to Tx/Rx, so we release Tx/Rx DMA later. */
	if (!bInMACPowerOn) {
		/* 8. release TRX DMA */
		/* write 0x284 bit[18] = 1'b0 */
		/* write 0x301 = 0x00 */
		if (bReleaseMACRxPause) {
			u1Tmp = PlatformEFIORead1Byte(Adapter, REG_RXDMA_CONTROL_8723D);
			PlatformEFIOWrite1Byte(Adapter, REG_RXDMA_CONTROL_8723D, (u1Tmp & ~BIT(2)));
		}
		PlatformEFIOWrite1Byte(Adapter, REG_PCIE_CTRL_REG_8723D + 1, BackUpPcieDMAPause);
	}

	/* 9. lock system register */
	/*		write 0xCC bit[2] = 1'b0 */
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_PMC_DBG_CTRL2_8723D);
	u1Tmp &= ~(BIT(2));
	PlatformEFIOWrite1Byte(Adapter, REG_PMC_DBG_CTRL2_8723D, u1Tmp);

}
VOID
PHY_InitAntennaSelection8723DE(
	IN PADAPTER Adapter
)
{

}

static VOID _InitBBRegBackup_8723DE(PADAPTER	Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	/* For Channel 1~11 (Default Value)*/
	pHalData->RegForRecover[0].offset = rCCK0_TxFilter2;
	pHalData->RegForRecover[0].value =
		PHY_QueryBBReg(Adapter,
			       pHalData->RegForRecover[0].offset, bMaskDWord);

	pHalData->RegForRecover[1].offset = rCCK0_DebugPort;
	pHalData->RegForRecover[1].value =
		PHY_QueryBBReg(Adapter,
			       pHalData->RegForRecover[1].offset, bMaskDWord);

	pHalData->RegForRecover[2].offset = 0xAAC;
	pHalData->RegForRecover[2].value =
		PHY_QueryBBReg(Adapter,
			       pHalData->RegForRecover[2].offset, bMaskDWord);
#if 0
	/* For 20 MHz	(Default Value)*/
	pHalData->RegForRecover[2].offset = rBBrx_DFIR;
	pHalData->RegForRecover[2].value = PHY_QueryBBReg(Adapter, pHalData->RegForRecover[2].offset, bMaskDWord);

	pHalData->RegForRecover[3].offset = rOFDM0_XATxAFE;
	pHalData->RegForRecover[3].value = PHY_QueryBBReg(Adapter, pHalData->RegForRecover[3].offset, bMaskDWord);

	pHalData->RegForRecover[4].offset = 0x1E;
	pHalData->RegForRecover[4].value = PHY_QueryRFReg(Adapter, ODM_RF_PATH_A, pHalData->RegForRecover[4].offset, bRFRegOffsetMask);
#endif
}

/* Set CCK and OFDM Block "ON" */
static void _BBTurnOnBlock(PADAPTER padapter)
{
#if (DISABLE_BB_RF)
	return;
#endif

	PHY_SetBBReg(padapter, rFPGA0_RFMOD, bCCKEn, 0x1);
	PHY_SetBBReg(padapter, rFPGA0_RFMOD, bOFDMEn, 0x1);
}

static u32 rtl8723de_hal_init(PADAPTER Adapter)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);
	u32	rtStatus = _SUCCESS;
	u8	tmpU1b, u1bRegCR;
	u32 u4Tmp;
	u32	i;
	BOOLEAN bSupportRemoteWakeUp;
	u32	NavUpper = WiFiNavUpperUs;
	PDM_ODM_T			pDM_Odm = &pHalData->odmpriv;


	/* */
	/* No I/O if device has been surprise removed */
	/* */
	if (rtw_is_surprise_removed(Adapter)) {
		RTW_INFO("rtl8723de_hal_init(): bSurpriseRemoved!\n");
		return _SUCCESS;
	}

	Adapter->init_adpt_in_progress = _TRUE;
	RTW_INFO("=======>rtl8723de_hal_init()\n");

	u1bRegCR = rtw_read8(Adapter, REG_CR);
	RTW_INFO(" power-on :REG_CR 0x100=0x%02x.\n", u1bRegCR);
	if (/*(tmpU1b&BIT3) && */ (u1bRegCR != 0 && u1bRegCR != 0xEA)) {
		/* pHalData->bMACFuncEnable = TRUE; */
		RTW_INFO(" MAC has already power on.\n");
	} else {
		/* pHalData->bMACFuncEnable = FALSE; */
		/* Set FwPSState to ALL_ON mode to prevent from the I/O be return because of 32k */
		/* state which is set before sleep under wowlan mode. 2012.01.04. by tynli. */
		/* pHalData->FwPSState = FW_PS_STATE_ALL_ON_88E; */
	}

	if (CheckPcieDMAHang8723DE(Adapter)) {
		/* ResetPcieInterfaceDMA8723DE(Adapter, pHalData->bMACFuncEnable); */
		ResetPcieInterfaceDMA8723DE(Adapter, (u1bRegCR != 0 && u1bRegCR != 0xEA));
		/* Release pHalData->bMACFuncEnable here because it will reset MAC in ResetPcieInterfaceDMA */
		/* function such that we need to allow LLT to be initialized in later flow. */
		/* pHalData->bMACFuncEnable = FALSE; */
	}

	/*if(pHalData->bMACFuncEnable == FALSE)*/
	{
		u1Byte u1Tmp;

		u1Tmp = PlatformEFIORead1Byte(Adapter, REG_HCI_OPT_CTRL_8723D+1);
		PlatformEFIOWrite1Byte(Adapter, REG_HCI_OPT_CTRL_8723D+1, (u1Tmp|BIT0));/*Disable USB Suspend Signal*/
	}

	/* */
	/* 1. MAC Initialize */
	/* */
	rtStatus = InitMAC_8723DE(Adapter);
	rtw_write8(Adapter, REG_SECONDARY_CCA_CTRL_8723D, 0x03);
	rtw_write8(Adapter, 0x976, 0);	/* hpfan_todo: 2nd CCA related */
	if (rtStatus != _SUCCESS) {
		RTW_INFO("Init MAC failed\n");
		return rtStatus;
	}

	/* <Kordan> InitHalDm should be put ahead of FirmwareDownload. (HWConfig flow: FW->MAC->-BB->RF) */
	/*rtl8723d_InitHalDm(Adapter);*/

#if 0	/* This statement is put on ex_halbtc8723d2ant_pre_load_firmware() */
	/*
		To inform FW about:
		bit0: "0" for no antenna inverse; "1" for antenna inverse
		bit1: "0" for internal switch; "1" for external switch
		bit2: "0" for one antenna; "1" for two antenna
	*/
	tmpU1b = 0;
	if (pHalData->EEPROMBluetoothAntNum == Ant_x2)
		tmpU1b |= BIT(2); /* two antenna case */

	rtw_write8(Adapter, 0x3E0, tmpU1b);
#endif

#if HAL_FW_ENABLE
	if (Adapter->registrypriv.mp_mode == 0
		#if defined(CONFIG_MP_INCLUDED) && defined(CONFIG_RTW_CUSTOMER_STR)
		|| Adapter->registrypriv.mp_customer_str
		#endif
	) {
		tmpU1b = rtw_read8(Adapter, REG_SYS_CFG);
		rtw_write8(Adapter, REG_SYS_CFG, tmpU1b & 0x7F);

		rtStatus = rtl8723d_FirmwareDownload(Adapter, _FALSE);

		if (rtStatus != _SUCCESS) {
			RTW_INFO("FwLoad failed\n");
			rtStatus = _SUCCESS;
			Adapter->bFWReady = _FALSE;
			pHalData->fw_ractrl = _FALSE;
		} else {
			RTW_INFO("FwLoad SUCCESSFULLY!!!\n");
			Adapter->bFWReady = _TRUE;
			pHalData->fw_ractrl = _TRUE;
		}
	}
#endif

	if (pHalData->AMPDUBurstMode)
		rtw_write8(Adapter, REG_AMPDU_BURST_MODE_8723D, 0x7F);

	pHalData->CurrentChannel = 0;/* set 0 to trigger switch correct channel */

	/* We should call the function before MAC/BB configuration. */
	PHY_InitAntennaSelection8723DE(Adapter);

	/* */
	/* 2. Initialize MAC/PHY Config by MACPHY_reg.txt */
	/* */
#if (HAL_MAC_ENABLE == 1)
	RTW_INFO("8723D MAC Config Start!\n");
	rtStatus = PHY_MACConfig8723D(Adapter);
	if (rtStatus != _SUCCESS) {
		RTW_INFO("8723D MAC Config failed\n");
		return rtStatus;
	}
	RTW_INFO("8723D MAC Config Finished!\n");

	/* without this statement, RCR doesn't take effect so that it will show 'validate_recv_ctrl_frame fail'. */
	/*rtw_write32(Adapter, REG_RCR, rtw_read32(Adapter, REG_RCR) & ~(RCR_ADF));*/
#endif	/*  #if (HAL_MAC_ENABLE == 1) */

	/* */
	/* 3. Initialize BB After MAC Config PHY_reg.txt, AGC_Tab.txt */
	/* */
#if (HAL_BB_ENABLE == 1)
	RTW_INFO("BB Config Start!\n");
	rtStatus = PHY_BBConfig8723D(Adapter);
	if (rtStatus != _SUCCESS) {
		RTW_INFO("BB Config failed\n");
		return rtStatus;
	}

	if (Adapter->registrypriv.wifi_spec == 1)
		ODM_SetBBReg(pDM_Odm, 0xc4c, bMaskDWord, 0x00fe0301);

	RTW_INFO("BB Config Finished!\n");
#endif	/*  #if (HAL_BB_ENABLE == 1) */

	/* */
	/* 4. Initiailze RF RAIO_A.txt RF RAIO_B.txt */
	/* */
#if (HAL_RF_ENABLE == 1)
	RTW_INFO("RF Config started!\n");
	rtStatus = PHY_RFConfig8723D(Adapter);
	if (rtStatus != _SUCCESS) {
		RTW_INFO("RF Config failed\n");
		return rtStatus;
	}

	/*---- Set CCK and OFDM Block "ON"----*/
	PHY_SetBBReg(Adapter, rFPGA0_RFMOD, bCCKEn, 0x1);
	PHY_SetBBReg(Adapter, rFPGA0_RFMOD, bOFDMEn, 0x1);

	RTW_INFO("RF Config Finished!\n");

	pHalData->RfRegChnlVal[0] = PHY_QueryRFReg(Adapter, 0, RF_CHNLBW, bRFRegOffsetMask);
	pHalData->RfRegChnlVal[1] = PHY_QueryRFReg(Adapter, 1, RF_CHNLBW, bRFRegOffsetMask);
	/* Defalut BW should be 20MH at first for the value. because HAL bandwidh defalut = 20MH. */
	/*
	pHalData->RfRegChnlVal[0] &= 0xFFF03FF;
	pHalData->RfRegChnlVal[0] |= (BIT(10) | BIT(11));
	*/

#endif	/*  #if (HAL_RF_ENABLE == 1) */

	_InitBBRegBackup_8723DE(Adapter);

	_InitMacAPLLSetting_8723D(Adapter);

	/* 3 Set Hardware(MAC default setting.) */
	HwConfigureRTL8723DE(Adapter);

	rtw_hal_set_chnl_bw(Adapter, Adapter->registrypriv.channel,
		CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE, HAL_PRIME_CHNL_OFFSET_DONT_CARE);

	/* We should set pHalData->bMACFuncEnable to TRUE after LLT initialize and Fw download */
	/* and before GPIO detect. by tynli. 2011.05.27. */
	/* pHalData->bMACFuncEnable = _TRUE; */

	_BBTurnOnBlock(Adapter);

	/* 3Security related */
	/* ----------------------------------------------------------------------------- */
	/* Set up security related. 070106, by rcnjko: */
	/* 1. Clear all H/W keys. */
	/* ----------------------------------------------------------------------------- */
	invalidate_cam_all(Adapter);

	/* Joseph debug: MAC_SEC_EN need to be set */
	rtw_write8(Adapter, REG_CR + 1, (rtw_read8(Adapter, REG_CR + 1) | BIT(1)));

	/* 2======================================================= */
	/* RF Power Save */
	/* 2======================================================= */
	/* Fix the bug that Hw/Sw radio off before S3/S4, the RF off action will not be executed */
	/* in MgntActSet_RF_State() after wake up, because the value of pHalData->eRFPowerState */
	/* is the same as eRfOff, we should change it to eRfOn after we config RF parameters. */
	/* Added by tynli. 2010.03.30. */
	pwrpriv->rf_pwrstate = rf_on;

	/* if(pPSC->bGpioRfSw) */
	if (0) {
		tmpU1b = rtw_read8(Adapter, 0x60);
		pwrpriv->rfoff_reason |= (tmpU1b & BIT(1)) ? 0 : RF_CHANGE_BY_HW;
	}

	pwrpriv->rfoff_reason |= (pwrpriv->reg_rfoff) ? RF_CHANGE_BY_SW : 0;

	if (pwrpriv->rfoff_reason & RF_CHANGE_BY_HW)
		pwrpriv->b_hw_radio_off = _TRUE;

	if (pwrpriv->rfoff_reason > RF_CHANGE_BY_PS) {
		/* H/W or S/W RF OFF before sleep. */
		RTW_INFO("InitializeAdapter8188EE(): Turn off RF for RfOffReason(%d) ----------\n", pwrpriv->rfoff_reason);
		/* MgntActSet_RF_State(Adapter, rf_off, pwrpriv->rfoff_reason, _TRUE); */
	} else {
		pwrpriv->rf_pwrstate = rf_on;
		pwrpriv->rfoff_reason = 0;

		RTW_INFO("InitializeAdapter8723DE(): Turn on  ----------\n");

		/* LED control */
		rtw_led_control(Adapter, LED_CTL_POWER_ON);

	}

	rtl8723d_InitAntenna_Selection(Adapter);

	/* Fix the bug that when the system enters S3/S4 then tirgger HW radio off, after system */
	/* wakes up, the scan OID will be set from upper layer, but we still in RF OFF state and scan */
	/* list is empty, such that the system might consider the NIC is in RF off state and will wait */
	/* for several seconds (during this time the scan OID will not be set from upper layer anymore) */
	/* even though we have already HW RF ON, so we tell the upper layer our RF state here. */
	/* Added by tynli. 2010.04.01. */
	/* DrvIFIndicateCurrentPhyStatus(Adapter); */

	if (Adapter->registrypriv.hw_wps_pbc) {
		tmpU1b = rtw_read8(Adapter, GPIO_IO_SEL);
		tmpU1b &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
		rtw_write8(Adapter, GPIO_IO_SEL, tmpU1b);	/* enable GPIO[2] as input mode */
	}

	/* */
	/* Execute TX power tracking later */
	/* */

	EnableAspmBackDoor_8723DE(Adapter);
	EnableL1Off_8723DE(Adapter);
	SetPciLtr_8723DE(Adapter);

	/* Init BT hw config. */

	rtl8723d_InitHalDm(Adapter);

	Adapter->init_adpt_in_progress = _FALSE;

#if defined(CONFIG_CONCURRENT_MODE) || defined(CONFIG_TX_MCAST2UNI)

#ifdef CONFIG_CHECK_AC_LIFETIME
	/* Enable lifetime check for the four ACs */
	rtw_write8(Adapter, REG_LIFETIME_CTRL, rtw_read8(Adapter, REG_LIFETIME_CTRL) | 0x0f);
#endif	/* CONFIG_CHECK_AC_LIFETIME */

#ifdef CONFIG_TX_MCAST2UNI
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x0400);	/* unit: 256us. 256ms */
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x0400);	/* unit: 256us. 256ms */
#else	/* CONFIG_TX_MCAST2UNI */
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x3000);	/* unit: 256us. 3s */
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x3000);	/* unit: 256us. 3s */
#endif	/* CONFIG_TX_MCAST2UNI */
#endif	/* CONFIG_CONCURRENT_MODE || CONFIG_TX_MCAST2UNI */


	/* enable tx DMA to drop the redundate data of packet */
	rtw_write16(Adapter, REG_TXDMA_OFFSET_CHK, (rtw_read16(Adapter, REG_TXDMA_OFFSET_CHK) | DROP_DATA_EN));

	pHalData->RegBcnCtrlVal = rtw_read8(Adapter, REG_BCN_CTRL);
	pHalData->RegTxPause = rtw_read8(Adapter, REG_TXPAUSE);
	pHalData->RegFwHwTxQCtrl = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL + 2);
	pHalData->RegReg542 = rtw_read8(Adapter, REG_TBTT_PROHIBIT + 2);

	/* EnableInterrupt8188EE(Adapter); */

#if (MP_DRIVER == 1)
	if (Adapter->registrypriv.mp_mode == 1) {
		Adapter->mppriv.channel = pHalData->CurrentChannel;
		MPT_InitializeAdapter(Adapter, Adapter->mppriv.channel);
	} else
#endif
	{
		struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(Adapter);

		pwrctrlpriv->rf_pwrstate = rf_on;

		if (pwrctrlpriv->rf_pwrstate == rf_on) {
			struct pwrctrl_priv *pwrpriv;
			u32 start_time;
			u8 restore_iqk_rst;
			u8 b2Ant;
			u8 h2cCmdBuf;

			pwrpriv = adapter_to_pwrctl(Adapter);

			PHY_LCCalibrate_8723D(&pHalData->odmpriv);

			/* Inform WiFi FW that it is the beginning of IQK */
			h2cCmdBuf = 1;
			FillH2CCmd8723D(Adapter, H2C_8723D_BT_WLAN_CALIBRATION, 1, &h2cCmdBuf);

			start_time = rtw_get_current_time();
			do {
				if (rtw_read8(Adapter, 0x1e7) & 0x01)
					break;

				rtw_msleep_os(50);
			} while (rtw_get_passing_time_ms(start_time) <= 400);

#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_IQKNotify(Adapter, _TRUE);
#endif
			restore_iqk_rst = (pwrpriv->bips_processing == _TRUE) ? _TRUE : _FALSE;
			b2Ant = (pHalData->EEPROMBluetoothAntNum == Ant_x2) ? _TRUE : _FALSE;
			pHalData->odmpriv.nIQK_Cnt = 0;
			pHalData->odmpriv.nIQK_OK_Cnt = 0;
			pHalData->odmpriv.nIQK_Fail_Cnt = 0;
			PHY_IQCalibrate_8723D(Adapter, _FALSE);/*, restore_iqk_rst, b2Ant, pHalData->ant_path);*/
			pHalData->bIQKInitialized = _TRUE;
#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_IQKNotify(Adapter, _FALSE);
#endif

			/* Inform WiFi FW that it is the finish of IQK */
			h2cCmdBuf = 0;
			FillH2CCmd8723D(Adapter, H2C_8723D_BT_WLAN_CALIBRATION, 1, &h2cCmdBuf);

			ODM_TXPowerTrackingCheck(&pHalData->odmpriv);
		}
	}

#ifdef CONFIG_BT_COEXIST
	/* Init BT hw config. */
	rtw_btcoex_HAL_Initialize(Adapter, _FALSE);
#else
	rtw_btcoex_HAL_Initialize(Adapter, _TRUE);
#endif

#if (defined(CONFIG_ANT_DETECTION))
	ODM_SingleDualAntennaDefaultSetting(pDM_Odm);
#endif

	/*
		tmpU1b = EFUSE_Read1Byte(Adapter, 0x1FA);

		if(!(tmpU1b & BIT(0)))
		{
			PHY_SetRFReg(Adapter, ODM_RF_PATH_A, 0x15, 0x0F, 0x05);
			RTW_INFO("PA BIAS path A\n");
		}

		if(!(tmpU1b & BIT(1)) && is2T2R)
		{
			PHY_SetRFReg(Adapter, ODM_RF_PATH_B, 0x15, 0x0F, 0x05);
			RTW_INFO("PA BIAS path B\n");
		}
	*/

	rtw_hal_set_hwreg(Adapter, HW_VAR_NAV_UPPER, ((u8 *)&NavUpper));


	PHY_SetBBReg(Adapter, rOFDM0_XAAGCCore1, bMaskByte0, 0x50);
	PHY_SetBBReg(Adapter, rOFDM0_XAAGCCore1, bMaskByte0, 0x20);


	/*{
		RTW_INFO("===== Start Dump Reg =====");
		for(i = 0 ; i <= 0xeff ; i+=4)
		{
	#if 1
			u32 tmp;

			if(i%16==0)
				printk("\n%04x: ",i);

			tmp = rtw_read32(Adapter, i);

			printk("%02X %02X %02X %02X ",
					( tmp >> 0 ) & 0xFF,
					( tmp >> 8 ) & 0xFF,
					( tmp >> 16 ) & 0xFF,
					( tmp >> 24 ) & 0xFF
					);
	#else
			if(i%16==0)
				RTW_INFO("\n%04x: ",i);
			RTW_INFO("0x%08x ",rtw_read32(Adapter, i));
	#endif
		}
		RTW_INFO("\n ===== End Dump Reg =====\n");
	}*/


	return rtStatus;
}

/*
 * 2009/10/13 MH Acoording to documetn form Scott/Alfred....
 * This is based on version 8.1.
 */




VOID
hal_poweroff_8723DE(
	IN	PADAPTER			Adapter
)
{
	u8	u1bTmp;
	u8 bMacPwrCtrlOn = _FALSE;

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if (bMacPwrCtrlOn == _FALSE)
		return;

	RTW_INFO("=====>%s\n", __func__);

	/* pHalData->bMACFuncEnable = _FALSE; */

	/* Run LPS WL RFOFF flow */
	HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8723D_enter_lps_flow);



	/* 0x1F[7:0] = 0		/ turn off RF */
	/* rtw_write8(Adapter, REG_RF_CTRL_8812, 0x00); */

	/* ==== Reset digital sequence   ====== */
	if ((rtw_read8(Adapter, REG_MCUFWDL) & RAM_DL_SEL) && Adapter->bFWReady) /* 8051 RAM code */
		rtl8723d_FirmwareSelfReset(Adapter);

	/* Reset MCU. Suggested by Filen. 2011.01.26. by tynli. */
	u1bTmp = rtw_read8(Adapter, REG_SYS_FUNC_EN + 1);
	rtw_write8(Adapter, REG_SYS_FUNC_EN + 1, (u1bTmp & (~BIT(2))));

	/* MCUFWDL 0x80[1:0]=0				/ reset MCU ready status */
	rtw_write8(Adapter, REG_MCUFWDL, 0x00);

	/* HW card disable configuration. */
	HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8723D_card_disable_flow);

	/* Reset MCU IO Wrapper */
	u1bTmp = rtw_read8(Adapter, REG_RSV_CTRL + 1);
	rtw_write8(Adapter, REG_RSV_CTRL + 1, (u1bTmp & (~BIT(0))));
	u1bTmp = rtw_read8(Adapter, REG_RSV_CTRL + 1);
	rtw_write8(Adapter, REG_RSV_CTRL + 1, u1bTmp | BIT(0));

	/* RSV_CTRL 0x1C[7:0] = 0x0E			/ lock ISO/CLK/Power control register */
	rtw_write8(Adapter, REG_RSV_CTRL, 0x0e);

	Adapter->bFWReady = _FALSE;
	bMacPwrCtrlOn = _FALSE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
}

VOID
PowerOffAdapter8723DE(
	IN	PADAPTER			Adapter
)
{
	rtw_hal_power_off(Adapter);
}

static u32 rtl8723de_hal_deinit(PADAPTER Adapter)
{
	u8	u1bTmp = 0;
	u32	count = 0;
	u8	bSupportRemoteWakeUp = _FALSE;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);


	RTW_INFO("====> %s()\n", __func__);

	if (Adapter->bHaltInProgress == _TRUE) {
		RTW_INFO("====> Abort rtl8723de_hal_deinit()\n");
		return _FAIL;
	}

	Adapter->bHaltInProgress = _TRUE;

	/* */
	/* No I/O if device has been surprise removed */
	/* */
	if (rtw_is_surprise_removed(Adapter)) {
		Adapter->bHaltInProgress = _FALSE;
		return _SUCCESS;
	}

	RT_SET_PS_LEVEL(pwrpriv, RT_RF_OFF_LEVL_HALT_NIC);

	/* Without supporting WoWLAN or the driver is in awake (D0) state, we should */
	if (!bSupportRemoteWakeUp) {/* ||!pMgntInfo->bPwrSaveState) */
		/* 2009/10/13 MH For power off test. */
		PowerOffAdapter8723DE(Adapter);
	}
#if 0
	else {
		s32	rtStatus = _SUCCESS;

		/* -------------------------------------------------------- */
		/* 3 <1> Prepare for configuring wowlan related infomations */
		/* -------------------------------------------------------- */

		/* Clear Fw WoWLAN event. */
		rtw_write8(Adapter, REG_MCUTST_WOWLAN, 0x0);

#if (USE_SPECIFIC_FW_TO_SUPPORT_WOWLAN == 1)
		SetFwRelatedForWoWLAN8812A(Adapter, _TRUE);
#endif

		SetWoWLANCAMEntry8812(Adapter);
		PlatformEFIOWrite1Byte(Adapter, REG_WKFMCAM_NUM_8812, pPSC->WoLPatternNum);

		/* Dynamically adjust Tx packet boundary for download reserved page packet. */
		rtStatus = DynamicRQPN8812AE(Adapter, 0xE0, 0x3, 0x80c20d0d); /* reserve 30 pages for rsvd page */
		if (rtStatus == RT_STATUS_SUCCESS)
			pHalData->bReInitLLTTable = _TRUE;

		/* -------------------------------------------------------- */
		/* 3 <2> Set Fw releted H2C cmd. */
		/* -------------------------------------------------------- */

		/* Set WoWLAN related security information. */
		SetFwGlobalInfoCmd_8812(Adapter);

		HalDownloadRSVDPage8812E(Adapter, _TRUE);


		/* Just enable AOAC related functions when we connect to AP. */
		if (pMgntInfo->mAssoc && pMgntInfo->OpMode == RT_OP_MODE_INFRASTRUCTURE) {
			/* Set Join Bss Rpt H2C cmd and download RSVD page. */
			/* Fw will check media status to send null packet and perform WoWLAN LPS. */
			Adapter->HalFunc.SetHwRegHandler(Adapter, HW_VAR_AID, 0);
			MacIdIndicateSpecificMediaStatus(Adapter, MAC_ID_STATIC_FOR_DEFAULT_PORT, RT_MEDIA_CONNECT);

			HalSetFWWoWlanMode8812(Adapter, _TRUE);
			/* Enable Fw Keep alive mechanism. */
			HalSetFwKeepAliveCmd8812(Adapter, _TRUE);

			/* Enable disconnect decision control. */
			SetFwDisconnectDecisionCtrlCmd_8812(Adapter, _TRUE);
		}

		/* -------------------------------------------------------- */
		/* 3 <3> Hw Configutations */
		/* -------------------------------------------------------- */

		/* Wait until Rx DMA Finished before host sleep. FW Pause Rx DMA may happens when received packet doing dma.  /YJ,add,111101 */
		rtw_write8(Adapter, REG_RXDMA_CONTROL_8812A, BIT(2));

		u1bTmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812A);
		count = 0;
		while (!(u1bTmp & BIT(1)) && (count++ < 100)) {
			rtw_udelay_os(10); /* 10 us */
			u1bTmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812A);
		}
		RTW_INFO("HaltAdapter8812E(): 222 Wait until Rx DMA Finished before host sleep. count=%d\n", count);

		rtw_write8(Adapter, REG_APS_FSMCO + 1, 0x0);

		PlatformClearPciPMEStatus(Adapter);

		/* tynli_test for normal chip wowlan. 2010.01.26. Suggested by Sd1 Isaac and designer Alfred. */
		rtw_write8(Adapter, REG_SYS_CLKR, (rtw_read8(Adapter, REG_SYS_CLKR) | BIT(3)));

		/* prevent 8051 to be reset by PERST# */
		rtw_write8(Adapter, REG_RSV_CTRL, 0x20);
		rtw_write8(Adapter, REG_RSV_CTRL, 0x60);
	}

	/* For wowlan+LPS+32k. */
	if (bSupportRemoteWakeUp && Adapter->bEnterPnpSleep) {
		BOOLEAN		bEnterFwLPS = TRUE;
		u1Byte		QueueID;
		PRT_TCB		pTcb;


		/* Set the WoWLAN related function control enable. */
		/* It should be the last H2C cmd in the WoWLAN flow. 2012.02.10. by tynli. */
		SetFwRemoteWakeCtrlCmd_8723D(Adapter, 1);

		/* Stop Pcie Interface Tx DMA. */
		PlatformEFIOWrite1Byte(Adapter, REG_PCIE_CTRL_REG_8723D + 1, 0xff);

		/* Wait for TxDMA idle. */
		count = 0;
		do {
			u1bTmp = PlatformEFIORead1Byte(Adapter, REG_PCIE_CTRL_REG_8723D);
			PlatformSleepUs(10); /* 10 us */
			count++;
		} while ((u1bTmp != 0) && (count < 100));

		/* Set Fw to enter LPS mode. */
		if (pMgntInfo->mAssoc &&
		    pMgntInfo->OpMode == RT_OP_MODE_INFRASTRUCTURE &&
		    (pPSC->WoWLANLPSLevel > 0)) {

			Adapter->HalFunc.SetHwRegHandler(Adapter, HW_VAR_FW_LPS_ACTION, (pu1Byte)(&bEnterFwLPS));
			pPSC->bEnterLPSDuringSuspend = TRUE;
		}

		PlatformAcquireSpinLock(Adapter, RT_TX_SPINLOCK);
		/* guangan, 2009/08/28, return TCB in busy queue to idle queue when resume from S3/S4. */
		for (QueueID = 0; QueueID < MAX_TX_QUEUE; QueueID++) {
			/* 2004.08.11, revised by rcnjko. */
			while (!RTIsListEmpty(&Adapter->TcbBusyQueue[QueueID])) {
				pTcb = (PRT_TCB)RTRemoveHeadList(&Adapter->TcbBusyQueue[QueueID]);
				ReturnTCB(Adapter, pTcb, RT_STATUS_SUCCESS);
			}
		}

		PlatformReleaseSpinLock(Adapter, RT_TX_SPINLOCK);

		if (pHalData->HwROFEnable) {
			u1bTmp = PlatformEFIORead1Byte(Adapter, REG_HSISR_8812 + 3);
			PlatformEFIOWrite1Byte(Adapter, REG_HSISR_8812 + 3, u1bTmp | BIT(1));
		}
	}
#endif

	RTW_INFO("%s() <====\n", __func__);

	Adapter->bHaltInProgress = _FALSE;


	return _SUCCESS;
}

void SetHwReg8723DE(PADAPTER Adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);


	switch (variable) {
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
			rtw_write8(Adapter, REG_PCIE_HRPWM, ps_state);
		}
#endif
		break;
	case HW_VAR_AMPDU_MAX_TIME: {
		u8	maxRate = *(u8 *)val;

		rtw_write8(Adapter, REG_AMPDU_MAX_TIME_8723D, 0x70);
	}
	break;
	case HW_VAR_PCIE_STOP_TX_DMA: {
		u16 tmp16;

		tmp16 = rtw_read16(Adapter, REG_PCIE_CTRL_REG_8723D);
		tmp16 |= 0x7FFF;
		rtw_write16(Adapter, REG_PCIE_CTRL_REG_8723D, tmp16);
	}
	break;
	case HW_VAR_DM_IN_LPS:	/* [NOTE] copy from 8723ds */
		rtl8723d_hal_dm_in_lps(Adapter);
		break;
#ifdef CONFIG_GPIO_WAKEUP
	case HW_SET_GPIO_WL_CTRL:
		RTW_INFO("%s: check GPIO pin mux first!!\n", __func__);
		break;
#endif
	default:
		SetHwReg8723D(Adapter, variable, val);
		break;
	}

}

void GetHwReg8723DE(PADAPTER Adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	/* DM_ODM_T			*podmpriv = &pHalData->odmpriv; */

	switch (variable) {
	default:
		GetHwReg8723D(Adapter, variable, val);
		break;
	}

}

/*
 *	Description:
 *		Change default setting of specified variable.
 */
u8
SetHalDefVar8723DE(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch (eVariable) {
	case HAL_DEF_PCI_SUUPORT_L1_BACKDOOR:
		pHalData->bSupportBackDoor = *((PBOOLEAN)pValue);
		break;
	default:
		SetHalDefVar8723D(Adapter, eVariable, pValue);
		break;
	}

	return bResult;
}

/*
 *	Description:
 *		Query setting of specified variable.
 */
u8
GetHalDefVar8723DE(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch (eVariable) {
	case HAL_DEF_PCI_SUUPORT_L1_BACKDOOR:
		*((PBOOLEAN)pValue) = pHalData->bSupportBackDoor;
		break;

	case HAL_DEF_PCI_AMD_L1_SUPPORT:
		*((PBOOLEAN)pValue) = _TRUE;/* Support L1 patch on AMD platform in default, added by Roger, 2012.04.30. */
		break;

#ifdef CONFIG_ANTENNA_DIVERSITY
	case HAL_DEF_IS_SUPPORT_ANT_DIV:
		*((u8 *)pValue) = (pHalData->AntDivCfg == 0) ? _FALSE : _TRUE;
		break;
#endif
	case HAL_DEF_DRVINFO_SZ:
		*((u32 *)pValue) = DRVINFO_SZ;
		break;

	default:
		GetHalDefVar8723D(Adapter, eVariable, pValue);
		break;
	}

	return bResult;
}

static void rtl8723de_init_default_value(_adapter *padapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	rtl8723d_init_default_value(padapter);

	pHalData->CurrentWirelessMode = WIRELESS_MODE_AUTO;
	pHalData->bDefaultAntenna = 1;

	/* */
	/* Set TCR-Transmit Control Register. The value is set in InitializeAdapter8190Pci() */
	/* */
	pHalData->TransmitConfig = CFENDFORM | BIT(12) | BIT(13);

	/* */
	/* Set RCR-Receive Control Register . The value is set in InitializeAdapter8190Pci(). */
	/* */
	pHalData->ReceiveConfig = (
					  /*RCR_APPFCS			|*/
					  RCR_APP_MIC			|
					  RCR_APP_ICV			|
					  RCR_APP_PHYST_RXFF	|
					  /*RCR_VHT_DACK	|*/
					  RCR_HTC_LOC_CTRL	|
					  RCR_AMF				|
					  /*RCR_ACF				|*/
					  RCR_ADF		| /* Note: This bit controls the PS-Poll packet filter. */
					  /*RCR_AICV			|*/
					  /*RCR_ACRC32			|*/
					  RCR_CBSSID_BCN		|
					  RCR_CBSSID_DATA		|
					  RCR_AB				|
					  RCR_AM				|
					  RCR_APM				|
					  0);


	/* */
	/* Set Interrupt Mask Register */
	/* */
	pHalData->IntrMaskDefault[0]	= (u32)(
			IMR_PSTIMEOUT_8723D		|
			/* IMR_GTINT4_8723D			| */
			IMR_GTINT3_8723D			|
			IMR_TXBCN0ERR_8723D		|
			IMR_TXBCN0OK_8723D		|
			IMR_BCNDMAINT0_8723D	|
			IMR_HSISR_IND_ON_INT_8723D	|
			IMR_C2HCMD_8723D		|
			/* IMR_CPWM_8723D			| */
			IMR_HIGHDOK_8723D		|
			IMR_MGNTDOK_8723D		|
			IMR_BKDOK_8723D			|
			IMR_BEDOK_8723D			|
			IMR_VIDOK_8723D			|
			IMR_VODOK_8723D			|
			IMR_RDU_8723D			|
			IMR_ROK_8723D			|
			0);
	pHalData->IntrMaskDefault[1] = (u32)(
					       IMR_RXFOVW_8723D		|
					       IMR_TXFOVW_8723D		|
					       0);

	/* 2012/03/27 hpfan Add for win8 DTM DPC ISR test */
	pHalData->IntrMaskReg[0]	=	(u32)(
			IMR_RDU_8723D		|
			IMR_PSTIMEOUT_8723D	|
			0);
	pHalData->IntrMaskReg[1]	=	(u32)(
			IMR_C2HCMD_8723D		|
			0);

	/* if (padapter->bUseThreadHandleInterrupt) */
	/* { */
	/*	pHalData->IntrMask[0] = pHalData->IntrMaskReg[0]; */
	/*	pHalData->IntrMask[1] = pHalData->IntrMaskReg[1]; */
	/* } */
	/* else */
	{
		pHalData->IntrMask[0] = pHalData->IntrMaskDefault[0];
		pHalData->IntrMask[1] = pHalData->IntrMaskDefault[1];
	}


	if (1) {/* IS_HARDWARE_TYPE_8723DE(padapter)) */

		pHalData->SysIntrMask[0] = (u32)(
						   HSIMR_PDN_INT_EN		|
						   HSIMR_RON_INT_EN		|
						   0);
	}
}

void rtl8723de_set_hal_ops(_adapter *padapter)
{
	struct hal_ops	*pHalFunc = &padapter->HalFunc;


	rtl8723d_set_hal_ops(pHalFunc);

	pHalFunc->hal_power_on = &_InitPowerOn_8723DE;
	pHalFunc->hal_power_off = &hal_poweroff_8723DE;
	pHalFunc->hal_init = &rtl8723de_hal_init;
	pHalFunc->hal_deinit = &rtl8723de_hal_deinit;

	pHalFunc->inirp_init = &rtl8723de_init_desc_ring;
	pHalFunc->inirp_deinit = &rtl8723de_free_desc_ring;
	pHalFunc->irp_reset = &rtl8723de_reset_desc_ring;

	pHalFunc->init_xmit_priv = &rtl8723de_init_xmit_priv;
	pHalFunc->free_xmit_priv = &rtl8723de_free_xmit_priv;

	pHalFunc->init_recv_priv = &rtl8723de_init_recv_priv;
	pHalFunc->free_recv_priv = &rtl8723de_free_recv_priv;
#ifdef CONFIG_SW_LED
	pHalFunc->InitSwLeds = &rtl8723de_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8723de_DeInitSwLeds;
#else /* case of hw led or no led */
	pHalFunc->InitSwLeds = NULL;
	pHalFunc->DeInitSwLeds = NULL;
#endif/* CONFIG_SW_LED */

	pHalFunc->init_default_value = &rtl8723de_init_default_value;
	pHalFunc->intf_chip_configure = &rtl8723de_interface_configure;
	pHalFunc->read_adapter_info = &ReadAdapterInfo8723DE;

	pHalFunc->enable_interrupt = &EnableInterrupt8723DE;
	pHalFunc->disable_interrupt = &DisableInterrupt8723DE;
	pHalFunc->interrupt_handler = &rtl8723de_interrupt;

	pHalFunc->SetHwRegHandler = &SetHwReg8723DE;
	pHalFunc->GetHwRegHandler = &GetHwReg8723DE;
	pHalFunc->GetHalDefVarHandler = &GetHalDefVar8723DE;
	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8723DE;

	pHalFunc->hal_xmit = &rtl8723de_hal_xmit;
	pHalFunc->mgnt_xmit = &rtl8723de_mgnt_xmit;
	pHalFunc->hal_xmitframe_enqueue = &rtl8723de_hal_xmitframe_enqueue;

#ifdef CONFIG_HOSTAPD_MLME
	pHalFunc->hostap_mgnt_xmit_entry = &rtl8723de_hostap_mgnt_xmit_entry;
#endif

#ifdef CONFIG_XMIT_THREAD_MODE
	pHalFunc->xmit_thread_handler = &rtl8723de_xmit_buf_handler;
#endif


}
