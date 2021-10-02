/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#ifndef __RTW_ODM_H__
#define __RTW_ODM_H__

#include <drv_types.h>
#include "../hal/phydm/phydm_types.h"
/*
* This file provides utilities/wrappers for rtw driver to use ODM
*/
enum hal_phydm_ops {
	HAL_PHYDM_DIS_ALL_FUNC,
	HAL_PHYDM_FUNC_SET,
	HAL_PHYDM_FUNC_CLR,
	HAL_PHYDM_ABILITY_BK,
	HAL_PHYDM_ABILITY_RESTORE,
	HAL_PHYDM_ABILITY_SET,
	HAL_PHYDM_ABILITY_GET,
};

#define DYNAMIC_FUNC_DISABLE		(0x0)
	u32 rtw_phydm_ability_ops(struct adapter *adapter, enum hal_phydm_ops ops, u32 ability);

#define rtw_phydm_func_disable_all(adapter)	\
		rtw_phydm_ability_ops(adapter, HAL_PHYDM_DIS_ALL_FUNC, 0)

#define rtw_phydm_func_for_offchannel(adapter) \
		do { \
			rtw_phydm_ability_ops(adapter, HAL_PHYDM_DIS_ALL_FUNC, 0); \
			if (rtw_odm_adaptivity_needed(adapter)) \
				rtw_phydm_ability_ops(adapter, HAL_PHYDM_FUNC_SET, ODM_BB_ADAPTIVITY); \
		} while (0)

#define rtw_phydm_func_clr(adapter, ability)	\
		rtw_phydm_ability_ops(adapter, HAL_PHYDM_FUNC_CLR, ability)

#define rtw_phydm_ability_backup(adapter)	\
		rtw_phydm_ability_ops(adapter, HAL_PHYDM_ABILITY_BK, 0)

#define rtw_phydm_ability_restore(adapter)	\
		rtw_phydm_ability_ops(adapter, HAL_PHYDM_ABILITY_RESTORE, 0)


static inline u32 rtw_phydm_ability_get(struct adapter *adapter)
{
	return rtw_phydm_ability_ops(adapter, HAL_PHYDM_ABILITY_GET, 0);
}


void rtw_odm_init_ic_type(struct adapter *adapter);

void rtw_odm_adaptivity_config_msg(void *sel, struct adapter *adapter);

bool rtw_odm_adaptivity_needed(struct adapter *adapter);
void rtw_odm_adaptivity_parm_msg(void *sel, struct adapter *adapter);
void rtw_odm_adaptivity_parm_set(struct adapter *adapter, s8 th_l2h_ini, s8 th_edcca_hl_diff, s8 th_l2h_ini_mode2, s8 th_edcca_hl_diff_mode2, u8 edcca_enable);
void rtw_odm_get_perpkt_rssi(void *sel, struct adapter *adapter);
void rtw_odm_acquirespinlock(struct adapter *adapter,	enum rt_spinlock_type type);
void rtw_odm_releasespinlock(struct adapter *adapter,	enum rt_spinlock_type type);

u8 rtw_odm_get_dfs_domain(struct adapter *adapter);
u8 rtw_odm_dfs_domain_unknown(struct adapter *adapter);
void rtw_odm_parse_rx_phy_status_chinfo(union recv_frame *rframe, u8 *phys);

#endif /* __RTW_ODM_H__ */
