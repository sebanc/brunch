/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BACKPORTS_LINUX_MATH_H
#define __BACKPORTS_LINUX_MATH_H
#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(5,11,0)
#include_next <linux/math.h>
#else
#include <linux/kernel.h>
#endif

#endif /* __BACKPORTS_LINUX_MATH_H */
