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

/*Image2HeaderVersion: 3.5.2*/
#if (RTL8812A_SUPPORT == 1)
#ifndef __INC_MP_BB_HW_IMG_8812A_H
#define __INC_MP_BB_HW_IMG_8812A_H


/******************************************************************************
*                           agc_tab.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_agc_tab( /* tc: Test Chip, mp: mp Chip*/
				     struct dm_struct *dm);
u32 odm_get_version_mp_8812a_agc_tab(void);

/******************************************************************************
*                           agc_tab_diff.TXT
******************************************************************************/

extern u32	array_mp_8812a_agc_tab_diff_lb[60];
extern u32	array_mp_8812a_agc_tab_diff_hb[60];
void
odm_read_and_config_mp_8812a_agc_tab_diff(struct dm_struct *dm, u32 array[],
					  u32 array_len);
u32 odm_get_version_mp_8812a_agc_tab_diff(void);

/******************************************************************************
*                           phy_reg.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg( /* tc: Test Chip, mp: mp Chip*/
				     struct dm_struct *dm);
u32 odm_get_version_mp_8812a_phy_reg(void);

/******************************************************************************
*                           phy_reg_mp.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg_mp( /* tc: Test Chip, mp: mp Chip*/
					struct dm_struct *dm);
u32 odm_get_version_mp_8812a_phy_reg_mp(void);

/******************************************************************************
*                           phy_reg_pg.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg_pg( /* tc: Test Chip, mp: mp Chip*/
					struct dm_struct *dm);
u32	odm_get_version_mp_8812a_phy_reg_pg(void);

/******************************************************************************
*                           phy_reg_pg_asus.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg_pg_asus( /* tc: Test Chip, mp: mp Chip*/
					     struct dm_struct *dm);
u32	odm_get_version_mp_8812a_phy_reg_pg_asus(void);

/******************************************************************************
*                           phy_reg_pg_dni.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg_pg_dni( /* tc: Test Chip, mp: mp Chip*/
					    struct dm_struct *dm);
u32	odm_get_version_mp_8812a_phy_reg_pg_dni(void);

/******************************************************************************
*                           phy_reg_pg_nec.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg_pg_nec( /* tc: Test Chip, mp: mp Chip*/
					    struct dm_struct *dm);
u32	odm_get_version_mp_8812a_phy_reg_pg_nec(void);

/******************************************************************************
*                           phy_reg_pg_tplink.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_phy_reg_pg_tplink( /* tc: Test Chip, mp: mp Chip*/
					       struct dm_struct *dm);
u32	odm_get_version_mp_8812a_phy_reg_pg_tplink(void);

#endif
#endif /* end of HWIMG_SUPPORT*/

