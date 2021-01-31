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

#ifndef __PHYDM_API_H__
#define __PHYDM_API_H__

#define PHYDM_API_VERSION "1.0" /* 2017.07.10  Dino, Add phydm_api.h*/

/* 1 ============================================================
 * 1  Definition
 * 1 ============================================================
 */

#define FUNC_ENABLE 1
#define FUNC_DISABLE 2

/*NBI API------------------------------------*/
#define NBI_TABLE_SIZE_128 27
#define NBI_TABLE_SIZE_256 59

#define NUM_START_CH_80M 7
#define NUM_START_CH_40M 14

#define CH_OFFSET_40M 2
#define CH_OFFSET_80M 6

#define FFT_128_TYPE 1
#define FFT_256_TYPE 2

#define FREQ_POSITIVE 1
#define FREQ_NEGATIVE 2
/*------------------------------------------------*/

#ifndef PHYDM_COMMON_API_SUPPORT
#define INVALID_RF_DATA 0xffffffff
#define INVALID_TXAGC_DATA 0xff
#endif

/* 1 ============================================================
 * 1  structure
 * 1 ============================================================
 */

struct phydm_api_stuc {
	u32 rx_iqc_reg_1; /*N-mode: for pathA REG0xc14*/
	u32 rx_iqc_reg_2; /*N-mode: for pathB REG0xc1c*/
	u8 tx_queue_bitmap; /*REG0x520[23:16]*/
};

/* 1 ============================================================
 * 1  enumeration
 * 1 ============================================================
 */

/* 1 ============================================================
 * 1  function prototype
 * 1 ============================================================
 */

void phydm_dynamic_ant_weighting(void *dm_void);

#ifdef DYN_ANT_WEIGHTING_SUPPORT
void phydm_dyn_ant_weight_dbg(
	void *dm_void,
	char input[][16],
	u32 *_used,
	char *output,
	u32 *_out_len,
	u32 input_num);
#endif

void phydm_pathb_q_matrix_rotate_en(void *dm_void);

void phydm_pathb_q_matrix_rotate(void *dm_void, u16 phase_idx);

void phydm_init_trx_antenna_setting(void *dm_void);

void phydm_config_ofdm_rx_path(void *dm_void, u32 path);

void phydm_config_cck_rx_path(void *dm_void, enum bb_path path);

void phydm_config_cck_rx_antenna_init(void *dm_void);

void phydm_config_trx_path(void *dm_void, u32 *const dm_value, u32 *_used,
			   char *output, u32 *_out_len);

void phydm_tx_2path(void *dm_void);

void phydm_stop_3_wire(void *dm_void, u8 set_type);

u8 phydm_stop_ic_trx(void *dm_void, u8 set_type);

void phydm_set_ext_switch(void *dm_void, u32 *const dm_value, u32 *_used,
			  char *output, u32 *_out_len);

void phydm_nbi_enable(void *dm_void, u32 enable);

u8 phydm_csi_mask_setting(void *dm_void, u32 enable, u32 channel, u32 bw,
			  u32 f_interference, u32 second_ch);

u8 phydm_nbi_setting(void *dm_void, u32 enable, u32 channel, u32 bw,
		     u32 f_interference, u32 second_ch);

void phydm_api_debug(void *dm_void, u32 function_map, u32 *const dm_value,
		     u32 *_used, char *output, u32 *_out_len);

void phydm_stop_ck320(void *dm_void, u8 enable);

boolean
phydm_set_bb_txagc_offset(void *dm_void, s8 power_offset, u8 add_half_db);

#ifdef PHYDM_COMMON_API_SUPPORT

boolean
phydm_api_set_txagc(void *dm_void, u32 power_index, enum rf_path path,
		    u8 hw_rate, boolean is_single_rate);

u8 phydm_api_get_txagc(void *dm_void, enum rf_path path, u8 hw_rate);

boolean
phydm_api_switch_bw_channel(void *dm_void, u8 central_ch, u8 primary_ch_idx,
			    enum channel_width bandwidth);

boolean
phydm_api_trx_mode(void *dm_void, enum bb_path tx_path, enum bb_path rx_path,
		   boolean is_tx2_path);

#endif

#endif
