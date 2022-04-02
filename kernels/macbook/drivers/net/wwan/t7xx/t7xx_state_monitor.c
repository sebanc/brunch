// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021-2022, Intel Corporation.
 *
 * Authors:
 *  Haijun Liu <haijun.liu@mediatek.com>
 *  Eliot Lee <eliot.lee@intel.com>
 *  Moises Veleta <moises.veleta@intel.com>
 *  Ricardo Martinez<ricardo.martinez@linux.intel.com>
 *
 * Contributors:
 *  Amir Hanania <amir.hanania@intel.com>
 *  Sreehari Kancharla <sreehari.kancharla@intel.com>
 */

#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "t7xx_hif_cldma.h"
#include "t7xx_mhccif.h"
#include "t7xx_modem_ops.h"
#include "t7xx_pci.h"
#include "t7xx_pcie_mac.h"
#include "t7xx_port_proxy.h"
#include "t7xx_reg.h"
#include "t7xx_state_monitor.h"
#include "t7xx_pci_rescan.h"
#include "t7xx_netdev.h"
#include "t7xx_port_devlink.h"
#include "t7xx_uevent.h"

#define FSM_DRM_DISABLE_DELAY_MS		200
#define FSM_EVENT_POLL_INTERVAL_MS		20
#define FSM_MD_EX_REC_OK_TIMEOUT_MS		10000
#define FSM_MD_EX_PASS_TIMEOUT_MS		45000
#define FSM_CMD_TIMEOUT_MS			2000

#define HOST_EVENT_MASK 0xFFFF0000
#define HOST_EVENT_OFFSET 28

static int count;
static int brom_dl_flag = 1;
bool da_down_stage_flag = true;
extern int fastboot_dl_mode;


enum host_event_e {
	HOST_EVENT_INIT = 0,
	BROM_REBOOT_ACK = 0x1,
	BROM_DLPT_NOTY = 0x2,
	FASTBOOT_DL_NOTY = 0x3,
};

void t7xx_fsm_notifier_register(struct t7xx_modem *md, struct t7xx_fsm_notifier *notifier)
{
	struct t7xx_fsm_ctl *ctl = md->fsm_ctl;
	unsigned long flags;

	spin_lock_irqsave(&ctl->notifier_lock, flags);
	list_add_tail(&notifier->entry, &ctl->notifier_list);
	spin_unlock_irqrestore(&ctl->notifier_lock, flags);
}

void t7xx_fsm_notifier_unregister(struct t7xx_modem *md, struct t7xx_fsm_notifier *notifier)
{
	struct t7xx_fsm_notifier *notifier_cur, *notifier_next;
	struct t7xx_fsm_ctl *ctl = md->fsm_ctl;
	unsigned long flags;

	spin_lock_irqsave(&ctl->notifier_lock, flags);
	list_for_each_entry_safe(notifier_cur, notifier_next, &ctl->notifier_list, entry) {
		if (notifier_cur == notifier)
			list_del(&notifier->entry);
	}
	spin_unlock_irqrestore(&ctl->notifier_lock, flags);
}

static void fsm_state_notify(struct t7xx_modem *md, enum md_state state)
{
	struct t7xx_fsm_ctl *ctl = md->fsm_ctl;
	struct t7xx_fsm_notifier *notifier;
	unsigned long flags;

	spin_lock_irqsave(&ctl->notifier_lock, flags);
	list_for_each_entry(notifier, &ctl->notifier_list, entry) {
		spin_unlock_irqrestore(&ctl->notifier_lock, flags);
		if (notifier->notifier_fn)
			notifier->notifier_fn(state, notifier->data);

		spin_lock_irqsave(&ctl->notifier_lock, flags);
	}
	spin_unlock_irqrestore(&ctl->notifier_lock, flags);
}

void t7xx_fsm_broadcast_state(struct t7xx_fsm_ctl *ctl, enum md_state state)
{
	ctl->md_state = state;

	/* Update to port first, otherwise sending message on HS2 may fail */
	t7xx_port_proxy_md_status_notify(ctl->md->port_prox, state);
	fsm_state_notify(ctl->md, state);
}

static void fsm_finish_command(struct t7xx_fsm_ctl *ctl, struct t7xx_fsm_command *cmd, int result)
{
	if (cmd->flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETION) {
		*cmd->ret = result;
		complete_all(cmd->done);
	}

	kfree(cmd);
}

static void fsm_del_kf_event(struct t7xx_fsm_event *event)
{
	list_del(&event->entry);
	kfree(event);
}

static void fsm_flush_event_cmd_qs(struct t7xx_fsm_ctl *ctl)
{
	struct device *dev = &ctl->md->t7xx_dev->pdev->dev;
	struct t7xx_fsm_event *event, *evt_next;
	struct t7xx_fsm_command *cmd, *cmd_next;
	unsigned long flags;

	spin_lock_irqsave(&ctl->command_lock, flags);
	list_for_each_entry_safe(cmd, cmd_next, &ctl->command_queue, entry) {
		dev_warn(dev, "Unhandled command %d\n", cmd->cmd_id);
		list_del(&cmd->entry);
		fsm_finish_command(ctl, cmd, -EINVAL);
	}
	spin_unlock_irqrestore(&ctl->command_lock, flags);

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, evt_next, &ctl->event_queue, entry) {
		dev_warn(dev, "Unhandled event %d\n", event->event_id);
		fsm_del_kf_event(event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
}

static void fsm_wait_for_event(struct t7xx_fsm_ctl *ctl, enum t7xx_fsm_event_state event_expected,
			       enum t7xx_fsm_event_state event_ignore, int retries)
{
	struct t7xx_fsm_event *event;
	bool event_received = false;
	unsigned long flags;
	int cnt = 0;

	while (cnt++ < retries && !event_received) {
		bool sleep_required = true;

		if (kthread_should_stop())
			return;

		spin_lock_irqsave(&ctl->event_lock, flags);
		event = list_first_entry_or_null(&ctl->event_queue, struct t7xx_fsm_event, entry);
		if (event) {
			event_received = event->event_id == event_expected;
			if (event_received || event->event_id == event_ignore) {
				fsm_del_kf_event(event);
				sleep_required = false;
			}
		}
		spin_unlock_irqrestore(&ctl->event_lock, flags);

		if (sleep_required)
			msleep(FSM_EVENT_POLL_INTERVAL_MS);
	}
}

static void fsm_routine_exception(struct t7xx_fsm_ctl *ctl, struct t7xx_fsm_command *cmd,
				  enum t7xx_ex_reason reason)
{
	struct device *dev = &ctl->md->t7xx_dev->pdev->dev;

	if (ctl->curr_state != FSM_STATE_READY && ctl->curr_state != FSM_STATE_STARTING) {
		if (cmd)
			fsm_finish_command(ctl, cmd, -EINVAL);

		return;
	}

	ctl->curr_state = FSM_STATE_EXCEPTION;

	switch (reason) {
	case EXCEPTION_HS_TIMEOUT:
		dev_err(dev, "Boot Handshake failure\n");
		t7xx_uevent_send(&ctl->md->t7xx_dev->pdev->dev, T7XX_UEVENT_MODEM_BOOT_HS_FAIL);
		break;

	case EXCEPTION_EVENT:
		dev_err(dev, "Exception event\n");
		port_ee_disable_wwan();
		t7xx_fsm_broadcast_state(ctl, MD_STATE_EXCEPTION);
		t7xx_uevent_send(&ctl->md->t7xx_dev->pdev->dev, T7XX_UEVENT_MODEM_EXCEPTION);
		t7xx_pci_pm_exp_detected(ctl->md->t7xx_dev);
		t7xx_md_exception_handshake(ctl->md);

		fsm_wait_for_event(ctl, FSM_EVENT_MD_EX_REC_OK, FSM_EVENT_MD_EX,
				   FSM_MD_EX_REC_OK_TIMEOUT_MS / FSM_EVENT_POLL_INTERVAL_MS);
		fsm_wait_for_event(ctl, FSM_EVENT_MD_EX_PASS, FSM_EVENT_INVALID,
				   FSM_MD_EX_PASS_TIMEOUT_MS / FSM_EVENT_POLL_INTERVAL_MS);
		break;

	default:
		dev_err(dev, "Exception %d\n", reason);
		break;
	}

	if (cmd)
		fsm_finish_command(ctl, cmd, 0);
}

static void brom_event_monitor(struct timer_list *timer)
{
	struct t7xx_modem *md;
	unsigned int dummy_reg = 0;
	struct coprocessor_ctl *temp_cocore_ctl = container_of(timer,
		struct coprocessor_ctl, event_check_timer);
	struct t7xx_fsm_ctl *fsm_ctl = container_of(temp_cocore_ctl,
		struct t7xx_fsm_ctl, sap_state_ctl);

	md = fsm_ctl->md;

	/*store dummy register*/
	dummy_reg = ioread32(IREG_BASE(md->t7xx_dev) + T7XX_PCIE_MISC_DEV_STATUS);
	dummy_reg &= MISC_DEV_STATUS_MASK;
	if (dummy_reg != fsm_ctl->prev_status)
		t7xx_fsm_append_cmd(fsm_ctl, FSM_CMD_START, FSM_CMD_FLAG_IN_INTERRUPT);
	mod_timer(timer, jiffies + HZ / 20);
}

static inline void start_brom_event_start_timer(struct t7xx_fsm_ctl *ctl)
{
	if (!ctl->sap_state_ctl.event_check_timer.function) {
		timer_setup(&ctl->sap_state_ctl.event_check_timer, brom_event_monitor, 0);
		ctl->sap_state_ctl.event_check_timer.expires = jiffies + (HZ / 20); // 50ms
		add_timer(&ctl->sap_state_ctl.event_check_timer);
	} else {
		mod_timer(&ctl->sap_state_ctl.event_check_timer, jiffies + HZ / 20);
	}
}

static inline void stop_brom_event_start_timer(struct t7xx_fsm_ctl *ctl)
{
	if (ctl->sap_state_ctl.event_check_timer.function) {
		del_timer(&ctl->sap_state_ctl.event_check_timer);
		ctl->sap_state_ctl.event_check_timer.expires = 0;
		ctl->sap_state_ctl.event_check_timer.function = NULL;
	}
}

static inline void host_event_notify(struct t7xx_modem *md, unsigned int event_id)
{
	u32 dummy_reg_val_temp = 0;

	dummy_reg_val_temp = ioread32(IREG_BASE(md->t7xx_dev) + T7XX_PCIE_MISC_DEV_STATUS);
	dummy_reg_val_temp &= ~HOST_EVENT_MASK;
	dummy_reg_val_temp |= event_id << HOST_EVENT_OFFSET;
	iowrite32(dummy_reg_val_temp, IREG_BASE(md->t7xx_dev) + T7XX_PCIE_MISC_DEV_STATUS);
}

static inline int brom_stage_event_handling(struct t7xx_fsm_ctl *ctl, unsigned int dummy_reg)
{
	unsigned char brom_event;
	struct t7xx_port *dl_port = NULL;
	struct t7xx_port_static *dl_port_static = NULL;
	struct t7xx_modem *md = ctl->md;
	struct device *dev;
	struct cldma_ctrl *md_ctrl;
	int ret = 0;

	dev = &md->t7xx_dev->pdev->dev;

	/*get brom event id*/
	brom_event = (dummy_reg & BROM_EVENT_MASK) >> 4;
	dev_info(dev, "device event: %x\n", brom_event);

	switch (brom_event) {
	case BROM_EVENT_NORMAL:
	case BROM_EVENT_JUMP_BL:
	case BROM_EVENT_TIME_OUT:
		dev_info(dev, "Non Download Mode\n");
		dev_info(dev, "jump next stage\n");
		dl_port = port_get_by_name("brom_download");
		if (dl_port) {
			dl_port_static = dl_port->port_static;
			dl_port_static->ops->disable_chl(dl_port);
		} else {
			dev_err(dev, "can't find DL port\n");
			ret = -EINVAL;
		}

		start_brom_event_start_timer(ctl);
		break;
	case BROM_EVENT_JUMP_DA:
		dev_info(dev, "jump DA and wait reset signal\n");
		da_down_stage_flag = false;
		host_event_notify(md, 2);
		start_brom_event_start_timer(ctl);
		dev_info(dev, "Device enter DA from Brom stage\n");
		break;
	case BROM_EVENT_CREATE_DL_PORT:
		dev_info(dev, "create DL port,brom_dl_flag:%d\n", brom_dl_flag);
		da_down_stage_flag = true;
		md_ctrl = ctl->md->md_ctrl[CLDMA_ID_AP];
		t7xx_cldma_hif_hw_init(md_ctrl);
		t7xx_cldma_stop(md_ctrl);
		t7xx_cldma_switch_cfg(md_ctrl, HIF_CFG2);
		dl_port = port_get_by_name("brom_download");
		if (dl_port) {
			if (brom_dl_flag) {
				dl_port_static = dl_port->port_static;
				dl_port_static->ops->enable_chl(dl_port);
				t7xx_cldma_start(md_ctrl);
			}
		} else {
			dev_err(dev, "can't find DL port\n");
			ret = -EINVAL;
		}
		/*start brom event check thread */
		start_brom_event_start_timer(ctl);
		break;
	case BROM_EVENT_RESET:
		pr_info("BROM EVENT reset received ..\n");
		host_event_notify(ctl->md, 1);
		mtk_queue_rescan_work(ctl->md->t7xx_dev->pdev);
		break;
	default:
		dev_err(dev, "Brom Event region content error\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static inline void lk_stage_event_handling(struct t7xx_fsm_ctl *ctl,
        unsigned int dummy_reg)
{
        unsigned char lk_event;
        struct t7xx_port *pd_port;
        struct t7xx_port_static *pd_port_static;
	struct cldma_ctrl *md_ctrl;

        /*get lk event id*/
        lk_event = (dummy_reg & LK_EVENT_MASK)>>8;
        switch (lk_event) {
        case LK_EVENT_NORMAL:
                pr_info("Device enter next stage from LK stage/n");
                break;
        case LK_EVENT_CREATE_PD_PORT:
	case LK_EVENT_CREATE_POST_DL_PORT:
		md_ctrl = ctl->md->md_ctrl[CLDMA_ID_AP];
                t7xx_cldma_hif_hw_init(md_ctrl);
                t7xx_cldma_stop(md_ctrl);
		t7xx_cldma_switch_cfg(md_ctrl, HIF_CFG2);
		port_switch_cfg(ctl->md, PORT_CFG1);
		pr_info("creating the ttyDUMP port.. \n");
                pd_port = port_get_by_name("ttyDUMP");
		if (pd_port != NULL) {
			if (lk_event == LK_EVENT_CREATE_PD_PORT)
				pd_port->dl->mode = T7XX_FB_DUMP_MODE;
			else
				pd_port->dl->mode = T7XX_FB_DL_MODE;
			pd_port_static = pd_port->port_static;
			pd_port_static->ops->enable_chl(pd_port);
			t7xx_cldma_start(md_ctrl);
		} else
			dev_err(&ctl->md->t7xx_dev->pdev->dev,
				"can't find PD port/n");
		if (lk_event == LK_EVENT_CREATE_PD_PORT)
			t7xx_uevent_send(&ctl->md->t7xx_dev->pdev->dev,
					 T7XX_UEVENT_MODEM_FASTBOOT_DUMP_MODE);
		else
			t7xx_uevent_send(&ctl->md->t7xx_dev->pdev->dev,
					 T7XX_UEVENT_MODEM_FASTBOOT_DL_MODE);
                break;
        case LK_EVENT_RESET:
                /*send r&r event to r&r thread*/
                break;
        default:
                pr_err("Brom Event region content error\n");
                break;
        }
}

static int fsm_stopped_handler(struct t7xx_fsm_ctl *ctl)
{
	ctl->curr_state = FSM_STATE_STOPPED;

	t7xx_fsm_broadcast_state(ctl, MD_STATE_STOPPED);
	pr_info("modem stopped");
	return t7xx_md_reset(ctl->md->t7xx_dev);
}

static void fsm_routine_stopped(struct t7xx_fsm_ctl *ctl, struct t7xx_fsm_command *cmd)
{
	/*remove timer to avoid pre start again*/
	stop_brom_event_start_timer(ctl);

	if (ctl->curr_state == FSM_STATE_STOPPED) {
		fsm_finish_command(ctl, cmd, -EINVAL);
		return;
	}

	fsm_finish_command(ctl, cmd, fsm_stopped_handler(ctl));
}

static void fsm_routine_stopping(struct t7xx_fsm_ctl *ctl, struct t7xx_fsm_command *cmd)
{
	struct t7xx_pci_dev *t7xx_dev;
	struct cldma_ctrl *md_ctrl;
	int err;

	if (ctl->curr_state == FSM_STATE_STOPPED || ctl->curr_state == FSM_STATE_STOPPING) {
		fsm_finish_command(ctl, cmd, -EINVAL);
		return;
	}

	md_ctrl = ctl->md->md_ctrl[CLDMA_ID_MD];
	t7xx_dev = ctl->md->t7xx_dev;

	ctl->curr_state = FSM_STATE_STOPPING;
	t7xx_fsm_broadcast_state(ctl, MD_STATE_WAITING_TO_STOP);
	t7xx_cldma_stop(md_ctrl);

	if (!ctl->md->rgu_irq_asserted) {

		if (fastboot_dl_mode == 1) {
			host_event_notify(ctl->md, FASTBOOT_DL_NOTY);
		}

		t7xx_mhccif_h2d_swint_trigger(t7xx_dev, H2D_CH_DRM_DISABLE_AP);
		/* Wait for the DRM disable to take effect */
		msleep(FSM_DRM_DISABLE_DELAY_MS);

		if (fastboot_dl_mode == 1) {
			/* not try fldr because device will always wait for
			 * MHCCIF bit 13 in fastboot download flow
			 */
			t7xx_mhccif_h2d_swint_trigger(t7xx_dev,
						      H2D_CH_DEVICE_RESET);
			fastboot_dl_mode = 0;
		} else {
			err = t7xx_acpi_fldr_func(t7xx_dev);
			if (err)
				t7xx_mhccif_h2d_swint_trigger(t7xx_dev,
							      H2D_CH_DEVICE_RESET);
		}
	}

	fsm_finish_command(ctl, cmd, fsm_stopped_handler(ctl));
}

static void t7xx_fsm_broadcast_ready_state(struct t7xx_fsm_ctl *ctl)
{
	if (ctl->md_state != MD_STATE_WAITING_FOR_HS2)
		return;

	ctl->md_state = MD_STATE_READY;

	fsm_state_notify(ctl->md, MD_STATE_READY);
	t7xx_port_proxy_md_status_notify(ctl->md->port_prox, MD_STATE_READY);
}

static void fsm_routine_ready(struct t7xx_fsm_ctl *ctl)
{
	struct t7xx_modem *md = ctl->md;

	ctl->curr_state = FSM_STATE_READY;
	t7xx_fsm_broadcast_ready_state(ctl);
	t7xx_uevent_send(&ctl->md->t7xx_dev->pdev->dev,
			 T7XX_UEVENT_MODEM_READY);
	t7xx_md_event_notify(md, FSM_READY);
}

static int fsm_routine_starting(struct t7xx_fsm_ctl *ctl)
{
	struct t7xx_modem *md = ctl->md;
	struct device *dev;

	ctl->curr_state = FSM_STATE_STARTING;

	port_switch_cfg(md, PORT_CFG0);
	t7xx_fsm_broadcast_state(ctl, MD_STATE_WAITING_FOR_HS1);
	t7xx_uevent_send(&ctl->md->t7xx_dev->pdev->dev,
			 T7XX_UEVENT_MODEM_WAITING_HS1);
	t7xx_md_event_notify(md, FSM_START);

	wait_event_interruptible_timeout(ctl->async_hk_wq,
					 (md->core_md.ready && md->core_sap.ready) ||
					 ctl->exp_flg, HZ * 60);
	dev = &md->t7xx_dev->pdev->dev;

	if (ctl->exp_flg)
		dev_err(dev, "MD exception is captured during handshake\n");

	if (!md->core_md.ready) {
		dev_err(dev, "MD handshake timeout\n");
		if (md->core_md.handshake_ongoing)
			t7xx_fsm_append_event(ctl, FSM_EVENT_MD_HS2_EXIT, NULL, 0);

		fsm_routine_exception(ctl, NULL, EXCEPTION_HS_TIMEOUT);
		return -ETIMEDOUT;
	}

	t7xx_pci_pm_init_late(md->t7xx_dev);
	fsm_routine_ready(ctl);
	return 0;
}

static void fsm_routine_start(struct t7xx_fsm_ctl *ctl, struct t7xx_fsm_command *cmd)
{
	struct t7xx_modem *md = ctl->md;
	unsigned char device_stage;
	struct cldma_ctrl *md_ctrl;
	struct device *dev;
	int cmd_ret = 0;
	u32 dev_status;

	if (!md)
		return;

	if (ctl->curr_state != FSM_STATE_INIT && ctl->curr_state != FSM_STATE_PRE_START &&
	    ctl->curr_state != FSM_STATE_STOPPED) {
		fsm_finish_command(ctl, cmd, -EINVAL);
		return;
	}

	dev = &md->t7xx_dev->pdev->dev;
	dev_status = ioread32(IREG_BASE(md->t7xx_dev) + T7XX_PCIE_MISC_DEV_STATUS);
	dev_status &= MISC_DEV_STATUS_MASK;
	dev_info(dev, "dev_status = %x modem state = %d\n", dev_status, ctl->md_state);
	if (dev_status == MISC_DEV_STATUS_MASK) {
		dev_err(dev, "invalid device status\n");
		fsm_finish_command(ctl, cmd, -1);
		ctl->prev_status = dev_status;
		return;
	}

	ctl->curr_state = FSM_STATE_PRE_START;
	t7xx_md_event_notify(md, FSM_PRE_START);
	port_switch_cfg(md, PORT_CFG1);

	if (dev_status != ctl->prev_status) {
		device_stage = dev_status & MISC_STAGE_MASK;
		switch (device_stage) {
		case INIT_STAGE:
			cmd_ret = t7xx_fsm_append_cmd(ctl, FSM_CMD_START, 0);
			break;
		case BROM_STAGE_PRE:
		case BROM_STAGE_POST:
			cmd_ret = brom_stage_event_handling(ctl, dev_status);
			break;
		case LK_STAGE:
                        da_down_stage_flag = false;
                        stop_brom_event_start_timer(ctl);
                        lk_stage_event_handling(ctl, dev_status);
                        break;
		case LINUX_STAGE:
			da_down_stage_flag = false;
			stop_brom_event_start_timer(ctl);
			md_ctrl = ctl->md->md_ctrl[CLDMA_ID_AP];
			t7xx_cldma_hif_hw_init(md_ctrl);
			md_ctrl = ctl->md->md_ctrl[CLDMA_ID_MD];
			t7xx_cldma_hif_hw_init(md_ctrl);
			cmd_ret = fsm_routine_starting(ctl);
			break;
		default:
			break;
		}
		ctl->prev_status = dev_status;
		count = 0;
	} else {
		if (dev_status == 0) {
			if (count++ >= 200) {
				count = 0;
			} else {
				msleep(100);
				t7xx_fsm_append_cmd(ctl, FSM_CMD_START, 0);
			}
		} else {
			count = 0;
		}
	}

	fsm_finish_command(ctl, cmd, cmd_ret);
}

static int fsm_main_thread(void *data)
{
	struct t7xx_fsm_ctl *ctl = data;
	struct t7xx_fsm_command *cmd;
	unsigned long flags;

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(ctl->command_wq, !list_empty(&ctl->command_queue) ||
					     kthread_should_stop()))
			continue;

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&ctl->command_lock, flags);
		cmd = list_first_entry(&ctl->command_queue, struct t7xx_fsm_command, entry);
		list_del(&cmd->entry);
		spin_unlock_irqrestore(&ctl->command_lock, flags);

		switch (cmd->cmd_id) {
		case FSM_CMD_START:
			fsm_routine_start(ctl, cmd);
			break;

		case FSM_CMD_EXCEPTION:
			fsm_routine_exception(ctl, cmd, FIELD_GET(FSM_CMD_EX_REASON, cmd->flag));
			break;

		case FSM_CMD_PRE_STOP:
			fsm_routine_stopping(ctl, cmd);
			break;

		case FSM_CMD_STOP:
			fsm_routine_stopped(ctl, cmd);
			break;

		default:
			fsm_finish_command(ctl, cmd, -EINVAL);
			fsm_flush_event_cmd_qs(ctl);
			break;
		}
	}

	return 0;
}

int t7xx_fsm_append_cmd(struct t7xx_fsm_ctl *ctl, enum t7xx_fsm_cmd_state cmd_id, unsigned int flag)
{
	DECLARE_COMPLETION_ONSTACK(done);
	struct t7xx_fsm_command *cmd;
	unsigned long flags;
	int ret;

	cmd = kzalloc(sizeof(*cmd), flag & FSM_CMD_FLAG_IN_INTERRUPT ? GFP_ATOMIC : GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	INIT_LIST_HEAD(&cmd->entry);
	cmd->cmd_id = cmd_id;
	cmd->flag = flag;
	if (flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETION) {
		cmd->done = &done;
		cmd->ret = &ret;
	}

	spin_lock_irqsave(&ctl->command_lock, flags);
	list_add_tail(&cmd->entry, &ctl->command_queue);
	spin_unlock_irqrestore(&ctl->command_lock, flags);

	wake_up(&ctl->command_wq);

	if (flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETION) {
		unsigned long wait_ret;

		wait_ret = wait_for_completion_timeout(&done,
						       msecs_to_jiffies(FSM_CMD_TIMEOUT_MS));
		if (!wait_ret)
			return -ETIMEDOUT;

		return ret;
	}

	return 0;
}

int t7xx_fsm_append_event(struct t7xx_fsm_ctl *ctl, enum t7xx_fsm_event_state event_id,
			  unsigned char *data, unsigned int length)
{
	struct device *dev = &ctl->md->t7xx_dev->pdev->dev;
	struct t7xx_fsm_event *event;
	unsigned long flags;

	if (event_id <= FSM_EVENT_INVALID || event_id >= FSM_EVENT_MAX) {
		dev_err(dev, "Invalid event %d\n", event_id);
		return -EINVAL;
	}

	event = kmalloc(sizeof(*event) + length, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	INIT_LIST_HEAD(&event->entry);
	event->event_id = event_id;
	event->length = length;

	if (data && length)
		memcpy((void *)event + sizeof(*event), data, length);

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_add_tail(&event->entry, &ctl->event_queue);
	spin_unlock_irqrestore(&ctl->event_lock, flags);

	wake_up_all(&ctl->event_wq);
	return 0;
}

void t7xx_fsm_clr_event(struct t7xx_fsm_ctl *ctl, enum t7xx_fsm_event_state event_id)
{
	struct t7xx_fsm_event *event, *evt_next;
	unsigned long flags;

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, evt_next, &ctl->event_queue, entry) {
		if (event->event_id == event_id)
			fsm_del_kf_event(event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
}

enum md_state t7xx_fsm_get_md_state(struct t7xx_fsm_ctl *ctl)
{
	if (ctl)
		return ctl->md_state;

	return MD_STATE_INVALID;
}

unsigned int t7xx_fsm_get_ctl_state(struct t7xx_fsm_ctl *ctl)
{
	if (ctl)
		return ctl->curr_state;

	return FSM_STATE_STOPPED;
}

int t7xx_fsm_recv_md_intr(struct t7xx_fsm_ctl *ctl, enum t7xx_md_irq_type type)
{
	unsigned int cmd_flags = FSM_CMD_FLAG_IN_INTERRUPT;

	if (type == MD_IRQ_PORT_ENUM) {
		return t7xx_fsm_append_cmd(ctl, FSM_CMD_START, cmd_flags);
	} else if (type == MD_IRQ_CCIF_EX) {
		ctl->exp_flg = true;
		wake_up(&ctl->async_hk_wq);
		cmd_flags |= FIELD_PREP(FSM_CMD_EX_REASON, EXCEPTION_EVENT);
		return t7xx_fsm_append_cmd(ctl, FSM_CMD_EXCEPTION, cmd_flags);
	}

	return -EINVAL;
}

void t7xx_fsm_reset(struct t7xx_modem *md)
{
	struct t7xx_fsm_ctl *ctl = md->fsm_ctl;

	fsm_flush_event_cmd_qs(ctl);
	ctl->curr_state = FSM_STATE_STOPPED;
	ctl->exp_flg = false;
	ctl->prev_status = 0;
}

int t7xx_fsm_init(struct t7xx_modem *md)
{
	struct device *dev = &md->t7xx_dev->pdev->dev;
	struct t7xx_fsm_ctl *ctl;

	ctl = devm_kzalloc(dev, sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return -ENOMEM;

	md->fsm_ctl = ctl;
	ctl->md = md;
	ctl->curr_state = FSM_STATE_INIT;
	INIT_LIST_HEAD(&ctl->command_queue);
	INIT_LIST_HEAD(&ctl->event_queue);
	init_waitqueue_head(&ctl->async_hk_wq);
	init_waitqueue_head(&ctl->event_wq);
	INIT_LIST_HEAD(&ctl->notifier_list);
	init_waitqueue_head(&ctl->command_wq);
	spin_lock_init(&ctl->event_lock);
	spin_lock_init(&ctl->command_lock);
	ctl->exp_flg = false;
	spin_lock_init(&ctl->notifier_lock);

	ctl->fsm_thread = kthread_run(fsm_main_thread, ctl, "t7xx_fsm");
	return PTR_ERR_OR_ZERO(ctl->fsm_thread);
}

void t7xx_fsm_uninit(struct t7xx_modem *md)
{
	struct t7xx_fsm_ctl *ctl = md->fsm_ctl;

	if (!ctl)
		return;

	if (ctl->fsm_thread) {
		stop_brom_event_start_timer(ctl);
		kthread_stop(ctl->fsm_thread);
	}

	fsm_flush_event_cmd_qs(ctl);
}
