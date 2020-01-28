// SPDX-License-Identifier: GPL-2.0
/*
 * EC communication for Peak Shift and Advanced Battery Charging schedules.
 *
 * Copyright 2019 Google LLC
 *
 * See Documentation/ABI/testing/sysfs-platform-wilco-ec for more info.
 */

#include <linux/platform_data/wilco-ec.h>
#include "charge_schedule.h"

/* Property IDs and related EC constants */
#define PID_PEAK_SHIFT				0x04EA
#define PID_PEAK_SHIFT_BATTERY_THRESHOLD	0x04EB
#define PID_PEAK_SHIFT_SUNDAY			0x04EE
#define PID_ADV_CHARGING			0x04ED
#define PID_ADV_CHARGING_SUNDAY			0x04F5

/*
 * Date and hour information is passed to/from the EC using packed bytes,
 * where each byte represents an hour and a minute that some event occurs.
 * The minute field supports quarter-hour intervals, so either
 * 0, 15, 30, or 45. This allows this info to be packed within 2 bits.
 * Along with the 5 bits of hour info [0-23], this gives us 7 used bits
 * within each packed byte:
 * +---------------+
 * |7|6|5|4|3|2|1|0|
 * +---------------+
 * |X|  hour   |min|
 * +---------------+
 */

#define MINUTE_POSITION	0	/* bits[0:1] */
#define MINUTE_MASK	0x03	/* 0b00000011 */
#define HOUR_POSITION	2	/* bits[2:6] */
#define HOUR_MASK	0x7c	/* 0b01111100 */

struct adv_charging_payload {
	u8 start_time;
	u8 duration_time;
	u16 RESERVED;
} __packed;

struct peak_shift_payload {
	u8 start_time;
	u8 end_time;
	u8 charge_start_time;
	u8 RESERVED;
} __packed;

/* Pack hour and minute info into a byte. */
static u8 pack_field(int hour, int minute)
{
	int result = 0;
	int quarter_hour;

	quarter_hour = minute / 15;
	result |= hour << HOUR_POSITION;
	result |= quarter_hour << MINUTE_POSITION;

	return (u8)result;
}

/* Extract hour and minute info from a byte. */
static void unpack_field(int *hour, int *minute, u8 field)
{
	int quarter_hour;

	*hour =		(field & HOUR_MASK)	>> HOUR_POSITION;
	quarter_hour =	(field & MINUTE_MASK)	>> MINUTE_POSITION;
	*minute = quarter_hour * 15;
}

#define hour_valid(h)   (h >= 0 && h < 24)
#define minute_valid(m) (m >= 0 && m < 60 && (m % 15 == 0))

static bool
is_adv_charging_sched_valid(const struct adv_charge_schedule *sched)
{
	return (hour_valid(sched->start_hour) &&
		hour_valid(sched->duration_hour) &&
		minute_valid(sched->start_minute) &&
		minute_valid(sched->duration_minute));
}

static bool
is_peak_shift_schedule_valid(const struct peak_shift_schedule *sched)
{
	return (hour_valid(sched->start_hour) &&
		hour_valid(sched->end_hour) &&
		hour_valid(sched->charge_start_hour) &&
		minute_valid(sched->start_minute) &&
		minute_valid(sched->end_minute) &&
		minute_valid(sched->charge_start_minute));
}

int wilco_ec_get_adv_charge_schedule(struct wilco_ec_device *ec,
				     struct adv_charge_schedule *sched)
{
	struct wilco_ec_property_msg msg;
	struct adv_charging_payload *payload;
	int ret;

	msg.property_id = PID_ADV_CHARGING_SUNDAY + sched->day_of_week;
	ret = wilco_ec_get_property(ec, &msg);
	if (ret)
		return ret;

	payload = (struct adv_charging_payload *) msg.data;
	unpack_field(&sched->start_hour, &sched->start_minute,
		     payload->start_time);
	unpack_field(&sched->duration_hour, &sched->duration_minute,
		     payload->duration_time);

	return 0;
}

int wilco_ec_set_adv_charge_schedule(struct wilco_ec_device *ec,
				     const struct adv_charge_schedule *sched)
{
	struct adv_charging_payload *payload;
	struct wilco_ec_property_msg msg;

	if (!is_adv_charging_sched_valid(sched))
		return -EINVAL;

	payload = (struct adv_charging_payload *)msg.data;
	memset(payload, 0, sizeof(*payload));
	payload->start_time = pack_field(sched->start_hour,
					 sched->start_minute);
	payload->duration_time = pack_field(sched->duration_hour,
					    sched->duration_minute);
	msg.length = sizeof(*payload);
	msg.property_id = PID_ADV_CHARGING_SUNDAY + sched->day_of_week;

	return wilco_ec_set_property(ec, &msg);
}

int wilco_ec_get_peak_shift_schedule(struct wilco_ec_device *ec,
				     struct peak_shift_schedule *sched)
{
	struct wilco_ec_property_msg msg;
	struct peak_shift_payload *payload;
	int ret;

	msg.property_id = PID_PEAK_SHIFT_SUNDAY + sched->day_of_week;
	ret = wilco_ec_get_property(ec, &msg);
	if (ret)
		return ret;

	payload = (struct peak_shift_payload *) msg.data;
	unpack_field(&sched->start_hour, &sched->start_minute,
		     payload->start_time);
	unpack_field(&sched->end_hour, &sched->end_minute, payload->end_time);
	unpack_field(&sched->charge_start_hour, &sched->charge_start_minute,
		     payload->charge_start_time);

	return 0;
}

int wilco_ec_set_peak_shift_schedule(struct wilco_ec_device *ec,
				     const struct peak_shift_schedule *sched)
{
	struct peak_shift_payload *payload;
	struct wilco_ec_property_msg msg;

	if (!is_peak_shift_schedule_valid(sched))
		return -EINVAL;

	payload = (struct peak_shift_payload *)msg.data;
	memset(payload, 0, sizeof(*payload));
	payload->start_time = pack_field(sched->start_hour,
					 sched->start_minute);
	payload->end_time = pack_field(sched->end_hour, sched->end_minute);
	payload->charge_start_time = pack_field(sched->charge_start_hour,
						sched->charge_start_minute);
	msg.length = sizeof(*payload);
	msg.property_id = PID_PEAK_SHIFT_SUNDAY + sched->day_of_week;

	return wilco_ec_set_property(ec, &msg);
}

int wilco_ec_get_peak_shift_enable(struct wilco_ec_device *ec, bool *enable)
{
	u8 result;
	int ret;

	ret = wilco_ec_get_byte_property(ec, PID_PEAK_SHIFT, &result);
	if (ret < 0)
		return ret;

	*enable = result;

	return 0;
}

int wilco_ec_set_peak_shift_enable(struct wilco_ec_device *ec, bool enable)
{
	return wilco_ec_set_byte_property(ec, PID_PEAK_SHIFT, (u8)enable);
}

int wilco_ec_get_adv_charging_enable(struct wilco_ec_device *ec, bool *enable)
{
	u8 result;
	int ret;

	ret = wilco_ec_get_byte_property(ec, PID_ADV_CHARGING, &result);
	if (ret < 0)
		return ret;

	*enable = result;

	return 0;
}

int wilco_ec_set_adv_charging_enable(struct wilco_ec_device *ec, bool enable)
{
	return wilco_ec_set_byte_property(ec, PID_ADV_CHARGING, (u8)enable);
}

int wilco_ec_get_peak_shift_battery_threshold(struct wilco_ec_device *ec,
					      int *percent)
{
	u8 result;
	int ret;

	ret = wilco_ec_get_byte_property(ec, PID_PEAK_SHIFT_BATTERY_THRESHOLD,
					 &result);
	if (ret < 0)
		return ret;

	*percent = result;

	return 0;
}
int wilco_ec_set_peak_shift_battery_threshold(struct wilco_ec_device *ec,
					      int percent)
{
	if (percent < WILCO_EC_PEAK_SHIFT_BATTERY_THRESHOLD_MIN ||
	    percent > WILCO_EC_PEAK_SHIFT_BATTERY_THRESHOLD_MAX)
		return -EINVAL;
	return wilco_ec_set_byte_property(ec, PID_PEAK_SHIFT_BATTERY_THRESHOLD,
					  (u8) percent);
}
