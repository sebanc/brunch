#ifndef __BACKPORT_LINUX_TIME_H
#define __BACKPORT_LINUX_TIME_H
#include_next <linux/time.h>

#include <linux/time64.h>

#if LINUX_VERSION_IS_LESS(4,8,0)
static inline void time64_to_tm(time64_t totalsecs, int offset,
				struct tm *result)
{
	time_to_tm((time_t)totalsecs, 0, result);
}
#endif

#ifndef time_after32
/**
 * time_after32 - compare two 32-bit relative times
 * @a:	the time which may be after @b
 * @b:	the time which may be before @a
 *
 * time_after32(a, b) returns true if the time @a is after time @b.
 * time_before32(b, a) returns true if the time @b is before time @a.
 *
 * Similar to time_after(), compare two 32-bit timestamps for relative
 * times.  This is useful for comparing 32-bit seconds values that can't
 * be converted to 64-bit values (e.g. due to disk format or wire protocol
 * issues) when it is known that the times are less than 68 years apart.
 */
#define time_after32(a, b)	((s32)((u32)(b) - (u32)(a)) < 0)
#endif

#endif /* __BACKPORT_LINUX_TIME_H */
