/*
 * Copyright (C) 2014 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SOC_ROCKCHIP_DMC_SYNC_H
#define __SOC_ROCKCHIP_DMC_SYNC_H

#include <linux/notifier.h>

/*
 * Time for the dmc set rate to happen in sram. This value is removed from the
 * pause cpu timeout to prevent screen jank.
 */
#define DMC_SET_RATE_TIME_NS	(500 * NSEC_PER_USEC)
#define DMC_PAUSE_CPU_TIME_NS	(30 * NSEC_PER_USEC)
#define DMC_MIN_VBLANK_NS	(DMC_SET_RATE_TIME_NS + DMC_PAUSE_CPU_TIME_NS)
#define DMC_DEFAULT_TIMEOUT_NS	NSEC_PER_SEC

/* The timeout assume we're running at least this fast */
#define DMC_TIMEOUT_MHZ		1500

enum dmc_enable_op {
	DMC_ENABLE = 0,
	DMC_DISABLE,
};

extern void rockchip_dmc_disable_timeout(void);
extern void rockchip_dmc_enable_timeout(void);
extern void rockchip_dmc_lock(void);
extern int rockchip_dmc_wait(ktime_t *timeout);
extern void rockchip_dmc_unlock(void);
extern void rockchip_dmc_en_lock(void);
extern void rockchip_dmc_en_unlock(void);
extern bool rockchip_dmc_enabled(void);

#ifdef CONFIG_ARCH_ROCKCHIP
extern void rockchip_dmc_enable(void);
extern void rockchip_dmc_disable(void);
#else
static inline void rockchip_dmc_enable(void) { }
static inline void rockchip_dmc_disable(void) { }
#endif

extern int rockchip_dmc_get(struct notifier_block *nb);
extern int rockchip_dmc_put(struct notifier_block *nb);
extern int rockchip_dmc_register_enable_notifier(struct notifier_block *nb);
extern int rockchip_dmc_unregister_enable_notifier(struct notifier_block *nb);

#endif /* __SOC_ROCKCHIP_RK3288_DMC_SYNC_H */
