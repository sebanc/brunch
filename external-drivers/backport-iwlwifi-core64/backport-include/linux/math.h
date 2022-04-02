#ifndef __BACKPORT_LINUX_MATH_H
#define __BACKPORT_LINUX_MATH_H

#if LINUX_VERSION_IS_LESS(5,11,0)
#include <linux/kernel.h>
#else /* LINUX_VERSION_IS_LESS(5,11,0) */
#include_next <linux/math.h>
#endif /* LINUX_VERSION_IS_LESS(5,11,0) */

#endif /* __BACKPORT_LINUX_MATH_H */
