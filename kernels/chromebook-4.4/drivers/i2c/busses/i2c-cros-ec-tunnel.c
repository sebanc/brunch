/*
 *  Copyright (C) 2013 Google, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * Expose an I2C passthrough to the ChromeOS EC.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/cros_ec.h>
#include <linux/mfd/cros_ec_commands.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define DRV_NAME "cros-ec-i2c-tunnel"

#define I2C_MAX_RETRIES 3

/**
 * struct ec_i2c_device - Driver data for I2C tunnel
 *
 * @dev: Device node
 * @adap: I2C adapter
 * @ec: Pointer to EC device
 * @remote_bus: The EC bus number we tunnel to on the other side.
 * @request_buf: Buffer for transmitting data; we expect most transfers to fit.
 * @response_buf: Buffer for receiving data; we expect most transfers to fit.
 */

struct ec_i2c_device {
	struct device *dev;
	struct i2c_adapter adap;
	struct cros_ec_device *ec;

	u16 remote_bus;

	u8 request_buf[256];
	u8 response_buf[256];
};

#define CHECK_I2C_WR(num, length) \
	(((msgs[num].flags & I2C_M_RD) == 0) && (msgs[num].len == length))

#define CHECK_I2C_RD(num, length) \
	((msgs[num].flags & I2C_M_RD) && (msgs[num].len == length))

/* Standard I2C address for smart batteries */
#define SBS_I2C_ADDR 0xB

static int ec_i2c_forward_msg(struct ec_i2c_device *bus, int cmd,
			      struct i2c_msg *outmsg, struct i2c_msg *inmsg)
{
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(inmsg->len, outmsg->len), GFP_KERNEL);

	msg->command = cmd;
	msg->outsize = outmsg ? outmsg->len : 0;
	msg->insize = inmsg ? inmsg->len : 0;

	if (outmsg)
		memcpy(msg->data, outmsg->buf, outmsg->len);

	ret = cros_ec_cmd_xfer_status(bus->ec, msg);
	if (ret >= 0 && inmsg)
		memcpy(inmsg->buf, msg->data, inmsg->len);
	return ret;
}

static int ec_i2c_limited_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			       int num)
{
	struct ec_i2c_device *bus = adap->algo_data;

	if (!num || (msgs[0].addr != SBS_I2C_ADDR))
		return -ENODEV;

	/* Battery device probing */
	if ((num == 1) && (msgs[0].len == 0)) {
		uint8_t dummy[] = { 0x0d, 0 };
		struct i2c_msg otmp = { .buf = dummy, .len = 1 };
		struct i2c_msg itmp = { .buf = dummy, .len = 2 };
		return ec_i2c_forward_msg(bus, EC_CMD_SB_READ_WORD,
					  &otmp, &itmp);
	}
	/* Read a word-sized register */
	if ((num == 1) && CHECK_I2C_WR(0, 3))
		return ec_i2c_forward_msg(bus, EC_CMD_SB_WRITE_WORD,
					  &msgs[0], NULL);
	/* Write a word-sized register */
	if ((num == 2) && CHECK_I2C_WR(0, 1) && CHECK_I2C_RD(1, 2))
		return ec_i2c_forward_msg(bus, EC_CMD_SB_READ_WORD,
					  &msgs[0], &msgs[1]);
	/* Retrieve string data length */
	if ((num == 2) && CHECK_I2C_WR(0, 1) && CHECK_I2C_RD(1, 1)) {
		msgs[1].buf[0] = I2C_SMBUS_BLOCK_MAX;
		return 0;
	}
	/* Read string data */
	if ((num == 2) && CHECK_I2C_WR(0, 1) &&
			  CHECK_I2C_RD(1, I2C_SMBUS_BLOCK_MAX)) {
		char tmpblock[I2C_SMBUS_BLOCK_MAX + 1];
		struct i2c_msg tmpmsg = { .buf = tmpblock,
					  .len = I2C_SMBUS_BLOCK_MAX };
		int ret;
		ret = ec_i2c_forward_msg(bus, EC_CMD_SB_READ_BLOCK,
					 &msgs[0], &tmpmsg);
		tmpblock[I2C_SMBUS_BLOCK_MAX] = 0;
		/* real string length */
		msgs[1].buf[0] = strlen(tmpblock);
		strlcpy(&msgs[1].buf[1], tmpblock, msgs[1].len);
		return ret;
	}

	return -EIO;
}

/**
 * ec_i2c_count_message - Count bytes needed for ec_i2c_construct_message
 *
 * @i2c_msgs: The i2c messages to read
 * @num: The number of i2c messages.
 *
 * Returns the number of bytes the messages will take up.
 */
static int ec_i2c_count_message(const struct i2c_msg i2c_msgs[], int num)
{
	int i;
	int size;

	size = sizeof(struct ec_params_i2c_passthru);
	size += num * sizeof(struct ec_params_i2c_passthru_msg);
	for (i = 0; i < num; i++)
		if (!(i2c_msgs[i].flags & I2C_M_RD))
			size += i2c_msgs[i].len;

	return size;
}

/**
 * ec_i2c_construct_message - construct a message to go to the EC
 *
 * This function effectively stuffs the standard i2c_msg format of Linux into
 * a format that the EC understands.
 *
 * @buf: The buffer to fill.  We assume that the buffer is big enough.
 * @i2c_msgs: The i2c messages to read.
 * @num: The number of i2c messages.
 * @bus_num: The remote bus number we want to talk to.
 *
 * Returns 0 or a negative error number.
 */
static int ec_i2c_construct_message(u8 *buf, const struct i2c_msg i2c_msgs[],
				    int num, u16 bus_num)
{
	struct ec_params_i2c_passthru *params;
	u8 *out_data;
	int i;

	out_data = buf + sizeof(struct ec_params_i2c_passthru) +
		   num * sizeof(struct ec_params_i2c_passthru_msg);

	params = (struct ec_params_i2c_passthru *)buf;
	params->port = bus_num;
	params->num_msgs = num;
	for (i = 0; i < num; i++) {
		const struct i2c_msg *i2c_msg = &i2c_msgs[i];
		struct ec_params_i2c_passthru_msg *msg = &params->msg[i];

		msg->len = i2c_msg->len;
		msg->addr_flags = i2c_msg->addr;

		if (i2c_msg->flags & I2C_M_TEN)
			return -EINVAL;

		if (i2c_msg->flags & I2C_M_RD) {
			msg->addr_flags |= EC_I2C_FLAG_READ;
		} else {
			memcpy(out_data, i2c_msg->buf, msg->len);
			out_data += msg->len;
		}
	}

	return 0;
}

/**
 * ec_i2c_count_response - Count bytes needed for ec_i2c_parse_response
 *
 * @i2c_msgs: The i2c messages to to fill up.
 * @num: The number of i2c messages expected.
 *
 * Returns the number of response bytes expeced.
 */
static int ec_i2c_count_response(struct i2c_msg i2c_msgs[], int num)
{
	int size;
	int i;

	size = sizeof(struct ec_response_i2c_passthru);
	for (i = 0; i < num; i++)
		if (i2c_msgs[i].flags & I2C_M_RD)
			size += i2c_msgs[i].len;

	return size;
}

/**
 * ec_i2c_parse_response - Parse a response from the EC
 *
 * We'll take the EC's response and copy it back into msgs.
 *
 * @buf: The buffer to parse.
 * @i2c_msgs: The i2c messages to to fill up.
 * @num: The number of i2c messages; will be modified to include the actual
 *	 number received.
 *
 * Returns 0 or a negative error number.
 */
static int ec_i2c_parse_response(const u8 *buf, struct i2c_msg i2c_msgs[],
				 int *num)
{
	const struct ec_response_i2c_passthru *resp;
	const u8 *in_data;
	int i;

	in_data = buf + sizeof(struct ec_response_i2c_passthru);

	resp = (const struct ec_response_i2c_passthru *)buf;
	if (resp->i2c_status & EC_I2C_STATUS_TIMEOUT)
		return -ETIMEDOUT;
	else if (resp->i2c_status & EC_I2C_STATUS_NAK)
		return -ENXIO;
	else if (resp->i2c_status & EC_I2C_STATUS_ERROR)
		return -EIO;

	/* Other side could send us back fewer messages, but not more */
	if (resp->num_msgs > *num)
		return -EPROTO;
	*num = resp->num_msgs;

	for (i = 0; i < *num; i++) {
		struct i2c_msg *i2c_msg = &i2c_msgs[i];

		if (i2c_msgs[i].flags & I2C_M_RD) {
			memcpy(i2c_msg->buf, in_data, i2c_msg->len);
			in_data += i2c_msg->len;
		}
	}

	return 0;
}

static int ec_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg i2c_msgs[],
		       int num)
{
	struct ec_i2c_device *bus = adap->algo_data;
	struct device *dev = bus->dev;
	const u16 bus_num = bus->remote_bus;
	int request_len;
	int response_len;
	int alloc_size;
	int result;
	struct cros_ec_command *msg;

	request_len = ec_i2c_count_message(i2c_msgs, num);
	if (request_len < 0) {
		dev_warn(dev, "Error constructing message %d\n", request_len);
		return request_len;
	}

	response_len = ec_i2c_count_response(i2c_msgs, num);
	if (response_len < 0) {
		/* Unexpected; no errors should come when NULL response */
		dev_warn(dev, "Error preparing response %d\n", response_len);
		return response_len;
	}

	alloc_size = max(request_len, response_len);
	msg = kmalloc(sizeof(*msg) + alloc_size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	result = ec_i2c_construct_message(msg->data, i2c_msgs, num, bus_num);
	if (result) {
		dev_err(dev, "Error constructing EC i2c message %d\n", result);
		goto exit;
	}

	msg->version = 0;
	msg->command = EC_CMD_I2C_PASSTHRU;
	msg->outsize = request_len;
	msg->insize = response_len;

	result = cros_ec_cmd_xfer_status(bus->ec, msg);
	if (result < 0) {
		dev_err(dev, "Error transferring EC i2c message %d\n", result);
		goto exit;
	}

	result = ec_i2c_parse_response(msg->data, i2c_msgs, &num);
	if (result < 0)
		goto exit;

	/* Indicate success by saying how many messages were sent */
	result = num;
exit:
	kfree(msg);
	return result;
}

static u32 ec_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm ec_i2c_limited_algorithm = {
	.master_xfer	= ec_i2c_limited_xfer,
	.functionality	= ec_i2c_functionality,
};

static const struct i2c_algorithm ec_i2c_algorithm = {
	.master_xfer	= ec_i2c_xfer,
	.functionality	= ec_i2c_functionality,
};

static int ec_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct cros_ec_device *ec = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct ec_i2c_device *bus = NULL;
	u32 remote_bus;
	int err;

	if (!ec->cmd_xfer) {
		dev_err(dev, "Missing sendrecv\n");
		return -EINVAL;
	}

	bus = devm_kzalloc(dev, sizeof(*bus), GFP_KERNEL);
	if (bus == NULL)
		return -ENOMEM;

	err = of_property_read_u32(np, "google,remote-bus", &remote_bus);
	if (err) {
		dev_err(dev, "Couldn't read remote-bus property\n");
		return err;
	}
	bus->remote_bus = remote_bus;

	bus->ec = ec;
	bus->dev = dev;

	bus->adap.owner = THIS_MODULE;
	strlcpy(bus->adap.name, DRV_NAME, sizeof(bus->adap.name));
	bus->adap.algo = &ec_i2c_algorithm;
	bus->adap.algo_data = bus;
	bus->adap.dev.parent = &pdev->dev;
	bus->adap.dev.of_node = np;
	bus->adap.retries = I2C_MAX_RETRIES;

	if (of_find_property(np, "google,limited-passthrough", NULL))
		bus->adap.algo = &ec_i2c_limited_algorithm;

	err = i2c_add_adapter(&bus->adap);
	if (err) {
		dev_err(dev, "cannot register i2c adapter\n");
		return err;
	}
	platform_set_drvdata(pdev, bus);

	return err;
}

static int ec_i2c_remove(struct platform_device *dev)
{
	struct ec_i2c_device *bus = platform_get_drvdata(dev);

	i2c_del_adapter(&bus->adap);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cros_ec_i2c_of_match[] = {
	{ .compatible = "google," DRV_NAME },
	{},
};
MODULE_DEVICE_TABLE(of, cros_ec_i2c_of_match);
#endif

static struct platform_driver ec_i2c_tunnel_driver = {
	.probe = ec_i2c_probe,
	.remove = ec_i2c_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(cros_ec_i2c_of_match),
	},
};

static int __init ec_i2c_init(void)
{
	return platform_driver_register(&ec_i2c_tunnel_driver);
}
subsys_initcall(ec_i2c_init);

static void __exit ec_i2c_exit(void)
{
	platform_driver_unregister(&ec_i2c_tunnel_driver);
}
module_exit(ec_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EC I2C tunnel driver");
MODULE_ALIAS("platform:" DRV_NAME);
