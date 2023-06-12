/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_REFCOUNT
#include_next <linux/refcount.h>
#else
#undef refcount_warn_saturate
#define refcount_warn_saturate LINUX_BACKPORT(refcount_warn_saturate)
#undef refcount_dec_if_one
#define refcount_dec_if_one LINUX_BACKPORT(refcount_dec_if_one)
#undef refcount_dec_not_one
#define refcount_dec_not_one LINUX_BACKPORT(refcount_dec_not_one)
#undef refcount_dec_and_mutex_lock
#define refcount_dec_and_mutex_lock LINUX_BACKPORT(refcount_dec_and_mutex_lock)
#undef refcount_dec_and_lock
#define refcount_dec_and_lock LINUX_BACKPORT(refcount_dec_and_lock)
#undef refcount_dec_and_lock_irqsave
#define refcount_dec_and_lock_irqsave LINUX_BACKPORT(refcount_dec_and_lock_irqsave)
#include <linux/backport-refcount.h>
#endif /* CPTCFG_BPAUTO_REFCOUNT */
