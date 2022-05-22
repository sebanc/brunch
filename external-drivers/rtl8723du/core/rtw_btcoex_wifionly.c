// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#include <drv_types.h>
#include <hal_btcoex_wifionly.h>
#include <hal_data.h>

void rtw_btcoex_wifionly_switchband_notify(struct adapter * adapt)
{
	hal_btcoex_wifionly_switchband_notify(adapt);
}

void rtw_btcoex_wifionly_scan_notify(struct adapter * adapt)
{
	hal_btcoex_wifionly_scan_notify(adapt);
}

void rtw_btcoex_wifionly_hw_config(struct adapter * adapt)
{
	hal_btcoex_wifionly_hw_config(adapt);
}

void rtw_btcoex_wifionly_initialize(struct adapter * adapt)
{
	hal_btcoex_wifionly_initlizevariables(adapt);
}

void rtw_btcoex_wifionly_AntInfoSetting(struct adapter * adapt)
{
	hal_btcoex_wifionly_AntInfoSetting(adapt);
}
