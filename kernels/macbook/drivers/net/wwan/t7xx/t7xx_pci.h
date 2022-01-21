/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PCI_H__
#define __T7XX_PCI_H__

#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "t7xx_reg.h"
#include "t7xx_skb_util.h"

/* struct mtk_addr_base - holds base addresses
 * @pcie_mac_ireg_base: PCIe MAC register base
 * @pcie_ext_reg_base: used to calculate base addresses for CLDMA, DPMA and MHCCIF registers
 * @pcie_dev_reg_trsl_addr: used to calculate the register base address
 * @infracfg_ao_base: base address used in CLDMA reset operations
 * @mhccif_rc_base: host view of MHCCIF rc base addr
 */
struct mtk_addr_base {
	void __iomem *pcie_mac_ireg_base;
	void __iomem *pcie_ext_reg_base;
	u32 pcie_dev_reg_trsl_addr;
	void __iomem *infracfg_ao_base;
	void __iomem *mhccif_rc_base;
};

typedef irqreturn_t (*mtk_intr_callback)(int irq, void *param);

/* struct mtk_pci_dev - MTK device context structure
 * @intr_handler: array of handler function for request_threaded_irq
 * @intr_thread: array of thread_fn for request_threaded_irq
 * @callback_param: array of cookie passed back to interrupt functions
 * @mhccif_bitmask: device to host interrupt mask
 * @pdev: pci device
 * @base_addr: memory base addresses of HW components
 * @md: modem interface
 * @md_pm_entities: list of pm entities
 * @md_pm_lock: protects PCIe sleep lock
 * @md_pm_entity_mtx: protects md_pm_entities list
 * @sleep_disable_count: PCIe L1.2 lock counter
 * @sleep_lock_acquire: indicates that sleep has been disabled
 * @pm_sr_ack: ack from the device when went to sleep or woke up
 * @md_pm_state: state for resume/suspend
 * @ccmni_ctlb: context structure used to control the network data path
 * @rgu_pci_irq_en: RGU callback isr registered and active
 * @pools: pre allocated skb pools
 */
struct mtk_pci_dev {
	mtk_intr_callback	intr_handler[EXT_INT_NUM];
	mtk_intr_callback	intr_thread[EXT_INT_NUM];
	void			*callback_param[EXT_INT_NUM];
	u32			mhccif_bitmask;
	struct pci_dev		*pdev;
	struct mtk_addr_base	base_addr;
	struct mtk_modem	*md;

	/* Low Power Items */
	struct list_head	md_pm_entities;
	spinlock_t		md_pm_lock;		/* protects PCI resource lock */
	struct mutex		md_pm_entity_mtx;	/* protects md_pm_entities list */
	atomic_t		sleep_disable_count;
	struct completion	sleep_lock_acquire;
	struct completion	pm_sr_ack;
	atomic_t		md_pm_state;

	struct ccmni_ctl_block	*ccmni_ctlb;
	bool			rgu_pci_irq_en;
	struct skb_pools	pools;
};

enum mtk_pm_id {
	PM_ENTITY_ID_CTRL1,
	PM_ENTITY_ID_CTRL2,
	PM_ENTITY_ID_DATA,
};

/* struct md_pm_entity - device power management entity
 * @entity: list of PM Entities
 * @suspend: callback invoked before sending D3 request to device
 * @suspend_late: callback invoked after getting D3 ACK from device
 * @resume_early: callback invoked before sending the resume request to device
 * @resume: callback invoked after getting resume ACK from device
 * @id: unique PM entity identifier
 * @entity_param: parameter passed to the registered callbacks
 *
 *  This structure is used to indicate PM operations required by internal
 *  HW modules such as CLDMA and DPMA.
 */
struct md_pm_entity {
	struct list_head	entity;
	int (*suspend)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	void (*suspend_late)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	void (*resume_early)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	int (*resume)(struct mtk_pci_dev *mtk_dev, void *entity_param);
	enum mtk_pm_id		id;
	void			*entity_param;
};

void mtk_pci_disable_sleep(struct mtk_pci_dev *mtk_dev);
void mtk_pci_enable_sleep(struct mtk_pci_dev *mtk_dev);
int mtk_pci_sleep_disable_complete(struct mtk_pci_dev *mtk_dev);
int mtk_pci_pm_entity_register(struct mtk_pci_dev *mtk_dev, struct md_pm_entity *pm_entity);
int mtk_pci_pm_entity_unregister(struct mtk_pci_dev *mtk_dev, struct md_pm_entity *pm_entity);
void mtk_pci_pm_init_late(struct mtk_pci_dev *mtk_dev);
void mtk_pci_pm_exp_detected(struct mtk_pci_dev *mtk_dev);

#endif /* __T7XX_PCI_H__ */
