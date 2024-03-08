/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_CONTROL_H
#define IPTS_CONTROL_H

#include <linux/types.h>

#include "context.h"
#include "spec-dma.h"
#include "spec-mei.h"

int ipts_control_request_flush(struct ipts_context *ipts);
int ipts_control_wait_flush(struct ipts_context *ipts);
int ipts_control_request_data(struct ipts_context *ipts);
int ipts_control_wait_data(struct ipts_context *ipts, struct ipts_rsp_ready_for_data *response);
int ipts_control_refill_buffer(struct ipts_context *ipts, struct ipts_data_buffer *buffer);
int ipts_control_hid2me_feedback(struct ipts_context *ipts, enum ipts_feedback_cmd_type cmd_type,
				 enum ipts_feedback_data_type data_type, void *data, size_t size);

int ipts_control_start(struct ipts_context *ipts);
int ipts_control_stop(struct ipts_context *ipts);
int ipts_control_restart(struct ipts_context *ipts);

#endif /* IPTS_CONTROL_H */
