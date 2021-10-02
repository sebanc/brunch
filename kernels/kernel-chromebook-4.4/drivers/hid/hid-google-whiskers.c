// SPDX-License-Identifier: GPL-2.0+
//
// HID driver for Google Whiskers device.
//
// Copyright (c) 2018 Google Inc.

#include <linux/acpi.h>
#include <linux/hid.h>
#include <linux/leds.h>
#include <linux/mfd/cros_ec.h>
#include <linux/mfd/cros_ec_commands.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/usb.h>
#include <asm/unaligned.h>

#include "hid-ids.h"
#include "hid-google-hammer.h"

#define HID_UP_GOOGLEVENDOR	0xffd10000
#define HID_VD_KBD_FOLDED	0x00000019
#define WHISKERS_KBD_FOLDED	(HID_UP_GOOGLEVENDOR | HID_VD_KBD_FOLDED)

struct whiskers_ec {
	struct device *dev;	/* The platform device (EC) */
	struct input_dev *input;
	bool base_present;
	bool base_folded;	/* false: not folded or unknown */
	struct notifier_block notifier;
};

static struct whiskers_ec whiskers_ec;
static DEFINE_SPINLOCK(whiskers_ec_lock);
static DEFINE_MUTEX(whiskers_ec_reglock);

static bool whiskers_parse_base_state(const void *data)
{
	u32 switches = get_unaligned_le32(data);

	return !!(switches & BIT(EC_MKBP_BASE_ATTACHED));
}

static int whiskers_ec_query_base(struct cros_ec_device *ec_dev, bool get_state,
				  bool *state)
{
	struct ec_params_mkbp_info *params;
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(sizeof(u32), sizeof(*params)),
		      GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->command = EC_CMD_MKBP_INFO;
	msg->version = 1;
	msg->outsize = sizeof(*params);
	msg->insize = sizeof(u32);
	params = (struct ec_params_mkbp_info *)msg->data;
	params->info_type = get_state ?
		EC_MKBP_INFO_CURRENT : EC_MKBP_INFO_SUPPORTED;
	params->event_type = EC_MKBP_EVENT_SWITCH;

	ret = cros_ec_cmd_xfer_status(ec_dev, msg);
	if (ret >= 0) {
		if (ret != sizeof(u32)) {
			dev_warn(ec_dev->dev, "wrong result size: %d != %zu\n",
				 ret, sizeof(u32));
			ret = -EPROTO;
		} else {
			*state = whiskers_parse_base_state(msg->data);
			ret = 0;
		}
	}

	kfree(msg);

	return ret;
}

static int whiskers_ec_notify(struct notifier_block *nb,
			      unsigned long queued_during_suspend,
			      void *_notify)
{
	struct cros_ec_device *ec = _notify;
	unsigned long flags;
	bool base_present;

	if (ec->event_data.event_type == EC_MKBP_EVENT_SWITCH) {
		base_present = whiskers_parse_base_state(
					&ec->event_data.data.switches);
		dev_dbg(whiskers_ec.dev,
			"%s: base: %d\n", __func__, base_present);

		if (device_may_wakeup(whiskers_ec.dev) ||
		    !queued_during_suspend) {

			pm_wakeup_event(whiskers_ec.dev, 0);

			spin_lock_irqsave(&whiskers_ec_lock, flags);

			/*
			 * While input layer dedupes the events, we do not want
			 * to disrupt the state reported by the base by
			 * overriding it with state reported by the LID. Only
			 * report changes, as we assume that on attach the base
			 * is not folded.
			 */
			if (base_present != whiskers_ec.base_present) {
				input_report_switch(whiskers_ec.input,
						    SW_TABLET_MODE,
						    !base_present);
				input_sync(whiskers_ec.input);
				whiskers_ec.base_present = base_present;
			}

			spin_unlock_irqrestore(&whiskers_ec_lock, flags);
		}
	}

	return NOTIFY_OK;
}

static __maybe_unused int whiskers_ec_resume(struct device *dev)
{
	struct cros_ec_device *ec = dev_get_drvdata(dev->parent);
	bool base_present;
	int error;

	error = whiskers_ec_query_base(ec, true, &base_present);
	if (error) {
		dev_warn(dev, "failed to fetch base state on resume: %d\n",
			 error);
	} else {
		spin_lock_irq(&whiskers_ec_lock);

		whiskers_ec.base_present = base_present;

		/*
		 * Only report if base is disconnected. If base is connected,
		 * it will resend its state on resume, and we'll update it
		 * in whiskers_event().
		 */
		if (!whiskers_ec.base_present) {
			input_report_switch(whiskers_ec.input,
					    SW_TABLET_MODE, 1);
			input_sync(whiskers_ec.input);
		}

		spin_unlock_irq(&whiskers_ec_lock);
	}

	return 0;
}

static const SIMPLE_DEV_PM_OPS(whiskers_ec_pm_ops, NULL, whiskers_ec_resume);

static void whiskers_ec_set_input(struct input_dev *input)
{
	/* Take the lock so whiskers_event does not race with us here */
	spin_lock_irq(&whiskers_ec_lock);
	whiskers_ec.input = input;
	spin_unlock_irq(&whiskers_ec_lock);
}

static int __whiskers_ec_probe(struct platform_device *pdev)
{
	struct cros_ec_device *ec = dev_get_drvdata(pdev->dev.parent);
	struct input_dev *input;
	bool base_supported;
	int error;

	error = whiskers_ec_query_base(ec, false, &base_supported);
	if (error)
		return error;

	if (!base_supported)
		return -ENXIO;

	input = devm_input_allocate_device(&pdev->dev);
	if (!input)
		return -ENOMEM;

	input->name = "Whiskers Tablet Mode Switch";

	input->id.bustype = BUS_HOST;
	input->id.version = 1;
	input->id.product = 0;

	input_set_capability(input, EV_SW, SW_TABLET_MODE);

	error = input_register_device(input);
	if (error) {
		dev_err(&pdev->dev, "cannot register input device: %d\n",
			error);
		return error;
	}

	/* Seed the state */
	error = whiskers_ec_query_base(ec, true, &whiskers_ec.base_present);
	if (error) {
		dev_err(&pdev->dev, "cannot query base state: %d\n", error);
		return error;
	}

	if (!whiskers_ec.base_present)
		whiskers_ec.base_folded = false;

	dev_dbg(&pdev->dev, "%s: base: %d, folded: %d\n", __func__,
		whiskers_ec.base_present, whiskers_ec.base_folded);

	input_report_switch(input, SW_TABLET_MODE,
			    !whiskers_ec.base_present ||
			    whiskers_ec.base_folded);

	whiskers_ec_set_input(input);

	whiskers_ec.dev = &pdev->dev;
	whiskers_ec.notifier.notifier_call = whiskers_ec_notify;
	error = blocking_notifier_chain_register(&ec->event_notifier,
						 &whiskers_ec.notifier);
	if (error) {
		dev_err(&pdev->dev, "cannot register notifier: %d\n", error);
		whiskers_ec_set_input(NULL);
		return error;
	}

	device_init_wakeup(&pdev->dev, true);
	return 0;
}

static int whiskers_ec_probe(struct platform_device *pdev)
{
	int retval;

	mutex_lock(&whiskers_ec_reglock);

	if (whiskers_ec.input) {
		retval = -EBUSY;
		goto out;
	}

	retval = __whiskers_ec_probe(pdev);

out:
	mutex_unlock(&whiskers_ec_reglock);
	return retval;
}

static int whiskers_ec_remove(struct platform_device *pdev)
{
	struct cros_ec_device *ec = dev_get_drvdata(pdev->dev.parent);

	mutex_lock(&whiskers_ec_reglock);

	blocking_notifier_chain_unregister(&ec->event_notifier,
					   &whiskers_ec.notifier);
	whiskers_ec_set_input(NULL);

	mutex_unlock(&whiskers_ec_reglock);
	return 0;
}

static const struct acpi_device_id whiskers_ec_acpi_ids[] = {
	{ "GOOG000B", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, whiskers_ec_acpi_ids);

static struct platform_driver whiskers_ec_driver = {
	.probe	= whiskers_ec_probe,
	.remove	= whiskers_ec_remove,
	.driver	= {
		.name			= "whiskers_ec",
		.acpi_match_table	= ACPI_PTR(whiskers_ec_acpi_ids),
		.pm			= &whiskers_ec_pm_ops,
	},
};

static int whiskers_input_mapping(struct hid_device *hdev, struct hid_input *hi,
				  struct hid_field *field,
				  struct hid_usage *usage,
				  unsigned long **bit, int *max)
{
	if (usage->hid == WHISKERS_KBD_FOLDED) {
		/*
		 * We do not want to have this usage mapped as it will get
		 * mixed in with "base attached" signal and delivered over
		 * separate input device for tablet switch mode.
		 */

		/*
		 * Also, override open/close and inhibit/uninhibit from
		 * hid-input.c as we can't power down the interface since
		 * we need to get signal when base is unfolded. So when
		 * device is inhibited we simply drop events in input core.
		 * This code would be better placed in input_configured()
		 * method, but since we reusing hammer's implementation we
		 * do it here instead.
		 */
		hi->input->open = NULL;
		hi->input->close = NULL;
		hi->input->inhibit = NULL;
		hi->input->uninhibit = NULL;

		return -1;
	}

	return 0;
}

static int whiskers_event(struct hid_device *hid, struct hid_field *field,
			  struct hid_usage *usage, __s32 value)
{
	unsigned long flags;

	if (usage->hid == WHISKERS_KBD_FOLDED) {
		spin_lock_irqsave(&whiskers_ec_lock, flags);

		/*
		 * If we are getting events from Whiskers that means that it
		 * is attached to the lid.
		 */
		whiskers_ec.base_present = true;
		whiskers_ec.base_folded = value;
		hid_dbg(hid, "%s: base: %d, folded: %d\n", __func__,
			whiskers_ec.base_present, whiskers_ec.base_folded);

		if (whiskers_ec.input) {
			input_report_switch(whiskers_ec.input,
					    SW_TABLET_MODE, value);
			input_sync(whiskers_ec.input);
		}

		spin_unlock_irqrestore(&whiskers_ec_lock, flags);
		return 1; /* We handled this event */
	}

	return 0;
}

static int whiskers_probe(struct hid_device *hdev,
			  const struct hid_device_id *id)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	int error;

	error = hid_parse(hdev);
	if (error)
		return error;

	error = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (error)
		return error;

	/*
	 * We always want to poll for, and handle tablet mode events, even when
	 * nobody has opened the input device. This also prevents the hid core
	 * from dropping early tablet mode events from the device.
	 */
	if (intf->cur_altsetting->desc.bInterfaceProtocol ==
			USB_INTERFACE_PROTOCOL_KEYBOARD) {
		hdev->quirks |= HID_QUIRK_ALWAYS_POLL;
		error = hid_hw_open(hdev);
		if (error)
			return error;
	}

	return 0;
}

static void whiskers_remove(struct hid_device *hdev)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	unsigned long flags;

	if (intf->cur_altsetting->desc.bInterfaceProtocol ==
			USB_INTERFACE_PROTOCOL_KEYBOARD) {
		hid_hw_close(hdev);

		/*
		 * If we are disconnecting then most likely Whiskers is
		 * being removed. Even if it is not removed, without proper
		 * keyboard we should not stay in clamshell mode.
		 *
		 * The reason for doing it here and not waiting for signal
		 * from EC, is that on some devices there are high leakage
		 * on Whiskers pins and we do not detect disconnect reliably,
		 * resulting in devices being stuck in clamshell mode.
		 */
		spin_lock_irqsave(&whiskers_ec_lock, flags);
		if (whiskers_ec.input && whiskers_ec.base_present) {
			input_report_switch(whiskers_ec.input,
					    SW_TABLET_MODE, 1);
			input_sync(whiskers_ec.input);
		}
		whiskers_ec.base_present = false;
		spin_unlock_irqrestore(&whiskers_ec_lock, flags);
	}

	hid_hw_stop(hdev);
}

static const struct hid_device_id whiskers_hid_devices[] = {
	{ HID_DEVICE(BUS_USB, HID_GROUP_GENERIC_OVERRIDE,
		     USB_VENDOR_ID_GOOGLE, USB_DEVICE_ID_GOOGLE_WHISKERS) },
	{ }
};
MODULE_DEVICE_TABLE(hid, whiskers_hid_devices);

static struct hid_driver whiskers_hid_driver = {
	.name			= "whiskers",
	.id_table		= whiskers_hid_devices,
	.probe			= whiskers_probe,
	.remove			= whiskers_remove,
	.input_configured	= hammer_input_configured,
	.input_mapping		= whiskers_input_mapping,
	.event			= whiskers_event,
};

static int __init whiskers_init(void)
{
	int error;

	error = platform_driver_register(&whiskers_ec_driver);
	if (error)
		return error;

	error = hid_register_driver(&whiskers_hid_driver);
	if (error) {
		platform_driver_unregister(&whiskers_ec_driver);
		return error;
	}

	return 0;
}
module_init(whiskers_init);

static void __exit whiskers_exit(void)
{
	hid_unregister_driver(&whiskers_hid_driver);
	platform_driver_unregister(&whiskers_ec_driver);
}
module_exit(whiskers_exit);

MODULE_LICENSE("GPL");
