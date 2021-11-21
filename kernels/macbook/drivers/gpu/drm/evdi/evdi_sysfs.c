// SPDX-License-Identifier: GPL-2.0-only
/*
 * evdi_sysfs.c
 *
 * Copyright (c) 2020 DisplayLink (UK) Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <linux/device.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "evdi_sysfs.h"
#include "evdi_params.h"
#include "evdi_debug.h"
#include "evdi_platform_drv.h"

#define MAX_EVDI_USB_ADDR 10

static ssize_t version_show(__always_unused struct device *dev,
			    __always_unused struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u.%u.%u\n", DRIVER_MAJOR,
			DRIVER_MINOR, DRIVER_PATCH);
}

static ssize_t count_show(__always_unused struct device *dev,
			  __always_unused struct device_attribute *attr,
			  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", evdi_platform_device_count(dev));
}

struct evdi_usb_addr {
	int addr[MAX_EVDI_USB_ADDR];
	int len;
	struct usb_device *usb;
};

static int evdi_platform_device_attach(struct device *device,
		struct evdi_usb_addr *parent_addr);

static ssize_t add_device_with_usb_path(struct device *dev,
			 const char *buf, size_t count)
{
	char *usb_path = kstrdup(buf, GFP_KERNEL);
	char *temp_path = usb_path;
	char *bus_token;
	char *usb_token;
	char *usb_token_copy = NULL;
	char *token;
	char *bus;
	char *port;
	struct evdi_usb_addr usb_addr;

	if (!usb_path)
		return -ENOMEM;

	memset(&usb_addr, 0, sizeof(usb_addr));
	temp_path = strnstr(temp_path, "usb:", count);
	if (!temp_path)
		goto err_parse_usb_path;


	temp_path = strim(temp_path);

	bus_token = strsep(&temp_path, ":");
	if (!bus_token)
		goto err_parse_usb_path;

	usb_token = strsep(&temp_path, ":");
	if (!usb_token)
		goto err_parse_usb_path;

	/* Separate trailing ':*' from usb_token */
	strsep(&temp_path, ":");

	token = usb_token_copy = kstrdup(usb_token, GFP_KERNEL);
	bus = strsep(&token, "-");
	if (!bus)
		goto err_parse_usb_path;
	if (kstrtouint(bus, 10, &usb_addr.addr[usb_addr.len++]))
		goto err_parse_usb_path;

	do {
		port = strsep(&token, ".");
		if (!port)
			goto err_parse_usb_path;
		if (kstrtouint(port, 10, &usb_addr.addr[usb_addr.len++]))
			goto err_parse_usb_path;
	} while (token && port && usb_addr.len < MAX_EVDI_USB_ADDR);

	if (evdi_platform_device_attach(dev, &usb_addr) != 0) {
		EVDI_ERROR("Unable to attach to: %s\n", buf);
		kfree(usb_path);
		kfree(usb_token_copy);
		return -EINVAL;
	}

	EVDI_INFO("Attaching to %s:%s\n", bus_token, usb_token);
	kfree(usb_path);
	kfree(usb_token_copy);
	return count;

err_parse_usb_path:
	EVDI_ERROR("Unable to parse usb path: %s", buf);
	kfree(usb_path);
	kfree(usb_token_copy);
	return -EINVAL;
}

static int find_usb_device_at_path(struct usb_device *usb, void *data)
{
	struct evdi_usb_addr *find_path = (struct evdi_usb_addr *)(data);
	struct usb_device *pdev = usb;
	int port = 0;
	int i;

	i = find_path->len - 1;
	while (pdev != NULL && i >= 0 && i < MAX_EVDI_USB_ADDR) {
		port = pdev->portnum;
		if (port == 0)
			port = pdev->bus->busnum;

		if (port != find_path->addr[i])
			return 0;

		if (pdev->parent == NULL && i == 0) {
			find_path->usb = usb;
			return 1;
		}
		pdev = pdev->parent;
		i--;
	}

	return 0;
}

static int evdi_platform_device_attach(struct device *device,
		struct evdi_usb_addr *parent_addr)
{
	struct device *parent = NULL;

	if (!parent_addr)
		return -EINVAL;

	if (!usb_for_each_dev(parent_addr, find_usb_device_at_path) ||
	    !parent_addr->usb)
		return -EINVAL;

	parent = &parent_addr->usb->dev;
	return evdi_platform_device_add(device, parent);
}

static ssize_t add_store(struct device *dev,
			 __always_unused struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int val;
	int ret;

	if (strnstr(buf, "usb:", count))
		return add_device_with_usb_path(dev, buf, count);

	if (kstrtouint(buf, 10, &val)) {
		EVDI_ERROR("Invalid device count \"%s\"\n", buf);
		return -EINVAL;
	}

	ret = evdi_platform_add_devices(dev, val);
	if (ret)
		return ret;

	return count;
}

static ssize_t remove_all_store(struct device *dev,
				__always_unused struct device_attribute *attr,
				__always_unused const char *buf,
				size_t count)
{
	evdi_platform_remove_all_devices(dev);
	return count;
}

static ssize_t loglevel_show(__always_unused struct device *dev,
			     __always_unused struct device_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", evdi_loglevel);
}

static ssize_t loglevel_store(__always_unused struct device *dev,
			      __always_unused struct device_attribute *attr,
			      const char *buf,
			      size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val)) {
		EVDI_ERROR("Unable to parse %u\n", val);
		return -EINVAL;
	}
	if (val > EVDI_LOGLEVEL_VERBOSE) {
		EVDI_ERROR("Invalid loglevel %u\n", val);
		return -EINVAL;
	}

	EVDI_INFO("Setting loglevel to %u\n", val);
	evdi_loglevel = val;
	return count;
}

static struct device_attribute evdi_device_attributes[] = {
	__ATTR_RO(count),
	__ATTR_RO(version),
	__ATTR_RW(loglevel),
	__ATTR_WO(add),
	__ATTR_WO(remove_all)
};

void evdi_sysfs_init(struct device *root)
{
	int i;

	if (!PTR_ERR_OR_ZERO(root))
		for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++)
			device_create_file(root, &evdi_device_attributes[i]);
}

void evdi_sysfs_exit(struct device *root)
{
	int i;

	if (PTR_ERR_OR_ZERO(root)) {
		EVDI_ERROR("root device is null");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++)
		device_remove_file(root, &evdi_device_attributes[i]);
}

