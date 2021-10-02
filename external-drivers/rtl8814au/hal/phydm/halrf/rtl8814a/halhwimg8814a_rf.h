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
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

/*Image2HeaderVersion: R3 1.5.6*/
#if (RTL8814A_SUPPORT == 1)
#ifndef __INC_MP_RF_HW_IMG_8814A_H
#define __INC_MP_RF_HW_IMG_8814A_H

/* Please add following compiler flags definition (#define CONFIG_XXX_DRV_DIS)
 * into driver source code to reduce code size if necessary.
 * #define CONFIG_8814A_DRV_DIS
 * #define CONFIG_8814A_TYPE0_DRV_DIS
 * #define CONFIG_8814A_TYPE1_DRV_DIS
 * #define CONFIG_8814A_TYPE10_DRV_DIS
 * #define CONFIG_8814A_TYPE11_DRV_DIS
 * #define CONFIG_8814A_TYPE2_DRV_DIS
 * #define CONFIG_8814A_TYPE3_DRV_DIS
 * #define CONFIG_8814A_TYPE4_DRV_DIS
 * #define CONFIG_8814A_TYPE5_DRV_DIS
 * #define CONFIG_8814A_TYPE6_DRV_DIS
 * #define CONFIG_8814A_TYPE7_DRV_DIS
 * #define CONFIG_8814A_TYPE8_DRV_DIS
 * #define CONFIG_8814A_TYPE9_DRV_DIS
 */

#define CONFIG_8814A
#ifdef CONFIG_8814A_DRV_DIS
    #undef CONFIG_8814A
#endif

#define CONFIG_8814A_TYPE0
#ifdef CONFIG_8814A_TYPE0_DRV_DIS
    #undef CONFIG_8814A_TYPE0
#endif

#define CONFIG_8814A_TYPE1
#ifdef CONFIG_8814A_TYPE1_DRV_DIS
    #undef CONFIG_8814A_TYPE1
#endif

#define CONFIG_8814A_TYPE10
#ifdef CONFIG_8814A_TYPE10_DRV_DIS
    #undef CONFIG_8814A_TYPE10
#endif

#define CONFIG_8814A_TYPE11
#ifdef CONFIG_8814A_TYPE11_DRV_DIS
    #undef CONFIG_8814A_TYPE11
#endif

#define CONFIG_8814A_TYPE2
#ifdef CONFIG_8814A_TYPE2_DRV_DIS
    #undef CONFIG_8814A_TYPE2
#endif

#define CONFIG_8814A_TYPE3
#ifdef CONFIG_8814A_TYPE3_DRV_DIS
    #undef CONFIG_8814A_TYPE3
#endif

#define CONFIG_8814A_TYPE4
#ifdef CONFIG_8814A_TYPE4_DRV_DIS
    #undef CONFIG_8814A_TYPE4
#endif

#define CONFIG_8814A_TYPE5
#ifdef CONFIG_8814A_TYPE5_DRV_DIS
    #undef CONFIG_8814A_TYPE5
#endif

#define CONFIG_8814A_TYPE6
#ifdef CONFIG_8814A_TYPE6_DRV_DIS
    #undef CONFIG_8814A_TYPE6
#endif

#define CONFIG_8814A_TYPE7
#ifdef CONFIG_8814A_TYPE7_DRV_DIS
    #undef CONFIG_8814A_TYPE7
#endif

#define CONFIG_8814A_TYPE8
#ifdef CONFIG_8814A_TYPE8_DRV_DIS
    #undef CONFIG_8814A_TYPE8
#endif

#define CONFIG_8814A_TYPE9
#ifdef CONFIG_8814A_TYPE9_DRV_DIS
    #undef CONFIG_8814A_TYPE9
#endif

/******************************************************************************
 *                           radioa.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_radioa(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_radioa(void);

/******************************************************************************
 *                           radiob.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_radiob(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_radiob(void);

/******************************************************************************
 *                           radioc.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_radioc(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_radioc(void);

/******************************************************************************
 *                           radiod.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_radiod(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_radiod(void);

/******************************************************************************
 *                           txpowertrack.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack(void);

/******************************************************************************
 *                           txpowertrack_type0.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type0(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type0(void);

/******************************************************************************
 *                           txpowertrack_type1.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type1(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type1(void);

/******************************************************************************
 *                           txpowertrack_type10.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type10(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type10(void);

/******************************************************************************
 *                           txpowertrack_type11.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type11(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type11(void);

/******************************************************************************
 *                           txpowertrack_type2.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type2(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type2(void);

/******************************************************************************
 *                           txpowertrack_type3.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type3(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type3(void);

/******************************************************************************
 *                           txpowertrack_type4.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type4(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type4(void);

/******************************************************************************
 *                           txpowertrack_type5.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type5(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type5(void);

/******************************************************************************
 *                           txpowertrack_type6.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type6(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type6(void);

/******************************************************************************
 *                           txpowertrack_type7.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type7(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type7(void);

/******************************************************************************
 *                           txpowertrack_type8.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type8(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type8(void);

/******************************************************************************
 *                           txpowertrack_type9.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertrack_type9(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertrack_type9(void);

/******************************************************************************
 *                           txpowertssi.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpowertssi(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpowertssi(void);

/******************************************************************************
 *                           txpwr_lmt.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt(void);

/******************************************************************************
 *                           txpwr_lmt_type0.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type0(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type0(void);

/******************************************************************************
 *                           txpwr_lmt_type1.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type1(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type1(void);

/******************************************************************************
 *                           txpwr_lmt_type10.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type10(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type10(void);

/******************************************************************************
 *                           txpwr_lmt_type11.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type11(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type11(void);

/******************************************************************************
 *                           txpwr_lmt_type2.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type2(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type2(void);

/******************************************************************************
 *                           txpwr_lmt_type3.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type3(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type3(void);

/******************************************************************************
 *                           txpwr_lmt_type4.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type4(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type4(void);

/******************************************************************************
 *                           txpwr_lmt_type5.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type5(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type5(void);

/******************************************************************************
 *                           txpwr_lmt_type6.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type6(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type6(void);

/******************************************************************************
 *                           txpwr_lmt_type7.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type7(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type7(void);

/******************************************************************************
 *                           txpwr_lmt_type8.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type8(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type8(void);

/******************************************************************************
 *                           txpwr_lmt_type9.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8814a_txpwr_lmt_type9(struct dm_struct *dm);
u32 odm_get_version_mp_8814a_txpwr_lmt_type9(void);

#endif
#endif /* end of HWIMG_SUPPORT*/

