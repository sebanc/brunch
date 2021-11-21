/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __T7XX_NETDEV_H__
#define __T7XX_NETDEV_H__

#include "t7xx_hif_dpmaif.h"
#include "t7xx_monitor.h"

#define RXQ_NUM			MAIFQ_MAX_DLQ_NUM
#define NIC_DEV_MAX		21	/* current for host driver */
#define NIC_DEV_DEFAULT		2
#define NIC_NAPI_POLL_BUDGET	64

/* should less than DPMAIF_HW_MTU_SIZE (3*1024 + 8) */
#define CCMNI_MTU_MAX		3000
#define CCMNI_MTU		1500

#define CCMNI_TX_QUEUE		1000
#define CCMNI_NETDEV_WDT_TO	(1 * HZ)

#define IPV4_VERSION		0x40
#define IPV6_VERSION		0x60

#define CCMNI_TX_PRINT_F	BIT(0)

/* This should be aligned with Data Path HW queues' configuration */
enum {
	HIF_MIN_TXQ = 0,
	HIF_TXQ0 = HIF_MIN_TXQ,
	HIF_NORMAL_QUEUE = HIF_TXQ0,
	HIF_TXQ1 = 1,
	HIF_ACK_QUEUE = HIF_TXQ1,
	HIF_TXQ2 = 2,
	HIF_TXQ3 = 3,
	HIF_TXQ4 = 4,
	HIF_MAX_TXQ = HIF_TXQ4,
};

/* HIF_TXQ0, HIF_TXQ1 */
#define HIF_TXQ_IN_USE	2

/* netdev TX queue */
enum {
	CCMNI_TXQ_NORMAL = 0,
	CCMNI_TXQ_FAST = 1,
	CCMNI_TXQ_NUM,
	CCMNI_TXQ_END = CCMNI_TXQ_NUM
};

struct ccmni_instance {
	unsigned int		index;

	atomic_t		usage;

	/* use pointer to keep these items unique,
	 * while switching between CCMNI instances
	 */
	struct net_device	*dev;
	unsigned int		rx_seq_num;
	unsigned int		tx_seq_num[2];
	unsigned int		flags[2];
	struct ccmni_ctl_block	*ctlb;
	unsigned long		tx_busy_cnt[2];
	unsigned long		tx_full_tick[2];
	unsigned long		tx_irq_tick[2];
	unsigned int		tx_full_cnt[2];
	unsigned int		tx_irq_cnt[2];
	unsigned int		rx_gro_cnt;
};

struct ccmni_ctl_block {
	struct mtk_pci_dev	*mtk_dev;
	struct md_dpmaif_ctrl	*hif_ctrl;
	struct ccmni_instance	*ccmni_inst[NIC_DEV_MAX];
	unsigned int		nic_dev_num;
	bool			nic_dev_created;
	unsigned int		md_sta;
	unsigned int		capability;

	struct net_device	dummy_dev;
	unsigned int		napi_poll_weigh;
	int (*napi_poll)(struct napi_struct *napi, int weight);
	struct napi_struct	*napi[RXQ_NUM];
	atomic_t		napi_usr_refcnt;
	atomic_t		napi_enable;

	struct fsm_notifier_block md_status_notify;
};

#define NIC_CAP_TXBUSY_STOP	BIT(0)
#define NIC_CAP_SGIO		BIT(1)
#define NIC_CAP_DATA_ACK_DVD	BIT(2)
#define NIC_CAP_CCMNI_MQ	BIT(3)
#define NIC_CAP_NAPI		BIT(4)

int ccmni_init(struct mtk_pci_dev *mtk_dev);
void ccmni_exit(struct mtk_pci_dev *mtk_dev);
int ccmni_rx_callback(struct ccmni_ctl_block *ctlb, int ccmni_idx,
		      struct sk_buff *skb, struct napi_struct *napi);
void ccmni_queue_state_notify(struct ccmni_ctl_block *ctlb,
			      enum HIF_STATE state, int qno);
unsigned int ccmni_get_nic_cap(struct ccmni_ctl_block *ctlb);

#endif /* __T7XX_NETDEV_H__ */
