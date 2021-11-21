// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>

#include "t7xx_pci.h"
#include "t7xx_isr.h"
#include "t7xx_dev_node.h"
#include "t7xx_modem_ops.h"

#define RT_IDLE_DELAY 20
#define PM_ACK_TIMEOUT 1500
#define PM_RESOURCE_POLL_TIMEOUT 10000
#define PM_RESOURCE_POLL_STEP 100
#define PM_RESOURCE_UNLOCK_DELAY 100

static bool res_delayed_unlock = true;

static const struct pci_device_id mtk_dev_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, MTK_DEV_T7XX) },
	{ }
};
MODULE_DEVICE_TABLE(pci, mtk_dev_ids);

static void mtk_dev_lock_set(struct mtk_pci_dev *mtk_dev, bool lock)
{
	void __iomem *ctrl_reg = IREG_BASE(mtk_dev) + PCIE_MISC_CTRL;
	u32 value;

	value = ioread32(ctrl_reg);
	if (lock)
		value |= PCIE_MISC_MAC_SLEEP_DIS;
	else
		value &= ~PCIE_MISC_MAC_SLEEP_DIS;
	iowrite32(value, ctrl_reg);
}

static inline void mtk_dev_lock(struct mtk_pci_dev *mtk_dev)
{
	mtk_dev_lock_set(mtk_dev, 1);
}

static inline void mtk_dev_unlock(struct mtk_pci_dev *mtk_dev)
{
	mtk_dev_lock_set(mtk_dev, 0);
}

static void mtk_wait_pm_config(struct mtk_pci_dev *mtk_dev)
{
	int ret, val;

	ret = read_poll_timeout(ioread32, val,
				(val & PCIE_RESOURCE_STATUS_MSK) == PCIE_RESOURCE_STATUS_MSK,
				PM_RESOURCE_POLL_STEP,
				PM_RESOURCE_POLL_TIMEOUT, true,
				IREG_BASE(mtk_dev) + PCIE_RESOURCE_STATUS);
	if (ret == -ETIMEDOUT)
		dev_err(&mtk_dev->pdev->dev, "PM config timeout\n");
}

static void mtk_l_res_unlock_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mtk_pci_dev *mtk_dev;
	unsigned long flags;

	mtk_dev = container_of(dwork, struct mtk_pci_dev, l_res_unlock_work);
	spin_lock_irqsave(&mtk_dev->md_pm_lock, flags);

	if (!atomic_read(&mtk_dev->l_res_lock_count))
		mtk_dev_unlock(mtk_dev);

	spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
}

static void mtk_pci_pm_init(struct mtk_pci_dev *mtk_dev)
{
	struct pci_dev *pdev = mtk_dev->pdev;

	INIT_LIST_HEAD(&mtk_dev->md_pm_entities);

	spin_lock_init(&mtk_dev->md_pm_lock);

	mutex_init(&mtk_dev->md_pm_entity_mtx);

	init_completion(&mtk_dev->l_res_acquire);

	init_completion(&mtk_dev->pm_suspend_ack);
	init_completion(&mtk_dev->pm_resume_ack);

	init_completion(&mtk_dev->pm_suspend_ack_sap);
	init_completion(&mtk_dev->pm_resume_ack_sap);

	INIT_DELAYED_WORK(&mtk_dev->l_res_unlock_work, mtk_l_res_unlock_work);

	atomic_set(&mtk_dev->l_res_lock_count, 0);
	device_init_wakeup(&pdev->dev, true);

	dev_pm_set_driver_flags(&pdev->dev,
				pdev->dev.power.driver_flags |
				DPM_FLAG_NO_DIRECT_COMPLETE);

	atomic_set(&mtk_dev->md_pm_init_done, 0);
	atomic_set(&mtk_dev->md_pm_resumed, 1);

	atomic_set(&mtk_dev->pm_counter, 0);

	iowrite32(L1_DISABLE_BIT(0),
		  IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	mtk_wait_pm_config(mtk_dev);
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
	atomic_inc(&mtk_dev->md_pm_init_done);

	/* enable runtime PM only after CCCI HS is done */
	pm_runtime_put_noidle(&mtk_dev->pdev->dev);
}

static void mtk_pci_pm_deinit(struct mtk_pci_dev *mtk_dev)
{
	cancel_delayed_work_sync(&mtk_dev->l_res_unlock_work);
}

static void mtk_pci_pm_reinit(struct mtk_pci_dev *mtk_dev)
{
	/* The device is kept in FSM re-init flow
	 * so just roll back PM setting to the init setting
	 */
	atomic_set(&mtk_dev->md_pm_init_done, 0);
	atomic_set(&mtk_dev->md_pm_resumed, 1);

	pm_runtime_get_noresume(&mtk_dev->pdev->dev);

	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	mtk_wait_pm_config(mtk_dev);
}

void mtk_pci_pm_exp_detected(struct mtk_pci_dev *mtk_dev)
{
	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	mtk_wait_pm_config(mtk_dev);
	atomic_dec(&mtk_dev->md_pm_init_done);
}

int mtk_pci_pm_entity_register(struct mtk_pci_dev *mtk_dev,
			       struct md_pm_entity *pm_entity)
{
	struct md_pm_entity *entity, *tmp_entity;

	mutex_lock(&mtk_dev->md_pm_entity_mtx);
	list_for_each_entry_safe(entity, tmp_entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->key == pm_entity->key) {
			dev_err(&mtk_dev->pdev->dev, "Registering an exist PM entity %d\n",
				pm_entity->key);
			mutex_unlock(&mtk_dev->md_pm_entity_mtx);
			return -EALREADY;
		}
	}
	list_add_tail(&pm_entity->entity, &mtk_dev->md_pm_entities);
	mutex_unlock(&mtk_dev->md_pm_entity_mtx);
	return 0;
}

int mtk_pci_pm_entity_unregister(struct mtk_pci_dev *mtk_dev,
				 struct md_pm_entity *pm_entity)
{
	struct md_pm_entity *entity;

	mutex_lock(&mtk_dev->md_pm_entity_mtx);

	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->key == pm_entity->key) {
			list_del(&pm_entity->entity);
			mutex_unlock(&mtk_dev->md_pm_entity_mtx);
			return 0;
		}
	}
	mutex_unlock(&mtk_dev->md_pm_entity_mtx);

	return -EALREADY;
}

void mtk_pci_l_resource_wait_complete(struct mtk_pci_dev *mtk_dev)
{
	if (mtk_dev->pm_enabled)
		wait_for_completion(&mtk_dev->l_res_acquire);
}

bool mtk_pci_l_resource_try_wait_complete(struct mtk_pci_dev *mtk_dev)
{
	if (mtk_dev->pm_enabled)
		return try_wait_for_completion(&mtk_dev->l_res_acquire);

	return true;
}

void mtk_pci_l_resource_lock(struct mtk_pci_dev *mtk_dev)
{
	unsigned long flags;

	if (!mtk_dev->pm_enabled)
		return;

	if ((atomic_read(&mtk_dev->md_pm_init_done) < 1) ||
	    (!atomic_read(&mtk_dev->md_pm_resumed))) {
		atomic_inc(&mtk_dev->l_res_lock_count);
		complete_all(&mtk_dev->l_res_acquire);
		return;
	}

	spin_lock_irqsave(&mtk_dev->md_pm_lock, flags);
	if (atomic_inc_return(&mtk_dev->l_res_lock_count) == 1) {
		reinit_completion(&mtk_dev->l_res_acquire);
		mtk_dev_lock(mtk_dev);

		if ((ioread32(IREG_BASE(mtk_dev) + PCIE_RESOURCE_STATUS) &
		     PCIE_RESOURCE_STATUS_MSK) == PCIE_RESOURCE_STATUS_MSK) {
			spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
			complete_all(&mtk_dev->l_res_acquire);
			return;
		}

		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_DS_LOCK);
		spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
}

void mtk_pci_l_resource_unlock(struct mtk_pci_dev *mtk_dev)
{
	unsigned long flags;

	if (!mtk_dev->pm_enabled)
		return;

	if ((atomic_read(&mtk_dev->md_pm_init_done) < 1) ||
	    (!atomic_read(&mtk_dev->md_pm_resumed))) {
		atomic_dec(&mtk_dev->l_res_lock_count);
		return;
	}

	if (atomic_dec_and_test(&mtk_dev->l_res_lock_count)) {
		if (res_delayed_unlock) {
			cancel_delayed_work(&mtk_dev->l_res_unlock_work);
			schedule_delayed_work(&mtk_dev->l_res_unlock_work,
					      msecs_to_jiffies(PM_RESOURCE_UNLOCK_DELAY));
		} else {
			spin_lock_irqsave(&mtk_dev->md_pm_lock, flags);
			mtk_dev_unlock(mtk_dev);
			spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
		}
	}
}

static int __mtk_pci_wait_suspend_ack(struct mtk_pci_dev *mtk_dev)
{
	return wait_for_completion_timeout(&mtk_dev->pm_suspend_ack,
					   msecs_to_jiffies(PM_ACK_TIMEOUT));
}

static int __mtk_pci_wait_resume_ack(struct mtk_pci_dev *mtk_dev)
{
	return wait_for_completion_timeout(&mtk_dev->pm_resume_ack,
					   msecs_to_jiffies(PM_ACK_TIMEOUT));
}

static int __mtk_pci_wait_suspend_ack_sap(struct mtk_pci_dev *mtk_dev)
{
	return wait_for_completion_timeout(&mtk_dev->pm_suspend_ack_sap,
					   msecs_to_jiffies(PM_ACK_TIMEOUT));
}

static int __mtk_pci_wait_resume_ack_sap(struct mtk_pci_dev *mtk_dev)
{
	return wait_for_completion_timeout(&mtk_dev->pm_resume_ack_sap,
					   msecs_to_jiffies(PM_ACK_TIMEOUT));
}

/* Counter to distinguish suspend/resume cycles */
static void mtk_pci_pm_counter(struct mtk_pci_dev *mtk_dev, u32 header)
{
	u32 pm_cnt = atomic_inc_return(&mtk_dev->pm_counter) & 0xffff;

	iowrite32(header | pm_cnt,
		  mtk_dev->base_addr.mhccif_rc_base + MHCCIF_H2D_REG_PCIE_PM_COUNTER);
}

static int __mtk_pci_pm_suspend(struct pci_dev *pdev, bool is_runtime)
{
	struct mtk_pci_dev *mtk_dev = pci_get_drvdata(pdev);
	struct md_pm_entity *entity;
	unsigned long wait_ret;
	unsigned int key = 255;
	int ret = 0;

	if (!mtk_dev->pm_enabled)
		return 0;

	if (atomic_read(&mtk_dev->md_pm_init_done) < 1) {
		dev_err(&pdev->dev,
			"[PM] Suspend Exit Because CCCI not Handshaked or in EE\n");
		return -EFAULT;
	}

	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_SET_0);
	mtk_wait_pm_config(mtk_dev);

	atomic_dec(&mtk_dev->md_pm_resumed);

	mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, true);
	atomic_set(&mtk_dev->rgu_pci_irq_en, 0);

	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->suspend) {
			ret = entity->suspend(mtk_dev, entity->entity_param);
			if (ret) {
				key = entity->key;
				break;
			}
		}
	}

	if (ret) {
		dev_err(&pdev->dev, "[PM] Suspend error %d, key %d\n", ret, key);

		list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
			if (key == entity->key)
				break;
			if (entity->resume)
				entity->resume(mtk_dev, entity->entity_param);
		}
		goto suspend_complete;
	}

	reinit_completion(&mtk_dev->pm_suspend_ack);
	reinit_completion(&mtk_dev->pm_suspend_ack_sap);
	/* Start to let device enter D3/L2 */
	mtk_pci_pm_counter(mtk_dev, PM_SUSPEND_HEADER);
	/* send d3 enter requests */
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_SUSPEND_REQ);
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_SUSPEND_REQ_AP);

	wait_ret = __mtk_pci_wait_suspend_ack(mtk_dev);
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Wait for device suspend ACK timeout\n");

	wait_ret = __mtk_pci_wait_suspend_ack_sap(mtk_dev);
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Wait for device suspend ACK timeout-SAP\n");

	/* Each HW's final work */
	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->suspend_late) {
			ret = entity->suspend_late(mtk_dev, entity->entity_param);
			if (ret) {
				key = entity->key;
				break;
			}
		}
	}

	if (res_delayed_unlock) {
		unsigned long flags = 0;

		cancel_delayed_work_sync(&mtk_dev->l_res_unlock_work);
		if (!atomic_read(&mtk_dev->l_res_lock_count)) {
			spin_lock_irqsave(&mtk_dev->md_pm_lock, flags);
			mtk_dev_unlock(mtk_dev);
			spin_unlock_irqrestore(&mtk_dev->md_pm_lock, flags);
		} else {
			dev_err(&pdev->dev, "[PM] resource lock count is not zero\n");
		}
	}

	if (ret)
		dev_err(&pdev->dev, "[PM] Suspend Late Entry error %d, key %d\n", ret, key);

suspend_complete:
	if (ret) {
		iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);
		atomic_inc(&mtk_dev->md_pm_resumed);
		mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, false);
	}

	return ret;
}

static void mtk_pcie_interrupt_reinit(struct mtk_pci_dev *mtk_dev)
{
	void __iomem *pbase = IREG_BASE(mtk_dev);

	mtk_msix_ctrl_cfg(pbase, mtk_dev->irq_count);

	/* disable interrupt first and let the IPs enable them */
	iowrite32(MSIX_MSK_SET_ALL, pbase + IMASK_HOST_MSIX_CLR_GRP0_0);

	/* L2 resume device will disable PCIe interrupt, and
	 * this function can re-enable PCIe interrupt for L3,
	 * just do this with no effect.
	 */
	mtk_enable_disable_int(pbase, true);

	mtk_clear_set_int_type(mtk_dev, MHCCIF_INT, false);
}

static int mtk_pcie_reinit(struct mtk_pci_dev *mtk_dev, bool pcie_only)
{
	int ret = pci_enable_device(mtk_dev->pdev);

	if (ret) {
		dev_err(&mtk_dev->pdev->dev, "pci_enable_device return fail\n");
		return ret;
	}

	mtk_atr_init(mtk_dev);

	/* interrupt re-init, in case this is MSIX interrupt with merged entries.*/
	mtk_pcie_interrupt_reinit(mtk_dev);

	/* Enable PCIe bus master */
	pci_set_master(mtk_dev->pdev);

	if (!pcie_only) {
		mhccif_init(mtk_dev);
		mtk_pci_pm_reinit(mtk_dev);
	}

	return 0;
}

static int __mtk_pci_pm_resume(struct pci_dev *pdev, bool state_check,
			       bool is_runtime)
{
	struct mtk_pci_dev *mtk_dev = pci_get_drvdata(pdev);
	struct md_pm_entity *entity;
	unsigned long wait_ret;
	unsigned int key = 255;
	u32 resume_reg_state;
	int ret = 0;

	pci_set_master(pdev);

	if (!mtk_dev->pm_enabled)
		return 0;

	if (atomic_read(&mtk_dev->md_pm_init_done) < 1)
		goto resume_complete;

	/* Get the previous state */
	resume_reg_state = ioread32(IREG_BASE(mtk_dev) + PCIE_DEBUG_DUMMY_3);

	if (state_check) {
		/* For D3/L3 resume, the device could boot so quickly that the
		 * initial value of the dummy register might be overwritten.
		 * We use atr_reg_val to help identify new boots.
		 * If it is not set by Host Driver, the value is 0x7F
		 */
		u32 atr_reg_val = ioread32(IREG_BASE(mtk_dev) +
					   ATR_PCIE_WIN0_T0_ATR_PARAM_SRC_ADDR_LSB);

		if (resume_reg_state == PM_RESUME_REG_STATE_L3 ||
		    (resume_reg_state == PM_RESUME_REG_STATE_INIT && atr_reg_val == 0x7f)) {
			ret = mtk_pci_event_handler(mtk_dev, PCIE_EVENT_L3ENTER);
			if (ret)
				return ret;
			ret = mtk_pcie_reinit(mtk_dev, false);
			if (ret)
				return ret;
			mtk_clear_rgu_irq(mtk_dev);
			return mtk_pci_event_handler(mtk_dev, PCIE_EVENT_L3EXIT);
		} else if (resume_reg_state == PM_RESUME_REG_STATE_EXP ||
			   resume_reg_state == PM_RESUME_REG_STATE_L2_EXP) {
			if (resume_reg_state == PM_RESUME_REG_STATE_L2_EXP) {
				ret = mtk_pcie_reinit(mtk_dev, true);
				if (ret)
					return ret;
			}
			atomic_dec(&mtk_dev->md_pm_init_done);
			atomic_set(&mtk_dev->rgu_pci_irq_en, 1);
			mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, false);

			mhccif_mask_clr(mtk_dev,
					D2H_INT_EXCEPTION_INIT |
					D2H_INT_EXCEPTION_INIT_DONE |
					D2H_INT_EXCEPTION_CLEARQ_DONE |
					D2H_INT_EXCEPTION_ALLQ_RESET |
					D2H_INT_PORT_ENUM);

			return ret;
		} else if (resume_reg_state == PM_RESUME_REG_STATE_L2) {
			ret = mtk_pcie_reinit(mtk_dev, true);
			if (ret)
				return ret;
		} else if (resume_reg_state != PM_RESUME_REG_STATE_L1 &&
			   resume_reg_state != PM_RESUME_REG_STATE_INIT) {
			ret = mtk_pci_event_handler(mtk_dev, PCIE_EVENT_L3ENTER);
			if (ret)
				return ret;

			mtk_clear_rgu_irq(mtk_dev);
			atomic_dec(&mtk_dev->md_pm_init_done);
			return 0;
		}
	}

	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->resume_early) {
			ret = entity->resume_early(mtk_dev, entity->entity_param);
			if (ret) {
				key = entity->key;
				dev_err(&pdev->dev,
					"[PM] Resume early Key %d ret %d\n",
					key, ret);
			}
		}
	}

	reinit_completion(&mtk_dev->pm_resume_ack);
	reinit_completion(&mtk_dev->pm_resume_ack_sap);

	mtk_pci_pm_counter(mtk_dev, PM_RESUME_HEADER);
	/* send d3 exit requests */
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_RESUME_REQ);
	mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_RESUME_REQ_AP);

	wait_ret = __mtk_pci_wait_resume_ack(mtk_dev);
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Wait for device resume ACK timeout\n");

	wait_ret = __mtk_pci_wait_resume_ack_sap(mtk_dev);
	if (!wait_ret)
		dev_err(&pdev->dev, "[PM] Wait for device resume ACK timeout-SAP\n");

	/* Each HW final restore works */
	list_for_each_entry(entity, &mtk_dev->md_pm_entities, entity) {
		if (entity->resume) {
			ret = entity->resume(mtk_dev, entity->entity_param);
			if (ret) {
				key = entity->key;
				dev_err(&pdev->dev,
					"[PM] Resume entry key %d ret %d\n",
					key, ret);
			}
		}
	}

	atomic_set(&mtk_dev->rgu_pci_irq_en, 1);
	mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, false);

	if (is_runtime)
		pm_runtime_mark_last_busy(&pdev->dev);

resume_complete:
	iowrite32(L1_DISABLE_BIT(0), IREG_BASE(mtk_dev) + DIS_ASPM_LOWPWR_CLR_0);
	atomic_inc(&mtk_dev->md_pm_resumed);
	return ret;
}

static void mtk_pci_shutdown(struct pci_dev *pdev)
{
	__mtk_pci_pm_suspend(pdev, false);
}

static int mtk_pcie_bar_init(struct mtk_pci_dev *mtk_dev)
{
	unsigned long res_0_start = pci_resource_start(mtk_dev->pdev, 0);
	unsigned long res_0_len = pci_resource_len(mtk_dev->pdev, 0);
	unsigned long res_2_start = pci_resource_start(mtk_dev->pdev, 2);
	unsigned long res_2_len = pci_resource_len(mtk_dev->pdev, 2);
	struct device *dev = &mtk_dev->pdev->dev;

	/* Allocate BAR0 memory */
	if (!devm_request_mem_region(dev, res_0_start, res_0_len,
				     dev->driver->name)) {
		dev_err(dev, "BAR0 already in use\n");
		return -EBUSY;
	}

	IREG_BASE(mtk_dev) = devm_ioremap(dev, res_0_start, res_0_len);
	if (!IREG_BASE(mtk_dev)) {
		dev_err(dev, "error mapping memory for BAR0\n");
		return -ENOMEM;
	}

	/* Allocate BAR2 memory */
	if (!devm_request_mem_region(dev, res_2_start, res_2_len,
				     dev->driver->name)) {
		dev_err(dev, "BAR2 already in use\n");
		return -EBUSY;
	}

	mtk_dev->base_addr.pcie_ext_reg_base = devm_ioremap(dev,
							    res_2_start, res_2_len);
	if (!mtk_dev->base_addr.pcie_ext_reg_base) {
		dev_err(dev, "error mapping memory for BAR2\n");
		return -ENOMEM;
	}

	return 0;
}

static int mtk_pci_pm_suspend(struct device *dev)
{
	return __mtk_pci_pm_suspend(to_pci_dev(dev), false);
}

static int mtk_pci_pm_resume(struct device *dev)
{
	return __mtk_pci_pm_resume(to_pci_dev(dev), true, false);
}

static int mtk_pci_pm_thaw(struct device *dev)
{
	return __mtk_pci_pm_resume(to_pci_dev(dev), false, false);
}

static int mtk_pci_pm_runtime_suspend(struct device *dev)
{
	return __mtk_pci_pm_suspend(to_pci_dev(dev), true);
}

static int mtk_pci_pm_runtime_resume(struct device *dev)
{
	return __mtk_pci_pm_resume(to_pci_dev(dev), true, true);
}

static int mtk_pci_pm_runtime_idle(struct device *dev)
{
	pm_schedule_suspend(dev, RT_IDLE_DELAY * MSEC_PER_SEC);

	return -EBUSY;
}

static const struct dev_pm_ops mtk_pci_pm_ops = {
	.suspend = mtk_pci_pm_suspend,
	.resume = mtk_pci_pm_resume,
	.freeze = mtk_pci_pm_suspend,
	.thaw = mtk_pci_pm_thaw,
	.poweroff = mtk_pci_pm_suspend,
	.restore = mtk_pci_pm_resume,
	SET_RUNTIME_PM_OPS(mtk_pci_pm_runtime_suspend, mtk_pci_pm_runtime_resume,
			   mtk_pci_pm_runtime_idle)
};

static inline void mtk_pci_infracfg_ao_calc(struct mtk_pci_dev *mtk_dev)
{
	mtk_dev->base_addr.infracfg_ao_base = mtk_dev->base_addr.pcie_ext_reg_base +
					      INFRACFG_AO_DEV_CHIP -
					      mtk_dev->base_addr.pcie_dev_reg_trsl_addr;
}

static void mtk_pci_bridge_configure(struct mtk_pci_dev *mtk_dev, struct pci_dev *pdev)
{
	struct pci_dev *bridge;
	int dpc_cap;
	u16 ctl;

	/* check hotplug capability */
	bridge = pci_upstream_bridge(pdev);
	if (!bridge) {
		dev_err(&pdev->dev, "Bridge is not found\n");
		return;
	}

	/* disable dpc capability */
	dpc_cap = pci_find_ext_capability(bridge, PCI_EXT_CAP_ID_DPC);
	if (dpc_cap) {
		pci_read_config_word(bridge, dpc_cap + PCI_EXP_DPC_CTL, &ctl);
		ctl &= ~(PCI_EXP_DPC_CTL_INT_EN | 0x3);
		pci_write_config_word(bridge, dpc_cap + PCI_EXP_DPC_CTL, ctl);
	}
}

static void mtk_pci_configure_ltr(struct pci_dev *pdev)
{
#ifdef CONFIG_PCIEASPM
	struct pci_dev *bridge;
	u32 cap, ctl;

	if (!pci_is_pcie(pdev))
		return;

	pcie_capability_read_dword(pdev, PCI_EXP_DEVCAP2, &cap);
	if (!(cap & PCI_EXP_DEVCAP2_LTR))
		return;

	bridge = pci_upstream_bridge(pdev);
	if (!bridge) {
		dev_err(&pdev->dev, "Bridge is not found\n");
		return;
	}
	/* re-configure bridge ltr to satisfy remove-rescan case */
	if (bridge->ltr_path == 1) {
		pcie_capability_read_dword(bridge, PCI_EXP_DEVCTL2, &ctl);
		if (!(ctl & PCI_EXP_DEVCTL2_LTR_EN))
			pcie_capability_set_word(bridge, PCI_EXP_DEVCTL2,
						 PCI_EXP_DEVCTL2_LTR_EN);
	}

	/* re-configure device ltr to satisfy remove-rescan case */
	if (pdev->ltr_path == 1) {
		pcie_capability_clear_word(pdev, PCI_EXP_DEVCTL2,
					   PCI_EXP_DEVCTL2_LTR_EN);
		pcie_capability_set_word(pdev, PCI_EXP_DEVCTL2,
					 PCI_EXP_DEVCTL2_LTR_EN);
	}
#endif
}

static int mtk_request_irq(struct pci_dev *pdev, int irq_count)
{
	struct mtk_pci_dev *mtk_dev = pci_get_drvdata(pdev);
	int ret;
	int i;

	mtk_dev->msix_res = pci_isr_alloc_table(mtk_dev, irq_count);
	if (!mtk_dev->msix_res)
		return -ENOMEM;

	for (i = 0; i < irq_count; i++) {
		mtk_dev->msix_res[i].mtk_dev = mtk_dev;

		mtk_dev->msix_res[i].irq = pci_irq_vector(pdev, i);
		sprintf(mtk_dev->msix_res[i].irq_descr,
			"%s_%d", pdev->driver->name, i);
		ret = devm_request_irq(&pdev->dev, pci_irq_vector(pdev, i),
				       mtk_dev->msix_res[i].isr,
				       IRQF_SHARED,
				       mtk_dev->msix_res[i].irq_descr,
				       &mtk_dev->msix_res[i]);

		if (ret) {
			dev_err(&pdev->dev, "failed to request_irq %d: %d\n",
				pci_irq_vector(pdev, i), ret);
			return ret;
		}
	}

	mtk_dev->irq_count = irq_count;

	return 0;
}

static void mtk_free_irq(struct pci_dev *pdev)
{
	struct mtk_pci_dev *mtk_dev = pci_get_drvdata(pdev);
	int i;

	for (i = 0; i < mtk_dev->irq_count; i++) {
		if (mtk_dev->msix_res[i].isr) {
			mtk_dev->msix_res[i].irq = 0;
			devm_free_irq(&pdev->dev,
				      pci_irq_vector(pdev, i),
				      &mtk_dev->msix_res[i]);
		}
	}
	pci_free_irq_vectors(mtk_dev->pdev);

	mtk_dev->irq_count = 0;
}

static int mtk_setup_msix(struct mtk_pci_dev *mtk_dev)
{
	int msix_count = MTK_IRQ_COUNT;
	int ret;

	do {
		ret = pci_alloc_irq_vectors(mtk_dev->pdev,
					    MTK_IRQ_MSIX_MIN_COUNT, msix_count, PCI_IRQ_MSIX);
		if (ret <= 0) {
			dev_err(&mtk_dev->pdev->dev,
				"failed to allocate MSI-X entry, errno %d\n", ret);
			return -ENOMEM;
		}
		if (ret == msix_count)
			break;

		/* Got less irq vectors than requested */
		pci_free_irq_vectors(mtk_dev->pdev);
		if (msix_count == 2) {
			dev_err(&mtk_dev->pdev->dev,
				"Could not allocate enough MSI-X entries\n");
			return -ENOMEM;
		}
		msix_count = BIT(fls(ret) - 1);
	} while (1);

	ret = mtk_request_irq(mtk_dev->pdev, msix_count);
	if (ret) {
		pci_free_irq_vectors(mtk_dev->pdev);
		return ret;
	}

	/* Set MSIX merge config */
	mtk_msix_ctrl_cfg(IREG_BASE(mtk_dev), msix_count);

	return 0;
}

static int mtk_interrupt_init(struct mtk_pci_dev *mtk_dev)
{
	char strwqbdf[128];
	int i;

	sprintf(strwqbdf, MTK_PCI_WQ_DESCR "(bdf:%d-%x-%x)", mtk_dev->pdev->bus->number,
		(mtk_dev->pdev->devfn) >> 12, (mtk_dev->pdev->devfn) & 0xfff);

	mtk_dev->pcie_isr_wq = create_singlethread_workqueue(strwqbdf);
	if (!mtk_dev->pcie_isr_wq) {
		dev_err(&mtk_dev->pdev->dev, "Failed to create workqueue: %s\n", strwqbdf);
		return -EINVAL;
	}

	INIT_WORK(&mtk_dev->service_task, pcie_isr_service_task);

	if (mtk_dev->pdev->msix_cap && (mtk_setup_msix(mtk_dev) == 0)) {
		/* let the IPs enable interrupts when they are ready */
		for (i = EXT_INT_START; i < EXT_INT_START + END_EXT_INT; i++)
			PCIE_MSIX_MSK_SET(IREG_BASE(mtk_dev), i);
		return 0;
	}

	return -1;
}

static int mtk_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mtk_pci_dev *mtk_dev;
	int ret;

	mtk_dev = devm_kzalloc(&pdev->dev, sizeof(*mtk_dev), GFP_KERNEL);
	if (!mtk_dev)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, mtk_dev);
	mtk_pci_bridge_configure(mtk_dev, pdev);
	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	if (!pdev->irq) {
		dev_err(&pdev->dev, "No irq\n");
		return -EINVAL;
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(&pdev->dev, "Set DMA mask error %d\n", ret);
		return ret;
	}

	ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(&pdev->dev, "Set Coherent DMA mask error %d\n", ret);
		return ret;
	}

	pdev->dma_mask = DMA_BIT_MASK(64);
	mtk_dev->pdev = pdev;

	ret = mtk_pcie_bar_init(mtk_dev);
	if (ret)
		return ret;

	mtk_pci_pm_init(mtk_dev);
	mtk_dev->pm_enabled = 1;

	/* Disable the interrupt to avoid unexpected interrupts */
	mtk_enable_disable_int(IREG_BASE(mtk_dev), false);

	ret = mtk_interrupt_init(mtk_dev);
	if (ret)
		return ret;

	/* Address translation table configuration */
	mtk_atr_init(mtk_dev);
	/* Initial infra ao base addr */
	mtk_pci_infracfg_ao_calc(mtk_dev);
	/* Initialize mhccif */
	mhccif_init(mtk_dev);

	ret = ccci_modem_init(mtk_dev);
	if (ret) {
		dev_err(&pdev->dev, "register ccci device fail\n");
		goto err_irq;
	}

	/* MSIX should enable mhccif */
	mtk_clear_set_int_type(mtk_dev, MHCCIF_INT, false);
	/* Enable interrupt */
	mtk_enable_disable_int(IREG_BASE(mtk_dev), true);
	/* Enable PCIe bus master */
	pci_set_master(pdev);

	mtk_pci_configure_ltr(pdev);
	return 0;

err_irq:
	mtk_free_irq(pdev);
	destroy_workqueue(mtk_dev->pcie_isr_wq);
	return ret;
}

static void mtk_pci_remove(struct pci_dev *pdev)
{
	struct mtk_pci_dev *mtk_dev = pci_get_drvdata(pdev);

	ccci_modem_exit(mtk_dev);
	mtk_pci_pm_deinit(mtk_dev);
	mtk_free_irq(pdev);
	cancel_work_sync(&mtk_dev->service_task);
	destroy_workqueue(mtk_dev->pcie_isr_wq);
	mtk_dev->pcie_isr_wq = NULL;
	pci_disable_device(pdev);
}

static struct pci_driver mtk_pci_driver = {
	.name = "mtk_t7xx",
	.id_table = mtk_dev_ids,
	.probe = mtk_pci_probe,
	.remove = mtk_pci_remove,
	.driver.pm = &mtk_pci_pm_ops,
	.shutdown = mtk_pci_shutdown,
};

static int __init mtk_pci_init(void)
{
	int retval = ccci_init();

	if (retval) {
		pr_err("Failed to ccci init\n");
		return retval;
	}

	retval = pci_register_driver(&mtk_pci_driver);
	if (retval) {
		pr_err("Failed to register pci driver\n");
		ccci_uninit();
	}

	return retval;
}
module_init(mtk_pci_init);

static void __exit mtk_pci_cleanup(void)
{
	pci_unregister_driver(&mtk_pci_driver);
	ccci_uninit();
}
module_exit(mtk_pci_cleanup);

MODULE_AUTHOR("MediaTek Inc");
MODULE_DESCRIPTION("MediaTek PCIe 5G WWAN modem T7XX driver");
MODULE_LICENSE("GPL v2");
