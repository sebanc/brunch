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
#if (RTL8821A_SUPPORT == 1)
#ifndef __INC_MP_BB_HW_IMG_8821A_H
#define __INC_MP_BB_HW_IMG_8821A_H


/******************************************************************************
*                           AGC_TAB.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_agc_tab(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_agc_tab(void);

/******************************************************************************
*                           PHY_REG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_phy_reg(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_phy_reg(void);

/******************************************************************************
*                           PHY_REG_PG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_phy_reg_pg(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_phy_reg_pg(void);

/******************************************************************************
*                           PHY_REG_PG_DNI_JP.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_phy_reg_pg_dni_jp(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_phy_reg_pg_dni_jp(void);

/******************************************************************************
*                           PHY_REG_PG_DNI_US.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_phy_reg_pg_dni_us(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_phy_reg_pg_dni_us(void);

/******************************************************************************
*                           PHY_REG_PG_E202SA.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_phy_reg_pg_e202_sa(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_phy_reg_pg_e202sa(void);

#endif
#endif /* end of HWIMG_SUPPORT*/
