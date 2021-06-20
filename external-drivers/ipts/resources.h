/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef _IPTS_RESOURCES_H_
#define _IPTS_RESOURCES_H_

#include "context.h"

int ipts_resources_alloc(struct ipts_context *ipts);
void ipts_resources_free(struct ipts_context *ipts);

#endif /* _IPTS_RESOURCES_H_ */
