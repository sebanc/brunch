// SPDX-License-Identifier: GPL-2.0
/*
 * Keyboard backlight LED driver for the Wilco Embedded Controller
 *
 * Copyright 2019 Google LLC
 *
 * The EC is in charge of controlling the keyboard backlight on
 * the Wilco platform. We expose a standard LED class device at
 * /sys/class/leds/chromeos::kbd_backlight. Power Manager normally
 * controls the backlight by writing a percentage in range [0, 100]
 * to the brightness property. This driver is modeled after the
 * standard Chrome OS keyboard backlight driver at
 * drivers/platform/chrome/cros_kbd_led_backlight.c
 *
 * Some Wilco devices do not support a keyboard backlight. This
 * is checked via wilco_ec_keyboard_backlight_exists() in the core driver,
 * and a platform_device will only be registered by the core if
 * a backlight is supported.
 *
 * After an EC reset the backlight could be in a non-PWM mode.
 * Earlier in the boot sequence the BIOS should send a command to
 * the EC to set the brightness, so things **should** be set up,
 * but we double check in probe() as we query the initial brightness.
 * If not set up, then we set the brightness to KBBL_DEFAULT_BRIGHTNESS.
 *
 * Since the EC will never change the backlight level of its own accord,
 * we don't need to implement a brightness_get() method.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_data/wilco-ec.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define DRV_NAME		"wilco-kbd-backlight"

#define KBBL_DEFAULT_BRIGHTNESS	0

struct wilco_keyboard_led_data {
	struct wilco_ec_device *ec;
	struct led_classdev keyboard;
};

/* Send a request, get a response, and check that the response is good. */
static int send_kbbl_msg(struct wilco_ec_device *ec,
			 struct wilco_ec_kbbl_msg *request,
			 struct wilco_ec_kbbl_msg *response)
{
	struct wilco_ec_message msg;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.type = WILCO_EC_MSG_LEGACY;
	msg.request_data = request;
	msg.request_size = sizeof(*request);
	msg.response_data = response;
	msg.response_size = sizeof(*response);

	ret = wilco_ec_mailbox(ec, &msg);
	if (ret < 0) {
		dev_err(ec->dev, "Failed sending brightness command: %d", ret);
		return ret;
	}

	if (response->status) {
		dev_err(ec->dev,
			"EC reported failure sending brightness command: %d",
			response->status);
		return -EIO;
	}

	return 0;
}

/* This may sleep because it uses wilco_ec_mailbox() */
static int keyboard_led_set_brightness(struct led_classdev *cdev,
				       enum led_brightness brightness)
{
	struct wilco_ec_kbbl_msg request;
	struct wilco_ec_kbbl_msg response;
	struct wilco_keyboard_led_data *data;

	memset(&request, 0, sizeof(request));
	request.command = WILCO_EC_COMMAND_KBBL;
	request.subcmd = WILCO_KBBL_SUBCMD_SET_STATE;
	request.mode = WILCO_KBBL_MODE_FLAG_PWM;
	request.percent = brightness;

	data = container_of(cdev, struct wilco_keyboard_led_data, keyboard);
	return send_kbbl_msg(data->ec, &request, &response);
}

/*
 * Get the current brightness, ensuring that we are in PWM mode. If not
 * in PWM mode, then the current brightness is meaningless, so set the
 * brightness to KBBL_DEFAULT_BRIGHTNESS.
 *
 * Return: Final brightness of the keyboard, or negative error code on failure.
 */
static int initialize_brightness(struct wilco_keyboard_led_data *data)
{
	struct wilco_ec_kbbl_msg request;
	struct wilco_ec_kbbl_msg response;
	int ret;

	memset(&request, 0, sizeof(request));
	request.command = WILCO_EC_COMMAND_KBBL;
	request.subcmd = WILCO_KBBL_SUBCMD_GET_STATE;

	ret = send_kbbl_msg(data->ec, &request, &response);
	if (ret < 0)
		return ret;

	if (response.mode & WILCO_KBBL_MODE_FLAG_PWM)
		return response.percent;

	dev_warn(data->ec->dev, "Keyboard brightness not initialized by BIOS");
	ret = keyboard_led_set_brightness(&data->keyboard,
					  KBBL_DEFAULT_BRIGHTNESS);
	if (ret < 0)
		return ret;

	return KBBL_DEFAULT_BRIGHTNESS;
}

static int keyboard_led_probe(struct platform_device *pdev)
{
	struct wilco_ec_device *ec = dev_get_drvdata(pdev->dev.parent);
	struct wilco_keyboard_led_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->ec = ec;
	/* This acts the same as the CrOS backlight, so use the same name */
	data->keyboard.name = "chromeos::kbd_backlight";
	data->keyboard.max_brightness = 100;
	data->keyboard.flags = LED_CORE_SUSPENDRESUME;
	data->keyboard.brightness_set_blocking = keyboard_led_set_brightness;
	ret = initialize_brightness(data);
	if (ret < 0)
		return ret;
	data->keyboard.brightness = ret;

	return devm_led_classdev_register(&pdev->dev, &data->keyboard);
}

static struct platform_driver keyboard_led_driver = {
	.driver = {
		.name = DRV_NAME,
	},
	.probe = keyboard_led_probe,
};
module_platform_driver(keyboard_led_driver);

MODULE_AUTHOR("Nick Crews <ncrews@chromium.org>");
MODULE_DESCRIPTION("Wilco keyboard backlight LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
