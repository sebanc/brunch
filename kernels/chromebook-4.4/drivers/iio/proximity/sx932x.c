/*
 * Copyright 2018 Google LLC.
 *
 * Driver for Semtech's SX9320 capacitive proximity/button solution.
 * Datasheet available at
 * <http://www.semtech.com/images/datasheet/sx9320.pdf>.
 * Based on SX9500 driver and Semtech driver using the input framework
 * <https://my.syncplicity.com/share/teouwsim8niiaud/
 *          linux-driver-SX9320_NoSmartHSensing>.
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

#define SX932X_DRIVER_NAME		"sx932x"
#define SX9320_ACPI_NAME		"STH9320"
#define SX9321_ACPI_NAME		"STH9321"
#define SX932X_DEV_NB			2

#define SX932X_IRQ_NAME			"sx932x_event"

#define SX932X_GPIO_INT			"interrupt"

/* Register definitions. */
#define SX932X_REG_IRQ_SRC		0x00
#define SX932X_REG_STAT0		0x01
#define SX932X_REG_STAT1		0x02
#define SX932X_REG_STAT2		0x03
#define SX932X_REG_STAT3		0x04
#define SX932X_REG_IRQ_MSK		0x05
#define SX932X_REG_IRQ_CFG0		0x06
#define SX932X_REG_IRQ_CFG2		0x07

#define SX932X_REG_GNRL_CTRL0		0x10
#define SX932X_REG_GNRL_CTRL1		0x11

#define SX932X_REG_I2C_ADDR		0x14
#define SX932X_REG_CLK_SPRD		0x15

#define SX932X_REG_AFE_CTRL0		0x20
#define SX932X_REG_AFE_CTRL1		0x21
#define SX932X_REG_AFE_CTRL2		0x22
#define SX932X_REG_AFE_CTRL3		0x23
#define SX932X_REG_AFE_CTRL4		0x24
#define SX932X_REG_AFE_CTRL5		0x25
#define SX932X_REG_AFE_CTRL6		0x26
#define SX932X_REG_AFE_CTRL7		0x27
#define SX932X_REG_AFE_PH0		0x28
#define SX932X_REG_AFE_PH1		0x29
#define SX932X_REG_AFE_PH2		0x2a
#define SX932X_REG_AFE_PH3		0x2b
#define SX932X_REG_AFE_CTRL8		0x2c
#define SX932X_REG_AFE_CTRL9		0x2d

#define SX932X_REG_PROX_CTRL0		0x30
#define SX932X_REG_PROX_CTRL1		0x31
#define SX932X_REG_PROX_CTRL2		0x32
#define SX932X_REG_PROX_CTRL3		0x33
#define SX932X_REG_PROX_CTRL4		0x34
#define SX932X_REG_PROX_CTRL5		0x35
#define SX932X_REG_PROX_CTRL6		0x36
#define SX932X_REG_PROX_CTRL7		0x37

#define SX932X_REG_ADV_CTRL0		0x40
#define SX932X_REG_ADV_CTRL1		0x41
#define SX932X_REG_ADV_CTRL2		0x42
#define SX932X_REG_ADV_CTRL3		0x43
#define SX932X_REG_ADV_CTRL4		0x44
#define SX932X_REG_ADV_CTRL5		0x45
#define SX932X_REG_ADV_CTRL6		0x46
#define SX932X_REG_ADV_CTRL7		0x47
#define SX932X_REG_ADV_CTRL8		0x48
#define SX932X_REG_ADV_CTRL9		0x49
#define SX932X_REG_ADV_CTRL10		0x4a
#define SX932X_REG_ADV_CTRL11		0x4b
#define SX932X_REG_ADV_CTRL12		0x4c
#define SX932X_REG_ADV_CTRL13		0x4d
#define SX932X_REG_ADV_CTRL14		0x4e
#define SX932X_REG_ADV_CTRL15		0x4f
#define SX932X_REG_ADV_CTRL16		0x50
#define SX932X_REG_ADV_CTRL17		0x51
#define SX932X_REG_ADV_CTRL18		0x52
#define SX932X_REG_ADV_CTRL19		0x53
#define SX932X_REG_ADV_CTRL20		0x54

#define SX932X_REG_PHASE_SEL		0x60

#define SX932X_REG_USE_MSB		0x61
#define SX932X_REG_USE_LSB		0x62

#define SX932X_REG_AVG_MSB		0x63
#define SX932X_REG_AVG_LSB		0x64

#define SX932X_REG_DIFF_MSB		0x65
#define SX932X_REG_DIFF_LSB		0x66

#define SX932X_REG_OFFSET_MSB		0x67
#define SX932X_REG_OFFSET_LSB		0x68

#define SX932X_REG_SAR_MSB		0x69
#define SX932X_REG_SAR_LSB		0x6a

#define SX932X_REG_RESET		0x9f
/* Write this to REG_RESET to do a soft reset. */
#define SX932X_SOFT_RESET		0xde

#define SX932X_REG_WHOAMI		0xfa
#define SX9320_WHOAMI			0x20
#define SX9321_WHOAMI			0x21
#define SX932X_REG_REVISION		0xfe


/* Sensor Readback */

/*
 * These serve for identifying IRQ source in the IRQ_SRC register, and
 * also for masking the IRQs in the IRQ_MSK register.
 */
#define SX932X_RESET_IRQ		BIT(7)
#define SX932X_CLOSE_IRQ		BIT(6)
#define SX932X_FAR_IRQ			BIT(5)
#define SX932X_COMPDONE_IRQ		BIT(4)
#define SX932X_CONVDONE_IRQ		BIT(3)

#define SX932X_SCAN_PERIOD_MASK		GENMASK(7, 4)
#define SX932X_SCAN_PERIOD_SHIFT	4

#define SX932X_COMPSTAT_MASK		GENMASK(3, 0)

/* 4 channels, as defined in STAT0: PH0, PH1, PH2 and PH3. */
#define SX932X_NUM_CHANNELS		4
#define SX932X_CHAN_MASK		GENMASK(1, 0)

struct sx932x_reg_config {
	const char *register_name;
	u8 reg;
	u8 def;
};

struct sx932x_data {
	struct mutex mutex;
	struct i2c_client *client;
	const struct sx932x_reg_config *reg_config;
	struct iio_trigger *trig;
	struct regmap *regmap;
	/*
	 * Last reading of the proximity status for each channel.
	 * We only send an event to user space when this changes.
	 */
	bool prox_stat[SX932X_NUM_CHANNELS];
	bool event_enabled[SX932X_NUM_CHANNELS];
	bool trigger_enabled;
	u16 *buffer;
	/* Remember which phases are enabled during a suspend. */
	unsigned int suspend_reg_gnrl_ctrl1;
	struct completion completion;
	int data_rdy_users, close_far_users;
	int channel_users[SX932X_NUM_CHANNELS];
};

static const struct iio_event_spec sx932x_events[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	},
};

#define SX932X_RAW_CHANNEL(idx, name, addr)			\
	{							\
		.type = IIO_PROXIMITY,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
		.indexed = 1,					\
		.channel = idx,					\
		.address = addr,				\
		.event_spec = sx932x_events,			\
		.num_event_specs = ARRAY_SIZE(sx932x_events),	\
		.extend_name = name,				\
		.scan_index = idx,				\
		.scan_type = {					\
			.sign = 's',				\
			.realbits = 16,				\
			.storagebits = 16,			\
			.shift = 0,				\
		},						\
	}

#define SX932X_PROCESSED_CHANNEL(idx, name, addr)		\
	{							\
		.type = IIO_PROXIMITY,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),	\
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
		.indexed = 1,					\
		.channel = idx,					\
		.address = addr,				\
		.event_spec = sx932x_events,			\
		.num_event_specs = ARRAY_SIZE(sx932x_events),	\
		.extend_name = name,				\
		.scan_index = idx,				\
		.scan_type = {					\
			.sign = 's',				\
			.realbits = 16,				\
			.storagebits = 16,			\
			.shift = 0,				\
		},						\
	}

static const struct iio_chan_spec sx932x_channels[] = {
	SX932X_RAW_CHANNEL(0, "UC0", SX932X_REG_USE_MSB),
	SX932X_RAW_CHANNEL(1, "UC1", SX932X_REG_USE_MSB),
	SX932X_RAW_CHANNEL(2, "UC2", SX932X_REG_USE_MSB),
	SX932X_RAW_CHANNEL(3, "UC3", SX932X_REG_USE_MSB),

	SX932X_RAW_CHANNEL(4, "AC0", SX932X_REG_AVG_MSB),
	SX932X_RAW_CHANNEL(5, "AC1", SX932X_REG_AVG_MSB),
	SX932X_RAW_CHANNEL(6, "AC2", SX932X_REG_AVG_MSB),
	SX932X_RAW_CHANNEL(7, "AC3", SX932X_REG_AVG_MSB),

	SX932X_RAW_CHANNEL(8,  "DC0", SX932X_REG_DIFF_MSB),
	SX932X_RAW_CHANNEL(9,  "DC1", SX932X_REG_DIFF_MSB),
	SX932X_RAW_CHANNEL(10, "DC2", SX932X_REG_DIFF_MSB),
	SX932X_RAW_CHANNEL(11, "DC3", SX932X_REG_DIFF_MSB),

	SX932X_RAW_CHANNEL(12, "CO0", SX932X_REG_OFFSET_MSB),
	SX932X_RAW_CHANNEL(13, "CO1", SX932X_REG_OFFSET_MSB),
	SX932X_RAW_CHANNEL(14, "CO2", SX932X_REG_OFFSET_MSB),
	SX932X_RAW_CHANNEL(15, "CO3", SX932X_REG_OFFSET_MSB),

	IIO_CHAN_SOFT_TIMESTAMP(16),
};

static const struct {
	int val;
	int val2;
} sx932x_samp_freq_table[] = {
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
static const unsigned int sx932x_scan_period_table[] = {
	2, 15, 30, 45, 60, 90, 120, 200, 400, 800, 1000, 2000, 3000, 4000, 5000,
};

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
	"500.0 66.666666 33.333333 22.222222 16.666666 "
	"11.111111 8.333333 5.0 2.500000 1.666666 1.250000 "
	"1.0 0.500000 8.333333 0.250000 0.200000");


static const struct regmap_range sx932x_writable_reg_ranges[] = {
	regmap_reg_range(SX932X_REG_IRQ_MSK, SX932X_REG_IRQ_CFG2),
	regmap_reg_range(SX932X_REG_GNRL_CTRL0, SX932X_REG_GNRL_CTRL1),
	/* Leave i2c and clock spreading as unavailable */
	regmap_reg_range(SX932X_REG_AFE_CTRL0, SX932X_REG_AFE_CTRL9),
	regmap_reg_range(SX932X_REG_PROX_CTRL0, SX932X_REG_PROX_CTRL7),
	regmap_reg_range(SX932X_REG_ADV_CTRL0, SX932X_REG_ADV_CTRL20),
	regmap_reg_range(SX932X_REG_PHASE_SEL, SX932X_REG_PHASE_SEL),
	regmap_reg_range(SX932X_REG_OFFSET_MSB, SX932X_REG_OFFSET_LSB),
	regmap_reg_range(SX932X_REG_RESET, SX932X_REG_RESET),
};

static const struct regmap_access_table sx932x_writeable_regs = {
	.yes_ranges = sx932x_writable_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(sx932x_writable_reg_ranges),
};

/*
 * All allocated registers are readable, so we just list unallocated
 * ones.
 */
static const struct regmap_range sx932x_non_readable_reg_ranges[] = {
	regmap_reg_range(SX932X_REG_IRQ_CFG2 + 1, SX932X_REG_GNRL_CTRL0 - 1),
	regmap_reg_range(SX932X_REG_GNRL_CTRL1 + 1, SX932X_REG_AFE_CTRL0 - 1),
	regmap_reg_range(SX932X_REG_AFE_CTRL9 + 1, SX932X_REG_PROX_CTRL0 - 1),
	regmap_reg_range(SX932X_REG_PROX_CTRL7 + 1, SX932X_REG_ADV_CTRL0 - 1),
	regmap_reg_range(SX932X_REG_ADV_CTRL20 + 1, SX932X_REG_PHASE_SEL - 1),
	regmap_reg_range(SX932X_REG_SAR_LSB + 1, SX932X_REG_RESET - 1),
	regmap_reg_range(SX932X_REG_RESET + 1, SX932X_REG_WHOAMI - 1),
	regmap_reg_range(SX932X_REG_WHOAMI + 1, SX932X_REG_REVISION - 1),
};

static const struct regmap_access_table sx932x_readable_regs = {
	.no_ranges = sx932x_non_readable_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(sx932x_non_readable_reg_ranges),
};

static const struct regmap_range sx932x_volatile_reg_ranges[] = {
	regmap_reg_range(SX932X_REG_IRQ_SRC, SX932X_REG_STAT3),
	regmap_reg_range(SX932X_REG_USE_MSB, SX932X_REG_DIFF_LSB),
	regmap_reg_range(SX932X_REG_SAR_MSB, SX932X_REG_SAR_LSB),
	regmap_reg_range(SX932X_REG_WHOAMI, SX932X_REG_WHOAMI),
	regmap_reg_range(SX932X_REG_REVISION, SX932X_REG_REVISION),
};

static const struct regmap_access_table sx932x_volatile_regs = {
	.yes_ranges = sx932x_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(sx932x_volatile_reg_ranges),
};

static const struct regmap_config sx932x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SX932X_REG_REVISION,
	.cache_type = REGCACHE_RBTREE,

	.wr_table = &sx932x_writeable_regs,
	.rd_table = &sx932x_readable_regs,
	.volatile_table = &sx932x_volatile_regs,
};

static int sx932x_inc_users(struct sx932x_data *data, int *counter,
			    unsigned int reg, unsigned int bitmask)
{
	++(*counter);
	if (*counter != 1)
		/* Bit is already active, nothing to do. */
		return 0;

	return regmap_update_bits(data->regmap, reg, bitmask, bitmask);
}

static int sx932x_dec_users(struct sx932x_data *data, int *counter,
			    unsigned int reg, unsigned int bitmask)
{
	--(*counter);
	if (*counter != 0)
		/* There are more users, do not deactivate. */
		return 0;

	return regmap_update_bits(data->regmap, reg, bitmask, 0);
}

static int sx932x_inc_chan_users(struct sx932x_data *data, int chan)
{
	return sx932x_inc_users(data, &data->channel_users[chan],
				SX932X_REG_PROX_CTRL0, BIT(chan));
}

static int sx932x_dec_chan_users(struct sx932x_data *data, int chan)
{
	return sx932x_dec_users(data, &data->channel_users[chan],
				SX932X_REG_PROX_CTRL0, BIT(chan));
}

static int sx932x_inc_data_rdy_users(struct sx932x_data *data)
{
	return sx932x_inc_users(data, &data->data_rdy_users,
				SX932X_REG_IRQ_MSK, SX932X_CONVDONE_IRQ);
}

static int sx932x_dec_data_rdy_users(struct sx932x_data *data)
{
	return sx932x_dec_users(data, &data->data_rdy_users,
				SX932X_REG_IRQ_MSK, SX932X_CONVDONE_IRQ);
}

static int sx932x_inc_close_far_users(struct sx932x_data *data)
{
	return sx932x_inc_users(data, &data->close_far_users,
				SX932X_REG_IRQ_MSK,
				SX932X_CLOSE_IRQ | SX932X_FAR_IRQ);
}

static int sx932x_dec_close_far_users(struct sx932x_data *data)
{
	return sx932x_dec_users(data, &data->close_far_users,
				SX932X_REG_IRQ_MSK,
				SX932X_CLOSE_IRQ | SX932X_FAR_IRQ);
}

static int sx932x_read_prox_data(struct sx932x_data *data,
				 const struct iio_chan_spec *chan,
				 int *val)
{
	int ret;
	__be16 regval;
	const int regid = chan->channel & 3;

	ret = regmap_write(data->regmap, SX932X_REG_PHASE_SEL, regid);
	if (ret < 0)
		return ret;

	ret = regmap_bulk_read(data->regmap, chan->address, &regval, 2);
	if (ret < 0)
		return ret;

	*val = sign_extend32(be16_to_cpu(regval), 15);

	return 0;
}

static int sx932x_read_stat_data(struct sx932x_data *data,
		const struct iio_chan_spec
		*chan,
		int *val)
{
	int ret;

	ret = regmap_read(data->regmap, chan->address, val);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * If we have no interrupt support, we have to wait for a scan period
 * after enabling a channel to get a result.
 */
static int sx932x_wait_for_sample(struct sx932x_data *data)
{
	int ret;
	unsigned int val;

	ret = regmap_read(data->regmap, SX932X_REG_PROX_CTRL0, &val);
	if (ret < 0)
		return ret;

	val = (val & SX932X_SCAN_PERIOD_MASK) >> SX932X_SCAN_PERIOD_SHIFT;

	msleep(sx932x_scan_period_table[val]);

	return 0;
}

static int sx932x_read_stat(struct sx932x_data *data,
				 const struct iio_chan_spec *chan,
				 int *val)
{
	int ret;

	mutex_lock(&data->mutex);

	ret = sx932x_inc_chan_users(data, chan->channel);
	if (ret < 0)
		goto out;

	ret = sx932x_inc_data_rdy_users(data);
	if (ret < 0)
		goto out_dec_chan;

	mutex_unlock(&data->mutex);

	if (data->client->irq > 0)
		ret = wait_for_completion_interruptible(&data->completion);
	else
		ret = sx932x_wait_for_sample(data);

	mutex_lock(&data->mutex);

	if (ret < 0)
		goto out_dec_data_rdy;

	ret = sx932x_read_stat_data(data, chan, val);
	if (ret < 0)
		goto out_dec_data_rdy;

	ret = sx932x_dec_data_rdy_users(data);
	if (ret < 0)
		goto out_dec_chan;

	ret = sx932x_dec_chan_users(data, chan->channel);
	if (ret < 0)
		goto out;

	ret = IIO_VAL_INT;

	goto out;

out_dec_data_rdy:
	sx932x_dec_data_rdy_users(data);
out_dec_chan:
	sx932x_dec_chan_users(data, chan->channel);
out:
	mutex_unlock(&data->mutex);
	reinit_completion(&data->completion);

	return ret;
}

static int sx932x_read_proximity(struct sx932x_data *data,
				 const struct iio_chan_spec *chan,
				 int *val)
{
	int ret;

	mutex_lock(&data->mutex);

	ret = sx932x_inc_chan_users(data, chan->channel);
	if (ret < 0)
		goto out;

	ret = sx932x_inc_data_rdy_users(data);
	if (ret < 0)
		goto out_dec_chan;

	mutex_unlock(&data->mutex);

	if (data->client->irq > 0)
		ret = wait_for_completion_interruptible(&data->completion);
	else
		ret = sx932x_wait_for_sample(data);

	mutex_lock(&data->mutex);

	if (ret < 0)
		goto out_dec_data_rdy;

	ret = sx932x_read_prox_data(data, chan, val);
	if (ret < 0)
		goto out_dec_data_rdy;

	ret = sx932x_dec_data_rdy_users(data);
	if (ret < 0)
		goto out_dec_chan;

	ret = sx932x_dec_chan_users(data, chan->channel);
	if (ret < 0)
		goto out;

	ret = IIO_VAL_INT;

	goto out;

out_dec_data_rdy:
	sx932x_dec_data_rdy_users(data);
out_dec_chan:
	sx932x_dec_chan_users(data, chan->channel);
out:
	mutex_unlock(&data->mutex);
	reinit_completion(&data->completion);

	return ret;
}

static int sx932x_read_samp_freq(struct sx932x_data *data,
				 int *val, int *val2)
{
	int ret;
	unsigned int regval;

	mutex_lock(&data->mutex);
	ret = regmap_read(data->regmap, SX932X_REG_PROX_CTRL0, &regval);
	mutex_unlock(&data->mutex);

	if (ret < 0)
		return ret;

	regval = (regval & SX932X_SCAN_PERIOD_MASK) >> SX932X_SCAN_PERIOD_SHIFT;
	*val = sx932x_samp_freq_table[regval].val;
	*val2 = sx932x_samp_freq_table[regval].val2;

	return IIO_VAL_INT_PLUS_MICRO;
}

static int sx932x_read_raw(struct iio_dev *indio_dev,
			   const struct iio_chan_spec *chan,
			   int *val, int *val2, long mask)
{
	struct sx932x_data *data = iio_priv(indio_dev);
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
			ret = sx932x_read_proximity(data, chan, val);
			mutex_unlock(&indio_dev->mlock);
			return ret;
		case IIO_CHAN_INFO_PROCESSED:
			mutex_lock(&indio_dev->mlock);

			if (iio_buffer_enabled(indio_dev)) {
				mutex_unlock(&indio_dev->mlock);
				return -EBUSY;
			}
			ret = sx932x_read_stat(data, chan, val);
			mutex_unlock(&indio_dev->mlock);
			return ret;
		case IIO_CHAN_INFO_SAMP_FREQ:
			return sx932x_read_samp_freq(data, val, val2);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static int sx932x_set_samp_freq(struct sx932x_data *data,
				int val, int val2)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(sx932x_samp_freq_table); i++)
		if (val == sx932x_samp_freq_table[i].val &&
		    val2 == sx932x_samp_freq_table[i].val2)
			break;

	if (i == ARRAY_SIZE(sx932x_samp_freq_table))
		return -EINVAL;

	mutex_lock(&data->mutex);

	ret = regmap_update_bits(data->regmap, SX932X_REG_PROX_CTRL0,
				 SX932X_SCAN_PERIOD_MASK,
				 i << SX932X_SCAN_PERIOD_SHIFT);

	mutex_unlock(&data->mutex);

	return ret;
}

static int sx932x_write_raw(struct iio_dev *indio_dev,
			    const struct iio_chan_spec *chan,
			    int val, int val2, long mask)
{
	struct sx932x_data *data = iio_priv(indio_dev);

	switch (chan->type) {
	case IIO_PROXIMITY:
		switch (mask) {
		case IIO_CHAN_INFO_SAMP_FREQ:
			return sx932x_set_samp_freq(data, val, val2);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static irqreturn_t sx932x_irq_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct sx932x_data *data = iio_priv(indio_dev);

	if (data->trigger_enabled)
		iio_trigger_poll(data->trig);

	/*
	 * Even if no event is enabled, we need to wake the thread to
	 * clear the interrupt state by reading SX932X_REG_IRQ_SRC.  It
	 * is not possible to do that here because regmap_read takes a
	 * mutex.
	 */
	return IRQ_WAKE_THREAD;
}

static void sx932x_push_events(struct iio_dev *indio_dev)
{
	int ret;
	unsigned int val, chan;
	struct sx932x_data *data = iio_priv(indio_dev);

	ret = regmap_read(data->regmap, SX932X_REG_STAT0, &val);
	if (ret < 0) {
		dev_err(&data->client->dev, "i2c transfer error in irq\n");
		return;
	}

	for (chan = 0; chan < SX932X_NUM_CHANNELS; chan++) {
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

static irqreturn_t sx932x_irq_thread_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret;
	unsigned int val;

	mutex_lock(&data->mutex);

	ret = regmap_read(data->regmap, SX932X_REG_IRQ_SRC, &val);
	if (ret < 0) {
		dev_err(&data->client->dev, "i2c transfer error in irq\n");
		goto out;
	}

	if (val & (SX932X_CLOSE_IRQ | SX932X_FAR_IRQ))
		sx932x_push_events(indio_dev);

	if (val & SX932X_CONVDONE_IRQ)
		complete(&data->completion);

out:
	mutex_unlock(&data->mutex);

	return IRQ_HANDLED;
}

static int sx932x_read_event_config(struct iio_dev *indio_dev,
				    const struct iio_chan_spec *chan,
				    enum iio_event_type type,
				    enum iio_event_direction dir)
{
	struct sx932x_data *data = iio_priv(indio_dev);

	if (chan->type != IIO_PROXIMITY || type != IIO_EV_TYPE_THRESH ||
	    dir != IIO_EV_DIR_EITHER)
		return -EINVAL;

	return data->event_enabled[chan->channel & SX932X_CHAN_MASK];
}

static int sx932x_write_event_config(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir,
				     int state)
{
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret;

	if (chan->type != IIO_PROXIMITY || type != IIO_EV_TYPE_THRESH ||
	    dir != IIO_EV_DIR_EITHER)
		return -EINVAL;

	mutex_lock(&data->mutex);

	if (state == 1) {
		ret = sx932x_inc_chan_users(data, chan->channel);
		if (ret < 0)
			goto out_unlock;
		ret = sx932x_inc_close_far_users(data);
		if (ret < 0)
			goto out_undo_chan;
	} else {
		ret = sx932x_dec_chan_users(data, chan->channel);
		if (ret < 0)
			goto out_unlock;
		ret = sx932x_dec_close_far_users(data);
		if (ret < 0)
			goto out_undo_chan;
	}

	data->event_enabled[chan->channel & SX932X_CHAN_MASK] = state;
	goto out_unlock;

out_undo_chan:
	if (state == 1)
		sx932x_dec_chan_users(data, chan->channel);
	else
		sx932x_inc_chan_users(data, chan->channel);
out_unlock:
	mutex_unlock(&data->mutex);
	return ret;
}

static int sx932x_update_scan_mode(struct iio_dev *indio_dev,
				   const unsigned long *scan_mask)
{
	struct sx932x_data *data = iio_priv(indio_dev);

	mutex_lock(&data->mutex);
	kfree(data->buffer);
	data->buffer = kzalloc(indio_dev->scan_bytes, GFP_KERNEL);
	mutex_unlock(&data->mutex);

	if (data->buffer == NULL)
		return -ENOMEM;

	return 0;
}

static ssize_t sx932x_uid_show(struct device *dev,
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

static IIO_DEVICE_ATTR(uid, 0444, sx932x_uid_show, NULL, 0);

struct sx932x_ctrl_attribute {
	struct device_attribute dev_attr;
	u8 ctrl;
};

static inline struct sx932x_ctrl_attribute *sx932x_to_ctrl_attr(
		struct device_attribute *attr)
{
	return container_of(attr, struct sx932x_ctrl_attribute, dev_attr);
}

static ssize_t sx932x_show_ctrl_attr(struct device *dev,
				     struct device_attribute *dev_attr,
				     char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sx932x_ctrl_attribute *attr = sx932x_to_ctrl_attr(dev_attr);
	struct sx932x_data *data = iio_priv(indio_dev);
	unsigned int val;
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_read(data->regmap, attr->ctrl, &val);
	mutex_unlock(&data->mutex);
	if (ret)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%02x\n", val);
}

static ssize_t sx932x_store_ctrl_attr(struct device *dev,
				      struct device_attribute *dev_attr,
				      const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sx932x_ctrl_attribute *attr = sx932x_to_ctrl_attr(dev_attr);
	struct sx932x_data *data = iio_priv(indio_dev);
	u8 val;
	int ret;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	mutex_lock(&data->mutex);
	ret = regmap_write(data->regmap, attr->ctrl, val);
	mutex_unlock(&data->mutex);
	if (ret)
		return ret;

	return len;
}

#define SX932X_CTRL_ATTR_FILL(_name, _mode, _show, _store, _ctrl, _offset) \
	{ .dev_attr	= __ATTR(_name, _mode, _show, _store),		\
	  .ctrl		= _ctrl + _offset }

#define SX932X_CTRL_ATTR(_name, _mode, _show, _store, _ctrl, _offset)	\
	struct sx932x_ctrl_attribute sx932x_ctrl_attr_##_name =		\
		SX932X_CTRL_ATTR_FILL( \
			_name, _mode, _show, _store, _ctrl, _offset)

#define SX932X_ADV_CTRL_RW(_ctrl)					\
	SX932X_CTRL_ATTR(adv_ctrl##_ctrl##_raw,	0644,			\
			 sx932x_show_ctrl_attr, sx932x_store_ctrl_attr,	\
			 _ctrl, SX932X_REG_ADV_CTRL0)

static SX932X_ADV_CTRL_RW(0);
static SX932X_ADV_CTRL_RW(1);
static SX932X_ADV_CTRL_RW(2);
static SX932X_ADV_CTRL_RW(3);
static SX932X_ADV_CTRL_RW(4);
static SX932X_ADV_CTRL_RW(5);
static SX932X_ADV_CTRL_RW(6);
static SX932X_ADV_CTRL_RW(7);
static SX932X_ADV_CTRL_RW(8);
static SX932X_ADV_CTRL_RW(9);
static SX932X_ADV_CTRL_RW(10);
static SX932X_ADV_CTRL_RW(11);
static SX932X_ADV_CTRL_RW(12);
static SX932X_ADV_CTRL_RW(13);
static SX932X_ADV_CTRL_RW(14);
static SX932X_ADV_CTRL_RW(15);
static SX932X_ADV_CTRL_RW(16);
static SX932X_ADV_CTRL_RW(17);
static SX932X_ADV_CTRL_RW(18);
static SX932X_ADV_CTRL_RW(19);
static SX932X_ADV_CTRL_RW(20);

static struct attribute *sx932x_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_uid.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl0_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl1_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl2_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl3_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl4_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl5_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl6_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl7_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl8_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl9_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl10_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl11_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl12_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl13_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl14_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl15_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl16_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl17_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl18_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl19_raw.dev_attr.attr,
	&sx932x_ctrl_attr_adv_ctrl20_raw.dev_attr.attr,
	NULL,
};

static const struct attribute_group sx932x_attribute_group = {
	.attrs = sx932x_attributes,
};

static const struct iio_info sx932x_info = {
	.attrs = &sx932x_attribute_group,
	.read_raw = &sx932x_read_raw,
	.write_raw = &sx932x_write_raw,
	.read_event_config = &sx932x_read_event_config,
	.write_event_config = &sx932x_write_event_config,
	.update_scan_mode = &sx932x_update_scan_mode,
};

static int sx932x_set_trigger_state(struct iio_trigger *trig,
				    bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);

	if (state)
		ret = sx932x_inc_data_rdy_users(data);
	else
		ret = sx932x_dec_data_rdy_users(data);
	if (ret < 0)
		goto out;

	data->trigger_enabled = state;

out:
	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_trigger_ops sx932x_trigger_ops = {
	.set_trigger_state = sx932x_set_trigger_state,
};

static irqreturn_t sx932x_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *pf = private;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct sx932x_data *data = iio_priv(indio_dev);
	int val, bit, ret, i = 0;

	mutex_lock(&data->mutex);

	for_each_set_bit(bit, indio_dev->active_scan_mask,
			 indio_dev->masklength) {
		ret = sx932x_read_prox_data(data, &indio_dev->channels[bit],
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

static int sx932x_buffer_preenable(struct iio_dev *indio_dev)
{
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret = 0, i;

	mutex_lock(&data->mutex);

	for (i = 0; i < SX932X_NUM_CHANNELS; i++)
		if (test_bit(i, indio_dev->active_scan_mask)) {
			ret = sx932x_inc_chan_users(data, i);
			if (ret)
				break;
		}

	if (ret)
		for (i = i - 1; i >= 0; i--)
			if (test_bit(i, indio_dev->active_scan_mask))
				sx932x_dec_chan_users(data, i);

	mutex_unlock(&data->mutex);

	return ret;
}

static int sx932x_buffer_predisable(struct iio_dev *indio_dev)
{
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret = 0, i;

	iio_triggered_buffer_predisable(indio_dev);

	mutex_lock(&data->mutex);

	for (i = 0; i < SX932X_NUM_CHANNELS; i++)
		if (test_bit(i, indio_dev->active_scan_mask)) {
			ret = sx932x_dec_chan_users(data, i);
			if (ret)
				break;
		}

	if (ret)
		for (i = i - 1; i >= 0; i--)
			if (test_bit(i, indio_dev->active_scan_mask))
				sx932x_inc_chan_users(data, i);

	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_buffer_setup_ops sx932x_buffer_setup_ops = {
	.preenable = sx932x_buffer_preenable,
	.postenable = iio_triggered_buffer_postenable,
	.predisable = sx932x_buffer_predisable,
};

#define SX9320_REG_CONFIG(_name, _reg, _def) \
{ \
	.register_name = SX9320_ACPI_NAME ",reg_" _name, \
	.reg = SX932X_REG_##_reg, \
	.def = _def \
}

#define SX9321_REG_CONFIG(_name, _reg, _def) \
{ \
	.register_name = SX9321_ACPI_NAME ",reg_" _name, \
	.reg = SX932X_REG_##_reg, \
	.def = _def \
}

static const struct sx932x_reg_config sx9320_reg_config[] = {
	{
		.register_name = NULL,
		.reg = SX932X_REG_ADV_CTRL11,
		.def = (5 << 4) | 2,
	},
	{
		.register_name = NULL,
		.reg = SX932X_REG_ADV_CTRL12,
		.def = (11 << 4) | 5,
	},
	SX9320_REG_CONFIG("adv_ctrl10", ADV_CTRL10, (5 << 4) | 12),
	SX9320_REG_CONFIG("afe_ctrl4", AFE_CTRL4, 7),
	SX9320_REG_CONFIG("afe_ctrl7", AFE_CTRL7, 7),
	SX9320_REG_CONFIG("afe_ctrl9", AFE_CTRL9, 15),
	SX9320_REG_CONFIG("prox_ctrl0", PROX_CTRL0, (2 << 3) | 2),
	SX9320_REG_CONFIG("prox_ctrl1", PROX_CTRL1, (2 << 3) | 2),
	SX9320_REG_CONFIG("prox_ctrl2", PROX_CTRL2, 0x80 | 16),
	SX9320_REG_CONFIG("prox_ctrl4", PROX_CTRL4, (1 << 3) | 4),
	SX9320_REG_CONFIG("prox_ctrl5", PROX_CTRL5, (1 << 4) | 2),
	SX9320_REG_CONFIG("prox_ctrl6", PROX_CTRL6, 60),
	SX9320_REG_CONFIG("prox_ctrl7", PROX_CTRL7, 88),
	SX9320_REG_CONFIG("adv_ctrl16", ADV_CTRL16, (3 << 4) | (2 << 2)),
	SX9320_REG_CONFIG("adv_ctrl17", ADV_CTRL17, (5 << 4) | 6),
	SX9320_REG_CONFIG("adv_ctrl18", ADV_CTRL18, (3 << 4) | 3),
	SX9320_REG_CONFIG("gnrl_ctrl1", GNRL_CTRL1, 0x2f),
	{
		.register_name = NULL,
		.reg = 0,
		.def = 0,
	},
};

static const struct sx932x_reg_config sx9321_reg_config[] = {
	{
		.register_name = NULL,
		.reg = SX932X_REG_ADV_CTRL11,
		.def = (5 << 4) | 2,
	},
	{
		.register_name = NULL,
		.reg = SX932X_REG_ADV_CTRL12,
		.def = (11 << 4) | 5,
	},
	SX9321_REG_CONFIG("adv_ctrl10", ADV_CTRL10, (5 << 4) | 12),
	SX9321_REG_CONFIG("afe_ctrl4", AFE_CTRL4, 7),
	SX9321_REG_CONFIG("afe_ctrl7", AFE_CTRL7, 7),
	SX9321_REG_CONFIG("afe_ctrl9", AFE_CTRL9, 15),
	SX9321_REG_CONFIG("prox_ctrl0", PROX_CTRL0, (2 << 3) | 2),
	SX9321_REG_CONFIG("prox_ctrl1", PROX_CTRL1, (2 << 3) | 2),
	SX9321_REG_CONFIG("prox_ctrl2", PROX_CTRL2, 0x80 | 16),
	SX9321_REG_CONFIG("prox_ctrl4", PROX_CTRL4, (1 << 3) | 4),
	SX9321_REG_CONFIG("prox_ctrl5", PROX_CTRL5, (1 << 4) | 2),
	SX9321_REG_CONFIG("prox_ctrl6", PROX_CTRL6, 60),
	SX9321_REG_CONFIG("prox_ctrl7", PROX_CTRL7, 88),
	SX9321_REG_CONFIG("adv_ctrl16", ADV_CTRL16, (3 << 4) | (2 << 2)),
	SX9321_REG_CONFIG("adv_ctrl17", ADV_CTRL17, (5 << 4) | 6),
	SX9321_REG_CONFIG("adv_ctrl18", ADV_CTRL18, (3 << 4) | 3),
	SX9321_REG_CONFIG("gnrl_ctrl1", GNRL_CTRL1, 0x2f),
	{
		.register_name = NULL,
		.reg = 0,
		.def = 0,
	},
};

static const struct sx932x_reg_config *sx932x_reg_configs[] = {
	sx9320_reg_config,
	sx9321_reg_config
};

static int sx932x_read_register_property(struct acpi_device *adev,
		const struct sx932x_reg_config *cfg,
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

static int sx932x_load_config(struct device *dev, struct sx932x_data *data)
{
	u8 val;
	int i, ret;
	const struct sx932x_reg_config *cfg;
	struct acpi_device *adev = ACPI_COMPANION(dev);

	for (i = 0; data->reg_config[i].reg != 0; i++) {
		cfg = &data->reg_config[i];
		ret = sx932x_read_register_property(adev, cfg, &val);
		if (ret < 0)
			return ret;

		ret = regmap_write(data->regmap, cfg->reg, val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Activate all channels and perform an initial compensation. */
static int sx932x_init_compensation(struct iio_dev *indio_dev)
{
	struct sx932x_data *data = iio_priv(indio_dev);
	int i, ret;
	bool success = false;
	unsigned int val;

	for (i = 100; i >= 0; i--) {
		usleep_range(10000, 20000);
		ret = regmap_read(data->regmap, SX932X_REG_STAT2, &val);
		if (ret < 0)
			goto out;
		if (!(val & SX932X_COMPSTAT_MASK)) {
			success = true;
			break;
		}
	}

	if (success) {
		dev_info(&data->client->dev,
			 "initial compensation success");
	} else {
		dev_err(&data->client->dev,
			"initial compensation timed out: 0x%02x", val);
		ret = -ETIMEDOUT;
	}

out:
	return ret;
}

static int sx932x_init_device(struct iio_dev *indio_dev)
{
	struct device *dev = &indio_dev->dev;
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret;
	unsigned int val;

	ret = regmap_read(data->regmap, SX932X_REG_WHOAMI, &val);
	if (ret < 0 || (val != SX9320_WHOAMI && val != SX9321_WHOAMI)) {
		dev_err(&data->client->dev,
			"Unable to identify the chip: %d - 0x%02x", ret, val);
		return -ENODEV;
	}

	ret = regmap_write(data->regmap, SX932X_REG_IRQ_MSK, 0);
	if (ret < 0)
		return ret;

	ret = regmap_write(data->regmap, SX932X_REG_RESET,
			   SX932X_SOFT_RESET);
	if (ret < 0)
		return ret;

	// wait for the reset to be completed
	usleep_range(10000, 20000);

	ret = regmap_write(data->regmap, SX932X_REG_RESET, 0);
	if (ret < 0)
		return ret;

	ret = regmap_read(data->regmap, SX932X_REG_IRQ_SRC, &val);
	if (ret < 0)
		return ret;

	ret = sx932x_load_config(dev, data);
	if (ret < 0)
		return ret;

	return sx932x_init_compensation(indio_dev);
}

static int sx932x_probe(struct i2c_client *client,
			const struct i2c_device_id *i2c_id)
{
	int ret;
	struct iio_dev *indio_dev;
	const struct acpi_device_id *acpi_id;
	struct sx932x_data *data;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (indio_dev == NULL)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	if (i2c_id) {
		data->reg_config = sx932x_reg_configs[i2c_id->driver_data];
	} else if (ACPI_HANDLE(&client->dev)) {
		acpi_id = acpi_match_device(
				client->dev.driver->acpi_match_table,
				&client->dev);
		if (!acpi_id) {
			dev_err(&client->dev, "No driver data\n");
			return -EINVAL;
		}
		data->reg_config = sx932x_reg_configs[acpi_id->driver_data];
	} else {
		return -ENODEV;
	}
	mutex_init(&data->mutex);
	init_completion(&data->completion);
	data->trigger_enabled = false;

	data->regmap = devm_regmap_init_i2c(client, &sx932x_regmap_config);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	ACPI_COMPANION_SET(&indio_dev->dev, ACPI_COMPANION(&client->dev));
	indio_dev->dev.parent = &client->dev;
	indio_dev->name = SX932X_DRIVER_NAME;
	indio_dev->channels = sx932x_channels;
	indio_dev->num_channels = ARRAY_SIZE(sx932x_channels);
	indio_dev->info = &sx932x_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	i2c_set_clientdata(client, indio_dev);

	ret = sx932x_init_device(indio_dev);
	if (ret < 0) {
		dev_err(&client->dev, "tried to init sx9320 - error %d\n",
			ret);
		return ret;
	}

	if (client->irq <= 0)
		dev_warn(&client->dev, "no valid irq found\n");
	else {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				sx932x_irq_handler, sx932x_irq_thread_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				SX932X_IRQ_NAME, indio_dev);
		if (ret < 0)
			return ret;

		data->trig = devm_iio_trigger_alloc(&client->dev,
				"%s-dev%d", indio_dev->name, indio_dev->id);
		if (!data->trig)
			return -ENOMEM;

		data->trig->dev.parent = &client->dev;
		data->trig->ops = &sx932x_trigger_ops;
		iio_trigger_set_drvdata(data->trig, indio_dev);

		ret = iio_trigger_register(data->trig);
		if (ret)
			return ret;
	}

	ret = devm_iio_triggered_buffer_setup(&client->dev, indio_dev,
					      NULL, sx932x_trigger_handler,
					      &sx932x_buffer_setup_ops);
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

static int sx932x_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct sx932x_data *data = iio_priv(indio_dev);

	if (client->irq > 0)
		iio_trigger_unregister(data->trig);
	kfree(data->buffer);
	return 0;
}

static int __maybe_unused sx932x_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_read(data->regmap, SX932X_REG_GNRL_CTRL1,
			  &data->suspend_reg_gnrl_ctrl1);

	if (ret < 0)
		goto out;

	// disable all phases, send the device to sleep
	ret = regmap_write(data->regmap, SX932X_REG_GNRL_CTRL1, 0);

out:
	mutex_unlock(&data->mutex);
	return ret;
}

static int __maybe_unused sx932x_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct sx932x_data *data = iio_priv(indio_dev);
	int ret;

	mutex_lock(&data->mutex);
	ret = regmap_write(data->regmap, SX932X_REG_GNRL_CTRL1,
			   data->suspend_reg_gnrl_ctrl1);
	mutex_unlock(&data->mutex);

	return ret;
}

static const struct dev_pm_ops sx932x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sx932x_suspend, sx932x_resume)
};

static const struct acpi_device_id sx932x_acpi_match[] = {
	{SX9320_ACPI_NAME, 0},
	{SX9321_ACPI_NAME, 1},
	{ },
};
MODULE_DEVICE_TABLE(acpi, sx932x_acpi_match);

static const struct of_device_id sx932x_of_match[] = {
	{ .compatible = "semtech,sx9320", },
	{ .compatible = "semtech,sx9321", },
	{ }
};
MODULE_DEVICE_TABLE(of, sx932x_of_match);

static const struct i2c_device_id sx932x_id[] = {
	{"sx9320", 0},
	{"sx9321", 1},
	{ },
};
MODULE_DEVICE_TABLE(i2c, sx932x_id);

static struct i2c_driver sx932x_driver = {
	.driver = {
		.name	= SX932X_DRIVER_NAME,
		.acpi_match_table = ACPI_PTR(sx932x_acpi_match),
		.of_match_table = of_match_ptr(sx932x_of_match),
		.pm = &sx932x_pm_ops,
	},
	.probe		= sx932x_probe,
	.remove		= sx932x_remove,
	.id_table	= sx932x_id,
};
module_i2c_driver(sx932x_driver);

MODULE_AUTHOR("Gwendal Grignou <gwendal@chromium.org>");
MODULE_DESCRIPTION("Driver for Semtech SX9320/SX9321 proximity sensor");
MODULE_LICENSE("GPL v2");
