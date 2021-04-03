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
#define RF_T_METER_8814A		0x42

#include "../halphyrf_ce.h"

void configure_txpower_track_8814a(
	struct txpwrtrack_cfg	*pConfig
	);

VOID
GetDeltaSwingTable_8814A(
	IN	PVOID		pDM_VOID,
	u8* 			*TemperatureUP_A,
	u8* 			*TemperatureDOWN_A,
	u8* 			*TemperatureUP_B,
	u8* 			*TemperatureDOWN_B	
	);

VOID
GetDeltaSwingTable_8814A_PathCD(
	IN	PVOID		pDM_VOID,
	u8* 			*TemperatureUP_C,
	u8* 			*TemperatureDOWN_C,
	u8* 			*TemperatureUP_D,
	u8* 			*TemperatureDOWN_D	
	);

VOID 
ODM_TxPwrTrackSetPwr8814A(
	IN	PVOID		pDM_VOID,
	enum pwrtrack_method 	Method,
	u8 				RFPath,
	u8 				ChannelMappedIndex
	);

u8
CheckRFGainOffset(
	struct dm_struct	*pDM_Odm,
	enum pwrtrack_method 	Method,
	u8				RFPath
	);

VOID
phy_iq_calibrate_8814a(
	IN	PVOID		pDM_VOID,
	boolean		bReCovery
	);

//
// LC calibrate
//
void	
phy_lc_calibrate_8814a(
	IN	PVOID		pDM_VOID
	);

//
// AP calibrate
//
void	
PHY_APCalibrate_8814A(		
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct	*		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	IN 	s1Byte		delta
	);


VOID	                                                 
PHY_DPCalibrate_8814A(                                   
	struct dm_struct	*	pDM_Odm                             
	);


VOID phy_set_rf_path_switch_8814a(
#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
	struct dm_struct	*		pDM_Odm,
#else
	IN	PADAPTER	pAdapter,
#endif
	boolean		bMain
	);

								
#endif	// #ifndef __HAL_PHY_RF_8188E_H__								

