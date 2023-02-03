/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _RTW_RECV_H_
#define _RTW_RECV_H_

#ifdef CONFIG_SINGLE_RECV_BUF
	#define NR_RECVBUFF (1)
#else
	#define NR_RECVBUFF (8)
#endif /* CONFIG_SINGLE_RECV_BUF */
#define NR_PREALLOC_RECV_SKB 8

#ifdef CONFIG_RTW_NAPI
	#define RTL_NAPI_WEIGHT (32)
#endif

#define NR_RECVFRAME 256

#define RXFRAME_ALIGN	8
#define RXFRAME_ALIGN_SZ	(1<<RXFRAME_ALIGN)

#define DRVINFO_SZ	4 /* unit is 8bytes */

#define MAX_RXFRAME_CNT	512
#define MAX_RX_NUMBLKS		(32)
#define RECVFRAME_HDR_ALIGN 128
#define MAX_CONTINUAL_NORXPACKET_COUNT 4    /*  In MAX_CONTINUAL_NORXPACKET_COUNT*2 sec  , no rx traffict would issue DELBA*/

#define PHY_RSSI_SLID_WIN_MAX				100
#define PHY_LINKQUALITY_SLID_WIN_MAX		20

#define MAX_SUBFRAME_COUNT     64
 
#define SNAP_SIZE sizeof(struct ieee80211_snap_hdr)

#define RX_MPDU_QUEUE				0
#define RX_CMD_QUEUE				1
#define RX_MAX_QUEUE				2

#define MAX_SUBFRAME_COUNT	64

/* for Rx reordering buffer control */
struct recv_reorder_ctrl {
	struct adapter	*adapt;
	u8 tid;
	u8 enable;
	__le16 indicate_seq;/* =wstart_b, init_value=0xffff */
	__le16 wend_b;
	u8 wsize_b;
	u8 ampdu_size;
	struct __queue pending_recvframe_queue;
	struct timer_list reordering_ctrl_timer;
	u8 bReorderWaiting;
};

struct	stainfo_rxcache	{
	u16	tid_rxseq[16];
	u8 iv[16][8];
	u8 last_tid;
};


struct smooth_rssi_data {
	u32	elements[100];	/* array to store values */
	u32	index;			/* index to current array to store */
	u32	total_num;		/* num of valid elements */
	u32	total_val;		/* sum of valid elements */
};

struct signal_stat {
	u8	update_req;		/* used to indicate */
	u8	avg_val;		/* avg of valid elements */
	u32	total_num;		/* num of valid elements */
	u32	total_val;		/* sum of valid elements	 */
};

struct rx_raw_rssi {
	u8 data_rate;
	u8 pwdball;
	s8 pwr_all;

	u8 mimo_signal_strength[4];/* in 0~100 index */
	u8 mimo_signal_quality[4];

	s8 ofdm_pwr[4];
	u8 ofdm_snr[4];
};


#include "rtw_sta_info.h"

struct rx_pkt_attrib	{
	__le16	pkt_len;
	u8	physt;
	u8	drvinfo_sz;
	u8	shift_sz;
	u8	hdrlen; /* the WLAN Header Len */
	u8	to_fr_ds;
	u8	amsdu;
	u8	qos;
	u8	priority;
	u8	pw_save;
	u8	mdata;
	__le16	seq_num;
	u8	frag_num;
	u8	mfrag;
	u8	order;
	u8	privacy; /* in frame_ctrl field */
	u8	bdecrypted;
	u8	encrypt; /* when 0 indicate no encrypt. when non-zero, indicate the encrypt algorith */
	u8	iv_len;
	u8	icv_len;
	u8	crc_err;
	u8	icv_err;

	__le16	eth_type;

	u8	dst[ETH_ALEN];
	u8	src[ETH_ALEN];
	u8	ta[ETH_ALEN];
	u8	ra[ETH_ALEN];
	u8	bssid[ETH_ALEN];

	u8	ack_policy;

	u8	key_index;

	u8	data_rate;
	u8 ch; /* RX channel */
	u8	bw;
	u8	stbc;
	u8	ldpc;
	u8	sgi;
	u8	pkt_rpt_type;
	__le32 tsfl;
	__le32	MacIDValidEntry[2];	/* 64 bits present 64 entry. */
	u8	ppdu_cnt;
	__le32 	free_cnt;		/* free run counter */
	struct phydm_phyinfo_struct phy_info;
};


/* These definition is used for Rx packet reordering. */
static inline bool SN_LESS(__le16 a, __le16 b)
{
	return (((le16_to_cpu(a)-le16_to_cpu(b)) & 0x800) != 0);
}
#define SN_EQUAL(a, b)	(a == b)
/* #define REORDER_WIN_SIZE	128 */
/* #define REORDER_ENTRY_NUM	128 */
#define REORDER_WAIT_TIME	(50) /* (ms) */

#define RECVBUFF_ALIGN_SZ 8

#ifdef CONFIG_TRX_BD_ARCH
	#define RX_WIFI_INFO_SIZE	24
#endif

#define RXDESC_SIZE	24
#define RXDESC_OFFSET RXDESC_SIZE

#ifdef CONFIG_TRX_BD_ARCH
struct rx_buf_desc {
	/* RX has exactly one segment */
#ifdef CONFIG_64BIT_DMA
	unsigned int dword[4];
#else
	unsigned int dword[2];
#endif
};

struct recv_stat {
	unsigned int rxdw[8];
};
#else
struct recv_stat {
	unsigned int rxdw0;

	unsigned int rxdw1;

#ifndef BUF_DESC_ARCH
	unsigned int rxdw4;

	unsigned int rxdw5;

#endif /* if BUF_DESC_ARCH is defined, rx_buf_desc occupy 4 double words */
};
#endif

#define EOR BIT(30)

/*
accesser of recv_priv: rtw_recv_entry(dispatch / passive level); recv_thread(passive) ; returnpkt(dispatch)
; halt(passive) ;

using enter_critical section to protect
*/

struct recv_priv {
	spinlock_t	lock;
	struct __queue	free_recv_queue;
	struct __queue	recv_pending_queue;
	struct __queue	uc_swdec_pending_queue;

	u8 *pallocated_frame_buf;
	u8 *precv_frame_buf;

	uint free_recvframe_cnt;

	struct adapter	*adapter;

	u32 is_any_non_be_pkts;

	u64	rx_bytes;
	u64	rx_pkts;
	u64	rx_drop;

	u64 dbg_rx_drop_count;
	u64 dbg_rx_ampdu_drop_count;
	u64 dbg_rx_ampdu_forced_indicate_count;
	u64 dbg_rx_ampdu_loss_count;
	u64 dbg_rx_dup_mgt_frame_drop_count;
	u64 dbg_rx_ampdu_window_shift_cnt;
	u64 dbg_rx_conflic_mac_addr_cnt;

	uint  rx_icv_err;
	uint  rx_largepacket_crcerr;
	uint  rx_smallpacket_crcerr;
	uint  rx_middlepacket_crcerr;

	/* u8 *pallocated_urb_buf; */
	struct semaphore allrxreturnevt;
	uint	ff_hwaddr;
	ATOMIC_T	rx_pending_cnt;

	struct tasklet_struct irq_prepare_beacon_tasklet;
	struct tasklet_struct recv_tasklet;
	struct sk_buff_head free_recv_skb_queue;
	struct sk_buff_head rx_skb_queue;
#ifdef CONFIG_RTW_NAPI
		struct sk_buff_head rx_napi_skb_queue;
#endif 
#ifdef CONFIG_RX_INDICATE_QUEUE
	struct task rx_indicate_tasklet;
	struct ifqueue rx_indicate_queue;
#endif /* CONFIG_RX_INDICATE_QUEUE */

	u8 *pallocated_recv_buf;
	u8 *precv_buf;    /* 4 alignment */
	struct __queue	free_recv_buf_queue;
	u32	free_recv_buf_queue_cnt;

	struct __queue	recv_buf_pending_queue;

	/* For display the phy informatiom */
	u8 is_signal_dbg;	/* for debug */
	u8 signal_strength_dbg;	/* for debug */

	u8 signal_strength;
	u8 signal_qual;
	s8 rssi;	/* translate_percentage_to_dbm(ptarget_wlan->network.PhyInfo.SignalStrength); */
	struct rx_raw_rssi raw_rssi_info;
	/* s8 rxpwdb;	 */
	/* int RxSNRdB[2]; */
	/* s8 RxRssi[2]; */
	/* int FalseAlmCnt_all; */


	struct timer_list signal_stat_timer;
	u32 signal_stat_sampling_interval;
	/* u32 signal_stat_converging_constant; */
	struct signal_stat signal_qual_data;
	struct signal_stat signal_strength_data;
	u16 sink_udpport, pre_rtp_rxseq, cur_rtp_rxseq;

	bool store_law_data_flag;
};

#define RX_BH_STG_UNKNOWN		0
#define RX_BH_STG_HDL_ENTER		1
#define RX_BH_STG_HDL_EXIT		2
#define RX_BH_STG_NEW_BUF		3
#define RX_BH_STG_NEW_FRAME		4
#define RX_BH_STG_NORMAL_RX		5
#define RX_BH_STG_NORMAL_RX_END	6
#define RX_BH_STG_C2H			7
#define RX_BH_STG_C2H_END		8

#define rx_bh_tk_set_stage(recv, s) do {} while (0)
#define rx_bh_tk_set_buf(recv, buf, data, dlen) do {} while (0)
#define rx_bh_tk_set_buf_pos(recv, pos) do {} while (0)
#define rx_bh_tk_set_frame(recv, frame) do {} while (0)
#define dump_rx_bh_tk(sel, recv) do {} while (0)

#define rtw_set_signal_stat_timer(recvpriv)		\
	_set_timer(&(recvpriv)->signal_stat_timer,	\
		   (recvpriv)->signal_stat_sampling_interval)

struct sta_recv_priv {
	spinlock_t	lock;
	int	option;
	struct __queue defrag_q;	 /* keeping the fragment frame until defrag */
	struct	stainfo_rxcache rxcache;
};

struct recv_buf {
	struct list_head list;

	spinlock_t recvbuf_lock;

	u32	ref_cnt;

	struct adapter * adapter;

	u8	*pbuf;
	u8	*pallocated_buf;

	u32	len;
	u8	*phead;
	u8	*pdata;
	u8	*ptail;
	u8	*pend;

	struct urb *	purb;
	dma_addr_t dma_transfer_addr;	/* (in) dma addr for transfer_buffer */
	u32 alloc_sz;
	u8  irp_pending;
	int  transfer_len;

	struct sk_buff *pskb;
};


/*
	head  ----->

		data  ----->

			payload

		tail  ----->


	end   ----->

	len = (unsigned int )(tail - data);

*/
struct recv_frame_hdr {
	struct list_head	list;
	struct sk_buff *pkt;

	struct adapter  *adapter;

	u8 fragcnt;

	int frame_tag;

	struct rx_pkt_attrib attrib;

	uint  len;
	u8 *rx_head;
	u8 *rx_data;
	u8 *rx_tail;
	u8 *rx_end;

	void *precvbuf;


	/*  */
	struct sta_info *psta;
	/* for A-MPDU Rx reordering buffer control */
	struct recv_reorder_ctrl *preorder_ctrl;
};


union recv_frame {

	union {
		struct list_head list;
		struct recv_frame_hdr hdr;
		uint mem[RECVFRAME_HDR_ALIGN >> 2];
	} u;

	/* uint mem[MAX_RXSZ>>2]; */

};

bool rtw_rframe_del_wfd_ie(union recv_frame *rframe, u8 ies_offset);

enum RX_PACKET_TYPE {
	NORMAL_RX,/* Normal rx packet */
	TX_REPORT1,/* CCX */
	TX_REPORT2,/* TX RPT */
	HIS_REPORT,/* USB HISR RPT */
	C2H_PACKET
};

union recv_frame *_rtw_alloc_recvframe(struct __queue *pfree_recv_queue);   /* get a free recv_frame from pfree_recv_queue */
union recv_frame *rtw_alloc_recvframe(struct __queue *pfree_recv_queue);   /* get a free recv_frame from pfree_recv_queue */
void rtw_init_recvframe(union recv_frame *precvframe , struct recv_priv *precvpriv);
int rtw_free_recvframe(union recv_frame *precvframe, struct __queue *pfree_recv_queue);

#define rtw_dequeue_recvframe(queue) rtw_alloc_recvframe(queue)
int _rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue);
int rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue);

void rtw_free_recvframe_queue(struct __queue *pframequeue,  struct __queue *pfree_recv_queue);
u32 rtw_free_uc_swdec_pending_queue(struct adapter *adapter);

int rtw_enqueue_recvbuf_to_head(struct recv_buf *precvbuf, struct __queue *queue);
int rtw_enqueue_recvbuf(struct recv_buf *precvbuf, struct __queue *queue);
struct recv_buf *rtw_dequeue_recvbuf(struct __queue *queue);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void rtw_reordering_ctrl_timeout_handler(struct timer_list *t);
#else
void rtw_reordering_ctrl_timeout_handler(void *pcontext);
#endif

void rx_query_phy_status(union recv_frame *rframe, u8 *phy_stat);
int rtw_inc_and_chk_continual_no_rx_packet(struct sta_info *sta, int tid_index);
void rtw_reset_continual_no_rx_packet(struct sta_info *sta, int tid_index);

static inline u8 *get_rxmem(union recv_frame *precvframe)
{
	/* always return rx_head... */
	if (!precvframe)
		return NULL;

	return precvframe->u.hdr.rx_head;
}

static inline u8 *get_rx_status(union recv_frame *precvframe)
{

	return get_rxmem(precvframe);

}

static inline u8 *get_recvframe_data(union recv_frame *precvframe)
{

	/* alwasy return rx_data */
	if (!precvframe)
		return NULL;

	return precvframe->u.hdr.rx_data;

}

static inline u8 *recvframe_push(union recv_frame *precvframe, int sz)
{
	/* append data before rx_data */

	/* add data to the start of recv_frame
	*
	*      This function extends the used data area of the recv_frame at the buffer
	*      start. rx_data must be still larger than rx_head, after pushing.
	*/

	if (!precvframe)
		return NULL;


	precvframe->u.hdr.rx_data -= sz ;
	if (precvframe->u.hdr.rx_data < precvframe->u.hdr.rx_head) {
		precvframe->u.hdr.rx_data += sz ;
		return NULL;
	}

	precvframe->u.hdr.len += sz;

	return precvframe->u.hdr.rx_data;

}

static inline u8 *recvframe_pull(union recv_frame *precvframe, int sz)
{
	/* rx_data += sz; move rx_data sz bytes  hereafter */

	/* used for extract sz bytes from rx_data, update rx_data and return the updated rx_data to the caller */


	if (!precvframe)
		return NULL;


	precvframe->u.hdr.rx_data += sz;

	if (precvframe->u.hdr.rx_data > precvframe->u.hdr.rx_tail) {
		precvframe->u.hdr.rx_data -= sz;
		return NULL;
	}

	precvframe->u.hdr.len -= sz;

	return precvframe->u.hdr.rx_data;
}

static inline u8 *recvframe_put(union recv_frame *precvframe, __le16 le_sz)
{
	s16 sz = le16_to_cpu(le_sz);
	/* rx_tai += sz; move rx_tail sz bytes  hereafter */

	/* used for append sz bytes from ptr to rx_tail, update rx_tail and return the updated rx_tail to the caller */
	/* after putting, rx_tail must be still larger than rx_end. */
	unsigned char *prev_rx_tail;

	if (!precvframe)
		return NULL;

	prev_rx_tail = precvframe->u.hdr.rx_tail;

	precvframe->u.hdr.rx_tail += sz;

	if (precvframe->u.hdr.rx_tail > precvframe->u.hdr.rx_end) {
		precvframe->u.hdr.rx_tail -= sz;
		return NULL;
	}

	precvframe->u.hdr.len += sz;

	return precvframe->u.hdr.rx_tail;
}

static inline u8 *recvframe_pull_tail(union recv_frame *precvframe, int sz)
{
	/* rmv data from rx_tail (by yitsen) */

	/* used for extract sz bytes from rx_end, update rx_end and return the updated rx_end to the caller */
	/* after pulling, rx_end must be still larger than rx_data. */

	if (!precvframe)
		return NULL;

	precvframe->u.hdr.rx_tail -= sz;

	if (precvframe->u.hdr.rx_tail < precvframe->u.hdr.rx_data) {
		precvframe->u.hdr.rx_tail += sz;
		return NULL;
	}

	precvframe->u.hdr.len -= sz;

	return precvframe->u.hdr.rx_tail;
}

static inline unsigned char *get_rxbuf_desc(union recv_frame *precvframe)
{
	unsigned char *buf_desc;

	if (!precvframe)
		return NULL;
	return buf_desc;
}

static inline union recv_frame *rxmem_to_recvframe(u8 *rxmem)
{
	/* due to the design of 2048 bytes alignment of recv_frame, we can reference the union recv_frame */
	/* from any given member of recv_frame. */
	/* rxmem indicates the any member/address in recv_frame */

	return (union recv_frame *)(((SIZE_PTR)rxmem >> RXFRAME_ALIGN) << RXFRAME_ALIGN);

}

static inline union recv_frame *pkt_to_recvframe(struct sk_buff *pkt)
{

	u8 *buf_star;
	union recv_frame *precv_frame;
	precv_frame = rxmem_to_recvframe((unsigned char *)buf_star);

	return precv_frame;
}

static inline u8 *pkt_to_recvmem(struct sk_buff *pkt)
{
	/* return the rx_head */

	union recv_frame *precv_frame = pkt_to_recvframe(pkt);

	return	precv_frame->u.hdr.rx_head;

}

static inline u8 *pkt_to_recvdata(struct sk_buff *pkt)
{
	/* return the rx_data */

	union recv_frame *precv_frame = pkt_to_recvframe(pkt);

	return	precv_frame->u.hdr.rx_data;

}


static inline int get_recvframe_len(union recv_frame *precvframe)
{
	return precvframe->u.hdr.len;
}


static inline int translate_percentage_to_dbm(u32 SignalStrengthIndex)
{
	int	SignalPower; /* in dBm. */

	/* Translate to dBm (x=y-100) */
	SignalPower = SignalStrengthIndex - 100;

	return SignalPower;
}

struct sta_info;

void _rtw_init_sta_recv_priv(struct sta_recv_priv *psta_recvpriv);

void  mgt_dispatcher(struct adapter *adapt, union recv_frame *precv_frame);

u8 adapter_allow_bmc_data_rx(struct adapter *adapter);
int pre_recv_entry(union recv_frame *precvframe, u8 *pphy_status);

#endif
