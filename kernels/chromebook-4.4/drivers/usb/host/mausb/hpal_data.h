/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_HPAL_DATA_H__
#define __MAUSB_HPAL_DATA_H__

#include <linux/types.h>

#include "hpal_events.h"

int mausb_send_in_data_msg(struct mausb_device *dev, struct mausb_event *event);
void mausb_receive_in_data(struct mausb_event *event,
			   struct mausb_urb_ctx *urb_ctx);

int mausb_send_out_data_msg(struct mausb_device *dev, struct mausb_event *event,
			    struct mausb_urb_ctx *urb_ctx);
void mausb_receive_out_data(struct mausb_event *event,
			    struct mausb_urb_ctx *urb_ctx);

#define MAUSB_ISOCH_IN_KVEC_NUM 3

int mausb_send_isoch_in_msg(struct mausb_device *dev,
			    struct mausb_event *event);
void mausb_receive_isoch_in_data(struct mausb_device *dev,
				 struct mausb_event *event,
				 struct mausb_urb_ctx *urb_ctx);

int mausb_send_isoch_out_msg(struct mausb_device *ma_dev,
			     struct mausb_event *mausb_event,
			     struct mausb_urb_ctx *urb_ctx);
void mausb_receive_isoch_out(struct mausb_event *event);

#endif /* __MAUSB_HPAL_DATA_H__ */
