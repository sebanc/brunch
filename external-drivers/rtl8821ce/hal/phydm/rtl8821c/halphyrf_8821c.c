/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8821C_SUPPORT == 1)

boolean
get_mix_mode_tx_agc_bbs_wing_offset_8821c(
	void				*p_dm_void,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				tx_power_index_offest_upper_bound,
	s8				tx_power_index_offest_lower_bound
)
{
	struct PHY_DM_STRUCT		*p_dm_odm	=	(struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_rf_calibration_structure	*p_rf_calibrate_info = &(p_dm_odm->rf_calibrate_info);

	u8	bb_swing_upper_bound = p_rf_calibrate_info->default_ofdm_index + 10;
	u8	bb_swing_lower_bound = 0;

	s8	tx_agc_index = 0;
	u8	tx_bb_swing_index = p_rf_calibrate_info->default_ofdm_index;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
		("Path_%d pRF->absolute_ofdm_swing_idx[rf_path]=%d, tx_power_index_offest_upper_bound=%d, tx_power_index_offest_lower_bound=%d\n",
		rf_path, p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path], tx_power_index_offest_upper_bound, tx_power_index_offest_lower_bound));

	if (tx_power_index_offest_upper_bound > 0XF)
		tx_power_index_offest_upper_bound = 0XF;

	if (tx_power_index_offest_lower_bound < -15)
		tx_power_index_offest_lower_bound = -15;


	if (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] >= 0 && p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] <= tx_power_index_offest_upper_bound) {
		tx_agc_index = p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path];
		tx_bb_swing_index = p_rf_calibrate_info->default_ofdm_index;
	} else if (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] >= 0 && (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] > tx_power_index_offest_upper_bound)) {
		tx_agc_index = tx_power_index_offest_upper_bound;
		p_rf_calibrate_info->remnant_ofdm_swing_idx[rf_path] = p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] - tx_power_index_offest_upper_bound;
		tx_bb_swing_index = p_rf_calibrate_info->default_ofdm_index + p_rf_calibrate_info->remnant_ofdm_swing_idx[rf_path];

		if (tx_bb_swing_index > bb_swing_upper_bound)
			tx_bb_swing_index = bb_swing_upper_bound;
	} else if (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] < 0 && (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] >= tx_power_index_offest_lower_bound)) {
		tx_agc_index = p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path];
		tx_bb_swing_index = p_rf_calibrate_info->default_ofdm_index;
	} else if (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] < 0 && (p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] < tx_power_index_offest_lower_bound)) {
		tx_agc_index = tx_power_index_offest_lower_bound;
		p_rf_calibrate_info->remnant_ofdm_swing_idx[rf_path] = p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] - tx_power_index_offest_lower_bound;

		if (p_rf_calibrate_info->default_ofdm_index > (p_rf_calibrate_info->remnant_ofdm_swing_idx[rf_path] * (-1)))
			tx_bb_swing_index = p_rf_calibrate_info->default_ofdm_index + p_rf_calibrate_info->remnant_ofdm_swing_idx[rf_path];
		else
			tx_bb_swing_index = bb_swing_lower_bound;
	}

	p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path] = tx_agc_index;
	p_rf_calibrate_info->bb_swing_idx_ofdm[rf_path] = tx_bb_swing_index;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
		("MixMode Offset Path_%d   pRF->absolute_ofdm_swing_idx[rf_path]=%d   pRF->bb_swing_idx_ofdm[rf_path]=%d   TxPwrIdxOffestUpper=%d   TxPwrIdxOffestLower=%d\n",
		rf_path, p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path], p_rf_calibrate_info->bb_swing_idx_ofdm[rf_path], tx_power_index_offest_upper_bound, tx_power_index_offest_lower_bound));

	return true;
}


void
odm_tx_pwr_track_set_pwr8821c(
	void				*p_dm_void,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _ADAPTER	*adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);
	struct odm_rf_calibration_structure	*p_rf_calibrate_info = &(p_dm_odm->rf_calibrate_info);
	u8			channel  = *p_dm_odm->p_channel;
	u8			band_width  = *p_dm_odm->p_band_width;
	u8			tx_power_index_offest_upper_bound = 0;
	s8			tx_power_index_offest_lower_bound = 0;
	u8			tx_power_index = 0;
	u8			tx_rate = 0xFF;


	if (p_dm_odm->mp_mode == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
	} else {
		u16	rate	 = *(p_dm_odm->p_forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = adapter->HalFunc.GetHwRateFromMRateHandler(p_dm_odm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			if (p_dm_odm->number_linked_client != 0)
				tx_rate = hw_rate_to_m_rate(p_dm_odm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8) rate;
	}

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD, ("Call:%s tx_rate=0x%X\n", __func__, tx_rate));

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
		("pRF->default_ofdm_index=%d   pRF->default_cck_index=%d\n", p_rf_calibrate_info->default_ofdm_index, p_rf_calibrate_info->default_cck_index));

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
		("pRF->absolute_ofdm_swing_idx=%d   pRF->remnant_ofdm_swing_idx=%d   pRF->absolute_cck_swing_idx=%d   pRF->remnant_cck_swing_idx=%d   rf_path=%d\n",
		p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path], p_rf_calibrate_info->remnant_ofdm_swing_idx[rf_path], p_rf_calibrate_info->absolute_cck_swing_idx[rf_path], p_rf_calibrate_info->remnant_cck_swing_idx, rf_path));

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	tx_power_index = odm_get_tx_power_index(p_dm_odm, (enum odm_rf_radio_path_e) rf_path, tx_rate, band_width, channel);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
	if (p_dm_odm->mp_mode == true)
		tx_power_index = odm_get_tx_power_index(p_dm_odm, (enum odm_rf_radio_path_e) rf_path, tx_rate, band_width, channel);
	else {
		if (p_dm_odm->number_linked_client != 0)
			tx_power_index = odm_get_tx_power_index(p_dm_odm, (enum odm_rf_radio_path_e) rf_path, tx_rate, band_width, channel);
	}
#endif

	if (tx_power_index >= 63)
		tx_power_index = 63;

	tx_power_index_offest_upper_bound = 63 - tx_power_index;

	tx_power_index_offest_lower_bound = 0 - tx_power_index;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
		("tx_power_index=%d tx_power_index_offest_upper_bound=%d tx_power_index_offest_lower_bound=%d rf_path=%d\n", tx_power_index, tx_power_index_offest_upper_bound, tx_power_index_offest_lower_bound, rf_path));

	if (method == BBSWING) {	/*use for mp driver clean power tracking status*/
		switch (rf_path) {
		case ODM_RF_PATH_A:
			odm_set_bb_reg(p_dm_odm, 0xC94, (BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1)), ((p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path]) & 0x3f));
			odm_set_bb_reg(p_dm_odm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[p_rf_calibrate_info->bb_swing_idx_ofdm[rf_path]]);
			break;

		default:
			break;
		}

	} else if (method == MIX_MODE) {
		switch (rf_path) {
		case ODM_RF_PATH_A:
			get_mix_mode_tx_agc_bbs_wing_offset_8821c(p_dm_odm, method, rf_path, tx_power_index_offest_upper_bound, tx_power_index_offest_lower_bound);
			odm_set_bb_reg(p_dm_odm, 0xC94, (BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1)), ((p_rf_calibrate_info->absolute_ofdm_swing_idx[rf_path]) & 0x3f));
			odm_set_bb_reg(p_dm_odm, REG_A_TX_SCALE_JAGUAR, 0xFFE00000, tx_scaling_table_jaguar[p_rf_calibrate_info->bb_swing_idx_ofdm[rf_path]]);

			ODM_RT_TRACE(p_dm_odm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD,
				("TXAGC(0xC94)=0x%x BBSwing(0xc1c)=0x%x BBSwingIndex=%d rf_path=%d\n",
				odm_get_bb_reg(p_dm_odm, 0xC94, (BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1))),
				odm_get_bb_reg(p_dm_odm, 0xc1c, 0xFFE00000),
				p_rf_calibrate_info->bb_swing_idx_ofdm[rf_path], rf_path));
			break;

		default:
			break;
		}
	}


}


void
get_delta_swing_table_8821c(
	void		*p_dm_void,
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b,
	u8 **temperature_up_cck_a,
	u8 **temperature_down_cck_a,
	u8 **temperature_up_cck_b,
	u8 **temperature_down_cck_b
#else
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
#endif
)
{
	struct PHY_DM_STRUCT		*p_dm_odm		= (struct PHY_DM_STRUCT *)p_dm_void;
	struct odm_rf_calibration_structure	*p_rf_calibrate_info	= &(p_dm_odm->rf_calibrate_info);

#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	u8			channel			= *(p_dm_odm->p_channel);
#else
	struct _ADAPTER		*adapter			= p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data		= GET_HAL_DATA(adapter);
	u8			channel			= *p_dm_odm->p_channel;
#endif

#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	*temperature_up_cck_a   = p_rf_calibrate_info->delta_swing_table_idx_2g_cck_a_p;
	*temperature_down_cck_a = p_rf_calibrate_info->delta_swing_table_idx_2g_cck_a_n;
	*temperature_up_cck_b   = p_rf_calibrate_info->delta_swing_table_idx_2g_cck_b_p;
	*temperature_down_cck_b = p_rf_calibrate_info->delta_swing_table_idx_2g_cck_b_n;
#endif

	*temperature_up_a   = p_rf_calibrate_info->delta_swing_table_idx_2ga_p;
	*temperature_down_a = p_rf_calibrate_info->delta_swing_table_idx_2ga_n;
	*temperature_up_b   = p_rf_calibrate_info->delta_swing_table_idx_2gb_p;
	*temperature_down_b = p_rf_calibrate_info->delta_swing_table_idx_2gb_n;

	if (36 <= channel && channel <= 64) {
		*temperature_up_a   = p_rf_calibrate_info->delta_swing_table_idx_5ga_p[0];
		*temperature_down_a = p_rf_calibrate_info->delta_swing_table_idx_5ga_n[0];
		*temperature_up_b   = p_rf_calibrate_info->delta_swing_table_idx_5gb_p[0];
		*temperature_down_b = p_rf_calibrate_info->delta_swing_table_idx_5gb_n[0];
	} else if (100 <= channel && channel <= 144)	{
		*temperature_up_a   = p_rf_calibrate_info->delta_swing_table_idx_5ga_p[1];
		*temperature_down_a = p_rf_calibrate_info->delta_swing_table_idx_5ga_n[1];
		*temperature_up_b   = p_rf_calibrate_info->delta_swing_table_idx_5gb_p[1];
		*temperature_down_b = p_rf_calibrate_info->delta_swing_table_idx_5gb_n[1];
	} else if (149 <= channel && channel <= 177)	{
		*temperature_up_a   = p_rf_calibrate_info->delta_swing_table_idx_5ga_p[2];
		*temperature_down_a = p_rf_calibrate_info->delta_swing_table_idx_5ga_n[2];
		*temperature_up_b   = p_rf_calibrate_info->delta_swing_table_idx_5gb_p[2];
		*temperature_down_b = p_rf_calibrate_info->delta_swing_table_idx_5gb_n[2];
	}
}


void
_phy_aac_calibrate_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm
	)
{
	u32 cnt = 0;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[AACK]AACK start!!!!!!!\n"));
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xb8, bRFRegOffsetMask, 0x80a00);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xb0, bRFRegOffsetMask, 0xff0fa);
	ODM_delay_ms(10);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xca, bRFRegOffsetMask, 0x80000);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xc9, bRFRegOffsetMask, 0x1c141);
	for (cnt = 0; cnt < 100; cnt++) {
		ODM_delay_ms(1);
		if (odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xca, 0x1000) != 0x1)
			break;
	}

	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xb0, bRFRegOffsetMask, 0xff0f8);

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[AACK]AACK end!!!!!!!\n"));
}


void
_phy_lc_calibrate_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	u32 lc_cal = 0, cnt = 0, tmp0xc00;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[LCK]LCK start!!!!!!!\n"));

	/*RF to standby mode*/
	tmp0xc00 = odm_read_4byte(p_dm_odm, 0xc00);
	odm_write_4byte(p_dm_odm, 0xc00, 0x4);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x0, bRFRegOffsetMask, 0x10000);

	_phy_aac_calibrate_8821c(p_dm_odm);

	/*backup RF0x18*/
	lc_cal = odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);
	/*Start LCK*/
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal | 0x08000);
	ODM_delay_ms(50);

	for (cnt = 0; cnt < 100; cnt++) {
		if (odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, RF_CHNLBW, 0x8000) != 0x1)
			break;
		ODM_delay_ms(10);
	}

	/*Recover channel number*/
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal);
	/**restore*/
	odm_write_4byte(p_dm_odm, 0xc00, tmp0xc00);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x0, bRFRegOffsetMask, 0x3ffff);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[LCK]LCK end!!!!!!!\n"));
}


void
phy_lc_calibrate_8821c(
	void	*p_dm_void
)
{
#if 1
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	boolean	is_start_cont_tx = false, is_single_tone = false, is_carrier_suppression = false;
	u64		start_time;
	u64		progressing_time;


#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct _ADAPTER		*p_adapter = p_dm_odm->adapter;

#if (MP_DRIVER == 1)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PMPT_CONTEXT	p_mpt_ctx = &(p_adapter->MptCtx);
#else
	PMPT_CONTEXT	p_mpt_ctx = &(p_adapter->mppriv.mpt_ctx);
#endif

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	is_start_cont_tx = p_mpt_ctx->bStartContTx;
	is_single_tone = p_mpt_ctx->bSingleTone;
	is_carrier_suppression = p_mpt_ctx->bCarrierSuppression;
#else
	is_start_cont_tx = p_mpt_ctx->is_start_cont_tx;
	is_single_tone = p_mpt_ctx->is_single_tone;
	is_carrier_suppression = p_mpt_ctx->is_carrier_suppression;
#endif
#endif
#endif

	if (is_start_cont_tx || is_single_tone || is_carrier_suppression) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[LCK]continues TX ing !!! LCK return\n"));
		return;
	}

	if (*(p_dm_odm->p_is_scan_in_process)) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[LCK]scan is in process, bypass LCK\n"));
		return;
	}

	start_time = odm_get_current_time(p_dm_odm);
	p_dm_odm->rf_calibrate_info.is_lck_in_progress = true;
	_phy_lc_calibrate_8821c(p_dm_odm);
	p_dm_odm->rf_calibrate_info.is_lck_in_progress = false;
	progressing_time = odm_get_progressing_time(p_dm_odm, start_time);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[LCK]LCK progressing_time = %lld\n", progressing_time));
#endif
}



void configure_txpower_track_8821c(
	struct _TXPWRTRACK_CFG	*p_config
)
{
	p_config->swing_table_size_cck = TXSCALE_TABLE_SIZE;
	p_config->swing_table_size_ofdm = TXSCALE_TABLE_SIZE;
	p_config->threshold_iqk = IQK_THRESHOLD;
	p_config->threshold_dpk = DPK_THRESHOLD;
	p_config->average_thermal_num = AVG_THERMAL_NUM_8821C;
	p_config->rf_path_count = MAX_PATH_NUM_8821C;
	p_config->thermal_reg_addr = RF_T_METER_8821C;

	p_config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8821c;
	p_config->do_iqk = do_iqk_8821c;
	p_config->phy_lc_calibrate = phy_lc_calibrate_8821c;

#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	p_config->get_delta_all_swing_table = get_delta_swing_table_8821c;
#else
	p_config->get_delta_swing_table = get_delta_swing_table_8821c;
#endif
}


void phy_set_rf_path_switch_8821c(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct PHY_DM_STRUCT		*p_dm_odm,
#else
	struct _ADAPTER	*p_adapter,
#endif
	boolean		is_main
)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(p_adapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	struct PHY_DM_STRUCT		*p_dm_odm = &p_hal_data->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct PHY_DM_STRUCT		*p_dm_odm = &p_hal_data->DM_OutSrc;
#endif
#endif
	u8		ant_num = 0; /*0: ANT_1, 1: ANT_2*/

	if (is_main)
		ant_num = SWITCH_TO_ANT1; /*Main = ANT_1*/
	else
		ant_num = SWITCH_TO_ANT2; /*Aux = ANT_2*/

	config_phydm_set_ant_path(p_dm_odm, p_dm_odm->current_rf_set_8821c, ant_num);

}

boolean
_phy_query_rf_path_switch_8821c(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct PHY_DM_STRUCT	*p_dm_odm
#else
	struct _ADAPTER	*p_adapter
#endif
)
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(p_adapter);
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	struct PHY_DM_STRUCT		*p_dm_odm = &p_hal_data->odmpriv;
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	struct PHY_DM_STRUCT		*p_dm_odm = &p_hal_data->DM_OutSrc;
#endif
#endif
	u8		ant_num = 0; /*0: ANT_1, 1: ANT_2*/

	ODM_delay_ms(300);

	ant_num = query_phydm_current_ant_num_8821c(p_dm_odm);

	if (ant_num == SWITCH_TO_ANT1)
		return true; /*Main = ANT_1*/
	else
		return false; /*Aux = ANT_2*/

}


boolean phy_query_rf_path_switch_8821c(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct PHY_DM_STRUCT		*p_dm_odm
#else
	struct _ADAPTER	*p_adapter
#endif
)
{

#if DISABLE_BB_RF
	return true;
#endif

#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	return _phy_query_rf_path_switch_8821c(p_dm_odm);
#else
	return _phy_query_rf_path_switch_8821c(p_adapter);
#endif
}


#endif	/* (RTL8821C_SUPPORT == 0)*/
