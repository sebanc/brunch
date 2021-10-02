// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2018-2020 Intel Corporation
 */
#include <linux/module.h>
#include <linux/types.h>

#include "xvt.h"
#include "fw/api/rx.h"
#include "fw/dbg.h"

/*
 * Returns true if sn2 - buffer_size < sn1 < sn2.
 * To be used only in order to compare reorder buffer head with NSSN.
 * We fully trust NSSN unless it is behind us due to reorder timeout.
 * Reorder timeout can only bring us up to buffer_size SNs ahead of NSSN.
 */
static bool iwl_xvt_is_sn_less(u16 sn1, u16 sn2, u16 buffer_size)
{
	return ieee80211_sn_less(sn1, sn2) &&
	       !ieee80211_sn_less(sn1, sn2 - buffer_size);
}

static void iwl_xvt_release_frames(struct iwl_xvt *xvt,
				   struct iwl_xvt_reorder_buffer *reorder_buf,
				   u16 nssn)
{
	u16 ssn = reorder_buf->head_sn;

	lockdep_assert_held(&reorder_buf->lock);
	IWL_DEBUG_HT(xvt, "reorder: release nssn=%d\n", nssn);

	/* ignore nssn smaller than head sn - this can happen due to timeout */
	if (iwl_xvt_is_sn_less(nssn, ssn, reorder_buf->buf_size))
		return;

	while (iwl_xvt_is_sn_less(ssn, nssn, reorder_buf->buf_size)) {
		int index = ssn % reorder_buf->buf_size;
		u16 frames_count = reorder_buf->entries[index];

		ssn = ieee80211_sn_inc(ssn);

		/*
		 * Reset frame count. Will have more than one frame for A-MSDU.
		 * entries=0 is valid as well since nssn indicates frames were
		 * received.
		 */
		IWL_DEBUG_HT(xvt, "reorder: deliver index=0x%x\n", index);

		reorder_buf->entries[index] = 0;
		reorder_buf->num_stored -= frames_count;
		reorder_buf->stats.released += frames_count;
	}
	reorder_buf->head_sn = nssn;

	/* don't mess with reorder timer for now */
}

void iwl_xvt_rx_frame_release(struct iwl_xvt *xvt, struct iwl_rx_packet *pkt)
{
	struct iwl_frame_release *release = (void *)pkt->data;
	struct iwl_xvt_reorder_buffer *buffer;
	int baid = release->baid;

	IWL_DEBUG_HT(xvt, "Frame release notification for BAID %u, NSSN %d\n",
		     baid, le16_to_cpu(release->nssn));

	if (WARN_ON_ONCE(baid >= IWL_MAX_BAID))
		return;

	buffer = &xvt->reorder_bufs[baid];
	if (buffer->sta_id == IWL_XVT_INVALID_STA)
		return;

	spin_lock_bh(&buffer->lock);
	iwl_xvt_release_frames(xvt, buffer, le16_to_cpu(release->nssn));
	spin_unlock_bh(&buffer->lock);
}

void iwl_xvt_destroy_reorder_buffer(struct iwl_xvt *xvt,
				    struct iwl_xvt_reorder_buffer *buf)
{
	if (buf->sta_id == IWL_XVT_INVALID_STA)
		return;

	spin_lock_bh(&buf->lock);
	iwl_xvt_release_frames(xvt, buf,
			       ieee80211_sn_add(buf->head_sn, buf->buf_size));
	buf->sta_id = IWL_XVT_INVALID_STA;
	spin_unlock_bh(&buf->lock);
}

static bool iwl_xvt_init_reorder_buffer(struct iwl_xvt_reorder_buffer *buf,
					u8 sta_id, u8 tid,
					u16 ssn, u8 buf_size)
{
	int j;

	if (WARN_ON(buf_size > ARRAY_SIZE(buf->entries)))
		return false;

	buf->num_stored = 0;
	buf->head_sn = ssn;
	buf->buf_size = buf_size;
	spin_lock_init(&buf->lock);
	buf->queue = 0;
	buf->sta_id = sta_id;
	buf->tid = tid;
	for (j = 0; j < buf->buf_size; j++)
		buf->entries[j] = 0;
	memset(&buf->stats, 0, sizeof(buf->stats));

	/* currently there's no need to mess with reorder timer */
	return true;
}

bool iwl_xvt_reorder(struct iwl_xvt *xvt, struct iwl_rx_packet *pkt)
{
	struct iwl_rx_mpdu_desc *desc = (void *)pkt->data;
	struct ieee80211_hdr *hdr;
	u32 reorder = le32_to_cpu(desc->reorder_data);
	struct iwl_xvt_reorder_buffer *buffer;
	u16 tail;
	bool last_subframe = desc->amsdu_info & IWL_RX_MPDU_AMSDU_LAST_SUBFRAME;
	u8 sub_frame_idx = desc->amsdu_info &
			   IWL_RX_MPDU_AMSDU_SUBFRAME_IDX_MASK;
	bool amsdu = desc->mac_flags2 & IWL_RX_MPDU_MFLG2_AMSDU;
	u16 nssn, sn, min_sn;
	int index;
	u8 baid;
	u8 sta_id = le32_get_bits(desc->status, IWL_RX_MPDU_STATUS_STA_ID);
	u8 tid;

	baid = (reorder & IWL_RX_MPDU_REORDER_BAID_MASK) >>
		IWL_RX_MPDU_REORDER_BAID_SHIFT;

	if (baid >= IWL_MAX_BAID)
		return false;

	if (xvt->trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210)
		hdr = (void *)(pkt->data + sizeof(struct iwl_rx_mpdu_desc));
	else
		hdr = (void *)(pkt->data + IWL_RX_DESC_SIZE_V1);

	/* not a data packet */
	if (!ieee80211_is_data_qos(hdr->frame_control) ||
	    is_multicast_ether_addr(hdr->addr1))
		return false;

	if (unlikely(!ieee80211_is_data_present(hdr->frame_control)))
		return false;

	nssn = reorder & IWL_RX_MPDU_REORDER_NSSN_MASK;
	sn = (reorder & IWL_RX_MPDU_REORDER_SN_MASK) >>
		IWL_RX_MPDU_REORDER_SN_SHIFT;
	min_sn = ieee80211_sn_less(sn, nssn) ? sn : nssn;

	tid = *ieee80211_get_qos_ctl(hdr) & IEEE80211_QOS_CTL_TID_MASK;

	/* Check if buffer needs to be initialized */
	buffer = &xvt->reorder_bufs[baid];
	if (buffer->sta_id == IWL_XVT_INVALID_STA) {
		/* don't initialize until first valid packet comes through */
		if (reorder & IWL_RX_MPDU_REORDER_BA_OLD_SN)
			return false;
		if (!iwl_xvt_init_reorder_buffer(buffer, sta_id, tid, min_sn,
						 IEEE80211_MAX_AMPDU_BUF_HT))
			return false;
	}

	/* verify sta_id and tid match the reorder buffer params */
	if (buffer->sta_id != sta_id || buffer->tid != tid) {
		/* TODO: add add_ba/del_ba notifications */
		WARN(1, "sta_id/tid doesn't match saved baid params\n");
		return false;
	}

	spin_lock_bh(&buffer->lock);

	/*
	 * If there was a significant jump in the nssn - adjust.
	 * If the SN is smaller than the NSSN it might need to first go into
	 * the reorder buffer, in which case we just release up to it and the
	 * rest of the function will take care of storing it and releasing up to
	 * the nssn
	 */
	if (!iwl_xvt_is_sn_less(nssn, buffer->head_sn + buffer->buf_size,
				buffer->buf_size) ||
	    !ieee80211_sn_less(sn, buffer->head_sn + buffer->buf_size))
		iwl_xvt_release_frames(xvt, buffer, min_sn);

	/* drop any outdated packets */
	if (ieee80211_sn_less(sn, buffer->head_sn))
		goto drop;

	/* release immediately if allowed by nssn and no stored frames */
	if (!buffer->num_stored && ieee80211_sn_less(sn, nssn)) {
		if (iwl_xvt_is_sn_less(buffer->head_sn, nssn,
				       buffer->buf_size) &&
		   (!amsdu || last_subframe))
			buffer->head_sn = nssn;
		/* No need to update AMSDU last SN - we are moving the head */
		spin_unlock_bh(&buffer->lock);
		buffer->stats.released++;
		buffer->stats.skipped++;
		return false;
	}

	index = sn % buffer->buf_size;

	/*
	 * Check if we already stored this frame
	 * As AMSDU is either received or not as whole, logic is simple:
	 * If we have frames in that position in the buffer and the last frame
	 * originated from AMSDU had a different SN then it is a retransmission.
	 * If it is the same SN then if the subframe index is incrementing it
	 * is the same AMSDU - otherwise it is a retransmission.
	 */
	tail = buffer->entries[index];
	if (tail && !amsdu)
		goto drop;
	else if (tail && (sn != buffer->last_amsdu ||
			  buffer->last_sub_index >= sub_frame_idx))
		goto drop;

	/* put in reorder buffer */
	buffer->entries[index]++;
	buffer->num_stored++;

	if (amsdu) {
		buffer->last_amsdu = sn;
		buffer->last_sub_index = sub_frame_idx;
	}
	buffer->stats.reordered++;

	/*
	 * We cannot trust NSSN for AMSDU sub-frames that are not the last.
	 * The reason is that NSSN advances on the first sub-frame, and may
	 * cause the reorder buffer to advance before all the sub-frames arrive.
	 * Example: reorder buffer contains SN 0 & 2, and we receive AMSDU with
	 * SN 1. NSSN for first sub frame will be 3 with the result of driver
	 * releasing SN 0,1, 2. When sub-frame 1 arrives - reorder buffer is
	 * already ahead and it will be dropped.
	 * If the last sub-frame is not on this queue - we will get frame
	 * release notification with up to date NSSN.
	 */
	if (!amsdu || last_subframe)
		iwl_xvt_release_frames(xvt, buffer, nssn);

	spin_unlock_bh(&buffer->lock);

	return true;

drop:
	buffer->stats.dropped++;
	spin_unlock_bh(&buffer->lock);
	return true;
}
