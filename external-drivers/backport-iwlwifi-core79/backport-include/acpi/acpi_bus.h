#ifndef _BACKPORTS_ACPI_BUS_H__
#define _BACKPORTS_ACPI_BUS_H__ 1

#include_next <acpi/acpi_bus.h>

#if LINUX_VERSION_IS_LESS(4,13,0)
static inline union acpi_object *
backport_acpi_evaluate_dsm(acpi_handle handle, const guid_t *guid, u64 rev, u64 func, union acpi_object *argv4)
{
	return acpi_evaluate_dsm(handle, guid->b, rev, func, argv4);
}
#define acpi_evaluate_dsm LINUX_BACKPORT(acpi_evaluate_dsm)
#endif /* x < 4.13.0 */

#endif /* _BACKPORTS_ACPI_BUS_H__ */
