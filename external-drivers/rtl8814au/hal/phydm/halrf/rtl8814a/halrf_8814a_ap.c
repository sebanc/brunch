/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
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

#if !defined(__ECOS) && !defined(CONFIG_COMPAT_WIRELESS)
	#include "mp_precomp.h"
#else
	#include "../mp_precomp.h"
#endif
#include "../../phydm_precomp.h"



/*---------------------------Define Local Constant---------------------------*/
/* 2010/04/25 MH Define the max tx power tracking tx agc power. */
#define		ODM_TXPWRTRACK_MAX_IDX8814A		6

/*---------------------------Define Local Constant---------------------------*/


/* 3============================================================
 * 3 Tx Power Tracking
 * 3============================================================ */

u8
check_rf_gain_offset(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path
)
{
	s8	upper_bound = 10, lower_bound = -5; /* 4'b1010 = 10 */
	s8	final_rf_index = 0;
	boolean	is_positive = false;
	u32	bit_mask = 0;
	u8	final_ofdm_swing_index = 0, tx_scaling_upper_bound = 28, tx_scaling_lower_bound = 4;/* upper bound +2dB, lower bound -9dB */
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);

	if (method == MIX_MODE) {	/* normal Tx power tracking */
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "is 8814 MP chip\n");
		bit_mask = BIT(19);
		cali_info->absolute_ofdm_swing_idx[rf_path] = cali_info->absolute_ofdm_swing_idx[rf_path] + cali_info->kfree_offset[rf_path];

		if (cali_info->absolute_ofdm_swing_idx[rf_path] >= 0)				/* check if RF_Index is positive or not */
			is_positive = true;
		else
			is_positive = false;

		odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, is_positive);

		bit_mask = BIT(18) | BIT(17) | BIT(16) | BIT(15);
		final_rf_index = cali_info->absolute_ofdm_swing_idx[rf_path] / 2;		/*TxBB 1 step equal 1dB, BB swing 1step equal 0.5dB*/

	}

	if (final_rf_index > upper_bound) {	/* Upper bound = 10dB, if more htan upper bound, then move to bb swing max = +2dB */
		odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, upper_bound);	/* set RF Reg0x55 per path */

		final_ofdm_swing_index = cali_info->default_ofdm_index + (cali_info->absolute_ofdm_swing_idx[rf_path] - (upper_bound << 1));

		if (final_ofdm_swing_index > tx_scaling_upper_bound)	/*	bb swing upper bound = +2dB */
			final_ofdm_swing_index = tx_scaling_upper_bound;

		return final_ofdm_swing_index;
	} else if (final_rf_index < lower_bound) {	/* lower bound = -5dB */
		odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, (-1) * (lower_bound));	/* set RF Reg0x55 per path */

		final_ofdm_swing_index = cali_info->default_ofdm_index - ((lower_bound << 1) - cali_info->absolute_ofdm_swing_idx[rf_path]);

		if (final_ofdm_swing_index < tx_scaling_lower_bound)	/* bb swing lower bound = -10dB */
			final_ofdm_swing_index = tx_scaling_lower_bound;
		return final_ofdm_swing_index;
	} else {	/* normal case */

		if (is_positive == true)
			odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, final_rf_index);	/* set RF Reg0x55 per path */
		else
			odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, (-1) * final_rf_index);	/* set RF Reg0x55 per path */

		final_ofdm_swing_index = cali_info->default_ofdm_index + (cali_info->absolute_ofdm_swing_idx[rf_path]) % 2;
		return final_ofdm_swing_index;
	}

	return false;
}


void
odm_tx_pwr_track_set_pwr8814a(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
)
{
	u8			final_ofdm_swing_index = 0;
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);

	if (method == MIX_MODE) {
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "dm->default_ofdm_index=%d, dm->absolute_ofdm_swing_idx[rf_path]=%d, rf_path = %d\n",
			cali_info->default_ofdm_index, cali_info->absolute_ofdm_swing_idx[rf_path], rf_path);

		final_ofdm_swing_index = check_rf_gain_offset(dm, MIX_MODE, rf_path);
	} else if (method == TSSI_MODE)
		odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, BIT(18) | BIT(17) | BIT(16) | BIT(15), 0);
	else if (method == BBSWING) {	/* use for mp driver clean power tracking status */
		cali_info->absolute_ofdm_swing_idx[rf_path] = cali_info->absolute_ofdm_swing_idx[rf_path] + cali_info->kfree_offset[rf_path];

		final_ofdm_swing_index = cali_info->default_ofdm_index + (cali_info->absolute_ofdm_swing_idx[rf_path]);

		odm_set_rf_reg(dm, rf_path, REG_RF_TX_GAIN_OFFSET, BIT(18) | BIT(17) | BIT(16) | BIT(15), 0);
	}

	if ((method == MIX_MODE) || (method == BBSWING)) {
		switch (rf_path) {
		case RF_PATH_A:

			odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);	/* set BBswing */

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"******Path_A Compensate with BBSwing, final_ofdm_swing_index = %d\n", final_ofdm_swing_index);
			break;

		case RF_PATH_B:

			odm_set_bb_reg(dm, REG_B_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);	/* set BBswing */

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"******Path_B Compensate with BBSwing, final_ofdm_swing_index = %d\n", final_ofdm_swing_index);
			break;

		case RF_PATH_C:

			odm_set_bb_reg(dm, REG_C_TX_SCALE_JAGUAR2, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);	/* set BBswing */

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"******Path_C Compensate with BBSwing, final_ofdm_swing_index = %d\n", final_ofdm_swing_index);
			break;

		case RF_PATH_D:

			odm_set_bb_reg(dm, REG_D_TX_SCALE_JAGUAR2, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);	/* set BBswing */

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"******Path_D Compensate with BBSwing, final_ofdm_swing_index = %d\n", final_ofdm_swing_index);
			break;

		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Wrong path name!!!!\n");

			break;
		}
	}
	return;
}	/* odm_tx_pwr_track_set_pwr8814a */

void
get_delta_swing_table_8814a(
	struct dm_struct			*dm,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
)
{
	struct dm_rf_calibration_struct	*cali_info	= &(dm->rf_calibrate_info);
	u16			rate				= *(dm->forced_data_rate);
	u8			channel			= *(dm->channel);

	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(rate)) {
			*temperature_up_a   = cali_info->delta_swing_table_idx_2g_cck_a_p;
			*temperature_down_a = cali_info->delta_swing_table_idx_2g_cck_a_n;
			*temperature_up_b   = cali_info->delta_swing_table_idx_2g_cck_b_p;
			*temperature_down_b = cali_info->delta_swing_table_idx_2g_cck_b_n;
		} else {
			*temperature_up_a   = cali_info->delta_swing_table_idx_2ga_p;
			*temperature_down_a = cali_info->delta_swing_table_idx_2ga_n;
			*temperature_up_b   = cali_info->delta_swing_table_idx_2gb_p;
			*temperature_down_b = cali_info->delta_swing_table_idx_2gb_n;
		}
	} else if (36 <= channel && channel <= 64) {
		*temperature_up_a   = cali_info->delta_swing_table_idx_5ga_p[0];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[0];
		*temperature_up_b   = cali_info->delta_swing_table_idx_5gb_p[0];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[0];
	} else if (100 <= channel && channel <= 144) {
		*temperature_up_a   = cali_info->delta_swing_table_idx_5ga_p[1];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[1];
		*temperature_up_b   = cali_info->delta_swing_table_idx_5gb_p[1];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[1];
	} else if (149 <= channel && channel <= 177) {
		*temperature_up_a   = cali_info->delta_swing_table_idx_5ga_p[2];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[2];
		*temperature_up_b   = cali_info->delta_swing_table_idx_5gb_p[2];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[2];
	} else {
		*temperature_up_a   = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_a = (u8 *)delta_swing_table_idx_2ga_n_default;
		*temperature_up_b   = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_b = (u8 *)delta_swing_table_idx_2ga_n_default;
	}

	return;
}


void
get_delta_swing_table_8814a_path_cd(
	struct dm_struct			*dm,
	u8 **temperature_up_c,
	u8 **temperature_down_c,
	u8 **temperature_up_d,
	u8 **temperature_down_d
)
{
	struct dm_rf_calibration_struct	*cali_info	= &(dm->rf_calibrate_info);
	u16			rate				= *(dm->forced_data_rate);
	u8			channel			= *(dm->channel);

	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(rate)) {
			*temperature_up_c  = cali_info->delta_swing_table_idx_2g_cck_c_p;
			*temperature_down_c = cali_info->delta_swing_table_idx_2g_cck_c_n;
			*temperature_up_d   = cali_info->delta_swing_table_idx_2g_cck_d_p;
			*temperature_down_d = cali_info->delta_swing_table_idx_2g_cck_d_n;
		} else {
			*temperature_up_c   = cali_info->delta_swing_table_idx_2gc_p;
			*temperature_down_c = cali_info->delta_swing_table_idx_2gc_n;
			*temperature_up_d   = cali_info->delta_swing_table_idx_2gd_p;
			*temperature_down_d = cali_info->delta_swing_table_idx_2gd_n;
		}
	} else if (36 <= channel && channel <= 64) {
		*temperature_up_c   = cali_info->delta_swing_table_idx_5gc_p[0];
		*temperature_down_c = cali_info->delta_swing_table_idx_5gc_n[0];
		*temperature_up_d   = cali_info->delta_swing_table_idx_5gd_p[0];
		*temperature_down_d = cali_info->delta_swing_table_idx_5gd_n[0];
	} else if (100 <= channel && channel <= 144) {
		*temperature_up_c   = cali_info->delta_swing_table_idx_5gc_p[1];
		*temperature_down_c = cali_info->delta_swing_table_idx_5gc_n[1];
		*temperature_up_d   = cali_info->delta_swing_table_idx_5gd_p[1];
		*temperature_down_d = cali_info->delta_swing_table_idx_5gd_n[1];
	} else if (149 <= channel && channel <= 177) {
		*temperature_up_c   = cali_info->delta_swing_table_idx_5gc_p[2];
		*temperature_down_c = cali_info->delta_swing_table_idx_5gc_n[2];
		*temperature_up_d   = cali_info->delta_swing_table_idx_5gd_p[2];
		*temperature_down_d = cali_info->delta_swing_table_idx_5gd_n[2];
	} else {
		*temperature_up_c   = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_c = (u8 *)delta_swing_table_idx_2ga_n_default;
		*temperature_up_d   = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_d = (u8 *)delta_swing_table_idx_2ga_n_default;
	}

	return;
}


void configure_txpower_track_8814a(
	struct txpwrtrack_cfg	*config
)
{
	config->swing_table_size_cck = ODM_CCK_TABLE_SIZE;
	config->swing_table_size_ofdm = ODM_OFDM_TABLE_SIZE;
	config->threshold_iqk = 8;
	config->average_thermal_num = AVG_THERMAL_NUM_8814A;
	config->rf_path_count = MAX_PATH_NUM_8814A;
	config->thermal_reg_addr = RF_T_METER_8814A;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8814a;
	config->phy_lc_calibrate = halrf_lck_trigger;
	config->do_iqk = do_iqk_8814a;
	config->get_delta_swing_table = get_delta_swing_table_8814a;
	config->get_delta_swing_table8814only = get_delta_swing_table_8814a_path_cd;
}



/* 1 7.	IQK */
void
_phy_save_adda_registers_8814a(
	struct dm_struct		*dm,
	u32		*adda_reg,
	u32		*adda_backup,
	u32		register_num
)
{
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "Save ADDA parameters.\n");
	for (i = 0 ; i < register_num ; i++)
		adda_backup[i] = odm_get_bb_reg(dm, adda_reg[i], MASKDWORD);
}


void
_phy_save_mac_registers_8814a(
	struct dm_struct		*dm,
	u32		*mac_reg,
	u32		*mac_backup
)
{
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "Save MAC parameters.\n");
	for (i = 0 ; i < (IQK_MAC_REG_NUM - 1); i++)
		mac_backup[i] = odm_read_1byte(dm, mac_reg[i]);
	mac_backup[i] = odm_read_4byte(dm, mac_reg[i]);

}


void
_phy_reload_adda_registers_8814a(
	struct dm_struct		*dm,
	u32		*adda_reg,
	u32		*adda_backup,
	u32		regiester_num
)
{
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "Reload ADDA power saving parameters !\n");
	for (i = 0 ; i < regiester_num; i++)
		odm_set_bb_reg(dm, adda_reg[i], MASKDWORD, adda_backup[i]);
}

void
_phy_reload_mac_registers_8814a(
	struct dm_struct		*dm,
	u32		*mac_reg,
	u32		*mac_backup
)
{
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "Reload MAC parameters !\n");
	for (i = 0 ; i < (IQK_MAC_REG_NUM - 1); i++)
		odm_write_1byte(dm, mac_reg[i], (u8)mac_backup[i]);
	odm_write_4byte(dm, mac_reg[i], mac_backup[i]);
}



void
_phy_mac_setting_calibration_8814a(
	struct dm_struct		*dm,
	u32		*mac_reg,
	u32		*mac_backup
)
{
	u32	i = 0;
	RF_DBG(dm, DBG_RF_IQK, "MAC settings for Calibration.\n");

	odm_write_1byte(dm, mac_reg[i], 0x3F);

	for (i = 1 ; i < (IQK_MAC_REG_NUM - 1); i++)
		odm_write_1byte(dm, mac_reg[i], (u8)(mac_backup[i] & (~BIT(3))));
	odm_write_1byte(dm, mac_reg[i], (u8)(mac_backup[i] & (~BIT(5))));

}


void
_phy_lc_calibrate_8814a(
	struct dm_struct		*dm,
	boolean		is2T
)
{
	u32	/*rf_amode=0, rf_bmode=0,*/ lc_cal = 0, tmp = 0;
	u32 cnt;

	/* Check continuous TX and Packet TX */
	u32	reg0x914 = odm_read_4byte(dm, REG_SINGLE_TONE_CONT_TX_JAGUAR);;

	/* Backup RF reg18. */

	if ((reg0x914 & 0x70000) == 0)
		odm_write_1byte(dm, REG_TXPAUSE_8812, 0xFF);

	/* 3 3. Read RF reg18 */
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);

	/* 3 4. Set LC calibration begin bit15 */
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, 0x8000, 0x1);

	ODM_delay_ms(100);

	for (cnt = 0; cnt < 100; cnt++) {
		if (odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, 0x8000) != 0x1)
			break;
		ODM_delay_ms(10);
	}
	RF_DBG(dm, DBG_RF_IQK, "retry cnt = %d\n", cnt);


	/* 3 Restore original situation */
	if ((reg0x914 & 70000) == 0)
		odm_write_1byte(dm, REG_TXPAUSE_8812, 0x00);

	/* Recover channel number */
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal);
}


void
phy_lc_calibrate_8814a(
	void		*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;

	_phy_lc_calibrate_8814a(dm, true);
}


void _phy_set_rf_path_switch_8814a(
	struct dm_struct		*dm,
	boolean		is_main,
	boolean		is2T
)
{
	if (is2T) {	/* 92C */
		if (is_main)
			odm_set_bb_reg(dm, REG_FPGA0_XB_RF_INTERFACE_OE, BIT(5) | BIT(6), 0x1);	/* 92C_Path_A */
		else
			odm_set_bb_reg(dm, REG_FPGA0_XB_RF_INTERFACE_OE, BIT(5) | BIT(6), 0x2);	/* BT */
	} else {		/* 88C */

		if (is_main)
			odm_set_bb_reg(dm, REG_FPGA0_XA_RF_INTERFACE_OE, BIT(8) | BIT(9), 0x2);	/* Main */
		else
			odm_set_bb_reg(dm, REG_FPGA0_XA_RF_INTERFACE_OE, BIT(8) | BIT(9), 0x1);	/* Aux */
	}
}
void phy_set_rf_path_switch_8814a(
	struct dm_struct		*dm,
	boolean		is_main
)
{
	/* HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter); */

#ifdef DISABLE_BB_RF
	return;
#endif

	{
		/* For 88C 1T1R */
		_phy_set_rf_path_switch_8814a(dm, is_main, false);
	}
}
