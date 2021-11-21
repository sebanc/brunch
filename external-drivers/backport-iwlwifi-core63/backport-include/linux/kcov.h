#ifndef __BACKPORT_LINUX_KCOV
#define __BACKPORT_LINUX_KCOV

#if LINUX_VERSION_IS_LESS(5,5,0)
#define kcov_remote_start_common(id) do {} while (0)
#define kcov_remote_stop() do {} while (0)
#else /* LINUX_VERSION_IS_LESS(5,5,0) */
#include_next <linux/kcov.h>
#endif /* LINUX_VERSION_IS_LESS(5,5,0) */

#endif /* __BACKPORT_LINUX_KCOV */
