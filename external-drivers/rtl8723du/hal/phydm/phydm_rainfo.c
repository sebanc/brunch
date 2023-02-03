// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */
#include "mp_precomp.h"
#include "phydm_precomp.h"

void
phydm_h2C_debug(
	void		*p_dm_void,
	u32		*const dm_value,
	u32		*_used,
	char		*output,
	u32		*_out_len
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8			h2c_parameter[H2C_MAX_LENGTH] = {0};
	u8			phydm_h2c_id = (u8)dm_value[0];
	u8			i;
	u32			used = *_used;
	u32			out_len = *_out_len;

	PHYDM_SNPRINTF((output + used, out_len - used, "Phydm Send H2C_ID (( 0x%x))\n", phydm_h2c_id));
	for (i = 0; i < H2C_MAX_LENGTH; i++) {

		h2c_parameter[i] = (u8)dm_value[i + 1];
		PHYDM_SNPRINTF((output + used, out_len - used, "H2C: Byte[%d] = ((0x%x))\n", i, h2c_parameter[i]));
	}

	odm_fill_h2c_cmd(p_dm, phydm_h2c_id, H2C_MAX_LENGTH, h2c_parameter);
	
	*_used = used;
	*_out_len = out_len;
}

static void
phydm_fw_fix_rate(
	void		*p_dm_void,
	u8		en, 
	u8		macid, 
	u8		bw, 
	u8		rate
	
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u32	reg_u32_tmp;

	if (p_dm->support_ic_type & PHYDM_IC_8051_SERIES) {
		
		reg_u32_tmp = (bw << 24) | (rate << 16) | (macid << 8) | en;
		odm_set_bb_reg(p_dm, 0x4a0, MASKDWORD, reg_u32_tmp);
			
	} else {
	
		if (en == 1)
			reg_u32_tmp = (0x60 << 24) | (macid << 16) | (bw << 8) | rate;
		else
			reg_u32_tmp = 0x40000000;
			
		odm_set_bb_reg(p_dm, 0x450, MASKDWORD, reg_u32_tmp);
	}
	if (en == 1) {
		PHYDM_DBG(p_dm, ODM_COMP_API, ("FW fix TX rate[id =%d], %dM, Rate(%d)=", macid, (20 << bw), rate));
		phydm_print_rate(p_dm, rate, ODM_COMP_API);
	} else {
		PHYDM_DBG(p_dm, ODM_COMP_API, ("Auto Rate\n"));
	}
}

void
phydm_ra_debug(
	void		*p_dm_void,
	char		input[][16],
	u32		*_used,
	char		*output,
	u32		*_out_len
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_			*p_ra_table = &p_dm->dm_ra_table;
	u32 used = *_used;
	u32 out_len = *_out_len;
	char	help[] = "-h";
	u32	var1[5] = {0};
	u8	i;

	for (i = 0; i < 5; i++) {
		PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &var1[i]);
	}
	if ((strcmp(input[1], help) == 0)) {
		PHYDM_SNPRINTF((output + used, out_len - used, "{1} {0:-,1:+} {ofst}: set offset\n"));
		PHYDM_SNPRINTF((output + used, out_len - used, "{1} {100}: show offset\n"));
		PHYDM_SNPRINTF((output + used, out_len - used, "{2} {en} {macid} {bw} {rate}: fw fix rate\n"));
		
	} else if (var1[0] == 1) { /*Adjust PCR offset*/

		if (var1[1] == 100) {
			PHYDM_SNPRINTF((output + used, out_len - used, "[Get] RA_ofst=((%s%d))\n", 
				((p_ra_table->RA_threshold_offset == 0) ? " " : ((p_ra_table->RA_offset_direction) ? "+" : "-")), p_ra_table->RA_threshold_offset));

		} else if (var1[1] == 0) {
			p_ra_table->RA_offset_direction = 0;
			p_ra_table->RA_threshold_offset = (u8)var1[2];
			PHYDM_SNPRINTF((output + used, out_len - used, "[Set] RA_ofst=((-%d))\n", p_ra_table->RA_threshold_offset));
		} else if (var1[1] == 1) {
			p_ra_table->RA_offset_direction = 1;
			p_ra_table->RA_threshold_offset = (u8)var1[2];
			PHYDM_SNPRINTF((output + used, out_len - used, "[Set] RA_ofst=((+%d))\n", p_ra_table->RA_threshold_offset));
		}
		
	} else if (var1[0] == 2) { /*FW fix rate*/

		PHYDM_SNPRINTF((output + used, out_len - used, 
			"[FW fix TX Rate] {en, macid,bw,rate}={%d, %d, %d, 0x%x}", var1[1], var1[2], var1[3], var1[4]));
		
		phydm_fw_fix_rate(p_dm, (u8)var1[1], (u8)var1[2], (u8)var1[3], (u8)var1[4]);
		
	} else {
		PHYDM_SNPRINTF((output + used, out_len - used, "[Set] Error\n"));
		/**/
	}
	*_used = used;
	*_out_len = out_len;
}



void
odm_c2h_ra_para_report_handler(
	void	*p_dm_void,
	u8	*cmd_buf,
	u8	cmd_len
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	para_idx = cmd_buf[0]; /*Retry Penalty, NH, NL*/
	u8	i;


	PHYDM_DBG(p_dm, DBG_RA, ("[ From FW C2H RA Para ]  cmd_buf[0]= (( %d ))\n", cmd_buf[0]));

		if (para_idx == RADBG_DEBUG_MONITOR1) {
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("-------------------------------\n"));
			if (p_dm->support_ic_type & PHYDM_IC_3081_SERIES) {

				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "RSSI =", cmd_buf[1]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "rate =", cmd_buf[2] & 0x7f));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "SGI =", (cmd_buf[2] & 0x80) >> 7));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "BW =", cmd_buf[3]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "BW_max =", cmd_buf[4]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "multi_rate0 =", cmd_buf[5]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "multi_rate1 =", cmd_buf[6]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "DISRA =",	cmd_buf[7]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "VHT_EN =", cmd_buf[8]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "SGI_support =",	cmd_buf[9]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "try_ness =", cmd_buf[10]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "pre_rate =", cmd_buf[11]));
			} else {
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "RSSI =", cmd_buf[1]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %x\n", "BW =", cmd_buf[2]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "DISRA =", cmd_buf[3]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "VHT_EN =", cmd_buf[4]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "Hightest rate =", cmd_buf[5]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "Lowest rate =", cmd_buf[6]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "SGI_support =", cmd_buf[7]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "Rate_ID =",	cmd_buf[8]));
			}
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("-------------------------------\n"));
		} else	 if (para_idx == RADBG_DEBUG_MONITOR2) {
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("-------------------------------\n"));
			if (p_dm->support_ic_type & PHYDM_IC_3081_SERIES) {
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "rate_id =", cmd_buf[1]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "highest_rate =", cmd_buf[2]));
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "lowest_rate =", cmd_buf[3]));

				for (i = 4; i <= 11; i++)
					PHYDM_DBG(p_dm, DBG_FW_TRACE, ("RAMASK =  0x%x\n", cmd_buf[i]));
			} else {
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %x%x  %x%x  %x%x  %x%x\n", "RA Mask:",
					cmd_buf[8], cmd_buf[7], cmd_buf[6], cmd_buf[5], cmd_buf[4], cmd_buf[3], cmd_buf[2], cmd_buf[1]));
			}
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("-------------------------------\n"));
		} else	 if (para_idx == RADBG_DEBUG_MONITOR3) {

			for (i = 0; i < (cmd_len - 1); i++)
				PHYDM_DBG(p_dm, DBG_FW_TRACE, ("content[%d] = %d\n", i, cmd_buf[1 + i]));
		} else	 if (para_idx == RADBG_DEBUG_MONITOR4)
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  {%d.%d}\n", "RA version =", cmd_buf[1], cmd_buf[2]));
		else if (para_idx == RADBG_DEBUG_MONITOR5) {
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "Current rate =", cmd_buf[1]));
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "Retry ratio =", cmd_buf[2]));
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  %d\n", "rate down ratio =", cmd_buf[3]));
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x\n", "highest rate =", cmd_buf[4]));
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  {0x%x 0x%x}\n", "Muti-try =", cmd_buf[5], cmd_buf[6]));
			PHYDM_DBG(p_dm, DBG_FW_TRACE, ("%5s  0x%x%x%x%x%x\n", "RA mask =", cmd_buf[11], cmd_buf[10], cmd_buf[9], cmd_buf[8], cmd_buf[7]));
		}
}

void
phydm_ra_dynamic_retry_count(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (!(p_dm->support_ability & ODM_BB_DYNAMIC_ARFR))
		return;

	/*PHYDM_DBG(p_dm, DBG_RA, ("p_dm->pre_b_noisy = %d\n", p_dm->pre_b_noisy ));*/
	if (p_dm->pre_b_noisy != p_dm->noisy_decision) {

		if (p_dm->noisy_decision) {
			PHYDM_DBG(p_dm, DBG_RA, ("Noisy Env. RA fallback\n"));
			odm_set_mac_reg(p_dm, 0x430, MASKDWORD, 0x0);
			odm_set_mac_reg(p_dm, 0x434, MASKDWORD, 0x04030201);
		} else {
			PHYDM_DBG(p_dm, DBG_RA, ("Clean Env. RA fallback\n"));
			odm_set_mac_reg(p_dm, 0x430, MASKDWORD, 0x01000000);
			odm_set_mac_reg(p_dm, 0x434, MASKDWORD, 0x06050402);
		}
		p_dm->pre_b_noisy = p_dm->noisy_decision;
	}
}

void
phydm_print_rate(
	void	*p_dm_void,
	u8	rate,
	u32	dbg_component
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8		legacy_table[12] = {1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54};
	u8		rate_idx = rate & 0x7f; /*remove bit7 SGI*/
	u8		vht_en = (rate_idx >= ODM_RATEVHTSS1MCS0) ? 1 : 0;
	u8		b_sgi = (rate & 0x80) >> 7;

	PHYDM_DBG_F(p_dm, dbg_component, ("( %s%s%s%s%d%s%s)\n",
		((rate_idx >= ODM_RATEVHTSS1MCS0) && (rate_idx <= ODM_RATEVHTSS1MCS9)) ? "VHT 1ss  " : "",
		((rate_idx >= ODM_RATEVHTSS2MCS0) && (rate_idx <= ODM_RATEVHTSS2MCS9)) ? "VHT 2ss " : "",
		((rate_idx >= ODM_RATEVHTSS3MCS0) && (rate_idx <= ODM_RATEVHTSS3MCS9)) ? "VHT 3ss " : "",
			(rate_idx >= ODM_RATEMCS0) ? "MCS " : "",
		(vht_en) ? ((rate_idx - ODM_RATEVHTSS1MCS0) % 10) : ((rate_idx >= ODM_RATEMCS0) ? (rate_idx - ODM_RATEMCS0) : ((rate_idx <= ODM_RATE54M) ? legacy_table[rate_idx] : 0)),
			(b_sgi) ? "-S" : "  ",
			(rate_idx >= ODM_RATEMCS0) ? "" : "M"));
}

void
phydm_c2h_ra_report_handler(
	void	*p_dm_void,
	u8   *cmd_buf,
	u8   cmd_len
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_		*p_ra_table = &p_dm->dm_ra_table;
	u8	macid = cmd_buf[1];
	u8	rate = cmd_buf[0];
	u8	rate_idx = rate & 0x7f; /*remove bit7 SGI*/
	u8	rate_order;
	struct cmn_sta_info			*p_sta = p_dm->p_phydm_sta_info[macid];

	if (cmd_len >=6) {
		p_ra_table->ra_ratio[macid] = cmd_buf[6];
		PHYDM_DBG(p_dm, DBG_RA, ("RA retry ratio: [%d]:", p_ra_table->ra_ratio[macid]));
			/**/
	} else if (cmd_len >= 4) {
		if (cmd_buf[3] == 0) {
			PHYDM_DBG(p_dm, DBG_RA, ("TX Init-rate Update[%d]:", macid));
			/**/
		} else if (cmd_buf[3] == 0xff) {
			PHYDM_DBG(p_dm, DBG_RA, ("FW Level: Fix rate[%d]:", macid));
			/**/
		} else if (cmd_buf[3] == 1) {
			PHYDM_DBG(p_dm, DBG_RA, ("Try Success[%d]:", macid));
			/**/
		} else if (cmd_buf[3] == 2) {
			PHYDM_DBG(p_dm, DBG_RA, ("Try Fail & Try Again[%d]:", macid));
			/**/
		} else if (cmd_buf[3] == 3) {
			PHYDM_DBG(p_dm, DBG_RA, ("rate Back[%d]:", macid));
			/**/
		} else if (cmd_buf[3] == 4) {
			PHYDM_DBG(p_dm, DBG_RA, ("start rate by RSSI[%d]:", macid));
			/**/
		} else if (cmd_buf[3] == 5) {
			PHYDM_DBG(p_dm, DBG_RA, ("Try rate[%d]:", macid));
			/**/
		}
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("Tx rate Update[%d]:", macid));
		/**/
	}

	/*phydm_print_rate(p_dm, pre_rate_idx, DBG_RA);*/
	/*PHYDM_DBG(p_dm, DBG_RA, (">\n",macid );*/
	phydm_print_rate(p_dm, rate, DBG_RA);
	if (macid >= 128) {
		u8 gid_index = macid - 128;
		p_ra_table->mu1_rate[gid_index] = rate;
	}
	
	/*p_ra_table->link_tx_rate[macid] = rate;*/
		
	if (is_sta_active(p_sta)) {
		p_sta->ra_info.curr_tx_rate = rate;
		/**/
	}

	/*trigger power training*/

	rate_order = phydm_rate_order_compute(p_dm, rate_idx);

	if ((p_dm->is_one_entry_only) ||
		((rate_order > p_ra_table->highest_client_tx_order) && (p_ra_table->power_tracking_flag == 1))
		) {
		halrf_update_pwr_track(p_dm, rate_idx);
		p_ra_table->power_tracking_flag = 0;
	}
}

void
odm_ra_post_action_on_assoc(
	void	*p_dm_void
)
{
}

void
phydm_modify_RA_PCR_threshold(
	void		*p_dm_void,
	u8		RA_offset_direction,
	u8		RA_threshold_offset

)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_			*p_ra_table = &p_dm->dm_ra_table;

	p_ra_table->RA_offset_direction = RA_offset_direction;
	p_ra_table->RA_threshold_offset = RA_threshold_offset;
	PHYDM_DBG(p_dm, DBG_RA, ("Set RA_threshold_offset = (( %s%d ))\n", ((RA_threshold_offset == 0) ? " " : ((RA_offset_direction) ? "+" : "-")), RA_threshold_offset));
}

static void
phydm_rate_adaptive_mask_init(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_t = &p_dm->dm_ra_table;

	p_ra_t->ldpc_thres = 35;
	p_ra_t->up_ramask_cnt = 0;
	p_ra_t->up_ramask_cnt_tmp = 0;

}

void
phydm_refresh_rate_adaptive_mask(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_t = &p_dm->dm_ra_table;

	PHYDM_DBG(p_dm, DBG_RA_MASK, ("%s ======>\n", __func__));

	if (!(p_dm->support_ability & ODM_BB_RA_MASK)) {
		PHYDM_DBG(p_dm, DBG_RA_MASK, ("Return: Not support\n"));
		return;
	}

	if (!p_dm->is_linked)
		return;

	p_ra_t->up_ramask_cnt++;
	/*p_ra_t->up_ramask_cnt_tmp++;*/
	
	phydm_ra_mask_watchdog(p_dm);
}

void
phydm_show_sta_info(
	void		*p_dm_void,
	char		input[][16],
	u32		*_used,
	char		*output,
	u32		*_out_len,
	u32		input_num
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct cmn_sta_info		*p_sta = NULL;
	struct ra_sta_info			*p_ra = NULL;
	char		help[] = "-h";
	u32		var1[10] = {0};
	u32		used = *_used;
	u32		out_len = *_out_len;
	u32		i, macid_start, macid_end;
	u8		tatal_sta_num = 0;

	PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

	if ((strcmp(input[1], help) == 0)) {
		PHYDM_SNPRINTF((output + used, out_len - used, "All STA: {1}\n"));
		PHYDM_SNPRINTF((output + used, out_len - used, "STA[macid]: {2} {macid}\n"));
		return;
	} else if (var1[0] == 1) {
		macid_start = 0;
		macid_end = ODM_ASSOCIATE_ENTRY_NUM;
	} else if (var1[0] == 2) {
		macid_start = var1[1];
		macid_end = var1[1];
	} else {
		PHYDM_SNPRINTF((output + used, out_len - used, "Warning input value!\n"));
		return;
	}
		
	for (i = macid_start; i < macid_end; i++) {
		
		p_sta = p_dm->p_phydm_sta_info[i];


		if (!is_sta_active(p_sta))
			continue;

		p_ra = &(p_sta->ra_info);

		tatal_sta_num++;

		PHYDM_SNPRINTF((output + used, out_len - used, "==[MACID: %d]============>\n", p_sta->mac_id));
		PHYDM_SNPRINTF((output + used, out_len - used, "AID:%d\n", p_sta->aid));
		PHYDM_SNPRINTF((output + used, out_len - used, "ADDR:%x-%x-%x-%x-%x-%x\n", 
		p_sta->mac_addr[5], p_sta->mac_addr[4], p_sta->mac_addr[3], p_sta->mac_addr[2], p_sta->mac_addr[1], p_sta->mac_addr[0]));
		PHYDM_SNPRINTF((output + used, out_len - used, "DM_ctrl:0x%x\n", p_sta->dm_ctrl));
		PHYDM_SNPRINTF((output + used, out_len - used, "BW:%d, MIMO_Type:0x%x\n", p_sta->bw_mode, p_sta->mimo_type));
		PHYDM_SNPRINTF((output + used, out_len - used, "STBC_en:%d, LDPC_en=%d\n", p_sta->stbc_en, p_sta->ldpc_en));

		/*[RSSI Info]*/
		PHYDM_SNPRINTF((output + used, out_len - used, "RSSI{All, OFDM, CCK}={%d, %d, %d}\n", 
			p_sta->rssi_stat.rssi, p_sta->rssi_stat.rssi_ofdm, p_sta->rssi_stat.rssi_cck));

		/*[RA Info]*/
		PHYDM_SNPRINTF((output + used, out_len - used, "Rate_ID:%d, RSSI_LV:%d, ra_bw:%d, SGI_en:%d\n", 
			p_ra->rate_id, p_ra->rssi_level, p_ra->ra_bw_mode, p_ra->is_support_sgi));

		PHYDM_SNPRINTF((output + used, out_len - used, "VHT_en:%d, Wireless_set=0x%x, sm_ps=%d\n", 
			p_ra->is_vht_enable, p_sta->support_wireless_set, p_sta->sm_ps));

		PHYDM_SNPRINTF((output + used, out_len - used, "Dis{RA, PT}={%d, %d}, TxRx:%d, Noisy:%d\n", 
			p_ra->disable_ra, p_ra->disable_pt, p_ra->txrx_state, p_ra->is_noisy));
		
		PHYDM_SNPRINTF((output + used, out_len - used, "TX{Rate, BW}={%d, %d}, RTY:%d\n", 
			p_ra->curr_tx_rate, p_ra->curr_tx_bw, p_ra->curr_retry_ratio));
	
		PHYDM_SNPRINTF((output + used, out_len - used, "RA_MAsk:0x%llx\n", p_ra->ramask));
		
		/*[TP]*/
		PHYDM_SNPRINTF((output + used, out_len - used, "TP{TX,RX}={%d, %d}\n", 
			p_sta->tx_moving_average_tp, p_sta->rx_moving_average_tp));
	}

	if (tatal_sta_num == 0) {
		PHYDM_SNPRINTF((output + used, out_len - used, "No Linked STA\n"));
	}
	
	*_used = used;
	*_out_len = out_len;
}

static u8
phydm_get_tx_stream_num(
	void		*p_dm_void,
	enum 	rf_type	mimo_type
	
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	tx_num = 1;

	if (mimo_type == RF_1T1R || mimo_type == RF_1T2R)
		tx_num = 1;
	else if (mimo_type == RF_2T2R || mimo_type == RF_2T3R  || mimo_type == RF_2T4R)
		tx_num = 2;
	else if (mimo_type == RF_3T3R || mimo_type == RF_3T4R)
		tx_num = 3;
	else if (mimo_type == RF_4T4R)
		tx_num = 4;
	else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warrning] no mimo_type is found\n"));
	}
	return tx_num;
}

static u64
phydm_get_bb_mod_ra_mask(
	void		*p_dm_void,
	u8		macid
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct cmn_sta_info		*p_sta = p_dm->p_phydm_sta_info[macid];
	struct ra_sta_info			*p_ra = NULL;
	enum channel_width		bw = 0;
	enum wireless_set			wireless_mode = 0;
	u8		tx_stream_num = 1;
	u8		rssi_lv = 0;
	u64		ra_mask_bitmap = 0;
	
	if (is_sta_active(p_sta)) {
		
		p_ra = &(p_sta->ra_info);
		bw = p_ra->ra_bw_mode;
		wireless_mode = p_sta->support_wireless_set;
		tx_stream_num = phydm_get_tx_stream_num(p_dm, p_sta->mimo_type);
		rssi_lv = p_ra->rssi_level;
		ra_mask_bitmap = p_ra->ramask;
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warning] %s invalid sta_info\n", __func__));
		return 0;
	}

	PHYDM_DBG(p_dm, DBG_RA, ("macid=%d ori_RA_Mask= 0x%llx\n", macid, ra_mask_bitmap));
	PHYDM_DBG(p_dm, DBG_RA, ("wireless_mode=0x%x, tx_stream_num=%d, BW=%d, MimoPs=%d, rssi_lv=%d\n",
		wireless_mode, tx_stream_num, bw, p_sta->sm_ps, rssi_lv));
	
	if (p_sta->sm_ps == SM_PS_STATIC) /*mimo_ps_enable*/
		tx_stream_num = 1;


	/*[Modify RA Mask by Wireless Mode]*/

	if (wireless_mode == WIRELESS_CCK)								/*B mode*/
		ra_mask_bitmap &= 0x0000000f;
	else if (wireless_mode == WIRELESS_OFDM)							/*G mode*/
		ra_mask_bitmap &= 0x00000ff0;
	else if (wireless_mode == (WIRELESS_CCK | WIRELESS_OFDM))			/*BG mode*/
		ra_mask_bitmap &= 0x00000ff5;
	else if (wireless_mode == (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_HT)) {
																	/*N_2G*/
		if (tx_stream_num == 1) {
			if (bw == CHANNEL_WIDTH_40)
				ra_mask_bitmap &= 0x000ff015;
			else
				ra_mask_bitmap &= 0x000ff005;
		} else if (tx_stream_num == 2) {

			if (bw == CHANNEL_WIDTH_40)
				ra_mask_bitmap &= 0x0ffff015;
			else
				ra_mask_bitmap &= 0x0ffff005;
		} else if (tx_stream_num == 3)
			ra_mask_bitmap &= 0xffffff015L;
	} else if (wireless_mode ==  (WIRELESS_OFDM | WIRELESS_HT)) {		/*N_5G*/
	
		if (tx_stream_num == 1) {
			if (bw == CHANNEL_WIDTH_40)
				ra_mask_bitmap &= 0x000ff030;
			else
				ra_mask_bitmap &= 0x000ff010;
		} else if (tx_stream_num == 2) {

			if (bw == CHANNEL_WIDTH_40)
				ra_mask_bitmap &= 0x0ffff030;
			else
				ra_mask_bitmap &= 0x0ffff010;
		} else if (tx_stream_num == 3)
			ra_mask_bitmap &= 0xffffff010L;
	} else if (wireless_mode ==  (WIRELESS_CCK |WIRELESS_OFDM | WIRELESS_VHT)) {
																	/*AC_2G*/
		if (tx_stream_num == 1)
			ra_mask_bitmap &= 0x003ff015;
		else if (tx_stream_num == 2)
			ra_mask_bitmap &= 0xfffff015;
		else if (tx_stream_num == 3)
			ra_mask_bitmap &= 0x3fffffff010L;
		

		if (bw == CHANNEL_WIDTH_20) {/* AC 20MHz doesn't support MCS9 */
			ra_mask_bitmap &= 0x1ff7fdfffffL;
		}
	} else if (wireless_mode ==  (WIRELESS_OFDM | WIRELESS_VHT)) {		/*AC_5G*/
	
		if (tx_stream_num == 1)
			ra_mask_bitmap &= 0x003ff010L;
		else if (tx_stream_num == 2)
			ra_mask_bitmap &= 0xfffff010L;
		else  if (tx_stream_num == 3)
			ra_mask_bitmap &= 0x3fffffff010L;

		if (bw == CHANNEL_WIDTH_20) /* AC 20MHz doesn't support MCS9 */
			ra_mask_bitmap &= 0x1ff7fdfffffL;
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warrning] No RA mask is found\n"));
		/**/
	}
	
	PHYDM_DBG(p_dm, DBG_RA, ("Mod by mode=0x%llx\n", ra_mask_bitmap));

	
	/*[Modify RA Mask by RSSI level]*/
	if (wireless_mode != WIRELESS_CCK) {

		if (rssi_lv == 0)
			ra_mask_bitmap &=  0xffffffff;
		else if (rssi_lv == 1)
			ra_mask_bitmap &=  0xfffffff0;
		else if (rssi_lv == 2)
			ra_mask_bitmap &=  0xffffefe0;
		else if (rssi_lv == 3)
			ra_mask_bitmap &=  0xffffcfc0;
		else if (rssi_lv == 4)
			ra_mask_bitmap &=  0xffff8f80;
		else if (rssi_lv >= 5)
			ra_mask_bitmap &=  0xffff0f00;

	}
	PHYDM_DBG(p_dm, DBG_RA, ("Mod by RSSI=0x%llx\n", ra_mask_bitmap));

	return ra_mask_bitmap;
}

static u8
phydm_get_rate_id(
	void			*p_dm_void,
	u8			macid
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct cmn_sta_info		*p_sta = p_dm->p_phydm_sta_info[macid];
	struct ra_sta_info			*p_ra =NULL;
	enum channel_width		bw = 0;
	enum wireless_set			wireless_mode = 0;
	u8	tx_stream_num = 1;
	u8	rate_id_idx = PHYDM_BGN_20M_1SS;

	if (is_sta_active(p_sta)) {
		
		p_ra = &(p_sta->ra_info);
		bw = p_ra->ra_bw_mode;
		wireless_mode = p_sta->support_wireless_set;
		tx_stream_num = phydm_get_tx_stream_num(p_dm, p_sta->mimo_type);

	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warning] %s: invalid sta_info\n", __func__));
		return 0;
	}

	PHYDM_DBG(p_dm, DBG_RA, ("macid=%d, wireless_set=0x%x, tx_stream_num=%d, BW=0x%x\n",
			macid, wireless_mode, tx_stream_num, bw));

	if (wireless_mode == WIRELESS_CCK)								/*B mode*/
		rate_id_idx = PHYDM_B_20M;
	else if (wireless_mode ==  WIRELESS_OFDM)						/*G mode*/
		rate_id_idx = PHYDM_G;
	else if (wireless_mode ==  (WIRELESS_CCK | WIRELESS_OFDM))			/*BG mode*/
		rate_id_idx = PHYDM_BG;
	else if (wireless_mode ==  (WIRELESS_OFDM | WIRELESS_HT)) {		/*GN mode*/
	
		if (tx_stream_num == 1)
			rate_id_idx = PHYDM_GN_N1SS;
		else if (tx_stream_num == 2)
			rate_id_idx = PHYDM_GN_N2SS;
		else if (tx_stream_num == 3)
			rate_id_idx = PHYDM_ARFR5_N_3SS;
	} else if (wireless_mode == (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_HT)) {	/*BGN mode*/
	

		if (bw == CHANNEL_WIDTH_40) {

			if (tx_stream_num == 1)
				rate_id_idx = PHYDM_BGN_40M_1SS;
			else if (tx_stream_num == 2)
				rate_id_idx = PHYDM_BGN_40M_2SS;
			else if (tx_stream_num == 3)
				rate_id_idx = PHYDM_ARFR5_N_3SS;

		} else {

			if (tx_stream_num == 1)
				rate_id_idx = PHYDM_BGN_20M_1SS;
			else if (tx_stream_num == 2)
				rate_id_idx = PHYDM_BGN_20M_2SS;
			else if (tx_stream_num == 3)
				rate_id_idx = PHYDM_ARFR5_N_3SS;
		}
	} else if (wireless_mode == (WIRELESS_OFDM | WIRELESS_VHT)) {	/*AC mode*/
	
		if (tx_stream_num == 1)
			rate_id_idx = PHYDM_ARFR1_AC_1SS;
		else if (tx_stream_num == 2)
			rate_id_idx = PHYDM_ARFR0_AC_2SS;
		else if (tx_stream_num == 3)
			rate_id_idx = PHYDM_ARFR4_AC_3SS;
	} else if (wireless_mode == (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_VHT)) {	/*AC 2.4G mode*/
	
		if (bw >= CHANNEL_WIDTH_80) {
			if (tx_stream_num == 1)
				rate_id_idx = PHYDM_ARFR1_AC_1SS;
			else if (tx_stream_num == 2)
				rate_id_idx = PHYDM_ARFR0_AC_2SS;
			else if (tx_stream_num == 3)
				rate_id_idx = PHYDM_ARFR4_AC_3SS;
		} else {

			if (tx_stream_num == 1)
				rate_id_idx = PHYDM_ARFR2_AC_2G_1SS;
			else if (tx_stream_num == 2)
				rate_id_idx = PHYDM_ARFR3_AC_2G_2SS;
			else if (tx_stream_num == 3)
				rate_id_idx = PHYDM_ARFR4_AC_3SS;
		}
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warrning] No rate_id is found\n"));
		rate_id_idx = 0;
	}
	
	PHYDM_DBG(p_dm, DBG_RA, ("Rate_ID=((0x%x))\n", rate_id_idx));

	return rate_id_idx;
}

static void
phydm_ra_h2c(
	void	*p_dm_void,
	u8	macid,
	u8	dis_ra,
	u8	dis_pt,
	u8	no_update_bw,
	u8	init_ra_lv,
	u64	ra_mask
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct cmn_sta_info			*p_sta = p_dm->p_phydm_sta_info[macid];
	struct ra_sta_info				*p_ra = NULL;
	u8		h2c_val[H2C_MAX_LENGTH] = {0};

	if (is_sta_active(p_sta)) {
		p_ra = &(p_sta->ra_info);
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warning] %s invalid sta_info\n", __func__));
		return;
	}
	
	PHYDM_DBG(p_dm, DBG_RA, ("%s ======>\n", __func__));
	PHYDM_DBG(p_dm, DBG_RA, ("MACID=%d\n", p_sta->mac_id));

	if (p_dm->is_disable_power_training)
		dis_pt = true;
	else if (!p_dm->is_disable_power_training)
		dis_pt = false;

	h2c_val[0] = p_sta->mac_id;
	h2c_val[1] = (p_ra->rate_id & 0x1f) | ((init_ra_lv & 0x3) << 5) | (p_ra->is_support_sgi << 7);
	h2c_val[2] = (u8)((p_ra->ra_bw_mode) | (((p_sta->ldpc_en) ? 1 : 0) << 2) | 
					((no_update_bw & 0x1) << 3) | (p_ra->is_vht_enable << 4) | 
					((dis_pt & 0x1) << 6) | ((dis_ra & 0x1) << 7));
	
	h2c_val[3] = (u8)(ra_mask & 0xff);
	h2c_val[4] = (u8)((ra_mask & 0xff00) >> 8);
	h2c_val[5] = (u8)((ra_mask & 0xff0000) >> 16);
	h2c_val[6] = (u8)((ra_mask & 0xff000000) >> 24);

	PHYDM_DBG(p_dm, DBG_RA, ("PHYDM h2c[0x40]=0x%x %x %x %x %x %x %x\n",
		h2c_val[6], h2c_val[5], h2c_val[4], h2c_val[3], h2c_val[2], h2c_val[1], h2c_val[0]));

	odm_fill_h2c_cmd(p_dm, PHYDM_H2C_RA_MASK, H2C_MAX_LENGTH, h2c_val);

	#if (defined(PHYDM_COMPILE_ABOVE_3SS))
	if (p_dm->support_ic_type & (PHYDM_IC_ABOVE_3SS)) {
		
		h2c_val[3] = (u8)((ra_mask >> 32) & 0x000000ff);
		h2c_val[4] = (u8)(((ra_mask >> 32) & 0x0000ff00) >> 8);
		h2c_val[5] = (u8)(((ra_mask >> 32) & 0x00ff0000) >> 16);
		h2c_val[6] = (u8)(((ra_mask >> 32) & 0xff000000) >> 24);

		PHYDM_DBG(p_dm, DBG_RA, ("PHYDM h2c[0x46]=0x%x %x %x %x %x %x %x\n",
		h2c_val[6], h2c_val[5], h2c_val[4], h2c_val[3], h2c_val[2], h2c_val[1], h2c_val[0]));
		
		odm_fill_h2c_cmd(p_dm, PHYDM_RA_MASK_ABOVE_3SS, 5, h2c_val);
	}
	#endif
}

void
phydm_ra_registed(
	void	*p_dm_void,
	u8	macid,
	u8	rssi_from_assoc
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_t = &p_dm->dm_ra_table;
	struct cmn_sta_info			*p_sta = p_dm->p_phydm_sta_info[macid];
	struct ra_sta_info				*p_ra = NULL;
	u8	init_ra_lv;
	u64	ra_mask;

	if (is_sta_active(p_sta)) {
		p_ra = &(p_sta->ra_info);
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warning] %s invalid sta_info\n", __func__));
		return;
	}

	PHYDM_DBG(p_dm, DBG_RA, ("%s ======>\n", __func__));
	PHYDM_DBG(p_dm, DBG_RA, ("MACID=%d\n", p_sta->mac_id));


	#if (RATE_ADAPTIVE_SUPPORT == 1)
	if (p_dm->support_ic_type == ODM_RTL8188E)
		phydm_get_rate_id_88e(p_dm, macid);
	else
	#endif
	{
		p_ra->rate_id = phydm_get_rate_id(p_dm, macid);
	}
	
	/*p_ra->is_vht_enable = (p_sta->support_wireless_set | WIRELESS_VHT) ? 1 : 0;*/
	/*p_ra->disable_ra = 0;*/
	/*p_ra->disable_pt = 0;*/
	ra_mask = phydm_get_bb_mod_ra_mask(p_dm, macid);


	if (rssi_from_assoc > 40)
		init_ra_lv = 3;
	else if (rssi_from_assoc > 20)
		init_ra_lv = 2;
	else
		init_ra_lv = 1;

	if (p_ra_t->record_ra_info)
		p_ra_t->record_ra_info(p_dm, macid, p_sta, ra_mask);

	#if (RATE_ADAPTIVE_SUPPORT == 1)
	if (p_dm->support_ic_type == ODM_RTL8188E)
		/*Driver RA*/
		odm_ra_update_rate_info_8188e(p_dm, macid, p_ra->rate_id, (u32)ra_mask, p_ra->is_support_sgi);
	else
	#endif
	{
		/*FW RA*/
		phydm_ra_h2c(p_dm, macid, p_ra->disable_ra, p_ra->disable_pt, 0, init_ra_lv, ra_mask);
	}

	

}

void
phydm_ra_offline(
	void	*p_dm_void,
	u8	macid
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_t = &p_dm->dm_ra_table;
	struct cmn_sta_info			*p_sta = p_dm->p_phydm_sta_info[macid];
	struct ra_sta_info				*p_ra = NULL;

	if (is_sta_active(p_sta)) {
		p_ra = &(p_sta->ra_info);
	} else {
		PHYDM_DBG(p_dm, DBG_RA, ("[Warning] %s invalid sta_info\n", __func__));
		return;
	}

	PHYDM_DBG(p_dm, DBG_RA, ("%s ======>\n", __func__));
	PHYDM_DBG(p_dm, DBG_RA, ("MACID=%d\n", p_sta->mac_id));

	odm_memory_set(p_dm, &(p_ra->rate_id), 0, sizeof(struct ra_sta_info));
	p_ra->disable_ra = 1;
	p_ra->disable_pt = 1;

	if (p_ra_t->record_ra_info)
		p_ra_t->record_ra_info(p_dm, macid, p_sta, 0);

	if (p_dm->support_ic_type != ODM_RTL8188E)
		phydm_ra_h2c(p_dm, macid, p_ra->disable_ra, p_ra->disable_pt, 0, 0, 0);
}

void
phydm_ra_mask_watchdog(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_t = &p_dm->dm_ra_table;
	struct cmn_sta_info			*p_sta = NULL;
	struct ra_sta_info				*p_ra = NULL;
	u8		macid;
	u64		ra_mask;
	u8		rssi_lv_new;

	if (!(p_dm->support_ability & ODM_BB_RA_MASK))
		return;
	
	if (((!p_dm->is_linked)) || (p_dm->phydm_sys_up_time % 2) == 1)
		return;

	PHYDM_DBG(p_dm, DBG_RA_MASK, ("%s ======>\n", __func__));
	
	p_ra_t->up_ramask_cnt++;

	for (macid = 0; macid < ODM_ASSOCIATE_ENTRY_NUM; macid++) {
		
		p_sta = p_dm->p_phydm_sta_info[macid];
		
		if (!is_sta_active(p_sta))
			continue;

		p_ra = &(p_sta->ra_info);

		if (p_ra->disable_ra)
			continue;
		rssi_lv_new = phydm_rssi_lv_dec(p_dm, (u32)p_sta->rssi_stat.rssi, p_ra->rssi_level);

		if ((p_ra->rssi_level != rssi_lv_new) || 
			(p_ra_t->up_ramask_cnt >= FORCED_UPDATE_RAMASK_PERIOD)) {

			PHYDM_DBG(p_dm, DBG_RA_MASK, ("RSSI LV:((%d))->((%d))\n", p_ra->rssi_level, rssi_lv_new));
			
			p_ra->rssi_level = rssi_lv_new;
			p_ra_t->up_ramask_cnt = 0;
			
			ra_mask = phydm_get_bb_mod_ra_mask(p_dm, macid);

			if (p_ra_t->record_ra_info)
				p_ra_t->record_ra_info(p_dm, macid, p_sta, ra_mask);

			#if (RATE_ADAPTIVE_SUPPORT == 1)
			if (p_dm->support_ic_type == ODM_RTL8188E)
				/*Driver RA*/
				odm_ra_update_rate_info_8188e(p_dm, macid, p_ra->rate_id, (u32)ra_mask, p_ra->is_support_sgi);
			else
			#endif
			{
				/*FW RA*/
				phydm_ra_h2c(p_dm, macid, p_ra->disable_ra, p_ra->disable_pt, 1, 0, ra_mask);
			}
		}
	}

}

u8
phydm_vht_en_mapping(
	void			*p_dm_void,
	u32			wireless_mode
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8			vht_en_out = 0;

	if ((wireless_mode == PHYDM_WIRELESS_MODE_AC_5G) ||
	    (wireless_mode == PHYDM_WIRELESS_MODE_AC_24G) ||
	    (wireless_mode == PHYDM_WIRELESS_MODE_AC_ONLY)
	   ) {
		vht_en_out = 1;
		/**/
	}

	PHYDM_DBG(p_dm, DBG_RA, ("wireless_mode= (( 0x%x )), VHT_EN= (( %d ))\n", wireless_mode, vht_en_out));
	return vht_en_out;
}

u8
phydm_rate_id_mapping(
	void			*p_dm_void,
	u32			wireless_mode,
	u8			rf_type,
	u8			bw
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8			rate_id_idx = 0;

	PHYDM_DBG(p_dm, DBG_RA, ("wireless_mode= (( 0x%x )), rf_type = (( 0x%x )), BW = (( 0x%x ))\n",
			wireless_mode, rf_type, bw));


	switch (wireless_mode) {

	case PHYDM_WIRELESS_MODE_N_24G:
	{

		if (bw == CHANNEL_WIDTH_40) {

			if (rf_type == RF_1T1R)
				rate_id_idx = PHYDM_BGN_40M_1SS;
			else if (rf_type == RF_2T2R)
				rate_id_idx = PHYDM_BGN_40M_2SS;
			else
				rate_id_idx = PHYDM_ARFR5_N_3SS;

		} else {

			if (rf_type == RF_1T1R)
				rate_id_idx = PHYDM_BGN_20M_1SS;
			else if (rf_type == RF_2T2R)
				rate_id_idx = PHYDM_BGN_20M_2SS;
			else
				rate_id_idx = PHYDM_ARFR5_N_3SS;
		}
	}
	break;

	case PHYDM_WIRELESS_MODE_N_5G:
	{
		if (rf_type == RF_1T1R)
			rate_id_idx = PHYDM_GN_N1SS;
		else if (rf_type == RF_2T2R)
			rate_id_idx = PHYDM_GN_N2SS;
		else
			rate_id_idx = PHYDM_ARFR5_N_3SS;
	}

	break;

	case PHYDM_WIRELESS_MODE_G:
		rate_id_idx = PHYDM_BG;
		break;

	case PHYDM_WIRELESS_MODE_A:
		rate_id_idx = PHYDM_G;
		break;

	case PHYDM_WIRELESS_MODE_B:
		rate_id_idx = PHYDM_B_20M;
		break;


	case PHYDM_WIRELESS_MODE_AC_5G:
	case PHYDM_WIRELESS_MODE_AC_ONLY:
	{
		if (rf_type == RF_1T1R)
			rate_id_idx = PHYDM_ARFR1_AC_1SS;
		else if (rf_type == RF_2T2R)
			rate_id_idx = PHYDM_ARFR0_AC_2SS;
		else
			rate_id_idx = PHYDM_ARFR4_AC_3SS;
	}
	break;

	case PHYDM_WIRELESS_MODE_AC_24G:
	{
		/*Becareful to set "Lowest rate" while using PHYDM_ARFR4_AC_3SS in 2.4G/5G*/
		if (bw >= CHANNEL_WIDTH_80) {
			if (rf_type == RF_1T1R)
				rate_id_idx = PHYDM_ARFR1_AC_1SS;
			else if (rf_type == RF_2T2R)
				rate_id_idx = PHYDM_ARFR0_AC_2SS;
			else
				rate_id_idx = PHYDM_ARFR4_AC_3SS;
		} else {

			if (rf_type == RF_1T1R)
				rate_id_idx = PHYDM_ARFR2_AC_2G_1SS;
			else if (rf_type == RF_2T2R)
				rate_id_idx = PHYDM_ARFR3_AC_2G_2SS;
			else
				rate_id_idx = PHYDM_ARFR4_AC_3SS;
		}
	}
	break;

	default:
		rate_id_idx = 0;
		break;
	}

	PHYDM_DBG(p_dm, DBG_RA, ("RA rate ID = (( 0x%x ))\n", rate_id_idx));

	return rate_id_idx;
}

void
phydm_update_hal_ra_mask(
	void			*p_dm_void,
	u32			wireless_mode,
	u8			rf_type,
	u8			bw,
	u8			mimo_ps_enable,
	u8			disable_cck_rate,
	u32			*ratr_bitmap_msb_in,
	u32			*ratr_bitmap_lsb_in,
	u8			tx_rate_level
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u32			ratr_bitmap = *ratr_bitmap_lsb_in, ratr_bitmap_msb = *ratr_bitmap_msb_in;

	/*PHYDM_DBG(p_dm, DBG_RA_MASK, ("phydm_rf_type = (( %x )), rf_type = (( %x ))\n", phydm_rf_type, rf_type));*/
	PHYDM_DBG(p_dm, DBG_RA_MASK, ("Platfoem original RA Mask = (( 0x %x | %x ))\n", ratr_bitmap_msb, ratr_bitmap));

	switch (wireless_mode) {

	case PHYDM_WIRELESS_MODE_B:
	{
		ratr_bitmap &= 0x0000000f;
	}
	break;

	case PHYDM_WIRELESS_MODE_G:
	{
		ratr_bitmap &= 0x00000ff5;
	}
	break;

	case PHYDM_WIRELESS_MODE_A:
	{
		ratr_bitmap &= 0x00000ff0;
	}
	break;

	case PHYDM_WIRELESS_MODE_N_24G:
	case PHYDM_WIRELESS_MODE_N_5G:
	{
		if (mimo_ps_enable)
			rf_type = RF_1T1R;

		if (rf_type == RF_1T1R) {

			if (bw == CHANNEL_WIDTH_40)
				ratr_bitmap &= 0x000ff015;
			else
				ratr_bitmap &= 0x000ff005;
		} else if (rf_type == RF_2T2R || rf_type == RF_2T4R || rf_type == RF_2T3R) {

			if (bw == CHANNEL_WIDTH_40)
				ratr_bitmap &= 0x0ffff015;
			else
				ratr_bitmap &= 0x0ffff005;
		} else { /*3T*/

			ratr_bitmap &= 0xfffff015;
			ratr_bitmap_msb &= 0xf;
		}
	}
	break;

	case PHYDM_WIRELESS_MODE_AC_24G:
	{
		if (rf_type == RF_1T1R)
			ratr_bitmap &= 0x003ff015;
		else if (rf_type == RF_2T2R || rf_type == RF_2T4R || rf_type == RF_2T3R)
			ratr_bitmap &= 0xfffff015;
		else {/*3T*/

			ratr_bitmap &= 0xfffff010;
			ratr_bitmap_msb &= 0x3ff;
		}

		if (bw == CHANNEL_WIDTH_20) {/* AC 20MHz doesn't support MCS9 */
			ratr_bitmap &= 0x7fdfffff;
			ratr_bitmap_msb &= 0x1ff;
		}
	}
	break;

	case PHYDM_WIRELESS_MODE_AC_5G:
	{
		if (rf_type == RF_1T1R)
			ratr_bitmap &= 0x003ff010;
		else if (rf_type == RF_2T2R || rf_type == RF_2T4R || rf_type == RF_2T3R)
			ratr_bitmap &= 0xfffff010;
		else {/*3T*/

			ratr_bitmap &= 0xfffff010;
			ratr_bitmap_msb &= 0x3ff;
		}

		if (bw == CHANNEL_WIDTH_20) {/* AC 20MHz doesn't support MCS9 */
			ratr_bitmap &= 0x7fdfffff;
			ratr_bitmap_msb &= 0x1ff;
		}
	}
	break;

	default:
		break;
	}

	if (wireless_mode != PHYDM_WIRELESS_MODE_B) {

		if (tx_rate_level == 0)
			ratr_bitmap &=  0xffffffff;
		else if (tx_rate_level == 1)
			ratr_bitmap &=  0xfffffff0;
		else if (tx_rate_level == 2)
			ratr_bitmap &=  0xffffefe0;
		else if (tx_rate_level == 3)
			ratr_bitmap &=  0xffffcfc0;
		else if (tx_rate_level == 4)
			ratr_bitmap &=  0xffff8f80;
		else if (tx_rate_level >= 5)
			ratr_bitmap &=  0xffff0f00;

	}

	if (disable_cck_rate)
		ratr_bitmap &= 0xfffffff0;

	PHYDM_DBG(p_dm, DBG_RA_MASK, ("wireless_mode= (( 0x%x )), rf_type = (( 0x%x )), BW = (( 0x%x )), MimoPs_en = (( %d )), tx_rate_level= (( 0x%x ))\n",
		wireless_mode, rf_type, bw, mimo_ps_enable, tx_rate_level));

	/*PHYDM_DBG(p_dm, DBG_RA_MASK, ("111 Phydm modified RA Mask = (( 0x %x | %x ))\n", ratr_bitmap_msb, ratr_bitmap));*/

	*ratr_bitmap_lsb_in = ratr_bitmap;
	*ratr_bitmap_msb_in = ratr_bitmap_msb;
	PHYDM_DBG(p_dm, DBG_RA_MASK, ("Phydm modified RA Mask = (( 0x %x | %x ))\n", *ratr_bitmap_msb_in, *ratr_bitmap_lsb_in));

}

u8
phydm_rssi_lv_dec(
	void			*p_dm_void,
	u32			rssi,
	u8			ratr_state
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	rssi_lv_table[RA_FLOOR_TABLE_SIZE] = {20, 34, 38, 42, 46, 50, 100}; /*MCS0 ~ MCS4 , VHT1SS MCS0 ~ MCS4 , G 6M~24M*/
	u8	new_rssi_lv = 0;
	u8	i;

	PHYDM_DBG(p_dm, DBG_RA_MASK, ("curr RA level=(%d), Table_ori=[%d, %d, %d, %d, %d, %d]\n",
		ratr_state, rssi_lv_table[0], rssi_lv_table[1], rssi_lv_table[2], rssi_lv_table[3], rssi_lv_table[4], rssi_lv_table[5]));

	for (i = 0; i < RA_FLOOR_TABLE_SIZE; i++) {

		if (i >= (ratr_state))
			rssi_lv_table[i] += RA_FLOOR_UP_GAP;
	}

	PHYDM_DBG(p_dm, DBG_RA_MASK, ("RSSI=(%d), Table_mod=[%d, %d, %d, %d, %d, %d]\n",
		rssi, rssi_lv_table[0], rssi_lv_table[1], rssi_lv_table[2], rssi_lv_table[3], rssi_lv_table[4], rssi_lv_table[5]));

	for (i = 0; i < RA_FLOOR_TABLE_SIZE; i++) {

		if (rssi < rssi_lv_table[i]) {
			new_rssi_lv = i;
			break;
		}
	}
	return	new_rssi_lv;
}

u8
phydm_rate_order_compute(
	void	*p_dm_void,
	u8	rate_idx
)
{
	u8		rate_order = 0;

	if (rate_idx >= ODM_RATEVHTSS4MCS0) {

		rate_idx -= ODM_RATEVHTSS4MCS0;
		/**/
	} else if (rate_idx >= ODM_RATEVHTSS3MCS0) {

		rate_idx -= ODM_RATEVHTSS3MCS0;
		/**/
	} else if (rate_idx >= ODM_RATEVHTSS2MCS0) {

		rate_idx -= ODM_RATEVHTSS2MCS0;
		/**/
	} else if (rate_idx >= ODM_RATEVHTSS1MCS0) {

		rate_idx -= ODM_RATEVHTSS1MCS0;
		/**/
	} else if (rate_idx >= ODM_RATEMCS24) {

		rate_idx -= ODM_RATEMCS24;
		/**/
	} else if (rate_idx >= ODM_RATEMCS16) {

		rate_idx -= ODM_RATEMCS16;
		/**/
	} else if (rate_idx >= ODM_RATEMCS8) {

		rate_idx -= ODM_RATEMCS8;
		/**/
	}
	rate_order = rate_idx;

	return rate_order;

}

static void
phydm_ra_common_info_update(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_		*p_ra_table = &p_dm->dm_ra_table;
	struct cmn_sta_info			*p_sta = NULL;
	u16		macid;
	u8		rate_order_tmp;
	u8		cnt = 0;

	p_ra_table->highest_client_tx_order = 0;
	p_ra_table->power_tracking_flag = 1;

	if (p_dm->number_linked_client != 0) {
		for (macid = 0; macid < ODM_ASSOCIATE_ENTRY_NUM; macid++) {

			p_sta = p_dm->p_phydm_sta_info[macid];
			
			if (is_sta_active(p_sta)) {
			
				rate_order_tmp = phydm_rate_order_compute(p_dm, (p_sta->ra_info.curr_tx_rate & 0x7f));

				if (rate_order_tmp >= (p_ra_table->highest_client_tx_order)) {
					p_ra_table->highest_client_tx_order = rate_order_tmp;
					p_ra_table->highest_client_tx_rate_order = macid;
				}

				cnt++;

				if (cnt == p_dm->number_linked_client)
					break;
			}
		}
		PHYDM_DBG(p_dm, DBG_RA, ("MACID[%d], Highest Tx order Update for power traking: %d\n", (p_ra_table->highest_client_tx_rate_order), (p_ra_table->highest_client_tx_order)));
	}
}

void
phydm_ra_info_watchdog(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	phydm_ra_common_info_update(p_dm);
	phydm_ra_dynamic_retry_count(p_dm);
	phydm_refresh_rate_adaptive_mask(p_dm);
}

void
phydm_ra_info_init(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_		*p_ra_table = &p_dm->dm_ra_table;

	p_ra_table->highest_client_tx_rate_order = 0;
	p_ra_table->highest_client_tx_order = 0;
	p_ra_table->RA_threshold_offset = 0;
	p_ra_table->RA_offset_direction = 0;
	
	#ifdef CONFIG_RA_DYNAMIC_RATE_ID
	phydm_ra_dynamic_rate_id_init(p_dm);
	#endif

	phydm_rate_adaptive_mask_init(p_dm);
}

u8
odm_find_rts_rate(
	void			*p_dm_void,
	u8			tx_rate,
	bool		is_erp_protect
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	rts_ini_rate = ODM_RATE6M;

	if (is_erp_protect) /* use CCK rate as RTS*/
		rts_ini_rate = ODM_RATE1M;
	else {
		switch (tx_rate) {
		case ODM_RATEVHTSS3MCS9:
		case ODM_RATEVHTSS3MCS8:
		case ODM_RATEVHTSS3MCS7:
		case ODM_RATEVHTSS3MCS6:
		case ODM_RATEVHTSS3MCS5:
		case ODM_RATEVHTSS3MCS4:
		case ODM_RATEVHTSS3MCS3:
		case ODM_RATEVHTSS2MCS9:
		case ODM_RATEVHTSS2MCS8:
		case ODM_RATEVHTSS2MCS7:
		case ODM_RATEVHTSS2MCS6:
		case ODM_RATEVHTSS2MCS5:
		case ODM_RATEVHTSS2MCS4:
		case ODM_RATEVHTSS2MCS3:
		case ODM_RATEVHTSS1MCS9:
		case ODM_RATEVHTSS1MCS8:
		case ODM_RATEVHTSS1MCS7:
		case ODM_RATEVHTSS1MCS6:
		case ODM_RATEVHTSS1MCS5:
		case ODM_RATEVHTSS1MCS4:
		case ODM_RATEVHTSS1MCS3:
		case ODM_RATEMCS15:
		case ODM_RATEMCS14:
		case ODM_RATEMCS13:
		case ODM_RATEMCS12:
		case ODM_RATEMCS11:
		case ODM_RATEMCS7:
		case ODM_RATEMCS6:
		case ODM_RATEMCS5:
		case ODM_RATEMCS4:
		case ODM_RATEMCS3:
		case ODM_RATE54M:
		case ODM_RATE48M:
		case ODM_RATE36M:
		case ODM_RATE24M:
			rts_ini_rate = ODM_RATE24M;
			break;
		case ODM_RATEVHTSS3MCS2:
		case ODM_RATEVHTSS3MCS1:
		case ODM_RATEVHTSS2MCS2:
		case ODM_RATEVHTSS2MCS1:
		case ODM_RATEVHTSS1MCS2:
		case ODM_RATEVHTSS1MCS1:
		case ODM_RATEMCS10:
		case ODM_RATEMCS9:
		case ODM_RATEMCS2:
		case ODM_RATEMCS1:
		case ODM_RATE18M:
		case ODM_RATE12M:
			rts_ini_rate = ODM_RATE12M;
			break;
		case ODM_RATEVHTSS3MCS0:
		case ODM_RATEVHTSS2MCS0:
		case ODM_RATEVHTSS1MCS0:
		case ODM_RATEMCS8:
		case ODM_RATEMCS0:
		case ODM_RATE9M:
		case ODM_RATE6M:
			rts_ini_rate = ODM_RATE6M;
			break;
		case ODM_RATE11M:
		case ODM_RATE5_5M:
		case ODM_RATE2M:
		case ODM_RATE1M:
			rts_ini_rate = ODM_RATE1M;
			break;
		default:
			rts_ini_rate = ODM_RATE6M;
			break;
		}
	}

	if (*p_dm->p_band_type == ODM_BAND_5G) {
		if (rts_ini_rate < ODM_RATE6M)
			rts_ini_rate = ODM_RATE6M;
	}
	return rts_ini_rate;

}

#if (defined(CONFIG_RA_DYNAMIC_RATE_ID))
void
phydm_ra_dynamic_rate_id_on_assoc(
	void	*p_dm_void,
	u8	wireless_mode,
	u8	init_rate_id
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	PHYDM_DBG(p_dm, DBG_RA, ("[ON ASSOC] rf_mode = ((0x%x)), wireless_mode = ((0x%x)), init_rate_id = ((0x%x))\n", p_dm->rf_type, wireless_mode, init_rate_id));

	if ((p_dm->rf_type == RF_2T2R) || (p_dm->rf_type == RF_2T3R) || (p_dm->rf_type == RF_2T4R)) {

		if ((p_dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8192E)) &&
		    (wireless_mode & (ODM_WM_N24G | ODM_WM_N5G))
		   ) {
			PHYDM_DBG(p_dm, DBG_RA, ("[ON ASSOC] set N-2SS ARFR5 table\n"));
			odm_set_mac_reg(p_dm, 0x4a4, MASKDWORD, 0xfc1ffff);	/*N-2SS, ARFR5, rate_id = 0xe*/
			odm_set_mac_reg(p_dm, 0x4a8, MASKDWORD, 0x0);		/*N-2SS, ARFR5, rate_id = 0xe*/
		} else if ((p_dm->support_ic_type & (ODM_RTL8812)) &&
			(wireless_mode & (ODM_WM_AC_5G | ODM_WM_AC_24G | ODM_WM_AC_ONLY))
			  ) {
			PHYDM_DBG(p_dm, DBG_RA, ("[ON ASSOC] set AC-2SS ARFR0 table\n"));
			odm_set_mac_reg(p_dm, 0x444, MASKDWORD, 0x0fff);	/*AC-2SS, ARFR0, rate_id = 0x9*/
			odm_set_mac_reg(p_dm, 0x448, MASKDWORD, 0xff01f000);		/*AC-2SS, ARFR0, rate_id = 0x9*/
		}
	}

}

void
phydm_ra_dynamic_rate_id_init(
	void	*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (p_dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8192E)) {

		odm_set_mac_reg(p_dm, 0x4a4, MASKDWORD, 0xfc1ffff);	/*N-2SS, ARFR5, rate_id = 0xe*/
		odm_set_mac_reg(p_dm, 0x4a8, MASKDWORD, 0x0);		/*N-2SS, ARFR5, rate_id = 0xe*/

		odm_set_mac_reg(p_dm, 0x444, MASKDWORD, 0x0fff);		/*AC-2SS, ARFR0, rate_id = 0x9*/
		odm_set_mac_reg(p_dm, 0x448, MASKDWORD, 0xff01f000);	/*AC-2SS, ARFR0, rate_id = 0x9*/
	}
}

void
phydm_update_rate_id(
	void	*p_dm_void,
	u8	rate,
	u8	platform_macid
)
{
}
#endif
