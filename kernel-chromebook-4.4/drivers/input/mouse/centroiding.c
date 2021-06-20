/*
 * centroiding.c
 *
 * Google Centroiding Touchpad Driver
 *
 * Copyright 2017 Google Inc.
 */

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>


#define DRIVER_NAME		"centroiding"
#define DRIVER_VERSION		"0.0.1"
#define GOOGLE_VENDOR_ID	0x18d1
#define GOOGLE_CENTROIDING_PRODUCT_ID	0x5028

#define CENTROIDING_MAX_FINGERS 10
#define CENTROIDING_MAX_X	1920
#define CENTROIDING_MAX_Y	1080
#define CENTROIDING_X_RES	16
#define CENTROIDING_Y_RES	16
#define CENTROIDING_MAX_PRESSURE	255
#define CENTROIDING_MAX_TRACKING_ID	0xffff


struct centroiding_data {
	struct i2c_client	*client;
	struct input_dev	*input;
};

struct point_data {
	__le16 id;
	__le16 x, y;
	u8 amp;
} __packed;

struct report_data {
	u8 button_down;
	u8 contact_num;
	struct point_data data[CENTROIDING_MAX_FINGERS];
} __packed;


static struct report_data report;

static irqreturn_t centroiding_isr(int irq, void *dev_id)
{
	struct centroiding_data *data = dev_id;
	struct i2c_client *client = data->client;
	struct input_dev *input = data->input;
	struct device *dev = &data->client->dev;
	int ret;
	int i;

	ret = i2c_master_recv(client, (char *)&report, sizeof(report));
	if (ret != sizeof(report)) {
		dev_err(dev, "err %d: failed to read packet from i2c\n", ret);
		return IRQ_HANDLED;
	}

	for (i = 0; i < CENTROIDING_MAX_FINGERS; i++) {
		struct point_data *point = &report.data[i];

		if ((s16)le16_to_cpu(point->id) != -1) {
			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
			input_report_abs(input, ABS_MT_POSITION_X,
					 le16_to_cpu(point->x));
			input_report_abs(input, ABS_MT_POSITION_Y,
					 le16_to_cpu(point->y));
			input_report_abs(input, ABS_MT_PRESSURE,
					 point->amp);
		} else {
			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER,
						   false);
		}
	}

	input_report_key(input, BTN_LEFT, report.button_down);
	input_sync(input);

	return IRQ_HANDLED;
}

static int centroiding_setup_input_devices(struct centroiding_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input;
	int error;

	input = devm_input_allocate_device(dev);
	if (!input)
		return -ENOMEM;

	input->name = "Google Touchpad";
	input->id.bustype = BUS_I2C;
	input->id.vendor = GOOGLE_VENDOR_ID;
	input->id.product = GOOGLE_CENTROIDING_PRODUCT_ID;
	input_set_drvdata(input, data);

	__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);
	input_set_capability(input, EV_KEY, BTN_LEFT);

	/* Set up MT parameters */
	input_set_abs_params(input, ABS_MT_POSITION_X, 0,
			     CENTROIDING_MAX_X, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0,
			     CENTROIDING_MAX_Y, 0, 0);
	input_abs_set_res(input, ABS_MT_POSITION_X, CENTROIDING_X_RES);
	input_abs_set_res(input, ABS_MT_POSITION_Y, CENTROIDING_Y_RES);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0,
			     CENTROIDING_MAX_PRESSURE, 0, 0);

	error = input_mt_init_slots(input, CENTROIDING_MAX_FINGERS,
				    INPUT_MT_POINTER);
	if (error) {
		dev_err(dev, "failed to initialize MT slots: %d\n", error);
		return error;
	}

	data->input = input;

	return 0;
}

static int centroiding_probe(struct i2c_client *client,
			     const struct i2c_device_id *dev_id)
{
	struct centroiding_data *data;
	int error;

	data = devm_kzalloc(&client->dev, sizeof(struct centroiding_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	data->client = client;

	centroiding_setup_input_devices(data);

	error = input_register_device(data->input);
	if (error) {
		dev_err(&client->dev, "failed to register input device: %d\n",
			error);
		return error;
	}

	error = devm_request_threaded_irq(&client->dev, client->irq,
					  NULL, centroiding_isr,
					  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					  client->name, data);
	if (error) {
		dev_err(&client->dev, "cannot register irq=%d\n", client->irq);
		return error;
	}

	return 0;
}


static const struct i2c_device_id centroiding_id[] = {
	{ DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, centroiding_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id centroiding_acpi_id[] = {
	{ "GOOG5400", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, centroiding_acpi_id);
#endif


static struct i2c_driver centroiding_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.acpi_match_table = ACPI_PTR(centroiding_acpi_id),
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe		= centroiding_probe,
	.id_table	= centroiding_id,
};

module_i2c_driver(centroiding_driver);

MODULE_AUTHOR("Wei-Ning Huang <wnhuang@google.com>");
MODULE_DESCRIPTION("Google Centroiding Touchpad Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
