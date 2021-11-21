// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/msi.h>

#include "t7xx_pci.h"
#include "t7xx_pci_ats.h"

static void mtk_disable_atr_table(void __iomem *pbase, enum atr_src_port port,
				  u32 table)
{
	void __iomem *reg;
	u32 val;
	int offset;

	/* Calculate table offset */
	offset = (ATR_PORT_OFFSET * port) + (ATR_TABLE_OFFSET * table);

	/* Disable table by SRC_ADDR_L */
	reg = pbase + ATR_PCIE_WIN0_T0_ATR_PARAM_SRC_ADDR_LSB + offset;
	val = 0x0;
	iowrite32(val, reg);
}

static void mtk_disable_all_atr_table(void __iomem *pbase,
				      enum atr_src_port port)
{
	int i;

	for (i = 0; i < ATR_TABLE_NUM_PER_ATR; i++)
		mtk_disable_atr_table(pbase, port, i);
}

static int mtk_setup_atr(void __iomem *pbase, struct mtk_atr_config *cfg)
{
	void __iomem *reg;
	u32 val, size_h, size_l;
	int atr_size, pos, offset;

	if (cfg->transparent) {
		/* No address conversion is performed */
		atr_size = ATR_TRANSPARENT_SIZE;
	} else {
		size_l = (u32)cfg->size;
		size_h = cfg->size >> 32;
		pos = ffs(size_l);
		if (pos) {
			atr_size = pos - 2;
		} else {
			pos = ffs(size_h);
			atr_size = pos + 32 - 2;
		}

		if (cfg->src_addr & (cfg->size - 1)) {
			pr_err("[%s]Src addr is not aligned to size\n", __FILE__);
			return -EINVAL;
		}

		if (cfg->trsl_addr & (cfg->size - 1)) {
			pr_err("[%s]Trsl addr %llx not aligned to size %llx\n",
			       __FILE__, cfg->trsl_addr, cfg->size - 1);
			return -EINVAL;
		}
	}

	/* Calculate table offset */
	offset = ATR_PORT_OFFSET * cfg->port + ATR_TABLE_OFFSET * cfg->table;

	/* SRC_ADDR_H */
	reg = pbase + ATR_PCIE_WIN0_T0_SRC_ADDR_MSB + offset;
	val = (u32)(cfg->src_addr >> 32);
	iowrite32(val, reg);

	/* TRSL_ADDR_L */
	reg = pbase + ATR_PCIE_WIN0_T0_TRSL_ADDR_LSB + offset;
	val = (u32)(cfg->trsl_addr & ATR_PCIE_WIN0_ADDR_LSB_ALGMT);
	iowrite32(val, reg);

	/* TRSL_ADDR_H */
	reg = pbase + ATR_PCIE_WIN0_T0_TRSL_ADDR_MSB + offset;
	val = (u32)(cfg->trsl_addr >> 32);
	iowrite32(val, reg);

	/* TRSL_PARAM */
	reg = pbase + ATR_PCIE_WIN0_T0_TRSL_PARAM + offset;
	val = (cfg->trsf_param << 16) | cfg->trsl_id;
	iowrite32(val, reg);

	/* SRC_ADDR_L */
	reg = pbase + ATR_PCIE_WIN0_T0_ATR_PARAM_SRC_ADDR_LSB + offset;
	val = (u32)(cfg->src_addr & ATR_PCIE_WIN0_ADDR_LSB_ALGMT) | (atr_size << 1) | 0x1;
	iowrite32(val, reg);

	/* Read back to ensure ATR is set */
	ioread32(reg);

	return 0;
}

int mtk_atr_init(struct mtk_pci_dev *mtk_dev)
{
	struct mtk_atr_config cfg;
	u32 i;

	/* Disable all ATR table for all ports */
	for (i = ATR_SRC_PCI_WIN0; i <= ATR_SRC_AXIS_3; i++)
		mtk_disable_all_atr_table(IREG_BASE(mtk_dev), i);

	/* Config ATR for RC to access device's register */
	cfg.src_addr = pci_resource_start(mtk_dev->pdev, PCIE_REG_BAR);
	cfg.size = PCIE_REG_SIZE_CHIP;
	cfg.trsl_addr = PCIE_REG_TRSL_ADDR_CHIP;
	cfg.port = PCIE_REG_PORT;
	cfg.table = PCIE_REG_TABLE_NUM;
	cfg.trsl_id = PCIE_REG_TRSL_PORT;
	cfg.trsf_param = 0x0;
	cfg.transparent = 0x0;
	mtk_disable_all_atr_table(IREG_BASE(mtk_dev), cfg.port);
	mtk_setup_atr(IREG_BASE(mtk_dev), &cfg);

	/* Update translation base */
	mtk_dev->base_addr.pcie_dev_reg_trsl_addr = PCIE_REG_TRSL_ADDR_CHIP;

	/* Config ATR for EP to access RC's memory */
	for (i = PCIE_DEV_DMA_PORT_START; i <= PCIE_DEV_DMA_PORT_END; i++) {
		cfg.src_addr = PCIE_DEV_DMA_SRC_ADDR;
		cfg.size = PCIE_DEV_DMA_SIZE;
		cfg.trsl_addr = PCIE_DEV_DMA_TRSL_ADDR;
		cfg.port = i;
		cfg.table = PCIE_DEV_DMA_TABLE_NUM;
		cfg.trsl_id = ATR_DST_PCI_TRX;
		cfg.trsf_param = 0x0;
		/* Enable transparent translation */
		cfg.transparent = PCIE_DEV_DMA_TRANSPARENT;
		mtk_disable_all_atr_table(IREG_BASE(mtk_dev), cfg.port);
		mtk_setup_atr(IREG_BASE(mtk_dev), &cfg);
	}

	/* After this, MMIO of BAR2/3 would be used by upper layer */
	mb();

	return 0;
}

void mtk_enable_disable_int(void __iomem *pbase, bool enable)
{
	union istat_hst_ctrl istat;

	istat.raw = ioread32(pbase + ISTAT_HST_CTRL);
	istat.bits.ISTAT_HST_DIS = !enable;
	iowrite32(istat.raw, pbase + ISTAT_HST_CTRL);
}

void mtk_enable_disable_msix_mask(struct pci_dev *dev, u32 entry_nr, bool enable)
{
#ifdef CONFIG_PCI_MSI
	struct msi_desc *desc;
	u32 mask_bits;
	unsigned int offset = entry_nr * PCI_MSIX_ENTRY_SIZE +
						PCI_MSIX_ENTRY_VECTOR_CTRL;

	desc = first_pci_msi_entry(dev);
	mask_bits = ioread32(desc->mask_base + offset);

	mask_bits &= ~PCI_MSIX_ENTRY_CTRL_MASKBIT;
	if (!enable)
		mask_bits |= PCI_MSIX_ENTRY_CTRL_MASKBIT;
	iowrite32(mask_bits, desc->mask_base + offset);
#endif
}

void mtk_clear_set_int_type(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type, bool clear)
{
	void __iomem *reg;
	union int_hst new_values;

	if (mtk_dev->pdev->msix_enabled) {
		if (clear)
			reg = IREG_BASE(mtk_dev) + IMASK_HOST_MSIX_CLR_GRP0_0;
		else
			reg = IREG_BASE(mtk_dev) + IMASK_HOST_MSIX_SET_GRP0_0;
	} else {
		if (clear)
			reg = IREG_BASE(mtk_dev) + INT_EN_HST_CLR;
		else
			reg = IREG_BASE(mtk_dev) + INT_EN_HST_SET;
	}

	new_values.raw = 0;
	if (int_type <= END_EXT_INT)
		new_values.raw = BIT(EXT_INT_START + int_type);
	else if (int_type == A_ATR_INT)
		new_values.bits.A_ATR_EVT = 0xf;
	else if (int_type == P_ATR_INT)
		new_values.bits.P_ATR_EVT = 0xf;

	iowrite32(new_values.raw, reg);

	/* make sure the interrupt clear/set is done before carrying on with other process */
	mb();
}

void mtk_clear_int_status(struct mtk_pci_dev *mtk_dev, enum pcie_int int_type)
{
	void __iomem *reg;
	union int_hst istat;

	if (mtk_dev->pdev->msix_enabled)
		reg = IREG_BASE(mtk_dev) + MSIX_ISTAT_HST_GRP0_0;
	else
		reg = IREG_BASE(mtk_dev) + ISTAT_HST;

	istat.raw = 0;
	if (int_type == A_ATR_INT)
		istat.bits.A_ATR_EVT = 0xf;
	else if (int_type == P_ATR_INT)
		istat.bits.P_ATR_EVT = 0xf;
	else
		istat.raw |= BIT(EXT_INT_START + int_type);

	iowrite32(istat.raw, reg);
}

unsigned int mtk_msix_status_get(void __iomem *pbase)
{
	return (ioread32(pbase + MSIX_ISTAT_HST_GRP0_0) &
		ioread32(pbase + IMASK_HOST_MSIX_GRP0_0));
}

void mtk_msix_ctrl_cfg(void __iomem *pbase, int msix_count)
{
	if (msix_count == 0)
		iowrite32(0, pbase + PCIE_CFG_MSIX);
	else
		iowrite32(ffs(msix_count) * 2 - 1, pbase + PCIE_CFG_MSIX);
}

void mtk_dummy_reg_set(struct mtk_pci_dev *mtk_dev, int val)
{
	iowrite32(val, IREG_BASE(mtk_dev) + PCIE_DEBUG_DUMMY_7);
}

u32 mtk_dummy_reg_get(struct mtk_pci_dev *mtk_dev)
{
	return ioread32(IREG_BASE(mtk_dev) + PCIE_DEBUG_DUMMY_7);
}
