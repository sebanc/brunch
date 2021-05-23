/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_BUILD_CRYPTO_LIB_ARC4
#include_next <crypto/arc4.h>
#else
#undef arc4_setkey
#define arc4_setkey LINUX_BACKPORT(arc4_setkey)
#undef arc4_crypt
#define arc4_crypt LINUX_BACKPORT(arc4_crypt)
#include <crypto/backport-arc4.h>
#endif /* CPTCFG_BPAUTO_BUILD_CRYPTO_LIB_ARC4 */
