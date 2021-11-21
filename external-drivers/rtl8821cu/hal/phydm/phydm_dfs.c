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

/*
 * ============================================================
 * include files
 * ============================================================
 */

#include "mp_precomp.h"
#include "phydm_precomp.h"

#if defined(CONFIG_PHYDM_DFS_MASTER)

boolean phydm_dfs_is_meteorology_channel(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u8 ch = *dm->channel;
	u8 bw = *dm->band_width;

	return ((bw  == CHANNEL_WIDTH_80 && (ch) >= 116 && (ch) <= 128) ||
		(bw  == CHANNEL_WIDTH_40 && (ch) >= 116 && (ch) <= 128) ||
		(bw  == CHANNEL_WIDTH_20 && (ch) >= 120 && (ch) <= 128));
}

void phydm_radar_detect_reset(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	odm_set_bb_reg(dm, R_0x924, BIT(15), 0);
	odm_set_bb_reg(dm, R_0x924, BIT(15), 1);
}

void phydm_radar_detect_disable(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	odm_set_bb_reg(dm, R_0x924, BIT(15), 0);
	PHYDM_DBG(dm, DBG_DFS, "\n");
}

static void phydm_radar_detect_with_dbg_parm(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	odm_set_bb_reg(dm, R_0x918, MASKDWORD, dm->radar_detect_reg_918);
	odm_set_bb_reg(dm, R_0x91c, MASKDWORD, dm->radar_detect_reg_91c);
	odm_set_bb_reg(dm, R_0x920, MASKDWORD, dm->radar_detect_reg_920);
	odm_set_bb_reg(dm, R_0x924, MASKDWORD, dm->radar_detect_reg_924);
}

/* Init radar detection parameters, called after ch, bw is set */

void phydm_radar_detect_enable(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u8 region_domain = dm->dfs_region_domain;
	u8 c_channel = *dm->channel;
	u8 band_width = *dm->band_width;
	u8 enable = 0;

	PHYDM_DBG(dm, DBG_DFS, "test, region_domain = %d\n", region_domain);
	if (region_domain == PHYDM_DFS_DOMAIN_UNKNOWN) {
		PHYDM_DBG(dm, DBG_DFS, "PHYDM_DFS_DOMAIN_UNKNOWN\n");
		goto exit;
	}

	if (dm->support_ic_type & (ODM_RTL8821 | ODM_RTL8812 | ODM_RTL8881A)) {
		odm_set_bb_reg(dm, R_0x814, 0x3fffffff, 0x04cc4d10);
		odm_set_bb_reg(dm, R_0x834, MASKBYTE0, 0x06);

		if (dm->radar_detect_dbg_parm_en) {
			phydm_radar_detect_with_dbg_parm(dm);
			enable = 1;
			goto exit;
		}

		if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
			odm_set_bb_reg(dm, R_0x918, MASKDWORD, 0x1c17ecdf);
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x01528500);
			odm_set_bb_reg(dm, R_0x91c, MASKDWORD, 0x0fa21a20);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe0f69204);

		} else if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x01528500);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe0d67234);

			if (c_channel >= 52 && c_channel <= 64) {
				odm_set_bb_reg(dm, R_0x918, MASKDWORD,
					       0x1c16ecdf);
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x0f141a20);
			} else {
				odm_set_bb_reg(dm, R_0x918, MASKDWORD,
					       0x1c16acdf);
				if (band_width == CHANNEL_WIDTH_20)
					odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
						       0x64721a20);
				else
					odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
						       0x68721a20);
			}

		} else if (region_domain == PHYDM_DFS_DOMAIN_FCC) {
			odm_set_bb_reg(dm, R_0x918, MASKDWORD, 0x1c16acdf);
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x01528500);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe0d67231);
			if (band_width == CHANNEL_WIDTH_20)
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x64741a20);
			else
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x68741a20);

		} else {
			/* not supported */
			PHYDM_DBG(dm, DBG_DFS,
				  "Unsupported dfs_region_domain:%d\n",
				  region_domain);
			goto exit;
		}

	} else if (dm->support_ic_type &
		   (ODM_RTL8814A | ODM_RTL8822B | ODM_RTL8821C)) {
		odm_set_bb_reg(dm, R_0x814, 0x3fffffff, 0x04cc4d10);
		odm_set_bb_reg(dm, R_0x834, MASKBYTE0, 0x06);

		/* 8822B only, when BW = 20M, DFIR output is 40Mhz,
		 * but DFS input is 80MMHz, so it need to upgrade to 80MHz
		 */
		if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C)) {
			if (band_width == CHANNEL_WIDTH_20)
				odm_set_bb_reg(dm, R_0x1984, BIT(26), 1);
			else
				odm_set_bb_reg(dm, R_0x1984, BIT(26), 0);
		}

		if (dm->radar_detect_dbg_parm_en) {
			phydm_radar_detect_with_dbg_parm(dm);
			enable = 1;
			goto exit;
		}

		if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
			odm_set_bb_reg(dm, R_0x918, MASKDWORD, 0x1c16acdf);
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x095a8500);
			odm_set_bb_reg(dm, R_0x91c, MASKDWORD, 0x0fa21a20);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe0f57204);

		} else if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x095a8500);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe0d67234);

			if (c_channel >= 52 && c_channel <= 64) {
				odm_set_bb_reg(dm, R_0x918, MASKDWORD,
					       0x1c16ecdf);
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x0f141a20);
			} else {
				odm_set_bb_reg(dm, R_0x918, MASKDWORD,
					       0x1c166cdf);
				if (band_width == CHANNEL_WIDTH_20)
					odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
						       0x64721a20);
				else
					odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
						       0x68721a20);
			}

		} else if (region_domain == PHYDM_DFS_DOMAIN_FCC) {
			odm_set_bb_reg(dm, R_0x918, MASKDWORD, 0x1c166cdf);
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x095a8500);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe0d67231);
			if (band_width == CHANNEL_WIDTH_20)
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x64741a20);
			else
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x68741a20);

		} else {
			/* not supported */
			PHYDM_DBG(dm, DBG_DFS,
				  "Unsupported dfs_region_domain:%d\n",
				  region_domain);
			goto exit;
		}
	} else {
		/* not supported IC type*/
		PHYDM_DBG(dm, DBG_DFS, "Unsupported IC type:%d\n",
			  dm->support_ic_type);
		goto exit;
	}

	enable = 1;

	dfs->st_l2h_cur = (u8)odm_get_bb_reg(dm, R_0x91c, 0x000000ff);
	dfs->pwdb_th = (u8)odm_get_bb_reg(dm, R_0x918, 0x00001f00);
	dfs->peak_th = (u8)odm_get_bb_reg(dm, R_0x918, 0x00030000);
	dfs->short_pulse_cnt_th = (u8)odm_get_bb_reg(dm, R_0x920, 0x000f0000);
	dfs->long_pulse_cnt_th = (u8)odm_get_bb_reg(dm, R_0x920, 0x00f00000);
	dfs->peak_window = (u8)odm_get_bb_reg(dm, R_0x920, 0x00000300);
	dfs->nb2wb_th = (u8)odm_get_bb_reg(dm, R_0x920, 0x0000e000);

	phydm_dfs_parameter_init(dm);

exit:
	if (enable) {
		phydm_radar_detect_reset(dm);
		PHYDM_DBG(dm, DBG_DFS, "on cch:%u, bw:%u\n", c_channel,
			  band_width);
	} else
		phydm_radar_detect_disable(dm);
}

void phydm_dfs_parameter_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;

	u8 i;

	dfs->fa_mask_th = 30;
	dfs->det_print = 1;
	dfs->det_print2 = 0;
	dfs->st_l2h_min = 0x20;
	dfs->st_l2h_max = 0x4e;
	dfs->pwdb_scalar_factor = 12;
	dfs->pwdb_th = 8;
	for (i = 0; i < 5; i++) {
		dfs->pulse_flag_hist[i] = 0;
		dfs->radar_det_mask_hist[i] = 0;
		dfs->fa_inc_hist[i] = 0;
	}
}

void phydm_dfs_dynamic_setting(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;

	u8 peak_th_cur = 0, short_pulse_cnt_th_cur = 0;
	u8 long_pulse_cnt_th_cur = 0, three_peak_opt_cur = 0;
	u8 three_peak_th2_cur = 0;
	u8 peak_window_cur = 0, nb2wb_th_cur = 0;
	u8 region_domain = dm->dfs_region_domain;
	u8 c_channel = *dm->channel;

	if (dm->rx_tp <= 2) {
		dfs->idle_mode = 1;
		if (dfs->force_TP_mode)
			dfs->idle_mode = 0;
	} else {
		dfs->idle_mode = 0;
	}

	if (dfs->idle_mode == 1) { /*idle (no traffic)*/
		peak_th_cur = 3;
		short_pulse_cnt_th_cur = 6;
		long_pulse_cnt_th_cur = 13;
		peak_window_cur = 2;
		nb2wb_th_cur = 6;
		three_peak_opt_cur = 1;
		three_peak_th2_cur = 2;
		if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			if (c_channel >= 52 && c_channel <= 64) {
				short_pulse_cnt_th_cur = 14;
				long_pulse_cnt_th_cur = 15;
				nb2wb_th_cur = 3;
				three_peak_th2_cur = 0;
			} else {
				short_pulse_cnt_th_cur = 6;
				nb2wb_th_cur = 3;
				three_peak_th2_cur = 0;
				long_pulse_cnt_th_cur = 10;
			}
		} else if (region_domain == PHYDM_DFS_DOMAIN_FCC) {
			three_peak_th2_cur = 0;
		} else if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
			long_pulse_cnt_th_cur = 15;
			if (phydm_dfs_is_meteorology_channel(dm)) {
			/*need to add check cac end condition*/
				peak_th_cur = 2;
				nb2wb_th_cur = 3;
				three_peak_opt_cur = 1;
				three_peak_th2_cur = 0;
				short_pulse_cnt_th_cur = 7;
			} else {
				three_peak_opt_cur = 1;
				three_peak_th2_cur = 0;
				short_pulse_cnt_th_cur = 7;
				nb2wb_th_cur = 3;
			}
		} else /*default: FCC*/
			three_peak_th2_cur = 0;

	} else { /*in service (with TP)*/
		peak_th_cur = 2;
		short_pulse_cnt_th_cur = 6;
		long_pulse_cnt_th_cur = 9;
		peak_window_cur = 2;
		nb2wb_th_cur = 3;
		three_peak_opt_cur = 1;
		three_peak_th2_cur = 2;
		if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			if (c_channel >= 52 && c_channel <= 64) {
				long_pulse_cnt_th_cur = 15;
				/*for high duty cycle*/
				short_pulse_cnt_th_cur = 5;
				three_peak_th2_cur = 0;
			} else {
				three_peak_opt_cur = 0;
				three_peak_th2_cur = 0;
				long_pulse_cnt_th_cur = 8;
			}
		} else if (region_domain == PHYDM_DFS_DOMAIN_FCC) {
		} else if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
			long_pulse_cnt_th_cur = 15;
			short_pulse_cnt_th_cur = 5;
			three_peak_opt_cur = 0;
		}
	}
}

boolean
phydm_radar_detect_dm_check(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u8 region_domain = dm->dfs_region_domain, index = 0;

	u16 i = 0, k = 0, fa_count_cur = 0, fa_count_inc = 0;
	u16 total_fa_in_hist = 0, pre_post_now_acc_fa_in_hist = 0;
	u16 max_fa_in_hist = 0, vht_crc_ok_cnt_cur = 0;
	u16 vht_crc_ok_cnt_inc = 0, ht_crc_ok_cnt_cur = 0;
	u16 ht_crc_ok_cnt_inc = 0, leg_crc_ok_cnt_cur = 0;
	u16 leg_crc_ok_cnt_inc = 0;
	u16 total_crc_ok_cnt_inc = 0, short_pulse_cnt_cur = 0;
	u16 short_pulse_cnt_inc = 0, long_pulse_cnt_cur = 0;
	u16 long_pulse_cnt_inc = 0, total_pulse_count_inc = 0;
	u32 regf98_value = 0, reg918_value = 0, reg91c_value = 0;
	u32 reg920_value = 0, reg924_value = 0;
	boolean tri_short_pulse = 0, tri_long_pulse = 0, radar_type = 0;
	boolean fault_flag_det = 0, fault_flag_psd = 0, fa_flag = 0;
	boolean radar_detected = 0;
	u8 st_l2h_new = 0, fa_mask_th = 0, sum = 0;
	u8 c_channel = *dm->channel;

	/*Get FA count during past 100ms*/
	fa_count_cur = (u16)odm_get_bb_reg(dm, R_0xf48, 0x0000ffff);

	if (dfs->fa_count_pre == 0)
		fa_count_inc = 0;
	else if (fa_count_cur >= dfs->fa_count_pre)
		fa_count_inc = fa_count_cur - dfs->fa_count_pre;
	else
		fa_count_inc = fa_count_cur;
	dfs->fa_count_pre = fa_count_cur;

	dfs->fa_inc_hist[dfs->mask_idx] = fa_count_inc;

	for (i = 0; i < 5; i++) {
		total_fa_in_hist = total_fa_in_hist + dfs->fa_inc_hist[i];
		if (dfs->fa_inc_hist[i] > max_fa_in_hist)
			max_fa_in_hist = dfs->fa_inc_hist[i];
	}
	if (dfs->mask_idx >= 2)
		index = dfs->mask_idx - 2;
	else
		index = 5 + dfs->mask_idx - 2;
	if (index == 0) {
		pre_post_now_acc_fa_in_hist = dfs->fa_inc_hist[index] +
					      dfs->fa_inc_hist[index + 1] +
					      dfs->fa_inc_hist[4];
	} else if (index == 4) {
		pre_post_now_acc_fa_in_hist = dfs->fa_inc_hist[index] +
					      dfs->fa_inc_hist[0] +
					      dfs->fa_inc_hist[index - 1];
	} else {
		pre_post_now_acc_fa_in_hist = dfs->fa_inc_hist[index] +
					      dfs->fa_inc_hist[index + 1] +
					      dfs->fa_inc_hist[index - 1];
	}

	/*Get VHT CRC32 ok count during past 100ms*/
	vht_crc_ok_cnt_cur = (u16)odm_get_bb_reg(dm, R_0xf0c, 0x00003fff);
	if (vht_crc_ok_cnt_cur >= dfs->vht_crc_ok_cnt_pre) {
		vht_crc_ok_cnt_inc = vht_crc_ok_cnt_cur -
				     dfs->vht_crc_ok_cnt_pre;
	} else {
		vht_crc_ok_cnt_inc = vht_crc_ok_cnt_cur;
	}
	dfs->vht_crc_ok_cnt_pre = vht_crc_ok_cnt_cur;

	/*Get HT CRC32 ok count during past 100ms*/
	ht_crc_ok_cnt_cur = (u16)odm_get_bb_reg(dm, R_0xf10, 0x00003fff);
	if (ht_crc_ok_cnt_cur >= dfs->ht_crc_ok_cnt_pre)
		ht_crc_ok_cnt_inc = ht_crc_ok_cnt_cur - dfs->ht_crc_ok_cnt_pre;
	else
		ht_crc_ok_cnt_inc = ht_crc_ok_cnt_cur;
	dfs->ht_crc_ok_cnt_pre = ht_crc_ok_cnt_cur;

	/*Get Legacy CRC32 ok count during past 100ms*/
	leg_crc_ok_cnt_cur = (u16)odm_get_bb_reg(dm, R_0xf14, 0x00003fff);
	if (leg_crc_ok_cnt_cur >= dfs->leg_crc_ok_cnt_pre)
		leg_crc_ok_cnt_inc = leg_crc_ok_cnt_cur - dfs->leg_crc_ok_cnt_pre;
	else
		leg_crc_ok_cnt_inc = leg_crc_ok_cnt_cur;
	dfs->leg_crc_ok_cnt_pre = leg_crc_ok_cnt_cur;

	if (vht_crc_ok_cnt_cur == 0x3fff ||
	    ht_crc_ok_cnt_cur == 0x3fff ||
	    leg_crc_ok_cnt_cur == 0x3fff) {
		odm_set_bb_reg(dm, R_0xb58, BIT(0), 1);
		odm_set_bb_reg(dm, R_0xb58, BIT(0), 0);
	}

	total_crc_ok_cnt_inc = vht_crc_ok_cnt_inc +
			       ht_crc_ok_cnt_inc +
			       leg_crc_ok_cnt_inc;

	/*Get short pulse count, need carefully handle the counter overflow*/
	regf98_value = odm_get_bb_reg(dm, R_0xf98, 0xffffffff);
	short_pulse_cnt_cur = (u16)(regf98_value & 0x000000ff);
	if (short_pulse_cnt_cur >= dfs->short_pulse_cnt_pre) {
		short_pulse_cnt_inc = short_pulse_cnt_cur -
				      dfs->short_pulse_cnt_pre;
	} else {
		short_pulse_cnt_inc = short_pulse_cnt_cur;
	}
	dfs->short_pulse_cnt_pre = short_pulse_cnt_cur;

	/*Get long pulse count, need carefully handle the counter overflow*/
	long_pulse_cnt_cur = (u16)((regf98_value & 0x0000ff00) >> 8);
	if (long_pulse_cnt_cur >= dfs->long_pulse_cnt_pre) {
		long_pulse_cnt_inc = long_pulse_cnt_cur -
				     dfs->long_pulse_cnt_pre;
	} else {
		long_pulse_cnt_inc = long_pulse_cnt_cur;
	}
	dfs->long_pulse_cnt_pre = long_pulse_cnt_cur;

	total_pulse_count_inc = short_pulse_cnt_inc + long_pulse_cnt_inc;

	if (dfs->det_print) {
		PHYDM_DBG(dm, DBG_DFS,
			  "===============================================\n");
		PHYDM_DBG(dm, DBG_DFS,
			  "Total_CRC_OK_cnt_inc[%d] VHT_CRC_ok_cnt_inc[%d] HT_CRC_ok_cnt_inc[%d] LEG_CRC_ok_cnt_inc[%d] FA_count_inc[%d]\n",
			  total_crc_ok_cnt_inc, vht_crc_ok_cnt_inc,
			  ht_crc_ok_cnt_inc, leg_crc_ok_cnt_inc, fa_count_inc);
		PHYDM_DBG(dm, DBG_DFS,
			  "Init_Gain[%x] 0x91c[%x] 0xf98[%08x] short_pulse_cnt_inc[%d] long_pulse_cnt_inc[%d]\n",
			  dfs->igi_cur, dfs->st_l2h_cur, regf98_value,
			  short_pulse_cnt_inc, long_pulse_cnt_inc);
		PHYDM_DBG(dm, DBG_DFS, "Throughput: %dMbps\n", dm->rx_tp);
		reg918_value = odm_get_bb_reg(dm, R_0x918, 0xffffffff);
		reg91c_value = odm_get_bb_reg(dm, R_0x91c, 0xffffffff);
		reg920_value = odm_get_bb_reg(dm, R_0x920, 0xffffffff);
		reg924_value = odm_get_bb_reg(dm, R_0x924, 0xffffffff);
		PHYDM_DBG(dm, DBG_DFS,
			  "0x918[%08x] 0x91c[%08x] 0x920[%08x] 0x924[%08x]\n",
			  reg918_value, reg91c_value, reg920_value,
			  reg924_value);
		PHYDM_DBG(dm, DBG_DFS,
			  "dfs_regdomain = %d, dbg_mode = %d, idle_mode = %d\n",
			  region_domain, dfs->dbg_mode, dfs->idle_mode);
	}
	tri_short_pulse = (regf98_value & BIT(17)) ? 1 : 0;
	tri_long_pulse = (regf98_value & BIT(19)) ? 1 : 0;

	if (tri_short_pulse)
		radar_type = 0;
	else if (tri_long_pulse)
		radar_type = 1;

	if (tri_short_pulse) {
		odm_set_bb_reg(dm, R_0x924, BIT(15), 0);
		odm_set_bb_reg(dm, R_0x924, BIT(15), 1);
	}
	if (tri_long_pulse) {
		odm_set_bb_reg(dm, R_0x924, BIT(15), 0);
		odm_set_bb_reg(dm, R_0x924, BIT(15), 1);
		if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			if (c_channel >= 52 && c_channel <= 64) {
				tri_long_pulse = 0;
			}
		}
		if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
			tri_long_pulse = 0;
		}
	}

	st_l2h_new = dfs->st_l2h_cur;
	dfs->pulse_flag_hist[dfs->mask_idx] = tri_short_pulse | tri_long_pulse;

	/* PSD(not ready) */

	fault_flag_det = 0;
	fault_flag_psd = 0;
	fa_flag = 0;
	if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
		fa_mask_th = dfs->fa_mask_th + 20;
	} else {
		fa_mask_th = dfs->fa_mask_th;
	}
	if (max_fa_in_hist >= fa_mask_th ||
	    total_fa_in_hist >= fa_mask_th ||
	    pre_post_now_acc_fa_in_hist >= fa_mask_th ||
	    dfs->igi_cur >= 0x30) {
		st_l2h_new = dfs->st_l2h_max;
		dfs->radar_det_mask_hist[index] = 1;
		if (dfs->pulse_flag_hist[index] == 1) {
			dfs->pulse_flag_hist[index] = 0;
			if (dfs->det_print2) {
				PHYDM_DBG(dm, DBG_DFS,
					  "Radar is masked : FA mask\n");
			}
		}
		fa_flag = 1;
	} else {
		dfs->radar_det_mask_hist[index] = 0;
	}

	if (dfs->det_print) {
		PHYDM_DBG(dm, DBG_DFS, "mask_idx: %d\n", dfs->mask_idx);
		PHYDM_DBG(dm, DBG_DFS, "radar_det_mask_hist: ");
		for (i = 0; i < 5; i++)
			PHYDM_DBG(dm, DBG_DFS, "%d ",
				  dfs->radar_det_mask_hist[i]);
		PHYDM_DBG(dm, DBG_DFS, "pulse_flag_hist: ");
		for (i = 0; i < 5; i++)
			PHYDM_DBG(dm, DBG_DFS, "%d ", dfs->pulse_flag_hist[i]);
		PHYDM_DBG(dm, DBG_DFS, "fa_inc_hist: ");
		for (i = 0; i < 5; i++)
			PHYDM_DBG(dm, DBG_DFS, "%d ", dfs->fa_inc_hist[i]);
		PHYDM_DBG(dm, DBG_DFS,
			  "\nfa_mask_th: %d max_fa_in_hist: %d total_fa_in_hist: %d pre_post_now_acc_fa_in_hist: %d ",
			  fa_mask_th, max_fa_in_hist, total_fa_in_hist,
			  pre_post_now_acc_fa_in_hist);
	}

	sum = 0;
	for (k = 0; k < 5; k++) {
		if (dfs->radar_det_mask_hist[k] == 1)
			sum++;
	}

	if (dfs->mask_hist_checked <= 5)
		dfs->mask_hist_checked++;

	if (dfs->mask_hist_checked >= 5 && dfs->pulse_flag_hist[index]) {
		if (sum <= 2) {
			radar_detected = 1;
			PHYDM_DBG(dm, DBG_DFS,
				  "Detected type %d radar signal!\n",
				  radar_type);
		} else {
			fault_flag_det = 1;
			if (dfs->det_print2) {
				PHYDM_DBG(dm, DBG_DFS,
					  "Radar is masked : mask_hist large than thd\n");
			}
		}
	}

	dfs->mask_idx++;
	if (dfs->mask_idx == 5)
		dfs->mask_idx = 0;

	if (fault_flag_det == 0 && fault_flag_psd == 0 && fa_flag == 0) {
		if (dfs->igi_cur < 0x30) {
			st_l2h_new = dfs->st_l2h_min;
		}
	}

	if (st_l2h_new != dfs->st_l2h_cur) {
		if (st_l2h_new < dfs->st_l2h_min) {
			dfs->st_l2h_cur = dfs->st_l2h_min;
		} else if (st_l2h_new > dfs->st_l2h_max)
			dfs->st_l2h_cur = dfs->st_l2h_max;
		else
			dfs->st_l2h_cur = st_l2h_new;
		odm_set_bb_reg(dm, R_0x91c, 0xff, dfs->st_l2h_cur);

		dfs->pwdb_th = ((int)dfs->st_l2h_cur - (int)dfs->igi_cur) / 2 +
			       dfs->pwdb_scalar_factor;

		/*limit the pwdb value to absolute lower bound 8*/
		dfs->pwdb_th = MAX_2(dfs->pwdb_th, (int)dfs->pwdb_th);

		/*limit the pwdb value to absolute upper bound 0x1f*/
		dfs->pwdb_th = MIN_2(dfs->pwdb_th, 0x1f);
		odm_set_bb_reg(dm, R_0x918, 0x00001f00, dfs->pwdb_th);
	}

	if (dfs->det_print) {
		PHYDM_DBG(dm, DBG_DFS,
			  "fault_flag_det[%d], fault_flag_psd[%d], DFS_detected [%d]\n",
			  fault_flag_det, fault_flag_psd, radar_detected);
	}

	return radar_detected;
}

boolean phydm_radar_detect(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	boolean enable_DFS = false;
	boolean radar_detected = false;

	dfs->igi_cur = (u8)odm_get_bb_reg(dm, R_0xc50, 0x0000007f);

	dfs->st_l2h_cur = (u8)odm_get_bb_reg(dm, R_0x91c, 0x000000ff);

	/* dynamic pwdb calibration */
	if (dfs->igi_pre != dfs->igi_cur) {
		dfs->pwdb_th = ((int)dfs->st_l2h_cur - (int)dfs->igi_cur) / 2 +
			       dfs->pwdb_scalar_factor;

		/* limit the pwdb value to absolute lower bound 0xa */
		dfs->pwdb_th = MAX_2(dfs->pwdb_th_cur, (int)dfs->pwdb_th);
		/* limit the pwdb value to absolute upper bound 0x1f */
		dfs->pwdb_th = MIN_2(dfs->pwdb_th_cur, 0x1f);
		odm_set_bb_reg(dm, R_0x918, 0x00001f00, dfs->pwdb_th);
	}

	dfs->igi_pre = dfs->igi_cur;

	phydm_dfs_dynamic_setting(dm);
	radar_detected = phydm_radar_detect_dm_check(dm);

	if (odm_get_bb_reg(dm, R_0x924, BIT(15)))
		enable_DFS = true;

	if (enable_DFS && radar_detected) {
		PHYDM_DBG(dm, DBG_DFS,
			  "Radar detect: enable_DFS:%d, radar_detected:%d\n",
			  enable_DFS, radar_detected);
		phydm_radar_detect_reset(dm);
		if (dfs->dbg_mode == 1) {
			PHYDM_DBG(dm, DBG_DFS,
				  "Radar is detected in DFS dbg mode.\n");
			radar_detected = 0;
		}
	}

	return enable_DFS && radar_detected;
}

void phydm_dfs_debug(
	void *dm_void,
	u32 *const argv,
	u32 *_used,
	char *output,
	u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u32 used = *_used;
	u32 out_len = *_out_len;

	dfs->dbg_mode = (boolean)argv[0];
	dfs->force_TP_mode = (boolean)argv[1];
	dfs->det_print = (boolean)argv[2];
	dfs->det_print2 = (boolean)argv[3];

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "dbg_mode: %d, force_TP_mode: %d, det_print: %d, det_print2: %d\n",
		 dfs->dbg_mode, dfs->force_TP_mode, dfs->det_print,
		 dfs->det_print2);

	/*switch (argv[0]) {
	case 1:
#if defined(CONFIG_PHYDM_DFS_MASTER)
		 set dbg parameters for radar detection instead of the default value
		if (argv[1] == 1) {
			dm->radar_detect_reg_918 = argv[2];
			dm->radar_detect_reg_91c = argv[3];
			dm->radar_detect_reg_920 = argv[4];
			dm->radar_detect_reg_924 = argv[5];
			dm->radar_detect_dbg_parm_en = 1;

			PDM_SNPF((output + used, out_len - used, "Radar detection with dbg parameter\n"));
			PDM_SNPF((output + used, out_len - used, "reg918:0x%08X\n", dm->radar_detect_reg_918));
			PDM_SNPF((output + used, out_len - used, "reg91c:0x%08X\n", dm->radar_detect_reg_91c));
			PDM_SNPF((output + used, out_len - used, "reg920:0x%08X\n", dm->radar_detect_reg_920));
			PDM_SNPF((output + used, out_len - used, "reg924:0x%08X\n", dm->radar_detect_reg_924));
		} else {
			dm->radar_detect_dbg_parm_en = 0;
			PDM_SNPF((output + used, out_len - used, "Radar detection with default parameter\n"));
		}
		phydm_radar_detect_enable(dm);
#endif  defined(CONFIG_PHYDM_DFS_MASTER)

		break;
	default:
		break;
	}*/
}

#endif /* defined(CONFIG_PHYDM_DFS_MASTER) */

boolean
phydm_is_dfs_band(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (((*dm->channel >= 52) && (*dm->channel <= 64)) ||
	    ((*dm->channel >= 100) && (*dm->channel <= 140)))
		return true;
	else
		return false;
}

boolean
phydm_dfs_master_enabled(void *dm_void)
{
#ifdef CONFIG_PHYDM_DFS_MASTER
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	return *dm->dfs_master_enabled ? true : false;
#else
	return false;
#endif
}
