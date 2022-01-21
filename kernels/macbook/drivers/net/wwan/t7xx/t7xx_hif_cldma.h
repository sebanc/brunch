/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_HIF_CLDMA_H__
#define __T7XX_HIF_CLDMA_H__

#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/types.h>

#include "t7xx_cldma.h"
#include "t7xx_common.h"
#include "t7xx_modem_ops.h"
#include "t7xx_pci.h"

#define CLDMA_NUM 2

enum cldma_id {
	ID_CLDMA0,
	ID_CLDMA1,
};

enum cldma_queue_type {
	CLDMA_SHARED_Q,
	CLDMA_DEDICATED_Q,
};

typedef enum {
        HIF_CFG_DEF = 0,
        HIF_CFG1,
        HIF_CFG2,
        HIF_CFG_MAX,
} HIF_CFG_ENUM;

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
};

struct cldma_queue;

/* cldma_ring is quite light, most of ring buffer operations require queue struct. */
struct cldma_ring {
	struct list_head gpd_ring;	/* ring of struct cldma_request */
	int length;			/* number of struct cldma_request */
	int pkt_size;			/* size of each packet in ring */

	int (*handle_tx_request)(struct cldma_queue *queue, struct cldma_request *req,
				 struct sk_buff *skb, unsigned int ioc_override);
	int (*handle_rx_done)(struct cldma_queue *queue, int budget);
	int (*handle_tx_done)(struct cldma_queue *queue);
};

struct cldma_queue {
	unsigned char index;
	struct mtk_modem *md;
	struct cldma_ring *tr_ring;
	struct cldma_request *tr_done;
	int budget;			/* same as ring buffer size by default */
	struct cldma_request *rx_refill;
	struct cldma_request *tx_xmit;
	enum cldma_queue_type q_type;
	wait_queue_head_t req_wq;	/* only for Tx */
	spinlock_t ring_lock;

	struct workqueue_struct *worker;
	struct work_struct cldma_rx_work;
	struct work_struct cldma_tx_work;

	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;

	enum cldma_id hif_id;
	enum direction dir;
};

struct cldma_ctrl {
	enum cldma_id hif_id;
	struct device *dev;
	struct mtk_pci_dev *mtk_dev;
	struct cldma_queue txq[CLDMA_TXQ_NUM];
	struct cldma_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
	unsigned short txq_started;
	spinlock_t cldma_lock; /* protects CLDMA structure */
	/* assuming T/R GPD/BD/SPD have the same size */
	struct dma_pool *gpd_dmapool;
	struct cldma_ring tx_ring[CLDMA_TXQ_NUM];
	struct cldma_ring rx_ring[CLDMA_RXQ_NUM];
	struct md_pm_entity *pm_entity;
	struct cldma_hw_info hw_info;
	bool is_late_init;
	int (*recv_skb)(struct cldma_queue *queue, struct sk_buff *skb);
};

#define GPD_FLAGS_HWO	BIT(0)
#define GPD_FLAGS_BDP	BIT(1)
#define GPD_FLAGS_BPS	BIT(2)
#define GPD_FLAGS_IOC	BIT(7)

struct cldma_tgpd {
	u8 gpd_flags;
	u16 non_used;
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
	u8 non_used;
	u16 data_allow_len;
	u32 next_gpd_ptr_h;
	u32 next_gpd_ptr_l;
	u32 data_buff_bd_ptr_h;
	u32 data_buff_bd_ptr_l;
	u16 data_buff_len;
	u8 non_used1;
	u8 debug_id;
} __packed;

int cldma_alloc(enum cldma_id hif_id, struct mtk_pci_dev *mtk_dev);
void cldma_hif_hw_init(enum cldma_id hif_id);
int cldma_init(enum cldma_id hif_id);
void cldma_exception(enum cldma_id hif_id, enum hif_ex_stage stage);
void cldma_exit(enum cldma_id hif_id);
void cldma_switch_cfg(enum cldma_id hif_id, unsigned int cfg_id);
int cldma_write_room(enum cldma_id hif_id, unsigned char qno);
void cldma_start(enum cldma_id hif_id);
int cldma_stop(enum cldma_id hif_id);
void cldma_reset(enum cldma_id hif_id);
void cldma_set_recv_skb(enum cldma_id hif_id,
			int (*recv_skb)(struct cldma_queue *queue, struct sk_buff *skb));
int cldma_send_skb(enum cldma_id hif_id, int qno, struct sk_buff *skb,
		   bool skb_from_pool, bool blocking);
int cldma_txq_mtu(unsigned char qno);

#endif /* __T7XX_HIF_CLDMA_H__ */
