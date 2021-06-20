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

#include "mp_precomp.h"
#include "../../phydm_precomp.h"

/*---------------------------Define Local Constant---------------------------*/
/* 2010/04/25 MH Define the max tx power tracking tx agc power. */
#define		ODM_TXPWRTRACK_MAX_IDX8821A		6

/*---------------------------Define Local Constant---------------------------*/

/* 3 ============================================================
 * 3 Tx Power Tracking
 * 3 ============================================================ */
void halrf_rf_lna_setting_8821a(
		struct dm_struct	*dm,
		enum halrf_lna_set type
)
{
	/*phydm_disable_lna*/
	if (type == HALRF_LNA_DISABLE) {
		odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0x80000, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, 0x30, 0xfffff, 0x18000);	/*select Rx mode*/
		odm_set_rf_reg(dm, RF_PATH_A, 0x31, 0xfffff, 0x0002f);
		odm_set_rf_reg(dm, RF_PATH_A, 0x32, 0xfffff, 0xfb09b);	/*disable LNA*/
		odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0x80000, 0x0);
	} else if (type == HALRF_LNA_ENABLE) {
		odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0x80000, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, 0x30, 0xfffff, 0x18000);	/*select Rx mode*/
		odm_set_rf_reg(dm, RF_PATH_A, 0x31, 0xfffff, 0x0002f);
		odm_set_rf_reg(dm, RF_PATH_A, 0x32, 0xfffff, 0xfb0bb);	/*disable LNA*/
		odm_set_rf_reg(dm, RF_PATH_A, 0xef, 0x80000, 0x0);
	}
}
void
odm_tx_pwr_track_set_pwr8821a(
	void		*dm_void,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct _ADAPTER		*adapter = dm->adapter;
	PHAL_DATA_TYPE	hal_data = GET_HAL_DATA(adapter);

	u8			pwr_tracking_limit = 26; /* +1.0dB */
	u8			tx_rate = 0xFF;
	u8			final_ofdm_swing_index = 0;
	u8			final_cck_swing_index = 0;
	u8			i = 0;
	u32			final_bb_swing_idx[1];
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);
	struct _hal_rf_ *rf = &(dm->rf_table);

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = adapter->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			if (dm->number_linked_client != 0)
				tx_rate = hw_rate_to_m_rate(dm->tx_rate);
			else
				tx_rate = rf->p_rate_index;
#endif
		} else   /*force rate*/
			tx_rate = (u8)rate;
	}

	//PHYDM_DBG(dm, DBG_COMP_MCC, "Power Tracking tx_rate=0x%X\n", tx_rate);
	//PHYDM_DBG(dm, DBG_COMP_MCC, "===>odm_tx_pwr_track_set_pwr8821a\n");

	if (tx_rate != 0xFF) {
		/* 2 CCK */
		if (((tx_rate >= MGN_1M) && (tx_rate <= MGN_5_5M)) || (tx_rate == MGN_11M))
			pwr_tracking_limit = 32; /* +4dB */
		/* 2 OFDM */
		else if ((tx_rate >= MGN_6M) && (tx_rate <= MGN_48M))
			pwr_tracking_limit = 30; /* +3dB */
		else if (tx_rate == MGN_54M)
			pwr_tracking_limit = 28; /* +2dB */
		/* 2 HT */
		else if ((tx_rate >= MGN_MCS0) && (tx_rate <= MGN_MCS2)) /* QPSK/BPSK */
			pwr_tracking_limit = 34; /* +5dB */
		else if ((tx_rate >= MGN_MCS3) && (tx_rate <= MGN_MCS4)) /* 16QAM */
			pwr_tracking_limit = 30; /* +3dB */
		else if ((tx_rate >= MGN_MCS5) && (tx_rate <= MGN_MCS7)) /* 64QAM */
			pwr_tracking_limit = 28; /* +2dB */

		/* 2 VHT */
		else if ((tx_rate >= MGN_VHT1SS_MCS0) && (tx_rate <= MGN_VHT1SS_MCS2)) /* QPSK/BPSK */
			pwr_tracking_limit = 34; /* +5dB */
		else if ((tx_rate >= MGN_VHT1SS_MCS3) && (tx_rate <= MGN_VHT1SS_MCS4)) /* 16QAM */
			pwr_tracking_limit = 30; /* +3dB */
		else if ((tx_rate >= MGN_VHT1SS_MCS5) && (tx_rate <= MGN_VHT1SS_MCS6)) /* 64QAM */
			pwr_tracking_limit = 28; /* +2dB */
		else if (tx_rate == MGN_VHT1SS_MCS7) /* 64QAM */
			pwr_tracking_limit = 26; /* +1dB */
		else if (tx_rate == MGN_VHT1SS_MCS8) /* 256QAM */
			pwr_tracking_limit = 24; /* +0dB */
		else if (tx_rate == MGN_VHT1SS_MCS9) /* 256QAM */
			pwr_tracking_limit = 22; /* -1dB */

		else
			pwr_tracking_limit = 24;
	}
	//PHYDM_DBG(dm, DBG_COMP_MCC, "tx_rate=0x%x, pwr_tracking_limit=%d\n", tx_rate, pwr_tracking_limit);

	if (method == BBSWING) {
		if (rf_path == RF_PATH_A) {
			final_bb_swing_idx[RF_PATH_A] = (dm->rf_calibrate_info.OFDM_index[RF_PATH_A] > pwr_tracking_limit) ? pwr_tracking_limit : dm->rf_calibrate_info.OFDM_index[RF_PATH_A];
			/*PHYDM_DBG(dm, DBG_COMP_MCC, "dm->rf_calibrate_info.OFDM_index[RF_PATH_A]=%d, dm->RealBbSwingIdx[RF_PATH_A]=%d\n",
				dm->rf_calibrate_info.OFDM_index[RF_PATH_A], final_bb_swing_idx[RF_PATH_A]);*/

			odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_bb_swing_idx[RF_PATH_A]]);
		}
	} else if (method == MIX_MODE) {
		/*PHYDM_DBG(dm, DBG_COMP_MCC, "cali_info->default_ofdm_index=%d, cali_info->absolute_ofdm_swing_idx[rf_path]=%d, rf_path = %d\n",
			cali_info->default_ofdm_index, cali_info->absolute_ofdm_swing_idx[rf_path], rf_path);*/

		final_cck_swing_index = cali_info->default_cck_index + cali_info->absolute_ofdm_swing_idx[rf_path];
		final_ofdm_swing_index = cali_info->default_ofdm_index + cali_info->absolute_ofdm_swing_idx[rf_path];

		if (rf_path == RF_PATH_A) {
			if (final_ofdm_swing_index > pwr_tracking_limit) {   /*BBSwing higher then Limit*/
				cali_info->remnant_cck_swing_idx = final_cck_swing_index - pwr_tracking_limit;
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index - pwr_tracking_limit;

				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[pwr_tracking_limit]);

				cali_info->modify_tx_agc_flag_path_a = true;

				phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_A);

				//PHYDM_DBG(dm, DBG_COMP_MCC, "******Path_A Over BBSwing Limit, pwr_tracking_limit = %d, Remnant tx_agc value = %d\n", pwr_tracking_limit, cali_info->remnant_ofdm_swing_idx[rf_path]);
			} else if (final_ofdm_swing_index <= 0) {
				cali_info->remnant_cck_swing_idx = final_cck_swing_index;
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index;

				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[0]);

				cali_info->modify_tx_agc_flag_path_a = true;

				phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_A);

				//PHYDM_DBG(dm, DBG_COMP_MCC, "******Path_A Lower then BBSwing lower bound  0, Remnant tx_agc value = %d\n", cali_info->remnant_ofdm_swing_idx[rf_path]);
			} else {
				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);

				//PHYDM_DBG(dm, DBG_COMP_MCC, "******Path_A Compensate with BBSwing, final_ofdm_swing_index = %d\n", final_ofdm_swing_index);

				if (cali_info->modify_tx_agc_flag_path_a) { /*If tx_agc has changed, reset tx_agc again*/
					cali_info->remnant_cck_swing_idx = 0;
					cali_info->remnant_ofdm_swing_idx[rf_path] = 0;

					phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_A);

					cali_info->modify_tx_agc_flag_path_a = false;

					PHYDM_DBG(dm, DBG_COMP_MCC, "******Path_A dm->Modify_TxAGC_Flag = false\n");
				}
			}
		}
	} else
		return;
}	/* odm_TxPwrTrackSetPwr88E */

void
get_delta_swing_table_8821a(
	void		*dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct _ADAPTER        *adapter		 = dm->adapter;
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);
	struct _hal_rf_ *rf = &(dm->rf_table);
	HAL_DATA_TYPE	*hal_data		 = GET_HAL_DATA(adapter);
	u8			tx_rate			= 0xFF;
	u8	channel		 = *dm->channel;

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = adapter->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			if (dm->number_linked_client != 0)
				tx_rate = hw_rate_to_m_rate(dm->tx_rate);
			else
				tx_rate = rf->p_rate_index;
#endif
		} else   /*force rate*/
			tx_rate = (u8)rate;
	}

	//PHYDM_DBG(dm, DBG_COMP_MCC, "Power Tracking tx_rate=0x%X\n", tx_rate);


	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(tx_rate)) {
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
		*temperature_up_a   = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_a = (u8 *)delta_swing_table_idx_2ga_n_8188e;
		*temperature_up_b   = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_b = (u8 *)delta_swing_table_idx_2ga_n_8188e;
	}

	return;
}

void configure_txpower_track_8821a(
	struct txpwrtrack_cfg	*config
)
{
	config->swing_table_size_cck = TXSCALE_TABLE_SIZE;
	config->swing_table_size_ofdm = TXSCALE_TABLE_SIZE;
	config->threshold_iqk = IQK_THRESHOLD;
	config->average_thermal_num = AVG_THERMAL_NUM_8812A;
	config->rf_path_count = MAX_PATH_NUM_8821A;
	config->thermal_reg_addr = RF_T_METER_8812A;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8821a;
	config->do_iqk = do_iqk_8821a;
	config->phy_lc_calibrate = halrf_lck_trigger;
	config->get_delta_swing_table = get_delta_swing_table_8821a;
}

void
phy_lc_calibrate_8821a(
	void		*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;

	phy_lc_calibrate_8812a(dm);
}
