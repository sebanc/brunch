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

/*@
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

/* @Init radar detection parameters, called after ch, bw is set */

void phydm_radar_detect_enable(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u8 region_domain = dm->dfs_region_domain;
	u8 c_channel = *dm->channel;
	u8 band_width = *dm->band_width;
	u8 enable = 0;
	u8 short_pw_upperbound = 0;

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
		/*@for 8822B RXHP H2L, since L will always cause DFS FRD
		if (dm->support_ic_type & (ODM_RTL8822B)) {
			odm_set_bb_reg(dm, 0x8d8, MASKDWORD, 0x29035612);
			odm_set_bb_reg(dm, 0x8cc, MASKDWORD, 0x08108492);
		}
		 */

		odm_set_bb_reg(dm, R_0x814, 0x3fffffff, 0x04cc4d10);
		odm_set_bb_reg(dm, R_0x834, MASKBYTE0, 0x06);

		/* @8822B only, when BW = 20M, DFIR output is 40Mhz,
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
			odm_set_bb_reg(dm, R_0x918, MASKDWORD, 0x1c176cdf);
			odm_set_bb_reg(dm, R_0x924, MASKDWORD, 0x095a8400);
			odm_set_bb_reg(dm, R_0x920, MASKDWORD, 0xe076d231);
			if (band_width == CHANNEL_WIDTH_20)
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x64901a20);
			else
				odm_set_bb_reg(dm, R_0x91c, MASKDWORD,
					       0x62901a20);

		} else {
			/* not supported */
			PHYDM_DBG(dm, DBG_DFS,
				  "Unsupported dfs_region_domain:%d\n",
				  region_domain);
			goto exit;
		}
		/*RXHP low corner will extend the pulse width,
		 *so we need to increase the upper bound.
		 */
		if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C)) {
			if (odm_get_bb_reg(dm, 0x8d8,
					   BIT28 | BIT27 | BIT26) == 0) {
				short_pw_upperbound =
					(u8)odm_get_bb_reg(dm, 0x91c,
						       BIT23 | BIT22 |
						       BIT21 | BIT20);
				if ((short_pw_upperbound + 4) > 15)
					odm_set_bb_reg(dm, 0x91c,
						       BIT23 | BIT22 |
						       BIT21 | BIT20, 15);
				else
					odm_set_bb_reg(dm, 0x91c,
						       BIT23 | BIT22 |
						       BIT21 | BIT20,
						       short_pw_upperbound + 4);
			}
			/*@if peak index -1~+1, use original NB method*/
			odm_set_bb_reg(dm, 0x19e4, 0x003C0000, 13);
			odm_set_bb_reg(dm, 0x924, 0x70000, 0);
		}

		if (dm->support_ic_type & (ODM_RTL8881A))
			odm_set_bb_reg(dm, 0xb00, 0xc0000000, 3);

		/*@for 8814 new dfs mechanism setting*/
		if (dm->support_ic_type &
		    (ODM_RTL8814A | ODM_RTL8822B | ODM_RTL8821C)) {
			/*Turn off dfs scaling factor*/
			odm_set_bb_reg(dm, 0x19e4, 0x1fff, 0x0c00);
			/*NonDC peak_th = 2times DC peak_th*/
			odm_set_bb_reg(dm, 0x19e4, 0x30000, 1);
			/*power for debug and auto test flow latch after ST*/
			odm_set_bb_reg(dm, 0x9f8, 0xc0000000, 3);

			/*@low pulse width radar pattern will cause wrong drop*/
			/*@disable peak index should the same
			 *during the same short pulse (new mechan)
			 */
			odm_set_bb_reg(dm, 0x9f4, 0x80000000, 0);

			/*@disable peak index should the same
			 *during the same short pulse (old mechan)
			 */
			odm_set_bb_reg(dm, 0x924, 0x20000000, 0);

			/*@if peak index diff >=2, then drop the result*/
			odm_set_bb_reg(dm, 0x19e4, 0xe000, 2);
			if (region_domain == 2) {
				if ((c_channel >= 52) && (c_channel <= 64)) {
					/*pulse width hist th setting*/
					/*th1=2*04us*/
					odm_set_bb_reg(dm, 0x19e4,
						       0xff000000, 2);
					/*th2 = 3*0.4us, th3 = 4*0.4us
					 *th4 = 7*0.4, th5 = 34*0.4
					 */
					odm_set_bb_reg(dm, 0x19e8,
						       MASKDWORD, 0x22070403);

					/*PRI hist th setting*/
					/*th1=42*32us*/
					odm_set_bb_reg(dm, 0x19b8,
						       0x00007f80, 42);
					/*th2=47*32us, th3=115*32us,
					 *th4=123*32us, th5=130*32us
					 */
					odm_set_bb_reg(dm, 0x19ec,
						       MASKDWORD, 0x827b732f);
				} else{
					/*pulse width hist th setting*/
					/*th1=2*04us*/
					odm_set_bb_reg(dm, 0x19e4,
						       0xff000000, 1);
					/*th2 = 13*0.4us, th3 = 26*0.4us
					 *th4 = 75*0.4us, th5 = 255*0.4us
					 */
					odm_set_bb_reg(dm, 0x19e8,
						       MASKDWORD, 0xff4b1a0d);
					/*PRI hist th setting*/
					/*th1=4*32us*/

					odm_set_bb_reg(dm, 0x19b8,
						       0x00007f80, 4);
					/*th2=8*32us, th3=16*32us,
					 *th4=32*32us, th5=128*32=4096us
					 */
					odm_set_bb_reg(dm, 0x19ec,
						       MASKDWORD, 0x80201008);
				}
			}
			/*@ETSI*/
			else if (region_domain == 3) {
				/*pulse width hist th setting*/
				/*th1=2*04us*/
				odm_set_bb_reg(dm, 0x19e4, 0xff000000, 1);
				odm_set_bb_reg(dm, 0x19e8,
					       MASKDWORD, 0x68260d06);
				/*PRI hist th setting*/
				/*th1=7*32us*/
				odm_set_bb_reg(dm, 0x19b8, 0x00007f80, 7);
				/*th2=40*32us, th3=80*32us,
				 *th4=110*32us, th5=157*32=5024
				 */
				odm_set_bb_reg(dm, 0x19ec,
					       MASKDWORD, 0x9d6e2010);
			}
			/*@FCC*/
			else if (region_domain == 1) {
				/*pulse width hist th setting*/
				/*th1=2*04us*/
				odm_set_bb_reg(dm, 0x19e4, 0xff000000, 2);
				/*th2 = 13*0.4us, th3 = 26*0.4us,
				 *th4 = 75*0.4us, th5 = 255*0.4us
				 */
				odm_set_bb_reg(dm, 0x19e8,
					       MASKDWORD, 0xff4b1a0d);

				/*PRI hist th setting*/
				/*th1=4*32us*/
				odm_set_bb_reg(dm, 0x19b8, 0x00007f80, 4);
				/*th2=8*32us, th3=21*32us,
				 *th4=32*32us, th5=96*32=3072
				 */
				if (band_width == CHANNEL_WIDTH_20)
					odm_set_bb_reg(dm, 0x19ec,
						       MASKDWORD, 0x60282010);
				else
					odm_set_bb_reg(dm, 0x19ec,
						       MASKDWORD, 0x60282420);
			} else {
			}
		}
	} else {
		/*not supported IC type*/
		PHYDM_DBG(dm, DBG_DFS, "Unsupported IC type:%d\n",
			  dm->support_ic_type);
		goto exit;
	}

	enable = 1;

	dfs->st_l2h_cur = (u8)odm_get_bb_reg(dm, R_0x91c, 0x000000ff);
	dfs->pwdb_th_cur = (u8)odm_get_bb_reg(dm, R_0x918, 0x00001f00);
	dfs->peak_th = (u8)odm_get_bb_reg(dm, R_0x918, 0x00030000);
	dfs->short_pulse_cnt_th = (u8)odm_get_bb_reg(dm, R_0x920, 0x000f0000);
	dfs->long_pulse_cnt_th = (u8)odm_get_bb_reg(dm, R_0x920, 0x00f00000);
	dfs->peak_window = (u8)odm_get_bb_reg(dm, R_0x920, 0x00000300);
	dfs->three_peak_opt = (u8)odm_get_bb_reg(dm, 0x924, 0x00000180);
	dfs->three_peak_th2 = (u8)odm_get_bb_reg(dm, 0x924, 0x00007000);

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
	dfs->force_TP_mode = 0;
	dfs->det_print = 0;
	dfs->det_print2 = 0;
	dfs->print_hist_rpt = 0;
	dfs->hist_cond_on = 1;
	dfs->st_l2h_min = 0x20;
	dfs->st_l2h_max = 0x4e;
	dfs->pwdb_scalar_factor = 12;
	dfs->pwdb_th = 8;
	for (i = 0; i < 5; i++) {
		dfs->pulse_flag_hist[i] = 0;
		dfs->pulse_type_hist[i] = 0;
		dfs->radar_det_mask_hist[i] = 0;
		dfs->fa_inc_hist[i] = 0;
	}

	/*@for dfs histogram*/
	dfs->pri_hist_th = 5;
	dfs->pri_sum_g1_th = 9;
	dfs->pri_sum_g5_th = 4;
	dfs->pri_sum_g1_fcc_th = 4;		/*@FCC Type6*/
	dfs->pri_sum_g3_fcc_th = 6;
	dfs->pri_sum_safe_th = 50;
	dfs->pri_sum_safe_fcc_th = 110;		/*@30 for AP*/
	dfs->pri_sum_type4_th = 16;
	dfs->pri_sum_type6_th = 12;
	dfs->pri_sum_g5_under_g1_th = 0;
	dfs->pri_pw_diff_th = 4;
	dfs->pri_pw_diff_fcc_th = 8;
	dfs->pri_pw_diff_fcc_idle_th = 2;
	dfs->pri_pw_diff_w53_th = 10;
	dfs->pw_std_th = 7;			/*@FCC Type4*/
	dfs->pw_std_idle_th = 10;
	dfs->pri_std_th = 6;			/*@FCC Type3,4,6*/
	dfs->pri_std_idle_th = 10;
	dfs->pri_type1_upp_fcc_th = 110;
	dfs->pri_type1_low_fcc_th = 50;
	dfs->pri_type1_cen_fcc_th = 70;
	dfs->pw_g0_th = 8;
	dfs->pw_long_lower_th = 6;		/*@7->6*/
	dfs->pri_long_upper_th = 30;
	dfs->pw_long_lower_20m_th = 7;		/*@7 for AP*/
	dfs->pw_long_sum_upper_th = 60;
	dfs->type4_pw_max_cnt = 7;
	dfs->type4_safe_pri_sum_th = 5;
}

void phydm_dfs_dynamic_setting(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;

	u8 peak_th_cur = 0, short_pulse_cnt_th_cur = 0;
	u8 long_pulse_cnt_th_cur = 0, three_peak_opt_cur = 0;
	u8 three_peak_th2_cur = 0;
	u8 peak_window_cur = 0;
	u8 region_domain = dm->dfs_region_domain;
	u8 c_channel = *dm->channel;

	if (dm->rx_tp + dm->tx_tp <= 2) {
		dfs->idle_mode = 1;
		if (dfs->force_TP_mode)
			dfs->idle_mode = 0;
	} else {
		dfs->idle_mode = 0;
	}

	if (dfs->idle_mode == 1) { /*@idle (no traffic)*/
		peak_th_cur = 3;
		short_pulse_cnt_th_cur = 6;
		long_pulse_cnt_th_cur = 9;
		peak_window_cur = 2;
		three_peak_opt_cur = 0;
		three_peak_th2_cur = 2;
		if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			if (c_channel >= 52 && c_channel <= 64) {
				short_pulse_cnt_th_cur = 14;
				long_pulse_cnt_th_cur = 15;
				three_peak_th2_cur = 0;
			} else {
				short_pulse_cnt_th_cur = 6;
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
				three_peak_opt_cur = 0;
				three_peak_th2_cur = 0;
				short_pulse_cnt_th_cur = 7;
			} else {
				three_peak_opt_cur = 0;
				three_peak_th2_cur = 0;
				short_pulse_cnt_th_cur = 7;
			}
		} else /*@default: FCC*/
			three_peak_th2_cur = 0;

	} else { /*@in service (with TP)*/
		peak_th_cur = 2;
		short_pulse_cnt_th_cur = 6;
		long_pulse_cnt_th_cur = 7;
		peak_window_cur = 2;
		three_peak_opt_cur = 0;
		three_peak_th2_cur = 2;
		if (region_domain == PHYDM_DFS_DOMAIN_MKK) {
			if (c_channel >= 52 && c_channel <= 64) {
				long_pulse_cnt_th_cur = 15;
				/*@for high duty cycle*/
				short_pulse_cnt_th_cur = 5;
				three_peak_th2_cur = 0;
			} else {
				three_peak_opt_cur = 0;
				three_peak_th2_cur = 0;
				long_pulse_cnt_th_cur = 8;
			}
		} else if (region_domain == PHYDM_DFS_DOMAIN_FCC) {
			long_pulse_cnt_th_cur = 5;	/*for 80M FCC*/
			short_pulse_cnt_th_cur = 5;	/*for 80M FCC*/
		} else if (region_domain == PHYDM_DFS_DOMAIN_ETSI) {
			long_pulse_cnt_th_cur = 15;
			short_pulse_cnt_th_cur = 5;
			three_peak_opt_cur = 0;
		}
	}
	if (dfs->peak_th != peak_th_cur)
		odm_set_bb_reg(dm, 0x918, 0x00030000, peak_th_cur);
	if (dfs->short_pulse_cnt_th != short_pulse_cnt_th_cur)
		odm_set_bb_reg(dm, 0x920, 0x000f0000, short_pulse_cnt_th_cur);
	if (dfs->long_pulse_cnt_th != long_pulse_cnt_th_cur)
		odm_set_bb_reg(dm, 0x920, 0x00f00000, long_pulse_cnt_th_cur);
	if (dfs->peak_window != peak_window_cur)
		odm_set_bb_reg(dm, 0x920, 0x00000300, peak_window_cur);
	if (dfs->three_peak_opt != three_peak_opt_cur)
		odm_set_bb_reg(dm, 0x924, 0x00000180, three_peak_opt_cur);
	if (dfs->three_peak_th2 != three_peak_th2_cur)
		odm_set_bb_reg(dm, 0x924, 0x00007000, three_peak_th2_cur);

	dfs->peak_th = peak_th_cur;
	dfs->short_pulse_cnt_th = short_pulse_cnt_th_cur;
	dfs->long_pulse_cnt_th = long_pulse_cnt_th_cur;
	dfs->peak_window = peak_window_cur;
	dfs->three_peak_opt = three_peak_opt_cur;
	dfs->three_peak_th2 = three_peak_th2_cur;
}

boolean
phydm_radar_detect_dm_check(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u8 region_domain = dm->dfs_region_domain, index = 0;

	u16 i = 0, j = 0, k = 0, fa_count_cur = 0, fa_count_inc = 0;
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

	/*@Get FA count during past 100ms*/
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

	/*@Get VHT CRC32 ok count during past 100ms*/
	vht_crc_ok_cnt_cur = (u16)odm_get_bb_reg(dm, R_0xf0c, 0x00003fff);
	if (vht_crc_ok_cnt_cur >= dfs->vht_crc_ok_cnt_pre) {
		vht_crc_ok_cnt_inc = vht_crc_ok_cnt_cur -
				     dfs->vht_crc_ok_cnt_pre;
	} else {
		vht_crc_ok_cnt_inc = vht_crc_ok_cnt_cur;
	}
	dfs->vht_crc_ok_cnt_pre = vht_crc_ok_cnt_cur;

	/*@Get HT CRC32 ok count during past 100ms*/
	ht_crc_ok_cnt_cur = (u16)odm_get_bb_reg(dm, R_0xf10, 0x00003fff);
	if (ht_crc_ok_cnt_cur >= dfs->ht_crc_ok_cnt_pre)
		ht_crc_ok_cnt_inc = ht_crc_ok_cnt_cur - dfs->ht_crc_ok_cnt_pre;
	else
		ht_crc_ok_cnt_inc = ht_crc_ok_cnt_cur;
	dfs->ht_crc_ok_cnt_pre = ht_crc_ok_cnt_cur;

	/*@Get Legacy CRC32 ok count during past 100ms*/
	leg_crc_ok_cnt_cur = (u16)odm_get_bb_reg(dm, R_0xf14, 0x00003fff);
	if (leg_crc_ok_cnt_cur >= dfs->leg_crc_ok_cnt_pre)
		leg_crc_ok_cnt_inc = leg_crc_ok_cnt_cur - dfs->leg_crc_ok_cnt_pre;
	else
		leg_crc_ok_cnt_inc = leg_crc_ok_cnt_cur;
	dfs->leg_crc_ok_cnt_pre = leg_crc_ok_cnt_cur;

	if (vht_crc_ok_cnt_cur == 0x3fff ||
	    ht_crc_ok_cnt_cur == 0x3fff ||
	    leg_crc_ok_cnt_cur == 0x3fff) {
		phydm_reset_bb_hw_cnt_ac(dm);
		/*@*/
	}

	total_crc_ok_cnt_inc = vht_crc_ok_cnt_inc +
			       ht_crc_ok_cnt_inc +
			       leg_crc_ok_cnt_inc;

	/*@Get short pulse count, need carefully handle the counter overflow*/
	regf98_value = odm_get_bb_reg(dm, R_0xf98, 0xffffffff);
	short_pulse_cnt_cur = (u16)(regf98_value & 0x000000ff);
	if (short_pulse_cnt_cur >= dfs->short_pulse_cnt_pre) {
		short_pulse_cnt_inc = short_pulse_cnt_cur -
				      dfs->short_pulse_cnt_pre;
	} else {
		short_pulse_cnt_inc = short_pulse_cnt_cur;
	}
	dfs->short_pulse_cnt_pre = short_pulse_cnt_cur;

	/*@Get long pulse count, need carefully handle the counter overflow*/
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
		PHYDM_DBG(dm, DBG_DFS, "Throughput: %dMbps\n",
			  dm->rx_tp + dm->tx_tp);
			  reg918_value = odm_get_bb_reg(dm, R_0x918,
							0xffffffff);
			  reg91c_value = odm_get_bb_reg(dm, R_0x91c,
							0xffffffff);
			  reg920_value = odm_get_bb_reg(dm, R_0x920,
							0xffffffff);
			  reg924_value = odm_get_bb_reg(dm, R_0x924,
							0xffffffff);
		PHYDM_DBG(dm, DBG_DFS,
			  "0x918[%08x] 0x91c[%08x] 0x920[%08x] 0x924[%08x]\n",
			  reg918_value, reg91c_value,
			  reg920_value, reg924_value);
		PHYDM_DBG(dm, DBG_DFS,
			  "dfs_regdomain = %d, dbg_mode = %d, idle_mode = %d, print_hist_rpt = %d, hist_cond_on = %d\n",
			  region_domain, dfs->dbg_mode,
			  dfs->idle_mode, dfs->print_hist_rpt,
			  dfs->hist_cond_on);
	}
	tri_short_pulse = (regf98_value & BIT(17)) ? 1 : 0;
	tri_long_pulse = (regf98_value & BIT(19)) ? 1 : 0;

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
	dfs->pulse_type_hist[dfs->mask_idx] = (tri_long_pulse) ? 1 : 0;

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
			if (dfs->hist_cond_on) {
				/*return the value from hist_radar_detected*/
				radar_detected = phydm_dfs_hist_log(dm, index);
			} else {
				if (dfs->pulse_type_hist[index] == 0)
					dfs->radar_type = 0;
				else if (dfs->pulse_type_hist[index] == 1)
					dfs->radar_type = 1;
				radar_detected = 1;
				PHYDM_DBG(dm, DBG_DFS,
					  "Detected type %d radar signal!\n",
					  dfs->radar_type);
			}
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
		/*odm_set_bb_reg(dm, R_0x91c, 0xff, dfs->st_l2h_cur);*/

		dfs->pwdb_th_cur = ((int)dfs->st_l2h_cur - (int)dfs->igi_cur)
				    / 2 + dfs->pwdb_scalar_factor;

		/*@limit the pwdb value to absolute lower bound 8*/
		dfs->pwdb_th_cur = MAX_2(dfs->pwdb_th_cur, (int)dfs->pwdb_th);

		/*@limit the pwdb value to absolute upper bound 0x1f*/
		dfs->pwdb_th_cur = MIN_2(dfs->pwdb_th_cur, 0x1f);
		odm_set_bb_reg(dm, R_0x918, 0x00001f00, dfs->pwdb_th_cur);
	}

	if (dfs->det_print) {
		PHYDM_DBG(dm, DBG_DFS,
			  "fault_flag_det[%d], fault_flag_psd[%d], DFS_detected [%d]\n",
			  fault_flag_det, fault_flag_psd, radar_detected);
	}

	return radar_detected;
}

void phydm_dfs_histogram_radar_distinguish(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u8 region_domain = dm->dfs_region_domain;
	u8 c_channel = *dm->channel;
	u8 band_width = *dm->band_width;

	u8 dfs_pw_thd1 = 0, dfs_pw_thd2 = 0, dfs_pw_thd3 = 0;
	u8 dfs_pw_thd4 = 0, dfs_pw_thd5 = 0;
	u8 dfs_pri_thd1 = 0, dfs_pri_thd2 = 0, dfs_pri_thd3 = 0;
	u8 dfs_pri_thd4 = 0, dfs_pri_thd5 = 0;
	u8 pri_th = 0, i = 0;
	u8 max_pri_idx = 0, max_pw_idx = 0, max_pri_cnt_th = 0;
	u8 max_pri_cnt_fcc_g1_th = 0, max_pri_cnt_fcc_g3_th = 0;
	u8 safe_pri_pw_diff_th = 0, safe_pri_pw_diff_fcc_th = 0;
	u8 safe_pri_pw_diff_w53_th = 0, safe_pri_pw_diff_fcc_idle_th = 0;
	u16 j = 0;
	u32 dfs_hist1_peak_index = 0, dfs_hist2_peak_index = 0;
	u32 dfs_hist1_pw = 0, dfs_hist2_pw = 0, g_pw[6] = {0};
	u32 g_peakindex[16] = {0}, g_mask_32 = 0, false_peak_hist1 = 0;
	u32 false_peak_hist2_above10 = 0, false_peak_hist2_above0 = 0;
	u32 dfs_hist1_pri = 0, dfs_hist2_pri = 0, g_pri[6] = {0};
	u32 pw_sum_g0g5 = 0, pw_sum_g1g2g3g4 = 0;
	u32 pri_sum_g0g5 = 0, pri_sum_g1g2g3g4 = 0;
	u32 pw_sum_ss_g1g2g3g4 = 0, pri_sum_ss_g1g2g3g4 = 0;
	u32 max_pri_cnt = 0, max_pw_cnt = 0;

	/*read peak index hist report*/
	odm_set_bb_reg(dm, 0x19e4, BIT(22) | BIT(23), 0x0);
	dfs_hist1_peak_index = odm_get_bb_reg(dm, 0xf5c, 0xffffffff);
	dfs_hist2_peak_index = odm_get_bb_reg(dm, 0xf74, 0xffffffff);

	g_peakindex[15] = ((dfs_hist1_peak_index & 0x0000000f) >> 0);
	g_peakindex[14] = ((dfs_hist1_peak_index & 0x000000f0) >> 4);
	g_peakindex[13] = ((dfs_hist1_peak_index & 0x00000f00) >> 8);
	g_peakindex[12] = ((dfs_hist1_peak_index & 0x0000f000) >> 12);
	g_peakindex[11] = ((dfs_hist1_peak_index & 0x000f0000) >> 16);
	g_peakindex[10] = ((dfs_hist1_peak_index & 0x00f00000) >> 20);
	g_peakindex[9] = ((dfs_hist1_peak_index & 0x0f000000) >> 24);
	g_peakindex[8] = ((dfs_hist1_peak_index & 0xf0000000) >> 28);
	g_peakindex[7] = ((dfs_hist2_peak_index & 0x0000000f) >> 0);
	g_peakindex[6] = ((dfs_hist2_peak_index & 0x000000f0) >> 4);
	g_peakindex[5] = ((dfs_hist2_peak_index & 0x00000f00) >> 8);
	g_peakindex[4] = ((dfs_hist2_peak_index & 0x0000f000) >> 12);
	g_peakindex[3] = ((dfs_hist2_peak_index & 0x000f0000) >> 16);
	g_peakindex[2] = ((dfs_hist2_peak_index & 0x00f00000) >> 20);
	g_peakindex[1] = ((dfs_hist2_peak_index & 0x0f000000) >> 24);
	g_peakindex[0] = ((dfs_hist2_peak_index & 0xf0000000) >> 28);

	/*read pulse width hist report*/
	odm_set_bb_reg(dm, 0x19e4, BIT(22) | BIT(23), 0x1);
	dfs_hist1_pw = odm_get_bb_reg(dm, 0xf5c, 0xffffffff);
	dfs_hist2_pw = odm_get_bb_reg(dm, 0xf74, 0xffffffff);

	g_pw[0] = (unsigned int)((dfs_hist2_pw & 0xff000000) >> 24);
	g_pw[1] = (unsigned int)((dfs_hist2_pw & 0x00ff0000) >> 16);
	g_pw[2] = (unsigned int)((dfs_hist2_pw & 0x0000ff00) >> 8);
	g_pw[3] = (unsigned int)dfs_hist2_pw & 0x000000ff;
	g_pw[4] = (unsigned int)((dfs_hist1_pw & 0xff000000) >> 24);
	g_pw[5] = (unsigned int)((dfs_hist1_pw & 0x00ff0000) >> 16);

	/*read pulse repetition interval hist report*/
	odm_set_bb_reg(dm, 0x19e4, BIT(22) | BIT(23), 0x3);
	dfs_hist1_pri = odm_get_bb_reg(dm, 0xf5c, 0xffffffff);
	dfs_hist2_pri = odm_get_bb_reg(dm, 0xf74, 0xffffffff);
	odm_set_bb_reg(dm, 0x19b4, 0x10000000, 1); /*reset histo report*/
	odm_set_bb_reg(dm, 0x19b4, 0x10000000, 0); /*@continue histo report*/

	g_pri[0] = (unsigned int)((dfs_hist2_pri & 0xff000000) >> 24);
	g_pri[1] = (unsigned int)((dfs_hist2_pri & 0x00ff0000) >> 16);
	g_pri[2] = (unsigned int)((dfs_hist2_pri & 0x0000ff00) >> 8);
	g_pri[3] = (unsigned int)dfs_hist2_pri & 0x000000ff;
	g_pri[4] = (unsigned int)((dfs_hist1_pri & 0xff000000) >> 24);
	g_pri[5] = (unsigned int)((dfs_hist1_pri & 0x00ff0000) >> 16);

	dfs->pri_cond1 = 0;
	dfs->pri_cond2 = 0;
	dfs->pri_cond3 = 0;
	dfs->pri_cond4 = 0;
	dfs->pri_cond5 = 0;
	dfs->pw_cond1 = 0;
	dfs->pw_cond2 = 0;
	dfs->pw_cond3 = 0;
	dfs->pri_type3_4_cond1 = 0;	/*@for ETSI*/
	dfs->pri_type3_4_cond2 = 0;	/*@for ETSI*/
	dfs->pw_long_cond1 = 0;		/*@for long radar*/
	dfs->pw_long_cond2 = 0;		/*@for long radar*/
	dfs->pri_long_cond1 = 0;	/*@for long radar*/
	dfs->pw_flag = 0;
	dfs->pri_flag = 0;
	dfs->pri_type3_4_flag = 0;	/*@for ETSI*/
	dfs->long_radar_flag = 0;
	dfs->pw_std = 0;	/*The std(var) of reasonable num of pw group*/
	dfs->pri_std = 0;	/*The std(var) of reasonable num of pri group*/

	for (i = 0; i < 6; i++) {
		dfs->pw_hold_sum[i] = 0;
		dfs->pri_hold_sum[i] = 0;
		dfs->pw_long_hold_sum[i] = 0;
		dfs->pri_long_hold_sum[i] = 0;
	}

	if (dfs->idle_mode == 1)
		pri_th = dfs->pri_hist_th;
	else
		pri_th = dfs->pri_hist_th - 1;

	for (i = 0; i < 6; i++) {
		dfs->pw_hold[dfs->hist_idx][i] = (u8)g_pw[i];
		dfs->pri_hold[dfs->hist_idx][i] = (u8)g_pri[i];
		/*@collect whole histogram report may take some time
		 *so we add the counter of 2 time slots in FCC and ETSI
		 */
		if (region_domain == 1 || region_domain == 3) {
			dfs->pw_hold_sum[i] = dfs->pw_hold_sum[i] +
				dfs->pw_hold[(dfs->hist_idx + 1) % 3][i] +
				dfs->pw_hold[(dfs->hist_idx + 2) % 3][i];
			dfs->pri_hold_sum[i] = dfs->pri_hold_sum[i] +
				dfs->pri_hold[(dfs->hist_idx + 1) % 3][i] +
				dfs->pri_hold[(dfs->hist_idx + 2) % 3][i];
		} else{
		/*@collect whole histogram report may take some time,
		 *so we add the counter of 3 time slots in MKK or else
		 */
			dfs->pw_hold_sum[i] = dfs->pw_hold_sum[i] +
				dfs->pw_hold[(dfs->hist_idx + 1) % 4][i] +
				dfs->pw_hold[(dfs->hist_idx + 2) % 4][i] +
				dfs->pw_hold[(dfs->hist_idx + 3) % 4][i];
			dfs->pri_hold_sum[i] = dfs->pri_hold_sum[i] +
				dfs->pri_hold[(dfs->hist_idx + 1) % 4][i] +
				dfs->pri_hold[(dfs->hist_idx + 2) % 4][i] +
				dfs->pri_hold[(dfs->hist_idx + 3) % 4][i];
		}
	}
	/*@For long radar type*/
	for (i = 0; i < 6; i++) {
		dfs->pw_long_hold[dfs->hist_long_idx][i] = (u8)g_pw[i];
		dfs->pri_long_hold[dfs->hist_long_idx][i] = (u8)g_pri[i];
		/*@collect whole histogram report may take some time,
		 *so we add the counter of 299 time slots for long radar
		 */
		for (j = 1; j < 300; j++) {
		dfs->pw_long_hold_sum[i] = dfs->pw_long_hold_sum[i] +
			dfs->pw_long_hold[(dfs->hist_long_idx + j) % 300][i];
		dfs->pri_long_hold_sum[i] = dfs->pri_long_hold_sum[i] +
			dfs->pri_long_hold[(dfs->hist_long_idx + j) % 300][i];
		}
	}
	dfs->hist_idx++;
	dfs->hist_long_idx++;
	if (dfs->hist_long_idx == 300)
		dfs->hist_long_idx = 0;
	if (region_domain == 1 || region_domain == 3) {
		if (dfs->hist_idx == 3)
			dfs->hist_idx = 0;
	} else if (dfs->hist_idx == 4) {
		dfs->hist_idx = 0;
	}

	max_pri_cnt = 0;
	max_pri_idx = 0;
	max_pw_cnt = 0;
	max_pw_idx = 0;
	max_pri_cnt_th = dfs->pri_sum_g1_th;
	max_pri_cnt_fcc_g1_th = dfs->pri_sum_g1_fcc_th;
	max_pri_cnt_fcc_g3_th = dfs->pri_sum_g3_fcc_th;
	safe_pri_pw_diff_th = dfs->pri_pw_diff_th;
	safe_pri_pw_diff_fcc_th = dfs->pri_pw_diff_fcc_th;
	safe_pri_pw_diff_fcc_idle_th = dfs->pri_pw_diff_fcc_idle_th;
	safe_pri_pw_diff_w53_th = dfs->pri_pw_diff_w53_th;

	/*@g1 to g4 is the reseasonable range of pri and pw*/
	for (i = 1; i <= 4; i++) {
		if (dfs->pri_hold_sum[i] > max_pri_cnt) {
			max_pri_cnt = dfs->pri_hold_sum[i];
			max_pri_idx = i;
		}
		if (dfs->pw_hold_sum[i] > max_pw_cnt) {
			max_pw_cnt = dfs->pw_hold_sum[i];
			max_pw_idx = i;
		}
		if (dfs->pri_hold_sum[i] >= pri_th)
			dfs->pri_cond1 = 1;
	}

	pri_sum_g0g5 = dfs->pri_hold_sum[0];
	if (pri_sum_g0g5 == 0)
		pri_sum_g0g5 = 1;
	pri_sum_g1g2g3g4 = dfs->pri_hold_sum[1] + dfs->pri_hold_sum[2]
			 + dfs->pri_hold_sum[3] + dfs->pri_hold_sum[4];

	/*pw will reduce because of dc, so we do not treat g0 as illegal group*/
	pw_sum_g0g5 = dfs->pw_hold_sum[5];
	if (pw_sum_g0g5 == 0)
		pw_sum_g0g5 = 1;
	pw_sum_g1g2g3g4 = dfs->pw_hold_sum[1] + dfs->pw_hold_sum[2] +
				dfs->pw_hold_sum[3] + dfs->pw_hold_sum[4];

	/*@Calculate the variation from g1 to g4*/
	for (i = 1; i < 5; i++) {
		/*Sum of square*/
		pw_sum_ss_g1g2g3g4 = pw_sum_ss_g1g2g3g4 +
		(dfs->pw_hold_sum[i] - (pw_sum_g1g2g3g4 / 4)) *
		(dfs->pw_hold_sum[i] - (pw_sum_g1g2g3g4 / 4));
		pri_sum_ss_g1g2g3g4 = pri_sum_ss_g1g2g3g4 +
		(dfs->pri_hold_sum[i] - (pri_sum_g1g2g3g4 / 4)) *
		(dfs->pri_hold_sum[i] - (pri_sum_g1g2g3g4 / 4));
	}
	/*The value may less than the normal variance,
	 *since the variable type is int (not float)
	 */
		dfs->pw_std = (u16)(pw_sum_ss_g1g2g3g4 / 4);
		dfs->pri_std = (u16)(pri_sum_ss_g1g2g3g4 / 4);

	if (region_domain == 1) {
		dfs->pri_type3_4_flag = 1;	/*@ETSI flag*/

		/*PRI judgment conditions for short radar type*/
		/*ratio of reasonable group and illegal group &&
		 *pri variation of short radar should be large (=6)
		 */
		if (max_pri_idx != 4 && dfs->pri_hold_sum[5] > 0)
			dfs->pri_cond2 = 0;
		else
			dfs->pri_cond2 = 1;

		/*reasonable group shouldn't large*/
		if ((pri_sum_g0g5 + pri_sum_g1g2g3g4) / pri_sum_g0g5 > 2 &&
		    pri_sum_g1g2g3g4 <= dfs->pri_sum_safe_fcc_th)
			dfs->pri_cond3 = 1;

		/*@Cancel the condition that the abs between pri and pw*/
		if (dfs->pri_std >= dfs->pri_std_th)
			dfs->pri_cond4 = 1;
		else if (max_pri_idx == 1 &&
			 max_pri_cnt >= max_pri_cnt_fcc_g1_th)
			dfs->pri_cond4 = 1;

		/*we set threshold = 7 (>4) for distinguishing type 3,4 (g3)*/
		if (max_pri_idx == 1 && dfs->pri_hold_sum[3] +
		    dfs->pri_hold_sum[4] + dfs->pri_hold_sum[5] > 0)
			dfs->pri_cond5 = 0;
		else
			dfs->pri_cond5 = 1;

		if (dfs->pri_cond1 && dfs->pri_cond2 && dfs->pri_cond3 &&
		    dfs->pri_cond4 && dfs->pri_cond5)
			dfs->pri_flag = 1;

		/* PW judgment conditions for short radar type */
		/*ratio of reasonable and illegal group && g5 should be zero*/
		if (((pw_sum_g0g5 + pw_sum_g1g2g3g4) / pw_sum_g0g5 > 2) &&
		    (dfs->pw_hold_sum[5] <= 1))
			dfs->pw_cond1 = 1;
		/*unreasonable group*/
		if (dfs->pw_hold_sum[4] == 0 && dfs->pw_hold_sum[5] == 0)
			dfs->pw_cond2 = 1;
		/*pw's std (short radar) should be large(=7)*/
		if (dfs->pw_std >= dfs->pw_std_th)
			dfs->pw_cond3 = 1;
		if (dfs->pw_cond1 && dfs->pw_cond2 && dfs->pw_cond3)
			dfs->pw_flag = 1;

		/* @Judgment conditions of long radar type */
		if (band_width == CHANNEL_WIDTH_20) {
			if (dfs->pw_long_hold_sum[4] >=
			    dfs->pw_long_lower_20m_th)
				dfs->pw_long_cond1 = 1;
		} else{
			if (dfs->pw_long_hold_sum[4] >= dfs->pw_long_lower_th)
				dfs->pw_long_cond1 = 1;
		}
		/* @Disable the condition that dfs->pw_long_hold_sum[1] */
		if (dfs->pw_long_hold_sum[2] + dfs->pw_long_hold_sum[3] +
		    dfs->pw_long_hold_sum[4] <= dfs->pw_long_sum_upper_th &&
		    dfs->pw_long_hold_sum[2] <= dfs->pw_long_hold_sum[4] &&
		    dfs->pw_long_hold_sum[3] <= dfs->pw_long_hold_sum[4])
			dfs->pw_long_cond2 = 1;
		/*@g4 should be large for long radar*/
		if (dfs->pri_long_hold_sum[4] <= dfs->pri_long_upper_th)
			dfs->pri_long_cond1 = 1;
		if (dfs->pw_long_cond1 && dfs->pw_long_cond2 &&
		    dfs->pri_long_cond1)
			dfs->long_radar_flag = 1;
	} else if (region_domain == 2) {
		dfs->pri_type3_4_flag = 1;	/*@ETSI flag*/

		/*PRI judgment conditions for short radar type*/
		if ((pri_sum_g0g5 + pri_sum_g1g2g3g4) / pri_sum_g0g5 > 2)
			dfs->pri_cond2 = 1;

		/*reasonable group shouldn't too large*/
		if (pri_sum_g1g2g3g4 <= dfs->pri_sum_safe_fcc_th)
			dfs->pri_cond3 = 1;

		/*the difference between pri and pw for idle mode (thr=2)*/
		if (dfs->idle_mode == 1) {
			if (abs(pw_sum_g1g2g3g4 - pri_sum_g1g2g3g4) <=
			    safe_pri_pw_diff_fcc_idle_th)
				dfs->pri_cond4 = 1;
		} else{
			if ((c_channel >= 52) && (c_channel <= 64)) {
		/*the difference between pri and pw for w53 TP mode (thr=15)*/
			if (abs(pw_sum_g1g2g3g4 - pri_sum_g1g2g3g4) <=
			    safe_pri_pw_diff_w53_th)
				dfs->pri_cond4 = 1;
			} else {
		/*the difference between pri and pw for TP mode (thr=8)*/
				if (abs(pw_sum_g1g2g3g4 - pri_sum_g1g2g3g4) <=
				    safe_pri_pw_diff_fcc_th)
					dfs->pri_cond4 = 1;
			}
		}

		if (dfs->idle_mode == 1) {
			if (dfs->pri_std >= dfs->pri_std_idle_th) {
				if (max_pw_idx == 3 &&
				    pri_sum_g1g2g3g4 <= dfs->pri_sum_type4_th){
		/*To distinguish between type 4 radar and false detection*/
					dfs->pri_cond5 = 1;
				} else if (max_pw_idx == 1 &&
					   pri_sum_g1g2g3g4 >=
					   dfs->pri_sum_type6_th) {
		/*To distinguish between type 6 radar and false detection*/
					dfs->pri_cond5 = 1;
				} else {
		/*pri variation of short radar should be large (idle mode)*/
					dfs->pri_cond5 = 1;
				}
			}
		} else {
		/*pri variation of short radar should be large (TP mode)*/
			if (dfs->pri_std >= dfs->pri_std_th)
				dfs->pri_cond5 = 1;
		}

		if (dfs->pri_cond1 && dfs->pri_cond2 && dfs->pri_cond3 &&
		    dfs->pri_cond4 && dfs->pri_cond5)
			dfs->pri_flag = 1;

		/* PW judgment conditions for short radar type */
		if (((pw_sum_g0g5 + pw_sum_g1g2g3g4) / pw_sum_g0g5 > 2) &&
		    (dfs->pw_hold_sum[5] <= 1))
		/*ratio of reasonable and illegal group && g5 should be zero*/
			dfs->pw_cond1 = 1;

		if ((c_channel >= 52) && (c_channel <= 64))
			dfs->pw_cond2 = 1;
		/*unreasonable group shouldn't too large*/
		else if (dfs->pw_hold_sum[0] <= dfs->pw_g0_th)
			dfs->pw_cond2 = 1;

		if (dfs->idle_mode == 1) {
		/*pw variation of short radar should be large (idle mode)*/
			if (dfs->pw_std >= dfs->pw_std_idle_th)
				dfs->pw_cond3 = 1;
		} else {
		/*pw variation of short radar should be large (TP mode)*/
			if (dfs->pw_std >= dfs->pw_std_th)
				dfs->pw_cond3 = 1;
		}
		if (dfs->pw_cond1 && dfs->pw_cond2 && dfs->pw_cond3)
			dfs->pw_flag = 1;

		/* @Judgment conditions of long radar type */
		if (band_width == CHANNEL_WIDTH_20) {
			if (dfs->pw_long_hold_sum[4] >=
			    dfs->pw_long_lower_20m_th)
				dfs->pw_long_cond1 = 1;
		} else{
			if (dfs->pw_long_hold_sum[4] >= dfs->pw_long_lower_th)
				dfs->pw_long_cond1 = 1;
		}
		if (dfs->pw_long_hold_sum[1] + dfs->pw_long_hold_sum[2] +
		    dfs->pw_long_hold_sum[3] + dfs->pw_long_hold_sum[4]
		    <= dfs->pw_long_sum_upper_th)
			dfs->pw_long_cond2 = 1;
		/*@g4 should be large for long radar*/
		if (dfs->pri_long_hold_sum[4] <= dfs->pri_long_upper_th)
			dfs->pri_long_cond1 = 1;
		if (dfs->pw_long_cond1 &&
		    dfs->pw_long_cond2 && dfs->pri_long_cond1)
			dfs->long_radar_flag = 1;
	} else if (region_domain == 3) {
		/*ratio of reasonable group and illegal group */
		if ((pri_sum_g0g5 + pri_sum_g1g2g3g4) / pri_sum_g0g5 > 2)
			dfs->pri_cond2 = 1;

		if (pri_sum_g1g2g3g4 <= dfs->pri_sum_safe_th)
			dfs->pri_cond3 = 1;

		/*@Cancel the condition that the abs between pri and pw*/
			dfs->pri_cond4 = 1;

		if (dfs->pri_hold_sum[5] <= dfs->pri_sum_g5_th)
			dfs->pri_cond5 = 1;

		if (band_width == CHANNEL_WIDTH_40) {
			if (max_pw_idx == 4) {
				if (max_pw_cnt >= dfs->type4_pw_max_cnt &&
				    pri_sum_g1g2g3g4 >=
				    dfs->type4_safe_pri_sum_th) {
					dfs->pri_cond1 = 1;
					dfs->pri_cond4 = 1;
					dfs->pri_type3_4_cond1 = 1;
				}
			}
		}

		if (dfs->pri_cond1 && dfs->pri_cond2 &&
		    dfs->pri_cond3 && dfs->pri_cond4 && dfs->pri_cond5)
			dfs->pri_flag = 1;

		if (((pw_sum_g0g5 + pw_sum_g1g2g3g4) / pw_sum_g0g5 > 2) &&
		    (dfs->pw_hold_sum[5] == 0))
			dfs->pw_flag = 1;

		/*@max num pri group is g1 means radar type3 or type4*/
		if (max_pri_idx == 1) {
			if (max_pri_cnt >= max_pri_cnt_th)
				dfs->pri_type3_4_cond1 = 1;
			if (dfs->pri_hold_sum[4] <=
			    dfs->pri_sum_g5_under_g1_th &&
			    dfs->pri_hold_sum[5] <= dfs->pri_sum_g5_under_g1_th)
				dfs->pri_type3_4_cond2 = 1;
		} else {
			dfs->pri_type3_4_cond1 = 1;
			dfs->pri_type3_4_cond2 = 1;
		}
		if (dfs->pri_type3_4_cond1 && dfs->pri_type3_4_cond2)
			dfs->pri_type3_4_flag = 1;
	} else {
	}

	if (dfs->print_hist_rpt) {
		dfs_pw_thd1 = (u8)odm_get_bb_reg(dm, 0x19e4, 0xff000000);
		dfs_pw_thd2 = (u8)odm_get_bb_reg(dm, 0x19e8, 0x000000ff);
		dfs_pw_thd3 = (u8)odm_get_bb_reg(dm, 0x19e8, 0x0000ff00);
		dfs_pw_thd4 = (u8)odm_get_bb_reg(dm, 0x19e8, 0x00ff0000);
		dfs_pw_thd5 = (u8)odm_get_bb_reg(dm, 0x19e8, 0xff000000);

		dfs_pri_thd1 = (u8)odm_get_bb_reg(dm, 0x19b8, 0x7F80);
		dfs_pri_thd2 = (u8)odm_get_bb_reg(dm, 0x19ec, 0x000000ff);
		dfs_pri_thd3 = (u8)odm_get_bb_reg(dm, 0x19ec, 0x0000ff00);
		dfs_pri_thd4 = (u8)odm_get_bb_reg(dm, 0x19ec, 0x00ff0000);
		dfs_pri_thd5 = (u8)odm_get_bb_reg(dm, 0x19ec, 0xff000000);

		PHYDM_DBG(dm, DBG_DFS, "peak index hist\n");
		PHYDM_DBG(dm, DBG_DFS, "dfs_hist_peak_index=%x %x\n",
			  dfs_hist1_peak_index, dfs_hist2_peak_index);
		PHYDM_DBG(dm, DBG_DFS, "g_peak_index_hist = ");
		for (i = 0; i < 16; i++)
			PHYDM_DBG(dm, DBG_DFS, " %x", g_peakindex[i]);
		PHYDM_DBG(dm, DBG_DFS, "\ndfs_pw_thd=%d %d %d %d %d\n",
			  dfs_pw_thd1, dfs_pw_thd2, dfs_pw_thd3,
			  dfs_pw_thd4, dfs_pw_thd5);
		PHYDM_DBG(dm, DBG_DFS, "-----pulse width hist-----\n");
		PHYDM_DBG(dm, DBG_DFS, "dfs_hist_pw=%x %x\n",
			  dfs_hist1_pw, dfs_hist2_pw);
		PHYDM_DBG(dm, DBG_DFS, "g_pw_hist = %x %x %x %x %x %x\n",
			  g_pw[0], g_pw[1], g_pw[2], g_pw[3],
			  g_pw[4], g_pw[5]);
		PHYDM_DBG(dm, DBG_DFS, "dfs_pri_thd=%d %d %d %d %d\n",
			  dfs_pri_thd1, dfs_pri_thd2, dfs_pri_thd3,
			  dfs_pri_thd4, dfs_pri_thd5);
		PHYDM_DBG(dm, DBG_DFS, "-----pulse interval hist-----\n");
		PHYDM_DBG(dm, DBG_DFS, "dfs_hist_pri=%x %x\n",
			  dfs_hist1_pri, dfs_hist2_pri);
		PHYDM_DBG(dm, DBG_DFS,
			  "g_pri_hist = %x %x %x %x %x %x, pw_flag = %d, pri_flag = %d\n",
			  g_pri[0], g_pri[1], g_pri[2], g_pri[3], g_pri[4],
			  g_pri[5], dfs->pw_flag, dfs->pri_flag);
		if (region_domain == 1 || region_domain == 3) {
			PHYDM_DBG(dm, DBG_DFS, "hist_idx= %d\n",
				  (dfs->hist_idx + 2) % 3);
		} else {
			PHYDM_DBG(dm, DBG_DFS, "hist_idx= %d\n",
				  (dfs->hist_idx + 3) % 4);
		}
		PHYDM_DBG(dm, DBG_DFS, "hist_long_idx= %d\n",
			  (dfs->hist_long_idx + 299) % 300);
		PHYDM_DBG(dm, DBG_DFS,
			  "pw_sum_g0g5 = %d, pw_sum_g1g2g3g4 = %d\n",
			  pw_sum_g0g5, pw_sum_g1g2g3g4);
		PHYDM_DBG(dm, DBG_DFS,
			  "pri_sum_g0g5 = %d, pri_sum_g1g2g3g4 = %d\n",
			  pri_sum_g0g5, pri_sum_g1g2g3g4);
		PHYDM_DBG(dm, DBG_DFS, "pw_hold_sum = %d %d %d %d %d %d\n",
			  dfs->pw_hold_sum[0], dfs->pw_hold_sum[1],
			  dfs->pw_hold_sum[2], dfs->pw_hold_sum[3],
			  dfs->pw_hold_sum[4], dfs->pw_hold_sum[5]);
		PHYDM_DBG(dm, DBG_DFS, "pri_hold_sum = %d %d %d %d %d %d\n",
			  dfs->pri_hold_sum[0], dfs->pri_hold_sum[1],
			  dfs->pri_hold_sum[2], dfs->pri_hold_sum[3],
			  dfs->pri_hold_sum[4], dfs->pri_hold_sum[5]);
		PHYDM_DBG(dm, DBG_DFS, "pw_long_hold_sum = %d %d %d %d %d %d\n",
			  dfs->pw_long_hold_sum[0], dfs->pw_long_hold_sum[1],
			  dfs->pw_long_hold_sum[2], dfs->pw_long_hold_sum[3],
			  dfs->pw_long_hold_sum[4], dfs->pw_long_hold_sum[5]);
		PHYDM_DBG(dm, DBG_DFS,
			  "pri_long_hold_sum = %d %d %d %d %d %d\n",
			  dfs->pri_long_hold_sum[0], dfs->pri_long_hold_sum[1],
			  dfs->pri_long_hold_sum[2], dfs->pri_long_hold_sum[3],
			  dfs->pri_long_hold_sum[4], dfs->pri_long_hold_sum[5]);
		PHYDM_DBG(dm, DBG_DFS, "idle_mode = %d\n", dfs->idle_mode);
		PHYDM_DBG(dm, DBG_DFS, "pw_standard = %d\n", dfs->pw_std);
		PHYDM_DBG(dm, DBG_DFS, "pri_standard = %d\n", dfs->pri_std);
		for (j = 0; j < 4; j++) {
			for (i = 0; i < 6; i++) {
				PHYDM_DBG(dm, DBG_DFS, "pri_hold = %d ",
					  dfs->pri_hold[j][i]);
			}
			PHYDM_DBG(dm, DBG_DFS, "\n");
		}
		PHYDM_DBG(dm, DBG_DFS, "\n");
		PHYDM_DBG(dm, DBG_DFS,
			  "pri_cond1 = %d, pri_cond2 = %d, pri_cond3 = %d, pri_cond4 = %d, pri_cond5 = %d\n",
			  dfs->pri_cond1, dfs->pri_cond2, dfs->pri_cond3,
			  dfs->pri_cond4, dfs->pri_cond5);
		PHYDM_DBG(dm, DBG_DFS,
			  "bandwidth = %d, pri_th = %d, max_pri_cnt_th = %d, safe_pri_pw_diff_th = %d\n",
			  band_width, pri_th, max_pri_cnt_th,
			  safe_pri_pw_diff_th);
	}
}

boolean phydm_dfs_hist_log(void *dm_void, u8 index)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u8 i = 0, j = 0;
	boolean hist_radar_detected = 0;

	if (dfs->pulse_type_hist[index] == 0) {
		dfs->radar_type = 0;
		if (dfs->pw_flag && dfs->pri_flag &&
		    dfs->pri_type3_4_flag) {
			hist_radar_detected = 1;
			PHYDM_DBG(dm, DBG_DFS,
				  "Detected type %d radar signal!\n",
				  dfs->radar_type);
			if (dfs->det_print2) {
				PHYDM_DBG(dm, DBG_DFS,
					  "hist_idx= %d\n",
					  (dfs->hist_idx + 3) % 4);
				for (j = 0; j < 4; j++) {
				for (i = 0; i < 6; i++) {
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_hold = %d ",
						  dfs->pri_hold[j][i]);
				}
				PHYDM_DBG(dm, DBG_DFS, "\n");
				}
				PHYDM_DBG(dm, DBG_DFS, "\n");
				for (j = 0; j < 4; j++) {
				for (i = 0; i < 6; i++) {
					PHYDM_DBG(dm, DBG_DFS, "pw_hold = %d ",
						  dfs->pw_hold[j][i]);
				}
					PHYDM_DBG(dm, DBG_DFS, "\n");
				}
				PHYDM_DBG(dm, DBG_DFS, "\n");
				PHYDM_DBG(dm, DBG_DFS, "idle_mode = %d\n",
					  dfs->idle_mode);
				PHYDM_DBG(dm, DBG_DFS,
					  "pw_hold_sum = %d %d %d %d %d %d\n",
					  dfs->pw_hold_sum[0],
					  dfs->pw_hold_sum[1],
					  dfs->pw_hold_sum[2],
					  dfs->pw_hold_sum[3],
					  dfs->pw_hold_sum[4],
					  dfs->pw_hold_sum[5]);
				PHYDM_DBG(dm, DBG_DFS,
					  "pri_hold_sum = %d %d %d %d %d %d\n",
					  dfs->pri_hold_sum[0],
					  dfs->pri_hold_sum[1],
					  dfs->pri_hold_sum[2],
					  dfs->pri_hold_sum[3],
					  dfs->pri_hold_sum[4],
					  dfs->pri_hold_sum[5]);
			}
		} else {
		if (dfs->det_print2) {
			if (dfs->pulse_flag_hist[index] &&
			    dfs->pri_flag == 0) {
				PHYDM_DBG(dm, DBG_DFS, "pri_variation = %d\n",
					  dfs->pri_std);
				PHYDM_DBG(dm, DBG_DFS,
					  "PRI criterion is not satisfied!\n");
				if (dfs->pri_cond1 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_cond1 is not satisfied!\n");
				if (dfs->pri_cond2 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_cond2 is not satisfied!\n");
				if (dfs->pri_cond3 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_cond3 is not satisfied!\n");
				if (dfs->pri_cond4 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_cond4 is not satisfied!\n");
				if (dfs->pri_cond5 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_cond5 is not satisfied!\n");
			}
			if (dfs->pulse_flag_hist[index] &&
			    dfs->pw_flag == 0) {
				PHYDM_DBG(dm, DBG_DFS, "pw_variation = %d\n",
					  dfs->pw_std);
				PHYDM_DBG(dm, DBG_DFS,
					  "PW criterion is not satisfied!\n");
				if (dfs->pw_cond1 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pw_cond1 is not satisfied!\n");
				if (dfs->pw_cond2 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pw_cond2 is not satisfied!\n");
				if (dfs->pw_cond3 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pw_cond3 is not satisfied!\n");
			}
			if (dfs->pulse_flag_hist[index] &&
			    (dfs->pri_type3_4_flag == 0)) {
				PHYDM_DBG(dm, DBG_DFS,
					  "pri_type3_4 criterion is not satisfied!\n");
				if (dfs->pri_type3_4_cond1 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_type3_4_cond1 is not satisfied!\n");
				if (dfs->pri_type3_4_cond2 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_type3_4_cond2 is not satisfied!\n");
			}
			PHYDM_DBG(dm, DBG_DFS, "hist_idx= %d\n",
				  (dfs->hist_idx + 3) % 4);
			for (j = 0; j < 4; j++) {
				for (i = 0; i < 6; i++) {
					PHYDM_DBG(dm, DBG_DFS,
						  "pri_hold = %d ",
						  dfs->pri_hold[j][i]);
				}
				PHYDM_DBG(dm, DBG_DFS, "\n");
			}
			PHYDM_DBG(dm, DBG_DFS, "\n");
			for (j = 0; j < 4; j++) {
				for (i = 0; i < 6; i++)
					PHYDM_DBG(dm, DBG_DFS,
						  "pw_hold = %d ",
						  dfs->pw_hold[j][i]);
				PHYDM_DBG(dm, DBG_DFS, "\n");
			}
			PHYDM_DBG(dm, DBG_DFS, "\n");
			PHYDM_DBG(dm, DBG_DFS, "idle_mode = %d\n",
				  dfs->idle_mode);
			PHYDM_DBG(dm, DBG_DFS,
				  "pw_hold_sum = %d %d %d %d %d %d\n",
				  dfs->pw_hold_sum[0], dfs->pw_hold_sum[1],
				  dfs->pw_hold_sum[2], dfs->pw_hold_sum[3],
				  dfs->pw_hold_sum[4], dfs->pw_hold_sum[5]);
			PHYDM_DBG(dm, DBG_DFS,
				  "pri_hold_sum = %d %d %d %d %d %d\n",
				  dfs->pri_hold_sum[0], dfs->pri_hold_sum[1],
				  dfs->pri_hold_sum[2], dfs->pri_hold_sum[3],
				  dfs->pri_hold_sum[4], dfs->pri_hold_sum[5]);
		}
		}
	} else {
		dfs->radar_type = 1;
		if (dfs->det_print2) {
			PHYDM_DBG(dm, DBG_DFS, "\n");
			PHYDM_DBG(dm, DBG_DFS, "idle_mode = %d\n",
				  dfs->idle_mode);
			PHYDM_DBG(dm, DBG_DFS,
				  "long_radar_pw_hold_sum = %d %d %d %d %d %d\n",
				  dfs->pw_long_hold_sum[0],
				  dfs->pw_long_hold_sum[1],
				  dfs->pw_long_hold_sum[2],
				  dfs->pw_long_hold_sum[3],
				  dfs->pw_long_hold_sum[4],
				  dfs->pw_long_hold_sum[5]);
			PHYDM_DBG(dm, DBG_DFS,
				  "long_radar_pri_hold_sum = %d %d %d %d %d %d\n",
				  dfs->pri_long_hold_sum[0],
				  dfs->pri_long_hold_sum[1],
				  dfs->pri_long_hold_sum[2],
				  dfs->pri_long_hold_sum[3],
				  dfs->pri_long_hold_sum[4],
				  dfs->pri_long_hold_sum[5]);
		}
		/* @Long radar should satisfy three conditions */
		if (dfs->long_radar_flag == 1) {
			hist_radar_detected = 1;
			PHYDM_DBG(dm, DBG_DFS,
				  "Detected type %d radar signal!\n",
				  dfs->radar_type);
		} else {
			if (dfs->det_print2) {
				if (dfs->pw_long_cond1 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "--pw_long_cond1 is not satisfied!--\n");
				if (dfs->pw_long_cond2 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "--pw_long_cond2 is not satisfied!--\n");
				if (dfs->pri_long_cond1 == 0)
					PHYDM_DBG(dm, DBG_DFS,
						  "--pri_long_cond1 is not satisfied!--\n");
			}
		}
	}
	return hist_radar_detected;
}

boolean phydm_radar_detect(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	boolean enable_DFS = false;
	boolean radar_detected = false;

	dfs->igi_cur = (u8)odm_get_bb_reg(dm, R_0xc50, 0x0000007f);

	dfs->st_l2h_cur = (u8)odm_get_bb_reg(dm, R_0x91c, 0x000000ff);

	/* @dynamic pwdb calibration */
	if (dfs->igi_pre != dfs->igi_cur) {
		dfs->pwdb_th_cur = ((int)dfs->st_l2h_cur - (int)dfs->igi_cur)
				    / 2 + dfs->pwdb_scalar_factor;

		/* @limit the pwdb value to absolute lower bound 0xa */
		dfs->pwdb_th_cur = MAX_2(dfs->pwdb_th_cur, (int)dfs->pwdb_th);
		/* @limit the pwdb value to absolute upper bound 0x1f */
		dfs->pwdb_th_cur = MIN_2(dfs->pwdb_th_cur, 0x1f);
		odm_set_bb_reg(dm, R_0x918, 0x00001f00, dfs->pwdb_th_cur);
	}

	dfs->igi_pre = dfs->igi_cur;

	phydm_dfs_dynamic_setting(dm);
	phydm_dfs_histogram_radar_distinguish(dm);
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

void phydm_dfs_debug(void *dm_void, char input[][16], u32 *_used,
		     char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _DFS_STATISTICS *dfs = &dm->dfs;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 argv[10] = {0};
	u8 i, input_idx = 0;

	for (i = 0; i < 6; i++) {
		if (input[i + 1]) {
			PHYDM_SSCANF(input[i + 1], DCMD_HEX, &argv[i]);
			input_idx++;
		}
	}

	if (input_idx == 0)
		return;

	dfs->dbg_mode = (boolean)argv[0];
	dfs->force_TP_mode = (boolean)argv[1];
	dfs->det_print = (boolean)argv[2];
	dfs->det_print2 = (boolean)argv[3];
	dfs->print_hist_rpt = (boolean)argv[4];
	dfs->hist_cond_on = (boolean)argv[5];

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "dbg_mode: %d, force_TP_mode: %d, det_print: %d,det_print2: %d, print_hist_rpt: %d, hist_cond_on: %d\n",
		 dfs->dbg_mode, dfs->force_TP_mode, dfs->det_print,
		 dfs->det_print2, dfs->print_hist_rpt, dfs->hist_cond_on);

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

u8 phydm_dfs_polling_time(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 dfs_polling_time = 0;

	if (dm->support_ic_type & (ODM_RTL8814A | ODM_RTL8822B | ODM_RTL8821C))
		dfs_polling_time = 40;
	else
		dfs_polling_time = 100;

	return dfs_polling_time;
}

#endif /* @defined(CONFIG_PHYDM_DFS_MASTER) */

boolean
phydm_is_dfs_band(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (((*dm->channel >= 52) && (*dm->channel <= 64)) ||
	    ((*dm->channel >= 100) && (*dm->channel <= 144)))
		return true;
	else
		return false;
}

boolean
phydm_dfs_master_enabled(void *dm_void)
{
#ifdef CONFIG_PHYDM_DFS_MASTER
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	boolean ret_val = false;

	if (dm->dfs_master_enabled) /*pointer protection*/
		ret_val = *dm->dfs_master_enabled ? true : false;

	return ret_val;
#else
	return false;
#endif
}
