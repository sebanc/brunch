// SPDX-License-Identifier: GPL-2.0
/*
 * Apple Ambient Light Sensor Driver
 *
 * Copyright (c) 2017-2018 Ronald Tschalär
 */

/*
 * MacBookPro models with an iBridge chip (13,[23] and 14,[23]) have an
 * ambient light sensor that is exposed via one of the USB interfaces on
 * the iBridge as a standard HID light sensor. However, we cannot use the
 * existing hid-sensor-als driver, for the following reasons:
 *
 * 1. The hid-sensor-als driver is part of the hid-sensor-hub which in turn
 *    is a hid driver, but you can't have more than one hid driver per hid
 *    device, which is a problem because the touch bar also needs to
 *    register as a driver for this hid device.
 *    (FIXME: not true if we use multiple virtual-hdev's per sub-driver)
 *
 * 2. The hid-sensor-hub expects the hid reports to be wrapped in a physical
 *    collection, but this sensor has them wrapped in an application
 *    collection.
 *
 * 3. The hid-sensor-attributes expects the absolute sensitivity to reported
 *    with a usage of 030Fh (Property: Change Sensitivity Absolute), and
 *    hid-sensor-als also tests for 04D0h (Data Field: Light + Modifier:
 *    Change Sensitivity Absolute); however the device's report uses a usage
 *    of 14D1h (Data Field: Illuminance + Modifier: Change Sensitivity
 *    Absolute)

 * 4. The device needs to be explicitly powered up before making various
 *    requests - this may be a timing issue, but the async pm calls done
 *    by the usbhid driver do not appear to be sufficient.
 *    (FIXME: the hid-sensor-als seems to be ok here)
 *
 * 5. This driver implements a dynamic sensitivity mode, basically a
 *    relative-percent sensitivity.
 */

#define dev_fmt(fmt) "als: " fmt

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/hid-sensor-ids.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "apple-ibridge.h"

#define APPLEALS_DYN_SENS		0	/* our dynamic sensitivity */
#define APPLEALS_DEF_CHANGE_SENS	APPLEALS_DYN_SENS

struct appleals_device {
	struct hid_device	*hid_dev;
	struct hid_report	*cfg_report;
	struct hid_field	*illum_field;
	struct iio_dev		*iio_dev;
	int			cur_sensitivity;
	int			cur_hysteresis;
	bool			events_enabled;
};

static struct hid_driver appleals_hid_driver;

/*
 * This is a primitive way to get a relative sensitivity, one where we get
 * notified when the value changes by a certain percentage rather than some
 * absolute value. MacOS somehow manages to configure the sensor to work this
 * way (with a 15% relative sensitivity), but I haven't been able to figure
 * out how so far. So until we do, this provides a less-than-perfect
 * simulation.
 *
 * When the brightness value is within one of the ranges, the sensitivity is
 * set to that range's sensitivity. But in order to reduce flapping when the
 * brightness is right on the border between two ranges, the ranges overlap
 * somewhat (by at least one sensitivity), and sensitivity is only changed if
 * the value leaves the current sensitivity's range (i.e. there's a hysteresis
 * between the individual ranges).
 *
 * The values chosen for the map are somewhat arbitrary: a compromise of not
 * too many ranges (and hence changing the sensitivity too often) but not too
 * small or large of a percentage of the min and max values in the range
 * (currently from 7.5% to 30%, i.e. within a factor of 2 of 15%), as well as
 * just plain "this feels reasonable to me and seems to work well".
 */
struct appleals_sensitivity_map {
	int	sensitivity;
	int	illum_low;
	int	illum_high;
};

static const struct appleals_sensitivity_map appleals_sensitivity_map[] = {
	{   1,    0,   14 },
	{   3,   10,   40 },
	{   9,   30,  120 },
	{  27,   90,  360 },
	{  81,  270, 1080 },
	{ 243,  810, 3240 },
	{ 729, 2430, 9720 },
};

static int appleals_compute_sensitivity(int cur_illum, int cur_sens)
{
	const struct appleals_sensitivity_map *entry;
	int i;

	/* see if we're still in current range */
	for (i = 0; i < ARRAY_SIZE(appleals_sensitivity_map); i++) {
		entry = &appleals_sensitivity_map[i];

		if (entry->sensitivity == cur_sens &&
		    entry->illum_low <= cur_illum &&
		    entry->illum_high >= cur_illum)
			return cur_sens;
		else if (entry->sensitivity > cur_sens)
			break;
	}

	/* not in current range, so find new sensitivity */
	for (i = 0; i < ARRAY_SIZE(appleals_sensitivity_map); i++) {
		entry = &appleals_sensitivity_map[i];

		if (entry->illum_low <= cur_illum &&
		    entry->illum_high >= cur_illum)
			return entry->sensitivity;
	}

	/* hmm, not in table, so assume we are above highest range */
	i = ARRAY_SIZE(appleals_sensitivity_map) - 1;
	return appleals_sensitivity_map[i].sensitivity;
}

static int appleals_get_field_value_for_usage(struct hid_field *field,
					      unsigned int usage)
{
	int u;

	if (!field)
		return -1;

	for (u = 0; u < field->maxusage; u++) {
		if (field->usage[u].hid == usage)
			return u + field->logical_minimum;
	}

	return -1;
}

static __s32 appleals_get_field_value(struct appleals_device *als_dev,
				      struct hid_field *field)
{
	bool powered_on = !hid_hw_power(als_dev->hid_dev, PM_HINT_FULLON);

	hid_hw_request(als_dev->hid_dev, field->report, HID_REQ_GET_REPORT);
	hid_hw_wait(als_dev->hid_dev);

	if (powered_on)
		hid_hw_power(als_dev->hid_dev, PM_HINT_NORMAL);

	return field->value[0];
}

static void appleals_set_field_value(struct appleals_device *als_dev,
				     struct hid_field *field, __s32 value)
{
	hid_set_field(field, 0, value);
	hid_hw_request(als_dev->hid_dev, field->report, HID_REQ_SET_REPORT);
}

static int appleals_get_config(struct appleals_device *als_dev,
			       unsigned int field_usage, __s32 *value)
{
	struct hid_field *field;

	field = appleib_find_report_field(als_dev->cfg_report, field_usage);
	if (!field)
		return -EINVAL;

	*value = appleals_get_field_value(als_dev, field);

	return 0;
}

static int appleals_set_config(struct appleals_device *als_dev,
			       unsigned int field_usage, __s32 value)
{
	struct hid_field *field;

	field = appleib_find_report_field(als_dev->cfg_report, field_usage);
	if (!field)
		return -EINVAL;

	appleals_set_field_value(als_dev, field, value);

	return 0;
}

static int appleals_set_enum_config(struct appleals_device *als_dev,
				    unsigned int field_usage,
				    unsigned int value_usage)
{
	struct hid_field *field;
	int value;

	field = appleib_find_report_field(als_dev->cfg_report, field_usage);
	if (!field)
		return -EINVAL;

	value = appleals_get_field_value_for_usage(field, value_usage);
	if (value >= 0)
		appleals_set_field_value(als_dev, field, value);

	return 0;
}

static void appleals_update_dyn_sensitivity(struct appleals_device *als_dev,
					    __s32 value)
{
	int new_sens;
	int rc;

	new_sens = appleals_compute_sensitivity(value,
						als_dev->cur_sensitivity);
	if (new_sens != als_dev->cur_sensitivity) {
		rc = appleals_set_config(als_dev,
			HID_USAGE_SENSOR_LIGHT_ILLUM |
			HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS,
			new_sens);
		if (!rc)
			als_dev->cur_sensitivity = new_sens;
	}
}

static void appleals_push_new_value(struct appleals_device *als_dev,
				    __s32 value)
{
	__s32 buf[2] = { value, value };

	iio_push_to_buffers(als_dev->iio_dev, buf);

	if (als_dev->cur_hysteresis == APPLEALS_DYN_SENS)
		appleals_update_dyn_sensitivity(als_dev, value);
}

static int appleals_hid_event(struct hid_device *hdev, struct hid_field *field,
			      struct hid_usage *usage, __s32 value)
{
	struct appleals_device *als_dev = hid_get_drvdata(hdev);

	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_SENSOR)
		return 0;

	if (usage->hid == HID_USAGE_SENSOR_LIGHT_ILLUM) {
		appleals_push_new_value(als_dev, value);
		return 1;
	}

	return 0;
}

static int appleals_enable_events(struct iio_trigger *trig, bool enable)
{
	struct appleals_device *als_dev = iio_trigger_get_drvdata(trig);
	int value;

	appleals_set_enum_config(als_dev, HID_USAGE_SENSOR_PROP_REPORT_STATE,
		enable ? HID_USAGE_SENSOR_PROP_REPORTING_STATE_ALL_EVENTS_ENUM :
			 HID_USAGE_SENSOR_PROP_REPORTING_STATE_NO_EVENTS_ENUM);
	als_dev->events_enabled = enable;

	/* if the sensor was enabled, push an initial value */
	if (enable) {
		value = appleals_get_field_value(als_dev, als_dev->illum_field);
		appleals_push_new_value(als_dev, value);
	}

	return 0;
}

static int appleals_read_raw(struct iio_dev *iio_dev,
			     struct iio_chan_spec const *chan,
			     int *val, int *val2, long mask)
{
	struct appleals_device *als_dev = iio_priv(iio_dev);
	__s32 value;
	int rc;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		*val = appleals_get_field_value(als_dev, als_dev->illum_field);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SAMP_FREQ:
		rc = appleals_get_config(als_dev,
					 HID_USAGE_SENSOR_PROP_REPORT_INTERVAL,
					 &value);
		if (rc)
			return rc;

		/* interval is in ms; val is in HZ, val2 in µHZ */
		value = 1000000000 / value;
		*val = value / 1000000;
		*val2 = value - (*val * 1000000);

		return IIO_VAL_INT_PLUS_MICRO;

	case IIO_CHAN_INFO_HYSTERESIS:
		if (als_dev->cur_hysteresis == APPLEALS_DYN_SENS) {
			*val = als_dev->cur_hysteresis;
			return IIO_VAL_INT;
		}

		rc = appleals_get_config(als_dev,
			HID_USAGE_SENSOR_LIGHT_ILLUM |
			HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS,
			val);
		if (!rc) {
			als_dev->cur_sensitivity = *val;
			als_dev->cur_hysteresis = *val;
		}
		return rc ? rc : IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int appleals_write_raw(struct iio_dev *iio_dev,
			      struct iio_chan_spec const *chan,
			      int val, int val2, long mask)
{
	struct appleals_device *als_dev = iio_priv(iio_dev);
	__s32 illum;
	int rc;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		rc = appleals_set_config(als_dev,
					 HID_USAGE_SENSOR_PROP_REPORT_INTERVAL,
					 1000000000 / (val * 1000000 + val2));
		return rc;

	case IIO_CHAN_INFO_HYSTERESIS:
		if (val == APPLEALS_DYN_SENS) {
			if (als_dev->cur_hysteresis != APPLEALS_DYN_SENS) {
				als_dev->cur_hysteresis = val;
				illum = appleals_get_field_value(als_dev,
							als_dev->illum_field);
				appleals_update_dyn_sensitivity(als_dev, illum);
			}

			return 0;
		}

		rc = appleals_set_config(als_dev,
			HID_USAGE_SENSOR_LIGHT_ILLUM |
			HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS,
			val);
		if (!rc) {
			als_dev->cur_sensitivity = val;
			als_dev->cur_hysteresis = val;
		}

		return rc;

	default:
		return -EINVAL;
	}
}

static const struct iio_chan_spec appleals_channels[] = {
	{
		.type = IIO_INTENSITY,
		.modified = 1,
		.channel2 = IIO_MOD_LIGHT_BOTH,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_type = {
			.sign = 'u',
			.realbits = 32,
			.storagebits = 32,
		},
		.scan_index = 0,
	},
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_type = {
			.sign = 'u',
			.realbits = 32,
			.storagebits = 32,
		},
		.scan_index = 1,
	}
};

static const struct iio_trigger_ops appleals_trigger_ops = {
	.set_trigger_state = &appleals_enable_events,
};

static const struct iio_info appleals_info = {
	.read_raw = &appleals_read_raw,
	.write_raw = &appleals_write_raw,
};

static void appleals_config_sensor(struct appleals_device *als_dev,
				   bool events_enabled, int sensitivity)
{
	struct hid_field *field;
	bool powered_on;
	__s32 val;

	powered_on = !hid_hw_power(als_dev->hid_dev, PM_HINT_FULLON);

	hid_hw_request(als_dev->hid_dev, als_dev->cfg_report,
		       HID_REQ_GET_REPORT);
	hid_hw_wait(als_dev->hid_dev);

	field = appleib_find_report_field(als_dev->cfg_report,
					  HID_USAGE_SENSOR_PROY_POWER_STATE);
	val = appleals_get_field_value_for_usage(field,
			HID_USAGE_SENSOR_PROP_POWER_STATE_D0_FULL_POWER_ENUM);
	if (val >= 0)
		hid_set_field(field, 0, val);

	field = appleib_find_report_field(als_dev->cfg_report,
					  HID_USAGE_SENSOR_PROP_REPORT_STATE);
	val = appleals_get_field_value_for_usage(field,
		events_enabled ?
			HID_USAGE_SENSOR_PROP_REPORTING_STATE_ALL_EVENTS_ENUM :
			HID_USAGE_SENSOR_PROP_REPORTING_STATE_NO_EVENTS_ENUM);
	if (val >= 0)
		hid_set_field(field, 0, val);

	field = appleib_find_report_field(als_dev->cfg_report,
					 HID_USAGE_SENSOR_PROP_REPORT_INTERVAL);
	hid_set_field(field, 0, field->logical_minimum);

	/*
	 * Set initial change sensitivity; if dynamic, enabling trigger will set
	 * it instead.
	 */
	if (sensitivity != APPLEALS_DYN_SENS) {
		field = appleib_find_report_field(als_dev->cfg_report,
			HID_USAGE_SENSOR_LIGHT_ILLUM |
			HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS);

		hid_set_field(field, 0, sensitivity);
	}

	hid_hw_request(als_dev->hid_dev, als_dev->cfg_report,
		       HID_REQ_SET_REPORT);

	if (powered_on)
		hid_hw_power(als_dev->hid_dev, PM_HINT_NORMAL);
}

static int appleals_config_iio(struct appleals_device *als_dev)
{
	struct iio_dev *iio_dev = als_dev->iio_dev;
	struct iio_trigger *iio_trig;
	struct device *parent = &als_dev->hid_dev->dev;
	int rc;

	iio_dev->channels = appleals_channels;
	iio_dev->num_channels = ARRAY_SIZE(appleals_channels);
	iio_dev->dev.parent = parent;
	iio_dev->info = &appleals_info;
	iio_dev->name = "als";
	iio_dev->modes = INDIO_DIRECT_MODE;

	rc = devm_iio_triggered_buffer_setup(parent, iio_dev,
					     &iio_pollfunc_store_time, NULL,
					     NULL);
	if (rc) {
		hid_err(als_dev->hid_dev,
			"Failed to set up iio triggered buffer: %d\n", rc);
		return rc;
	}

	iio_trig = devm_iio_trigger_alloc(parent, "%s-dev%d", iio_dev->name,
					  iio_dev->id);
	if (!iio_trig)
		return -ENOMEM;

	iio_trig->dev.parent = parent;
	iio_trig->ops = &appleals_trigger_ops;
	iio_trigger_set_drvdata(iio_trig, als_dev);

	rc = devm_iio_trigger_register(parent, iio_trig);
	if (rc) {
		hid_err(als_dev->hid_dev,
			"Failed to register iio trigger: %d\n",
			rc);
		return rc;
	}

	rc = devm_iio_device_register(parent, iio_dev);
	if (rc) {
		hid_err(als_dev->hid_dev, "Failed to register iio device: %d\n",
			rc);
		return rc;
	}

	als_dev->iio_dev = iio_dev;

	return 0;
}

static int appleals_probe(struct hid_device *hdev,
			  const struct hid_device_id *id)
{
	struct appleals_device *als_dev;
	struct iio_dev *iio_dev;
	struct hid_field *state_field;
	struct hid_field *illum_field;
	int rc;

	/* find als fields and reports */
	rc = hid_parse(hdev);
	if (rc) {
		hid_err(hdev, "als: hid parse failed (%d)\n", rc);
		return rc;
	}

	state_field = appleib_find_hid_field(hdev, HID_USAGE_SENSOR_ALS,
					    HID_USAGE_SENSOR_PROP_REPORT_STATE);
	illum_field = appleib_find_hid_field(hdev, HID_USAGE_SENSOR_ALS,
					     HID_USAGE_SENSOR_LIGHT_ILLUM);
	if (!state_field || !illum_field)
		return -ENODEV;

	hid_dbg(hdev, "Found ambient light sensor\n");

	/* initialize device */
	iio_dev = devm_iio_device_alloc(&hdev->dev, sizeof(*als_dev));
	if (!iio_dev)
		return -ENOMEM;

	als_dev = iio_priv(iio_dev);

	als_dev->hid_dev = hdev;
	als_dev->iio_dev = iio_dev;
	als_dev->cfg_report = state_field->report;
	als_dev->illum_field = illum_field;

	als_dev->cur_hysteresis = APPLEALS_DEF_CHANGE_SENS;
	als_dev->cur_sensitivity = APPLEALS_DEF_CHANGE_SENS;

	hid_set_drvdata(hdev, als_dev);

	rc = hid_hw_start(hdev, HID_CONNECT_DRIVER);
	if (rc) {
		hid_err(hdev, "als: hw start failed (%d)\n", rc);
		return rc;
	}

	hid_device_io_start(hdev);
	appleals_config_sensor(als_dev, false, als_dev->cur_sensitivity);
	hid_device_io_stop(hdev);

	rc = appleals_config_iio(als_dev);
	if (rc)
		return rc;

	return hid_hw_open(hdev);
}

#ifdef CONFIG_PM
static int appleals_reset_resume(struct hid_device *hdev)
{
	struct appleals_device *als_dev = hid_get_drvdata(hdev);
	__s32 illum;

	appleals_config_sensor(als_dev, als_dev->events_enabled,
			       als_dev->cur_sensitivity);

	illum = appleals_get_field_value(als_dev, als_dev->illum_field);
	appleals_push_new_value(als_dev, illum);

	return 0;
}
#endif

static const struct hid_device_id appleals_hid_ids[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_LINUX_FOUNDATION,
			 USB_DEVICE_ID_IBRIDGE_ALS) },
	{ HID_USB_DEVICE(/* USB_VENDOR_ID_APPLE */ 0x05ac, 0x8262) },
	{ },
};

MODULE_DEVICE_TABLE(hid, appleals_hid_ids);

static struct hid_driver appleals_hid_driver = {
	.name = "apple-ib-als",
	.id_table = appleals_hid_ids,
	.probe = appleals_probe,
	.event = appleals_hid_event,
#ifdef CONFIG_PM
	.reset_resume = appleals_reset_resume,
#endif
};

module_hid_driver(appleals_hid_driver);

MODULE_AUTHOR("Ronald Tschalär");
MODULE_DESCRIPTION("Apple iBridge ALS driver");
MODULE_LICENSE("GPL v2");
