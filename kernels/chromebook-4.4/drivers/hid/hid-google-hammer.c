/*
 *  HID driver for Google Hammer device.
 *
 *  Copyright (c) 2017 Google Inc.
 *  Author: Wei-Ning Huang <wnhuang@google.com>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "hid-ids.h"
#include "hid-google-hammer.h"

#define MAX_BRIGHTNESS 100

struct hammer_kbd_leds {
	struct led_classdev cdev;
	struct hid_device *hdev;
	u8 buf[2] ____cacheline_aligned;
};

static int hammer_kbd_brightness_set_blocking(struct led_classdev *cdev,
		enum led_brightness br)
{
	struct hammer_kbd_leds *led = container_of(cdev,
						   struct hammer_kbd_leds,
						   cdev);
	int ret;

	led->buf[0] = 0;
	led->buf[1] = br;

	/*
	 * Request USB HID device to be in Full On mode,
	 * So that sending hardware output report and hardware
	 * raw request won't fail.
	 */
	ret = hid_hw_power(led->hdev, PM_HINT_FULLON);
	if (ret < 0) {
		hid_err(led->hdev, "failed: device not resumed %d\n",
			ret);
		return ret;
	}

	ret = hid_hw_output_report(led->hdev, led->buf, sizeof(led->buf));
	if (ret == -ENOSYS)
		ret = hid_hw_raw_request(led->hdev, 0, led->buf,
					 sizeof(led->buf),
					 HID_OUTPUT_REPORT,
					 HID_REQ_SET_REPORT);
	if (ret < 0)
		hid_err(led->hdev, "failed to set keyboard backlight: %d\n",
			ret);
	/* Request USB HID device back to Normal Mode. */
	hid_hw_power(led->hdev, PM_HINT_NORMAL);

	return ret;
}

static int hammer_register_leds(struct hid_device *hdev)
{
	struct hammer_kbd_leds *kbd_backlight;

	kbd_backlight = devm_kzalloc(&hdev->dev,
				     sizeof(*kbd_backlight),
				     GFP_KERNEL);
	if (!kbd_backlight)
		return -ENOMEM;

	kbd_backlight->hdev = hdev;
	kbd_backlight->cdev.name = "hammer::kbd_backlight";
	kbd_backlight->cdev.max_brightness = MAX_BRIGHTNESS;
	kbd_backlight->cdev.brightness_set_blocking =
		hammer_kbd_brightness_set_blocking;
	kbd_backlight->cdev.flags = LED_HW_PLUGGABLE;

	/* Set backlight to 0% initially. */
	hammer_kbd_brightness_set_blocking(&kbd_backlight->cdev, 0);

	return devm_led_classdev_register(&hdev->dev, &kbd_backlight->cdev);
}

int hammer_input_configured(struct hid_device *hdev, struct hid_input *hi)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);

	struct list_head *report_list =
		&hdev->report_enum[HID_OUTPUT_REPORT].report_list;

	if (intf->cur_altsetting->desc.bInterfaceProtocol ==
	    USB_INTERFACE_PROTOCOL_KEYBOARD && !list_empty(report_list)) {
		int err = hammer_register_leds(hdev);
		if (err)
			hid_warn(hdev,
				 "Failed to register keyboard backlight: %d\n",
				 err);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hammer_input_configured);

static const struct hid_device_id hammer_devices[] = {
	{ HID_DEVICE(BUS_USB, HID_GROUP_GENERIC_OVERRIDE,
		     USB_VENDOR_ID_GOOGLE, USB_DEVICE_ID_GOOGLE_HAMMER) },
	{ HID_DEVICE(BUS_USB, HID_GROUP_GENERIC_OVERRIDE,
		     USB_VENDOR_ID_GOOGLE, USB_DEVICE_ID_GOOGLE_STAFF) },
	{ HID_DEVICE(BUS_USB, HID_GROUP_GENERIC_OVERRIDE,
		     USB_VENDOR_ID_GOOGLE, USB_DEVICE_ID_GOOGLE_WAND) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hammer_devices);

static struct hid_driver hammer_driver = {
	.name = "hammer",
	.id_table = hammer_devices,
	.input_configured = hammer_input_configured,
};
module_hid_driver(hammer_driver);

MODULE_LICENSE("GPL");
