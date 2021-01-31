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
#ifdef PHYDM_PRIMARY_CCA

void phydm_write_dynamic_cca(
	void *dm_void,
	u8 curr_mf_state

	)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_pricca_struct *primary_cca = &dm->dm_pri_cca;

	if (primary_cca->mf_state == curr_mf_state)
		return;

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (curr_mf_state == MF_USC_LSC) {
			odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), MF_USC_LSC);
			odm_set_bb_reg(dm, R_0xc84, 0xf0000000, primary_cca->cca_th_40m_bkp); /*40M OFDM MF CCA threshold*/
		} else {
			odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), curr_mf_state);
			odm_set_bb_reg(dm, R_0xc84, 0xf0000000, 0); /*40M OFDM MF CCA threshold*/
		}
	}

	primary_cca->mf_state = curr_mf_state;
	PHYDM_DBG(dm, DBG_PRI_CCA, "Set CCA at ((%s SB)), 0xc6c[8:7]=((%d))\n",
		  ((curr_mf_state == MF_USC_LSC) ? "D" :
		  ((curr_mf_state == MF_LSC) ? "L" : "U")), curr_mf_state);
}

void phydm_primary_cca_reset(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_pricca_struct *primary_cca = &dm->dm_pri_cca;

	PHYDM_DBG(dm, DBG_PRI_CCA, "[PriCCA] Reset\n");
	primary_cca->mf_state = 0xff;
	primary_cca->pre_bw = (enum channel_width)0xff;
	phydm_write_dynamic_cca(dm, MF_USC_LSC);
}

void phydm_primary_cca_11n(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_pricca_struct *primary_cca = &dm->dm_pri_cca;
	enum channel_width curr_bw = (enum channel_width) *dm->band_width;

	if (!(dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	if (!dm->is_linked) { /* is_linked==False */
		PHYDM_DBG(dm, DBG_PRI_CCA, "[PriCCA][No Link!!!]\n");

		if (primary_cca->pri_cca_is_become_linked == true) {
			phydm_primary_cca_reset(dm);
			primary_cca->pri_cca_is_become_linked = dm->is_linked;
		}
		return;

	} else {
		if (primary_cca->pri_cca_is_become_linked == false) {
			PHYDM_DBG(dm, DBG_PRI_CCA, "[PriCCA][Linked !!!]\n");
			primary_cca->pri_cca_is_become_linked = dm->is_linked;
		}
	}

	if (curr_bw != primary_cca->pre_bw) {
		PHYDM_DBG(dm, DBG_PRI_CCA, "[Primary CCA] start ==>\n");
		primary_cca->pre_bw = curr_bw;

		if (curr_bw == CHANNEL_WIDTH_40) {
			if (*dm->sec_ch_offset == SECOND_CH_AT_LSB) { /* Primary CH @ upper sideband*/

				PHYDM_DBG(dm, DBG_PRI_CCA,
					  "BW40M, Primary CH at USB\n");
				phydm_write_dynamic_cca(dm, MF_USC);

			} else { /*Primary CH @ lower sideband*/

				PHYDM_DBG(dm, DBG_PRI_CCA,
					  "BW40M, Primary CH at LSB\n");
				phydm_write_dynamic_cca(dm, MF_LSC);
			}
		} else {
			PHYDM_DBG(dm, DBG_PRI_CCA, "Not BW40M, USB + LSB\n");
			phydm_primary_cca_reset(dm);
		}
	}
}

#if 0
#if (RTL8188E_SUPPORT == 1)
void
odm_dynamic_primary_cca_8188e(
	void			*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct sta_info	*entry;
	struct cmn_sta_info	*sta;
	struct phydm_fa_struct		*false_alm_cnt = (struct phydm_fa_struct *)phydm_get_structure(dm, PHYDM_FALSEALMCNT);
	struct phydm_pricca_struct		*primary_cca = &(dm->dm_pri_cca);
	boolean		client_40mhz = false, client_tmp = false;      /* connected client BW */
	boolean		is_connected = false;		/* connected or not */
	u8	client_40mhz_pre = 0;
	u32	counter = 0;
	u8	delay = 1;
	u64		cur_tx_ok_cnt;
	u64		cur_rx_ok_cnt;
	u8		sec_ch_offset = *(dm->sec_ch_offset);
	u8		i;

	if (!dm->is_linked)
		return;

	if (!(dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	if (*dm->band_width == CHANNEL_WIDTH_20) {	/*curr bw*/
		odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 0);
		return;
	}

	#if (DM_ODM_SUPPORT_TYPE == ODM_WIN) || (DM_ODM_SUPPORT_TYPE == ODM_CE)
	sec_ch_offset = sec_ch_offset % 2 + 1; /* NIC's definition is reverse to AP   1:secondary below,  2: secondary above */
	#endif

	PHYDM_DBG(dm, DBG_PRI_CCA, "Second CH Offset = %d\n", sec_ch_offset);

	/* 3 Check Current WLAN Traffic */
	cur_tx_ok_cnt = dm->tx_tp;
	cur_rx_ok_cnt = dm->rx_tp;

	/* ==================Debug Message==================== */
	PHYDM_DBG(dm, DBG_PRI_CCA, "TP = %llu\n",
		  cur_tx_ok_cnt + cur_rx_ok_cnt);
	PHYDM_DBG(dm, DBG_PRI_CCA, "is_BW40 = %d\n", *(dm->band_width));
	PHYDM_DBG(dm, DBG_PRI_CCA, "BW_LSC = %d\n", false_alm_cnt->cnt_bw_lsc);
	PHYDM_DBG(dm, DBG_PRI_CCA, "BW_USC = %d\n", false_alm_cnt->cnt_bw_usc);
	PHYDM_DBG(dm, DBG_PRI_CCA, "CCA OFDM = %d\n",
		  false_alm_cnt->cnt_ofdm_cca);
	PHYDM_DBG(dm, DBG_PRI_CCA, "CCA CCK = %d\n",
		  false_alm_cnt->cnt_cck_cca);
	PHYDM_DBG(dm, DBG_PRI_CCA, "OFDM FA = %d\n",
		  false_alm_cnt->cnt_ofdm_fail);
	PHYDM_DBG(dm, DBG_PRI_CCA, "CCK FA = %d\n",
		  false_alm_cnt->cnt_cck_fail);
	/* ================================================ */

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	if (ACTING_AS_AP(dm->adapter))   /* primary cca process only do at AP mode */
#endif
	{
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		PHYDM_DBG(dm, DBG_PRI_CCA, "ACTING as AP mode=%d\n",
			  ACTING_AS_AP(dm->adapter));
		/* 3 To get entry's connection and BW infomation status. */
		for (i = 0; i < ASSOCIATE_ENTRY_NUM; i++) {
			if (IsAPModeExist(dm->adapter) && GetFirstExtAdapter(dm->adapter) != NULL)
				entry = AsocEntry_EnumStation(GetFirstExtAdapter(dm->adapter), i);
			else
				entry = AsocEntry_EnumStation(GetDefaultAdapter(dm->adapter), i);
			if (entry != NULL) {
				client_tmp = entry->BandWidth;   /* client BW */
				PHYDM_DBG(dm, DBG_PRI_CCA, "Client_BW=%d\n",
					  client_tmp);
				if (client_tmp > client_40mhz)
					client_40mhz = client_tmp;     /* 40M/20M coexist => 40M priority is High */

				if (entry->bAssociated) {
					is_connected = true;  /* client is connected or not */
					break;
				}
			} else
				break;
		}
#elif (DM_ODM_SUPPORT_TYPE == ODM_AP)
		/* 3 To get entry's connection and BW infomation status. */

		for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
			sta = dm->phydm_sta_info[i];
			if (is_sta_active(sta)) {
				client_tmp = sta->bw_mode;
				if (client_tmp > client_40mhz)
					client_40mhz = client_tmp;     /* 40M/20M coexist => 40M priority is High */

				is_connected = true;
			}
		}
#endif
		PHYDM_DBG(dm, DBG_PRI_CCA, "is_connected=%d\n", is_connected);
		PHYDM_DBG(dm, DBG_PRI_CCA, "Is Client 40MHz=%d\n",
			  client_40mhz);
		/* 1 Monitor whether the interference exists or not */
		if (primary_cca->monitor_flag == 1) {
			if (sec_ch_offset == 1) {    /* secondary channel is below the primary channel */
				if (false_alm_cnt->cnt_ofdm_cca > 500 && (false_alm_cnt->cnt_bw_lsc > false_alm_cnt->cnt_bw_usc + 500)) {
					if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1) {
						primary_cca->intf_type = 1;
						primary_cca->pri_cca_flag = 1;
						odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT7, 2); /* USC MF */
						if (primary_cca->dup_rts_flag == 1)
							primary_cca->dup_rts_flag = 0;
					} else {
						primary_cca->intf_type = 2;
						if (primary_cca->dup_rts_flag == 0)
							primary_cca->dup_rts_flag = 1;
					}

				} else { /* interferecne disappear */
					primary_cca->dup_rts_flag = 0;
					primary_cca->intf_flag = 0;
					primary_cca->intf_type = 0;
				}
			} else if (sec_ch_offset == 2) { /* secondary channel is above the primary channel */
				if (false_alm_cnt->cnt_ofdm_cca > 500 && (false_alm_cnt->cnt_bw_usc > false_alm_cnt->cnt_bw_lsc + 500)) {
					if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1) {
						primary_cca->intf_type = 1;
						primary_cca->pri_cca_flag = 1;
						odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT7, 1); /* LSC MF */
						if (primary_cca->dup_rts_flag == 1)
							primary_cca->dup_rts_flag = 0;
					} else {
						primary_cca->intf_type = 2;
						if (primary_cca->dup_rts_flag == 0)
							primary_cca->dup_rts_flag = 1;
					}

				} else { /* interferecne disappear */
					primary_cca->dup_rts_flag = 0;
					primary_cca->intf_flag = 0;
					primary_cca->intf_type = 0;
				}

			}
			primary_cca->monitor_flag = 0;
		}

		/* 1 Dynamic Primary CCA Main Function */
		if (primary_cca->monitor_flag == 0) {
			if (*dm->band_width == CHANNEL_WIDTH_40) {		/* if RFBW==40M mode which require to process primary cca */
				/* 2 STA is NOT Connected */
				if (!is_connected) {
					PHYDM_DBG(dm, DBG_PRI_CCA, "STA NOT Connected!!!!\n");

					if (primary_cca->pri_cca_flag == 1) {	/* reset primary cca when STA is disconnected */
						primary_cca->pri_cca_flag = 0;
						odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 0);
					}
					if (primary_cca->dup_rts_flag == 1)		/* reset Duplicate RTS when STA is disconnected */
						primary_cca->dup_rts_flag = 0;

					if (sec_ch_offset == 1) { /* secondary channel is below the primary channel */
						if (false_alm_cnt->cnt_ofdm_cca > 800 && (false_alm_cnt->cnt_bw_lsc * 5 > false_alm_cnt->cnt_bw_usc * 9)) {
							primary_cca->intf_flag = 1;    /* secondary channel interference is detected!!! */
							if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1)
								primary_cca->intf_type = 1;   	/* interference is shift */
							else
								primary_cca->intf_type = 2;   	/* interference is in-band */
						} else {
							primary_cca->intf_flag = 0;
							primary_cca->intf_type = 0;
						}
					} else if (sec_ch_offset == 2) { /* secondary channel is above the primary channel */
						if (false_alm_cnt->cnt_ofdm_cca > 800 && (false_alm_cnt->cnt_bw_usc * 5 > false_alm_cnt->cnt_bw_lsc * 9)) {
							primary_cca->intf_flag = 1;    /* secondary channel interference is detected!!! */
							if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1)
								primary_cca->intf_type = 1;   	/* interference is shift */
							else
								primary_cca->intf_type = 2;   	/* interference is in-band */
						} else {
							primary_cca->intf_flag = 0;
							primary_cca->intf_type = 0;
						}
					}
					PHYDM_DBG(dm, DBG_PRI_CCA, "primary_cca=%d\n", primary_cca->pri_cca_flag);
					PHYDM_DBG(dm, DBG_PRI_CCA, "Intf_Type=%d\n", primary_cca->intf_type);
				}
				/* 2 STA is Connected */
				else {
					if (client_40mhz == 0)		/* 3 */ { /* client BW = 20MHz */
						if (primary_cca->pri_cca_flag == 0) {
							primary_cca->pri_cca_flag = 1;
							if (sec_ch_offset == 1)
								odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 2);
							else if (sec_ch_offset == 2)
								odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 1);
						}
						PHYDM_DBG(dm, DBG_PRI_CCA, "STA Connected 20M!!! primary_cca=%d\n", primary_cca->pri_cca_flag);
					} else		/* 3 */ { /* client BW = 40MHz */
						if (primary_cca->intf_flag == 1) { /* interference is detected!! */
							if (primary_cca->intf_type == 1) {
								if (primary_cca->pri_cca_flag != 1) {
									primary_cca->pri_cca_flag = 1;
									if (sec_ch_offset == 1)
										odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 2);
									else if (sec_ch_offset == 2)
										odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 1);
								}
							} else if (primary_cca->intf_type == 2) {
								if (primary_cca->dup_rts_flag != 1)
									primary_cca->dup_rts_flag = 1;
							}
						} else { /* if intf_flag==0 */
							if ((cur_tx_ok_cnt + cur_rx_ok_cnt) < 1) { /* idle mode or TP traffic is very low */
								if (sec_ch_offset == 1) {
									if (false_alm_cnt->cnt_ofdm_cca > 800 && (false_alm_cnt->cnt_bw_lsc * 5 > false_alm_cnt->cnt_bw_usc * 9)) {
										primary_cca->intf_flag = 1;
										if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1)
											primary_cca->intf_type = 1;   	/* interference is shift */
										else
											primary_cca->intf_type = 2;   	/* interference is in-band */
									}
								} else if (sec_ch_offset == 2) {
									if (false_alm_cnt->cnt_ofdm_cca > 800 && (false_alm_cnt->cnt_bw_usc * 5 > false_alm_cnt->cnt_bw_lsc * 9)) {
										primary_cca->intf_flag = 1;
										if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1)
											primary_cca->intf_type = 1;   	/* interference is shift */
										else
											primary_cca->intf_type = 2;   	/* interference is in-band */
									}
								}
							} else { /* TP Traffic is High */
								if (sec_ch_offset == 1) {
									if (false_alm_cnt->cnt_bw_lsc > (false_alm_cnt->cnt_bw_usc + 500)) {
										if (delay == 0) { /* add delay to avoid interference occurring abruptly, jump one time */
											primary_cca->intf_flag = 1;
											if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1)
												primary_cca->intf_type = 1;   	/* interference is shift */
											else
												primary_cca->intf_type = 2;   	/* interference is in-band */
											delay = 1;
										} else
											delay = 0;
									}
								} else if (sec_ch_offset == 2) {
									if (false_alm_cnt->cnt_bw_usc > (false_alm_cnt->cnt_bw_lsc + 500)) {
										if (delay == 0) { /* add delay to avoid interference occurring abruptly */
											primary_cca->intf_flag = 1;
											if (false_alm_cnt->cnt_ofdm_fail > false_alm_cnt->cnt_ofdm_cca >> 1)
												primary_cca->intf_type = 1;   	/* interference is shift */
											else
												primary_cca->intf_type = 2;   	/* interference is in-band */
											delay = 1;
										} else
											delay = 0;
									}
								}
							}
						}
						PHYDM_DBG(dm, DBG_PRI_CCA, "Primary CCA=%d\n", primary_cca->pri_cca_flag);
						PHYDM_DBG(dm, DBG_PRI_CCA, "Duplicate RTS=%d\n", primary_cca->dup_rts_flag);
					}

				} /* end of connected */
			}
		}
		/* 1 Dynamic Primary CCA Monitor counter */
		if (primary_cca->pri_cca_flag == 1 || primary_cca->dup_rts_flag == 1) {
			if (client_40mhz == 0) {  /* client=20M no need to monitor primary cca flag */
				client_40mhz_pre = client_40mhz;
				return;
			}
			counter++;
			PHYDM_DBG(dm, DBG_PRI_CCA, "counter=%d\n", counter);
			if (counter == 30 || ((client_40mhz - client_40mhz_pre) == 1)) { /* Every 60 sec to monitor one time */
				primary_cca->monitor_flag = 1;     /* monitor flag is triggered!!!!! */
				if (primary_cca->pri_cca_flag == 1) {
					primary_cca->pri_cca_flag = 0;
					odm_set_bb_reg(dm, R_0xc6c, BIT(8) | BIT(7), 0);
				}
				counter = 0;
			}
		}
	}

	client_40mhz_pre = client_40mhz;
}
#endif

#if (RTL8192E_SUPPORT == 1)

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)

void
odm_dynamic_primary_cca_mp_8192e(
	void			*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	void	*adapter = dm->adapter;
	HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter);
	struct phydm_fa_struct		*false_alm_cnt = &(dm->false_alm_cnt);
	struct phydm_pricca_struct		*primary_cca = &(dm->dm_pri_cca);
	u64			OFDM_CCA, OFDM_FA, bw_usc_cnt, bw_lsc_cnt;
	u8			sec_ch_offset;
	static u8		count_down = PRI_CCA_MONITOR_TIME;

	if (!dm->is_linked)
		return;

	if (!(dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	OFDM_CCA = false_alm_cnt->cnt_ofdm_cca;
	OFDM_FA = false_alm_cnt->cnt_ofdm_fail;
	bw_usc_cnt = false_alm_cnt->cnt_bw_usc;
	bw_lsc_cnt = false_alm_cnt->cnt_bw_lsc;
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: OFDM CCA=%d\n", OFDM_CCA);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: OFDM FA=%d\n", OFDM_FA);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: BW_USC=%d\n", bw_usc_cnt);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: BW_LSC=%d\n", bw_lsc_cnt);
	sec_ch_offset = *(dm->sec_ch_offset);		/* NIC: 2: sec is below,  1: sec is above */


	if (IsAPModeExist(adapter)) {
		phydm_write_dynamic_cca(dm, MF_USC_LSC);
		return;
	}

	if (*(dm->band_width) != CHANNEL_WIDTH_40)
		return;

	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: Cont Down= %d\n", count_down);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: Primary_CCA_flag=%d\n",
		  primary_cca->pri_cca_flag);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: Intf_Type=%d\n",
		  primary_cca->intf_type);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: Intf_flag=%d\n",
		  primary_cca->intf_flag);
	PHYDM_DBG(dm, DBG_PRI_CCA, "92E: Duplicate RTS Flag=%d\n",
		  primary_cca->dup_rts_flag);

	if (primary_cca->pri_cca_flag == 0) {
		if (sec_ch_offset == SECOND_CH_AT_LSB) {  /* Primary channel is above   NOTE: duplicate CTS can remove this condition */

			if (OFDM_CCA > OFDMCCA_TH && (bw_lsc_cnt > (bw_usc_cnt + bw_ind_bias))
			    && (OFDM_FA > (OFDM_CCA >> 1))) {
				primary_cca->intf_type = 1;
				primary_cca->intf_flag = 1;
				phydm_write_dynamic_cca(dm, MF_USC);
				primary_cca->pri_cca_flag = 1;
			} else if ((OFDM_CCA > OFDMCCA_TH) && (bw_lsc_cnt > (bw_usc_cnt + bw_ind_bias))
				&& (OFDM_FA < (OFDM_CCA >> 1))) {
				primary_cca->intf_type = 2;
				primary_cca->intf_flag = 1;
				phydm_write_dynamic_cca(dm, MF_USC);
				primary_cca->pri_cca_flag = 1;
				primary_cca->dup_rts_flag = 1;
				hal_data->RTSEN = 1;
			} else {
				primary_cca->intf_type = 0;
				primary_cca->intf_flag = 0;
				phydm_write_dynamic_cca(dm, MF_USC_LSC);
				hal_data->RTSEN = 0;
				primary_cca->dup_rts_flag = 0;
			}

		} else if (sec_ch_offset == SECOND_CH_AT_USB) {
			if (OFDM_CCA > OFDMCCA_TH && (bw_usc_cnt > (bw_lsc_cnt + bw_ind_bias))
			    && (OFDM_FA > (OFDM_CCA >> 1))) {
				primary_cca->intf_type = 1;
				primary_cca->intf_flag = 1;
				phydm_write_dynamic_cca(dm, MF_LSC);
				primary_cca->pri_cca_flag = 1;
			} else if ((OFDM_CCA > OFDMCCA_TH) && (bw_usc_cnt > (bw_lsc_cnt + bw_ind_bias))
				&& (OFDM_FA < (OFDM_CCA >> 1))) {
				primary_cca->intf_type = 2;
				primary_cca->intf_flag = 1;
				phydm_write_dynamic_cca(dm, MF_LSC);
				primary_cca->pri_cca_flag = 1;
				primary_cca->dup_rts_flag = 1;
				hal_data->RTSEN = 1;
			} else {
				primary_cca->intf_type = 0;
				primary_cca->intf_flag = 0;
				phydm_write_dynamic_cca(dm, MF_USC_LSC);
				hal_data->RTSEN = 0;
				primary_cca->dup_rts_flag = 0;
			}
		}

	} else {	/* primary_cca->pri_cca_flag==1 */

		count_down--;
		if (count_down == 0) {
			count_down = PRI_CCA_MONITOR_TIME;
			primary_cca->pri_cca_flag = 0;
			phydm_write_dynamic_cca(dm, MF_USC_LSC);   /* default */
			hal_data->RTSEN = 0;
			primary_cca->dup_rts_flag = 0;
			primary_cca->intf_type = 0;
			primary_cca->intf_flag = 0;
		}
	}
}

#elif (DM_ODM_SUPPORT_TYPE == ODM_AP)

void
odm_intf_detection(
	void			*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct		*false_alm_cnt = &(dm->false_alm_cnt);
	struct phydm_pricca_struct			*primary_cca = &(dm->dm_pri_cca);

	if (false_alm_cnt->cnt_ofdm_cca > OFDMCCA_TH
	    && (false_alm_cnt->cnt_bw_lsc > (false_alm_cnt->cnt_bw_usc + bw_ind_bias))) {
		primary_cca->intf_flag = 1;
		primary_cca->ch_offset = 1;  /* 1:LSC, 2:USC */
		if (false_alm_cnt->cnt_ofdm_fail > (false_alm_cnt->cnt_ofdm_cca >> 1))
			primary_cca->intf_type = 1;
		else
			primary_cca->intf_type = 2;
	} else if ((false_alm_cnt->cnt_ofdm_cca > OFDMCCA_TH)
		&& (false_alm_cnt->cnt_bw_usc > (false_alm_cnt->cnt_bw_lsc + bw_ind_bias))) {
		primary_cca->intf_flag = 1;
		primary_cca->ch_offset = 2;  /* 1:LSC, 2:USC */
		if (false_alm_cnt->cnt_ofdm_fail > (false_alm_cnt->cnt_ofdm_cca >> 1))
			primary_cca->intf_type = 1;
		else
			primary_cca->intf_type = 2;
	} else {
		primary_cca->intf_flag = 0;
		primary_cca->intf_type = 0;
		primary_cca->ch_offset = 0;
	}
}

void
odm_dynamic_primary_cca_ap_8192e(
	void			*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct phydm_pricca_struct		*primary_cca = &(dm->dm_pri_cca);
	u8		i;
	static u32	count_down = PRI_CCA_MONITOR_TIME;
	u8		STA_BW = false, STA_BW_pre = false, STA_BW_TMP = false;
	boolean		is_connected = false;
	u8		sec_ch_offset;
	u8		cur_mf_state;
	struct cmn_sta_info	*entry;

	if (!dm->is_linked)
		return;

	if (!(dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	sec_ch_offset = *(dm->sec_ch_offset);		/* AP: 1: sec is below,  2: sec is above */

	for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
		entry = dm->phydm_sta_info[i];
		if (is_sta_active(entry)) {
			STA_BW_TMP = entry->bw_mode;
			if (STA_BW_TMP > STA_BW)
				STA_BW = STA_BW_TMP;
			is_connected = true;
		}
	}

	if (*dm->band_width == CHANNEL_WIDTH_40) {
		if (primary_cca->pri_cca_flag == 0) {
			if (is_connected) {
				if (STA_BW == CHANNEL_WIDTH_20) { /* 2 STA BW=20M */
					primary_cca->pri_cca_flag = 1;
					if (sec_ch_offset == 1) {
						cur_mf_state = MF_USC;
						phydm_write_dynamic_cca(dm, cur_mf_state);
					} else if (sec_ch_offset == 2) {
						cur_mf_state = MF_USC;
						phydm_write_dynamic_cca(dm, cur_mf_state);
					}
				} else {     			/* 2  STA BW=40M */
					if (primary_cca->intf_flag == 0)
						odm_intf_detection(dm);
					else {	/* intf_flag = 1 */
						if (primary_cca->intf_type == 1) {
							if (primary_cca->ch_offset == 1) {
								cur_mf_state = MF_USC;
								if (sec_ch_offset == 1)  /* AP,  1: primary is above  2: primary is below */
									phydm_write_dynamic_cca(dm, cur_mf_state);
							} else if (primary_cca->ch_offset == 2) {
								cur_mf_state = MF_LSC;
								if (sec_ch_offset == 2)
									phydm_write_dynamic_cca(dm, cur_mf_state);
							}
						} else if (primary_cca->intf_type == 2)
							PHYDM_DBG(dm, DBG_PRI_CCA, "92E: primary_cca->intf_type = 2\n");
					}
				}

			} else		/* disconnected  interference detection */
				odm_intf_detection(dm); /* end of disconnected */


		} else {	/* primary_cca->pri_cca_flag == 1 */

			if (STA_BW == 0) {
				STA_BW_pre = STA_BW;
				return;
			}

			count_down--;
			if (count_down == 0 || ((STA_BW & STA_BW_pre) != 1)) {
				count_down = PRI_CCA_MONITOR_TIME;
				primary_cca->pri_cca_flag = 0;
				primary_cca->intf_type = 0;
				primary_cca->intf_flag = 0;
				cur_mf_state = MF_USC_LSC;
				phydm_write_dynamic_cca(dm, cur_mf_state); /* default */
			}
		}
		STA_BW_pre = STA_BW;

	} else {
		/* 2 Reset */
		phydm_primary_cca_init(dm);
		cur_mf_state = MF_USC_LSC;
		phydm_write_dynamic_cca(dm, cur_mf_state);
		count_down = PRI_CCA_MONITOR_TIME;
	}
}
#endif

#endif /* RTL8192E_SUPPORT == 1 */
#endif

#endif

boolean
odm_dynamic_primary_cca_dup_rts(void *dm_void)
{
#ifdef PHYDM_PRIMARY_CCA
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_pricca_struct *primary_cca = &dm->dm_pri_cca;

	return primary_cca->dup_rts_flag;
#else
	return 0;
#endif
}

void phydm_primary_cca_init(void *dm_void)
{
#ifdef PHYDM_PRIMARY_CCA
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_pricca_struct *primary_cca = &dm->dm_pri_cca;

	if (!(dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	PHYDM_DBG(dm, DBG_PRI_CCA, "[PriCCA] Init ==>\n");
#if (RTL8188E_SUPPORT == 1) || (RTL8192E_SUPPORT == 1)
	primary_cca->dup_rts_flag = 0;
	primary_cca->intf_flag = 0;
	primary_cca->intf_type = 0;
	primary_cca->monitor_flag = 0;
	primary_cca->pri_cca_flag = 0;
	primary_cca->ch_offset = 0;
#endif
	primary_cca->mf_state = 0xff;
	primary_cca->pre_bw = (enum channel_width)0xff;

	if (dm->support_ic_type & ODM_IC_11N_SERIES)
		primary_cca->cca_th_40m_bkp = (u8)odm_get_bb_reg(dm, R_0xc84, 0xf0000000);
#endif
}

void phydm_primary_cca(void *dm_void)
{
#ifdef PHYDM_PRIMARY_CCA
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (!(dm->support_ic_type & ODM_IC_11N_SERIES))
		return;

	if (!(dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	phydm_primary_cca_11n(dm);

#endif
}
