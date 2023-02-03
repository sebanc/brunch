#ifndef __BACKPORT_KERNEL_H
#define __BACKPORT_KERNEL_H
#include_next <linux/kernel.h>
/* needed to include BUILD_BUG_ON_ZERO on <= 4.12 */
#include <linux/bug.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,17,0)

/*
 * This returns a constant expression while determining if an argument is
 * a constant expression, most importantly without evaluating the argument.
 * Glory to Martin Uecker <Martin.Uecker@med.uni-goettingen.de>
 */
#define __is_constexpr(x) \
	(sizeof(int) == sizeof(*(8 ? ((void *)((long)(x) * 0l)) : (int *)8)))

#endif /* < 4.17.0 */

#if LINUX_VERSION_IS_LESS(4,6,0)
#define kstrtobool LINUX_BACKPORT(kstrtobool)
int __must_check kstrtobool(const char *s, bool *res);
#define kstrtobool_from_user LINUX_BACKPORT(kstrtobool_from_user)
int __must_check kstrtobool_from_user(const char __user *s, size_t count, bool *res);
#endif

#if LINUX_VERSION_IS_LESS(4,5,0)

#undef abs
/**
 * abs - return absolute value of an argument
 * @x: the value.  If it is unsigned type, it is converted to signed type first.
 *     char is treated as if it was signed (regardless of whether it really is)
 *     but the macro's return type is preserved as char.
 *
 * Return: an absolute value of x.
 */
#define abs(x)	__abs_choose_expr(x, long long,				\
		__abs_choose_expr(x, long,				\
		__abs_choose_expr(x, int,				\
		__abs_choose_expr(x, short,				\
		__abs_choose_expr(x, char,				\
		__builtin_choose_expr(					\
			__builtin_types_compatible_p(typeof(x), char),	\
			(char)({ signed char __x = (x); __x<0?-__x:__x; }), \
			((void)0)))))))

#define __abs_choose_expr(x, type, other) __builtin_choose_expr(	\
	__builtin_types_compatible_p(typeof(x),   signed type) ||	\
	__builtin_types_compatible_p(typeof(x), unsigned type),		\
	({ signed type __x = (x); __x < 0 ? -__x : __x; }), other)

#endif

#endif /* __BACKPORT_KERNEL_H */
