#ifndef __BACKPORT_ASM_ATOMIC_H
#define __BACKPORT_ASM_ATOMIC_H
#include_next <asm/atomic.h>
#include <linux/version.h>
#include <asm/barrier.h>

#if LINUX_VERSION_IS_LESS(3,1,0)
/*
 * In many versions, several architectures do not seem to include an
 * atomic64_t implementation, and do not include the software emulation from
 * asm-generic/atomic64_t.
 * Detect and handle this here.
 */
#if (!defined(ATOMIC64_INIT) && !defined(CONFIG_X86) && !(defined(CONFIG_ARM) && !defined(CONFIG_GENERIC_ATOMIC64)))
#include <asm-generic/atomic64.h>
#endif
#endif

#endif /* __BACKPORT_ASM_ATOMIC_H */
