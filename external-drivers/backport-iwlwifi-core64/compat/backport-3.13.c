/*
 * Copyright (c) 2013  Hauke Mehrtens <hauke@hauke-m.de>
 * Copyright (c) 2013  Hannes Frederic Sowa <hannes@stressinduktion.org>
 * Copyright (c) 2014  Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * Backport functionality introduced in Linux 3.13.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/net.h>

#ifdef __BACKPORT_NET_GET_RANDOM_ONCE
struct __net_random_once_work {
	struct work_struct work;
	struct static_key *key;
};

static void __net_random_once_deferred(struct work_struct *w)
{
	struct __net_random_once_work *work =
		container_of(w, struct __net_random_once_work, work);
	if (!static_key_enabled(work->key))
		static_key_slow_inc(work->key);
	kfree(work);
}

static void __net_random_once_disable_jump(struct static_key *key)
{
	struct __net_random_once_work *w;

	w = kmalloc(sizeof(*w), GFP_ATOMIC);
	if (!w)
		return;

	INIT_WORK(&w->work, __net_random_once_deferred);
	w->key = key;
	schedule_work(&w->work);
}

bool __net_get_random_once(void *buf, int nbytes, bool *done,
			   struct static_key *done_key)
{
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	if (*done) {
		spin_unlock_irqrestore(&lock, flags);
		return false;
	}

	get_random_bytes(buf, nbytes);
	*done = true;
	spin_unlock_irqrestore(&lock, flags);

	__net_random_once_disable_jump(done_key);

	return true;
}
EXPORT_SYMBOL_GPL(__net_get_random_once);
#endif /* __BACKPORT_NET_GET_RANDOM_ONCE */

#ifdef CONFIG_PCI
#define pci_bus_read_dev_vendor_id LINUX_BACKPORT(pci_bus_read_dev_vendor_id)
static bool pci_bus_read_dev_vendor_id(struct pci_bus *bus, int devfn, u32 *l,
				int crs_timeout)
{
	int delay = 1;

	if (pci_bus_read_config_dword(bus, devfn, PCI_VENDOR_ID, l))
		return false;

	/* some broken boards return 0 or ~0 if a slot is empty: */
	if (*l == 0xffffffff || *l == 0x00000000 ||
	    *l == 0x0000ffff || *l == 0xffff0000)
		return false;

	/*
	 * Configuration Request Retry Status.  Some root ports return the
	 * actual device ID instead of the synthetic ID (0xFFFF) required
	 * by the PCIe spec.  Ignore the device ID and only check for
	 * (vendor id == 1).
	 */
	while ((*l & 0xffff) == 0x0001) {
		if (!crs_timeout)
			return false;

		msleep(delay);
		delay *= 2;
		if (pci_bus_read_config_dword(bus, devfn, PCI_VENDOR_ID, l))
			return false;
		/* Card hasn't responded in 60 seconds?  Must be stuck. */
		if (delay > crs_timeout) {
			printk(KERN_WARNING "pci %04x:%02x:%02x.%d: not responding\n",
			       pci_domain_nr(bus), bus->number, PCI_SLOT(devfn),
			       PCI_FUNC(devfn));
			return false;
		}
	}

	return true;
}

bool pci_device_is_present(struct pci_dev *pdev)
{
	u32 v;

	return pci_bus_read_dev_vendor_id(pdev->bus, pdev->devfn, &v, 0);
}
EXPORT_SYMBOL_GPL(pci_device_is_present);
#endif /* CONFIG_PCI */

#ifdef CONFIG_HWMON
struct device*
hwmon_device_register_with_groups(struct device *dev, const char *name,
				  void *drvdata,
				  const struct attribute_group **groups)
{
	struct device *hwdev;

	hwdev = hwmon_device_register(dev);
	hwdev->groups = groups;
	dev_set_drvdata(hwdev, drvdata);
	return hwdev;
}

static void devm_hwmon_release(struct device *dev, void *res)
{
	struct device *hwdev = *(struct device **)res;

	hwmon_device_unregister(hwdev);
}

struct device *
devm_hwmon_device_register_with_groups(struct device *dev, const char *name,
				       void *drvdata,
				       const struct attribute_group **groups)
{
	struct device **ptr, *hwdev;

	if (!dev)
		return ERR_PTR(-EINVAL);

	ptr = devres_alloc(devm_hwmon_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	hwdev = hwmon_device_register_with_groups(dev, name, drvdata, groups);
	if (IS_ERR(hwdev))
		goto error;

	*ptr = hwdev;
	devres_add(dev, ptr);
	return hwdev;

error:
	devres_free(ptr);
	return hwdev;
}
EXPORT_SYMBOL_GPL(devm_hwmon_device_register_with_groups);
#endif
