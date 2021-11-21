// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/delay.h>

#include "t7xx_monitor.h"
#include "t7xx_modem_ops.h"
#include "t7xx_port_proxy.h"
#include "t7xx_port.h"

void mdee_set_ex_start_str(struct ccci_fsm_ee *ee_ctl,
			   unsigned int type, char *str)
{
	unsigned long rem_nsec;
	u64 ts_nsec;

	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	snprintf(ee_ctl->ex_start_time,
		 MD_EX_START_TIME_LEN, "Host detect handshake timeout:%5llu.%06lu\n",
		(unsigned long long)ts_nsec, rem_nsec / 1000);
	pr_notice("%s\n", ee_ctl->ex_start_time);
}

void fsm_md_exception_stage(struct ccci_fsm_ee *ee_ctl, int stage)
{
	unsigned long flags;

	if (stage == 0) { /* handshake fail */
		mdee_set_ex_start_str(ee_ctl, 0, NULL);
		ee_ctl->mdlog_dump_done = 0;
		ee_ctl->ee_info_flag = 0;
		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		ee_ctl->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_SWINT_GET);
		if (ccci_fsm_get_md_state() == BOOT_WAITING_FOR_HS1)
			ee_ctl->ee_info_flag |= MD_EE_DUMP_IN_GPD;
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
		ee_ctl->ex_flag = 1; /* set ex_flag */
	} else if (stage == 1) { /* got MD_EX_REC_OK or first timeout */
		pr_err("MD exception stage 1! ee=%x\n", ee_ctl->ee_info_flag);
	} else if (stage == 2) { /* got MD_EX_PASS or second timeout */
		pr_err("MD exception stage 2! ee=%x\n", ee_ctl->ee_info_flag);
		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		/* this flag should be the last action of a
		 * regular exception flow, clear flag for reset MD later
		 */
		ee_ctl->ee_info_flag = 0;
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
	}
}

void fsm_ee_message_handler(struct ccci_fsm_ee *ee_ctl, struct sk_buff *skb)
{
	struct ccci_fsm_ctl *ctl = container_of(ee_ctl, struct ccci_fsm_ctl, ee_ctl);
	struct ctrl_msg_header *ctrl_msg_h = (struct ctrl_msg_header *)skb->data;
	enum MD_STATE md_state = ccci_fsm_get_md_state();
	unsigned long flags;

	if (md_state != EXCEPTION) {
		pr_err("receive invalid MD_EX %x when MD state is %d\n",
		       ctrl_msg_h->reserved, md_state);
		return;
	}
	if (ctrl_msg_h->ctrl_msg_id == CTL_ID_MD_EX) {
		if (ctrl_msg_h->reserved != MD_EX_CHK_ID) {
			pr_err("receive invalid MD_EX %x\n", ctrl_msg_h->reserved);
		} else {
			spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
			ee_ctl->ee_info_flag |=
				(MD_EE_FLOW_START | MD_EE_MSG_GET);
			spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
			proxy_send_msg_to_md(CCCI_CONTROL_TX, CTL_ID_MD_EX,
					     MD_EX_CHK_ID, 1);
			fsm_append_event(ctl, CCCI_EVENT_MD_EX, NULL, 0);
		}
	} else if (ctrl_msg_h->ctrl_msg_id == CTL_ID_MD_EX_REC_OK) {
		if (ctrl_msg_h->reserved != MD_EX_REC_OK_CHK_ID) {
			pr_err("receive invalid MD_EX_REC_OK %x\n", ctrl_msg_h->reserved);
		} else {
			spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
			ee_ctl->ee_info_flag |= MD_EE_FLOW_START | MD_EE_OK_MSG_GET;
			spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
			fsm_append_event(ctl, CCCI_EVENT_MD_EX_REC_OK, NULL, 0);
		}
	} else if (ctrl_msg_h->ctrl_msg_id == CTL_ID_MD_EX_PASS) {
		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		ee_ctl->ee_info_flag |= MD_EE_PASS_MSG_GET;
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
		fsm_append_event(ctl, CCCI_EVENT_MD_EX_PASS, NULL, 0);
	} else if (ctrl_msg_h->ctrl_msg_id == CCCI_DRV_VER_ERROR) {
		pr_err("AP/MD driver version mismatch\n");
	}
}

int fsm_check_ee_done(struct ccci_fsm_ee *ee_ctl, struct ccci_modem *md, int timeout)
{
	int time_step = 200; /* ms */
	int loop_max = timeout * 1000 / time_step;
	bool is_ee_done;
	int count = 0;

	while (ccci_fsm_get_md_state() == EXCEPTION) {
		if (ccci_port_get_critical_user(CRIT_USR_MDLOG))
			is_ee_done = !(ee_ctl->ee_info_flag & MD_EE_FLOW_START) &&
				ee_ctl->mdlog_dump_done;
		else
			is_ee_done = !(ee_ctl->ee_info_flag & MD_EE_FLOW_START);

		if (!is_ee_done && atomic_read(&md->rgu_irq_asserted) == 0) {
			msleep(time_step);
			count++;
		} else {
			break;
		}

		if (loop_max && count > loop_max) {
			pr_err("wait EE done timeout\n");
			return -ETIMEDOUT;
		}
	}
	return 0;
}

void fsm_ee_reset(struct ccci_fsm_ee *ee_ctl)
{
	ee_ctl->port_en = 0;
	/* port time default align BOOTMISC0_CCCI_TIMEOUT (61) */
	ee_ctl->port_maintain_sec = 61;
	ee_ctl->ex_flag = 0;
	ee_ctl->ex_buff = NULL;
}
