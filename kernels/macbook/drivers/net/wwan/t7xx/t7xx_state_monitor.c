// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>

#include "t7xx_pci.h"
#include "t7xx_monitor.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"
#include "t7xx_isr.h"
#include "t7xx_modem_ops.h"
#include "t7xx_pci_ats.h"

#define HOST_EVENT_OFFSET 28

static struct ccci_fsm_ctl *ccci_fsm_entry;
static int count;

int fsm_notifier_register(struct fsm_notifier_block *notifier)
{
	struct ccci_fsm_ctl *ctl;
	unsigned long flags;

	if (!notifier) {
		pr_err("Invalid Arguments");
		return -EINVAL;
	}

	ctl = ccci_fsm_entry;
	spin_lock_irqsave(&ctl->notifier_lock, flags);
	list_add_tail(&notifier->entry, &ctl->notifier_list);
	spin_unlock_irqrestore(&ctl->notifier_lock, flags);

	return 0;
}

int fsm_notifier_unregister(struct fsm_notifier_block *notifier)
{
	struct ccci_fsm_ctl *ctl;
	unsigned long flags;
	struct fsm_notifier_block *notifier_cur, *notifier_next;

	if (!notifier) {
		pr_err("Invalid Arguments");
		return -EINVAL;
	}

	ctl = ccci_fsm_entry;
	if (!ctl) {
		pr_err("Invalid Arguments. ctl is null");
		return -EINVAL;
	}
	spin_lock_irqsave(&ctl->notifier_lock, flags);
	list_for_each_entry_safe(notifier_cur, notifier_next, &ctl->notifier_list, entry) {
		if (notifier_cur == notifier)
			list_del(&notifier->entry);
	}
	spin_unlock_irqrestore(&ctl->notifier_lock, flags);

	return 0;
}

static int fsm_state_notify(enum MD_STATE state)
{
	struct ccci_fsm_ctl *ctl;
	unsigned long flags;
	struct fsm_notifier_block *notifier;

	if (state >= MD_STATE_MAX) {
		pr_err("Invalid Arguments");
		return -EINVAL;
	}

	ctl = ccci_fsm_entry;
	if (!ctl) {
		pr_err("Invalid Arguments");
		return -EINVAL;
	}
	spin_lock_irqsave(&ctl->notifier_lock, flags);
	list_for_each_entry(notifier, &ctl->notifier_list, entry) {
		spin_unlock_irqrestore(&ctl->notifier_lock, flags);
		if (notifier->notifier_fn)
			notifier->notifier_fn(state, notifier->data);
		spin_lock_irqsave(&ctl->notifier_lock, flags);
	}
	spin_unlock_irqrestore(&ctl->notifier_lock, flags);

	return 0;
}

void fsm_broadcast_state(struct ccci_fsm_ctl *ctl,
			 enum MD_STATE state)
{
	if (ctl->md_state != BOOT_WAITING_FOR_HS2 && state == READY)
		return;
	ctl->md_state = state;

	/* update to port first, otherwise sending message on HS2 may fail */
	ccci_port_md_status_notify(state);

	fsm_state_notify(state);
}

static void fsm_finish_command(struct ccci_fsm_ctl *ctl,
			       struct ccci_fsm_command *cmd, int result)
{
	unsigned long flags;

	if (cmd->flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETE) {
		spin_lock_irqsave(&ctl->cmd_complete_lock, flags);
		cmd->complete = result;
		/* do not dereference cmd after this line */
		wake_up_all(&cmd->complete_wq);
		/* after cmd in list, processing thread may see
		 * it without being awaken, so spinlock is needed
		 */
		spin_unlock_irqrestore(&ctl->cmd_complete_lock, flags);
	} else {
		/* no one is waiting for this, free */
		kfree(cmd);
	}
}

/* call only with protection of event_lock */
static void fsm_finish_event(struct ccci_fsm_ctl *ctl,
			     struct ccci_fsm_event *event)
{
	list_del(&event->entry);
	kfree(event);
}

static void fsm_routine_zombie(struct ccci_fsm_ctl *ctl)
{
	struct ccci_fsm_event *event, *evt_next;
	struct ccci_fsm_command *cmd, *cmd_next;
	unsigned long flags;

	spin_lock_irqsave(&ctl->command_lock, flags);
	list_for_each_entry_safe(cmd, cmd_next, &ctl->command_queue, entry) {
		pr_err("unhandled command %d\n", cmd->cmd_id);
		list_del(&cmd->entry);
		fsm_finish_command(ctl, cmd, -1);
	}
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, evt_next, &ctl->event_queue, entry) {
		pr_err("unhandled event %d\n", event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
}

/* cmd is not NULL only when reason is ordinary EE */
static void fsm_routine_exception(struct ccci_fsm_ctl *ctl,
				  enum ccci_ee_reason reason)
{
	int cnt, rec_ok_got = 0, pass_got = 0;
	struct ccci_fsm_event *event;
	unsigned long flags;
	struct t7xx_port *log_port, *meta_port;

	pr_err("exception %d, from %ps\n", reason, __builtin_return_address(0));
	/* state sanity check */
	if (ctl->curr_state != CCCI_FSM_READY && ctl->curr_state != CCCI_FSM_STARTING)
		return;

	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_EXCEPTION;
	if (reason == EXCEPTION_HS_TIMEOUT)
		mdee_set_ex_start_str(&ctl->ee_ctl, 0, NULL);

	/* check EE reason */
	switch (reason) {
	case EXCEPTION_HS_TIMEOUT:
		pr_err("BOOT_HS_FAIL!\n");
		break;
	case EXCEPTION_EE:
		fsm_broadcast_state(ctl, EXCEPTION);
		mtk_pci_pm_exp_detected(ctl->md->mtk_dev);
		ccci_md_exception_handshake(ctl->md, MD_EX_CCIF_TIMEOUT);
		cnt = 0;
		while (cnt < MD_EX_REC_OK_TIMEOUT / EVENT_POLL_INTEVAL) {
			if (kthread_should_stop())
				return;

			spin_lock_irqsave(&ctl->event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue,
							 struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX) {
					fsm_finish_event(ctl, event);
				} else if (event->event_id == CCCI_EVENT_MD_EX_REC_OK) {
					rec_ok_got = 1;
					fsm_finish_event(ctl, event);
				}
			}
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			if (rec_ok_got)
				break;

			cnt++;
			msleep(EVENT_POLL_INTEVAL);
		}

		fsm_md_exception_stage(&ctl->ee_ctl, 1);

		cnt = 0;
		while (cnt < MD_EX_PASS_TIMEOUT / EVENT_POLL_INTEVAL) {
			if (kthread_should_stop())
				return;

			spin_lock_irqsave(&ctl->event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue,
							 struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX_PASS) {
					pass_got = 1;
					fsm_finish_event(ctl, event);
				}
			}
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			if (pass_got) {
				log_port = port_get_by_name("ttyCMdLog");
				if (log_port)
					log_port->ops->enable_chl(log_port);
				else
					pr_err("can't found logging port\n");
				meta_port = port_get_by_name("ttyCMdMeta");
				if (meta_port)
					meta_port->ops->enable_chl(meta_port);
				else
					pr_err("can't found meta port\n");
				break;
			}
			cnt++;
			msleep(EVENT_POLL_INTEVAL);
		}

		fsm_md_exception_stage(&ctl->ee_ctl, 2);

		/* wait until modem memory dump is done */
		fsm_check_ee_done(&ctl->ee_ctl, ctl->md, EE_DONE_TIMEOUT);

		break;
	default:
		break;
	}
}

static void fsm_stopped_handler(struct ccci_fsm_ctl *ctl)
{
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STOPPED;

	fsm_broadcast_state(ctl, STOPPED);
	ccci_modem_reset(ctl->md->mtk_dev);
}

static void fsm_routine_stopped(struct ccci_fsm_ctl *ctl,
				struct ccci_fsm_command *cmd)
{
	/* state sanity check */
	if (ctl->curr_state == CCCI_FSM_STOPPED) {
		fsm_finish_command(ctl, cmd, -1);
		return;
	}

	fsm_stopped_handler(ctl);
	fsm_finish_command(ctl, cmd, 1);
}

static void fsm_routine_stopping(struct ccci_fsm_ctl *ctl,
				 struct ccci_fsm_command *cmd)
{
	struct mtk_pci_dev *mtk_dev = ctl->md->mtk_dev;
	int err;

	/* state sanity check */
	if (ctl->curr_state == CCCI_FSM_STOPPED ||
	    ctl->curr_state == CCCI_FSM_STOPPING) {
		fsm_finish_command(ctl, cmd, -1);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STOPPING;

	fsm_broadcast_state(ctl, WAITING_TO_STOP);
	/* stop hw */
	ccci_cldma_stop(ID_CLDMA1);

	if (atomic_read(&ctl->md->rgu_irq_asserted) == 0) {
		pci_ignore_hotplug(mtk_dev->pdev);
		/* To disable DRM before FLDR */;
		mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_DRM_DISABLE_AP);
		msleep(200);
		/* try fldr first */
		err = mtk_acpi_fldr_func(mtk_dev);
		if (err)
			mhccif_h2d_swint_trigger(mtk_dev, H2D_CH_DEVICE_RESET);
	}

	/* auto jump to stopped state handler */
	fsm_stopped_handler(ctl);

	fsm_finish_command(ctl, cmd, 1);
}

static void fsm_routine_ready(struct ccci_fsm_ctl *ctl)
{
	struct ccci_modem *md = ctl->md;

	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_READY;
	fsm_broadcast_state(ctl, READY);
	if (!md || !md->ops->md_event_notify) {
		pr_err("invalid md or md_event_notify function pointer\n");
		return;
	}
	md->ops->md_event_notify(md, FSM_READY);
}

static void fsm_routine_starting(struct ccci_fsm_ctl *ctl)
{
	struct ccci_modem *md = ctl->md;
	struct device *dev;
	int ret;

	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STARTING;

	ret = ccci_md_start(md);
	if (ret) {
		pr_err("fail to start MD\n");
		return;
	}
	fsm_broadcast_state(ctl, BOOT_WAITING_FOR_HS1);
	if (!md || !md->ops->md_event_notify) {
		pr_err("invalid md or md_event_notify function pointer\n");
		return;
	}
	dev = &md->mtk_dev->pdev->dev;
	md->ops->md_event_notify(md, FSM_START);
	do {
		ret = wait_event_interruptible_timeout(ctl->async_hk_wq,
						       atomic_read(&md->core_md.ready) ||
						       atomic_read(&ctl->exp_flg),
						       HZ * 60);
		if (kthread_should_stop())
			break;
	} while (ret == -ERESTARTSYS);
	if (ret == 0) {
		if (atomic_read(&md->core_md.ready) == 0) {
			dev_err(dev, "MD handshake timeout\n");
			if (atomic_read(&md->core_md.handshake_ongoing) == 1)
				fsm_append_event(ctl, CCCI_EVENT_MD_HS2_EXIT, NULL, 0);

			fsm_routine_exception(ctl, EXCEPTION_HS_TIMEOUT);
		}
	} else if (atomic_read(&ctl->exp_flg)) {
		dev_err(dev, "md exception is captured during handshake\n");
		if (atomic_read(&md->core_md.ready) == 0 &&
		    atomic_read(&md->core_md.handshake_ongoing) == 1)
			fsm_append_event(ctl, CCCI_EVENT_MD_HS2_EXIT, NULL, 0);
	} else {
		mtk_pci_pm_init_late(md->mtk_dev);
		fsm_routine_ready(ctl);
	}
}

static void fsm_routine_pre_start(struct ccci_fsm_ctl *ctl,
				  struct ccci_fsm_command *cmd)
{
	u32 dummy_reg;
	unsigned char device_stage;
	struct ccci_modem *md = ctl->md;

	if (!md)
		return;

	dummy_reg = mtk_dummy_reg_get(md->mtk_dev) & ~HOST_EVENT_MASK;
	if (dummy_reg == (0xFFFFFFFF & ~HOST_EVENT_MASK)) {
		pr_err("invalid PCIe register value\n");
		fsm_finish_command(ctl, cmd, -1);
		ctl->last_dummy_reg = dummy_reg;
		return;
	}
	/* state sanity check */
	if (ctl->curr_state != CCCI_FSM_INIT &&
	    ctl->curr_state != CCCI_FSM_PRE_STARTING &&
	    ctl->curr_state != CCCI_FSM_STOPPED) {
		fsm_finish_command(ctl, cmd, -1);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_PRE_STARTING;
	if (!md->ops->md_event_notify) {
		pr_err("Invalid md_event_notify function pointer\n");
		fsm_finish_command(ctl, cmd, -1);
		return;
	}
	md->ops->md_event_notify(md, FSM_PRE_START);

	if (dummy_reg != ctl->last_dummy_reg) {
		device_stage = dummy_reg & DEV_STAGE_MASK;
		switch (device_stage) {
		case INIT_STAGE:
			fsm_append_command(ctl, CCCI_COMMAND_PRE_START, 0);
			break;
		case LINUX_STAGE:
			ccci_cldma_hif_hw_init(ID_CLDMA1);
			fsm_routine_starting(ctl);
			break;
		default:
			break;
		}
		/* store dummy_register */
		ctl->last_dummy_reg = dummy_reg;
		count = 0;
	} else {
		if (dummy_reg == 0) {
			if (count++ >= 200) {
				count = 0;
			} else {
				msleep(100);
				fsm_append_command(ctl, CCCI_COMMAND_PRE_START, 0);
			}
		} else {
			count = 0;
		}
	}
	fsm_finish_command(ctl, cmd, 1);
}

static int fsm_main_thread(void *data)
{
	struct ccci_fsm_ctl *ctl = data;
	struct ccci_fsm_command *cmd;
	unsigned long flags;
	int ret;

	while (1) {
		ret = wait_event_interruptible(ctl->command_wq,
					       (!list_empty(&ctl->command_queue) ||
					       kthread_should_stop()));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&ctl->command_lock, flags);
		cmd = list_first_entry(&ctl->command_queue,
				       struct ccci_fsm_command, entry);
		/* delete first, otherwise hard to peek next command in routine */
		list_del(&cmd->entry);
		spin_unlock_irqrestore(&ctl->command_lock, flags);

		switch (cmd->cmd_id) {
		case CCCI_COMMAND_PRE_START:
			fsm_routine_pre_start(ctl, cmd);
			break;
		case CCCI_COMMAND_PRE_STOP:
			fsm_routine_stopping(ctl, cmd);
			break;
		case CCCI_COMMAND_STOP:
			fsm_routine_stopped(ctl, cmd);
			break;
		default:
			pr_err("Invalid command %d\n", cmd->cmd_id);
			fsm_finish_command(ctl, cmd, -1);
			fsm_routine_zombie(ctl);
			break;
		}
	}
	return 0;
}

int fsm_append_command(struct ccci_fsm_ctl *ctl,
		       enum ccci_fsm_cmd_state cmd_id, unsigned int flag)
{
	struct ccci_fsm_command *cmd;
	int result = 0;
	unsigned long flags;

	if (cmd_id <= CCCI_COMMAND_INVALID || cmd_id >= CCCI_COMMAND_MAX) {
		pr_err("invalid command %d\n", cmd_id);
		return -EINVAL;
	}

	cmd = kmalloc(sizeof(*cmd),
		      (in_irq() || in_softirq() || irqs_disabled()) ? GFP_ATOMIC : GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	INIT_LIST_HEAD(&cmd->entry);
	init_waitqueue_head(&cmd->complete_wq);
	cmd->cmd_id = cmd_id;
	cmd->complete = 0;
	if (in_irq() || irqs_disabled())
		flag &= ~FSM_CMD_FLAG_WAIT_FOR_COMPLETE;
	cmd->flag = flag;

	spin_lock_irqsave(&ctl->command_lock, flags);
	list_add_tail(&cmd->entry, &ctl->command_queue);
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	/* after this line, only dereference cmd when "wait-for-complete" */
	wake_up(&ctl->command_wq);
	if (flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETE) {
		if (in_irq() || in_softirq())
			pr_err("Not to block in atomic context!\n");
		wait_event(cmd->complete_wq, cmd->complete);
		if (cmd->complete != 1)
			result = -1;
		spin_lock_irqsave(&ctl->cmd_complete_lock, flags);
		kfree(cmd);
		spin_unlock_irqrestore(&ctl->cmd_complete_lock, flags);
	}
	return result;
}

int fsm_append_event(struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id,
		     unsigned char *data, unsigned int length)
{
	struct ccci_fsm_event *event;
	unsigned long flags;

	if (event_id <= CCCI_EVENT_INVALID || event_id >= CCCI_EVENT_MAX) {
		pr_err("invalid event %d\n", event_id);
		return -EINVAL;
	}
	event = kmalloc(struct_size(event, data, length),
			in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	INIT_LIST_HEAD(&event->entry);
	event->event_id = event_id;
	event->length = length;
	if (data && length)
		memcpy(event->data, data, flex_array_size(event, data, event->length));

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_add_tail(&event->entry, &ctl->event_queue);
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	wake_up_all(&ctl->event_wq);
	return 0;
}

void fsm_clear_event(struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id)
{
	struct ccci_fsm_event *event, *evt_next;
	unsigned long flags;

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, evt_next, &ctl->event_queue, entry) {
		pr_err("unhandled event %d\n", event->event_id);
		if (event->event_id == event_id)
			fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
}

struct ccci_fsm_ctl *fsm_get_entity_by_device_number(dev_t dev_n)
{
	if (ccci_fsm_entry && ccci_fsm_entry->monitor_ctl.dev_n == dev_n)
		return ccci_fsm_entry;

	return NULL;
}

struct ccci_fsm_ctl *fsm_get_entry(void)
{
	return ccci_fsm_entry;
}

enum MD_STATE ccci_fsm_get_md_state(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entry;

	if (ctl)
		return ctl->md_state;
	else
		return INVALID;
}

unsigned int ccci_fsm_get_current_state(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entry;

	if (ctl)
		return ctl->curr_state;
	else
		return CCCI_FSM_STOPPED;
}

int ccci_fsm_recv_md_interrupt(enum md_irq_type type)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entry;

	if (!ctl)
		return -EINVAL;

	if (type == MD_IRQ_PORT_ENUM) {
		fsm_append_command(ctl, CCCI_COMMAND_PRE_START, 0);
	} else if (type == MD_IRQ_CCIF_EX) {
		/* interrupt handshake flow */
		atomic_set(&ctl->exp_flg, 1);
		wake_up(&ctl->async_hk_wq);
		fsm_md_exception_stage(&ctl->ee_ctl, 0);
		fsm_routine_exception(ctl, EXCEPTION_EE);
	}
	return 0;
}

int ccci_fsm_reset(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entry;

	if (!ctl) {
		pr_err("Invalid value for ccci_fsm_ctl\n");
		return -EINVAL;
	}

	/* Clear event and command queues */
	fsm_routine_zombie(ctl);

	ctl->last_state = CCCI_FSM_INIT;
	ctl->curr_state = CCCI_FSM_STOPPED;
	atomic_set(&ctl->fs_ongoing, 0);
	atomic_set(&ctl->exp_flg, 0);
	ctl->last_dummy_reg = 0;

	fsm_ee_reset(&ctl->ee_ctl);
	return 0;
}

int ccci_fsm_init(struct ccci_modem *md)
{
	struct ccci_fsm_ctl *ctl;

	ctl = kzalloc(sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return -ENOMEM;
	ccci_fsm_entry = ctl;
	ctl->md = md;
	ctl->last_state = CCCI_FSM_INIT;
	ctl->curr_state = CCCI_FSM_INIT;
	atomic_set(&ctl->fs_ongoing, 0);
	ctl->last_dummy_reg = 0;
	INIT_LIST_HEAD(&ctl->command_queue);
	INIT_LIST_HEAD(&ctl->event_queue);
	init_waitqueue_head(&ctl->async_hk_wq);
	init_waitqueue_head(&ctl->event_wq);
	INIT_LIST_HEAD(&ctl->notifier_list);
	init_waitqueue_head(&ctl->command_wq);
	spin_lock_init(&ctl->event_lock);
	spin_lock_init(&ctl->command_lock);
	spin_lock_init(&ctl->cmd_complete_lock);
	atomic_set(&ctl->fs_ongoing, 0);
	atomic_set(&ctl->exp_flg, 0);
	spin_lock_init(&ctl->notifier_lock);

	ctl->fsm_thread = kthread_run(fsm_main_thread, ctl, "ccci_fsm");
	fsm_ee_reset(&ctl->ee_ctl);
	spin_lock_init(&ctl->ee_ctl.ctrl_lock);

	return 0;
}

void ccci_fsm_uninit(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entry;

	if (!ctl)
		return;

	if (ctl->fsm_thread)
		kthread_stop(ctl->fsm_thread);

	fsm_routine_zombie(ctl);
	kfree_sensitive(ctl);
}

int ccci_fsm_recv_status_packet(struct sk_buff *skb)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entry();
	struct ccci_fsm_poller *poller_ctl;

	if (!ctl)
		return -EINVAL;

	poller_ctl = &ctl->poller_ctl;

	poller_ctl->poller_state = FSM_POLLER_RECEIVED_RESPONSE;
	wake_up(&poller_ctl->status_rx_wq);
	ccci_free_skb(skb);

	return 0;
}
