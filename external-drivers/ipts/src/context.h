/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_CONTEXT_H
#define IPTS_CONTEXT_H

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/mei_cl_bus.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "mei.h"
#include "resources.h"
#include "spec-device.h"
#include "thread.h"

struct ipts_context {
	struct device *dev;
	struct ipts_mei mei;

	enum ipts_mode mode;

	/*
	 * Prevents concurrent GET_FEATURE reports.
	 */
	struct mutex feature_lock;
	struct completion feature_event;

	/*
	 * These are not inside of struct ipts_resources
	 * because they don't own the memory they point to.
	 */
	struct ipts_buffer feature_report;
	struct ipts_buffer descriptor;

	struct hid_device *hid;
	struct ipts_device_info info;
	struct ipts_resources resources;

	struct ipts_thread receiver_loop;
};

#endif /* IPTS_CONTEXT_H */
