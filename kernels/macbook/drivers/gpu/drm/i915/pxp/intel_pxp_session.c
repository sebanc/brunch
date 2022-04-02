// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#include <drm/i915_drm.h>

#include "i915_drv.h"

#include "intel_pxp.h"
#include "intel_pxp_cmd.h"
#include "intel_pxp_session.h"
#include "intel_pxp_tee.h"
#include "intel_pxp_types.h"

#define KCR_STATUS_1   _MMIO(0x320f4)
#define KCR_STATUS_1_ATTACK_MASK 0x80000000

/* PXP global terminate register for session termination */
#define PXP_GLOBAL_TERMINATE _MMIO(0x320f8)

static u8 get_next_instance_id(struct intel_pxp *pxp, u32 id)
{
	u8 next_id = ++pxp->next_tag_id[id];

	if  (!next_id)
		next_id = ++pxp->next_tag_id[id];

	return next_id;
}

static u32 set_pxp_tag(struct intel_pxp *pxp, int session_idx, int protection_mode)
{
	u32 pxp_tag = 0;

	switch (protection_mode) {
	case DOWNSTREAM_DRM_I915_PXP_MODE_LM:
		break;
	case DOWNSTREAM_DRM_I915_PXP_MODE_HM:
		pxp_tag |= DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_HM;
		break;
	case DOWNSTREAM_DRM_I915_PXP_MODE_SM:
		pxp_tag |= DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_HM;
		pxp_tag |= DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_SM;
		break;
	default:
		MISSING_CASE(protection_mode);
	}

	pxp_tag |= DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_ENABLED;
	pxp_tag |= FIELD_PREP(DOWNSTREAM_DRM_I915_PXP_TAG_INSTANCE_ID_MASK,
			      get_next_instance_id(pxp, session_idx));
	pxp_tag |= FIELD_PREP(DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_ID_MASK, session_idx);

	return pxp_tag;
}

bool intel_pxp_session_is_in_play(struct intel_pxp *pxp, u32 id)
{
	struct intel_uncore *uncore = pxp_to_gt(pxp)->uncore;
	intel_wakeref_t wakeref;
	u32 sip = 0;

	/* if we're suspended the session is considered off */
	with_intel_runtime_pm_if_in_use(uncore->rpm, wakeref)
		sip = intel_uncore_read(uncore, GEN12_KCR_SIP);

	return sip & BIT(id);
}

static int pxp_wait_for_session_state(struct intel_pxp *pxp, u32 id, bool in_play)
{
	struct intel_uncore *uncore = pxp_to_gt(pxp)->uncore;
	intel_wakeref_t wakeref;
	u32 mask = BIT(id);
	int ret;

	/* if we're suspended the session is considered off */
	wakeref = intel_runtime_pm_get_if_in_use(uncore->rpm);
	if (!wakeref)
		return in_play ? -ENODEV : 0;

	ret = intel_wait_for_register(uncore,
				      GEN12_KCR_SIP,
				      mask,
				      in_play ? mask : 0,
				      100);

	intel_runtime_pm_put(uncore->rpm, wakeref);

	return ret;
}

/**
 * is_hwdrm_session_attacked - To check if hwdrm active sessions are attacked.
 * @pxp: pointer pxp struct
 *
 * Return: true if hardware sessions is attacked, false otherwise.
 */
static bool is_hwdrm_session_attacked(struct intel_pxp *pxp)
{
	u32 regval = 0;
	intel_wakeref_t wakeref;
	struct intel_gt *gt = pxp_to_gt(pxp);

	if (pxp->hw_state_invalidated)
		return true;

	with_intel_runtime_pm(gt->uncore->rpm, wakeref)
		regval = intel_uncore_read(gt->uncore, KCR_STATUS_1);

	return regval & KCR_STATUS_1_ATTACK_MASK;
}

static void __init_session_entry(struct intel_pxp *pxp,
				 struct intel_pxp_session *session,
				 struct drm_file *drmfile,
				 int protection_mode, int session_index)
{
	session->protection_mode = protection_mode;
	session->index = session_index;
	session->is_valid = false;
	session->drmfile = drmfile;
	session->tag = set_pxp_tag(pxp, session_index, protection_mode);
}

/**
 * create_session_entry - Create a new session entry with provided info.
 * @pxp: pointer to pxp struct
 * @drmfile: pointer to drm_file
 * @session_index: Numeric session identifier.
 *
 * Return: status. 0 means creation is successful.
 */
static int create_session_entry(struct intel_pxp *pxp, struct drm_file *drmfile,
				int protection_mode, int session_index)
{
	struct intel_pxp_session *session = NULL;

	if (pxp->hwdrm_sessions[session_index])
		return -EEXIST;

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	__init_session_entry(pxp, session, drmfile, protection_mode, session_index);

	pxp->hwdrm_sessions[session_index] = session;
	set_bit(session_index, pxp->reserved_sessions);

	return 0;
}

static void free_session_entry(struct intel_pxp *pxp, int session_index)
{
	if (!pxp->hwdrm_sessions[session_index])
		return;

	clear_bit(session_index, pxp->reserved_sessions);
	kfree(fetch_and_zero(&pxp->hwdrm_sessions[session_index]));
}

void intel_pxp_init_arb_session(struct intel_pxp *pxp)
{
	__init_session_entry(pxp, &pxp->arb_session, NULL, DOWNSTREAM_DRM_I915_PXP_MODE_HM, ARB_SESSION);
	pxp->hwdrm_sessions[ARB_SESSION] = &pxp->arb_session;
	set_bit(ARB_SESSION, pxp->reserved_sessions);
}

void intel_pxp_fini_arb_session(struct intel_pxp *pxp)
{
	pxp->hwdrm_sessions[ARB_SESSION] = NULL;
	clear_bit(ARB_SESSION, pxp->reserved_sessions);
}

/**
 * intel_pxp_sm_ioctl_reserve_session - To reserve an available protected session.
 * @pxp: pointer to pxp struct
 * @drmfile: pointer to drm_file.
 * @pxp_tag: Numeric session identifier returned back to caller.
 *
 * Return: status. 0 means reserve is successful.
 */
int intel_pxp_sm_ioctl_reserve_session(struct intel_pxp *pxp, struct drm_file *drmfile,
				       int protection_mode, u32 *pxp_tag)
{
	int ret;
	int idx = 0;

	if (!drmfile || !pxp_tag)
		return -EINVAL;

	lockdep_assert_held(&pxp->session_mutex);

	/* check if sessions are under attack. if so, don't allow creation */
	if (is_hwdrm_session_attacked(pxp))
		return -EPERM;

	if (protection_mode < DOWNSTREAM_DRM_I915_PXP_MODE_LM ||
	    protection_mode > DOWNSTREAM_DRM_I915_PXP_MODE_SM)
		return -EINVAL;

	idx = find_first_zero_bit(pxp->reserved_sessions,
				  INTEL_PXP_MAX_HWDRM_SESSIONS);
	if (idx >= INTEL_PXP_MAX_HWDRM_SESSIONS)
		return DOWNSTREAM_DRM_I915_PXP_OP_STATUS_SESSION_NOT_AVAILABLE;

        ret = pxp_wait_for_session_state(pxp, idx, false);
        if (ret) {
                /* force termination of old reservation */
                ret = intel_pxp_terminate_session(pxp, idx);
                if (ret)
                        return DOWNSTREAM_DRM_I915_PXP_OP_STATUS_RETRY_REQUIRED;
                /* wait again for HW state */
                ret = pxp_wait_for_session_state(pxp, idx, false);
                if (ret)
                        return DOWNSTREAM_DRM_I915_PXP_OP_STATUS_RETRY_REQUIRED;
        }

	ret = create_session_entry(pxp, drmfile,
				   protection_mode, idx);
	*pxp_tag = pxp->hwdrm_sessions[idx]->tag;

	return ret;
}

/**
 * intel_pxp_sm_ioctl_terminate_session - To terminate an active HW session and free its entry.
 * @pxp: pointer to pxp struct.
 * @drmfile: drm_file of the app issuing the termination
 * @session_id: Session identifier of the session, containing type and index info
 *
 * Return: 0 means terminate is successful, or didn't find the desired session.
 */
int intel_pxp_sm_ioctl_terminate_session(struct intel_pxp *pxp,
					 struct drm_file *drmfile,
					 u32 pxp_tag)
{
	u8 session_id = pxp_tag & DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_ID_MASK;
	int ret;

	lockdep_assert_held(&pxp->session_mutex);

	if (!pxp->hwdrm_sessions[session_id])
		return 0;

	if (pxp->hwdrm_sessions[session_id]->drmfile != drmfile)
		return -EPERM;

	ret = intel_pxp_terminate_session(pxp, session_id);
	if (ret)
		return ret;

	free_session_entry(pxp, session_id);

	return 0;
}

int intel_pxp_sm_ioctl_query_pxp_tag(struct intel_pxp *pxp,
				     u32 *session_is_alive, u32 *pxp_tag)
{
	int session_id = 0;

	if (!session_is_alive || !pxp_tag)
		return -EINVAL;

	session_id = *pxp_tag & DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_ID_MASK;

	if (!pxp->hwdrm_sessions[session_id]) {
		*pxp_tag = 0;
		*session_is_alive = 0;
		return 0;
	}

	*pxp_tag = pxp->hwdrm_sessions[session_id]->tag;

	if (session_is_alive)
		*session_is_alive = pxp->hwdrm_sessions[session_id]->is_valid;

	return 0;
}

/**
 * intel_pxp_sm_ioctl_mark_session_in_play - Put an reserved session to "in_play" state
 * @pxp: pointer to pxp struct
 * @drmfile: drm_file of the app marking the session as in play
 * @session_id: Session identifier of the session, containing type and index info
 *
 * Return: status. 0 means update is successful.
 */
int intel_pxp_sm_ioctl_mark_session_in_play(struct intel_pxp *pxp,
					    struct drm_file *drmfile,
					    u32 session_id)
{
	lockdep_assert_held(&pxp->session_mutex);

	if (!pxp->hwdrm_sessions[session_id])
		return -EINVAL;

	if (pxp->hwdrm_sessions[session_id]->drmfile != drmfile)
		return -EPERM;

	pxp->hwdrm_sessions[session_id]->is_valid = true;

	return 0;
}

void intel_pxp_file_close(struct intel_pxp *pxp, struct drm_file *drmfile)
{
	int idx;
	int ret;

	lockdep_assert_held(&pxp->session_mutex);

	for_each_set_bit(idx, pxp->reserved_sessions, INTEL_PXP_MAX_HWDRM_SESSIONS) {
		if (pxp->hwdrm_sessions[idx]->drmfile == drmfile) {
			ret = intel_pxp_terminate_session(pxp, idx);
			if (ret)
				drm_err(&pxp_to_gt(pxp)->i915->drm,
					"failed to correctly close PXP session %u\n",
					idx);

			free_session_entry(pxp, idx);
		}
	}

	return;
}

static int pxp_create_arb_session(struct intel_pxp *pxp)
{
	struct intel_gt *gt = pxp_to_gt(pxp);
	int ret;

	pxp->arb_session.is_valid = false;

	if (intel_pxp_session_is_in_play(pxp, ARB_SESSION)) {
		drm_err(&gt->i915->drm, "arb session already in play at creation time\n");
		return -EEXIST;
	}

	ret = intel_pxp_tee_cmd_create_arb_session(pxp, ARB_SESSION);
	if (ret) {
		drm_err(&gt->i915->drm, "tee cmd for arb session creation failed\n");
		return ret;
	}

	ret = pxp_wait_for_session_state(pxp, ARB_SESSION, true);
	if (ret) {
		drm_err(&gt->i915->drm, "arb session failed to go in play\n");
		return ret;
	}

	if (!++pxp->key_instance)
		++pxp->key_instance;

	pxp->arb_session.tag = set_pxp_tag(pxp, ARB_SESSION, DOWNSTREAM_DRM_I915_PXP_MODE_HM);
	pxp->arb_session.is_valid = true;

	return 0;
}

static int pxp_terminate_all_sessions(struct intel_pxp *pxp)
{
	int ret;
	u32 idx;
	long mask = 0;

	if (!intel_pxp_is_enabled(pxp))
		return 0;

	lockdep_assert_held(&pxp->session_mutex);

	for_each_set_bit(idx, pxp->reserved_sessions, INTEL_PXP_MAX_HWDRM_SESSIONS) {
		pxp->hwdrm_sessions[idx]->is_valid = false;
		mask |= BIT(idx);
	}

	if (mask) {
		ret = intel_pxp_terminate_sessions(pxp, mask);
		if (ret)
			return ret;
	}

	for_each_set_bit(idx, pxp->reserved_sessions, INTEL_PXP_MAX_HWDRM_SESSIONS) {
		/* we don't want to free the arb session! */
		if (idx == ARB_SESSION)
			continue;

		free_session_entry(pxp, idx);
	}

	return 0;
}

static int pxp_terminate_all_sessions_and_global(struct intel_pxp *pxp)
{
	int ret;
	struct intel_gt *gt = pxp_to_gt(pxp);

	/* must mark termination in progress calling this function */
	GEM_WARN_ON(pxp->arb_session.is_valid);

	mutex_lock(&pxp->session_mutex);

	/* terminate the hw sessions */
	ret = pxp_terminate_all_sessions(pxp);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to submit session termination\n");
		goto out;
	}

	ret = pxp_wait_for_session_state(pxp, ARB_SESSION, false);
	if (ret) {
		drm_err(&gt->i915->drm, "Session state did not clear\n");
		goto out;
	}

	intel_uncore_write(gt->uncore, PXP_GLOBAL_TERMINATE, 1);

out:
	mutex_unlock(&pxp->session_mutex);
	return ret;
}

static void pxp_terminate(struct intel_pxp *pxp)
{
	int ret;

	ret = pxp_terminate_all_sessions_and_global(pxp);

	/*
	 * if we fail to submit the termination there is no point in waiting for
	 * it to complete. PXP will be marked as non-active until the next
	 * termination is issued.
	 */
	if (ret)
		complete_all(&pxp->termination);
}

static void pxp_terminate_complete(struct intel_pxp *pxp)
{
	/* Re-create the arb session after teardown handle complete */
	if (pxp->hw_state_invalidated) {
		/*
		 * WA: Insert delay to allow GuC/CSE to resume allowing session
		 * inits after an explicit terminate before attempting to
		 * reinitialize the arb session
		 */
		msleep(50);

		pxp_create_arb_session(pxp);
		pxp->hw_state_invalidated = false;
	}

	complete_all(&pxp->termination);
}

void intel_pxp_session_work(struct work_struct *work)
{
	struct intel_pxp *pxp = container_of(work, typeof(*pxp), session_work);
	struct intel_gt *gt = pxp_to_gt(pxp);
	intel_wakeref_t wakeref;
	u32 events = 0;

	spin_lock_irq(&gt->irq_lock);
	events = fetch_and_zero(&pxp->session_events);
	spin_unlock_irq(&gt->irq_lock);

	if (!events)
		return;

	/*
	 * If we're processing an event while suspending then don't bother,
	 * we're going to re-init everything on resume anyway.
	 */
	wakeref = intel_runtime_pm_get_if_in_use(gt->uncore->rpm);
	if (!wakeref)
		return;

	if (events & PXP_TERMINATION_REQUEST) {
		events &= ~PXP_TERMINATION_COMPLETE;
		pxp_terminate(pxp);
	}

	if (events & PXP_TERMINATION_COMPLETE)
		pxp_terminate_complete(pxp);

	intel_runtime_pm_put(gt->uncore->rpm, wakeref);
}

