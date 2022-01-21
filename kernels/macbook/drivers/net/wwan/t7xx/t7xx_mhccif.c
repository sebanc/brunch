// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/completion.h>
#include <linux/pci.h>

#include "t7xx_mhccif.h"
#include "t7xx_modem_ops.h"
#include "t7xx_pci.h"
#include "t7xx_pcie_mac.h"

#define D2H_INT_SR_ACK		(D2H_INT_SUSPEND_ACK |		\
				 D2H_INT_RESUME_ACK |		\
				 D2H_INT_SUSPEND_ACK_AP |	\
				 D2H_INT_RESUME_ACK_AP)

static void mhccif_clear_interrupts(struct mtk_pci_dev *mtk_dev, u32 mask)
{
	void __iomem *mhccif_pbase;

	mhccif_pbase = mtk_dev->base_addr.mhccif_rc_base;

	/* Clear level 2 interrupt */
	iowrite32(mask, mhccif_pbase + REG_EP2RC_SW_INT_ACK);

	/* Read back to ensure write is done */
	mhccif_read_sw_int_sts(mtk_dev);

	/* Clear level 1 interrupt */
	mtk_pcie_mac_clear_int_status(mtk_dev, MHCCIF_INT);
}

static irqreturn_t mhccif_isr_thread(int irq, void *data)
{
	struct mtk_pci_dev *mtk_dev;
	u32 int_sts;

	mtk_dev = data;

	/* Use 1*4 bits to avoid low power bits*/
	iowrite32(L1_1_DISABLE_BIT(1) | L1_2_DISABLE_BIT(1),
		  IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);

	int_sts = mhccif_read_sw_int_sts(mtk_dev);
	if (int_sts & mtk_dev->mhccif_bitmask)
		mtk_pci_mhccif_isr(mtk_dev);

	/* Clear 2 & 1 level interrupts */
	mhccif_clear_interrupts(mtk_dev, int_sts);

	if (int_sts & D2H_INT_DS_LOCK_ACK)
		complete_all(&mtk_dev->sleep_lock_acquire);

	if (int_sts & D2H_INT_SR_ACK)
		complete(&mtk_dev->pm_sr_ack);

	/* Use the 1 bits to avoid low power bits */
	iowrite32(L1_DISABLE_BIT(1), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);

	int_sts = mhccif_read_sw_int_sts(mtk_dev);
	if (!int_sts)
		iowrite32(L1_1_DISABLE_BIT(1) | L1_2_DISABLE_BIT(1),
			  IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);

	/* Enable corresponding interrupt */
	mtk_pcie_mac_set_int(mtk_dev, MHCCIF_INT);
	return IRQ_HANDLED;
}

u32 mhccif_read_sw_int_sts(struct mtk_pci_dev *mtk_dev)
{
	return ioread32(mtk_dev->base_addr.mhccif_rc_base + REG_EP2RC_SW_INT_STS);
}

void mhccif_mask_set(struct mtk_pci_dev *mtk_dev, u32 val)
{
	iowrite32(val, mtk_dev->base_addr.mhccif_rc_base + REG_EP2RC_SW_INT_EAP_MASK_SET);
}

void mhccif_mask_clr(struct mtk_pci_dev *mtk_dev, u32 val)
{
	iowrite32(val, mtk_dev->base_addr.mhccif_rc_base + REG_EP2RC_SW_INT_EAP_MASK_CLR);
}

u32 mhccif_mask_get(struct mtk_pci_dev *mtk_dev)
{
	/* mtk_dev is validated in calling function */
	return ioread32(mtk_dev->base_addr.mhccif_rc_base + REG_EP2RC_SW_INT_EAP_MASK);
}

static irqreturn_t mhccif_isr_handler(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

void mhccif_init(struct mtk_pci_dev *mtk_dev)
{
	mtk_dev->base_addr.mhccif_rc_base = mtk_dev->base_addr.pcie_ext_reg_base +
					    MHCCIF_RC_DEV_BASE -
					    mtk_dev->base_addr.pcie_dev_reg_trsl_addr;

	/* Register MHCCHIF int handler to handle */
	mtk_dev->intr_handler[MHCCIF_INT] = mhccif_isr_handler;
	mtk_dev->intr_thread[MHCCIF_INT] = mhccif_isr_thread;
	mtk_dev->callback_param[MHCCIF_INT] = mtk_dev;
}

void mhccif_h2d_swint_trigger(struct mtk_pci_dev *mtk_dev, u32 channel)
{
	void __iomem *mhccif_pbase;

	mhccif_pbase = mtk_dev->base_addr.mhccif_rc_base;
	iowrite32(BIT(channel), mhccif_pbase + REG_RC2EP_SW_BSY);
	iowrite32(channel, mhccif_pbase + REG_RC2EP_SW_TCHNUM);
}
