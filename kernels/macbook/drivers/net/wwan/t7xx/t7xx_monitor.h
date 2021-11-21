/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __T7XX_MONITOR_H__
#define __T7XX_MONITOR_H__

#include <linux/cdev.h>

#include "t7xx_common.h"
#include "t7xx_monitor.h"
#include "t7xx_buffer_manager.h"

enum ccci_fsm_state {
	CCCI_FSM_INIT = 0,
	CCCI_FSM_PRE_STARTING,
	CCCI_FSM_STARTING,
	CCCI_FSM_READY,
	CCCI_FSM_EXCEPTION,
	CCCI_FSM_STOPPING,
	CCCI_FSM_STOPPED,
};

enum ccci_fsm_event_state {
	CCCI_EVENT_INVALID = 0,
	CCCI_EVENT_MD_HS2,
	CCCI_EVENT_MD_EX,
	CCCI_EVENT_MD_EX_REC_OK,
	CCCI_EVENT_MD_EX_PASS,
	CCCI_EVENT_MD_HS2_EXIT,
	CCCI_EVENT_AP_HS2_EXIT,
	CCCI_EVENT_MAX,
};

enum ccci_fsm_cmd_state {
	CCCI_COMMAND_INVALID = 0,
	CCCI_COMMAND_PRE_START,
	CCCI_COMMAND_PRE_STOP,	/* from other modules */
	CCCI_COMMAND_STOP,	/* from other modules */
	CCCI_COMMAND_EE,	/* from MD */
	CCCI_COMMAND_MD_HANG,	/* from status polling thread */
	CCCI_COMMAND_MAX,
};

enum ccci_ee_reason {
	EXCEPTION_INVALID = 0,
	EXCEPTION_HS_TIMEOUT,
	EXCEPTION_EE,
	EXCEPTION_MAX,
};

enum ccci_fsm_poller_state {
	FSM_POLLER_INVALID = 0,
	FSM_POLLER_WAITING_RESPONSE,
	FSM_POLLER_RECEIVED_RESPONSE,
};

enum ccci_md_msg {
	CCCI_MD_MSG_FORCE_STOP_REQUEST = 0xFAF50001,
	CCCI_MD_MSG_FLIGHT_STOP_REQUEST,
	CCCI_MD_MSG_FORCE_START_REQUEST,
	CCCI_MD_MSG_FLIGHT_START_REQUEST,
	CCCI_MD_MSG_RESET_REQUEST,

	CCCI_MD_MSG_EXCEPTION,
	CCCI_MD_MSG_SEND_BATTERY_INFO,
	CCCI_MD_MSG_STORE_NVRAM_MD_TYPE,
	CCCI_MD_MSG_CFG_UPDATE,
	CCCI_MD_MSG_RANDOM_PATTERN,
};

enum {
	/* also used to check EE flow done */
	MD_EE_FLOW_START	= BIT(0),
	MD_EE_MSG_GET		= BIT(1),
	MD_EE_OK_MSG_GET	= BIT(2),
	MD_EE_PASS_MSG_GET	= BIT(3),
	MD_EE_PENDING_TOO_LONG	= BIT(4),
	MD_EE_WDT_GET		= BIT(5),
	MD_EE_DUMP_IN_GPD	= BIT(6),
	MD_EE_SWINT_GET		= BIT(7),
};

enum md_irq_type {
	MD_IRQ_WDT,
	MD_IRQ_CCIF_EX,
	MD_IRQ_PORT_ENUM,
};

#define FSM_NAME "ccci_fsm"
#define FSM_CMD_FLAG_WAIT_FOR_COMPLETE BIT(0)
#define FSM_CMD_FLAG_FLIGHT_MODE BIT(1)

#define EVENT_POLL_INTEVAL 20 /* ms */
#define MD_EX_CCIF_TIMEOUT 10000
#define MD_EX_REC_OK_TIMEOUT 10000
#define MD_EX_PASS_TIMEOUT (45 * 1000)
#define EE_DONE_TIMEOUT 30 /* s */

#define MD_EX_START_TIME_LEN 128

struct ccci_fsm_poller {
	enum ccci_fsm_poller_state poller_state;
	wait_queue_head_t status_rx_wq;
};

struct ccci_fsm_ee;

struct ccci_fsm_ee {
	unsigned int ee_info_flag;
	spinlock_t ctrl_lock; /* protects ee_info_flag */
	char ex_start_time[MD_EX_START_TIME_LEN];
	unsigned int mdlog_dump_done;
	unsigned int ex_flag;
	char *ex_buff;
	u16 port_maintain_sec;
	u16 port_en;
};

struct ccci_fsm_monitor {
	dev_t dev_n;
	struct cdev *char_dev;
	atomic_t usage_cnt;
	wait_queue_head_t rx_wq;
	struct ccci_skb_queue rx_skb_list;
};

struct ccci_fsm_ctl {
	struct ccci_modem *md;
	enum MD_STATE md_state;
	unsigned int curr_state;
	unsigned int last_state;
	u32 last_dummy_reg;
	struct list_head command_queue;
	struct list_head event_queue;
	wait_queue_head_t command_wq;
	wait_queue_head_t event_wq;
	wait_queue_head_t async_hk_wq;
	spinlock_t event_lock;		/* protects event_queue */
	spinlock_t command_lock;	/* protects command_queue */
	spinlock_t cmd_complete_lock;	/* protects fsm_command */
	struct task_struct *fsm_thread;
	atomic_t fs_ongoing;
	atomic_t exp_flg;
	struct ccci_fsm_poller poller_ctl;
	struct ccci_fsm_ee ee_ctl;
	struct ccci_fsm_monitor monitor_ctl;
	spinlock_t notifier_lock;	/* protects notifier_list */
	struct list_head notifier_list;
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
	int complete; /* -1: fail; 0: on-going; 1: success */
	wait_queue_head_t complete_wq;
};

struct fsm_notifier_block {
	struct list_head entry;
	int (*notifier_fn)(enum MD_STATE state, void *data);
	void *data;
};

int fsm_append_command(struct ccci_fsm_ctl *ctl, enum ccci_fsm_cmd_state cmd_id,
		       unsigned int flag);
int fsm_append_event(struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id,
		     unsigned char *data, unsigned int length);
void fsm_clear_event(struct ccci_fsm_ctl *ctl, enum ccci_fsm_event_state event_id);

void fsm_ee_reset(struct ccci_fsm_ee *ee_ctl);

struct ccci_fsm_ctl *fsm_get_entity_by_device_number(dev_t dev_n);
struct ccci_fsm_ctl *fsm_get_entry(void);

void fsm_md_exception_stage(struct ccci_fsm_ee *ee_ctl, int stage);
void fsm_ee_message_handler(struct ccci_fsm_ee *ee_ctl, struct sk_buff *skb);
int fsm_check_ee_done(struct ccci_fsm_ee *ee_ctl, struct ccci_modem *md, int timeout);
void mdee_set_ex_start_str(struct ccci_fsm_ee *ee_ctl,
			   unsigned int type, char *str);
void fsm_broadcast_state(struct ccci_fsm_ctl *ctl,
			 enum MD_STATE state);
int ccci_fsm_init(struct ccci_modem *md);
int ccci_fsm_reset(void);
void ccci_fsm_uninit(void);
int ccci_fsm_recv_status_packet(struct sk_buff *skb);
int ccci_fsm_recv_md_interrupt(enum md_irq_type type);
enum MD_STATE ccci_fsm_get_md_state(void);
unsigned int ccci_fsm_get_current_state(void);
int fsm_notifier_register(struct fsm_notifier_block *notifier);
int fsm_notifier_unregister(struct fsm_notifier_block *notifier);

#endif /* __T7XX_MONITOR_H__ */
