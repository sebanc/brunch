#ifndef __BACKPORT_LINUX_ACPI_H
#define __BACKPORT_LINUX_ACPI_H
#include_next <linux/acpi.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,13,0)
#define devm_acpi_dev_add_driver_gpios LINUX_BACKPORT(devm_acpi_dev_add_driver_gpios)
static inline int devm_acpi_dev_add_driver_gpios(struct device *dev,
			      const struct acpi_gpio_mapping *gpios)
{
	return -ENXIO;
}
#endif /* LINUX_VERSION_IS_LESS(4,13,0) */

#endif /* __BACKPORT_LINUX_ACPI_H */
