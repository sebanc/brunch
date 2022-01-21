/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_NETDEV_H__
#define __T7XX_NETDEV_H__

#include <linux/bitfield.h>
#include <linux/netdevice.h>

#include "t7xx_common.h"
#include "t7xx_hif_dpmaif.h"
#include "t7xx_monitor.h"

#define RXQ_NUM			DPMAIF_RXQ_NUM
#define NIC_DEV_MAX		21
#define NIC_DEV_DEFAULT		2
#define NIC_CAP_TXBUSY_STOP	BIT(0)
#define NIC_CAP_SGIO		BIT(1)
#define NIC_CAP_DATA_ACK_DVD	BIT(2)
#define NIC_CAP_CCMNI_MQ	BIT(3)

/* must be less than DPMAIF_HW_MTU_SIZE (3*1024 + 8) */
#define CCMNI_MTU_MAX		3000
#define CCMNI_TX_QUEUE		1000
#define CCMNI_NETDEV_WDT_TO	(1 * HZ)

#define IPV4_VERSION		0x40
#define IPV6_VERSION		0x60

struct ccmni_instance {
	unsigned int		index;
	atomic_t		usage;
	struct net_device	*dev;
	struct ccmni_ctl_block	*ctlb;
	unsigned long		tx_busy_cnt[TXQ_TYPE_CNT];
};

struct ccmni_ctl_block {
	struct mtk_pci_dev	*mtk_dev;
	struct dpmaif_ctrl	*hif_ctrl;
	struct ccmni_instance	*ccmni_inst[NIC_DEV_MAX];
	struct dpmaif_callbacks	callbacks;
	unsigned int		nic_dev_num;
	unsigned int		md_sta;
	unsigned int		capability;

	struct fsm_notifier_block md_status_notify;
};

int ccmni_init(struct mtk_pci_dev *mtk_dev);
void ccmni_exit(struct mtk_pci_dev *mtk_dev);

#endif /* __T7XX_NETDEV_H__ */
