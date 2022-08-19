/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020-2022 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_RECEIVER_H
#define IPTS_RECEIVER_H

#include "context.h"

void ipts_receiver_start(struct ipts_context *ipts);
void ipts_receiver_stop(struct ipts_context *ipts);

#endif /* IPTS_RECEIVER_H */
