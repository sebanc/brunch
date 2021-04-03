/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDM_AUTO_DBG_H__
#define    __PHYDM_AUTO_DBG_H__

#define AUTO_DBG_VERSION	"1.0"		/* 2017.05.015  Dino, Add phydm_auto_dbg.h*/


/* 1 ============================================================
 * 1  Definition
 * 1 ============================================================ */

#define	AUTO_CHK_HANG_STEP_MAX	3
#define	DBGPORT_CHK_NUM			6

void
phydm_auto_dbg_engine(
	void			*p_dm_void
);

void
phydm_auto_dbg_engine_init(
	void		*p_dm_void
);
#endif
