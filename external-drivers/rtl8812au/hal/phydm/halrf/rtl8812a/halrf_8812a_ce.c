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
#define ODM_TXPWRTRACK_MAX_IDX8812A 6

/*---------------------------Define Local Constant---------------------------*/

/* 3============================================================
 * 3 Tx Power Tracking
 * 3============================================================ */
void halrf_rf_lna_setting_8812a(struct dm_struct *dm, enum halrf_lna_set type)
{
	/*phydm_disable_lna*/
	if (type == HALRF_LNA_DISABLE) {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x30, 0xfffff, 0x18000); /*select Rx mode*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x31, 0xfffff, 0x3f7ff);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x32, 0xfffff, 0xc22bf); /*disable LNA*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x0);
		if (dm->rf_type > RF_1T1R) {
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x30, 0xfffff, 0x18000); /*select Rx mode*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x31, 0xfffff, 0x3f7ff);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x32, 0xfffff, 0xc22bf); /*disable LNA*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x0);
		}
	} else if (type == HALRF_LNA_ENABLE) {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x30, 0xfffff, 0x18000); /*select Rx mode*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x31, 0xfffff, 0x3f7ff);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x32, 0xfffff, 0xc26bf); /*disable LNA*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x0);
		if (dm->rf_type > RF_1T1R) {
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x30, 0xfffff, 0x18000); /*select Rx mode*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x31, 0xfffff, 0x3f7ff);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x32, 0xfffff, 0xc26bf); /*disable LNA*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x0);
		}
	}
}

void do_iqk_8812a(void *dm_void, u8 delta_thermal_index, u8 thermal_value,
		  u8 threshold)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	odm_reset_iqk_result(dm);
	dm->rf_calibrate_info.thermal_value_iqk = thermal_value;
	halrf_iqk_trigger(dm, false);
}

/*-----------------------------------------------------------------------------
 * Function:	odm_TxPwrTrackSetPwr88E()
 *
 * Overview:	88E change all channel tx power accordign to flag.
 *				OFDM & CCK are all different.
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	04/23/2012	MHC		Create version 0.
 *
 *---------------------------------------------------------------------------*/
void odm_tx_pwr_track_set_pwr8812a(void *dm_void, enum pwrtrack_method method,
				   u8 rf_path, u8 channel_mapped_index)
{
	u32 final_bb_swing_idx[2];
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _ADAPTER *adapter = dm->adapter;
	PHAL_DATA_TYPE hal_data = GET_HAL_DATA(adapter);

	u8 pwr_tracking_limit = 26; /* +1.0dB */
	u8 tx_rate = 0xFF;
	u8 final_ofdm_swing_index = 0;
	u8 final_cck_swing_index = 0;
	u8 i = 0;
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);
	struct _hal_rf_ *rf = &(dm->rf_table);

	if (*dm->mp_mode == true) {
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
		u16 rate = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = adapter->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			if (dm->number_linked_client != 0)
				tx_rate = hw_rate_to_m_rate(dm->tx_rate);
			else
				tx_rate = rf->p_rate_index;
#endif
		} else /*force rate*/
			tx_rate = (u8)rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Power Tracking tx_rate=0x%X\n",
	       tx_rate);
	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "===>%s\n", __func__);

	if (tx_rate != 0xFF) { /* 20130429 Mimic Modify High rate BBSwing Limit. */
		/* 2 CCK */
		if ((tx_rate >= MGN_1M && tx_rate <= MGN_5_5M) || tx_rate == MGN_11M)
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

		else if ((tx_rate >= MGN_MCS8) && (tx_rate <= MGN_MCS10)) /* QPSK/BPSK */
			pwr_tracking_limit = 34; /* +5dB */
		else if ((tx_rate >= MGN_MCS11) && (tx_rate <= MGN_MCS12)) /* 16QAM */
			pwr_tracking_limit = 30; /* +3dB */
		else if ((tx_rate >= MGN_MCS13) && (tx_rate <= MGN_MCS15)) /* 64QAM */
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

		else if ((tx_rate >= MGN_VHT2SS_MCS0) && (tx_rate <= MGN_VHT2SS_MCS2)) /* QPSK/BPSK */
			pwr_tracking_limit = 34; /* +5dB */
		else if ((tx_rate >= MGN_VHT2SS_MCS3) && (tx_rate <= MGN_VHT2SS_MCS4)) /* 16QAM */
			pwr_tracking_limit = 30; /* +3dB */
		else if ((tx_rate >= MGN_VHT2SS_MCS5) && (tx_rate <= MGN_VHT2SS_MCS6)) /* 64QAM */
			pwr_tracking_limit = 28; /* +2dB */
		else if (tx_rate == MGN_VHT2SS_MCS7) /* 64QAM */
			pwr_tracking_limit = 26; /* +1dB */
		else if (tx_rate == MGN_VHT2SS_MCS8) /* 256QAM */
			pwr_tracking_limit = 24; /* +0dB */
		else if (tx_rate == MGN_VHT2SS_MCS9) /* 256QAM */
			pwr_tracking_limit = 22; /* -1dB */

		else
			pwr_tracking_limit = 24;
	}
	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "tx_rate=0x%x, pwr_tracking_limit=%d\n",
	       tx_rate, pwr_tracking_limit);

	if (method == BBSWING) {
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "===>%s\n", __func__);

		if (rf_path == RF_PATH_A) {
			final_bb_swing_idx[RF_PATH_A] = (dm->rf_calibrate_info.OFDM_index[RF_PATH_A] > pwr_tracking_limit) ? pwr_tracking_limit : dm->rf_calibrate_info.OFDM_index[RF_PATH_A];
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "dm->rf_calibrate_info.OFDM_index[RF_PATH_A]=%d, dm->RealBbSwingIdx[RF_PATH_A]=%d\n",
			       dm->rf_calibrate_info.OFDM_index[RF_PATH_A],
			       final_bb_swing_idx[RF_PATH_A]);

			odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_bb_swing_idx[RF_PATH_A]]);
		} else {
			final_bb_swing_idx[RF_PATH_B] = (dm->rf_calibrate_info.OFDM_index[RF_PATH_B] > pwr_tracking_limit) ? pwr_tracking_limit : dm->rf_calibrate_info.OFDM_index[RF_PATH_B];
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "dm->rf_calibrate_info.OFDM_index[RF_PATH_B]=%d, dm->RealBbSwingIdx[RF_PATH_B]=%d\n",
			       dm->rf_calibrate_info.OFDM_index[RF_PATH_B],
			       final_bb_swing_idx[RF_PATH_B]);

			odm_set_bb_reg(dm, REG_B_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_bb_swing_idx[RF_PATH_B]]);
		}
	}

	else if (method == MIX_MODE) {
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
		       "cali_info->default_ofdm_index=%d, cali_info->absolute_ofdm_swing_idx[rf_path]=%d, rf_path = %d\n",
		       cali_info->default_ofdm_index,
		       cali_info->absolute_ofdm_swing_idx[rf_path], rf_path);

		final_cck_swing_index = cali_info->default_cck_index + cali_info->absolute_ofdm_swing_idx[rf_path];
		final_ofdm_swing_index = cali_info->default_ofdm_index + cali_info->absolute_ofdm_swing_idx[rf_path];

		if (rf_path == RF_PATH_A) {
			if (final_ofdm_swing_index > pwr_tracking_limit) { /* BBSwing higher then Limit */
				cali_info->remnant_cck_swing_idx = final_cck_swing_index - pwr_tracking_limit; /*CCK Follow the same compensate value as path A*/
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index - pwr_tracking_limit;

				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[pwr_tracking_limit]);

				cali_info->modify_tx_agc_flag_path_a = true;

				phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_A);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
				       "******Path_A Over BBSwing Limit, pwr_tracking_limit = %d, Remnant tx_agc value = %d\n",
				       pwr_tracking_limit,
				       cali_info->remnant_ofdm_swing_idx[rf_path
				       ]);
			} else if (final_ofdm_swing_index <= 0) {
				cali_info->remnant_cck_swing_idx = final_cck_swing_index; /*CCK Follow the same compensate value as path A*/
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index;

				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[0]);

				cali_info->modify_tx_agc_flag_path_a = true;

				phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_A);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
				       "******Path_A Lower then BBSwing lower bound  0, Remnant tx_agc value = %d\n",
				       cali_info->remnant_ofdm_swing_idx[rf_path
				       ]);
			} else {
				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
				       "******Path_A Compensate with BBSwing, final_ofdm_swing_index = %d\n",
				       final_ofdm_swing_index);

				if (cali_info->modify_tx_agc_flag_path_a) { /*If tx_agc has changed, reset tx_agc again*/
					cali_info->remnant_cck_swing_idx = 0;
					cali_info->remnant_ofdm_swing_idx[rf_path] = 0;

					phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_A);

					cali_info->modify_tx_agc_flag_path_a = false;

					RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "******Path_A dm->Modify_TxAGC_Flag = false\n");
				}
			}
		}

		if (rf_path == RF_PATH_B) {
			if (final_ofdm_swing_index > pwr_tracking_limit) { /* BBSwing higher then Limit */
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index - pwr_tracking_limit;

				odm_set_bb_reg(dm, REG_B_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[pwr_tracking_limit]);

				cali_info->modify_tx_agc_flag_path_b = true;

				phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_B);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
				       "******Path_B Over BBSwing Limit, pwr_tracking_limit = %d, Remnant tx_agc value = %d\n",
				       pwr_tracking_limit,
				       cali_info->remnant_ofdm_swing_idx[rf_path
				       ]);
			} else if (final_ofdm_swing_index <= 0) {
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index;

				odm_set_bb_reg(dm, REG_B_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[0]);

				cali_info->modify_tx_agc_flag_path_b = true;

				phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_B);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
				       "******Path_B Lower then BBSwing lower bound  0, Remnant tx_agc value = %d\n",
				       cali_info->remnant_ofdm_swing_idx[rf_path
				       ]);
			} else {
				odm_set_bb_reg(dm, REG_B_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
				       "******Path_B Compensate with BBSwing, final_ofdm_swing_index = %d\n",
				       final_ofdm_swing_index);

				if (cali_info->modify_tx_agc_flag_path_b) { /*If tx_agc has changed, reset tx_agc again*/
					cali_info->remnant_ofdm_swing_idx[rf_path] = 0;

					phy_set_tx_power_level_by_path(adapter, *dm->channel, RF_PATH_B);

					cali_info->modify_tx_agc_flag_path_b = false;

					RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "******Path_B dm->Modify_TxAGC_Flag = false\n");
				}
			}
		}

	} else
		return;
}

void get_delta_swing_table_8812a(void *dm_void, u8 **temperature_up_a,
				 u8 **temperature_down_a, u8 **temperature_up_b,
				 u8 **temperature_down_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _ADAPTER *adapter = dm->adapter;
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);
	struct _hal_rf_ *rf = &(dm->rf_table);
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	u8 tx_rate = 0xFF;
	u8 channel = *dm->channel;

	if (*dm->mp_mode == true) {
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
		u16 rate = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = adapter->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			if (dm->number_linked_client != 0)
				tx_rate = hw_rate_to_m_rate(dm->tx_rate);
			else
				tx_rate = rf->p_rate_index;
#endif
		} else /*force rate*/
			tx_rate = (u8)rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Power Tracking tx_rate=0x%X\n",
	       tx_rate);

	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(tx_rate)) {
			*temperature_up_a = cali_info->delta_swing_table_idx_2g_cck_a_p;
			*temperature_down_a = cali_info->delta_swing_table_idx_2g_cck_a_n;
			*temperature_up_b = cali_info->delta_swing_table_idx_2g_cck_b_p;
			*temperature_down_b = cali_info->delta_swing_table_idx_2g_cck_b_n;
		} else {
			*temperature_up_a = cali_info->delta_swing_table_idx_2ga_p;
			*temperature_down_a = cali_info->delta_swing_table_idx_2ga_n;
			*temperature_up_b = cali_info->delta_swing_table_idx_2gb_p;
			*temperature_down_b = cali_info->delta_swing_table_idx_2gb_n;
		}
	} else if (36 <= channel && channel <= 64) {
		*temperature_up_a = cali_info->delta_swing_table_idx_5ga_p[0];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[0];
		*temperature_up_b = cali_info->delta_swing_table_idx_5gb_p[0];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[0];
	} else if (100 <= channel && channel <= 144) {
		*temperature_up_a = cali_info->delta_swing_table_idx_5ga_p[1];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[1];
		*temperature_up_b = cali_info->delta_swing_table_idx_5gb_p[1];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[1];
	} else if (149 <= channel && channel <= 177) {
		*temperature_up_a = cali_info->delta_swing_table_idx_5ga_p[2];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[2];
		*temperature_up_b = cali_info->delta_swing_table_idx_5gb_p[2];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[2];
	} else {
		*temperature_up_a = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_a = (u8 *)delta_swing_table_idx_2ga_n_8188e;
		*temperature_up_b = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_b = (u8 *)delta_swing_table_idx_2ga_n_8188e;
	}

	return;
}

void configure_txpower_track_8812a(struct txpwrtrack_cfg *config)
{
	config->swing_table_size_cck = TXSCALE_TABLE_SIZE;
	config->swing_table_size_ofdm = TXSCALE_TABLE_SIZE;
	config->threshold_iqk = IQK_THRESHOLD;
	config->average_thermal_num = AVG_THERMAL_NUM_8812A;
	config->rf_path_count = MAX_PATH_NUM_8812A;
	config->thermal_reg_addr = RF_T_METER_8812A;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8812a;
	config->do_iqk = do_iqk_8812a;
	config->phy_lc_calibrate = halrf_lck_trigger;
	config->get_delta_swing_table = get_delta_swing_table_8812a;
}

#define BW_20M 0
#define BW_40M 1
#define BW_80M 2

void _iqk_rx_fill_iqc_8812a(struct dm_struct *dm, enum rf_path path,
			    unsigned int RX_X, unsigned int RX_Y)
{
	switch (path) {
	case RF_PATH_A: {
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
		if (RX_X >> 1 >= 0x112 || (RX_Y >> 1 >= 0x12 && RX_Y >> 1 <= 0x3ee)) {
			odm_set_bb_reg(dm, R_0xc10, 0x000003ff, 0x100);
			odm_set_bb_reg(dm, R_0xc10, 0x03ff0000, 0);
			RF_DBG(dm, DBG_RF_IQK,
			       "RX_X = %x;;RX_Y = %x ====>fill to IQC\n",
			       RX_X >> 1 & 0x000003ff, RX_Y >> 1 & 0x000003ff);
		} else {
			odm_set_bb_reg(dm, R_0xc10, 0x000003ff, RX_X >> 1);
			odm_set_bb_reg(dm, R_0xc10, 0x03ff0000, RX_Y >> 1);
			RF_DBG(dm, DBG_RF_IQK,
			       "RX_X = %x;;RX_Y = %x ====>fill to IQC\n",
			       RX_X >> 1 & 0x000003ff, RX_Y >> 1 & 0x000003ff);
			RF_DBG(dm, DBG_RF_IQK, "0xc10 = %x ====>fill to IQC\n",
			       odm_read_4byte(dm, 0xc10));
		}
	} break;
	case RF_PATH_B: {
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
		if (RX_X >> 1 >= 0x112 || (RX_Y >> 1 >= 0x12 && RX_Y >> 1 <= 0x3ee)) {
			odm_set_bb_reg(dm, R_0xe10, 0x000003ff, 0x100);
			odm_set_bb_reg(dm, R_0xe10, 0x03ff0000, 0);
			RF_DBG(dm, DBG_RF_IQK,
			       "RX_X = %x;;RX_Y = %x ====>fill to IQC\n",
			       RX_X >> 1 & 0x000003ff, RX_Y >> 1 & 0x000003ff);
		} else {
			odm_set_bb_reg(dm, R_0xe10, 0x000003ff, RX_X >> 1);
			odm_set_bb_reg(dm, R_0xe10, 0x03ff0000, RX_Y >> 1);
			RF_DBG(dm, DBG_RF_IQK,
			       "RX_X = %x;;RX_Y = %x====>fill to IQC\n ",
			       RX_X >> 1 & 0x000003ff, RX_Y >> 1 & 0x000003ff);
			RF_DBG(dm, DBG_RF_IQK, "0xe10 = %x====>fill to IQC\n",
			       odm_read_4byte(dm, 0xe10));
		}
	} break;
	default:
		break;
	};
}

void _iqk_tx_fill_iqc_8812a(struct dm_struct *dm, enum rf_path path,
			    unsigned int TX_X, unsigned int TX_Y)
{
	struct _hal_rf_ *rf = &(dm->rf_table);

	switch (path) {
	case RF_PATH_A: {
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
		odm_set_bb_reg(dm, R_0xc90, BIT(7), 0x1);
		odm_set_bb_reg(dm, R_0xcc4, BIT(18), 0x1);
		if (!rf->dpk_done)
			odm_set_bb_reg(dm, R_0xcc4, BIT(29), 0x1);
		odm_set_bb_reg(dm, R_0xcc8, BIT(29), 0x1);
		odm_set_bb_reg(dm, R_0xccc, 0x000007ff, TX_Y);
		odm_set_bb_reg(dm, R_0xcd4, 0x000007ff, TX_X);
		RF_DBG(dm, DBG_RF_IQK,
		       "TX_X = %x;;TX_Y = %x =====> fill to IQC\n",
		       TX_X & 0x000007ff, TX_Y & 0x000007ff);
		RF_DBG(dm, DBG_RF_IQK,
		       "0xcd4 = %x;;0xccc = %x ====>fill to IQC\n",
		       odm_get_bb_reg(dm, R_0xcd4, 0x000007ff),
		       odm_get_bb_reg(dm, R_0xccc, 0x000007ff));
	} break;
	case RF_PATH_B: {
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
		odm_set_bb_reg(dm, R_0xe90, BIT(7), 0x1);
		odm_set_bb_reg(dm, R_0xec4, BIT(18), 0x1);
		if (!rf->dpk_done)
			odm_set_bb_reg(dm, R_0xec4, BIT(29), 0x1);
		odm_set_bb_reg(dm, R_0xec8, BIT(29), 0x1);
		odm_set_bb_reg(dm, R_0xecc, 0x000007ff, TX_Y);
		odm_set_bb_reg(dm, R_0xed4, 0x000007ff, TX_X);
		RF_DBG(dm, DBG_RF_IQK,
		       "TX_X = %x;;TX_Y = %x =====> fill to IQC\n",
		       TX_X & 0x000007ff, TX_Y & 0x000007ff);
		RF_DBG(dm, DBG_RF_IQK,
		       "0xed4 = %x;;0xecc = %x ====>fill to IQC\n",
		       odm_get_bb_reg(dm, R_0xed4, 0x000007ff),
		       odm_get_bb_reg(dm, R_0xecc, 0x000007ff));
	} break;
	default:
		break;
	};
}

void _iqk_backup_mac_bb_8812a(struct dm_struct *dm, u32 *MACBB_backup,
			      u32 *backup_macbb_reg, u32 MACBB_NUM)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* save MACBB default value */
	for (i = 0; i < MACBB_NUM; i++)
		MACBB_backup[i] = odm_read_4byte(dm, backup_macbb_reg[i]);

	RF_DBG(dm, DBG_RF_IQK, "BackupMacBB Success!!!!\n");
}
void _iqk_backup_rf_8812a(struct dm_struct *dm, u32 *RFA_backup,
			  u32 *RFB_backup, u32 *backup_rf_reg, u32 RF_NUM)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Save RF Parameters */
	for (i = 0; i < RF_NUM; i++) {
		RFA_backup[i] = odm_get_rf_reg(dm, RF_PATH_A, backup_rf_reg[i], MASKDWORD);
		RFB_backup[i] = odm_get_rf_reg(dm, RF_PATH_B, backup_rf_reg[i], MASKDWORD);
	}
	RF_DBG(dm, DBG_RF_IQK, "BackupRF Success!!!!\n");
}
void _iqk_backup_afe_8812a(struct dm_struct *dm, u32 *AFE_backup,
			   u32 *backup_afe_reg, u32 AFE_NUM)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Save AFE Parameters */
	for (i = 0; i < AFE_NUM; i++)
		AFE_backup[i] = odm_read_4byte(dm, backup_afe_reg[i]);
	RF_DBG(dm, DBG_RF_IQK, "BackupAFE Success!!!!\n");
}
void _iqk_restore_mac_bb_8812a(struct dm_struct *dm, u32 *MACBB_backup,
			       u32 *backup_macbb_reg, u32 MACBB_NUM)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Reload MacBB Parameters */
	for (i = 0; i < MACBB_NUM; i++)
		odm_write_4byte(dm, backup_macbb_reg[i], MACBB_backup[i]);
	RF_DBG(dm, DBG_RF_IQK, "RestoreMacBB Success!!!!\n");
}
void _iqk_restore_rf_8812a(struct dm_struct *dm, enum rf_path path,
			   u32 *backup_rf_reg, u32 *RF_backup, u32 RF_REG_NUM)
{
	u32 i;

	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	for (i = 0; i < RF_REG_NUM; i++)
		odm_set_rf_reg(dm, (enum rf_path)path, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i]);

	odm_set_rf_reg(dm, path, RF_0xef, RFREGOFFSETMASK, 0x0);

	switch (path) {
	case RF_PATH_A: {
		RF_DBG(dm, DBG_RF_IQK, "RestoreRF path A Success!!!!\n");
	} break;
	case RF_PATH_B: {
		RF_DBG(dm, DBG_RF_IQK, "RestoreRF path B Success!!!!\n");
	} break;
	default:
		break;
	}
}
void _iqk_restore_afe_8812a(struct dm_struct *dm, u32 *AFE_backup,
			    u32 *backup_afe_reg, u32 AFE_NUM)
{
	struct _hal_rf_ *rf = &(dm->rf_table);
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Reload AFE Parameters */
	for (i = 0; i < AFE_NUM; i++)
		odm_write_4byte(dm, backup_afe_reg[i], AFE_backup[i]);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
	odm_write_4byte(dm, 0xc80, 0x0);
	odm_write_4byte(dm, 0xc84, 0x0);
	odm_write_4byte(dm, 0xc88, 0x0);
	odm_write_4byte(dm, 0xc8c, 0x3c000000);
	odm_set_bb_reg(dm, R_0xc90, BIT(7), 0x1);
	odm_set_bb_reg(dm, R_0xcc4, BIT(18), 0x1);
	if (!rf->dpk_done)
		odm_set_bb_reg(dm, R_0xcc4, BIT(29), 0x1);
	odm_set_bb_reg(dm, R_0xcc8, BIT(29), 0x1);
	/* odm_write_4byte(dm, 0xcb8, 0x0); */
	odm_write_4byte(dm, 0xe80, 0x0);
	odm_write_4byte(dm, 0xe84, 0x0);
	odm_write_4byte(dm, 0xe88, 0x0);
	odm_write_4byte(dm, 0xe8c, 0x3c000000);
	odm_set_bb_reg(dm, R_0xe90, BIT(7), 0x1);
	odm_set_bb_reg(dm, R_0xec4, BIT(18), 0x1);
	if (!rf->dpk_done)
		odm_set_bb_reg(dm, R_0xec4, BIT(29), 0x1);
	odm_set_bb_reg(dm, R_0xec8, BIT(29), 0x1);
	/* odm_write_4byte(dm, 0xeb8, 0x0); */
	RF_DBG(dm, DBG_RF_IQK, "RestoreAFE Success!!!!\n");
}

void _iqk_configure_mac_8812a(struct dm_struct *dm)
{
	/* ========MAC register setting======== */
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_write_1byte(dm, 0x522, 0x3f);
	odm_set_bb_reg(dm, R_0x550, BIT(11) | BIT(3), 0x0);
	odm_write_1byte(dm, 0x808, 0x00); /*		RX ante off */
	odm_set_bb_reg(dm, R_0x838, 0xf, 0xc); /*		CCA off */
	odm_write_1byte(dm, 0xa07, 0xf); /*		CCK RX path off */
}

#define cal_num 10

void _iqk_tx_8812a(struct dm_struct *dm, u8 chnl_idx)
{
	u8 delay_count, cal = 0;
	u8 cal0_retry, cal1_retry, tx0_average = 0, tx1_average = 0, rx0_average = 0, rx1_average = 0;
	int TX_IQC_temp[10][4], TX_IQC[4] = {}; /* TX_IQC = [TX0_X, TX0_Y,TX1_X,TX1_Y]; for 3 times */
	int RX_IQC_temp[10][4], RX_IQC[4] = {}; /* RX_IQC = [RX0_X, RX0_Y,RX1_X,RX1_Y]; for 3 times */
	boolean TX0_fail = true, RX0_fail = true, IQK0_ready = false, TX0_finish = false, RX0_finish = false;
	boolean TX1_fail = true, RX1_fail = true, IQK1_ready = false, TX1_finish = false, RX1_finish = false, VDF_enable = false;
	int i, ii, dx = 0, dy = 0;
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);

	RF_DBG(dm, DBG_RF_IQK,
	       "band_width = %d, ext_pa_5g = %d, ExtPA2G = %d\n",
	       *dm->band_width, dm->ext_pa_5g, dm->ext_pa);
	RF_DBG(dm, DBG_RF_IQK, "Interface = %d, RFE_Type = %d\n",
	       dm->support_interface, dm->rfe_type);
	if (*dm->band_width == 2)
		VDF_enable = true;
	VDF_enable = false;

	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* ========path-A AFE all on======== */
	/* Port 0 DAC/ADC on */
	odm_write_4byte(dm, 0xc60, 0x77777777);
	odm_write_4byte(dm, 0xc64, 0x77777777);

	/* Port 1 DAC/ADC on */
	odm_write_4byte(dm, 0xe60, 0x77777777);
	odm_write_4byte(dm, 0xe64, 0x77777777);

	odm_write_4byte(dm, 0xc68, 0x19791979);
	odm_write_4byte(dm, 0xe68, 0x19791979);
	odm_set_bb_reg(dm, R_0xc00, 0xf, 0x4); /*	hardware 3-wire off */
	odm_set_bb_reg(dm, R_0xe00, 0xf, 0x4); /*	hardware 3-wire off */

	/* DAC/ADC sampling rate (160 MHz) */
	odm_set_bb_reg(dm, R_0xc5c, BIT(26) | BIT(25) | BIT(24), 0x7);
	odm_set_bb_reg(dm, R_0xe5c, BIT(26) | BIT(25) | BIT(24), 0x7);

	/* ====== path A TX IQK RF setting ====== */
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, RFREGOFFSETMASK, 0x80002);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x30, RFREGOFFSETMASK, 0x20000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x31, RFREGOFFSETMASK, 0x3fffd);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x32, RFREGOFFSETMASK, 0xfe83f);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x65, RFREGOFFSETMASK, 0x931d5);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0x8a001);
	/* ====== path B TX IQK RF setting ====== */
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, RFREGOFFSETMASK, 0x80002);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x30, RFREGOFFSETMASK, 0x20000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x31, RFREGOFFSETMASK, 0x3fffd);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x32, RFREGOFFSETMASK, 0xfe83f);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x65, RFREGOFFSETMASK, 0x931d5);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0x8a001);
	odm_write_4byte(dm, 0x90c, 0x00008000);
	odm_set_bb_reg(dm, R_0xc94, BIT(0), 0x1);
	odm_set_bb_reg(dm, R_0xe94, BIT(0), 0x1);
	odm_write_4byte(dm, 0x978, 0x29002000); /* TX (X,Y) */
	odm_write_4byte(dm, 0x97c, 0xa9002000); /* RX (X,Y) */
	odm_write_4byte(dm, 0x984, 0x00462910); /* [0]:AGC_en, [15]:idac_K_Mask */
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */

	if (dm->ext_pa_5g) {
		if (dm->rfe_type == 1) {
			odm_write_4byte(dm, 0xc88, 0x821403e3);
			odm_write_4byte(dm, 0xe88, 0x821403e3);
		} else {
			odm_write_4byte(dm, 0xc88, 0x821403f7);
			odm_write_4byte(dm, 0xe88, 0x821403f7);
		}
	} else {
		odm_write_4byte(dm, 0xc88, 0x821403f1);
		odm_write_4byte(dm, 0xe88, 0x821403f1);
	}
	if (*dm->band_type == ODM_BAND_5G) {
		odm_write_4byte(dm, 0xc8c, 0x68163e96);
		odm_write_4byte(dm, 0xe8c, 0x68163e96);
	} else {
		odm_write_4byte(dm, 0xc8c, 0x28163e96);
		odm_write_4byte(dm, 0xe8c, 0x28163e96);
		if (dm->rfe_type == 3) {
			if (dm->ext_pa)
				odm_write_4byte(dm, 0xc88, 0x821403e3);
			else
				odm_write_4byte(dm, 0xc88, 0x821403f7);
		}
	}

	if (VDF_enable) {
	} else {
		odm_write_4byte(dm, 0xc80, 0x18008c10); /* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
		odm_write_4byte(dm, 0xc84, 0x38008c10); /* RX_Tone_idx[9:0], RxK_Mask[29] */
		odm_write_4byte(dm, 0xce8, 0x00000000);
		odm_write_4byte(dm, 0xe80, 0x18008c10); /* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
		odm_write_4byte(dm, 0xe84, 0x38008c10); /* RX_Tone_idx[9:0], RxK_Mask[29] */
		odm_write_4byte(dm, 0xee8, 0x00000000);

		cal0_retry = 0;
		cal1_retry = 0;
		while (1) {
			/* one shot */
			odm_write_4byte(dm, 0xcb8, 0x00100000);
			odm_write_4byte(dm, 0xeb8, 0x00100000);
			odm_write_4byte(dm, 0x980, 0xfa000000);
			odm_write_4byte(dm, 0x980, 0xf8000000);

			ODM_delay_ms(10); /* delay 10ms */
			odm_write_4byte(dm, 0xcb8, 0x00000000);
			odm_write_4byte(dm, 0xeb8, 0x00000000);
			delay_count = 0;
			while (1) {
				if (!TX0_finish)
					IQK0_ready = (boolean)odm_get_bb_reg(dm, R_0xd00, BIT(10));
				if (!TX1_finish)
					IQK1_ready = (boolean)odm_get_bb_reg(dm, R_0xd40, BIT(10));
				if ((IQK0_ready && IQK1_ready) || delay_count > 20)
					break;

				ODM_delay_ms(1);
				delay_count++;
			}
			RF_DBG(dm, DBG_RF_IQK, "TX delay_count = %d\n",
			       delay_count);
			if (delay_count < 20) { /* If 20ms No Result, then cal_retry++ */
				/* ============TXIQK Check============== */
				TX0_fail = (boolean)odm_get_bb_reg(dm, R_0xd00, BIT(12));
				TX1_fail = (boolean)odm_get_bb_reg(dm, R_0xd40, BIT(12));
				if (!(TX0_fail || TX0_finish)) {
					odm_write_4byte(dm, 0xcb8, 0x02000000);
					TX_IQC_temp[tx0_average][0] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
					odm_write_4byte(dm, 0xcb8, 0x04000000);
					TX_IQC_temp[tx0_average][1] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
					RF_DBG(dm, DBG_RF_IQK, "TX_X0[%d] = %x ;; TX_Y0[%d] = %x\n", tx0_average, (TX_IQC_temp[tx0_average][0]) >> 21 & 0x000007ff, tx0_average, (TX_IQC_temp[tx0_average][1]) >> 21 & 0x000007ff);
					/*

					odm_write_4byte(dm, 0xcb8, 0x01000000);
					reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
					odm_write_4byte(dm, 0xcb8, 0x02000000);
					reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
					image_power = (reg2<<32)+reg1;
					dbg_print("Before PW = %d\n", image_power);
					odm_write_4byte(dm, 0xcb8, 0x03000000);
					reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
					odm_write_4byte(dm, 0xcb8, 0x04000000);
					reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
					image_power = (reg2<<32)+reg1;
					dbg_print("After PW = %d\n", image_power);
					*/
					tx0_average++;
				} else {
					cal0_retry++;
					if (cal0_retry == 10)
						break;
				}
				if (!(TX1_fail || TX1_finish)) {
					odm_write_4byte(dm, 0xeb8, 0x02000000);
					TX_IQC_temp[tx1_average][2] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
					odm_write_4byte(dm, 0xeb8, 0x04000000);
					TX_IQC_temp[tx1_average][3] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
					RF_DBG(dm, DBG_RF_IQK, "TX_X1[%d] = %x ;; TX_Y1[%d] = %x\n", tx1_average, (TX_IQC_temp[tx1_average][2]) >> 21 & 0x000007ff, tx1_average, (TX_IQC_temp[tx1_average][3]) >> 21 & 0x000007ff);
					/*
					int			reg1 = 0, reg2 = 0, image_power = 0;
					odm_write_4byte(dm, 0xeb8, 0x01000000);
					reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
					odm_write_4byte(dm, 0xeb8, 0x02000000);
					reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
					image_power = (reg2<<32)+reg1;
					dbg_print("Before PW = %d\n", image_power);
					odm_write_4byte(dm, 0xeb8, 0x03000000);
					reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
					odm_write_4byte(dm, 0xeb8, 0x04000000);
					reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
					image_power = (reg2<<32)+reg1;
					dbg_print("After PW = %d\n", image_power);
					*/
					tx1_average++;
				} else {
					cal1_retry++;
					if (cal1_retry == 10)
						break;
				}
			} else {
				cal0_retry++;
				cal1_retry++;
				RF_DBG(dm, DBG_RF_IQK,
				       "delay 20ms TX IQK Not Ready!!!!!\n");
				if (cal0_retry == 10)
					break;
			}
			if (tx0_average >= 2) {
				for (i = 0; i < tx0_average; i++) {
					for (ii = i + 1; ii < tx0_average; ii++) {
						dx = (TX_IQC_temp[i][0] >> 21) - (TX_IQC_temp[ii][0] >> 21);
						if (dx < 4 && dx > -4) {
							dy = (TX_IQC_temp[i][1] >> 21) - (TX_IQC_temp[ii][1] >> 21);
							if (dy < 4 && dy > -4) {
								TX_IQC[0] = ((TX_IQC_temp[i][0] >> 21) + (TX_IQC_temp[ii][0] >> 21)) / 2;
								TX_IQC[1] = ((TX_IQC_temp[i][1] >> 21) + (TX_IQC_temp[ii][1] >> 21)) / 2;
								RF_DBG(dm, DBG_RF_IQK, "TXA_X = %x;;TXA_Y = %x\n", TX_IQC[0] & 0x000007ff, TX_IQC[1] & 0x000007ff);
								TX0_finish = true;
							}
						}
					}
				}
			}
			if (tx1_average >= 2) {
				for (i = 0; i < tx1_average; i++) {
					for (ii = i + 1; ii < tx1_average; ii++) {
						dx = (TX_IQC_temp[i][2] >> 21) - (TX_IQC_temp[ii][2] >> 21);
						if (dx < 4 && dx > -4) {
							dy = (TX_IQC_temp[i][3] >> 21) - (TX_IQC_temp[ii][3] >> 21);
							if (dy < 4 && dy > -4) {
								TX_IQC[2] = ((TX_IQC_temp[i][2] >> 21) + (TX_IQC_temp[ii][2] >> 21)) / 2;
								TX_IQC[3] = ((TX_IQC_temp[i][3] >> 21) + (TX_IQC_temp[ii][3] >> 21)) / 2;
								RF_DBG(dm, DBG_RF_IQK, "TXB_X = %x;;TXB_Y = %x\n", TX_IQC[2] & 0x000007ff, TX_IQC[3] & 0x000007ff);
								TX1_finish = true;
							}
						}
					}
				}
			}
			RF_DBG(dm, DBG_RF_IQK,
			       "tx0_average = %d, tx1_average = %d\n",
			       tx0_average, tx1_average);
			RF_DBG(dm, DBG_RF_IQK,
			       "TX0_finish = %d, TX1_finish = %d\n", TX0_finish,
			       TX1_finish);
			if (TX0_finish && TX1_finish)
				break;
			if ((cal0_retry + tx0_average) >= 10 || (cal1_retry + tx1_average) >= 10)
				break;
		}
		RF_DBG(dm, DBG_RF_IQK, "TXA_cal_retry = %d\n", cal0_retry);
		RF_DBG(dm, DBG_RF_IQK, "TXB_cal_retry = %d\n", cal1_retry);
	}

	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x58, 0x7fe00, odm_get_rf_reg(dm, RF_PATH_A, RF_0x8, 0xffc00)); /* Load LOK */
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x58, 0x7fe00, odm_get_rf_reg(dm, RF_PATH_B, RF_0x8, 0xffc00)); /* Load LOK */
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */

	if (VDF_enable == 1) {
	} else {
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
		if (TX0_finish) {
			/* ====== path A RX IQK RF setting====== */
			odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, RFREGOFFSETMASK, 0x80000);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x30, RFREGOFFSETMASK, 0x30000);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x31, RFREGOFFSETMASK, 0x3f7ff);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x32, RFREGOFFSETMASK, 0xfe7bf);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0x88001);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x65, RFREGOFFSETMASK, 0x931d1);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, RFREGOFFSETMASK, 0x00000);
		}
		if (TX1_finish) {
			/* ====== path B RX IQK RF setting====== */
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, RFREGOFFSETMASK, 0x80000);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x30, RFREGOFFSETMASK, 0x30000);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x31, RFREGOFFSETMASK, 0x3f7ff);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x32, RFREGOFFSETMASK, 0xfe7bf);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0x88001);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x65, RFREGOFFSETMASK, 0x931d1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, RFREGOFFSETMASK, 0x00000);
		}
		odm_set_bb_reg(dm, R_0x978, BIT(31), 0x1);
		odm_set_bb_reg(dm, R_0x97c, BIT(31), 0x0);
		odm_write_4byte(dm, 0x90c, 0x00008000);
		if (dm->support_interface == ODM_ITRF_PCIE)
			odm_write_4byte(dm, 0x984, 0x0046a911);
		else
			odm_write_4byte(dm, 0x984, 0x0046a890);
		/* odm_write_4byte(dm, 0x984, 0x0046a890); */
		if (dm->rfe_type == 1) {
			odm_write_4byte(dm, 0xcb0, 0x77777717);
			odm_write_4byte(dm, 0xcb4, 0x00000077);
			odm_write_4byte(dm, 0xeb0, 0x77777717);
			odm_write_4byte(dm, 0xeb4, 0x00000077);
		} else {
			odm_write_4byte(dm, 0xcb0, 0x77777717);
			odm_write_4byte(dm, 0xcb4, 0x02000077);
			odm_write_4byte(dm, 0xeb0, 0x77777717);
			odm_write_4byte(dm, 0xeb4, 0x02000077);
		}

		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
		if (TX0_finish) {
			odm_write_4byte(dm, 0xc80, 0x38008c10); /* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
			odm_write_4byte(dm, 0xc84, 0x18008c10); /* RX_Tone_idx[9:0], RxK_Mask[29] */
			odm_write_4byte(dm, 0xc88, 0x82140119);
		}
		if (TX1_finish) {
			odm_write_4byte(dm, 0xe80, 0x38008c10); /* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
			odm_write_4byte(dm, 0xe84, 0x18008c10); /* RX_Tone_idx[9:0], RxK_Mask[29] */
			odm_write_4byte(dm, 0xe88, 0x82140119);
		}
		cal0_retry = 0;
		cal1_retry = 0;
		while (1) {
			/* one shot */
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
			if (TX0_finish) {
				odm_set_bb_reg(dm, R_0x978, 0x03FF8000, (TX_IQC[0]) & 0x000007ff);
				odm_set_bb_reg(dm, R_0x978, 0x000007FF, (TX_IQC[1]) & 0x000007ff);
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
				if (dm->rfe_type == 1)
					odm_write_4byte(dm, 0xc8c, 0x28161500);
				else
					odm_write_4byte(dm, 0xc8c, 0x28160cc0);
				odm_write_4byte(dm, 0xcb8, 0x00300000);
				odm_write_4byte(dm, 0xcb8, 0x00100000);
				ODM_delay_ms(5); /* delay 5ms */
				odm_write_4byte(dm, 0xc8c, 0x3c000000);
				odm_write_4byte(dm, 0xcb8, 0x00000000);
			}
			if (TX1_finish) {
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
				odm_set_bb_reg(dm, R_0x978, 0x03FF8000, (TX_IQC[2]) & 0x000007ff);
				odm_set_bb_reg(dm, R_0x978, 0x000007FF, (TX_IQC[3]) & 0x000007ff);
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
				if (dm->rfe_type == 1)
					odm_write_4byte(dm, 0xe8c, 0x28161500);
				else
					odm_write_4byte(dm, 0xe8c, 0x28160ca0);
				odm_write_4byte(dm, 0xeb8, 0x00300000);
				odm_write_4byte(dm, 0xeb8, 0x00100000);
				ODM_delay_ms(5); /* delay 5ms */
				odm_write_4byte(dm, 0xe8c, 0x3c000000);
				odm_write_4byte(dm, 0xeb8, 0x00000000);
			}
			delay_count = 0;
			while (1) {
				if (!RX0_finish && TX0_finish)
					IQK0_ready = (boolean)odm_get_bb_reg(dm, R_0xd00, BIT(10));
				if (!RX1_finish && TX1_finish)
					IQK1_ready = (boolean)odm_get_bb_reg(dm, R_0xd40, BIT(10));
				if ((IQK0_ready && IQK1_ready) || delay_count > 20)
					break;

				ODM_delay_ms(1);
				delay_count++;
			}
			RF_DBG(dm, DBG_RF_IQK, "RX delay_count = %d\n",
			       delay_count);
			if (delay_count < 20) { /* If 20ms No Result, then cal_retry++ */
				/* ============RXIQK Check============== */
				RX0_fail = (boolean)odm_get_bb_reg(dm, R_0xd00, BIT(11));
				RX1_fail = (boolean)odm_get_bb_reg(dm, R_0xd40, BIT(11));
				if (!(RX0_fail || RX0_finish) && TX0_finish) {
					odm_write_4byte(dm, 0xcb8, 0x06000000);
					RX_IQC_temp[rx0_average][0] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
					odm_write_4byte(dm, 0xcb8, 0x08000000);
					RX_IQC_temp[rx0_average][1] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
					RF_DBG(dm, DBG_RF_IQK, "RX_X0[%d] = %x ;; RX_Y0[%d] = %x\n", rx0_average, (RX_IQC_temp[rx0_average][0]) >> 21 & 0x000007ff, rx0_average, (RX_IQC_temp[rx0_average][1]) >> 21 & 0x000007ff);
					/*
										odm_write_4byte(dm, 0xcb8, 0x05000000);
										reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
										odm_write_4byte(dm, 0xcb8, 0x06000000);
										reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
										dbg_print("reg1 = %d, reg2 = %d", reg1, reg2);
										image_power = (reg2<<32)+reg1;
										dbg_print("Before PW = %d\n", image_power);
										odm_write_4byte(dm, 0xcb8, 0x07000000);
										reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
										odm_write_4byte(dm, 0xcb8, 0x08000000);
										reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
										image_power = (reg2<<32)+reg1;
										dbg_print("After PW = %d\n", image_power);
					*/
					rx0_average++;
				} else {
					RF_DBG(dm, DBG_RF_IQK, "1. RXA_cal_retry = %d\n", cal0_retry);
					cal0_retry++;
					if (cal0_retry == 10)
						break;
				}
				if (!(RX1_fail || RX1_finish) && TX1_finish) {
					odm_write_4byte(dm, 0xeb8, 0x06000000);
					RX_IQC_temp[rx1_average][2] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
					odm_write_4byte(dm, 0xeb8, 0x08000000);
					RX_IQC_temp[rx1_average][3] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
					RF_DBG(dm, DBG_RF_IQK, "RX_X1[%d] = %x ;; RX_Y1[%d] = %x\n", rx1_average, (RX_IQC_temp[rx1_average][2]) >> 21 & 0x000007ff, rx1_average, (RX_IQC_temp[rx1_average][3]) >> 21 & 0x000007ff);
					/*
										odm_write_4byte(dm, 0xeb8, 0x05000000);
										reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
										odm_write_4byte(dm, 0xeb8, 0x06000000);
										reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
										dbg_print("reg1 = %d, reg2 = %d", reg1, reg2);
										image_power = (reg2<<32)+reg1;
										dbg_print("Before PW = %d\n", image_power);
										odm_write_4byte(dm, 0xeb8, 0x07000000);
										reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
										odm_write_4byte(dm, 0xeb8, 0x08000000);
										reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
										image_power = (reg2<<32)+reg1;
										dbg_print("After PW = %d\n", image_power);
					*/
					rx1_average++;
				} else {
					cal1_retry++;
					if (cal1_retry == 10)
						break;
				}

			} else {
				RF_DBG(dm, DBG_RF_IQK,
				       "2. RXA_cal_retry = %d\n", cal0_retry);
				cal0_retry++;
				cal1_retry++;
				RF_DBG(dm, DBG_RF_IQK,
				       "delay 20ms RX IQK Not Ready!!!!!\n");
				if (cal0_retry == 10)
					break;
			}
			RF_DBG(dm, DBG_RF_IQK, "3. RXA_cal_retry = %d\n",
			       cal0_retry);
			if (rx0_average >= 2) {
				for (i = 0; i < rx0_average; i++) {
					for (ii = i + 1; ii < rx0_average; ii++) {
						dx = (RX_IQC_temp[i][0] >> 21) - (RX_IQC_temp[ii][0] >> 21);
						if (dx < 4 && dx > -4) {
							dy = (RX_IQC_temp[i][1] >> 21) - (RX_IQC_temp[ii][1] >> 21);
							if (dy < 4 && dy > -4) {
								RX_IQC[0] = ((RX_IQC_temp[i][0] >> 21) + (RX_IQC_temp[ii][0] >> 21)) / 2;
								RX_IQC[1] = ((RX_IQC_temp[i][1] >> 21) + (RX_IQC_temp[ii][1] >> 21)) / 2;
								RX0_finish = true;
								break;
							}
						}
					}
				}
			}
			if (rx1_average >= 2) {
				for (i = 0; i < rx1_average; i++) {
					for (ii = i + 1; ii < rx1_average; ii++) {
						dx = (RX_IQC_temp[i][2] >> 21) - (RX_IQC_temp[ii][2] >> 21);
						if (dx < 4 && dx > -4) {
							dy = (RX_IQC_temp[i][3] >> 21) - (RX_IQC_temp[ii][3] >> 21);
							if (dy < 4 && dy > -4) {
								RX_IQC[2] = ((RX_IQC_temp[i][2] >> 21) + (RX_IQC_temp[ii][2] >> 21)) / 2;
								RX_IQC[3] = ((RX_IQC_temp[i][3] >> 21) + (RX_IQC_temp[ii][3] >> 21)) / 2;
								RX1_finish = true;
								break;
							}
						}
					}
				}
			}
			RF_DBG(dm, DBG_RF_IQK,
			       "rx0_average = %d, rx1_average = %d\n",
			       rx0_average, rx1_average);
			RF_DBG(dm, DBG_RF_IQK,
			       "RX0_finish = %d, RX1_finish = %d\n", RX0_finish,
			       RX1_finish);
			if ((RX0_finish || !TX0_finish) && (RX1_finish || !TX1_finish))
				break;
			if ((cal0_retry + rx0_average) >= 10 || (cal1_retry + rx1_average) >= 10 || rx0_average == 3 || rx1_average == 3)
				break;
		}
		RF_DBG(dm, DBG_RF_IQK, "RXA_cal_retry = %d\n", cal0_retry);
		RF_DBG(dm, DBG_RF_IQK, "RXB_cal_retry = %d\n", cal1_retry);
	}

	/* FillIQK Result */
	RF_DBG(dm, DBG_RF_IQK, "========Path_A =======\n");

	if (TX0_finish)
		_iqk_tx_fill_iqc_8812a(dm, RF_PATH_A, TX_IQC[0], TX_IQC[1]);
	else
		_iqk_tx_fill_iqc_8812a(dm, RF_PATH_A, 0x200, 0x0);

	if (RX0_finish)
		_iqk_rx_fill_iqc_8812a(dm, RF_PATH_A, RX_IQC[0], RX_IQC[1]);
	else
		_iqk_rx_fill_iqc_8812a(dm, RF_PATH_A, 0x200, 0x0);

	RF_DBG(dm, DBG_RF_IQK, "========Path_B =======\n");

	if (TX1_finish)
		_iqk_tx_fill_iqc_8812a(dm, RF_PATH_B, TX_IQC[2], TX_IQC[3]);
	else
		_iqk_tx_fill_iqc_8812a(dm, RF_PATH_B, 0x200, 0x0);

	if (RX1_finish)
		_iqk_rx_fill_iqc_8812a(dm, RF_PATH_B, RX_IQC[2], RX_IQC[3]);
	else
		_iqk_rx_fill_iqc_8812a(dm, RF_PATH_B, 0x200, 0x0);
}

#define MACBB_REG_NUM 9
#define AFE_REG_NUM 12
#define RF_REG_NUM 3

/* Maintained by BB James. */
void _phy_iq_calibrate_8812a(struct dm_struct *dm, u8 channel)
{
	u32 MACBB_backup[MACBB_REG_NUM], AFE_backup[AFE_REG_NUM] = {0}, RFA_backup[RF_REG_NUM] = {0}, RFB_backup[RF_REG_NUM] = {0};
	u32 backup_macbb_reg[MACBB_REG_NUM] = {0x520, 0x550, 0x808, 0xa04, 0x90c, 0xc00, 0xe00, 0x838, 0x82c};
	u32 backup_afe_reg[AFE_REG_NUM] = {0xc5c, 0xc60, 0xc64, 0xc68, 0xcb0, 0xcb4,
					   0xe5c, 0xe60, 0xe64, 0xe68, 0xeb0, 0xeb4};
	u32 reg_c1b8, reg_e1b8;
	u32 backup_rf_reg[RF_REG_NUM] = {0x65, 0x8f, 0x0};
	u8 chnl_idx = odm_get_right_chnl_place_for_iqk(channel);

	_iqk_backup_mac_bb_8812a(dm, MACBB_backup, backup_macbb_reg, MACBB_REG_NUM);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
	reg_c1b8 = odm_read_4byte(dm, 0xcb8);
	reg_e1b8 = odm_read_4byte(dm, 0xeb8);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
	_iqk_backup_afe_8812a(dm, AFE_backup, backup_afe_reg, AFE_REG_NUM);
	_iqk_backup_rf_8812a(dm, RFA_backup, RFB_backup, backup_rf_reg, RF_REG_NUM);

	_iqk_configure_mac_8812a(dm);
	_iqk_tx_8812a(dm, chnl_idx);
	_iqk_restore_rf_8812a(dm, RF_PATH_A, backup_rf_reg, RFA_backup, RF_REG_NUM);
	_iqk_restore_rf_8812a(dm, RF_PATH_B, backup_rf_reg, RFB_backup, RF_REG_NUM);

	_iqk_restore_afe_8812a(dm, AFE_backup, backup_afe_reg, AFE_REG_NUM);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
	odm_write_4byte(dm, 0xcb8, reg_c1b8);
	odm_write_4byte(dm, 0xeb8, reg_e1b8);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
	_iqk_restore_mac_bb_8812a(dm, MACBB_backup, backup_macbb_reg, MACBB_REG_NUM);
}

void _phy_lc_calibrate_8812a(struct dm_struct *dm, boolean is2T)
{
	u32 /*rf_amode=0, rf_bmode=0,*/ lc_cal = 0, tmp = 0;

	/* Check continuous TX and Packet TX */
	u32 reg0x914 = odm_read_4byte(dm, REG_SINGLE_TONE_CONT_TX_JAGUAR);
	;

	/* Backup RF reg18. */
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);

	if ((reg0x914 & 0x70000) != 0) /* If contTx, disable all continuous TX. 0x914[18:16] */
		/* <20121121, Kordan> A workaround: If we set 0x914[18:16] as zero, BB turns off ContTx */
		/* until another packet comes in. To avoid ContTx being turned off, we skip this step. */
		; /* odm_write_4byte(dm, REG_SINGLE_TONE_CONT_TX_JAGUAR, reg0x914 & (~0x70000)); */
	else /* If packet Tx-ing, pause Tx. */
		odm_write_1byte(dm, REG_TXPAUSE, 0xFF);

#if 0
	/* 3 1. Read original RF mode */
	rf_amode = odm_get_rf_reg(dm, RF_PATH_A, RF_AC, RFREGOFFSETMASK);
	if (is2T)
		rf_bmode = odm_get_rf_reg(dm, RF_PATH_B, RF_AC, RFREGOFFSETMASK);


	/* 3 2. Set RF mode = standby mode */
	odm_set_rf_reg(dm, RF_PATH_A, RF_AC, RFREGOFFSETMASK, (rf_amode & 0x8FFFF) | 0x10000);
	if (is2T)
		odm_set_rf_reg(dm, RF_PATH_B, RF_AC, RFREGOFFSETMASK, (rf_bmode & 0x8FFFF) | 0x10000);
#endif

	/* Enter LCK mode */
	tmp = odm_get_rf_reg(dm, RF_PATH_A, RF_LCK, RFREGOFFSETMASK);
	odm_set_rf_reg(dm, RF_PATH_A, RF_LCK, RFREGOFFSETMASK, tmp | BIT(14));

	/* 3 3. Read RF reg18 */
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);

	/* 3 4. Set LC calibration begin bit15 */
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal | 0x08000);

	ODM_delay_ms(150); /* suggest by RFSI Binson */

	/* Leave LCK mode */
	tmp = odm_get_rf_reg(dm, RF_PATH_A, RF_LCK, RFREGOFFSETMASK);
	odm_set_rf_reg(dm, RF_PATH_A, RF_LCK, RFREGOFFSETMASK, tmp & ~BIT(14));

	/* 3 Restore original situation */
	if ((reg0x914 & 70000) != 0) { /* Deal with contisuous TX case, 0x914[18:16] */
		/* <20121121, Kordan> A workaround: If we set 0x914[18:16] as zero, BB turns off ContTx */
		/* until another packet comes in. To avoid ContTx being turned off, we skip this step. */
		/* odm_write_4byte(dm, REG_SINGLE_TONE_CONT_TX_JAGUAR, reg0x914); */
	} else /* Deal with Packet TX case */
		odm_write_1byte(dm, REG_TXPAUSE, 0x00);

	/* Recover channel number */
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal);

	/*
	odm_set_rf_reg(dm, RF_PATH_A, RF_AC, RFREGOFFSETMASK, rf_amode);
	if(is2T)
		odm_set_rf_reg(dm, RF_PATH_B, RF_AC, RFREGOFFSETMASK, rf_bmode);
		*/
}

void phy_reload_iqk_setting_8812a(struct dm_struct *dm, u8 channel)
{
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);

	u8 chnl_idx = odm_get_right_chnl_place_for_iqk(channel);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
	odm_set_bb_reg(dm, R_0xccc, 0x000007ff, cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][0] & 0x7ff);
	odm_set_bb_reg(dm, R_0xcd4, 0x000007ff, (cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][0] & 0x7ff0000) >> 16);
	odm_set_bb_reg(dm, R_0xecc, 0x000007ff, cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][2] & 0x7ff);
	odm_set_bb_reg(dm, R_0xed4, 0x000007ff, (cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][2] & 0x7ff0000) >> 16);

	if (*dm->band_width != 2) {
		odm_write_4byte(dm, 0xce8, 0x0);
		odm_write_4byte(dm, 0xee8, 0x0);
	} else {
		odm_write_4byte(dm, 0xce8, cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][4]);
		odm_write_4byte(dm, 0xee8, cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][5]);
	}
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_set_bb_reg(dm, R_0xc10, 0x000003ff, (cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][1] & 0x7ff0000) >> 17);
	odm_set_bb_reg(dm, R_0xc10, 0x03ff0000, (cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][1] & 0x7ff) >> 1);
	odm_set_bb_reg(dm, R_0xe10, 0x000003ff, (cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][3] & 0x7ff0000) >> 17);
	odm_set_bb_reg(dm, R_0xe10, 0x03ff0000, (cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][3] & 0x7ff) >> 1);
}

void phy_reset_iqk_result_8812a(struct dm_struct *dm)
{
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
	odm_set_bb_reg(dm, R_0xccc, 0x000007ff, 0x0);
	odm_set_bb_reg(dm, R_0xcd4, 0x000007ff, 0x200);
	odm_set_bb_reg(dm, R_0xecc, 0x000007ff, 0x0);
	odm_set_bb_reg(dm, R_0xed4, 0x000007ff, 0x200);
	odm_write_4byte(dm, 0xce8, 0x0);
	odm_write_4byte(dm, 0xee8, 0x0);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_set_bb_reg(dm, R_0xc10, 0x000003ff, 0x100);
	odm_set_bb_reg(dm, R_0xe10, 0x000003ff, 0x100);
}

void _phy_iq_calibrate_by_fw_8812a(struct dm_struct *dm)
{
	u8 iqk_cmd[3] = {*dm->channel, 0x0, 0x0};
	u8 buf1 = 0x0;
	u8 buf2 = 0x0;

	/* Byte 2, Bit 4 ~ Bit 5 : band_type */
	if (*dm->band_type == ODM_BAND_5G)
		buf1 = 0x2 << 4;
	else
		buf1 = 0x1 << 4;

	/* Byte 2, Bit 0 ~ Bit 3 : bandwidth */
	if (*dm->band_width == CHANNEL_WIDTH_20)
		buf2 = 0x1;
	else if (*dm->band_width == CHANNEL_WIDTH_40)
		buf2 = 0x1 << 1;
	else if (*dm->band_width == CHANNEL_WIDTH_80)
		buf2 = 0x1 << 2;
	else
		buf2 = 0x1 << 3;

	iqk_cmd[1] = buf1 | buf2;
	iqk_cmd[2] = dm->ext_pa_5g | dm->ext_pa << 1 | dm->support_interface << 2 | dm->rfe_type << 5;

	RF_DBG(dm, DBG_RF_IQK, "== FW IQK Start ==\n");
	odm_fill_h2c_cmd(dm, ODM_H2C_IQ_CALIBRATION, 3, iqk_cmd);
	//	fill_h2c_cmd_8812(adapter, 0x45, 3, iqk_cmd);
	//	rtl8812_iqk_wait(adapter, 500);
}

void phy_iq_calibrate_8812a(void *dm_void, boolean is_recovery)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);
	u32 counter = 0;

	if (dm->fw_offload_ability & PHYDM_RF_IQK_OFFLOAD) {
		_phy_iq_calibrate_by_fw_8812a(dm);
		phydm_iqk_wait(dm, 500);
		if (dm->rf_calibrate_info.is_iqk_in_progress)
			RF_DBG(dm, DBG_RF_IQK,
			       "== FW IQK TIMEOUT (Still in progress after 500ms) ==\n");
	} else
		_phy_iq_calibrate_8812a(dm, *dm->channel);
}

void phy_lc_calibrate_8812a(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	_phy_lc_calibrate_8812a(dm, true);
}

void _phy_set_rf_path_switch_8812a(
#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
				   struct dm_struct *dm,
#else
				   void *adapter,
#endif
				   boolean is_main, boolean is2T)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	void *adapter = dm->adapter;
#endif
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct dm_struct *dm = &hal_data->DM_OutSrc;
#endif
#endif

	if (IS_HARDWARE_TYPE_8821(adapter)) {
		if (is_main)
			odm_set_bb_reg(dm, REG_A_RFE_PINMUX_JAGUAR + 4, BIT(29) | BIT28, 0x1); /* Main */
		else
			odm_set_bb_reg(dm, REG_A_RFE_PINMUX_JAGUAR + 4, BIT(29) | BIT28, 0x2); /* Aux */
	} else if (IS_HARDWARE_TYPE_8812(adapter)) {
		if (hal_data->rfe_type == 5) {
			if (is_main) {
				/* WiFi */
				odm_set_bb_reg(dm, REG_ANTSEL_SW_JAGUAR, BIT(1) | BIT(0), 0x2);
				odm_set_bb_reg(dm, REG_ANTSEL_SW_JAGUAR, BIT(9) | BIT(8), 0x3);
			} else {
				/* BT */
				odm_set_bb_reg(dm, REG_ANTSEL_SW_JAGUAR, BIT(1) | BIT(0), 0x1);
				odm_set_bb_reg(dm, REG_ANTSEL_SW_JAGUAR, BIT(9) | BIT(8), 0x3);
			}
		}
	}
}

void phy_set_rf_path_switch_8812a(
#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
				  struct dm_struct *dm,
#else
				  void *adapter,
#endif
				  boolean is_main)
{
#if DISABLE_BB_RF
	return;
#endif

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	_phy_set_rf_path_switch_8812a(dm, is_main, true);
#else
	_phy_set_rf_path_switch_8812a(adapter, is_main, true);
#endif
#endif
}

#if 0

#define DP_BB_REG_NUM 7
#define DP_RF_REG_NUM 1
#define DP_RETRY_LIMIT 10
#define DP_PATH_NUM 2
#define DP_DPK_NUM 3
#define DP_DPK_VALUE_NUM 2

void
_dpk_thermal_compensation(
	struct dm_struct	*dm
)
{
}

void
_DPK_parareload(
	struct dm_struct	*dm,
	u32		*MACBB_backup,
	u32		*backup_macbb_reg,
	u32		MACBB_NUM


)
{
	u32 i;

	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* save MACBB default value */
	for (i = 0; i < MACBB_NUM; i++)
		odm_write_4byte(dm, backup_macbb_reg[i], MACBB_backup[i]);
}


void
_DPK_parabackup(
	struct dm_struct	*dm,
	u32		*MACBB_backup,
	u32		*backup_macbb_reg,
	u32		MACBB_NUM


)
{
	u32 i;

	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* save MACBB default value */
	for (i = 0; i < MACBB_NUM; i++)
		MACBB_backup[i] = odm_read_4byte(dm, backup_macbb_reg[i]);
}

void
_dpk_globalparaset(
	struct dm_struct	*dm
)
{
	/* *************************************** */
	/* set MAC register */
	/* *************************************** */

	/* TX pause */
	odm_write_4byte(dm, 0x520, 0x007f3F0F);

	/* *************************************** */
	/* set BB register */
	/* *************************************** */

	odm_write_4byte(dm, 0x82c, 0x002083d5);

	/* test pin in/out control */
	odm_write_4byte(dm, 0x970, 0x00000000);

	/* path A regcb8[3:0] = h'd, TRSW to TX */
	odm_write_4byte(dm, 0xcb8, 0x0050824d);

	/* path B regeb8[3:0] = h'd, TRSW to TX */
	odm_write_4byte(dm, 0xeb8, 0x0050824d);

	/* reg838[3:0] = h'c, CCA off */
	odm_write_4byte(dm, 0x838, 0x06c8d24c);

	/* path A 3-wire off */
	odm_write_4byte(dm, 0xc00, 0x00000004);

	/* path B 3-wire off */
	odm_write_4byte(dm, 0xe00, 0x00000004);

	/* reg90c[15] = b'1, DAC fifo reset by CSWU */
	odm_write_4byte(dm, 0x90c, 0x00008000);

	/* reset DPK circuit */
	odm_write_4byte(dm, 0xb00, 0x03000100);

	odm_write_4byte(dm, 0xc94, 0x01000001);

	odm_write_4byte(dm, 0xe94, 0x01000001);

	/* *************************************** */
	/* set AFE register */
	/* *************************************** */

	/* path A */

	odm_write_4byte(dm, 0xc68, 0x19791979);
	odm_write_4byte(dm, 0xc6c, 0x19791979);
	odm_write_4byte(dm, 0xc70, 0x19791979);
	odm_write_4byte(dm, 0xc74, 0x19791979);
	odm_write_4byte(dm, 0xc78, 0x19791979);
	odm_write_4byte(dm, 0xc7c, 0x19791979);
	odm_write_4byte(dm, 0xc80, 0x19791979);
	odm_write_4byte(dm, 0xc84, 0x19791979);

	/* force DAC/ADC power on */
	odm_write_4byte(dm, 0xc60, 0x77777777);
	odm_write_4byte(dm, 0xc64, 0x77777777);

	/* path B */

	odm_write_4byte(dm, 0xe68, 0x19791979);
	odm_write_4byte(dm, 0xe6c, 0x19791979);
	odm_write_4byte(dm, 0xe70, 0x19791979);
	odm_write_4byte(dm, 0xe74, 0x19791979);
	odm_write_4byte(dm, 0xe78, 0x19791979);
	odm_write_4byte(dm, 0xe7c, 0x19791979);
	odm_write_4byte(dm, 0xe80, 0x19791979);
	odm_write_4byte(dm, 0xe84, 0x19791979);

	/* force DAC/ADC power on */
	odm_write_4byte(dm, 0xe60, 0x77777777);
	odm_write_4byte(dm, 0xe64, 0x77777777);
}


void
_dpk_get_gain_loss(
	struct dm_struct	*dm,
	u8 path
)
{
	u32 GL_I = 0, GL_Q = 0;
	u32 GL_I_tmp = 0, GL_Q_tmp = 0;

	u32 power_gl;
	u16 scaler[] = {0x4000, 0x41db, 0x43c7, 0x45c3, 0x47cf, 0x49ec, 0x4c19, 0x4e46, 0x5093, 0x52f2, /* 10 */
			0x5560, 0x57cf, 0x5a7f, 0x5d0e, 0x5fbe
		       };
	u8 sindex = 0;
	u32 pagesel = 0, regsel = 0;

	if (path == 0) { /* pathA */
		pagesel = 0;
		regsel = 0;
	} else {	/* pathB */
		pagesel = 0x200;
		regsel = 0x40;
	}

	odm_write_4byte(dm, 0xc90 + pagesel, 0x0601f0bf);
	odm_write_4byte(dm, 0xcb8 + pagesel, 0x0c000000);



	GL_I_tmp = odm_get_bb_reg(dm, 0xd00 + regsel, 0xffff0000);
	GL_Q_tmp = odm_get_bb_reg(dm, 0xd00 + regsel, 0x0000ffff);

	if (GL_I_tmp >= 0x8000)
		GL_I = (GL_I_tmp - 0x8000 + 0x1);
	else
		GL_I = GL_I_tmp;


	if (GL_Q_tmp >= 0x8000)
		GL_Q = (GL_Q_tmp - 0x8000 + 0x1);
	else
		GL_Q = GL_Q_tmp;

	power_gl = ((GL_I * GL_I) + (GL_Q * GL_Q)) / 4;
	RF_DBG(dm, DBG_RF_IQK, "power_gl = 0x%x", power_gl);

	if (power_gl > 63676) {
		sindex = 0;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 0 dB\n");
	} else if (63676 >= power_gl && power_gl > 60114) {
		sindex = 1;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	0.25 dB\n");
	} else if (60114 >= power_gl && power_gl > 56751) {
		sindex = 2;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	0.5 dB\n");
	} else if (56751 >= power_gl && power_gl > 53577) {
		sindex = 3;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	0.75 dB\n");
	} else if (53577 >= power_gl && power_gl > 49145) {
		sindex = 4;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	1 dB\n");
	} else if (49145 >= power_gl && power_gl > 47750) {
		sindex = 5;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	1.25 dB\n");
	} else if (47750 >= power_gl && power_gl > 45079) {
		sindex = 6;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	1.5 dB\n");
	} else if (45079 >= power_gl && power_gl > 42557) {
		sindex = 7;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	1.75 dB\n");
	} else if (42557 >= power_gl && power_gl > 40177) {
		sindex = 8;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	2 dB\n");
	} else if (40177 >= power_gl && power_gl > 37929) {
		sindex = 9;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	2.25 dB\n");
	} else if (37929 >= power_gl && power_gl > 35807) {
		sindex = 10;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	2.5 dB\n");
	} else if (35807 >= power_gl && power_gl > 33804) {
		sindex = 11;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	2.75 dB\n");
	} else if (33804 >= power_gl && power_gl > 31913) {
		sindex = 12;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	3 dB\n");
	} else if (31913 >= power_gl && power_gl > 30128) {
		sindex = 13;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	3.25 dB\n");
	} else if (30128 >= power_gl) {
		sindex = 14;
		RF_DBG(dm, DBG_RF_IQK, "Gainloss = 	3.5 dB\n");
	}


	odm_write_4byte(dm, 0xc98 + pagesel, (scaler[sindex] << 16) | scaler[sindex]);
	RF_DBG(dm, DBG_RF_IQK, "Set Gainloss reg 0xc98(0xe98)= 0x%x\n",
	       odm_read_4byte(dm, 0xc98 + pagesel));
}


void
_dpk_enable_dp(
	struct dm_struct	*dm,
	u8 path,
	u32 tx_index
)
{
	/* *************************************** */
	/* Enable DP */
	/* *************************************** */

	/* PWSF[6] = 0x40 = 0dB, set the address represented tx_index as 0dB */
	u8 PWSF[] = { 0xff, 0xca, 0xa1, 0x80, 0x65, 0x51, 0x40,  /* 6~0dB */
		      0x33, 0x28, 0x20, 0x19, 0x14, 0x10, 0x0d,  /* -1~-7dB */
		      0x0a, 0x08, 0x06, 0x05, 0x04, 0x03, 0x03,  /* -8~-14dB */
		      0x02, 0x02, 0x01, 0x01,
		    };
	u8 zeropoint;
	u8 pwsf1, pwsf2;
	u8 i;
	u32 pagesel = 0, regsel = 0;

	if (path == 0) {
		pagesel = 0;
		regsel = 0;
	} else {
		pagesel = 0x200;
		regsel = 0x40;
	}


	/* ========= */
	/* DPK setting	 */
	/* ========= */
	odm_write_4byte(dm, 0x82c, 0x802083d5);


	odm_write_4byte(dm, 0xc90 + pagesel, 0x0000f098);
	odm_write_4byte(dm, 0xc94 + pagesel, 0x776c9f84);
	odm_write_4byte(dm, 0xcc4 + pagesel, 0x08840000);
	odm_write_4byte(dm, 0xcc8 + pagesel, 0x20000000);
	odm_write_4byte(dm, 0xc8c + pagesel, 0x3c000000);


	odm_write_4byte(dm, 0xc20 + pagesel, 0x00000800);

	RF_DBG(dm, DBG_RF_IQK, "Write PWSF table\n");


	if (tx_index == 0x1f)
		zeropoint = 0;
	else if (tx_index == 0x1e)
		zeropoint = 1;
	else if (tx_index == 0x1d)
		zeropoint = 2;
	else if (tx_index == 0x1c)
		zeropoint = 3;
	else if (tx_index == 0x1b)
		zeropoint = 4;
	else if (tx_index == 0x1a)
		zeropoint = 5;
	else if (tx_index == 0x19)
		zeropoint = 6;
	else
		zeropoint = 6;



	for (i = 0; i < 16; i++) {
		pwsf1 = (6 - zeropoint) + i * 2;
		if (pwsf1 > 24)
			pwsf1 = 24;

		pwsf2 = (6 - zeropoint - 1) + i * 2;
		if (pwsf2 > 24)
			pwsf2 = 24;

		odm_write_4byte(dm, 0xce4 + pagesel, 0x00000001 | i << 1 | (PWSF[pwsf1] << 8) | (PWSF[pwsf2] << 16));
		RF_DBG(dm, DBG_RF_IQK, "0x%x\n",
		       odm_read_4byte(dm, 0xce4 + pagesel));
		odm_set_bb_reg(dm, 0xce4 + pagesel, 0xff, 0x0);
	}

	odm_write_4byte(dm, 0xce4 + pagesel, 0x00000000);

	odm_write_4byte(dm, 0x82c, 0x002083d5);
}


void
_dpk_path_abdpk(
	struct dm_struct	*dm
)
{
	u32 tx_index = 0;
	u8 path = 0;
	u32 pagesel = 0, regsel = 0;
	u32 i = 0, j = 0;

	for (path = 0; path < 2; path++) {	/* path A = 0; path B = 1; */
		RF_DBG(dm, DBG_RF_IQK, "path %s DPK start!!!\n",
		       (path == 0) ? "A" : "B");

		if (path == 0) {
			pagesel = 0;
			regsel = 0;
		} else {
			pagesel = 0x200;
			regsel = 0x40;
		}

		/* *************************************** */
		/* find compress-2.5dB TX index */
		/* *************************************** */


		odm_write_4byte(dm, 0x82c, 0x802083d5);

		/* dB_sel = b'000 ' 1.0 dB PA gain loss */
		/* dB_sel = b'001 ' 1.5 dB PA gain loss */
		/* dB_sel = b'010 ' 2.0 dB PA gain loss */
		/* dB_sel = b'011 ' 2.5 dB PA gain loss */
		/* dB_sel = b'100 ' 3.0 dB PA gain loss */
		/* dB_sel = b'101 ' 3.5 dB PA gain loss */
		/* dB_sel = b'110 ' 4.0 dB PA gain loss */
		odm_write_4byte(dm, 0xc20 + pagesel, 0x00006000);

		odm_write_4byte(dm, 0xc90 + pagesel, 0x0401e038);
		odm_write_4byte(dm, 0xc94 + pagesel, 0xf76c9f84);
		odm_write_4byte(dm, 0xcc8 + pagesel, 0x000c5599);
		odm_write_4byte(dm, 0xcc4 + pagesel, 0x148b0000);
		odm_write_4byte(dm, 0xc8c + pagesel, 0x3c000000);

		odm_write_4byte(dm, 0xc98 + pagesel, 0x41382e21);
		odm_write_4byte(dm, 0xc9c + pagesel, 0x5b554f48);
		odm_write_4byte(dm, 0xca0 + pagesel, 0x6f6b6661);
		odm_write_4byte(dm, 0xca4 + pagesel, 0x817d7874);
		odm_write_4byte(dm, 0xca8 + pagesel, 0x908c8884);
		odm_write_4byte(dm, 0xcac + pagesel, 0x9d9a9793);
		odm_write_4byte(dm, 0xcb0 + pagesel, 0xaaa7a4a1);
		odm_write_4byte(dm, 0xcb4 + pagesel, 0xb6b3b0ad);

		odm_write_4byte(dm, 0xc40 + pagesel, 0x02ce03e9);
		odm_write_4byte(dm, 0xc44 + pagesel, 0x01fd0249);
		odm_write_4byte(dm, 0xc48 + pagesel, 0x01a101c9);
		odm_write_4byte(dm, 0xc4c + pagesel, 0x016a0181);
		odm_write_4byte(dm, 0xc50 + pagesel, 0x01430155);
		odm_write_4byte(dm, 0xc54 + pagesel, 0x01270135);
		odm_write_4byte(dm, 0xc58 + pagesel, 0x0112011c);
		odm_write_4byte(dm, 0xc5c + pagesel, 0x01000108);
		odm_write_4byte(dm, 0xc60 + pagesel, 0x00f100f8);
		odm_write_4byte(dm, 0xc64 + pagesel, 0x00e500eb);
		odm_write_4byte(dm, 0xc68 + pagesel, 0x00db00e0);
		odm_write_4byte(dm, 0xc6c + pagesel, 0x00d100d5);
		odm_write_4byte(dm, 0xc70 + pagesel, 0x00c900cd);
		odm_write_4byte(dm, 0xc74 + pagesel, 0x00c200c5);
		odm_write_4byte(dm, 0xc78 + pagesel, 0x00bb00be);
		odm_write_4byte(dm, 0xc7c + pagesel, 0x00b500b8);

		/* ============ */
		/* RF setting for DPK */
		/* ============ */

		/* pathA,pathB standby mode */
		odm_set_rf_reg(dm, (enum rf_path)0x0, RF_0x0, RFREGOFFSETMASK, 0x10000);
		odm_set_rf_reg(dm, (enum rf_path)0x1, RF_0x0, RFREGOFFSETMASK, 0x10000);

		/* 00[4:0] = Tx AGC, 00[9:5] = Rx AGC (BB), 00[12:10] = Rx AGC (LNA) */
		odm_set_rf_reg(dm, (enum rf_path)(0x0 + path), RF_0x0, RFREGOFFSETMASK, 0x50bff);


		/* 64[14:12] = loop back attenuation */
		odm_set_rf_reg(dm, (enum rf_path)(0x0 + path), RF_0x64, RFREGOFFSETMASK, 0x19aac);

		/* 8f[14:13] = PGA2 gain */
		odm_set_rf_reg(dm, (enum rf_path)(0x0 + path), RF_0x8f, RFREGOFFSETMASK, 0x8e001);


		/* one shot */
		odm_write_4byte(dm, 0xcc8 + pagesel, 0x800c5599);
		odm_write_4byte(dm, 0xcc8 + pagesel, 0x000c5599);


		/* delay 100 ms */
		ODM_delay_ms(100);


		/* read back */
		odm_write_4byte(dm, 0xc90 + pagesel, 0x0109f018);
		odm_write_4byte(dm, 0xcb8 + pagesel, 0x09000000);

		tx_index = odm_get_bb_reg(dm, 0xd00 + regsel, 0x0000000f);


		/* *************************************** */
		/* get LUT */
		/* *************************************** */

		odm_write_4byte(dm, 0xc90 + pagesel, 0x0001e038);
		odm_write_4byte(dm, 0xc94 + pagesel, 0xf76c9f84);
		odm_write_4byte(dm, 0xcc8 + pagesel, 0x400c5599);
		odm_write_4byte(dm, 0xcc4 + pagesel, 0x11930080);		/* 0xcc4[9:4]= DPk fail threshold */
		odm_write_4byte(dm, 0xc8c + pagesel, 0x3c000000);



		odm_write_4byte(dm, 0xc98 + pagesel, 0x41382e21);
		odm_write_4byte(dm, 0xc9c + pagesel, 0x5b554f48);
		odm_write_4byte(dm, 0xca0 + pagesel, 0x6f6b6661);
		odm_write_4byte(dm, 0xca4 + pagesel, 0x817d7874);
		odm_write_4byte(dm, 0xca8 + pagesel, 0x908c8884);
		odm_write_4byte(dm, 0xcac + pagesel, 0x9d9a9793);
		odm_write_4byte(dm, 0xcb0 + pagesel, 0xaaa7a4a1);
		odm_write_4byte(dm, 0xcb4 + pagesel, 0xb6b3b0ad);

		odm_write_4byte(dm, 0xc40 + pagesel, 0x02ce03e9);
		odm_write_4byte(dm, 0xc44 + pagesel, 0x01fd0249);
		odm_write_4byte(dm, 0xc48 + pagesel, 0x01a101c9);
		odm_write_4byte(dm, 0xc4c + pagesel, 0x016a0181);
		odm_write_4byte(dm, 0xc50 + pagesel, 0x01430155);
		odm_write_4byte(dm, 0xc54 + pagesel, 0x01270135);
		odm_write_4byte(dm, 0xc58 + pagesel, 0x0112011c);
		odm_write_4byte(dm, 0xc5c + pagesel, 0x01000108);
		odm_write_4byte(dm, 0xc60 + pagesel, 0x00f100f8);
		odm_write_4byte(dm, 0xc64 + pagesel, 0x00e500eb);
		odm_write_4byte(dm, 0xc68 + pagesel, 0x00db00e0);
		odm_write_4byte(dm, 0xc6c + pagesel, 0x00d100d5);
		odm_write_4byte(dm, 0xc70 + pagesel, 0x00c900cd);
		odm_write_4byte(dm, 0xc74 + pagesel, 0x00c200c5);
		odm_write_4byte(dm, 0xc78 + pagesel, 0x00bb00be);
		odm_write_4byte(dm, 0xc7c + pagesel, 0x00b500b8);

		/* fill BB TX index for the DPK reference */
		odm_write_4byte(dm, 0x82c, 0x002083d5);

		odm_write_4byte(dm, 0xc20 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc24 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc28 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc2c + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc30 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc34 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc38 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc3c + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc40 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc44 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc48 + pagesel, 0x3c3c3c3c);
		odm_write_4byte(dm, 0xc4c + pagesel, 0x3c3c3c3c);

		odm_write_4byte(dm, 0x82c, 0x802083d5);



		/* r_agc_boudary */
		odm_write_4byte(dm, 0xcbc + pagesel, 0x0006c23f);

		odm_write_4byte(dm, 0xcb8 + pagesel, 0x000fffff);

		/* ============ */
		/* RF setting for DPK */
		/* ============ */
		/* 00[4:0] = Tx AGC, 00[9:5] = Rx AGC (BB), 00[12:10] = Rx AGC (LNA) */
		odm_set_rf_reg(dm, (enum rf_path)(0x0 + path), RF_0x0, RFREGOFFSETMASK, 0x517e0 | tx_index);
		RF_DBG(dm, DBG_RF_IQK, "RF 0x0 = 0x%x\n", 0x517e0 | tx_index);

		/* one shot */
		odm_write_4byte(dm, 0xcc8 + pagesel, 0xc00c5599);
		odm_write_4byte(dm, 0xcc8 + pagesel, 0x400c5599);

		/* delay 100 ms */
		ODM_delay_ms(100);

		/* read back dp_fail report */
		odm_write_4byte(dm, 0xcb8 + pagesel, 0x00000000);

		/* if d00[6] = 1, DPK fail */
		if (odm_get_bb_reg(dm, 0xd00 + regsel, BIT(6))) {
			/* bypass DPK */
			odm_write_4byte(dm, 0xcc4 + pagesel, 0x28848000);
			RF_DBG(dm, DBG_RF_IQK,
			       "path %s DPK fail!!!!!!!!!!!!!!!!!!!!!\n",
			       (path == 0) ? "A" : "B");

			return;
		}

		RF_DBG(dm, DBG_RF_IQK, "path %s DPK ok!!!!!!!!!!!!!!!!!!!!!\n",
		       (path == 0) ? "A" : "B");



		/* read LMS table -->debug message only */
		RF_DBG(dm, DBG_RF_IQK, "read LMS\n");

		for (i = 0; i < 8; i++) {
			odm_write_4byte(dm, 0xc90 + pagesel, 0x0601f0b8 + i);
			for (j = 0; j < 4; j++) {
				odm_write_4byte(dm, 0xcb8 + pagesel, 0x09000000 + (j << 24));
				RF_DBG(dm, DBG_RF_IQK, "0x%x",
				       odm_read_4byte(dm, 0xd00 + regsel));
			}
		}


		/* *************************************** */
		/* get gain loss */
		/* *************************************** */

		_dpk_get_gain_loss(dm, path);


		/* *************************************** */
		/* Enable DP */
		/* *************************************** */

		_dpk_enable_dp(dm, path, tx_index);

	}

}



void
_phy_dp_calibrate_8812a(
	struct dm_struct	*dm
)
{
	u32 backup_reg_addrs[] = {
		0x970, 0xcb8, 0x838, 0xc00, 0x90c, 0xb00, 0xc94, 0x82c, 0x520, 0xc60, /* 10 */
		0xc64, 0xc68, 0xc6c, 0xc70, 0xc74, 0xc78, 0xc7c, 0xc80, 0xc84, 0xc50, /* 20 */
		0xc20, 0xc24, 0xc28, 0xc2c, 0xc30, 0xc34, 0xc38, 0xc3c, 0xc40, 0xc44, /* 30 */
		0xc48, 0xc4c, 0xe50, 0xe20, 0xe24, 0xe28, 0xe2c, 0xe30, 0xe34, 0xe38, /* 40 */
		0xe3c, 0xe40, 0xe44, 0xe48, 0xe4c, 0xeb8, 0xe00, 0xe94, 0xe60, 0xe64, /* 50 */
		0xe68, 0xe6c, 0xe70, 0xe74, 0xe78, 0xe7c, 0xe80, 0xe84
	};

	u32 backup_reg_data[sizeof(backup_reg_addrs) / sizeof(u32)];


	/* backup BB&MAC default value */

	_DPK_parabackup(dm, backup_reg_data, backup_reg_addrs, sizeof(backup_reg_addrs) / sizeof(u32));

	/* set global parameters */
	_dpk_globalparaset(dm);

	/* DPK */
	_dpk_path_abdpk(dm);

	/* TH_DPK=thermalmeter */


	/* reload BB&MAC defaul value; */
	_DPK_parareload(dm, backup_reg_data, backup_reg_addrs, sizeof(backup_reg_addrs) / sizeof(u32));
}
#endif

void phy_dp_calibrate_8812a(struct dm_struct *dm)
{
#if 0
	struct _hal_rf_			*rf = &(dm->rf_table);

	rf->dpk_done = true;
	RF_DBG(dm, DBG_RF_IQK, "===> %s\n", __func__);
	_phy_dp_calibrate_8812a(dm);
	RF_DBG(dm, DBG_RF_IQK, "<=== %s\n", __func__);
#endif
}
