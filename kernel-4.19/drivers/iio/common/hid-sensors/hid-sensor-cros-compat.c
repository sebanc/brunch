/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/hid-sensor-hub.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/kfifo_buf.h>

/************************************POWER*************************************/

static int hid_sensor_cros_compat_power_state(struct hid_sensor_common *st, bool state)
{
	int state_val;
	int report_val;
	s32 poll_value = 0;

	if (state) {
		if (sensor_hub_device_open(st->hsdev))
			return -EIO;

		atomic_inc(&st->data_ready);

		state_val = hid_sensor_get_usage_index(st->hsdev,
			st->power_state.report_id,
			st->power_state.index,
			HID_USAGE_SENSOR_PROP_POWER_STATE_D0_FULL_POWER_ENUM);
		report_val = hid_sensor_get_usage_index(st->hsdev,
			st->report_state.report_id,
			st->report_state.index,
			HID_USAGE_SENSOR_PROP_REPORTING_STATE_ALL_EVENTS_ENUM);

		poll_value = hid_sensor_read_poll_value(st);
	} else {
		int val;

		val = atomic_dec_if_positive(&st->data_ready);
		if (val < 0)
			return 0;

		sensor_hub_device_close(st->hsdev);
		state_val = hid_sensor_get_usage_index(st->hsdev,
			st->power_state.report_id,
			st->power_state.index,
			HID_USAGE_SENSOR_PROP_POWER_STATE_D4_POWER_OFF_ENUM);
		report_val = hid_sensor_get_usage_index(st->hsdev,
			st->report_state.report_id,
			st->report_state.index,
			HID_USAGE_SENSOR_PROP_REPORTING_STATE_NO_EVENTS_ENUM);
	}

	if (state_val >= 0) {
		state_val += st->power_state.logical_minimum;
		sensor_hub_set_feature(st->hsdev, st->power_state.report_id,
				       st->power_state.index, sizeof(state_val),
				       &state_val);
	}

	if (report_val >= 0) {
		report_val += st->report_state.logical_minimum;
		sensor_hub_set_feature(st->hsdev, st->report_state.report_id,
				       st->report_state.index,
				       sizeof(report_val),
				       &report_val);
	}

	pr_debug("HID_SENSOR %s set power_state %d report_state %d\n",
		 st->pdev->name, state_val, report_val);

	sensor_hub_get_feature(st->hsdev, st->power_state.report_id,
			       st->power_state.index,
			       sizeof(state_val), &state_val);
	if (state && poll_value)
		msleep_interruptible(poll_value * 2);

	return 0;
}

/************************************RING**************************************/

struct cros_ec_sensors_ring {
	struct platform_device *cros_ec_ring_pdev;
	struct iio_dev *cros_ec_ring_dev;
	struct accel_3d_state *accel_state;
	struct als_state *als_state;
	int initialised;
	int sensors_in_ring;
};

struct cros_ec_sensors_ring ring;

struct cros_ec_sensors_ring_sample {
	uint8_t sensor_id;
	uint8_t flag;
	int16_t vector[3];
	s64     timestamp;
} __packed;

enum {
	CHANNEL_SENSOR_ID,
	CHANNEL_SENSOR_FLAG,
	CHANNEL_X,
	CHANNEL_Y,
	CHANNEL_Z,
	CHANNEL_TIMESTAMP,
	MAX_CHANNEL,
};

#define CROS_EC_RING_ID(_id, _name)		\
{						\
	.type = IIO_ACCEL,			\
	.scan_index = _id,			\
	.scan_type = {				\
		.sign = 'u',			\
		.realbits = 8,			\
		.storagebits = 8,		\
	},					\
	.extend_name = _name,			\
}

#define CROS_EC_RING_AXIS(_axis)		\
{						\
	.type = IIO_ACCEL,			\
	.modified = 1,				\
	.channel2 = IIO_MOD_##_axis,		\
	.scan_index = CHANNEL_##_axis,		\
	.scan_type = {				\
		.sign = 's',			\
		.realbits = 16,			\
		.storagebits = 16,		\
	},					\
	.extend_name = "ring",			\
}

static const struct iio_chan_spec cros_ec_ring_channels[] = {
	CROS_EC_RING_ID(CHANNEL_SENSOR_ID, "id"),
	CROS_EC_RING_ID(CHANNEL_SENSOR_FLAG, "flag"),
	CROS_EC_RING_AXIS(X),
	CROS_EC_RING_AXIS(Y),
	CROS_EC_RING_AXIS(Z),
	IIO_CHAN_SOFT_TIMESTAMP(CHANNEL_TIMESTAMP)
};

static const struct iio_info ec_sensors_info = { };

static void cros_ec_ring_init(void)
{
	struct iio_buffer *buffer;
	int ret;

	ring.cros_ec_ring_pdev = platform_device_register_simple("cros-ec-ring-compat", 0, NULL, 0);
	if (IS_ERR(ring.cros_ec_ring_pdev))
		return;

	ring.cros_ec_ring_dev = devm_iio_device_alloc(&ring.cros_ec_ring_pdev->dev, sizeof(struct cros_ec_sensors_ring_sample));
	if (ring.cros_ec_ring_dev == NULL) {
		dev_err(&ring.cros_ec_ring_pdev->dev, "device allocation failed\n");
		platform_device_unregister(ring.cros_ec_ring_pdev);
		return;
	}

	platform_set_drvdata(ring.cros_ec_ring_pdev, ring.cros_ec_ring_dev);

	ring.cros_ec_ring_dev->dev.parent = &ring.cros_ec_ring_pdev->dev;
	ring.cros_ec_ring_dev->name = "cros-ec-ring";
	ring.cros_ec_ring_dev->channels = cros_ec_ring_channels;
	ring.cros_ec_ring_dev->num_channels = ARRAY_SIZE(cros_ec_ring_channels);
	ring.cros_ec_ring_dev->info = &ec_sensors_info;
	ring.cros_ec_ring_dev->modes = INDIO_BUFFER_SOFTWARE;

	buffer = devm_iio_kfifo_allocate(ring.cros_ec_ring_dev->dev.parent);
	if (!buffer) {
		dev_err(&ring.cros_ec_ring_pdev->dev, "buffer allocation failed\n");
		platform_device_unregister(ring.cros_ec_ring_pdev);
		return;
	}

	iio_device_attach_buffer(ring.cros_ec_ring_dev, buffer);

	ret = iio_device_register(ring.cros_ec_ring_dev);
	if (ret) {
		dev_err(&ring.cros_ec_ring_pdev->dev, "device register failed\n");
		platform_device_unregister(ring.cros_ec_ring_pdev);
		return;
	}

	ring.initialised = 1;
}

static void cros_ec_ring_fini(void)
{
	ring.initialised = 0;

	iio_device_unregister(ring.cros_ec_ring_dev);
	platform_device_unregister(ring.cros_ec_ring_pdev);
}

/**********************************ATTRIBUTES**********************************/

static ssize_t cros_compat_calibrate(struct iio_dev *indio_dev,
		uintptr_t private, const struct iio_chan_spec *chan,
		const char *buf, size_t len)
{
	int ret;
	bool calibrate;

	ret = strtobool(buf, &calibrate);
	if (ret < 0)
		return ret;

	return ret ? ret : len;
}

static ssize_t cros_compat_id(struct iio_dev *indio_dev,
					uintptr_t private,
					const struct iio_chan_spec *chan,
					char *buf)
{
	if (chan->type == IIO_ACCEL)
		return sprintf(buf, "0\n");
	else if (chan->type == IIO_INTENSITY)
		return sprintf(buf, "4\n");
	else
		return sprintf(buf, "9\n");
}

static ssize_t cros_compat_loc(struct iio_dev *indio_dev,
					uintptr_t private,
					const struct iio_chan_spec *chan,
					char *buf)
{
	return sprintf(buf, "lid\n");
}

static const struct iio_chan_spec_ext_info cros_compat_ext_info[] = {
	{
		.name = "calibrate",
		.shared = IIO_SHARED_BY_ALL,
		.write = cros_compat_calibrate
	},
	{
		.name = "id",
		.shared = IIO_SHARED_BY_ALL,
		.read = cros_compat_id
	},
	{
		.name = "location",
		.shared = IIO_SHARED_BY_ALL,
		.read = cros_compat_loc
	},
	{ }
};

/************************************ALS***************************************/

enum {
	CHANNEL_SCAN_INDEX_INTENSITY,
	CHANNEL_SCAN_INDEX_ILLUM,
	CHANNEL_SCAN_INDEX_LIGHT_MAX,
};

struct als_state {
	struct hid_sensor_hub_callbacks callbacks;
	struct hid_sensor_common common_attributes;
	struct hid_sensor_hub_attribute_info als_illum;
	u16 illum[CHANNEL_SCAN_INDEX_LIGHT_MAX];
	int scale_pre_factor;
	int scale_post_factor;
	int scale_pre_decml;
	int scale_post_decml;
	int scale_precision;
	int value_offset;
};

static const struct iio_chan_spec als_channels[] = {
	{
		.type = IIO_INTENSITY,
		.modified = 1,
		.channel2 = IIO_MOD_LIGHT_BOTH,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_CALIBBIAS) |
			BIT(IIO_CHAN_INFO_CALIBSCALE),
		.info_mask_shared_by_all =
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_FREQUENCY) |
			BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.ext_info = cros_compat_ext_info,
		.scan_index = CHANNEL_SCAN_INDEX_INTENSITY,
	},
	{
		.type = IIO_LIGHT,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_CALIBBIAS) |
			BIT(IIO_CHAN_INFO_CALIBSCALE),
		.info_mask_shared_by_all =
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_FREQUENCY) |
			BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.ext_info = cros_compat_ext_info,
		.scan_index = CHANNEL_SCAN_INDEX_ILLUM,
	}
};

static void als_adjust_channel_bit_mask(struct iio_chan_spec *channels,
					int channel, int size)
{
	channels[channel].scan_type.sign = 'u';
	channels[channel].scan_type.realbits = 16;
	channels[channel].scan_type.storagebits = 16;
}

static int als_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val, int *val2,
			      long mask)
{
	struct als_state *als_state = iio_priv(indio_dev);
	int report_id = -1;
	u32 address;
	int ret_type;
	s32 min;

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->scan_index) {
		case  CHANNEL_SCAN_INDEX_INTENSITY:
		case  CHANNEL_SCAN_INDEX_ILLUM:
			report_id = als_state->als_illum.report_id;
			min = als_state->als_illum.logical_minimum;
			address = HID_USAGE_SENSOR_LIGHT_ILLUM;
			break;
		default:
			report_id = -1;
			break;
		}
		if (report_id >= 0) {
			*val = sensor_hub_input_attr_get_raw_value(
					als_state->common_attributes.hsdev,
					HID_USAGE_SENSOR_ALS, address,
					report_id,
					SENSOR_HUB_SYNC,
					min < 0) * (als_state->scale_pre_factor * (s64)1000000000 + als_state->scale_post_factor) / (s64)1000000000;
		} else {
			*val = 0;
			return -EINVAL;
		}
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = als_state->scale_pre_decml;
		*val2 = als_state->scale_post_decml;
		ret_type = als_state->scale_precision;
		break;
	case IIO_CHAN_INFO_OFFSET:
		*val = als_state->value_offset;
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		ret_type = hid_sensor_read_samp_freq_value(
				&als_state->common_attributes, val, val2);
		break;
	case IIO_CHAN_INFO_HYSTERESIS:
		ret_type = hid_sensor_read_raw_hyst_value(
				&als_state->common_attributes, val, val2);
		break;
	default:
		ret_type = IIO_VAL_INT;
		break;
	}

	return ret_type;
}

static int als_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	return 0;
}

static const struct iio_info als_info = {
	.read_raw = &als_read_raw,
	.write_raw = &als_write_raw,
};

static int als_capture_sample(struct hid_sensor_hub_device *hsdev,
				unsigned usage_id,
				size_t raw_len, char *raw_data,
				void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct als_state *als_state = iio_priv(indio_dev);
	int ret = -EINVAL;
	u16 sample_data = (u16)(((*(u32 *)(raw_data)) * (als_state->scale_pre_factor * (s64)1000000000 + als_state->scale_post_factor)) / (s64)1000000000);

	switch (usage_id) {
	case HID_USAGE_SENSOR_LIGHT_ILLUM:
		als_state->illum[CHANNEL_SCAN_INDEX_INTENSITY] = sample_data;
		als_state->illum[CHANNEL_SCAN_INDEX_ILLUM] = sample_data;
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

static int als_parse_report(struct platform_device *pdev,
				struct hid_sensor_hub_device *hsdev,
				struct iio_chan_spec *channels,
				unsigned usage_id,
				struct als_state *st)
{
	int ret;

	ret = sensor_hub_input_get_attribute_info(hsdev, HID_INPUT_REPORT,
			usage_id,
			HID_USAGE_SENSOR_LIGHT_ILLUM,
			&st->als_illum);
	if (ret < 0)
		return ret;
	als_adjust_channel_bit_mask(channels, CHANNEL_SCAN_INDEX_INTENSITY,
				    st->als_illum.size);
	als_adjust_channel_bit_mask(channels, CHANNEL_SCAN_INDEX_ILLUM,
					st->als_illum.size);

	dev_dbg(&pdev->dev, "als %x:%x\n", st->als_illum.index,
			st->als_illum.report_id);

	st->scale_precision = hid_sensor_format_scale(
				HID_USAGE_SENSOR_ALS,
				&st->als_illum,
				&st->scale_pre_decml, &st->scale_post_decml);

	st->scale_pre_factor = (st->scale_pre_decml * (s64)1000000000 + st->scale_post_decml) / (s64)1000000000;
	st->scale_post_factor = (((st->scale_pre_decml * (s64)1000000000 + st->scale_post_decml) - (st->scale_pre_factor * (s64)1000000000)) * ((s64)1000000000 / (s64)1000000000));
	st->scale_pre_decml = 1;
	st->scale_post_decml = 0;

	if (st->common_attributes.sensitivity.index < 0) {
		sensor_hub_input_get_attribute_info(hsdev,
			HID_FEATURE_REPORT, usage_id,
			HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS |
			HID_USAGE_SENSOR_DATA_LIGHT,
			&st->common_attributes.sensitivity);
		dev_dbg(&pdev->dev, "Sensitivity index:report %d:%d\n",
			st->common_attributes.sensitivity.index,
			st->common_attributes.sensitivity.report_id);
	}
	return ret;
}

static int hid_als_probe(struct platform_device *pdev)
{
	int ret = 0;
	static const char *name = "cros-ec-light";
	struct iio_dev *indio_dev;
	struct als_state *als_state;
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct als_state));
	if (!indio_dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, indio_dev);

	als_state = iio_priv(indio_dev);
	als_state->common_attributes.hsdev = hsdev;
	als_state->common_attributes.pdev = pdev;

	ret = hid_sensor_parse_common_attributes(hsdev, HID_USAGE_SENSOR_ALS,
					&als_state->common_attributes);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup common attributes\n");
		return ret;
	}

	indio_dev->channels = kmemdup(als_channels,
				      sizeof(als_channels), GFP_KERNEL);
	if (!indio_dev->channels) {
		dev_err(&pdev->dev, "failed to duplicate channels\n");
		return -ENOMEM;
	}

	ret = als_parse_report(pdev, hsdev,
			       (struct iio_chan_spec *)indio_dev->channels,
			       HID_USAGE_SENSOR_ALS, als_state);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup attributes\n");
		goto error_free_dev_mem;
	}

	indio_dev->num_channels =
				ARRAY_SIZE(als_channels);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &als_info;
	indio_dev->name = name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time,
		NULL, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize trigger buffer\n");
		goto error_free_dev_mem;
	}
	atomic_set(&als_state->common_attributes.data_ready, 0);
	iio_device_set_drvdata(indio_dev, &als_state->common_attributes);

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "device register failed\n");
		goto error_unreg_buffer_funcs;
	}

	als_state->callbacks.capture_sample = als_capture_sample;
	als_state->callbacks.pdev = pdev;
	ret = sensor_hub_register_callback(hsdev, HID_USAGE_SENSOR_ALS,
					&als_state->callbacks);
	if (ret < 0) {
		dev_err(&pdev->dev, "callback reg failed\n");
		goto error_iio_unreg;
	}

	hid_sensor_cros_compat_power_state(&als_state->common_attributes, true);

	ring.als_state = als_state;

	return ret;

error_iio_unreg:
	iio_device_unregister(indio_dev);
error_unreg_buffer_funcs:
	iio_triggered_buffer_cleanup(indio_dev);
error_free_dev_mem:
	kfree(indio_dev->channels);
	return ret;
}

static int hid_als_remove(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct als_state *als_state = iio_priv(indio_dev);

	ring.als_state = NULL;

	hid_sensor_cros_compat_power_state(&als_state->common_attributes, false);
	sensor_hub_remove_callback(hsdev, HID_USAGE_SENSOR_ALS);
	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	kfree(indio_dev->channels);

	return 0;
}

/************************************ACCEL*************************************/

enum channel_scan_indexes {
	CHANNEL_SCAN_INDEX_X,
	CHANNEL_SCAN_INDEX_Y,
	CHANNEL_SCAN_INDEX_Z,
	CHANNEL_SCAN_INDEX_MAX,
};

struct accel_3d_state {
	struct hid_sensor_hub_callbacks callbacks;
	struct hid_sensor_common common_attributes;
	struct hid_sensor_hub_attribute_info accel[CHANNEL_SCAN_INDEX_MAX];
	u16 accel_val[CHANNEL_SCAN_INDEX_MAX];
	int scale_pre_factor;
	int scale_post_factor;
	int scale_pre_decml;
	int scale_post_decml;
	int scale_precision;
	int value_offset;
	int64_t timestamp;
};

static const u32 accel_3d_addresses[CHANNEL_SCAN_INDEX_MAX] = {
	HID_USAGE_SENSOR_ACCEL_X_AXIS,
	HID_USAGE_SENSOR_ACCEL_Y_AXIS,
	HID_USAGE_SENSOR_ACCEL_Z_AXIS
};

static const struct iio_chan_spec accel_3d_channels[] = {
	{
		.type = IIO_ACCEL,
		.modified = 1,
		.channel2 = IIO_MOD_X,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_CALIBBIAS) |
			BIT(IIO_CHAN_INFO_CALIBSCALE),
		.info_mask_shared_by_all =
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_FREQUENCY) |
			BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.ext_info = cros_compat_ext_info,
		.scan_index = CHANNEL_SCAN_INDEX_X,
	}, {
		.type = IIO_ACCEL,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_CALIBBIAS) |
			BIT(IIO_CHAN_INFO_CALIBSCALE),
		.info_mask_shared_by_all =
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_FREQUENCY) |
			BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.ext_info = cros_compat_ext_info,
		.scan_index = CHANNEL_SCAN_INDEX_Y,
	}, {
		.type = IIO_ACCEL,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_CALIBBIAS) |
			BIT(IIO_CHAN_INFO_CALIBSCALE),
		.info_mask_shared_by_all =
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_FREQUENCY) |
			BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.ext_info = cros_compat_ext_info,
		.scan_index = CHANNEL_SCAN_INDEX_Z,
	},
	IIO_CHAN_SOFT_TIMESTAMP(3)
};

static void accel_3d_adjust_channel_bit_mask(struct iio_chan_spec *channels,
						int channel, int size)
{
	channels[channel].scan_type.sign = 's';
	channels[channel].scan_type.realbits = 16;
	channels[channel].scan_type.storagebits = 16;
}

static int accel_3d_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val, int *val2,
			      long mask)
{
	struct accel_3d_state *accel_state = iio_priv(indio_dev);
	int report_id = -1;
	u32 address;
	int ret_type;
	s32 min;
	struct hid_sensor_hub_device *hsdev =
					accel_state->common_attributes.hsdev;

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		report_id = accel_state->accel[chan->scan_index].report_id;
		min = accel_state->accel[chan->scan_index].logical_minimum;
		address = accel_3d_addresses[chan->scan_index];
		if (report_id >= 0)
			*val = -sensor_hub_input_attr_get_raw_value(
					accel_state->common_attributes.hsdev,
					hsdev->usage, address, report_id,
					SENSOR_HUB_SYNC,
					min < 0) * (accel_state->scale_pre_factor * (s64)1000000000 + accel_state->scale_post_factor) / (s64)1000000000;
		else {
			*val = 0;
			return -EINVAL;
		}
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = accel_state->scale_pre_decml;
		*val2 = accel_state->scale_post_decml;
		ret_type = accel_state->scale_precision;
		break;
	case IIO_CHAN_INFO_OFFSET:
		*val = accel_state->value_offset;
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		ret_type = hid_sensor_read_samp_freq_value(
			&accel_state->common_attributes, val, val2);
		break;
	case IIO_CHAN_INFO_HYSTERESIS:
		ret_type = hid_sensor_read_raw_hyst_value(
			&accel_state->common_attributes, val, val2);
		break;
	default:
		ret_type = IIO_VAL_INT;
		break;
	}

	return ret_type;
}

static int accel_3d_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	return 0;
}

static const struct iio_info accel_3d_info = {
	.read_raw = &accel_3d_read_raw,
	.write_raw = &accel_3d_write_raw,
};

static int accel_3d_capture_sample(struct hid_sensor_hub_device *hsdev,
				unsigned usage_id,
				size_t raw_len, char *raw_data,
				void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct accel_3d_state *accel_state = iio_priv(indio_dev);
	int offset;
	int ret = -EINVAL;

	switch (usage_id) {
	case HID_USAGE_SENSOR_ACCEL_X_AXIS:
	case HID_USAGE_SENSOR_ACCEL_Y_AXIS:
	case HID_USAGE_SENSOR_ACCEL_Z_AXIS:
		offset = usage_id - HID_USAGE_SENSOR_ACCEL_X_AXIS;
		accel_state->accel_val[CHANNEL_SCAN_INDEX_X + offset] =
			(u16)((-(*(s32 *)(raw_data)) * (accel_state->scale_pre_factor * (s64)1000000000 + accel_state->scale_post_factor)) / (s64)1000000000);
		ret = 0;
	break;
	case HID_USAGE_SENSOR_TIME_TIMESTAMP:
		accel_state->timestamp =
			hid_sensor_convert_timestamp(
					&accel_state->common_attributes,
					*(int64_t *)raw_data);
	break;
	default:
		break;
	}

	return ret;
}

static int accel_3d_parse_report(struct platform_device *pdev,
				struct hid_sensor_hub_device *hsdev,
				struct iio_chan_spec *channels,
				unsigned usage_id,
				struct accel_3d_state *st)
{
	int ret;
	int i;

	for (i = 0; i <= CHANNEL_SCAN_INDEX_Z; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_INPUT_REPORT,
				usage_id,
				HID_USAGE_SENSOR_ACCEL_X_AXIS + i,
				&st->accel[CHANNEL_SCAN_INDEX_X + i]);
		if (ret < 0)
			break;
		accel_3d_adjust_channel_bit_mask(channels,
				CHANNEL_SCAN_INDEX_X + i,
				st->accel[CHANNEL_SCAN_INDEX_X + i].size);
	}
	dev_dbg(&pdev->dev, "accel_3d %x:%x, %x:%x, %x:%x\n",
			st->accel[0].index,
			st->accel[0].report_id,
			st->accel[1].index, st->accel[1].report_id,
			st->accel[2].index, st->accel[2].report_id);

	st->scale_precision = hid_sensor_format_scale(
				hsdev->usage,
				&st->accel[CHANNEL_SCAN_INDEX_X],
				&st->scale_pre_decml, &st->scale_post_decml);

	st->scale_pre_factor = (st->scale_pre_decml * (s64)1000000000 + st->scale_post_decml) / 980665;
	st->scale_post_factor = (((st->scale_pre_decml * (s64)1000000000 + st->scale_post_decml) - (st->scale_pre_factor * 980665)) * ((s64)1000000000 / 980665));
	st->scale_pre_decml = 0;
	st->scale_post_decml = 980665;

	if (st->common_attributes.sensitivity.index < 0) {
		sensor_hub_input_get_attribute_info(hsdev,
			HID_FEATURE_REPORT, usage_id,
			HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS |
			HID_USAGE_SENSOR_DATA_ACCELERATION,
			&st->common_attributes.sensitivity);
		dev_dbg(&pdev->dev, "Sensitivity index:report %d:%d\n",
			st->common_attributes.sensitivity.index,
			st->common_attributes.sensitivity.report_id);
	}

	return ret;
}

irqreturn_t cros_compat_sensors_capture(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cros_ec_sensors_ring_sample *cros_ec_ring_sample = iio_priv(ring.cros_ec_ring_dev);

	if (ring.accel_state && atomic_read(&ring.accel_state->common_attributes.data_ready)) {
		iio_push_to_buffers_with_timestamp(indio_dev,
				     ring.accel_state->accel_val,
				     iio_get_time_ns(indio_dev));
		cros_ec_ring_sample->sensor_id = 0;
		cros_ec_ring_sample->flag = 0;
		cros_ec_ring_sample->vector[0] = ring.accel_state->accel_val[0];
		cros_ec_ring_sample->vector[1] = ring.accel_state->accel_val[1];
		cros_ec_ring_sample->vector[2] = ring.accel_state->accel_val[2];
		cros_ec_ring_sample->timestamp = iio_get_time_ns(ring.cros_ec_ring_dev);
		iio_push_to_buffers(ring.cros_ec_ring_dev,  cros_ec_ring_sample);
	}

	if (ring.als_state && atomic_read(&ring.als_state->common_attributes.data_ready)) {
		cros_ec_ring_sample->sensor_id = 4;
		cros_ec_ring_sample->flag = 0;
		cros_ec_ring_sample->vector[0] = ring.als_state->illum[1];
		cros_ec_ring_sample->vector[1] = 0;
		cros_ec_ring_sample->vector[2] = 0;
		cros_ec_ring_sample->timestamp = iio_get_time_ns(ring.cros_ec_ring_dev);
		iio_push_to_buffers(ring.cros_ec_ring_dev,
				     cros_ec_ring_sample);
	}

	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int hid_accel_3d_probe(struct platform_device *pdev)
{
	int ret = 0;
	const char *name;
	struct iio_dev *indio_dev;
	struct accel_3d_state *accel_state;
	const struct iio_chan_spec *channel_spec;
	int channel_size;

	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;

	indio_dev = devm_iio_device_alloc(&pdev->dev,
					  sizeof(struct accel_3d_state));
	if (indio_dev == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, indio_dev);

	accel_state = iio_priv(indio_dev);
	accel_state->common_attributes.hsdev = hsdev;
	accel_state->common_attributes.pdev = pdev;

	name = "cros-ec-accel";
	channel_spec = accel_3d_channels;
	channel_size = sizeof(accel_3d_channels);
	indio_dev->num_channels = ARRAY_SIZE(accel_3d_channels);

	ret = hid_sensor_parse_common_attributes(hsdev, hsdev->usage,
					&accel_state->common_attributes);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup common attributes\n");
		return ret;
	}
	indio_dev->channels = kmemdup(channel_spec, channel_size, GFP_KERNEL);

	if (!indio_dev->channels) {
		dev_err(&pdev->dev, "failed to duplicate channels\n");
		return -ENOMEM;
	}
	ret = accel_3d_parse_report(pdev, hsdev,
				(struct iio_chan_spec *)indio_dev->channels,
				hsdev->usage, accel_state);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup attributes\n");
		goto error_free_dev_mem;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &accel_3d_info;
	indio_dev->name = name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_triggered_buffer_setup(indio_dev, NULL, cros_compat_sensors_capture, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize trigger buffer\n");
		goto error_free_dev_mem;
	}
	atomic_set(&accel_state->common_attributes.data_ready, 0);
	iio_device_set_drvdata(indio_dev, &accel_state->common_attributes);

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "device register failed\n");
		goto error_unreg_buffer_funcs;
	}

	accel_state->callbacks.capture_sample = accel_3d_capture_sample;
	accel_state->callbacks.pdev = pdev;
	ret = sensor_hub_register_callback(hsdev, hsdev->usage,
					&accel_state->callbacks);
	if (ret < 0) {
		dev_err(&pdev->dev, "callback reg failed\n");
		goto error_iio_unreg;
	}

	hid_sensor_cros_compat_power_state(&accel_state->common_attributes, true);

	ring.accel_state = accel_state;

	return ret;

error_iio_unreg:
	iio_device_unregister(indio_dev);
error_unreg_buffer_funcs:
	iio_triggered_buffer_cleanup(indio_dev);
error_free_dev_mem:
	kfree(indio_dev->channels);
	return ret;
}

static int hid_accel_3d_remove(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct accel_3d_state *accel_state = iio_priv(indio_dev);

	ring.accel_state = NULL;

	hid_sensor_cros_compat_power_state(&accel_state->common_attributes, false);
	sensor_hub_remove_callback(hsdev, hsdev->usage);
	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	kfree(indio_dev->channels);

	return 0;
}

/************************************INIT**************************************/

static int hid_sensor_cros_compat_probe(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	int ret = 0;

	if (hsdev->usage != HID_USAGE_SENSOR_ACCEL_3D &&
	    hsdev->usage != HID_USAGE_SENSOR_ALS) {
		dev_dbg(&pdev->dev, "sensors usage %x not implemented\n", hsdev->usage);
		return 0;
	}

	if (!ring.sensors_in_ring)
		cros_ec_ring_init();

	ring.sensors_in_ring++;

	if (hsdev->usage == HID_USAGE_SENSOR_ACCEL_3D)
		ret = hid_accel_3d_probe(pdev);
	else if (hsdev->usage == HID_USAGE_SENSOR_ALS)
		ret = hid_als_probe(pdev);

	return ret;
}

static int hid_sensor_cros_compat_remove(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	int ret = 0;

	if (hsdev->usage != HID_USAGE_SENSOR_ACCEL_3D &&
	    hsdev->usage != HID_USAGE_SENSOR_ALS)
		return 0;

	if (hsdev->usage == HID_USAGE_SENSOR_ACCEL_3D)
		ret = hid_accel_3d_remove(pdev);
	else if (hsdev->usage == HID_USAGE_SENSOR_ALS)
		ret = hid_als_remove(pdev);

	ring.sensors_in_ring--;

	if (ring.initialised && !ring.sensors_in_ring)
		cros_ec_ring_fini();

	return ret;
}

static int hid_sensor_cros_compat_suspend(struct device *dev)
{
	struct hid_sensor_hub_device *hsdev = to_platform_device(dev)->dev.platform_data;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct hid_sensor_common *attrb = iio_device_get_drvdata(indio_dev);

	if (hsdev->usage != HID_USAGE_SENSOR_ACCEL_3D &&
	    hsdev->usage != HID_USAGE_SENSOR_ALS)
		return 0;

	return hid_sensor_cros_compat_power_state(attrb, false);
}

static int hid_sensor_cros_compat_resume(struct device *dev)
{
	struct hid_sensor_hub_device *hsdev = to_platform_device(dev)->dev.platform_data;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct hid_sensor_common *attrb = iio_device_get_drvdata(indio_dev);

	if (hsdev->usage != HID_USAGE_SENSOR_ACCEL_3D &&
	    hsdev->usage != HID_USAGE_SENSOR_ALS)
		return 0;

	return hid_sensor_cros_compat_power_state(attrb, true);
}

const struct dev_pm_ops hid_sensor_cros_compat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hid_sensor_cros_compat_suspend, hid_sensor_cros_compat_resume)
};

static const struct platform_device_id hid_sensor_cros_compat_ids[] = {
	{
		.name = "cros-ec-compat",
	},
	{ }
};
MODULE_DEVICE_TABLE(platform, hid_sensor_cros_compat_ids);

static struct platform_driver hid_sensor_cros_compat_platform_driver = {
	.id_table = hid_sensor_cros_compat_ids,
	.driver = {
		.name	= KBUILD_MODNAME,
		.pm	= &hid_sensor_cros_compat_pm_ops,
	},
	.probe		= hid_sensor_cros_compat_probe,
	.remove		= hid_sensor_cros_compat_remove,
};
module_platform_driver(hid_sensor_cros_compat_platform_driver);

MODULE_DESCRIPTION("HID Sensors support for ChromeOS");
MODULE_LICENSE("GPL");
