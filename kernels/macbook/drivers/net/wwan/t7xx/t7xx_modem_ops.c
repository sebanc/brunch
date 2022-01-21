// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>

#include "t7xx_hif_cldma.h"
#include "t7xx_mhccif.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_netdev.h"
#include "t7xx_pci.h"
#include "t7xx_pcie_mac.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"

#define RGU_RESET_DELAY_US	20
#define PORT_RESET_DELAY_US	2000

enum mtk_feature_support_type {
	MTK_FEATURE_DOES_NOT_EXIST,
	MTK_FEATURE_NOT_SUPPORTED,
	MTK_FEATURE_MUST_BE_SUPPORTED,
};

static inline unsigned int get_interrupt_status(struct mtk_pci_dev *mtk_dev)
{
	return mhccif_read_sw_int_sts(mtk_dev) & D2H_SW_INT_MASK;
}

/**
 * mtk_pci_mhccif_isr() - Process MHCCIF interrupts
 * @mtk_dev: MTK device
 *
 * Check the interrupt status, and queue commands accordingly
 *
 * Returns: 0 on success or -EINVAL on failure
 */
int mtk_pci_mhccif_isr(struct mtk_pci_dev *mtk_dev)
{
	struct md_sys_info *md_info;
	struct ccci_fsm_ctl *ctl;
	struct mtk_modem *md;
	unsigned int int_sta;
	unsigned long flags;
	u32 mask;

	md = mtk_dev->md;
	ctl = fsm_get_entry();
	if (!ctl) {
		dev_err(&mtk_dev->pdev->dev,
			"process MHCCIF interrupt before modem monitor was initialized\n");
		return -EINVAL;
	}

	md_info = md->md_info;
	spin_lock_irqsave(&md_info->exp_spinlock, flags);
	int_sta = get_interrupt_status(mtk_dev);
	md_info->exp_id |= int_sta;

	if (md_info->exp_id & D2H_INT_PORT_ENUM) {
		md_info->exp_id &= ~D2H_INT_PORT_ENUM;
		if (ctl->curr_state == CCCI_FSM_INIT ||
		    ctl->curr_state == CCCI_FSM_PRE_START ||
		    ctl->curr_state == CCCI_FSM_STOPPED)
			ccci_fsm_recv_md_interrupt(MD_IRQ_PORT_ENUM);
	}

	if (md_info->exp_id & D2H_INT_EXCEPTION_INIT) {
		if (ctl->md_state == MD_STATE_INVALID ||
		    ctl->md_state == MD_STATE_WAITING_FOR_HS1 ||
		    ctl->md_state == MD_STATE_WAITING_FOR_HS2 ||
		    ctl->md_state == MD_STATE_READY) {
			md_info->exp_id &= ~D2H_INT_EXCEPTION_INIT;
			ccci_fsm_recv_md_interrupt(MD_IRQ_CCIF_EX);
		}
	} else if (ctl->md_state == MD_STATE_WAITING_FOR_HS1) {
		/* start handshake if MD not assert */
		mask = mhccif_mask_get(mtk_dev);
		if ((md_info->exp_id & D2H_INT_ASYNC_MD_HK) && !(mask & D2H_INT_ASYNC_MD_HK)) {
			md_info->exp_id &= ~D2H_INT_ASYNC_MD_HK;
			queue_work(md->handshake_wq, &md->handshake_work);
		}
	}

	spin_unlock_irqrestore(&md_info->exp_spinlock, flags);

	return 0;
}

static void clr_device_irq_via_pcie(struct mtk_pci_dev *mtk_dev)
{
	struct mtk_addr_base *pbase_addr;
	void __iomem *rgu_pciesta_reg;

	pbase_addr = &mtk_dev->base_addr;
	rgu_pciesta_reg = pbase_addr->pcie_ext_reg_base + TOPRGU_CH_PCIE_IRQ_STA -
			  pbase_addr->pcie_dev_reg_trsl_addr;

	/* clear RGU PCIe IRQ state */
	iowrite32(ioread32(rgu_pciesta_reg), rgu_pciesta_reg);
}

void mtk_clear_rgu_irq(struct mtk_pci_dev *mtk_dev)
{
	/* clear L2 */
	clr_device_irq_via_pcie(mtk_dev);
	/* clear L1 */
	mtk_pcie_mac_clear_int_status(mtk_dev, SAP_RGU_INT);
}

static int mtk_acpi_reset(struct mtk_pci_dev *mtk_dev, char *fn_name)
{
#ifdef CONFIG_ACPI
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status acpi_ret;
	struct device *dev;
	acpi_handle handle;

	dev = &mtk_dev->pdev->dev;

	if (acpi_disabled) {
		dev_err(dev, "acpi function isn't enabled\n");
		return -EFAULT;
	}

	handle = ACPI_HANDLE(dev);
	if (!handle) {
		dev_err(dev, "acpi handle isn't found\n");
		return -EFAULT;
	}

	if (!acpi_has_method(handle, fn_name)) {
		dev_err(dev, "%s method isn't found\n", fn_name);
		return -EFAULT;
	}

	acpi_ret = acpi_evaluate_object(handle, fn_name, NULL, &buffer);
	if (ACPI_FAILURE(acpi_ret)) {
		dev_err(dev, "%s method fail: %s\n", fn_name, acpi_format_exception(acpi_ret));
		return -EFAULT;
	}
#endif
	return 0;
}

int mtk_acpi_fldr_func(struct mtk_pci_dev *mtk_dev)
{
	return mtk_acpi_reset(mtk_dev, "._RST");
}

static void reset_device_via_pmic(struct mtk_pci_dev *mtk_dev)
{
	unsigned int val;

	val = ioread32(IREG_BASE(mtk_dev) + PCIE_MISC_DEV_STATUS);

	if (val & MISC_RESET_TYPE_PLDR)
		mtk_acpi_reset(mtk_dev, "MRST._RST");
	else if (val & MISC_RESET_TYPE_FLDR)
		mtk_acpi_fldr_func(mtk_dev);
}

static irqreturn_t rgu_isr_thread(int irq, void *data)
{
	struct mtk_pci_dev *mtk_dev;
	struct mtk_modem *modem;

	mtk_dev = data;
	modem = mtk_dev->md;

	msleep(RGU_RESET_DELAY_US);
	reset_device_via_pmic(modem->mtk_dev);
	return IRQ_HANDLED;
}

static irqreturn_t rgu_isr_handler(int irq, void *data)
{
	struct mtk_pci_dev *mtk_dev;
	struct mtk_modem *modem;

	mtk_dev = data;
	modem = mtk_dev->md;

	mtk_clear_rgu_irq(mtk_dev);

	if (!mtk_dev->rgu_pci_irq_en)
		return IRQ_HANDLED;

	atomic_set(&modem->rgu_irq_asserted, 1);
	mtk_pcie_mac_clear_int(mtk_dev, SAP_RGU_INT);
	return IRQ_WAKE_THREAD;
}

static void mtk_pcie_register_rgu_isr(struct mtk_pci_dev *mtk_dev)
{
	/* registers RGU callback isr with PCIe driver */
	mtk_pcie_mac_clear_int(mtk_dev, SAP_RGU_INT);
	mtk_pcie_mac_clear_int_status(mtk_dev, SAP_RGU_INT);

	mtk_dev->intr_handler[SAP_RGU_INT] = rgu_isr_handler;
	mtk_dev->intr_thread[SAP_RGU_INT] = rgu_isr_thread;
	mtk_dev->callback_param[SAP_RGU_INT] = mtk_dev;
	mtk_pcie_mac_set_int(mtk_dev, SAP_RGU_INT);
}

static void md_exception(struct mtk_modem *md, enum hif_ex_stage stage)
{
	struct mtk_pci_dev *mtk_dev;

	mtk_dev = md->mtk_dev;

	if (stage == HIF_EX_CLEARQ_DONE) {
		/* give DHL time to flush data.
		 * this is an empirical value that assure
		 * that DHL have enough time to flush all the date.
		 */
		msleep(PORT_RESET_DELAY_US);
		port_proxy_reset(&mtk_dev->pdev->dev);
	}

	cldma_exception(ID_CLDMA1, stage);

	if (stage == HIF_EX_INIT)
		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_EXCEPTION_ACK);
	else if (stage == HIF_EX_CLEARQ_DONE)
		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_EXCEPTION_CLEARQ_ACK);
}

static int wait_hif_ex_hk_event(struct mtk_modem *md, int event_id)
{
	struct md_sys_info *md_info;
	int sleep_time = 10;
	int retries = 500; /* MD timeout is 5s */

	md_info = md->md_info;
	do {
		if (md_info->exp_id & event_id)
			return 0;

		msleep(sleep_time);
	} while (--retries);

	return -EFAULT;
}

static void md_sys_sw_init(struct mtk_pci_dev *mtk_dev)
{
	/* Register the MHCCIF isr for MD exception, port enum and
	 * async handshake notifications.
	 */
	mhccif_mask_set(mtk_dev, D2H_SW_INT_MASK);
	mtk_dev->mhccif_bitmask = D2H_SW_INT_MASK;
	mhccif_mask_clr(mtk_dev, D2H_INT_PORT_ENUM);

	/* register RGU irq handler for sAP exception notification */
	mtk_dev->rgu_pci_irq_en = true;
	mtk_pcie_register_rgu_isr(mtk_dev);
}

struct feature_query {
	u32 head_pattern;
	u8 feature_set[FEATURE_COUNT];
	u32 tail_pattern;
};

static void prepare_host_rt_data_query(struct core_sys_info *core)
{
	struct ctrl_msg_header *ctrl_msg_h;
	struct feature_query *ft_query;
	struct ccci_header *ccci_h;
	struct sk_buff *skb;
	size_t packet_size;

	packet_size = sizeof(struct ccci_header) +
		      sizeof(struct ctrl_msg_header) +
		      sizeof(struct feature_query);
	skb = ccci_alloc_skb(packet_size, GFS_BLOCKING);
	if (!skb)
		return;

	skb_put(skb, packet_size);
	/* fill CCCI header */
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->data[0] = 0;
	ccci_h->data[1] = packet_size;
	ccci_h->status &= ~HDR_FLD_CHN;
	ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, core->ctl_port->tx_ch);
	ccci_h->status &= ~HDR_FLD_SEQ;
	ccci_h->reserved = 0;
	/* fill control message */
	ctrl_msg_h = (struct ctrl_msg_header *)(skb->data +
						sizeof(struct ccci_header));
	ctrl_msg_h->ctrl_msg_id = CTL_ID_HS1_MSG;
	ctrl_msg_h->reserved = 0;
	ctrl_msg_h->data_length = sizeof(struct feature_query);
	/* fill feature query */
	ft_query = (struct feature_query *)(skb->data +
					    sizeof(struct ccci_header) +
					    sizeof(struct ctrl_msg_header));
	ft_query->head_pattern = MD_FEATURE_QUERY_ID;
	memcpy(ft_query->feature_set, core->feature_set, FEATURE_COUNT);
	ft_query->tail_pattern = MD_FEATURE_QUERY_ID;
	/* send HS1 message to device */
	port_proxy_send_skb(core->ctl_port, skb, 0);
}

static int prepare_device_rt_data(struct core_sys_info *core, struct device *dev,
				  void *data, int data_length)
{
	struct mtk_runtime_feature rt_feature;
	struct ctrl_msg_header *ctrl_msg_h;
	struct feature_query *md_feature;
	struct ccci_header *ccci_h;
	struct sk_buff *skb;
	int packet_size = 0;
	char *rt_data;
	int i;

	skb = ccci_alloc_skb(MTK_SKB_4K, GFS_BLOCKING);
	if (!skb)
		return -EFAULT;

	/* fill CCCI header */
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->data[0] = 0;
	ccci_h->status &= ~HDR_FLD_CHN;
	ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, core->ctl_port->tx_ch);
	ccci_h->status &= ~HDR_FLD_SEQ;
	ccci_h->reserved = 0;
	/* fill control message header */
	ctrl_msg_h = (struct ctrl_msg_header *)(skb->data + sizeof(struct ccci_header));
	ctrl_msg_h->ctrl_msg_id = CTL_ID_HS3_MSG;
	ctrl_msg_h->reserved = 0;
	rt_data = (skb->data + sizeof(struct ccci_header) + sizeof(struct ctrl_msg_header));

	/* parse MD runtime data query */
	md_feature = data;
	if (md_feature->head_pattern != MD_FEATURE_QUERY_ID ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_ID) {
		dev_err(dev, "md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			md_feature->head_pattern, md_feature->tail_pattern);
		return -EINVAL;
	}

	/* fill runtime feature */
	for (i = 0; i < FEATURE_COUNT; i++) {
		u8 md_feature_mask = FIELD_GET(FEATURE_MSK, md_feature->feature_set[i]);

		memset(&rt_feature, 0, sizeof(rt_feature));
		rt_feature.feature_id = i;
		switch (md_feature_mask) {
		case MTK_FEATURE_DOES_NOT_EXIST:
		case MTK_FEATURE_MUST_BE_SUPPORTED:
			rt_feature.support_info = md_feature->feature_set[i];
			break;

		default:
			break;
		}

		if (FIELD_GET(FEATURE_MSK, rt_feature.support_info) !=
		    MTK_FEATURE_MUST_BE_SUPPORTED) {
			rt_feature.data_len = 0;
			memcpy(rt_data, &rt_feature, sizeof(struct mtk_runtime_feature));
			rt_data += sizeof(struct mtk_runtime_feature);
		}

		packet_size += (sizeof(struct mtk_runtime_feature) + rt_feature.data_len);
	}

	ctrl_msg_h->data_length = packet_size;
	ccci_h->data[1] = packet_size + sizeof(struct ctrl_msg_header) +
			  sizeof(struct ccci_header);
	skb_put(skb, ccci_h->data[1]);
	/* send HS3 message to device */
	port_proxy_send_skb(core->ctl_port, skb, 0);
	return 0;
}

static int parse_host_rt_data(struct core_sys_info *core, struct device *dev,
			      void *data, int data_length)
{
	enum mtk_feature_support_type ft_spt_st, ft_spt_cfg;
	struct mtk_runtime_feature *rt_feature;
	int i, offset;

	offset = sizeof(struct feature_query);
	for (i = 0; i < FEATURE_COUNT && offset < data_length; i++) {
		rt_feature = (struct mtk_runtime_feature *)(data + offset);
		ft_spt_st = FIELD_GET(FEATURE_MSK, rt_feature->support_info);
		ft_spt_cfg = FIELD_GET(FEATURE_MSK, core->feature_set[i]);
		offset += sizeof(struct mtk_runtime_feature) + rt_feature->data_len;

		if (ft_spt_cfg == MTK_FEATURE_MUST_BE_SUPPORTED) {
			if (ft_spt_st != MTK_FEATURE_MUST_BE_SUPPORTED) {
				dev_err(dev, "mismatch: runtime feature%d (%d,%d)\n",
					i, ft_spt_cfg, ft_spt_st);
				return -EINVAL;
			}

			if ((i == RT_ID_MD_PORT_ENUM) || (i == RT_ID_SAP_PORT_ENUM))
				port_proxy_node_control(dev, (struct port_msg *)rt_feature->data);
		}
	}

	return 0;
}

static void core_reset(struct mtk_modem *md)
{
	struct ccci_fsm_ctl *fsm_ctl;

	atomic_set(&md->core_md.ready, 0);
	fsm_ctl = fsm_get_entry();

	if (!fsm_ctl) {
		dev_err(&md->mtk_dev->pdev->dev, "fsm ctl is not initialized\n");
		return;
	}

	/* append HS2_EXIT event to cancel the ongoing handshake in core_hk_handler() */
	if (atomic_read(&md->core_md.handshake_ongoing))
		fsm_append_event(fsm_ctl, CCCI_EVENT_MD_HS2_EXIT, NULL, 0);

	atomic_set(&md->core_md.handshake_ongoing, 0);
}

static void core_hk_handler(struct mtk_modem *md, struct ccci_fsm_ctl *ctl,
			    enum ccci_fsm_event_state event_id,
			    enum ccci_fsm_event_state err_detect)
{
	struct core_sys_info *core_info;
	struct ccci_fsm_event *event, *event_next;
	unsigned long flags;
	struct device *dev;
	int ret;

	core_info = &md->core_md;
	dev = &md->mtk_dev->pdev->dev;
	prepare_host_rt_data_query(core_info);
	while (!kthread_should_stop()) {
		bool event_received = false;

		spin_lock_irqsave(&ctl->event_lock, flags);
		list_for_each_entry_safe(event, event_next, &ctl->event_queue, entry) {
			if (event->event_id == err_detect) {
				list_del(&event->entry);
				spin_unlock_irqrestore(&ctl->event_lock, flags);
				dev_err(dev, "core handshake error event received\n");
				goto exit;
			}

			if (event->event_id == event_id) {
				list_del(&event->entry);
				event_received = true;
				break;
			}
		}

		spin_unlock_irqrestore(&ctl->event_lock, flags);

		if (event_received)
			break;

		wait_event_interruptible(ctl->event_wq, !list_empty(&ctl->event_queue) ||
					 kthread_should_stop());
		if (kthread_should_stop())
			goto exit;
	}

	if (atomic_read(&ctl->exp_flg))
		goto exit;

	ret = parse_host_rt_data(core_info, dev, event->data, event->length);
	if (ret) {
		dev_err(dev, "host runtime data parsing fail:%d\n", ret);
		goto exit;
	}

	if (atomic_read(&ctl->exp_flg))
		goto exit;

	ret = prepare_device_rt_data(core_info, dev, event->data, event->length);
	if (ret) {
		dev_err(dev, "device runtime data parsing fail:%d", ret);
		goto exit;
	}

	atomic_set(&core_info->ready, 1);
	atomic_set(&core_info->handshake_ongoing, 0);
	wake_up(&ctl->async_hk_wq);
exit:
	kfree(event);
}

static void md_hk_wq(struct work_struct *work)
{
	struct ccci_fsm_ctl *ctl;
	struct mtk_modem *md;

	ctl = fsm_get_entry();

	/* clear the HS2 EXIT event appended in core_reset() */
	fsm_clear_event(ctl, CCCI_EVENT_MD_HS2_EXIT);
	cldma_switch_cfg(ID_CLDMA1, HIF_CFG1);
	cldma_start(ID_CLDMA1);
	fsm_broadcast_state(ctl, MD_STATE_WAITING_FOR_HS2);
	md = container_of(work, struct mtk_modem, handshake_work);
	atomic_set(&md->core_md.handshake_ongoing, 1);
	core_hk_handler(md, ctl, CCCI_EVENT_MD_HS2, CCCI_EVENT_MD_HS2_EXIT);
}

static void core_sap_handler(struct mtk_modem *md,
			    struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id,
			    enum ccci_fsm_event_state err_detect)
{
	struct core_sys_info *core_info = &md->core_sap;
	struct device *dev = &md->mtk_dev->pdev->dev;
	struct ccci_fsm_event *event;
	unsigned long flags;
	int ret;

	prepare_host_rt_data_query(core_info);

	while (1) {
		spin_lock_irqsave(&ctl->event_lock, flags);
		if (list_empty(&ctl->event_queue)) {
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			wait_event_interruptible(ctl->event_wq,
						 !list_empty(&ctl->event_queue));
			continue;
		}

		event = list_first_entry(&ctl->event_queue,
					 struct ccci_fsm_event, entry);

		if (event->event_id == err_detect) {
			list_del(&event->entry);
			dev_err(dev, "core handshake 2 exit\n");
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			goto exit;
		} else if (event->event_id == event_id) {
			break;
		}

		spin_unlock_irqrestore(&ctl->event_lock, flags);
	}

	list_del(&event->entry);
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	if (atomic_read(&ctl->exp_flg))
		goto exit;

	ret = parse_host_rt_data(core_info, dev, event->data, event->length);
	if (ret) {
		dev_err(dev, "core handshake 2 fail for the SAP :%d\n", ret);
		goto exit;
	}
	if (atomic_read(&ctl->exp_flg))
		goto exit;

	ret = prepare_device_rt_data(core_info, dev, event->data, event->length);
	if (ret) {
		dev_err(dev, "core: handshake 3 fail:%d", ret);
		goto exit;
	}

	atomic_set(&core_info->ready, 1);
	atomic_set(&core_info->handshake_ongoing, 0);
	wake_up(&ctl->async_hk_wq);
exit:
	kfree(event);
}

static void sap_hk_wq(struct work_struct *work)
{
	struct mtk_modem *md = container_of(work, struct mtk_modem, sap_handshake_work);
        struct core_sys_info *core_info;
        struct ccci_fsm_ctl *ctl = fsm_get_entry ();

	core_info = &(md->core_sap);
        fsm_clear_event(ctl, CCCI_EVENT_AP_HS2_EXIT);
	cldma_switch_cfg(ID_CLDMA0, HIF_CFG_DEF);
	cldma_start(ID_CLDMA0);
        atomic_set(&core_info->handshake_ongoing, 1);
        core_sap_handler(md, ctl, CCCI_EVENT_SAP_HS2, CCCI_EVENT_AP_HS2_EXIT);
}


void mtk_md_event_notify(struct mtk_modem *md, enum md_event_id evt_id)
{
	struct md_sys_info *md_info;
	void __iomem *mhccif_base;
	struct ccci_fsm_ctl *ctl;
	unsigned int int_sta;
	unsigned long flags;

	ctl = fsm_get_entry();
	md_info = md->md_info;

	switch (evt_id) {
	case FSM_PRE_START:
		mhccif_mask_clr(md->mtk_dev, D2H_INT_PORT_ENUM);
		break;

	case FSM_START:
		mhccif_mask_set(md->mtk_dev, D2H_INT_PORT_ENUM);
		spin_lock_irqsave(&md_info->exp_spinlock, flags);
		int_sta = get_interrupt_status(md->mtk_dev);
		md_info->exp_id |= int_sta;
		if (md_info->exp_id & D2H_INT_EXCEPTION_INIT) {
			atomic_set(&ctl->exp_flg, 1);
			md_info->exp_id &= ~D2H_INT_EXCEPTION_INIT;
			md_info->exp_id &= ~D2H_INT_ASYNC_MD_HK;
		} else if (atomic_read(&ctl->exp_flg)) {
			md_info->exp_id &= ~D2H_INT_ASYNC_MD_HK;
		} else if (md_info->exp_id & D2H_INT_ASYNC_MD_HK) {
			queue_work(md->handshake_wq, &md->handshake_work);
			md_info->exp_id &= ~D2H_INT_ASYNC_MD_HK;
			mhccif_base = md->mtk_dev->base_addr.mhccif_rc_base;
			iowrite32(D2H_INT_ASYNC_MD_HK, mhccif_base + REG_EP2RC_SW_INT_ACK);
			mhccif_mask_set(md->mtk_dev, D2H_INT_ASYNC_MD_HK);
		} else {
			/* unmask async handshake interrupt */
			mhccif_mask_clr(md->mtk_dev, D2H_INT_ASYNC_MD_HK);
		}
		
		if (md_info->exp_id & D2H_INT_ASYNC_SAP_HK) {
			queue_work(md->sap_handshake_wq, &md->sap_handshake_work);
			md_info->exp_id &= ~D2H_INT_ASYNC_SAP_HK;
			mhccif_base = md->mtk_dev->base_addr.mhccif_rc_base;
			iowrite32(D2H_INT_ASYNC_SAP_HK,
				  mhccif_base + REG_EP2RC_SW_INT_ACK);
			mhccif_mask_set(md->mtk_dev, D2H_INT_ASYNC_SAP_HK);
		} else {
			/* unmask async handshake interrupt */
			mhccif_mask_clr(md->mtk_dev, D2H_INT_ASYNC_MD_HK);
			mhccif_mask_clr(md->mtk_dev, D2H_INT_ASYNC_SAP_HK);
		}

		spin_unlock_irqrestore(&md_info->exp_spinlock, flags);
		/* unmask exception interrupt */
		mhccif_mask_clr(md->mtk_dev,
				D2H_INT_EXCEPTION_INIT |
				D2H_INT_EXCEPTION_INIT_DONE |
				D2H_INT_EXCEPTION_CLEARQ_DONE |
				D2H_INT_EXCEPTION_ALLQ_RESET);
		break;

	case FSM_READY:
		/* mask async handshake interrupt */
		mhccif_mask_set(md->mtk_dev, D2H_INT_ASYNC_MD_HK);
		mhccif_mask_set(md->mtk_dev, D2H_INT_ASYNC_SAP_HK);
		break;

	default:
		break;
	}
}

static void md_structure_reset(struct mtk_modem *md)
{
	struct md_sys_info *md_info;

	md_info = md->md_info;
	md_info->exp_id = 0;
	spin_lock_init(&md_info->exp_spinlock);
}

void mtk_md_exception_handshake(struct mtk_modem *md)
{
	struct mtk_pci_dev *mtk_dev;
	int ret;

	mtk_dev = md->mtk_dev;
	md_exception(md, HIF_EX_INIT);
	ret = wait_hif_ex_hk_event(md, D2H_INT_EXCEPTION_INIT_DONE);

	if (ret)
		dev_err(&mtk_dev->pdev->dev, "EX CCIF HS timeout, RCH 0x%lx\n",
			D2H_INT_EXCEPTION_INIT_DONE);

	md_exception(md, HIF_EX_INIT_DONE);
	ret = wait_hif_ex_hk_event(md, D2H_INT_EXCEPTION_CLEARQ_DONE);
	if (ret)
		dev_err(&mtk_dev->pdev->dev, "EX CCIF HS timeout, RCH 0x%lx\n",
			D2H_INT_EXCEPTION_CLEARQ_DONE);

	md_exception(md, HIF_EX_CLEARQ_DONE);
	ret = wait_hif_ex_hk_event(md, D2H_INT_EXCEPTION_ALLQ_RESET);
	if (ret)
		dev_err(&mtk_dev->pdev->dev, "EX CCIF HS timeout, RCH 0x%lx\n",
			D2H_INT_EXCEPTION_ALLQ_RESET);

	md_exception(md, HIF_EX_ALLQ_RESET);
}

static struct mtk_modem *ccci_md_alloc(struct mtk_pci_dev *mtk_dev)
{
	struct mtk_modem *md;

	md = devm_kzalloc(&mtk_dev->pdev->dev, sizeof(*md), GFP_KERNEL);
	if (!md)
		return NULL;

	md->md_info = devm_kzalloc(&mtk_dev->pdev->dev, sizeof(*md->md_info), GFP_KERNEL);
	if (!md->md_info)
		return NULL;

	md->mtk_dev = mtk_dev;
	mtk_dev->md = md;
	atomic_set(&md->core_md.ready, 0);
	atomic_set(&md->core_md.handshake_ongoing, 0);
	md->handshake_wq = alloc_workqueue("%s",
					   WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI,
					   0, "md_hk_wq");
	if (!md->handshake_wq)
		return NULL;

	INIT_WORK(&md->handshake_work, md_hk_wq);
	md->core_md.feature_set[RT_ID_MD_PORT_ENUM] &= ~FEATURE_MSK;
	md->core_md.feature_set[RT_ID_MD_PORT_ENUM] |=
		FIELD_PREP(FEATURE_MSK, MTK_FEATURE_MUST_BE_SUPPORTED);

	atomic_set(&md->core_sap.ready, 0);
	atomic_set(&md->core_sap.handshake_ongoing, 0);
	md->sap_handshake_wq =
		alloc_workqueue("%s", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 
					   0, "sap_hk_wq");
	if (!md->sap_handshake_wq)
		return NULL;
	INIT_WORK(&md->sap_handshake_work, sap_hk_wq);
	md->core_sap.feature_set[RT_ID_SAP_PORT_ENUM] &= ~FEATURE_MSK;
	md->core_sap.feature_set[RT_ID_SAP_PORT_ENUM] |= 
		FIELD_PREP(FEATURE_MSK, MTK_FEATURE_MUST_BE_SUPPORTED);

	return md;
}

void mtk_md_reset(struct mtk_pci_dev *mtk_dev)
{
	struct mtk_modem *md;

	md = mtk_dev->md;
	md->md_init_finish = false;
	md_structure_reset(md);
	ccci_fsm_reset();
	cldma_reset(ID_CLDMA1);
	cldma_reset(ID_CLDMA0);
	port_proxy_reset(&mtk_dev->pdev->dev);
	md->md_init_finish = true;
	core_reset(md);
}

/**
 * mtk_md_init() - Initialize modem
 * @mtk_dev: MTK device
 *
 * Allocate and initialize MD ctrl block, and initialize data path
 * Register MHCCIF ISR and RGU ISR, and start the state machine
 *
 * Return: 0 on success or -ENOMEM on allocation failure
 */
int mtk_md_init(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_fsm_ctl *fsm_ctl;
	struct mtk_modem *md;
	int ret;

	/* allocate and initialize md ctrl memory */
	md = ccci_md_alloc(mtk_dev);
	if (!md)
		return -ENOMEM;

	ret = cldma_alloc(ID_CLDMA1, mtk_dev);
	if (ret)
		goto err_alloc;

	ret = cldma_alloc(ID_CLDMA0, mtk_dev);
	if (ret)
		goto err_alloc;

	/* initialize md ctrl block */
	md_structure_reset(md);

	ret = ccci_fsm_init(md);
	if (ret)
		goto err_alloc;

	/* init the data path */
	ret = ccmni_init(mtk_dev);
	if (ret)
		goto err_fsm_init;

	ret = cldma_init(ID_CLDMA1);
	if (ret)
		goto err_ccmni_init;

	ret = cldma_init(ID_CLDMA0);
	if (ret)
		goto err_cldma_init;

	ret = port_proxy_init(mtk_dev->md);
	if (ret)
		goto err_cldma_init;

	fsm_ctl = fsm_get_entry();
	fsm_append_command(fsm_ctl, CCCI_COMMAND_START, 0);

	md_sys_sw_init(mtk_dev);

	md->md_init_finish = true;
	return 0;

err_cldma_init:
	cldma_exit(ID_CLDMA1);
err_ccmni_init:
	ccmni_exit(mtk_dev);
err_fsm_init:
	ccci_fsm_uninit();
err_alloc:
	destroy_workqueue(md->handshake_wq);
	destroy_workqueue(md->sap_handshake_wq);

	dev_err(&mtk_dev->pdev->dev, "modem init failed\n");
	return ret;
}

void mtk_md_exit(struct mtk_pci_dev *mtk_dev)
{
	struct mtk_modem *md = mtk_dev->md;
	struct ccci_fsm_ctl *fsm_ctl;

	md = mtk_dev->md;

	mtk_pcie_mac_clear_int(mtk_dev, SAP_RGU_INT);

	if (!md->md_init_finish)
		return;

	fsm_ctl = fsm_get_entry();
	/* change FSM state, will auto jump to stopped */
	fsm_append_command(fsm_ctl, CCCI_COMMAND_PRE_STOP, 1);
	port_proxy_uninit();
	cldma_exit(ID_CLDMA0);
	cldma_exit(ID_CLDMA1);
	ccmni_exit(mtk_dev);
	ccci_fsm_uninit();
	destroy_workqueue(md->handshake_wq);
	destroy_workqueue(md->sap_handshake_wq);
}
