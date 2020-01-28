/*
 * cros_ec_sensors_activity - Driver for activities/gesture recognition.
 *
 * Copyright (C) 2015 Google, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This driver uses the cros-ec interface to communicate with the Chrome OS
 * EC about accelerometer data. Accelerometer access is presented through
 * iio sysfs.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/common/cros_ec_sensors_core.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/mfd/cros_ec.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>

#define DRV_NAME "cros-ec-activity"

/* st data for ec_sensors iio driver. */
struct cros_ec_sensors_state {
	/* Shared by all sensors */
	struct cros_ec_sensors_core_state core;

	struct iio_chan_spec *channels;
	unsigned nb_activities;
};

static const struct iio_event_spec cros_ec_activity_single_shot[] = {
	{
		.type = IIO_EV_TYPE_CHANGE,
		/* significant motion trigger when we get out of still. */
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	 },
};

static int ec_sensors_read(struct iio_dev *indio_dev,
			  struct iio_chan_spec const *chan,
			  int *val, int *val2, long mask)
{
	dev_warn(&indio_dev->dev, "%s: Not Expected: %d\n", __func__,
		 chan->channel2);
	return -ENOSYS;
}

static int ec_sensors_write(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val, int val2, long mask)
{
	dev_warn(&indio_dev->dev, "%s: Not Expected: %d\n", __func__,
		 chan->channel2);
	return -ENOSYS;
}

static int cros_ec_read_event_config(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir)
{
	struct cros_ec_sensors_state *st = iio_priv(indio_dev);
	int ret;

	if (chan->type != IIO_ACTIVITY)
		return -EINVAL;

	mutex_lock(&st->core.cmd_lock);
	st->core.param.cmd = MOTIONSENSE_CMD_LIST_ACTIVITIES;
	ret = cros_ec_motion_send_host_cmd(&st->core, 0);
	if (!ret) {
		switch (chan->channel2) {
		case IIO_MOD_STILL:
			ret = !!(st->core.resp->list_activities.enabled &
				 (1 << MOTIONSENSE_ACTIVITY_SIG_MOTION));
			break;
		case IIO_MOD_DOUBLE_TAP:
			ret = !!(st->core.resp->list_activities.enabled &
				 (1 << MOTIONSENSE_ACTIVITY_DOUBLE_TAP));
			break;
		default:
			dev_warn(&indio_dev->dev, "Unknown activity: %d\n",
				 chan->channel2);
			ret = -EINVAL;
		}
	}
	mutex_unlock(&st->core.cmd_lock);
	return ret;
}

static int cros_ec_write_event_config(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan,
				      enum iio_event_type type,
				      enum iio_event_direction dir, int state)
{
	struct cros_ec_sensors_state *st = iio_priv(indio_dev);
	int ret;

	if (chan->type != IIO_ACTIVITY)
		return -EINVAL;

	mutex_lock(&st->core.cmd_lock);
	st->core.param.cmd = MOTIONSENSE_CMD_SET_ACTIVITY;
	switch (chan->channel2) {
	case IIO_MOD_STILL:
		st->core.param.set_activity.activity =
			MOTIONSENSE_ACTIVITY_SIG_MOTION;
		break;
	case IIO_MOD_DOUBLE_TAP:
		st->core.param.set_activity.activity =
			MOTIONSENSE_ACTIVITY_DOUBLE_TAP;
		break;
	default:
		dev_warn(&indio_dev->dev, "Unknown activity: %d\n",
			 chan->channel2);
	}
	st->core.param.set_activity.enable = state;

	ret = cros_ec_motion_send_host_cmd(&st->core, 0);

	mutex_unlock(&st->core.cmd_lock);
	return ret;
}

/* Not implemented */
static int cros_ec_read_event_value(struct iio_dev *indio_dev,
				    const struct iio_chan_spec *chan,
				    enum iio_event_type type,
				    enum iio_event_direction dir,
				    enum iio_event_info info,
				    int *val, int *val2)
{
	dev_warn(&indio_dev->dev, "%s: Not Expected: %d\n", __func__,
		 chan->channel2);
	return -ENOSYS;
}

static int cros_ec_write_event_value(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir,
				     enum iio_event_info info,
				     int val, int val2)
{
	dev_warn(&indio_dev->dev, "%s: Not Expected: %d\n", __func__,
		 chan->channel2);
	return -ENOSYS;
}

static const struct iio_info ec_sensors_info = {
	.read_raw = &ec_sensors_read,
	.write_raw = &ec_sensors_write,
	.read_event_config = cros_ec_read_event_config,
	.write_event_config = cros_ec_write_event_config,
	.read_event_value = cros_ec_read_event_value,
	.write_event_value = cros_ec_write_event_value,
};

static int cros_ec_sensors_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_device *ec_device = dev_get_drvdata(dev->parent);
	struct iio_dev *indio_dev;
	struct cros_ec_sensors_state *st;
	struct iio_chan_spec *channel;
	unsigned long activities;
	int i, index, ret, nb_activities;

	if (!ec_device) {
		dev_warn(&pdev->dev, "No CROS EC device found.\n");
		return -EINVAL;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*st));
	if (!indio_dev)
		return -ENOMEM;

	ret = cros_ec_sensors_core_init(pdev, indio_dev, true);
	if (ret)
		return ret;

	indio_dev->info = &ec_sensors_info;
	st = iio_priv(indio_dev);
	st->core.type = st->core.resp->info.type;
	st->core.loc = st->core.resp->info.location;

	/*
	 * List all available activities
	 */
	st->core.param.cmd = MOTIONSENSE_CMD_LIST_ACTIVITIES;
	ret = cros_ec_motion_send_host_cmd(&st->core, 0);
	if (ret)
		return ret;
	activities = st->core.resp->list_activities.enabled |
		     st->core.resp->list_activities.disabled;
	nb_activities = hweight_long(activities) + 1;

	if (!activities)
		return -ENODEV;

	/* Allocate a channel per activity and one for timestamp */
	st->channels = devm_kcalloc(&pdev->dev, nb_activities,
				    sizeof(*st->channels), GFP_KERNEL);
	if (!st->channels)
		return -ENOMEM;

	channel = &st->channels[0];
	index = 0;
	for_each_set_bit(i, &activities, BITS_PER_LONG) {
		channel->scan_index = index;

		/* List all available activities */
		channel->type = IIO_ACTIVITY;
		channel->modified = 1;
		channel->event_spec = cros_ec_activity_single_shot;
		channel->num_event_specs =
				ARRAY_SIZE(cros_ec_activity_single_shot);
		switch (i) {
		case MOTIONSENSE_ACTIVITY_SIG_MOTION:
			channel->channel2 = IIO_MOD_STILL;
			break;
		case MOTIONSENSE_ACTIVITY_DOUBLE_TAP:
			channel->channel2 = IIO_MOD_DOUBLE_TAP;
			break;
		default:
			dev_warn(&pdev->dev, "Unknown activity: %d\n", i);
			continue;
		}
		channel->ext_info = cros_ec_sensors_limited_info;
		channel++;
		index++;
	}

	/* Timestamp */
	channel->scan_index = index;
	channel->type = IIO_TIMESTAMP;
	channel->channel = -1;
	channel->scan_type.sign = 's';
	channel->scan_type.realbits = 64;
	channel->scan_type.storagebits = 64;

	indio_dev->channels = st->channels;
	indio_dev->num_channels = index + 1;

	st->core.read_ec_sensors_data = cros_ec_sensors_read_cmd;

	/* Driver is incomplete: by itself, no way to get event directly */
	ret = iio_device_register(indio_dev);
	return ret;
}

static int cros_ec_sensors_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_device_unregister(indio_dev);
	return 0;
}

static struct platform_driver cros_ec_sensors_platform_driver = {
	.driver = {
		.name	= DRV_NAME,
	},
	.probe		= cros_ec_sensors_probe,
	.remove		= cros_ec_sensors_remove,
};
module_platform_driver(cros_ec_sensors_platform_driver);

MODULE_DESCRIPTION("ChromeOS EC activity sensors driver");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_LICENSE("GPL v2");
