#ifndef __BACKPORT_ASM_GENERIC_UNALIGNED_H
#define __BACKPORT_ASM_GENERIC_UNALIGNED_H
#include_next <asm/unaligned.h>

#if LINUX_VERSION_IS_LESS(5,7,0)
static inline u32 __get_unaligned_be24(const u8 *p)
{
	return p[0] << 16 | p[1] << 8 | p[2];
}

static inline u32 get_unaligned_be24(const void *p)
{
	return __get_unaligned_be24(p);
}
#endif /* < 5.7 */

#endif /* __BACKPORT_ASM_GENERIC_UNALIGNED_H */
