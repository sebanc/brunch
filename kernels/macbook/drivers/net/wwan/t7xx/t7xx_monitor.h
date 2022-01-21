/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_MONITOR_H__
#define __T7XX_MONITOR_H__

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "t7xx_common.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_skb_util.h"

enum ccci_fsm_state {
	CCCI_FSM_INIT,
	CCCI_FSM_PRE_START,
	CCCI_FSM_STARTING,
	CCCI_FSM_READY,
	CCCI_FSM_EXCEPTION,
	CCCI_FSM_STOPPING,
	CCCI_FSM_STOPPED,
};

enum ccci_fsm_event_state {
	CCCI_EVENT_INVALID,
	CCCI_EVENT_MD_HS2,
	CCCI_EVENT_SAP_HS2,
	CCCI_EVENT_MD_EX,
	CCCI_EVENT_MD_EX_REC_OK,
	CCCI_EVENT_MD_EX_PASS,
	CCCI_EVENT_MD_HS2_EXIT,
	CCCI_EVENT_AP_HS2_EXIT,
	CCCI_EVENT_MAX
};

enum ccci_fsm_cmd_state {
	CCCI_COMMAND_INVALID,
	CCCI_COMMAND_START,
	CCCI_COMMAND_EXCEPTION,
	CCCI_COMMAND_PRE_STOP,
	CCCI_COMMAND_STOP,
};

enum ccci_ex_reason {
	EXCEPTION_HS_TIMEOUT,
	EXCEPTION_EE,
	EXCEPTION_EVENT,
};

enum md_irq_type {
	MD_IRQ_WDT,
	MD_IRQ_CCIF_EX,
	MD_IRQ_PORT_ENUM,
};

enum fsm_cmd_result {
	FSM_CMD_RESULT_PENDING,
	FSM_CMD_RESULT_OK,
	FSM_CMD_RESULT_FAIL,
};

#define FSM_CMD_FLAG_WAITING_TO_COMPLETE	BIT(0)
#define FSM_CMD_FLAG_FLIGHT_MODE		BIT(1)

#define EVENT_POLL_INTERVAL_MS			20
#define MD_EX_REC_OK_TIMEOUT_MS			10000
#define MD_EX_PASS_TIMEOUT_MS			(45 * 1000)

struct ccci_fsm_monitor {
	dev_t dev_n;
	wait_queue_head_t rx_wq;
	struct ccci_skb_queue rx_skb_list;
};

struct coprocessor_ctl {
	unsigned int last_dummy_reg;
	struct timer_list event_check_timer;
};

struct ccci_fsm_ctl {
	struct mtk_modem *md;
	enum md_state md_state;
	unsigned int curr_state;
	unsigned int last_state;
	u32 prev_status;
	struct list_head command_queue;
	struct list_head event_queue;
	wait_queue_head_t command_wq;
	wait_queue_head_t event_wq;
	wait_queue_head_t async_hk_wq;
	spinlock_t event_lock;		/* protects event_queue */
	spinlock_t command_lock;	/* protects command_queue */
	spinlock_t cmd_complete_lock;	/* protects fsm_command */
	struct task_struct *fsm_thread;
	atomic_t exp_flg;
	struct ccci_fsm_monitor monitor_ctl;
	spinlock_t notifier_lock;	/* protects notifier_list */
	struct list_head notifier_list;
	struct coprocessor_ctl sap_state_ctl;
};

struct ccci_fsm_event {
	struct list_head entry;
	enum ccci_fsm_event_state event_id;
	unsigned int length;
	unsigned char data[];
};

struct ccci_fsm_command {
	struct list_head entry;
	enum ccci_fsm_cmd_state cmd_id;
	unsigned int flag;
	enum fsm_cmd_result result;
	wait_queue_head_t complete_wq;
};

struct fsm_notifier_block {
	struct list_head entry;
	int (*notifier_fn)(enum md_state state, void *data);
	void *data;
};

int fsm_append_command(struct ccci_fsm_ctl *ctl, enum ccci_fsm_cmd_state cmd_id,
		       unsigned int flag);
int fsm_append_event(struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id,
		     unsigned char *data, unsigned int length);
void fsm_clear_event(struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id);

struct ccci_fsm_ctl *fsm_get_entity_by_device_number(dev_t dev_n);
struct ccci_fsm_ctl *fsm_get_entry(void);

void fsm_broadcast_state(struct ccci_fsm_ctl *ctl, enum md_state state);
void ccci_fsm_reset(void);
int ccci_fsm_init(struct mtk_modem *md);
void ccci_fsm_uninit(void);
void ccci_fsm_recv_md_interrupt(enum md_irq_type type);
enum md_state ccci_fsm_get_md_state(void);
unsigned int ccci_fsm_get_current_state(void);
void fsm_notifier_register(struct fsm_notifier_block *notifier);
void fsm_notifier_unregister(struct fsm_notifier_block *notifier);

#endif /* __T7XX_MONITOR_H__ */
