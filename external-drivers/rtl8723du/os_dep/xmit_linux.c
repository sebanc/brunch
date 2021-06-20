// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _XMIT_OSDEP_C_

#include <drv_types.h>

#define DBG_DUMP_OS_QUEUE_CTL 0

uint rtw_remainder_len(struct pkt_file *pfile)
{
	return pfile->buf_len - ((SIZE_PTR)(pfile->cur_addr) - (SIZE_PTR)(pfile->buf_start));
}

void _rtw_open_pktfile(struct sk_buff *pktptr, struct pkt_file *pfile)
{

	pfile->pkt = pktptr;
	pfile->cur_addr = pfile->buf_start = pktptr->data;
	pfile->pkt_len = pfile->buf_len = pktptr->len;

	pfile->cur_buffer = pfile->buf_start ;

}

uint _rtw_pktfile_read(struct pkt_file *pfile, u8 *rmem, uint rlen)
{
	uint	len = 0;


	len =  rtw_remainder_len(pfile);
	len = (rlen > len) ? len : rlen;

	if (rmem)
		skb_copy_bits(pfile->pkt, pfile->buf_len - pfile->pkt_len, rmem, len);

	pfile->cur_addr += len;
	pfile->pkt_len -= len;


	return len;
}

int rtw_endofpktfile(struct pkt_file *pfile)
{

	if (pfile->pkt_len == 0) {
		return true;
	}


	return false;
}

void rtw_set_tx_chksum_offload(struct sk_buff *pkt, struct pkt_attrib *pattrib)
{

#ifdef CONFIG_TCP_CSUM_OFFLOAD_TX
	struct sk_buff *skb = (struct sk_buff *)pkt;
	pattrib->hw_tcp_csum = 0;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		if (skb_shinfo(skb)->nr_frags == 0) {
			const struct iphdr *ip = ip_hdr(skb);
			if (ip->protocol == IPPROTO_TCP) {
				/* TCP checksum offload by HW */
				RTW_INFO("CHECKSUM_PARTIAL TCP\n");
				pattrib->hw_tcp_csum = 1;
				/* skb_checksum_help(skb); */
			} else if (ip->protocol == IPPROTO_UDP) {
				/* RTW_INFO("CHECKSUM_PARTIAL UDP\n"); */
				skb_checksum_help(skb);
			} else {
				RTW_INFO("%s-%d TCP CSUM offload Error!!\n", __func__, __LINE__);
				WARN_ON(1);     /* we need a WARN() */
			}
		} else { /* IP fragmentation case */
			RTW_INFO("%s-%d nr_frags != 0, using skb_checksum_help(skb);!!\n", __func__, __LINE__);
			skb_checksum_help(skb);
		}
	}
#endif

}

int rtw_os_xmit_resource_alloc(struct adapter *adapt, struct xmit_buf *pxmitbuf, u32 alloc_sz, u8 flag)
{
	if (alloc_sz > 0) {
		pxmitbuf->pallocated_buf = rtw_zmalloc(alloc_sz);
		if (!pxmitbuf->pallocated_buf)
			return _FAIL;

		pxmitbuf->pbuf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pxmitbuf->pallocated_buf), XMITBUF_ALIGN_SZ);
	}

	if (flag) {
		int i;
		for (i = 0; i < 8; i++) {
			pxmitbuf->pxmit_urb[i] = usb_alloc_urb(0, GFP_KERNEL);
			if (!pxmitbuf->pxmit_urb[i]) {
				RTW_INFO("pxmitbuf->pxmit_urb[i]==NULL");
				return _FAIL;
			}
		}
	}

	return _SUCCESS;
}

void rtw_os_xmit_resource_free(struct adapter *adapt, struct xmit_buf *pxmitbuf, u32 free_sz, u8 flag)
{
	if (flag) {
		int i;

		for (i = 0; i < 8; i++) {
			if (pxmitbuf->pxmit_urb[i]) {
				/* usb_kill_urb(pxmitbuf->pxmit_urb[i]); */
				usb_free_urb(pxmitbuf->pxmit_urb[i]);
			}
		}
	}

	if (free_sz > 0) {
		if (pxmitbuf->pallocated_buf)
			rtw_mfree(pxmitbuf->pallocated_buf, free_sz);
	}
}

void dump_os_queue(void *sel, struct adapter *adapt)
{
	struct net_device *ndev = adapt->pnetdev;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	int i;

	for (i = 0; i < 4; i++) {
		RTW_PRINT_SEL(sel, "os_queue[%d]:%s\n"
			, i, __netif_subqueue_stopped(ndev, i) ? "stopped" : "waked");
	}
#else
	RTW_PRINT_SEL(sel, "os_queue:%s\n"
		      , netif_queue_stopped(ndev) ? "stopped" : "waked");
#endif
}

#define WMM_XMIT_THRESHOLD	(NR_XMITFRAME*2/5)

static inline bool rtw_os_need_wake_queue(struct adapter *adapt, u16 qidx)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	if (adapt->registrypriv.wifi_spec) {
		if (pxmitpriv->hwxmits[qidx].accnt < WMM_XMIT_THRESHOLD)
			return true;
	} else {
		return true;
	}
	return false;
#else
	return true;
#endif
}

static inline bool rtw_os_need_stop_queue(struct adapter *adapt, u16 qidx)
{
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	if (adapt->registrypriv.wifi_spec) {
		/* No free space for Tx, tx_worker is too slow */
		if (pxmitpriv->hwxmits[qidx].accnt > WMM_XMIT_THRESHOLD)
			return true;
	} else {
		if (pxmitpriv->free_xmitframe_cnt <= 4)
			return true;
	}
#else
	if (pxmitpriv->free_xmitframe_cnt <= 4)
		return true;
#endif
	return false;
}

void rtw_os_pkt_complete(struct adapter *adapt, struct sk_buff *pkt)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	u16	qidx;

	qidx = skb_get_queue_mapping(pkt);
	if (rtw_os_need_wake_queue(adapt, qidx)) {
		if (DBG_DUMP_OS_QUEUE_CTL)
			RTW_INFO(FUNC_ADPT_FMT": netif_wake_subqueue[%d]\n", FUNC_ADPT_ARG(adapt), qidx);
		netif_wake_subqueue(adapt->pnetdev, qidx);
	}
#else
	if (rtw_os_need_wake_queue(adapt, 0)) {
		if (DBG_DUMP_OS_QUEUE_CTL)
			RTW_INFO(FUNC_ADPT_FMT": netif_wake_queue\n", FUNC_ADPT_ARG(adapt));
		netif_wake_queue(adapt->pnetdev);
	}
#endif

	dev_kfree_skb_any(pkt);
}

void rtw_os_xmit_complete(struct adapter *adapt, struct xmit_frame *pxframe)
{
	if (pxframe->pkt)
		rtw_os_pkt_complete(adapt, pxframe->pkt);

	pxframe->pkt = NULL;
}

void rtw_os_xmit_schedule(struct adapter *adapt)
{
	unsigned long  irqL;
	struct xmit_priv *pxmitpriv;

	if (!adapt)
		return;

	pxmitpriv = &adapt->xmitpriv;

	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	if (rtw_txframes_pending(adapt))
		tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);

	_exit_critical_bh(&pxmitpriv->lock, &irqL);
}

static bool rtw_check_xmit_resource(struct adapter *adapt, struct sk_buff *pkt)
{
	bool busy = false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	u16	qidx;

	qidx = skb_get_queue_mapping(pkt);
	if (rtw_os_need_stop_queue(adapt, qidx)) {
		if (DBG_DUMP_OS_QUEUE_CTL)
			RTW_INFO(FUNC_ADPT_FMT": netif_stop_subqueue[%d]\n", FUNC_ADPT_ARG(adapt), qidx);
		netif_stop_subqueue(adapt->pnetdev, qidx);
		busy = true;
	}
#else
	if (rtw_os_need_stop_queue(adapt, 0)) {
		if (DBG_DUMP_OS_QUEUE_CTL)
			RTW_INFO(FUNC_ADPT_FMT": netif_stop_queue\n", FUNC_ADPT_ARG(adapt));
		rtw_netif_stop_queue(adapt->pnetdev);
		busy = true;
	}
#endif
	return busy;
}

void rtw_os_wake_queue_at_free_stainfo(struct adapter *adapt, int *qcnt_freed)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	int i;

	for (i = 0; i < 4; i++) {
		if (qcnt_freed[i] == 0)
			continue;

		if (rtw_os_need_wake_queue(adapt, i)) {
			if (DBG_DUMP_OS_QUEUE_CTL)
				RTW_INFO(FUNC_ADPT_FMT": netif_wake_subqueue[%d]\n", FUNC_ADPT_ARG(adapt), i);
			netif_wake_subqueue(adapt->pnetdev, i);
		}
	}
#else
	if (qcnt_freed[0] || qcnt_freed[1] || qcnt_freed[2] || qcnt_freed[3]) {
		if (rtw_os_need_wake_queue(adapt, 0)) {
			if (DBG_DUMP_OS_QUEUE_CTL)
				RTW_INFO(FUNC_ADPT_FMT": netif_wake_queue\n", FUNC_ADPT_ARG(adapt));
			netif_wake_queue(adapt->pnetdev);
		}
	}
#endif
}

static int rtw_mlcst2unicst(struct adapter *adapt, struct sk_buff *skb)
{
	struct	sta_priv *pstapriv = &adapt->stapriv;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	unsigned long	irqL;
	struct list_head	*phead, *plist;
	struct sk_buff *newskb;
	struct sta_info *psta = NULL;
	u8 chk_alive_num = 0;
	char chk_alive_list[NUM_STA];
	u8 bc_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u8 null_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	int i;
	int	res;

	DBG_COUNTER(adapt->tx_logs.os_tx_m2u);

	_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
	phead = &pstapriv->asoc_list;
	plist = get_next(phead);

	/* free sta asoc_queue */
	while ((!rtw_end_of_queue_search(phead, plist))) {
		int stainfo_offset;
		psta = container_of(plist, struct sta_info, asoc_list);
		plist = get_next(plist);

		stainfo_offset = rtw_stainfo_offset(pstapriv, psta);
		if (stainfo_offset_valid(stainfo_offset))
			chk_alive_list[chk_alive_num++] = stainfo_offset;
	}
	_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

	for (i = 0; i < chk_alive_num; i++) {
		psta = rtw_get_stainfo_by_offset(pstapriv, chk_alive_list[i]);
		if (!(psta->state & _FW_LINKED)) {
			DBG_COUNTER(adapt->tx_logs.os_tx_m2u_ignore_fw_linked);
			continue;
		}

		/* avoid come from STA1 and send back STA1 */
		if (!memcmp(psta->cmn.mac_addr, &skb->data[6], 6)
			|| !memcmp(psta->cmn.mac_addr, null_addr, 6)
			|| !memcmp(psta->cmn.mac_addr, bc_addr, 6)
		) {
			DBG_COUNTER(adapt->tx_logs.os_tx_m2u_ignore_self);
			continue;
		}

		DBG_COUNTER(adapt->tx_logs.os_tx_m2u_entry);

		newskb = rtw_skb_copy(skb);

		if (newskb) {
			memcpy(newskb->data, psta->cmn.mac_addr, 6);
			res = rtw_xmit(adapt, &newskb);
			if (res < 0) {
				DBG_COUNTER(adapt->tx_logs.os_tx_m2u_entry_err_xmit);
				RTW_INFO("%s()-%d: rtw_xmit() return error! res=%d\n", __func__, __LINE__, res);
				pxmitpriv->tx_drop++;
				dev_kfree_skb_any(newskb);
			}
		} else {
			DBG_COUNTER(adapt->tx_logs.os_tx_m2u_entry_err_skb);
			RTW_INFO("%s-%d: rtw_skb_copy() failed!\n", __func__, __LINE__);
			pxmitpriv->tx_drop++;
			/* dev_kfree_skb_any(skb); */
			return false;	/* Caller shall tx this multicast frame via normal way. */
		}
	}

	dev_kfree_skb_any(skb);
	return true;
}

int _rtw_xmit_entry(struct sk_buff *pkt, struct  net_device * pnetdev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	int res = 0;

	if (adapt->registrypriv.mp_mode) {
		RTW_INFO("MP_TX_DROP_OS_FRAME\n");
		goto drop_packet;
	}
	DBG_COUNTER(adapt->tx_logs.os_tx);

	if (!rtw_if_up(adapt)) {
		DBG_COUNTER(adapt->tx_logs.os_tx_err_up);
		goto drop_packet;
	}

	rtw_check_xmit_resource(adapt, pkt);

	if (!rtw_mc2u_disable
		&& (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt))
		&& (IP_MCAST_MAC(pkt->data)
			|| ICMPV6_MCAST_MAC(pkt->data)
			#ifdef CONFIG_TX_BCAST2UNI
			|| is_broadcast_mac_addr(pkt->data)
			#endif
			)
		&& (adapt->registrypriv.wifi_spec == 0)
	) {
		if (pxmitpriv->free_xmitframe_cnt > (NR_XMITFRAME / 4)) {
			res = rtw_mlcst2unicst(adapt, pkt);
			if (res)
				goto exit;
		} else {
			/* RTW_INFO("Stop M2U(%d, %d)! ", pxmitpriv->free_xmitframe_cnt, pxmitpriv->free_xmitbuf_cnt); */
			/* RTW_INFO("!m2u ); */
			DBG_COUNTER(adapt->tx_logs.os_tx_m2u_stop);
		}
	}

	res = rtw_xmit(adapt, &pkt);
	if (res < 0)
		goto drop_packet;

	goto exit;

drop_packet:
	pxmitpriv->tx_drop++;
	rtw_os_pkt_complete(adapt, pkt);

exit:


	return 0;
}

int rtw_xmit_entry(struct sk_buff *pkt, struct  net_device * pnetdev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	int ret = 0;

	if (pkt) {
		if (check_fwstate(pmlmepriv, WIFI_MONITOR_STATE)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
			rtw_monitor_xmit_entry((struct sk_buff *)pkt, pnetdev);
#endif
		}
		else {
			rtw_mstat_update(MSTAT_TYPE_SKB, MSTAT_ALLOC_SUCCESS, pkt->truesize);
			ret = _rtw_xmit_entry(pkt, pnetdev);
		}

	}

	return ret;
}
