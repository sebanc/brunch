/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

/*************************************************************
 * include files
 * *************************************************************/
#include "mp_precomp.h"
#include "phydm_precomp.h"

#ifdef PHYDM_LNA_SAT_CHK_SUPPORT

#ifdef PHYDM_LNA_SAT_CHK_TYPE1
void phydm_lna_sat_chk_init(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t *lna_info = &dm->dm_lna_sat_info;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s ==>\n", __func__);

	lna_info->check_time = 0;
	lna_info->sat_cnt_acc_patha = 0;
	lna_info->sat_cnt_acc_pathb = 0;
	lna_info->cur_sat_status = 0;
	lna_info->pre_sat_status = 0;
	lna_info->cur_timer_check_cnt = 0;
	lna_info->pre_timer_check_cnt = 0;
}

void phydm_set_ofdm_agc_tab(
	void *dm_void,
	u8 tab_sel)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	/* table sel:0/2, 1 is used for CCK */
	if (tab_sel == OFDM_AGC_TAB_0)
		odm_set_bb_reg(dm, R_0xc70, 0x1e00, OFDM_AGC_TAB_0);
	else if (tab_sel == OFDM_AGC_TAB_2)
		odm_set_bb_reg(dm, R_0xc70, 0x1e00, OFDM_AGC_TAB_2);
	else
		odm_set_bb_reg(dm, R_0xc70, 0x1e00, OFDM_AGC_TAB_0);
}

u8 phydm_get_ofdm_agc_tab(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	return (u8)odm_get_bb_reg(dm, R_0xc70, 0x1e00);
}

void phydm_lna_sat_chk(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_lna_sat_t *lna_info = &dm->dm_lna_sat_info;
	u8 igi_rssi_min;
	u8 rssi_min = dm->rssi_min;
	u32 sat_status_a, sat_status_b;
	u8 igi_restore = dig_t->cur_ig_value;
	u8 i, chk_cnt = lna_info->chk_cnt;
	u32 lna_sat_cnt_thd = 0;
	u8 agc_tab;
	u32 max_check_time = 0;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "\n%s ==>\n", __func__);

	if (!(dm->support_ability & ODM_BB_LNA_SAT_CHK)) {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "Func disable\n");
		return;
	}

	if (lna_info->is_disable_lna_sat_chk) {
		phydm_lna_sat_chk_init(dm);
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "disable_lna_sat_chk\n");
		return;
	}

	/* move igi to target pin of rssi_min */
	if (rssi_min == 0 || rssi_min == 0xff) {
		/* adapt agc table 0*/
		phydm_set_ofdm_agc_tab(dm, OFDM_AGC_TAB_0);
		phydm_lna_sat_chk_init(dm);
		return;
	} else if (rssi_min % 2 != 0) {
		igi_rssi_min = rssi_min + DIFF_RSSI_TO_IGI - 1;
	} else {
		igi_rssi_min = rssi_min + DIFF_RSSI_TO_IGI;
	}

	if ((lna_info->chk_period > 0) && (lna_info->chk_period <= ONE_SEC_MS))
		max_check_time = chk_cnt * (ONE_SEC_MS / (lna_info->chk_period)) * 5;
	else
		max_check_time = chk_cnt * 5;

	lna_sat_cnt_thd = (max_check_time * lna_info->chk_duty_cycle) / 100;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
		  "check_time=%d, rssi_min=%d, igi_rssi_min=0x%x\nchk_cnt=%d, chk_period=%d, max_check_time=%d, lna_sat_cnt_thd=%d\n",
		  lna_info->check_time,
		  rssi_min,
		  igi_rssi_min,
		  chk_cnt,
		  lna_info->chk_period,
		  max_check_time,
		  lna_sat_cnt_thd);

	odm_write_dig(dm, igi_rssi_min);

	/* adapt agc table 0 check saturation status */
	phydm_set_ofdm_agc_tab(dm, OFDM_AGC_TAB_0);
	/* open rf power detection ckt & set detection range */
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x86, 0x1f, 0x10);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x86, 0x1f, 0x10);

	/* check saturation status*/
	for (i = 0; i < chk_cnt; i++) {
		sat_status_a = odm_get_rf_reg(dm, RF_PATH_A, RF_0xae, 0xc0000);
		sat_status_b = odm_get_rf_reg(dm, RF_PATH_B, RF_0xae, 0xc0000);
		if (sat_status_a != 0)
			lna_info->sat_cnt_acc_patha++;
		if (sat_status_b != 0)
			lna_info->sat_cnt_acc_pathb++;

		if (lna_info->sat_cnt_acc_patha >= lna_sat_cnt_thd ||
		    lna_info->sat_cnt_acc_pathb >= lna_sat_cnt_thd) {
			lna_info->cur_sat_status = 1;
			PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
				  "cur_sat_status=%d, check_time=%d\n",
				  lna_info->cur_sat_status,
				  lna_info->check_time);
			break;
		}
		lna_info->cur_sat_status = 0;
	}

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
		  "cur_sat_status=%d, pre_sat_status=%d, sat_cnt_acc_patha=%d, sat_cnt_acc_pathb=%d\n",
		  lna_info->cur_sat_status,
		  lna_info->pre_sat_status,
		  lna_info->sat_cnt_acc_patha,
		  lna_info->sat_cnt_acc_pathb);

	// agc table decision
	if (lna_info->cur_sat_status) {
		if (!lna_info->dis_agc_table_swh)
			phydm_set_ofdm_agc_tab(dm, OFDM_AGC_TAB_2);
		lna_info->check_time = 0;
		lna_info->sat_cnt_acc_patha = 0;
		lna_info->sat_cnt_acc_pathb = 0;
		lna_info->pre_sat_status = lna_info->cur_sat_status;

	} else if (lna_info->check_time <= (max_check_time - 1)) {
		if (lna_info->pre_sat_status && !lna_info->dis_agc_table_swh)
			phydm_set_ofdm_agc_tab(dm, OFDM_AGC_TAB_2);
		lna_info->check_time++;

	} else if (lna_info->check_time == max_check_time) {
		if (!lna_info->dis_agc_table_swh && lna_info->pre_sat_status == 1)
			phydm_set_ofdm_agc_tab(dm, OFDM_AGC_TAB_0);
		lna_info->check_time = 0;
		lna_info->sat_cnt_acc_patha = 0;
		lna_info->sat_cnt_acc_pathb = 0;
		lna_info->pre_sat_status = lna_info->cur_sat_status;
	}

	agc_tab = phydm_get_ofdm_agc_tab(dm);

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "use AGC tab %d\n", agc_tab);

	/* restore previous igi*/
	odm_write_dig(dm, igi_restore);
	lna_info->cur_timer_check_cnt++;
	odm_set_timer(dm, &lna_info->phydm_lna_sat_chk_timer, lna_info->chk_period);
}

void phydm_lna_sat_chk_callback(
	void *dm_void

	)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "\n%s ==>\n", __func__);
	phydm_lna_sat_chk(dm);
}

void phydm_lna_sat_chk_timers(
	void *dm_void,
	u8 state)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t *lna_info = &dm->dm_lna_sat_info;

	if (state == INIT_LNA_SAT_CHK_TIMMER) {
		odm_initialize_timer(dm,
				     &lna_info->phydm_lna_sat_chk_timer,
				     (void *)phydm_lna_sat_chk_callback, NULL,
				     "phydm_lna_sat_chk_timer");
	} else if (state == CANCEL_LNA_SAT_CHK_TIMMER) {
		odm_cancel_timer(dm, &lna_info->phydm_lna_sat_chk_timer);
	} else if (state == RELEASE_LNA_SAT_CHK_TIMMER) {
		odm_release_timer(dm, &lna_info->phydm_lna_sat_chk_timer);
	}
}

void phydm_lna_sat_chk_watchdog_type1(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t *lna_info = &dm->dm_lna_sat_info;

	u8 rssi_min = dm->rssi_min;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "\n%s ==>\n", __func__);

	if (!(dm->support_ability & ODM_BB_LNA_SAT_CHK)) {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
			  "func disable\n");
		return;
	}

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
		  "pre_timer_check_cnt=%d, cur_timer_check_cnt=%d\n",
		  lna_info->pre_timer_check_cnt,
		  lna_info->cur_timer_check_cnt);

	if (lna_info->is_disable_lna_sat_chk) {
		phydm_lna_sat_chk_init(dm);
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
			  "is_disable_lna_sat_chk=%d, return\n", lna_info->is_disable_lna_sat_chk);
		return;
	}

	if (!(dm->support_ic_type & ODM_RTL8197F)) {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
			  "support_ic_type != ODM_RTL8197F, return\n");
		return;
	}

	if (rssi_min == 0 || rssi_min == 0xff) {
		/* adapt agc table 0 */
		phydm_set_ofdm_agc_tab(dm, OFDM_AGC_TAB_0);
		phydm_lna_sat_chk_init(dm);
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
			  "rssi_min=%d, return\n", rssi_min);
		return;
	}

	if (lna_info->cur_timer_check_cnt == lna_info->pre_timer_check_cnt) {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "fail, restart timer\n");
		phydm_lna_sat_chk(dm);
	} else {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "Timer check pass\n");
	}
	lna_info->pre_timer_check_cnt = lna_info->cur_timer_check_cnt;
}

#endif

#ifdef PHYDM_LNA_SAT_CHK_TYPE2

void phydm_bubble_sort(
	void *dm_void,
	u8 *array,
	u16 array_length)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u16 i, j;
	u8 temp;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s ==>\n", __func__);
	for (i = 0; i < (array_length - 1); i++) {
		for (j = (i + 1); j < (array_length); j++) {
			if (array[i] > array[j]) {
				temp = array[i];
				array[i] = array[j];
				array[j] = temp;
			}
		}
	}
}

void phydm_lna_sat_chk_type2_init(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;
	u8 real_shift = pinfo->total_bit_shift;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s ==>\n", __func__);

	pinfo->total_cnt_snr = 1 << real_shift;
	pinfo->is_sm_done = TRUE;
	pinfo->is_snr_done = FALSE;
	pinfo->cur_snr_mean = 0;
	pinfo->cur_snr_var = 0;
	pinfo->cur_lower_snr_mean = 0;
	pinfo->pre_snr_mean = 0;
	pinfo->pre_snr_var = 0;
	pinfo->pre_lower_snr_mean = 0;
	pinfo->nxt_state = ORI_TABLE_MONITOR;
	pinfo->pre_state = ORI_TABLE_MONITOR;
}

void phydm_snr_collect(
	void *dm_void,
	u8 rx_snr)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	if (pinfo->is_sm_done) {
		/*PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s ==>\n", __func__);*/

		/* adapt only path-A for calculation */
		pinfo->snr_statistic[pinfo->cnt_snr_statistic] = rx_snr;

		if (pinfo->cnt_snr_statistic == (pinfo->total_cnt_snr - 1)) {
			pinfo->is_snr_done = TRUE;
			pinfo->cnt_snr_statistic = 0;
		} else {
			pinfo->cnt_snr_statistic++;
		}
	} else {
		return;
	}
}

void phydm_parsing_snr(void *dm_void, void *pktinfo_void, s8 *rx_snr)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*lna_t = &dm->dm_lna_sat_info;
	struct phydm_perpkt_info_struct *pktinfo = NULL;
	u8 target_macid = dm->rssi_min_macid;

	if (!(dm->support_ability & ODM_BB_LNA_SAT_CHK))
		return;

	pktinfo = (struct phydm_perpkt_info_struct *)pktinfo_void;

	if (!pktinfo->is_packet_match_bssid)
		return;

	if (lna_t->force_traget_macid != 0)
		target_macid = lna_t->force_traget_macid;

	if (target_macid != pktinfo->station_id)
		return;

	phydm_snr_collect(dm, rx_snr[0]); /*path-A B C D???*/
}

void phydm_snr_data_processing(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;
	u8 real_shift = pinfo->total_bit_shift;
	u16 total_snr_cnt = pinfo->total_cnt_snr;
	u16 total_loop_cnt = (total_snr_cnt - 1), i;
	u32 temp;
	u32 sum_snr_statistic = 0;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s ==>\n", __func__);
	PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
		  "total_loop_cnt=%d\n", total_loop_cnt);

	for (i = 0; (i <= total_loop_cnt); i++) {
		if (pinfo->is_snr_detail_en) {
			PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
				  "snr[%d]=%d\n", i, pinfo->snr_statistic[i]);
		}

		sum_snr_statistic += (u32)(pinfo->snr_statistic[i]);

		pinfo->snr_statistic_sqr[i] = (u16)(pinfo->snr_statistic[i] * pinfo->snr_statistic[i]);
	}

	phydm_bubble_sort(dm, pinfo->snr_statistic, pinfo->total_cnt_snr);

	/*update SNR's cur mean*/
	pinfo->cur_snr_mean = (sum_snr_statistic >> real_shift);

	for (i = 0; (i <= total_loop_cnt); i++) {
		if (pinfo->snr_statistic[i] >= pinfo->cur_snr_mean)
			temp = pinfo->snr_statistic[i] - pinfo->cur_snr_mean;
		else
			temp = pinfo->cur_snr_mean - pinfo->snr_statistic[i];

		pinfo->cur_snr_var += (temp * temp);
	}

	/*update SNR's VAR*/
	pinfo->cur_snr_var = (pinfo->cur_snr_var >> real_shift);

	/*acquire lower SNR's statistics*/
	temp = 0;
	pinfo->cnt_lower_snr_statistic = (total_snr_cnt >> pinfo->lwr_snr_ratio_bit_shift);
	pinfo->cnt_lower_snr_statistic = MAX_2(pinfo->cnt_lower_snr_statistic, SNR_RPT_MAX);

	for (i = 0; i < pinfo->cnt_lower_snr_statistic; i++)
		temp += pinfo->snr_statistic[i];

	pinfo->cur_lower_snr_mean = temp >> (real_shift - pinfo->lwr_snr_ratio_bit_shift);
}

boolean phydm_is_snr_improve(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;
	boolean is_snr_improve;
	u8 cur_state = pinfo->nxt_state;
	u32 cur_mean = pinfo->cur_snr_mean;
	u32 pre_mean = pinfo->pre_snr_mean;
	u32 cur_lower_mean = pinfo->cur_lower_snr_mean;
	u32 pre_lower_mean = pinfo->pre_lower_snr_mean;
	u32 cur_var = pinfo->cur_snr_var;

	/*special case, zero VAR, interference is gone*/
	 /*make sure pre_var is larger enough*/
	if (cur_state == SAT_TABLE_MONITOR ||
	    cur_state == ORI_TABLE_TRAINING) {
		if (cur_mean >= pre_mean) {
			if (cur_var == 0)
				return true;
		}
	}
#if 0
	/*special case, mean degrade less than VAR improvement*/
	/*make sure pre_var is larger enough*/
	if (cur_state == ORI_TABLE_MONITOR &&
	    cur_mean < pre_mean &&
	    cur_var < pre_var) {
		diff_mean = pre_mean - cur_mean;
		diff_var = pre_var - cur_var;
		return (diff_var > (2 * diff_mean * diff_mean)) ? true : false;
	}

#endif
	if (cur_lower_mean >= (pre_lower_mean + pinfo->delta_snr_mean))
		is_snr_improve = true;
	else
		is_snr_improve = false;
#if 0
/* condition refine, mean is bigger enough or VAR is smaller enough*/
/* 1. from mean's view, mean improve delta_snr_mean(2), VAR not degrade lot*/
	if (cur_mean > (pre_mean + pinfo->delta_snr_mean)) {
		is_mean_improve = TRUE;
		is_var_improve = (cur_var <= pre_var + dm->delta_snr_var)
				 ? TRUE : FALSE;

	} else if (cur_var + dm->delta_snr_var <= pre_var) {
		is_var_improve = TRUE;
		is_mean_improve = ((cur_mean + 1) >= pre_mean) ? TRUE : FALSE;
	} else {
		return false;
	}
#endif
	return is_snr_improve;
}

boolean phydm_is_snr_degrade(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;
	u32 cur_lower_mean = pinfo->cur_lower_snr_mean;
	u32 pre_lower_mean = pinfo->pre_lower_snr_mean;
	boolean is_degrade;

	if (cur_lower_mean <= (pre_lower_mean - pinfo->delta_snr_mean))
		is_degrade = TRUE;
	else
		is_degrade = FALSE;
#if 0
	is_mean_dgrade = (pinfo->cur_snr_mean + pinfo->delta_snr_mean <= pinfo->pre_snr_mean) ? TRUE : FALSE;
	is_var_degrade = (pinfo->cur_snr_var > (pinfo->pre_snr_var + pinfo->delta_snr_mean)) ? TRUE : FALSE;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s: cur_mean=%d, pre_mean=%d, cur_var=%d, pre_var=%d\n",
		  __func__,
		  pinfo->cur_snr_mean,
		  pinfo->pre_snr_mean,
		  pinfo->cur_snr_var,
		  pinfo->pre_snr_var);

	return (is_mean_dgrade & is_var_degrade);
#endif
	return is_degrade;
}

boolean phydm_is_large_var(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;
	boolean is_large_var = (pinfo->cur_snr_var >= pinfo->snr_var_thd) ? TRUE : FALSE;

	return is_large_var;
}

void phydm_update_pre_status(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	pinfo->pre_lower_snr_mean = pinfo->cur_lower_snr_mean;
	pinfo->pre_snr_mean = pinfo->cur_snr_mean;
	pinfo->pre_snr_var = pinfo->cur_snr_var;
}

void phydm_ori_table_monitor(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	if (phydm_is_large_var(dm)) {
		pinfo->nxt_state = SAT_TABLE_TRAINING;
		config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
	} else {
		pinfo->nxt_state = ORI_TABLE_MONITOR;
		/*switch to anti-sat table*/
		config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);
	}
	phydm_update_pre_status(dm);
	pinfo->pre_state = ORI_TABLE_MONITOR;
}

void phydm_sat_table_training(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	#if 0
	if pre_state = ORI_TABLE_MONITOR || SAT_TABLE_TRY_FAIL,
	/*"pre" adapt ori-table, "cur" adapt sat-table*/
	/*adapt ori table*/
	if (pinfo->pre_state == ORI_TABLE_MONITOR) {
		pinfo->nxt_state = SAT_TABLE_TRAINING;
		config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
	} else {
	#endif
	if (phydm_is_snr_improve(dm)) {
		pinfo->nxt_state = SAT_TABLE_MONITOR;
	} else {
		pinfo->nxt_state = SAT_TABLE_TRY_FAIL;
		config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);
	}
	/*}*/

	phydm_update_pre_status(dm);
	pinfo->pre_state = SAT_TABLE_TRAINING;
}

void phydm_sat_table_try_fail(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	/* if pre_state = SAT_TABLE_TRAINING, "pre" adapt sat-table, "cur" adapt ori-table */
	/* if pre_state = SAT_TABLE_TRY_FAIL, "pre" adapt ori-table, "cur" adapt ori-table */

	if (phydm_is_large_var(dm)) {
		if (phydm_is_snr_degrade(dm)) {
			pinfo->nxt_state = SAT_TABLE_TRAINING;
			config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
		} else {
			pinfo->nxt_state = SAT_TABLE_TRY_FAIL;
		}
	} else {
		pinfo->nxt_state = ORI_TABLE_MONITOR;
	}
	phydm_update_pre_status(dm);
	pinfo->pre_state = SAT_TABLE_TRY_FAIL;
}

void phydm_sat_table_monitor(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	if (phydm_is_snr_improve(dm)) {
		pinfo->sat_table_monitor_times = 0;

		/* if pre_state = SAT_TABLE_MONITOR, "pre" adapt sat-table, "cur" adapt sat-table */
		if (pinfo->pre_state == SAT_TABLE_MONITOR) {
			pinfo->nxt_state = ORI_TABLE_TRAINING;
			config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);
			//phydm_update_pre_status(dm);
		} else {
			pinfo->nxt_state = SAT_TABLE_MONITOR;
		}

		/* if pre_state = SAT_TABLE_TRAINING, "pre" adapt sat-table, "cur" adapt sat-table */
		/* if pre_state = ORI_TABLE_TRAINING, "pre" adapt ori-table, "cur" adapt sat-table */
		/*pre_state above is no need to update*/
	} else {
		if (pinfo->sat_table_monitor_times == pinfo->force_change_period) {
			PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s: sat_table_monitor_times=%d\n",
				  __func__, pinfo->sat_table_monitor_times);

			pinfo->nxt_state = ORI_TABLE_TRAINING;
			pinfo->sat_table_monitor_times = 0;
			config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);
		} else {
			pinfo->nxt_state = SAT_TABLE_MONITOR;
			pinfo->sat_table_monitor_times++;
		}
	}
	phydm_update_pre_status(dm);
	pinfo->pre_state = SAT_TABLE_MONITOR;
}

void phydm_ori_table_training(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	/* pre_state = SAT_TABLE_MONITOR, "pre" adapt sat-table, "cur" adapt ori-table */

	if (phydm_is_snr_degrade(dm) == FALSE) {
		pinfo->nxt_state = ORI_TABLE_MONITOR;
	} else {
		if (pinfo->pre_snr_var == 0)
			pinfo->nxt_state = ORI_TABLE_TRY_FAIL;
		else
			pinfo->nxt_state = SAT_TABLE_MONITOR;

		config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
	}
	phydm_update_pre_status(dm);
	pinfo->pre_state = ORI_TABLE_TRAINING;
}

void phydm_ori_table_try_fail(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;

	if (pinfo->pre_state == ORI_TABLE_TRY_FAIL) {
		if (phydm_is_snr_improve(dm)) {
			pinfo->nxt_state = ORI_TABLE_TRAINING;
			pinfo->ori_table_try_fail_times = 0;
			config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);
		} else {
			if (pinfo->ori_table_try_fail_times == pinfo->force_change_period) {
				PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
					  "%s: ori_table_try_fail_times=%d\n", __func__, pinfo->ori_table_try_fail_times);

				pinfo->nxt_state = ORI_TABLE_TRY_FAIL;
				pinfo->ori_table_try_fail_times = 0;
				phydm_update_pre_status(dm);
			} else {
				pinfo->nxt_state = ORI_TABLE_TRY_FAIL;
				pinfo->ori_table_try_fail_times++;
				phydm_update_pre_status(dm);
				//config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
			}
		}
	} else {
		pinfo->nxt_state = ORI_TABLE_TRY_FAIL;
		pinfo->ori_table_try_fail_times = 0;
		phydm_update_pre_status(dm);
		//config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
	}

#if 0
	if (phydm_is_large_var(dm)) {
		if (phydm_is_snr_degrade(dm)) {
			pinfo->nxt_state = SAT_TABLE_TRAINING;
			config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
		} else {
			pinfo->nxt_state = SAT_TABLE_TRY_FAIL;
		}
	} else {
		pinfo->nxt_state = ORI_TABLE_MONITOR;
	}

	phydm_update_pre_status(dm);
#endif
	pinfo->pre_state = ORI_TABLE_TRY_FAIL;
}

char *phydm_lna_sat_state_msg(
	void *dm_void,
	IN u8 state)
{
	char *dbg_message;

	switch (state) {
	case ORI_TABLE_MONITOR:
		dbg_message = "ORI_TABLE_MONITOR";
		break;

	case SAT_TABLE_TRAINING:
		dbg_message = "SAT_TABLE_TRAINING";
		break;

	case SAT_TABLE_TRY_FAIL:
		dbg_message = "SAT_TABLE_TRY_FAIL";
		break;

	case SAT_TABLE_MONITOR:
		dbg_message = "SAT_TABLE_MONITOR";
		break;

	case ORI_TABLE_TRAINING:
		dbg_message = "ORI_TABLE_TRAINING";
		break;

	case ORI_TABLE_TRY_FAIL:
		dbg_message = "ORI_TABLE_TRY_FAIL";
		break;

	default:
		dbg_message = "ORI_TABLE_MONITOR";
		break;
	}

	return dbg_message;
}

void phydm_lna_sat_type2_sm(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*pinfo = &dm->dm_lna_sat_info;
	u8 state = pinfo->nxt_state;
	u8 agc_tab = (u8)odm_get_bb_reg(dm, 0x958, 0x1f);
	char *dbg_message, *nxt_dbg_message;
	u8 real_shift = pinfo->total_bit_shift;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "\n\n%s ==>\n", __func__);

	if ((dm->support_ic_type & ODM_RTL8822B) == FALSE) {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "ODM_BB_LNA_SAT_CHK_TYPE2 only support 22B.\n");
		return;
	}

	if ((dm->support_ability & ODM_BB_LNA_SAT_CHK) == FALSE) {
		phydm_lna_sat_chk_type2_init(dm);
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "ODM_BB_LNA_SAT_CHK_TYPE2 is NOT supported, cur table=%d\n", agc_tab);
		return;
	}

	if (pinfo->is_snr_done)
		phydm_snr_data_processing(dm);
	else
		return;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "cur agc table %d\n", agc_tab);

	if (pinfo->is_force_lna_sat_table != AUTO_AGC_TABLE) {
		/*reset state machine*/
		pinfo->nxt_state = ORI_TABLE_MONITOR;
		if (pinfo->is_snr_done) {
			if (pinfo->is_force_lna_sat_table == DEFAULT_AGC_TABLE)
				config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);
			else if (pinfo->is_force_lna_sat_table == LNA_SAT_AGC_TABLE)
				config_phydm_switch_agc_tab_8822b(dm, *dm->channel, LNA_SAT_AGC_TABLE);
			else
				config_phydm_switch_agc_tab_8822b(dm, *dm->channel, DEFAULT_AGC_TABLE);

			PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
				  "%s: cur_mean=%d, pre_mean=%d, cur_var=%d, pre_var=%d,cur_lower_mean=%d, pre_lower_mean=%d, cnt_lower_snr=%d\n",
				  __func__,
				  pinfo->cur_snr_mean,
				  pinfo->pre_snr_mean,
				  pinfo->cur_snr_var,
				  pinfo->pre_snr_var,
				  pinfo->cur_lower_snr_mean,
				  pinfo->pre_lower_snr_mean,
				  pinfo->cnt_lower_snr_statistic);

			pinfo->is_snr_done = FALSE;
			pinfo->is_sm_done = TRUE;
			phydm_update_pre_status(dm);
		} else {
			return;
		}
	} else if (pinfo->is_snr_done) {
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK,
			  "%s: cur_mean=%d, pre_mean=%d, cur_var=%d, pre_var=%d,cur_lower_mean=%d, pre_lower_mean=%d, cnt_lower_snr=%d\n",
			  __func__,
			  pinfo->cur_snr_mean,
			  pinfo->pre_snr_mean,
			  pinfo->cur_snr_var,
			  pinfo->pre_snr_var,
			  pinfo->cur_lower_snr_mean,
			  pinfo->pre_lower_snr_mean,
			  pinfo->cnt_lower_snr_statistic);

		switch (state) {
		case ORI_TABLE_MONITOR:
			dbg_message = "ORI_TABLE_MONITOR";
			phydm_ori_table_monitor(dm);
			break;

		case SAT_TABLE_TRAINING:
			dbg_message = "SAT_TABLE_TRAINING";
			phydm_sat_table_training(dm);
			break;

		case SAT_TABLE_TRY_FAIL:
			dbg_message = "SAT_TABLE_TRY_FAIL";
			phydm_sat_table_try_fail(dm);
			break;

		case SAT_TABLE_MONITOR:
			dbg_message = "SAT_TABLE_MONITOR";
			phydm_sat_table_monitor(dm);
			break;

		case ORI_TABLE_TRAINING:
			dbg_message = "ORI_TABLE_TRAINING";
			phydm_ori_table_training(dm);
			break;

		case ORI_TABLE_TRY_FAIL:
			dbg_message = "ORI_TABLE_TRAINING";
			phydm_ori_table_try_fail(dm);
			break;

		default:
			dbg_message = "ORI_TABLE_MONITOR";
			phydm_ori_table_monitor(dm);
			break;
		}

		dbg_message = phydm_lna_sat_state_msg(dm, state);
		nxt_dbg_message = phydm_lna_sat_state_msg(dm, pinfo->nxt_state);
		PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "state: [%s]->[%s]\n",
			  dbg_message, nxt_dbg_message);

		pinfo->is_snr_done = FALSE;
		pinfo->is_sm_done = TRUE;
		pinfo->total_cnt_snr = 1 << real_shift;

	} else {
		return;
	}
}


#endif /*#if (PHYDM_LNA_SAT_CHK_TYPE2 == 1)*/

void phydm_lna_sat_debug(
	void *dm_void,
	char input[][16],
	u32 *_used,
	char *output,
	u32 *_out_len,
	u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*lna_t = &dm->dm_lna_sat_info;
	char help[] = "-h";
	u32 var1[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;

	if ((strcmp(input[1], help) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "help content\n");

	} else {
		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		for (i = 1; i < 10; i++) {
			if (input[i + 1])
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
		}
		#ifdef PHYDM_LNA_SAT_CHK_TYPE2
		if (var1[0] == 1)
			lna_t->force_traget_macid = var1[1];
		#endif
	}
	*_used = used;
	*_out_len = out_len;
}

void phydm_lna_sat_chk_watchdog(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t *lna_sat = &dm->dm_lna_sat_info;

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "%s ==>\n", __func__);

	if (lna_sat->lna_sat_type == LNA_SAT_WITH_PEAK_DET) {
		#ifdef PHYDM_LNA_SAT_CHK_TYPE1
		phydm_lna_sat_chk_watchdog_type1(dm);
		#endif
	} else if (lna_sat->lna_sat_type == LNA_SAT_WITH_TRAIN) {
		#ifdef PHYDM_LNA_SAT_CHK_TYPE2

		#endif
	}

}

void phydm_lna_sat_config(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*lna_sat = &dm->dm_lna_sat_info;

	#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & (ODM_RTL8822B))
		lna_sat->lna_sat_type = LNA_SAT_WITH_TRAIN;
	#endif

	#if (RTL8197F_SUPPORT == 1) || (RTL8192F_SUPPORT == 1) || (RTL8198F_SUPPORT == 1)
	if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F | ODM_RTL8198F))
		lna_sat->lna_sat_type = LNA_SAT_WITH_PEAK_DET;
	#endif

	PHYDM_DBG(dm, DBG_LNA_SAT_CHK, "[%s] lna_sat_type=%d\n",
		  __func__, lna_sat->lna_sat_type);
}

void phydm_lna_sat_check_init(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_lna_sat_t	*lna_sat = &dm->dm_lna_sat_info;

	if ((dm->support_ability & ODM_BB_LNA_SAT_CHK))
		return;

	if (lna_sat->lna_sat_type == LNA_SAT_WITH_PEAK_DET) {
		#ifdef PHYDM_LNA_SAT_CHK_TYPE1
		phydm_lna_sat_chk_init(dm);
		#endif
	} else if (lna_sat->lna_sat_type == LNA_SAT_WITH_TRAIN) {
		#ifdef PHYDM_LNA_SAT_CHK_TYPE2
		phydm_lna_sat_chk_type2_init(dm);
		#endif
	}
}

#endif /*#if (PHYDM_LNA_SAT_CHK_SUPPORT == 1)*/
