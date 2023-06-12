#ifndef __BACKPORT_DEVICE_H_
#define __BACKPORT_DEVICE_H_
#include_next <linux/device.h>

#ifndef DEVICE_ATTR_ADMIN_RW
#define DEVICE_ATTR_ADMIN_RW(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RW_MODE(_name, 0600)
#endif

#if LINUX_VERSION_IS_LESS(6,2,0)
#define __ns_const
#else
#define __ns_const const
#endif

#endif /* __BACKPORT_DEVICE_H_ */
