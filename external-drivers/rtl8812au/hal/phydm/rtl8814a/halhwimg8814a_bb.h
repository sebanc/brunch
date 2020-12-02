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
#ifndef __INC_MP_BB_HW_IMG_8814A_H
#define __INC_MP_BB_HW_IMG_8814A_H


/******************************************************************************
*                           AGC_TAB.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_agc_tab(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_AGC_TAB(void);

/******************************************************************************
*                           PHY_REG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_phy_reg(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_PHY_REG(void);

/******************************************************************************
*                           PHY_REG_MP.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_phy_reg_mp(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_PHY_REG_MP(void);

/******************************************************************************
*                           PHY_REG_PG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_phy_reg_pg(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_PHY_REG_PG(void);

/******************************************************************************
*                           PHY_REG_PG_Type2.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_phy_reg_pg_type2(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_PHY_REG_PG_Type2(void);

/******************************************************************************
*                           PHY_REG_PG_Type3.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_phy_reg_pg_type3(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_PHY_REG_PG_Type3(void);

/******************************************************************************
*                           PHY_REG_PG_Type5.TXT
******************************************************************************/

void
odm_read_and_config_mp_8814a_phy_reg_pg_type5(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct    *  pDM_Odm
);
u4Byte ODM_GetVersion_MP_8814A_PHY_REG_PG_Type5(void);

#endif
#endif /* end of HWIMG_SUPPORT*/

