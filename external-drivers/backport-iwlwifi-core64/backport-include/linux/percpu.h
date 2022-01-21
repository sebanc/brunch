/*
 * Copyright (C) 2018 Intel Corporation
 */
#ifndef __BACKPORT_PERCPU_H
#define __BACKPORT_PERCPU_H
#include_next <linux/percpu.h>

#if LINUX_VERSION_IS_LESS(3,18,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline void __percpu *__alloc_gfp_warn(void)
{
	WARN(1, "Cannot backport alloc_percpu_gfp");
	return NULL;
}

#define alloc_percpu_gfp(type, gfp) \
	({ (gfp == GFP_KERNEL) ? alloc_percpu(type) : __alloc_gfp_warn(); })
#endif /* LINUX_VERSION_IS_LESS(3,18,0) */

#endif /* __BACKPORT_PERCPU_H */
