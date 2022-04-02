/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 Google, Inc.
 */

#ifndef __DEVFREQ_EVENT_ROCKCHIP_DFI_H
#define __DEVFREQ_EVENT_ROCKCHIP_DFI_H

#include <linux/devfreq-event.h>

#if IS_ENABLED(CONFIG_DEVFREQ_EVENT_ROCKCHIP_DFI)
int rockchip_dfi_calc_top_threshold(struct devfreq_event_dev *edev,
				    unsigned long rate,
				    unsigned int percent);
int rockchip_dfi_calc_floor_threshold(struct devfreq_event_dev *edev,
				      unsigned long rate,
				      unsigned int percent);
#else
static inline int rockchip_dfi_calc_top_threshold(
		struct devfreq_event_dev *edev,
		unsigned long rate,
		unsigned int percent) { return 0; }
static inline int rockchip_dfi_calc_floor_threshold(
		struct devfreq_event_dev *edev,
		unsigned long rate,
		unsigned int percent) { return 0; }
#endif
#endif
