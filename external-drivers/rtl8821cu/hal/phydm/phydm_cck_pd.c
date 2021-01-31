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

#ifdef PHYDM_SUPPORT_CCKPD

void phydm_write_cck_cca_th_new_cs_ratio(void *dm_void, u8 cca_th,
					 u8 cca_th_aaa)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;

	PHYDM_DBG(dm, DBG_CCKPD, "%s ======>\n", __func__);
	PHYDM_DBG(dm, DBG_CCKPD, "[New] pd_th=0x%x, cs_ratio=0x%x\n\n", cca_th,
		  cca_th_aaa);

	if (cckpd_t->cur_cck_cca_thres != cca_th) {
		cckpd_t->cur_cck_cca_thres = cca_th;
		odm_set_bb_reg(dm, R_0xa08, 0xf0000, cca_th);
		cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
	}

	if (cckpd_t->cck_cca_th_aaa != cca_th_aaa) {
		cckpd_t->cck_cca_th_aaa = cca_th_aaa;
		odm_set_bb_reg(dm, R_0xaa8, 0x1f0000, cca_th_aaa);
		cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
	}
}

void phydm_write_cck_cca_th_92f(void *dm_void, u8 cca_pd_th, u8 cca_cs_ratio,
				u8 type)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;

	PHYDM_DBG(dm, DBG_CCKPD, "%s ======>\n", __func__);
	PHYDM_DBG(dm, DBG_CCKPD,
		  "[92F] pd_th=0x%x, cs_ratio=0x%x, type=0x%x\n\n", cca_pd_th,
		  cca_cs_ratio, type);
#if 0
	type=0:  regAc8[7:0]      r_PD_lim_RFBW20_1R
	type=1:  regAc8[15:8]	  r_PD_lim_RFBW20_2R
	type=2:  regAcc[7:0]       r_PD_lim_RFBW40_1R
	type=3:  regAcc[15:8]	  r_PD_lim_RFBW40_2R

	type=0:  Ad0[4:0]	       r_CS_ratio_RFBW20_1R[4:0]
	type=1:  Ad0[9:5]	       r_CS_ratio_RFBW20_2R[4:0]
	type=2:  Ad0[24:20]	r_CS_ratio_RFBW40_1R[4:0]
	type=3:  Ad0[29:25]	r_CS_ratio_RFBW40_2R[4:0]
#endif
	switch (type) {
	case 0: /*RFBW20_1R*/
	{
		if (cckpd_t->cur_cck_pd_20m_1r != cca_pd_th) {
			cckpd_t->cur_cck_pd_20m_1r = cca_pd_th;
			odm_set_bb_reg(dm, R_0xac8, 0xff, cca_pd_th);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}
		if (cckpd_t->cur_cck_cs_ratio_20m_1r != cca_cs_ratio) {
			cckpd_t->cur_cck_cs_ratio_20m_1r = cca_cs_ratio;
			odm_set_bb_reg(dm, R_0xad0, 0x1f, cca_cs_ratio);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}
	} break;
	case 1: /*RFBW20_2R*/
	{
		if (cckpd_t->cur_cck_pd_20m_2r != cca_pd_th) {
			cckpd_t->cur_cck_pd_20m_2r = cca_pd_th;
			odm_set_bb_reg(dm, R_0xac8, 0xff00, cca_pd_th);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}
		if (cckpd_t->cur_cck_cs_ratio_20m_2r != cca_cs_ratio) {
			cckpd_t->cur_cck_cs_ratio_20m_2r = cca_cs_ratio;
			odm_set_bb_reg(dm, R_0xad0, 0x3e0, cca_cs_ratio);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}

	} break;
	case 2: /*RFBW40_1R*/
	{
		if (cckpd_t->cur_cck_pd_40m_1r != cca_pd_th) {
			cckpd_t->cur_cck_pd_40m_1r = cca_pd_th;
			odm_set_bb_reg(dm, R_0xacc, 0xff, cca_pd_th);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}
		if (cckpd_t->cur_cck_cs_ratio_40m_1r != cca_cs_ratio) {
			cckpd_t->cur_cck_cs_ratio_40m_1r = cca_cs_ratio;
			odm_set_bb_reg(dm, R_0xad0, 0x1f00000, cca_cs_ratio);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}

	} break;
	case 3: /*RFBW40_2R*/
	{
		if (cckpd_t->cur_cck_pd_40m_2r != cca_pd_th) {
			cckpd_t->cur_cck_pd_40m_2r = cca_pd_th;
			odm_set_bb_reg(dm, R_0xacc, 0xff00, cca_pd_th);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}
		if (cckpd_t->cur_cck_cs_ratio_40m_2r != cca_cs_ratio) {
			cckpd_t->cur_cck_cs_ratio_40m_2r = cca_cs_ratio;
			odm_set_bb_reg(dm, R_0xad0, 0x3e000000, cca_cs_ratio);
			cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
		}

	} break;

	default:
		break;
	}
}

void phydm_write_cck_cca_th(void *dm_void, u8 cca_th)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;

	PHYDM_DBG(dm, DBG_CCKPD, "%s ======>\n", __func__);
	PHYDM_DBG(dm, DBG_CCKPD, "New cck_cca_th=((0x%x))\n\n", cca_th);

	if (cckpd_t->cur_cck_cca_thres != cca_th) {
		odm_write_1byte(dm, ODM_REG(CCK_CCA, dm), cca_th);
		cckpd_t->cck_fa_ma = CCK_FA_MA_RESET;
	}
	cckpd_t->cur_cck_cca_thres = cca_th;
}

void phydm_set_cckpd_val(void *dm_void, u32 *val_buf, u8 val_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 val_0 = 0, val_1 = 0;
	u8 en_2rcca;
	u8 en_bw40m;
	if (val_len != 2) {
		PHYDM_DBG(dm, ODM_COMP_API, "[Error][CCKPD]Need val_len=2\n");
		return;
	}

	/*val_buf[0]: 0xa0a*/
	/*val_buf[1]: 0xaaa*/
	val_0 = (u8)val_buf[0];
	val_1 = (u8)val_buf[1];

	if (dm->support_ic_type & EXTEND_CCK_CCATH_AAA_IC)
		phydm_write_cck_cca_th_new_cs_ratio(dm, val_0, val_1);
	else if (dm->support_ic_type & EXTEND_CCK_CCATH_92F_IC) {
		en_2rcca = (u8)(odm_get_bb_reg(dm, R_0xa2c, BIT(17)));
		en_bw40m = (u8)(odm_get_bb_reg(dm, R_0x800, BIT(0)));
		if (en_2rcca == 0) {
			if (en_bw40m == 0)
				phydm_write_cck_cca_th_92f(dm, val_0, val_1, 0);
			else
				phydm_write_cck_cca_th_92f(dm, val_0, val_1, 2);
		} else {
			if (en_bw40m == 0)
				phydm_write_cck_cca_th_92f(dm, val_0, val_1, 1);
			else
				phydm_write_cck_cca_th_92f(dm, val_0, val_1, 3);
		}
	}
	else
		phydm_write_cck_cca_th(dm, val_0);
}

boolean
phydm_stop_cck_pd_th(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (!(dm->support_ability & (ODM_BB_CCK_PD | ODM_BB_FA_CNT))) {
		PHYDM_DBG(dm, DBG_CCKPD, "Not Support\n");

		#if (DM_ODM_SUPPORT_TYPE & (ODM_AP))
		#ifdef MCR_WIRELESS_EXTEND
		phydm_write_cck_cca_th(dm, 0x43);
		#endif
		#endif

		return true;
	}

	if (dm->pause_ability & ODM_BB_CCK_PD) {
		PHYDM_DBG(dm, DBG_CCKPD, "Return: Pause CCKPD in LV=%d\n",
			  dm->pause_lv_table.lv_cckpd);
		return true;
	}

	#if 0 /*(DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))*/
	if (dm->ext_lna)
		return true;
	#endif

	return false;
}

void phydm_cckpd(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 cur_cck_cca_th = cckpd_t->cur_cck_cca_thres;

	if (dm->is_linked) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))

		/*Add hp_hw_id condition due to 22B LPS power consumption issue
		 *and [PCIE-1596]
		 */
		if (dm->hp_hw_id && dm->traffic_load == TRAFFIC_ULTRA_LOW) {
			cur_cck_cca_th = 0x40;
		} else if (dm->rssi_min > 60) {
			cur_cck_cca_th = 0xdd;
		} else if (dm->rssi_min > 35) {
			cur_cck_cca_th = 0xcd;
		} else if (dm->rssi_min > 20) {
			if (cckpd_t->cck_fa_ma > 500)
				cur_cck_cca_th = 0xcd;
			else if (cckpd_t->cck_fa_ma < 250)
				cur_cck_cca_th = 0x83;

		} else {
			if ((dm->p_advance_ota & PHYDM_ASUS_OTA_SETTING) &&
			    cckpd_t->cck_fa_ma > 200)
				cur_cck_cca_th = 0xc3; /*for ASUS OTA test*/
			else
				cur_cck_cca_th = 0x83;
		}

#else /*ODM_AP*/
		if (dig_t->cur_ig_value > 0x32)
			cur_cck_cca_th = 0xed;
		else if (dig_t->cur_ig_value > 0x2a)
			cur_cck_cca_th = 0xdd;
		else if (dig_t->cur_ig_value > 0x24)
			cur_cck_cca_th = 0xcd;
		else
			cur_cck_cca_th = 0x83;

#endif
	} else {
		if (cckpd_t->cck_fa_ma > 1000)
			cur_cck_cca_th = 0x83;
		else if (cckpd_t->cck_fa_ma < 500)
			cur_cck_cca_th = 0x40;
	}

	phydm_write_cck_cca_th(dm, cur_cck_cca_th);
	PHYDM_DBG(dm, DBG_CCKPD, "New cck_cca_th=((0x%x)), IGI=((0x%x))\n",
		  cur_cck_cca_th, dig_t->cur_ig_value);
}

void phydm_cckpd_new_cs_ratio(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 pd_th = 0, cs_ratio = 0, cs_2r_offset = 0;
	u8 igi_curr = dig_t->cur_ig_value;
	u8 en_2rcca;
	boolean is_update = true;

	PHYDM_DBG(dm, DBG_CCKPD, "%s ======>\n", __func__);

	en_2rcca = (u8)(odm_get_bb_reg(dm, R_0xa2c, BIT(18)) &&
			odm_get_bb_reg(dm, R_0xa2c, BIT(22)));

	if (dm->is_linked) {
		if (igi_curr > 0x38 && dm->rssi_min > 32) {
			cs_ratio = dig_t->aaa_default + AAA_BASE + AAA_STEP * 2;
			cs_2r_offset = 5;
			pd_th = 0xd;
		} else if ((igi_curr > 0x2a) && (dm->rssi_min > 32)) {
			cs_ratio = dig_t->aaa_default + AAA_BASE + AAA_STEP;
			cs_2r_offset = 4;
			pd_th = 0xd;
		} else if ((igi_curr > 0x24) ||
			   (dm->rssi_min > 24 && dm->rssi_min <= 30)) {
			cs_ratio = dig_t->aaa_default + AAA_BASE;
			cs_2r_offset = 3;
			pd_th = 0xd;
		} else if ((igi_curr <= 0x24) || (dm->rssi_min < 22)) {
			if (cckpd_t->cck_fa_ma > 1000) {
				cs_ratio = dig_t->aaa_default + AAA_STEP;
				cs_2r_offset = 1;
				pd_th = 0x7;
			} else if (cckpd_t->cck_fa_ma < 500) {
				cs_ratio = dig_t->aaa_default;
				pd_th = 0x3;
			} else {
				is_update = false;
				cs_ratio = cckpd_t->cck_cca_th_aaa;
				pd_th = cckpd_t->cur_cck_cca_thres;
			}
		}
	} else {
		if (cckpd_t->cck_fa_ma > 1000) {
			cs_ratio = dig_t->aaa_default + AAA_STEP;
			cs_2r_offset = 1;
			pd_th = 0x7;
		} else if (cckpd_t->cck_fa_ma < 500) {
			cs_ratio = dig_t->aaa_default;
			pd_th = 0x3;
		} else {
			is_update = false;
			cs_ratio = cckpd_t->cck_cca_th_aaa;
			pd_th = cckpd_t->cur_cck_cca_thres;
		}
	}

	if (en_2rcca) {
		if (cs_ratio >= cs_2r_offset)
			cs_ratio = cs_ratio - cs_2r_offset;
		else
			cs_ratio = 0;
	}

	PHYDM_DBG(dm, DBG_CCKPD, "[New] cs_ratio=0x%x, pd_th=0x%x\n", cs_ratio,
		  pd_th);

	if (is_update) {
		cckpd_t->cur_cck_cca_thres = pd_th;
		cckpd_t->cck_cca_th_aaa = cs_ratio;
		odm_set_bb_reg(dm, R_0xa08, 0xf0000, pd_th);
		odm_set_bb_reg(dm, R_0xaa8, 0x1f0000, cs_ratio);
	}

	/*phydm_write_cck_cca_th_new_cs_ratio(dm, pd_th, cs_ratio);*/
}

#if (RTL8192F_SUPPORT == 1)

void phydm_cckpd_92f_1r_linked(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 pd_th_20m_1r = 0, cs_ratio_20m_1r = 0;
	u8 pd_th_40m_1r = 0, cs_ratio_40m_1r = 0;
	u8 igi_curr = dig_t->cur_ig_value;

	if (igi_curr > 0x38 && dm->rssi_min > 32) {
		pd_th_20m_1r = dig_t->cck_pd_20m_1r + 4;
		pd_th_40m_1r = dig_t->cck_pd_40m_1r + 4;
		cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r + 3;
		cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r + 3;
	} else if ((igi_curr > 0x2a) && (dm->rssi_min > 32)) {
		pd_th_20m_1r = dig_t->cck_pd_20m_1r + 3;
		pd_th_40m_1r = dig_t->cck_pd_40m_1r + 3;
		cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r + 2;
		cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r + 2;
	} else if ((igi_curr > 0x24) ||
		   (dm->rssi_min > 24 && dm->rssi_min <= 30)) {
		pd_th_20m_1r = dig_t->cck_pd_20m_1r + 2;
		pd_th_40m_1r = dig_t->cck_pd_40m_1r + 2;
		cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r + 1;
		cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r + 1;

	} else if ((igi_curr <= 0x24) || (dm->rssi_min < 22)) {
		if (cckpd_t->cck_fa_ma > 1000) {
			pd_th_20m_1r = dig_t->cck_pd_20m_1r + 1;
			pd_th_40m_1r = dig_t->cck_pd_40m_1r + 1;
			cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r + 1;
			cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r + 1;
		} else if (cckpd_t->cck_fa_ma < 500) {
			pd_th_20m_1r = dig_t->cck_pd_20m_1r;
			pd_th_40m_1r = dig_t->cck_pd_40m_1r;
			cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r;
			cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r;
		} else {
			pd_th_20m_1r = cckpd_t->cur_cck_pd_20m_1r;
			pd_th_40m_1r = cckpd_t->cur_cck_pd_40m_1r;
			cs_ratio_20m_1r = cckpd_t->cur_cck_cs_ratio_20m_1r;
			cs_ratio_40m_1r = cckpd_t->cur_cck_cs_ratio_40m_1r;
		}
	}
	cckpd_t->cur_cck_pd_20m_1r = pd_th_20m_1r;
	cckpd_t->cur_cck_pd_40m_1r = pd_th_40m_1r;
	cckpd_t->cur_cck_cs_ratio_20m_1r = cs_ratio_20m_1r;
	cckpd_t->cur_cck_cs_ratio_40m_1r = cs_ratio_40m_1r;
	PHYDM_DBG(dm, DBG_CCKPD,
		  "[92F 1R linked] pd_th_20m_1r=0x%x, pd_th_40m_1r=0x%x, cs_ratio_20m_1r=0x%x, cs_ratio_40m_1r=0x%x\n",
		  pd_th_20m_1r, pd_th_40m_1r, cs_ratio_20m_1r, cs_ratio_40m_1r);
	/*1R 20M*/
	/*phydm_write_cck_cca_th_92f(p_dm, pd_th_20m_1r, cs_ratio_20m_1r,0);*/
	odm_set_bb_reg(dm, R_0xac8, 0xff, pd_th_20m_1r);
	odm_set_bb_reg(dm, R_0xad0, 0x1f, cs_ratio_20m_1r);
	/*1R 40M*/
	/*phydm_write_cck_cca_th_92f(p_dm, pd_th_40m_1r, cs_ratio_40m_1r,2);*/
	odm_set_bb_reg(dm, R_0xacc, 0xff, pd_th_40m_1r);
	odm_set_bb_reg(dm, R_0xad0, 0x1f00000, cs_ratio_40m_1r);
}

void phydm_cckpd_92f_1r(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 pd_th_20m_1r = 0, cs_ratio_20m_1r = 0;
	u8 pd_th_40m_1r = 0, cs_ratio_40m_1r = 0;
	u8 igi_curr = dig_t->cur_ig_value;

	if (dm->is_linked) {
		phydm_cckpd_92f_1r_linked(dm);
	} else {
		if (cckpd_t->cck_fa_ma > 1000) {
			pd_th_20m_1r = dig_t->cck_pd_20m_1r + 1;
			pd_th_40m_1r = dig_t->cck_pd_40m_1r + 1;
			cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r + 1;
			cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r + 1;
		} else if (cckpd_t->cck_fa_ma < 500) {
			pd_th_20m_1r = dig_t->cck_pd_20m_1r;
			pd_th_40m_1r = dig_t->cck_pd_40m_1r;
			cs_ratio_20m_1r = dig_t->cck_cs_ratio_20m_1r;
			cs_ratio_40m_1r = dig_t->cck_cs_ratio_40m_1r;
		} else {
			pd_th_20m_1r = cckpd_t->cur_cck_pd_20m_1r;
			pd_th_40m_1r = cckpd_t->cur_cck_pd_40m_1r;
			cs_ratio_20m_1r = cckpd_t->cur_cck_cs_ratio_20m_1r;
			cs_ratio_40m_1r = cckpd_t->cur_cck_cs_ratio_40m_1r;
		}
		cckpd_t->cur_cck_pd_20m_1r = pd_th_20m_1r;
		cckpd_t->cur_cck_pd_40m_1r = pd_th_40m_1r;
		cckpd_t->cur_cck_cs_ratio_20m_1r = cs_ratio_20m_1r;
		cckpd_t->cur_cck_cs_ratio_40m_1r = cs_ratio_40m_1r;
		PHYDM_DBG(dm, DBG_CCKPD,
		"[92F 1R unlink] pd_th_20m_1r=0x%x, pd_th_40m_1r=0x%x, cs_ratio_20m_1r=0x%x, cs_ratio_40m_1r=0x%x\n",
		pd_th_20m_1r, pd_th_40m_1r, cs_ratio_20m_1r, cs_ratio_40m_1r);
		/*1R 20M*/
		/*phydm_write_cck_cca_th_92f(p_dm, pd_th_20m_1r, cs_ratio_20m_1r,0);*/
		odm_set_bb_reg(dm, R_0xac8, 0xff, pd_th_20m_1r);
		odm_set_bb_reg(dm, R_0xad0, 0x1f, cs_ratio_20m_1r);
		/*1R 40M*/
		/*phydm_write_cck_cca_th_92f(p_dm, pd_th_40m_1r, cs_ratio_40m_1r,2);*/
		odm_set_bb_reg(dm, R_0xacc, 0xff, pd_th_40m_1r);
		odm_set_bb_reg(dm, R_0xad0, 0x1f00000, cs_ratio_40m_1r);
	}
}

void phydm_cckpd_92f_2r_linked(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 pd_th_20m_2r = 0, cs_ratio_20m_2r = 0;
	u8 pd_th_40m_2r = 0, cs_ratio_40m_2r = 0;
	u8 igi_curr = dig_t->cur_ig_value;

	if (igi_curr > 0x38 && dm->rssi_min > 32) {
		pd_th_20m_2r = dig_t->cck_pd_20m_2r + 4;
		pd_th_40m_2r = dig_t->cck_pd_40m_2r + 4;
		cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r + 2;
		cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r + 2;
	} else if ((igi_curr > 0x2a) && (dm->rssi_min > 32)) {
		pd_th_20m_2r = dig_t->cck_pd_20m_2r + 3;
		pd_th_40m_2r = dig_t->cck_pd_40m_2r + 3;
		cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r + 1;
		cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r + 1;
	} else if ((igi_curr > 0x24) ||
		   (dm->rssi_min > 24 && dm->rssi_min <= 30)) {
		pd_th_20m_2r = dig_t->cck_pd_20m_2r + 2;
		pd_th_40m_2r = dig_t->cck_pd_40m_2r + 2;
		cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r + 1;
		cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r + 1;
	} else if ((igi_curr <= 0x24) || (dm->rssi_min < 22)) {
		if (cckpd_t->cck_fa_ma > 1000) {
			pd_th_20m_2r = dig_t->cck_pd_20m_2r + 1;
			pd_th_40m_2r = dig_t->cck_pd_40m_2r + 1;
			cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r + 1;
			cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r + 1;
		} else if (cckpd_t->cck_fa_ma < 500) {
			pd_th_20m_2r = dig_t->cck_pd_20m_2r;
			pd_th_40m_2r = dig_t->cck_pd_40m_2r;
			cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r;
			cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r;
		} else {
			pd_th_20m_2r = cckpd_t->cur_cck_pd_20m_2r;
			pd_th_40m_2r = cckpd_t->cur_cck_pd_40m_2r;
			cs_ratio_20m_2r = cckpd_t->cur_cck_cs_ratio_20m_2r;
			cs_ratio_40m_2r = cckpd_t->cur_cck_cs_ratio_40m_2r;
		}
	}
	cckpd_t->cur_cck_pd_20m_2r = pd_th_20m_2r;
	cckpd_t->cur_cck_pd_40m_2r = pd_th_40m_2r;
	cckpd_t->cur_cck_cs_ratio_20m_2r = cs_ratio_20m_2r;
	cckpd_t->cur_cck_cs_ratio_40m_2r = cs_ratio_40m_2r;
	PHYDM_DBG(dm, DBG_CCKPD,
		  "[92F 2R linked] pd_th_20m_2r=0x%x, pd_th_40m_2r=0x%x, cs_ratio_20m_2r=0x%x, cs_ratio_40m_2r=0x%x\n",
		  pd_th_20m_2r, pd_th_40m_2r, cs_ratio_20m_2r, cs_ratio_40m_2r);

	/*2R 20M*/
	/*phydm_write_cck_cca_th_92f(p_dm, pd_th_20m_2r, cs_ratio_20m_2r,1);*/
	odm_set_bb_reg(dm, R_0xac8, 0xff00, pd_th_20m_2r);
	odm_set_bb_reg(dm, R_0xad0, 0x3e0, cs_ratio_20m_2r);

	/*2R 40M*/
	/*phydm_write_cck_cca_th_92f(p_dm, pd_th_40m_2r, cs_ratio_40m_2r,3);*/
	odm_set_bb_reg(dm, R_0xacc, 0xff00, pd_th_40m_2r);
	odm_set_bb_reg(dm, R_0xad0, 0x3e000000, cs_ratio_40m_2r);
}

void phydm_cckpd_92f_2r(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 pd_th_20m_2r = 0, cs_ratio_20m_2r = 0;
	u8 pd_th_40m_2r = 0, cs_ratio_40m_2r = 0;
	u8 igi_curr = dig_t->cur_ig_value;

	if (dm->is_linked) {
		phydm_cckpd_92f_2r_linked(dm);
	} else {
		if (cckpd_t->cck_fa_ma > 1000) {
			pd_th_20m_2r = dig_t->cck_pd_20m_2r + 1;
			pd_th_40m_2r = dig_t->cck_pd_40m_2r + 1;
			cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r + 1;
			cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r + 1;
		} else if (cckpd_t->cck_fa_ma < 500) {
			pd_th_20m_2r = dig_t->cck_pd_20m_2r;
			pd_th_40m_2r = dig_t->cck_pd_40m_2r;
			cs_ratio_20m_2r = dig_t->cck_cs_ratio_20m_2r;
			cs_ratio_40m_2r = dig_t->cck_cs_ratio_40m_2r;
		} else {
			pd_th_20m_2r = cckpd_t->cur_cck_pd_20m_2r;
			pd_th_40m_2r = cckpd_t->cur_cck_pd_40m_2r;
			cs_ratio_20m_2r = cckpd_t->cur_cck_cs_ratio_20m_2r;
			cs_ratio_40m_2r = cckpd_t->cur_cck_cs_ratio_40m_2r;
		}
		cckpd_t->cur_cck_pd_20m_2r = pd_th_20m_2r;
		cckpd_t->cur_cck_pd_40m_2r = pd_th_40m_2r;
		cckpd_t->cur_cck_cs_ratio_20m_2r = cs_ratio_20m_2r;
		cckpd_t->cur_cck_cs_ratio_40m_2r = cs_ratio_40m_2r;
		PHYDM_DBG(dm, DBG_CCKPD,
			  "[92F 2R unlink] pd_th_20m_2r=0x%x, pd_th_40m_2r=0x%x, cs_ratio_20m_2r=0x%x, cs_ratio_40m_2r=0x%x\n",
			  pd_th_20m_2r, pd_th_40m_2r, cs_ratio_20m_2r, cs_ratio_40m_2r);
		
		/*2R 20M*/
		/*phydm_write_cck_cca_th_92f(p_dm, pd_th_20m_2r, cs_ratio_20m_2r,1);*/
		odm_set_bb_reg(dm, R_0xac8, 0xff00, pd_th_20m_2r);
		odm_set_bb_reg(dm, R_0xad0, 0x3e0, cs_ratio_20m_2r);
		
		/*2R 40M*/
		/*phydm_write_cck_cca_th_92f(p_dm, pd_th_40m_2r, cs_ratio_40m_2r,3);*/
		odm_set_bb_reg(dm, R_0xacc, 0xff00, pd_th_40m_2r);
		odm_set_bb_reg(dm, R_0xad0, 0x3e000000, cs_ratio_40m_2r);
	}
}

void phydm_cckpd_92f(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u32 en_2rcca = 0;

	PHYDM_DBG(dm, DBG_CCKPD, "%s ======>\n", __func__);

	if (dm->support_ic_type & ODM_RTL8192F) {
		en_2rcca = odm_get_bb_reg(dm, R_0xa2c, BIT(17));
	} else {
		en_2rcca = odm_get_bb_reg(dm, R_0xa2c, BIT(18)) &&
		   odm_get_bb_reg(dm, R_0xa2c, BIT(22));
	}

	if (en_2rcca == 0)
		phydm_cckpd_92f_1r(dm);
	else
		phydm_cckpd_92f_2r(dm);

}

void phydm_cck_pd_init_92f(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	u32 reg_tmp = 0;
	/*
	 *type = 0 :  regAc8[7:0]		r_PD_lim_RFBW20_1R
	 *type = 1 :  regAc8[15:8]	r_PD_lim_RFBW20_2R
	 *type = 2 :  regAcc[7:0]		r_PD_lim_RFBW40_1R
	 *type = 3 :  regAcc[15:8]	r_PD_lim_RFBW40_2R

	 *type = 0 :  Ad0[4:0]		r_CS_ratio_RFBW20_1R[4:0]
	 *type = 1 :  Ad0[9:5]		r_CS_ratio_RFBW20_2R[4:0]
	 *type = 2 :  Ad0[24:20]		r_CS_ratio_RFBW40_1R[4:0]
	 *type = 3 :  Ad0[29:25]		r_CS_ratio_RFBW40_2R[4:0]
	 */
	dig_t->cck_pd_20m_1r = (u8)odm_get_bb_reg(dm, R_0xac8, 0xff);
	dig_t->cck_pd_20m_2r = (u8)odm_get_bb_reg(dm, R_0xac8, 0xff00);
	dig_t->cck_pd_40m_1r = (u8)odm_get_bb_reg(dm, R_0xacc, 0xff);
	dig_t->cck_pd_40m_2r = (u8)odm_get_bb_reg(dm, R_0xacc, 0xff00);

	reg_tmp = odm_get_bb_reg(dm, R_0xad0, MASKDWORD);
	dig_t->cck_cs_ratio_20m_1r = (u8)(reg_tmp & 0x1f);
	dig_t->cck_cs_ratio_20m_2r = (u8)((reg_tmp & 0x3e0) >> 5);
	dig_t->cck_cs_ratio_40m_1r = (u8)((reg_tmp & 0x1f00000) >> 20);
	dig_t->cck_cs_ratio_40m_2r = (u8)((reg_tmp & 0x3e000000) >> 25);

	cckpd_t->cur_cck_pd_20m_1r = (u8)odm_get_bb_reg(dm, R_0xac8, 0xff);
	cckpd_t->cur_cck_pd_20m_2r = (u8)odm_get_bb_reg(dm, R_0xac8, 0xff00);
	cckpd_t->cur_cck_pd_40m_1r = (u8)odm_get_bb_reg(dm, R_0xacc, 0xff);
	cckpd_t->cur_cck_pd_40m_2r = (u8)odm_get_bb_reg(dm, R_0xacc, 0xff00);

	cckpd_t->cur_cck_cs_ratio_20m_1r = (u8)(reg_tmp & 0x1f);
	cckpd_t->cur_cck_cs_ratio_20m_2r = (u8)((reg_tmp & 0x3e0) >> 5);
	cckpd_t->cur_cck_cs_ratio_40m_1r = (u8)((reg_tmp & 0x1f00000) >> 20);
	cckpd_t->cur_cck_cs_ratio_40m_2r = (u8)((reg_tmp & 0x3e000000) >> 25);

}

#endif

#endif

void phydm_cck_pd_th(void *dm_void)
{
#ifdef PHYDM_SUPPORT_CCKPD
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u32 fail_tmp = fa_t->cnt_cck_fail;
	#ifdef PHYDM_TDMA_DIG_SUPPORT
	struct phydm_fa_acc_struct *fa_acc_t = &dm->false_alm_cnt_acc;
	#endif

	PHYDM_DBG(dm, DBG_CCKPD, "%s ======>\n", __func__);

	if (phydm_stop_cck_pd_th(dm) == true)
		return;

#ifdef PHYDM_TDMA_DIG_SUPPORT
	if (dm->original_dig_restore)
		fail_tmp = fa_t->cnt_cck_fail;
	else
		fail_tmp = fa_acc_t->cnt_cck_fail_1sec;
#endif

	if (cckpd_t->cck_fa_ma == CCK_FA_MA_RESET)
		cckpd_t->cck_fa_ma = fail_tmp;
	else
		cckpd_t->cck_fa_ma = (cckpd_t->cck_fa_ma * 3 + fail_tmp) >> 2;

	PHYDM_DBG(dm, DBG_CCKPD, "CCK FA=%d\n", cckpd_t->cck_fa_ma);

	if (dm->support_ic_type & EXTEND_CCK_CCATH_AAA_IC)
		phydm_cckpd_new_cs_ratio(dm);
	#if (RTL8192F_SUPPORT == 1)
	else if (dm->support_ic_type & EXTEND_CCK_CCATH_92F_IC)
		phydm_cckpd_92f(dm);
	#endif
	else
		phydm_cckpd(dm);

#endif
}

void odm_pause_cck_packet_detection(void *dm_void, enum phydm_pause_type type,
				    enum phydm_pause_level lv,
				    u8 cck_pd_th_input)
{
#ifdef PHYDM_SUPPORT_CCKPD
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	u8 rpt = false;
	u32 val_buf[2] = {0};

	val_buf[0] = (u32)cck_pd_th_input;
	val_buf[1] = (u32)cckpd_t->cck_cca_th_aaa;

if (dm->support_ic_type & EXTEND_CCK_CCATH_92F_IC) {
	if (cck_pd_th_input == 0x40) {
		val_buf[0] = (u32)0x00;
		val_buf[1] = (u32)0x0C;
	} else if (cck_pd_th_input == 0x83) {
		val_buf[0] = (u32)0x08;
		val_buf[1] = (u32)0x10;
	} else if (cck_pd_th_input == 0xcd) {
		val_buf[0] = (u32)0x08;
		val_buf[1] = (u32)0x12;
	} else if (cck_pd_th_input == 0x0) {
		val_buf[0] = (u32)0x00;
		val_buf[1] = (u32)0x0c;
	} else {
		val_buf[0] = (u32)0x08;
		val_buf[1] = (u32)0x10;
	}
}

	PHYDM_DBG(dm, DBG_CCKPD,
		  "[%s] type = %s, LV = %d, cck_pd_th_input = 0x%x\n", __func__,
		  ((type == PHYDM_PAUSE) ? "Pause" :
		  ((type == PHYDM_RESUME) ? "Resume" : "PauseNoSet")), lv,
		  cck_pd_th_input);

	switch (type) {
	case PHYDM_PAUSE:
	case PHYDM_PAUSE_NO_SET: {
		rpt = phydm_pause_func(dm, F05_CCK_PD, PHYDM_PAUSE,
				       lv, 2, &val_buf[0]);
		break;
	}
	case PHYDM_RESUME: {
		rpt = phydm_pause_func(dm, F05_CCK_PD, PHYDM_RESUME,
				       lv, 2, &val_buf[0]);
		break;
	}
	default:
		PHYDM_DBG(dm, DBG_CCKPD, "Wrong type\n");
		break;
	}

	PHYDM_DBG(dm, DBG_CCKPD, "pause_result=%d\n", rpt);
#endif
}

void phydm_cck_pd_init(void *dm_void)
{
#ifdef PHYDM_SUPPORT_CCKPD
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cckpd_struct *cckpd_t = &dm->dm_cckpd_table;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	cckpd_t->cur_cck_cca_thres = 0;
	cckpd_t->cck_cca_th_aaa = 0;

	if (dm->support_ic_type & EXTEND_CCK_CCATH_AAA_IC) {
		dig_t->aaa_default = odm_read_1byte(dm, 0xaaa) & 0x1f;
		dig_t->a0a_default = (u8)odm_get_bb_reg(dm, R_0xa08, 0xff0000);
		cckpd_t->cck_cca_th_aaa = dig_t->aaa_default;
		cckpd_t->cur_cck_cca_thres = dig_t->a0a_default;
	#if (RTL8192F_SUPPORT == 1)
	} else if (dm->support_ic_type & EXTEND_CCK_CCATH_92F_IC) {
		phydm_cck_pd_init_92f(dm);
	#endif
	} else {
		dig_t->a0a_default = (u8)odm_get_bb_reg(dm, R_0xa08, 0xff0000);
		cckpd_t->cur_cck_cca_thres = dig_t->a0a_default;
	}
#endif
}
