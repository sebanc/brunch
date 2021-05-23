/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__ODM_PRECOMP_H__
#define __ODM_PRECOMP_H__

#include "phydm_types.h"
#include "phydm_features.h"

#define		TEST_FALG___		1

/* 2 Config Flags and Structs - defined by each ODM type */

#define __PACK
#define __WLAN_ATTRIB_PACK__

/* 2 OutSrc Header Files */

#include "phydm.h"
#include "phydm_hwconfig.h"
#include "phydm_phystatus.h"
#include "phydm_debug.h"
#include "phydm_regdefine11ac.h"
#include "phydm_regdefine11n.h"
#include "phydm_interface.h"
#include "phydm_reg.h"

void
phy_set_tx_power_limit(
	struct PHY_DM_STRUCT	*p_dm,
	u8	*regulation,
	u8	*band,
	u8	*bandwidth,
	u8	*rate_section,
	u8	*rf_path,
	u8	*channel,
	u8	*power_limit
);

enum hal_status
rtw_phydm_fw_iqk(
	struct PHY_DM_STRUCT	*p_dm,
	u8 clear,
	u8 segment
);

enum hal_status
rtw_phydm_cfg_phy_para(
	struct PHY_DM_STRUCT	*p_dm,
	enum phydm_halmac_param config_type,
	u32 offset,
	u32 data,
	u32 mask,
	enum rf_path e_rf_path,
	u32 delay_time
);

#include "halhwimg8723d_bb.h"
#include "halhwimg8723d_mac.h"
#include "halhwimg8723d_rf.h"
#include "phydm_regconfig8723d.h"
#include "hal8723dreg.h"
#include "phydm_rtl8723d.h"
#include "halrf_8723d.h"
#include "version_rtl8723d.h"
#include "rtl8723d_hal.h"

#endif /* __ODM_PRECOMP_H__ */
