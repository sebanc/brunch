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
#include "mp_precomp.h"
#include "phydm_precomp.h"

void phydm_set_crystal_cap(void *dm_void, u8 crystal_cap)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;
	u32 reg_val = 0;

	if (cfo_track->crystal_cap == crystal_cap)
		return;

	crystal_cap = crystal_cap & 0x3F;
	reg_val = crystal_cap | (crystal_cap << 6);
	cfo_track->crystal_cap = crystal_cap;

	if (dm->support_ic_type & (ODM_RTL8188E | ODM_RTL8188F)) {
		#if (RTL8188E_SUPPORT == 1) || (RTL8188F_SUPPORT == 1)
		/* write 0x24[22:17] = 0x24[16:11] = crystal_cap */
		odm_set_bb_reg(dm, 0x24, 0x7ff800, reg_val);
		#endif
	}
	#if (RTL8812A_SUPPORT == 1)
	else if (dm->support_ic_type & ODM_RTL8812) {
		/* write 0x2C[30:25] = 0x2C[24:19] = crystal_cap */
		odm_set_bb_reg(dm, 0x2c, 0x7FF80000, reg_val);
	}
	#endif
	#if (RTL8703B_SUPPORT == 1) || (RTL8723B_SUPPORT == 1) || (RTL8192E_SUPPORT == 1) || (RTL8821A_SUPPORT == 1) || (RTL8723D_SUPPORT == 1)
	else if ((dm->support_ic_type &
		 (ODM_RTL8703B | ODM_RTL8723B | ODM_RTL8192E | ODM_RTL8821 |
		 ODM_RTL8723D))) {
		/* 0x2C[23:18] = 0x2C[17:12] = crystal_cap */
		odm_set_bb_reg(dm, 0x2c, 0x00FFF000, reg_val);
	}
	#endif
	#if (RTL8814A_SUPPORT == 1)
	else if (dm->support_ic_type & ODM_RTL8814A) {
		/* write 0x2C[26:21] = 0x2C[20:15] = crystal_cap */
		odm_set_bb_reg(dm, 0x2c, 0x07FF8000, reg_val);
	}
	#endif
	#if (RTL8822B_SUPPORT == 1) || (RTL8821C_SUPPORT == 1) || (RTL8197F_SUPPORT == 1)
	else if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C |
		 ODM_RTL8197F)) {
		/* write 0x24[30:25] = 0x28[6:1] = crystal_cap */
		odm_set_bb_reg(dm, REG_AFE_XTAL_CTRL, 0x7e000000, crystal_cap);
		odm_set_bb_reg(dm, REG_AFE_PLL_CTRL, 0x7e, crystal_cap);
	}
	#endif
	#if (RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	else if (dm->support_ic_type & ODM_RTL8192F) {
		/* write 0x24[30:25] = 0x28[6:1] = crystal_cap */
		odm_set_bb_reg(dm, REG_AFE_XTAL_CTRL, 0x7e000000, crystal_cap);
		odm_set_bb_reg(dm, REG_AFE_PLL_CTRL, 0x7e, crystal_cap);
	}
	#endif
	#if (RTL8710B_SUPPORT == 1)
	else if (dm->support_ic_type & (ODM_RTL8710B)) {
		#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		/* write 0x60[29:24] = 0x60[23:18] = crystal_cap */
		HAL_SetSYSOnReg(dm->adapter, REG_SYS_XTAL_CTRL0, 0x3FFC0000, (crystal_cap | (crystal_cap << 6)));
		#endif
	}
	#endif
	PHYDM_DBG(dm, DBG_CFO_TRK, "Set rystal_cap = 0x%x\n",
		  cfo_track->crystal_cap);
}

u8 phydm_get_default_crytaltal_cap(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;
	u8 crystal_cap = 0x20;

	crystal_cap = (cfo_track->crystal_cap_default) & 0x3f;

	return crystal_cap;
}

void phydm_set_atc_status(void *dm_void, boolean atc_status)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;
	u32 reg_tmp = 0;
	u32 mask_tmp = 0;

	if (cfo_track->is_atc_status == atc_status)
		return;

	reg_tmp = ODM_REG(BB_ATC, dm);
	mask_tmp = ODM_BIT(BB_ATC, dm);
	odm_set_bb_reg(dm, reg_tmp, mask_tmp, atc_status);
	cfo_track->is_atc_status = atc_status;
}

boolean
phydm_get_atc_status(void *dm_void)
{
	boolean atc_status;
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 reg_tmp = 0;
	u32 mask_tmp = 0;

	reg_tmp = ODM_REG(BB_ATC, dm);
	mask_tmp = ODM_BIT(BB_ATC, dm);

	atc_status = (boolean)odm_get_bb_reg(dm, reg_tmp, mask_tmp);
	return atc_status;
}

void phydm_cfo_tracking_reset(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;

	PHYDM_DBG(dm, DBG_CFO_TRK, "%s ======>\n", __func__);

	cfo_track->def_x_cap = phydm_get_default_crytaltal_cap(dm);
	cfo_track->is_adjust = true;

	if (cfo_track->crystal_cap > cfo_track->def_x_cap) {
		phydm_set_crystal_cap(dm, cfo_track->crystal_cap - 1);
		PHYDM_DBG(dm, DBG_CFO_TRK, "approch to Init-val (0x%x)\n",
			  cfo_track->crystal_cap);

	} else if (cfo_track->crystal_cap < cfo_track->def_x_cap) {
		phydm_set_crystal_cap(dm, cfo_track->crystal_cap + 1);
		PHYDM_DBG(dm, DBG_CFO_TRK, "approch to init-val 0x%x\n",
			  cfo_track->crystal_cap);
	}

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
	phydm_set_atc_status(dm, true);
#endif
}

void phydm_cfo_tracking_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;

	cfo_track->crystal_cap = phydm_get_default_crytaltal_cap(dm);
	cfo_track->def_x_cap = cfo_track->crystal_cap;
	cfo_track->is_atc_status = phydm_get_atc_status(dm);
	cfo_track->is_adjust = true;
	PHYDM_DBG(dm, DBG_CFO_TRK, "[%s]=========>\n", __func__);
	PHYDM_DBG(dm, DBG_CFO_TRK,
		  "is_atc_status = %d, crystal_cap = 0x%x\n",
		  cfo_track->is_atc_status, cfo_track->def_x_cap);

#if RTL8822B_SUPPORT
	/* Crystal cap. control by WiFi */
	if (dm->support_ic_type & ODM_RTL8822B)
		odm_set_bb_reg(dm, R_0x10, 0x40, 0x1);
#endif

#if RTL8821C_SUPPORT
	/* Crystal cap. control by WiFi */
	if (dm->support_ic_type & ODM_RTL8821C)
		odm_set_bb_reg(dm, R_0x10, 0x40, 0x1);
#endif
}

void phydm_cfo_tracking(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;
	s32 cfo_avg = 0, cfo_path_sum = 0, cfo_abs = 0;
	u32 cfo_rpt_sum, cfo_khz_avg[4] = {0};
	s8 crystal_cap = cfo_track->crystal_cap;
	u8 i, valid_path_cnt = 0;

	if (!(dm->support_ability & ODM_BB_CFO_TRACKING))
		return;

	PHYDM_DBG(dm, DBG_CFO_TRK, "%s ======>\n", __func__);

	if (!dm->is_linked || !dm->is_one_entry_only) {
		phydm_cfo_tracking_reset(dm);
		PHYDM_DBG(dm, DBG_CFO_TRK,
			  "is_linked = %d, one_entry_only = %d\n",
			  dm->is_linked, dm->is_one_entry_only);

	} else {
		/* No new packet */
		if (cfo_track->packet_count == cfo_track->packet_count_pre) {
			PHYDM_DBG(dm, DBG_CFO_TRK, "Pkt cnt doesn't change\n");
			return;
		}
		cfo_track->packet_count_pre = cfo_track->packet_count;

		/*Calculate CFO */
		for (i = 0; i < dm->num_rf_path; i++) {
			if (cfo_track->CFO_cnt[i] == 0)
				continue;

			valid_path_cnt++;

			if (cfo_track->CFO_tail[i] < 0)
				cfo_abs = 0 - cfo_track->CFO_tail[i];
			else
				cfo_abs = cfo_track->CFO_tail[i];

			cfo_rpt_sum = (u32)CFO_HW_RPT_2_KHZ(cfo_abs);
			cfo_khz_avg[i] = cfo_rpt_sum / cfo_track->CFO_cnt[i];

			PHYDM_DBG(dm, DBG_CFO_TRK,
				  "[Path-%d] CFO_sum=((%d)), cnt=((%d)), CFO_avg=((%s%d))kHz\n",
				  i, cfo_rpt_sum, cfo_track->CFO_cnt[i],
				  ((cfo_track->CFO_tail[i] < 0) ? "-" : " "),
				  cfo_khz_avg[i]);
		}

		for (i = 0; i < valid_path_cnt; i++) {
			if (cfo_track->CFO_tail[i] < 0)
				cfo_path_sum += (0 - (s32)cfo_khz_avg[i]);
			else
				cfo_path_sum += (s32)cfo_khz_avg[i];
		}

		if (valid_path_cnt >= 2)
			cfo_avg = cfo_path_sum / valid_path_cnt;
		else
			cfo_avg = cfo_path_sum;

		cfo_track->CFO_ave_pre = cfo_avg;

		PHYDM_DBG(dm, DBG_CFO_TRK,
			  "path_cnt = ((%d)), CFO_avg_path=((%d kHz))\n",
			  valid_path_cnt, cfo_avg);

		/*reset counter*/
		for (i = 0; i < dm->num_rf_path; i++) {
			cfo_track->CFO_tail[i] = 0;
			cfo_track->CFO_cnt[i] = 0;
		}

		/* To adjust crystal cap or not */
		if (!cfo_track->is_adjust) {
			if (cfo_avg > CFO_TRK_ENABLE_TH ||
			    cfo_avg < (-CFO_TRK_ENABLE_TH))
				cfo_track->is_adjust = true;
		} else {
			if (cfo_avg < CFO_TRK_STOP_TH &&
			    cfo_avg > (-CFO_TRK_STOP_TH))
				cfo_track->is_adjust = false;
		}

		#ifdef ODM_CONFIG_BT_COEXIST
		/*BT case: Disable CFO tracking */
		if (dm->bt_info_table.is_bt_enabled) {
			cfo_track->is_adjust = false;
			phydm_set_crystal_cap(dm, cfo_track->def_x_cap);
			PHYDM_DBG(dm, DBG_CFO_TRK,
				  "Disable CFO tracking for BT\n");
		}
		#endif

		/*Adjust Crystal Cap. */
		if (cfo_track->is_adjust) {
			if (cfo_avg > CFO_TRK_STOP_TH)
				crystal_cap += 1;
			else if (cfo_avg < (-CFO_TRK_STOP_TH))
				crystal_cap -= 1;

			if (crystal_cap > 0x3f)
				crystal_cap = 0x3f;
			else if (crystal_cap < 0)
				crystal_cap = 0;

			phydm_set_crystal_cap(dm, (u8)crystal_cap);
		}

		PHYDM_DBG(dm, DBG_CFO_TRK,
			  "Crystal cap{Current, Default}={0x%x, 0x%x}\n\n",
			  cfo_track->crystal_cap, cfo_track->def_x_cap);

		/* Dynamic ATC switch */
		#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		if (dm->support_ic_type & ODM_IC_11N_SERIES) {
			if (cfo_avg < CFO_TH_ATC && cfo_avg > -CFO_TH_ATC) {
				phydm_set_atc_status(dm, false);
				PHYDM_DBG(dm, DBG_CFO_TRK, "Disable ATC\n");
			} else {
				phydm_set_atc_status(dm, true);
				PHYDM_DBG(dm, DBG_CFO_TRK, "Enable ATC\n");
			}
		}
		#endif
	}
}

void phydm_parsing_cfo(void *dm_void, void *pktinfo_void, s8 *pcfotail,
		       u8 num_ss)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_perpkt_info_struct *pktinfo = NULL;
	struct phydm_cfo_track_struct *cfo_track = &dm->dm_cfo_track;
	u8 i;

	if (!(dm->support_ability & ODM_BB_CFO_TRACKING))
		return;

	pktinfo = (struct phydm_perpkt_info_struct *)pktinfo_void;

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
	if (pktinfo->is_packet_match_bssid) {
#else
	if (pktinfo->station_id != 0) {
#endif
		if (num_ss > dm->num_rf_path) /*For fool proof*/
			num_ss = dm->num_rf_path;
		#if 0
		PHYDM_DBG(dm, DBG_CFO_TRK,
			  "num_ss = ((%d)),  dm->num_rf_path = ((%d))\n",
			  num_ss,  dm->num_rf_path);
		#endif

		/* 3 Update CFO report for path-A & path-B */
		/* Only paht-A and path-B have CFO tail and short CFO */
		for (i = 0; i < num_ss; i++) {
			cfo_track->CFO_tail[i] += pcfotail[i];
			cfo_track->CFO_cnt[i]++;
			#if 0
			PHYDM_DBG(dm, DBG_CFO_TRK,
				  "[ID %d][path %d][rate 0x%x] CFO_tail = ((%d)), CFO_tail_sum = ((%d)), CFO_cnt = ((%d))\n",
				  pktinfo->station_id, i, pktinfo->data_rate,
				  pcfotail[i], cfo_track->CFO_tail[i],
				  cfo_track->CFO_cnt[i]);
			#endif
		}

		/* 3 Update packet counter */
		if (cfo_track->packet_count == 0xffffffff)
			cfo_track->packet_count = 0;
		else
			cfo_track->packet_count++;
	}
}
