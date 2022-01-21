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

void phydm_init_debug_setting(struct dm_struct *dm)
{
	dm->fw_debug_components = 0;
	dm->debug_components =

#if DBG
	/*BB Functions*/
	/*DBG_DIG					|*/
	/*DBG_RA_MASK					|*/
	/*DBG_DYN_TXPWR					|*/
	/*DBG_FA_CNT					|*/
	/*DBG_RSSI_MNTR					|*/
	/*DBG_CCKPD					|*/
	/*DBG_ANT_DIV					|*/
	/*DBG_SMT_ANT					|*/
	/*DBG_PWR_TRAIN					|*/
	/*DBG_RA					|*/
	/*DBG_PATH_DIV					|*/
	/*DBG_DFS					|*/
	/*DBG_DYN_ARFR					|*/
	/*DBG_ADPTVTY					|*/
	/*DBG_CFO_TRK					|*/
	/*DBG_ENV_MNTR					|*/
	/*DBG_PRI_CCA					|*/
	/*DBG_ADPTV_SOML				|*/
	/*DBG_LNA_SAT_CHK				|*/
	/*DBG_PHY_STATUS				|*/
	/*DBG_TMP					|*/
	/*DBG_FW_TRACE					|*/
	/*DBG_TXBF					|*/
	/*DBG_COMMON_FLOW				|*/
	/*ODM_PHY_CONFIG				|*/
	/*ODM_COMP_INIT					|*/
	/*DBG_CMN					|*/
	/*ODM_COMP_API					|*/
#endif
	0;

	dm->fw_buff_is_enpty = true;
	dm->pre_c2h_seq = 0;
	dm->c2h_cmd_start = 0;
	dm->cmn_dbg_msg_cnt = PHYDM_WATCH_DOG_PERIOD;
	dm->cmn_dbg_msg_period = PHYDM_WATCH_DOG_PERIOD;
}

void phydm_bb_dbg_port_header_sel(void *dm_void, u32 header_idx)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x8f8, 0x3c00000, header_idx);

		/*
		 * header_idx:
		 *	(0:) '{ofdm_dbg[31:0]}'
		 *	(1:) '{cca,crc32_fail,dbg_ofdm[29:0]}'
		 *	(2:) '{vbon,crc32_fail,dbg_ofdm[29:0]}'
		 *	(3:) '{cca,crc32_ok,dbg_ofdm[29:0]}'
		 *	(4:) '{vbon,crc32_ok,dbg_ofdm[29:0]}'
		 *	(5:) '{dbg_iqk_anta}'
		 *	(6:) '{cca,ofdm_crc_ok,dbg_dp_anta[29:0]}'
		 *	(7:) '{dbg_iqk_antb}'
		 *	(8:) '{DBGOUT_RFC_b[31:0]}'
		 *	(9:) '{DBGOUT_RFC_a[31:0]}'
		 *	(a:) '{dbg_ofdm}'
		 *	(b:) '{dbg_cck}'
		 */
	}
}

void phydm_bb_dbg_port_clock_en(void *dm_void, u8 enable)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 reg_value = 0;

	if (dm->support_ic_type &
	    (ODM_RTL8822B | ODM_RTL8821C | ODM_RTL8814A | ODM_RTL8814B)) {
		/*enable/disable debug port clock, for power saving*/
		reg_value = enable ? 0x7 : 0;
		odm_set_bb_reg(dm, R_0x198c, 0x7, reg_value);
	}
}

u8 phydm_set_bb_dbg_port(void *dm_void, u8 curr_dbg_priority, u32 debug_port)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 dbg_port_result = false;

	if (curr_dbg_priority > dm->pre_dbg_priority) {
		if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
			phydm_bb_dbg_port_clock_en(dm, true);

			odm_set_bb_reg(dm, R_0x8fc, MASKDWORD, debug_port);
			/**/
		} else if (dm->support_ic_type & ODM_RTL8198F) {
			odm_set_bb_reg(dm, R_0x1c3c, 0xfff00, debug_port);

		} else { /*if (dm->support_ic_type & ODM_IC_11N_SERIES)*/
			odm_set_bb_reg(dm, R_0x908, MASKDWORD, debug_port);
			/**/
		}
		PHYDM_DBG(dm, ODM_COMP_API,
			  "DbgPort ((0x%x)) set success, Cur_priority=((%d)), Pre_priority=((%d))\n",
			  debug_port, curr_dbg_priority, dm->pre_dbg_priority);
		dm->pre_dbg_priority = curr_dbg_priority;
		dbg_port_result = true;
	}

	return dbg_port_result;
}

void phydm_release_bb_dbg_port(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	phydm_bb_dbg_port_clock_en(dm, false);
	phydm_bb_dbg_port_header_sel(dm, 0);

	dm->pre_dbg_priority = DBGPORT_RELEASE;
	PHYDM_DBG(dm, ODM_COMP_API, "Release BB dbg_port\n");
}

u32 phydm_get_bb_dbg_port_val(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 dbg_port_value = 0;

	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		dbg_port_value = odm_get_bb_reg(dm, R_0xfa0, MASKDWORD);
		/**/
	} else /*if (dm->support_ic_type & ODM_IC_11N_SERIES)*/ {
		dbg_port_value = odm_get_bb_reg(dm, R_0xdf4, MASKDWORD);
		/**/
	}
	PHYDM_DBG(dm, ODM_COMP_API, "dbg_port_value = 0x%x\n", dbg_port_value);
	return dbg_port_value;
}

#ifdef CONFIG_PHYDM_DEBUG_FUNCTION

void phydm_bb_debug_info_n_series(void *dm_void, u32 *_used, char *output,
				  u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;

	u32 value32 = 0, value32_1 = 0;
	u8 rf_gain_a = 0, rf_gain_b = 0, rf_gain_c = 0, rf_gain_d = 0;
	u8 rx_snr_a = 0, rx_snr_b = 0, rx_snr_c = 0, rx_snr_d = 0;

	s8 rxevm_0 = 0, rxevm_1 = 0;
	s32 short_cfo_a = 0, short_cfo_b = 0, long_cfo_a = 0, long_cfo_b = 0;
	s32 scfo_a = 0, scfo_b = 0, avg_cfo_a = 0, avg_cfo_b = 0;
	s32 cfo_end_a = 0, cfo_end_b = 0, acq_cfo_a = 0, acq_cfo_b = 0;

	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s\n",
		 "BB Report Info");

	/*AGC result*/
	value32 = odm_get_bb_reg(dm, R_0xdd0, MASKDWORD);
	rf_gain_a = (u8)(value32 & 0x3f);
	rf_gain_a = rf_gain_a << 1;

	rf_gain_b = (u8)((value32 >> 8) & 0x3f);
	rf_gain_b = rf_gain_b << 1;

	rf_gain_c = (u8)((value32 >> 16) & 0x3f);
	rf_gain_c = rf_gain_c << 1;

	rf_gain_d = (u8)((value32 >> 24) & 0x3f);
	rf_gain_d = rf_gain_d << 1;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d / %d", "OFDM RX RF Gain(A/B/C/D)",
		 rf_gain_a, rf_gain_b, rf_gain_c, rf_gain_d);

	/*SNR report*/
	value32 = odm_get_bb_reg(dm, R_0xdd4, MASKDWORD);
	rx_snr_a = (u8)(value32 & 0xff);
	rx_snr_a = rx_snr_a >> 1;

	rx_snr_b = (u8)((value32 >> 8) & 0xff);
	rx_snr_b = rx_snr_b >> 1;

	rx_snr_c = (u8)((value32 >> 16) & 0xff);
	rx_snr_c = rx_snr_c >> 1;

	rx_snr_d = (u8)((value32 >> 24) & 0xff);
	rx_snr_d = rx_snr_d >> 1;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d / %d", "RXSNR(A/B/C/D, dB)",
		 rx_snr_a, rx_snr_b, rx_snr_c, rx_snr_d);

	/* PostFFT related info*/
	value32 = odm_get_bb_reg(dm, R_0xdd8, MASKDWORD);

	rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
	rxevm_0 /= 2;
	if (rxevm_0 < -63)
		rxevm_0 = 0;

	rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
	rxevm_1 /= 2;
	if (rxevm_1 < -63)
		rxevm_1 = 0;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "RXEVM (1ss/2ss)", rxevm_0, rxevm_1);

	/*CFO Report Info*/
	odm_set_bb_reg(dm, R_0xd00, BIT(26), 1);

	/*Short CFO*/
	value32 = odm_get_bb_reg(dm, R_0xdac, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xdb0, MASKDWORD);

	short_cfo_b = (s32)(value32 & 0xfff); /*S(12,11)*/
	short_cfo_a = (s32)((value32 & 0x0fff0000) >> 16);

	long_cfo_b = (s32)(value32_1 & 0x1fff); /*S(13,12)*/
	long_cfo_a = (s32)((value32_1 & 0x1fff0000) >> 16);

	/*SFO 2's to dec*/
	if (short_cfo_a > 2047)
		short_cfo_a = short_cfo_a - 4096;
	if (short_cfo_b > 2047)
		short_cfo_b = short_cfo_b - 4096;

	short_cfo_a = (short_cfo_a * 312500) / 2048;
	short_cfo_b = (short_cfo_b * 312500) / 2048;

	/*LFO 2's to dec*/

	if (long_cfo_a > 4095)
		long_cfo_a = long_cfo_a - 8192;

	if (long_cfo_b > 4095)
		long_cfo_b = long_cfo_b - 8192;

	long_cfo_a = long_cfo_a * 312500 / 4096;
	long_cfo_b = long_cfo_b * 312500 / 4096;

	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "CFO Report Info");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "Short CFO(Hz) <A/B>", short_cfo_a,
		 short_cfo_b);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "Long CFO(Hz) <A/B>", long_cfo_a,
		 long_cfo_b);

	/*SCFO*/
	value32 = odm_get_bb_reg(dm, R_0xdb8, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xdb4, MASKDWORD);

	scfo_b = (s32)(value32 & 0x7ff); /*S(11,10)*/
	scfo_a = (s32)((value32 & 0x07ff0000) >> 16);

	if (scfo_a > 1023)
		scfo_a = scfo_a - 2048;

	if (scfo_b > 1023)
		scfo_b = scfo_b - 2048;

	scfo_a = scfo_a * 312500 / 1024;
	scfo_b = scfo_b * 312500 / 1024;

	avg_cfo_b = (s32)(value32_1 & 0x1fff); /*S(13,12)*/
	avg_cfo_a = (s32)((value32_1 & 0x1fff0000) >> 16);

	if (avg_cfo_a > 4095)
		avg_cfo_a = avg_cfo_a - 8192;

	if (avg_cfo_b > 4095)
		avg_cfo_b = avg_cfo_b - 8192;

	avg_cfo_a = avg_cfo_a * 312500 / 4096;
	avg_cfo_b = avg_cfo_b * 312500 / 4096;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "value SCFO(Hz) <A/B>", scfo_a,
		 scfo_b);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "Avg CFO(Hz) <A/B>", avg_cfo_a,
		 avg_cfo_b);

	value32 = odm_get_bb_reg(dm, R_0xdbc, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xde0, MASKDWORD);

	cfo_end_b = (s32)(value32 & 0x1fff); /*S(13,12)*/
	cfo_end_a = (s32)((value32 & 0x1fff0000) >> 16);

	if (cfo_end_a > 4095)
		cfo_end_a = cfo_end_a - 8192;

	if (cfo_end_b > 4095)
		cfo_end_b = cfo_end_b - 8192;

	cfo_end_a = cfo_end_a * 312500 / 4096;
	cfo_end_b = cfo_end_b * 312500 / 4096;

	acq_cfo_b = (s32)(value32_1 & 0x1fff); /*S(13,12)*/
	acq_cfo_a = (s32)((value32_1 & 0x1fff0000) >> 16);

	if (acq_cfo_a > 4095)
		acq_cfo_a = acq_cfo_a - 8192;

	if (acq_cfo_b > 4095)
		acq_cfo_b = acq_cfo_b - 8192;

	acq_cfo_a = acq_cfo_a * 312500 / 4096;
	acq_cfo_b = acq_cfo_b * 312500 / 4096;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "End CFO(Hz) <A/B>", cfo_end_a,
		 cfo_end_b);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "ACQ CFO(Hz) <A/B>", acq_cfo_a,
		 acq_cfo_b);
}

void phydm_bb_debug_info(void *dm_void, u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;

	char *tmp_string = NULL;

	u8 rx_ht_bw, rx_vht_bw, rxsc, rx_ht, rx_bw;
	static u8 v_rx_bw;
	u32 value32, value32_1, value32_2, value32_3;
	s32 sfo_a, sfo_b, sfo_c, sfo_d;
	s32 lfo_a, lfo_b, lfo_c, lfo_d;
	static u8 MCSS, tail, parity, rsv, vrsv, idx, smooth, htsound, agg;
	static u8 stbc, vstbc, fec, fecext, sgi, sgiext, htltf, vgid, v_nsts;
	static u8 vtxops, vrsv2, vbrsv, bf, vbcrc;
	static u16 h_length, htcrc8, length;
	static u16 vpaid;
	static u16 v_length, vhtcrc8, v_mcss, v_tail, vb_tail;
	static u8 hmcss, hrx_bw;

	u8 pwdb;
	s8 rxevm_0, rxevm_1, rxevm_2;
	u8 rf_gain_path_a, rf_gain_path_b, rf_gain_path_c, rf_gain_path_d;
	u8 rx_snr_path_a, rx_snr_path_b, rx_snr_path_c, rx_snr_path_d;
	s32 sig_power;
	const u8 l_rate[8] = {6, 9, 12, 18, 24, 36, 48, 54};

#if 0
	const double evm_comp_20M = 0.579919469776867; /* 10*log10(64.0/56.0) */
	const double evm_comp_40M = 0.503051183113957; /* 10*log10(128.0/114.0) */
	const double evm_comp_80M = 0.244245993314183; /* 10*log10(256.0/242.0) */
	const double evm_comp_160M = 0.244245993314183; /* 10*log10(512.0/484.0) */
#endif

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		phydm_bb_debug_info_n_series(dm, &used, output, &out_len);
		return;
	}

	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s\n",
		 "BB Report Info");

	/*BW & mode Detection*/

	value32 = odm_get_bb_reg(dm, R_0xf80, MASKDWORD);
	value32_2 = value32;
	rx_ht_bw = (u8)(value32 & 0x1);
	rx_vht_bw = (u8)((value32 >> 1) & 0x3);
	rxsc = (u8)(value32 & 0x78);
	value32_1 = (value32 & 0x180) >> 7;
	rx_ht = (u8)(value32_1);

	rx_bw = 0;

	if (rx_ht == 2) {
		if (rx_vht_bw == 0)
			tmp_string = "20M";
		else if (rx_vht_bw == 1)
			tmp_string = "40M";
		else
			tmp_string = "80M";
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "\r\n %-35s %s %s", "mode", "VHT", tmp_string);
		rx_bw = rx_vht_bw;
	} else if (rx_ht == 1) {
		if (rx_ht_bw == 0)
			tmp_string = "20M";
		else if (rx_ht_bw == 1)
			tmp_string = "40M";
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "\r\n %-35s %s %s", "mode", "HT", tmp_string);
		rx_bw = rx_ht_bw;
	} else {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "\r\n %-35s %s", "mode", "Legacy");
	}

	if (rx_ht != 0) {
		if (rxsc == 0)
			tmp_string = "duplicate/full bw";
		else if (rxsc == 1)
			tmp_string = "usc20-1";
		else if (rxsc == 2)
			tmp_string = "lsc20-1";
		else if (rxsc == 3)
			tmp_string = "usc20-2";
		else if (rxsc == 4)
			tmp_string = "lsc20-2";
		else if (rxsc == 9)
			tmp_string = "usc40";
		else if (rxsc == 10)
			tmp_string = "lsc40";

		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "  %-35s", tmp_string);
	}

	/* RX signal power and AGC related info*/

	value32 = odm_get_bb_reg(dm, R_0xf90, MASKDWORD);
	pwdb = (u8)((value32 & MASKBYTE1) >> 8);
	pwdb = pwdb >> 1;
	sig_power = -110 + pwdb;
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d", "OFDM RX Signal Power(dB)", sig_power);

	value32 = odm_get_bb_reg(dm, R_0xd14, MASKDWORD);
	rx_snr_path_a = (u8)(value32 & 0xFF) >> 1;
	rf_gain_path_a = (s8)((value32 & MASKBYTE1) >> 8);
	rf_gain_path_a *= 2;
	value32 = odm_get_bb_reg(dm, R_0xd54, MASKDWORD);
	rx_snr_path_b = (u8)(value32 & 0xFF) >> 1;
	rf_gain_path_b = (s8)((value32 & MASKBYTE1) >> 8);
	rf_gain_path_b *= 2;
	value32 = odm_get_bb_reg(dm, R_0xd94, MASKDWORD);
	rx_snr_path_c = (u8)(value32 & 0xFF) >> 1;
	rf_gain_path_c = (s8)((value32 & MASKBYTE1) >> 8);
	rf_gain_path_c *= 2;
	value32 = odm_get_bb_reg(dm, R_0xdd4, MASKDWORD);
	rx_snr_path_d = (u8)(value32 & 0xFF) >> 1;
	rf_gain_path_d = (s8)((value32 & MASKBYTE1) >> 8);
	rf_gain_path_d *= 2;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d / %d", "OFDM RX RF Gain(A/B/C/D)",
		 rf_gain_path_a, rf_gain_path_b, rf_gain_path_c,
		 rf_gain_path_d);

	/* RX counter related info*/

	value32 = odm_get_bb_reg(dm, R_0xf08, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d", "OFDM CCA counter",
		 ((value32 & 0xFFFF0000) >> 16));

	value32 = odm_get_bb_reg(dm, R_0xfd0, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d", "OFDM SBD Fail counter", value32 & 0xFFFF);

	value32 = odm_get_bb_reg(dm, R_0xfc4, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "VHT SIGA/SIGB CRC8 Fail counter",
		 value32 & 0xFFFF, ((value32 & 0xFFFF0000) >> 16));

	value32 = odm_get_bb_reg(dm, R_0xfcc, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d", "CCK CCA counter", value32 & 0xFFFF);

	value32 = odm_get_bb_reg(dm, R_0xfbc, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d",
		 "LSIG (parity Fail/rate Illegal) counter", value32 & 0xFFFF,
		 ((value32 & 0xFFFF0000) >> 16));

	value32_1 = odm_get_bb_reg(dm, R_0xfc8, MASKDWORD);
	value32_2 = odm_get_bb_reg(dm, R_0xfc0, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "HT/VHT MCS NOT SUPPORT counter",
		 ((value32_2 & 0xFFFF0000) >> 16), value32_1 & 0xFFFF);

	/* PostFFT related info*/
	value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);
	rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
	rxevm_0 /= 2;
	if (rxevm_0 < -63)
		rxevm_0 = 0;

	rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
	rxevm_1 /= 2;
	value32 = odm_get_bb_reg(dm, R_0xf88, MASKDWORD);
	rxevm_2 = (s8)((value32 & MASKBYTE2) >> 16);
	rxevm_2 /= 2;

	if (rxevm_1 < -63)
		rxevm_1 = 0;
	if (rxevm_2 < -63)
		rxevm_2 = 0;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d", "RXEVM (1ss/2ss/3ss)", rxevm_0,
		 rxevm_1, rxevm_2);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d / %d", "RXSNR(A/B/C/D, dB)",
		 rx_snr_path_a, rx_snr_path_b, rx_snr_path_c, rx_snr_path_d);

	value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d", "CSI_1st /CSI_2nd", value32 & 0xFFFF,
		 ((value32 & 0xFFFF0000) >> 16));

	/*BW & mode Detection*/

	/*Reset Page F counter*/
	odm_set_bb_reg(dm, R_0xb58, BIT(0), 1);
	odm_set_bb_reg(dm, R_0xb58, BIT(0), 0);

	/*CFO Report Info*/
	/*Short CFO*/
	value32 = odm_get_bb_reg(dm, R_0xd0c, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xd4c, MASKDWORD);
	value32_2 = odm_get_bb_reg(dm, R_0xd8c, MASKDWORD);
	value32_3 = odm_get_bb_reg(dm, R_0xdcc, MASKDWORD);

	sfo_a = (s32)(value32 & 0xfff);
	sfo_b = (s32)(value32_1 & 0xfff);
	sfo_c = (s32)(value32_2 & 0xfff);
	sfo_d = (s32)(value32_3 & 0xfff);

	lfo_a = (s32)(value32 >> 16);
	lfo_b = (s32)(value32_1 >> 16);
	lfo_c = (s32)(value32_2 >> 16);
	lfo_d = (s32)(value32_3 >> 16);

	/*SFO 2's to dec*/
	if (sfo_a > 2047)
		sfo_a = sfo_a - 4096;
	sfo_a = (sfo_a * 312500) / 2048;
	if (sfo_b > 2047)
		sfo_b = sfo_b - 4096;
	sfo_b = (sfo_b * 312500) / 2048;
	if (sfo_c > 2047)
		sfo_c = sfo_c - 4096;
	sfo_c = (sfo_c * 312500) / 2048;
	if (sfo_d > 2047)
		sfo_d = sfo_d - 4096;
	sfo_d = (sfo_d * 312500) / 2048;

	/*LFO 2's to dec*/

	if (lfo_a > 4095)
		lfo_a = lfo_a - 8192;

	if (lfo_b > 4095)
		lfo_b = lfo_b - 8192;

	if (lfo_c > 4095)
		lfo_c = lfo_c - 8192;

	if (lfo_d > 4095)
		lfo_d = lfo_d - 8192;
	lfo_a = lfo_a * 312500 / 4096;
	lfo_b = lfo_b * 312500 / 4096;
	lfo_c = lfo_c * 312500 / 4096;
	lfo_d = lfo_d * 312500 / 4096;
	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "CFO Report Info");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d /%d", "Short CFO(Hz) <A/B/C/D>",
		 sfo_a, sfo_b, sfo_c, sfo_d);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d /%d", "Long CFO(Hz) <A/B/C/D>",
		 lfo_a, lfo_b, lfo_c, lfo_d);

	/*SCFO*/
	value32 = odm_get_bb_reg(dm, R_0xd10, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xd50, MASKDWORD);
	value32_2 = odm_get_bb_reg(dm, R_0xd90, MASKDWORD);
	value32_3 = odm_get_bb_reg(dm, R_0xdd0, MASKDWORD);

	sfo_a = (s32)(value32 & 0x7ff);
	sfo_b = (s32)(value32_1 & 0x7ff);
	sfo_c = (s32)(value32_2 & 0x7ff);
	sfo_d = (s32)(value32_3 & 0x7ff);

	if (sfo_a > 1023)
		sfo_a = sfo_a - 2048;

	if (sfo_b > 2047)
		sfo_b = sfo_b - 4096;

	if (sfo_c > 2047)
		sfo_c = sfo_c - 4096;

	if (sfo_d > 2047)
		sfo_d = sfo_d - 4096;

	sfo_a = sfo_a * 312500 / 1024;
	sfo_b = sfo_b * 312500 / 1024;
	sfo_c = sfo_c * 312500 / 1024;
	sfo_d = sfo_d * 312500 / 1024;

	lfo_a = (s32)(value32 >> 16);
	lfo_b = (s32)(value32_1 >> 16);
	lfo_c = (s32)(value32_2 >> 16);
	lfo_d = (s32)(value32_3 >> 16);

	if (lfo_a > 4095)
		lfo_a = lfo_a - 8192;

	if (lfo_b > 4095)
		lfo_b = lfo_b - 8192;

	if (lfo_c > 4095)
		lfo_c = lfo_c - 8192;

	if (lfo_d > 4095)
		lfo_d = lfo_d - 8192;
	lfo_a = lfo_a * 312500 / 4096;
	lfo_b = lfo_b * 312500 / 4096;
	lfo_c = lfo_c * 312500 / 4096;
	lfo_d = lfo_d * 312500 / 4096;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d /%d", "value SCFO(Hz) <A/B/C/D>",
		 sfo_a, sfo_b, sfo_c, sfo_d);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d /%d", "ACQ CFO(Hz) <A/B/C/D>",
		 lfo_a, lfo_b, lfo_c, lfo_d);

	value32 = odm_get_bb_reg(dm, R_0xd14, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xd54, MASKDWORD);
	value32_2 = odm_get_bb_reg(dm, R_0xd94, MASKDWORD);
	value32_3 = odm_get_bb_reg(dm, R_0xdd4, MASKDWORD);

	lfo_a = (s32)(value32 >> 16);
	lfo_b = (s32)(value32_1 >> 16);
	lfo_c = (s32)(value32_2 >> 16);
	lfo_d = (s32)(value32_3 >> 16);

	if (lfo_a > 4095)
		lfo_a = lfo_a - 8192;

	if (lfo_b > 4095)
		lfo_b = lfo_b - 8192;

	if (lfo_c > 4095)
		lfo_c = lfo_c - 8192;

	if (lfo_d > 4095)
		lfo_d = lfo_d - 8192;

	lfo_a = lfo_a * 312500 / 4096;
	lfo_b = lfo_b * 312500 / 4096;
	lfo_c = lfo_c * 312500 / 4096;
	lfo_d = lfo_d * 312500 / 4096;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %d / %d / %d /%d", "End CFO(Hz) <A/B/C/D>",
		 lfo_a, lfo_b, lfo_c, lfo_d);

	value32 = odm_get_bb_reg(dm, R_0xf20, MASKDWORD); /*L SIG*/

	tail = (u8)((value32 & 0xfc0000) >> 16);
	parity = (u8)((value32 & 0x20000) >> 16);
	length = (u16)((value32 & 0x1ffe00) >> 8);
	rsv = (u8)(value32 & 0x10);
	MCSS = (u8)(value32 & 0x0f);

	switch (MCSS) {
	case 0x0b:
		idx = 0;
		break;
	case 0x0f:
		idx = 1;
		break;
	case 0x0a:
		idx = 2;
		break;
	case 0x0e:
		idx = 3;
		break;
	case 0x09:
		idx = 4;
		break;
	case 0x08:
		idx = 5;
		break;
	case 0x0c:
		idx = 6;
		break;
	default:
		idx = 6;
		break;
	}

	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "L-SIG");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s : %d M", "rate", l_rate[idx]);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x", "Rsv/length/parity", rsv, rx_bw,
		 length);

	value32 = odm_get_bb_reg(dm, R_0xf2c, MASKDWORD); /*HT SIG*/
	if (rx_ht == 1) {
		hmcss = (u8)(value32 & 0x7F);
		hrx_bw = (u8)(value32 & 0x80);
		h_length = (u16)((value32 >> 8) & 0xffff);
	}
	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "HT-SIG1");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x", "MCS/BW/length", hmcss, hrx_bw,
		 h_length);

	value32 = odm_get_bb_reg(dm, R_0xf30, MASKDWORD); /*HT SIG*/

	if (rx_ht == 1) {
		smooth = (u8)(value32 & 0x01);
		htsound = (u8)(value32 & 0x02);
		rsv = (u8)(value32 & 0x04);
		agg = (u8)(value32 & 0x08);
		stbc = (u8)(value32 & 0x30);
		fec = (u8)(value32 & 0x40);
		sgi = (u8)(value32 & 0x80);
		htltf = (u8)((value32 & 0x300) >> 8);
		htcrc8 = (u16)((value32 & 0x3fc00) >> 8);
		tail = (u8)((value32 & 0xfc0000) >> 16);
	}
	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "HT-SIG2");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x / %x / %x / %x",
		 "Smooth/NoSound/Rsv/Aggregate/STBC/LDPC", smooth, htsound, rsv,
		 agg, stbc, fec);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x / %x", "SGI/E-HT-LTFs/CRC/tail",
		 sgi, htltf, htcrc8, tail);

	value32 = odm_get_bb_reg(dm, R_0xf2c, MASKDWORD); /*VHT SIG A1*/
	if (rx_ht == 2) {
		/* value32 = odm_get_bb_reg(dm, R_0xf2c,MASKDWORD);*/
		v_rx_bw = (u8)(value32 & 0x03);
		vrsv = (u8)(value32 & 0x04);
		vstbc = (u8)(value32 & 0x08);
		vgid = (u8)((value32 & 0x3f0) >> 4);
		v_nsts = (u8)(((value32 & 0x1c00) >> 8) + 1);
		vpaid = (u16)(value32 & 0x3fe);
		vtxops = (u8)((value32 & 0x400000) >> 20);
		vrsv2 = (u8)((value32 & 0x800000) >> 20);
	}
	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "VHT-SIG-A1");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x / %x / %x / %x / %x / %x",
		 "BW/Rsv1/STBC/GID/Nsts/PAID/TXOPPS/Rsv2", v_rx_bw, vrsv, vstbc,
		 vgid, v_nsts, vpaid, vtxops, vrsv2);

	value32 = odm_get_bb_reg(dm, R_0xf30, MASKDWORD); /*VHT SIG*/

	if (rx_ht == 2) {
		/*value32 = odm_get_bb_reg(dm, R_0xf30,MASKDWORD); */ /*VHT SIG*/

		/* sgi=(u8)(value32&0x01); */
		sgiext = (u8)(value32 & 0x03);
		/* fec = (u8)(value32&0x04); */
		fecext = (u8)(value32 & 0x0C);

		v_mcss = (u8)(value32 & 0xf0);
		bf = (u8)((value32 & 0x100) >> 8);
		vrsv = (u8)((value32 & 0x200) >> 8);
		vhtcrc8 = (u16)((value32 & 0x3fc00) >> 8);
		v_tail = (u8)((value32 & 0xfc0000) >> 16);
	}
	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "VHT-SIG-A2");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x / %x / %x / %x / %x",
		 "SGI/FEC/MCS/BF/Rsv/CRC/tail", sgiext, fecext, v_mcss, bf,
		 vrsv, vhtcrc8, v_tail);

	value32 = odm_get_bb_reg(dm, R_0xf34, MASKDWORD); /*VHT SIG*/
	{
		v_length = (u16)(value32 & 0x1fffff);
		vbrsv = (u8)((value32 & 0x600000) >> 20);
		vb_tail = (u16)((value32 & 0x1f800000) >> 20);
		vbcrc = (u8)((value32 & 0x80000000) >> 28);
	}
	PDM_SNPF(out_len, used, output + used, out_len - used, "\r\n %-35s",
		 "VHT-SIG-B");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\r\n %-35s = %x / %x / %x / %x", "length/Rsv/tail/CRC",
		 v_length, vbrsv, vb_tail, vbcrc);

	/*for Condition number*/
	if (dm->support_ic_type & ODM_RTL8822B) {
		s32 condition_num = 0;
		char *factor = NULL;

		/*enable report condition number*/
		odm_set_bb_reg(dm, R_0x1988, BIT(22), 0x1);

		condition_num = odm_get_bb_reg(dm, R_0xf84, MASKDWORD);
		condition_num = (condition_num & 0x3ffff) >> 4;

		if (*dm->band_width == CHANNEL_WIDTH_80) {
			factor = "256/234";
		} else if (*dm->band_width == CHANNEL_WIDTH_40) {
			factor = "128/108";
		} else if (*dm->band_width == CHANNEL_WIDTH_20) {
			if (rx_ht != 2 || rx_ht != 1)
				factor = "64/52"; /*HT or VHT*/
			else
				factor = "64/48"; /*legacy*/
		}

		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "\r\n %-35s = %d (factor = %s)", "Condition number",
			 condition_num, factor);
	}
	*_used = used;
	*_out_len = out_len;
}
#endif /*#ifdef CONFIG_PHYDM_DEBUG_FUNCTION*/

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
void phydm_basic_dbg_msg_cli_win(void *dm_void, char *buf)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;
	struct phydm_cfo_track_struct *cfo_t = &dm->dm_cfo_track;
	struct odm_phy_dbg_info *dbg = &dm->phy_dbg_info;
	struct phydm_phystatus_statistic *dbg_statistic = &dm->phy_dbg_info.phystatus_statistic_info;
	struct phydm_phystatus_avg *dbg_avg = &dm->phy_dbg_info.phystatus_statistic_avg;
	u16 macid, phydm_macid, client_cnt = 0;
	u8 i = 0;
	u8 rate_num = dm->num_rf_path;
	u8 rate_ss_shift = 0;
	struct cmn_sta_info *entry = NULL;
	char dbg_buf[PHYDM_SNPRINT_SIZE] = {0};

	RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n PHYDM Common Dbg Msg --------->");
	RT_PRINT(buf);
	RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n System up time=%d", dm->phydm_sys_up_time);
	RT_PRINT(buf);

	if (dm->is_linked) {
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n ID=((%d)), BW=((%d)), fc=((CH-%d))",
			   dm->curr_station_id, 20 << *dm->band_width, *dm->channel);
		RT_PRINT(buf);

		if (((*dm->channel <= 14) && (*dm->band_width == CHANNEL_WIDTH_40)) &&
		    (dm->support_ic_type & ODM_IC_11N_SERIES)) {
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n Primary CCA at ((%s SB))",
				   (*dm->sec_ch_offset == SECOND_CH_AT_LSB) ? "U" : "L");
			RT_PRINT(buf);
		}

		if ((dm->support_ic_type & PHYSTS_2ND_TYPE_IC) || dm->rx_rate > ODM_RATE11M) {
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n [AGC Idx] {0x%x, 0x%x, 0x%x, 0x%x}",
				   dm->ofdm_agc_idx[0], dm->ofdm_agc_idx[1],
				   dm->ofdm_agc_idx[2], dm->ofdm_agc_idx[3]);
			RT_PRINT(buf);
		} else {
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n [CCK AGC Idx] {LNA,VGA}={0x%x, 0x%x}",
				   dm->cck_lna_idx, dm->cck_vga_idx);
			RT_PRINT(buf);
		}

		phydm_print_rate_2_buff(dm, dm->rx_rate, dbg_buf, PHYDM_SNPRINT_SIZE);
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n RSSI:{%d, %d, %d, %d}, RxRate:%s (0x%x)",
			   (dm->rssi_a == 0xff) ? 0 : dm->rssi_a,
			   (dm->rssi_b == 0xff) ? 0 : dm->rssi_b,
			   (dm->rssi_c == 0xff) ? 0 : dm->rssi_c,
			   (dm->rssi_d == 0xff) ? 0 : dm->rssi_d,
			  dbg_buf, dm->rx_rate);
		RT_PRINT(buf);

		phydm_print_rate_2_buff(dm, dm->phy_dbg_info.beacon_phy_rate, dbg_buf, PHYDM_SNPRINT_SIZE);
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n Beacon_cnt=%d, rate_idx:%s (0x%x)",
			   dm->phy_dbg_info.beacon_cnt_in_period,
			   dbg_buf,
			   dm->phy_dbg_info.beacon_phy_rate);
		RT_PRINT(buf);

		/*Show phydm_rx_rate_distribution;*/
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n [RxRate Cnt] =============>");
		RT_PRINT(buf);

		/*======CCK===========================================================*/
		if (*dm->channel <= 14) {
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * CCK = {%d, %d, %d, %d}",
				   dbg->num_qry_legacy_pkt[0], dbg->num_qry_legacy_pkt[1],
				   dbg->num_qry_legacy_pkt[2], dbg->num_qry_legacy_pkt[3]);
			RT_PRINT(buf);
		}
		/*======OFDM==========================================================*/
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * OFDM = {%d, %d, %d, %d, %d, %d, %d, %d}",
			   dbg->num_qry_legacy_pkt[4], dbg->num_qry_legacy_pkt[5],
			   dbg->num_qry_legacy_pkt[6], dbg->num_qry_legacy_pkt[7],
			   dbg->num_qry_legacy_pkt[8], dbg->num_qry_legacy_pkt[9],
			   dbg->num_qry_legacy_pkt[10], dbg->num_qry_legacy_pkt[11]);
		RT_PRINT(buf);

		/*======HT============================================================*/
		if (dbg->ht_pkt_not_zero) {
			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = (i << 3);

				RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}",
					   (rate_ss_shift), (rate_ss_shift + 7),
					   dbg->num_qry_ht_pkt[rate_ss_shift + 0], dbg->num_qry_ht_pkt[rate_ss_shift + 1],
					   dbg->num_qry_ht_pkt[rate_ss_shift + 2], dbg->num_qry_ht_pkt[rate_ss_shift + 3],
					   dbg->num_qry_ht_pkt[rate_ss_shift + 4], dbg->num_qry_ht_pkt[rate_ss_shift + 5],
					   dbg->num_qry_ht_pkt[rate_ss_shift + 6], dbg->num_qry_ht_pkt[rate_ss_shift + 7]);
				RT_PRINT(buf);
			}

			if (dbg->low_bw_20_occur) {
				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = (i << 3);

					RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * [Low BW 20M] HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}",
						   (rate_ss_shift), (rate_ss_shift + 7),
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 0], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 1],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 2], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 3],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 4], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 5],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 6], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 7]);
					RT_PRINT(buf);
				}
			}
		}

		#if ODM_IC_11AC_SERIES_SUPPORT
		/*======VHT=============================================================*/
		if (dbg->vht_pkt_not_zero) {
			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = 10 * i;

				RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}",
					   (i + 1),
					   dbg->num_qry_vht_pkt[rate_ss_shift + 0], dbg->num_qry_vht_pkt[rate_ss_shift + 1],
					   dbg->num_qry_vht_pkt[rate_ss_shift + 2], dbg->num_qry_vht_pkt[rate_ss_shift + 3],
					   dbg->num_qry_vht_pkt[rate_ss_shift + 4], dbg->num_qry_vht_pkt[rate_ss_shift + 5],
					   dbg->num_qry_vht_pkt[rate_ss_shift + 6], dbg->num_qry_vht_pkt[rate_ss_shift + 7],
					   dbg->num_qry_vht_pkt[rate_ss_shift + 8], dbg->num_qry_vht_pkt[rate_ss_shift + 9]);
				RT_PRINT(buf);
			}

			if (dbg->low_bw_20_occur) {
				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = 10 * i;

					RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n *[Low BW 20M] VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}",
						   (i + 1),
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 0], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 1],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 2], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 3],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 4], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 5],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 6], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 7],
						   dbg->num_qry_pkt_sc_20m[rate_ss_shift + 8], dbg->num_qry_pkt_sc_20m[rate_ss_shift + 9]);
					RT_PRINT(buf);
				}
			}

			if (dbg->low_bw_40_occur) {
				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = 10 * i;

					RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n *[Low BW 40M] VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}",
						   (i + 1),
						   dbg->num_qry_pkt_sc_40m[rate_ss_shift + 0], dbg->num_qry_pkt_sc_40m[rate_ss_shift + 1],
						   dbg->num_qry_pkt_sc_40m[rate_ss_shift + 2], dbg->num_qry_pkt_sc_40m[rate_ss_shift + 3],
						   dbg->num_qry_pkt_sc_40m[rate_ss_shift + 4], dbg->num_qry_pkt_sc_40m[rate_ss_shift + 5],
						   dbg->num_qry_pkt_sc_40m[rate_ss_shift + 6], dbg->num_qry_pkt_sc_40m[rate_ss_shift + 7],
						   dbg->num_qry_pkt_sc_40m[rate_ss_shift + 8], dbg->num_qry_pkt_sc_40m[rate_ss_shift + 9]);
					RT_PRINT(buf);
				}
			}
		}
		#endif

		phydm_reset_rx_rate_distribution(dm);

		//1 Show phydm_avg_phystatus_val
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n [Avg PHY Statistic] ==============>");
		RT_PRINT(buf);

		phydm_reset_phystatus_avg(dm);

		/*CCK*/
		dbg_avg->rssi_cck_avg = (u8)((dbg_statistic->rssi_cck_cnt != 0) ? (dbg_statistic->rssi_cck_sum / dbg_statistic->rssi_cck_cnt) : 0);
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * cck Cnt= ((%d)) RSSI:{%d}",
			   dbg_statistic->rssi_cck_cnt, dbg_avg->rssi_cck_avg);
		RT_PRINT(buf);

		/*OFDM*/
		if (dbg_statistic->rssi_ofdm_cnt != 0) {
			dbg_avg->rssi_ofdm_avg = (u8)(dbg_statistic->rssi_ofdm_sum / dbg_statistic->rssi_ofdm_cnt);
			dbg_avg->evm_ofdm_avg = (u8)(dbg_statistic->evm_ofdm_sum / dbg_statistic->rssi_ofdm_cnt);
			dbg_avg->snr_ofdm_avg = (u8)(dbg_statistic->snr_ofdm_sum / dbg_statistic->rssi_ofdm_cnt);
		}

		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * ofdm Cnt= ((%d)) RSSI:{%d} EVM:{%d} SNR:{%d}",
			   dbg_statistic->rssi_ofdm_cnt, dbg_avg->rssi_ofdm_avg,
			   dbg_avg->evm_ofdm_avg, dbg_avg->snr_ofdm_avg);
		RT_PRINT(buf);

		if (dbg_statistic->rssi_1ss_cnt != 0) {
			dbg_avg->rssi_1ss_avg = (u8)(dbg_statistic->rssi_1ss_sum / dbg_statistic->rssi_1ss_cnt);
			dbg_avg->evm_1ss_avg = (u8)(dbg_statistic->evm_1ss_sum / dbg_statistic->rssi_1ss_cnt);
			dbg_avg->snr_1ss_avg = (u8)(dbg_statistic->snr_1ss_sum / dbg_statistic->rssi_1ss_cnt);
		}

		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * 1-ss Cnt= ((%d)) RSSI:{%d} EVM:{%d} SNR:{%d}",
			   dbg_statistic->rssi_1ss_cnt, dbg_avg->rssi_1ss_avg,
			   dbg_avg->evm_1ss_avg, dbg_avg->snr_1ss_avg);
		RT_PRINT(buf);

#if (defined(PHYDM_COMPILE_ABOVE_2SS))
		if (dm->support_ic_type & (PHYDM_IC_ABOVE_2SS)) {
			if (dbg_statistic->rssi_2ss_cnt != 0) {
				dbg_avg->rssi_2ss_avg[0] = (u8)(dbg_statistic->rssi_2ss_sum[0] / dbg_statistic->rssi_2ss_cnt);
				dbg_avg->rssi_2ss_avg[1] = (u8)(dbg_statistic->rssi_2ss_sum[1] / dbg_statistic->rssi_2ss_cnt);

				dbg_avg->evm_2ss_avg[0] = (u8)(dbg_statistic->evm_2ss_sum[0] / dbg_statistic->rssi_2ss_cnt);
				dbg_avg->evm_2ss_avg[1] = (u8)(dbg_statistic->evm_2ss_sum[1] / dbg_statistic->rssi_2ss_cnt);

				dbg_avg->snr_2ss_avg[0] = (u8)(dbg_statistic->snr_2ss_sum[0] / dbg_statistic->rssi_2ss_cnt);
				dbg_avg->snr_2ss_avg[1] = (u8)(dbg_statistic->snr_2ss_sum[1] / dbg_statistic->rssi_2ss_cnt);
			}

			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * 2-ss Cnt= ((%d)) RSSI:{%d, %d}, EVM:{%d, %d}, SNR:{%d, %d}",
				   dbg_statistic->rssi_2ss_cnt, dbg_avg->rssi_2ss_avg[0],
				   dbg_avg->rssi_2ss_avg[1], dbg_avg->evm_2ss_avg[0],
				   dbg_avg->evm_2ss_avg[1], dbg_avg->snr_2ss_avg[0],
				   dbg_avg->snr_2ss_avg[1]);
			RT_PRINT(buf);
		}
#endif

#if (defined(PHYDM_COMPILE_ABOVE_3SS))
		if (dm->support_ic_type & (PHYDM_IC_ABOVE_3SS)) {
			if (dbg_statistic->rssi_3ss_cnt != 0) {
				dbg_avg->rssi_3ss_avg[0] = (u8)(dbg_statistic->rssi_3ss_sum[0] / dbg_statistic->rssi_3ss_cnt);
				dbg_avg->rssi_3ss_avg[1] = (u8)(dbg_statistic->rssi_3ss_sum[1] / dbg_statistic->rssi_3ss_cnt);
				dbg_avg->rssi_3ss_avg[2] = (u8)(dbg_statistic->rssi_3ss_sum[2] / dbg_statistic->rssi_3ss_cnt);

				dbg_avg->evm_3ss_avg[0] = (u8)(dbg_statistic->evm_3ss_sum[0] / dbg_statistic->rssi_3ss_cnt);
				dbg_avg->evm_3ss_avg[1] = (u8)(dbg_statistic->evm_3ss_sum[1] / dbg_statistic->rssi_3ss_cnt);
				dbg_avg->evm_3ss_avg[2] = (u8)(dbg_statistic->evm_3ss_sum[2] / dbg_statistic->rssi_3ss_cnt);

				dbg_avg->snr_3ss_avg[0] = (u8)(dbg_statistic->snr_3ss_sum[0] / dbg_statistic->rssi_3ss_cnt);
				dbg_avg->snr_3ss_avg[1] = (u8)(dbg_statistic->snr_3ss_sum[1] / dbg_statistic->rssi_3ss_cnt);
				dbg_avg->snr_3ss_avg[2] = (u8)(dbg_statistic->snr_3ss_sum[2] / dbg_statistic->rssi_3ss_cnt);
			}

			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * 3-ss Cnt= ((%d)) RSSI:{%d, %d, %d} EVM:{%d, %d, %d} SNR:{%d, %d, %d}",
				   dbg_statistic->rssi_3ss_cnt, dbg_avg->rssi_3ss_avg[0],
				   dbg_avg->rssi_3ss_avg[1], dbg_avg->rssi_3ss_avg[2],
				   dbg_avg->evm_3ss_avg[0], dbg_avg->evm_3ss_avg[1],
				   dbg_avg->evm_3ss_avg[2], dbg_avg->snr_3ss_avg[0],
				   dbg_avg->snr_3ss_avg[1], dbg_avg->snr_3ss_avg[2]);
			RT_PRINT(buf);
		}
#endif

#if (defined(PHYDM_COMPILE_ABOVE_4SS))
		if (dm->support_ic_type & PHYDM_IC_ABOVE_4SS) {
			if (dbg_statistic->rssi_4ss_cnt != 0) {
				dbg_avg->rssi_4ss_avg[0] = (u8)(dbg_statistic->rssi_4ss_sum[0] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->rssi_4ss_avg[1] = (u8)(dbg_statistic->rssi_4ss_sum[1] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->rssi_4ss_avg[2] = (u8)(dbg_statistic->rssi_4ss_sum[2] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->rssi_4ss_avg[3] = (u8)(dbg_statistic->rssi_4ss_sum[3] / dbg_statistic->rssi_4ss_cnt);

				dbg_avg->evm_4ss_avg[0] = (u8)(dbg_statistic->evm_4ss_sum[0] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->evm_4ss_avg[1] = (u8)(dbg_statistic->evm_4ss_sum[1] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->evm_4ss_avg[2] = (u8)(dbg_statistic->evm_4ss_sum[2] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->evm_4ss_avg[3] = (u8)(dbg_statistic->evm_4ss_sum[3] / dbg_statistic->rssi_4ss_cnt);

				dbg_avg->snr_4ss_avg[0] = (u8)(dbg_statistic->snr_4ss_sum[0] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->snr_4ss_avg[1] = (u8)(dbg_statistic->snr_4ss_sum[1] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->snr_4ss_avg[2] = (u8)(dbg_statistic->snr_4ss_sum[2] / dbg_statistic->rssi_4ss_cnt);
				dbg_avg->snr_4ss_avg[3] = (u8)(dbg_statistic->snr_4ss_sum[3] / dbg_statistic->rssi_4ss_cnt);
			}

			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n * 4-ss Cnt= ((%d)) RSSI:{%d, %d, %d, %d} EVM:{%d, %d, %d, %d} SNR:{%d, %d, %d, %d}",
				   dbg_statistic->rssi_4ss_cnt, dbg_avg->rssi_4ss_avg[0],
				   dbg_avg->rssi_4ss_avg[1], dbg_avg->rssi_4ss_avg[2],
				   dbg_avg->rssi_4ss_avg[3], dbg_avg->evm_4ss_avg[0],
				   dbg_avg->evm_4ss_avg[1], dbg_avg->evm_4ss_avg[2],
				   dbg_avg->evm_4ss_avg[3], dbg_avg->snr_4ss_avg[0],
				   dbg_avg->snr_4ss_avg[1], dbg_avg->snr_4ss_avg[2],
				   dbg_avg->snr_4ss_avg[3]);
			RT_PRINT(buf);
		}
#endif
		phydm_reset_phystatus_statistic(dm);
		/*----------------------------------------------------------*/

		/*Print TX rate*/
		for (macid = 0; macid < ODM_ASSOCIATE_ENTRY_NUM; macid++) {
			entry = dm->phydm_sta_info[macid];

			if (!is_sta_active(entry))
				continue;

			phydm_macid = (dm->phydm_macid_table[macid]);
			phydm_print_rate_2_buff(dm, entry->ra_info.curr_tx_rate, dbg_buf, PHYDM_SNPRINT_SIZE);
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n TxRate[%d]=%s (0x%x)", macid, dbg_buf, entry->ra_info.curr_tx_rate);
			RT_PRINT(buf);

			client_cnt++;

			if (client_cnt >= dm->number_linked_client)
				break;
		}

		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE,
			   "\r\n TP {Tx, Rx, Total} = {%d, %d, %d}Mbps, Traffic_Load=(%d))",
			   dm->tx_tp, dm->rx_tp, dm->total_tp, dm->traffic_load);
		RT_PRINT(buf);

		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n CFO_avg=((%d kHz)), CFO_traking = ((%s%d))",
			   cfo_t->CFO_ave_pre,
			   ((cfo_t->crystal_cap > cfo_t->def_x_cap) ? "+" : "-"),
			   DIFF_2(cfo_t->crystal_cap, cfo_t->def_x_cap));
		RT_PRINT(buf);

		/* Condition number */
		#if (RTL8822B_SUPPORT == 1)
		if (dm->support_ic_type == ODM_RTL8822B) {
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n Condi_Num=((%d.%.4d))",
				   dm->phy_dbg_info.condi_num >> 4,
				   phydm_show_fraction_num(dm->phy_dbg_info.condi_num & 0xf, 4));
			RT_PRINT(buf);
		}
		#endif

		#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
		/*STBC or LDPC pkt*/
		if (dm->support_ic_type & PHYSTS_2ND_TYPE_IC)
			RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n Coding: LDPC=((%s)), STBC=((%s))",
				   (dm->phy_dbg_info.is_ldpc_pkt) ? "Y" : "N",
				   (dm->phy_dbg_info.is_stbc_pkt) ? "Y" : "N");
			RT_PRINT(buf);
		#endif

	} else {
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n No Link !!!");
		RT_PRINT(buf);
	}

	RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n [CCA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}",
		   fa_t->cnt_cck_cca, fa_t->cnt_ofdm_cca, fa_t->cnt_cca_all);
	RT_PRINT(buf);

	RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE, "\r\n [FA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}",
		   fa_t->cnt_cck_fail, fa_t->cnt_ofdm_fail, fa_t->cnt_all);
	RT_PRINT(buf);

	#if (ODM_IC_11N_SERIES_SUPPORT == 1)
	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE,
			   "\r\n [OFDM FA Detail] Parity_Fail=%d, Rate_Illegal=%d, CRC8=%d, MCS_fail=%d, Fast_sync=%d, SB_Search_fail=%d",
			   fa_t->cnt_parity_fail, fa_t->cnt_rate_illegal,
			   fa_t->cnt_crc8_fail, fa_t->cnt_mcs_fail,
			   fa_t->cnt_fast_fsync, fa_t->cnt_sb_search_fail);
		RT_PRINT(buf);
	}
	#endif
	RT_SPRINTF(buf, DBGM_CLI_BUF_SIZE,
		   "\r\n is_linked = %d, Num_client = %d, rssi_min = %d, IGI = 0x%x, bNoisy=%d",
		   dm->is_linked, dm->number_linked_client, dm->rssi_min,
		   dm->dm_dig_table.cur_ig_value, dm->noisy_decision);
	RT_PRINT(buf);
}
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
void phydm_sbd_check(
	struct dm_struct *dm)
{
	static u32 pkt_cnt = 0;
	static boolean sbd_state = 0;
	u32 sym_count, count, value32;

	if (sbd_state == 0) {
		pkt_cnt++;
		/*read SBD conter once every 5 packets*/
		if (pkt_cnt % 5 == 0) {
			odm_set_timer(dm, &dm->sbdcnt_timer, 0); /*ms*/
			sbd_state = 1;
		}
	} else { /*read counter*/
		value32 = odm_get_bb_reg(dm, R_0xf98, MASKDWORD);
		sym_count = (value32 & 0x7C000000) >> 26;
		count = (value32 & 0x3F00000) >> 20;
		pr_debug("#SBD# sym_count %d count %d\n", sym_count, count);
		sbd_state = 0;
	}
}
#endif

void phydm_sbd_callback(
	struct phydm_timer_list *timer)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	void *adapter = timer->Adapter;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	struct dm_struct *dm = &hal_data->DM_OutSrc;

#if USE_WORKITEM
	odm_schedule_work_item(&dm->sbdcnt_workitem);
#else
	phydm_sbd_check(dm);
#endif
#endif
}

void phydm_sbd_workitem_callback(
	void *context)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	void *adapter = (void *)context;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA((PADAPTER)adapter);
	struct dm_struct *dm = &hal_data->DM_OutSrc;

	phydm_sbd_check(dm);
#endif
}
#endif

void phydm_reset_rx_rate_distribution(struct dm_struct *dm)
{
	struct odm_phy_dbg_info *dbg = &dm->phy_dbg_info;

	odm_memory_set(dm, &dbg->num_qry_legacy_pkt[0], 0,
		       (LEGACY_RATE_NUM * 2));
	odm_memory_set(dm, &dbg->num_qry_ht_pkt[0], 0,
		       (HT_RATE_NUM * 2));
	odm_memory_set(dm, &dbg->num_qry_pkt_sc_20m[0], 0,
		       (LOW_BW_RATE_NUM * 2));

	dbg->ht_pkt_not_zero = false;
	dbg->low_bw_20_occur = false;

#if ODM_IC_11AC_SERIES_SUPPORT
	odm_memory_set(dm, &dbg->num_qry_vht_pkt[0], 0,
		       (VHT_RATE_NUM * 2));
	odm_memory_set(dm, &dbg->num_qry_pkt_sc_40m[0], 0,
		       (LOW_BW_RATE_NUM * 2));

	dbg->vht_pkt_not_zero = false;
	dbg->low_bw_40_occur = false;
#endif
}

void phydm_rx_rate_distribution(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct odm_phy_dbg_info *dbg = &dm->phy_dbg_info;
	u8 i = 0;
	u8 rate_num = dm->num_rf_path, rate_ss_shift = 0;

	PHYDM_DBG(dm, DBG_CMN, "[RxRate Cnt] =============>\n");

	/*======CCK===========================================================*/
	if (*dm->channel <= 14) {
		PHYDM_DBG(dm, DBG_CMN, "* CCK = {%d, %d, %d, %d}\n",
			  dbg->num_qry_legacy_pkt[0],
			  dbg->num_qry_legacy_pkt[1],
			  dbg->num_qry_legacy_pkt[2],
			  dbg->num_qry_legacy_pkt[3]);
	}
	/*======OFDM==========================================================*/
	PHYDM_DBG(dm, DBG_CMN, "* OFDM = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
		  dbg->num_qry_legacy_pkt[4], dbg->num_qry_legacy_pkt[5],
		  dbg->num_qry_legacy_pkt[6], dbg->num_qry_legacy_pkt[7],
		  dbg->num_qry_legacy_pkt[8], dbg->num_qry_legacy_pkt[9],
		  dbg->num_qry_legacy_pkt[10], dbg->num_qry_legacy_pkt[11]);

	/*======HT============================================================*/
	if (dbg->ht_pkt_not_zero) {
		for (i = 0; i < rate_num; i++) {
			rate_ss_shift = (i << 3);

			PHYDM_DBG(dm, DBG_CMN,
				  "* HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
				  (rate_ss_shift), (rate_ss_shift + 7),
				  dbg->num_qry_ht_pkt[rate_ss_shift + 0],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 1],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 2],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 3],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 4],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 5],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 6],
				  dbg->num_qry_ht_pkt[rate_ss_shift + 7]);
		}

		if (dbg->low_bw_20_occur) {
			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = (i << 3);

				PHYDM_DBG(dm, DBG_CMN,
					  "* [Low BW 20M] HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
					  (rate_ss_shift), (rate_ss_shift + 7),
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 0],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 1],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 2],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 3],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 4],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 5],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 6],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 7]);
			}
		}
	}

#if ODM_IC_11AC_SERIES_SUPPORT
	/*======VHT=============================================================*/
	if (dbg->vht_pkt_not_zero) {
		for (i = 0; i < rate_num; i++) {
			rate_ss_shift = 10 * i;

			PHYDM_DBG(dm, DBG_CMN,
				  "* VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
				  (i + 1),
				  dbg->num_qry_vht_pkt[rate_ss_shift + 0],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 1],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 2],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 3],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 4],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 5],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 6],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 7],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 8],
				  dbg->num_qry_vht_pkt[rate_ss_shift + 9]);
		}

		if (dbg->low_bw_20_occur) {
			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = 10 * i;

				PHYDM_DBG(dm, DBG_CMN,
					  "*[Low BW 20M] VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
					  (i + 1),
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 0],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 1],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 2],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 3],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 4],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 5],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 6],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 7],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 8],
					  dbg->num_qry_pkt_sc_20m[rate_ss_shift
					  + 9]);
			}
		}

		if (dbg->low_bw_40_occur) {
			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = 10 * i;

				PHYDM_DBG(dm, DBG_CMN,
					  "*[Low BW 40M] VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
					  (i + 1),
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 0],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 1],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 2],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 3],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 4],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 5],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 6],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 7],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 8],
					  dbg->num_qry_pkt_sc_40m[rate_ss_shift
					  + 9]);
			}
		}
	}
#endif
}

void phydm_get_avg_phystatus_val(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_phystatus_statistic *dbg_statistic = &dm->phy_dbg_info.phystatus_statistic_info;
	struct phydm_phystatus_avg *dbg_avg = &dm->phy_dbg_info.phystatus_statistic_avg;

	PHYDM_DBG(dm, DBG_CMN, "[Avg PHY Statistic] ==============>\n");

	phydm_reset_phystatus_avg(dm);

	/*CCK*/
	dbg_avg->rssi_cck_avg = (u8)((dbg_statistic->rssi_cck_cnt != 0) ? (dbg_statistic->rssi_cck_sum / dbg_statistic->rssi_cck_cnt) : 0);
	PHYDM_DBG(dm, DBG_CMN, "* cck Cnt= ((%d)) RSSI:{%d}\n",
		  dbg_statistic->rssi_cck_cnt, dbg_avg->rssi_cck_avg);

	/*OFDM*/
	if (dbg_statistic->rssi_ofdm_cnt != 0) {
		dbg_avg->rssi_ofdm_avg = (u8)(dbg_statistic->rssi_ofdm_sum / dbg_statistic->rssi_ofdm_cnt);
		dbg_avg->evm_ofdm_avg = (u8)(dbg_statistic->evm_ofdm_sum / dbg_statistic->rssi_ofdm_cnt);
		dbg_avg->snr_ofdm_avg = (u8)(dbg_statistic->snr_ofdm_sum / dbg_statistic->rssi_ofdm_cnt);
	}

	PHYDM_DBG(dm, DBG_CMN,
		  "* ofdm Cnt= ((%d)) RSSI:{%d} EVM:{%d} SNR:{%d}\n",
		  dbg_statistic->rssi_ofdm_cnt, dbg_avg->rssi_ofdm_avg,
		  dbg_avg->evm_ofdm_avg, dbg_avg->snr_ofdm_avg);

	if (dbg_statistic->rssi_1ss_cnt != 0) {
		dbg_avg->rssi_1ss_avg = (u8)(dbg_statistic->rssi_1ss_sum / dbg_statistic->rssi_1ss_cnt);
		dbg_avg->evm_1ss_avg = (u8)(dbg_statistic->evm_1ss_sum / dbg_statistic->rssi_1ss_cnt);
		dbg_avg->snr_1ss_avg = (u8)(dbg_statistic->snr_1ss_sum / dbg_statistic->rssi_1ss_cnt);
	}

	PHYDM_DBG(dm, DBG_CMN,
		  "* 1-ss Cnt= ((%d)) RSSI:{%d} EVM:{%d} SNR:{%d}\n",
		  dbg_statistic->rssi_1ss_cnt, dbg_avg->rssi_1ss_avg,
		  dbg_avg->evm_1ss_avg, dbg_avg->snr_1ss_avg);

#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	if (dm->support_ic_type & (PHYDM_IC_ABOVE_2SS)) {
		if (dbg_statistic->rssi_2ss_cnt != 0) {
			dbg_avg->rssi_2ss_avg[0] = (u8)(dbg_statistic->rssi_2ss_sum[0] / dbg_statistic->rssi_2ss_cnt);
			dbg_avg->rssi_2ss_avg[1] = (u8)(dbg_statistic->rssi_2ss_sum[1] / dbg_statistic->rssi_2ss_cnt);

			dbg_avg->evm_2ss_avg[0] = (u8)(dbg_statistic->evm_2ss_sum[0] / dbg_statistic->rssi_2ss_cnt);
			dbg_avg->evm_2ss_avg[1] = (u8)(dbg_statistic->evm_2ss_sum[1] / dbg_statistic->rssi_2ss_cnt);

			dbg_avg->snr_2ss_avg[0] = (u8)(dbg_statistic->snr_2ss_sum[0] / dbg_statistic->rssi_2ss_cnt);
			dbg_avg->snr_2ss_avg[1] = (u8)(dbg_statistic->snr_2ss_sum[1] / dbg_statistic->rssi_2ss_cnt);
		}

		PHYDM_DBG(dm, DBG_CMN,
			  "* 2-ss Cnt= ((%d)) RSSI:{%d, %d}, EVM:{%d, %d}, SNR:{%d, %d}\n",
			  dbg_statistic->rssi_2ss_cnt, dbg_avg->rssi_2ss_avg[0],
			  dbg_avg->rssi_2ss_avg[1], dbg_avg->evm_2ss_avg[0],
			  dbg_avg->evm_2ss_avg[1], dbg_avg->snr_2ss_avg[0],
			  dbg_avg->snr_2ss_avg[1]);
	}
#endif

#if (defined(PHYDM_COMPILE_ABOVE_3SS))
	if (dm->support_ic_type & (PHYDM_IC_ABOVE_3SS)) {
		if (dbg_statistic->rssi_3ss_cnt != 0) {
			dbg_avg->rssi_3ss_avg[0] = (u8)(dbg_statistic->rssi_3ss_sum[0] / dbg_statistic->rssi_3ss_cnt);
			dbg_avg->rssi_3ss_avg[1] = (u8)(dbg_statistic->rssi_3ss_sum[1] / dbg_statistic->rssi_3ss_cnt);
			dbg_avg->rssi_3ss_avg[2] = (u8)(dbg_statistic->rssi_3ss_sum[2] / dbg_statistic->rssi_3ss_cnt);

			dbg_avg->evm_3ss_avg[0] = (u8)(dbg_statistic->evm_3ss_sum[0] / dbg_statistic->rssi_3ss_cnt);
			dbg_avg->evm_3ss_avg[1] = (u8)(dbg_statistic->evm_3ss_sum[1] / dbg_statistic->rssi_3ss_cnt);
			dbg_avg->evm_3ss_avg[2] = (u8)(dbg_statistic->evm_3ss_sum[2] / dbg_statistic->rssi_3ss_cnt);

			dbg_avg->snr_3ss_avg[0] = (u8)(dbg_statistic->snr_3ss_sum[0] / dbg_statistic->rssi_3ss_cnt);
			dbg_avg->snr_3ss_avg[1] = (u8)(dbg_statistic->snr_3ss_sum[1] / dbg_statistic->rssi_3ss_cnt);
			dbg_avg->snr_3ss_avg[2] = (u8)(dbg_statistic->snr_3ss_sum[2] / dbg_statistic->rssi_3ss_cnt);
		}

		PHYDM_DBG(dm, DBG_CMN,
			  "* 3-ss Cnt= ((%d)) RSSI:{%d, %d, %d} EVM:{%d, %d, %d} SNR:{%d, %d, %d}\n",
			  dbg_statistic->rssi_3ss_cnt, dbg_avg->rssi_3ss_avg[0],
			  dbg_avg->rssi_3ss_avg[1], dbg_avg->rssi_3ss_avg[2],
			  dbg_avg->evm_3ss_avg[0], dbg_avg->evm_3ss_avg[1],
			  dbg_avg->evm_3ss_avg[2], dbg_avg->snr_3ss_avg[0],
			  dbg_avg->snr_3ss_avg[1], dbg_avg->snr_3ss_avg[2]);
	}
#endif

#if (defined(PHYDM_COMPILE_ABOVE_4SS))
	if (dm->support_ic_type & PHYDM_IC_ABOVE_4SS) {
		if (dbg_statistic->rssi_4ss_cnt != 0) {
			dbg_avg->rssi_4ss_avg[0] = (u8)(dbg_statistic->rssi_4ss_sum[0] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->rssi_4ss_avg[1] = (u8)(dbg_statistic->rssi_4ss_sum[1] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->rssi_4ss_avg[2] = (u8)(dbg_statistic->rssi_4ss_sum[2] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->rssi_4ss_avg[3] = (u8)(dbg_statistic->rssi_4ss_sum[3] / dbg_statistic->rssi_4ss_cnt);

			dbg_avg->evm_4ss_avg[0] = (u8)(dbg_statistic->evm_4ss_sum[0] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->evm_4ss_avg[1] = (u8)(dbg_statistic->evm_4ss_sum[1] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->evm_4ss_avg[2] = (u8)(dbg_statistic->evm_4ss_sum[2] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->evm_4ss_avg[3] = (u8)(dbg_statistic->evm_4ss_sum[3] / dbg_statistic->rssi_4ss_cnt);

			dbg_avg->snr_4ss_avg[0] = (u8)(dbg_statistic->snr_4ss_sum[0] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->snr_4ss_avg[1] = (u8)(dbg_statistic->snr_4ss_sum[1] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->snr_4ss_avg[2] = (u8)(dbg_statistic->snr_4ss_sum[2] / dbg_statistic->rssi_4ss_cnt);
			dbg_avg->snr_4ss_avg[3] = (u8)(dbg_statistic->snr_4ss_sum[3] / dbg_statistic->rssi_4ss_cnt);
		}

		PHYDM_DBG(dm, DBG_CMN,
			  "* 4-ss Cnt= ((%d)) RSSI:{%d, %d, %d, %d} EVM:{%d, %d, %d, %d} SNR:{%d, %d, %d, %d}\n",
			  dbg_statistic->rssi_4ss_cnt, dbg_avg->rssi_4ss_avg[0],
			  dbg_avg->rssi_4ss_avg[1], dbg_avg->rssi_4ss_avg[2],
			  dbg_avg->rssi_4ss_avg[3], dbg_avg->evm_4ss_avg[0],
			  dbg_avg->evm_4ss_avg[1], dbg_avg->evm_4ss_avg[2],
			  dbg_avg->evm_4ss_avg[3], dbg_avg->snr_4ss_avg[0],
			  dbg_avg->snr_4ss_avg[1], dbg_avg->snr_4ss_avg[2],
			  dbg_avg->snr_4ss_avg[3]);
	}
#endif
}

void phydm_get_phy_statistic(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	phydm_rx_rate_distribution(dm);
	phydm_reset_rx_rate_distribution(dm);

	phydm_get_avg_phystatus_val(dm);
	phydm_reset_phystatus_statistic(dm);
};

void phydm_basic_dbg_msg_linked(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_t = &dm->dm_cfo_track;
	struct odm_phy_dbg_info *dbg_t = &dm->phy_dbg_info;
	u16 macid, phydm_macid, client_cnt = 0;
	u8 rate = 0;
	struct cmn_sta_info *entry = NULL;
	char dbg_buf[PHYDM_SNPRINT_SIZE] = {0};

	PHYDM_DBG(dm, DBG_CMN, "ID=((%d)), BW=((%d)), fc=((CH-%d))\n",
		  dm->curr_station_id, 20 << *dm->band_width, *dm->channel);

	#ifdef ODM_IC_11N_SERIES_SUPPORT
	if (((*dm->channel <= 14) && (*dm->band_width == CHANNEL_WIDTH_40)) &&
	    (dm->support_ic_type & ODM_IC_11N_SERIES)) {
		PHYDM_DBG(dm, DBG_CMN, "Primary CCA at ((%s SB))\n",
			  ((*dm->sec_ch_offset == SECOND_CH_AT_LSB) ? "U" :
			  "L"));
	}
	#endif

	if ((dm->support_ic_type & PHYSTS_2ND_TYPE_IC) ||
	    dm->rx_rate > ODM_RATE11M) {
		PHYDM_DBG(dm, DBG_CMN, "[AGC Idx] {0x%x, 0x%x, 0x%x, 0x%x}\n",
			  dm->ofdm_agc_idx[0], dm->ofdm_agc_idx[1],
			  dm->ofdm_agc_idx[2], dm->ofdm_agc_idx[3]);
	} else {
		PHYDM_DBG(dm, DBG_CMN, "[CCK AGC Idx] {LNA,VGA}={0x%x, 0x%x}\n",
			  dm->cck_lna_idx, dm->cck_vga_idx);
	}

	phydm_print_rate_2_buff(dm, dm->rx_rate, dbg_buf, PHYDM_SNPRINT_SIZE);
	PHYDM_DBG(dm, DBG_CMN, "RSSI:{%d, %d, %d, %d}, RxRate:%s (0x%x)\n",
		  (dm->rssi_a == 0xff) ? 0 : dm->rssi_a,
		  (dm->rssi_b == 0xff) ? 0 : dm->rssi_b,
		  (dm->rssi_c == 0xff) ? 0 : dm->rssi_c,
		  (dm->rssi_d == 0xff) ? 0 : dm->rssi_d,
		  dbg_buf, dm->rx_rate);

	rate = dbg_t->beacon_phy_rate;
	phydm_print_rate_2_buff(dm, rate, dbg_buf, PHYDM_SNPRINT_SIZE);

	PHYDM_DBG(dm, DBG_CMN, "Beacon_cnt=%d, rate_idx=%s (0x%x)\n",
		  dbg_t->num_qry_beacon_pkt,
		  dbg_buf,
		  dbg_t->beacon_phy_rate);

	phydm_get_phy_statistic(dm);

	/*Print TX rate*/
	for (macid = 0; macid < ODM_ASSOCIATE_ENTRY_NUM; macid++) {
		entry = dm->phydm_sta_info[macid];

		if (!is_sta_active(entry))
			continue;

		phydm_macid = (dm->phydm_macid_table[macid]);
		rate = entry->ra_info.curr_tx_rate;
		phydm_print_rate_2_buff(dm, rate, dbg_buf, PHYDM_SNPRINT_SIZE);
		PHYDM_DBG(dm, DBG_CMN, "TxRate[%d]=%s (0x%x)\n",
			  macid, dbg_buf, entry->ra_info.curr_tx_rate);

		client_cnt++;

		if (client_cnt >= dm->number_linked_client)
			break;
	}

	PHYDM_DBG(dm, DBG_CMN,
		  "TP {Tx, Rx, Total} = {%d, %d, %d}Mbps, Traffic_Load=(%d))\n",
		  dm->tx_tp, dm->rx_tp, dm->total_tp, dm->traffic_load);

	PHYDM_DBG(dm, DBG_CMN, "CFO_avg=((%d kHz)), CFO_traking = ((%s%d))\n",
		  cfo_t->CFO_ave_pre,
		  ((cfo_t->crystal_cap > cfo_t->def_x_cap) ? "+" : "-"),
		  DIFF_2(cfo_t->crystal_cap, cfo_t->def_x_cap));

/* Condition number */
#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type == ODM_RTL8822B) {
		PHYDM_DBG(dm, DBG_CMN, "Condi_Num=((%d.%.4d)), %d\n",
			  dbg_t->condi_num >> 4,
			  phydm_show_fraction_num(dbg_t->condi_num & 0xf, 4), dbg_t->condi_num);
	}
#endif

#if (ODM_PHY_STATUS_NEW_TYPE_SUPPORT == 1)
	/*STBC or LDPC pkt*/
	if (dm->support_ic_type & PHYSTS_2ND_TYPE_IC)
		PHYDM_DBG(dm, DBG_CMN, "Coding: LDPC=((%s)), STBC=((%s))\n",
			  (dbg_t->is_ldpc_pkt) ? "Y" : "N",
			  (dbg_t->is_stbc_pkt) ? "Y" : "N");
#endif
}

void phydm_basic_dbg_message(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_fa_struct *fa_t = &dm->false_alm_cnt;

	/*if (!(dm->debug_components & DBG_CMN))*/
	/*	return;				*/

	if (dm->cmn_dbg_msg_cnt >= dm->cmn_dbg_msg_period) {
		dm->cmn_dbg_msg_cnt = PHYDM_WATCH_DOG_PERIOD;
	} else {
		dm->cmn_dbg_msg_cnt += PHYDM_WATCH_DOG_PERIOD;
		return;
	}

	PHYDM_DBG(dm, DBG_CMN, "[%s] System up time: ((%d sec))---->\n",
		  __func__, dm->phydm_sys_up_time);

	if (dm->is_linked)
		phydm_basic_dbg_msg_linked(dm);
	else
		PHYDM_DBG(dm, DBG_CMN, "No Link !!!\n");

	PHYDM_DBG(dm, DBG_CMN, "[CCA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}\n",
		  fa_t->cnt_cck_cca, fa_t->cnt_ofdm_cca, fa_t->cnt_cca_all);

	PHYDM_DBG(dm, DBG_CMN, "[FA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}\n",
		  fa_t->cnt_cck_fail, fa_t->cnt_ofdm_fail, fa_t->cnt_all);

	#if (ODM_IC_11N_SERIES_SUPPORT == 1)
	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		PHYDM_DBG(dm, DBG_CMN,
			  "[OFDM FA Detail] Parity_Fail=%d, Rate_Illegal=%d, CRC8=%d, MCS_fail=%d, Fast_sync=%d, SB_Search_fail=%d\n",
			  fa_t->cnt_parity_fail, fa_t->cnt_rate_illegal,
			  fa_t->cnt_crc8_fail, fa_t->cnt_mcs_fail,
			  fa_t->cnt_fast_fsync, fa_t->cnt_sb_search_fail);
	}
	#endif
	PHYDM_DBG(dm, DBG_CMN,
		  "is_linked = %d, Num_client = %d, rssi_min = %d, IGI = 0x%x, bNoisy=%d\n\n",
		  dm->is_linked, dm->number_linked_client, dm->rssi_min,
		  dm->dm_dig_table.cur_ig_value, dm->noisy_decision);
}

void phydm_basic_profile(void *dm_void, u32 *_used, char *output, u32 *_out_len)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	char *cut = NULL;
	char *ic_type = NULL;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 date = 0;
	char *commit_by = NULL;
	u32 release_ver = 0;

	PDM_SNPF(out_len, used, output + used, out_len - used, "%-35s\n",
		 "% Basic Profile %");

	if (dm->support_ic_type == ODM_RTL8188E) {
#if (RTL8188E_SUPPORT == 1)
		ic_type = "RTL8188E";
		date = RELEASE_DATE_8188E;
		commit_by = COMMIT_BY_8188E;
		release_ver = RELEASE_VERSION_8188E;
#endif
#if (RTL8812A_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8812) {
		ic_type = "RTL8812A";
		date = RELEASE_DATE_8812A;
		commit_by = COMMIT_BY_8812A;
		release_ver = RELEASE_VERSION_8812A;
#endif
#if (RTL8821A_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8821) {
		ic_type = "RTL8821A";
		date = RELEASE_DATE_8821A;
		commit_by = COMMIT_BY_8821A;
		release_ver = RELEASE_VERSION_8821A;
#endif
#if (RTL8192E_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8192E) {
		ic_type = "RTL8192E";
		date = RELEASE_DATE_8192E;
		commit_by = COMMIT_BY_8192E;
		release_ver = RELEASE_VERSION_8192E;
#endif
#if (RTL8723B_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8723B) {
		ic_type = "RTL8723B";
		date = RELEASE_DATE_8723B;
		commit_by = COMMIT_BY_8723B;
		release_ver = RELEASE_VERSION_8723B;
#endif
#if (RTL8814A_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8814A) {
		ic_type = "RTL8814A";
		date = RELEASE_DATE_8814A;
		commit_by = COMMIT_BY_8814A;
		release_ver = RELEASE_VERSION_8814A;
#endif
#if (RTL8881A_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8881A) {
		ic_type = "RTL8881A";
		/**/
#endif
#if (RTL8822B_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8822B) {
		ic_type = "RTL8822B";
		date = RELEASE_DATE_8822B;
		commit_by = COMMIT_BY_8822B;
		release_ver = RELEASE_VERSION_8822B;
#endif
#if (RTL8197F_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8197F) {
		ic_type = "RTL8197F";
		date = RELEASE_DATE_8197F;
		commit_by = COMMIT_BY_8197F;
		release_ver = RELEASE_VERSION_8197F;
#endif
#if (RTL8703B_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8703B) {
		ic_type = "RTL8703B";
		date = RELEASE_DATE_8703B;
		commit_by = COMMIT_BY_8703B;
		release_ver = RELEASE_VERSION_8703B;
#endif
#if (RTL8195A_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8195A) {
		ic_type = "RTL8195A";
		/**/
#endif
#if (RTL8188F_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8188F) {
		ic_type = "RTL8188F";
		date = RELEASE_DATE_8188F;
		commit_by = COMMIT_BY_8188F;
		release_ver = RELEASE_VERSION_8188F;
#endif
#if (RTL8723D_SUPPORT == 1)
	} else if (dm->support_ic_type == ODM_RTL8723D) {
		ic_type = "RTL8723D";
		date = RELEASE_DATE_8723D;
		commit_by = COMMIT_BY_8723D;
		release_ver = RELEASE_VERSION_8723D;
		/**/
#endif
	}

/* JJ ADD 20161014 */
#if (RTL8710B_SUPPORT == 1)
	else if (dm->support_ic_type == ODM_RTL8710B) {
		ic_type = "RTL8710B";
		date = RELEASE_DATE_8710B;
		commit_by = COMMIT_BY_8710B;
		release_ver = RELEASE_VERSION_8710B;
		/**/
	}
#endif

#if (RTL8821C_SUPPORT == 1)
	else if (dm->support_ic_type == ODM_RTL8821C) {
		ic_type = "RTL8821C";
		date = RELEASE_DATE_8821C;
		commit_by = COMMIT_BY_8821C;
		release_ver = RELEASE_VERSION_8821C;
	}
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	else if (dm->support_ic_type == ODM_RTL8192F) {
		ic_type = "RTL8192F";
		date = RELEASE_DATE_8192F;
		commit_by = COMMIT_BY_8192F;
		release_ver = RELEASE_VERSION_8192F;
	}
#endif

#if (RTL8198F_SUPPORT == 1)
	else if (dm->support_ic_type == ODM_RTL8198F) {
		ic_type = "RTL8198F";
		date = RELEASE_DATE_8198F;
		commit_by = COMMIT_BY_8198F;
		release_ver = RELEASE_VERSION_8198F;
	}
#endif

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "  %-35s: %s (MP Chip: %s)\n", "IC type", ic_type,
		 dm->is_mp_chip ? "Yes" : "No");

	if (dm->cut_version == ODM_CUT_A)
		cut = "A";
	else if (dm->cut_version == ODM_CUT_B)
		cut = "B";
	else if (dm->cut_version == ODM_CUT_C)
		cut = "C";
	else if (dm->cut_version == ODM_CUT_D)
		cut = "D";
	else if (dm->cut_version == ODM_CUT_E)
		cut = "E";
	else if (dm->cut_version == ODM_CUT_F)
		cut = "F";
	else if (dm->cut_version == ODM_CUT_I)
		cut = "I";

	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %d\n",
		 "RFE type", dm->rfe_type);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Cut Ver", cut);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %d\n",
		 "PHY Para Ver", odm_get_hw_img_version(dm));
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %d\n",
		 "PHY Para Commit date", date);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "PHY Para Commit by", commit_by);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %d\n",
		 "PHY Para Release Ver", release_ver);

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "  %-35s: %d (Subversion: %d)\n", "FW Ver", dm->fw_version,
		 dm->fw_sub_version);

	/* 1 PHY DM version List */
	PDM_SNPF(out_len, used, output + used, out_len - used, "%-35s\n",
		 "% PHYDM version %");
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Code base", PHYDM_CODE_BASE);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Release Date", PHYDM_RELEASE_DATE);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Adaptivity", ADAPTIVITY_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "DIG", DIG_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "CFO Tracking", CFO_TRACKING_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "AntDiv", ANTDIV_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Dynamic TxPower", DYNAMIC_TXPWR_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "RA Info", RAINFO_VERSION);
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "AntDetect", ANTDECT_VERSION);
#endif
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "PathDiv", PATHDIV_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Adaptive SOML", ADAPTIVE_SOML_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "LA mode", DYNAMIC_LA_MODE);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "Primary CCA", PRIMARYCCA_VERSION);
	PDM_SNPF(out_len, used, output + used, out_len - used, "  %-35s: %s\n",
		 "DFS", DFS_VERSION);

#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8822B)
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "  %-35s: %s\n", "PHY config 8822B",
			 PHY_CONFIG_VERSION_8822B);

#endif
#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F)
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "  %-35s: %s\n", "PHY config 8197F",
			 PHY_CONFIG_VERSION_8197F);
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192F)
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "  %-35s: %s\n", "PHY config 8192F",
			 PHY_CONFIG_VERSION_8192F);
#endif

	*_used = used;
	*_out_len = out_len;

#endif /*#if CONFIG_PHYDM_DEBUG_FUNCTION*/
}

#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
void phydm_fw_trace_en_h2c(void *dm_void, boolean enable,
			   u32 fw_debug_component, u32 monitor_mode, u32 macid)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 h2c_parameter[7] = {0};
	u8 cmd_length;

	if (dm->support_ic_type & PHYDM_IC_3081_SERIES) {
		h2c_parameter[0] = enable;
		h2c_parameter[1] = (u8)(fw_debug_component & MASKBYTE0);
		h2c_parameter[2] = (u8)((fw_debug_component & MASKBYTE1) >> 8);
		h2c_parameter[3] = (u8)((fw_debug_component & MASKBYTE2) >> 16);
		h2c_parameter[4] = (u8)((fw_debug_component & MASKBYTE3) >> 24);
		h2c_parameter[5] = (u8)monitor_mode;
		h2c_parameter[6] = (u8)macid;
		cmd_length = 7;

	} else {
		h2c_parameter[0] = enable;
		h2c_parameter[1] = (u8)monitor_mode;
		h2c_parameter[2] = (u8)macid;
		cmd_length = 3;
	}

	PHYDM_DBG(dm, DBG_FW_TRACE, "---->\n");
	if (monitor_mode == 0)
		PHYDM_DBG(dm, DBG_FW_TRACE, "[H2C] FW_debug_en: (( %d ))\n",
			  enable);
	else
		PHYDM_DBG(dm, DBG_FW_TRACE,
			  "[H2C] FW_debug_en: (( %d )), mode: (( %d )), macid: (( %d ))\n",
			  enable, monitor_mode, macid);
	odm_fill_h2c_cmd(dm, PHYDM_H2C_FW_TRACE_EN, cmd_length, h2c_parameter);
}

void phydm_get_per_path_txagc(void *dm_void, u8 path, u32 *_used, char *output,
			      u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 rate_idx;
	u8 txagc;
	u32 used = *_used;
	u32 out_len = *_out_len;

#ifdef PHYDM_COMMON_API_SUPPORT
	if (((dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8192F)) && path <= RF_PATH_B) || /*jj add 20170822*/
	    ((dm->support_ic_type & (ODM_RTL8821C | ODM_RTL8195B)) && path <= RF_PATH_A)) {
		for (rate_idx = 0; rate_idx <= 0x53; rate_idx++) {
			if (rate_idx == ODM_RATE1M)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "  %-35s\n",
					 "CCK====>");
			else if (rate_idx == ODM_RATE6M)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "OFDM====>");
			else if (rate_idx == ODM_RATEMCS0)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "HT 1ss====>");
			else if (rate_idx == ODM_RATEMCS8)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "HT 2ss====>");
			else if (rate_idx == ODM_RATEMCS16)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "HT 3ss====>");
			else if (rate_idx == ODM_RATEMCS24)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "HT 4ss====>");
			else if (rate_idx == ODM_RATEVHTSS1MCS0)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "VHT 1ss====>");
			else if (rate_idx == ODM_RATEVHTSS2MCS0)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "VHT 2ss====>");
			else if (rate_idx == ODM_RATEVHTSS3MCS0)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "VHT 3ss====>");
			else if (rate_idx == ODM_RATEVHTSS4MCS0)
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "\n  %-35s\n",
					 "VHT 4ss====>");

			txagc = phydm_api_get_txagc(dm, (enum rf_path)path, rate_idx);
			if (config_phydm_read_txagc_check(txagc))
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "  0x%02x    ", txagc);
			else
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "  0x%s    ", "xx");
		}
	}
#endif

	*_used = used;
	*_out_len = out_len;
}

void phydm_get_txagc(void *dm_void, u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;

	/* path-A */
	PDM_SNPF(out_len, used, output + used, out_len - used, "%-35s\n",
		 "path-A====================");
	phydm_get_per_path_txagc(dm, RF_PATH_A, &used, output, &out_len);

	/* path-B */
	PDM_SNPF(out_len, used, output + used, out_len - used, "\n%-35s\n",
		 "path-B====================");
	phydm_get_per_path_txagc(dm, RF_PATH_B, &used, output, &out_len);

	/* path-C */
	PDM_SNPF(out_len, used, output + used, out_len - used, "\n%-35s\n",
		 "path-C====================");
	phydm_get_per_path_txagc(dm, RF_PATH_C, &used, output, &out_len);

	/* path-D */
	PDM_SNPF(out_len, used, output + used, out_len - used, "\n%-35s\n",
		 "path-D====================");
	phydm_get_per_path_txagc(dm, RF_PATH_D, &used, output, &out_len);

	*_used = used;
	*_out_len = out_len;
}

void phydm_set_txagc(void *dm_void, u32 *const dm_value, u32 *_used,
		     char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;
	u32 power_index;
	boolean status = true;

/*dm_value[1] = path*/
/*dm_value[2] = hw_rate*/
/*dm_value[3] = power_index*/

#ifdef PHYDM_COMMON_API_SUPPORT
	if ((dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8821C | ODM_RTL8192F | ODM_RTL8195B)) == 0) /*jj add 20170822*/
		return;

	if (dm_value[1] >= dm->num_rf_path) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Write path-%d rate_idx-0x%x fail\n", dm_value[1],
			 dm_value[2]);
	} else if ((u8)dm_value[2] != 0xff) {
		if (phydm_api_set_txagc(dm, dm_value[3], (enum rf_path)dm_value[1], (u8)dm_value[2], true))
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Write path-%d rate_idx-0x%x = 0x%x\n",
				 dm_value[1], dm_value[2], dm_value[3]);
		else
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Write path-%d rate index-0x%x fail\n",
				 dm_value[1], dm_value[2]);
	} else {
		power_index = (dm_value[3] & 0x3f);

		if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C | ODM_RTL8195B)) {
			power_index = (power_index << 24) | (power_index << 16) | (power_index << 8) | (power_index);

			for (i = 0; i < ODM_RATEVHTSS2MCS9; i += 4)
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, false);
		} else if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F)) { /*jj add 20170822*/
			for (i = 0; i <= ODM_RATEMCS15; i++)
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, false);
		}

		if (status)
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Write all TXAGC of path-%d = 0x%x\n",
				 dm_value[1], dm_value[3]);
		else
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Write all TXAGC of path-%d fail\n",
				 dm_value[1]);
	}

#endif
	*_used = used;
	*_out_len = out_len;
}

void phydm_shift_txagc(void *dm_void, u32 *const dm_value, u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;
	u32 power_index;
	u32 power_offset;
	boolean status = true;

/*dm_value[1] = path*/
/*dm_value[2] = hw_rate*/
/*dm_value[3] = power_index*/

#ifdef PHYDM_COMMON_API_SUPPORT
	if ((dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8821C |
				   ODM_RTL8192F | ODM_RTL8195B)) == 0)/*jj add 20170822*/
		return;

	if (dm_value[1] >= dm->num_rf_path) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Write path-%d fail\n", dm_value[1]);
		return;
	}

	if ((u8)dm_value[2] == 0) {  /*{0:-, 1:+} {Pwr Offset}*/
		if (dm->support_ic_type & (ODM_RTL8195B | ODM_RTL8821C)) {
			for (i = 0; i <= ODM_RATEMCS7; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) - dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
			for (i = ODM_RATEVHTSS1MCS0; i <= ODM_RATEVHTSS1MCS9; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) - dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
		} else if (dm->support_ic_type & (ODM_RTL8822B)) {
			for (i = 0; i <= ODM_RATEMCS15; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) - dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
			for (i = ODM_RATEVHTSS1MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) - dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
		} else if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F)) {
			for (i = 0; i <= ODM_RATEMCS15; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) - dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
		}
	} else if ((u8)dm_value[2] == 1) {  /*{0:-, 1:+} {Pwr Offset}*/
		if (dm->support_ic_type & (ODM_RTL8195B | ODM_RTL8821C)) {
			for (i = 0; i <= ODM_RATEMCS7; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) + dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
			for (i = ODM_RATEVHTSS1MCS0; i <= ODM_RATEVHTSS1MCS9; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) + dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
		} else if (dm->support_ic_type & (ODM_RTL8822B)) {
			for (i = 0; i <= ODM_RATEMCS15; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) + dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
			for (i = ODM_RATEVHTSS1MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) + dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
		} else if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F)) {
			for (i = 0; i <= ODM_RATEMCS15; i++) {
				power_index = phydm_api_get_txagc(dm, (enum rf_path)dm_value[1], i) + dm_value[3];
				status &= phydm_api_set_txagc(dm, power_index, (enum rf_path)dm_value[1], i, true);
			}
		}
	}
#endif
	*_used = used;
	*_out_len = out_len;
}

void phydm_debug_trace(void *dm_void, char input[][16], u32 *_used,
		       char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u64 pre_debug_components, one = 1;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 dm_value[10] = {0};
	u8 i;

	for (i = 0; i < 5; i++) {
		if (input[i + 1])
			PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &dm_value[i]);
	}

	pre_debug_components = dm->debug_components;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "\n================================\n");
	if (dm_value[0] == 100) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "[DBG MSG] Component Selection\n");
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "================================\n");
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "00. (( %s ))DIG\n",
			 ((dm->debug_components & DBG_DIG) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "01. (( %s ))RA_MASK\n",
			 ((dm->debug_components & DBG_RA_MASK) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "02. (( %s ))DYN_TXPWR\n",
			 ((dm->debug_components & DBG_DYN_TXPWR) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "03. (( %s ))FA_CNT\n",
			 ((dm->debug_components & DBG_FA_CNT) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "04. (( %s ))RSSI_MNTR\n",
			 ((dm->debug_components & DBG_RSSI_MNTR) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "05. (( %s ))CCKPD\n",
			 ((dm->debug_components & DBG_CCKPD) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "06. (( %s ))ANT_DIV\n",
			 ((dm->debug_components & DBG_ANT_DIV) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "07. (( %s ))SMT_ANT\n",
			 ((dm->debug_components & DBG_SMT_ANT) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "08. (( %s ))PWR_TRAIN\n",
			 ((dm->debug_components & DBG_PWR_TRAIN) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "09. (( %s ))RA\n",
			 ((dm->debug_components & DBG_RA) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "10. (( %s ))PATH_DIV\n",
			 ((dm->debug_components & DBG_PATH_DIV) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "11. (( %s ))DFS\n",
			 ((dm->debug_components & DBG_DFS) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "12. (( %s ))DYN_ARFR\n",
			 ((dm->debug_components & DBG_DYN_ARFR) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "13. (( %s ))ADAPTIVITY\n",
			 ((dm->debug_components & DBG_ADPTVTY) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "14. (( %s ))CFO_TRK\n",
			 ((dm->debug_components & DBG_CFO_TRK) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "15. (( %s ))ENV_MNTR\n",
			 ((dm->debug_components & DBG_ENV_MNTR) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "16. (( %s ))PRI_CCA\n",
			 ((dm->debug_components & DBG_PRI_CCA) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "17. (( %s ))ADPTV_SOML\n",
			 ((dm->debug_components & DBG_ADPTV_SOML) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "18. (( %s ))LNA_SAT_CHK\n",
			 ((dm->debug_components & DBG_LNA_SAT_CHK) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "20. (( %s ))PHY_STATUS\n",
			 ((dm->debug_components & DBG_PHY_STATUS) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "21. (( %s ))TMP\n",
			 ((dm->debug_components & DBG_TMP) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "22. (( %s ))FW_DBG_TRACE\n",
			 ((dm->debug_components & DBG_FW_TRACE) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "23. (( %s ))TXBF\n",
			 ((dm->debug_components & DBG_TXBF) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "24. (( %s ))COMMON_FLOW\n",
			 ((dm->debug_components & DBG_COMMON_FLOW) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "28. (( %s ))PHY_CONFIG\n",
			 ((dm->debug_components & ODM_PHY_CONFIG) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "29. (( %s ))INIT\n",
			 ((dm->debug_components & ODM_COMP_INIT) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "30. (( %s ))COMMON\n",
			 ((dm->debug_components & DBG_CMN) ? ("V") : (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "31. (( %s ))API\n",
			 ((dm->debug_components & ODM_COMP_API) ? ("V") :
			 (".")));
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "================================\n");

	} else if (dm_value[0] == 101) {
		dm->debug_components = 0;
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Disable all debug components\n");
	} else {
		if (dm_value[1] == 1) /*enable*/
			dm->debug_components |= (one << dm_value[0]);
		else if (dm_value[1] == 2) /*disable*/
			dm->debug_components &= ~(one << dm_value[0]);
		else
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[Warning]  1:on,  2:off\n");

		if ((BIT(dm_value[0]) == DBG_PHY_STATUS) && dm_value[1] == 1) {
			dm->phy_dbg_info.show_phy_sts_all_pkt = (u8)dm_value[2];
			dm->phy_dbg_info.show_phy_sts_max_cnt = (u16)dm_value[3];

			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "show_phy_sts_all_pkt=%d, show_phy_sts_max=%d\n\n",
				 dm->phy_dbg_info.show_phy_sts_all_pkt,
				 dm->phy_dbg_info.show_phy_sts_max_cnt);

		} else if ((BIT(dm_value[0]) == DBG_CMN) && (dm_value[1] == 1)) {
			dm->cmn_dbg_msg_period = (u8)dm_value[2];

			if (dm->cmn_dbg_msg_period < PHYDM_WATCH_DOG_PERIOD)
				dm->cmn_dbg_msg_period = PHYDM_WATCH_DOG_PERIOD;

			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "cmn_dbg_msg_period=%d\n",
				 dm->cmn_dbg_msg_period);
		}
	}
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "pre-DbgComponents = 0x%llx\n", pre_debug_components);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "Curr-DbgComponents = 0x%llx\n", dm->debug_components);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "================================\n");

	*_used = used;
	*_out_len = out_len;
}

void phydm_fw_debug_trace(void *dm_void, u32 *const dm_value, u32 *_used,
			  char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 pre_fw_debug_components, one = 1;
	u32 used = *_used;
	u32 out_len = *_out_len;

	pre_fw_debug_components = dm->fw_debug_components;

	PDM_SNPF(out_len, used, output + used, out_len - used, "\n%s\n",
		 "================================");
	if (dm_value[0] == 100) {
		PDM_SNPF(out_len, used, output + used, out_len - used, "%s\n",
			 "[FW Debug Component]");
		PDM_SNPF(out_len, used, output + used, out_len - used, "%s\n",
			 "================================");
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "00. (( %s ))RA\n",
			 ((dm->fw_debug_components & PHYDM_FW_COMP_RA) ? ("V") :
			 (".")));

		if (dm->support_ic_type & PHYDM_IC_3081_SERIES) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "01. (( %s ))MU\n",
				 ((dm->fw_debug_components & PHYDM_FW_COMP_MU) ?
				 ("V") : (".")));
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "02. (( %s ))path Div\n",
				 ((dm->fw_debug_components &
				 PHYDM_FW_COMP_PATH_DIV) ? ("V") : (".")));
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "03. (( %s ))Power training\n",
				 ((dm->fw_debug_components & PHYDM_FW_COMP_PT) ?
				 ("V") : (".")));
		}
		PDM_SNPF(out_len, used, output + used, out_len - used, "%s\n",
			 "================================");

	} else {
		if (dm_value[0] == 101) {
			dm->fw_debug_components = 0;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "%s\n", "Clear all fw debug components");
		} else {
			if (dm_value[1] == 1) /*enable*/
				dm->fw_debug_components |= (one << dm_value[0]);
			else if (dm_value[1] == 2) /*disable*/
				dm->fw_debug_components &= ~(one << dm_value[0]);
			else
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "%s\n",
					 "[Warning!!!]  1:enable,  2:disable");
		}

		if (dm->fw_debug_components == 0) {
			dm->debug_components &= ~DBG_FW_TRACE;
			phydm_fw_trace_en_h2c(dm, false, dm->fw_debug_components, dm_value[2], dm_value[3]); /*H2C to enable C2H Msg*/
		} else {
			dm->debug_components |= DBG_FW_TRACE;
			phydm_fw_trace_en_h2c(dm, true, dm->fw_debug_components, dm_value[2], dm_value[3]); /*H2C to enable C2H Msg*/
		}
	}
}

#if (ODM_IC_11N_SERIES_SUPPORT)
void phydm_dump_bb_reg_n(
	void *dm_void,
	u32 *_used,
	char *output,
	u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 addr = 0;
	u32 used = *_used;
	u32 out_len = *_out_len;

	/* BB Reg, For Nseries IC we only need to dump page8 to pageF using 3 digits*/
	for (addr = 0x800; addr < 0xfff; addr += 4) {
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%03x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));
	}

	*_used = used;
	*_out_len = out_len;
}
#endif

#if (ODM_IC_11AC_SERIES_SUPPORT)
void phydm_dump_bb_reg_ac(void *dm_void, u32 *_used, char *output,
			  u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 addr = 0;
	u32 used = *_used;
	u32 out_len = *_out_len;

	for (addr = 0x800; addr < 0xfff; addr += 4) {
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));
	}

	if (!(dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8814A | ODM_RTL8821C)))
		goto rpt_reg;

	if (dm->rf_type > RF_2T2R) {
		for (addr = 0x1800; addr < 0x18ff; addr += 4)
			PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
				      addr, odm_get_bb_reg(dm, addr, MASKDWORD));
	}

	if (dm->rf_type > RF_3T3R) {
		for (addr = 0x1a00; addr < 0x1aff; addr += 4)
			PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
				      addr, odm_get_bb_reg(dm, addr, MASKDWORD));
	}

	for (addr = 0x1900; addr < 0x19ff; addr += 4)
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));

	for (addr = 0x1c00; addr < 0x1cff; addr += 4)
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));

	for (addr = 0x1f00; addr < 0x1fff; addr += 4)
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));

rpt_reg:

	*_used = used;
	*_out_len = out_len;
}

#endif

void phydm_dump_bb_reg(void *dm_void, u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;

	PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "BB==========\n");

	if (dm->support_ic_type & ODM_IC_11N_SERIES)
#if (ODM_IC_11N_SERIES_SUPPORT)
		phydm_dump_bb_reg_n(dm, &used, output, &out_len);
#else
		;
#endif
	else
#if (ODM_IC_11AC_SERIES_SUPPORT)
		phydm_dump_bb_reg_ac(dm, &used, output, &out_len);
#else
		;
#endif

	*_used = used;
	*_out_len = out_len;
}

void phydm_dump_rf_reg(void *dm_void, u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 addr = 0;
	u32 used = *_used;
	u32 out_len = *_out_len;

	/* dump RF register */
	PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "RF-A==========\n");
	for (addr = 0; addr < 0xFF; addr++)
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%02x 0x%05x\n",
			      addr, odm_get_rf_reg(dm, RF_PATH_A, addr, RFREGOFFSETMASK));
#ifdef PHYDM_COMPILE_ABOVE_2SS
	if (dm->rf_type > RF_1T1R) {
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "RF-B==========\n");
		for (addr = 0; addr < 0xFF; addr++)
			PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%02x 0x%05x\n",
				      addr, odm_get_rf_reg(dm, RF_PATH_B, addr, RFREGOFFSETMASK));
	}
#endif

#ifdef PHYDM_COMPILE_ABOVE_3SS
	if (dm->rf_type > RF_2T2R) {
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "RF-C==========\n");
		for (addr = 0; addr < 0xFF; addr++)
			PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%02x 0x%05x\n",
				      addr, odm_get_rf_reg(dm, RF_PATH_C, addr, RFREGOFFSETMASK));
	}
#endif

#ifdef PHYDM_COMPILE_ABOVE_4SS
	if (dm->rf_type > RF_3T3R) {
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "RF-D==========\n");
		for (addr = 0; addr < 0xFF; addr++)
			PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%02x 0x%05x\n",
				      addr, odm_get_rf_reg(dm, RF_PATH_D, addr, RFREGOFFSETMASK));
	}
#endif

	*_used = used;
	*_out_len = out_len;
}

void phydm_dump_mac_reg(void *dm_void, u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 addr = 0;
	u32 used = *_used;
	u32 out_len = *_out_len;

	/* dump MAC register */
	PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "MAC==========\n");
	for (addr = 0; addr < 0x7ff; addr += 4)
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));

	for (addr = 0x1000; addr < 0x17ff; addr += 4)
		PDM_VAST_SNPF(out_len, used, output + used, out_len - used, "0x%04x 0x%08x\n",
			      addr, odm_get_bb_reg(dm, addr, MASKDWORD));

	*_used = used;
	*_out_len = out_len;
}

void phydm_dump_reg(void *dm_void, char input[][16], u32 *_used, char *output,
		    u32 *_out_len, u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	char help[] = "-h";
	u32 var1[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;

	if (input[1])
		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

	PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

	if ((strcmp(input[1], help) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "dumpreg {0:all, 1:BB, 2:RF, 3:MAC}\n");
	} else if (var1[0] == 0) {
		phydm_dump_mac_reg(dm, &used, output, &out_len);
		phydm_dump_bb_reg(dm, &used, output, &out_len);
		phydm_dump_rf_reg(dm, &used, output, &out_len);
	} else if (var1[0] == 1) {
		phydm_dump_bb_reg(dm, &used, output, &out_len);
	} else if (var1[0] == 2) {
		phydm_dump_rf_reg(dm, &used, output, &out_len);
	} else if (var1[0] == 3) {
		phydm_dump_mac_reg(dm, &used, output, &out_len);
	}

	*_used = used;
	*_out_len = out_len;
}

void phydm_enable_big_jump(struct dm_struct *dm, boolean state)
{
#if (RTL8822B_SUPPORT == 1)
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	if (state == false) {
		dm->dm_dig_table.enable_adjust_big_jump = false;
		odm_set_bb_reg(dm, R_0x8c8, 0xfe, ((dig_t->big_jump_step3 << 5) | (dig_t->big_jump_step2 << 3) | dig_t->big_jump_step1));
	} else {
		dm->dm_dig_table.enable_adjust_big_jump = true;
	}
#endif
}

#if (RTL8822B_SUPPORT == 1 | RTL8821C_SUPPORT == 1 | RTL8814B_SUPPORT == 1 | RTL8195B_SUPPORT == 1)

void phydm_show_rx_rate(struct dm_struct *dm, u32 *_used, char *output,
			u32 *_out_len)
{
	u32 used = *_used;
	u32 out_len = *_out_len;

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "=====Rx SU rate Statistics=====\n");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "1SS MCS0 = %d, 1SS MCS1 = %d, 1SS MCS2 = %d, 1SS MCS 3 = %d\n",
		 dm->phy_dbg_info.num_qry_vht_pkt[0],
		 dm->phy_dbg_info.num_qry_vht_pkt[1],
		 dm->phy_dbg_info.num_qry_vht_pkt[2],
		 dm->phy_dbg_info.num_qry_vht_pkt[3]);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "1SS MCS4 = %d, 1SS MCS5 = %d, 1SS MCS6 = %d, 1SS MCS 7 = %d\n",
		 dm->phy_dbg_info.num_qry_vht_pkt[4],
		 dm->phy_dbg_info.num_qry_vht_pkt[5],
		 dm->phy_dbg_info.num_qry_vht_pkt[6],
		 dm->phy_dbg_info.num_qry_vht_pkt[7]);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "1SS MCS8 = %d, 1SS MCS9 = %d\n",
		 dm->phy_dbg_info.num_qry_vht_pkt[8],
		 dm->phy_dbg_info.num_qry_vht_pkt[9]);

#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	if (dm->support_ic_type & (PHYDM_IC_ABOVE_2SS)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "2SS MCS0 = %d, 2SS MCS1 = %d, 2SS MCS2 = %d, 2SS MCS 3 = %d\n",
			 dm->phy_dbg_info.num_qry_vht_pkt[10],
			 dm->phy_dbg_info.num_qry_vht_pkt[11],
			 dm->phy_dbg_info.num_qry_vht_pkt[12],
			 dm->phy_dbg_info.num_qry_vht_pkt[13]);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "2SS MCS4 = %d, 2SS MCS5 = %d, 2SS MCS6 = %d, 2SS MCS 7 = %d\n",
			 dm->phy_dbg_info.num_qry_vht_pkt[14],
			 dm->phy_dbg_info.num_qry_vht_pkt[15],
			 dm->phy_dbg_info.num_qry_vht_pkt[16],
			 dm->phy_dbg_info.num_qry_vht_pkt[17]);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "2SS MCS8 = %d, 2SS MCS9 = %d\n",
			 dm->phy_dbg_info.num_qry_vht_pkt[18],
			 dm->phy_dbg_info.num_qry_vht_pkt[19]);
	}
#endif

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "=====Rx MU rate Statistics=====\n");
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "1SS MCS0 = %d, 1SS MCS1 = %d, 1SS MCS2 = %d, 1SS MCS 3 = %d\n",
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[0],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[1],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[2],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[3]);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "1SS MCS4 = %d, 1SS MCS5 = %d, 1SS MCS6 = %d, 1SS MCS 7 = %d\n",
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[4],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[5],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[6],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[7]);
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "1SS MCS8 = %d, 1SS MCS9 = %d\n",
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[8],
		 dm->phy_dbg_info.num_qry_mu_vht_pkt[9]);

#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	if (dm->support_ic_type & (PHYDM_IC_ABOVE_2SS)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "2SS MCS0 = %d, 2SS MCS1 = %d, 2SS MCS2 = %d, 2SS MCS 3 = %d\n",
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[10],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[11],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[12],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[13]);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "2SS MCS4 = %d, 2SS MCS5 = %d, 2SS MCS6 = %d, 2SS MCS 7 = %d\n",
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[14],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[15],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[16],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[17]);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "2SS MCS8 = %d, 2SS MCS9 = %d\n",
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[18],
			 dm->phy_dbg_info.num_qry_mu_vht_pkt[19]);
	}
#endif

	*_used = used;
	*_out_len = out_len;
}

#endif

void phydm_per_tone_evm(void *dm_void, char input[][16], u32 *_used,
			char *output, u32 *_out_len, u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 i, j;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 var1[4] = {0};
	u32 value32, tone_num, round;
	s8 rxevm_0, rxevm_1;
	s32 avg_num, evm_tone_0[256] = {0}, evm_tone_1[256] = {0};
	s32 rxevm_sum_0, rxevm_sum_1;

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		pr_debug("n series not support yet !\n");
		return;
	}

	for (i = 0; i < 4; i++) {
		if (input[i + 1])
			PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
	}
	avg_num = var1[0];
	round = var1[1];

	if (dm->is_linked) {
		pr_debug("ID=((%d)), BW=((%d)), fc=((CH-%d))\n", dm->curr_station_id,
			 20 << *dm->band_width, *dm->channel);
		pr_debug("avg_num =((%d)), round =((%d))\n", avg_num, round);
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
		watchdog_stop(dm->priv);
#endif
		for (j = 0; j < round; j++) {
			pr_debug("\nround((%d))\n", (j + 1));
			if (*dm->band_width == CHANNEL_WIDTH_20) {
				for (tone_num = 228; tone_num <= 255; tone_num++) {
					odm_set_bb_reg(dm, R_0x8c4, 0xff8, tone_num);
					rxevm_sum_0 = 0;
					rxevm_sum_1 = 0;
					for (i = 0; i < avg_num; i++) {
						value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);

						rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
						rxevm_0 = (rxevm_0 / 2);
						if (rxevm_0 < -63)
							rxevm_0 = 0;

						rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
						rxevm_1 = (rxevm_1 / 2);
						if (rxevm_1 < -63)
							rxevm_1 = 0;
						rxevm_sum_0 += rxevm_0;
						rxevm_sum_1 += rxevm_1;
						ODM_delay_ms(1);
					}
					evm_tone_0[tone_num] = (rxevm_sum_0 / avg_num);
					evm_tone_1[tone_num] = (rxevm_sum_1 / avg_num);
					pr_debug("Tone((-%-3d)) RXEVM (1ss/2ss) =%d , %d\n", (256 - tone_num), evm_tone_0[tone_num], evm_tone_1[tone_num]);
				}

				for (tone_num = 1; tone_num <= 28; tone_num++) {
					odm_set_bb_reg(dm, R_0x8c4, 0xff8, tone_num);
					rxevm_sum_0 = 0;
					rxevm_sum_1 = 0;
					for (i = 0; i < avg_num; i++) {
						value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);

						rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
						rxevm_0 = (rxevm_0 / 2);
						if (rxevm_0 < -63)
							rxevm_0 = 0;

						rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
						rxevm_1 = (rxevm_1 / 2);
						if (rxevm_1 < -63)
							rxevm_1 = 0;
						rxevm_sum_0 += rxevm_0;
						rxevm_sum_1 += rxevm_1;
						ODM_delay_ms(1);
					}
					evm_tone_0[tone_num] = (rxevm_sum_0 / avg_num);
					evm_tone_1[tone_num] = (rxevm_sum_1 / avg_num);
					pr_debug("Tone(( %-3d)) RXEVM (1ss/2ss) =%d , %d\n", tone_num, evm_tone_0[tone_num], evm_tone_1[tone_num]);
				}
			} else if (*dm->band_width == CHANNEL_WIDTH_40) {
				for (tone_num = 198; tone_num <= 254; tone_num++) {
					odm_set_bb_reg(dm, R_0x8c4, 0xff8, tone_num);
					rxevm_sum_0 = 0;
					rxevm_sum_1 = 0;
					for (i = 0; i < avg_num; i++) {
						value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);

						rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
						rxevm_0 = (rxevm_0 / 2);
						if (rxevm_0 < -63)
							rxevm_0 = 0;

						rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
						rxevm_1 = (rxevm_1 / 2);
						if (rxevm_1 < -63)
							rxevm_1 = 0;

						rxevm_sum_0 += rxevm_0;
						rxevm_sum_1 += rxevm_1;
						ODM_delay_ms(1);
					}
					evm_tone_0[tone_num] = (rxevm_sum_0 / avg_num);
					evm_tone_1[tone_num] = (rxevm_sum_1 / avg_num);
					pr_debug("Tone((-%-3d)) RXEVM (1ss/2ss) =%d , %d\n", (256 - tone_num), evm_tone_0[tone_num], evm_tone_1[tone_num]);
				}

				for (tone_num = 2; tone_num <= 58; tone_num++) {
					odm_set_bb_reg(dm, R_0x8c4, 0xff8, tone_num);
					rxevm_sum_0 = 0;
					rxevm_sum_1 = 0;
					for (i = 0; i < avg_num; i++) {
						value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);

						rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
						rxevm_0 = (rxevm_0 / 2);
						if (rxevm_0 < -63)
							rxevm_0 = 0;

						rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
						rxevm_1 = (rxevm_1 / 2);
						if (rxevm_1 < -63)
							rxevm_1 = 0;
						rxevm_sum_0 += rxevm_0;
						rxevm_sum_1 += rxevm_1;
						ODM_delay_ms(1);
					}
					evm_tone_0[tone_num] = (rxevm_sum_0 / avg_num);
					evm_tone_1[tone_num] = (rxevm_sum_1 / avg_num);
					pr_debug("Tone(( %-3d)) RXEVM (1ss/2ss) =%d , %d\n", tone_num, evm_tone_0[tone_num], evm_tone_1[tone_num]);
				}
			} else if (*dm->band_width == CHANNEL_WIDTH_80) {
				for (tone_num = 134; tone_num <= 254; tone_num++) {
					odm_set_bb_reg(dm, R_0x8c4, 0xff8, tone_num);
					rxevm_sum_0 = 0;
					rxevm_sum_1 = 0;
					for (i = 0; i < avg_num; i++) {
						value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);

						rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
						rxevm_0 = (rxevm_0 / 2);
						if (rxevm_0 < -63)
							rxevm_0 = 0;

						rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
						rxevm_1 = (rxevm_1 / 2);
						if (rxevm_1 < -63)
							rxevm_1 = 0;
						rxevm_sum_0 += rxevm_0;
						rxevm_sum_1 += rxevm_1;
						ODM_delay_ms(1);
					}
					evm_tone_0[tone_num] = (rxevm_sum_0 / avg_num);
					evm_tone_1[tone_num] = (rxevm_sum_1 / avg_num);
					pr_debug("Tone((-%-3d)) RXEVM (1ss/2ss) =%d , %d\n", (256 - tone_num), evm_tone_0[tone_num], evm_tone_1[tone_num]);
				}

				for (tone_num = 2; tone_num <= 122; tone_num++) {
					odm_set_bb_reg(dm, R_0x8c4, 0xff8, tone_num);
					rxevm_sum_0 = 0;
					rxevm_sum_1 = 0;
					for (i = 0; i < avg_num; i++) {
						value32 = odm_get_bb_reg(dm, R_0xf8c, MASKDWORD);

						rxevm_0 = (s8)((value32 & MASKBYTE2) >> 16);
						rxevm_0 = (rxevm_0 / 2);
						if (rxevm_0 < -63)
							rxevm_0 = 0;

						rxevm_1 = (s8)((value32 & MASKBYTE3) >> 24);
						rxevm_1 = (rxevm_1 / 2);
						if (rxevm_1 < -63)
							rxevm_1 = 0;
						rxevm_sum_0 += rxevm_0;
						rxevm_sum_1 += rxevm_1;
						ODM_delay_ms(1);
					}
					evm_tone_0[tone_num] = (rxevm_sum_0 / avg_num);
					evm_tone_1[tone_num] = (rxevm_sum_1 / avg_num);
					pr_debug("Tone(( %-3d)) RXEVM (1ss/2ss) =%d , %d\n", tone_num, evm_tone_0[tone_num], evm_tone_1[tone_num]);
				}
			}
		}
	} else {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "No Link !!\n");
	}
}

void phydm_api_adjust(void *dm_void, char input[][16], u32 *_used, char *output,
		      u32 *_out_len, u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	char help[] = "-h";
	u32 var1[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;
	boolean is_enable_dbg_mode;
	u8 central_ch, primary_ch_idx;
	enum channel_width bandwidth;

#ifdef PHYDM_COMMON_API_SUPPORT

	if ((strcmp(input[1], help) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "{en} {ch_num} {prm_ch 1/2/3/4/9/10} {0:20M, 1:40M, 2:80M}\n");
		goto out;
	}

	if ((dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8821C | ODM_RTL8192F)) == 0) { /*jj add 20170822*/
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "This IC doesn't support PHYDM API function\n");
		/**/
		goto out;
	}

	for (i = 0; i < 4; i++) {
		if (input[i + 1])
			PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
	}

	is_enable_dbg_mode = (boolean)var1[0];
	central_ch = (u8)var1[1];
	primary_ch_idx = (u8)var1[2];
	bandwidth = (enum channel_width)var1[3];

	if (is_enable_dbg_mode) {
		dm->is_disable_phy_api = false;
		phydm_api_switch_bw_channel(dm, central_ch, primary_ch_idx, bandwidth);
		dm->is_disable_phy_api = true;
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "central_ch = %d, primary_ch_idx = %d, bandwidth = %d\n",
			 central_ch, primary_ch_idx, bandwidth);
	} else {
		dm->is_disable_phy_api = false;
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Disable API debug mode\n");
	}
out:
#else
	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "This IC doesn't support PHYDM API function\n");
#endif

	*_used = used;
	*_out_len = out_len;
}

void phydm_parameter_adjust(void *dm_void, char input[][16], u32 *_used,
			    char *output, u32 *_out_len, u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_cfo_track_struct *cfo_track = (struct phydm_cfo_track_struct *)phydm_get_structure(dm, PHYDM_CFOTRACK);
	char help[] = "-h";
	u32 var1[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;

	if ((strcmp(input[1], help) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "1. X_cap = ((0x%x))\n", cfo_track->crystal_cap);

	} else {
		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		if (var1[0] == 0) {
			PHYDM_SSCANF(input[2], DCMD_HEX, &var1[1]);
			phydm_set_crystal_cap(dm, (u8)var1[1]);
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "X_cap = ((0x%x))\n", cfo_track->crystal_cap);
		}
	}
	*_used = used;
	*_out_len = out_len;
}

struct phydm_command {
	char name[16];
	u8 id;
};

enum PHYDM_CMD_ID {
	PHYDM_HELP,
	PHYDM_DEMO,
	PHYDM_RF_CMD,
	PHYDM_DIG,
	PHYDM_RA,
	PHYDM_PROFILE,
	PHYDM_ANTDIV,
	PHYDM_PATHDIV,
	PHYDM_DEBUG,
	PHYDM_FW_DEBUG,
	PHYDM_SUPPORT_ABILITY,
	PHYDM_GET_TXAGC,
	PHYDM_SET_TXAGC,
	PHYDM_SMART_ANT,
	PHYDM_API,
	PHYDM_TRX_PATH,
	PHYDM_LA_MODE,
	PHYDM_DUMP_REG,
	PHYDM_AUTO_DBG,
	PHYDM_BIG_JUMP,
	PHYDM_SHOW_RXRATE,
	PHYDM_NBI_EN,
	PHYDM_CSI_MASK_EN,
	PHYDM_DFS_DEBUG,
	PHYDM_NHM,
	PHYDM_CLM,
	PHYDM_FAHM,
	PHYDM_ENV_MNTR,
	PHYDM_BB_INFO,
	PHYDM_TXBF,
	PHYDM_H2C,
	PHYDM_ANT_SWITCH,
	PHYDM_ADAPTIVE_SOML,
	PHYDM_PSD,
	PHYDM_DEBUG_PORT,
	PHYDM_DIS_HTSTF_CONTROL,
	PHYDM_TUNE_PARAMETER,
	PHYDM_ADAPTIVITY_DEBUG,
	PHYDM_DIS_DYM_ANT_WEIGHTING,
	PHYDM_FORECE_PT_STATE,
	PHYDM_DIS_RXHP_CTR,
	PHYDM_STA_INFO,
	PHYDM_PAUSE_FUNC,
	PHYDM_PER_TONE_EVM,
	PHYDM_DYN_TXPWR,
	PHYDM_LNA_SAT
};

struct phydm_command phy_dm_ary[] = {
	{"-h", PHYDM_HELP}, /*do not move this element to other position*/
	{"demo", PHYDM_DEMO}, /*do not move this element to other position*/
	{"rf", PHYDM_RF_CMD},
	{"dig", PHYDM_DIG},
	{"ra", PHYDM_RA},
	{"profile", PHYDM_PROFILE},
	{"antdiv", PHYDM_ANTDIV},
	{"pathdiv", PHYDM_PATHDIV},
	{"dbg", PHYDM_DEBUG},
	{"fw_dbg", PHYDM_FW_DEBUG},
	{"ability", PHYDM_SUPPORT_ABILITY},
	{"get_txagc", PHYDM_GET_TXAGC},
	{"set_txagc", PHYDM_SET_TXAGC},
	{"smtant", PHYDM_SMART_ANT},
	{"api", PHYDM_API},
	{"trxpath", PHYDM_TRX_PATH},
	{"lamode", PHYDM_LA_MODE},
	{"dumpreg", PHYDM_DUMP_REG},
	{"auto_dbg", PHYDM_AUTO_DBG},
	{"bigjump", PHYDM_BIG_JUMP},
	{"rxrate", PHYDM_SHOW_RXRATE},
	{"nbi", PHYDM_NBI_EN},
	{"csi_mask", PHYDM_CSI_MASK_EN},
	{"dfs", PHYDM_DFS_DEBUG},
	{"nhm", PHYDM_NHM},
	{"clm", PHYDM_CLM},
	{"fahm", PHYDM_FAHM},
	{"env_mntr", PHYDM_ENV_MNTR},
	{"bbinfo", PHYDM_BB_INFO},
	{"txbf", PHYDM_TXBF},
	{"h2c", PHYDM_H2C},
	{"ant_switch", PHYDM_ANT_SWITCH},
	{"soml", PHYDM_ADAPTIVE_SOML},
	{"psd", PHYDM_PSD},
	{"dbgport", PHYDM_DEBUG_PORT},
	{"dis_htstf", PHYDM_DIS_HTSTF_CONTROL},
	{"tune_para", PHYDM_TUNE_PARAMETER},
	{"adapt_debug", PHYDM_ADAPTIVITY_DEBUG},
	{"dis_dym_ant_wgt", PHYDM_DIS_DYM_ANT_WEIGHTING},
	{"force_pt_state", PHYDM_FORECE_PT_STATE},
	{"dis_drxhp", PHYDM_DIS_RXHP_CTR},
	{"sta_info", PHYDM_STA_INFO},
	{"pause", PHYDM_PAUSE_FUNC},
	{"evm", PHYDM_PER_TONE_EVM},
	{"dyn_txpwr", PHYDM_DYN_TXPWR},
	{"lna_sat", PHYDM_LNA_SAT} };

#endif /*#ifdef CONFIG_PHYDM_DEBUG_FUNCTION*/

void phydm_cmd_parser(struct dm_struct *dm, char input[][MAX_ARGV],
		      u32 input_num, u8 flag, char *output, u32 out_len)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	u32 used = 0;
	u8 id = 0;
	u32 var1[10] = {0};
	u32 i, input_idx = 0;
	u32 phydm_ary_size = sizeof(phy_dm_ary) / sizeof(struct phydm_command);
	char help[] = "-h";

	if (flag == 0) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "GET, nothing to print\n");
		return;
	}

	PDM_SNPF(out_len, used, output + used, out_len - used, "\n");

	/* Parsing Cmd ID */
	if (input_num) {
		for (i = 0; i < phydm_ary_size; i++) {
			if (strcmp(phy_dm_ary[i].name, input[0]) == 0) {
				id = phy_dm_ary[i].id;
				break;
			}
		}
		if (i == phydm_ary_size) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "SET, command not found!\n");
			return;
		}
	}

	switch (id) {
	case PHYDM_HELP: {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "BB cmd ==>\n");

		for (i = 0; i < phydm_ary_size - 2; i++) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "  %-5d: %s\n", i, phy_dm_ary[i + 2].name);
			/**/
		}
	} break;

	case PHYDM_DEMO: { /*echo demo 10 0x3a z abcde >cmd*/
		u32 directory = 0;

		#if (DM_ODM_SUPPORT_TYPE & (ODM_CE | ODM_AP))
		char char_temp;
		#else
		u32 char_temp = ' ';
		#endif

		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &directory);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Decimal value = %d\n", directory);
		PHYDM_SSCANF(input[2], DCMD_HEX, &directory);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Hex value = 0x%x\n", directory);
		PHYDM_SSCANF(input[3], DCMD_CHAR, &char_temp);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Char = %c\n", char_temp);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "String = %s\n", input[4]);
	} break;
	case PHYDM_RF_CMD:
		halrf_cmd_parser(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_DIG:
		phydm_dig_debug(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_RA:
		phydm_ra_debug(dm, &input[0], &used, output, &out_len);
		break;

	case PHYDM_ANTDIV:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);

				/*PDM_SNPF((output+used, out_len-used, "new SET, PATHDIV_var[%d]= (( %d ))\n", i, var1[i]));*/
				input_idx++;
			}
		}

		if (input_idx >= 1)
			#if 0
			PDM_SNPF((output+used, out_len-used, "odm_PATHDIV_debug\n"));
			#endif
			#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
			phydm_antdiv_debug(dm, (u32 *)var1, &used, output, &out_len);
			#else
			;
			#endif

		break;

	case PHYDM_PATHDIV:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);

				/*PDM_SNPF((output+used, out_len-used, "new SET, PATHDIV_var[%d]= (( %d ))\n", i, var1[i]));*/
				input_idx++;
			}
		}

		if (input_idx >= 1)
			#if 0
			PDM_SNPF((output+used, out_len-used, "odm_PATHDIV_debug\n"));
			#endif
			#if (defined(CONFIG_PATH_DIVERSITY))
			odm_pathdiv_debug(dm, (u32 *)var1, &used, output, &out_len);
			#else
			;
			#endif
		break;

	case PHYDM_DEBUG:

		phydm_debug_trace(dm, &input[0], &used, output, &out_len);

		break;

	case PHYDM_FW_DEBUG:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1)
			phydm_fw_debug_trace(dm, (u32 *)var1, &used, output, &out_len);

		break;

	case PHYDM_SUPPORT_ABILITY:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1)
			phydm_support_ability_debug(dm, (u32 *)var1, &used, output, &out_len);
		break;

	case PHYDM_SMART_ANT:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1) {
	#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))

		#ifdef CONFIG_HL_SMART_ANTENNA_TYPE2
		phydm_hl_smart_ant_debug_type2(dm, &input[0], &used, output, &out_len, input_num);
		#elif (defined(CONFIG_HL_SMART_ANTENNA_TYPE1))
		phydm_hl_smart_ant_debug(dm, &input[0], &used, output, &out_len, input_num);
		#endif

	#endif

	#if (defined(CONFIG_CUMITEK_SMART_ANTENNA))
		phydm_cumitek_smt_ant_debug(dm, &input[0], &used, output, &out_len, input_num);
	#endif
		}

		break;

	case PHYDM_API:
		phydm_api_adjust(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_PROFILE:
		phydm_basic_profile(dm, &used, output, &out_len);
		break;

	case PHYDM_GET_TXAGC:
		phydm_get_txagc(dm, &used, output, &out_len);
		break;

	case PHYDM_SET_TXAGC: {
		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);
				input_idx++;
			}
		}
		if ((strcmp(input[1], help) == 0)) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "{Dis:0, En:1} {pathA~D(0~3)} {rate_idx(Hex), All_rate:0xff} {txagc_idx (Hex)}\n");
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "{Pwr Shift:2} {pathA~D(0~3)} {0:-, 1:+} {Pwr Offset (Hex)}\n");
		} else if (var1[0] == 0) {
			dm->is_disable_phy_api = true;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Disable API debug mode\n");
		} else if (var1[0] == 1) {
			dm->is_disable_phy_api = false;
			phydm_set_txagc(dm, (u32 *)var1, &used, output, &out_len);
			dm->is_disable_phy_api = true;
		} else if (var1[0] == 2) {
			dm->is_disable_phy_api = false;
			phydm_shift_txagc(dm, (u32 *)var1, &used, output, &out_len);
			dm->is_disable_phy_api = true;
		}
	} break;

	case PHYDM_TRX_PATH:

		for (i = 0; i < 4; i++) {
			if (input[i + 1])
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
		}
#if (RTL8822B_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1) /*jj add 20170822*/
		if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8192F)) { /*jj add 20170822*/
			u8 tx_path, rx_path;
			boolean is_enable_dbg_mode, is_tx2_path;

			is_enable_dbg_mode = (boolean)var1[0];
			tx_path = (u8)var1[1];
			rx_path = (u8)var1[2];
			is_tx2_path = (boolean)var1[3];

			if (is_enable_dbg_mode) {
				dm->is_disable_phy_api = false;
				phydm_api_trx_mode(dm, (enum bb_path)tx_path, (enum bb_path)rx_path, is_tx2_path);
				dm->is_disable_phy_api = true;
				PDM_SNPF(out_len, used, output + used,
					 out_len - used,
					 "tx_path = 0x%x, rx_path = 0x%x, is_tx2_path = %d\n",
					 tx_path, rx_path, is_tx2_path);
			} else {
				dm->is_disable_phy_api = false;
				PDM_SNPF(out_len, used, output + used,
					 out_len - used,
					 "Disable API debug mode\n");
			}
		} else {
			phydm_config_trx_path(dm, (u32 *)var1, &used, output, &out_len);
		}
#else
		phydm_config_trx_path(dm, (u32 *)var1, &used, output, &out_len);
#endif

		break;

	case PHYDM_LA_MODE:

		#if (PHYDM_LA_MODE_SUPPORT == 1)
		phydm_lamode_trigger_setting(dm, &input[0], &used, output, &out_len, input_num);
		#else
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "This IC doesn't support LA mode\n");
		#endif

		break;

	case PHYDM_DUMP_REG:
		phydm_dump_reg(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_BIG_JUMP: {
#if (RTL8822B_SUPPORT == 1)
		if (dm->support_ic_type & ODM_RTL8822B) {
			if (input[1]) {
				PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);
				phydm_enable_big_jump(dm, (boolean)(var1[0]));
			} else {
				PDM_SNPF(out_len, used, output + used,
					 out_len - used, "unknown command!\n");
			}
		} else {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "The command is only for 8822B!\n");
		}
#endif
		break;
	}

	case PHYDM_AUTO_DBG:
		#ifdef PHYDM_AUTO_DEGBUG
		phydm_auto_dbg_console(dm, &input[0], &used, output, &out_len, input_num);
		#endif
		break;

	case PHYDM_SHOW_RXRATE: {
#if (RTL8822B_SUPPORT == 1 | RTL8821C_SUPPORT == 1 | RTL8814B_SUPPORT == 1 | RTL8195B_SUPPORT == 1)
		u8 rate_idx;
		if ((dm->support_ic_type & PHYDM_IC_SUPPORT_MU_BFEE) == 0)
			break;

		if (input[1])
			PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		if (var1[0] == 1) {
			phydm_show_rx_rate(dm, &used, output, &out_len);
		} else {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Reset Rx rate counter\n");

			for (rate_idx = 0; rate_idx < VHT_RATE_NUM; rate_idx++) {
				dm->phy_dbg_info.num_qry_vht_pkt[rate_idx] = 0;
				dm->phy_dbg_info.num_qry_mu_vht_pkt[rate_idx] = 0;
			}
		}
#endif
		break;
	}

	case PHYDM_NBI_EN:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1) {
			phydm_api_debug(dm, PHYDM_API_NBI, (u32 *)var1, &used, output, &out_len);
			/**/
		}

		break;

	case PHYDM_CSI_MASK_EN:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1) {
			phydm_api_debug(dm, PHYDM_API_CSI_MASK, (u32 *)var1, &used, output, &out_len);
			/**/
		}

		break;

	case PHYDM_DFS_DEBUG: {
#ifdef CONFIG_PHYDM_DFS_MASTER
		u32 var[4] = {0};

		for (i = 0; i < 4; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1)
			phydm_dfs_debug(dm, var, &used, output, &out_len);
#endif
		break;
	}

	case PHYDM_NHM:
		#ifdef NHM_SUPPORT
		phydm_nhm_dbg(dm, &input[0], &used, output, &out_len, input_num);
		#endif
		break;

	case PHYDM_CLM:
		#ifdef CLM_SUPPORT
		phydm_clm_dbg(dm, &input[0], &used, output, &out_len, input_num);
		#endif
		break;

	#ifdef FAHM_SUPPORT
	case PHYDM_FAHM:
		phydm_fahm_dbg(dm, &input[0], &used, output, &out_len, input_num);
		break;
	#endif

	case PHYDM_ENV_MNTR:
		phydm_env_mntr_dbg(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_BB_INFO: {
		s32 value32 = 0;

		phydm_bb_debug_info(dm, &used, output, &out_len);

		if (dm->support_ic_type & ODM_RTL8822B && input[1]) {
			PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);
			odm_set_bb_reg(dm, R_0x1988, 0x003fff00, var1[0]);
			value32 = odm_get_bb_reg(dm, R_0xf84, MASKDWORD);
			value32 = (value32 & 0xff000000) >> 24;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "\r\n %-35s = condition num = %d, subcarriers = %d\n",
				 "Over condition num subcarrier", var1[0],
				 value32);
			odm_set_bb_reg(dm, R_0x1988, BIT(22), 0x0); /*disable report condition number*/
		}
	} break;

	case PHYDM_TXBF: {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (BEAMFORMING_SUPPORT == 1)
		struct _RT_BEAMFORMING_INFO *beamforming_info = &dm->beamforming_info;

		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);
		if (var1[0] == 0) {
			beamforming_info->apply_v_matrix = false;
			beamforming_info->snding3ss = true;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "\r\n dont apply V matrix and 3SS 789 snding\n");
		} else if (var1[0] == 1) {
			beamforming_info->apply_v_matrix = true;
			beamforming_info->snding3ss = true;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "\r\n apply V matrix and 3SS 789 snding\n");
		} else if (var1[0] == 2) {
			beamforming_info->apply_v_matrix = true;
			beamforming_info->snding3ss = false;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "\r\n default txbf setting\n");
		} else
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "\r\n unknown cmd!!\n");
#else
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "\r\n no TxBF !!\n");
#endif
#endif
	} break;

	case PHYDM_H2C:

		for (i = 0; i < 8; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1)
			phydm_h2C_debug(dm, (u32 *)var1, &used, output, &out_len);

		break;

	case PHYDM_ANT_SWITCH:

		for (i = 0; i < 8; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1) {
#if (RTL8821A_SUPPORT == 1)
			phydm_set_ext_switch(dm, (u32 *)var1, &used, output, &out_len);
#else
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Not Support IC");
#endif
		}

		break;

	case PHYDM_ADAPTIVE_SOML:

#ifdef CONFIG_ADAPTIVE_SOML
		for (i = 0; i < 8; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1)
			phydm_soml_debug(dm, (u32 *)var1, &used, output, &out_len);

#else
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Not Support IC");
#endif

		break;

	case PHYDM_PSD:

		#ifdef CONFIG_PSD_TOOL
		phydm_psd_debug(dm, &input[0], &used, output, &out_len, input_num);
		#endif

		break;

	case PHYDM_DEBUG_PORT: {
		u32 dbg_port_value;

		PHYDM_SSCANF(input[1], DCMD_HEX, &var1[0]);

		dm->debug_components |= ODM_COMP_API;
		if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_3, var1[0])) { /*set debug port to 0x0*/

			dbg_port_value = phydm_get_bb_dbg_port_val(dm);
			phydm_release_bb_dbg_port(dm);

			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Dbg Port[0x%x] = ((0x%x))\n", var1[0],
				 dbg_port_value);
		}
		dm->debug_components &= (~ODM_COMP_API);
	} break;

	case PHYDM_DIS_HTSTF_CONTROL: {
		if (input[1])
			PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		if (var1[0] == 1) {
			/* setting being false is for debug */
			dm->bhtstfdisabled = true;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Dynamic HT-STF Gain Control is Disable\n");
		} else {
			/* default setting should be true, always be dynamic control*/
			dm->bhtstfdisabled = false;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Dynamic HT-STF Gain Control is Enable\n");
		}
	} break;

	case PHYDM_TUNE_PARAMETER:
		phydm_parameter_adjust(dm, &input[0], &used, output, &out_len, input_num);
		break;
#ifdef PHYDM_SUPPORT_ADAPTIVITY
	case PHYDM_ADAPTIVITY_DEBUG:

		for (i = 0; i < 5; i++) {
			if (input[i + 1]) {
				PHYDM_SSCANF(input[i + 1], DCMD_HEX, &var1[i]);
				input_idx++;
			}
		}

		if (input_idx >= 1)
			phydm_adaptivity_debug(dm, (u32 *)var1, &used, output, &out_len);

		break;
#endif
	case PHYDM_DIS_DYM_ANT_WEIGHTING:
		#ifdef DYN_ANT_WEIGHTING_SUPPORT
		phydm_dyn_ant_weight_dbg(dm, &input[0], &used, output, &out_len, input_num);
		#endif
		break;

	case PHYDM_FORECE_PT_STATE: {
		#ifdef PHYDM_POWER_TRAINING_SUPPORT
		phydm_pow_train_debug(dm, &input[0], &used, output, &out_len, input_num);
		#else
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Pow training: Not Support\n");
		#endif

		break;
	}

	case PHYDM_DIS_RXHP_CTR: {
		if (input[1])
			PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		if (var1[0] == 1) {
			/* the setting being on is at debug mode to disconnect RxHP seeting with SoML on/odd */
			dm->disrxhpsoml = true;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Dynamic RxHP Control with SoML on/off is Disable\n");
		} else if (var1[0] == 0) {
			/* default setting, RxHP setting will follow SoML on/off setting */
			dm->disrxhpsoml = false;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Dynamic RxHP Control with SoML on/off is Enable\n");
		} else {
			dm->disrxhpsoml = false;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Default Setting, Dynamic RxHP Control with SoML on/off is Enable\n");
		}
	} break;

	case PHYDM_STA_INFO:
		phydm_show_sta_info(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_PAUSE_FUNC:
		phydm_pause_func_console(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_PER_TONE_EVM:
		phydm_per_tone_evm(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_DYN_TXPWR:
		phydm_dtp_debug(dm, &input[0], &used, output, &out_len, input_num);
		break;

	case PHYDM_LNA_SAT:
		#ifdef PHYDM_LNA_SAT_CHK_SUPPORT
		phydm_lna_sat_debug(dm, &input[0], &used, output, &out_len, input_num);
		#endif
		break;

	default:
		break;
	}
#endif /*#ifdef CONFIG_PHYDM_DEBUG_FUNCTION*/
}

#ifdef __ECOS
char *strsep(char **s, const char *ct)
{
	char *sbegin = *s;
	char *end;

	if (sbegin == NULL)
		return NULL;

	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;
	return sbegin;
}
#endif

#if (DM_ODM_SUPPORT_TYPE & (ODM_CE | ODM_AP))
s32 phydm_cmd(struct dm_struct *dm, char *input, u32 in_len, u8 flag,
	      char *output, u32 out_len)
{
	char *token;
	u32 argc = 0;
	char argv[MAX_ARGC][MAX_ARGV];

	do {
		token = strsep(&input, ", ");
		if (token) {
			strcpy(argv[argc], token);
			argc++;
		} else {
			break;
		}
	} while (argc < MAX_ARGC);

	if (argc == 1)
		argv[0][strlen(argv[0]) - 1] = '\0';

	phydm_cmd_parser(dm, argv, argc, flag, output, out_len);

	return 0;
}
#endif

void phydm_fw_trace_handler(void *dm_void, u8 *cmd_buf, u8 cmd_len)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	/*u8	debug_trace_11byte[60];*/
	u8 freg_num, c2h_seq, buf_0 = 0;

	if (!(dm->support_ic_type & PHYDM_IC_3081_SERIES))
		return;

	if (cmd_len > 12 || cmd_len == 0) {
		pr_debug("[Warning] Error C2H cmd_len=%d\n", cmd_len);
		return;
	}

	buf_0 = cmd_buf[0];
	freg_num = (buf_0 & 0xf);
	c2h_seq = (buf_0 & 0xf0) >> 4;
	/*PHYDM_DBG(dm, DBG_FW_TRACE,"[FW debug message] freg_num = (( %d )), c2h_seq = (( %d ))\n", freg_num,c2h_seq );*/

	/*strncpy(debug_trace_11byte,&cmd_buf[1],(cmd_len-1));*/
	/*debug_trace_11byte[cmd_len-1] = '\0';*/
	/*PHYDM_DBG(dm, DBG_FW_TRACE,"[FW debug message] %s\n", debug_trace_11byte);*/
	/*PHYDM_DBG(dm, DBG_FW_TRACE,"[FW debug message] cmd_len = (( %d ))\n", cmd_len);*/
	/*PHYDM_DBG(dm, DBG_FW_TRACE,"[FW debug message] c2h_cmd_start  = (( %d ))\n", dm->c2h_cmd_start);*/

	/*PHYDM_DBG(dm, DBG_FW_TRACE,"pre_seq = (( %d )), current_seq = (( %d ))\n", dm->pre_c2h_seq, c2h_seq);*/
	/*PHYDM_DBG(dm, DBG_FW_TRACE,"fw_buff_is_enpty = (( %d ))\n", dm->fw_buff_is_enpty);*/

	if (c2h_seq != dm->pre_c2h_seq && dm->fw_buff_is_enpty == false) {
		dm->fw_debug_trace[dm->c2h_cmd_start] = '\0';
		PHYDM_DBG(dm, DBG_FW_TRACE, "[FW Dbg Queue Overflow] %s\n",
			  dm->fw_debug_trace);
		dm->c2h_cmd_start = 0;
	}

	if ((cmd_len - 1) > (60 - dm->c2h_cmd_start)) {
		dm->fw_debug_trace[dm->c2h_cmd_start] = '\0';
		PHYDM_DBG(dm, DBG_FW_TRACE,
			  "[FW Dbg Queue error: wrong C2H length] %s\n",
			  dm->fw_debug_trace);
		dm->c2h_cmd_start = 0;
		return;
	}

	strncpy((char *)&dm->fw_debug_trace[dm->c2h_cmd_start],
		(char *)&cmd_buf[1], (cmd_len - 1));
	dm->c2h_cmd_start += (cmd_len - 1);
	dm->fw_buff_is_enpty = false;

	if (freg_num == 0 || dm->c2h_cmd_start >= 60) {
		if (dm->c2h_cmd_start < 60)
			dm->fw_debug_trace[dm->c2h_cmd_start] = '\0';
		else
			dm->fw_debug_trace[59] = '\0';

		PHYDM_DBG(dm, DBG_FW_TRACE, "[FW DBG Msg] %s\n",
			  dm->fw_debug_trace);
		/*dbg_print("[FW DBG Msg] %s\n", dm->fw_debug_trace);*/
		dm->c2h_cmd_start = 0;
		dm->fw_buff_is_enpty = true;
	}

	dm->pre_c2h_seq = c2h_seq;
#endif /*#ifdef CONFIG_PHYDM_DEBUG_FUNCTION*/
}

void phydm_fw_trace_handler_code(void *dm_void, u8 *buffer, u8 cmd_len)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 function = buffer[0];
	u8 dbg_num = buffer[1];
	u16 content_0 = (((u16)buffer[3]) << 8) | ((u16)buffer[2]);
	u16 content_1 = (((u16)buffer[5]) << 8) | ((u16)buffer[4]);
	u16 content_2 = (((u16)buffer[7]) << 8) | ((u16)buffer[6]);
	u16 content_3 = (((u16)buffer[9]) << 8) | ((u16)buffer[8]);
	u16 content_4 = (((u16)buffer[11]) << 8) | ((u16)buffer[10]);

	if (cmd_len > 12)
		PHYDM_DBG(dm, DBG_FW_TRACE,
			  "[FW Msg] Invalid cmd length (( %d )) >12\n",
			  cmd_len);

/* PHYDM_DBG(dm, DBG_FW_TRACE,"[FW Msg] Func=((%d)),  num=((%d)), ct_0=((%d)), ct_1=((%d)), ct_2=((%d)), ct_3=((%d)), ct_4=((%d))\n", */
/*	function, dbg_num, content_0, content_1, content_2, content_3, content_4); */

/*--------------------------------------------*/
#ifdef CONFIG_RA_FW_DBG_CODE
	if (function == RATE_DECISION) {
		if (dbg_num == 0) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] RA_CNT=((%d))  Max_device=((%d))--------------------------->\n",
					  content_1, content_2);
			else if (content_0 == 2)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Check RA macid= ((%d)), MediaStatus=((%d)), Dis_RA=((%d)),  try_bit=((0x%x))\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 3)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Check RA  total=((%d)),  drop=((0x%x)), TXRPT_TRY_bit=((%x)), bNoisy=((%x))\n",
					  content_1, content_2, content_3,
					  content_4);
		} else if (dbg_num == 1) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] RTY[0,1,2,3]=[ %d , %d , %d , %d ]\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 2) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] RTY[4]=[ %d ], drop=(( %d )), total=(( %d )), current_rate=((0x %x ))",
					  content_1, content_2, content_3,
					  content_4);
				phydm_print_rate(dm, (u8)content_4, DBG_FW_TRACE);
			} else if (content_0 == 3)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] penality_idx=(( %d ))\n",
					  content_1);
			else if (content_0 == 4)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] RSSI=(( %d )), ra_stage = (( %d ))\n",
					  content_1, content_2);
		} else if (dbg_num == 3) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Fast_RA (( DOWN ))  total=((%d)),  total>>1=((%d)), R4+R3+R2 = ((%d)), RateDownHold = ((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 2)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Fast_RA (( UP ))  total_acc=((%d)),  total_acc>>1=((%d)), R4+R3+R2 = ((%d)), RateDownHold = ((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 3)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Fast_RA (( UP )) ((rate Down Hold))  RA_CNT=((%d))\n",
					  content_1);
			else if (content_0 == 4)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Fast_RA (( UP )) ((tota_accl<5 skip))  RA_CNT=((%d))\n",
					  content_1);
			else if (content_0 == 8)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] Fast_RA (( Reset Tx Rpt )) RA_CNT=((%d))\n",
					  content_1);
		} else if (dbg_num == 4) {
			if (content_0 == 3) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] RER_CNT   PCR_ori =(( %d )),  ratio_ori =(( %d )), pcr_updown_bitmap =(( 0x%x )), pcr_var_diff =(( %d ))\n",
					  content_1, content_2, content_3,
					  content_4);
				/**/
			} else if (content_0 == 4) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] pcr_shift_value =(( %s%d )), rate_down_threshold =(( %d )), rate_up_threshold =(( %d ))\n",
					  ((content_1) ? "+" : "-"), content_2,
					  content_3, content_4);
				/**/
			} else if (content_0 == 5) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] pcr_mean =(( %d )), PCR_VAR =(( %d )), offset =(( %d )), decision_offset_p =(( %d ))\n",
					  content_1, content_2, content_3,
					  content_4);
				/**/
			}
		} else if (dbg_num == 5) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] (( UP))  Nsc=(( %d )), N_High=(( %d )), RateUp_Waiting=(( %d )), RateUp_Fail=(( %d ))\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 2)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] ((DOWN))  Nsc=(( %d )), N_Low=(( %d ))\n",
					  content_1, content_2);
			else if (content_0 == 3)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] ((HOLD))  Nsc=((%d)), N_High=((%d)), N_Low=((%d)), Reset_CNT=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
		} else if (dbg_num == 0x60) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] ((AP RPT))  macid=((%d)), BUPDATE[macid]=((%d))\n",
					  content_1, content_2);
			else if (content_0 == 4)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] ((AP RPT))  pass=((%d)), rty_num=((%d)), drop=((%d)), total=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 5)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW] ((AP RPT))  PASS=((%d)), RTY_NUM=((%d)), DROP=((%d)), TOTAL=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
		}
	} else if (function == INIT_RA_TABLE) {
		if (dbg_num == 3)
			PHYDM_DBG(dm, DBG_FW_TRACE,
				  "[FW][INIT_RA_INFO] Ra_init, RA_SKIP_CNT = (( %d ))\n",
				  content_0);
	} else if (function == RATE_UP) {
		if (dbg_num == 2) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][RateUp]  ((Highest rate->return)), macid=((%d))  Nsc=((%d))\n",
					  content_1, content_2);
		} else if (dbg_num == 5) {
			if (content_0 == 0)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][RateUp]  ((rate UP)), up_rate_tmp=((0x%x)), rate_idx=((0x%x)), SGI_en=((%d)),  SGI=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
			else if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][RateUp]  ((rate UP)), rate_1=((0x%x)), rate_2=((0x%x)), BW=((%d)), Try_Bit=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
		}
	} else if (function == RATE_DOWN) {
		if (dbg_num == 5) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][RateDownStep]  ((rate Down)), macid=((%d)), rate1=((0x%x)),  rate2=((0x%x)), BW=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
		}
	} else if (function == TRY_DONE) {
		if (dbg_num == 1) {
			if (content_0 == 1) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][Try Done]  ((try succsess )) macid=((%d)), Try_Done_cnt=((%d))\n",
					  content_1, content_2);
				/**/
			}
		} else if (dbg_num == 2) {
			if (content_0 == 1) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][Try Done]  ((try)) macid=((%d)), Try_Done_cnt=((%d)),  rate_2=((%d)),  try_succes=((%d))\n",
					  content_1, content_2, content_3,
					  content_4);
				/**/
			}
		}
	} else if (function == RA_H2C) {
		if (dbg_num == 1) {
			if (content_0 == 0) {
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][H2C=0x49]  fw_trace_en=((%d)), mode =((%d)),  macid=((%d))\n",
					  content_1, content_2, content_3);
				/**/
			}
		}
	} else if (function == F_RATE_AP_RPT) {
		if (dbg_num == 1) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][AP RPT]  ((1)), SPE_STATIS=((0x%x))---------->\n",
					  content_3);
		} else if (dbg_num == 2) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][AP RPT]  RTY_all=((%d))\n",
					  content_1);
		} else if (dbg_num == 3) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][AP RPT]  MACID1[%d], TOTAL=((%d)),  RTY=((%d))\n",
					  content_3, content_1, content_2);
		} else if (dbg_num == 4) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][AP RPT]  MACID2[%d], TOTAL=((%d)),  RTY=((%d))\n",
					  content_3, content_1, content_2);
		} else if (dbg_num == 5) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][AP RPT]  MACID1[%d], PASS=((%d)),  DROP=((%d))\n",
					  content_3, content_1, content_2);
		} else if (dbg_num == 6) {
			if (content_0 == 1)
				PHYDM_DBG(dm, DBG_FW_TRACE,
					  "[FW][AP RPT]  MACID2[%d],, PASS=((%d)),  DROP=((%d))\n",
					  content_3, content_1, content_2);
		}
	} else if (function == DBC_FW_CLM) {
		PHYDM_DBG(dm, DBG_FW_TRACE,
			  "[FW][CLM][%d, %d] = {%d, %d, %d, %d}\n", dbg_num,
			  content_0, content_1, content_2, content_3,
			  content_4);
	} else {
		PHYDM_DBG(dm, DBG_FW_TRACE,
			  "[FW][general][%d, %d, %d] = {%d, %d, %d, %d}\n",
			  function, dbg_num, content_0, content_1, content_2,
			  content_3, content_4);
	}
#else
	PHYDM_DBG(dm, DBG_FW_TRACE,
		  "[FW][general][%d, %d, %d] = {%d, %d, %d, %d}\n", function,
		  dbg_num, content_0, content_1, content_2, content_3,
		  content_4);
#endif
/*--------------------------------------------*/

#endif /*#ifdef CONFIG_PHYDM_DEBUG_FUNCTION*/
}

void phydm_fw_trace_handler_8051(void *dm_void, u8 *buffer, u8 cmd_len)
{
#ifdef CONFIG_PHYDM_DEBUG_FUNCTION
	struct dm_struct *dm = (struct dm_struct *)dm_void;
#if 0
	if (cmd_len >= 3)
		cmd_buf[cmd_len - 1] = '\0';
	PHYDM_DBG(dm, DBG_FW_TRACE, "[FW DBG Msg] %s\n", &(cmd_buf[3]));
#else

	int i = 0;
	u8 extend_c2h_sub_id = 0, extend_c2h_dbg_len = 0, extend_c2h_dbg_seq = 0;
	u8 fw_debug_trace[128];
	u8 *extend_c2h_dbg_content = 0;

	if (cmd_len > 127)
		return;

	extend_c2h_sub_id = buffer[0];
	extend_c2h_dbg_len = buffer[1];
	extend_c2h_dbg_content = buffer + 2; /*DbgSeq+DbgContent  for show HEX*/

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	RT_DISP(FC2H, C2H_Summary, ("[Extend C2H packet], Extend_c2hSubId=0x%x, extend_c2h_dbg_len=%d\n",
				    extend_c2h_sub_id, extend_c2h_dbg_len));

	RT_DISP_DATA(FC2H, C2H_Summary, "[Extend C2H packet], Content Hex:", extend_c2h_dbg_content, cmd_len - 2);
#endif

go_backfor_aggre_dbg_pkt:
	i = 0;
	extend_c2h_dbg_seq = buffer[2];
	extend_c2h_dbg_content = buffer + 3;

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	RT_DISP(FC2H, C2H_Summary, ("[RTKFW, SEQ= %d] :", extend_c2h_dbg_seq));
#endif

	for (;; i++) {
		fw_debug_trace[i] = extend_c2h_dbg_content[i];
		if (extend_c2h_dbg_content[i + 1] == '\0') {
			fw_debug_trace[i + 1] = '\0';
			PHYDM_DBG(dm, DBG_FW_TRACE, "[FW DBG Msg] %s",
				  &fw_debug_trace[0]);
			break;
		} else if (extend_c2h_dbg_content[i] == '\n') {
			fw_debug_trace[i + 1] = '\0';
			PHYDM_DBG(dm, DBG_FW_TRACE, "[FW DBG Msg] %s",
				  &fw_debug_trace[0]);
			buffer = extend_c2h_dbg_content + i + 3;
			goto go_backfor_aggre_dbg_pkt;
		}
	}

#endif
#endif /*#ifdef CONFIG_PHYDM_DEBUG_FUNCTION*/
}
