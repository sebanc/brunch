// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#include "gt/intel_context.h"
#include "gt/intel_engine_pm.h"

#include "intel_pxp_arb.h"
#include "intel_pxp.h"
#include "intel_pxp_context.h"
#include "intel_pxp_tee.h"
#include "intel_pxp_cmd.h"

/* Arbitrary session */
#define ARB_SESSION_INDEX 0xf
#define ARB_SESSION_TYPE SESSION_TYPE_TYPE0
#define ARB_PROTECTION_MODE PROTECTION_MODE_HM

#define PXP_GLOBAL_TERMINATE _MMIO(0x320f8)

static bool is_hw_arb_session_in_play(struct intel_pxp *pxp)
{
	u32 regval_sip = 0;
	intel_wakeref_t wakeref;
	struct intel_gt *gt = container_of(pxp, struct intel_gt, pxp);

	with_intel_runtime_pm(&gt->i915->runtime_pm, wakeref) {
		regval_sip = intel_uncore_read(gt->uncore, GEN12_KCR_SIP);
	}

	return regval_sip & BIT(ARB_SESSION_INDEX);
}

/* wait hw session_in_play reg to match the current sw state */
static int wait_arb_hw_sw_state(struct intel_pxp *pxp)
{
	const int max_retry = 10;
	const int ms_delay = 10;
	int retry = 0;
	int ret;
	struct pxp_protected_session *arb = &pxp->ctx.arb_session;

	ret = -EINVAL;
	for (retry = 0; retry < max_retry; retry++) {
		if (is_hw_arb_session_in_play(pxp) ==
		    arb->is_in_play) {
			ret = 0;
			break;
		}

		msleep(ms_delay);
	}

	return ret;
}

static void arb_session_entry_init(struct intel_pxp *pxp)
{
	struct pxp_protected_session *arb = &pxp->ctx.arb_session;

	arb->context_id = pxp->ctx.id;
	arb->type = ARB_SESSION_TYPE;
	arb->protection_mode = ARB_PROTECTION_MODE;
	arb->index = ARB_SESSION_INDEX;
	arb->is_in_play = false;
}

static int intel_pxp_arb_reserve_session(struct intel_pxp *pxp)
{
	int ret;

	lockdep_assert_held(&pxp->ctx.mutex);

	arb_session_entry_init(pxp);
	ret = wait_arb_hw_sw_state(pxp);

	return ret;
}

/**
 * intel_pxp_arb_mark_session_in_play - To put an reserved protected session to "in_play" state
 * @pxp: pointer to pxp struct.
 *
 * Return: status. 0 means update is successful.
 */
static int intel_pxp_arb_mark_session_in_play(struct intel_pxp *pxp)
{
	struct pxp_protected_session *arb = &pxp->ctx.arb_session;

	lockdep_assert_held(&pxp->ctx.mutex);

	if (arb->is_in_play)
		return -EINVAL;

	arb->is_in_play = true;
	return 0;
}

int intel_pxp_arb_create_session(struct intel_pxp *pxp)
{
	int ret;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	lockdep_assert_held(&pxp->ctx.mutex);

	if (pxp->ctx.flag_display_hm_surface_keys) {
		drm_err(&gt->i915->drm, "%s: arb session is alive so skipping the creation\n",
			__func__);
		return 0;
	}

	ret = intel_pxp_arb_reserve_session(pxp);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to reserve arb session\n");
		goto end;
	}

	ret = intel_pxp_tee_cmd_create_arb_session(pxp);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to send tee cmd for arb session creation\n");
		goto end;
	}

	ret = intel_pxp_arb_mark_session_in_play(pxp);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to mark arb session status in play\n");
		goto end;
	}

	pxp->ctx.flag_display_hm_surface_keys = true;

end:
	return ret;
}

static int intel_pxp_arb_session_termination(struct intel_pxp *pxp)
{
	u32 *cmd = NULL;
	u32 *cmd_ptr = NULL;
	int cmd_size_in_dw = 0;
	int ret;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	/* Calculate how many bytes need to be alloc */
	cmd_size_in_dw += intel_pxp_cmd_add_prolog(pxp, NULL, ARB_SESSION_TYPE, ARB_SESSION_INDEX);
	cmd_size_in_dw += intel_pxp_cmd_add_inline_termination(NULL);
	cmd_size_in_dw += intel_pxp_cmd_add_epilog(NULL);

	cmd = kzalloc(cmd_size_in_dw * 4, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	/* Program the command */
	cmd_ptr = cmd;
	cmd_ptr += intel_pxp_cmd_add_prolog(pxp, cmd_ptr, ARB_SESSION_TYPE, ARB_SESSION_INDEX);
	cmd_ptr += intel_pxp_cmd_add_inline_termination(cmd_ptr);
	cmd_ptr += intel_pxp_cmd_add_epilog(cmd_ptr);

	if (cmd_size_in_dw != (cmd_ptr - cmd)) {
		ret = -EINVAL;
		drm_err(&gt->i915->drm, "Failed to %s\n", __func__);
		goto end;
	}

	if (drm_debug_syslog_enabled(DRM_UT_DRIVER)) {
		print_hex_dump(KERN_DEBUG, "global termination cmd binaries:",
			       DUMP_PREFIX_OFFSET, 4, 4, cmd, cmd_size_in_dw * 4, true);
	}

	ret = intel_pxp_cmd_submit(pxp, cmd, cmd_size_in_dw);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to intel_pxp_cmd_submit()\n");
		goto end;
	}

end:
	kfree(cmd);
	return ret;
}

/**
 * intel_pxp_arb_terminate_session - Terminate the arb hw session and its entries.
 * @pxp: pointer to pxp struct.
 *
 * This function is NOT intended to be called from the ioctl, and need to be protected by
 * ctx.mutex to ensure no SIP change during the call.
 *
 * Return: status. 0 means terminate is successful.
 */
int intel_pxp_arb_terminate_session_with_global_terminate(struct intel_pxp *pxp)
{
	int ret;
	struct intel_gt *gt = container_of(pxp, struct intel_gt, pxp);
	struct pxp_protected_session *arb = &pxp->ctx.arb_session;

	lockdep_assert_held(&pxp->ctx.mutex);

	/* terminate the hw sessions */
	ret = intel_pxp_arb_session_termination(pxp);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to intel_pxp_arb_session_termination\n");
		return ret;
	}

	arb->is_in_play = false;

	ret = wait_arb_hw_sw_state(pxp);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to wait_arb_hw_sw_state\n");
		return ret;
	}

	intel_uncore_write(gt->uncore, PXP_GLOBAL_TERMINATE, 1);

	return ret;
}

