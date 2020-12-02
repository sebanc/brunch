#ifndef LINUX_BACKPORTS_PRIVATE_H
#define LINUX_BACKPORTS_PRIVATE_H

#include <linux/version.h>

#ifdef CPTCFG_BPAUTO_BUILD_WANT_DEV_COREDUMP
int devcoredump_init(void);
void devcoredump_exit(void);
#else
static inline int devcoredump_init(void)
{ return 0; }
static inline void devcoredump_exit(void)
{}
#endif

#endif /* LINUX_BACKPORTS_PRIVATE_H */
