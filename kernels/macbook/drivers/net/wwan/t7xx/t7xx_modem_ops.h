/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_MODEM_OPS_H__
#define __T7XX_MODEM_OPS_H__

#include <linux/bits.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "t7xx_pci.h"

#define RT_ID_MD_PORT_ENUM 0
#define RT_ID_SAP_PORT_ENUM 1
#define FEATURE_COUNT 64
/* Modem feature query identification number */
#define MD_FEATURE_QUERY_ID	0x49434343

#define FEATURE_VER GENMASK(7, 4)
#define FEATURE_MSK GENMASK(3, 0)

/**
 * enum hif_ex_stage -	HIF exception handshake stages with the HW
 * @HIF_EX_INIT:        disable and clear TXQ
 * @HIF_EX_INIT_DONE:   polling for init to be done
 * @HIF_EX_CLEARQ_DONE: disable RX, flush TX/RX workqueues and clear RX
 * @HIF_EX_ALLQ_RESET:  HW is back in safe mode for reinitialization and restart
 */
enum hif_ex_stage {
	HIF_EX_INIT = 0,
	HIF_EX_INIT_DONE,
	HIF_EX_CLEARQ_DONE,
	HIF_EX_ALLQ_RESET,
};

struct mtk_runtime_feature {
	u8 feature_id;
	u8 support_info;
	u8 reserved[2];
	u32 data_len;
	u8 data[];
};

enum md_event_id {
	FSM_PRE_START,
	FSM_START,
	FSM_READY,
};

struct core_sys_info {
	atomic_t ready;
	atomic_t handshake_ongoing;
	u8 feature_set[FEATURE_COUNT];
	struct t7xx_port *ctl_port;
};

struct mtk_modem {
	struct md_sys_info *md_info;
	struct mtk_pci_dev *mtk_dev;
	struct core_sys_info core_md;
	struct core_sys_info core_sap;
	bool md_init_finish;
	atomic_t rgu_irq_asserted;
	struct workqueue_struct *handshake_wq;
	struct work_struct handshake_work;
	struct workqueue_struct *sap_handshake_wq;
	struct work_struct sap_handshake_work;
};

struct md_sys_info {
	unsigned int	exp_id;
	spinlock_t	exp_spinlock; /* protect exp_id containing EX events */
};

void mtk_md_exception_handshake(struct mtk_modem *md);
void mtk_md_event_notify(struct mtk_modem *md, enum md_event_id evt_id);
void mtk_md_reset(struct mtk_pci_dev *mtk_dev);
int mtk_md_init(struct mtk_pci_dev *mtk_dev);
void mtk_md_exit(struct mtk_pci_dev *mtk_dev);
void mtk_clear_rgu_irq(struct mtk_pci_dev *mtk_dev);
int mtk_acpi_fldr_func(struct mtk_pci_dev *mtk_dev);
int mtk_pci_mhccif_isr(struct mtk_pci_dev *mtk_dev);

#endif	/* __T7XX_MODEM_OPS_H__ */
