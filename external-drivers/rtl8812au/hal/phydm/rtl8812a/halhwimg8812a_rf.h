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
#ifndef __INC_MP_RF_HW_IMG_8812A_H
#define __INC_MP_RF_HW_IMG_8812A_H


/******************************************************************************
*                           radioa.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_radioa( /* tc: Test Chip, mp: mp Chip*/
				    struct dm_struct *dm);
u32 odm_get_version_mp_8812a_radioa(void);

/******************************************************************************
*                           radiob.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_radiob( /* tc: Test Chip, mp: mp Chip*/
				    struct dm_struct *dm);
u32 odm_get_version_mp_8812a_radiob(void);

/******************************************************************************
*                           txpowertrack_ap.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpowertrack_ap( /* tc: Test Chip, mp: mp Chip*/
					     struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpowertrack_ap(void);

/******************************************************************************
*                           txpowertrack_pcie.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpowertrack_pcie( /* tc: Test Chip, mp: mp Chip*/
					       struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpowertrack_pcie(void);

/******************************************************************************
*                           txpowertrack_rfe3.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpowertrack_rfe3( /* tc: Test Chip, mp: mp Chip*/
					       struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpowertrack_rfe3(void);

/******************************************************************************
*                           txpowertrack_rfe4.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpowertrack_rfe4( /* tc: Test Chip, mp: mp Chip*/
					       struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpowertrack_rfe4(void);

/******************************************************************************
*                           txpowertrack_usb.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpowertrack_usb( /* tc: Test Chip, mp: mp Chip*/
					      struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpowertrack_usb(void);

/******************************************************************************
*                           txpwr_lmt.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpwr_lmt( /* tc: Test Chip, mp: mp Chip*/
				       struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpwr_lmt(void);

/******************************************************************************
*                           txpwr_lmt_hm812a03.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpwr_lmt_hm812a03( /* tc: Test Chip, mp: mp Chip*/
						struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpwr_lmt_hm812a03(void);

/******************************************************************************
*                           txpwr_lmt_nfa812a00.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpwr_lmt_nfa812a00(
						 /* tc: Test Chip, mp: mp Chip*/
						 struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpwr_lmt_nfa812a00(void);

/******************************************************************************
*                           txpwr_lmt_tplink.TXT
******************************************************************************/

void
odm_read_and_config_mp_8812a_txpwr_lmt_tplink( /* tc: Test Chip, mp: mp Chip*/
					      struct dm_struct *dm);
u32	odm_get_version_mp_8812a_txpwr_lmt_tplink(void);

#endif
#endif /* end of HWIMG_SUPPORT*/

