// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _HAL_COM_PHYCFG_C_

#include <drv_types.h>
#include <hal_data.h>

#define PG_TXPWR_MSB_DIFF_S4BIT(_pg_v) (((_pg_v) & 0xf0) >> 4)
#define PG_TXPWR_LSB_DIFF_S4BIT(_pg_v) ((_pg_v) & 0x0f)
#define PG_TXPWR_MSB_DIFF_TO_S8BIT(_pg_v) ((PG_TXPWR_MSB_DIFF_S4BIT(_pg_v) & BIT3) ? (PG_TXPWR_MSB_DIFF_S4BIT(_pg_v) | 0xF0) : PG_TXPWR_MSB_DIFF_S4BIT(_pg_v))
#define PG_TXPWR_LSB_DIFF_TO_S8BIT(_pg_v) ((PG_TXPWR_LSB_DIFF_S4BIT(_pg_v) & BIT3) ? (PG_TXPWR_LSB_DIFF_S4BIT(_pg_v) | 0xF0) : PG_TXPWR_LSB_DIFF_S4BIT(_pg_v))
#define IS_PG_TXPWR_BASE_INVALID(_base) ((_base) > 63)
#define IS_PG_TXPWR_DIFF_INVALID(_diff) ((_diff) > 7 || (_diff) < -8)
#define PG_TXPWR_INVALID_BASE 255
#define PG_TXPWR_INVALID_DIFF 8

#if !IS_PG_TXPWR_BASE_INVALID(PG_TXPWR_INVALID_BASE)
#error "PG_TXPWR_BASE definition has problem"
#endif

#if !IS_PG_TXPWR_DIFF_INVALID(PG_TXPWR_INVALID_DIFF)
#error "PG_TXPWR_DIFF definition has problem"
#endif

#define PG_TXPWR_SRC_PG_DATA	0
#define PG_TXPWR_SRC_IC_DEF		1
#define PG_TXPWR_SRC_DEF		2
#define PG_TXPWR_SRC_NUM		3

static const char *const _pg_txpwr_src_str[] = {
	"PG_DATA",
	"IC_DEF",
	"DEF",
	"UNKNOWN"
};

#define pg_txpwr_src_str(src) (((src) >= PG_TXPWR_SRC_NUM) ? _pg_txpwr_src_str[PG_TXPWR_SRC_NUM] : _pg_txpwr_src_str[(src)])

#ifndef DBG_PG_TXPWR_READ
#define DBG_PG_TXPWR_READ 0
#endif

#if DBG_PG_TXPWR_READ
static void dump_pg_txpwr_info_2g(void *sel, struct TxPowerInfo24G *txpwr_info, u8 rfpath_num, u8 max_tx_cnt)
{
	int path, group, tx_idx;

	RTW_PRINT_SEL(sel, "2.4G\n");
	RTW_PRINT_SEL(sel, "CCK-1T base:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (group = 0; group < MAX_CHNL_GROUP_24G; group++)
		_RTW_PRINT_SEL(sel, "G%02d ", group);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (group = 0; group < MAX_CHNL_GROUP_24G; group++)
			_RTW_PRINT_SEL(sel, "%3u ", txpwr_info->IndexCCK_Base[path][group]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "CCK diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dT ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->CCK_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW40-1S base:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (group = 0; group < MAX_CHNL_GROUP_24G - 1; group++)
		_RTW_PRINT_SEL(sel, "G%02d ", group);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (group = 0; group < MAX_CHNL_GROUP_24G - 1; group++)
			_RTW_PRINT_SEL(sel, "%3u ", txpwr_info->IndexBW40_Base[path][group]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "OFDM diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dT ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->OFDM_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW20 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dS ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->BW20_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW40 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dS ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->BW40_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");
}

static void dump_pg_txpwr_info_5g(void *sel, struct TxPowerInfo5G *txpwr_info, u8 rfpath_num, u8 max_tx_cnt)
{
	int path, group, tx_idx;

	RTW_PRINT_SEL(sel, "5G\n");
	RTW_PRINT_SEL(sel, "BW40-1S base:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (group = 0; group < MAX_CHNL_GROUP_5G; group++)
		_RTW_PRINT_SEL(sel, "G%02d ", group);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (group = 0; group < MAX_CHNL_GROUP_5G; group++)
			_RTW_PRINT_SEL(sel, "%3u ", txpwr_info->IndexBW40_Base[path][group]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "OFDM diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dT ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->OFDM_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW20 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dS ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->BW20_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW40 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dS ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->BW40_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW80 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dS ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->BW80_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW160 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++)
		_RTW_PRINT_SEL(sel, "%dS ", path + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", txpwr_info->BW160_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");
}
#endif /* DBG_PG_TXPWR_READ */

static const struct map_t pg_txpwr_def_info =
	MAP_ENT(0xB8, 1, 0xFF
		, MAPSEG_ARRAY_ENT(0x10, 168,
			0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x24, 0xEE, 0xEE, 0xEE, 0xEE,
			0xEE, 0xEE, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
			0x04, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
			0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x24, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0x2A, 0x2A, 0x2A, 0x2A,
			0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x04, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
			0xEE, 0xEE, 0xEE, 0xEE, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x24,
			0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
			0x2A, 0x2A, 0x2A, 0x2A, 0x04, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0x2D, 0x2D,
			0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x24, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
			0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x04, 0xEE,
			0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE)
	);

static const struct map_t rtl8723d_pg_txpwr_def_info =
	MAP_ENT(0xB8, 2, 0xFF
		, MAPSEG_ARRAY_ENT(0x10, 12,
			0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x02)
		, MAPSEG_ARRAY_ENT(0x3A, 12,
			0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x21, 0x21, 0x21, 0x21, 0x21, 0x02)
	);

const struct map_t *hal_pg_txpwr_def_info(struct adapter *adapter)
{
	u8 interface_type = 0;
	const struct map_t *map = NULL;

	interface_type = rtw_get_intf_type(adapter);

	switch (rtw_get_chip_type(adapter)) {
	case RTL8723D:
		map = &rtl8723d_pg_txpwr_def_info;
		break;
	}

	if (!map) {
		RTW_ERR("%s: unknown chip_type:%u\n"
			, __func__, rtw_get_chip_type(adapter));
		rtw_warn_on(1);
	}

	return map;
}

static u8 hal_chk_pg_txpwr_info_2g(struct adapter *adapter, struct TxPowerInfo24G *pwr_info)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 path, group, tx_idx;

	if (!pwr_info || !hal_chk_band_cap(adapter, BAND_CAP_2G))
		return _SUCCESS;

	for (path = 0; path < MAX_RF_PATH; path++) {
		if (!HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path))
			continue;
		for (group = 0; group < MAX_CHNL_GROUP_24G; group++) {
			if (IS_PG_TXPWR_BASE_INVALID(pwr_info->IndexCCK_Base[path][group])
				|| IS_PG_TXPWR_BASE_INVALID(pwr_info->IndexBW40_Base[path][group]))
				return _FAIL;
		}
		for (tx_idx = 0; tx_idx < MAX_TX_COUNT; tx_idx++) {
			if (!HAL_SPEC_CHK_TX_CNT(hal_spec, tx_idx))
				continue;
			if (IS_PG_TXPWR_DIFF_INVALID(pwr_info->CCK_Diff[path][tx_idx])
				|| IS_PG_TXPWR_DIFF_INVALID(pwr_info->OFDM_Diff[path][tx_idx])
				|| IS_PG_TXPWR_DIFF_INVALID(pwr_info->BW20_Diff[path][tx_idx])
				|| IS_PG_TXPWR_DIFF_INVALID(pwr_info->BW40_Diff[path][tx_idx]))
				return _FAIL;
		}
	}

	return _SUCCESS;
}

static u8 hal_chk_pg_txpwr_info_5g(struct adapter *adapter, struct TxPowerInfo5G *pwr_info)
{
	return _SUCCESS;
}

static inline void hal_init_pg_txpwr_info_2g(struct adapter *adapter, struct TxPowerInfo24G *pwr_info)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 path, group, tx_idx;

	if (!pwr_info)
		return;

	memset(pwr_info, 0, sizeof(struct TxPowerInfo24G));

	/* init with invalid value */
	for (path = 0; path < MAX_RF_PATH; path++) {
		for (group = 0; group < MAX_CHNL_GROUP_24G; group++) {
			pwr_info->IndexCCK_Base[path][group] = PG_TXPWR_INVALID_BASE;
			pwr_info->IndexBW40_Base[path][group] = PG_TXPWR_INVALID_BASE;
		}
		for (tx_idx = 0; tx_idx < MAX_TX_COUNT; tx_idx++) {
			pwr_info->CCK_Diff[path][tx_idx] = PG_TXPWR_INVALID_DIFF;
			pwr_info->OFDM_Diff[path][tx_idx] = PG_TXPWR_INVALID_DIFF;
			pwr_info->BW20_Diff[path][tx_idx] = PG_TXPWR_INVALID_DIFF;
			pwr_info->BW40_Diff[path][tx_idx] = PG_TXPWR_INVALID_DIFF;
		}
	}

	/* init for dummy base and diff */
	for (path = 0; path < MAX_RF_PATH; path++) {
		if (!HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path))
			break;
		/* 2.4G BW40 base has 1 less group than CCK base*/
		pwr_info->IndexBW40_Base[path][MAX_CHNL_GROUP_24G - 1] = 0;

		/* dummy diff */
		pwr_info->CCK_Diff[path][0] = 0; /* 2.4G CCK-1TX */
		pwr_info->BW40_Diff[path][0] = 0; /* 2.4G BW40-1S */
	}
}

static inline void hal_init_pg_txpwr_info_5g(struct adapter *adapter, struct TxPowerInfo5G *pwr_info)
{
}

#if DBG_PG_TXPWR_READ
#define LOAD_PG_TXPWR_WARN_COND(_txpwr_src) 1
#else
#define LOAD_PG_TXPWR_WARN_COND(_txpwr_src) (_txpwr_src > PG_TXPWR_SRC_PG_DATA)
#endif

static u16 hal_load_pg_txpwr_info_path_2g(
	struct adapter *adapter,
	struct TxPowerInfo24G	*pwr_info,
	u32 path,
	u8 txpwr_src,
	const struct map_t *txpwr_map,
	u16 pg_offset)
{
#define PG_TXPWR_1PATH_BYTE_NUM_2G 18

	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u16 offset = pg_offset;
	u8 group, tx_idx;
	u8 val;
	u8 tmp_base;
	s8 tmp_diff;

	if (!pwr_info || !hal_chk_band_cap(adapter, BAND_CAP_2G)) {
		offset += PG_TXPWR_1PATH_BYTE_NUM_2G;
		goto exit;
	}

	if (DBG_PG_TXPWR_READ)
		RTW_INFO("%s [%c] offset:0x%03x\n", __func__, rf_path_char(path), offset);

	for (group = 0; group < MAX_CHNL_GROUP_24G; group++) {
		if (HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path)) {
			tmp_base = map_read8(txpwr_map, offset);
			if (!IS_PG_TXPWR_BASE_INVALID(tmp_base)
				&& IS_PG_TXPWR_BASE_INVALID(pwr_info->IndexCCK_Base[path][group])
			) {
				pwr_info->IndexCCK_Base[path][group] = tmp_base;
				if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
					RTW_INFO("[%c] 2G G%02d CCK-1T base:%u from %s\n", rf_path_char(path), group, tmp_base, pg_txpwr_src_str(txpwr_src));
			}
		}
		offset++;
	}

	for (group = 0; group < MAX_CHNL_GROUP_24G - 1; group++) {
		if (HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path)) {
			tmp_base = map_read8(txpwr_map, offset);
			if (!IS_PG_TXPWR_BASE_INVALID(tmp_base)
				&& IS_PG_TXPWR_BASE_INVALID(pwr_info->IndexBW40_Base[path][group])
			) {
				pwr_info->IndexBW40_Base[path][group] =	tmp_base;
				if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
					RTW_INFO("[%c] 2G G%02d BW40-1S base:%u from %s\n", rf_path_char(path), group, tmp_base, pg_txpwr_src_str(txpwr_src));
			}
		}
		offset++;
	}

	for (tx_idx = 0; tx_idx < MAX_TX_COUNT; tx_idx++) {
		if (tx_idx == 0) {
			if (HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path) && HAL_SPEC_CHK_TX_CNT(hal_spec, tx_idx)) {
				val = map_read8(txpwr_map, offset);
				tmp_diff = PG_TXPWR_MSB_DIFF_TO_S8BIT(val);
				if (!IS_PG_TXPWR_DIFF_INVALID(tmp_diff)
					&& IS_PG_TXPWR_DIFF_INVALID(pwr_info->BW20_Diff[path][tx_idx])
				) {
					pwr_info->BW20_Diff[path][tx_idx] = tmp_diff;
					if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
						RTW_INFO("[%c] 2G BW20-%dS diff:%d from %s\n", rf_path_char(path), tx_idx + 1, tmp_diff, pg_txpwr_src_str(txpwr_src));
				}
				tmp_diff = PG_TXPWR_LSB_DIFF_TO_S8BIT(val);
				if (!IS_PG_TXPWR_DIFF_INVALID(tmp_diff)
					&& IS_PG_TXPWR_DIFF_INVALID(pwr_info->OFDM_Diff[path][tx_idx])
				) {
					pwr_info->OFDM_Diff[path][tx_idx] = tmp_diff;
					if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
						RTW_INFO("[%c] 2G OFDM-%dT diff:%d from %s\n", rf_path_char(path), tx_idx + 1, tmp_diff, pg_txpwr_src_str(txpwr_src));
				}
			}
			offset++;
		} else {
			if (HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path) && HAL_SPEC_CHK_TX_CNT(hal_spec, tx_idx)) {
				val = map_read8(txpwr_map, offset);
				tmp_diff = PG_TXPWR_MSB_DIFF_TO_S8BIT(val);
				if (!IS_PG_TXPWR_DIFF_INVALID(tmp_diff)
					&& IS_PG_TXPWR_DIFF_INVALID(pwr_info->BW40_Diff[path][tx_idx])
				) {
					pwr_info->BW40_Diff[path][tx_idx] = tmp_diff;
					if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
						RTW_INFO("[%c] 2G BW40-%dS diff:%d from %s\n", rf_path_char(path), tx_idx + 1, tmp_diff, pg_txpwr_src_str(txpwr_src));

				}
				tmp_diff = PG_TXPWR_LSB_DIFF_TO_S8BIT(val);
				if (!IS_PG_TXPWR_DIFF_INVALID(tmp_diff)
					&& IS_PG_TXPWR_DIFF_INVALID(pwr_info->BW20_Diff[path][tx_idx])
				) {
					pwr_info->BW20_Diff[path][tx_idx] = tmp_diff;
					if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
						RTW_INFO("[%c] 2G BW20-%dS diff:%d from %s\n", rf_path_char(path), tx_idx + 1, tmp_diff, pg_txpwr_src_str(txpwr_src));
				}
			}
			offset++;

			if (HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path) && HAL_SPEC_CHK_TX_CNT(hal_spec, tx_idx)) {
				val = map_read8(txpwr_map, offset);
				tmp_diff = PG_TXPWR_MSB_DIFF_TO_S8BIT(val);
				if (!IS_PG_TXPWR_DIFF_INVALID(tmp_diff)
					&& IS_PG_TXPWR_DIFF_INVALID(pwr_info->OFDM_Diff[path][tx_idx])
				) {
					pwr_info->OFDM_Diff[path][tx_idx] = tmp_diff;
					if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
						RTW_INFO("[%c] 2G OFDM-%dT diff:%d from %s\n", rf_path_char(path), tx_idx + 1, tmp_diff, pg_txpwr_src_str(txpwr_src));
				}
				tmp_diff = PG_TXPWR_LSB_DIFF_TO_S8BIT(val);
				if (!IS_PG_TXPWR_DIFF_INVALID(tmp_diff)
					&& IS_PG_TXPWR_DIFF_INVALID(pwr_info->CCK_Diff[path][tx_idx])
				) {
					pwr_info->CCK_Diff[path][tx_idx] =	tmp_diff;
					if (LOAD_PG_TXPWR_WARN_COND(txpwr_src))
						RTW_INFO("[%c] 2G CCK-%dT diff:%d from %s\n", rf_path_char(path), tx_idx + 1, tmp_diff, pg_txpwr_src_str(txpwr_src));
				}
			}
			offset++;
		}
	}

	if (offset != pg_offset + PG_TXPWR_1PATH_BYTE_NUM_2G) {
		RTW_ERR("%s parse %d bytes != %d\n", __func__, offset - pg_offset, PG_TXPWR_1PATH_BYTE_NUM_2G);
		rtw_warn_on(1);
	}

exit:	
	return offset;
}

static u16 hal_load_pg_txpwr_info_path_5g(
	struct adapter *adapter,
	struct TxPowerInfo5G	*pwr_info,
	u32 path,
	u8 txpwr_src,
	const struct map_t *txpwr_map,
	u16 pg_offset)
{
#define PG_TXPWR_1PATH_BYTE_NUM_5G 24
	u16 offset = pg_offset;

	offset += PG_TXPWR_1PATH_BYTE_NUM_5G;
	return offset;
}

static void hal_load_pg_txpwr_info(
	struct adapter *adapter,
	struct TxPowerInfo24G *pwr_info_2g,
	struct TxPowerInfo5G *pwr_info_5g,
	u8 *pg_data,
	bool AutoLoadFail
)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 path;
	u16 pg_offset;
	u8 txpwr_src = PG_TXPWR_SRC_PG_DATA;
	struct map_t pg_data_map = MAP_ENT(184, 1, 0xFF, MAPSEG_PTR_ENT(0x00, 184, pg_data));
	const struct map_t *txpwr_map = NULL;

	/* init with invalid value and some dummy base and diff */
	hal_init_pg_txpwr_info_2g(adapter, pwr_info_2g);
	hal_init_pg_txpwr_info_5g(adapter, pwr_info_5g);

select_src:
	pg_offset = 0x10;

	switch (txpwr_src) {
	case PG_TXPWR_SRC_PG_DATA:
		txpwr_map = &pg_data_map;
		break;
	case PG_TXPWR_SRC_IC_DEF:
		txpwr_map = hal_pg_txpwr_def_info(adapter);
		break;
	case PG_TXPWR_SRC_DEF:
	default:
		txpwr_map = &pg_txpwr_def_info;
		break;
	};

	if (!txpwr_map)
		goto end_parse;

	for (path = 0; path < MAX_RF_PATH ; path++) {
		if (!HAL_SPEC_CHK_RF_PATH_2G(hal_spec, path) && !HAL_SPEC_CHK_RF_PATH_5G(hal_spec, path))
			break;
		pg_offset = hal_load_pg_txpwr_info_path_2g(adapter, pwr_info_2g, path, txpwr_src, txpwr_map, pg_offset);
		pg_offset = hal_load_pg_txpwr_info_path_5g(adapter, pwr_info_5g, path, txpwr_src, txpwr_map, pg_offset);
	}

	if (hal_chk_pg_txpwr_info_2g(adapter, pwr_info_2g) == _SUCCESS
		&& hal_chk_pg_txpwr_info_5g(adapter, pwr_info_5g) == _SUCCESS)
		goto exit;

end_parse:
	txpwr_src++;
	if (txpwr_src < PG_TXPWR_SRC_NUM)
		goto select_src;

	if (hal_chk_pg_txpwr_info_2g(adapter, pwr_info_2g) != _SUCCESS
		|| hal_chk_pg_txpwr_info_5g(adapter, pwr_info_5g) != _SUCCESS)
		rtw_warn_on(1);

exit:
	#if DBG_PG_TXPWR_READ
	if (pwr_info_2g)
		dump_pg_txpwr_info_2g(RTW_DBGDUMP, pwr_info_2g, 4, 4);
	if (pwr_info_5g)
		dump_pg_txpwr_info_5g(RTW_DBGDUMP, pwr_info_5g, 4, 4);
	#endif

	return;
}

void hal_load_txpwr_info(
	struct adapter *adapter,
	struct TxPowerInfo24G *pwr_info_2g,
	struct TxPowerInfo5G *pwr_info_5g,
	u8 *pg_data
)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 max_tx_cnt = hal_spec->max_tx_cnt;
	u8 rfpath, ch_idx, group, tx_idx;

	/* load from pg data (or default value) */
	hal_load_pg_txpwr_info(adapter, pwr_info_2g, pwr_info_5g, pg_data, false);

	/* transform to hal_data */
	for (rfpath = 0; rfpath < MAX_RF_PATH; rfpath++) {

		if (!pwr_info_2g || !HAL_SPEC_CHK_RF_PATH_2G(hal_spec, rfpath))
			goto bypass_2g;

		/* 2.4G base */
		for (ch_idx = 0; ch_idx < CENTER_CH_2G_NUM; ch_idx++) {
			u8 cck_group;

			if (rtw_get_ch_group(ch_idx + 1, &group, &cck_group) != BAND_ON_2_4G)
				continue;

			hal_data->Index24G_CCK_Base[rfpath][ch_idx] = pwr_info_2g->IndexCCK_Base[rfpath][cck_group];
			hal_data->Index24G_BW40_Base[rfpath][ch_idx] = pwr_info_2g->IndexBW40_Base[rfpath][group];
		}

		/* 2.4G diff */
		for (tx_idx = 0; tx_idx < MAX_TX_COUNT; tx_idx++) {
			if (tx_idx >= max_tx_cnt)
				break;

			hal_data->CCK_24G_Diff[rfpath][tx_idx] = pwr_info_2g->CCK_Diff[rfpath][tx_idx];
			hal_data->OFDM_24G_Diff[rfpath][tx_idx] = pwr_info_2g->OFDM_Diff[rfpath][tx_idx];
			hal_data->BW20_24G_Diff[rfpath][tx_idx] = pwr_info_2g->BW20_Diff[rfpath][tx_idx];
			hal_data->BW40_24G_Diff[rfpath][tx_idx] = pwr_info_2g->BW40_Diff[rfpath][tx_idx];
		}
bypass_2g:
		;
	}
}

void dump_hal_txpwr_info_2g(void *sel, struct adapter *adapter, u8 rfpath_num, u8 max_tx_cnt)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	int path, ch_idx, tx_idx;

	RTW_PRINT_SEL(sel, "2.4G\n");
	RTW_PRINT_SEL(sel, "CCK-1T base:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (ch_idx = 0; ch_idx < CENTER_CH_2G_NUM; ch_idx++)
		_RTW_PRINT_SEL(sel, "%2d ", center_ch_2g[ch_idx]);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (ch_idx = 0; ch_idx < CENTER_CH_2G_NUM; ch_idx++)
			_RTW_PRINT_SEL(sel, "%2u ", hal_data->Index24G_CCK_Base[path][ch_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "CCK diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
		_RTW_PRINT_SEL(sel, "%dT ", tx_idx + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", hal_data->CCK_24G_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW40-1S base:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (ch_idx = 0; ch_idx < CENTER_CH_2G_NUM; ch_idx++)
		_RTW_PRINT_SEL(sel, "%2d ", center_ch_2g[ch_idx]);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (ch_idx = 0; ch_idx < CENTER_CH_2G_NUM; ch_idx++)
			_RTW_PRINT_SEL(sel, "%2u ", hal_data->Index24G_BW40_Base[path][ch_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "OFDM diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
		_RTW_PRINT_SEL(sel, "%dT ", tx_idx + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", hal_data->OFDM_24G_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW20 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
		_RTW_PRINT_SEL(sel, "%dS ", tx_idx + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", hal_data->BW20_24G_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "BW40 diff:\n");
	RTW_PRINT_SEL(sel, "%4s ", "");
	for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
		_RTW_PRINT_SEL(sel, "%dS ", tx_idx + 1);
	_RTW_PRINT_SEL(sel, "\n");
	for (path = 0; path < MAX_RF_PATH && path < rfpath_num; path++) {
		RTW_PRINT_SEL(sel, "[%c]: ", rf_path_char(path));
		for (tx_idx = RF_1TX; tx_idx < MAX_TX_COUNT && tx_idx < max_tx_cnt; tx_idx++)
			_RTW_PRINT_SEL(sel, "%2d ", hal_data->BW40_24G_Diff[path][tx_idx]);
		_RTW_PRINT_SEL(sel, "\n");
	}
	RTW_PRINT_SEL(sel, "\n");
}

void dump_hal_txpwr_info_5g(void *sel, struct adapter *adapter, u8 rfpath_num, u8 max_tx_cnt)
{
}

/*
* rtw_regsty_get_target_tx_power -
*
* Return dBm or -1 for undefined
*/
static s8 rtw_regsty_get_target_tx_power(
	struct adapter *		Adapter,
	u8				Band,
	u8				RfPath,
	enum rate_section RateSection
)
{
	struct registry_priv *regsty = adapter_to_regsty(Adapter);
	s8 value = 0;

	if (RfPath > RF_PATH_D) {
		RTW_PRINT("%s invalid RfPath:%d\n", __func__, RfPath);
		return -1;
	}

	if (Band != BAND_ON_2_4G) {
		RTW_PRINT("%s invalid Band:%d\n", __func__, Band);
		return -1;
	}

	if (RateSection >= RATE_SECTION_NUM) {
		RTW_PRINT("%s invalid RateSection:%d in Band:%d, RfPath:%d\n", __func__
			, RateSection, Band, RfPath);
		return -1;
	}

	if (Band == BAND_ON_2_4G)
		value = regsty->target_tx_pwr_2g[RfPath][RateSection];

	return value;
}

static bool rtw_regsty_chk_target_tx_power_valid(struct adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int path, tx_num, band, rs;
	s8 target;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++) {
		if (!hal_is_band_support(adapter, band))
			continue;

		for (path = 0; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				break;

			for (rs = 0; rs < RATE_SECTION_NUM; rs++) {
				tx_num = rate_section_to_tx_num(rs);
				if (tx_num >= hal_spec->tx_nss_num)
					continue;

				if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
					continue;

				if (IS_VHT_RATE_SECTION(rs))
					continue;

				target = rtw_regsty_get_target_tx_power(adapter, band, path, rs);
				if (target == -1) {
					RTW_PRINT("%s return false for band:%d, path:%d, rs:%d, t:%d\n", __func__, band, path, rs, target);
					return false;
				}
			}
		}
	}

	return true;
}

/*
* PHY_GetTxPowerByRateBase -
*
* Return 2 times of dBm
*/
u8
PHY_GetTxPowerByRateBase(
	struct adapter *		Adapter,
	u8				Band,
	u8				RfPath,
	enum rate_section RateSection
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	u8 value = 0;

	if (RfPath > RF_PATH_D) {
		RTW_PRINT("%s invalid RfPath:%d\n", __func__, RfPath);
		return 0;
	}

	if (Band != BAND_ON_2_4G && Band != BAND_ON_5G) {
		RTW_PRINT("%s invalid Band:%d\n", __func__, Band);
		return 0;
	}

	if (RateSection >= RATE_SECTION_NUM
		|| (Band == BAND_ON_5G && RateSection == CCK)
	) {
		RTW_PRINT("%s invalid RateSection:%d in Band:%d, RfPath:%d\n", __func__
			, RateSection, Band, RfPath);
		return 0;
	}

	if (Band == BAND_ON_2_4G)
		value = pHalData->TxPwrByRateBase2_4G[RfPath][RateSection];
	else /* BAND_ON_5G */
		value = pHalData->TxPwrByRateBase5G[RfPath][RateSection - 1];

	return value;
}

static void
phy_SetTxPowerByRateBase(
	struct adapter *		Adapter,
	u8				Band,
	u8				RfPath,
	enum rate_section RateSection,
	u8				Value
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	if (RfPath > RF_PATH_D) {
		RTW_PRINT("%s invalid RfPath:%d\n", __func__, RfPath);
		return;
	}

	if (Band != BAND_ON_2_4G && Band != BAND_ON_5G) {
		RTW_PRINT("%s invalid Band:%d\n", __func__, Band);
		return;
	}

	if (RateSection >= RATE_SECTION_NUM
		|| (Band == BAND_ON_5G && RateSection == CCK)
	) {
		RTW_PRINT("%s invalid RateSection:%d in %sG, RfPath:%d\n", __func__
			, RateSection, (Band == BAND_ON_2_4G) ? "2.4" : "5", RfPath);
		return;
	}

	if (Band == BAND_ON_2_4G)
		pHalData->TxPwrByRateBase2_4G[RfPath][RateSection] = Value;
	else /* BAND_ON_5G */
		pHalData->TxPwrByRateBase5G[RfPath][RateSection - 1] = Value;
}

static inline bool phy_is_txpwr_by_rate_undefined_of_band_path(struct adapter *adapter, u8 band, u8 path)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 rate_idx = 0;

	for (rate_idx = 0; rate_idx < TX_PWR_BY_RATE_NUM_RATE; rate_idx++) {
		if (hal_data->TxPwrByRateOffset[band][path][rate_idx] != 0)
			goto exit;
	}

exit:
	return rate_idx >= TX_PWR_BY_RATE_NUM_RATE ? true : false;
}

static inline void phy_txpwr_by_rate_duplicate_band_path(struct adapter *adapter, u8 band, u8 s_path, u8 t_path)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 rate_idx = 0;

	for (rate_idx = 0; rate_idx < TX_PWR_BY_RATE_NUM_RATE; rate_idx++)
		hal_data->TxPwrByRateOffset[band][t_path][rate_idx] = hal_data->TxPwrByRateOffset[band][s_path][rate_idx];
}

static void phy_txpwr_by_rate_chk_for_path_dup(struct adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 band, path;
	s8 src_path;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++)
		for (path = RF_PATH_A; path < RF_PATH_MAX; path++)
			hal_data->txpwr_by_rate_undefined_band_path[band][path] = 0;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++) {
		if (!hal_is_band_support(adapter, band))
			continue;

		for (path = RF_PATH_A; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				continue;

			if (phy_is_txpwr_by_rate_undefined_of_band_path(adapter, band, path))
				hal_data->txpwr_by_rate_undefined_band_path[band][path] = 1;
		}
	}

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++) {
		if (!hal_is_band_support(adapter, band))
			continue;

		src_path = -1;
		for (path = RF_PATH_A; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				continue;

			/* find src */
			if (src_path == -1 && hal_data->txpwr_by_rate_undefined_band_path[band][path] == 0)
				src_path = path;
		}

		if (src_path == -1) {
			RTW_ERR("%s all power by rate undefined\n", __func__);
			continue;
		}

		for (path = RF_PATH_A; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				continue;

			/* duplicate src to undefined one */
			if (hal_data->txpwr_by_rate_undefined_band_path[band][path] == 1) {
				RTW_INFO("%s duplicate %s [%c] to [%c]\n", __func__
					, band_str(band), rf_path_char(src_path), rf_path_char(path));
				phy_txpwr_by_rate_duplicate_band_path(adapter, band, src_path, path);
			}
		}
	}
}

static void
phy_StoreTxPowerByRateBase(
	struct adapter *	pAdapter
)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(pAdapter);
	struct registry_priv *regsty = adapter_to_regsty(pAdapter);

	u8 rate_sec_base[RATE_SECTION_NUM] = {
		MGN_11M,
		MGN_54M,
		MGN_MCS7,
		MGN_MCS15,
		MGN_MCS23,
		MGN_MCS31,
		MGN_VHT1SS_MCS7,
		MGN_VHT2SS_MCS7,
		MGN_VHT3SS_MCS7,
		MGN_VHT4SS_MCS7,
	};

	u8 band, path, rs, tx_num, base;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++) {
		if (!hal_is_band_support(pAdapter, band))
			continue;

		for (path = RF_PATH_A; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				break;

			for (rs = 0; rs < RATE_SECTION_NUM; rs++) {
				tx_num = rate_section_to_tx_num(rs);
				if (tx_num >= hal_spec->tx_nss_num)
					continue;

				if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
					continue;

				if (IS_VHT_RATE_SECTION(rs))
					continue;

				if (regsty->target_tx_pwr_valid)
					base = 2 * rtw_regsty_get_target_tx_power(pAdapter, band, path, rs);
				else
					base = _PHY_GetTxPowerByRate(pAdapter, band, path, rate_sec_base[rs]);
				phy_SetTxPowerByRateBase(pAdapter, band, path, rs, base);
			}
		}
	}
}

void
PHY_GetRateValuesOfTxPowerByRate(
	struct adapter * pAdapter,
	u32 RegAddr,
	u32 BitMask,
	u32 Value,
	u8 *Rate,
	s8 *PwrByRateVal,
	u8 *RateNum
)
{
	u8 i;

	switch (RegAddr) {
	case rTxAGC_A_Rate18_06:
	case rTxAGC_B_Rate18_06:
		Rate[0] = MGN_6M;
		Rate[1] = MGN_9M;
		Rate[2] = MGN_12M;
		Rate[3] = MGN_18M;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case rTxAGC_A_Rate54_24:
	case rTxAGC_B_Rate54_24:
		Rate[0] = MGN_24M;
		Rate[1] = MGN_36M;
		Rate[2] = MGN_48M;
		Rate[3] = MGN_54M;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case rTxAGC_A_CCK1_Mcint:
		Rate[0] = MGN_1M;
		PwrByRateVal[0] = (s8)((((Value >> (8 + 4)) & 0xF)) * 10 +
				       ((Value >> 8) & 0xF));
		*RateNum = 1;
		break;
	case rTxAGC_B_CCK11_A_CCK2_11:
		if (BitMask == 0xffffff00) {
			Rate[0] = MGN_2M;
			Rate[1] = MGN_5_5M;
			Rate[2] = MGN_11M;
			for (i = 1; i < 4; ++i) {
				PwrByRateVal[i - 1] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
						   ((Value >> (i * 8)) & 0xF));
			}
			*RateNum = 3;
		} else if (BitMask == 0x000000ff) {
			Rate[0] = MGN_11M;
			PwrByRateVal[0] = (s8)((((Value >> 4) & 0xF)) * 10 +
					       (Value & 0xF));
			*RateNum = 1;
		}
		break;
	case rTxAGC_A_Mcs03_Mcs00:
	case rTxAGC_B_Mcs03_Mcs00:
		Rate[0] = MGN_MCS0;
		Rate[1] = MGN_MCS1;
		Rate[2] = MGN_MCS2;
		Rate[3] = MGN_MCS3;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case rTxAGC_A_Mcs07_Mcs04:
	case rTxAGC_B_Mcs07_Mcs04:
		Rate[0] = MGN_MCS4;
		Rate[1] = MGN_MCS5;
		Rate[2] = MGN_MCS6;
		Rate[3] = MGN_MCS7;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case rTxAGC_A_Mcs11_Mcs08:
	case rTxAGC_B_Mcs11_Mcs08:
		Rate[0] = MGN_MCS8;
		Rate[1] = MGN_MCS9;
		Rate[2] = MGN_MCS10;
		Rate[3] = MGN_MCS11;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case rTxAGC_A_Mcs15_Mcs12:
	case rTxAGC_B_Mcs15_Mcs12:
		Rate[0] = MGN_MCS12;
		Rate[1] = MGN_MCS13;
		Rate[2] = MGN_MCS14;
		Rate[3] = MGN_MCS15;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case rTxAGC_B_CCK1_55_Mcint:
		Rate[0] = MGN_1M;
		Rate[1] = MGN_2M;
		Rate[2] = MGN_5_5M;
		for (i = 1; i < 4; ++i) {
			PwrByRateVal[i - 1] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
						   ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 3;
		break;
	case 0xC20:
	case 0xE20:
	case 0x1820:
	case 0x1a20:
		Rate[0] = MGN_1M;
		Rate[1] = MGN_2M;
		Rate[2] = MGN_5_5M;
		Rate[3] = MGN_11M;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC24:
	case 0xE24:
	case 0x1824:
	case 0x1a24:
		Rate[0] = MGN_6M;
		Rate[1] = MGN_9M;
		Rate[2] = MGN_12M;
		Rate[3] = MGN_18M;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC28:
	case 0xE28:
	case 0x1828:
	case 0x1a28:
		Rate[0] = MGN_24M;
		Rate[1] = MGN_36M;
		Rate[2] = MGN_48M;
		Rate[3] = MGN_54M;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC2C:
	case 0xE2C:
	case 0x182C:
	case 0x1a2C:
		Rate[0] = MGN_MCS0;
		Rate[1] = MGN_MCS1;
		Rate[2] = MGN_MCS2;
		Rate[3] = MGN_MCS3;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC30:
	case 0xE30:
	case 0x1830:
	case 0x1a30:
		Rate[0] = MGN_MCS4;
		Rate[1] = MGN_MCS5;
		Rate[2] = MGN_MCS6;
		Rate[3] = MGN_MCS7;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC34:
	case 0xE34:
	case 0x1834:
	case 0x1a34:
		Rate[0] = MGN_MCS8;
		Rate[1] = MGN_MCS9;
		Rate[2] = MGN_MCS10;
		Rate[3] = MGN_MCS11;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC38:
	case 0xE38:
	case 0x1838:
	case 0x1a38:
		Rate[0] = MGN_MCS12;
		Rate[1] = MGN_MCS13;
		Rate[2] = MGN_MCS14;
		Rate[3] = MGN_MCS15;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC3C:
	case 0xE3C:
	case 0x183C:
	case 0x1a3C:
		Rate[0] = MGN_VHT1SS_MCS0;
		Rate[1] = MGN_VHT1SS_MCS1;
		Rate[2] = MGN_VHT1SS_MCS2;
		Rate[3] = MGN_VHT1SS_MCS3;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC40:
	case 0xE40:
	case 0x1840:
	case 0x1a40:
		Rate[0] = MGN_VHT1SS_MCS4;
		Rate[1] = MGN_VHT1SS_MCS5;
		Rate[2] = MGN_VHT1SS_MCS6;
		Rate[3] = MGN_VHT1SS_MCS7;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC44:
	case 0xE44:
	case 0x1844:
	case 0x1a44:
		Rate[0] = MGN_VHT1SS_MCS8;
		Rate[1] = MGN_VHT1SS_MCS9;
		Rate[2] = MGN_VHT2SS_MCS0;
		Rate[3] = MGN_VHT2SS_MCS1;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC48:
	case 0xE48:
	case 0x1848:
	case 0x1a48:
		Rate[0] = MGN_VHT2SS_MCS2;
		Rate[1] = MGN_VHT2SS_MCS3;
		Rate[2] = MGN_VHT2SS_MCS4;
		Rate[3] = MGN_VHT2SS_MCS5;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xC4C:
	case 0xE4C:
	case 0x184C:
	case 0x1a4C:
		Rate[0] = MGN_VHT2SS_MCS6;
		Rate[1] = MGN_VHT2SS_MCS7;
		Rate[2] = MGN_VHT2SS_MCS8;
		Rate[3] = MGN_VHT2SS_MCS9;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xCD8:
	case 0xED8:
	case 0x18D8:
	case 0x1aD8:
		Rate[0] = MGN_MCS16;
		Rate[1] = MGN_MCS17;
		Rate[2] = MGN_MCS18;
		Rate[3] = MGN_MCS19;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xCDC:
	case 0xEDC:
	case 0x18DC:
	case 0x1aDC:
		Rate[0] = MGN_MCS20;
		Rate[1] = MGN_MCS21;
		Rate[2] = MGN_MCS22;
		Rate[3] = MGN_MCS23;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xCE0:
	case 0xEE0:
	case 0x18E0:
	case 0x1aE0:
		Rate[0] = MGN_VHT3SS_MCS0;
		Rate[1] = MGN_VHT3SS_MCS1;
		Rate[2] = MGN_VHT3SS_MCS2;
		Rate[3] = MGN_VHT3SS_MCS3;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xCE4:
	case 0xEE4:
	case 0x18E4:
	case 0x1aE4:
		Rate[0] = MGN_VHT3SS_MCS4;
		Rate[1] = MGN_VHT3SS_MCS5;
		Rate[2] = MGN_VHT3SS_MCS6;
		Rate[3] = MGN_VHT3SS_MCS7;
		for (i = 0; i < 4; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 4;
		break;
	case 0xCE8:
	case 0xEE8:
	case 0x18E8:
	case 0x1aE8:
		Rate[0] = MGN_VHT3SS_MCS8;
		Rate[1] = MGN_VHT3SS_MCS9;
		for (i = 0; i < 2; ++i) {
			PwrByRateVal[i] = (s8)((((Value >> (i * 8 + 4)) & 0xF)) * 10 +
					       ((Value >> (i * 8)) & 0xF));
		}
		*RateNum = 2;
		break;
	default:
		RTW_PRINT("Invalid RegAddr 0x%x in %s()\n", RegAddr, __func__);
		break;
	};
}

static void
PHY_StoreTxPowerByRateNew(
	struct adapter *	pAdapter,
	u32			Band,
	u32			RfPath,
	u32			RegAddr,
	u32			BitMask,
	u32			Data
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(pAdapter);
	u8	i = 0, rates[4] = {0}, rateNum = 0;
	s8	PwrByRateVal[4] = {0};

	PHY_GetRateValuesOfTxPowerByRate(pAdapter, RegAddr, BitMask, Data, rates, PwrByRateVal, &rateNum);

	if (Band != BAND_ON_2_4G && Band != BAND_ON_5G) {
		RTW_PRINT("Invalid Band %d\n", Band);
		return;
	}

	if (RfPath > RF_PATH_D) {
		RTW_PRINT("Invalid RfPath %d\n", RfPath);
		return;
	}

	for (i = 0; i < rateNum; ++i) {
		u8 rate_idx = PHY_GetRateIndexOfTxPowerByRate(rates[i]);

		pHalData->TxPwrByRateOffset[Band][RfPath][rate_idx] = PwrByRateVal[i];
	}
}

void
PHY_InitTxPowerByRate(
	struct adapter *	pAdapter
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(pAdapter);
	u8	band = 0, rfPath = 0, rate = 0;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; ++band)
		for (rfPath = 0; rfPath < TX_PWR_BY_RATE_NUM_RF; ++rfPath)
				for (rate = 0; rate < TX_PWR_BY_RATE_NUM_RATE; ++rate)
					pHalData->TxPwrByRateOffset[band][rfPath][rate] = 0;
}

void
phy_store_tx_power_by_rate(
	struct adapter *	pAdapter,
	u32			Band,
	u32			RfPath,
	u32			TxNum,
	u32			RegAddr,
	u32			BitMask,
	u32			Data
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(pAdapter);
	struct PHY_DM_STRUCT		*pDM_Odm = &pHalData->odmpriv;

	if (pDM_Odm->phy_reg_pg_version > 0)
		PHY_StoreTxPowerByRateNew(pAdapter, Band, RfPath, RegAddr, BitMask, Data);
	else
		RTW_INFO("Invalid PHY_REG_PG.txt version %d\n",  pDM_Odm->phy_reg_pg_version);

}

static void
phy_ConvertTxPowerByRateInDbmToRelativeValues(
	struct adapter *	pAdapter
)
{
	u8 base = 0, i = 0, value = 0, band = 0, path = 0;
	u8	cckRates[4] = {MGN_1M, MGN_2M, MGN_5_5M, MGN_11M},
		ofdmRates[8] = {MGN_6M, MGN_9M, MGN_12M, MGN_18M, MGN_24M, MGN_36M, MGN_48M, MGN_54M},
		mcs0_7Rates[8] = {MGN_MCS0, MGN_MCS1, MGN_MCS2, MGN_MCS3, MGN_MCS4, MGN_MCS5, MGN_MCS6, MGN_MCS7},
		mcs8_15Rates[8] = {MGN_MCS8, MGN_MCS9, MGN_MCS10, MGN_MCS11, MGN_MCS12, MGN_MCS13, MGN_MCS14, MGN_MCS15},
		mcs16_23Rates[8] = {MGN_MCS16, MGN_MCS17, MGN_MCS18, MGN_MCS19, MGN_MCS20, MGN_MCS21, MGN_MCS22, MGN_MCS23},
		vht1ssRates[10] = {MGN_VHT1SS_MCS0, MGN_VHT1SS_MCS1, MGN_VHT1SS_MCS2, MGN_VHT1SS_MCS3, MGN_VHT1SS_MCS4,
			MGN_VHT1SS_MCS5, MGN_VHT1SS_MCS6, MGN_VHT1SS_MCS7, MGN_VHT1SS_MCS8, MGN_VHT1SS_MCS9},
		vht2ssRates[10] = {MGN_VHT2SS_MCS0, MGN_VHT2SS_MCS1, MGN_VHT2SS_MCS2, MGN_VHT2SS_MCS3, MGN_VHT2SS_MCS4,
			MGN_VHT2SS_MCS5, MGN_VHT2SS_MCS6, MGN_VHT2SS_MCS7, MGN_VHT2SS_MCS8, MGN_VHT2SS_MCS9},
		vht3ssRates[10] = {MGN_VHT3SS_MCS0, MGN_VHT3SS_MCS1, MGN_VHT3SS_MCS2, MGN_VHT3SS_MCS3, MGN_VHT3SS_MCS4,
			MGN_VHT3SS_MCS5, MGN_VHT3SS_MCS6, MGN_VHT3SS_MCS7, MGN_VHT3SS_MCS8, MGN_VHT3SS_MCS9};

	/* RTW_INFO("===>PHY_ConvertTxPowerByRateInDbmToRelativeValues()\n" ); */

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; ++band) {
		for (path = RF_PATH_A; path <= RF_PATH_D; ++path) {
			/* CCK */
			if (band == BAND_ON_2_4G) {
				base = PHY_GetTxPowerByRateBase(pAdapter, band, path, CCK);
				for (i = 0; i < sizeof(cckRates); ++i) {
					value = PHY_GetTxPowerByRate(pAdapter, band, path, cckRates[i]);
					PHY_SetTxPowerByRate(pAdapter, band, path, cckRates[i], value - base);
				}
			}

			/* OFDM */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, OFDM);
			for (i = 0; i < sizeof(ofdmRates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, ofdmRates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, ofdmRates[i], value - base);
			}

			/* HT MCS0~7 */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, HT_1SS);
			for (i = 0; i < sizeof(mcs0_7Rates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, mcs0_7Rates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, mcs0_7Rates[i], value - base);
			}

			/* HT MCS8~15 */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, HT_2SS);
			for (i = 0; i < sizeof(mcs8_15Rates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, mcs8_15Rates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, mcs8_15Rates[i], value - base);
			}

			/* HT MCS16~23 */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, HT_3SS);
			for (i = 0; i < sizeof(mcs16_23Rates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, mcs16_23Rates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, mcs16_23Rates[i], value - base);
			}

			/* VHT 1SS */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, VHT_1SS);
			for (i = 0; i < sizeof(vht1ssRates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, vht1ssRates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, vht1ssRates[i], value - base);
			}

			/* VHT 2SS */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, VHT_2SS);
			for (i = 0; i < sizeof(vht2ssRates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, vht2ssRates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, vht2ssRates[i], value - base);
			}

			/* VHT 3SS */
			base = PHY_GetTxPowerByRateBase(pAdapter, band, path, VHT_3SS);
			for (i = 0; i < sizeof(vht3ssRates); ++i) {
				value = PHY_GetTxPowerByRate(pAdapter, band, path, vht3ssRates[i]);
				PHY_SetTxPowerByRate(pAdapter, band, path, vht3ssRates[i], value - base);
			}
		}
	}

	/* RTW_INFO("<===PHY_ConvertTxPowerByRateInDbmToRelativeValues()\n" ); */
}

/*
  * This function must be called if the value in the PHY_REG_PG.txt(or header)
  * is exact dBm values
  */
void
PHY_TxPowerByRateConfiguration(
	struct adapter *			pAdapter
)
{
	phy_txpwr_by_rate_chk_for_path_dup(pAdapter);
	phy_StoreTxPowerByRateBase(pAdapter);
	phy_ConvertTxPowerByRateInDbmToRelativeValues(pAdapter);
}

void
phy_set_tx_power_index_by_rate_section(
	struct adapter *		pAdapter,
	enum rf_path		RFPath,
	u8				Channel,
	u8				RateSection
)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(pAdapter);

	if (RateSection >= RATE_SECTION_NUM) {
		RTW_INFO("Invalid RateSection %d in %s", RateSection, __func__);
		rtw_warn_on(1);
		goto exit;
	}

	if (RateSection == CCK && pHalData->current_band_type != BAND_ON_2_4G)
		goto exit;

	PHY_SetTxPowerIndexByRateArray(pAdapter, RFPath, pHalData->current_channel_bw, Channel,
		rates_by_sections[RateSection].rates, rates_by_sections[RateSection].rate_num);

exit:
	return;
}

static bool
phy_GetChnlIndex(
	u8	Channel,
	u8	*ChannelIdx
)
{
	u8  i = 0;
	bool bIn24G = true;

	if (Channel <= 14) {
		bIn24G = true;
		*ChannelIdx = Channel - 1;
	} else {
		bIn24G = false;

		for (i = 0; i < CENTER_CH_5G_ALL_NUM; ++i) {
			if (center_ch_5g_all[i] == Channel) {
				*ChannelIdx = i;
				return bIn24G;
			}
		}
	}

	return bIn24G;
}

u8
PHY_GetTxPowerIndexBase(
	struct adapter *		pAdapter,
	enum rf_path		RFPath,
	u8				Rate,
	u8 ntx_idx,
	enum channel_width	BandWidth,
	u8				Channel,
	bool *bIn24G
)
{
	struct hal_com_data *		pHalData = GET_HAL_DATA(pAdapter);
	u8					txPower = 0;
	u8					chnlIdx = (Channel - 1);

	if (!HAL_IsLegalChannel(pAdapter, Channel)) {
		chnlIdx = 0;
		RTW_INFO("Illegal channel!!\n");
	}

	*bIn24G = phy_GetChnlIndex(Channel, &chnlIdx);

	if (*bIn24G) {
		if (IS_CCK_RATE(Rate)) {
			/* CCK-nTX */
			txPower = pHalData->Index24G_CCK_Base[RFPath][chnlIdx];
			txPower += pHalData->CCK_24G_Diff[RFPath][RF_1TX];
			if (ntx_idx >= RF_2TX)
				txPower += pHalData->CCK_24G_Diff[RFPath][RF_2TX];
			if (ntx_idx >= RF_3TX)
				txPower += pHalData->CCK_24G_Diff[RFPath][RF_3TX];
			if (ntx_idx >= RF_4TX)
				txPower += pHalData->CCK_24G_Diff[RFPath][RF_4TX];
			goto exit;
		}

		txPower = pHalData->Index24G_BW40_Base[RFPath][chnlIdx];

		/* OFDM-nTX */
		if ((MGN_6M <= Rate && Rate <= MGN_54M) && !IS_CCK_RATE(Rate)) {
			txPower += pHalData->OFDM_24G_Diff[RFPath][RF_1TX];
			if (ntx_idx >= RF_2TX)
				txPower += pHalData->OFDM_24G_Diff[RFPath][RF_2TX];
			if (ntx_idx >= RF_3TX)
				txPower += pHalData->OFDM_24G_Diff[RFPath][RF_3TX];
			if (ntx_idx >= RF_4TX)
				txPower += pHalData->OFDM_24G_Diff[RFPath][RF_4TX];
			goto exit;
		}

		/* BW20-nS */
		if (BandWidth == CHANNEL_WIDTH_20) {
			if ((MGN_MCS0 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT1SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW20_24G_Diff[RFPath][RF_1TX];
			if ((MGN_MCS8 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT2SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW20_24G_Diff[RFPath][RF_2TX];
			if ((MGN_MCS16 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT3SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW20_24G_Diff[RFPath][RF_3TX];
			if ((MGN_MCS24 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT4SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW20_24G_Diff[RFPath][RF_4TX];
			goto exit;
		}

		/* BW40-nS */
		if (BandWidth == CHANNEL_WIDTH_40) {
			if ((MGN_MCS0 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT1SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_1TX];
			if ((MGN_MCS8 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT2SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_2TX];
			if ((MGN_MCS16 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT3SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_3TX];
			if ((MGN_MCS24 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT4SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_4TX];
			goto exit;
		}

		/* Willis suggest adopt BW 40M power index while in BW 80 mode */
		if (BandWidth == CHANNEL_WIDTH_80) {
			if ((MGN_MCS0 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT1SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_1TX];
			if ((MGN_MCS8 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT2SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_2TX];
			if ((MGN_MCS16 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT3SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_3TX];
			if ((MGN_MCS24 <= Rate && Rate <= MGN_MCS31) || (MGN_VHT4SS_MCS0 <= Rate && Rate <= MGN_VHT4SS_MCS9))
				txPower += pHalData->BW40_24G_Diff[RFPath][RF_4TX];
			goto exit;
		}
	}

exit:
	return txPower;
}

s8
PHY_GetTxPowerTrackingOffset(
	struct adapter *	pAdapter,
	enum rf_path	RFPath,
	u8			Rate
)
{
	struct hal_com_data *		pHalData = GET_HAL_DATA(pAdapter);
	struct PHY_DM_STRUCT			*pDM_Odm = &pHalData->odmpriv;
	s8	offset = 0;

	if (!pDM_Odm->rf_calibrate_info.txpowertrack_control)
		return offset;

	if ((Rate == MGN_1M) || (Rate == MGN_2M) || (Rate == MGN_5_5M) || (Rate == MGN_11M)) {
		offset = pDM_Odm->rf_calibrate_info.remnant_cck_swing_idx;
		/*RTW_INFO("+Remnant_CCKSwingIdx = 0x%x\n", RFPath, Rate, pRFCalibrateInfo->Remnant_CCKSwingIdx);*/
	} else {
		offset = pDM_Odm->rf_calibrate_info.remnant_ofdm_swing_idx[RFPath];
		/*RTW_INFO("+Remanant_OFDMSwingIdx[RFPath %u][Rate 0x%x] = 0x%x\n", RFPath, Rate, pRFCalibrateInfo->Remnant_OFDMSwingIdx[RFPath]);	*/

	}

	return offset;
}

/*The same as MRateToHwRate in hal_com.c*/
u8
PHY_GetRateIndexOfTxPowerByRate(
	u8		Rate
)
{
	u8	index = 0;
	switch (Rate) {
	case MGN_1M:
		index = 0;
		break;
	case MGN_2M:
		index = 1;
		break;
	case MGN_5_5M:
		index = 2;
		break;
	case MGN_11M:
		index = 3;
		break;
	case MGN_6M:
		index = 4;
		break;
	case MGN_9M:
		index = 5;
		break;
	case MGN_12M:
		index = 6;
		break;
	case MGN_18M:
		index = 7;
		break;
	case MGN_24M:
		index = 8;
		break;
	case MGN_36M:
		index = 9;
		break;
	case MGN_48M:
		index = 10;
		break;
	case MGN_54M:
		index = 11;
		break;
	case MGN_MCS0:
		index = 12;
		break;
	case MGN_MCS1:
		index = 13;
		break;
	case MGN_MCS2:
		index = 14;
		break;
	case MGN_MCS3:
		index = 15;
		break;
	case MGN_MCS4:
		index = 16;
		break;
	case MGN_MCS5:
		index = 17;
		break;
	case MGN_MCS6:
		index = 18;
		break;
	case MGN_MCS7:
		index = 19;
		break;
	case MGN_MCS8:
		index = 20;
		break;
	case MGN_MCS9:
		index = 21;
		break;
	case MGN_MCS10:
		index = 22;
		break;
	case MGN_MCS11:
		index = 23;
		break;
	case MGN_MCS12:
		index = 24;
		break;
	case MGN_MCS13:
		index = 25;
		break;
	case MGN_MCS14:
		index = 26;
		break;
	case MGN_MCS15:
		index = 27;
		break;
	case MGN_MCS16:
		index = 28;
		break;
	case MGN_MCS17:
		index = 29;
		break;
	case MGN_MCS18:
		index = 30;
		break;
	case MGN_MCS19:
		index = 31;
		break;
	case MGN_MCS20:
		index = 32;
		break;
	case MGN_MCS21:
		index = 33;
		break;
	case MGN_MCS22:
		index = 34;
		break;
	case MGN_MCS23:
		index = 35;
		break;
	case MGN_MCS24:
		index = 36;
		break;
	case MGN_MCS25:
		index = 37;
		break;
	case MGN_MCS26:
		index = 38;
		break;
	case MGN_MCS27:
		index = 39;
		break;
	case MGN_MCS28:
		index = 40;
		break;
	case MGN_MCS29:
		index = 41;
		break;
	case MGN_MCS30:
		index = 42;
		break;
	case MGN_MCS31:
		index = 43;
		break;
	case MGN_VHT1SS_MCS0:
		index = 44;
		break;
	case MGN_VHT1SS_MCS1:
		index = 45;
		break;
	case MGN_VHT1SS_MCS2:
		index = 46;
		break;
	case MGN_VHT1SS_MCS3:
		index = 47;
		break;
	case MGN_VHT1SS_MCS4:
		index = 48;
		break;
	case MGN_VHT1SS_MCS5:
		index = 49;
		break;
	case MGN_VHT1SS_MCS6:
		index = 50;
		break;
	case MGN_VHT1SS_MCS7:
		index = 51;
		break;
	case MGN_VHT1SS_MCS8:
		index = 52;
		break;
	case MGN_VHT1SS_MCS9:
		index = 53;
		break;
	case MGN_VHT2SS_MCS0:
		index = 54;
		break;
	case MGN_VHT2SS_MCS1:
		index = 55;
		break;
	case MGN_VHT2SS_MCS2:
		index = 56;
		break;
	case MGN_VHT2SS_MCS3:
		index = 57;
		break;
	case MGN_VHT2SS_MCS4:
		index = 58;
		break;
	case MGN_VHT2SS_MCS5:
		index = 59;
		break;
	case MGN_VHT2SS_MCS6:
		index = 60;
		break;
	case MGN_VHT2SS_MCS7:
		index = 61;
		break;
	case MGN_VHT2SS_MCS8:
		index = 62;
		break;
	case MGN_VHT2SS_MCS9:
		index = 63;
		break;
	case MGN_VHT3SS_MCS0:
		index = 64;
		break;
	case MGN_VHT3SS_MCS1:
		index = 65;
		break;
	case MGN_VHT3SS_MCS2:
		index = 66;
		break;
	case MGN_VHT3SS_MCS3:
		index = 67;
		break;
	case MGN_VHT3SS_MCS4:
		index = 68;
		break;
	case MGN_VHT3SS_MCS5:
		index = 69;
		break;
	case MGN_VHT3SS_MCS6:
		index = 70;
		break;
	case MGN_VHT3SS_MCS7:
		index = 71;
		break;
	case MGN_VHT3SS_MCS8:
		index = 72;
		break;
	case MGN_VHT3SS_MCS9:
		index = 73;
		break;
	case MGN_VHT4SS_MCS0:
		index = 74;
		break;
	case MGN_VHT4SS_MCS1:
		index = 75;
		break;
	case MGN_VHT4SS_MCS2:
		index = 76;
		break;
	case MGN_VHT4SS_MCS3:
		index = 77;
		break;
	case MGN_VHT4SS_MCS4:
		index = 78;
		break;
	case MGN_VHT4SS_MCS5:
		index = 79;
		break;
	case MGN_VHT4SS_MCS6:
		index = 80;
		break;
	case MGN_VHT4SS_MCS7:
		index = 81;
		break;
	case MGN_VHT4SS_MCS8:
		index = 82;
		break;
	case MGN_VHT4SS_MCS9:
		index = 83;
		break;
	default:
		RTW_INFO("Invalid rate 0x%x in %s\n", Rate, __func__);
		break;
	};

	return index;
}

s8
_PHY_GetTxPowerByRate(
	struct adapter *	pAdapter,
	u8			Band,
	enum rf_path	RFPath,
	u8			Rate
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(pAdapter);
	s8 value = 0;
	u8 rateIndex = PHY_GetRateIndexOfTxPowerByRate(Rate);

	if (Band != BAND_ON_2_4G && Band != BAND_ON_5G) {
		RTW_INFO("Invalid band %d in %s\n", Band, __func__);
		goto exit;
	}
	if (RFPath > RF_PATH_D) {
		RTW_INFO("Invalid RfPath %d in %s\n", RFPath, __func__);
		goto exit;
	}
	if (rateIndex >= TX_PWR_BY_RATE_NUM_RATE) {
		RTW_INFO("Invalid RateIndex %d in %s\n", rateIndex, __func__);
		goto exit;
	}

	value = pHalData->TxPwrByRateOffset[Band][RFPath][rateIndex];

exit:
	return value;
}


s8
PHY_GetTxPowerByRate(
	struct adapter *	pAdapter,
	u8			Band,
	enum rf_path	RFPath,
	u8			Rate
)
{
	if (!phy_is_tx_power_by_rate_needed(pAdapter))
		return 0;

	return _PHY_GetTxPowerByRate(pAdapter, Band, RFPath, Rate);
}

void
PHY_SetTxPowerByRate(
	struct adapter *	pAdapter,
	u8			Band,
	enum rf_path	RFPath,
	u8			Rate,
	s8			Value
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(pAdapter);
	u8	rateIndex = PHY_GetRateIndexOfTxPowerByRate(Rate);

	if (Band != BAND_ON_2_4G && Band != BAND_ON_5G) {
		RTW_INFO("Invalid band %d in %s\n", Band, __func__);
		return;
	}
	if (RFPath > RF_PATH_D) {
		RTW_INFO("Invalid RfPath %d in %s\n", RFPath, __func__);
		return;
	}
	if (rateIndex >= TX_PWR_BY_RATE_NUM_RATE) {
		RTW_INFO("Invalid RateIndex %d in %s\n", rateIndex, __func__);
		return;
	}

	pHalData->TxPwrByRateOffset[Band][RFPath][rateIndex] = Value;
}

void
phy_set_tx_power_level_by_path(
	struct adapter *	Adapter,
	u8			channel,
	u8			path
)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	bool bIsIn24G = (pHalData->current_band_type == BAND_ON_2_4G);

	/* if ( pMgntInfo->RegNByteAccess == 0 ) */
	{
		if (bIsIn24G)
			phy_set_tx_power_index_by_rate_section(Adapter, path, channel, CCK);

		phy_set_tx_power_index_by_rate_section(Adapter, path, channel, OFDM);
		phy_set_tx_power_index_by_rate_section(Adapter, path, channel, HT_MCS0_MCS7);

		if (pHalData->NumTotalRFPath >= 2)
			phy_set_tx_power_index_by_rate_section(Adapter, path, channel, HT_MCS8_MCS15);
	}
}

void
PHY_SetTxPowerIndexByRateArray(
	struct adapter *			pAdapter,
	enum rf_path			RFPath,
	enum channel_width	BandWidth,
	u8					Channel,
	u8					*Rates,
	u8					RateArraySize
)
{
	u32	powerIndex = 0;
	int	i = 0;

	for (i = 0; i < RateArraySize; ++i) {
		powerIndex = phy_get_tx_power_index(pAdapter, RFPath, Rates[i], BandWidth, Channel);
		PHY_SetTxPowerIndex(pAdapter, powerIndex, RFPath, Rates[i]);
	}
}

#ifdef CONFIG_TXPWR_LIMIT
const char *const _txpwr_lmt_rs_str[] = {
	"CCK",
	"OFDM",
	"HT",
	"VHT",
	"UNKNOWN",
};

static s8
phy_GetChannelIndexOfTxPowerLimit(
	u8			Band,
	u8			Channel
)
{
	s8	channelIndex = -1;
	u8	i = 0;

	if (Band == BAND_ON_2_4G)
		channelIndex = Channel - 1;
	else if (Band == BAND_ON_5G) {
		for (i = 0; i < CENTER_CH_5G_ALL_NUM; ++i) {
			if (center_ch_5g_all[i] == Channel)
				channelIndex = i;
		}
	} else
		RTW_PRINT("Invalid Band %d in %s\n", Band, __func__);

	if (channelIndex == -1)
		RTW_PRINT("Invalid Channel %d of Band %d in %s\n", Channel, Band, __func__);

	return channelIndex;
}

/*
* return txpwr limit absolute value
* MAX_POWER_INDEX is returned when NO limit
*/
s8 phy_get_txpwr_lmt_abs(
	struct adapter *			Adapter,
	const char			*regd_name,
	BAND_TYPE			Band,
	enum channel_width		bw,
	u8 tlrs,
	u8 ntx_idx,
	u8 cch,
	u8 lock
)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(Adapter);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(Adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(Adapter);
	struct txpwr_lmt_ent *ent = NULL;
	unsigned long irqL;
	struct list_head *cur, *head;
	s8 ch_idx;
	u8 is_ww_regd = 0;
	s8 lmt = MAX_POWER_INDEX;

	if ((Adapter->registrypriv.RegEnableTxPowerLimit == 2 && hal_data->EEPROMRegulatory != 1) ||
		Adapter->registrypriv.RegEnableTxPowerLimit == 0)
		goto exit;

	if (Band != BAND_ON_2_4G && Band != BAND_ON_5G) {
		RTW_ERR("%s invalid band:%u\n", __func__, Band);
		rtw_warn_on(1);
		goto exit;
	}

	if (Band == BAND_ON_5G  && tlrs == TXPWR_LMT_RS_CCK) {
		RTW_ERR("5G has no CCK\n");
		goto exit;
	}

	if (lock)
		_enter_critical_mutex(&rfctl->txpwr_lmt_mutex, &irqL);

	if (!regd_name) /* no regd_name specified, use currnet */
		regd_name = rfctl->regd_name;

	if (rfctl->txpwr_regd_num == 0
		|| strcmp(regd_name, regd_str(TXPWR_LMT_NONE)) == 0)
		goto release_lock;

	if (strcmp(regd_name, regd_str(TXPWR_LMT_WW)) == 0)
		is_ww_regd = 1;

	if (!is_ww_regd) {
		ent = _rtw_txpwr_lmt_get_by_name(rfctl, regd_name);
		if (!ent)
			goto release_lock;
	}

	ch_idx = phy_GetChannelIndexOfTxPowerLimit(Band, cch);
	if (ch_idx == -1)
		goto release_lock;

	if (Band == BAND_ON_2_4G) {
		if (!is_ww_regd) {
			lmt = ent->lmt_2g[bw][tlrs][ch_idx][ntx_idx];
			if (lmt != -MAX_POWER_INDEX)
				goto release_lock;
		}

		/* search for min value for WW regd or WW limit */
		lmt = MAX_POWER_INDEX;
		head = &rfctl->txpwr_lmt_list;
		cur = get_next(head);
		while ((!rtw_end_of_queue_search(head, cur))) {
			ent = container_of(cur, struct txpwr_lmt_ent, list);
			cur = get_next(cur);
			if (ent->lmt_2g[bw][tlrs][ch_idx][ntx_idx] != -MAX_POWER_INDEX)
				lmt = rtw_min(lmt, ent->lmt_2g[bw][tlrs][ch_idx][ntx_idx]);
		}
	}
release_lock:
	if (lock)
		_exit_critical_mutex(&rfctl->txpwr_lmt_mutex, &irqL);

exit:
	return lmt;
}

/*
* return txpwr limit diff value
* MAX_POWER_INDEX is returned when NO limit
*/
inline s8 phy_get_txpwr_lmt(struct adapter *adapter
	, const char *regd_name
	, BAND_TYPE band, enum channel_width bw
	, u8 rfpath, u8 rs, u8 ntx_idx, u8 cch, u8 lock
)
{
	u8 tlrs;
	s8 lmt = MAX_POWER_INDEX;

	if (IS_CCK_RATE_SECTION(rs))
		tlrs = TXPWR_LMT_RS_CCK;
	else if (IS_OFDM_RATE_SECTION(rs))
		tlrs = TXPWR_LMT_RS_OFDM;
	else if (IS_HT_RATE_SECTION(rs))
		tlrs = TXPWR_LMT_RS_HT;
	else if (IS_VHT_RATE_SECTION(rs))
		tlrs = TXPWR_LMT_RS_VHT;
	else {
		RTW_ERR("%s invalid rs %u\n", __func__, rs);
		rtw_warn_on(1);
		goto exit;
	}

	lmt = phy_get_txpwr_lmt_abs(adapter, regd_name, band, bw, tlrs, ntx_idx, cch, lock);

	if (lmt != MAX_POWER_INDEX) {
		/* return diff value */
		lmt = lmt - PHY_GetTxPowerByRateBase(adapter, band, rfpath, rs);
	}

exit:
	return lmt;
}

/*
* May search for secondary channels for min limit
* return txpwr limit diff value
*/
s8
PHY_GetTxPowerLimit(struct adapter *adapter
	, const char *regd_name
	, BAND_TYPE band, enum channel_width bw
	, u8 rfpath, u8 rate, u8 ntx_idx, u8 cch)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	bool no_sc = false;
	s8 tlrs = -1, rs = -1;
	s8 lmt = MAX_POWER_INDEX;
	u8 tmp_cch = 0;
	u8 tmp_bw;
	u8 bw_bmp = 0;
	s8 min_lmt = MAX_POWER_INDEX;
	u8 final_bw = bw, final_cch = cch;
	unsigned long irqL;

	if (IS_CCK_RATE(rate)) {
		tlrs = TXPWR_LMT_RS_CCK;
		rs = CCK;
	} else if (IS_OFDM_RATE(rate)) {
		tlrs = TXPWR_LMT_RS_OFDM;
		rs = OFDM;
	} else if (IS_HT_RATE(rate)) {
		tlrs = TXPWR_LMT_RS_HT;
		rs = HT_1SS + (IS_HT1SS_RATE(rate) ? 0 : IS_HT2SS_RATE(rate) ? 1 : IS_HT3SS_RATE(rate) ? 2 : IS_HT4SS_RATE(rate) ? 3 : 0);
	} else if (IS_VHT_RATE(rate)) {
		tlrs = TXPWR_LMT_RS_VHT;
		rs = VHT_1SS + (IS_VHT1SS_RATE(rate) ? 0 : IS_VHT2SS_RATE(rate) ? 1 : IS_VHT3SS_RATE(rate) ? 2 : IS_VHT4SS_RATE(rate) ? 3 : 0);
	} else {
		RTW_ERR("%s invalid rate 0x%x\n", __func__, rate);
		rtw_warn_on(1);
		goto exit;
	}

	if (no_sc) {
		/* use the input center channel and bandwidth directly */
		tmp_cch = cch;
		bw_bmp = ch_width_to_bw_cap(bw);
	} else {
		/*
		* find the possible tx bandwidth bmp for this rate, and then will get center channel for each bandwidth
		* if no possible tx bandwidth bmp, select valid bandwidth up to current RF bandwidth into bmp
		*/
		if (tlrs == TXPWR_LMT_RS_CCK || tlrs == TXPWR_LMT_RS_OFDM)
			bw_bmp = BW_CAP_20M; /* CCK, OFDM only BW 20M */
		else if (tlrs == TXPWR_LMT_RS_HT) {
			bw_bmp = rtw_get_tx_bw_bmp_of_ht_rate(dvobj, rate, bw);
			if (bw_bmp == 0)
				bw_bmp = ch_width_to_bw_cap(bw > CHANNEL_WIDTH_40 ? CHANNEL_WIDTH_40 : bw);
		} else if (tlrs == TXPWR_LMT_RS_VHT) {
			bw_bmp = rtw_get_tx_bw_bmp_of_vht_rate(dvobj, rate, bw);
			if (bw_bmp == 0)
				bw_bmp = ch_width_to_bw_cap(bw > CHANNEL_WIDTH_160 ? CHANNEL_WIDTH_160 : bw);
		} else
			rtw_warn_on(1);
	}

	if (bw_bmp == 0)
		goto exit;

	_enter_critical_mutex(&rfctl->txpwr_lmt_mutex, &irqL);

	/* loop for each possible tx bandwidth to find minimum limit */
	for (tmp_bw = CHANNEL_WIDTH_20; tmp_bw <= bw; tmp_bw++) {
		if (!(ch_width_to_bw_cap(tmp_bw) & bw_bmp))
			continue;

		if (!no_sc) {
			if (tmp_bw == CHANNEL_WIDTH_20)
				tmp_cch = hal_data->cch_20;
			else if (tmp_bw == CHANNEL_WIDTH_40)
				tmp_cch = hal_data->cch_40;
			else if (tmp_bw == CHANNEL_WIDTH_80)
				tmp_cch = hal_data->cch_80;
			else {
				tmp_cch = 0;
				rtw_warn_on(1);
			}
		}

		lmt = phy_get_txpwr_lmt_abs(adapter, regd_name, band, tmp_bw, tlrs, ntx_idx, tmp_cch, 0);

		if (min_lmt >= lmt) {
			min_lmt = lmt;
			final_cch = tmp_cch;
			final_bw = tmp_bw;
		}

	}

	_exit_critical_mutex(&rfctl->txpwr_lmt_mutex, &irqL);

	if (min_lmt != MAX_POWER_INDEX) {
		/* return diff value */
		min_lmt = min_lmt - PHY_GetTxPowerByRateBase(adapter, band, rfpath, rs);
	}

exit:

	return min_lmt;
}

static void phy_txpwr_lmt_cck_ofdm_mt_chk(struct adapter *adapter)
{
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	struct txpwr_lmt_ent *ent;
	struct list_head *cur, *head;
	u8 channel, tlrs, ntx_idx;

	rfctl->txpwr_lmt_2g_cck_ofdm_state = 0;

	head = &rfctl->txpwr_lmt_list;
	cur = get_next(head);

	while ((!rtw_end_of_queue_search(head, cur))) {
		ent = container_of(cur, struct txpwr_lmt_ent, list);
		cur = get_next(cur);

		/* check 2G CCK, OFDM state*/
		for (tlrs = TXPWR_LMT_RS_CCK; tlrs <= TXPWR_LMT_RS_OFDM; tlrs++) {
			for (ntx_idx = RF_1TX; ntx_idx < MAX_TX_COUNT; ntx_idx++) {
				for (channel = 0; channel < CENTER_CH_2G_NUM; ++channel) {
					if (ent->lmt_2g[CHANNEL_WIDTH_20][tlrs][channel][ntx_idx] != MAX_POWER_INDEX) {
						if (tlrs == TXPWR_LMT_RS_CCK)
							rfctl->txpwr_lmt_2g_cck_ofdm_state |= TXPWR_LMT_HAS_CCK_1T << ntx_idx;
						else
							rfctl->txpwr_lmt_2g_cck_ofdm_state |= TXPWR_LMT_HAS_OFDM_1T << ntx_idx;
						break;
					}
				}
			}
		}

		/* if 2G OFDM multi-TX is not defined, reference HT20 */
		for (channel = 0; channel < CENTER_CH_2G_NUM; ++channel) {
			for (ntx_idx = RF_2TX; ntx_idx < MAX_TX_COUNT; ntx_idx++) {
				if (rfctl->txpwr_lmt_2g_cck_ofdm_state & (TXPWR_LMT_HAS_OFDM_1T << ntx_idx))
					continue;
				ent->lmt_2g[CHANNEL_WIDTH_20][TXPWR_LMT_RS_OFDM][channel][ntx_idx] =
					ent->lmt_2g[CHANNEL_WIDTH_20][TXPWR_LMT_RS_HT][channel][ntx_idx];
			}
		}
	}
}

static void phy_txpwr_lmt_post_hdl(struct adapter *adapter)
{
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	unsigned long irqL;

	_enter_critical_mutex(&rfctl->txpwr_lmt_mutex, &irqL);

	phy_txpwr_lmt_cck_ofdm_mt_chk(adapter);

	_exit_critical_mutex(&rfctl->txpwr_lmt_mutex, &irqL);
}

bool
GetS1ByteIntegerFromStringInDecimal(
		char	*str,
	s8		*val
)
{
	u8 negative = 0;
	u16 i = 0;

	*val = 0;

	while (str[i] != '\0') {
		if (i == 0 && (str[i] == '+' || str[i] == '-')) {
			if (str[i] == '-')
				negative = 1;
		} else if (str[i] >= '0' && str[i] <= '9') {
			*val *= 10;
			*val += (str[i] - '0');
		} else
			return false;
		++i;
	}

	if (negative)
		*val = -*val;

	return true;
}
#endif /* CONFIG_TXPWR_LIMIT */

/*
* phy_set_tx_power_limit - Parsing TX power limit from phydm array, called by odm_ConfigBB_TXPWR_LMT_XXX in phydm
*/
void
phy_set_tx_power_limit(
	struct PHY_DM_STRUCT		*pDM_Odm,
	u8				*Regulation,
	u8				*Band,
	u8				*Bandwidth,
	u8				*RateSection,
	u8				*ntx,
	u8				*Channel,
	u8				*PowerLimit
)
{
#ifdef CONFIG_TXPWR_LIMIT
	struct adapter * Adapter = pDM_Odm->adapter;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	u8 band = 0, bandwidth = 0, tlrs = 0, channel;
	u8 ntx_idx;
	s8 powerLimit = 0, prevPowerLimit, channelIndex;

	if (!GetU1ByteIntegerFromStringInDecimal((char *)Channel, &channel) ||
	    !GetS1ByteIntegerFromStringInDecimal((char *)PowerLimit, &powerLimit)) {
		RTW_PRINT("Illegal index of power limit table [ch %s][val %s]\n", Channel, PowerLimit);
		return;
	}

	if (powerLimit < -MAX_POWER_INDEX || powerLimit > MAX_POWER_INDEX)
		RTW_PRINT("Illegal power limit value [ch %s][val %s]\n", Channel, PowerLimit);

	powerLimit = powerLimit > MAX_POWER_INDEX ? MAX_POWER_INDEX : powerLimit;
	powerLimit = powerLimit < -MAX_POWER_INDEX ? -MAX_POWER_INDEX + 1 : powerLimit;

	if (eqNByte(RateSection, (u8 *)("CCK"), 3))
		tlrs = TXPWR_LMT_RS_CCK;
	else if (eqNByte(RateSection, (u8 *)("OFDM"), 4))
		tlrs = TXPWR_LMT_RS_OFDM;
	else if (eqNByte(RateSection, (u8 *)("HT"), 2))
		tlrs = TXPWR_LMT_RS_HT;
	else if (eqNByte(RateSection, (u8 *)("VHT"), 3))
		tlrs = TXPWR_LMT_RS_VHT;
	else {
		RTW_PRINT("Wrong rate section:%s\n", RateSection);
		return;
	}

	if (eqNByte(ntx, (u8 *)("1T"), 2))
		ntx_idx = RF_1TX;
	else if (eqNByte(ntx, (u8 *)("2T"), 2))
		ntx_idx = RF_2TX;
	else if (eqNByte(ntx, (u8 *)("3T"), 2))
		ntx_idx = RF_3TX;
	else if (eqNByte(ntx, (u8 *)("4T"), 2))
		ntx_idx = RF_4TX;
	else {
		RTW_PRINT("Wrong tx num:%s\n", ntx);
		return;
	}

	if (eqNByte(Bandwidth, (u8 *)("20M"), 3))
		bandwidth = CHANNEL_WIDTH_20;
	else if (eqNByte(Bandwidth, (u8 *)("40M"), 3))
		bandwidth = CHANNEL_WIDTH_40;
	else if (eqNByte(Bandwidth, (u8 *)("80M"), 3))
		bandwidth = CHANNEL_WIDTH_80;
	else if (eqNByte(Bandwidth, (u8 *)("160M"), 4))
		bandwidth = CHANNEL_WIDTH_160;
	else {
		RTW_PRINT("unknown bandwidth: %s\n", Bandwidth);
		return;
	}

	if (eqNByte(Band, (u8 *)("2.4G"), 4)) {
		band = BAND_ON_2_4G;
		channelIndex = phy_GetChannelIndexOfTxPowerLimit(BAND_ON_2_4G, channel);

		if (channelIndex == -1) {
			RTW_PRINT("unsupported channel: %d at 2.4G\n", channel);
			return;
		}

		if (bandwidth >= MAX_2_4G_BANDWIDTH_NUM) {
			RTW_PRINT("unsupported bandwidth: %s at 2.4G\n", Bandwidth);
			return;
		}

		rtw_txpwr_lmt_add(adapter_to_rfctl(Adapter), Regulation, band, bandwidth, tlrs, ntx_idx, channelIndex, powerLimit);
	} else {
		RTW_PRINT("unknown/unsupported band:%s\n", Band);
		return;
	}
#endif
}

u8
phy_get_tx_power_index(
	struct adapter *			pAdapter,
	enum rf_path			RFPath,
	u8					Rate,
	enum channel_width	BandWidth,
	u8					Channel
)
{
	return rtw_hal_get_tx_power_index(pAdapter, RFPath, Rate, BandWidth, Channel, NULL);
}

void
PHY_SetTxPowerIndex(
	struct adapter *		pAdapter,
	u32				PowerIndex,
	enum rf_path		RFPath,
	u8				Rate
)
{
	PHY_SetTxPowerIndex_8723D(pAdapter, PowerIndex, RFPath, Rate);
}

void dump_tx_power_idx_title(void *sel, struct adapter *adapter)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 bw = hal_data->current_channel_bw;

	RTW_PRINT_SEL(sel, "%s", ch_width_str(bw));
	if (bw >= CHANNEL_WIDTH_80)
		_RTW_PRINT_SEL(sel, ", cch80:%u", hal_data->cch_80);
	if (bw >= CHANNEL_WIDTH_40)
		_RTW_PRINT_SEL(sel, ", cch40:%u", hal_data->cch_40);
	_RTW_PRINT_SEL(sel, ", cch20:%u\n", hal_data->cch_20);

	RTW_PRINT_SEL(sel, "%-4s %-9s %2s %-3s %-4s %-3s %-4s %-4s %-3s %-5s\n"
		, "path", "rate", "", "pwr", "base", "", "(byr", "lmt)", "tpt", "ebias");
}

void dump_tx_power_idx_by_path_rs(void *sel, struct adapter *adapter, u8 rfpath, u8 rs)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	u8 power_idx;
	struct txpwr_idx_comp tic;
	u8 tx_num, i;
	u8 band = hal_data->current_band_type;
	u8 cch = hal_data->current_channel;
	u8 bw = hal_data->current_channel_bw;

	if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, rfpath))
		return;

	if (rs >= RATE_SECTION_NUM)
		return;

	tx_num = rate_section_to_tx_num(rs);
	if (tx_num >= hal_spec->tx_nss_num || tx_num >= hal_spec->max_tx_cnt)
		return;

	if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
		return;

	if (IS_VHT_RATE_SECTION(rs))
		return;

	for (i = 0; i < rates_by_sections[rs].rate_num; i++) {
		power_idx = rtw_hal_get_tx_power_index(adapter, rfpath, rates_by_sections[rs].rates[i], bw, cch, &tic);

		RTW_PRINT_SEL(sel, "%4c %9s %uT %3u %4u %3d (%3d %3d) %3d %5d\n"
			, rf_path_char(rfpath), MGN_RATE_STR(rates_by_sections[rs].rates[i]), tic.ntx_idx + 1
			, power_idx, tic.base, (tic.by_rate > tic.limit ? tic.limit : tic.by_rate), tic.by_rate, tic.limit, tic.tpt, tic.ebias);
	}
}

void dump_tx_power_idx(void *sel, struct adapter *adapter)
{
	u8 rfpath, rs;

	dump_tx_power_idx_title(sel, adapter);
	for (rfpath = RF_PATH_A; rfpath < RF_PATH_MAX; rfpath++)
		for (rs = CCK; rs < RATE_SECTION_NUM; rs++)
			dump_tx_power_idx_by_path_rs(sel, adapter, rfpath, rs);
}

bool phy_is_tx_power_limit_needed(struct adapter *adapter)
{
#ifdef CONFIG_TXPWR_LIMIT
	if (regsty->RegEnableTxPowerLimit == 1
		|| (regsty->RegEnableTxPowerLimit == 2 && hal_data->EEPROMRegulatory == 1))
		return true;
#endif

	return false;
}

bool phy_is_tx_power_by_rate_needed(struct adapter *adapter)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct registry_priv *regsty = dvobj_to_regsty(adapter_to_dvobj(adapter));

	if (regsty->RegEnableTxPowerByRate == 1
		|| (regsty->RegEnableTxPowerByRate == 2 && hal_data->EEPROMRegulatory != 2))
		return true;
	return false;
}

int phy_load_tx_power_by_rate(struct adapter *adapter, u8 chk_file)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	int ret = _FAIL;

	hal_data->txpwr_by_rate_loaded = 0;
	PHY_InitTxPowerByRate(adapter);

	/* tx power limit is based on tx power by rate */
	hal_data->txpwr_limit_loaded = 0;

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	if (chk_file
		&& phy_ConfigBBWithPgParaFile(adapter, PHY_FILE_PHY_REG_PG) == _SUCCESS
	) {
		hal_data->txpwr_by_rate_from_file = 1;
		goto post_hdl;
	}
#endif

#ifdef CONFIG_EMBEDDED_FWIMG
	if (HAL_STATUS_SUCCESS == odm_config_bb_with_header_file(&hal_data->odmpriv, CONFIG_BB_PHY_REG_PG)) {
		RTW_INFO("default power by rate loaded\n");
		hal_data->txpwr_by_rate_from_file = 0;
		goto post_hdl;
	}
#endif

	RTW_ERR("%s():Read Tx power by rate fail\n", __func__);
	goto exit;

post_hdl:
	if (hal_data->odmpriv.phy_reg_pg_value_type != PHY_REG_PG_EXACT_VALUE) {
		rtw_warn_on(1);
		goto exit;
	}

	PHY_TxPowerByRateConfiguration(adapter);
	hal_data->txpwr_by_rate_loaded = 1;

	ret = _SUCCESS;

exit:
	return ret;
}

#ifdef CONFIG_TXPWR_LIMIT
int phy_load_tx_power_limit(struct adapter *adapter, u8 chk_file)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct registry_priv *regsty = dvobj_to_regsty(adapter_to_dvobj(adapter));
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	int ret = _FAIL;

	hal_data->txpwr_limit_loaded = 0;
	rtw_regd_exc_list_free(rfctl);
	rtw_txpwr_lmt_list_free(rfctl);

	if (!hal_data->txpwr_by_rate_loaded && !regsty->target_tx_pwr_valid) {
		RTW_ERR("%s():Read Tx power limit before target tx power is specify\n", __func__);
		goto exit;
	}

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	if (chk_file
		&& PHY_ConfigRFWithPowerLimitTableParaFile(adapter, PHY_FILE_TXPWR_LMT) == _SUCCESS
	) {
		hal_data->txpwr_limit_from_file = 1;
		goto post_hdl;
	}
#endif

#ifdef CONFIG_EMBEDDED_FWIMG
	if (odm_config_rf_with_header_file(&hal_data->odmpriv, CONFIG_RF_TXPWR_LMT, RF_PATH_A) == HAL_STATUS_SUCCESS) {
		RTW_INFO("default power limit loaded\n");
		hal_data->txpwr_limit_from_file = 0;
		goto post_hdl;
	}
#endif

	RTW_ERR("%s():Read Tx power limit fail\n", __func__);
	goto exit;

post_hdl:
	phy_txpwr_lmt_post_hdl(adapter);
	rtw_txpwr_init_regd(rfctl);
	hal_data->txpwr_limit_loaded = 1;
	ret = _SUCCESS;

exit:
	return ret;
}
#endif /* CONFIG_TXPWR_LIMIT */

void phy_load_tx_power_ext_info(struct adapter *adapter, u8 chk_file)
{
	struct registry_priv *regsty = adapter_to_regsty(adapter);

	/* check registy target tx power */
	regsty->target_tx_pwr_valid = rtw_regsty_chk_target_tx_power_valid(adapter);

	/* power by rate and limit */
	if (phy_is_tx_power_by_rate_needed(adapter) ||
	    (phy_is_tx_power_limit_needed(adapter) &&
	    !regsty->target_tx_pwr_valid))
		phy_load_tx_power_by_rate(adapter, chk_file);

#ifdef CONFIG_TXPWR_LIMIT
	if (phy_is_tx_power_limit_needed(adapter))
		phy_load_tx_power_limit(adapter, chk_file);
#endif
}

inline void phy_reload_tx_power_ext_info(struct adapter *adapter)
{
	phy_load_tx_power_ext_info(adapter, 1);
}

inline void phy_reload_default_tx_power_ext_info(struct adapter *adapter)
{
	phy_load_tx_power_ext_info(adapter, 0);
}

void dump_tx_power_ext_info(void *sel, struct adapter *adapter)
{
	struct registry_priv *regsty = adapter_to_regsty(adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);

	if (regsty->target_tx_pwr_valid)
		RTW_PRINT_SEL(sel, "target_tx_power: from registry\n");
	else if (phy_is_tx_power_by_rate_needed(adapter))
		RTW_PRINT_SEL(sel, "target_tx_power: from power by rate\n"); 
	else
		RTW_PRINT_SEL(sel, "target_tx_power: unavailable\n");

	RTW_PRINT_SEL(sel, "tx_power_by_rate: %s, %s, %s\n"
		, phy_is_tx_power_by_rate_needed(adapter) ? "enabled" : "disabled"
		, hal_data->txpwr_by_rate_loaded ? "loaded" : "unloaded"
		, hal_data->txpwr_by_rate_from_file ? "file" : "default"
	);

	RTW_PRINT_SEL(sel, "tx_power_limit: %s, %s, %s\n"
		, phy_is_tx_power_limit_needed(adapter) ? "enabled" : "disabled"
		, hal_data->txpwr_limit_loaded ? "loaded" : "unloaded"
		, hal_data->txpwr_limit_from_file ? "file" : "default"
	);
}

void dump_target_tx_power(void *sel, struct adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct registry_priv *regsty = adapter_to_regsty(adapter);
	int path, tx_num, band, rs;
	u8 target;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++) {
		if (!hal_is_band_support(adapter, band))
			continue;

		for (path = 0; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				break;

			RTW_PRINT_SEL(sel, "[%s][%c]%s\n", band_str(band), rf_path_char(path)
				, (!regsty->target_tx_pwr_valid && hal_data->txpwr_by_rate_undefined_band_path[band][path]) ? "(dup)" : "");

			for (rs = 0; rs < RATE_SECTION_NUM; rs++) {
				tx_num = rate_section_to_tx_num(rs);
				if (tx_num >= hal_spec->tx_nss_num)
					continue;

				if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
					continue;

				if (IS_VHT_RATE_SECTION(rs))
					continue;

				target = PHY_GetTxPowerByRateBase(adapter, band, path, rs);

				if (target % 2)
					_RTW_PRINT_SEL(sel, "%7s: %2d.5\n", rate_section_str(rs), target / 2);
				else
					_RTW_PRINT_SEL(sel, "%7s: %4d\n", rate_section_str(rs), target / 2);
			}
		}
	}
	return;
}

void dump_tx_power_by_rate(void *sel, struct adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	int path, tx_num, band, n, rs;
	u8 rate_num, max_rate_num, base;
	s8 by_rate_offset;

	for (band = BAND_ON_2_4G; band <= BAND_ON_5G; band++) {
		if (!hal_is_band_support(adapter, band))
			continue;

		for (path = 0; path < RF_PATH_MAX; path++) {
			if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
				break;

			RTW_PRINT_SEL(sel, "[%s][%c]%s\n", band_str(band), rf_path_char(path)
				, hal_data->txpwr_by_rate_undefined_band_path[band][path] ? "(dup)" : "");

			for (rs = 0; rs < RATE_SECTION_NUM; rs++) {
				tx_num = rate_section_to_tx_num(rs);
				if (tx_num >= hal_spec->tx_nss_num)
					continue;

				if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
					continue;

				if (IS_VHT_RATE_SECTION(rs))
					continue;

				max_rate_num = 8;
				rate_num = rate_section_rate_num(rs);
				base = PHY_GetTxPowerByRateBase(adapter, band, path, rs);

				RTW_PRINT_SEL(sel, "%7s: ", rate_section_str(rs));

				/* dump power by rate in db */
				for (n = rate_num - 1; n >= 0; n--) {
					by_rate_offset = PHY_GetTxPowerByRate(adapter, band, path, rates_by_sections[rs].rates[n]);

					if ((base + by_rate_offset) % 2)
						_RTW_PRINT_SEL(sel, "%2d.5 ", (base + by_rate_offset) / 2);
					else
						_RTW_PRINT_SEL(sel, "%4d ", (base + by_rate_offset) / 2);
				}
				for (n = 0; n < max_rate_num - rate_num; n++)
					_RTW_PRINT_SEL(sel, "%4s ", "");

				_RTW_PRINT_SEL(sel, "|");

				/* dump power by rate in offset */
				for (n = rate_num - 1; n >= 0; n--) {
					by_rate_offset = PHY_GetTxPowerByRate(adapter, band, path, rates_by_sections[rs].rates[n]);
					_RTW_PRINT_SEL(sel, "%3d ", by_rate_offset);
				}
				RTW_PRINT_SEL(sel, "\n");

			}
		}
	}
}

/*
 * phy file path is stored in global char array rtw_phy_para_file_path
 * need to care about racing
 */
int rtw_get_phy_file_path(struct adapter *adapter, const char *file_name)
{
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	int len = 0;

	if (file_name) {
		len += snprintf(rtw_phy_para_file_path, PATH_LENGTH_MAX, "%s", rtw_phy_file_path);
		#if defined(REALTEK_CONFIG_PATH_WITH_IC_NAME_FOLDER)
		len += snprintf(rtw_phy_para_file_path + len, PATH_LENGTH_MAX - len, "%s/", hal_spec->ic_name);
		#endif
		len += snprintf(rtw_phy_para_file_path + len, PATH_LENGTH_MAX - len, "%s", file_name);

		return true;
	}
#endif
	return false;
}

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
int
phy_ConfigMACWithParaFile(
	struct adapter *	Adapter,
	char		*pFileName
)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;
	char	*szLine, *ptmp;
	u32	u4bRegOffset, u4bRegValue, u4bMove;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_MAC_PARA_FILE))
		return rtStatus;

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if ((pHalData->mac_reg_len == 0) && (!pHalData->mac_reg)) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pHalData->mac_reg = vzalloc(rlen);
				if (pHalData->mac_reg) {
					memcpy(pHalData->mac_reg, pHalData->para_file_buf, rlen);
					pHalData->mac_reg_len = rlen;
				} else
					RTW_INFO("%s mac_reg alloc fail !\n", __func__);
			}
		}
	} else {
		if ((pHalData->mac_reg_len != 0) && (pHalData->mac_reg)) {
			memcpy(pHalData->para_file_buf, pHalData->mac_reg, pHalData->mac_reg_len);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS) {
		ptmp = pHalData->para_file_buf;
		for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
			if (!IsCommentString(szLine)) {
				/* Get 1st hex value as register offset */
				if (GetHexValueFromString(szLine, &u4bRegOffset, &u4bMove)) {
					if (u4bRegOffset == 0xffff) {
						/* Ending. */
						break;
					}

					/* Get 2nd hex value as register value. */
					szLine += u4bMove;
					if (GetHexValueFromString(szLine, &u4bRegValue, &u4bMove))
						rtw_write8(Adapter, u4bRegOffset, (u8)u4bRegValue);
				}
			}
		}
	} else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);

	return rtStatus;
}

int
phy_ConfigBBWithParaFile(
	struct adapter *	Adapter,
	char		*pFileName,
	u32			ConfigType
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;
	char	*szLine, *ptmp;
	u32	u4bRegOffset, u4bRegValue, u4bMove;
	char	*pBuf = NULL;
	u32	*pBufLen = NULL;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_BB_PARA_FILE))
		return rtStatus;

	switch (ConfigType) {
	case CONFIG_BB_PHY_REG:
		pBuf = pHalData->bb_phy_reg;
		pBufLen = &pHalData->bb_phy_reg_len;
		break;
	case CONFIG_BB_AGC_TAB:
		pBuf = pHalData->bb_agc_tab;
		pBufLen = &pHalData->bb_agc_tab_len;
		break;
	default:
		RTW_INFO("Unknown ConfigType!! %d\r\n", ConfigType);
		break;
	}

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if ((pBufLen) && (*pBufLen == 0) && (!pBuf)) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pBuf = vzalloc(rlen);
				if (pBuf) {
					memcpy(pBuf, pHalData->para_file_buf, rlen);
					*pBufLen = rlen;

					switch (ConfigType) {
					case CONFIG_BB_PHY_REG:
						pHalData->bb_phy_reg = pBuf;
						break;
					case CONFIG_BB_AGC_TAB:
						pHalData->bb_agc_tab = pBuf;
						break;
					}
				} else
					RTW_INFO("%s(): ConfigType %d  alloc fail !\n", __func__, ConfigType);
			}
		}
	} else {
		if ((pBufLen) && (*pBufLen != 0) && (pBuf)) {
			memcpy(pHalData->para_file_buf, pBuf, *pBufLen);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS) {
		ptmp = pHalData->para_file_buf;
		for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
			if (!IsCommentString(szLine)) {
				/* Get 1st hex value as register offset. */
				if (GetHexValueFromString(szLine, &u4bRegOffset, &u4bMove)) {
					if (u4bRegOffset == 0xffff) {
						/* Ending. */
						break;
					} else if (u4bRegOffset == 0xfe || u4bRegOffset == 0xffe) {
						rtw_msleep_os(50);
					} else if (u4bRegOffset == 0xfd)
						rtw_mdelay_os(5);
					else if (u4bRegOffset == 0xfc)
						rtw_mdelay_os(1);
					else if (u4bRegOffset == 0xfb)
						rtw_udelay_os(50);
					else if (u4bRegOffset == 0xfa)
						rtw_udelay_os(5);
					else if (u4bRegOffset == 0xf9)
						rtw_udelay_os(1);

					/* Get 2nd hex value as register value. */
					szLine += u4bMove;
					if (GetHexValueFromString(szLine, &u4bRegValue, &u4bMove)) {
						/* RTW_INFO("[BB-ADDR]%03lX=%08lX\n", u4bRegOffset, u4bRegValue); */
						phy_set_bb_reg(Adapter, u4bRegOffset, bMaskDWord, u4bRegValue);

						if (u4bRegOffset == 0xa24)
							pHalData->odmpriv.rf_calibrate_info.rega24 = u4bRegValue;

						/* Add 1us delay between BB/RF register setting. */
						rtw_udelay_os(1);
					}
				}
			}
		}
	} else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);

	return rtStatus;
}

static void
phy_DecryptBBPgParaFile(
	struct adapter *		Adapter,
	char			*buffer
)
{
	u32	i = 0, j = 0;
	u8	map[95] = {0};
	u8	currentChar;
	char	*BufOfLines, *ptmp;

	/* RTW_INFO("=====>phy_DecryptBBPgParaFile()\n"); */
	/* 32 the ascii code of the first visable char, 126 the last one */
	for (i = 0; i < 95; ++i)
		map[i] = (u8)(94 - i);

	ptmp = buffer;
	i = 0;
	for (BufOfLines = GetLineFromBuffer(ptmp); BufOfLines; BufOfLines = GetLineFromBuffer(ptmp)) {
		/* RTW_INFO("Encrypted Line: %s\n", BufOfLines); */

		for (j = 0; j < strlen(BufOfLines); ++j) {
			currentChar = BufOfLines[j];

			if (currentChar == '\0')
				break;

			currentChar -= (u8)((((i + j) * 3) % 128));

			BufOfLines[j] = map[currentChar - 32] + 32;
		}
		/* RTW_INFO("Decrypted Line: %s\n", BufOfLines ); */
		if (strlen(BufOfLines) != 0)
			i++;
		BufOfLines[strlen(BufOfLines)] = '\n';
	}
}

static int
phy_ParseBBPgParaFile(
	struct adapter *		Adapter,
	char			*buffer
)
{
	int	rtStatus = _SUCCESS;
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	char	*szLine, *ptmp;
	u32	u4bRegOffset, u4bRegMask, u4bRegValue;
	u32	u4bMove;
	bool firstLine = true;
	u8	tx_num = 0;
	u8	band = 0, rf_path = 0;

	/* RTW_INFO("=====>phy_ParseBBPgParaFile()\n"); */

	if (Adapter->registrypriv.RegDecryptCustomFile == 1)
		phy_DecryptBBPgParaFile(Adapter, buffer);

	ptmp = buffer;
	for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
		if (isAllSpaceOrTab(szLine, sizeof(*szLine)))
			continue;

		if (!IsCommentString(szLine)) {
			/* Get header info (relative value or exact value) */
			if (firstLine) {
				if (eqNByte(szLine, (u8 *)("#[v1]"), 5)) {

					pHalData->odmpriv.phy_reg_pg_version = szLine[3] - '0';
					/* RTW_INFO("This is a new format PHY_REG_PG.txt\n"); */
				} else if (eqNByte(szLine, (u8 *)("#[v0]"), 5)) {
					pHalData->odmpriv.phy_reg_pg_version = szLine[3] - '0';
					/* RTW_INFO("This is a old format PHY_REG_PG.txt ok\n"); */
				} else {
					RTW_INFO("The format in PHY_REG_PG are invalid %s\n", szLine);
					return _FAIL;
				}

				if (eqNByte(szLine + 5, (u8 *)("[Exact]#"), 8)) {
					pHalData->odmpriv.phy_reg_pg_value_type = PHY_REG_PG_EXACT_VALUE;
					/* RTW_INFO("The values in PHY_REG_PG are exact values ok\n"); */
					firstLine = false;
					continue;
				} else if (eqNByte(szLine + 5, (u8 *)("[Relative]#"), 11)) {
					pHalData->odmpriv.phy_reg_pg_value_type = PHY_REG_PG_RELATIVE_VALUE;
					/* RTW_INFO("The values in PHY_REG_PG are relative values ok\n"); */
					firstLine = false;
					continue;
				} else {
					RTW_INFO("The values in PHY_REG_PG are invalid %s\n", szLine);
					return _FAIL;
				}
			}

			if (pHalData->odmpriv.phy_reg_pg_version == 0) {
				/* Get 1st hex value as register offset. */
				if (GetHexValueFromString(szLine, &u4bRegOffset, &u4bMove)) {
					szLine += u4bMove;
					if (u4bRegOffset == 0xffff) {
						/* Ending. */
						break;
					}

					/* Get 2nd hex value as register mask. */
					if (GetHexValueFromString(szLine, &u4bRegMask, &u4bMove))
						szLine += u4bMove;
					else
						return _FAIL;

					if (pHalData->odmpriv.phy_reg_pg_value_type == PHY_REG_PG_RELATIVE_VALUE) {
						/* Get 3rd hex value as register value. */
						if (GetHexValueFromString(szLine, &u4bRegValue, &u4bMove)) {
							phy_store_tx_power_by_rate(Adapter, 0, 0, 1, u4bRegOffset, u4bRegMask, u4bRegValue);
							/* RTW_INFO("[ADDR] %03X=%08X Mask=%08x\n", u4bRegOffset, u4bRegValue, u4bRegMask); */
						} else
							return _FAIL;
					} else if (pHalData->odmpriv.phy_reg_pg_value_type == PHY_REG_PG_EXACT_VALUE) {
						u32	combineValue = 0;
						u8	integer = 0, fraction = 0;

						if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
							szLine += u4bMove;
						else
							return _FAIL;

						integer *= 2;
						if (fraction == 5)
							integer += 1;
						combineValue |= (((integer / 10) << 4) + (integer % 10));
						/* RTW_INFO(" %d", integer ); */

						if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
							szLine += u4bMove;
						else
							return _FAIL;

						integer *= 2;
						if (fraction == 5)
							integer += 1;
						combineValue <<= 8;
						combineValue |= (((integer / 10) << 4) + (integer % 10));
						/* RTW_INFO(" %d", integer ); */

						if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
							szLine += u4bMove;
						else
							return _FAIL;

						integer *= 2;
						if (fraction == 5)
							integer += 1;
						combineValue <<= 8;
						combineValue |= (((integer / 10) << 4) + (integer % 10));
						/* RTW_INFO(" %d", integer ); */

						if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
							szLine += u4bMove;
						else
							return _FAIL;

						integer *= 2;
						if (fraction == 5)
							integer += 1;
						combineValue <<= 8;
						combineValue |= (((integer / 10) << 4) + (integer % 10));
						/* RTW_INFO(" %d", integer ); */
						phy_store_tx_power_by_rate(Adapter, 0, 0, 1, u4bRegOffset, u4bRegMask, combineValue);

						/* RTW_INFO("[ADDR] 0x%3x = 0x%4x\n", u4bRegOffset, combineValue ); */
					}
				}
			} else if (pHalData->odmpriv.phy_reg_pg_version > 0) {
				u32	index = 0;

				if (eqNByte(szLine, "0xffff", 6))
					break;

				if (!eqNByte("#[END]#", szLine, 7)) {
					/* load the table label info */
					if (szLine[0] == '#') {
						index = 0;
						if (eqNByte(szLine, "#[2.4G]" , 7)) {
							band = BAND_ON_2_4G;
							index += 8;
						} else if (eqNByte(szLine, "#[5G]", 5)) {
							band = BAND_ON_5G;
							index += 6;
						} else {
							RTW_INFO("Invalid band %s in PHY_REG_PG.txt\n", szLine);
							return _FAIL;
						}

						rf_path = szLine[index] - 'A';
						/* RTW_INFO(" Table label Band %d, RfPath %d\n", band, rf_path ); */
					} else { /* load rows of tables */
						if (szLine[1] == '1')
							tx_num = RF_1TX;
						else if (szLine[1] == '2')
							tx_num = RF_2TX;
						else if (szLine[1] == '3')
							tx_num = RF_3TX;
						else if (szLine[1] == '4')
							tx_num = RF_4TX;
						else {
							RTW_INFO("Invalid row in PHY_REG_PG.txt '%c'(%d)\n", szLine[1], szLine[1]);
							return _FAIL;
						}

						while (szLine[index] != ']')
							++index;
						++index;/* skip ] */

						/* Get 2nd hex value as register offset. */
						szLine += index;
						if (GetHexValueFromString(szLine, &u4bRegOffset, &u4bMove))
							szLine += u4bMove;
						else
							return _FAIL;

						/* Get 2nd hex value as register mask. */
						if (GetHexValueFromString(szLine, &u4bRegMask, &u4bMove))
							szLine += u4bMove;
						else
							return _FAIL;

						if (pHalData->odmpriv.phy_reg_pg_value_type == PHY_REG_PG_RELATIVE_VALUE) {
							/* Get 3rd hex value as register value. */
							if (GetHexValueFromString(szLine, &u4bRegValue, &u4bMove)) {
								phy_store_tx_power_by_rate(Adapter, band, rf_path, tx_num, u4bRegOffset, u4bRegMask, u4bRegValue);
								/* RTW_INFO("[ADDR] %03X (tx_num %d) =%08X Mask=%08x\n", u4bRegOffset, tx_num, u4bRegValue, u4bRegMask); */
							} else
								return _FAIL;
						} else if (pHalData->odmpriv.phy_reg_pg_value_type == PHY_REG_PG_EXACT_VALUE) {
							u32	combineValue = 0;
							u8	integer = 0, fraction = 0;

							if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
								szLine += u4bMove;
							else
								return _FAIL;

							integer *= 2;
							if (fraction == 5)
								integer += 1;
							combineValue |= (((integer / 10) << 4) + (integer % 10));
							/* RTW_INFO(" %d", integer ); */

							if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
								szLine += u4bMove;
							else
								return _FAIL;

							integer *= 2;
							if (fraction == 5)
								integer += 1;
							combineValue <<= 8;
							combineValue |= (((integer / 10) << 4) + (integer % 10));
							/* RTW_INFO(" %d", integer ); */

							if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
								szLine += u4bMove;
							else
								return _FAIL;

							integer *= 2;
							if (fraction == 5)
								integer += 1;
							combineValue <<= 8;
							combineValue |= (((integer / 10) << 4) + (integer % 10));
							/* RTW_INFO(" %d", integer ); */

							if (GetFractionValueFromString(szLine, &integer, &fraction, &u4bMove))
								szLine += u4bMove;
							else
								return _FAIL;

							integer *= 2;
							if (fraction == 5)
								integer += 1;
							combineValue <<= 8;
							combineValue |= (((integer / 10) << 4) + (integer % 10));
							/* RTW_INFO(" %d", integer ); */
							phy_store_tx_power_by_rate(Adapter, band, rf_path, tx_num, u4bRegOffset, u4bRegMask, combineValue);

							/* RTW_INFO("[ADDR] 0x%3x (tx_num %d) = 0x%4x\n", u4bRegOffset, tx_num, combineValue ); */
						}
					}
				}
			}
		}
	}
	/* RTW_INFO("<=====phy_ParseBBPgParaFile()\n"); */
	return rtStatus;
}

int
phy_ConfigBBWithPgParaFile(
	struct adapter *	Adapter,
	const char	*pFileName)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_BB_PG_PARA_FILE))
		return rtStatus;

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if (!pHalData->bb_phy_reg_pg) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pHalData->bb_phy_reg_pg = vzalloc(rlen);
				if (pHalData->bb_phy_reg_pg) {
					memcpy(pHalData->bb_phy_reg_pg, pHalData->para_file_buf, rlen);
					pHalData->bb_phy_reg_pg_len = rlen;
				} else
					RTW_INFO("%s bb_phy_reg_pg alloc fail !\n", __func__);
			}
		}
	} else {
		if ((pHalData->bb_phy_reg_pg_len != 0) && (pHalData->bb_phy_reg_pg)) {
			memcpy(pHalData->para_file_buf, pHalData->bb_phy_reg_pg, pHalData->bb_phy_reg_pg_len);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS) {
		/* RTW_INFO("phy_ConfigBBWithPgParaFile(): read %s ok\n", pFileName); */
		phy_ParseBBPgParaFile(Adapter, pHalData->para_file_buf);
	} else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);

	return rtStatus;
}

#if (MP_DRIVER == 1)

int
phy_ConfigBBWithMpParaFile(
	struct adapter *	Adapter,
	char		*pFileName
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;
	char	*szLine, *ptmp;
	u32	u4bRegOffset, u4bRegValue, u4bMove;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_BB_MP_PARA_FILE))
		return rtStatus;

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if ((pHalData->bb_phy_reg_mp_len == 0) && (!pHalData->bb_phy_reg_mp)) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pHalData->bb_phy_reg_mp = vzalloc(rlen);
				if (pHalData->bb_phy_reg_mp) {
					memcpy(pHalData->bb_phy_reg_mp, pHalData->para_file_buf, rlen);
					pHalData->bb_phy_reg_mp_len = rlen;
				} else
					RTW_INFO("%s bb_phy_reg_mp alloc fail !\n", __func__);
			}
		}
	} else {
		if ((pHalData->bb_phy_reg_mp_len != 0) && (pHalData->bb_phy_reg_mp)) {
			memcpy(pHalData->para_file_buf, pHalData->bb_phy_reg_mp, pHalData->bb_phy_reg_mp_len);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS) {
		/* RTW_INFO("phy_ConfigBBWithMpParaFile(): read %s ok\n", pFileName); */

		ptmp = pHalData->para_file_buf;
		for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
			if (!IsCommentString(szLine)) {
				/* Get 1st hex value as register offset. */
				if (GetHexValueFromString(szLine, &u4bRegOffset, &u4bMove)) {
					if (u4bRegOffset == 0xffff) {
						/* Ending. */
						break;
					} else if (u4bRegOffset == 0xfe || u4bRegOffset == 0xffe) {
						rtw_msleep_os(50);
					} else if (u4bRegOffset == 0xfd)
						rtw_mdelay_os(5);
					else if (u4bRegOffset == 0xfc)
						rtw_mdelay_os(1);
					else if (u4bRegOffset == 0xfb)
						rtw_udelay_os(50);
					else if (u4bRegOffset == 0xfa)
						rtw_udelay_os(5);
					else if (u4bRegOffset == 0xf9)
						rtw_udelay_os(1);

					/* Get 2nd hex value as register value. */
					szLine += u4bMove;
					if (GetHexValueFromString(szLine, &u4bRegValue, &u4bMove)) {
						/* RTW_INFO("[ADDR]%03lX=%08lX\n", u4bRegOffset, u4bRegValue); */
						phy_set_bb_reg(Adapter, u4bRegOffset, bMaskDWord, u4bRegValue);

						/* Add 1us delay between BB/RF register setting. */
						rtw_udelay_os(1);
					}
				}
			}
		}
	} else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);

	return rtStatus;
}

#endif

int
PHY_ConfigRFWithParaFile(
	struct adapter *	Adapter,
	char		*pFileName,
	enum rf_path		eRFPath
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;
	char	*szLine, *ptmp;
	u32	u4bRegOffset, u4bRegValue, u4bMove;
	u16	i;
	char	*pBuf = NULL;
	u32	*pBufLen = NULL;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_RF_PARA_FILE))
		return rtStatus;

	switch (eRFPath) {
	case RF_PATH_A:
		pBuf = pHalData->rf_radio_a;
		pBufLen = &pHalData->rf_radio_a_len;
		break;
	case RF_PATH_B:
		pBuf = pHalData->rf_radio_b;
		pBufLen = &pHalData->rf_radio_b_len;
		break;
	default:
		RTW_INFO("Unknown RF path!! %d\r\n", eRFPath);
		break;
	}

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if ((pBufLen) && (*pBufLen == 0) && (!pBuf)) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pBuf = vzalloc(rlen);
				if (pBuf) {
					memcpy(pBuf, pHalData->para_file_buf, rlen);
					*pBufLen = rlen;

					switch (eRFPath) {
					case RF_PATH_A:
						pHalData->rf_radio_a = pBuf;
						break;
					case RF_PATH_B:
						pHalData->rf_radio_b = pBuf;
						break;
					default:
						RTW_INFO("Unknown RF path!! %d\r\n", eRFPath);
						break;
					}
				} else
					RTW_INFO("%s(): eRFPath=%d  alloc fail !\n", __func__, eRFPath);
			}
		}
	} else {
		if ((pBufLen) && (*pBufLen != 0) && (pBuf)) {
			memcpy(pHalData->para_file_buf, pBuf, *pBufLen);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS) {
		/* RTW_INFO("%s(): read %s successfully\n", __func__, pFileName); */

		ptmp = pHalData->para_file_buf;
		for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
			if (!IsCommentString(szLine)) {
				/* Get 1st hex value as register offset. */
				if (GetHexValueFromString(szLine, &u4bRegOffset, &u4bMove)) {
					if (u4bRegOffset == 0xfe || u4bRegOffset == 0xffe) {
						/* Delay specific ms. Only RF configuration require delay. */
						rtw_msleep_os(50);
					} else if (u4bRegOffset == 0xfd) {
						/* delay_ms(5); */
						for (i = 0; i < 100; i++)
							rtw_udelay_os(MAX_STALL_TIME);
					} else if (u4bRegOffset == 0xfc) {
						/* delay_ms(1); */
						for (i = 0; i < 20; i++)
							rtw_udelay_os(MAX_STALL_TIME);
					} else if (u4bRegOffset == 0xfb)
						rtw_udelay_os(50);
					else if (u4bRegOffset == 0xfa)
						rtw_udelay_os(5);
					else if (u4bRegOffset == 0xf9)
						rtw_udelay_os(1);
					else if (u4bRegOffset == 0xffff)
						break;

					/* Get 2nd hex value as register value. */
					szLine += u4bMove;
					if (GetHexValueFromString(szLine, &u4bRegValue, &u4bMove)) {
						phy_set_rf_reg(Adapter, eRFPath, u4bRegOffset, bRFRegOffsetMask, u4bRegValue);

						/* Temp add, for frequency lock, if no delay, that may cause */
						/* frequency shift, ex: 2412MHz => 2417MHz */
						/* If frequency shift, the following action may works. */
						/* Fractional-N table in radio_a.txt */
						/* 0x2a 0x00001		 */ /* channel 1 */
						/* 0x2b 0x00808		frequency divider. */
						/* 0x2b 0x53333 */
						/* 0x2c 0x0000c */
						rtw_udelay_os(1);
					}
				}
			}
		}
	} else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);

	return rtStatus;
}

static void
initDeltaSwingIndexTables(
	struct adapter *	Adapter,
	char		*Band,
	char		*Path,
	char		*Sign,
	char		*Channel,
	char		*Rate,
	char		*Data
)
{
#define STR_EQUAL_5G(_band, _path, _sign, _rate, _chnl) \
	((strcmp(Band, _band) == 0) && (strcmp(Path, _path) == 0) && (strcmp(Sign, _sign) == 0) &&\
	 (strcmp(Rate, _rate) == 0) && (strcmp(Channel, _chnl) == 0)\
	)
#define STR_EQUAL_2G(_band, _path, _sign, _rate) \
	((strcmp(Band, _band) == 0) && (strcmp(Path, _path) == 0) && (strcmp(Sign, _sign) == 0) &&\
	 (strcmp(Rate, _rate) == 0)\
	)

#define STORE_SWING_TABLE(_array, _iteratedIdx) \
	do {	\
	for (token = strsep(&Data, delim); token; token = strsep(&Data, delim)) {\
		sscanf(token, "%d", &idx);\
		_array[_iteratedIdx++] = (u8)idx;\
	} } while (0)\

	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	struct PHY_DM_STRUCT		*pDM_Odm = &pHalData->odmpriv;
	struct odm_rf_calibration_structure	*pRFCalibrateInfo = &(pDM_Odm->rf_calibrate_info);
	u32	j = 0;
	char	*token;
	char	delim[] = ",";
	u32	idx = 0;

	/* RTW_INFO("===>initDeltaSwingIndexTables(): Band: %s;\nPath: %s;\nSign: %s;\nChannel: %s;\nRate: %s;\n, Data: %s;\n",  */
	/*	Band, Path, Sign, Channel, Rate, Data); */

	if (STR_EQUAL_2G("2G", "A", "+", "CCK"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2g_cck_a_p, j);
	else if (STR_EQUAL_2G("2G", "A", "-", "CCK"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2g_cck_a_n, j);
	else if (STR_EQUAL_2G("2G", "B", "+", "CCK"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2g_cck_b_p, j);
	else if (STR_EQUAL_2G("2G", "B", "-", "CCK"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2g_cck_b_n, j);
	else if (STR_EQUAL_2G("2G", "A", "+", "ALL"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2ga_p, j);
	else if (STR_EQUAL_2G("2G", "A", "-", "ALL"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2ga_n, j);
	else if (STR_EQUAL_2G("2G", "B", "+", "ALL"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2gb_p, j);
	else if (STR_EQUAL_2G("2G", "B", "-", "ALL"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_2gb_n, j);
	else if (STR_EQUAL_5G("5G", "A", "+", "ALL", "0"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_p[0], j);
	else if (STR_EQUAL_5G("5G", "A", "-", "ALL", "0"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_n[0], j);
	else if (STR_EQUAL_5G("5G", "B", "+", "ALL", "0"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_p[0], j);
	else if (STR_EQUAL_5G("5G", "B", "-", "ALL", "0"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_n[0], j);
	else if (STR_EQUAL_5G("5G", "A", "+", "ALL", "1"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_p[1], j);
	else if (STR_EQUAL_5G("5G", "A", "-", "ALL", "1"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_n[1], j);
	else if (STR_EQUAL_5G("5G", "B", "+", "ALL", "1"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_p[1], j);
	else if (STR_EQUAL_5G("5G", "B", "-", "ALL", "1"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_n[1], j);
	else if (STR_EQUAL_5G("5G", "A", "+", "ALL", "2"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_p[2], j);
	else if (STR_EQUAL_5G("5G", "A", "-", "ALL", "2"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_n[2], j);
	else if (STR_EQUAL_5G("5G", "B", "+", "ALL", "2"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_p[2], j);
	else if (STR_EQUAL_5G("5G", "B", "-", "ALL", "2"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_n[2], j);
	else if (STR_EQUAL_5G("5G", "A", "+", "ALL", "3"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_p[3], j);
	else if (STR_EQUAL_5G("5G", "A", "-", "ALL", "3"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5ga_n[3], j);
	else if (STR_EQUAL_5G("5G", "B", "+", "ALL", "3"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_p[3], j);
	else if (STR_EQUAL_5G("5G", "B", "-", "ALL", "3"))
		STORE_SWING_TABLE(pRFCalibrateInfo->delta_swing_table_idx_5gb_n[3], j);
	else
		RTW_INFO("===>initDeltaSwingIndexTables(): The input is invalid!!\n");
}

int
PHY_ConfigRFWithTxPwrTrackParaFile(
	struct adapter *		Adapter,
	char			*pFileName
)
{
	struct hal_com_data		*pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;
	char	*szLine, *ptmp;
	u32	i = 0;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_RF_TXPWR_TRACK_PARA_FILE))
		return rtStatus;

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if ((pHalData->rf_tx_pwr_track_len == 0) && (!pHalData->rf_tx_pwr_track)) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pHalData->rf_tx_pwr_track = vzalloc(rlen);
				if (pHalData->rf_tx_pwr_track) {
					memcpy(pHalData->rf_tx_pwr_track, pHalData->para_file_buf, rlen);
					pHalData->rf_tx_pwr_track_len = rlen;
				} else
					RTW_INFO("%s rf_tx_pwr_track alloc fail !\n", __func__);
			}
		}
	} else {
		if ((pHalData->rf_tx_pwr_track_len != 0) && (pHalData->rf_tx_pwr_track)) {
			memcpy(pHalData->para_file_buf, pHalData->rf_tx_pwr_track, pHalData->rf_tx_pwr_track_len);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS) {
		/* RTW_INFO("%s(): read %s successfully\n", __func__, pFileName); */

		ptmp = pHalData->para_file_buf;
		for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
			if (!IsCommentString(szLine)) {
				char	band[5] = "", path[5] = "", sign[5]  = "";
				char	chnl[5] = "", rate[10] = "";
				char	data[300] = ""; /* 100 is too small */

				if (strlen(szLine) < 10 || szLine[0] != '[')
					continue;

				strncpy(band, szLine + 1, 2);
				strncpy(path, szLine + 5, 1);
				strncpy(sign, szLine + 8, 1);

				i = 10; /* szLine+10 */
				if (!ParseQualifiedString(szLine, &i, rate, '[', ']')) {
					/* RTW_INFO("Fail to parse rate!\n"); */
				}
				if (!ParseQualifiedString(szLine, &i, chnl, '[', ']')) {
					/* RTW_INFO("Fail to parse channel group!\n"); */
				}
				while (szLine[i] != '{' && i < strlen(szLine))
					i++;
				if (!ParseQualifiedString(szLine, &i, data, '{', '}')) {
					/* RTW_INFO("Fail to parse data!\n"); */
				}

				initDeltaSwingIndexTables(Adapter, band, path, sign, chnl, rate, data);
			}
		}
	} else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);
	return rtStatus;
}

#ifdef CONFIG_TXPWR_LIMIT

#define PARSE_RET_NO_HDL	0
#define PARSE_RET_SUCCESS	1
#define PARSE_RET_FAIL		2

/*
* @@Ver=2.0
* or
* @@DomainCode=0x28, Regulation=C6
* or
* @@CountryCode=GB, Regulation=C7
*/
static u8 parse_reg_exc_config(struct adapter *adapter, char *szLine)
{
#define VER_PREFIX "Ver="
#define DOMAIN_PREFIX "DomainCode=0x"
#define COUNTRY_PREFIX "CountryCode="
#define REG_PREFIX "Regulation="

	const u8 ver_prefix_len = strlen(VER_PREFIX);
	const u8 domain_prefix_len = strlen(DOMAIN_PREFIX);
	const u8 country_prefix_len = strlen(COUNTRY_PREFIX);
	const u8 reg_prefix_len = strlen(REG_PREFIX);
	u32 i, i_val_s, i_val_e;
	u32 j;
	u8 domain = 0xFF;
	char *country = NULL;
	u8 parse_reg = 0;

	if (szLine[0] != '@' || szLine[1] != '@')
		return PARSE_RET_NO_HDL;

	i = 2;
	if (strncmp(szLine + i, VER_PREFIX, ver_prefix_len) == 0)
		; /* nothing to do */
	else if (strncmp(szLine + i, DOMAIN_PREFIX, domain_prefix_len) == 0) {
		/* get string after domain prefix to ',' */
		i += domain_prefix_len;
		i_val_s = i;
		while (szLine[i] != ',') {
			if (szLine[i] == '\0')
				return PARSE_RET_FAIL;
			i++;
		}
		i_val_e = i;

		/* check if all hex */
		for (j = i_val_s; j < i_val_e; j++)
			if (!IsHexDigit(szLine[j]))
				return PARSE_RET_FAIL;

		/* get value from hex string */
		if (sscanf(szLine + i_val_s, "%hhx", &domain) != 1)
			return PARSE_RET_FAIL;

		parse_reg = 1;
	} else if (strncmp(szLine + i, COUNTRY_PREFIX, country_prefix_len) == 0) {
		/* get string after country prefix to ',' */
		i += country_prefix_len;
		i_val_s = i;
		while (szLine[i] != ',') {
			if (szLine[i] == '\0')
				return PARSE_RET_FAIL;
			i++;
		}
		i_val_e = i;

		if (i_val_e - i_val_s != 2)
			return PARSE_RET_FAIL;

		/* check if all alpha */
		for (j = i_val_s; j < i_val_e; j++)
			if (!is_alpha(szLine[j]))
				return PARSE_RET_FAIL;

		country = szLine + i_val_s;

		parse_reg = 1;

	} else
		return PARSE_RET_FAIL;

	if (parse_reg) {
		/* move to 'R' */
		while (szLine[i] != 'R') {
			if (szLine[i] == '\0')
				return PARSE_RET_FAIL;
			i++;
		}

		/* check if matching regulation prefix */
		if (strncmp(szLine + i, REG_PREFIX, reg_prefix_len) != 0)
			return PARSE_RET_FAIL;

		/* get string after regulation prefix ending with space */
		i += reg_prefix_len;
		i_val_s = i;
		while (szLine[i] != ' ' && szLine[i] != '\t' && szLine[i] != '\0')
			i++;

		if (i == i_val_s)
			return PARSE_RET_FAIL;

		rtw_regd_exc_add_with_nlen(adapter_to_rfctl(adapter), country, domain, szLine + i_val_s, i - i_val_s);
	}

	return PARSE_RET_SUCCESS;
}

static int
phy_ParsePowerLimitTableFile(
	struct adapter *		Adapter,
	char			*buffer
)
{
#define LD_STAGE_EXC_MAPPING	0
#define LD_STAGE_TAB_DEFINE		1
#define LD_STAGE_TAB_START		2
#define LD_STAGE_COLUMN_DEFINE	3
#define LD_STAGE_CH_ROW			4

	int	rtStatus = _FAIL;
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	struct PHY_DM_STRUCT	*pDM_Odm = &(pHalData->odmpriv);
	u8	loadingStage = LD_STAGE_EXC_MAPPING;
	u32	i = 0, forCnt = 0;
	u8 limitValue = 0, fraction = 0, negative = 0;
	char	*szLine, *ptmp;
	char band[10], bandwidth[10], rateSection[10], ntx[10], colNumBuf[10];
	char **regulation = NULL;
	u8	colNum = 0;

	RTW_INFO("%s enter\n", __func__);

	if (Adapter->registrypriv.RegDecryptCustomFile == 1)
		phy_DecryptBBPgParaFile(Adapter, buffer);

	ptmp = buffer;
	for (szLine = GetLineFromBuffer(ptmp); szLine; szLine = GetLineFromBuffer(ptmp)) {
		if (isAllSpaceOrTab(szLine, sizeof(*szLine)))
			continue;
		if (IsCommentString(szLine))
			continue;

		if (loadingStage == LD_STAGE_EXC_MAPPING) {
			if (szLine[0] == '#' || szLine[1] == '#') {
				loadingStage = LD_STAGE_TAB_DEFINE;
			} else {
				if (parse_reg_exc_config(Adapter, szLine) == PARSE_RET_FAIL) {
					RTW_ERR("Fail to parse regulation exception ruls!\n");
					goto exit;
				}
				continue;
			}
		}

		if (loadingStage == LD_STAGE_TAB_DEFINE) {
			/* read "##	2.4G, 20M, 1T, CCK" */
			if (szLine[0] != '#' || szLine[1] != '#')
				continue;

			/* skip the space */
			i = 2;
			while (szLine[i] == ' ' || szLine[i] == '\t')
				++i;

			szLine[--i] = ' '; /* return the space in front of the regulation info */

			/* Parse the label of the table */
			memset((void *) band, 0, 10);
			memset((void *) bandwidth, 0, 10);
			memset((void *) ntx, 0, 10);
			memset((void *) rateSection, 0, 10);
			if (!ParseQualifiedString(szLine, &i, band, ' ', ',')) {
				RTW_ERR("Fail to parse band!\n");
				goto exit;
			}
			if (!ParseQualifiedString(szLine, &i, bandwidth, ' ', ',')) {
				RTW_ERR("Fail to parse bandwidth!\n");
				goto exit;
			}
			if (!ParseQualifiedString(szLine, &i, ntx, ' ', ',')) {
				RTW_ERR("Fail to parse ntx!\n");
				goto exit;
			}
			if (!ParseQualifiedString(szLine, &i, rateSection, ' ', ',')) {
				RTW_ERR("Fail to parse rate!\n");
				goto exit;
			}

			loadingStage = LD_STAGE_TAB_START;
		} else if (loadingStage == LD_STAGE_TAB_START) {
			/* read "##	START" */
			if (szLine[0] != '#' || szLine[1] != '#')
				continue;

			/* skip the space */
			i = 2;
			while (szLine[i] == ' ' || szLine[i] == '\t')
				++i;

			if (!eqNByte((u8 *)(szLine + i), (u8 *)("START"), 5)) {
				RTW_ERR("Missing \"##   START\" label\n");
				goto exit;
			}

			loadingStage = LD_STAGE_COLUMN_DEFINE;
		} else if (loadingStage == LD_STAGE_COLUMN_DEFINE) {
			/* read "##	#5#	FCC	ETSI	MKK	IC	KCC" */
			if (szLine[0] != '#' || szLine[1] != '#')
				continue;

			/* skip the space */
			i = 2;
			while (szLine[i] == ' ' || szLine[i] == '\t')
				++i;

			memset((void *) colNumBuf, 0, 10);
			if (!ParseQualifiedString(szLine, &i, colNumBuf, '#', '#')) {
				RTW_ERR("Fail to parse column number!\n");
				goto exit;
			}
			if (!GetU1ByteIntegerFromStringInDecimal(colNumBuf, &colNum)) {
				RTW_ERR("Column number \"%s\" is not unsigned decimal\n", colNumBuf);
				goto exit;
			}
			if (colNum == 0) {
				RTW_ERR("Column number is 0\n");
				goto exit;
			}

			regulation = (char **)rtw_zmalloc(sizeof(char *) * colNum);
			if (!regulation) {
				RTW_ERR("Regulation alloc fail\n");
				goto exit;
			}

			for (forCnt = 0; forCnt < colNum; ++forCnt) {
				u32 i_ns;

				/* skip the space */
				while (szLine[i] == ' ' || szLine[i] == '\t')
					i++;
				i_ns = i;

				while (szLine[i] != ' ' && szLine[i] != '\t' && szLine[i] != '\0')
					i++;

				regulation[forCnt] = (char *)rtw_malloc(i - i_ns + 1);
				if (!regulation[forCnt]) {
					RTW_ERR("Regulation alloc fail\n");
					goto exit;
				}

				memcpy(regulation[forCnt], szLine + i_ns, i - i_ns);
				regulation[forCnt][i - i_ns] = '\0';
			}

			loadingStage = LD_STAGE_CH_ROW;
		} else if (loadingStage == LD_STAGE_CH_ROW) {
			char	channel[10] = {0}, powerLimit[10] = {0};
			u8	cnt = 0;

			/* the table ends */
			if (szLine[0] == '#' && szLine[1] == '#') {
				i = 2;
				while (szLine[i] == ' ' || szLine[i] == '\t')
					++i;

				if (eqNByte((u8 *)(szLine + i), (u8 *)("END"), 3)) {
					loadingStage = LD_STAGE_TAB_DEFINE;
					if (regulation) {
						for (forCnt = 0; forCnt < colNum; ++forCnt) {
							if (regulation[forCnt]) {
								rtw_mfree(regulation[forCnt], strlen(regulation[forCnt]) + 1);
								regulation[forCnt] = NULL;
							}
						}
						rtw_mfree((u8 *)regulation, sizeof(char *) * colNum);
						regulation = NULL;
					}
					colNum = 0;
					continue;
				} else {
					RTW_ERR("Missing \"##   END\" label\n");
					goto exit;
				}
			}

			if ((szLine[0] != 'c' && szLine[0] != 'C') ||
				(szLine[1] != 'h' && szLine[1] != 'H')
			) {
				RTW_WARN("Wrong channel prefix: '%c','%c'(%d,%d)\n", szLine[0], szLine[1], szLine[0], szLine[1]);
				continue;
			}
			i = 2;/* move to the  location behind 'h' */

			/* load the channel number */
			cnt = 0;
			while (szLine[i] >= '0' && szLine[i] <= '9') {
				channel[cnt] = szLine[i];
				++cnt;
				++i;
			}
			/* RTW_INFO("chnl %s!\n", channel); */

			for (forCnt = 0; forCnt < colNum; ++forCnt) {
				/* skip the space between channel number and the power limit value */
				while (szLine[i] == ' ' || szLine[i] == '\t')
					++i;

				/* load the power limit value */
				cnt = 0;
				fraction = 0;
				negative = 0;
				memset((void *) powerLimit, 0, 10);

				while ((szLine[i] >= '0' && szLine[i] <= '9') || szLine[i] == '.'
					|| szLine[i] == '+' || szLine[i] == '-'
				) {
					/* try to get valid decimal number */
					if (szLine[i] == '+' || szLine[i] == '-') {
						if (cnt != 0) {
							RTW_ERR("Wrong position for sign '%c'\n", szLine[i]);
							goto exit;
						}
						if (szLine[i] == '-') {
							negative = 1;
							++i;
							continue;
						}

					} else if (szLine[i] == '.') {
						if ((szLine[i + 1] >= '0' && szLine[i + 1] <= '9')) {
							fraction = szLine[i + 1];
							i += 2;
						} else {
							RTW_ERR("Wrong fraction '%c'(%d)\n", szLine[i + 1], szLine[i + 1]);
							goto exit;
						}

						break;
					}

					powerLimit[cnt] = szLine[i];
					++cnt;
					++i;
				}

				if (powerLimit[0] == '\0') {
					if (szLine[i] == 'W' && szLine[i + 1] == 'W') {
						/*
						* case "WW" assign special value -63
						* means to get minimal limit in other regulations at same channel
						*/
						powerLimit[0] = '-';
						powerLimit[1] = '6';
						powerLimit[2] = '3';
						i += 2;
					} else if (szLine[i] == 'N' && szLine[i + 1] == 'A') {
						/*
						* case "NA" assign special value 63
						* means no limitation
						*/
						powerLimit[0] = '6';
						powerLimit[1] = '3';
						i += 2;
					} else {
						RTW_ERR("Wrong limit expression \"%c%c\"(%d, %d)\n"
							, szLine[i], szLine[i + 1], szLine[i], szLine[i + 1]);
						goto exit;
					}
				} else {
					/* transform dicimal value to power index */
					if (!GetU1ByteIntegerFromStringInDecimal(powerLimit, &limitValue)) {
						RTW_ERR("Limit \"%s\" is not valid decimal\n", powerLimit);
						goto exit;
					}

					limitValue *= 2;
					cnt = 0;

					if (negative)
						powerLimit[cnt++] = '-';

					if (fraction == '5')
						++limitValue;

					/* the value is greater or equal to 100 */
					if (limitValue >= 100) {
						powerLimit[cnt++] = limitValue / 100 + '0';
						limitValue %= 100;

						if (limitValue >= 10) {
							powerLimit[cnt++] = limitValue / 10 + '0';
							limitValue %= 10;
						} else
							powerLimit[cnt++] = '0';

						powerLimit[cnt++] = limitValue + '0';
					}
					/* the value is greater or equal to 10 */
					else if (limitValue >= 10) {
						powerLimit[cnt++] = limitValue / 10 + '0';
						limitValue %= 10;
						powerLimit[cnt++] = limitValue + '0';
					}
					/* the value is less than 10 */
					else
						powerLimit[cnt++] = limitValue + '0';

					powerLimit[cnt] = '\0';
				}

				/* RTW_INFO("ch%s => %s\n", channel, powerLimit); */

				/* store the power limit value */
				phy_set_tx_power_limit(pDM_Odm, (u8 *)regulation[forCnt], (u8 *)band,
					(u8 *)bandwidth, (u8 *)rateSection, (u8 *)ntx, (u8 *)channel, (u8 *)powerLimit);

			}
		}
	}

	rtStatus = _SUCCESS;

exit:
	if (regulation) {
		for (forCnt = 0; forCnt < colNum; ++forCnt) {
			if (regulation[forCnt]) {
				rtw_mfree(regulation[forCnt], strlen(regulation[forCnt]) + 1);
				regulation[forCnt] = NULL;
			}
		}
		rtw_mfree((u8 *)regulation, sizeof(char *) * colNum);
		regulation = NULL;
	}

	RTW_INFO("%s return %d\n", __func__, rtStatus);
	return rtStatus;
}

int
PHY_ConfigRFWithPowerLimitTableParaFile(
	struct adapter *	Adapter,
	const char	*pFileName
)
{
	struct hal_com_data		*pHalData = GET_HAL_DATA(Adapter);
	int	rlen = 0, rtStatus = _FAIL;

	if (!(Adapter->registrypriv.load_phy_file & LOAD_RF_TXPWR_LMT_PARA_FILE))
		return rtStatus;

	memset(pHalData->para_file_buf, 0, MAX_PARA_FILE_BUF_LEN);

	if (!pHalData->rf_tx_pwr_lmt) {
		rtw_get_phy_file_path(Adapter, pFileName);
		if (rtw_is_file_readable(rtw_phy_para_file_path)) {
			rlen = rtw_retrieve_from_file(rtw_phy_para_file_path, pHalData->para_file_buf, MAX_PARA_FILE_BUF_LEN);
			if (rlen > 0) {
				rtStatus = _SUCCESS;
				pHalData->rf_tx_pwr_lmt = vzalloc(rlen);
				if (pHalData->rf_tx_pwr_lmt) {
					memcpy(pHalData->rf_tx_pwr_lmt, pHalData->para_file_buf, rlen);
					pHalData->rf_tx_pwr_lmt_len = rlen;
				} else
					RTW_INFO("%s rf_tx_pwr_lmt alloc fail !\n", __func__);
			}
		}
	} else {
		if ((pHalData->rf_tx_pwr_lmt_len != 0) && (pHalData->rf_tx_pwr_lmt)) {
			memcpy(pHalData->para_file_buf, pHalData->rf_tx_pwr_lmt, pHalData->rf_tx_pwr_lmt_len);
			rtStatus = _SUCCESS;
		} else
			RTW_INFO("%s(): Critical Error !!!\n", __func__);
	}

	if (rtStatus == _SUCCESS)
		rtStatus = phy_ParsePowerLimitTableFile(Adapter, pHalData->para_file_buf);
	else
		RTW_INFO("%s(): No File %s, Load from HWImg Array!\n", __func__, pFileName);

	return rtStatus;
}
#endif /* CONFIG_TXPWR_LIMIT */

void phy_free_filebuf_mask(struct adapter *adapt, u8 mask)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	if (pHalData->mac_reg && (mask & LOAD_MAC_PARA_FILE)) {
		vfree(pHalData->mac_reg);
		pHalData->mac_reg = NULL;
	}
	if (mask & LOAD_BB_PARA_FILE) {
		if (pHalData->bb_phy_reg) {
			vfree(pHalData->bb_phy_reg);
			pHalData->bb_phy_reg = NULL;
		}
		if (pHalData->bb_agc_tab) {
			vfree(pHalData->bb_agc_tab);
			pHalData->bb_agc_tab = NULL;
		}
	}
	if (pHalData->bb_phy_reg_pg && (mask & LOAD_BB_PG_PARA_FILE)) {
		vfree(pHalData->bb_phy_reg_pg);
		pHalData->bb_phy_reg_pg = NULL;
	}
	if (pHalData->bb_phy_reg_mp && (mask & LOAD_BB_MP_PARA_FILE)) {
		vfree(pHalData->bb_phy_reg_mp);
		pHalData->bb_phy_reg_mp = NULL;
	}
	if (mask & LOAD_RF_PARA_FILE) {
		if (pHalData->rf_radio_a) {
			vfree(pHalData->rf_radio_a);
			pHalData->rf_radio_a = NULL;
		}
		if (pHalData->rf_radio_b) {
			vfree(pHalData->rf_radio_b);
			pHalData->rf_radio_b = NULL;
		}
	}
	if (pHalData->rf_tx_pwr_track && (mask & LOAD_RF_TXPWR_TRACK_PARA_FILE)) {
		vfree(pHalData->rf_tx_pwr_track);
		pHalData->rf_tx_pwr_track = NULL;
	}
	if (pHalData->rf_tx_pwr_lmt && (mask & LOAD_RF_TXPWR_LMT_PARA_FILE)) {
		vfree(pHalData->rf_tx_pwr_lmt);
		pHalData->rf_tx_pwr_lmt = NULL;
	}
}

inline void phy_free_filebuf(struct adapter *adapt)
{
	phy_free_filebuf_mask(adapt, 0xFF);
}

#endif
