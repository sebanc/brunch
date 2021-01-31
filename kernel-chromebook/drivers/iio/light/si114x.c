/*
 * si114x.c - Support for Silabs si114x combined ambient light and
 * proximity sensor
 *
 * Copyright 2012 Peter Meerwald <pmeerw@pmeerw.net>
 *
 * This file is subject to the terms and conditions of version 2 of
 * the GNU General Public License.  See the file COPYING in the main
 * directory of this archive for more details.
 *
 * IIO driver for si114x (7-bit I2C slave address 0x5a) with sequencer
 * version >= A03
 *
 * driver supports IRQ and non-IRQ mode; an IRQ is required for
 * autonomous measurement mode
 * TODO:
 *   thresholds
 *   power management (measurement rate zero)
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/gpio.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/buffer.h>

#define SI114X_REG_PART_ID		0x00
#define SI114X_REG_REV_ID		0x01
#define SI114X_REG_SEQ_ID		0x02
#define SI114X_REG_INT_CFG		0x03
#define SI114X_REG_IRQ_ENABLE		0x04
#define SI114X_REG_IRQ_MODE1		0x05
#define SI114X_REG_IRQ_MODE2		0x06
#define SI114X_REG_HW_KEY		0x07
/* RATE stores a 16 bit value compressed to 8 bit */
#define SI114X_REG_MEAS_RATE		0x08
#define SI114X_REG_ALS_RATE		0x09
#define SI114X_REG_PS_RATE		0x0a
#define SI114X_REG_ALS_LOW_TH0		0x0b
#define SI114X_REG_ALS_LOW_TH1		0x0c
#define SI114X_REG_ALS_HI_TH0		0x0d
#define SI114X_REG_ALS_HI_TH1		0x0e
#define SI114X_REG_PS_LED21		0x0f
#define SI114X_REG_PS_LED3		0x10
/*
 * for rev A10 and below TH0 stores a 16 bit value compressed to 8 bit and
 * TH1 is not used; newer revision have the LSB in TH0 and the MSB in TH1
 */
#define SI114X_REG_PS1_TH0		0x11
#define SI114X_REG_PS1_TH1		0x12
#define SI114X_REG_PS2_TH0		0x13
#define SI114X_REG_PS2_TH1		0x11
#define SI114X_REG_PS3_TH0		0x15
#define SI114X_REG_PS3_TH1		0x16
#define SI114X_REG_PARAM_WR		0x17
#define SI114X_REG_COMMAND		0x18
#define SI114X_REG_RESPONSE		0x20
#define SI114X_REG_IRQ_STATUS		0x21
#define SI114X_REG_ALSVIS_DATA0		0x22
#define SI114X_REG_ALSVIS_DATA1		0x23
#define SI114X_REG_ALSIR_DATA0		0x24
#define SI114X_REG_ALSIR_DATA1		0x25
#define SI114X_REG_PS1_DATA0		0x26
#define SI114X_REG_PS1_DATA1		0x27
#define SI114X_REG_PS2_DATA0		0x28
#define SI114X_REG_PS2_DATA1		0x29
#define SI114X_REG_PS3_DATA0		0x2a
#define SI114X_REG_PS3_DATA1		0x2b
#define SI114X_REG_AUX_DATA0		0x2c
#define SI114X_REG_AUX_DATA1		0x2d
#define SI114X_REG_PARAM_RD		0x2e
#define SI114X_REG_CHIP_STAT		0x30

/* helper to figure out PS_LED register / shift per channel */
#define SI114X_PS_LED_REG(ch) \
	(((ch) == 2) ? SI114X_REG_PS_LED3 : SI114X_REG_PS_LED21)
#define SI114X_PS_LED_SHIFT(ch) \
	(((ch) == 1) ? 4 : 0)

/* Parameter offsets */
#define SI114X_PARAM_I2C_ADDR		0x00
#define SI114X_PARAM_CHLIST		0x01
#define SI114X_PARAM_PSLED12_SELECT	0x02
#define SI114X_PARAM_PSLED3_SELECT	0x03
#define SI114X_PARAM_FILTER_EN		0x04
#define SI114X_PARAM_PS_ENCODING	0x05
#define SI114X_PARAM_ALS_ENCODING	0x06
#define SI114X_PARAM_PS1_ADC_MUX	0x07
#define SI114X_PARAM_PS2_ADC_MUX	0x08
#define SI114X_PARAM_PS3_ADC_MUX	0x09
#define SI114X_PARAM_PS_ADC_COUNTER	0x0a
#define SI114X_PARAM_PS_ADC_GAIN	0x0b
#define SI114X_PARAM_PS_ADC_MISC	0x0c
#define SI114X_PARAM_ALS_ADC_MUX	0x0d
#define SI114X_PARAM_ALSIR_ADC_MUX	0x0e
#define SI114X_PARAM_AUX_ADC_MUX	0x0f
#define SI114X_PARAM_ALSVIS_ADC_COUNTER	0x10
#define SI114X_PARAM_ALSVIS_ADC_GAIN	0x11
#define SI114X_PARAM_ALSVIS_ADC_MISC	0x12
#define SI114X_PARAM_ALS_HYST		0x16
#define SI114X_PARAM_PS_HYST		0x17
#define SI114X_PARAM_PS_HISTORY		0x18
#define SI114X_PARAM_ALS_HISTORY	0x19
#define SI114X_PARAM_ADC_OFFSET		0x1a
#define SI114X_PARAM_SLEEP_CTRL		0x1b
#define SI114X_PARAM_LED_RECOVERY	0x1c
#define SI114X_PARAM_ALSIR_ADC_COUNTER	0x1d
#define SI114X_PARAM_ALSIR_ADC_GAIN	0x1e
#define SI114X_PARAM_ALSIR_ADC_MISC	0x1f

/* Channel enable masks for CHLIST parameter */
#define SI114X_CHLIST_EN_PS1		0x01
#define SI114X_CHLIST_EN_PS2		0x02
#define SI114X_CHLIST_EN_PS3		0x04
#define SI114X_CHLIST_EN_ALSVIS		0x10
#define SI114X_CHLIST_EN_ALSIR		0x20
#define SI114X_CHLIST_EN_AUX		0x40

/* Signal range mask for ADC_MISC parameter */
#define SI114X_MISC_RANGE		0x20

/* Commands for REG_COMMAND */
#define SI114X_CMD_NOP			0x00
#define SI114X_CMD_RESET		0x01
#define SI114X_CMD_BUSADDR		0x02
#define SI114X_CMD_PS_FORCE		0x05
#define SI114X_CMD_ALS_FORCE		0x06
#define SI114X_CMD_PSALS_FORCE		0x07
#define SI114X_CMD_PS_PAUSE		0x09
#define SI114X_CMD_ALS_PAUSE		0x0a
#define SI114X_CMD_PSALS_PAUSE		0x0b
#define SI114X_CMD_PS_AUTO		0x0d
#define SI114X_CMD_ALS_AUTO		0x0e
#define SI114X_CMD_PSALS_AUTO		0x0f
#define SI114X_CMD_PARAM_QUERY		0x80
#define SI114X_CMD_PARAM_SET		0xa0
#define SI114X_CMD_PARAM_AND		0xc0
#define SI114X_CMD_PARAM_OR		0xe0

/* Interrupt configuration masks for INT_CFG register */
#define SI114X_INT_CFG_OE		0x01 /* enable interrupt */
#define SI114X_INT_CFG_MODE		0x02 /* auto reset interrupt pin */

/* Interrupt enable masks for IRQ_ENABLE register */
#define SI114X_CMD_IE			0x20
#define SI114X_PS3_IE			0x10
#define SI114X_PS2_IE			0x08
#define SI114X_PS1_IE			0x04
#define SI114X_ALS_INT1_IE		0x02
#define SI114X_ALS_INT0_IE		0x01

/* Interrupt mode masks for IRQ_MODE1 register */
#define SI114X_PS2_IM_GREATER		0xc0
#define SI114X_PS2_IM_CROSS		0x40
#define SI114X_PS1_IM_GREATER		0x30
#define SI114X_PS1_IM_CROSS		0x10

/* Interrupt mode masks for IRQ_MODE2 register */
#define SI114X_CMD_IM_ERROR		0x04
#define SI114X_PS3_IM_GREATER		0x03
#define SI114X_PS3_IM_CROSS		0x01

/* Measurement rate settings */
#define SI114X_MEAS_RATE_FORCED		0x00
#define SI114X_MEAS_RATE_10MS		0x84
#define SI114X_MEAS_RATE_20MS		0x94
#define SI114X_MEAS_RATE_100MS		0xb9
#define SI114X_MEAS_RATE_496MS		0xdf
#define SI114X_MEAS_RATE_1984MS		0xff

/* ALS rate settings relative to measurement rate */
#define SI114X_ALS_RATE_OFF		0x00
#define SI114X_ALS_RATE_1X		0x08
#define SI114X_ALS_RATE_10X		0x32
#define SI114X_ALS_RATE_100X		0x69

/* PS rate settings relative to measurement rate */
#define SI114X_PS_RATE_OFF		0x00
#define SI114X_PS_RATE_1X		0x08
#define SI114X_PS_RATE_10X		0x32
#define SI114X_PS_RATE_100X		0x69

/* Sequencer revision from SEQ_ID */
#define SI114X_SEQ_REV_A01		0x01
#define SI114X_SEQ_REV_A02		0x02
#define SI114X_SEQ_REV_A03		0x03
#define SI114X_SEQ_REV_A10		0x08
#define SI114X_SEQ_REV_A11		0x09

#define SI114X_DRV_NAME "si114x"

/**
 * struct si114x_data - si114x chip state data
 * @client:	I2C client
 * @mutex:	mutex to protect multi-step I2C accesses and state changes
 * @part:	chip part number (0x41, 0x42, 0x43) for 1 to 3 LEDs version
 * @seq:	sequencer firmware revision determines chip features
 * @data_avail:	wait queue for single measurement IRQ completion
 * @got_data:	wait condition variable
 * @use_irq:	set when IRQ is available
 * @autonomous:	set when chip performs autonomous measurements (only when
 *		IRQ available)
 * @trig:	IIO trigger (only when IRQ available)
 *
 * The driver supports polling and IRQ operation for single and buffered
 * measurements. A device trigger enables autonomous measurements are when
 * an IRQ is available.
 **/
struct si114x_data {
	struct i2c_client *client;
	struct mutex mutex;
	u8 part;
	u8 seq;
	wait_queue_head_t data_avail;
	bool got_data;
	bool use_irq;
	bool autonomous;
	struct iio_trigger *trig;
};

/* expand 8 bit compressed value to 16 bit, see Silabs AN498 */
static u16 si114x_uncompress(u8 x)
{
	u16 result = 0;
	u8 exponent = 0;

	if (x < 8)
		return 0;

	exponent = (x & 0xf0) >> 4;
	result = 0x10 | (x & 0x0f);

	if (exponent >= 4)
		return result << (exponent - 4);
	return result >> (4 - exponent);
}

/* compress 16 bit to 8 bit using 4 bit exponent and 4 bit fraction,
 * see Silabs AN498 */
static u8 si114x_compress(u16 x)
{
	u32 exponent = 0;
	u32 significand = 0;
	u32 tmp = x;

	if (x == 0x0000)
		return 0x00;
	if (x == 0x0001)
		return 0x08;

	while (1) {
		tmp >>= 1;
		exponent += 1;
		if (tmp == 1)
			break;
	}

	if (exponent < 5) {
		significand = x << (4 - exponent);
		return (exponent << 4) | (significand & 0xF);
	}

	significand = x >> (exponent - 5);
	if (significand & 1) {
		significand += 2;
		if (significand & 0x0040) {
			exponent += 1;
			significand >>= 1;
		}
	}

	return (exponent << 4) | ((significand >> 1) & 0xF);
}

/* helper function to operate on parameter values: op can be query/set/or/and */
static int si114x_param_op(struct si114x_data *data, u8 op, u8 param, u8 value)
{
	struct i2c_client *client = data->client;
	int ret;

	mutex_lock(&data->mutex);

	if (op != SI114X_CMD_PARAM_QUERY) {
		ret = i2c_smbus_write_byte_data(client,
			SI114X_REG_PARAM_WR, value);
		if (ret < 0)
			goto error;
	}

	ret = i2c_smbus_write_byte_data(client, SI114X_REG_COMMAND,
		op | (param & 0x1F));
	if (ret < 0)
		goto error;

	ret = i2c_smbus_read_byte_data(client, SI114X_REG_PARAM_RD);
	if (ret < 0)
		return ret;

	mutex_unlock(&data->mutex);

	return ret & 0xff;
error:
	mutex_unlock(&data->mutex);
	return ret;
}

static irqreturn_t si114x_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *pf = private;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct si114x_data *data = iio_priv(indio_dev);
	/* maximum buffer size:
	 * 6*2 bytes channels data + 4 bytes alignment +
	 * 8 bytes timestamp
	 */
	u8 buffer[24];
	int len = 0;
	int i, j = 0;
	int ret;

	if (!data->autonomous) {
		ret = i2c_smbus_write_byte_data(data->client,
			SI114X_REG_COMMAND, SI114X_CMD_PSALS_FORCE);
		if (ret < 0)
			goto done;
		msleep(20);
	}

	for_each_set_bit(i, indio_dev->active_scan_mask,
		indio_dev->masklength) {
		ret = i2c_smbus_read_word_data(data->client,
			indio_dev->channels[i].address);
		if (ret < 0)
			goto done;

		((u16 *) buffer)[j++] = ret & 0xffff;
		len += 2;
	}

	if (indio_dev->scan_timestamp)
		*(s64 *)(buffer + ALIGN(len, sizeof(s64)))
			= iio_get_time_ns();
	iio_push_to_buffers(indio_dev, buffer);

done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static irqreturn_t si114x_irq(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;

	ret = i2c_smbus_read_byte_data(data->client,
		SI114X_REG_IRQ_STATUS);
	if (ret < 0 || !(ret & (SI114X_PS3_IE | SI114X_PS2_IE | SI114X_PS1_IE |
		SI114X_ALS_INT1_IE | SI114X_ALS_INT0_IE)))
		return IRQ_HANDLED;

	if (iio_buffer_enabled(indio_dev))
		iio_trigger_poll_chained(indio_dev->trig);
	else {
		data->got_data = true;
		wake_up_interruptible(&data->data_avail);
	}

	/* clearing IRQ */
	ret = i2c_smbus_write_byte_data(data->client,
		SI114X_REG_IRQ_STATUS, ret & 0x1f);
	if (ret < 0)
		dev_err(&data->client->dev, "clearing irq failed\n");

	return IRQ_HANDLED;
}

static int si114x_trigger_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;
	int cmd;

	/* configure autonomous mode */
	cmd = state ? SI114X_CMD_PSALS_AUTO :
		SI114X_CMD_PSALS_PAUSE;

	ret = i2c_smbus_write_byte_data(data->client,
		SI114X_REG_COMMAND, cmd);
	if (ret < 0)
		return ret;

	data->autonomous = state;

	return 0;
}

static const struct iio_trigger_ops si114x_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = si114x_trigger_set_state,
};

static int si114x_probe_trigger(struct iio_dev *indio_dev)
{
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;

	data->trig = iio_trigger_alloc("si114x-dev%d", indio_dev->id);
	if (!data->trig)
		return -ENOMEM;

	data->trig->dev.parent = &data->client->dev;
	data->trig->ops = &si114x_trigger_ops;
	ret = iio_trigger_register(data->trig);
	if (ret)
		goto error_free_trig;

	/* select default trigger */
	indio_dev->trig = data->trig;

	return 0;

error_free_trig:
	iio_trigger_free(data->trig);
	return ret;
}

static void si114x_remove_trigger(struct iio_dev *indio_dev)
{
	struct si114x_data *data = iio_priv(indio_dev);

	iio_trigger_unregister(data->trig);
	iio_trigger_free(data->trig);
}

static int si114x_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	struct si114x_data *data = iio_priv(indio_dev);
	int ret = -EINVAL;
	u8 cmd, reg;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_LIGHT:
		case IIO_PROXIMITY:
		case IIO_TEMP:
			if (iio_buffer_enabled(indio_dev))
				return -EBUSY;

			if (chan->type == IIO_PROXIMITY)
				cmd = SI114X_CMD_PS_FORCE;
			else
				cmd = SI114X_CMD_ALS_FORCE;
			ret = i2c_smbus_write_byte_data(data->client,
				SI114X_REG_COMMAND, cmd);
			if (ret < 0)
				return ret;
			if (data->use_irq) {
				ret = wait_event_interruptible_timeout(
					data->data_avail, data->got_data,
					msecs_to_jiffies(1000));
				data->got_data = false;
				if (ret == 0)
					ret = -ETIMEDOUT;
			} else
				msleep(20);
			if (ret < 0)
				return ret;

			ret = i2c_smbus_read_word_data(data->client,
				chan->address);
			if (ret < 0)
				return ret;

			*val = ret & 0xffff;

			ret = IIO_VAL_INT;
			break;
		case IIO_CURRENT:
			ret = i2c_smbus_read_byte_data(data->client,
				SI114X_PS_LED_REG(chan->channel));
			if (ret < 0)
				return ret;

			*val = (ret >> SI114X_PS_LED_SHIFT(chan->channel))
				& 0x0f;

			ret = IIO_VAL_INT;
			break;
		default:
			break;
		}
		break;
	case IIO_CHAN_INFO_HARDWAREGAIN:
		switch (chan->type) {
		case IIO_PROXIMITY:
			reg = SI114X_PARAM_PS_ADC_GAIN;
			break;
		case IIO_LIGHT:
			if (chan->channel2 == IIO_MOD_LIGHT_IR)
				reg = SI114X_PARAM_ALSIR_ADC_GAIN;
			else
				reg = SI114X_PARAM_ALSVIS_ADC_GAIN;
			break;
		default:
			return -EINVAL;
		}

		ret = si114x_param_op(data, SI114X_CMD_PARAM_QUERY, reg, 0);
		if (ret < 0)
			return ret;

		*val = ret & 0x07;

		ret = IIO_VAL_INT;
		break;
	}

	return ret;
}

static int si114x_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val, int val2, long mask)
{
	struct si114x_data *data = iio_priv(indio_dev);
	u8 reg1, reg2, shift;
	int ret = -EINVAL;

	switch (mask) {
	case IIO_CHAN_INFO_HARDWAREGAIN:
		switch (chan->type) {
		case IIO_PROXIMITY:
			if (val < 0 || val > 5)
				return -EINVAL;
			reg1 = SI114X_PARAM_PS_ADC_GAIN;
			reg2 = SI114X_PARAM_PS_ADC_COUNTER;
			break;
		case IIO_LIGHT:
			if (val < 0 || val > 7)
				return -EINVAL;
			if (chan->channel2 == IIO_MOD_LIGHT_IR) {
				reg1 = SI114X_PARAM_ALSIR_ADC_GAIN;
				reg2 = SI114X_PARAM_ALSIR_ADC_COUNTER;
			} else {
				reg1 = SI114X_PARAM_ALSVIS_ADC_GAIN;
				reg2 = SI114X_PARAM_ALSVIS_ADC_COUNTER;
			}
			break;
		default:
			return -EINVAL;
		}

		ret = si114x_param_op(data, SI114X_CMD_PARAM_SET,
			reg1, val);
		if (ret < 0)
			return ret;
		/* set recovery period to one's complement of gain */
		ret = si114x_param_op(data, SI114X_CMD_PARAM_SET,
			reg2, (~val & 0x07) << 4);
		break;
	case IIO_CHAN_INFO_RAW:
		if (chan->type != IIO_CURRENT)
			return -EINVAL;

		if (val < 0 || val > 0xf)
			return -EINVAL;

		reg1 = SI114X_PS_LED_REG(chan->channel);
		shift = SI114X_PS_LED_SHIFT(chan->channel);
		ret = i2c_smbus_read_byte_data(data->client, reg1);
		if (ret < 0)
			return ret;
		ret = i2c_smbus_write_byte_data(data->client, reg1,
			(ret & ~(0x0f << shift)) |
			((val & 0x0f) << shift));
		break;
	}
	return ret;
}

#if defined(CONFIG_DEBUG_FS)
static int si114x_reg_access(struct iio_dev *indio_dev,
			      unsigned reg, unsigned writeval,
			      unsigned *readval)
{
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;

	if (readval) {
		ret = i2c_smbus_read_byte_data(data->client, reg);
		if (ret < 0)
			return ret;
		*readval = ret;
		ret = 0;
	} else
		ret = i2c_smbus_write_byte_data(data->client, reg, writeval);

	return ret;
}
#endif

static int si114x_revisions(struct si114x_data *data)
{
	int ret = i2c_smbus_read_byte_data(data->client, SI114X_REG_PART_ID);
	if (ret < 0)
		return ret;

	switch (ret) {
	case 0x41:
	case 0x42:
	case 0x43:
		data->part = ret;
		break;
	default:
		dev_err(&data->client->dev, "invalid part\n");
		return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(data->client, SI114X_REG_SEQ_ID);
	if (ret < 0)
		return ret;
	data->seq = ret;

	if (data->seq < SI114X_SEQ_REV_A03)
		dev_info(&data->client->dev,
			 "WARNING: old sequencer revision\n");

	return 0;
}

static inline unsigned int si114x_leds(struct si114x_data *data)
{
	return data->part - 0x40;
}

#define SI114X_LIGHT_CHANNEL(_si) { \
	.type = IIO_LIGHT, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | \
			      BIT(IIO_CHAN_INFO_HARDWAREGAIN), \
	.scan_type = { \
		.sign = 'u', \
		.realbits = 16, \
		.storagebits = 16, \
		.shift = 0, \
		.endianness = IIO_LE, \
	}, \
	.scan_index = _si, \
	.address = SI114X_REG_ALSVIS_DATA0, \
}

#define SI114X_LIGHT_IR_CHANNEL(_si) { \
	.type = IIO_LIGHT, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | \
			      BIT(IIO_CHAN_INFO_HARDWAREGAIN), \
	.modified = 1, \
	.channel2 = IIO_MOD_LIGHT_IR, \
	.scan_type = { \
		.sign = 'u', \
		.realbits = 16, \
		.storagebits = 16, \
		.shift = 0, \
		.endianness = IIO_LE, \
	}, \
	.scan_index = _si, \
	.address = SI114X_REG_ALSIR_DATA0 \
}

#define SI114X_TEMP_CHANNEL(_si) { \
	.type = IIO_TEMP, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.scan_type = { \
		.sign = 'u', \
		.realbits = 16, \
		.storagebits = 16, \
		.shift = 0, \
		.endianness = IIO_LE, \
	}, \
	.scan_index = _si, \
	.address = SI114X_REG_AUX_DATA0 \
}

#define SI114X_PROXIMITY_CHANNEL(_si, _ch) { \
	.type = IIO_PROXIMITY, \
	.indexed = 1, \
	.channel = _ch, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | \
			      BIT(IIO_CHAN_INFO_HARDWAREGAIN), \
	.scan_type = { \
		.sign = 'u', \
		.realbits = 16, \
		.storagebits = 16, \
		.shift = 0, \
		.endianness = IIO_LE, \
	}, \
	.scan_index = _si, \
	.address = SI114X_REG_PS1_DATA0 + _ch*2 \
}

#define SI114X_CURRENT_CHANNEL(_ch) { \
	.type = IIO_CURRENT, \
	.indexed = 1, \
	.channel = _ch, \
	.output = 1, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) \
}

static const struct iio_chan_spec si114x_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(0),
	SI114X_LIGHT_CHANNEL(1),
	SI114X_LIGHT_IR_CHANNEL(2),
	SI114X_TEMP_CHANNEL(3),
	SI114X_PROXIMITY_CHANNEL(4, 0),
	SI114X_CURRENT_CHANNEL(0),
	SI114X_PROXIMITY_CHANNEL(5, 1),
	SI114X_CURRENT_CHANNEL(1),
	SI114X_PROXIMITY_CHANNEL(6, 2),
	SI114X_CURRENT_CHANNEL(2),
};

static ssize_t si114x_range_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct iio_dev_attr *dev_attr = to_iio_dev_attr(attr);
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;

	if (sysfs_streq(buf, "normal"))
		ret = si114x_param_op(data, SI114X_CMD_PARAM_AND,
			dev_attr->address, ~SI114X_MISC_RANGE);
	else if (sysfs_streq(buf, "high"))
		ret = si114x_param_op(data, SI114X_CMD_PARAM_OR,
			dev_attr->address, SI114X_MISC_RANGE);
	else
		return -EINVAL;

	return ret ? ret : len;
}

static ssize_t si114x_range_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct iio_dev_attr *dev_attr = to_iio_dev_attr(attr);
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;

	ret = si114x_param_op(data, SI114X_CMD_PARAM_QUERY,
		dev_attr->address, 0);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%s\n",
		(ret & SI114X_MISC_RANGE) ? "high" : "normal");
}

static IIO_DEVICE_ATTR(in_proximity_range, S_IRUGO | S_IWUSR,
			si114x_range_show,
			si114x_range_store,
			SI114X_PARAM_PS_ADC_MISC);

static IIO_DEVICE_ATTR(in_illuminance_range, S_IRUGO | S_IWUSR,
			si114x_range_show,
			si114x_range_store,
			SI114X_PARAM_ALSVIS_ADC_MISC);

static IIO_DEVICE_ATTR(in_illuminance_ir_range, S_IRUGO | S_IWUSR,
			si114x_range_show,
			si114x_range_store,
			SI114X_PARAM_ALSIR_ADC_MISC);

static IIO_CONST_ATTR(in_proximity_range_available, "normal high");
static IIO_CONST_ATTR(in_illuminance_range_available, "normal high");
static IIO_CONST_ATTR(in_illuminance_ir_range_available, "normal high");

static int si114x_set_chlist(struct iio_dev *indio_dev, bool all)
{
	struct si114x_data *data = iio_priv(indio_dev);
	u8 reg = 0;
	int i;

	if (all) {
		reg = SI114X_CHLIST_EN_ALSVIS | SI114X_CHLIST_EN_ALSIR |
			SI114X_CHLIST_EN_AUX;
		switch (si114x_leds(data)) {
		case 3:
			reg |= SI114X_CHLIST_EN_PS3;
		case 2:
			reg |= SI114X_CHLIST_EN_PS2;
		case 1:
			reg |= SI114X_CHLIST_EN_PS1;
			break;
		}
	} else
		for_each_set_bit(i, indio_dev->active_scan_mask,
			indio_dev->masklength) {
			switch (indio_dev->channels[i].address) {
			case SI114X_REG_ALSVIS_DATA0:
				reg |= SI114X_CHLIST_EN_ALSVIS;
				break;
			case SI114X_REG_ALSIR_DATA0:
				reg |= SI114X_CHLIST_EN_ALSIR;
				break;
			case SI114X_REG_PS1_DATA0:
				reg |= SI114X_CHLIST_EN_PS1;
				break;
			case SI114X_REG_PS2_DATA0:
				reg |= SI114X_CHLIST_EN_PS2;
				break;
			case SI114X_REG_PS3_DATA0:
				reg |= SI114X_CHLIST_EN_PS3;
				break;
			case SI114X_REG_AUX_DATA0:
				reg |= SI114X_CHLIST_EN_AUX;
				break;
			}
		}

	return si114x_param_op(data, SI114X_CMD_PARAM_SET,
		SI114X_PARAM_CHLIST, reg);
}

static int si114x_initialize(struct iio_dev *indio_dev)
{
	struct si114x_data *data = iio_priv(indio_dev);
	struct i2c_client *client = data->client;
	int ret;

	/* send reset command */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_COMMAND,
		SI114X_CMD_RESET);
	if (ret < 0)
		return ret;
	msleep(20);

	/* hardware key, magic value */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_HW_KEY, 0x17);
	if (ret < 0)
		return ret;
	msleep(20);

	/* interrupt configuration, interrupt output enable */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_INT_CFG,
		data->use_irq ? SI114X_INT_CFG_OE : 0);
	if (ret < 0)
		return ret;

	/* enable interrupt for certain activities */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_IRQ_ENABLE,
		SI114X_PS3_IE | SI114X_PS2_IE | SI114X_PS1_IE |
		SI114X_ALS_INT0_IE);
	if (ret < 0)
		return ret;

	/* in autonomous mode, wakeup every 100 ms */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_MEAS_RATE,
	    SI114X_MEAS_RATE_100MS);
	if (ret < 0)
		return ret;

	/* measure ALS every time device wakes up */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_ALS_RATE,
		SI114X_ALS_RATE_1X);
	if (ret < 0)
		return ret;

	/* measure proximity every time device wakes up */
	ret = i2c_smbus_write_byte_data(client, SI114X_REG_PS_RATE,
		SI114X_PS_RATE_1X);
	if (ret < 0)
		return ret;

	/* set LED currents to maximum */
	switch (si114x_leds(data)) {
	case 3:
		ret = i2c_smbus_write_byte_data(client,
			SI114X_REG_PS_LED3, 0x0f);
		if (ret < 0)
			return ret;
		ret = i2c_smbus_write_byte_data(client,
			SI114X_REG_PS_LED21, 0xff);
		break;
	case 2:
		ret = i2c_smbus_write_byte_data(client,
			SI114X_REG_PS_LED21, 0xff);
		break;
	case 1:
		ret = i2c_smbus_write_byte_data(client,
			SI114X_REG_PS_LED21, 0x0f);
		break;
	}
	if (ret < 0)
		return ret;

	ret = si114x_set_chlist(indio_dev, true);
	if (ret < 0)
		return ret;

	/* set normal proximity measurement mode, set high signal range
	 * PS measurement */
	ret = si114x_param_op(data, SI114X_CMD_PARAM_SET,
		SI114X_PARAM_PS_ADC_MISC, 0x20 | 0x04);
	if (ret < 0)
		return ret;

	return 0;
}

static ssize_t si114x_read_frequency(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct si114x_data *data = iio_priv(indio_dev);
	int ret;
	u16 rate;

	ret = i2c_smbus_read_byte_data(data->client, SI114X_REG_MEAS_RATE);
	if (ret < 0)
		return ret;

	if (ret == 0)
		rate = 0;
	else
		rate = 32000 / si114x_uncompress(ret);

	return sprintf(buf, "%d\n", rate);
}

static ssize_t si114x_write_frequency(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct si114x_data *data = iio_priv(indio_dev);
	unsigned long val;
	int ret;
	u8 rate;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	switch (val) {
	case 250:
	case 100:
	case 50:
	case 25:
	case 10:
	case 5:
	case 2:
	case 1:
		rate = si114x_compress(32000 / val);
		break;
	case 0:
		rate = 0;
		break;
	default:
		return -EINVAL;
	}

	ret = i2c_smbus_write_byte_data(data->client, SI114X_REG_MEAS_RATE,
		rate);

	return ret ? ret : len;
}

/* sysfs attributes if IRQ available */
static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
			      si114x_read_frequency,
			      si114x_write_frequency);

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("1 2 5 10 25 50 100 250");

static struct attribute *si114x_attrs_trigger[] = {
	&iio_dev_attr_in_proximity_range.dev_attr.attr,
	&iio_const_attr_in_proximity_range_available.dev_attr.attr,
	&iio_dev_attr_in_illuminance_range.dev_attr.attr,
	&iio_const_attr_in_illuminance_range_available.dev_attr.attr,
	&iio_dev_attr_in_illuminance_ir_range.dev_attr.attr,
	&iio_const_attr_in_illuminance_ir_range_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL
};

static struct attribute_group si114x_attr_group_trigger = {
	.attrs = si114x_attrs_trigger
};

static const struct iio_info si114x_info_trigger = {
	.read_raw = si114x_read_raw,
	.write_raw = si114x_write_raw,
#if defined(CONFIG_DEBUG_FS)
	.debugfs_reg_access = si114x_reg_access,
#endif
	.driver_module = THIS_MODULE,
	.attrs = &si114x_attr_group_trigger
};

/* sysfs attributes if no IRQ available */
static struct attribute *si114x_attrs_no_trigger[] = {
	&iio_dev_attr_in_proximity_range.dev_attr.attr,
	&iio_const_attr_in_proximity_range_available.dev_attr.attr,
	&iio_dev_attr_in_illuminance_range.dev_attr.attr,
	&iio_const_attr_in_illuminance_range_available.dev_attr.attr,
	&iio_dev_attr_in_illuminance_ir_range.dev_attr.attr,
	&iio_const_attr_in_illuminance_ir_range_available.dev_attr.attr,
	NULL
};

static struct attribute_group si114x_attr_group_no_trigger = {
	.attrs = si114x_attrs_no_trigger
};

static const struct iio_info si114x_info_no_trigger = {
	.read_raw = si114x_read_raw,
	.write_raw = si114x_write_raw,
#if defined(CONFIG_DEBUG_FS)
	.debugfs_reg_access = si114x_reg_access,
#endif
	.driver_module = THIS_MODULE,
	.attrs = &si114x_attr_group_no_trigger
};

static int si114x_buffer_preenable(struct iio_dev *indio_dev)
{
	struct iio_buffer *buffer = indio_dev->buffer;

	/* at least one channel besides the timestamp must be enabled */
	if (!bitmap_weight(buffer->scan_mask, indio_dev->masklength))
		return -EINVAL;

	return 0;
}

static int si114x_buffer_postenable(struct iio_dev *indio_dev)
{
	int ret;

	ret = iio_triggered_buffer_postenable(indio_dev);
	if (ret < 0)
		return ret;

	/* measure only enabled channels */
	ret = si114x_set_chlist(indio_dev, false);
	if (ret < 0)
		return ret;

	return 0;
}

static int si114x_buffer_predisable(struct iio_dev *indio_dev)
{
	int ret;

	/* measure all channels */
	ret = si114x_set_chlist(indio_dev, true);
	if (ret < 0)
		return ret;

	return iio_triggered_buffer_predisable(indio_dev);
}

static const struct iio_buffer_setup_ops si114x_buffer_setup_ops = {
	.preenable = si114x_buffer_preenable,
	.postenable = si114x_buffer_postenable,
	.predisable = si114x_buffer_predisable,
};

static int si114x_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct si114x_data *data;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = iio_device_alloc(sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	data->client = client;
	data->use_irq = client->irq != 0;

	ret = si114x_revisions(data);
	if (ret < 0)
		goto error_free_dev;

	dev_info(&client->dev, "Si11%02x Ambient light/proximity sensor, Seq: %02x\n",
		data->part, data->seq);

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = SI114X_DRV_NAME;
	indio_dev->channels = si114x_channels;
	/* Compute number of channels: use of last six channels depends on
	 * the number of proximity LEDs supported (determined by the part
	 * number: si1141, si1142, si114x).
	 * Each proximity LED has an input intensity and output voltage
	 * channel.
	 */
	indio_dev->num_channels = ARRAY_SIZE(si114x_channels) -
		6 + si114x_leds(data)*2;
	indio_dev->info = data->use_irq ?
		&si114x_info_trigger : &si114x_info_no_trigger;
	indio_dev->modes = INDIO_DIRECT_MODE;

	mutex_init(&data->mutex);
	init_waitqueue_head(&data->data_avail);

	ret = si114x_initialize(indio_dev);
	if (ret < 0)
		goto error_free_dev;

	ret = iio_triggered_buffer_setup(indio_dev, NULL,
		si114x_trigger_handler, &si114x_buffer_setup_ops);
	if (ret < 0)
		goto error_free_dev;

	if (data->use_irq) {
		ret = request_threaded_irq(client->irq,
			   NULL, si114x_irq,
			   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			   "si114x_irq", indio_dev);
		if (ret < 0) {
			dev_err(&client->dev, "irq request failed\n");
			goto error_free_buffer;
		}

		ret = si114x_probe_trigger(indio_dev);
		if (ret < 0)
			goto error_free_irq;
	} else
		dev_info(&client->dev, "no irq, using polling\n");

	ret = iio_device_register(indio_dev);
	if (ret < 0)
		goto error_free_trigger;

	return 0;

error_free_trigger:
	if (data->use_irq)
		si114x_remove_trigger(indio_dev);
error_free_irq:
	if (data->use_irq)
		free_irq(client->irq, indio_dev);
error_free_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
error_free_dev:
	iio_device_free(indio_dev);
	return ret;
}

static const struct i2c_device_id si114x_id[] = {
	{ "si114x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, si114x_id);

static int si114x_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct si114x_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	if (data->use_irq) {
		si114x_remove_trigger(indio_dev);
		free_irq(client->irq, indio_dev);
	}
	iio_device_free(indio_dev);

	return 0;
}

static struct i2c_driver si114x_driver = {
	.driver = {
		.name   = SI114X_DRV_NAME,
		.owner  = THIS_MODULE,
	},
	.probe  = si114x_probe,
	.remove = si114x_remove,
	.id_table = si114x_id,
};

module_i2c_driver(si114x_driver);

MODULE_AUTHOR("Peter Meerwald <pmeerw@pmeerw.net>");
MODULE_DESCRIPTION("Silabs si114x proximity/ambient light sensor driver");
MODULE_LICENSE("GPL");
