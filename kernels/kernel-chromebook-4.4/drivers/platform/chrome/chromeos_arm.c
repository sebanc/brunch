/*
 *  Copyright (C) 2011 The Chromium OS Authors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define pr_fmt(fmt) "chromeos_arm: " fmt

#include <linux/bcd.h>
#include <linux/gpio.h>
#include <linux/notifier.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "chromeos.h"
#include "elog.h"

struct chromeos_arm_elog_panic_buffer {
	uint32_t start;
	uint32_t size;
	void __iomem *virt_addr;
	struct notifier_block nb;
};

/*
 * Update the checksum at the last byte
 */
static void elog_update_checksum(struct event_header *event, u8 checksum)
{
	u8 *event_data = (u8 *)event;
	event_data[event->length - 1] = checksum;
}

/*
 * Simple byte checksum for events
 */
static u8 elog_checksum_event(struct event_header *event)
{
	u8 index, checksum = 0;
	u8 *data = (u8 *)event;

	for (index = 0; index < event->length; index++)
		checksum += data[index];
	return checksum;
}

/*
 * Populate timestamp in event header with current time
 */
static void elog_fill_timestamp(struct event_header *event)
{
	struct timeval timeval;
	struct tm time;

	do_gettimeofday(&timeval);
	time_to_tm(timeval.tv_sec, 0, &time);

	event->second = bin2bcd(time.tm_sec);
	event->minute = bin2bcd(time.tm_min);
	event->hour   = bin2bcd(time.tm_hour);
	event->day    = bin2bcd(time.tm_mday);
	event->month  = bin2bcd(time.tm_mon + 1);
	event->year   = bin2bcd(time.tm_year % 100);
}

/*
 * Fill out an event structure with space for the data and checksum.
 */
void elog_prepare_event(struct event_header *event, u8 event_type, void *data,
			u8 data_size)
{
	event->type = event_type;
	event->length = sizeof(*event) + data_size + 1;
	elog_fill_timestamp(event);

	if (data_size)
		memcpy(&event[1], data, data_size);

	/* Zero the checksum byte and then compute checksum */
	elog_update_checksum(event, 0);
	elog_update_checksum(event, -(elog_checksum_event(event)));
}

static int chromeos_arm_elog_panic(struct notifier_block *this,
				   unsigned long p_event, void *ptr)
{
	struct chromeos_arm_elog_panic_buffer *buf;
	uint32_t reason = ELOG_SHUTDOWN_PANIC;
	const u8 data_size = sizeof(reason);
	union {
		struct event_header hdr;
		u8 bytes[sizeof(struct event_header) + data_size + 1];
	} event;

	buf = container_of(this, struct chromeos_arm_elog_panic_buffer, nb);
	elog_prepare_event(&event.hdr, ELOG_TYPE_OS_EVENT, &reason, data_size);
	memcpy_toio(buf->virt_addr, event.bytes, sizeof(event.bytes));

	return NOTIFY_DONE;
}

static int chromeos_arm_panic_init(struct platform_device *pdev, u32 start,
				   u32 size)
{
	int ret = -EINVAL;
	struct chromeos_arm_elog_panic_buffer *buf;

	buf = kmalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		dev_err(&pdev->dev, "failed to allocate panic notifier.\n");
		ret = -ENOMEM;
		goto fail1;
	}

	buf->start = start;
	buf->size = size;
	buf->nb.notifier_call = chromeos_arm_elog_panic;

	if (!request_mem_region(start, size, "elog panic event")) {
		dev_err(&pdev->dev, "failed to request panic event buffer.\n");
		goto fail2;
	}

	buf->virt_addr = ioremap(start, size);
	if (!buf->virt_addr) {
		dev_err(&pdev->dev, "failed to map panic event buffer.\n");
		goto fail3;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &buf->nb);

	platform_set_drvdata(pdev, buf);

	return 0;

fail3:
	release_mem_region(start, size);
fail2:
	kfree(buf);
fail1:
	return ret;
}

static int dt_gpio_init(struct platform_device *pdev, const char *of_list_name,
			const char *gpio_desc_name, const char *sysfs_name)
{
	int gpio, err;
	enum of_gpio_flags flags;
	struct device_node *np = pdev->dev.of_node;
	unsigned long gpio_flags = GPIOF_DIR_IN;

	gpio = of_get_named_gpio_flags(np, of_list_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		dev_err(&pdev->dev, "invalid %s descriptor\n", of_list_name);
		return -EINVAL;
	}

	if (flags & OF_GPIO_ACTIVE_LOW)
		gpio_flags |= GPIOF_ACTIVE_LOW;

	err = gpio_request_one(gpio, gpio_flags, gpio_desc_name);
	if (err)
		return err;

	gpio_export(gpio, 0);
	gpio_export_link(&pdev->dev, sysfs_name, gpio);
	return 0;
}

static int chromeos_arm_probe(struct platform_device *pdev)
{
	int err;
	u32 elog_panic_event[2];
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		err = -ENODEV;
		goto err;
	}

	err = dt_gpio_init(pdev, "write-protect-gpio",
			   "firmware-write-protect", "write-protect");
	if (err)
		goto err;
	err = dt_gpio_init(pdev, "recovery-switch",
			   "firmware-recovery-switch", "recovery-switch");
	err = dt_gpio_init(pdev, "developer-switch",
			   "firmware-developer-switch", "developer-switch");

	if (!of_property_read_u32_array(np, "elog-panic-event",
					elog_panic_event,
					ARRAY_SIZE(elog_panic_event))) {
		err = chromeos_arm_panic_init(pdev, elog_panic_event[0],
					      elog_panic_event[1]);
		if (err)
			goto err;
	}

	dev_info(&pdev->dev, "chromeos system detected\n");

	err = 0;
err:
	of_node_put(np);

	return err;
}

static int chromeos_arm_remove(struct platform_device *pdev)
{
	struct chromeos_arm_elog_panic_buffer *buf;

	buf = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	if (buf) {
		atomic_notifier_chain_unregister(&panic_notifier_list,
						 &buf->nb);
		release_mem_region(buf->start, buf->size);
		iounmap(buf->virt_addr);
		kfree(buf);
	}
	return 0;
}

static struct platform_driver chromeos_arm_driver = {
	.probe = chromeos_arm_probe,
	.remove = chromeos_arm_remove,
	.driver = {
		.name = "chromeos_arm",
	},
};

static int __init chromeos_arm_init(void)
{
	struct device_node *fw_dn;
	struct platform_device *pdev;

	fw_dn = of_find_compatible_node(NULL, NULL, "chromeos-firmware");
	if (!fw_dn)
		return -ENODEV;

	pdev = platform_device_register_simple("chromeos_arm", -1, NULL, 0);
	pdev->dev.of_node = fw_dn;

	platform_driver_register(&chromeos_arm_driver);

	return 0;
}
subsys_initcall(chromeos_arm_init);
