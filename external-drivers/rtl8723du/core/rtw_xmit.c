// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_XMIT_C_

#include <drv_types.h>
#include <hal_data.h>

static u8 P802_1H_OUI[P80211_OUI_LEN] = { 0x00, 0x00, 0xf8 };
static u8 RFC1042_OUI[P80211_OUI_LEN] = { 0x00, 0x00, 0x00 };

static void _init_txservq(struct tx_servq *ptxservq)
{
	INIT_LIST_HEAD(&ptxservq->tx_pending);
	_rtw_init_queue(&ptxservq->sta_pending);
	ptxservq->qcnt = 0;
}


void	_rtw_init_sta_xmit_priv(struct sta_xmit_priv *psta_xmitpriv)
{


	memset((unsigned char *)psta_xmitpriv, 0, sizeof(struct sta_xmit_priv));

	spin_lock_init(&psta_xmitpriv->lock);

	/* for(i = 0 ; i < MAX_NUMBLKS; i++) */
	/*	_init_txservq(&(psta_xmitpriv->blk_q[i])); */

	_init_txservq(&psta_xmitpriv->be_q);
	_init_txservq(&psta_xmitpriv->bk_q);
	_init_txservq(&psta_xmitpriv->vi_q);
	_init_txservq(&psta_xmitpriv->vo_q);
	INIT_LIST_HEAD(&psta_xmitpriv->legacy_dz);
	INIT_LIST_HEAD(&psta_xmitpriv->apsd);


}

void rtw_init_xmit_block(struct adapter *adapt)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);

	spin_lock_init(&dvobj->xmit_block_lock);
	dvobj->xmit_block = XMIT_BLOCK_NONE;

}

static void rtw_free_xmit_block(struct adapter *adapt)
{
}

int	_rtw_init_xmit_priv(struct xmit_priv *pxmitpriv, struct adapter *adapt)
{
	int i;
	struct xmit_buf *pxmitbuf;
	struct xmit_frame *pxframe;
	int	res = _SUCCESS;


	/* We don't need to memset adapt->XXX to zero, because adapter is allocated by vzalloc(). */
	/* memset((unsigned char *)pxmitpriv, 0, sizeof(struct xmit_priv)); */

	spin_lock_init(&pxmitpriv->lock);
	spin_lock_init(&pxmitpriv->lock_sctx);
	sema_init(&pxmitpriv->xmit_sema, 0);

	/*
	Please insert all the queue initializaiton using _rtw_init_queue below
	*/

	pxmitpriv->adapter = adapt;

	/* for(i = 0 ; i < MAX_NUMBLKS; i++) */
	/*	_rtw_init_queue(&pxmitpriv->blk_strms[i]); */

	_rtw_init_queue(&pxmitpriv->be_pending);
	_rtw_init_queue(&pxmitpriv->bk_pending);
	_rtw_init_queue(&pxmitpriv->vi_pending);
	_rtw_init_queue(&pxmitpriv->vo_pending);
	_rtw_init_queue(&pxmitpriv->bm_pending);

	/* _rtw_init_queue(&pxmitpriv->legacy_dz_queue); */
	/* _rtw_init_queue(&pxmitpriv->apsd_queue); */

	_rtw_init_queue(&pxmitpriv->free_xmit_queue);

	/*
	Please allocate memory with the sz = (struct xmit_frame) * NR_XMITFRAME,
	and initialize free_xmit_frame below.
	Please also apply  free_txobj to link_up all the xmit_frames...
	*/

	pxmitpriv->pallocated_frame_buf = vzalloc(NR_XMITFRAME * sizeof(struct xmit_frame) + 4);

	if (!pxmitpriv->pallocated_frame_buf ) {
		pxmitpriv->pxmit_frame_buf = NULL;
		res = _FAIL;
		goto exit;
	}
	pxmitpriv->pxmit_frame_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pxmitpriv->pallocated_frame_buf), 4);
	/* pxmitpriv->pxmit_frame_buf = pxmitpriv->pallocated_frame_buf + 4 - */
	/*						((SIZE_PTR) (pxmitpriv->pallocated_frame_buf) &3); */

	pxframe = (struct xmit_frame *) pxmitpriv->pxmit_frame_buf;

	for (i = 0; i < NR_XMITFRAME; i++) {
		INIT_LIST_HEAD(&(pxframe->list));

		pxframe->adapt = adapt;
		pxframe->frame_tag = NULL_FRAMETAG;

		pxframe->pkt = NULL;

		pxframe->buf_addr = NULL;
		pxframe->pxmitbuf = NULL;

		list_add_tail(&(pxframe->list), &(pxmitpriv->free_xmit_queue.queue));

		pxframe++;
	}

	pxmitpriv->free_xmitframe_cnt = NR_XMITFRAME;

	pxmitpriv->frag_len = MAX_FRAG_THRESHOLD;


	/* init xmit_buf */
	_rtw_init_queue(&pxmitpriv->free_xmitbuf_queue);
	_rtw_init_queue(&pxmitpriv->pending_xmitbuf_queue);

	pxmitpriv->pallocated_xmitbuf = vzalloc(NR_XMITBUFF * sizeof(struct xmit_buf) + 4);

	if (!pxmitpriv->pallocated_xmitbuf ) {
		res = _FAIL;
		goto exit;
	}

	pxmitpriv->pxmitbuf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pxmitpriv->pallocated_xmitbuf), 4);
	/* pxmitpriv->pxmitbuf = pxmitpriv->pallocated_xmitbuf + 4 - */
	/*						((SIZE_PTR) (pxmitpriv->pallocated_xmitbuf) &3); */

	pxmitbuf = (struct xmit_buf *)pxmitpriv->pxmitbuf;

	for (i = 0; i < NR_XMITBUFF; i++) {
		INIT_LIST_HEAD(&pxmitbuf->list);

		pxmitbuf->priv_data = NULL;
		pxmitbuf->adapt = adapt;
		pxmitbuf->buf_tag = XMITBUF_DATA;

		/* Tx buf allocation may fail sometimes, so sleep and retry. */
		res = rtw_os_xmit_resource_alloc(adapt, pxmitbuf, (MAX_XMITBUF_SZ + XMITBUF_ALIGN_SZ), true);
		if (res == _FAIL) {
			rtw_msleep_os(10);
			res = rtw_os_xmit_resource_alloc(adapt, pxmitbuf, (MAX_XMITBUF_SZ + XMITBUF_ALIGN_SZ), true);
			if (res == _FAIL)
				goto exit;
		}
		pxmitbuf->flags = XMIT_VO_QUEUE;

		list_add_tail(&pxmitbuf->list, &(pxmitpriv->free_xmitbuf_queue.queue));
		pxmitbuf++;
	}

	pxmitpriv->free_xmitbuf_cnt = NR_XMITBUFF;

	/* init xframe_ext queue,  the same count as extbuf */
	_rtw_init_queue(&pxmitpriv->free_xframe_ext_queue);

	pxmitpriv->xframe_ext_alloc_addr = vzalloc(NR_XMIT_EXTBUFF * sizeof(struct xmit_frame) + 4);

	if (!pxmitpriv->xframe_ext_alloc_addr ) {
		pxmitpriv->xframe_ext = NULL;
		res = _FAIL;
		goto exit;
	}
	pxmitpriv->xframe_ext = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pxmitpriv->xframe_ext_alloc_addr), 4);
	pxframe = (struct xmit_frame *)pxmitpriv->xframe_ext;

	for (i = 0; i < NR_XMIT_EXTBUFF; i++) {
		INIT_LIST_HEAD(&(pxframe->list));

		pxframe->adapt = adapt;
		pxframe->frame_tag = NULL_FRAMETAG;

		pxframe->pkt = NULL;

		pxframe->buf_addr = NULL;
		pxframe->pxmitbuf = NULL;

		pxframe->ext_tag = 1;

		list_add_tail(&(pxframe->list), &(pxmitpriv->free_xframe_ext_queue.queue));

		pxframe++;
	}
	pxmitpriv->free_xframe_ext_cnt = NR_XMIT_EXTBUFF;

	/* Init xmit extension buff */
	_rtw_init_queue(&pxmitpriv->free_xmit_extbuf_queue);

	pxmitpriv->pallocated_xmit_extbuf = vzalloc(NR_XMIT_EXTBUFF * sizeof(struct xmit_buf) + 4);

	if (!pxmitpriv->pallocated_xmit_extbuf ) {
		res = _FAIL;
		goto exit;
	}

	pxmitpriv->pxmit_extbuf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pxmitpriv->pallocated_xmit_extbuf), 4);

	pxmitbuf = (struct xmit_buf *)pxmitpriv->pxmit_extbuf;

	for (i = 0; i < NR_XMIT_EXTBUFF; i++) {
		INIT_LIST_HEAD(&pxmitbuf->list);

		pxmitbuf->priv_data = NULL;
		pxmitbuf->adapt = adapt;
		pxmitbuf->buf_tag = XMITBUF_MGNT;

		res = rtw_os_xmit_resource_alloc(adapt, pxmitbuf, MAX_XMIT_EXTBUF_SZ + XMITBUF_ALIGN_SZ, true);
		if (res == _FAIL) {
			res = _FAIL;
			goto exit;
		}

		list_add_tail(&pxmitbuf->list, &(pxmitpriv->free_xmit_extbuf_queue.queue));
		pxmitbuf++;
	}

	pxmitpriv->free_xmit_extbuf_cnt = NR_XMIT_EXTBUFF;

	for (i = 0; i < CMDBUF_MAX; i++) {
		pxmitbuf = &pxmitpriv->pcmd_xmitbuf[i];
		if (pxmitbuf) {
			INIT_LIST_HEAD(&pxmitbuf->list);

			pxmitbuf->priv_data = NULL;
			pxmitbuf->adapt = adapt;
			pxmitbuf->buf_tag = XMITBUF_CMD;

			res = rtw_os_xmit_resource_alloc(adapt, pxmitbuf, MAX_CMDBUF_SZ + XMITBUF_ALIGN_SZ, true);
			if (res == _FAIL) {
				res = _FAIL;
				goto exit;
			}
			pxmitbuf->alloc_sz = MAX_CMDBUF_SZ + XMITBUF_ALIGN_SZ;
		}
	}

	rtw_alloc_hwxmits(adapt);
	rtw_init_hwxmits(pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);

	for (i = 0; i < 4; i++)
		pxmitpriv->wmm_para_seq[i] = i;

	pxmitpriv->txirp_cnt = 1;

	sema_init(&(pxmitpriv->tx_retevt), 0);

	/* per AC pending irp */
	pxmitpriv->beq_cnt = 0;
	pxmitpriv->bkq_cnt = 0;
	pxmitpriv->viq_cnt = 0;
	pxmitpriv->voq_cnt = 0;

	pxmitpriv->ack_tx = false;
	_rtw_mutex_init(&pxmitpriv->ack_tx_mutex);
	rtw_sctx_init(&pxmitpriv->ack_tx_ops, 0);

#ifdef CONFIG_TX_AMSDU
	rtw_init_timer(&(pxmitpriv->amsdu_vo_timer), adapt,
		rtw_amsdu_vo_timeout_handler, adapt);
	pxmitpriv->amsdu_vo_timeout = RTW_AMSDU_TIMER_UNSET;

	rtw_init_timer(&(pxmitpriv->amsdu_vi_timer), adapt,
		rtw_amsdu_vi_timeout_handler, adapt);
	pxmitpriv->amsdu_vi_timeout = RTW_AMSDU_TIMER_UNSET;

	rtw_init_timer(&(pxmitpriv->amsdu_be_timer), adapt,
		rtw_amsdu_be_timeout_handler, adapt);
	pxmitpriv->amsdu_be_timeout = RTW_AMSDU_TIMER_UNSET;

	rtw_init_timer(&(pxmitpriv->amsdu_bk_timer), adapt,
		rtw_amsdu_bk_timeout_handler, adapt);
	pxmitpriv->amsdu_bk_timeout = RTW_AMSDU_TIMER_UNSET;

	pxmitpriv->amsdu_debug_set_timer = 0;
	pxmitpriv->amsdu_debug_timeout = 0;
	pxmitpriv->amsdu_debug_coalesce_one = 0;
	pxmitpriv->amsdu_debug_coalesce_two = 0;
#endif
	rtw_init_xmit_block(adapt);
	rtw_hal_init_xmit_priv(adapt);

exit:
	return res;
}

void  rtw_mfree_xmit_priv_lock(struct xmit_priv *pxmitpriv);
void  rtw_mfree_xmit_priv_lock(struct xmit_priv *pxmitpriv)
{
}

void _rtw_free_xmit_priv(struct xmit_priv *pxmitpriv)
{
	int i;
	struct adapter *adapt = pxmitpriv->adapter;
	struct xmit_frame	*pxmitframe = (struct xmit_frame *) pxmitpriv->pxmit_frame_buf;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)pxmitpriv->pxmitbuf;


	rtw_hal_free_xmit_priv(adapt);

	rtw_mfree_xmit_priv_lock(pxmitpriv);

	if (!pxmitpriv->pxmit_frame_buf)
		goto out;

	for (i = 0; i < NR_XMITFRAME; i++) {
		rtw_os_xmit_complete(adapt, pxmitframe);

		pxmitframe++;
	}

	for (i = 0; i < NR_XMITBUFF; i++) {
		rtw_os_xmit_resource_free(adapt, pxmitbuf, (MAX_XMITBUF_SZ + XMITBUF_ALIGN_SZ), true);

		pxmitbuf++;
	}

	if (pxmitpriv->pallocated_frame_buf)
		vfree(pxmitpriv->pallocated_frame_buf);

	if (pxmitpriv->pallocated_xmitbuf)
		vfree(pxmitpriv->pallocated_xmitbuf);

	/* free xframe_ext queue,  the same count as extbuf */
	if ((pxmitframe = (struct xmit_frame *)pxmitpriv->xframe_ext)) {
		for (i = 0; i < NR_XMIT_EXTBUFF; i++) {
			rtw_os_xmit_complete(adapt, pxmitframe);
			pxmitframe++;
		}
	}
	if (pxmitpriv->xframe_ext_alloc_addr)
		vfree(pxmitpriv->xframe_ext_alloc_addr);

	pxmitbuf = (struct xmit_buf *)pxmitpriv->pxmit_extbuf;
	for (i = 0; i < NR_XMIT_EXTBUFF; i++) {
		rtw_os_xmit_resource_free(adapt, pxmitbuf, (MAX_XMIT_EXTBUF_SZ + XMITBUF_ALIGN_SZ), true);

		pxmitbuf++;
	}

	if (pxmitpriv->pallocated_xmit_extbuf)
		vfree(pxmitpriv->pallocated_xmit_extbuf);

	for (i = 0; i < CMDBUF_MAX; i++) {
		pxmitbuf = &pxmitpriv->pcmd_xmitbuf[i];
		if (pxmitbuf)
			rtw_os_xmit_resource_free(adapt, pxmitbuf, MAX_CMDBUF_SZ + XMITBUF_ALIGN_SZ , true);
	}

	rtw_free_hwxmits(adapt);

	_rtw_mutex_free(&pxmitpriv->ack_tx_mutex);
	rtw_free_xmit_block(adapt);
out:
	return;
}

u8 rtw_get_tx_bw_mode(struct adapter *adapter, struct sta_info *sta)
{
	u8 bw;

	bw = sta->cmn.bw_mode;
	if (MLME_STATE(adapter) & WIFI_ASOC_STATE) {
		if (adapter->mlmeextpriv.cur_channel <= 14)
			bw = rtw_min(bw, ADAPTER_TX_BW_2G(adapter));
		else
			bw = rtw_min(bw, ADAPTER_TX_BW_5G(adapter));
	}

	return bw;
}

void rtw_get_adapter_tx_rate_bmp_by_bw(struct adapter *adapter, u8 bw, u16 *r_bmp_cck_ofdm, u32 *r_bmp_ht, u32 *r_bmp_vht)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	u8 fix_bw = 0xFF;
	u16 bmp_cck_ofdm = 0;
	u32 bmp_ht = 0;
	u32 bmp_vht = 0;
	int i;

	if (adapter->fix_rate != 0xFF && adapter->fix_bw != 0xFF)
		fix_bw = adapter->fix_bw;

	/* TODO: adapter->fix_rate */

	for (i = 0; i < macid_ctl->num; i++) {
		if (!rtw_macid_is_used(macid_ctl, i))
			continue;
		if (!rtw_macid_is_iface_specific(macid_ctl, i, adapter))
			continue;

		if (bw == CHANNEL_WIDTH_20) /* CCK, OFDM always 20MHz */
			bmp_cck_ofdm |= macid_ctl->rate_bmp0[i] & 0x00000FFF;

		/* bypass mismatch bandwidth for HT, VHT */
		if ((fix_bw != 0xFF && fix_bw != bw) || (fix_bw == 0xFF && macid_ctl->bw[i] != bw))
			continue;

		if (macid_ctl->vht_en[i])
			bmp_vht |= (macid_ctl->rate_bmp0[i] >> 12) | (macid_ctl->rate_bmp1[i] << 20);
		else
			bmp_ht |= (macid_ctl->rate_bmp0[i] >> 12) | (macid_ctl->rate_bmp1[i] << 20);
	}
	if (r_bmp_cck_ofdm)
		*r_bmp_cck_ofdm = bmp_cck_ofdm;
	if (r_bmp_ht)
		*r_bmp_ht = bmp_ht;
	if (r_bmp_vht)
		*r_bmp_vht = bmp_vht;
}

static void rtw_get_shared_macid_tx_rate_bmp_by_bw(struct dvobj_priv *dvobj, u8 bw, u16 *r_bmp_cck_ofdm, u32 *r_bmp_ht, u32 *r_bmp_vht)
{
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	u16 bmp_cck_ofdm = 0;
	u32 bmp_ht = 0;
	u32 bmp_vht = 0;
	int i;

	for (i = 0; i < macid_ctl->num; i++) {
		if (!rtw_macid_is_used(macid_ctl, i))
			continue;
		if (!rtw_macid_is_iface_shared(macid_ctl, i))
			continue;

		if (bw == CHANNEL_WIDTH_20) /* CCK, OFDM always 20MHz */
			bmp_cck_ofdm |= macid_ctl->rate_bmp0[i] & 0x00000FFF;

		/* bypass mismatch bandwidth for HT, VHT */
		if (macid_ctl->bw[i] != bw)
			continue;

		if (macid_ctl->vht_en[i])
			bmp_vht |= (macid_ctl->rate_bmp0[i] >> 12) | (macid_ctl->rate_bmp1[i] << 20);
		else
			bmp_ht |= (macid_ctl->rate_bmp0[i] >> 12) | (macid_ctl->rate_bmp1[i] << 20);
	}

	if (r_bmp_cck_ofdm)
		*r_bmp_cck_ofdm = bmp_cck_ofdm;
	if (r_bmp_ht)
		*r_bmp_ht = bmp_ht;
	if (r_bmp_vht)
		*r_bmp_vht = bmp_vht;
}

void rtw_update_tx_rate_bmp(struct dvobj_priv *dvobj)
{
	struct rf_ctl_t *rf_ctl = dvobj_to_rfctl(dvobj);
	struct adapter *adapter = dvobj_get_primary_adapter(dvobj);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 bw;
	u16 bmp_cck_ofdm, tmp_cck_ofdm;
	u32 bmp_ht, tmp_ht, ori_bmp_ht[2];
	u8 ori_highest_ht_rate_bw_bmp;
	u32 bmp_vht, tmp_vht, ori_bmp_vht[4];
	u8 ori_highest_vht_rate_bw_bmp;
	int i;

	/* backup the original ht & vht highest bw bmp */
	ori_highest_ht_rate_bw_bmp = rf_ctl->highest_ht_rate_bw_bmp;
	ori_highest_vht_rate_bw_bmp = rf_ctl->highest_vht_rate_bw_bmp;

	for (bw = CHANNEL_WIDTH_20; bw <= CHANNEL_WIDTH_160; bw++) {
		/* backup the original ht & vht bmp */
		if (bw <= CHANNEL_WIDTH_40)
			ori_bmp_ht[bw] = rf_ctl->rate_bmp_ht_by_bw[bw];
		if (bw <= CHANNEL_WIDTH_160)
			ori_bmp_vht[bw] = rf_ctl->rate_bmp_vht_by_bw[bw];

		bmp_cck_ofdm = bmp_ht = bmp_vht = 0;
		if (hal_is_bw_support(dvobj_get_primary_adapter(dvobj), bw)) {
			for (i = 0; i < dvobj->iface_nums; i++) {
				if (!dvobj->adapters[i])
					continue;
				rtw_get_adapter_tx_rate_bmp_by_bw(dvobj->adapters[i], bw, &tmp_cck_ofdm, &tmp_ht, &tmp_vht);
				bmp_cck_ofdm |= tmp_cck_ofdm;
				bmp_ht |= tmp_ht;
				bmp_vht |= tmp_vht;
			}
			rtw_get_shared_macid_tx_rate_bmp_by_bw(dvobj, bw, &tmp_cck_ofdm, &tmp_ht, &tmp_vht);
			bmp_cck_ofdm |= tmp_cck_ofdm;
			bmp_ht |= tmp_ht;
			bmp_vht |= tmp_vht;
		}
		if (bw == CHANNEL_WIDTH_20)
			rf_ctl->rate_bmp_cck_ofdm = bmp_cck_ofdm;
		if (bw <= CHANNEL_WIDTH_40)
			rf_ctl->rate_bmp_ht_by_bw[bw] = bmp_ht;
		if (bw <= CHANNEL_WIDTH_160)
			rf_ctl->rate_bmp_vht_by_bw[bw] = bmp_vht;
	}

#ifndef DBG_HIGHEST_RATE_BMP_BW_CHANGE
#define DBG_HIGHEST_RATE_BMP_BW_CHANGE 0
#endif

	{
		u8 highest_rate_bw;
		u8 highest_rate_bw_bmp;
		u8 update_ht_rs = false;
		u8 update_vht_rs = false;

		highest_rate_bw_bmp = BW_CAP_20M;
		highest_rate_bw = CHANNEL_WIDTH_20;
		for (bw = CHANNEL_WIDTH_20; bw <= CHANNEL_WIDTH_40; bw++) {
			if (rf_ctl->rate_bmp_ht_by_bw[highest_rate_bw] < rf_ctl->rate_bmp_ht_by_bw[bw]) {
				highest_rate_bw_bmp = ch_width_to_bw_cap(bw);
				highest_rate_bw = bw;
			} else if (rf_ctl->rate_bmp_ht_by_bw[highest_rate_bw] == rf_ctl->rate_bmp_ht_by_bw[bw])
				highest_rate_bw_bmp |= ch_width_to_bw_cap(bw);
		}
		rf_ctl->highest_ht_rate_bw_bmp = highest_rate_bw_bmp;

		if (ori_highest_ht_rate_bw_bmp != rf_ctl->highest_ht_rate_bw_bmp
			|| largest_bit(ori_bmp_ht[highest_rate_bw]) != largest_bit(rf_ctl->rate_bmp_ht_by_bw[highest_rate_bw])
		) {
			if (DBG_HIGHEST_RATE_BMP_BW_CHANGE) {
				RTW_INFO("highest_ht_rate_bw_bmp:0x%02x=>0x%02x\n", ori_highest_ht_rate_bw_bmp, rf_ctl->highest_ht_rate_bw_bmp);
				RTW_INFO("rate_bmp_ht_by_bw[%u]:0x%08x=>0x%08x\n", highest_rate_bw, ori_bmp_ht[highest_rate_bw], rf_ctl->rate_bmp_ht_by_bw[highest_rate_bw]);
			}
			update_ht_rs = true;
		}

		highest_rate_bw_bmp = BW_CAP_20M;
		highest_rate_bw = CHANNEL_WIDTH_20;
		for (bw = CHANNEL_WIDTH_20; bw <= CHANNEL_WIDTH_160; bw++) {
			if (rf_ctl->rate_bmp_vht_by_bw[highest_rate_bw] < rf_ctl->rate_bmp_vht_by_bw[bw]) {
				highest_rate_bw_bmp = ch_width_to_bw_cap(bw);
				highest_rate_bw = bw;
			} else if (rf_ctl->rate_bmp_vht_by_bw[highest_rate_bw] == rf_ctl->rate_bmp_vht_by_bw[bw])
				highest_rate_bw_bmp |= ch_width_to_bw_cap(bw);
		}
		rf_ctl->highest_vht_rate_bw_bmp = highest_rate_bw_bmp;

		if (ori_highest_vht_rate_bw_bmp != rf_ctl->highest_vht_rate_bw_bmp
			|| largest_bit(ori_bmp_vht[highest_rate_bw]) != largest_bit(rf_ctl->rate_bmp_vht_by_bw[highest_rate_bw])
		) {
			if (DBG_HIGHEST_RATE_BMP_BW_CHANGE) {
				RTW_INFO("highest_vht_rate_bw_bmp:0x%02x=>0x%02x\n", ori_highest_vht_rate_bw_bmp, rf_ctl->highest_vht_rate_bw_bmp);
				RTW_INFO("rate_bmp_vht_by_bw[%u]:0x%08x=>0x%08x\n", highest_rate_bw, ori_bmp_vht[highest_rate_bw], rf_ctl->rate_bmp_vht_by_bw[highest_rate_bw]);
			}
			update_vht_rs = true;
		}

		/* TODO: per rfpath and rate section handling? */
		if (update_ht_rs || update_vht_rs)
			rtw_hal_set_tx_power_level(dvobj_get_primary_adapter(dvobj), hal_data->current_channel);
	}
}

inline u16 rtw_get_tx_rate_bmp_cck_ofdm(struct dvobj_priv *dvobj)
{
	struct rf_ctl_t *rf_ctl = dvobj_to_rfctl(dvobj);

	return rf_ctl->rate_bmp_cck_ofdm;
}

inline u32 rtw_get_tx_rate_bmp_ht_by_bw(struct dvobj_priv *dvobj, u8 bw)
{
	struct rf_ctl_t *rf_ctl = dvobj_to_rfctl(dvobj);

	return rf_ctl->rate_bmp_ht_by_bw[bw];
}

inline u32 rtw_get_tx_rate_bmp_vht_by_bw(struct dvobj_priv *dvobj, u8 bw)
{
	struct rf_ctl_t *rf_ctl = dvobj_to_rfctl(dvobj);

	return rf_ctl->rate_bmp_vht_by_bw[bw];
}

u8 rtw_get_tx_bw_bmp_of_ht_rate(struct dvobj_priv *dvobj, u8 rate, u8 max_bw)
{
	struct rf_ctl_t *rf_ctl = dvobj_to_rfctl(dvobj);
	u8 bw;
	u8 bw_bmp = 0;
	u32 rate_bmp;

	if (!IS_HT_RATE(rate)) {
		rtw_warn_on(1);
		goto exit;
	}

	rate_bmp = 1 << (rate - MGN_MCS0);

	if (max_bw > CHANNEL_WIDTH_40)
		max_bw = CHANNEL_WIDTH_40;

	for (bw = CHANNEL_WIDTH_20; bw <= max_bw; bw++) {
		/* RA may use lower rate for retry */
		if (rf_ctl->rate_bmp_ht_by_bw[bw] >= rate_bmp)
			bw_bmp |= ch_width_to_bw_cap(bw);
	}

exit:
	return bw_bmp;
}

u8 rtw_get_tx_bw_bmp_of_vht_rate(struct dvobj_priv *dvobj, u8 rate, u8 max_bw)
{
	struct rf_ctl_t *rf_ctl = dvobj_to_rfctl(dvobj);
	u8 bw;
	u8 bw_bmp = 0;
	u32 rate_bmp;

	if (!IS_VHT_RATE(rate)) {
		rtw_warn_on(1);
		goto exit;
	}

	rate_bmp = 1 << (rate - MGN_VHT1SS_MCS0);

	if (max_bw > CHANNEL_WIDTH_160)
		max_bw = CHANNEL_WIDTH_160;

	for (bw = CHANNEL_WIDTH_20; bw <= max_bw; bw++) {
		/* RA may use lower rate for retry */
		if (rf_ctl->rate_bmp_vht_by_bw[bw] >= rate_bmp)
			bw_bmp |= ch_width_to_bw_cap(bw);
	}

exit:
	return bw_bmp;
}

u8 query_ra_short_GI(struct sta_info *psta, u8 bw)
{
	u8	sgi = false, sgi_20m = false, sgi_40m = false, sgi_80m = false;

	sgi_20m = psta->htpriv.sgi_20m;
	sgi_40m = psta->htpriv.sgi_40m;

	switch (bw) {
	case CHANNEL_WIDTH_80:
		sgi = sgi_80m;
		break;
	case CHANNEL_WIDTH_40:
		sgi = sgi_40m;
		break;
	case CHANNEL_WIDTH_20:
	default:
		sgi = sgi_20m;
		break;
	}

	return sgi;
}

static void update_attrib_vcs_info(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	u32	sz;
	struct pkt_attrib	*pattrib = &pxmitframe->attrib;
	/* struct sta_info	*psta = pattrib->psta; */
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	/*
		if(pattrib->psta)
		{
			psta = pattrib->psta;
		}
		else
		{
			RTW_INFO("%s, call rtw_get_stainfo()\n", __func__);
			psta=rtw_get_stainfo(&adapt->stapriv ,&pattrib->ra[0] );
		}

		if(psta==NULL)
		{
			RTW_INFO("%s, psta==NUL\n", __func__);
			return;
		}

		if(!(psta->state &_FW_LINKED))
		{
			RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, psta->state);
			return;
		}
	*/

	if (pattrib->nr_frags != 1)
		sz = adapt->xmitpriv.frag_len;
	else /* no frag */
		sz = pattrib->last_txcmdsz;

	/* (1) RTS_Threshold is compared to the MPDU, not MSDU. */
	/* (2) If there are more than one frag in  this MSDU, only the first frag uses protection frame. */
	/*		Other fragments are protected by previous fragment. */
	/*		So we only need to check the length of first fragment. */
	if (pmlmeext->cur_wireless_mode < WIRELESS_11_24N  || adapt->registrypriv.wifi_spec) {
		if (sz > adapt->registrypriv.rts_thresh)
			pattrib->vcs_mode = RTS_CTS;
		else {
			if (pattrib->rtsen)
				pattrib->vcs_mode = RTS_CTS;
			else if (pattrib->cts2self)
				pattrib->vcs_mode = CTS_TO_SELF;
			else
				pattrib->vcs_mode = NONE_VCS;
		}
	} else {
		while (true) {
			/* IOT action */
			if ((pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_ATHEROS) && (pattrib->ampdu_en) &&
			    (adapt->securitypriv.dot11PrivacyAlgrthm == _AES_)) {
				pattrib->vcs_mode = CTS_TO_SELF;
				break;
			}


			/* check ERP protection */
			if (pattrib->rtsen || pattrib->cts2self) {
				if (pattrib->rtsen)
					pattrib->vcs_mode = RTS_CTS;
				else if (pattrib->cts2self)
					pattrib->vcs_mode = CTS_TO_SELF;

				break;
			}

			/* check HT op mode */
			if (pattrib->ht_en) {
				u8 HTOpMode = pmlmeinfo->HT_protection;
				if ((pmlmeext->cur_bwmode && (HTOpMode == 2 || HTOpMode == 3)) ||
				    (!pmlmeext->cur_bwmode && HTOpMode == 3)) {
					pattrib->vcs_mode = RTS_CTS;
					break;
				}
			}

			/* check rts */
			if (sz > adapt->registrypriv.rts_thresh) {
				pattrib->vcs_mode = RTS_CTS;
				break;
			}

			/* to do list: check MIMO power save condition. */

			/* check AMPDU aggregation for TXOP */
			if (pattrib->ampdu_en) {
				pattrib->vcs_mode = RTS_CTS;
				break;
			}

			pattrib->vcs_mode = NONE_VCS;
			break;
		}
	}

	/* for debug : force driver control vrtl_carrier_sense. */
	if (adapt->driver_vcs_en == 1) {
		/* u8 driver_vcs_en; */ /* Enable=1, Disable=0 driver control vrtl_carrier_sense. */
		/* u8 driver_vcs_type; */ /* force 0:disable VCS, 1:RTS-CTS, 2:CTS-to-self when vcs_en=1. */
		pattrib->vcs_mode = adapt->driver_vcs_type;
	}

}

static void update_attrib_phy_info(struct adapter *adapt, struct pkt_attrib *pattrib, struct sta_info *psta)
{
	struct mlme_ext_priv *mlmeext = &adapt->mlmeextpriv;
	u8 bw;

	pattrib->rtsen = psta->rtsen;
	pattrib->cts2self = psta->cts2self;

	pattrib->mdata = 0;
	pattrib->eosp = 0;
	pattrib->triggered = 0;
	pattrib->ampdu_spacing = 0;

	/* qos_en, ht_en, init rate, ,bw, ch_offset, sgi */
	pattrib->qos_en = psta->qos_option;

	pattrib->raid = psta->cmn.ra_info.rate_id;

	bw = rtw_get_tx_bw_mode(adapt, psta);
	pattrib->bwmode = rtw_min(bw, mlmeext->cur_bwmode);
	pattrib->sgi = query_ra_short_GI(psta, pattrib->bwmode);

	pattrib->ldpc = psta->cmn.ldpc_en;
	pattrib->stbc = psta->cmn.stbc_en;

	pattrib->ht_en = psta->htpriv.ht_option;
	pattrib->ch_offset = psta->htpriv.ch_offset;
	pattrib->ampdu_en = false;

	if (adapt->driver_ampdu_spacing != 0xFF) /* driver control AMPDU Density for peer sta's rx */
		pattrib->ampdu_spacing = adapt->driver_ampdu_spacing;
	else
		pattrib->ampdu_spacing = psta->htpriv.rx_ampdu_min_spacing;

	/* check if enable ampdu */
	if (pattrib->ht_en && psta->htpriv.ampdu_enable) {
		if (psta->htpriv.agg_enable_bitmap & BIT(pattrib->priority)) {
			pattrib->ampdu_en = true;
			if (psta->htpriv.tx_amsdu_enable)
				pattrib->amsdu_ampdu_en = true;
			else
				pattrib->amsdu_ampdu_en = false;
		}
	}
	/* if(pattrib->ht_en && psta->htpriv.ampdu_enable) */
	/* { */
	/*	if(psta->htpriv.agg_enable_bitmap & BIT(pattrib->priority)) */
	/*		pattrib->ampdu_en = true; */
	/* }	 */

	pattrib->retry_ctrl = false;
}

static int update_attrib_sec_info(struct adapter *adapt, struct pkt_attrib *pattrib, struct sta_info *psta)
{
	int res = _SUCCESS;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	int bmcast = IS_MCAST(pattrib->ra);

	memset(pattrib->dot118021x_UncstKey.skey,  0, 16);
	memset(pattrib->dot11tkiptxmickey.skey,  0, 16);
	pattrib->mac_id = psta->cmn.mac_id;

	if (psta->ieee8021x_blocked) {

		pattrib->encrypt = 0;

		if ((pattrib->ether_type != 0x888e) && (!check_fwstate(pmlmepriv, WIFI_MP_STATE))) {
			res = _FAIL;
			goto exit;
		}
	} else {
		GET_ENCRY_ALGO(psecuritypriv, psta, pattrib->encrypt, bmcast);

		switch (psecuritypriv->dot11AuthAlgrthm) {
		case dot11AuthAlgrthm_Open:
		case dot11AuthAlgrthm_Shared:
		case dot11AuthAlgrthm_Auto:
			pattrib->key_idx = (u8)psecuritypriv->dot11PrivacyKeyIndex;
			break;
		case dot11AuthAlgrthm_8021X:
			if (bmcast)
				pattrib->key_idx = (u8)psecuritypriv->dot118021XGrpKeyid;
			else
				pattrib->key_idx = 0;
			break;
		default:
			pattrib->key_idx = 0;
			break;
		}

		/* For WPS 1.0 WEP, driver should not encrypt EAPOL Packet for WPS handshake. */
		if (((pattrib->encrypt == _WEP40_) || (pattrib->encrypt == _WEP104_)) && (pattrib->ether_type == 0x888e))
			pattrib->encrypt = _NO_PRIVACY_;

	}
	switch (pattrib->encrypt) {
	case _WEP40_:
	case _WEP104_:
		pattrib->iv_len = 4;
		pattrib->icv_len = 4;
		WEP_IV(pattrib->iv, psta->dot11txpn, pattrib->key_idx);
		break;

	case _TKIP_:
		pattrib->iv_len = 8;
		pattrib->icv_len = 4;

		if (psecuritypriv->busetkipkey == _FAIL) {
			res = _FAIL;
			goto exit;
		}

		if (bmcast)
			TKIP_IV(pattrib->iv, psta->dot11txpn, pattrib->key_idx);
		else
			TKIP_IV(pattrib->iv, psta->dot11txpn, 0);

		memcpy(pattrib->dot11tkiptxmickey.skey, psta->dot11tkiptxmickey.skey, 16);

		break;
	case _AES_:
		pattrib->iv_len = 8;
		pattrib->icv_len = 8;

		if (bmcast)
			AES_IV(pattrib->iv, psta->dot11txpn, pattrib->key_idx);
		else
			AES_IV(pattrib->iv, psta->dot11txpn, 0);
		break;
	default:
		pattrib->iv_len = 0;
		pattrib->icv_len = 0;
		break;
	}

	if (pattrib->encrypt > 0)
		memcpy(pattrib->dot118021x_UncstKey.skey, psta->dot118021x_UncstKey.skey, 16);


	if (pattrib->encrypt &&
	    ((adapt->securitypriv.sw_encrypt) || (!psecuritypriv->hw_decrypted))) {
		pattrib->bswenc = true;
	} else {
		pattrib->bswenc = false;
	}

#if defined(CONFIG_CONCURRENT_MODE)
	pattrib->bmc_camid = adapt->securitypriv.dot118021x_bmc_cam_id;
#endif

	if (pattrib->encrypt && bmcast && _rtw_camctl_chk_flags(adapt, SEC_STATUS_STA_PK_GK_CONFLICT_DIS_BMC_SEARCH))
		pattrib->bswenc = true;

exit:

	return res;
}

u8	qos_acm(u8 acm_mask, u8 priority)
{
	u8	change_priority = priority;

	switch (priority) {
	case 0:
	case 3:
		if (acm_mask & BIT(1))
			change_priority = 1;
		break;
	case 1:
	case 2:
		break;
	case 4:
	case 5:
		if (acm_mask & BIT(2))
			change_priority = 0;
		break;
	case 6:
	case 7:
		if (acm_mask & BIT(3))
			change_priority = 5;
		break;
	default:
		RTW_INFO("qos_acm(): invalid pattrib->priority: %d!!!\n", priority);
		break;
	}

	return change_priority;
}

static void set_qos(struct pkt_file *ppktfile, struct pkt_attrib *pattrib)
{
	struct ethhdr etherhdr;
	struct iphdr ip_hdr;
	int UserPriority = 0;


	_rtw_open_pktfile(ppktfile->pkt, ppktfile);
	_rtw_pktfile_read(ppktfile, (unsigned char *)&etherhdr, ETH_HLEN);

	/* get UserPriority from IP hdr */
	if (pattrib->ether_type == 0x0800) {
		_rtw_pktfile_read(ppktfile, (u8 *)&ip_hdr, sizeof(ip_hdr));
		/*		UserPriority = (ntohs(ip_hdr.tos) >> 5) & 0x3; */
		UserPriority = ip_hdr.tos >> 5;
	}
	/*
		else if (pattrib->ether_type == 0x888e) {


			UserPriority = 7;
		}
	*/
	pattrib->priority = UserPriority;
	pattrib->hdrlen = WLAN_HDR_A3_QOS_LEN;
	pattrib->subtype = WIFI_QOS_DATA_TYPE;
}

/*get non-qos hw_ssn control register,mapping to REG_HW_SEQ0,1,2,3*/
inline u8 rtw_get_hwseq_no(struct adapter *adapt)
{
	u8 hwseq_num = 0;
#ifdef CONFIG_CONCURRENT_MODE
	if (adapt->adapter_type != PRIMARY_ADAPTER)
		hwseq_num = 1;
	/* else */
	/*	hwseq_num = 2; */
#endif /* CONFIG_CONCURRENT_MODE */
	return hwseq_num;
}
static int update_attrib(struct adapter *adapt, struct sk_buff *pkt, struct pkt_attrib *pattrib)
{
	uint i;
	struct pkt_file pktfile;
	struct sta_info *psta = NULL;
	struct ethhdr etherhdr;
	int bmcast;
	struct sta_priv		*pstapriv = &adapt->stapriv;
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct qos_priv		*pqospriv = &pmlmepriv->qospriv;
	struct xmit_priv		*pxmitpriv = &adapt->xmitpriv;
	int res = _SUCCESS;


	DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib);

	_rtw_open_pktfile(pkt, &pktfile);
	i = _rtw_pktfile_read(&pktfile, (u8 *)&etherhdr, ETH_HLEN);

	pattrib->ether_type = ntohs(etherhdr.h_proto);


	memcpy(pattrib->dst, &etherhdr.h_dest, ETH_ALEN);
	memcpy(pattrib->src, &etherhdr.h_source, ETH_ALEN);


	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) {
		memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);
		memcpy(pattrib->ta, adapter_mac_addr(adapt), ETH_ALEN);
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_adhoc);
	} else if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		memcpy(pattrib->ra, get_bssid(pmlmepriv), ETH_ALEN);
		memcpy(pattrib->ta, adapter_mac_addr(adapt), ETH_ALEN);
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_sta);
	} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);
		memcpy(pattrib->ta, get_bssid(pmlmepriv), ETH_ALEN);
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_ap);
	} else
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_unknown);

	bmcast = IS_MCAST(pattrib->ra);
	if (bmcast) {
		psta = rtw_get_bcmc_stainfo(adapt);
		if (!psta) { /* if we cannot get psta => drop the pkt */
			DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_err_sta);
			res = _FAIL;
			goto exit;
		}
	} else {
		psta = rtw_get_stainfo(pstapriv, pattrib->ra);
		if (!psta) { /* if we cannot get psta => drop the pkt */
			DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_err_ucast_sta);
			res = _FAIL;
			goto exit;
		} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE) && !(psta->state & _FW_LINKED)) {
			DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_err_ucast_ap_link);
			res = _FAIL;
			goto exit;
		}
	}

	if (!(psta->state & _FW_LINKED)) {
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_err_link);
		RTW_INFO("%s-"ADPT_FMT" psta("MAC_FMT")->state(0x%x) != _FW_LINKED\n",
			__func__, ADPT_ARG(adapt), MAC_ARG(psta->cmn.mac_addr), psta->state);
		res = _FAIL;
		goto exit;
	}

	pattrib->pktlen = pktfile.pkt_len;

	/* TODO: 802.1Q VLAN header */
	/* TODO: IPV6 */

	if (ETH_P_IP == pattrib->ether_type) {
		u8 ip[20];

		_rtw_pktfile_read(&pktfile, ip, 20);

		if (GET_IPV4_IHL(ip) * 4 > 20)
			_rtw_pktfile_read(&pktfile, NULL, GET_IPV4_IHL(ip) - 20);

		pattrib->icmp_pkt = 0;
		pattrib->dhcp_pkt = 0;

		if (GET_IPV4_PROTOCOL(ip) == 0x01) { /* ICMP */
			pattrib->icmp_pkt = 1;
			DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_icmp);

		} else if (GET_IPV4_PROTOCOL(ip) == 0x11) { /* UDP */
			u8 udp[8];

			_rtw_pktfile_read(&pktfile, udp, 8);

			if ((GET_UDP_SRC(udp) == 68 && GET_UDP_DST(udp) == 67)
				|| (GET_UDP_SRC(udp) == 67 && GET_UDP_DST(udp) == 68)
			) {
				/* 67 : UDP BOOTP server, 68 : UDP BOOTP client */
				if (pattrib->pktlen > 282) { /* MINIMUM_DHCP_PACKET_SIZE */
					pattrib->dhcp_pkt = 1;
					DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_dhcp);
				}
			}

		} else if (GET_IPV4_PROTOCOL(ip) == 0x06 /* TCP */
			&& rtw_st_ctl_chk_reg_s_proto(&psta->st_ctl, 0x06)
		) {
			u8 tcp[20];

			_rtw_pktfile_read(&pktfile, tcp, 20);

			if (rtw_st_ctl_chk_reg_rule(&psta->st_ctl, adapt, IPV4_SRC(ip), TCP_SRC(tcp), IPV4_DST(ip), TCP_DST(tcp))) {
				if (GET_TCP_SYN(tcp) && GET_TCP_ACK(tcp)) {
					session_tracker_add_cmd(adapt, psta
						, IPV4_SRC(ip), TCP_SRC(tcp)
						, IPV4_SRC(ip), TCP_DST(tcp));
					if (DBG_SESSION_TRACKER)
						RTW_INFO(FUNC_ADPT_FMT" local:"IP_FMT":"PORT_FMT", remote:"IP_FMT":"PORT_FMT" SYN-ACK\n"
							, FUNC_ADPT_ARG(adapt)
							, IP_ARG(IPV4_SRC(ip)), PORT_ARG(TCP_SRC(tcp))
							, IP_ARG(IPV4_DST(ip)), PORT_ARG(TCP_DST(tcp)));
				}
				if (GET_TCP_FIN(tcp)) {
					session_tracker_del_cmd(adapt, psta
						, IPV4_SRC(ip), TCP_SRC(tcp)
						, IPV4_SRC(ip), TCP_DST(tcp));
					if (DBG_SESSION_TRACKER)
						RTW_INFO(FUNC_ADPT_FMT" local:"IP_FMT":"PORT_FMT", remote:"IP_FMT":"PORT_FMT" FIN\n"
							, FUNC_ADPT_ARG(adapt)
							, IP_ARG(IPV4_SRC(ip)), PORT_ARG(TCP_SRC(tcp))
							, IP_ARG(IPV4_DST(ip)), PORT_ARG(TCP_DST(tcp)));
				}
			}
		}

	} else if (0x888e == pattrib->ether_type)
		RTW_PRINT("send eapol packet\n");

	if ((pattrib->ether_type == 0x888e) || (pattrib->dhcp_pkt == 1))
		rtw_mi_set_scan_deny(adapt, 3000);

	/* If EAPOL , ARP , OR DHCP packet, driver must be in active mode. */
	if (pattrib->icmp_pkt == 1)
		rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_LEAVE, 1);
	else if (pattrib->dhcp_pkt == 1)
	{
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_active);
		rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_SPECIAL_PACKET, 1);
	}

	/* TODO:_lock */
	if (update_attrib_sec_info(adapt, pattrib, psta) == _FAIL) {
		DBG_COUNTER(adapt->tx_logs.core_tx_upd_attrib_err_sec);
		res = _FAIL;
		goto exit;
	}

	update_attrib_phy_info(adapt, pattrib, psta);

	pattrib->psta = psta;
	/* TODO:_unlock */

	pattrib->pctrl = 0;

	pattrib->ack_policy = 0;
	/* get ether_hdr_len */
	pattrib->pkt_hdrlen = ETH_HLEN;/* (pattrib->ether_type == 0x8100) ? (14 + 4 ): 14; */ /* vlan tag */

	pattrib->hdrlen = WLAN_HDR_A3_LEN;
	pattrib->subtype = WIFI_DATA_TYPE;
	pattrib->priority = 0;

	if (bmcast)
		pattrib->rate = psta->init_rate;

	if (check_fwstate(pmlmepriv, WIFI_AP_STATE | WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE)) {
		if (pattrib->qos_en)
			set_qos(&pktfile, pattrib);
	} else {
		if (pqospriv->qos_option) {
			set_qos(&pktfile, pattrib);

			if (pmlmepriv->acm_mask != 0)
				pattrib->priority = qos_acm(pmlmepriv->acm_mask, pattrib->priority);
		}
	}
	pattrib->hw_ssn_sel = pxmitpriv->hw_ssn_seq_no;
	rtw_set_tx_chksum_offload(pkt, pattrib);

exit:
	return res;
}

static int xmitframe_addmic(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	int			curfragnum, length;
	u8	*pframe, *payload, mic[8];
	struct	mic_data		micdata;
	/* struct	sta_info		*stainfo; */
	struct	pkt_attrib	*pattrib = &pxmitframe->attrib;
	struct	security_priv	*psecuritypriv = &adapt->securitypriv;
	struct	xmit_priv		*pxmitpriv = &adapt->xmitpriv;
	u8 priority[4] = {0x0, 0x0, 0x0, 0x0};
	u8 hw_hdr_offset = 0;
	int bmcst = IS_MCAST(pattrib->ra);

	/*
		if(pattrib->psta)
		{
			stainfo = pattrib->psta;
		}
		else
		{
			RTW_INFO("%s, call rtw_get_stainfo()\n", __func__);
			stainfo=rtw_get_stainfo(&adapt->stapriv ,&pattrib->ra[0]);
		}

		if(stainfo==NULL)
		{
			RTW_INFO("%s, psta==NUL\n", __func__);
			return _FAIL;
		}

		if(!(stainfo->state &_FW_LINKED))
		{
			RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, stainfo->state);
			return _FAIL;
		}
	*/


	hw_hdr_offset = TXDESC_OFFSET;

	if (pattrib->encrypt == _TKIP_) { /* if(psecuritypriv->dot11PrivacyAlgrthm==_TKIP_PRIVACY_) */
		/* encode mic code */
		{
			u8 null_key[16] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

			pframe = pxmitframe->buf_addr + hw_hdr_offset;

			if (bmcst) {
				if (!memcmp(psecuritypriv->dot118021XGrptxmickey[psecuritypriv->dot118021XGrpKeyid].skey, null_key, 16)) {
					return _FAIL;
				}
				/* start to calculate the mic code */
				rtw_secmicsetkey(&micdata, psecuritypriv->dot118021XGrptxmickey[psecuritypriv->dot118021XGrpKeyid].skey);
			} else {
				if (!memcmp(&pattrib->dot11tkiptxmickey.skey[0], null_key, 16)) {
					return _FAIL;
				}
				/* start to calculate the mic code */
				rtw_secmicsetkey(&micdata, &pattrib->dot11tkiptxmickey.skey[0]);
			}

			if (pframe[1] & 1) { /* ToDS==1 */
				rtw_secmicappend(&micdata, &pframe[16], 6);  /* DA */
				if (pframe[1] & 2) /* From Ds==1 */
					rtw_secmicappend(&micdata, &pframe[24], 6);
				else
					rtw_secmicappend(&micdata, &pframe[10], 6);
			} else {	/* ToDS==0 */
				rtw_secmicappend(&micdata, &pframe[4], 6);   /* DA */
				if (pframe[1] & 2) /* From Ds==1 */
					rtw_secmicappend(&micdata, &pframe[16], 6);
				else
					rtw_secmicappend(&micdata, &pframe[10], 6);

			}

			/* if(pqospriv->qos_option==1) */
			if (pattrib->qos_en)
				priority[0] = (u8)pxmitframe->attrib.priority;


			rtw_secmicappend(&micdata, &priority[0], 4);

			payload = pframe;

			for (curfragnum = 0; curfragnum < pattrib->nr_frags; curfragnum++) {
				payload = (u8 *)RND4((SIZE_PTR)(payload));

				payload = payload + pattrib->hdrlen + pattrib->iv_len;
				if ((curfragnum + 1) == pattrib->nr_frags) {
					length = pattrib->last_txcmdsz - pattrib->hdrlen - pattrib->iv_len - ((pattrib->bswenc) ? pattrib->icv_len : 0);
					rtw_secmicappend(&micdata, payload, length);
					payload = payload + length;
				} else {
					length = pxmitpriv->frag_len - pattrib->hdrlen - pattrib->iv_len - ((pattrib->bswenc) ? pattrib->icv_len : 0);
					rtw_secmicappend(&micdata, payload, length);
					payload = payload + length + pattrib->icv_len;
				}
			}
			rtw_secgetmic(&micdata, &(mic[0]));
			/* add mic code  and add the mic code length in last_txcmdsz */

			memcpy(payload, &(mic[0]), 8);
			pattrib->last_txcmdsz += 8;

			payload = payload - pattrib->last_txcmdsz + 8;
		}
	}


	return _SUCCESS;
}

static int xmitframe_swencrypt(struct adapter *adapt, struct xmit_frame *pxmitframe)
{

	struct	pkt_attrib	*pattrib = &pxmitframe->attrib;
	/* struct 	security_priv	*psecuritypriv=&adapt->securitypriv; */


	/* if((psecuritypriv->sw_encrypt)||(pattrib->bswenc))	 */
	if (pattrib->bswenc) {
		switch (pattrib->encrypt) {
		case _WEP40_:
		case _WEP104_:
			rtw_wep_encrypt(adapt, (u8 *)pxmitframe);
			break;
		case _TKIP_:
			rtw_tkip_encrypt(adapt, (u8 *)pxmitframe);
			break;
		case _AES_:
			rtw_aes_encrypt(adapt, (u8 *)pxmitframe);
			break;
		default:
			break;
		}

	}


	return _SUCCESS;
}

int rtw_make_wlanhdr(struct adapter *adapt , u8 *hdr, struct pkt_attrib *pattrib)
{
	u16 *qc;

	struct rtw_ieee80211_hdr *pwlanhdr = (struct rtw_ieee80211_hdr *)hdr;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct qos_priv *pqospriv = &pmlmepriv->qospriv;
	u8 qos_option = false;
	int res = _SUCCESS;
	__le16 *fctrl = &pwlanhdr->frame_ctl;

	memset(hdr, 0, WLANHDR_OFFSET);

	set_frame_sub_type(fctrl, pattrib->subtype);

	if (pattrib->subtype & WIFI_DATA_TYPE) {
		if ((check_fwstate(pmlmepriv,  WIFI_STATION_STATE))) {
			/* to_ds = 1, fr_ds = 0; */
			/* 1.Data transfer to AP */
			/* 2.Arp pkt will relayed by AP */
			SetToDs(fctrl);
			memcpy(pwlanhdr->addr1, get_bssid(pmlmepriv), ETH_ALEN);
			memcpy(pwlanhdr->addr2, pattrib->ta, ETH_ALEN);
			memcpy(pwlanhdr->addr3, pattrib->dst, ETH_ALEN);

			if (pqospriv->qos_option)
				qos_option = true;
		} else if ((check_fwstate(pmlmepriv,  WIFI_AP_STATE))) {
			/* to_ds = 0, fr_ds = 1; */
			SetFrDs(fctrl);
			memcpy(pwlanhdr->addr1, pattrib->dst, ETH_ALEN);
			memcpy(pwlanhdr->addr2, get_bssid(pmlmepriv), ETH_ALEN);
			memcpy(pwlanhdr->addr3, pattrib->src, ETH_ALEN);

			if (pattrib->qos_en)
				qos_option = true;
		} else if ((check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) ||
			(check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) {
			memcpy(pwlanhdr->addr1, pattrib->dst, ETH_ALEN);
			memcpy(pwlanhdr->addr2, pattrib->ta, ETH_ALEN);
			memcpy(pwlanhdr->addr3, get_bssid(pmlmepriv), ETH_ALEN);

			if (pattrib->qos_en)
				qos_option = true;
		} else {
			res = _FAIL;
			goto exit;
		}

		if (pattrib->mdata)
			SetMData(fctrl);

		if (pattrib->encrypt)
			SetPrivacy(fctrl);

		if (qos_option) {
			qc = (unsigned short *)(hdr + pattrib->hdrlen - 2);

			if (pattrib->priority)
				SetPriority(qc, pattrib->priority);

			SetEOSP(qc, pattrib->eosp);

			SetAckpolicy(qc, pattrib->ack_policy);

			if(pattrib->amsdu)
				SetAMsdu(qc, pattrib->amsdu);
		}

		/* TODO: fill HT Control Field */

		/* Update Seq Num will be handled by f/w */
		{
			struct sta_info *psta;
			psta = rtw_get_stainfo(&adapt->stapriv, pattrib->ra);
			if (pattrib->psta != psta) {
				RTW_INFO("%s, pattrib->psta(%p) != psta(%p)\n", __func__, pattrib->psta, psta);
				return _FAIL;
			}

			if (!psta) {
				RTW_INFO("%s, psta==NUL\n", __func__);
				return _FAIL;
			}

			if (!(psta->state & _FW_LINKED)) {
				RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, psta->state);
				return _FAIL;
			}


			if (psta) {
				psta->sta_xmitpriv.txseq_tid[pattrib->priority]++;
				psta->sta_xmitpriv.txseq_tid[pattrib->priority] &= 0xFFF;
				pattrib->seqnum = psta->sta_xmitpriv.txseq_tid[pattrib->priority];

				SetSeqNum(hdr, pattrib->seqnum);

				/* re-check if enable ampdu by BA_starting_seqctrl */
				if (pattrib->ampdu_en) {
					u16 tx_seq;

					tx_seq = psta->BA_starting_seqctrl[pattrib->priority & 0x0f];

					/* check BA_starting_seqctrl */
					if (SN_LESS(cpu_to_le16(pattrib->seqnum), cpu_to_le16(tx_seq))) {
						/* RTW_INFO("tx ampdu seqnum(%d) < tx_seq(%d)\n", pattrib->seqnum, tx_seq); */
						pattrib->ampdu_en = false;/* AGG BK */
					} else if (SN_EQUAL(pattrib->seqnum, tx_seq)) {
						psta->BA_starting_seqctrl[pattrib->priority & 0x0f] = (tx_seq + 1) & 0xfff;

						pattrib->ampdu_en = true;/* AGG EN */
					} else {
						/* RTW_INFO("tx ampdu over run\n"); */
						psta->BA_starting_seqctrl[pattrib->priority & 0x0f] = (pattrib->seqnum + 1) & 0xfff;
						pattrib->ampdu_en = true;/* AGG EN */
					}

				}
			}
		}
	}

exit:

	return res;
}

int rtw_txframes_pending(struct adapter *adapt)
{
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	return ((!_rtw_queue_empty(&pxmitpriv->be_pending)) ||
		(!_rtw_queue_empty(&pxmitpriv->bk_pending)) ||
		(!_rtw_queue_empty(&pxmitpriv->vi_pending)) ||
		(!_rtw_queue_empty(&pxmitpriv->vo_pending)));
}

int rtw_txframes_sta_ac_pending(struct adapter *adapt, struct pkt_attrib *pattrib)
{
	struct sta_info *psta;
	struct tx_servq *ptxservq;
	int priority = pattrib->priority;
	/*
		if(pattrib->psta)
		{
			psta = pattrib->psta;
		}
		else
		{
			RTW_INFO("%s, call rtw_get_stainfo()\n", __func__);
			psta=rtw_get_stainfo(&adapt->stapriv ,&pattrib->ra[0]);
		}
	*/
	psta = rtw_get_stainfo(&adapt->stapriv, pattrib->ra);
	if (pattrib->psta != psta) {
		RTW_INFO("%s, pattrib->psta(%p) != psta(%p)\n", __func__, pattrib->psta, psta);
		return 0;
	}

	if (!psta) {
		RTW_INFO("%s, psta==NUL\n", __func__);
		return 0;
	}

	if (!(psta->state & _FW_LINKED)) {
		RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, psta->state);
		return 0;
	}

	switch (priority) {
	case 1:
	case 2:
		ptxservq = &(psta->sta_xmitpriv.bk_q);
		break;
	case 4:
	case 5:
		ptxservq = &(psta->sta_xmitpriv.vi_q);
		break;
	case 6:
	case 7:
		ptxservq = &(psta->sta_xmitpriv.vo_q);
		break;
	case 0:
	case 3:
	default:
		ptxservq = &(psta->sta_xmitpriv.be_q);
		break;

	}

	return ptxservq->qcnt;
}

/*
 * Calculate wlan 802.11 packet MAX size from pkt_attrib
 * This function doesn't consider fragment case
 */
u32 rtw_calculate_wlan_pkt_size_by_attribue(struct pkt_attrib *pattrib)
{
	u32	len = 0;

	len = pattrib->hdrlen /* WLAN Header */
		+ pattrib->iv_len /* IV */
		+ SNAP_SIZE + sizeof(u16) /* LLC */
		+ pattrib->pktlen
		+ (pattrib->encrypt == _TKIP_ ? 8 : 0) /* MIC */
		+ (pattrib->bswenc ? pattrib->icv_len : 0) /* ICV */
		;

	return len;
}

#ifdef CONFIG_TX_AMSDU
int check_amsdu(struct xmit_frame *pxmitframe)
{
	struct pkt_attrib *pattrib;
	int ret = true;

	if (!pxmitframe)
		ret = false;

	pattrib = &pxmitframe->attrib;

	if (IS_MCAST(pattrib->ra))
		ret = false;

	if ((pattrib->ether_type == 0x888e) ||
		(pattrib->ether_type == 0x0806) ||
		(pattrib->ether_type == 0x88b4) ||
		(pattrib->dhcp_pkt == 1))
		ret = false;

	if ((pattrib->encrypt == _WEP40_) ||
	    (pattrib->encrypt == _WEP104_) ||
	    (pattrib->encrypt == _TKIP_))
		ret = false;

	if (!pattrib->qos_en)
		ret = false;

	if (IS_AMSDU_AMPDU_NOT_VALID(pattrib))
		ret = false;

	return ret;
}

int check_amsdu_tx_support(struct adapter *adapt)
{
	struct dvobj_priv *pdvobjpriv;
	int tx_amsdu;
	int tx_amsdu_rate;
	int current_tx_rate;
	int ret = false;

	pdvobjpriv = adapter_to_dvobj(adapt);
	tx_amsdu = adapt->tx_amsdu;
	tx_amsdu_rate = adapt->tx_amsdu_rate;
	current_tx_rate = pdvobjpriv->traffic_stat.cur_tx_tp;

	if (tx_amsdu == 1)
		ret = true;
	else if (tx_amsdu == 2 && (tx_amsdu_rate == 0 || current_tx_rate > tx_amsdu_rate))
		ret = true;
	else
		ret = false;

	return ret;
}

int rtw_xmitframe_coalesce_amsdu(struct adapter *adapt, struct xmit_frame *pxmitframe, struct xmit_frame *pxmitframe_queue)
{

	struct pkt_file pktfile;
	struct pkt_attrib *pattrib;
	struct sk_buff *pkt;

	struct pkt_file pktfile_queue;
	struct pkt_attrib *pattrib_queue;
	struct sk_buff *pkt_queue;

	int llc_sz, mem_sz;

	int padding = 0;

	u8 *pframe, *mem_start;
	u8 hw_hdr_offset;

	u16* len;
	u8 *pbuf_start;
	int res = _SUCCESS;

	if (!pxmitframe->buf_addr) {
		RTW_INFO("==> %s buf_addr==NULL\n", __func__);
		return _FAIL;
	}


	pbuf_start = pxmitframe->buf_addr;

	hw_hdr_offset = TXDESC_OFFSET;

	mem_start = pbuf_start + hw_hdr_offset; //for DMA

	pattrib = &pxmitframe->attrib;

	pattrib->amsdu = 1;

	if (rtw_make_wlanhdr(adapt, mem_start, pattrib) == _FAIL) {
		RT_TRACE(_module_rtl871x_xmit_c_, _drv_err_, ("rtw_xmitframe_coalesce: rtw_make_wlanhdr fail; drop pkt\n"));
		RTW_INFO("rtw_xmitframe_coalesce: rtw_make_wlanhdr fail; drop pkt\n");
		res = _FAIL;
		goto exit;
	}

	llc_sz = 0;

	pframe = mem_start;

	//SetMFrag(mem_start);
	ClearMFrag(mem_start);

	pframe += pattrib->hdrlen;

	/* adding icv, if necessary... */
	if (pattrib->iv_len) {
		memcpy(pframe, pattrib->iv, pattrib->iv_len); // queue or new?

		RT_TRACE(_module_rtl871x_xmit_c_, _drv_notice_,
			("rtw_xmitframe_coalesce: keyid=%d pattrib->iv[3]=%.2x pframe=%.2x %.2x %.2x %.2x\n",
			adapt->securitypriv.dot11PrivacyKeyIndex, pattrib->iv[3], *pframe, *(pframe + 1), *(pframe + 2), *(pframe + 3)));

		pframe += pattrib->iv_len;
	}

	pattrib->last_txcmdsz = pattrib->hdrlen + pattrib->iv_len;

	if(pxmitframe_queue) {
		pattrib_queue = &pxmitframe_queue->attrib;
		pkt_queue = pxmitframe_queue->pkt;

		_rtw_open_pktfile(pkt_queue, &pktfile_queue);
		_rtw_pktfile_read(&pktfile_queue, NULL, pattrib_queue->pkt_hdrlen);

		/* 802.3 MAC Header DA(6)  SA(6)  Len(2)*/

		memcpy(pframe, pattrib_queue->dst, ETH_ALEN);
		pframe += ETH_ALEN;

		memcpy(pframe, pattrib_queue->src, ETH_ALEN);
		pframe += ETH_ALEN;

		len = (u16*) pframe;
		pframe += 2;

		llc_sz = rtw_put_snap(pframe, pattrib_queue->ether_type);
		pframe += llc_sz;

		mem_sz = _rtw_pktfile_read(&pktfile_queue, pframe, pattrib_queue->pktlen);
		pframe += mem_sz;

		*len = htons(llc_sz + mem_sz);

		//calc padding
		padding = 4 - ((ETH_HLEN + llc_sz + mem_sz) & (4-1));
		if(padding == 4)
			padding = 0;

		//memset(pframe,0xaa, padding);
		pframe += padding;

		pattrib->last_txcmdsz += ETH_HLEN + llc_sz + mem_sz + padding ;
	}

	//2nd mpdu

	pkt = pxmitframe->pkt;
	_rtw_open_pktfile(pkt, &pktfile);
	_rtw_pktfile_read(&pktfile, NULL, pattrib->pkt_hdrlen);

	/* 802.3 MAC Header  DA(6)  SA(6)  Len(2) */

	memcpy(pframe, pattrib->dst, ETH_ALEN);
	pframe += ETH_ALEN;

	memcpy(pframe, pattrib->src, ETH_ALEN);
	pframe += ETH_ALEN;

	len = (u16*) pframe;
	pframe += 2;

	llc_sz = rtw_put_snap(pframe, pattrib->ether_type);
	pframe += llc_sz;

	mem_sz = _rtw_pktfile_read(&pktfile, pframe, pattrib->pktlen);

	pframe += mem_sz;

	*len = htons(llc_sz + mem_sz);

	//the last ampdu has no padding
	padding = 0;

	pattrib->nr_frags = 1;

	pattrib->last_txcmdsz += ETH_HLEN + llc_sz + mem_sz + padding +
		((pattrib->bswenc) ? pattrib->icv_len : 0) ;

	if ((pattrib->icv_len > 0) && (pattrib->bswenc)) {
		memcpy(pframe, pattrib->icv, pattrib->icv_len);
		pframe += pattrib->icv_len;
	}

	if (xmitframe_addmic(adapt, pxmitframe) == _FAIL) {
		RT_TRACE(_module_rtl871x_xmit_c_, _drv_err_, ("xmitframe_addmic(adapt, pxmitframe)==_FAIL\n"));
		RTW_INFO("xmitframe_addmic(adapt, pxmitframe)==_FAIL\n");
		res = _FAIL;
		goto exit;
	}

	xmitframe_swencrypt(adapt, pxmitframe);

	pattrib->vcs_mode = NONE_VCS;

exit:
	return res;
}
#endif /* CONFIG_TX_AMSDU */

/*

This sub-routine will perform all the following:

1. remove 802.3 header.
2. create wlan_header, based on the info in pxmitframe
3. append sta's iv/ext-iv
4. append LLC
5. move frag chunk from pframe to pxmitframe->mem
6. apply sw-encrypt, if necessary.

*/
int rtw_xmitframe_coalesce(struct adapter *adapt, struct sk_buff *pkt, struct xmit_frame *pxmitframe)
{
	struct pkt_file pktfile;

	int frg_inx, frg_len, mpdu_len, llc_sz, mem_sz;

	SIZE_PTR addr;

	u8 *pframe, *mem_start;
	u8 hw_hdr_offset;

	/* struct sta_info		*psta; */
	/* struct sta_priv		*pstapriv = &adapt->stapriv; */
	/* struct mlme_priv	*pmlmepriv = &adapt->mlmepriv; */
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;

	struct pkt_attrib	*pattrib = &pxmitframe->attrib;

	u8 *pbuf_start;

	int bmcst = IS_MCAST(pattrib->ra);
	int res = _SUCCESS;

	if (!pxmitframe->buf_addr) {
		RTW_INFO("==> %s buf_addr==NULL\n", __func__);
		return _FAIL;
	}

	pbuf_start = pxmitframe->buf_addr;

	hw_hdr_offset = TXDESC_OFFSET;

	mem_start = pbuf_start +	hw_hdr_offset;

	if (rtw_make_wlanhdr(adapt, mem_start, pattrib) == _FAIL) {
		RTW_INFO("rtw_xmitframe_coalesce: rtw_make_wlanhdr fail; drop pkt\n");
		res = _FAIL;
		goto exit;
	}

	_rtw_open_pktfile(pkt, &pktfile);
	_rtw_pktfile_read(&pktfile, NULL, pattrib->pkt_hdrlen);

	frg_inx = 0;
	frg_len = pxmitpriv->frag_len - 4;/* 2346-4 = 2342 */

	while (1) {
		llc_sz = 0;

		mpdu_len = frg_len;

		pframe = mem_start;

		SetMFrag(mem_start);

		pframe += pattrib->hdrlen;
		mpdu_len -= pattrib->hdrlen;

		/* adding icv, if necessary... */
		if (pattrib->iv_len) {
			memcpy(pframe, pattrib->iv, pattrib->iv_len);

			pframe += pattrib->iv_len;

			mpdu_len -= pattrib->iv_len;
		}

		if (frg_inx == 0) {
			llc_sz = rtw_put_snap(pframe, pattrib->ether_type);
			pframe += llc_sz;
			mpdu_len -= llc_sz;
		}

		if ((pattrib->icv_len > 0) && (pattrib->bswenc))
			mpdu_len -= pattrib->icv_len;


		if (bmcst) {
			/* don't do fragment to broadcat/multicast packets */
			mem_sz = _rtw_pktfile_read(&pktfile, pframe, pattrib->pktlen);
		} else
			mem_sz = _rtw_pktfile_read(&pktfile, pframe, mpdu_len);

		pframe += mem_sz;

		if ((pattrib->icv_len > 0) && (pattrib->bswenc)) {
			memcpy(pframe, pattrib->icv, pattrib->icv_len);
			pframe += pattrib->icv_len;
		}

		frg_inx++;

		if (bmcst || (rtw_endofpktfile(&pktfile))) {
			pattrib->nr_frags = frg_inx;

			pattrib->last_txcmdsz = pattrib->hdrlen + pattrib->iv_len + ((pattrib->nr_frags == 1) ? llc_sz : 0) +
				((pattrib->bswenc) ? pattrib->icv_len : 0) + mem_sz;

			ClearMFrag(mem_start);

			break;
		}

		addr = (SIZE_PTR)(pframe);

		mem_start = (unsigned char *)RND4(addr) + hw_hdr_offset;
		memcpy(mem_start, pbuf_start + hw_hdr_offset, pattrib->hdrlen);

	}

	if (xmitframe_addmic(adapt, pxmitframe) == _FAIL) {
		RTW_INFO("xmitframe_addmic(adapt, pxmitframe)==_FAIL\n");
		res = _FAIL;
		goto exit;
	}

	xmitframe_swencrypt(adapt, pxmitframe);

	if (!bmcst)
		update_attrib_vcs_info(adapt, pxmitframe);
	else
		pattrib->vcs_mode = NONE_VCS;

exit:


	return res;
}

#if defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH)
/*
 * CCMP encryption for unicast robust mgmt frame and broadcast group privicy action
 * BIP for broadcast robust mgmt frame
 */
int rtw_mgmt_xmitframe_coalesce(struct adapter *adapt, struct sk_buff *pkt, struct xmit_frame *pxmitframe)
{
#define DBG_MGMT_XMIT_COALESEC_DUMP 0
#define DBG_MGMT_XMIT_BIP_DUMP 0
#define DBG_MGMT_XMIT_ENC_DUMP 0

	struct pkt_file pktfile;
	int frg_inx, frg_len, mpdu_len, llc_sz, mem_sz;
	SIZE_PTR addr;
	u8 *pframe, *mem_start = NULL, *tmp_buf = NULL;
	u8 hw_hdr_offset, subtype ;
	u8 category = 0xFF;
	struct sta_info		*psta = NULL;
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;
	struct pkt_attrib	*pattrib = &pxmitframe->attrib;
	u8 *pbuf_start;
	int bmcst = IS_MCAST(pattrib->ra);
	int res = _FAIL;
	u8 *BIP_AAD = NULL;
	u8 *MGMT_body = NULL;

	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct rtw_ieee80211_hdr	*pwlanhdr;
	u8 MME[_MME_IE_LENGTH_];

	unsigned long irqL;
	u32	ori_len;
	union pn48 *pn = NULL;
	u8 kid;

	if (!pxmitframe->buf_addr) {
		RTW_WARN(FUNC_ADPT_FMT" pxmitframe->buf_addr\n"
			, FUNC_ADPT_ARG(adapt));
		return _FAIL;
	}

	mem_start = pframe = (u8 *)(pxmitframe->buf_addr) + TXDESC_OFFSET;
	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;
	subtype = get_frame_sub_type(pframe); /* bit(7)~bit(2) */

	/* check if robust mgmt frame */
	if (subtype != WIFI_DEAUTH && subtype != WIFI_DISASSOC && subtype != WIFI_ACTION)
		return _SUCCESS;
	if (subtype == WIFI_ACTION) {
		category = *(pframe + sizeof(struct rtw_ieee80211_hdr_3addr));
		if (CATEGORY_IS_NON_ROBUST(category))
			return _SUCCESS;
	}
	if (!bmcst) {
		if (pattrib->psta)
			psta = pattrib->psta;
		else
			pattrib->psta = psta = rtw_get_stainfo(&adapt->stapriv, pattrib->ra);
		if (!psta) {
			RTW_INFO(FUNC_ADPT_FMT" unicast sta is NULL\n", FUNC_ADPT_ARG(adapt));
			return _FAIL;
		}
		if (!(psta->flags & WLAN_STA_MFP)) {
			/* peer is not MFP capable, no need to encrypt */
			return _SUCCESS;
		}
		if (!psta->bpairwise_key_installed) {
			RTW_INFO(FUNC_ADPT_FMT" PTK is not installed\n"
				, FUNC_ADPT_ARG(adapt));
			return _FAIL;
		}
	}

	ori_len = BIP_AAD_SIZE + pattrib->pktlen;
	tmp_buf = BIP_AAD = rtw_zmalloc(ori_len);
	if (!BIP_AAD)
		return _FAIL;

	_enter_critical_bh(&adapt->security_key_mutex, &irqL);

	if (bmcst) {
		if (subtype == WIFI_ACTION && CATEGORY_IS_GROUP_PRIVACY(category)) {
			/* broadcast group privacy action frame */
			#if DBG_MGMT_XMIT_COALESEC_DUMP
			RTW_INFO(FUNC_ADPT_FMT" broadcast gp action(%u)\n"
				, FUNC_ADPT_ARG(adapt), category);
			#endif

			if (pattrib->psta)
				psta = pattrib->psta;
			else
				pattrib->psta = psta = rtw_get_bcmc_stainfo(adapt);
			if (!psta) {
				RTW_INFO(FUNC_ADPT_FMT" broadcast sta is NULL\n"
					, FUNC_ADPT_ARG(adapt));
				goto xmitframe_coalesce_fail;
			}
			if (!adapt->securitypriv.binstallGrpkey) {
				RTW_INFO(FUNC_ADPT_FMT" GTK is not installed\n"
					, FUNC_ADPT_ARG(adapt));
				goto xmitframe_coalesce_fail;
			}

			pn = &psta->dot11txpn;
			kid = adapt->securitypriv.dot118021XGrpKeyid;
		} else {
			#ifdef CONFIG_IEEE80211W
			/* broadcast robust mgmt frame, using BIP */
			int frame_body_len;
			u8 mic[16];

			#if DBG_MGMT_XMIT_COALESEC_DUMP
			if (subtype == WIFI_DEAUTH)
				RTW_INFO(FUNC_ADPT_FMT" braodcast deauth\n", FUNC_ADPT_ARG(adapt));
			else if (subtype == WIFI_DISASSOC)
				RTW_INFO(FUNC_ADPT_FMT" braodcast disassoc\n", FUNC_ADPT_ARG(adapt));
			else if (subtype == WIFI_ACTION) {
				RTW_INFO(FUNC_ADPT_FMT" braodcast action(%u)\n"
					, FUNC_ADPT_ARG(adapt), category);
			}
			#endif

			/* IGTK key is not install, it may not support 802.11w */
			if (!SEC_IS_BIP_KEY_INSTALLED(&adapt->securitypriv)) {
				RTW_INFO("no instll BIP key\n");
				goto xmitframe_coalesce_success;
			}

			memset(MME, 0, _MME_IE_LENGTH_);

			MGMT_body = pframe + sizeof(struct rtw_ieee80211_hdr_3addr);
			pframe += pattrib->pktlen;

			/* octent 0 and 1 is key index ,BIP keyid is 4 or 5, LSB only need octent 0 */
			MME[0] = adapt->securitypriv.dot11wBIPKeyid;
			/* increase PN and apply to packet */
			adapt->securitypriv.dot11wBIPtxpn.val++;
			RTW_PUT_LE64(&MME[2], adapt->securitypriv.dot11wBIPtxpn.val);

			/* add MME IE with MIC all zero, MME string doesn't include element id and length */
			pframe = rtw_set_ie(pframe, _MME_IE_ , 16 , MME, &(pattrib->pktlen));
			pattrib->last_txcmdsz = pattrib->pktlen;
			/* total frame length - header length */
			frame_body_len = pattrib->pktlen - sizeof(struct rtw_ieee80211_hdr_3addr);

			/* conscruct AAD, copy frame control field */
			memcpy(BIP_AAD, &pwlanhdr->frame_ctl, 2);
			ClearRetry(BIP_AAD);
			ClearPwrMgt(BIP_AAD);
			ClearMData(BIP_AAD);
			/* conscruct AAD, copy address 1 to address 3 */
			memcpy(BIP_AAD + 2, pwlanhdr->addr1, 18);
			/* copy management fram body */
			memcpy(BIP_AAD + BIP_AAD_SIZE, MGMT_body, frame_body_len);

			#if DBG_MGMT_XMIT_BIP_DUMP
			/* dump total packet include MME with zero MIC */
			{
				int i;
				printk("Total packet: ");
				for (i = 0; i < BIP_AAD_SIZE + frame_body_len; i++)
					printk(" %02x ", BIP_AAD[i]);
				printk("\n");
			}
			#endif

			/* calculate mic */
			if (omac1_aes_128(adapt->securitypriv.dot11wBIPKey[adapt->securitypriv.dot11wBIPKeyid].skey
				  , BIP_AAD, BIP_AAD_SIZE + frame_body_len, mic))
				goto xmitframe_coalesce_fail;

			#if DBG_MGMT_XMIT_BIP_DUMP
			/* dump calculated mic result */
			{
				int i;
				printk("Calculated mic result: ");
				for (i = 0; i < 16; i++)
					printk(" %02x ", mic[i]);
				printk("\n");
			}
			#endif

			/* copy right BIP mic value, total is 128bits, we use the 0~63 bits */
			memcpy(pframe - 8, mic, 8);

			#if DBG_MGMT_XMIT_BIP_DUMP
			/*dump all packet after mic ok */
			{
				int pp;
				printk("pattrib->pktlen = %d\n", pattrib->pktlen);
				for(pp=0;pp< pattrib->pktlen; pp++)
					printk(" %02x ", mem_start[pp]);
				printk("\n");
			}
			#endif

			#endif /* CONFIG_IEEE80211W */

			goto xmitframe_coalesce_success;
		}
	}
	else {
		/* unicast robust mgmt frame */
		#if DBG_MGMT_XMIT_COALESEC_DUMP
		if (subtype == WIFI_DEAUTH) {
			RTW_INFO(FUNC_ADPT_FMT" unicast deauth to "MAC_FMT"\n"
				, FUNC_ADPT_ARG(adapt), MAC_ARG(pattrib->ra));
		} else if (subtype == WIFI_DISASSOC) {
			RTW_INFO(FUNC_ADPT_FMT" unicast disassoc to "MAC_FMT"\n"
				, FUNC_ADPT_ARG(adapt), MAC_ARG(pattrib->ra));
		} else if (subtype == WIFI_ACTION) {
			RTW_INFO(FUNC_ADPT_FMT" unicast action(%u) to "MAC_FMT"\n"
				, FUNC_ADPT_ARG(adapt), category, MAC_ARG(pattrib->ra));
		}
		#endif

		memcpy(pattrib->dot118021x_UncstKey.skey, psta->dot118021x_UncstKey.skey, 16);

		/* To use wrong key */
		if (pattrib->key_type == IEEE80211W_WRONG_KEY) {
			RTW_INFO("use wrong key\n");
			pattrib->dot118021x_UncstKey.skey[0] = 0xff;
		}

		pn = &psta->dot11txpn;
		kid = 0;
	}

	#if DBG_MGMT_XMIT_ENC_DUMP
	/* before encrypt dump the management packet content */
	{
		int i;
		printk("Management pkt: ");
		for(i=0; i<pattrib->pktlen; i++)
		printk(" %02x ", pframe[i]);
		printk("=======\n");
	}
	#endif

	/* bakeup original management packet */
	memcpy(tmp_buf, pframe, pattrib->pktlen);
	/* move to data portion */
	pframe += pattrib->hdrlen;

	/* 802.11w encrypted management packet must be _AES_ */
	if (pattrib->key_type != IEEE80211W_NO_KEY) {
		pattrib->encrypt = _AES_;
		pattrib->bswenc = true;
	}

	pattrib->iv_len = 8;
	/* it's MIC of AES */
	pattrib->icv_len = 8;

	switch (pattrib->encrypt) {
	case _AES_:
		/* set AES IV header */
		AES_IV(pattrib->iv, (*pn), kid);
		break;
	default:
		goto xmitframe_coalesce_fail;
	}

	/* insert iv header into management frame */
	memcpy(pframe, pattrib->iv, pattrib->iv_len);
	pframe += pattrib->iv_len;
	/* copy mgmt data portion after CCMP header */
	memcpy(pframe, tmp_buf + pattrib->hdrlen, pattrib->pktlen - pattrib->hdrlen);
	/* move pframe to end of mgmt pkt */
	pframe += pattrib->pktlen - pattrib->hdrlen;
	/* add 8 bytes CCMP IV header to length */
	pattrib->pktlen += pattrib->iv_len;

	#if DBG_MGMT_XMIT_ENC_DUMP
	/* dump management packet include AES IV header */
	{
		int i;
		printk("Management pkt + IV: ");
		/* for(i=0; i<pattrib->pktlen; i++) */

		printk("@@@@@@@@@@@@@\n");
	}
	#endif

	if ((pattrib->icv_len > 0) && (pattrib->bswenc)) {
		memcpy(pframe, pattrib->icv, pattrib->icv_len);
		pframe += pattrib->icv_len;
	}
	/* add 8 bytes MIC */
	pattrib->pktlen += pattrib->icv_len;
	/* set final tx command size */
	pattrib->last_txcmdsz = pattrib->pktlen;

	/* set protected bit must be beofre SW encrypt */
	SetPrivacy(mem_start);

	#if DBG_MGMT_XMIT_ENC_DUMP
	/* dump management packet include AES header */
	{
		int i;
		printk("prepare to enc Management pkt + IV: ");
		for (i = 0; i < pattrib->pktlen; i++)
			printk(" %02x ", mem_start[i]);
		printk("@@@@@@@@@@@@@\n");
	}
	#endif

	/* software encrypt */
	xmitframe_swencrypt(adapt, pxmitframe);

xmitframe_coalesce_success:
	_exit_critical_bh(&adapt->security_key_mutex, &irqL);
	rtw_mfree(BIP_AAD, ori_len);
	return _SUCCESS;

xmitframe_coalesce_fail:
	_exit_critical_bh(&adapt->security_key_mutex, &irqL);
	rtw_mfree(BIP_AAD, ori_len);

	return _FAIL;
}
#endif /* defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH) */

/* Logical Link Control(LLC) SubNetwork Attachment Point(SNAP) header
 * IEEE LLC/SNAP header contains 8 octets
 * First 3 octets comprise the LLC portion
 * SNAP portion, 5 octets, is divided into two fields:
 *	Organizationally Unique Identifier(OUI), 3 octets,
 *	type, defined by that organization, 2 octets.
 */
int rtw_put_snap(u8 *data, u16 h_proto)
{
	struct ieee80211_snap_hdr *snap;
	u8 *oui;


	snap = (struct ieee80211_snap_hdr *)data;
	snap->dsap = 0xaa;
	snap->ssap = 0xaa;
	snap->ctrl = 0x03;

	if (h_proto == 0x8137 || h_proto == 0x80f3)
		oui = P802_1H_OUI;
	else
		oui = RFC1042_OUI;

	snap->oui[0] = oui[0];
	snap->oui[1] = oui[1];
	snap->oui[2] = oui[2];

	*(__be16 *)(data + SNAP_SIZE) = htons(h_proto);


	return SNAP_SIZE + sizeof(u16);
}

void rtw_update_protection(struct adapter *adapt, u8 *ie, uint ie_len)
{

	uint	protection;
	u8	*perp;
	int	 erp_len;
	struct	xmit_priv *pxmitpriv = &adapt->xmitpriv;
	struct	registry_priv *pregistrypriv = &adapt->registrypriv;


	switch (pxmitpriv->vcs_setting) {
	case DISABLE_VCS:
		pxmitpriv->vcs = NONE_VCS;
		break;

	case ENABLE_VCS:
		break;

	case AUTO_VCS:
	default:
		perp = rtw_get_ie(ie, _ERPINFO_IE_, &erp_len, ie_len);
		if (!perp)
			pxmitpriv->vcs = NONE_VCS;
		else {
			protection = (*(perp + 2)) & BIT(1);
			if (protection) {
				if (pregistrypriv->vcs_type == RTS_CTS)
					pxmitpriv->vcs = RTS_CTS;
				else
					pxmitpriv->vcs = CTS_TO_SELF;
			} else
				pxmitpriv->vcs = NONE_VCS;
		}

		break;

	}


}

void rtw_count_tx_stats(struct adapter * adapt, struct xmit_frame *pxmitframe, int sz)
{
	struct sta_info *psta = NULL;
	struct stainfo_stats *pstats = NULL;
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	u8	pkt_num = 1;

	if ((pxmitframe->frame_tag & 0x0f) == DATA_FRAMETAG) {
		pmlmepriv->LinkDetectInfo.NumTxOkInPeriod += pkt_num;

		pxmitpriv->tx_pkts += pkt_num;

		pxmitpriv->tx_bytes += sz;

		psta = pxmitframe->attrib.psta;
		if (psta) {
			pstats = &psta->sta_stats;

			pstats->tx_pkts += pkt_num;

			pstats->tx_bytes += sz;
		}

#ifdef CONFIG_CHECK_LEAVE_LPS
		/* traffic_check_for_leave_lps(adapt, true); */
#endif

	}
}

static struct xmit_buf *__rtw_alloc_cmd_xmitbuf(struct xmit_priv *pxmitpriv,
		enum cmdbuf_type buf_type)
{
	struct xmit_buf *pxmitbuf =  NULL;


	pxmitbuf = &pxmitpriv->pcmd_xmitbuf[buf_type];
	if (pxmitbuf !=  NULL) {
		pxmitbuf->priv_data = NULL;

		if (pxmitbuf->sctx) {
			RTW_INFO("%s pxmitbuf->sctx is not NULL\n", __func__);
			rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_BUF_ALLOC);
		}
	} else {
		RTW_INFO("%s fail, no xmitbuf available !!!\n", __func__);
	}
	return pxmitbuf;
}

struct xmit_frame *__rtw_alloc_cmdxmitframe(struct xmit_priv *pxmitpriv,
		enum cmdbuf_type buf_type)
{
	struct xmit_frame		*pcmdframe;
	struct xmit_buf		*pxmitbuf;

	pcmdframe = rtw_alloc_xmitframe(pxmitpriv);
	if (!pcmdframe) {
		RTW_INFO("%s, alloc xmitframe fail\n", __func__);
		return NULL;
	}

	pxmitbuf = __rtw_alloc_cmd_xmitbuf(pxmitpriv, buf_type);
	if (!pxmitbuf) {
		RTW_INFO("%s, alloc xmitbuf fail\n", __func__);
		rtw_free_xmitframe(pxmitpriv, pcmdframe);
		return NULL;
	}

	pcmdframe->frame_tag = MGNT_FRAMETAG;

	pcmdframe->pxmitbuf = pxmitbuf;

	pcmdframe->buf_addr = pxmitbuf->pbuf;

	/* initial memory to zero */
	memset(pcmdframe->buf_addr, 0, MAX_CMDBUF_SZ);

	pxmitbuf->priv_data = pcmdframe;

	return pcmdframe;

}

struct xmit_buf *rtw_alloc_xmitbuf_ext(struct xmit_priv *pxmitpriv)
{
	unsigned long irqL;
	struct xmit_buf *pxmitbuf =  NULL;
	struct list_head *plist, *phead;
	struct __queue *pfree_queue = &pxmitpriv->free_xmit_extbuf_queue;


	_enter_critical(&pfree_queue->lock, &irqL);

	if (_rtw_queue_empty(pfree_queue))
		pxmitbuf = NULL;
	else {

		phead = get_list_head(pfree_queue);

		plist = get_next(phead);

		pxmitbuf = container_of(plist, struct xmit_buf, list);

		rtw_list_delete(&(pxmitbuf->list));
	}

	if (pxmitbuf !=  NULL) {
		pxmitpriv->free_xmit_extbuf_cnt--;
		pxmitbuf->priv_data = NULL;

		if (pxmitbuf->sctx) {
			RTW_INFO("%s pxmitbuf->sctx is not NULL\n", __func__);
			rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_BUF_ALLOC);
		}

	}

	_exit_critical(&pfree_queue->lock, &irqL);


	return pxmitbuf;
}

int rtw_free_xmitbuf_ext(struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{
	unsigned long irqL;
	struct __queue *pfree_queue = &pxmitpriv->free_xmit_extbuf_queue;


	if (!pxmitbuf)
		return _FAIL;

	_enter_critical(&pfree_queue->lock, &irqL);

	rtw_list_delete(&pxmitbuf->list);

	list_add_tail(&(pxmitbuf->list), get_list_head(pfree_queue));
	pxmitpriv->free_xmit_extbuf_cnt++;

	_exit_critical(&pfree_queue->lock, &irqL);


	return _SUCCESS;
}

struct xmit_buf *rtw_alloc_xmitbuf(struct xmit_priv *pxmitpriv)
{
	unsigned long irqL;
	struct xmit_buf *pxmitbuf =  NULL;
	struct list_head *plist, *phead;
	struct __queue *pfree_xmitbuf_queue = &pxmitpriv->free_xmitbuf_queue;


	/* RTW_INFO("+rtw_alloc_xmitbuf\n"); */

	_enter_critical(&pfree_xmitbuf_queue->lock, &irqL);

	if (_rtw_queue_empty(pfree_xmitbuf_queue))
		pxmitbuf = NULL;
	else {

		phead = get_list_head(pfree_xmitbuf_queue);

		plist = get_next(phead);

		pxmitbuf = container_of(plist, struct xmit_buf, list);

		rtw_list_delete(&(pxmitbuf->list));
	}

	if (pxmitbuf !=  NULL) {
		pxmitpriv->free_xmitbuf_cnt--;

		pxmitbuf->priv_data = NULL;

		if (pxmitbuf->sctx) {
			RTW_INFO("%s pxmitbuf->sctx is not NULL\n", __func__);
			rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_BUF_ALLOC);
		}
	}

	_exit_critical(&pfree_xmitbuf_queue->lock, &irqL);


	return pxmitbuf;
}

int rtw_free_xmitbuf(struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{
	unsigned long irqL;
	struct __queue *pfree_xmitbuf_queue = &pxmitpriv->free_xmitbuf_queue;


	/* RTW_INFO("+rtw_free_xmitbuf\n"); */

	if (!pxmitbuf)
		return _FAIL;

	if (pxmitbuf->sctx) {
		RTW_INFO("%s pxmitbuf->sctx is not NULL\n", __func__);
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_BUF_FREE);
	}

	if (pxmitbuf->buf_tag == XMITBUF_CMD) {
	} else if (pxmitbuf->buf_tag == XMITBUF_MGNT)
		rtw_free_xmitbuf_ext(pxmitpriv, pxmitbuf);
	else {
		_enter_critical(&pfree_xmitbuf_queue->lock, &irqL);

		rtw_list_delete(&pxmitbuf->list);

		list_add_tail(&(pxmitbuf->list), get_list_head(pfree_xmitbuf_queue));

		pxmitpriv->free_xmitbuf_cnt++;
		_exit_critical(&pfree_xmitbuf_queue->lock, &irqL);
	}
	return _SUCCESS;
}

static void rtw_init_xmitframe(struct xmit_frame *pxframe)
{
	if (pxframe !=  NULL) { /* default value setting */
		pxframe->buf_addr = NULL;
		pxframe->pxmitbuf = NULL;

		memset(&pxframe->attrib, 0, sizeof(struct pkt_attrib));
		/* pxframe->attrib.psta = NULL; */

		pxframe->frame_tag = DATA_FRAMETAG;

		pxframe->pkt = NULL;
#ifdef USB_PACKET_OFFSET_SZ
		pxframe->pkt_offset = (PACKET_OFFSET_SZ / 8);
#else
		pxframe->pkt_offset = 1;/* default use pkt_offset to fill tx desc */
#endif

		pxframe->ack_report = 0;
	}
}

/* Calling context:
 * 1. OS_TXENTRY
 * 2. RXENTRY (rx_thread or RX_ISR/RX_CallBack)

 * If we turn on USE_RXTHREAD, then, no need for critical section.
 * Otherwise, we must use _enter/_exit critical to protect free_xmit_queue...

 * Be very very cautious...
 */
struct xmit_frame *rtw_alloc_xmitframe(struct xmit_priv *pxmitpriv)/* (_queue *pfree_xmit_queue) */
{
/* Please remember to use all the osdep_service api,
 * and lock/unlock or _enter/_exit critical to protect
 * pfree_xmit_queue
 */

	unsigned long irqL;
	struct xmit_frame *pxframe = NULL;
	struct list_head *plist, *phead;
	struct __queue *pfree_xmit_queue = &pxmitpriv->free_xmit_queue;

	_enter_critical_bh(&pfree_xmit_queue->lock, &irqL);

	if (_rtw_queue_empty(pfree_xmit_queue)) {
		pxframe =  NULL;
	} else {
		phead = get_list_head(pfree_xmit_queue);

		plist = get_next(phead);

		pxframe = container_of(plist, struct xmit_frame, list);

		rtw_list_delete(&(pxframe->list));
		pxmitpriv->free_xmitframe_cnt--;
	}

	_exit_critical_bh(&pfree_xmit_queue->lock, &irqL);

	rtw_init_xmitframe(pxframe);


	return pxframe;
}

struct xmit_frame *rtw_alloc_xmitframe_ext(struct xmit_priv *pxmitpriv)
{
	unsigned long irqL;
	struct xmit_frame *pxframe = NULL;
	struct list_head *plist, *phead;
	struct __queue *queue = &pxmitpriv->free_xframe_ext_queue;


	_enter_critical_bh(&queue->lock, &irqL);

	if (_rtw_queue_empty(queue)) {
		pxframe =  NULL;
	} else {
		phead = get_list_head(queue);
		plist = get_next(phead);
		pxframe = container_of(plist, struct xmit_frame, list);

		rtw_list_delete(&(pxframe->list));
		pxmitpriv->free_xframe_ext_cnt--;
	}

	_exit_critical_bh(&queue->lock, &irqL);

	rtw_init_xmitframe(pxframe);


	return pxframe;
}

struct xmit_frame *rtw_alloc_xmitframe_once(struct xmit_priv *pxmitpriv)
{
	struct xmit_frame *pxframe = NULL;
	u8 *alloc_addr;

	alloc_addr = rtw_zmalloc(sizeof(struct xmit_frame) + 4);

	if (!alloc_addr)
		goto exit;

	pxframe = (struct xmit_frame *)N_BYTE_ALIGMENT((SIZE_PTR)(alloc_addr), 4);
	pxframe->alloc_addr = alloc_addr;

	pxframe->adapt = pxmitpriv->adapter;
	pxframe->frame_tag = NULL_FRAMETAG;

	pxframe->pkt = NULL;

	pxframe->buf_addr = NULL;
	pxframe->pxmitbuf = NULL;

	rtw_init_xmitframe(pxframe);

	RTW_INFO("################## %s ##################\n", __func__);

exit:
	return pxframe;
}

int rtw_free_xmitframe(struct xmit_priv *pxmitpriv, struct xmit_frame *pxmitframe)
{
	unsigned long irqL;
	struct __queue *queue = NULL;
	struct adapter *adapt = pxmitpriv->adapter;
	struct sk_buff *pndis_pkt = NULL;


	if (!pxmitframe) {
		goto exit;
	}

	if (pxmitframe->pkt) {
		pndis_pkt = pxmitframe->pkt;
		pxmitframe->pkt = NULL;
	}

	if (pxmitframe->alloc_addr) {
		RTW_INFO("################## %s with alloc_addr ##################\n", __func__);
		rtw_mfree(pxmitframe->alloc_addr, sizeof(struct xmit_frame) + 4);
		goto check_pkt_complete;
	}

	if (pxmitframe->ext_tag == 0)
		queue = &pxmitpriv->free_xmit_queue;
	else if (pxmitframe->ext_tag == 1)
		queue = &pxmitpriv->free_xframe_ext_queue;
	else
		rtw_warn_on(1);

	_enter_critical_bh(&queue->lock, &irqL);

	rtw_list_delete(&pxmitframe->list);
	list_add_tail(&pxmitframe->list, get_list_head(queue));
	if (pxmitframe->ext_tag == 0) {
		pxmitpriv->free_xmitframe_cnt++;
	} else if (pxmitframe->ext_tag == 1) {
		pxmitpriv->free_xframe_ext_cnt++;
	} else {
	}

	_exit_critical_bh(&queue->lock, &irqL);

check_pkt_complete:

	if (pndis_pkt)
		rtw_os_pkt_complete(adapt, pndis_pkt);

exit:


	return _SUCCESS;
}

void rtw_free_xmitframe_queue(struct xmit_priv *pxmitpriv, struct __queue *pframequeue)
{
	unsigned long irqL;
	struct list_head	*plist, *phead;
	struct	xmit_frame	*pxmitframe;


	_enter_critical_bh(&(pframequeue->lock), &irqL);

	phead = get_list_head(pframequeue);
	plist = get_next(phead);

	while (!rtw_end_of_queue_search(phead, plist)) {

		pxmitframe = container_of(plist, struct xmit_frame, list);

		plist = get_next(plist);

		rtw_free_xmitframe(pxmitpriv, pxmitframe);

	}
	_exit_critical_bh(&(pframequeue->lock), &irqL);

}

int rtw_xmitframe_enqueue(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	DBG_COUNTER(adapt->tx_logs.core_tx_enqueue);
	if (rtw_xmit_classifier(adapt, pxmitframe) == _FAIL) {
		/*		pxmitframe->pkt = NULL; */
		return _FAIL;
	}

	return _SUCCESS;
}

static struct xmit_frame *dequeue_one_xmitframe(struct xmit_priv *pxmitpriv, struct hw_xmit *phwxmit, struct tx_servq *ptxservq, struct __queue *pframe_queue)
{
	struct list_head	*xmitframe_plist, *xmitframe_phead;
	struct	xmit_frame	*pxmitframe = NULL;

	xmitframe_phead = get_list_head(pframe_queue);
	xmitframe_plist = get_next(xmitframe_phead);

	while ((!rtw_end_of_queue_search(xmitframe_phead, xmitframe_plist))) {
		pxmitframe = container_of(xmitframe_plist, struct xmit_frame, list);

		rtw_list_delete(&pxmitframe->list);

		ptxservq->qcnt--;
		break;
	}

	return pxmitframe;
}

struct xmit_frame *rtw_dequeue_xframe(struct xmit_priv *pxmitpriv, struct hw_xmit *phwxmit_i, int entry)
{
	unsigned long irqL0;
	struct list_head *sta_plist, *sta_phead;
	struct hw_xmit *phwxmit;
	struct tx_servq *ptxservq = NULL;
	struct __queue *pframe_queue = NULL;
	struct xmit_frame *pxmitframe = NULL;
	struct adapter *adapt = pxmitpriv->adapter;
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	int i, inx[4];


	inx[0] = 0;
	inx[1] = 1;
	inx[2] = 2;
	inx[3] = 3;

	if (pregpriv->wifi_spec == 1) {
		int j;

		for (j = 0; j < 4; j++)
			inx[j] = pxmitpriv->wmm_para_seq[j];
	}

	_enter_critical_bh(&pxmitpriv->lock, &irqL0);

	for (i = 0; i < entry; i++) {
		phwxmit = phwxmit_i + inx[i];

		/* _enter_critical_ex(&phwxmit->sta_queue->lock, &irqL0); */

		sta_phead = get_list_head(phwxmit->sta_queue);
		sta_plist = get_next(sta_phead);

		while ((!rtw_end_of_queue_search(sta_phead, sta_plist))) {

			ptxservq = container_of(sta_plist, struct tx_servq, tx_pending);

			pframe_queue = &ptxservq->sta_pending;

			pxmitframe = dequeue_one_xmitframe(pxmitpriv, phwxmit, ptxservq, pframe_queue);

			if (pxmitframe) {
				phwxmit->accnt--;

				/* Remove sta node when there is no pending packets. */
				if (_rtw_queue_empty(pframe_queue)) /* must be done after get_next and before break */
					rtw_list_delete(&ptxservq->tx_pending);

				/* _exit_critical_ex(&phwxmit->sta_queue->lock, &irqL0); */

				goto exit;
			}

			sta_plist = get_next(sta_plist);

		}

		/* _exit_critical_ex(&phwxmit->sta_queue->lock, &irqL0); */

	}

exit:

	_exit_critical_bh(&pxmitpriv->lock, &irqL0);

	return pxmitframe;
}

struct tx_servq *rtw_get_sta_pending(struct adapter *adapt, struct sta_info *psta, int up, u8 *ac)
{
	struct tx_servq *ptxservq = NULL;


	switch (up) {
	case 1:
	case 2:
		ptxservq = &(psta->sta_xmitpriv.bk_q);
		*(ac) = 3;
		break;

	case 4:
	case 5:
		ptxservq = &(psta->sta_xmitpriv.vi_q);
		*(ac) = 1;
		break;

	case 6:
	case 7:
		ptxservq = &(psta->sta_xmitpriv.vo_q);
		*(ac) = 0;
		break;

	case 0:
	case 3:
	default:
		ptxservq = &(psta->sta_xmitpriv.be_q);
		*(ac) = 2;
		break;

	}


	return ptxservq;
}

/*
 * Will enqueue pxmitframe to the proper queue,
 * and indicate it to xx_pending list.....
 */
int rtw_xmit_classifier(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	/* unsigned long irqL0; */
	u8	ac_index;
	struct sta_info	*psta;
	struct tx_servq	*ptxservq;
	struct pkt_attrib	*pattrib = &pxmitframe->attrib;
	struct hw_xmit	*phwxmits =  adapt->xmitpriv.hwxmits;
	int res = _SUCCESS;


	DBG_COUNTER(adapt->tx_logs.core_tx_enqueue_class);

	/*
		if (pattrib->psta) {
			psta = pattrib->psta;
		} else {
			RTW_INFO("%s, call rtw_get_stainfo()\n", __func__);
			psta = rtw_get_stainfo(pstapriv, pattrib->ra);
		}
	*/

	psta = rtw_get_stainfo(&adapt->stapriv, pattrib->ra);
	if (pattrib->psta != psta) {
		DBG_COUNTER(adapt->tx_logs.core_tx_enqueue_class_err_sta);
		RTW_INFO("%s, pattrib->psta(%p) != psta(%p)\n", __func__, pattrib->psta, psta);
		return _FAIL;
	}

	if (!psta) {
		DBG_COUNTER(adapt->tx_logs.core_tx_enqueue_class_err_nosta);
		res = _FAIL;
		RTW_INFO("rtw_xmit_classifier: psta is NULL\n");
		goto exit;
	}

	if (!(psta->state & _FW_LINKED)) {
		DBG_COUNTER(adapt->tx_logs.core_tx_enqueue_class_err_fwlink);
		RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, psta->state);
		return _FAIL;
	}

	ptxservq = rtw_get_sta_pending(adapt, psta, pattrib->priority, (u8 *)(&ac_index));

	/* _enter_critical(&pstapending->lock, &irqL0); */

	if (rtw_is_list_empty(&ptxservq->tx_pending))
		list_add_tail(&ptxservq->tx_pending, get_list_head(phwxmits[ac_index].sta_queue));

	/* _enter_critical(&ptxservq->sta_pending.lock, &irqL1); */

	list_add_tail(&pxmitframe->list, get_list_head(&ptxservq->sta_pending));
	ptxservq->qcnt++;
	phwxmits[ac_index].accnt++;

	/* _exit_critical(&ptxservq->sta_pending.lock, &irqL1); */

	/* _exit_critical(&pstapending->lock, &irqL0); */

exit:


	return res;
}

void rtw_alloc_hwxmits(struct adapter *adapt)
{
	struct hw_xmit *hwxmits;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	pxmitpriv->hwxmit_entry = HWXMIT_ENTRY;

	pxmitpriv->hwxmits = NULL;

	pxmitpriv->hwxmits = (struct hw_xmit *)rtw_zmalloc(sizeof(struct hw_xmit) * pxmitpriv->hwxmit_entry);

	if (!pxmitpriv->hwxmits) {
		RTW_INFO("alloc hwxmits fail!...\n");
		return;
	}

	hwxmits = pxmitpriv->hwxmits;

	if (pxmitpriv->hwxmit_entry == 5) {
		/* pxmitpriv->bmc_txqueue.head = 0; */
		/* hwxmits[0] .phwtxqueue = &pxmitpriv->bmc_txqueue; */
		hwxmits[0] .sta_queue = &pxmitpriv->bm_pending;

		/* pxmitpriv->vo_txqueue.head = 0; */
		/* hwxmits[1] .phwtxqueue = &pxmitpriv->vo_txqueue; */
		hwxmits[1] .sta_queue = &pxmitpriv->vo_pending;

		/* pxmitpriv->vi_txqueue.head = 0; */
		/* hwxmits[2] .phwtxqueue = &pxmitpriv->vi_txqueue; */
		hwxmits[2] .sta_queue = &pxmitpriv->vi_pending;

		/* pxmitpriv->bk_txqueue.head = 0; */
		/* hwxmits[3] .phwtxqueue = &pxmitpriv->bk_txqueue; */
		hwxmits[3] .sta_queue = &pxmitpriv->bk_pending;

		/* pxmitpriv->be_txqueue.head = 0; */
		/* hwxmits[4] .phwtxqueue = &pxmitpriv->be_txqueue; */
		hwxmits[4] .sta_queue = &pxmitpriv->be_pending;

	} else if (pxmitpriv->hwxmit_entry == 4) {

		/* pxmitpriv->vo_txqueue.head = 0; */
		/* hwxmits[0] .phwtxqueue = &pxmitpriv->vo_txqueue; */
		hwxmits[0] .sta_queue = &pxmitpriv->vo_pending;

		/* pxmitpriv->vi_txqueue.head = 0; */
		/* hwxmits[1] .phwtxqueue = &pxmitpriv->vi_txqueue; */
		hwxmits[1] .sta_queue = &pxmitpriv->vi_pending;

		/* pxmitpriv->be_txqueue.head = 0; */
		/* hwxmits[2] .phwtxqueue = &pxmitpriv->be_txqueue; */
		hwxmits[2] .sta_queue = &pxmitpriv->be_pending;

		/* pxmitpriv->bk_txqueue.head = 0; */
		/* hwxmits[3] .phwtxqueue = &pxmitpriv->bk_txqueue; */
		hwxmits[3] .sta_queue = &pxmitpriv->bk_pending;
	} else {


	}


}

void rtw_free_hwxmits(struct adapter *adapt)
{
	struct hw_xmit *hwxmits;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	hwxmits = pxmitpriv->hwxmits;
	if (hwxmits)
		rtw_mfree((u8 *)hwxmits, (sizeof(struct hw_xmit) * pxmitpriv->hwxmit_entry));
}

void rtw_init_hwxmits(struct hw_xmit *phwxmit, int entry)
{
	int i;
	for (i = 0; i < entry; i++, phwxmit++) {
		/* spin_lock_init(&phwxmit->xmit_lock); */
		/* INIT_LIST_HEAD(&phwxmit->pending);		 */
		/* phwxmit->txcmdcnt = 0; */
		phwxmit->accnt = 0;
	}
}

static int rtw_br_client_tx(struct adapter *adapt, struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	unsigned long irqL;
	int res, is_vlan_tag = 0, i, do_nat25 = 1;
	unsigned short vlan_hdr = 0;
	void *br_port = NULL;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
	br_port = adapt->pnetdev->br_port;
#else   /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
	rcu_read_lock();
	br_port = rcu_dereference(adapt->pnetdev->rx_handler_data);
	rcu_read_unlock();
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
	_enter_critical_bh(&adapt->br_ext_lock, &irqL);
	if (!(skb->data[0] & 1) &&
	    br_port &&
	    memcmp(skb->data + MACADDRLEN, adapt->br_mac, MACADDRLEN) &&
	    *((__be16 *)(skb->data + MACADDRLEN * 2)) != __constant_htons(ETH_P_8021Q) &&
	    *((__be16 *)(skb->data + MACADDRLEN * 2)) == __constant_htons(ETH_P_IP) &&
	    !memcmp(adapt->scdb_mac, skb->data + MACADDRLEN, MACADDRLEN) && adapt->scdb_entry) {
		memcpy(skb->data + MACADDRLEN, GET_MY_HWADDR(adapt), MACADDRLEN);
		adapt->scdb_entry->ageing_timer = jiffies;
		_exit_critical_bh(&adapt->br_ext_lock, &irqL);
	} else
		/* if (!priv->pmib->ethBrExtInfo.nat25_disable)		 */
	{
		/*			if (priv->dev->br_port &&
		 *				 !memcmp(skb->data+MACADDRLEN, priv->br_mac, MACADDRLEN)) { */
		if (*((__be16 *)(skb->data + MACADDRLEN * 2)) == __constant_htons(ETH_P_8021Q)) {
			is_vlan_tag = 1;
			vlan_hdr = *((unsigned short *)(skb->data + MACADDRLEN * 2 + 2));
			for (i = 0; i < 6; i++)
				*((unsigned short *)(skb->data + MACADDRLEN * 2 + 2 - i * 2)) = *((unsigned short *)(skb->data + MACADDRLEN * 2 - 2 - i * 2));
			skb_pull(skb, 4);
		}
		/* if SA == br_mac && skb== IP  => copy SIP to br_ip ?? why */
		if (!memcmp(skb->data + MACADDRLEN, adapt->br_mac, MACADDRLEN) &&
		    (*((__be16 *)(skb->data + MACADDRLEN * 2)) == __constant_htons(ETH_P_IP)))
			memcpy(adapt->br_ip, skb->data + WLAN_ETHHDR_LEN + 12, 4);

		if (*((__be16 *)(skb->data + MACADDRLEN * 2)) == __constant_htons(ETH_P_IP)) {
			if (memcmp(adapt->scdb_mac, skb->data + MACADDRLEN, MACADDRLEN)) {
				adapt->scdb_entry = (struct nat25_network_db_entry *)scdb_findEntry(adapt,
					skb->data + MACADDRLEN, skb->data + WLAN_ETHHDR_LEN + 12);
				if (adapt->scdb_entry) {
					memcpy(adapt->scdb_mac, skb->data + MACADDRLEN, MACADDRLEN);
					memcpy(adapt->scdb_ip, skb->data + WLAN_ETHHDR_LEN + 12, 4);
					adapt->scdb_entry->ageing_timer = jiffies;
					do_nat25 = 0;
				}
			} else {
				if (adapt->scdb_entry) {
					adapt->scdb_entry->ageing_timer = jiffies;
					do_nat25 = 0;
				} else {
					memset(adapt->scdb_mac, 0, MACADDRLEN);
					memset(adapt->scdb_ip, 0, 4);
				}
			}
		}
		_exit_critical_bh(&adapt->br_ext_lock, &irqL);
		if (do_nat25) {
			if (nat25_db_handle(adapt, skb, NAT25_CHECK) == 0) {
				struct sk_buff *newskb;

				if (is_vlan_tag) {
					skb_push(skb, 4);
					for (i = 0; i < 6; i++)
						*((unsigned short *)(skb->data + i * 2)) = *((unsigned short *)(skb->data + 4 + i * 2));
					*((__be16 *)(skb->data + MACADDRLEN * 2)) = __constant_htons(ETH_P_8021Q);
					*((unsigned short *)(skb->data + MACADDRLEN * 2 + 2)) = vlan_hdr;
				}
				newskb = rtw_skb_copy(skb);
				if (!newskb) {
					DEBUG_ERR("TX DROP: rtw_skb_copy fail!\n");
					return -1;
				}
				dev_kfree_skb_any(skb);
				*pskb = skb = newskb;
				if (is_vlan_tag) {
					vlan_hdr = *((unsigned short *)(skb->data + MACADDRLEN * 2 + 2));
					for (i = 0; i < 6; i++)
						*((unsigned short *)(skb->data + MACADDRLEN * 2 + 2 - i * 2)) = *((unsigned short *)(skb->data + MACADDRLEN * 2 - 2 - i * 2));
					skb_pull(skb, 4);
				}
			}
			if (skb_is_nonlinear(skb))
				DEBUG_ERR("%s(): skb_is_nonlinear!!\n", __func__);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
			res = skb_linearize(skb, GFP_ATOMIC);
#else	/* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)) */
			res = skb_linearize(skb);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)) */
			if (res < 0) {
				DEBUG_ERR("TX DROP: skb_linearize fail!\n");
				return -1;
			}

			res = nat25_db_handle(adapt, skb, NAT25_INSERT);
			if (res < 0) {
				if (res == -2) {
					DEBUG_ERR("TX DROP: nat25_db_handle fail!\n");
					return -1;

				}
				return 0;
			}
		}

		memcpy(skb->data + MACADDRLEN, GET_MY_HWADDR(adapt), MACADDRLEN);

		dhcp_flag_bcast(adapt, skb);

		if (is_vlan_tag) {
			skb_push(skb, 4);
			for (i = 0; i < 6; i++)
				*((unsigned short *)(skb->data + i * 2)) = *((unsigned short *)(skb->data + 4 + i * 2));
			*((__be16 *)(skb->data + MACADDRLEN * 2)) = __constant_htons(ETH_P_8021Q);
			*((unsigned short *)(skb->data + MACADDRLEN * 2 + 2)) = vlan_hdr;
		}
	}
	/* check if SA is equal to our MAC */
	if (memcmp(skb->data + MACADDRLEN, GET_MY_HWADDR(adapt), MACADDRLEN)) {
		DEBUG_ERR("TX DROP: untransformed frame SA:%02X%02X%02X%02X%02X%02X!\n",
			skb->data[6], skb->data[7], skb->data[8], skb->data[9], skb->data[10], skb->data[11]);
		return -1;
	}
	return 0;
}

u32 rtw_get_ff_hwaddr(struct xmit_frame *pxmitframe)
{
	u32 addr;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;

	switch (pattrib->qsel) {
	case 0:
	case 3:
		addr = BE_QUEUE_INX;
		break;
	case 1:
	case 2:
		addr = BK_QUEUE_INX;
		break;
	case 4:
	case 5:
		addr = VI_QUEUE_INX;
		break;
	case 6:
	case 7:
		addr = VO_QUEUE_INX;
		break;
	case 0x10:
		addr = BCN_QUEUE_INX;
		break;
	case 0x11: /* BC/MC in PS (HIQ) */
		addr = HIGH_QUEUE_INX;
		break;
	case 0x13:
		addr = TXCMD_QUEUE_INX;
		break;
	case 0x12:
	default:
		addr = MGT_QUEUE_INX;
		break;

	}

	return addr;

}

static void do_queue_select(struct adapter	*adapt, struct pkt_attrib *pattrib)
{
	u8 qsel;

	qsel = pattrib->priority;
	pattrib->qsel = qsel;
}

/*
 * The main transmit(tx) entry
 *
 * Return
 *	1	enqueue
 *	0	success, hardware will handle this xmit frame(packet)
 *	<0	fail
 */
 #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
int rtw_monitor_xmit_entry(struct sk_buff *skb, struct net_device *ndev)
{
	int rtap_len;
	u16 frame_ctl;
	struct rtw_ieee80211_hdr *dot11_hdr;
	struct ieee80211_radiotap_header *rtap_hdr;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(ndev);

	if (skb)
		rtw_mstat_update(MSTAT_TYPE_SKB, MSTAT_ALLOC_SUCCESS, skb->truesize);

	if (unlikely(skb->len < sizeof(struct ieee80211_radiotap_header)))
		goto fail;

	rtap_hdr = (struct ieee80211_radiotap_header *)skb->data;
	if (unlikely(rtap_hdr->it_version))
		goto fail;

	rtap_len = ieee80211_get_radiotap_len(skb->data);
	if (unlikely(skb->len < rtap_len))
		goto fail;

	if (rtap_len != 12) {
		RTW_INFO("radiotap len (should be 14): %d\n", rtap_len);
		goto fail;
	}

	/* Skip the ratio tap header */
	skb_pull(skb, rtap_len);

	dot11_hdr = (struct rtw_ieee80211_hdr *)skb->data;
	frame_ctl = le16_to_cpu(dot11_hdr->frame_ctl);
	/* Check if the QoS bit is set */

	if ((frame_ctl & RTW_IEEE80211_FCTL_FTYPE) == RTW_IEEE80211_FTYPE_DATA) {
		struct xmit_frame		*pmgntframe;
		struct pkt_attrib	*pattrib;
		unsigned char	*pframe;
		struct rtw_ieee80211_hdr *pwlanhdr;
		struct xmit_priv	*pxmitpriv = &(adapt->xmitpriv);
		struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
		u8 *buf = skb->data;
		u32 len = skb->len;

		pmgntframe = alloc_mgtxmitframe(pxmitpriv);
		if (!pmgntframe) {
			rtw_udelay_os(500);
			goto fail;
		}
		pattrib = &pmgntframe->attrib;

		update_monitor_frame_attrib(adapt, pattrib);

		pattrib->retry_ctrl = false;

		memset(pmgntframe->buf_addr, 0, WLANHDR_OFFSET + TXDESC_OFFSET);

		pframe = (u8 *)(pmgntframe->buf_addr) + TXDESC_OFFSET;

		memcpy(pframe, (void *)buf, len);

		pattrib->pktlen = len;

		pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

		if (is_broadcast_mac_addr(pwlanhdr->addr3) || is_broadcast_mac_addr(pwlanhdr->addr1))
			pattrib->rate = MGN_24M;

		pmlmeext->mgnt_seq = GetSequence(pwlanhdr);
		pattrib->seqnum = pmlmeext->mgnt_seq;
		pmlmeext->mgnt_seq++;

		pattrib->last_txcmdsz = pattrib->pktlen;

		dump_mgntframe(adapt, pmgntframe);

	} else {
		struct xmit_frame		*pmgntframe;
		struct pkt_attrib	*pattrib;
		unsigned char	*pframe;
		struct rtw_ieee80211_hdr *pwlanhdr;
		struct xmit_priv	*pxmitpriv = &(adapt->xmitpriv);
		struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
		u8 *buf = skb->data;
		u32 len = skb->len;

		pmgntframe = alloc_mgtxmitframe(pxmitpriv);
		if (!pmgntframe)
			goto fail;

		pattrib = &pmgntframe->attrib;
		update_mgntframe_attrib(adapt, pattrib);
		pattrib->retry_ctrl = false;

		memset(pmgntframe->buf_addr, 0, WLANHDR_OFFSET + TXDESC_OFFSET);

		pframe = (u8 *)(pmgntframe->buf_addr) + TXDESC_OFFSET;

		memcpy(pframe, (void *)buf, len);

		pattrib->pktlen = len;

		pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

		pmlmeext->mgnt_seq = GetSequence(pwlanhdr);
		pattrib->seqnum = pmlmeext->mgnt_seq;
		pmlmeext->mgnt_seq++;

		pattrib->last_txcmdsz = pattrib->pktlen;

		dump_mgntframe(adapt, pmgntframe);

	}

fail:

	dev_kfree_skb_any(skb);

	return 0;
}
#endif
/*
 * The main transmit(tx) entry
 *
 * Return
 *	1	enqueue
 *	0	success, hardware will handle this xmit frame(packet)
 *	<0	fail
 */
int rtw_xmit(struct adapter *adapt, struct sk_buff **ppkt)
{
	static unsigned long start = 0;
	static u32 drop_cnt = 0;
	unsigned long irqL0;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	struct xmit_frame *pxmitframe = NULL;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	void *br_port = NULL;
	int res;

	DBG_COUNTER(adapt->tx_logs.core_tx);

	if (IS_CH_WAITING(adapter_to_rfctl(adapt)))
		return -1;

	if (start == 0)
		start = rtw_get_current_time();

	pxmitframe = rtw_alloc_xmitframe(pxmitpriv);

	if (rtw_get_passing_time_ms(start) > 2000) {
		if (drop_cnt)
			RTW_INFO("DBG_TX_DROP_FRAME %s no more pxmitframe, drop_cnt:%u\n", __func__, drop_cnt);
		start = rtw_get_current_time();
		drop_cnt = 0;
	}

	if (!pxmitframe) {
		drop_cnt++;
		/*RTW_INFO("%s-"ADPT_FMT" no more xmitframe\n", __func__, ADPT_ARG(adapt));*/
		DBG_COUNTER(adapt->tx_logs.core_tx_err_pxmitframe);
		return -1;
	}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
	br_port = adapt->pnetdev->br_port;
#else   /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
	rcu_read_lock();
	br_port = rcu_dereference(adapt->pnetdev->rx_handler_data);
	rcu_read_unlock();
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */

	if (br_port && check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_ADHOC_STATE)) {
		res = rtw_br_client_tx(adapt, ppkt);
		if (res == -1) {
			rtw_free_xmitframe(pxmitpriv, pxmitframe);
			DBG_COUNTER(adapt->tx_logs.core_tx_err_brtx);
			return -1;
		}
	}
	res = update_attrib(adapt, *ppkt, &pxmitframe->attrib);

	if (res == _FAIL) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
		return -1;
	}
	pxmitframe->pkt = *ppkt;

	rtw_led_control(adapt, LED_CTL_TX);

	do_queue_select(adapt, &pxmitframe->attrib);

	_enter_critical_bh(&pxmitpriv->lock, &irqL0);
	if (xmitframe_enqueue_for_sleeping_sta(adapt, pxmitframe)) {
		_exit_critical_bh(&pxmitpriv->lock, &irqL0);
		DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue);
		return 1;
	}
	_exit_critical_bh(&pxmitpriv->lock, &irqL0);

	/* pre_xmitframe */
	if (!rtw_hal_xmit(adapt, pxmitframe))
		return 1;

	return 0;
}

#define RTW_HIQ_FILTER_ALLOW_ALL 0
#define RTW_HIQ_FILTER_ALLOW_SPECIAL 1
#define RTW_HIQ_FILTER_DENY_ALL 2

inline bool xmitframe_hiq_filter(struct xmit_frame *xmitframe)
{
	bool allow = false;
	struct adapter *adapter = xmitframe->adapt;
	struct registry_priv *registry = &adapter->registrypriv;

	if (adapter->registrypriv.wifi_spec == 1)
		allow = true;
	else if (registry->hiq_filter == RTW_HIQ_FILTER_ALLOW_SPECIAL) {

		struct pkt_attrib *attrib = &xmitframe->attrib;

		if (attrib->ether_type == 0x0806 ||
		    attrib->ether_type == 0x888e || attrib->dhcp_pkt)
			allow = true;
	} else if (registry->hiq_filter == RTW_HIQ_FILTER_ALLOW_ALL)
		allow = true;
	else if (registry->hiq_filter == RTW_HIQ_FILTER_DENY_ALL)
		allow = false;
	else
		rtw_warn_on(1);

	return allow;
}

int xmitframe_enqueue_for_sleeping_sta(struct adapter *adapt, struct xmit_frame *pxmitframe)
{
	unsigned long irqL;
	int ret = false;
	struct sta_info *psta = NULL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	int bmcst = IS_MCAST(pattrib->ra);
	bool update_tim = false;

	if (!MLME_IS_AP(adapt) && !MLME_IS_MESH(adapt)) {
		DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_warn_fwstate);
		return ret;
	}
	psta = rtw_get_stainfo(&adapt->stapriv, pattrib->ra);
	if (pattrib->psta != psta) {
		DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_warn_sta);
		RTW_INFO("%s, pattrib->psta(%p) != psta(%p)\n", __func__, pattrib->psta, psta);
		return false;
	}

	if (!psta) {
		DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_warn_nosta);
		RTW_INFO("%s, psta==NUL\n", __func__);
		return false;
	}

	if (!(psta->state & _FW_LINKED)) {
		DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_warn_link);
		RTW_INFO("%s, psta->state(0x%x) != _FW_LINKED\n", __func__, psta->state);
		return false;
	}

	if (pattrib->triggered == 1) {
		DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_warn_trigger);
		/* RTW_INFO("directly xmit pspoll_triggered packet\n"); */

		/* pattrib->triggered=0; */
		if (bmcst && xmitframe_hiq_filter(pxmitframe))
			pattrib->qsel = QSLT_HIGH;/* HIQ */

		return ret;
	}


	if (bmcst) {
		_enter_critical_bh(&psta->sleep_q.lock, &irqL);

		if (rtw_tim_map_anyone_be_set(adapt, pstapriv->sta_dz_bitmap)) { /* if anyone sta is in ps mode */
			/* pattrib->qsel = QSLT_HIGH; */ /* HIQ */

			rtw_list_delete(&pxmitframe->list);

			/*_enter_critical_bh(&psta->sleep_q.lock, &irqL);*/

			list_add_tail(&pxmitframe->list, get_list_head(&psta->sleep_q));

			psta->sleepq_len++;

			if (!(rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, 0)))
				update_tim = true;

			rtw_tim_map_set(adapt, pstapriv->tim_bitmap, 0);
			rtw_tim_map_set(adapt, pstapriv->sta_dz_bitmap, 0);

			/* RTW_INFO("enqueue, sq_len=%d\n", psta->sleepq_len); */
			/* RTW_INFO_DUMP("enqueue, tim=", pstapriv->tim_bitmap, pstapriv->aid_bmp_len); */
			if (update_tim) {
				if (is_broadcast_mac_addr(pattrib->ra))
					_update_beacon(adapt, _TIM_IE_, NULL, true, "buffer BC");
				else
					_update_beacon(adapt, _TIM_IE_, NULL, true, "buffer MC");
			} else
				chk_bmc_sleepq_cmd(adapt);

			/*_exit_critical_bh(&psta->sleep_q.lock, &irqL);*/

			ret = true;

			DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_mcast);
		}

		_exit_critical_bh(&psta->sleep_q.lock, &irqL);

		return ret;

	}


	_enter_critical_bh(&psta->sleep_q.lock, &irqL);

	if (psta->state & WIFI_SLEEP_STATE) {
		u8 wmmps_ac = 0;

		if (rtw_tim_map_is_set(adapt, pstapriv->sta_dz_bitmap, psta->cmn.aid)) {
			rtw_list_delete(&pxmitframe->list);

			/* _enter_critical_bh(&psta->sleep_q.lock, &irqL);	 */

			list_add_tail(&pxmitframe->list, get_list_head(&psta->sleep_q));

			psta->sleepq_len++;

			switch (pattrib->priority) {
			case 1:
			case 2:
				wmmps_ac = psta->uapsd_bk & BIT(0);
				break;
			case 4:
			case 5:
				wmmps_ac = psta->uapsd_vi & BIT(0);
				break;
			case 6:
			case 7:
				wmmps_ac = psta->uapsd_vo & BIT(0);
				break;
			case 0:
			case 3:
			default:
				wmmps_ac = psta->uapsd_be & BIT(0);
				break;
			}

			if (wmmps_ac)
				psta->sleepq_ac_len++;

			if (((psta->has_legacy_ac) && (!wmmps_ac)) || ((!psta->has_legacy_ac) && (wmmps_ac))) {
				if (!(rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, psta->cmn.aid)))
					update_tim = true;

				rtw_tim_map_set(adapt, pstapriv->tim_bitmap, psta->cmn.aid);

				/* RTW_INFO("enqueue, sq_len=%d\n", psta->sleepq_len); */
				/* RTW_INFO_DUMP("enqueue, tim=", pstapriv->tim_bitmap, pstapriv->aid_bmp_len); */

				if (update_tim) {
					/* RTW_INFO("sleepq_len==1, update BCNTIM\n"); */
					/* upate BCN for TIM IE */
					_update_beacon(adapt, _TIM_IE_, NULL, true, "buffer UC");
				}
			}

			/* _exit_critical_bh(&psta->sleep_q.lock, &irqL);			 */

			/* if(psta->sleepq_len > (NR_XMITFRAME>>3)) */
			/* { */
			/*	wakeup_sta_to_xmit(adapt, psta); */
			/* }	 */

			ret = true;

			DBG_COUNTER(adapt->tx_logs.core_tx_ap_enqueue_ucast);
		}

	}

	_exit_critical_bh(&psta->sleep_q.lock, &irqL);

	return ret;

}

static void dequeue_xmitframes_to_sleeping_queue(struct adapter *adapt, struct sta_info *psta, struct __queue *pframequeue)
{
	int ret;
	struct list_head	*plist, *phead;
	u8	ac_index;
	struct tx_servq	*ptxservq;
	struct pkt_attrib	*pattrib;
	struct xmit_frame	*pxmitframe;
	struct hw_xmit *phwxmits =  adapt->xmitpriv.hwxmits;

	phead = get_list_head(pframequeue);
	plist = get_next(phead);

	while (!rtw_end_of_queue_search(phead, plist)) {
		pxmitframe = container_of(plist, struct xmit_frame, list);

		plist = get_next(plist);

		pattrib = &pxmitframe->attrib;

		pattrib->triggered = 0;

		ret = xmitframe_enqueue_for_sleeping_sta(adapt, pxmitframe);

		if (ret) {
			ptxservq = rtw_get_sta_pending(adapt, psta, pattrib->priority, (u8 *)(&ac_index));

			ptxservq->qcnt--;
			phwxmits[ac_index].accnt--;
		} else {
			/* RTW_INFO("xmitframe_enqueue_for_sleeping_sta return false\n"); */
		}

	}

}

void stop_sta_xmit(struct adapter *adapt, struct sta_info *psta)
{
	unsigned long irqL0;
	struct sta_info *psta_bmc;
	struct sta_xmit_priv *pstaxmitpriv;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	pstaxmitpriv = &psta->sta_xmitpriv;

	/* for BC/MC Frames */
	psta_bmc = rtw_get_bcmc_stainfo(adapt);


	_enter_critical_bh(&pxmitpriv->lock, &irqL0);

	psta->state |= WIFI_SLEEP_STATE;

	rtw_tim_map_set(adapt, pstapriv->sta_dz_bitmap, psta->cmn.aid);

	dequeue_xmitframes_to_sleeping_queue(adapt, psta, &pstaxmitpriv->vo_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->vo_q.tx_pending));


	dequeue_xmitframes_to_sleeping_queue(adapt, psta, &pstaxmitpriv->vi_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->vi_q.tx_pending));


	dequeue_xmitframes_to_sleeping_queue(adapt, psta, &pstaxmitpriv->be_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->be_q.tx_pending));


	dequeue_xmitframes_to_sleeping_queue(adapt, psta, &pstaxmitpriv->bk_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->bk_q.tx_pending));

	/* for BC/MC Frames */
	pstaxmitpriv = &psta_bmc->sta_xmitpriv;
	dequeue_xmitframes_to_sleeping_queue(adapt, psta_bmc, &pstaxmitpriv->be_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->be_q.tx_pending));

	_exit_critical_bh(&pxmitpriv->lock, &irqL0);
}

void wakeup_sta_to_xmit(struct adapter *adapt, struct sta_info *psta)
{
	unsigned long irqL;
	u8 update_mask = 0, wmmps_ac = 0;
	struct sta_info *psta_bmc;
	struct list_head	*xmitframe_plist, *xmitframe_phead;
	struct xmit_frame *pxmitframe = NULL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	psta_bmc = rtw_get_bcmc_stainfo(adapt);

	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	xmitframe_phead = get_list_head(&psta->sleep_q);
	xmitframe_plist = get_next(xmitframe_phead);

	while ((!rtw_end_of_queue_search(xmitframe_phead, xmitframe_plist))) {
		pxmitframe = container_of(xmitframe_plist, struct xmit_frame, list);

		xmitframe_plist = get_next(xmitframe_plist);

		rtw_list_delete(&pxmitframe->list);

		switch (pxmitframe->attrib.priority) {
		case 1:
		case 2:
			wmmps_ac = psta->uapsd_bk & BIT(1);
			break;
		case 4:
		case 5:
			wmmps_ac = psta->uapsd_vi & BIT(1);
			break;
		case 6:
		case 7:
			wmmps_ac = psta->uapsd_vo & BIT(1);
			break;
		case 0:
		case 3:
		default:
			wmmps_ac = psta->uapsd_be & BIT(1);
			break;
		}

		psta->sleepq_len--;
		if (psta->sleepq_len > 0)
			pxmitframe->attrib.mdata = 1;
		else
			pxmitframe->attrib.mdata = 0;

		if (wmmps_ac) {
			psta->sleepq_ac_len--;
			if (psta->sleepq_ac_len > 0) {
				pxmitframe->attrib.mdata = 1;
				pxmitframe->attrib.eosp = 0;
			} else {
				pxmitframe->attrib.mdata = 0;
				pxmitframe->attrib.eosp = 1;
			}
		}
		pxmitframe->attrib.triggered = 1;

		rtw_hal_xmitframe_enqueue(adapt, pxmitframe);
	}

	if (psta->sleepq_len == 0) {
		if (rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, psta->cmn.aid))
			update_mask = BIT(0);

		rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, psta->cmn.aid);

		if (psta->state & WIFI_SLEEP_STATE)
			psta->state ^= WIFI_SLEEP_STATE;

		if (psta->state & WIFI_STA_ALIVE_CHK_STATE) {
			RTW_INFO("%s alive check\n", __func__);
			psta->expire_to = pstapriv->expire_to;
			psta->state ^= WIFI_STA_ALIVE_CHK_STATE;
		}

		rtw_tim_map_clear(adapt, pstapriv->sta_dz_bitmap, psta->cmn.aid);
	}

	/* for BC/MC Frames */
	if (!psta_bmc)
		goto _exit;

	if (!(rtw_tim_map_anyone_be_set_exclude_aid0(adapt, pstapriv->sta_dz_bitmap))) { /* no any sta in ps mode */
		xmitframe_phead = get_list_head(&psta_bmc->sleep_q);
		xmitframe_plist = get_next(xmitframe_phead);

		while ((!rtw_end_of_queue_search(xmitframe_phead, xmitframe_plist))) {
			pxmitframe = container_of(xmitframe_plist, struct xmit_frame, list);

			xmitframe_plist = get_next(xmitframe_plist);

			rtw_list_delete(&pxmitframe->list);

			psta_bmc->sleepq_len--;
			if (psta_bmc->sleepq_len > 0)
				pxmitframe->attrib.mdata = 1;
			else
				pxmitframe->attrib.mdata = 0;


			pxmitframe->attrib.triggered = 1;
			rtw_hal_xmitframe_enqueue(adapt, pxmitframe);

		}

		if (psta_bmc->sleepq_len == 0) {
			if (rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, 0))
				update_mask |= BIT(1);
			rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, 0);
			rtw_tim_map_clear(adapt, pstapriv->sta_dz_bitmap, 0);
		}

	}

_exit:
	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	if (update_mask) {
		/* update_BCNTIM(adapt); */
		if ((update_mask & (BIT(0) | BIT(1))) == (BIT(0) | BIT(1)))
			_update_beacon(adapt, _TIM_IE_, NULL, true, "clear UC&BMC");
		else if ((update_mask & BIT(1)) == BIT(1))
			_update_beacon(adapt, _TIM_IE_, NULL, true, "clear BMC");
		else
			_update_beacon(adapt, _TIM_IE_, NULL, true, "clear UC");
	}

}

void xmit_delivery_enabled_frames(struct adapter *adapt, struct sta_info *psta)
{
	unsigned long irqL;
	u8 wmmps_ac = 0;
	struct list_head	*xmitframe_plist, *xmitframe_phead;
	struct xmit_frame *pxmitframe = NULL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;


	/* _enter_critical_bh(&psta->sleep_q.lock, &irqL); */
	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	xmitframe_phead = get_list_head(&psta->sleep_q);
	xmitframe_plist = get_next(xmitframe_phead);

	while ((!rtw_end_of_queue_search(xmitframe_phead, xmitframe_plist))) {
		pxmitframe = container_of(xmitframe_plist, struct xmit_frame, list);

		xmitframe_plist = get_next(xmitframe_plist);

		switch (pxmitframe->attrib.priority) {
		case 1:
		case 2:
			wmmps_ac = psta->uapsd_bk & BIT(1);
			break;
		case 4:
		case 5:
			wmmps_ac = psta->uapsd_vi & BIT(1);
			break;
		case 6:
		case 7:
			wmmps_ac = psta->uapsd_vo & BIT(1);
			break;
		case 0:
		case 3:
		default:
			wmmps_ac = psta->uapsd_be & BIT(1);
			break;
		}

		if (!wmmps_ac)
			continue;

		rtw_list_delete(&pxmitframe->list);

		psta->sleepq_len--;
		psta->sleepq_ac_len--;

		if (psta->sleepq_ac_len > 0) {
			pxmitframe->attrib.mdata = 1;
			pxmitframe->attrib.eosp = 0;
		} else {
			pxmitframe->attrib.mdata = 0;
			pxmitframe->attrib.eosp = 1;
		}

		pxmitframe->attrib.triggered = 1;
		rtw_hal_xmitframe_enqueue(adapt, pxmitframe);

		if ((psta->sleepq_ac_len == 0) && (!psta->has_legacy_ac) && (wmmps_ac)) {
			rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, psta->cmn.aid);

			/* upate BCN for TIM IE */
			update_beacon(adapt, _TIM_IE_, NULL, true);
		}
	}
	_exit_critical_bh(&pxmitpriv->lock, &irqL);
	return;
}

void rtw_set_xmit_block(struct adapter *adapt, enum XMIT_BLOCK_REASON reason)
{
	unsigned long irqL;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);

	_enter_critical_bh(&dvobj->xmit_block_lock, &irqL);
	dvobj->xmit_block |= reason;
	_exit_critical_bh(&dvobj->xmit_block_lock, &irqL);
}

void rtw_clr_xmit_block(struct adapter *adapt, enum XMIT_BLOCK_REASON reason)
{
	unsigned long irqL;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);

	_enter_critical_bh(&dvobj->xmit_block_lock, &irqL);
	dvobj->xmit_block &= ~reason;
	_exit_critical_bh(&dvobj->xmit_block_lock, &irqL);
}
bool rtw_is_xmit_blocked(struct adapter *adapt)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);

	return ((dvobj->xmit_block) ? true : false);
}

bool rtw_xmit_ac_blocked(struct adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	struct adapter *iface;
	struct mlme_ext_priv *mlmeext;
	bool blocked = false;
	int i;

	if (rfctl->offch_state != OFFCHS_NONE)
		blocked = true;

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		mlmeext = &iface->mlmeextpriv;

		/* check scan state */
		if (mlmeext_scan_state(mlmeext) != SCAN_DISABLE
			&& mlmeext_scan_state(mlmeext) != SCAN_BACK_OP
		) {
			blocked = true;
			goto exit;
		}

		if (mlmeext_scan_state(mlmeext) == SCAN_BACK_OP
			&& !mlmeext_chk_scan_backop_flags(mlmeext, SS_BACKOP_TX_RESUME)
		) {
			blocked = true;
			goto exit;
		}
	}

exit:
	return blocked;
}

#ifdef CONFIG_TX_AMSDU
void rtw_amsdu_vo_timeout_handler(void *FunctionContext)
{
	struct adapter *adapter = (struct adapter *)FunctionContext;

	adapter->xmitpriv.amsdu_vo_timeout = RTW_AMSDU_TIMER_TIMEOUT;

	tasklet_hi_schedule(&adapter->xmitpriv.xmit_tasklet);
}

void rtw_amsdu_vi_timeout_handler(void *FunctionContext)
{
	struct adapter *adapter = (struct adapter *)FunctionContext;

	adapter->xmitpriv.amsdu_vi_timeout = RTW_AMSDU_TIMER_TIMEOUT;

	tasklet_hi_schedule(&adapter->xmitpriv.xmit_tasklet);
}

void rtw_amsdu_be_timeout_handler(void *FunctionContext)
{
	struct adapter *adapter = (struct adapter *)FunctionContext;

	adapter->xmitpriv.amsdu_be_timeout = RTW_AMSDU_TIMER_TIMEOUT;

	if (printk_ratelimit())
		RTW_INFO("%s Timeout!\n",__func__);

	tasklet_hi_schedule(&adapter->xmitpriv.xmit_tasklet);
}

void rtw_amsdu_bk_timeout_handler(void *FunctionContext)
{
	struct adapter *adapter = (struct adapter *)FunctionContext;

	adapter->xmitpriv.amsdu_bk_timeout = RTW_AMSDU_TIMER_TIMEOUT;

	tasklet_hi_schedule(&adapter->xmitpriv.xmit_tasklet);
}

u8 rtw_amsdu_get_timer_status(struct adapter *adapt, u8 priority)
{
	struct xmit_priv        *pxmitpriv = &adapt->xmitpriv;

	u8 status =  RTW_AMSDU_TIMER_UNSET;

	switch(priority)
	{
		case 1:
		case 2:
			status = pxmitpriv->amsdu_bk_timeout;
			break;
		case 4:
		case 5:
			status = pxmitpriv->amsdu_vi_timeout;
			break;
		case 6:
		case 7:
			status = pxmitpriv->amsdu_vo_timeout;
			break;
		case 0:
		case 3:
		default:
			status = pxmitpriv->amsdu_be_timeout;
			break;
	}
	return status;
}

void rtw_amsdu_set_timer_status(struct adapter *adapt, u8 priority, u8 status)
{
	struct xmit_priv        *pxmitpriv = &adapt->xmitpriv;

	switch(priority)
	{
		case 1:
		case 2:
			pxmitpriv->amsdu_bk_timeout = status;
			break;
		case 4:
		case 5:
			pxmitpriv->amsdu_vi_timeout = status;
			break;
		case 6:
		case 7:
			pxmitpriv->amsdu_vo_timeout = status;
			break;
		case 0:
		case 3:
		default:
			pxmitpriv->amsdu_be_timeout = status;
			break;
	}
}

void rtw_amsdu_set_timer(struct adapter *adapt, u8 priority)
{
	struct xmit_priv        *pxmitpriv = &adapt->xmitpriv;

	_timer* amsdu_timer = NULL;

	switch(priority)
	{
		case 1:
		case 2:
			amsdu_timer = &pxmitpriv->amsdu_bk_timer;
			break;
		case 4:
		case 5:
			amsdu_timer = &pxmitpriv->amsdu_vi_timer;
			break;
		case 6:
		case 7:
			amsdu_timer = &pxmitpriv->amsdu_vo_timer;
			break;
		case 0:
		case 3:
		default:
			amsdu_timer = &pxmitpriv->amsdu_be_timer;
			break;
	}
	_set_timer(amsdu_timer, 1);
}

void rtw_amsdu_cancel_timer(struct adapter *adapt, u8 priority)
{
	struct xmit_priv        *pxmitpriv = &adapt->xmitpriv;
	_timer* amsdu_timer = NULL;

	switch(priority)
	{
		case 1:
		case 2:
			amsdu_timer = &pxmitpriv->amsdu_bk_timer;
			break;
		case 4:
		case 5:
			amsdu_timer = &pxmitpriv->amsdu_vi_timer;
			break;
		case 6:
		case 7:
			amsdu_timer = &pxmitpriv->amsdu_vo_timer;
			break;
		case 0:
		case 3:
		default:
			amsdu_timer = &pxmitpriv->amsdu_be_timer;
			break;
	}
	_cancel_timer_ex(amsdu_timer);
}
#endif /* CONFIG_TX_AMSDU */

void rtw_sctx_init(struct submit_ctx *sctx, int timeout_ms)
{
	sctx->timeout_ms = timeout_ms;
	sctx->submit_time = rtw_get_current_time();
	init_completion(&sctx->done);
	sctx->status = RTW_SCTX_SUBMITTED;
}

int rtw_sctx_wait(struct submit_ctx *sctx, const char *msg)
{
	int ret = _FAIL;
	unsigned long expire;
	int status = 0;

	expire = sctx->timeout_ms ? msecs_to_jiffies(sctx->timeout_ms) : MAX_SCHEDULE_TIMEOUT;
	if (!wait_for_completion_timeout(&sctx->done, expire)) {
		/* timeout, do something?? */
		status = RTW_SCTX_DONE_TIMEOUT;
		RTW_INFO("%s timeout: %s\n", __func__, msg);
	} else
		status = sctx->status;

	if (status == RTW_SCTX_DONE_SUCCESS)
		ret = _SUCCESS;

	return ret;
}

static bool rtw_sctx_chk_waring_status(int status)
{
	switch (status) {
	case RTW_SCTX_DONE_UNKNOWN:
	case RTW_SCTX_DONE_BUF_ALLOC:
	case RTW_SCTX_DONE_BUF_FREE:

	case RTW_SCTX_DONE_DRV_STOP:
	case RTW_SCTX_DONE_DEV_REMOVE:
		return true;
	default:
		return false;
	}
}

void rtw_sctx_done_err(struct submit_ctx **sctx, int status)
{
	if (*sctx) {
		if (rtw_sctx_chk_waring_status(status))
			RTW_INFO("%s status:%d\n", __func__, status);
		(*sctx)->status = status;
		complete(&((*sctx)->done));
		*sctx = NULL;
	}
}

void rtw_sctx_done(struct submit_ctx **sctx)
{
	rtw_sctx_done_err(sctx, RTW_SCTX_DONE_SUCCESS);
}

int rtw_ack_tx_wait(struct xmit_priv *pxmitpriv, u32 timeout_ms)
{
	struct submit_ctx *pack_tx_ops = &pxmitpriv->ack_tx_ops;

	pack_tx_ops->submit_time = rtw_get_current_time();
	pack_tx_ops->timeout_ms = timeout_ms;
	pack_tx_ops->status = RTW_SCTX_SUBMITTED;

	return rtw_sctx_wait(pack_tx_ops, __func__);
}

void rtw_ack_tx_done(struct xmit_priv *pxmitpriv, int status)
{
	struct submit_ctx *pack_tx_ops = &pxmitpriv->ack_tx_ops;

	if (pxmitpriv->ack_tx)
		rtw_sctx_done_err(&pack_tx_ops, status);
	else
		RTW_INFO("%s ack_tx not set\n", __func__);
}
