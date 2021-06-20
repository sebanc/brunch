/*
 * Copyright (C) 2017 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
void rockchip_dfi_set_devfreq(struct devfreq_event_dev *edev,
			      struct devfreq *devfreq);
#else
static inline int rockchip_dfi_calc_top_threshold(
		struct devfreq_event_dev *edev,
		unsigned long rate,
		unsigned int percent) { return 0; }
static inline int rockchip_dfi_calc_floor_threshold(
		struct devfreq_event_dev *edev,
		unsigned long rate,
		unsigned int percent) { return 0; }
static inline void rockchip_dfi_set_devfreq(struct devfreq_event_dev *edev,
					    struct devfreq *devfreq) {}
#endif
#endif
