/*
 * linux/drivers/devfreq/governor_passive.c
 *
 * Copyright (C) 2016 Samsung Electronics
 * Author: Chanwoo Choi <cw00.choi@samsung.com>
 * Author: MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/slab.h>
#include "governor.h"

static unsigned int xlate_cpufreq_to_devfreq(struct devfreq_passive_data *data,
					     unsigned int cpu)
{
	unsigned int cpu_min, cpu_max, dev_min, dev_max, cpu_percent, max_state;
	struct devfreq_cpu_state *cpu_state = data->cpu_state[cpu];
	struct devfreq *devfreq = (struct devfreq *)data->this;
	unsigned long *freq_table = devfreq->profile->freq_table;
	struct dev_pm_opp *opp = NULL, *cpu_opp = NULL;
	unsigned long cpu_freq, freq;

	if (!cpu_state || cpu_state->first_cpu != cpu ||
	    !cpu_state->opp_table || !devfreq->opp_table)
		return 0;

	cpu_freq = cpu_state->freq * 1000;
	cpu_opp = devfreq_recommended_opp(cpu_state->dev, &cpu_freq, 0);
	if (IS_ERR(cpu_opp))
		return 0;

	opp = dev_pm_opp_xlate_opp(cpu_state->opp_table,
				   devfreq->opp_table, cpu_opp);
	dev_pm_opp_put(cpu_opp);

	if (!IS_ERR(opp)) {
		freq = dev_pm_opp_get_freq(opp);
		dev_pm_opp_put(opp);
	} else {
		/* Use Interpolation if required opps is not available */
		cpu_min = cpu_state->min_freq;
		cpu_max = cpu_state->max_freq;
		cpu_freq = cpu_state->freq;

		if (freq_table) {
			/* Get minimum frequency according to sorting order */
			max_state = freq_table[devfreq->profile->max_state - 1];
			if (freq_table[0] < max_state) {
				dev_min = freq_table[0];
				dev_max = max_state;
			} else {
				dev_min = max_state;
				dev_max = freq_table[0];
			}
		} else {
			if (devfreq->max_freq <= devfreq->min_freq)
				return 0;
			dev_min = devfreq->min_freq;
			dev_max = devfreq->max_freq;
		}
		cpu_percent = ((cpu_freq - cpu_min) * 100) / cpu_max - cpu_min;
		freq = dev_min + mult_frac(dev_max - dev_min, cpu_percent, 100);
	}

	return freq;
}

static int get_target_freq_with_cpufreq(struct devfreq *devfreq,
					unsigned long *freq)
{
	struct devfreq_passive_data *p_data =
				(struct devfreq_passive_data *)devfreq->data;
	unsigned int cpu, target_freq = 0;

	for_each_online_cpu(cpu)
		target_freq = max(target_freq,
				  xlate_cpufreq_to_devfreq(p_data, cpu));

	*freq = target_freq;

	return 0;
}

static int get_target_freq_with_devfreq(struct devfreq *devfreq,
					unsigned long *freq)
{
	struct devfreq_passive_data *p_data
			= (struct devfreq_passive_data *)devfreq->data;
	struct devfreq *parent_devfreq = (struct devfreq *)p_data->parent;
	unsigned long child_freq = ULONG_MAX;
	struct dev_pm_opp *opp = NULL, *p_opp = NULL;
	int i, count, ret = 0;

	/*
	 * If the parent and passive devfreq device uses the OPP table,
	 * get the next frequency by using the OPP table.
	 */

	/*
	 * - parent devfreq device uses the governors except for passive.
	 * - passive devfreq device uses the passive governor.
	 *
	 * Each devfreq has the OPP table. After deciding the new frequency
	 * from the governor of parent devfreq device, the passive governor
	 * need to get the index of new frequency on OPP table of parent
	 * device. And then the index is used for getting the suitable
	 * new frequency for passive devfreq device.
	 */
	if (!devfreq->profile || !devfreq->profile->freq_table
		|| devfreq->profile->max_state <= 0)
		return -EINVAL;

	/*
	 * The passive governor have to get the correct frequency from OPP
	 * list of parent device. Because in this case, *freq is temporary
	 * value which is decided by ondemand governor.
	 */
	p_opp = devfreq_recommended_opp(parent_devfreq->dev.parent, freq, 0);
	if (IS_ERR(p_opp)) {
		ret = PTR_ERR(p_opp);
		goto out;
	}

	if (devfreq->opp_table && parent_devfreq->opp_table)
		opp = dev_pm_opp_xlate_opp(parent_devfreq->opp_table,
					   devfreq->opp_table, p_opp);
	if (opp) {
		*freq = dev_pm_opp_get_freq(opp);
		dev_pm_opp_put(opp);
		goto out;
	}

	/*
	 * Get the OPP table's index of decided freqeuncy by governor
	 * of parent device.
	 */
	for (i = 0; i < parent_devfreq->profile->max_state; i++)
		if (parent_devfreq->profile->freq_table[i] == *freq)
			break;

	if (i == parent_devfreq->profile->max_state) {
		ret = -EINVAL;
		goto out;
	}

	/* Get the suitable frequency by using index of parent device. */
	if (i < devfreq->profile->max_state) {
		child_freq = devfreq->profile->freq_table[i];
	} else {
		count = devfreq->profile->max_state;
		child_freq = devfreq->profile->freq_table[count - 1];
	}

	/* Return the suitable frequency for passive device. */
	*freq = child_freq;

out:
	if (!IS_ERR_OR_NULL(opp))
		dev_pm_opp_put(p_opp);

	return ret;
}

static int devfreq_passive_get_target_freq(struct devfreq *devfreq,
					   unsigned long *freq)
{
	struct devfreq_passive_data *p_data =
				(struct devfreq_passive_data *)devfreq->data;
	int ret;

	/*
	 * If the devfreq device with passive governor has the specific method
	 * to determine the next frequency, should use the get_target_freq()
	 * of struct devfreq_passive_data.
	 */
	if (p_data->get_target_freq)
		return p_data->get_target_freq(devfreq, freq);

	switch (p_data->parent_type) {
	case DEVFREQ_PARENT_DEV:
		ret = get_target_freq_with_devfreq(devfreq, freq);
		break;
	case CPUFREQ_PARENT_DEV:
		ret = get_target_freq_with_cpufreq(devfreq, freq);
		break;
	default:
		ret = -EINVAL;
		dev_err(&devfreq->dev, "Invalid parent type\n");
		break;
	}

	return ret;
}

static int update_devfreq_passive(struct devfreq *devfreq, unsigned long freq)
{
	int ret;

	if (!devfreq->governor)
		return -EINVAL;

	mutex_lock_nested(&devfreq->lock, SINGLE_DEPTH_NESTING);

	ret = devfreq->governor->get_target_freq(devfreq, &freq);
	if (ret < 0)
		goto out;

	ret = devfreq->profile->target(devfreq->dev.parent, &freq, 0);
	if (ret < 0)
		goto out;

	if (devfreq->profile->freq_table
		&& (devfreq_update_status(devfreq, freq)))
		dev_err(&devfreq->dev,
			"Couldn't update frequency transition information.\n");

	devfreq->previous_freq = freq;

out:
	mutex_unlock(&devfreq->lock);

	return 0;
}

static int devfreq_passive_notifier_call(struct notifier_block *nb,
				unsigned long event, void *ptr)
{
	struct devfreq_passive_data *data
			= container_of(nb, struct devfreq_passive_data, nb);
	struct devfreq *devfreq = (struct devfreq *)data->this;
	struct devfreq *parent = (struct devfreq *)data->parent;
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)ptr;
	unsigned long freq = freqs->new;

	switch (event) {
	case DEVFREQ_PRECHANGE:
		if (parent->previous_freq > freq)
			update_devfreq_passive(devfreq, freq);
		break;
	case DEVFREQ_POSTCHANGE:
		if (parent->previous_freq < freq)
			update_devfreq_passive(devfreq, freq);
		break;
	}

	return NOTIFY_DONE;
}

static int cpufreq_passive_notifier_call(struct notifier_block *nb,
					 unsigned long event, void *ptr)
{
	struct devfreq_passive_data *data =
			container_of(nb, struct devfreq_passive_data, nb);
	struct devfreq *devfreq = (struct devfreq *)data->this;
	struct devfreq_cpu_state *cpu_state;
	struct cpufreq_freqs *freq = ptr;
	unsigned int current_freq;
	int ret;

	if (event != CPUFREQ_POSTCHANGE || !freq ||
	    !data->cpu_state[freq->policy->cpu])
		return 0;

	cpu_state = data->cpu_state[freq->policy->cpu];
	if (cpu_state->freq == freq->new)
		return 0;

	/* Backup current freq and pre-update cpu state freq*/
	current_freq = cpu_state->freq;
	cpu_state->freq = freq->new;

	mutex_lock(&devfreq->lock);
	ret = update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
	if (ret) {
		cpu_state->freq = current_freq;
		dev_err(&devfreq->dev, "Couldn't update the frequency.\n");
		return ret;
	}

	return 0;
}

static int cpufreq_passive_register(struct devfreq_passive_data **p_data)
{
	struct devfreq_passive_data *data = *p_data;
	struct devfreq *devfreq = (struct devfreq *)data->this;
	struct device *dev = devfreq->dev.parent;
	struct opp_table *opp_table = NULL;
	struct devfreq_cpu_state *state;
	struct cpufreq_policy *policy;
	struct device *cpu_dev;
	unsigned int cpu;
	int ret;

	get_online_cpus();
	data->nb.notifier_call = cpufreq_passive_notifier_call;
	ret = cpufreq_register_notifier(&data->nb,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		dev_err(dev, "Couldn't register cpufreq notifier.\n");
		data->nb.notifier_call = NULL;
		goto out;
	}

	/* Populate devfreq_cpu_state */
	for_each_online_cpu(cpu) {
		if (data->cpu_state[cpu])
			continue;

		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			state = kzalloc(sizeof(*state), GFP_KERNEL);
			if (!state) {
				ret = -ENOMEM;
				goto out;
			}

			cpu_dev = get_cpu_device(cpu);
			if (!cpu_dev) {
				dev_err(dev, "Couldn't get cpu device.\n");
				ret = -ENODEV;
				goto out;
			}

			opp_table = dev_pm_opp_get_opp_table(cpu_dev);
			if (IS_ERR(devfreq->opp_table)) {
				ret = PTR_ERR(opp_table);
				goto out;
			}

			state->dev = cpu_dev;
			state->opp_table = opp_table;
			state->first_cpu = cpumask_first(policy->related_cpus);
			state->freq = policy->cur;
			state->min_freq = policy->cpuinfo.min_freq;
			state->max_freq = policy->cpuinfo.max_freq;
			data->cpu_state[cpu] = state;
			cpufreq_cpu_put(policy);
		} else {
			ret = -EPROBE_DEFER;
			goto out;
		}
	}
out:
	put_online_cpus();
	if (ret)
		return ret;

	/* Update devfreq */
	mutex_lock(&devfreq->lock);
	ret = update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
	if (ret)
		dev_err(dev, "Couldn't update the frequency.\n");

	return ret;
}

static int cpufreq_passive_unregister(struct devfreq_passive_data **p_data)
{
	struct devfreq_passive_data *data = *p_data;
	struct devfreq_cpu_state *cpu_state;
	int cpu;

	if (data->nb.notifier_call)
		cpufreq_unregister_notifier(&data->nb,
					    CPUFREQ_TRANSITION_NOTIFIER);

	for_each_possible_cpu(cpu) {
		cpu_state = data->cpu_state[cpu];
		if (cpu_state) {
			if (cpu_state->opp_table)
				dev_pm_opp_put_opp_table(cpu_state->opp_table);
			kfree(cpu_state);
			cpu_state = NULL;
		}
	}

	return 0;
}

static int devfreq_passive_event_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	struct devfreq_passive_data *p_data
			= (struct devfreq_passive_data *)devfreq->data;
	struct devfreq *parent = (struct devfreq *)p_data->parent;
	struct notifier_block *nb = &p_data->nb;
	int ret = 0;

	if (p_data->parent_type == DEVFREQ_PARENT_DEV && !parent)
		return -EPROBE_DEFER;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!p_data->this)
			p_data->this = devfreq;

		if (p_data->parent_type == DEVFREQ_PARENT_DEV) {
			nb->notifier_call = devfreq_passive_notifier_call;
			ret = devfreq_register_notifier(parent, nb,
						DEVFREQ_TRANSITION_NOTIFIER);
		} else if (p_data->parent_type == CPUFREQ_PARENT_DEV) {
			ret = cpufreq_passive_register(&p_data);
		} else {
			ret = -EINVAL;
		}
		break;
	case DEVFREQ_GOV_STOP:
		if (p_data->parent_type == DEVFREQ_PARENT_DEV)
			WARN_ON(devfreq_unregister_notifier(parent, nb,
						DEVFREQ_TRANSITION_NOTIFIER));
		else if (p_data->parent_type == CPUFREQ_PARENT_DEV)
			cpufreq_passive_unregister(&p_data);
		else
			ret = -EINVAL;
		break;
	default:
		break;
	}

	return ret;
}

static struct devfreq_governor devfreq_passive = {
	.name = DEVFREQ_GOV_PASSIVE,
	.immutable = 1,
	.get_target_freq = devfreq_passive_get_target_freq,
	.event_handler = devfreq_passive_event_handler,
};

static int __init devfreq_passive_init(void)
{
	return devfreq_add_governor(&devfreq_passive);
}
subsys_initcall(devfreq_passive_init);

static void __exit devfreq_passive_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_passive);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);
}
module_exit(devfreq_passive_exit);

MODULE_AUTHOR("Chanwoo Choi <cw00.choi@samsung.com>");
MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("DEVFREQ Passive governor");
MODULE_LICENSE("GPL v2");
