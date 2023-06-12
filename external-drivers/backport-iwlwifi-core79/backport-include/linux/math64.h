#ifndef __BACKPORT_LINUX_MATH64_H
#define __BACKPORT_LINUX_MATH64_H
#include_next <linux/math64.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,9,0)
#define mul_u64_u64_div_u64 LINUX_BACKPORT(mul_u64_u64_div_u64)
extern u64 mul_u64_u64_div_u64(u64 a, u64 b, u64 c);
#endif /* < 5.9 */

#endif /* __BACKPORT_LINUX_MATH64_H */
