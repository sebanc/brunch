/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_RECEIVER_H
#define IPTS_RECEIVER_H

#include "context.h"

int ipts_receiver_start(struct ipts_context *ipts);
int ipts_receiver_stop(struct ipts_context *ipts);

#endif /* IPTS_RECEIVER_H */
