/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __HAL_DM_H__
#define __HAL_DM_H__

#define adapter_to_phydm(adapter) (&(GET_HAL_DATA(adapter)->odmpriv))

void Init_ODM_ComInfo(struct adapter *adapter);
void rtw_hal_turbo_edca(struct adapter *adapter);
u8 rtw_phydm_is_iqk_in_progress(struct adapter *adapter);

void GetHalODMVar(
	struct adapter *				Adapter,
	enum hal_odm_variable		eVariable,
	void *					pValue1,
	void *					pValue2);
void SetHalODMVar(
	struct adapter *				Adapter,
	enum hal_odm_variable		eVariable,
	void *					pValue1,
	bool					bSet);

void rtw_phydm_ra_registed(struct adapter *adapter, struct sta_info *psta);

void rtw_phydm_watchdog(struct adapter *adapter);

void rtw_hal_update_iqk_fw_offload_cap(struct adapter *adapter);
void dump_sta_info(void *sel, struct sta_info *psta);
void dump_sta_traffic(void *sel, struct adapter *adapter, struct sta_info *psta);

#ifdef CONFIG_DBG_RF_CAL
void rtw_hal_iqk_test(struct adapter *adapter, bool recovery, bool clear, bool segment);
void rtw_hal_lck_test(struct adapter *adapter);
#endif

s8 rtw_phydm_get_min_rssi(struct adapter *adapter);
u8 rtw_phydm_get_cur_igi(struct adapter *adapter);

enum phy_cnt {
	FA_OFDM,
	FA_CCK,
	FA_TOTAL,
	CCA_OFDM,
	CCA_CCK,
	CCA_ALL,
	CRC32_OK_VHT,
	CRC32_OK_HT,
	CRC32_OK_LEGACY,
	CRC32_OK_CCK,
	CRC32_ERROR_VHT,
	CRC32_ERROR_HT,
	CRC32_ERROR_LEGACY,
	CRC32_ERROR_CCK,
};
u32 rtw_phydm_get_phy_cnt(struct adapter *adapter, enum phy_cnt cnt);

#endif /* __HAL_DM_H__ */
