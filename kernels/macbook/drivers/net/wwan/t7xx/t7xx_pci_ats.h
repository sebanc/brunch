/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PCI_ATS_H__
#define __T7XX_PCI_ATS_H__

#include "t7xx_reg.h"

#define PCIE_REG_BAR			2
#define PCIE_REG_PORT			ATR_SRC_PCI_WIN0
#define PCIE_REG_TABLE_NUM		0
#define PCIE_REG_TRSL_PORT		ATR_DST_AXIM_0

#define PCIE_DEV_DMA_PORT_START		ATR_SRC_AXIS_0
#define PCIE_DEV_DMA_PORT_END		ATR_SRC_AXIS_2
#define PCIE_DEV_DMA_TABLE_NUM		0
#define PCIE_DEV_DMA_TRSL_ADDR		0x00000000

#define PCIE_DEV_DMA_SRC_ADDR		0x00000000
#define PCIE_DEV_DMA_TRANSPARENT	1
#define PCIE_DEV_DMA_SIZE		0

#define PCIE_MSIX_MSK_SET(pbase, ext_id)	\
	iowrite32(BIT(ext_id), (pbase) + IMASK_HOST_MSIX_SET_GRP0_0)

struct mtk_addr_base {
	void __iomem *pcie_mac_ireg_base; /* PCIe MAC register base */
	void __iomem *pcie_ext_reg_base; /* ext reg base */
	u32 pcie_dev_reg_trsl_addr;
	void __iomem *infracfg_ao_base; /* host view infra ao base */
	void __iomem *mhccif_rc_base; /* host view of mhccif rc base addr */
	void __iomem *mhccif_ep_base; /* host view of mhccif ep base addr */
};

enum atr_type {
	ATR_PCI2AXI = 0,
	ATR_AXI2PCI
};

enum atr_src_port {
	ATR_SRC_PCI_WIN0 = 0,
	ATR_SRC_PCI_WIN1,
	ATR_SRC_AXIS_0,
	ATR_SRC_AXIS_1,
	ATR_SRC_AXIS_2,
	ATR_SRC_AXIS_3,
};

enum atr_dst_port {
	ATR_DST_PCI_TRX = 0,
	ATR_DST_PCI_CONFIG,
	ATR_DST_AXIM_0 = 4,
	ATR_DST_AXIM_1,
	ATR_DST_AXIM_2,
	ATR_DST_AXIM_3,
};

struct mtk_atr_config {
	u64 src_addr;
	u64 trsl_addr;
	u64 size;
	u32 port;
	u32 table; //Table number(8 tables for each port)
	enum atr_dst_port trsl_id;
	u32 trsf_param;
	u32 transparent;
};

void mtk_enable_disable_int(void __iomem *pbase, bool enable);
int mtk_atr_init(struct mtk_pci_dev *mtk_dev);
void mtk_enable_disable_msix_mask(struct pci_dev *dev, u32 entry_nr, bool enable);
void mtk_clear_set_int_type(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type, bool clear);
void mtk_clear_int_status(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type);
unsigned int mtk_msix_status_get(void __iomem *pbase);
void mtk_msix_ctrl_cfg(void __iomem *pbase, int msix_count);
void mtk_dummy_reg_set(struct mtk_pci_dev *mtk_dev, int val);
u32 mtk_dummy_reg_get(struct mtk_pci_dev *mtk_dev);

#endif /* __T7XX_PCI_ATS_H__ */
