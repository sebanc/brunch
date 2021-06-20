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
#ifndef __INC_ODM_REGCONFIG_H_8814A
#define __INC_ODM_REGCONFIG_H_8814A
 
#if (RTL8814A_SUPPORT == 1)

void
odm_ConfigRFReg_8814A(
	struct dm_struct    *pDM_Odm,
	u32 					Addr,
	u32 					Data,
	enum rf_path     RF_PATH,
	u32				    RegAddr
	);

void 
odm_ConfigRF_RadioA_8814A(
	struct dm_struct    *pDM_Odm,
	u32 					Addr,
	u32 					Data
	);

void 
odm_ConfigRF_RadioB_8814A(
	struct dm_struct    *pDM_Odm,
	u32 					Addr,
	u32 					Data
	);

void 
odm_ConfigRF_RadioC_8814A(
	struct dm_struct    *pDM_Odm,
	u32 					Addr,
	u32 					Data
	);

void 
odm_ConfigRF_RadioD_8814A(
	struct dm_struct    *pDM_Odm,
	u32 					Addr,
	u32 					Data
	);

void 
odm_ConfigMAC_8814A(
 	struct dm_struct    *pDM_Odm,
 	u32 		Addr,
 	IN 	u1Byte 		Data
 	);

void 
odm_ConfigBB_AGC_8814A(
    struct dm_struct    *pDM_Odm,
    u32 		Addr,
    u32 		Bitmask,
    u32 		Data
    );

void
odm_ConfigBB_PHY_REG_PG_8814A(
	struct dm_struct    *pDM_Odm,
	u32		Band,
	u32		RfPath,
	u32		TxNum,
    u32 		Addr,
    u32 		Bitmask,
    u32 		Data
    );

void 
odm_ConfigBB_PHY_8814A(
	struct dm_struct    *pDM_Odm,
    u32 		Addr,
    u32 		Bitmask,
    u32 		Data
    );

void
odm_ConfigBB_TXPWR_LMT_8814A(
	struct dm_struct    *pDM_Odm,
	u8*		Regulation,
	u8*		Band,
	u8*		Bandwidth,
	u8*		RateSection,
	u8*		RfPath,
	u8* 	Channel,
	u8*		PowerLimit
    );
#endif
#endif // end of SUPPORT

