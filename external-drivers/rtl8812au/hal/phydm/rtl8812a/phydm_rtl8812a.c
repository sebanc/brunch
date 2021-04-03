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

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"

#include "../phydm_precomp.h"

#if (RTL8812A_SUPPORT == 1)

s8 phydm_cck_rssi_8812a(struct dm_struct *dm, u16 lna_idx, u8 vga_idx)
{
	s8 rx_pwr_all = 0;

	switch (lna_idx) {
	case 7:
		if (vga_idx <= 27)
			rx_pwr_all = -94 + 2 * (27 - vga_idx);
		else
			rx_pwr_all = -94;
		break;
	case 6:
		rx_pwr_all = -42 + 2 * (2 - vga_idx);
		break;
	case 5:
		rx_pwr_all = -36 + 2 * (7 - vga_idx);
		break;
	case 4:
		rx_pwr_all = -30 + 2 * (7 - vga_idx);
		break;
	case 3:
		rx_pwr_all = -18 + 2 * (7 - vga_idx);
		break;
	case 2:
		rx_pwr_all = 2 * (5 - vga_idx);
		break;
	case 1:
		rx_pwr_all = 14 - 2 * vga_idx;
		break;
	case 0:
		rx_pwr_all = 20 - 2 * vga_idx;
		break;
	default:
		break;
	}

	return rx_pwr_all;
}

#ifdef DYN_ANT_WEIGHTING_SUPPORT
void phydm_dynamic_ant_weighting_8812a(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 rssi_l2h = 43, rssi_h2l = 37;
	u8 reg_8;

	if (dm->is_disable_dym_ant_weighting)
		return;

	if (*dm->channel <= 14) {
		if (dm->rssi_min >= rssi_l2h) {
			odm_set_bb_reg(dm, R_0x854, BIT(3), 0x1);
			odm_set_bb_reg(dm, R_0x850, BIT(21) | BIT(22), 0x0);

			/*equal weighting*/
			reg_8 = (u8)odm_get_bb_reg(dm, R_0xf94, BIT(0) | BIT(1) | BIT(2));
			PHYDM_DBG(dm, ODM_COMP_API,
				  "Equal weighting ,rssi_min = %d\n, 0xf94[2:0] = 0x%x\n",
				  dm->rssi_min, reg_8);
		} else if (dm->rssi_min <= rssi_h2l) {
			odm_set_bb_reg(dm, R_0x854, BIT(3), 0x1);
			odm_set_bb_reg(dm, R_0x850, BIT(21) | BIT(22), 0x1);

			/*fix sec_min_wgt = 1/2*/
			reg_8 = (u8)odm_get_bb_reg(dm, R_0xf94, BIT(0) | BIT(1) | BIT(2));
			PHYDM_DBG(dm, ODM_COMP_API,
				  "AGC weighting ,rssi_min = %d\n, 0xf94[2:0] = 0x%x\n",
				  dm->rssi_min, reg_8);
		}
	} else {
		odm_set_bb_reg(dm, R_0x854, BIT(3), 0x1);
		odm_set_bb_reg(dm, R_0x850, BIT(21) | BIT(22), 0x1);

		reg_8 = (u8)odm_get_bb_reg(dm, R_0xf94, BIT(0) | BIT(1) | BIT(2));
		PHYDM_DBG(dm, ODM_COMP_API,
			  "AGC weighting ,rssi_min = %d\n, 0xf94[2:0] = 0x%x\n",
			  dm->rssi_min, reg_8);
		/*fix sec_min_wgt = 1/2*/
	}
}
#endif

void phydm_hwsetting_8812a(struct dm_struct *dm)
{
	phydm_dynamic_ant_weighting(dm);
}

#endif /* #if (RTL8812A_SUPPORT == 1) */
