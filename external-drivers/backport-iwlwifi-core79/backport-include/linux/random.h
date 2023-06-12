#ifndef __BACKPORT_RANDOM_H
#define __BACKPORT_RANDOM_H
#include_next <linux/random.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(4,11,0)
static inline u32 get_random_u32(void)
{
	return get_random_int();
}
#endif

#if LINUX_VERSION_IS_LESS(6,1,0)
static inline u16 get_random_u16(void)
{
	return get_random_int() & 0xffff;
}
#endif

#if LINUX_VERSION_IS_LESS(6,1,4)
static inline u32 get_random_u32_below(u32 ceil)
{
	return prandom_u32_max(ceil);
}

static inline u32 get_random_u32_inclusive(u32 floor, u32 ceil)
{
	BUILD_BUG_ON_MSG(__builtin_constant_p(floor) && __builtin_constant_p(ceil) &&
			 (floor > ceil || ceil - floor == U32_MAX),
			 "get_random_u32_inclusive() must take floor <= ceil");
	return floor + get_random_u32_below(ceil - floor + 1);
}
#endif

#endif /* __BACKPORT_RANDOM_H */
