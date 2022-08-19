/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_CONTEXT_H
#define IPTS_CONTEXT_H

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/mei_cl_bus.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "resources.h"
#include "spec-device.h"

struct ipts_context {
	struct device *dev;
	struct mei_cl_device *cldev;

	enum ipts_mode mode;
	u8 *get_feature_report;

	u8 *descriptor;
	size_t desc_size;

	struct hid_device *hid;
	struct ipts_device_info info;
	struct ipts_resources resources;

	struct task_struct *event_loop;
	struct task_struct *doorbell_loop;
};

#endif /* IPTS_CONTEXT_H */
