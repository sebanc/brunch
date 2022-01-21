/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PCIE_MAC_H__
#define __T7XX_PCIE_MAC_H__

#include <linux/bitops.h>
#include <linux/io.h>

#include "t7xx_pci.h"
#include "t7xx_reg.h"

#define IREG_BASE(mtk_dev)	((mtk_dev)->base_addr.pcie_mac_ireg_base)

#define PCIE_MAC_MSIX_MSK_SET(mtk_dev, ext_id)	\
	iowrite32(BIT(ext_id), IREG_BASE(mtk_dev) + IMASK_HOST_MSIX_SET_GRP0_0)

void mtk_pcie_mac_interrupts_en(struct mtk_pci_dev *mtk_dev);
void mtk_pcie_mac_interrupts_dis(struct mtk_pci_dev *mtk_dev);
void mtk_pcie_mac_atr_init(struct mtk_pci_dev *mtk_dev);
void mtk_pcie_mac_clear_int(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type);
void mtk_pcie_mac_set_int(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type);
void mtk_pcie_mac_clear_int_status(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type);
void mtk_pcie_mac_msix_cfg(struct mtk_pci_dev *mtk_dev, unsigned int irq_count);

#endif /* __T7XX_PCIE_MAC_H__ */
