#ifndef __BACKPORT_TIMEKEEPING_H
#define __BACKPORT_TIMEKEEPING_H
#include <linux/version.h>
#include <linux/types.h>

#if LINUX_VERSION_IS_GEQ(3,17,0) || \
	RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
#include_next <linux/timekeeping.h>
#endif

#if LINUX_VERSION_IS_LESS(3,17,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
#define ktime_get_ns LINUX_BACKPORT(ktime_get_ns)
extern ktime_t ktime_get(void);
#define ktime_get_ns LINUX_BACKPORT(ktime_get_ns)
static inline u64 ktime_get_ns(void)
{
	return ktime_to_ns(ktime_get());
}

extern ktime_t ktime_get_boottime(void);
#define ktime_get_boot_ns LINUX_BACKPORT(ktime_get_boot_ns)
static inline u64 ktime_get_boot_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}
#endif /* < 3.17 */

#if LINUX_VERSION_IS_GEQ(5,3,0)
/*
 * In v5.3, this function was renamed, so rename it here for v5.3+.
 * When we merge v5.3 back from upstream, the opposite should be done
 * (i.e. we will have _boottime_ and need to rename to _boot_ in <
 * v5.3 instead).
*/
#define ktime_get_boot_ns ktime_get_boottime_ns
#endif /* > 5.3.0 */

#if LINUX_VERSION_IS_LESS(4,18,0)
extern time64_t ktime_get_boottime_seconds(void);
#endif /* < 4.18 */

#if LINUX_VERSION_IS_LESS(3,19,0)
static inline time64_t ktime_get_seconds(void)
{
	struct timespec t;

	ktime_get_ts(&t);

	return t.tv_sec;
}

static inline time64_t ktime_get_real_seconds(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return tv.tv_sec;
}
#endif

#if LINUX_VERSION_IS_LESS(3,17,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline void ktime_get_ts64(struct timespec64 *ts)
{
	ktime_get_ts(ts);
}
#endif

#if LINUX_VERSION_IS_LESS(3,17,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
/* This was introduced in 4.15, but we only need it in the
 * ktime_get_raw_ts64 backport() for < 3.17.
 */
#if __BITS_PER_LONG == 64
static inline struct timespec64 timespec_to_timespec64(const struct timespec ts)
{
	return *(const struct timespec64 *)&ts;
}

#else
static inline struct timespec64 timespec_to_timespec64(const struct timespec ts)
{
	struct timespec64 ret;

	ret.tv_sec = ts.tv_sec;
	ret.tv_nsec = ts.tv_nsec;
	return ret;
}
#endif
#endif /* < 3.17 */

#if LINUX_VERSION_IS_LESS(4,18,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
#define ktime_get_raw_ts64 LINUX_BACKPORT(ktime_get_raw_ts64)
static inline void ktime_get_raw_ts64(struct timespec64 *ts)
{
#if LINUX_VERSION_IS_LESS(3,19,0)
	struct timespec64 ts64;

	getrawmonotonic(&ts64);
	*ts = timespec_to_timespec64(ts64);
#else
	return getrawmonotonic64(ts);
#endif /* < 3.19 */
}
#endif

#endif /* __BACKPORT_TIMEKEEPING_H */
