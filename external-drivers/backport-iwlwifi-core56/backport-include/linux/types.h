#ifndef __BACKPORT_TYPES
#define __BACKPORT_TYPES
#include <linux/version.h>
#include_next <linux/types.h>

#if LINUX_VERSION_IS_LESS(3,17,0)
typedef __s64 time64_t;
#endif

#if LINUX_VERSION_IS_LESS(4,16,0)
typedef unsigned __poll_t;
#endif

#endif /* __BACKPORT_TYPES */
