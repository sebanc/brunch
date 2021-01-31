/*
 *  HID driver for Quickstep, ChromeOS's Latency Measurement Gadget
 *
 *  The device is connected via USB and transmits a byte each time a
 *  laster is crossed.  The job of the driver is to record when those events
 *  happen and then make that information availible to the user via sysfs
 *  entries.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/time.h>

#include "hid-ids.h"

#define MAX_CROSSINGS 64

enum change_type { OFF, ON };

struct qs_event {
	struct timespec time;
	enum change_type direction;
};

struct qs_data {
	unsigned int head;
	struct qs_event events[MAX_CROSSINGS];
};

static ssize_t append_event(struct qs_event *event, char *buf, ssize_t len)
{
	return snprintf(buf, len, "%010ld.%09ld\t%d\n", event->time.tv_sec,
			event->time.tv_nsec, event->direction);

}

static ssize_t show_log(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i, str_len;
	struct qs_data *data = dev_get_drvdata(dev);

	str_len = snprintf(buf, PAGE_SIZE,
			"Laser Crossings:\ntime\t\t\tdirection\n");

	if (data->head >= MAX_CROSSINGS) {
		for (i = data->head % MAX_CROSSINGS; i < MAX_CROSSINGS; i++) {
			str_len += append_event(&data->events[i], buf + str_len,
						PAGE_SIZE - str_len);
		}
	}

	for (i = 0; i < data->head % MAX_CROSSINGS; i++) {
		str_len += append_event(&data->events[i], buf + str_len,
					PAGE_SIZE - str_len);
	}

	return str_len;
}

static void empty_quickstep_data(struct qs_data *data)
{
	if (data == NULL)
		return;
	data->head = 0;
}

static ssize_t clear_log(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t len)
{
	empty_quickstep_data(dev_get_drvdata(dev));
	return len;
}

static DEVICE_ATTR(laser, 0444, show_log, NULL);
static DEVICE_ATTR(clear, 0220, NULL, clear_log);
static struct attribute *dev_attrs[] = {
	&dev_attr_laser.attr,
	&dev_attr_clear.attr,
	NULL,
};
static struct attribute_group dev_attr_group = {.attrs = dev_attrs};

static int quickstep_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	int ret;
	struct qs_data *data;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	ret = hid_hw_open(hdev);
	if (ret) {
		hid_err(hdev, "hw open failed\n");
		hid_hw_stop(hdev);
		return ret;
	}

	data = kmalloc(sizeof(struct qs_data), GFP_KERNEL);
	empty_quickstep_data(data);
	hid_set_drvdata(hdev, data);

	ret = sysfs_create_group(&hdev->dev.kobj, &dev_attr_group);

	return ret;
}

static void quickstep_remove(struct hid_device *hdev)
{
	sysfs_remove_group(&hdev->dev.kobj, &dev_attr_group);
	hid_hw_stop(hdev);
	kfree(hid_get_drvdata(hdev));
}

static int quickstep_raw_event(struct hid_device *hdev,
	struct hid_report *report, u8 *msg, int size)
{
	struct timespec time;
	struct qs_data *data = hid_get_drvdata(hdev);

	getnstimeofday(&time);

	data->events[data->head % MAX_CROSSINGS].time = time;
	data->events[data->head % MAX_CROSSINGS].direction = msg[0] ? ON : OFF;

	data->head++;
	if (data->head >= MAX_CROSSINGS * 2)
		data->head = MAX_CROSSINGS + data->head % MAX_CROSSINGS;

	return 0;
}

static const struct hid_device_id quickstep_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_GOOGLE,
		USB_DEVICE_ID_GOOGLE_QUICKSTEP) },
	{ }
};
MODULE_DEVICE_TABLE(hid, quickstep_devices);

static struct hid_driver quickstep_driver = {
	.name = "quickstep",
	.id_table = quickstep_devices,
	.probe = quickstep_probe,
	.remove = quickstep_remove,
	.raw_event = quickstep_raw_event,
};

static int __init quickstep_init(void)
{
	return hid_register_driver(&quickstep_driver);
}

static void __exit quickstep_exit(void)
{
	hid_unregister_driver(&quickstep_driver);
}

module_init(quickstep_init);
module_exit(quickstep_exit);
MODULE_AUTHOR("Charlie Mooney <charliemooney@google.com>");
MODULE_LICENSE("GPL");
