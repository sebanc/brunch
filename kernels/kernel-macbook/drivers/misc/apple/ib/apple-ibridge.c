// SPDX-License-Identifier: GPL-2.0
/*
 * Apple iBridge Driver
 *
 * Copyright (c) 2018 Ronald Tschalär
 */

/**
 * DOC: Overview
 *
 * MacBookPro models with a Touch Bar (13,[23] and 14,[23]) have an Apple
 * iBridge chip (also known as T1 chip) which exposes the touch bar,
 * built-in webcam (iSight), ambient light sensor, and Secure Enclave
 * Processor (SEP) for TouchID. It shows up in the system as a USB device
 * with 3 configurations: 'Default iBridge Interfaces', 'Default iBridge
 * Interfaces(OS X)', and 'Default iBridge Interfaces(Recovery)'. While
 * the second one is used by MacOS to provide the fancy touch bar
 * functionality with custom buttons etc, this driver just uses the first.
 *
 * In the first (default after boot) configuration, 4 usb interfaces are
 * exposed: 2 related to the webcam, and 2 USB HID interfaces representing
 * the touch bar and the ambient light sensor. The webcam interfaces are
 * already handled by the uvcvideo driver; furthermore, the handling of the
 * input reports when "keys" on the touch bar are pressed is already handled
 * properly by the generic USB HID core. This leaves the management of the
 * touch bar modes (e.g. switching between function and special keys when the
 * FN key is pressed), the touch bar display (dimming and turning off), the
 * key-remapping when the FN key is pressed, and handling of the light sensor.
 *
 * This driver is implemented as a HID driver that creates virtual HID devices
 * for the ALS and touch bar functionality, and the ALS and touch bar drivers
 * are HID drivers which in turn attach to these virtual HID devices. This
 * driver then relays the calls on the real HID devices to the virtual ones,
 * and visa versa.
 *
 * Lastly, this driver also takes care of the power-management for the
 * iBridge when suspending and resuming.
 */

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/version.h>

#include "apple-ibridge.h"

#ifdef UPSTREAM
#include "../hid/usbhid/usbhid.h"
#else
#define	hid_to_usb_dev(hid_dev) \
	to_usb_device((hid_dev)->dev.parent->parent)
#endif

#define USB_ID_VENDOR_APPLE	0x05ac
#define USB_ID_PRODUCT_IBRIDGE	0x8600

#define APPLEIB_BASIC_CONFIG	1

#define	LOG_DEV(ib_dev)		(&(ib_dev)->acpi_dev->dev)

static struct hid_device_id appleib_sub_hid_ids[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_LINUX_FOUNDATION,
			 USB_DEVICE_ID_IBRIDGE_TB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_LINUX_FOUNDATION,
			 USB_DEVICE_ID_IBRIDGE_ALS) },
};

struct appleib_device {
	struct acpi_device	*acpi_dev;
	acpi_handle		asoc_socw;
};

struct appleib_hid_dev_info {
	struct hid_device	*hdev;
	struct hid_device	*sub_hdevs[ARRAY_SIZE(appleib_sub_hid_ids)];
};

/**
 * appleib_find_report_field() - Find the field in the report with the given
 * usage.
 * @report: the report to search
 * @field_usage: the usage of the field to search for
 *
 * Returns: the hid field if found, or NULL if none found.
 */
struct hid_field *appleib_find_report_field(struct hid_report *report,
					    unsigned int field_usage)
{
	int f, u;

	for (f = 0; f < report->maxfield; f++) {
		struct hid_field *field = report->field[f];

		if (field->logical == field_usage)
			return field;

		for (u = 0; u < field->maxusage; u++) {
			if (field->usage[u].hid == field_usage)
				return field;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(appleib_find_report_field);

/**
 * appleib_find_hid_field() - Search all the reports of the device for the
 * field with the given usage.
 * @hdev: the device whose reports to search
 * @application: the usage of application collection that the field must
 *               belong to
 * @field_usage: the usage of the field to search for
 *
 * Returns: the hid field if found, or NULL if none found.
 */
struct hid_field *appleib_find_hid_field(struct hid_device *hdev,
					 unsigned int application,
					 unsigned int field_usage)
{
	static const int report_types[] = { HID_INPUT_REPORT, HID_OUTPUT_REPORT,
					    HID_FEATURE_REPORT };
	struct hid_report *report;
	struct hid_field *field;
	int t;

	for (t = 0; t < ARRAY_SIZE(report_types); t++) {
		struct list_head *report_list =
			    &hdev->report_enum[report_types[t]].report_list;
		list_for_each_entry(report, report_list, list) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
			if (report->application != application)
				continue;
#endif

			field = appleib_find_report_field(report, field_usage);
			if (field)
				return field;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(appleib_find_hid_field);

static int appleib_hid_raw_event(struct hid_device *hdev,
				 struct hid_report *report, u8 *data, int size)
{
	struct appleib_hid_dev_info *hdev_info = hid_get_drvdata(hdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(hdev_info->sub_hdevs); i++)
		hid_input_report(hdev_info->sub_hdevs[i], report->type, data,
				 size, 0);

	return 0;
}

static __u8 *appleib_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				  unsigned int *rsize)
{
	/* Some fields have a size of 64 bits, which according to HID 1.11
	 * Section 8.4 is not valid ("An item field cannot span more than 4
	 * bytes in a report"). Furthermore, hid_field_extract() complains
	 * when encountering such a field. So turn them into two 32-bit fields
	 * instead.
	 */

	if (*rsize == 634 &&
	    /* Usage Page 0xff12 (vendor defined) */
	    rdesc[212] == 0x06 && rdesc[213] == 0x12 && rdesc[214] == 0xff &&
	    /* Usage 0x51 */
	    rdesc[416] == 0x09 && rdesc[417] == 0x51 &&
	    /* report size 64 */
	    rdesc[432] == 0x75 && rdesc[433] == 64 &&
	    /* report count 1 */
	    rdesc[434] == 0x95 && rdesc[435] == 1) {
		rdesc[433] = 32;
		rdesc[435] = 2;
		hid_dbg(hdev, "Fixed up first 64-bit field\n");
	}

	if (*rsize == 634 &&
	    /* Usage Page 0xff12 (vendor defined) */
	    rdesc[212] == 0x06 && rdesc[213] == 0x12 && rdesc[214] == 0xff &&
	    /* Usage 0x51 */
	    rdesc[611] == 0x09 && rdesc[612] == 0x51 &&
	    /* report size 64 */
	    rdesc[627] == 0x75 && rdesc[628] == 64 &&
	    /* report count 1 */
	    rdesc[629] == 0x95 && rdesc[630] == 1) {
		rdesc[628] = 32;
		rdesc[630] = 2;
		hid_dbg(hdev, "Fixed up second 64-bit field\n");
	}

	return rdesc;
}

#ifdef CONFIG_PM
/**
 * appleib_forward_int_op() - Forward a hid-driver callback to all drivers on
 * all virtual HID devices attached to the given real HID device.
 * @hdev the real hid-device
 * @forward a function that calls the callback on the given driver
 * @args arguments for the forward function
 *
 * This is for callbacks that return a status as an int.
 *
 * Returns: 0 on success, or the first error returned by the @forward function.
 */
static int appleib_forward_int_op(struct hid_device *hdev,
				  int (*forward)(struct hid_driver *,
						 struct hid_device *, void *),
				  void *args)
{
	struct appleib_hid_dev_info *hdev_info = hid_get_drvdata(hdev);
	struct hid_device *sub_hdev;
	int rc = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(hdev_info->sub_hdevs); i++) {
		sub_hdev = hdev_info->sub_hdevs[i];
		if (sub_hdev->driver) {
			rc = forward(sub_hdev->driver, sub_hdev, args);
			if (rc)
				break;
		}

		break;
	}

	return rc;
}

static int appleib_hid_suspend_fwd(struct hid_driver *drv,
				   struct hid_device *hdev, void *args)
{
	int rc = 0;

	if (drv->suspend)
		rc = drv->suspend(hdev, *(pm_message_t *)args);

	return rc;
}

static int appleib_hid_suspend(struct hid_device *hdev, pm_message_t message)
{
	return appleib_forward_int_op(hdev, appleib_hid_suspend_fwd, &message);
}

static int appleib_hid_resume_fwd(struct hid_driver *drv,
				  struct hid_device *hdev, void *args)
{
	int rc = 0;

	if (drv->resume)
		rc = drv->resume(hdev);

	return rc;
}

static int appleib_hid_resume(struct hid_device *hdev)
{
	return appleib_forward_int_op(hdev, appleib_hid_resume_fwd, NULL);
}

static int appleib_hid_reset_resume_fwd(struct hid_driver *drv,
					struct hid_device *hdev, void *args)
{
	int rc = 0;

	if (drv->reset_resume)
		rc = drv->reset_resume(hdev);

	return rc;
}

static int appleib_hid_reset_resume(struct hid_device *hdev)
{
	return appleib_forward_int_op(hdev, appleib_hid_reset_resume_fwd, NULL);
}
#endif /* CONFIG_PM */

static int appleib_ll_start(struct hid_device *hdev)
{
	return 0;
}

static void appleib_ll_stop(struct hid_device *hdev)
{
}

static int appleib_ll_open(struct hid_device *hdev)
{
	/*
	 * This is not currently implemented, because the existing sub-drivers
	 * don't need it to be, and it seems unlikely they will ever need it to
	 * be: we call hid_hw_open() in this driver, so implementing this
	 * function here and appleib_ll_close() below would just involve the
	 * enabling/disabling of the forwarding of events to the sub-device;
	 * but since the sub-drivers only call hid_hw_open() inside their probe
	 * functions and hid_hw_close() inside their remove functions, this
	 * means we can always just forward the events and they will get
	 * delivered if a sub-driver is bound to the sub-device and not if not.
	 */
	return 0;
}

static void appleib_ll_close(struct hid_device *hdev)
{
	/* see comment in appleib_ll_open */
}

static int appleib_ll_power(struct hid_device *hdev, int level)
{
	struct appleib_hid_dev_info *hdev_info = hdev->driver_data;

	return hid_hw_power(hdev_info->hdev, level);
}

static int appleib_ll_parse(struct hid_device *hdev)
{
	struct appleib_hid_dev_info *hdev_info = hdev->driver_data;

	return hid_parse_report(hdev, hdev_info->hdev->rdesc,
				hdev_info->hdev->rsize);
}

static void appleib_ll_request(struct hid_device *hdev,
			       struct hid_report *report, int reqtype)
{
	struct appleib_hid_dev_info *hdev_info = hdev->driver_data;

	hid_hw_request(hdev_info->hdev, report, reqtype);
}

static int appleib_ll_wait(struct hid_device *hdev)
{
	struct appleib_hid_dev_info *hdev_info = hdev->driver_data;

	hid_hw_wait(hdev_info->hdev);
	return 0;
}

static int appleib_ll_raw_request(struct hid_device *hdev,
				  unsigned char reportnum, __u8 *buf,
				  size_t len, unsigned char rtype, int reqtype)
{
	struct appleib_hid_dev_info *hdev_info = hdev->driver_data;

	return hid_hw_raw_request(hdev_info->hdev, reportnum, buf, len, rtype,
				  reqtype);
}

static int appleib_ll_output_report(struct hid_device *hdev, __u8 *buf,
				    size_t len)
{
	struct appleib_hid_dev_info *hdev_info = hdev->driver_data;

	return hid_hw_output_report(hdev_info->hdev, buf, len);
}

static struct hid_ll_driver appleib_ll_driver = {
	.start = appleib_ll_start,
	.stop = appleib_ll_stop,
	.open = appleib_ll_open,
	.close = appleib_ll_close,
	.power = appleib_ll_power,
	.parse = appleib_ll_parse,
	.request = appleib_ll_request,
	.wait = appleib_ll_wait,
	.raw_request = appleib_ll_raw_request,
	.output_report = appleib_ll_output_report,
};

static struct hid_device *
appleib_add_sub_dev(struct appleib_hid_dev_info *hdev_info,
		    struct hid_device_id *dev_id)
{
	struct hid_device *sub_hdev;
	int rc;

	sub_hdev = hid_allocate_device();
	if (IS_ERR(sub_hdev))
		return sub_hdev;

	sub_hdev->dev.parent = &hdev_info->hdev->dev;

	sub_hdev->bus = dev_id->bus;
	sub_hdev->group = dev_id->group;
	sub_hdev->vendor = dev_id->vendor;
	sub_hdev->product = dev_id->product;

	sub_hdev->ll_driver = &appleib_ll_driver;

	snprintf(sub_hdev->name, sizeof(sub_hdev->name),
		 "iBridge Virtual HID %s/%04x:%04x",
		 dev_name(sub_hdev->dev.parent), sub_hdev->vendor,
		 sub_hdev->product);

	sub_hdev->driver_data = hdev_info;

	rc = hid_add_device(sub_hdev);
	if (rc) {
		hid_destroy_device(sub_hdev);
		return ERR_PTR(rc);
	}

	return sub_hdev;
}

static struct appleib_hid_dev_info *appleib_add_device(struct hid_device *hdev)
{
	struct appleib_hid_dev_info *hdev_info;
	int i;

	hdev_info = devm_kzalloc(&hdev->dev, sizeof(*hdev_info), GFP_KERNEL);
	if (!hdev_info)
		return ERR_PTR(-ENOMEM);

	hdev_info->hdev = hdev;

	for (i = 0; i < ARRAY_SIZE(hdev_info->sub_hdevs); i++) {
		hdev_info->sub_hdevs[i] =
			appleib_add_sub_dev(hdev_info, &appleib_sub_hid_ids[i]);

		if (IS_ERR(hdev_info->sub_hdevs[i])) {
			while (i-- > 0)
				hid_destroy_device(hdev_info->sub_hdevs[i]);
			return (void *)hdev_info->sub_hdevs[i];
		}
	}

	return hdev_info;
}

static void appleib_remove_device(struct hid_device *hdev)
{
	struct appleib_hid_dev_info *hdev_info = hid_get_drvdata(hdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(hdev_info->sub_hdevs); i++)
		hid_destroy_device(hdev_info->sub_hdevs[i]);

	hid_set_drvdata(hdev, NULL);
}

static int appleib_hid_probe(struct hid_device *hdev,
			     const struct hid_device_id *id)
{
	struct appleib_hid_dev_info *hdev_info;
	struct usb_device *udev;
	int rc;

	/* check and set usb config first */
	udev = hid_to_usb_dev(hdev);

	if (udev->actconfig->desc.bConfigurationValue != APPLEIB_BASIC_CONFIG) {
		rc = usb_driver_set_configuration(udev, APPLEIB_BASIC_CONFIG);
		return rc ? rc : -ENODEV;
	}

	rc = hid_parse(hdev);
	if (rc) {
		hid_err(hdev, "ib: hid parse failed (%d)\n", rc);
		goto error;
	}

	rc = hid_hw_start(hdev, HID_CONNECT_DRIVER);
	if (rc) {
		hid_err(hdev, "ib: hw start failed (%d)\n", rc);
		goto error;
	}

	hdev_info = appleib_add_device(hdev);
	if (IS_ERR(hdev_info)) {
		rc = PTR_ERR(hdev_info);
		goto stop_hw;
	}

	hid_set_drvdata(hdev, hdev_info);

	rc = hid_hw_open(hdev);
	if (rc) {
		hid_err(hdev, "ib: failed to open hid: %d\n", rc);
		goto remove_dev;
	}

	return 0;

remove_dev:
	appleib_remove_device(hdev);
stop_hw:
	hid_hw_stop(hdev);
error:
	return rc;
}

static void appleib_hid_remove(struct hid_device *hdev)
{
	hid_hw_close(hdev);
	appleib_remove_device(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id appleib_hid_ids[] = {
	{ HID_USB_DEVICE(USB_ID_VENDOR_APPLE, USB_ID_PRODUCT_IBRIDGE) },
	{ },
};

static struct hid_driver appleib_hid_driver = {
	.name = "apple-ibridge-hid",
	.id_table = appleib_hid_ids,
	.probe = appleib_hid_probe,
	.remove = appleib_hid_remove,
	.raw_event = appleib_hid_raw_event,
	.report_fixup = appleib_report_fixup,
#ifdef CONFIG_PM
	.suspend = appleib_hid_suspend,
	.resume = appleib_hid_resume,
	.reset_resume = appleib_hid_reset_resume,
#endif
};

static struct appleib_device *appleib_alloc_device(struct acpi_device *acpi_dev)
{
	struct appleib_device *ib_dev;
	acpi_status sts;

	ib_dev = devm_kzalloc(&acpi_dev->dev, sizeof(*ib_dev), GFP_KERNEL);
	if (!ib_dev)
		return ERR_PTR(-ENOMEM);

	ib_dev->acpi_dev = acpi_dev;

	/* get iBridge acpi power control method for suspend/resume */
	sts = acpi_get_handle(acpi_dev->handle, "SOCW", &ib_dev->asoc_socw);
	if (ACPI_FAILURE(sts)) {
		dev_err(LOG_DEV(ib_dev),
			"Error getting handle for ASOC.SOCW method: %s\n",
			acpi_format_exception(sts));
		return ERR_PTR(-ENXIO);
	}

	/* ensure iBridge is powered on */
	sts = acpi_execute_simple_method(ib_dev->asoc_socw, NULL, 1);
	if (ACPI_FAILURE(sts))
		dev_warn(LOG_DEV(ib_dev), "SOCW(1) failed: %s\n",
			 acpi_format_exception(sts));

	return ib_dev;
}

static int appleib_probe(struct acpi_device *acpi)
{
	struct appleib_device *ib_dev;
	int ret;

	ib_dev = appleib_alloc_device(acpi);
	if (IS_ERR(ib_dev))
		return PTR_ERR(ib_dev);

	ret = hid_register_driver(&appleib_hid_driver);
	if (ret) {
		dev_err(LOG_DEV(ib_dev), "Error registering hid driver: %d\n",
			ret);
		return ret;
	}

	acpi->driver_data = ib_dev;

	return 0;
}

static int appleib_remove(struct acpi_device *acpi)
{
	hid_unregister_driver(&appleib_hid_driver);

	return 0;
}

static int appleib_suspend(struct device *dev)
{
	struct appleib_device *ib_dev;
	int rc;

	ib_dev = acpi_driver_data(to_acpi_device(dev));

	rc = acpi_execute_simple_method(ib_dev->asoc_socw, NULL, 0);
	if (ACPI_FAILURE(rc))
		dev_warn(dev, "SOCW(0) failed: %s\n",
			 acpi_format_exception(rc));

	return 0;
}

static int appleib_resume(struct device *dev)
{
	struct appleib_device *ib_dev;
	int rc;

	ib_dev = acpi_driver_data(to_acpi_device(dev));

	rc = acpi_execute_simple_method(ib_dev->asoc_socw, NULL, 1);
	if (ACPI_FAILURE(rc))
		dev_warn(dev, "SOCW(1) failed: %s\n",
			 acpi_format_exception(rc));

	return 0;
}

static const struct dev_pm_ops appleib_pm = {
	.suspend = appleib_suspend,
	.resume = appleib_resume,
	.restore = appleib_resume,
};

static const struct acpi_device_id appleib_acpi_match[] = {
	{ "APP7777", 0 },
	{ },
};

MODULE_DEVICE_TABLE(acpi, appleib_acpi_match);

static struct acpi_driver appleib_driver = {
	.name		= "apple-ibridge",
	.class		= "apple_ibridge",
	.owner		= THIS_MODULE,
	.ids		= appleib_acpi_match,
	.ops		= {
		.add		= appleib_probe,
		.remove		= appleib_remove,
	},
	.drv		= {
		.pm		= &appleib_pm,
	},
};

module_acpi_driver(appleib_driver)

MODULE_AUTHOR("Ronald Tschalär");
MODULE_DESCRIPTION("Apple iBridge driver");
MODULE_LICENSE("GPL v2");
