/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PCI_H__
#define __T7XX_PCI_H__

#include <linux/pci.h>

struct mtk_pci_dev;
#include "t7xx_reg.h"
#include "t7xx_pci_ats.h"

#define MTK_PCI_WQ_DESCR	"ccci_wq"

enum mtk_pcie_state_t {
	PCIE_EP2RC_SW_INT,
	PCIE_A_ATR_INT,
	PCIE_P_ATR_INT,
	PCIE_SERVICE_SCHED,
};

enum mtk_pcie_event_t {
	PCIE_EVENT_L3ENTER,
	PCIE_EVENT_L3EXIT,
};

#define PM_RESUME_REG_STATE_L3		0
#define PM_RESUME_REG_STATE_L1		1
#define PM_RESUME_REG_STATE_INIT	2
#define PM_RESUME_REG_STATE_EXP		3
#define PM_RESUME_REG_STATE_L2		4
#define PM_RESUME_REG_STATE_L2_EXP	5

#define PM_ENTITY_KEY_CTRL		1U	/* Control Path */
#define PM_ENTITY_KEY_CTRL2		2U	/* Control Path */
#define PM_ENTITY_KEY_DATA		3U	/* Data Path */
#define PM_ENTITY_KEY_RESERVE0		4U	/* Reserved */
#define PM_ENTITY_KEY_RESERVE1		5U	/* Reserved */

#define PM_RESUME_HEADER		0x52450000
#define PM_SUSPEND_HEADER		0x53550000

#define MSIX_MSK_SET_ALL		0xFF000000

typedef int (*mtk_pci_intr_callback)(void *param);

/* struct md_pm_entity - device power management entity
 * @entity: list of PM Entities
 * @suspend: callback invoked before sending D3 request to device
 * @suspend_late: callback invoked after getting D3 ACK from device
 * @resume_early: callback invoked before sending the resume request to device
 * @resume: callback invoked after getting resume ACK from device
 * @key: unique PM entity identifier
 * @entity_param: parameter passed to the registered callbacks
 */
struct md_pm_entity {
	struct list_head entity;
	int (*suspend)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	int (*suspend_late)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	int (*resume_early)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	int (*resume)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	unsigned int		key;
	void			*entity_param;
};

struct mtk_pci_dev {
	unsigned long		state[1];
	int			irq_count;
	struct mtk_pci_isr_res	*msix_res;
	mtk_pci_intr_callback	intr_callback[TOTAL_EXT_INT];
	void			*callback_param[TOTAL_EXT_INT];
	u32			mhccif_bitmask;
	struct pci_dev		*pdev;
	struct work_struct	service_task;
	struct mtk_addr_base	base_addr;
	struct workqueue_struct	*pcie_isr_wq;
	struct ccci_modem	*md;

	/* Low Power Items */
	struct list_head	md_pm_entities;
	spinlock_t		md_pm_lock;		/* protects PCI resource lock */
	struct mutex		md_pm_entity_mtx;	/* protects md_pm_entities list */
	atomic_t		l_res_lock_count;	/* PCIe L1.2 lock counter */
	struct completion	l_res_acquire;
	struct completion	pm_suspend_ack;
	struct completion	pm_resume_ack;
	struct completion	pm_suspend_ack_sap;
	struct completion	pm_resume_ack_sap;
	atomic_t		md_pm_init_done;
	atomic_t		md_pm_resumed;
	struct delayed_work	l_res_unlock_work;
	atomic_t		pm_counter;

	struct ccmni_ctl_block	*ccmni_ctlb;
	u32			pm_enabled:1;
	atomic_t		rgu_pci_irq_en;
};

void mtk_pci_l_resource_lock(struct mtk_pci_dev *mtk_dev);
void mtk_pci_l_resource_unlock(struct mtk_pci_dev *mtk_dev);
void mtk_pci_l_resource_wait_complete(struct mtk_pci_dev *mtk_dev);
/* Non-blocking function for interrupt context to check the wait status
 * return value:
 * @true, wait is completed
 * @false, wait is in progress
 */
bool mtk_pci_l_resource_try_wait_complete(struct mtk_pci_dev *mtk_dev);
int mtk_pci_pm_entity_register(struct mtk_pci_dev *mtk_dev, struct md_pm_entity *pm_entity);
int mtk_pci_pm_entity_unregister(struct mtk_pci_dev *mtk_dev, struct md_pm_entity *pm_entity);
void mtk_pci_pm_init_late(struct mtk_pci_dev *mtk_dev);
void mtk_pci_pm_exp_detected(struct mtk_pci_dev *mtk_dev);

#endif /* __T7XX_PCI_H__ */
