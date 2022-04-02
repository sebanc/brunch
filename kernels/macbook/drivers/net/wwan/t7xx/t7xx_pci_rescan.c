// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":pcie:%s: " fmt, __func__
#define dev_fmt(fmt) "pcie: " fmt

#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "t7xx_pci.h"
#include "t7xx_pci_rescan.h"

static struct remove_rescan_context g_mtk_rescan_context;

/* reference to bus rescan function */
void mtk_pci_dev_rescan(void)
{
	struct pci_bus *b = NULL;

	pci_lock_rescan_remove();
	while ((b = pci_find_next_bus(b)) != NULL)
		pci_rescan_bus(b);

	pci_unlock_rescan_remove();
}

void mtk_rescan_done(void)
{
	unsigned long flags;

	spin_lock_irqsave(&g_mtk_rescan_context.dev_lock, flags);
	if (g_mtk_rescan_context.rescan_done == 0) {
		pr_info("this is a rescan probe\n");
		g_mtk_rescan_context.rescan_done = 1;
	} else {
		pr_info("this is a init probe\n");
	}
	spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
}

static void mtk_remove_rescan(struct work_struct *work)
{
	struct pci_dev *pdev;
	unsigned long flags;
	int retry;

	spin_lock_irqsave(&g_mtk_rescan_context.dev_lock, flags);
	g_mtk_rescan_context.rescan_done = 0;
	pdev = g_mtk_rescan_context.dev;
	spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
	if (pdev) {
		/* bus provided remove function */
		pci_stop_and_remove_bus_device_locked(pdev);
		pr_info("start mtk remove and rescan flow\n");
	}

	for (retry = 35; retry > 0; retry--) {
		msleep(DELAY_RESCAN_MTIME);
		mtk_pci_dev_rescan();
		spin_lock_irqsave(&g_mtk_rescan_context.dev_lock, flags);
		if (g_mtk_rescan_context.rescan_done == 1) {
			spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
	}
	pr_info("rescan try times left %d\n", retry);
}

void mtk_queue_rescan_work(struct pci_dev *pdev)
{
	unsigned long flags;

	dev_info(&pdev->dev, "start queue_mtk_rescan_work\n");
	spin_lock_irqsave(&g_mtk_rescan_context.dev_lock, flags);
	if (g_mtk_rescan_context.rescan_done == 0) {
		dev_err(&pdev->dev, "mtk rescan failed because last rescan undone\n");
		goto fail_unlock;
	}
	g_mtk_rescan_context.dev = pdev;
	dev_dbg(&pdev->dev, "get lock and start to queue\n");
	spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
	queue_work(g_mtk_rescan_context.pcie_rescan_wq,
		   &g_mtk_rescan_context.service_task);
	return;

fail_unlock:
	spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
}

int mtk_rescan_init(void)
{
	pr_info("%s: Enter\n", __func__);
	spin_lock_init(&g_mtk_rescan_context.dev_lock);
	g_mtk_rescan_context.rescan_done = 1;
	g_mtk_rescan_context.dev = NULL;
	g_mtk_rescan_context.pcie_rescan_wq =
		create_singlethread_workqueue(MTK_RESCAN_WQ);
	if (!g_mtk_rescan_context.pcie_rescan_wq) {
		pr_err("Failed to create workqueue: %s\n", MTK_RESCAN_WQ);
		goto fail;
	}
	INIT_WORK(&g_mtk_rescan_context.service_task, mtk_remove_rescan);

	return 0;

fail:
	destroy_workqueue(g_mtk_rescan_context.pcie_rescan_wq);
	return -1;
}

void mtk_rescan_deinit(void)
{
	unsigned long flags;

	pr_debug("Enter\n");
	spin_lock_irqsave(&g_mtk_rescan_context.dev_lock, flags);
	g_mtk_rescan_context.rescan_done = 1;
	g_mtk_rescan_context.dev = NULL;
	spin_unlock_irqrestore(&g_mtk_rescan_context.dev_lock, flags);
	cancel_work_sync(&g_mtk_rescan_context.service_task);
	destroy_workqueue(g_mtk_rescan_context.pcie_rescan_wq);
}
