/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef _IPTS_RECEIVER_H_
#define _IPTS_RECEIVER_H_

#include <linux/mei_cl_bus.h>

void ipts_receiver_callback(struct mei_cl_device *cldev);

#endif /* _IPTS_RECEIVER_H_ */
