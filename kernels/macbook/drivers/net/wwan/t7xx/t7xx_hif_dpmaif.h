/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __MODEM_DPMA_H__
#define __MODEM_DPMA_H__

#include <linux/netdevice.h>

#include "t7xx_buffer_manager.h"
#include "t7xx_hif_dpmaif.h"
#include "t7xx_pci.h"

/***********************************************************************
 *  DPMAIF HW feature
 *
 ***********************************************************************/
#define DPMA_CHECK_REG_TIMEOUT	1000000
#define DPMA_MASK_FAIL_CNT_MAX	100

#define DPMAIF_UL_DRB_ENTRY_SIZE  6144  // from 512
#define DPMAIF_DL_BAT_ENTRY_SIZE  8192  // from 512
#define DPMAIF_DL_FRG_ENTRY_SIZE  4814  // from 512
#define DPMAIF_DL_PIT_ENTRY_SIZE  (DPMAIF_DL_BAT_ENTRY_SIZE * 2) // from 512

#define DPMAIF_DL_PIT_BYTE_SIZE   16

#define DPMAIF_DL_BAT_BYTE_SIZE   8
#define DPMAIF_DL_FRG_BYTE_SIZE   8
#define DPMAIF_UL_DRB_BYTE_SIZE   16

#define DPMAIF_UL_DRB_ENTRY_WORD  (DPMAIF_UL_DRB_BYTE_SIZE >> 2)
#define DPMAIF_DL_PIT_ENTRY_WORD  (DPMAIF_DL_PIT_BYTE_SIZE >> 2)
#define DPMAIF_DL_BAT_ENTRY_WORD  (DPMAIF_DL_BAT_BYTE_SIZE >> 2)

#define DPMAIF_DL_PIT_SEQ_VALUE	251
#define DPMF_PIT_SEQ_MAX           DPMAIF_DL_PIT_SEQ_VALUE

/* Default DPMAIF DL common setting */
#define DPMAIF_HW_BAT_REMAIN       64

#define DPMAIF_HW_BAT_PKTBUF       (128 * 28)

#define DPMAIF_HW_FRG_PKTBUF       128
#define DPMAIF_HW_BAT_RSVLEN       64
#define DPMAIF_HW_PKT_BIDCNT       1
#define DPMAIF_HW_PKT_ALIGN        64

#define DPMAIF_HW_MTU_SIZE          (3 * 1024 + 8)

#define DPMAIF_HW_CHK_BAT_NUM      62
#define DPMAIF_HW_CHK_FRG_NUM      3
#define DPMAIF_HW_CHK_PIT_NUM      (2 * DPMAIF_HW_CHK_BAT_NUM)

#define DPMAIF_DLQ0_NUM            0
#define DPMAIF_DLQ1_NUM            1
#define DPMAIF_DLQPIT_SEPARATE_NUM 2

#define MAIFQ_MAX_DLQ_NUM          DPMAIF_DLQPIT_SEPARATE_NUM
#define MAIFQ_MAX_ULQ_NUM          5

/***********************************************************************
 *  DPMAIF common structures
 *
 ***********************************************************************/
struct dpmaif_isr_en_mask {
	unsigned int    ap_ul_l2intr_en_msk;
	unsigned int    ap_dl_l2intr_en_msk;
	unsigned int    ap_udl_ip_busy_en_msk;
	unsigned int    ap_dl_l2intr_err_en_msk;
};

struct dpmaif_ul_st {
	bool		que_started;
	unsigned char	reserve[3];
	unsigned long	drb_base;
	unsigned int	drb_size_cnt;
};

struct dpmaif_dl_st {
	bool		que_started;
	unsigned char	reserve[3];
	unsigned long	pit_base;
	unsigned int	pit_size_cnt;
	unsigned long	bat_base;
	unsigned int	bat_size_cnt;
	unsigned long	frg_base;
	unsigned int	frg_size_cnt;
	unsigned int	pit_seq;
};

struct dpmaif_dl_hwq {
	unsigned int    bat_remain_size;
	unsigned int    bat_pkt_bufsz;
	unsigned int    frg_pkt_bufsz;
	unsigned int    bat_rsv_length;
	unsigned int    pkt_bid_max_cnt;
	unsigned int    pkt_alignment;
	unsigned int    mtu_size;
	unsigned int    chk_pit_num;
	unsigned int    chk_bat_num;
	unsigned int    chk_frg_num;
};

struct dpmaif_hw_info {
	void __iomem			*pcie_base;
	struct dpmaif_dl_st		dl_que[MAIFQ_MAX_DLQ_NUM];
	struct dpmaif_ul_st		ul_que[MAIFQ_MAX_ULQ_NUM];
	struct dpmaif_dl_hwq		dl_que_hw[MAIFQ_MAX_DLQ_NUM];
	struct dpmaif_isr_en_mask	isr_en_mask;
	unsigned short			dlq_mask_fail_cnt[MAIFQ_MAX_DLQ_NUM];
};

enum bat_type {
	NORMAL_BUF = 0,
	FRAG_BUF = 1,
};

#define DPMAIF_RXQ_NUM			MAIFQ_MAX_DLQ_NUM
#define DPMAIF_TXQ_NUM			MAIFQ_MAX_ULQ_NUM

#define DPMAIF_BUF_PKT_SIZE		NET_RX_BUF
#define DPMAIF_BUF_FRAG_SIZE		DPMAIF_HW_FRG_PKTBUF

#define DPMF_BAT_CNT_UPDATE_THRESHOLD	30
#define DPMF_PIT_CNT_UPDATE_THRESHOLD	60

#define DPMF_RX_PUSH_THRESHOLD_MASK	0x7
#define DPMF_NOTIFY_RELEASE_COUNT	128

#define DPMF_POLL_PIT_TIME		20 /* us */
#define DPMF_POLL_RX_TIME		10 /* us */
#define DPMF_POLL_PIT_CNT_MAX		(2000 / DPMF_POLL_PIT_TIME) /* poll up to 2ms */
#define DPMAIF_PIT_SEQ_CHECK_FAIL_CNT	2500

#define DPMF_TX_REL_TIMEOUT		0 /* ms */
#define DPMF_SKB_TX_BURST_CNT		5

#define DPMF_PCIE_LOCK_RES_TRY_WAIT_CNT	20
#define DPMF_WORKQUEUE_TIME_LIMIT	2 /* ms */

/* DPMAIF HW checksum check result */
#define DPMF_CS_RESULT_PASS		0

/****************************************************************************
 * Structure of DL PIT
 ****************************************************************************/
struct dpmaifq_normal_pit {
	unsigned int	packet_type:1;
	unsigned int	c_bit:1;
	unsigned int	buffer_type:1;
	unsigned int	buffer_id:13;
	unsigned int	data_len:16;
	unsigned int	p_data_addr;
	unsigned int	data_addr_ext;
	unsigned int	pit_seq:8;
	unsigned int	h_bid:3;
	unsigned int	reserved1:5;
	unsigned int	ig:1;
	unsigned int	bi_f:2;
	unsigned int	header_offset:5;
	unsigned int	ulq_done:6;
	unsigned int	dlq_done:2;
};

/* packet_type */
#define DES_PT_PD	0x00
#define DES_PT_MSG	0x01
/* buffer_type */
#define PKT_BUF_FRAG	(0x01)

struct dpmaifq_msg_pit {
	unsigned int	packet_type:1;
	unsigned int	c_bit:1;
	unsigned int	check_sum:2;
	unsigned int	error_bit:1;
	unsigned int	src_qid:3;
	unsigned int	hpc_idx:4;
	unsigned int	reserved:4;
	unsigned int	channel_id:8;
	unsigned int	network_type:3;
	unsigned int	reserved2:4;
	unsigned int	dp:1;

	unsigned int	count_l:16;
	unsigned int	flow:5;
	unsigned int	reserved3:3;
	unsigned int	cmd:3;
	unsigned int	hp_idx:5;

	unsigned int	reserved4:3;
	unsigned int	vbid:13;
	unsigned int	pro:2;
	unsigned int	reserved5:6;
	unsigned int	hash:8;

	unsigned int	pit_seq:8;
	unsigned int	h_bid:3;
	unsigned int	reserved6:5;
	unsigned int	ig:1;
	unsigned int	reserved7:3;
	unsigned int	mr:2;
	unsigned int	reserved8:1;
	unsigned int	ip:1;
	unsigned int	ulq_done:6;
	unsigned int	dlq_done:2;
};

/****************************************************************************
 * Structure of DL BAT
 ****************************************************************************/
struct dpmaif_cur_rx_skb_info {
	bool msg_pit_received;
	struct sk_buff *cur_skb;
	unsigned int cur_chn_idx;
	unsigned int check_sum;
	unsigned int pit_dp;
	int err_payload;
};

struct dpmaif_bat {
	unsigned int	p_buffer_addr;
	unsigned int	buffer_addr_ext;
};

struct dpmaif_bat_skb {
	struct sk_buff *skb;
	dma_addr_t data_bus_addr;
	unsigned int data_len;
};

struct dpmaif_bat_page {
	struct page *page;
	dma_addr_t data_bus_addr;
	unsigned int offset;
	unsigned int data_len;
};

#define SKB_RX_LIST_MAX_LEN 0xFFFFFFFF

struct dpmaif_bat_request {
	void *bat_base;
	dma_addr_t bat_bus_addr;
	unsigned int	bat_size_cnt;
	unsigned short	  bat_wr_idx;
	unsigned short	  bat_rel_rd_idx;
	void *bat_skb_ptr;	/* collect skb linked to bat */
	unsigned int	 skb_pkt_cnt;
	unsigned int pkt_buf_sz;
	unsigned char *bat_mask;

	atomic_t refcnt;
};

struct dpmaif_rx_queue {
	unsigned char index;
	bool que_started;
	unsigned short budget;

	void *pit_base;
	dma_addr_t pit_bus_addr;
	unsigned int	pit_size_cnt;

	unsigned short	  pit_rd_idx;
	unsigned short	  pit_wr_idx;
	unsigned short	  pit_rel_rd_idx;

	struct dpmaif_bat_request *bat_req; /* for NORMAL_BAT */
	struct dpmaif_bat_request *bat_frag; /* for FRG_BAT feature */

	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;
	struct ccci_skb_queue skb_list;

	/* none napi mode */
	struct tasklet_struct dpmaif_rxq_task;
	struct workqueue_struct *worker;
	struct work_struct dpmaif_rxq_work;

	atomic_t rx_processing;

	struct napi_struct napi;
	int check_bid_fail_cnt;

	struct md_dpmaif_ctrl *dpmaif_ctrl;
	unsigned int expect_pit_seq;
	unsigned int pit_remain_rel_cnt;
	int cur_irq_cpu;	/* CPU that isr is running on */
	int cur_rx_thrd_cpu;	/* CPU that rx push thread is running on */

	unsigned int pit_seq_fail_cnt;

	struct dpmaif_cur_rx_skb_info rx_data_info;
};

/****************************************************************************
 * Structure of UL DRB
 ****************************************************************************
 */
/* UL unit: WORD == 4bytes, && 1drb == 8bytes,
 * so 1 drb = size_cnt * 2, rd_idx_sw = rd_idx_hw/2.
 */
struct dpmaif_drb_pd {
	unsigned int	dtyp:2;
	unsigned int	c_bit:1;
	unsigned int	reserved:13;
	unsigned int	data_len:16;
	unsigned int	p_data_addr;
	unsigned int	data_addr_ext;
	unsigned int	reserved2;
};

/* drb->dtype */
#define DES_DTYP_PD	0x00
#define DES_DTYP_MSG	0x01

struct dpmaif_drb_msg {
	unsigned int	dtyp:2;
	unsigned int	c_bit:1;
	unsigned int	reserved:13;
	unsigned int	packet_len:16;
	unsigned int	count_l:16;
	unsigned int	channel_id:8;
	unsigned int	network_type:3;
	unsigned int	reserved2:1;
	unsigned int	ip_chk:1;
	unsigned int	l4_chk:1;
	unsigned int	reserved3:2;
	unsigned int	reserved4;
	unsigned int	reserved5;
};

struct dpmaif_drb_skb {
	struct sk_buff *skb;

	dma_addr_t bus_addr;
	unsigned short data_len;

	/* for debug */
	unsigned short drb_idx:13;
	unsigned short is_msg:1;
	unsigned short is_frag:1;
	unsigned short is_last_one:1;
};

struct dpmaif_tx_queue {
	unsigned char		index;
	bool			que_started;
	atomic_t		tx_budget;
	void			*drb_base;
	dma_addr_t		drb_bus_addr;
	unsigned int		drb_size_cnt;
	unsigned short		drb_wr_idx;
	unsigned short		drb_rd_idx;
	unsigned short		drb_rel_rd_idx;
	unsigned short		last_ch_id;
	void			*drb_skb_base;
	wait_queue_head_t	req_wq;
	struct workqueue_struct	*worker;
	struct delayed_work	dpmaif_tx_work;
	spinlock_t		tx_lock; /* protects txq drb */
	atomic_t		tx_processing;

	struct md_dpmaif_ctrl	*dpmaif_ctrl;
	/* tx thread skb_list */
	spinlock_t		tx_event_lock;
	struct list_head	tx_event_queue;
	unsigned int		tx_submit_skb_cnt;
	unsigned int		tx_list_max_len;
	unsigned int		tx_skb_stat;
	bool			drb_lack;
};

enum hifdpmaif_state {
	HIFDPMAIF_STATE_MIN	 = 0,
	HIFDPMAIF_STATE_PWROFF,
	HIFDPMAIF_STATE_PWRON,
	HIFDPMAIF_STATE_EXCEPTION,
	HIFDPMAIF_STATE_MAX,
};

#define DPMA_SKB_SIZE_OVER_HEAD	SKB_DATA_ALIGN(sizeof(struct skb_shared_info))
#define DPMA_SKB_SIZE_EXTRA\
	(SKB_DATA_ALIGN((NET_SKB_PAD) + (DPMA_SKB_SIZE_OVER_HEAD)))
#define DPMA_SKB_SIZE_TRUE(s)		((s) + (DPMA_SKB_SIZE_EXTRA))
#define DPMA_SKB_SIZE_MAX\
	(DPMAIF_HW_BAT_PKTBUF)	/* Should be less than PAGE_SIZE */
#define DPMA_SKB_QUEUE_CNT		1
#define DPMA_SKB_QUEUE_SIZE\
	((DPMAIF_DL_BAT_ENTRY_SIZE) * DPMA_SKB_SIZE_TRUE(DPMA_SKB_SIZE_MAX))
#define DPMA_SKB_TRUE_SIZE_MIN		32
#define DPMA_RELOAD_TH_1		4
#define DPMA_RELOAD_TH_2		5

/* data path skb pool */
struct dpmaif_map_skb {
	struct list_head head;
	u32 qlen;
	spinlock_t lock; /* protects skb queue*/
};

struct dpmaif_skb_info {
	struct list_head entry;
	struct sk_buff *skb;
	unsigned int data_len;
	unsigned long long data_bus_addr;
};

struct dpmaif_skb_queue {
	struct dpmaif_map_skb skb_list;
	unsigned int size;
	unsigned int max_len;
	unsigned long busy_cnt;
};

struct dpmaif_skb_pool {
	struct dpmaif_skb_queue queue[DPMA_SKB_QUEUE_CNT];
	struct workqueue_struct *pool_reload_work_queue;
	struct work_struct reload_work;
	unsigned int queue_cnt;
};

/* DPMAIF over PCIe interrupt structure */
struct dpmaif_isr_para {
	struct md_dpmaif_ctrl *dpmaif_ctrl;
	unsigned char pcie_int;
	unsigned char dlq_id;
};

struct dpmaif_tx_event {
	struct list_head entry;
	int qno;
	struct sk_buff *skb;
	int blocking;
	int drb_cnt;
};

struct md_dpmaif_ctrl {
	struct device *dev;
	struct mtk_pci_dev *mtk_dev;
	struct ccmni_ctl_block *ctlb;
	bool napi_enable;
	enum hifdpmaif_state dpmaif_state;
	struct dpmaif_tx_queue txq[DPMAIF_TXQ_NUM];
	struct dpmaif_rx_queue rxq[DPMAIF_RXQ_NUM];

	unsigned char rxq_int_mapping[DPMAIF_RXQ_NUM];
	struct dpmaif_isr_para isr_para[DPMAIF_RXQ_NUM];

	spinlock_t bat_release_lock;		/* protects bat_req->mask */
	struct dpmaif_bat_request bat_req;	/* for NORMAL_BAT feature */
	spinlock_t frag_bat_release_lock;	/* protects bat_frag->mask */
	struct dpmaif_bat_request bat_frag;	/* for FRG_BAT feature */

	struct workqueue_struct *bat_rel_work_queue;
	struct work_struct bat_rel_work;

	atomic_t dpmaif_irq_enabled;

	struct dpmaif_hw_info hif_hw_info;
	struct dpmaif_skb_pool skb_pool;

	bool dpmaif_sw_init_done;

	struct md_pm_entity dpmaif_pm_entity;

	/* TX thread */
	wait_queue_head_t tx_wq;
	struct task_struct *tx_thread;
	unsigned char txq_select_times;
};

int dpmaif_hif_init(struct ccmni_ctl_block *ctlb);
void dpmaif_hif_exit(struct ccmni_ctl_block *ctlb);

int dpmaif_md_state_callback(struct md_dpmaif_ctrl *dpmaif_ctrl, unsigned char state);

unsigned int ring_buf_get_next_wrdx(unsigned int buf_len,
				    unsigned int buf_idx);
unsigned int ring_buf_writeable(unsigned int total_cnt,
				unsigned int rel_idx,
				unsigned int wrt_idx);

#endif /* __MODEM_DPMA_H__ */
