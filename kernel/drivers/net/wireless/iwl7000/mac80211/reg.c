/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2008-2011	Luis R. Rodriguez <mcgrof@qca.qualcomm.com>
 * Copyright 2013-2014  Intel Mobile Communications GmbH
 * Copyright 2017	Intel Deutschland GmbH
 * Copyright (C) 2018 Intel Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if CFG80211_VERSION < KERNEL_VERSION(4,0,0)
/**
 * DOC: Wireless regulatory infrastructure
 *
 * The usual implementation is for a driver to read a device EEPROM to
 * determine which regulatory domain it should be operating under, then
 * looking up the allowable channels in a driver-local table and finally
 * registering those channels in the wiphy structure.
 *
 * Another set of compliance enforcement is for drivers to use their
 * own compliance limits which can be stored on the EEPROM. The host
 * driver or firmware may ensure these are used.
 *
 * In addition to all this we provide an extra layer of regulatory
 * conformance. For drivers which do not have any regulatory
 * information CRDA provides the complete regulatory solution.
 * For others it provides a community effort on further restrictions
 * to enhance compliance.
 *
 * Note: When number of rules --> infinity we will not be able to
 * index on alpha2 any more, instead we'll probably have to
 * rely on some SHA1 checksum of the regdomain for example.
 *
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include "ieee80211_i.h"

static LIST_HEAD(cfg80211_rdev_list);

static struct cfg80211_registered_device *wiphy_to_rdev(struct wiphy *wiphy)
{
	struct cfg80211_registered_device *rdev;
	struct ieee80211_local *local;

	/* must have RTNL or reg_requests_lock held */

	list_for_each_entry(rdev, &cfg80211_rdev_list, list) {
		local = container_of(rdev, struct ieee80211_local, rdev);
		if (wiphy == local->hw.wiphy)
			return rdev;
	}

	return NULL;
}

#ifdef CONFIG_CFG80211_REG_DEBUG
#define REG_DBG_PRINT(format, args...)			\
	printk(KERN_DEBUG pr_fmt(format), ##args)
#else
#define REG_DBG_PRINT(args...)
#endif

static DEFINE_SPINLOCK(reg_requests_lock);

static void reg_todo(struct work_struct *work);
static DECLARE_WORK(reg_work, reg_todo);

static const struct ieee80211_regdomain *
reg_copy_regd(const struct ieee80211_regdomain *src_regd)
{
	struct ieee80211_regdomain *regd;
	int size_of_regd;
	unsigned int i;

	size_of_regd =
		sizeof(struct ieee80211_regdomain) +
		src_regd->n_reg_rules * sizeof(struct ieee80211_reg_rule);

	regd = kzalloc(size_of_regd, GFP_KERNEL);
	if (!regd)
		return ERR_PTR(-ENOMEM);

	memcpy(regd, src_regd, sizeof(struct ieee80211_regdomain));

	for (i = 0; i < src_regd->n_reg_rules; i++)
		memcpy(&regd->reg_rules[i], &src_regd->reg_rules[i],
		       sizeof(struct ieee80211_reg_rule));

	return regd;
}

static unsigned int
reg_get_max_bandwidth_from_range(const struct ieee80211_regdomain *rd,
				 const struct ieee80211_reg_rule *rule)
{
	const struct ieee80211_freq_range *freq_range = &rule->freq_range;
	const struct ieee80211_freq_range *freq_range_tmp;
	const struct ieee80211_reg_rule *tmp;
	u32 start_freq, end_freq, idx, no;

	for (idx = 0; idx < rd->n_reg_rules; idx++)
		if (rule == &rd->reg_rules[idx])
			break;

	if (idx == rd->n_reg_rules)
		return 0;

	/* get start_freq */
	no = idx;

	while (no) {
		tmp = &rd->reg_rules[--no];
		freq_range_tmp = &tmp->freq_range;

		if (freq_range_tmp->end_freq_khz < freq_range->start_freq_khz)
			break;

		freq_range = freq_range_tmp;
	}

	start_freq = freq_range->start_freq_khz;

	/* get end_freq */
	freq_range = &rule->freq_range;
	no = idx;

	while (no < rd->n_reg_rules - 1) {
		tmp = &rd->reg_rules[++no];
		freq_range_tmp = &tmp->freq_range;

		if (freq_range_tmp->start_freq_khz > freq_range->end_freq_khz)
			break;

		freq_range = freq_range_tmp;
	}

	end_freq = freq_range->end_freq_khz;

	return end_freq - start_freq;
}

unsigned int reg_get_max_bandwidth(const struct ieee80211_regdomain *rd,
				   const struct ieee80211_reg_rule *rule)
{
	unsigned int bw = reg_get_max_bandwidth_from_range(rd, rule);

	if (rule->flags & NL80211_RRF_NO_160MHZ)
		bw = min_t(unsigned int, bw, MHZ_TO_KHZ(80));
	if (rule->flags & NL80211_RRF_NO_80MHZ)
		bw = min_t(unsigned int, bw, MHZ_TO_KHZ(40));

	/*
	 * HT40+/HT40- limits are handled per-channel. Only limit BW if both
	 * are not allowed.
	 */
	if (rule->flags & NL80211_RRF_NO_HT40MINUS &&
	    rule->flags & NL80211_RRF_NO_HT40PLUS)
		bw = min_t(unsigned int, bw, MHZ_TO_KHZ(20));

	return bw;
}

/* Sanity check on a regulatory rule */
static bool is_valid_reg_rule(const struct ieee80211_reg_rule *rule)
{
	const struct ieee80211_freq_range *freq_range = &rule->freq_range;
	u32 freq_diff;

	if (freq_range->start_freq_khz <= 0 || freq_range->end_freq_khz <= 0)
		return false;

	if (freq_range->start_freq_khz > freq_range->end_freq_khz)
		return false;

	freq_diff = freq_range->end_freq_khz - freq_range->start_freq_khz;

	if (freq_range->end_freq_khz <= freq_range->start_freq_khz ||
	    freq_range->max_bandwidth_khz > freq_diff)
		return false;

	return true;
}

static bool is_valid_rd(const struct ieee80211_regdomain *rd)
{
	const struct ieee80211_reg_rule *reg_rule = NULL;
	unsigned int i;

	if (!rd->n_reg_rules)
		return false;

	if (WARN_ON(rd->n_reg_rules > NL80211_MAX_SUPP_REG_RULES))
		return false;

	for (i = 0; i < rd->n_reg_rules; i++) {
		reg_rule = &rd->reg_rules[i];
		if (!is_valid_reg_rule(reg_rule))
			return false;
	}

	return true;
}

static bool reg_does_bw_fit(const struct ieee80211_freq_range *freq_range,
			    u32 center_freq_khz, u32 bw_khz)
{
	u32 start_freq_khz, end_freq_khz;

	start_freq_khz = center_freq_khz - (bw_khz/2);
	end_freq_khz = center_freq_khz + (bw_khz/2);

	if (start_freq_khz >= freq_range->start_freq_khz &&
	    end_freq_khz <= freq_range->end_freq_khz)
		return true;

	return false;
}

/**
 * freq_in_rule_band - tells us if a frequency is in a frequency band
 * @freq_range: frequency rule we want to query
 * @freq_khz: frequency we are inquiring about
 *
 * This lets us know if a specific frequency rule is or is not relevant to
 * a specific frequency's band. Bands are device specific and artificial
 * definitions (the "2.4 GHz band", the "5 GHz band" and the "60GHz band"),
 * however it is safe for now to assume that a frequency rule should not be
 * part of a frequency's band if the start freq or end freq are off by more
 * than 2 GHz for the 2.4 and 5 GHz bands, and by more than 10 GHz for the
 * 60 GHz band.
 * This resolution can be lowered and should be considered as we add
 * regulatory rule support for other "bands".
 **/
static bool freq_in_rule_band(const struct ieee80211_freq_range *freq_range,
			      u32 freq_khz)
{
#define ONE_GHZ_IN_KHZ	1000000
	/*
	 * From 802.11ad: directional multi-gigabit (DMG):
	 * Pertaining to operation in a frequency band containing a channel
	 * with the Channel starting frequency above 45 GHz.
	 */
	u32 limit = freq_khz > 45 * ONE_GHZ_IN_KHZ ?
			10 * ONE_GHZ_IN_KHZ : 2 * ONE_GHZ_IN_KHZ;
	if (abs(freq_khz - freq_range->start_freq_khz) <= limit)
		return true;
	if (abs(freq_khz - freq_range->end_freq_khz) <= limit)
		return true;
	return false;
#undef ONE_GHZ_IN_KHZ
}

/*
 * XXX: add support for the rest of enum nl80211_reg_rule_flags, we may
 * want to just have the channel structure use these
 */
static u32 map_regdom_flags(u32 rd_flags)
{
	u32 channel_flags = 0;
	if (rd_flags & NL80211_RRF_NO_IR_ALL)
		channel_flags |= IEEE80211_CHAN_NO_IR;
	if (rd_flags & NL80211_RRF_DFS)
		channel_flags |= IEEE80211_CHAN_RADAR;
	if (rd_flags & NL80211_RRF_NO_OFDM)
		channel_flags |= IEEE80211_CHAN_NO_OFDM;
	if (rd_flags & NL80211_RRF_NO_OUTDOOR)
		channel_flags |= IEEE80211_CHAN_INDOOR_ONLY;
	if (rd_flags & NL80211_RRF_GO_CONCURRENT)
		channel_flags |= IEEE80211_CHAN_IR_CONCURRENT;
	if (rd_flags & NL80211_RRF_NO_HT40MINUS)
		channel_flags |= IEEE80211_CHAN_NO_HT40MINUS;
	if (rd_flags & NL80211_RRF_NO_HT40PLUS)
		channel_flags |= IEEE80211_CHAN_NO_HT40PLUS;
	if (rd_flags & NL80211_RRF_NO_80MHZ)
		channel_flags |= IEEE80211_CHAN_NO_80MHZ;
	if (rd_flags & NL80211_RRF_NO_160MHZ)
		channel_flags |= IEEE80211_CHAN_NO_160MHZ;
	return channel_flags;
}

static const struct ieee80211_reg_rule *
freq_reg_info_regd(struct wiphy *wiphy, u32 center_freq,
		   const struct ieee80211_regdomain *regd)
{
	int i;
	bool band_rule_found = false;
	bool bw_fits = false;

	if (!regd)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < regd->n_reg_rules; i++) {
		const struct ieee80211_reg_rule *rr;
		const struct ieee80211_freq_range *fr = NULL;

		rr = &regd->reg_rules[i];
		fr = &rr->freq_range;

		/*
		 * We only need to know if one frequency rule was
		 * was in center_freq's band, that's enough, so lets
		 * not overwrite it once found
		 */
		if (!band_rule_found)
			band_rule_found = freq_in_rule_band(fr, center_freq);

		bw_fits = reg_does_bw_fit(fr, center_freq, MHZ_TO_KHZ(20));

		if (band_rule_found && bw_fits)
			return rr;
	}

	if (!band_rule_found)
		return ERR_PTR(-ERANGE);

	return ERR_PTR(-EINVAL);
}

#ifdef CONFIG_CFG80211_REG_DEBUG
static void chan_reg_rule_print_dbg(const struct ieee80211_regdomain *regd,
				    struct ieee80211_channel *chan,
				    const struct ieee80211_reg_rule *reg_rule)
{
	const struct ieee80211_power_rule *power_rule;
	const struct ieee80211_freq_range *freq_range;
	char max_antenna_gain[32], bw[32];

	power_rule = &reg_rule->power_rule;
	freq_range = &reg_rule->freq_range;

	if (!power_rule->max_antenna_gain)
		snprintf(max_antenna_gain, sizeof(max_antenna_gain), "N/A");
	else
		snprintf(max_antenna_gain, sizeof(max_antenna_gain), "%d",
			 power_rule->max_antenna_gain);

	if (reg_rule->flags & NL80211_RRF_AUTO_BW)
		snprintf(bw, sizeof(bw), "%d KHz, %d KHz AUTO",
			 freq_range->max_bandwidth_khz,
			 reg_get_max_bandwidth(regd, reg_rule));
	else
		snprintf(bw, sizeof(bw), "%d KHz",
			 freq_range->max_bandwidth_khz);

	REG_DBG_PRINT("Updating information on frequency %d MHz with regulatory rule:\n",
		      chan->center_freq);

	REG_DBG_PRINT("%d KHz - %d KHz @ %s), (%s mBi, %d mBm)\n",
		      freq_range->start_freq_khz, freq_range->end_freq_khz,
		      bw, max_antenna_gain,
		      power_rule->max_eirp);
}
#else
static void chan_reg_rule_print_dbg(const struct ieee80211_regdomain *regd,
				    struct ieee80211_channel *chan,
				    const struct ieee80211_reg_rule *reg_rule)
{
	return;
}
#endif

static bool is_ht40_allowed(struct ieee80211_channel *chan)
{
	if (!chan)
		return false;
	if (chan->flags & IEEE80211_CHAN_DISABLED)
		return false;
	/* This would happen when regulatory rules disallow HT40 completely */
	if ((chan->flags & IEEE80211_CHAN_NO_HT40) == IEEE80211_CHAN_NO_HT40)
		return false;
	return true;
}

static void reg_process_ht_flags_channel(struct wiphy *wiphy,
					 struct ieee80211_channel *channel)
{
	struct ieee80211_supported_band *sband = wiphy->bands[channel->band];
	struct ieee80211_channel *channel_before = NULL, *channel_after = NULL;
	unsigned int i;

	if (!is_ht40_allowed(channel)) {
		channel->flags |= IEEE80211_CHAN_NO_HT40;
		return;
	}

	/*
	 * We need to ensure the extension channels exist to
	 * be able to use HT40- or HT40+, this finds them (or not)
	 */
	for (i = 0; i < sband->n_channels; i++) {
		struct ieee80211_channel *c = &sband->channels[i];

		if (c->center_freq == (channel->center_freq - 20))
			channel_before = c;
		if (c->center_freq == (channel->center_freq + 20))
			channel_after = c;
	}

	/*
	 * Please note that this assumes target bandwidth is 20 MHz,
	 * if that ever changes we also need to change the below logic
	 * to include that as well.
	 */
	if (!is_ht40_allowed(channel_before))
		channel->flags |= IEEE80211_CHAN_NO_HT40MINUS;
	else
		channel->flags &= ~IEEE80211_CHAN_NO_HT40MINUS;

	if (!is_ht40_allowed(channel_after))
		channel->flags |= IEEE80211_CHAN_NO_HT40PLUS;
	else
		channel->flags &= ~IEEE80211_CHAN_NO_HT40PLUS;
}

static void reg_process_ht_flags_band(struct wiphy *wiphy,
				      struct ieee80211_supported_band *sband)
{
	unsigned int i;

	if (!sband)
		return;

	for (i = 0; i < sband->n_channels; i++)
		reg_process_ht_flags_channel(wiphy, &sband->channels[i]);
}

static void reg_process_ht_flags(struct wiphy *wiphy)
{
	enum nl80211_band band;

	if (!wiphy)
		return;

	for (band = 0; band < NUM_NL80211_BANDS; band++)
		reg_process_ht_flags_band(wiphy, wiphy->bands[band]);
}

static void handle_channel_custom(struct wiphy *wiphy,
				  struct ieee80211_channel *chan,
				  const struct ieee80211_regdomain *regd)
{
	u32 bw_flags = 0;
	const struct ieee80211_reg_rule *reg_rule = NULL;
	const struct ieee80211_power_rule *power_rule = NULL;
	const struct ieee80211_freq_range *freq_range = NULL;
	u32 max_bandwidth_khz;

	reg_rule = freq_reg_info_regd(wiphy, MHZ_TO_KHZ(chan->center_freq),
				      regd);

	if (IS_ERR(reg_rule)) {
		REG_DBG_PRINT("Disabling freq %d MHz as custom regd has no rule that fits it\n",
			      chan->center_freq);
		if (wiphy->regulatory_flags & REGULATORY_WIPHY_SELF_MANAGED) {
			chan->flags |= IEEE80211_CHAN_DISABLED;
		} else {
			chan->orig_flags |= IEEE80211_CHAN_DISABLED;
			chan->flags = chan->orig_flags;
		}
		return;
	}

	chan_reg_rule_print_dbg(regd, chan, reg_rule);

	power_rule = &reg_rule->power_rule;
	freq_range = &reg_rule->freq_range;

	max_bandwidth_khz = freq_range->max_bandwidth_khz;
	/* Check if auto calculation requested */
	if (reg_rule->flags & NL80211_RRF_AUTO_BW)
		max_bandwidth_khz = reg_get_max_bandwidth(regd, reg_rule);

	if (max_bandwidth_khz < MHZ_TO_KHZ(40))
		bw_flags = IEEE80211_CHAN_NO_HT40;
	if (max_bandwidth_khz < MHZ_TO_KHZ(80))
		bw_flags |= IEEE80211_CHAN_NO_80MHZ;
	if (max_bandwidth_khz < MHZ_TO_KHZ(160))
		bw_flags |= IEEE80211_CHAN_NO_160MHZ;

	chan->beacon_found = false;

	if (wiphy->regulatory_flags & REGULATORY_WIPHY_SELF_MANAGED)
		chan->flags = chan->orig_flags | bw_flags |
			      map_regdom_flags(reg_rule->flags);
	else
		chan->flags |= map_regdom_flags(reg_rule->flags) | bw_flags;

	chan->max_antenna_gain = (int) MBI_TO_DBI(power_rule->max_antenna_gain);
	chan->max_reg_power = chan->max_power =
		(int) MBM_TO_DBM(power_rule->max_eirp);

	chan->max_power = chan->max_reg_power;
}

static void handle_band_custom(struct wiphy *wiphy,
			       struct ieee80211_supported_band *sband,
			       const struct ieee80211_regdomain *regd)
{
	unsigned int i;

	if (!sband)
		return;

	for (i = 0; i < sband->n_channels; i++)
		handle_channel_custom(wiphy, &sband->channels[i], regd);
}

static void reg_process_self_managed_hints(void)
{
	struct cfg80211_registered_device *rdev;
	struct wiphy *wiphy;
	const struct ieee80211_regdomain *regd;
	enum nl80211_band band;
	struct ieee80211_local *local;

	ASSERT_RTNL();

	list_for_each_entry(rdev, &cfg80211_rdev_list, list) {
		local = container_of(rdev, struct ieee80211_local, rdev);
		wiphy = local->hw.wiphy;
		if (!wiphy)
			continue;

		spin_lock(&reg_requests_lock);
		regd = rdev->requested_regd;
		rdev->requested_regd = NULL;
		spin_unlock(&reg_requests_lock);

		if (regd == NULL)
			continue;

		for (band = 0; band < NUM_NL80211_BANDS; band++)
			handle_band_custom(wiphy, wiphy->bands[band], regd);

		reg_process_ht_flags(wiphy);
		kfree(wiphy->regd);
		wiphy->regd = regd;
	}
}

static void reg_todo(struct work_struct *work)
{
	rtnl_lock();
	reg_process_self_managed_hints();
	rtnl_unlock();
}

static void print_rd_rules(const struct ieee80211_regdomain *rd)
{
	unsigned int i;
	const struct ieee80211_reg_rule *reg_rule = NULL;
	const struct ieee80211_freq_range *freq_range = NULL;
	const struct ieee80211_power_rule *power_rule = NULL;
	char bw[32], cac_time[32];

	pr_info("  (start_freq - end_freq @ bandwidth), (max_antenna_gain, max_eirp), (dfs_cac_time)\n");

	for (i = 0; i < rd->n_reg_rules; i++) {
		reg_rule = &rd->reg_rules[i];
		freq_range = &reg_rule->freq_range;
		power_rule = &reg_rule->power_rule;

		if (reg_rule->flags & NL80211_RRF_AUTO_BW)
			snprintf(bw, sizeof(bw), "%d KHz, %d KHz AUTO",
				 freq_range->max_bandwidth_khz,
				 reg_get_max_bandwidth(rd, reg_rule));
		else
			snprintf(bw, sizeof(bw), "%d KHz",
				 freq_range->max_bandwidth_khz);

		/*
		 * There may not be documentation for max antenna gain
		 * in certain regions
		 */
		if (power_rule->max_antenna_gain)
			pr_info("  (%d KHz - %d KHz @ %s), (%d mBi, %d mBm), (%s)\n",
				freq_range->start_freq_khz,
				freq_range->end_freq_khz,
				bw,
				power_rule->max_antenna_gain,
				power_rule->max_eirp,
				cac_time);
		else
			pr_info("  (%d KHz - %d KHz @ %s), (N/A, %d mBm), (%s)\n",
				freq_range->start_freq_khz,
				freq_range->end_freq_khz,
				bw,
				power_rule->max_eirp,
				cac_time);
	}
}

static void print_regdomain_info(const struct ieee80211_regdomain *rd)
{
	pr_info("Regulatory domain: %c%c\n", rd->alpha2[0], rd->alpha2[1]);
	print_rd_rules(rd);
}

int regulatory_set_wiphy_regd(struct wiphy *wiphy,
			      struct ieee80211_regdomain *rd)
{
	const struct ieee80211_regdomain *regd;
	const struct ieee80211_regdomain *prev_regd;
	struct cfg80211_registered_device *rdev;

	if (WARN_ON(!wiphy || !rd))
		return -EINVAL;

	if (WARN(!(wiphy->regulatory_flags & REGULATORY_WIPHY_SELF_MANAGED),
		 "wiphy should have REGULATORY_WIPHY_SELF_MANAGED\n"))
		return -EPERM;

	if (WARN(!is_valid_rd(rd), "Invalid regulatory domain detected\n")) {
		print_regdomain_info(rd);
		return -EINVAL;
	}

	regd = reg_copy_regd(rd);
	if (IS_ERR(regd))
		return PTR_ERR(regd);

	spin_lock(&reg_requests_lock);
	rdev = wiphy_to_rdev(wiphy);
	if (WARN_ON(!rdev)) {
		spin_unlock(&reg_requests_lock);
		return -ENODEV;
	}

	prev_regd = rdev->requested_regd;
	rdev->requested_regd = regd;
	spin_unlock(&reg_requests_lock);

	kfree(prev_regd);

	schedule_work(&reg_work);
	return 0;
}
EXPORT_SYMBOL(regulatory_set_wiphy_regd);

int regulatory_set_wiphy_regd_sync_rtnl(struct wiphy *wiphy,
					struct ieee80211_regdomain *rd)
{
	int ret;

	ASSERT_RTNL();

	ret = regulatory_set_wiphy_regd(wiphy, rd);
	if (ret)
		return ret;

	/* process the request immediately */
	reg_process_self_managed_hints();
	return 0;
}
EXPORT_SYMBOL(regulatory_set_wiphy_regd_sync_rtnl);

void intel_regulatory_register(struct ieee80211_local *local)
{
	struct wiphy *wiphy = local->hw.wiphy;

	rtnl_lock();
	/* self-managed devices ignore external hints */
	if (wiphy->regulatory_flags & REGULATORY_WIPHY_SELF_MANAGED) {
		unsigned long flags;

		wiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS |
					   REGULATORY_COUNTRY_IE_IGNORE;

		spin_lock_irqsave(&reg_requests_lock, flags);
		list_add(&local->rdev.list, &cfg80211_rdev_list);
		spin_unlock_irqrestore(&reg_requests_lock, flags);
		dev_info(&wiphy->dev, "LAR device registered\n");
	}
	rtnl_unlock();
}

void intel_regulatory_deregister(struct ieee80211_local *local)
{
	struct wiphy *wiphy = local->hw.wiphy;
	struct cfg80211_registered_device *rdev;
	unsigned long flags;

	rtnl_lock();
	rdev = wiphy_to_rdev(wiphy);

	/* intel non-LAR device */
	if (!rdev) {
		rtnl_unlock();
		return;
	}

	spin_lock_irqsave(&reg_requests_lock, flags);
	list_del(&rdev->list);
	spin_unlock_irqrestore(&reg_requests_lock, flags);
	kfree(rdev->requested_regd);
	rdev->requested_regd = NULL;
	rtnl_unlock();
	dev_info(&wiphy->dev, "LAR device unregistered\n");

	flush_work(&reg_work);
}
#endif /* CFG80211_VERSION < KERNEL_VERSION(4,0,0) */
