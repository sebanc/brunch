/* SPDX-License-Identifier: GPL-2.0 */
/*
 * EC communication for Peak Shift and Advanced Battery Charging schedules.
 *
 * Copyright 2019 Google LLC
 *
 * See Documentation/ABI/testing/sysfs-platform-wilco-ec for more info.
 */
#ifndef WILCO_EC_CHARGE_SCHEDULE_H
#define WILCO_EC_CHARGE_SCHEDULE_H

#include <linux/platform_data/wilco-ec.h>

#define WILCO_EC_PEAK_SHIFT_BATTERY_THRESHOLD_MIN	15
#define WILCO_EC_PEAK_SHIFT_BATTERY_THRESHOLD_MAX	100

struct peak_shift_schedule {
	int day_of_week;		/* 0==Sunday, 1==Monday, ... */
	int start_hour;			/* 0..23 */
	int start_minute;		/* One of {0, 15, 30, 45} */
	int end_hour;			/* 0..23 */
	int end_minute;			/* One of {0, 15, 30, 45} */
	int charge_start_hour;		/* 0..23 */
	int charge_start_minute;	/* One of {0, 15, 30, 45} */
};

struct adv_charge_schedule {
	int day_of_week;	/* 0==Sunday, 1==Monday, ... */
	int start_hour;		/* 0..23 */
	int start_minute;	/* One of {0, 15, 30, 45} */
	int duration_hour;	/* 0..23 */
	int duration_minute;	/* One of {0, 15, 30, 45} */
};

/*
 * Return 0 on success, negative error code on failure. For the getters()
 * the sched.day_of_week field should be filled before use. For the setters()
 * all of the sched fields should be filled before use.
 */
int wilco_ec_get_adv_charge_schedule(struct wilco_ec_device *ec,
				     struct adv_charge_schedule *sched);
int wilco_ec_set_adv_charge_schedule(struct wilco_ec_device *ec,
				     const struct adv_charge_schedule *sched);
int wilco_ec_get_peak_shift_schedule(struct wilco_ec_device *ec,
				     struct peak_shift_schedule *sched);
int wilco_ec_set_peak_shift_schedule(struct wilco_ec_device *ec,
				     const struct peak_shift_schedule *sched);

/* Return 0 on success, negative error code on failure. */
int wilco_ec_get_peak_shift_enable(struct wilco_ec_device *ec, bool *enable);
int wilco_ec_set_peak_shift_enable(struct wilco_ec_device *ec, bool enable);
int wilco_ec_get_adv_charging_enable(struct wilco_ec_device *ec, bool *enable);
int wilco_ec_set_adv_charging_enable(struct wilco_ec_device *ec, bool enable);

/*
 * Return 0 on success, negative error code on failure.
 * Valid range for setting is from |WILCO_EC_PEAK_SHIFT_BATTERY_THRESHOLD_MIN|
 * to |WILCO_EC_PEAK_SHIFT_BATTERY_THRESHOLD_MAX|, inclusive.
 */
int wilco_ec_get_peak_shift_battery_threshold(struct wilco_ec_device *ec,
					      int *percent);
int wilco_ec_set_peak_shift_battery_threshold(struct wilco_ec_device *ec,
					      int percent);

#endif /* WILCO_EC_CHARGE_SCHEDULE_H */
