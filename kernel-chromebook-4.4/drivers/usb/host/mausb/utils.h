/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_UTILS_H__
#define __MAUSB_UTILS_H__

#include "hpal.h"

int mausb_host_dev_register(void);
void mausb_host_dev_deregister(void);
void mausb_notify_completed_user_events(struct mausb_ring_buffer *ring_buffer);
void mausb_notify_ring_events(struct mausb_ring_buffer *ring_buffer);
void mausb_stop_ring_events(void);

#endif /* __MAUSB_UTILS_H__ */
