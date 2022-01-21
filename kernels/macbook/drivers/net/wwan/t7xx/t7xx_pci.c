// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>

#include "t7xx_mhccif.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_pci.h"
#include "t7xx_pcie_mac.h"
#include "t7xx_reg.h"
#include "t7xx_skb_util.h"

#define	PCI_IREG_BASE			0
#define	PCI_EREG_BASE			2

#define MTK_WAIT_TIMEOUT_MS		10
#define PM_ACK_TIMEOUT_MS		1500
#define PM_AUTOSUSPEND_MS		20000
#define PM_RESOURCE_POLL_TIMEOUT_US	10000
#define PM_RESOURCE_POLL_STEP_US	100

enum mtk_pm_state {
	MTK_PM_EXCEPTION,	/* Exception flow */
	MTK_PM_INIT,		/* Device initialized, but handshake not completed */
	MTK_PM_SUSPENDED,	/* Device in suspend state */
	MTK_PM_RESUMED,		/* Device in resume state */
};

static void mtk_dev_set_sleep_capability(struct mtk_pci_dev *mtk_dev, bool enable)
{
	void __iomem *ctrl_reg;
	u32 value;

	ctrl_reg = IREG_BASE(mtk_dev) + PCIE_MISC_CTRL;
	value = ioread32(ctrl_reg);

	if (enable)
		value &= ~PCIE_MISC_MAC_SLEEP_DIS;
	else
		value |= PCIE_MISC_MAC_SLEEP_DIS;

	iowrite32(value, ctrl_reg);
}

static int mtk_wait_pm_config(struct mtk_pci_dev *mtk_dev)
{
	int ret, val;

	ret = read_poll_timeout(ioread32, val,
				(val & PCIE_RESOURCE_STATUS_MSK) == PCIE_RESOURCE_STATUS_MSK,
				PM_RESOURCE_POLL_STEP_US, PM_RESOURCE_POLL_TIMEOUT_US, true,
				IREG_BASE(mtk_dev) + PCIE_RESOURCE_STATUS);
	if (ret == -ETIMEDOUT)
		dev_err(&mtk_dev->pdev->dev, "PM configuration timed out\n");

	return ret;
}

static int mtk_pci_pm_init(struct mtk_pci_dev *mtk_dev)
{
	struct pci_dev *pdev;

	pdev = mtk_dev->pdev;

	INIT_LIST_HEAD(&mtk_dev->md_pm_entities);

	spin_lock_init(&mtk_dev->md_pm_lock);

	mutex_init(&mtk_dev->md_pm_entity_mtx);

	init_completion(&mtk_dev->sleep_lock_acquire);
	init_completion(&mtk_dev->pm_sr_ack);

	atomic_set(&mtk_dev->sleep_disable_count, 0);
	device_init_wakeup(&pdev->dev, true);

	dev_pm_set_driver_flags(&pdev->dev, pdev->dev.power.driver_flags |
				DPM_FLAG_NO_DIRECT_COMPLETE);

	atomic_set(&mtk_dev->md_pm_state, MTK_PM_INIT);

	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	pm_runtime_set_autosuspend_delay(&pdev->dev, PM_AUTOSUSPEND_MS);
	pm_runtime_use_autosuspend(&pdev->dev);

	udelay(1000);
	return 0;
	//return mtk_wait_pm_config(mtk_dev);
}

void mtk_pci_pm_init_late(struct mtk_pci_dev *mtk_dev)
{
	/* enable the PCIe Resource Lock only after MD deep sleep is done */
	mhccif_mask_clr(mtk_dev,
			D2H_INT_DS_LOCK_ACK |
			D2H_INT_SUSPEND_ACK |
			D2H_INT_RESUME_ACK |
			D2H_INT_SUSPEND_ACK_AP |
			D2H_INT_RESUME_ACK_AP);
	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);
	atomic_set(&mtk_dev->md_pm_state, MTK_PM_RESUMED);

	pm_runtime_put_noidle(&mtk_dev->pdev->dev);
}

static int mtk_pci_pm_reinit(struct mtk_pci_dev *mtk_dev)
{
	/* The device is kept in FSM re-init flow
	 * so just roll back PM setting to the init setting.
	 */
	atomic_set(&mtk_dev->md_pm_state, MTK_PM_INIT);

	pm_runtime_get_noresume(&mtk_dev->pdev->dev);

	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	return mtk_wait_pm_config(mtk_dev);
}

void mtk_pci_pm_exp_detected(struct mtk_pci_dev *mtk_dev)
{
	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	mtk_wait_pm_config(mtk_dev);
	atomic_set(&mtk_dev->md_pm_state, MTK_PM_EXCEPTION);
}

int mtk_pci_pm_entity_register(struct mtk_pci_dev *mtk_dev, struct md_pm_entity *pm_entity)
{
	struct md_pm_entity *entity;

	mutex_lock(&mtk_dev->md_pm_entity_mtx);
	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->id == pm_entity->id) {
			mutex_unlock(&mtk_dev->md_pm_entity_mtx);
			return -EEXIST;
		}
	}

	list_add_tail(&pm_entity->entity, &mtk_dev->md_pm_entities);
	mutex_unlock(&mtk_dev->md_pm_entity_mtx);
	return 0;
}

int mtk_pci_pm_entity_unregister(struct mtk_pci_dev *mtk_dev, struct md_pm_entity *pm_entity)
{
	struct md_pm_entity *entity, *tmp_entity;

	mutex_lock(&mtk_dev->md_pm_entity_mtx);

	list_for_each_entry_safe(entity, tmp_entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->id == pm_entity->id) {
			list_del(&pm_entity->entity);
			mutex_unlock(&mtk_dev->md_pm_entity_mtx);
			return 0;
		}
	}

	mutex_unlock(&mtk_dev->md_pm_entity_mtx);

	return -ENXIO;
}

int mtk_pci_sleep_disable_complete(struct mtk_pci_dev *mtk_dev)
{
	int ret;

	ret = wait_for_completion_timeout(&mtk_dev->sleep_lock_acquire,
					  msecs_to_jiffies(MTK_WAIT_TIMEOUT_MS));
	if (!ret)
		dev_err_ratelimited(&mtk_dev->pdev->dev, "Resource wait complete timed out\n");

	return ret;
}

/**
 * mtk_pci_disable_sleep() - disable deep sleep capability
 * @mtk_dev: MTK device
 *
 * Lock the deep sleep capabitily, note that the device can go into deep sleep
 * state while it is still in D0 state from the host point of view.
 *
 * If device is in deep sleep state then wake up the device and disable deep sleep capability.
 */
void mtk_pci_disable_sleep(struct mtk_pci_dev *mtk_dev)
{
	unsigned long flags;

	if (atomic_read(&mtk_dev->md_pm_state) < MTK_PM_RESUMED) {
		atomic_inc(&mtk_dev->sleep_disable_count);
		complete_all(&mtk_dev->sleep_lock_acquire);
		return;
	}

	spin_lock_irqsave(&mtk_dev->md_pm_lock, flags);
	if (atomic_inc_return(&mtk_dev->sleep_disable_count) == 1) {
		reinit_completion(&mtk_dev->sleep_lock_acquire);
		mtk_dev_set_sleep_capability(mtk_dev, false);
		/* read register status to check whether the device's
		 * deep sleep is disabled or not.
		 */
		if ((ioread32(IREG_BASE(mtk_dev) + PCIE_RESOURCE_STATUS) &
		     PCIE_RESOURCE_STATUS_MSK) == PCIE_RESOURCE_STATUS_MSK) {
			spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
			complete_all(&mtk_dev->sleep_lock_acquire);
			return;
		}

		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_DS_LOCK);
	}

	spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
}

/**
 * mtk_pci_enable_sleep() - enable deep sleep capability
 * @mtk_dev: MTK device
 *
 * After enabling deep sleep, device can enter into deep sleep state.
 */
void mtk_pci_enable_sleep(struct mtk_pci_dev *mtk_dev)
{
	unsigned long flags;

	if (atomic_read(&mtk_dev->md_pm_state) < MTK_PM_RESUMED) {
		atomic_dec(&mtk_dev->sleep_disable_count);
		return;
	}

	if (atomic_dec_and_test(&mtk_dev->sleep_disable_count)) {
		spin_lock_irqsave(&mtk_dev->md_pm_lock, flags);
		mtk_dev_set_sleep_capability(mtk_dev, true);
		spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
	}
}

static int __mtk_pci_pm_suspend(struct pci_dev *pdev)
{
	struct mtk_pci_dev *mtk_dev;
	struct md_pm_entity *entity;
	unsigned long wait_ret;
	enum mtk_pm_id id;
	int ret = 0;

	mtk_dev = pci_get_drvdata(pdev);

	if (atomic_read(&mtk_dev->md_pm_state) <= MTK_PM_INIT) {
		dev_err(&pdev->dev,
			"[PM] Exiting suspend, because handshake failure or in an exception\n");
		return -EFAULT;
	}

	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	ret = mtk_wait_pm_config(mtk_dev);
	if (ret)
		return ret;

	atomic_set(&mtk_dev->md_pm_state, MTK_PM_SUSPENDED);

	mtk_pcie_mac_clear_int(mtk_dev, SAP_RGU_INT);
	mtk_dev->rgu_pci_irq_en = false;

	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->suspend) {
			ret = entity->suspend(mtk_dev, entity->entity_param);
			if (ret) {
				id = entity->id;
				break;
			}
		}
	}

	if (ret) {
		dev_err(&pdev->dev, "[PM] Suspend error: %d, id: %d\n", ret, id);

		list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
			if (id == entity->id)
				break;

			if (entity->resume)
				entity->resume(mtk_dev, entity->entity_param);
		}

		goto suspend_complete;
	}

	reinit_completion(&mtk_dev->pm_sr_ack);
	/* send D3 enter request to MD */
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_SUSPEND_REQ);
	wait_ret = wait_for_completion_timeout(&mtk_dev->pm_sr_ack,
					       msecs_to_jiffies(PM_ACK_TIMEOUT_MS));
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Wait for device suspend ACK timeout-MD\n");

	reinit_completion(&mtk_dev->pm_sr_ack);
	/* send D3 enter request to sAP */
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_SUSPEND_REQ_AP);
	wait_ret = wait_for_completion_timeout(&mtk_dev->pm_sr_ack,
					       msecs_to_jiffies(PM_ACK_TIMEOUT_MS));
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Wait for device suspend ACK timeout-SAP\n");

	/* Each HW's final work */
	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->suspend_late)
			entity->suspend_late(mtk_dev, entity->entity_param);
	}

suspend_complete:
	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);
	if (ret) {
		atomic_set(&mtk_dev->md_pm_state, MTK_PM_RESUMED);
		mtk_pcie_mac_set_int(mtk_dev, SAP_RGU_INT);
	}

	return ret;
}

static void mtk_pcie_interrupt_reinit(struct mtk_pci_dev *mtk_dev)
{
	mtk_pcie_mac_msix_cfg(mtk_dev, EXT_INT_NUM);

	/* Disable interrupt first and let the IPs enable them */
	iowrite32(MSIX_MSK_SET_ALL, IREG_BASE(mtk_dev) + IMASK_HOST_MSIX_CLR_GRP0_0);

	/* Device disables PCIe interrupts during resume and
	 * following function will re-enable PCIe interrupts.
	 */
	mtk_pcie_mac_interrupts_en(mtk_dev);
	mtk_pcie_mac_set_int(mtk_dev, MHCCIF_INT);
}

static int mtk_pcie_reinit(struct mtk_pci_dev *mtk_dev, bool is_d3)
{
	int ret;

	ret = pcim_enable_device(mtk_dev->pdev);
	if (ret)
		return ret;

	mtk_pcie_mac_atr_init(mtk_dev);
	mtk_pcie_interrupt_reinit(mtk_dev);

	if (is_d3) {
		mhccif_init(mtk_dev);
		return mtk_pci_pm_reinit(mtk_dev);
	}

	return 0;
}

static int mtk_send_fsm_command(struct mtk_pci_dev *mtk_dev, u32 event)
{
	struct ccci_fsm_ctl *fsm_ctl;
	int ret = -EINVAL;

	fsm_ctl = fsm_get_entry();

	switch (event) {
	case CCCI_COMMAND_STOP:
		ret = fsm_append_command(fsm_ctl, CCCI_COMMAND_STOP, 1);
		break;

	case CCCI_COMMAND_START:
		mtk_pcie_mac_clear_int(mtk_dev, SAP_RGU_INT);
		mtk_pcie_mac_clear_int_status(mtk_dev, SAP_RGU_INT);
		mtk_dev->rgu_pci_irq_en = true;
		mtk_pcie_mac_set_int(mtk_dev, SAP_RGU_INT);
		ret = fsm_append_command(fsm_ctl, CCCI_COMMAND_START, 0);
		break;

	default:
		break;
	}
	if (ret)
		dev_err(&mtk_dev->pdev->dev, "handling FSM CMD event: %u error: %d\n", event, ret);

	return ret;
}

static int __mtk_pci_pm_resume(struct pci_dev *pdev, bool state_check)
{
	struct mtk_pci_dev *mtk_dev;
	struct md_pm_entity *entity;
	unsigned long wait_ret;
	u32 resume_reg_state;
	int ret = 0;

	mtk_dev = pci_get_drvdata(pdev);

	if (atomic_read(&mtk_dev->md_pm_state) <= MTK_PM_INIT) {
		iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);
		return 0;
	}

	/* Get the previous state */
	resume_reg_state = ioread32(IREG_BASE(mtk_dev) + PCIE_PM_RESUME_STATE);

	if (state_check) {
		/* For D3/L3 resume, the device could boot so quickly that the
		 * initial value of the dummy register might be overwritten.
		 * Identify new boots if the ATR source address register is not initialized.
		 */
		u32 atr_reg_val = ioread32(IREG_BASE(mtk_dev) +
					   ATR_PCIE_WIN0_T0_ATR_PARAM_SRC_ADDR);

		if (resume_reg_state == PM_RESUME_REG_STATE_L3 ||
		    (resume_reg_state == PM_RESUME_REG_STATE_INIT &&
		     atr_reg_val == ATR_SRC_ADDR_INVALID)) {
			ret = mtk_send_fsm_command(mtk_dev, CCCI_COMMAND_STOP);
			if (ret)
				return ret;

			ret = mtk_pcie_reinit(mtk_dev, true);
			if (ret)
				return ret;

			mtk_clear_rgu_irq(mtk_dev);
			return mtk_send_fsm_command(mtk_dev, CCCI_COMMAND_START);
		} else if (resume_reg_state == PM_RESUME_REG_STATE_EXP ||
			   resume_reg_state == PM_RESUME_REG_STATE_L2_EXP) {
			if (resume_reg_state == PM_RESUME_REG_STATE_L2_EXP) {
				ret = mtk_pcie_reinit(mtk_dev, false);
				if (ret)
					return ret;
			}

			atomic_set(&mtk_dev->md_pm_state, MTK_PM_SUSPENDED);
			mtk_dev->rgu_pci_irq_en = true;
			mtk_pcie_mac_set_int(mtk_dev, SAP_RGU_INT);

			mhccif_mask_clr(mtk_dev,
					D2H_INT_EXCEPTION_INIT |
					D2H_INT_EXCEPTION_INIT_DONE |
					D2H_INT_EXCEPTION_CLEARQ_DONE |
					D2H_INT_EXCEPTION_ALLQ_RESET |
					D2H_INT_PORT_ENUM);

			return ret;
		} else if (resume_reg_state == PM_RESUME_REG_STATE_L2) {
			ret = mtk_pcie_reinit(mtk_dev, false);
			if (ret)
				return ret;

		} else if (resume_reg_state != PM_RESUME_REG_STATE_L1 &&
			   resume_reg_state != PM_RESUME_REG_STATE_INIT) {
			ret = mtk_send_fsm_command(mtk_dev, CCCI_COMMAND_STOP);
			if (ret)
				return ret;

			mtk_clear_rgu_irq(mtk_dev);
			atomic_set(&mtk_dev->md_pm_state, MTK_PM_SUSPENDED);
			return 0;
		}
	}

	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	mtk_wait_pm_config(mtk_dev);

	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->resume_early)
			entity->resume_early(mtk_dev, entity->entity_param);
	}

	reinit_completion(&mtk_dev->pm_sr_ack);
	/* send D3 exit request to MD */
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_RESUME_REQ);
	wait_ret = wait_for_completion_timeout(&mtk_dev->pm_sr_ack,
					       msecs_to_jiffies(PM_ACK_TIMEOUT_MS));
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Timed out waiting for device MD resume ACK\n");

	reinit_completion(&mtk_dev->pm_sr_ack);
	/* send D3 exit request to sAP */
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_RESUME_REQ_AP);
	wait_ret = wait_for_completion_timeout(&mtk_dev->pm_sr_ack,
					       msecs_to_jiffies(PM_ACK_TIMEOUT_MS));
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Timed out waiting for device SAP resume ACK\n");

	/* Each HW final restore works */
	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->resume) {
			ret = entity->resume(mtk_dev, entity->entity_param);
			if (ret)
				dev_err(&pdev->dev, "[PM] Resume entry ID: %d err: %d\n",
					entity->id, ret);
		}
	}

	mtk_dev->rgu_pci_irq_en = true;
	mtk_pcie_mac_set_int(mtk_dev, SAP_RGU_INT);
	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);
	pm_runtime_mark_last_busy(&pdev->dev);
	atomic_set(&mtk_dev->md_pm_state, MTK_PM_RESUMED);

	return ret;
}

static int mtk_pci_pm_resume_noirq(struct device *dev)
{
	struct mtk_pci_dev *mtk_dev;
	struct pci_dev *pdev;
	void __iomem *pbase;

	pdev = to_pci_dev(dev);
	mtk_dev = pci_get_drvdata(pdev);
	pbase = IREG_BASE(mtk_dev);

	/* disable interrupt first and let the IPs enable them */
	iowrite32(MSIX_MSK_SET_ALL, pbase + IMASK_HOST_MSIX_CLR_GRP0_0);

	return 0;
}

static void mtk_pci_shutdown(struct pci_dev *pdev)
{
	__mtk_pci_pm_suspend(pdev);
}

static int mtk_pci_pm_suspend(struct device *dev)
{
	return __mtk_pci_pm_suspend(to_pci_dev(dev));
}

static int mtk_pci_pm_resume(struct device *dev)
{
	return __mtk_pci_pm_resume(to_pci_dev(dev), true);
}

static int mtk_pci_pm_thaw(struct device *dev)
{
	return __mtk_pci_pm_resume(to_pci_dev(dev), false);
}

static int mtk_pci_pm_runtime_suspend(struct device *dev)
{
	return __mtk_pci_pm_suspend(to_pci_dev(dev));
}

static int mtk_pci_pm_runtime_resume(struct device *dev)
{
	return __mtk_pci_pm_resume(to_pci_dev(dev), true);
}

static const struct dev_pm_ops mtk_pci_pm_ops = {
	.suspend = mtk_pci_pm_suspend,
	.resume = mtk_pci_pm_resume,
	.resume_noirq = mtk_pci_pm_resume_noirq,
	.freeze = mtk_pci_pm_suspend,
	.thaw = mtk_pci_pm_thaw,
	.poweroff = mtk_pci_pm_suspend,
	.restore = mtk_pci_pm_resume,
	.restore_noirq = mtk_pci_pm_resume_noirq,
	.runtime_suspend = mtk_pci_pm_runtime_suspend,
	.runtime_resume = mtk_pci_pm_runtime_resume
};

static int mtk_request_irq(struct pci_dev *pdev)
{
	struct mtk_pci_dev *mtk_dev;
	int ret, i;

	mtk_dev = pci_get_drvdata(pdev);

	for (i = 0; i < EXT_INT_NUM; i++) {
		const char *irq_descr;
		int irq_vec;

		if (!mtk_dev->intr_handler[i])
			continue;

		irq_descr = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s_%d", pdev->driver->name, i);
		if (!irq_descr)
			return -ENOMEM;

		irq_vec = pci_irq_vector(pdev, i);
		ret = request_threaded_irq(irq_vec, mtk_dev->intr_handler[i],
					   mtk_dev->intr_thread[i], 0, irq_descr,
					   mtk_dev->callback_param[i]);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request_irq: %d, int: %d, ret: %d\n",
				irq_vec, i, ret);
			while (i--) {
				if (!mtk_dev->intr_handler[i])
					continue;

				free_irq(pci_irq_vector(pdev, i), mtk_dev->callback_param[i]);
			}

			return ret;
		}
	}

	return 0;
}

static int mtk_setup_msix(struct mtk_pci_dev *mtk_dev)
{
	int ret;

	/* We are interested only in 6 interrupts, but HW-design requires power-of-2
	 * IRQs allocation.
	 */
	ret = pci_alloc_irq_vectors(mtk_dev->pdev, EXT_INT_NUM, EXT_INT_NUM, PCI_IRQ_MSIX);
	if (ret < 0) {
		dev_err(&mtk_dev->pdev->dev, "Failed to allocate MSI-X entry, errno: %d\n", ret);
		return ret;
	}

	ret = mtk_request_irq(mtk_dev->pdev);
	if (ret) {
		pci_free_irq_vectors(mtk_dev->pdev);
		return ret;
	}

	/* Set MSIX merge config */
	mtk_pcie_mac_msix_cfg(mtk_dev, EXT_INT_NUM);
	return 0;
}

static int mtk_interrupt_init(struct mtk_pci_dev *mtk_dev)
{
	int ret, i;

	if (!mtk_dev->pdev->msix_cap)
		return -EINVAL;

	ret = mtk_setup_msix(mtk_dev);
	if (ret)
		return ret;

	/* let the IPs enable interrupts when they are ready */
	for (i = EXT_INT_START; i < EXT_INT_START + EXT_INT_NUM; i++)
		PCIE_MAC_MSIX_MSK_SET(mtk_dev, i);

	return 0;
}

static inline void mtk_pci_infracfg_ao_calc(struct mtk_pci_dev *mtk_dev)
{
	mtk_dev->base_addr.infracfg_ao_base = mtk_dev->base_addr.pcie_ext_reg_base +
					      INFRACFG_AO_DEV_CHIP -
					      mtk_dev->base_addr.pcie_dev_reg_trsl_addr;
}

static int mtk_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mtk_pci_dev *mtk_dev;
	int ret;

	mtk_dev = devm_kzalloc(&pdev->dev, sizeof(*mtk_dev), GFP_KERNEL);
	if (!mtk_dev)
		return -ENOMEM;

	pci_set_drvdata(pdev, mtk_dev);
	mtk_dev->pdev = pdev;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	ret = pcim_iomap_regions(pdev, BIT(PCI_IREG_BASE) | BIT(PCI_EREG_BASE), pci_name(pdev));
	if (ret) {
		dev_err(&pdev->dev, "PCIm iomap regions fail %d\n", ret);
		return -ENOMEM;
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(&pdev->dev, "Could not set PCI DMA mask, err: %d\n", ret);
			return ret;
		}
	}

	ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(&pdev->dev, "Could not set consistent PCI DMA mask, err: %d\n",
				ret);
			return ret;
		}
	}

	IREG_BASE(mtk_dev) = pcim_iomap_table(pdev)[PCI_IREG_BASE];
	mtk_dev->base_addr.pcie_ext_reg_base = pcim_iomap_table(pdev)[PCI_EREG_BASE];

	ret = ccci_skb_pool_alloc(&mtk_dev->pools);
	if (ret)
		return ret;

	ret = mtk_pci_pm_init(mtk_dev);
	if (ret)
		goto err;

	mtk_pcie_mac_atr_init(mtk_dev);
	mtk_pci_infracfg_ao_calc(mtk_dev);
	mhccif_init(mtk_dev);

	ret = mtk_md_init(mtk_dev);
	if (ret)
		goto err;

	mtk_pcie_mac_interrupts_dis(mtk_dev);
	ret = mtk_interrupt_init(mtk_dev);
	if (ret)
		goto err;

	mtk_pcie_mac_set_int(mtk_dev, MHCCIF_INT);
	mtk_pcie_mac_interrupts_en(mtk_dev);
	pci_set_master(pdev);

	return 0;

err:
	ccci_skb_pool_free(&mtk_dev->pools);
	return ret;
}

static void mtk_pci_remove(struct pci_dev *pdev)
{
	struct mtk_pci_dev *mtk_dev;
	int i;

	mtk_dev = pci_get_drvdata(pdev);
	mtk_md_exit(mtk_dev);

	for (i = 0; i < EXT_INT_NUM; i++) {
		if (!mtk_dev->intr_handler[i])
			continue;

		free_irq(pci_irq_vector(pdev, i), mtk_dev->callback_param[i]);
	}

	pci_free_irq_vectors(mtk_dev->pdev);
	ccci_skb_pool_free(&mtk_dev->pools);
}

static const struct pci_device_id t7xx_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x4d75) },
	{ }
};
MODULE_DEVICE_TABLE(pci, t7xx_pci_table);

static struct pci_driver mtk_pci_driver = {
	.name = "mtk_t7xx",
	.id_table = t7xx_pci_table,
	.probe = mtk_pci_probe,
	.remove = mtk_pci_remove,
	.driver.pm = &mtk_pci_pm_ops,
	.shutdown = mtk_pci_shutdown,
};

static int __init mtk_pci_init(void)
{
	return pci_register_driver(&mtk_pci_driver);
}
module_init(mtk_pci_init);

static void __exit mtk_pci_cleanup(void)
{
	pci_unregister_driver(&mtk_pci_driver);
}
module_exit(mtk_pci_cleanup);

MODULE_AUTHOR("MediaTek Inc");
MODULE_DESCRIPTION("MediaTek PCIe 5G WWAN modem t7xx driver");
MODULE_LICENSE("GPL");
