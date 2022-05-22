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
#ifndef __RTW_CHPLAN_H__
#define __RTW_CHPLAN_H__

#define RTW_CHPLAN_UNSPECIFIED 0xFF

u8 rtw_chplan_get_default_regd(u8 id);
bool rtw_chplan_is_empty(u8 id);
bool rtw_is_channel_plan_valid(u8 id);
bool rtw_regsty_is_excl_chs(struct registry_priv *regsty, u8 ch);

enum regd_src_t {
	REGD_SRC_RTK_PRIV = 0, /* Regulatory settings from Realtek framework (Realtek defined or customized) */
	REGD_SRC_OS = 1, /* Regulatory settings from OS */
	REGD_SRC_NUM,
};

#define regd_src_is_valid(src) ((src) < REGD_SRC_NUM)

extern const char *_regd_src_str[];
#define regd_src_str(src) ((src) >= REGD_SRC_NUM ? _regd_src_str[REGD_SRC_NUM] : _regd_src_str[src])

struct _RT_CHANNEL_INFO;
u8 init_channel_set(_adapter *adapter);
bool rtw_chset_is_dfs_range(struct _RT_CHANNEL_INFO *chset, u32 hi, u32 lo);
bool rtw_chset_is_dfs_ch(struct _RT_CHANNEL_INFO *chset, u8 ch);
bool rtw_chset_is_dfs_chbw(struct _RT_CHANNEL_INFO *chset, u8 ch, u8 bw, u8 offset);
u8 rtw_process_beacon_hint(_adapter *adapter, WLAN_BSSID_EX *bss);

#define IS_ALPHA2_NO_SPECIFIED(_alpha2) ((*((u16 *)(_alpha2))) == 0xFFFF)
#define IS_ALPHA2_WORLDWIDE(_alpha2) (strncmp(_alpha2, "00", 2) == 0)

#define RTW_MODULE_RTL8821AE_HMC_M2		BIT0	/* RTL8821AE(HMC + M.2) */
#define RTW_MODULE_RTL8821AU			BIT1	/* RTL8821AU */
#define RTW_MODULE_RTL8812AENF_NGFF		BIT2	/* RTL8812AENF(8812AE+8761)_NGFF */
#define RTW_MODULE_RTL8812AEBT_HMC		BIT3	/* RTL8812AEBT(8812AE+8761)_HMC */
#define RTW_MODULE_RTL8188EE_HMC_M2		BIT4	/* RTL8188EE(HMC + M.2) */
#define RTW_MODULE_RTL8723BE_HMC_M2		BIT5	/* RTL8723BE(HMC + M.2) */
#define RTW_MODULE_RTL8723BS_NGFF1216	BIT6	/* RTL8723BS(NGFF1216) */
#define RTW_MODULE_RTL8192EEBT_HMC_M2	BIT7	/* RTL8192EEBT(8192EE+8761AU)_(HMC + M.2) */
#define RTW_MODULE_RTL8723DE_NGFF1630	BIT8	/* RTL8723DE(NGFF1630) */
#define RTW_MODULE_RTL8822BE			BIT9	/* RTL8822BE */
#define RTW_MODULE_RTL8821CE			BIT10	/* RTL8821CE */
#define RTW_MODULE_RTL8822CE			BIT11	/* RTL8822CE */

enum rtw_regd {
	RTW_REGD_NA = 0,
	RTW_REGD_FCC = 1,
	RTW_REGD_MKK = 2,
	RTW_REGD_ETSI = 3,
	RTW_REGD_IC = 4,
	RTW_REGD_KCC = 5,
	RTW_REGD_NCC = 6,
	RTW_REGD_ACMA = 7,
	RTW_REGD_CHILE = 8,
	RTW_REGD_MEX = 9,
	RTW_REGD_WW,
	RTW_REGD_NUM,
};

extern const char *const _regd_str[];
#define regd_str(regd) (((regd) >= RTW_REGD_NUM) ? _regd_str[RTW_REGD_NA] : _regd_str[(regd)])

enum rtw_edcca_mode {
	RTW_EDCCA_NORMAL	= 0, /* normal */
	RTW_EDCCA_ADAPT		= 1, /* adaptivity */
	RTW_EDCCA_CS		= 2, /* carrier sense */
	RTW_EDCCA_MODE_NUM,
	RTW_EDCCA_MODE_AUTO	= 0xFF, /* folllow channel plan */
};

extern const char *const _rtw_edcca_mode_str[];
#define rtw_edcca_mode_str(mode) (((mode) >= RTW_EDCCA_MODE_NUM) ? _rtw_edcca_mode_str[RTW_EDCCA_NORMAL] : _rtw_edcca_mode_str[(mode)])

enum rtw_dfs_regd {
	RTW_DFS_REGD_NONE	= 0,
	RTW_DFS_REGD_FCC	= 1,
	RTW_DFS_REGD_MKK	= 2,
	RTW_DFS_REGD_ETSI	= 3,
	RTW_DFS_REGD_NUM,
	RTW_DFS_REGD_AUTO	= 0xFF, /* follow channel plan */
};

extern const char *_rtw_dfs_regd_str[];
#define rtw_dfs_regd_str(region) (((region) >= RTW_DFS_REGD_NUM) ? _rtw_dfs_regd_str[RTW_DFS_REGD_NONE] : _rtw_dfs_regd_str[(region)])

typedef enum _REGULATION_TXPWR_LMT {
	TXPWR_LMT_NONE = 0, /* no limit */
	TXPWR_LMT_FCC = 1,
	TXPWR_LMT_MKK = 2,
	TXPWR_LMT_ETSI = 3,
	TXPWR_LMT_IC = 4,
	TXPWR_LMT_KCC = 5,
	TXPWR_LMT_NCC = 6,
	TXPWR_LMT_ACMA = 7,
	TXPWR_LMT_CHILE = 8,
	TXPWR_LMT_UKRAINE = 9,
	TXPWR_LMT_MEXICO = 10,
	TXPWR_LMT_CN = 11,
	TXPWR_LMT_QATAR = 12,
	TXPWR_LMT_WW, /* smallest of all available limit, keep last */

	TXPWR_LMT_NUM,
	TXPWR_LMT_DEF = TXPWR_LMT_NUM, /* default (ref to domain code), used at country chplan map's override field */
} REGULATION_TXPWR_LMT;

extern const char *const _txpwr_lmt_str[];
#define txpwr_lmt_str(regd) (((regd) > TXPWR_LMT_WW) ? _txpwr_lmt_str[TXPWR_LMT_WW] : _txpwr_lmt_str[(regd)])

extern const REGULATION_TXPWR_LMT _txpwr_lmt_alternate[];
#define txpwr_lmt_alternate(ori) (((ori) > TXPWR_LMT_WW) ? _txpwr_lmt_alternate[TXPWR_LMT_WW] : _txpwr_lmt_alternate[(ori)])

#define TXPWR_LMT_ALTERNATE_DEFINED(txpwr_lmt) (txpwr_lmt_alternate(txpwr_lmt) != txpwr_lmt)

extern const enum rtw_edcca_mode _rtw_regd_to_edcca_mode[RTW_REGD_NUM];
#define rtw_regd_to_edcca_mode(regd) (((regd) >= RTW_REGD_NUM) ? RTW_EDCCA_NORMAL : _rtw_regd_to_edcca_mode[(regd)])

extern const REGULATION_TXPWR_LMT _rtw_regd_to_txpwr_lmt[];
#define rtw_regd_to_txpwr_lmt(regd) (((regd) >= RTW_REGD_NUM) ? TXPWR_LMT_WW : _rtw_regd_to_txpwr_lmt[(regd)])

#define EDCCA_MODES_STR_LEN (((6 + 3 + 1) * BAND_MAX) + 1)
char *rtw_get_edcca_modes_str(char *buf, u8 modes[]);
void rtw_edcca_mode_update(struct dvobj_priv *dvobj);
u8 rtw_get_edcca_mode(struct dvobj_priv *dvobj, BAND_TYPE band);

#define CCHPLAN_PROTO_EN_AC		BIT0
#define CCHPLAN_PROTO_EN_AX		BIT1
#define CCHPLAN_PROTO_EN_ALL	0xFF

struct country_chplan {
	char alpha2[2]; /* "00" means worldwide */
	u8 chplan;
	u8 txpwr_lmt_override;
#if defined(CONFIG_80211AX_HE) || defined(CONFIG_80211AC_VHT)
	u8 proto_en;
#endif
};

#ifdef CONFIG_80211AC_VHT
#define COUNTRY_CHPLAN_EN_11AC(_ent) ((_ent)->proto_en & CCHPLAN_PROTO_EN_AC)
#else
#define COUNTRY_CHPLAN_EN_11AC(_ent) 0
#endif

#ifdef CONFIG_80211AX_HE
#define COUNTRY_CHPLAN_EN_11AX(_ent) ((_ent)->proto_en & CCHPLAN_PROTO_EN_AX)
#else
#define COUNTRY_CHPLAN_EN_11AX(_ent) 0
#endif

const struct country_chplan *rtw_get_chplan_from_country(const char *country_code);

void dump_country_chplan(void *sel, const struct country_chplan *ent, bool regd_info);
void dump_country_chplan_map(void *sel, bool regd_info);
void dump_country_list(void *sel);
void dump_chplan_id_list(void *sel);
void dump_chplan_country_list(void *sel);
#ifdef CONFIG_RTW_DEBUG
void dump_chplan_test(void *sel);
#endif
void dump_chplan_ver(void *sel);

#endif /* __RTW_CHPLAN_H__ */
