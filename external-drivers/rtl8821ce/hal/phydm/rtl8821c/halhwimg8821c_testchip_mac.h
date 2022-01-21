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

/*Image2HeaderVersion: 2.22*/
#if (RTL8821C_SUPPORT == 1)
#ifndef __INC_TC_MAC_HW_IMG_8821C_H
#define __INC_TC_MAC_HW_IMG_8821C_H


/******************************************************************************
*                           MAC_REG.TXT
******************************************************************************/

void
odm_read_and_config_tc_8821c_mac_reg(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_tc_8821c_mac_reg(void);

#endif
#endif /* end of HWIMG_SUPPORT*/
