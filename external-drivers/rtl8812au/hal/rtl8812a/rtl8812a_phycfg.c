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
#define _RTL8812A_PHYCFG_C_

#include <rtl8812a_hal.h>

/* Manual Transmit Power Control 
   The following options take values from 0 to 63, where:
   0 - disable
   1 - lowest transmit power the device can do
   2 - highest transmit power the device can do
   Note that these options may override your country's regulations about transmit power.
   Setting the device to work at higher transmit powers most of the time may cause premature 
   failure or damage by overheating. Make sure the device has enough airflow before you increase this.
   It is currently unknown what these values translate to in dBm.
*/

// Transmit Power Boost
// This value is added to the device's calculation of transmit power index.
// Useful if you want to keep power usage low while still boosting/decreasing transmit power.
// Can take a negative value as well to reduce power.
// Zero disables it. Default: 2, for a tiny boost.
int transmit_power_boost = 2;
// (ADVANCED) To know what transmit powers this device decides to use dynamically, see:
// https://github.com/lwfinger/rtl8192ee/blob/42ad92dcc71cb15a62f8c39e50debe3a28566b5f/hal/phydm/rtl8192e/halhwimg8192e_rf.c#L1310

// Transmit Power Override
// This value completely overrides the driver's calculations and uses only one value for all transmissions.
// Zero disables it. Default: 0
int transmit_power_override = 0;

/* Manual Transmit Power Control */

u32
PHY_QueryBBReg8812(
	IN	PADAPTER	Adapter,
	IN	u32			RegAddr,
	IN	u32			BitMask
)
{
	u32	ReturnValue = 0, OriginalValue, BitShift;

#if (DISABLE_BB_RF == 1)
	return 0;
#endif

	/* RTW_INFO("--->PHY_QueryBBReg8812(): RegAddr(%#x), BitMask(%#x)\n", RegAddr, BitMask); */


	OriginalValue = rtw_read32(Adapter, RegAddr);
	BitShift = PHY_CalculateBitShift(BitMask);
	ReturnValue = (OriginalValue & BitMask) >> BitShift;

	/* RTW_INFO("BBR MASK=0x%x Addr[0x%x]=0x%x\n", BitMask, RegAddr, OriginalValue); */
	return ReturnValue;
}


VOID
PHY_SetBBReg8812(
	IN	PADAPTER	Adapter,
	IN	u4Byte		RegAddr,
	IN	u4Byte		BitMask,
	IN	u4Byte		Data
)
{
	u4Byte			OriginalValue, BitShift;

#if (DISABLE_BB_RF == 1)
	return;
#endif

	if (BitMask != bMaskDWord) {
		/* if not "double word" write */
		OriginalValue = rtw_read32(Adapter, RegAddr);
		BitShift = PHY_CalculateBitShift(BitMask);
		Data = ((OriginalValue)&(~BitMask)) | (((Data << BitShift)) & BitMask);
	}

	rtw_write32(Adapter, RegAddr, Data);

	/* RTW_INFO("BBW MASK=0x%x Addr[0x%x]=0x%x\n", BitMask, RegAddr, Data); */
}

/*
 * 2. RF register R/W API
 *   */

static	u32
phy_RFSerialRead(
	IN	PADAPTER		Adapter,
	IN	enum rf_path		eRFPath,
	IN	u32				Offset
)
{
	u32							retValue = 0;
	HAL_DATA_TYPE				*pHalData = GET_HAL_DATA(Adapter);
	BB_REGISTER_DEFINITION_T	*pPhyReg = &pHalData->PHYRegDef[eRFPath];
	BOOLEAN						bIsPIMode = _FALSE;


	_enter_critical_mutex(&(adapter_to_dvobj(Adapter)->rf_read_reg_mutex) , NULL);
	/* 2009/06/17 MH We can not execute IO for power save or other accident mode. */
	/* if(RT_CANNOT_IO(Adapter)) */
	/* { */
	/*	RT_DISP(FPHY, PHY_RFR, ("phy_RFSerialRead return all one\n")); */
	/*	return	0xFFFFFFFF; */
	/* } */

	/* <20120809, Kordan> CCA OFF(when entering), asked by James to avoid reading the wrong value. */
	/* <20120828, Kordan> Toggling CCA would affect RF 0x0, skip it! */
	if (Offset != 0x0 &&  !(IS_VENDOR_8812A_C_CUT(Adapter) || IS_HARDWARE_TYPE_8821(Adapter)))
		phy_set_bb_reg(Adapter, rCCAonSec_Jaguar, 0x8, 1);

	Offset &= 0xff;

	if (eRFPath == RF_PATH_A)
		bIsPIMode = (BOOLEAN)phy_query_bb_reg(Adapter, 0xC00, 0x4);
	else if (eRFPath == RF_PATH_B)
		bIsPIMode = (BOOLEAN)phy_query_bb_reg(Adapter, 0xE00, 0x4);

	phy_set_bb_reg(Adapter, pPhyReg->rfHSSIPara2, bHSSIRead_addr_Jaguar, Offset);

	if (IS_VENDOR_8812A_C_CUT(Adapter) || IS_HARDWARE_TYPE_8821(Adapter))
		rtw_udelay_os(20);

	if (bIsPIMode) {
		retValue = phy_query_bb_reg(Adapter, pPhyReg->rfLSSIReadBackPi, rRead_data_Jaguar);
		/* RTW_INFO("[PI mode] RFR-%d Addr[0x%x]=0x%x\n", eRFPath, pPhyReg->rfLSSIReadBackPi, retValue); */
	} else {
		retValue = phy_query_bb_reg(Adapter, pPhyReg->rfLSSIReadBack, rRead_data_Jaguar);
		/* RTW_INFO("[SI mode] RFR-%d Addr[0x%x]=0x%x\n", eRFPath, pPhyReg->rfLSSIReadBack, retValue); */
	}

	/* <20120809, Kordan> CCA ON(when exiting), asked by James to avoid reading the wrong value. */
	/* <20120828, Kordan> Toggling CCA would affect RF 0x0, skip it! */
	if (Offset != 0x0 &&  !(IS_VENDOR_8812A_C_CUT(Adapter) || IS_HARDWARE_TYPE_8821(Adapter)))
		phy_set_bb_reg(Adapter, rCCAonSec_Jaguar, 0x8, 0);

	_exit_critical_mutex(&(adapter_to_dvobj(Adapter)->rf_read_reg_mutex), NULL);
	return retValue;
}


static	VOID
phy_RFSerialWrite(
	IN	PADAPTER		Adapter,
	IN	enum rf_path		eRFPath,
	IN	u32				Offset,
	IN	u32				Data
)
{
	u32							DataAndAddr = 0;
	HAL_DATA_TYPE				*pHalData = GET_HAL_DATA(Adapter);
	BB_REGISTER_DEFINITION_T	*pPhyReg = &pHalData->PHYRegDef[eRFPath];

	/* 2009/06/17 MH We can not execute IO for power save or other accident mode. */
	/* if(RT_CANNOT_IO(Adapter)) */
	/* { */
	/*	RTPRINT(FPHY, PHY_RFW, ("phy_RFSerialWrite stop\n")); */
	/*	return; */
	/* } */

	Offset &= 0xff;

	/* Shadow Update */
	/* PHY_RFShadowWrite(Adapter, eRFPath, Offset, Data); */

	/* Put write addr in [27:20]  and write data in [19:00] */
	DataAndAddr = ((Offset << 20) | (Data & 0x000fffff)) & 0x0fffffff;

	/* Write Operation */
	/* TODO: Dynamically determine whether using PI or SI to write RF registers. */
	phy_set_bb_reg(Adapter, pPhyReg->rf3wireOffset, bMaskDWord, DataAndAddr);
	/* RTW_INFO("RFW-%d Addr[0x%x]=0x%x\n", eRFPath, pPhyReg->rf3wireOffset, DataAndAddr); */

}

u32
PHY_QueryRFReg8812(
	IN	PADAPTER		Adapter,
	IN	enum rf_path		eRFPath,
	IN	u32				RegAddr,
	IN	u32				BitMask
)
{
	u32				Original_Value, Readback_Value, BitShift;

#if (DISABLE_BB_RF == 1)
	return 0;
#endif

	Original_Value = phy_RFSerialRead(Adapter, eRFPath, RegAddr);

	BitShift =  PHY_CalculateBitShift(BitMask);
	Readback_Value = (Original_Value & BitMask) >> BitShift;

	return Readback_Value;
}

VOID
PHY_SetRFReg8812(
	IN	PADAPTER		Adapter,
	IN	enum rf_path		eRFPath,
	IN	u32				RegAddr,
	IN	u32				BitMask,
	IN	u32				Data
)
{
#if (DISABLE_BB_RF == 1)
	return;
#endif

	if (BitMask == 0)
		return;

	/* RF data is 20 bits only */
	if (BitMask != bLSSIWrite_data_Jaguar) {
		u32	Original_Value, BitShift;
		Original_Value = phy_RFSerialRead(Adapter, eRFPath, RegAddr);
		BitShift =  PHY_CalculateBitShift(BitMask);
		Data = ((Original_Value)&(~BitMask)) | (Data << BitShift);
	}

	phy_RFSerialWrite(Adapter, eRFPath, RegAddr, Data);

}

/*
 * 3. Initial MAC/BB/RF config by reading MAC/BB/RF txt.
 *   */

s32 PHY_MACConfig8812(PADAPTER Adapter)
{
	int				rtStatus = _FAIL;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	/*  */
	/* Config MAC */
	/*  */
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	rtStatus = phy_ConfigMACWithParaFile(Adapter, PHY_FILE_MAC_REG);
	if (rtStatus == _FAIL)
#endif
	{
#ifdef CONFIG_EMBEDDED_FWIMG
		odm_config_mac_with_header_file(&pHalData->odmpriv);
		rtStatus = _SUCCESS;
#endif/* CONFIG_EMBEDDED_FWIMG */
	}

	return rtStatus;
}


static	VOID
phy_InitBBRFRegisterDefinition(
	IN	PADAPTER		Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);

	/* RF Interface Sowrtware Control */
	pHalData->PHYRegDef[RF_PATH_A].rfintfs = rFPGA0_XAB_RFInterfaceSW; /* 16 LSBs if read 32-bit from 0x870 */
	pHalData->PHYRegDef[RF_PATH_B].rfintfs = rFPGA0_XAB_RFInterfaceSW; /* 16 MSBs if read 32-bit from 0x870 (16-bit for 0x872) */

	/* RF Interface Output (and Enable) */
	pHalData->PHYRegDef[RF_PATH_A].rfintfo = rFPGA0_XA_RFInterfaceOE; /* 16 LSBs if read 32-bit from 0x860 */
	pHalData->PHYRegDef[RF_PATH_B].rfintfo = rFPGA0_XB_RFInterfaceOE; /* 16 LSBs if read 32-bit from 0x864 */

	/* RF Interface (Output and)  Enable */
	pHalData->PHYRegDef[RF_PATH_A].rfintfe = rFPGA0_XA_RFInterfaceOE; /* 16 MSBs if read 32-bit from 0x860 (16-bit for 0x862) */
	pHalData->PHYRegDef[RF_PATH_B].rfintfe = rFPGA0_XB_RFInterfaceOE; /* 16 MSBs if read 32-bit from 0x864 (16-bit for 0x866) */

	pHalData->PHYRegDef[RF_PATH_A].rf3wireOffset = rA_LSSIWrite_Jaguar; /* LSSI Parameter */
	pHalData->PHYRegDef[RF_PATH_B].rf3wireOffset = rB_LSSIWrite_Jaguar;

	pHalData->PHYRegDef[RF_PATH_A].rfHSSIPara2 = rHSSIRead_Jaguar;  /* wire control parameter2 */
	pHalData->PHYRegDef[RF_PATH_B].rfHSSIPara2 = rHSSIRead_Jaguar;  /* wire control parameter2 */

	/* Tranceiver Readback LSSI/HSPI mode */
	pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBack = rA_SIRead_Jaguar;
	pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBack = rB_SIRead_Jaguar;
	pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBackPi = rA_PIRead_Jaguar;
	pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBackPi = rB_PIRead_Jaguar;
}

VOID
PHY_BB8812_Config_1T(
	IN PADAPTER Adapter
)
{
	/* BB OFDM RX Path_A */
	phy_set_bb_reg(Adapter, rRxPath_Jaguar, bRxPath_Jaguar, 0x11);
	/* BB OFDM TX Path_A */
	phy_set_bb_reg(Adapter, rTxPath_Jaguar, bMaskLWord, 0x1111);
	/* BB CCK R/Rx Path_A */
	phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, bCCK_RX_Jaguar, 0x0);
	/* MCS support */
	phy_set_bb_reg(Adapter, 0x8bc, 0xc0000060, 0x4);
	/* RF Path_B HSSI OFF */
	phy_set_bb_reg(Adapter, 0xe00, 0xf, 0x4);
	/* RF Path_B Power Down */
	phy_set_bb_reg(Adapter, 0xe90, bMaskDWord, 0);
	/* ADDA Path_B OFF */
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0);
	phy_set_bb_reg(Adapter, 0xe64, bMaskDWord, 0);
}


static	int
phy_BB8812_Config_ParaFile(
	IN	PADAPTER	Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	int			rtStatus = _SUCCESS;

	/* Read PHY_REG.TXT BB INIT!! */
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	if (phy_ConfigBBWithParaFile(Adapter, PHY_FILE_PHY_REG, CONFIG_BB_PHY_REG) == _FAIL)
#endif
	{
#ifdef CONFIG_EMBEDDED_FWIMG
		if (HAL_STATUS_SUCCESS != odm_config_bb_with_header_file(&pHalData->odmpriv, CONFIG_BB_PHY_REG))
			rtStatus = _FAIL;
#endif
	}

	if (rtStatus != _SUCCESS) {
		RTW_INFO("%s(): CONFIG_BB_PHY_REG Fail!!\n", __FUNCTION__);
		goto phy_BB_Config_ParaFile_Fail;
	}

	/* Read PHY_REG_MP.TXT BB INIT!! */
#if (MP_DRIVER == 1)
	if (Adapter->registrypriv.mp_mode == 1) {
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
		if (phy_ConfigBBWithMpParaFile(Adapter, PHY_FILE_PHY_REG_MP) == _FAIL)
#endif
		{
#ifdef CONFIG_EMBEDDED_FWIMG
			if (HAL_STATUS_SUCCESS != odm_config_bb_with_header_file(&pHalData->odmpriv, CONFIG_BB_PHY_REG_MP))
				rtStatus = _FAIL;
#endif
		}

		if (rtStatus != _SUCCESS) {
			RTW_INFO("phy_BB8812_Config_ParaFile():Write BB Reg MP Fail!!\n");
			goto phy_BB_Config_ParaFile_Fail;
		}
	}
#endif	/*  #if (MP_DRIVER == 1) */

	/* BB AGC table Initialization */
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	if (phy_ConfigBBWithParaFile(Adapter, PHY_FILE_AGC_TAB, CONFIG_BB_AGC_TAB) == _FAIL)
#endif
	{
#ifdef CONFIG_EMBEDDED_FWIMG
		if (HAL_STATUS_SUCCESS != odm_config_bb_with_header_file(&pHalData->odmpriv, CONFIG_BB_AGC_TAB))
			rtStatus = _FAIL;
#endif
	}

	if (rtStatus != _SUCCESS)
		RTW_INFO("%s(): CONFIG_BB_AGC_TAB Fail!!\n", __FUNCTION__);

phy_BB_Config_ParaFile_Fail:

	return rtStatus;
}

int
PHY_BBConfig8812(
	IN	PADAPTER	Adapter
)
{
	int	rtStatus = _SUCCESS;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8	TmpU1B = 0;

	phy_InitBBRFRegisterDefinition(Adapter);

	/* tangw check start 20120412 */
	/* . APLL_EN,,APLL_320_GATEB,APLL_320BIAS,  auto config by hw fsm after pfsm_go (0x4 bit 8) set */
	TmpU1B = rtw_read8(Adapter, REG_SYS_FUNC_EN);

	if (IS_HARDWARE_TYPE_8812AU(Adapter) || IS_HARDWARE_TYPE_8821U(Adapter))
		TmpU1B |= FEN_USBA;
	else  if (IS_HARDWARE_TYPE_8812E(Adapter) || IS_HARDWARE_TYPE_8821E(Adapter))
		TmpU1B |= FEN_PCIEA;

	rtw_write8(Adapter, REG_SYS_FUNC_EN, TmpU1B);

	rtw_write8(Adapter, REG_SYS_FUNC_EN, (TmpU1B | FEN_BB_GLB_RSTn | FEN_BBRSTB)); /* same with 8812 */
	/* 6. 0x1f[7:0] = 0x07 PathA RF Power On */
	rtw_write8(Adapter, REG_RF_CTRL, 0x07);/* RF_SDMRSTB,RF_RSTB,RF_EN same with 8723a */
	/* 7.  PathB RF Power On */
	rtw_write8(Adapter, REG_OPT_CTRL_8812 + 2, 0x7); /* RF_SDMRSTB,RF_RSTB,RF_EN same with 8723a */
	/* tangw check end 20120412 */


	/*  */
	/* Config BB and AGC */
	/*  */
	rtStatus = phy_BB8812_Config_ParaFile(Adapter);

	hal_set_crystal_cap(Adapter, pHalData->crystal_cap);

	return rtStatus;
}

int
PHY_RFConfig8812(
	IN	PADAPTER	Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	int		rtStatus = _SUCCESS;

	if (RTW_CANNOT_RUN(Adapter))
		return _FAIL;

	switch (pHalData->rf_chip) {
	case RF_PSEUDO_11N:
		RTW_INFO("%s(): RF_PSEUDO_11N\n", __FUNCTION__);
		break;
	default:
		rtStatus = PHY_RF6052_Config_8812(Adapter);
		break;
	}

	return rtStatus;
}

VOID
PHY_TxPowerTrainingByPath_8812(
	IN	PADAPTER			Adapter,
	IN	enum channel_width	BandWidth,
	IN	u8					Channel,
	IN	u8					RfPath
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	u8	i;
	u32	PowerLevel, writeData, writeOffset;

	if (RfPath >= pHalData->NumTotalRFPath)
		return;

	writeData = 0;
	if (RfPath == RF_PATH_A) {
		PowerLevel = phy_get_tx_power_index(Adapter, RF_PATH_A, MGN_MCS7, BandWidth, Channel);
		writeOffset =  rA_TxPwrTraing_Jaguar;
	} else {
		PowerLevel = phy_get_tx_power_index(Adapter, RF_PATH_B, MGN_MCS7, BandWidth, Channel);
		writeOffset =  rB_TxPwrTraing_Jaguar;
	}

	for (i = 0; i < 3; i++) {
		if (i == 0)
			PowerLevel = PowerLevel - 10;
		else if (i == 1)
			PowerLevel = PowerLevel - 8;
		else
			PowerLevel = PowerLevel - 6;
		writeData |= (((PowerLevel > 2) ? (PowerLevel) : 2) << (i * 8));
	}

	phy_set_bb_reg(Adapter, writeOffset, 0xffffff, writeData);
}

VOID
PHY_GetTxPowerLevel8812(
	IN	PADAPTER		Adapter,
	OUT s32		*powerlevel
)
{
	/*HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	*powerlevel = pHalData->CurrentTxPwrIdx;*/
#if 0
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	s4Byte			TxPwrDbm = 13;

	if (pMgntInfo->ClientConfigPwrInDbm != UNSPECIFIED_PWR_DBM)
		*powerlevel = pMgntInfo->ClientConfigPwrInDbm;
	else
		*powerlevel = TxPwrDbm;
#endif
}

/* create new definition of PHY_SetTxPowerLevel8812 by YP.
 * Page revised on 20121106
 * the new way to set tx power by rate, NByte access, here N byte shall be 4 byte(DWord) or NByte(N>4) access. by page/YP, 20121106 */
VOID
PHY_SetTxPowerLevel8812(
	IN	PADAPTER		Adapter,
	IN	u8				Channel
)
{

	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	u8			path = 0;

	/* RTW_INFO("==>PHY_SetTxPowerLevel8812()\n"); */

	for (path = RF_PATH_A; path < pHalData->NumTotalRFPath; ++path) {
		phy_set_tx_power_level_by_path(Adapter, Channel, path);
		PHY_TxPowerTrainingByPath_8812(Adapter, pHalData->current_channel_bw, Channel, path);
	}

	/* RTW_INFO("<==PHY_SetTxPowerLevel8812()\n"); */
}


/**************************************************************************************************************
 *   Description:
 *       The low-level interface to get the FINAL Tx Power Index , called  by both MP and Normal Driver.
 *
 *                                                                                    <20120830, Kordan>
 **************************************************************************************************************/
u8
PHY_GetTxPowerIndex_8812A(
	IN	PADAPTER			pAdapter,
	IN	enum rf_path			RFPath,
	IN	u8					Rate,
	IN	u8					BandWidth,
	IN	u8					Channel,
	struct txpwr_idx_comp *tic
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(pAdapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(pAdapter);
	s16 power_idx;
	u8 base_idx = 0;
	s8 by_rate_diff = 0, limit = 0, tpt_offset = 0, extra_bias = 0;
	u8 ntx_idx = phy_get_current_tx_num(pAdapter, Rate);
	BOOLEAN bIn24G = _FALSE;

	base_idx = PHY_GetTxPowerIndexBase(pAdapter, RFPath, Rate, ntx_idx, BandWidth, Channel, &bIn24G);

	by_rate_diff = PHY_GetTxPowerByRate(pAdapter, (u8)(!bIn24G), RFPath, Rate);
#ifdef CONFIG_USB_HCI
	/* no external power, so disable power by rate in VHT to avoid card disable */
#ifndef CONFIG_USE_EXTERNAL_POWER
	/*	2013/01/29 MH For preventing VHT rate of 8812AU to be used in USB 2.0 mode	*/
	/*	and the current will be more than 500mA and card disappear. We need to limit	*/
	/*	TX power with any power by rate for VHT in U2.								*/
	/*	2013/01/30 MH According to power current test compare with BCM AC NIC, we	*/
	/*	decide to use host hub = 2.0 mode to enable tx power limit behavior			*/
	/*	2013/01/29 MH For preventing VHT rate of 8812AU to be used in USB 2.0 mode	*/

	if (adapter_to_dvobj(pAdapter)->usb_speed == RTW_USB_SPEED_2
		&& IS_HARDWARE_TYPE_8812AU(pAdapter)) {

		/* VHT rate 0~7, disable TxPowerByRate, but enable TX power limit */
		if ((Rate >= MGN_VHT1SS_MCS0 && Rate <= MGN_VHT1SS_MCS7) ||
			(Rate >= MGN_VHT2SS_MCS0 && Rate <= MGN_VHT2SS_MCS7))
			by_rate_diff = 0;
	}
#endif
#endif

	limit = PHY_GetTxPowerLimit(pAdapter, NULL, (u8)(!bIn24G), pHalData->current_channel_bw, RFPath, Rate, ntx_idx, pHalData->current_channel);

	tpt_offset = PHY_GetTxPowerTrackingOffset(pAdapter, RFPath, Rate);

	if (tic) {
		tic->ntx_idx = ntx_idx;
		tic->base = base_idx;
		tic->by_rate = by_rate_diff;
		tic->limit = limit;
		tic->tpt = tpt_offset;
		tic->ebias = extra_bias;
	}

	by_rate_diff = by_rate_diff > limit ? limit : by_rate_diff;
	power_idx = base_idx + by_rate_diff + tpt_offset + extra_bias + transmit_power_boost;

	if (transmit_power_override != 0)
		power_idx = transmit_power_override;
	if (power_idx < 1)
		power_idx = 1;

	if (power_idx < 0)
		power_idx = 0;
	else if (power_idx > hal_spec->txgi_max)
		power_idx = hal_spec->txgi_max;

	if (power_idx % 2 == 1 && !IS_NORMAL_CHIP(pHalData->version_id))
		--power_idx;

	return power_idx;
}

/**************************************************************************************************************
 *   Description:
 *       The low-level interface to set TxAGC , called by both MP and Normal Driver.
 *
 *                                                                                    <20120830, Kordan>
 **************************************************************************************************************/

VOID
PHY_SetTxPowerIndex_8812A(
	IN	PADAPTER		Adapter,
	IN	u32				PowerIndex,
	IN	enum rf_path		RFPath,
	IN	u8				Rate
)
{
	HAL_DATA_TYPE		*pHalData	= GET_HAL_DATA(Adapter);

	/* <20120928, Kordan> A workaround in 8812A/8821A testchip, to fix the bug of odd Tx power indexes. */
	if ((PowerIndex % 2 == 1) && IS_HARDWARE_TYPE_JAGUAR(Adapter) && IS_TEST_CHIP(pHalData->version_id))
		PowerIndex -= 1;

	if (RFPath == RF_PATH_A) {
		switch (Rate) {
		case MGN_1M:
			phy_set_bb_reg(Adapter, rTxAGC_A_CCK11_CCK1_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_2M:
			phy_set_bb_reg(Adapter, rTxAGC_A_CCK11_CCK1_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_5_5M:
			phy_set_bb_reg(Adapter, rTxAGC_A_CCK11_CCK1_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_11M:
			phy_set_bb_reg(Adapter, rTxAGC_A_CCK11_CCK1_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_6M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm18_Ofdm6_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_9M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm18_Ofdm6_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_12M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm18_Ofdm6_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_18M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm18_Ofdm6_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_24M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm54_Ofdm24_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_36M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm54_Ofdm24_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_48M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm54_Ofdm24_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_54M:
			phy_set_bb_reg(Adapter, rTxAGC_A_Ofdm54_Ofdm24_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS0:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS3_MCS0_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS1:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS3_MCS0_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS2:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS3_MCS0_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS3:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS3_MCS0_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS4:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS7_MCS4_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS5:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS7_MCS4_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS6:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS7_MCS4_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS7:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS7_MCS4_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS8:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS11_MCS8_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS9:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS11_MCS8_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS10:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS11_MCS8_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS11:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS11_MCS8_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS12:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS15_MCS12_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS13:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS15_MCS12_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS14:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS15_MCS12_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS15:
			phy_set_bb_reg(Adapter, rTxAGC_A_MCS15_MCS12_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT1SS_MCS0:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index3_Nss1Index0_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT1SS_MCS1:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index3_Nss1Index0_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT1SS_MCS2:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index3_Nss1Index0_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT1SS_MCS3:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index3_Nss1Index0_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT1SS_MCS4:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index7_Nss1Index4_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT1SS_MCS5:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index7_Nss1Index4_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT1SS_MCS6:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index7_Nss1Index4_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT1SS_MCS7:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss1Index7_Nss1Index4_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT1SS_MCS8:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index1_Nss1Index8_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT1SS_MCS9:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index1_Nss1Index8_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT2SS_MCS0:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index1_Nss1Index8_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT2SS_MCS1:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index1_Nss1Index8_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT2SS_MCS2:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index5_Nss2Index2_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT2SS_MCS3:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index5_Nss2Index2_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT2SS_MCS4:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index5_Nss2Index2_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT2SS_MCS5:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index5_Nss2Index2_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT2SS_MCS6:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index9_Nss2Index6_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT2SS_MCS7:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index9_Nss2Index6_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT2SS_MCS8:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index9_Nss2Index6_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT2SS_MCS9:
			phy_set_bb_reg(Adapter, rTxAGC_A_Nss2Index9_Nss2Index6_JAguar, bMaskByte3, PowerIndex);
			break;

		default:
			RTW_INFO("Invalid Rate!!\n");
			break;
		}
	} else if (RFPath == RF_PATH_B) {
		switch (Rate) {
		case MGN_1M:
			phy_set_bb_reg(Adapter, rTxAGC_B_CCK11_CCK1_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_2M:
			phy_set_bb_reg(Adapter, rTxAGC_B_CCK11_CCK1_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_5_5M:
			phy_set_bb_reg(Adapter, rTxAGC_B_CCK11_CCK1_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_11M:
			phy_set_bb_reg(Adapter, rTxAGC_B_CCK11_CCK1_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_6M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm18_Ofdm6_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_9M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm18_Ofdm6_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_12M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm18_Ofdm6_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_18M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm18_Ofdm6_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_24M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm54_Ofdm24_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_36M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm54_Ofdm24_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_48M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm54_Ofdm24_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_54M:
			phy_set_bb_reg(Adapter, rTxAGC_B_Ofdm54_Ofdm24_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS0:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS3_MCS0_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS1:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS3_MCS0_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS2:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS3_MCS0_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS3:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS3_MCS0_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS4:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS7_MCS4_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS5:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS7_MCS4_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS6:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS7_MCS4_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS7:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS7_MCS4_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS8:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS11_MCS8_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS9:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS11_MCS8_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS10:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS11_MCS8_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS11:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS11_MCS8_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS12:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS15_MCS12_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS13:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS15_MCS12_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS14:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS15_MCS12_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS15:
			phy_set_bb_reg(Adapter, rTxAGC_B_MCS15_MCS12_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT1SS_MCS0:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index3_Nss1Index0_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT1SS_MCS1:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index3_Nss1Index0_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT1SS_MCS2:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index3_Nss1Index0_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT1SS_MCS3:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index3_Nss1Index0_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT1SS_MCS4:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index7_Nss1Index4_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT1SS_MCS5:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index7_Nss1Index4_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT1SS_MCS6:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index7_Nss1Index4_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT1SS_MCS7:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss1Index7_Nss1Index4_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT1SS_MCS8:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index1_Nss1Index8_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT1SS_MCS9:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index1_Nss1Index8_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT2SS_MCS0:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index1_Nss1Index8_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT2SS_MCS1:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index1_Nss1Index8_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT2SS_MCS2:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index5_Nss2Index2_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT2SS_MCS3:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index5_Nss2Index2_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT2SS_MCS4:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index5_Nss2Index2_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT2SS_MCS5:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index5_Nss2Index2_JAguar, bMaskByte3, PowerIndex);
			break;

		case MGN_VHT2SS_MCS6:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index9_Nss2Index6_JAguar, bMaskByte0, PowerIndex);
			break;
		case MGN_VHT2SS_MCS7:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index9_Nss2Index6_JAguar, bMaskByte1, PowerIndex);
			break;
		case MGN_VHT2SS_MCS8:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index9_Nss2Index6_JAguar, bMaskByte2, PowerIndex);
			break;
		case MGN_VHT2SS_MCS9:
			phy_set_bb_reg(Adapter, rTxAGC_B_Nss2Index9_Nss2Index6_JAguar, bMaskByte3, PowerIndex);
			break;

		default:
			RTW_INFO("Invalid Rate!!\n");
			break;
		}
	} else
		RTW_INFO("Invalid RFPath!!\n");
}

BOOLEAN
PHY_UpdateTxPowerDbm8812(
	IN	PADAPTER	Adapter,
	IN	int		powerInDbm
)
{
	return _TRUE;
}


u32 phy_get_tx_bb_swing_8812a(
	IN	PADAPTER	Adapter,
	IN	BAND_TYPE	Band,
	IN	enum rf_path	RFPath
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(GetDefaultAdapter(Adapter));
	struct dm_struct		*pDM_Odm = &pHalData->odmpriv;
	struct dm_rf_calibration_struct	*pRFCalibrateInfo = &(pDM_Odm->rf_calibrate_info);

	s8	bbSwing_2G = -1 * GetRegTxBBSwing_2G(Adapter);
	s8	bbSwing_5G = -1 * GetRegTxBBSwing_5G(Adapter);
	u32	out = 0x200;
	const s8	AUTO = -1;


	if (pHalData->bautoload_fail_flag) {
		if (Band == BAND_ON_2_4G) {
			pRFCalibrateInfo->bb_swing_diff_2g = bbSwing_2G;
			if (bbSwing_2G == 0)
				out = 0x200; /* 0 dB */
			else if (bbSwing_2G == -3)
				out = 0x16A; /* -3 dB */
			else if (bbSwing_2G == -6)
				out = 0x101; /* -6 dB */
			else if (bbSwing_2G == -9)
				out = 0x0B6; /* -9 dB */
			else {
				if (pHalData->ExternalPA_2G) {
					pRFCalibrateInfo->bb_swing_diff_2g = -3;
					out = 0x16A;
				} else  {
					pRFCalibrateInfo->bb_swing_diff_2g = 0;
					out = 0x200;
				}
			}
		} else if (Band == BAND_ON_5G) {
			pRFCalibrateInfo->bb_swing_diff_5g = bbSwing_5G;
			if (bbSwing_5G == 0)
				out = 0x200; /* 0 dB */
			else if (bbSwing_5G == -3)
				out = 0x16A; /* -3 dB */
			else if (bbSwing_5G == -6)
				out = 0x101; /* -6 dB */
			else if (bbSwing_5G == -9)
				out = 0x0B6; /* -9 dB */
			else {
				if (pHalData->external_pa_5g || IS_HARDWARE_TYPE_8821(Adapter)) {
					pRFCalibrateInfo->bb_swing_diff_5g = -3;
					out = 0x16A;
				} else {
					pRFCalibrateInfo->bb_swing_diff_5g = 0;
					out = 0x200;
				}
			}
		} else  {
			pRFCalibrateInfo->bb_swing_diff_2g = -3;
			pRFCalibrateInfo->bb_swing_diff_5g = -3;
			out = 0x16A; /* -3 dB */
		}
	} else {
		u32 swing = 0, onePathSwing = 0;

		if (Band == BAND_ON_2_4G) {
			if (GetRegTxBBSwing_2G(Adapter) == AUTO) {
				EFUSE_ShadowRead(Adapter, 1, EEPROM_TX_BBSWING_2G_8812, (u32 *)&swing);
				swing = (swing == 0xFF) ? 0x00 : swing;
			} else if (bbSwing_2G ==  0)
				swing = 0x00; /* 0 dB */
			else if (bbSwing_2G == -3)
				swing = 0x05; /* -3 dB */
			else if (bbSwing_2G == -6)
				swing = 0x0A; /* -6 dB */
			else if (bbSwing_2G == -9)
				swing = 0xFF; /* -9 dB */
			else
				swing = 0x00;
		} else {
			if (GetRegTxBBSwing_5G(Adapter) == AUTO) {
				EFUSE_ShadowRead(Adapter, 1, EEPROM_TX_BBSWING_5G_8812, (u32 *)&swing);
				swing = (swing == 0xFF) ? 0x00 : swing;
			} else if (bbSwing_5G ==  0)
				swing = 0x00; /* 0 dB */
			else if (bbSwing_5G == -3)
				swing = 0x05; /* -3 dB */
			else if (bbSwing_5G == -6)
				swing = 0x0A; /* -6 dB */
			else if (bbSwing_5G == -9)
				swing = 0xFF; /* -9 dB */
			else
				swing = 0x00;
		}

		if (RFPath == RF_PATH_A)
			onePathSwing = (swing & 0x3) >> 0; /* 0xC6/C7[1:0] */
		else if (RFPath == RF_PATH_B)
			onePathSwing = (swing & 0xC) >> 2; /* 0xC6/C7[3:2] */

		if (onePathSwing == 0x0) {
			if (Band == BAND_ON_2_4G)
				pRFCalibrateInfo->bb_swing_diff_2g = 0;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = 0;
			out = 0x200; /* 0 dB */
		} else if (onePathSwing == 0x1) {
			if (Band == BAND_ON_2_4G)
				pRFCalibrateInfo->bb_swing_diff_2g = -3;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = -3;
			out = 0x16A; /* -3 dB */
		} else if (onePathSwing == 0x2) {
			if (Band == BAND_ON_2_4G)
				pRFCalibrateInfo->bb_swing_diff_2g = -6;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = -6;
			out = 0x101; /* -6 dB */
		} else if (onePathSwing == 0x3) {
			if (Band == BAND_ON_2_4G)
				pRFCalibrateInfo->bb_swing_diff_2g = -9;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = -9;
			out = 0x0B6; /* -9 dB */
		}
	}

	/* RTW_INFO("<=== phy_get_tx_bb_swing_8812a, out = 0x%X\n", out); */

	return out;
}

VOID
phy_SetRFEReg8812(
	IN PADAPTER		Adapter,
	IN u8			Band
)
{
	u1Byte			u1tmp = 0;
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	if (Band == BAND_ON_2_4G) {
		switch (pHalData->rfe_type) {
		case 0:
		case 2:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			break;
		case 1:
#ifdef CONFIG_BT_COEXIST
			if (hal_btcoex_IsBtExist(Adapter) && (Adapter->registrypriv.mp_mode == 0)) {
				phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, 0xffffff, 0x777777);
				phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
				phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, 0x33f00000, 0x000);
				phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			} else
#endif
			{
				phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
				phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
				phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
				phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			}
			break;
		case 3:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x54337770);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x54337770);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			phy_set_bb_reg(Adapter, r_ANTSEL_SW_Jaguar, 0x00000303, 0x1);
			break;
		case 4:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x001);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x001);
			break;
		case 5:
			rtw_write8(Adapter, rA_RFE_Pinmux_Jaguar + 2, 0x77);

			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);
			u1tmp = rtw_read8(Adapter, rA_RFE_Inv_Jaguar + 3);
			rtw_write8(Adapter, rA_RFE_Inv_Jaguar + 3, (u1tmp &= ~BIT0));
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			break;
		case 6:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x07772770);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x07772770);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMaskDWord, 0x00000077);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMaskDWord, 0x00000077);
			break;
		default:
			break;
		}
	} else {
		switch (pHalData->rfe_type) {
		case 0:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337717);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337717);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			break;
		case 1:
#ifdef CONFIG_BT_COEXIST
			if (hal_btcoex_IsBtExist(Adapter) && (Adapter->registrypriv.mp_mode == 0)) {
				phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, 0xffffff, 0x337717);
				phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337717);
				phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, 0x33f00000, 0x000);
				phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			} else
#endif
			{
				phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337717);
				phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337717);
				phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
				phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x000);
			}
			break;
		case 2:
		case 4:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337777);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337777);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			break;
		case 3:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x54337717);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x54337717);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			phy_set_bb_reg(Adapter, r_ANTSEL_SW_Jaguar, 0x00000303, 0x1);
			break;
		case 5:
			rtw_write8(Adapter, rA_RFE_Pinmux_Jaguar + 2, 0x33);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77337777);
			u1tmp = rtw_read8(Adapter, rA_RFE_Inv_Jaguar + 3);
			rtw_write8(Adapter, rA_RFE_Inv_Jaguar + 3, (u1tmp |= BIT0));
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMask_RFEInv_Jaguar, 0x010);
			break;	
		case 6:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x07737717);
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x07737717);
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, bMaskDWord, 0x00000077);
			phy_set_bb_reg(Adapter, rB_RFE_Inv_Jaguar, bMaskDWord, 0x00000077);
			break;
		default:
			break;
		}
	}
}

void phy_SetBBSwingByBand_8812A(
	IN PADAPTER		Adapter,
	IN u8			Band,
	IN u1Byte		PreviousBand
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(GetDefaultAdapter(Adapter));

	/* <20120903, Kordan> Tx BB swing setting for RL6286, asked by Ynlin. */
	if (IS_NORMAL_CHIP(pHalData->version_id) || IS_HARDWARE_TYPE_8821(Adapter)) {
#if (MP_DRIVER == 1)
		PMPT_CONTEXT	pMptCtx = &(Adapter->mppriv.mpt_ctx);
#endif
		s8	BBDiffBetweenBand = 0;
		struct dm_struct		*pDM_Odm = &pHalData->odmpriv;
		struct dm_rf_calibration_struct	*pRFCalibrateInfo = &(pDM_Odm->rf_calibrate_info);
		u8	path = RF_PATH_A;

		phy_set_bb_reg(Adapter, rA_TxScale_Jaguar, 0xFFE00000,
			phy_get_tx_bb_swing_8812a(Adapter, (BAND_TYPE)Band, RF_PATH_A)); /* 0xC1C[31:21] */
		phy_set_bb_reg(Adapter, rB_TxScale_Jaguar, 0xFFE00000,
			phy_get_tx_bb_swing_8812a(Adapter, (BAND_TYPE)Band, RF_PATH_B)); /* 0xE1C[31:21] */

		/* <20121005, Kordan> When TxPowerTrack is ON, we should take care of the change of BB swing. */
		/* That is, reset all info to trigger Tx power tracking. */
		{
#if (MP_DRIVER == 1)
			path = pMptCtx->mpt_rf_path;
#endif

			if (Band != PreviousBand) {
				BBDiffBetweenBand = (pRFCalibrateInfo->bb_swing_diff_2g - pRFCalibrateInfo->bb_swing_diff_5g);
				BBDiffBetweenBand = (Band == BAND_ON_2_4G) ? BBDiffBetweenBand : (-1 * BBDiffBetweenBand);
				pRFCalibrateInfo->default_ofdm_index += BBDiffBetweenBand * 2;
			}

			odm_clear_txpowertracking_state(pDM_Odm);
		}
	}
}


VOID
phy_SetRFEReg8821(
	IN PADAPTER	Adapter,
	IN u1Byte		Band
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (Band == BAND_ON_2_4G) {
		/* Turn off RF PA and LNA */
		phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, 0xF000, 0x7);	/* 0xCB0[15:12] = 0x7 (LNA_On) */
		phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, 0xF0, 0x7);	/* 0xCB0[7:4] = 0x7 (PAPE_A)			 */

		if (pHalData->ExternalLNA_2G) {
			/* <20131223, VincentL> Turn on 2.4G External LNA (Asked by Luke Lee & Alex Wang) */
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, BIT20, 1);			/* 0xCB4 = 0x10100077; */
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, BIT22, 0);			/* 0xCB4 = 0x10100077; */
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, BIT2 | BIT1 | BIT0, 0x2);	/* 0xCB0[2:0] = b'010 */
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, BIT10 | BIT9 | BIT8, 0x2);	/* 0xCB0[10:8] = b'010 */

		} else {
			/* <20131223, VincentL> Bypass 2.4G External LNA (Asked by Luke Lee & Alex Wang) */
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, BIT20, 0);			/* 0xCB4 = 0x10000077; */
			phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, BIT22, 0);			/* 0xCB4 = 0x10000077; */
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, BIT2 | BIT1 | BIT0, 0x7);	/* 0xCB0[2:0] = b'111 */
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, BIT10 | BIT9 | BIT8, 0x7);	/* 0xCB0[10:8] = b'111 */
		}
	} else {
		/* Turn ON RF PA and LNA */
		phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, 0xF000, 0x5);	/* 0xCB0[15:12] = 0x5 (LNA_On) */
		phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, 0xF0, 0x4);	/* 0xCB0[7:4] = 0x4 (PAPE_A)			 */

		/* <20131223, VincentL> Bypass 2.4G External LNA (Asked by Luke Lee & Alex Wang) */
		phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, BIT20, 0);			/* 0xCB4 = 0x10000077; */
		phy_set_bb_reg(Adapter, rA_RFE_Inv_Jaguar, BIT22, 0);			/* 0xCB4 = 0x10000077; */
		phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, BIT2 | BIT1 | BIT0, 0x7);	/* 0xCB0[2:0] = b'111 */
		phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, BIT10 | BIT9 | BIT8, 0x7);	/* 0xCB0[10:8] = b'111 */

	}
}



s32
PHY_SwitchWirelessBand8812(
	IN PADAPTER	Adapter,
	IN u8			Band
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);
	u8				currentBand = pHalData->current_band_type;
	u8 current_bw = pHalData->current_channel_bw;
	u8 rf_type = pHalData->rf_type;
	u8 eLNA_2g = pHalData->ExternalLNA_2G;

	/* RTW_INFO("==>PHY_SwitchWirelessBand8812() %s\n", ((Band==0)?"2.4G":"5G")); */

	pHalData->current_band_type = (BAND_TYPE)Band;

	if (Band == BAND_ON_2_4G) {
		/* 2.4G band */

#ifdef CONFIG_RTL8821A
		/* 20160224 yiwei ,  8811au one antenna  module don't support antenna  div , so driver must to control antenna  band , otherwise one of the band will has issue */
		if (IS_HARDWARE_TYPE_8821(Adapter)) {
			if (Adapter->registrypriv.drv_ant_band_switch == 1 && pHalData->AntDivCfg == 0) {
				phydm_set_ext_band_switch_8821A(&(pHalData->odmpriv) , ODM_BAND_2_4G);
				RTW_DBG("Switch ant band to ODM_BAND_2_4G\n");
			}
		}
#endif /*#ifdef CONFIG_RTL8821A*/

		phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar | bCCKEN_Jaguar, 0x03);

		if (IS_HARDWARE_TYPE_8821(Adapter))
			phy_SetRFEReg8821(Adapter, Band);

		if (IS_HARDWARE_TYPE_8812(Adapter)) {
			/* <20131128, VincentL> Remove 0x830[3:1] setting when switching 2G/5G, requested by Yn. */
			phy_set_bb_reg(Adapter, rBWIndication_Jaguar, 0x3, 0x1);		/* 0x834[1:0] = 0x1 */
			/* set PD_TH_20M for BB Yn user guide R27 */
			phy_set_bb_reg(Adapter, rPwed_TH_Jaguar, BIT13 | BIT14 | BIT15 | BIT16 | BIT17, 0x17);		/* 0x830[17:13]=5'b10111 */
		}

		/* set PWED_TH for BB Yn user guide R29 */
		if (IS_HARDWARE_TYPE_8812(Adapter)) {
			if (current_bw == CHANNEL_WIDTH_20
			    && pHalData->rf_type == RF_1T1R
			    && eLNA_2g == 0) {
				/* 0x830[3:1]=3'b010 */
				phy_set_bb_reg(Adapter, rPwed_TH_Jaguar, BIT1 | BIT2 | BIT3, 0x02);
			} else
				/* 0x830[3:1]=3'b100 */
				phy_set_bb_reg(Adapter, rPwed_TH_Jaguar, BIT1 | BIT2 | BIT3, 0x04);
		}

		/* AGC table select */
		if (IS_VENDOR_8821A_MP_CHIP(Adapter))
			phy_set_bb_reg(Adapter, rA_TxScale_Jaguar, 0xF00, 0);		/* 0xC1C[11:8] = 0 */
		else
			phy_set_bb_reg(Adapter, rAGC_table_Jaguar, 0x3, 0);			/* 0x82C[1:0] = 2b'00 */

		if (IS_HARDWARE_TYPE_8812(Adapter))
			phy_SetRFEReg8812(Adapter, Band);

		/* <20131106, Kordan> Workaround to fix CCK FA for scan issue. */
		/* if( pHalData->bMPMode == FALSE) */
		if (Adapter->registrypriv.mp_mode == 0) {
			phy_set_bb_reg(Adapter, rTxPath_Jaguar, 0xf0, 0x1);
			phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0x0f000000, 0x1);
		}

		update_tx_basic_rate(Adapter, WIRELESS_11BG);

		/* CCK_CHECK_en */
		rtw_write8(Adapter, REG_CCK_CHECK_8812, rtw_read8(Adapter, REG_CCK_CHECK_8812) & (~BIT(7)));
	} else {	/* 5G band */
		u16 count = 0, reg41A = 0;

#ifdef CONFIG_RTL8821A
		/* 20160224 yiwei ,  8811a one antenna  module don't support antenna  div , so driver must to control antenna  band , otherwise one of the band will has issue */
		if (IS_HARDWARE_TYPE_8821(Adapter)) {
			if (Adapter->registrypriv.drv_ant_band_switch == 1 && pHalData->AntDivCfg == 0) {
				phydm_set_ext_band_switch_8821A(&(pHalData->odmpriv) , ODM_BAND_5G);
				RTW_DBG("Switch ant band to ODM_BAND_5G\n");
			}
		}
#endif /*#ifdef CONFIG_RTL8821A*/

		if (IS_HARDWARE_TYPE_8821(Adapter))
			phy_SetRFEReg8821(Adapter, Band);

		/* CCK_CHECK_en */
		rtw_write8(Adapter, REG_CCK_CHECK_8812, rtw_read8(Adapter, REG_CCK_CHECK_8812) | BIT(7));

		count = 0;
		reg41A = rtw_read16(Adapter, REG_TXPKT_EMPTY);
		/* RTW_INFO("Reg41A value %d", reg41A); */
		reg41A &= 0x30;
		while ((reg41A != 0x30) && (count < 50)) {
			rtw_udelay_os(50);
			/* RTW_INFO("Delay 50us\n"); */

			reg41A = rtw_read16(Adapter, REG_TXPKT_EMPTY);
			reg41A &= 0x30;
			count++;
			/* RTW_INFO("Reg41A value %d", reg41A); */
		}
		if (count != 0)
			RTW_INFO("PHY_SwitchWirelessBand8812(): Switch to 5G Band. Count = %d reg41A=0x%x\n", count, reg41A);

		/* 2012/02/01, Sinda add registry to switch workaround without long-run verification for scan issue. */
		if (Adapter->registrypriv.mp_mode == 0)
			phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar | bCCKEN_Jaguar, 0x03);

		if (IS_HARDWARE_TYPE_8812(Adapter)) {
			/* <20131128, VincentL> Remove 0x830[3:1] setting when switching 2G/5G, requested by Yn. */
			phy_set_bb_reg(Adapter, rBWIndication_Jaguar, 0x3, 0x2); /* 0x834[1:0] = 0x2 */
			/* set PD_TH_20M for BB Yn user guide R27 */
			phy_set_bb_reg(Adapter, rPwed_TH_Jaguar, BIT13 | BIT14 | BIT15 | BIT16 | BIT17, 0x15);		/* 0x830[17:13]=5'b10101 */
		}

		/* set PWED_TH for BB Yn user guide R29 */
		if (IS_HARDWARE_TYPE_8812(Adapter))
			/* 0x830[3:1]=3'b100 */
			phy_set_bb_reg(Adapter, rPwed_TH_Jaguar, BIT1 | BIT2 | BIT3, 0x04);

		/* AGC table select */
		if (IS_VENDOR_8821A_MP_CHIP(Adapter))
			phy_set_bb_reg(Adapter, rA_TxScale_Jaguar, 0xF00, 1);	/* 0xC1C[11:8] = 1 */
		else
			phy_set_bb_reg(Adapter, rAGC_table_Jaguar, 0x3, 1);		/* 0x82C[1:0] = 2'b00 */

		if (IS_HARDWARE_TYPE_8812(Adapter))
			phy_SetRFEReg8812(Adapter, Band);

		/* <20131106, Kordan> Workaround to fix CCK FA for scan issue. */
		/* if( pHalData->bMPMode == FALSE) */
		if (Adapter->registrypriv.mp_mode == 0) {
			phy_set_bb_reg(Adapter, rTxPath_Jaguar, 0xf0, 0x0);
			phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0x0f000000, 0xF);
		} else {
			/* cck_enable */
			phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar | bCCKEN_Jaguar, 0x02);
		}

		/* avoid using cck rate in 5G band */
		/* Set RRSR rate table. */
		update_tx_basic_rate(Adapter, WIRELESS_11A);


		/* RTW_INFO("==>PHY_SwitchWirelessBand8812() BAND_ON_5G settings OFDM index 0x%x\n", pHalData->OFDM_index[RF_PATH_A]); */
	}

	phy_SetBBSwingByBand_8812A(Adapter, Band, currentBand);
	/* RTW_INFO("<==PHY_SwitchWirelessBand8812():Switch Band OK.\n"); */
	return _SUCCESS;
}

BOOLEAN
phy_SwBand8812(
	IN	PADAPTER	pAdapter,
	IN	u8			channelToSW
)
{
	u8			u1Btmp;
	BOOLEAN		ret_value = _TRUE;
	u8			Band = BAND_ON_5G, BandToSW;

	u1Btmp = rtw_read8(pAdapter, REG_CCK_CHECK_8812);
	if (u1Btmp & BIT7)
		Band = BAND_ON_5G;
	else
		Band = BAND_ON_2_4G;

	/* Use current channel to judge Band Type and switch Band if need. */
	if (channelToSW > 14)
		BandToSW = BAND_ON_5G;
	else
		BandToSW = BAND_ON_2_4G;

	if (BandToSW != Band)
		PHY_SwitchWirelessBand8812(pAdapter, BandToSW);

	return ret_value;
}

#pragma clang optimize off
u8
phy_GetSecondaryChnl_8812(
	IN	PADAPTER	Adapter
)
{
	u8					SCSettingOf40 = 0, SCSettingOf20 = 0;
	PHAL_DATA_TYPE		pHalData = GET_HAL_DATA(Adapter);

	/* RTW_INFO("SCMapping: Case: pHalData->current_channel_bw %d, pHalData->nCur80MhzPrimeSC %d, pHalData->nCur40MhzPrimeSC %d\n",pHalData->current_channel_bw,pHalData->nCur80MhzPrimeSC,pHalData->nCur40MhzPrimeSC); */
	if (pHalData->current_channel_bw == CHANNEL_WIDTH_80) {
		if (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
			SCSettingOf40 = VHT_DATA_SC_40_LOWER_OF_80MHZ;
		else if (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
			SCSettingOf40 = VHT_DATA_SC_40_UPPER_OF_80MHZ;
		else
			RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");

		if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER))
			SCSettingOf20 = VHT_DATA_SC_20_LOWEST_OF_80MHZ;
		else if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER))
			SCSettingOf20 = VHT_DATA_SC_20_LOWER_OF_80MHZ;
		else if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER))
			SCSettingOf20 = VHT_DATA_SC_20_UPPER_OF_80MHZ;
		else if ((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER))
			SCSettingOf20 = VHT_DATA_SC_20_UPPERST_OF_80MHZ;
		else
			RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
	} else if (pHalData->current_channel_bw == CHANNEL_WIDTH_40) {
		/* RTW_INFO("SCMapping: Case: pHalData->current_channel_bw %d, pHalData->nCur40MhzPrimeSC %d\n",pHalData->current_channel_bw,pHalData->nCur40MhzPrimeSC); */

		if (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
			SCSettingOf20 = VHT_DATA_SC_20_UPPER_OF_80MHZ;
		else if (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
			SCSettingOf20 = VHT_DATA_SC_20_LOWER_OF_80MHZ;
		else
			RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
	}

	/*RTW_INFO("SCMapping: SC Value %x\n", ((SCSettingOf40 << 4) | SCSettingOf20));*/
	return (SCSettingOf40 << 4) | SCSettingOf20;
}
#pragma clang optimize on

VOID
phy_SetRegBW_8812(
	IN	PADAPTER		Adapter,
	enum channel_width	CurrentBW
)
{
	u16	RegRfMod_BW, u2tmp = 0;
	RegRfMod_BW = rtw_read16(Adapter, REG_WMAC_TRXPTCL_CTL);

	switch (CurrentBW) {
	case CHANNEL_WIDTH_20:
		rtw_write16(Adapter, REG_WMAC_TRXPTCL_CTL, (RegRfMod_BW & 0xFE7F)); /* BIT 7 = 0, BIT 8 = 0 */
		break;

	case CHANNEL_WIDTH_40:
		u2tmp = RegRfMod_BW | BIT7;
		rtw_write16(Adapter, REG_WMAC_TRXPTCL_CTL, (u2tmp & 0xFEFF)); /* BIT 7 = 1, BIT 8 = 0 */
		break;

	case CHANNEL_WIDTH_80:
		u2tmp = RegRfMod_BW | BIT8;
		rtw_write16(Adapter, REG_WMAC_TRXPTCL_CTL, (u2tmp & 0xFF7F)); /* BIT 7 = 0, BIT 8 = 1 */
		break;

	default:
		RTW_INFO("phy_PostSetBWMode8812():	unknown Bandwidth: %#X\n", CurrentBW);
		break;
	}

}

void
phy_FixSpur_8812A(
	IN	PADAPTER	        pAdapter,
	IN  enum channel_width    Bandwidth,
	IN  u1Byte			    Channel
)
{
	/* C cut Item12 ADC FIFO CLOCK */
	if (IS_VENDOR_8812A_C_CUT(pAdapter)) {
		if (Bandwidth == CHANNEL_WIDTH_40 && Channel == 11)
			phy_set_bb_reg(pAdapter, rRFMOD_Jaguar, 0xC00, 0x3)	;		/* 0x8AC[11:10] = 2'b11 */
		else
			phy_set_bb_reg(pAdapter, rRFMOD_Jaguar, 0xC00, 0x2);		/* 0x8AC[11:10] = 2'b10 */

		/* <20120914, Kordan> A workarould to resolve 2480Mhz spur by setting ADC clock as 160M. (Asked by Binson) */
		if (Bandwidth == CHANNEL_WIDTH_20 &&
		    (Channel == 13 || Channel == 14)) {

			phy_set_bb_reg(pAdapter, rRFMOD_Jaguar, 0x300, 0x3);		/* 0x8AC[9:8] = 2'b11 */
			phy_set_bb_reg(pAdapter, rADC_Buf_Clk_Jaguar, BIT30, 1);	/* 0x8C4[30] = 1 */

		} else if (Bandwidth == CHANNEL_WIDTH_40 &&
			   Channel == 11) {

			phy_set_bb_reg(pAdapter, rADC_Buf_Clk_Jaguar, BIT30, 1);	/* 0x8C4[30] = 1 */

		} else if (Bandwidth != CHANNEL_WIDTH_80) {

			phy_set_bb_reg(pAdapter, rRFMOD_Jaguar, 0x300, 0x2);		/* 0x8AC[9:8] = 2'b10	 */
			phy_set_bb_reg(pAdapter, rADC_Buf_Clk_Jaguar, BIT30, 0);	/* 0x8C4[30] = 0 */

		}
	} else if (IS_HARDWARE_TYPE_8812(pAdapter)) {
		/* <20120914, Kordan> A workarould to resolve 2480Mhz spur by setting ADC clock as 160M. (Asked by Binson) */
		if (Bandwidth == CHANNEL_WIDTH_20 &&
		    (Channel == 13 || Channel == 14))
			phy_set_bb_reg(pAdapter, rRFMOD_Jaguar, 0x300, 0x3);  /* 0x8AC[9:8] = 11 */
		else if (Channel <= 14) /* 2.4G only */
			phy_set_bb_reg(pAdapter, rRFMOD_Jaguar, 0x300, 0x2);  /* 0x8AC[9:8] = 10 */
	}

}

VOID
phy_PostSetBwMode8812(
	IN	PADAPTER	Adapter
)
{
	u8			SubChnlNum = 0;
	u8			L1pkVal = 0, reg_837 = 0;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);


	/* 3 Set Reg668 BW */
	phy_SetRegBW_8812(Adapter, pHalData->current_channel_bw);

	/* 3 Set Reg483 */
	SubChnlNum = phy_GetSecondaryChnl_8812(Adapter);
	rtw_write8(Adapter, REG_DATA_SC_8812, SubChnlNum);

	if (pHalData->rf_chip == RF_PSEUDO_11N) {
		RTW_INFO("phy_PostSetBwMode8812: return for PSEUDO\n");
		return;
	}

	reg_837 = rtw_read8(Adapter, rBWIndication_Jaguar + 3);
	/* 3 Set Reg848 Reg864 Reg8AC Reg8C4 RegA00 */
	switch (pHalData->current_channel_bw) {
	case CHANNEL_WIDTH_20:
		phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x003003C3, 0x00300200); /* 0x8ac[21,20,9:6,1,0]=8'b11100000 */
		phy_set_bb_reg(Adapter, rADC_Buf_Clk_Jaguar, BIT30, 0);			/* 0x8c4[30] = 1'b0 */

		if (pHalData->rf_type == RF_2T2R)
			phy_set_bb_reg(Adapter, rL1PeakTH_Jaguar, 0x03C00000, 7);	/* 2R 0x848[25:22] = 0x7 */
		else
			phy_set_bb_reg(Adapter, rL1PeakTH_Jaguar, 0x03C00000, 8);	/* 1R 0x848[25:22] = 0x8 */

		break;

	case CHANNEL_WIDTH_40:
		phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x003003C3, 0x00300201); /* 0x8ac[21,20,9:6,1,0]=8'b11100000		 */
		phy_set_bb_reg(Adapter, rADC_Buf_Clk_Jaguar, BIT30, 0);			/* 0x8c4[30] = 1'b0 */
		phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x3C, SubChnlNum);
		phy_set_bb_reg(Adapter, rCCAonSec_Jaguar, 0xf0000000, SubChnlNum);

		if (reg_837 & BIT2)
			L1pkVal = 6;
		else {
			if (pHalData->rf_type == RF_2T2R)
				L1pkVal = 7;
			else
				L1pkVal = 8;
		}

		phy_set_bb_reg(Adapter, rL1PeakTH_Jaguar, 0x03C00000, L1pkVal);	/* 0x848[25:22] = 0x6 */

		if (SubChnlNum == VHT_DATA_SC_20_UPPER_OF_80MHZ)
			phy_set_bb_reg(Adapter, rCCK_System_Jaguar, bCCK_System_Jaguar, 1);
		else
			phy_set_bb_reg(Adapter, rCCK_System_Jaguar, bCCK_System_Jaguar, 0);
		break;

	case CHANNEL_WIDTH_80:
		phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x003003C3, 0x00300202); /* 0x8ac[21,20,9:6,1,0]=8'b11100010 */
		phy_set_bb_reg(Adapter, rADC_Buf_Clk_Jaguar, BIT30, 1);			/* 0x8c4[30] = 1 */
		phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x3C, SubChnlNum);
		phy_set_bb_reg(Adapter, rCCAonSec_Jaguar, 0xf0000000, SubChnlNum);

		if (reg_837 & BIT2)
			L1pkVal = 5;
		else {
			if (pHalData->rf_type == RF_2T2R)
				L1pkVal = 6;
			else
				L1pkVal = 7;
		}
		phy_set_bb_reg(Adapter, rL1PeakTH_Jaguar, 0x03C00000, L1pkVal);	/* 0x848[25:22] = 0x5 */

		break;

	default:
		RTW_INFO("phy_PostSetBWMode8812():	unknown Bandwidth: %#X\n", pHalData->current_channel_bw);
		break;
	}

	/* <20121109, Kordan> A workaround for 8812A only. */
	phy_FixSpur_8812A(Adapter, pHalData->current_channel_bw, pHalData->current_channel);

	/* RTW_INFO("phy_PostSetBwMode8812(): Reg483: %x\n", rtw_read8(Adapter, 0x483)); */
	/* RTW_INFO("phy_PostSetBwMode8812(): Reg668: %x\n", rtw_read32(Adapter, 0x668)); */
	/* RTW_INFO("phy_PostSetBwMode8812(): Reg8AC: %x\n", phy_query_bb_reg(Adapter, rRFMOD_Jaguar, 0xffffffff)); */

	/* 3 Set RF related register */
	PHY_RF6052SetBandwidth8812(Adapter, pHalData->current_channel_bw);
}

/* <20130207, Kordan> The variales initialized here are used in odm_LNAPowerControl(). */
VOID phy_InitRssiTRSW(
	IN	PADAPTER					pAdapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	struct dm_struct		*pDM_Odm = &pHalData->odmpriv;
	u8			channel = pHalData->current_channel;

	if (pHalData->rfe_type == 3) {

		if (channel <= 14) {
			pDM_Odm->rssi_trsw_h    = 70; /* Unit: percentage(%) */
			pDM_Odm->rssi_trsw_iso  = 25;
		} else {
			pDM_Odm->rssi_trsw_h   = 80;
			pDM_Odm->rssi_trsw_iso = 25;
		}

		pDM_Odm->rssi_trsw_l = pDM_Odm->rssi_trsw_h - pDM_Odm->rssi_trsw_iso - 10;
	}
}

/*Referenced from "WB-20130801-YN-RL6286 Settings for Spur Issues R02.xls"*/
VOID
phy_SpurCalibration_8812A(
	IN	PADAPTER	pAdapter,
	IN	u8			Channel,
	IN	u8			Bandwidth
)
{

	/* 2 1. Reset */
	phy_set_bb_reg(pAdapter, 0x874, BIT0, 0);
	phy_set_bb_reg(pAdapter, 0x874, BIT21 , 0);
	phy_set_bb_reg(pAdapter, 0x878, BIT8 | BIT7 | BIT6, 0);
	phy_set_bb_reg(pAdapter, 0x878, BIT0, 0);
	phy_set_bb_reg(pAdapter, 0x87C, BIT13, 0);
	phy_set_bb_reg(pAdapter, 0x880, bMaskDWord, 0);
	phy_set_bb_reg(pAdapter, 0x884, bMaskDWord, 0);
	phy_set_bb_reg(pAdapter, 0x898, bMaskDWord, 0);
	phy_set_bb_reg(pAdapter, 0x89C, bMaskDWord, 0);


	/* 2 2. Register Setting 1 (False Alarm) */
	if (((Channel == 149 || Channel == 153) && Bandwidth == CHANNEL_WIDTH_20) ||
	    (Channel == 151 && Bandwidth == CHANNEL_WIDTH_40) ||
	    (Channel == 155 && Bandwidth == CHANNEL_WIDTH_80)) {

		phy_set_bb_reg(pAdapter, 0x878, BIT6, 0);
		phy_set_bb_reg(pAdapter, 0x878, BIT7, 0);
		phy_set_bb_reg(pAdapter, 0x878, BIT8, 1);
		phy_set_bb_reg(pAdapter, 0x878, BIT0, 1);
		phy_set_bb_reg(pAdapter, 0x87C, BIT13, 1);
	}

	/* 2 3. Register Setting 2 (SINR) */
	phy_set_bb_reg(pAdapter, 0x874, BIT21, 1);
	phy_set_bb_reg(pAdapter, 0x874, BIT0 , 1);

	if (Channel == 149 && Bandwidth == CHANNEL_WIDTH_20)
		phy_set_bb_reg(pAdapter, 0x884, bMaskDWord, 0x00010000);
	else if (Channel == 153 && Bandwidth == CHANNEL_WIDTH_20)
		phy_set_bb_reg(pAdapter, 0x89C, bMaskDWord, 0x00010000);
	else if (Channel == 151 && Bandwidth == CHANNEL_WIDTH_40)
		phy_set_bb_reg(pAdapter, 0x880, bMaskDWord, 0x00010000);
	else if (Channel == 155 && Bandwidth == CHANNEL_WIDTH_80)
		phy_set_bb_reg(pAdapter, 0x898, bMaskDWord, 0x00010000);

}

VOID
phy_SwChnl8812(
	IN	PADAPTER	pAdapter
)
{
	enum rf_path	eRFPath = RF_PATH_A;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	u8	channelToSW = pHalData->current_channel;
	u8	bandwidthToSw = pHalData->current_channel_bw;

	if (phy_SwBand8812(pAdapter, channelToSW) == _FALSE)
		RTW_INFO("error Chnl %d !\n", channelToSW);

	/* <20130313, Kordan> Sample code to demonstrate how to configure AGC_TAB_DIFF.(Disabled by now) */
#if  (MP_DRIVER == 1)
	if (pAdapter->registrypriv.mp_mode == 1)
		odm_config_bb_with_header_file(&pHalData->odmpriv, CONFIG_BB_AGC_TAB_DIFF);
#endif

	if (pHalData->rf_chip == RF_PSEUDO_11N) {
		RTW_INFO("phy_SwChnl8812: return for PSEUDO\n");
		return;
	}

	/* RTW_INFO("[BW:CHNL], phy_SwChnl8812(), switch to channel %d !!\n", channelToSW); */

	/* fc_area		 */
	if (36 <= channelToSW && channelToSW <= 48)
		phy_set_bb_reg(pAdapter, rFc_area_Jaguar, 0x1ffe0000, 0x494);
	else if (50 <= channelToSW && channelToSW <= 64)
		phy_set_bb_reg(pAdapter, rFc_area_Jaguar, 0x1ffe0000, 0x453);
	else if (100 <= channelToSW && channelToSW <= 116)
		phy_set_bb_reg(pAdapter, rFc_area_Jaguar, 0x1ffe0000, 0x452);
	else if (118 <= channelToSW)
		phy_set_bb_reg(pAdapter, rFc_area_Jaguar, 0x1ffe0000, 0x412);
	else
		phy_set_bb_reg(pAdapter, rFc_area_Jaguar, 0x1ffe0000, 0x96a);

	for (eRFPath = 0; eRFPath < pHalData->NumTotalRFPath; eRFPath++) {
		/* RF_MOD_AG */
		if (36 <= channelToSW && channelToSW <= 64)
			phy_set_rf_reg(pAdapter, eRFPath, RF_CHNLBW_Jaguar, BIT18 | BIT17 | BIT16 | BIT9 | BIT8, 0x101); /* 5'b00101); */
		else if (100 <= channelToSW && channelToSW <= 140)
			phy_set_rf_reg(pAdapter, eRFPath, RF_CHNLBW_Jaguar, BIT18 | BIT17 | BIT16 | BIT9 | BIT8, 0x301); /* 5'b01101); */
		else if (140 < channelToSW)
			phy_set_rf_reg(pAdapter, eRFPath, RF_CHNLBW_Jaguar, BIT18 | BIT17 | BIT16 | BIT9 | BIT8, 0x501); /* 5'b10101); */
		else
			phy_set_rf_reg(pAdapter, eRFPath, RF_CHNLBW_Jaguar, BIT18 | BIT17 | BIT16 | BIT9 | BIT8, 0x000); /* 5'b00000); */

		/* <20121109, Kordan> A workaround for 8812A only. */
		phy_FixSpur_8812A(pAdapter, pHalData->current_channel_bw, channelToSW);

		phy_set_rf_reg(pAdapter, eRFPath, RF_CHNLBW_Jaguar, bMaskByte0, channelToSW);

		/* <20130104, Kordan> APK for MP chip is done on initialization from folder. */
		if (IS_HARDWARE_TYPE_8821U(pAdapter) && (!IS_NORMAL_CHIP(pHalData->version_id)) && channelToSW > 14) {
			/* <20121116, Kordan> For better result of APK. Asked by AlexWang. */
			if (36 <= channelToSW && channelToSW <= 64)
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x710E7);
			else if (100 <= channelToSW && channelToSW <= 140)
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x716E9);
			else
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x714E9);
		} else if (IS_HARDWARE_TYPE_8821S(pAdapter) && channelToSW > 14) {
			/* <20130111, Kordan> For better result of APK. Asked by Willson. */
			if (36 <= channelToSW && channelToSW <= 64)
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x714E9);
			else if (100 <= channelToSW && channelToSW <= 140)
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x110E9);
			else
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x714E9);
		} else if (IS_HARDWARE_TYPE_8821E(pAdapter) && channelToSW > 14) {
			/* <20130613, Kordan> For better result of APK. Asked by Willson. */
			if (36 <= channelToSW && channelToSW <= 64)
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x114E9);
			else if (100 <= channelToSW && channelToSW <= 140)
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x110E9);
			else
				phy_set_rf_reg(pAdapter, eRFPath, RF_APK_Jaguar, bRFRegOffsetMask, 0x110E9);
		}
	}

	/*only for 8812A mp mode*/
	if (IS_HARDWARE_TYPE_8812(pAdapter) && (pHalData->LNAType_5G == 0x00)
	    && pAdapter->registrypriv.mp_mode == _TRUE)
		phy_SpurCalibration_8812A(pAdapter, channelToSW, bandwidthToSw);
}

VOID
phy_SwChnlAndSetBwMode8812(
	IN  PADAPTER		Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct			*pDM_Odm = &pHalData->odmpriv;
	/* RTW_INFO("phy_SwChnlAndSetBwMode8812(): bSwChnl %d, bSetChnlBW %d\n", pHalData->bSwChnl, pHalData->bSetChnlBW); */
	if (Adapter->bNotifyChannelChange) {
		RTW_INFO("[%s] bSwChnl=%d, ch=%d, bSetChnlBW=%d, bw=%d\n",
			 __FUNCTION__,
			 pHalData->bSwChnl,
			 pHalData->current_channel,
			 pHalData->bSetChnlBW,
			 pHalData->current_channel_bw);
	}

	if (RTW_CANNOT_RUN(Adapter))
		return;


	if (pHalData->bSwChnl) {
		phy_SwChnl8812(Adapter);
		pHalData->bSwChnl = _FALSE;
	}

	if (pHalData->bSetChnlBW) {
		phy_PostSetBwMode8812(Adapter);
		pHalData->bSetChnlBW = _FALSE;
	}

	odm_clear_txpowertracking_state(&pHalData->odmpriv);
	PHY_SetTxPowerLevel8812(Adapter, pHalData->current_channel);

	if (IS_HARDWARE_TYPE_8812(Adapter))
		phy_InitRssiTRSW(Adapter);

	if ((pHalData->bNeedIQK == _TRUE)
#if (MP_DRIVER == 1)
	    || (Adapter->registrypriv.mp_mode == 1)
#endif
	   ) {
		if (IS_HARDWARE_TYPE_8812(Adapter)) {
#if (RTL8812A_SUPPORT == 1)
			/*phy_iq_calibrate_8812a(Adapter, _FALSE);*/
			halrf_iqk_trigger(&pHalData->odmpriv, _FALSE);
#endif
		} else if (IS_HARDWARE_TYPE_8821(Adapter)) {
#if (RTL8821A_SUPPORT == 1)
			/*phy_iq_calibrate_8821a(pDM_Odm, _FALSE);*/
			halrf_iqk_trigger(&pHalData->odmpriv, _FALSE);
#endif
		}
		pHalData->bNeedIQK = _FALSE;
	}
}

VOID
PHY_HandleSwChnlAndSetBW8812(
	IN	PADAPTER			Adapter,
	IN	BOOLEAN				bSwitchChannel,
	IN	BOOLEAN				bSetBandWidth,
	IN	u8					ChannelNum,
	IN	enum channel_width		ChnlWidth,
	IN	u8					ChnlOffsetOf40MHz,
	IN	u8					ChnlOffsetOf80MHz,
	IN	u8					CenterFrequencyIndex1
)
{
	PADAPTER			pDefAdapter =  GetDefaultAdapter(Adapter);
	PHAL_DATA_TYPE		pHalData = GET_HAL_DATA(pDefAdapter);
	u8					tmpChannel = pHalData->current_channel;
	enum channel_width	tmpBW = pHalData->current_channel_bw;
	u8					tmpnCur40MhzPrimeSC = pHalData->nCur40MhzPrimeSC;
	u8					tmpnCur80MhzPrimeSC = pHalData->nCur80MhzPrimeSC;
	u8					tmpCenterFrequencyIndex1 = pHalData->CurrentCenterFrequencyIndex1;
	struct mlme_ext_priv	*pmlmeext = &Adapter->mlmeextpriv;

	/* RTW_INFO("=> PHY_HandleSwChnlAndSetBW8812: bSwitchChannel %d, bSetBandWidth %d\n",bSwitchChannel,bSetBandWidth); */

	/* check is swchnl or setbw */
	if (!bSwitchChannel && !bSetBandWidth) {
		RTW_INFO("PHY_HandleSwChnlAndSetBW8812:  not switch channel and not set bandwidth\n");
		return;
	}

	/* skip change for channel or bandwidth is the same */
	if (bSwitchChannel) {
		if (pHalData->current_channel != ChannelNum) {
			if (HAL_IsLegalChannel(Adapter, ChannelNum))
				pHalData->bSwChnl = _TRUE;
			else
				return;
		}
	}

	if (bSetBandWidth) {
		if (pHalData->bChnlBWInitialized == _FALSE) {
			pHalData->bChnlBWInitialized = _TRUE;
			pHalData->bSetChnlBW = _TRUE;
		} else if ((pHalData->current_channel_bw != ChnlWidth) ||
			   (pHalData->nCur40MhzPrimeSC != ChnlOffsetOf40MHz) ||
			   (pHalData->nCur80MhzPrimeSC != ChnlOffsetOf80MHz) ||
			(pHalData->CurrentCenterFrequencyIndex1 != CenterFrequencyIndex1))
			pHalData->bSetChnlBW = _TRUE;
	}

	if (!pHalData->bSetChnlBW && !pHalData->bSwChnl && pHalData->bNeedIQK != _TRUE) {
		/* RTW_INFO("<= PHY_HandleSwChnlAndSetBW8812: bSwChnl %d, bSetChnlBW %d\n",pHalData->bSwChnl,pHalData->bSetChnlBW); */
		return;
	}


	if (pHalData->bSwChnl) {
		pHalData->current_channel = ChannelNum;
		pHalData->CurrentCenterFrequencyIndex1 = ChannelNum;
	}


	if (pHalData->bSetChnlBW) {
		pHalData->current_channel_bw = ChnlWidth;
#if 0
		if (ExtChnlOffsetOf40MHz == EXTCHNL_OFFSET_LOWER)
			pHalData->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_UPPER;
		else if (ExtChnlOffsetOf40MHz == EXTCHNL_OFFSET_UPPER)
			pHalData->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_LOWER;
		else
			pHalData->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

		if (ExtChnlOffsetOf80MHz == EXTCHNL_OFFSET_LOWER)
			pHalData->nCur80MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_UPPER;
		else if (ExtChnlOffsetOf80MHz == EXTCHNL_OFFSET_UPPER)
			pHalData->nCur80MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_LOWER;
		else
			pHalData->nCur80MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
#else
		pHalData->nCur40MhzPrimeSC = ChnlOffsetOf40MHz;
		pHalData->nCur80MhzPrimeSC = ChnlOffsetOf80MHz;
#endif

		pHalData->CurrentCenterFrequencyIndex1 = CenterFrequencyIndex1;
	}

	/* Switch workitem or set timer to do switch channel or setbandwidth operation */
	if (!RTW_CANNOT_RUN(Adapter))
		phy_SwChnlAndSetBwMode8812(Adapter);
	else {
		if (pHalData->bSwChnl) {
			pHalData->current_channel = tmpChannel;
			pHalData->CurrentCenterFrequencyIndex1 = tmpChannel;
		}
		if (pHalData->bSetChnlBW) {
			pHalData->current_channel_bw = tmpBW;
			pHalData->nCur40MhzPrimeSC = tmpnCur40MhzPrimeSC;
			pHalData->nCur80MhzPrimeSC = tmpnCur80MhzPrimeSC;
			pHalData->CurrentCenterFrequencyIndex1 = tmpCenterFrequencyIndex1;
		}
	}

	/* RTW_INFO("Channel %d ChannelBW %d ",pHalData->current_channel, pHalData->current_channel_bw); */
	/* RTW_INFO("40MhzPrimeSC %d 80MhzPrimeSC %d ",pHalData->nCur40MhzPrimeSC, pHalData->nCur80MhzPrimeSC); */
	/* RTW_INFO("CenterFrequencyIndex1 %d\n",pHalData->CurrentCenterFrequencyIndex1); */

	/* RTW_INFO("<= PHY_HandleSwChnlAndSetBW8812: bSwChnl %d, bSetChnlBW %d\n",pHalData->bSwChnl,pHalData->bSetChnlBW); */

}

VOID
PHY_SetSwChnlBWMode8812(
	IN	PADAPTER			Adapter,
	IN	u8					channel,
	IN	enum channel_width	Bandwidth,
	IN	u8					Offset40,
	IN	u8					Offset80
)
{
	/* RTW_INFO("%s()===>\n",__FUNCTION__); */

	PHY_HandleSwChnlAndSetBW8812(Adapter, _TRUE, _TRUE, channel, Bandwidth, Offset40, Offset80, channel);

	/* RTW_INFO("<==%s()\n",__FUNCTION__); */
}
