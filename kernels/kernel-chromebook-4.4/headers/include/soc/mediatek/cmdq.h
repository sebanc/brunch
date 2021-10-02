/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CMDQ_H__
#define __MTK_CMDQ_H__

#include <linux/platform_device.h>
#include <linux/types.h>

enum cmdq_eng {
	CMDQ_ENG_DISP_AAL,
	CMDQ_ENG_DISP_COLOR0,
	CMDQ_ENG_DISP_COLOR1,
	CMDQ_ENG_DISP_DPI0,
	CMDQ_ENG_DISP_DSI0,
	CMDQ_ENG_DISP_DSI1,
	CMDQ_ENG_DISP_GAMMA,
	CMDQ_ENG_DISP_OD,
	CMDQ_ENG_DISP_OVL0,
	CMDQ_ENG_DISP_OVL1,
	CMDQ_ENG_DISP_PWM0,
	CMDQ_ENG_DISP_PWM1,
	CMDQ_ENG_DISP_RDMA0,
	CMDQ_ENG_DISP_RDMA1,
	CMDQ_ENG_DISP_RDMA2,
	CMDQ_ENG_DISP_UFOE,
	CMDQ_ENG_DISP_WDMA0,
	CMDQ_ENG_DISP_WDMA1,
	CMDQ_ENG_MAX,
};

/* events for CMDQ and display */
enum cmdq_event {
	/* Display start of frame(SOF) events */
	CMDQ_EVENT_DISP_OVL0_SOF = 11,
	CMDQ_EVENT_DISP_OVL1_SOF = 12,
	CMDQ_EVENT_DISP_RDMA0_SOF = 13,
	CMDQ_EVENT_DISP_RDMA1_SOF = 14,
	CMDQ_EVENT_DISP_RDMA2_SOF = 15,
	CMDQ_EVENT_DISP_WDMA0_SOF = 16,
	CMDQ_EVENT_DISP_WDMA1_SOF = 17,
	/* Display end of frame(EOF) events */
	CMDQ_EVENT_DISP_OVL0_EOF = 39,
	CMDQ_EVENT_DISP_OVL1_EOF = 40,
	CMDQ_EVENT_DISP_RDMA0_EOF = 41,
	CMDQ_EVENT_DISP_RDMA1_EOF = 42,
	CMDQ_EVENT_DISP_RDMA2_EOF = 43,
	CMDQ_EVENT_DISP_WDMA0_EOF = 44,
	CMDQ_EVENT_DISP_WDMA1_EOF = 45,
	/* Mutex end of frame(EOF) events */
	CMDQ_EVENT_MUTEX0_STREAM_EOF = 53,
	CMDQ_EVENT_MUTEX1_STREAM_EOF = 54,
	CMDQ_EVENT_MUTEX2_STREAM_EOF = 55,
	CMDQ_EVENT_MUTEX3_STREAM_EOF = 56,
	CMDQ_EVENT_MUTEX4_STREAM_EOF = 57,
	/* Display underrun events */
	CMDQ_EVENT_DISP_RDMA0_UNDERRUN = 63,
	CMDQ_EVENT_DISP_RDMA1_UNDERRUN = 64,
	CMDQ_EVENT_DISP_RDMA2_UNDERRUN = 65,
	/* Keep this at the end of HW events */
	CMDQ_MAX_HW_EVENT_COUNT = 260,
	/* This is max event and also can be used as mask. */
	CMDQ_SYNC_TOKEN_MAX = 0x1ff,
	/* Invalid event */
	CMDQ_SYNC_TOKEN_INVALID = -1,
};

/* called after isr done or task done */
typedef int (*cmdq_async_flush_cb)(void *data);

struct cmdq_task;
struct cmdq;

struct cmdq_rec {
	struct cmdq		*cqctx;
	u64			engine_flag;
	size_t			block_size; /* command size */
	void			*buf_ptr;
	size_t			buf_size;
	/* running task after flush */
	struct cmdq_task	*running_task_ptr;
	bool			finalized;
};

/**
 * cmdq_rec_create() - create command queue record handle
 * @dev:		device
 * @engine_flag:	command queue engine flag
 * @handle_ptr:		command queue record handle pointer to retrieve cmdq_rec
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_rec_create(struct device *dev, u64 engine_flag,
		    struct cmdq_rec **handle_ptr);

/**
 * cmdq_rec_reset() - reset command queue record commands
 * @handle:	the command queue record handle
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_rec_reset(struct cmdq_rec *handle);

/**
 * cmdq_rec_write() - append write command to the command queue record
 * @handle:	the command queue record handle
 * @value:	the specified target register value
 * @addr:	the specified target register physical address
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_rec_write(struct cmdq_rec *handle, u32 value, u32 addr);

/**
 * cmdq_rec_write_mask() - append write command with mask to the command queue
 *			   record
 * @handle:	the command queue record handle
 * @value:	the specified target register value
 * @addr:	the specified target register physical address
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_rec_write_mask(struct cmdq_rec *handle, u32 value,
			u32 addr, u32 mask);

/**
 * cmdq_rec_wait() - append wait command to the command queue record
 * @handle:	the command queue record handle
 * @event:	the desired event type to "wait and CLEAR"
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_rec_wait(struct cmdq_rec *handle, enum cmdq_event event);

/**
 * cmdq_rec_clear_event() - append clear event command to the command queue
 *			    record
 * @handle:	the command queue record handle
 * @event:	the desired event to be cleared
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_rec_clear_event(struct cmdq_rec *handle, enum cmdq_event event);

/**
 * cmdq_rec_flush() - trigger CMDQ to execute the recorded commands
 * @handle:	the command queue record handle
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to execute the recorded commands. Note that this is a
 * synchronous flush function. When the function returned, the recorded
 * commands have been done.
 */
int cmdq_rec_flush(struct cmdq_rec *handle);

/**
 * cmdq_rec_flush_async() - trigger CMDQ to asynchronously execute the
 *			    recorded commands
 * @handle:	the command queue record handle
 *
 * Return: 0 for successfully start execution; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the recorded commands. Note that this
 * is an ASYNC function. When the function returned, it may or may not be
 * finished. There is no way to retrieve the result.
 */
int cmdq_rec_flush_async(struct cmdq_rec *handle);

/**
 * cmdq_rec_flush_async_callback() - trigger CMDQ to asynchronously execute
 *				     the recorded commands and call back after
 *				     ISR is finished and this flush is finished
 * @handle:	the command queue record handle
 * @isr_cb:	called by ISR in the end of CMDQ ISR
 * @isr_data:	this data will pass back to isr_cb
 * @done_cb:	called after flush is done
 * @done_data:	this data will pass back to done_cb
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the recorded commands and call back
 * after ISR is finished and this flush is finished. Note that this is an ASYNC
 * function. When the function returned, it may or may not be finished. The ISR
 * callback function is called in the end of ISR, and  the done callback
 * function is called after all commands are done.
 */
int cmdq_rec_flush_async_callback(struct cmdq_rec *handle,
				  cmdq_async_flush_cb isr_cb,
				  void *isr_data,
				  cmdq_async_flush_cb done_cb,
				  void *done_data);

/**
 * cmdq_rec_destroy() - destroy command queue record handle
 * @handle:	the command queue record handle
 */
void cmdq_rec_destroy(struct cmdq_rec *handle);

#endif	/* __MTK_CMDQ_H__ */
