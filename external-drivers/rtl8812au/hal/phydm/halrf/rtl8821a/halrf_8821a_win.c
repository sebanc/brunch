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

#if RT_PLATFORM==PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../phydm_precomp.h"
#endif



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


void set_iqk_matrix_8821a(
	struct dm_struct	*dm,
	u8		OFDM_index,
	u8		rf_path,
	s32		iqk_result_x,
	s32		iqk_result_y
)
{
	s32			ele_A = 0, ele_D, ele_C = 0, value32;

	ele_D = (ofdm_swing_table_new[OFDM_index] & 0xFFC00000) >> 22;

	/* new element A = element D x X */
	if ((iqk_result_x != 0) && (*(dm->band_type) == ODM_BAND_2_4G)) {
		if ((iqk_result_x & 0x00000200) != 0)	/* consider minus */
			iqk_result_x = iqk_result_x | 0xFFFFFC00;
		ele_A = ((iqk_result_x * ele_D) >> 8) & 0x000003FF;

		/* new element C = element D x Y */
		if ((iqk_result_y & 0x00000200) != 0)
			iqk_result_y = iqk_result_y | 0xFFFFFC00;
		ele_C = ((iqk_result_y * ele_D) >> 8) & 0x000003FF;

		if (rf_path == RF_PATH_A)
			switch (rf_path) {
			case RF_PATH_A:
				/* wirte new elements A, C, D to regC80 and regC94, element B is always 0 */
				value32 = (ele_D << 22) | ((ele_C & 0x3F) << 16) | ele_A;
				odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD, value32);

				value32 = (ele_C & 0x000003C0) >> 6;
				odm_set_bb_reg(dm, REG_OFDM_0_XC_TX_AFE, MASKH4BITS, value32);

				value32 = ((iqk_result_x * ele_D) >> 7) & 0x01;
				odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(24), value32);
				break;
			default:
				break;
			}
	} else {
		switch (rf_path) {
		case RF_PATH_A:
			odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD, ofdm_swing_table_new[OFDM_index]);
			odm_set_bb_reg(dm, REG_OFDM_0_XC_TX_AFE, MASKH4BITS, 0x00);
			odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(24), 0x00);
			break;

		default:
			break;
		}
	}

	PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "TxPwrTracking path B: X = 0x%x, Y = 0x%x ele_A = 0x%x ele_C = 0x%x ele_D = 0x%x 0xeb4 = 0x%x 0xebc = 0x%x\n",
		(u32)iqk_result_x, (u32)iqk_result_y, (u32)ele_A, (u32)ele_C, (u32)ele_D, (u32)iqk_result_x, (u32)iqk_result_y);
}

void do_iqk_8821a(
	void *dm_void,
	u8		delta_thermal_index,
	u8		thermal_value,
	u8		threshold
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;

	odm_reset_iqk_result(dm);
	dm->rf_calibrate_info.thermal_value_iqk = thermal_value;
	halrf_iqk_trigger(dm, false);
}


void
odm_tx_pwr_track_set_pwr8821a(
	void *dm_void,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	PADAPTER   adapter = (PADAPTER)dm->adapter;

	u8			pwr_tracking_limit = 26; /* +1.0dB */
	u8			tx_rate = 0xFF;
	u8			final_ofdm_swing_index = 0;
	u8			final_cck_swing_index = 0;
	u32			final_bb_swing_idx[1];
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
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
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8)rate;
	}

	PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "Power Tracking tx_rate=0x%X\n", tx_rate);
	PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "===>odm_tx_pwr_track_set_pwr8821a\n");

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
	PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "tx_rate=0x%x, pwr_tracking_limit=%d\n", tx_rate, pwr_tracking_limit);

	if (method == BBSWING) {
		if (rf_path == RF_PATH_A) {
			final_bb_swing_idx[RF_PATH_A] = (cali_info->OFDM_index[RF_PATH_A] > pwr_tracking_limit) ? pwr_tracking_limit : cali_info->OFDM_index[RF_PATH_A];
			PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "cali_info->OFDM_index[RF_PATH_A]=%d, dm->RealBbSwingIdx[RF_PATH_A]=%d\n",
				cali_info->OFDM_index[RF_PATH_A], final_bb_swing_idx[RF_PATH_A]);

			odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_bb_swing_idx[RF_PATH_A]]);
		}
	} else if (method == MIX_MODE) {
		PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "cali_info->default_ofdm_index=%d, cali_info->absolute_ofdm_swing_idx[rf_path]=%d, rf_path = %d\n",
			cali_info->default_ofdm_index, cali_info->absolute_ofdm_swing_idx[rf_path], rf_path);

		final_cck_swing_index = cali_info->default_cck_index + cali_info->absolute_ofdm_swing_idx[rf_path];
		final_ofdm_swing_index = cali_info->default_ofdm_index + cali_info->absolute_ofdm_swing_idx[rf_path];

		if (rf_path == RF_PATH_A) {
			if (final_ofdm_swing_index > pwr_tracking_limit) {  /* BBSwing higher then Limit */
				cali_info->remnant_cck_swing_idx = final_cck_swing_index - pwr_tracking_limit;
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index - pwr_tracking_limit;

				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[pwr_tracking_limit]);

				cali_info->modify_tx_agc_flag_path_a = true;

				PHY_SetTxPowerLevelByPath(adapter, *dm->channel, RF_PATH_A);

				PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "******Path_A Over BBSwing Limit, pwr_tracking_limit = %d, Remnant tx_agc value = %d\n", pwr_tracking_limit, cali_info->remnant_ofdm_swing_idx[rf_path]);
			} else if (final_ofdm_swing_index < 0) {
				cali_info->remnant_cck_swing_idx = final_cck_swing_index;
				cali_info->remnant_ofdm_swing_idx[rf_path] = final_ofdm_swing_index;

				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[0]);

				cali_info->modify_tx_agc_flag_path_a = true;

				PHY_SetTxPowerLevelByPath(adapter, *dm->channel, RF_PATH_A);

				PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "******Path_A Lower then BBSwing lower bound  0, Remnant tx_agc value = %d\n", cali_info->remnant_ofdm_swing_idx[rf_path]);
			} else {
				odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[final_ofdm_swing_index]);

				PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "******Path_A Compensate with BBSwing, final_ofdm_swing_index = %d\n", final_ofdm_swing_index);

				if (cali_info->modify_tx_agc_flag_path_a) { /* If tx_agc has changed, reset tx_agc again */
					cali_info->remnant_cck_swing_idx = 0;
					cali_info->remnant_ofdm_swing_idx[rf_path] = 0;

					PHY_SetTxPowerLevelByPath(adapter, *dm->channel, RF_PATH_A);

					cali_info->modify_tx_agc_flag_path_a = false;

					PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "******Path_A dm->Modify_TxAGC_Flag = false\n");
				}
			}
		}
	} else
		return;
}	/* odm_TxPwrTrackSetPwr88E */

void
get_delta_swing_table_8821a(
	void *dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	PADAPTER      adapter = (PADAPTER)dm->adapter;
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);
	u8		tx_rate			= 0xFF;
	u8	channel		 = *dm->channel;

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
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
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8)rate;
	}

	PHYDM_DBG(dm, ODM_COMP_TX_PWR_TRACK, "Power Tracking tx_rate=0x%X\n", tx_rate);


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
	config->phy_lc_calibrate = phy_lc_calibrate_8821a;
	config->get_delta_swing_table = get_delta_swing_table_8821a;
}

/* 1 7.	IQK */
#define MAX_TOLERANCE		5
#define IQK_DELAY_TIME		1		/* ms */

void _iqk_rx_fill_iqc_8821a(
	struct dm_struct			*dm,
	enum rf_path	path,
	unsigned int			RX_X,
	unsigned int			RX_Y
)
{
	switch (path) {
	case RF_PATH_A:
	{
		odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
		odm_set_bb_reg(dm, 0xc10, 0x000003ff, RX_X >> 1);
		odm_set_bb_reg(dm, 0xc10, 0x03ff0000, RX_Y >> 1);
		PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "RX_X = %x;;RX_Y = %x ====>fill to IQC\n", RX_X >> 1, RX_Y >> 1);
		PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "0xc10 = %x ====>fill to IQC\n", odm_read_4byte(dm, 0xc10));
	}
	break;
	default:
		break;
	};
}

void _iqk_tx_fill_iqc_8821a(
	struct dm_struct			*dm,
	enum rf_path	path,
	unsigned int			TX_X,
	unsigned int			TX_Y
)
{
	switch (path) {
	case RF_PATH_A:
	{
		odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
		odm_write_4byte(dm, 0xc90, 0x00000080);
		odm_write_4byte(dm, 0xcc4, 0x20040000);
		odm_write_4byte(dm, 0xcc8, 0x20000000);
		odm_set_bb_reg(dm, 0xccc, 0x000007ff, TX_Y);
		odm_set_bb_reg(dm, 0xcd4, 0x000007ff, TX_X);
		PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "TX_X = %x;;TX_Y = %x =====> fill to IQC\n", TX_X, TX_Y);
		PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "0xcd4 = %x;;0xccc = %x ====>fill to IQC\n", odm_get_bb_reg(dm, 0xcd4, 0x000007ff), odm_get_bb_reg(dm, 0xccc, 0x000007ff));
	}
	break;
	default:
		break;
	};
}

void _iqk_backup_mac_bb_8821a(
	struct dm_struct	*dm,
	u32		*MACBB_backup,
	u32		*backup_macbb_reg,
	u32		MACBB_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* save MACBB default value */
	for (i = 0; i < MACBB_NUM; i++)
		MACBB_backup[i] = odm_read_4byte(dm, backup_macbb_reg[i]);

	PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "BackupMacBB Success!!!!\n");
}
void _iqk_backup_rf_8821a(
	struct dm_struct	*dm,
	u32		*RFA_backup,
	u32		*RFB_backup,
	u32		*backup_rf_reg,
	u32		RF_NUM
)
{

	u32 i;
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Save RF Parameters */
	for (i = 0; i < RF_NUM; i++)
		RFA_backup[i] = odm_get_rf_reg(dm, RF_PATH_A, backup_rf_reg[i], MASKDWORD);
	PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "BackupRF Success!!!!\n");
}
void _iqk_backup_afe_8821a(
	struct dm_struct		*dm,
	u32		*AFE_backup,
	u32		*backup_afe_reg,
	u32		AFE_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Save AFE Parameters */
	for (i = 0; i < AFE_NUM; i++)
		AFE_backup[i] = odm_read_4byte(dm, backup_afe_reg[i]);
	PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "BackupAFE Success!!!!\n");
}
void _iqk_restore_mac_bb_8821a(
	struct dm_struct		*dm,
	u32		*MACBB_backup,
	u32		*backup_macbb_reg,
	u32		MACBB_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Reload MacBB Parameters */
	for (i = 0; i < MACBB_NUM; i++)
		odm_write_4byte(dm, backup_macbb_reg[i], MACBB_backup[i]);
	PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "RestoreMacBB Success!!!!\n");
}
void _iqk_restore_rf_8821a(
	struct dm_struct			*dm,
	enum rf_path	path,
	u32			*backup_rf_reg,
	u32			*RF_backup,
	u32			RF_REG_NUM
)
{
	u32 i;

	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	for (i = 0; i < RF_REG_NUM; i++)
		odm_set_rf_reg(dm, (enum rf_path)path, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i]);

	switch (path) {
	case RF_PATH_A:
	{
		PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "RestoreRF path A Success!!!!\n");
	}
	break;
	default:
		break;
	}
}
void _iqk_restore_afe_8821a(
	struct dm_struct		*dm,
	u32		*AFE_backup,
	u32		*backup_afe_reg,
	u32		AFE_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Reload AFE Parameters */
	for (i = 0; i < AFE_NUM; i++)
		odm_write_4byte(dm, backup_afe_reg[i], AFE_backup[i]);
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
	odm_write_4byte(dm, 0xc80, 0x0);
	odm_write_4byte(dm, 0xc84, 0x0);
	odm_write_4byte(dm, 0xc88, 0x0);
	odm_write_4byte(dm, 0xc8c, 0x3c000000);
	odm_write_4byte(dm, 0xc90, 0x00000080);
	odm_write_4byte(dm, 0xc94, 0x00000000);
	odm_write_4byte(dm, 0xcc4, 0x20040000);
	odm_write_4byte(dm, 0xcc8, 0x20000000);
	odm_write_4byte(dm, 0xcb8, 0x0);
	PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "RestoreAFE Success!!!!\n");
}


void _iqk_configure_mac_8821a(
	struct dm_struct		*dm
)
{
	/* ========MAC register setting======== */
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_write_1byte(dm, 0x522, 0x3f);
	odm_set_bb_reg(dm, 0x550, BIT(11) | BIT(3), 0x0);
	odm_write_1byte(dm, 0x808, 0x00);		/*		RX ante off */
	odm_set_bb_reg(dm, 0x838, 0xf, 0xc);		/*		CCA off */
	odm_write_1byte(dm, 0xa07, 0xf);		/*		CCK RX path off */
}

#define cal_num 3

void _iqk_tx_8821a(
	struct dm_struct		*dm,
	enum rf_path path
)
{
	u32		TX_fail, RX_fail, delay_count, IQK_ready, cal_retry, cal = 0;
	int		TX_X = 0, TX_Y = 0, RX_X = 0, RX_Y = 0, tx_average = 0, rx_average = 0, rx_iqk_loop = 0, RX_X_temp = 0, RX_Y_temp = 0;
	int		TX_X0[cal_num], TX_Y0[cal_num], RX_X0[2][cal_num], RX_Y0[2][cal_num];
	boolean	TX0IQKOK = false, RX0IQKOK = false;
	int			i, ii, dx = 0, dy = 0, TX_finish = 0, RX_finish1 = 0, RX_finish2 = 0;


	PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "band_width = %d, support_interface = %d, ext_pa = %d, ext_pa_5g = %d\n", *dm->band_width, dm->support_interface, dm->ext_pa, dm->ext_pa_5g);

	while (cal < cal_num) {
		switch (path) {
		case RF_PATH_A:
		{
			/* path-A LOK */
			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
			/* ========path-A AFE all on======== */
			/* Port 0 DAC/ADC on */
			odm_write_4byte(dm, 0xc60, 0x77777777);
			odm_write_4byte(dm, 0xc64, 0x77777777);

			odm_write_4byte(dm, 0xc68, 0x19791979);

			odm_set_bb_reg(dm, 0xc00, 0xf, 0x4);/*	hardware 3-wire off */

			/* LOK setting */
			/* ====== LOK ====== */
			/* 1. DAC/ADC sampling rate (160 MHz) */
			odm_set_bb_reg(dm, 0xc5c, BIT(26) | BIT(25) | BIT(24), 0x7);

			/* 2. LoK RF setting (at BW = 20M) */
			odm_set_rf_reg(dm, (enum rf_path)path, 0xef, RFREGOFFSETMASK, 0x80002);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x18, 0x00c00, 0x3);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x30, RFREGOFFSETMASK, 0x20000);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x31, RFREGOFFSETMASK, 0x0003f);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x32, RFREGOFFSETMASK, 0xf3fc3);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x65, RFREGOFFSETMASK, 0x931d5);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, RFREGOFFSETMASK, 0x8a001);
			odm_write_4byte(dm, 0x90c, 0x00008000);
			odm_set_bb_reg(dm, 0xc94, BIT(0), 0x1);
			odm_write_4byte(dm, 0x978, 0x29002000);/* TX (X,Y) */
			odm_write_4byte(dm, 0x97c, 0xa9002000);/* RX (X,Y) */
			odm_write_4byte(dm, 0x984, 0x00462910);/* [0]:AGC_en, [15]:idac_K_Mask */

			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */

			if (dm->ext_pa_5g)
				odm_write_4byte(dm, 0xc88, 0x821403f7);
			else
				odm_write_4byte(dm, 0xc88, 0x821403f4);

			if (*dm->band_type == ODM_BAND_5G)
				odm_write_4byte(dm, 0xc8c, 0x68163e96);
			else
				odm_write_4byte(dm, 0xc8c, 0x28163e96);

			odm_write_4byte(dm, 0xc80, 0x18008c10);/* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
			odm_write_4byte(dm, 0xc84, 0x38008c10);/* RX_Tone_idx[9:0], RxK_Mask[29] */
			odm_write_4byte(dm, 0xcb8, 0x00100000);/* cb8[20] N SI/PI iqk_dpk module */
			odm_write_4byte(dm, 0x980, 0xfa000000);
			odm_write_4byte(dm, 0x980, 0xf8000000);

			delay_ms(10); /* delay 10ms */
			odm_write_4byte(dm, 0xcb8, 0x00000000);

			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
			odm_set_rf_reg(dm, (enum rf_path)path, 0x58, 0x7fe00, odm_get_rf_reg(dm, (enum rf_path)path, 0x8, 0xffc00));
			switch (*dm->band_width) {
			case 1:
			{
				odm_set_rf_reg(dm, (enum rf_path)path, 0x18, 0x00c00, 0x1);
			}
			break;
			case 2:
			{
				odm_set_rf_reg(dm, (enum rf_path)path, 0x18, 0x00c00, 0x0);
			}
			break;
			default:
				break;

			}
			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */

			/* 3. TX RF setting */
			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
			odm_set_rf_reg(dm, (enum rf_path)path, 0xef, RFREGOFFSETMASK, 0x80000);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x30, RFREGOFFSETMASK, 0x20000);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x31, RFREGOFFSETMASK, 0x0003f);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x32, RFREGOFFSETMASK, 0xf3fc3);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x65, RFREGOFFSETMASK, 0x931d5);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, RFREGOFFSETMASK, 0x8a001);
			odm_set_rf_reg(dm, (enum rf_path)path, 0xef, RFREGOFFSETMASK, 0x00000);
			odm_write_4byte(dm, 0x90c, 0x00008000);
			odm_set_bb_reg(dm, 0xc94, BIT(0), 0x1);
			odm_write_4byte(dm, 0x978, 0x29002000);/* TX (X,Y) */
			odm_write_4byte(dm, 0x97c, 0xa9002000);/* RX (X,Y) */
			odm_write_4byte(dm, 0x984, 0x0046a910);/* [0]:AGC_en, [15]:idac_K_Mask */

			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */

			if (dm->ext_pa_5g)
				odm_write_4byte(dm, 0xc88, 0x821403f7);
			else
				odm_write_4byte(dm, 0xc88, 0x821403e3);

			if (*dm->band_type == ODM_BAND_5G)
				odm_write_4byte(dm, 0xc8c, 0x40163e96);
			else
				odm_write_4byte(dm, 0xc8c, 0x00163e96);

			odm_write_4byte(dm, 0xc80, 0x18008c10);/* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
			odm_write_4byte(dm, 0xc84, 0x38008c10);/* RX_Tone_idx[9:0], RxK_Mask[29] */
			odm_write_4byte(dm, 0xcb8, 0x00100000);/* cb8[20] SI/PIiqk_dpk module */
			cal_retry = 0;
			while (1) {
				/* one shot */
				odm_write_4byte(dm, 0x980, 0xfa000000);
				odm_write_4byte(dm, 0x980, 0xf8000000);

				delay_ms(10); /* delay 10ms */
				odm_write_4byte(dm, 0xcb8, 0x00000000);
				delay_count = 0;
				while (1) {
					IQK_ready = odm_get_bb_reg(dm, 0xd00, BIT(10));
					if ((~IQK_ready) || (delay_count > 20))
						break;
					else {
						delay_ms(1);
						delay_count++;
					}
				}

				if (delay_count < 20) {							/* If 20ms No Result, then cal_retry++ */
					/* ============TXIQK Check============== */
					TX_fail = odm_get_bb_reg(dm, 0xd00, BIT(12));

					if (~TX_fail) {
						odm_write_4byte(dm, 0xcb8, 0x02000000);
						TX_X0[cal] = odm_get_bb_reg(dm, 0xd00, 0x07ff0000) << 21;
						odm_write_4byte(dm, 0xcb8, 0x04000000);
						TX_Y0[cal] = odm_get_bb_reg(dm, 0xd00, 0x07ff0000) << 21;
						TX0IQKOK = true;
						break;
					} else {
						odm_set_bb_reg(dm, 0xccc, 0x000007ff, 0x0);
						odm_set_bb_reg(dm, 0xcd4, 0x000007ff, 0x200);
						TX0IQKOK = false;
						cal_retry++;
						if (cal_retry == 10)
							break;
					}
				} else {
					TX0IQKOK = false;
					cal_retry++;
					if (cal_retry == 10)
						break;
				}
			}


			if (TX0IQKOK == false)
				break;				/* TXK fail, Don't do RXK */

			/* ====== RX IQK ====== */
			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
			/* 1. RX RF setting */
			odm_set_rf_reg(dm, (enum rf_path)path, 0xef, RFREGOFFSETMASK, 0x80000);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x30, RFREGOFFSETMASK, 0x30000);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x31, RFREGOFFSETMASK, 0x0002f);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x32, RFREGOFFSETMASK, 0xfffbb);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x8f, RFREGOFFSETMASK, 0x88001);
			odm_set_rf_reg(dm, (enum rf_path)path, 0x65, RFREGOFFSETMASK, 0x931d8);
			odm_set_rf_reg(dm, (enum rf_path)path, 0xef, RFREGOFFSETMASK, 0x00000);

			odm_set_bb_reg(dm, 0x978, 0x03FF8000, (TX_X0[cal]) >> 21 & 0x000007ff);
			odm_set_bb_reg(dm, 0x978, 0x000007FF, (TX_Y0[cal]) >> 21 & 0x000007ff);
			odm_set_bb_reg(dm, 0x978, BIT(31), 0x1);
			odm_set_bb_reg(dm, 0x97c, BIT(31), 0x0);
			odm_write_4byte(dm, 0x90c, 0x00008000);
			odm_write_4byte(dm, 0x984, 0x0046a911);

			odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
			odm_write_4byte(dm, 0xc80, 0x38008c10);/* TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16 */
			odm_write_4byte(dm, 0xc84, 0x18008c10);/* RX_Tone_idx[9:0], RxK_Mask[29] */
			odm_write_4byte(dm, 0xc88, 0x02140119);

			if (dm->support_interface == 1) {
				rx_iqk_loop = 2;				/* for 2% fail; */
			} else
				rx_iqk_loop = 1;
			for (i = 0; i < rx_iqk_loop; i++) {
				if (dm->support_interface == 1)
					if (i == 0)
						odm_write_4byte(dm, 0xc8c, 0x28161100);  /* Good */
					else
						odm_write_4byte(dm, 0xc8c, 0x28160d00);
				else
					odm_write_4byte(dm, 0xc8c, 0x28160d00);

				odm_write_4byte(dm, 0xcb8, 0x00100000);/* cb8[20]  SI/PI iqk_dpk module */

				cal_retry = 0;
				while (1) {
					/* one shot */
					odm_write_4byte(dm, 0x980, 0xfa000000);
					odm_write_4byte(dm, 0x980, 0xf8000000);
					delay_ms(10); /* delay 10ms */
					odm_write_4byte(dm, 0xcb8, 0x00000000);
					delay_count = 0;
					while (1) {
						IQK_ready = odm_get_bb_reg(dm, 0xd00, BIT(10));
						if ((~IQK_ready) || (delay_count > 20))
							break;
						else {
							delay_ms(1);
							delay_count++;
						}
					}

					if (delay_count < 20) {	/* If 20ms No Result, then cal_retry++ */
						/* ============RXIQK Check============== */
						RX_fail = odm_get_bb_reg(dm, 0xd00, BIT(11));
						if (RX_fail == 0) {
							/*
							dbg_print("====== RXIQK (%d) ======", i);
							odm_write_4byte(dm, 0xcb8, 0x05000000);
							reg1 = odm_get_bb_reg(dm, 0xd00, 0xffffffff);
							odm_write_4byte(dm, 0xcb8, 0x06000000);
							reg2 = odm_get_bb_reg(dm, 0xd00, 0x0000001f);
							dbg_print("reg1 = %d, reg2 = %d", reg1, reg2);
							image_power = (reg2<<32)+reg1;
							dbg_print("Before PW = %d\n", image_power);
							odm_write_4byte(dm, 0xcb8, 0x07000000);
							reg1 = odm_get_bb_reg(dm, 0xd00, 0xffffffff);
							odm_write_4byte(dm, 0xcb8, 0x08000000);
							reg2 = odm_get_bb_reg(dm, 0xd00, 0x0000001f);
							image_power = (reg2<<32)+reg1;
							dbg_print("After PW = %d\n", image_power);
							*/

							odm_write_4byte(dm, 0xcb8, 0x06000000);
							RX_X0[i][cal] = odm_get_bb_reg(dm, 0xd00, 0x07ff0000) << 21;
							odm_write_4byte(dm, 0xcb8, 0x08000000);
							RX_Y0[i][cal] = odm_get_bb_reg(dm, 0xd00, 0x07ff0000) << 21;
							RX0IQKOK = true;
							break;
						} else {
							odm_set_bb_reg(dm, 0xc10, 0x000003ff, 0x200 >> 1);
							odm_set_bb_reg(dm, 0xc10, 0x03ff0000, 0x0 >> 1);
							RX0IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;

						}
					} else {
						RX0IQKOK = false;
						cal_retry++;
						if (cal_retry == 10)
							break;
					}
				}
			}

			if (TX0IQKOK)
				tx_average++;
			if (RX0IQKOK)
				rx_average++;
		}
		break;
		default:
			break;
		}
		cal++;
	}
	/* FillIQK Result */
	switch (path) {
	case RF_PATH_A:
	{
		PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "========Path_A =======\n");
		if (tx_average == 0)
			break;

		for (i = 0; i < tx_average; i++)
			PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "TX_X0[%d] = %x ;; TX_Y0[%d] = %x\n", i, (TX_X0[i]) >> 21 & 0x000007ff, i, (TX_Y0[i]) >> 21 & 0x000007ff);
		for (i = 0; i < tx_average; i++) {
			for (ii = i + 1; ii < tx_average; ii++) {
				dx = (TX_X0[i] >> 21) - (TX_X0[ii] >> 21);
				if (dx < 3 && dx > -3) {
					dy = (TX_Y0[i] >> 21) - (TX_Y0[ii] >> 21);
					if (dy < 3 && dy > -3) {
						TX_X = ((TX_X0[i] >> 21) + (TX_X0[ii] >> 21)) / 2;
						TX_Y = ((TX_Y0[i] >> 21) + (TX_Y0[ii] >> 21)) / 2;
						TX_finish = 1;
						break;
					}
				}
			}
			if (TX_finish == 1)
				break;
		}

		if (TX_finish == 1)
			_iqk_tx_fill_iqc_8821a(dm, path, TX_X, TX_Y);
		else
			_iqk_tx_fill_iqc_8821a(dm, path, 0x200, 0x0);

		if (rx_average == 0)
			break;

		for (i = 0; i < rx_average; i++) {
			PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "RX_X0[0][%d] = %x ;; RX_Y0[0][%d] = %x\n", i, (RX_X0[0][i]) >> 21 & 0x000007ff, i, (RX_Y0[0][i]) >> 21 & 0x000007ff);
			if (rx_iqk_loop == 2)
				PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "RX_X0[1][%d] = %x ;; RX_Y0[1][%d] = %x\n", i, (RX_X0[1][i]) >> 21 & 0x000007ff, i, (RX_Y0[1][i]) >> 21 & 0x000007ff);
		}
		for (i = 0; i < rx_average; i++) {
			for (ii = i + 1; ii < rx_average; ii++) {
				dx = (RX_X0[0][i] >> 21) - (RX_X0[0][ii] >> 21);
				if (dx < 4 && dx > -4) {
					dy = (RX_Y0[0][i] >> 21) - (RX_Y0[0][ii] >> 21);
					if (dy < 4 && dy > -4) {
						RX_X_temp = ((RX_X0[0][i] >> 21) + (RX_X0[0][ii] >> 21)) / 2;
						RX_Y_temp = ((RX_Y0[0][i] >> 21) + (RX_Y0[0][ii] >> 21)) / 2;
						RX_finish1 = 1;
						break;
					}
				}
			}
			if (RX_finish1 == 1) {
				RX_X = RX_X_temp;
				RX_Y = RX_Y_temp;
				break;
			}
		}
		if (rx_iqk_loop == 2) {
			for (i = 0; i < rx_average; i++) {
				for (ii = i + 1; ii < rx_average; ii++) {
					dx = (RX_X0[1][i] >> 21) - (RX_X0[1][ii] >> 21);
					if (dx < 4 && dx > -4) {
						dy = (RX_Y0[1][i] >> 21) - (RX_Y0[1][ii] >> 21);
						if (dy < 4 && dy > -4) {
							RX_X = ((RX_X0[1][i] >> 21) + (RX_X0[1][ii] >> 21)) / 2;
							RX_Y = ((RX_Y0[1][i] >> 21) + (RX_Y0[1][ii] >> 21)) / 2;
							RX_finish2 = 1;
							break;
						}
					}
				}
				if (RX_finish2 == 1)
					break;
			}
			if (RX_finish1 && RX_finish2) {
				RX_X = (RX_X + RX_X_temp) / 2;
				RX_Y = (RX_Y + RX_Y_temp) / 2;
			}
		}
		if (RX_finish1 || RX_finish1)
			_iqk_rx_fill_iqc_8821a(dm, path, RX_X, RX_Y);
		else
			_iqk_rx_fill_iqc_8821a(dm, path, 0x200, 0x0);
	}
	break;
	default:
		break;
	}
}

#define MACBB_REG_NUM 8
#define AFE_REG_NUM 4
#define RF_REG_NUM 3

void
_phy_iq_calibrate_by_fw_8821a(
	struct dm_struct		*dm
)
{
	PADAPTER      adapter = (PADAPTER)dm->adapter;
	HAL_DATA_TYPE	*hal_data = GET_HAL_DATA((adapter));
	u8			iqk_cmd[3] = {hal_data->CurrentChannel, 0x0, 0x0};
	u8			buf1 = 0x0;
	u8			buf2 = 0x0;

	/* Byte 2, Bit 4 ~ Bit 5 : band_type */
	if (hal_data->CurrentBandType)
		buf1 = 0x2 << 4;
	else
		buf1 = 0x1 << 4;

	/* Byte 2, Bit 0 ~ Bit 3 : bandwidth */
	if (hal_data->CurrentChannelBW == CHANNEL_WIDTH_20)
		buf2 = 0x1;
	else if (hal_data->CurrentChannelBW == CHANNEL_WIDTH_40)
		buf2 = 0x1 << 1;
	else if (hal_data->CurrentChannelBW == CHANNEL_WIDTH_80)
		buf2 = 0x1 << 2;
	else
		buf2 = 0x1 << 3;

	iqk_cmd[1] = buf1 | buf2;
	iqk_cmd[2] = hal_data->ExternalPA_5G | hal_data->ExternalLNA_5G << 1;


	RT_TRACE(COMP_MP, DBG_LOUD, ("== FW IQK Start ==\n"));
	hal_data->IQK_StartTimer = 0;
	hal_data->IQK_StartTimer = PlatformGetCurrentTime();
	RT_TRACE(COMP_MP, DBG_LOUD, ("== start_time: %u\n", hal_data->IQK_StartTimer));

#if (H2C_USE_IO_THREAD == 1)
	FW8821A_FillH2cCommand(adapter, 0x45, 3, iqk_cmd);
#else
	FillH2CCommand8821A(adapter, 0x45, 3, iqk_cmd);
#endif
}

void
_phy_iq_calibrate_8821a(
	struct dm_struct		*dm
)
{
	u32	MACBB_backup[MACBB_REG_NUM], AFE_backup[AFE_REG_NUM], RFA_backup[RF_REG_NUM], RFB_backup[RF_REG_NUM];
	u32	backup_macbb_reg[MACBB_REG_NUM] = {0x520, 0x550, 0x808, 0xa04, 0x90c, 0xc00, 0x838, 0x82c};
	u32	backup_afe_reg[AFE_REG_NUM] = {0xc5c, 0xc60, 0xc64, 0xc68};
	u32	backup_rf_reg[RF_REG_NUM] = {0x65, 0x8f, 0x0};

	_iqk_backup_mac_bb_8821a(dm, MACBB_backup, backup_macbb_reg, MACBB_REG_NUM);
	_iqk_backup_afe_8821a(dm, AFE_backup, backup_afe_reg, AFE_REG_NUM);
	_iqk_backup_rf_8821a(dm, RFA_backup, RFB_backup, backup_rf_reg, RF_REG_NUM);

	_iqk_configure_mac_8821a(dm);
	_iqk_tx_8821a(dm, RF_PATH_A);
	_iqk_restore_rf_8821a(dm, RF_PATH_A, backup_rf_reg, RFA_backup, RF_REG_NUM);

	_iqk_restore_afe_8821a(dm, AFE_backup, backup_afe_reg, AFE_REG_NUM);
	_iqk_restore_mac_bb_8821a(dm, MACBB_backup, backup_macbb_reg, MACBB_REG_NUM);

	/* _IQK_Exit_8821A(dm); */
	/* _IQK_TX_CheckResult_8821A */

}

void
phy_reset_iqk_result_8821a(
	struct dm_struct	*dm
)
{
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
	odm_set_bb_reg(dm, 0xccc, 0x000007ff, 0x0);
	odm_set_bb_reg(dm, 0xcd4, 0x000007ff, 0x200);
	odm_write_4byte(dm, 0xce8, 0x0);
	odm_set_bb_reg(dm, 0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_set_bb_reg(dm, 0xc10, 0x000003ff, 0x100);
}

/*for win*/
/*IQK:0x1*/
void
phy_iq_calibrate_8821a(
	void		*dm_void,
	boolean	is_recovery
)
{

	struct dm_struct	*dm = (struct dm_struct *)dm_void;
	u32			counter = 0;

	if (dm->fw_offload_ability & PHYDM_RF_IQK_OFFLOAD) {
		_phy_iq_calibrate_by_fw_8821a(dm);
		for (counter = 0; counter < 10; counter++) {
			PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "== FW IQK PROGRESS == #%d\n", counter);
			ODM_delay_ms(50);
			if (!dm->rf_calibrate_info.is_iqk_in_progress) {
				PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "== FW IQK RETURN FROM WAITING ==\n");
				break;
			}
		}
		if (dm->rf_calibrate_info.is_iqk_in_progress)
			PHYDM_DBG(dm, ODM_COMP_CALIBRATION, "== FW IQK TIMEOUT (Still in progress after 500ms) ==\n");
	} else
		_phy_iq_calibrate_8821a(dm);
}


void
phy_lc_calibrate_8821a(
	void		*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;

	phy_lc_calibrate_8812a(dm);
}
