// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"

#define READ_AND_CONFIG_MP(ic, txt) (odm_read_and_config_mp_##ic##txt(p_dm))
#define READ_AND_CONFIG_TC(ic, txt) (odm_read_and_config_tc_##ic##txt(p_dm))


#if (PHYDM_TESTCHIP_SUPPORT == 1)
#define READ_AND_CONFIG(ic, txt) do {\
		if (p_dm->is_mp_chip)\
			READ_AND_CONFIG_MP(ic, txt);\
		else\
			READ_AND_CONFIG_TC(ic, txt);\
	} while (0)
#else
#define READ_AND_CONFIG     READ_AND_CONFIG_MP
#endif

#define GET_VERSION_MP(ic, txt)		(odm_get_version_mp_##ic##txt())
#define GET_VERSION_TC(ic, txt)		(odm_get_version_tc_##ic##txt())

#if (PHYDM_TESTCHIP_SUPPORT == 1)
	#define GET_VERSION(ic, txt) (p_dm->is_mp_chip ? GET_VERSION_MP(ic, txt) : GET_VERSION_TC(ic, txt))
#else
	#define GET_VERSION(ic, txt) GET_VERSION_MP(ic, txt)
#endif

enum hal_status
odm_config_rf_with_header_file(
	struct PHY_DM_STRUCT		*p_dm,
	enum odm_rf_config_type		config_type,
	u8			e_rf_path
)
{
	enum hal_status	result = HAL_STATUS_SUCCESS;

	PHYDM_DBG(p_dm, ODM_COMP_INIT,
		("===>odm_config_rf_with_header_file (%s)\n", (p_dm->is_mp_chip) ? "MPChip" : "TestChip"));
	PHYDM_DBG(p_dm, ODM_COMP_INIT,
		("support_platform: 0x%X, support_interface: 0x%X, board_type: 0x%X\n",
		p_dm->support_platform, p_dm->support_interface, p_dm->board_type));

	/* 1 AP doesn't use PHYDM power tracking table in these ICs */
	if (p_dm->support_ic_type == ODM_RTL8723D) {
		if (config_type == CONFIG_RF_RADIO) {
			if (e_rf_path == RF_PATH_A)
				READ_AND_CONFIG_MP(8723d, _radioa);
		} else if (config_type == CONFIG_RF_TXPWR_LMT)
			READ_AND_CONFIG_MP(8723d, _txpwr_lmt);
	}
	/* 1 All platforms support */
	if (config_type == CONFIG_RF_RADIO) {
		if (p_dm->fw_offload_ability & PHYDM_PHY_PARAM_OFFLOAD) {

			result = phydm_set_reg_by_fw(p_dm,
							PHYDM_HALMAC_CMD_END,
							0,
							0,
							0,
							(enum rf_path)0,
							0);
			PHYDM_DBG(p_dm, ODM_COMP_INIT,
				("rf param offload end!result = %d", result));
		}
	}

	return result;
}

enum hal_status
odm_config_rf_with_tx_pwr_track_header_file(
	struct PHY_DM_STRUCT		*p_dm
)
{
	PHYDM_DBG(p_dm, ODM_COMP_INIT,
		("===>odm_config_rf_with_tx_pwr_track_header_file (%s)\n", (p_dm->is_mp_chip) ? "MPChip" : "TestChip"));
	PHYDM_DBG(p_dm, ODM_COMP_INIT,
		("support_platform: 0x%X, support_interface: 0x%X, board_type: 0x%X\n",
		p_dm->support_platform, p_dm->support_interface, p_dm->board_type));


	/* 1 AP doesn't use PHYDM power tracking table in these ICs */
	if (p_dm->support_ic_type == ODM_RTL8723D) {
		if (p_dm->support_interface == ODM_ITRF_PCIE)
			READ_AND_CONFIG_MP(8723d, _txpowertrack_pcie);
		else if (p_dm->support_interface == ODM_ITRF_USB)
			READ_AND_CONFIG_MP(8723d, _txpowertrack_usb);
		else if (p_dm->support_interface == ODM_ITRF_SDIO)
			READ_AND_CONFIG_MP(8723d, _txpowertrack_sdio);

		READ_AND_CONFIG_MP(8723d, _txxtaltrack);
	}
	/* 1 All platforms support */
	return HAL_STATUS_SUCCESS;
}

enum hal_status
odm_config_bb_with_header_file(
	struct PHY_DM_STRUCT		*p_dm,
	enum odm_bb_config_type		config_type
)
{
	enum hal_status	result = HAL_STATUS_SUCCESS;

	/* 1 AP doesn't use PHYDM initialization in these ICs */
	if (p_dm->support_ic_type == ODM_RTL8723D) {
		if (config_type == CONFIG_BB_PHY_REG)
			READ_AND_CONFIG_MP(8723d, _phy_reg);
		else if (config_type == CONFIG_BB_AGC_TAB)
			READ_AND_CONFIG_MP(8723d, _agc_tab);
		else if (config_type == CONFIG_BB_PHY_REG_PG)
			READ_AND_CONFIG_MP(8723d, _phy_reg_pg);
	}
	/* 1 All platforms support */
	if (config_type == CONFIG_BB_PHY_REG || config_type == CONFIG_BB_AGC_TAB)
		if (p_dm->fw_offload_ability & PHYDM_PHY_PARAM_OFFLOAD) {

			result = phydm_set_reg_by_fw(p_dm,
								PHYDM_HALMAC_CMD_END,
								0,
								0,
								0,
								(enum rf_path)0,
								0);
			PHYDM_DBG(p_dm, ODM_COMP_INIT,
				("phy param offload end!result = %d", result));
		}

	return result;
}

enum hal_status
odm_config_mac_with_header_file(
	struct PHY_DM_STRUCT	*p_dm
)
{
	enum hal_status	result = HAL_STATUS_SUCCESS;
	PHYDM_DBG(p_dm, ODM_COMP_INIT,
		("===>odm_config_mac_with_header_file (%s)\n", (p_dm->is_mp_chip) ? "MPChip" : "TestChip"));
	PHYDM_DBG(p_dm, ODM_COMP_INIT,
		("support_platform: 0x%X, support_interface: 0x%X, board_type: 0x%X\n",
		p_dm->support_platform, p_dm->support_interface, p_dm->board_type));

	/* 1 AP doesn't use PHYDM initialization in these ICs */
	if (p_dm->support_ic_type == ODM_RTL8723D)
		READ_AND_CONFIG_MP(8723d, _mac_reg);
	/* 1 All platforms support */
	if (p_dm->fw_offload_ability & PHYDM_PHY_PARAM_OFFLOAD) {

		result = phydm_set_reg_by_fw(p_dm,
							PHYDM_HALMAC_CMD_END,
							0,
							0,
							0,
							(enum rf_path)0,
							0);
		PHYDM_DBG(p_dm, ODM_COMP_INIT,
			("mac param offload end!result = %d", result));
	}

	return result;
}

u32
odm_get_hw_img_version(
	struct PHY_DM_STRUCT	*p_dm
)
{
	u32  version = 0;

	/* 1 AP doesn't use PHYDM initialization in these ICs */
	if (p_dm->support_ic_type == ODM_RTL8723D)
		version = GET_VERSION_MP(8723d, _mac_reg);
	/*1 All platforms support*/

	return version;
}


u32
query_phydm_trx_capability(
	struct PHY_DM_STRUCT					*p_dm
)
{
	u32 value32 = 0xFFFFFFFF;

	return value32;
}

u32
query_phydm_stbc_capability(
	struct PHY_DM_STRUCT					*p_dm
)
{
	u32 value32 = 0xFFFFFFFF;

	return value32;
}

u32
query_phydm_ldpc_capability(
	struct PHY_DM_STRUCT					*p_dm
)
{
	u32 value32 = 0xFFFFFFFF;

	return value32;
}

u32
query_phydm_txbf_parameters(
	struct PHY_DM_STRUCT					*p_dm
)
{
	u32 value32 = 0xFFFFFFFF;

	return value32;
}

u32
query_phydm_txbf_capability(
	struct PHY_DM_STRUCT					*p_dm
)
{
	u32 value32 = 0xFFFFFFFF;

	return value32;
}
