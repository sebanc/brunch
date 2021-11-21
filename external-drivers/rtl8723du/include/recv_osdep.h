/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RECV_OSDEP_H_
#define __RECV_OSDEP_H_


int _rtw_init_recv_priv(struct recv_priv *precvpriv, struct adapter *adapt);
void _rtw_free_recv_priv(struct recv_priv *precvpriv);


int  rtw_recv_entry(union recv_frame *precv_frame);
int rtw_recv_indicatepkt(struct adapter *adapter, union recv_frame *precv_frame);
void rtw_recv_returnpacket(struct  net_device * cnxt, struct sk_buff *preturnedpkt);

int rtw_recv_monitor(struct adapter *adapt, union recv_frame *precv_frame);

struct sta_info;
void rtw_handle_tkip_mic_err(struct adapter *adapt, struct sta_info *sta, u8 bgroup);


int rtw_os_recv_resource_init(struct recv_priv *precvpriv, struct adapter *adapt);
int rtw_os_recv_resource_alloc(struct adapter *adapt, union recv_frame *precvframe);
void rtw_os_recv_resource_free(struct recv_priv *precvpriv);


int rtw_os_alloc_recvframe(struct adapter *adapt, union recv_frame *precvframe,
			   u8 *pdata, struct sk_buff *pskb);
int rtw_os_recvframe_duplicate_skb(struct adapter *adapt,
				   union recv_frame *pcloneframe,
				   struct sk_buff *pskb);
void rtw_os_free_recvframe(union recv_frame *precvframe);


int rtw_os_recvbuf_resource_alloc(struct adapter *adapt, struct recv_buf *precvbuf);
int rtw_os_recvbuf_resource_free(struct adapter *adapt, struct recv_buf *precvbuf);

struct sk_buff *rtw_os_alloc_msdu_pkt(union recv_frame *prframe, u16 nSubframe_Length, u8 *pdata);
void rtw_os_recv_indicate_pkt(struct adapter *adapt, struct sk_buff *pkt, union recv_frame *rframe);

void rtw_os_read_port(struct adapter *adapt, struct recv_buf *precvbuf);

#ifdef CONFIG_RTW_NAPI
#include <linux/netdevice.h>	/* struct napi_struct */

int rtw_recv_napi_poll(struct napi_struct *, int budget);
#endif /* CONFIG_RTW_NAPI */

#endif /*  */
