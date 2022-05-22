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

void phydm_dynamicsoftmletting(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 ret_val;

#if (RTL8822B_SUPPORT == 1)
	if (!*dm->mp_mode) {
		if (dm->support_ic_type & ODM_RTL8822B) {
			if (!dm->is_linked | dm->iot_table.is_linked_cmw500)
				return;

			if (dm->bsomlenabled) {
				PHYDM_DBG(dm, ODM_COMP_API,
					  "PHYDM_DynamicSoftMLSetting(): SoML has been enable, skip dynamic SoML switch\n");
				return;
			}

			ret_val = odm_get_bb_reg(dm, R_0xf8c, MASKBYTE0);
			PHYDM_DBG(dm, ODM_COMP_API,
				  "PHYDM_DynamicSoftMLSetting(): Read 0xF8C = 0x%08X\n",
				  ret_val);

			if (ret_val < 0x16) {
				PHYDM_DBG(dm, ODM_COMP_API,
					  "PHYDM_DynamicSoftMLSetting(): 0xF8C(== 0x%08X) < 0x16, enable SoML\n",
					  ret_val);
				phydm_somlrxhp_setting(dm, true);
				/*odm_set_bb_reg(dm, R_0x19a8, MASKDWORD, 0xc10a0000);*/
				dm->bsomlenabled = true;
			}
		}
	}
#endif
}

#ifdef CONFIG_ADAPTIVE_SOML
void phydm_soml_on_off(
	void *dm_void,
	u8 swch)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	if (swch == SOML_ON) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML, "(( Turn on )) SOML\n");

		if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
			odm_set_bb_reg(dm, R_0x998, BIT(6), swch);
#if (RTL8822B_SUPPORT == 1)
		else if (dm->support_ic_type == ODM_RTL8822B)
			phydm_somlrxhp_setting(dm, true);
#endif

	} else if (swch == SOML_OFF) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML, "(( Turn off )) SOML\n");

		if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
			odm_set_bb_reg(dm, R_0x998, BIT(6), swch);
#if (RTL8822B_SUPPORT == 1)
		else if (dm->support_ic_type == ODM_RTL8822B)
			phydm_somlrxhp_setting(dm, false);
#endif
	}
	dm_soml_table->soml_on_off = swch;
}

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
void phydm_adaptive_soml_callback(
	struct phydm_timer_list *timer)
{
	void *adapter = (void *)timer->Adapter;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(((PADAPTER)adapter));
	struct dm_struct *dm = &hal_data->DM_OutSrc;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	#if DEV_BUS_TYPE == RT_PCI_INTERFACE
	#if USE_WORKITEM
	odm_schedule_work_item(&dm_soml_table->phydm_adaptive_soml_workitem);
	#else
	{
		/*dbg_print("phydm_adaptive_soml-phydm_adaptive_soml_callback\n");*/
		phydm_adsl(dm);
	}
	#endif
	#else
	odm_schedule_work_item(&dm_soml_table->phydm_adaptive_soml_workitem);
	#endif
}

void phydm_adaptive_soml_workitem_callback(
	void *context)
{
#ifdef CONFIG_ADAPTIVE_SOML
	void *adapter = (void *)context;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(((PADAPTER)adapter));
	struct dm_struct *dm = &hal_data->DM_OutSrc;

	/*dbg_print("phydm_adaptive_soml-phydm_adaptive_soml_workitem_callback\n");*/
	phydm_adsl(dm);
#endif
}

#elif (DM_ODM_SUPPORT_TYPE == ODM_CE)
void phydm_adaptive_soml_callback(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	void *padapter = dm->adapter;

	if (*dm->is_net_closed == true)
		return;
	if (dm->support_interface == ODM_ITRF_PCIE)
		phydm_adsl(dm);
	else {
		/* Can't do I/O in timer callback*/
		rtw_run_in_thread_cmd(padapter, phydm_adaptive_soml_workitem_callback, padapter);
	}
}

void phydm_adaptive_soml_workitem_callback(
	void *context)
{
	void *adapter = (void *)context;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(((PADAPTER)adapter));
	struct dm_struct *dm = &hal_data->odmpriv;

	/*dbg_print("phydm_adaptive_soml-phydm_adaptive_soml_workitem_callback\n");*/
	phydm_adsl(dm);
}

#else
void phydm_adaptive_soml_callback(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	PHYDM_DBG(dm, DBG_ADPTV_SOML, "******SOML_Callback******\n");
	phydm_adsl(dm);
}
#endif

void phydm_rx_rate_for_soml(
	void *dm_void,
	void *pkt_info_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;
	struct phydm_perpkt_info_struct *pktinfo = (struct phydm_perpkt_info_struct *)pkt_info_void;
	u8 data_rate = (pktinfo->data_rate & 0x7f);

	if (pktinfo->data_rate >= ODM_RATEMCS0 && pktinfo->data_rate <= ODM_RATEMCS31)
		dm_soml_table->num_ht_cnt[data_rate - ODM_RATEMCS0]++;
	else if ((pktinfo->data_rate >= ODM_RATEVHTSS1MCS0) && (pktinfo->data_rate <= ODM_RATEVHTSS4MCS9))
		dm_soml_table->num_vht_cnt[data_rate - ODM_RATEVHTSS1MCS0]++;
}

void phydm_rx_qam_for_soml(
	void *dm_void,
	void *pkt_info_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;
	struct phydm_perpkt_info_struct *pktinfo = (struct phydm_perpkt_info_struct *)pkt_info_void;
	u8 date_rate = (pktinfo->data_rate & 0x7f);

	if (dm_soml_table->soml_state_cnt < (dm_soml_table->soml_train_num << 1)) {
		if (dm_soml_table->soml_on_off == SOML_ON)
			return;
		else if (dm_soml_table->soml_on_off == SOML_OFF) {
			if (date_rate >= ODM_RATEMCS8 && date_rate <= ODM_RATEMCS10)
				dm_soml_table->num_ht_qam[BPSK_QPSK]++;

			else if ((date_rate >= ODM_RATEMCS11) && (date_rate <= ODM_RATEMCS12))
				dm_soml_table->num_ht_qam[QAM16]++;

			else if ((date_rate >= ODM_RATEMCS13) && (date_rate <= ODM_RATEMCS15))
				dm_soml_table->num_ht_qam[QAM64]++;

			else if ((date_rate >= ODM_RATEVHTSS2MCS0) && (date_rate <= ODM_RATEVHTSS2MCS2))
				dm_soml_table->num_vht_qam[BPSK_QPSK]++;

			else if ((date_rate >= ODM_RATEVHTSS2MCS3) && (date_rate <= ODM_RATEVHTSS2MCS4))
				dm_soml_table->num_vht_qam[QAM16]++;

			else if ((date_rate >= ODM_RATEVHTSS2MCS5) && (date_rate <= ODM_RATEVHTSS2MCS5))
				dm_soml_table->num_vht_qam[QAM64]++;

			else if ((date_rate >= ODM_RATEVHTSS2MCS8) && (date_rate <= ODM_RATEVHTSS2MCS9))
				dm_soml_table->num_vht_qam[QAM256]++;
		}
	}
}

void phydm_soml_reset_rx_rate(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;
	u8 order;

	for (order = 0; order < HT_RATE_IDX; order++)
		dm_soml_table->num_ht_cnt[order] = 0;

	for (order = 0; order < VHT_RATE_IDX; order++)
		dm_soml_table->num_vht_cnt[order] = 0;
}

void phydm_soml_reset_qam(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;
	u8 order;

	for (order = 0; order < HT_ORDER_TYPE; order++)
		dm_soml_table->num_ht_qam[order] = 0;

	for (order = 0; order < VHT_ORDER_TYPE; order++)
		dm_soml_table->num_vht_qam[order] = 0;
}

void phydm_soml_cfo_process(
	void *dm_void,
	s32 *diff_a,
	s32 *diff_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 value32, value32_1, value32_2, value32_3;
	s32 cfo_acq_a, cfo_acq_b, cfo_end_a, cfo_end_b;

	value32 = odm_get_bb_reg(dm, R_0xd10, MASKDWORD);
	value32_1 = odm_get_bb_reg(dm, R_0xd14, MASKDWORD);
	value32_2 = odm_get_bb_reg(dm, R_0xd50, MASKDWORD);
	value32_3 = odm_get_bb_reg(dm, R_0xd54, MASKDWORD);

	cfo_acq_a = (s32)((value32 & 0x1fff0000) >> 16);
	cfo_end_a = (s32)((value32_1 & 0x1fff0000) >> 16);
	cfo_acq_b = (s32)((value32_2 & 0x1fff0000) >> 16);
	cfo_end_b = (s32)((value32_3 & 0x1fff0000) >> 16);

	*diff_a = ((cfo_acq_a >= cfo_end_a) ? (cfo_acq_a - cfo_end_a) : (cfo_end_a - cfo_acq_a));
	*diff_b = ((cfo_acq_b >= cfo_end_b) ? (cfo_acq_b - cfo_end_b) : (cfo_end_b - cfo_acq_b));

	*diff_a = ((*diff_a * 312) + (*diff_a >> 1)) >> 12; /* 312.5/2^12 */
	*diff_b = ((*diff_b * 312) + (*diff_b >> 1)) >> 12; /* 312.5/2^12 */
}

void phydm_soml_debug(
	void *dm_void,
	u32 *const dm_value,
	u32 *_used,
	char *output,
	u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;
	u32 used = *_used;
	u32 out_len = *_out_len;

	if (dm_value[0] == 1) { /*Turn on/off SOML*/
		dm_soml_table->soml_select = (u8)dm_value[1];

	} else if (dm_value[0] == 2) { /*training number for SOML*/

		dm_soml_table->soml_train_num = (u8)dm_value[1];
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_train_num = ((%d))\n",
			 dm_soml_table->soml_train_num);
	} else if (dm_value[0] == 3) { /*training interval for SOML*/

		dm_soml_table->soml_intvl = (u8)dm_value[1];
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_intvl = ((%d))\n", dm_soml_table->soml_intvl);
	} else if (dm_value[0] == 4) { /*function period for SOML*/

		dm_soml_table->soml_period = (u8)dm_value[1];
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_period = ((%d))\n", dm_soml_table->soml_period);
	} else if (dm_value[0] == 5) { /*delay_time for SOML*/

		dm_soml_table->soml_delay_time = (u8)dm_value[1];
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_delay_time = ((%d))\n",
			 dm_soml_table->soml_delay_time);
	} else if (dm_value[0] == 6) { /* for SOML Rx QAM distribution th*/
		if (dm_value[1] == 256) {
			dm_soml_table->qam256_dist_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "qam256_dist_th = ((%d))\n",
				 dm_soml_table->qam256_dist_th);
		} else if (dm_value[1] == 64) {
			dm_soml_table->qam64_dist_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "qam64_dist_th = ((%d))\n",
				 dm_soml_table->qam64_dist_th);
		} else if (dm_value[1] == 16) {
			dm_soml_table->qam16_dist_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "qam16_dist_th = ((%d))\n",
				 dm_soml_table->qam16_dist_th);
		} else if (dm_value[1] == 4) {
			dm_soml_table->bpsk_qpsk_dist_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "bpsk_qpsk_dist_th = ((%d))\n",
				 dm_soml_table->bpsk_qpsk_dist_th);
		}
	} else if (dm_value[0] == 7) { /* for SOML cfo th*/
		if (dm_value[1] == 256) {
			dm_soml_table->cfo_qam256_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "cfo_qam256_th = ((%d KHz))\n",
				 dm_soml_table->cfo_qam256_th);
		} else if (dm_value[1] == 64) {
			dm_soml_table->cfo_qam64_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "cfo_qam64_th = ((%d KHz))\n",
				 dm_soml_table->cfo_qam64_th);
		} else if (dm_value[1] == 16) {
			dm_soml_table->cfo_qam16_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "cfo_qam16_th = ((%d KHz))\n",
				 dm_soml_table->cfo_qam16_th);
		} else if (dm_value[1] == 4) {
			dm_soml_table->cfo_qpsk_th = (u8)dm_value[2];
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "cfo_qpsk_th = ((%d KHz))\n",
				 dm_soml_table->cfo_qpsk_th);
		}
	} else if (dm_value[0] == 100) { /*show parameters*/
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_select = ((%d))\n", dm_soml_table->soml_select);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_train_num = ((%d))\n",
			 dm_soml_table->soml_train_num);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_intvl = ((%d))\n", dm_soml_table->soml_intvl);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_period = ((%d))\n", dm_soml_table->soml_period);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "soml_delay_time = ((%d))\n\n",
			 dm_soml_table->soml_delay_time);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "qam256_dist_th = ((%d)),  qam64_dist_th = ((%d)), ",
			 dm_soml_table->qam256_dist_th,
			 dm_soml_table->qam64_dist_th);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "qam16_dist_th = ((%d)),  bpsk_qpsk_dist_th = ((%d))\n",
			 dm_soml_table->qam16_dist_th,
			 dm_soml_table->bpsk_qpsk_dist_th);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "cfo_qam256_th = ((%d KHz)),  cfo_qam64_th = ((%d KHz)), ",
			 dm_soml_table->cfo_qam256_th,
			 dm_soml_table->cfo_qam64_th);
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "cfo_qam16_th = ((%d KHz)),  cfo_qpsk_th  = ((%d KHz))\n",
			 dm_soml_table->cfo_qam16_th,
			 dm_soml_table->cfo_qpsk_th);
	}
	*_used = used;
	*_out_len = out_len;
}

void phydm_soml_statistics(
	void *dm_void,
	u8 on_off_state

	)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	u8 i, j;
	u32 num_bytes_diff, num_rate_diff;

	if (on_off_state == SOML_ON) {
		if (*dm->channel <= 14) {
			for (i = ODM_RATEMCS0; i <= ODM_RATEMCS15; i++) {
				num_rate_diff = dm_soml_table->num_ht_cnt[i - ODM_RATEMCS0] - dm_soml_table->pre_num_ht_cnt[i - ODM_RATEMCS0];
				dm_soml_table->num_ht_cnt_on[i - ODM_RATEMCS0] += num_rate_diff;
				dm_soml_table->pre_num_ht_cnt[i - ODM_RATEMCS0] = dm_soml_table->num_ht_cnt[i - ODM_RATEMCS0];
				num_bytes_diff = dm_soml_table->num_ht_bytes[i - ODM_RATEMCS0] - dm_soml_table->pre_num_ht_bytes[i - ODM_RATEMCS0];
				dm_soml_table->num_ht_bytes_on[i - ODM_RATEMCS0] += num_bytes_diff;
				dm_soml_table->pre_num_ht_bytes[i - ODM_RATEMCS0] = dm_soml_table->num_ht_bytes[i - ODM_RATEMCS0];
			}
		}
		if (dm->support_ic_type == ODM_RTL8822B) {
			for (j = ODM_RATEVHTSS1MCS0; j <= ODM_RATEVHTSS2MCS9; j++) {
				num_rate_diff = dm_soml_table->num_vht_cnt[j - ODM_RATEVHTSS1MCS0] - dm_soml_table->pre_num_vht_cnt[j - ODM_RATEVHTSS1MCS0];
				dm_soml_table->num_vht_cnt_on[j - ODM_RATEVHTSS1MCS0] += num_rate_diff;
				dm_soml_table->pre_num_vht_cnt[j - ODM_RATEVHTSS1MCS0] = dm_soml_table->num_vht_cnt[j - ODM_RATEVHTSS1MCS0];
				num_bytes_diff = dm_soml_table->num_vht_bytes[j - ODM_RATEVHTSS1MCS0] - dm_soml_table->pre_num_vht_bytes[j - ODM_RATEVHTSS1MCS0];
				dm_soml_table->num_vht_bytes_on[j - ODM_RATEVHTSS1MCS0] += num_bytes_diff;
				dm_soml_table->pre_num_vht_bytes[j - ODM_RATEVHTSS1MCS0] = dm_soml_table->num_vht_bytes[j - ODM_RATEVHTSS1MCS0];
			}
		}
	} else if (on_off_state == SOML_OFF) {
		if (*dm->channel <= 14) {
			for (i = ODM_RATEMCS0; i <= ODM_RATEMCS15; i++) {
				num_rate_diff = dm_soml_table->num_ht_cnt[i - ODM_RATEMCS0] - dm_soml_table->pre_num_ht_cnt[i - ODM_RATEMCS0];
				dm_soml_table->num_ht_cnt_off[i - ODM_RATEMCS0] += num_rate_diff;
				dm_soml_table->pre_num_ht_cnt[i - ODM_RATEMCS0] = dm_soml_table->num_ht_cnt[i - ODM_RATEMCS0];
				num_bytes_diff = dm_soml_table->num_ht_bytes[i - ODM_RATEMCS0] - dm_soml_table->pre_num_ht_bytes[i - ODM_RATEMCS0];
				dm_soml_table->num_ht_bytes_off[i - ODM_RATEMCS0] += num_bytes_diff;
				dm_soml_table->pre_num_ht_bytes[i - ODM_RATEMCS0] = dm_soml_table->num_ht_bytes[i - ODM_RATEMCS0];
			}
		}
		if (dm->support_ic_type == ODM_RTL8822B) {
			for (j = ODM_RATEVHTSS1MCS0; j <= ODM_RATEVHTSS2MCS9; j++) {
				num_rate_diff = dm_soml_table->num_vht_cnt[j - ODM_RATEVHTSS1MCS0] - dm_soml_table->pre_num_vht_cnt[j - ODM_RATEVHTSS1MCS0];
				dm_soml_table->num_vht_cnt_off[j - ODM_RATEVHTSS1MCS0] += num_rate_diff;
				dm_soml_table->pre_num_vht_cnt[j - ODM_RATEVHTSS1MCS0] = dm_soml_table->num_vht_cnt[j - ODM_RATEVHTSS1MCS0];
				num_bytes_diff = dm_soml_table->num_vht_bytes[j - ODM_RATEVHTSS1MCS0] - dm_soml_table->pre_num_vht_bytes[j - ODM_RATEVHTSS1MCS0];
				dm_soml_table->num_vht_bytes_off[j - ODM_RATEVHTSS1MCS0] += num_bytes_diff;
				dm_soml_table->pre_num_vht_bytes[j - ODM_RATEVHTSS1MCS0] = dm_soml_table->num_vht_bytes[j - ODM_RATEVHTSS1MCS0];
			}
		}
	}
}

void phydm_adsl(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	u8 i;
	u8 next_on_off;
	u8 rate_num = 1, rate_ss_shift = 0;
	u32 byte_total_on = 0, byte_total_off = 0, num_total_qam = 0;
	u32 num_ht_total_cnt_on = 0, num_ht_total_cnt_off = 0, total_ht_rate_on = 0, total_ht_rate_off = 0;
	u32 num_vht_total_cnt_on = 0, num_vht_total_cnt_off = 0, total_vht_rate_on = 0, total_vht_rate_off = 0;
	u32 rate_per_pkt_on = 0, rate_per_pkt_off = 0;
	u32 ht_reset[HT_RATE_IDX] = {0}, vht_reset[VHT_RATE_IDX] = {0};
	u8 size = sizeof(ht_reset[0]);
	u16 vht_phy_rate_table[] = {
		/*20M*/
		6, 13, 19, 26, 39, 52, 58, 65, 78, 90, /*1SS MCS0~9*/
		13, 26, 39, 52, 78, 104, 117, 130, 156, 180 /*2SSMCS0~9*/
	};

	if (dm->support_ic_type & ODM_IC_4SS)
		rate_num = 4;
	else if (dm->support_ic_type & ODM_IC_3SS)
		rate_num = 3;
	else if (dm->support_ic_type & ODM_IC_2SS)
		rate_num = 2;

	if (dm->support_ic_type & ODM_ADAPTIVE_SOML_SUPPORT_IC) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML, "soml_state_cnt =((%d))\n",
			  dm_soml_table->soml_state_cnt);
		/*Traning state: 0(alt) 1(ori) 2(alt) 3(ori)============================================================*/
		if (dm_soml_table->soml_state_cnt < (dm_soml_table->soml_train_num << 1)) {
			if (dm_soml_table->soml_state_cnt == 0) {
				phydm_soml_reset_rx_rate(dm);
				odm_move_memory(dm, dm_soml_table->num_ht_bytes, ht_reset, HT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->num_ht_bytes_on, ht_reset, HT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->num_ht_bytes_off, ht_reset, HT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->num_vht_bytes, vht_reset, VHT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->num_vht_bytes_on, vht_reset, VHT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->num_vht_bytes_off, vht_reset, VHT_RATE_IDX * size);
				if (dm->support_ic_type == ODM_RTL8822B) {
					dm_soml_table->cfo_counter++;
					phydm_soml_cfo_process(dm,
							       &dm_soml_table->cfo_diff_a,
							       &dm_soml_table->cfo_diff_b);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ (%d) cfo_diff_a = %d KHz; cfo_diff_b = %d KHz ]\n", dm_soml_table->cfo_counter, dm_soml_table->cfo_diff_a, dm_soml_table->cfo_diff_b);
					dm_soml_table->cfo_diff_sum_a += dm_soml_table->cfo_diff_a;
					dm_soml_table->cfo_diff_sum_b += dm_soml_table->cfo_diff_b;
				}

				dm_soml_table->is_soml_method_enable = 1;
				dm_soml_table->soml_state_cnt++;
				next_on_off = (dm_soml_table->soml_on_off == SOML_ON) ? SOML_ON : SOML_OFF;
				phydm_soml_on_off(dm, next_on_off);
				odm_set_timer(dm, &dm_soml_table->phydm_adaptive_soml_timer, dm_soml_table->soml_delay_time); /*ms*/
			} else if ((dm_soml_table->soml_state_cnt % 2) != 0) {
				dm_soml_table->soml_state_cnt++;
				odm_move_memory(dm, dm_soml_table->pre_num_ht_cnt, dm_soml_table->num_ht_cnt, HT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->pre_num_vht_cnt, dm_soml_table->num_vht_cnt, VHT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->pre_num_ht_bytes, dm_soml_table->num_ht_bytes, HT_RATE_IDX * size);
				odm_move_memory(dm, dm_soml_table->pre_num_vht_bytes, dm_soml_table->num_vht_bytes, VHT_RATE_IDX * size);

				if (dm->support_ic_type == ODM_RTL8822B) {
					dm_soml_table->cfo_counter++;
					phydm_soml_cfo_process(dm,
							       &dm_soml_table->cfo_diff_a,
							       &dm_soml_table->cfo_diff_b);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ (%d) cfo_diff_a = %d KHz; cfo_diff_b = %d KHz ]\n", dm_soml_table->cfo_counter, dm_soml_table->cfo_diff_a, dm_soml_table->cfo_diff_b);
					dm_soml_table->cfo_diff_sum_a += dm_soml_table->cfo_diff_a;
					dm_soml_table->cfo_diff_sum_b += dm_soml_table->cfo_diff_b;
				}
				odm_set_timer(dm, &dm_soml_table->phydm_adaptive_soml_timer, dm_soml_table->soml_intvl); /*ms*/
			} else if ((dm_soml_table->soml_state_cnt % 2) == 0) {
				if (dm->support_ic_type == ODM_RTL8822B) {
					dm_soml_table->cfo_counter++;
					phydm_soml_cfo_process(dm,
							       &dm_soml_table->cfo_diff_a,
							       &dm_soml_table->cfo_diff_b);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ (%d) cfo_diff_a = %d KHz; cfo_diff_b = %d KHz ]\n", dm_soml_table->cfo_counter, dm_soml_table->cfo_diff_a, dm_soml_table->cfo_diff_b);
					dm_soml_table->cfo_diff_sum_a += dm_soml_table->cfo_diff_a;
					dm_soml_table->cfo_diff_sum_b += dm_soml_table->cfo_diff_b;
				}
				dm_soml_table->soml_state_cnt++;
				phydm_soml_statistics(dm, dm_soml_table->soml_on_off);
				next_on_off = (dm_soml_table->soml_on_off == SOML_ON) ? SOML_OFF : SOML_ON;
				phydm_soml_on_off(dm, next_on_off);
				odm_set_timer(dm, &dm_soml_table->phydm_adaptive_soml_timer, dm_soml_table->soml_delay_time); /*ms*/
			}
		}
		/*Decision state: ==============================================================*/
		else {
			PHYDM_DBG(dm, DBG_ADPTV_SOML, "[Decisoin state ]\n");
			phydm_soml_statistics(dm, dm_soml_table->soml_on_off);
			if (*dm->channel <= 14) {
				/* [Search 1st and 2nd rate by counter] */
				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = (i << 3);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "*num_ht_cnt_on  HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
						  (rate_ss_shift), (rate_ss_shift + 7),
						  dm_soml_table->num_ht_cnt_on[rate_ss_shift + 0], dm_soml_table->num_ht_cnt_on[rate_ss_shift + 1],
						  dm_soml_table->num_ht_cnt_on[rate_ss_shift + 2], dm_soml_table->num_ht_cnt_on[rate_ss_shift + 3],
						  dm_soml_table->num_ht_cnt_on[rate_ss_shift + 4], dm_soml_table->num_ht_cnt_on[rate_ss_shift + 5],
						  dm_soml_table->num_ht_cnt_on[rate_ss_shift + 6], dm_soml_table->num_ht_cnt_on[rate_ss_shift + 7]);
				}

				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = (i << 3);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "*num_ht_bytes_off  HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
						  (rate_ss_shift), (rate_ss_shift + 7),
						  dm_soml_table->num_ht_cnt_off[rate_ss_shift + 0], dm_soml_table->num_ht_cnt_off[rate_ss_shift + 1],
						  dm_soml_table->num_ht_cnt_off[rate_ss_shift + 2], dm_soml_table->num_ht_cnt_off[rate_ss_shift + 3],
						  dm_soml_table->num_ht_cnt_off[rate_ss_shift + 4], dm_soml_table->num_ht_cnt_off[rate_ss_shift + 5],
						  dm_soml_table->num_ht_cnt_off[rate_ss_shift + 6], dm_soml_table->num_ht_cnt_off[rate_ss_shift + 7]);
				}

				for (i = ODM_RATEMCS8; i <= ODM_RATEMCS15; i++) {
					num_ht_total_cnt_on += dm_soml_table->num_ht_cnt_on[i - ODM_RATEMCS0];
					num_ht_total_cnt_off += dm_soml_table->num_ht_cnt_off[i - ODM_RATEMCS0];
					total_ht_rate_on += (dm_soml_table->num_ht_cnt_on[i - ODM_RATEMCS0] * phy_rate_table[i]);
					total_ht_rate_off += (dm_soml_table->num_ht_cnt_off[i - ODM_RATEMCS0] * phy_rate_table[i]);
				}
				rate_per_pkt_on = (num_ht_total_cnt_on != 0) ? ((total_ht_rate_on << 3) / num_ht_total_cnt_on) : 0;
				rate_per_pkt_off = (num_ht_total_cnt_off != 0) ? ((total_ht_rate_off << 3) / num_ht_total_cnt_off) : 0;
			}
#if 0
			if (*dm->channel <= 14) {
			/* [Search 1st and 2ed rate by counter] */
				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = (i << 3);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "*num_ht_bytes_on  HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
						(rate_ss_shift), (rate_ss_shift + 7),
						dm_soml_table->num_ht_bytes_on[rate_ss_shift + 0], dm_soml_table->num_ht_bytes_on[rate_ss_shift + 1],
						dm_soml_table->num_ht_bytes_on[rate_ss_shift + 2], dm_soml_table->num_ht_bytes_on[rate_ss_shift + 3],
						dm_soml_table->num_ht_bytes_on[rate_ss_shift + 4], dm_soml_table->num_ht_bytes_on[rate_ss_shift + 5],
						dm_soml_table->num_ht_bytes_on[rate_ss_shift + 6], dm_soml_table->num_ht_bytes_on[rate_ss_shift + 7]);
				}

				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = (i << 3);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "*num_ht_bytes_off  HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
						(rate_ss_shift), (rate_ss_shift + 7),
						dm_soml_table->num_ht_bytes_off[rate_ss_shift + 0], dm_soml_table->num_ht_bytes_off[rate_ss_shift + 1],
						dm_soml_table->num_ht_bytes_off[rate_ss_shift + 2], dm_soml_table->num_ht_bytes_off[rate_ss_shift + 3],
						dm_soml_table->num_ht_bytes_off[rate_ss_shift + 4], dm_soml_table->num_ht_bytes_off[rate_ss_shift + 5],
						dm_soml_table->num_ht_bytes_off[rate_ss_shift + 6], dm_soml_table->num_ht_bytes_off[rate_ss_shift + 7]);
				}

				for (i = ODM_RATEMCS8; i <= ODM_RATEMCS15; i++) {
					byte_total_on += dm_soml_table->num_ht_bytes_on[i - ODM_RATEMCS0];
					byte_total_off += dm_soml_table->num_ht_bytes_off[i - ODM_RATEMCS0];
				}
			}
#endif

			if (dm->support_ic_type == ODM_RTL8822B) {
				dm_soml_table->cfo_diff_avg_a = (dm_soml_table->cfo_counter != 0) ? (dm_soml_table->cfo_diff_sum_a / dm_soml_table->cfo_counter) : 0;
				dm_soml_table->cfo_diff_avg_b = (dm_soml_table->cfo_counter != 0) ? (dm_soml_table->cfo_diff_sum_b / dm_soml_table->cfo_counter) : 0;
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ cfo_diff_avg_a = %d KHz; cfo_diff_avg_b = %d KHz]\n",
					  dm_soml_table->cfo_diff_avg_a,
					  dm_soml_table->cfo_diff_avg_b);
				for (i = 0; i < VHT_ORDER_TYPE; i++)
					num_total_qam += dm_soml_table->num_vht_qam[i];

				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ ((2SS)) BPSK_QPSK_count = %d ; 16QAM_count = %d ; 64QAM_count = %d ; 256QAM_count = %d ; num_total_qam = %d]\n",
					  dm_soml_table->num_vht_qam[BPSK_QPSK],
					  dm_soml_table->num_vht_qam[QAM16],
					  dm_soml_table->num_vht_qam[QAM64],
					  dm_soml_table->num_vht_qam[QAM256],
					  num_total_qam);
				if (((dm_soml_table->num_vht_qam[QAM256] * 100) > (num_total_qam * dm_soml_table->qam256_dist_th)) && dm_soml_table->cfo_diff_avg_a > dm_soml_table->cfo_qam256_th && dm_soml_table->cfo_diff_avg_b > dm_soml_table->cfo_qam256_th) {
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  QAM256_ratio > %d ; cfo_diff_avg_a > %d KHz ==> SOML_OFF]\n", dm_soml_table->qam256_dist_th, dm_soml_table->cfo_qam256_th);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Final decisoin ] : ");
					phydm_soml_on_off(dm, SOML_OFF);
					return;
				} else if (((dm_soml_table->num_vht_qam[QAM64] * 100) > (num_total_qam * dm_soml_table->qam64_dist_th)) && (dm_soml_table->cfo_diff_avg_a > dm_soml_table->cfo_qam64_th) && (dm_soml_table->cfo_diff_avg_b > dm_soml_table->cfo_qam64_th)) {
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  QAM64_ratio > %d ; cfo_diff_avg_a > %d KHz ==> SOML_OFF]\n", dm_soml_table->qam64_dist_th, dm_soml_table->cfo_qam64_th);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Final decisoin ] : ");
					phydm_soml_on_off(dm, SOML_OFF);
					return;
				} else if (((dm_soml_table->num_vht_qam[QAM16] * 100) > (num_total_qam * dm_soml_table->qam16_dist_th)) && (dm_soml_table->cfo_diff_avg_a > dm_soml_table->cfo_qam16_th) && (dm_soml_table->cfo_diff_avg_b > dm_soml_table->cfo_qam16_th)) {
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  QAM16_ratio > %d ; cfo_diff_avg_a > %d KHz ==> SOML_OFF]\n", dm_soml_table->qam16_dist_th, dm_soml_table->cfo_qam16_th);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Final decisoin ] : ");
					phydm_soml_on_off(dm, SOML_OFF);
					return;
				} else if (((dm_soml_table->num_vht_qam[BPSK_QPSK] * 100) > (num_total_qam * dm_soml_table->bpsk_qpsk_dist_th)) && (dm_soml_table->cfo_diff_avg_a > dm_soml_table->cfo_qpsk_th) && (dm_soml_table->cfo_diff_avg_b > dm_soml_table->cfo_qpsk_th)) {
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  BPSK_QPSK_ratio > %d ; cfo_diff_avg_a > %d KHz ==> SOML_OFF]\n", dm_soml_table->bpsk_qpsk_dist_th, dm_soml_table->cfo_qpsk_th);
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Final decisoin ] : ");
					phydm_soml_on_off(dm, SOML_OFF);
					return;
				}

				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = 10 * i;
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  num_vht_cnt_on  VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d} ]\n",
						  (i + 1),
						  dm_soml_table->num_vht_cnt_on[rate_ss_shift + 0], dm_soml_table->num_vht_cnt_on[rate_ss_shift + 1],
						  dm_soml_table->num_vht_cnt_on[rate_ss_shift + 2], dm_soml_table->num_vht_cnt_on[rate_ss_shift + 3],
						  dm_soml_table->num_vht_cnt_on[rate_ss_shift + 4], dm_soml_table->num_vht_cnt_on[rate_ss_shift + 5],
						  dm_soml_table->num_vht_cnt_on[rate_ss_shift + 6], dm_soml_table->num_vht_cnt_on[rate_ss_shift + 7],
						  dm_soml_table->num_vht_cnt_on[rate_ss_shift + 8], dm_soml_table->num_vht_cnt_on[rate_ss_shift + 9]);
				}

				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = 10 * i;
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  num_vht_cnt_off  VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d} ]\n",
						  (i + 1),
						  dm_soml_table->num_vht_cnt_off[rate_ss_shift + 0], dm_soml_table->num_vht_cnt_off[rate_ss_shift + 1],
						  dm_soml_table->num_vht_cnt_off[rate_ss_shift + 2], dm_soml_table->num_vht_cnt_off[rate_ss_shift + 3],
						  dm_soml_table->num_vht_cnt_off[rate_ss_shift + 4], dm_soml_table->num_vht_cnt_off[rate_ss_shift + 5],
						  dm_soml_table->num_vht_cnt_off[rate_ss_shift + 6], dm_soml_table->num_vht_cnt_off[rate_ss_shift + 7],
						  dm_soml_table->num_vht_cnt_off[rate_ss_shift + 8], dm_soml_table->num_vht_cnt_off[rate_ss_shift + 9]);
				}

				for (i = ODM_RATEVHTSS2MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
					num_vht_total_cnt_on += dm_soml_table->num_vht_cnt_on[i - ODM_RATEVHTSS1MCS0];
					num_vht_total_cnt_off += dm_soml_table->num_vht_cnt_off[i - ODM_RATEVHTSS1MCS0];
					total_vht_rate_on += (dm_soml_table->num_vht_cnt_on[i - ODM_RATEVHTSS1MCS0] * vht_phy_rate_table[i - ODM_RATEVHTSS1MCS0]);
					total_vht_rate_off += (dm_soml_table->num_vht_cnt_off[i - ODM_RATEVHTSS1MCS0] * vht_phy_rate_table[i - ODM_RATEVHTSS1MCS0]);
				}
				rate_per_pkt_on = (num_vht_total_cnt_on != 0) ? ((total_vht_rate_on << 3) / num_vht_total_cnt_on) : 0;
				rate_per_pkt_off = (num_vht_total_cnt_off != 0) ? ((total_vht_rate_off << 3) / num_vht_total_cnt_off) : 0;

				#if 0
				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = 10 * i;
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  num_vht_bytes_on  VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d} ]\n",
						(i + 1),
						dm_soml_table->num_vht_bytes_on[rate_ss_shift + 0], dm_soml_table->num_vht_bytes_on[rate_ss_shift + 1],
						dm_soml_table->num_vht_bytes_on[rate_ss_shift + 2], dm_soml_table->num_vht_bytes_on[rate_ss_shift + 3],
						dm_soml_table->num_vht_bytes_on[rate_ss_shift + 4], dm_soml_table->num_vht_bytes_on[rate_ss_shift + 5],
						dm_soml_table->num_vht_bytes_on[rate_ss_shift + 6], dm_soml_table->num_vht_bytes_on[rate_ss_shift + 7],
						dm_soml_table->num_vht_bytes_on[rate_ss_shift + 8], dm_soml_table->num_vht_bytes_on[rate_ss_shift + 9]);
				}

				for (i = 0; i < rate_num; i++) {
					rate_ss_shift = 10 * i;
					PHYDM_DBG(dm, DBG_ADPTV_SOML, "[  num_vht_bytes_off  VHT-%d ss MCS[0:9] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d} ]\n",
						(i + 1),
						dm_soml_table->num_vht_bytes_off[rate_ss_shift + 0], dm_soml_table->num_vht_bytes_off[rate_ss_shift + 1],
						dm_soml_table->num_vht_bytes_off[rate_ss_shift + 2], dm_soml_table->num_vht_bytes_off[rate_ss_shift + 3],
						dm_soml_table->num_vht_bytes_off[rate_ss_shift + 4], dm_soml_table->num_vht_bytes_off[rate_ss_shift + 5],
						dm_soml_table->num_vht_bytes_off[rate_ss_shift + 6], dm_soml_table->num_vht_bytes_off[rate_ss_shift + 7],
						dm_soml_table->num_vht_bytes_off[rate_ss_shift + 8], dm_soml_table->num_vht_bytes_off[rate_ss_shift + 9]);
				}

				for (i = ODM_RATEVHTSS2MCS0; i <= ODM_RATEVHTSS2MCS9; i++) {
					byte_total_on += dm_soml_table->num_vht_bytes_on[i - ODM_RATEVHTSS1MCS0];
					byte_total_off += dm_soml_table->num_vht_bytes_off[i - ODM_RATEVHTSS1MCS0];
				}
				#endif
			}

			/* [Decision] */
			PHYDM_DBG(dm, DBG_ADPTV_SOML,
				  "[  rate_per_pkt_on = %d ; rate_per_pkt_off = %d ]\n",
				  rate_per_pkt_on, rate_per_pkt_off);
			if (rate_per_pkt_on > rate_per_pkt_off) {
				next_on_off = SOML_ON;
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ rate_per_pkt_on > rate_per_pkt_off ==> SOML_ON ]\n");
			} else if (rate_per_pkt_on < rate_per_pkt_off) {
				next_on_off = SOML_OFF;
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ rate_per_pkt_on < rate_per_pkt_off ==> SOML_OFF ]\n");
			} else {
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ stay at soml_last_state ]\n");
				next_on_off = dm_soml_table->soml_last_state;
			}
			#if 0
			PHYDM_DBG(dm, DBG_ADPTV_SOML,
				  "[  byte_total_on = %d ; byte_total_off = %d ]\n",
				  byte_total_on, byte_total_off);
			if (byte_total_on > byte_total_off) {
				next_on_off = SOML_ON;
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ byte_total_on > byte_total_off ==> SOML_ON ]\n");
			} else if (byte_total_on < byte_total_off) {
				next_on_off = SOML_OFF;
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ byte_total_on < byte_total_off ==> SOML_OFF ]\n");
			} else {
				PHYDM_DBG(dm, DBG_ADPTV_SOML,
					  "[ stay at soml_last_state ]\n");
				next_on_off = dm_soml_table->soml_last_state;
			}
			#endif

			PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Final decisoin ] : ");
			phydm_soml_on_off(dm, next_on_off);
			dm_soml_table->soml_last_state = next_on_off;
		}
	}
}
void phydm_adaptive_soml_reset(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	dm_soml_table->soml_state_cnt = 0;
	dm_soml_table->is_soml_method_enable = 0;
	dm_soml_table->soml_counter = 0;
}

void phydm_set_adsl_val(
	void *dm_void,
	u32 *val_buf,
	u8 val_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (val_len != 1) {
		PHYDM_DBG(dm, ODM_COMP_API, "[Error][ADSL]Need val_len=1\n");
		return;
	}

	phydm_soml_on_off(dm, (u8)val_buf[1]);
}

#endif /* end of CONFIG_ADAPTIVE_SOML*/

void phydm_soml_bytes_acq(void *dm_void, u8 rate_id, u32 length)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	if (rate_id >= ODM_RATEMCS0 && rate_id <= ODM_RATEMCS31)
		dm_soml_table->num_ht_bytes[rate_id - ODM_RATEMCS0] += length;
	else if ((rate_id >= ODM_RATEVHTSS1MCS0) && (rate_id <= ODM_RATEVHTSS4MCS9))
		dm_soml_table->num_vht_bytes[rate_id - ODM_RATEVHTSS1MCS0] += length;

#endif
}

void phydm_adaptive_soml_timers(void *dm_void, u8 state)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	if (state == INIT_SOML_TIMMER) {
		odm_initialize_timer(dm, &dm_soml_table->phydm_adaptive_soml_timer,
				     (void *)phydm_adaptive_soml_callback, NULL, "phydm_adaptive_soml_timer");
	} else if (state == CANCEL_SOML_TIMMER) {
		odm_cancel_timer(dm, &dm_soml_table->phydm_adaptive_soml_timer);
	} else if (state == RELEASE_SOML_TIMMER) {
		odm_release_timer(dm, &dm_soml_table->phydm_adaptive_soml_timer);
	}
#endif
}

void phydm_adaptive_soml_init(void *dm_void)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;
#if 0
	if (!(dm->support_ability & ODM_BB_ADAPTIVE_SOML)) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML,
			  "[Return]   Not Support Adaptive SOML\n");
		return;
	}
#endif
	PHYDM_DBG(dm, DBG_ADPTV_SOML, "%s\n", __func__);

	dm_soml_table->soml_state_cnt = 0;
	dm_soml_table->soml_delay_time = 40;
	dm_soml_table->soml_intvl = 150;
	dm_soml_table->soml_train_num = 4;
	dm_soml_table->is_soml_method_enable = 0;
	dm_soml_table->soml_counter = 0;
	dm_soml_table->soml_period = 1;
	dm_soml_table->soml_select = 0;
	dm_soml_table->cfo_counter = 0;
	dm_soml_table->cfo_diff_sum_a = 0;
	dm_soml_table->cfo_diff_sum_b = 0;

	dm_soml_table->cfo_qpsk_th = 94;
	dm_soml_table->cfo_qam16_th = 38;
	dm_soml_table->cfo_qam64_th = 17;
	dm_soml_table->cfo_qam256_th = 7;

	dm_soml_table->bpsk_qpsk_dist_th = 20;
	dm_soml_table->qam16_dist_th = 20;
	dm_soml_table->qam64_dist_th = 20;
	dm_soml_table->qam256_dist_th = 20;

	if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
		odm_set_bb_reg(dm, 0x988, BIT(25), 1);
#endif
}

void phydm_adaptive_soml(void *dm_void)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	if (!(dm->support_ability & ODM_BB_ADAPTIVE_SOML)) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML,
			  "[Return!!!] Not Support Adaptive SOML Function\n");
		return;
	}

	if (dm->pause_ability & ODM_BB_ADAPTIVE_SOML) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML, "Return: Pause ADSL in LV=%d\n",
			  dm->pause_lv_table.lv_adsl);
		return;
	}

	if (dm_soml_table->soml_counter < dm_soml_table->soml_period) {
		dm_soml_table->soml_counter++;
		return;
	}
	dm_soml_table->soml_counter = 0;
	dm_soml_table->soml_state_cnt = 0;
	dm_soml_table->cfo_counter = 0;
	dm_soml_table->cfo_diff_sum_a = 0;
	dm_soml_table->cfo_diff_sum_b = 0;

	phydm_soml_reset_qam(dm);

	if (dm_soml_table->soml_select == 0) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML,
			  "[ Adaptive SOML Training !!!]\n");
	} else if (dm_soml_table->soml_select == 1) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Stop Adaptive SOML !!!]\n");
		phydm_soml_on_off(dm, SOML_ON);
		return;
	} else if (dm_soml_table->soml_select == 2) {
		PHYDM_DBG(dm, DBG_ADPTV_SOML, "[ Stop Adaptive SOML !!!]\n");
		phydm_soml_on_off(dm, SOML_OFF);
		return;
	}

	if (dm->support_ic_type & ODM_ADAPTIVE_SOML_SUPPORT_IC)
		phydm_adsl(dm);

#endif
}

void phydm_enable_adaptive_soml(void *dm_void)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	PHYDM_DBG(dm, DBG_ADPTV_SOML, "[%s][Return!!!]  enable Adaptive SOML\n",
		  __func__);
	dm->support_ability |= ODM_BB_ADAPTIVE_SOML;
	phydm_soml_on_off(dm, SOML_ON);
#endif
}

void phydm_stop_adaptive_soml(void *dm_void)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	PHYDM_DBG(dm, DBG_ADPTV_SOML, "[%s][Return!!!]  Stop Adaptive SOML\n",
		  __func__);
	dm->support_ability &= ~ODM_BB_ADAPTIVE_SOML;
	phydm_soml_on_off(dm, SOML_ON);

#endif
}

void phydm_adaptive_soml_para_set(void *dm_void, u8 train_num, u8 intvl,
				  u8 period, u8 delay_time)
{
#ifdef CONFIG_ADAPTIVE_SOML
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct adaptive_soml *dm_soml_table = &dm->dm_soml_table;

	dm_soml_table->soml_train_num = train_num;
	dm_soml_table->soml_intvl = intvl;
	dm_soml_table->soml_period = period;
	dm_soml_table->soml_delay_time = delay_time;
#endif
}

void phydm_init_soft_ml_setting(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

#if (RTL8822B_SUPPORT == 1)
	if (!*dm->mp_mode) {
		if (dm->support_ic_type & ODM_RTL8822B) {
			/*odm_set_bb_reg(dm, R_0x19a8, MASKDWORD, 0xd10a0000);*/
			phydm_somlrxhp_setting(dm, true);
			dm->bsomlenabled = true;
		}
	}
#endif
#if (RTL8821C_SUPPORT == 1)
	if (!*dm->mp_mode) {
		if (dm->support_ic_type & ODM_RTL8821C)
			odm_set_bb_reg(dm, R_0x19a8, BIT(31) | BIT(30) | BIT(29) | BIT(28), 0xd);
	}
#endif
}
