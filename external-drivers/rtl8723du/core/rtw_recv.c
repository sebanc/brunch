// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_RECV_C_

#include <drv_types.h>
#include <hal_data.h>

static u8 SNAP_ETH_TYPE_APPLETALK_AARP[2] = {0x80, 0xf3};
static u8 SNAP_ETH_TYPE_IPX[2] = {0x81, 0x37};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void rtw_signal_stat_timer_hdl(struct timer_list *t);
#else
void rtw_signal_stat_timer_hdl(void *FunctionContext);
#endif

enum {
	SIGNAL_STAT_CALC_PROFILE_0 = 0,
	SIGNAL_STAT_CALC_PROFILE_1,
	SIGNAL_STAT_CALC_PROFILE_MAX
};

static u8 signal_stat_calc_profile[SIGNAL_STAT_CALC_PROFILE_MAX][2] = {
	{4, 1},	/* Profile 0 => pre_stat : curr_stat = 4 : 1 */
	{3, 7}	/* Profile 1 => pre_stat : curr_stat = 3 : 7 */
};

#ifndef RTW_SIGNAL_STATE_CALC_PROFILE
	#define RTW_SIGNAL_STATE_CALC_PROFILE SIGNAL_STAT_CALC_PROFILE_1
#endif

void _rtw_init_sta_recv_priv(struct sta_recv_priv *psta_recvpriv)
{



	memset((u8 *)psta_recvpriv, 0, sizeof(struct sta_recv_priv));

	spin_lock_init(&psta_recvpriv->lock);

	/* for(i=0; i<MAX_RX_NUMBLKS; i++) */
	/*	_rtw_init_queue(&psta_recvpriv->blk_strms[i]); */

	_rtw_init_queue(&psta_recvpriv->defrag_q);


}

int _rtw_init_recv_priv(struct recv_priv *precvpriv, struct adapter *adapt)
{
	int i;

	union recv_frame *precvframe;
	int	res = _SUCCESS;


	/* We don't need to memset adapt->XXX to zero, because adapter is allocated by vzalloc(). */
	/* memset((unsigned char *)precvpriv, 0, sizeof (struct  recv_priv)); */

	spin_lock_init(&precvpriv->lock);

	_rtw_init_queue(&precvpriv->free_recv_queue);
	_rtw_init_queue(&precvpriv->recv_pending_queue);
	_rtw_init_queue(&precvpriv->uc_swdec_pending_queue);

	precvpriv->adapter = adapt;

	precvpriv->free_recvframe_cnt = NR_RECVFRAME;

	precvpriv->sink_udpport = 0;
	precvpriv->pre_rtp_rxseq = 0;
	precvpriv->cur_rtp_rxseq = 0;

	precvpriv->store_law_data_flag = 0;

	rtw_os_recv_resource_init(precvpriv, adapt);

	precvpriv->pallocated_frame_buf = vzalloc(NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ);

	if (!precvpriv->pallocated_frame_buf) {
		res = _FAIL;
		goto exit;
	}

	precvpriv->precv_frame_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(precvpriv->pallocated_frame_buf), RXFRAME_ALIGN_SZ);

	precvframe = (union recv_frame *) precvpriv->precv_frame_buf;


	for (i = 0; i < NR_RECVFRAME ; i++) {
		INIT_LIST_HEAD(&(precvframe->u.list));

		list_add_tail(&(precvframe->u.list), &(precvpriv->free_recv_queue.queue));

		res = rtw_os_recv_resource_alloc(adapt, precvframe);

		precvframe->u.hdr.len = 0;

		precvframe->u.hdr.adapter = adapt;
		precvframe++;

	}

	ATOMIC_SET(&(precvpriv->rx_pending_cnt), 1);

	sema_init(&precvpriv->allrxreturnevt, 0);

	res = rtw_hal_init_recv_priv(adapt);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	timer_setup(&precvpriv->signal_stat_timer, rtw_signal_stat_timer_hdl, 0);
#else
	rtw_init_timer(&precvpriv->signal_stat_timer, adapt, rtw_signal_stat_timer_hdl, adapt);
#endif

	precvpriv->signal_stat_sampling_interval = 2000; /* ms */
	/* precvpriv->signal_stat_converging_constant = 5000; */ /* ms */

	rtw_set_signal_stat_timer(precvpriv);

exit:

	return res;
}

static void rtw_mfree_recv_priv_lock(struct recv_priv *precvpriv)
{
}

void _rtw_free_recv_priv(struct recv_priv *precvpriv)
{
	struct adapter	*adapt = precvpriv->adapter;


	rtw_free_uc_swdec_pending_queue(adapt);

	rtw_mfree_recv_priv_lock(precvpriv);

	rtw_os_recv_resource_free(precvpriv);

	if (precvpriv->pallocated_frame_buf)
		vfree(precvpriv->pallocated_frame_buf);

	rtw_hal_free_recv_priv(adapt);


}

bool rtw_rframe_del_wfd_ie(union recv_frame *rframe, u8 ies_offset)
{
#define DBG_RFRAME_DEL_WFD_IE 0
	u8 *ies = rframe->u.hdr.rx_data + sizeof(struct rtw_ieee80211_hdr_3addr) + ies_offset;
	uint ies_len_ori = rframe->u.hdr.len - (ies - rframe->u.hdr.rx_data);
	uint ies_len;

	ies_len = rtw_del_wfd_ie(ies, ies_len_ori, DBG_RFRAME_DEL_WFD_IE ? __func__ : NULL);
	rframe->u.hdr.len -= ies_len_ori - ies_len;

	return ies_len_ori != ies_len;
}

union recv_frame *_rtw_alloc_recvframe(struct __queue *pfree_recv_queue)
{

	union recv_frame  *precvframe;
	struct list_head	*plist, *phead;
	struct adapter *adapt;
	struct recv_priv *precvpriv;

	if (_rtw_queue_empty(pfree_recv_queue))
		precvframe = NULL;
	else {
		phead = get_list_head(pfree_recv_queue);

		plist = get_next(phead);

		precvframe = container_of(plist, union recv_frame, u.list);

		rtw_list_delete(&precvframe->u.hdr.list);
		adapt = precvframe->u.hdr.adapter;
		if (adapt) {
			precvpriv = &adapt->recvpriv;
			if (pfree_recv_queue == &precvpriv->free_recv_queue)
				precvpriv->free_recvframe_cnt--;
		}
	}


	return precvframe;

}

union recv_frame *rtw_alloc_recvframe(struct __queue *pfree_recv_queue)
{
	unsigned long irqL;
	union recv_frame  *precvframe;

	_enter_critical_bh(&pfree_recv_queue->lock, &irqL);

	precvframe = _rtw_alloc_recvframe(pfree_recv_queue);

	_exit_critical_bh(&pfree_recv_queue->lock, &irqL);

	return precvframe;
}

void rtw_init_recvframe(union recv_frame *precvframe, struct recv_priv *precvpriv)
{
	/* Perry: This can be removed */
	INIT_LIST_HEAD(&precvframe->u.hdr.list);

	precvframe->u.hdr.len = 0;
}

int rtw_free_recvframe(union recv_frame *precvframe, struct __queue *pfree_recv_queue)
{
	unsigned long irqL;
	struct adapter *adapt = precvframe->u.hdr.adapter;
	struct recv_priv *precvpriv = &adapt->recvpriv;


#ifdef CONFIG_CONCURRENT_MODE
	adapt = GET_PRIMARY_ADAPTER(adapt);
	precvpriv = &adapt->recvpriv;
	pfree_recv_queue = &precvpriv->free_recv_queue;
	precvframe->u.hdr.adapter = adapt;
#endif


	rtw_os_free_recvframe(precvframe);


	_enter_critical_bh(&pfree_recv_queue->lock, &irqL);

	rtw_list_delete(&(precvframe->u.hdr.list));

	precvframe->u.hdr.len = 0;

	list_add_tail(&(precvframe->u.hdr.list), get_list_head(pfree_recv_queue));

	if (adapt) {
		if (pfree_recv_queue == &precvpriv->free_recv_queue)
			precvpriv->free_recvframe_cnt++;
	}

	_exit_critical_bh(&pfree_recv_queue->lock, &irqL);


	return _SUCCESS;

}




int _rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue)
{

	struct adapter *adapt = precvframe->u.hdr.adapter;
	struct recv_priv *precvpriv = &adapt->recvpriv;


	/* INIT_LIST_HEAD(&(precvframe->u.hdr.list)); */
	rtw_list_delete(&(precvframe->u.hdr.list));


	list_add_tail(&(precvframe->u.hdr.list), get_list_head(queue));

	if (adapt) {
		if (queue == &precvpriv->free_recv_queue)
			precvpriv->free_recvframe_cnt++;
	}


	return _SUCCESS;
}

int rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue)
{
	int ret;
	unsigned long irqL;

	/* _spinlock(&pfree_recv_queue->lock); */
	_enter_critical_bh(&queue->lock, &irqL);
	ret = _rtw_enqueue_recvframe(precvframe, queue);
	/* spin_unlock(&pfree_recv_queue->lock); */
	_exit_critical_bh(&queue->lock, &irqL);

	return ret;
}

/*
int	rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue)
{
	return rtw_free_recvframe(precvframe, queue);
}
*/




/*
caller : defrag ; recvframe_chk_defrag in recv_thread  (passive)
pframequeue: defrag_queue : will be accessed in recv_thread  (passive)

using spinlock to protect

*/

void rtw_free_recvframe_queue(struct __queue *pframequeue,  struct __queue *pfree_recv_queue)
{
	union	recv_frame	*precvframe;
	struct list_head	*plist, *phead;

	spin_lock(&pframequeue->lock);

	phead = get_list_head(pframequeue);
	plist = get_next(phead);

	while (!rtw_end_of_queue_search(phead, plist)) {
		precvframe = container_of(plist, union recv_frame, u.list);

		plist = get_next(plist);

		/* rtw_list_delete(&precvframe->u.hdr.list); */ /* will do this in rtw_free_recvframe() */

		rtw_free_recvframe(precvframe, pfree_recv_queue);
	}

	spin_unlock(&pframequeue->lock);


}

u32 rtw_free_uc_swdec_pending_queue(struct adapter *adapter)
{
	u32 cnt = 0;
	union recv_frame *pending_frame;
	while ((pending_frame = rtw_alloc_recvframe(&adapter->recvpriv.uc_swdec_pending_queue))) {
		rtw_free_recvframe(pending_frame, &adapter->recvpriv.free_recv_queue);
		cnt++;
	}

	if (cnt)
		RTW_INFO(FUNC_ADPT_FMT" dequeue %d\n", FUNC_ADPT_ARG(adapter), cnt);

	return cnt;
}


int rtw_enqueue_recvbuf_to_head(struct recv_buf *precvbuf, struct __queue *queue)
{
	unsigned long irqL;

	_enter_critical_bh(&queue->lock, &irqL);

	rtw_list_delete(&precvbuf->list);
	list_add(&precvbuf->list, get_list_head(queue));

	_exit_critical_bh(&queue->lock, &irqL);

	return _SUCCESS;
}

int rtw_enqueue_recvbuf(struct recv_buf *precvbuf, struct __queue *queue)
{
	unsigned long irqL;
	_enter_critical_ex(&queue->lock, &irqL);

	rtw_list_delete(&precvbuf->list);

	list_add_tail(&precvbuf->list, get_list_head(queue));
	_exit_critical_ex(&queue->lock, &irqL);
	return _SUCCESS;

}

struct recv_buf *rtw_dequeue_recvbuf(struct __queue *queue)
{
	unsigned long irqL;
	struct recv_buf *precvbuf;
	struct list_head	*plist, *phead;

	_enter_critical_ex(&queue->lock, &irqL);

	if (_rtw_queue_empty(queue))
		precvbuf = NULL;
	else {
		phead = get_list_head(queue);

		plist = get_next(phead);

		precvbuf = container_of(plist, struct recv_buf, list);

		rtw_list_delete(&precvbuf->list);

	}

	_exit_critical_ex(&queue->lock, &irqL);

	return precvbuf;

}

int recvframe_chkmic(struct adapter *adapter,  union recv_frame *precvframe);
int recvframe_chkmic(struct adapter *adapter,  union recv_frame *precvframe)
{

	int	i, res = _SUCCESS;
	u32	datalen;
	u8	miccode[8];
	u8	bmic_err = false, brpt_micerror = true;
	u8	*pframe, *payload, *pframemic;
	u8	*mickey;
	/* u8	*iv,rxdata_key_idx=0; */
	struct	sta_info		*stainfo;
	struct	rx_pkt_attrib	*prxattrib = &precvframe->u.hdr.attrib;
	struct	security_priv	*psecuritypriv = &adapter->securitypriv;

	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	stainfo = rtw_get_stainfo(&adapter->stapriv , &prxattrib->ta[0]);

	if (prxattrib->encrypt == _TKIP_) {

		/* calculate mic code */
		if (stainfo) {
			if (IS_MCAST(prxattrib->ra)) {
				mickey = &psecuritypriv->dot118021XGrprxmickey[prxattrib->key_index].skey[0];

				if (!psecuritypriv->binstallGrpkey) {
					res = _FAIL;
					RTW_INFO("\n recvframe_chkmic:didn't install group key!!!!!!!!!!\n");
					goto exit;
				}
			} else {
				mickey = &stainfo->dot11tkiprxmickey.skey[0];
			}

			datalen = precvframe->u.hdr.len - prxattrib->hdrlen - prxattrib->iv_len - prxattrib->icv_len - 8; /* icv_len included the mic code */
			pframe = precvframe->u.hdr.rx_data;
			payload = pframe + prxattrib->hdrlen + prxattrib->iv_len;

			rtw_seccalctkipmic(mickey, pframe, payload, datalen , &miccode[0], (unsigned char)prxattrib->priority); /* care the length of the data */

			pframemic = payload + datalen;

			bmic_err = false;

			for (i = 0; i < 8; i++) {
				if (miccode[i] != *(pframemic + i)) {
					bmic_err = true;
				}
			}


			if (bmic_err) {



				/* double check key_index for some timing issue , */
				/* cannot compare with psecuritypriv->dot118021XGrpKeyid also cause timing issue */
				if ((IS_MCAST(prxattrib->ra))  && (prxattrib->key_index != pmlmeinfo->key_index))
					brpt_micerror = false;

				if ((prxattrib->bdecrypted) && (brpt_micerror)) {
					rtw_handle_tkip_mic_err(adapter, stainfo, (u8)IS_MCAST(prxattrib->ra));
					RTW_INFO(" mic error :prxattrib->bdecrypted=%d\n", prxattrib->bdecrypted);
				} else {
					RTW_INFO(" mic error :prxattrib->bdecrypted=%d\n", prxattrib->bdecrypted);
				}

				res = _FAIL;

			} else {
				/* mic checked ok */
				if ((!psecuritypriv->bcheck_grpkey) && (IS_MCAST(prxattrib->ra))) {
					psecuritypriv->bcheck_grpkey = true;
				}
			}

		}

		recvframe_pull_tail(precvframe, 8);

	}

exit:


	return res;

}

/* decrypt and set the ivlen,icvlen of the recv_frame */
union recv_frame *decryptor(struct adapter *adapt, union recv_frame *precv_frame);
union recv_frame *decryptor(struct adapter *adapt, union recv_frame *precv_frame)
{

	struct rx_pkt_attrib *prxattrib = &precv_frame->u.hdr.attrib;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	union recv_frame *return_packet = precv_frame;
	u32	 res = _SUCCESS;


	DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt);


	if (prxattrib->encrypt > 0) {
		u8 *iv = precv_frame->u.hdr.rx_data + prxattrib->hdrlen;
		prxattrib->key_index = (((iv[3]) >> 6) & 0x3) ;

		if (prxattrib->key_index > WEP_KEYS) {
			RTW_INFO("prxattrib->key_index(%d) > WEP_KEYS\n", prxattrib->key_index);

			switch (prxattrib->encrypt) {
			case _WEP40_:
			case _WEP104_:
				prxattrib->key_index = psecuritypriv->dot11PrivacyKeyIndex;
				break;
			case _TKIP_:
			case _AES_:
			default:
				prxattrib->key_index = psecuritypriv->dot118021XGrpKeyid;
				break;
			}
		}
	}

	if (prxattrib->encrypt && !prxattrib->bdecrypted) {
		if (GetFrameType(get_recvframe_data(precv_frame)) == WIFI_DATA
			#ifdef CONFIG_CONCURRENT_MODE
			&& !IS_MCAST(prxattrib->ra) /* bc/mc packets may use sw decryption for concurrent mode */
			#endif
		)
			psecuritypriv->hw_decrypted = false;

		switch (prxattrib->encrypt) {
		case _WEP40_:
		case _WEP104_:
			DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt_wep);
			rtw_wep_decrypt(adapt, (u8 *)precv_frame);
			break;
		case _TKIP_:
			DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt_tkip);
			res = rtw_tkip_decrypt(adapt, (u8 *)precv_frame);
			break;
		case _AES_:
			DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt_aes);
			res = rtw_aes_decrypt(adapt, (u8 *)precv_frame);
			break;
		default:
			break;
		}
	} else if (prxattrib->bdecrypted == 1 && prxattrib->encrypt > 0 &&
		   (psecuritypriv->busetkipkey == 1 ||
		    prxattrib->encrypt != _TKIP_)) {
		DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt_hw);

		psecuritypriv->hw_decrypted = true;
	} else {
		DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt_unknown);
	}

	if (res == _FAIL) {
		rtw_free_recvframe(return_packet, &adapt->recvpriv.free_recv_queue);
		return_packet = NULL;
	} else
		prxattrib->bdecrypted = true;


	return return_packet;

}
/* ###set the security information in the recv_frame */
union recv_frame *portctrl(struct adapter *adapter, union recv_frame *precv_frame);
union recv_frame *portctrl(struct adapter *adapter, union recv_frame *precv_frame)
{
	u8 *psta_addr = NULL;
	u8 *ptr;
	uint  auth_alg;
	struct recv_frame_hdr *pfhdr;
	struct sta_info *psta;
	struct sta_priv *pstapriv ;
	union recv_frame *prtnframe;
	u16	ether_type = 0;
	u16  eapol_type = 0x888e;/* for Funia BD's WPA issue  */
	struct rx_pkt_attrib *pattrib;
	__be16 be_tmp;

	pstapriv = &adapter->stapriv;

	auth_alg = adapter->securitypriv.dot11AuthAlgrthm;

	ptr = get_recvframe_data(precv_frame);
	pfhdr = &precv_frame->u.hdr;
	pattrib = &pfhdr->attrib;
	psta_addr = pattrib->ta;

	prtnframe = NULL;

	psta = rtw_get_stainfo(pstapriv, psta_addr);


	if (auth_alg == dot11AuthAlgrthm_8021X) {
		if ((psta) && (psta->ieee8021x_blocked)) {
			/* blocked */
			/* only accept EAPOL frame */

			prtnframe = precv_frame;

			/* get ether_type */
			ptr = ptr + pfhdr->attrib.hdrlen + pfhdr->attrib.iv_len + LLC_HEADER_SIZE;
			memcpy(&be_tmp, ptr, 2);
			ether_type = ntohs((__be16)be_tmp);

			if (ether_type == eapol_type)
				prtnframe = precv_frame;
			else {
				/* free this frame */
				rtw_free_recvframe(precv_frame, &adapter->recvpriv.free_recv_queue);
				prtnframe = NULL;
			}
		} else {
			/* allowed */
			/* check decryption status, and decrypt the frame if needed */


			prtnframe = precv_frame;
			/* check is the EAPOL frame or not (Rekey) */
			/* if(ether_type == eapol_type){ */
			/* check Rekey */

			/*	prtnframe=precv_frame; */
			/* } */
		}
	} else
		prtnframe = precv_frame;


	return prtnframe;

}

static int recv_decache(union recv_frame *precv_frame, u8 bretry)
{
	struct sta_info *sta = precv_frame->u.hdr.psta;
	struct stainfo_rxcache *prxcache = &sta->sta_recvpriv.rxcache;

	int tid = precv_frame->u.hdr.attrib.priority;
	u16 seq_ctrl = ((le16_to_cpu(precv_frame->u.hdr.attrib.seq_num) & 0xffff) << 4) |
			(precv_frame->u.hdr.attrib.frag_num & 0xf);

	if (tid > 15)
		return _FAIL;

	if (seq_ctrl == prxcache->tid_rxseq[tid]) {
		/* for non-AMPDU case	*/
		sta->sta_stats.duplicate_cnt++;

		if (sta->sta_stats.duplicate_cnt % 100 == 0)
			RTW_INFO("%s: tid=%u seq=%d frag=%d\n", __func__
				, tid, precv_frame->u.hdr.attrib.seq_num
				, precv_frame->u.hdr.attrib.frag_num);

		return _FAIL;
	}

	prxcache->tid_rxseq[tid] = seq_ctrl;


	return _SUCCESS;

}

/* VALID_PN_CHK
 * Return true when PN is legal, otherwise false.
 * Legal PN:
 *	1. If old PN is 0, any PN is legal
 *	2. PN > old PN
 */
#define PN_LESS_CHK(a, b)	(((a-b) & 0x800000000000L) != 0)
#define VALID_PN_CHK(new, old)	(((old) == 0) || PN_LESS_CHK(old, new))
#define CCMPH_2_KEYID(ch)	(((ch) & 0x00000000c0000000L) >> 30)
static int recv_ucast_pn_decache(union recv_frame *precv_frame)
{
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	struct sta_info *sta = precv_frame->u.hdr.psta;
	struct stainfo_rxcache *prxcache = &sta->sta_recvpriv.rxcache;
	u8 *pdata = precv_frame->u.hdr.rx_data;
	int tid = precv_frame->u.hdr.attrib.priority;
	u64 tmp_iv_hdr = 0;
	u64 curr_pn = 0, pkt_pn = 0;

	if (tid > 15)
		return _FAIL;

	if (pattrib->encrypt == _AES_) {
		tmp_iv_hdr = le64_to_cpu(*(__le64*)(pdata + pattrib->hdrlen));
		pkt_pn = CCMPH_2_PN(tmp_iv_hdr);
		tmp_iv_hdr = le64_to_cpu(*(__le64*)prxcache->iv[tid]);
		curr_pn = CCMPH_2_PN(tmp_iv_hdr);

		if (!VALID_PN_CHK(pkt_pn, curr_pn)) {
			/* return _FAIL; */
		} else {
			prxcache->last_tid = tid;
			memcpy(prxcache->iv[tid],
				    (pdata + pattrib->hdrlen),
				    sizeof(prxcache->iv[tid]));
		}
	}

	return _SUCCESS;
}

int recv_bcast_pn_decache(union recv_frame *precv_frame);
int recv_bcast_pn_decache(union recv_frame *precv_frame)
{
	struct adapter *adapt = precv_frame->u.hdr.adapter;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	u8 *pdata = precv_frame->u.hdr.rx_data;
	u64 tmp_iv_hdr = 0;
	u64 curr_pn = 0, pkt_pn = 0;
	u8 key_id;

	if ((pattrib->encrypt == _AES_) &&
		(check_fwstate(pmlmepriv, WIFI_STATION_STATE))) {		

		tmp_iv_hdr = le64_to_cpu(*(__le64*)(pdata + pattrib->hdrlen));
		key_id = CCMPH_2_KEYID(tmp_iv_hdr);
		pkt_pn = CCMPH_2_PN(tmp_iv_hdr);
	
		curr_pn = le64_to_cpu(*(__le64*)psecuritypriv->iv_seq[key_id]);
		curr_pn &= 0x0000ffffffffffffL;

		if (!VALID_PN_CHK(pkt_pn, curr_pn))
			return _FAIL;

		*(__le64*)psecuritypriv->iv_seq[key_id] = cpu_to_le64(pkt_pn);
	}

	return _SUCCESS;
}

static void process_pwrbit_data(struct adapter *adapt, union recv_frame *precv_frame, struct sta_info *psta)
{
	unsigned char pwrbit;
	u8 *ptr = precv_frame->u.hdr.rx_data;

	pwrbit = GetPwrMgt(ptr);
	if (pwrbit) {
		if (!(psta->state & WIFI_SLEEP_STATE)) {
			stop_sta_xmit(adapt, psta);
		}
	} else {
		if (psta->state & WIFI_SLEEP_STATE) {
			wakeup_sta_to_xmit(adapt, psta);
		}
	}
}

static void process_wmmps_data(struct adapter *adapt, union recv_frame *precv_frame, struct sta_info *psta)
{
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;

	if (!psta->qos_option)
		return;

	if (!(psta->qos_info & 0xf))
		return;

	if (psta->state & WIFI_SLEEP_STATE) {
		u8 wmmps_ac = 0;

		switch (pattrib->priority) {
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

		if (wmmps_ac) {
			if (psta->sleepq_ac_len > 0) {
				/* process received triggered frame */
				xmit_delivery_enabled_frames(adapt, psta);
			} else {
				/* issue one qos null frame with More data bit = 0 and the EOSP bit set (=1) */
				issue_qos_nulldata(adapt, psta->cmn.mac_addr, (u16)pattrib->priority, 0, 0);
			}
		}

	}
}

static void count_rx_stats(struct adapter *adapt, union recv_frame *prframe, struct sta_info *sta)
{
	int	sz;
	struct sta_info		*psta = NULL;
	struct stainfo_stats	*pstats = NULL;
	struct rx_pkt_attrib	*pattrib = &prframe->u.hdr.attrib;
	struct recv_priv		*precvpriv = &adapt->recvpriv;

	sz = get_recvframe_len(prframe);
	precvpriv->rx_bytes += sz;

	adapt->mlmepriv.LinkDetectInfo.NumRxOkInPeriod++;

	if ((!MacAddr_isBcst(pattrib->dst)) && (!IS_MCAST(pattrib->dst)))
		adapt->mlmepriv.LinkDetectInfo.NumRxUnicastOkInPeriod++;

	if (sta)
		psta = sta;
	else
		psta = prframe->u.hdr.psta;

	if (psta) {
		u8 is_ra_bmc = IS_MCAST(pattrib->ra);

		pstats = &psta->sta_stats;

		pstats->rx_data_pkts++;
		pstats->rx_bytes += sz;
		if (is_broadcast_mac_addr(pattrib->ra)) {
			pstats->rx_data_bc_pkts++;
			pstats->rx_bc_bytes += sz;
		} else if (is_ra_bmc) {
			pstats->rx_data_mc_pkts++;
			pstats->rx_mc_bytes += sz;
		}

		if (!is_ra_bmc) {
			pstats->rxratecnt[pattrib->data_rate]++;
			/*record rx packets for every tid*/
			pstats->rx_data_qos_pkts[pattrib->priority]++;
		}
	}

#ifdef CONFIG_CHECK_LEAVE_LPS
	traffic_check_for_leave_lps(adapt, false, 0);
#endif

}

static int sta2sta_data_frame(
	struct adapter *adapter,
	union recv_frame *precv_frame,
	struct sta_info **psta
)
{
	u8 *ptr = precv_frame->u.hdr.rx_data;
	int ret = _SUCCESS;
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	struct	sta_priv		*pstapriv = &adapter->stapriv;
	struct	mlme_priv	*pmlmepriv = &adapter->mlmepriv;
	u8 *mybssid  = get_bssid(pmlmepriv);
	u8 *myhwaddr = adapter_mac_addr(adapter);
	u8 *sta_addr = pattrib->ta;
	int bmcast = IS_MCAST(pattrib->dst);

	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))) {

		/* filter packets that SA is myself or multicast or broadcast */
		if (!memcmp(myhwaddr, pattrib->src, ETH_ALEN)) {
			ret = _FAIL;
			goto exit;
		}

		if ((memcmp(myhwaddr, pattrib->dst, ETH_ALEN))	&& (!bmcast)) {
			ret = _FAIL;
			goto exit;
		}

		if (!memcmp(pattrib->bssid, "\x0\x0\x0\x0\x0\x0", ETH_ALEN) ||
		    !memcmp(mybssid, "\x0\x0\x0\x0\x0\x0", ETH_ALEN) ||
		    (memcmp(pattrib->bssid, mybssid, ETH_ALEN))) {
			ret = _FAIL;
			goto exit;
		}

	} else if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		/* For Station mode, sa and bssid should always be BSSID, and DA is my mac-address */
		if (memcmp(pattrib->bssid, pattrib->src, ETH_ALEN)) {
			ret = _FAIL;
			goto exit;
		}
	} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		if (bmcast) {
			/* For AP mode, if DA == MCAST, then BSSID should be also MCAST */
			if (!IS_MCAST(pattrib->bssid)) {
				ret = _FAIL;
				goto exit;
			}
		} else { /* not mc-frame */
			/* For AP mode, if DA is non-MCAST, then it must be BSSID, and bssid == BSSID */
			if (memcmp(pattrib->bssid, pattrib->dst, ETH_ALEN)) {
				ret = _FAIL;
				goto exit;
			}
		}

	} else if (check_fwstate(pmlmepriv, WIFI_MP_STATE)) {
		memcpy(pattrib->dst, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->src, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->bssid, GetAddr3Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);
		memcpy(pattrib->ta, pattrib->src, ETH_ALEN);

		sta_addr = mybssid;
	} else
		ret  = _FAIL;
	*psta = rtw_get_stainfo(pstapriv, sta_addr);
	if (!*psta) {
		ret = _FAIL;
		goto exit;
	}

exit:
	return ret;

}

static int ap2sta_data_frame(
	struct adapter *adapter,
	union recv_frame *precv_frame,
	struct sta_info **psta)
{
	u8 *ptr = precv_frame->u.hdr.rx_data;
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	int ret = _SUCCESS;
	struct	sta_priv		*pstapriv = &adapter->stapriv;
	struct	mlme_priv	*pmlmepriv = &adapter->mlmepriv;
	u8 *mybssid  = get_bssid(pmlmepriv);
	u8 *myhwaddr = adapter_mac_addr(adapter);
	int bmcast = IS_MCAST(pattrib->dst);


	if ((check_fwstate(pmlmepriv, WIFI_STATION_STATE))
	    && (check_fwstate(pmlmepriv, _FW_LINKED)
		|| check_fwstate(pmlmepriv, _FW_UNDER_LINKING))
	   ) {

		/* filter packets that SA is myself or multicast or broadcast */
		if (!memcmp(myhwaddr, pattrib->src, ETH_ALEN)) {
			ret = _FAIL;
			goto exit;
		}

		/* da should be for me */
		if ((memcmp(myhwaddr, pattrib->dst, ETH_ALEN)) && (!bmcast)) {
			ret = _FAIL;
			goto exit;
		}


		/* check BSSID */
		if (!memcmp(pattrib->bssid, "\x0\x0\x0\x0\x0\x0", ETH_ALEN) ||
		    !memcmp(mybssid, "\x0\x0\x0\x0\x0\x0", ETH_ALEN) ||
		    (memcmp(pattrib->bssid, mybssid, ETH_ALEN))) {
			if (!bmcast) {
				RTW_INFO(ADPT_FMT" -issue_deauth to the nonassociated ap=" MAC_FMT " for the reason(7)\n", ADPT_ARG(adapter), MAC_ARG(pattrib->bssid));
				issue_deauth(adapter, pattrib->bssid, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
			}

			ret = _FAIL;
			goto exit;
		}

		*psta = rtw_get_stainfo(pstapriv, pattrib->ta);
		if (!*psta) {
			ret = _FAIL;
			goto exit;
		}

		/*if ((get_frame_sub_type(ptr) & WIFI_QOS_DATA_TYPE) == WIFI_QOS_DATA_TYPE) {
		}
		*/

		if (get_frame_sub_type(ptr) & BIT(6)) {
			/* No data, will not indicate to upper layer, temporily count it here */
			count_rx_stats(adapter, precv_frame, *psta);
			ret = RTW_RX_HANDLED;
			goto exit;
		}

	} else if ((check_fwstate(pmlmepriv, WIFI_MP_STATE)) &&
		   (check_fwstate(pmlmepriv, _FW_LINKED))) {
		memcpy(pattrib->dst, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->src, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->bssid, GetAddr3Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);
		memcpy(pattrib->ta, pattrib->src, ETH_ALEN);


		*psta = rtw_get_stainfo(pstapriv, pattrib->bssid); /* get sta_info */
		if (!*psta) {
			ret = _FAIL;
			goto exit;
		}


	} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		/* Special case */
		ret = RTW_RX_HANDLED;
		goto exit;
	} else {
		if (!memcmp(myhwaddr, pattrib->dst, ETH_ALEN) && (!bmcast)) {
			*psta = rtw_get_stainfo(pstapriv, pattrib->ta);
			if (!*psta) {

				/* for AP multicast issue , modify by yiwei */
				static unsigned long send_issue_deauth_time = 0;

				/* RTW_INFO("After send deauth , %u ms has elapsed.\n", rtw_get_passing_time_ms(send_issue_deauth_time)); */

				if (rtw_get_passing_time_ms(send_issue_deauth_time) > 10000 || send_issue_deauth_time == 0) {
					send_issue_deauth_time = rtw_get_current_time();

					RTW_INFO("issue_deauth to the ap=" MAC_FMT " for the reason(7)\n", MAC_ARG(pattrib->bssid));

					issue_deauth(adapter, pattrib->bssid, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
				}
			}
		}

		ret = _FAIL;
	}

exit:


	return ret;

}

static int sta2ap_data_frame(
	struct adapter *adapter,
	union recv_frame *precv_frame,
	struct sta_info **psta)
{
	u8 *ptr = precv_frame->u.hdr.rx_data;
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	struct	sta_priv		*pstapriv = &adapter->stapriv;
	struct	mlme_priv	*pmlmepriv = &adapter->mlmepriv;
	unsigned char *mybssid  = get_bssid(pmlmepriv);
	int ret = _SUCCESS;


	if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		/* For AP mode, RA=BSSID, TX=STA(SRC_ADDR), A3=DST_ADDR */
		if (memcmp(pattrib->bssid, mybssid, ETH_ALEN)) {
			ret = _FAIL;
			goto exit;
		}

		*psta = rtw_get_stainfo(pstapriv, pattrib->ta);
		if (!*psta) {
			RTW_INFO("issue_deauth to sta=" MAC_FMT " for the reason(7)\n", MAC_ARG(pattrib->src));

			issue_deauth(adapter, pattrib->src, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
			ret = RTW_RX_HANDLED;
			goto exit;
		}

		process_pwrbit_data(adapter, precv_frame, *psta);

		if ((get_frame_sub_type(ptr) & WIFI_QOS_DATA_TYPE) == WIFI_QOS_DATA_TYPE)
			process_wmmps_data(adapter, precv_frame, *psta);

		if (get_frame_sub_type(ptr) & BIT(6)) {
			/* No data, will not indicate to upper layer, temporily count it here */
			count_rx_stats(adapter, precv_frame, *psta);
			ret = RTW_RX_HANDLED;
			goto exit;
		}
	} else if ((check_fwstate(pmlmepriv, WIFI_MP_STATE)) &&
		   (check_fwstate(pmlmepriv, _FW_LINKED))) {
		/* RTW_INFO("%s ,in WIFI_MP_STATE\n",__func__); */
		memcpy(pattrib->dst, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->src, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->bssid, GetAddr3Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);
		memcpy(pattrib->ta, pattrib->src, ETH_ALEN);


		*psta = rtw_get_stainfo(pstapriv, pattrib->bssid); /* get sta_info */
		if (!*psta) {
			ret = _FAIL;
			goto exit;
		}

	} else {
		u8 *myhwaddr = adapter_mac_addr(adapter);
		if (memcmp(pattrib->ra, myhwaddr, ETH_ALEN)) {
			ret = RTW_RX_HANDLED;
			goto exit;
		}
		RTW_INFO("issue_deauth to sta=" MAC_FMT " for the reason(7)\n", MAC_ARG(pattrib->src));
		issue_deauth(adapter, pattrib->src, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
		ret = RTW_RX_HANDLED;
		goto exit;
	}

exit:


	return ret;

}

int validate_recv_ctrl_frame(struct adapter *adapt, union recv_frame *precv_frame);
int validate_recv_ctrl_frame(struct adapter *adapt, union recv_frame *precv_frame)
{
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	struct sta_priv *pstapriv = &adapt->stapriv;
	u8 *pframe = precv_frame->u.hdr.rx_data;
	struct sta_info *psta = NULL;
	/* uint len = precv_frame->u.hdr.len; */

	/* RTW_INFO("+validate_recv_ctrl_frame\n"); */

	if (GetFrameType(pframe) != WIFI_CTRL_TYPE)
		return _FAIL;

	/* receive the frames that ra(a1) is my address */
	if (memcmp(GetAddr1Ptr(pframe), adapter_mac_addr(adapt), ETH_ALEN))
		return _FAIL;

	psta = rtw_get_stainfo(pstapriv, get_addr2_ptr(pframe));
	if (!psta)
		return _FAIL;

	/* for rx pkt statistics */
	psta->sta_stats.rx_ctrl_pkts++;

	/* only handle ps-poll */
	if (get_frame_sub_type(pframe) == WIFI_PSPOLL) {
		u16 aid;
		u8 wmmps_ac = 0;

		aid = GetAid(pframe);
		if (psta->cmn.aid != aid)
			return _FAIL;

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
			return _FAIL;

		if (psta->state & WIFI_STA_ALIVE_CHK_STATE) {
			RTW_INFO("%s alive check-rx ps-poll\n", __func__);
			psta->expire_to = pstapriv->expire_to;
			psta->state ^= WIFI_STA_ALIVE_CHK_STATE;
		}

		if ((psta->state & WIFI_SLEEP_STATE) && (rtw_tim_map_is_set(adapt, pstapriv->sta_dz_bitmap, psta->cmn.aid))) {
			unsigned long irqL;
			struct list_head	*xmitframe_plist, *xmitframe_phead;
			struct xmit_frame *pxmitframe = NULL;
			struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

			/* _enter_critical_bh(&psta->sleep_q.lock, &irqL); */
			_enter_critical_bh(&pxmitpriv->lock, &irqL);

			xmitframe_phead = get_list_head(&psta->sleep_q);
			xmitframe_plist = get_next(xmitframe_phead);

			if ((!rtw_end_of_queue_search(xmitframe_phead, xmitframe_plist))) {
				pxmitframe = container_of(xmitframe_plist, struct xmit_frame, list);

				xmitframe_plist = get_next(xmitframe_plist);

				rtw_list_delete(&pxmitframe->list);

				psta->sleepq_len--;

				if (psta->sleepq_len > 0)
					pxmitframe->attrib.mdata = 1;
				else
					pxmitframe->attrib.mdata = 0;

				pxmitframe->attrib.triggered = 1;

				rtw_hal_xmitframe_enqueue(adapt, pxmitframe);

				if (psta->sleepq_len == 0) {
					rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, psta->cmn.aid);

					/* upate BCN for TIM IE */
					/* update_BCNTIM(adapt);		 */
					update_beacon(adapt, _TIM_IE_, NULL, true);
				}

				_exit_critical_bh(&pxmitpriv->lock, &irqL);

			} else {
				_exit_critical_bh(&pxmitpriv->lock, &irqL);

				if (rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, psta->cmn.aid)) {
					if (psta->sleepq_len == 0) {
						RTW_INFO("no buffered packets to xmit\n");

						/* issue nulldata with More data bit = 0 to indicate we have no buffered packets */
						issue_nulldata(adapt, psta->cmn.mac_addr, 0, 0, 0);
					} else {
						RTW_INFO("error!psta->sleepq_len=%d\n", psta->sleepq_len);
						psta->sleepq_len = 0;
					}

					rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, psta->cmn.aid);

					/* upate BCN for TIM IE */
					update_beacon(adapt, _TIM_IE_, NULL, true);
				}
			}
		}
	} else if (get_frame_sub_type(pframe) == WIFI_NDPA) {
	} else if (get_frame_sub_type(pframe) == WIFI_BAR) {
		rtw_process_bar_frame(adapt, precv_frame);
	}

	return _FAIL;

}

#if defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH)
static int validate_mgmt_protect(struct adapter *adapter, union recv_frame *precv_frame)
{
#define DBG_VALIDATE_MGMT_PROTECT 0
#define DBG_VALIDATE_MGMT_DEC 0

	struct security_priv *sec = &adapter->securitypriv;
	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	struct sta_info	*psta = precv_frame->u.hdr.psta;
	u8 *ptr;
	u8 type;
	u8 subtype;
	u8 is_bmc;
	u8 category = 0xFF;

#ifdef CONFIG_IEEE80211W
	const u8 *igtk;
	u16 igtk_id;
	u64* ipn;
#endif

	u8 *mgmt_DATA;
	u32 data_len = 0;

	int ret;

	if ((!MLME_IS_MESH(adapter) && !SEC_IS_BIP_KEY_INSTALLED(sec))
		#ifdef CONFIG_RTW_MESH
		|| (MLME_IS_MESH(adapter) && !adapter->mesh_info.mesh_auth_id)
		#endif
	)
		return pattrib->privacy ? _FAIL : _SUCCESS;

	ptr = precv_frame->u.hdr.rx_data;
	type = GetFrameType(ptr);
	subtype = get_frame_sub_type(ptr); /* bit(7)~bit(2) */
	is_bmc = IS_MCAST(GetAddr1Ptr(ptr));

#if DBG_VALIDATE_MGMT_PROTECT
	if (subtype == WIFI_DEAUTH) {
		RTW_INFO(FUNC_ADPT_FMT" bmc:%u, deauth, privacy:%u, encrypt:%u, bdecrypted:%u\n"
			, FUNC_ADPT_ARG(adapter)
			, is_bmc, pattrib->privacy, pattrib->encrypt, pattrib->bdecrypted);
	} else if (subtype == WIFI_DISASSOC) {
		RTW_INFO(FUNC_ADPT_FMT" bmc:%u, disassoc, privacy:%u, encrypt:%u, bdecrypted:%u\n"
			, FUNC_ADPT_ARG(adapter)
			, is_bmc, pattrib->privacy, pattrib->encrypt, pattrib->bdecrypted);
	} if (subtype == WIFI_ACTION) {
		if (pattrib->privacy) {
			RTW_INFO(FUNC_ADPT_FMT" bmc:%u, action(?), privacy:%u, encrypt:%u, bdecrypted:%u\n"
				, FUNC_ADPT_ARG(adapter)
				, is_bmc, pattrib->privacy, pattrib->encrypt, pattrib->bdecrypted);
		} else {
			RTW_INFO(FUNC_ADPT_FMT" bmc:%u, action(%u), privacy:%u, encrypt:%u, bdecrypted:%u\n"
				, FUNC_ADPT_ARG(adapter), is_bmc
				, *(ptr + sizeof(struct rtw_ieee80211_hdr_3addr))
				, pattrib->privacy, pattrib->encrypt, pattrib->bdecrypted);
		}
	}
#endif

	if (!pattrib->privacy) {
		if (!psta || !(psta->flags & WLAN_STA_MFP)) {
			/* peer is not MFP capable, no need to check */
			goto exit;
		}

		if (subtype == WIFI_ACTION)
			category = *(ptr + sizeof(struct rtw_ieee80211_hdr_3addr));
	
		if (is_bmc) {
			/* broadcast cases */
			if (subtype == WIFI_ACTION) {
				if (CATEGORY_IS_GROUP_PRIVACY(category)) {
					/* drop broadcast group privacy action frame without encryption */
					#if DBG_VALIDATE_MGMT_PROTECT
					RTW_INFO(FUNC_ADPT_FMT" broadcast gp action(%u) w/o encrypt\n"
						, FUNC_ADPT_ARG(adapter), category);
					#endif
					goto fail;
				}
				if (CATEGORY_IS_ROBUST(category)) {
					/* broadcast robust action frame need BIP check */
					goto bip_verify;
				}
			}
			if (subtype == WIFI_DEAUTH || subtype == WIFI_DISASSOC) {
				/* broadcast deauth or disassoc frame need BIP check */
				goto bip_verify;
			}
			goto exit;

		} else {
			/* unicast cases */
			#ifdef CONFIG_IEEE80211W
			if (subtype == WIFI_DEAUTH || subtype == WIFI_DISASSOC) {
				if (!MLME_IS_MESH(adapter)) {
					unsigned short reason = le16_to_cpu(*(unsigned short *)(ptr + WLAN_HDR_A3_LEN));

					#if DBG_VALIDATE_MGMT_PROTECT
					RTW_INFO(FUNC_ADPT_FMT" unicast %s, reason=%d w/o encrypt\n"
						, FUNC_ADPT_ARG(adapter), subtype == WIFI_DEAUTH ? "deauth" : "disassoc", reason);
					#endif
					if (reason == 6 || reason == 7) {
						/* issue sa query request */
						issue_action_SA_Query(adapter, psta->cmn.mac_addr, 0, 0, IEEE80211W_RIGHT_KEY);
					}
				}
				goto fail;
			}
			#endif

			if (subtype == WIFI_ACTION && CATEGORY_IS_ROBUST(category)) {
				if (psta->bpairwise_key_installed) {
					#if DBG_VALIDATE_MGMT_PROTECT
					RTW_INFO(FUNC_ADPT_FMT" unicast robust action(%d) w/o encrypt\n"
						, FUNC_ADPT_ARG(adapter), category);
					#endif
					goto fail;
				}
			}
			goto exit;
		}

bip_verify:
#ifdef CONFIG_IEEE80211W
		#ifdef CONFIG_RTW_MESH
		if (MLME_IS_MESH(adapter)) {
			if (psta->igtk_bmp) {
				igtk = psta->igtk.skey;
				igtk_id = psta->igtk_id;
				ipn = &psta->igtk_pn.val;
			} else
				goto exit;
		} else
		#endif
		{
			igtk = sec->dot11wBIPKey[sec->dot11wBIPKeyid].skey;
			igtk_id = sec->dot11wBIPKeyid;
			ipn = &sec->dot11wBIPrxpn.val;
		}

		/* verify BIP MME IE */
		ret = rtw_BIP_verify(adapter
			, get_recvframe_data(precv_frame)
			, get_recvframe_len(precv_frame)
			, igtk, igtk_id, ipn);
		if (ret == _FAIL) {
			/* RTW_INFO("802.11w BIP verify fail\n"); */
			goto fail;

		} else if (ret == RTW_RX_HANDLED) {
			#if DBG_VALIDATE_MGMT_PROTECT
			RTW_INFO(FUNC_ADPT_FMT" none protected packet\n", FUNC_ADPT_ARG(adapter));
			#endif
			goto fail;
		}
#endif /* CONFIG_IEEE80211W */
		goto exit;
	}

	/* cases to decrypt mgmt frame */
	pattrib->bdecrypted = 0;
	pattrib->encrypt = _AES_;
	pattrib->hdrlen = sizeof(struct rtw_ieee80211_hdr_3addr);

	/* set iv and icv length */
	SET_ICE_IV_LEN(pattrib->iv_len, pattrib->icv_len, pattrib->encrypt);
	memcpy(pattrib->ra, GetAddr1Ptr(ptr), ETH_ALEN);
	memcpy(pattrib->ta, get_addr2_ptr(ptr), ETH_ALEN);

	/* actual management data frame body */
	data_len = pattrib->pkt_len - pattrib->hdrlen - pattrib->iv_len - pattrib->icv_len;
	mgmt_DATA = rtw_zmalloc(data_len);
	if (!mgmt_DATA) {
		RTW_INFO(FUNC_ADPT_FMT" mgmt allocate fail  !!!!!!!!!\n", FUNC_ADPT_ARG(adapter));
		goto fail;
	}

#if DBG_VALIDATE_MGMT_DEC
	/* dump the packet content before decrypt */
	{
		int pp;

		printk("pattrib->pktlen = %d =>", pattrib->pkt_len);
		for (pp = 0; pp < pattrib->pkt_len; pp++)
		printk(" %02x ", ptr[pp]);
		printk("\n");
	}
#endif

	precv_frame = decryptor(adapter, precv_frame);
	/* save actual management data frame body */
	memcpy(mgmt_DATA, ptr + pattrib->hdrlen + pattrib->iv_len, data_len);
	/* overwrite the iv field */
	memcpy(ptr + pattrib->hdrlen, mgmt_DATA, data_len);
	/* remove the iv and icv length */
	pattrib->pkt_len = pattrib->pkt_len - pattrib->iv_len - pattrib->icv_len;
	rtw_mfree(mgmt_DATA, data_len);

#if DBG_VALIDATE_MGMT_DEC
	/* print packet content after decryption */
	{
		int pp;

		printk("after decryption pattrib->pktlen = %d @@=>", pattrib->pkt_len);
		for (pp = 0; pp < pattrib->pkt_len; pp++)
		printk(" %02x ", ptr[pp]);
		printk("\n");
	}
#endif

	if (!precv_frame) {
		#if DBG_VALIDATE_MGMT_PROTECT
		RTW_INFO(FUNC_ADPT_FMT" mgmt descrypt fail  !!!!!!!!!\n", FUNC_ADPT_ARG(adapter));
		#endif
		goto fail;
	}

exit:
	return _SUCCESS;

fail:
	return _FAIL;

}
#endif /* defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH) */

union recv_frame *recvframe_chk_defrag(struct adapter * adapt, union recv_frame *precv_frame);

static int validate_recv_mgnt_frame(struct adapter * adapt, union recv_frame *precv_frame)
{
	struct sta_info *psta = precv_frame->u.hdr.psta
		= rtw_get_stainfo(&adapt->stapriv, get_addr2_ptr(precv_frame->u.hdr.rx_data));

#if defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH)
	if (validate_mgmt_protect(adapt, precv_frame) == _FAIL) {
		DBG_COUNTER(adapt->rx_logs.core_rx_pre_mgmt_err_80211w);
		goto exit;
	}
#endif

	precv_frame = recvframe_chk_defrag(adapt, precv_frame);
	if (!precv_frame)
		return _SUCCESS;

	/* for rx pkt statistics */
	if (psta) {
		psta->sta_stats.rx_mgnt_pkts++;
		if (get_frame_sub_type(precv_frame->u.hdr.rx_data) == WIFI_BEACON)
			psta->sta_stats.rx_beacon_pkts++;
		else if (get_frame_sub_type(precv_frame->u.hdr.rx_data) == WIFI_PROBEREQ)
			psta->sta_stats.rx_probereq_pkts++;
		else if (get_frame_sub_type(precv_frame->u.hdr.rx_data) == WIFI_PROBERSP) {
			if (!memcmp(adapter_mac_addr(adapt), GetAddr1Ptr(precv_frame->u.hdr.rx_data), ETH_ALEN))
				psta->sta_stats.rx_probersp_pkts++;
			else if (is_broadcast_mac_addr(GetAddr1Ptr(precv_frame->u.hdr.rx_data))
				|| is_multicast_mac_addr(GetAddr1Ptr(precv_frame->u.hdr.rx_data)))
				psta->sta_stats.rx_probersp_bm_pkts++;
			else
				psta->sta_stats.rx_probersp_uo_pkts++;
		}
	}
	mgt_dispatcher(adapt, precv_frame);

	return _SUCCESS;
}

static int validate_recv_data_frame(struct adapter *adapter, union recv_frame *precv_frame)
{
	u8 bretry;
	struct sta_info *psta = NULL;
	u8 *ptr = precv_frame->u.hdr.rx_data;
	struct rx_pkt_attrib	*pattrib = &precv_frame->u.hdr.attrib;
	struct security_priv	*psecuritypriv = &adapter->securitypriv;
	int ret = _SUCCESS;

	bretry = GetRetry(ptr);

	switch (pattrib->to_fr_ds) {
	case 0:
		memcpy(pattrib->ra, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->ta, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->dst, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->src, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->bssid, GetAddr3Ptr(ptr), ETH_ALEN);
		ret = sta2sta_data_frame(adapter, precv_frame, &psta);
		break;

	case 1:
		memcpy(pattrib->ra, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->ta, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->dst, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->src, GetAddr3Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->bssid, get_addr2_ptr(ptr), ETH_ALEN);
		ret = ap2sta_data_frame(adapter, precv_frame, &psta);
		break;

	case 2:
		memcpy(pattrib->ra, GetAddr1Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->ta, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->dst, GetAddr3Ptr(ptr), ETH_ALEN);
		memcpy(pattrib->src, get_addr2_ptr(ptr), ETH_ALEN);
		memcpy(pattrib->bssid, GetAddr1Ptr(ptr), ETH_ALEN);
		ret = sta2ap_data_frame(adapter, precv_frame, &psta);
		break;

	case 3:
	default:
		/* WDS is not supported */
		ret = _FAIL;
		break;
	}

	if (ret == _FAIL) {
		goto exit;
	} else if (ret == RTW_RX_HANDLED)
		goto exit;


	if (!psta) {
		ret = _FAIL;
		goto exit;
	}

	precv_frame->u.hdr.psta = psta;


	pattrib->amsdu = 0;
	pattrib->ack_policy = 0;
	/* parsing QC field */
	if (pattrib->qos == 1) {
		pattrib->priority = GetPriority((ptr + 24));
		pattrib->ack_policy = GetAckpolicy((ptr + 24));
		pattrib->amsdu = GetAMsdu((ptr + 24));
		pattrib->hdrlen = pattrib->to_fr_ds == 3 ? 32 : 26;

		if (pattrib->priority != 0 && pattrib->priority != 3)
			adapter->recvpriv.is_any_non_be_pkts = true;
		else
			adapter->recvpriv.is_any_non_be_pkts = false;
	} else {
		pattrib->priority = 0;
		pattrib->hdrlen = pattrib->to_fr_ds == 3 ? 30 : 24;
	}


	if (pattrib->order) /* HT-CTRL 11n */
		pattrib->hdrlen += 4;

	if (!IS_MCAST(pattrib->ra)) {
		precv_frame->u.hdr.preorder_ctrl = &psta->recvreorder_ctrl[pattrib->priority];

		/* decache, drop duplicate recv packets */
		if ((recv_decache(precv_frame, bretry) == _FAIL) ||
			(recv_ucast_pn_decache(precv_frame) == _FAIL)) {
			ret = _FAIL;
			goto exit;
		}
	} else {
		if (recv_bcast_pn_decache(precv_frame) == _FAIL) {
			ret = _FAIL;
			goto exit;
		}	

		precv_frame->u.hdr.preorder_ctrl = NULL;
	}

	if (pattrib->privacy) {
		GET_ENCRY_ALGO(psecuritypriv, psta, pattrib->encrypt, IS_MCAST(pattrib->ra));
		SET_ICE_IV_LEN(pattrib->iv_len, pattrib->icv_len, pattrib->encrypt);
	} else {
		pattrib->encrypt = 0;
		pattrib->iv_len = pattrib->icv_len = 0;
	}

exit:

	return ret;
}

static inline void dump_rx_packet(u8 *ptr)
{
	int i;

	RTW_INFO("#############################\n");
	for (i = 0; i < 64; i = i + 8)
		RTW_INFO("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:\n", *(ptr + i),
			*(ptr + i + 1), *(ptr + i + 2) , *(ptr + i + 3) , *(ptr + i + 4), *(ptr + i + 5), *(ptr + i + 6), *(ptr + i + 7));
	RTW_INFO("#############################\n");
}

int validate_recv_frame(struct adapter *adapter, union recv_frame *precv_frame);
int validate_recv_frame(struct adapter *adapter, union recv_frame *precv_frame)
{
	/* shall check frame subtype, to / from ds, da, bssid */

	/* then call check if rx seq/frag. duplicated. */

	u8 type;
	u8 subtype;
	int retval = _SUCCESS;

	struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
	struct recv_priv  *precvpriv = &adapter->recvpriv;

	u8 *ptr = precv_frame->u.hdr.rx_data;
	u8  ver = (unsigned char)(*ptr) & 0x3 ;

	/* add version chk */
	if (ver != 0) {
		retval = _FAIL;
		DBG_COUNTER(adapter->rx_logs.core_rx_pre_ver_err);
		goto exit;
	}

	type =  GetFrameType(ptr);
	subtype = get_frame_sub_type(ptr); /* bit(7)~bit(2) */

	pattrib->to_fr_ds = get_tofr_ds(ptr);

	pattrib->frag_num = GetFragNum(ptr);
	pattrib->seq_num = cpu_to_le16(GetSequence(ptr));

	pattrib->pw_save = GetPwrMgt(ptr);
	pattrib->mfrag = GetMFrag(ptr);
	pattrib->mdata = GetMData(ptr);
	pattrib->privacy = GetPrivacy(ptr);
	pattrib->order = GetOrder(ptr);

	{
		u8 bDumpRxPkt = 0;

		rtw_hal_get_def_var(adapter, HAL_DEF_DBG_DUMP_RXPKT, &(bDumpRxPkt));
		if (bDumpRxPkt == 1) /* dump all rx packets */
			dump_rx_packet(ptr);
		else if ((bDumpRxPkt == 2) && (type == WIFI_MGT_TYPE))
			dump_rx_packet(ptr);
		else if ((bDumpRxPkt == 3) && (type == WIFI_DATA_TYPE))
			dump_rx_packet(ptr);
	}
	switch (type) {
	case WIFI_MGT_TYPE: /* mgnt */
		DBG_COUNTER(adapter->rx_logs.core_rx_pre_mgmt);
		retval = validate_recv_mgnt_frame(adapter, precv_frame);
		if (retval == _FAIL) {
			DBG_COUNTER(adapter->rx_logs.core_rx_pre_mgmt_err);
		}
		retval = _FAIL; /* only data frame return _SUCCESS */
		break;
	case WIFI_CTRL_TYPE: /* ctrl */
		DBG_COUNTER(adapter->rx_logs.core_rx_pre_ctrl);
		retval = validate_recv_ctrl_frame(adapter, precv_frame);
		if (retval == _FAIL) {
			DBG_COUNTER(adapter->rx_logs.core_rx_pre_ctrl_err);
		}
		retval = _FAIL; /* only data frame return _SUCCESS */
		break;
	case WIFI_DATA_TYPE: /* data */
		DBG_COUNTER(adapter->rx_logs.core_rx_pre_data);
		pattrib->qos = (subtype & BIT(7)) ? 1 : 0;
		retval = validate_recv_data_frame(adapter, precv_frame);
		if (retval == _FAIL) {
			precvpriv->dbg_rx_drop_count++;
			DBG_COUNTER(adapter->rx_logs.core_rx_pre_data_err);
		} else
			DBG_COUNTER(adapter->rx_logs.core_rx_pre_data_handled);
		break;
	default:
		DBG_COUNTER(adapter->rx_logs.core_rx_pre_unknown);
		retval = _FAIL;
		break;
	}

exit:


	return retval;
}


/* remove the wlanhdr and add the eth_hdr */
static int wlanhdr_to_ethhdr(union recv_frame *precvframe)
{
	int	rmv_len;
	u16	eth_type, len;
	u8	bsnaphdr;
	u8	*psnap_type;
	struct ieee80211_snap_hdr	*psnap;
	__be16 be_tmp;
	int ret = _SUCCESS;
	struct adapter			*adapter = precvframe->u.hdr.adapter;
	struct mlme_priv	*pmlmepriv = &adapter->mlmepriv;

	u8	*ptr = get_recvframe_data(precvframe) ; /* point to frame_ctrl field */
	struct rx_pkt_attrib *pattrib = &precvframe->u.hdr.attrib;


	if (pattrib->encrypt)
		recvframe_pull_tail(precvframe, pattrib->icv_len);

	psnap = (struct ieee80211_snap_hdr *)(ptr + pattrib->hdrlen + pattrib->iv_len);
	psnap_type = ptr + pattrib->hdrlen + pattrib->iv_len + SNAP_SIZE;
	/* convert hdr + possible LLC headers into Ethernet header */
	/* eth_type = (psnap_type[0] << 8) | psnap_type[1]; */
	if ((!memcmp(psnap, rtw_rfc1042_header, SNAP_SIZE) &&
	     (memcmp(psnap_type, SNAP_ETH_TYPE_IPX, 2)) &&
	     (memcmp(psnap_type, SNAP_ETH_TYPE_APPLETALK_AARP, 2))) ||
	    /* eth_type != ETH_P_AARP && eth_type != ETH_P_IPX) || */
	    !memcmp(psnap, rtw_bridge_tunnel_header, SNAP_SIZE)) {
		/* remove RFC1042 or Bridge-Tunnel encapsulation and replace EtherType */
		bsnaphdr = true;
	} else {
		/* Leave Ethernet header part of hdr and full payload */
		bsnaphdr = false;
	}

	rmv_len = pattrib->hdrlen + pattrib->iv_len + (bsnaphdr ? SNAP_SIZE : 0);
	len = precvframe->u.hdr.len - rmv_len;


	memcpy(&be_tmp, ptr + rmv_len, 2);
	eth_type = ntohs(be_tmp); /* pattrib->ether_type */
	pattrib->eth_type = cpu_to_le16(eth_type);


	if ((check_fwstate(pmlmepriv, WIFI_MP_STATE))) {
		ptr += rmv_len ;
		*ptr = 0x87;
		*(ptr + 1) = 0x12;

		eth_type = 0x8712;
		/* append rx status for mp test packets */
		ptr = recvframe_pull(precvframe, (rmv_len - sizeof(struct ethhdr) + 2) - 24);
		if (!ptr) {
			ret = _FAIL;
			goto exiting;
		}
		memcpy(ptr, get_rxmem(precvframe), 24);
		ptr += 24;
	} else {
		ptr = recvframe_pull(precvframe, (rmv_len - sizeof(struct ethhdr) + (bsnaphdr ? 2 : 0)));
		if (!ptr) {
			ret = _FAIL;
			goto exiting;
		}
	}

	if (ptr) {
		memcpy(ptr, pattrib->dst, ETH_ALEN);
		memcpy(ptr + ETH_ALEN, pattrib->src, ETH_ALEN);

		if (!bsnaphdr) {
			__be16 be_len = htons(len);
			memcpy(ptr + 12, &be_len, 2);
		}
	}

exiting:
	return ret;

}

/* perform defrag */
union recv_frame *recvframe_defrag(struct adapter *adapter, struct __queue *defrag_q);
union recv_frame *recvframe_defrag(struct adapter *adapter, struct __queue *defrag_q)
{
	struct list_head	*plist, *phead;
	u8	*data, wlanhdr_offset;
	u8	curfragnum;
	struct recv_frame_hdr *pfhdr, *pnfhdr;
	union recv_frame *prframe, *pnextrframe;
	struct __queue	*pfree_recv_queue;


	curfragnum = 0;
	pfree_recv_queue = &adapter->recvpriv.free_recv_queue;

	phead = get_list_head(defrag_q);
	plist = get_next(phead);
	prframe = container_of(plist, union recv_frame, u.list);
	pfhdr = &prframe->u.hdr;
	rtw_list_delete(&(prframe->u.list));

	if (curfragnum != pfhdr->attrib.frag_num) {
		/* the first fragment number must be 0 */
		/* free the whole queue */
		rtw_free_recvframe(prframe, pfree_recv_queue);
		rtw_free_recvframe_queue(defrag_q, pfree_recv_queue);

		return NULL;
	}

	curfragnum++;

	plist = get_list_head(defrag_q);

	plist = get_next(plist);

	data = get_recvframe_data(prframe);

	while (!rtw_end_of_queue_search(phead, plist)) {
		pnextrframe = container_of(plist, union recv_frame , u.list);
		pnfhdr = &pnextrframe->u.hdr;


		/* check the fragment sequence  (2nd ~n fragment frame) */

		if (curfragnum != pnfhdr->attrib.frag_num) {
			/* the fragment number must be increasing  (after decache) */
			/* release the defrag_q & prframe */
			rtw_free_recvframe(prframe, pfree_recv_queue);
			rtw_free_recvframe_queue(defrag_q, pfree_recv_queue);
			return NULL;
		}

		curfragnum++;

		/* copy the 2nd~n fragment frame's payload to the first fragment */
		/* get the 2nd~last fragment frame's payload */

		wlanhdr_offset = pnfhdr->attrib.hdrlen + pnfhdr->attrib.iv_len;

		recvframe_pull(pnextrframe, wlanhdr_offset);

		/* append  to first fragment frame's tail (if privacy frame, pull the ICV) */
		recvframe_pull_tail(prframe, pfhdr->attrib.icv_len);

		/* memcpy */
		memcpy(pfhdr->rx_tail, pnfhdr->rx_data, pnfhdr->len);

		recvframe_put(prframe, cpu_to_le16(pnfhdr->len));

		pfhdr->attrib.icv_len = pnfhdr->attrib.icv_len;
		plist = get_next(plist);

	};

	/* free the defrag_q queue and return the prframe */
	rtw_free_recvframe_queue(defrag_q, pfree_recv_queue);



	return prframe;
}

/* check if need to defrag, if needed queue the frame to defrag_q */
union recv_frame *recvframe_chk_defrag(struct adapter * adapt, union recv_frame *precv_frame)
{
	u8	ismfrag;
	u8	fragnum;
	u8	*psta_addr;
	struct recv_frame_hdr *pfhdr;
	struct sta_info *psta;
	struct sta_priv *pstapriv;
	struct list_head *phead;
	union recv_frame *prtnframe = NULL;
	struct __queue *pfree_recv_queue, *pdefrag_q;


	pstapriv = &adapt->stapriv;

	pfhdr = &precv_frame->u.hdr;

	pfree_recv_queue = &adapt->recvpriv.free_recv_queue;

	/* need to define struct of wlan header frame ctrl */
	ismfrag = pfhdr->attrib.mfrag;
	fragnum = pfhdr->attrib.frag_num;

	psta_addr = pfhdr->attrib.ta;
	psta = rtw_get_stainfo(pstapriv, psta_addr);
	if (!psta) {
		u8 type = GetFrameType(pfhdr->rx_data);
		if (type != WIFI_DATA_TYPE) {
			psta = rtw_get_bcmc_stainfo(adapt);
			pdefrag_q = &psta->sta_recvpriv.defrag_q;
		} else
			pdefrag_q = NULL;
	} else
		pdefrag_q = &psta->sta_recvpriv.defrag_q;

	if ((ismfrag == 0) && (fragnum == 0)) {
		prtnframe = precv_frame;/* isn't a fragment frame */
	}

	if (ismfrag == 1) {
		/* 0~(n-1) fragment frame */
		/* enqueue to defraf_g */
		if (pdefrag_q) {
			if (fragnum == 0) {
				/* the first fragment */
				if (!_rtw_queue_empty(pdefrag_q)) {
					/* free current defrag_q */
					rtw_free_recvframe_queue(pdefrag_q, pfree_recv_queue);
				}
			}


			/* Then enqueue the 0~(n-1) fragment into the defrag_q */

			/* spin_lock(&pdefrag_q->lock); */
			phead = get_list_head(pdefrag_q);
			list_add_tail(&pfhdr->list, phead);
			/* spin_unlock(&pdefrag_q->lock); */


			prtnframe = NULL;

		} else {
			/* can't find this ta's defrag_queue, so free this recv_frame */
			rtw_free_recvframe(precv_frame, pfree_recv_queue);
			prtnframe = NULL;
		}

	}

	if ((ismfrag == 0) && (fragnum != 0)) {
		/* the last fragment frame */
		/* enqueue the last fragment */
		if (pdefrag_q) {
			/* spin_lock(&pdefrag_q->lock); */
			phead = get_list_head(pdefrag_q);
			list_add_tail(&pfhdr->list, phead);
			/* spin_unlock(&pdefrag_q->lock); */

			/* call recvframe_defrag to defrag */
			precv_frame = recvframe_defrag(adapt, pdefrag_q);
			prtnframe = precv_frame;

		} else {
			/* can't find this ta's defrag_queue, so free this recv_frame */
			rtw_free_recvframe(precv_frame, pfree_recv_queue);
			prtnframe = NULL;
		}

	}


	if ((prtnframe) && (prtnframe->u.hdr.attrib.privacy)) {
		/* after defrag we must check tkip mic code */
		if (recvframe_chkmic(adapt,  prtnframe) == _FAIL) {
			rtw_free_recvframe(prtnframe, pfree_recv_queue);
			prtnframe = NULL;
		}
	}


	return prtnframe;

}

static int rtw_recv_indicatepkt_check(union recv_frame *rframe, u8 *ehdr_pos, u32 pkt_len)
{
	struct adapter *adapter = rframe->u.hdr.adapter;
	struct recv_priv *recvpriv = &adapter->recvpriv;
	struct ethhdr *ehdr = (struct ethhdr *)ehdr_pos;

	if (rframe->u.hdr.psta)
		rtw_st_ctl_rx(rframe->u.hdr.psta, ehdr_pos);

	if (ntohs(ehdr->h_proto) == 0x888e)
		RTW_PRINT("recv eapol packet\n");

	if (recvpriv->sink_udpport > 0)
		rtw_sink_rtp_seq_dbg(adapter, ehdr_pos);

	return _SUCCESS;
}

static int amsdu_to_msdu(struct adapter *adapt, union recv_frame *prframe)
{
	struct rx_pkt_attrib *rattrib = &prframe->u.hdr.attrib;
	int	a_len, padding_len;
	u16	nSubframe_Length;
	u8	nr_subframes, i;
	u8	*pdata;
	struct sk_buff *sub_pkt, *subframes[MAX_SUBFRAME_COUNT];
	struct recv_priv *precvpriv = &adapt->recvpriv;
	struct __queue *pfree_recv_queue = &(precvpriv->free_recv_queue);
	int	ret = _SUCCESS;

	nr_subframes = 0;

	recvframe_pull(prframe, rattrib->hdrlen);

	if (rattrib->iv_len > 0)
		recvframe_pull(prframe, rattrib->iv_len);

	a_len = prframe->u.hdr.len;
	pdata = prframe->u.hdr.rx_data;

	while (a_len > ETH_HLEN) {

		/* Offset 12 denote 2 mac address */
		nSubframe_Length = RTW_GET_BE16(pdata + 12);

		if (a_len < (ETHERNET_HEADER_SIZE + nSubframe_Length)) {
			RTW_INFO("nRemain_Length is %d and nSubframe_Length is : %d\n", a_len, nSubframe_Length);
			break;
		}

		sub_pkt = rtw_os_alloc_msdu_pkt(prframe, nSubframe_Length, pdata);
		if (!sub_pkt) {
			RTW_INFO("%s(): allocate sub packet fail !!!\n", __func__);
			break;
		}

		if (rtw_recv_indicatepkt_check(prframe, sub_pkt->data,
					       sub_pkt->len) == _SUCCESS)
			subframes[nr_subframes++] = sub_pkt;
		else
			dev_kfree_skb_any(sub_pkt);

		/* move the data point to data content */
		pdata += ETH_HLEN;
		a_len -= ETH_HLEN;

		if (nr_subframes >= MAX_SUBFRAME_COUNT) {
			RTW_WARN("ParseSubframe(): Too many Subframes! Packets dropped!\n");
			break;
		}

		pdata += nSubframe_Length;
		a_len -= nSubframe_Length;
		if (a_len != 0) {
			padding_len = 4 - ((nSubframe_Length + ETH_HLEN) & (4 - 1));
			if (padding_len == 4)
				padding_len = 0;

			if (a_len < padding_len) {
				RTW_INFO("ParseSubframe(): a_len < padding_len !\n");
				break;
			}
			pdata += padding_len;
			a_len -= padding_len;
		}
	}

	for (i = 0; i < nr_subframes; i++) {
		sub_pkt = subframes[i];

		/* Indicat the packets to upper layer */
		if (sub_pkt)
			rtw_os_recv_indicate_pkt(adapt, sub_pkt, prframe);
	}

	prframe->u.hdr.len = 0;
	rtw_free_recvframe(prframe, pfree_recv_queue);/* free this recv_frame */

	return ret;
}

static int recv_process_mpdu(struct adapter *adapt, union recv_frame *prframe)
{
	struct __queue *pfree_recv_queue = &adapt->recvpriv.free_recv_queue;
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;
	int ret;

	if (pattrib->amsdu) {
		ret = amsdu_to_msdu(adapt, prframe);
		if (ret != _SUCCESS) {
			rtw_free_recvframe(prframe, pfree_recv_queue);
			goto exit;
		}
	} else {
		ret = wlanhdr_to_ethhdr(prframe);
		if (ret != _SUCCESS) {
			rtw_free_recvframe(prframe, pfree_recv_queue);
			goto exit;
		}

		if (!RTW_CANNOT_RUN(adapt)) {
			ret = rtw_recv_indicatepkt_check(prframe
				, get_recvframe_data(prframe), get_recvframe_len(prframe));
			if (ret != _SUCCESS) {
				rtw_free_recvframe(prframe, pfree_recv_queue);
				goto exit;
			}

			/* indicate this recv_frame */
			ret = rtw_recv_indicatepkt(adapt, prframe);
			if (ret != _SUCCESS) {
				goto exit;
			}
		} else {
			ret = _SUCCESS; /* don't count as packet drop */
			rtw_free_recvframe(prframe, pfree_recv_queue);
		}
	}

exit:
	return ret;
}

static int check_indicate_seq(struct recv_reorder_ctrl *preorder_ctrl, __le16 seq_num)
{
	struct adapter * adapt = preorder_ctrl->adapt;
	struct recv_priv  *precvpriv = &adapt->recvpriv;
	u8	wsize = preorder_ctrl->wsize_b;
	u16	wend = (le16_to_cpu(preorder_ctrl->indicate_seq) + wsize - 1) & 0xFFF; /* % 4096; */

	/* Rx Reorder initialize condition. */
	if (preorder_ctrl->indicate_seq == cpu_to_le16(0xFFFF)) {
		preorder_ctrl->indicate_seq = seq_num;
	}

	/* Drop out the packet which SeqNum is smaller than WinStart */
	if (SN_LESS(seq_num, preorder_ctrl->indicate_seq))
		return false;

	/*
	* Sliding window manipulation. Conditions includes:
	* 1. Incoming SeqNum is equal to WinStart =>Window shift 1
	* 2. Incoming SeqNum is larger than the WinEnd => Window shift N
	*/
	if (SN_EQUAL(seq_num, preorder_ctrl->indicate_seq)) {
		preorder_ctrl->indicate_seq = cpu_to_le16((le16_to_cpu(preorder_ctrl->indicate_seq) + 1) & 0xFFF);
	} else if (SN_LESS(cpu_to_le16(wend), seq_num)) {
		/* boundary situation, when seq_num cross 0xFFF */
		if (le16_to_cpu(seq_num) >= (wsize - 1))
			preorder_ctrl->indicate_seq = cpu_to_le16(le16_to_cpu(seq_num) + 1 - wsize);
		else
			preorder_ctrl->indicate_seq = cpu_to_le16(0xFFF - (wsize - (le16_to_cpu(seq_num) + 1)) + 1);

		precvpriv->dbg_rx_ampdu_window_shift_cnt++;
	}

	return true;
}

static int enqueue_reorder_recvframe(struct recv_reorder_ctrl *preorder_ctrl, union recv_frame *prframe)
{
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;
	struct __queue *ppending_recvframe_queue = &preorder_ctrl->pending_recvframe_queue;
	struct list_head	*phead, *plist;
	union recv_frame *pnextrframe;
	struct rx_pkt_attrib *pnextattrib;

	phead = get_list_head(ppending_recvframe_queue);
	plist = get_next(phead);

	while (!rtw_end_of_queue_search(phead, plist)) {
		pnextrframe = container_of(plist, union recv_frame, u.list);
		pnextattrib = &pnextrframe->u.hdr.attrib;

		if (SN_LESS(pnextattrib->seq_num, pattrib->seq_num))
			plist = get_next(plist);
		else if (SN_EQUAL(pnextattrib->seq_num, pattrib->seq_num)) {
			/* Duplicate entry is found!! Do not insert current entry. */

			/* _exit_critical_ex(&ppending_recvframe_queue->lock, &irql); */

			return false;
		} else
			break;
	}
	rtw_list_delete(&(prframe->u.hdr.list));

	list_add_tail(&(prframe->u.hdr.list), plist);

	return true;
}

static void recv_indicatepkts_pkt_loss_cnt(struct adapter *adapt, u64 prev_seq, u64 current_seq)
{
	struct recv_priv *precvpriv = &adapt->recvpriv;

	if (current_seq < prev_seq) {
		precvpriv->dbg_rx_ampdu_loss_count += (4096 + current_seq - prev_seq);
		precvpriv->rx_drop += (4096 + current_seq - prev_seq);
	} else {
		precvpriv->dbg_rx_ampdu_loss_count += (current_seq - prev_seq);
		precvpriv->rx_drop += (current_seq - prev_seq);
	}
}

static int recv_indicatepkts_in_order(struct adapter *adapt, struct recv_reorder_ctrl *preorder_ctrl, int bforced)
{
	struct list_head	*phead, *plist;
	union recv_frame *prframe;
	struct rx_pkt_attrib *pattrib;
	int bPktInBuf = false;
	struct recv_priv *precvpriv = &adapt->recvpriv;
	struct __queue *ppending_recvframe_queue = &preorder_ctrl->pending_recvframe_queue;

	DBG_COUNTER(adapt->rx_logs.core_rx_post_indicate_in_oder);

	phead =	get_list_head(ppending_recvframe_queue);
	plist = get_next(phead);

	/* Handling some condition for forced indicate case. */
	if (bforced) {
		precvpriv->dbg_rx_ampdu_forced_indicate_count++;
		if (rtw_is_list_empty(phead)) {
			/* _exit_critical_ex(&ppending_recvframe_queue->lock, &irql); */
			/* spin_unlock(&ppending_recvframe_queue->lock); */
			return true;
		}

		prframe = container_of(plist, union recv_frame, u.list);
		pattrib = &prframe->u.hdr.attrib;

		recv_indicatepkts_pkt_loss_cnt(adapt, le16_to_cpu(preorder_ctrl->indicate_seq), le16_to_cpu(pattrib->seq_num));
		preorder_ctrl->indicate_seq = pattrib->seq_num;
	}

	/* Prepare indication list and indication. */
	/* Check if there is any packet need indicate. */
	while (!rtw_is_list_empty(phead)) {

		prframe = container_of(plist, union recv_frame, u.list);
		pattrib = &prframe->u.hdr.attrib;

		if (!SN_LESS(preorder_ctrl->indicate_seq, pattrib->seq_num)) {
			plist = get_next(plist);
			rtw_list_delete(&(prframe->u.hdr.list));

			if (SN_EQUAL(preorder_ctrl->indicate_seq, pattrib->seq_num))
				preorder_ctrl->indicate_seq = cpu_to_le16((le16_to_cpu(preorder_ctrl->indicate_seq) + 1) & 0xFFF);
			if (recv_process_mpdu(adapt, prframe) != _SUCCESS)
				precvpriv->dbg_rx_drop_count++;

			/* Update local variables. */
			bPktInBuf = false;

		} else {
			bPktInBuf = true;
			break;
		}
	}
	/* return true; */
	return bPktInBuf;
}

static int recv_indicatepkt_reorder(struct adapter *adapt, union recv_frame *prframe)
{
	unsigned long irql;
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;
	struct recv_reorder_ctrl *preorder_ctrl = prframe->u.hdr.preorder_ctrl;
	struct __queue *ppending_recvframe_queue = preorder_ctrl ? &preorder_ctrl->pending_recvframe_queue : NULL;
	struct recv_priv  *precvpriv = &adapt->recvpriv;

	if (!pattrib->qos || !preorder_ctrl || !preorder_ctrl->enable)
		goto _success_exit;

	DBG_COUNTER(adapt->rx_logs.core_rx_post_indicate_reoder);

	_enter_critical_bh(&ppending_recvframe_queue->lock, &irql);

	/* s2. check if winstart_b(indicate_seq) needs to been updated */
	if (!check_indicate_seq(preorder_ctrl, pattrib->seq_num)) {
		precvpriv->dbg_rx_ampdu_drop_count++;
		goto _err_exit;
	}


	/* s3. Insert all packet into Reorder Queue to maintain its ordering. */
	if (!enqueue_reorder_recvframe(preorder_ctrl, prframe))
		goto _err_exit;

	/* s4. */
	/* Indication process. */
	/* After Packet dropping and Sliding Window shifting as above, we can now just indicate the packets */
	/* with the SeqNum smaller than latest WinStart and buffer other packets. */
	/*  */
	/* For Rx Reorder condition: */
	/* 1. All packets with SeqNum smaller than WinStart => Indicate */
	/* 2. All packets with SeqNum larger than or equal to WinStart => Buffer it. */
	/*  */

	/* recv_indicatepkts_in_order(adapt, preorder_ctrl, true); */
	if (recv_indicatepkts_in_order(adapt, preorder_ctrl, false)) {
		if (!preorder_ctrl->bReorderWaiting) {
			preorder_ctrl->bReorderWaiting = true;
			_set_timer(&preorder_ctrl->reordering_ctrl_timer, REORDER_WAIT_TIME);
		}
		_exit_critical_bh(&ppending_recvframe_queue->lock, &irql);
	} else {
		preorder_ctrl->bReorderWaiting = false;
		_exit_critical_bh(&ppending_recvframe_queue->lock, &irql);
		_cancel_timer_ex(&preorder_ctrl->reordering_ctrl_timer);
	}

	return RTW_RX_HANDLED;

_success_exit:

	return _SUCCESS;

_err_exit:

	_exit_critical_bh(&ppending_recvframe_queue->lock, &irql);

	return _FAIL;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void rtw_reordering_ctrl_timeout_handler(struct timer_list *t)
#else
void rtw_reordering_ctrl_timeout_handler(void *pcontext)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	struct recv_reorder_ctrl *preorder_ctrl = from_timer(preorder_ctrl, t, reordering_ctrl_timer);
#else
	struct recv_reorder_ctrl *preorder_ctrl = (struct recv_reorder_ctrl *)pcontext;
#endif
	struct adapter *adapt = preorder_ctrl->adapt;
	unsigned long irql;
	struct __queue *ppending_recvframe_queue = &preorder_ctrl->pending_recvframe_queue;


	if (RTW_CANNOT_RUN(adapt))
		return;

	/* RTW_INFO("+rtw_reordering_ctrl_timeout_handler()=>\n"); */

	_enter_critical_bh(&ppending_recvframe_queue->lock, &irql);

	if (preorder_ctrl)
		preorder_ctrl->bReorderWaiting = false;

	if (recv_indicatepkts_in_order(adapt, preorder_ctrl, true))
		_set_timer(&preorder_ctrl->reordering_ctrl_timer, REORDER_WAIT_TIME);

	_exit_critical_bh(&ppending_recvframe_queue->lock, &irql);

}

static void recv_set_iseq_before_mpdu_process(union recv_frame *rframe, __le16 seq_num, const char *caller)
{
	struct recv_reorder_ctrl *reorder_ctrl = rframe->u.hdr.preorder_ctrl;

	if (reorder_ctrl)
		reorder_ctrl->indicate_seq = seq_num;
}

static void recv_set_iseq_after_mpdu_process(union recv_frame *rframe, __le16 seq_num, const char *caller)
{
	struct recv_reorder_ctrl *reorder_ctrl = rframe->u.hdr.preorder_ctrl;

	if (reorder_ctrl)
		reorder_ctrl->indicate_seq = cpu_to_le16((le16_to_cpu(reorder_ctrl->indicate_seq) + 1) % 4096);
}

static int fill_radiotap_hdr(struct adapter *adapt, union recv_frame *precvframe, u8 *buf)
{
#define CHAN2FREQ(a) ((a < 14) ? (2407+5*a) : (5000+5*a))

#ifndef IEEE80211_RADIOTAP_RX_FLAGS
#define IEEE80211_RADIOTAP_RX_FLAGS 14
#endif

#ifndef IEEE80211_RADIOTAP_MCS
#define IEEE80211_RADIOTAP_MCS 19
#endif
#ifndef IEEE80211_RADIOTAP_VHT
#define IEEE80211_RADIOTAP_VHT 21
#endif

#ifndef IEEE80211_RADIOTAP_F_BADFCS
#define IEEE80211_RADIOTAP_F_BADFCS 0x40 /* bad FCS */
#endif

	int ret = _SUCCESS;
	struct rx_pkt_attrib *pattrib = &precvframe->u.hdr.attrib;
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	u16 tmp_16bit = 0;
	__le16 le_tmp;
	u8 data_rate[] = {
		2, 4, 11, 22, /* CCK */
		12, 18, 24, 36, 48, 72, 93, 108, /* OFDM */
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, /* HT MCS index */
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, /* VHT Nss 1 */
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, /* VHT Nss 2 */
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, /* VHT Nss 3 */
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, /* VHT Nss 4 */
	};
	struct sk_buff *pskb = NULL;
	struct ieee80211_radiotap_header *rtap_hdr = NULL;
	u8 *ptr = NULL;
	u8 hdr_buf[64] = {0};
	u16 rt_len = 8;

	/* create header */
	rtap_hdr = (struct ieee80211_radiotap_header *)&hdr_buf[0];
	rtap_hdr->it_version = PKTHDR_RADIOTAP_VERSION;

	/* tsft */
	if (pattrib->tsfl) {
		__le64 tmp_64bit;

		rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_TSFT);
		tmp_64bit = cpu_to_le64(le32_to_cpu(pattrib->tsfl));
		memcpy(&hdr_buf[rt_len], &tmp_64bit, 8);
		rt_len += 8;
	}

	/* flags */
	rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_FLAGS);

	if ((pattrib->encrypt == 1) || (pattrib->encrypt == 5))
		hdr_buf[rt_len] |= IEEE80211_RADIOTAP_F_WEP;

	if (pattrib->mfrag)
		hdr_buf[rt_len] |= IEEE80211_RADIOTAP_F_FRAG;

	/* always append FCS */
	hdr_buf[rt_len] |= IEEE80211_RADIOTAP_F_FCS;

	if (pattrib->crc_err)
		hdr_buf[rt_len] |= IEEE80211_RADIOTAP_F_BADFCS;

	if (pattrib->sgi) {
		/* Currently unspecified but used */
		hdr_buf[rt_len] |= 0x80;
	}
	rt_len += 1;

	/* rate */
	if (pattrib->data_rate < 12) {
		rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
		if (pattrib->data_rate < 4) {
			/* CCK */
			hdr_buf[rt_len] = data_rate[pattrib->data_rate];
		} else {
			/* OFDM */
			hdr_buf[rt_len] = data_rate[pattrib->data_rate];
		}
	}
	rt_len += 1; /* force padding 1 byte for aligned */

	/* channel */
	tmp_16bit = 0;
	rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_CHANNEL);
	tmp_16bit = CHAN2FREQ(rtw_get_oper_ch(adapt));
	/*tmp_16bit = CHAN2FREQ(pHalData->current_channel);*/
	memcpy(&hdr_buf[rt_len], &tmp_16bit, 2);
	rt_len += 2;

	/* channel flags */
	le_tmp = 0;
	if (pHalData->current_band_type == 0)
		le_tmp |= cpu_to_le16(IEEE80211_CHAN_2GHZ);
	else
		le_tmp |= cpu_to_le16(IEEE80211_CHAN_5GHZ);

	if (pattrib->data_rate < 12) {
		if (pattrib->data_rate < 4) {
			/* CCK */
			le_tmp |= cpu_to_le16(IEEE80211_CHAN_CCK);
		} else {
			/* OFDM */
			le_tmp |= cpu_to_le16(IEEE80211_CHAN_OFDM);
		}
	} else
		le_tmp |= cpu_to_le16(IEEE80211_CHAN_DYN);
	memcpy(&hdr_buf[rt_len], &le_tmp, 2);
	rt_len += 2;

	/* dBm Antenna Signal */
	rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL);
	hdr_buf[rt_len] = pattrib->phy_info.recv_signal_power;
	rt_len += 1;

	/* Antenna */
	rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_ANTENNA);
	hdr_buf[rt_len] = 0; /* pHalData->rf_type; */
	rt_len += 1;

	/* RX flags */
	rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RX_FLAGS);
	rt_len += 2;

	/* MCS information */
	if (pattrib->data_rate >= 12 && pattrib->data_rate < 44) {
		rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_MCS);
		/* known, flag */
		hdr_buf[rt_len] |= BIT1; /* MCS index known */

		/* bandwidth */
		hdr_buf[rt_len] |= BIT0;
		hdr_buf[rt_len + 1] |= (pattrib->bw & 0x03);

		/* guard interval */
		hdr_buf[rt_len] |= BIT2;
		hdr_buf[rt_len + 1] |= (pattrib->sgi & 0x01) << 2;

		/* STBC */
		hdr_buf[rt_len] |= BIT5;
		hdr_buf[rt_len + 1] |= (pattrib->stbc & 0x03) << 5;

		rt_len += 2;

		/* MCS rate index */
		hdr_buf[rt_len] = data_rate[pattrib->data_rate];
		rt_len += 1;
	}

	/* VHT */
	if (pattrib->data_rate >= 44 && pattrib->data_rate < 84) {
		rtap_hdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_VHT);

		/* known 16 bit, flag 8 bit */
		tmp_16bit = 0;

		/* Bandwidth */
		tmp_16bit |= BIT6;

		/* Group ID */
		tmp_16bit |= BIT7;

		/* Partial AID */
		tmp_16bit |= BIT8;

		/* STBC */
		tmp_16bit |= BIT0;
		hdr_buf[rt_len + 2] |= (pattrib->stbc & 0x01);

		/* Guard interval */
		tmp_16bit |= BIT2;
		hdr_buf[rt_len + 2] |= (pattrib->sgi & 0x01) << 2;

		/* LDPC extra OFDM symbol */
		tmp_16bit |= BIT4;
		hdr_buf[rt_len + 2] |= (pattrib->ldpc & 0x01) << 4;

		memcpy(&hdr_buf[rt_len], &tmp_16bit, 2);
		rt_len += 3;

		/* bandwidth */
		if (pattrib->bw == 0)
			hdr_buf[rt_len] |= 0;
		else if (pattrib->bw == 1)
			hdr_buf[rt_len] |= 1;
		else if (pattrib->bw == 2)
			hdr_buf[rt_len] |= 4;
		else if (pattrib->bw == 3)
			hdr_buf[rt_len] |= 11;
		rt_len += 1;

		/* mcs_nss */
		if (pattrib->data_rate >= 44 && pattrib->data_rate < 54) {
			hdr_buf[rt_len] |= 1;
			hdr_buf[rt_len] |= data_rate[pattrib->data_rate] << 4;
		} else if (pattrib->data_rate >= 54 && pattrib->data_rate < 64) {
			hdr_buf[rt_len + 1] |= 2;
			hdr_buf[rt_len + 1] |= data_rate[pattrib->data_rate] << 4;
		} else if (pattrib->data_rate >= 64 && pattrib->data_rate < 74) {
			hdr_buf[rt_len + 2] |= 3;
			hdr_buf[rt_len + 2] |= data_rate[pattrib->data_rate] << 4;
		} else if (pattrib->data_rate >= 74 && pattrib->data_rate < 84) {
			hdr_buf[rt_len + 3] |= 4;
			hdr_buf[rt_len + 3] |= data_rate[pattrib->data_rate] << 4;
		}
		rt_len += 4;

		/* coding */
		hdr_buf[rt_len] = 0;
		rt_len += 1;

		/* group_id */
		hdr_buf[rt_len] = 0;
		rt_len += 1;

		/* partial_aid */
		tmp_16bit = 0;
		memcpy(&hdr_buf[rt_len], &tmp_16bit, 2);
		rt_len += 2;
	}

	/* push to skb */
	pskb = (struct sk_buff *)buf;
	if (skb_headroom(pskb) < rt_len) {
		RTW_INFO("%s:%d %s headroom is too small.\n", __FILE__, __LINE__, __func__);
		ret = _FAIL;
		return ret;
	}

	ptr = skb_push(pskb, rt_len);
	if (ptr) {
		rtap_hdr->it_len = cpu_to_le16(rt_len);
		memcpy(ptr, rtap_hdr, rt_len);
	} else
		ret = _FAIL;

	return ret;

}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
static int recv_frame_monitor(struct adapter *adapt, union recv_frame *rframe)
{
	int ret = _SUCCESS;
	struct __queue *pfree_recv_queue = &adapt->recvpriv.free_recv_queue;
	struct sk_buff *pskb = NULL;

	/* read skb information from recv frame */
	pskb = rframe->u.hdr.pkt;
	pskb->len = rframe->u.hdr.len;
	pskb->data = rframe->u.hdr.rx_data;
	skb_set_tail_pointer(pskb, rframe->u.hdr.len);

	/* fill radiotap header */
	if (fill_radiotap_hdr(adapt, rframe, (u8 *)pskb) == _FAIL) {
		ret = _FAIL;
		rtw_free_recvframe(rframe, pfree_recv_queue); /* free this recv_frame */
		goto exit;
	}

	/* write skb information to recv frame */
	skb_reset_mac_header(pskb);
	rframe->u.hdr.len = pskb->len;
	rframe->u.hdr.rx_data = pskb->data;
	rframe->u.hdr.rx_head = pskb->head;
	rframe->u.hdr.rx_tail = skb_tail_pointer(pskb);
	rframe->u.hdr.rx_end = skb_end_pointer(pskb);

	if (!RTW_CANNOT_RUN(adapt)) {
		/* indicate this recv_frame */
		ret = rtw_recv_monitor(adapt, rframe);
		if (ret != _SUCCESS) {
			ret = _FAIL;
			rtw_free_recvframe(rframe, pfree_recv_queue); /* free this recv_frame */
			goto exit;
		}
	} else {
		ret = _FAIL;
		rtw_free_recvframe(rframe, pfree_recv_queue); /* free this recv_frame */
		goto exit;
	}

exit:
	return ret;
}
#endif
static int recv_func_prehandle(struct adapter *adapt, union recv_frame *rframe)
{
	int ret = _SUCCESS;
	struct __queue *pfree_recv_queue = &adapt->recvpriv.free_recv_queue;

	/* check the frame crtl field and decache */
	ret = validate_recv_frame(adapt, rframe);
	if (ret != _SUCCESS) {
		rtw_free_recvframe(rframe, pfree_recv_queue);/* free this recv_frame */
		goto exit;
	}
exit:
	return ret;
}

static int recv_func_posthandle(struct adapter *adapt, union recv_frame *prframe)
{
	int ret = _SUCCESS;
	union recv_frame *orig_prframe = prframe;
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;
	struct recv_priv *precvpriv = &adapt->recvpriv;
	struct __queue *pfree_recv_queue = &adapt->recvpriv.free_recv_queue;

	DBG_COUNTER(adapt->rx_logs.core_rx_post);

	/* DATA FRAME */
	rtw_led_control(adapt, LED_CTL_RX);

	prframe = decryptor(adapt, prframe);
	if (!prframe) {
		ret = _FAIL;
		DBG_COUNTER(adapt->rx_logs.core_rx_post_decrypt_err);
		goto _recv_data_drop;
	}

	prframe = recvframe_chk_defrag(adapt, prframe);
	if (!prframe)	{
		DBG_COUNTER(adapt->rx_logs.core_rx_post_defrag_err);
		goto _recv_data_drop;
	}

	prframe = portctrl(adapt, prframe);
	if (!prframe) {
		ret = _FAIL;
		DBG_COUNTER(adapt->rx_logs.core_rx_post_portctrl_err);
		goto _recv_data_drop;
	}

	count_rx_stats(adapt, prframe, NULL);

	/* including perform A-MPDU Rx Ordering Buffer Control */
	ret = recv_indicatepkt_reorder(adapt, prframe);
	if (ret == _FAIL) {
		rtw_free_recvframe(orig_prframe, pfree_recv_queue);
		goto _recv_data_drop;
	} else if (ret == RTW_RX_HANDLED) /* queued OR indicated in order */
		goto _exit_recv_func;

	recv_set_iseq_before_mpdu_process(prframe, pattrib->seq_num, __func__);
	ret = recv_process_mpdu(adapt, prframe);
	recv_set_iseq_after_mpdu_process(prframe, pattrib->seq_num, __func__);
	if (ret == _FAIL)
		goto _recv_data_drop;

_exit_recv_func:
	return ret;

_recv_data_drop:
	precvpriv->dbg_rx_drop_count++;
	return ret;
}

static int recv_func(struct adapter *adapt, union recv_frame *rframe)
{
	int ret;
	struct rx_pkt_attrib *prxattrib = &rframe->u.hdr.attrib;
	struct recv_priv *recvpriv = &adapt->recvpriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;
	struct mlme_priv *mlmepriv = &adapt->mlmepriv;

	if (check_fwstate(mlmepriv, WIFI_MONITOR_STATE)) {
		/* monitor mode */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
		recv_frame_monitor(adapt, rframe);
#endif
		ret = _SUCCESS;
		goto exit;
	} else

		/* check if need to handle uc_swdec_pending_queue*/
		if (check_fwstate(mlmepriv, WIFI_STATION_STATE) && psecuritypriv->busetkipkey) {
			union recv_frame *pending_frame;
			int cnt = 0;

			while ((pending_frame = rtw_alloc_recvframe(&adapt->recvpriv.uc_swdec_pending_queue))) {
				cnt++;
				DBG_COUNTER(adapt->rx_logs.core_rx_dequeue);
				recv_func_posthandle(adapt, pending_frame);
			}

			if (cnt)
				RTW_INFO(FUNC_ADPT_FMT" dequeue %d from uc_swdec_pending_queue\n",
					 FUNC_ADPT_ARG(adapt), cnt);
		}

	DBG_COUNTER(adapt->rx_logs.core_rx);
	ret = recv_func_prehandle(adapt, rframe);

	if (ret == _SUCCESS) {

		/* check if need to enqueue into uc_swdec_pending_queue*/
		if (check_fwstate(mlmepriv, WIFI_STATION_STATE) &&
		    !IS_MCAST(prxattrib->ra) && prxattrib->encrypt > 0 &&
		    (prxattrib->bdecrypted == 0 || psecuritypriv->sw_decrypt) &&
		    psecuritypriv->ndisauthtype == Ndis802_11AuthModeWPAPSK &&
		    !psecuritypriv->busetkipkey) {
			DBG_COUNTER(adapt->rx_logs.core_rx_enqueue);
			rtw_enqueue_recvframe(rframe, &adapt->recvpriv.uc_swdec_pending_queue);
			/* RTW_INFO("%s: no key, enqueue uc_swdec_pending_queue\n", __func__); */

			if (recvpriv->free_recvframe_cnt < NR_RECVFRAME / 4) {
				/* to prevent from recvframe starvation, get recvframe from uc_swdec_pending_queue to free_recvframe_cnt */
				rframe = rtw_alloc_recvframe(&adapt->recvpriv.uc_swdec_pending_queue);
				if (rframe)
					goto do_posthandle;
			}
			goto exit;
		}

do_posthandle:
		ret = recv_func_posthandle(adapt, rframe);
	}

exit:
	return ret;
}


int rtw_recv_entry(union recv_frame *precvframe)
{
	struct adapter *adapt;
	struct recv_priv *precvpriv;
	int ret = _SUCCESS;



	adapt = precvframe->u.hdr.adapter;

	precvpriv = &adapt->recvpriv;


	ret = recv_func(adapt, precvframe);
	if (ret == _FAIL) {
		goto _recv_entry_drop;
	}


	precvpriv->rx_pkts++;


	return ret;

_recv_entry_drop:

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void rtw_signal_stat_timer_hdl(struct timer_list *t)
#else
void rtw_signal_stat_timer_hdl(void *FunctionContext)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	struct adapter *adapter = from_timer(adapter, t, recvpriv.signal_stat_timer);
#else
	struct adapter *adapter = (struct adapter *)FunctionContext;
#endif
	struct recv_priv *recvpriv = &adapter->recvpriv;

	u32 tmp_s, tmp_q;
	u8 avg_signal_strength = 0;
	u8 avg_signal_qual = 0;
	u32 num_signal_strength = 0;
	u32 num_signal_qual = 0;
	u8 ratio_pre_stat = 0, ratio_curr_stat = 0, ratio_total = 0, ratio_profile = SIGNAL_STAT_CALC_PROFILE_0;

	if (adapter->recvpriv.is_signal_dbg) {
		/* update the user specific value, signal_strength_dbg, to signal_strength, rssi */
		adapter->recvpriv.signal_strength = adapter->recvpriv.signal_strength_dbg;
		adapter->recvpriv.rssi = (s8)translate_percentage_to_dbm((u8)adapter->recvpriv.signal_strength_dbg);
	} else {

		if (recvpriv->signal_strength_data.update_req == 0) { /* update_req is clear, means we got rx */
			avg_signal_strength = recvpriv->signal_strength_data.avg_val;
			num_signal_strength = recvpriv->signal_strength_data.total_num;
			/* after avg_vals are accquired, we can re-stat the signal values */
			recvpriv->signal_strength_data.update_req = 1;
		}

		if (recvpriv->signal_qual_data.update_req == 0) { /* update_req is clear, means we got rx */
			avg_signal_qual = recvpriv->signal_qual_data.avg_val;
			num_signal_qual = recvpriv->signal_qual_data.total_num;
			/* after avg_vals are accquired, we can re-stat the signal values */
			recvpriv->signal_qual_data.update_req = 1;
		}

		if (num_signal_strength == 0) {
			if (rtw_get_on_cur_ch_time(adapter) == 0
			    || rtw_get_passing_time_ms(rtw_get_on_cur_ch_time(adapter)) < 2 * adapter->mlmeextpriv.mlmext_info.bcn_interval
			   )
				goto set_timer;
		}

		if (check_fwstate(&adapter->mlmepriv, _FW_UNDER_SURVEY)
		    || !check_fwstate(&adapter->mlmepriv, _FW_LINKED)
		   )
			goto set_timer;

#ifdef CONFIG_CONCURRENT_MODE
		if (rtw_mi_buddy_check_fwstate(adapter, _FW_UNDER_SURVEY))
			goto set_timer;
#endif

		if (RTW_SIGNAL_STATE_CALC_PROFILE < SIGNAL_STAT_CALC_PROFILE_MAX)
			ratio_profile = RTW_SIGNAL_STATE_CALC_PROFILE;

		ratio_pre_stat = signal_stat_calc_profile[ratio_profile][0];
		ratio_curr_stat = signal_stat_calc_profile[ratio_profile][1];
		ratio_total = ratio_pre_stat + ratio_curr_stat;

		/* update value of signal_strength, rssi, signal_qual */
		tmp_s = (ratio_curr_stat * avg_signal_strength + ratio_pre_stat * recvpriv->signal_strength);
		if (tmp_s % ratio_total)
			tmp_s = tmp_s / ratio_total + 1;
		else
			tmp_s = tmp_s / ratio_total;
		if (tmp_s > 100)
			tmp_s = 100;

		tmp_q = (ratio_curr_stat * avg_signal_qual + ratio_pre_stat * recvpriv->signal_qual);
		if (tmp_q % ratio_total)
			tmp_q = tmp_q / ratio_total + 1;
		else
			tmp_q = tmp_q / ratio_total;
		if (tmp_q > 100)
			tmp_q = 100;

		recvpriv->signal_strength = tmp_s;
		recvpriv->rssi = (s8)translate_percentage_to_dbm(tmp_s);
		recvpriv->signal_qual = tmp_q;
	}

set_timer:
	rtw_set_signal_stat_timer(recvpriv);
}

static void rx_process_rssi(struct adapter *adapt, union recv_frame *prframe)
{
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;
	struct signal_stat *signal_stat = &adapt->recvpriv.signal_strength_data;

	if (signal_stat->update_req) {
		signal_stat->total_num = 0;
		signal_stat->total_val = 0;
		signal_stat->update_req = 0;
	}

	signal_stat->total_num++;
	signal_stat->total_val  += pattrib->phy_info.signal_strength;
	signal_stat->avg_val = signal_stat->total_val / signal_stat->total_num;
}

static void rx_process_link_qual(struct adapter *adapt, union recv_frame *prframe)
{
	struct rx_pkt_attrib *pattrib;
	struct signal_stat *signal_stat;

	if (!prframe || !adapt)
		return;

	pattrib = &prframe->u.hdr.attrib;
	signal_stat = &adapt->recvpriv.signal_qual_data;

	if (signal_stat->update_req) {
		signal_stat->total_num = 0;
		signal_stat->total_val = 0;
		signal_stat->update_req = 0;
	}
	signal_stat->total_num++;
	signal_stat->total_val  += pattrib->phy_info.signal_quality;
	signal_stat->avg_val = signal_stat->total_val / signal_stat->total_num;
}

static void rx_process_phy_info(struct adapter *adapt, union recv_frame *rframe)
{
	/* Check RSSI */
	rx_process_rssi(adapt, rframe);

	/* Check EVM */
	rx_process_link_qual(adapt, rframe);
	rtw_store_phy_info(adapt, rframe);
}

void rx_query_phy_status(
	union recv_frame	*precvframe,
	u8 *pphy_status)
{
	struct adapter *			adapt = precvframe->u.hdr.adapter;
	struct rx_pkt_attrib	*pattrib = &precvframe->u.hdr.attrib;
	struct hal_com_data		*pHalData = GET_HAL_DATA(adapt);
	struct phydm_phyinfo_struct *p_phy_info = &pattrib->phy_info;
	u8					*wlanhdr;
	struct phydm_perpkt_info_struct pkt_info;
	u8 *ta, *ra;
	u8 is_ra_bmc;
	struct sta_priv *pstapriv;
	struct sta_info *psta = NULL;
	struct recv_priv  *precvpriv = &adapt->recvpriv;
	/* unsigned long		irqL; */

	pkt_info.is_packet_match_bssid = false;
	pkt_info.is_packet_to_self = false;
	pkt_info.is_packet_beacon = false;
	pkt_info.ppdu_cnt = pattrib->ppdu_cnt;
	pkt_info.station_id = 0xFF;

	wlanhdr = get_recvframe_data(precvframe);

	ta = rtw_get_ta(wlanhdr);
	ra = rtw_get_ra(wlanhdr);
	is_ra_bmc = IS_MCAST(ra);

	if (!memcmp(adapter_mac_addr(adapt), ta, ETH_ALEN)) {
		static unsigned long start_time = 0;

		if ((start_time == 0) || (rtw_get_passing_time_ms(start_time) > 5000)) {
			RTW_PRINT("Warning!!! %s: Confilc mac addr!!\n", __func__);
			start_time = rtw_get_current_time();
		}
		precvpriv->dbg_rx_conflic_mac_addr_cnt++;
	} else {
		pstapriv = &adapt->stapriv;
		psta = rtw_get_stainfo(pstapriv, ta);
		if (psta)
			pkt_info.station_id = psta->cmn.mac_id;
	}

	pkt_info.is_packet_match_bssid = (!IsFrameTypeCtrl(wlanhdr))
		&& (!pattrib->icv_err) && (!pattrib->crc_err)
		&& !memcmp(get_hdr_bssid(wlanhdr), get_bssid(&adapt->mlmepriv), ETH_ALEN);

	pkt_info.is_to_self = (!pattrib->icv_err) && (!pattrib->crc_err)
		&& !memcmp(ra, adapter_mac_addr(adapt), ETH_ALEN);

	pkt_info.is_packet_to_self = pkt_info.is_packet_match_bssid
		&& !memcmp(ra, adapter_mac_addr(adapt), ETH_ALEN);

	pkt_info.is_packet_beacon = pkt_info.is_packet_match_bssid
				 && (get_frame_sub_type(wlanhdr) == WIFI_BEACON);

	if (psta && IsFrameTypeData(wlanhdr)) {
		if (is_ra_bmc)
			psta->curr_rx_rate_bmc = pattrib->data_rate;
		else
			psta->curr_rx_rate = pattrib->data_rate;
	}
	pkt_info.data_rate = pattrib->data_rate;

	odm_phy_status_query(&pHalData->odmpriv, p_phy_info, pphy_status, &pkt_info);

	{
		precvframe->u.hdr.psta = NULL;
		if ((!MLME_IS_MESH(adapt) && pkt_info.is_packet_match_bssid)
			|| adapt->registrypriv.mp_mode == 1
		) {
			if (psta) {
				precvframe->u.hdr.psta = psta;
				rx_process_phy_info(adapt, precvframe);
			}
		} else if (pkt_info.is_packet_to_self || pkt_info.is_packet_beacon) {

			if (psta)
				precvframe->u.hdr.psta = psta;
			rx_process_phy_info(adapt, precvframe);
		}
	}

	rtw_odm_parse_rx_phy_status_chinfo(precvframe, pphy_status);
}
/*
* Increase and check if the continual_no_rx_packet of this @param pmlmepriv is larger than MAX_CONTINUAL_NORXPACKET_COUNT
* @return true:
* @return false:
*/
int rtw_inc_and_chk_continual_no_rx_packet(struct sta_info *sta, int tid_index)
{

	int ret = false;
	int value = ATOMIC_INC_RETURN(&sta->continual_no_rx_packet[tid_index]);

	if (value >= MAX_CONTINUAL_NORXPACKET_COUNT)
		ret = true;

	return ret;
}

/*
* Set the continual_no_rx_packet of this @param pmlmepriv to 0
*/
void rtw_reset_continual_no_rx_packet(struct sta_info *sta, int tid_index)
{
	ATOMIC_SET(&sta->continual_no_rx_packet[tid_index], 0);
}

u8 adapter_allow_bmc_data_rx(struct adapter *adapter)
{
	if (check_fwstate(&adapter->mlmepriv, WIFI_MONITOR_STATE | WIFI_MP_STATE))
		return 1;

	if (MLME_IS_AP(adapter))
		return 0;

	if (!rtw_linked_check(adapter))
		return 0;

	return 1;
}

int pre_recv_entry(union recv_frame *precvframe, u8 *pphy_status)
{
	int ret = _SUCCESS;
	u8 *pbuf = precvframe->u.hdr.rx_data;
	u8 *pda = rtw_get_ra(pbuf);
	u8 ra_is_bmc = IS_MCAST(pda);
#ifdef CONFIG_CONCURRENT_MODE
	struct adapter *iface = NULL;
	struct adapter *primary_adapt = precvframe->u.hdr.adapter;

	if (!ra_is_bmc) { /*unicast packets*/
		iface = rtw_get_iface_by_macddr(primary_adapt , pda);
		if (!iface) {
			RTW_INFO("%s [WARN] Cannot find appropriate adapter - mac_addr : "MAC_FMT"\n", __func__, MAC_ARG(pda));
			/*rtw_warn_on(1);*/
		} else
			precvframe->u.hdr.adapter = iface;
	} else   /* Handle BC/MC Packets	*/
		rtw_mi_buddy_clone_bcmc_packet(primary_adapt, precvframe, pphy_status);
#endif /* CONFIG_CONCURRENT_MODE */

	/* skip unnecessary bmc data frame for primary adapter */
	if (ra_is_bmc && GetFrameType(pbuf) == WIFI_DATA_TYPE
		&& !adapter_allow_bmc_data_rx(precvframe->u.hdr.adapter)
	) {
		rtw_free_recvframe(precvframe, &precvframe->u.hdr.adapter->recvpriv.free_recv_queue);
		goto exit;
	}

	if (pphy_status)
		rx_query_phy_status(precvframe, pphy_status);
	ret = rtw_recv_entry(precvframe);

exit:
	return ret;
}
