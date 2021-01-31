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
#ifndef __INC_MP_MAC_HW_IMG_8821A_H
#define __INC_MP_MAC_HW_IMG_8821A_H


/******************************************************************************
*                           MAC_REG.TXT
******************************************************************************/

void
odm_read_and_config_mp_8821a_mac_reg(/* TC: Test Chip, MP: MP Chip*/
	struct dm_struct  *dm
);
u32 odm_get_version_mp_8821a_mac_reg(void);

#endif
#endif /* end of HWIMG_SUPPORT*/
