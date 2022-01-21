// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020 Intel Corporation.
 */
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include "intel_pxp.h"
#include "intel_pxp_cmd.h"
#include "intel_pxp_irq.h"
#include "intel_pxp_session.h"
#include "intel_pxp_tee.h"
#include "gt/intel_context.h"
#include "i915_drv.h"

/**
 * DOC: PXP
 *
 * PXP (Protected Xe Path) is a feature available in Gen12 and newer platforms.
 * It allows execution and flip to display of protected (i.e. encrypted)
 * objects. The SW support is enabled via the CONFIG_DRM_I915_PXP kconfig.
 *
 * Objects can opt-in to PXP encryption at creation time via the
 * I915_GEM_CREATE_EXT_PROTECTED_CONTENT create_ext flag. For objects to be
 * correctly protected they must be used in conjunction with a context created
 * with the I915_CONTEXT_PARAM_PROTECTED_CONTENT flag. See the documentation
 * of those two uapi flags for details and restrictions.
 *
 * Protected objects are tied to a pxp session; currently we only support one
 * session, which i915 manages and whose index is available in the uapi
 * (I915_PROTECTED_CONTENT_DEFAULT_SESSION) for use in instructions targeting
 * protected objects.
 * The session is invalidated by the HW when certain events occur (e.g.
 * suspend/resume). When this happens, all the objects that were used with the
 * session are marked as invalid and all contexts marked as using protected
 * content are banned. Any further attempt at using them in an execbuf call is
 * rejected, while flips are converted to black frames.
 *
 * Some of the PXP setup operations are performed by the Management Engine,
 * which is handled by the mei driver; communication between i915 and mei is
 * performed via the mei_pxp component module.
 */

struct intel_gt *pxp_to_gt(const struct intel_pxp *pxp)
{
	return container_of(pxp, struct intel_gt, pxp);
}

bool intel_pxp_is_active(const struct intel_pxp *pxp)
{
	return pxp->arb_session.is_valid;
}

/* KCR register definitions */
#define KCR_INIT _MMIO(0x320f0)
/* Setting KCR Init bit is required after system boot */
#define KCR_INIT_ALLOW_DISPLAY_ME_WRITES REG_BIT(14)

static void kcr_pxp_enable(struct intel_gt *gt)
{
	intel_uncore_write(gt->uncore, KCR_INIT,
			   _MASKED_BIT_ENABLE(KCR_INIT_ALLOW_DISPLAY_ME_WRITES));
}

static void kcr_pxp_disable(struct intel_gt *gt)
{
	intel_uncore_write(gt->uncore, KCR_INIT,
			   _MASKED_BIT_DISABLE(KCR_INIT_ALLOW_DISPLAY_ME_WRITES));
}

static int create_vcs_context(struct intel_pxp *pxp)
{
	struct intel_gt *gt = pxp_to_gt(pxp);
	struct intel_engine_cs *engine;
	struct intel_context *ce;
	int i;

	/*
	 * Find the first VCS engine present. We're guaranteed there is one
	 * if we're in this function due to the check in has_pxp
	 */
	for (i = 0, engine = NULL; !engine; i++)
		engine = gt->engine_class[VIDEO_DECODE_CLASS][i];

	GEM_BUG_ON(!engine || engine->class != VIDEO_DECODE_CLASS);

	ce = engine->kernel_context;

	if (IS_ERR(ce)) {
		drm_err(&gt->i915->drm, "failed to create VCS ctx for PXP\n");
		return PTR_ERR(ce);
	}

	pxp->ce = ce;

	return 0;
}

static void destroy_vcs_context(struct intel_pxp *pxp)
{
	pxp->ce = NULL;
}

void intel_pxp_init(struct intel_pxp *pxp)
{
	struct intel_gt *gt = pxp_to_gt(pxp);
	int ret;

	if (!HAS_PXP(gt->i915))
		return;

	/*
	 * we'll use the completion to check if there is a termination pending,
	 * so we start it as completed and we reinit it when a termination
	 * is triggered.
	 */
	init_completion(&pxp->termination);
	complete_all(&pxp->termination);

	mutex_init(&pxp->arb_mutex);
	INIT_WORK(&pxp->session_work, intel_pxp_session_work);

	mutex_init(&pxp->tee_mutex);
	mutex_init(&pxp->session_mutex);

	ret = create_vcs_context(pxp);
	if (ret)
		return;

	intel_pxp_init_arb_session(pxp);

	ret = intel_pxp_tee_component_init(pxp);
	if (ret)
		goto out_session;

	drm_info(&gt->i915->drm, "Protected Xe Path (PXP) protected content support initialized\n");

	return;

out_session:
	intel_pxp_fini_arb_session(pxp);
	destroy_vcs_context(pxp);
}

void intel_pxp_fini(struct intel_pxp *pxp)
{
	if (!intel_pxp_is_enabled(pxp))
		return;

	pxp->arb_session.is_valid = false;

	intel_pxp_tee_component_fini(pxp);

	intel_pxp_fini_arb_session(pxp);

	destroy_vcs_context(pxp);
}

void intel_pxp_mark_termination_in_progress(struct intel_pxp *pxp)
{
	pxp->hw_state_invalidated = true;
	pxp->arb_session.is_valid = false;
	pxp->arb_session.tag = 0;
	reinit_completion(&pxp->termination);
}

static void pxp_queue_termination(struct intel_pxp *pxp)
{
	struct intel_gt *gt = pxp_to_gt(pxp);

	/*
	 * We want to get the same effect as if we received a termination
	 * interrupt, so just pretend that we did.
	 */
	spin_lock_irq(&gt->irq_lock);
	intel_pxp_mark_termination_in_progress(pxp);
	pxp->session_events |= PXP_TERMINATION_REQUEST;
	queue_work(system_unbound_wq, &pxp->session_work);
	spin_unlock_irq(&gt->irq_lock);
}

/*
 * the arb session is restarted from the irq work when we receive the
 * termination completion interrupt
 */
int intel_pxp_start(struct intel_pxp *pxp)
{
	int ret = 0;

	if (!intel_pxp_is_enabled(pxp))
		return -ENODEV;

	mutex_lock(&pxp->arb_mutex);

	if (pxp->arb_session.is_valid)
		goto unlock;

	pxp_queue_termination(pxp);

	if (!wait_for_completion_timeout(&pxp->termination,
					msecs_to_jiffies(250))) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	/* make sure the compiler doesn't optimize the double access */
	barrier();

	if (!pxp->arb_session.is_valid)
		ret = -EIO;

unlock:
	mutex_unlock(&pxp->arb_mutex);
	return ret;
}

void intel_pxp_init_hw(struct intel_pxp *pxp)
{
	kcr_pxp_enable(pxp_to_gt(pxp));
	intel_pxp_irq_enable(pxp);
}

void intel_pxp_fini_hw(struct intel_pxp *pxp)
{
	kcr_pxp_disable(pxp_to_gt(pxp));

	intel_pxp_irq_disable(pxp);
}

int intel_pxp_key_check(struct intel_pxp *pxp,
			struct drm_i915_gem_object *obj,
			bool assign)
{
	if (!intel_pxp_is_active(pxp))
		return -ENODEV;

	if (!i915_gem_object_is_protected(obj))
		return -EINVAL;

	GEM_BUG_ON(!pxp->key_instance);

	/*
	 * If this is the first time we're using this object, it's not
	 * encrypted yet; it will be encrypted with the current key, so mark it
	 * as such. If the object is already encrypted, check instead if the
	 * used key is still valid.
	 */
	if (!obj->pxp_key_instance && assign)
		obj->pxp_key_instance = pxp->key_instance;

	if (obj->pxp_key_instance != pxp->key_instance)
		return -ENOEXEC;

	return 0;
}

static int pxp_set_session_status(struct intel_pxp *pxp,
				  struct downstream_drm_i915_pxp_ops *pxp_ops,
				  struct drm_file *drmfile)
{
	struct downstream_drm_i915_pxp_set_session_status_params params;
	struct downstream_drm_i915_pxp_set_session_status_params __user *uparams =
		u64_to_user_ptr(pxp_ops->params);
	int ret = 0;

	if (copy_from_user(&params, uparams, sizeof(params)) != 0)
		return -EFAULT;

	switch (params.req_session_state) {
	case DOWNSTREAM_DRM_I915_PXP_REQ_SESSION_ID_INIT:
		ret = intel_pxp_sm_ioctl_reserve_session(pxp, drmfile,
							 params.session_mode,
							 &params.pxp_tag);
		break;
	case DOWNSTREAM_DRM_I915_PXP_REQ_SESSION_IN_PLAY:
		ret = intel_pxp_sm_ioctl_mark_session_in_play(pxp, drmfile,
							      params.pxp_tag);
		break;
	case DOWNSTREAM_DRM_I915_PXP_REQ_SESSION_TERMINATE:
		ret = intel_pxp_sm_ioctl_terminate_session(pxp, drmfile,
							   params.pxp_tag);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret >= 0) {
		pxp_ops->status = ret;

		if (copy_to_user(uparams, &params, sizeof(params)))
			ret = -EFAULT;
		else
			ret = 0;
	}

	return ret;
}

static int pxp_send_tee_msg(struct intel_pxp *pxp,
			    struct downstream_drm_i915_pxp_ops *pxp_ops,
			    struct drm_file *drmfile)
{
	struct drm_i915_private *i915 = pxp_to_gt(pxp)->i915;
	struct downstream_drm_i915_pxp_tee_io_message_params params;
	struct downstream_drm_i915_pxp_tee_io_message_params __user *uparams =
		u64_to_user_ptr(pxp_ops->params);
	int ret = 0;

	if (copy_from_user(&params, uparams, sizeof(params)) != 0)
		return -EFAULT;

	ret = intel_pxp_tee_ioctl_io_message(pxp, &params);
	if (ret >= 0) {
		pxp_ops->status = ret;

		if (copy_to_user(uparams, &params, sizeof(params)))
			ret = -EFAULT;
		else
			ret = 0;
	} else {
		drm_dbg(&i915->drm, "Failed to send user TEE IO message\n");
	}

	return ret;
}

static int pxp_query_tag(struct intel_pxp *pxp, struct downstream_drm_i915_pxp_ops *pxp_ops)
{
	struct downstream_drm_i915_pxp_query_tag params;
	struct downstream_drm_i915_pxp_query_tag __user *uparams =
		u64_to_user_ptr(pxp_ops->params);
	int ret = 0;

	if (copy_from_user(&params, uparams, sizeof(params)) != 0)
		return -EFAULT;

	ret = intel_pxp_sm_ioctl_query_pxp_tag(pxp, &params.session_is_alive,
					       &params.pxp_tag);
	if (ret >= 0) {
		pxp_ops->status = ret;

		if (copy_to_user(uparams, &params, sizeof(params)))
			ret = -EFAULT;
		else
			ret = 0;
	}

	return ret;
}

int i915_pxp_ops_ioctl(struct drm_device *dev, void *data, struct drm_file *drmfile)
{
	int ret = 0;
	struct downstream_drm_i915_pxp_ops *pxp_ops = data;
	struct drm_i915_private *i915 = to_i915(dev);
	struct intel_pxp *pxp = &i915->gt.pxp;
	intel_wakeref_t wakeref;

	if (!intel_pxp_is_enabled(pxp))
		return -ENODEV;

	wakeref = intel_runtime_pm_get_if_in_use(&i915->runtime_pm);
	if (!wakeref) {
		drm_dbg(&i915->drm, "pxp ioctl blocked due to state in suspend\n");
		pxp_ops->status = DOWNSTREAM_DRM_I915_PXP_OP_STATUS_SESSION_NOT_AVAILABLE;
		return 0;
	}
	if (pxp->hw_state_invalidated) {
		drm_dbg(&i915->drm, "pxp ioctl retry required due to state attacked\n");
		pxp_ops->status = DOWNSTREAM_DRM_I915_PXP_OP_STATUS_RETRY_REQUIRED;
		goto out_unlock;
	}

	if (!intel_pxp_is_active(pxp)) {
		ret = intel_pxp_start(pxp);
		if (ret)
			goto out_pm;
	}

	mutex_lock(&pxp->session_mutex);

	switch (pxp_ops->action) {
	case DOWNSTREAM_DRM_I915_PXP_ACTION_SET_SESSION_STATUS:
		ret = pxp_set_session_status(pxp, pxp_ops, drmfile);
		break;
	case DOWNSTREAM_DRM_I915_PXP_ACTION_TEE_IO_MESSAGE:
		ret = pxp_send_tee_msg(pxp, pxp_ops, drmfile);
		break;
	case DOWNSTREAM_DRM_I915_PXP_ACTION_QUERY_PXP_TAG:
		ret = pxp_query_tag(pxp, pxp_ops);
		break;
	default:
		ret = -EINVAL;
		break;
	}

out_unlock:
	mutex_unlock(&pxp->session_mutex);
out_pm:
	intel_runtime_pm_put(&i915->runtime_pm, wakeref);

	return ret;
}

void intel_pxp_close(struct intel_pxp *pxp, struct drm_file *drmfile)
{
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	if (!intel_pxp_is_enabled(pxp) || !drmfile)
		return;

	mutex_lock(&pxp->session_mutex);
	intel_pxp_file_close(pxp, drmfile);
	mutex_unlock(&pxp->session_mutex);
}
