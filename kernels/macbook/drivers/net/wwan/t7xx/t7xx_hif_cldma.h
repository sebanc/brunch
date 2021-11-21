/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __MODEM_CD_H__
#define __MODEM_CD_H__

#include "t7xx_dev_node.h"
#include "t7xx_pci.h"
#include "t7xx_modem_ops.h"
#include "t7xx_ctrl_dma.h"

#define MAX_BD_NUM (MAX_SKB_FRAGS + 1)

#define CLDMA_NUM 2

enum cldma_id {
	ID_CLDMA0,
	ID_CLDMA1
};

enum queue_type {
	SHARED_Q = 0,
	DEDICATED_Q,
};

struct cldma_request {
	void *gpd;		/* virtual address for CPU */
	dma_addr_t gpd_addr;	/* physical address for DMA */
	struct sk_buff *skb;
	dma_addr_t mapped_buff;
	struct list_head entry;
	struct list_head bd;
	/* inherit from skb */
	/* bit7: override or not; bit0: IOC setting */
	unsigned char ioc_override;
} __packed;

enum cldma_ring_type {
	RING_GPD = 0,
	RING_GPD_BD = 1,
	RING_SPD = 2,
};

struct md_cd_queue;

/* cldma_ring is quite light, most of ring buffer operations require queue struct. */
struct cldma_ring {
	struct list_head gpd_ring;	/* ring of struct cldma_request */
	int length;		/* number of struct cldma_request */
	int pkt_size;		/* size of each packet in ring */
	enum cldma_ring_type type;

	int (*handle_tx_request)(struct md_cd_queue *queue,
				 struct cldma_request *req, struct sk_buff *skb,
		unsigned int ioc_override);
	int (*handle_rx_done)(struct md_cd_queue *queue,
			      int budget, int blocking);
	int (*handle_tx_done)(struct md_cd_queue *queue,
			      int budget, int blocking);
};

struct md_cd_queue {
	unsigned char index;
	struct ccci_modem *md;
	struct cldma_ring *tr_ring;
	struct cldma_request *tr_done;
	int budget;		/* same as ring buffer size by default */
	struct cldma_request *rx_refill;	/* only for Rx */
	struct cldma_request *tx_xmit;	/* only for Tx */
	enum queue_type q_type;
	wait_queue_head_t req_wq;	/* only for Tx */
	spinlock_t ring_lock;

	struct workqueue_struct *worker;
	struct work_struct cldma_rx_work;
	struct delayed_work cldma_tx_work;

	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;

	enum cldma_id hif_id;
	enum direction dir;
	unsigned int busy_count;
};

struct md_cd_ctrl {
	enum cldma_id hif_id;
	struct device *dev;
	struct mtk_pci_dev *mtk_dev;
	struct md_cd_queue txq[CLDMA_TXQ_NUM];
	struct md_cd_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
	unsigned short txq_started;
	atomic_t cldma_irq_enabled;
	spinlock_t cldma_timeout_lock; /* protects CLDMA, not only for timeout checking */
	atomic_t wakeup_src;
	unsigned int tx_busy_warn_cnt;
	/* assuming T/R GPD/BD/SPD have the same size */
	struct dma_pool *gpd_dmapool;
	struct cldma_ring normal_tx_ring[CLDMA_TXQ_NUM];
	struct cldma_ring normal_rx_ring[CLDMA_RXQ_NUM];
	struct md_pm_entity *pm_entity;
	struct cldma_hw_info hw_info;
	unsigned char is_late_init;
};

#define GPD_FLAGS_HWO	BIT(0)
#define GPD_FLAGS_BDP	BIT(1)
#define GPD_FLAGS_BPS	BIT(2)
#define GPD_FLAGS_IOC	BIT(7)

struct cldma_tgpd {
	u8 gpd_flags;
	/* original checksum bits, used for debug. 1: Tx in, 2: Tx done */
	u16 debug_tx;
	u8 debug_id;
	u32 next_gpd_ptr_h;
	u32 next_gpd_ptr_l;
	u32 data_buff_bd_ptr_h;
	u32 data_buff_bd_ptr_l;
	u16 data_buff_len;
	u16 non_used1;
} __packed;

struct cldma_rgpd {
	u8 gpd_flags;
	u8 non_used; /* original checksum bits */
	u16 data_allow_len;
	u32 next_gpd_ptr_h;
	u32 next_gpd_ptr_l;
	u32 data_buff_bd_ptr_h;
	u32 data_buff_bd_ptr_l;
	u16 data_buff_len;
	u8 non_used1;
	u8 debug_id;
} __packed;

struct cldma_tbd {
	u8 bd_flags;
	u8 non_used; /* original checksum bits */
	u16 non_used2;
	u32 next_bd_ptr_h;
	u32 next_bd_ptr_l;
	u32 data_buff_ptr_h;
	u32 data_buff_ptr_l;
	u16 data_buff_len;
	u16 non_used1;
} __packed;

int ccci_cldma_alloc(struct mtk_pci_dev *mtk_dev);
void ccci_cldma_hif_hw_init(enum cldma_id hif_id);
int ccci_cldma_init(enum cldma_id hif_id);
void ccci_cldma_exception(enum cldma_id hif_id, enum hif_ex_stage stage);
void ccci_cldma_exit(enum cldma_id hif_id);
void ccci_cldma_switch_cfg(enum cldma_id hif_id);
int ccci_cldma_write_room(enum cldma_id hif_id, unsigned char qno);
int ccci_cldma_txq_mtu(unsigned char qno);
int ccci_cldma_start(enum cldma_id hif_id);
int ccci_cldma_stop(enum cldma_id hif_id);
void ccci_cldma_reset(void);
int ccci_cldma_send_skb(enum cldma_id hif_id, int qno, struct sk_buff *skb,
			int skb_from_pool, int blocking);

#endif /* __MODEM_CD_H__ */
