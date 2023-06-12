#ifndef __BACKPORT_LINUX_BITS_H
#define __BACKPORT_LINUX_BITS_H
#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(4,19,0) || \
    LINUX_VERSION_IN_RANGE(4,14,119, 4,15,0) || \
    LINUX_VERSION_IN_RANGE(4,9,176, 4,10,0) || \
    LINUX_VERSION_IN_RANGE(4,4,180, 4,5,0)
#include_next <linux/bits.h>
#else
#include <linux/bitops.h>
#endif /* >= 4.19 */

#endif /* __BACKPORT_LINUX_BITS_H */
