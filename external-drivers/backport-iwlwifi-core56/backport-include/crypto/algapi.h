#ifndef __BP_ALGAPI_H
#define __BP_ALGAPI_H
#include <linux/version.h>
#include_next <crypto/algapi.h>

#if LINUX_VERSION_IS_LESS(3,13,0)
#define __crypto_memneq LINUX_BACKPORT(__crypto_memneq)
noinline unsigned long __crypto_memneq(const void *a, const void *b, size_t size);
#define crypto_memneq LINUX_BACKPORT(crypto_memneq)
static inline int crypto_memneq(const void *a, const void *b, size_t size)
{
        return __crypto_memneq(a, b, size) != 0UL ? 1 : 0;
}
#endif

#endif /* __BP_ALGAPI_H */
