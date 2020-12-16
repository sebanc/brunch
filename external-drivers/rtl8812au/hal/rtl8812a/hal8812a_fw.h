/******************************************************************************
*
* Copyright(c) 2007 - 2016 Realtek Corporation. All rights reserved.
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

#ifdef CONFIG_RTL8812A

#ifndef _FW_HEADER_8812A_H
#define _FW_HEADER_8812A_H

#ifdef LOAD_FW_HEADER_FROM_DRIVER
#if (defined(CONFIG_AP_WOWLAN) || (DM_ODM_SUPPORT_TYPE & (ODM_AP)))
extern u8 array_mp_8812a_fw_ap[24070];
extern u32 array_length_mp_8812a_fw_ap;
#endif

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN)) || (DM_ODM_SUPPORT_TYPE & (ODM_CE))
extern u8 array_mp_8812a_fw_nic[27030];
extern u32 array_length_mp_8812a_fw_nic;
extern u8 array_mp_8812a_fw_nic_bt[29926];
extern u32 array_length_mp_8812a_fw_nic_bt;
extern u8 array_mp_8812a_fw_nic_mcc[29592];
extern u32 array_length_mp_8812a_fw_nic_mcc;
extern u8 array_mp_8812a_fw_wowlan[32404];
extern u32 array_length_mp_8812a_fw_wowlan;
#endif
#endif /* end of LOAD_FW_HEADER_FROM_DRIVER */

#endif

#endif

