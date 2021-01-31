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

#if (RTL8814A_SUPPORT == 1)
#ifndef __INC_MP_FW_HW_IMG_8814A_H
#define __INC_MP_FW_HW_IMG_8814A_H


/******************************************************************************
*                           FW_AP.TXT
******************************************************************************/

void
ODM_ReadFirmware_MP_8814A_FW_AP(
     struct dm_struct    *pDM_Odm,
     u8       *pFirmware,
     u32       *pFirmwareSize
);
u4Byte ODM_GetVersion_MP_8814A_FW_AP(void);
extern u32 array_length_mp_8814a_fw_ap;
extern u8 array_mp_8814a_fw_ap[];

/******************************************************************************
*                           FW_NIC.TXT
******************************************************************************/

void
ODM_ReadFirmware_MP_8814A_FW_NIC(
     struct dm_struct    *pDM_Odm,
     u8       *pFirmware,
     u32       *pFirmwareSize
);
u4Byte ODM_GetVersion_MP_8814A_FW_NIC(void);
extern u32 array_length_mp_8814a_fw_nic;
extern u8 array_mp_8814a_fw_nic[];

#endif
#endif // end of HWIMG_SUPPORT

