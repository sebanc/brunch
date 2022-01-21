/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright(c) 2018-2019  Realtek Corporation
 */
#ifndef _UAPI_NL80211_VND_REALTEK_H
#define _UAPI_NL80211_VND_REALTEK_H

/**
 * This vendor ID is the value of atrribute %NL80211_ATTR_VENDOR_ID used by
 * %NL80211_CMD_VENDOR to send vendor command.
 */
#define REALTEK_NL80211_VENDOR_ID	0x00E04C

/**
 * enum realtek_nl80211_vndcmd - supported vendor subcmds
 *
 * @REALTEK_NL80211_VNDCMD_SET_SAR: set SAR power limit
 *	%realtek_vndcmd_sar_band within attribute %REALTEK_VNDCMD_ATTR_SAR_BAND
 *	and corresponding power limit attribute %REALTEK_VNDCMD_ATTR_SAR_POWER.
 *	The two attributes are in nested attribute %REALTEK_VNDCMD_ATTR_SAR_RULES.
 */
enum realtek_nl80211_vndcmd {
	REALTEK_NL80211_VNDCMD_SET_SAR = 0x88,
};

/**
 * enum realtek_vndcmd_sar_band - bands of SAR power limit
 *
 * @REALTEK_VNDCMD_SAR_BAND_2G: all channels of 2G band
 * @REALTEK_VNDCMD_SAR_BAND_5G_BAND1: channels of 5G band1 (5.15~5.35G)
 * @REALTEK_VNDCMD_SAR_BAND_5G_BAND2: channels of 5G band2 (5.35~5.47G)
 *	5G band2 isn't used by rtw88 by now, so don't need to set SAR power
 *	limit for this band. But we still enumerate this band as a placeholder
 *	for the furture.
 * @REALTEK_VNDCMD_SAR_BAND_5G_BAND3: channels of 5G band3 (5.47~5.725G)
 * @REALTEK_VNDCMD_SAR_BAND_5G_BAND4: channels of 5G band4 (5.725~5.95G)
 */
enum realtek_vndcmd_sar_band {
	REALTEK_VNDCMD_SAR_BAND_2G,
	REALTEK_VNDCMD_SAR_BAND_5G_BAND1,
	REALTEK_VNDCMD_SAR_BAND_5G_BAND2,
	REALTEK_VNDCMD_SAR_BAND_5G_BAND3,
	REALTEK_VNDCMD_SAR_BAND_5G_BAND4,

	REALTEK_VNDCMD_SAR_BAND_NR,
};

/**
 * enum realtek_vndcmd_sar_rule_attr - attributes of vendor command
 *	%REALTEK_NL80211_VNDCMD_SET_SAR
 *
 * @REALTEK_VNDCMD_ATTR_SAR_RULES: nested attribute to hold SAR rules containing
 *	band and corresponding power limit.
 *
 * @REALTEK_VNDCMD_ATTR_SAR_BAND: an attribute within %REALTEK_VNDCMD_ATTR_SAR_RULES,
 *	and its value is %realtek_vndcmd_sar_band (u32 data type).
 * @REALTEK_VNDCMD_ATTR_SAR_POWER: an attribute within %REALTEK_VNDCMD_ATTR_SAR_RULES.
 *	SAR power limit is 'u8' type and in unit of 0.125 dBm, so its range is
 *	0 to 31.875 dBm.
 */
enum realtek_vndcmd_sar_rule_attr {
	__REALTEK_VNDCMD_SAR_RULE_ATTR_INVALID,

	REALTEK_VNDCMD_ATTR_SAR_RULES,
	REALTEK_VNDCMD_ATTR_SAR_BAND,
	REALTEK_VNDCMD_ATTR_SAR_POWER,

	/* keep last */
	__REALTEK_VNDCMD_SAR_RULE_ATTR_AFTER_LAST,
	REALTEK_VNDCMD_SAR_RULE_ATTR_MAX = __REALTEK_VNDCMD_SAR_RULE_ATTR_AFTER_LAST - 1,
};

#endif /* _UAPI_NL80211_VND_REALTEK_H */
