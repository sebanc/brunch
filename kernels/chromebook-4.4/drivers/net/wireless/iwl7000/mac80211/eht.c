// SPDX-License-Identifier: GPL-2.0-only
/*
 * EHT handling
 *
 * Copyright(c) 2021 Intel Corporation
 */

#include "ieee80211_i.h"

void
ieee80211_eht_cap_ie_to_sta_eht_cap(struct ieee80211_sub_if_data *sdata,
				    struct ieee80211_supported_band *sband,
				    const u8 *he_cap_ie, u8 he_cap_len,
				    const struct ieee80211_eht_cap_elem *eht_cap_ie_elem,
				    u8 eht_cap_len, struct sta_info *sta)
{
	struct ieee80211_sta_eht_cap *eht_cap = &sta->sta.eht_cap;
	struct ieee80211_he_cap_elem *he_cap_ie_elem = (void *)he_cap_ie;
	u8 eht_ppe_size = 0;
	u8 mcs_nss_size;
	u8 eht_total_size = sizeof(eht_cap->eht_cap_elem);
	u8 *pos = (u8 *)eht_cap_ie_elem;

	memset(eht_cap, 0, sizeof(*eht_cap));

	if (!eht_cap_ie_elem ||
	    !ieee80211_get_eht_iftype_cap(sband,
					 ieee80211_vif_type_p2p(&sdata->vif)))
		return;

	mcs_nss_size = ieee80211_eht_mcs_nss_size(he_cap_ie_elem,
						  eht_cap_ie_elem);

	eht_total_size += mcs_nss_size;

	/* Calculate the PPE thresholds length only if the header is present */
	if (eht_total_size + sizeof(u16) < eht_cap_len)
		eht_ppe_size =
			ieee80211_eht_ppe_size(eht_cap_ie_elem->optional[mcs_nss_size],
					       eht_cap_ie_elem->phy_cap_info);
	else if ((eht_cap_ie_elem->phy_cap_info[5] &
		  IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT))
		return;

	eht_total_size += eht_ppe_size;
	if (eht_cap_len < eht_total_size)
		return;

	/* Copy the static portion of the EHT capabilities */
	memcpy(&eht_cap->eht_cap_elem, pos, sizeof(eht_cap->eht_cap_elem));
	pos += sizeof(eht_cap->eht_cap_elem);

	/* Copy MCS/NSS which depends on the peer capabilities */
	if (!(he_cap_ie_elem->phy_cap_info[0] &
	      IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_MASK_ALL)) {
		memcpy(&eht_cap->eht_mcs_nss_supp.only_20mhz, pos,
		       sizeof(eht_cap->eht_mcs_nss_supp.only_20mhz));
		pos += 4;
	} else {
		if (he_cap_ie_elem->phy_cap_info[0] &
		    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G) {
			memcpy(&eht_cap->eht_mcs_nss_supp.bw_80, pos,
			       sizeof(eht_cap->eht_mcs_nss_supp.bw_80));
			pos += 3;
		}

		if (he_cap_ie_elem->phy_cap_info[0] &
		    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G) {
			memcpy(&eht_cap->eht_mcs_nss_supp.bw_160, pos,
			       sizeof(eht_cap->eht_mcs_nss_supp.bw_160));
			pos += 3;
		}

		if (eht_cap_ie_elem->phy_cap_info[0] &
		    IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ) {
			memcpy(&eht_cap->eht_mcs_nss_supp.bw_320, pos,
			       sizeof(eht_cap->eht_mcs_nss_supp.bw_320));
			pos += 3;
		}
	}

	if (eht_cap->eht_cap_elem.phy_cap_info[5] &
	    IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT)
		memcpy(eht_cap->eht_ppe_thres,
		       &eht_cap_ie_elem->optional[mcs_nss_size],
		       eht_ppe_size);

	eht_cap->has_eht = true;

	sta->cur_max_bandwidth = ieee80211_sta_cap_rx_bw(sta);
	sta->sta.bandwidth = ieee80211_sta_cur_vht_bw(sta);
}
