/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_CONTROL_H
#define IPTS_CONTROL_H

#include <linux/types.h>

#include "context.h"
#include "spec-device.h"

/*
 * Sends a request to stop the data flow to the device.
 * All outstanding data needs to be acknowledged using
 * feedback before the request can be finalized.
 */
int ipts_control_request_flush(struct ipts_context *ipts);

/*
 * Waits until the flushing request has been finalized.
 */
int ipts_control_wait_flush(struct ipts_context *ipts);

/*
 * Notify the device that the driver can receive new data.
 */
int ipts_control_request_data(struct ipts_context *ipts);

/*
 * Wait until new data is available on the device.
 * In doorbell mode, this function will never return while the data
 * flow is active. Instead, the doorbell will be incremented when new
 * data is available.
 *
 * If shutdown == true, the function will block, and errors that are
 * produced by the device due to a disabled sensor will be ignored.
 *
 * If shutdown == false, the function will not block. If no data is
 * available, -EAGAIN will be returned.
 */
int ipts_control_wait_data(struct ipts_context *ipts, bool shutdown);

/*
 * Submits the given feedback buffer to the hardware.
 */
int ipts_control_send_feedback(struct ipts_context *ipts, u32 buffer);

/*
 * Acknowledges that the data in a buffer has been processed.
 */
int ipts_control_refill_buffer(struct ipts_context *ipts, u32 buffer);

/*
 * Initializes the IPTS device and starts the data flow.
 */
int ipts_control_start(struct ipts_context *ipts);

/*
 * Stops the data flow and resets the device.
 */
int ipts_control_stop(struct ipts_context *ipts);

/*
 * Stops the device and immideately starts it again.
 */
int ipts_control_restart(struct ipts_context *ipts);

#endif /* IPTS_CONTROL_H */
