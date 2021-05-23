/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#ifndef __RTW_BTCOEX_WIFIONLY_H__
#define __RTW_BTCOEX_WIFIONLY_H__

void rtw_btcoex_wifionly_switchband_notify(struct adapter * adapt);
void rtw_btcoex_wifionly_scan_notify(struct adapter * adapt);
void rtw_btcoex_wifionly_hw_config(struct adapter * adapt);
void rtw_btcoex_wifionly_initialize(struct adapter * adapt);
void rtw_btcoex_wifionly_AntInfoSetting(struct adapter * adapt);
#endif
