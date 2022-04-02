#ifndef __BACKPORT_LINUX_TIME_H
#define __BACKPORT_LINUX_TIME_H

#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
#include_next <linux/time64.h>
#include <uapi/linux/time.h>
#endif

#include_next <linux/time.h>

#include <linux/time64.h>

#if LINUX_VERSION_IS_LESS(4,8,0)
static inline void time64_to_tm(time64_t totalsecs, int offset,
				struct tm *result)
{
	time_to_tm((time_t)totalsecs, 0, result);
}
#endif

#endif /* __BACKPORT_LINUX_TIME_H */
