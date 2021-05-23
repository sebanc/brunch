/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __INC_PHYDM_BEAMFORMING_H
#define __INC_PHYDM_BEAMFORMING_H

/*Beamforming Related*/
#include "halcomtxbf.h"
#include "haltxbfjaguar.h"
#include "haltxbfinterface.h"

#define beamforming_gid_paid(adapter, p_tcb)
#define	phydm_acting_determine(p_dm, type)	false
#define beamforming_enter(p_dm, sta_idx)
#define beamforming_leave(p_dm, ra)
#define beamforming_end_fw(p_dm)
#define beamforming_control_v1(p_dm, ra, aid, mode, bw, rate)		true
#define beamforming_control_v2(p_dm, idx, mode, bw, period)		true
#define phydm_beamforming_end_sw(p_dm, _status)
#define beamforming_timer_callback(p_dm)
#define phydm_beamforming_init(p_dm)
#define phydm_beamforming_control_v2(p_dm, _idx, _mode, _bw, _period)	false
#define beamforming_watchdog(p_dm)
#define phydm_beamforming_watchdog(p_dm)

#endif
