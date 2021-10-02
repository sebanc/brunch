// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#include "mp_precomp.h"
#include "phydm_precomp.h"

void
odm_config_rf_reg_8723d(
	struct PHY_DM_STRUCT				*p_dm,
	u32					addr,
	u32					data,
	enum rf_path     RF_PATH,
	u32				    reg_addr
)
{
	if (addr == 0xfe || addr == 0xffe) {
		ODM_sleep_ms(50);
	} else {
		odm_set_rf_reg(p_dm, RF_PATH, reg_addr, RFREGOFFSETMASK, data);
		/* Add 1us delay between BB/RF register setting. */
		ODM_delay_us(1);
	}
}


void
odm_config_rf_radio_a_8723d(
	struct PHY_DM_STRUCT				*p_dm,
	u32					addr,
	u32					data
)
{
	u32  content = 0x1000; /* RF_Content: radioa_txt */
	u32	maskfor_phy_set = (u32)(content & 0xE000);

	odm_config_rf_reg_8723d(p_dm, addr, data, RF_PATH_A, addr | maskfor_phy_set);

	PHYDM_DBG(p_dm, ODM_COMP_INIT, ("===> odm_config_rf_with_header_file: [RadioA] %08X %08X\n", addr, data));
}

void
odm_config_rf_radio_b_8723d(
	struct PHY_DM_STRUCT				*p_dm,
	u32					addr,
	u32					data
)
{
	u32  content = 0x1001; /* RF_Content: radiob_txt */
	u32	maskfor_phy_set = (u32)(content & 0xE000);

	odm_config_rf_reg_8723d(p_dm, addr, data, RF_PATH_B, addr | maskfor_phy_set);

	PHYDM_DBG(p_dm, ODM_COMP_INIT, ("===> odm_config_rf_with_header_file: [RadioB] %08X %08X\n", addr, data));

}

void
odm_config_mac_8723d(
	struct PHY_DM_STRUCT	*p_dm,
	u32		addr,
	u8		data
)
{
	odm_write_1byte(p_dm, addr, data);
	PHYDM_DBG(p_dm, ODM_COMP_INIT, ("===> odm_config_mac_with_header_file: [MAC_REG] %08X %08X\n", addr, data));
}

void
odm_config_bb_agc_8723d(
	struct PHY_DM_STRUCT	*p_dm,
	u32		addr,
	u32		bitmask,
	u32		data
)
{
	odm_set_bb_reg(p_dm, addr, bitmask, data);
	/* Add 1us delay between BB/RF register setting. */
	ODM_delay_us(1);

	PHYDM_DBG(p_dm, ODM_COMP_INIT, ("===> odm_config_bb_with_header_file: [AGC_TAB] %08X %08X\n", addr, data));
}

void
odm_config_bb_phy_reg_pg_8723d(
	struct PHY_DM_STRUCT	*p_dm,
	u32		band,
	u32		rf_path,
	u32		tx_num,
	u32		addr,
	u32		bitmask,
	u32		data
)
{
	if (addr == 0xfe || addr == 0xffe)
		ODM_sleep_ms(50);
	else {
		phy_store_tx_power_by_rate(p_dm->adapter, band, rf_path, tx_num, addr, bitmask, data);
	}
	PHYDM_DBG(p_dm, ODM_COMP_INIT, ("===> odm_config_bb_with_header_file: [PHY_REG] %08X %08X %08X\n", addr, bitmask, data));
}

void
odm_config_bb_phy_8723d(
	struct PHY_DM_STRUCT	*p_dm,
	u32		addr,
	u32		bitmask,
	u32		data
)
{
	/*dbg_print("odm_config_bb_phy_8723d(), addr = 0x%x, data = 0x%x\n", addr, data);*/
	if (addr == 0xfe)
		ODM_sleep_ms(50);
	else if (addr == 0xfd)
		ODM_delay_ms(5);
	else if (addr == 0xfc)
		ODM_delay_ms(1);
	else if (addr == 0xfb)
		ODM_delay_us(50);
	else if (addr == 0xfa)
		ODM_delay_us(5);
	else if (addr == 0xf9)
		ODM_delay_us(1);
	else
		odm_set_bb_reg(p_dm, addr, bitmask, data);

	/* Add 1us delay between BB/RF register setting. */
	ODM_delay_us(1);
	PHYDM_DBG(p_dm, ODM_COMP_INIT, ("===> odm_config_bb_with_header_file: [PHY_REG] %08X %08X\n", addr, data));
}

void
odm_config_bb_txpwr_lmt_8723d(
	struct PHY_DM_STRUCT	*p_dm,
	u8		*regulation,
	u8		*band,
	u8		*bandwidth,
	u8		*rate_section,
	u8		*rf_path,
	u8	*channel,
	u8		*power_limit
)
{
	phy_set_tx_power_limit(p_dm, regulation, band,
		       bandwidth, rate_section, rf_path, channel, power_limit);
}
