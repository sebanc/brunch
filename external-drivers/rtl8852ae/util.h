/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
 * Copyright(c) 2019-2020  Realtek Corporation
 */
#ifndef __RTW89_UTIL_H__
#define __RTW89_UTIL_H__

#include "core.h"

#define rtw89_iterate_vifs_bh(rtwdev, iterator, data)                          \
	ieee80211_iterate_active_interfaces_atomic((rtwdev)->hw,               \
			IEEE80211_IFACE_ITER_NORMAL, iterator, data)
void __rtw89_iterate_vifs(struct rtw89_dev *rtwdev,
			  void (*iterator)(void *data, u8 *mac,
					   struct ieee80211_vif *vif),
			  void *data);
static inline
void rtw89_iterate_vifs(struct rtw89_dev *rtwdev,
			void (*iterator)(void *data, u8 *mac,
					 struct ieee80211_vif *vif),
			void *data, bool held_vifmtx)
{
	if (!held_vifmtx) {
		ieee80211_iterate_active_interfaces((rtwdev)->hw,
				IEEE80211_IFACE_ITER_NORMAL, iterator, data);
		return;
	}

	__rtw89_iterate_vifs(rtwdev, iterator, data);
}

#endif
