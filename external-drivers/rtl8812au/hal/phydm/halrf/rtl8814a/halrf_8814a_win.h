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

#ifndef __HAL_PHY_RF_8814A_H__
#define __HAL_PHY_RF_8814A_H__

/*--------------------------Define Parameters-------------------------------*/
#define AVG_THERMAL_NUM_8814A	4

#include "halphyrf_win.h"

void ConfigureTxpowerTrack_8814A(
	PTXPWRTRACK_CFG	pConfig
	);

VOID
GetDeltaSwingTable_8814A(
	IN 	PDM_ODM_T			pDM_Odm,
	OUT pu1Byte 			*TemperatureUP_A,
	OUT pu1Byte 			*TemperatureDOWN_A,
	OUT pu1Byte 			*TemperatureUP_B,
	OUT pu1Byte 			*TemperatureDOWN_B	
	);

VOID
GetDeltaSwingTable_8814A_PathCD(
	IN 	PDM_ODM_T			pDM_Odm,
	OUT pu1Byte 			*TemperatureUP_C,
	OUT pu1Byte 			*TemperatureDOWN_C,
	OUT pu1Byte 			*TemperatureUP_D,
	OUT pu1Byte 			*TemperatureDOWN_D	
	);


VOID
ODM_TxPwrTrackSetPwr8814A(
	PDM_ODM_T			pDM_Odm,
	PWRTRACK_METHOD 	Method,
	u1Byte 				RFPath,
	u1Byte 				ChannelMappedIndex
	);

u1Byte
CheckRFGainOffset(
	PDM_ODM_T			pDM_Odm,
	u1Byte				RFPath
	);


//
// LC calibrate
//
void	
PHY_LCCalibrate_8814A(
	IN PDM_ODM_T		pDM_Odm
	);

//
// AP calibrate
//
void	
PHY_APCalibrate_8814A(		
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	IN PDM_ODM_T		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	IN 	s1Byte		delta
	);


VOID	                                                 
PHY_DPCalibrate_8814A(                                   
	IN 	PDM_ODM_T	pDM_Odm                             
	);


VOID PHY_SetRFPathSwitch_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	IN PDM_ODM_T		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	IN	BOOLEAN		bMain
	);

								
#endif	// #ifndef __HAL_PHY_RF_8188E_H__								

