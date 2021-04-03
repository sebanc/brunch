// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/*============================================================
 include files
============================================================*/

#include "mp_precomp.h"
#include "phydm_precomp.h"

s8
odm_cckrssi_8723d(
	u8	lna_idx,
	u8	vga_idx
)
{
	s8	rx_pwr_all = 0x00;

	switch (lna_idx) {

	case 0xf:
		rx_pwr_all = -46 - (2 * vga_idx);
		break;
	case 0xa:
		rx_pwr_all = -20 - (2 * vga_idx);
		break;
	case 7:
		rx_pwr_all = -10 - (2 * vga_idx);
		break;
	case 4:
		rx_pwr_all = 4 - (2 * vga_idx);
		break;
	default:
		break;
	}

	return rx_pwr_all;

}
