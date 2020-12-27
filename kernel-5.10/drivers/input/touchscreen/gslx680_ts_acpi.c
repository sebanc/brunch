/*
 * Silead GSL1680/3680 touchscreen driver
 *
 * Copyright (c) 2015 Gregor Riepl <onitake@gmail.com>
 *
 * This driver is based on gslx680_ts.c and elan_ts.c
 * Copyright (c) 2012 Shanghai Basewin
 *   Guan Yuwei<guanyuwei@basewin.com>
 * Copyright (c) 2013 Joe Burmeister
 *   Joe Burmeister<joe.a.burmeister@googlemail.com>
 * Copyright (C) 2014 Elan Microelectronics Corporation.
 * Scott Liu <scott.liu@emc.com.tw>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/input/mt.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/version.h>

/* Device and driver information */
#define DEVICE_NAME "gslx680"
#define DRIVER_VERSION "0.2.1"

/* Hardware API constants */
#define GSL_DATA_REG 0x80
#define GSL_STATUS_REG 0xe0
#define GSL_PAGE_REG 0xf0
#define GSL_TOUCH_STATUS_REG 0xbc
#define GSL_PAGE_SIZE 128
/* Why 126? */
#define GSL_MAX_READ 126
/* Actually 129: 1 command byte and 128 data bytes
 * this was 125 originally - using 128 to allow firmware transfers
 */
#define GSL_MAX_WRITE 128
#define GSL_STATUS_FW 0x80
#define GSL_STATUS_TOUCH 0x00

#define GSL_PWR_GPIO "power"

#define GSL_MAX_CONTACTS 10
#define GSL_MAX_AXIS 0xfff
#define GSL_DMAX 0
#define GSL_JITTER 0
#define GSL_DEADZONE 0

#define GSL_PACKET_SIZE ( \
	GSL_MAX_CONTACTS * sizeof(struct gsl_ts_packet_touch) + \
	sizeof(struct gsl_ts_packet_header) \
)

#define GSL_FW_NAME_MAXLEN 32
#define GSL_FW_NAME_DEFAULT "silead_ts.fw"
#define GSL_FW_VERSION 1
#define GSL_FW_ID_LIT(a, b, c, d) ( \
	((a) & 0xff) | \
	(((b) & 0xff) << 8) | \
	(((c) & 0xff) << 16) | \
	(((d) & 0xff) << 24) \
)
#define GSL_FW_MAGIC GSL_FW_ID_LIT('G', 'S', 'L', 'X')
#define GSL_FW_MODEL_LIT(m) GSL_FW_ID_LIT( \
	(m)/1000%10+'0', \
	(m)/100%10+'0', \
	(m)/10%10+'0', \
	(m)%10+'0' \
)
#define GSL_FW_MODEL_1680 GSL_FW_MODEL_LIT(1680)

/* Module Parameters (optional) */
static char *gsl_fw_name = NULL;
module_param_named(fw_name, gsl_fw_name, charp, 0);

/* Driver state */
enum gsl_ts_state {
	GSL_TS_INIT,
	GSL_TS_SHUTDOWN,
	GSL_TS_GREEN,
};

/* Driver instance data structure */
struct gsl_ts_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct gpio_desc *gpio;
	char fw_name[GSL_FW_NAME_MAXLEN];
	
	enum gsl_ts_state state;

	bool wake_irq_enabled;

	unsigned int x_max;
	unsigned int y_max;
	unsigned int multi_touches;
	bool x_reversed;
	bool y_reversed;
	bool xy_swapped;
	bool soft_tracking;
	int jitter;
	int deadzone;
};

/* Firmware header */
struct gsl_ts_fw_header {
	u32 magic;
	u32 model;
	u16 version;
	u16 touches;
	u16 width;
	u16 height;
	u8 swapped;
	u8 xflipped;
	u8 yflipped;
	u8 tracking;
	u32 pages;
} __packed;
/* Firmware data page */
struct gsl_ts_fw_page {
	u16 address;
	u16 size;
	u8 data[128];
} __packed;

/* TODO use get_unaligned_le16 instead of packed structures */
/* Hardware touch event data header */
struct gsl_ts_packet_header {
	u8 num_fingers;
	u8 reserved;
	u16 time_stamp; /* little-endian */
} __packed;
/* Hardware touch event data per finger */
struct gsl_ts_packet_touch {
	u16 y_z; /* little endian, lower 12 bits = y, upper 4 bits = pressure (?) */
	u16 x_id; /* little endian, lower 12 bits = x, upper 4 bits = id */
} __packed;


static int gsl_ts_init(struct gsl_ts_data *ts, const struct firmware *fw)
{
	struct gsl_ts_fw_header *header;
	u32 magic;
	u16 version;

	dev_dbg(&ts->client->dev, "%s: initialising driver state\n", __func__);

	ts->wake_irq_enabled = false;
	ts->state = GSL_TS_INIT;

	if (fw->size < sizeof(struct gsl_ts_fw_header)) {
		dev_err(&ts->client->dev, "%s: invalid firmware: file too small.\n", __func__);
		return -EINVAL;
	}

	header = (struct gsl_ts_fw_header *) fw->data;
	magic = le32_to_cpu(header->magic);
	if (magic != GSL_FW_MAGIC) {
		dev_err(&ts->client->dev, "%s: invalid firmware: invalid magic 0x%08x.\n", __func__, magic);
		return -EINVAL;
	}

	version = le16_to_cpu(header->version);
	if (version != GSL_FW_VERSION) {
		dev_err(&ts->client->dev, "%s: invalid firmware: unsupported version %d.\n", __func__, version);
		return -EINVAL;
	}

	ts->x_max = le16_to_cpu(header->width);
	ts->y_max = le16_to_cpu(header->height);
	ts->multi_touches = le16_to_cpu(header->touches);
	if (ts->x_max == 0 || ts->y_max == 0 || ts->multi_touches == 0) {
		dev_err(&ts->client->dev, "%s: invalid firmware: panel width, height or number of touch points is zero.\n", __func__);
		return -EINVAL;
	}
	if (ts->x_max > GSL_MAX_AXIS || ts->y_max > GSL_MAX_AXIS || ts->multi_touches > GSL_MAX_CONTACTS) {
		dev_err(&ts->client->dev, "%s: invalid firmware: maximum panel width, height or number of touch points exceeded.\n", __func__);
		return -EINVAL;
	}

	ts->x_reversed = header->xflipped ? 1 : 0;
	ts->y_reversed = header->yflipped ? 1 : 0;
	ts->xy_swapped = header->swapped ? 1 : 0;
	ts->soft_tracking = header->tracking ? 1 : 0;
	ts->jitter = GSL_JITTER;
	ts->deadzone = GSL_DEADZONE;

	return 0;
}

static int gsl_ts_write(struct i2c_client *client, u8 reg, const u8 *pdata, int datalen)
{
	u8 buf[GSL_PAGE_SIZE + 1];
	unsigned int bytelen = 0;

	if (datalen > GSL_MAX_WRITE) {
		dev_err(&client->dev, "%s: data transfer too large (%d > %d)\n", __func__, datalen, GSL_MAX_WRITE);
		return -EINVAL;
	}

	buf[0] = reg;
	bytelen++;

	if (datalen != 0 && pdata != NULL) {
		memcpy(&buf[bytelen], pdata, datalen);
		bytelen += datalen;
	}

	return i2c_master_send(client, buf, bytelen);
}

static int gsl_ts_read(struct i2c_client *client, u8 reg, u8 *pdata, unsigned int datalen)
{
	int ret = 0;

	if (datalen > GSL_MAX_READ) {
		dev_err(&client->dev, "%s: data transfer too large (%d > %d)\n", __func__, datalen, GSL_MAX_READ);
		return -EINVAL;
	}

	ret = gsl_ts_write(client, reg, NULL, 0);
	if (ret < 0) {
		dev_err(&client->dev, "%s: sending register location failed\n", __func__);
		return ret;
	}

	return i2c_master_recv(client, pdata, datalen);
}

static int gsl_ts_startup_chip(struct i2c_client *client)
{
	int rc;
	u8 tmp = 0x00;

	dev_dbg(&client->dev, "%s: starting up\n", __func__);
	rc = gsl_ts_write(client, 0xe0, &tmp, 1);
	usleep_range(10000, 20000);

	return rc;
}

static int gsl_ts_reset_chip(struct i2c_client *client)
{
	int rc;
	u8 arg[4] = { 0x00, 0x00, 0x00, 0x00 };

	dev_dbg(&client->dev, "%s: resetting\n", __func__);

	arg[0] = 0x88;
	rc = gsl_ts_write(client, 0xe0, arg, 1);
	if (rc < 0) {
		dev_err(&client->dev, "%s: gsl_ts_write 1 fail!\n", __func__);
		return rc;
	}
	usleep_range(10000, 20000);

	arg[0] = 0x04;
	rc = gsl_ts_write(client, 0xe4, arg, 1);
	if (rc < 0) {
		dev_err(&client->dev, "%s: gsl_ts_write 2 fail!\n", __func__);
		return rc;
	}
	usleep_range(10000, 20000);

	arg[0] = 0x00;
	rc = gsl_ts_write(client, 0xbc, arg, 4);
	if (rc < 0) {
		dev_err(&client->dev, "%s: gsl_ts_write 3 fail!\n", __func__);
		return rc;
	}
	usleep_range(10000, 20000);

	return 0;
}

static int gsl_ts_write_fw(struct gsl_ts_data *ts, const struct firmware *fw)
{
	int rc = 0;
	struct i2c_client *client = ts->client;
	const struct gsl_ts_fw_header *header;
	const struct gsl_ts_fw_page *page;
	u32 pages, address;
	u16 size;
	size_t i;

	dev_dbg(&client->dev, "%s: sending firmware\n", __func__);

	header = (const struct gsl_ts_fw_header *) fw->data;
	pages = le32_to_cpu(header->pages);
	if (fw->size < sizeof(struct gsl_ts_fw_header) + pages * sizeof(struct gsl_ts_fw_page)) {
		dev_err(&client->dev, "%s: firmware page data too small.\n", __func__);
		return -EINVAL;
	}

	for (i = 0; rc >= 0 && i < pages; i++) {
		page = (const struct gsl_ts_fw_page *) &fw->data[sizeof(struct gsl_ts_fw_header) + i * sizeof(struct gsl_ts_fw_page)];
		/* The controller expects a little endian address */
		address = cpu_to_le32(le16_to_cpu(page->address));
		size = le16_to_cpu(page->size);
		rc = gsl_ts_write(client, GSL_PAGE_REG, (u8 *) &address, sizeof(address));
		if (rc < 0) {
			dev_err(&client->dev, "%s: error setting page register. (page = 0x%x)\n", __func__, le32_to_cpu(address));
		} else {
			rc = gsl_ts_write(client, 0, page->data, size);
			if (rc < 0) {
				dev_err(&client->dev, "%s: error writing page data. (page = 0x%x)\n", __func__, le32_to_cpu(address));
			}
		}
	}

	if (rc < 0) {
		return rc;
	}
	return 0;
}

static void gsl_ts_mt_event(struct gsl_ts_data *ts, u8 *buf)
{
	int rc;
	struct input_dev *input = ts->input;
	struct device *dev = &ts->client->dev;
	struct gsl_ts_packet_header *header;
	struct gsl_ts_packet_touch *touch;
	u8 i;
	u16 touches, tseq, x, y, id, pressure;
	struct input_mt_pos positions[GSL_MAX_CONTACTS];
	int slots[GSL_MAX_CONTACTS];

	header = (struct gsl_ts_packet_header *) buf;
	touches = header->num_fingers;
	tseq = le16_to_cpu(header->time_stamp);
	/* time_stamp is 0 on zero-touch events, seems to wrap around 21800 */
	dev_vdbg(dev, "%s: got touch events for %u fingers @%u\n", __func__, touches, tseq);

	if (touches > GSL_MAX_CONTACTS) {
		touches = GSL_MAX_CONTACTS;
	}

	for (i = 0; i < touches; i++) {
		touch = (struct gsl_ts_packet_touch *) &buf[sizeof(*header) + i * sizeof(*touch)];
		y = le16_to_cpu(touch->y_z);
		x = le16_to_cpu(touch->x_id);
		id = x >> 12;
		x &= 0xfff;
		pressure = y >> 12;
		y &= 0xfff;

		if (ts->xy_swapped) {
			swap(x, y);
		}
		if (ts->x_reversed) {
			x = ts->x_max - x;
		}
		if (ts->y_reversed) {
			y = ts->y_max - y;
		}

		dev_vdbg(dev, "%s: touch event %u: x=%u y=%u id=0x%x p=%u\n", __func__, i, x, y, id, pressure);

		positions[i].x = x;
		positions[i].y = y;
		if (!ts->soft_tracking) {
			slots[i] = id;
		}
	}
	if (ts->soft_tracking) {
		/* This platform does not support finger tracking.
		 * Use the input core finger tracker instead.
		 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
		rc = input_mt_assign_slots(input, slots, positions, touches);
#else
		rc = input_mt_assign_slots(input, slots, positions, touches, GSL_DMAX);
#endif
		if (rc < 0) {
			dev_err(dev, "%s: input_mt_assign_slots returned %d\n", __func__, rc);
			return;
		}
	}

	for (i = 0; i < touches; i++) {
		input_mt_slot(input, slots[i]);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
		input_report_abs(input, ABS_MT_POSITION_X, positions[i].x);
		input_report_abs(input, ABS_MT_POSITION_Y, positions[i].y);
	}

	input_mt_sync_frame(input);
	input_sync(input);
}

static irqreturn_t gsl_ts_irq(int irq, void *arg)
{
	int rc;
	struct gsl_ts_data *ts = (struct gsl_ts_data *) arg;
	struct i2c_client *client = ts->client;
	struct device *dev = &client->dev;
	u8 status[4] = { 0, 0, 0, 0 };
	u8 event[GSL_PACKET_SIZE];

	dev_dbg(&client->dev, "%s: IRQ received\n", __func__);

	if (ts->state == GSL_TS_SHUTDOWN) {
		dev_warn(&client->dev, "%s: device supended, not handling interrupt\n", __func__);
		return IRQ_HANDLED;
	}

	rc = gsl_ts_read(client, GSL_STATUS_REG, status, sizeof(status));
	if (rc < 0) {
		dev_err(dev, "%s: error reading chip status\n", __func__);
		return IRQ_HANDLED;
	}

	if (status[0] == GSL_STATUS_FW) {
		/* TODO: Send firmware here instead of during init */
		dev_info(dev, "%s: device waiting for firmware\n", __func__);

	} else if (status[0] == GSL_STATUS_TOUCH) {
		dev_vdbg(dev, "%s: touch event\n", __func__);

		rc = gsl_ts_read(client, GSL_DATA_REG, event, sizeof(event));
		if (rc < 0) {
			dev_err(dev, "%s: touch data read failed\n", __func__);
			return IRQ_HANDLED;
		}
		if (event[0] == 0xff) {
			dev_warn(dev, "%s: ignoring invalid touch record (0xff)\n", __func__);
			return IRQ_HANDLED;
		}

		rc = gsl_ts_read(client, GSL_TOUCH_STATUS_REG, status, sizeof(status));
		if (rc < 0) {
			dev_err(dev, "%s: reading touch status register failed\n", __func__);
			return IRQ_HANDLED;
		}

		if ((status[0] | status[1] | status[2] | status[3]) == 0) {
			gsl_ts_mt_event(ts, event);

		} else {
			dev_warn(dev, "%s: device seems to be stuck, resetting\n", __func__);

			rc = gsl_ts_reset_chip(ts->client);
			if (rc < 0) {
				dev_err(dev, "%s: reset_chip failed\n", __func__);
				return IRQ_HANDLED;
			}
			rc = gsl_ts_startup_chip(ts->client);
			if (rc < 0) {
				dev_err(dev, "%s: startup_chip failed\n", __func__);
				return IRQ_HANDLED;
			}
		}
	} else {
		dev_warn(&client->dev, "%s: IRQ received, unknown status 0x%02x\n", __func__, status[0]);
	}

	return IRQ_HANDLED;
}

static void gsl_ts_power(struct i2c_client *client, bool turnoff)
{
	struct gsl_ts_data *data = i2c_get_clientdata(client);
#ifdef CONFIG_ACPI
	int error;
#endif

	if (data) {
		if (data->gpio) {
			gpiod_set_value_cansleep(data->gpio, turnoff ? 0 : 1);
#ifdef CONFIG_ACPI
		} else {
			error = acpi_bus_set_power(ACPI_HANDLE(&client->dev), turnoff ? ACPI_STATE_D3 : ACPI_STATE_D0);
			if (error) {
				dev_warn(&client->dev, "%s: error changing power state: %d\n", __func__, error);
			}
#endif
		}
		usleep_range(20000, 50000);
	}
}

static int gsl_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct gsl_ts_data *ts;
	const struct firmware *fw = NULL;
	unsigned long irqflags;
	int error;
	bool acpipower;

	dev_warn(&client->dev, "%s: got a device named %s at address 0x%x, IRQ %d, flags 0x%x\n", __func__, client->name, client->addr, client->irq, client->flags);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c check functionality error\n", __func__);
		error = -ENXIO;
		goto release;
	}

	if (client->irq <= 0) {
		dev_err(&client->dev, "%s: missing IRQ configuration\n", __func__);
		error = -ENODEV;
		goto release;
	}
	
	ts = devm_kzalloc(&client->dev, sizeof(struct gsl_ts_data), GFP_KERNEL);
	if (!ts) {
		error = -ENOMEM;
		goto release;
	}
	
	ts->client = client;
	i2c_set_clientdata(client, ts);
	
	if (gsl_fw_name != NULL) {
		strncpy(ts->fw_name, gsl_fw_name, sizeof(ts->fw_name));
	} else {
		strncpy(ts->fw_name, GSL_FW_NAME_DEFAULT, sizeof(ts->fw_name));
	}
	error = request_firmware(&fw, ts->fw_name, &ts->client->dev);
	if (error < 0) {
		dev_err(&client->dev, "%s: failed to load firmware: %d\n", __func__, error);
		goto release;
	}

	error = gsl_ts_init(ts, fw);
	if (error < 0) {
		dev_err(&client->dev, "%s: failed to initialize: %d\n", __func__, error);
		goto release;
	}

	ts->input = devm_input_allocate_device(&client->dev);
	if (!ts->input) {
		dev_err(&client->dev, "%s: failed to allocate input device\n", __func__);
		error = -ENOMEM;
		goto release;
	}

	ts->input->name = "Silead GSLx680 Touchscreen";
	ts->input->id.bustype = BUS_I2C;
	ts->input->phys = "input/ts";

	input_set_capability(ts->input, EV_ABS, ABS_X);
	input_set_capability(ts->input, EV_ABS, ABS_Y);

	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->x_max, ts->jitter, ts->deadzone);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->y_max, ts->jitter, ts->deadzone);

	input_mt_init_slots(ts->input, ts->multi_touches, INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED | INPUT_MT_TRACK);

	input_set_drvdata(ts->input, ts);

	error = input_register_device(ts->input);
	if (error) {
		dev_err(&client->dev, "%s: unable to register input device: %d\n", __func__, error);
		goto release;
	}

	/* Try to use ACPI power methods first */
	acpipower = false;
#ifdef CONFIG_ACPI
	if (ACPI_COMPANION(&client->dev)) {
		/* Wake the device up with a power on reset */
		if (acpi_bus_set_power(ACPI_HANDLE(&client->dev), ACPI_STATE_D3)) {
			dev_warn(&client->dev, "%s: failed to wake up device through ACPI: %d, using GPIO controls instead\n", __func__, error);
		} else {
			acpipower = true;
		}
	}
#endif
	/* Not available, use GPIO settings from DSDT/DT instead */
	if (!acpipower) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
		ts->gpio = devm_gpiod_get_index(&client->dev, GSL_PWR_GPIO, 0);
#else
		ts->gpio = devm_gpiod_get_index(&client->dev, GSL_PWR_GPIO, 0, GPIOD_OUT_LOW);
#endif
		if (IS_ERR(ts->gpio)) {
			dev_err(&client->dev, "%s: error obtaining power pin GPIO resource\n", __func__);
			error = PTR_ERR(ts->gpio);
			goto release;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
		error = gpiod_direction_output(ts->gpio, 0);
		if (error < 0) {
			dev_err(&client->dev, "%s: error setting GPIO pin direction\n", __func__);
			goto release;
		}
#endif
	} else {
		ts->gpio = NULL;
	}

	/* Enable power */
	gsl_ts_power(client, false);

	/* Execute the controller startup sequence */
	error = gsl_ts_reset_chip(client);
	if (error < 0) {
		dev_err(&client->dev, "%s: chip reset failed\n", __func__);
		goto release;
	}
	error = gsl_ts_write_fw(ts, fw);
	if (error < 0) {
		dev_err(&client->dev, "%s: firmware transfer failed\n", __func__);
		goto release;
	}
	error = gsl_ts_startup_chip(client);
	if (error < 0) {
		dev_err(&client->dev, "%s: chip startup failed\n", __func__);
		goto release;
	}

	/*
	 * Systems using device tree should set up interrupt via DTS,
	 * the rest will use the default falling edge interrupts.
	 */
	irqflags = client->dev.of_node ? 0 : IRQF_TRIGGER_FALLING;

	/* Set up interrupt handler - do we still need to account for shared interrupts? */
	error = devm_request_threaded_irq(
		&client->dev,
		client->irq,
		NULL,
		gsl_ts_irq,
		irqflags | IRQF_ONESHOT,
		client->name,
		ts
	);
	if (error) {
		dev_err(&client->dev, "%s: failed to register interrupt\n", __func__);
		goto release;
	}

	/*
	 * Systems using device tree should set up wakeup via DTS,
	 * the rest will configure device as wakeup source by default.
	 */
	if (!client->dev.of_node) {
		device_init_wakeup(&client->dev, true);
	}

	ts->state = GSL_TS_GREEN;

release:
	if (fw) {
		release_firmware(fw);
	}

	if (error < 0) {
		return error;
	}
	return 0;
}

int gsl_ts_remove(struct i2c_client *client) {
	/* Power the device off */
	gsl_ts_power(client, true);
	return 0;
}

static int __maybe_unused gsl_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gsl_ts_data *ts = i2c_get_clientdata(client);

	dev_warn(&client->dev, "%s: suspending device\n", __func__);

	disable_irq(client->irq);

	gsl_ts_reset_chip(client);
	usleep_range(10000, 20000);

	gsl_ts_power(client, true);

	if (device_may_wakeup(dev)) {
		ts->wake_irq_enabled = (enable_irq_wake(client->irq) == 0);
	}

	ts->state = GSL_TS_SHUTDOWN;

	return 0;
}

static int __maybe_unused gsl_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gsl_ts_data *ts = i2c_get_clientdata(client);

	dev_warn(&client->dev, "%s: resuming device\n", __func__);

	if (device_may_wakeup(dev) && ts->wake_irq_enabled) {
		disable_irq_wake(client->irq);
	}

	gsl_ts_power(client, false);

	gsl_ts_reset_chip(client);
	gsl_ts_startup_chip(client);

	enable_irq(client->irq);

	ts->state = GSL_TS_GREEN;

	return 0;
}

static const struct i2c_device_id gsl_ts_i2c_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gsl_ts_i2c_id);

#ifdef CONFIG_PM
static SIMPLE_DEV_PM_OPS(gsl_ts_pm_ops, gsl_ts_suspend, gsl_ts_resume);
#endif

#ifdef CONFIG_ACPI
/* GSL3680 ACPI IDs are untested */
static const struct acpi_device_id gsl_ts_acpi_match[] = {
	{ "MSSL1680", 0 },
	{ "MSSL3680", 0 },
	{ "PNP1680", 0 },
	{ "PNP3680", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, gsl_ts_acpi_match);
#endif

#ifdef CONFIG_OF
/* These should work, but more testing is needed */
static const struct of_device_id gsl_ts_of_match[] = {
	{ .compatible = "silead,gsl1680" },
	{ .compatible = "silead,gsl3680" },
	{ .compatible = "silead,gslx680" },
	{ }
};
MODULE_DEVICE_TABLE(of, gsl_ts_of_match);
#endif

static struct i2c_driver gslx680_ts_driver = {
	.probe = gsl_ts_probe,
	.remove = gsl_ts_remove,
	.id_table = gsl_ts_i2c_id,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &gsl_ts_pm_ops,
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(gsl_ts_acpi_match),
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(gsl_ts_of_match),
#endif
	},
};
module_i2c_driver(gslx680_ts_driver);

MODULE_DESCRIPTION("GSLX680 touchscreen controller driver");
MODULE_AUTHOR("Gregor Riepl <onitake@gmail.com>");
MODULE_PARM_DESC(fw_name, "firmware file name (default: " GSL_FW_NAME_DEFAULT ")");
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
