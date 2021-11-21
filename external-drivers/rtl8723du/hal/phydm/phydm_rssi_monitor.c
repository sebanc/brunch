// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"
 
static void
phydm_rssi_monitor_h2c(
	void	*p_dm_void,
	u8	macid
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_t = &p_dm->dm_ra_table;
	struct cmn_sta_info			*p_sta = p_dm->p_phydm_sta_info[macid];
	struct ra_sta_info				*p_ra = NULL;
	u8		h2c_val[H2C_MAX_LENGTH] = {0};
	u8		stbc_en, ldpc_en;
	u8		bf_en = 0;
	u8		is_rx, is_tx;

	if (is_sta_active(p_sta)) {
		p_ra = &(p_sta->ra_info);
	} else {
		PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("[Warning] %s invalid sta_info\n", __func__));
		return;
	}
	
	PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("%s ======>\n", __func__));
	PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("MACID=%d\n", p_sta->mac_id));

	is_rx = (p_ra->txrx_state == RX_STATE) ? 1 : 0;
	is_tx = (p_ra->txrx_state == TX_STATE) ? 1 : 0;
	stbc_en = (p_sta->stbc_en) ? 1 : 0;
	ldpc_en = (p_sta->ldpc_en) ? 1 : 0;

	if (p_ra_t->RA_threshold_offset != 0) {
		PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("RA_th_ofst = (( %s%d ))\n",
			((p_ra_t->RA_offset_direction) ? "+" : "-"), p_ra_t->RA_threshold_offset));
	}

	h2c_val[0] = p_sta->mac_id;
	h2c_val[1] = 0;
	h2c_val[2] = p_sta->rssi_stat.rssi;
	h2c_val[3] = is_rx | (stbc_en << 1) | ((p_dm->noisy_decision & 0x1) << 2) |  (bf_en << 6);
	h2c_val[4] = (p_ra_t->RA_threshold_offset & 0x7f) | ((p_ra_t->RA_offset_direction & 0x1) << 7);
	h2c_val[5] = 0;
	h2c_val[6] = 0;

	PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("PHYDM h2c[0x42]=0x%x %x %x %x %x %x %x\n",
		h2c_val[6], h2c_val[5], h2c_val[4], h2c_val[3], h2c_val[2], h2c_val[1], h2c_val[0]));

	odm_fill_h2c_cmd(p_dm, ODM_H2C_RSSI_REPORT, H2C_MAX_LENGTH, h2c_val);
}

static void
phydm_calculate_rssi_min_max(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct cmn_sta_info		*p_sta;
	s8	rssi_max_tmp = 0, rssi_min_tmp = 100;
	u8	i;
	u8	sta_cnt = 0;

	if (!p_dm->is_linked)
		return;

	PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("%s ======>\n", __func__));

	for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
		p_sta = p_dm->p_phydm_sta_info[i];
		if (is_sta_active(p_sta)) {

			sta_cnt++;

			if (p_sta->rssi_stat.rssi < rssi_min_tmp)
				rssi_min_tmp = p_sta->rssi_stat.rssi;

			if (p_sta->rssi_stat.rssi > rssi_max_tmp)
				rssi_max_tmp = p_sta->rssi_stat.rssi;

			/*[Send RSSI to FW]*/
			if (!p_sta->ra_info.disable_ra)
				phydm_rssi_monitor_h2c(p_dm, i);

			if (sta_cnt == p_dm->number_linked_client)
				break;
		}
	}

	p_dm->rssi_max = (u8)rssi_max_tmp;
	p_dm->rssi_min = (u8)rssi_min_tmp;

}

void
phydm_rssi_monitor_check(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (!(p_dm->support_ability & ODM_BB_RSSI_MONITOR))
		return;

	if ((p_dm->phydm_sys_up_time % 2) == 1) /*for AP watchdog period = 1 sec*/
		return;

	PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("%s ======>\n", __func__));

	phydm_calculate_rssi_min_max(p_dm);

	PHYDM_DBG(p_dm, DBG_RSSI_MNTR, ("RSSI {max, min} = {%d, %d}\n",
		p_dm->rssi_max, p_dm->rssi_min));

}

void
phydm_rssi_monitor_init(
	void		*p_dm_void
)
{

	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _rate_adaptive_table_	*p_ra_table = &p_dm->dm_ra_table;

	p_ra_table->firstconnect = false;
	p_dm->rssi_max = 0;
	p_dm->rssi_min = 0;

}
