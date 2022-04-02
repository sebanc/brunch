/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 * Author: Lin Huang <hl@rock-chips.com>
 */

#ifndef __SOC_RK3399_DMC_H
#define __SOC_RK3399_DMC_H

struct devfreq;
struct notifier_block;

#define DMC_MIN_SET_RATE_NS	(250 * NSEC_PER_USEC)
#define DMC_MIN_VBLANK_NS	(DMC_MIN_SET_RATE_NS + 50 * NSEC_PER_USEC)

#if IS_ENABLED(CONFIG_ARM_RK3399_DMC_DEVFREQ)
int rockchip_dmcfreq_register_clk_sync_nb(struct devfreq *devfreq,
					struct notifier_block *nb);
int rockchip_dmcfreq_unregister_clk_sync_nb(struct devfreq *devfreq,
					  struct notifier_block *nb);
int rockchip_dmcfreq_block(struct devfreq *devfreq);
int rockchip_dmcfreq_unblock(struct devfreq *devfreq);
#else
static inline int rockchip_dmcfreq_register_clk_sync_nb(struct devfreq *devfreq,
		struct notifier_block *nb) { return 0; }
static inline int rockchip_dmcfreq_unregister_clk_sync_nb(
		struct devfreq *devfreq,
		struct notifier_block *nb) { return 0; }
static inline int rockchip_dmcfreq_block(struct devfreq *devfreq) { return 0; }
static inline int rockchip_dmcfreq_unblock(struct devfreq *devfreq)
{ return 0; }
#endif

#if IS_ENABLED(CONFIG_ROCKCHIP_PM_DOMAINS)

int pd_register_dmc_nb(struct devfreq *devfreq);
int pd_unregister_dmc_nb(struct devfreq *devfreq);

#else

static inline int pd_register_dmc_nb(struct devfreq *devfreq) { return 0; }
static inline int pd_unregister_dmc_nb(struct devfreq *devfreq) { return 0; }

#endif /* CONFIG_ROCKCHIP_PM_DOMAINS */

#endif
