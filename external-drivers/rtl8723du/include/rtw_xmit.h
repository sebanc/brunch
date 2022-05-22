/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _RTW_XMIT_H_
#define _RTW_XMIT_H_


	#define MAX_XMITBUF_SZ	(2048)

	#ifdef CONFIG_SINGLE_XMIT_BUF
		#define NR_XMITBUFF	(1)
	#else
		#define NR_XMITBUFF	(4)
	#endif /* CONFIG_SINGLE_XMIT_BUF */

		#ifdef USB_XMITBUF_ALIGN_SZ
			#define XMITBUF_ALIGN_SZ (USB_XMITBUF_ALIGN_SZ)
		#else
			#define XMITBUF_ALIGN_SZ 512
		#endif

/* xmit extension buff defination */
#define MAX_XMIT_EXTBUF_SZ	(1536)

#ifdef CONFIG_SINGLE_XMIT_BUF
	#define NR_XMIT_EXTBUFF	(1)
#else
	#define NR_XMIT_EXTBUFF	(32)
#endif

#define MAX_CMDBUF_SZ   (5120)  /* (4096) */

#define MAX_NUMBLKS		(1)

#define XMIT_VO_QUEUE (0)
#define XMIT_VI_QUEUE (1)
#define XMIT_BE_QUEUE (2)
#define XMIT_BK_QUEUE (3)

#define VO_QUEUE_INX		0
#define VI_QUEUE_INX		1
#define BE_QUEUE_INX		2
#define BK_QUEUE_INX		3
#define BCN_QUEUE_INX		4
#define MGT_QUEUE_INX		5
#define HIGH_QUEUE_INX		6
#define TXCMD_QUEUE_INX	7

#define HW_QUEUE_ENTRY	8

#define WEP_IV(pattrib_iv, dot11txpn, keyidx)\
	do {\
		dot11txpn.val = (dot11txpn.val == 0xffffff) ? 0 : (dot11txpn.val + 1);\
		pattrib_iv[0] = dot11txpn._byte_.TSC0;\
		pattrib_iv[1] = dot11txpn._byte_.TSC1;\
		pattrib_iv[2] = dot11txpn._byte_.TSC2;\
		pattrib_iv[3] = ((keyidx & 0x3)<<6);\
	} while (0)


#define TKIP_IV(pattrib_iv, dot11txpn, keyidx)\
	do {\
		dot11txpn.val = dot11txpn.val == 0xffffffffffffULL ? 0 : (dot11txpn.val + 1);\
		pattrib_iv[0] = dot11txpn._byte_.TSC1;\
		pattrib_iv[1] = (dot11txpn._byte_.TSC1 | 0x20) & 0x7f;\
		pattrib_iv[2] = dot11txpn._byte_.TSC0;\
		pattrib_iv[3] = BIT(5) | ((keyidx & 0x3)<<6);\
		pattrib_iv[4] = dot11txpn._byte_.TSC2;\
		pattrib_iv[5] = dot11txpn._byte_.TSC3;\
		pattrib_iv[6] = dot11txpn._byte_.TSC4;\
		pattrib_iv[7] = dot11txpn._byte_.TSC5;\
	} while (0)

#define AES_IV(pattrib_iv, dot11txpn, keyidx)\
	do {\
		dot11txpn.val = dot11txpn.val == 0xffffffffffffULL ? 0 : (dot11txpn.val + 1);\
		pattrib_iv[0] = dot11txpn._byte_.TSC0;\
		pattrib_iv[1] = dot11txpn._byte_.TSC1;\
		pattrib_iv[2] = 0;\
		pattrib_iv[3] = BIT(5) | ((keyidx & 0x3)<<6);\
		pattrib_iv[4] = dot11txpn._byte_.TSC2;\
		pattrib_iv[5] = dot11txpn._byte_.TSC3;\
		pattrib_iv[6] = dot11txpn._byte_.TSC4;\
		pattrib_iv[7] = dot11txpn._byte_.TSC5;\
	} while (0)

/* Check if AMPDU Tx is supported or not. If it is supported,
* it need to check "amsdu in ampdu" is supported or not.
* (ampdu_en, amsdu_ampdu_en) =
* (0, x) : AMPDU is not enable, but AMSDU is valid to send.
* (1, 0) : AMPDU is enable, AMSDU in AMPDU is not enable. So, AMSDU is not valid to send.
* (1, 1) : AMPDU and AMSDU in AMPDU are enable. So, AMSDU is valid to send.
*/
#define IS_AMSDU_AMPDU_NOT_VALID(pattrib)\
	 ((pattrib->ampdu_en) && (!pattrib->amsdu_ampdu_en))

#define IS_AMSDU_AMPDU_VALID(pattrib)\
	 !((pattrib->ampdu_en) && (!pattrib->amsdu_ampdu_en))

#define HWXMIT_ENTRY	4

/* For Buffer Descriptor ring architecture */
#if defined(BUF_DESC_ARCH) || defined(CONFIG_TRX_BD_ARCH)
	#define TX_BUFFER_SEG_NUM	1 /* 0:2 seg, 1: 4 seg, 2: 8 seg. */
#endif

#define TXDESC_SIZE 40

#ifdef USB_PACKET_OFFSET_SZ
	#define PACKET_OFFSET_SZ (USB_PACKET_OFFSET_SZ)
#else
	#define PACKET_OFFSET_SZ (8)
#endif
#define TXDESC_OFFSET (TXDESC_SIZE + PACKET_OFFSET_SZ)

enum TXDESC_SC {
	SC_DONT_CARE = 0x00,
	SC_UPPER = 0x01,
	SC_LOWER = 0x02,
	SC_DUPLICATE = 0x03
};

#define TXDESC_40_BYTES

#ifdef CONFIG_TRX_BD_ARCH
struct tx_buf_desc {
#ifdef CONFIG_64BIT_DMA
#define TX_BUFFER_SEG_SIZE	4	/* in unit of DWORD */
#else
#define TX_BUFFER_SEG_SIZE	2	/* in unit of DWORD */
#endif
	unsigned int dword[TX_BUFFER_SEG_SIZE * (2 << TX_BUFFER_SEG_NUM)];
} __packed;
#else
struct tx_desc {
	__le32 txdw0;
	__le32 txdw1;
	__le32 txdw2;
	__le32 txdw3;
	__le32 txdw4;
	__le32 txdw5;
	__le32 txdw6;
	__le32 txdw7;

#if defined(TXDESC_40_BYTES) || defined(TXDESC_64_BYTES)
	__le32 txdw8;
	__le32 txdw9;
#endif /* TXDESC_40_BYTES */

#ifdef TXDESC_64_BYTES
	__le32 txdw10;
	__le32 txdw11;

	/* 2008/05/15 MH Because PCIE HW memory R/W 4K limit. And now,  our descriptor */
	/* size is 40 bytes. If you use more than 102 descriptor( 103*40>4096), HW will execute */
	/* memoryR/W CRC error. And then all DMA fetch will fail. We must decrease descriptor */
	/* number or enlarge descriptor size as 64 bytes. */
	__le32 txdw12;
	__le32 txdw13;
	__le32 txdw14;
	__le32 txdw15;
#endif
};
#endif

#ifndef CONFIG_TRX_BD_ARCH
union txdesc {
	struct tx_desc txdesc;
	__le32 value[TXDESC_SIZE >> 2];
};
#endif

struct	hw_xmit	{
	/* spinlock_t xmit_lock; */
	/* _list	pending; */
	struct __queue *sta_queue;
	/* struct hw_txqueue *phwtxqueue; */
	/* int	txcmdcnt; */
	int	accnt;
};

/* reduce size */
struct pkt_attrib {
	u8	type;
	u8	subtype;
	u8	bswenc;
	u8	dhcp_pkt;
	u16	ether_type;
	u16	seqnum;
	u8	hw_ssn_sel;	/* for HW_SEQ0,1,2,3 */
	u16	pkt_hdrlen;	/* the original 802.3 pkt header len */
	u16	hdrlen;		/* the WLAN Header Len */
	u32	pktlen;		/* the original 802.3 pkt raw_data len (not include ether_hdr data) */
	u32	last_txcmdsz;
	u8	nr_frags;
	u8	encrypt;	/* when 0 indicate no encrypt. when non-zero, indicate the encrypt algorith */
#if defined(CONFIG_CONCURRENT_MODE)
	u8	bmc_camid;
#endif
	u8	iv_len;
	u8	icv_len;
	u8	iv[18];
	u8	icv[16];
	u8	priority;
	u8	ack_policy;
	u8	mac_id;
	u8	vcs_mode;	/* virtual carrier sense method */
	u8	dst[ETH_ALEN];
	u8	src[ETH_ALEN];
	u8	ta[ETH_ALEN];
	u8	ra[ETH_ALEN];
	u8	key_idx;
	u8	qos_en;
	u8	ht_en;
	u8	raid;/* rate adpative id */
	u8	bwmode;
	u8	ch_offset;/* PRIME_CHNL_OFFSET */
	u8	sgi;/* short GI */
	u8	ampdu_en;/* tx ampdu enable */
	u8	ampdu_spacing; /* ampdu_min_spacing for peer sta's rx */
	u8	amsdu;
	u8	amsdu_ampdu_en;/* tx amsdu in ampdu enable */
	u8	mdata;/* more data bit */
	u8	pctrl;/* per packet txdesc control enable */
	u8	triggered;/* for ap mode handling Power Saving sta */
	u8	qsel;
	u8	order;/* order bit */
	u8	eosp;
	u8	rate;
	u8	intel_proxim;
	u8	retry_ctrl;
	u8   mbssid;
	u8	ldpc;
	u8	stbc;
	struct sta_info *psta;
#ifdef CONFIG_TCP_CSUM_OFFLOAD_TX
	u8	hw_tcp_csum;
#endif

	u8 rtsen;
	u8 cts2self;
	union Keytype	dot11tkiptxmickey;
	/* union Keytype	dot11tkiprxmickey; */
	union Keytype	dot118021x_UncstKey;
	u8 key_type;
	u8 icmp_pkt;
};

#ifdef CONFIG_TX_AMSDU
enum {
	RTW_AMSDU_TIMER_UNSET = 0,
	RTW_AMSDU_TIMER_SETTING,
	RTW_AMSDU_TIMER_TIMEOUT,
};
#endif

#define WLANHDR_OFFSET	64

#define NULL_FRAMETAG		(0x0)
#define DATA_FRAMETAG		0x01
#define L2_FRAMETAG		0x02
#define MGNT_FRAMETAG		0x03
#define AMSDU_FRAMETAG	0x04

#define EII_FRAMETAG		0x05
#define IEEE8023_FRAMETAG  0x06

#define MP_FRAMETAG		0x07

#define TXAGG_FRAMETAG	0x08

enum {
	XMITBUF_DATA = 0,
	XMITBUF_MGNT = 1,
	XMITBUF_CMD = 2,
};

bool rtw_xmit_ac_blocked(struct adapter *adapter);

struct  submit_ctx {
	unsigned long submit_time; /* */
	u32 timeout_ms; /* <0: not synchronous, 0: wait forever, >0: up to ms waiting */
	int status; /* status for operation */
	struct completion done;
};

enum {
	RTW_SCTX_SUBMITTED = -1,
	RTW_SCTX_DONE_SUCCESS = 0,
	RTW_SCTX_DONE_UNKNOWN,
	RTW_SCTX_DONE_TIMEOUT,
	RTW_SCTX_DONE_BUF_ALLOC,
	RTW_SCTX_DONE_BUF_FREE,
	RTW_SCTX_DONE_WRITE_PORT_ERR,
	RTW_SCTX_DONE_TX_DESC_NA,
	RTW_SCTX_DONE_TX_DENY,
	RTW_SCTX_DONE_CCX_PKT_FAIL,
	RTW_SCTX_DONE_DRV_STOP,
	RTW_SCTX_DONE_DEV_REMOVE,
	RTW_SCTX_DONE_CMD_ERROR,
	RTW_SCTX_DONE_CMD_DROP,
	RTX_SCTX_CSTR_WAIT_RPT2,
};


void rtw_sctx_init(struct submit_ctx *sctx, int timeout_ms);
int rtw_sctx_wait(struct submit_ctx *sctx, const char *msg);
void rtw_sctx_done_err(struct submit_ctx **sctx, int status);
void rtw_sctx_done(struct submit_ctx **sctx);

struct xmit_buf {
	struct list_head	list;

	struct adapter *adapt;

	u8 *pallocated_buf;

	u8 *pbuf;

	void *priv_data;

	u16 buf_tag; /* 0: Normal xmitbuf, 1: extension xmitbuf, 2:cmd xmitbuf */
	u16 flags;
	u32 alloc_sz;

	u32  len;

	struct submit_ctx *sctx;


	/* u32 sz[8]; */
	u32	ff_hwaddr;

	struct urb *	pxmit_urb[8];
	dma_addr_t dma_transfer_addr;	/* (in) dma addr for transfer_buffer */

	u8 bpending[8];

	int last[8];
};

struct xmit_frame {
	struct list_head	list;
	struct pkt_attrib attrib;
	struct sk_buff *pkt;
	struct xmit_buf *pxmitbuf;
	int	frame_tag;
	struct adapter *adapt;
	u8	*buf_addr;
	u8 *alloc_addr; /* the actual address this xmitframe allocated */
	s8	pkt_offset;
	u8 ack_report;
	u8 ext_tag; /* 0:data, 1:mgmt */
};

struct tx_servq {
	struct list_head	tx_pending;
	struct __queue	sta_pending;
	int qcnt;
};

struct sta_xmit_priv {
	spinlock_t	lock;
	int	option;
	int	apsd_setting;	/* When bit mask is on, the associated edca queue supports APSD. */


	/* struct tx_servq blk_q[MAX_NUMBLKS]; */
	struct tx_servq	be_q;			/* priority == 0,3 */
	struct tx_servq	bk_q;			/* priority == 1,2 */
	struct tx_servq	vi_q;			/* priority == 4,5 */
	struct tx_servq	vo_q;			/* priority == 6,7 */
	struct list_head	legacy_dz;
	struct list_head  apsd;

	u16 txseq_tid[16];

	/* uint	sta_tx_bytes; */
	/* u64	sta_tx_pkts; */
	/* uint	sta_tx_fail; */


};


struct	hw_txqueue	{
	volatile int	head;
	volatile int	tail;
	volatile int 	free_sz;	/* in units of 64 bytes */
	volatile int      free_cmdsz;
	volatile int	 txsz[8];
	uint	ff_hwaddr;
	uint	cmd_hwaddr;
	int	ac_tag;
};

struct agg_pkt_info {
	u16 offset;
	u16 pkt_len;
};

enum cmdbuf_type {
	CMDBUF_BEACON = 0x00,
	CMDBUF_RSVD,
	CMDBUF_MAX
};

u8 rtw_get_hwseq_no(struct adapter *adapt);

struct	xmit_priv	{

	spinlock_t	lock;

	struct semaphore	xmit_sema;

	/* struct __queue	blk_strms[MAX_NUMBLKS]; */
	struct __queue	be_pending;
	struct __queue	bk_pending;
	struct __queue	vi_pending;
	struct __queue	vo_pending;
	struct __queue	bm_pending;

	/* struct __queue	legacy_dz_queue; */
	/* struct __queue	apsd_queue; */

	u8 *pallocated_frame_buf;
	u8 *pxmit_frame_buf;
	uint free_xmitframe_cnt;
	struct __queue	free_xmit_queue;

	/* uint mapping_addr; */
	/* uint pkt_sz; */

	u8 *xframe_ext_alloc_addr;
	u8 *xframe_ext;
	uint free_xframe_ext_cnt;
	struct __queue free_xframe_ext_queue;

	/* struct	hw_txqueue	be_txqueue; */
	/* struct	hw_txqueue	bk_txqueue; */
	/* struct	hw_txqueue	vi_txqueue; */
	/* struct	hw_txqueue	vo_txqueue; */
	/* struct	hw_txqueue	bmc_txqueue; */

	uint	frag_len;

	struct adapter	*adapter;

	u8   vcs_setting;
	u8	vcs;
	u8	vcs_type;
	/* u16  rts_thresh; */

	u64	tx_bytes;
	u64	tx_pkts;
	u64	tx_drop;
	u64	last_tx_pkts;

	struct hw_xmit *hwxmits;
	u8	hwxmit_entry;

	u8	wmm_para_seq[4];/* sequence for wmm ac parameter strength from large to small. it's value is 0->vo, 1->vi, 2->be, 3->bk. */

	struct semaphore	tx_retevt;/* all tx return event; */
	u8		txirp_cnt;

	struct tasklet_struct xmit_tasklet;
	/* per AC pending irp */
	int beq_cnt;
	int bkq_cnt;
	int viq_cnt;
	int voq_cnt;

	struct __queue free_xmitbuf_queue;
	struct __queue pending_xmitbuf_queue;
	u8 *pallocated_xmitbuf;
	u8 *pxmitbuf;
	uint free_xmitbuf_cnt;

	struct __queue free_xmit_extbuf_queue;
	u8 *pallocated_xmit_extbuf;
	u8 *pxmit_extbuf;
	uint free_xmit_extbuf_cnt;

	struct xmit_buf	pcmd_xmitbuf[CMDBUF_MAX];
	u8   hw_ssn_seq_no;/* mapping to REG_HW_SEQ 0,1,2,3 */
	u16	nqos_ssn;
	int	ack_tx;
	_mutex ack_tx_mutex;
	struct submit_ctx ack_tx_ops;
	u8 seq_no;

#ifdef CONFIG_TX_AMSDU
	_timer amsdu_vo_timer;
	u8 amsdu_vo_timeout;

	_timer amsdu_vi_timer;
	u8 amsdu_vi_timeout;

	_timer amsdu_be_timer;
	u8 amsdu_be_timeout;

	_timer amsdu_bk_timer;
	u8 amsdu_bk_timeout;

	u32 amsdu_debug_set_timer;
	u32 amsdu_debug_timeout;
	u32 amsdu_debug_coalesce_one;
	u32 amsdu_debug_coalesce_two;

#endif
	spinlock_t lock_sctx;

};

struct xmit_frame *__rtw_alloc_cmdxmitframe(struct xmit_priv *pxmitpriv,
		enum cmdbuf_type buf_type);
#define rtw_alloc_cmdxmitframe(p) __rtw_alloc_cmdxmitframe(p, CMDBUF_RSVD)
#define rtw_alloc_bcnxmitframe(p) __rtw_alloc_cmdxmitframe(p, CMDBUF_BEACON)

struct xmit_buf *rtw_alloc_xmitbuf_ext(struct xmit_priv *pxmitpriv);
int rtw_free_xmitbuf_ext(struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf);

struct xmit_buf *rtw_alloc_xmitbuf(struct xmit_priv *pxmitpriv);
int rtw_free_xmitbuf(struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf);

void rtw_count_tx_stats(struct adapter *adapt, struct xmit_frame *pxmitframe, int sz);
void rtw_update_protection(struct adapter *adapt, u8 *ie, uint ie_len);

int rtw_make_wlanhdr(struct adapter *adapt, u8 *hdr, struct pkt_attrib *pattrib);
int rtw_put_snap(u8 *data, u16 h_proto);

struct xmit_frame *rtw_alloc_xmitframe(struct xmit_priv *pxmitpriv);
struct xmit_frame *rtw_alloc_xmitframe_ext(struct xmit_priv *pxmitpriv);
struct xmit_frame *rtw_alloc_xmitframe_once(struct xmit_priv *pxmitpriv);
int rtw_free_xmitframe(struct xmit_priv *pxmitpriv, struct xmit_frame *pxmitframe);
void rtw_free_xmitframe_queue(struct xmit_priv *pxmitpriv, struct __queue *pframequeue);
struct tx_servq *rtw_get_sta_pending(struct adapter *adapt, struct sta_info *psta, int up, u8 *ac);
int rtw_xmitframe_enqueue(struct adapter *adapt, struct xmit_frame *pxmitframe);
struct xmit_frame *rtw_dequeue_xframe(struct xmit_priv *pxmitpriv, struct hw_xmit *phwxmit_i, int entry);

int rtw_xmit_classifier(struct adapter *adapt, struct xmit_frame *pxmitframe);
u32 rtw_calculate_wlan_pkt_size_by_attribue(struct pkt_attrib *pattrib);
#define rtw_wlan_pkt_size(f) rtw_calculate_wlan_pkt_size_by_attribue(&f->attrib)
int rtw_xmitframe_coalesce(struct adapter *adapt, struct sk_buff *pkt, struct xmit_frame *pxmitframe);
#if defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH)
int rtw_mgmt_xmitframe_coalesce(struct adapter *adapt, struct sk_buff *pkt, struct xmit_frame *pxmitframe);
#endif
int _rtw_init_hw_txqueue(struct hw_txqueue *phw_txqueue, u8 ac_tag);
void _rtw_init_sta_xmit_priv(struct sta_xmit_priv *psta_xmitpriv);

int rtw_txframes_pending(struct adapter *adapt);
int rtw_txframes_sta_ac_pending(struct adapter *adapt, struct pkt_attrib *pattrib);
void rtw_init_hwxmits(struct hw_xmit *phwxmit, int entry);


int _rtw_init_xmit_priv(struct xmit_priv *pxmitpriv, struct adapter *adapt);
void _rtw_free_xmit_priv(struct xmit_priv *pxmitpriv);


void rtw_alloc_hwxmits(struct adapter *adapt);
void rtw_free_hwxmits(struct adapter *adapt);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
int rtw_monitor_xmit_entry(struct sk_buff *skb, struct net_device *ndev);
#endif
int rtw_xmit(struct adapter *adapt, struct sk_buff **pkt);
bool xmitframe_hiq_filter(struct xmit_frame *xmitframe);
int xmitframe_enqueue_for_sleeping_sta(struct adapter *adapt, struct xmit_frame *pxmitframe);
void stop_sta_xmit(struct adapter *adapt, struct sta_info *psta);
void wakeup_sta_to_xmit(struct adapter *adapt, struct sta_info *psta);
void xmit_delivery_enabled_frames(struct adapter *adapt, struct sta_info *psta);

u8 rtw_get_tx_bw_mode(struct adapter *adapter, struct sta_info *sta);

void rtw_get_adapter_tx_rate_bmp_by_bw(struct adapter *adapter, u8 bw, u16 *r_bmp_cck_ofdm, u32 *r_bmp_ht, u32 *r_bmp_vht);
void rtw_update_tx_rate_bmp(struct dvobj_priv *dvobj);
u16 rtw_get_tx_rate_bmp_cck_ofdm(struct dvobj_priv *dvobj);
u32 rtw_get_tx_rate_bmp_ht_by_bw(struct dvobj_priv *dvobj, u8 bw);
u32 rtw_get_tx_rate_bmp_vht_by_bw(struct dvobj_priv *dvobj, u8 bw);
u8 rtw_get_tx_bw_bmp_of_ht_rate(struct dvobj_priv *dvobj, u8 rate, u8 max_bw);
u8 rtw_get_tx_bw_bmp_of_vht_rate(struct dvobj_priv *dvobj, u8 rate, u8 max_bw);

u8 query_ra_short_GI(struct sta_info *psta, u8 bw);

u8	qos_acm(u8 acm_mask, u8 priority);

#ifdef CONFIG_TX_AMSDU
void rtw_amsdu_vo_timeout_handler(void *FunctionContext);
void rtw_amsdu_vi_timeout_handler(void *FunctionContext);
void rtw_amsdu_be_timeout_handler(void *FunctionContext);
void rtw_amsdu_bk_timeout_handler(void *FunctionContext);

u8 rtw_amsdu_get_timer_status(struct adapter *adapt, u8 priority);
void rtw_amsdu_set_timer_status(struct adapter *adapt, u8 priority, u8 status);
void rtw_amsdu_set_timer(struct adapter *adapt, u8 priority);
void rtw_amsdu_cancel_timer(struct adapter *adapt, u8 priority);

int rtw_xmitframe_coalesce_amsdu(struct adapter *adapt, struct xmit_frame *pxmitframe, struct xmit_frame *pxmitframe_queue);	
int check_amsdu(struct xmit_frame *pxmitframe);
int check_amsdu_tx_support(struct adapter *adapt);
struct xmit_frame *rtw_get_xframe(struct xmit_priv *pxmitpriv, int *num_frame);
#endif

u32	rtw_get_ff_hwaddr(struct xmit_frame	*pxmitframe);
int rtw_ack_tx_wait(struct xmit_priv *pxmitpriv, u32 timeout_ms);
void rtw_ack_tx_done(struct xmit_priv *pxmitpriv, int status);

enum XMIT_BLOCK_REASON {
	XMIT_BLOCK_NONE = 0,
	XMIT_BLOCK_REDLMEM = BIT0, /*LPS-PG*/
	XMIT_BLOCK_SUSPEND = BIT1, /*WOW*/
	XMIT_BLOCK_MAX = 0xFF,
};
void rtw_init_xmit_block(struct adapter *adapt);
void rtw_deinit_xmit_block(struct adapter *adapt);

void rtw_set_xmit_block(struct adapter *adapt, enum XMIT_BLOCK_REASON reason);
void rtw_clr_xmit_block(struct adapter *adapt, enum XMIT_BLOCK_REASON reason);
bool rtw_is_xmit_blocked(struct adapter *adapt);

/* include after declaring struct xmit_buf, in order to avoid warning */
#include <xmit_osdep.h>

#endif /* _RTL871X_XMIT_H_ */
