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

/*Image2HeaderVersion: 2.18*/
#if (RTL8192E_SUPPORT == 1)
#ifndef __INC_MP_BB_HW_IMG_8192E_H
#define __INC_MP_BB_HW_IMG_8192E_H


/******************************************************************************
*                           AGC_TAB.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_agc_tab(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_agc_tab(void);

/******************************************************************************
*                           PHY_REG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_phy_reg(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_phy_reg(void);

/******************************************************************************
*                           PHY_REG_PG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_phy_reg_pg(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_phy_reg_pg(void);

#endif
#endif /* end of HWIMG_SUPPORT*/
