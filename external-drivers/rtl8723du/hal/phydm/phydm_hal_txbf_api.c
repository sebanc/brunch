// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#include "mp_precomp.h"
#include "phydm_precomp.h"

u8 beamforming_get_htndp_tx_rate(void *p_dm_void, u8 comp_steering_num_of_bfer)
{
	u8 nr_index = 0;
	u8 ndp_tx_rate;
	/*Find nr*/
	nr_index = tx_bf_nr(1, comp_steering_num_of_bfer);

	switch (nr_index) {
	case 1:
		ndp_tx_rate = ODM_MGN_MCS8;
		break;
	case 2:
		ndp_tx_rate = ODM_MGN_MCS16;
		break;
	case 3:
		ndp_tx_rate = ODM_MGN_MCS24;
		break;
	default:
		ndp_tx_rate = ODM_MGN_MCS8;
		break;
	}
	return ndp_tx_rate;
}

u8 beamforming_get_vht_ndp_tx_rate(void *p_dm_void,
				   u8 comp_steering_num_of_bfer)
{
	u8 nr_index = 0;
	u8 ndp_tx_rate;
	/*Find nr*/
	nr_index = tx_bf_nr(1, comp_steering_num_of_bfer);

	switch (nr_index) {
	case 1:
		ndp_tx_rate = ODM_MGN_VHT2SS_MCS0;
		break;

	case 2:
		ndp_tx_rate = ODM_MGN_VHT3SS_MCS0;
		break;

	case 3:
		ndp_tx_rate = ODM_MGN_VHT4SS_MCS0;
		break;

	default:
		ndp_tx_rate = ODM_MGN_VHT2SS_MCS0;
		break;
	}

	return ndp_tx_rate;

}
