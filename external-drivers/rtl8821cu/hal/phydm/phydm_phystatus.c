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
 ************************************************************/

#include "mp_precomp.h"
#include "phydm_precomp.h"

void phydm_rx_statistic_cal(struct dm_struct *phydm,
			    struct phydm_phyinfo_struct *phy_info,
			    u8 *phy_status_inf,
			    struct phydm_perpkt_info_struct *pktinfo)
{
#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
	struct phy_status_rpt_jaguar2_type1 *phy_sta_rpt = (struct phy_status_rpt_jaguar2_type1 *)phy_status_inf;
	u8 phy_status_type = (*phy_status_inf & 0xf);
#endif
	u8 date_rate = (pktinfo->data_rate & 0x7f);
	u8 bw_idx = phy_info->band_width;

	if (date_rate <= ODM_RATE54M) {
		phydm->phy_dbg_info.num_qry_legacy_pkt[date_rate]++;
		/**/
	} else if (date_rate <= ODM_RATEMCS31) {
		phydm->phy_dbg_info.ht_pkt_not_zero = true;

		if (phydm->support_ic_type & PHYSTS_2ND_TYPE_IC) {
			if (bw_idx == *phydm->band_width) {
				phydm->phy_dbg_info.num_qry_ht_pkt[date_rate - ODM_RATEMCS0]++;

			} else if (bw_idx == CHANNEL_WIDTH_20) {
				phydm->phy_dbg_info.num_qry_pkt_sc_20m[date_rate - ODM_RATEMCS0]++;
				phydm->phy_dbg_info.low_bw_20_occur = true;
			}
		} else {
			phydm->phy_dbg_info.num_qry_ht_pkt[date_rate - ODM_RATEMCS0]++;
		}
	}
#if ODM_IC_11AC_SERIES_SUPPORT
	else if (date_rate <= ODM_RATEVHTSS4MCS9) {
		#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
		if (phy_status_type == 1 &&
		    phy_sta_rpt->gid != 0 &&
		    phy_sta_rpt->gid != 63 &&
		    (phydm->support_ic_type & PHYSTS_2ND_TYPE_IC)) {
			phydm->phy_dbg_info.num_qry_mu_vht_pkt[date_rate - ODM_RATEVHTSS1MCS0]++;
			if (pktinfo->ppdu_cnt < 4) {
				phydm->phy_dbg_info.num_of_ppdu[pktinfo->ppdu_cnt] = date_rate | BIT(7);
				phydm->phy_dbg_info.gid_num[pktinfo->ppdu_cnt] = phy_sta_rpt->gid;
			}
		} else
		#endif
		{
			phydm->phy_dbg_info.vht_pkt_not_zero = true;

			if (phydm->support_ic_type & PHYSTS_2ND_TYPE_IC) {
				if (bw_idx == *phydm->band_width) {
					phydm->phy_dbg_info.num_qry_vht_pkt[date_rate - ODM_RATEVHTSS1MCS0]++;

				} else if (bw_idx == CHANNEL_WIDTH_20) {
					phydm->phy_dbg_info.num_qry_pkt_sc_20m[date_rate - ODM_RATEVHTSS1MCS0]++;
					phydm->phy_dbg_info.low_bw_20_occur = true;
				} else {/*if (bw_idx == CHANNEL_WIDTH_40)*/
					phydm->phy_dbg_info.num_qry_pkt_sc_40m[date_rate - ODM_RATEVHTSS1MCS0]++;
					phydm->phy_dbg_info.low_bw_40_occur = true;
				}
			} else {
				phydm->phy_dbg_info.num_qry_vht_pkt[date_rate - ODM_RATEVHTSS1MCS0]++;
			}

			#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
			if (pktinfo->ppdu_cnt < 4) {
				phydm->phy_dbg_info.num_of_ppdu[pktinfo->ppdu_cnt] = date_rate;
				phydm->phy_dbg_info.gid_num[pktinfo->ppdu_cnt] = phy_sta_rpt->gid;
			}
			#endif
		}
	}
#endif
}

void phydm_reset_phystatus_avg(struct dm_struct *dm)
{
	struct phydm_phystatus_avg *dbg_avg = &dm->phy_dbg_info.phystatus_statistic_avg;

	odm_memory_set(dm, &dbg_avg->rssi_cck_avg, 0,
		       sizeof(struct phydm_phystatus_avg));
}

void phydm_reset_phystatus_statistic(struct dm_struct *dm)
{
	struct phydm_phystatus_statistic *dbg_statistic = &dm->phy_dbg_info.phystatus_statistic_info;

	odm_memory_set(dm, &dbg_statistic->rssi_cck_sum, 0,
		       sizeof(struct phydm_phystatus_statistic));
}

void phydm_avg_phystatus_index(struct dm_struct *dm,
			       struct phydm_phyinfo_struct *phy_info,
			       struct phydm_perpkt_info_struct *pktinfo)
{
	struct phydm_phystatus_statistic *dbg_statistic = &dm->phy_dbg_info.phystatus_statistic_info;

	if (pktinfo->data_rate <= ODM_RATE11M) {
		/*RSSI*/
		dbg_statistic->rssi_cck_sum += phy_info->rx_mimo_signal_strength[0];
		dbg_statistic->rssi_cck_cnt++;
	} else if (pktinfo->data_rate <= ODM_RATE54M) {
		/*evm*/
		dbg_statistic->evm_ofdm_sum += phy_info->rx_mimo_evm_dbm[0];

		/*SNR*/
		dbg_statistic->snr_ofdm_sum += phy_info->rx_snr[0];

		/*RSSI*/
		dbg_statistic->rssi_ofdm_sum += phy_info->rx_mimo_signal_strength[0];
		dbg_statistic->rssi_ofdm_cnt++;
	} else if (pktinfo->rate_ss == 1) {
		/*evm*/
		dbg_statistic->evm_1ss_sum += phy_info->rx_mimo_evm_dbm[0];

		/*SNR*/
		dbg_statistic->snr_1ss_sum += phy_info->rx_snr[0];

		dbg_statistic->rssi_1ss_sum += phy_info->rx_mimo_signal_strength[0];
		dbg_statistic->rssi_1ss_cnt++;
	} else if (pktinfo->rate_ss == 2) {
		#if (defined(PHYDM_COMPILE_ABOVE_2SS))
		/*evm*/
		dbg_statistic->evm_2ss_sum[0] += phy_info->rx_mimo_evm_dbm[0];
		dbg_statistic->evm_2ss_sum[1] += phy_info->rx_mimo_evm_dbm[1];

		/*SNR*/
		dbg_statistic->snr_2ss_sum[0] += phy_info->rx_snr[0];
		dbg_statistic->snr_2ss_sum[1] += phy_info->rx_snr[1];

		/*RSSI*/
		dbg_statistic->rssi_2ss_sum[0] += phy_info->rx_mimo_signal_strength[0];
		dbg_statistic->rssi_2ss_sum[1] += phy_info->rx_mimo_signal_strength[1];
		dbg_statistic->rssi_2ss_cnt++;
		#endif
	} else if (pktinfo->rate_ss == 3) {
		#if (defined(PHYDM_COMPILE_ABOVE_3SS))
		/*evm*/
		dbg_statistic->evm_3ss_sum[0] += phy_info->rx_mimo_evm_dbm[0];
		dbg_statistic->evm_3ss_sum[1] += phy_info->rx_mimo_evm_dbm[1];
		dbg_statistic->evm_3ss_sum[2] += phy_info->rx_mimo_evm_dbm[2];

		/*SNR*/
		dbg_statistic->snr_3ss_sum[0] += phy_info->rx_snr[0];
		dbg_statistic->snr_3ss_sum[1] += phy_info->rx_snr[1];
		dbg_statistic->snr_3ss_sum[2] += phy_info->rx_snr[2];

		/*RSSI*/
		dbg_statistic->rssi_3ss_sum[0] += phy_info->rx_mimo_signal_strength[0];
		dbg_statistic->rssi_3ss_sum[1] += phy_info->rx_mimo_signal_strength[1];
		dbg_statistic->rssi_3ss_sum[2] += phy_info->rx_mimo_signal_strength[2];
		dbg_statistic->rssi_3ss_cnt++;
		#endif
	} else if (pktinfo->rate_ss == 4) {
		#if (defined(PHYDM_COMPILE_ABOVE_4SS))
		/*evm*/
		dbg_statistic->evm_4ss_sum[0] += phy_info->rx_mimo_evm_dbm[0];
		dbg_statistic->evm_4ss_sum[1] += phy_info->rx_mimo_evm_dbm[1];
		dbg_statistic->evm_4ss_sum[2] += phy_info->rx_mimo_evm_dbm[2];
		dbg_statistic->evm_4ss_sum[3] += phy_info->rx_mimo_evm_dbm[3];

		/*SNR*/
		dbg_statistic->snr_4ss_sum[0] += phy_info->rx_snr[0];
		dbg_statistic->snr_4ss_sum[1] += phy_info->rx_snr[1];
		dbg_statistic->snr_4ss_sum[2] += phy_info->rx_snr[2];
		dbg_statistic->snr_4ss_sum[3] += phy_info->rx_snr[3];

		/*RSSI*/
		dbg_statistic->rssi_4ss_sum[0] += phy_info->rx_mimo_signal_strength[0];
		dbg_statistic->rssi_4ss_sum[1] += phy_info->rx_mimo_signal_strength[1];
		dbg_statistic->rssi_4ss_sum[2] += phy_info->rx_mimo_signal_strength[2];
		dbg_statistic->rssi_4ss_sum[3] += phy_info->rx_mimo_signal_strength[3];
		dbg_statistic->rssi_4ss_cnt++;
		#endif
	}
}

u8 phydm_get_signal_quality(struct phydm_phyinfo_struct *phy_info,
			    struct dm_struct *dm,
			    struct phy_status_rpt_8192cd *phy_sta_rpt)
{
	u8 sq_rpt;
	u8 result = 0;

	if (phy_info->rx_pwdb_all > 40 && !dm->is_in_hct_test) {
		result = 100;
	} else {
		sq_rpt = phy_sta_rpt->cck_sig_qual_ofdm_pwdb_all;

		if (sq_rpt > 64)
			result = 0;
		else if (sq_rpt < 20)
			result = 100;
		else
			result = ((64 - sq_rpt) * 100) / 44;
	}

	return result;
}

u8 phydm_query_rx_pwr_percentage(s8 ant_power)
{
	if ((ant_power <= -100) || ant_power >= 20)
		return 0;
	else if (ant_power >= 0)
		return 100;
	else
		return 100 + ant_power;
}

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))

#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
s32 phydm_signal_scale_mapping_92c_series(struct dm_struct *dm, s32 curr_sig)
{
	s32 ret_sig = 0;
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)
	if (dm->support_interface == ODM_ITRF_PCIE) {
		/* step 1. Scale mapping. */
		if (curr_sig >= 61 && curr_sig <= 100)
			ret_sig = 90 + ((curr_sig - 60) / 4);
		else if (curr_sig >= 41 && curr_sig <= 60)
			ret_sig = 78 + ((curr_sig - 40) / 2);
		else if (curr_sig >= 31 && curr_sig <= 40)
			ret_sig = 66 + (curr_sig - 30);
		else if (curr_sig >= 21 && curr_sig <= 30)
			ret_sig = 54 + (curr_sig - 20);
		else if (curr_sig >= 5 && curr_sig <= 20)
			ret_sig = 42 + (((curr_sig - 5) * 2) / 3);
		else if (curr_sig == 4)
			ret_sig = 36;
		else if (curr_sig == 3)
			ret_sig = 27;
		else if (curr_sig == 2)
			ret_sig = 18;
		else if (curr_sig == 1)
			ret_sig = 9;
		else
			ret_sig = curr_sig;
	}
#endif

#if ((DEV_BUS_TYPE == RT_USB_INTERFACE) || (DEV_BUS_TYPE == RT_SDIO_INTERFACE))
	if (dm->support_interface == ODM_ITRF_USB || dm->support_interface == ODM_ITRF_SDIO) {
		if (curr_sig >= 51 && curr_sig <= 100)
			ret_sig = 100;
		else if (curr_sig >= 41 && curr_sig <= 50)
			ret_sig = 80 + ((curr_sig - 40) * 2);
		else if (curr_sig >= 31 && curr_sig <= 40)
			ret_sig = 66 + (curr_sig - 30);
		else if (curr_sig >= 21 && curr_sig <= 30)
			ret_sig = 54 + (curr_sig - 20);
		else if (curr_sig >= 10 && curr_sig <= 20)
			ret_sig = 42 + (((curr_sig - 10) * 2) / 3);
		else if (curr_sig >= 5 && curr_sig <= 9)
			ret_sig = 22 + (((curr_sig - 5) * 3) / 2);
		else if (curr_sig >= 1 && curr_sig <= 4)
			ret_sig = 6 + (((curr_sig - 1) * 3) / 2);
		else
			ret_sig = curr_sig;
	}

#endif
	return ret_sig;
}

s32 phydm_signal_scale_mapping(struct dm_struct *dm, s32 curr_sig)
{
#ifdef CONFIG_SIGNAL_SCALE_MAPPING
	return phydm_signal_scale_mapping_92c_series(dm, curr_sig);
#else
	return curr_sig;
#endif
}
#endif

void phydm_process_signal_strength(struct dm_struct *dm,
				   struct phydm_phyinfo_struct *phy_info,
				   struct phydm_perpkt_info_struct *pktinfo)
{
	u8 avg_rssi = 0, tmp_rssi = 0, best_rssi = 0, second_rssi = 0;
	u8 i;

	/* 2015/01 Sean, use the best two RSSI only, suggested by Ynlin and ChenYu.*/
	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		tmp_rssi = phy_info->rx_mimo_signal_strength[i];

		/*Get the best two RSSI*/
		if (tmp_rssi > best_rssi && tmp_rssi > second_rssi) {
			second_rssi = best_rssi;
			best_rssi = tmp_rssi;
		} else if (tmp_rssi > second_rssi && tmp_rssi <= best_rssi) {
			second_rssi = tmp_rssi;
		}
	}

	if (best_rssi == 0)
		return;

	avg_rssi = (pktinfo->rate_ss == 1) ? best_rssi : ((best_rssi + second_rssi) >> 1);

	if (dm->support_ic_type & PHYSTS_2ND_TYPE_IC) {
	#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)

		/* Update signal strength to UI, and phy_info->rx_pwdb_all is the maximum RSSI of all path */
		#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		phy_info->signal_strength = SignalScaleProc(dm->adapter, phy_info->rx_pwdb_all, false, false);
		#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)
		phy_info->signal_strength = (u8)(phydm_signal_scale_mapping(dm, phy_info->rx_pwdb_all));
		#endif

	#endif
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
	#if ODM_IC_11AC_SERIES_SUPPORT

		/*UI BSS List signal strength(in percentage), make it good looking, from 0~100.*/
		/*It is assigned to the BSS List in GetValueFromBeaconOrProbeRsp().*/
		if (pktinfo->is_cck_rate)
			#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			/*2012/01/12 MH Use customeris signal strength from HalComRxdDesc.c/*/
			phy_info->signal_strength = SignalScaleProc(dm->adapter, phy_info->rx_pwdb_all, false, true);
			#else
			phy_info->signal_strength = (u8)(phydm_signal_scale_mapping(dm, phy_info->rx_pwdb_all)); /*pwdb_all;*/
			#endif
		else
			#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			/* 2012/01/12 MH Use customeris signal strength from HalComRxdDesc.c/*/
			phy_info->signal_strength = SignalScaleProc(dm->adapter, avg_rssi, false, false);
			#else
			phy_info->signal_strength = (u8)(phydm_signal_scale_mapping(dm, avg_rssi));
			#endif
	#endif
	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
	#if ODM_IC_11N_SERIES_SUPPORT

		/* UI BSS List signal strength(in percentage), make it good looking, from 0~100. */
		/* It is assigned to the BSS List in GetValueFromBeaconOrProbeRsp(). */
		if (pktinfo->is_cck_rate)
			#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			/* 2012/01/12 MH Use customeris signal strength from HalComRxdDesc.c/ */
			phy_info->signal_strength = SignalScaleProc(dm->adapter, phy_info->rx_pwdb_all, true, true);
			#else
			phy_info->signal_strength = (u8)(phydm_signal_scale_mapping(dm, phy_info->rx_pwdb_all)); /*pwdb_all;*/
			#endif
		else
			#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			/* 2012/01/12 MH Use customeris signal strength from HalComRxdDesc.c/ */
			phy_info->signal_strength = SignalScaleProc(dm->adapter, avg_rssi, true, false);
			#else
			phy_info->signal_strength = (u8)(phydm_signal_scale_mapping(dm, avg_rssi));
			#endif
	#endif
	}
}
#endif

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
static u8 phydm_sq_patch_rt_cid_819x_lenovo(
	struct dm_struct *dm,
	u8 is_cck_rate,
	u8 pwdb_all,
	u8 path,
	u8 RSSI)
{
	u8 sq = 0;

	if (is_cck_rate) {
		if (IS_HARDWARE_TYPE_8192E(dm->adapter)) {
			/*  */
			/* <Roger_Notes> Expected signal strength and bars indication at Lenovo lab. 2013.04.11 */
			/* 802.11n, 802.11b, 802.11g only at channel 6 */
			/*  */
			/*		Attenuation (dB)	OS Signal Bars	RSSI by Xirrus (dBm) */
			/*			50				5			-49 */
			/*			55				5			-49 */
			/*			60				5			-50 */
			/*			65				5			-51 */
			/*			70				5			-52 */
			/*			75				5			-54 */
			/*			80				5			-55 */
			/*			85				4			-60 */
			/*			90				3			-63 */
			/*			95				3			-65 */
			/*			100				2			-67 */
			/*			102				2			-67 */
			/*			104				1			-70 */
			/*  */

			if (pwdb_all >= 50)
				sq = 100;
			else if (pwdb_all >= 35 && pwdb_all < 50)
				sq = 80;
			else if (pwdb_all >= 31 && pwdb_all < 35)
				sq = 60;
			else if (pwdb_all >= 22 && pwdb_all < 31)
				sq = 40;
			else if (pwdb_all >= 18 && pwdb_all < 22)
				sq = 20;
			else
				sq = 10;
		} else {
			if (pwdb_all >= 50)
				sq = 100;
			else if (pwdb_all >= 35 && pwdb_all < 50)
				sq = 80;
			else if (pwdb_all >= 22 && pwdb_all < 35)
				sq = 60;
			else if (pwdb_all >= 18 && pwdb_all < 22)
				sq = 40;
			else
				sq = 10;
		}

	} else {
		/* OFDM rate */

		if (IS_HARDWARE_TYPE_8192E(dm->adapter)) {
			if (RSSI >= 45)
				sq = 100;
			else if (RSSI >= 22 && RSSI < 45)
				sq = 80;
			else if (RSSI >= 18 && RSSI < 22)
				sq = 40;
			else
				sq = 20;
		} else {
			if (RSSI >= 45)
				sq = 100;
			else if (RSSI >= 22 && RSSI < 45)
				sq = 80;
			else if (RSSI >= 18 && RSSI < 22)
				sq = 40;
			else
				sq = 20;
		}
	}

	RT_TRACE(COMP_DBG, DBG_TRACE, ("is_cck_rate(%#d), pwdb_all(%#d), RSSI(%#d), sq(%#d)\n", is_cck_rate, pwdb_all, RSSI, sq));

	return sq;
}

static u8 phydm_sq_patch_rt_cid_819x_acer(
	struct dm_struct *dm,
	u8 is_cck_rate,
	u8 pwdb_all,
	u8 path,
	u8 RSSI)
{
	u8 sq = 0;

	if (is_cck_rate) {
		RT_TRACE(COMP_DBG, DBG_WARNING, ("odm_SQ_process_patch_RT_Acer\n"));

#if OS_WIN_FROM_WIN8(OS_VERSION)

		if (pwdb_all >= 50)
			sq = 100;
		else if (pwdb_all >= 35 && pwdb_all < 50)
			sq = 80;
		else if (pwdb_all >= 30 && pwdb_all < 35)
			sq = 60;
		else if (pwdb_all >= 25 && pwdb_all < 30)
			sq = 40;
		else if (pwdb_all >= 20 && pwdb_all < 25)
			sq = 20;
		else
			sq = 10;
#else
		if (pwdb_all >= 50)
			sq = 100;
		else if (pwdb_all >= 35 && pwdb_all < 50)
			sq = 80;
		else if (pwdb_all >= 30 && pwdb_all < 35)
			sq = 60;
		else if (pwdb_all >= 25 && pwdb_all < 30)
			sq = 40;
		else if (pwdb_all >= 20 && pwdb_all < 25)
			sq = 20;
		else
			sq = 10;

		if (pwdb_all == 0) /* Abnormal case, do not indicate the value above 20 on Win7 */
			sq = 20;
#endif

	} else {
		/* OFDM rate */

		if (IS_HARDWARE_TYPE_8192E(dm->adapter)) {
			if (RSSI >= 45)
				sq = 100;
			else if (RSSI >= 22 && RSSI < 45)
				sq = 80;
			else if (RSSI >= 18 && RSSI < 22)
				sq = 40;
			else
				sq = 20;
		} else {
			if (RSSI >= 35)
				sq = 100;
			else if (RSSI >= 30 && RSSI < 35)
				sq = 80;
			else if (RSSI >= 25 && RSSI < 30)
				sq = 40;
			else
				sq = 20;
		}
	}

	RT_TRACE(COMP_DBG, DBG_LOUD, ("is_cck_rate(%#d), pwdb_all(%#d), RSSI(%#d), sq(%#d)\n", is_cck_rate, pwdb_all, RSSI, sq));

	return sq;
}
#endif

static u8
phydm_evm_db_to_percentage(
	s8 value)
{
	/*  */
	/* -33dB~0dB to 0%~99% */
	/*  */
	s8 ret_val;

	ret_val = value;
	ret_val /= 2;

/*dbg_print("value=%d\n", value);*/
/*ODM_RT_DISP(FRX, RX_PHY_SQ, ("EVMdbToPercentage92C value=%d / %x\n", ret_val, ret_val));*/
#ifdef ODM_EVM_ENHANCE_ANTDIV
	if (ret_val >= 0)
		ret_val = 0;

	if (ret_val <= -40)
		ret_val = -40;

	ret_val = 0 - ret_val;
	ret_val *= 3;
#else
	if (ret_val >= 0)
		ret_val = 0;

	if (ret_val <= -33)
		ret_val = -33;

	ret_val = 0 - ret_val;
	ret_val *= 3;

	if (ret_val == 99)
		ret_val = 100;
#endif

	return (u8)ret_val;
}

static u8
phydm_evm_dbm_jaguar_series(
	s8 value)
{
	s8 ret_val = value;

	/* -33dB~0dB to 33dB ~ 0dB */
	if (ret_val == -128)
		ret_val = 127;
	else if (ret_val < 0)
		ret_val = 0 - ret_val;

	ret_val = ret_val >> 1;
	return (u8)ret_val;
}

static s16
phydm_cfo(
	s8 value)
{
	s16 ret_val;

	if (value < 0) {
		ret_val = 0 - value;
		ret_val = (ret_val << 1) + (ret_val >> 1); /* *2.5~=312.5/2^7 */
		ret_val = ret_val | BIT(12); /* set bit12 as 1 for negative cfo */
	} else {
		ret_val = value;
		ret_val = (ret_val << 1) + (ret_val >> 1); /* *2.5~=312.5/2^7 */
	}
	return ret_val;
}

s8 phydm_cck_rssi_convert(struct dm_struct *dm, u16 lna_idx, u8 vga_idx)
{
	return (dm->cck_lna_gain_table[lna_idx] - (vga_idx << 1));
}

void phydm_get_cck_rssi_table_from_reg(struct dm_struct *dm)
{
	u8 used_lna_idx_tmp;
	u32 reg_0xa80 = 0x7431, reg_0xabc = 0xcbe5edfd; /*example: {-53, -43, -33, -27, -19, -13, -3, 1}*/ /*{0xCB, 0xD5, 0xDF, 0xE5, 0xED, 0xF3, 0xFD, 0x2}*/
	u8 i;

	PHYDM_DBG(dm, ODM_COMP_INIT, "CCK LNA Gain table init\n");

	if (!(dm->support_ic_type & (ODM_RTL8197F)))
		return;

	reg_0xa80 = odm_get_bb_reg(dm, R_0xa80, 0xFFFF);
	reg_0xabc = odm_get_bb_reg(dm, R_0xabc, MASKDWORD);

	PHYDM_DBG(dm, ODM_COMP_INIT, "reg_0xa80 = 0x%x\n", reg_0xa80);
	PHYDM_DBG(dm, ODM_COMP_INIT, "reg_0xabc = 0x%x\n", reg_0xabc);

	for (i = 0; i <= 3; i++) {
		used_lna_idx_tmp = (u8)((reg_0xa80 >> (4 * i)) & 0x7);
		dm->cck_lna_gain_table[used_lna_idx_tmp] = (s8)((reg_0xabc >> (8 * i)) & 0xff);
	}

	PHYDM_DBG(dm, ODM_COMP_INIT,
		  "cck_lna_gain_table = {%d,%d,%d,%d,%d,%d,%d,%d}\n",
		  dm->cck_lna_gain_table[0], dm->cck_lna_gain_table[1],
		  dm->cck_lna_gain_table[2], dm->cck_lna_gain_table[3],
		  dm->cck_lna_gain_table[4], dm->cck_lna_gain_table[5],
		  dm->cck_lna_gain_table[6], dm->cck_lna_gain_table[7]);
}

#if (RTL8703B_SUPPORT == 1)
s8 phydm_cck_rssi_8703B(
	u16 LNA_idx,
	u8 VGA_idx)
{
	s8 rx_pwr_all = 0x00;

	switch (LNA_idx) {
	case 0xf:
		rx_pwr_all = -48 - (2 * VGA_idx);
		break;
	case 0xb:
		rx_pwr_all = -42 - (2 * VGA_idx); /*TBD*/
		break;
	case 0xa:
		rx_pwr_all = -36 - (2 * VGA_idx);
		break;
	case 8:
		rx_pwr_all = -32 - (2 * VGA_idx);
		break;
	case 7:
		rx_pwr_all = -19 - (2 * VGA_idx);
		break;
	case 4:
		rx_pwr_all = -6 - (2 * VGA_idx);
		break;
	case 0:
		rx_pwr_all = -2 - (2 * VGA_idx);
		break;
	default:
		/*rx_pwr_all = -53+(2*(31-VGA_idx));*/
		/*dbg_print("wrong LNA index\n");*/
		break;
	}
	return rx_pwr_all;
}
#endif

#if (RTL8195A_SUPPORT == 1)
s8 phydm_cck_rssi_8195a(
	struct dm_struct *dm,
	u16 LNA_idx,
	u8 VGA_idx)
{
	s8 rx_pwr_all = 0;
	s8 lna_gain = 0;
	s8 lna_gain_table_0[8] = {0, -8, -15, -22, -29, -36, -45, -54};
	s8 lna_gain_table_1[8] = {0, -8, -15, -22, -29, -36, -45, -54}; /*use 8195A to calibrate this table. 2016.06.24, Dino*/

	if (dm->cck_agc_report_type == 0)
		lna_gain = lna_gain_table_0[LNA_idx];
	else
		lna_gain = lna_gain_table_1[LNA_idx];

	rx_pwr_all = lna_gain - (2 * VGA_idx);

	return rx_pwr_all;
}
#endif

#if (RTL8192E_SUPPORT == 1)
s8 phydm_cck_rssi_8192e(
	struct dm_struct *dm,
	u16 LNA_idx,
	u8 VGA_idx)
{
	s8 rx_pwr_all = 0;
	s8 lna_gain = 0;
	s8 lna_gain_table_0[8] = {15, 9, -10, -21, -23, -27, -43, -44};
	s8 lna_gain_table_1[8] = {24, 18, 13, -4, -11, -18, -31, -36}; /*use 8192EU to calibrate this table. 2015.12.15, Dino*/

	if (dm->cck_agc_report_type == 0)
		lna_gain = lna_gain_table_0[LNA_idx];
	else
		lna_gain = lna_gain_table_1[LNA_idx];

	rx_pwr_all = lna_gain - (2 * VGA_idx);

	return rx_pwr_all;
}
#endif

#if (RTL8188E_SUPPORT == 1)
s8 phydm_cck_rssi_8188e(
	struct dm_struct *dm,
	u16 LNA_idx,
	u8 VGA_idx)
{
	s8 rx_pwr_all = 0;
	s8 lna_gain = 0;
	s8 lna_gain_table_0[8] = {17, -1, -13, -29, -32, -35, -38, -41}; /*only use lna0/1/2/3/7*/
	s8 lna_gain_table_1[8] = {29, 20, 12, 3, -6, -15, -24, -33}; /*only use lna3 /7*/
	s8 lna_gain_table_2[8] = {17, -1, -13, -17, -32, -43, -38, -47};/*only use lna1/3/5/7*/

	if (dm->cut_version >= ODM_CUT_I) {	/*SMIC*/
		if (dm->ext_lna == 0x1) {
			switch (dm->type_glna) {
				case 0x2:	/*eLNA 14dB*/
					lna_gain = lna_gain_table_2[LNA_idx];
					break;
				default:
					lna_gain = lna_gain_table_0[LNA_idx];
					break;
			}
		} else {
			lna_gain = lna_gain_table_0[LNA_idx];
		}
	} else {	 /*TSMC*/
		lna_gain = lna_gain_table_1[LNA_idx];
	}

	rx_pwr_all = lna_gain - (2 * VGA_idx);

	return rx_pwr_all;
}
#endif

#if (RTL8821C_SUPPORT == 1)
s8 phydm_cck_rssi_8821c(struct dm_struct *dm, u8 LNA_idx, u8 VGA_idx)
{
	s8 rx_pwr_all = 0;
	s8 lna_gain = 0;
	s8 lna_gain_table_0[8] = {22, 8, -6, -22, -31, -40, -46, -52}; /*only use lna2/3/5/7*/
	s8 lna_gain_table_1[16] = {10, 6, 2, -2, -6, -10, -14, -17,
				   -20, -24, -28, -31, -34, -37, -40, -44}; /*only use lna4/8/C/F*/

	if (dm->cck_agc_report_type == 0)
		lna_gain = lna_gain_table_0[LNA_idx];
	else
		lna_gain = lna_gain_table_1[LNA_idx];

	rx_pwr_all = lna_gain - (2 * VGA_idx);

	return rx_pwr_all;
}
#endif

#if (RTL8195B_SUPPORT == 1)
s8 phydm_cck_rssi_8195B(struct dm_struct *dm, u8 LNA_idx, u8 VGA_idx)
{
	s8 rx_pwr_all = 0;
	s8 lna_gain = 0;
	s8 lna_gain_table_0[8] = {22, 8, -6, -22, -31, -40, -46, -52}; /*only use lna2/3/5/7*/

	lna_gain = lna_gain_table_0[LNA_idx];
	rx_pwr_all = lna_gain - (2 * VGA_idx);

	return rx_pwr_all;
}
#endif

#if (ODM_IC_11N_SERIES_SUPPORT == 1)
void phydm_rx_phy_status92c_series_parsing(
	struct dm_struct *dm,
	struct phydm_phyinfo_struct *phy_info,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo)
{
	u8 i, max_spatial_stream;
	s8 rx_pwr[4], rx_pwr_all = 0;
	u8 EVM, pwdb_all = 0, pwdb_all_bt;
	u8 RSSI, total_rssi = 0;
	u8 rf_rx_num = 0;
	u8 LNA_idx = 0;
	u8 VGA_idx = 0;
	u8 cck_agc_rpt;
	u8 stream_rxevm_tmp = 0;
	u8 sq;
	struct phy_status_rpt_8192cd *phy_sta_rpt = (struct phy_status_rpt_8192cd *)phy_status_inf;

	if (pktinfo->is_to_self)
		dm->curr_station_id = pktinfo->station_id;

	if (pktinfo->is_cck_rate) {
		cck_agc_rpt = phy_sta_rpt->cck_agc_rpt_ofdm_cfosho_a;

		if (dm->support_ic_type & (ODM_RTL8703B)) {
#if (RTL8703B_SUPPORT == 1)
			if (dm->cck_agc_report_type == 1) { /*4 bit LNA*/

				u8 cck_agc_rpt_b = (phy_sta_rpt->cck_rpt_b_ofdm_cfosho_b & BIT(7)) ? 1 : 0;

				LNA_idx = (cck_agc_rpt_b << 3) | ((cck_agc_rpt & 0xE0) >> 5);
				VGA_idx = (cck_agc_rpt & 0x1F);

				rx_pwr_all = phydm_cck_rssi_8703B(LNA_idx, VGA_idx);
			}
#endif
		} else { /*3 bit LNA*/

			LNA_idx = ((cck_agc_rpt & 0xE0) >> 5);
			VGA_idx = (cck_agc_rpt & 0x1F);

			if (dm->support_ic_type & (ODM_RTL8188E)) {
#if (RTL8188E_SUPPORT == 1)
				rx_pwr_all = phydm_cck_rssi_8188e(dm, LNA_idx, VGA_idx);
#endif
			}
#if (RTL8192E_SUPPORT == 1)
			else if (dm->support_ic_type & (ODM_RTL8192E)) {
				rx_pwr_all = phydm_cck_rssi_8192e(dm, LNA_idx, VGA_idx);
				/**/
			}
#endif
#if (RTL8723B_SUPPORT == 1)
			else if (dm->support_ic_type & (ODM_RTL8723B)) {
				rx_pwr_all = odm_CCKRSSI_8723B(LNA_idx, VGA_idx);
				/**/
			}
#endif
#if (RTL8188F_SUPPORT == 1)
			else if (dm->support_ic_type & (ODM_RTL8188F)) {
				rx_pwr_all = odm_CCKRSSI_8188F(LNA_idx, VGA_idx);
				/**/
			}
#endif
#if (RTL8195A_SUPPORT == 1)
			else if (dm->support_ic_type & (ODM_RTL8195A)) {
				rx_pwr_all = phydm_cck_rssi_8195a(LNA_idx, VGA_idx);
				/**/
			}
#endif
		}

		PHYDM_DBG(dm, DBG_RSSI_MNTR,
			  "ext_lna_gain (( %d )), LNA_idx: (( 0x%x )), VGA_idx: (( 0x%x )), rx_pwr_all: (( %d ))\n",
			  dm->ext_lna_gain, LNA_idx, VGA_idx, rx_pwr_all);

#if ~(RTL8188E_SUPPORT == 1)
		if (dm->board_type & ODM_BOARD_EXT_LNA)
			rx_pwr_all -= dm->ext_lna_gain;
#endif

		pwdb_all = phydm_query_rx_pwr_percentage(rx_pwr_all);

		if (pktinfo->is_to_self) {
			dm->cck_lna_idx = LNA_idx;
			dm->cck_vga_idx = VGA_idx;
		}

		phy_info->rx_pwdb_all = pwdb_all;
		phy_info->bt_rx_rssi_percentage = pwdb_all;
		phy_info->recv_signal_power = rx_pwr_all;

		/* (3) Get Signal Quality (EVM) */
		#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		if (dm->iot_table.win_patch_id == RT_CID_819X_LENOVO)
			sq = phydm_sq_patch_rt_cid_819x_lenovo(dm, pktinfo->is_cck_rate, pwdb_all, 0, 0);
		else if (dm->iot_table.win_patch_id == RT_CID_819X_ACER)
			sq = phydm_sq_patch_rt_cid_819x_acer(dm, pktinfo->is_cck_rate, pwdb_all, 0, 0);
		else
		#endif
			sq = phydm_get_signal_quality(phy_info, dm, phy_sta_rpt);

		/* dbg_print("cck sq = %d\n", sq); */
		phy_info->signal_quality = sq;
		phy_info->rx_mimo_signal_quality[RF_PATH_A] = sq;
		phy_info->rx_mimo_signal_quality[RF_PATH_B] = -1;

		for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
			if (i == 0)
				phy_info->rx_mimo_signal_strength[0] = pwdb_all;
			else
				phy_info->rx_mimo_signal_strength[i] = 0;
		}
	} else { /* 2 is OFDM rate */

		/*  */
		/* (1)Get RSSI for HT rate */
		/*  */

		for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH_N; i++) {
			if (dm->rf_path_rx_enable & BIT(i))
				rf_rx_num++;

			rx_pwr[i] = ((phy_sta_rpt->path_agc[i].gain & 0x3F) * 2) - 110;

			if (pktinfo->is_to_self) {
				dm->ofdm_agc_idx[i] = (phy_sta_rpt->path_agc[i].gain & 0x3F);
				/**/
			}

			phy_info->rx_pwr[i] = rx_pwr[i];
			RSSI = phydm_query_rx_pwr_percentage(rx_pwr[i]);
			total_rssi += RSSI;
			/* RT_DISP(FRX, RX_PHY_SS, ("RF-%d RXPWR=%x RSSI=%d\n", i, rx_pwr[i], RSSI)); */

			phy_info->rx_mimo_signal_strength[i] = (u8)RSSI;

			/* Get Rx snr value in DB */
			phy_info->rx_snr[i] = (s8)(phy_sta_rpt->path_rxsnr[i] / 2);

			/* Record Signal Strength for next packet */

			#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			if (i == RF_PATH_A) {
				if (dm->iot_table.win_patch_id == RT_CID_819X_LENOVO) {
					phy_info->signal_quality = phydm_sq_patch_rt_cid_819x_lenovo(dm, pktinfo->is_cck_rate, pwdb_all, i, RSSI);
				} else if (dm->iot_table.win_patch_id == RT_CID_819X_ACER)
					phy_info->signal_quality = phydm_sq_patch_rt_cid_819x_acer(dm, pktinfo->is_cck_rate, pwdb_all, 0, RSSI);
			}
			#endif
		}

		/* (2)PWDB, Average PWDB calculated by hardware (for rate adaptive) */
		rx_pwr_all = (((phy_sta_rpt->cck_sig_qual_ofdm_pwdb_all) >> 1) & 0x7f) - 110;

		pwdb_all_bt = pwdb_all = phydm_query_rx_pwr_percentage(rx_pwr_all);

		phy_info->rx_pwdb_all = pwdb_all;
		phy_info->bt_rx_rssi_percentage = pwdb_all_bt;
		phy_info->rx_power = rx_pwr_all;
		phy_info->recv_signal_power = rx_pwr_all;

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		if (dm->iot_table.win_patch_id == RT_CID_819X_LENOVO) {
			/* do nothing */
		} else if (dm->iot_table.win_patch_id == RT_CID_819X_ACER) {
			/* do nothing */
		} else {
#endif
			/* (3)EVM of HT rate */

			if (pktinfo->data_rate >= ODM_RATEMCS8 && pktinfo->data_rate <= ODM_RATEMCS15)
				max_spatial_stream = 2; /* both spatial stream make sense */
			else
				max_spatial_stream = 1; /* only spatial stream 1 makes sense */

			for (i = 0; i < max_spatial_stream; i++) {
				/* Do not use shift operation like "rx_evmX >>= 1" because the compilor of free build environment */
				/* fill most significant bit to "zero" when doing shifting operation which may change a negative */
				/* value to positive one, then the dbm value (which is supposed to be negative)  is not correct anymore. */
				EVM = phydm_evm_db_to_percentage((phy_sta_rpt->stream_rxevm[i])); /* dbm */

				if (i == RF_PATH_A) /* Fill value in RFD, Get the first spatial stream only */
					phy_info->signal_quality = (u8)(EVM & 0xff);

				phy_info->rx_mimo_signal_quality[i] = (u8)(EVM & 0xff);

				if (phy_sta_rpt->stream_rxevm[i] < 0)
					stream_rxevm_tmp = (u8)(0 - (phy_sta_rpt->stream_rxevm[i]));

				if (stream_rxevm_tmp == 64)
					stream_rxevm_tmp = 0;

				phy_info->rx_mimo_evm_dbm[i] = stream_rxevm_tmp;
			}
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		}
#endif

		phydm_parsing_cfo(dm, pktinfo, phy_sta_rpt->path_cfotail, pktinfo->rate_ss);
	}

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	dm->dm_fat_table.antsel_rx_keep_0 = phy_sta_rpt->ant_sel;
	dm->dm_fat_table.antsel_rx_keep_1 = phy_sta_rpt->ant_sel_b;
	dm->dm_fat_table.antsel_rx_keep_2 = phy_sta_rpt->antsel_rx_keep_2;
#endif
}
#endif

#if ODM_IC_11AC_SERIES_SUPPORT

void phydm_rx_phy_bw_jaguar_series_parsing(struct phydm_phyinfo_struct *phy_info
					   ,
					   struct phydm_perpkt_info_struct *
					   pktinfo,
					   struct phy_status_rpt_8812 *
					   phy_sta_rpt)
{
	if (pktinfo->data_rate <= ODM_RATE54M) {
		switch (phy_sta_rpt->r_RFMOD) {
		case 1:
			if (phy_sta_rpt->sub_chnl == 0)
				phy_info->band_width = 1;
			else
				phy_info->band_width = 0;
			break;

		case 2:
			if (phy_sta_rpt->sub_chnl == 0)
				phy_info->band_width = 2;
			else if (phy_sta_rpt->sub_chnl == 9 || phy_sta_rpt->sub_chnl == 10)
				phy_info->band_width = 1;
			else
				phy_info->band_width = 0;
			break;

		default:
		case 0:
			phy_info->band_width = 0;
			break;
		}
	}
}

void phydm_rx_phy_status_jaguar_series_parsing(struct dm_struct *dm,
					       struct phydm_phyinfo_struct *
					       phy_info, u8 *phy_status_inf,
					       struct phydm_perpkt_info_struct *
					       pktinfo)
{
	u8 i, max_spatial_stream;
	s8 rx_pwr[4], rx_pwr_all = 0;
	u8 EVM = 0, evm_dbm, pwdb_all = 0, pwdb_all_bt;
	u8 RSSI = 0;
	u8 rf_rx_num = 0;
	u8 cck_highpwr = 0;
	u8 LNA_idx, VGA_idx;
	struct phy_status_rpt_8812 *phy_sta_rpt = (struct phy_status_rpt_8812 *)phy_status_inf;
	struct phydm_fat_struct *fat_tab = &dm->dm_fat_table;

	phydm_rx_phy_bw_jaguar_series_parsing(phy_info, pktinfo, phy_sta_rpt);

	if (pktinfo->is_to_self)
		dm->curr_station_id = pktinfo->station_id;
	else
		dm->curr_station_id = 0xff;

	if (pktinfo->is_cck_rate) {
		u8 cck_agc_rpt;

		/*(1)Hardware does not provide RSSI for CCK*/
		/*(2)PWDB, Average PWDB calculated by hardware (for rate adaptive)*/

		/*if(hal_data->e_rf_power_state == e_rf_on)*/
		cck_highpwr = dm->is_cck_high_power;
		/*else*/
		/*cck_highpwr = false;*/

		cck_agc_rpt = phy_sta_rpt->cfosho[0];
		LNA_idx = ((cck_agc_rpt & 0xE0) >> 5);
		VGA_idx = (cck_agc_rpt & 0x1F);

		if (dm->support_ic_type == ODM_RTL8812) {
			switch (LNA_idx) {
			case 7:
				if (VGA_idx <= 27)
					rx_pwr_all = -100 + 2 * (27 - VGA_idx); /*VGA_idx = 27~2*/
				else
					rx_pwr_all = -100;
				break;
			case 6:
				rx_pwr_all = -48 + 2 * (2 - VGA_idx); /*VGA_idx = 2~0*/
				break;
			case 5:
				rx_pwr_all = -42 + 2 * (7 - VGA_idx); /*VGA_idx = 7~5*/
				break;
			case 4:
				rx_pwr_all = -36 + 2 * (7 - VGA_idx); /*VGA_idx = 7~4*/
				break;
			case 3:
				/*rx_pwr_all = -28 + 2*(7-VGA_idx); VGA_idx = 7~0*/
				rx_pwr_all = -24 + 2 * (7 - VGA_idx); /*VGA_idx = 7~0*/
				break;
			case 2:
				if (cck_highpwr)
					rx_pwr_all = -12 + 2 * (5 - VGA_idx); /*VGA_idx = 5~0*/
				else
					rx_pwr_all = -6 + 2 * (5 - VGA_idx);
				break;
			case 1:
				rx_pwr_all = 8 - 2 * VGA_idx;
				break;
			case 0:
				rx_pwr_all = 14 - 2 * VGA_idx;
				break;
			default:
				/*dbg_print("CCK Exception default\n");*/
				break;
			}
			rx_pwr_all += 6;
			pwdb_all = phydm_query_rx_pwr_percentage(rx_pwr_all);

			if (!cck_highpwr) {
				if (pwdb_all >= 80)
					pwdb_all = ((pwdb_all - 80) << 1) + ((pwdb_all - 80) >> 1) + 80;
				else if ((pwdb_all <= 78) && (pwdb_all >= 20))
					pwdb_all += 3;
				if (pwdb_all > 100)
					pwdb_all = 100;
			}
		} else if (dm->support_ic_type & (ODM_RTL8821 | ODM_RTL8881A)) {
			s8 pout = -6;

			switch (LNA_idx) {
			case 5:
				rx_pwr_all = pout - 32 - (2 * VGA_idx);
				break;
			case 4:
				rx_pwr_all = pout - 24 - (2 * VGA_idx);
				break;
			case 2:
				rx_pwr_all = pout - 11 - (2 * VGA_idx);
				break;
			case 1:
				rx_pwr_all = pout + 5 - (2 * VGA_idx);
				break;
			case 0:
				rx_pwr_all = pout + 21 - (2 * VGA_idx);
				break;
			}
			pwdb_all = phydm_query_rx_pwr_percentage(rx_pwr_all);
		} else if (dm->support_ic_type == ODM_RTL8814A) {
			s8 pout = -6;

			switch (LNA_idx) {
			/*CCK only use LNA: 2, 3, 5, 7*/
			case 7:
				rx_pwr_all = pout - 32 - (2 * VGA_idx);
				break;
			case 5:
				rx_pwr_all = pout - 22 - (2 * VGA_idx);
				break;
			case 3:
				rx_pwr_all = pout - 2 - (2 * VGA_idx);
				break;
			case 2:
				rx_pwr_all = pout + 5 - (2 * VGA_idx);
				break;
			/*case 6:*/
			/*rx_pwr_all = pout -26 - (2*VGA_idx);*/
			/*break;*/
			/*case 4:*/
			/*rx_pwr_all = pout - 8 - (2*VGA_idx);*/
			/*break;*/
			/*case 1:*/
			/*rx_pwr_all = pout + 21 - (2*VGA_idx);*/
			/*break;*/
			/*case 0:*/
			/*rx_pwr_all = pout + 10 - (2*VGA_idx);*/
			/*	break; */
			default:
				/* dbg_print("CCK Exception default\n"); */
				break;
			}
			pwdb_all = phydm_query_rx_pwr_percentage(rx_pwr_all);
		}

		dm->cck_lna_idx = LNA_idx;
		dm->cck_vga_idx = VGA_idx;
		phy_info->rx_pwdb_all = pwdb_all;
/* if(pktinfo->station_id == 0) */
/* { */
/*	dbg_print("CCK: LNA_idx = %d, VGA_idx = %d, phy_info->rx_pwdb_all = %d\n", */
/*		LNA_idx, VGA_idx, phy_info->rx_pwdb_all); */
/* } */
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		phy_info->bt_rx_rssi_percentage = pwdb_all;
		phy_info->recv_signal_power = rx_pwr_all;
#endif
		/*(3) Get Signal Quality (EVM)*/
		/*if (pktinfo->is_packet_match_bssid)*/
		{
			u8 sq, sq_rpt;

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			if (dm->iot_table.win_patch_id == RT_CID_819X_LENOVO)
				sq = phydm_sq_patch_rt_cid_819x_lenovo(dm, pktinfo->is_cck_rate, pwdb_all, 0, 0);
			else
#endif
			if (phy_info->rx_pwdb_all > 40 && !dm->is_in_hct_test) {
				sq = 100;
			} else {
				sq_rpt = phy_sta_rpt->pwdb_all;

				if (sq_rpt > 64)
					sq = 0;
				else if (sq_rpt < 20)
					sq = 100;
				else
					sq = ((64 - sq_rpt) * 100) / 44;
			}

			/* dbg_print("cck sq = %d\n", sq); */
			phy_info->signal_quality = sq;
			phy_info->rx_mimo_signal_quality[RF_PATH_A] = sq;
		}

		for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
			if (i == 0)
				phy_info->rx_mimo_signal_strength[0] = pwdb_all;
			else
				phy_info->rx_mimo_signal_strength[i] = 0;
		}
	} else {
		/*is OFDM rate*/
		fat_tab->hw_antsw_occur = phy_sta_rpt->hw_antsw_occur;

		/*(1)Get RSSI for OFDM rate*/
		for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
			/*2008/01/30 MH we will judge RF RX path now.*/
			/* dbg_print("dm->rf_path_rx_enable = %x\n", dm->rf_path_rx_enable); */
			if (dm->rf_path_rx_enable & BIT(i))
				rf_rx_num++;
			/* else */
			/* continue; */
			/*2012.05.25 LukeLee: Testchip AGC report is wrong, it should be restored back to old formula in MP chip*/
			/* if((dm->support_ic_type & (ODM_RTL8812|ODM_RTL8821)) && (!dm->is_mp_chip)) */
			if (i < RF_PATH_C) {
				rx_pwr[i] = (phy_sta_rpt->gain_trsw[i] & 0x7F) - 110;

				if (pktinfo->is_to_self)
					dm->ofdm_agc_idx[i] = phy_sta_rpt->gain_trsw[i];

			} else {
				rx_pwr[i] = (phy_sta_rpt->gain_trsw_cd[i - 2] & 0x7F) - 110;
			}
			/* else */
			/*rx_pwr[i] = ((phy_sta_rpt->gain_trsw[i]& 0x3F)*2) - 110;  OLD FORMULA*/

			phy_info->rx_pwr[i] = rx_pwr[i];

			/* Translate DBM to percentage. */
			RSSI = phydm_query_rx_pwr_percentage(rx_pwr[i]);

			phy_info->rx_mimo_signal_strength[i] = (u8)RSSI;

			/*Get Rx snr value in DB*/
			if (i < RF_PATH_C)
				phy_info->rx_snr[i] = phy_sta_rpt->rxsnr[i] / 2;
			else if (dm->support_ic_type & (ODM_RTL8814A))
				phy_info->rx_snr[i] = phy_sta_rpt->csi_current[i - 2] / 2;

#if (DM_ODM_SUPPORT_TYPE != ODM_AP)
			/*(2) CFO_short  & CFO_tail*/
			if (i < RF_PATH_C) {
				phy_info->cfo_short[i] = phydm_cfo((phy_sta_rpt->cfosho[i]));
				phy_info->cfo_tail[i] = phydm_cfo((phy_sta_rpt->cfotail[i]));
			}
#endif
			/* Record Signal Strength for next packet */
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
			if (pktinfo->is_packet_match_bssid && i == RF_PATH_A) {
				if (dm->iot_table.win_patch_id == RT_CID_819X_LENOVO) {
					phy_info->signal_quality = phydm_sq_patch_rt_cid_819x_lenovo(dm, pktinfo->is_cck_rate, pwdb_all, i, RSSI);
				}
			}
#endif
		}

		/*(3)PWDB, Average PWDB calculated by hardware (for rate adaptive)*/

		/*2012.05.25 LukeLee: Testchip AGC report is wrong, it should be restored back to old formula in MP chip*/
		if ((dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8821 | ODM_RTL8881A)) && !dm->is_mp_chip)
			rx_pwr_all = (phy_sta_rpt->pwdb_all & 0x7f) - 110;
		else
			rx_pwr_all = (((phy_sta_rpt->pwdb_all) >> 1) & 0x7f) - 110; /*OLD FORMULA*/

		pwdb_all_bt = pwdb_all = phydm_query_rx_pwr_percentage(rx_pwr_all);

		phy_info->rx_pwdb_all = pwdb_all;
		/*PHYDM_DBG(dm,DBG_RSSI_MNTR, "ODM OFDM RSSI=%d\n",phy_info->rx_pwdb_all);*/
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
		phy_info->bt_rx_rssi_percentage = pwdb_all_bt;
		phy_info->rx_power = rx_pwr_all;
		phy_info->recv_signal_power = rx_pwr_all;
#endif

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		if (dm->iot_table.win_patch_id == RT_CID_819X_LENOVO) {
			/*do nothing*/
		} else {
#endif
			/*(4)EVM of OFDM rate*/

			if (pktinfo->data_rate >= ODM_RATEMCS8 &&
			    pktinfo->data_rate <= ODM_RATEMCS15)
				max_spatial_stream = 2;
			else if ((pktinfo->data_rate >= ODM_RATEVHTSS2MCS0) &&
				 (pktinfo->data_rate <= ODM_RATEVHTSS2MCS9))
				max_spatial_stream = 2;
			else if ((pktinfo->data_rate >= ODM_RATEMCS16) &&
				 (pktinfo->data_rate <= ODM_RATEMCS23))
				max_spatial_stream = 3;
			else if ((pktinfo->data_rate >= ODM_RATEVHTSS3MCS0) &&
				 (pktinfo->data_rate <= ODM_RATEVHTSS3MCS9))
				max_spatial_stream = 3;
			else
				max_spatial_stream = 1;

			/*if (pktinfo->is_packet_match_bssid) */
			/*dbg_print("pktinfo->data_rate = %d\n", pktinfo->data_rate);*/

			for (i = 0; i < max_spatial_stream; i++) {
				/*Do not use shift operation like "rx_evmX >>= 1" because the compilor of free build environment*/
				/*fill most significant bit to "zero" when doing shifting operation which may change a negative*/
				/*value to positive one, then the dbm value (which is supposed to be negative)  is not correct anymore.*/

				if (pktinfo->data_rate >= ODM_RATE6M && pktinfo->data_rate <= ODM_RATE54M) {
					if (i == RF_PATH_A) {
						EVM = phydm_evm_db_to_percentage((phy_sta_rpt->sigevm)); /*dbm*/
						EVM += 20;
						if (EVM > 100)
							EVM = 100;
					}
				} else {
					if (i < RF_PATH_C) {
						if (phy_sta_rpt->rxevm[i] == -128)
							phy_sta_rpt->rxevm[i] = -25;
						EVM = phydm_evm_db_to_percentage((phy_sta_rpt->rxevm[i])); /*dbm*/
					} else {
						if (phy_sta_rpt->rxevm_cd[i - 2] == -128)
							phy_sta_rpt->rxevm_cd[i - 2] = -25;
						EVM = phydm_evm_db_to_percentage((phy_sta_rpt->rxevm_cd[i - 2])); /*dbm*/
					}
				}

				if (i < RF_PATH_C)
					evm_dbm = phydm_evm_dbm_jaguar_series(phy_sta_rpt->rxevm[i]);
				else
					evm_dbm = phydm_evm_dbm_jaguar_series(phy_sta_rpt->rxevm_cd[i - 2]);
				/*RT_DISP(FRX, RX_PHY_SQ, ("RXRATE=%x RXEVM=%x EVM=%s%d\n",*/
				/*pktinfo->data_rate, phy_sta_rpt->rxevm[i], "%", EVM));*/

				if (i == RF_PATH_A) {
					/*Fill value in RFD, Get the first spatial stream only*/
					phy_info->signal_quality = EVM;
				}
				phy_info->rx_mimo_signal_quality[i] = EVM;
#if (DM_ODM_SUPPORT_TYPE != ODM_AP)
				phy_info->rx_mimo_evm_dbm[i] = evm_dbm;
#endif
			}
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		}
#endif

		phydm_parsing_cfo(dm, pktinfo, phy_sta_rpt->cfotail, pktinfo->rate_ss);
	}
	/* dbg_print("is_cck_rate= %d, phy_info->signal_strength=%d % PWDB_AL=%d rf_rx_num=%d\n", is_cck_rate, phy_info->signal_strength, pwdb_all, rf_rx_num); */

	dm->rx_pwdb_ave = dm->rx_pwdb_ave + phy_info->rx_pwdb_all;

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	dm->dm_fat_table.antsel_rx_keep_0 = phy_sta_rpt->antidx_anta;
	dm->dm_fat_table.antsel_rx_keep_1 = phy_sta_rpt->antidx_antb;
	dm->dm_fat_table.antsel_rx_keep_2 = phy_sta_rpt->antidx_antc;
	dm->dm_fat_table.antsel_rx_keep_3 = phy_sta_rpt->antidx_antd;
#endif

	/*PHYDM_DBG(dm, DBG_ANT_DIV, "StaID[%d]:  antidx_anta = ((%d)), MatchBSSID =  ((%d))\n", pktinfo->station_id, phy_sta_rpt->antidx_anta, pktinfo->is_packet_match_bssid);*/

	/*		dbg_print("phy_sta_rpt->antidx_anta = %d, phy_sta_rpt->antidx_antb = %d\n",*/
	/*			phy_sta_rpt->antidx_anta, phy_sta_rpt->antidx_antb);*/
	/*		dbg_print("----------------------------\n");*/
	/*		dbg_print("pktinfo->station_id=%d, pktinfo->data_rate=0x%x\n",pktinfo->station_id, pktinfo->data_rate);*/
	/*		dbg_print("phy_sta_rpt->r_RFMOD = %d\n", phy_sta_rpt->r_RFMOD);*/
	/*		dbg_print("phy_sta_rpt->gain_trsw[0]=0x%x, phy_sta_rpt->gain_trsw[1]=0x%x\n",*/
	/*				phy_sta_rpt->gain_trsw[0],phy_sta_rpt->gain_trsw[1]);*/
	/*		dbg_print("phy_sta_rpt->gain_trsw[2]=0x%x, phy_sta_rpt->gain_trsw[3]=0x%x\n",*/
	/*				phy_sta_rpt->gain_trsw_cd[0],phy_sta_rpt->gain_trsw_cd[1]);*/
	/*		dbg_print("phy_sta_rpt->pwdb_all = 0x%x, phy_info->rx_pwdb_all = %d\n", phy_sta_rpt->pwdb_all, phy_info->rx_pwdb_all);*/
	/*		dbg_print("phy_sta_rpt->cfotail[i] = 0x%x, phy_sta_rpt->CFO_tail[i] = 0x%x\n", phy_sta_rpt->cfotail[0], phy_sta_rpt->cfotail[1]);*/
	/*		dbg_print("phy_sta_rpt->rxevm[0] = %d, phy_sta_rpt->rxevm[1] = %d\n", phy_sta_rpt->rxevm[0], phy_sta_rpt->rxevm[1]);*/
	/*		dbg_print("phy_sta_rpt->rxevm[2] = %d, phy_sta_rpt->rxevm[3] = %d\n", phy_sta_rpt->rxevm_cd[0], phy_sta_rpt->rxevm_cd[1]);*/
	/*		dbg_print("phy_info->rx_mimo_signal_strength[0]=%d, phy_info->rx_mimo_signal_strength[1]=%d, rx_pwdb_all=%d\n",*/
	/*				phy_info->rx_mimo_signal_strength[0], phy_info->rx_mimo_signal_strength[1], phy_info->rx_pwdb_all);*/
	/*		dbg_print("phy_info->rx_mimo_signal_strength[2]=%d, phy_info->rx_mimo_signal_strength[3]=%d\n",*/
	/*				phy_info->rx_mimo_signal_strength[2], phy_info->rx_mimo_signal_strength[3]);*/
	/*		dbg_print("ppPhyInfo->rx_mimo_signal_quality[0]=%d, phy_info->rx_mimo_signal_quality[1]=%d\n",*/
	/*				phy_info->rx_mimo_signal_quality[0], phy_info->rx_mimo_signal_quality[1]);*/
	/*		dbg_print("ppPhyInfo->rx_mimo_signal_quality[2]=%d, phy_info->rx_mimo_signal_quality[3]=%d\n",*/
	/*				phy_info->rx_mimo_signal_quality[2], phy_info->rx_mimo_signal_quality[3]);*/
}

#endif

void phydm_reset_rssi_for_dm(struct dm_struct *dm, u8 station_id)
{
	struct cmn_sta_info *sta;
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN))
	void *adapter = dm->adapter;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
#endif
	sta = dm->phydm_sta_info[station_id];

	if (!is_sta_active(sta)) {
		/**/
		return;
	}
	PHYDM_DBG(dm, DBG_RSSI_MNTR, "Reset RSSI for macid = (( %d ))\n",
		  station_id);

	sta->rssi_stat.rssi_cck = -1;
	sta->rssi_stat.rssi_ofdm = -1;
	sta->rssi_stat.rssi = -1;
	sta->rssi_stat.ofdm_pkt_cnt = 0;
	sta->rssi_stat.cck_pkt_cnt = 0;
	sta->rssi_stat.cck_sum_power = 0;
	sta->rssi_stat.is_send_rssi = RA_RSSI_STATE_INIT;
	sta->rssi_stat.packet_map = 0;
	sta->rssi_stat.valid_bit = 0;
}

void phydm_process_rssi_for_dm(struct dm_struct *dm,
			       struct phydm_phyinfo_struct *phy_info,
			       struct phydm_perpkt_info_struct *pktinfo)
{
	s32 rssi_ave;
	s8 undecorated_smoothed_pwdb, undecorated_smoothed_cck, undecorated_smoothed_ofdm;
	u8 i;
	u8 rssi_max, rssi_min;
	u32 weighting = 0;
	u8 send_rssi_2_fw = 0;
	struct cmn_sta_info *sta;
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN))
	struct phydm_fat_struct *fat_tab = &dm->dm_fat_table;
	void *adapter = dm->adapter;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
#endif

	if (pktinfo->station_id >= ODM_ASSOCIATE_ENTRY_NUM)
		return;

#ifdef CONFIG_S0S1_SW_ANTENNA_DIVERSITY
	odm_s0s1_sw_ant_div_by_ctrl_frame_process_rssi(dm, phy_info, pktinfo);
#endif

	sta = dm->phydm_sta_info[pktinfo->station_id];

	if (!is_sta_active(sta)) {
		return;
		/**/
	}

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN))
	if ((dm->support_ability & ODM_BB_ANT_DIV) &&
	    fat_tab->enable_ctrl_frame_antdiv) {
		if (pktinfo->is_packet_match_bssid)
			dm->data_frame_num++;

		if (fat_tab->use_ctrl_frame_antdiv) {
			if (!pktinfo->is_to_self) /*data frame + CTRL frame*/
				return;
		} else {
			if (!pktinfo->is_packet_match_bssid) /*data frame only*/
				return;
		}
	} else
#endif
	{
		if (!pktinfo->is_packet_match_bssid) /*data frame only*/
			return;
	}

	if (pktinfo->is_packet_beacon) {
		dm->phy_dbg_info.num_qry_beacon_pkt++;
		dm->phy_dbg_info.beacon_phy_rate = pktinfo->data_rate;
	}

	/* --------------Statistic for antenna/path diversity------------------ */
	if (dm->support_ability & ODM_BB_ANT_DIV)
#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
		odm_process_rssi_for_ant_div(dm, phy_info, pktinfo);
#else
		;
#endif
#if (defined(CONFIG_PATH_DIVERSITY))
	else if (dm->support_ability & ODM_BB_PATH_DIV)
		phydm_process_rssi_for_path_div(dm, phy_info, pktinfo);
#endif
	/* -----------------Smart Antenna Debug Message------------------ */

	undecorated_smoothed_cck = sta->rssi_stat.rssi_cck;
	undecorated_smoothed_ofdm = sta->rssi_stat.rssi_ofdm;
	undecorated_smoothed_pwdb = sta->rssi_stat.rssi;

	if (pktinfo->is_packet_to_self || pktinfo->is_packet_beacon) {
		if (!pktinfo->is_cck_rate) { /* ofdm rate */
#if (RTL8814A_SUPPORT == 1)
			if (dm->support_ic_type & (ODM_RTL8814A)) {
				u8 rx_count = 0;
				u32 rssi_linear = 0;

				if (dm->rx_ant_status & BB_PATH_A) {
					rx_count++;
					rssi_linear += odm_convert_to_linear(phy_info->rx_mimo_signal_strength[RF_PATH_A]);
				}

				if (dm->rx_ant_status & BB_PATH_B) {
					rx_count++;
					rssi_linear += odm_convert_to_linear(phy_info->rx_mimo_signal_strength[RF_PATH_B]);
				}

				if (dm->rx_ant_status & BB_PATH_C) {
					rx_count++;
					rssi_linear += odm_convert_to_linear(phy_info->rx_mimo_signal_strength[RF_PATH_C]);
				}

				if (dm->rx_ant_status & BB_PATH_D) {
					rx_count++;
					rssi_linear += odm_convert_to_linear(phy_info->rx_mimo_signal_strength[RF_PATH_D]);
				}

				/* Calculate average RSSI */
				switch (rx_count) {
				case 2:
					rssi_linear = (rssi_linear >> 1);
					break;
				case 3:
					rssi_linear = ((rssi_linear) + (rssi_linear << 1) + (rssi_linear << 3)) >> 5; /* rssi_linear/3 ~ rssi_linear*11/32 */
					break;
				case 4:
					rssi_linear = (rssi_linear >> 2);
					break;
				}
				rssi_ave = odm_convert_to_db(rssi_linear);
			} else
#endif
			{
				if (phy_info->rx_mimo_signal_strength[RF_PATH_B] == 0) {
					rssi_ave = phy_info->rx_mimo_signal_strength[RF_PATH_A];
				} else {
					/*dbg_print("rfd->status.rx_mimo_signal_strength[0] = %d, rfd->status.rx_mimo_signal_strength[1] = %d\n",*/
					/*rfd->status.rx_mimo_signal_strength[0], rfd->status.rx_mimo_signal_strength[1]);*/

					if (phy_info->rx_mimo_signal_strength[RF_PATH_A] > phy_info->rx_mimo_signal_strength[RF_PATH_B]) {
						rssi_max = phy_info->rx_mimo_signal_strength[RF_PATH_A];
						rssi_min = phy_info->rx_mimo_signal_strength[RF_PATH_B];
					} else {
						rssi_max = phy_info->rx_mimo_signal_strength[RF_PATH_B];
						rssi_min = phy_info->rx_mimo_signal_strength[RF_PATH_A];
					}
					if ((rssi_max - rssi_min) < 3)
						rssi_ave = rssi_max;
					else if ((rssi_max - rssi_min) < 6)
						rssi_ave = rssi_max - 1;
					else if ((rssi_max - rssi_min) < 10)
						rssi_ave = rssi_max - 2;
					else
						rssi_ave = rssi_max - 3;
				}
			}

			/* 1 Process OFDM RSSI */
			if (undecorated_smoothed_ofdm <= 0) { /* initialize */
				undecorated_smoothed_ofdm = (s8)phy_info->rx_pwdb_all;
				PHYDM_DBG(dm, DBG_RSSI_MNTR,
					  "OFDM_INIT: (( %d ))\n",
					  undecorated_smoothed_ofdm);
			} else {
				if (phy_info->rx_pwdb_all > (u32)undecorated_smoothed_ofdm) {
					undecorated_smoothed_ofdm =
						(s8)((((undecorated_smoothed_ofdm) * (RX_SMOOTH_FACTOR - 1)) +
						      (rssi_ave)) /
						     (RX_SMOOTH_FACTOR));
					undecorated_smoothed_ofdm = undecorated_smoothed_ofdm + 1;
					PHYDM_DBG(dm, DBG_RSSI_MNTR, "OFDM_1: (( %d ))\n", undecorated_smoothed_ofdm);
				} else {
					undecorated_smoothed_ofdm =
						(s8)((((undecorated_smoothed_ofdm) * (RX_SMOOTH_FACTOR - 1)) +
						      (rssi_ave)) /
						     (RX_SMOOTH_FACTOR));
					PHYDM_DBG(dm, DBG_RSSI_MNTR, "OFDM_2: (( %d ))\n", undecorated_smoothed_ofdm);
				}
			}
			if (sta->rssi_stat.ofdm_pkt_cnt != 64) {
				i = 63;
				sta->rssi_stat.ofdm_pkt_cnt -= (u8)(((sta->rssi_stat.packet_map >> i) & BIT(0)) - 1);
			}
			sta->rssi_stat.packet_map = (sta->rssi_stat.packet_map << 1) | BIT(0);

		} else {
			rssi_ave = phy_info->rx_pwdb_all;

			if (sta->rssi_stat.cck_pkt_cnt <= 63)
				sta->rssi_stat.cck_pkt_cnt++;

			/* 1 Process CCK RSSI */
			if (undecorated_smoothed_cck <= 0) { /* initialize */
				undecorated_smoothed_cck = (s8)phy_info->rx_pwdb_all;
				sta->rssi_stat.cck_sum_power = (u16)phy_info->rx_pwdb_all; /*reset*/
				sta->rssi_stat.cck_pkt_cnt = 1; /*reset*/
				PHYDM_DBG(dm, DBG_RSSI_MNTR,
					  "CCK_INIT: (( %d ))\n",
					  undecorated_smoothed_cck);
			} else if (sta->rssi_stat.cck_pkt_cnt <= CCK_RSSI_INIT_COUNT) {
				sta->rssi_stat.cck_sum_power = sta->rssi_stat.cck_sum_power + (u16)phy_info->rx_pwdb_all;
				undecorated_smoothed_cck = sta->rssi_stat.cck_sum_power / sta->rssi_stat.cck_pkt_cnt;

				PHYDM_DBG(dm, DBG_RSSI_MNTR,
					  "CCK_0: (( %d )), SumPow = (( %d )), cck_pkt = (( %d ))\n",
					  undecorated_smoothed_cck,
					  sta->rssi_stat.cck_sum_power,
					  sta->rssi_stat.cck_pkt_cnt);
			} else {
				if (phy_info->rx_pwdb_all > (u32)undecorated_smoothed_cck) {
					undecorated_smoothed_cck =
						(s8)((((undecorated_smoothed_cck) * (RX_SMOOTH_FACTOR - 1)) +
						      (phy_info->rx_pwdb_all)) /
						     (RX_SMOOTH_FACTOR));
					undecorated_smoothed_cck = undecorated_smoothed_cck + 1;
					PHYDM_DBG(dm, DBG_RSSI_MNTR, "CCK_1: (( %d ))\n", undecorated_smoothed_cck);
				} else {
					undecorated_smoothed_cck =
						(s8)((((undecorated_smoothed_cck) * (RX_SMOOTH_FACTOR - 1)) +
						      (phy_info->rx_pwdb_all)) /
						     (RX_SMOOTH_FACTOR));
					PHYDM_DBG(dm, DBG_RSSI_MNTR, "CCK_2: (( %d ))\n", undecorated_smoothed_cck);
				}
			}
			i = 63;
			sta->rssi_stat.ofdm_pkt_cnt -= (u8)((sta->rssi_stat.packet_map >> i) & BIT(0));
			sta->rssi_stat.packet_map = sta->rssi_stat.packet_map << 1;
		}

		/* if(entry) */

		/* 2011.07.28 LukeLee: modified to prevent unstable CCK RSSI */
		if (sta->rssi_stat.ofdm_pkt_cnt == 64) { /* speed up when all packets are OFDM*/
			undecorated_smoothed_pwdb = undecorated_smoothed_ofdm;
			PHYDM_DBG(dm, DBG_RSSI_MNTR, "PWDB_0[%d] = (( %d ))\n",
				  pktinfo->station_id,
				  undecorated_smoothed_cck);
		} else {
			if (sta->rssi_stat.valid_bit < 64)
				sta->rssi_stat.valid_bit++;

			if (sta->rssi_stat.valid_bit == 64) {
				weighting = ((sta->rssi_stat.ofdm_pkt_cnt) > 4) ? 64 : (sta->rssi_stat.ofdm_pkt_cnt << 4);
				undecorated_smoothed_pwdb = (s8)((weighting * undecorated_smoothed_ofdm + (64 - weighting) * undecorated_smoothed_cck) >> 6);
				PHYDM_DBG(dm, DBG_RSSI_MNTR,
					  "PWDB_1[%d] = (( %d )), W = (( %d ))\n",
					  pktinfo->station_id,
					  undecorated_smoothed_cck, weighting);
			} else {
				if (sta->rssi_stat.valid_bit != 0)
					undecorated_smoothed_pwdb =
						(sta->rssi_stat.ofdm_pkt_cnt * undecorated_smoothed_ofdm + (sta->rssi_stat.valid_bit - sta->rssi_stat.ofdm_pkt_cnt) * undecorated_smoothed_cck) / sta->rssi_stat.valid_bit;
				else
					undecorated_smoothed_pwdb = 0;

				PHYDM_DBG(dm, DBG_RSSI_MNTR,
					  "PWDB_2[%d] = (( %d )), ofdm_pkt = (( %d )), Valid_Bit = (( %d ))\n",
					  pktinfo->station_id,
					  undecorated_smoothed_cck,
					  sta->rssi_stat.ofdm_pkt_cnt,
					  sta->rssi_stat.valid_bit);
			}
		}

		if ((sta->rssi_stat.ofdm_pkt_cnt >= 1 || sta->rssi_stat.cck_pkt_cnt >= 5) && sta->rssi_stat.is_send_rssi == RA_RSSI_STATE_INIT) {
			send_rssi_2_fw = 1;
			sta->rssi_stat.is_send_rssi = RA_RSSI_STATE_SEND;
		}

		sta->rssi_stat.rssi_cck = undecorated_smoothed_cck;
		sta->rssi_stat.rssi_ofdm = undecorated_smoothed_ofdm;
		sta->rssi_stat.rssi = undecorated_smoothed_pwdb;

		if (send_rssi_2_fw) { /* Trigger init rate by RSSI */

			if (sta->rssi_stat.ofdm_pkt_cnt != 0)
				sta->rssi_stat.rssi = undecorated_smoothed_ofdm;

			PHYDM_DBG(dm, DBG_RSSI_MNTR,
				  "[Send to FW] PWDB = (( %d )), ofdm_pkt = (( %d )), cck_pkt = (( %d ))\n",
				  undecorated_smoothed_pwdb,
				  sta->rssi_stat.ofdm_pkt_cnt,
				  sta->rssi_stat.cck_pkt_cnt);
		}

		/* dbg_print("ofdm_pkt=%d, weighting=%d\n", ofdm_pkt_cnt, weighting); */
		/* dbg_print("undecorated_smoothed_ofdm=%d, undecorated_smoothed_pwdb=%d, undecorated_smoothed_cck=%d\n", */
		/*	undecorated_smoothed_ofdm, undecorated_smoothed_pwdb, undecorated_smoothed_cck); */
	}
}

/*
 * Endianness before calling this API
 */
#ifdef PHYSTS_3RD_TYPE_SUPPORT
void phydm_print_phystat_jaguar3(
	struct dm_struct *dm,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info,
	u8 phy_status_page_num)
{
	struct phy_status_rpt_jaguar3_type0 *rpt0 = (struct phy_status_rpt_jaguar3_type0 *)phy_status_inf;
	struct phy_status_rpt_jaguar3_type1 *rpt1 = (struct phy_status_rpt_jaguar3_type1 *)phy_status_inf;
	struct phy_status_rpt_jaguar3_type2_type3 *rpt2 = (struct phy_status_rpt_jaguar3_type2_type3 *)phy_status_inf;
	struct phy_status_rpt_jaguar3_type4 *rpt3 = (struct phy_status_rpt_jaguar3_type4 *)phy_status_inf;
	struct phy_status_rpt_jaguar3_type5 *rpt4 = (struct phy_status_rpt_jaguar3_type5 *)phy_status_inf;
	struct odm_phy_dbg_info *dbg = &dm->phy_dbg_info;
	u32 phy_status[PHY_STATUS_JRGUAR3_DW_LEN] = {0};
	u8 i;

	odm_move_memory(dm, phy_status, phy_status_inf, (PHY_STATUS_JRGUAR3_DW_LEN << 2));

	if (!(dm->debug_components & DBG_PHY_STATUS))
		return;

	if (dbg->show_phy_sts_all_pkt == 0) {
		if (!pktinfo->is_packet_match_bssid)
			return;
	}

	dbg->show_phy_sts_cnt++;
	/*dbg_print("cnt=%d, max=%d\n", dbg->show_phy_sts_cnt, dbg->show_phy_sts_max_cnt);*/

	if (dbg->show_phy_sts_max_cnt != SHOW_PHY_STATUS_UNLIMITED) {
		if (dbg->show_phy_sts_cnt > dbg->show_phy_sts_max_cnt)
			return;
	}

	pr_debug("Phy Status Rpt: OFDM_%d\n", phy_status_page_num);
	pr_debug("StaID=%d, RxRate = 0x%x match_bssid=%d\n", pktinfo->station_id, pktinfo->data_rate, pktinfo->is_packet_match_bssid);

	for (i = 0; i < PHY_STATUS_JRGUAR3_DW_LEN; i++)
		pr_debug("Offset[%d:%d] = 0x%x\n", ((4 * i) + 3), (4 * i), phy_status[i]);

	if (phy_status_page_num == 0) { /* CCK(default) */
		pr_debug("[0] Pkt_cnt=%d, Channel_msb=%d, Pwdb_a=%d, Gain_a=%d, TRSW=%d, AGC_table_b=%d, AGC_table_c=%d,\n",
			 rpt0->pkt_cnt, rpt0->channel_msb, rpt0->pwdb_a, rpt0->gain_a, rpt0->trsw, rpt0->agc_table_b, rpt0->agc_table_c);
		pr_debug("[4] Gnt_BT_keep_cnt=%d, HW_AntSW_occur_keep_cck=%d,\n Band=%d, Channel=%d, AGC_table_a=%d, l_RXSC=%d, AGC_table_d=%d\n",
			 rpt0->gnt_bt_keep_cck, rpt0->hw_antsw_occur_keep_cck,
			 rpt0->band, rpt0->channel, rpt0->agc_table_a, rpt0->l_rxsc, rpt0->agc_table_d);
		pr_debug("[8] AntIdx={%d, %d, %d, %d}, Length=%d\n",
			 rpt0->antidx_d, rpt0->antidx_c, rpt0->antidx_b, rpt0->antidx_a, rpt0->length);
		pr_debug("[12] lna_h_a=%d, CCK_BB_power_a=%d, lna_l_a=%d, vga_a=%d, sq=%d\n",
			 rpt0->lna_h_a, rpt0->bb_power_a, rpt0->lna_l_a, rpt0->vga_a, rpt0->signal_quality);
		pr_debug("[16] Gain_b=%d, lna_h_b=%d, CCK_BB_power_b=%d, lna_l_b=%d, vga_b=%d, Pwdb_b=%d\n",
			 rpt0->gain_b, rpt0->lna_h_b, rpt0->bb_power_b, rpt0->lna_l_b, rpt0->vga_b, rpt0->pwdb_b);
		pr_debug("[20] Gain_c=%d, lna_h_c=%d, CCK_BB_power_c=%d, lna_l_c=%d, vga_c=%d, Pwdb_c=%d\n",
			 rpt0->gain_c, rpt0->lna_h_c, rpt0->bb_power_c, rpt0->lna_l_c, rpt0->vga_c, rpt0->pwdb_c);
		pr_debug("[24] Gain_d=%d, lna_h_d=%d, CCK_BB_power_d=%d, lna_l_d=%d, vga_d=%d, Pwdb_d=%d\n",
			 rpt0->gain_c, rpt0->lna_h_c, rpt0->bb_power_c, rpt0->lna_l_c, rpt0->vga_c, rpt0->pwdb_c);
	} else if (phy_status_page_num == 1) {
		pr_debug("[0] pwdb[C:A]={%d, %d, %d}, Channel_pri_msb=%d, Pkt_cnt=%d,\n",
			 rpt1->pwdb_c, rpt1->pwdb_b, rpt1->pwdb_a, rpt1->channel_pri_msb, rpt1->pkt_cnt);
		pr_debug("[4] BF: %d, stbc=%d, ldpc=%d, gnt_bt=%d, band=%d, Ch_pri_lsb=%d, rxsc[ht, l]={%d, %d}\n",
			 rpt1->beamformed, rpt1->stbc, rpt1->ldpc, rpt1->gnt_bt,
			 rpt1->band, rpt1->channel_pri_lsb, rpt1->ht_rxsc, rpt1->l_rxsc);
		pr_debug("[8] AntIdx[D:A]={%d, %d, %d, %d}, HW_AntSW_occur[D:A]={%d, %d, %d,%d}, Channel_sec[msb,lsb]={%d, %d}\n",
			 rpt1->antidx_d, rpt1->antidx_c, rpt1->antidx_b, rpt1->antidx_a,
			 rpt1->hw_antsw_occur_d, rpt1->hw_antsw_occur_c, rpt1->hw_antsw_occur_b, rpt1->hw_antsw_occur_a,
			 rpt1->channel_sec_msb, rpt1->channel_sec_lsb);
		pr_debug("[12] GID=%d, PAID[msb,lsb]={%d,%d}\n",
			 rpt1->gid, rpt1->paid_msb, rpt1->paid);
		pr_debug("[16] RX_EVM[D:A]={%d, %d, %d, %d}\n",
			 rpt1->rxevm[3], rpt1->rxevm[2], rpt1->rxevm[1], rpt1->rxevm[0]);
		pr_debug("[20] CFO_tail[D:A]={%d, %d, %d, %d}\n",
			 rpt1->cfo_tail[3], rpt1->cfo_tail[2], rpt1->cfo_tail[1], rpt1->cfo_tail[0]);
		pr_debug("[24] RX_SNR[D:A]={%d, %d, %d, %d}\n\n",
			 rpt1->rxsnr[3], rpt1->rxsnr[2], rpt1->rxsnr[1], rpt1->rxsnr[0]);
	} else if (phy_status_page_num == 2) { /*type 2, 3*/
		pr_debug("[0] pwdb[C:A]={%d, %d, %d}, Channel_mdb=%d, Pkt_cnt=%d\n",
			 rpt2->pwdb[2], rpt2->pwdb[1], rpt2->pwdb[0],
			 rpt2->channel_msb, rpt2->pkt_cnt);
		pr_debug("[4] BF=%d, STBC=%d, LDPC=%d, Gnt_BT=%d, HW_AntSW_occur=%d, band=%d, CH_lsb=%d, rxsc[ht, l]={%d, %d}, pwdb_D=%d\n",
			 rpt2->beamformed, rpt2->stbc, rpt2->ldpc, rpt2->gnt_bt,
			 rpt2->hw_antsw_occu, rpt2->band, rpt2->channel_lsb,
			 rpt2->ht_rxsc, rpt2->l_rxsc, rpt2->pwdb[3]);
		pr_debug("[8] AgcTab[D:A]={%d, %d, %d, %d}, pwed_th=%d, shift_l_map=%d\n",
			 rpt2->agc_table_d, rpt2->agc_table_c, rpt2->agc_table_b, rpt2->agc_table_a,
			 rpt2->pwed_th, rpt2->shift_l_map);
		pr_debug("[12] AvgNoisePowerdB=%d, mp_gain_c[msb, lsb]={%d, %d}, mp_gain_b[msb, lsb]={%d, %d}, mp_gain_a=%d, cnt_cca2agc_rdy=%d\n",
			 rpt2->avg_noise_pwr_lsb, rpt2->mp_gain_c_msb, rpt2->mp_gain_c_lsb,
			 rpt2->mp_gain_b_msb, rpt2->mp_gain_b_lsb, rpt2->mp_gain_a,
			 rpt2->cnt_cca2agc_rdy);
		pr_debug("[16] HT AAGC gain[B:A]={%d, %d}, AAGC step[D:A]={%d, %d, %d, %d}, IsFreqSelectFadimg=%d\n",
			 rpt2->ht_aagc_gain[1], rpt2->ht_aagc_gain[0],
			 rpt2->aagc_step_d, rpt2->aagc_step_c, rpt2->aagc_step_b, rpt2->aagc_step_a,
			 rpt2->is_freq_select_fading);
		pr_debug("[20] DAGC gain ant[B:A]={%d, %d}, HT AAGC gain[D:C]={%d, %d}\n",
			 rpt2->dagc_gain[1], rpt2->dagc_gain[0],
			 rpt2->ht_aagc_gain[3], rpt2->ht_aagc_gain[2]);
		pr_debug("[24] AvgNoisePwerdB=%d, syn_count[msb, lsb]={%d, %d}, counter=%d, DAGC gain ant[D:C]={%d, %d}\n",
			 rpt2->avg_noise_pwr_msb, rpt2->syn_count_msb, rpt2->syn_count_lsb,
			 rpt2->dagc_gain[3], rpt2->dagc_gain[2]);
	} else if (phy_status_page_num == 3) { /*type 4*/
		pr_debug("[0] pwdb[C:A]={%d, %d, %d}, Channel_mdb=%d, Pkt_cnt=%d\n",
			 rpt3->pwdb[2], rpt3->pwdb[1], rpt3->pwdb[0],
			 rpt3->channel_msb, rpt3->pkt_cnt);
		pr_debug("[4] BF=%d, STBC=%d, LDPC=%d, GNT_BT=%d, band=%d, CH_pri=%d, rxsc[ht, l]={%d, %d}, pwdb_D=%d\n",
			 rpt3->beamformed, rpt3->stbc, rpt3->ldpc, rpt3->gnt_bt,
			 rpt3->band, rpt3->channel_lsb, rpt3->ht_rxsc, rpt3->l_rxsc, rpt3->pwdb[3]);
		pr_debug("[8] AntIdx[D:A]={%d, %d, %d, %d}, HW_AntSW_occur[D:A]={%d, %d, %d, %d}, Training_done[D:A]={%d, %d, %d, %d},\n    BadToneCnt_CN_excess_0=%d, BadToneCnt_min_eign_0=%d\n",
			 rpt3->antidx_d, rpt3->antidx_c, rpt3->antidx_b, rpt3->antidx_a,
			 rpt3->hw_antsw_occur_d, rpt3->hw_antsw_occur_c,
			 rpt3->hw_antsw_occur_b, rpt3->hw_antsw_occur_a,
			 rpt3->training_done_d, rpt3->training_done_c,
			 rpt3->training_done_b, rpt3->training_done_a,
			 rpt3->bad_tone_cnt_cn_excess_0, rpt3->bad_tone_cnt_min_eign_0);
		pr_debug("[12] avg_cond_num_1_msb=%d, avg_cond_num_1_lsb=%d, avg_cond_num_0=%d, bad_tone_cnt_cn_excess_1=%d, bad_tone_cnt_min_eign_1=%d\n",
			 rpt3->avg_cond_num_1_msb, rpt3->avg_cond_num_1_lsb,
			 rpt3->avg_cond_num_0, rpt3->bad_tone_cnt_cn_excess_1,
			 rpt3->bad_tone_cnt_min_eign_1);
		pr_debug("[16] Stream RXEVM[D:A]={%d, %d, %d, %d}\n",
			 rpt3->rxevm[3], rpt3->rxevm[2], rpt3->rxevm[1], rpt3->rxevm[0]);
		pr_debug("[20] Eigenvalue[D:A]={%d, %d, %d, %d}\n",
			 rpt3->eigenvalue[3], rpt3->eigenvalue[2],
			 rpt3->eigenvalue[1], rpt3->eigenvalue[0]);
		pr_debug("[24] RX SNR[D:A]={%d, %d, %d, %d}\n",
			 rpt3->rxsnr[3], rpt3->rxsnr[2], rpt3->rxsnr[1], rpt3->rxsnr[0]);
	} else if (phy_status_page_num == 4) { /*type 5*/
		pr_debug("[0] pwdb[C:A]={%d, %d, %d}, Channel_mdb=%d, Pkt_cnt=%d\n",
			 rpt4->pwdb[2], rpt4->pwdb[1], rpt4->pwdb[0],
			 rpt4->channel_msb, rpt4->pkt_cnt);
		pr_debug("[4] BF=%d, STBC=%d, LDPC=%d, GNT_BT=%d, band=%d, CH_pri=%d, rxsc[ht, l]={%d, %d}, pwdb_D=%d\n",
			 rpt4->beamformed, rpt4->stbc, rpt4->ldpc, rpt4->gnt_bt,
			 rpt4->band, rpt4->channel_lsb, rpt4->ht_rxsc, rpt4->l_rxsc, rpt4->pwdb[3]);
		pr_debug("[8] AntIdx[D:A]={%d, %d, %d, %d}, HW_AntSW_occur[D:A]={%d, %d, %d, %d}\n",
			 rpt4->antidx_d, rpt4->antidx_c, rpt4->antidx_b, rpt4->antidx_a,
			 rpt4->hw_antsw_occur_d, rpt4->hw_antsw_occur_c,
			 rpt4->hw_antsw_occur_b, rpt4->hw_antsw_occur_a);
		pr_debug("[12] Inf_posD[1,0]={%d, %d}, Inf_posC[1,0]={%d, %d}, Inf_posB[1,0]={%d, %d}, Inf_posA[1,0]={%d, %d}, Tx_pkt_cnt=%d\n",
			 rpt4->inf_pos_1_D_flg, rpt4->inf_pos_0_D_flg,
			 rpt4->inf_pos_1_C_flg, rpt4->inf_pos_0_C_flg,
			 rpt4->inf_pos_1_B_flg, rpt4->inf_pos_0_B_flg,
			 rpt4->inf_pos_1_A_flg, rpt4->inf_pos_0_A_flg,
			 rpt4->tx_pkt_cnt);
		pr_debug("[16] Inf_pos_B[1,0]={%d, %d}, Inf_pos_A[1,0]={%d, %d}\n",
			 rpt4->inf_pos_1_b, rpt4->inf_pos_0_b,
			 rpt4->inf_pos_1_a, rpt4->inf_pos_0_a);
		pr_debug("[20] Inf_pos_D[1,0]={%d, %d}, Inf_pos_C[1,0]={%d, %d}\n",
			 rpt4->inf_pos_1_d, rpt4->inf_pos_0_d,
			 rpt4->inf_pos_1_c, rpt4->inf_pos_0_c);
	}
}

void phydm_reset_phy_info_3rd(
	struct dm_struct *phydm,
	struct phydm_phyinfo_struct *phy_info)
{
	phy_info->rx_pwdb_all = 0;
	phy_info->signal_quality = 0;
	phy_info->band_width = 0;
	phy_info->rx_count = 0;
	odm_memory_set(phydm, phy_info->rx_mimo_signal_quality, 0, 4);
	odm_memory_set(phydm, phy_info->rx_mimo_signal_strength, 0, 4);
	odm_memory_set(phydm, phy_info->rx_snr, 0, 4);

	phy_info->rx_power = -110;
	phy_info->recv_signal_power = -110;
	phy_info->bt_rx_rssi_percentage = 0;
	phy_info->signal_strength = 0;
	phy_info->channel = 0;
	phy_info->is_mu_packet = 0;
	phy_info->is_beamformed = 0;
	phy_info->rxsc = 0;
	odm_memory_set(phydm, phy_info->rx_pwr, -110, 4);
	odm_memory_set(phydm, phy_info->cfo_short, 0, 8);
	odm_memory_set(phydm, phy_info->cfo_tail, 0, 8);

	odm_memory_set(phydm, phy_info->rx_mimo_evm_dbm, 0, 4);
}
phydm_per_path_phy_info_3rd(
	u8 rx_path,
	s8 rx_pwr,
	s8 rx_evm,
	s8 cfo_tail,
	s8 rx_snr,
	struct phydm_phyinfo_struct *phy_info)
{
	u8 evm_dbm = 0;
	u8 evm_percentage = 0;

	/* SNR is S(8,1), EVM is S(8,1), CFO is S(8,7) */

	if (rx_evm < 0) {
		/* Calculate EVM in dBm */
		evm_dbm = ((u8)(0 - rx_evm) >> 1);

		if (evm_dbm == 64)
			evm_dbm = 0; /*if 1SS rate, evm_dbm [2nd stream] =64*/

		if (evm_dbm != 0) {
			/* Convert EVM to 0%~100% percentage */
			if (evm_dbm >= 34)
				evm_percentage = 100;
			else
				evm_percentage = (evm_dbm << 1) + (evm_dbm);
		}
	}

	phy_info->rx_pwr[rx_path] = rx_pwr;

	phy_info->cfo_tail[rx_path] = (cfo_tail * 5) >> 1; /* CFO(kHz) = CFO_tail * 312.5(kHz) / 2^7 ~= CFO tail * 5/2 (kHz)*/
	phy_info->rx_mimo_evm_dbm[rx_path] = evm_dbm;
	phy_info->rx_mimo_signal_strength[rx_path] = phydm_query_rx_pwr_percentage(rx_pwr);
	phy_info->rx_mimo_signal_quality[rx_path] = evm_percentage;
	phy_info->rx_snr[rx_path] = rx_snr >> 1;
}

void phydm_common_phy_info_3rd(
	s8 rx_power,
	u8 channel,
	boolean is_beamformed,
	boolean is_mu_packet,
	u8 bandwidth,
	u8 signal_quality,
	u8 rxsc,
	struct phydm_phyinfo_struct *phy_info)
{
	phy_info->rx_power = rx_power; /* RSSI in dB */
	phy_info->recv_signal_power = rx_power; /* RSSI in dB */
	phy_info->channel = channel; /* channel number */
	phy_info->is_beamformed = is_beamformed; /* apply BF */
	phy_info->is_mu_packet = is_mu_packet; /* MU packet */
	phy_info->rxsc = rxsc;

	phy_info->rx_pwdb_all = phydm_query_rx_pwr_percentage(rx_power); /* RSSI in percentage */
	phy_info->signal_quality = signal_quality; /* signal quality */
	phy_info->band_width = bandwidth; /* bandwidth */

#if 0
	/* if (pktinfo->is_packet_match_bssid) */
	{
		dbg_print("rx_pwdb_all = %d, rx_power = %d, recv_signal_power = %d\n", phy_info->rx_pwdb_all, phy_info->rx_power, phy_info->recv_signal_power);
		dbg_print("signal_quality = %d\n", phy_info->signal_quality);
		dbg_print("is_beamformed = %d, is_mu_packet = %d, rx_count = %d\n", phy_info->is_beamformed, phy_info->is_mu_packet, phy_info->rx_count + 1);
		dbg_print("channel = %d, rxsc = %d, band_width = %d\n", channel, rxsc, bandwidth);
	}
#endif
}

void phydm_get_physts_jarguar3_0(
	struct dm_struct *dm,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info)
{
	/* type 0 is used for cck packet */
	struct phy_status_rpt_jaguar3_type0 *phy_sta_rpt = (struct phy_status_rpt_jaguar3_type0 *)phy_status_inf;
	u8 sq = 0, i;
	s8 rx_power[4];
	s8 rx_pwr_db_max = -120;

	/* Setting the RX power: agc_idx -110 dBm*/
	rx_power[0] = phy_sta_rpt->pwdb_a - 110;
	rx_power[1] = phy_sta_rpt->pwdb_b - 110;
	rx_power[2] = phy_sta_rpt->pwdb_c - 110;
	rx_power[3] = phy_sta_rpt->pwdb_d - 110;

	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (rx_power[i] > rx_pwr_db_max)
			rx_pwr_db_max = rx_power[i];
	}
	if (pktinfo->is_to_self) {
		dm->ofdm_agc_idx[0] = phy_sta_rpt->pwdb_a;
		dm->ofdm_agc_idx[1] = phy_sta_rpt->pwdb_b;
		dm->ofdm_agc_idx[2] = phy_sta_rpt->pwdb_c;
		dm->ofdm_agc_idx[3] = phy_sta_rpt->pwdb_d;
	}

	/* Calculate Signal Quality*/
	if (phy_sta_rpt->signal_quality >= 64) {
		sq = 0;
	} else if (phy_sta_rpt->signal_quality <= 20) {
		sq = 100;
	} else {
		/* mapping to 2~99% */
		sq = 64 - phy_sta_rpt->signal_quality;
		sq = ((sq << 3) + sq) >> 2;
	}

	/* Modify CCK PWDB if old AGC */
	if (!dm->cck_new_agc) {
		u8 lna_idx[4], vga_idx[4];

		lna_idx[0] = ((phy_sta_rpt->lna_h_a << 3) | phy_sta_rpt->lna_l_a);
		vga_idx[0] = phy_sta_rpt->vga_a;
		lna_idx[1] = ((phy_sta_rpt->lna_h_b << 3) | phy_sta_rpt->lna_l_b);
		vga_idx[1] = phy_sta_rpt->vga_b;
		lna_idx[2] = ((phy_sta_rpt->lna_h_c << 3) | phy_sta_rpt->lna_l_c);
		vga_idx[2] = phy_sta_rpt->vga_c;
		lna_idx[3] = ((phy_sta_rpt->lna_h_d << 3) | phy_sta_rpt->lna_l_d);
		vga_idx[3] = phy_sta_rpt->vga_d;
		#if (RTL8198F_SUPPORT == 1)
			/*phydm_cck_rssi_8198f*/
		#endif
	}

	/*CCK no STBC and LDPC*/
	dm->phy_dbg_info.is_ldpc_pkt = false;
	dm->phy_dbg_info.is_stbc_pkt = false;

	/* Update Common information */
	phydm_common_phy_info_3rd(rx_pwr_db_max, phy_sta_rpt->channel, false,
				  false, CHANNEL_WIDTH_20, sq, phy_sta_rpt->l_rxsc, phy_info);

	/* Update CCK pwdb */
	phydm_per_path_phy_info_3rd(RF_PATH_A, rx_power[0], 0, 0, 0, phy_info); /* Update per-path information */
	phydm_per_path_phy_info_3rd(RF_PATH_B, rx_power[1], 0, 0, 0, phy_info);
	phydm_per_path_phy_info_3rd(RF_PATH_C, rx_power[2], 0, 0, 0, phy_info);
	phydm_per_path_phy_info_3rd(RF_PATH_D, rx_power[3], 0, 0, 0, phy_info);

	dm->dm_fat_table.antsel_rx_keep_0 = phy_sta_rpt->antidx_a;
	dm->dm_fat_table.antsel_rx_keep_1 = phy_sta_rpt->antidx_b;
	dm->dm_fat_table.antsel_rx_keep_2 = phy_sta_rpt->antidx_c;
	dm->dm_fat_table.antsel_rx_keep_3 = phy_sta_rpt->antidx_d;
}

void phydm_get_physts_jarguar3_1(
	struct dm_struct *dm,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info)
{
	/* type 1 is used for ofdm packet */
	struct phy_status_rpt_jaguar3_type1 *phy_sta_rpt = (struct phy_status_rpt_jaguar3_type1 *)phy_status_inf;
	s8 rx_pwr_db = -120;
	s8 rx_path_pwr_db;
	u8 i, rxsc, bw = CHANNEL_WIDTH_20, rx_count = 0;
	u8 pwdb[4];
	boolean is_mu;

	pwdb[0] = phy_sta_rpt->pwdb_a;
	pwdb[1] = phy_sta_rpt->pwdb_b;
	pwdb[2] = phy_sta_rpt->pwdb_c;
	pwdb[3] = phy_sta_rpt->pwdb_d;

	/* Update per-path information */
	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (dm->rx_ant_status & BIT(i)) {
			rx_count++; /* check the number of the ant */

			if (rx_count > dm->num_rf_path)
				break;

			/* Update per-path information (RSSI_dB RSSI_percentage EVM SNR CFO sq) */
			/* EVM report is reported by stream, not path */
			rx_path_pwr_db = pwdb[i] - 110; /* per-path pwdb in dB domain */

			if (pktinfo->is_to_self)
				dm->ofdm_agc_idx[i] = pwdb[i];

			phydm_per_path_phy_info_3rd(i, rx_path_pwr_db, phy_sta_rpt->rxevm[i],
						    phy_sta_rpt->cfo_tail[i], phy_sta_rpt->rxsnr[i], phy_info);

			/* search maximum pwdb */
			if (rx_path_pwr_db > rx_pwr_db)
				rx_pwr_db = rx_path_pwr_db;
		}
	}

	/* mapping RX counter from 1~4 to 0~3 */
	if (rx_count > 0)
		phy_info->rx_count = rx_count - 1;

	/* Check if MU packet or not */
	if (phy_sta_rpt->gid != 0 && phy_sta_rpt->gid != 63) {
		is_mu = true;
		dm->phy_dbg_info.num_qry_mu_pkt++;
	} else {
		is_mu = false;
	}

	/* count BF packet */
	dm->phy_dbg_info.num_qry_bf_pkt = dm->phy_dbg_info.num_qry_bf_pkt + phy_sta_rpt->beamformed;

	/*STBC or LDPC pkt*/
	dm->phy_dbg_info.is_ldpc_pkt = phy_sta_rpt->ldpc;
	dm->phy_dbg_info.is_stbc_pkt = phy_sta_rpt->stbc;

	/* Check sub-channel */
	if (pktinfo->data_rate > ODM_RATE11M && pktinfo->data_rate < ODM_RATEMCS0)
		rxsc = phy_sta_rpt->l_rxsc; /*Legacy*/
	else
		rxsc = phy_sta_rpt->ht_rxsc; /* HT and VHT */

	/* Check RX bandwidth */
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (rxsc >= 1 && rxsc <= 8)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc >= 9) && (rxsc <= 12))
			bw = CHANNEL_WIDTH_40;
		else if (rxsc >= 13)
			bw = CHANNEL_WIDTH_80;

	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (rxsc == 1 || rxsc == 2)
			bw = CHANNEL_WIDTH_20;
		else
			bw = CHANNEL_WIDTH_40;
	}

	/* Update packet information */
	/* RX power choose the path with the maximum power */
	phydm_common_phy_info_3rd(rx_pwr_db, phy_sta_rpt->channel_pri_lsb, (boolean)phy_sta_rpt->beamformed,
				  is_mu, bw, phy_info->rx_mimo_signal_quality[0], rxsc, phy_info);

	phydm_parsing_cfo(dm, pktinfo, phy_sta_rpt->cfo_tail, pktinfo->rate_ss);

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	dm->dm_fat_table.antsel_rx_keep_0 = phy_sta_rpt->antidx_a;
	dm->dm_fat_table.antsel_rx_keep_1 = phy_sta_rpt->antidx_b;
	dm->dm_fat_table.antsel_rx_keep_2 = phy_sta_rpt->antidx_c;
	dm->dm_fat_table.antsel_rx_keep_3 = phy_sta_rpt->antidx_d;
#endif
}

void phydm_get_physts_jarguar3_2_3(
	struct dm_struct *dm,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info)
{
	/* type 1 is used for ofdm packet */
	struct phy_status_rpt_jaguar3_type2_type3 *phy_sta_rpt = (struct phy_status_rpt_jaguar3_type2_type3 *)phy_status_inf;
	s8 rx_pwr_db_max = -120;
	s8 rx_path_pwr_db;
	u8 i, rxsc, bw = CHANNEL_WIDTH_20, rx_count = 0;

	/* Update per-path information */
	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (dm->rx_ant_status & BIT(i)) {
			rx_count++; /* check the number of the ant */

			if (rx_count > dm->num_rf_path)
				break;

			/* Update per-path information (RSSI_dB RSSI_percentage EVM SNR CFO sq) */
			/* EVM report is reported by stream, not path */
			rx_path_pwr_db = phy_sta_rpt->pwdb[i] - 110; /* per-path pwdb in dB domain */

			if (pktinfo->is_to_self)
				dm->ofdm_agc_idx[i] = phy_sta_rpt->pwdb[i];

			phydm_per_path_phy_info_3rd(i, rx_path_pwr_db, 0,
						    0, 0, phy_info);

			/* search maximum pwdb */
			if (rx_path_pwr_db > rx_pwr_db_max)
				rx_pwr_db_max = rx_path_pwr_db;
		}
	}

	/* mapping RX counter from 1~4 to 0~3 */
	if (rx_count > 0)
		phy_info->rx_count = rx_count - 1;

	/* count BF packet */
	dm->phy_dbg_info.num_qry_bf_pkt = dm->phy_dbg_info.num_qry_bf_pkt + phy_sta_rpt->beamformed;

	/*STBC or LDPC pkt*/
	dm->phy_dbg_info.is_ldpc_pkt = phy_sta_rpt->ldpc;
	dm->phy_dbg_info.is_stbc_pkt = phy_sta_rpt->stbc;

	/* Check sub-channel */
	if (pktinfo->data_rate > ODM_RATE11M && pktinfo->data_rate < ODM_RATEMCS0)
		rxsc = phy_sta_rpt->l_rxsc; /*Legacy*/
	else
		rxsc = phy_sta_rpt->ht_rxsc; /* HT and VHT */

	/* Check RX bandwidth */
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (rxsc >= 1 && rxsc <= 8)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc >= 9) && (rxsc <= 12))
			bw = CHANNEL_WIDTH_40;
		else if (rxsc >= 13)
			bw = CHANNEL_WIDTH_80;

	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (rxsc == 1 || rxsc == 2)
			bw = CHANNEL_WIDTH_20;
		else
			bw = CHANNEL_WIDTH_40;
	}

	/* Update packet information */
	/* RX power choose the path with the maximum power */
	phydm_common_phy_info_3rd(rx_pwr_db_max, phy_sta_rpt->channel_lsb, (boolean)phy_sta_rpt->beamformed,
				  false, bw, 0, rxsc, phy_info);
}
void phydm_get_physts_jarguar3_4(
	struct dm_struct *dm,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info)
{
	/* type 1 is used for ofdm packet */
	struct phy_status_rpt_jaguar3_type4 *phy_sta_rpt = (struct phy_status_rpt_jaguar3_type4 *)phy_status_inf;
	s8 rx_pwr_db_max = -120;
	s8 rx_path_pwr_db;
	u8 i, rxsc, bw = CHANNEL_WIDTH_20, rx_count = 0;

	/* Update per-path information */
	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (dm->rx_ant_status & BIT(i)) {
			rx_count++; /* check the number of the ant */

			if (rx_count > dm->num_rf_path)
				break;

			/* Update per-path information (RSSI_dB RSSI_percentage EVM SNR CFO sq) */
			/* EVM report is reported by stream, not path */
			rx_path_pwr_db = phy_sta_rpt->pwdb[i] - 110; /* per-path pwdb in dB domain */

			if (pktinfo->is_to_self)
				dm->ofdm_agc_idx[i] = phy_sta_rpt->pwdb[i];

			phydm_per_path_phy_info_3rd(i, rx_path_pwr_db, phy_sta_rpt->rxevm[i],
						    0, phy_sta_rpt->rxsnr[i], phy_info);

			/* search maximum pwdb */
			if (rx_path_pwr_db > rx_pwr_db_max)
				rx_pwr_db_max = rx_path_pwr_db;
		}
	}

	/* mapping RX counter from 1~4 to 0~3 */
	if (rx_count > 0)
		phy_info->rx_count = rx_count - 1;

	/* count BF packet */
	dm->phy_dbg_info.num_qry_bf_pkt = dm->phy_dbg_info.num_qry_bf_pkt + phy_sta_rpt->beamformed;

	/*STBC or LDPC pkt*/
	dm->phy_dbg_info.is_ldpc_pkt = phy_sta_rpt->ldpc;
	dm->phy_dbg_info.is_stbc_pkt = phy_sta_rpt->stbc;

	/* Check sub-channel */
	if (pktinfo->data_rate > ODM_RATE11M && pktinfo->data_rate < ODM_RATEMCS0)
		rxsc = phy_sta_rpt->l_rxsc; /*Legacy*/
	else
		rxsc = phy_sta_rpt->ht_rxsc; /* HT and VHT */

	/* Check RX bandwidth */
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (rxsc >= 1 && rxsc <= 8)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc >= 9) && (rxsc <= 12))
			bw = CHANNEL_WIDTH_40;
		else if (rxsc >= 13)
			bw = CHANNEL_WIDTH_80;

	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (rxsc == 1 || rxsc == 2)
			bw = CHANNEL_WIDTH_20;
		else
			bw = CHANNEL_WIDTH_40;
	}

	/* Update packet information */
	/* RX power choose the path with the maximum power */
	phydm_common_phy_info_3rd(rx_pwr_db_max, phy_sta_rpt->channel_lsb, (boolean)phy_sta_rpt->beamformed,
				  false, bw, 0, rxsc, phy_info);
}

void phydm_get_physts_jarguar3_5(
	struct dm_struct *dm,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info)
{
	/* type 1 is used for ofdm packet */
	struct phy_status_rpt_jaguar3_type5 *phy_sta_rpt = (struct phy_status_rpt_jaguar3_type5 *)phy_status_inf;
	s8 rx_pwr_db_max = -120;
	s8 rx_path_pwr_db;
	u8 i, rxsc, bw = CHANNEL_WIDTH_20, rx_count = 0;

	/* Update per-path information */
	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (dm->rx_ant_status & BIT(i)) {
			rx_count++; /* check the number of the ant */

			if (rx_count > dm->num_rf_path)
				break;

			/* Update per-path information (RSSI_dB RSSI_percentage EVM SNR CFO sq) */
			/* EVM report is reported by stream, not path */
			rx_path_pwr_db = phy_sta_rpt->pwdb[i] - 110; /* per-path pwdb in dB domain */

			if (pktinfo->is_to_self)
				dm->ofdm_agc_idx[i] = phy_sta_rpt->pwdb[i];

			phydm_per_path_phy_info_3rd(i, rx_path_pwr_db, 0, 0, 0, phy_info);

			/* search maximum pwdb */
			if (rx_path_pwr_db > rx_pwr_db_max)
				rx_pwr_db_max = rx_path_pwr_db;
		}
	}

	/* mapping RX counter from 1~4 to 0~3 */
	if (rx_count > 0)
		phy_info->rx_count = rx_count - 1;

	/* count BF packet */
	dm->phy_dbg_info.num_qry_bf_pkt = dm->phy_dbg_info.num_qry_bf_pkt + phy_sta_rpt->beamformed;

	/*STBC or LDPC pkt*/
	dm->phy_dbg_info.is_ldpc_pkt = phy_sta_rpt->ldpc;
	dm->phy_dbg_info.is_stbc_pkt = phy_sta_rpt->stbc;

	/* Check sub-channel */
	if (pktinfo->data_rate > ODM_RATE11M && pktinfo->data_rate < ODM_RATEMCS0)
		rxsc = phy_sta_rpt->l_rxsc; /*Legacy*/
	else
		rxsc = phy_sta_rpt->ht_rxsc; /* HT and VHT */

	/* Check RX bandwidth */
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (rxsc >= 1 && rxsc <= 8)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc >= 9) && (rxsc <= 12))
			bw = CHANNEL_WIDTH_40;
		else if (rxsc >= 13)
			bw = CHANNEL_WIDTH_80;

	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (rxsc == 1 || rxsc == 2)
			bw = CHANNEL_WIDTH_20;
		else
			bw = CHANNEL_WIDTH_40;
	}

	/* Update packet information */
	/* RX power choose the path with the maximum power */
	phydm_common_phy_info_3rd(rx_pwr_db_max, phy_sta_rpt->channel_lsb, (boolean)phy_sta_rpt->beamformed,
				  false, bw, 0, rxsc, phy_info);
}

void phydm_process_rssi_for_dm_3rd_type(
	struct dm_struct *dm,
	struct phydm_phyinfo_struct *phy_info,
	struct phydm_perpkt_info_struct *pktinfo)
{
	s32 rssi_pre;
	u32 rssi_linear = 0;
	s16 rssi_avg_db = 0;
	u8 i;
	struct cmn_sta_info *sta;

	/*[Step4]*/

	if (pktinfo->station_id >= ODM_ASSOCIATE_ENTRY_NUM)
		return;

	sta = dm->phydm_sta_info[pktinfo->station_id];

	if (!is_sta_active(sta))
		return;

	if (!pktinfo->is_packet_match_bssid) /*data frame only*/
		return;

	if (!(pktinfo->is_packet_to_self) && !(pktinfo->is_packet_beacon))
		return;

	if (pktinfo->is_packet_beacon) {
		dm->phy_dbg_info.num_qry_beacon_pkt++;
		dm->phy_dbg_info.beacon_phy_rate = pktinfo->data_rate;
	}

	rssi_pre = sta->rssi_stat.rssi;

	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (phy_info->rx_mimo_signal_strength[i] != 0)
			rssi_linear += odm_convert_to_linear(phy_info->rx_mimo_signal_strength[i]);
	}

	switch (phy_info->rx_count + 1) {
	case 2:
		rssi_linear = (rssi_linear >> 1); /*rssi_linear/2*/
		break;
	case 3:
		rssi_linear = ((rssi_linear) + (rssi_linear << 1) + (rssi_linear << 3)) >> 5; /* rssi_linear/3 ~ rssi_linear*11/32 */
		break;
	case 4:
		rssi_linear = (rssi_linear >> 2); /*rssi_linear/4*/
		break;
	}

	rssi_avg_db = (s16)odm_convert_to_db(rssi_linear);

	if (rssi_pre <= 0) {
		sta->rssi_stat.rssi_acc = (s16)(phy_info->rx_pwdb_all << RSSI_MA_FACTOR);
		sta->rssi_stat.rssi = (s8)(phy_info->rx_pwdb_all);
	} else {
		sta->rssi_stat.rssi_acc = sta->rssi_stat.rssi_acc - (sta->rssi_stat.rssi_acc >> RSSI_MA_FACTOR) + rssi_avg_db;
		sta->rssi_stat.rssi = (s8)((sta->rssi_stat.rssi_acc + (1 << (RSSI_MA_FACTOR - 1))) >> RSSI_MA_FACTOR);
	}

	if (pktinfo->is_cck_rate)
		sta->rssi_stat.rssi_cck = (s8)rssi_avg_db;
	else
		sta->rssi_stat.rssi_ofdm = (s8)rssi_avg_db;
}

void phydm_rx_physts_3rd_type(
	void *dm_void,
	u8 *phy_status_inf,
	struct phydm_perpkt_info_struct *pktinfo,
	struct phydm_phyinfo_struct *phy_info)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
#ifdef PHYDM_PHYSTAUS_SMP_MODE
	struct pkt_process_info *pkt_process = &dm->pkt_proc_struct;
#endif
	u8 phy_status_type = (*phy_status_inf & 0xf);

#ifdef PHYDM_PHYSTAUS_SMP_MODE
	if (pkt_process->phystatus_smp_mode_en && phy_status_type != 0) {
		if (pkt_process->pre_ppdu_cnt == pktinfo->ppdu_cnt)
			return;
		pkt_process->pre_ppdu_cnt = pktinfo->ppdu_cnt;
	}
#endif

	/*[Step 2]*/
	phydm_reset_phy_info_3rd(dm, phy_info); /* Memory reset */

	/* Phy status parsing */
	switch (phy_status_type) {
	case 0: /*CCK*/
		phydm_get_physts_jarguar3_0(dm, phy_status_inf, pktinfo, phy_info);
		break;
	case 1:
		phydm_get_physts_jarguar3_1(dm, phy_status_inf, pktinfo, phy_info);
		break;
	case 2:
	case 3:
		phydm_get_physts_jarguar3_2_3(dm, phy_status_inf, pktinfo, phy_info);
		break;
	case 4:
		phydm_get_physts_jarguar3_4(dm, phy_status_inf, pktinfo, phy_info);
		break;
	case 5:
		phydm_get_physts_jarguar3_5(dm, phy_status_inf, pktinfo, phy_info);
		break;
	default:
		break;
	}


	/*[Step 1]*/
	#if (RTL8198F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8198F)
		phydm_print_phystat_jaguar3(dm, phy_status_inf, pktinfo, phy_info, phy_status_type);
	#endif
}

#endif

#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
/* For 8822B only!! need to move to FW finally */
/*==============================================*/

boolean
phydm_query_is_mu_api(struct dm_struct *phydm, u8 ppdu_idx, u8 *p_data_rate,
		      u8 *p_gid)
{
	u8 data_rate = 0, gid = 0;
	boolean is_mu = false;

	data_rate = phydm->phy_dbg_info.num_of_ppdu[ppdu_idx];
	gid = phydm->phy_dbg_info.gid_num[ppdu_idx];

	if (data_rate & BIT(7)) {
		is_mu = true;
		data_rate = data_rate & ~(BIT(7));
	} else {
		is_mu = false;
	}

	*p_data_rate = data_rate;
	*p_gid = gid;

	return is_mu;
}

void phydm_reset_phy_info(struct dm_struct *phydm,
			  struct phydm_phyinfo_struct *phy_info)
{
	phy_info->rx_pwdb_all = 0;
	phy_info->signal_quality = 0;
	phy_info->band_width = 0;
	phy_info->rx_count = 0;
	odm_memory_set(phydm, phy_info->rx_mimo_signal_quality, 0, 4);
	odm_memory_set(phydm, phy_info->rx_mimo_signal_strength, 0, 4);
	odm_memory_set(phydm, phy_info->rx_snr, 0, 4);

	phy_info->rx_power = -110;
	phy_info->recv_signal_power = -110;
	phy_info->bt_rx_rssi_percentage = 0;
	phy_info->signal_strength = 0;
	phy_info->channel = 0;
	phy_info->is_mu_packet = 0;
	phy_info->is_beamformed = 0;
	phy_info->rxsc = 0;
	odm_memory_set(phydm, phy_info->rx_pwr, -110, 4);
	odm_memory_set(phydm, phy_info->cfo_short, 0, 8);
	odm_memory_set(phydm, phy_info->cfo_tail, 0, 8);

	odm_memory_set(phydm, phy_info->rx_mimo_evm_dbm, 0, 4);
}

void phydm_print_phy_status_jarguar2(struct dm_struct *dm, u8 *phy_status_inf,
				     struct phydm_perpkt_info_struct *pktinfo,
				     struct phydm_phyinfo_struct *phy_info,
				     u8 phy_status_page_num)
{
	struct phy_status_rpt_jaguar2_type0 *rpt0 = (struct phy_status_rpt_jaguar2_type0 *)phy_status_inf;
	struct phy_status_rpt_jaguar2_type1 *rpt = (struct phy_status_rpt_jaguar2_type1 *)phy_status_inf;
	struct phy_status_rpt_jaguar2_type2 *rpt2 = (struct phy_status_rpt_jaguar2_type2 *)phy_status_inf;
	struct odm_phy_dbg_info *dbg = &dm->phy_dbg_info;
	u32 phy_status[PHY_STATUS_JRGUAR2_DW_LEN] = {0};
	u8 i;

	odm_move_memory(dm, phy_status, phy_status_inf, (PHY_STATUS_JRGUAR2_DW_LEN << 2));

	if (!(dm->debug_components & DBG_PHY_STATUS))
		return;

	if (dbg->show_phy_sts_all_pkt == 0) {
		if (!pktinfo->is_packet_match_bssid)
			return;
	}

	dbg->show_phy_sts_cnt++;
	/*dbg_print("cnt=%d, max=%d\n", dbg->show_phy_sts_cnt, dbg->show_phy_sts_max_cnt);*/

	if (dbg->show_phy_sts_max_cnt != SHOW_PHY_STATUS_UNLIMITED) {
		if (dbg->show_phy_sts_cnt > dbg->show_phy_sts_max_cnt)
			return;
	}

	pr_debug("Phy Status Rpt: OFDM_%d\n", phy_status_page_num);
	pr_debug("StaID=%d, RxRate = 0x%x match_bssid=%d\n", pktinfo->station_id, pktinfo->data_rate, pktinfo->is_packet_match_bssid);

	for (i = 0; i < PHY_STATUS_JRGUAR2_DW_LEN; i++)
		pr_debug("Offset[%d:%d] = 0x%x\n", ((4 * i) + 3), (4 * i), phy_status[i]);

	if (phy_status_page_num == 0) {
		pr_debug("[0] TRSW=%d, MP_gain_idx=%d, pwdb=%d\n", rpt0->trsw, rpt0->gain, rpt0->pwdb);
		pr_debug("[4] band=%d, CH=%d, agc_table = %d, rxsc = %d\n", rpt0->band, rpt0->channel, rpt0->agc_table, rpt0->rxsc);
		pr_debug("[8] AntIdx[D:A]={%d, %d, %d, %d}, LSIG_len=%d\n",
			 rpt0->antidx_d, rpt0->antidx_c, rpt0->antidx_b, rpt0->antidx_a, rpt0->length);
		pr_debug("[12] lna_h=%d, bb_power=%d, lna_l=%d, vga=%d, sq=%d\n",
			 rpt0->lna_h, rpt0->bb_power, rpt0->lna_l, rpt0->vga, rpt0->signal_quality);

	} else if (phy_status_page_num == 1) {
		pr_debug("[0] pwdb[D:A]={%d, %d, %d, %d}\n",
			 rpt->pwdb[3], rpt->pwdb[2], rpt->pwdb[1], rpt->pwdb[0]);
		pr_debug("[4] BF: %d, ldpc=%d, stbc=%d, g_bt=%d, antsw=%d, band=%d, CH=%d, rxsc[ht, l]={%d, %d}\n",
			 rpt->beamformed, rpt->ldpc, rpt->stbc, rpt->gnt_bt,
			 rpt->hw_antsw_occu, rpt->band, rpt->channel, rpt->ht_rxsc, rpt->l_rxsc);
		pr_debug("[8] AntIdx[D:A]={%d, %d, %d, %d}, LSIG_len=%d\n",
			 rpt->antidx_d, rpt->antidx_c, rpt->antidx_b, rpt->antidx_a, rpt->lsig_length);
		pr_debug("[12] rf_mode=%d, NBI=%d, Intf_pos=%d, GID=%d, PAID=%d\n",
			 rpt->rf_mode, rpt->nb_intf_flag,
			 (rpt->intf_pos + (rpt->intf_pos_msb << 8)), rpt->gid,
			 (rpt->paid + (rpt->paid_msb << 8)));
		pr_debug("[16] EVM[D:A]={%d, %d, %d, %d}\n", rpt->rxevm[3], rpt->rxevm[2], rpt->rxevm[1], rpt->rxevm[0]);
		pr_debug("[20] CFO[D:A]={%d, %d, %d, %d}\n", rpt->cfo_tail[3], rpt->cfo_tail[2], rpt->cfo_tail[1], rpt->cfo_tail[0]);
		pr_debug("[24] SNR[D:A]={%d, %d, %d, %d}\n\n", rpt->rxsnr[3], rpt->rxsnr[2], rpt->rxsnr[1], rpt->rxsnr[0]);

	} else if (phy_status_page_num == 2) {
		pr_debug("[0] pwdb[D:A]={%d, %d, %d, %d}\n",
			 rpt2->pwdb[3], rpt2->pwdb[2], rpt2->pwdb[1], rpt2->pwdb[0]);
		pr_debug("[4] BF: %d, ldpc=%d, stbc=%d, g_bt=%d, antsw=%d, band=%d, CH=%d, rxsc[ht, l]={%d, %d}\n",
			 rpt2->beamformed, rpt2->ldpc, rpt2->stbc, rpt2->gnt_bt,
			 rpt2->hw_antsw_occu, rpt2->band, rpt2->channel, rpt2->ht_rxsc, rpt2->l_rxsc);
		pr_debug("[8] AgcTab[D:A]={%d, %d, %d, %d}, cnt_pw2cca=%d, shift_l_map=%d\n",
			 rpt2->agc_table_d, rpt2->agc_table_c, rpt2->agc_table_b, rpt2->agc_table_a,
			 rpt2->cnt_pw2cca, rpt2->shift_l_map);
		pr_debug("[12] (TRSW|Gain)[D:A]={%d %d, %d %d, %d %d, %d %d}, cnt_cca2agc_rdy=%d\n",
			 rpt2->trsw_d, rpt2->gain_d, rpt2->trsw_c, rpt2->gain_c,
			 rpt2->trsw_b, rpt2->gain_b, rpt2->trsw_a, rpt2->gain_a, rpt2->cnt_cca2agc_rdy);
		pr_debug("[16] AAGC step[D:A]={%d, %d, %d, %d} HT AAGC gain[D:A]={%d, %d, %d, %d}\n",
			 rpt2->aagc_step_d, rpt2->aagc_step_c, rpt2->aagc_step_b, rpt2->aagc_step_a,
			 rpt2->ht_aagc_gain[3], rpt2->ht_aagc_gain[2], rpt2->ht_aagc_gain[1], rpt2->ht_aagc_gain[0]);
		pr_debug("[20] DAGC gain[D:A]={%d, %d, %d, %d}\n", rpt2->dagc_gain[3],
			 rpt2->dagc_gain[2], rpt2->dagc_gain[1], rpt2->dagc_gain[0]);
		pr_debug("[24] syn_cnt: %d, Cnt=%d\n\n", rpt2->syn_count, rpt2->counter);
	}
}

void phydm_set_per_path_phy_info(u8 rx_path, s8 rx_pwr, s8 rx_evm, s8 cfo_tail,
				 s8 rx_snr,
				 struct phydm_phyinfo_struct *phy_info)
{
	u8 evm_dbm = 0;
	u8 evm_percentage = 0;

	/* SNR is S(8,1), EVM is S(8,1), CFO is S(8,7) */

	if (rx_evm < 0) {
		/* Calculate EVM in dBm */
		evm_dbm = ((u8)(0 - rx_evm) >> 1);

		if (evm_dbm == 64)
			evm_dbm = 0; /*if 1SS rate, evm_dbm [2nd stream] =64*/

		if (evm_dbm != 0) {
			/* Convert EVM to 0%~100% percentage */
			if (evm_dbm >= 34)
				evm_percentage = 100;
			else
				evm_percentage = (evm_dbm << 1) + (evm_dbm);
		}
	}

	phy_info->rx_pwr[rx_path] = rx_pwr;

	phy_info->cfo_tail[rx_path] = (cfo_tail * 5) >> 1; /* CFO(kHz) = CFO_tail * 312.5(kHz) / 2^7 ~= CFO tail * 5/2 (kHz)*/
	phy_info->rx_mimo_evm_dbm[rx_path] = evm_dbm;
	phy_info->rx_mimo_signal_strength[rx_path] = phydm_query_rx_pwr_percentage(rx_pwr);
	phy_info->rx_mimo_signal_quality[rx_path] = evm_percentage;
	phy_info->rx_snr[rx_path] = rx_snr >> 1;

#if 0
	/* if (pktinfo->is_packet_match_bssid) */
	{
		dbg_print("path (%d)--------\n", rx_path);
		dbg_print("rx_pwr = %d, Signal strength = %d\n", phy_info->rx_pwr[rx_path], phy_info->rx_mimo_signal_strength[rx_path]);
		dbg_print("evm_dbm = %d, Signal quality = %d\n", phy_info->rx_mimo_evm_dbm[rx_path], phy_info->rx_mimo_signal_quality[rx_path]);
		dbg_print("CFO = %d, SNR = %d\n", phy_info->cfo_tail[rx_path], phy_info->rx_snr[rx_path]);
	}
#endif
}

void phydm_set_common_phy_info(s8 rx_power, u8 channel, boolean is_beamformed,
			       boolean is_mu_packet, u8 bandwidth,
			       u8 signal_quality, u8 rxsc,
			       struct phydm_phyinfo_struct *phy_info)
{
	phy_info->rx_power = rx_power; /* RSSI in dB */
	phy_info->recv_signal_power = rx_power; /* RSSI in dB */
	phy_info->channel = channel; /* channel number */
	phy_info->is_beamformed = is_beamformed; /* apply BF */
	phy_info->is_mu_packet = is_mu_packet; /* MU packet */
	phy_info->rxsc = rxsc;

	phy_info->rx_pwdb_all = phydm_query_rx_pwr_percentage(rx_power); /* RSSI in percentage */
	phy_info->signal_quality = signal_quality; /* signal quality */
	phy_info->band_width = bandwidth; /* bandwidth */

#if 0
	/* if (pktinfo->is_packet_match_bssid) */
	{
		dbg_print("rx_pwdb_all = %d, rx_power = %d, recv_signal_power = %d\n", phy_info->rx_pwdb_all, phy_info->rx_power, phy_info->recv_signal_power);
		dbg_print("signal_quality = %d\n", phy_info->signal_quality);
		dbg_print("is_beamformed = %d, is_mu_packet = %d, rx_count = %d\n", phy_info->is_beamformed, phy_info->is_mu_packet, phy_info->rx_count + 1);
		dbg_print("channel = %d, rxsc = %d, band_width = %d\n", channel, rxsc, bandwidth);
	}
#endif
}

void phydm_get_rx_phy_status_type0(struct dm_struct *dm, u8 *phy_status_inf,
				   struct phydm_perpkt_info_struct *pktinfo,
				   struct phydm_phyinfo_struct *phy_info)
{
	/* type 0 is used for cck packet */
	struct phy_status_rpt_jaguar2_type0 *phy_sta_rpt = (struct phy_status_rpt_jaguar2_type0 *)phy_status_inf;
	u8 sq = 0;
	s8 rx_power = phy_sta_rpt->pwdb - 110;

	if (dm->support_ic_type & ODM_RTL8723D) {
#if (RTL8723D_SUPPORT == 1)
		rx_power = phy_sta_rpt->pwdb - 97;
#endif
	}
/*#if (RTL8710B_SUPPORT == 1)*/
/*if (dm->support_ic_type & ODM_RTL8710B)*/
/*rx_power = phy_sta_rpt->pwdb - 97;*/
/*#endif*/

#if (RTL8821C_SUPPORT == 1)
	else if (dm->support_ic_type & ODM_RTL8821C) {
		if (phy_sta_rpt->pwdb >= -57)
			rx_power = phy_sta_rpt->pwdb - 100;
		else
			rx_power = phy_sta_rpt->pwdb - 102;
	}
#endif

	if (pktinfo->is_to_self) {
		dm->ofdm_agc_idx[0] = phy_sta_rpt->pwdb;
		dm->ofdm_agc_idx[1] = 0;
		dm->ofdm_agc_idx[2] = 0;
		dm->ofdm_agc_idx[3] = 0;
	}

	/* Calculate Signal Quality*/
	if (phy_sta_rpt->signal_quality >= 64) {
		sq = 0;
	} else if (phy_sta_rpt->signal_quality <= 20) {
		sq = 100;
	} else {
		/* mapping to 2~99% */
		sq = 64 - phy_sta_rpt->signal_quality;
		sq = ((sq << 3) + sq) >> 2;
	}

	/* Modify CCK PWDB if old AGC */
	if (!dm->cck_new_agc) {
		#if (RTL8197F_SUPPORT == 1)
		if (dm->support_ic_type & ODM_RTL8197F)
			rx_power = phydm_cck_rssi_convert(dm, phy_sta_rpt->lna_l, phy_sta_rpt->vga);
		else
		#endif
		{
			u8 lna_idx, vga_idx;

			lna_idx = ((phy_sta_rpt->lna_h << 3) | phy_sta_rpt->lna_l);
			vga_idx = phy_sta_rpt->vga;

			#if (RTL8723D_SUPPORT == 1)
			if (dm->support_ic_type & ODM_RTL8723D)
				rx_power = odm_cckrssi_8723d(lna_idx, vga_idx);
			#endif

			#if (RTL8710B_SUPPORT == 1)
			if (dm->support_ic_type & ODM_RTL8710B)
				rx_power = odm_cckrssi_8710b(lna_idx, vga_idx);
			#endif
			#if (RTL8192F_SUPPORT == 1)
			if (dm->support_ic_type & ODM_RTL8192F)
				rx_power = odm_cckrssi_8192f(lna_idx, vga_idx);
			#endif

			#if (RTL8822B_SUPPORT == 1)
			/* Need to do !! */
			/*if (dm->support_ic_type & ODM_RTL8822B) */
			/*rx_power = odm_CCKRSSI_8822B(LNA_idx, VGA_idx);*/
			#endif

			#if (RTL8821C_SUPPORT == 1)
			if (dm->support_ic_type & ODM_RTL8821C)
				rx_power = phydm_cck_rssi_8821c(dm, lna_idx, vga_idx);
			#endif

			#if (RTL8195B_SUPPORT == 1)
			if (dm->support_ic_type & ODM_RTL8195B)
				rx_power = phydm_cck_rssi_8195B(dm, lna_idx, vga_idx);
			#endif
		}
	}

	/* Confirm CCK RSSI */
	#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F) {
		u8 bb_pwr_th_l = 5; /* round( 31*0.15 ) */
		u8 bb_pwr_th_h = 27; /* round( 31*0.85 ) */

		if (phy_sta_rpt->bb_power < bb_pwr_th_l || phy_sta_rpt->bb_power > bb_pwr_th_h)
			rx_power = 0; /* Error RSSI for CCK ; set 100*/
	}
	#endif

	/*CCK no STBC and LDPC*/
	dm->phy_dbg_info.is_ldpc_pkt = false;
	dm->phy_dbg_info.is_stbc_pkt = false;

	/* Update Common information */
	phydm_set_common_phy_info(rx_power, phy_sta_rpt->channel, false,
				  false, CHANNEL_WIDTH_20, sq, phy_sta_rpt->rxsc, phy_info);

	/* Update CCK pwdb */
	phydm_set_per_path_phy_info(RF_PATH_A, rx_power, 0, 0, 0, phy_info); /* Update per-path information */

	dm->dm_fat_table.antsel_rx_keep_0 = phy_sta_rpt->antidx_a;
	dm->dm_fat_table.antsel_rx_keep_1 = phy_sta_rpt->antidx_b;
	dm->dm_fat_table.antsel_rx_keep_2 = phy_sta_rpt->antidx_c;
	dm->dm_fat_table.antsel_rx_keep_3 = phy_sta_rpt->antidx_d;
}

void phydm_get_rx_phy_status_type1(struct dm_struct *dm, u8 *phy_status_inf,
				   struct phydm_perpkt_info_struct *pktinfo,
				   struct phydm_phyinfo_struct *phy_info)
{
	/* type 1 is used for ofdm packet */
	struct phy_status_rpt_jaguar2_type1 *phy_sta_rpt = (struct phy_status_rpt_jaguar2_type1 *)phy_status_inf;
	s8 rx_pwr_db = -120;
	s8 rx_path_pwr_db;
	u8 i, rxsc, bw = CHANNEL_WIDTH_20, rx_count = 0;
	boolean is_mu;

	/* Update per-path information */
	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (dm->rx_ant_status & BIT(i)) {
			rx_count++;

			if (rx_count > dm->num_rf_path)
				break;

			/* Update per-path information (RSSI_dB RSSI_percentage EVM SNR CFO sq) */
			/* EVM report is reported by stream, not path */
			rx_path_pwr_db = phy_sta_rpt->pwdb[i] - 110; /* per-path pwdb in dB domain */

			if (pktinfo->is_to_self)
				dm->ofdm_agc_idx[i] = phy_sta_rpt->pwdb[i];

			phydm_set_per_path_phy_info(i, rx_path_pwr_db, phy_sta_rpt->rxevm[rx_count - 1],
						    phy_sta_rpt->cfo_tail[i], phy_sta_rpt->rxsnr[i], phy_info);

			/* search maximum pwdb */
			if (rx_path_pwr_db > rx_pwr_db)
				rx_pwr_db = rx_path_pwr_db;
		}
	}

	/* mapping RX counter from 1~4 to 0~3 */
	if (rx_count > 0)
		phy_info->rx_count = rx_count - 1;

	/* Check if MU packet or not */
	if (phy_sta_rpt->gid != 0 && phy_sta_rpt->gid != 63) {
		is_mu = true;
		dm->phy_dbg_info.num_qry_mu_pkt++;
	} else {
		is_mu = false;
	}

	/* count BF packet */
	dm->phy_dbg_info.num_qry_bf_pkt = dm->phy_dbg_info.num_qry_bf_pkt + phy_sta_rpt->beamformed;

	/*STBC or LDPC pkt*/
	dm->phy_dbg_info.is_ldpc_pkt = phy_sta_rpt->ldpc;
	dm->phy_dbg_info.is_stbc_pkt = phy_sta_rpt->stbc;

	/* Check sub-channel */
	if (pktinfo->data_rate > ODM_RATE11M && pktinfo->data_rate < ODM_RATEMCS0)
		rxsc = phy_sta_rpt->l_rxsc;
	else
		rxsc = phy_sta_rpt->ht_rxsc;

	/* Check RX bandwidth */
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (rxsc >= 1 && rxsc <= 8)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc >= 9) && (rxsc <= 12))
			bw = CHANNEL_WIDTH_40;
		else if (rxsc >= 13)
			bw = CHANNEL_WIDTH_80;
		else
			bw = phy_sta_rpt->rf_mode;

	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (phy_sta_rpt->rf_mode == 0)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc == 1) || (rxsc == 2))
			bw = CHANNEL_WIDTH_20;
		else
			bw = CHANNEL_WIDTH_40;
	}

	/* Update packet information */
	phydm_set_common_phy_info(rx_pwr_db, phy_sta_rpt->channel, (boolean)phy_sta_rpt->beamformed,
				  is_mu, bw, phy_info->rx_mimo_signal_quality[0], rxsc, phy_info);

	phydm_parsing_cfo(dm, pktinfo, phy_sta_rpt->cfo_tail, pktinfo->rate_ss);
	#ifdef PHYDM_LNA_SAT_CHK_TYPE2
	phydm_parsing_snr(dm, pktinfo, phy_sta_rpt->rxsnr);
	#endif

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	dm->dm_fat_table.antsel_rx_keep_0 = phy_sta_rpt->antidx_a;
	dm->dm_fat_table.antsel_rx_keep_1 = phy_sta_rpt->antidx_b;
	dm->dm_fat_table.antsel_rx_keep_2 = phy_sta_rpt->antidx_c;
	dm->dm_fat_table.antsel_rx_keep_3 = phy_sta_rpt->antidx_d;
#endif
}

void phydm_get_rx_phy_status_type2(struct dm_struct *dm, u8 *phy_status_inf,
				   struct phydm_perpkt_info_struct *pktinfo,
				   struct phydm_phyinfo_struct *phy_info)
{
	struct phy_status_rpt_jaguar2_type2 *phy_sta_rpt = (struct phy_status_rpt_jaguar2_type2 *)phy_status_inf;
	s8 rx_pwr_db_max = -120;
	s8 rx_path_pwr_db;
	u8 i, rxsc, bw = CHANNEL_WIDTH_20, rx_count = 0;

	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (dm->rx_ant_status & BIT(i)) {
			rx_count++;

			if (rx_count > dm->num_rf_path)
				break;

/* Update per-path information (RSSI_dB RSSI_percentage EVM SNR CFO sq) */
#if (RTL8197F_SUPPORT == 1)
			if ((dm->support_ic_type & ODM_RTL8197F) && phy_sta_rpt->pwdb[i] == 0x7f) { /*for 97f workaround*/

				if (i == RF_PATH_A) {
					rx_path_pwr_db = (phy_sta_rpt->gain_a) << 1;
					rx_path_pwr_db = rx_path_pwr_db - 110;
				} else if (i == RF_PATH_B) {
					rx_path_pwr_db = (phy_sta_rpt->gain_b) << 1;
					rx_path_pwr_db = rx_path_pwr_db - 110;
				} else
					rx_path_pwr_db = 0;
			} else
#endif
				rx_path_pwr_db = phy_sta_rpt->pwdb[i] - 110; /*unit: (dBm)*/

			phydm_set_per_path_phy_info(i, rx_path_pwr_db, 0, 0, 0, phy_info);

			if (rx_path_pwr_db > rx_pwr_db_max /* search maximum pwdb */)
				rx_pwr_db_max = rx_path_pwr_db;
		}
	}

	/* mapping RX counter from 1~4 to 0~3 */
	if (rx_count > 0)
		phy_info->rx_count = rx_count - 1;

	/* Check RX sub-channel */
	if (pktinfo->data_rate > ODM_RATE11M && pktinfo->data_rate < ODM_RATEMCS0)
		rxsc = phy_sta_rpt->l_rxsc;
	else
		rxsc = phy_sta_rpt->ht_rxsc;

	/*STBC or LDPC pkt*/
	dm->phy_dbg_info.is_ldpc_pkt = phy_sta_rpt->ldpc;
	dm->phy_dbg_info.is_stbc_pkt = phy_sta_rpt->stbc;

	/* Check RX bandwidth */
	/* the BW information of sc=0 is useless, because there is no information of RF mode*/
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (rxsc >= 1 && rxsc <= 8)
			bw = CHANNEL_WIDTH_20;
		else if ((rxsc >= 9) && (rxsc <= 12))
			bw = CHANNEL_WIDTH_40;
		else if (rxsc >= 13)
			bw = CHANNEL_WIDTH_80;

	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (rxsc == 3)
			bw = CHANNEL_WIDTH_40;
		else if ((rxsc == 1) || (rxsc == 2))
			bw = CHANNEL_WIDTH_20;
	}

	/* Update packet information */
	phydm_set_common_phy_info(rx_pwr_db_max, phy_sta_rpt->channel, (boolean)phy_sta_rpt->beamformed,
				  false, bw, 0, rxsc, phy_info);
}

void phydm_process_rssi_for_dm_new_type(struct dm_struct *dm,
					struct phydm_phyinfo_struct *phy_info,
					struct phydm_perpkt_info_struct *pktinfo
					)
{
	s32 rssi_pre;
	u32 rssi_linear = 0;
	s16 rssi_avg_db = 0;
	u8 i;
	struct cmn_sta_info *sta = NULL;

	if (pktinfo->station_id >= ODM_ASSOCIATE_ENTRY_NUM)
		return;

	sta = dm->phydm_sta_info[pktinfo->station_id];

	if (!is_sta_active(sta))
		return;

	if (!pktinfo->is_packet_match_bssid) /*data frame only*/
		return;

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	if (dm->support_ability & ODM_BB_ANT_DIV)
		odm_process_rssi_for_ant_div(dm, phy_info, pktinfo);
#endif

#ifdef CONFIG_ADAPTIVE_SOML
	phydm_rx_qam_for_soml(dm, pktinfo);
	phydm_rx_rate_for_soml(dm, pktinfo);
#endif

	if (!(pktinfo->is_packet_to_self) && !(pktinfo->is_packet_beacon))
		return;

	if (pktinfo->is_packet_beacon) {
		dm->phy_dbg_info.num_qry_beacon_pkt++;
		dm->phy_dbg_info.beacon_phy_rate = pktinfo->data_rate;
	}

	rssi_pre = sta->rssi_stat.rssi;

	for (i = RF_PATH_A; i < PHYDM_MAX_RF_PATH; i++) {
		if (phy_info->rx_mimo_signal_strength[i] != 0)
			rssi_linear += odm_convert_to_linear(phy_info->rx_mimo_signal_strength[i]);
	}

	switch (phy_info->rx_count + 1) {
	case 2:
		rssi_linear = (rssi_linear >> 1);
		break;
	case 3:
		rssi_linear = ((rssi_linear) + (rssi_linear << 1) + (rssi_linear << 3)) >> 5; /* rssi_linear/3 ~ rssi_linear*11/32 */
		break;
	case 4:
		rssi_linear = (rssi_linear >> 2);
		break;
	}

	rssi_avg_db = (s16)odm_convert_to_db(rssi_linear);

	if (rssi_pre <= 0) {
		sta->rssi_stat.rssi_acc = (s16)(phy_info->rx_pwdb_all << RSSI_MA_FACTOR);
		sta->rssi_stat.rssi = (s8)(phy_info->rx_pwdb_all);
	} else {
		sta->rssi_stat.rssi_acc = sta->rssi_stat.rssi_acc - (sta->rssi_stat.rssi_acc >> RSSI_MA_FACTOR) + rssi_avg_db;
		sta->rssi_stat.rssi = (s8)((sta->rssi_stat.rssi_acc + (1 << (RSSI_MA_FACTOR - 1))) >> RSSI_MA_FACTOR);
	}

	#if 0
	if (pktinfo->is_packet_match_bssid) {
		PHYDM_DBG(dm, DBG_TMP, "RSSI[%d]{A,B,Avg}=%d, %d, %d\n",
			  pktinfo->station_id,
			  phy_info->rx_mimo_signal_strength[0],
			  phy_info->rx_mimo_signal_strength[1], rssi_ave);
		PHYDM_DBG(dm, DBG_TMP, "{new, old}=%d, %d\n",
			  sta->rssi_stat.rssi, rssi_pre);
	}
	#endif

	if (pktinfo->is_cck_rate)
		sta->rssi_stat.rssi_cck = (s8)rssi_avg_db;
	else
		sta->rssi_stat.rssi_ofdm = (s8)rssi_avg_db;
}

void phydm_rx_phy_status_new_type(void *dm_void, u8 *phy_status_inf,
				  struct phydm_perpkt_info_struct *pktinfo,
				  struct phydm_phyinfo_struct *phy_info)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
#ifdef PHYDM_PHYSTAUS_SMP_MODE
	struct pkt_process_info *pkt_process = &dm->pkt_proc_struct;
#endif
	u8 phy_status_type = (*phy_status_inf & 0xf);

#ifdef PHYDM_PHYSTAUS_SMP_MODE
	if (pkt_process->phystatus_smp_mode_en && phy_status_type != 0) {
		if (pkt_process->pre_ppdu_cnt == pktinfo->ppdu_cnt)
			return;

		pkt_process->pre_ppdu_cnt = pktinfo->ppdu_cnt;
	}
#endif

	phydm_reset_phy_info(dm, phy_info); /* Memory reset */

	/* Phy status parsing */
	switch (phy_status_type) {
	case 0: /*CCK*/
		phydm_get_rx_phy_status_type0(dm, phy_status_inf, pktinfo, phy_info);
		break;
	case 1:
		phydm_get_rx_phy_status_type1(dm, phy_status_inf, pktinfo, phy_info);
		break;
	case 2:
		phydm_get_rx_phy_status_type2(dm, phy_status_inf, pktinfo, phy_info);
		break;
	default:
		break;
	}

#if (RTL8822B_SUPPORT || RTL8821C_SUPPORT || RTL8195B_SUPPORT)
	if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C | ODM_RTL8195B))
		phydm_print_phy_status_jarguar2(dm, phy_status_inf, pktinfo, phy_info, phy_status_type);
#endif
}
/*==============================================*/
#endif

void odm_phy_status_query(struct dm_struct *dm,
			  struct phydm_phyinfo_struct *phy_info,
			  u8 *phy_status_inf,
			  struct phydm_perpkt_info_struct *pktinfo)
{
	pktinfo->is_cck_rate = (pktinfo->data_rate <= ODM_RATE11M) ? true : false;
	pktinfo->rate_ss = phydm_rate_to_num_ss(dm, pktinfo->data_rate);
	dm->rate_ss = pktinfo->rate_ss; /*For AP EVM SW antenna diversity use*/

	if (pktinfo->is_cck_rate)
		dm->phy_dbg_info.num_qry_phy_status_cck++;
	else
		dm->phy_dbg_info.num_qry_phy_status_ofdm++;

	/*Reset phy_info*/
	odm_memory_set(dm, phy_info->rx_mimo_signal_strength, 0, 4);
	odm_memory_set(dm, phy_info->rx_mimo_signal_quality, 0, 4);
	if (dm->support_ic_type & PHYSTS_3RD_TYPE_IC) {
		#ifdef PHYSTS_3RD_TYPE_SUPPORT
		phydm_rx_physts_3rd_type(dm, phy_status_inf, pktinfo, phy_info);
		phydm_process_rssi_for_dm_3rd_type(dm, phy_info, pktinfo);
		#endif
	} else if (dm->support_ic_type & PHYSTS_2ND_TYPE_IC) {
		#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
		phydm_rx_phy_status_new_type(dm, phy_status_inf, pktinfo, phy_info);
		phydm_process_rssi_for_dm_new_type(dm, phy_info, pktinfo);
		#endif
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		#if ODM_IC_11AC_SERIES_SUPPORT
		phydm_rx_phy_status_jaguar_series_parsing(dm, phy_info, phy_status_inf, pktinfo);
		phydm_process_rssi_for_dm(dm, phy_info, pktinfo);
		#endif
	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		#if ODM_IC_11N_SERIES_SUPPORT
		phydm_rx_phy_status92c_series_parsing(dm, phy_info, phy_status_inf, pktinfo);
		phydm_process_rssi_for_dm(dm, phy_info, pktinfo);
		#endif
	}

	#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
	phydm_process_signal_strength(dm, phy_info, pktinfo);
	#endif

	if (pktinfo->is_packet_match_bssid) {
		dm->rx_rate = pktinfo->data_rate;
		dm->rssi_a = phy_info->rx_mimo_signal_strength[RF_PATH_A];
		dm->rssi_b = phy_info->rx_mimo_signal_strength[RF_PATH_B];
		dm->rssi_c = phy_info->rx_mimo_signal_strength[RF_PATH_C];
		dm->rssi_d = phy_info->rx_mimo_signal_strength[RF_PATH_D];

		phydm_avg_phystatus_index(dm, phy_info, pktinfo);
		phydm_rx_statistic_cal(dm, phy_info, phy_status_inf, pktinfo);
	}
}

void phydm_rx_phy_status_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct odm_phy_dbg_info *dbg = &dm->phy_dbg_info;
#ifdef PHYDM_PHYSTAUS_SMP_MODE
	struct pkt_process_info *pkt_process = &dm->pkt_proc_struct;

	if (dm->support_ic_type == ODM_RTL8822B) {
		pkt_process->phystatus_smp_mode_en = 1;
		pkt_process->pre_ppdu_cnt = 0xff;

		odm_set_mac_reg(dm, R_0x60f, BIT(7), 1); /*phystatus sampling mode enable*/

		odm_set_bb_reg(dm, R_0x9e4, 0x3ff, 0x0); /*First update timming*/
		odm_set_bb_reg(dm, R_0x9e4, 0xfc00, 0x0); /*Update Sampling time*/
	} else if (dm->support_ic_type == ODM_RTL8198F) {
		odm_set_bb_reg(dm, R_0x8c0, 0x3ff0, 0x0); /*First update timming*/
		odm_set_bb_reg(dm, R_0x8c0, 0xfc000, 0x0); /*Update Sampling time*/
	}
#endif

	dbg->show_phy_sts_all_pkt = 0;
	dbg->show_phy_sts_max_cnt = 1;
	dbg->show_phy_sts_cnt = 0;
}
