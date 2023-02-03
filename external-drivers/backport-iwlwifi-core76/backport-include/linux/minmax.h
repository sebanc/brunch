#ifndef __BACKPORT_MINMAX_H
#define __BACKPORT_MINMAX_H
#if LINUX_VERSION_IS_LESS(5,10,0)
#include <linux/kernel.h>
#else
#include_next <linux/minmax.h>
#endif
#endif /* __BACKPORT_MINMAX_H */
