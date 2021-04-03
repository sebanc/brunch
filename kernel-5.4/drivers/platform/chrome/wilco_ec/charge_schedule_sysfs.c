// SPDX-License-Identifier: GPL-2.0
/*
 * Sysfs interface for Peak Shift and Advanced Battery Charging schedules.
 *
 * Copyright 2019 Google LLC
 *
 * See Documentation/ABI/testing/sysfs-platform-wilco-ec for more info.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_data/wilco-ec.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "charge_schedule.h"

#define DRV_NAME "wilco-charge-schedule"

static ssize_t peak_shift_enable_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	bool enabled;
	int ret;

	ret = wilco_ec_get_peak_shift_enable(ec, &enabled);
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%d\n", enabled);
}

static ssize_t peak_shift_enable_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	bool enable;
	int ret;

	if (strtobool(buf, &enable) < 0)
		return -EINVAL;

	ret = wilco_ec_set_peak_shift_enable(ec, enable);
	if (ret < 0)
		return ret;

	return count;
}

static struct device_attribute dev_attr_peak_shift_enable =
		__ATTR(enable, 0644,
		       peak_shift_enable_show, peak_shift_enable_store);

static ssize_t advanced_charging_enable_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	bool enabled;
	int ret;

	ret = wilco_ec_get_adv_charging_enable(ec, &enabled);
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%d\n", enabled);
}
static ssize_t advanced_charging_enable_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	bool enable;
	int ret;

	if (strtobool(buf, &enable) < 0)
		return -EINVAL;

	ret = wilco_ec_set_adv_charging_enable(ec, enable);
	if (ret < 0)
		return ret;

	return count;
}

static struct device_attribute dev_attr_advanced_charging_enable =
		__ATTR(enable, 0644,
		       advanced_charging_enable_show,
		       advanced_charging_enable_store);

static ssize_t
peak_shift_battery_threshold_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	int ret, val;

	ret = wilco_ec_get_peak_shift_battery_threshold(ec, &val);
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t
peak_shift_battery_threshold_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	int ret, val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	ret = wilco_ec_set_peak_shift_battery_threshold(ec, val);
	if (ret < 0)
		return ret;

	return count;
}

struct device_attribute dev_attr_peak_shift_battery_threshold =
		__ATTR(battery_threshold, 0644,
		       peak_shift_battery_threshold_show,
		       peak_shift_battery_threshold_store);

struct wilco_schedule_attribute {
	struct device_attribute dev_attr;
	int day_of_week;	/* 0==Sunday, 1==Monday, ... */
};

#define to_wilco_schedule_attr(_dev_attr) \
	container_of(_dev_attr, struct wilco_schedule_attribute, dev_attr)

static ssize_t advanced_charging_schedule_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	struct wilco_schedule_attribute *wsa;
	struct adv_charge_schedule sched;
	int ret;

	wsa = to_wilco_schedule_attr(attr);
	sched.day_of_week = wsa->day_of_week;
	ret = wilco_ec_get_adv_charge_schedule(ec, &sched);
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%02d:%02d %02d:%02d\n",
			sched.start_hour, sched.start_minute,
			sched.duration_hour, sched.duration_minute);
}

static ssize_t advanced_charging_schedule_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	struct wilco_schedule_attribute *wsa;
	struct adv_charge_schedule sched;
	int ret;

	ret = sscanf(buf, "%d:%d %d:%d",
		     &sched.start_hour, &sched.start_minute,
		     &sched.duration_hour, &sched.duration_minute);
	if (ret != 4)
		return -EINVAL;

	wsa = to_wilco_schedule_attr(attr);
	sched.day_of_week = wsa->day_of_week;
	ret = wilco_ec_set_adv_charge_schedule(ec, &sched);
	if (ret < 0)
		return ret;

	return count;
}

#define ADVANCED_CHARGING_SCHED_ATTR(_name, _day_of_week)		\
	struct wilco_schedule_attribute adv_charging_sched_attr_##_name = { \
		.dev_attr = __ATTR(_name, 0644,				\
				   advanced_charging_schedule_show,	\
				   advanced_charging_schedule_store),	\
		.day_of_week = _day_of_week				\
	}

static ADVANCED_CHARGING_SCHED_ATTR(sunday, 0);
static ADVANCED_CHARGING_SCHED_ATTR(monday, 1);
static ADVANCED_CHARGING_SCHED_ATTR(tuesday, 2);
static ADVANCED_CHARGING_SCHED_ATTR(wednesday, 3);
static ADVANCED_CHARGING_SCHED_ATTR(thursday, 4);
static ADVANCED_CHARGING_SCHED_ATTR(friday, 5);
static ADVANCED_CHARGING_SCHED_ATTR(saturday, 6);

static struct attribute *wilco_advanced_charging_attrs[] = {
	&dev_attr_advanced_charging_enable.attr,
	&adv_charging_sched_attr_sunday.dev_attr.attr,
	&adv_charging_sched_attr_monday.dev_attr.attr,
	&adv_charging_sched_attr_tuesday.dev_attr.attr,
	&adv_charging_sched_attr_wednesday.dev_attr.attr,
	&adv_charging_sched_attr_thursday.dev_attr.attr,
	&adv_charging_sched_attr_friday.dev_attr.attr,
	&adv_charging_sched_attr_saturday.dev_attr.attr,
	NULL,
};

static struct attribute_group wilco_advanced_charging_attr_group = {
	.name = "advanced_charging",
	.attrs = wilco_advanced_charging_attrs,
};

static ssize_t peak_shift_schedule_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	struct wilco_schedule_attribute *wsa;
	struct peak_shift_schedule sched;
	int ret;

	wsa = to_wilco_schedule_attr(attr);
	sched.day_of_week = wsa->day_of_week;
	ret = wilco_ec_get_peak_shift_schedule(ec, &sched);
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%02d:%02d %02d:%02d %02d:%02d\n",
			sched.start_hour, sched.start_minute,
			sched.end_hour, sched.end_minute,
			sched.charge_start_hour, sched.charge_start_minute);
}

static ssize_t peak_shift_schedule_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct wilco_ec_device *ec = dev_get_platdata(dev);
	struct wilco_schedule_attribute *wsa;
	struct peak_shift_schedule sched;
	int ret;

	ret = sscanf(buf, "%d:%d %d:%d %d:%d",
		     &sched.start_hour, &sched.start_minute,
		     &sched.end_hour, &sched.end_minute,
		     &sched.charge_start_hour, &sched.charge_start_minute);
	if (ret != 6)
		return -EINVAL;

	wsa = to_wilco_schedule_attr(attr);
	sched.day_of_week = wsa->day_of_week;
	ret = wilco_ec_set_peak_shift_schedule(ec, &sched);
	if (ret < 0)
		return ret;

	return count;
}

#define PEAK_SHIFT_SCHED_ATTR(_name, _day_of_week)			\
	struct wilco_schedule_attribute peak_shift_sched_attr_##_name = { \
		.dev_attr = __ATTR(_name, 0644,				\
				   peak_shift_schedule_show,		\
				   peak_shift_schedule_store),		\
		.day_of_week = _day_of_week				\
	}

static PEAK_SHIFT_SCHED_ATTR(sunday, 0);
static PEAK_SHIFT_SCHED_ATTR(monday, 1);
static PEAK_SHIFT_SCHED_ATTR(tuesday, 2);
static PEAK_SHIFT_SCHED_ATTR(wednesday, 3);
static PEAK_SHIFT_SCHED_ATTR(thursday, 4);
static PEAK_SHIFT_SCHED_ATTR(friday, 5);
static PEAK_SHIFT_SCHED_ATTR(saturday, 6);

static struct attribute *wilco_peak_shift_attrs[] = {
	&dev_attr_peak_shift_enable.attr,
	&dev_attr_peak_shift_battery_threshold.attr,
	&peak_shift_sched_attr_sunday.dev_attr.attr,
	&peak_shift_sched_attr_monday.dev_attr.attr,
	&peak_shift_sched_attr_tuesday.dev_attr.attr,
	&peak_shift_sched_attr_wednesday.dev_attr.attr,
	&peak_shift_sched_attr_thursday.dev_attr.attr,
	&peak_shift_sched_attr_friday.dev_attr.attr,
	&peak_shift_sched_attr_saturday.dev_attr.attr,
	NULL,
};

static struct attribute_group wilco_peak_shift_attr_group = {
	.name = "peak_shift",
	.attrs = wilco_peak_shift_attrs,
};

static const struct attribute_group *wilco_charge_schedule_attr_groups[] = {
	&wilco_advanced_charging_attr_group,
	&wilco_peak_shift_attr_group,
	NULL
};

static int wilco_charge_schedule_probe(struct platform_device *pdev)
{
	return devm_device_add_groups(&pdev->dev,
				      wilco_charge_schedule_attr_groups);
}

static struct platform_driver wilco_charge_schedule_driver = {
	.probe	= wilco_charge_schedule_probe,
	.driver = {
		.name = DRV_NAME,
	}
};
module_platform_driver(wilco_charge_schedule_driver);

MODULE_ALIAS("platform:" DRV_NAME);
MODULE_AUTHOR("Nick Crews <ncrews@chromium.org>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Wilco EC charge scheduling driver");
