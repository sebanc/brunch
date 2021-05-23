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

#if !defined(__ECOS) && !defined(CONFIG_COMPAT_WIRELESS)
#include "mp_precomp.h"
#else
#include "../mp_precomp.h"
#endif
#include "../phydm_precomp.h"



/*---------------------------Define Local Constant---------------------------*/
// 2010/04/25 MH Define the max tx power tracking tx agc power.
#define		ODM_TXPWRTRACK_MAX_IDX8814A		6

/*---------------------------Define Local Constant---------------------------*/


//3============================================================
//3 Tx Power Tracking
//3============================================================

u1Byte
CheckRFGainOffset(
	PDM_ODM_T			pDM_Odm,
	PWRTRACK_METHOD 	Method,
	u1Byte				RFPath
	)
{
	s1Byte	UpperBound = 10, LowerBound = -5; // 4'b1010 = 10
	s1Byte	Final_RF_Index = 0;
	BOOLEAN	bPositive = FALSE;
	u4Byte	bitMask = 0;
	u1Byte	Final_OFDM_Swing_Index = 0, TxScalingUpperBound = 28, TxScalingLowerBound = 4;// upper bound +2dB, lower bound -9dB
	PODM_RF_CAL_T	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);

	if(Method == MIX_MODE)	//normal Tx power tracking
	{
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,("is 8814 MP chip\n"));
		bitMask = BIT19;
		pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] = pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] + pRFCalibrateInfo->KfreeOffset[RFPath];

		if( pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] >= 0)				// check if RF_Index is positive or not
			bPositive = TRUE;
		else
			bPositive = FALSE;

		ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, bitMask, bPositive);
		
		bitMask = BIT18|BIT17|BIT16|BIT15;
		Final_RF_Index = pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] / 2;		/*TxBB 1 step equal 1dB, BB swing 1step equal 0.5dB*/

	}
	
	if(Final_RF_Index > UpperBound)		//Upper bound = 10dB, if more htan upper bound, then move to bb swing max = +2dB
	{
		ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, bitMask, UpperBound);	//set RF Reg0x55 per path
			
		Final_OFDM_Swing_Index = pRFCalibrateInfo->DefaultOfdmIndex + (pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] - (UpperBound << 1));
		
		if(Final_OFDM_Swing_Index > TxScalingUpperBound)	//	bb swing upper bound = +2dB
			Final_OFDM_Swing_Index = TxScalingUpperBound;

		return Final_OFDM_Swing_Index;
	}
	else if(Final_RF_Index < LowerBound)	// lower bound = -5dB
	{
		ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, bitMask, (-1)*(LowerBound));	//set RF Reg0x55 per path

		Final_OFDM_Swing_Index = pRFCalibrateInfo->DefaultOfdmIndex - ((LowerBound<<1) - pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath]);

		if(Final_OFDM_Swing_Index < TxScalingLowerBound)	// bb swing lower bound = -10dB
			Final_OFDM_Swing_Index = TxScalingLowerBound;
		return Final_OFDM_Swing_Index;
	}
	else		// normal case
	{

		if(bPositive == TRUE)
			ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, bitMask, Final_RF_Index);	//set RF Reg0x55 per path
		else
			ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, bitMask, (-1)*Final_RF_Index);	//set RF Reg0x55 per path

		Final_OFDM_Swing_Index = pRFCalibrateInfo->DefaultOfdmIndex + (pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath])%2;
		return Final_OFDM_Swing_Index;	
	}
	
	return FALSE;
}


VOID
ODM_TxPwrTrackSetPwr8814A(
	PDM_ODM_T			pDM_Odm,
	PWRTRACK_METHOD 	Method,
	u1Byte 				RFPath,
	u1Byte 				ChannelMappedIndex
	)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)		
		PADAPTER		Adapter = pDM_Odm->Adapter;
		PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
#endif
		u1Byte			Final_OFDM_Swing_Index = 0; 
	PODM_RF_CAL_T	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);

		if (Method == MIX_MODE)			
		{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,("pDM_Odm->DefaultOfdmIndex=%d, pDM_Odm->Absolute_OFDMSwingIdx[RFPath]=%d, RF_Path = %d\n",
				pRFCalibrateInfo->DefaultOfdmIndex, pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath], RFPath));
		
			Final_OFDM_Swing_Index = CheckRFGainOffset(pDM_Odm, MIX_MODE, RFPath);
		}
		else if(Method == TSSI_MODE)
		{
			ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, BIT18|BIT17|BIT16|BIT15, 0);
		}
		else if(Method == BBSWING)		// use for mp driver clean power tracking status
		{
			pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] = pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] + pRFCalibrateInfo->KfreeOffset[RFPath];

			Final_OFDM_Swing_Index = pRFCalibrateInfo->DefaultOfdmIndex + (pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath]);

			ODM_SetRFReg(pDM_Odm, RFPath, rRF_TxGainOffset, BIT18|BIT17|BIT16|BIT15, 0);
		}

		if((Method == MIX_MODE) || (Method == BBSWING))
		{
			switch(RFPath)
			{
				case ODM_RF_PATH_A:
					
					ODM_SetBBReg(pDM_Odm, rA_TxScale_Jaguar, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

					ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
						("******Path_A Compensate with BBSwing , Final_OFDM_Swing_Index = %d \n", Final_OFDM_Swing_Index));
					break;

				case ODM_RF_PATH_B:
						
					ODM_SetBBReg(pDM_Odm, rB_TxScale_Jaguar, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

					ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
						("******Path_B Compensate with BBSwing , Final_OFDM_Swing_Index = %d \n", Final_OFDM_Swing_Index));
					break;

				case ODM_RF_PATH_C:
						
					ODM_SetBBReg(pDM_Odm, rC_TxScale_Jaguar2, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

					ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
						("******Path_C Compensate with BBSwing , Final_OFDM_Swing_Index = %d \n", Final_OFDM_Swing_Index));
			            	break;

				case ODM_RF_PATH_D:
						
					ODM_SetBBReg(pDM_Odm, rD_TxScale_Jaguar2, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

					ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
						("******Path_D Compensate with BBSwing , Final_OFDM_Swing_Index = %d \n", Final_OFDM_Swing_Index));
					break;

				default:
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
						("Wrong Path name!!!! \n"));

				break;
			}
		}
		return;
}	// ODM_TxPwrTrackSetPwr8814A

VOID
GetDeltaSwingTable_8814A(
	IN 	PDM_ODM_T			pDM_Odm,
	OUT pu1Byte 			*TemperatureUP_A,
	OUT pu1Byte 			*TemperatureDOWN_A,
	OUT pu1Byte 			*TemperatureUP_B,
	OUT pu1Byte 			*TemperatureDOWN_B	
	)
{
    PODM_RF_CAL_T	pRFCalibrateInfo	= &(pDM_Odm->RFCalibrateInfo);
	u2Byte			rate				= *(pDM_Odm->pForcedDataRate);
	u1Byte			channel			= *(pDM_Odm->pChannel);

	if ( 1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(rate)) {
	        *TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKA_P;
	        *TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKA_N;
	        *TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKB_P;
	        *TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKB_N;		
		} else {
	        *TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_2GA_P;
	        *TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_2GA_N;
	        *TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_2GB_P;
	        *TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_2GB_N;			
		}
 	} else if ( 36 <= channel && channel <= 64) {
        *TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_P[0];
        *TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_N[0];
        *TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_P[0];
        *TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_N[0];
    } else if ( 100 <= channel && channel <= 140) {
        *TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_P[1];
        *TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_N[1];
        *TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_P[1];
        *TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_N[1];
    } else if ( 149 <= channel && channel <= 173) {
        *TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_P[2]; 
        *TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_N[2]; 
        *TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_P[2]; 
        *TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_N[2]; 
    } else {
	    *TemperatureUP_A   = (pu1Byte)DeltaSwingTableIdx_2GA_P_DEFAULT;
	    *TemperatureDOWN_A = (pu1Byte)DeltaSwingTableIdx_2GA_N_DEFAULT;	
	    *TemperatureUP_B   = (pu1Byte)DeltaSwingTableIdx_2GA_P_DEFAULT;
	    *TemperatureDOWN_B = (pu1Byte)DeltaSwingTableIdx_2GA_N_DEFAULT;		
    }
	
	return;
}


VOID
GetDeltaSwingTable_8814A_PathCD(
	IN 	PDM_ODM_T			pDM_Odm,
	OUT pu1Byte 			*TemperatureUP_C,
	OUT pu1Byte 			*TemperatureDOWN_C,
	OUT pu1Byte 			*TemperatureUP_D,
	OUT pu1Byte 			*TemperatureDOWN_D	
	)
{
	PODM_RF_CAL_T 	pRFCalibrateInfo	= &(pDM_Odm->RFCalibrateInfo);
	u2Byte			rate				= *(pDM_Odm->pForcedDataRate);
	u1Byte			channel			= *(pDM_Odm->pChannel);

	if ( 1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(rate)) {
	        *TemperatureUP_C  = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKC_P;
	        *TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKC_N;
	        *TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKD_P;
	        *TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_2GCCKD_N;		
		} else {
	        *TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_2GC_P;
	        *TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_2GC_N;
	        *TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_2GD_P;
	        *TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_2GD_N;			
		}
 	} else if ( 36 <= channel && channel <= 64) {
        *TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_P[0];
        *TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_N[0];
        *TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_P[0];
        *TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_N[0];
    } else if ( 100 <= channel && channel <= 140) {
        *TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_P[1];
        *TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_N[1];
        *TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_P[1];
        *TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_N[1];
    } else if ( 149 <= channel && channel <= 173) {
        *TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_P[2]; 
        *TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_N[2]; 
        *TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_P[2]; 
        *TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_N[2]; 
    } else {
	    *TemperatureUP_C   = (pu1Byte)DeltaSwingTableIdx_2GA_P_DEFAULT;
	    *TemperatureDOWN_C = (pu1Byte)DeltaSwingTableIdx_2GA_N_DEFAULT;	
	    *TemperatureUP_D   = (pu1Byte)DeltaSwingTableIdx_2GA_P_DEFAULT;
	    *TemperatureDOWN_D = (pu1Byte)DeltaSwingTableIdx_2GA_N_DEFAULT;		
    }
	
	return;
}


void ConfigureTxpowerTrack_8814A(
		IN PTXPWRTRACK_CFG	pConfig
		)
{
	pConfig->SwingTableSize_CCK = ODM_CCK_TABLE_SIZE;
	pConfig->SwingTableSize_OFDM = ODM_OFDM_TABLE_SIZE;
	pConfig->Threshold_IQK = 8;
	pConfig->AverageThermalNum = AVG_THERMAL_NUM_8814A;
	pConfig->RfPathCount = MAX_PATH_NUM_8814A;
	pConfig->ThermalRegAddr = RF_T_METER_8814A;

	pConfig->ODM_TxPwrTrackSetPwr = ODM_TxPwrTrackSetPwr8814A;
	pConfig->PHY_LCCalibrate = PHY_LCCalibrate_8814A;
	pConfig->DoIQK = DoIQK_8814A;
	pConfig->GetDeltaSwingTable = GetDeltaSwingTable_8814A;
	pConfig->GetDeltaSwingTable8814only = GetDeltaSwingTable_8814A_PathCD;
}



//1 7.	IQK



//
// 2011/07/26 MH Add an API for testing IQK fail case.
//
// MP Already declare in odm.c 
#if 0	//!(DM_ODM_SUPPORT_TYPE & ODM_WIN) 
BOOLEAN
ODM_CheckPowerStatus(
		IN	PADAPTER		Adapter)
{
	/*
	   HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	   PDM_ODM_T			pDM_Odm = &pHalData->DM_OutSrc;
	   RT_RF_POWER_STATE 	rtState;
	   PMGNT_INFO			pMgntInfo	= &(Adapter->MgntInfo);

	// 2011/07/27 MH We are not testing ready~~!! We may fail to get correct value when init sequence.
	if (pMgntInfo->init_adpt_in_progress == TRUE)
	{
	ODM_RT_TRACE(pDM_Odm,COMP_INIT, DBG_LOUD, ("ODM_CheckPowerStatus Return TRUE, due to initadapter"));
	return	TRUE;
	}

	//
	//	2011/07/19 MH We can not execute tx pwoer tracking/ LLC calibrate or IQK.
	//
	Adapter->HalFunc.GetHwRegHandler(Adapter, HW_VAR_RF_STATE, (pu1Byte)(&rtState));	
	if(Adapter->bDriverStopped || Adapter->bDriverIsGoingToPnpSetPowerSleep || rtState == eRfOff)
	{
	ODM_RT_TRACE(pDM_Odm,COMP_INIT, DBG_LOUD, ("ODM_CheckPowerStatus Return FALSE, due to %d/%d/%d\n", 
	Adapter->bDriverStopped, Adapter->bDriverIsGoingToPnpSetPowerSleep, rtState));
	return	FALSE;
	}
	 */
	return	TRUE;
}
#endif

VOID
	_PHY_SaveADDARegisters_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	pu4Byte		ADDAReg,
			IN	pu4Byte		ADDABackup,
			IN	u4Byte		RegisterNum
			)
{
	u4Byte	i;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif

	if (ODM_CheckPowerStatus(pAdapter) == FALSE)
		return;
#endif

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("Save ADDA parameters.\n"));
	for( i = 0 ; i < RegisterNum ; i++){
		ADDABackup[i] = ODM_GetBBReg(pDM_Odm, ADDAReg[i], bMaskDWord);
	}
}


VOID
	_PHY_SaveMACRegisters_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	pu4Byte		MACReg,
			IN	pu4Byte		MACBackup
			)
{
	u4Byte	i;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif	
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("Save MAC parameters.\n"));
	for( i = 0 ; i < (IQK_MAC_REG_NUM - 1); i++){
		MACBackup[i] = ODM_Read1Byte(pDM_Odm, MACReg[i]);		
	}
	MACBackup[i] = ODM_Read4Byte(pDM_Odm, MACReg[i]);		

}


VOID
	_PHY_ReloadADDARegisters_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	pu4Byte		ADDAReg,
			IN	pu4Byte		ADDABackup,
			IN	u4Byte		RegiesterNum
			)
{
	u4Byte	i;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("Reload ADDA power saving parameters !\n"));
	for(i = 0 ; i < RegiesterNum; i++)
	{
		ODM_SetBBReg(pDM_Odm, ADDAReg[i], bMaskDWord, ADDABackup[i]);
	}
}

VOID
	_PHY_ReloadMACRegisters_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	pu4Byte		MACReg,
			IN	pu4Byte		MACBackup
			)
{
	u4Byte	i;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("Reload MAC parameters !\n"));
	for(i = 0 ; i < (IQK_MAC_REG_NUM - 1); i++){
		ODM_Write1Byte(pDM_Odm, MACReg[i], (u1Byte)MACBackup[i]);
	}
	ODM_Write4Byte(pDM_Odm, MACReg[i], MACBackup[i]);	
}



VOID
	_PHY_MACSettingCalibration_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	pu4Byte		MACReg,
			IN	pu4Byte		MACBackup	
			)
{
	u4Byte	i = 0;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif	
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("MAC settings for Calibration.\n"));

	ODM_Write1Byte(pDM_Odm, MACReg[i], 0x3F);

	for(i = 1 ; i < (IQK_MAC_REG_NUM - 1); i++){
		ODM_Write1Byte(pDM_Odm, MACReg[i], (u1Byte)(MACBackup[i]&(~BIT3)));
	}
	ODM_Write1Byte(pDM_Odm, MACReg[i], (u1Byte)(MACBackup[i]&(~BIT5)));	

}

#if 0
#define BW_20M 	0
#define	BW_40M  1
#define	BW_80M	2
#endif

VOID
	phy_LCCalibrate_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	BOOLEAN		is2T
			)
{
	u4Byte	/*RF_Amode=0, RF_Bmode=0,*/ LC_Cal = 0, tmp = 0;
	u4Byte cnt;
	
	//Check continuous TX and Packet TX
	u4Byte	reg0x914 = ODM_Read4Byte(pDM_Odm, rSingleTone_ContTx_Jaguar);;

	// Backup RF reg18.

	if((reg0x914 & 0x70000) == 0)
		ODM_Write1Byte(pDM_Odm, REG_TXPAUSE_8812, 0xFF);			

	//3 3. Read RF reg18
	LC_Cal = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask);

	//3 4. Set LC calibration begin bit15
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, 0x8000, 0x1);

	ODM_delay_ms(100);	

	for (cnt = 0; cnt < 100; cnt++) {
		if (ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, 0x8000) != 0x1)
			break;
		ODM_delay_ms(10);
	}
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("retry cnt = %d\n", cnt));


	//3 Restore original situation
	if((reg0x914 & 70000) == 0)
		ODM_Write1Byte(pDM_Odm, REG_TXPAUSE_8812, 0x00);	

	// Recover channel number
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask, LC_Cal);
}

//Analog Pre-distortion calibration
#define		APK_BB_REG_NUM	8
#define		APK_CURVE_REG_NUM 4
#define		PATH_NUM		2

VOID	
	phy_APCalibrate_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	s1Byte 		delta,
			IN	BOOLEAN		is2T
			)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif
	u4Byte 			regD[PATH_NUM];
	u4Byte			tmpReg, index, offset,  apkbound;
	u1Byte			path, i, pathbound = PATH_NUM;		
	u4Byte			BB_backup[APK_BB_REG_NUM];
	u4Byte			BB_REG[APK_BB_REG_NUM] = {	
		rFPGA1_TxBlock, 	rOFDM0_TRxPathEnable, 
		rFPGA0_RFMOD, 	rOFDM0_TRMuxPar, 
		rFPGA0_XCD_RFInterfaceSW,	rFPGA0_XAB_RFInterfaceSW, 
		rFPGA0_XA_RFInterfaceOE, 	rFPGA0_XB_RFInterfaceOE	};
	u4Byte			BB_AP_MODE[APK_BB_REG_NUM] = {	
		0x00000020, 0x00a05430, 0x02040000, 
		0x000800e4, 0x00204000 };
	u4Byte			BB_normal_AP_MODE[APK_BB_REG_NUM] = {	
		0x00000020, 0x00a05430, 0x02040000, 
		0x000800e4, 0x22204000 };						

	u4Byte			AFE_backup[IQK_ADDA_REG_NUM];
	u4Byte			AFE_REG[IQK_ADDA_REG_NUM] = {	
		rFPGA0_XCD_SwitchControl, 	rBlue_Tooth, 	
		rRx_Wait_CCA, 		rTx_CCK_RFON,
		rTx_CCK_BBON, 	rTx_OFDM_RFON, 	
		rTx_OFDM_BBON, 	rTx_To_Rx,
		rTx_To_Tx, 		rRx_CCK, 	
		rRx_OFDM, 		rRx_Wait_RIFS,
		rRx_TO_Rx, 		rStandby, 	
		rSleep, 			rPMPD_ANAEN };

	u4Byte			MAC_backup[IQK_MAC_REG_NUM];
	u4Byte			MAC_REG[IQK_MAC_REG_NUM] = {
		REG_TXPAUSE, 		REG_BCN_CTRL,	
		REG_BCN_CTRL_1,	REG_GPIO_MUXCFG};

	u4Byte			APK_RF_init_value[PATH_NUM][APK_BB_REG_NUM] = {
		{0x0852c, 0x1852c, 0x5852c, 0x1852c, 0x5852c},
		{0x2852e, 0x0852e, 0x3852e, 0x0852e, 0x0852e}
	};	

	u4Byte			APK_normal_RF_init_value[PATH_NUM][APK_BB_REG_NUM] = {
		{0x0852c, 0x0a52c, 0x3a52c, 0x5a52c, 0x5a52c},	//path settings equal to path b settings
		{0x0852c, 0x0a52c, 0x5a52c, 0x5a52c, 0x5a52c}
	};

	u4Byte			APK_RF_value_0[PATH_NUM][APK_BB_REG_NUM] = {
		{0x52019, 0x52014, 0x52013, 0x5200f, 0x5208d},
		{0x5201a, 0x52019, 0x52016, 0x52033, 0x52050}
	};

	u4Byte			APK_normal_RF_value_0[PATH_NUM][APK_BB_REG_NUM] = {
		{0x52019, 0x52017, 0x52010, 0x5200d, 0x5206a},	//path settings equal to path b settings
		{0x52019, 0x52017, 0x52010, 0x5200d, 0x5206a}
	};

	u4Byte			AFE_on_off[PATH_NUM] = {
		0x04db25a4, 0x0b1b25a4};	//path A on path B off / path A off path B on

	u4Byte			APK_offset[PATH_NUM] = {
		rConfig_AntA, rConfig_AntB};

	u4Byte			APK_normal_offset[PATH_NUM] = {
		rConfig_Pmpd_AntA, rConfig_Pmpd_AntB};

	u4Byte			APK_value[PATH_NUM] = {
		0x92fc0000, 0x12fc0000};					

	u4Byte			APK_normal_value[PATH_NUM] = {
		0x92680000, 0x12680000};					

	s1Byte			APK_delta_mapping[APK_BB_REG_NUM][13] = {
		{-4, -3, -2, -2, -1, -1, 0, 1, 2, 3, 4, 5, 6},
		{-4, -3, -2, -2, -1, -1, 0, 1, 2, 3, 4, 5, 6},											
		{-6, -4, -2, -2, -1, -1, 0, 1, 2, 3, 4, 5, 6},
		{-1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6},
		{-11, -9, -7, -5, -3, -1, 0, 0, 0, 0, 0, 0, 0}
	};

	u4Byte			APK_normal_setting_value_1[13] = {
		0x01017018, 0xf7ed8f84, 0x1b1a1816, 0x2522201e, 0x322e2b28,
		0x433f3a36, 0x5b544e49, 0x7b726a62, 0xa69a8f84, 0xdfcfc0b3,
		0x12680000, 0x00880000, 0x00880000
	};

	u4Byte			APK_normal_setting_value_2[16] = {
		0x01c7021d, 0x01670183, 0x01000123, 0x00bf00e2, 0x008d00a3,
		0x0068007b, 0x004d0059, 0x003a0042, 0x002b0031, 0x001f0025,
		0x0017001b, 0x00110014, 0x000c000f, 0x0009000b, 0x00070008,
		0x00050006
	};

	u4Byte			APK_result[PATH_NUM][APK_BB_REG_NUM];	//val_1_1a, val_1_2a, val_2a, val_3a, val_4a
	//	u4Byte			AP_curve[PATH_NUM][APK_CURVE_REG_NUM];

	s4Byte			BB_offset, delta_V, delta_offset;

#if defined(MP_DRIVER) && (MP_DRIVER == 1)
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PMPT_CONTEXT	pMptCtx = &(pAdapter->mppriv.MptCtx);	
#else
	PMPT_CONTEXT	pMptCtx = &(pAdapter->MptCtx);	
#endif
	pMptCtx->APK_bound[0] = 45;
	pMptCtx->APK_bound[1] = 52;		

#endif

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==>phy_APCalibrate_8814A() delta %d\n", delta));
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("AP Calibration for %s\n", (is2T ? "2T2R" : "1T1R")));
	if(!is2T)
		pathbound = 1;

	//2 FOR NORMAL CHIP SETTINGS

	// Temporarily do not allow normal driver to do the following settings because these offset
	// and value will cause RF internal PA to be unpredictably disabled by HW, such that RF Tx signal
	// will disappear after disable/enable card many times on 88CU. RF SD and DD have not find the
	// root cause, so we remove these actions temporarily. Added by tynli and SD3 Allen. 2010.05.31.
#if !defined(MP_DRIVER) || (MP_DRIVER != 1)
	return;
#endif
	//settings adjust for normal chip
	for(index = 0; index < PATH_NUM; index ++)
	{
		APK_offset[index] = APK_normal_offset[index];
		APK_value[index] = APK_normal_value[index];
		AFE_on_off[index] = 0x6fdb25a4;
	}

	for(index = 0; index < APK_BB_REG_NUM; index ++)
	{
		for(path = 0; path < pathbound; path++)
		{
			APK_RF_init_value[path][index] = APK_normal_RF_init_value[path][index];
			APK_RF_value_0[path][index] = APK_normal_RF_value_0[path][index];
		}
		BB_AP_MODE[index] = BB_normal_AP_MODE[index];
	}			

	apkbound = 6;

	//save BB default value
	for(index = 0; index < APK_BB_REG_NUM ; index++)
	{
		if(index == 0)		//skip 
			continue;				
		BB_backup[index] = ODM_GetBBReg(pDM_Odm, BB_REG[index], bMaskDWord);
	}

	//save MAC default value													
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_SaveMACRegisters_8814A(pAdapter, MAC_REG, MAC_backup);

	//save AFE default value
	_PHY_SaveADDARegisters_8814A(pAdapter, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);
#else
	_PHY_SaveMACRegisters_8814A(pDM_Odm, MAC_REG, MAC_backup);

	//save AFE default value
	_PHY_SaveADDARegisters_8814A(pDM_Odm, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);
#endif

	for(path = 0; path < pathbound; path++)
	{


		if(path == RF_PATH_A)
		{
			//path A APK
			//load APK setting
			//path-A		
			offset = rPdp_AntA;
			for(index = 0; index < 11; index ++)			
			{
				ODM_SetBBReg(pDM_Odm, offset, bMaskDWord, APK_normal_setting_value_1[index]);
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", offset, ODM_GetBBReg(pDM_Odm, offset, bMaskDWord))); 	

				offset += 0x04;
			}

			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x12680000);

			offset = rConfig_AntA;
			for(; index < 13; index ++) 		
			{
				ODM_SetBBReg(pDM_Odm, offset, bMaskDWord, APK_normal_setting_value_1[index]);
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", offset, ODM_GetBBReg(pDM_Odm, offset, bMaskDWord))); 	

				offset += 0x04;
			}	

			//page-B1
			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x40000000);

			//path A
			offset = rPdp_AntA;
			for(index = 0; index < 16; index++)
			{
				ODM_SetBBReg(pDM_Odm, offset, bMaskDWord, APK_normal_setting_value_2[index]);		
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", offset, ODM_GetBBReg(pDM_Odm, offset, bMaskDWord))); 	

				offset += 0x04;
			}				
			ODM_SetBBReg(pDM_Odm,  rFPGA0_IQK, bMaskDWord, 0x00000000);							
		}
		else if(path == RF_PATH_B)
		{
			//path B APK
			//load APK setting
			//path-B		
			offset = rPdp_AntB;
			for(index = 0; index < 10; index ++)			
			{
				ODM_SetBBReg(pDM_Odm, offset, bMaskDWord, APK_normal_setting_value_1[index]);
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", offset, ODM_GetBBReg(pDM_Odm, offset, bMaskDWord))); 	

				offset += 0x04;
			}
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntA, bMaskDWord, 0x12680000);			
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x12680000);

			offset = rConfig_AntA;
			index = 11;
			for(; index < 13; index ++) //offset 0xb68, 0xb6c		
			{
				ODM_SetBBReg(pDM_Odm, offset, bMaskDWord, APK_normal_setting_value_1[index]);
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", offset, ODM_GetBBReg(pDM_Odm, offset, bMaskDWord))); 	

				offset += 0x04;
			}	

			//page-B1
			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x40000000);

			//path B
			offset = 0xb60;
			for(index = 0; index < 16; index++)
			{
				ODM_SetBBReg(pDM_Odm, offset, bMaskDWord, APK_normal_setting_value_2[index]);		
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", offset, ODM_GetBBReg(pDM_Odm, offset, bMaskDWord))); 	

				offset += 0x04;
			}				
			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0);							
		}

		//save RF default value
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
		regD[path] = PHY_QueryRFReg(pAdapter, path, RF_TXBIAS_A, bMaskDWord);
#else
		regD[path] = ODM_GetRFReg(pDM_Odm, path, RF_TXBIAS_A, bMaskDWord);
#endif

		//Path A AFE all on, path B AFE All off or vise versa
		for(index = 0; index < IQK_ADDA_REG_NUM ; index++)
			ODM_SetBBReg(pDM_Odm, AFE_REG[index], bMaskDWord, AFE_on_off[path]);
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0xe70 %x\n", ODM_GetBBReg(pDM_Odm, rRx_Wait_CCA, bMaskDWord)));		

		//BB to AP mode
		if(path == 0)
		{				
			for(index = 0; index < APK_BB_REG_NUM ; index++)
			{

				if(index == 0)		//skip 
					continue;			
				else if (index < 5)
					ODM_SetBBReg(pDM_Odm, BB_REG[index], bMaskDWord, BB_AP_MODE[index]);
				else if (BB_REG[index] == 0x870)
					ODM_SetBBReg(pDM_Odm, BB_REG[index], bMaskDWord, BB_backup[index]|BIT10|BIT26);
				else
					ODM_SetBBReg(pDM_Odm, BB_REG[index], BIT10, 0x0);					
			}

			ODM_SetBBReg(pDM_Odm, rTx_IQK_Tone_A, bMaskDWord, 0x01008c00);			
			ODM_SetBBReg(pDM_Odm, rRx_IQK_Tone_A, bMaskDWord, 0x01008c00);					
		}
		else		//path B
		{
			ODM_SetBBReg(pDM_Odm, rTx_IQK_Tone_B, bMaskDWord, 0x01008c00);			
			ODM_SetBBReg(pDM_Odm, rRx_IQK_Tone_B, bMaskDWord, 0x01008c00);					

		}

		ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x800 %x\n", ODM_GetBBReg(pDM_Odm, 0x800, bMaskDWord)));				

		//MAC settings
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
		_PHY_MACSettingCalibration_8814A(pAdapter, MAC_REG, MAC_backup);
#else
		_PHY_MACSettingCalibration_8814A(pDM_Odm, MAC_REG, MAC_backup);
#endif

		if(path == RF_PATH_A)	//Path B to standby mode
		{
			ODM_SetRFReg(pDM_Odm, RF_PATH_B, RF_AC, bMaskDWord, 0x10000);			
		}
		else			//Path A to standby mode
		{
			ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_AC, bMaskDWord, 0x10000);			
			ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_MODE1, bMaskDWord, 0x1000f);			
			ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_MODE2, bMaskDWord, 0x20103);						
		}

		delta_offset = ((delta+14)/2);
		if(delta_offset < 0)
			delta_offset = 0;
		else if (delta_offset > 12)
			delta_offset = 12;

		//AP calibration
		for(index = 0; index < APK_BB_REG_NUM; index++)
		{
			if(index != 1)	//only DO PA11+PAD01001, AP RF setting
				continue;

			tmpReg = APK_RF_init_value[path][index];
#if 1			
			if(!pDM_Odm->RFCalibrateInfo.bAPKThermalMeterIgnore)
			{
				BB_offset = (tmpReg & 0xF0000) >> 16;

				if(!(tmpReg & BIT15)) //sign bit 0
				{
					BB_offset = -BB_offset;
				}

				delta_V = APK_delta_mapping[index][delta_offset];

				BB_offset += delta_V;

				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() APK index %d tmpReg 0x%x delta_V %d delta_offset %d\n", index, tmpReg, (int)delta_V, (int)delta_offset));

				if(BB_offset < 0)
				{
					tmpReg = tmpReg & (~BIT15);
					BB_offset = -BB_offset;
				}
				else
				{
					tmpReg = tmpReg | BIT15;
				}
				tmpReg = (tmpReg & 0xFFF0FFFF) | (BB_offset << 16);
			}
#endif

			ODM_SetRFReg(pDM_Odm, path, RF_IPA_A, bMaskDWord, 0x8992e);
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0xc %x\n", PHY_QueryRFReg(pAdapter, path, RF_IPA_A, bMaskDWord)));		
			ODM_SetRFReg(pDM_Odm, path, RF_AC, bMaskDWord, APK_RF_value_0[path][index]);
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("phy_APCalibrate_8814A() offset 0x0 %x\n", PHY_QueryRFReg(pAdapter, path, RF_AC, bMaskDWord)));		
			ODM_SetRFReg(pDM_Odm, path, RF_TXBIAS_A, bMaskDWord, tmpReg);
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0xd %x\n", PHY_QueryRFReg(pAdapter, path, RF_TXBIAS_A, bMaskDWord)));					
#else
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0xc %x\n", ODM_GetRFReg(pDM_Odm, path, RF_IPA_A, bMaskDWord)));		
			ODM_SetRFReg(pDM_Odm, path, RF_AC, bMaskDWord, APK_RF_value_0[path][index]);
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("phy_APCalibrate_8814A() offset 0x0 %x\n", ODM_GetRFReg(pDM_Odm, path, RF_AC, bMaskDWord)));		
			ODM_SetRFReg(pDM_Odm, path, RF_TXBIAS_A, bMaskDWord, tmpReg);
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0xd %x\n", ODM_GetRFReg(pDM_Odm, path, RF_TXBIAS_A, bMaskDWord)));					
#endif

			// PA11+PAD01111, one shot	
			i = 0;
			do
			{
				ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x80000000);
				{
					ODM_SetBBReg(pDM_Odm, APK_offset[path], bMaskDWord, APK_value[0]);		
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", APK_offset[path], ODM_GetBBReg(pDM_Odm, APK_offset[path], bMaskDWord)));
					ODM_delay_ms(3);				
					ODM_SetBBReg(pDM_Odm, APK_offset[path], bMaskDWord, APK_value[1]);
					ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0x%x value 0x%x\n", APK_offset[path], ODM_GetBBReg(pDM_Odm, APK_offset[path], bMaskDWord)));

					ODM_delay_ms(20);
				}
				ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);

				if(path == RF_PATH_A)
					tmpReg = ODM_GetBBReg(pDM_Odm, rAPK, 0x03E00000);
				else
					tmpReg = ODM_GetBBReg(pDM_Odm, rAPK, 0xF8000000);
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_APCalibrate_8814A() offset 0xbd8[25:21] %x\n", tmpReg));		


				i++;
			}
			while(tmpReg > apkbound && i < 4);

			APK_result[path][index] = tmpReg;
		}
	}

	//reload MAC default value	
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_ReloadMACRegisters_8814A(pAdapter, MAC_REG, MAC_backup);
#else
	_PHY_ReloadMACRegisters_8814A(pDM_Odm, MAC_REG, MAC_backup);
#endif

	//reload BB default value	
	for(index = 0; index < APK_BB_REG_NUM ; index++)
	{

		if(index == 0)		//skip 
			continue;					
		ODM_SetBBReg(pDM_Odm, BB_REG[index], bMaskDWord, BB_backup[index]);
	}

	//reload AFE default value
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_ReloadADDARegisters_8814A(pAdapter, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);
#else
	_PHY_ReloadADDARegisters_8814A(pDM_Odm, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);
#endif

	//reload RF path default value
	for(path = 0; path < pathbound; path++)
	{
		ODM_SetRFReg(pDM_Odm, path, 0xd, bMaskDWord, regD[path]);
		if(path == RF_PATH_B)
		{
			ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_MODE1, bMaskDWord, 0x1000f);			
			ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_MODE2, bMaskDWord, 0x20101);						
		}

		//note no index == 0
		if (APK_result[path][1] > 6)
			APK_result[path][1] = 6;
		ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("apk path %d result %d 0x%x \t", path, 1, APK_result[path][1]));					
	}

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("\n"));


	for(path = 0; path < pathbound; path++)
	{
		ODM_SetRFReg(pDM_Odm, path, 0x3, bMaskDWord, 
				((APK_result[path][1] << 15) | (APK_result[path][1] << 10) | (APK_result[path][1] << 5) | APK_result[path][1]));
		if(path == RF_PATH_A)
			ODM_SetRFReg(pDM_Odm, path, 0x4, bMaskDWord, 
					((APK_result[path][1] << 15) | (APK_result[path][1] << 10) | (0x00 << 5) | 0x05));		
		else
			ODM_SetRFReg(pDM_Odm, path, 0x4, bMaskDWord, 
					((APK_result[path][1] << 15) | (APK_result[path][1] << 10) | (0x02 << 5) | 0x05));								
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			ODM_SetRFReg(pDM_Odm, path, RF_BS_PA_APSET_G9_G11, bMaskDWord, 
					((0x08 << 15) | (0x08 << 10) | (0x08 << 5) | 0x08));			
#endif		
	}

	pDM_Odm->RFCalibrateInfo.bAPKdone = TRUE;

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("<==phy_APCalibrate_8814A()\n"));
}






VOID
PHY_LCCalibrate_8814A(
		IN PDM_ODM_T		pDM_Odm
		)
{
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("===> PHY_LCCalibrate_8814A\n"));
	phy_LCCalibrate_8814A(pDM_Odm, TRUE);
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("<=== PHY_LCCalibrate_8814A\n"));
}

VOID
	PHY_APCalibrate_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	s1Byte 		delta	
			)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif	
#ifdef DISABLE_BB_RF
	return;
#endif

	return;
#if (DM_ODM_SUPPORT_TYPE & (ODM_CE|ODM_AP))
	if(!(pDM_Odm->SupportAbility & ODM_RF_CALIBRATION))
	{
		return;
	}
#endif	

#if defined(FOR_BRAZIL_PRETEST) && (FOR_BRAZIL_PRETEST != 1)
	if(pDM_Odm->RFCalibrateInfo.bAPKdone)
#endif		
		return;

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	if(IS_92C_SERIAL( pHalData->VersionID)){
		phy_APCalibrate_8814A(pAdapter, delta, TRUE);
	}
	else
#endif
	{
		// For 88C 1T1R
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
		phy_APCalibrate_8814A(pAdapter, delta, FALSE);
#else
		phy_APCalibrate_8814A(pDM_Odm, delta, FALSE);
#endif
	}
}
	VOID phy_SetRFPathSwitch_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	BOOLEAN		bMain,
			IN	BOOLEAN		is2T
			)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#elif (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	if(!pAdapter->bHWInitReady)	
#elif  (DM_ODM_SUPPORT_TYPE == ODM_CE)
		if(pAdapter->hw_init_completed == _FALSE)
#endif
		{
			u1Byte	u1bTmp;
			u1bTmp = ODM_Read1Byte(pDM_Odm, REG_LEDCFG2) | BIT7;
			ODM_Write1Byte(pDM_Odm, REG_LEDCFG2, u1bTmp);
			//ODM_SetBBReg(pDM_Odm, REG_LEDCFG0, BIT23, 0x01);
			ODM_SetBBReg(pDM_Odm, rFPGA0_XAB_RFParameter, BIT13, 0x01);
		}

#endif		

	if(is2T)	//92C
	{
		if(bMain)
			ODM_SetBBReg(pDM_Odm, rFPGA0_XB_RFInterfaceOE, BIT5|BIT6, 0x1);	//92C_Path_A			
		else
			ODM_SetBBReg(pDM_Odm, rFPGA0_XB_RFInterfaceOE, BIT5|BIT6, 0x2);	//BT							
	}
	else			//88C
	{

		if(bMain)
			ODM_SetBBReg(pDM_Odm, rFPGA0_XA_RFInterfaceOE, BIT8|BIT9, 0x2);	//Main
		else
			ODM_SetBBReg(pDM_Odm, rFPGA0_XA_RFInterfaceOE, BIT8|BIT9, 0x1);	//Aux		
	}	
}
	VOID PHY_SetRFPathSwitch_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	BOOLEAN		bMain
			)
{
	//HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

#ifdef DISABLE_BB_RF
	return;
#endif

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	if (IS_92C_SERIAL(pHalData->VersionID))
	{
		phy_SetRFPathSwitch_8814A(pAdapter, bMain, TRUE);
	}
	else
#endif		
	{
		// For 88C 1T1R
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
		phy_SetRFPathSwitch_8814A(pAdapter, bMain, FALSE);
#else
		phy_SetRFPathSwitch_8814A(pDM_Odm, bMain, FALSE);
#endif
	}
}


#define		DP_BB_REG_NUM		7
#define		DP_RF_REG_NUM		1
#define		DP_RETRY_LIMIT		10
#define		DP_PATH_NUM		2
#define		DP_DPK_NUM			3
#define		DP_DPK_VALUE_NUM	2




#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
//digital predistortion
VOID	
	phy_DigitalPredistortion_8814A(
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN	PADAPTER	pAdapter,
#else
			IN PDM_ODM_T	pDM_Odm,
#endif
			IN	BOOLEAN		is2T
			)
{
#if (RT_PLATFORM == PLATFORM_WINDOWS)
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif	

	u4Byte			tmpReg, tmpReg2, index,  i;		
	u1Byte			path, pathbound = PATH_NUM;
	u4Byte			AFE_backup[IQK_ADDA_REG_NUM];
	u4Byte			AFE_REG[IQK_ADDA_REG_NUM] = {	
		rFPGA0_XCD_SwitchControl, 	rBlue_Tooth, 	
		rRx_Wait_CCA, 		rTx_CCK_RFON,
		rTx_CCK_BBON, 	rTx_OFDM_RFON, 	
		rTx_OFDM_BBON, 	rTx_To_Rx,
		rTx_To_Tx, 		rRx_CCK, 	
		rRx_OFDM, 		rRx_Wait_RIFS,
		rRx_TO_Rx, 		rStandby, 	
		rSleep, 			rPMPD_ANAEN };

	u4Byte			BB_backup[DP_BB_REG_NUM];	
	u4Byte			BB_REG[DP_BB_REG_NUM] = {
		rOFDM0_TRxPathEnable, rFPGA0_RFMOD, 
		rOFDM0_TRMuxPar, 	rFPGA0_XCD_RFInterfaceSW,
		rFPGA0_XAB_RFInterfaceSW, rFPGA0_XA_RFInterfaceOE, 
		rFPGA0_XB_RFInterfaceOE};						
	u4Byte			BB_settings[DP_BB_REG_NUM] = {
		0x00a05430, 0x02040000, 0x000800e4, 0x22208000, 
		0x0, 0x0, 0x0};	

	u4Byte			RF_backup[DP_PATH_NUM][DP_RF_REG_NUM];
	u4Byte			RF_REG[DP_RF_REG_NUM] = {
		RF_TXBIAS_A};

	u4Byte			MAC_backup[IQK_MAC_REG_NUM];
	u4Byte			MAC_REG[IQK_MAC_REG_NUM] = {
		REG_TXPAUSE, 		REG_BCN_CTRL,	
		REG_BCN_CTRL_1,	REG_GPIO_MUXCFG};

	u4Byte			Tx_AGC[DP_DPK_NUM][DP_DPK_VALUE_NUM] = {
		{0x1e1e1e1e, 0x03901e1e},
		{0x18181818, 0x03901818},
		{0x0e0e0e0e, 0x03900e0e}
	};

	u4Byte			AFE_on_off[PATH_NUM] = {
		0x04db25a4, 0x0b1b25a4};	//path A on path B off / path A off path B on

	u1Byte			RetryCount = 0;


	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==>phy_DigitalPredistortion_8814A()\n"));

	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("phy_DigitalPredistortion_8814A for %s %s\n", (is2T ? "2T2R" : "1T1R")));

	//save BB default value
	for(index=0; index<DP_BB_REG_NUM; index++)
		BB_backup[index] = ODM_GetBBReg(pDM_Odm, BB_REG[index], bMaskDWord);

	//save MAC default value
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_SaveMACRegisters_8814A(pAdapter, BB_REG, MAC_backup);
#else
	_PHY_SaveMACRegisters_8814A(pDM_Odm, BB_REG, MAC_backup);
#endif	

	//save RF default value
	for(path=0; path<DP_PATH_NUM; path++)
	{
		for(index=0; index<DP_RF_REG_NUM; index++)
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			RF_backup[path][index] = PHY_QueryRFReg(pAdapter, path, RF_REG[index], bMaskDWord);	
#else
		RF_backup[path][index] = ODM_GetRFReg(pAdapter, path, RF_REG[index], bMaskDWord);	
#endif	
	}	

	//save AFE default value
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_SaveADDARegisters_8814A(pAdapter, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);
#else
	RF_backup[path][index] = ODM_GetRFReg(pAdapter, path, RF_REG[index], bMaskDWord);	
#endif	

	//Path A/B AFE all on
	for(index = 0; index < IQK_ADDA_REG_NUM ; index++)
		ODM_SetBBReg(pDM_Odm, AFE_REG[index], bMaskDWord, 0x6fdb25a4);

	//BB register setting
	for(index = 0; index < DP_BB_REG_NUM; index++)
	{
		if(index < 4)
			ODM_SetBBReg(pDM_Odm, BB_REG[index], bMaskDWord, BB_settings[index]);
		else if (index == 4)
			ODM_SetBBReg(pDM_Odm,BB_REG[index], bMaskDWord, BB_backup[index]|BIT10|BIT26);			
		else
			ODM_SetBBReg(pDM_Odm, BB_REG[index], BIT10, 0x00);			
	}

	//MAC register setting
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_MACSettingCalibration_8814A(pAdapter, MAC_REG, MAC_backup);
#else
	_PHY_MACSettingCalibration_8814A(pDM_Odm, MAC_REG, MAC_backup);
#endif	

	//PAGE-E IQC setting	
	ODM_SetBBReg(pDM_Odm, rTx_IQK_Tone_A, bMaskDWord, 0x01008c00); 		
	ODM_SetBBReg(pDM_Odm, rRx_IQK_Tone_A, bMaskDWord, 0x01008c00);	
	ODM_SetBBReg(pDM_Odm, rTx_IQK_Tone_B, bMaskDWord, 0x01008c00);	
	ODM_SetBBReg(pDM_Odm, rRx_IQK_Tone_B, bMaskDWord, 0x01008c00);	

	//path_A DPK
	//Path B to standby mode
	ODM_SetRFReg(pDM_Odm, RF_PATH_B, RF_AC, bMaskDWord, 0x10000);

	// PA gain = 11 & PAD1 => tx_agc 1f ~11
	// PA gain = 11 & PAD2 => tx_agc 10~0e
	// PA gain = 01 => tx_agc 0b~0d
	// PA gain = 00 => tx_agc 0a~00
	ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x40000000);	
	ODM_SetBBReg(pDM_Odm, 0xbc0, bMaskDWord, 0x0005361f);		
	ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);	

	//do inner loopback DPK 3 times 
	for(i = 0; i < 3; i++)
	{
		//PA gain = 11 & PAD2 => tx_agc = 0x0f/0x0c/0x07
		for(index = 0; index < 3; index++)
			ODM_SetBBReg(pDM_Odm, 0xe00+index*4, bMaskDWord, Tx_AGC[i][0]);			
		ODM_SetBBReg(pDM_Odm,0xe00+index*4, bMaskDWord, Tx_AGC[i][1]);			
		for(index = 0; index < 4; index++)
			ODM_SetBBReg(pDM_Odm,0xe10+index*4, bMaskDWord, Tx_AGC[i][0]);			

		// PAGE_B for Path-A inner loopback DPK setting
		ODM_SetBBReg(pDM_Odm,rPdp_AntA, bMaskDWord, 0x02097098);
		ODM_SetBBReg(pDM_Odm,rPdp_AntA_4, bMaskDWord, 0xf76d9f84);
		ODM_SetBBReg(pDM_Odm,rConfig_Pmpd_AntA, bMaskDWord, 0x0004ab87);
		ODM_SetBBReg(pDM_Odm,rConfig_AntA, bMaskDWord, 0x00880000);		

		//----send one shot signal----//
		// Path A
		ODM_SetBBReg(pDM_Odm,rConfig_Pmpd_AntA, bMaskDWord, 0x80047788);
		ODM_delay_ms(1);
		ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntA, bMaskDWord, 0x00047788);
		ODM_delay_ms(50);
	}

	//PA gain = 11 => tx_agc = 1a
	for(index = 0; index < 3; index++)		
		ODM_SetBBReg(pDM_Odm,0xe00+index*4, bMaskDWord, 0x34343434);	
	ODM_SetBBReg(pDM_Odm,0xe08+index*4, bMaskDWord, 0x03903434);	
	for(index = 0; index < 4; index++)		
		ODM_SetBBReg(pDM_Odm,0xe10+index*4, bMaskDWord, 0x34343434);	

	//====================================
	// PAGE_B for Path-A DPK setting
	//====================================
	// open inner loopback @ b00[19]:10 od 0xb00 0x01097018
	ODM_SetBBReg(pDM_Odm,rPdp_AntA, bMaskDWord, 0x02017098);
	ODM_SetBBReg(pDM_Odm,rPdp_AntA_4, bMaskDWord, 0xf76d9f84);
	ODM_SetBBReg(pDM_Odm,rConfig_Pmpd_AntA, bMaskDWord, 0x0004ab87);
	ODM_SetBBReg(pDM_Odm,rConfig_AntA, bMaskDWord, 0x00880000);		

	//rf_lpbk_setup
	//1.rf 00:5205a, rf 0d:0e52c
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, 0x0c, bMaskDWord, 0x8992b);
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, 0x0d, bMaskDWord, 0x0e52c); 	
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, 0x00, bMaskDWord, 0x5205a );		

	//----send one shot signal----//
	// Path A
	ODM_SetBBReg(pDM_Odm,rConfig_Pmpd_AntA, bMaskDWord, 0x800477c0);
	ODM_delay_ms(1);
	ODM_SetBBReg(pDM_Odm,rConfig_Pmpd_AntA, bMaskDWord, 0x000477c0);
	ODM_delay_ms(50);

	while(RetryCount < DP_RETRY_LIMIT && !pDM_Odm->RFCalibrateInfo.bDPPathAOK)
	{
		//----read back measurement results----//
		ODM_SetBBReg(pDM_Odm, rPdp_AntA, bMaskDWord, 0x0c297018);
		tmpReg = ODM_GetBBReg(pDM_Odm, 0xbe0, bMaskDWord);
		ODM_delay_ms(10);
		ODM_SetBBReg(pDM_Odm, rPdp_AntA, bMaskDWord, 0x0c29701f);
		tmpReg2 = ODM_GetBBReg(pDM_Odm, 0xbe8, bMaskDWord);
		ODM_delay_ms(10);

		tmpReg = (tmpReg & bMaskHWord) >> 16;
		tmpReg2 = (tmpReg2 & bMaskHWord) >> 16;		
		if(tmpReg < 0xf0 || tmpReg > 0x105 || tmpReg2 > 0xff )
		{
			ODM_SetBBReg(pDM_Odm, rPdp_AntA, bMaskDWord, 0x02017098);

			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x80000000);
			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);
			ODM_delay_ms(1);
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntA, bMaskDWord, 0x800477c0);
			ODM_delay_ms(1);			
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntA, bMaskDWord, 0x000477c0);			
			ODM_delay_ms(50);			
			RetryCount++;			
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("path A DPK RetryCount %d 0xbe0[31:16] %x 0xbe8[31:16] %x\n", RetryCount, tmpReg, tmpReg2));										
		}
		else
		{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("path A DPK Sucess\n"));		
			pDM_Odm->RFCalibrateInfo.bDPPathAOK = TRUE;
			break;
		}		
	}
	RetryCount = 0;

	//DPP path A
	if(pDM_Odm->RFCalibrateInfo.bDPPathAOK)
	{	
		// DP settings
		ODM_SetBBReg(pDM_Odm, rPdp_AntA, bMaskDWord, 0x01017098);
		ODM_SetBBReg(pDM_Odm, rPdp_AntA_4, bMaskDWord, 0x776d9f84);
		ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntA, bMaskDWord, 0x0004ab87);
		ODM_SetBBReg(pDM_Odm, rConfig_AntA, bMaskDWord, 0x00880000);
		ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x40000000);

		for(i=rPdp_AntA; i<=0xb3c; i+=4)
		{
			ODM_SetBBReg(pDM_Odm, i, bMaskDWord, 0x40004000);	
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("path A ofsset = 0x%x\n", i));		
		}

		//pwsf
		ODM_SetBBReg(pDM_Odm, 0xb40, bMaskDWord, 0x40404040);	
		ODM_SetBBReg(pDM_Odm, 0xb44, bMaskDWord, 0x28324040);			
		ODM_SetBBReg(pDM_Odm, 0xb48, bMaskDWord, 0x10141920);					

		for(i=0xb4c; i<=0xb5c; i+=4)
		{
			ODM_SetBBReg(pDM_Odm, i, bMaskDWord, 0x0c0c0c0c);	
		}		

		//TX_AGC boundary
		ODM_SetBBReg(pDM_Odm, 0xbc0, bMaskDWord, 0x0005361f);	
		ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);					
	}
	else
	{
		ODM_SetBBReg(pDM_Odm, rPdp_AntA, bMaskDWord, 0x00000000);	
		ODM_SetBBReg(pDM_Odm, rPdp_AntA_4, bMaskDWord, 0x00000000);			
	}

	//DPK path B
	if(is2T)
	{
		//Path A to standby mode
		ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_AC, bMaskDWord, 0x10000);

		// LUTs => tx_agc
		// PA gain = 11 & PAD1, => tx_agc 1f ~11
		// PA gain = 11 & PAD2, => tx_agc 10 ~0e
		// PA gain = 01 => tx_agc 0b ~0d
		// PA gain = 00 => tx_agc 0a ~00
		ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x40000000);	
		ODM_SetBBReg(pDM_Odm, 0xbc4, bMaskDWord, 0x0005361f);		
		ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);	

		//do inner loopback DPK 3 times 
		for(i = 0; i < 3; i++)
		{
			//PA gain = 11 & PAD2 => tx_agc = 0x0f/0x0c/0x07
			for(index = 0; index < 4; index++)
				ODM_SetBBReg(pDM_Odm, 0x830+index*4, bMaskDWord, Tx_AGC[i][0]);			
			for(index = 0; index < 2; index++)
				ODM_SetBBReg(pDM_Odm, 0x848+index*4, bMaskDWord, Tx_AGC[i][0]);			
			for(index = 0; index < 2; index++)
				ODM_SetBBReg(pDM_Odm, 0x868+index*4, bMaskDWord, Tx_AGC[i][0]);			

			// PAGE_B for Path-A inner loopback DPK setting
			ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x02097098);
			ODM_SetBBReg(pDM_Odm, rPdp_AntB_4, bMaskDWord, 0xf76d9f84);
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x0004ab87);
			ODM_SetBBReg(pDM_Odm, rConfig_AntB, bMaskDWord, 0x00880000);		

			//----send one shot signal----//
			// Path B
			ODM_SetBBReg(pDM_Odm,rConfig_Pmpd_AntB, bMaskDWord, 0x80047788);
			ODM_delay_ms(1);
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x00047788);
			ODM_delay_ms(50);
		}

		// PA gain = 11 => tx_agc = 1a	
		for(index = 0; index < 4; index++)
			ODM_SetBBReg(pDM_Odm, 0x830+index*4, bMaskDWord, 0x34343434);	
		for(index = 0; index < 2; index++)
			ODM_SetBBReg(pDM_Odm, 0x848+index*4, bMaskDWord, 0x34343434);	
		for(index = 0; index < 2; index++)
			ODM_SetBBReg(pDM_Odm, 0x868+index*4, bMaskDWord, 0x34343434);	

		// PAGE_B for Path-B DPK setting
		ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x02017098);		
		ODM_SetBBReg(pDM_Odm, rPdp_AntB_4, bMaskDWord, 0xf76d9f84);		
		ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x0004ab87);		
		ODM_SetBBReg(pDM_Odm, rConfig_AntB, bMaskDWord, 0x00880000);		

		// RF lpbk switches on
		ODM_SetBBReg(pDM_Odm, 0x840, bMaskDWord, 0x0101000f);		
		ODM_SetBBReg(pDM_Odm, 0x840, bMaskDWord, 0x01120103);		

		//Path-B RF lpbk
		ODM_SetRFReg(pDM_Odm, RF_PATH_B, 0x0c, bMaskDWord, 0x8992b);
		ODM_SetRFReg(pDM_Odm, RF_PATH_B, 0x0d, bMaskDWord, 0x0e52c);
		ODM_SetRFReg(pDM_Odm, RF_PATH_B, RF_AC, bMaskDWord, 0x5205a); 

		//----send one shot signal----//
		ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x800477c0);		
		ODM_delay_ms(1);	
		ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x000477c0);		
		ODM_delay_ms(50);

		while(RetryCount < DP_RETRY_LIMIT && !pDM_Odm->RFCalibrateInfo.bDPPathBOK)
		{
			//----read back measurement results----//
			ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x0c297018);		
			tmpReg = ODM_GetBBReg(pDM_Odm, 0xbf0, bMaskDWord);
			ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x0c29701f);		
			tmpReg2 = ODM_GetBBReg(pDM_Odm, 0xbf8, bMaskDWord);

			tmpReg = (tmpReg & bMaskHWord) >> 16;
			tmpReg2 = (tmpReg2 & bMaskHWord) >> 16;

			if(tmpReg < 0xf0 || tmpReg > 0x105 || tmpReg2 > 0xff)
			{
				ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x02017098);		

				ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x80000000);
				ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);
				ODM_delay_ms(1);
				ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x800477c0);		
				ODM_delay_ms(1);	
				ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x000477c0);		
				ODM_delay_ms(50);			
				RetryCount++;			
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD,  ("path B DPK RetryCount %d 0xbf0[31:16] %x, 0xbf8[31:16] %x\n", RetryCount , tmpReg, tmpReg2));														
			}
			else
			{
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("path B DPK Success\n"));						
				pDM_Odm->RFCalibrateInfo.bDPPathBOK = TRUE;
				break;
			}						
		}

		//DPP path B
		if(pDM_Odm->RFCalibrateInfo.bDPPathBOK)
		{
			// DP setting
			// LUT by SRAM
			ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x01017098);
			ODM_SetBBReg(pDM_Odm, rPdp_AntB_4, bMaskDWord, 0x776d9f84);
			ODM_SetBBReg(pDM_Odm, rConfig_Pmpd_AntB, bMaskDWord, 0x0004ab87);
			ODM_SetBBReg(pDM_Odm, rConfig_AntB, bMaskDWord, 0x00880000);

			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x40000000);
			for(i=0xb60; i<=0xb9c; i+=4)
			{
				ODM_SetBBReg(pDM_Odm, i, bMaskDWord, 0x40004000);	
				ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("path B ofsset = 0x%x\n", i));
			}

			// PWSF
			ODM_SetBBReg(pDM_Odm, 0xba0, bMaskDWord, 0x40404040);	
			ODM_SetBBReg(pDM_Odm, 0xba4, bMaskDWord, 0x28324050);			
			ODM_SetBBReg(pDM_Odm, 0xba8, bMaskDWord, 0x0c141920);					

			for(i=0xbac; i<=0xbbc; i+=4)
			{
				ODM_SetBBReg(pDM_Odm, i, bMaskDWord, 0x0c0c0c0c);	
			}		

			// tx_agc boundary
			ODM_SetBBReg(pDM_Odm, 0xbc4, bMaskDWord, 0x0005361f);	
			ODM_SetBBReg(pDM_Odm, rFPGA0_IQK, bMaskDWord, 0x00000000);			

		}
		else
		{
			ODM_SetBBReg(pDM_Odm, rPdp_AntB, bMaskDWord, 0x00000000);	
			ODM_SetBBReg(pDM_Odm, rPdp_AntB_4, bMaskDWord, 0x00000000);					
		}
	}

	//reload BB default value
	for(index=0; index<DP_BB_REG_NUM; index++)
		ODM_SetBBReg(pDM_Odm, BB_REG[index], bMaskDWord, BB_backup[index]);

	//reload RF default value
	for(path = 0; path<DP_PATH_NUM; path++)
	{
		for( i = 0 ; i < DP_RF_REG_NUM ; i++){
			ODM_SetRFReg(pDM_Odm, path, RF_REG[i], bMaskDWord, RF_backup[path][i]);
		}
	}
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_MODE1, bMaskDWord, 0x1000f);	//standby mode
	ODM_SetRFReg(pDM_Odm, RF_PATH_A, RF_MODE2, bMaskDWord, 0x20101);		//RF lpbk switches off

	//reload AFE default value
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	_PHY_ReloadADDARegisters_8814A(pAdapter, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);	

	//reload MAC default value	
	_PHY_ReloadMACRegisters_8814A(pAdapter, MAC_REG, MAC_backup);
#else
	_PHY_ReloadADDARegisters_8814A(pDM_Odm, AFE_REG, AFE_backup, IQK_ADDA_REG_NUM);	

	//reload MAC default value	
	_PHY_ReloadMACRegisters_8814A(pDM_Odm, MAC_REG, MAC_backup);
#endif		

	pDM_Odm->RFCalibrateInfo.bDPdone = TRUE;
	ODM_RT_TRACE(pDM_Odm,ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("<==phy_DigitalPredistortion_8814A()\n"));
#endif		
}

VOID
	phy_DigitalPredistortion_8814A_8814A(
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN	PADAPTER	pAdapter
#else
			IN PDM_ODM_T	pDM_Odm
#endif		
			)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif	
#if DISABLE_BB_RF
	return;
#endif

	return;

	if(pDM_Odm->RFCalibrateInfo.bDPdone)
		return;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)

	if(IS_92C_SERIAL( pHalData->VersionID)){
		phy_DigitalPredistortion_8814A(pAdapter, TRUE);
	}
	else
#endif		
	{
		// For 88C 1T1R
		phy_DigitalPredistortion_8814A(pAdapter, FALSE);
	}
}



//return value TRUE => Main; FALSE => Aux

	BOOLEAN phy_QueryRFPathSwitch_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	BOOLEAN		is2T
			)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PDM_ODM_T		pDM_Odm = &pHalData->DM_OutSrc;
#endif
#endif	
	if(!pAdapter->bHWInitReady)
	{
		u1Byte	u1bTmp;
		u1bTmp = ODM_Read1Byte(pDM_Odm, REG_LEDCFG2) | BIT7;
		ODM_Write1Byte(pDM_Odm, REG_LEDCFG2, u1bTmp);
		//ODM_SetBBReg(pDM_Odm, REG_LEDCFG0, BIT23, 0x01);
		ODM_SetBBReg(pDM_Odm, rFPGA0_XAB_RFParameter, BIT13, 0x01);
	}

	if(is2T)		//
	{
		if(ODM_GetBBReg(pDM_Odm, rFPGA0_XB_RFInterfaceOE, BIT5|BIT6) == 0x01)
			return TRUE;
		else 
			return FALSE;
	}
	else
	{
		if((ODM_GetBBReg(pDM_Odm, rFPGA0_XB_RFInterfaceOE, BIT5|BIT4|BIT3) == 0x0) ||
				(ODM_GetBBReg(pDM_Odm, rConfig_ram64x16, BIT31) == 0x0))
			return TRUE;
		else 
			return FALSE;
	}
}



//return value TRUE => Main; FALSE => Aux
	BOOLEAN PHY_QueryRFPathSwitch_8814A(	
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm
#else
			IN	PADAPTER	pAdapter
#endif
			)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

#if DISABLE_BB_RF
	return TRUE;
#endif
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)

	//if(IS_92C_SERIAL( pHalData->VersionID)){
	if(IS_2T2R( pHalData->VersionID)){
		return phy_QueryRFPathSwitch_8814A(pAdapter, TRUE);
	}
	else
#endif		
	{
		// For 88C 1T1R
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
		return phy_QueryRFPathSwitch_8814A(pAdapter, FALSE);
#else
		return phy_QueryRFPathSwitch_8814A(pDM_Odm, FALSE);
#endif
	}
}
#endif


