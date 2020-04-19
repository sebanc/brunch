#ifndef __BP_BUILD_BUG_H
#define __BP_BUILD_BUG_H

#if LINUX_VERSION_IS_GEQ(4,13,0)
#include_next <linux/build_bug.h>
#else /* LINUX_VERSION_IS_GEQ(4,13,0) */
#include <linux/bug.h>
#endif /* LINUX_VERSION_IS_GEQ(4,13,0) */

#endif /* __BP_BUILD_BUG_H */
