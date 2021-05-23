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
#define _RTL8814A_PHYCFG_C_

//#include <drv_types.h>

#include <rtl8814a_hal.h>
#include "hal_com_h2c.h"

/*---------------------Define local function prototype-----------------------*/

/*----------------------------Function Body----------------------------------*/
//1   1. BB register R/W API

u32
PHY_QueryBBReg8814A(
	IN	PADAPTER	Adapter,
	IN	u32		RegAddr,
	IN	u32		BitMask
	)
{
	u32	ReturnValue = 0, OriginalValue, BitShift;

#if (DISABLE_BB_RF == 1)
	return 0;
#endif

#if(SIC_ENABLE == 1)
	return SIC_QueryBBReg(Adapter, RegAddr, BitMask);
#endif

	OriginalValue = rtw_read32(Adapter, RegAddr);
	BitShift = PHY_CalculateBitShift(BitMask);
	ReturnValue = (OriginalValue & BitMask) >> BitShift;

	//RTW_INFO("BBR MASK=0x%x Addr[0x%x]=0x%x\n", BitMask, RegAddr, OriginalValue);

	return (ReturnValue);
}


VOID
PHY_SetBBReg8814A(
	IN	PADAPTER	Adapter,
	IN	u32		RegAddr,
	IN	u32		BitMask,
	IN	u32		Data
	)
{
	u32			OriginalValue, BitShift;

#if (DISABLE_BB_RF == 1)
	return;
#endif

#if(SIC_ENABLE == 1)
	SIC_SetBBReg(Adapter, RegAddr, BitMask, Data);
	return; 
#endif

	if(BitMask!= bMaskDWord)
	{//if not "double word" write
		OriginalValue = rtw_read32(Adapter, RegAddr);
		BitShift = PHY_CalculateBitShift(BitMask);
		Data = ((OriginalValue) & (~BitMask)) |( ((Data << BitShift)) & BitMask);
	}

	rtw_write32(Adapter, RegAddr, Data);
	
	//RTW_INFO("BBW MASK=0x%x Addr[0x%x]=0x%x\n", BitMask, RegAddr, Data);
}



static	u32
phy_RFRead_8814A(
	IN	PADAPTER			Adapter,
	IN	u8				eRFPath,
	IN	u32				RegAddr,
	IN	u32				BitMask
	)
{
	u32	DataAndAddr = 0;
	u32	Readback_Value, Direct_Addr;	
	
	RegAddr &= 0xff;
	switch(eRFPath){
		case RF_PATH_A:
			Direct_Addr = 0x2800+RegAddr*4;
		break;
		case RF_PATH_B:
			Direct_Addr = 0x2c00+RegAddr*4;
		break;
		case RF_PATH_C:
			Direct_Addr = 0x3800+RegAddr*4;
		break;
		case RF_PATH_D:
			Direct_Addr = 0x3c00+RegAddr*4;
		break;
		default: //pathA
			Direct_Addr = 0x2800+RegAddr*4;
		break;
	}
	

	BitMask &= bRFRegOffsetMask;
	
	Readback_Value = phy_query_bb_reg(Adapter, Direct_Addr, BitMask);		
	//RTW_INFO("RFR-%d Addr[0x%x]=0x%x\n", eRFPath, RegAddr, Readback_Value);

	return Readback_Value;
}


static	VOID
phy_RFWrite_8814A(
	IN	PADAPTER			Adapter,
	IN	u8				eRFPath,
	IN	u32				Offset,
	IN	u32				Data
	)
{
	u32						DataAndAddr = 0;
	HAL_DATA_TYPE				*pHalData = GET_HAL_DATA(Adapter);
	BB_REGISTER_DEFINITION_T		*pPhyReg = &pHalData->PHYRegDef[eRFPath];

	// 2009/06/17 MH We can not execute IO for power save or other accident mode.
	//if(RT_CANNOT_IO(Adapter))
	//{
		//RT_DISP(FPHY, PHY_RFW, ("phy_RFSerialWrite stop\n"));
		//return;
	//}

	Offset &= 0xff;

	// Shadow Update
	//PHY_RFShadowWrite(Adapter, eRFPath, Offset, Data);

	// Put write addr in [27:20]  and write data in [19:00]
	DataAndAddr = ((Offset<<20) | (Data&0x000fffff)) & 0x0fffffff;	

	// Write Operation 
	phy_set_bb_reg(Adapter, pPhyReg->rf3wireOffset, bMaskDWord, DataAndAddr);
	//RTW_INFO("RFW-%d Addr[0x%x]=0x%x\n", eRFPath, pPhyReg->rf3wireOffset, DataAndAddr);
}


u32
PHY_QueryRFReg8814A(
	IN	PADAPTER			Adapter,
	IN	enum rf_path				eRFPath,
	IN	u32				RegAddr,
	IN	u32				BitMask
	)
{
	u32	Readback_Value;	

#if (DISABLE_BB_RF == 1)
	return 0;
#endif
	
	Readback_Value = phy_RFRead_8814A(Adapter, eRFPath, RegAddr, BitMask);

	return (Readback_Value);
}


VOID
PHY_SetRFReg8814A(
	IN	PADAPTER			Adapter,
	IN	enum rf_path				eRFPath,
	IN	u32				RegAddr,
	IN	u32				BitMask,
	IN	u32				Data
	)
{

#if (DISABLE_BB_RF == 1)
	return;
#endif

	if(BitMask == 0)
		return;

	RegAddr &= 0xff;
	// RF data is 20 bits only
	if (BitMask != bLSSIWrite_data_Jaguar) {
		u32			Original_Value, BitShift;

		Original_Value = phy_RFRead_8814A(Adapter, eRFPath, RegAddr, bLSSIWrite_data_Jaguar);
		BitShift =  PHY_CalculateBitShift(BitMask);
		Data = ((Original_Value) & (~BitMask)) | (Data<< BitShift);
	}
	
	phy_RFWrite_8814A(Adapter, eRFPath, RegAddr, Data);


}

//
// 3. Initial MAC/BB/RF config by reading MAC/BB/RF txt.
//

s32 PHY_MACConfig8814(PADAPTER Adapter)
{
	int				rtStatus = _FAIL;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	//
	// Config MAC
	//
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	rtStatus = phy_ConfigMACWithParaFile(Adapter, PHY_FILE_MAC_REG);
	if (rtStatus == _FAIL)
#endif //CONFIG_LOAD_PHY_PARA_FROM_FILE
	{
#ifdef CONFIG_EMBEDDED_FWIMG
		odm_config_mac_with_header_file(&pHalData->odmpriv);
		rtStatus = _SUCCESS;
#endif//CONFIG_EMBEDDED_FWIMG
	}

	return rtStatus;
}


static	VOID
phy_InitBBRFRegisterDefinition(
	IN	PADAPTER		Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);	

	// RF Interface Sowrtware Control
	pHalData->PHYRegDef[RF_PATH_A].rfintfs = rFPGA0_XAB_RFInterfaceSW; // 16 LSBs if read 32-bit from 0x870
	pHalData->PHYRegDef[RF_PATH_B].rfintfs = rFPGA0_XAB_RFInterfaceSW; // 16 MSBs if read 32-bit from 0x870 (16-bit for 0x872)

	// RF Interface Output (and Enable)
	pHalData->PHYRegDef[RF_PATH_A].rfintfo = rFPGA0_XA_RFInterfaceOE; // 16 LSBs if read 32-bit from 0x860
	pHalData->PHYRegDef[RF_PATH_B].rfintfo = rFPGA0_XB_RFInterfaceOE; // 16 LSBs if read 32-bit from 0x864

	// RF Interface (Output and)  Enable
	pHalData->PHYRegDef[RF_PATH_A].rfintfe = rFPGA0_XA_RFInterfaceOE; // 16 MSBs if read 32-bit from 0x860 (16-bit for 0x862)
	pHalData->PHYRegDef[RF_PATH_B].rfintfe = rFPGA0_XB_RFInterfaceOE; // 16 MSBs if read 32-bit from 0x864 (16-bit for 0x866)

	if(IS_HARDWARE_TYPE_JAGUAR_AND_JAGUAR2(Adapter))
	{
		pHalData->PHYRegDef[RF_PATH_A].rf3wireOffset = rA_LSSIWrite_Jaguar; //LSSI Parameter
		pHalData->PHYRegDef[RF_PATH_B].rf3wireOffset = rB_LSSIWrite_Jaguar;

		pHalData->PHYRegDef[RF_PATH_A].rfHSSIPara2 = rHSSIRead_Jaguar;  //wire control parameter2
		pHalData->PHYRegDef[RF_PATH_B].rfHSSIPara2 = rHSSIRead_Jaguar;  //wire control parameter2
	}
	else
	{
		pHalData->PHYRegDef[RF_PATH_A].rf3wireOffset = rFPGA0_XA_LSSIParameter; //LSSI Parameter
		pHalData->PHYRegDef[RF_PATH_B].rf3wireOffset = rFPGA0_XB_LSSIParameter;

		pHalData->PHYRegDef[RF_PATH_A].rfHSSIPara2 = rFPGA0_XA_HSSIParameter2;  //wire control parameter2
		pHalData->PHYRegDef[RF_PATH_B].rfHSSIPara2 = rFPGA0_XB_HSSIParameter2;  //wire control parameter2
	}

	if(IS_HARDWARE_TYPE_8814A(Adapter))
	{
		pHalData->PHYRegDef[RF_PATH_C].rf3wireOffset = rC_LSSIWrite_Jaguar2; //LSSI Parameter
		pHalData->PHYRegDef[RF_PATH_D].rf3wireOffset = rD_LSSIWrite_Jaguar2;

		pHalData->PHYRegDef[RF_PATH_C].rfHSSIPara2 = rHSSIRead_Jaguar;  //wire control parameter2
		pHalData->PHYRegDef[RF_PATH_D].rfHSSIPara2 = rHSSIRead_Jaguar;  //wire control parameter2
	}
	
	if(IS_HARDWARE_TYPE_JAGUAR_AND_JAGUAR2(Adapter))
	{
		 // Tranceiver Readback LSSI/HSPI mode 
		pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBack = rA_SIRead_Jaguar;
		pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBack = rB_SIRead_Jaguar;
		pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBackPi = rA_PIRead_Jaguar;
		pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBackPi = rB_PIRead_Jaguar;
	}
	else
	{
		// Tranceiver Readback LSSI/HSPI mode 
		pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBack = rFPGA0_XA_LSSIReadBack;
		pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBack = rFPGA0_XB_LSSIReadBack;
		pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBackPi = TransceiverA_HSPI_Readback;
		pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBackPi = TransceiverB_HSPI_Readback;
	}

	if(IS_HARDWARE_TYPE_8814A(Adapter))
	{
		// Tranceiver Readback LSSI/HSPI mode 
		pHalData->PHYRegDef[RF_PATH_C].rfLSSIReadBack = rC_SIRead_Jaguar2;
		pHalData->PHYRegDef[RF_PATH_D].rfLSSIReadBack = rD_SIRead_Jaguar2;
		pHalData->PHYRegDef[RF_PATH_C].rfLSSIReadBackPi = rC_PIRead_Jaguar2;
		pHalData->PHYRegDef[RF_PATH_D].rfLSSIReadBackPi = rD_PIRead_Jaguar2;
	}

	//pHalData->bPhyValueInitReady=TRUE;
}


int
PHY_BBConfig8814(
	IN	PADAPTER	Adapter
	)
{
	int	rtStatus = _SUCCESS;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8	TmpU1B=0;

	phy_InitBBRFRegisterDefinition(Adapter);

    // . APLL_EN,,APLL_320_GATEB,APLL_320BIAS,  auto config by hw fsm after pfsm_go (0x4 bit 8) set
	TmpU1B = PlatformEFIORead1Byte(Adapter, REG_SYS_FUNC_EN_8814A);

	if(IS_HARDWARE_TYPE_8814AU(Adapter))
		TmpU1B |= FEN_USBA;
	else if(IS_HARDWARE_TYPE_8814AE(Adapter))
		TmpU1B |= FEN_PCIEA;

	PlatformEFIOWrite1Byte(Adapter, REG_SYS_FUNC_EN, TmpU1B);

	TmpU1B = PlatformEFIORead1Byte(Adapter, 0x1002);
	PlatformEFIOWrite1Byte(Adapter, 0x1002, (TmpU1B|FEN_BB_GLB_RSTn|FEN_BBRSTB));//same with 8812

	//6. 0x1f[7:0] = 0x07 PathA RF Power On
	PlatformEFIOWrite1Byte(Adapter, REG_RF_CTRL0_8814A , 0x07);//RF_SDMRSTB,RF_RSTB,RF_EN same with 8723a
	//7. 0x20[7:0] = 0x07 PathB RF Power On
	//8. 0x21[7:0] = 0x07 PathC RF Power On
	PlatformEFIOWrite2Byte(Adapter, REG_RF_CTRL1_8814A , 0x0707);//RF_SDMRSTB,RF_RSTB,RF_EN same with 8723a    
	//9. 0x76[7:0] = 0x07 PathD RF Power On
	PlatformEFIOWrite1Byte(Adapter, REG_RF_CTRL3_8814A , 0x7);

	//
	// Config BB and AGC
	//
	rtStatus = phy_BB8814A_Config_ParaFile(Adapter);

	hal_set_crystal_cap(Adapter, pHalData->crystal_cap);
	
	switch (Adapter->registrypriv.rf_config) {
	case RF_1T1R:
	case RF_2T4R:
	case RF_3T3R:
		/*RX CCK disable 2R CCA*/
		phy_set_bb_reg(Adapter, rCCK0_FalseAlarmReport+2, BIT2|BIT6, 0);
		/*pathB tx on, path A/C/D tx off*/
		phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0xf0000000, 0x4);
		/*pathB rx*/
		phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0x0f000000, 0x5);
		break;
	default:
		/*RX CCK disable 2R CCA*/
		phy_set_bb_reg(Adapter, rCCK0_FalseAlarmReport+2, BIT2|BIT6, 0);
		/*pathB tx on, path A/C/D tx off*/
		phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0xf0000000, 0x4);
		/*pathB rx*/
		phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0x0f000000, 0x5);
		RTW_INFO("%s, unknown rf_config: %d\n", __func__, Adapter->registrypriv.rf_config);
		break;
	}
	
	return rtStatus;	
}

int phy_BB8814A_Config_ParaFile(
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
			RTW_INFO("phy_BB8814_Config_ParaFile():Write BB Reg MP Fail!!\n");
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


VOID
phy_ADC_CLK_8814A(
	IN	PADAPTER	Adapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32			MAC_REG_520, BB_REG_8FC, BB_REG_808, RXIQC[4];
	u32			Search_index = 0, MAC_Active = 1;
	u32			RXIQC_REG[2][4] = {{0xc10, 0xe10, 0x1810, 0x1a10}, {0xc14, 0xe14, 0x1814, 0x1a14}} ;

	if (GET_CVID_CUT_VERSION(pHalData->version_id) != A_CUT_VERSION)
		return;

//1 Step1. MAC TX pause
	MAC_REG_520 = phy_query_bb_reg( Adapter, 0x520, bMaskDWord);
	BB_REG_8FC = phy_query_bb_reg( Adapter, 0x8fc, bMaskDWord);
	BB_REG_808 = phy_query_bb_reg( Adapter, 0x808, bMaskDWord);
	phy_set_bb_reg(Adapter, 0x520, bMaskByte2, 0x3f);
	
//1 Step 2. Backup RXIQC & RXIQC = 0
	for(Search_index = 0; Search_index<4; Search_index++){
		RXIQC[Search_index] = phy_query_bb_reg( Adapter, RXIQC_REG[0][Search_index], bMaskDWord);
		phy_set_bb_reg(Adapter, RXIQC_REG[0][Search_index], bMaskDWord, 0x0);
		phy_set_bb_reg(Adapter, RXIQC_REG[1][Search_index], bMaskDWord, 0x0);
	}
	phy_set_bb_reg(Adapter, 0xa14, 0x00000300, 0x3);
	Search_index = 0;
	
//1 Step 3. Monitor MAC IDLE
	phy_set_bb_reg(Adapter, 0x8fc, bMaskDWord, 0x0);
	while(MAC_Active){
		MAC_Active = phy_query_bb_reg( Adapter, 0xfa0, bMaskDWord) & (0x803e0008);
		Search_index++;
		if(Search_index>1000){
			break;
		}
	}

//1 Step 4. ADC clk flow
	phy_set_bb_reg(Adapter, 0x808, bMaskByte0, 0x11);
	phy_set_bb_reg(Adapter, 0x90c, BIT(13), 0x1);
	phy_set_bb_reg(Adapter, 0x764, BIT(10)|BIT(9), 0x3);
	phy_set_bb_reg(Adapter, 0x804, BIT(2), 0x1);

	// 0xc1c/0xe1c/0x181c/0x1a1c[4] must=1 to ensure table can be written when bbrstb=0         
	// 0xc60/0xe60/0x1860/0x1a60[15] always = 1 after this line              
	// 0xc60/0xe60/0x1860/0x1a60[14] always = 0 bcz its error in A-cut          

	// power_off/clk_off @ anapar_state=idle mode
	phy_set_bb_reg(Adapter, 0xc60, bMaskDWord, 0x15800002);	//0xc60       0x15808002    
	phy_set_bb_reg(Adapter, 0xc60, bMaskDWord, 0x01808003);	//0xc60       0x01808003
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0x15800002);	//0xe60      0x15808002    
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0x01808003);	//0xe60      0x01808003    
	phy_set_bb_reg(Adapter, 0x1860, bMaskDWord, 0x15800002);	//0x1860    0x15808002    
	phy_set_bb_reg(Adapter, 0x1860, bMaskDWord, 0x01808003);	//0x1860    0x01808003    
	phy_set_bb_reg(Adapter, 0x1a60, bMaskDWord, 0x15800002);	//0x1a60    0x15808002        
	phy_set_bb_reg(Adapter, 0x1a60, bMaskDWord, 0x01808003);	//0x1a60    0x01808003     
 
	phy_set_bb_reg(Adapter, 0x764, BIT(10), 0x0);
	phy_set_bb_reg(Adapter, 0x804, BIT(2), 0x0);
	phy_set_bb_reg(Adapter, 0xc5c, bMaskDWord,  0x0D080058);	//0xc5c       0x00080058  // [19] =1 to turn off ADC  
	phy_set_bb_reg(Adapter, 0xe5c, bMaskDWord,  0x0D080058);	//0xe5c       0x00080058  // [19] =1 to turn off ADC     
	phy_set_bb_reg(Adapter, 0x185c, bMaskDWord,  0x0D080058);	//0x185c     0x00080058 // [19] =1 to turn off ADC    
	phy_set_bb_reg(Adapter, 0x1a5c, bMaskDWord,  0x0D080058);	//0x1a5c     0x00080058 // [19] =1 to turn off ADC       
 
	// power_on/clk_off 
	//phy_set_bb_reg(Adapter, 0x764, BIT(10), 0x1);
	phy_set_bb_reg(Adapter, 0xc5c, bMaskDWord,  0x0D000058);	//0xc5c       0x0D000058   // [19] =0 to turn on ADC    
	phy_set_bb_reg(Adapter, 0xe5c, bMaskDWord,  0x0D000058);	//0xe5c       0x0D000058  // [19] =0 to turn on ADC     
	phy_set_bb_reg(Adapter, 0x185c, bMaskDWord,  0x0D000058);	//0x185c     0x0D000058  // [19] =0 to turn on ADC     
	phy_set_bb_reg(Adapter, 0x1a5c, bMaskDWord,  0x0D000058);	//0x1a5c     0x0D000058 // [19] =0 to turn on ADC       

	// power_on/clk_on @ anapar_state=BT mode
	phy_set_bb_reg(Adapter, 0xc60, bMaskDWord, 0x05808032);	//0xc60 0x05808002 
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0x05808032);	//0xe60 0x05808002           
	phy_set_bb_reg(Adapter, 0x1860, bMaskDWord, 0x05808032);	//0x1860 0x05808002    
	phy_set_bb_reg(Adapter, 0x1a60, bMaskDWord, 0x05808032);	//0x1a60 0x05808002            
       phy_set_bb_reg(Adapter, 0x764, BIT(10), 0x1);
	phy_set_bb_reg(Adapter, 0x804, BIT(2), 0x1);

	// recover original setting @ anapar_state=BT mode                              
	phy_set_bb_reg(Adapter, 0xc60, bMaskDWord, 0x05808032);	//0xc60       0x05808036    
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0x05808032);	//0xe60      0x05808036    
	phy_set_bb_reg(Adapter, 0x1860, bMaskDWord, 0x05808032);	//0x1860    0x05808036    
	phy_set_bb_reg(Adapter, 0x1a60, bMaskDWord, 0x05808032);	//0x1a60    0x05808036        

	phy_set_bb_reg(Adapter, 0xc60, bMaskDWord, 0x05800002);	//0xc60       0x05800002    
	phy_set_bb_reg(Adapter, 0xc60, bMaskDWord, 0x07808003);	//0xc60       0x07808003
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0x05800002);	//0xe60      0x05800002    
	phy_set_bb_reg(Adapter, 0xe60, bMaskDWord, 0x07808003);	//0xe60      0x07808003    
	phy_set_bb_reg(Adapter, 0x1860, bMaskDWord, 0x05800002);	//0x1860    0x05800002    
	phy_set_bb_reg(Adapter, 0x1860, bMaskDWord, 0x07808003);	//0x1860    0x07808003    
	phy_set_bb_reg(Adapter, 0x1a60, bMaskDWord, 0x05800002);	//0x1a60    0x05800002        
	phy_set_bb_reg(Adapter, 0x1a60, bMaskDWord, 0x07808003);	//0x1a60    0x07808003     

	phy_set_bb_reg(Adapter, 0x764, BIT(10)|BIT(9), 0x0);
	phy_set_bb_reg(Adapter, 0x804, BIT(2), 0x0);
	phy_set_bb_reg(Adapter, 0x90c, BIT(13), 0x0);

//1 Step 5. Recover MAC TX & IQC
	phy_set_bb_reg(Adapter, 0x520, bMaskDWord, MAC_REG_520);
	phy_set_bb_reg(Adapter, 0x8fc, bMaskDWord, BB_REG_8FC);
	phy_set_bb_reg(Adapter, 0x808, bMaskDWord, BB_REG_808);
	for(Search_index = 0; Search_index<4; Search_index++){
		phy_set_bb_reg(Adapter, RXIQC_REG[0][Search_index], bMaskDWord, RXIQC[Search_index]);
		phy_set_bb_reg(Adapter, RXIQC_REG[1][Search_index], bMaskDWord, 0x01000000);
	}
	phy_set_bb_reg(Adapter, 0xa14, 0x00000300, 0x0);
}

VOID
PHY_ConfigBB_8814A(
	IN	PADAPTER	Adapter
	)
{

	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);

	RTW_DBG(" ===> PHY_ConfigBB_8814A() \n");
	phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar|bCCKEN_Jaguar, 0x3);
}



//2 3.3 RF Config

s32
PHY_RFConfig8814A(
	IN	PADAPTER	Adapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	int	rtStatus = _SUCCESS;

	//vivi added this, 20100610
	if (rtw_is_surprise_removed(Adapter))
		return _FAIL;

	switch(pHalData->rf_chip)
	{
		case RF_PSEUDO_11N:
			RTW_INFO("%s(): RF_PSEUDO_11N\n",__FUNCTION__);
			break;
		default: 
			rtStatus = PHY_RF6052_Config_8814A(Adapter);
			break;
	}

	return rtStatus;
}

//1 4. RF State setting API

/*  todo
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)

//
// 2009/11/03 MH add for LPS mode power save sequence.
// 2009/11/03 According to document V10.
// 2009/11/24 According to document V11. by tynli.
//
VOID
phy_SetRTL8814ERfOn(
	IN	PADAPTER			Adapter
)
{
	rtw_write8(Adapter, REG_SPS0_CTRL_8814A, 0x2b);

	// c.	For PCIE: SYS_FUNC_EN 0x02[7:0] = 0xE3	//enable BB TRX function
	//	For USB: SYS_FUNC_EN 0x02[7:0] = 0x17
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A, 0xE3);
#else
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A, 0x17);
#endif

	// RF_ON_EXCEP(d~g):
	// d.	APSD_CTRL 0x600[7:0] = 0x00
	//rtw_write8(Adapter, REG_APSD_CTRL, 0x00);

	// e.	For PCIE: SYS_FUNC_EN 0x02[7:0] = 0xE2	//reset BB TRX function again
	//f.	For PCIE: SYS_FUNC_EN 0x02[7:0] = 0xE3	//enable BB TRX function
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A, 0xE2);
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A, 0xE3);
#else
	// e.For USB: SYS_FUNC_EN 0x02[7:0] = 0x16
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A, 0x16);
	// f. For USB: SYS_FUNC_EN 0x02[7:0] = 0x17
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A, 0x17);
#endif

	// g.	TXPAUSE 0x522[7:0] = 0x00				//enable MAC TX queue
	rtw_write8(Adapter, REG_TXPAUSE_8814A, 0x00);
}	// phy_SetRTL8188EERfSleep


BOOLEAN
phy_SetRFPowerState_8814E(
	IN	PADAPTER			Adapter,
	IN	rt_rf_power_state	eRFPowerState
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	BOOLEAN			bResult = TRUE;
	u8			i, QueueID;
	PRT_POWER_SAVE_CONTROL	pPSC = GET_POWER_SAVE_CONTROL(pMgntInfo);
	
	pHalData->SetRFPowerStateInProgress = TRUE;

	switch( eRFPowerState )
	{
		//
		// SW radio on/IPS site survey call will execute all flow
		// HW radio on
		//
		case eRfOn:
			{
			#if(MUTUAL_AUTHENTICATION == 1)
				if(pHalData->MutualAuthenticationFail)
					break;
			#endif
				if((pHalData->eRFPowerState == eRfOff) && RT_IN_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC))
				{ // The current RF state is OFF and the RF OFF level is halting the NIC, re-initialize the NIC.
					s32 rtstatus;
					u32 InitializeCount = 0;
					do
					{	
						InitializeCount++;
						rtstatus = NicIFEnableNIC( Adapter );
					}while( (rtstatus != _SUCCESS) &&(InitializeCount <10) );
					RT_ASSERT(rtstatus == _SUCCESS,("Nic Initialize Fail\n"));
					RT_CLEAR_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC);
				}
				else
				{ // This is the normal case, we just turn on the RF.
					phy_SetRTL8814ERfOn(Adapter);
				}
		
				// Turn on RF we are still linked, which might happen when 
				// we quickly turn off and on HW RF. 2006.05.12, by rcnjko.
				if( pMgntInfo->bMediaConnect == TRUE )
					Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_LINK);
				else // Turn off LED if RF is not ON.
					Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_NO_LINK);
			}
			break;

		// Card Disable/SW radio off/HW radio off/IPS enter call
		case eRfOff:
			{					
				// Make sure BusyQueue is empty befor turn off RFE pwoer.
				for(QueueID = 0, i = 0; QueueID < MAX_TX_QUEUE; )
				{
					if(RTIsListEmpty(&Adapter->TcbBusyQueue[QueueID]))
					{
						QueueID++;
						continue;
					}
					else if(IsLowPowerState(Adapter))
					{
							RT_TRACE((COMP_POWER|COMP_RF), DBG_LOUD, 
							("eRf Off/Sleep: %d times TcbBusyQueue[%d] !=0 but lower power state!\n", (i+1), QueueID));
						break;
					}
					else
					{
						RT_TRACE((COMP_POWER|COMP_RF), DBG_LOUD, 
							("eRf Off/Sleep: %d times TcbBusyQueue[%d] !=0 before doze!\n", (i+1), QueueID));
							PlatformStallExecution(10);
						i++;
					}
					
					if(i >= MAX_DOZE_WAITING_TIMES_9x)
					{
						RT_TRACE((COMP_POWER|COMP_RF), DBG_WARNING, ("\n\n\n SetZebraRFPowerState8185B(): eRfOff: %d times TcbBusyQueue[%d] != 0 !!!\n\n\n", MAX_DOZE_WAITING_TIMES_9x, QueueID));
						break;
					}
				}

				if(pPSC->RegRfPsLevel & RT_RF_OFF_LEVL_HALT_NIC)
				{ // Disable all components.
					NicIFDisableNIC(Adapter);

					if(IS_HARDWARE_TYPE_8814AE(Adapter))
						NicIFEnableInterrupt(Adapter);
					RT_SET_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC);
				} 
				else
				{ // Normal case.
					//If Rf off reason is from IPS, Led should blink with no link, by Maddest 071015
					if(pMgntInfo->RfOffReason==RF_CHANGE_BY_IPS )
						Adapter->HalFunc.LedControlHandler(Adapter,LED_CTL_NO_LINK); 
					else // Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_POWER_OFF); 
				}
			}
			break;

		default:
		case eRfSleep:// Not used LPS is running on FW
			bResult = FALSE;
			RT_ASSERT(FALSE, ("phy_SetRFPowerState_8814E(): unknow state to set: 0x%X!!!\n", eRFPowerState));
			break;
	} 

	if(bResult)
	{
		// Update current RF state variable.
		pHalData->eRFPowerState = eRFPowerState;
	}
	
	pHalData->SetRFPowerStateInProgress = FALSE;

	return bResult;
}

#elif (DEV_BUS_TYPE == RT_USB_INTERFACE)

BOOLEAN
phy_SetRFPowerState_8814U(
	IN	PADAPTER			Adapter,
	IN	rt_rf_power_state	eRFPowerState
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	BOOLEAN			bResult = TRUE;
	u8			i, QueueID;
	PRT_USB_DEVICE				pDevice = GET_RT_USB_DEVICE(Adapter);
	
	if(pHalData->SetRFPowerStateInProgress == TRUE)
		return FALSE;
	
	pHalData->SetRFPowerStateInProgress = TRUE;
	RTW_DBG("======> phy_SetRFPowerState_8814U .\n");

	switch( eRFPowerState )
	{
		case eRfOn:
			if((pHalData->eRFPowerState == eRfOff) &&
				RT_IN_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC))
			{ // The current RF state is OFF and the RF OFF level is halting the NIC, re-initialize the NIC.
				RT_TRACE(COMP_RF, DBG_LOUD, ("======> phy_SetRFPowerState_8814U-eRfOn .\n"));			

				if(!Adapter->bInHctTest)
				{
					// 2010/09/01 MH For 92CU, we do not make sure the RF B short initialize sequence
					// So disable the different RF on/off sequence for hidden AP.
					NicIFEnableNIC(Adapter);
					RT_CLEAR_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC);
				}
			}
			break;
	    
		// 
		// In current solution, RFSleep=RFOff in order to save power under 802.11 power save.
		// By Bruce, 2008-01-16.
		//
		case eRfSleep:
			{
				// ToDo:
			}
			break;
			
		case eRfOff:
			// HW setting had been configured.
			// Both of these RF configures are the same, configuring twice may cause HW abnormal.
			if(pHalData->eRFPowerState == eRfSleep || pHalData->eRFPowerState== eRfOff)
				break;
			rtw_write8(Adapter, 0xf015, 0x40);  //page added for usb3 bus
			// Make sure BusyQueue is empty befor turn off RFE pwoer.
			for(QueueID = 0, i = 0; QueueID < MAX_TX_QUEUE; )
			{
				if(RTIsListEmpty(&Adapter->TcbBusyQueue[QueueID]))
				{
					QueueID++;
					continue;
				}					
				else
				{
					RT_TRACE(COMP_POWER, DBG_LOUD, ("eRf Off/Sleep: %d times TcbBusyQueue[%d] !=0 before doze!\n", (i+1), QueueID));
					PlatformSleepUs(10);
					i++;
				}
				
				if(i >= MAX_DOZE_WAITING_TIMES_9x)
				{
					RT_TRACE(COMP_POWER, DBG_LOUD, ("\n\n\n SetZebraRFPowerState8185B(): eRfOff: %d times TcbBusyQueue[%d] != 0 !!!\n\n\n", MAX_DOZE_WAITING_TIMES_9x, QueueID));
					break;
				}
			}				

			// 
			//RF Off/Sleep sequence. Designed/tested from SD4 Scott, SD1 Grent and Jonbon.
			// Added by 
			//
			//==================================================================				
			// CU will call card disable flow to set RF off, such that we call halt directly
			// and set the PS_LEVEL to HALT_NIC or we might call halt twice in N6usbHalt in some cases.
			// 2010.03.05. Added by tynli. 				
			if(pMgntInfo->RfOffReason & RF_CHANGE_BY_IPS ||
				pMgntInfo->RfOffReason & RF_CHANGE_BY_HW ||
				pMgntInfo->RfOffReason & RF_CHANGE_BY_SW)
			{	//for HW/Sw radio off and IPS flow
				//RT_TRACE(COMP_INIT, DBG_LOUD, ("======> CardDisableWithoutHWSM -eRfOff.\n"));				
				if(!Adapter->bInHctTest)
				{
					// 2010/09/01 MH For 92CU, we do not make sure the RF B short initialize sequence
					// So disable the different RF on/off sequence for hidden AP.
					NicIFDisableNIC(Adapter);
					RT_SET_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC);
				}
			}				
			break;

		default:
			bResult = FALSE;
			RT_ASSERT(FALSE, ("phy_SetRFPowerState_8814U(): unknow state to set: 0x%X!!!\n", eRFPowerState));
			break;
	} 
			
	if(bResult)
	{
		// Update current RF state variable.
		pHalData->eRFPowerState = eRFPowerState;
		
		switch(pHalData->rf_chip )
		{
			default:		
			switch(pHalData->eRFPowerState)
			{
				case eRfOff:
					//
					//If Rf off reason is from IPS, Led should blink with no link, by Maddest 071015
					//
					if(pMgntInfo->RfOffReason==RF_CHANGE_BY_IPS )
						Adapter->HalFunc.LedControlHandler(Adapter,LED_CTL_NO_LINK); 
					else		// Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_POWER_OFF); 
					break;
        		
				case eRfOn:
					// Turn on RF we are still linked, which might happen when 
					// we quickly turn off and on HW RF. 2006.05.12, by rcnjko.
					if( pMgntInfo->bMediaConnect == TRUE )
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_LINK); 
					else		// Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_NO_LINK);
					break;
        		
				default:
					// do nothing.
					break;
			}// Switch RF state

			break;
		}// Switch rf_chip
	}
	
	pHalData->SetRFPowerStateInProgress = FALSE;
	RT_TRACE(COMP_INIT, DBG_LOUD, ("<====== phy_SetRFPowerState_8814U .\n"));
	return bResult;
}

#elif DEV_BUS_TYPE == RT_SDIO_INTERFACE

BOOLEAN
phy_SetRFPowerState_8814Sdio(
	IN	PADAPTER			Adapter,
	IN	rt_rf_power_state	eRFPowerState
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	BOOLEAN			bResult = TRUE;
	u8			i, QueueID;
	PRT_SDIO_DEVICE				pDevice = GET_RT_SDIO_DEVICE(Adapter);
	
	if(pHalData->SetRFPowerStateInProgress == TRUE)
		return FALSE;
	
	pHalData->SetRFPowerStateInProgress = TRUE;
	RT_TRACE(COMP_INIT, DBG_LOUD, ("======> phy_SetRFPowerState_8814Sdio .\n"))
		
	switch( eRFPowerState )
	{
		case eRfOn:
			if((pHalData->eRFPowerState == eRfOff) &&
				RT_IN_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC))
			{ // The current RF state is OFF and the RF OFF level is halting the NIC, re-initialize the NIC.
				RT_TRACE(COMP_RF, DBG_LOUD, ("======> phy_SetRFPowerState_8814Sdio-eRfOn .\n"));			

				if(!Adapter->bInHctTest)
				{													
					// 2010/09/01 MH For 92CU, we do not make sure the RF B short initialize sequence
					// So disable the different RF on/off sequence for hidden AP.
					NicIFEnableNIC(Adapter);
					RT_CLEAR_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC);
				}
			}

			// 2010/08/26 MH Prevent IQK to send out packet.
			if(pHalData->bIQKInitialized )
				phy_iq_calibrate_8814a(Adapter, TRUE);
			else
			{
				phy_iq_calibrate_8814a(Adapter,FALSE);
				pHalData->bIQKInitialized = _TRUE;
			}
			break;
	    
		// 
		// In current solution, RFSleep=RFOff in order to save power under 802.11 power save.
		// By Bruce, 2008-01-16.
		//
		case eRfSleep:
			{
				// ToDo:
			}
			break;
			
		case eRfOff:
			// HW setting had been configured.
			// Both of these RF configures are the same, configuring twice may cause HW abnormal.
			if(pHalData->eRFPowerState == eRfSleep || pHalData->eRFPowerState== eRfOff)
				break;

			// Make sure BusyQueue is empty befor turn off RFE pwoer.
			for(QueueID = 0, i = 0; QueueID < MAX_TX_QUEUE; )
			{
				if(RTIsListEmpty(&Adapter->TcbBusyQueue[QueueID]))
				{
					//DbgPrint("QueueID = %d", QueueID);
					QueueID++;
					continue;
				}					
				else
				{
					RT_TRACE(COMP_POWER, DBG_LOUD, ("eRf Off/Sleep: %d times TcbBusyQueue[%d] !=0 before doze!\n", (i+1), QueueID));
					PlatformSleepUs(10);
					i++;
				}
				
				if(i >= MAX_DOZE_WAITING_TIMES_9x)
				{
					RT_TRACE(COMP_POWER, DBG_LOUD, ("\n\n\n SetZebraRFPowerState8185B(): eRfOff: %d times TcbBusyQueue[%d] != 0 !!!\n\n\n", MAX_DOZE_WAITING_TIMES_9x, QueueID));
					break;
				}
			}				

			// 
			//RF Off/Sleep sequence. Designed/tested from SD4 Scott, SD1 Grent and Jonbon.
			// Added by 
			//
			//==================================================================				
			// CU will call card disable flow to set RF off, such that we call halt directly
			// and set the PS_LEVEL to HALT_NIC or we might call halt twice in N6usbHalt in some cases.
			// 2010.03.05. Added by tynli. 				
			if(pMgntInfo->RfOffReason & RF_CHANGE_BY_IPS ||
				pMgntInfo->RfOffReason & RF_CHANGE_BY_HW ||
				pMgntInfo->RfOffReason & RF_CHANGE_BY_SW)
			{	//for HW/Sw radio off and IPS flow
				//RT_TRACE(COMP_INIT, DBG_LOUD, ("======> CardDisableWithoutHWSM -eRfOff.\n"));				
				if(!Adapter->bInHctTest)
				{											
					// 2010/09/01 MH For 92CU, we do not make sure the RF B short initialize sequence
					// So disable the different RF on/off sequence for hidden AP.
					NicIFDisableNIC(Adapter);		
					
					RT_SET_PS_LEVEL(Adapter, RT_RF_OFF_LEVL_HALT_NIC);
				}
			}				
			break;

		default:
			bResult = FALSE;
			RT_ASSERT(FALSE, ("phy_SetRFPowerState_8814Sdio(): unknow state to set: 0x%X!!!\n", eRFPowerState));
			break;
	} 
			
	if(bResult)
	{
		// Update current RF state variable.
		pHalData->eRFPowerState = eRFPowerState;
		
		switch(pHalData->rf_chip )
		{
			default:		
			switch(pHalData->eRFPowerState)
			{
				case eRfOff:
					//
					//If Rf off reason is from IPS, Led should blink with no link, by Maddest 071015
					//
					if(pMgntInfo->RfOffReason==RF_CHANGE_BY_IPS )
						Adapter->HalFunc.LedControlHandler(Adapter,LED_CTL_NO_LINK); 
					else		// Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_POWER_OFF); 
					break;
        		
				case eRfOn:
					// Turn on RF we are still linked, which might happen when 
					// we quickly turn off and on HW RF. 2006.05.12, by rcnjko.
					if( pMgntInfo->bMediaConnect == TRUE )
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_LINK); 
					else		// Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_NO_LINK); 
					break;
        		
				default:
					// do nothing.
					break;
			}// Switch RF state

				break;
		}// Switch rf_chip
	}
	
	pHalData->SetRFPowerStateInProgress = FALSE;

	return bResult;
}

#endif



BOOLEAN
PHY_SetRFPowerState8814A(
	IN	PADAPTER			Adapter, 
	IN	rt_rf_power_state	eRFPowerState
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	BOOLEAN			bResult = FALSE;

	RT_TRACE(COMP_RF, DBG_LOUD, ("---------> PHY_SetRFPowerState8814(): eRFPowerState(%d)\n", eRFPowerState));
	if(eRFPowerState == pHalData->eRFPowerState)
	{
		RT_TRACE(COMP_RF, DBG_LOUD, ("<--------- PHY_SetRFPowerState8814(): discard the request for eRFPowerState(%d) is the same.\n", eRFPowerState));
		return bResult;
	}
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)
	bResult = phy_SetRFPowerState_8814E(Adapter, eRFPowerState);
#elif (DEV_BUS_TYPE == RT_USB_INTERFACE)
	bResult = phy_SetRFPowerState_8814U(Adapter, eRFPowerState);
#elif (DEV_BUS_TYPE == RT_SDIO_INTERFACE)
	bResult = phy_SetRFPowerState_8814Sdio(Adapter, eRFPowerState);
#endif

	RT_TRACE(COMP_RF, DBG_LOUD, ("<--------- PHY_SetRFPowerState8814(): bResult(%d)\n", bResult));

	return bResult;
}
todo */
//1 5. Tx  Power setting API

VOID
phy_TxPwrAdjInPercentage(
	IN		PADAPTER		Adapter,
	OUT		u8*				pTxPwrIdx)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	int txPower = *pTxPwrIdx + pHalData->CurrentTxPwrIdx - 18;

	*pTxPwrIdx = txPower > RF6052_MAX_TX_PWR ? RF6052_MAX_TX_PWR : txPower;
}

VOID
PHY_GetTxPowerLevel8814(
	IN	PADAPTER		Adapter,
	OUT s32*    		powerlevel
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	*powerlevel = pHalData->CurrentTxPwrIdx;
#if 0
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	s4Byte			TxPwrDbm = 13;

	if ( pMgntInfo->ClientConfigPwrInDbm != UNSPECIFIED_PWR_DBM )
		*powerlevel = pMgntInfo->ClientConfigPwrInDbm;
	else
		*powerlevel = TxPwrDbm;
#endif //0
/*
	//PMPT_CONTEXT            pMptCtx = &(Adapter->mppriv.mpt_ctx);
	//u8 mgn_rate = mpt_to_mgnt_rate(HwRateToMPTRate(Adapter->mppriv.rateidx));
	*powerlevel=PHY_GetTxPowerIndex8814A(Adapter,RF_PATH_A ,MGN_MCS7, pHalData->current_channel_bw, pHalData->current_channel, NULL);
	*powerlevel/=2;
*/
}

VOID
PHY_SetTxPowerLevel8814(
	IN	PADAPTER		Adapter,
	IN	u8			Channel
	)
{
	u32			i, j, k = 0;
	u32			value[264]={0};
	u32			path = 0, PowerIndex, txagc_table_wd = 0x00801000;

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	u8	jaguar2Rates[][4] =	{ {MGN_1M, MGN_2M, MGN_5_5M, MGN_11M}, 
								{MGN_6M, MGN_9M, MGN_12M, MGN_18M}, 
						  		{MGN_24M, MGN_36M, MGN_48M, MGN_54M}, 
						  		{MGN_MCS0, MGN_MCS1, MGN_MCS2, MGN_MCS3},
						  		{MGN_MCS4, MGN_MCS5, MGN_MCS6, MGN_MCS7}, 
						  		{MGN_MCS8, MGN_MCS9, MGN_MCS10, MGN_MCS11},
						   		{MGN_MCS12, MGN_MCS13, MGN_MCS14, MGN_MCS15},
						   		{MGN_MCS16, MGN_MCS17, MGN_MCS18, MGN_MCS19}, 
								{MGN_MCS20, MGN_MCS21, MGN_MCS22, MGN_MCS23},
						   		{MGN_VHT1SS_MCS0, MGN_VHT1SS_MCS1, MGN_VHT1SS_MCS2, MGN_VHT1SS_MCS3}, 
						   		{MGN_VHT1SS_MCS4, MGN_VHT1SS_MCS5, MGN_VHT1SS_MCS6, MGN_VHT1SS_MCS7}, 
						 		{MGN_VHT2SS_MCS8, MGN_VHT2SS_MCS9, MGN_VHT2SS_MCS0, MGN_VHT2SS_MCS1}, 
						 		{MGN_VHT2SS_MCS2, MGN_VHT2SS_MCS3, MGN_VHT2SS_MCS4, MGN_VHT2SS_MCS5}, 
								{MGN_VHT2SS_MCS6, MGN_VHT2SS_MCS7, MGN_VHT2SS_MCS8, MGN_VHT2SS_MCS9},
						   		{MGN_VHT3SS_MCS0, MGN_VHT3SS_MCS1, MGN_VHT3SS_MCS2, MGN_VHT3SS_MCS3}, 
						   		{MGN_VHT3SS_MCS4, MGN_VHT3SS_MCS5, MGN_VHT3SS_MCS6, MGN_VHT3SS_MCS7}, 
						 		{MGN_VHT3SS_MCS8, MGN_VHT3SS_MCS9, 0, 0}};	


	for( path = RF_PATH_A; path <= RF_PATH_D; ++path )
	{
			phy_set_tx_power_level_by_path(Adapter, Channel, (u8)path);
	}
#if 0 //todo H2C_TXPOWER_INDEX_OFFLOAD ?
	if(Adapter->MgntInfo.bScanInProgress == FALSE &&  pHalData->RegFWOffload == 2)
	{
		HalDownloadTxPowerLevel8814(Adapter, value);
	}
#endif //0
}

/**************************************************************************************************************
 *   Description:
 *       The low-level interface to get the FINAL Tx Power Index , called  by both MP and Normal Driver.
 *
 *                                                                                    <20120830, Kordan>
 **************************************************************************************************************/
u8
PHY_GetTxPowerIndex8814A(
	IN	PADAPTER		pAdapter,
	IN	enum rf_path		RFPath,
	IN	u8			Rate,
	IN	u8			BandWidth,
	IN	u8			Channel,
	struct txpwr_idx_comp *tic
	)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(pAdapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(pAdapter);
	s8				powerDiffByRate = 0;
	s8				txPower = 0, limit = 0;
	u8				ntx_idx = MgntQuery_NssTxRate(Rate );
	BOOLEAN			bIn24G = FALSE;
	s8				tpt_offset = 0;

	/* RTW_INFO( "===>%s\n", __FUNCTION__ ); */
	if(pAdapter->mppriv.bSetTxPower)
	{
		PMPT_CONTEXT pMptCtx = &(pAdapter->mppriv.mpt_ctx);
		txPower = pMptCtx->TxPwrLevel[RFPath];
	}
	else
	{
		txPower = (s8) PHY_GetTxPowerIndexBase( pAdapter, RFPath, Rate, ntx_idx, BandWidth, Channel, &bIn24G );

		powerDiffByRate = PHY_GetTxPowerByRate( pAdapter, (u8)(!bIn24G), RFPath, Rate );

		limit = PHY_GetTxPowerLimit( pAdapter, NULL, (u8)(!bIn24G), pHalData->current_channel_bw, RFPath, Rate,ntx_idx, pHalData->current_channel);
		tpt_offset = PHY_GetTxPowerTrackingOffset(pAdapter, RFPath, Rate);

		powerDiffByRate = powerDiffByRate > limit ? limit : powerDiffByRate;
		/*RTW_INFO("Rate-0x%x: (TxPower, PowerDiffByRate Path-%c) = (0x%X, %d)\n", Rate, ((RFPath==0)?'A':(RFPath==1)?'B':(RFPath==2)?'C':'D'), txPower, powerDiffByRate);*/

		txPower += powerDiffByRate;
		
		//txPower += PHY_GetTxPowerTrackingOffset( pAdapter, RFPath, Rate );
	#if 0 //todo ?
	#if CCX_SUPPORT
		CCX_CellPowerLimit( pAdapter, Channel, Rate, &txPower );
	#endif
	#endif
		phy_TxPwrAdjInPercentage(pAdapter, (u8 *)&txPower);
	}
	if (tic) {
		tic->ntx_idx = ntx_idx;
		tic->base = txPower;
		tic->by_rate = powerDiffByRate;
		tic->limit = limit;
		tic->tpt = tpt_offset;
		tic->ebias = 0;
	}

	if(txPower > hal_spec->txgi_max)
		txPower = hal_spec->txgi_max;

	//if (Adapter->registrypriv.mp_mode==0 && 
		//(pHalData->bautoload_fail_flag || pHalData->EfuseMap[EFUSE_INIT_MAP][EEPROM_TX_PWR_INX_JAGUAR] == 0xFF))
		//txPower = 0x12;

	/*RTW_INFO("Final Tx Power(RF-%c, Channel: %d) = %d(0x%X)\n", ((RFPath==0)?'A':(RFPath==1)?'B':(RFPath==2)?'C':'D'), Channel,
		txPower, txPower);*/

	return (u8) txPower;
}


VOID
PHY_SetTxPowerIndex_8814A(
	IN	PADAPTER			Adapter,
	IN	u32				PowerIndex,
	IN	enum rf_path				RFPath,
	IN	u8				Rate
	)
{
	u32	txagc_table_wd = 0x00801000;

	txagc_table_wd |= (RFPath << 8) | MRateToHwRate(Rate) | (PowerIndex << 24);
	phy_set_bb_reg(Adapter, 0x1998, bMaskDWord, txagc_table_wd);
	/* RTW_INFO("txagc_table_wd %x\n", txagc_table_wd); */
	if (Rate == MGN_1M) {
		phy_set_bb_reg(Adapter, 0x1998, bMaskDWord, txagc_table_wd);	/* first time to turn on the txagc table */
										/* second to write the addr0 */
	}
}


BOOLEAN
PHY_UpdateTxPowerDbm8814A(
	IN	PADAPTER	Adapter,
	IN	s4Byte		powerInDbm
	)
{
	return TRUE;
}


u32 
PHY_GetTxBBSwing_8814A(
	IN	PADAPTER	Adapter,
	IN	BAND_TYPE 	Band,
	IN 	enum rf_path 		RFPath
	)
{
    HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(GetDefaultAdapter(Adapter));
    struct dm_struct		*		pDM_Odm = &pHalData->odmpriv;
    struct dm_rf_calibration_struct	*  	pRFCalibrateInfo = &(pDM_Odm->rf_calibrate_info);
    s8 			bbSwing_2G = -1 * GetRegTxBBSwing_2G(Adapter);
    s8 			bbSwing_5G = -1 * GetRegTxBBSwing_5G(Adapter);
    u32          		out = 0x200;
    const s8		AUTO = -1;

	RTW_DBG("===> PHY_GetTxBBSwing_8814A, bbSwing_2G: %d, bbSwing_5G: %d\n", 
										  (s4Byte)bbSwing_2G, (s4Byte)bbSwing_5G);

    if ( pHalData->bautoload_fail_flag )
    {
		if ( Band == BAND_ON_2_4G ) 
		{
			pRFCalibrateInfo->bb_swing_diff_2g = bbSwing_2G;
			if      (bbSwing_2G == 0)  out = 0x200; //  0 dB
			else if (bbSwing_2G == -3) out = 0x16A; // -3 dB
			else if (bbSwing_2G == -6) out = 0x101; // -6 dB
			else if (bbSwing_2G == -9) out = 0x0B6; // -9 dB
			else 
			{
				if ( pHalData->ExternalPA_2G ) 
				{
					pRFCalibrateInfo->bb_swing_diff_2g = -3;
					out = 0x16A;
				} 
				else  
				{
					pRFCalibrateInfo->bb_swing_diff_2g = 0;
					out = 0x200;
				}
	            }
	        } 
		else if ( Band == BAND_ON_5G ) 
		{
			pRFCalibrateInfo->bb_swing_diff_5g = bbSwing_5G;
			if(bbSwing_5G == 0)  out = 0x200; //  0 dB
			else if (bbSwing_5G == -3) out = 0x16A; // -3 dB
			else if (bbSwing_5G == -6) out = 0x101; // -6 dB
			else if (bbSwing_5G == -9) out = 0x0B6; // -9 dB
			else 
			{
				if (pHalData->external_pa_5g) 
				{
					pRFCalibrateInfo->bb_swing_diff_5g = -3;
					out = 0x16A;
				} 
				else 
				{
					pRFCalibrateInfo->bb_swing_diff_5g = 0;
					out = 0x200;
				}
			}
		}
		else  
		{
			pRFCalibrateInfo->bb_swing_diff_2g = -3;
			pRFCalibrateInfo->bb_swing_diff_5g = -3;			
			out = 0x16A; // -3 dB
		}
	}
	else
	{
		u32 swing = 0, onePathSwing = 0;

		if (Band == BAND_ON_2_4G)
		{
			if (GetRegTxBBSwing_2G(Adapter) == AUTO)
			{
				EFUSE_ShadowRead(Adapter, 1, EEPROM_TX_BBSWING_2G_8814, (u32 *)&swing);
				if (swing == 0xFF) 
				{
					if(bbSwing_2G ==  0) swing = 0x00; //  0 dB
					else if (bbSwing_2G == -3) swing = 0x55; // -3 dB
					else if (bbSwing_2G == -6) swing = 0xAA; // -6 dB
					else if (bbSwing_2G == -9) swing = 0xFF; // -9 dB
					else swing = 0x00;					
				}
			}			
			else if (bbSwing_2G ==  0) swing = 0x00; //  0 dB
			else if (bbSwing_2G == -3) swing = 0x55; // -3 dB
			else if (bbSwing_2G == -6) swing = 0xAA; // -6 dB
			else if (bbSwing_2G == -9) swing = 0xFF; // -9 dB
			else swing = 0x00;
		}
		else
		{
			if (GetRegTxBBSwing_5G(Adapter) == AUTO)
			{
				EFUSE_ShadowRead(Adapter, 1, EEPROM_TX_BBSWING_5G_8814, (u32 *)&swing);
				if (swing == 0xFF) 
				{
					if(bbSwing_5G ==  0) swing = 0x00; //  0 dB
					else if (bbSwing_5G == -3) swing = 0x55; // -3 dB
					else if (bbSwing_5G == -6) swing = 0xAA; // -6 dB
					else if (bbSwing_5G == -9) swing = 0xFF; // -9 dB
					else swing = 0x00;
				}
			}
			else if (bbSwing_5G ==  0) swing = 0x00; //  0 dB
      			else if (bbSwing_5G == -3) swing = 0x55; // -3 dB
			else if (bbSwing_5G == -6) swing = 0xAA; // -6 dB
			else if (bbSwing_5G == -9) swing = 0xFF; // -9 dB
			else swing = 0x00;
		}

		if (RFPath == RF_PATH_A)
			onePathSwing = (swing & 0x3) >> 0; // 0xC6/C7[1:0]
		else if(RFPath == RF_PATH_B)		
			onePathSwing = (swing & 0xC) >> 2; // 0xC6/C7[3:2]
		else if(RFPath == RF_PATH_C)
			onePathSwing = (swing & 0x30) >> 4; // 0xC6/C7[5:4]
		else if(RFPath == RF_PATH_D)
			onePathSwing = (swing & 0xC0) >> 6; // 0xC6/C7[7:6]

		if (onePathSwing == 0x0) 
		{
			if (Band == BAND_ON_2_4G) 
				pRFCalibrateInfo->bb_swing_diff_2g = 0;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = 0;
			out = 0x200; // 0 dB
		} 
		else if (onePathSwing == 0x1) 
		{
			if (Band == BAND_ON_2_4G) 
				pRFCalibrateInfo->bb_swing_diff_2g = -3;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = -3;
			out = 0x16A; // -3 dB
		} 
		else if (onePathSwing == 0x2) 
		{
			if (Band == BAND_ON_2_4G) 
				pRFCalibrateInfo->bb_swing_diff_2g = -6;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = -6;
			out = 0x101; // -6 dB
		} 
		else if (onePathSwing == 0x3) 
		{
			if (Band == BAND_ON_2_4G) 
				pRFCalibrateInfo->bb_swing_diff_2g = -9;
			else
				pRFCalibrateInfo->bb_swing_diff_5g = -9;
			out = 0x0B6; // -9 dB
		}
	}
	RTW_DBG("<=== PHY_GetTxBBSwing_8814A, out = 0x%X\n", out);
	return out;
}


//1 7. BandWidth setting API

VOID
phy_SetBwRegAdc_8814A(
	IN	PADAPTER		Adapter,
	IN	u8			Band,
	IN	enum channel_width 	CurrentBW
)	
{
	switch(CurrentBW)
	{
		case CHANNEL_WIDTH_20:
			if(Band == BAND_ON_5G)
			{
				phy_set_bb_reg(Adapter, rRFMOD_Jaguar, BIT(1)|BIT(0), 0x0); 	// 0x8ac[28, 21,20,16, 9:6,1,0]=10'b10_0011_0000
			}
			else
			{
				phy_set_bb_reg(Adapter, rRFMOD_Jaguar, BIT(1)|BIT(0), 0x0); 	// 0x8ac[28, 21,20,16, 9:6,1,0]=10'b10_0101_0000
			}
			break;

		case CHANNEL_WIDTH_40:
			if(Band == BAND_ON_5G)
			{
				phy_set_bb_reg(Adapter, rRFMOD_Jaguar, BIT(1)|BIT(0), 0x1);		// 0x8ac[17, 11, 10, 7:6,1,0]=7'b100_0001
			}
			else
			{
				phy_set_bb_reg(Adapter, rRFMOD_Jaguar, BIT(1)|BIT(0), 0x1);		// 0x8ac[17, 11, 10, 7:6,1,0]=7'b101_0001
			}
			break;

		case CHANNEL_WIDTH_80:
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, BIT(1)|BIT(0), 0x02);				// 0x8ac[7:6,1,0]=4'b0010
			break;

		default:
			RT_DISP(FPHY, PHY_BBW, ("phy_SetBwRegAdc_8814A():	unknown Bandwidth: %#X\n",CurrentBW));
			break;
	}
}


VOID
phy_SetBwRegAgc_8814A(
	IN	PADAPTER		Adapter,
	IN	u8			Band,
	IN	enum channel_width 	CurrentBW
)	
{
	u32 AgcValue = 7;
	switch(CurrentBW)
	{
		case CHANNEL_WIDTH_20:
			if(Band == BAND_ON_5G)
				AgcValue = 6;
			else
				AgcValue = 6;
			break;

		case CHANNEL_WIDTH_40:
			if(Band == BAND_ON_5G)
				AgcValue = 8;
			else
				AgcValue = 7;
			break;

		case CHANNEL_WIDTH_80:
			AgcValue = 3;
			break;

		default:
			RT_DISP(FPHY, PHY_BBW, ("phy_SetBwRegAgc_8814A():	unknown Bandwidth: %#X\n",CurrentBW));
			break;
	}

	phy_set_bb_reg(Adapter, rAGC_table_Jaguar, 0xf000, AgcValue);	// 0x82C[15:12] = AgcValue				
}


BOOLEAN
phy_SwBand8814A(
	IN	PADAPTER	pAdapter,
	IN	u8			channelToSW)
{
	u8			u1Btmp; 
	BOOLEAN		ret_value = _TRUE;
	u8			Band = BAND_ON_5G, BandToSW;

	u1Btmp = rtw_read8(pAdapter, REG_CCK_CHECK_8814A);
	if(u1Btmp & BIT7)
		Band = BAND_ON_5G;
	else
		Band = BAND_ON_2_4G;

	// Use current channel to judge Band Type and switch Band if need.
	if(channelToSW > 14)
	{
		BandToSW = BAND_ON_5G;
	}
	else
	{
		BandToSW = BAND_ON_2_4G;
	}

	if(BandToSW != Band)
	{
		PHY_SwitchWirelessBand8814A(pAdapter,BandToSW);
	}
		
	return ret_value;
}


VOID
PHY_SetRFEReg8814A(
	IN PADAPTER		Adapter,
	IN BOOLEAN		bInit,
	IN u8		Band
)
{
	u8			u1tmp = 0;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if(bInit)
	{	
		switch(pHalData->rfe_type){
		case 2:case 1:
			phy_set_bb_reg(Adapter, 0x1994, 0xf, 0xf);								// 0x1994[3:0] = 0xf
			u1tmp = PlatformEFIORead1Byte(Adapter, REG_GPIO_IO_SEL_8814A);
			rtw_write8(Adapter, REG_GPIO_IO_SEL_8814A, u1tmp | 0xf0);	// 0x40[23:20] = 0xf
			break;
		case 0:
			phy_set_bb_reg(Adapter, 0x1994, 0xf, 0xf);								// 0x1994[3:0] = 0xf
			u1tmp = PlatformEFIORead1Byte(Adapter, REG_GPIO_IO_SEL_8814A);
			rtw_write8(Adapter, REG_GPIO_IO_SEL_8814A, u1tmp | 0xc0);	// 0x40[23:22] = 2b'11
			break;
		}
	}
	else if(Band == BAND_ON_2_4G)
	{
		switch(pHalData->rfe_type){
			case 2:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x72707270);	// 0xCB0 = 0x72707270
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x72707270);	// 0xEB0 = 0x72707270
			phy_set_bb_reg(Adapter, rC_RFE_Pinmux_Jaguar, bMaskDWord, 0x72707270);	// 0x18B4 = 0x72707270
			phy_set_bb_reg(Adapter, rD_RFE_Pinmux_Jaguar, bMaskDWord, 0x77707770);	// 0x1AB4 = 0x77707770
			phy_set_bb_reg(Adapter, 0x1ABC, 0x0ff00000, 0x72);						// 0x1ABC[27:20] = 0x72
			break;

		case 1:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0xCB0 = 0x77777777
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0xEB0 = 0x77777777
			phy_set_bb_reg(Adapter, rC_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0x18B4 = 0x77777777
			phy_set_bb_reg(Adapter, rD_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0x1AB4 = 0x77777777
			phy_set_bb_reg(Adapter, 0x1ABC, 0x0ff00000, 0x77);						// 0x1ABC[27:20] = 0x77
			break;

		case 0:
		default:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0xCB0 = 0x77777777
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0xEB0 = 0x77777777
			phy_set_bb_reg(Adapter, rC_RFE_Pinmux_Jaguar, bMaskDWord, 0x77777777);	// 0x18B4 = 0x77777777
			phy_set_bb_reg(Adapter, 0x1ABC, 0x0ff00000, 0x77);						// 0x1ABC[27:20] = 0x77
			break;
			
		}
	}
	else
	{
		switch(pHalData->rfe_type){
		case 2:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x33173717);	// 0xCB0 = 0x33173717
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x33173717);	// 0xEB0 = 0x33173717
			phy_set_bb_reg(Adapter, rC_RFE_Pinmux_Jaguar, bMaskDWord, 0x33173717);	// 0x18B4 = 0x33173717
			phy_set_bb_reg(Adapter, rD_RFE_Pinmux_Jaguar, bMaskDWord, 0x77177717);	// 0x1AB4 = 0x77177717
			phy_set_bb_reg(Adapter, 0x1ABC, 0x0ff00000, 0x37);						// 0x1ABC[27:20] = 0x37
			break;
			
		case 1:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x33173317);	// 0xCB0 = 0x33173317
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x33173317);	// 0xEB0 = 0x33173317
			phy_set_bb_reg(Adapter, rC_RFE_Pinmux_Jaguar, bMaskDWord, 0x33173317);	// 0x18B4 = 0x33173317
			phy_set_bb_reg(Adapter, rD_RFE_Pinmux_Jaguar, bMaskDWord, 0x77177717);	// 0x1AB4 = 0x77177717
			phy_set_bb_reg(Adapter, 0x1ABC, 0x0ff00000, 0x33);						// 0x1ABC[27:20] = 0x33
			break;

		case 0:
		default:
			phy_set_bb_reg(Adapter, rA_RFE_Pinmux_Jaguar, bMaskDWord, 0x54775477);	// 0xCB0 = 0x54775477
			phy_set_bb_reg(Adapter, rB_RFE_Pinmux_Jaguar, bMaskDWord, 0x54775477);	// 0xEB0 = 0x54775477
			phy_set_bb_reg(Adapter, rC_RFE_Pinmux_Jaguar, bMaskDWord, 0x54775477);	// 0x18B4 = 0x54775477
			phy_set_bb_reg(Adapter, rD_RFE_Pinmux_Jaguar, bMaskDWord, 0x54775477);	// 0x1AB4 = 0x54775477
			phy_set_bb_reg(Adapter, 0x1ABC, 0x0ff00000, 0x54);						// 0x1ABC[27:20] = 0x54
			break;
		}
	}
}

VOID 
phy_SetBBSwingByBand_8814A(
	IN PADAPTER		Adapter,
	IN u8		Band,
	IN u8		PreviousBand
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	s8 			BBDiffBetweenBand = 0; 		
	struct dm_struct		*		pDM_Odm = &pHalData->odmpriv;
	struct dm_rf_calibration_struct	*  	pRFCalibrateInfo = &(pDM_Odm->rf_calibrate_info);
	
	phy_set_bb_reg(Adapter, rA_TxScale_Jaguar, 0xFFE00000, 
				 PHY_GetTxBBSwing_8814A(Adapter, (BAND_TYPE)Band, RF_PATH_A)); // 0xC1C[31:21]
	phy_set_bb_reg(Adapter, rB_TxScale_Jaguar, 0xFFE00000, 
				 PHY_GetTxBBSwing_8814A(Adapter, (BAND_TYPE)Band, RF_PATH_B)); // 0xE1C[31:21]
	phy_set_bb_reg(Adapter, rC_TxScale_Jaguar2, 0xFFE00000, 
				 PHY_GetTxBBSwing_8814A(Adapter, (BAND_TYPE)Band, RF_PATH_C)); // 0x181C[31:21]
	phy_set_bb_reg(Adapter, rD_TxScale_Jaguar2, 0xFFE00000, 
				 PHY_GetTxBBSwing_8814A(Adapter, (BAND_TYPE)Band, RF_PATH_D)); // 0x1A1C[31:21]
				 
	// <20121005, Kordan> When TxPowerTrack is ON, we should take care of the change of BB swing.
	// That is, reset all info to trigger Tx power tracking.
		
	if (Band != PreviousBand) 
	{
		BBDiffBetweenBand = (pRFCalibrateInfo->bb_swing_diff_2g - pRFCalibrateInfo->bb_swing_diff_5g);
		BBDiffBetweenBand = (Band == BAND_ON_2_4G) ? BBDiffBetweenBand : (-1 * BBDiffBetweenBand);
		pRFCalibrateInfo->default_ofdm_index += BBDiffBetweenBand*2;				
	}
	
	odm_clear_txpowertracking_state(pDM_Odm);
}


s32
PHY_SwitchWirelessBand8814A(
	IN PADAPTER		Adapter,
	IN u8		Band
)
{
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);
	u8	PreBand = pHalData->current_band_type, tepReg = 0;
			
	RTW_INFO("==>PHY_SwitchWirelessBand8814() %s\n", ((Band==0)?"2.4G":"5G"));

	pHalData->current_band_type =(BAND_TYPE)Band;

	/*clear 0x1000[16],	When this bit is set to 0, CCK and OFDM are disabled, and clock are gated. Otherwise, CCK and OFDM are enabled. */
	tepReg = rtw_read8(Adapter, REG_SYS_CFG3_8814A+2);
	rtw_write8(Adapter, REG_SYS_CFG3_8814A+2, tepReg & (~BIT0));
	
	// STOP Tx/Rx
	//phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar|bCCKEN_Jaguar, 0x00);	

	if(Band == BAND_ON_2_4G)
	{// 2.4G band

		// AGC table select 
		phy_set_bb_reg(Adapter, rAGC_table_Jaguar2, 0x1F, 0);									// 0x958[4:0] = 5b'00000

		PHY_SetRFEReg8814A(Adapter, FALSE, Band);

		// cck_enable
		//phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar|bCCKEN_Jaguar, 0x3);

		if(Adapter->registrypriv.mp_mode == 0)
		{
			// 0x80C & 0xa04 should use same antenna.
			phy_set_bb_reg(Adapter, rTxPath_Jaguar, 0xf0, 0x2);
			phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0x0f000000, 0x5);
		}

		phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar|bCCKEN_Jaguar, 0x3);

		
		// CCK_CHECK_en
		rtw_write8(Adapter, REG_CCK_CHECK_8814A, 0x0);
		/* after 5G swicth 2G , set A82[2] = 0 */
		phy_set_bb_reg(Adapter, 0xa80, BIT18, 0x0);
			
	}
	else	//5G band
	{		
		// CCK_CHECK_en
		rtw_write8(Adapter, REG_CCK_CHECK_8814A, 0x80);
		/* Enable CCK Tx function, even when CCK is off */
		phy_set_bb_reg(Adapter, 0xa80, BIT18, 0x1);
	
		// AGC table select 
		// Postpone to channel switch
		//phy_set_bb_reg(Adapter, rAGC_table_Jaguar2, 0x1F, 1);									// 0x958[4:0] = 5b'00001

		PHY_SetRFEReg8814A(Adapter, FALSE, Band);

		if(Adapter->registrypriv.mp_mode == 0)
		{
			phy_set_bb_reg(Adapter, rTxPath_Jaguar, 0xf0, 0x0);
			phy_set_bb_reg(Adapter, rCCK_RX_Jaguar, 0x0f000000, 0xF);
		}

		phy_set_bb_reg(Adapter, rOFDMCCKEN_Jaguar, bOFDMEN_Jaguar|bCCKEN_Jaguar, 0x02);
		//RTW_INFO("==>PHY_SwitchWirelessBand8814() BAND_ON_5G settings OFDM index 0x%x\n", pHalData->OFDM_index[0]);
	}

	phy_SetBBSwingByBand_8814A(Adapter, Band, PreBand);
	phy_SetBwRegAdc_8814A(Adapter, Band, pHalData->current_channel_bw);
	phy_SetBwRegAgc_8814A(Adapter, Band, pHalData->current_channel_bw);
	/* set 0x1000[16], When this bit is set to 0, CCK and OFDM are disabled, and clock are gated. Otherwise, CCK and OFDM are enabled.*/
	tepReg = rtw_read8(Adapter, REG_SYS_CFG3_8814A+2);
	rtw_write8(Adapter, REG_SYS_CFG3_8814A+2, tepReg | BIT0);
	
	RTW_INFO("<==PHY_SwitchWirelessBand8814():Switch Band OK.\n");
	return _SUCCESS;	
}


u8 
phy_GetSecondaryChnl_8814A(
	IN	PADAPTER	Adapter
)
{
	u8						SCSettingOf40 = 0, SCSettingOf20 = 0;
	PHAL_DATA_TYPE				pHalData = GET_HAL_DATA(Adapter);

	//RTW_INFO("SCMapping: Case: pHalData->current_channel_bw %d, pHalData->nCur80MhzPrimeSC %d, pHalData->nCur40MhzPrimeSC %d \n",pHalData->current_channel_bw,pHalData->nCur80MhzPrimeSC,pHalData->nCur40MhzPrimeSC);
	if(pHalData->current_channel_bw== CHANNEL_WIDTH_80)
	{
		if(pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
			SCSettingOf40 = VHT_DATA_SC_40_LOWER_OF_80MHZ;
		else if(pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
			SCSettingOf40 = VHT_DATA_SC_40_UPPER_OF_80MHZ;
		else
			RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
		
		if((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER))
			SCSettingOf20 = VHT_DATA_SC_20_LOWEST_OF_80MHZ;
		else if((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER))
			SCSettingOf20 = VHT_DATA_SC_20_LOWER_OF_80MHZ;
		else if((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER))
			SCSettingOf20 = VHT_DATA_SC_20_UPPER_OF_80MHZ;
		else if((pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER) && (pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER))
			SCSettingOf20 = VHT_DATA_SC_20_UPPERST_OF_80MHZ;
		else
		{
			if(pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
				SCSettingOf20 = VHT_DATA_SC_40_LOWER_OF_80MHZ;
			else if(pHalData->nCur80MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
				SCSettingOf20 = VHT_DATA_SC_40_UPPER_OF_80MHZ;
			else
				RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
		}	
	}
	else if(pHalData->current_channel_bw == CHANNEL_WIDTH_40)
	{
		RTW_INFO("SCMapping: pHalData->current_channel_bw %d, pHalData->nCur40MhzPrimeSC %d \n",pHalData->current_channel_bw,pHalData->nCur40MhzPrimeSC);

		if(pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
			SCSettingOf20 = VHT_DATA_SC_20_UPPER_OF_80MHZ;
		else if(pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
			SCSettingOf20 = VHT_DATA_SC_20_LOWER_OF_80MHZ;
		else
			RTW_INFO("SCMapping: DONOT CARE Mode Setting\n");
	}

	/*RTW_INFO("SCMapping: SC Value %x\n", ((SCSettingOf40 << 4) | SCSettingOf20));*/
	return  ( (SCSettingOf40 << 4) | SCSettingOf20);
}


VOID
phy_SetBwRegMac_8814A(
	IN	PADAPTER		Adapter,
	enum channel_width 	CurrentBW
)	
{
	u16		RegRfMod_BW, u2tmp = 0;
	RegRfMod_BW = PlatformEFIORead2Byte(Adapter, REG_TRXPTCL_CTL_8814A);

	switch(CurrentBW)
	{
		case CHANNEL_WIDTH_20:
			PlatformEFIOWrite2Byte(Adapter, REG_TRXPTCL_CTL_8814A, (RegRfMod_BW & 0xFE7F)); // BIT 7 = 0, BIT 8 = 0
			break;

		case CHANNEL_WIDTH_40:
			u2tmp = RegRfMod_BW | BIT7;
			PlatformEFIOWrite2Byte(Adapter, REG_TRXPTCL_CTL_8814A, (u2tmp & 0xFEFF)); // BIT 7 = 1, BIT 8 = 0
			break;

		case CHANNEL_WIDTH_80:
			u2tmp = RegRfMod_BW | BIT8;
			PlatformEFIOWrite2Byte(Adapter, REG_TRXPTCL_CTL_8814A, (u2tmp & 0xFF7F)); // BIT 7 = 0, BIT 8 = 1
			break;

		default:
			RT_DISP(FPHY, PHY_BBW, ("phy_SetBwRegMac_8814A():	unknown Bandwidth: %#X\n",CurrentBW));
			break;
	}
}

void PHY_Set_SecCCATH_by_RXANT_8814A(PADAPTER	pAdapter,u4Byte	ulAntennaRx)
{
	PHAL_DATA_TYPE		pHalData	= GET_HAL_DATA(pAdapter);

	if ((pHalData->bSWToBW40M == TRUE) && (pHalData->current_channel_bw != CHANNEL_WIDTH_40)) {
		phy_set_bb_reg(pAdapter, rPwed_TH_Jaguar, 0x007c0000,pHalData->BackUp_BB_REG_4_2nd_CCA[0]);
		phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0000ff00,pHalData->BackUp_BB_REG_4_2nd_CCA[1]);
		phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,pHalData->BackUp_BB_REG_4_2nd_CCA[2]);
		pHalData->bSWToBW40M = FALSE;
	}

	if ((pHalData->bSWToBW80M == TRUE) && (pHalData->current_channel_bw != CHANNEL_WIDTH_80)) {
		phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000, pHalData->BackUp_BB_REG_4_2nd_CCA[2]);
		pHalData->bSWToBW80M = FALSE;
	}

	/*1 Setting CCA TH 2nd CCA parameter by Rx Antenna*/
	if (pHalData->current_channel_bw == CHANNEL_WIDTH_80) {
		if (pHalData->bSWToBW80M == FALSE) {
			pHalData->BackUp_BB_REG_4_2nd_CCA[2] = phy_query_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000);
		}

		pHalData->bSWToBW80M = TRUE;

		switch (ulAntennaRx) {
		case ANTENNA_A:
		case ANTENNA_B:
		case ANTENNA_C:
		case ANTENNA_D:
				phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0b);/* 0x844[27:24] = 0xb */
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1); /* 0x838 Enable 2ndCCA */
				phy_set_bb_reg(pAdapter, rAGC_table_Jaguar, 0x00FF0000, 0x89); /* 0x82C[23:20] = 8, PWDB_TH_QB, 0x82C[19:16] = 9, PWDB_TH_HB*/
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0FFF0000, 0x887); /* 838[27:24]=8, RF80_secondary40, 838[23:20]=8, RF80_secondary20, 838[19:16]=7, RF80_primary*/
				phy_set_bb_reg(pAdapter, rL1_Weight_Jaguar, 0x0000F000, 0x7);	/* 840[15:12]=7, L1_square_Pk_weight_80M*/
		break;

		case ANTENNA_AB:
		case ANTENNA_AC:
		case ANTENNA_AD:	
		case ANTENNA_BC:
		case ANTENNA_BD:
		case ANTENNA_CD:
				phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0d);
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1); /* Enable 2ndCCA*/
				phy_set_bb_reg(pAdapter, rAGC_table_Jaguar, 0x00FF0000, 0x78); /* 0x82C[23:20] = 7, PWDB_TH_QB, 0x82C[19:16] = 8, PWDB_TH_HB*/
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0FFF0000, 0x444); /* 838[27:24]=4, RF80_secondary40, 838[23:20]=4, RF80_secondary20, 838[19:16]=4, RF80_primary*/
				phy_set_bb_reg(pAdapter, rL1_Weight_Jaguar, 0x0000F000, 0x6); /* 840[15:12]=6, L1_square_Pk_weight_80M*/
		break;

		case ANTENNA_ABC:
		case ANTENNA_ABD:
		case ANTENNA_ACD:
		case ANTENNA_BCD:
				phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0d);
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1); /* Enable 2ndCCA*/
				phy_set_bb_reg(pAdapter, rAGC_table_Jaguar, 0x00FF0000, 0x98); /* 0x82C[23:20] = 9, PWDB_TH_QB, 0x82C[19:16] = 8, PWDB_TH_HB*/
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0FFF0000, 0x666); /* 838[27:24]=6, RF80_secondary40, 838[23:20]=6, RF80_secondary20, 838[19:16]=6, RF80_primary*/
				phy_set_bb_reg(pAdapter, rL1_Weight_Jaguar, 0x0000F000, 0x6); /* 840[15:12]=6, L1_square_Pk_weight_80M*/
		break;

		case ANTENNA_ABCD:
				phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0d);
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1); /*Enable 2ndCCA*/
				phy_set_bb_reg(pAdapter, rAGC_table_Jaguar, 0x00FF0000, 0x98); /* 0x82C[23:20] = 9, PWDB_TH_QB, 0x82C[19:16] = 8, PWDB_TH_HB*/
				phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0FFF0000, 0x666); /* 838[27:24]=6, RF80_secondary40, 838[23:20]=6, RF80_secondary20, 838[19:16]=6, RF80_primary*/
				phy_set_bb_reg(pAdapter, rL1_Weight_Jaguar, 0x0000F000, 0x7); /*840[15:12]=7, L1_square_Pk_weight_80M*/
		break;

		default:
				RTW_INFO("Unknown Rx antenna.\n");
		break;
		}
	} else if(pHalData->current_channel_bw == CHANNEL_WIDTH_40) {
		if (pHalData->bSWToBW40M == FALSE) {
			pHalData->BackUp_BB_REG_4_2nd_CCA[0] = phy_query_bb_reg(pAdapter, rPwed_TH_Jaguar, 0x007c0000);
			pHalData->BackUp_BB_REG_4_2nd_CCA[1] = phy_query_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0000ff00);
			pHalData->BackUp_BB_REG_4_2nd_CCA[2] = phy_query_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000);
		}

		switch (ulAntennaRx) {
		case ANTENNA_A:  /* xT1R*/
		case ANTENNA_B:
		case ANTENNA_C:
		case ANTENNA_D:
						phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0b);
						phy_set_bb_reg(pAdapter, rPwed_TH_Jaguar, 0x007c0000, 0xe);
						phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0000ff00, 0x43);
						phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1);
						break;
		case ANTENNA_AB: /* xT2R*/
		case ANTENNA_AC:
		case ANTENNA_AD:
		case ANTENNA_BC:
		case ANTENNA_BD:
		case ANTENNA_CD:
						phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0d);
						phy_set_bb_reg(pAdapter, rPwed_TH_Jaguar, 0x007c0000, 0x8);
						phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0000ff00, 0x43);
						phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1);
						break;
		case ANTENNA_ABC: /* xT3R*/
		case ANTENNA_ABD:
		case ANTENNA_ACD:
		case ANTENNA_BCD:
		case ANTENNA_ABCD:  /* xT4R*/
						phy_set_bb_reg(pAdapter, r_L1_SBD_start_time, 0x0f000000,0x0d);
						phy_set_bb_reg(pAdapter, rPwed_TH_Jaguar, 0x007c0000, 0xa);
						phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0000ff00, 0x43);
						phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x1);
						break;
		default:
						break;
		}
		pHalData->bSWToBW40M = TRUE;
	} else {
		phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x00000001, 0x0); /* Enable 2ndCCA*/
		phy_set_bb_reg(pAdapter, rAGC_table_Jaguar, 0x00FF0000, 0x43); /* 0x82C[23:20] = 9, PWDB_TH_QB, 0x82C[19:16] = 8, PWDB_TH_HB*/
		phy_set_bb_reg(pAdapter, rCCAonSec_Jaguar, 0x0FFF0000, 0x7aa); /* 838[27:24]=6, RF80_secondary40, 838[23:20]=6, RF80_secondary20, 838[19:16]=6, RF80_primary*/
		phy_set_bb_reg(pAdapter, rL1_Weight_Jaguar, 0x0000F000, 0x7); /* 840[15:12]=7, L1_square_Pk_weight_80M*/
	}
	
}


VOID PHY_SetRXSC_by_TXSC_8814A(PADAPTER	Adapter, u1Byte SubChnlNum)	
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);

	if (pHalData->current_channel_bw == CHANNEL_WIDTH_80) {
		if (SubChnlNum == 0)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x1);
		else if (SubChnlNum == 1)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x1);
		else if (SubChnlNum == 2)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x2);
		else if (SubChnlNum == 4)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x4);
		else if (SubChnlNum == 3)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x3);
		else if (SubChnlNum == 9)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x1);
		else if (SubChnlNum == 10)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x2);
	} else if (pHalData->current_channel_bw == CHANNEL_WIDTH_40) { 
		if (SubChnlNum == 1)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x1);
		else if (SubChnlNum == 2)
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x2);
	} else
		phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x00000003c, 0x0);
}


/* <20141230, James> A workaround to eliminate the 5280MHz & 5600MHz & 5760MHzspur of 8814A. (Asked by BBSD Neil.)*/
VOID phy_SpurCalibration_8814A(PADAPTER	Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	BOOLEAN		Reset_NBI_CSI = TRUE;
	struct dm_struct		*		pDM_Odm = &pHalData->odmpriv;

	/*RTW_INFO("%s(),RFE Type =%d, CurrentCh = %d ,ChannelBW =%d\n", __func__, pHalData->rfe_type, pHalData->current_channel, pHalData->current_channel_bw);*/
	/*RTW_INFO("%s(),Before RrNBI_Setting_Jaguar= %x\n", __func__, phy_query_bb_reg(Adapter, rNBI_Setting_Jaguar, bMaskDWord));*/
	
	if (pHalData->rfe_type == 0) {
		switch (pHalData->current_channel_bw) {
		case CHANNEL_WIDTH_40:
				if (pHalData->current_channel == 54 || pHalData->current_channel == 118) {
					phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x3e >> 1);
					phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, BIT(0), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, bMaskDWord, 0);
					Reset_NBI_CSI = FALSE;
				} else if (pHalData->current_channel == 151) {
					phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x1e >> 1);
					phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar,  BIT(16), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, bMaskDWord, 0);
					Reset_NBI_CSI = FALSE;
				}
		break;

		case CHANNEL_WIDTH_80:
				if (pHalData->current_channel == 58 || pHalData->current_channel == 122) {
					phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x3a >> 1);
					phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, BIT(0), 1);
					Reset_NBI_CSI = FALSE;
				} else if (pHalData->current_channel == 155) {
					phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x5a >> 1);
					phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, BIT(16), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, bMaskDWord, 0);
					Reset_NBI_CSI = FALSE;
				}
		break;
		case CHANNEL_WIDTH_20:
				if (pHalData->current_channel == 153) {
					phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x1e >> 1);
					phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
					phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, BIT(16), 1);
					Reset_NBI_CSI = FALSE;
					}
		break;
			
		default:
		break;
		}
	} else if (pHalData->rfe_type == 1 || pHalData->rfe_type == 2) {
			switch (pHalData->current_channel_bw) {
			case CHANNEL_WIDTH_20:
					if (pHalData->current_channel == 153) {
						phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x1E >> 1);
						phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, BIT(16), 1);
						Reset_NBI_CSI = FALSE;
					}
			break;
			case CHANNEL_WIDTH_40:
					if (pHalData->current_channel == 151) {
						phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x1e >> 1);
						phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, BIT(16), 1);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, bMaskDWord, 0);
						Reset_NBI_CSI = FALSE;
					}
			break;
			case CHANNEL_WIDTH_80:
					if (pHalData->current_channel == 155) {
						phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0x5a >> 1);
						phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 1);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, BIT(16), 1);
						phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, bMaskDWord, 0);
						Reset_NBI_CSI = FALSE;
					}
			break;

			default:
			break;
			}
	}
	
	if (Reset_NBI_CSI) {
		phy_set_bb_reg(Adapter, rNBI_Setting_Jaguar, 0x000fe000, 0xfc >> 1);
		phy_set_bb_reg(Adapter, rCSI_Mask_Setting1_Jaguar, BIT(0), 0);
		phy_set_bb_reg(Adapter, rCSI_Fix_Mask0_Jaguar, bMaskDWord, 0);
		phy_set_bb_reg(Adapter, rCSI_Fix_Mask1_Jaguar, bMaskDWord, 0);
		phy_set_bb_reg(Adapter, rCSI_Fix_Mask6_Jaguar, bMaskDWord, 0);
		phy_set_bb_reg(Adapter, rCSI_Fix_Mask7_Jaguar, bMaskDWord, 0);
	}
	
	phydm_spur_nbi_setting_8814a(pDM_Odm);
	/*RTW_INFO("%s(),After RrNBI_Setting_Jaguar= %x\n", __func__, phy_query_bb_reg(Adapter, rNBI_Setting_Jaguar, bMaskDWord));*/
}


void phy_ModifyInitialGain_8814A(
	PADAPTER		Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8 			channel = pHalData->current_channel;
	s1Byte		offset[4]; /*{A,B,C,D}*/
	u8			i = 0;
	u8			chnl_section = 0xff;

	if (channel <= 14 && channel > 0)
		chnl_section = 0; /*2G*/
	else if (channel <= 64 && channel >= 36)
		chnl_section = 1; /*5GL*/
	else if (channel <= 144 && channel >= 100)
		chnl_section = 2; /*5GM*/
	else if (channel <= 177 && channel >= 149)
		chnl_section = 3; /*5GH*/

	if (chnl_section > 3) {
		RTW_INFO("%s: worng channel section\n", __func__);
		return;
	}

	for (i = 0; i < 4; i++) {
		u1Byte	hex_offset;

		hex_offset = (u1Byte)(pHalData->RxGainOffset[chnl_section] >> (12-4*i))&0x0f;
		RTW_INFO("%s: pHalData->RxGainOffset[%d] = %x\n", __func__, chnl_section, pHalData->RxGainOffset[chnl_section]);
		RTW_INFO("%s: hex_offset = %x\n", __func__, hex_offset);

		if (hex_offset == 0xf)
			offset[i] = 0;
		else if (hex_offset >= 0x8)
			offset[i] = 0x11 - hex_offset;
		else
		 	offset[i] = 0x0 - hex_offset;
		 offset[i] = (offset[i] / 2) * 2;
		 RTW_INFO("%s: offset[%d] = %x\n", __func__, i, offset[i]);
		 RTW_INFO("%s: BackUp_IG_REG_4_Chnl_Section[%d] = %x\n", __func__, i, pHalData->BackUp_IG_REG_4_Chnl_Section[i]);
	}

	if (pHalData->BackUp_IG_REG_4_Chnl_Section[0] != 0 &&
		pHalData->BackUp_IG_REG_4_Chnl_Section[1] != 0 &&
		pHalData->BackUp_IG_REG_4_Chnl_Section[2] != 0 &&
		pHalData->BackUp_IG_REG_4_Chnl_Section[3] != 0
		) {
		phy_set_bb_reg(Adapter, rA_IGI_Jaguar, 0x000000ff, pHalData->BackUp_IG_REG_4_Chnl_Section[0] + offset[0]);
		phy_set_bb_reg(Adapter, rB_IGI_Jaguar, 0x000000ff, pHalData->BackUp_IG_REG_4_Chnl_Section[1] + offset[1]);
		phy_set_bb_reg(Adapter, rC_IGI_Jaguar2, 0x000000ff, pHalData->BackUp_IG_REG_4_Chnl_Section[2] + offset[2]);
		phy_set_bb_reg(Adapter, rD_IGI_Jaguar2, 0x000000ff, pHalData->BackUp_IG_REG_4_Chnl_Section[3] + offset[3]);
	}
}


VOID phy_SetBwMode8814A(PADAPTER	Adapter)
{
	u8			SubChnlNum = 0;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	//3 Set Reg668 BW
	phy_SetBwRegMac_8814A(Adapter, pHalData->current_channel_bw);

	//3 Set Reg483
	SubChnlNum = phy_GetSecondaryChnl_8814A(Adapter);
	rtw_write8(Adapter, REG_DATA_SC_8814A, SubChnlNum);

	if(pHalData->rf_chip == RF_PSEUDO_11N)
	{
		RTW_INFO("phy_SetBwMode8814A: return for PSEUDO \n");
		return;
	}

	//3 Set Reg8AC Reg8C4 Reg8C8
	phy_SetBwRegAdc_8814A(Adapter, pHalData->current_band_type, pHalData->current_channel_bw);
	//3 Set Reg82C
	phy_SetBwRegAgc_8814A(Adapter, pHalData->current_band_type, pHalData->current_channel_bw);

	//3 Set Reg848  RegA00
	switch(pHalData->current_channel_bw)
	{
		case CHANNEL_WIDTH_20:
			break;

		case CHANNEL_WIDTH_40:
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x3C, SubChnlNum);			// 0x8ac[5:2]=1/2

			if(SubChnlNum == VHT_DATA_SC_20_UPPER_OF_80MHZ)					// 0xa00[4]=1/0
				phy_set_bb_reg(Adapter, rCCK_System_Jaguar, bCCK_System_Jaguar, 1);
			else
				phy_set_bb_reg(Adapter, rCCK_System_Jaguar, bCCK_System_Jaguar, 0);
			break;

		case CHANNEL_WIDTH_80:
			phy_set_bb_reg(Adapter, rRFMOD_Jaguar, 0x3C, SubChnlNum);			// 0x8ac[5:2]=1/2/3/4/9/10
			break;

		default:
			RTW_INFO("%s():unknown Bandwidth:%#X\n", __func__, pHalData->current_channel_bw);
			break;
	}
	
#if (MP_DRIVER == 1)
if (Adapter->registrypriv.mp_mode == 1) {
	/* 2 Set Reg 0x8AC */
	PHY_SetRXSC_by_TXSC_8814A(Adapter, (SubChnlNum & 0xf));
	PHY_Set_SecCCATH_by_RXANT_8814A(Adapter, pHalData->AntennaRxPath);
}
#endif	
	/* 3 Set RF related register */
	PHY_RF6052SetBandwidth8814A(Adapter, pHalData->current_channel_bw);

	phy_ADC_CLK_8814A(Adapter);
	phy_SpurCalibration_8814A(Adapter);
}



//1 6. Channel setting API

// <YuChen, 140529> Add for KFree Feature Requested by RF David.
// We need support ABCD four path Kfree
#if 0 /* no equivalent in 5.2.20... maybe not needed */
VOID
phy_SetKfreeToRF_8814A(
	IN	PADAPTER			Adapter,
	IN	u8				eRFPath,
	IN	u8				Data
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(GetDefaultAdapter(Adapter));	
	struct dm_struct		*	pDM_Odm = &pHalData->odmpriv;
	BOOLEAN bOdd;
	struct dm_rf_calibration_struct	*	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);
	if((Data%2) != 0)		//odd -> positive
	{
		Data = Data - 1;
		phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT19, 1);
		bOdd = TRUE;
	}
	else		// even -> negative
	{
		phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT19, 0);
		bOdd = FALSE;
	}
	RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): RF_0x55[19]= %d\n", bOdd));
	switch(Data)
	{
		case 2:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT14, 1);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 0;
		break;
		case 4:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 1);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 1;
		break;
		case 6:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT14, 1);
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 1);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 1;
		break;
		case 8:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 2);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 2;
		break;
		case 10:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT14, 1);
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 2);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 2;
		break;
		case 12:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 3);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 3;
		break;
		case 14:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT14, 1);
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 3);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 3;
		break;
		case 16:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 4);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 4;
		break;
		case 18:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT14, 1);
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 4);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 4;
		break;
		case 20:
			phy_set_rf_reg(Adapter, eRFPath, rRF_TxGainOffset, BIT17|BIT16|BIT15, 5);
			pRFCalibrateInfo->KfreeOffset[eRFPath] = 5;
		break;

		default:
		break;
	}

	if(bOdd == FALSE)			// that means Kfree offset is negative, we need to record it.
	{
		pRFCalibrateInfo->KfreeOffset[eRFPath] = (-1)*pRFCalibrateInfo->KfreeOffset[eRFPath];
		RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): KfreeOffset = %d\n", pRFCalibrateInfo->KfreeOffset[eRFPath]));
	}
	else
		RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): KfreeOffset = %d\n", pRFCalibrateInfo->KfreeOffset[eRFPath]));
	
}


VOID
phy_ConfigKFree8814A(
	IN	PADAPTER	Adapter,
	IN	u8 		channelToSW,
	IN	BAND_TYPE	bandType
	)
{
	u8			targetval_A = 0xFF;
	u8			targetval_B = 0xFF;
	u8			targetval_C = 0xFF;
	u8			targetval_D = 0xFF;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	
	//RTW_INFO("===>phy_ConfigKFree8814A()\n");
	
	if (Adapter->registrypriv.RegPwrTrimEnable == 2)
	{
		//RTW_INFO("phy_ConfigKFree8814A(): RegPwrTrimEnable == 2, Disable \n");
		return;
	}
	else if (Adapter->registrypriv.RegPwrTrimEnable == 1 || Adapter->registrypriv.RegPwrTrimEnable == 0)
	{
		RTW_INFO("phy_ConfigKFree8814A(): RegPwrTrimEnable == TRUE \n");
		if (bandType == BAND_ON_2_4G) // 2G
		{
			RTW_INFO("phy_ConfigKFree8814A(): bandType == BAND_ON_2_4G, channelToSW= %d  \n", channelToSW);
			if (channelToSW <= 14 && channelToSW >= 1)
			{
				efuse_OneByteRead(Adapter, 0x3F4, &targetval_A, FALSE);	// for Path A and B
				efuse_OneByteRead(Adapter, 0x3F5, &targetval_B, FALSE);	// for Path C and D
			}
			
		}
		else if (bandType == BAND_ON_5G)
		{
			RTW_INFO("phy_ConfigKFree8814A(): bandType == BAND_ON_5G, channelToSW= %d  \n", channelToSW);
			if (channelToSW >= 36 && channelToSW < 50) // 5GLB_1
			{
				efuse_OneByteRead(Adapter, 0x3E0, &targetval_A, FALSE);
				efuse_OneByteRead(Adapter, 0x3E1, &targetval_B, FALSE);
				efuse_OneByteRead(Adapter, 0x3E2, &targetval_C, FALSE);
				efuse_OneByteRead(Adapter, 0x3E3, &targetval_D, FALSE);
			}
			else if (channelToSW >= 50 && channelToSW <= 64) // 5GLB_2
			{
				efuse_OneByteRead(Adapter, 0x3E4, &targetval_A, FALSE);
				efuse_OneByteRead(Adapter, 0x3E5, &targetval_B, FALSE);
				efuse_OneByteRead(Adapter, 0x3E6, &targetval_C, FALSE);
				efuse_OneByteRead(Adapter, 0x3E7, &targetval_D, FALSE);
			}
			else if (channelToSW >= 100 && channelToSW <= 118) // 5GMB_1
			{
				efuse_OneByteRead(Adapter, 0x3E8, &targetval_A, FALSE);
				efuse_OneByteRead(Adapter, 0x3E9, &targetval_B, FALSE);
				efuse_OneByteRead(Adapter, 0x3EA, &targetval_C, FALSE);
				efuse_OneByteRead(Adapter, 0x3EB, &targetval_D, FALSE);
			}
			else if (channelToSW >= 120 && channelToSW <= 140) // 5GMB_2
			{
				efuse_OneByteRead(Adapter, 0x3EC, &targetval_A, FALSE);
				efuse_OneByteRead(Adapter, 0x3ED, &targetval_B, FALSE);
				efuse_OneByteRead(Adapter, 0x3EE, &targetval_C, FALSE);
				efuse_OneByteRead(Adapter, 0x3EF, &targetval_D, FALSE);
			}
			else if (channelToSW >= 149 && channelToSW <= 165) // 5GHB
			{
				efuse_OneByteRead(Adapter, 0x3F0, &targetval_A, FALSE);
				efuse_OneByteRead(Adapter, 0x3F1, &targetval_B, FALSE);
				efuse_OneByteRead(Adapter, 0x3F2, &targetval_C, FALSE);
				efuse_OneByteRead(Adapter, 0x3F3, &targetval_D, FALSE);
			}
		}
		RTW_INFO("phy_ConfigKFree8814A(): targetval_A= %#x \n", targetval_A);
		RTW_INFO("phy_ConfigKFree8814A(): targetval_B= %#x \n", targetval_B);
		RTW_INFO("phy_ConfigKFree8814A(): targetval_C= %#x \n", targetval_C);
		RTW_INFO("phy_ConfigKFree8814A(): targetval_D= %#x \n", targetval_D);
		
		// Make sure the targetval is defined
		if ((Adapter->registrypriv.RegPwrTrimEnable == 1) && ((targetval_A != 0xFF) || (pHalData->RfKFreeEnable == TRUE)))
		{
			if (bandType == BAND_ON_2_4G) // 2G
			{
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_A: %#x \n", targetval_A&0x0F));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_A, targetval_A&0x0F);
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_B: %#x \n", (targetval_A&0xF0)>>4));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_B, (targetval_A&0xF0)>>4);
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_C: %#x \n", targetval_B&0x0F));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_C, targetval_B&0x0F);
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_D: %#x \n", (targetval_B&0xF0)>>4));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_D, (targetval_B&0xF0)>>4);
			}
			else if(bandType == BAND_ON_5G)
			{
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_A: %#x \n", targetval_A&0x1F));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_A, targetval_A&0x1F);
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_B: %#x \n", targetval_B&0x1F));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_B, targetval_B&0x1F);
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_C: %#x \n", targetval_C&0x1F));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_C, targetval_C&0x1F);
				RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): PATH_D: %#x \n", targetval_D&0x1F));
				phy_SetKfreeToRF_8814A(Adapter, RF_PATH_D, targetval_D&0x1F);
			}
		}
		else
		{
			RT_TRACE(COMP_MP, DBG_LOUD, ("phy_ConfigKFree8814A(): targetval not defined, Don't execute KFree Process.\n"));
			return;
		}
	}
	RT_TRACE(COMP_MP, DBG_LOUD, ("<===phy_ConfigKFree8814A()\n"));
}
#endif 
VOID
phy_SwChnl8814A(	
	IN	PADAPTER					pAdapter
	)
{
	u8			eRFPath = 0 , channelIdx = 0;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#ifdef CONFIG_RF_GAIN_OFFSET
	struct kfree_data_t *kfree_data = &pHalData->kfree_data;
#endif
	u8 			channelToSW = pHalData->current_channel;
	u32			RFValToWR , RFTmpVal, BitShift, BitMask;
	  
	//RTW_INFO("[BW:CHNL], phy_SwChnl8814A(), switch to channel %d !!\n", channelToSW);

	if (phy_SwBand8814A(pAdapter, channelToSW) == FALSE)
	{
		RTW_INFO("error Chnl %d", channelToSW);
	}

	if(pHalData->rf_chip == RF_PSEUDO_11N)
	{
		RTW_DBG("phy_SwChnl8814A: return for PSEUDO\n");
		return;
	}
	
#ifdef CONFIG_RF_GAIN_OFFSET
	/* <YuChen, 140529> Add for KFree Feature Requested by RF David. */
	if (kfree_data->flag & KFREE_FLAG_ON) {

		channelIdx = rtw_ch_to_bb_gain_sel(channelToSW);
	#if 0
		if (pHalData->RfKFree_ch_group != channelIdx) {
			/* Todo: wait for new phydm ready */
			phy_ConfigKFree8814A(pAdapter, channelToSW, pHalData->current_band_type);
			phydm_ConfigKFree(pDM_Odm, channelToSW, kfree_data->bb_gain);
			RTW_INFO("RfKFree_ch_group =%d\n", channelIdx);
		}
	#endif

		pHalData->RfKFree_ch_group = channelIdx;

	}
#endif
	if(pHalData->RegFWOffload == 2)
	{
		FillH2CCmd_8814(pAdapter, H2C_CHNL_SWITCH_OFFLOAD, 1, &channelToSW);
	}
	else
	{
		// fc_area		
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

		for(eRFPath = 0; eRFPath < pHalData->NumTotalRFPath; eRFPath++)
		{
			// RF_MOD_AG
			if (36 <= channelToSW && channelToSW <= 64)
				RFValToWR = 0x101;	 //5'b00101
			else if (100 <= channelToSW && channelToSW <= 140) 
				RFValToWR = 0x301; 	//5'b01101
			else if (140 < channelToSW) 
				RFValToWR = 0x501; 	//5'b10101
			else	
				RFValToWR = 0x000; 	//5'b00000

			// Channel to switch
			BitMask = BIT18|BIT17|BIT16|BIT9|BIT8;
			BitShift =  PHY_CalculateBitShift(BitMask);
			RFTmpVal = channelToSW | (RFValToWR << BitShift);

			BitMask = BIT18|BIT17|BIT16|BIT9|BIT8|bMaskByte0;

			phy_set_rf_reg(pAdapter, eRFPath, RF_CHNLBW_Jaguar, BitMask, RFTmpVal);
		}

		if (36 <= channelToSW && channelToSW <= 64)				// Band 1 & Band 2
			phy_set_bb_reg(pAdapter, rAGC_table_Jaguar2, 0x1F, 1);	// 0x958[4:0] = 0x1
		else if (100 <= channelToSW && channelToSW <= 144) 			// Band 3
			phy_set_bb_reg(pAdapter, rAGC_table_Jaguar2, 0x1F, 2);	// 0x958[4:0] = 0x2
		else	if(channelToSW >= 149)								// Band 4
			phy_set_bb_reg(pAdapter, rAGC_table_Jaguar2, 0x1F, 3);	// 0x958[4:0] = 0x3
	}

	if (pAdapter->registrypriv.mp_mode == 1) {
				if (!pHalData->bSetChnlBW)
					phy_ADC_CLK_8814A(pAdapter);
		phy_SpurCalibration_8814A(pAdapter);
		phy_ModifyInitialGain_8814A(pAdapter);
	}
	
	/* 2.4G CCK TX DFIR  */
	if (channelToSW >= 1 && channelToSW <= 11) {
		phy_set_bb_reg(pAdapter, rCCK0_TxFilter1, bMaskDWord, 0x1a1b0030);
		phy_set_bb_reg(pAdapter, rCCK0_TxFilter2, bMaskDWord, 0x090e1317);
		phy_set_bb_reg(pAdapter, rCCK0_DebugPort, bMaskDWord, 0x00000204);
	} else if (channelToSW >= 12 && channelToSW <= 13) {
		phy_set_bb_reg(pAdapter, rCCK0_TxFilter1, bMaskDWord, 0x1a1b0030);
		phy_set_bb_reg(pAdapter, rCCK0_TxFilter2, bMaskDWord, 0x090e1217);
		phy_set_bb_reg(pAdapter, rCCK0_DebugPort, bMaskDWord, 0x00000305);
	} else if (channelToSW == 14) {
		phy_set_bb_reg(pAdapter, rCCK0_TxFilter1, bMaskDWord, 0x1a1b0030);
		phy_set_bb_reg(pAdapter, rCCK0_TxFilter2, bMaskDWord, 0x00000E17);
		phy_set_bb_reg(pAdapter, rCCK0_DebugPort, bMaskDWord, 0x00000000);
	}

}

/*
VOID
PHY_SwChnlTimerCallback8814A(
	IN	PRT_TIMER		pTimer
	)
{
	PADAPTER		pAdapter = (PADAPTER)pTimer->Adapter;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	
	RT_TRACE(COMP_SCAN, DBG_LOUD, ("==>PHY_SwChnlTimerCallback8814A(), switch to channel %d\n", pHalData->current_channel));
	
	if (rtw_is_drv_stopped(padapter))
		return;
	
	if(pHalData->rf_chip == RF_PSEUDO_11N)
	{
		pHalData->SwChnlInProgress=FALSE;
		return; 								//return immediately if it is peudo-phy	
	}


	PlatformAcquireSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
	pHalData->SwChnlInProgress=TRUE;
	PlatformReleaseSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
	
	phy_SwChnl8814A(pAdapter);

	PlatformAcquireSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
	pHalData->SwChnlInProgress=FALSE;
	PlatformReleaseSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
	
	RT_TRACE(COMP_SCAN, DBG_LOUD, ("<==PHY_SwChnlTimerCallback8814()\n"));
}


VOID
PHY_SwChnlWorkItemCallback8814A(
	IN PVOID            pContext
	)
{
	PADAPTER		pAdapter = (PADAPTER)pContext;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	RT_TRACE(COMP_SCAN, DBG_LOUD, ("==>PHY_SwChnlWorkItemCallback8814A(), switch to channel %d\n", pHalData->current_channel));

	if(pAdapter->bInSetPower && RT_USB_CANNOT_IO(pAdapter))
	{
		RT_TRACE(COMP_SCAN, DBG_LOUD, ("<== PHY_SwChnlWorkItemCallback8814A() SwChnlInProgress FALSE driver sleep or unload\n"));
	
		pHalData->SwChnlInProgress = FALSE;		
		return;
	}
	
	if (rtw_is_drv_stopped(padapter))
		return;
	
	if(pHalData->rf_chip == RF_PSEUDO_11N)
	{
		pHalData->SwChnlInProgress=FALSE;
		return; 								//return immediately if it is peudo-phy	
	}

	PlatformAcquireSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
	pHalData->SwChnlInProgress=TRUE;
	PlatformReleaseSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
		
	phy_SwChnl8814A(pAdapter);

	PlatformAcquireSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);	
	pHalData->SwChnlInProgress=FALSE;
	PlatformReleaseSpinLock(pAdapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
	
	RT_TRACE(COMP_P2P, DBG_LOUD, ("PHY_SwChnlWorkItemCallback8814A(), switch to channel %d\n", pHalData->current_channel));
	RT_TRACE(COMP_SCAN, DBG_LOUD, ("<==PHY_SwChnlWorkItemCallback8814A()\n"));
}


VOID
HAL_HandleSwChnl8814A(	// Call after initialization
	IN	PADAPTER	pAdapter,
	IN	u8		channel
	)
{
	PADAPTER Adapter =  GetDefaultAdapter(pAdapter);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	RT_TRACE(COMP_SCAN | COMP_RM, DBG_LOUD, ("HAL_HandleSwChnl8814A()===>\n"));
	pHalData->current_channel = channel;
	phy_SwChnl8814A(Adapter);


#if (MP_DRIVER == 1) 
	// <20120712, Kordan> IQK on each channel, asked by James.
	PHY_IQCalibrate_8814A(pAdapter, FALSE);
#endif

	RT_TRACE(COMP_SCAN | COMP_RM, DBG_LOUD, ("<==HAL_HandleSwChnl8814A()\n"));
}
*/

VOID
phy_SwChnlAndSetBwMode8814A(
	IN  PADAPTER		Adapter
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*		pDM_Odm = &pHalData->odmpriv;

	//RTW_INFO("phy_SwChnlAndSetBwMode8814A(): bSwChnl %d, bSetChnlBW %d \n", pHalData->bSwChnl, pHalData->bSetChnlBW);
	if ( Adapter->bNotifyChannelChange )
	{
		RTW_INFO( "[%s] bSwChnl=%d, ch=%d, bSetChnlBW=%d, bw=%d\n", 
			__FUNCTION__, 
			pHalData->bSwChnl,
			pHalData->current_channel,
			pHalData->bSetChnlBW,
			pHalData->current_channel_bw);
	}
	
	if (RTW_CANNOT_RUN(Adapter)) {
		pHalData->bSwChnlAndSetBWInProgress= FALSE;
		return;
	}

	if (pHalData->bSwChnl)
	{
		phy_SwChnl8814A(Adapter);
		pHalData->bSwChnl = FALSE;
	}	

	if (pHalData->bSetChnlBW)
	{
		phy_SetBwMode8814A(Adapter);
		pHalData->bSetChnlBW = FALSE;
	}	

	if (Adapter->registrypriv.mp_mode == 0) {
		odm_clear_txpowertracking_state(pDM_Odm);
		PHY_SetTxPowerLevel8814(Adapter, pHalData->current_channel);
		if (pHalData->bNeedIQK == _TRUE) {
			phy_iq_calibrate_8814a(pDM_Odm, _FALSE);
			pHalData->bNeedIQK = _FALSE;
		}
	} else
		phy_iq_calibrate_8814a(pDM_Odm, _FALSE);
#if 0 //todo
#if (AUTO_CHNL_SEL_NHM == 1)	
	if(IS_AUTO_CHNL_SUPPORT(Adapter) && 
		P2PIsSocialChannel(pHalData->current_channel))
	{	
		RT_TRACE(COMP_SCAN, DBG_TRACE, ("[ACS] phy_SwChnlAndSetBwMode8723B(): current_channel %d Reset NHM counter!!\n", pHalData->current_channel));
		RT_TRACE(COMP_SCAN, DBG_TRACE, ("[ACS] phy_SwChnlAndSetBwMode8723B(): AutoChnlSelPeriod(%d)\n", 
			GetDefaultAdapter(Adapter)->MgntInfo.AutoChnlSel.AutoChnlSelPeriod));

		// Reset NHM counter		
    		odm_AutoChannelSelectReset(GET_PDM_ODM(Adapter));
		
		SET_AUTO_CHNL_STATE(Adapter, ACS_BEFORE_NHM);// Before NHM measurement
	}	
#endif
#endif //0
	pHalData->bSwChnlAndSetBWInProgress= FALSE;
}


VOID
PHY_SwChnlAndSetBWModeCallback8814A(
	IN PVOID            pContext
)
{
	PADAPTER		Adapter = (PADAPTER)pContext;
	phy_SwChnlAndSetBwMode8814A(Adapter);
}

/*
//
// Description:
//	Switch channel synchronously. Called by SwChnlByDelayHandler.
//
// Implemented by Bruce, 2008-02-14.
// The following procedure is operted according to SwChanlCallback8190Pci().
// However, this procedure is performed synchronously  which should be running under
// passive level.
// 
VOID
PHY_SwChnlSynchronously8814A(	// Only called during initialize
	IN	PADAPTER	Adapter,
	IN	u8		channel
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	RT_TRACE(COMP_SCAN | COMP_RM, DBG_LOUD, ("==>PHY_SwChnlSynchronously(), switch from channel %d to channel %d.\n", pHalData->current_channel, channel));

	// Cannot IO.
	if(RT_CANNOT_IO(Adapter))
		return;

	// Channel Switching is in progress.
	if(pHalData->bSwChnlAndSetBWInProgress)
		return;
	
	//return immediately if it is peudo-phy
	if(pHalData->rf_chip == RF_PSEUDO_11N)
	{
		pHalData->bSwChnlAndSetBWInProgress=FALSE;
		return;
	}

	switch(pHalData->CurrentWirelessMode)
	{
		case WIRELESS_MODE_A:
		case WIRELESS_MODE_N_5G:
		case WIRELESS_MODE_AC_5G:
			//Get first channel error when change between 5G and 2.4G band.
			//FIX ME!!!
			if(channel <=14)
				return;
			RT_ASSERT((channel>14), ("WIRELESS_MODE_A but channel<=14"));		
			break;
			
		case WIRELESS_MODE_B:
		case WIRELESS_MODE_G:
		case WIRELESS_MODE_N_24G:
		case WIRELESS_MODE_AC_24G:
			//Get first channel error when change between 5G and 2.4G band.
			//FIX ME!!!
			if(channel > 14)
				return;
			RT_ASSERT((channel<=14), ("WIRELESS_MODE_G but channel>14"));
			break;

		default:
			RT_ASSERT(FALSE, ("Invalid WirelessMode(%#x)!!\n", pHalData->CurrentWirelessMode));
			break;
	
	}	

	pHalData->bSwChnlAndSetBWInProgress = TRUE;
	if( channel == 0)
		channel = 1;

	pHalData->bSwChnl = TRUE;
	pHalData->bSetChnlBW = FALSE;
	pHalData->current_channel=channel;

	phy_SwChnlAndSetBwMode8814A(Adapter);

	RT_TRACE(COMP_SCAN | COMP_RM, DBG_LOUD, ("<==PHY_SwChnlSynchronously(), switch from channel %d to channel %d.\n", pHalData->current_channel, channel));
	
}
*/

VOID
PHY_HandleSwChnlAndSetBW8814A(
	IN	PADAPTER			Adapter,
	IN	BOOLEAN				bSwitchChannel,
	IN	BOOLEAN				bSetBandWidth,
	IN	u8					ChannelNum,
	enum channel_width		ChnlWidth,
	IN	u8					ChnlOffsetOf40MHz,
	IN	u8					ChnlOffsetOf80MHz,
	IN	u8					CenterFrequencyIndex1
)
{
	PADAPTER  			pDefAdapter =  GetDefaultAdapter(Adapter);
	PHAL_DATA_TYPE		pHalData = GET_HAL_DATA(pDefAdapter);
	u8					tmpChannel = pHalData->current_channel;
	enum channel_width		tmpBW= pHalData->current_channel_bw;
	u8					tmpnCur40MhzPrimeSC = pHalData->nCur40MhzPrimeSC;
	u8					tmpnCur80MhzPrimeSC = pHalData->nCur80MhzPrimeSC;
	u8					tmpCenterFrequencyIndex1 =pHalData->CurrentCenterFrequencyIndex1;
	struct mlme_ext_priv	*pmlmeext = &Adapter->mlmeextpriv;

	//check is swchnl or setbw
	if(!bSwitchChannel && !bSetBandWidth)
	{
		RTW_INFO("PHY_HandleSwChnlAndSetBW8812:  not switch channel and not set bandwidth \n");
		return;
	}

	//skip change for channel or bandwidth is the same
	if(bSwitchChannel)
	{
		if(pHalData->current_channel != ChannelNum)
		{
			if (HAL_IsLegalChannel(Adapter, ChannelNum))
				pHalData->bSwChnl = _TRUE;
			else
				return;
		}
	}

	if(bSetBandWidth)
	{
		if(pHalData->bChnlBWInitialized == _FALSE)
		{
			pHalData->bChnlBWInitialized = _TRUE;
			pHalData->bSetChnlBW = _TRUE;
		}
		else if((pHalData->current_channel_bw != ChnlWidth) ||
			(pHalData->nCur40MhzPrimeSC != ChnlOffsetOf40MHz) || 
			(pHalData->nCur80MhzPrimeSC != ChnlOffsetOf80MHz) ||
			(pHalData->CurrentCenterFrequencyIndex1!= CenterFrequencyIndex1))
		{
			pHalData->bSetChnlBW = _TRUE;
		}
	}

	if(!pHalData->bSetChnlBW && !pHalData->bSwChnl)
	{
		//RTW_INFO("<= PHY_HandleSwChnlAndSetBW8812: bSwChnl %d, bSetChnlBW %d \n",pHalData->bSwChnl,pHalData->bSetChnlBW);
		return;
	}


	if(pHalData->bSwChnl)
	{
		pHalData->current_channel=ChannelNum;
		pHalData->CurrentCenterFrequencyIndex1 = ChannelNum;
	}
	

	if(pHalData->bSetChnlBW)
	{
		pHalData->current_channel_bw = ChnlWidth;
#if 0
		if(ExtChnlOffsetOf40MHz==EXTCHNL_OFFSET_LOWER)
			pHalData->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_UPPER;
		else if(ExtChnlOffsetOf40MHz==EXTCHNL_OFFSET_UPPER)
			pHalData->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_LOWER;
		else
			pHalData->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

		if(ExtChnlOffsetOf80MHz==EXTCHNL_OFFSET_LOWER)
			pHalData->nCur80MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_UPPER;
		else if(ExtChnlOffsetOf80MHz==EXTCHNL_OFFSET_UPPER)
			pHalData->nCur80MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_LOWER;
		else
			pHalData->nCur80MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
#else
		pHalData->nCur40MhzPrimeSC = ChnlOffsetOf40MHz;
		pHalData->nCur80MhzPrimeSC = ChnlOffsetOf80MHz;
#endif

		pHalData->CurrentCenterFrequencyIndex1 = CenterFrequencyIndex1;		
	}

	//Switch workitem or set timer to do switch channel or setbandwidth operation
	if (!RTW_CANNOT_RUN(Adapter))
		phy_SwChnlAndSetBwMode8814A(Adapter);
	else {
		if(pHalData->bSwChnl)
		{
			pHalData->current_channel = tmpChannel;
			pHalData->CurrentCenterFrequencyIndex1 = tmpChannel;
		}	
		if(pHalData->bSetChnlBW)
		{
			pHalData->current_channel_bw = tmpBW;
			pHalData->nCur40MhzPrimeSC = tmpnCur40MhzPrimeSC;
			pHalData->nCur80MhzPrimeSC = tmpnCur80MhzPrimeSC;
			pHalData->CurrentCenterFrequencyIndex1 = tmpCenterFrequencyIndex1;
		}
	}

	//RTW_INFO("Channel %d ChannelBW %d ",pHalData->current_channel, pHalData->current_channel_bw);
	//RTW_INFO("40MhzPrimeSC %d 80MhzPrimeSC %d ",pHalData->nCur40MhzPrimeSC, pHalData->nCur80MhzPrimeSC);
	//RTW_INFO("CenterFrequencyIndex1 %d \n",pHalData->CurrentCenterFrequencyIndex1);

	//RTW_INFO("<= PHY_HandleSwChnlAndSetBW8812: bSwChnl %d, bSetChnlBW %d \n",pHalData->bSwChnl,pHalData->bSetChnlBW);

}


/*
//
//	Description:
//		Configure H/W functionality to enable/disable Monitor mode.
//		Note, because we possibly need to configure BB and RF in this function, 
//		so caller should in PASSIVE_LEVEL. 080118, by rcnjko.
//
VOID
PHY_SetMonitorMode8814A(
	IN	PADAPTER			pAdapter,
	IN	BOOLEAN				bEnableMonitorMode
	)
{
	HAL_DATA_TYPE		*pHalData	= GET_HAL_DATA(pAdapter);
	BOOLEAN				bFilterOutNonAssociatedBSSID = FALSE;

	//2 Note: we may need to stop antenna diversity.
	if(bEnableMonitorMode)
	{
		bFilterOutNonAssociatedBSSID = FALSE;
		RT_TRACE(COMP_RM, DBG_LOUD, ("PHY_SetMonitorMode8814A(): enable monitor mode\n"));

		pHalData->bInMonitorMode = TRUE;
		pAdapter->HalFunc.AllowAllDestAddrHandler(pAdapter, TRUE, TRUE);
		rtw_hal_set_hwreg(pAdapter, HW_VAR_CHECK_BSSID, (u8*)&bFilterOutNonAssociatedBSSID);
	}
	else
	{
		bFilterOutNonAssociatedBSSID = TRUE;
		RT_TRACE(COMP_RM, DBG_LOUD, ("PHY_SetMonitorMode8814A(): disable monitor mode\n"));

		pAdapter->HalFunc.AllowAllDestAddrHandler(pAdapter, FALSE, TRUE);
		pHalData->bInMonitorMode = FALSE;
		rtw_hal_set_hwreg(pAdapter, HW_VAR_CHECK_BSSID, (u8*)&bFilterOutNonAssociatedBSSID);
	}
}
*/

BOOLEAN
SetAntennaConfig8814A(
	IN	PADAPTER		pAdapter,
	IN	u8			DefaultAnt		// 0: Main, 1: Aux.
)
{
	return TRUE;
}

VOID
PHY_SetBWMode8814(
	IN	PADAPTER			Adapter,
	IN	enum channel_width	Bandwidth,	// 20M or 40M
	IN	u8					Offset		// Upper, Lower, or Don't care
)
{
	PHAL_DATA_TYPE		pHalData = GET_HAL_DATA(Adapter);

	//RTW_INFO("%s()===>\n",__FUNCTION__);

	PHY_HandleSwChnlAndSetBW8814A(Adapter, _FALSE, _TRUE, pHalData->current_channel, Bandwidth, Offset, Offset, pHalData->current_channel);

	//RTW_INFO("<==%s()\n",__FUNCTION__);
}

VOID
PHY_SwChnl8814(
	IN	PADAPTER	Adapter,
	IN	u8			channel
	)
{
	//RTW_INFO("%s()===>\n",__FUNCTION__);

	PHY_HandleSwChnlAndSetBW8814A(Adapter, _TRUE, _FALSE, channel, 0, 0, 0, channel);

	//RTW_INFO("<==%s()\n",__FUNCTION__);
}

VOID
PHY_SetSwChnlBWMode8814(
	IN	PADAPTER			Adapter,
	IN	u8					channel,
	IN	enum channel_width		Bandwidth,
	IN	u8					Offset40,
	IN	u8					Offset80
)
{
	//RTW_INFO("%s()===>\n",__FUNCTION__);

	PHY_HandleSwChnlAndSetBW8814A(Adapter, _TRUE, _TRUE, channel, Bandwidth, Offset40, Offset80, channel);

	//RTW_INFO("<==%s()\n",__FUNCTION__);
}

