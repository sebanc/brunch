/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __HAL_VERSION_DEF_H__
#define __HAL_VERSION_DEF_H__

/* HAL_IC_TYPE */
enum hal_ic_type {
	CHIP_8192S	=	0,
	CHIP_8188C	=	1,
	CHIP_8192C	=	2,
	CHIP_8192D	=	3,
	CHIP_8723A	=	4,
	CHIP_8188E	=	5,
	CHIP_8812	=	6,
	CHIP_8821	=	7,
	CHIP_8723B	=	8,
	CHIP_8192E	=	9,
	CHIP_8814A	=	10,
	CHIP_8703B	=	11,
	CHIP_8188F	=	12,
	CHIP_8822B	=	13,
	CHIP_8723D	=	14,
	CHIP_8821C	=	15
};

/* HAL_CHIP_TYPE */
enum hal_chip_type {
	TEST_CHIP		=	0,
	NORMAL_CHIP	=	1,
	FPGA			=	2,
};

/* HAL_CUT_VERSION */
enum hal_cut_version {
	A_CUT_VERSION		=	0,
	B_CUT_VERSION		=	1,
	C_CUT_VERSION		=	2,
	D_CUT_VERSION		=	3,
	E_CUT_VERSION		=	4,
	F_CUT_VERSION		=	5,
	G_CUT_VERSION		=	6,
	H_CUT_VERSION		=	7,
	I_CUT_VERSION		=	8,
	J_CUT_VERSION		=	9,
	K_CUT_VERSION		=	10,
};

/* HAL_Manufacturer */
enum hal_vendor {
	CHIP_VENDOR_TSMC	=	0,
	CHIP_VENDOR_UMC	=	1,
	CHIP_VENDOR_SMIC	=	2,
};

enum hal_rf_type {
	RF_TYPE_1T1R	=	0,
	RF_TYPE_1T2R	=	1,
	RF_TYPE_2T2R	=	2,
	RF_TYPE_2T3R	=	3,
	RF_TYPE_2T4R	=	4,
	RF_TYPE_3T3R	=	5,
	RF_TYPE_3T4R	=	6,
	RF_TYPE_4T4R	=	7,
};

struct hal_version {
	enum hal_ic_type	ICType;
	enum hal_chip_type	ChipType;
	enum hal_cut_version	CUTVersion;
	enum hal_vendor		VendorType;
	enum hal_rf_type	RFType;
	u8			ROMVer;
};

/* VERSION_8192C			VersionID;
 * struct hal_version			VersionID; */

/* Get element */
#define GET_CVID_IC_TYPE(version)		((version).ICType)
#define GET_CVID_CHIP_TYPE(version)		((version).ChipType)
#define GET_CVID_RF_TYPE(version)		((version).RFType)
#define GET_CVID_MANUFACTUER(version)		((version).VendorType)
#define GET_CVID_CUT_VERSION(version)		((version).CUTVersion)
#define GET_CVID_ROM_VERSION(version)		(((version).ROMVer) & ROM_VERSION_MASK)

/* ----------------------------------------------------------------------------
 * Common Macro. --
 * ----------------------------------------------------------------------------
 * struct hal_version VersionID */

/* enum hal_ic_type */

#define IS_8188E(version)			((GET_CVID_IC_TYPE(version) == CHIP_8188E) ? true : false)
#define IS_8188F(version)			((GET_CVID_IC_TYPE(version) == CHIP_8188F) ? true : false)
#define IS_8192E(version)			((GET_CVID_IC_TYPE(version) == CHIP_8192E) ? true : false)
#define IS_8812_SERIES(version)			((GET_CVID_IC_TYPE(version) == CHIP_8812) ? true : false)
#define IS_8821_SERIES(version)			((GET_CVID_IC_TYPE(version) == CHIP_8821) ? true : false)
#define IS_8814A_SERIES(version)		((GET_CVID_IC_TYPE(version) == CHIP_8814A) ? true : false)
#define IS_8723B_SERIES(version)		((GET_CVID_IC_TYPE(version) == CHIP_8723B) ? true : false)
#define IS_8703B_SERIES(version)		((GET_CVID_IC_TYPE(version) == CHIP_8703B) ? true : false)
#define IS_8822B_SERIES(version)		((GET_CVID_IC_TYPE(version) == CHIP_8822B) ? true : false)
#define IS_8821C_SERIES(version)		((GET_CVID_IC_TYPE(version) == CHIP_8821C) ? true : false)
#define IS_8723D_SERIES(version)\
	((GET_CVID_IC_TYPE(version) == CHIP_8723D) ? true : false)
/* enum hal_chip_type */
#define IS_TEST_CHIP(version)			((GET_CVID_CHIP_TYPE(version) == TEST_CHIP) ? true : false)
#define IS_NORMAL_CHIP(version)			((GET_CVID_CHIP_TYPE(version) == NORMAL_CHIP) ? true : false)

/* enum hal_cut_version */
#define IS_A_CUT(version)			((GET_CVID_CUT_VERSION(version) == A_CUT_VERSION) ? true : false)
#define IS_B_CUT(version)			((GET_CVID_CUT_VERSION(version) == B_CUT_VERSION) ? true : false)
#define IS_C_CUT(version)			((GET_CVID_CUT_VERSION(version) == C_CUT_VERSION) ? true : false)
#define IS_D_CUT(version)			((GET_CVID_CUT_VERSION(version) == D_CUT_VERSION) ? true : false)
#define IS_E_CUT(version)			((GET_CVID_CUT_VERSION(version) == E_CUT_VERSION) ? true : false)
#define IS_F_CUT(version)			((GET_CVID_CUT_VERSION(version) == F_CUT_VERSION) ? true : false)
#define IS_I_CUT(version)			((GET_CVID_CUT_VERSION(version) == I_CUT_VERSION) ? true : false)
#define IS_J_CUT(version)			((GET_CVID_CUT_VERSION(version) == J_CUT_VERSION) ? true : false)
#define IS_K_CUT(version)			((GET_CVID_CUT_VERSION(version) == K_CUT_VERSION) ? true : false)

/* enum hal_vendor */
#define IS_CHIP_VENDOR_TSMC(version)	((GET_CVID_MANUFACTUER(version) == CHIP_VENDOR_TSMC) ? true : false)
#define IS_CHIP_VENDOR_UMC(version)	((GET_CVID_MANUFACTUER(version) == CHIP_VENDOR_UMC) ? true : false)
#define IS_CHIP_VENDOR_SMIC(version)	((GET_CVID_MANUFACTUER(version) == CHIP_VENDOR_SMIC) ? true : false)

/* enum hal_rf_type */
#define IS_1T1R(version)			((GET_CVID_RF_TYPE(version) == RF_TYPE_1T1R) ? true : false)
#define IS_1T2R(version)			((GET_CVID_RF_TYPE(version) == RF_TYPE_1T2R) ? true : false)
#define IS_2T2R(version)			((GET_CVID_RF_TYPE(version) == RF_TYPE_2T2R) ? true : false)
#define IS_3T3R(version)			((GET_CVID_RF_TYPE(version) == RF_TYPE_3T3R) ? true : false)
#define IS_3T4R(version)			((GET_CVID_RF_TYPE(version) == RF_TYPE_3T4R) ? true : false)
#define IS_4T4R(version)			((GET_CVID_RF_TYPE(version) == RF_TYPE_4T4R) ? true : false)

/* ----------------------------------------------------------------------------
 * Chip version Macro. --
 * ---------------------------------------------------------------------------- */
#define IS_VENDOR_8188E_I_CUT_SERIES(_Adapter)		((IS_8188E(GET_HAL_DATA(_Adapter)->version_id)) ? ((GET_CVID_CUT_VERSION(GET_HAL_DATA(_Adapter)->version_id) >= I_CUT_VERSION) ? true : false) : false)
#define IS_VENDOR_8812A_TEST_CHIP(_Adapter)		((IS_8812_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? false : true) : false)
#define IS_VENDOR_8812A_MP_CHIP(_Adapter)		((IS_8812_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? true : false) : false)
#define IS_VENDOR_8812A_C_CUT(_Adapter)			((IS_8812_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((GET_CVID_CUT_VERSION(GET_HAL_DATA(_Adapter)->version_id) == C_CUT_VERSION) ? true : false) : false)

#define IS_VENDOR_8821A_TEST_CHIP(_Adapter)	((IS_8821_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? false : true) : false)
#define IS_VENDOR_8821A_MP_CHIP(_Adapter)		((IS_8821_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? true : false) : false)

#define IS_VENDOR_8192E_B_CUT(_Adapter)		((GET_CVID_CUT_VERSION(GET_HAL_DATA(_Adapter)->version_id) == B_CUT_VERSION) ? true : false)

#define IS_VENDOR_8723B_TEST_CHIP(_Adapter)	((IS_8723B_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? false : true) : false)
#define IS_VENDOR_8723B_MP_CHIP(_Adapter)		((IS_8723B_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? true : false) : false)

#define IS_VENDOR_8703B_TEST_CHIP(_Adapter)	((IS_8703B_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? false : true) : false)
#define IS_VENDOR_8703B_MP_CHIP(_Adapter)		((IS_8703B_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? true : false) : false)
#define IS_VENDOR_8814A_TEST_CHIP(_Adapter)	((IS_8814A_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? false : true) : false)
#define IS_VENDOR_8814A_MP_CHIP(_Adapter)		((IS_8814A_SERIES(GET_HAL_DATA(_Adapter)->version_id)) ? ((IS_NORMAL_CHIP(GET_HAL_DATA(_Adapter)->version_id)) ? true : false) : false)

#endif
