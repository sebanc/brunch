/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __XMIT_OSDEP_H_
#define __XMIT_OSDEP_H_


struct pkt_file {
	struct sk_buff *pkt;
	__kernel_size_t pkt_len;	 /* the remainder length of the open_file */
	unsigned char *cur_buffer;
	u8 *buf_start;
	u8 *cur_addr;
	__kernel_size_t buf_len;
};

#define NR_XMITFRAME	256

struct xmit_priv;
struct pkt_attrib;
struct sta_xmit_priv;
struct xmit_frame;
struct xmit_buf;

extern int _rtw_xmit_entry(struct sk_buff *pkt, struct  net_device * pnetdev);
extern int rtw_xmit_entry(struct sk_buff *pkt, struct  net_device * pnetdev);

void rtw_os_xmit_schedule(struct adapter *adapt);

int rtw_os_xmit_resource_alloc(struct adapter *adapt, struct xmit_buf *pxmitbuf, u32 alloc_sz, u8 flag);
void rtw_os_xmit_resource_free(struct adapter *adapt, struct xmit_buf *pxmitbuf, u32 free_sz, u8 flag);

extern void rtw_set_tx_chksum_offload(struct sk_buff *pkt, struct pkt_attrib *pattrib);

extern uint rtw_remainder_len(struct pkt_file *pfile);
extern void _rtw_open_pktfile(struct sk_buff *pkt, struct pkt_file *pfile);
extern uint _rtw_pktfile_read(struct pkt_file *pfile, u8 *rmem, uint rlen);
extern int rtw_endofpktfile(struct pkt_file *pfile);

extern void rtw_os_pkt_complete(struct adapter *adapt, struct sk_buff *pkt);
extern void rtw_os_xmit_complete(struct adapter *adapt, struct xmit_frame *pxframe);

void rtw_os_wake_queue_at_free_stainfo(struct adapter *adapt, int *qcnt_freed);

void dump_os_queue(void *sel, struct adapter *adapt);

#endif /* __XMIT_OSDEP_H_ */
