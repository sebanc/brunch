// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */
#include "mp_precomp.h"
#include "phydm_precomp.h"


bool
odm_check_power_status(
	void		*p_dm_void
)
{
	return	true;
	
}

void
halrf_update_pwr_track(
	void		*p_dm_void,
	u8		rate
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	ODM_RT_TRACE(p_dm, ODM_COMP_TX_PWR_TRACK, ODM_DBG_LOUD, ("Pwr Track Get rate=0x%x\n", rate));

	p_dm->tx_rate = rate;
}
