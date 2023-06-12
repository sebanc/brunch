#ifndef __MBEDTLS_PLATFORM_H
#define __MBEDTLS_PLATFORM_H
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#define mbedtls_printf		pr_debug
#define mbedtls_calloc(a, b)	kcalloc(a, b, GFP_KERNEL)
#define mbedtls_free		kfree
#define mbedtls_snprintf	snprintf

#endif /* __MBEDTLS_PLATFORM_H */
