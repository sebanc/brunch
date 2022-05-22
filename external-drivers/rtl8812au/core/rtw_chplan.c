/******************************************************************************
 *
 * Copyright(c) 2007 - 2018 Realtek Corporation.
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
#define _RTW_CHPLAN_C_

#include <drv_types.h>

#define RTW_DOMAIN_MAP_VER		"55"
#define RTW_DOMAIN_MAP_M_VER	"g"
#define RTW_COUNTRY_MAP_VER		"29"

struct ch_list_t {
	u8 *len_ch_attr;
};

#define CLA_2G_12_14_PASSIVE	BIT0

#define CLA_5G_B1_PASSIVE		BIT0
#define CLA_5G_B2_PASSIVE		BIT1
#define CLA_5G_B3_PASSIVE		BIT2
#define CLA_5G_B4_PASSIVE		BIT3
#define CLA_5G_B2_DFS			BIT4
#define CLA_5G_B3_DFS			BIT5
#define CLA_5G_B4_DFS			BIT6

#define CH_LIST_ENT(_len, arg...) \
	{.len_ch_attr = (u8[_len + 2]) {_len, ##arg}, }

#define CH_LIST_LEN(_ch_list) (_ch_list.len_ch_attr[0])
#define CH_LIST_CH(_ch_list, _i) (_ch_list.len_ch_attr[_i + 1])
#define CH_LIST_ATTRIB(_ch_list) (_ch_list.len_ch_attr[CH_LIST_LEN(_ch_list) + 1])

enum rtw_chd_2g {
	RTW_CHD_2G_00 = 0,
	RTW_CHD_2G_01 = 1,
	RTW_CHD_2G_02 = 2,
	RTW_CHD_2G_03 = 3,
	RTW_CHD_2G_04 = 4,
	RTW_CHD_2G_05 = 5,
	RTW_CHD_2G_06 = 6,

	RTW_CHD_2G_MAX,
	RTW_CHD_2G_NULL = RTW_CHD_2G_00,
};

enum rtw_chd_5g {
	RTW_CHD_5G_00 = 0,
	RTW_CHD_5G_01 = 1,
	RTW_CHD_5G_02 = 2,
	RTW_CHD_5G_03 = 3,
	RTW_CHD_5G_04 = 4,
	RTW_CHD_5G_05 = 5,
	RTW_CHD_5G_06 = 6,
	RTW_CHD_5G_07 = 7,
	RTW_CHD_5G_08 = 8,
	RTW_CHD_5G_09 = 9,
	RTW_CHD_5G_10 = 10,
	RTW_CHD_5G_11 = 11,
	RTW_CHD_5G_12 = 12,
	RTW_CHD_5G_13 = 13,
	RTW_CHD_5G_14 = 14,
	RTW_CHD_5G_15 = 15,
	RTW_CHD_5G_16 = 16,
	RTW_CHD_5G_17 = 17,
	RTW_CHD_5G_18 = 18,
	RTW_CHD_5G_19 = 19,
	RTW_CHD_5G_20 = 20,
	RTW_CHD_5G_21 = 21,
	RTW_CHD_5G_22 = 22,
	RTW_CHD_5G_23 = 23,
	RTW_CHD_5G_24 = 24,
	RTW_CHD_5G_25 = 25,
	RTW_CHD_5G_26 = 26,
	RTW_CHD_5G_27 = 27,
	RTW_CHD_5G_28 = 28,
	RTW_CHD_5G_29 = 29,
	RTW_CHD_5G_30 = 30,
	RTW_CHD_5G_31 = 31,
	RTW_CHD_5G_32 = 32,
	RTW_CHD_5G_33 = 33,
	RTW_CHD_5G_34 = 34,
	RTW_CHD_5G_35 = 35,
	RTW_CHD_5G_36 = 36,
	RTW_CHD_5G_37 = 37,
	RTW_CHD_5G_38 = 38,
	RTW_CHD_5G_39 = 39,
	RTW_CHD_5G_40 = 40,
	RTW_CHD_5G_41 = 41,
	RTW_CHD_5G_42 = 42,
	RTW_CHD_5G_43 = 43,
	RTW_CHD_5G_44 = 44,
	RTW_CHD_5G_45 = 45,
	RTW_CHD_5G_46 = 46,
	RTW_CHD_5G_47 = 47,
	RTW_CHD_5G_48 = 48,
	RTW_CHD_5G_49 = 49,
	RTW_CHD_5G_50 = 50,
	RTW_CHD_5G_51 = 51,

	RTW_CHD_5G_MAX,
	RTW_CHD_5G_NULL = RTW_CHD_5G_00,
};

static const struct ch_list_t rtw_channel_def_2g[] = {
	/* 0, RTW_CHD_2G_00 */	CH_LIST_ENT(0, 0),
	/* 1, RTW_CHD_2G_01 */	CH_LIST_ENT(13, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, CLA_2G_12_14_PASSIVE),
	/* 2, RTW_CHD_2G_02 */	CH_LIST_ENT(13, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0),
	/* 3, RTW_CHD_2G_03 */	CH_LIST_ENT(11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0),
	/* 4, RTW_CHD_2G_04 */	CH_LIST_ENT(14, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0),
	/* 5, RTW_CHD_2G_05 */	CH_LIST_ENT(4, 10, 11, 12, 13, 0),
	/* 6, RTW_CHD_2G_06 */	CH_LIST_ENT(14, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, CLA_2G_12_14_PASSIVE),
};

#if CONFIG_IEEE80211_BAND_5GHZ
static const struct ch_list_t rtw_channel_def_5g[] = {
	/* 0, RTW_CHD_5G_00 */	CH_LIST_ENT(0, 0),
	/* 1, RTW_CHD_5G_01 */	CH_LIST_ENT(21, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 2, RTW_CHD_5G_02 */	CH_LIST_ENT(19, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 3, RTW_CHD_5G_03 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 4, RTW_CHD_5G_04 */	CH_LIST_ENT(22, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 5, RTW_CHD_5G_05 */	CH_LIST_ENT(19, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 149, 153, 157, 161, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 6, RTW_CHD_5G_06 */	CH_LIST_ENT(9, 36, 40, 44, 48, 149, 153, 157, 161, 165, 0),
	/* 7, RTW_CHD_5G_07 */	CH_LIST_ENT(13, 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165, CLA_5G_B2_DFS),
	/* 8, RTW_CHD_5G_08 */	CH_LIST_ENT(12, 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, CLA_5G_B2_DFS),
	/* 9, RTW_CHD_5G_09 */	CH_LIST_ENT(5, 149, 153, 157, 161, 165, 0),
	/* 10, RTW_CHD_5G_10 */	CH_LIST_ENT(8, 36, 40, 44, 48, 52, 56, 60, 64, CLA_5G_B2_DFS),
	/* 11, RTW_CHD_5G_11 */	CH_LIST_ENT(11, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, CLA_5G_B3_DFS),
	/* 12, RTW_CHD_5G_12 */	CH_LIST_ENT(16, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 13, RTW_CHD_5G_13 */	CH_LIST_ENT(8, 56, 60, 64, 149, 153, 157, 161, 165, CLA_5G_B2_DFS),
	/* 14, RTW_CHD_5G_14 */	CH_LIST_ENT(4, 36, 40, 44, 48, 0),
	/* 15, RTW_CHD_5G_15 */	CH_LIST_ENT(4, 149, 153, 157, 161, 0),
	/* 16, RTW_CHD_5G_16 */	CH_LIST_ENT(11, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0),
	/* 17, RTW_CHD_5G_17 */	CH_LIST_ENT(16, 36, 40, 44, 48, 52, 56, 60, 64, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 18, RTW_CHD_5G_18 */	CH_LIST_ENT(17, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 19, RTW_CHD_5G_19 */	CH_LIST_ENT(16, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 20, RTW_CHD_5G_20 */	CH_LIST_ENT(20, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 21, RTW_CHD_5G_21 */	CH_LIST_ENT(11, 36, 40, 44, 48, 52, 56, 60, 64, 132, 136, 140, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 22, RTW_CHD_5G_22 */	CH_LIST_ENT(25, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 23, RTW_CHD_5G_23 */	CH_LIST_ENT(21, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 24, RTW_CHD_5G_24 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE | CLA_5G_B4_PASSIVE),
	/* 25, RTW_CHD_5G_25 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE),
	/* 26, RTW_CHD_5G_26 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE),
	/* 27, RTW_CHD_5G_27 */	CH_LIST_ENT(21, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE),
	/* 28, RTW_CHD_5G_28 */	CH_LIST_ENT(13, 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165, CLA_5G_B2_PASSIVE),
	/* 29, RTW_CHD_5G_29 */	CH_LIST_ENT(13, 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE),
	/* 30, RTW_CHD_5G_30 */	CH_LIST_ENT(9, 36, 40, 44, 48, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B4_PASSIVE),
	/* 31, RTW_CHD_5G_31 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE | CLA_5G_B4_PASSIVE),
	/* 32, RTW_CHD_5G_32 */	CH_LIST_ENT(9, 52, 56, 60, 64, 149, 153, 157, 161, 165, CLA_5G_B2_DFS),
	/* 33, RTW_CHD_5G_33 */	CH_LIST_ENT(22, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 144, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 34, RTW_CHD_5G_34 */	CH_LIST_ENT(13, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B3_DFS),
	/* 35, RTW_CHD_5G_35 */	CH_LIST_ENT(8, 100, 104, 108, 112, 116, 132, 136, 140, CLA_5G_B3_DFS),
	/* 36, RTW_CHD_5G_36 */	CH_LIST_ENT(25, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE | CLA_5G_B2_DFS | CLA_5G_B3_PASSIVE | CLA_5G_B3_DFS | CLA_5G_B4_PASSIVE),
	/* 37, RTW_CHD_5G_37 */	CH_LIST_ENT(8, 36, 40, 44, 48, 52, 56, 60, 64, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE),
	/* 38, RTW_CHD_5G_38 */	CH_LIST_ENT(16, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 39, RTW_CHD_5G_39 */	CH_LIST_ENT(21, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_DFS | CLA_5G_B3_DFS | CLA_5G_B4_DFS),
	/* 40, RTW_CHD_5G_40 */	CH_LIST_ENT(21, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 41, RTW_CHD_5G_41 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS | CLA_5G_B4_PASSIVE),
	/* 42, RTW_CHD_5G_42 */	CH_LIST_ENT(24, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_DFS | CLA_5G_B3_DFS | CLA_5G_B4_PASSIVE),
	/* 43, RTW_CHD_5G_43 */	CH_LIST_ENT(23, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE | CLA_5G_B4_PASSIVE),
	/* 44, RTW_CHD_5G_44 */	CH_LIST_ENT(21, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE | CLA_5G_B4_PASSIVE),
	/* 45, RTW_CHD_5G_45 */	CH_LIST_ENT(13, 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165, CLA_5G_B1_PASSIVE | CLA_5G_B2_PASSIVE | CLA_5G_B4_PASSIVE),
	/* 46, RTW_CHD_5G_46 */	CH_LIST_ENT(12, 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, CLA_5G_B2_PASSIVE),
	/* 47, RTW_CHD_5G_47 */	CH_LIST_ENT(19, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, CLA_5G_B2_PASSIVE | CLA_5G_B3_PASSIVE),
	/* 48, RTW_CHD_5G_48 */	CH_LIST_ENT(20, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 49, RTW_CHD_5G_49 */	CH_LIST_ENT(17, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 50, RTW_CHD_5G_50 */	CH_LIST_ENT(17, 36, 40, 44, 48, 52, 56, 60, 64, 132, 136, 140, 144, 149, 153, 157, 161, 165, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
	/* 51, RTW_CHD_5G_51 */	CH_LIST_ENT(13, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, CLA_5G_B2_DFS | CLA_5G_B3_DFS),
};
#endif /* CONFIG_IEEE80211_BAND_5GHZ */

struct chplan_ent_t {
	u8 regd_2g; /* value of REGULATION_TXPWR_LMT */
	u8 chd_2g;
#if CONFIG_IEEE80211_BAND_5GHZ
	u8 regd_5g; /* value of REGULATION_TXPWR_LMT */
	u8 chd_5g;
#endif
};

#if CONFIG_IEEE80211_BAND_5GHZ
#define CHPLAN_ENT(_regd_2g, _chd_2g, _regd_5g, _chd_5g) {.regd_2g = RTW_REGD_##_regd_2g, .chd_2g = RTW_CHD_2G_##_chd_2g, .regd_5g = RTW_REGD_##_regd_5g, .chd_5g = RTW_CHD_5G_##_chd_5g}
#else
#define CHPLAN_ENT(_regd_2g, _chd_2g, _regd_5g, _chd_5g) {.regd_2g = RTW_REGD_##_regd_2g, .chd_2g = RTW_CHD_2G_##_chd_2g}
#endif

#define CHPLAN_ENT_NOT_DEFINED CHPLAN_ENT(NA, NULL, NA, NULL)

static const struct chplan_ent_t RTW_ChannelPlanMap[] = {
	/* 0x00 */	CHPLAN_ENT(ETSI , 02, ETSI , 49),
	/* 0x01 */	CHPLAN_ENT(ETSI , 02, ETSI , 50),
	/* 0x02 */	CHPLAN_ENT(ETSI , 03, ETSI , 07),
	/* 0x03 */	CHPLAN_ENT(ACMA , 02, ACMA , 33),
	/* 0x04 */	CHPLAN_ENT(ETSI , 02, ETSI , 51),
	/* 0x05 */	CHPLAN_ENT(ETSI , 02, ETSI , 06),
	/* 0x06 */	CHPLAN_ENT(ETSI , 02, ETSI , 07),
	/* 0x07 */	CHPLAN_ENT(ETSI , 02, ETSI , 23),
	/* 0x08 */	CHPLAN_ENT(ETSI , 02, ETSI , 21),
	/* 0x09 */	CHPLAN_ENT(ETSI , 02, ETSI , 17),
	/* 0x0A */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x0B */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x0C */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x0D */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x0E */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x0F */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x10 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x11 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x12 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x13 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x14 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x15 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x16 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x17 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x18 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x19 */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x1A */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x1B */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x1C */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x1D */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x1E */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x1F */	CHPLAN_ENT_NOT_DEFINED,
	/* 0x20 */	CHPLAN_ENT(WW   , 01, NA   , 00),
	/* 0x21 */	CHPLAN_ENT(ETSI , 02, NA   , 00),
	/* 0x22 */	CHPLAN_ENT(FCC  , 03, NA   , 00),
	/* 0x23 */	CHPLAN_ENT(MKK  , 04, NA   , 00),
	/* 0x24 */	CHPLAN_ENT(ETSI , 05, NA   , 00),
	/* 0x25 */	CHPLAN_ENT(FCC  , 03, FCC  , 03),
	/* 0x26 */	CHPLAN_ENT(ETSI , 01, ETSI , 02),
	/* 0x27 */	CHPLAN_ENT(MKK  , 04, MKK  , 02),
	/* 0x28 */	CHPLAN_ENT(KCC  , 01, KCC  , 05),
	/* 0x29 */	CHPLAN_ENT(FCC  , 01, FCC  , 06),
	/* 0x2A */	CHPLAN_ENT(FCC  , 02, NA   , 00),
	/* 0x2B */	CHPLAN_ENT(IC   , 02, IC   , 33),
	/* 0x2C */	CHPLAN_ENT(MKK  , 02, NA   , 00),
	/* 0x2D */	CHPLAN_ENT(CHILE, 01, CHILE, 22),
	/* 0x2E */	CHPLAN_ENT(WW   , 03, WW   , 37),
	/* 0x2F */	CHPLAN_ENT(CHILE, 01, CHILE, 38),
	/* 0x30 */	CHPLAN_ENT(FCC  , 01, FCC  , 07),
	/* 0x31 */	CHPLAN_ENT(FCC  , 01, FCC  , 08),
	/* 0x32 */	CHPLAN_ENT(FCC  , 01, FCC  , 09),
	/* 0x33 */	CHPLAN_ENT(FCC  , 01, FCC  , 10),
	/* 0x34 */	CHPLAN_ENT(FCC  , 03, FCC  , 01),
	/* 0x35 */	CHPLAN_ENT(ETSI , 01, ETSI , 03),
	/* 0x36 */	CHPLAN_ENT(ETSI , 01, ETSI , 04),
	/* 0x37 */	CHPLAN_ENT(MKK  , 04, MKK  , 10),
	/* 0x38 */	CHPLAN_ENT(MKK  , 04, MKK  , 11),
	/* 0x39 */	CHPLAN_ENT(NCC  , 03, NCC  , 12),
	/* 0x3A */	CHPLAN_ENT(ETSI , 02, ETSI , 02),
	/* 0x3B */	CHPLAN_ENT(ACMA , 02, ACMA , 01),
	/* 0x3C */	CHPLAN_ENT(ETSI , 02, ETSI , 10),
	/* 0x3D */	CHPLAN_ENT(ETSI , 02, ETSI , 15),
	/* 0x3E */	CHPLAN_ENT(KCC  , 02, KCC  , 03),
	/* 0x3F */	CHPLAN_ENT(FCC  , 03, FCC  , 22),
	/* 0x40 */	CHPLAN_ENT(NCC  , 03, NCC  , 13),
	/* 0x41 */	CHPLAN_ENT(WW   , 06, NA   , 00),
	/* 0x42 */	CHPLAN_ENT(ETSI , 02, ETSI , 14),
	/* 0x43 */	CHPLAN_ENT(FCC  , 03, FCC  , 06),
	/* 0x44 */	CHPLAN_ENT(NCC  , 03, NCC  , 09),
	/* 0x45 */	CHPLAN_ENT(ACMA , 01, ACMA , 01),
	/* 0x46 */	CHPLAN_ENT(FCC  , 03, FCC  , 15),
	/* 0x47 */	CHPLAN_ENT(ETSI , 01, ETSI , 10),
	/* 0x48 */	CHPLAN_ENT(ETSI , 01, ETSI , 07),
	/* 0x49 */	CHPLAN_ENT(ETSI , 01, ETSI , 06),
	/* 0x4A */	CHPLAN_ENT(IC   , 03, IC   , 33),
	/* 0x4B */	CHPLAN_ENT(KCC  , 02, KCC  , 22),
	/* 0x4C */	CHPLAN_ENT(FCC  , 03, FCC  , 28),
	/* 0x4D */	CHPLAN_ENT(MEX  , 02, MEX  , 01),
	/* 0x4E */	CHPLAN_ENT(ETSI , 02, ETSI , 42),
	/* 0x4F */	CHPLAN_ENT(NA   , 00, MKK  , 43),
	/* 0x50 */	CHPLAN_ENT(ETSI , 01, ETSI , 16),
	/* 0x51 */	CHPLAN_ENT(ETSI , 01, ETSI , 09),
	/* 0x52 */	CHPLAN_ENT(ETSI , 01, ETSI , 17),
	/* 0x53 */	CHPLAN_ENT(NCC  , 03, NCC  , 18),
	/* 0x54 */	CHPLAN_ENT(ETSI , 01, ETSI , 15),
	/* 0x55 */	CHPLAN_ENT(FCC  , 03, FCC  , 01),
	/* 0x56 */	CHPLAN_ENT(ETSI , 01, ETSI , 19),
	/* 0x57 */	CHPLAN_ENT(FCC  , 03, FCC  , 20),
	/* 0x58 */	CHPLAN_ENT(MKK  , 02, MKK  , 14),
	/* 0x59 */	CHPLAN_ENT(ETSI , 01, ETSI , 21),
	/* 0x5A */	CHPLAN_ENT(NA   , 00, FCC  , 44),
	/* 0x5B */	CHPLAN_ENT(NA   , 00, FCC  , 45),
	/* 0x5C */	CHPLAN_ENT(NA   , 00, FCC  , 43),
	/* 0x5D */	CHPLAN_ENT(ETSI , 02, ETSI , 08),
	/* 0x5E */	CHPLAN_ENT(ETSI , 02, ETSI , 03),
	/* 0x5F */	CHPLAN_ENT(MKK  , 02, MKK  , 47),
	/* 0x60 */	CHPLAN_ENT(FCC  , 03, FCC  , 09),
	/* 0x61 */	CHPLAN_ENT(FCC  , 02, FCC  , 01),
	/* 0x62 */	CHPLAN_ENT(FCC  , 02, FCC  , 03),
	/* 0x63 */	CHPLAN_ENT(ETSI , 01, ETSI , 23),
	/* 0x64 */	CHPLAN_ENT(MKK  , 02, MKK  , 24),
	/* 0x65 */	CHPLAN_ENT(ETSI , 02, ETSI , 24),
	/* 0x66 */	CHPLAN_ENT(FCC  , 03, FCC  , 27),
	/* 0x67 */	CHPLAN_ENT(FCC  , 03, FCC  , 25),
	/* 0x68 */	CHPLAN_ENT(FCC  , 02, FCC  , 27),
	/* 0x69 */	CHPLAN_ENT(FCC  , 02, FCC  , 25),
	/* 0x6A */	CHPLAN_ENT(ETSI , 02, ETSI , 25),
	/* 0x6B */	CHPLAN_ENT(FCC  , 01, FCC  , 29),
	/* 0x6C */	CHPLAN_ENT(FCC  , 01, FCC  , 26),
	/* 0x6D */	CHPLAN_ENT(FCC  , 02, FCC  , 28),
	/* 0x6E */	CHPLAN_ENT(FCC  , 01, FCC  , 25),
	/* 0x6F */	CHPLAN_ENT(NA   , 00, ETSI , 06),
	/* 0x70 */	CHPLAN_ENT(NA   , 00, ETSI , 30),
	/* 0x71 */	CHPLAN_ENT(NA   , 00, ETSI , 25),
	/* 0x72 */	CHPLAN_ENT(NA   , 00, ETSI , 31),
	/* 0x73 */	CHPLAN_ENT(FCC  , 01, FCC  , 01),
	/* 0x74 */	CHPLAN_ENT(FCC  , 02, FCC  , 19),
	/* 0x75 */	CHPLAN_ENT(ETSI , 01, ETSI , 32),
	/* 0x76 */	CHPLAN_ENT(FCC  , 02, FCC  , 22),
	/* 0x77 */	CHPLAN_ENT(ETSI , 01, ETSI , 34),
	/* 0x78 */	CHPLAN_ENT(FCC  , 03, FCC  , 35),
	/* 0x79 */	CHPLAN_ENT(MKK  , 02, MKK  , 02),
	/* 0x7A */	CHPLAN_ENT(ETSI , 02, ETSI , 28),
	/* 0x7B */	CHPLAN_ENT(ETSI , 02, ETSI , 46),
	/* 0x7C */	CHPLAN_ENT(ETSI , 02, ETSI , 47),
	/* 0x7D */	CHPLAN_ENT(MKK  , 04, MKK  , 48),
	/* 0x7E */	CHPLAN_ENT(MKK  , 02, MKK  , 48),
	/* 0x7F */	CHPLAN_ENT(FCC  , 01, FCC  , 03),
};

const int RTW_ChannelPlanMap_size = sizeof(RTW_ChannelPlanMap) / sizeof(RTW_ChannelPlanMap[0]);

u8 rtw_chplan_get_default_regd_2g(u8 id)
{
	return RTW_ChannelPlanMap[id].regd_2g;
}

u8 rtw_chplan_get_default_regd_5g(u8 id)
{
#if CONFIG_IEEE80211_BAND_5GHZ
	return RTW_ChannelPlanMap[id].regd_5g;
#else
	return TXPWR_LMT_NONE;
#endif
}

u8 rtw_chplan_get_default_regd(u8 id)
{
	u8 regd_2g = rtw_chplan_get_default_regd_2g(id);
	u8 regd_5g = rtw_chplan_get_default_regd_5g(id);

	if (regd_2g != RTW_REGD_NA && regd_5g != RTW_REGD_NA) {
		if (regd_2g != regd_5g)
			RTW_WARN("channel_plan:0x%02x, regd_2g:%u, regd_5g:%u not the same\n", id, regd_2g, regd_5g);
		return regd_5g;
	}
	return regd_2g != RTW_REGD_NA ? regd_2g : regd_5g;
}

bool rtw_chplan_is_empty(u8 id)
{
	const struct chplan_ent_t *chplan_map = &RTW_ChannelPlanMap[id];

	if (chplan_map->chd_2g == RTW_CHD_2G_NULL
		#if CONFIG_IEEE80211_BAND_5GHZ
		&& chplan_map->chd_5g == RTW_CHD_5G_NULL
		#endif
	)
		return _TRUE;

	return _FALSE;
}

bool rtw_is_channel_plan_valid(u8 id)
{
	return id < RTW_ChannelPlanMap_size && !rtw_chplan_is_empty(id);
}

bool rtw_regsty_is_excl_chs(struct registry_priv *regsty, u8 ch)
{
	int i;

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		if (regsty->excl_chs[i] == 0)
			break;
		if (regsty->excl_chs[i] == ch)
			return _TRUE;
	}
	return _FALSE;
}

const char *_regd_src_str[] = {
	[REGD_SRC_RTK_PRIV] = "RTK_PRIV",
	[REGD_SRC_OS] = "OS",
	[REGD_SRC_NUM] = "UNKNOWN",
};

static u8 init_channel_set_from_rtk_priv(_adapter *padapter, RT_CHANNEL_INFO *channel_set)
{
	struct rf_ctl_t *rfctl = adapter_to_rfctl(padapter);
	struct registry_priv *regsty = adapter_to_regsty(padapter);
	u8 ChannelPlan = rfctl->ChannelPlan;
	u8 index, chanset_size = 0;
	u8 b5GBand = _FALSE, b2_4GBand = _FALSE;
	u8 ch, attrib;
#ifdef CONFIG_DFS_MASTER
	int i;
#endif

	if (!rtw_is_channel_plan_valid(ChannelPlan)) {
		RTW_ERR("ChannelPlan ID 0x%02X error !!!!!\n", ChannelPlan);
		return chanset_size;
	}

	_rtw_memset(channel_set, 0, sizeof(RT_CHANNEL_INFO) * MAX_CHANNEL_NUM);

	if (IsSupported24G(regsty->wireless_mode) && hal_chk_band_cap(padapter, BAND_CAP_2G))
		b2_4GBand = _TRUE;

	if (is_supported_5g(regsty->wireless_mode) && hal_chk_band_cap(padapter, BAND_CAP_5G))
		b5GBand = _TRUE;

	if (b2_4GBand == _FALSE && b5GBand == _FALSE) {
		RTW_WARN("HW band_cap has no intersection with SW wireless_mode setting\n");
		return chanset_size;
	}

	if (b2_4GBand) {
		u8 chd_2g = RTW_ChannelPlanMap[ChannelPlan].chd_2g;

		attrib = CH_LIST_ATTRIB(rtw_channel_def_2g[chd_2g]);

		for (index = 0; index < CH_LIST_LEN(rtw_channel_def_2g[chd_2g]); index++) {
			ch = CH_LIST_CH(rtw_channel_def_2g[chd_2g], index);
			if (rtw_regsty_is_excl_chs(regsty, ch) == _TRUE)
				continue;

			if (chanset_size >= MAX_CHANNEL_NUM) {
				RTW_WARN("chset size can't exceed MAX_CHANNEL_NUM(%u)\n", MAX_CHANNEL_NUM);
				break;
			}

			channel_set[chanset_size].ChannelNum = ch;

			if (ch >= 12 && ch <= 14 && (attrib & CLA_2G_12_14_PASSIVE))
				channel_set[chanset_size].flags |= RTW_CHF_NO_IR;

			if (channel_set[chanset_size].flags & RTW_CHF_NO_IR) {
				if (rfctl->country_ent || ch <= 11)
					RTW_INFO("ch%u is PASSIVE\n", ch);
			}

			chanset_size++;
		}
	}

#if CONFIG_IEEE80211_BAND_5GHZ
	if (b5GBand) {
		bool dfs;
		u8 chd_5g = RTW_ChannelPlanMap[ChannelPlan].chd_5g;

		attrib = CH_LIST_ATTRIB(rtw_channel_def_5g[chd_5g]);

		for (index = 0; index < CH_LIST_LEN(rtw_channel_def_5g[chd_5g]); index++) {
			ch = CH_LIST_CH(rtw_channel_def_5g[chd_5g], index);
			if (rtw_regsty_is_excl_chs(regsty, ch) == _TRUE)
				continue;
			dfs = (rtw_is_5g_band2(ch) && (attrib & CLA_5G_B2_DFS))
				|| (rtw_is_5g_band3(ch) && (attrib & CLA_5G_B3_DFS))
				|| (rtw_is_5g_band4(ch) && (attrib & CLA_5G_B4_DFS));
			#if !CONFIG_DFS
			if (dfs)
				continue;
			#endif

			if (chanset_size >= MAX_CHANNEL_NUM) {
				RTW_WARN("chset size can't exceed MAX_CHANNEL_NUM(%u)\n", MAX_CHANNEL_NUM);
				break;
			}

			channel_set[chanset_size].ChannelNum = ch;

			if ((rtw_is_5g_band1(ch) && (attrib & CLA_5G_B1_PASSIVE)) /* band1 passive */
				|| (rtw_is_5g_band2(ch) && (attrib & CLA_5G_B2_PASSIVE)) /* band2 passive */
				|| (rtw_is_5g_band3(ch) && (attrib & CLA_5G_B3_PASSIVE)) /* band3 passive */
				|| (rtw_is_5g_band4(ch) && (attrib & CLA_5G_B4_PASSIVE)) /* band4 passive */
			)
				channel_set[chanset_size].flags |= RTW_CHF_NO_IR;

			if (dfs)
				channel_set[chanset_size].flags |= RTW_CHF_DFS;

			if (channel_set[chanset_size].flags & RTW_CHF_NO_IR) {
				if (rfctl->country_ent || (channel_set[chanset_size].flags & RTW_CHF_DFS))
					RTW_INFO("ch%u is PASSIVE%s\n", ch, dfs ? " DFS" : "");
			}

			chanset_size++;
		}
	}

	#ifdef CONFIG_DFS_MASTER
	for (i = 0; i < chanset_size; i++)
		channel_set[i].non_ocp_end_time = rtw_get_current_time();
	#endif
#endif /* CONFIG_IEEE80211_BAND_5GHZ */

	if (chanset_size)
		RTW_INFO(FUNC_ADPT_FMT" ChannelPlan ID:0x%02x, ch num:%d\n"
			, FUNC_ADPT_ARG(padapter), ChannelPlan, chanset_size);
	else
		RTW_WARN(FUNC_ADPT_FMT" ChannelPlan ID:0x%02x, final chset has no channel\n"
			, FUNC_ADPT_ARG(padapter), ChannelPlan);

	return chanset_size;
}

u8 init_channel_set(_adapter *adapter)
{
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);

	if (rfctl->regd_src == REGD_SRC_RTK_PRIV)
		return init_channel_set_from_rtk_priv(adapter, rfctl->channel_set);
#ifdef CONFIG_REGD_SRC_FROM_OS
	else if (rfctl->regd_src == REGD_SRC_OS)
		return rtw_os_init_channel_set(adapter, rfctl->channel_set);
#endif
	else
		rtw_warn_on(1);

	return 0;
}

bool rtw_chset_is_dfs_range(struct _RT_CHANNEL_INFO *chset, u32 hi, u32 lo)
{
	u8 hi_ch = rtw_freq2ch(hi);
	u8 lo_ch = rtw_freq2ch(lo);
	int i;

	for (i = 0; i < MAX_CHANNEL_NUM && chset[i].ChannelNum != 0; i++){
		if (!(chset[i].flags & RTW_CHF_DFS))
			continue;
		if (hi_ch > chset[i].ChannelNum && lo_ch < chset[i].ChannelNum)
			return 1;
	}

	return 0;
}

bool rtw_chset_is_dfs_ch(struct _RT_CHANNEL_INFO *chset, u8 ch)
{
	int i;

	for (i = 0; i < MAX_CHANNEL_NUM && chset[i].ChannelNum != 0; i++){
		if (chset[i].ChannelNum == ch)
			return chset[i].flags & RTW_CHF_DFS ? 1 : 0;
	}

	return 0;
}

bool rtw_chset_is_dfs_chbw(struct _RT_CHANNEL_INFO *chset, u8 ch, u8 bw, u8 offset)
{
	u32 hi, lo;

	if (!rtw_chbw_to_freq_range(ch, bw, offset, &hi, &lo))
		return 0;

	return rtw_chset_is_dfs_range(chset, hi, lo);
}

u8 rtw_process_beacon_hint(_adapter *adapter, WLAN_BSSID_EX *bss)
{
#ifndef RTW_CHPLAN_BEACON_HINT_NON_WORLD_WIDE
#define RTW_CHPLAN_BEACON_HINT_NON_WORLD_WIDE 0
#endif

#ifndef RTW_CHPLAN_BEACON_HINT_ON_2G_CH_1_11
#define RTW_CHPLAN_BEACON_HINT_ON_2G_CH_1_11 0
#endif

#ifndef RTW_CHPLAN_BEACON_HINT_ON_DFS_CH
#define RTW_CHPLAN_BEACON_HINT_ON_DFS_CH 0
#endif

	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	RT_CHANNEL_INFO *chset = rfctl->channel_set;
	u8 ch = bss->Configuration.DSConfig;
	int chset_idx = rtw_chset_search_ch(chset, ch);
	u8 act_cnt = 0;

	if (chset_idx < 0)
		goto exit;

	if ((chset[chset_idx].flags & RTW_CHF_NO_IR)
		&& (RTW_CHPLAN_BEACON_HINT_NON_WORLD_WIDE || !rfctl->country_ent || IS_ALPHA2_WORLDWIDE(rfctl->country_ent->alpha2))
		&& (RTW_CHPLAN_BEACON_HINT_ON_2G_CH_1_11 || !(ch <= 11))
		&& (RTW_CHPLAN_BEACON_HINT_ON_DFS_CH || !(chset[chset_idx].flags & RTW_CHF_DFS))
	) {
		RTW_INFO("%s: change ch:%d to active\n", __func__, ch);
		chset[chset_idx].flags &= ~RTW_CHF_NO_IR;
		act_cnt++;
	}

exit:
	return act_cnt;
}

const char *const _regd_str[] = {
	[RTW_REGD_NA]		= "NA",
	[RTW_REGD_FCC]		= "FCC",
	[RTW_REGD_MKK]		= "MKK",
	[RTW_REGD_ETSI]		= "ETSI",
	[RTW_REGD_IC]		= "IC",
	[RTW_REGD_KCC]		= "KCC",
	[RTW_REGD_NCC]		= "NCC",
	[RTW_REGD_ACMA]		= "ACMA",
	[RTW_REGD_CHILE]	= "CHILE",
	[RTW_REGD_MEX]		= "MEX",
	[RTW_REGD_WW]		= "WW",
};

const char *const _rtw_edcca_mode_str[] = {
	[RTW_EDCCA_NORMAL]	= "NORMAL",
	[RTW_EDCCA_ADAPT]	= "ADAPT",
	[RTW_EDCCA_CS]		= "CS",
};

const char *_rtw_dfs_regd_str[] = {
	[RTW_DFS_REGD_NONE]	= "NONE",
	[RTW_DFS_REGD_FCC]	= "FCC",
	[RTW_DFS_REGD_MKK]	= "MKK",
	[RTW_DFS_REGD_ETSI]	= "ETSI",
};

const char *const _txpwr_lmt_str[] = {
	[TXPWR_LMT_NONE]	= "NONE",
	[TXPWR_LMT_FCC]		= "FCC",
	[TXPWR_LMT_MKK]		= "MKK",
	[TXPWR_LMT_ETSI]	= "ETSI",
	[TXPWR_LMT_IC]		= "IC",
	[TXPWR_LMT_KCC]		= "KCC",
	[TXPWR_LMT_NCC]		= "NCC",
	[TXPWR_LMT_ACMA]	= "ACMA",
	[TXPWR_LMT_CHILE]	= "CHILE",
	[TXPWR_LMT_UKRAINE]	= "UKRAINE",
	[TXPWR_LMT_MEXICO]	= "MEXICO",
	[TXPWR_LMT_CN]		= "CN",
	[TXPWR_LMT_QATAR]	= "QATAR",
	[TXPWR_LMT_WW]		= "WW",
};

const REGULATION_TXPWR_LMT _txpwr_lmt_alternate[] = {
	[TXPWR_LMT_NONE]	= TXPWR_LMT_NONE,
	[TXPWR_LMT_FCC]		= TXPWR_LMT_FCC,
	[TXPWR_LMT_MKK]		= TXPWR_LMT_MKK,
	[TXPWR_LMT_ETSI]	= TXPWR_LMT_ETSI,
	[TXPWR_LMT_WW]		= TXPWR_LMT_WW,

	[TXPWR_LMT_IC]		= TXPWR_LMT_FCC,
	[TXPWR_LMT_KCC]		= TXPWR_LMT_ETSI,
	[TXPWR_LMT_NCC]		= TXPWR_LMT_FCC,
	[TXPWR_LMT_ACMA]	= TXPWR_LMT_ETSI,
	[TXPWR_LMT_CHILE]	= TXPWR_LMT_FCC,
	[TXPWR_LMT_UKRAINE]	= TXPWR_LMT_ETSI,
	[TXPWR_LMT_MEXICO]	= TXPWR_LMT_FCC,
	[TXPWR_LMT_CN]		= TXPWR_LMT_ETSI,
	[TXPWR_LMT_QATAR]	= TXPWR_LMT_ETSI,
};

const enum rtw_edcca_mode _rtw_regd_to_edcca_mode[RTW_REGD_NUM] = {
	[RTW_REGD_NA] = RTW_EDCCA_MODE_NUM,
	[RTW_REGD_MKK] = RTW_EDCCA_CS,
	[RTW_REGD_ETSI] = RTW_EDCCA_ADAPT,
	[RTW_REGD_WW] = RTW_EDCCA_ADAPT,
};

const REGULATION_TXPWR_LMT _rtw_regd_to_txpwr_lmt[] = {
	[RTW_REGD_NA]		= TXPWR_LMT_NONE,
	[RTW_REGD_FCC]		= TXPWR_LMT_FCC,
	[RTW_REGD_MKK]		= TXPWR_LMT_MKK,
	[RTW_REGD_ETSI]		= TXPWR_LMT_ETSI,
	[RTW_REGD_IC]		= TXPWR_LMT_IC,
	[RTW_REGD_KCC]		= TXPWR_LMT_KCC,
	[RTW_REGD_NCC]		= TXPWR_LMT_NCC,
	[RTW_REGD_ACMA]		= TXPWR_LMT_ACMA,
	[RTW_REGD_CHILE]	= TXPWR_LMT_CHILE,
	[RTW_REGD_MEX]		= TXPWR_LMT_MEXICO,
	[RTW_REGD_WW]		= TXPWR_LMT_WW,
};

char *rtw_get_edcca_modes_str(char *buf, u8 modes[])
{
#define EDCCA_MODE_SEQ_COMPARE(result, operand) (result == RTW_EDCCA_MODE_NUM ? operand : (operand == RTW_EDCCA_MODE_NUM ? result : (result != operand ? -1 : result)))

	int mode = RTW_EDCCA_MODE_NUM;
	int cnt = 0;

	mode = EDCCA_MODE_SEQ_COMPARE(mode, modes[BAND_ON_2_4G]);
#if CONFIG_IEEE80211_BAND_5GHZ
	mode = EDCCA_MODE_SEQ_COMPARE(mode, modes[BAND_ON_5G]);
#endif

	if (mode != -1) { /* all available values are the same */
		cnt += snprintf(buf + cnt, EDCCA_MODES_STR_LEN - cnt - 1, "%s(%u) ", rtw_edcca_mode_str(mode), mode);
		if (cnt >= EDCCA_MODES_STR_LEN - 1)
			goto exit;
	} else {
		cnt += snprintf(buf + cnt, EDCCA_MODES_STR_LEN - cnt - 1, "%s(%u) ", rtw_edcca_mode_str(modes[BAND_ON_2_4G]), modes[BAND_ON_2_4G]);
		if (cnt >= EDCCA_MODES_STR_LEN - 1)
			goto exit;
		#if CONFIG_IEEE80211_BAND_5GHZ
		cnt += snprintf(buf + cnt, EDCCA_MODES_STR_LEN - cnt - 1, "%s(%u) ", rtw_edcca_mode_str(modes[BAND_ON_5G]), modes[BAND_ON_5G]);
		if (cnt >= EDCCA_MODES_STR_LEN - 1)
			goto exit;
		#endif
	}

exit:
	return buf;
}

void rtw_edcca_mode_update(struct dvobj_priv *dvobj)
{
	struct registry_priv *regsty = dvobj_to_regsty(dvobj);
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(dvobj);

	if (regsty->adaptivity_en == 0) {
		/* force disable */
		rfctl->edcca_mode_2g = RTW_EDCCA_NORMAL;
		#if CONFIG_IEEE80211_BAND_5GHZ
		rfctl->edcca_mode_5g = RTW_EDCCA_NORMAL;
		#endif

	} else if (regsty->adaptivity_en == 1) {
		/* force enable */
		if (!regsty->adaptivity_mode) {
			/* adaptivity */
			rfctl->edcca_mode_2g = RTW_EDCCA_ADAPT;
			#if CONFIG_IEEE80211_BAND_5GHZ
			rfctl->edcca_mode_5g = RTW_EDCCA_ADAPT;
			#endif
		} else {
			/* carrier sense */
			rfctl->edcca_mode_2g = RTW_EDCCA_CS;
			#if CONFIG_IEEE80211_BAND_5GHZ
			rfctl->edcca_mode_5g = RTW_EDCCA_CS;
			#endif
		}

	} else {
		u8 modes[BAND_MAX];
		char buf[EDCCA_MODES_STR_LEN];

		/* by regulatory setting */
		#ifdef CONFIG_REGD_SRC_FROM_OS
		if (rfctl->regd_src == REGD_SRC_OS
			&& rfctl->ChannelPlan == RTW_CHPLAN_UNSPECIFIED
		) {
			modes[BAND_ON_2_4G] = rfctl->edcca_mode_2g = RTW_EDCCA_ADAPT;
			#if CONFIG_IEEE80211_BAND_5GHZ
			modes[BAND_ON_5G] = rfctl->edcca_mode_5g = RTW_EDCCA_ADAPT;
			#endif
			RTW_PRINT("mapping %scountry:%c%c to edcca_mode:%s\n"
				, IS_ALPHA2_WORLDWIDE(rfctl->country_ent->alpha2) ? "" : "unsupported "
				, rfctl->country_ent->alpha2[0]
				, rfctl->country_ent->alpha2[1]
				, rtw_get_edcca_modes_str(buf, modes)
			);
		} else
		#endif
		{
			modes[BAND_ON_2_4G] = rfctl->edcca_mode_2g = rtw_regd_to_edcca_mode(rtw_chplan_get_default_regd_2g(rfctl->ChannelPlan));
			#if CONFIG_IEEE80211_BAND_5GHZ
			modes[BAND_ON_5G] = rfctl->edcca_mode_5g = rtw_regd_to_edcca_mode(rtw_chplan_get_default_regd_5g(rfctl->ChannelPlan));
			#endif
			RTW_PRINT("default mapping domain:0x%02x to edcca_mode:%s\n"
				, rfctl->ChannelPlan
				, rtw_get_edcca_modes_str(buf, modes)
			);
		}
	}
}

u8 rtw_get_edcca_mode(struct dvobj_priv *dvobj, BAND_TYPE band)
{
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(dvobj);
	u8 edcca_mode = RTW_EDCCA_NORMAL;

	if (band == BAND_ON_2_4G)
		edcca_mode = rfctl->edcca_mode_2g;
	#if CONFIG_IEEE80211_BAND_5GHZ
	else if (band == BAND_ON_5G)
		edcca_mode = rfctl->edcca_mode_5g;
	#endif

	return edcca_mode;
}

#if defined(CONFIG_80211AC_VHT) || defined(CONFIG_80211AX_HE)
#define COUNTRY_CHPLAN_ASSIGN_PROTO_EN(_val) , .proto_en = (_val)
#else
#define COUNTRY_CHPLAN_ASSIGN_PROTO_EN(_val)
#endif

#define COUNTRY_CHPLAN_ENT(_alpha2, _chplan, _txpwr_lmt_override, _en_11ax, _en_11ac) \
	{.alpha2 = (_alpha2), .chplan = (_chplan), .txpwr_lmt_override = TXPWR_LMT_##_txpwr_lmt_override \
		COUNTRY_CHPLAN_ASSIGN_PROTO_EN((_en_11ax ? CCHPLAN_PROTO_EN_AX : 0) | (_en_11ac ? CCHPLAN_PROTO_EN_AC : 0)) \
	}

#ifdef CONFIG_CUSTOMIZED_COUNTRY_CHPLAN_MAP

#include "../platform/custom_country_chplan.h"

#elif RTW_DEF_MODULE_REGULATORY_CERT

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8821AE_HMC_M2) /* 2013 certify */
static const struct country_chplan RTL8821AE_HMC_M2_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x3D, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x36, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8821AU) /* 2014 certify */
static const struct country_chplan RTL8821AU_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x3D, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x36, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8812AENF_NGFF) /* 2014 certify */
static const struct country_chplan RTL8812AENF_NGFF_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8812AEBT_HMC) /* 2013 certify */
static const struct country_chplan RTL8812AEBT_HMC_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x36, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8188EE_HMC_M2) /* 2012 certify */
static const struct country_chplan RTL8188EE_HMC_M2_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AW", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BB", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x20, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ET", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GD", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x5D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ML", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8723BE_HMC_M2) /* 2013 certify */
static const struct country_chplan RTL8723BE_HMC_M2_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AO", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AW", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BS", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x20, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ET", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GD", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x5D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ML", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8723BS_NGFF1216) /* 2014 certify */
static const struct country_chplan RTL8723BS_NGFF1216_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AO", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BB", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x20, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x5D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("YE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8192EEBT_HMC_M2) /* 2013 certify */
static const struct country_chplan RTL8192EEBT_HMC_M2_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AO", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AW", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x20, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EH", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ET", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GD", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x5D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ML", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SC", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ST", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x39, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("YT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8723DE_NGFF1630) /* 2016 certify */
static const struct country_chplan RTL8723DE_NGFF1630_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x2A, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x5D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x34, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8822BE) /* 2016 certify */
static const struct country_chplan RTL8822BE_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AO", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BB", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BM", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BS", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x2B, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EH", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FJ", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GP", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GU", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x3D, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ML", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MO", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x4D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ST", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, UKRAINE, 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("YT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8821CE) /* 2016 certify */
static const struct country_chplan RTL8821CE_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AO", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BB", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BM", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BS", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x2B, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EH", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ET", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FJ", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GP", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GU", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ID", 0x3D, DEF    , 0, 0),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x28, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ML", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MO", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x4D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ST", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, UKRAINE, 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("YT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

#if (RTW_DEF_MODULE_REGULATORY_CERT & RTW_MODULE_RTL8822CE) /* 2018 certify */
static const struct country_chplan RTL8822CE_country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AE", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AO", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AR", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AU", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AW", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("AZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BB", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BH", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BM", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BO", 0x73, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BS", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BW", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("BY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CA", 0x2B, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CL", 0x2D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CN", 0x48, CN     , 0, 1),
	COUNTRY_CHPLAN_ENT("CO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CY", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("CZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DO", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EG", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("EH", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ES", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ET", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FJ", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("FR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GP", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GT", 0x61, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GU", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("GW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HK", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HT", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("HU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IL", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IN", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("IT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JM", 0x32, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JO", 0x49, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("JP", 0x27, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KE", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KH", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KR", 0x4B, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("KZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LB", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("LV", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MA", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MC", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ME", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ML", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MO", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MQ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MU", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MW", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MX", 0x4D, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MY", 0x63, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("MZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NA", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NI", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NP", 0x48, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("NZ", 0x45, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("OM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PA", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PE", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PR", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("PY", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("QA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RO", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RS", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SC", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SE", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SG", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SI", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SK", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SL", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SN", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ST", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SV", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("SZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TD", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TF", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TH", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TJ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TN", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TR", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TW", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("TZ", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UA", 0x35, UKRAINE, 0, 1),
	COUNTRY_CHPLAN_ENT("UG", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("US", 0x76, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("UZ", 0x47, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("VN", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("YT", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZA", 0x35, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZM", 0x26, DEF    , 0, 1),
	COUNTRY_CHPLAN_ENT("ZW", 0x26, DEF    , 0, 1),
};
#endif

/**
 * rtw_def_module_country_chplan_map -
 * @hal_map: returned map
 * @return: size of map
 */
static u16 rtw_def_module_country_chplan_map(const struct country_chplan **hal_map)
{
	u16 hal_map_sz = 0;

	/* TODO: runtime selection for multi driver */
#if (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8821AE_HMC_M2)
	*hal_map = RTL8821AE_HMC_M2_country_chplan_map;
	hal_map_sz = sizeof(RTL8821AE_HMC_M2_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8821AU)
	*hal_map = RTL8821AU_country_chplan_map;
	hal_map_sz = sizeof(RTL8821AU_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8812AENF_NGFF)
	*hal_map = RTL8812AENF_NGFF_country_chplan_map;
	hal_map_sz = sizeof(RTL8812AENF_NGFF_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8812AEBT_HMC)
	*hal_map = RTL8812AEBT_HMC_country_chplan_map;
	hal_map_sz = sizeof(RTL8812AEBT_HMC_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8188EE_HMC_M2)
	*hal_map = RTL8188EE_HMC_M2_country_chplan_map;
	hal_map_sz = sizeof(RTL8188EE_HMC_M2_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8723BE_HMC_M2)
	*hal_map = RTL8723BE_HMC_M2_country_chplan_map;
	hal_map_sz = sizeof(RTL8723BE_HMC_M2_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8723BS_NGFF1216)
	*hal_map = RTL8723BS_NGFF1216_country_chplan_map;
	hal_map_sz = sizeof(RTL8723BS_NGFF1216_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8192EEBT_HMC_M2)
	*hal_map = RTL8192EEBT_HMC_M2_country_chplan_map;
	hal_map_sz = sizeof(RTL8192EEBT_HMC_M2_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8723DE_NGFF1630)
	*hal_map = RTL8723DE_NGFF1630_country_chplan_map;
	hal_map_sz = sizeof(RTL8723DE_NGFF1630_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8822BE)
	*hal_map = RTL8822BE_country_chplan_map;
	hal_map_sz = sizeof(RTL8822BE_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8821CE)
	*hal_map = RTL8821CE_country_chplan_map;
	hal_map_sz = sizeof(RTL8821CE_country_chplan_map) / sizeof(struct country_chplan);
#elif (RTW_DEF_MODULE_REGULATORY_CERT == RTW_MODULE_RTL8822CE)
	*hal_map = RTL8822CE_country_chplan_map;
	hal_map_sz = sizeof(RTL8822CE_country_chplan_map) / sizeof(struct country_chplan);
#endif

	return hal_map_sz;
}
#else

static const struct country_chplan country_chplan_map[] = {
	COUNTRY_CHPLAN_ENT("AD", 0x3A, DEF    , 1, 1), /* Andorra */
	COUNTRY_CHPLAN_ENT("AE", 0x5E, DEF    , 1, 1), /* United Arab Emirates */
	COUNTRY_CHPLAN_ENT("AF", 0x42, DEF    , 1, 1), /* Afghanistan */
	COUNTRY_CHPLAN_ENT("AG", 0x76, DEF    , 1, 1), /* Antigua & Barbuda */
	COUNTRY_CHPLAN_ENT("AI", 0x5E, DEF    , 1, 1), /* Anguilla(UK) */
	COUNTRY_CHPLAN_ENT("AL", 0x5E, DEF    , 1, 1), /* Albania */
	COUNTRY_CHPLAN_ENT("AM", 0x5E, DEF    , 1, 1), /* Armenia */
	COUNTRY_CHPLAN_ENT("AN", 0x76, DEF    , 1, 1), /* Netherlands Antilles */
	COUNTRY_CHPLAN_ENT("AO", 0x5E, DEF    , 1, 1), /* Angola */
	COUNTRY_CHPLAN_ENT("AQ", 0x26, DEF    , 1, 1), /* Antarctica */
	COUNTRY_CHPLAN_ENT("AR", 0x4D, DEF    , 1, 1), /* Argentina */
	COUNTRY_CHPLAN_ENT("AS", 0x76, DEF    , 1, 1), /* American Samoa */
	COUNTRY_CHPLAN_ENT("AT", 0x5E, DEF    , 1, 1), /* Austria */
	COUNTRY_CHPLAN_ENT("AU", 0x03, DEF    , 1, 1), /* Australia */
	COUNTRY_CHPLAN_ENT("AW", 0x76, DEF    , 1, 1), /* Aruba */
	COUNTRY_CHPLAN_ENT("AZ", 0x5E, DEF    , 1, 1), /* Azerbaijan */
	COUNTRY_CHPLAN_ENT("BA", 0x5E, DEF    , 1, 1), /* Bosnia & Herzegovina */
	COUNTRY_CHPLAN_ENT("BB", 0x76, DEF    , 1, 1), /* Barbados */
	COUNTRY_CHPLAN_ENT("BD", 0x5E, DEF    , 1, 1), /* Bangladesh */
	COUNTRY_CHPLAN_ENT("BE", 0x5E, DEF    , 1, 1), /* Belgium */
	COUNTRY_CHPLAN_ENT("BF", 0x5E, DEF    , 1, 1), /* Burkina Faso */
	COUNTRY_CHPLAN_ENT("BG", 0x5E, DEF    , 1, 1), /* Bulgaria */
	COUNTRY_CHPLAN_ENT("BH", 0x06, DEF    , 1, 1), /* Bahrain */
	COUNTRY_CHPLAN_ENT("BI", 0x3A, DEF    , 1, 1), /* Burundi */
	COUNTRY_CHPLAN_ENT("BJ", 0x3A, DEF    , 1, 1), /* Benin */
	COUNTRY_CHPLAN_ENT("BM", 0x76, DEF    , 1, 1), /* Bermuda (UK) */
	COUNTRY_CHPLAN_ENT("BN", 0x06, DEF    , 1, 1), /* Brunei */
	COUNTRY_CHPLAN_ENT("BO", 0x61, DEF    , 1, 1), /* Bolivia */
	COUNTRY_CHPLAN_ENT("BR", 0x62, DEF    , 1, 1), /* Brazil */
	COUNTRY_CHPLAN_ENT("BS", 0x76, DEF    , 1, 1), /* Bahamas */
	COUNTRY_CHPLAN_ENT("BT", 0x5E, DEF    , 1, 1), /* Bhutan */
	COUNTRY_CHPLAN_ENT("BV", 0x5E, DEF    , 1, 1), /* Bouvet Island (Norway) */
	COUNTRY_CHPLAN_ENT("BW", 0x5E, DEF    , 1, 1), /* Botswana */
	COUNTRY_CHPLAN_ENT("BY", 0x08, DEF    , 0, 1), /* Belarus */
	COUNTRY_CHPLAN_ENT("BZ", 0x76, DEF    , 1, 1), /* Belize */
	COUNTRY_CHPLAN_ENT("CA", 0x2B, DEF    , 1, 1), /* Canada */
	COUNTRY_CHPLAN_ENT("CC", 0x3A, DEF    , 1, 1), /* Cocos (Keeling) Islands (Australia) */
	COUNTRY_CHPLAN_ENT("CD", 0x3A, DEF    , 1, 1), /* Congo, Republic of the */
	COUNTRY_CHPLAN_ENT("CF", 0x3A, DEF    , 1, 1), /* Central African Republic */
	COUNTRY_CHPLAN_ENT("CG", 0x3A, DEF    , 1, 1), /* Congo, Democratic Republic of the. Zaire */
	COUNTRY_CHPLAN_ENT("CH", 0x5E, DEF    , 1, 1), /* Switzerland */
	COUNTRY_CHPLAN_ENT("CI", 0x42, DEF    , 1, 1), /* Cote d'Ivoire */
	COUNTRY_CHPLAN_ENT("CK", 0x5E, DEF    , 1, 1), /* Cook Islands */
	COUNTRY_CHPLAN_ENT("CL", 0x76, CHILE  , 1, 1), /* Chile */
	COUNTRY_CHPLAN_ENT("CM", 0x5E, DEF    , 1, 1), /* Cameroon */
	COUNTRY_CHPLAN_ENT("CN", 0x06, CN     , 1, 1), /* China */
	COUNTRY_CHPLAN_ENT("CO", 0x76, DEF    , 1, 1), /* Colombia */
	COUNTRY_CHPLAN_ENT("CR", 0x76, DEF    , 1, 1), /* Costa Rica */
	COUNTRY_CHPLAN_ENT("CV", 0x5E, DEF    , 1, 1), /* Cape Verde */
	COUNTRY_CHPLAN_ENT("CX", 0x03, DEF    , 1, 1), /* Christmas Island (Australia) */
	COUNTRY_CHPLAN_ENT("CY", 0x5E, DEF    , 1, 1), /* Cyprus */
	COUNTRY_CHPLAN_ENT("CZ", 0x5E, DEF    , 1, 1), /* Czech Republic */
	COUNTRY_CHPLAN_ENT("DE", 0x5E, DEF    , 1, 1), /* Germany */
	COUNTRY_CHPLAN_ENT("DJ", 0x3A, DEF    , 1, 1), /* Djibouti */
	COUNTRY_CHPLAN_ENT("DK", 0x5E, DEF    , 1, 1), /* Denmark */
	COUNTRY_CHPLAN_ENT("DM", 0x76, DEF    , 1, 1), /* Dominica */
	COUNTRY_CHPLAN_ENT("DO", 0x76, DEF    , 1, 1), /* Dominican Republic */
	COUNTRY_CHPLAN_ENT("DZ", 0x00, DEF    , 1, 1), /* Algeria */
	COUNTRY_CHPLAN_ENT("EC", 0x76, DEF    , 1, 1), /* Ecuador */
	COUNTRY_CHPLAN_ENT("EE", 0x5E, DEF    , 1, 1), /* Estonia */
	COUNTRY_CHPLAN_ENT("EG", 0x3C, DEF    , 1, 1), /* Egypt */
	COUNTRY_CHPLAN_ENT("EH", 0x3C, DEF    , 1, 1), /* Western Sahara */
	COUNTRY_CHPLAN_ENT("ER", 0x3A, DEF    , 1, 1), /* Eritrea */
	COUNTRY_CHPLAN_ENT("ES", 0x5E, DEF    , 1, 1), /* Spain, Canary Islands, Ceuta, Melilla */
	COUNTRY_CHPLAN_ENT("ET", 0x3A, DEF    , 1, 1), /* Ethiopia */
	COUNTRY_CHPLAN_ENT("FI", 0x5E, DEF    , 1, 1), /* Finland */
	COUNTRY_CHPLAN_ENT("FJ", 0x76, DEF    , 1, 1), /* Fiji */
	COUNTRY_CHPLAN_ENT("FK", 0x3A, DEF    , 1, 1), /* Falkland Islands (Islas Malvinas) (UK) */
	COUNTRY_CHPLAN_ENT("FM", 0x76, DEF    , 1, 1), /* Micronesia, Federated States of (USA) */
	COUNTRY_CHPLAN_ENT("FO", 0x3A, DEF    , 1, 1), /* Faroe Islands (Denmark) */
	COUNTRY_CHPLAN_ENT("FR", 0x5E, DEF    , 1, 1), /* France */
	COUNTRY_CHPLAN_ENT("GA", 0x5E, DEF    , 1, 1), /* Gabon */
	COUNTRY_CHPLAN_ENT("GB", 0x5E, DEF    , 1, 1), /* Great Britain (United Kingdom; England) */
	COUNTRY_CHPLAN_ENT("GD", 0x76, DEF    , 1, 1), /* Grenada */
	COUNTRY_CHPLAN_ENT("GE", 0x5E, DEF    , 1, 1), /* Georgia */
	COUNTRY_CHPLAN_ENT("GF", 0x3A, DEF    , 1, 1), /* French Guiana */
	COUNTRY_CHPLAN_ENT("GG", 0x5E, DEF    , 1, 1), /* Guernsey (UK) */
	COUNTRY_CHPLAN_ENT("GH", 0x5E, DEF    , 1, 1), /* Ghana */
	COUNTRY_CHPLAN_ENT("GI", 0x3A, DEF    , 1, 1), /* Gibraltar (UK) */
	COUNTRY_CHPLAN_ENT("GL", 0x5E, DEF    , 1, 1), /* Greenland (Denmark) */
	COUNTRY_CHPLAN_ENT("GM", 0x3A, DEF    , 1, 1), /* Gambia */
	COUNTRY_CHPLAN_ENT("GN", 0x3A, DEF    , 1, 1), /* Guinea */
	COUNTRY_CHPLAN_ENT("GP", 0x3A, DEF    , 1, 1), /* Guadeloupe (France) */
	COUNTRY_CHPLAN_ENT("GQ", 0x5E, DEF    , 1, 1), /* Equatorial Guinea */
	COUNTRY_CHPLAN_ENT("GR", 0x5E, DEF    , 1, 1), /* Greece */
	COUNTRY_CHPLAN_ENT("GS", 0x3A, DEF    , 1, 1), /* South Georgia and the Sandwich Islands (UK) */
	COUNTRY_CHPLAN_ENT("GT", 0x76, DEF    , 1, 1), /* Guatemala */
	COUNTRY_CHPLAN_ENT("GU", 0x76, DEF    , 1, 1), /* Guam (USA) */
	COUNTRY_CHPLAN_ENT("GW", 0x3A, DEF    , 1, 1), /* Guinea-Bissau */
	COUNTRY_CHPLAN_ENT("GY", 0x44, DEF    , 1, 1), /* Guyana */
	COUNTRY_CHPLAN_ENT("HK", 0x5E, DEF    , 1, 1), /* Hong Kong */
	COUNTRY_CHPLAN_ENT("HM", 0x03, DEF    , 1, 1), /* Heard and McDonald Islands (Australia) */
	COUNTRY_CHPLAN_ENT("HN", 0x32, DEF    , 1, 1), /* Honduras */
	COUNTRY_CHPLAN_ENT("HR", 0x5E, DEF    , 1, 1), /* Croatia */
	COUNTRY_CHPLAN_ENT("HT", 0x76, DEF    , 0, 1), /* Haiti */
	COUNTRY_CHPLAN_ENT("HU", 0x5E, DEF    , 1, 1), /* Hungary */
	COUNTRY_CHPLAN_ENT("ID", 0x5D, DEF    , 1, 1), /* Indonesia */
	COUNTRY_CHPLAN_ENT("IE", 0x5E, DEF    , 1, 1), /* Ireland */
	COUNTRY_CHPLAN_ENT("IL", 0x5E, DEF    , 1, 1), /* Israel */
	COUNTRY_CHPLAN_ENT("IM", 0x3A, DEF    , 1, 1), /* Isle of Man (UK) */
	COUNTRY_CHPLAN_ENT("IN", 0x06, DEF    , 1, 1), /* India */
	COUNTRY_CHPLAN_ENT("IO", 0x3A, DEF    , 1, 1), /* British Indian Ocean Territory (UK) */
	COUNTRY_CHPLAN_ENT("IQ", 0x05, DEF    , 1, 1), /* Iraq */
	COUNTRY_CHPLAN_ENT("IR", 0x3A, DEF    , 0, 0), /* Iran */
	COUNTRY_CHPLAN_ENT("IS", 0x5E, DEF    , 1, 1), /* Iceland */
	COUNTRY_CHPLAN_ENT("IT", 0x5E, DEF    , 1, 1), /* Italy */
	COUNTRY_CHPLAN_ENT("JE", 0x3A, DEF    , 1, 1), /* Jersey (UK) */
	COUNTRY_CHPLAN_ENT("JM", 0x76, DEF    , 1, 1), /* Jamaica */
	COUNTRY_CHPLAN_ENT("JO", 0x05, DEF    , 1, 1), /* Jordan */
	COUNTRY_CHPLAN_ENT("JP", 0x7D, DEF    , 1, 1), /* Japan- Telec */
	COUNTRY_CHPLAN_ENT("KE", 0x5E, DEF    , 1, 1), /* Kenya */
	COUNTRY_CHPLAN_ENT("KG", 0x5E, DEF    , 1, 1), /* Kyrgyzstan */
	COUNTRY_CHPLAN_ENT("KH", 0x5E, DEF    , 1, 1), /* Cambodia */
	COUNTRY_CHPLAN_ENT("KI", 0x5E, DEF    , 1, 1), /* Kiribati */
	COUNTRY_CHPLAN_ENT("KM", 0x3A, DEF    , 1, 1), /* Comoros */
	COUNTRY_CHPLAN_ENT("KN", 0x76, DEF    , 1, 1), /* Saint Kitts and Nevis */
	COUNTRY_CHPLAN_ENT("KR", 0x4B, DEF    , 1, 1), /* South Korea */
	COUNTRY_CHPLAN_ENT("KW", 0x5E, DEF    , 1, 1), /* Kuwait */
	COUNTRY_CHPLAN_ENT("KY", 0x76, DEF    , 1, 1), /* Cayman Islands (UK) */
	COUNTRY_CHPLAN_ENT("KZ", 0x5E, DEF    , 1, 1), /* Kazakhstan */
	COUNTRY_CHPLAN_ENT("LA", 0x5E, DEF    , 1, 1), /* Laos */
	COUNTRY_CHPLAN_ENT("LB", 0x5E, DEF    , 1, 1), /* Lebanon */
	COUNTRY_CHPLAN_ENT("LC", 0x76, DEF    , 1, 1), /* Saint Lucia */
	COUNTRY_CHPLAN_ENT("LI", 0x5E, DEF    , 1, 1), /* Liechtenstein */
	COUNTRY_CHPLAN_ENT("LK", 0x5E, DEF    , 1, 1), /* Sri Lanka */
	COUNTRY_CHPLAN_ENT("LR", 0x5E, DEF    , 1, 1), /* Liberia */
	COUNTRY_CHPLAN_ENT("LS", 0x5E, DEF    , 1, 1), /* Lesotho */
	COUNTRY_CHPLAN_ENT("LT", 0x5E, DEF    , 1, 1), /* Lithuania */
	COUNTRY_CHPLAN_ENT("LU", 0x5E, DEF    , 1, 1), /* Luxembourg */
	COUNTRY_CHPLAN_ENT("LV", 0x5E, DEF    , 1, 1), /* Latvia */
	COUNTRY_CHPLAN_ENT("LY", 0x5E, DEF    , 1, 1), /* Libya */
	COUNTRY_CHPLAN_ENT("MA", 0x3C, DEF    , 1, 1), /* Morocco */
	COUNTRY_CHPLAN_ENT("MC", 0x5E, DEF    , 1, 1), /* Monaco */
	COUNTRY_CHPLAN_ENT("MD", 0x5E, DEF    , 1, 1), /* Moldova */
	COUNTRY_CHPLAN_ENT("ME", 0x3A, DEF    , 1, 1), /* Montenegro */
	COUNTRY_CHPLAN_ENT("MF", 0x76, DEF    , 1, 1), /* Saint Martin */
	COUNTRY_CHPLAN_ENT("MG", 0x5E, DEF    , 1, 1), /* Madagascar */
	COUNTRY_CHPLAN_ENT("MH", 0x76, DEF    , 1, 1), /* Marshall Islands (USA) */
	COUNTRY_CHPLAN_ENT("MK", 0x3A, DEF    , 1, 1), /* Republic of Macedonia (FYROM) */
	COUNTRY_CHPLAN_ENT("ML", 0x5E, DEF    , 1, 1), /* Mali */
	COUNTRY_CHPLAN_ENT("MM", 0x3A, DEF    , 1, 1), /* Burma (Myanmar) */
	COUNTRY_CHPLAN_ENT("MN", 0x5E, DEF    , 1, 1), /* Mongolia */
	COUNTRY_CHPLAN_ENT("MO", 0x5E, DEF    , 1, 1), /* Macau */
	COUNTRY_CHPLAN_ENT("MP", 0x76, DEF    , 1, 1), /* Northern Mariana Islands (USA) */
	COUNTRY_CHPLAN_ENT("MQ", 0x3A, DEF    , 1, 1), /* Martinique (France) */
	COUNTRY_CHPLAN_ENT("MR", 0x5E, DEF    , 1, 1), /* Mauritania */
	COUNTRY_CHPLAN_ENT("MS", 0x3A, DEF    , 1, 1), /* Montserrat (UK) */
	COUNTRY_CHPLAN_ENT("MT", 0x5E, DEF    , 1, 1), /* Malta */
	COUNTRY_CHPLAN_ENT("MU", 0x5E, DEF    , 1, 1), /* Mauritius */
	COUNTRY_CHPLAN_ENT("MV", 0x3C, DEF    , 1, 1), /* Maldives */
	COUNTRY_CHPLAN_ENT("MW", 0x5E, DEF    , 1, 1), /* Malawi */
	COUNTRY_CHPLAN_ENT("MX", 0x4D, DEF    , 1, 1), /* Mexico */
	COUNTRY_CHPLAN_ENT("MY", 0x07, DEF    , 1, 1), /* Malaysia */
	COUNTRY_CHPLAN_ENT("MZ", 0x5E, DEF    , 1, 1), /* Mozambique */
	COUNTRY_CHPLAN_ENT("NA", 0x5E, DEF    , 1, 1), /* Namibia */
	COUNTRY_CHPLAN_ENT("NC", 0x3A, DEF    , 1, 1), /* New Caledonia */
	COUNTRY_CHPLAN_ENT("NE", 0x5E, DEF    , 1, 1), /* Niger */
	COUNTRY_CHPLAN_ENT("NF", 0x03, DEF    , 1, 1), /* Norfolk Island (Australia) */
	COUNTRY_CHPLAN_ENT("NG", 0x75, DEF    , 1, 1), /* Nigeria */
	COUNTRY_CHPLAN_ENT("NI", 0x76, DEF    , 1, 1), /* Nicaragua */
	COUNTRY_CHPLAN_ENT("NL", 0x5E, DEF    , 1, 1), /* Netherlands */
	COUNTRY_CHPLAN_ENT("NO", 0x5E, DEF    , 1, 1), /* Norway */
	COUNTRY_CHPLAN_ENT("NP", 0x06, DEF    , 1, 1), /* Nepal */
	COUNTRY_CHPLAN_ENT("NR", 0x3A, DEF    , 1, 1), /* Nauru */
	COUNTRY_CHPLAN_ENT("NU", 0x03, DEF    , 1, 1), /* Niue */
	COUNTRY_CHPLAN_ENT("NZ", 0x03, DEF    , 1, 1), /* New Zealand */
	COUNTRY_CHPLAN_ENT("OM", 0x5E, DEF    , 1, 1), /* Oman */
	COUNTRY_CHPLAN_ENT("PA", 0x76, DEF    , 1, 1), /* Panama */
	COUNTRY_CHPLAN_ENT("PE", 0x76, DEF    , 1, 1), /* Peru */
	COUNTRY_CHPLAN_ENT("PF", 0x3A, DEF    , 1, 1), /* French Polynesia (France) */
	COUNTRY_CHPLAN_ENT("PG", 0x5E, DEF    , 1, 1), /* Papua New Guinea */
	COUNTRY_CHPLAN_ENT("PH", 0x5E, DEF    , 1, 1), /* Philippines */
	COUNTRY_CHPLAN_ENT("PK", 0x51, DEF    , 1, 1), /* Pakistan */
	COUNTRY_CHPLAN_ENT("PL", 0x5E, DEF    , 1, 1), /* Poland */
	COUNTRY_CHPLAN_ENT("PM", 0x3A, DEF    , 1, 1), /* Saint Pierre and Miquelon (France) */
	COUNTRY_CHPLAN_ENT("PR", 0x76, DEF    , 1, 1), /* Puerto Rico */
	COUNTRY_CHPLAN_ENT("PT", 0x5E, DEF    , 1, 1), /* Portugal */
	COUNTRY_CHPLAN_ENT("PW", 0x76, DEF    , 1, 1), /* Palau */
	COUNTRY_CHPLAN_ENT("PY", 0x76, DEF    , 1, 1), /* Paraguay */
	COUNTRY_CHPLAN_ENT("QA", 0x5E, QATAR  , 1, 1), /* Qatar */
	COUNTRY_CHPLAN_ENT("RE", 0x3A, DEF    , 1, 1), /* Reunion (France) */
	COUNTRY_CHPLAN_ENT("RO", 0x5E, DEF    , 1, 1), /* Romania */
	COUNTRY_CHPLAN_ENT("RS", 0x5E, DEF    , 1, 1), /* Serbia */
	COUNTRY_CHPLAN_ENT("RU", 0x09, DEF    , 1, 1), /* Russia(fac/gost) */
//	COUNTRY_CHPLAN_ENT("RU", 0x59, DEF    , 1, 1), /* Kaliningrad(Russia) */
	COUNTRY_CHPLAN_ENT("RW", 0x5E, DEF    , 1, 1), /* Rwanda */
	COUNTRY_CHPLAN_ENT("SA", 0x5E, DEF    , 1, 1), /* Saudi Arabia */
	COUNTRY_CHPLAN_ENT("SB", 0x3A, DEF    , 1, 1), /* Solomon Islands */
	COUNTRY_CHPLAN_ENT("SC", 0x76, DEF    , 1, 1), /* Seychelles */
	COUNTRY_CHPLAN_ENT("SE", 0x5E, DEF    , 1, 1), /* Sweden */
	COUNTRY_CHPLAN_ENT("SG", 0x5E, DEF    , 1, 1), /* Singapore */
	COUNTRY_CHPLAN_ENT("SH", 0x3A, DEF    , 1, 1), /* Saint Helena (UK) */
	COUNTRY_CHPLAN_ENT("SI", 0x5E, DEF    , 1, 1), /* Slovenia */
	COUNTRY_CHPLAN_ENT("SJ", 0x3A, DEF    , 1, 1), /* Svalbard (Norway) */
	COUNTRY_CHPLAN_ENT("SK", 0x5E, DEF    , 1, 1), /* Slovakia */
	COUNTRY_CHPLAN_ENT("SL", 0x5E, DEF    , 1, 1), /* Sierra Leone */
	COUNTRY_CHPLAN_ENT("SM", 0x3A, DEF    , 1, 1), /* San Marino */
	COUNTRY_CHPLAN_ENT("SN", 0x3A, DEF    , 1, 1), /* Senegal */
	COUNTRY_CHPLAN_ENT("SO", 0x3A, DEF    , 1, 1), /* Somalia */
	COUNTRY_CHPLAN_ENT("SR", 0x74, DEF    , 1, 1), /* Suriname */
	COUNTRY_CHPLAN_ENT("ST", 0x76, DEF    , 1, 1), /* Sao Tome and Principe */
	COUNTRY_CHPLAN_ENT("SV", 0x76, DEF    , 1, 1), /* El Salvador */
	COUNTRY_CHPLAN_ENT("SX", 0x76, DEF    , 1, 1), /* Sint Marteen */
	COUNTRY_CHPLAN_ENT("SZ", 0x5E, DEF    , 1, 1), /* Swaziland */
	COUNTRY_CHPLAN_ENT("TC", 0x3A, DEF    , 1, 1), /* Turks and Caicos Islands (UK) */
	COUNTRY_CHPLAN_ENT("TD", 0x3A, DEF    , 1, 1), /* Chad */
	COUNTRY_CHPLAN_ENT("TF", 0x3A, DEF    , 1, 1), /* French Southern and Antarctic Lands (FR Southern Territories) */
	COUNTRY_CHPLAN_ENT("TG", 0x5E, DEF    , 1, 1), /* Togo */
	COUNTRY_CHPLAN_ENT("TH", 0x5E, DEF    , 1, 1), /* Thailand */
	COUNTRY_CHPLAN_ENT("TJ", 0x5E, DEF    , 1, 1), /* Tajikistan */
	COUNTRY_CHPLAN_ENT("TK", 0x03, DEF    , 1, 1), /* Tokelau */
	COUNTRY_CHPLAN_ENT("TM", 0x3A, DEF    , 1, 1), /* Turkmenistan */
	COUNTRY_CHPLAN_ENT("TN", 0x04, DEF    , 1, 1), /* Tunisia */
	COUNTRY_CHPLAN_ENT("TO", 0x3A, DEF    , 1, 1), /* Tonga */
//	COUNTRY_CHPLAN_ENT("TR", 0x3A, DEF    , 1, 1), /* Northern Cyprus */
	COUNTRY_CHPLAN_ENT("TR", 0x5E, DEF    , 1, 1), /* Turkey */
	COUNTRY_CHPLAN_ENT("TT", 0x76, DEF    , 1, 1), /* Trinidad & Tobago */
	COUNTRY_CHPLAN_ENT("TV", 0x21, DEF    , 0, 0), /* Tuvalu */
	COUNTRY_CHPLAN_ENT("TW", 0x76, DEF    , 1, 1), /* Taiwan */
	COUNTRY_CHPLAN_ENT("TZ", 0x5E, DEF    , 1, 1), /* Tanzania */
	COUNTRY_CHPLAN_ENT("UA", 0x5E, UKRAINE, 1, 1), /* Ukraine */
	COUNTRY_CHPLAN_ENT("UG", 0x3A, DEF    , 1, 1), /* Uganda */
	COUNTRY_CHPLAN_ENT("US", 0x76, DEF    , 1, 1), /* United States of America (USA) */
	COUNTRY_CHPLAN_ENT("UY", 0x30, DEF    , 1, 1), /* Uruguay */
	COUNTRY_CHPLAN_ENT("UZ", 0x3C, DEF    , 1, 1), /* Uzbekistan */
	COUNTRY_CHPLAN_ENT("VA", 0x3A, DEF    , 1, 1), /* Holy See (Vatican City) */
	COUNTRY_CHPLAN_ENT("VC", 0x76, DEF    , 1, 1), /* Saint Vincent and the Grenadines */
	COUNTRY_CHPLAN_ENT("VE", 0x30, DEF    , 1, 1), /* Venezuela */
	COUNTRY_CHPLAN_ENT("VG", 0x76, DEF    , 1, 1), /* British Virgin Islands (UK) */
	COUNTRY_CHPLAN_ENT("VI", 0x76, DEF    , 1, 1), /* United States Virgin Islands (USA) */
	COUNTRY_CHPLAN_ENT("VN", 0x5E, DEF    , 1, 1), /* Vietnam */
	COUNTRY_CHPLAN_ENT("VU", 0x26, DEF    , 1, 1), /* Vanuatu */
	COUNTRY_CHPLAN_ENT("WF", 0x3A, DEF    , 1, 1), /* Wallis and Futuna (France) */
	COUNTRY_CHPLAN_ENT("WS", 0x76, DEF    , 1, 1), /* Samoa */
	COUNTRY_CHPLAN_ENT("XK", 0x3A, DEF    , 1, 1), /* Kosovo */
	COUNTRY_CHPLAN_ENT("YE", 0x3A, DEF    , 1, 1), /* Yemen */
	COUNTRY_CHPLAN_ENT("YT", 0x5E, DEF    , 1, 1), /* Mayotte (France) */
	COUNTRY_CHPLAN_ENT("ZA", 0x5E, DEF    , 1, 1), /* South Africa */
	COUNTRY_CHPLAN_ENT("ZM", 0x5E, DEF    , 1, 1), /* Zambia */
	COUNTRY_CHPLAN_ENT("ZW", 0x5E, DEF    , 1, 1), /* Zimbabwe */
};
#endif /* CONFIG_CUSTOMIZED_COUNTRY_CHPLAN_MAP or RTW_DEF_MODULE_REGULATORY_CERT or newest */

/*
* rtw_get_chplan_from_country -
* @country_code: string of country code
*
* Return pointer of struct country_chplan entry or NULL when unsupported country_code is given
*/
const struct country_chplan *rtw_get_chplan_from_country(const char *country_code)
{
	const struct country_chplan *ent = NULL;
	const struct country_chplan *map = NULL;
	u16 map_sz = 0;
	char code[2];
	int i;

	code[0] = alpha_to_upper(country_code[0]);
	code[1] = alpha_to_upper(country_code[1]);

#ifdef CONFIG_CUSTOMIZED_COUNTRY_CHPLAN_MAP
	map = CUSTOMIZED_country_chplan_map;
	map_sz = sizeof(CUSTOMIZED_country_chplan_map) / sizeof(struct country_chplan);
#elif RTW_DEF_MODULE_REGULATORY_CERT
	map_sz = rtw_def_module_country_chplan_map(&map);
#else
	map = country_chplan_map;
	map_sz = sizeof(country_chplan_map) / sizeof(struct country_chplan);
#endif

	for (i = 0; i < map_sz; i++) {
		if (strncmp(code, map[i].alpha2, 2) == 0) {
			ent = &map[i];
			break;
		}
	}

	return ent;
}

void dump_country_chplan(void *sel, const struct country_chplan *ent, bool regd_info)
{
	char buf[16];

	if (ent->chplan == RTW_CHPLAN_UNSPECIFIED)
		sprintf(buf, "NA");
	else
		sprintf(buf, "0x%02X", ent->chplan);

	RTW_PRINT_SEL(sel, "\"%c%c\", %s"
		, ent->alpha2[0], ent->alpha2[1], buf);

	if (regd_info) {
		u8 txpwr_lmt;

		if (ent->txpwr_lmt_override != TXPWR_LMT_DEF)
			_RTW_PRINT_SEL(sel, " %-7s", txpwr_lmt_str(ent->txpwr_lmt_override));
		else if (ent->chplan != RTW_CHPLAN_UNSPECIFIED) {
			txpwr_lmt = rtw_regd_to_txpwr_lmt(rtw_chplan_get_default_regd(ent->chplan));
			_RTW_PRINT_SEL(sel, " %-7s", txpwr_lmt_str(txpwr_lmt));
		}
	}

	_RTW_PRINT_SEL(sel, " %s", COUNTRY_CHPLAN_EN_11AX(ent) ? "ax" : "  ");
	_RTW_PRINT_SEL(sel, " %s", COUNTRY_CHPLAN_EN_11AC(ent) ? "ac" : "  ");

	_RTW_PRINT_SEL(sel, "\n");
}

void dump_country_chplan_map(void *sel, bool regd_info)
{
	const struct country_chplan *ent;
	u8 code[2];

#if RTW_DEF_MODULE_REGULATORY_CERT
	RTW_PRINT_SEL(sel, "RTW_DEF_MODULE_REGULATORY_CERT:0x%x\n", RTW_DEF_MODULE_REGULATORY_CERT);
#endif
#ifdef CONFIG_CUSTOMIZED_COUNTRY_CHPLAN_MAP
	RTW_PRINT_SEL(sel, "CONFIG_CUSTOMIZED_COUNTRY_CHPLAN_MAP\n");
#endif

	for (code[0] = 'A'; code[0] <= 'Z'; code[0]++) {
		for (code[1] = 'A'; code[1] <= 'Z'; code[1]++) {
			ent = rtw_get_chplan_from_country(code);
			if (!ent)
				continue;

			dump_country_chplan(sel, ent, regd_info);
		}
	}
}

void dump_country_list(void *sel)
{
	const struct country_chplan *ent;
	u8 code[2];
	u8 first = 1;

	for (code[0] = 'A'; code[0] <= 'Z'; code[0]++) {
		for (code[1] = 'A'; code[1] <= 'Z'; code[1]++) {
			ent = rtw_get_chplan_from_country(code);
			if (!ent)
				continue;

			if (first) {
				RTW_PRINT_SEL(sel, "%c%c ", code[0], code[1]);
				first = 0;
			} else
				_RTW_PRINT_SEL(sel, "%c%c ", code[0], code[1]);
		}
	}
	if (first == 0)
		_RTW_PRINT_SEL(sel, "\n");
}

void dump_chplan_id_list(void *sel)
{
	u8 first = 1;
	int i;

	for (i = 0; i < RTW_ChannelPlanMap_size; i++) {
		if (!rtw_is_channel_plan_valid(i))
			continue;

		if (first) {
			RTW_PRINT_SEL(sel, "0x%02X ", i);
			first = 0;
		} else
			_RTW_PRINT_SEL(sel, "0x%02X ", i);
	}
	if (first == 0)
		_RTW_PRINT_SEL(sel, "\n");
}

void dump_chplan_country_list(void *sel)
{
	int i;

	for (i = 0; i < RTW_ChannelPlanMap_size; i++) {
		const struct country_chplan *ent;
		u8 code[2];
		u8 first;

		if (!rtw_is_channel_plan_valid(i))
			continue;

		first = 1;
		for (code[0] = 'A'; code[0] <= 'Z'; code[0]++) {
			for (code[1] = 'A'; code[1] <= 'Z'; code[1]++) {
				ent = rtw_get_chplan_from_country(code);
				if (!ent || ent->chplan != i)
					continue;

				if (first) {
					RTW_PRINT_SEL(sel, "0x%02X %c%c ", i, code[0], code[1]);
					first = 0;
				} else
					_RTW_PRINT_SEL(sel, "%c%c ", code[0], code[1]);
			}
		}
		if (first == 0)
			_RTW_PRINT_SEL(sel, "\n");
	}
}

#ifdef CONFIG_RTW_DEBUG
void dump_chplan_test(void *sel)
{
	int i, j;

	/* check redundent */
	for (i = 0; i < RTW_CHD_2G_MAX; i++) {
		for (j = 0; j < i; j++) {
			if (CH_LIST_LEN(rtw_channel_def_2g[i]) == CH_LIST_LEN(rtw_channel_def_2g[j])
				&& _rtw_memcmp(&CH_LIST_CH(rtw_channel_def_2g[i], 0), &CH_LIST_CH(rtw_channel_def_2g[j], 0), CH_LIST_LEN(rtw_channel_def_2g[i]) + 1) == _TRUE)
				RTW_PRINT_SEL(sel, "2G chd:%u and %u is the same\n", i, j);
		}
	}

	/* check invalid channel */
	for (i = 0; i < RTW_CHD_2G_MAX; i++) {
		for (j = 0; j < CH_LIST_LEN(rtw_channel_def_2g[i]); j++) {
			if (rtw_ch2freq(CH_LIST_CH(rtw_channel_def_2g[i], j)) == 0)
				RTW_PRINT_SEL(sel, "2G invalid ch:%u at (%d,%d)\n", CH_LIST_CH(rtw_channel_def_2g[i], j), i, j);
		}
	}

#if CONFIG_IEEE80211_BAND_5GHZ
	/* check redundent */
	for (i = 0; i < RTW_CHD_5G_MAX; i++) {
		for (j = 0; j < i; j++) {
			if (CH_LIST_LEN(rtw_channel_def_5g[i]) == CH_LIST_LEN(rtw_channel_def_5g[j])
				&& _rtw_memcmp(&CH_LIST_CH(rtw_channel_def_5g[i], 0), &CH_LIST_CH(rtw_channel_def_5g[j], 0), CH_LIST_LEN(rtw_channel_def_5g[i]) + 1) == _TRUE)
				RTW_PRINT_SEL(sel, "5G chd:%u and %u is the same\n", i, j);
		}
	}

	/* check invalid channel */
	for (i = 0; i < RTW_CHD_5G_MAX; i++) {
		for (j = 0; j < CH_LIST_LEN(rtw_channel_def_5g[i]); j++) {
			if (rtw_ch2freq(CH_LIST_CH(rtw_channel_def_5g[i], j)) == 0)
				RTW_PRINT_SEL(sel, "5G invalid ch:%u at (%d,%d)\n", CH_LIST_CH(rtw_channel_def_5g[i], j), i, j);
		}
	}
#endif

	/* check redundent */
	for (i = 0; i < RTW_ChannelPlanMap_size; i++) {
		if (!rtw_is_channel_plan_valid(i))
			continue;
		for (j = 0; j < i; j++) {
			if (!rtw_is_channel_plan_valid(j))
				continue;
			if (_rtw_memcmp(&RTW_ChannelPlanMap[i], &RTW_ChannelPlanMap[j], sizeof(RTW_ChannelPlanMap[i])) == _TRUE)
				RTW_PRINT_SEL(sel, "channel plan 0x%02x and 0x%02x is the same\n", i, j);
		}
	}
}
#endif /* CONFIG_RTW_DEBUG */

void dump_chplan_ver(void *sel)
{
	RTW_PRINT_SEL(sel, "%s%s-%s\n", RTW_DOMAIN_MAP_VER, RTW_DOMAIN_MAP_M_VER, RTW_COUNTRY_MAP_VER);
}
