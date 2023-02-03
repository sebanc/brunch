// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_STA_MGT_C_

#include <drv_types.h>

inline void rtw_st_ctl_init(struct st_ctl_t *st_ctl)
{
	memset(st_ctl->reg, 0 , sizeof(struct st_register) * SESSION_TRACKER_REG_ID_NUM);
	_rtw_init_queue(&st_ctl->tracker_q);
}

inline void rtw_st_ctl_clear_tracker_q(struct st_ctl_t *st_ctl)
{
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct session_tracker *st;

	_enter_critical_bh(&st_ctl->tracker_q.lock, &irqL);
	phead = &st_ctl->tracker_q.queue;
	plist = get_next(phead);
	while (!rtw_end_of_queue_search(phead, plist)) {
		st = container_of(plist, struct session_tracker, list);
		plist = get_next(plist);
		rtw_list_delete(&st->list);
		rtw_mfree((u8 *)st, sizeof(struct session_tracker));
	}
	_exit_critical_bh(&st_ctl->tracker_q.lock, &irqL);
}

inline void rtw_st_ctl_deinit(struct st_ctl_t *st_ctl)
{
	rtw_st_ctl_clear_tracker_q(st_ctl);
}

inline void rtw_st_ctl_register(struct st_ctl_t *st_ctl, u8 st_reg_id, struct st_register *reg)
{
	if (st_reg_id >= SESSION_TRACKER_REG_ID_NUM) {
		rtw_warn_on(1);
		return;
	}

	st_ctl->reg[st_reg_id].s_proto = reg->s_proto;
	st_ctl->reg[st_reg_id].rule = reg->rule;
}

inline void rtw_st_ctl_unregister(struct st_ctl_t *st_ctl, u8 st_reg_id)
{
	int i;

	if (st_reg_id >= SESSION_TRACKER_REG_ID_NUM) {
		rtw_warn_on(1);
		return;
	}

	st_ctl->reg[st_reg_id].s_proto = 0;
	st_ctl->reg[st_reg_id].rule = NULL;

	/* clear tracker queue if no session trecker registered */
	for (i = 0; i < SESSION_TRACKER_REG_ID_NUM; i++)
		if (st_ctl->reg[i].s_proto != 0)
			break;
	if (i >= SESSION_TRACKER_REG_ID_NUM)
		rtw_st_ctl_clear_tracker_q(st_ctl);
}

inline bool rtw_st_ctl_chk_reg_s_proto(struct st_ctl_t *st_ctl, u8 s_proto)
{
	bool ret = false;
	int i;

	for (i = 0; i < SESSION_TRACKER_REG_ID_NUM; i++) {
		if (st_ctl->reg[i].s_proto == s_proto) {
			ret = true;
			break;
		}
	}

	return ret;
}

inline bool rtw_st_ctl_chk_reg_rule(struct st_ctl_t *st_ctl, struct adapter *adapter, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port)
{
	bool ret = false;
	int i;
	st_match_rule rule;

	for (i = 0; i < SESSION_TRACKER_REG_ID_NUM; i++) {
		rule = st_ctl->reg[i].rule;
		if (rule && rule(adapter, local_naddr, local_port, remote_naddr, remote_port)) {
			ret = true;
			break;
		}
	}

	return ret;
}

void rtw_st_ctl_rx(struct sta_info *sta, u8 *ehdr_pos)
{
	struct adapter *adapter = sta->adapt;
	struct ethhdr *etherhdr = (struct ethhdr *)ehdr_pos;

	if (ntohs(etherhdr->h_proto) == ETH_P_IP) {
		u8 *ip = ehdr_pos + ETH_HLEN;

		if (GET_IPV4_PROTOCOL(ip) == 0x06  /* TCP */
			&& rtw_st_ctl_chk_reg_s_proto(&sta->st_ctl, 0x06)
		) {
			u8 *tcp = ip + GET_IPV4_IHL(ip) * 4;

			if (rtw_st_ctl_chk_reg_rule(&sta->st_ctl, adapter, IPV4_DST(ip), TCP_DST(tcp), IPV4_SRC(ip), TCP_SRC(tcp))) {
				if (GET_TCP_SYN(tcp) && GET_TCP_ACK(tcp)) {
					session_tracker_add_cmd(adapter, sta
						, IPV4_DST(ip), TCP_DST(tcp)
						, IPV4_SRC(ip), TCP_SRC(tcp));
					if (DBG_SESSION_TRACKER)
						RTW_INFO(FUNC_ADPT_FMT" local:"IP_FMT":"PORT_FMT", remote:"IP_FMT":"PORT_FMT" SYN-ACK\n"
							, FUNC_ADPT_ARG(adapter)
							, IP_ARG(IPV4_DST(ip)), PORT_ARG(TCP_DST(tcp))
							, IP_ARG(IPV4_SRC(ip)), PORT_ARG(TCP_SRC(tcp)));
				}
				if (GET_TCP_FIN(tcp)) {
					session_tracker_del_cmd(adapter, sta
						, IPV4_DST(ip), TCP_DST(tcp)
						, IPV4_SRC(ip), TCP_SRC(tcp));
					if (DBG_SESSION_TRACKER)
						RTW_INFO(FUNC_ADPT_FMT" local:"IP_FMT":"PORT_FMT", remote:"IP_FMT":"PORT_FMT" FIN\n"
							, FUNC_ADPT_ARG(adapter)
							, IP_ARG(IPV4_DST(ip)), PORT_ARG(TCP_DST(tcp))
							, IP_ARG(IPV4_SRC(ip)), PORT_ARG(TCP_SRC(tcp)));
				}
			}

		}
	}
}

#define SESSION_TRACKER_FMT IP_FMT":"PORT_FMT" "IP_FMT":"PORT_FMT" %u %d"
#define SESSION_TRACKER_ARG(st) IP_ARG(&(st)->local_naddr), PORT_ARG(&(st)->local_port), IP_ARG(&(st)->remote_naddr), PORT_ARG(&(st)->remote_port), (st)->status, rtw_get_passing_time_ms((st)->set_time)

void dump_st_ctl(void *sel, struct st_ctl_t *st_ctl)
{
	int i;
	unsigned long irqL;
	struct list_head *plist, *phead;
	struct session_tracker *st;

	if (!DBG_SESSION_TRACKER)
		return;

	for (i = 0; i < SESSION_TRACKER_REG_ID_NUM; i++)
		RTW_PRINT_SEL(sel, "reg%d: %u %p\n", i, st_ctl->reg[i].s_proto, st_ctl->reg[i].rule);

	_enter_critical_bh(&st_ctl->tracker_q.lock, &irqL);
	phead = &st_ctl->tracker_q.queue;
	plist = get_next(phead);
	while (!rtw_end_of_queue_search(phead, plist)) {
		st = container_of(plist, struct session_tracker, list);
		plist = get_next(plist);

		RTW_PRINT_SEL(sel, SESSION_TRACKER_FMT"\n", SESSION_TRACKER_ARG(st));
	}
	_exit_critical_bh(&st_ctl->tracker_q.lock, &irqL);

}

void _rtw_init_stainfo(struct sta_info *psta);
void _rtw_init_stainfo(struct sta_info *psta)
{
	memset((u8 *)psta, 0, sizeof(struct sta_info));

	spin_lock_init(&psta->lock);
	INIT_LIST_HEAD(&psta->list);
	INIT_LIST_HEAD(&psta->hash_list);
	/* INIT_LIST_HEAD(&psta->asoc_list); */
	/* INIT_LIST_HEAD(&psta->sleep_list); */
	/* INIT_LIST_HEAD(&psta->wakeup_list);	 */

	_rtw_init_queue(&psta->sleep_q);

	_rtw_init_sta_xmit_priv(&psta->sta_xmitpriv);
	_rtw_init_sta_recv_priv(&psta->sta_recvpriv);

	INIT_LIST_HEAD(&psta->asoc_list);
	INIT_LIST_HEAD(&psta->auth_list);
	psta->bpairwise_key_installed = false;

#ifdef CONFIG_RTW_80211R
	psta->ft_pairwise_key_installed = false;
#endif

	rtw_st_ctl_init(&psta->st_ctl);
}

u32	_rtw_init_sta_priv(struct	sta_priv *pstapriv)
{
	struct adapter *adapter = container_of(pstapriv, struct adapter, stapriv);
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);
	struct sta_info *psta;
	int i;
	u32 ret = _FAIL;
	u16 sz = 0;

	pstapriv->adapt = adapter;

	pstapriv->pallocated_stainfo_buf = vzalloc(sizeof(struct sta_info) * NUM_STA + 4);
	if (!pstapriv->pallocated_stainfo_buf)
		goto exit;

	pstapriv->pstainfo_buf = pstapriv->pallocated_stainfo_buf + 4 -
			 ((SIZE_PTR)(pstapriv->pallocated_stainfo_buf) & 3);

	_rtw_init_queue(&pstapriv->free_sta_queue);

	spin_lock_init(&pstapriv->sta_hash_lock);

	/* _rtw_init_queue(&pstapriv->asoc_q); */
	pstapriv->asoc_sta_count = 0;
	_rtw_init_queue(&pstapriv->sleep_q);
	_rtw_init_queue(&pstapriv->wakeup_q);

	psta = (struct sta_info *)(pstapriv->pstainfo_buf);


	for (i = 0; i < NUM_STA; i++) {
		_rtw_init_stainfo(psta);

		INIT_LIST_HEAD(&(pstapriv->sta_hash[i]));

		list_add_tail(&psta->list, get_list_head(&pstapriv->free_sta_queue));

		psta++;
	}

	pstapriv->adhoc_expire_to = 4; /* 4 * 2 = 8 sec */

	pstapriv->max_aid = macid_ctl->num;
	pstapriv->rr_aid = 0;
	pstapriv->started_aid = 1;
	sz = pstapriv->max_aid * sizeof(struct sta_info *);
	pstapriv->sta_aid = rtw_zmalloc(sz);
	if (!pstapriv->sta_aid)
		goto exit;
	pstapriv->aid_bmp_len = AID_BMP_LEN(pstapriv->max_aid);
	pstapriv->sta_dz_bitmap = rtw_zmalloc(pstapriv->aid_bmp_len);
	if (!pstapriv->sta_dz_bitmap)
		goto exit;
	pstapriv->tim_bitmap = rtw_zmalloc(pstapriv->aid_bmp_len);
	if (!pstapriv->tim_bitmap)
		goto exit;

	INIT_LIST_HEAD(&pstapriv->asoc_list);
	INIT_LIST_HEAD(&pstapriv->auth_list);
	spin_lock_init(&pstapriv->asoc_list_lock);
	spin_lock_init(&pstapriv->auth_list_lock);
	pstapriv->asoc_list_cnt = 0;
	pstapriv->auth_list_cnt = 0;

	pstapriv->auth_to = 3; /* 3*2 = 6 sec */
	pstapriv->assoc_to = 3;
	/* pstapriv->expire_to = 900; */ /* 900*2 = 1800 sec = 30 min, expire after no any traffic. */
	/* pstapriv->expire_to = 30; */ /* 30*2 = 60 sec = 1 min, expire after no any traffic. */
	pstapriv->expire_to = 3; /* 3*2 = 6 sec */
	pstapriv->max_num_sta = NUM_STA;

#if CONFIG_RTW_MACADDR_ACL
	_rtw_init_queue(&(pstapriv->acl_list.acl_node_q));
#endif

#if CONFIG_RTW_PRE_LINK_STA
	rtw_pre_link_sta_ctl_init(pstapriv);
#endif

	ret = _SUCCESS;

exit:
	if (ret != _SUCCESS) {
		if (pstapriv->pallocated_stainfo_buf)
			vfree(pstapriv->pallocated_stainfo_buf);
		if (pstapriv->sta_aid)
			rtw_mfree(pstapriv->sta_aid, sz);
		if (pstapriv->sta_dz_bitmap)
			rtw_mfree(pstapriv->sta_dz_bitmap, pstapriv->aid_bmp_len);
	}

	return ret;
}

inline int rtw_stainfo_offset(struct sta_priv *stapriv, struct sta_info *sta)
{
	int offset = (((u8 *)sta) - stapriv->pstainfo_buf) / sizeof(struct sta_info);

	if (!stainfo_offset_valid(offset))
		RTW_INFO("%s invalid offset(%d), out of range!!!", __func__, offset);

	return offset;
}

inline struct sta_info *rtw_get_stainfo_by_offset(struct sta_priv *stapriv, int offset)
{
	if (!stainfo_offset_valid(offset))
		RTW_INFO("%s invalid offset(%d), out of range!!!", __func__, offset);

	return (struct sta_info *)(stapriv->pstainfo_buf + offset * sizeof(struct sta_info));
}

void	_rtw_free_sta_xmit_priv_lock(struct sta_xmit_priv *psta_xmitpriv);
void	_rtw_free_sta_xmit_priv_lock(struct sta_xmit_priv *psta_xmitpriv)
{
}

static void	_rtw_free_sta_recv_priv_lock(struct sta_recv_priv *psta_recvpriv)
{
}

void rtw_mfree_stainfo(struct sta_info *psta);
void rtw_mfree_stainfo(struct sta_info *psta)
{
	_rtw_free_sta_xmit_priv_lock(&psta->sta_xmitpriv);
	_rtw_free_sta_recv_priv_lock(&psta->sta_recvpriv);
}

/* this function is used to free the memory of lock || sema for all stainfos */
void rtw_mfree_all_stainfo(struct sta_priv *pstapriv);
void rtw_mfree_all_stainfo(struct sta_priv *pstapriv)
{
	unsigned long	 irqL;
	struct list_head	*plist, *phead;
	struct sta_info *psta = NULL;


	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	phead = get_list_head(&pstapriv->free_sta_queue);
	plist = get_next(phead);

	while ((!rtw_end_of_queue_search(phead, plist))) {
		psta = container_of(plist, struct sta_info , list);
		plist = get_next(plist);

		rtw_mfree_stainfo(psta);
	}

	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);


}

void rtw_mfree_sta_priv_lock(struct	sta_priv *pstapriv);
void rtw_mfree_sta_priv_lock(struct	sta_priv *pstapriv)
{
	rtw_mfree_all_stainfo(pstapriv); /* be done before free sta_hash_lock */
}

u32	_rtw_free_sta_priv(struct	sta_priv *pstapriv)
{
	unsigned long	irqL;
	struct list_head	*phead, *plist;
	struct sta_info *psta = NULL;
	struct recv_reorder_ctrl *preorder_ctrl;
	int	index;
	u16 sz = 0;

	if (pstapriv) {

		/*	delete all reordering_ctrl_timer		*/
		_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
		for (index = 0; index < NUM_STA; index++) {
			phead = &(pstapriv->sta_hash[index]);
			plist = get_next(phead);

			while ((!rtw_end_of_queue_search(phead, plist))) {
				int i;
				psta = container_of(plist, struct sta_info , hash_list);
				plist = get_next(plist);

				for (i = 0; i < 16 ; i++) {
					preorder_ctrl = &psta->recvreorder_ctrl[i];
					_cancel_timer_ex(&preorder_ctrl->reordering_ctrl_timer);
				}
			}
		}
		_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);
		/*===============================*/

		rtw_mfree_sta_priv_lock(pstapriv);

#if CONFIG_RTW_PRE_LINK_STA
		rtw_pre_link_sta_ctl_deinit(pstapriv);
#endif

		if (pstapriv->pallocated_stainfo_buf)
			vfree(pstapriv->pallocated_stainfo_buf);
		sz = pstapriv->max_aid * sizeof(struct sta_info *);
		if (pstapriv->sta_aid)
			rtw_mfree(pstapriv->sta_aid, sz);
		if (pstapriv->sta_dz_bitmap)
			rtw_mfree(pstapriv->sta_dz_bitmap, pstapriv->aid_bmp_len);
		if (pstapriv->tim_bitmap)
			rtw_mfree(pstapriv->tim_bitmap, pstapriv->aid_bmp_len);
	}
	return _SUCCESS;
}


static void rtw_init_recv_timer(struct recv_reorder_ctrl *preorder_ctrl)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	timer_setup(&preorder_ctrl->reordering_ctrl_timer, rtw_reordering_ctrl_timeout_handler, 0);
#else
	rtw_init_timer(&(preorder_ctrl->reordering_ctrl_timer), preorder_ctrl->adapt, rtw_reordering_ctrl_timeout_handler, preorder_ctrl);
#endif
}

/* struct	sta_info *rtw_alloc_stainfo(struct __queue *pfree_sta_queue, unsigned char *hwaddr) */
struct	sta_info *rtw_alloc_stainfo(struct	sta_priv *pstapriv, const u8 *hwaddr)
{
	unsigned long irqL2;
	int	index;
	struct list_head	*phash_list;
	struct sta_info	*psta;
	struct __queue *pfree_sta_queue;
	struct recv_reorder_ctrl *preorder_ctrl;
	int i = 0;
	u16  wRxSeqInitialValue = 0xffff;


	pfree_sta_queue = &pstapriv->free_sta_queue;

	_enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL2);
	if (_rtw_queue_empty(pfree_sta_queue)) {
		psta = NULL;
	} else {
		psta = container_of(get_next(&pfree_sta_queue->queue), struct sta_info, list);

		rtw_list_delete(&(psta->list));

		/* _exit_critical_bh(&(pfree_sta_queue->lock), &irqL); */
		_rtw_init_stainfo(psta);

		psta->adapt = pstapriv->adapt;

		memcpy(psta->cmn.mac_addr, hwaddr, ETH_ALEN);

		index = wifi_mac_hash(hwaddr);

		if (index >= NUM_STA) {
			psta = NULL;
			goto exit;
		}
		phash_list = &(pstapriv->sta_hash[index]);

		list_add_tail(&psta->hash_list, phash_list);

		pstapriv->asoc_sta_count++;

		/* Commented by Albert 2009/08/13
		 * For the SMC router, the sequence number of first packet of WPS handshake will be 0.
		 * In this case, this packet will be dropped by recv_decache function if we use the 0x00 as the default value for tid_rxseq variable.
		 * So, we initialize the tid_rxseq variable as the 0xffff. */

		for (i = 0; i < 16; i++) {
			memcpy(&psta->sta_recvpriv.rxcache.tid_rxseq[i], &wRxSeqInitialValue, 2);
			memset(&psta->sta_recvpriv.rxcache.iv[i], 0, sizeof(psta->sta_recvpriv.rxcache.iv[i]));
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
		timer_setup(&psta->addba_retry_timer, addba_timer_hdl, 0);
#ifdef CONFIG_IEEE80211W
		timer_setup(&psta->dot11w_expire_timer, sa_query_timer_hdl, 0);
#endif /* CONFIG_IEEE80211W */
#else
		rtw_init_timer(&psta->addba_retry_timer, psta->adapt, addba_timer_hdl, psta);
#ifdef CONFIG_IEEE80211W
		rtw_init_timer(&psta->dot11w_expire_timer, psta->adapt, sa_query_timer_hdl, psta);
#endif /* CONFIG_IEEE80211W */
#endif
		/* for A-MPDU Rx reordering buffer control */
		for (i = 0; i < 16 ; i++) {
			preorder_ctrl = &psta->recvreorder_ctrl[i];
			preorder_ctrl->adapt = pstapriv->adapt;
			preorder_ctrl->tid = i;
			preorder_ctrl->enable = false;
			preorder_ctrl->indicate_seq = cpu_to_le16(0xffff);
			preorder_ctrl->wend_b = cpu_to_le16(0xffff);
			/* preorder_ctrl->wsize_b = (NR_RECVBUFF-2); */
			preorder_ctrl->wsize_b = 64;/* 64; */
			preorder_ctrl->ampdu_size = RX_AMPDU_SIZE_INVALID;

			_rtw_init_queue(&preorder_ctrl->pending_recvframe_queue);

			rtw_init_recv_timer(preorder_ctrl);
		}


		/* init for DM */
		psta->cmn.rssi_stat.rssi = (-1);
		psta->cmn.rssi_stat.rssi_cck = (-1);
		psta->cmn.rssi_stat.rssi_ofdm = (-1);
		/* init for the sequence number of received management frame */
		psta->RxMgmtFrameSeqNum = 0xffff;

		rtw_alloc_macid(pstapriv->adapt, psta);
	}

exit:

	_exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL2);

	if (psta)
		rtw_mi_update_iface_status(&(pstapriv->adapt->mlmepriv), 0);
	return psta;
}


/* using pstapriv->sta_hash_lock to protect */
u32	rtw_free_stainfo(struct adapter *adapt , struct sta_info *psta)
{
	int i;
	unsigned long irqL0;
	struct __queue *pfree_sta_queue;
	struct recv_reorder_ctrl *preorder_ctrl;
	struct	sta_xmit_priv	*pstaxmitpriv;
	struct	xmit_priv	*pxmitpriv = &adapt->xmitpriv;
	struct	sta_priv *pstapriv = &adapt->stapriv;
	struct hw_xmit *phwxmit;
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	int pending_qcnt[4];
	u8 is_pre_link_sta = false;

	if (!psta)
		goto exit;

	is_pre_link_sta = rtw_is_pre_link_sta(pstapriv, psta->cmn.mac_addr);

	if (!is_pre_link_sta) {
		_enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL0);
		rtw_list_delete(&psta->hash_list);
		pstapriv->asoc_sta_count--;
		_exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL0);
		rtw_mi_update_iface_status(&(adapt->mlmepriv), 0);
	} else {
		_enter_critical_bh(&psta->lock, &irqL0);
		psta->state = WIFI_FW_PRE_LINK;
		_exit_critical_bh(&psta->lock, &irqL0);
	}

	_enter_critical_bh(&psta->lock, &irqL0);
	psta->state &= ~_FW_LINKED;
	_exit_critical_bh(&psta->lock, &irqL0);

	pfree_sta_queue = &pstapriv->free_sta_queue;


	pstaxmitpriv = &psta->sta_xmitpriv;

	/* rtw_list_delete(&psta->sleep_list); */

	/* rtw_list_delete(&psta->wakeup_list); */

	_enter_critical_bh(&pxmitpriv->lock, &irqL0);

	rtw_free_xmitframe_queue(pxmitpriv, &psta->sleep_q);
	psta->sleepq_len = 0;

	/* vo */
	/* _enter_critical_bh(&(pxmitpriv->vo_pending.lock), &irqL0); */
	rtw_free_xmitframe_queue(pxmitpriv, &pstaxmitpriv->vo_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->vo_q.tx_pending));
	phwxmit = pxmitpriv->hwxmits;
	phwxmit->accnt -= pstaxmitpriv->vo_q.qcnt;
	pending_qcnt[0] = pstaxmitpriv->vo_q.qcnt;
	pstaxmitpriv->vo_q.qcnt = 0;
	/* _exit_critical_bh(&(pxmitpriv->vo_pending.lock), &irqL0); */

	/* vi */
	/* _enter_critical_bh(&(pxmitpriv->vi_pending.lock), &irqL0); */
	rtw_free_xmitframe_queue(pxmitpriv, &pstaxmitpriv->vi_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->vi_q.tx_pending));
	phwxmit = pxmitpriv->hwxmits + 1;
	phwxmit->accnt -= pstaxmitpriv->vi_q.qcnt;
	pending_qcnt[1] = pstaxmitpriv->vi_q.qcnt;
	pstaxmitpriv->vi_q.qcnt = 0;
	/* _exit_critical_bh(&(pxmitpriv->vi_pending.lock), &irqL0); */

	/* be */
	/* _enter_critical_bh(&(pxmitpriv->be_pending.lock), &irqL0); */
	rtw_free_xmitframe_queue(pxmitpriv, &pstaxmitpriv->be_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->be_q.tx_pending));
	phwxmit = pxmitpriv->hwxmits + 2;
	phwxmit->accnt -= pstaxmitpriv->be_q.qcnt;
	pending_qcnt[2] = pstaxmitpriv->be_q.qcnt;
	pstaxmitpriv->be_q.qcnt = 0;
	/* _exit_critical_bh(&(pxmitpriv->be_pending.lock), &irqL0); */

	/* bk */
	/* _enter_critical_bh(&(pxmitpriv->bk_pending.lock), &irqL0); */
	rtw_free_xmitframe_queue(pxmitpriv, &pstaxmitpriv->bk_q.sta_pending);
	rtw_list_delete(&(pstaxmitpriv->bk_q.tx_pending));
	phwxmit = pxmitpriv->hwxmits + 3;
	phwxmit->accnt -= pstaxmitpriv->bk_q.qcnt;
	pending_qcnt[3] = pstaxmitpriv->bk_q.qcnt;
	pstaxmitpriv->bk_q.qcnt = 0;
	/* _exit_critical_bh(&(pxmitpriv->bk_pending.lock), &irqL0); */

	rtw_os_wake_queue_at_free_stainfo(adapt, pending_qcnt);

	_exit_critical_bh(&pxmitpriv->lock, &irqL0);


	/* re-init sta_info; 20061114 */ /* will be init in alloc_stainfo */
	/* _rtw_init_sta_xmit_priv(&psta->sta_xmitpriv); */
	/* _rtw_init_sta_recv_priv(&psta->sta_recvpriv); */
#ifdef CONFIG_IEEE80211W
	_cancel_timer_ex(&psta->dot11w_expire_timer);
#endif /* CONFIG_IEEE80211W */
	_cancel_timer_ex(&psta->addba_retry_timer);

	/* for A-MPDU Rx reordering buffer control, cancel reordering_ctrl_timer */
	for (i = 0; i < 16 ; i++) {
		unsigned long irqL;
		struct list_head	*phead, *plist;
		union recv_frame *prframe;
		struct __queue *ppending_recvframe_queue;
		struct __queue *pfree_recv_queue = &adapt->recvpriv.free_recv_queue;

		preorder_ctrl = &psta->recvreorder_ctrl[i];

		_cancel_timer_ex(&preorder_ctrl->reordering_ctrl_timer);


		ppending_recvframe_queue = &preorder_ctrl->pending_recvframe_queue;

		_enter_critical_bh(&ppending_recvframe_queue->lock, &irqL);

		phead =	get_list_head(ppending_recvframe_queue);
		plist = get_next(phead);

		while (!rtw_is_list_empty(phead)) {
			prframe = container_of(plist, union recv_frame, u.list);

			plist = get_next(plist);

			rtw_list_delete(&(prframe->u.hdr.list));

			rtw_free_recvframe(prframe, pfree_recv_queue);
		}

		_exit_critical_bh(&ppending_recvframe_queue->lock, &irqL);

	}

	if (!((psta->state & WIFI_AP_STATE) || MacAddr_isBcst(psta->cmn.mac_addr)) && !is_pre_link_sta)
		rtw_hal_set_odm_var(adapt, HAL_ODM_STA_INFO, psta, false);


	/* release mac id for non-bc/mc station, */
	if (!is_pre_link_sta)
		rtw_release_macid(pstapriv->adapt, psta);

	_enter_critical_bh(&pstapriv->auth_list_lock, &irqL0);
	if (!rtw_is_list_empty(&psta->auth_list)) {
		rtw_list_delete(&psta->auth_list);
		pstapriv->auth_list_cnt--;
	}
	_exit_critical_bh(&pstapriv->auth_list_lock, &irqL0);

	psta->expire_to = 0;
	psta->sleepq_ac_len = 0;
	psta->qos_info = 0;

	psta->max_sp_len = 0;
	psta->uapsd_bk = 0;
	psta->uapsd_be = 0;
	psta->uapsd_vi = 0;
	psta->uapsd_vo = 0;

	psta->has_legacy_ac = 0;

	if (pmlmeinfo->state == _HW_STATE_AP_) {
		rtw_tim_map_clear(adapt, pstapriv->sta_dz_bitmap, psta->cmn.aid);
		rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, psta->cmn.aid);

		/* rtw_indicate_sta_disassoc_event(adapt, psta); */

		if ((psta->cmn.aid > 0) && (pstapriv->sta_aid[psta->cmn.aid - 1] == psta)) {
			pstapriv->sta_aid[psta->cmn.aid - 1] = NULL;
			psta->cmn.aid = 0;
		}
	}

	psta->under_exist_checking = 0;

	rtw_st_ctl_deinit(&psta->st_ctl);

	if (!is_pre_link_sta) {
		_enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL0);
		list_add_tail(&psta->list, get_list_head(pfree_sta_queue));
		_exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL0);
	}

exit:
	return _SUCCESS;
}

/* free all stainfo which in sta_hash[all] */
void rtw_free_all_stainfo(struct adapter *adapt)
{
	unsigned long	 irqL;
	struct list_head	*plist, *phead;
	int	index;
	struct sta_info *psta = NULL;
	struct	sta_priv *pstapriv = &adapt->stapriv;
	struct sta_info *pbcmc_stainfo = rtw_get_bcmc_stainfo(adapt);
	u8 free_sta_num = 0;
	char free_sta_list[NUM_STA];
	int stainfo_offset;


	if (pstapriv->asoc_sta_count == 1)
		goto exit;

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	for (index = 0; index < NUM_STA; index++) {
		phead = &(pstapriv->sta_hash[index]);
		plist = get_next(phead);

		while ((!rtw_end_of_queue_search(phead, plist))) {
			psta = container_of(plist, struct sta_info , hash_list);

			plist = get_next(plist);

			if (pbcmc_stainfo != psta) {
				if (!rtw_is_pre_link_sta(pstapriv, psta->cmn.mac_addr))
					rtw_list_delete(&psta->hash_list);

				stainfo_offset = rtw_stainfo_offset(pstapriv, psta);
				if (stainfo_offset_valid(stainfo_offset))
					free_sta_list[free_sta_num++] = stainfo_offset;
			}

		}
	}

	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);


	for (index = 0; index < free_sta_num; index++) {
		psta = rtw_get_stainfo_by_offset(pstapriv, free_sta_list[index]);
		rtw_free_stainfo(adapt , psta);
	}

exit:
	return;
}

/* any station allocated can be searched by hash list */
struct sta_info *rtw_get_stainfo(struct sta_priv *pstapriv, const u8 *hwaddr)
{

	unsigned long	 irqL;

	struct list_head	*plist, *phead;

	struct sta_info *psta = NULL;

	u32	index;

	const u8 *addr;

	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


	if (!hwaddr)
		return NULL;

	if (IS_MCAST(hwaddr))
		addr = bc_addr;
	else
		addr = hwaddr;

	index = wifi_mac_hash(addr);

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	phead = &(pstapriv->sta_hash[index]);
	plist = get_next(phead);


	while ((!rtw_end_of_queue_search(phead, plist))) {

		psta = container_of(plist, struct sta_info, hash_list);

		if ((!memcmp(psta->cmn.mac_addr, addr, ETH_ALEN))) {
			/* if found the matched address */
			break;
		}
		psta = NULL;
		plist = get_next(plist);
	}

	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	return psta;

}

u32 rtw_init_bcmc_stainfo(struct adapter *adapt)
{

	struct sta_info	*psta;
	struct tx_servq	*ptxservq;
	u32 res = _SUCCESS;
	unsigned char bcast_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	struct	sta_priv *pstapriv = &adapt->stapriv;
	/* struct __queue	*pstapending = &adapt->xmitpriv.bm_pending; */


	psta = rtw_alloc_stainfo(pstapriv, bcast_addr);

	if (!psta) {
		res = _FAIL;
		goto exit;
	}

	ptxservq = &(psta->sta_xmitpriv.be_q);

exit:
	return _SUCCESS;
}


struct sta_info *rtw_get_bcmc_stainfo(struct adapter *adapt)
{
	struct sta_info	*psta;
	struct sta_priv	*pstapriv = &adapt->stapriv;
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	psta = rtw_get_stainfo(pstapriv, bc_addr);
	return psta;

}

u16 rtw_aid_alloc(struct adapter *adapter, struct sta_info *sta)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	u16 aid, i, used_cnt = 0;

	for (i = 0; i < stapriv->max_aid; i++) {
		aid = ((i + stapriv->started_aid - 1) % stapriv->max_aid) + 1;
		if (!stapriv->sta_aid[aid - 1])
			break;
		if (++used_cnt >= stapriv->max_num_sta)
			break;
	}

	/* check for aid limit and assoc limit  */
	if (i >= stapriv->max_aid || used_cnt >= stapriv->max_num_sta)
		aid = 0;

	sta->cmn.aid = aid;
	if (aid) {
		stapriv->sta_aid[aid - 1] = sta;
		if (stapriv->rr_aid)
			stapriv->started_aid = (aid % stapriv->max_aid) + 1;
	}

	return aid;
}

void dump_aid_status(void *sel, struct adapter *adapter)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	u8 *aid_bmp;
	u16 i, used_cnt = 0;

	aid_bmp = rtw_zmalloc(stapriv->aid_bmp_len);
	if (!aid_bmp)
		return;

	for (i = 1; i <= stapriv->max_aid; i++) {
		if (stapriv->sta_aid[i - 1]) {
			aid_bmp[i / 8] |= BIT(i % 8);
			++used_cnt;
		}
	}

	RTW_PRINT_SEL(sel, "used_cnt:%u/%u\n", used_cnt, stapriv->max_aid);
	RTW_MAP_DUMP_SEL(sel, "aid_map:", aid_bmp, stapriv->aid_bmp_len);
	RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "%-2s %-11s\n", "rr", "started_aid");
	RTW_PRINT_SEL(sel, "%2d %11d\n", stapriv->rr_aid, stapriv->started_aid);

	rtw_mfree(aid_bmp, stapriv->aid_bmp_len);
}

#if CONFIG_RTW_MACADDR_ACL
const char *const _acl_mode_str[] = {
	"DISABLED",
	"ACCEPT_UNLESS_LISTED",
	"DENY_UNLESS_LISTED",
};

u8 rtw_access_ctrl(struct adapter *adapter, u8 *mac_addr)
{
	u8 res = true;
	unsigned long irqL;
	struct list_head *list, *head;
	struct rtw_wlan_acl_node *acl_node;
	u8 match = false;
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;
	struct __queue	*acl_node_q = &acl->acl_node_q;

	_enter_critical_bh(&(acl_node_q->lock), &irqL);
	head = get_list_head(acl_node_q);
	list = get_next(head);
	while (!rtw_end_of_queue_search(head, list)) {
		acl_node = container_of(list, struct rtw_wlan_acl_node, list);
		list = get_next(list);

		if (!memcmp(acl_node->addr, mac_addr, ETH_ALEN)) {
			if (acl_node->valid) {
				match = true;
				break;
			}
		}
	}
	_exit_critical_bh(&(acl_node_q->lock), &irqL);

	if (acl->mode == RTW_ACL_MODE_ACCEPT_UNLESS_LISTED)
		res = (match) ?  false : true;
	else if (acl->mode == RTW_ACL_MODE_DENY_UNLESS_LISTED)
		res = (match) ?  true : false;
	else
		res = true;

	return res;
}

void dump_macaddr_acl(void *sel, struct adapter *adapter)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	struct wlan_acl_pool *acl = &stapriv->acl_list;
	int i;

	RTW_PRINT_SEL(sel, "mode:%s(%d)\n", acl_mode_str(acl->mode), acl->mode);
	RTW_PRINT_SEL(sel, "num:%d/%d\n", acl->num, NUM_ACL);
	for (i = 0; i < NUM_ACL; i++) {
		if (!acl->aclnode[i].valid)
			continue;
		RTW_PRINT_SEL(sel, MAC_FMT"\n", MAC_ARG(acl->aclnode[i].addr));
	}
}
#endif /* CONFIG_RTW_MACADDR_ACL */

bool rtw_is_pre_link_sta(struct sta_priv *stapriv, u8 *addr)
{
#if CONFIG_RTW_PRE_LINK_STA
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	struct sta_info *sta = NULL;
	u8 exist = false;
	int i;
	unsigned long irqL;

	_enter_critical_bh(&(pre_link_sta_ctl->lock), &irqL);
	for (i = 0; i < RTW_PRE_LINK_STA_NUM; i++) {
		if (pre_link_sta_ctl->node[i].valid
			&& !memcmp(pre_link_sta_ctl->node[i].addr, addr, ETH_ALEN)
		) {
			exist = true;
			break;
		}
	}
	_exit_critical_bh(&(pre_link_sta_ctl->lock), &irqL);

	return exist;
#else
	return false;
#endif
}

#if CONFIG_RTW_PRE_LINK_STA
struct sta_info *rtw_pre_link_sta_add(struct sta_priv *stapriv, u8 *hwaddr)
{
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	struct pre_link_sta_node_t *node = NULL;
	struct sta_info *sta = NULL;
	u8 exist = false;
	int i;
	unsigned long irqL;

	if (rtw_check_invalid_mac_address(hwaddr, false))
		goto exit;

	_enter_critical_bh(&(pre_link_sta_ctl->lock), &irqL);
	for (i = 0; i < RTW_PRE_LINK_STA_NUM; i++) {
		if (pre_link_sta_ctl->node[i].valid
			&& !memcmp(pre_link_sta_ctl->node[i].addr, hwaddr, ETH_ALEN)
		) {
			node = &pre_link_sta_ctl->node[i];
			exist = true;
			break;
		}

		if (!node && !pre_link_sta_ctl->node[i].valid)
			node = &pre_link_sta_ctl->node[i];
	}

	if (!exist && node) {
		memcpy(node->addr, hwaddr, ETH_ALEN);
		node->valid = true;
		pre_link_sta_ctl->num++;
	}
	_exit_critical_bh(&(pre_link_sta_ctl->lock), &irqL);

	if (!node)
		goto exit;

	sta = rtw_get_stainfo(stapriv, hwaddr);
	if (sta)
		goto odm_hook;

	sta = rtw_alloc_stainfo(stapriv, hwaddr);
	if (!sta)
		goto exit;

	sta->state = WIFI_FW_PRE_LINK;

odm_hook:
	rtw_hal_set_odm_var(stapriv->adapt, HAL_ODM_STA_INFO, sta, true);

exit:
	return sta;
}

void rtw_pre_link_sta_del(struct sta_priv *stapriv, u8 *hwaddr)
{
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	struct pre_link_sta_node_t *node = NULL;
	struct sta_info *sta = NULL;
	u8 exist = false;
	int i;
	unsigned long irqL;

	if (rtw_check_invalid_mac_address(hwaddr, false))
		goto exit;

	_enter_critical_bh(&(pre_link_sta_ctl->lock), &irqL);
	for (i = 0; i < RTW_PRE_LINK_STA_NUM; i++) {
		if (pre_link_sta_ctl->node[i].valid
			&& !memcmp(pre_link_sta_ctl->node[i].addr, hwaddr, ETH_ALEN)
		) {
			node = &pre_link_sta_ctl->node[i];
			exist = true;
			break;
		}
	}

	if (exist && node) {
		node->valid = false;
		pre_link_sta_ctl->num--;
	}
	_exit_critical_bh(&(pre_link_sta_ctl->lock), &irqL);

	if (!exist)
		goto exit;

	sta = rtw_get_stainfo(stapriv, hwaddr);
	if (!sta)
		goto exit;

	if (sta->state == WIFI_FW_PRE_LINK)
		rtw_free_stainfo(stapriv->adapt, sta);

exit:
	return;
}

void rtw_pre_link_sta_ctl_reset(struct sta_priv *stapriv)
{
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	struct pre_link_sta_node_t *node = NULL;
	struct sta_info *sta = NULL;
	int i, j = 0;
	unsigned long irqL;

	u8 addrs[RTW_PRE_LINK_STA_NUM][ETH_ALEN];

	memset(addrs, 0, RTW_PRE_LINK_STA_NUM * ETH_ALEN);

	_enter_critical_bh(&(pre_link_sta_ctl->lock), &irqL);
	for (i = 0; i < RTW_PRE_LINK_STA_NUM; i++) {
		if (!pre_link_sta_ctl->node[i].valid)
			continue;
		memcpy(&(addrs[j][0]), pre_link_sta_ctl->node[i].addr, ETH_ALEN);
		pre_link_sta_ctl->node[i].valid = false;
		pre_link_sta_ctl->num--;
		j++;
	}
	_exit_critical_bh(&(pre_link_sta_ctl->lock), &irqL);

	for (i = 0; i < j; i++) {
		sta = rtw_get_stainfo(stapriv, &(addrs[i][0]));
		if (!sta)
			continue;

		if (sta->state == WIFI_FW_PRE_LINK)
			rtw_free_stainfo(stapriv->adapt, sta);
	}
}

void rtw_pre_link_sta_ctl_init(struct sta_priv *stapriv)
{
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	int i;

	spin_lock_init(&pre_link_sta_ctl->lock);
	pre_link_sta_ctl->num = 0;
	for (i = 0; i < RTW_PRE_LINK_STA_NUM; i++)
		pre_link_sta_ctl->node[i].valid = false;
}

void rtw_pre_link_sta_ctl_deinit(struct sta_priv *stapriv)
{
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	int i;

	rtw_pre_link_sta_ctl_reset(stapriv);
}

void dump_pre_link_sta_ctl(void *sel, struct sta_priv *stapriv)
{
	struct pre_link_sta_ctl_t *pre_link_sta_ctl = &stapriv->pre_link_sta_ctl;
	int i;

	RTW_PRINT_SEL(sel, "num:%d/%d\n", pre_link_sta_ctl->num, RTW_PRE_LINK_STA_NUM);

	for (i = 0; i < RTW_PRE_LINK_STA_NUM; i++) {
		if (!pre_link_sta_ctl->node[i].valid)
			continue;
		RTW_PRINT_SEL(sel, MAC_FMT"\n", MAC_ARG(pre_link_sta_ctl->node[i].addr));
	}
}
#endif /* CONFIG_RTW_PRE_LINK_STA */

