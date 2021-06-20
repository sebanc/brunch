/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_PM_H__
#define __INTEL_PXP_PM_H__

#include "i915_drv.h"

#ifdef CONFIG_DRM_I915_PXP
void intel_pxp_pm_prepare_suspend(struct intel_pxp *pxp);

int intel_pxp_pm_resume(struct intel_pxp *pxp);
#else
static inline void intel_pxp_pm_prepare_suspend(struct intel_pxp *pxp)
{
}

static inline int intel_pxp_pm_resume(struct intel_pxp *pxp)
{
	return 0;
}
#endif

#endif /* __INTEL_PXP_PM_H__ */
