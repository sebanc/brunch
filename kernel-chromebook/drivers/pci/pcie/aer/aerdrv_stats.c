// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Google Inc, All Rights Reserved.
 *
 * AER Statistics - exposed to userspace via /sysfs attributes.
 */

#include <linux/pci.h>
#include "aerdrv.h"

/* AER stats for the device */
struct aer_stats {

	/*
	 * Fields for all AER capable devices. They indicate the errors
	 * "as seen by this device". Note that this may mean that if an
	 * end point is causing problems, the AER counters may increment
	 * at its link partner (e.g. root port) because the errors will be
	 * "seen" by the link partner and not the the problematic end point
	 * itself (which may report all counters as 0 as it never saw any
	 * problems).
	 */
	/* Counters for different type of correctable errors */
	u64 dev_cor_errs[AER_MAX_TYPEOF_COR_ERRS];
	/* Counters for different type of fatal uncorrectable errors */
	u64 dev_fatal_errs[AER_MAX_TYPEOF_UNCOR_ERRS];
	/* Counters for different type of nonfatal uncorrectable errors */
	u64 dev_nonfatal_errs[AER_MAX_TYPEOF_UNCOR_ERRS];
	/* Total number of ERR_COR sent by this device */
	u64 dev_total_cor_errs;
	/* Total number of ERR_FATAL sent by this device */
	u64 dev_total_fatal_errs;
	/* Total number of ERR_NONFATAL sent by this device */
	u64 dev_total_nonfatal_errs;

	/*
	 * Fields for Root ports & root complex event collectors only, these
	 * indicate the total number of ERR_COR, ERR_FATAL, and ERR_NONFATAL
	 * messages received by the root port / event collector, INCLUDING the
	 * ones that are generated internally (by the rootport itself)
	 */
	u64 rootport_total_cor_errs;
	u64 rootport_total_fatal_errs;
	u64 rootport_total_nonfatal_errs;
};


void pci_aer_init(struct pci_dev *dev)
{
	dev->aer_cap = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);

	if (dev->aer_cap)
		dev->aer_stats = kzalloc(sizeof(struct aer_stats), GFP_KERNEL);

	pci_cleanup_aer_error_status_regs(dev);
}

void pci_aer_exit(struct pci_dev *dev)
{
	kfree(dev->aer_stats);
	dev->aer_stats = NULL;
}

#define aer_stats_dev_attr(name, stats_array, strings_array,		\
			   total_string, total_field)			\
	static ssize_t							\
	name##_show(struct device *dev, struct device_attribute *attr,	\
		     char *buf)						\
{									\
	unsigned int i;							\
	char *str = buf;						\
	struct pci_dev *pdev = to_pci_dev(dev);				\
	u64 *stats = pdev->aer_stats->stats_array;			\
									\
	for (i = 0; i < ARRAY_SIZE(strings_array); i++) {		\
		if (strings_array[i])					\
			str += sprintf(str, "%s %llu\n",		\
				       strings_array[i], stats[i]);	\
		else if (stats[i])					\
			str += sprintf(str, #stats_array "_bit[%d] %llu\n",\
				       i, stats[i]);			\
	}								\
	str += sprintf(str, "TOTAL_%s %llu\n", total_string,		\
		       pdev->aer_stats->total_field);			\
	return str-buf;							\
}									\
static DEVICE_ATTR_RO(name)

aer_stats_dev_attr(aer_dev_correctable, dev_cor_errs,
		   aer_correctable_error_string, "ERR_COR",
		   dev_total_cor_errs);
aer_stats_dev_attr(aer_dev_fatal, dev_fatal_errs,
		   aer_uncorrectable_error_string, "ERR_FATAL",
		   dev_total_fatal_errs);
aer_stats_dev_attr(aer_dev_nonfatal, dev_nonfatal_errs,
		   aer_uncorrectable_error_string, "ERR_NONFATAL",
		   dev_total_nonfatal_errs);

#define aer_stats_rootport_attr(name, field)				\
	static ssize_t							\
	name##_show(struct device *dev, struct device_attribute *attr,	\
		     char *buf)						\
{									\
	struct pci_dev *pdev = to_pci_dev(dev);				\
	return sprintf(buf, "%llu\n", pdev->aer_stats->field);		\
}									\
static DEVICE_ATTR_RO(name)

aer_stats_rootport_attr(aer_rootport_total_err_cor,
			 rootport_total_cor_errs);
aer_stats_rootport_attr(aer_rootport_total_err_fatal,
			 rootport_total_fatal_errs);
aer_stats_rootport_attr(aer_rootport_total_err_nonfatal,
			 rootport_total_nonfatal_errs);

static struct attribute *aer_stats_attrs[] __ro_after_init = {
	&dev_attr_aer_dev_correctable.attr,
	&dev_attr_aer_dev_fatal.attr,
	&dev_attr_aer_dev_nonfatal.attr,
	&dev_attr_aer_rootport_total_err_cor.attr,
	&dev_attr_aer_rootport_total_err_fatal.attr,
	&dev_attr_aer_rootport_total_err_nonfatal.attr,
	NULL
};

static umode_t aer_stats_attrs_are_visible(struct kobject *kobj,
					   struct attribute *a, int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct pci_dev *pdev = to_pci_dev(dev);

	if (!pdev->aer_stats)
		return 0;

	if ((a == &dev_attr_aer_rootport_total_err_cor.attr ||
	     a == &dev_attr_aer_rootport_total_err_fatal.attr ||
	     a == &dev_attr_aer_rootport_total_err_nonfatal.attr) &&
	    pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT)
		return 0;

	return a->mode;
}

const struct attribute_group aer_stats_attr_group = {
	.attrs  = aer_stats_attrs,
	.is_visible = aer_stats_attrs_are_visible,
};

void pci_dev_aer_stats_incr(struct pci_dev *pdev,
			    struct aer_err_info *info)
{
	int status, i, max = -1;
	u64 *counter = NULL;
	struct aer_stats *aer_stats = pdev->aer_stats;

	if (!aer_stats)
		return;

	switch (info->severity) {
	case AER_CORRECTABLE:
		aer_stats->dev_total_cor_errs++;
		counter = &aer_stats->dev_cor_errs[0];
		max = AER_MAX_TYPEOF_COR_ERRS;
		break;
	case AER_NONFATAL:
		aer_stats->dev_total_nonfatal_errs++;
		counter = &aer_stats->dev_nonfatal_errs[0];
		max = AER_MAX_TYPEOF_UNCOR_ERRS;
		break;
	case AER_FATAL:
		aer_stats->dev_total_fatal_errs++;
		counter = &aer_stats->dev_fatal_errs[0];
		max = AER_MAX_TYPEOF_UNCOR_ERRS;
		break;
	}

	status = (info->status & ~info->mask);
	for (i = 0; i < max; i++)
		if (status & (1 << i))
			counter[i]++;
}

void pci_rootport_aer_stats_incr(struct pci_dev *pdev,
				 struct aer_err_source *e_src)
{
	struct aer_stats *aer_stats = pdev->aer_stats;

	if (!aer_stats)
		return;

	if (e_src->status & PCI_ERR_ROOT_COR_RCV)
		aer_stats->rootport_total_cor_errs++;

	if (e_src->status & PCI_ERR_ROOT_UNCOR_RCV) {
		if (e_src->status & PCI_ERR_ROOT_FATAL_RCV)
			aer_stats->rootport_total_fatal_errs++;
		else
			aer_stats->rootport_total_nonfatal_errs++;
	}
}
