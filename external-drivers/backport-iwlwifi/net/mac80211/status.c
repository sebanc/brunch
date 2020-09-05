// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2013-2015  Intel Mobile Communications GmbH
 * Copyright 2018-2019  Intel Corporation
 */

#include <linux/export.h>
#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <asm/unaligned.h>
#include "ieee80211_i.h"
#include "rate.h"
#include "mesh.h"
#include "led.h"
#include "wme.h"


void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw,
				 struct sk_buff *skb)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	int tmp;

	skb->pkt_type = IEEE80211_TX_STATUS_MSG;
	skb_queue_tail(info->flags & IEEE80211_TX_CTL_REQ_TX_STATUS ?
		       &local->skb_queue : &local->skb_queue_unreliable, skb);
	tmp = skb_queue_len(&local->skb_queue) +
		skb_queue_len(&local->skb_queue_unreliable);
	while (tmp > IEEE80211_IRQSAFE_QUEUE_LIMIT &&
	       (skb = skb_dequeue(&local->skb_queue_unreliable))) {
		ieee80211_free_txskb(hw, skb);
		tmp--;
		I802_DEBUG_INC(local->tx_status_drop);
	}
	tasklet_schedule(&local->tasklet);
}
EXPORT_SYMBOL(ieee80211_tx_status_irqsafe);

static void ieee80211_handle_filtered_frame(struct ieee80211_local *local,
					    struct sta_info *sta,
					    struct sk_buff *skb)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (void *)skb->data;
	int ac;

	if (info->flags & (IEEE80211_TX_CTL_NO_PS_BUFFER |
			   IEEE80211_TX_CTL_AMPDU)) {
		ieee80211_free_txskb(&local->hw, skb);
		return;
	}

	/*
	 * This skb 'survived' a round-trip through the driver, and
	 * hopefully the driver didn't mangle it too badly. However,
	 * we can definitely not rely on the control information
	 * being correct. Clear it so we don't get junk there, and
	 * indicate that it needs new processing, but must not be
	 * modified/encrypted again.
	 */
	memset(&info->control, 0, sizeof(info->control));

	info->control.jiffies = jiffies;
	info->control.vif = &sta->sdata->vif;
	info->flags |= IEEE80211_TX_INTFL_NEED_TXPROCESSING |
		       IEEE80211_TX_INTFL_RETRANSMISSION;
	info->flags &= ~IEEE80211_TX_TEMPORARY_FLAGS;

	sta->status_stats.filtered++;

	/*
	 * Clear more-data bit on filtered frames, it might be set
	 * but later frames might time out so it might have to be
	 * clear again ... It's all rather unlikely (this frame
	 * should time out first, right?) but let's not confuse
	 * peers unnecessarily.
	 */
	if (hdr->frame_control & cpu_to_le16(IEEE80211_FCTL_MOREDATA))
		hdr->frame_control &= ~cpu_to_le16(IEEE80211_FCTL_MOREDATA);

	if (ieee80211_is_data_qos(hdr->frame_control)) {
		u8 *p = ieee80211_get_qos_ctl(hdr);
		int tid = *p & IEEE80211_QOS_CTL_TID_MASK;

		/*
		 * Clear EOSP if set, this could happen e.g.
		 * if an absence period (us being a P2P GO)
		 * shortens the SP.
		 */
		if (*p & IEEE80211_QOS_CTL_EOSP)
			*p &= ~IEEE80211_QOS_CTL_EOSP;
		ac = ieee80211_ac_from_tid(tid);
	} else {
		ac = IEEE80211_AC_BE;
	}

	/*
	 * Clear the TX filter mask for this STA when sending the next
	 * packet. If the STA went to power save mode, this will happen
	 * when it wakes up for the next time.
	 */
	set_sta_flag(sta, WLAN_STA_CLEAR_PS_FILT);
	ieee80211_clear_fast_xmit(sta);

	/*
	 * This code races in the following way:
	 *
	 *  (1) STA sends frame indicating it will go to sleep and does so
	 *  (2) hardware/firmware adds STA to filter list, passes frame up
	 *  (3) hardware/firmware processes TX fifo and suppresses a frame
	 *  (4) we get TX status before having processed the frame and
	 *	knowing that the STA has gone to sleep.
	 *
	 * This is actually quite unlikely even when both those events are
	 * processed from interrupts coming in quickly after one another or
	 * even at the same time because we queue both TX status events and
	 * RX frames to be processed by a tasklet and process them in the
	 * same order that they were received or TX status last. Hence, there
	 * is no race as long as the frame RX is processed before the next TX
	 * status, which drivers can ensure, see below.
	 *
	 * Note that this can only happen if the hardware or firmware can
	 * actually add STAs to the filter list, if this is done by the
	 * driver in response to set_tim() (which will only reduce the race
	 * this whole filtering tries to solve, not completely solve it)
	 * this situation cannot happen.
	 *
	 * To completely solve this race drivers need to make sure that they
	 *  (a) don't mix the irq-safe/not irq-safe TX status/RX processing
	 *	functions and
	 *  (b) always process RX events before TX status events if ordering
	 *      can be unknown, for example with different interrupt status
	 *	bits.
	 *  (c) if PS mode transitions are manual (i.e. the flag
	 *      %IEEE80211_HW_AP_LINK_PS is set), always process PS state
	 *      changes before calling TX status events if ordering can be
	 *	unknown.
	 */
	if (test_sta_flag(sta, WLAN_STA_PS_STA) &&
	    skb_queue_len(&sta->tx_filtered[ac]) < STA_MAX_TX_BUFFER) {
		skb_queue_tail(&sta->tx_filtered[ac], skb);
		sta_info_recalc_tim(sta);

		if (!timer_pending(&local->sta_cleanup))
			mod_timer(&local->sta_cleanup,
				  round_jiffies(jiffies +
						STA_INFO_CLEANUP_INTERVAL));
		return;
	}

	if (!test_sta_flag(sta, WLAN_STA_PS_STA) &&
	    !(info->flags & IEEE80211_TX_INTFL_RETRIED)) {
		/* Software retry the packet once */
		info->flags |= IEEE80211_TX_INTFL_RETRIED;
		ieee80211_add_pending_skb(local, skb);
		return;
	}

	ps_dbg_ratelimited(sta->sdata,
			   "dropped TX filtered frame, queue_len=%d PS=%d @%lu\n",
			   skb_queue_len(&sta->tx_filtered[ac]),
			   !!test_sta_flag(sta, WLAN_STA_PS_STA), jiffies);
	ieee80211_free_txskb(&local->hw, skb);
}

static void ieee80211_check_pending_bar(struct sta_info *sta, u8 *addr, u8 tid)
{
	struct tid_ampdu_tx *tid_tx;

	tid_tx = rcu_dereference(sta->ampdu_mlme.tid_tx[tid]);
	if (!tid_tx || !tid_tx->bar_pending)
		return;

	tid_tx->bar_pending = false;
	ieee80211_send_bar(&sta->sdata->vif, addr, tid, tid_tx->failed_bar_ssn);
}

static void ieee80211_frame_acked(struct sta_info *sta, struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = (void *) skb->data;
	struct ieee80211_local *local = sta->local;
	struct ieee80211_sub_if_data *sdata = sta->sdata;
	struct ieee80211_tx_info *txinfo = IEEE80211_SKB_CB(skb);

	if (ieee80211_hw_check(&local->hw, REPORTS_TX_ACK_STATUS)) {
		sta->status_stats.last_ack = jiffies;
		if (txinfo->status.is_valid_ack_signal) {
			sta->status_stats.last_ack_signal =
					 (s8)txinfo->status.ack_signal;
			sta->status_stats.ack_signal_filled = true;
			ewma_avg_signal_add(&sta->status_stats.avg_ack_signal,
					    -txinfo->status.ack_signal);
		}
	}

	if (ieee80211_is_data_qos(mgmt->frame_control)) {
		struct ieee80211_hdr *hdr = (void *) skb->data;
		u8 *qc = ieee80211_get_qos_ctl(hdr);
		u16 tid = qc[0] & 0xf;

		ieee80211_check_pending_bar(sta, hdr->addr1, tid);
	}

	if (ieee80211_is_action(mgmt->frame_control) &&
	    !ieee80211_has_protected(mgmt->frame_control) &&
	    mgmt->u.action.category == WLAN_CATEGORY_HT &&
	    mgmt->u.action.u.ht_smps.action == WLAN_HT_ACTION_SMPS &&
	    ieee80211_sdata_running(sdata)) {
		enum ieee80211_smps_mode smps_mode;

		switch (mgmt->u.action.u.ht_smps.smps_control) {
		case WLAN_HT_SMPS_CONTROL_DYNAMIC:
			smps_mode = IEEE80211_SMPS_DYNAMIC;
			break;
		case WLAN_HT_SMPS_CONTROL_STATIC:
			smps_mode = IEEE80211_SMPS_STATIC;
			break;
		case WLAN_HT_SMPS_CONTROL_DISABLED:
		default: /* shouldn't happen since we don't send that */
			smps_mode = IEEE80211_SMPS_OFF;
			break;
		}

		if (sdata->vif.type == NL80211_IFTYPE_STATION) {
			/*
			 * This update looks racy, but isn't -- if we come
			 * here we've definitely got a station that we're
			 * talking to, and on a managed interface that can
			 * only be the AP. And the only other place updating
			 * this variable in managed mode is before association.
			 */
			sdata->smps_mode = smps_mode;
			ieee80211_queue_work(&local->hw, &sdata->recalc_smps);
		} else if (sdata->vif.type == NL80211_IFTYPE_AP ||
			   sdata->vif.type == NL80211_IFTYPE_AP_VLAN) {
			sta->known_smps_mode = smps_mode;
		}
	}
}

static void ieee80211_set_bar_pending(struct sta_info *sta, u8 tid, u16 ssn)
{
	struct tid_ampdu_tx *tid_tx;

	tid_tx = rcu_dereference(sta->ampdu_mlme.tid_tx[tid]);
	if (!tid_tx)
		return;

	tid_tx->failed_bar_ssn = ssn;
	tid_tx->bar_pending = true;
}

static int ieee80211_tx_radiotap_len(struct ieee80211_tx_info *info)
{
	int len = sizeof(struct ieee80211_radiotap_header);

	/* IEEE80211_RADIOTAP_RATE rate */
	if (info->status.rates[0].idx >= 0 &&
	    !(info->status.rates[0].flags & (IEEE80211_TX_RC_MCS |
					     IEEE80211_TX_RC_VHT_MCS)))
		len += 2;

	/* IEEE80211_RADIOTAP_TX_FLAGS */
	len += 2;

	/* IEEE80211_RADIOTAP_DATA_RETRIES */
	len += 1;

	/* IEEE80211_RADIOTAP_MCS
	 * IEEE80211_RADIOTAP_VHT */
	if (info->status.rates[0].idx >= 0) {
		if (info->status.rates[0].flags & IEEE80211_TX_RC_MCS)
			len += 3;
		else if (info->status.rates[0].flags & IEEE80211_TX_RC_VHT_MCS)
			len = ALIGN(len, 2) + 12;
	}

	return len;
}

static void
ieee80211_add_tx_radiotap_header(struct ieee80211_local *local,
				 struct ieee80211_supported_band *sband,
				 struct sk_buff *skb, int retry_count,
				 int rtap_len, int shift)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	struct ieee80211_radiotap_header *rthdr;
	unsigned char *pos;
	u16 txflags;

	rthdr = skb_push(skb, rtap_len);

	memset(rthdr, 0, rtap_len);
	rthdr->it_len = cpu_to_le16(rtap_len);
	rthdr->it_present =
		cpu_to_le32((1 << IEEE80211_RADIOTAP_TX_FLAGS) |
			    (1 << IEEE80211_RADIOTAP_DATA_RETRIES));
	pos = (unsigned char *)(rthdr + 1);

	/*
	 * XXX: Once radiotap gets the bitmap reset thing the vendor
	 *	extensions proposal contains, we can actually report
	 *	the whole set of tries we did.
	 */

	/* IEEE80211_RADIOTAP_RATE */
	if (info->status.rates[0].idx >= 0 &&
	    !(info->status.rates[0].flags & (IEEE80211_TX_RC_MCS |
					     IEEE80211_TX_RC_VHT_MCS))) {
		u16 rate;

		rthdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
		rate = sband->bitrates[info->status.rates[0].idx].bitrate;
		*pos = DIV_ROUND_UP(rate, 5 * (1 << shift));
		/* padding for tx flags */
		pos += 2;
	}

	/* IEEE80211_RADIOTAP_TX_FLAGS */
	txflags = 0;
	if (!(info->flags & IEEE80211_TX_STAT_ACK) &&
	    !is_multicast_ether_addr(hdr->addr1))
		txflags |= IEEE80211_RADIOTAP_F_TX_FAIL;

	if (info->status.rates[0].flags & IEEE80211_TX_RC_USE_CTS_PROTECT)
		txflags |= IEEE80211_RADIOTAP_F_TX_CTS;
	if (info->status.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS)
		txflags |= IEEE80211_RADIOTAP_F_TX_RTS;

	put_unaligned_le16(txflags, pos);
	pos += 2;

	/* IEEE80211_RADIOTAP_DATA_RETRIES */
	/* for now report the total retry_count */
	*pos = retry_count;
	pos++;

	if (info->status.rates[0].idx < 0)
		return;

	/* IEEE80211_RADIOTAP_MCS
	 * IEEE80211_RADIOTAP_VHT */
	if (info->status.rates[0].flags & IEEE80211_TX_RC_MCS) {
		rthdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_MCS);
		pos[0] = IEEE80211_RADIOTAP_MCS_HAVE_MCS |
			 IEEE80211_RADIOTAP_MCS_HAVE_GI |
			 IEEE80211_RADIOTAP_MCS_HAVE_BW;
		if (info->status.rates[0].flags & IEEE80211_TX_RC_SHORT_GI)
			pos[1] |= IEEE80211_RADIOTAP_MCS_SGI;
		if (info->status.rates[0].flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
			pos[1] |= IEEE80211_RADIOTAP_MCS_BW_40;
		if (info->status.rates[0].flags & IEEE80211_TX_RC_GREEN_FIELD)
			pos[1] |= IEEE80211_RADIOTAP_MCS_FMT_GF;
		pos[2] = info->status.rates[0].idx;
		pos += 3;
	} else if (info->status.rates[0].flags & IEEE80211_TX_RC_VHT_MCS) {
		u16 known = local->hw.radiotap_vht_details &
			(IEEE80211_RADIOTAP_VHT_KNOWN_GI |
			 IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH);

		rthdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_VHT);

		/* required alignment from rthdr */
		pos = (u8 *)rthdr + ALIGN(pos - (u8 *)rthdr, 2);

		/* u16 known - IEEE80211_RADIOTAP_VHT_KNOWN_* */
		put_unaligned_le16(known, pos);
		pos += 2;

		/* u8 flags - IEEE80211_RADIOTAP_VHT_FLAG_* */
		if (info->status.rates[0].flags & IEEE80211_TX_RC_SHORT_GI)
			*pos |= IEEE80211_RADIOTAP_VHT_FLAG_SGI;
		pos++;

		/* u8 bandwidth */
		if (info->status.rates[0].flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
			*pos = 1;
		else if (info->status.rates[0].flags & IEEE80211_TX_RC_80_MHZ_WIDTH)
			*pos = 4;
		else if (info->status.rates[0].flags & IEEE80211_TX_RC_160_MHZ_WIDTH)
			*pos = 11;
		else /* IEEE80211_TX_RC_{20_MHZ_WIDTH,FIXME:DUP_DATA} */
			*pos = 0;
		pos++;

		/* u8 mcs_nss[4] */
		*pos = (ieee80211_rate_get_vht_mcs(&info->status.rates[0]) << 4) |
			ieee80211_rate_get_vht_nss(&info->status.rates[0]);
		pos += 4;

		/* u8 coding */
		pos++;
		/* u8 group_id */
		pos++;
		/* u16 partial_aid */
		pos += 2;
	}
}

/*
 * Handles the tx for TDLS teardown frames.
 * If the frame wasn't ACKed by the peer - it will be re-sent through the AP
 */
static void ieee80211_tdls_td_tx_handle(struct ieee80211_local *local,
					struct ieee80211_sub_if_data *sdata,
					struct sk_buff *skb, u32 flags)
{
	struct sk_buff *teardown_skb;
	struct sk_buff *orig_teardown_skb;
	bool is_teardown = false;

	/* Get the teardown data we need and free the lock */
	spin_lock(&sdata->u.mgd.teardown_lock);
	teardown_skb = sdata->u.mgd.teardown_skb;
	orig_teardown_skb = sdata->u.mgd.orig_teardown_skb;
	if ((skb == orig_teardown_skb) && teardown_skb) {
		sdata->u.mgd.teardown_skb = NULL;
		sdata->u.mgd.orig_teardown_skb = NULL;
		is_teardown = true;
	}
	spin_unlock(&sdata->u.mgd.teardown_lock);

	if (is_teardown) {
		/* This mechanism relies on being able to get ACKs */
		WARN_ON(!ieee80211_hw_check(&local->hw, REPORTS_TX_ACK_STATUS));

		/* Check if peer has ACKed */
		if (flags & IEEE80211_TX_STAT_ACK) {
			dev_kfree_skb_any(teardown_skb);
		} else {
			tdls_dbg(sdata,
				 "TDLS Resending teardown through AP\n");

			ieee80211_subif_start_xmit(teardown_skb, skb->dev);
		}
	}
}

static struct ieee80211_sub_if_data *
ieee80211_sdata_from_skb(struct ieee80211_local *local, struct sk_buff *skb)
{
	struct ieee80211_sub_if_data *sdata;

	if (skb->dev) {
		list_for_each_entry_rcu(sdata, &local->interfaces, list) {
			if (!sdata->dev)
				continue;

			if (skb->dev == sdata->dev)
				return sdata;
		}

		return NULL;
	}

	return rcu_dereference(local->p2p_sdata);
}

static void ieee80211_report_ack_skb(struct ieee80211_local *local,
				     struct ieee80211_tx_info *info,
				     bool acked, bool dropped)
{
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&local->ack_status_lock, flags);
	skb = idr_remove(&local->ack_status_frames, info->ack_frame_id);
	spin_unlock_irqrestore(&local->ack_status_lock, flags);

	if (!skb)
		return;

	if (info->flags & IEEE80211_TX_INTFL_NL80211_FRAME_TX) {
		u64 cookie = IEEE80211_SKB_CB(skb)->ack.cookie;
		struct ieee80211_sub_if_data *sdata;
		struct ieee80211_hdr *hdr = (void *)skb->data;

		rcu_read_lock();
		sdata = ieee80211_sdata_from_skb(local, skb);
		if (sdata) {
			if (ieee80211_is_nullfunc(hdr->frame_control) ||
			    ieee80211_is_qos_nullfunc(hdr->frame_control))
				cfg80211_probe_status(sdata->dev, hdr->addr1,
						      cookie, acked,
						      info->status.ack_signal,
						      info->status.is_valid_ack_signal,
						      GFP_ATOMIC);
			else
				cfg80211_mgmt_tx_status(&sdata->wdev, cookie,
							skb->data, skb->len,
							acked, GFP_ATOMIC);
		}
		rcu_read_unlock();

		dev_kfree_skb_any(skb);
	} else if (dropped) {
		dev_kfree_skb_any(skb);
	} else {
		/* consumes skb */
		skb_complete_wifi_ack(skb, acked);
	}
}

static void ieee80211_report_used_skb(struct ieee80211_local *local,
				      struct sk_buff *skb, bool dropped)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (void *)skb->data;
	bool acked = info->flags & IEEE80211_TX_STAT_ACK;

	if (dropped)
		acked = false;

	if (info->flags & IEEE80211_TX_INTFL_MLME_CONN_TX) {
		struct ieee80211_sub_if_data *sdata;

		rcu_read_lock();

		sdata = ieee80211_sdata_from_skb(local, skb);

		if (!sdata) {
			skb->dev = NULL;
		} else {
			unsigned int hdr_size =
				ieee80211_hdrlen(hdr->frame_control);

			/* Check to see if packet is a TDLS teardown packet */
			if (ieee80211_is_data(hdr->frame_control) &&
			    (ieee80211_get_tdls_action(skb, hdr_size) ==
			     WLAN_TDLS_TEARDOWN))
				ieee80211_tdls_td_tx_handle(local, sdata, skb,
							    info->flags);
			else
				ieee80211_mgd_conn_tx_status(sdata,
							     hdr->frame_control,
							     acked);
		}

		rcu_read_unlock();
	} else if (info->ack_frame_id) {
		ieee80211_report_ack_skb(local, info, acked, dropped);
	}

	if (!dropped && skb->destructor) {
#if LINUX_VERSION_IS_GEQ(3,3,0)
		skb->wifi_acked_valid = 1;
		skb->wifi_acked = acked;
#endif
	}

	ieee80211_led_tx(local);

	if (skb_has_frag_list(skb)) {
		kfree_skb_list(skb_shinfo(skb)->frag_list);
		skb_shinfo(skb)->frag_list = NULL;
	}
}

#ifdef CPTCFG_MAC80211_LATENCY_MEASUREMENTS
static void update_consec_bins(u32 *bins, u32 *bin_ranges, int bin_range_count,
			       int msrmnt)
{
	int i;

	for (i = bin_range_count - 1; 0 <= i; i--) {
		if (bin_ranges[i] <= msrmnt) {
			bins[i]++;
			break;
		}
	}
}

/*
 * Measure how many tx frames that were consecutively lost
 * or that were sent successfully but  there latency passed
 * a certain thershold and therefor considered lost.
 */
static void
tx_consec_loss_msrmnt(struct ieee80211_tx_consec_loss_ranges *tx_consec,
		      struct sta_info *sta, int tid, u32 msrmnt,
		      int pkt_loss, bool send_failed)
{
	u32 bin_range_count;
	u32 *bin_ranges;
	struct ieee80211_tx_consec_loss_stat *tx_csc;

	/* assert Tx consecutive packet loss stats are enabled */
	if (!tx_consec)
		return;

	tx_csc = &sta->tx_consec[tid];

	bin_range_count = tx_consec->n_ranges;
	bin_ranges = tx_consec->ranges;

	/*
	 * count how many Tx frames were consecutively lost within the
	 * appropriate range
	 */

	if (send_failed ||
	    (!send_failed && tx_consec->late_threshold < msrmnt / 1000)) {
		tx_csc->consec_total_loss++;
	} else {
		update_consec_bins(tx_csc->total_loss_bins, bin_ranges,
				   bin_range_count, tx_csc->consec_total_loss);
		tx_csc->consec_total_loss = 0;
	}

	/* count sent successfully && before packets were lost */
	if (!send_failed && pkt_loss)
		update_consec_bins(tx_csc->loss_bins, bin_ranges,
				   bin_range_count, pkt_loss);

	/*
	 * count how many consecutive Tx packet latencies were greater than
	 * late threshold within the appropriate range
	 * (and are considered lost even though they were sent successfully)
	 */
	if (send_failed) /* only count packets sent successfully */
		return;

	if (tx_consec->late_threshold < msrmnt / 1000) {
		tx_csc->consec_late_loss++;
	} else {
		update_consec_bins(tx_csc->late_bins, bin_ranges,
				   bin_range_count,
				   tx_csc->consec_late_loss);
		tx_csc->consec_late_loss = 0;
	}
}

/*
 * Measure Tx frames latency.
 */
static void
tx_latency_msrmnt(struct ieee80211_tx_latency_bin_ranges *tx_latency,
		  struct sta_info *sta, int tid, u32 msrmnt)
{
	int bin_range_count, i;
	u32 *bin_ranges;
	struct ieee80211_tx_latency_stat *tx_lat;

	if (!tx_latency)
		return;

	tx_lat = &sta->tx_lat[tid];

	if (tx_lat->max < msrmnt) { /* update stats */
		tx_lat->max = msrmnt;
		tx_lat->max_ts = ktime_to_us(ktime_get());
	}
	tx_lat->counter++;
	tx_lat->sum += msrmnt;

	if (!tx_lat->bins) /* bins not activated */
		return;

	/* count how many Tx frames transmitted with the appropriate latency */
	bin_range_count = tx_latency->n_ranges;
	bin_ranges = tx_latency->ranges;

	for (i = 0; i < bin_range_count; i++) {
		if (msrmnt <= bin_ranges[i]) {
			tx_lat->bins[i]++;
			break;
		}
	}
	if (i == bin_range_count) /* msrmnt is bigger than the biggest range */
		tx_lat->bins[i]++;
}

#ifdef CPTCFG_NL80211_TESTMODE
static void
tx_latency_threshold(struct ieee80211_local *local, struct sk_buff *skb,
		     struct ieee80211_tx_latency_threshold *tx_thrshld,
		     struct sta_info *sta, int tid, u32 msrmnt)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

	/*
	 * Make sure that tx_threshold is enabled
	 * for this iface && tid
	 */
	if (!tx_thrshld || !sta->tx_lat_thrshld ||
	    !sta->tx_lat_thrshld[tid])
		return;

	if (sta->tx_lat_thrshld[tid] < msrmnt / 1000) {
		struct ieee80211_event event = {
			.type = TX_LATENCY_EVENT,
			.u.tx_lat.mode = tx_thrshld->monitor_record_mode,
			.u.tx_lat.monitor_collec_wind =
				tx_thrshld->monitor_collec_wind,
			.u.tx_lat.pkt_start = ktime_to_us(skb->tstamp),
			.u.tx_lat.pkt_end = ktime_to_us(skb->tstamp) + msrmnt,
			.u.tx_lat.msrmnt = msrmnt / 1000,
			.u.tx_lat.tid = tid,
			.u.tx_lat.seq = (le16_to_cpu(hdr->seq_ctrl) &
					 IEEE80211_SCTL_SEQ) >> 4,

		};
		drv_event_callback(local, sta->sdata, &event);
	}
}
#endif

static u32 ieee80211_calc_tx_latency(struct ieee80211_local *local,
				     ktime_t skb_arv)
{
	s64 tmp;
	s64 ts[IEEE80211_TX_LAT_MAX_POINT];
	u32 msrmnt;

	ts[IEEE80211_TX_LAT_DEL] = ktime_to_us(ktime_get());

	/* extract previous time stamps */
	ts[IEEE80211_TX_LAT_ENTER] = ktime_to_ns(skb_arv) >> 32;
	tmp = ktime_to_ns(skb_arv) & 0xFFFFFFFF;
	ts[IEEE80211_TX_LAT_WRITE] = (tmp >> 16) + ts[IEEE80211_TX_LAT_ENTER];
	ts[IEEE80211_TX_LAT_ACK] = (tmp & 0xFFFF) + ts[IEEE80211_TX_LAT_ENTER];

	/* calculate packet latency */
	msrmnt = ts[local->tx_msrmnt_points[1]] -
		ts[local->tx_msrmnt_points[0]];

	return msrmnt;
}

/*
 * 1) Measure Tx frame completion and removal time for Tx latency statistics
 * calculation. A single Tx frame latency should be measured from when it
 * is entering the Kernel until we receive Tx complete confirmation indication
 * and remove the skb.
 * 2) Measure consecutive Tx frames that were lost or that there latency passed
 * a certain thershold and therefor considered lost.
 */
static void ieee80211_collect_tx_timing_stats(struct ieee80211_local *local,
					      struct sk_buff *skb,
					      struct sta_info *sta,
					      struct ieee80211_hdr *hdr,
					      int pkt_loss, bool send_fail)
{
	u32 msrmnt;
	u16 tid;
	__le16 fc;
	struct ieee80211_tx_latency_bin_ranges *tx_latency;
	struct ieee80211_tx_consec_loss_ranges *tx_consec;
	struct ieee80211_tx_latency_threshold *tx_thrshld;
	ktime_t skb_arv = skb->tstamp;

	tx_latency = rcu_dereference(local->tx_latency);
	tx_consec = rcu_dereference(local->tx_consec);
	tx_thrshld = rcu_dereference(local->tx_threshold);

	/*
	 * assert Tx latency or Tx consecutive packets loss stats are enabled
	 * & frame arrived when enabled
	 */
	if ((!tx_latency && !tx_consec && !tx_thrshld) ||
	    !ktime_to_ns(skb_arv))
		return;

	fc = hdr->frame_control;

	if (!ieee80211_is_data(fc)) /* make sure it is a data frame */
		return;

	/* get frame tid */
	if (ieee80211_is_data_qos(hdr->frame_control))
		tid = ieee80211_get_tid(hdr);
	else
		tid = 0;

	/* Calculate the latency */
	msrmnt = ieee80211_calc_tx_latency(local, skb_arv);

	/* update statistic regarding consecutive lost packets */
	tx_consec_loss_msrmnt(tx_consec, sta, tid, msrmnt, pkt_loss,
			      send_fail);

	/* update statistic regarding latency */
	tx_latency_msrmnt(tx_latency, sta, tid, msrmnt);

#ifdef CPTCFG_NL80211_TESTMODE
	/*
	 * trigger retrival of monitor logs
	 * (if a threshold was configured & passed)
	 */
	tx_latency_threshold(local, skb, tx_thrshld, sta, tid, msrmnt);
#endif
}

static int ieee80211_tx_lat_set_thrshld(u32 **thrshlds, u32 bitmask,
					u32 thrshld)
{
	u32 alloc_size_thrshld = sizeof(u32) * IEEE80211_NUM_TIDS;
	u32 i;

	if (!*thrshlds) {
		*thrshlds = kzalloc(alloc_size_thrshld, GFP_KERNEL);
		if (!*thrshlds)
			return -ENOMEM;
	}

	/* Set thrshld for each tid */
	for (i = 0; i < IEEE80211_NUM_TIDS; i++)
		if (bitmask & BIT(i))
			(*thrshlds)[i] = thrshld;

	return 0;
}

/*
 * Configure the Tx latency threshold
 */
void ieee80211_tx_lat_thrshld_cfg(struct ieee80211_hw *hw,
				  u32 thrshld, u16 tid_bitmap,
				  u16 window, u16 mode, u32 iface)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_latency_threshold *tx_thrshld;
	u32 alloc_size_struct;

	/* cannot change config once we have stations */
	if (local->num_sta)
		return;

	/* Tx threshold already enabled */
	if (rcu_access_pointer(local->tx_threshold))
		return;

	alloc_size_struct = sizeof(struct ieee80211_tx_latency_threshold);

	tx_thrshld = kzalloc(alloc_size_struct, GFP_KERNEL);

	if (!tx_thrshld)
		return;

	/* Not a valid interface */
	if (!(iface & BIT(IEEE80211_TX_LATENCY_BSS)) &&
	    !(iface & BIT(IEEE80211_TX_LATENCY_P2P))) {
		kfree(tx_thrshld);
		return;
	}

	/* Check iface parameters is valid */
	if (iface & BIT(IEEE80211_TX_LATENCY_BSS)) {
		if (ieee80211_tx_lat_set_thrshld(&tx_thrshld->thresholds_bss,
						 tid_bitmap, thrshld)) {
			kfree(tx_thrshld);
			return;
		}
	}
	if (iface & BIT(IEEE80211_TX_LATENCY_P2P)) {
		if (ieee80211_tx_lat_set_thrshld(&tx_thrshld->thresholds_p2p,
						 tid_bitmap, thrshld)) {
			/* free thresholds_bss in case it was alloced above */
			kfree(tx_thrshld->thresholds_bss);
			kfree(tx_thrshld);
			return;
		}
	}

	tx_thrshld->monitor_collec_wind  = window;
	tx_thrshld->monitor_record_mode = mode;

	/* Tx latency points of measurment allocation */
	local->tx_msrmnt_points[0] = IEEE80211_TX_LAT_ENTER;
	local->tx_msrmnt_points[1] = IEEE80211_TX_LAT_DEL;

	rcu_assign_pointer(local->tx_threshold, tx_thrshld);
	synchronize_rcu();
}
EXPORT_SYMBOL(ieee80211_tx_lat_thrshld_cfg);
#endif /* CPTCFG_MAC80211_LATENCY_MEASUREMENTS */

/*
 * Use a static threshold for now, best value to be determined
 * by testing ...
 * Should it depend on:
 *  - on # of retransmissions
 *  - current throughput (higher value for higher tpt)?
 */
#define STA_LOST_PKT_THRESHOLD	50
#define STA_LOST_TDLS_PKT_THRESHOLD	10
#define STA_LOST_TDLS_PKT_TIME		(10*HZ) /* 10secs since last ACK */

static void ieee80211_lost_packet(struct sta_info *sta,
				  struct ieee80211_tx_info *info)
{
	/* If driver relies on its own algorithm for station kickout, skip
	 * mac80211 packet loss mechanism.
	 */
	if (ieee80211_hw_check(&sta->local->hw, REPORTS_LOW_ACK))
		return;

	/* This packet was aggregated but doesn't carry status info */
	if ((info->flags & IEEE80211_TX_CTL_AMPDU) &&
	    !(info->flags & IEEE80211_TX_STAT_AMPDU))
		return;

	sta->status_stats.lost_packets++;
	if (!sta->sta.tdls &&
	    sta->status_stats.lost_packets < STA_LOST_PKT_THRESHOLD)
		return;

	/*
	 * If we're in TDLS mode, make sure that all STA_LOST_TDLS_PKT_THRESHOLD
	 * of the last packets were lost, and that no ACK was received in the
	 * last STA_LOST_TDLS_PKT_TIME ms, before triggering the CQM packet-loss
	 * mechanism.
	 */
	if (sta->sta.tdls &&
	    (sta->status_stats.lost_packets < STA_LOST_TDLS_PKT_THRESHOLD ||
	     time_before(jiffies,
			 sta->status_stats.last_tdls_pkt_time +
			 STA_LOST_TDLS_PKT_TIME)))
		return;

	cfg80211_cqm_pktloss_notify(sta->sdata->dev, sta->sta.addr,
				    sta->status_stats.lost_packets, GFP_ATOMIC);
	sta->status_stats.lost_packets = 0;
}

static int ieee80211_tx_get_rates(struct ieee80211_hw *hw,
				  struct ieee80211_tx_info *info,
				  int *retry_count)
{
	int rates_idx = -1;
	int count = -1;
	int i;

	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		if ((info->flags & IEEE80211_TX_CTL_AMPDU) &&
		    !(info->flags & IEEE80211_TX_STAT_AMPDU)) {
			/* just the first aggr frame carry status info */
			info->status.rates[i].idx = -1;
			info->status.rates[i].count = 0;
			break;
		} else if (info->status.rates[i].idx < 0) {
			break;
		} else if (i >= hw->max_report_rates) {
			/* the HW cannot have attempted that rate */
			info->status.rates[i].idx = -1;
			info->status.rates[i].count = 0;
			break;
		}

		count += info->status.rates[i].count;
	}
	rates_idx = i - 1;

	if (count < 0)
		count = 0;

	*retry_count = count;
	return rates_idx;
}

void ieee80211_tx_monitor(struct ieee80211_local *local, struct sk_buff *skb,
			  struct ieee80211_supported_band *sband,
			  int retry_count, int shift, bool send_to_cooked)
{
	struct sk_buff *skb2;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_sub_if_data *sdata;
	struct net_device *prev_dev = NULL;
	int rtap_len;

	/* send frame to monitor interfaces now */
	rtap_len = ieee80211_tx_radiotap_len(info);
	if (WARN_ON_ONCE(skb_headroom(skb) < rtap_len)) {
		pr_err("ieee80211_tx_status: headroom too small\n");
		dev_kfree_skb(skb);
		return;
	}
	ieee80211_add_tx_radiotap_header(local, sband, skb, retry_count,
					 rtap_len, shift);

	/* XXX: is this sufficient for BPF? */
	skb_reset_mac_header(skb);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_802_2);
	memset(skb->cb, 0, sizeof(skb->cb));

	rcu_read_lock();
	list_for_each_entry_rcu(sdata, &local->interfaces, list) {
		if (sdata->vif.type == NL80211_IFTYPE_MONITOR) {
			if (!ieee80211_sdata_running(sdata))
				continue;

			if ((sdata->u.mntr.flags & MONITOR_FLAG_COOK_FRAMES) &&
			    !send_to_cooked)
				continue;

			if (prev_dev) {
				skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2) {
					skb2->dev = prev_dev;
					netif_rx(skb2);
				}
			}

			prev_dev = sdata->dev;
		}
	}
	if (prev_dev) {
		skb->dev = prev_dev;
		netif_rx(skb);
		skb = NULL;
	}
	rcu_read_unlock();
	dev_kfree_skb(skb);
}

static void __ieee80211_tx_status(struct ieee80211_hw *hw,
				  struct ieee80211_tx_status *status)
{
	struct sk_buff *skb = status->skb;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_info *info = status->info;
	struct sta_info *sta;
	__le16 fc;
	struct ieee80211_supported_band *sband;
	int retry_count;
	int rates_idx;
	bool send_to_cooked;
	bool acked;
	struct ieee80211_bar *bar;
	int shift = 0;
	int tid = IEEE80211_NUM_TIDS;
#ifdef CPTCFG_MAC80211_LATENCY_MEASUREMENTS
	int prev_loss_pkt = 0;
	bool send_fail = true;
#endif /* CPTCFG_MAC80211_LATENCY_MEASUREMENTS */

	rates_idx = ieee80211_tx_get_rates(hw, info, &retry_count);

	sband = local->hw.wiphy->bands[info->band];
	fc = hdr->frame_control;

	if (status->sta) {
		sta = container_of(status->sta, struct sta_info, sta);
		shift = ieee80211_vif_get_shift(&sta->sdata->vif);

		if (info->flags & IEEE80211_TX_STATUS_EOSP)
			clear_sta_flag(sta, WLAN_STA_SP);

		acked = !!(info->flags & IEEE80211_TX_STAT_ACK);

		/* mesh Peer Service Period support */
		if (ieee80211_vif_is_mesh(&sta->sdata->vif) &&
		    ieee80211_is_data_qos(fc))
			ieee80211_mpsp_trigger_process(
				ieee80211_get_qos_ctl(hdr), sta, true, acked);

		if (!acked && test_sta_flag(sta, WLAN_STA_PS_STA)) {
			/*
			 * The STA is in power save mode, so assume
			 * that this TX packet failed because of that.
			 */
			ieee80211_handle_filtered_frame(local, sta, skb);
			return;
		}

		if (ieee80211_hw_check(&local->hw, HAS_RATE_CONTROL) &&
		    (ieee80211_is_data(hdr->frame_control)) &&
		    (rates_idx != -1))
			sta->tx_stats.last_rate =
				info->status.rates[rates_idx];

		if ((info->flags & IEEE80211_TX_STAT_AMPDU_NO_BACK) &&
		    (ieee80211_is_data_qos(fc))) {
			u16 ssn;
			u8 *qc;

			qc = ieee80211_get_qos_ctl(hdr);
			tid = qc[0] & 0xf;
			ssn = ((le16_to_cpu(hdr->seq_ctrl) + 0x10)
						& IEEE80211_SCTL_SEQ);
			ieee80211_send_bar(&sta->sdata->vif, hdr->addr1,
					   tid, ssn);
		} else if (ieee80211_is_data_qos(fc)) {
			u8 *qc = ieee80211_get_qos_ctl(hdr);

			tid = qc[0] & 0xf;
		}

		if (!acked && ieee80211_is_back_req(fc)) {
			u16 control;

			/*
			 * BAR failed, store the last SSN and retry sending
			 * the BAR when the next unicast transmission on the
			 * same TID succeeds.
			 */
			bar = (struct ieee80211_bar *) skb->data;
			control = le16_to_cpu(bar->control);
			if (!(control & IEEE80211_BAR_CTRL_MULTI_TID)) {
				u16 ssn = le16_to_cpu(bar->start_seq_num);

				tid = (control &
				       IEEE80211_BAR_CTRL_TID_INFO_MASK) >>
				      IEEE80211_BAR_CTRL_TID_INFO_SHIFT;

				ieee80211_set_bar_pending(sta, tid, ssn);
			}
		}

		if (info->flags & IEEE80211_TX_STAT_TX_FILTERED) {
			ieee80211_handle_filtered_frame(local, sta, skb);
			return;
		} else {
			if (!acked)
				sta->status_stats.retry_failed++;
			sta->status_stats.retry_count += retry_count;

			if (ieee80211_is_data_present(fc)) {
				if (!acked)
					sta->status_stats.msdu_failed[tid]++;

				sta->status_stats.msdu_retries[tid] +=
					retry_count;
			}
		}

		rate_control_tx_status(local, sband, status);
		if (ieee80211_vif_is_mesh(&sta->sdata->vif))
			ieee80211s_update_metric(local, sta, status);

		if (!(info->flags & IEEE80211_TX_CTL_INJECTED) && acked)
			ieee80211_frame_acked(sta, skb);

		if ((sta->sdata->vif.type == NL80211_IFTYPE_STATION) &&
		    ieee80211_hw_check(&local->hw, REPORTS_TX_ACK_STATUS))
			ieee80211_sta_tx_notify(sta->sdata, (void *) skb->data,
						acked, info->status.tx_time);

		if (info->status.tx_time &&
		    wiphy_ext_feature_isset(local->hw.wiphy,
					    NL80211_EXT_FEATURE_AIRTIME_FAIRNESS))
			ieee80211_sta_register_airtime(&sta->sta, tid,
						       info->status.tx_time, 0);

		if (ieee80211_hw_check(&local->hw, REPORTS_TX_ACK_STATUS)) {
			if (info->flags & IEEE80211_TX_STAT_ACK) {
#ifdef CPTCFG_MAC80211_LATENCY_MEASUREMENTS
				send_fail = false;
				if (sta->status_stats.lost_packets) {
					/*
					 * need to keep track of the amount for
					 * timing statistics later on
					 */
					prev_loss_pkt =
						sta->status_stats.lost_packets;
				}
#endif /* CPTCFG_MAC80211_LATENCY_MEASUREMENTS */

				if (sta->status_stats.lost_packets)
					sta->status_stats.lost_packets = 0;

				/* Track when last TDLS packet was ACKed */
				if (test_sta_flag(sta, WLAN_STA_TDLS_PEER_AUTH))
					sta->status_stats.last_tdls_pkt_time =
						jiffies;
			} else {
				ieee80211_lost_packet(sta, info);
			}
		}

#ifdef CPTCFG_MAC80211_LATENCY_MEASUREMENTS
		/* Measure Tx latency & Tx consecutive loss statistics */
		ieee80211_collect_tx_timing_stats(local, skb, sta, hdr,
						  prev_loss_pkt, send_fail);
#endif /* CPTCFG_MAC80211_LATENCY_MEASUREMENTS */
	}

	/* SNMP counters
	 * Fragments are passed to low-level drivers as separate skbs, so these
	 * are actually fragments, not frames. Update frame counters only for
	 * the first fragment of the frame. */
	if ((info->flags & IEEE80211_TX_STAT_ACK) ||
	    (info->flags & IEEE80211_TX_STAT_NOACK_TRANSMITTED)) {
		if (ieee80211_is_first_frag(hdr->seq_ctrl)) {
			I802_DEBUG_INC(local->dot11TransmittedFrameCount);
			if (is_multicast_ether_addr(ieee80211_get_DA(hdr)))
				I802_DEBUG_INC(local->dot11MulticastTransmittedFrameCount);
			if (retry_count > 0)
				I802_DEBUG_INC(local->dot11RetryCount);
			if (retry_count > 1)
				I802_DEBUG_INC(local->dot11MultipleRetryCount);
		}

		/* This counter shall be incremented for an acknowledged MPDU
		 * with an individual address in the address 1 field or an MPDU
		 * with a multicast address in the address 1 field of type Data
		 * or Management. */
		if (!is_multicast_ether_addr(hdr->addr1) ||
		    ieee80211_is_data(fc) ||
		    ieee80211_is_mgmt(fc))
			I802_DEBUG_INC(local->dot11TransmittedFragmentCount);
	} else {
		if (ieee80211_is_first_frag(hdr->seq_ctrl))
			I802_DEBUG_INC(local->dot11FailedCount);
	}

	if (ieee80211_is_nullfunc(fc) && ieee80211_has_pm(fc) &&
	    ieee80211_hw_check(&local->hw, REPORTS_TX_ACK_STATUS) &&
	    !(info->flags & IEEE80211_TX_CTL_INJECTED) &&
	    local->ps_sdata && !(local->scanning)) {
		if (info->flags & IEEE80211_TX_STAT_ACK) {
			local->ps_sdata->u.mgd.flags |=
					IEEE80211_STA_NULLFUNC_ACKED;
		} else
			mod_timer(&local->dynamic_ps_timer, jiffies +
					msecs_to_jiffies(10));
	}

	ieee80211_report_used_skb(local, skb, false);

	/* this was a transmitted frame, but now we want to reuse it */
	skb_orphan(skb);

	/* Need to make a copy before skb->cb gets cleared */
	send_to_cooked = !!(info->flags & IEEE80211_TX_CTL_INJECTED) ||
			 !(ieee80211_is_data(fc));

	/*
	 * This is a bit racy but we can avoid a lot of work
	 * with this test...
	 */
	if (!local->monitors && (!send_to_cooked || !local->cooked_mntrs)) {
		dev_kfree_skb(skb);
		return;
	}

	/* send to monitor interfaces */
	ieee80211_tx_monitor(local, skb, sband, retry_count, shift, send_to_cooked);
}

void ieee80211_tx_status(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_status status = {
		.skb = skb,
		.info = IEEE80211_SKB_CB(skb),
	};
	struct rhlist_head *tmp;
	struct sta_info *sta;

	rcu_read_lock();

	for_each_sta_info(local, hdr->addr1, sta, tmp) {
		/* skip wrong virtual interface */
		if (!ether_addr_equal(hdr->addr2, sta->sdata->vif.addr))
			continue;

		status.sta = &sta->sta;
		break;
	}

	__ieee80211_tx_status(hw, &status);
	rcu_read_unlock();
}
EXPORT_SYMBOL(ieee80211_tx_status);

void ieee80211_tx_status_ext(struct ieee80211_hw *hw,
			     struct ieee80211_tx_status *status)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_info *info = status->info;
	struct ieee80211_sta *pubsta = status->sta;
	struct ieee80211_supported_band *sband;
	int retry_count;
	bool acked, noack_success;

	if (status->skb)
		return __ieee80211_tx_status(hw, status);

	if (!status->sta)
		return;

	ieee80211_tx_get_rates(hw, info, &retry_count);

	sband = hw->wiphy->bands[info->band];

	acked = !!(info->flags & IEEE80211_TX_STAT_ACK);
	noack_success = !!(info->flags & IEEE80211_TX_STAT_NOACK_TRANSMITTED);

	if (pubsta) {
		struct sta_info *sta;

		sta = container_of(pubsta, struct sta_info, sta);

		if (!acked)
			sta->status_stats.retry_failed++;
		sta->status_stats.retry_count += retry_count;

		if (acked) {
			sta->status_stats.last_ack = jiffies;

			if (sta->status_stats.lost_packets)
				sta->status_stats.lost_packets = 0;

			/* Track when last TDLS packet was ACKed */
			if (test_sta_flag(sta, WLAN_STA_TDLS_PEER_AUTH))
				sta->status_stats.last_tdls_pkt_time = jiffies;
		} else if (test_sta_flag(sta, WLAN_STA_PS_STA)) {
			return;
		} else {
			ieee80211_lost_packet(sta, info);
		}

		rate_control_tx_status(local, sband, status);
		if (ieee80211_vif_is_mesh(&sta->sdata->vif))
			ieee80211s_update_metric(local, sta, status);
	}

	if (acked || noack_success) {
		I802_DEBUG_INC(local->dot11TransmittedFrameCount);
		if (!pubsta)
			I802_DEBUG_INC(local->dot11MulticastTransmittedFrameCount);
		if (retry_count > 0)
			I802_DEBUG_INC(local->dot11RetryCount);
		if (retry_count > 1)
			I802_DEBUG_INC(local->dot11MultipleRetryCount);
	} else {
		I802_DEBUG_INC(local->dot11FailedCount);
	}
}
EXPORT_SYMBOL(ieee80211_tx_status_ext);

void ieee80211_tx_rate_update(struct ieee80211_hw *hw,
			      struct ieee80211_sta *pubsta,
			      struct ieee80211_tx_info *info)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_supported_band *sband = hw->wiphy->bands[info->band];
	struct sta_info *sta = container_of(pubsta, struct sta_info, sta);
	struct ieee80211_tx_status status = {
		.info = info,
		.sta = pubsta,
	};

	rate_control_tx_status(local, sband, &status);

	if (ieee80211_hw_check(&local->hw, HAS_RATE_CONTROL))
		sta->tx_stats.last_rate = info->status.rates[0];
}
EXPORT_SYMBOL(ieee80211_tx_rate_update);

void ieee80211_report_low_ack(struct ieee80211_sta *pubsta, u32 num_packets)
{
	struct sta_info *sta = container_of(pubsta, struct sta_info, sta);
	cfg80211_cqm_pktloss_notify(sta->sdata->dev, sta->sta.addr,
				    num_packets, GFP_ATOMIC);
}
EXPORT_SYMBOL(ieee80211_report_low_ack);

void ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ieee80211_local *local = hw_to_local(hw);

	ieee80211_report_used_skb(local, skb, true);
	dev_kfree_skb_any(skb);
}
EXPORT_SYMBOL(ieee80211_free_txskb);

void ieee80211_purge_tx_queue(struct ieee80211_hw *hw,
			      struct sk_buff_head *skbs)
{
	struct sk_buff *skb;

	while ((skb = __skb_dequeue(skbs)))
		ieee80211_free_txskb(hw, skb);
}
