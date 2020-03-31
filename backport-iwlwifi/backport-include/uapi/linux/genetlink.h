#ifndef __BACKPORT_UAPI_LINUX_GENETLINK_H
#define __BACKPORT_UAPI_LINUX_GENETLINK_H
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(3,7,0)
#include_next <uapi/linux/genetlink.h>
#else
#include_next <linux/genetlink.h>
#endif

#ifndef GENL_UNS_ADMIN_PERM
#define GENL_UNS_ADMIN_PERM GENL_ADMIN_PERM
#endif

#endif /* __BACKPORT_UAPI_LINUX_GENETLINK_H */
