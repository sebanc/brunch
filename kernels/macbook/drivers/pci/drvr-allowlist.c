// SPDX-License-Identifier: GPL-2.0
/*
 * Allowlist of PCI drivers that are allowed to bind to external devices
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/pci.h>
#include "pci.h"

/*
 * Parameter to essentially disable allowlist code (thus allow all drivers to
 * connect to any external PCI devices).
 */
static bool trust_external_pci_devices;
core_param(trust_external_pci_devices, trust_external_pci_devices, bool, 0444);

/* Driver allowlist */
struct allowlist_entry {
	const char *drvr_name;
	struct list_head node;
};

static LIST_HEAD(allowlist);
static DECLARE_RWSEM(allowlist_sem);

#define TRUNCATED	"...<truncated>\n"

/*
 * Locks down the binding of drivers to untrusted devices
 * (No PCI drivers to bind to any new untrusted PCI device)
 */
static bool drivers_allowlist_lockdown = true;
static DECLARE_RWSEM(lockdown_sem);

static ssize_t drivers_allowlist_show(struct bus_type *bus, char *buf)
{
	size_t count = 0;
	struct allowlist_entry *entry;

	down_read(&allowlist_sem);
	list_for_each_entry(entry, &allowlist, node) {
		if (count + strlen(entry->drvr_name) + sizeof(TRUNCATED) <
		    PAGE_SIZE) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					  "%s\n", entry->drvr_name);
		} else {
			count += snprintf(buf + count, PAGE_SIZE - count,
					  TRUNCATED);
			break;
		}
	}
	up_read(&allowlist_sem);
	return count;
}

static ssize_t drivers_allowlist_store(struct bus_type *bus, const char *buf,
				       size_t count)
{
	struct allowlist_entry *entry;
	ssize_t ret = count;
	unsigned int i;
	char *drv;

	if (!count)
		return -EINVAL;

	drv = kstrndup(buf, count, GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	/* Remove any trailing white spaces */
	strim(drv);
	if (!*drv) {
		ret = -EINVAL;
		goto out_kfree;
	}

	/* Driver names cannot have special characters */
	for (i = 0; i < strlen(drv); i++)
		if (!isalnum(drv[i]) && drv[i] != '_') {
			ret = -EINVAL;
			goto out_kfree;
		}

	down_write(&allowlist_sem);

	/* Lookup in the allowlist */
	list_for_each_entry(entry, &allowlist, node)
		if (!strcmp(drv, entry->drvr_name)) {
			ret = -EEXIST;
			goto out_release_sem;
		}

	/* Add a driver to the allowlist */
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		ret = -ENOMEM;
		goto out_release_sem;
	}
	entry->drvr_name = drv;
	list_add_tail(&entry->node, &allowlist);
	up_write(&allowlist_sem);
	return ret;

out_release_sem:
	up_write(&allowlist_sem);
out_kfree:
	kfree(drv);
	return ret;
}
static BUS_ATTR_RW(drivers_allowlist);

static ssize_t drivers_allowlist_lockdown_show(struct bus_type *bus, char *buf)
{
	int ret;

	down_read(&lockdown_sem);
	ret = sprintf(buf, "%u\n", drivers_allowlist_lockdown);
	up_read(&lockdown_sem);

	return ret;
}

static ssize_t
drivers_allowlist_lockdown_store(struct bus_type *bus, const char *buf,
				 size_t count)
{
	bool lockdown, state_changed = false;
	struct pci_dev *dev = NULL;

	if (strtobool(buf, &lockdown))
		return -EINVAL;

	down_write(&lockdown_sem);
	if (drivers_allowlist_lockdown != lockdown) {
		drivers_allowlist_lockdown = lockdown;
		state_changed = true;
	}
	up_write(&lockdown_sem);

	if (state_changed && !lockdown) {
		/* Attach any devices blocked earlier, subject to allowlist */
		for_each_pci_dev(dev) {
			if (dev_is_removable(&dev->dev) &&
			    !device_attach(&dev->dev))
				pci_dbg(dev, "No driver\n");
		}
	}
	return count;
}
static BUS_ATTR_RW(drivers_allowlist_lockdown);

static int __init pci_drivers_allowlist_init(void)
{
	int ret;

	if (trust_external_pci_devices)
		return 0;

	ret = bus_create_file(&pci_bus_type, &bus_attr_drivers_allowlist);
	if (ret) {
		pr_err("%s: failed to create allowlist in sysfs\n", __func__);
		return ret;
	}

	ret = bus_create_file(&pci_bus_type,
			      &bus_attr_drivers_allowlist_lockdown);
	if (ret) {
		pr_err("%s: failed to create allowlist_lockdown\n", __func__);
		bus_remove_file(&pci_bus_type, &bus_attr_drivers_allowlist);
	}
	return ret;
}
late_initcall(pci_drivers_allowlist_init);

static bool pci_driver_is_allowed(const char *name)
{
	struct allowlist_entry *entry;

	down_read(&allowlist_sem);
	list_for_each_entry(entry, &allowlist, node) {
		if (!strcmp(name, entry->drvr_name)) {
			up_read(&allowlist_sem);
			return true;
		}
	}
	up_read(&allowlist_sem);
	return false;
}

bool pci_allowed_to_attach(struct pci_driver *drv, struct pci_dev *dev)
{
	char event[16], drvr[32], *reason;
	char *udev_env[] = { event, drvr, NULL };

	snprintf(drvr, sizeof(drvr), "DRVR=%s", drv->name);

	/* Bypass Allowlist code, if platform wants so */
	if (trust_external_pci_devices) {
		reason = "trust_external_pci_devices";
		goto allowed;
	}

	/* Allow internal devices */
	if (!dev_is_removable(&dev->dev)) {
		reason = "internal device";
		goto allowed;
	}

	/* Don't allow any driver attaches, if locked down */
	down_read(&lockdown_sem);
	if (drivers_allowlist_lockdown) {
		up_read(&lockdown_sem);
		reason = "drivers_allowlist_lockdown enforced";
		goto not_allowed;
	}
	up_read(&lockdown_sem);

	/* Allow if driver is in allowlist */
	if (pci_driver_is_allowed(drv->name)) {
		reason = "drvr in allowlist";
		goto allowed;
	}
	reason = "drvr not in allowlist";

not_allowed:
	pci_err(dev, "attach not allowed to drvr %s [%s]\n", drv->name, reason);
	snprintf(event, sizeof(event), "EVENT=BLOCKED");
	kobject_uevent_env(&dev->dev.kobj, KOBJ_CHANGE, udev_env);
	return false;

allowed:
	pci_info(dev, "attach allowed to drvr %s [%s]\n", drv->name, reason);
	snprintf(event, sizeof(event), "EVENT=ALLOWED");
	kobject_uevent_env(&dev->dev.kobj, KOBJ_CHANGE, udev_env);
	return true;
}
