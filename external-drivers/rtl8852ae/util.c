// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2019-2020  Realtek Corporation
 */

#include "util.h"

static void
__rtw89_vifs_collect_iter(void *data, u8 *mac, struct ieee80211_vif *vif)
{
	struct list_head *vif_list = (struct list_head *)data;
	struct rtw89_vif *rtwvif = (struct rtw89_vif *)vif->drv_priv;

	list_add_tail(&rtwvif->list, vif_list);
}

void __rtw89_iterate_vifs(struct rtw89_dev *rtwdev,
			  void (*iterator)(void *data, u8 *mac,
					   struct ieee80211_vif *vif),
			  void *data)
{
	struct ieee80211_vif *vif;
	struct rtw89_vif *rtwvif;
	LIST_HEAD(vif_list);

	/* iflist_mtx & mutex are held */
	lockdep_assert_held(&rtwdev->mutex);

	/* Since iflist_mtx is held, we can use vif outside of iterator */
	ieee80211_iterate_active_interfaces_atomic(rtwdev->hw,
			IEEE80211_IFACE_ITER_NORMAL, __rtw89_vifs_collect_iter,
			&vif_list);

	list_for_each_entry(rtwvif, &vif_list, list) {
		vif = rtwvif_to_vif(rtwvif);
		iterator(data, vif->addr, vif);
	}
}
