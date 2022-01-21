// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"

#ifdef NEVER
void
phydm_dynamicsoftmletting(
	struct PHY_DM_STRUCT		*p_dm
)
{

	u32 ret_val;

#if (RTL8822B_SUPPORT == 1)
	if (!*p_dm->p_mp_mode) {
		if (p_dm->support_ic_type & ODM_RTL8822B) {

			if ((!p_dm->is_linked)|(p_dm->iot_table.is_linked_cmw500))
				return;

			if (p_dm->bsomlenabled) {
				PHYDM_DBG(p_dm, ODM_COMP_API, ("PHYDM_DynamicSoftMLSetting(): SoML has been enable, skip dynamic SoML switch\n"));
				return; 		
			}

			ret_val = odm_get_bb_reg(p_dm, 0xf8c, MASKBYTE0);
			PHYDM_DBG(p_dm, ODM_COMP_API, ("PHYDM_DynamicSoftMLSetting(): Read 0xF8C = 0x%08X\n", ret_val));

			if (ret_val < 0x16) {
				PHYDM_DBG(p_dm, ODM_COMP_API, ("PHYDM_DynamicSoftMLSetting(): 0xF8C(== 0x%08X) < 0x16, enable SoML\n", ret_val));
				phydm_somlrxhp_setting(p_dm, true);
				/*odm_set_bb_reg(p_dm, 0x19a8, MASKDWORD, 0xc10a0000);*/
				p_dm->bsomlenabled = true;
			}
		}
	}
#endif

}
#endif

#ifdef CONFIG_ADAPTIVE_SOML
void
phydm_soml_on_off(
	void		*p_dm_void,
	u8		swch
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &(p_dm->dm_soml_table);

	if (swch == SOML_ON) {

		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("(( Turn on )) SOML\n"));

		if (p_dm->support_ic_type == ODM_RTL8822B)
			phydm_somlrxhp_setting(p_dm, true);
		else if (p_dm->support_ic_type == ODM_RTL8197F)
			odm_set_bb_reg(p_dm, 0x998, BIT(6), swch);

	} else if (swch == SOML_OFF) {

		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("(( Turn off )) SOML\n"));

		if (p_dm->support_ic_type == ODM_RTL8822B)
			phydm_somlrxhp_setting(p_dm, false);
		else if (p_dm->support_ic_type == ODM_RTL8197F)
			odm_set_bb_reg(p_dm, 0x998, BIT(6), swch);
	}
	p_dm_soml_table->soml_on_off = swch;
}

void
phydm_adaptive_soml_callback(
	void	*dm_void
)
{
	struct PHY_DM_STRUCT	*dm = (struct PHY_DM_STRUCT *)dm_void;
	void	*padapter = dm->adapter;

	if (*dm->p_is_net_closed)
		return;
	if (dm->support_interface == ODM_ITRF_PCIE) {
		phydm_adsl(dm);
	} else {
		/* Can't do I/O in timer callback*/
		rtw_run_in_thread_cmd(padapter, phydm_adaptive_soml_workitem_callback, padapter);
	}
}

void
phydm_adaptive_soml_workitem_callback(
	void	*context
)
{
	void *adapter = (void *)context;
	struct hal_com_data	*hal_data = GET_HAL_DATA(((struct adapter *)adapter));
	struct PHY_DM_STRUCT		*dm = &hal_data->odmpriv;

	/*dbg_print("phydm_adaptive_soml-phydm_adaptive_soml_workitem_callback\n");*/
	phydm_adsl(dm);
}

void
phydm_soml_debug(
	void		*p_dm_void,
	u32		*const dm_value,
	u32		*_used,
	char		*output,
	u32		*_out_len
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &(p_dm->dm_soml_table);
	u32 used = *_used;
	u32 out_len = *_out_len;

	if (dm_value[0] == 1) { /*Turn on/off SOML*/
		p_dm_soml_table->soml_select = (u8)dm_value[1];

	} else if (dm_value[0] == 2) { /*training number for SOML*/

		p_dm_soml_table->soml_train_num = (u8)dm_value[1];
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_train_num = ((%d))\n", p_dm_soml_table->soml_train_num));
	} else if (dm_value[0] == 3) { /*training interval for SOML*/

		p_dm_soml_table->soml_intvl = (u8)dm_value[1];
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_intvl = ((%d))\n", p_dm_soml_table->soml_intvl));
	} else if (dm_value[0] == 4) { /*function period for SOML*/

		p_dm_soml_table->soml_period = (u8)dm_value[1];
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_period = ((%d))\n", p_dm_soml_table->soml_period));
	} else if (dm_value[0] == 5) { /*delay_time for SOML*/

		p_dm_soml_table->soml_delay_time = (u8)dm_value[1];
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_delay_time = ((%d))\n", p_dm_soml_table->soml_delay_time));
	} else if (dm_value[0] == 100) { /*show parameters*/

		PHYDM_SNPRINTF((output + used, out_len - used, "soml_train_num = ((%d))\n", p_dm_soml_table->soml_train_num));
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_intvl = ((%d))\n", p_dm_soml_table->soml_intvl));
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_period = ((%d))\n", p_dm_soml_table->soml_period));
		PHYDM_SNPRINTF((output + used, out_len - used, "soml_delay_time = ((%d))\n", p_dm_soml_table->soml_delay_time));
	}
	*_used = used;
	*_out_len = out_len;
}

void
phydm_soml_statistics(
	void		*p_dm_void,
	u8		on_off_state

)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &(p_dm->dm_soml_table);

	u8	i;
	u32	num_bytes_diff;

	if (on_off_state == SOML_ON) {
		if (*p_dm->p_channel <= 14) {
			for (i = ODM_RATEMCS0; i <= ODM_RATEMCS15; i++) {
				num_bytes_diff = p_dm_soml_table->num_ht_bytes[i] - p_dm_soml_table->pre_num_ht_bytes[i];
				p_dm_soml_table->num_ht_bytes_on[i] += num_bytes_diff;
				p_dm_soml_table->pre_num_ht_bytes[i] = p_dm_soml_table->num_ht_bytes[i];
			}
		}
		if (p_dm->support_ic_type == ODM_RTL8822B) {
			for (i = ODM_RATEVHTSS1MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
				num_bytes_diff = p_dm_soml_table->num_vht_bytes[i] - p_dm_soml_table->pre_num_vht_bytes[i];
				p_dm_soml_table->num_vht_bytes_on[i] += num_bytes_diff;
				p_dm_soml_table->pre_num_vht_bytes[i] = p_dm_soml_table->num_vht_bytes[i];
			}
		}
	} else if (on_off_state == SOML_OFF) {
		if (*p_dm->p_channel <= 14) {
			for (i = ODM_RATEMCS0; i <= ODM_RATEMCS15; i++) {
				num_bytes_diff = p_dm_soml_table->num_ht_bytes[i] - p_dm_soml_table->pre_num_ht_bytes[i];
				p_dm_soml_table->num_ht_bytes_off[i] += num_bytes_diff;
				p_dm_soml_table->pre_num_ht_bytes[i] = p_dm_soml_table->num_ht_bytes[i];
			}
		}
		if (p_dm->support_ic_type == ODM_RTL8822B) {
			for (i = ODM_RATEVHTSS1MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
				num_bytes_diff = p_dm_soml_table->num_vht_bytes[i] - p_dm_soml_table->pre_num_vht_bytes[i];
				p_dm_soml_table->num_vht_bytes_off[i] += num_bytes_diff;
				p_dm_soml_table->pre_num_vht_bytes[i] = p_dm_soml_table->num_vht_bytes[i];
			}
		}
	}
}

void
phydm_adsl(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml		*p_dm_soml_table = &(p_dm->dm_soml_table);

	u8	i;
	u8	next_on_off;
	u8	rate_num = 1, rate_ss_shift = 0;
	u32	byte_total_on = 0, byte_total_off = 0;
	u32	ht_reset[HT_RATE_IDX] = {0}, vht_reset[VHT_RATE_IDX] = {0};
	u8	size = sizeof(ht_reset[0]);

	if (p_dm->support_ic_type & ODM_IC_4SS)
		rate_num = 4;
	else if (p_dm->support_ic_type & ODM_IC_3SS)
		rate_num = 3;
	else if (p_dm->support_ic_type & ODM_IC_2SS)
		rate_num = 2;

	if (!(p_dm->support_ic_type & ODM_ADAPTIVE_SOML_SUPPORT_IC))
		return;
		
	PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("rssi_min =%d\n", p_dm->rssi_min));
	PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("soml_state_cnt =((%d))\n", p_dm_soml_table->soml_state_cnt));
	/*Traning state: 0(alt) 1(ori) 2(alt) 3(ori)============================================================*/
	if (p_dm_soml_table->soml_state_cnt < (p_dm_soml_table->soml_train_num << 1)) {

		if (p_dm_soml_table->soml_state_cnt == 0) {

			odm_move_memory(p_dm, p_dm_soml_table->num_ht_bytes, ht_reset, HT_RATE_IDX * size);
			odm_move_memory(p_dm, p_dm_soml_table->num_ht_bytes_on, ht_reset, HT_RATE_IDX * size);
			odm_move_memory(p_dm, p_dm_soml_table->num_ht_bytes_off, ht_reset, HT_RATE_IDX * size);
			odm_move_memory(p_dm, p_dm_soml_table->num_vht_bytes, vht_reset, VHT_RATE_IDX * size);
			odm_move_memory(p_dm, p_dm_soml_table->num_vht_bytes_on, vht_reset, VHT_RATE_IDX * size);
			odm_move_memory(p_dm, p_dm_soml_table->num_vht_bytes_off, vht_reset, VHT_RATE_IDX * size);

			p_dm_soml_table->is_soml_method_enable = 1;
			p_dm_soml_table->soml_state_cnt++;
			next_on_off = (p_dm_soml_table->soml_on_off == SOML_ON) ? SOML_ON : SOML_OFF;
			phydm_soml_on_off(p_dm, next_on_off);
			odm_set_timer(p_dm, &p_dm_soml_table->phydm_adaptive_soml_timer, p_dm_soml_table->soml_delay_time); /*ms*/
		} else if ((p_dm_soml_table->soml_state_cnt % 2) != 0) {

			p_dm_soml_table->soml_state_cnt++;
			odm_move_memory(p_dm, p_dm_soml_table->pre_num_ht_bytes, p_dm_soml_table->num_ht_bytes, HT_RATE_IDX * size);
			odm_move_memory(p_dm, p_dm_soml_table->pre_num_vht_bytes, p_dm_soml_table->num_vht_bytes, VHT_RATE_IDX * size);
			odm_set_timer(p_dm, &p_dm_soml_table->phydm_adaptive_soml_timer, p_dm_soml_table->soml_intvl); /*ms*/
		} else if ((p_dm_soml_table->soml_state_cnt % 2) == 0) {

			p_dm_soml_table->soml_state_cnt++;
			phydm_soml_statistics(p_dm, p_dm_soml_table->soml_on_off);
			next_on_off = (p_dm_soml_table->soml_on_off == SOML_ON) ? SOML_OFF : SOML_ON;
			phydm_soml_on_off(p_dm, next_on_off);
			odm_set_timer(p_dm, &p_dm_soml_table->phydm_adaptive_soml_timer, p_dm_soml_table->soml_delay_time); /*ms*/
		}
	}
	/*Decision state: ==============================================================*/
	else {

		p_dm_soml_table->soml_state_cnt = 0;
		phydm_soml_statistics(p_dm, p_dm_soml_table->soml_on_off);

		/* [Search 1st and 2ed rate by counter] */
		if (*p_dm->p_channel <= 14) {

			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = (i << 3);
				PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("*num_ht_bytes_on  HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
					  (rate_ss_shift), 
					  (rate_ss_shift + 7),
					  p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 0], p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 1],
					  p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 2], p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 3],
					  p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 4], p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 5],
					  p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 6], p_dm_soml_table->num_ht_bytes_on[rate_ss_shift + 7]));
			}

			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = (i << 3);
				PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("*num_ht_bytes_off  HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
					  (rate_ss_shift), (rate_ss_shift + 7),
					  p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 0], p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 1],
					  p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 2], p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 3],
					  p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 4], p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 5],
					  p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 6], p_dm_soml_table->num_ht_bytes_off[rate_ss_shift + 7]));
			}

			for (i = ODM_RATEMCS8; i <= ODM_RATEMCS15; i++) {

				byte_total_on += p_dm_soml_table->num_vht_bytes_on[i - ODM_RATEMCS0];
				byte_total_off += p_dm_soml_table->num_vht_bytes_off[i - ODM_RATEMCS0];
			}

		} else if (p_dm->support_ic_type == ODM_RTL8822B) {

			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = 10 * i;
				PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("* num_vht_bytes_on  VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
					 (i + 1),
					  p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 0], p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 1],
					  p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 2], p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 3],
					  p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 4], p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 5],
					  p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 6], p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 7],
					  p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 8], p_dm_soml_table->num_vht_bytes_on[rate_ss_shift + 9]));
			}

			for (i = 0; i < rate_num; i++) {
				rate_ss_shift = 10 * i;
				PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("* num_vht_bytes_off  VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
					 (i + 1),
					  p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 0], p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 1],
					  p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 2], p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 3],
					  p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 4], p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 5],
					  p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 6], p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 7],
					  p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 8], p_dm_soml_table->num_vht_bytes_off[rate_ss_shift + 9]));
			}
			for (i = ODM_RATEVHTSS2MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
				byte_total_on += p_dm_soml_table->num_vht_bytes_on[i - ODM_RATEVHTSS1MCS0];
				byte_total_off += p_dm_soml_table->num_vht_bytes_off[i - ODM_RATEVHTSS1MCS0];
			}
		}

		/* [Decision] */
		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[  byte_total_on = %d ; byte_total_off = %d ]\n", byte_total_on, byte_total_off));
		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[Decisoin state ]\n"));
		if (byte_total_on > byte_total_off) {
			next_on_off = SOML_ON;
			PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[ byte_total_on > byte_total_off ==> SOML_ON ]\n"));
		} else if (byte_total_on < byte_total_off) {
			next_on_off = SOML_OFF;
			PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[ byte_total_on < byte_total_off ==> SOML_OFF ]\n"));
		} else {
			PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[ stay at soml_last_state ]\n"));
			next_on_off = p_dm_soml_table->soml_last_state;
		}

		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[ Final decisoin ] : "));
		phydm_soml_on_off(p_dm, next_on_off);
		p_dm_soml_table->soml_last_state = next_on_off;
	}

}

void
phydm_adaptive_soml_reset(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &p_dm->dm_soml_table;

	p_dm_soml_table->soml_state_cnt = 0;
	p_dm_soml_table->is_soml_method_enable = 0;
	p_dm_soml_table->soml_counter = 0;
}

#endif /* end of CONFIG_ADAPTIVE_SOML*/
void
phydm_soml_bytes_acq(
	void		*dm_void,
	u8		rate_id,
	u32		length
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*dm = (struct PHY_DM_STRUCT *)dm_void;
	struct adaptive_soml	*dm_soml_table = &dm->dm_soml_table;

	if ((rate_id >= ODM_RATEMCS0) && (rate_id <= ODM_RATEMCS31))
		dm_soml_table->num_ht_bytes[rate_id - ODM_RATEMCS0] += length;
	else if ((rate_id >= ODM_RATEVHTSS1MCS0) && (rate_id <= ODM_RATEVHTSS4MCS9))
		dm_soml_table->num_vht_bytes[rate_id - ODM_RATEVHTSS1MCS0] += length;

#endif
}

void
phydm_adaptive_soml_timers(
	void		*p_dm_void,
	u8		state
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &p_dm->dm_soml_table;

	if (state == INIT_SOML_TIMMER) {
		odm_initialize_timer(p_dm, &p_dm_soml_table->phydm_adaptive_soml_timer,
			(void *)phydm_adaptive_soml_callback, NULL, "phydm_adaptive_soml_timer");
	} else if (state == CANCEL_SOML_TIMMER) {
		odm_cancel_timer(p_dm, &p_dm_soml_table->phydm_adaptive_soml_timer);
	} else if (state == RELEASE_SOML_TIMMER) {
		odm_release_timer(p_dm, &p_dm_soml_table->phydm_adaptive_soml_timer);
	}
#endif
}

void
phydm_adaptive_soml_init(
	void		*p_dm_void
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &p_dm->dm_soml_table;

	PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("phydm_adaptive_soml_init\n"));

	p_dm_soml_table->soml_state_cnt = 0;
	p_dm_soml_table->soml_delay_time = 40;
	p_dm_soml_table->soml_intvl = 150;
	p_dm_soml_table->soml_train_num = 4;
	p_dm_soml_table->is_soml_method_enable = 0;
	p_dm_soml_table->soml_counter = 0;
	p_dm_soml_table->soml_period = 4;
	p_dm_soml_table->soml_select = 0;
	if (p_dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
		odm_set_bb_reg(p_dm, 0x988, BIT(25), 1);
#endif
}

void
phydm_adaptive_soml(
	void		*p_dm_void
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct adaptive_soml	*p_dm_soml_table = &p_dm->dm_soml_table;

	if (!(p_dm->support_ability & ODM_BB_ADAPTIVE_SOML)) {
		PHYDM_DBG(p_dm, DBG_ADPTV_SOML,
			("[Return!!!]   Not Support Adaptive SOML Function\n"));
		return;
	}

	if (p_dm_soml_table->soml_counter <  p_dm_soml_table->soml_period) {
		p_dm_soml_table->soml_counter++;
		return;
	}
	p_dm_soml_table->soml_counter = 0;


	if (p_dm_soml_table->soml_select == 0) {
		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[Adaptive SOML Training !!!]\n"));
	} else if (p_dm_soml_table->soml_select == 1) {
		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[Turn on SOML !!!] Exit from Adaptive SOML Training\n"));
		phydm_soml_on_off(p_dm, SOML_ON);
		return;
	} else if (p_dm_soml_table->soml_select == 2) {
		PHYDM_DBG(p_dm, DBG_ADPTV_SOML, ("[Turn off SOML !!!] Exit from Adaptive SOML Training\n"));
		phydm_soml_on_off(p_dm, SOML_OFF);
		return;
	}
	if (p_dm->support_ic_type & ODM_ADAPTIVE_SOML_SUPPORT_IC)
		phydm_adsl(p_dm);

#endif
}

void
phydm_enable_adaptive_soml(
	void		*dm_void
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*dm = (struct PHY_DM_STRUCT *)dm_void;

	PHYDM_DBG(dm, DBG_ADPTV_SOML, ("[%s][Return!!!]  enable Adaptive SOML\n\n", __func__));
	dm->support_ability |= ODM_BB_ADAPTIVE_SOML;
	phydm_soml_on_off(dm, SOML_ON);
#endif
}

void
phydm_stop_adaptive_soml(
	void		*dm_void
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*dm = (struct PHY_DM_STRUCT *)dm_void;

	PHYDM_DBG(dm, DBG_ADPTV_SOML, ("[%s][Return!!!]  Stop Adaptive SOML\n\n", __func__));
	dm->support_ability &= ~ODM_BB_ADAPTIVE_SOML;
	phydm_soml_on_off(dm, SOML_ON);

#endif
}

void
phydm_adaptive_soml_para_set(
	void		*dm_void,
	u8		train_num,
	u8		intvl,
	u8		period,
	u8		delay_time
)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct PHY_DM_STRUCT		*dm = (struct PHY_DM_STRUCT *)dm_void;
	struct adaptive_soml	*dm_soml_table = &dm->dm_soml_table;

	dm_soml_table->soml_train_num = train_num;
	dm_soml_table->soml_intvl = intvl;
	dm_soml_table->soml_period = period;
	dm_soml_table->soml_delay_time = delay_time;
#endif
}

void
phydm_init_soft_ml_setting(
	void		*p_dm_void
)
{
}

