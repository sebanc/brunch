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
#ifndef __INC_MP_RF_HW_IMG_8192E_H
#define __INC_MP_RF_HW_IMG_8192E_H


/******************************************************************************
*                           RadioA.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_radioa(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_radioa(void);

/******************************************************************************
*                           RadioB.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_radiob(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_radiob(void);

/******************************************************************************
*                           TxPowerTrack_AP.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_txpowertrack_ap(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_txpowertrack_ap(void);

/******************************************************************************
*                           TxPowerTrack_PCIE.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_txpowertrack_pcie(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_txpowertrack_pcie(void);

/******************************************************************************
*                           TxPowerTrack_SDIO.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_txpowertrack_sdio(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_txpowertrack_sdio(void);

/******************************************************************************
*                           TxPowerTrack_USB.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_txpowertrack_usb(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_txpowertrack_usb(void);

/******************************************************************************
*                           TXPWR_LMT.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_txpwr_lmt(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_txpwr_lmt(void);

/******************************************************************************
*                           TXPWR_LMT_8192E_SAR_5mm.TXT
******************************************************************************/

void
odm_read_and_config_mp_8192e_txpwr_lmt_8192e_sar_5mm(/* TC: Test Chip, MP: MP Chip*/
	struct PHY_DM_STRUCT  *p_dm_odm
);
u32 odm_get_version_mp_8192e_txpwr_lmt_8192e_sar_5mm(void);

#endif
#endif /* end of HWIMG_SUPPORT*/
