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

/* ************************************************************
 * include files
 * ************************************************************
 */
#include "mp_precomp.h"
#include "phydm_precomp.h"

void phydm_dig_recorder_reset(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_dig_recoder_strcut *dig_rc = &dig_t->dig_recoder_t;

	PHYDM_DBG(dm, DBG_DIG, "%s ======>\n", __func__);

	odm_memory_set(dm, &dig_rc->igi_bitmap, 0,
		       sizeof(struct phydm_dig_recoder_strcut));
}

void phydm_dig_recorder(void *dm_void, boolean first_connect, u8 igi_curr, u32 fa_cnt)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_dig_recoder_strcut *dig_rc = &dig_t->dig_recoder_t;
	u8 igi_pre = dig_rc->igi_history[0];
	u8 igi_up = 0;

	PHYDM_DBG(dm, DBG_DIG, "%s ======>\n", __func__);

	if (!dm->is_linked)
		return;

	if (first_connect) {
		phydm_dig_recorder_reset(dm);
		dig_rc->igi_history[0] = igi_curr;
		dig_rc->fa_history[0] = fa_cnt;
		return;
	}

	igi_pre = dig_rc->igi_history[0];
	igi_up = (igi_curr > igi_pre) ? 1 : 0;
	dig_rc->igi_bitmap = ((dig_rc->igi_bitmap << 1) & 0xfe) | igi_up;

	dig_rc->igi_history[3] = dig_rc->igi_history[2];
	dig_rc->igi_history[2] = dig_rc->igi_history[1];
	dig_rc->igi_history[1] = dig_rc->igi_history[0];
	dig_rc->igi_history[0] = igi_curr;

	dig_rc->fa_history[3] = dig_rc->fa_history[2];
	dig_rc->fa_history[2] = dig_rc->fa_history[1];
	dig_rc->fa_history[1] = dig_rc->fa_history[0];
	dig_rc->fa_history[0] = fa_cnt;

	PHYDM_DBG(dm, DBG_DIG, "igi_history[3:0] = {0x%x, 0x%x, 0x%x, 0x%x}\n",
		  dig_rc->igi_history[3], dig_rc->igi_history[2],
		  dig_rc->igi_history[1], dig_rc->igi_history[0]);
	PHYDM_DBG(dm, DBG_DIG, "fa_history[3:0] = {%d, %d, %d, %d}\n",
		  dig_rc->fa_history[3], dig_rc->fa_history[2],
		  dig_rc->fa_history[1], dig_rc->fa_history[0]);
	PHYDM_DBG(dm, DBG_DIG, "igi_bitmap = 0x%x\n", dig_rc->igi_bitmap);
}

boolean
phydm_dig_go_up_check(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct ccx_info *ccx_info = &dm->dm_ccx_info;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	u8 cur_ig_value = dig_t->cur_ig_value;
	u8 max_cover_bond = 0;
	u8 rx_gain_range_max = dig_t->rx_gain_range_max;
	u8 i = 0, j = 0;
	u8 total_nhm_cnt = ccx_info->nhm_rpt_sum;
	u32 dig_cnt = 0;
	u32 over_dig_cnt = 0;
	boolean ret = true;

	if (*dm->bb_op_mode == PHYDM_PERFORMANCE_MODE)
		return ret;

	max_cover_bond = DIG_MAX_BALANCE_MODE - dig_t->upcheck_init_val;

	if (cur_ig_value < max_cover_bond - 6)
		dig_t->go_up_chk_lv = DIG_GOUPCHECK_LEVEL_0;
	else if (cur_ig_value <= DIG_MAX_BALANCE_MODE)
		dig_t->go_up_chk_lv = DIG_GOUPCHECK_LEVEL_1;
	else /* cur_ig_value > DM_DIG_MAX_AP, foolproof */
		dig_t->go_up_chk_lv = DIG_GOUPCHECK_LEVEL_2;

	PHYDM_DBG(dm, DBG_DIG, "check_lv = %d, max_cover_bond = 0x%x\n",
		  dig_t->go_up_chk_lv, max_cover_bond);

	if (total_nhm_cnt == 0)
		return true;

	if (dig_t->go_up_chk_lv == DIG_GOUPCHECK_LEVEL_0) {
		for (i = 3; i <= 11; i++)
			dig_cnt += ccx_info->nhm_result[i];

		if ((dig_t->lv0_ratio_reciprocal * dig_cnt) >= total_nhm_cnt)
			ret = true;
		else
			ret = false;

	} else if (dig_t->go_up_chk_lv == DIG_GOUPCHECK_LEVEL_1) {
		/* search index */
		for (i = 0; i <= 10; i++) {
			if ((max_cover_bond * 2) == ccx_info->nhm_th[i]) {
				for (j = (i + 1); j <= 11; j++)
					over_dig_cnt += ccx_info->nhm_result[j];
				break;
			}
		}

		if (dig_t->lv1_ratio_reciprocal * over_dig_cnt < total_nhm_cnt)
			ret = true;
		else
			ret = false;

		if (!ret) {
			/* update dig_t->rx_gain_range_max */
			if (rx_gain_range_max + 6 >= max_cover_bond)
				dig_t->rx_gain_range_max =  max_cover_bond - 6;
			else
				dig_t->rx_gain_range_max =  rx_gain_range_max;

			PHYDM_DBG(dm, DBG_DIG,
				  "Noise pwr over DIG can filter, lock rx_gain_range_max to 0x%x\n",
				  dig_t->rx_gain_range_max);
		}
	} else if (dig_t->go_up_chk_lv == DIG_GOUPCHECK_LEVEL_2) {
		/* cur_ig_value > DM_DIG_MAX_AP, foolproof */
		ret = true;
	}

	return ret;
}

void odm_fa_threshold_check(void *dm_void, boolean is_dfs_band)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	if (dig_t->is_dbg_fa_th) {
		PHYDM_DBG(dm, DBG_DIG, "Manual Fix FA_th\n");

	} else if (dm->is_linked) {
		if (dm->rssi_min < 20) { /*[PHYDM-252]*/
			dig_t->fa_th[0] = 500;
			dig_t->fa_th[1] = 750;
			dig_t->fa_th[2] = 1000;
		} else if (((dm->rx_tp >> 2) > dm->tx_tp) && /*Test RX TP*/
			   (dm->rx_tp < 10) && (dm->rx_tp > 1)) { /*TP=1~10Mb*/
			dig_t->fa_th[0] = 125;
			dig_t->fa_th[1] = 250;
			dig_t->fa_th[2] = 500;
		} else {
			dig_t->fa_th[0] = 250;
			dig_t->fa_th[1] = 500;
			dig_t->fa_th[2] = 750;
		}
	} else {
		if (is_dfs_band) { /* For DFS band and no link */

			dig_t->fa_th[0] = 250;
			dig_t->fa_th[1] = 1000;
			dig_t->fa_th[2] = 2000;
		} else {
			dig_t->fa_th[0] = 2000;
			dig_t->fa_th[1] = 4000;
			dig_t->fa_th[2] = 5000;
		}
	}

	PHYDM_DBG(dm, DBG_DIG, "FA_th={%d,%d,%d}\n", dig_t->fa_th[0],
		  dig_t->fa_th[1], dig_t->fa_th[2]);
}

void phydm_set_big_jump_step(void *dm_void, u8 curr_igi)
{
#if (RTL8822B_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	u8 step1[8] = {24, 30, 40, 50, 60, 70, 80, 90};
	u8 big_jump_lmt = dig_t->big_jump_lmt[dig_t->agc_table_idx];
	u8 i;

	if (dig_t->enable_adjust_big_jump == 0)
		return;

	for (i = 0; i <= dig_t->big_jump_step1; i++) {
		if ((curr_igi + step1[i]) > big_jump_lmt) {
			if (i != 0)
				i = i - 1;
			break;
		} else if (i == dig_t->big_jump_step1) {
			break;
		}
	}
	if (dm->support_ic_type & ODM_RTL8822B)
		odm_set_bb_reg(dm, R_0x8c8, 0xe, i);
	else if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
		odm_set_bb_reg(dm, ODM_REG_BB_AGC_SET_2_11N, 0xe, i);

	PHYDM_DBG(dm, DBG_DIG, "Bigjump = %d (ori = 0x%x), LMT=0x%x\n", i,
		  dig_t->big_jump_step1, big_jump_lmt);
#endif
}

#ifdef PHYDM_IC_11AC_3_SERIES_SUPPORT
void phydm_write_dig_reg_ac3(void *dm_void, u8 igi)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	PHYDM_DBG(dm, DBG_DIG, "%s===>\n", __func__);

	/* Set IGI value */
	if (!(dm->support_ic_type & ODM_IC_11AC_3_SERIES))
		return;

	odm_set_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_11AC, igi);

	#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	if (dm->support_ic_type & PHYDM_IC_ABOVE_2SS)
		odm_set_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_B_11AC3, igi);
	#endif

	#if (defined(PHYDM_COMPILE_ABOVE_4SS))
	if (dm->support_ic_type & PHYDM_IC_ABOVE_4SS) {
		odm_set_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_C_11AC3, igi);
		odm_set_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_D_11AC3, igi);
	}
	#endif
}

u8 phydm_get_igi_reg_val_ac3(
	void *dm_void,
	enum bb_path path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 val = 0;

	PHYDM_DBG(dm, DBG_DIG, "%s===>\n", __func__);

	/* Set IGI value */
	if (!(dm->support_ic_type & ODM_IC_11AC_3_SERIES))
		return;

	if (path == BB_PATH_A)
		val = odm_get_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_11AC);
	else if (path == BB_PATH_B)
		#if (defined(PHYDM_COMPILE_ABOVE_2SS))
		val = odm_get_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_B_11AC3);
		#else
		;
		#endif
	else if (path == BB_PATH_C)
		#if (defined(PHYDM_COMPILE_ABOVE_3SS))
		val = odm_get_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_C_11AC3);
		#else
		;
		#endif
	else if (path == BB_PATH_D)
		#if (defined(PHYDM_COMPILE_ABOVE_4SS))
		val = odm_get_bb_reg(dm, R_0x1d70, ODM_BIT_IGI_D_11AC3);
		#else
		;
		#endif

	return (u8)val;
}

void phydm_fa_cnt_statistics_ac3(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;
	u32 ret_value = 0;
	u32 cck_enable = 0;

	if (!(dm->support_ic_type & ODM_IC_11AC_3_SERIES))
		return;

	ret_value = odm_get_bb_reg(dm, R_0x2d20, MASKDWORD);
	fa_t->cnt_fast_fsync = (ret_value & 0xffff);
	fa_t->cnt_sb_search_fail = ((ret_value & 0xffff0000) >> 16);

	ret_value = odm_get_bb_reg(dm, R_0x2d04, MASKDWORD);
	fa_t->cnt_parity_fail = ((ret_value & 0xffff0000) >> 16);

	ret_value = odm_get_bb_reg(dm, R_0x2d08, MASKDWORD);
	fa_t->cnt_rate_illegal = (ret_value & 0xffff);
	fa_t->cnt_crc8_fail = ((ret_value & 0xffff0000) >> 16);

	ret_value = odm_get_bb_reg(dm, R_0x2d10, MASKDWORD);
	fa_t->cnt_mcs_fail = (ret_value & 0xffff);

	/* read OFDM FA counter */
	fa_t->cnt_ofdm_fail = odm_get_bb_reg(dm, R_0x2d00, MASKLWORD);

	/* Read CCK FA counter */
	fa_t->cnt_cck_fail = odm_get_bb_reg(dm, R_0x1a5c, 0xff);

	/* read CCK/OFDM CCA counter */
	ret_value = odm_get_bb_reg(dm, R_0x2c08, MASKDWORD);
	fa_t->cnt_ofdm_cca = ((ret_value & 0xffff0000) >> 16);
	fa_t->cnt_cck_cca = ret_value & 0xffff;

	/* read CCK CRC32 counter */
	ret_value = odm_get_bb_reg(dm, R_0x2c04, MASKDWORD);
	fa_t->cnt_cck_crc32_error = ((ret_value & 0xffff0000) >> 16);
	fa_t->cnt_cck_crc32_ok = ret_value & 0xffff;

	/* read OFDM CRC32 counter */
	ret_value = odm_get_bb_reg(dm, R_0x2c14, MASKDWORD);
	fa_t->cnt_ofdm_crc32_error = ((ret_value & 0xffff0000) >> 16);
	fa_t->cnt_ofdm_crc32_ok = ret_value & 0xffff;

	/* read HT CRC32 counter */
	ret_value = odm_get_bb_reg(dm, R_0x2c10, MASKDWORD);
	fa_t->cnt_ht_crc32_error = ((ret_value & 0xffff0000) >> 16);
	fa_t->cnt_ht_crc32_ok = ret_value & 0xffff;

	cck_enable = odm_get_bb_reg(dm, R_0x1c3c, BIT(1)); /* 98f 1C3c[1] */
	if (cck_enable) { /* if(*dm->band_type == ODM_BAND_2_4G) */
		fa_t->cnt_all = fa_t->cnt_ofdm_fail + fa_t->cnt_cck_fail;
		fa_t->cnt_cca_all = fa_t->cnt_cck_cca + fa_t->cnt_ofdm_cca;
	} else {
		fa_t->cnt_all = fa_t->cnt_ofdm_fail;
		fa_t->cnt_cca_all = fa_t->cnt_ofdm_cca;
	}

	PHYDM_DBG(dm, DBG_FA_CNT,
		  "[OFDM FA Detail] Parity_fail=((%d)), Rate_Illegal=((%d)), CRC8_fail=((%d)), Mcs_fail=((%d)), Fast_Fsync=((%d)), SBD_fail=((%d))\n",
		  fa_t->cnt_parity_fail, fa_t->cnt_rate_illegal,
		  fa_t->cnt_crc8_fail, fa_t->cnt_mcs_fail, fa_t->cnt_fast_fsync,
		  fa_t->cnt_sb_search_fail);
}

#endif

void phydm_write_dig_reg(void *dm_void, u8 igi)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	PHYDM_DBG(dm, DBG_DIG, "%s===>\n", __func__);

	odm_set_bb_reg(dm, ODM_REG(IGI_A, dm), ODM_BIT(IGI, dm), igi);

	#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	if (dm->support_ic_type & PHYDM_IC_ABOVE_2SS)
		odm_set_bb_reg(dm, ODM_REG(IGI_B, dm), ODM_BIT(IGI, dm), igi);
	#endif

	#if (defined(PHYDM_COMPILE_ABOVE_4SS))
	if (dm->support_ic_type & PHYDM_IC_ABOVE_4SS) {
		odm_set_bb_reg(dm, ODM_REG(IGI_C, dm), ODM_BIT(IGI, dm), igi);
		odm_set_bb_reg(dm, ODM_REG(IGI_D, dm), ODM_BIT(IGI, dm), igi);
	}
	#endif
}

void odm_write_dig(void *dm_void, u8 new_igi)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_adaptivity_struct *adaptivity = &dm->adaptivity;

	PHYDM_DBG(dm, DBG_DIG, "%s===>\n", __func__);

	/* 1 Check IGI by upper bound */
	if (adaptivity->igi_lmt_en &&
	    new_igi > adaptivity->adapt_igi_up && dm->is_linked) {
		new_igi = adaptivity->adapt_igi_up;

		PHYDM_DBG(dm, DBG_DIG, "Force Adaptivity Up-bound=((0x%x))\n",
			  new_igi);
	}

	#if (RTL8192F_SUPPORT == 1)
	if ((dm->support_ic_type & ODM_RTL8192F) &&
	    dm->cut_version == ODM_CUT_A &&
	    new_igi > 0x38) {
		new_igi = 0x38;
		PHYDM_DBG(dm, DBG_DIG,
			  "Force 92F Adaptivity Up-bound=((0x%x))\n", new_igi);
	}
	#endif

	if (dig_t->cur_ig_value != new_igi) {
		#if (RTL8822B_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1)
		/* Modify big jump step for 8822B and 8197F */
		if (dm->support_ic_type &
		    (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8192F))
			phydm_set_big_jump_step(dm, new_igi);
		#endif

		#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
		/* Set IGI value of CCK for new CCK AGC */
		if (dm->cck_new_agc &&
		    (dm->support_ic_type & PHYSTS_2ND_TYPE_IC))
			odm_set_bb_reg(dm, R_0xa0c, 0x3f00, (new_igi >> 1));
		#endif

		/*Add by YuChen for USB IO too slow issue*/
		if (dm->support_ic_type &
		    (ODM_IC_11AC_GAIN_IDX_EDCCA | ODM_IC_11N_GAIN_IDX_EDCCA)) {
			if ((dm->support_ability & ODM_BB_ADAPTIVITY) &&
			    new_igi < dig_t->cur_ig_value) {
				dig_t->cur_ig_value = new_igi;
				phydm_adaptivity(dm);
			}
		} else {
			if ((dm->support_ability & ODM_BB_ADAPTIVITY) &&
			    new_igi > dig_t->cur_ig_value) {
				dig_t->cur_ig_value = new_igi;
				phydm_adaptivity(dm);
			}
		}

		#ifdef PHYDM_IC_11AC_3_SERIES_SUPPORT
		if (dm->support_ic_type & ODM_IC_11AC_3_SERIES)
			phydm_write_dig_reg_ac3(dm, new_igi);
		else
		#endif
			phydm_write_dig_reg(dm, new_igi);

		dig_t->cur_ig_value = new_igi;
	}

	PHYDM_DBG(dm, DBG_DIG, "New_igi=((0x%x))\n\n", new_igi);
}

u8 phydm_get_igi_reg_val(
	void *dm_void,
	enum bb_path path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 val = 0;

	if (path == BB_PATH_A)
		val = odm_get_bb_reg(dm, ODM_REG(IGI_A, dm), ODM_BIT(IGI, dm));
	else if (path == BB_PATH_B)
		#if (defined(PHYDM_COMPILE_ABOVE_2SS))
		val = odm_get_bb_reg(dm, ODM_REG(IGI_B, dm), ODM_BIT(IGI, dm));
		#else
		;
		#endif
	else if (path == BB_PATH_C)
		#if (defined(PHYDM_COMPILE_ABOVE_3SS))
		val = odm_get_bb_reg(dm, ODM_REG(IGI_C, dm), ODM_BIT(IGI, dm));
		#else
		;
		#endif
	else if (path == BB_PATH_D)
		#if (defined(PHYDM_COMPILE_ABOVE_4SS))
		val = odm_get_bb_reg(dm, ODM_REG(IGI_D, dm), ODM_BIT(IGI, dm));
		#else
		;
		#endif

	return (u8)val;
}

u8 phydm_get_igi(
	void *dm_void,
	enum bb_path path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 val;

	#ifdef PHYDM_IC_11AC_3_SERIES_SUPPORT
	if (dm->support_ic_type & ODM_IC_11AC_3_SERIES)
		val = phydm_get_igi_reg_val_ac3(dm, path);
	else
	#endif
		val = phydm_get_igi_reg_val(dm, path);

	return val;
}

void phydm_set_dig_val(void *dm_void, u32 *val_buf, u8 val_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (val_len != 1) {
		PHYDM_DBG(dm, ODM_COMP_API, "[Error][DIG]Need val_len=1\n");
		return;
	}

	odm_write_dig(dm, (u8)(*val_buf));
}

void odm_pause_dig(void *dm_void, enum phydm_pause_type type,
		   enum phydm_pause_level lv, u8 igi_input)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 rpt = false;
	u32 igi = (u32)igi_input;

	PHYDM_DBG(dm, DBG_DIG, "[%s]type=%d, LV=%d, igi=0x%x\n", __func__, type,
		  lv, igi);

	switch (type) {
	case PHYDM_PAUSE:
	case PHYDM_PAUSE_NO_SET: {
		rpt = phydm_pause_func(dm, F00_DIG, PHYDM_PAUSE, lv, 1, &igi);
		break;
	}

	case PHYDM_RESUME: {
		rpt = phydm_pause_func(dm, F00_DIG, PHYDM_RESUME, lv, 1, &igi);
		break;
	}
	default:
		PHYDM_DBG(dm, DBG_DIG, "Wrong type\n");
		break;
	}

	PHYDM_DBG(dm, DBG_DIG, "pause_result=%d\n", rpt);
}

boolean
odm_dig_abort(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	void *adapter = dm->adapter;
#endif

	/* support_ability */
	if ((!(dm->support_ability & ODM_BB_FA_CNT)) ||
	    (!(dm->support_ability & ODM_BB_DIG)) ||
	    *dm->is_scan_in_process) {
		PHYDM_DBG(dm, DBG_DIG, "Not Support\n");
		return true;
	}

	if (dm->pause_ability & ODM_BB_DIG) {
		PHYDM_DBG(dm, DBG_DIG, "Return: Pause DIG in LV=%d\n",
			  dm->pause_lv_table.lv_dig);
		return true;
	}

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
#if OS_WIN_FROM_WIN7(OS_VERSION)
	if (IsAPModeExist(adapter) && ((PADAPTER)(adapter))->bInHctTest) {
		PHYDM_DBG(dm, DBG_DIG, " Return: Is AP mode or In HCT Test\n");
		return true;
	}
#endif
#endif

	return false;
}

void phydm_dig_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
#if (DM_ODM_SUPPORT_TYPE & (ODM_AP))
	struct phydm_fa_struct *false_alm_cnt = &dm->false_alm_cnt;
#endif
	u32 ret_value = 0;
	u8 i;

	dig_t->dm_dig_max = DIG_MAX_BALANCE_MODE;
	dig_t->dm_dig_min = DIG_MIN_PERFORMANCE;
	dig_t->dig_max_of_min = DIG_MAX_OF_MIN_BALANCE_MODE;

	dig_t->cur_ig_value = phydm_get_igi(dm, BB_PATH_A);

	dig_t->is_media_connect = false;

	dig_t->fa_th[0] = 250;
	dig_t->fa_th[1] = 500;
	dig_t->fa_th[2] = 750;
	dig_t->is_dbg_fa_th = false;
#if (DM_ODM_SUPPORT_TYPE & (ODM_AP))
	/* For RTL8881A */
	false_alm_cnt->cnt_ofdm_fail_pre = 0;
#endif

	dig_t->rx_gain_range_max = DIG_MAX_BALANCE_MODE;
	dig_t->rx_gain_range_min = dig_t->cur_ig_value;

#if (RTL8822B_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1)
	dig_t->enable_adjust_big_jump = 1;
	if (dm->support_ic_type & ODM_RTL8822B)
		ret_value = odm_get_bb_reg(dm, R_0x8c8, MASKLWORD);
	else if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
		ret_value = odm_get_bb_reg(dm, R_0xc74, MASKLWORD);

	dig_t->big_jump_step1 = (u8)(ret_value & 0xe) >> 1;
	dig_t->big_jump_step2 = (u8)(ret_value & 0x30) >> 4;
	dig_t->big_jump_step3 = (u8)(ret_value & 0xc0) >> 6;

	if (dm->support_ic_type &
	    (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8192F)) {
		for (i = 0; i < sizeof(dig_t->big_jump_lmt); i++) {
			if (dig_t->big_jump_lmt[i] == 0)
				dig_t->big_jump_lmt[i] = 0x64;
				/* Set -10dBm as default value */
		}
	}
#endif

#ifdef PHYDM_TDMA_DIG_SUPPORT
	dm->original_dig_restore = 1;
#endif
	phydm_dig_recorder_reset(dm);
}

void phydm_dig_abs_boundary_decision(struct dm_struct *dm, boolean is_dfs_band)
{
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	if (!dm->is_linked) {
		dig_t->dm_dig_max = DIG_MAX_COVERAGR;
		dig_t->dm_dig_min = DIG_MIN_COVERAGE;
	} else if (is_dfs_band == true) {
		if (*dm->band_width == CHANNEL_WIDTH_20)
			dig_t->dm_dig_min = DIG_MIN_DFS + 2;
		else
			dig_t->dm_dig_min = DIG_MIN_DFS;

		dig_t->dig_max_of_min = DIG_MAX_OF_MIN_BALANCE_MODE;
		dig_t->dm_dig_max = DIG_MAX_BALANCE_MODE;
	} else {
		if (*dm->bb_op_mode == PHYDM_BALANCE_MODE) {
		/*service > 2 devices*/
			dig_t->dm_dig_max = DIG_MAX_BALANCE_MODE;
			#if (DIG_HW == 1)
			dig_t->dig_max_of_min = DIG_MIN_COVERAGE;
			#else
			dig_t->dig_max_of_min = DIG_MAX_OF_MIN_BALANCE_MODE;
			#endif
		} else if (*dm->bb_op_mode == PHYDM_PERFORMANCE_MODE) {
		/*service 1 devices*/
			dig_t->dm_dig_max = DIG_MAX_PERFORMANCE_MODE;
			dig_t->dig_max_of_min = DIG_MAX_OF_MIN_PERFORMANCE_MODE;
		}

		if (dm->support_ic_type &
		    (ODM_RTL8814A | ODM_RTL8812 | ODM_RTL8821 | ODM_RTL8822B))
			dig_t->dm_dig_min = 0x1c;
		else if (dm->support_ic_type & ODM_RTL8197F)
			dig_t->dm_dig_min = 0x1e; /*For HW setting*/
		else
			dig_t->dm_dig_min = DIG_MIN_PERFORMANCE;
	}

	PHYDM_DBG(dm, DBG_DIG, "Abs{Max, Min}={0x%x, 0x%x}, Max_of_min=0x%x\n",
		  dig_t->dm_dig_max, dig_t->dm_dig_min, dig_t->dig_max_of_min);
}

void phydm_dig_dym_boundary_decision(struct dm_struct *dm)
{
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	u8 offset = 15, tmp_max = 0;
	u8 max_of_rssi_min = 0;

	PHYDM_DBG(dm, DBG_RA, "%s ======>\n", __func__);

	if (!dm->is_linked) {
		/*if no link, always stay at lower bound*/
		dig_t->rx_gain_range_max = dig_t->dig_max_of_min;
		dig_t->rx_gain_range_min = dig_t->dm_dig_min;

		PHYDM_DBG(dm, DBG_DIG, "No-Link, Dyn{Max, Min}={0x%x, 0x%x}\n",
			  dig_t->rx_gain_range_max, dig_t->rx_gain_range_min);
		return;
	}

	PHYDM_DBG(dm, DBG_DIG, "rssi_min=%d, ofst=%d\n", dm->rssi_min, offset);

	/* DIG lower bound */
	if (dm->rssi_min > dig_t->dig_max_of_min)
		dig_t->rx_gain_range_min = dig_t->dig_max_of_min;
	else if (dm->rssi_min < dig_t->dm_dig_min)
		dig_t->rx_gain_range_min = dig_t->dm_dig_min;
	else
		dig_t->rx_gain_range_min = dm->rssi_min;

	/* DIG upper bound */
	tmp_max = dig_t->rx_gain_range_min + offset;
	if (dig_t->rx_gain_range_min != dm->rssi_min) {
		max_of_rssi_min = dm->rssi_min + offset;
		if (tmp_max > max_of_rssi_min)
			tmp_max = max_of_rssi_min;
	}

	if (tmp_max > dig_t->dm_dig_max)
		dig_t->rx_gain_range_max = dig_t->dm_dig_max;
	else
		dig_t->rx_gain_range_max = tmp_max;

	/* 1 Force Lower Bound for AntDiv */
	if (!dm->is_one_entry_only &&
	    (dm->support_ability & ODM_BB_ANT_DIV) &&
	    (dm->ant_div_type == CG_TRX_HW_ANTDIV ||
	     dm->ant_div_type == CG_TRX_SMART_ANTDIV)) {
		if (dig_t->ant_div_rssi_max > dig_t->dig_max_of_min)
			dig_t->rx_gain_range_min = dig_t->dig_max_of_min;
		else
			dig_t->rx_gain_range_min = (u8)dig_t->ant_div_rssi_max;

		PHYDM_DBG(dm, DBG_DIG, "Force Dyn-Min=0x%x, RSSI_max=0x%x\n",
			  dig_t->rx_gain_range_min, dig_t->ant_div_rssi_max);
	}

	PHYDM_DBG(dm, DBG_DIG, "Dyn{Max, Min}={0x%x, 0x%x}\n",
		  dig_t->rx_gain_range_max, dig_t->rx_gain_range_min);
}

void phydm_dig_abnormal_case(struct dm_struct *dm)
{
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	/* Abnormal lower bound case */
	if (dig_t->rx_gain_range_min > dig_t->rx_gain_range_max)
		dig_t->rx_gain_range_min = dig_t->rx_gain_range_max;

	PHYDM_DBG(dm, DBG_DIG, "Abnoraml checked {Max, Min}={0x%x, 0x%x}\n",
		  dig_t->rx_gain_range_max, dig_t->rx_gain_range_min);
}

u8 phydm_new_igi_by_fa(
	struct dm_struct *dm,
	u8 igi,
	u32 fa_cnt,
	u8 *step_size)
{
	boolean dig_go_up_check = true;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	/*dig_go_up_check = phydm_dig_go_up_check(dm);*/
	PHYDM_DBG(dm, DBG_DIG, "Total fa_cnt = %d\n", fa_cnt);

	if (fa_cnt > dig_t->fa_th[2] && dig_go_up_check)
		igi = igi + step_size[0];
	else if ((fa_cnt > dig_t->fa_th[1]) && dig_go_up_check)
		igi = igi + step_size[1];
	else if (fa_cnt < dig_t->fa_th[0])
		igi = igi - step_size[2];

	return igi;
}

u8 phydm_get_new_igi(
	struct dm_struct *dm,
	u8 igi,
	u32 fa_cnt,
	boolean is_dfs_band)
{
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	u8 step[3] = {0};
	boolean first_connect = false, first_dis_connect = false;

	first_connect = (dm->is_linked) && !dig_t->is_media_connect;
	first_dis_connect = (!dm->is_linked) && dig_t->is_media_connect;

	if (dm->is_linked) {
		if (dm->pre_rssi_min <= dm->rssi_min) {
			PHYDM_DBG(dm, DBG_DIG, "pre_rssi_min <= rssi_min\n");
			step[0] = 2;
			step[1] = 1;
			step[2] = 2;
		} else {
			step[0] = 4;
			step[1] = 2;
			step[2] = 2;
		}
	} else {
		step[0] = 2;
		step[1] = 1;
		step[2] = 2;
	}

	PHYDM_DBG(dm, DBG_DIG, "step = {-%d, +%d, +%d}\n", step[2], step[1],
		  step[0]);

	if (first_connect) {
		if (is_dfs_band) {
			if (dm->rssi_min > DIG_MAX_DFS)
				igi = DIG_MAX_DFS;
			else
				igi = dm->rssi_min;
			PHYDM_DBG(dm, DBG_DIG, "DFS band:IgiMax=0x%x\n",
				  dig_t->rx_gain_range_max);
		} else {
			igi = dig_t->rx_gain_range_min;
		}

		#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		#if (RTL8812A_SUPPORT == 1)
		if (dm->support_ic_type == ODM_RTL8812)
			odm_config_bb_with_header_file(dm, CONFIG_BB_AGC_TAB_DIFF);
		#endif
		#endif
		PHYDM_DBG(dm, DBG_DIG, "First connect: foce IGI=0x%x\n", igi);
	} else if (dm->is_linked) {
		PHYDM_DBG(dm, DBG_DIG, "Adjust IGI @ linked\n");
		/* 4 Abnormal # beacon case */
		#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		if (dm->phy_dbg_info.num_qry_beacon_pkt < 5 &&
		    fa_cnt < DM_DIG_FA_TH1 && dm->bsta_state &&
		    dm->support_ic_type != ODM_RTL8723D) {
			dig_t->rx_gain_range_min = 0x1c;
			igi = dig_t->rx_gain_range_min;
			PHYDM_DBG(dm, DBG_DIG, "Beacon_num=%d,force igi=0x%x\n",
				  dm->phy_dbg_info.num_qry_beacon_pkt, igi);
		} else {
			igi = phydm_new_igi_by_fa(dm, igi, fa_cnt, step);
		}
		#else
		igi = phydm_new_igi_by_fa(dm, igi, fa_cnt, step);
		#endif
	} else {
		/* 2 Before link */
		PHYDM_DBG(dm, DBG_DIG, "Adjust IGI before link\n");

		if (first_dis_connect) {
			igi = dig_t->dm_dig_min;
			PHYDM_DBG(dm, DBG_DIG,
				  "First disconnect:foce IGI to lower bound\n");
		} else {
			PHYDM_DBG(dm, DBG_DIG, "Pre_IGI=((0x%x)), FA=((%d))\n",
				  igi, fa_cnt);

			igi = phydm_new_igi_by_fa(dm, igi, fa_cnt, step);
		}
	}

	/*Check IGI by dyn-upper/lower bound */
	if (igi < dig_t->rx_gain_range_min)
		igi = dig_t->rx_gain_range_min;

	if (igi > dig_t->rx_gain_range_max)
		igi = dig_t->rx_gain_range_max;

	PHYDM_DBG(dm, DBG_DIG, "New_IGI=((0x%x))\n", igi);

	return igi;
}

void phydm_dig(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_fa_struct *falm_cnt = &dm->false_alm_cnt;
#ifdef PHYDM_TDMA_DIG_SUPPORT
	struct phydm_fa_acc_struct *falm_cnt_acc = &dm->false_alm_cnt_acc;
#endif
	boolean first_connect, first_disconnect;
	u8 igi = dig_t->cur_ig_value;
	u8 new_igi = 0x20;
	u32 fa_cnt = falm_cnt->cnt_all;
	boolean is_dfs_band = false, is_performance = true;

#ifdef PHYDM_TDMA_DIG_SUPPORT
	if (dm->original_dig_restore == 0) {
		if (dig_t->cur_ig_value_tdma == 0)
			dig_t->cur_ig_value_tdma = dig_t->cur_ig_value;

		igi = dig_t->cur_ig_value_tdma;
		fa_cnt = falm_cnt_acc->cnt_all_1sec;
	}
#endif

	if (odm_dig_abort(dm) == true) {
		dig_t->cur_ig_value = (u8)odm_get_bb_reg(dm, R_0xc50, 0x7f);
		return;
	}

	PHYDM_DBG(dm, DBG_DIG, "%s Start===>\n", __func__);

	/* 1 Update status */
	first_connect = (dm->is_linked) && !dig_t->is_media_connect;
	first_disconnect = (!dm->is_linked) && dig_t->is_media_connect;

	PHYDM_DBG(dm, DBG_DIG,
		  "is_linked=%d, RSSI=%d, 1stConnect=%d, 1stDisconnect=%d\n",
		  dm->is_linked, dm->rssi_min, first_connect, first_disconnect);

#if (DM_ODM_SUPPORT_TYPE & (ODM_AP | ODM_CE))
	/* Modify lower bound for DFS band */
	if (dm->is_dfs_band) {
		#if (DM_ODM_SUPPORT_TYPE & (ODM_CE))
		if (phydm_dfs_master_enabled(dm))
		#endif
			is_dfs_band = true;

		PHYDM_DBG(dm, DBG_DIG, "In DFS band\n");
	}
#endif

	PHYDM_DBG(dm, DBG_DIG, "DIG ((%s)) mode\n",
		  (*dm->bb_op_mode ? "Balance" : "Performance"));

	/*Record IGI History*/
	phydm_dig_recorder(dm, first_connect, igi, fa_cnt);

	/*Absolute Boundary Decision */
	phydm_dig_abs_boundary_decision(dm, is_dfs_band);

	/*Dynamic Boundary Decision*/
	phydm_dig_dym_boundary_decision(dm);

	/*Abnormal case check*/
	phydm_dig_abnormal_case(dm);

	/*FA threshold decision */
	odm_fa_threshold_check(dm, is_dfs_band);

	/*Select new IGI by FA */
	new_igi = phydm_get_new_igi(dm, igi, fa_cnt, is_dfs_band);


	/* 1 Update status */
	#ifdef PHYDM_TDMA_DIG_SUPPORT
	if (dm->original_dig_restore == 0) {
		dig_t->cur_ig_value_tdma = new_igi;
		/*It is possible fa_acc_1sec_tsf >= */
		/*1sec while tdma_dig_state == 0*/
		if (dig_t->tdma_dig_state != 0)
			odm_write_dig(dm, dig_t->cur_ig_value_tdma);
	} else
	#endif
		odm_write_dig(dm, new_igi);

	dig_t->is_media_connect = dm->is_linked;
}

void phydm_dig_lps_32k(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 current_igi = dm->rssi_min;

	odm_write_dig(dm, current_igi);
}

void phydm_dig_by_rssi_lps(void *dm_void)
{
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *falm_cnt;

	u8 rssi_lower = DIG_MIN_LPS; /* 0x1E or 0x1C */
	u8 current_igi = dm->rssi_min;

	falm_cnt = &dm->false_alm_cnt;
	if (odm_dig_abort(dm) == true)
		return;

	current_igi = current_igi + RSSI_OFFSET_DIG_LPS;
	PHYDM_DBG(dm, DBG_DIG, "%s==>\n", __func__);

	/* Using FW PS mode to make IGI */
	/* Adjust by  FA in LPS MODE */
	if (falm_cnt->cnt_all > DM_DIG_FA_TH2_LPS)
		current_igi = current_igi + 4;
	else if (falm_cnt->cnt_all > DM_DIG_FA_TH1_LPS)
		current_igi = current_igi + 2;
	else if (falm_cnt->cnt_all < DM_DIG_FA_TH0_LPS)
		current_igi = current_igi - 2;

	/* Lower bound checking */

	/* RSSI Lower bound check */
	if ((dm->rssi_min - 10) > DIG_MIN_LPS)
		rssi_lower = (dm->rssi_min - 10);
	else
		rssi_lower = DIG_MIN_LPS;

	/* Upper and Lower Bound checking */
	if (current_igi > DIG_MAX_LPS)
		current_igi = DIG_MAX_LPS;
	else if (current_igi < rssi_lower)
		current_igi = rssi_lower;

	PHYDM_DBG(dm, DBG_DIG, "fa_cnt_all=%d, rssi_min=%d, curr_igi=0x%x\n",
		  falm_cnt->cnt_all, dm->rssi_min, current_igi);
	odm_write_dig(dm, current_igi);
#endif
}

/* 3============================================================
 * 3 FASLE ALARM CHECK
 * 3============================================================
 */
void phydm_false_alarm_counter_reg_reset(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *falm_cnt = &dm->false_alm_cnt;
#ifdef PHYDM_TDMA_DIG_SUPPORT
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_fa_acc_struct *falm_cnt_acc = &dm->false_alm_cnt_acc;
#endif
	u32 false_alm_cnt;

#ifdef PHYDM_TDMA_DIG_SUPPORT
	if (dm->original_dig_restore == 0) {
		if (dig_t->cur_ig_value_tdma == 0)
			dig_t->cur_ig_value_tdma = dig_t->cur_ig_value;

		false_alm_cnt = falm_cnt_acc->cnt_all_1sec;
	} else
#endif
	{
		false_alm_cnt = falm_cnt->cnt_all;
	}

#if (ODM_IC_11N_SERIES_SUPPORT == 1)
	if (dm->support_ic_type & ODM_IC_11AC_3_SERIES) {
		/* reset CCK FA counter */
		odm_set_bb_reg(dm, R_0x1a2c, BIT(15) | BIT(14), 0);
		odm_set_bb_reg(dm, R_0x1a2c, BIT(15) | BIT(14), 2);

		/* reset CCK CCA counter */
		odm_set_bb_reg(dm, R_0x1a2c, BIT(13) | BIT(12), 0);
		odm_set_bb_reg(dm, R_0x1a2c, BIT(13) | BIT(12), 2);

		/* reset CCA counter, OFDM FA counter*/
		odm_set_bb_reg(dm, R_0x1eb4, BIT(25), 1);
		odm_set_bb_reg(dm, R_0x1eb4, BIT(25), 0);
	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		/*reset false alarm counter registers*/
		odm_set_bb_reg(dm, R_0xc0c, BIT(31), 1);
		odm_set_bb_reg(dm, R_0xc0c, BIT(31), 0);
		odm_set_bb_reg(dm, R_0xd00, BIT(27), 1);
		odm_set_bb_reg(dm, R_0xd00, BIT(27), 0);

		/*update ofdm counter*/
		/*update page C counter*/
		odm_set_bb_reg(dm, R_0xd00, BIT(31), 0);
		/*update page D counter*/
		odm_set_bb_reg(dm, R_0xd00, BIT(31), 0);

		/*reset CCK CCA counter*/
		odm_set_bb_reg(dm, R_0xa2c, BIT(13) | BIT(12), 0);
		odm_set_bb_reg(dm, R_0xa2c, BIT(13) | BIT(12), 2);

		/*reset CCK FA counter*/
		odm_set_bb_reg(dm, R_0xa2c, BIT(15) | BIT(14), 0);
		odm_set_bb_reg(dm, R_0xa2c, BIT(15) | BIT(14), 2);

		/*reset CRC32 counter*/
		odm_set_bb_reg(dm, R_0xf14, BIT(16), 1);
		odm_set_bb_reg(dm, R_0xf14, BIT(16), 0);
	}
#endif /* #if (ODM_IC_11N_SERIES_SUPPORT == 1) */

#if (ODM_IC_11AC_SERIES_SUPPORT == 1)
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		#if (RTL8881A_SUPPORT == 1)
		/* Reset FA counter by enable/disable OFDM */
		if ((dm->support_ic_type == ODM_RTL8881A) &&
		    false_alm_cnt->cnt_ofdm_fail_pre >= 0x7fff) {
			/* reset OFDM */
			odm_set_bb_reg(dm, R_0x808, BIT(29), 0);
			odm_set_bb_reg(dm, R_0x808, BIT(29), 1);
			false_alm_cnt->cnt_ofdm_fail_pre = 0;
			PHYDM_DBG(dm, DBG_FA_CNT, "Reset FA_cnt\n");
		}
		#endif /* #if (RTL8881A_SUPPORT == 1) */

		/* reset OFDM FA countner */
		odm_set_bb_reg(dm, R_0x9a4, BIT(17), 1);
		odm_set_bb_reg(dm, R_0x9a4, BIT(17), 0);

		/* reset CCK FA counter */
		odm_set_bb_reg(dm, R_0xa2c, BIT(15), 0);
		odm_set_bb_reg(dm, R_0xa2c, BIT(15), 1);

		/* reset CCA counter */
		odm_set_bb_reg(dm, R_0xb58, BIT(0), 1);
		odm_set_bb_reg(dm, R_0xb58, BIT(0), 0);
	}
#endif /* #if (ODM_IC_11AC_SERIES_SUPPORT == 1) */
}

void phydm_false_alarm_counter_reg_hold(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	if (dm->support_ic_type & ODM_IC_11AC_3_SERIES) {
		/* hold cck counter */
		odm_set_bb_reg(dm, R_0x1a2c, BIT(12), 1);
		odm_set_bb_reg(dm, R_0x1a2c, BIT(14), 1);
#if 0
		panic_printk("98F 11AC32~~~~~~~~~~~~~~~~~~~~\n");
#endif
	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		/*hold ofdm counter*/
		/*hold page C counter*/
		odm_set_bb_reg(dm, R_0xc00, BIT(31), 1);
		/*hold page D counter*/
		odm_set_bb_reg(dm, R_0xd00, BIT(31), 1);

		/*hold cck counter*/
		odm_set_bb_reg(dm, R_0xa2c, BIT(12), 1);
		odm_set_bb_reg(dm, R_0xa2c, BIT(14), 1);
	}
}

#if (ODM_IC_11N_SERIES_SUPPORT == 1)
void phydm_fa_cnt_statistics_n(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;
	u32 reg = 0;

	if (!(dm->support_ic_type & ODM_IC_11N_SERIES))
		return;

	/* hold ofdm & cck counter */
	phydm_false_alarm_counter_reg_hold(dm);

	reg = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE1_11N, MASKDWORD);
	fa_t->cnt_fast_fsync = (reg & 0xffff);
	fa_t->cnt_sb_search_fail = ((reg & 0xffff0000) >> 16);

	reg = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE2_11N, MASKDWORD);
	fa_t->cnt_ofdm_cca = (reg & 0xffff);
	fa_t->cnt_parity_fail = ((reg & 0xffff0000) >> 16);

	reg = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE3_11N, MASKDWORD);
	fa_t->cnt_rate_illegal = (reg & 0xffff);
	fa_t->cnt_crc8_fail = ((reg & 0xffff0000) >> 16);

	reg = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE4_11N, MASKDWORD);
	fa_t->cnt_mcs_fail = (reg & 0xffff);

	fa_t->cnt_ofdm_fail =
		fa_t->cnt_parity_fail + fa_t->cnt_rate_illegal +
		fa_t->cnt_crc8_fail + fa_t->cnt_mcs_fail +
		fa_t->cnt_fast_fsync + fa_t->cnt_sb_search_fail;

	/* read CCK CRC32 counter */
	fa_t->cnt_cck_crc32_error = odm_get_bb_reg(dm, R_0xf84, MASKDWORD);
	fa_t->cnt_cck_crc32_ok = odm_get_bb_reg(dm, R_0xf88, MASKDWORD);

	/* read OFDM CRC32 counter */
	reg = odm_get_bb_reg(dm, ODM_REG_OFDM_CRC32_CNT_11N, MASKDWORD);
	fa_t->cnt_ofdm_crc32_error = (reg & 0xffff0000) >> 16;
	fa_t->cnt_ofdm_crc32_ok = reg & 0xffff;

	/* read HT CRC32 counter */
	reg = odm_get_bb_reg(dm, ODM_REG_HT_CRC32_CNT_11N, MASKDWORD);
	fa_t->cnt_ht_crc32_error = (reg & 0xffff0000) >> 16;
	fa_t->cnt_ht_crc32_ok = reg & 0xffff;

	/* read VHT CRC32 counter */
	fa_t->cnt_vht_crc32_error = 0;
	fa_t->cnt_vht_crc32_ok = 0;

	#if (RTL8723D_SUPPORT == 1)
	if (dm->support_ic_type == ODM_RTL8723D) {
		/* read HT CRC32 agg counter */
		reg = odm_get_bb_reg(dm, R_0xfb8, MASKDWORD);
		fa_t->cnt_ht_crc32_error_agg = (reg & 0xffff0000) >> 16;
		fa_t->cnt_ht_crc32_ok_agg = reg & 0xffff;
	}
	#endif

	#if (RTL8188E_SUPPORT == 1)
	if (dm->support_ic_type == ODM_RTL8188E) {
		reg = odm_get_bb_reg(dm, ODM_REG_SC_CNT_11N, MASKDWORD);
		fa_t->cnt_bw_lsc = (reg & 0xffff);
		fa_t->cnt_bw_usc = ((reg & 0xffff0000) >> 16);
	}
	#endif

	reg = odm_get_bb_reg(dm, ODM_REG_CCK_FA_LSB_11N, MASKBYTE0);
	fa_t->cnt_cck_fail = reg;

	reg = odm_get_bb_reg(dm, ODM_REG_CCK_FA_MSB_11N, MASKBYTE3);
	fa_t->cnt_cck_fail += (reg & 0xff) << 8;

	reg = odm_get_bb_reg(dm, ODM_REG_CCK_CCA_CNT_11N, MASKDWORD);
	fa_t->cnt_cck_cca = ((reg & 0xFF) << 8) | ((reg & 0xFF00) >> 8);

	fa_t->cnt_all_pre = fa_t->cnt_all;

	fa_t->cnt_all = fa_t->cnt_fast_fsync +
			fa_t->cnt_sb_search_fail +
			fa_t->cnt_parity_fail +
			fa_t->cnt_rate_illegal +
			fa_t->cnt_crc8_fail +
			fa_t->cnt_mcs_fail +
			fa_t->cnt_cck_fail;

	fa_t->cnt_cca_all = fa_t->cnt_ofdm_cca + fa_t->cnt_cck_cca;

	PHYDM_DBG(dm, DBG_FA_CNT,
		  "[OFDM FA Detail] Parity_Fail=((%d)), Rate_Illegal=((%d)), CRC8_fail=((%d)), Mcs_fail=((%d)), Fast_Fsync=(( %d )), SBD_fail=((%d))\n",
		  fa_t->cnt_parity_fail, fa_t->cnt_rate_illegal,
		  fa_t->cnt_crc8_fail, fa_t->cnt_mcs_fail, fa_t->cnt_fast_fsync,
		  fa_t->cnt_sb_search_fail);
}
#endif

#if (ODM_IC_11AC_SERIES_SUPPORT == 1)
void phydm_fa_cnt_statistics_ac(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;
	u32 ret_value = 0;
	u32 cck_enable;

	if (!(dm->support_ic_type & ODM_IC_11AC_SERIES))
		return;

	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE1_11AC, MASKDWORD);
	fa_t->cnt_fast_fsync = ((ret_value & 0xffff0000) >> 16);

	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE2_11AC, MASKDWORD);
	fa_t->cnt_sb_search_fail = (ret_value & 0xffff);

	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE3_11AC, MASKDWORD);
	fa_t->cnt_parity_fail = (ret_value & 0xffff);
	fa_t->cnt_rate_illegal = ((ret_value & 0xffff0000) >> 16);

	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE4_11AC, MASKDWORD);
	fa_t->cnt_crc8_fail = (ret_value & 0xffff);
	fa_t->cnt_mcs_fail = ((ret_value & 0xffff0000) >> 16);

	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE5_11AC, MASKDWORD);
	fa_t->cnt_crc8_fail_vht = (ret_value & 0xffff);

	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_FA_TYPE6_11AC, MASKDWORD);
	fa_t->cnt_mcs_fail_vht = (ret_value & 0xffff);

	/* read OFDM FA counter */
	fa_t->cnt_ofdm_fail = odm_get_bb_reg(dm, R_0xf48, MASKLWORD);

	/* Read CCK FA counter */
	fa_t->cnt_cck_fail = odm_get_bb_reg(dm, ODM_REG_CCK_FA_11AC, MASKLWORD);

	/* read CCK/OFDM CCA counter */
	ret_value = odm_get_bb_reg(dm, ODM_REG_CCK_CCA_CNT_11AC, MASKDWORD);
	fa_t->cnt_ofdm_cca = (ret_value & 0xffff0000) >> 16;
	fa_t->cnt_cck_cca = ret_value & 0xffff;

	/* read CCK CRC32 counter */
	ret_value = odm_get_bb_reg(dm, ODM_REG_CCK_CRC32_CNT_11AC, MASKDWORD);
	fa_t->cnt_cck_crc32_error = (ret_value & 0xffff0000) >> 16;
	fa_t->cnt_cck_crc32_ok = ret_value & 0xffff;

	/* read OFDM CRC32 counter */
	ret_value = odm_get_bb_reg(dm, ODM_REG_OFDM_CRC32_CNT_11AC, MASKDWORD);
	fa_t->cnt_ofdm_crc32_error = (ret_value & 0xffff0000) >> 16;
	fa_t->cnt_ofdm_crc32_ok = ret_value & 0xffff;

	/* read HT CRC32 counter */
	ret_value = odm_get_bb_reg(dm, ODM_REG_HT_CRC32_CNT_11AC, MASKDWORD);
	fa_t->cnt_ht_crc32_error = (ret_value & 0xffff0000) >> 16;
	fa_t->cnt_ht_crc32_ok = ret_value & 0xffff;

	/* read VHT CRC32 counter */
	ret_value = odm_get_bb_reg(dm, ODM_REG_VHT_CRC32_CNT_11AC, MASKDWORD);
	fa_t->cnt_vht_crc32_error = (ret_value & 0xffff0000) >> 16;
	fa_t->cnt_vht_crc32_ok = ret_value & 0xffff;

	#if (RTL8881A_SUPPORT == 1)
	if (dm->support_ic_type == ODM_RTL8881A) {
		u32 tmp = 0;

		if (fa_t->cnt_ofdm_fail >= fa_t->cnt_ofdm_fail_pre) {
			tmp = fa_t->cnt_ofdm_fail_pre;
			fa_t->cnt_ofdm_fail_pre = fa_t->cnt_ofdm_fail;
			fa_t->cnt_ofdm_fail = fa_t->cnt_ofdm_fail - tmp;
		} else {
			fa_t->cnt_ofdm_fail_pre = fa_t->cnt_ofdm_fail;
		}

		PHYDM_DBG(dm, DBG_FA_CNT,
			  "[8881]cnt_ofdm_fail{curr,pre}={%d,%d}\n",
			  fa_t->cnt_ofdm_fail_pre, tmp);
	}
	#endif

	cck_enable = odm_get_bb_reg(dm, ODM_REG_BB_RX_PATH_11AC, BIT(28));

	if (cck_enable) { /* if(*dm->band_type == ODM_BAND_2_4G) */
		fa_t->cnt_all = fa_t->cnt_ofdm_fail + fa_t->cnt_cck_fail;
		fa_t->cnt_cca_all = fa_t->cnt_cck_cca + fa_t->cnt_ofdm_cca;
	} else {
		fa_t->cnt_all = fa_t->cnt_ofdm_fail;
		fa_t->cnt_cca_all = fa_t->cnt_ofdm_cca;
	}
}
#endif

void phydm_get_dbg_port_info(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;
	u32 dbg_port = dm->adaptivity.adaptivity_dbg_port;
	u32 val = 0;

	/*set debug port to 0x0*/
	if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_1, 0x0)) {
		fa_t->dbg_port0 = phydm_get_bb_dbg_port_val(dm);
		phydm_release_bb_dbg_port(dm);
	}

	if (dm->support_ic_type == ODM_RTL8723D) {
		val = odm_get_bb_reg(dm, R_0x9a0, BIT(29));
	} else if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_1, dbg_port)) {
		if (dm->support_ic_type & (ODM_RTL8723B | ODM_RTL8188E))
			val = (phydm_get_bb_dbg_port_val(dm) & BIT(30)) >> 30;
		else
			val = (phydm_get_bb_dbg_port_val(dm) & BIT(29)) >> 29;
		phydm_release_bb_dbg_port(dm);
	}

	fa_t->edcca_flag = (boolean)val;

	PHYDM_DBG(dm, DBG_FA_CNT, "FA_Cnt: Dbg port 0x0 = 0x%x, EDCCA = %d\n\n",
		  fa_t->dbg_port0, fa_t->edcca_flag);
}

void odm_false_alarm_counter_statistics(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;

	if (!(dm->support_ability & ODM_BB_FA_CNT))
		return;

	PHYDM_DBG(dm, DBG_FA_CNT, "[%s]======>\n", __func__);

	if (dm->support_ic_type & ODM_IC_11AC_3_SERIES) {
		#ifdef PHYDM_IC_11AC_3_SERIES_SUPPORT
		phydm_fa_cnt_statistics_ac3(dm);
		#endif
	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		#if (ODM_IC_11N_SERIES_SUPPORT == 1)
		phydm_fa_cnt_statistics_n(dm);
		#endif
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		#if (ODM_IC_11AC_SERIES_SUPPORT == 1)
		phydm_fa_cnt_statistics_ac(dm);
		#endif
	}

	phydm_get_dbg_port_info(dm);
	phydm_false_alarm_counter_reg_reset(dm_void);

	fa_t->time_fa_all = fa_t->cnt_fast_fsync * 12 +
			    fa_t->cnt_sb_search_fail * 12 +
			    fa_t->cnt_parity_fail * 28 +
			    fa_t->cnt_rate_illegal * 28 +
			    fa_t->cnt_crc8_fail * 36 +
			    fa_t->cnt_crc8_fail_vht * 36 +
			    fa_t->cnt_mcs_fail_vht * 36 +
			    fa_t->cnt_mcs_fail * 32 +
			    fa_t->cnt_cck_fail * 80;

	fa_t->cnt_crc32_error_all = fa_t->cnt_vht_crc32_error +
				    fa_t->cnt_ht_crc32_error +
				    fa_t->cnt_ofdm_crc32_error +
				    fa_t->cnt_cck_crc32_error;

	fa_t->cnt_crc32_ok_all = fa_t->cnt_vht_crc32_ok +
				 fa_t->cnt_ht_crc32_ok +
				 fa_t->cnt_ofdm_crc32_ok +
				 fa_t->cnt_cck_crc32_ok;

	PHYDM_DBG(dm, DBG_FA_CNT,
		  "[OFDM FA Detail-1] Parity=((%d)), Rate_Illegal=((%d)), HT_CRC8=((%d)), HT_MCS=((%d))\n",
		  fa_t->cnt_parity_fail, fa_t->cnt_rate_illegal,
		  fa_t->cnt_crc8_fail, fa_t->cnt_mcs_fail);
	PHYDM_DBG(dm, DBG_FA_CNT,
		  "[OFDM FA Detail-2] Fast_Fsync=((%d)), SBD=((%d)), VHT_CRC8=((%d)), VHT_MCS=((%d))\n",
		  fa_t->cnt_fast_fsync, fa_t->cnt_sb_search_fail,
		  fa_t->cnt_crc8_fail_vht, fa_t->cnt_mcs_fail_vht);
	PHYDM_DBG(dm, DBG_FA_CNT,
		  "[CCA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}\n",
		  fa_t->cnt_cck_cca, fa_t->cnt_ofdm_cca, fa_t->cnt_cca_all);
	PHYDM_DBG(dm, DBG_FA_CNT,
		  "[FA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}\n",
		  fa_t->cnt_cck_fail, fa_t->cnt_ofdm_fail, fa_t->cnt_all);
	PHYDM_DBG(dm, DBG_FA_CNT, "[CCK]  CRC32 {error, ok}= {%d, %d}\n",
		  fa_t->cnt_cck_crc32_error, fa_t->cnt_cck_crc32_ok);
	PHYDM_DBG(dm, DBG_FA_CNT, "[OFDM]CRC32 {error, ok}= {%d, %d}\n",
		  fa_t->cnt_ofdm_crc32_error, fa_t->cnt_ofdm_crc32_ok);
	PHYDM_DBG(dm, DBG_FA_CNT, "[ HT ]  CRC32 {error, ok}= {%d, %d}\n",
		  fa_t->cnt_ht_crc32_error, fa_t->cnt_ht_crc32_ok);
	PHYDM_DBG(dm, DBG_FA_CNT, "[VHT]  CRC32 {error, ok}= {%d, %d}\n",
		  fa_t->cnt_vht_crc32_error, fa_t->cnt_vht_crc32_ok);
	PHYDM_DBG(dm, DBG_FA_CNT, "[TOTAL]  CRC32 {error, ok}= {%d, %d}\n",
		  fa_t->cnt_crc32_error_all, fa_t->cnt_crc32_ok_all);
}

#ifdef PHYDM_TDMA_DIG_SUPPORT
void phydm_set_tdma_dig_timer(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 delta_time_us = dm->tdma_dig_timer_ms * 1000;
	struct phydm_dig_struct *dig_t;
	u32 timeout;
	u32 current_time_stamp, diff_time_stamp, regb0;

	dig_t = &dm->dm_dig_table;
	/*some IC has no FREERUN_CUNT register, like 92E*/
	if (dm->support_ic_type & ODM_RTL8197F)
		current_time_stamp = odm_get_bb_reg(dm, R_0x568, bMaskDWord);
	else
		return;

	timeout = current_time_stamp + delta_time_us;

	diff_time_stamp = current_time_stamp - dig_t->cur_timestamp;
	dig_t->pre_timestamp = dig_t->cur_timestamp;
	dig_t->cur_timestamp = current_time_stamp;

	/*HIMR0, it shows HW interrupt mask*/
	regb0 = odm_get_bb_reg(dm, R_0xb0, bMaskDWord);

	PHYDM_DBG(dm, DBG_DIG, "Set next timer\n");
	PHYDM_DBG(dm, DBG_DIG,
		  "curr_time_stamp=%d, delta_time_us=%d, timeout=%d, diff_time_stamp=%d, Reg0xb0 = 0x%x\n",
		  current_time_stamp, delta_time_us, timeout, diff_time_stamp,
		  regb0);

	if (dm->support_ic_type & ODM_RTL8197F) /*REG_PS_TIMER2*/
		odm_set_bb_reg(dm, R_0x588, bMaskDWord, timeout);
	else {
		PHYDM_DBG(dm, DBG_DIG, "NOT 97F, NOT start\n");
		return;
	}
}

void phydm_tdma_dig_timer_check(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t;

	dig_t = &dm->dm_dig_table;

	PHYDM_DBG(dm, DBG_DIG, "tdma_dig_cnt=%d, pre_tdma_dig_cnt=%d\n",
		  dig_t->tdma_dig_cnt, dig_t->pre_tdma_dig_cnt);

	if (dig_t->tdma_dig_cnt == 0 ||
	    dig_t->tdma_dig_cnt == dig_t->pre_tdma_dig_cnt) {
		if (dm->support_ability & ODM_BB_DIG) {
			/*if interrupt mask info is got.*/
			/*Reg0xb0 is no longer needed*/
			/*regb0 = odm_get_bb_reg(dm, R_0xb0, bMaskDWord);*/
			PHYDM_DBG(dm, DBG_DIG,
				  "Check fail, Mask[0]=0x%x, restart timer\n",
				  *dm->interrupt_mask);

			phydm_tdma_dig_add_interrupt_mask_handler(dm);
			phydm_enable_rx_related_interrupt_handler(dm);
			phydm_set_tdma_dig_timer(dm);
		}
	} else {
		PHYDM_DBG(dm, DBG_DIG, "Check pass, update pre_tdma_dig_cnt\n");
	}

	dig_t->pre_tdma_dig_cnt = dig_t->tdma_dig_cnt;
}

/*different IC/team may use different timer for tdma-dig*/
void phydm_tdma_dig_add_interrupt_mask_handler(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

#if (DM_ODM_SUPPORT_TYPE == (ODM_AP))
	if (dm->support_ic_type & ODM_RTL8197F) {
		/*HAL_INT_TYPE_PSTIMEOUT2*/
		phydm_add_interrupt_mask_handler(dm, HAL_INT_TYPE_PSTIMEOUT2);
	}
#elif (DM_ODM_SUPPORT_TYPE == (ODM_WIN))
#elif (DM_ODM_SUPPORT_TYPE == (ODM_CE))
#endif
}

void phydm_tdma_dig(
	void *dm_void)
{
	struct dm_struct *dm;
	struct phydm_dig_struct *dig_t;
	struct phydm_fa_struct *falm_cnt;
	u32 reg_c50;

	dm = (struct dm_struct *)dm_void;
	dig_t = &dm->dm_dig_table;
	falm_cnt = &dm->false_alm_cnt;
	reg_c50 = odm_get_bb_reg(dm, R_0xc50, MASKBYTE0);

	dig_t->tdma_dig_state =
		dig_t->tdma_dig_cnt % dm->tdma_dig_state_number;

	PHYDM_DBG(dm, DBG_DIG, "tdma_dig_state=%d, regc50=0x%x\n",
		  dig_t->tdma_dig_state, reg_c50);

	dig_t->tdma_dig_cnt++;

	if (dig_t->tdma_dig_state == 1) {
		// update IGI from tdma_dig_state == 0
		if (dig_t->cur_ig_value_tdma == 0)
			dig_t->cur_ig_value_tdma = dig_t->cur_ig_value;

		odm_write_dig(dm, dig_t->cur_ig_value_tdma);
		phydm_tdma_false_alarm_counter_check(dm);
		PHYDM_DBG(dm, DBG_DIG, "tdma_dig_state=%d, reset FA counter\n",
			  dig_t->tdma_dig_state);

	} else if (dig_t->tdma_dig_state == 0) {
		/* update dig_t->CurIGValue,*/
		/* it may different from dig_t->cur_ig_value_tdma */
		/* TDMA IGI upperbond @ L-state = */
		/* rf_ft_var.tdma_dig_low_upper_bond = 0x26 */

		if (dig_t->cur_ig_value >= dm->tdma_dig_low_upper_bond)
			dig_t->low_ig_value = dm->tdma_dig_low_upper_bond;
		else
			dig_t->low_ig_value = dig_t->cur_ig_value;

		odm_write_dig(dm, dig_t->low_ig_value);
		phydm_tdma_false_alarm_counter_check(dm);
	} else
		phydm_tdma_false_alarm_counter_check(dm);
}

/*============================================================*/
/*FASLE ALARM CHECK*/
/*============================================================*/

void phydm_tdma_false_alarm_counter_check(
	void *dm_void)
{
	struct dm_struct *dm;
	struct phydm_fa_struct *falm_cnt;
	struct phydm_fa_acc_struct *falm_cnt_acc;
	struct phydm_dig_struct *dig_t;
	boolean rssi_dump_en = 0;
	u32 timestamp;
	u8 tdma_dig_state_number;
	u32 start_th = 0;

	dm = (struct dm_struct *)dm_void;
	falm_cnt = &dm->false_alm_cnt;
	falm_cnt_acc = &dm->false_alm_cnt_acc;
	dig_t = &dm->dm_dig_table;

	if (dig_t->tdma_dig_state == 1)
		phydm_false_alarm_counter_reset(dm);
	/* Reset FalseAlarmCounterStatistics */
	/* fa_acc_1sec_tsf = fa_acc_1sec_tsf, keep */
	/* fa_end_tsf = fa_start_tsf = TSF */
	else {
		odm_false_alarm_counter_statistics(dm);
		if (dm->support_ic_type & ODM_RTL8197F) /*REG_FREERUN_CNT*/
			timestamp = odm_get_bb_reg(dm, R_0x568, bMaskDWord);
		else {
			PHYDM_DBG(dm, DBG_DIG, "NOT 97F! NOT start\n");
			return;
		}
		dig_t->fa_end_timestamp = timestamp;
		dig_t->fa_acc_1sec_timestamp +=
			(dig_t->fa_end_timestamp - dig_t->fa_start_timestamp);

		/*prevent dumb*/
		if (dm->tdma_dig_state_number == 1)
			dm->tdma_dig_state_number = 2;

		tdma_dig_state_number = dm->tdma_dig_state_number;
		dig_t->sec_factor =
			tdma_dig_state_number / (tdma_dig_state_number - 1);

		/*1sec = 1000000us*/
		if (dig_t->sec_factor)
			start_th = (u32)(1000000 / dig_t->sec_factor);

		if (dig_t->fa_acc_1sec_timestamp >= start_th) {
			rssi_dump_en = 1;
			phydm_false_alarm_counter_acc(dm, rssi_dump_en);
			PHYDM_DBG(dm, DBG_DIG,
				  "sec_factor=%d, total FA=%d, is_linked=%d\n",
				  dig_t->sec_factor, falm_cnt_acc->cnt_all,
				  dm->is_linked);

			phydm_noisy_detection(dm);
			phydm_cck_pd_th(dm);
			phydm_dig(dm);
			phydm_false_alarm_counter_acc_reset(dm);

			/* Reset FalseAlarmCounterStatistics */
			/* fa_end_tsf = fa_start_tsf = TSF, keep */
			/* fa_acc_1sec_tsf = 0 */
			phydm_false_alarm_counter_reset(dm);
		} else {
			phydm_false_alarm_counter_acc(dm, rssi_dump_en);
		}
	}
}

void phydm_false_alarm_counter_acc(
	void *dm_void,
	boolean rssi_dump_en)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *falm_cnt;
	struct phydm_fa_acc_struct *falm_cnt_acc;
	struct phydm_dig_struct *dig_t;

	falm_cnt = &dm->false_alm_cnt;
	falm_cnt_acc = &dm->false_alm_cnt_acc;
	dig_t = &dm->dm_dig_table;

	falm_cnt_acc->cnt_parity_fail += falm_cnt->cnt_parity_fail;
	falm_cnt_acc->cnt_rate_illegal += falm_cnt->cnt_rate_illegal;
	falm_cnt_acc->cnt_crc8_fail += falm_cnt->cnt_crc8_fail;
	falm_cnt_acc->cnt_mcs_fail += falm_cnt->cnt_mcs_fail;
	falm_cnt_acc->cnt_ofdm_fail += falm_cnt->cnt_ofdm_fail;
	falm_cnt_acc->cnt_cck_fail += falm_cnt->cnt_cck_fail;
	falm_cnt_acc->cnt_all += falm_cnt->cnt_all;
	falm_cnt_acc->cnt_fast_fsync += falm_cnt->cnt_fast_fsync;
	falm_cnt_acc->cnt_sb_search_fail += falm_cnt->cnt_sb_search_fail;
	falm_cnt_acc->cnt_ofdm_cca += falm_cnt->cnt_ofdm_cca;
	falm_cnt_acc->cnt_cck_cca += falm_cnt->cnt_cck_cca;
	falm_cnt_acc->cnt_cca_all += falm_cnt->cnt_cca_all;
	falm_cnt_acc->cnt_cck_crc32_error += falm_cnt->cnt_cck_crc32_error;
	falm_cnt_acc->cnt_cck_crc32_ok += falm_cnt->cnt_cck_crc32_ok;
	falm_cnt_acc->cnt_ofdm_crc32_error += falm_cnt->cnt_ofdm_crc32_error;
	falm_cnt_acc->cnt_ofdm_crc32_ok += falm_cnt->cnt_ofdm_crc32_ok;
	falm_cnt_acc->cnt_ht_crc32_error += falm_cnt->cnt_ht_crc32_error;
	falm_cnt_acc->cnt_ht_crc32_ok += falm_cnt->cnt_ht_crc32_ok;
	falm_cnt_acc->cnt_vht_crc32_error += falm_cnt->cnt_vht_crc32_error;
	falm_cnt_acc->cnt_vht_crc32_ok += falm_cnt->cnt_vht_crc32_ok;
	falm_cnt_acc->cnt_crc32_error_all += falm_cnt->cnt_crc32_error_all;
	falm_cnt_acc->cnt_crc32_ok_all += falm_cnt->cnt_crc32_ok_all;

	if (rssi_dump_en == 1) {
		falm_cnt_acc->cnt_all_1sec =
			falm_cnt_acc->cnt_all * dig_t->sec_factor;
		falm_cnt_acc->cnt_cca_all_1sec =
			falm_cnt_acc->cnt_cca_all * dig_t->sec_factor;
		falm_cnt_acc->cnt_cck_fail_1sec =
			falm_cnt_acc->cnt_cck_fail * dig_t->sec_factor;
	}
}

void phydm_false_alarm_counter_acc_reset(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_acc_struct *falm_cnt_acc;

	falm_cnt_acc = &dm->false_alm_cnt_acc;

	/* Cnt_all_for_rssi_dump & Cnt_CCA_all_for_rssi_dump */
	/* do NOT need to be reset */
	odm_memory_set(dm, falm_cnt_acc, 0, sizeof(falm_cnt_acc));
}

void phydm_false_alarm_counter_reset(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *falm_cnt;
	struct phydm_dig_struct *dig_t;
	u32 timestamp;

	falm_cnt = &dm->false_alm_cnt;
	dig_t = &dm->dm_dig_table;

	memset(falm_cnt, 0, sizeof(dm->false_alm_cnt));
	phydm_false_alarm_counter_reg_reset(dm);

	if (dig_t->tdma_dig_state != 1)
		dig_t->fa_acc_1sec_timestamp = 0;
	else
		dig_t->fa_acc_1sec_timestamp = dig_t->fa_acc_1sec_timestamp;

	/*REG_FREERUN_CNT*/
	timestamp = odm_get_bb_reg(dm, R_0x568, bMaskDWord);
	dig_t->fa_start_timestamp = timestamp;
	dig_t->fa_end_timestamp = timestamp;
}

#endif /*#ifdef PHYDM_TDMA_DIG_SUPPORT*/

void phydm_dig_debug(void *dm_void, char input[][16], u32 *_used, char *output,
		     u32 *_out_len, u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	char help[] = "-h";
	char monitor[] = "-m";
	u32 var1[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;

	if ((strcmp(input[1], help) == 0))
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "{0} fa[0] fa[1] fa[2]\n");
	else if ((strcmp(input[1], monitor) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Read DIG fa_th[0:2]= {%d, %d, %d}\n", dig_t->fa_th[0],
			 dig_t->fa_th[1], dig_t->fa_th[2]);

	} else {
		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		for (i = 1; i < 10; i++)
			PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);

		if (var1[0] == 0) {
			dig_t->is_dbg_fa_th = true;
			dig_t->fa_th[0] = (u16)var1[1];
			dig_t->fa_th[1] = (u16)var1[2];
			dig_t->fa_th[2] = (u16)var1[3];

			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Set DIG fa_th[0:2]= {%d, %d, %d}\n",
				 dig_t->fa_th[0], dig_t->fa_th[1],
				 dig_t->fa_th[2]);
		} else {
			dig_t->is_dbg_fa_th = false;
		}
	}
	*_used = used;
	*_out_len = out_len;
}

