/*
 * Copyright 2018 Google LLC.
 *
 * Driver for Semtech's SX9310 capacitive proximity/button solution.
 * Datasheet available at
 * <http://www.semtech.com/images/datasheet/sx9310.pdf>.
 * Based on SX9500 driver and Semtech driver using the input framework
 * <https://my.syncplicity.com/share/teouwsim8niiaud/
 *          linux-driver-SX9310_NoSmartHSensing>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/pm.h>
#include <linux/delay.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define SX9310_DRIVER_NAME		"sx9310"
#define SX9310_ACPI_NAME		"STH9310"
#define SX9310_IRQ_NAME			"sx9310_event"

#define SX9310_GPIO_INT			"interrupt"

/* Register definitions. */
#define SX9310_REG_IRQ_SRC		0x00
#define SX9310_REG_STAT0		0x01
#define SX9310_REG_STAT1		0x02
#define SX9310_REG_IRQ_MSK		0x03
#define SX9310_REG_IRQ_FUNC		0x04

#define SX9310_REG_PROX_CTRL0		0x10
#define SX9310_REG_PROX_CTRL1		0x11
#define SX9310_REG_PROX_CTRL2		0x12
#define SX9310_REG_PROX_CTRL3		0x13
#define SX9310_REG_PROX_CTRL4		0x14
#define SX9310_REG_PROX_CTRL5		0x15
#define SX9310_REG_PROX_CTRL6		0x16
#define SX9310_REG_PROX_CTRL7		0x17
#define SX9310_REG_PROX_CTRL8		0x18
#define SX9310_REG_PROX_CTRL9		0x19
#define SX9310_REG_PROX_CTRL10		0x1A
#define SX9310_REG_PROX_CTRL11		0x1B
#define SX9310_REG_PROX_CTRL12		0x1C
#define SX9310_REG_PROX_CTRL13		0x1D
#define SX9310_REG_PROX_CTRL14		0x1E
#define SX9310_REG_PROX_CTRL15		0x1F
#define SX9310_REG_PROX_CTRL16		0x20
#define SX9310_REG_PROX_CTRL17		0x21
#define SX9310_REG_PROX_CTRL18		0x22
#define SX9310_REG_PROX_CTRL19		0x23
#define SX9310_REG_SAR_CTRL0		0x2A
#define SX9310_REG_SAR_CTRL1		0x2B
#define SX9310_REG_SAR_CTRL2		0x2C

#define SX9310_REG_SENSOR_SEL		0x30

#define SX9310_REG_USE_MSB		0x31
#define SX9310_REG_USE_LSB		0x32

#define SX9310_REG_AVG_MSB		0x33
#define SX9310_REG_AVG_LSB		0x34

#define SX9310_REG_DIFF_MSB		0x35
#define SX9310_REG_DIFF_LSB		0x36

#define SX9310_REG_OFFSET_MSB		0x37
#define SX9310_REG_OFFSET_LSB		0x38

#define SX9310_REG_SAR_MSB		0x39
#define SX9310_REG_SAR_LSB		0x3A

#define SX9310_REG_I2CADDR		0x40
#define SX9310_REG_PAUSE		0x41
#define SX9310_REG_WHOAMI		0x42
/* Expected content of the WHOAMI register. */
#define SX9310_WHOAMI_VALUE		0x01

#define SX9310_REG_RESET		0x7f
/* Write this to REG_RESET to do a soft reset. */
#define SX9310_SOFT_RESET		0xde


/* Sensor Readback */

/*
 * These serve for identifying IRQ source in the IRQ_SRC register, and
 * also for masking the IRQs in the IRQ_MSK register.
 */
#define SX9310_RESET_IRQ		BIT(7)
#define SX9310_CLOSE_IRQ		BIT(6)
#define SX9310_FAR_IRQ			BIT(5)
#define SX9310_COMPDONE_IRQ		BIT(4)
#define SX9310_CONVDONE_IRQ		BIT(3)

#define SX9310_SCAN_PERIOD_MASK		GENMASK(7, 4)
#define SX9310_SCAN_PERIOD_SHIFT	4

#define SX9310_COMPSTAT_MASK		GENMASK(3, 0)

/* 4 channels, as defined in STAT0: COMB, CS2, CS1 and CS0. */
#define SX9310_NUM_CHANNELS		4
#define SX9310_CHAN_MASK		GENMASK(2, 0)

struct sx9310_data {
	struct mutex mutex;
	struct i2c_client *client;
	struct iio_trigger *trig;
	struct regmap *regmap;
	/*
	 * Last reading of the proximity status for each channel.
	 * We only send an event to user space when this changes.
	 */
	bool prox_stat[SX9310_NUM_CHANNELS];
	bool event_enabled[SX9310_NUM_CHANNELS];
	bool trigger_enabled;
	u16 *buffer;
	/* Remember enabled channels and sample rate during suspend. */
	unsigned int suspend_ctrl0;
	struct completion completion;
	int data_rdy_users, close_far_users;
	int channel_users[SX9310_NUM_CHANNELS];
	unsigned int num_irqs;
};

static const struct iio_event_spec sx9310_events[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	},
};

#define SX9310_CHANNEL(idx, name, addr)				\
	{							\
		.type = IIO_PROXIMITY,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
		.indexed = 1,					\
		.channel = idx,					\
		.address = addr,				\
		.event_spec = sx9310_events,			\
		.num_event_specs = ARRAY_SIZE(sx9310_events),	\
		.extend_name = name,				\
		.scan_index = idx,				\
		.scan_type = {					\
			.sign = 's',				\
			.realbits = 16,				\
			.storagebits = 16,			\
			.shift = 0,				\
		},						\
	}

static const struct iio_chan_spec sx9310_channels[] = {
	SX9310_CHANNEL(0, "USE_CS0", SX9310_REG_USE_MSB),
	SX9310_CHANNEL(1, "USE_CS1", SX9310_REG_USE_MSB),
	SX9310_CHANNEL(2, "USE_CS2", SX9310_REG_USE_MSB),
	SX9310_CHANNEL(3, "USE_COMB", SX9310_REG_USE_MSB),

	SX9310_CHANNEL(4, "DIFF_CS0", SX9310_REG_DIFF_MSB),
	SX9310_CHANNEL(5, "DIFF_CS1", SX9310_REG_DIFF_MSB),
	SX9310_CHANNEL(6, "DIFF_CS2", SX9310_REG_DIFF_MSB),
	SX9310_CHANNEL(7, "DIFF_COMB", SX9310_REG_DIFF_MSB),

	IIO_CHAN_SOFT_TIMESTAMP(8),
};

/*
 * Each entry contains the integer part (val) and the fractional part, in micro
 * seconds. It conforms to the IIO output IIO_VAL_INT_PLUS_MICRO.
 */
static const struct {
	int val;
	int val2;
} sx9310_samp_freq_table[] = {
	{500, 0}, /* 0000: Min (no idle time) */
	{66, 666666}, /* 0001: 15 ms */
	{33, 333333}, /* 0010: 30 ms (Typ.) */
	{22, 222222}, /* 0011: 45 ms */
	{16, 666666}, /* 0100: 60 ms */
	{11, 111111}, /* 0101: 90 ms */
	{8, 333333}, /* 0110: 120 ms */
	{5, 0}, /* 0111: 200 ms */
	{2, 500000}, /* 1000: 400 ms */
	{1, 666666}, /* 1001: 600 ms */
	{1, 250000}, /* 1010: 800 ms */
	{1, 0}, /* 1011: 1 s */
	{0, 500000}, /* 1100: 2 s */
	{8, 333333}, /* 1101: 3 s */
	{0, 250000}, /* 1110: 4 s */
	{0, 200000}, /* 1111: 5 s */
};
static const unsigned int sx9310_scan_period_table[] = {
	2, 15, 30, 45, 60, 90, 120, 200, 400, 800, 1000, 2000, 3000, 4000, 5000,
};

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
	"500.0 66.666666 33.333333 22.222222 16.666666 "
	"11.111111 8.333333 5.0 2.500000 1.666666 1.250000 "
	"1.0 0.500000 8.333333 0.250000 0.200000");


static const struct regmap_range sx9310_writable_reg_ranges[] = {
	regmap_reg_range(SX9310_REG_IRQ_MSK, SX9310_REG_IRQ_FUNC),
	regmap_reg_range(SX9310_REG_PROX_CTRL0, SX9310_REG_PROX_CTRL19),
	regmap_reg_range(SX9310_REG_SAR_CTRL0, SX9310_REG_SAR_CTRL2),
	regmap_reg_range(SX9310_REG_SENSOR_SEL, SX9310_REG_SENSOR_SEL),
	regmap_reg_range(SX9310_REG_OFFSET_MSB, SX9310_REG_OFFSET_LSB),
	regmap_reg_range(SX9310_REG_PAUSE, SX9310_REG_PAUSE),
	regmap_reg_range(SX9310_REG_RESET, SX9310_REG_RESET),
};

static const struct regmap_access_table sx9310_writeable_regs = {
	.yes_ranges = sx9310_writable_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(sx9310_writable_reg_ranges),
};

/*
 * All allocated registers are readable, so we just list unallocated
 * ones.
 */
static const struct regmap_range sx9310_non_readable_reg_ranges[] = {
	regmap_reg_range(SX9310_REG_IRQ_FUNC + 1, SX9310_REG_PROX_CTRL0 - 1),
	regmap_reg_range(SX9310_REG_SAR_CTRL2 + 1, SX9310_REG_SENSOR_SEL - 1),
	regmap_reg_range(SX9310_REG_SAR_LSB + 1, SX9310_REG_I2CADDR - 1),
	regmap_reg_range(SX9310_REG_WHOAMI + 1, SX9310_REG_RESET - 1),
};

static const struct regmap_access_table sx9310_readable_regs = {
	.no_ranges = sx9310_non_readable_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(sx9310_non_readable_reg_ranges),
};

static const struct regmap_range sx9310_volatile_reg_ranges[] = {
	regmap_reg_range(SX9310_REG_IRQ_SRC, SX9310_REG_STAT1),
	regmap_reg_range(SX9310_REG_USE_MSB, SX9310_REG_DIFF_LSB),
	regmap_reg_range(SX9310_REG_SAR_MSB, SX9310_REG_SAR_LSB),
	regmap_reg_range(SX9310_REG_RESET, SX9310_REG_RESET),
};

static const struct regmap_access_table sx9310_volatile_regs = {
	.yes_ranges = sx9310_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(sx9310_volatile_reg_ranges),
};

static const struct regmap_config sx9310_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SX9310_REG_RESET,
	.cache_type = REGCACHE_RBTREE,

	.wr_table = &sx9310_writeable_regs,
	.rd_table = &sx9310_readable_regs,
	.volatile_table = &sx9310_volatile_regs,
};

static int sx9310_inc_users(struct sx9310_data *data, int *counter,
			    unsigned int reg, unsigned int bitmask)
{
	++(*counter);
	if (*counter != 1)
		/* Bit is already active, nothing to do. */
		return 0;

	return regmap_update_bits(data->regmap, reg, bitmask, bitmask);
}

static int sx9310_dec_users(struct sx9310_data *data, int *counter,
			    unsigned int reg, unsigned int bitmask)
{
	--(*counter);
	if (*counter != 0)
		/* There are more users, do not deactivate. */
		return 0;

	return regmap_update_bits(data->regmap, reg, bitmask, 0);
}

static int sx9310_inc_chan_users(struct sx9310_data *data, int chan)
{
	return sx9310_inc_users(data, &data->channel_users[chan],
				SX9310_REG_PROX_CTRL0, BIT(chan));
}

static int sx9310_dec_chan_users(struct sx9310_data *data, int chan)
{
	return sx9310_dec_users(data, &data->channel_users[chan],
				SX9310_REG_PROX_CTRL0, BIT(chan));
}

static int sx9310_inc_data_rdy_users(struct sx9310_data *data)
{
	return sx9310_inc_users(data, &data->data_rdy_users,
				SX9310_REG_IRQ_MSK, SX9310_CONVDONE_IRQ);
}

static int sx9310_dec_data_rdy_users(struct sx9310_data *data)
{
	return sx9310_dec_users(data, &data->data_rdy_users,
				SX9310_REG_IRQ_MSK, SX9310_CONVDONE_IRQ);
}

static int sx9310_inc_close_far_users(struct sx9310_data *data)
{
	return sx9310_inc_users(data, &data->close_far_users,
				SX9310_REG_IRQ_MSK,
				SX9310_CLOSE_IRQ | SX9310_FAR_IRQ);
}

static int sx9310_dec_close_far_users(struct sx9310_data *data)
{
	return sx9310_dec_users(data, &data->close_far_users,
				SX9310_REG_IRQ_MSK,
				SX9310_CLOSE_IRQ | SX9310_FAR_IRQ);
}

static int sx9310_read_prox_data(struct sx9310_data *data,
				 const struct iio_chan_spec *chan,
				 int *val)
{
	int ret;
	__be16 regval;

	ret = regmap_write(data->regmap, SX9310_REG_SENSOR_SEL, chan->channel);
	if (ret < 0)
		return ret;

	ret = regmap_bulk_read(data->regmap, chan->address, &regval, 2);
	if (ret < 0)
		return ret;

	*val = sign_extend32(be16_to_cpu(regval),
			(chan->address == SX9310_REG_DIFF_MSB ? 11 : 15));

	return 0;
}

/*
 * If we have no interrupt support, we have to wait for a scan period
 * after enabling a channel to get a result.
 */
static int sx9310_wait_for_sample(struct sx9310_data *data)
{
	int ret;
	unsigned int val;

	ret = regmap_read(data->regmap, SX9310_REG_PROX_CTRL0, &val);
	if (ret < 0)
		return ret;

	val = (val & SX9310_SCAN_PERIOD_MASK) >> SX9310_SCAN_PERIOD_SHIFT;

	msleep(sx9310_scan_period_table[val]);

	return 0;
}

static int sx9310_read_proximity(struct sx9310_data *data,
				 const struct iio_chan_spec *chan,
				 int *val)
{
	int ret;

	mutex_lock(&data->mutex);

	ret = sx9310_inc_chan_users(data, chan->channel & SX9310_CHAN_MASK);
	if (ret < 0)
		goto out;

	ret = sx9310_inc_data_rdy_users(data);
	if (ret < 0)
		goto out_dec_chan;

	mutex_unlock(&data->mutex);

	if (data->client->irq > 0)
		ret = wait_for_completion_interruptible(&data->completion);
	else
		ret = sx9310_wait_for_sample(data);

	mutex_lock(&data->mutex);

	if (ret < 0)
		goto out_dec_data_rdy;

	ret = sx9310_read_prox_data(data, chan, val);
	if (ret < 0)
		goto out_dec_data_rdy;

	ret = sx9310_dec_data_rdy_users(data);
	if (ret < 0)
		goto out_dec_chan;

	ret = sx9310_dec_chan_users(data, chan->channel & SX9310_CHAN_MASK);
	if (ret < 0)
		goto out;

	ret = IIO_VAL_INT;

	goto out;

out_dec_data_rdy:
	sx9310_dec_data_rdy_users(data);
out_dec_chan:
	sx9310_dec_chan_users(data, chan->channel & SX9310_CHAN_MASK);
out:
	mutex_unlock(&data->mutex);
	reinit_completion(&data->completion);

	return ret;
}

static int sx9310_read_samp_freq(struct sx9310_data *data,
				 int *val, int *val2)
{
	int ret;
	unsigned int regval;

	mutex_lock(&data->mutex);
	ret = regmap_read(data->regmap, SX9310_REG_PROX_CTRL0, &regval);
	mutex_unlock(&data->mutex);

	if (ret < 0)
		return ret;

	regval = (regval & SX9310_SCAN_PERIOD_MASK) >> SX9310_SCAN_PERIOD_SHIFT;
	*val = sx9310_samp_freq_table[regval].val;
	*val2 = sx9310_samp_freq_table[regval].val2;

	return IIO_VAL_INT_PLUS_MICRO;
}

static int sx9310_read_raw(struct iio_dev *indio_dev,
			   const struct iio_chan_spec *chan,
			   int *val, int *val2, long mask)
{
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret;

	switch (chan->type) {
	case IIO_PROXIMITY:
		switch (mask) {
		case IIO_CHAN_INFO_RAW:
			mutex_lock(&indio_dev->mlock);

			if (iio_buffer_enabled(indio_dev)) {
				mutex_unlock(&indio_dev->mlock);
				return -EBUSY;
			}
			ret = sx9310_read_proximity(data, chan, val);
			mutex_unlock(&indio_dev->mlock);
			return ret;
		case IIO_CHAN_INFO_SAMP_FREQ:
			return sx9310_read_samp_freq(data, val, val2);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static int sx9310_set_samp_freq(struct sx9310_data *data,
				int val, int val2)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(sx9310_samp_freq_table); i++)
		if (val == sx9310_samp_freq_table[i].val &&
		    val2 == sx9310_samp_freq_table[i].val2)
			break;

	if (i == ARRAY_SIZE(sx9310_samp_freq_table))
		return -EINVAL;

	mutex_lock(&data->mutex);

	ret = regmap_update_bits(data->regmap, SX9310_REG_PROX_CTRL0,
				 SX9310_SCAN_PERIOD_MASK,
				 i << SX9310_SCAN_PERIOD_SHIFT);

	mutex_unlock(&data->mutex);

	return ret;
}

static int sx9310_write_raw(struct iio_dev *indio_dev,
			    const struct iio_chan_spec *chan,
			    int val, int val2, long mask)
{
	struct sx9310_data *data = iio_priv(indio_dev);

	switch (chan->type) {
	case IIO_PROXIMITY:
		switch (mask) {
		case IIO_CHAN_INFO_SAMP_FREQ:
			return sx9310_set_samp_freq(data, val, val2);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static irqreturn_t sx9310_irq_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct sx9310_data *data = iio_priv(indio_dev);

	if (data->trigger_enabled)
		iio_trigger_poll(data->trig);

	/*
	 * Even if no event is enabled, we need to wake the thread to
	 * clear the interrupt state by reading SX9310_REG_IRQ_SRC.  It
	 * is not possible to do that here because regmap_read takes a
	 * mutex.
	 */
	return IRQ_WAKE_THREAD;
}

static void sx9310_push_events(struct iio_dev *indio_dev)
{
	int ret;
	unsigned int val, chan;
	struct sx9310_data *data = iio_priv(indio_dev);

	ret = regmap_read(data->regmap, SX9310_REG_STAT0, &val);
	if (ret < 0) {
		dev_err(&data->client->dev, "i2c transfer error in irq\n");
		return;
	}

	for (chan = 0; chan < SX9310_NUM_CHANNELS; chan++) {
		int dir;
		u64 ev;
		bool new_prox = val & BIT(chan);

		if (!data->event_enabled[chan])
			continue;
		if (new_prox == data->prox_stat[chan])
			/* No change on this channel. */
			continue;

		dir = new_prox ? IIO_EV_DIR_FALLING : IIO_EV_DIR_RISING;
		ev = IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY, chan,
					  IIO_EV_TYPE_THRESH, dir);
		iio_push_event(indio_dev, ev, iio_get_time_ns());
		data->prox_stat[chan] = new_prox;
	}
}

static irqreturn_t sx9310_irq_thread_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret;
	unsigned int val;

	mutex_lock(&data->mutex);

	ret = regmap_read(data->regmap, SX9310_REG_IRQ_SRC, &val);
	if (ret < 0) {
		dev_err(&data->client->dev, "i2c transfer error in irq\n");
		goto out;
	}

	if (val & (SX9310_CLOSE_IRQ | SX9310_FAR_IRQ))
		sx9310_push_events(indio_dev);

	if (val & SX9310_CONVDONE_IRQ)
		complete(&data->completion);

out:
	mutex_unlock(&data->mutex);

	return IRQ_HANDLED;
}

static int sx9310_read_event_config(struct iio_dev *indio_dev,
				    const struct iio_chan_spec *chan,
				    enum iio_event_type type,
				    enum iio_event_direction dir)
{
	struct sx9310_data *data = iio_priv(indio_dev);

	if (chan->type != IIO_PROXIMITY || type != IIO_EV_TYPE_THRESH ||
	    dir != IIO_EV_DIR_EITHER)
		return -EINVAL;

	return data->event_enabled[chan->channel & SX9310_CHAN_MASK];
}

static int sx9310_write_event_config(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir,
				     int state)
{
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret, sx_channel = chan->channel & SX9310_CHAN_MASK;

	if (chan->type != IIO_PROXIMITY || type != IIO_EV_TYPE_THRESH ||
	    dir != IIO_EV_DIR_EITHER)
		return -EINVAL;

	mutex_lock(&data->mutex);

	if (state == 1) {
		ret = sx9310_inc_chan_users(data, sx_channel);
		if (ret < 0)
			goto out_unlock;
		ret = sx9310_inc_close_far_users(data);
		if (ret < 0)
			goto out_undo_chan;
	} else {
		ret = sx9310_dec_chan_users(data, sx_channel);
		if (ret < 0)
			goto out_unlock;
		ret = sx9310_dec_close_far_users(data);
		if (ret < 0)
			goto out_undo_chan;
	}

	data->event_enabled[sx_channel] = state;
	goto out_unlock;

out_undo_chan:
	if (state == 1)
		sx9310_dec_chan_users(data, sx_channel);
	else
		sx9310_inc_chan_users(data, sx_channel);
out_unlock:
	mutex_unlock(&data->mutex);
	return ret;
}

static int sx9310_update_scan_mode(struct iio_dev *indio_dev,
				   const unsigned long *scan_mask)
{
	struct sx9310_data *data = iio_priv(indio_dev);

	mutex_lock(&data->mutex);
	kfree(data->buffer);
	data->buffer = kzalloc(indio_dev->scan_bytes, GFP_KERNEL);
	mutex_unlock(&data->mutex);

	if (data->buffer == NULL)
		return -ENOMEM;

	return 0;
}

static ssize_t sx9310_uid_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct acpi_device *adev = ACPI_COMPANION(dev);
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);

	if (adev == NULL)
		return sprintf(buf, "%d\n", indio_dev->id);
	else
		return sprintf(buf, "%s\n", acpi_device_uid(adev));
}

static IIO_DEVICE_ATTR(uid, 0444, sx9310_uid_show, NULL, 0);

static struct attribute *sx9310_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_uid.dev_attr.attr,
	NULL,
};

static const struct attribute_group sx9310_attribute_group = {
	.attrs = sx9310_attributes,
};

static const struct iio_info sx9310_info = {
	.attrs = &sx9310_attribute_group,
	.read_raw = &sx9310_read_raw,
	.write_raw = &sx9310_write_raw,
	.read_event_config = &sx9310_read_event_config,
	.write_event_config = &sx9310_write_event_config,
	.update_scan_mode = &sx9310_update_scan_mode,
};

static int sx9310_set_trigger_state(struct iio_trigger *trig,
				    bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);

	if (state)
		ret = sx9310_inc_data_rdy_users(data);
	else
		ret = sx9310_dec_data_rdy_users(data);
	if (ret < 0)
		goto out;

	data->trigger_enabled = state;

out:
	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_trigger_ops sx9310_trigger_ops = {
	.set_trigger_state = sx9310_set_trigger_state,
};

static irqreturn_t sx9310_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *pf = private;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct sx9310_data *data = iio_priv(indio_dev);
	int val, bit, ret, i = 0;

	mutex_lock(&data->mutex);

	for_each_set_bit(bit, indio_dev->active_scan_mask,
			 indio_dev->masklength) {
		ret = sx9310_read_prox_data(data, &indio_dev->channels[bit],
					    &val);
		if (ret < 0)
			goto out;

		data->buffer[i++] = val;
	}

	iio_push_to_buffers_with_timestamp(indio_dev, data->buffer,
					   iio_get_time_ns());

out:
	mutex_unlock(&data->mutex);

	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int sx9310_buffer_preenable(struct iio_dev *indio_dev)
{
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret = 0, i;

	mutex_lock(&data->mutex);

	for (i = 0; i < SX9310_NUM_CHANNELS; i++)
		if (test_bit(i, indio_dev->active_scan_mask)) {
			ret = sx9310_inc_chan_users(data, i);
			if (ret)
				break;
		}

	if (ret)
		for (i = i - 1; i >= 0; i--)
			if (test_bit(i, indio_dev->active_scan_mask))
				sx9310_dec_chan_users(data, i);

	mutex_unlock(&data->mutex);

	return ret;
}

static int sx9310_buffer_predisable(struct iio_dev *indio_dev)
{
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret = 0, i;

	iio_triggered_buffer_predisable(indio_dev);

	mutex_lock(&data->mutex);

	for (i = 0; i < SX9310_NUM_CHANNELS; i++)
		if (test_bit(i, indio_dev->active_scan_mask)) {
			ret = sx9310_dec_chan_users(data, i);
			if (ret)
				break;
		}

	if (ret)
		for (i = i - 1; i >= 0; i--)
			if (test_bit(i, indio_dev->active_scan_mask))
				sx9310_inc_chan_users(data, i);

	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_buffer_setup_ops sx9310_buffer_setup_ops = {
	.preenable = sx9310_buffer_preenable,
	.postenable = iio_triggered_buffer_postenable,
	.predisable = sx9310_buffer_predisable,
};

struct sx9310_reg_config {
	const char *register_name;
	u8 reg;
	u8 def;
};

#define SX9310_REG_CONFIG(_name, _reg, _def) \
{ \
	.register_name = SX9310_ACPI_NAME ",reg_" _name, \
	.reg = SX9310_REG_##_reg, \
	.def = _def \
}

static const struct sx9310_reg_config sx9310_default_regs[] = {
	{
		.register_name = NULL,
		.reg = SX9310_REG_IRQ_MSK,
		.def = 0x60,
	},
	{
		.register_name = NULL,
		.reg = SX9310_REG_IRQ_FUNC,
		.def = 0x00,
	},
	/*
	 * The lower 4 bits should not be set as it enable sensors measurements.
	 * Turning the detection on before the configuration values are set to
	 * good values can cause the device to return erroneous readings.
	 */
	SX9310_REG_CONFIG("prox_ctrl0",  PROX_CTRL0,  0x10),
	SX9310_REG_CONFIG("prox_ctrl1",  PROX_CTRL1,  0x00),
	SX9310_REG_CONFIG("prox_ctrl2",  PROX_CTRL2,  0x84),
	SX9310_REG_CONFIG("prox_ctrl3",  PROX_CTRL3,  0x0F),
	SX9310_REG_CONFIG("prox_ctrl4",  PROX_CTRL4,  0x07),
	SX9310_REG_CONFIG("prox_ctrl5",  PROX_CTRL5,  0xC2),
	SX9310_REG_CONFIG("prox_ctrl6",  PROX_CTRL6,  0x20),
	SX9310_REG_CONFIG("prox_ctrl7",  PROX_CTRL7,  0x0D),
	SX9310_REG_CONFIG("prox_ctrl8",  PROX_CTRL8,  0x8D),
	SX9310_REG_CONFIG("prox_ctrl9",  PROX_CTRL9,  0x43),
	SX9310_REG_CONFIG("prox_ctrl10", PROX_CTRL10, 0x11),
	SX9310_REG_CONFIG("prox_ctrl11", PROX_CTRL11, 0x00),
	SX9310_REG_CONFIG("prox_ctrl12", PROX_CTRL12, 0x00),
	SX9310_REG_CONFIG("prox_ctrl13", PROX_CTRL13, 0x00),
	SX9310_REG_CONFIG("prox_ctrl14", PROX_CTRL14, 0x00),
	SX9310_REG_CONFIG("prox_ctrl15", PROX_CTRL15, 0x00),
	SX9310_REG_CONFIG("prox_ctrl16", PROX_CTRL16, 0x00),
	SX9310_REG_CONFIG("prox_ctrl17", PROX_CTRL17, 0x00),
	SX9310_REG_CONFIG("prox_ctrl18", PROX_CTRL18, 0x00),
	SX9310_REG_CONFIG("prox_ctrl19", PROX_CTRL19, 0x00),
	SX9310_REG_CONFIG("sar_ctrl0",   SAR_CTRL0,   0x50),
	SX9310_REG_CONFIG("sar_ctrl1",   SAR_CTRL1,   0x8A),
	SX9310_REG_CONFIG("sar_ctrl2",   SAR_CTRL2,   0x3C),
};

static int sx9310_read_register_property(struct acpi_device *adev,
					const struct sx9310_reg_config *cfg,
					u8 *value)
{
	/* FIXME: only ACPI supported. */
	const union acpi_object *acpi_value = NULL;
	int ret;

	if ((adev == NULL) || (cfg->register_name == NULL)) {
		*value = cfg->def;
		return 0;
	}

	ret = acpi_dev_get_property(adev, cfg->register_name,
				    ACPI_TYPE_INTEGER, &acpi_value);
	switch (ret) {
	case -EPROTO:
		dev_err(&adev->dev, "ACPI property %s typed incorrectly\n",
			cfg->register_name);
		break;
	case -EINVAL:
		dev_dbg(&adev->dev, "property %s missing from ACPI\n",
			cfg->register_name);
		break;
	}

	*value = acpi_value ? (u8)acpi_value->integer.value : cfg->def;
	return 0;
}

static int sx9310_load_config(struct device *dev, struct regmap *regmap)
{
	u8 val;
	int i, ret;
	const struct sx9310_reg_config *cfg;
	struct acpi_device *adev = ACPI_COMPANION(dev);

	if (adev == NULL)
		dev_warn(dev, "ACPI configuration missing\n");

	for (i = 0; i < ARRAY_SIZE(sx9310_default_regs); ++i) {
		cfg = &sx9310_default_regs[i];
		ret = sx9310_read_register_property(adev, cfg, &val);
		if (ret < 0)
			return ret;
		ret = regmap_write(regmap, cfg->reg, val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Activate all channels and perform an initial compensation. */
static int sx9310_init_compensation(struct iio_dev *indio_dev)
{
	struct sx9310_data *data = iio_priv(indio_dev);
	int i, ret;
	unsigned int val;
	unsigned int ctrl0;

	ret = regmap_read(data->regmap, SX9310_REG_PROX_CTRL0, &ctrl0);
	if (ret < 0)
		return ret;

	/* run the compensation phase on all channels */
	ret = regmap_write(data->regmap, SX9310_REG_PROX_CTRL0, ctrl0 | 0xF);
	if (ret < 0)
		return ret;

	for (i = 100; i >= 0; i--) {
		usleep_range(10000, 20000);
		ret = regmap_read(data->regmap, SX9310_REG_STAT1, &val);
		if (ret < 0)
			goto out;
		if (!(val & SX9310_COMPSTAT_MASK))
			break;
	}

	if (i < 0) {
		dev_err(&data->client->dev,
			"initial compensation timed out: 0x%02x", val);
		ret = -ETIMEDOUT;
	}

out:
	regmap_write(data->regmap, SX9310_REG_PROX_CTRL0, ctrl0);
	return ret;
}

static int sx9310_init_device(struct iio_dev *indio_dev)
{
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret;
	unsigned int val;

	ret = regmap_write(data->regmap, SX9310_REG_IRQ_MSK, 0);
	if (ret < 0)
		return ret;

	ret = regmap_write(data->regmap, SX9310_REG_RESET,
			   SX9310_SOFT_RESET);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);  /* power-up time is ~1ms. */

	ret = regmap_write(data->regmap, SX9310_REG_RESET, 0);
	if (ret < 0)
		return ret;

	ret = regmap_read(data->regmap, SX9310_REG_IRQ_SRC, &val);
	if (ret < 0)
		return ret;

	ret = sx9310_load_config(&indio_dev->dev, data->regmap);
	if (ret < 0)
		return ret;

	return sx9310_init_compensation(indio_dev);
}

static int sx9310_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct iio_dev *indio_dev;
	struct sx9310_data *data;
	unsigned int whoami;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (indio_dev == NULL)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	mutex_init(&data->mutex);
	init_completion(&data->completion);
	data->trigger_enabled = false;

	data->regmap = devm_regmap_init_i2c(client, &sx9310_regmap_config);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	ret = regmap_read(data->regmap, SX9310_REG_WHOAMI, &whoami);
	if (ret < 0) {
		dev_err(&client->dev,
				"error in reading WHOAMI register: %d", ret);
		return -ENODEV;
	}
	if (whoami != SX9310_WHOAMI_VALUE) {
		dev_err(&client->dev, "unexpected WHOAMI response: %u", whoami);
		return -ENODEV;
	}

	ACPI_COMPANION_SET(&indio_dev->dev, ACPI_COMPANION(&client->dev));
	indio_dev->dev.parent = &client->dev;
	indio_dev->name = SX9310_DRIVER_NAME;
	indio_dev->channels = sx9310_channels;
	indio_dev->num_channels = ARRAY_SIZE(sx9310_channels);
	indio_dev->info = &sx9310_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	i2c_set_clientdata(client, indio_dev);

	ret = sx9310_init_device(indio_dev);
	if (ret < 0)
		return ret;

	if (client->irq <= 0)
		dev_warn(&client->dev, "no valid irq found\n");
	else {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				sx9310_irq_handler, sx9310_irq_thread_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				SX9310_IRQ_NAME, indio_dev);
		if (ret < 0)
			return ret;

		data->trig = devm_iio_trigger_alloc(&client->dev,
				"%s-dev%d", indio_dev->name, indio_dev->id);
		if (!data->trig)
			return -ENOMEM;

		data->trig->dev.parent = &client->dev;
		data->trig->ops = &sx9310_trigger_ops;
		iio_trigger_set_drvdata(data->trig, indio_dev);

		ret = iio_trigger_register(data->trig);
		if (ret)
			return ret;
	}

	ret = devm_iio_triggered_buffer_setup(&client->dev, indio_dev,
					      NULL, sx9310_trigger_handler,
					      &sx9310_buffer_setup_ops);
	if (ret < 0)
		goto out_trigger_unregister;

	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret < 0)
		goto out_trigger_unregister;

	return 0;

out_trigger_unregister:
	if (client->irq > 0)
		iio_trigger_unregister(data->trig);

	return ret;
}

static int sx9310_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct sx9310_data *data = iio_priv(indio_dev);

	if (client->irq > 0)
		iio_trigger_unregister(data->trig);
	kfree(data->buffer);
	return 0;
}

static int __maybe_unused sx9310_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret;

	disable_irq_nosync(data->client->irq);

	mutex_lock(&data->mutex);
	ret = regmap_write(data->regmap, SX9310_REG_PAUSE, 0);
	mutex_unlock(&data->mutex);

	return ret;
}

static int __maybe_unused sx9310_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct sx9310_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_write(data->regmap, SX9310_REG_PAUSE, 1);
	mutex_unlock(&data->mutex);

	enable_irq(data->client->irq);

	return ret;
}

static const struct dev_pm_ops sx9310_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sx9310_suspend, sx9310_resume)
};

static const struct acpi_device_id sx9310_acpi_match[] = {
	{SX9310_ACPI_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, sx9310_acpi_match);

static const struct of_device_id sx9310_of_match[] = {
	{ .compatible = "semtech," SX9310_DRIVER_NAME, },
	{ }
};
MODULE_DEVICE_TABLE(of, sx9310_of_match);

static const struct i2c_device_id sx9310_id[] = {
	{SX9310_DRIVER_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, sx9310_id);

static struct i2c_driver sx9310_driver = {
	.driver = {
		.name	= SX9310_DRIVER_NAME,
		.acpi_match_table = ACPI_PTR(sx9310_acpi_match),
		.of_match_table = of_match_ptr(sx9310_of_match),
		.pm = &sx9310_pm_ops,
	},
	.probe		= sx9310_probe,
	.remove		= sx9310_remove,
	.id_table	= sx9310_id,
};
module_i2c_driver(sx9310_driver);

MODULE_AUTHOR("Gwendal Grignou <gwendal@chromium.org>");
MODULE_DESCRIPTION("Driver for Semtech SX9310 proximity sensor");
MODULE_LICENSE("GPL v2");
