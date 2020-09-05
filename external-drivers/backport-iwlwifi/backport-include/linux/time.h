#ifndef __BACKPORT_LINUX_TIME_H
#define __BACKPORT_LINUX_TIME_H

#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
#include_next <linux/time64.h>
#include <uapi/linux/time.h>
#endif

#include_next <linux/time.h>

#include <linux/time64.h>

#endif /* __BACKPORT_LINUX_TIME_H */
