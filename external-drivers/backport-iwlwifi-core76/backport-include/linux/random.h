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

#endif /* __BACKPORT_RANDOM_H */
