/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef _IPTS_CONTROL_H_
#define _IPTS_CONTROL_H_

#include <linux/types.h>

#include "context.h"

int ipts_control_send(struct ipts_context *ipts, u32 cmd, void *payload,
		      size_t size);
int ipts_control_send_feedback(struct ipts_context *ipts, u32 buffer);
int ipts_control_set_feature(struct ipts_context *ipts, u8 report, u8 value);
int ipts_control_start(struct ipts_context *ipts);
int ipts_control_restart(struct ipts_context *ipts);
int ipts_control_stop(struct ipts_context *ipts);

#endif /* _IPTS_CONTROL_H_ */
