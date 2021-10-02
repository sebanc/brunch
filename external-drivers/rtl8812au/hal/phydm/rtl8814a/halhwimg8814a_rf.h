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

/*Image2HeaderVersion: 2.19*/
#if (RTL8814A_SUPPORT == 1)
#ifndef __INC_MP_RF_HW_IMG_8814A_H
#define __INC_MP_RF_HW_IMG_8814A_H


/******************************************************************************
*                           RadioA.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_radioa(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_RadioA(void);

/******************************************************************************
*                           RadioB.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_radiob(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_RadioB(void);

/******************************************************************************
*                           RadioC.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_radioc(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_RadioC(void);

/******************************************************************************
*                           RadioD.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_radiod(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_RadioD(void);

/******************************************************************************
*                           TxPowerTrack.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpowertrack(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TxPowerTrack(void);

/******************************************************************************
*                           TxPowerTrack_Type0.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpowertrack_type0(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TxPowerTrack_Type0(void);

/******************************************************************************
*                           TxPowerTrack_Type2.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpowertrack_type2(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TxPowerTrack_Type2(void);

/******************************************************************************
*                           TxPowerTrack_Type5.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpowertrack_type5(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TxPowerTrack_Type5(void);

/******************************************************************************
*                           TXPWR_LMT.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpwr_lmt(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TXPWR_LMT(void);

/******************************************************************************
*                           TXPWR_LMT_type2.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpwr_lmt_type2(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TXPWR_LMT_type2(void);

/******************************************************************************
*                           TXPWR_LMT_Type3.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpwr_lmt_type3(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TXPWR_LMT_Type3(void);

/******************************************************************************
*                           TXPWR_LMT_Type5.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_txpwr_lmt_type5(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_TXPWR_LMT_Type5(void);

#endif
#endif /* end of HWIMG_SUPPORT*/

