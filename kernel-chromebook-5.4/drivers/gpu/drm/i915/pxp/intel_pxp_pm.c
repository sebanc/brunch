// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020 Intel Corporation.
 */

#include "intel_pxp_context.h"
#include "intel_pxp_arb.h"
#include "intel_pxp_pm.h"
#include "intel_pxp_sm.h"

void intel_pxp_pm_prepare_suspend(struct intel_pxp *pxp)
{
	if (pxp->ctx.id == 0)
		return;

	mutex_lock(&pxp->ctx.mutex);

	/* Disable PXP-IOCTLs */
	pxp->ctx.global_state_in_suspend = true;

	mutex_unlock(&pxp->ctx.mutex);
}

int intel_pxp_pm_resume(struct intel_pxp *pxp)
{
	int ret = 0;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	if (pxp->ctx.id == 0)
		return 0;

	mutex_lock(&pxp->ctx.mutex);

	/* Re-enable PXP-IOCTLs */
	if (pxp->ctx.global_state_in_suspend) {
		/* reset the attacked flag even there was a pending */
		pxp->ctx.global_state_attacked = false;

		pxp->ctx.flag_display_hm_surface_keys = false;

		ret = intel_pxp_sm_terminate_all_sessions(pxp, SESSION_TYPE_TYPE0);
		if (ret) {
			drm_err(&gt->i915->drm, "Failed to terminate the sessions\n");
			goto end;
		}

		ret = intel_pxp_arb_terminate_session_with_global_terminate(pxp);
		if (ret) {
			drm_err(&gt->i915->drm, "Failed to terminate the arb session\n");
			goto end;
		}

		pxp->ctx.global_state_in_suspend = false;
	}

end:
	mutex_unlock(&pxp->ctx.mutex);

	return ret;
}
