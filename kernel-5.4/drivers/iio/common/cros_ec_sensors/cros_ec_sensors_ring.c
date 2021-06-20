/*
 * cros_ec_sensors_ring - Driver for Chrome OS EC Sensor hub FIFO.
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
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_sensorhub.h>
#include <linux/platform_device.h>

#define DRV_NAME "cros-ec-ring"

/*
 * The ring is a FIFO that return sensor information from
 * the single EC FIFO.
 * There are always 5 channels returned:
 * | ID | FLAG | X | Y | Z | Timestamp |
 * ID is the EC sensor id
 * FLAG are extra information provided by the EC.
 */

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

static const struct iio_info ec_sensors_info = {
};

static int cros_sensor_ring_push_sample(
		struct iio_dev *indio_dev,
		struct cros_ec_sensors_ring_sample *sample)
{
	return iio_push_to_buffers(indio_dev, (u8 *)sample);
}

static int cros_ec_ring_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_sensorhub *sensor_hub = dev_get_drvdata(dev->parent);
	struct iio_dev *indio_dev;
	struct iio_buffer *buffer;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, 0);
	if (!indio_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, indio_dev);
	indio_dev->dev.parent = dev;
	indio_dev->name = pdev->name;
	indio_dev->channels = cros_ec_ring_channels;
	indio_dev->num_channels = ARRAY_SIZE(cros_ec_ring_channels);
	indio_dev->info = &ec_sensors_info;
	indio_dev->modes = INDIO_BUFFER_SOFTWARE;

	buffer = devm_iio_kfifo_allocate(dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(indio_dev, buffer);
	ret = cros_ec_sensorhub_register_push_sample(
			sensor_hub, indio_dev, cros_sensor_ring_push_sample);
	if (ret)
		return ret;

	return devm_iio_device_register(dev, indio_dev);
}

static int cros_ec_ring_remove(struct platform_device *pdev)
{
	struct cros_ec_sensorhub *sensor_hub =
		dev_get_drvdata(pdev->dev.parent);

	cros_ec_sensorhub_unregister_push_sample(sensor_hub);

	return 0;
}

static struct platform_driver cros_ec_ring_platform_driver = {
	.driver = {
		.name	= DRV_NAME,
	},
	.probe		= cros_ec_ring_probe,
	.remove		= cros_ec_ring_remove,
};
module_platform_driver(cros_ec_ring_platform_driver);

MODULE_DESCRIPTION("ChromeOS EC sensor hub ring driver");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_LICENSE("GPL v2");
