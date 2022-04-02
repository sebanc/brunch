/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __MTK_PCI_RESCAN_H__
#define __MTK_PCI_RESCAN_H__

#define MTK_RESCAN_WQ "mtk_rescan_wq"

#define DELAY_RESCAN_MTIME 1000

struct remove_rescan_context {
	struct work_struct service_task;
	struct workqueue_struct *pcie_rescan_wq;
	spinlock_t dev_lock; /* protects device */
	struct pci_dev *dev;
	int rescan_done;
};

void mtk_pci_dev_rescan(void);
void mtk_queue_rescan_work(struct pci_dev *pdev);
int mtk_rescan_init(void);
void mtk_rescan_deinit(void);
void mtk_rescan_done(void);

#endif	/* __MTK_PCI_RESCAN_H__ */
