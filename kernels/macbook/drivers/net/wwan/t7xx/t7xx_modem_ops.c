// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/device.h>

#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"
#include "t7xx_pci.h"
#include "t7xx_isr.h"
#include "t7xx_netdev.h"

void ccci_md_reset(struct ccci_modem *md)
{
	if (!md) {
		pr_err("Invalid arguments\n");
		return;
	}

	md->md_init_finish = 0;
}

struct ccci_modem *ccci_md_alloc(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_modem *md = kzalloc(sizeof(*md), GFP_KERNEL);

	if (!md)
		return NULL;

	md->md_info = kzalloc(sizeof(*md->md_info), GFP_KERNEL);
	if (!md->md_info) {
		kfree(md);
		return NULL;
	}
	md->mtk_dev = mtk_dev;
	mtk_dev->md = md;

	return md;
}

int ccci_md_register(struct device *dev, struct ccci_modem *md)
{
	if (md->ops->init)
		md->ops->init(md);

	return ccci_fsm_init(md);
}

void ccci_md_unregister(struct ccci_modem *md)
{
	ccci_port_uninit();
	ccci_fsm_uninit();
	if (md->ops->uninit)
		md->ops->uninit(md);
}

int ccci_md_start(struct ccci_modem *md)
{
	int ret;

	if (md && md->ops->start) {
		ret = md->ops->start(md);
	} else {
		pr_err("Callback Fail,Pmd:%p\n", md);
		ret = -EINVAL;
	}

	return ret;
}

void ccci_md_exception_handshake(struct ccci_modem *md, int timeout)
{
	if (md && md->ops->ee_handshake)
		md->ops->ee_handshake(md, timeout);
	else
		pr_err("Callback Fail,Pmd:%p\n", md);
}

static inline unsigned int get_interrupt_status(struct mtk_pci_dev *mtk_dev)
{
	return mhccif_read_sw_int_sts(mtk_dev) & D2H_SW_INT_MASK;
}

int mtk_pci_mhccif_isr(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_modem *md = mtk_dev->md;
	struct md_sys1_info *md_info = md->md_info;
	struct ccci_fsm_ctl *ctl = fsm_get_entry();
	unsigned int int_sta;
	unsigned long flags;
	u32 mask;

	if (!ctl) {
		dev_err(&mtk_dev->pdev->dev, "Invalid Arguments. ccci_fsm_ctl is null");
		return -EINVAL;
	}

	spin_lock_irqsave(&md_info->exp_spinlock, flags);
	int_sta = get_interrupt_status(mtk_dev);
	md_info->exp_id |= int_sta;

	if (md_info->exp_id & D2H_INT_PORT_ENUM) {
		md_info->exp_id &= ~D2H_INT_PORT_ENUM;
		if (ctl->curr_state == CCCI_FSM_INIT ||
		    ctl->curr_state == CCCI_FSM_PRE_STARTING ||
		    ctl->curr_state == CCCI_FSM_STOPPED)
			ccci_fsm_recv_md_interrupt(MD_IRQ_PORT_ENUM);
	}

	if (md_info->exp_id & D2H_INT_EXCEPTION_INIT) {
		if (ctl->md_state == INVALID /*Added for MD early EE */ ||
		    ctl->md_state == BOOT_WAITING_FOR_HS1 ||
		    ctl->md_state == BOOT_WAITING_FOR_HS2 ||
		    ctl->md_state == READY) {
			md_info->exp_id &= ~D2H_INT_EXCEPTION_INIT;
			ccci_fsm_recv_md_interrupt(MD_IRQ_CCIF_EX);
		}
	} else if (ctl->md_state == BOOT_WAITING_FOR_HS1) {
		/* start handshake if MD not assert */
		mask = mhccif_mask_get(mtk_dev);
		if ((md_info->exp_id & D2H_INT_ASYNC_MD_HK) &&
		    !(mask & D2H_INT_ASYNC_MD_HK)) {
			md_info->exp_id &= ~D2H_INT_ASYNC_MD_HK;
			queue_work(md->handshake_wq, &md->handshake_work);
		}
	}

	spin_unlock_irqrestore(&md_info->exp_spinlock, flags);

	return 0;
}

static void clr_device_irq_via_pcie(struct mtk_pci_dev *mtk_dev)
{
	struct mtk_addr_base *pbase_addr = &mtk_dev->base_addr;
	void __iomem *rgu_pciesta_reg;

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
	mtk_clear_int_status(mtk_dev, SAP_RGU_INT);
}

static int mtk_acpi_reset(struct mtk_pci_dev *mtk_dev, char *fn_name)
{
#ifdef CONFIG_ACPI
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct device *dev = &mtk_dev->pdev->dev;
	acpi_status acpi_ret;
	acpi_handle handle;

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
		dev_err(dev, "%s method fail: %s\n", fn_name,
			acpi_format_exception(acpi_ret));
		return -EFAULT;
	}
#endif
	return 0;
}

int mtk_acpi_fldr_func(struct mtk_pci_dev *mtk_dev)
{
	return mtk_acpi_reset(mtk_dev, "_RST");
}

static void reset_device_via_pmic(struct mtk_pci_dev *mtk_dev)
{
	unsigned int val;

	/*[27:26] 1:cold reset, 2: warm reset, others: cold reset */
	val = mtk_dummy_reg_get(mtk_dev);

	if (val & DUMMY_7_RST_TYPE_PLDR)
		mtk_acpi_reset(mtk_dev, "MRST._RST");
	else if (val & DUMMY_7_RST_TYPE_FLDR)
		mtk_acpi_fldr_func(mtk_dev);

	/* fix me, cold reset */
}

static void rgu_irq_work_fn(struct work_struct *work)
{
	struct delayed_work *irq_work;
	struct mtk_pci_dev *mtk_dev;
	struct ccci_modem *modem;

	irq_work = to_delayed_work(work);
	modem = container_of(irq_work, struct ccci_modem, rgu_irq_work);
	mtk_dev = (struct mtk_pci_dev *)modem->mtk_dev;

	msleep(20);
	reset_device_via_pmic(mtk_dev);
}

static int rgu_handler_func(void *data)
{
	struct mtk_pci_dev *mtk_dev = (struct mtk_pci_dev *)data;
	struct ccci_modem *modem;

	modem = mtk_dev->md;

	mtk_clear_rgu_irq(mtk_dev);

	if (atomic_read(&mtk_dev->rgu_pci_irq_en) == 0) {
		atomic_set(&modem->rgu_irq_asserted_in_suspend, 1);
		goto exit;
	}

	atomic_set(&modem->rgu_irq_asserted, 1);
	mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, true);
	queue_delayed_work(modem->rgu_workqueue, &modem->rgu_irq_work,
			   msecs_to_jiffies(0));

exit:
	return IRQ_HANDLED;
}

static void mtk_pcie_register_rgu_isr(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_modem *modem;

	modem = mtk_dev->md;

	modem->rgu_workqueue = create_singlethread_workqueue("rgu_workqueue");
	INIT_DELAYED_WORK(&modem->rgu_irq_work, rgu_irq_work_fn);

	/* registers RGU callback isr with PCIe driver */
	mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, true);
	mtk_clear_int_status(mtk_dev, SAP_RGU_INT);

	mtk_dev->intr_callback[SAP_RGU_INT] = rgu_handler_func;
	mtk_dev->callback_param[SAP_RGU_INT] = mtk_dev;
	mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, false);
}

static void md_exception(struct ccci_modem *md, enum hif_ex_stage stage)
{
	struct mtk_pci_dev *mtk_dev = md->mtk_dev;

	if (stage == HIF_EX_CLEARQ_DONE) {
		/* give DHL some time to flush data */
		msleep(2000);
		port_reset();
	}

	ccci_cldma_exception(ID_CLDMA1, stage);

	if (stage == HIF_EX_INIT)
		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_EXCEPTION_ACK);
	else if (stage == HIF_EX_CLEARQ_DONE)
		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_EXCEPTION_CLEARQ_ACK);
}

static int wait_hif_ex_hk_event(struct ccci_modem *md, int event_id)
{
	struct md_sys1_info *md_info = md->md_info;
	int time_once = 10;
	int cnt = 500; /* MD timeout is 10s */

	while (cnt > 0) {
		if (md_info->exp_id & event_id)
			return 0;
		msleep(time_once);
		cnt--;
	}
	return -EFAULT;
}

static int md_ee_handshake(struct ccci_modem *md, int timeout)
{
	struct mtk_pci_dev *mtk_dev = md->mtk_dev;
	int ret;

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

	return 0;
}

static void md_sys1_sw_init(struct mtk_pci_dev *mtk_dev)
{
	/* register the mhccif isr for md exception, port enum and
	 * async handshake notifications
	 */
	mhccif_mask_set(mtk_dev, D2H_SW_INT_MASK);
	mtk_dev->mhccif_bitmask = D2H_SW_INT_MASK;
	mhccif_mask_clr(mtk_dev, D2H_INT_PORT_ENUM);

	/* register rgu irq handler for sAP exception notification */
	atomic_set(&mtk_dev->rgu_pci_irq_en, 1);
	mtk_pcie_register_rgu_isr((void *)mtk_dev);
}

struct feature_query {
	u32 head_pattern;
	struct ccci_feature_support feature_set[FEATURE_COUNT];
	u32 tail_pattern;
};

static void prepare_host_rt_data_query(struct core_sys_info *core)
{
	struct ctrl_msg_header *ctrl_msg_h;
	struct feature_query *ft_query;
	struct ccci_header *ccci_h;
	struct sk_buff *skb;
	int packet_size;

	packet_size = sizeof(struct ccci_header) +
		      sizeof(struct ctrl_msg_header) +
		      sizeof(struct feature_query);
	skb = ccci_alloc_skb(packet_size, 0, 1);
	if (!skb)
		return;
	skb_put(skb, packet_size);
	/* fill ccci header */
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->data[0] = 0;
	ccci_h->data[1] = packet_size;
	ccci_h->channel = core->ctl_port->tx_ch;
	ccci_h->seq_num = 0;
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
	ft_query->head_pattern = MD_FEATURE_QUERY_PATTERN;
	memcpy(ft_query->feature_set, core->feature_set,
	       sizeof(struct ccci_feature_support) * FEATURE_COUNT);
	ft_query->tail_pattern = MD_FEATURE_QUERY_PATTERN;
	/* send HS1 msg to device */
	ccci_port_send_hif(core->ctl_port, skb, 0, 1);
}

static int prepare_device_rt_data(struct core_sys_info *core, struct device *dev,
				  void *data, int data_length)
{
	struct ccci_runtime_feature rt_feature;
	struct ctrl_msg_header *ctrl_msg_h;
	struct feature_query *md_feature;
	struct ccci_header *ccci_h;
	struct sk_buff *skb;
	int packet_size = 0;
	char *rt_data;
	int i;

	skb = ccci_alloc_skb(SKB_4K, 0, 1);
	if (!skb)
		return -EFAULT;

	/* fill ccci header */
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->data[0] = 0;
	ccci_h->channel = core->ctl_port->tx_ch;
	ccci_h->seq_num = 0;
	ccci_h->reserved = 0;
	/* fill control message header */
	ctrl_msg_h = (struct ctrl_msg_header *)
				 (skb->data + sizeof(struct ccci_header));
	ctrl_msg_h->ctrl_msg_id = CTL_ID_HS3_MSG;
	ctrl_msg_h->reserved = 0;
	rt_data = (skb->data + sizeof(struct ccci_header) +
		   sizeof(struct ctrl_msg_header));

	/* parse md runtime data query */
	md_feature = (struct feature_query *)data;
	if (md_feature->head_pattern != MD_FEATURE_QUERY_PATTERN ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_PATTERN) {
		dev_err(dev, "md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			md_feature->head_pattern, md_feature->tail_pattern);
		return -EINVAL;
	}
	/* fill runtime feature */
	for (i = 0; i < FEATURE_COUNT; i++) {
		memset(&rt_feature, 0, sizeof(struct ccci_runtime_feature));
		rt_feature.feature_id = i;
		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_NOT_EXIST) {
			rt_feature.support_info = md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.support_info = md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_OPTIONAL_SUPPORT) {
			if (md_feature->feature_set[i].version ==
				core->support_feature_set[i].version &&
				core->support_feature_set[i].support_mask >=
				CCCI_FEATURE_MUST_SUPPORT) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					core->support_feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					core->support_feature_set[i].version;
			}
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (md_feature->feature_set[i].version >=
				core->support_feature_set[i].version) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					core->support_feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					core->support_feature_set[i].version;
			}
		}
		if (rt_feature.support_info.support_mask !=
			CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.data_len = 0;
			memcpy(rt_data, &rt_feature, sizeof(struct ccci_runtime_feature));
			rt_data += sizeof(struct ccci_runtime_feature);
		}
		packet_size += (sizeof(struct ccci_runtime_feature) + rt_feature.data_len);
	}
	ctrl_msg_h->data_length = packet_size;
	ccci_h->data[1] = packet_size + sizeof(struct ctrl_msg_header) +
			  sizeof(struct ccci_header);
	skb_put(skb, ccci_h->data[1]);
	/* send HS3 msg to device */
	ccci_port_send_hif(core->ctl_port, skb, 0, 1);
	return 0;
}

static int parse_host_rt_data(struct core_sys_info *core, struct device *dev,
			      void *data, int data_length)
{
	struct ccci_runtime_feature *rt_feature;
	unsigned char ft_spt_st, ft_spt_cfg, cur_ft_spt = 0;
	unsigned char *rt_ft_data, *ft_data;
	int ft_id_index, offset;
	int ret = 0;

	offset = sizeof(struct feature_query);
	for (ft_id_index = 0;
		 ft_id_index < FEATURE_COUNT && offset < data_length;
		 ft_id_index++) {
		rt_ft_data = data + offset;
		rt_feature = (struct ccci_runtime_feature *)rt_ft_data;
		ft_spt_st = rt_feature->support_info.support_mask;
		ft_spt_cfg = core->feature_set[ft_id_index].support_mask;
		ft_data = rt_feature->data;
		if (ft_spt_cfg == CCCI_FEATURE_NOT_EXIST ||
		    ft_spt_cfg == CCCI_FEATURE_NOT_SUPPORT) {
			offset += sizeof(struct ccci_runtime_feature)
				+ rt_feature->data_len;
			continue;
		}
		if (ft_spt_cfg == CCCI_FEATURE_MUST_SUPPORT) {
			if (ft_spt_st != CCCI_FEATURE_MUST_SUPPORT) {
				dev_err(dev, "mismatch: runtime feature%d (%d,%d)\n",
					ft_id_index, ft_spt_cfg, ft_spt_st);
				ret = -1;
				break;
			}
			cur_ft_spt = CCCI_FEATURE_MUST_SUPPORT;
		} else if (ft_spt_cfg == CCCI_FEATURE_OPTIONAL_SUPPORT) {
			cur_ft_spt = ft_spt_cfg;
		} else if (ft_spt_cfg == CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (ft_spt_st == CCCI_FEATURE_MUST_SUPPORT) {
				cur_ft_spt = CCCI_FEATURE_MUST_SUPPORT;
			} else {
				dev_err(dev, "mismatch: runtime feature%d (%d,%d)\n",
					ft_id_index, ft_spt_cfg, ft_spt_st);
				ret = -1;
				break;
			}
		}

		if (cur_ft_spt == CCCI_FEATURE_MUST_SUPPORT && ft_id_index == RT_ID_MD_PORT_ENUM)
			ccci_port_node_control(ft_data);

		offset += sizeof(struct ccci_runtime_feature) + rt_feature->data_len;
	}
	return ret;
}

static void core_reset(struct ccci_modem *md)
{
	struct ccci_fsm_ctl *fsm_ctl;

	atomic_set(&md->core_md.ready, 0);
	fsm_ctl = fsm_get_entry();

	if (!fsm_ctl) {
		dev_err(&md->mtk_dev->pdev->dev, "fsm ctrl isn't initialized\n");
		return;
	}
	/* Append HS2 EXIT event to exit the work "core_hk_handler" if the last
	 * handshake wasn't done yet.
	 * Remember to clear this event before entering the work "core_hk_handler".
	 */
	if (atomic_read(&md->core_md.handshake_ongoing) == 1)
		fsm_append_event(fsm_ctl, CCCI_EVENT_MD_HS2_EXIT, NULL, 0);
	atomic_set(&md->core_md.handshake_ongoing, 0);
}

static void core_hk_handler(struct ccci_modem *md,
			    struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id,
			    enum ccci_fsm_event_state err_detect)
{
	struct core_sys_info *core_info = &md->core_md;
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
		dev_err(dev, "core handshake 2 fail:%d\n", ret);
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

static void md_hk_wq(struct work_struct *work)
{
	struct ccci_modem *md = container_of(work, struct ccci_modem, handshake_work);
	struct ccci_fsm_ctl *ctl = fsm_get_entry();

	if (!ctl) {
		dev_err(&md->mtk_dev->pdev->dev, "Invalid Arguments. ccci_fsm_ctl is null");
		return;
	}

	/* Clear the HS2 EXIT event appended in core_reset(). */
	fsm_clear_event(ctl, CCCI_EVENT_MD_HS2_EXIT);
	ccci_cldma_switch_cfg(ID_CLDMA1);
	ccci_cldma_start(ID_CLDMA1);
	fsm_broadcast_state(ctl, BOOT_WAITING_FOR_HS2);
	atomic_set(&md->core_md.handshake_ongoing, 1);
	core_hk_handler(md, ctl, CCCI_EVENT_MD_HS2, CCCI_EVENT_MD_HS2_EXIT);
}

static int md_init(struct ccci_modem *md)
{
	atomic_set(&md->core_md.ready, 0);
	atomic_set(&md->core_md.handshake_ongoing, 0);
	md->handshake_wq =
		alloc_workqueue("%s", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 0, "md_hk_wq");
	INIT_WORK(&md->handshake_work, md_hk_wq);
	md->core_md.feature_set[RT_ID_MD_PORT_ENUM].support_mask = CCCI_FEATURE_MUST_SUPPORT;

	return 0;
}

static int md_uninit(struct ccci_modem *md)
{
	destroy_workqueue(md->handshake_wq);
	destroy_workqueue(md->rgu_workqueue);
	return 0;
}

static int md_start(struct ccci_modem *md)
{
	atomic_set(&md->reset_on_going, 0);
	return 0;
}

static int md_evt_notify_hd(struct ccci_modem *md, enum md_event_id evt_id)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entry();
	struct md_sys1_info *md_info = md->md_info;
	void __iomem *mhccif_base;
	unsigned long flags;
	unsigned int int_sta;

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
			iowrite32(D2H_INT_ASYNC_MD_HK,
				  mhccif_base + REG_EP2RC_SW_INT_ACK);
			mhccif_mask_set(md->mtk_dev, D2H_INT_ASYNC_MD_HK);
		} else {
			/* unmask async handshake interrupt */
			mhccif_mask_clr(md->mtk_dev, D2H_INT_ASYNC_MD_HK);
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
		break;
	}
	return 0;
}

static struct ccci_modem_ops md_ops = {
	.init = &md_init,
	.uninit = &md_uninit,
	.start = &md_start,
	.ee_handshake = &md_ee_handshake,
	.md_event_notify = &md_evt_notify_hd,
};

static void start_device(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_fsm_ctl *fsm_ctl = fsm_get_entry();
	u32 dummy_register;

	if (!fsm_ctl) {
		dev_err(&mtk_dev->pdev->dev, "fsm ctrl isn't initialized\n");
		return;
	}

	/* read PCIe Dummy Register */
	dummy_register = mtk_dummy_reg_get(mtk_dev);

	if (dummy_register == 0xffffffff) {
		dev_err(&mtk_dev->pdev->dev, "failed to read PCIe register\n");
		return;
	}

	ccci_port_init(mtk_dev->md);
	fsm_append_command(fsm_ctl, CCCI_COMMAND_PRE_START, 0);
}

static void md_structure_reset(struct ccci_modem *md)
{
	struct md_sys1_info *md_info;

	md->ops = &md_ops;
	/* initial modem private data */
	md_info = md->md_info;
	md_info->exp_id = 0;
	spin_lock_init(&md_info->exp_spinlock);
	atomic_set(&md->reset_on_going, 1);
	/* IRQ is default enabled after request_irq */
	atomic_set(&md->wdt_enabled, 1);
}

int mtk_pci_event_handler(struct mtk_pci_dev *mtk_dev, u32 event)
{
	struct ccci_fsm_ctl *fsm_ctl = fsm_get_entry();
	int ret = 0;

	switch (event) {
	case PCIE_EVENT_L3ENTER:
		ret = fsm_append_command(fsm_ctl, CCCI_COMMAND_STOP, 1);
		break;
	case PCIE_EVENT_L3EXIT:
		mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, true);
		mtk_clear_int_status(mtk_dev, SAP_RGU_INT);
		atomic_set(&mtk_dev->rgu_pci_irq_en, 1);
		mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, false);
		ret = fsm_append_command(fsm_ctl, CCCI_COMMAND_PRE_START, 0);
		break;
	default:
		break;
	}

	if (ret)
		dev_err(&mtk_dev->pdev->dev, "Error %d handling mtk event %u\n", ret, event);
	return ret;
}

int ccci_modem_reset(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_modem *md = mtk_dev->md;

	ccci_md_reset(md);
	md_structure_reset(md);
	ccci_fsm_reset();
	ccci_cldma_reset();
	port_reset();
	md->md_init_finish = 1;
	core_reset(md);

	return 0;
}

int ccci_modem_init(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_modem *md;
	int ret;

	/* allocate and initialize md ctrl memory */
	md = ccci_md_alloc(mtk_dev);
	if (!md)
		return -ENOMEM;

	ret = ccci_cldma_alloc(mtk_dev);
	if (ret)
		goto err_cldma_alloc;

	/* initialize md ctrl block */
	md_structure_reset(md);

	ret = ccci_md_register(&mtk_dev->pdev->dev, md);
	if (ret)
		goto err_md_register;

	/* init the data path */
	ret = ccmni_init(mtk_dev);
	if (ret)
		goto err_ccmni_init;

	ret = ccci_cldma_init(ID_CLDMA1);
	if (ret)
		goto err_cldma_init;

	start_device(mtk_dev);
	md_sys1_sw_init(mtk_dev);

	md->md_init_finish = 1;
	return 0;

err_cldma_init:
	ccmni_exit(mtk_dev);
err_ccmni_init:
	ccci_md_unregister(md);
err_md_register:
	ccci_cldma_exit(ID_CLDMA1);
err_cldma_alloc:
	kfree(md->md_info);
	kfree(md);

	dev_err(&mtk_dev->pdev->dev, "modem init failed.\n");

	return ret;
}

int ccci_modem_exit(struct mtk_pci_dev *mtk_dev)
{
	struct ccci_modem *md = mtk_dev->md;
	struct ccci_fsm_ctl *fsm_ctl;

	if (!md)
		return 0;

	mtk_clear_set_int_type(mtk_dev, SAP_RGU_INT, true);

	if (md->md_init_finish) {
		fsm_ctl = fsm_get_entry();
		/* change fsm state, stopping will auto jump to stopped */
		fsm_append_command(fsm_ctl, CCCI_COMMAND_PRE_STOP, 1);

		/* remove the data path */
		ccmni_exit(mtk_dev);

		ccci_md_unregister(md);

		ccci_cldma_exit(ID_CLDMA1);
	}

	mtk_dev->md = NULL;
	kfree_sensitive(md->md_info);
	kfree_sensitive(md);

	return 0;
}
