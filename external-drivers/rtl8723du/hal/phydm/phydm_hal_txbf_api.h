/******************************************************************************
 *
 * Copyright(c) 2011 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifndef	__PHYDM_HAL_TXBF_API_H__
#define __PHYDM_HAL_TXBF_API_H__

#define tx_bf_nr(a, b) ((a > b) ? (b) : (a))

u8 beamforming_get_htndp_tx_rate(void *p_dm_void, u8 comp_steering_num_of_bfer);

u8 beamforming_get_vht_ndp_tx_rate(void *p_dm_void,
				   u8 comp_steering_num_of_bfer);

#define phydm_get_beamforming_sounding_info(p_dm_void, throughput,	\
					    total_bfee_num, tx_rate)
#define phydm_get_ndpa_rate(p_dm_void)
#define phydm_get_mu_bfee_snding_decision(p_dm_void, throughput)

#endif
