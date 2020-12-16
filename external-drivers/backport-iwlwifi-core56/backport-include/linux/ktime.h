#ifndef __BACKPORT_LINUX_KTIME_H
#define __BACKPORT_LINUX_KTIME_H
#include_next <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/version.h>

#if  LINUX_VERSION_IS_LESS(3,17,0) && \
     RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)

#define ktime_get_raw LINUX_BACKPORT(ktime_get_raw)
extern ktime_t ktime_get_raw(void);

#endif /* < 3.17 */

#ifndef ktime_to_timespec64
/* Map the ktime_t to timespec conversion to ns_to_timespec function */
#define ktime_to_timespec64(kt)		ns_to_timespec64((kt).tv64)
#endif

#endif /* __BACKPORT_LINUX_KTIME_H */
