// SPDX-License-Identifier: GPL-2.0
/*
 * cros_ec_light_prox - Driver for light and prox sensors behing CrosEC.
 *
 * Copyright (C) 2017 Google, Inc
 */

#include <linux/device.h>
#include <linux/iio/buffer.h>
#include <linux/iio/common/cros_ec_sensors_core.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/kernel.h>
#include <linux/mfd/cros_ec.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/*
 * At least We only represent one entry for light or  proximity.
 * For proximity, we currently support only one light source.
 * For light we support single sensor or 4 channels (C + RGB).
 */
#define CROS_EC_LIGHT_PROX_MIN_CHANNELS (1 + 1)

/* State data for ec_sensors iio driver. */
struct cros_ec_light_prox_state {
	/* Shared by all sensors */
	struct cros_ec_sensors_core_state core;
	struct iio_chan_spec *channel;

	u16 rgb_space[CROS_EC_SENSOR_MAX_AXIS];
	struct calib_data rgb_calib[CROS_EC_SENSOR_MAX_AXIS];
};

static void cros_ec_light_channel_common(struct iio_chan_spec *channel)
{
	channel->info_mask_shared_by_all =
		BIT(IIO_CHAN_INFO_SAMP_FREQ) |
		BIT(IIO_CHAN_INFO_FREQUENCY);
	channel->info_mask_separate =
		BIT(IIO_CHAN_INFO_RAW) |
		BIT(IIO_CHAN_INFO_CALIBBIAS) |
		BIT(IIO_CHAN_INFO_CALIBSCALE);
	channel->scan_type.realbits = CROS_EC_SENSOR_BITS;
	channel->scan_type.storagebits = CROS_EC_SENSOR_BITS;
	channel->scan_type.shift = 0;
	channel->scan_index = 0;
	channel->ext_info = cros_ec_sensors_ext_info;
	channel->scan_type.sign = 'u';
}

static int cros_ec_light_extra_send_host_cmd(
		struct cros_ec_sensors_core_state *state,
		int increment,
		u16 opt_length)
{
	uint8_t save_sensor_num = state->param.info.sensor_num;
	int ret;

	state->param.info.sensor_num += increment;
	ret = cros_ec_motion_send_host_cmd(state, opt_length);
	state->param.info.sensor_num = save_sensor_num;
	return ret;
}



static int cros_ec_light_prox_read_data(
		struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val)
{
	struct cros_ec_light_prox_state *st = iio_priv(indio_dev);
	int i, ret;
	int idx = chan->scan_index;

	st->core.param.cmd = MOTIONSENSE_CMD_DATA;

	/*
	 * The data coming from the light sensor is
	 * pre-processed and represents the ambient light
	 * illuminance reading expressed in lux.
	 */
	if (idx == 0) {
		ret = cros_ec_motion_send_host_cmd(
				&st->core, sizeof(st->core.resp->data));
		if (ret)
			return ret;
		*val = st->core.resp->data.data[0];
	} else {
		ret = cros_ec_light_extra_send_host_cmd(
				&st->core, 1, sizeof(st->core.resp->data));
		if (ret)
			return ret;

		for (i = 0; i < CROS_EC_SENSOR_MAX_AXIS; i++)
			st->rgb_space[i] =
				st->core.resp->data.data[i];
		*val = st->rgb_space[idx - 1];
	}

	return IIO_VAL_INT;
}

static int cros_ec_light_prox_read(struct iio_dev *indio_dev,
				   struct iio_chan_spec const *chan,
				   int *val, int *val2, long mask)
{
	struct cros_ec_light_prox_state *st = iio_priv(indio_dev);
	int i, ret = IIO_VAL_INT;
	int idx = chan->scan_index;
	s64 val64;

	mutex_lock(&st->core.cmd_lock);
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	case IIO_CHAN_INFO_PROCESSED:
		ret = cros_ec_light_prox_read_data(indio_dev, chan, val);
		break;
	case IIO_CHAN_INFO_CALIBBIAS:
		st->core.param.cmd = MOTIONSENSE_CMD_SENSOR_OFFSET;
		st->core.param.sensor_offset.flags = 0;

		if (idx == 0)
			ret = cros_ec_motion_send_host_cmd(&st->core, 0);
		else
			ret = cros_ec_light_extra_send_host_cmd(
					&st->core, 1, 0);
		if (ret)
			break;
		if (idx == 0) {
			*val = st->core.calib[0].offset =
				st->core.resp->sensor_offset.offset[0];
		} else {
			for (i = CROS_EC_SENSOR_X; i < CROS_EC_SENSOR_MAX_AXIS;
			     i++)
				st->rgb_calib[i].offset =
					st->core.resp->sensor_offset.offset[i];
			*val = st->rgb_calib[idx - 1].offset;
		}
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_CALIBSCALE:
		if (indio_dev->num_channels > CROS_EC_LIGHT_PROX_MIN_CHANNELS) {
			u16 scale;

			st->core.param.cmd = MOTIONSENSE_CMD_SENSOR_SCALE;
			st->core.param.sensor_scale.flags = 0;
			if (idx == 0)
				ret = cros_ec_motion_send_host_cmd(
						&st->core, 0);
			else
				ret = cros_ec_light_extra_send_host_cmd(
						&st->core, 1, 0);
			if (ret)
				break;
			if (idx == 0) {
				scale = st->core.calib[0].scale =
					st->core.resp->sensor_scale.scale[0];
			} else {
				for (i = CROS_EC_SENSOR_X;
				     i < CROS_EC_SENSOR_MAX_AXIS;
				     i++)
					st->rgb_calib[i].scale =
					  st->core.resp->sensor_scale.scale[i];
				scale = st->rgb_calib[idx - 1].scale;
			}
			/*
			 * scale is a number x.y, where x is coded on 1 bit,
			 * y coded on 15 bits, between 0 and 9999.
			 */
			*val = scale >> 15;
			*val2 = ((scale & 0x7FFF) * 1000000LL) /
				MOTION_SENSE_DEFAULT_SCALE;
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		}
		/* RANGE is used for calibration in 1 channel sensors. */
		/* Fall through */
	case IIO_CHAN_INFO_SCALE:
		/*
		 * RANGE is used for calibration
		 * scale is a number x.y, where x is coded on 16 bits,
		 * y coded on 16 bits, between 0 and 9999.
		 */
		st->core.param.cmd = MOTIONSENSE_CMD_SENSOR_RANGE;
		st->core.param.sensor_range.data = EC_MOTION_SENSE_NO_VALUE;

		ret = cros_ec_motion_send_host_cmd(&st->core, 0);
		if (ret)
			break;

		val64 = st->core.resp->sensor_range.ret;
		*val = val64 >> 16;
		*val2 = (val64 & 0xffff) * 100;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		ret = cros_ec_sensors_core_read(&st->core, chan, val, val2,
						mask);
		break;
	}

	mutex_unlock(&st->core.cmd_lock);

	return ret;
}

static int cros_ec_light_prox_write(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val, int val2, long mask)
{
	struct cros_ec_light_prox_state *st = iio_priv(indio_dev);
	int ret, i;
	int idx = chan->scan_index;

	mutex_lock(&st->core.cmd_lock);

	switch (mask) {
	case IIO_CHAN_INFO_CALIBBIAS:
		/* Send to EC for each axis, even if not complete */
		st->core.param.cmd = MOTIONSENSE_CMD_SENSOR_OFFSET;
		st->core.param.sensor_offset.flags = MOTION_SENSE_SET_OFFSET;
		st->core.param.sensor_offset.temp =
					EC_MOTION_SENSE_INVALID_CALIB_TEMP;
		if (idx == 0) {
			st->core.calib[0].offset = val;
			st->core.param.sensor_offset.offset[0] = val;
			ret = cros_ec_motion_send_host_cmd(&st->core, 0);
		} else {
			st->rgb_calib[idx - 1].offset = val;
			for (i = CROS_EC_SENSOR_X;
			     i < CROS_EC_SENSOR_MAX_AXIS;
			     i++)
				st->core.param.sensor_offset.offset[i] =
					st->rgb_calib[i].offset;
			ret = cros_ec_light_extra_send_host_cmd(
					&st->core, 1, 0);
		}
		break;
	case IIO_CHAN_INFO_CALIBSCALE:
		if (indio_dev->num_channels >
				CROS_EC_LIGHT_PROX_MIN_CHANNELS) {
			u16 scale;

			if (val >= 2) {
				/*
				 * The user space is sending values already
				 * multiplied by MOTION_SENSE_DEFAULT_SCALE.
				 */
				scale = val;
			} else {
				u64 val64 = val2 * MOTION_SENSE_DEFAULT_SCALE;

				do_div(val64, 1000000);
				scale = (val << 15) | val64;
			}

			st->core.param.cmd = MOTIONSENSE_CMD_SENSOR_SCALE;
			st->core.param.sensor_offset.flags =
				MOTION_SENSE_SET_OFFSET;
			st->core.param.sensor_offset.temp =
				EC_MOTION_SENSE_INVALID_CALIB_TEMP;
			if (idx == 0) {
				st->core.calib[0].scale = scale;
				st->core.param.sensor_scale.scale[0] = scale;
				ret = cros_ec_motion_send_host_cmd(
						&st->core, 0);
			} else {
				st->rgb_calib[idx - 1].scale = scale;
				for (i = CROS_EC_SENSOR_X;
				     i < CROS_EC_SENSOR_MAX_AXIS;
				     i++)
					st->core.param.sensor_scale.scale[i] =
						st->rgb_calib[i].scale;
				ret = cros_ec_light_extra_send_host_cmd(
						&st->core, 1, 0);
			}
			break;
		}
		/*
		 * For sensors with only one channel, _RANGE is used
		 * instead of _SCALE.
		 */
		/* Fall through */
	case IIO_CHAN_INFO_SCALE:
		st->core.param.cmd = MOTIONSENSE_CMD_SENSOR_RANGE;
		st->core.param.sensor_range.data = (val << 16) | (val2 / 100);
		ret = cros_ec_motion_send_host_cmd(&st->core, 0);
		break;
	default:
		ret = cros_ec_sensors_core_write(&st->core, chan, val, val2,
						 mask);
		break;
	}

	mutex_unlock(&st->core.cmd_lock);

	return ret;
}

static irqreturn_t cros_ec_light_capture(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int ret, i, idx = 0;
	s16 data = 0;
	const unsigned long scan_mask = *(indio_dev->active_scan_mask);

	mutex_lock(&st->cmd_lock);

	/* Clear capture data. */
	memset(st->samples, 0, indio_dev->scan_bytes);

	/* Read first channel. */
	ret = cros_ec_sensors_read_cmd(indio_dev, 1, &data);
	if (ret < 0)
		goto done;
	if (test_bit(0, indio_dev->active_scan_mask))
		((s16 *)st->samples)[idx++] = data;

	/* Read remaining channels. */
	if (scan_mask & ((1 << indio_dev->num_channels) - 2)) {
		ret = cros_ec_light_extra_send_host_cmd(
				st, 1, sizeof(st->resp->data));
		if (ret < 0)
			goto done;
		for (i = 0; i < CROS_EC_SENSOR_MAX_AXIS; i++)
			if (test_bit(i + 1, indio_dev->active_scan_mask))
				((s16 *)st->samples)[idx++] =
					st->resp->data.data[i];
	}

	iio_push_to_buffers_with_timestamp(indio_dev, st->samples,
					   iio_get_time_ns(indio_dev));

done:
	/*
	 * Tell the core we are done with this trigger and ready for the
	 * next one.
	 */
	iio_trigger_notify_done(indio_dev->trig);

	mutex_unlock(&st->cmd_lock);

	return IRQ_HANDLED;
}

static const struct iio_info cros_ec_light_prox_info = {
	.read_raw = &cros_ec_light_prox_read,
	.write_raw = &cros_ec_light_prox_write,
};


static int cros_ec_light_prox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_dev *ec_dev = dev_get_drvdata(dev->parent);
	struct iio_dev *indio_dev;
	struct cros_ec_light_prox_state *state;
	struct iio_chan_spec *channel;
	int ret, i, num_channels = CROS_EC_LIGHT_PROX_MIN_CHANNELS;

	if (!ec_dev || !ec_dev->ec_dev) {
		dev_warn(dev, "No CROS EC device found.\n");
		return -EINVAL;
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*state));
	if (!indio_dev)
		return -ENOMEM;

	ret = cros_ec_sensors_core_init(pdev, indio_dev, true);
	if (ret)
		return ret;

	indio_dev->info = &cros_ec_light_prox_info;
	state = iio_priv(indio_dev);
	state->core.type = state->core.resp->info.type;
	state->core.loc = state->core.resp->info.location;

	/* Check if we need more sensors for RGB (or XYZ). */
	state->core.param.cmd = MOTIONSENSE_CMD_INFO;
	if (cros_ec_light_extra_send_host_cmd(&state->core, 1, 0) == 0 &&
	    state->core.resp->info.type == MOTIONSENSE_TYPE_LIGHT_RGB)
		num_channels += CROS_EC_SENSOR_MAX_AXIS;

	channel = devm_kcalloc(dev, num_channels, sizeof(*channel), 0);
	if (channel == NULL)
		return -ENOMEM;

	indio_dev->channels = channel;
	indio_dev->num_channels = num_channels;

	cros_ec_light_channel_common(channel);
	/* Sensor specific */
	switch (state->core.type) {
	case MOTIONSENSE_TYPE_LIGHT:
		channel->type = IIO_LIGHT;
		if (num_channels < CROS_EC_LIGHT_PROX_MIN_CHANNELS +
				CROS_EC_SENSOR_MAX_AXIS) {
			/* For backward compatibility. */
			channel->info_mask_separate =
				BIT(IIO_CHAN_INFO_PROCESSED) |
				BIT(IIO_CHAN_INFO_CALIBBIAS) |
				BIT(IIO_CHAN_INFO_CALIBSCALE);
		} else {
			/*
			 * To set a global scale, as CALIB_SCALE for RGB sensor
			 * is limited between 0 and 2.
			 */
			channel->info_mask_shared_by_all |=
				BIT(IIO_CHAN_INFO_SCALE);
		}
		break;
	case MOTIONSENSE_TYPE_PROX:
		channel->type = IIO_PROXIMITY;
		break;
	default:
		dev_warn(dev, "Unknown motion sensor\n");
		return -EINVAL;
	}
	channel++;

	if (num_channels > CROS_EC_LIGHT_PROX_MIN_CHANNELS) {
		for (i = CROS_EC_SENSOR_X; i < CROS_EC_SENSOR_MAX_AXIS;
				i++, channel++) {
			cros_ec_light_channel_common(channel);
			channel->scan_index = i + 1;
			channel->modified = 1;
			channel->channel2 = IIO_MOD_LIGHT_RED + i;
			channel->type = IIO_LIGHT;
		}
	}

	/* Timestamp */
	channel->type = IIO_TIMESTAMP;
	channel->channel = -1;
	channel->scan_index = num_channels - 1;
	channel->scan_type.sign = 's';
	channel->scan_type.realbits = 64;
	channel->scan_type.storagebits = 64;

	state->core.read_ec_sensors_data = cros_ec_sensors_read_cmd;

	ret = devm_iio_triggered_buffer_setup(dev, indio_dev, NULL,
					      cros_ec_light_capture, NULL);
	if (ret)
		return ret;

	return devm_iio_device_register(dev, indio_dev);
}

static const struct platform_device_id cros_ec_light_prox_ids[] = {
	{
		.name = "cros-ec-prox",
	},
	{
		.name = "cros-ec-light",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, cros_ec_light_prox_ids);

static struct platform_driver cros_ec_light_prox_platform_driver = {
	.driver = {
		.name	= "cros-ec-light-prox",
		.pm	= &cros_ec_sensors_pm_ops,
	},
	.probe		= cros_ec_light_prox_probe,
	.id_table	= cros_ec_light_prox_ids,
};
module_platform_driver(cros_ec_light_prox_platform_driver);

MODULE_DESCRIPTION("ChromeOS EC light/proximity sensors driver");
MODULE_LICENSE("GPL v2");
