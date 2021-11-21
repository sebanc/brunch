/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_ISR_H__
#define __T7XX_ISR_H__

#include "t7xx_reg.h"

#define MTK_IRQ_COUNT		16
#define MTK_IRQ_MSIX_MIN_COUNT	2
#define D2H_SW_INT_MASK (D2H_INT_EXCEPTION_INIT | \
			 D2H_INT_EXCEPTION_INIT_DONE | \
			 D2H_INT_EXCEPTION_CLEARQ_DONE | \
			 D2H_INT_EXCEPTION_ALLQ_RESET | \
			 D2H_INT_PORT_ENUM | \
			 D2H_INT_ASYNC_MD_HK)

struct mtk_pci_isr_res {
	struct mtk_pci_dev *mtk_dev;
	int isr_vector;
	int irq;
	irqreturn_t (*isr)(int irq, void *res);
	char irq_descr[64];
};

struct mtk_pci_isr_res *pci_isr_alloc_table(struct mtk_pci_dev *mtk_dev, int msi_count);
void pcie_isr_service_task(struct work_struct *work);
void pcie_schedule_isr_service_task(struct mtk_pci_dev *mtk_dev);
void mhccif_mask_set(struct mtk_pci_dev *mtk_dev, u32 val);
void mhccif_mask_clr(struct mtk_pci_dev *mtk_dev, u32 val);
u32 mhccif_mask_get(struct mtk_pci_dev *mtk_dev);
void mhccif_init(struct mtk_pci_dev *mtk_dev);
void mtk_pcie_clear_mhccif_int(struct mtk_pci_dev *mtk_dev, u32 reg);
u32 mhccif_read_sw_int_sts(struct mtk_pci_dev *mtk_dev);
void mhccif_h2d_swint_trigger(struct mtk_pci_dev *mtk_dev, u32 channel);

#endif /*__T7XX_ISR_H__ */
