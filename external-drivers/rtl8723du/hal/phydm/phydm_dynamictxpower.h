/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDMDYNAMICTXPOWER_H__
#define    __PHYDMDYNAMICTXPOWER_H__

/*#define DYNAMIC_TXPWR_VERSION	"1.0"*/
/*#define DYNAMIC_TXPWR_VERSION	"1.3" */ /*2015.08.26, Add 8814 Dynamic TX power*/
#define DYNAMIC_TXPWR_VERSION	"1.4" /*2015.11.06, Add CE 8821A Dynamic TX power*/

#define		TX_POWER_NEAR_FIELD_THRESH_LVL2	74
#define		TX_POWER_NEAR_FIELD_THRESH_LVL1	60

#define		tx_high_pwr_level_normal		0
#define		tx_high_pwr_level_level1		1
#define		tx_high_pwr_level_level2		2

#define		tx_high_pwr_level_bt1			3
#define		tx_high_pwr_level_bt2			4
#define		tx_high_pwr_level_15			5
#define		tx_high_pwr_level_35			6
#define		tx_high_pwr_level_50			7
#define		tx_high_pwr_level_70			8
#define		tx_high_pwr_level_100			9

void
phydm_dynamic_tx_power_init(
	void					*p_dm_void
);

void
odm_dynamic_tx_power_restore_power_index(
	void					*p_dm_void
);

void
odm_dynamic_tx_power_nic(
	void					*p_dm_void
);

void
odm_dynamic_tx_power_save_power_index(
	void					*p_dm_void
);

void
odm_dynamic_tx_power_write_power_index(
	void					*p_dm_void,
	u8		value);

void
odm_dynamic_tx_power_8821(
	void					*p_dm_void,
	u8					*p_desc,
	u8					mac_id
);

void
odm_dynamic_tx_power(
	void					*p_dm_void
);

#endif
