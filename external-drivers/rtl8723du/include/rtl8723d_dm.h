/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTL8723D_DM_H__
#define __RTL8723D_DM_H__
/* ************************************************************
 * Description:
 *
 * This file is for 8723D dynamic mechanism only
 *
 *
 * ************************************************************ */

/* ************************************************************
 * structure and define
 * ************************************************************ */

/* ************************************************************
 * function prototype
 * ************************************************************ */

void rtl8723d_init_dm_priv(struct adapter * adapt);
void rtl8723d_deinit_dm_priv(struct adapter * adapt);

void rtl8723d_InitHalDm(struct adapter * adapt);
void rtl8723d_HalDmWatchDog(struct adapter * adapt);

#endif
