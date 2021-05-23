/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */


#ifndef __RTW_RM_H_
#define __RTW_RM_H_

u8 rm_post_event_hdl(struct adapter *adapt, u8 *pbuf);

#define RM_TIMER_NUM 		32
#define RM_ALL_MEAS		BIT(1)
#define RM_ID_FOR_ALL(aid)	((aid<<16)|RM_ALL_MEAS)

#define RM_CAP_ARG(x) ((u8 *)(x))[4], ((u8 *)(x))[3], ((u8 *)(x))[2], ((u8 *)(x))[1], ((u8 *)(x))[0]
#define RM_CAP_FMT "%02x %02x%02x %02x%02x"

/* remember to modify rm_event_name() when adding new event */
enum RM_EV_ID {
	RM_EV_state_in,
	RM_EV_busy_timer_expire,
	RM_EV_delay_timer_expire,
	RM_EV_meas_timer_expire,
	RM_EV_retry_timer_expire,
	RM_EV_repeat_delay_expire,
	RM_EV_request_timer_expire,
	RM_EV_wait_report,
	RM_EV_start_meas,
	RM_EV_survey_done,
	RM_EV_recv_rep,
	RM_EV_cancel,
	RM_EV_state_out,
	RM_EV_max
};

struct rm_event {
	u32 rmid;
	enum RM_EV_ID evid;
	struct list_head list;
};

#endif /* __RTW_RM_H_ */
