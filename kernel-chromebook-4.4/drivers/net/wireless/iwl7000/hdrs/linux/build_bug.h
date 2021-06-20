#ifndef __BP_BUILD_BUG_H
#define __BP_BUILD_BUG_H

#if LINUX_VERSION_IS_GEQ(4,13,0)
#include_next <linux/build_bug.h>
#else /* LINUX_VERSION_IS_GEQ(4,13,0) */
#include <linux/bug.h>
#endif /* LINUX_VERSION_IS_GEQ(4,13,0) */

#ifndef static_assert
/**
 * static_assert - check integer constant expression at build time
 *
 * static_assert() is a wrapper for the C11 _Static_assert, with a
 * little macro magic to make the message optional (defaulting to the
 * stringification of the tested expression).
 *
 * Contrary to BUILD_BUG_ON(), static_assert() can be used at global
 * scope, but requires the expression to be an integer constant
 * expression (i.e., it is not enough that __builtin_constant_p() is
 * true for expr).
 *
 * Also note that BUILD_BUG_ON() fails the build if the condition is
 * true, while static_assert() fails the build if the expression is
 * false.
 */
#define static_assert(expr, ...) __static_assert(expr, ##__VA_ARGS__, #expr)
#define __static_assert(expr, msg, ...) _Static_assert(expr, msg)
#endif

#endif /* __BP_BUILD_BUG_H */
