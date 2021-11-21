// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include "t7xx_pci.h"
#include "t7xx_isr.h"
#include "t7xx_modem_ops.h"

static inline void int_callback(struct mtk_pci_dev *mtk_dev, int pcie_int)
{
	if (mtk_dev->intr_callback[pcie_int])
		mtk_dev->intr_callback[pcie_int](mtk_dev->callback_param[pcie_int]);
}

static irqreturn_t pcie_msix_irq(int irq, void *res)
{
	struct mtk_pci_isr_res *mtk_isr_res = (struct mtk_pci_isr_res *)res;
	struct mtk_pci_dev *mtk_dev = mtk_isr_res->mtk_dev;
	unsigned int msix_status = mtk_msix_status_get(IREG_BASE(mtk_dev));
	int isr_vector = mtk_isr_res->isr_vector;
	bool swq_en = false;
	int i;

	/* Disable interrupt with bit mask */
	mtk_enable_disable_msix_mask(mtk_dev->pdev, isr_vector, false);

	for (i = VALID_INT_START + isr_vector; i <= VALID_INT_END; i += mtk_dev->irq_count) {
		if (msix_status & BIT(i)) {
			PCIE_MSIX_MSK_SET(IREG_BASE(mtk_dev), i);
			if (i >= EXT_INT_START) {
				int_callback(mtk_dev, i - EXT_INT_START);
			} else if (i >= P_ATR_INT_START) {
				set_bit(PCIE_P_ATR_INT, mtk_dev->state);
				swq_en = true;
			} else if (i >= A_ATR_INT_START) {
				set_bit(PCIE_A_ATR_INT, mtk_dev->state);
				swq_en = true;
			}
		}
	}

	if (swq_en)
		pcie_schedule_isr_service_task(mtk_dev);

	/* Enable interrupt with bit mask */
	mtk_enable_disable_msix_mask(mtk_dev->pdev, isr_vector, true);

	return IRQ_HANDLED;
}

struct mtk_pci_isr_res *pci_isr_alloc_table(struct mtk_pci_dev *mtk_dev, int msi_count)
{
	struct mtk_pci_isr_res *res;
	int i;

	res = devm_kcalloc(&mtk_dev->pdev->dev, msi_count, sizeof(*res), GFP_KERNEL);
	if (!res)
		return NULL;

	for (i = 0; i < msi_count; i++) {
		res[i].isr_vector = i;
		res[i].isr = pcie_msix_irq;
	}
	return res;
}

void pcie_schedule_isr_service_task(struct mtk_pci_dev *mtk_dev)
{
	if (!test_and_set_bit(PCIE_SERVICE_SCHED, mtk_dev->state))
		queue_work(mtk_dev->pcie_isr_wq, &mtk_dev->service_task);
}

static void pcie_handle_sw_int(struct mtk_pci_dev *mtk_dev)
{
	u32 int_sts;

	if (!test_bit(PCIE_EP2RC_SW_INT, mtk_dev->state))
		return;

	/* Use 1*4 bits to avoid low power bits*/
	iowrite32(L1_1_DISABLE_BIT(1) | L1_2_DISABLE_BIT(1),
		  IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);

	int_sts = mhccif_read_sw_int_sts(mtk_dev);
	if (int_sts & mtk_dev->mhccif_bitmask)
		mtk_pci_mhccif_isr(mtk_dev);

	/* Clear 2 & 1 level interrupts */
	mtk_pcie_clear_mhccif_int(mtk_dev, int_sts);

	if (int_sts & D2H_INT_DS_LOCK_ACK)
		complete_all(&mtk_dev->l_res_acquire);
	if (int_sts & D2H_INT_SUSPEND_ACK)
		complete(&mtk_dev->pm_suspend_ack);
	if (int_sts & D2H_INT_RESUME_ACK)
		complete(&mtk_dev->pm_resume_ack);
	if (int_sts & D2H_INT_SUSPEND_ACK_AP)
		complete(&mtk_dev->pm_suspend_ack_sap);
	if (int_sts & D2H_INT_RESUME_ACK_AP)
		complete(&mtk_dev->pm_resume_ack_sap);

	/* Use the 1 bits to avoid low power bits */
	iowrite32(L1_DISABLE_BIT(1), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);

	int_sts = mhccif_read_sw_int_sts(mtk_dev);
	if (!int_sts)
		iowrite32(L1_1_DISABLE_BIT(1) | L1_2_DISABLE_BIT(1),
			  IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);

	/* Re-enable related interrupt */
	clear_bit(PCIE_EP2RC_SW_INT, mtk_dev->state);

	/* Enable corresponding interrupt */
	mtk_clear_set_int_type(mtk_dev, MHCCIF_INT, false);
}

static void pcie_handle_a_atr_int(struct mtk_pci_dev *mtk_dev)
{
	if (!test_bit(PCIE_A_ATR_INT, mtk_dev->state))
		return;

	/* Clear interrupt */
	mtk_clear_int_status(mtk_dev, A_ATR_INT);

	/* Re-enable related interrupt */
	clear_bit(PCIE_A_ATR_INT, mtk_dev->state);

	/* Enable corresponding interrupt */
	mtk_clear_set_int_type(mtk_dev, A_ATR_INT, false);
}

static void pcie_handle_p_atr_int(struct mtk_pci_dev *mtk_dev)
{
	if (!test_bit(PCIE_P_ATR_INT, mtk_dev->state))
		return;

	/* Clear interrupt */
	mtk_clear_int_status(mtk_dev, P_ATR_INT);

	/* Re-enable related interrupt */
	clear_bit(PCIE_P_ATR_INT, mtk_dev->state);

	/* Enable corresponding interrupt */
	mtk_clear_set_int_type(mtk_dev, P_ATR_INT, false);
}

void pcie_isr_service_task(struct work_struct *work)
{
	struct mtk_pci_dev *mtk_dev = container_of(work, struct mtk_pci_dev,
						   service_task);

	pcie_handle_sw_int(mtk_dev);
	pcie_handle_a_atr_int(mtk_dev);
	pcie_handle_p_atr_int(mtk_dev);

	/* Flush memory to sync state */
	smp_mb();
	clear_bit(PCIE_SERVICE_SCHED, mtk_dev->state);

	/* Reschedule for additinonal tasks*/
	if (test_bit(PCIE_EP2RC_SW_INT, mtk_dev->state))
		pcie_schedule_isr_service_task(mtk_dev);
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

static void mhccif_update_reg_trsl_addr(struct mtk_pci_dev *mtk_dev)
{
	mtk_dev->base_addr.mhccif_rc_base =
		mtk_dev->base_addr.pcie_ext_reg_base +
		MHCCIF_RC_DEV_BASE - mtk_dev->base_addr.pcie_dev_reg_trsl_addr;
	mtk_dev->base_addr.mhccif_ep_base =
		mtk_dev->base_addr.pcie_ext_reg_base +
		MHCCIF_EP_DEV_BASE - mtk_dev->base_addr.pcie_dev_reg_trsl_addr;
}

static int mhccif_int_callback(void *param)
{
	struct mtk_pci_dev *mtk_dev = param;

	set_bit(PCIE_EP2RC_SW_INT, mtk_dev->state);

	pcie_schedule_isr_service_task(mtk_dev);

	return 0;
}

void mhccif_init(struct mtk_pci_dev *mtk_dev)
{
	mhccif_update_reg_trsl_addr(mtk_dev);

	/* Register MHCCHIF int handler to handle */
	mtk_dev->intr_callback[MHCCIF_INT] = mhccif_int_callback;
	mtk_dev->callback_param[MHCCIF_INT] = (void *)mtk_dev;
}

void mtk_pcie_clear_mhccif_int(struct mtk_pci_dev *mtk_dev, u32 reg)
{
	void __iomem *mhccif_pbase;

	mhccif_pbase = mtk_dev->base_addr.mhccif_rc_base;

	/* Clear level 2 interrupt */
	iowrite32(reg, mhccif_pbase + REG_EP2RC_SW_INT_ACK);

	/* Read back to ensure write is done */
	mhccif_read_sw_int_sts(mtk_dev);

	/* Clear level 1 interrupt */
	mtk_clear_int_status(mtk_dev, MHCCIF_INT);
}

void mhccif_h2d_swint_trigger(struct mtk_pci_dev *mtk_dev, u32 channel)
{
	void __iomem *mhccif_pbase = mtk_dev->base_addr.mhccif_rc_base;

	iowrite32(BIT(channel), mhccif_pbase + REG_RC2EP_SW_BSY);
	iowrite32(channel, mhccif_pbase + REG_RC2EP_SW_TCHNUM);
}
