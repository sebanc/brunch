// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2018-2019  Realtek Corporation
 */

#include <uapi/nl80211-vnd-realtek.h>

#include "main.h"
#include "phy.h"
#include "debug.h"

static const struct nla_policy
rtw_sar_rule_policy[REALTEK_VNDCMD_SAR_RULE_ATTR_MAX + 1] = {
	[REALTEK_VNDCMD_ATTR_SAR_RULES] = { .type = NLA_NESTED_ARRAY },
	[REALTEK_VNDCMD_ATTR_SAR_BAND]	= { .type = NLA_U32 },
	[REALTEK_VNDCMD_ATTR_SAR_POWER]	= { .type = NLA_U8 },
};

static const struct sar_band2ch {
	u8 ch_start;
	u8 ch_end;
} sar_band2chs[REALTEK_VNDCMD_SAR_BAND_NR] = {
	[REALTEK_VNDCMD_SAR_BAND_2G]	   = { .ch_start = 1,   .ch_end = 14 },
	[REALTEK_VNDCMD_SAR_BAND_5G_BAND1] = { .ch_start = 36,  .ch_end = 64 },
	/* REALTEK_VNDCMD_SAR_BAND_5G_BAND2 isn't used by now. */
	[REALTEK_VNDCMD_SAR_BAND_5G_BAND3] = { .ch_start = 100, .ch_end = 144 },
	[REALTEK_VNDCMD_SAR_BAND_5G_BAND4] = { .ch_start = 149, .ch_end = 165 },
};

static int rtw_apply_vndcmd_sar(struct rtw_dev *rtwdev, u32 band, u8 power)
{
	const struct sar_band2ch *sar_band2ch;
	u8 path, rd;

	if (band >= REALTEK_VNDCMD_SAR_BAND_NR)
		return -EINVAL;

	sar_band2ch = &sar_band2chs[band];
	if (!sar_band2ch->ch_start || !sar_band2ch->ch_end)
		return 0;

	/* SAR values from vendor command apply to all regulatory domains,
	 * and we can still ensure TX power under power limit because of
	 * "tx_power = base + min(by_rate, limit, sar)".
	 */
	for (path = 0; path < rtwdev->hal.rf_path_num; path++)
		for (rd = 0; rd < RTW_REGD_MAX; rd++)
			rtw_phy_set_tx_power_sar(rtwdev, rd, path,
						 sar_band2ch->ch_start,
						 sar_band2ch->ch_end, power);

	rtw_info(rtwdev, "set SAR power limit %u.%03u on band %u\n",
		 power >> 3, (power & 7) * 125, band);

	rtwdev->sar.source = RTW_SAR_SOURCE_VNDCMD;

	return 0;
}

static int rtw_vndcmd_set_sar(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct rtw_dev *rtwdev = hw->priv;
	struct rtw_hal *hal = &rtwdev->hal;
	struct nlattr *tb_root[REALTEK_VNDCMD_SAR_RULE_ATTR_MAX + 1];
	struct nlattr *tb[REALTEK_VNDCMD_SAR_RULE_ATTR_MAX + 1];
	struct nlattr *nl_sar_rule;
	int rem_sar_rules, r;
	u32 band;
	u8 power;

	if (rtwdev->sar.source != RTW_SAR_SOURCE_NONE &&
	    rtwdev->sar.source != RTW_SAR_SOURCE_VNDCMD) {
		rtw_info(rtwdev, "SAR source 0x%x is in use", rtwdev->sar.source);
		return -EBUSY;
	}

	r = nla_parse(tb_root, REALTEK_VNDCMD_SAR_RULE_ATTR_MAX, data, data_len,
		      rtw_sar_rule_policy, NULL);
	if (r) {
		rtw_warn(rtwdev, "invalid SAR attr\n");
		return r;
	}

	if (!tb_root[REALTEK_VNDCMD_ATTR_SAR_RULES]) {
		rtw_warn(rtwdev, "no SAR rule attr\n");
		return -EINVAL;
	}

	nla_for_each_nested(nl_sar_rule, tb_root[REALTEK_VNDCMD_ATTR_SAR_RULES],
			    rem_sar_rules) {
		r = nla_parse_nested(tb, REALTEK_VNDCMD_SAR_RULE_ATTR_MAX,
				     nl_sar_rule, rtw_sar_rule_policy, NULL);
		if (r)
			return r;
		if (!tb[REALTEK_VNDCMD_ATTR_SAR_BAND])
			return -EINVAL;
		if (!tb[REALTEK_VNDCMD_ATTR_SAR_POWER])
			return -EINVAL;

		band = nla_get_u32(tb[REALTEK_VNDCMD_ATTR_SAR_BAND]);
		power = nla_get_u8(tb[REALTEK_VNDCMD_ATTR_SAR_POWER]);

		r = rtw_apply_vndcmd_sar(rtwdev, band, power);
		if (r)
			return r;
	}

	rtw_phy_set_tx_power_level(rtwdev, hal->current_channel);

	return 0;
}

static const struct wiphy_vendor_command rtw88_vendor_commands[] = {
	{
		.info = {
			.vendor_id = REALTEK_NL80211_VENDOR_ID,
			.subcmd = REALTEK_NL80211_VNDCMD_SET_SAR,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = rtw_vndcmd_set_sar,
		.policy = rtw_sar_rule_policy,
		.maxattr = REALTEK_VNDCMD_SAR_RULE_ATTR_MAX,
	}
};

void rtw_register_vndcmd(struct ieee80211_hw *hw)
{
	hw->wiphy->vendor_commands = rtw88_vendor_commands;
	hw->wiphy->n_vendor_commands = ARRAY_SIZE(rtw88_vendor_commands);
}
