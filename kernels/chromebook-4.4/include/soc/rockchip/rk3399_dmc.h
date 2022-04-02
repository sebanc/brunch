/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 * Author: Lin Huang <hl@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __SOC_RK3399_DMC_H
#define __SOC_RK3399_DMC_H

#include <linux/devfreq.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>

#define DMC_MIN_SET_RATE_NS	(250 * NSEC_PER_USEC)
#define DMC_MIN_VBLANK_NS	(DMC_MIN_SET_RATE_NS + 50 * NSEC_PER_USEC)

struct rk3399_dmcfreq {
	struct device *dev;
	struct devfreq *devfreq;
	struct clk *dmc_clk;
	struct devfreq_event_dev *edev;
	struct mutex lock;
	struct mutex en_lock;
	int num_sync_nb;
	int disable_count;
	unsigned int pd_idle;
	unsigned int sr_idle;
	unsigned int sr_mc_gate_idle;
	unsigned int srpd_lite_idle;
	unsigned int standby_idle;
	unsigned int pd_idle_dis_freq;
	unsigned int sr_idle_dis_freq;
	unsigned int sr_mc_gate_idle_dis_freq;
	unsigned int srpd_lite_idle_dis_freq;
	unsigned int standby_idle_dis_freq;
	unsigned int odt_dis_freq;
	struct regulator *vdd_center;
	unsigned long rate;
	unsigned long min_freq;
	unsigned long max_freq;
	unsigned long volt;
	struct delayed_work throttle_work;
	unsigned int target_load;
	unsigned int boosted_target_load;
	unsigned int hysteresis;
	unsigned int down_throttle_ms;
	struct task_struct *boost_thread;
	wait_queue_head_t boost_thread_wq;
	struct notifier_block cpufreq_policy_nb;
	struct notifier_block cpufreq_trans_nb;
	struct notifier_block cpu_hotplug_nb;
	bool was_boosted;
};

#if IS_ENABLED(CONFIG_ARM_RK3399_DMC_DEVFREQ)
int rockchip_dmcfreq_register_clk_sync_nb(struct devfreq *devfreq,
					struct notifier_block *nb);
int rockchip_dmcfreq_unregister_clk_sync_nb(struct devfreq *devfreq,
					  struct notifier_block *nb);
int rockchip_dmcfreq_block(struct devfreq *devfreq);
int rockchip_dmcfreq_unblock(struct devfreq *devfreq);
int pd_register_dmc_nb(struct devfreq *devfreq);
int pd_unregister_dmc_nb(struct devfreq *devfreq);
#else
static inline int rockchip_dmcfreq_register_clk_sync_nb(struct devfreq *devfreq,
		struct notifier_block *nb) { return 0; }
static inline int rockchip_dmcfreq_unregister_clk_sync_nb(
		struct devfreq *devfreq,
		struct notifier_block *nb) { return 0; }
static inline int rockchip_dmcfreq_block(struct devfreq *devfreq) { return 0; }
static inline int rockchip_dmcfreq_unblock(struct devfreq *devfreq)
{ return 0; }
static inline int pd_register_dmc_nb(struct devfreq *devfreq) { return 0; }
static inline int pd_unregister_dmc_nb(struct devfreq *devfreq) { return 0; }
#endif
#endif
