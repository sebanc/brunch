/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2022-2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef IPTS_HID_H
#define IPTS_HID_H

#include <linux/types.h>

#include "context.h"
#include "spec-dma.h"

static inline void ipts_hid_enable(struct ipts_context *ipts)
{
	WRITE_ONCE(ipts->hid_active, true);
}

static inline void ipts_hid_disable(struct ipts_context *ipts)
{
	WRITE_ONCE(ipts->hid_active, false);
}

int ipts_hid_input_data(struct ipts_context *ipts, struct ipts_data_buffer *buffer);

int ipts_hid_init(struct ipts_context *ipts);
int ipts_hid_free(struct ipts_context *ipts);

#endif /* IPTS_HID_H */
