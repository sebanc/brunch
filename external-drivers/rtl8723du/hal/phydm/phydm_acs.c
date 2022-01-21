// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */
#include "mp_precomp.h"
#include "phydm_precomp.h"


u8
odm_get_auto_channel_select_result(
	void			*p_dm_void,
	u8			band
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _ACS_					*p_acs = &p_dm->dm_acs;

	PHYDM_DBG(p_dm, ODM_COMP_API, ("%s ======>\n", __func__));

	if (band == ODM_BAND_2_4G) {
		PHYDM_DBG(p_dm, ODM_COMP_API, ("clean_CH_2g=%d\n", p_acs->clean_channel_2g));
		return (u8)p_acs->clean_channel_2g;
	} else {
		PHYDM_DBG(p_dm, ODM_COMP_API, ("clean_CH_5g=%d\n", p_acs->clean_channel_5g));
		return (u8)p_acs->clean_channel_5g;
	}
}

void
odm_auto_channel_select_init(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _ACS_						*p_acs = &p_dm->dm_acs;
	u8						i;

	if (!(p_dm->support_ability & ODM_BB_ENV_MONITOR))
		return;

	if (p_acs->is_force_acs_result)
		return;

	PHYDM_DBG(p_dm, ODM_COMP_API, ("%s ======>\n", __func__));

	p_acs->clean_channel_2g = 1;
	p_acs->clean_channel_5g = 36;

	for (i = 0; i < ODM_MAX_CHANNEL_2G; ++i) {
		p_acs->channel_info_2g[0][i] = 0;
		p_acs->channel_info_2g[1][i] = 0;
	}

	if (p_dm->support_ic_type & ODM_IC_11AC_SERIES) {
		for (i = 0; i < ODM_MAX_CHANNEL_5G; ++i) {
			p_acs->channel_info_5g[0][i] = 0;
			p_acs->channel_info_5g[1][i] = 0;
		}
	}
}

void
odm_auto_channel_select_reset(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _ACS_						*p_acs = &p_dm->dm_acs;
	struct _CCX_INFO		*ccx_info = &p_dm->dm_ccx_info;

	if (!(p_dm->support_ability & ODM_BB_ENV_MONITOR))
		return;

	if (p_acs->is_force_acs_result)
		return;

	PHYDM_DBG(p_dm, ODM_COMP_API, ("%s ======>\n", __func__));

	ccx_info->nhm_period = 0x1388;	/*20ms*/
	phydm_nhm_setting(p_dm, SET_NHM_SETTING);
	phydm_nhm_trigger(p_dm);
}

void
odm_auto_channel_select(
	void			*p_dm_void,
	u8			channel
)
{
	struct PHY_DM_STRUCT					*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _ACS_						*p_acs = &p_dm->dm_acs;
	struct _CCX_INFO		*ccx_info = &p_dm->dm_ccx_info;
	u8						channel_idx = 0, search_idx = 0;
	u8						noisy_nhm_th = 0x52;
	u8						i, noisy_nhm_th_index, low_pwr_cnt = 0;
	u16						max_score = 0;

	PHYDM_DBG(p_dm, ODM_COMP_API, ("%s ======>\n", __func__));

	if (!(p_dm->support_ability & ODM_BB_ENV_MONITOR)) {
		PHYDM_DBG(p_dm, DBG_DIG, ("Return: Not support\n"));
		return;
	}

	if (p_acs->is_force_acs_result) {
		PHYDM_DBG(p_dm, DBG_DIG, ("Force clean CH{2G,5G}={%d,%d}\n",
			p_acs->clean_channel_2g, p_acs->clean_channel_5g));
		return;
	}

	PHYDM_DBG(p_dm, ODM_COMP_API, ("CH=%d\n", channel));

	phydm_get_nhm_result(p_dm);
	noisy_nhm_th_index = (noisy_nhm_th - ccx_info->nhm_th[0]) << 2;

	for (i = 0; i <= 11; i++) {
		if (i <= noisy_nhm_th_index)
			low_pwr_cnt += ccx_info->nhm_result[i];
	}

	ccx_info->nhm_period = 0x2710;
	phydm_nhm_setting(p_dm, SET_NHM_SETTING);

	if (channel >= 1 && channel <= 14) {
		channel_idx = channel - 1;
		p_acs->channel_info_2g[1][channel_idx]++;

		if (p_acs->channel_info_2g[1][channel_idx] >= 2)
			p_acs->channel_info_2g[0][channel_idx] = (p_acs->channel_info_2g[0][channel_idx] >> 1) +
				(p_acs->channel_info_2g[0][channel_idx] >> 2) + (low_pwr_cnt >> 2);
		else
			p_acs->channel_info_2g[0][channel_idx] = low_pwr_cnt;

		PHYDM_DBG(p_dm, ODM_COMP_API, ("low_pwr_cnt = %d\n", low_pwr_cnt));
		PHYDM_DBG(p_dm, ODM_COMP_API, ("CH_Info[0][%d]=%d, CH_Info[1][%d]=%d\n", channel_idx, p_acs->channel_info_2g[0][channel_idx], channel_idx, p_acs->channel_info_2g[1][channel_idx]));

		for (search_idx = 0; search_idx < ODM_MAX_CHANNEL_2G; search_idx++) {
			if (p_acs->channel_info_2g[1][search_idx] != 0 && p_acs->channel_info_2g[0][search_idx] >= max_score) {
				max_score = p_acs->channel_info_2g[0][search_idx];
				p_acs->clean_channel_2g = search_idx + 1;
			}
		}
		PHYDM_DBG(p_dm, ODM_COMP_API, ("clean_CH_2g=%d, max_score=%d\n",
				p_acs->clean_channel_2g, max_score));

	} else if (channel >= 36) {
		/* Need to do */
		p_acs->clean_channel_5g = channel;
	}
}

bool
phydm_acs_check(
	void	*p_dm_void
)
{
	return false;
}
