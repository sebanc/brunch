#ifndef __BACKPORT_LINUX_TIME64_H
#define __BACKPORT_LINUX_TIME64_H
#if LINUX_VERSION_IS_GEQ(3,17,0)
#include_next <linux/time64.h>
#else
#include <linux/time.h>
#endif

#if LINUX_VERSION_IS_LESS(3,17,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
#define timespec64_equal		timespec_equal
#define timespec64_compare		timespec_compare
#define set_normalized_timespec64	set_normalized_timespec
#define timespec64_add_safe		timespec_add_safe
#define timespec64_add			timespec_add
#define timespec64_sub			timespec_sub
#define timespec64_valid		timespec_valid
#define timespec64_valid_strict		timespec_valid_strict
#define timespec64_to_ns		timespec_to_ns
#define ns_to_timespec64		ns_to_timespec
#define timespec64_add_ns		timespec_add_ns
#define timespec64			timespec
#endif /* LINUX_VERSION_IS_LESS(3,17,0) */

#if LINUX_VERSION_IS_LESS(3,19,0)
static inline time64_t mktime64(const unsigned int year0, const unsigned int mon0,
				const unsigned int day, const unsigned int hour,
				const unsigned int min, const unsigned int sec)
{
	unsigned int mon = mon0, year = year0;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (int) (mon -= 2)) {
		mon += 12;	/* Puts Feb last since it has leap day */
		year -= 1;
	}

	return ((((time64_t)
		  (year/4 - year/100 + year/400 + 367*mon/12 + day) +
		  year*365 - 719499
	    )*24 + hour /* now have hours - midnight tomorrow handled here */
	  )*60 + min /* now have minutes */
	)*60 + sec; /* finally seconds */
}
#endif

#endif /* __BACKPORT_LINUX_TIME64_H */
