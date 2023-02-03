// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RECV_OSDEP_C_

#include <drv_types.h>

u8 rtw_rfc1042_header[] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};
/* Bridge-Tunnel header (for EtherTypes ETH_P_AARP and ETH_P_IPX) */
u8 rtw_bridge_tunnel_header[] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0xf8};

int rtw_os_recvframe_duplicate_skb(struct adapter *adapt, union recv_frame *pcloneframe, struct sk_buff *pskb)
{
	int res = _SUCCESS;
	struct sk_buff	*pkt_copy = NULL;

	if (!pskb) {
		RTW_INFO("%s [WARN] skb is NULL, drop frag frame\n", __func__);
		return _FAIL;
	}
	pkt_copy = rtw_skb_copy(pskb);

	if (!pkt_copy) {
		RTW_INFO("%s [WARN] rtw_skb_copy fail , drop frag frame\n", __func__);
		return _FAIL;
	}
	pkt_copy->dev = adapt->pnetdev;

	pcloneframe->u.hdr.pkt = pkt_copy;
	pcloneframe->u.hdr.rx_head = pkt_copy->head;
	pcloneframe->u.hdr.rx_data = pkt_copy->data;
	pcloneframe->u.hdr.rx_end = skb_end_pointer(pkt_copy);
	pcloneframe->u.hdr.rx_tail = skb_tail_pointer(pkt_copy);
	pcloneframe->u.hdr.len = pkt_copy->len;

	return res;
}

int rtw_os_alloc_recvframe(struct adapter *adapt, union recv_frame *precvframe, u8 *pdata, struct sk_buff *pskb)
{
	int res = _SUCCESS;
	u8	shift_sz = 0;
	u32	skb_len, alloc_sz;
	struct sk_buff	*pkt_copy = NULL;
	struct rx_pkt_attrib *pattrib = &precvframe->u.hdr.attrib;


	if (!pdata) {
		precvframe->u.hdr.pkt = NULL;
		res = _FAIL;
		return res;
	}


	/*	Modified by Albert 20101213 */
	/*	For 8 bytes IP header alignment. */
	shift_sz = pattrib->qos ? 6 : 0; /*	Qos data, wireless lan header length is 26 */

	skb_len = le16_to_cpu(pattrib->pkt_len);

	/* for first fragment packet, driver need allocate 1536+drvinfo_sz+RXDESC_SIZE to defrag packet. */
	/* modify alloc_sz for recvive crc error packet by thomas 2011-06-02 */
	if ((pattrib->mfrag == 1) && (pattrib->frag_num == 0)) {
		/* alloc_sz = 1664;	 */ /* 1664 is 128 alignment. */
		alloc_sz = (skb_len <= 1650) ? 1664 : (skb_len + 14);
	} else {
		alloc_sz = skb_len;
		/*	6 is for IP header 8 bytes alignment in QoS packet case. */
		/*	8 is for skb->data 4 bytes alignment. */
		alloc_sz += 14;
	}

	pkt_copy = rtw_skb_alloc(alloc_sz);

	if (pkt_copy) {
		pkt_copy->dev = adapt->pnetdev;
		pkt_copy->len = skb_len;
		precvframe->u.hdr.pkt = pkt_copy;
		precvframe->u.hdr.rx_head = pkt_copy->head;
		precvframe->u.hdr.rx_end = pkt_copy->data + alloc_sz;
		skb_reserve(pkt_copy, 8 - ((SIZE_PTR)(pkt_copy->data) & 7));  /* force pkt_copy->data at 8-byte alignment address */
		skb_reserve(pkt_copy, shift_sz);/* force ip_hdr at 8-byte alignment address according to shift_sz. */
		memcpy(pkt_copy->data, pdata, skb_len);
		precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail = pkt_copy->data;
	} else {
		if ((pattrib->mfrag == 1) && (pattrib->frag_num == 0)) {
			RTW_INFO("%s: alloc_skb fail , drop frag frame\n", __func__);
			/* rtw_free_recvframe(precvframe, pfree_recv_queue); */
			res = _FAIL;
			goto exit_rtw_os_recv_resource_alloc;
		}

		if (!pskb) {
			res = _FAIL;
			goto exit_rtw_os_recv_resource_alloc;
		}

		precvframe->u.hdr.pkt = rtw_skb_clone(pskb);
		if (precvframe->u.hdr.pkt) {
			precvframe->u.hdr.pkt->dev = adapt->pnetdev;
			precvframe->u.hdr.rx_head = precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail = pdata;
			precvframe->u.hdr.rx_end =  pdata + alloc_sz;
		} else {
			RTW_INFO("%s: rtw_skb_clone fail\n", __func__);
			/* rtw_free_recvframe(precvframe, pfree_recv_queue); */
			/*exit_rtw_os_recv_resource_alloc;*/
			res = _FAIL;
		}
	}

exit_rtw_os_recv_resource_alloc:

	return res;
}

void rtw_os_free_recvframe(union recv_frame *precvframe)
{
	if (precvframe->u.hdr.pkt) {
		dev_kfree_skb_any(precvframe->u.hdr.pkt);
		precvframe->u.hdr.pkt = NULL;
	}
}

/* init os related resource in struct recv_priv */
int rtw_os_recv_resource_init(struct recv_priv *precvpriv, struct adapter *adapt)
{
	int	res = _SUCCESS;


#ifdef CONFIG_RTW_NAPI
	skb_queue_head_init(&precvpriv->rx_napi_skb_queue);
#endif /* CONFIG_RTW_NAPI */

	return res;
}

/* alloc os related resource in union recv_frame */
int rtw_os_recv_resource_alloc(struct adapter *adapt, union recv_frame *precvframe)
{
	int	res = _SUCCESS;

	precvframe->u.hdr.pkt = NULL;

	return res;
}

/* free os related resource in union recv_frame */
void rtw_os_recv_resource_free(struct recv_priv *precvpriv)
{
	int i;
	union recv_frame *precvframe;
	precvframe = (union recv_frame *) precvpriv->precv_frame_buf;


#ifdef CONFIG_RTW_NAPI
	if (skb_queue_len(&precvpriv->rx_napi_skb_queue))
		RTW_WARN("rx_napi_skb_queue not empty\n");
	rtw_skb_queue_purge(&precvpriv->rx_napi_skb_queue);
#endif /* CONFIG_RTW_NAPI */

	for (i = 0; i < NR_RECVFRAME; i++) {
		rtw_os_free_recvframe(precvframe);
		precvframe++;
	}
}

/* alloc os related resource in struct recv_buf */
int rtw_os_recvbuf_resource_alloc(struct adapter *adapt, struct recv_buf *precvbuf)
{
	int res = _SUCCESS;

	precvbuf->irp_pending = false;
	precvbuf->purb = usb_alloc_urb(0, GFP_KERNEL);
	if (!precvbuf->purb)
		res = _FAIL;

	precvbuf->pskb = NULL;

	precvbuf->pallocated_buf  = precvbuf->pbuf = NULL;

	precvbuf->pdata = precvbuf->phead = precvbuf->ptail = precvbuf->pend = NULL;

	precvbuf->transfer_len = 0;

	precvbuf->len = 0;
	return res;
}

/* free os related resource in struct recv_buf */
int rtw_os_recvbuf_resource_free(struct adapter *adapt, struct recv_buf *precvbuf)
{
	int ret = _SUCCESS;

	if (precvbuf->purb) {
		/* usb_kill_urb(precvbuf->purb); */
		usb_free_urb(precvbuf->purb);
	}
	if (precvbuf->pskb)
			dev_kfree_skb_any(precvbuf->pskb);
	return ret;
}

struct sk_buff *rtw_os_alloc_msdu_pkt(union recv_frame *prframe, u16 nSubframe_Length, u8 *pdata)
{
	u16	eth_type;
	u8	*data_ptr;
	struct sk_buff *sub_skb;
	struct rx_pkt_attrib *pattrib;

	pattrib = &prframe->u.hdr.attrib;

	sub_skb = rtw_skb_alloc(nSubframe_Length + 14);
	if (sub_skb) {
		skb_reserve(sub_skb, 14);
		data_ptr = (u8 *)skb_put(sub_skb, nSubframe_Length);
		memcpy(data_ptr, (pdata + ETH_HLEN), nSubframe_Length);
	} else {
		sub_skb = rtw_skb_clone(prframe->u.hdr.pkt);
		if (sub_skb) {
			sub_skb->data = pdata + ETH_HLEN;
			sub_skb->len = nSubframe_Length;
			skb_set_tail_pointer(sub_skb, nSubframe_Length);
		} else {
			RTW_INFO("%s(): rtw_skb_clone() Fail!!!\n", __func__);
			return NULL;
		}
	}

	eth_type = RTW_GET_BE16(&sub_skb->data[6]);

	if (sub_skb->len >= 8 &&
	    ((!memcmp(sub_skb->data, rtw_rfc1042_header, SNAP_SIZE) &&
	      eth_type != ETH_P_AARP && eth_type != ETH_P_IPX) ||
	     !memcmp(sub_skb->data, rtw_bridge_tunnel_header, SNAP_SIZE))) {
		/* remove RFC1042 or Bridge-Tunnel encapsulation and replace EtherType */
		skb_pull(sub_skb, SNAP_SIZE);
		memcpy(skb_push(sub_skb, ETH_ALEN), pdata + 6, ETH_ALEN);
		memcpy(skb_push(sub_skb, ETH_ALEN), pdata, ETH_ALEN);
	} else {
		u16 len;
		/* Leave Ethernet header part of hdr and full payload */
		len = sub_skb->len;
		memcpy(skb_push(sub_skb, 2), &len, 2);
		memcpy(skb_push(sub_skb, ETH_ALEN), pdata + 6, ETH_ALEN);
		memcpy(skb_push(sub_skb, ETH_ALEN), pdata, ETH_ALEN);
	}

	return sub_skb;
}

#ifdef CONFIG_RTW_NAPI
static int napi_recv(struct adapter *adapt, int budget)
{
	struct sk_buff *pskb;
	struct recv_priv *precvpriv = &adapt->recvpriv;
	int work_done = 0;
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	u8 rx_ok;


	while ((work_done < budget) &&
	       (!skb_queue_empty(&precvpriv->rx_napi_skb_queue))) {
		pskb = skb_dequeue(&precvpriv->rx_napi_skb_queue);
		if (!pskb)
			break;

		rx_ok = false;

		if (pregistrypriv->en_gro) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 12, 0)
			if (rtw_napi_gro_receive(&adapt->napi, pskb) != GRO_DROP)
				rx_ok = true;
#else
			rtw_napi_gro_receive(&adapt->napi, pskb);
			rx_ok = true;
#endif
			goto next;
		}
		if (rtw_netif_receive_skb(adapt->pnetdev, pskb) == NET_RX_SUCCESS)
			rx_ok = true;

next:
		if (rx_ok) {
			work_done++;
			DBG_COUNTER(adapt->rx_logs.os_netif_ok);
		} else {
			DBG_COUNTER(adapt->rx_logs.os_netif_err);
		}
	}

	return work_done;
}

int rtw_recv_napi_poll(struct napi_struct *napi, int budget)
{
	struct adapter *adapt = container_of(napi, struct adapter, napi);
	int work_done = 0;
	struct recv_priv *precvpriv = &adapt->recvpriv;


	work_done = napi_recv(adapt, budget);
	if (work_done < budget) {
		napi_complete(napi);
		if (!skb_queue_empty(&precvpriv->rx_napi_skb_queue))
			napi_schedule(napi);
	}

	return work_done;
}
#endif /* CONFIG_RTW_NAPI */

void rtw_os_recv_indicate_pkt(struct adapter *adapt, struct sk_buff *pkt, union recv_frame *rframe)
{
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct recv_priv *precvpriv = &(adapt->recvpriv);
	struct registry_priv	*pregistrypriv = &adapt->registrypriv;
	void *br_port = NULL;
	int ret;

	/* Indicat the packets to upper layer */
	if (pkt) {
		struct ethhdr *ehdr = (struct ethhdr *)pkt->data;

		DBG_COUNTER(adapt->rx_logs.os_indicate);

		if (MLME_IS_AP(adapt)) {
			struct sk_buff *pskb2 = NULL;
			struct sta_info *psta = NULL;
			struct sta_priv *pstapriv = &adapt->stapriv;
			int bmcast = IS_MCAST(ehdr->h_dest);

			/* RTW_INFO("bmcast=%d\n", bmcast); */

			if (memcmp(ehdr->h_dest, adapter_mac_addr(adapt), ETH_ALEN)) {
				/* RTW_INFO("not ap psta=%p, addr=%pM\n", psta, ehdr->h_dest); */

				if (bmcast) {
					psta = rtw_get_bcmc_stainfo(adapt);
					pskb2 = rtw_skb_clone(pkt);
				} else
					psta = rtw_get_stainfo(pstapriv, ehdr->h_dest);

				if (psta) {
					struct net_device *pnetdev = (struct net_device *)adapt->pnetdev;

					/* RTW_INFO("directly forwarding to the rtw_xmit_entry\n"); */

					/* skb->ip_summed = CHECKSUM_NONE; */
					pkt->dev = pnetdev;
					#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
					skb_set_queue_mapping(pkt, rtw_recv_select_queue(pkt));
					#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35) */

					_rtw_xmit_entry(pkt, pnetdev);

					if (bmcast && (pskb2)) {
						pkt = pskb2;
						DBG_COUNTER(adapt->rx_logs.os_indicate_ap_mcast);
					} else {
						DBG_COUNTER(adapt->rx_logs.os_indicate_ap_forward);
						return;
					}
				}
			} else { /* to APself */
				/* RTW_INFO("to APSelf\n"); */
				DBG_COUNTER(adapt->rx_logs.os_indicate_ap_self);
			}
		}
		if (check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_ADHOC_STATE)) {
			/* Insert NAT2.5 RX here! */
			#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
			br_port = adapt->pnetdev->br_port;
			#else
			rcu_read_lock();
			br_port = rcu_dereference(adapt->pnetdev->rx_handler_data);
			rcu_read_unlock();
			#endif

			if (br_port) {
				if (nat25_handle_frame(adapt, pkt) == -1) {
					/* priv->ext_stats.rx_data_drops++; */
					/* DEBUG_ERR("RX DROP: nat25_handle_frame fail!\n"); */
					/* return FAIL; */

					/* bypass this frame to upper layer!! */
				}
			}
		}
		/* After eth_type_trans process , pkt->data pointer will move from ethrnet header to ip header */
		pkt->protocol = eth_type_trans(pkt, adapt->pnetdev);
		pkt->dev = adapt->pnetdev;
		pkt->ip_summed = CHECKSUM_NONE; /* CONFIG_TCP_CSUM_OFFLOAD_RX */

#ifdef CONFIG_RTW_NAPI
		if (pregistrypriv->en_napi
			#ifdef CONFIG_RTW_NAPI_DYNAMIC
			&& adapter_to_dvobj(adapt)->en_napi_dynamic
			#endif
		) {
			skb_queue_tail(&precvpriv->rx_napi_skb_queue, pkt);
			#ifndef CONFIG_RTW_NAPI_V2
			napi_schedule(&adapt->napi);
			#endif
			return;
		}
#endif /* CONFIG_RTW_NAPI */

		ret = rtw_netif_rx(adapt->pnetdev, pkt);
		if (ret == NET_RX_SUCCESS)
			DBG_COUNTER(adapt->rx_logs.os_netif_ok);
		else
			DBG_COUNTER(adapt->rx_logs.os_netif_err);
	}
}

void rtw_handle_tkip_mic_err(struct adapter *adapt, struct sta_info *sta, u8 bgroup)
{
	enum nl80211_key_type key_type = 0;
	union iwreq_data wrqu;
	struct iw_michaelmicfailure    ev;
	struct security_priv	*psecuritypriv = &adapt->securitypriv;
	unsigned long cur_time = 0;

	if (psecuritypriv->last_mic_err_time == 0)
		psecuritypriv->last_mic_err_time = rtw_get_current_time();
	else {
		cur_time = rtw_get_current_time();

		if (cur_time - psecuritypriv->last_mic_err_time < 60 * HZ) {
			psecuritypriv->btkip_countermeasure = true;
			psecuritypriv->last_mic_err_time = 0;
			psecuritypriv->btkip_countermeasure_time = cur_time;
		} else
			psecuritypriv->last_mic_err_time = rtw_get_current_time();
	}
	if (bgroup)
		key_type |= NL80211_KEYTYPE_GROUP;
	else
		key_type |= NL80211_KEYTYPE_PAIRWISE;

	cfg80211_michael_mic_failure(adapt->pnetdev, sta->cmn.mac_addr, key_type, -1, NULL, GFP_ATOMIC);

	memset(&ev, 0x00, sizeof(ev));
	if (bgroup)
		ev.flags |= IW_MICFAILURE_GROUP;
	else
		ev.flags |= IW_MICFAILURE_PAIRWISE;

	ev.src_addr.sa_family = ARPHRD_ETHER;
	memcpy(ev.src_addr.sa_data, sta->cmn.mac_addr, ETH_ALEN);

	memset(&wrqu, 0x00, sizeof(wrqu));
	wrqu.data.length = sizeof(ev);
}

int rtw_recv_monitor(struct adapter *adapt, union recv_frame *precv_frame)
{
	int ret = _FAIL;
	struct recv_priv *precvpriv;
	struct __queue	*pfree_recv_queue;
	struct sk_buff *skb;
	struct rx_pkt_attrib *pattrib;

	if (!precv_frame)
		goto _recv_drop;
	pattrib = &precv_frame->u.hdr.attrib;
	precvpriv = &(adapt->recvpriv);
	pfree_recv_queue = &(precvpriv->free_recv_queue);

	skb = precv_frame->u.hdr.pkt;
	if (!skb) {
		RTW_INFO("%s :skb==NULL something wrong!!!!\n", __func__);
		goto _recv_drop;
	}

	skb->data = precv_frame->u.hdr.rx_data;
	skb_set_tail_pointer(skb, precv_frame->u.hdr.len);
	skb->len = precv_frame->u.hdr.len;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(0x0019); /* ETH_P_80211_RAW */

	rtw_netif_rx(adapt->pnetdev, skb);

	/* pointers to NULL before rtw_free_recvframe() */
	precv_frame->u.hdr.pkt = NULL;

	ret = _SUCCESS;

_recv_drop:

	/* enqueue back to free_recv_queue */
	if (precv_frame)
		rtw_free_recvframe(precv_frame, pfree_recv_queue);

	return ret;

}

int rtw_recv_indicatepkt(struct adapter *adapt, union recv_frame *precv_frame)
{
	struct recv_priv *precvpriv;
	struct __queue	*pfree_recv_queue;
	struct sk_buff *skb;

	precvpriv = &(adapt->recvpriv);
	pfree_recv_queue = &(precvpriv->free_recv_queue);

	skb = precv_frame->u.hdr.pkt;
	if (!skb)
		goto _recv_indicatepkt_drop;

	skb->data = precv_frame->u.hdr.rx_data;
	skb_set_tail_pointer(skb, precv_frame->u.hdr.len);
	skb->len = precv_frame->u.hdr.len;

	rtw_os_recv_indicate_pkt(adapt, skb, precv_frame);

	precv_frame->u.hdr.pkt = NULL;
	rtw_free_recvframe(precv_frame, pfree_recv_queue);
	return _SUCCESS;

_recv_indicatepkt_drop:
	rtw_free_recvframe(precv_frame, pfree_recv_queue);
	DBG_COUNTER(adapt->rx_logs.os_indicate_err);
	return _FAIL;
}

void rtw_os_read_port(struct adapter *adapt, struct recv_buf *precvbuf)
{
	struct recv_priv *precvpriv = &adapt->recvpriv;

	precvbuf->ref_cnt--;

	/* free skb in recv_buf */
	dev_kfree_skb_any(precvbuf->pskb);

	precvbuf->pskb = NULL;

	if (!precvbuf->irp_pending)
		rtw_read_port(adapt, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
}

