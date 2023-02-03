/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BACKPORTS_LINUX_KCOV_H
#define __BACKPORTS_LINUX_KCOV_H
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(4,6,0)
#include_next <linux/kcov.h>
#endif

#if LINUX_VERSION_IS_LESS(5,5,0)
#define kcov_remote_stop LINUX_BACKPORT(kcov_remote_stop)
static inline void kcov_remote_stop(void) {}
#define kcov_remote_start_common LINUX_BACKPORT(kcov_remote_start_common)
static inline void kcov_remote_start_common(u64 id) {}
#endif

#endif /* __BACKPORTS_LINUX_KCOV_H */
