#ifndef __BACKPORT_LINUX_PROPERTY_H_
#define __BACKPORT_LINUX_PROPERTY_H_
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(3,18,17) || \
	RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
#include_next <linux/property.h>
#endif

#if LINUX_VERSION_IS_LESS(4,3,0)

#define device_get_mac_address LINUX_BACKPORT(device_get_mac_address)
void *device_get_mac_address(struct device *dev, char *addr, int alen);

#endif /* < 4.3 */

#endif /* __BACKPORT_LINUX_PROPERTY_H_ */
