#ifndef _BP_LINUX_VERSION_H
#define _BP_LINUX_VERSION_H
#include_next <linux/version.h>

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a,b) (((a) << 8) + (b))
#endif

#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif

#define LINUX_VERSION_IS_LESS(x1,x2,x3)	(LINUX_VERSION_CODE < KERNEL_VERSION(x1,x2,x3))
#define LINUX_VERSION_IS_GEQ(x1,x2,x3)	(LINUX_VERSION_CODE >= KERNEL_VERSION(x1,x2,x3))
#define LINUX_VERSION_IN_RANGE(x1,x2,x3, y1,y2,y3) \
	(LINUX_VERSION_IS_GEQ(x1,x2,x3) && LINUX_VERSION_IS_LESS(y1,y2,y3))
#endif /* _BP_LINUX_VERSION_H */
