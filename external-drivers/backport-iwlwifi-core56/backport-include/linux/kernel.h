#ifndef __BACKPORT_KERNEL_H
#define __BACKPORT_KERNEL_H
#include_next <linux/kernel.h>
#include <linux/bug.h>
#include <linux/version.h>
/*
 * some older kernels don't have this and thus don't
 * include it from kernel.h like new kernels
 */
#include <linux/printk.h>

/*
 * This backports:
 *
 *   From a3860c1c5dd1137db23d7786d284939c5761d517 Mon Sep 17 00:00:00 2001
 *   From: Xi Wang <xi.wang@gmail.com>
 *   Date: Thu, 31 May 2012 16:26:04 -0700
 *   Subject: [PATCH] introduce SIZE_MAX
 */
#ifndef SIZE_MAX
#define SIZE_MAX    (~(size_t)0)
#endif

/* This backports:
 *
 * commit 36a26c69b4c70396ef569c3452690fba0c1dec08
 * Author: Nicholas Bellinger <nab@linux-iscsi.org>
 * Date:   Tue Jul 26 00:35:26 2011 -0700
 *
 * 	kernel.h: Add DIV_ROUND_UP_ULL and DIV_ROUND_UP_SECTOR_T macro usage
 */
#ifndef DIV_ROUND_UP_ULL
#define DIV_ROUND_UP_ULL(ll,d) \
	({ unsigned long long _tmp = (ll)+(d)-1; do_div(_tmp, d); _tmp; })
#endif

#ifndef USHRT_MAX
#define USHRT_MAX      ((u16)(~0U))
#endif

#ifndef SHRT_MAX
#define SHRT_MAX       ((s16)(USHRT_MAX>>1))
#endif

#ifndef SHRT_MIN
#define SHRT_MIN       ((s16)(-SHRT_MAX - 1))
#endif

#ifndef U8_MAX
#define U8_MAX		((u8)~0U)
#endif

#ifndef S8_MAX
#define S8_MAX		((s8)(U8_MAX>>1))
#endif

#ifndef S8_MIN
#define S8_MIN		((s8)(-S8_MAX - 1))
#endif

#ifndef U16_MAX
#define U16_MAX		((u16)~0U)
#endif

#ifndef S16_MAX
#define S16_MAX		((s16)(U16_MAX>>1))
#endif

#ifndef S16_MIN
#define S16_MIN		((s16)(-S16_MAX - 1))
#endif

#ifndef U32_MAX
#define U32_MAX		((u32)~0U)
#endif

#ifndef S32_MAX
#define S32_MAX		((s32)(U32_MAX>>1))
#endif

#ifndef S32_MIN
#define S32_MIN		((s32)(-S32_MAX - 1))
#endif

#ifndef __round_mask
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))
#endif

#ifndef DIV_ROUND_CLOSEST
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(x) __x = x;				\
	typeof(divisor) __d = divisor;			\
	(((typeof(x))-1) > 0 ||				\
	 ((typeof(divisor))-1) > 0 || (__x) > 0) ?	\
		(((__x) + ((__d) / 2)) / (__d)) :	\
		(((__x) - ((__d) / 2)) / (__d));	\
}							\
)
#endif

#ifndef DIV_ROUND_CLOSEST_ULL
#define DIV_ROUND_CLOSEST_ULL(x, divisor)(		\
{							\
	typeof(divisor) __d = divisor;			\
	unsigned long long _tmp = (x) + (__d) / 2;	\
	do_div(_tmp, __d);				\
	_tmp;						\
}							\
)
#endif

#ifndef swap
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)
#endif

#ifndef lower_32_bits
#define lower_32_bits(n) ((u32)(n))
#endif

#ifndef clamp
#define clamp(val, min, max) ({			\
	typeof(val) __val = (val);		\
	typeof(min) __min = (min);		\
	typeof(max) __max = (max);		\
	(void) (&__val == &__min);		\
	(void) (&__val == &__max);		\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })
#endif

#ifndef clamp_t
#define clamp_t(type, val, min, max) ({		\
	type __val = (val);			\
	type __min = (min);			\
	type __max = (max);			\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })
#endif

#ifndef clamp_val
#define clamp_val(val, min, max) ({             \
	typeof(val) __val = (val);              \
	typeof(val) __min = (min);              \
	typeof(val) __max = (max);              \
	__val = __val < __min ? __min: __val;   \
	__val > __max ? __max: __val; })
#endif

#ifndef rounddown
#define rounddown(x, y) (				\
{							\
	typeof(x) __x = (x);				\
	__x - (__x % (y));				\
}							\
)
#endif /* rounddown */

#if LINUX_VERSION_IS_LESS(3,2,0)
#define hex_byte_pack pack_hex_byte

/* kernels before 3.2 didn't have error checking for the function */
#define hex2bin LINUX_BACKPORT(hex2bin)
int __must_check hex2bin(u8 *dst, const char *src, size_t count);
#endif /* < 3.2 */

#if LINUX_VERSION_IS_LESS(3,18,0)
#undef clamp
#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)
#endif /* < 3.18 */

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

#if LINUX_VERSION_IS_LESS(3,14,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline u32 reciprocal_scale(u32 val, u32 ep_ro)
{
	return (u32)(((u64) val * ep_ro) >> 32);
}
#endif /* LINUX_VERSION_IS_LESS(3,14,0) */

#if LINUX_VERSION_IS_LESS(3,18,0)
#define bin2hex LINUX_BACKPORT(bin2hex)
extern char *bin2hex(char *dst, const void *src, size_t count);
#endif

#endif /* __BACKPORT_KERNEL_H */

/*
 * We have to do this outside the include guard, because
 * out own header (linux/export.h) has to include kernel.h
 * indirectly (through module.h) and then undef's pr_fmt.
 * Then, when the real kernel.h gets included again, it's
 * not defined and we get problems ...
 */
#ifndef pr_fmt
#define pr_fmt(msg) msg
#endif
