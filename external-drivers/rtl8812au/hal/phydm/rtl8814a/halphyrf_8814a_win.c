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

#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8814A_SUPPORT == 1)


/*---------------------------Define Local Constant---------------------------*/
// 2010/04/25 MH Define the max tx power tracking tx agc power.
#define		ODM_TXPWRTRACK_MAX_IDX_8814A		6

/*---------------------------Define Local Constant---------------------------*/

//3============================================================
//3 Tx Power Tracking
//3============================================================

// Add CheckRFGainOffset By YuChen to make sure that RF gain offset will not over upperbound 4'b1010

u1Byte
CheckRFGainOffset(
	PDM_ODM_T			pDM_Odm,
	u1Byte				RFPath
	)
{
	u1Byte	UpperBound = 10; // 4'b1010 = 10
	u1Byte	Final_RF_Index = 0;
	BOOLEAN	bPositive = FALSE;
	PODM_RF_CAL_T	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);

	if( pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] >= 0)				// check if RF_Index is positive or not
	{
		Final_RF_Index = pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] >> 1;
		bPositive = TRUE;
		ODM_SetRFReg(pDM_Odm, (ODM_RF_RADIO_PATH_E)RFPath, rRF_TxGainOffset, BIT15, bPositive);
	}
	else
	{
		Final_RF_Index = (-1)*pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] >> 1;
		bPositive = FALSE;
		ODM_SetRFReg(pDM_Odm, (ODM_RF_RADIO_PATH_E)RFPath, rRF_TxGainOffset, BIT15, bPositive);
	}

	if(bPositive == TRUE)
	{
		if(Final_RF_Index >= UpperBound)
		{
			ODM_SetRFReg(pDM_Odm, (ODM_RF_RADIO_PATH_E)RFPath, rRF_TxGainOffset, 0xF0000, UpperBound);	//set RF Reg0x55 per path
			return UpperBound;
		}
		else
		{
			ODM_SetRFReg(pDM_Odm, (ODM_RF_RADIO_PATH_E)RFPath, rRF_TxGainOffset, 0xF0000, Final_RF_Index);	//set RF Reg0x55 per path
			return Final_RF_Index;
		}
	}
	else
	{
		ODM_SetRFReg(pDM_Odm, (ODM_RF_RADIO_PATH_E)RFPath, rRF_TxGainOffset, 0xF0000, Final_RF_Index);	//set RF Reg0x55 per path	
		return Final_RF_Index;
		
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
		u1Byte			Final_OFDM_Swing_Index = 0; 
		u1Byte			Final_CCK_Swing_Index = 0;
		u1Byte			Final_RF_Index = 0;
		u1Byte			UpperBound = 10, TxScalingUpperBound = 28; // Upperbound = 4'b1010, TxScalingUpperBound = +2 dB
		PODM_RF_CAL_T	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);

		
		if (Method == MIX_MODE)
		{
			ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,("pRFCalibrateInfo->DefaultOfdmIndex=%d, pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath]=%d, RF_Path = %d\n",
				pRFCalibrateInfo->DefaultOfdmIndex, pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath], RFPath));
			
			Final_CCK_Swing_Index = pRFCalibrateInfo->DefaultCckIndex + pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath];	
			Final_OFDM_Swing_Index = pRFCalibrateInfo->DefaultOfdmIndex + (pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath])%2;
	
			Final_RF_Index = CheckRFGainOffset(pDM_Odm, RFPath);		// check if Final_RF_Index >= 10

			if((Final_RF_Index == UpperBound) && (pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] >= 0))		// check BBSW is not over +2dB
			{
				Final_OFDM_Swing_Index = pRFCalibrateInfo->DefaultOfdmIndex + (pRFCalibrateInfo->Absolute_OFDMSwingIdx[RFPath] - (UpperBound << 1));
				if(Final_OFDM_Swing_Index > TxScalingUpperBound)
					Final_OFDM_Swing_Index = TxScalingUpperBound;
			}
			
		switch(RFPath)
				{
					case ODM_RF_PATH_A:
						
						ODM_SetBBReg(pDM_Odm, rA_TxScale_Jaguar, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

						ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
							("******Path_A Compensate with BBSwing , Final_OFDM_Swing_Index = %d, Final_RF_Index = %d \n", Final_OFDM_Swing_Index, Final_RF_Index));
						break;

					case ODM_RF_PATH_B:
						
						ODM_SetBBReg(pDM_Odm, rB_TxScale_Jaguar, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

						ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
							("******Path_B Compensate with BBSwing , Final_OFDM_Swing_Index = %d, Final_RF_Index = %d \n", Final_OFDM_Swing_Index, Final_RF_Index));
						break;

					case ODM_RF_PATH_C:
						
						ODM_SetBBReg(pDM_Odm, rC_TxScale_Jaguar2, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

						ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
							("******Path_C Compensate with BBSwing , Final_OFDM_Swing_Index = %d, Final_RF_Index = %d \n", Final_OFDM_Swing_Index, Final_RF_Index));
				            	break;

					case ODM_RF_PATH_D:
						
						ODM_SetBBReg(pDM_Odm, rD_TxScale_Jaguar2, 0xFFE00000, TxScalingTable_Jaguar[Final_OFDM_Swing_Index]);	//set BBswing

						ODM_RT_TRACE(pDM_Odm,ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
							("******Path_D Compensate with BBSwing , Final_OFDM_Swing_Index = %d, Final_RF_Index = %d \n", Final_OFDM_Swing_Index, Final_RF_Index));
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
    PADAPTER        Adapter   		 = pDM_Odm->Adapter;
	PODM_RF_CAL_T  	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);
	HAL_DATA_TYPE  	*pHalData  		 = GET_HAL_DATA(Adapter);
	u1Byte		TxRate			= 0xFF;
	u1Byte         	channel   		 = pHalData->CurrentChannel;

	
	if (pDM_Odm->mp_mode == TRUE) {
	#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			#if (MP_DRIVER == 1)
					PMPT_CONTEXT pMptCtx = &(Adapter->MptCtx);
					
					TxRate = MptToMgntRate(pMptCtx->MptRateIndex);
			#endif
		#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
				PMPT_CONTEXT pMptCtx = &(Adapter->mppriv.MptCtx);
				
				TxRate = MptToMgntRate(pMptCtx->MptRateIndex);
		#endif	
	#endif
	} else {
		u2Byte	rate	 = *(pDM_Odm->pForcedDataRate);
		
		if (!rate) { /*auto rate*/
			if (rate != 0xFF) {
			#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
						TxRate = Adapter->HalFunc.GetHwRateFromMRateHandler(pDM_Odm->TxRate);
			#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
						TxRate = HwRateToMRate(pDM_Odm->TxRate);
			#endif
			}
		} else { /*force rate*/
			TxRate = (u1Byte)rate;
		}
	}
		
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD, ("Power Tracking TxRate=0x%X\n", TxRate));

	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(TxRate)) {
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
	} else if (36 <= channel && channel <= 64) {
		*TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_P[0];
		*TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_N[0];
		*TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_P[0];
		*TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_N[0];
	} else if (100 <= channel && channel <= 144) {
		*TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_P[1];
		*TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_N[1];
		*TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_P[1];
		*TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_N[1];
	} else if (149 <= channel && channel <= 173) {
		*TemperatureUP_A   = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_P[2]; 
		*TemperatureDOWN_A = pRFCalibrateInfo->DeltaSwingTableIdx_5GA_N[2]; 
		*TemperatureUP_B   = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_P[2]; 
		*TemperatureDOWN_B = pRFCalibrateInfo->DeltaSwingTableIdx_5GB_N[2]; 
	} else {
		*TemperatureUP_A   = (pu1Byte)DeltaSwingTableIdx_2GA_P_8188E;
		*TemperatureDOWN_A = (pu1Byte)DeltaSwingTableIdx_2GA_N_8188E;	
		*TemperatureUP_B   = (pu1Byte)DeltaSwingTableIdx_2GA_P_8188E;
		*TemperatureDOWN_B = (pu1Byte)DeltaSwingTableIdx_2GA_N_8188E;		
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
    PADAPTER        Adapter   		 = pDM_Odm->Adapter;
	PODM_RF_CAL_T  	pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);
	HAL_DATA_TYPE  	*pHalData  		 = GET_HAL_DATA(Adapter);
	u1Byte		TxRate			= 0xFF;
	u1Byte         	channel   		 = pHalData->CurrentChannel;

	if (pDM_Odm->mp_mode == TRUE) {
	#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			#if (MP_DRIVER == 1)
					PMPT_CONTEXT pMptCtx = &(Adapter->MptCtx);
					
					TxRate = MptToMgntRate(pMptCtx->MptRateIndex);
			#endif
		#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
				PMPT_CONTEXT pMptCtx = &(Adapter->mppriv.MptCtx);
				
				TxRate = MptToMgntRate(pMptCtx->MptRateIndex);
		#endif	
	#endif
	} else {
		u2Byte	rate	 = *(pDM_Odm->pForcedDataRate);
		
		if (!rate) { /*auto rate*/
			if (rate != 0xFF) {
			#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
						TxRate = Adapter->HalFunc.GetHwRateFromMRateHandler(pDM_Odm->TxRate);
			#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
						TxRate = HwRateToMRate(pDM_Odm->TxRate);
			#endif
			}
		} else { /*force rate*/
			TxRate = (u1Byte)rate;
		}
	}
		
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD, ("Power Tracking TxRate=0x%X\n", TxRate));


	if ( 1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(TxRate)) {
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
	} else if (36 <= channel && channel <= 64) {
		*TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_P[0];
		*TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_N[0];
		*TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_P[0];
		*TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_N[0];
	} else if (100 <= channel && channel <= 144) {
		*TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_P[1];
		*TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_N[1];
		*TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_P[1];
		*TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_N[1];
	} else if (149 <= channel && channel <= 173) {
		*TemperatureUP_C   = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_P[2]; 
		*TemperatureDOWN_C = pRFCalibrateInfo->DeltaSwingTableIdx_5GC_N[2]; 
		*TemperatureUP_D   = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_P[2]; 
		*TemperatureDOWN_D = pRFCalibrateInfo->DeltaSwingTableIdx_5GD_N[2]; 
	} else {
		*TemperatureUP_C   = (pu1Byte)DeltaSwingTableIdx_2GA_P_8188E;
		*TemperatureDOWN_C = (pu1Byte)DeltaSwingTableIdx_2GA_N_8188E;	
		*TemperatureUP_D   = (pu1Byte)DeltaSwingTableIdx_2GA_P_8188E;
		*TemperatureDOWN_D = (pu1Byte)DeltaSwingTableIdx_2GA_N_8188E;		
	}

	
	return;
}

void ConfigureTxpowerTrack_8814A(
	PTXPWRTRACK_CFG	pConfig
	)
{
	pConfig->SwingTableSize_CCK = CCK_TABLE_SIZE;
	pConfig->SwingTableSize_OFDM = OFDM_TABLE_SIZE;
	pConfig->Threshold_IQK = 8;
	pConfig->AverageThermalNum = AVG_THERMAL_NUM_8814A;
	pConfig->RfPathCount = MAX_PATH_NUM_8814A;
	pConfig->ThermalRegAddr = RF_T_METER_88E;
		
	pConfig->ODM_TxPwrTrackSetPwr = ODM_TxPwrTrackSetPwr8814A;
	pConfig->DoIQK = DoIQK_8814A;
	pConfig->PHY_LCCalibrate = PHY_LCCalibrate_8814A;
	pConfig->GetDeltaSwingTable = GetDeltaSwingTable_8814A;
	pConfig->GetDeltaSwingTable8814only = GetDeltaSwingTable_8814A_PathCD;
}

VOID	
phy_LCCalibrate_8814A(
	IN PDM_ODM_T		pDM_Odm,
	IN	BOOLEAN		is2T
	)
{
	u4Byte	LC_Cal = 0, cnt;
	
	//Check continuous TX and Packet TX
	u4Byte	reg0x914 = ODM_Read4Byte(pDM_Odm, rSingleTone_ContTx_Jaguar);;

	// Backup RF reg18.
	LC_Cal = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask);

	if((reg0x914 & 0x70000) == 0)
		ODM_Write1Byte(pDM_Odm, REG_TXPAUSE_8812A, 0xFF);			

	//3 3. Read RF reg18
	LC_Cal = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask);
	
	//3 4. Set LC calibration begin bit15
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask, LC_Cal|0x08000);

	ODM_delay_ms(100);		

	for (cnt = 0; cnt < 100; cnt++) {
		if (ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, 0x8000) != 0x1)
			break;	
		ODM_delay_ms(10);
	}
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("retry cnt = %d\n", cnt));



	//3 Restore original situation
	if((reg0x914 & 70000) == 0)
		ODM_Write1Byte(pDM_Odm, REG_TXPAUSE_8812A, 0x00);	

	// Recover channel number
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask, LC_Cal);

	DbgPrint("Call %s\n", __FUNCTION__);
}


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
}


VOID
PHY_LCCalibrate_8814A(
	IN PDM_ODM_T		pDM_Odm
	)
{
	
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	PADAPTER 		pAdapter = pDM_Odm->Adapter;
	
#if (MP_DRIVER == 1)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)	
	PMPT_CONTEXT	pMptCtx = &(pAdapter->MptCtx);
#else
	PMPT_CONTEXT	pMptCtx = &(pAdapter->mppriv.MptCtx);		
#endif	
#endif
#endif	

	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("===> PHY_LCCalibrate_8814A\n"));

//#if (MP_DRIVER == 1)	
	phy_LCCalibrate_8814A(pDM_Odm, TRUE);
//#endif 

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

}


VOID	                                                 
PHY_DPCalibrate_8814A(                                   
	IN 	PDM_ODM_T	pDM_Odm                             
	)
{
}


BOOLEAN 
phy_QueryRFPathSwitch_8814A(
	IN	PADAPTER	pAdapter
	)
{
	return TRUE;
}


BOOLEAN PHY_QueryRFPathSwitch_8814A(	
	IN	PADAPTER	pAdapter
	)
{

#if DISABLE_BB_RF
	return TRUE;
#endif

	return phy_QueryRFPathSwitch_8814A(pAdapter);
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
}




#else	/* (RTL8814A_SUPPORT == 0)*/
VOID
PHY_LCCalibrate_8814A(
	IN PDM_ODM_T		pDM_Odm
	){}

VOID
PHY_IQCalibrate_8814A(
	IN	PDM_ODM_T	pDM_Odm,
	IN	BOOLEAN bReCovery
	){}
#endif	/* (RTL8814A_SUPPORT == 0)*/
