/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd.
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

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/rwsem.h>
#include <linux/suspend.h>

#include <soc/rockchip/rockchip_sip.h>
#include <soc/rockchip/rk3399_dmc.h>

#include "event/rockchip-dfi.h"
#include "governor.h"
#include "rk3399_dmc_priv.h"

#define NS_TO_CYCLE(NS, MHz)		((NS * MHz) / NSEC_PER_USEC)
#define DFI_DEFAULT_TARGET_LOAD		15
#define DFI_DEFAULT_BOOSTED_TARGET_LOAD	5
#define DFI_DEFAULT_HYSTERESIS		3
#define DFI_DEFAULT_DOWN_THROTTLE_MS	200

static DEFINE_PER_CPU(unsigned int, cpufreq_max);
static DEFINE_PER_CPU(unsigned int, cpufreq_cur);

static ssize_t show_target_load(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;

	return sprintf(buf, "%u\n", dmcfreq->target_load);
}

static ssize_t store_load(unsigned int *load, const char *buf, size_t count)
{
	unsigned int wanted;
	int err;

	err = kstrtouint(buf, 0, &wanted);
	if (err < 0)
		return err;

	if (wanted == 0 || wanted >= 100)
		return -EINVAL;

	*load = wanted;
	return count;
}

static ssize_t store_target_load(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;

	return store_load(&dmcfreq->target_load, buf, count);
}

static ssize_t show_boosted_target_load(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;

	return sprintf(buf, "%u\n", dmcfreq->boosted_target_load);
}

static ssize_t store_boosted_target_load(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;

	return store_load(&dmcfreq->boosted_target_load, buf, count);
}

static ssize_t show_hysteresis(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;

	return sprintf(buf, "%u\n", dmcfreq->hysteresis);
}

static ssize_t store_hysteresis(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{

	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;
	int err;

	err = kstrtouint(buf, 0, &dmcfreq->hysteresis);
	if (err < 0)
		return err;

	return count;
}

static ssize_t show_down_throttle_ms(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;

	return sprintf(buf, "%u\n", dmcfreq->down_throttle_ms);
}

static ssize_t store_down_throttle_ms(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct rk3399_dmcfreq *dmcfreq = devfreq->data;
	int err;

	err = kstrtouint(buf, 0, &dmcfreq->down_throttle_ms);
	if (err < 0)
		return err;

	return count;
}

static DEVICE_ATTR(target_load, 0644, show_target_load, store_target_load);
static DEVICE_ATTR(boosted_target_load, 0644, show_boosted_target_load,
		   store_boosted_target_load);
static DEVICE_ATTR(hysteresis, 0644, show_hysteresis, store_hysteresis);
static DEVICE_ATTR(down_throttle_ms, 0644, show_down_throttle_ms,
		   store_down_throttle_ms);
static struct attribute *dev_entries[] = {
	&dev_attr_target_load.attr,
	&dev_attr_boosted_target_load.attr,
	&dev_attr_hysteresis.attr,
	&dev_attr_down_throttle_ms.attr,
	NULL,
};
static struct attribute_group dev_attr_group = {
	.name = "rk3399-dfi",
	.attrs = dev_entries,
};

static bool rk3399_need_boost(void)
{
	unsigned int cpu;
	bool boosted = false;

	for_each_possible_cpu(cpu) {
		unsigned int max = per_cpu(cpufreq_max, cpu);

		if (max && max == per_cpu(cpufreq_cur, cpu))
			boosted = true;
	}

	return boosted;
}

static int rk3399_cpuhp_notify(struct notifier_block *nb, unsigned long val,
			       void *data)
{
	unsigned int cpu = (unsigned long)data;

	switch (val) {
	case CPU_ONLINE:
		per_cpu(cpufreq_cur, cpu) = per_cpu(cpufreq_max, cpu);
		break;

	case CPU_DEAD:
		per_cpu(cpufreq_cur, cpu) = 0;
		break;
	}

	return NOTIFY_OK;
}

static int rk3399_cpufreq_trans_notify(struct notifier_block *nb,
				       unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct rk3399_dmcfreq *dmcfreq = container_of(nb, struct rk3399_dmcfreq,
						      cpufreq_trans_nb);

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	per_cpu(cpufreq_cur, freq->cpu) = freq->new;
	if (rk3399_need_boost() != dmcfreq->was_boosted)
		wake_up_interruptible(&dmcfreq->boost_thread_wq);

	return NOTIFY_OK;
}

static int rk3399_cpufreq_policy_notify(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct rk3399_dmcfreq *dmcfreq = container_of(nb, struct rk3399_dmcfreq,
						      cpufreq_policy_nb);

	if (val != CPUFREQ_NOTIFY)
		return NOTIFY_OK;

	/* If the max doesn't change, we don't care. */
	if (policy->max == per_cpu(cpufreq_max, policy->cpu))
		return NOTIFY_OK;

	per_cpu(cpufreq_max, policy->cpu) = policy->max;
	if (rk3399_need_boost() != dmcfreq->was_boosted)
		wake_up_interruptible(&dmcfreq->boost_thread_wq);

	return NOTIFY_OK;
}

static int rk3399_dfi_get_target(struct devfreq *devfreq, unsigned long *freq)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	struct devfreq_dev_status *stat;
	unsigned long long a;
	int err;

	if (devfreq->stop_polling) {
		*freq = devfreq->suspend_freq;
		return 0;
	}

	err = devfreq_update_stats(devfreq);
	if (err)
		return err;

	stat = &devfreq->last_status;

	if (stat->total_time == 0) {
		*freq = dmcfreq->max_freq;
		return 0;
	}

	if (stat->busy_time >= (1 << 24) || stat->total_time >= (1 << 24)) {
		stat->busy_time >>= 7;
		stat->total_time >>= 7;
	}

	a = stat->busy_time * stat->current_frequency;
	a = div_u64(a, stat->total_time);
	a *= 100;
	a = div_u64(a, rk3399_need_boost() ? dmcfreq->boosted_target_load :
					  dmcfreq->target_load);
	*freq = (unsigned long)a;

	if (*freq < dmcfreq->min_freq)
		*freq = dmcfreq->min_freq;
	if (*freq > dmcfreq->max_freq)
		*freq = dmcfreq->max_freq;

	return 0;
}

static void rk3399_dfi_calc_top_threshold(struct devfreq *devfreq)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	unsigned int percent;

	if (dmcfreq->rate >= dmcfreq->max_freq)
		percent = 100;
	else
		percent = (rk3399_need_boost() ? dmcfreq->boosted_target_load :
			   dmcfreq->target_load) + dmcfreq->hysteresis;

	rockchip_dfi_calc_top_threshold(dmcfreq->edev, dmcfreq->rate, percent);
}

static void rk3399_dfi_calc_floor_threshold(struct devfreq *devfreq)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	struct dev_pm_opp *opp;
	unsigned long rate;
	unsigned int percent;

	if (dmcfreq->rate <= dmcfreq->min_freq)
		percent = 0;
	else
		percent = (rk3399_need_boost() ? dmcfreq->boosted_target_load :
			   dmcfreq->target_load) - dmcfreq->hysteresis;

	rate = dmcfreq->rate - 1;
	rcu_read_lock();
	opp = devfreq_recommended_opp(devfreq->dev.parent, &rate,
				      DEVFREQ_FLAG_LEAST_UPPER_BOUND);
	rate = dev_pm_opp_get_freq(opp);
	rcu_read_unlock();
	rockchip_dfi_calc_floor_threshold(dmcfreq->edev, rate, percent);
}

static int rk3399_dfi_event_handler(struct devfreq *devfreq, unsigned int event,
				    void *data)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	struct devfreq_event_dev *edev = dmcfreq->edev;
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = devfreq_event_enable_edev(edev);
		if (ret < 0) {
			dev_err(&devfreq->dev, "Unable to enable DFI edev\n");
			return ret;
		}

		devfreq->data = dmcfreq;
		rk3399_dfi_calc_top_threshold(devfreq);
		rk3399_dfi_calc_floor_threshold(devfreq);
		ret = devfreq_event_set_event(edev);
		if (ret < 0) {
			dev_err(&devfreq->dev, "Unable to set DFI event\n");
			devfreq->data = NULL;
			devfreq_event_disable_edev(edev);
			return ret;
		}

		devfreq_monitor_start(devfreq);
		return sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
	case DEVFREQ_GOV_STOP:
		ret = devfreq_event_disable_edev(edev);
		if (ret < 0) {
			dev_err(&devfreq->dev, "Unable to disable DFI edev\n");
			return ret;
		}

		devfreq_monitor_stop(devfreq);
		devfreq->data = NULL;
		sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
		break;
	case DEVFREQ_GOV_SUSPEND:
		ret = devfreq_event_disable_edev(edev);
		if (ret < 0) {
			dev_err(&devfreq->dev, "Unable to disable DFI edev\n");
			return ret;
		}

		devfreq_monitor_suspend(devfreq);
		break;
	case DEVFREQ_GOV_RESUME:
		ret = devfreq_event_enable_edev(edev);
		if (ret < 0) {
			dev_err(&devfreq->dev, "Unable to enable DFI edev\n");
			return ret;
		}

		devfreq_monitor_resume(devfreq);
		rk3399_dfi_calc_top_threshold(devfreq);
		rk3399_dfi_calc_floor_threshold(devfreq);
		ret = devfreq_event_set_event(edev);
		if (ret < 0) {
			dev_err(&devfreq->dev, "Unable to set DFI event\n");
			devfreq->data = NULL;
			devfreq_event_disable_edev(edev);
			return ret;
		}

		break;
	default:
		break;
	}

	return 0;
}

static struct devfreq_governor rk3399_dfi_governor = {
	.name = "rk3399-dfi",
	.get_target_freq = rk3399_dfi_get_target,
	.event_handler = rk3399_dfi_event_handler,
};

static void rk3399_dmcfreq_throttle_work(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work,
						  work);
	struct rk3399_dmcfreq *dmcfreq = container_of(dwork,
						      struct rk3399_dmcfreq,
						      throttle_work);

	rk3399_dfi_calc_floor_threshold(dmcfreq->devfreq);
	devfreq_event_set_event(dmcfreq->edev);
}

static int rk3399_dmcfreq_target(struct device *dev, unsigned long *freq,
				 u32 flags)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(dev);
	struct devfreq_event_dev *edev;
	unsigned long old_clk_rate = dmcfreq->rate;
	unsigned long target_volt, target_rate;
	struct arm_smccc_res res;
	struct dev_pm_opp *opp;
	int dram_flag, err, odt_pd_arg0, odt_pd_arg1;
	unsigned int pd_idle_cycle, standby_idle_cycle, sr_idle_cycle;
	unsigned int sr_mc_gate_idle_cycle, srpd_lite_idle_cycle;
	unsigned int ddrcon_mhz;

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		return PTR_ERR(opp);
	}

	target_rate = dev_pm_opp_get_freq(opp);
	target_volt = dev_pm_opp_get_voltage(opp);
	rcu_read_unlock();

	if (dmcfreq->rate == target_rate)
		return 0;

	mutex_lock(&dmcfreq->lock);

	dram_flag = 0;
	if (target_rate >= dmcfreq->odt_dis_freq)
		dram_flag = 1;

	/*
	 * idle parameter base on the ddr controller clock which
	 * is half of the ddr frequency.
	 * pd_idle, standby_idle base on the controller clock cycle.
	 * sr_idle_cycle, sr_mc_gate_idle_cycle, srpd_lite_idle_cycle,
	 * base on the 1024 controller clock cycle
	 */
	ddrcon_mhz = target_rate / USEC_PER_SEC / 2;
	pd_idle_cycle = NS_TO_CYCLE(dmcfreq->pd_idle, ddrcon_mhz);
	standby_idle_cycle = NS_TO_CYCLE(dmcfreq->standby_idle, ddrcon_mhz);
	sr_idle_cycle = DIV_ROUND_UP(NS_TO_CYCLE(dmcfreq->sr_idle, ddrcon_mhz),
				     1024);
	sr_mc_gate_idle_cycle = DIV_ROUND_UP(
			NS_TO_CYCLE(dmcfreq->sr_mc_gate_idle, ddrcon_mhz),
				    1024);
	srpd_lite_idle_cycle = DIV_ROUND_UP(NS_TO_CYCLE(dmcfreq->srpd_lite_idle,
							ddrcon_mhz), 1024);

	/*
	 * odt_pd_arg0:
	 * bit0-7: sr_idle value
	 * bit8-15: sr_mc_gate_idle
	 * bit16-31: standby_idle
	 */
	odt_pd_arg0 = (sr_idle_cycle & 0xff) |
		      ((sr_mc_gate_idle_cycle & 0xff) << 8) |
		      ((standby_idle_cycle & 0xffff) << 16);

	/* odt_pd_arg1:
	 * bit0-11: pd_idle
	 * bit16-27: srpd_lite_idle
	 */
	odt_pd_arg1 = (pd_idle_cycle & 0xfff) |
		      ((srpd_lite_idle_cycle & 0xfff) << 16);

	if (target_rate >= dmcfreq->sr_idle_dis_freq)
		odt_pd_arg0 = odt_pd_arg0 & 0xffffff00;
	if (target_rate >= dmcfreq->sr_mc_gate_idle_dis_freq)
		odt_pd_arg0 = odt_pd_arg0 & 0xffff00ff;
	if (target_rate >= dmcfreq->standby_idle_dis_freq)
		odt_pd_arg0 = odt_pd_arg0 & 0x0000ffff;

	if (target_rate >= dmcfreq->pd_idle_dis_freq)
		odt_pd_arg1 = odt_pd_arg1 & 0xfffff000;
	if (target_rate >= dmcfreq->srpd_lite_idle_dis_freq)
		odt_pd_arg1 = odt_pd_arg1 & 0xf000ffff;

	arm_smccc_smc(ROCKCHIP_SIP_DRAM_FREQ, odt_pd_arg0, odt_pd_arg1,
		      ROCKCHIP_SIP_CONFIG_DRAM_SET_ODT_PD,
		      dram_flag, 0, 0, 0, &res);

	/*
	 * If frequency scaling from low to high, adjust voltage first.
	 * If frequency scaling from high to low, adjust frequency first.
	 */
	if (old_clk_rate < target_rate) {
		err = regulator_set_voltage(dmcfreq->vdd_center, target_volt,
					    target_volt);
		if (err) {
			dev_err(dev, "Cannot set voltage %lu uV\n",
				target_volt);
			goto out;
		}
	}

	err = clk_set_rate(dmcfreq->dmc_clk, target_rate);
	if (err) {
		dev_err(dev, "Cannot set frequency %lu (%d)\n", target_rate,
			err);
		regulator_set_voltage(dmcfreq->vdd_center, dmcfreq->volt,
				      dmcfreq->volt);
		goto out;
	}

	/*
	 * Setting the dpll is asynchronous since clk_set_rate grabs a global
	 * common clk lock and set_rate for the dpll takes up to one display
	 * frame to complete. We still need to wait for the set_rate to complete
	 * here, though, before we change voltage.
	 */
	rockchip_ddrclk_wait_set_rate(dmcfreq->dmc_clk);

	/*
	 * Check the dpll rate,
	 * There only two result we will get,
	 * 1. Ddr frequency scaling fail, we still get the old rate.
	 * 2. Ddr frequency scaling sucessful, we get the rate we set.
	 */
	dmcfreq->rate = clk_get_rate(dmcfreq->dmc_clk);

	/* If get the incorrect rate, set voltage to old value. */
	if (dmcfreq->rate != target_rate) {
		dev_dbg(dev, "Got wrong frequency, Request %lu, Current %lu\n",
			target_rate, dmcfreq->rate);
		regulator_set_voltage(dmcfreq->vdd_center, dmcfreq->volt,
				      dmcfreq->volt);
		goto out;
	} else if (old_clk_rate > target_rate)
		err = regulator_set_voltage(dmcfreq->vdd_center, target_volt,
					    target_volt);
	if (err)
		dev_err(dev, "Cannot set voltage %lu uV\n", target_volt);
	else
		dmcfreq->volt = target_volt;

	edev = dmcfreq->edev;
	if (old_clk_rate < target_rate) {
		cancel_delayed_work_sync(&dmcfreq->throttle_work);
		rockchip_dfi_calc_floor_threshold(edev, 0, 0);
		schedule_delayed_work(&dmcfreq->throttle_work,
				msecs_to_jiffies(dmcfreq->down_throttle_ms));
	} else {
		rk3399_dfi_calc_floor_threshold(dmcfreq->devfreq);
	}

	rk3399_dfi_calc_top_threshold(dmcfreq->devfreq);
	devfreq_event_set_event(dmcfreq->edev);
out:
	*freq = dmcfreq->rate;
	mutex_unlock(&dmcfreq->lock);
	return err;
}

static int rk3399_dmcfreq_get_dev_status(struct device *dev,
					 struct devfreq_dev_status *stat)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(dev);
	struct devfreq_event_data edata;
	int ret = 0;

	ret = devfreq_event_get_event(dmcfreq->edev, &edata);
	if (ret < 0)
		return ret;

	stat->current_frequency = dmcfreq->rate;
	stat->busy_time = edata.load_count;
	stat->total_time = edata.total_count;

	return ret;
}

static int rk3399_dmcfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(dev);

	*freq = dmcfreq->rate;

	return 0;
}

static struct devfreq_dev_profile rk3399_devfreq_dmc_profile = {
	.polling_ms	= 0,
	.target		= rk3399_dmcfreq_target,
	.get_dev_status	= rk3399_dmcfreq_get_dev_status,
	.get_cur_freq	= rk3399_dmcfreq_get_cur_freq,
};

static __maybe_unused int rk3399_dmcfreq_suspend(struct device *dev)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(dev);

	return rockchip_dmcfreq_block(dmcfreq->devfreq);
}

static __maybe_unused int rk3399_dmcfreq_resume(struct device *dev)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(dev);

	return rockchip_dmcfreq_unblock(dmcfreq->devfreq);
}

static SIMPLE_DEV_PM_OPS(rk3399_dmcfreq_pm, rk3399_dmcfreq_suspend,
			 rk3399_dmcfreq_resume);

int rockchip_dmcfreq_register_clk_sync_nb(struct devfreq *devfreq,
					struct notifier_block *nb)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	mutex_lock(&dmcfreq->en_lock);
	/*
	 * We have a short amount of time (~1ms or less typically) to run
	 * dmcfreq after we sync with the notifier, so syncing with more than
	 * one notifier is not generally possible. Thus, if more than one sync
	 * notifier is registered, disable dmcfreq.
	 */
	if (dmcfreq->num_sync_nb == 1 && dmcfreq->disable_count <= 0) {
		rockchip_ddrclk_set_timeout_en(dmcfreq->dmc_clk, false);
		devfreq_suspend_device(devfreq);
	}

	ret = rockchip_ddrclk_register_sync_nb(dmcfreq->dmc_clk, nb);
	if (ret == 0)
		dmcfreq->num_sync_nb++;
	else if (dmcfreq->num_sync_nb == 1 && dmcfreq->disable_count <= 0) {
		rockchip_ddrclk_set_timeout_en(dmcfreq->dmc_clk, true);
		devfreq_resume_device(devfreq);
	}

	mutex_unlock(&dmcfreq->en_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmcfreq_register_clk_sync_nb);

int rockchip_dmcfreq_unregister_clk_sync_nb(struct devfreq *devfreq,
					  struct notifier_block *nb)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	mutex_lock(&dmcfreq->en_lock);
	ret = rockchip_ddrclk_unregister_sync_nb(dmcfreq->dmc_clk, nb);
	if (ret == 0) {
		dmcfreq->num_sync_nb--;
		if (dmcfreq->num_sync_nb == 1 && dmcfreq->disable_count <= 0) {
			rockchip_ddrclk_set_timeout_en(dmcfreq->dmc_clk, true);
			devfreq_resume_device(devfreq);
		}
	}

	mutex_unlock(&dmcfreq->en_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmcfreq_unregister_clk_sync_nb);

int rockchip_dmcfreq_block(struct devfreq *devfreq)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	int ret = 0;

	mutex_lock(&dmcfreq->en_lock);
	if (dmcfreq->num_sync_nb <= 1 && dmcfreq->disable_count <= 0) {
		rockchip_ddrclk_set_timeout_en(dmcfreq->dmc_clk, false);
		ret = devfreq_suspend_device(devfreq);
	}

	if (!ret)
		dmcfreq->disable_count++;
	mutex_unlock(&dmcfreq->en_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmcfreq_block);

int rockchip_dmcfreq_unblock(struct devfreq *devfreq)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(devfreq->dev.parent);
	int ret = 0;

	mutex_lock(&dmcfreq->en_lock);
	if (dmcfreq->num_sync_nb <= 1 && dmcfreq->disable_count == 1) {
		rockchip_ddrclk_set_timeout_en(dmcfreq->dmc_clk, true);
		ret = devfreq_resume_device(devfreq);
	}

	if (!ret)
		dmcfreq->disable_count--;
	WARN_ON(dmcfreq->disable_count < 0);
	mutex_unlock(&dmcfreq->en_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmcfreq_unblock);

static int rk3399_boost_kthread_fn(void *p)
{
	struct rk3399_dmcfreq *dmcfreq = p;
	struct devfreq *devfreq = dmcfreq->devfreq;

	do {
		bool need_boost = rk3399_need_boost();

		if (dmcfreq->was_boosted != need_boost) {
			dmcfreq->was_boosted = need_boost;
			mutex_lock(&devfreq->lock);
			update_devfreq(devfreq);
			mutex_unlock(&devfreq->lock);
		}
		wait_event_interruptible(dmcfreq->boost_thread_wq,
					 (dmcfreq->was_boosted !=
					  rk3399_need_boost()));
	} while (!kthread_should_stop());

	return 0;
}

static int rk3399_dmcfreq_init_boost_kthread(struct rk3399_dmcfreq *dmcfreq)
{
	struct sched_param sched = { .sched_priority = 16 };

	init_waitqueue_head(&dmcfreq->boost_thread_wq);
	dmcfreq->boost_thread = kthread_run(rk3399_boost_kthread_fn,
					    dmcfreq,
					    "rk3399_dmcfreq_boost");
	if (IS_ERR(dmcfreq->boost_thread)) {
		dev_err(dmcfreq->dev, "Failed to init boost kthread %ld\n",
			PTR_ERR(dmcfreq->boost_thread));
		return PTR_ERR(dmcfreq->boost_thread);
	}
	return sched_setscheduler(dmcfreq->boost_thread, SCHED_FIFO, &sched);
}

static int rk3399_dmcfreq_stop_boost_kthread(struct rk3399_dmcfreq *dmcfreq)
{
	return kthread_stop(dmcfreq->boost_thread);
}

static int rk3399_dmcfreq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct rk3399_dmcfreq *data;
	unsigned long rate;
	struct cpufreq_policy policy;
	unsigned int cpu_tmp;
	int ret;
	struct dev_pm_opp *opp;

	data = devm_kzalloc(dev, sizeof(struct rk3399_dmcfreq), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->lock);
	mutex_init(&data->en_lock);

	data->vdd_center = devm_regulator_get(dev, "center");
	if (IS_ERR(data->vdd_center)) {
		if (PTR_ERR(data->vdd_center) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		dev_err(dev, "Cannot get the regulator \"center\"\n");
		return PTR_ERR(data->vdd_center);
	}

	data->dmc_clk = devm_clk_get(dev, "dmc_clk");
	if (IS_ERR(data->dmc_clk)) {
		if (PTR_ERR(data->dmc_clk) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		dev_err(dev, "Cannot get the clk dmc_clk\n");
		return PTR_ERR(data->dmc_clk);
	};

	data->edev = devfreq_event_get_edev_by_phandle(dev, 0);
	if (IS_ERR(data->edev))
		return -EPROBE_DEFER;

	of_property_read_u32(np, "rockchip,pd_idle_ns", &data->pd_idle);
	of_property_read_u32(np, "rockchip,sr_idle_ns", &data->sr_idle);
	of_property_read_u32(np, "rockchip,sr_mc_gate_idle_ns",
			     &data->sr_mc_gate_idle);
	of_property_read_u32(np, "rockchip,srpd_lite_idle_ns",
			     &data->srpd_lite_idle);
	of_property_read_u32(np, "rockchip,standby_idle_ns",
			     &data->standby_idle);
	of_property_read_u32(np, "rockchip,pd_idle_dis_freq",
			     &data->pd_idle_dis_freq);
	of_property_read_u32(np, "rockchip,sr_idle_dis_freq",
			     &data->sr_idle_dis_freq);
	of_property_read_u32(np, "rockchip,sr_mc_gate_idle_dis_freq",
			     &data->sr_mc_gate_idle_dis_freq);
	of_property_read_u32(np, "rockchip,srpd_lite_idle_dis_freq",
			     &data->srpd_lite_idle_dis_freq);
	of_property_read_u32(np, "rockchip,standby_idle_dis_freq",
			     &data->standby_idle_dis_freq);
	of_property_read_u32(np, "rockchip,odt_dis_freq", &data->odt_dis_freq);

	/*
	 * We add a devfreq driver to our parent since it has a device tree node
	 * with operating points.
	 */
	if (dev_pm_opp_of_add_table(dev)) {
		dev_err(dev, "Invalid operating-points in device tree.\n");
		rcu_read_unlock();
		return -EINVAL;
	}

	data->target_load = DFI_DEFAULT_TARGET_LOAD;
	data->boosted_target_load = DFI_DEFAULT_BOOSTED_TARGET_LOAD;
	data->hysteresis = DFI_DEFAULT_HYSTERESIS;
	data->down_throttle_ms = DFI_DEFAULT_DOWN_THROTTLE_MS;
	INIT_DELAYED_WORK(&data->throttle_work, rk3399_dmcfreq_throttle_work);
	data->rate = clk_get_rate(data->dmc_clk);

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, &data->rate, 0);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		return PTR_ERR(opp);
	}
	data->volt = dev_pm_opp_get_voltage(opp);
	rcu_read_unlock();

	rk3399_devfreq_dmc_profile.initial_freq = data->rate;

	ret = devfreq_add_governor(&rk3399_dfi_governor);
	if (ret < 0) {
		dev_err(dev, "Failed to add dfi governor\n");
		return ret;
	}

	data->dev = dev;
	platform_set_drvdata(pdev, data);
	data->devfreq = devm_devfreq_add_device(dev,
						&rk3399_devfreq_dmc_profile,
						"rk3399-dfi",
						NULL);
	if (IS_ERR(data->devfreq))
		return PTR_ERR(data->devfreq);

	/*
	 * Find the min and max frequency since the rk3399-dfi governor relies
	 * on knowing these to avoid uneccesary interrupts.
	 */
	rcu_read_lock();
	rate = ULONG_MAX;
	opp = devfreq_recommended_opp(dev, &rate, 0);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		return PTR_ERR(opp);
	}
	rate = dev_pm_opp_get_freq(opp);
	data->max_freq = rate;
	rate = 0;
	opp = devfreq_recommended_opp(dev, &rate,
				      DEVFREQ_FLAG_LEAST_UPPER_BOUND);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		return PTR_ERR(opp);
	}
	rate = dev_pm_opp_get_freq(opp);
	data->min_freq = rate;
	rcu_read_unlock();

	devm_devfreq_register_opp_notifier(dev, data->devfreq);

	ret = pd_register_dmc_nb(data->devfreq);
	if (ret < 0)
		return ret;

	ret = rk3399_dmcfreq_init_boost_kthread(data);
	if (ret < 0)
		goto pd_unregister;

	data->cpufreq_policy_nb.notifier_call = rk3399_cpufreq_policy_notify;
	data->cpufreq_trans_nb.notifier_call = rk3399_cpufreq_trans_notify;
	ret = cpufreq_register_notifier(&data->cpufreq_policy_nb,
					CPUFREQ_POLICY_NOTIFIER);
	if (ret < 0)
		goto kthread_destroy;

	ret = cpufreq_register_notifier(&data->cpufreq_trans_nb,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (ret < 0)
		goto policy_unregister;

	data->cpu_hotplug_nb.notifier_call = rk3399_cpuhp_notify;
	ret = register_cpu_notifier(&data->cpu_hotplug_nb);
	if (ret < 0)
		goto trans_unregister;

	for_each_possible_cpu(cpu_tmp) {
		ret = cpufreq_get_policy(&policy, cpu_tmp);
		if (ret < 0)
			continue;

		per_cpu(cpufreq_max, cpu_tmp) = policy.max;
		per_cpu(cpufreq_cur, cpu_tmp) = policy.cur;
	}

	/* The dfi irq won't trigger a frequency update until this is done. */
	rockchip_dfi_set_devfreq(data->edev, data->devfreq);

	return 0;

trans_unregister:
	cpufreq_unregister_notifier(&data->cpufreq_trans_nb,
				    CPUFREQ_TRANSITION_NOTIFIER);
policy_unregister:
	cpufreq_unregister_notifier(&data->cpufreq_policy_nb,
				    CPUFREQ_POLICY_NOTIFIER);
kthread_destroy:
	rk3399_dmcfreq_stop_boost_kthread(data);
pd_unregister:
	pd_unregister_dmc_nb(data->devfreq);


	return ret;
}

static int rk3399_dmcfreq_remove(struct platform_device *pdev)
{
	struct rk3399_dmcfreq *dmcfreq = dev_get_drvdata(&pdev->dev);

	rockchip_dfi_set_devfreq(dmcfreq->edev, NULL);

	unregister_cpu_notifier(&dmcfreq->cpu_hotplug_nb);
	WARN_ON(cpufreq_unregister_notifier(&dmcfreq->cpufreq_trans_nb,
					    CPUFREQ_TRANSITION_NOTIFIER));
	WARN_ON(cpufreq_unregister_notifier(&dmcfreq->cpufreq_policy_nb,
					    CPUFREQ_POLICY_NOTIFIER));
	WARN_ON(rk3399_dmcfreq_stop_boost_kthread(dmcfreq));
	WARN_ON(pd_unregister_dmc_nb(dmcfreq->devfreq));

	return devfreq_remove_governor(&rk3399_dfi_governor);
}

static const struct of_device_id rk3399dmc_devfreq_of_match[] = {
	{ .compatible = "rockchip,rk3399-dmc" },
	{ },
};

static struct platform_driver rk3399_dmcfreq_driver = {
	.probe	= rk3399_dmcfreq_probe,
	.remove	= rk3399_dmcfreq_remove,
	.driver = {
		.name	= "rk3399-dmc-freq",
		.pm	= &rk3399_dmcfreq_pm,
		.of_match_table = rk3399dmc_devfreq_of_match,
	},
};
module_platform_driver(rk3399_dmcfreq_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lin Huang <hl@rock-chips.com>");
MODULE_DESCRIPTION("RK3399 dmcfreq driver with devfreq framework");
