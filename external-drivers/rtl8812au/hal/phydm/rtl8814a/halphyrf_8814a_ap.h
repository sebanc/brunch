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
#define	IQK_DELAY_TIME_8814A		10		//ms
#define	index_mapping_NUM_8814A	15
#define	AVG_THERMAL_NUM_8814A	4
#define	RF_T_METER_8814A 		0x42
#define	MAX_PATH_NUM_8814A	4

#include "../halphyrf_ap.h"


void ConfigureTxpowerTrack_8814A(
	PTXPWRTRACK_CFG	pConfig
	);

VOID
GetDeltaSwingTable_8814A(
	struct dm_struct    *pDM_Odm,
	u8* 			*TemperatureUP_A,
	u8* 			*TemperatureDOWN_A,
	u8* 			*TemperatureUP_B,
	u8* 			*TemperatureDOWN_B	
	);

VOID
GetDeltaSwingTable_8814A_PathCD(
	struct dm_struct    *pDM_Odm,
	u8* 			*TemperatureUP_C,
	u8* 			*TemperatureDOWN_C,
	u8* 			*TemperatureUP_D,
	u8* 			*TemperatureDOWN_D	
	);

VOID
ConfigureTxpowerTrack_8814A(
	IN PTXPWRTRACK_CFG	pConfig
	);


VOID
ODM_TxPwrTrackSetPwr8814A(
	IN PDM_ODM_T			pDM_Odm,
	IN PWRTRACK_METHOD 	Method,
	IN u1Byte 				RFPath,
	IN u1Byte 				ChannelMappedIndex
	);


u1Byte
CheckRFGainOffset(
	PDM_ODM_T			pDM_Odm,
	PWRTRACK_METHOD 	Method,
	u1Byte				RFPath
	);


//
// LC calibrate
//
void	
PHY_LCCalibrate_8814A(
	IN PDM_ODM_T		pDM_Odm
);

void
phy_LCCalibrate_8814A(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			IN PDM_ODM_T		pDM_Odm,
#else
			IN	PADAPTER	pAdapter,
#endif
			IN	BOOLEAN		is2T
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
							IN 	s1Byte		delta);
void	
PHY_DigitalPredistortion_8814A(		IN	PADAPTER	pAdapter);


#if 0 //FOR_8814_IQK
VOID
_PHY_SaveADDARegisters(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	IN PDM_ODM_T		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	IN	pu4Byte		ADDAReg,
	IN	pu4Byte		ADDABackup,
	IN	u4Byte		RegisterNum
	);

VOID
_PHY_PathADDAOn(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	IN PDM_ODM_T		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	IN	pu4Byte		ADDAReg,
	IN	BOOLEAN		isPathAOn,
	IN	BOOLEAN		is2T
	);

VOID
_PHY_MACSettingCalibration(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	IN PDM_ODM_T		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	IN	pu4Byte		MACReg,
	IN	pu4Byte		MACBackup	
	);



VOID
_PHY_PathAStandBy(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	IN PDM_ODM_T		pDM_Odm
#else
	IN	PADAPTER	pAdapter
#endif
	);

#endif

								
#endif	// #ifndef __HAL_PHY_RF_8814A_H__								

