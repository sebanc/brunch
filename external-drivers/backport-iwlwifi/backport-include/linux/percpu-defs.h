#ifndef __BACKPORT_PERCPU_DEFS_H
#define __BACKPORT_PERCPU_DEFS_H
#include_next <linux/percpu-defs.h>

/* override this with the array-safe version */
#undef __verify_pcpu_ptr
#define __verify_pcpu_ptr(ptr)						\
do {									\
	const void __percpu *__vpp_verify = (typeof((ptr) + 0))NULL;	\
	(void)__vpp_verify;						\
} while (0)

#endif /* __BACKPORT_PERCPU_DEFS_H */
