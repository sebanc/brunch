// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020 Intel Corporation.
 */
#include "i915_drv.h"
#include "intel_pxp.h"
#include "intel_pxp_context.h"
#include "intel_pxp_tee.h"
#include "intel_pxp_arb.h"
#include "intel_pxp_sm.h"
#include "intel_pxp_cmd.h"
#include "gt/intel_gt_pm.h"

/* KCR register definitions */
#define KCR_INIT            _MMIO(0x320f0)
#define KCR_INIT_MASK_SHIFT (16)
/* Setting KCR Init bit is required after system boot */
#define KCR_INIT_ALLOW_DISPLAY_ME_WRITES (BIT(14) | (BIT(14) << KCR_INIT_MASK_SHIFT))

enum pxp_ioctl_action {
	PXP_ACTION_QUERY_PXP_TAG = 0,
	PXP_ACTION_SET_SESSION_STATUS = 1,
	PXP_ACTION_TEE_IO_MESSAGE = 4,
};

enum pxp_session_req {
	/* Request KMD to allocate session id and move it to IN INIT */
	PXP_REQ_SESSION_ID_INIT = 0x0,
	/* Inform KMD that UMD has completed the initialization */
	PXP_REQ_SESSION_IN_PLAY,
	/* Request KMD to terminate the session */
	PXP_REQ_SESSION_TERMINATE
};

static void intel_pxp_write_irq_mask_reg(struct intel_gt *gt, u32 mask)
{
	lockdep_assert_held(&gt->irq_lock);

	intel_uncore_write(gt->uncore, GEN11_CRYPTO_RSVD_INTR_MASK, mask << 16);
}

static int intel_pxp_teardown_required_callback(struct intel_pxp *pxp)
{
	int ret;

	mutex_lock(&pxp->ctx.mutex);

	pxp->ctx.global_state_attacked = true;
	pxp->ctx.flag_display_hm_surface_keys = false;

	ret = intel_pxp_sm_terminate_all_sessions(pxp, SESSION_TYPE_TYPE0);
	if (ret)
		goto end;

	ret = intel_pxp_arb_terminate_session_with_global_terminate(pxp);
end:
	mutex_unlock(&pxp->ctx.mutex);

	return ret;
}

static int intel_pxp_global_terminate_complete_callback(struct intel_pxp *pxp)
{
	int ret = 0;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	mutex_lock(&pxp->ctx.mutex);

	if (pxp->ctx.global_state_attacked) {
		pxp->ctx.global_state_attacked = false;

		/* Re-create the arb session after teardown handle complete */
		ret = intel_pxp_arb_create_session(pxp);
		if (ret) {
			drm_err(&gt->i915->drm, "Failed to create arb session\n");
			goto end;
		}
	}
end:
	mutex_unlock(&pxp->ctx.mutex);
	return ret;
}

static void intel_pxp_irq_work(struct work_struct *work)
{
	struct intel_pxp *pxp = container_of(work, typeof(*pxp), irq_work);
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);
	u32 events = 0;

	spin_lock_irq(&gt->irq_lock);
	events = fetch_and_zero(&pxp->current_events);
	spin_unlock_irq(&gt->irq_lock);

	if (events & PXP_IRQ_VECTOR_DISPLAY_PXP_STATE_TERMINATED ||
	    events & PXP_IRQ_VECTOR_DISPLAY_APP_TERM_PER_FW_REQ)
		intel_pxp_teardown_required_callback(pxp);

	if (events & PXP_IRQ_VECTOR_PXP_DISP_STATE_RESET_COMPLETE)
		intel_pxp_global_terminate_complete_callback(pxp);

	spin_lock_irq(&gt->irq_lock);
	intel_pxp_write_irq_mask_reg(gt, ~pxp->handled_irr);
	spin_unlock_irq(&gt->irq_lock);
}

int intel_pxp_init(struct intel_pxp *pxp)
{
	struct intel_gt *gt = container_of(pxp, struct intel_gt, pxp);
	int i;

	/* PXP only available for GEN12+ */
	if (INTEL_GEN(gt->i915) < 12)
		return -ENODEV;

	/* Find the first VCS engine present */
	for (i = 0; i < I915_MAX_VCS; i++) {
		if (HAS_ENGINE(gt, _VCS(i))) {
			pxp->vcs_engine = gt->engine[_VCS(i)];
			break;
		}
	}
	if (!pxp->vcs_engine) {
		drm_err(&gt->i915->drm, "Could not find a VCS engine\n");
		return -ENODEV;
	}

	intel_pxp_ctx_init(&pxp->ctx);

	intel_uncore_write(gt->uncore, KCR_INIT, KCR_INIT_ALLOW_DISPLAY_ME_WRITES);

	intel_pxp_tee_component_init(pxp);

	INIT_WORK(&pxp->irq_work, intel_pxp_irq_work);

	pxp->handled_irr = (PXP_IRQ_VECTOR_DISPLAY_PXP_STATE_TERMINATED |
			    PXP_IRQ_VECTOR_DISPLAY_APP_TERM_PER_FW_REQ |
			    PXP_IRQ_VECTOR_PXP_DISP_STATE_RESET_COMPLETE);

	drm_info(&gt->i915->drm, "Protected Xe Path (PXP) protected content support initialized\n");

	return 0;
}

void intel_pxp_uninit(struct intel_pxp *pxp)
{
	intel_pxp_tee_component_fini(pxp);

	intel_pxp_ctx_fini(&pxp->ctx);
}

/**
 * intel_pxp_irq_handler - Proxies KCR interrupts to PXP.
 * @pxp: pointer to pxp struct
 * @iir: GT interrupt vector associated with the interrupt
 *
 * Dispatches each vector element into an IRQ to PXP.
 */
void intel_pxp_irq_handler(struct intel_pxp *pxp, u16 iir)
{
	const u32 events = iir & pxp->handled_irr;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	if (!gt || !gt->i915 || INTEL_GEN(gt->i915) < 12)
		return;

	lockdep_assert_held(&gt->irq_lock);

	if (!events) {
		drm_err(&gt->i915->drm, "pxp irq handler called with zero irr\n");
		return;
	}

	intel_pxp_write_irq_mask_reg(gt, ~0);

	pxp->current_events |= events;
	schedule_work(&pxp->irq_work);
}

bool intel_pxp_gem_object_status(struct drm_i915_private *i915)
{
	if (i915->gt.pxp.ctx.id != 0 &&
	    i915->gt.pxp.ctx.flag_display_hm_surface_keys)
		return true;
	else
		return false;
}

int i915_pxp_ops_ioctl(struct drm_device *dev, void *data, struct drm_file *drmfile)
{
	int ret;
	intel_wakeref_t wakeref;
	struct pxp_info pxp_info = {0};
	struct drm_i915_pxp_ops *pxp_ops = data;
	struct drm_i915_private *i915 = to_i915(dev);
	struct intel_pxp *pxp = &i915->gt.pxp;

	if (pxp->ctx.id == 0 || !drmfile || !pxp_ops ||
	    pxp_ops->info_size != sizeof(pxp_info))
		return -EINVAL;

	if (copy_from_user(&pxp_info, pxp_ops->info_ptr, sizeof(pxp_info)) != 0)
		return -EFAULT;

	mutex_lock(&pxp->ctx.mutex);

	if (pxp->ctx.global_state_in_suspend) {
		wakeref = intel_runtime_pm_get_if_in_use(&i915->runtime_pm);
		if (wakeref) {
			/*
			 * Workaround for scenario when intel_pxp_pm_prepare_suspend()
			 * is called but suspend is aborted before i915 suspends so
			 * intel_pxp_pm_resume() isn't called and this flag left set
			 *
			 * In this scenario, since we didn't actually go into suspend
			 * the sessions are valid and alive so can just clear this
			 * flag if awake.
			*/
			pxp->ctx.global_state_in_suspend = false;
			intel_runtime_pm_put(&i915->runtime_pm, wakeref);
		} else {
			drm_err(&i915->drm, "Return failure due to state in suspend\n");
			pxp_info.sm_status = PXP_SM_STATUS_SESSION_NOT_AVAILABLE;
			ret = 0;
			goto end;
		}
	}

	if (pxp->ctx.global_state_attacked) {
		drm_err(&i915->drm, "Retry required due to state attacked\n");
		pxp_info.sm_status = PXP_SM_STATUS_RETRY_REQUIRED;
		ret = 0;
		goto end;
	}

	switch (pxp_info.action) {
	case PXP_ACTION_SET_SESSION_STATUS:
	{
		struct pxp_set_session_status_params *params = &pxp_info.set_session_status;

		if (params->req_session_state == PXP_REQ_SESSION_ID_INIT) {
			ret = intel_pxp_sm_ioctl_reserve_session(pxp, drmfile,
								 params->session_type,
								 params->session_mode,
								 &params->pxp_tag);
			if (ret == PXP_SM_STATUS_RETRY_REQUIRED ||
			    ret == PXP_SM_STATUS_SESSION_NOT_AVAILABLE) {
				pxp_info.sm_status = ret;
				ret = 0;
			}
		} else if (params->req_session_state == PXP_REQ_SESSION_IN_PLAY) {
			ret = intel_pxp_sm_ioctl_mark_session_in_play(pxp, params->session_type,
								      params->pxp_tag);
		} else if (params->req_session_state == PXP_REQ_SESSION_TERMINATE) {
			ret = intel_pxp_sm_ioctl_terminate_session(pxp, params->session_type,
								   params->pxp_tag);
		} else {
			ret = -EINVAL;
		}
		break;
	}
	case PXP_ACTION_QUERY_PXP_TAG:
	{
		struct pxp_sm_query_pxp_tag *params = &pxp_info.query_pxp_tag;

		ret = intel_pxp_sm_ioctl_query_pxp_tag(pxp, &params->session_is_alive,
						       &params->pxp_tag);
		break;
	}
	case PXP_ACTION_TEE_IO_MESSAGE:
	{
		struct pxp_tee_io_message_params *params = &pxp_info.tee_io_message;

		ret = intel_pxp_tee_ioctl_io_message(pxp,
						     params->msg_in, params->msg_in_size,
						     params->msg_out, &params->msg_out_size,
						     params->msg_out_buf_size);
		if (ret) {
			drm_err(&i915->drm, "Failed to send TEE IO message\n");
			ret = -EFAULT;
		}
		break;
	}
	default:
		drm_err(&i915->drm, "Failed to %s due to bad params\n", __func__);
		ret = -EINVAL;
		break;
	}

end:
	mutex_unlock(&pxp->ctx.mutex);

	if (ret == 0)
		if (copy_to_user(pxp_ops->info_ptr, &pxp_info, sizeof(pxp_info)) != 0)
			ret = -EFAULT;
	return ret;
}

void intel_pxp_close(struct intel_pxp *pxp, struct drm_file *drmfile)
{
	int ret;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	if (pxp->ctx.id == 0 || !drmfile)
		return;

	mutex_lock(&pxp->ctx.mutex);
	ret = intel_pxp_sm_close(pxp, drmfile);
	if (ret)
		drm_err(&gt->i915->drm, "Failed to %s, ret=[%d]\n", __func__, ret);
	mutex_unlock(&pxp->ctx.mutex);
}
