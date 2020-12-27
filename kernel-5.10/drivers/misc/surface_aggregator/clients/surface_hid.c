// SPDX-License-Identifier: GPL-2.0+
/*
 * Surface System Aggregator Module (SSAM) HID device driver.
 *
 * Provides support for HID input devices connected via the Surface System
 * Aggregator Module.
 *
 * Copyright (C) 2019-2020 Blaž Hrastnik <blaz@mxxn.io>,
 *                         Maximilian Luz <luzmaximilian@gmail.com>
 */

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/usb/ch9.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/device.h>

enum surface_hid_descriptor_entry {
	SURFACE_HID_DESC_HID    = 0,
	SURFACE_HID_DESC_REPORT = 1,
	SURFACE_HID_DESC_ATTRS  = 2,
};

struct surface_hid_descriptor {
	__u8 desc_len;			/* = 9 */
	__u8 desc_type;			/* = HID_DT_HID */
	__le16 hid_version;
	__u8 country_code;
	__u8 num_descriptors;		/* = 1 */

	__u8 report_desc_type;		/* = HID_DT_REPORT */
	__le16 report_desc_len;
} __packed;

static_assert(sizeof(struct surface_hid_descriptor) == 9);

struct surface_hid_attributes {
	__le32 length;
	__le16 vendor;
	__le16 product;
	__le16 version;
	__u8 _unknown[22];
} __packed;

static_assert(sizeof(struct surface_hid_attributes) == 32);

struct surface_hid_device;

struct surface_hid_device_ops {
	int (*get_descriptor)(struct surface_hid_device *shid, u8 entry,
			      u8 *buf, size_t len);
	int (*output_report)(struct surface_hid_device *shid, u8 report_id,
			     u8 *data, size_t len);
	int (*get_feature_report)(struct surface_hid_device *shid, u8 report_id,
				  u8 *data, size_t len);
	int (*set_feature_report)(struct surface_hid_device *shid, u8 report_id,
				  u8 *data, size_t len);
};

struct surface_hid_device {
	struct device *dev;
	struct ssam_controller *ctrl;
	struct ssam_device_uid uid;

	struct surface_hid_descriptor hid_desc;
	struct surface_hid_attributes attrs;

	struct ssam_event_notifier notif;
	struct hid_device *hid;

	struct surface_hid_device_ops ops;
};


/* -- SAM interface (HID). -------------------------------------------------- */

#ifdef CONFIG_SURFACE_AGGREGATOR_BUS

struct surface_hid_buffer_slice {
	__u8 entry;
	__le32 offset;
	__le32 length;
	__u8 end;
	__u8 data[];
} __packed;

static_assert(sizeof(struct surface_hid_buffer_slice) == 10);

enum surface_hid_cid {
	SURFACE_HID_CID_OUTPUT_REPORT      = 0x01,
	SURFACE_HID_CID_GET_FEATURE_REPORT = 0x02,
	SURFACE_HID_CID_SET_FEATURE_REPORT = 0x03,
	SURFACE_HID_CID_GET_DESCRIPTOR     = 0x04,
};

static int ssam_hid_get_descriptor(struct surface_hid_device *shid, u8 entry,
				   u8 *buf, size_t len)
{
	u8 buffer[sizeof(struct surface_hid_buffer_slice) + 0x76];
	struct surface_hid_buffer_slice *slice;
	struct ssam_request rqst;
	struct ssam_response rsp;
	u32 buffer_len, offset, length;
	int status;

	/*
	 * Note: The 0x76 above has been chosen because that's what's used by
	 * the Windows driver. Together with the header, this leads to a 128
	 * byte payload in total.
	 */

	buffer_len = ARRAY_SIZE(buffer) - sizeof(struct surface_hid_buffer_slice);

	rqst.target_category = shid->uid.category;
	rqst.target_id = shid->uid.target;
	rqst.command_id = SURFACE_HID_CID_GET_DESCRIPTOR;
	rqst.instance_id = shid->uid.instance;
	rqst.flags = SSAM_REQUEST_HAS_RESPONSE;
	rqst.length = sizeof(struct surface_hid_buffer_slice);
	rqst.payload = buffer;

	rsp.capacity = ARRAY_SIZE(buffer);
	rsp.pointer = buffer;

	slice = (struct surface_hid_buffer_slice *)buffer;
	slice->entry = entry;
	slice->end = 0;

	offset = 0;
	length = buffer_len;

	while (!slice->end && offset < len) {
		put_unaligned_le32(offset, &slice->offset);
		put_unaligned_le32(length, &slice->length);

		rsp.length = 0;

		status = ssam_retry(ssam_request_sync_onstack, shid->ctrl,
				    &rqst, &rsp, sizeof(*slice));
		if (status)
			return status;

		offset = get_unaligned_le32(&slice->offset);
		length = get_unaligned_le32(&slice->length);

		/* Don't mess stuff up in case we receive garbage. */
		if (length > buffer_len || offset > len)
			return -EPROTO;

		if (offset + length > len)
			length = len - offset;

		memcpy(buf + offset, &slice->data[0], length);

		offset += length;
		length = buffer_len;
	}

	if (offset != len) {
		dev_err(shid->dev,
			"unexpected descriptor length: got %u, expected %zu\n",
			offset, len);
		return -EPROTO;
	}

	return 0;
}

static int ssam_hid_set_raw_report(struct surface_hid_device *shid,
				   u8 report_id, bool feature, u8 *buf,
				   size_t len)
{
	struct ssam_request rqst;
	u8 cid;

	if (feature)
		cid = SURFACE_HID_CID_SET_FEATURE_REPORT;
	else
		cid = SURFACE_HID_CID_OUTPUT_REPORT;

	rqst.target_category = shid->uid.category;
	rqst.target_id = shid->uid.target;
	rqst.instance_id = shid->uid.instance;
	rqst.command_id = cid;
	rqst.flags = 0;
	rqst.length = len;
	rqst.payload = buf;

	buf[0] = report_id;

	return ssam_retry(ssam_request_sync, shid->ctrl, &rqst, NULL);
}

static int ssam_hid_get_raw_report(struct surface_hid_device *shid,
				   u8 report_id, u8 *buf, size_t len)
{
	struct ssam_request rqst;
	struct ssam_response rsp;

	rqst.target_category = shid->uid.category;
	rqst.target_id = shid->uid.target;
	rqst.instance_id = shid->uid.instance;
	rqst.command_id = SURFACE_HID_CID_GET_FEATURE_REPORT;
	rqst.flags = 0;
	rqst.length = sizeof(report_id);
	rqst.payload = &report_id;

	rsp.capacity = len;
	rsp.length = 0;
	rsp.pointer = buf;

	return ssam_retry(ssam_request_sync_onstack, shid->ctrl, &rqst, &rsp,
			  sizeof(report_id));
}

static u32 ssam_hid_event_fn(struct ssam_event_notifier *nf,
			     const struct ssam_event *event)
{
	struct surface_hid_device *shid;
	int status;

	shid = container_of(nf, struct surface_hid_device, notif);

	if (event->command_id != 0x00)
		return 0;

	status = hid_input_report(shid->hid, HID_INPUT_REPORT,
				  (u8 *)&event->data[0], event->length, 0);

	return ssam_notifier_from_errno(status) | SSAM_NOTIF_HANDLED;
}


/* -- Transport driver (HID). ----------------------------------------------- */

static int shid_output_report(struct surface_hid_device *shid, u8 report_id,
			      u8 *data, size_t len)
{
	int status;

	status =  ssam_hid_set_raw_report(shid, report_id, false, data, len);
	return status >= 0 ? len : status;
}

static int shid_get_feature_report(struct surface_hid_device *shid,
				   u8 report_id, u8 *data, size_t len)
{
	int status;

	status = ssam_hid_get_raw_report(shid, report_id, data, len);
	return status >= 0 ? len : status;
}

static int shid_set_feature_report(struct surface_hid_device *shid,
				   u8 report_id, u8 *data, size_t len)
{
	int status;

	status =  ssam_hid_set_raw_report(shid, report_id, true, data, len);
	return status >= 0 ? len : status;
}

#endif /* CONFIG_SURFACE_AGGREGATOR_BUS */


/* -- SAM interface (KBD). -------------------------------------------------- */

#define KBD_FEATURE_REPORT_SIZE		7  /* 6 + report ID */

enum surface_kbd_cid {
	SURFACE_KBD_CID_GET_DESCRIPTOR     = 0x00,
	SURFACE_KBD_CID_SET_CAPSLOCK_LED   = 0x01,
	SURFACE_KBD_CID_EVT_INPUT_GENERIC  = 0x03,
	SURFACE_KBD_CID_EVT_INPUT_HOTKEYS  = 0x04,
	SURFACE_KBD_CID_GET_FEATURE_REPORT = 0x0b,
};

static int ssam_kbd_get_descriptor(struct surface_hid_device *shid, u8 entry,
				   u8 *buf, size_t len)
{
	struct ssam_request rqst;
	struct ssam_response rsp;
	int status;

	rqst.target_category = shid->uid.category;
	rqst.target_id = shid->uid.target;
	rqst.command_id = SURFACE_KBD_CID_GET_DESCRIPTOR;
	rqst.instance_id = shid->uid.instance;
	rqst.flags = SSAM_REQUEST_HAS_RESPONSE;
	rqst.length = sizeof(entry);
	rqst.payload = &entry;

	rsp.capacity = len;
	rsp.length = 0;
	rsp.pointer = buf;

	status = ssam_retry(ssam_request_sync_onstack, shid->ctrl, &rqst, &rsp,
			    sizeof(entry));
	if (status)
		return status;

	if (rsp.length != len) {
		dev_err(shid->dev,
			"invalid descriptor length: got %zu, expected, %zu\n",
			rsp.length, len);
		return -EPROTO;
	}

	return 0;
}

static int ssam_kbd_set_caps_led(struct surface_hid_device *shid, bool value)
{
	struct ssam_request rqst;
	u8 value_u8 = value;

	rqst.target_category = shid->uid.category;
	rqst.target_id = shid->uid.target;
	rqst.command_id = SURFACE_KBD_CID_SET_CAPSLOCK_LED;
	rqst.instance_id = shid->uid.instance;
	rqst.flags = 0;
	rqst.length = sizeof(value_u8);
	rqst.payload = &value_u8;

	return ssam_retry(ssam_request_sync_onstack, shid->ctrl, &rqst, NULL,
			  sizeof(value_u8));
}

static int ssam_kbd_get_feature_report(struct surface_hid_device *shid, u8 *buf,
				       size_t len)
{
	struct ssam_request rqst;
	struct ssam_response rsp;
	u8 payload = 0;
	int status;

	rqst.target_category = shid->uid.category;
	rqst.target_id = shid->uid.target;
	rqst.command_id = SURFACE_KBD_CID_GET_FEATURE_REPORT;
	rqst.instance_id = shid->uid.instance;
	rqst.flags = SSAM_REQUEST_HAS_RESPONSE;
	rqst.length = sizeof(payload);
	rqst.payload = &payload;

	rsp.capacity = len;
	rsp.length = 0;
	rsp.pointer = buf;

	status = ssam_retry(ssam_request_sync_onstack, shid->ctrl, &rqst, &rsp,
			    sizeof(payload));
	if (status)
		return status;

	if (rsp.length != len) {
		dev_err(shid->dev,
			"invalid feature report length: got %zu, expected, %zu\n",
			rsp.length, len);
		return -EPROTO;
	}

	return 0;
}

static bool ssam_kbd_is_input_event(const struct ssam_event *event)
{
	if (event->command_id == SURFACE_KBD_CID_EVT_INPUT_GENERIC)
		return true;

	if (event->command_id == SURFACE_KBD_CID_EVT_INPUT_HOTKEYS)
		return true;

	return false;
}

static u32 ssam_kbd_event_fn(struct ssam_event_notifier *nf,
			     const struct ssam_event *event)
{
	struct surface_hid_device *shid;
	int status;

	shid = container_of(nf, struct surface_hid_device, notif);

	/*
	 * Check against device UID manually, as registry and device target
	 * category doesn't line up.
	 */

	if (shid->uid.category != event->target_category)
		return 0;

	if (shid->uid.target != event->target_id)
		return 0;

	if (shid->uid.instance != event->instance_id)
		return 0;

	if (!ssam_kbd_is_input_event(event))
		return 0;

	status = hid_input_report(shid->hid, HID_INPUT_REPORT,
				  (u8 *)&event->data[0], event->length, 0);

	return ssam_notifier_from_errno(status) | SSAM_NOTIF_HANDLED;
}


/* -- Transport driver (KBD). ----------------------------------------------- */

static int skbd_get_caps_led_value(struct hid_device *hid, u8 report_id,
				   u8 *data, size_t len)
{
	struct hid_field *field;
	unsigned int offset, size;
	int i;

	/* Get led field. */
	field = hidinput_get_led_field(hid);
	if (!field)
		return -ENOENT;

	/* Check if we got the correct report. */
	if (len != hid_report_len(field->report))
		return -ENOENT;

	if (report_id != field->report->id)
		return -ENOENT;

	/* Get caps lock led index. */
	for (i = 0; i < field->report_count; i++)
		if ((field->usage[i].hid & 0xffff) == 0x02)
			break;

	if (i == field->report_count)
		return -ENOENT;

	/* Extract value. */
	size = field->report_size;
	offset = field->report_offset + i * size;
	return !!hid_field_extract(hid, data + 1, size, offset);
}

static int skbd_output_report(struct surface_hid_device *shid, u8 report_id,
			      u8 *data, size_t len)
{
	int caps_led;
	int status;

	caps_led = skbd_get_caps_led_value(shid->hid, report_id, data, len);
	if (caps_led < 0)
		return -EIO;	/* Only caps output reports are supported. */

	status = ssam_kbd_set_caps_led(shid, caps_led);
	if (status < 0)
		return status;

	return len;
}

static int skbd_get_feature_report(struct surface_hid_device *shid,
				   u8 report_id, u8 *data, size_t len)
{
	u8 report[KBD_FEATURE_REPORT_SIZE];
	int status;

	/*
	 * The keyboard only has a single hard-coded read-only feature report
	 * of size KBD_FEATURE_REPORT_SIZE. Try to load it and compare its
	 * report ID against the requested one.
	 */

	if (len < ARRAY_SIZE(report))
		return -ENOSPC;

	status = ssam_kbd_get_feature_report(shid, report, ARRAY_SIZE(report));
	if (status < 0)
		return status;

	if (report_id != report[0])
		return -ENOENT;

	memcpy(data, report, ARRAY_SIZE(report));
	return len;
}

static int skbd_set_feature_report(struct surface_hid_device *shid,
				   u8 report_id, u8 *data, size_t len)
{
	return -EIO;
}


/* -- Device descriptor access. --------------------------------------------- */

static int surface_hid_load_hid_descriptor(struct surface_hid_device *shid)
{
	int status;

	status = shid->ops.get_descriptor(shid, SURFACE_HID_DESC_HID,
			(u8 *)&shid->hid_desc, sizeof(shid->hid_desc));
	if (status)
		return status;

	if (shid->hid_desc.desc_len != sizeof(shid->hid_desc)) {
		dev_err(shid->dev,
			"unexpected HID descriptor length: got %u, expected %zu\n",
			shid->hid_desc.desc_len, sizeof(shid->hid_desc));
		return -EPROTO;
	}

	if (shid->hid_desc.desc_type != HID_DT_HID) {
		dev_err(shid->dev,
			"unexpected HID descriptor type: got %#04x, expected %#04x\n",
			shid->hid_desc.desc_type, HID_DT_HID);
		return -EPROTO;
	}

	if (shid->hid_desc.num_descriptors != 1) {
		dev_err(shid->dev,
			"unexpected number of descriptors: got %u, expected 1\n",
			shid->hid_desc.num_descriptors);
		return -EPROTO;
	}

	if (shid->hid_desc.report_desc_type != HID_DT_REPORT) {
		dev_err(shid->dev,
			"unexpected report descriptor type: got %#04x, expected %#04x\n",
			shid->hid_desc.report_desc_type, HID_DT_REPORT);
		return -EPROTO;
	}

	return 0;
}

static int surface_hid_load_device_attributes(struct surface_hid_device *shid)
{
	int status;

	status = shid->ops.get_descriptor(shid, SURFACE_HID_DESC_ATTRS,
			(u8 *)&shid->attrs, sizeof(shid->attrs));
	if (status)
		return status;

	if (get_unaligned_le32(&shid->attrs.length) != sizeof(shid->attrs)) {
		dev_err(shid->dev,
			"unexpected attribute length: got %u, expected %zu\n",
			get_unaligned_le32(&shid->attrs.length), sizeof(shid->attrs));
		return -EPROTO;
	}

	return 0;
}


/* -- Transport driver (common). -------------------------------------------- */

static int surface_hid_start(struct hid_device *hid)
{
	struct surface_hid_device *shid = hid->driver_data;

	return ssam_notifier_register(shid->ctrl, &shid->notif);
}

static void surface_hid_stop(struct hid_device *hid)
{
	struct surface_hid_device *shid = hid->driver_data;

	/* Note: This call will log errors for us, so ignore them here. */
	ssam_notifier_unregister(shid->ctrl, &shid->notif);
}

static int surface_hid_open(struct hid_device *hid)
{
	return 0;
}

static void surface_hid_close(struct hid_device *hid)
{
}

static int surface_hid_parse(struct hid_device *hid)
{
	struct surface_hid_device *shid = hid->driver_data;
	size_t len = get_unaligned_le16(&shid->hid_desc.report_desc_len);
	u8 *buf;
	int status;

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	status = shid->ops.get_descriptor(shid, SURFACE_HID_DESC_REPORT, buf, len);
	if (!status)
		status = hid_parse_report(hid, buf, len);

	kfree(buf);
	return status;
}

static int surface_hid_raw_request(struct hid_device *hid, unsigned char reportnum,
				   u8 *buf, size_t len, unsigned char rtype,
				   int reqtype)
{
	struct surface_hid_device *shid = hid->driver_data;

	if (rtype == HID_OUTPUT_REPORT && reqtype == HID_REQ_SET_REPORT)
		return shid->ops.output_report(shid, reportnum, buf, len);

	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT)
		return shid->ops.get_feature_report(shid, reportnum, buf, len);

	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_SET_REPORT)
		return shid->ops.set_feature_report(shid, reportnum, buf, len);

	return -EIO;
}

static struct hid_ll_driver surface_hid_ll_driver = {
	.start       = surface_hid_start,
	.stop        = surface_hid_stop,
	.open        = surface_hid_open,
	.close       = surface_hid_close,
	.parse       = surface_hid_parse,
	.raw_request = surface_hid_raw_request,
};


/* -- Common device setup. -------------------------------------------------- */

static int surface_hid_device_add(struct surface_hid_device *shid)
{
	int status;

	status = surface_hid_load_hid_descriptor(shid);
	if (status)
		return status;

	status = surface_hid_load_device_attributes(shid);
	if (status)
		return status;

	shid->hid = hid_allocate_device();
	if (IS_ERR(shid->hid))
		return PTR_ERR(shid->hid);

	shid->hid->dev.parent = shid->dev;
	shid->hid->bus = BUS_HOST;		// TODO: BUS_SURFACE
	shid->hid->vendor = cpu_to_le16(shid->attrs.vendor);
	shid->hid->product = cpu_to_le16(shid->attrs.product);
	shid->hid->version = cpu_to_le16(shid->hid_desc.hid_version);
	shid->hid->country = shid->hid_desc.country_code;

	snprintf(shid->hid->name, sizeof(shid->hid->name),
		 "Microsoft Surface %04X:%04X",
		 shid->hid->vendor, shid->hid->product);

	strlcpy(shid->hid->phys, dev_name(shid->dev), sizeof(shid->hid->phys));

	shid->hid->driver_data = shid;
	shid->hid->ll_driver = &surface_hid_ll_driver;

	status = hid_add_device(shid->hid);
	if (status)
		hid_destroy_device(shid->hid);

	return status;
}

static void surface_hid_device_destroy(struct surface_hid_device *shid)
{
	hid_destroy_device(shid->hid);
}


/* -- PM ops. --------------------------------------------------------------- */

#ifdef CONFIG_PM_SLEEP

static int surface_hid_suspend(struct device *dev)
{
	struct surface_hid_device *d = dev_get_drvdata(dev);

	if (d->hid->driver && d->hid->driver->suspend)
		return d->hid->driver->suspend(d->hid, PMSG_SUSPEND);

	return 0;
}

static int surface_hid_resume(struct device *dev)
{
	struct surface_hid_device *d = dev_get_drvdata(dev);

	if (d->hid->driver && d->hid->driver->resume)
		return d->hid->driver->resume(d->hid);

	return 0;
}

static int surface_hid_freeze(struct device *dev)
{
	struct surface_hid_device *d = dev_get_drvdata(dev);

	if (d->hid->driver && d->hid->driver->suspend)
		return d->hid->driver->suspend(d->hid, PMSG_FREEZE);

	return 0;
}

static int surface_hid_poweroff(struct device *dev)
{
	struct surface_hid_device *d = dev_get_drvdata(dev);

	if (d->hid->driver && d->hid->driver->suspend)
		return d->hid->driver->suspend(d->hid, PMSG_HIBERNATE);

	return 0;
}

static int surface_hid_restore(struct device *dev)
{
	struct surface_hid_device *d = dev_get_drvdata(dev);

	if (d->hid->driver && d->hid->driver->reset_resume)
		return d->hid->driver->reset_resume(d->hid);

	return 0;
}

const struct dev_pm_ops surface_hid_pm_ops = {
	.freeze   = surface_hid_freeze,
	.thaw     = surface_hid_resume,
	.suspend  = surface_hid_suspend,
	.resume   = surface_hid_resume,
	.poweroff = surface_hid_poweroff,
	.restore  = surface_hid_restore,
};

#else /* CONFIG_PM_SLEEP */

const struct dev_pm_ops surface_hid_pm_ops = { };

#endif /* CONFIG_PM_SLEEP */


/* -- Driver setup (HID). --------------------------------------------------- */

#ifdef CONFIG_SURFACE_AGGREGATOR_BUS

static int surface_hid_probe(struct ssam_device *sdev)
{
	struct surface_hid_device *shid;

	shid = devm_kzalloc(&sdev->dev, sizeof(*shid), GFP_KERNEL);
	if (!shid)
		return -ENOMEM;

	shid->dev = &sdev->dev;
	shid->ctrl = sdev->ctrl;
	shid->uid = sdev->uid;

	shid->notif.base.priority = 1;
	shid->notif.base.fn = ssam_hid_event_fn;
	shid->notif.event.reg = SSAM_EVENT_REGISTRY_REG;
	shid->notif.event.id.target_category = sdev->uid.category;
	shid->notif.event.id.instance = sdev->uid.instance;
	shid->notif.event.mask = SSAM_EVENT_MASK_STRICT;
	shid->notif.event.flags = 0;

	shid->ops.get_descriptor = ssam_hid_get_descriptor;
	shid->ops.output_report = shid_output_report;
	shid->ops.get_feature_report = shid_get_feature_report;
	shid->ops.set_feature_report = shid_set_feature_report;

	ssam_device_set_drvdata(sdev, shid);
	return surface_hid_device_add(shid);
}

static void surface_hid_remove(struct ssam_device *sdev)
{
	surface_hid_device_destroy(ssam_device_get_drvdata(sdev));
}

static const struct ssam_device_id surface_hid_match[] = {
	{ SSAM_SDEV(HID, 0x02, SSAM_ANY_IID, 0x00) },
	{ },
};
MODULE_DEVICE_TABLE(ssam, surface_hid_match);

static struct ssam_device_driver surface_hid_driver = {
	.probe = surface_hid_probe,
	.remove = surface_hid_remove,
	.match_table = surface_hid_match,
	.driver = {
		.name = "surface_hid",
		.pm = &surface_hid_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

static int surface_hid_driver_register(void)
{
	return ssam_device_driver_register(&surface_hid_driver);
}

static void surface_hid_driver_unregister(void)
{
	ssam_device_driver_unregister(&surface_hid_driver);
}

#else /* CONFIG_SURFACE_AGGREGATOR_BUS */

static int surface_hid_driver_register(void)
{
	return 0;
}

static void surface_hid_driver_unregister(void)
{
}

#endif /* CONFIG_SURFACE_AGGREGATOR_BUS */


/* -- Driver setup (KBD). --------------------------------------------------- */

static int surface_kbd_probe(struct platform_device *pdev)
{
	struct ssam_controller *ctrl;
	struct surface_hid_device *shid;

	/* Add device link to EC. */
	ctrl = ssam_client_bind(&pdev->dev);
	if (IS_ERR(ctrl))
		return PTR_ERR(ctrl) == -ENODEV ? -EPROBE_DEFER : PTR_ERR(ctrl);

	shid = devm_kzalloc(&pdev->dev, sizeof(*shid), GFP_KERNEL);
	if (!shid)
		return -ENOMEM;

	shid->dev = &pdev->dev;
	shid->ctrl = ctrl;

	shid->uid.domain = SSAM_DOMAIN_SERIALHUB;
	shid->uid.category = SSAM_SSH_TC_KBD;
	shid->uid.target = 2;
	shid->uid.instance = 0;
	shid->uid.function = 0;

	shid->notif.base.priority = 1;
	shid->notif.base.fn = ssam_kbd_event_fn;
	shid->notif.event.reg = SSAM_EVENT_REGISTRY_SAM;
	shid->notif.event.id.target_category = shid->uid.category;
	shid->notif.event.id.instance = shid->uid.instance;
	shid->notif.event.mask = SSAM_EVENT_MASK_NONE;
	shid->notif.event.flags = 0;

	shid->ops.get_descriptor = ssam_kbd_get_descriptor;
	shid->ops.output_report = skbd_output_report;
	shid->ops.get_feature_report = skbd_get_feature_report;
	shid->ops.set_feature_report = skbd_set_feature_report;

	platform_set_drvdata(pdev, shid);
	return surface_hid_device_add(shid);
}

static int surface_kbd_remove(struct platform_device *pdev)
{
	surface_hid_device_destroy(platform_get_drvdata(pdev));
	return 0;
}

static const struct acpi_device_id surface_kbd_match[] = {
	{ "MSHW0096" },
	{ },
};
MODULE_DEVICE_TABLE(acpi, surface_kbd_match);

static struct platform_driver surface_kbd_driver = {
	.probe = surface_kbd_probe,
	.remove = surface_kbd_remove,
	.driver = {
		.name = "surface_keyboard",
		.acpi_match_table = surface_kbd_match,
		.pm = &surface_hid_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


/* -- Module setup. --------------------------------------------------------- */

static int __init surface_hid_init(void)
{
	int status;

	status = surface_hid_driver_register();
	if (status)
		return status;

	status = platform_driver_register(&surface_kbd_driver);
	if (status)
		surface_hid_driver_unregister();

	return status;
}
module_init(surface_hid_init);

static void __exit surface_hid_exit(void)
{
	platform_driver_unregister(&surface_kbd_driver);
	surface_hid_driver_unregister();
}
module_exit(surface_hid_exit);

MODULE_AUTHOR("Blaž Hrastnik <blaz@mxxn.io>");
MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("HID transport-/device-driver for Surface System Aggregator Module");
MODULE_LICENSE("GPL");
