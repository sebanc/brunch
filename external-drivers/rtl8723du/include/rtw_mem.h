/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_MEM_H__
#define __RTW_MEM_H__

#include <drv_conf.h>
#include <basic_types.h>
#include <osdep_service.h>

#define MAX_RTKM_RECVBUF_SZ (15360) /* 15k */
#define MAX_RTKM_NR_PREALLOC_RECV_SKB 16

u16 rtw_rtkm_get_buff_size(void);
u8 rtw_rtkm_get_nr_recv_skb(void);
struct u8 *rtw_alloc_revcbuf_premem(void);
struct sk_buff *rtw_alloc_skb_premem(u16 in_size);
int rtw_free_skb_premem(struct sk_buff *pskb);


#endif /* __RTW_MEM_H__ */
