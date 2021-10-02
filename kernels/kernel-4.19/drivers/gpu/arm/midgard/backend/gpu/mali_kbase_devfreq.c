/*
 *
 * (C) COPYRIGHT 2014-2018 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <mali_kbase.h>
#include <mali_kbase_tlstream.h>
#include <mali_kbase_config_defaults.h>
#include <backend/gpu/mali_kbase_pm_internal.h>

#include <linux/of.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/devfreq_cooling.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#include <linux/pm_opp.h>
#else /* Linux >= 3.13 */
/* In 3.13 the OPP include header file, types, and functions were all
 * renamed. Use the old filename for the include, and define the new names to
 * the old, when an old kernel is detected.
 */
#include <linux/opp.h>
#define dev_pm_opp opp
#define dev_pm_opp_get_voltage opp_get_voltage
#define dev_pm_opp_get_opp_count opp_get_opp_count
#define dev_pm_opp_find_freq_ceil opp_find_freq_ceil
#define dev_pm_opp_find_freq_floor opp_find_freq_floor
#endif /* Linux >= 3.13 */

/**
 * opp_translate - Translate nominal OPP frequency from devicetree into real
 *                 frequency and core mask
 * @kbdev:     Device pointer
 * @freq:      Nominal frequency
 * @core_mask: Pointer to u64 to store core mask to
 *
 * Return: Real target frequency
 *
 * This function will only perform translation if an operating-points-v2-mali
 * table is present in devicetree. If one is not present then it will return an
 * untranslated frequency and all cores enabled.
 */
static unsigned long opp_translate(struct kbase_device *kbdev,
		unsigned long freq, u64 *core_mask)
{
	int i;

	for (i = 0; i < kbdev->num_opps; i++) {
		if (kbdev->opp_table[i].opp_freq == freq) {
			*core_mask = kbdev->opp_table[i].core_mask;
			return kbdev->opp_table[i].real_freq;
		}
	}

	/* Failed to find OPP - return all cores enabled & nominal frequency */
	*core_mask = kbdev->gpu_props.props.raw_props.shader_present;

	return freq;
}

static void voltage_range_check(struct kbase_device *kbdev,
				unsigned long *voltages)
{
	if (kbdev->devfreq_ops.voltage_range_check)
		kbdev->devfreq_ops.voltage_range_check(kbdev, voltages);
}

#ifdef CONFIG_REGULATOR
static int set_voltages(struct kbase_device *kbdev, unsigned long *voltages,
			bool inc)
{
	int i;
	int err;

	if (kbdev->devfreq_ops.set_voltages)
		return kbdev->devfreq_ops.set_voltages(kbdev, voltages, inc);

	for (i = 0; i < kbdev->regulator_num; i++) {
		err = regulator_set_voltage(kbdev->regulator[i],
					    voltages[i], voltages[i]);
		if (err) {
			dev_err(kbdev->dev,
				"Failed to set reg %d voltage err:(%d)\n",
				i, err);
			return err;
		}
	}

	return 0;
}
#endif

static int set_frequency(struct kbase_device *kbdev, unsigned long freq)
{
	if (kbdev->devfreq_ops.set_frequency)
		return kbdev->devfreq_ops.set_frequency(kbdev, freq);

	return clk_set_rate(kbdev->clock, freq);
}

static int
kbase_devfreq_target(struct device *dev, unsigned long *target_freq, u32 flags)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long nominal_freq;
	unsigned long freq = 0;
	unsigned long target_volt[KBASE_MAX_REGULATORS];
	int err, i;
	u64 core_mask;

	freq = *target_freq;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_lock();
#endif
	opp = devfreq_recommended_opp(dev, &freq, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_unlock();
#endif
	if (IS_ERR_OR_NULL(opp)) {
		dev_err(dev, "Failed to get opp (%ld)\n", PTR_ERR(opp));
		return PTR_ERR(opp);
	}

	for (i = 0; i < kbdev->regulator_num; i++)
		target_volt[i] = dev_pm_opp_get_voltage_supply(opp, i);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	dev_pm_opp_put(opp);
#endif

	nominal_freq = freq;

	/*
	 * Only update if there is a change of frequency
	 */
	if (kbdev->current_nominal_freq == nominal_freq &&
		kbdev->current_voltage[0] == target_volt[0]) {
		*target_freq = nominal_freq;
		return 0;
	}

	freq = opp_translate(kbdev, nominal_freq, &core_mask);
	voltage_range_check(kbdev, target_volt);

#ifdef CONFIG_REGULATOR
	if (kbdev->current_voltage[0] < target_volt[0]) {
		err = set_voltages(kbdev, target_volt, true);
		if (err) {
			dev_err(kbdev->dev, "Failed to increase voltage\n");
			return err;
		}
	}
#endif

	err = set_frequency(kbdev, freq);
	if (err) {
		dev_err(dev, "Failed to set clock %lu (target %lu)\n",
				freq, *target_freq);
		return err;
	}

#ifdef CONFIG_REGULATOR
	if (kbdev->current_voltage[0] > target_volt[0]) {
		err = set_voltages(kbdev, target_volt, false);
		if (err) {
			dev_err(kbdev->dev, "Failed to decrease voltage\n");
			return err;
		}
	}
#endif

	kbase_devfreq_set_core_mask(kbdev, core_mask);

	*target_freq = nominal_freq;
	for (i = 0; i < kbdev->regulator_num; i++)
		kbdev->current_voltage[i] = target_volt[i];
	kbdev->current_nominal_freq = nominal_freq;
	kbdev->current_freq = freq;
	kbdev->current_core_mask = core_mask;

	KBASE_TLSTREAM_AUX_DEVFREQ_TARGET((u64)nominal_freq);

	return err;
}

static int
kbase_devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	*freq = kbdev->current_nominal_freq;

	return 0;
}

static int
kbase_devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct kbasep_pm_metrics diff;

	kbase_pm_get_dvfs_metrics(kbdev, &kbdev->last_devfreq_metrics, &diff);

	stat->busy_time = diff.time_busy;
	stat->total_time = diff.time_busy + diff.time_idle;
	stat->current_frequency = kbdev->current_nominal_freq;
	stat->private_data = NULL;

	return 0;
}

static int kbase_devfreq_init_freq_table(struct kbase_device *kbdev,
		struct devfreq_dev_profile *dp)
{
	int count;
	int i = 0;
	unsigned long freq;
	struct dev_pm_opp *opp;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_lock();
#endif
	count = dev_pm_opp_get_opp_count(kbdev->dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_unlock();
#endif
	if (count < 0)
		return count;

	dp->freq_table = kmalloc_array(count, sizeof(dp->freq_table[0]),
				GFP_KERNEL);
	if (!dp->freq_table)
		return -ENOMEM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_lock();
#endif
	for (i = 0, freq = ULONG_MAX; i < count; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(kbdev->dev, &freq);
		if (IS_ERR(opp))
			break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
		dev_pm_opp_put(opp);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0) */

		dp->freq_table[i] = freq;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rcu_read_unlock();
#endif

	if (count != i)
		dev_warn(kbdev->dev, "Unable to enumerate all OPPs (%d!=%d\n",
				count, i);

	dp->max_state = i;

	return 0;
}

static void kbase_devfreq_term_freq_table(struct kbase_device *kbdev)
{
	struct devfreq_dev_profile *dp = kbdev->devfreq->profile;

	kfree(dp->freq_table);
}

static void kbase_devfreq_exit(struct device *dev)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	kbase_devfreq_term_freq_table(kbdev);
}

static int kbase_devfreq_init_core_mask_table(struct kbase_device *kbdev)
{
	struct device_node *opp_node = of_parse_phandle(kbdev->dev->of_node,
			"operating-points-v2", 0);
	struct device_node *node;
	int i = 0;
	int count;
	u64 shader_present = kbdev->gpu_props.props.raw_props.shader_present;

	if (!opp_node)
		return 0;
	if (!of_device_is_compatible(opp_node, "operating-points-v2-mali"))
		return 0;

	count = dev_pm_opp_get_opp_count(kbdev->dev);
	kbdev->opp_table = kmalloc_array(count,
			sizeof(struct kbase_devfreq_opp), GFP_KERNEL);
	if (!kbdev->opp_table)
		return -ENOMEM;

	for_each_available_child_of_node(opp_node, node) {
		u64 core_mask;
		u64 opp_freq, real_freq;
		const void *core_count_p;

		if (of_property_read_u64(node, "opp-hz", &opp_freq)) {
			dev_warn(kbdev->dev, "OPP is missing required opp-hz property\n");
			continue;
		}
		if (of_property_read_u64(node, "opp-hz-real", &real_freq))
			real_freq = opp_freq;
		if (of_property_read_u64(node, "opp-core-mask", &core_mask))
			core_mask = shader_present;
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_11056) &&
				core_mask != shader_present) {
			dev_warn(kbdev->dev, "Ignoring OPP %llu - Dynamic Core Scaling not supported on this GPU\n",
					opp_freq);
			continue;
		}

		core_count_p = of_get_property(node, "opp-core-count", NULL);
		if (core_count_p) {
			u64 remaining_core_mask =
				kbdev->gpu_props.props.raw_props.shader_present;
			int core_count = be32_to_cpup(core_count_p);

			core_mask = 0;

			for (; core_count > 0; core_count--) {
				int core = ffs(remaining_core_mask);

				if (!core) {
					dev_err(kbdev->dev, "OPP has more cores than GPU\n");
					return -ENODEV;
				}

				core_mask |= (1ull << (core-1));
				remaining_core_mask &= ~(1ull << (core-1));
			}
		}

		if (!core_mask) {
			dev_err(kbdev->dev, "OPP has invalid core mask of 0\n");
			return -ENODEV;
		}

		kbdev->opp_table[i].opp_freq = opp_freq;
		kbdev->opp_table[i].real_freq = real_freq;
		kbdev->opp_table[i].core_mask = core_mask;

		dev_info(kbdev->dev, "OPP %d : opp_freq=%llu real_freq=%llu core_mask=%llx\n",
				i, opp_freq, real_freq, core_mask);

		i++;
	}

	kbdev->num_opps = i;

	return 0;
}

/*
 * Magic values given to simple_ondemand to make things work better.
 *
 * These values are loosely based on values that were added for veyron in
 * https://crrev.com/c/223221.  That patch claimed:
 *
 * > the values used in this patch are temporary values.
 * > A future patch will tune these values.
 *
 * ...but no future patch ever materialized and the 3.14 tree that veyron
 * shipped with continued to use values like these.
 *
 * On newer kernels mali was switched to use devfreq instead of a custom
 * dvfs and that means we used the standard thresholds to bump up and down
 * frequencies.  For simple_ondemand this means we bump up to the max
 * if the utilization > 90% and stay there as long as it stays > 85%.  If
 * it drops below that we'll do some math to figure out what the frequency
 * should be.  Specifically I believe we do:
 *   utilPct * curFreq / (.90 - .05/2)
 *
 * It turns out that simple_ondemand's guidelines aren't ideal for Mali, at
 * least for certain workloads.  At least in workloads like those produced by
 * glmark2 on veyron with the sched cpugovernor, it appears that a fully
 * loaded GPU only measures a utilization between 30-70% for the most part.
 * That meant we were just sitting at the lowest GPU frequency.  Doh!
 *
 * This is believed to be caused by the fact that scheduling tasks to the
 * GPU actually involves two actors: the CPU and the GPU.  Said another
 * way there is CPU overhead involved in loading jobs to the GPU.  This
 * CPU overhead is _not_ measured in the devfreq utilization stats.  That
 * means that a 100% graphics path (both CPU and GPU together) will not report
 * 100% utilization.  We need to fudge things a little bit to handle this case.
 *
 * Let's adjust the devfreq thresholds to make this work better.  Now:
 * - If utilization > 35%, go to max.
 * - If utilization > 30% stay the same.
 * - Set freq to: utilPct * curFreq / (.35 - .05/2)
 *
 * NOTE: struct is static because simple_ondemand hangs outo it.  It also
 * could be const but the normal devfreq framework doesn't declare it as such.
 */
static struct devfreq_simple_ondemand_data kbase_ondemand_data = {
	.upthreshold = 35,	/* bump to max if utilization > up */
	.downdifferential = 5,	/* recalculate if utilization <= (up-down) */
};

int kbase_devfreq_init(struct kbase_device *kbdev)
{
	struct devfreq_dev_profile *dp;
	int err;

	if (!kbdev->clock) {
		dev_err(kbdev->dev, "Clock not available for devfreq\n");
		return -ENODEV;
	}

	/* Can't do devfreq without this table */
	if (!kbdev->dev_opp_table) {
		dev_err(kbdev->dev, "Uninitialized devfreq opp table\n");
		return -ENODEV;
	}

	kbdev->current_freq = clk_get_rate(kbdev->clock);
	kbdev->current_nominal_freq = kbdev->current_freq;

	dp = &kbdev->devfreq_profile;

	dp->initial_freq = kbdev->current_freq;
	dp->polling_ms = 100;
	dp->target = kbase_devfreq_target;
	dp->get_dev_status = kbase_devfreq_status;
	dp->get_cur_freq = kbase_devfreq_cur_freq;
	dp->exit = kbase_devfreq_exit;

	if (kbase_devfreq_init_freq_table(kbdev, dp))
		return -EFAULT;

	if (dp->max_state > 0) {
		/* Record the maximum frequency possible */
		kbdev->gpu_props.props.core_props.gpu_freq_khz_max =
			dp->freq_table[0] / 1000;
	};

	err = kbase_devfreq_init_core_mask_table(kbdev);
	if (err)
		return err;

	kbdev->devfreq = devfreq_add_device(kbdev->dev, dp,
				"simple_ondemand", &kbase_ondemand_data);
	if (IS_ERR(kbdev->devfreq)) {
		kfree(dp->freq_table);
		return PTR_ERR(kbdev->devfreq);
	}

	/* devfreq_add_device only copies a few of kbdev->dev's fields, so
	 * set drvdata explicitly so IPA models can access kbdev. */
	dev_set_drvdata(&kbdev->devfreq->dev, kbdev);

	err = devfreq_register_opp_notifier(kbdev->dev, kbdev->devfreq);
	if (err) {
		dev_err(kbdev->dev,
			"Failed to register OPP notifier (%d)\n", err);
		goto opp_notifier_failed;
	}

#ifdef CONFIG_DEVFREQ_THERMAL
	err = kbase_ipa_init(kbdev);
	if (err) {
		dev_err(kbdev->dev, "IPA initialization failed\n");
		goto cooling_failed;
	}

	kbdev->devfreq_cooling = of_devfreq_cooling_register_power(
			kbdev->dev->of_node,
			kbdev->devfreq,
			&kbase_ipa_power_model_ops);
	if (IS_ERR_OR_NULL(kbdev->devfreq_cooling)) {
		err = PTR_ERR(kbdev->devfreq_cooling);
		dev_err(kbdev->dev,
			"Failed to register cooling device (%d)\n",
			err);
		goto cooling_failed;
	}
#endif

	return 0;

#ifdef CONFIG_DEVFREQ_THERMAL
cooling_failed:
	devfreq_unregister_opp_notifier(kbdev->dev, kbdev->devfreq);
#endif /* CONFIG_DEVFREQ_THERMAL */
opp_notifier_failed:
	if (devfreq_remove_device(kbdev->devfreq))
		dev_err(kbdev->dev, "Failed to terminate devfreq (%d)\n", err);
	else
		kbdev->devfreq = NULL;

	return err;
}

void kbase_devfreq_term(struct kbase_device *kbdev)
{
	int err;

	dev_dbg(kbdev->dev, "Term Mali devfreq\n");

#ifdef CONFIG_DEVFREQ_THERMAL
	if (kbdev->devfreq_cooling)
		devfreq_cooling_unregister(kbdev->devfreq_cooling);

	kbase_ipa_term(kbdev);
#endif

	devfreq_unregister_opp_notifier(kbdev->dev, kbdev->devfreq);

	err = devfreq_remove_device(kbdev->devfreq);
	if (err)
		dev_err(kbdev->dev, "Failed to terminate devfreq (%d)\n", err);
	else
		kbdev->devfreq = NULL;

	kfree(kbdev->opp_table);
}
