/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_TYPES_H__
#define __INTEL_PXP_TYPES_H__

#include <linux/completion.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

struct intel_context;
struct i915_pxp_component;

#define INTEL_PXP_MAX_HWDRM_SESSIONS 16

struct intel_pxp_session {
	/** @index: Numeric identifier for this protected session */
	int index;
	/** @protection_type: type of protection requested */
	int protection_type;
	/** @protection_mode: mode of protection requested */
	int protection_mode;
	/** @drmfile: pointer to drm_file, which is allocated on device file open() call */
	struct drm_file *drmfile;

	/**
	 * @is_valid: indicates whether the session has been established
	 *            in the HW root of trust. Note that, after a teardown, the
	 *            session can still be considered in play on the HW even if
	 *            the keys are gone, so we can't rely on the HW state of the
	 *            session to know if it's valid.
	 */
	bool is_valid;

	u32 tag;
};

/**
 * struct intel_pxp - pxp state
 */
struct intel_pxp {
	/**
	 * @pxp_component: i915_pxp_component struct of the bound mei_pxp
	 * module. Only set and cleared inside component bind/unbind functions,
	 * which are protected by &tee_mutex.
	 */
	struct i915_pxp_component *pxp_component;
	/**
	 * @pxp_component_added: track if the pxp component has been added.
	 * Set and cleared in tee init and fini functions respectively.
	 */
	bool pxp_component_added;

	/** @ce: kernel-owned context used for PXP operations */
	struct intel_context *ce;

	/** @arb_mutex: protects arb session start */
	struct mutex arb_mutex;

	/**
	 * @key_instance: tracks which key instance we're on, so we can use it
	 * to determine if an object was created using the current key or a
	 * previous one.
	 */
	u32 key_instance;

	/** @tee_mutex: protects the tee channel binding and messaging. */
	struct mutex tee_mutex;

	/**
	 * @hw_state_invalidated: if the HW perceives an attack on the integrity
	 * of the encryption it will invalidate the keys and expect SW to
	 * re-initialize the session. We keep track of this state to make sure
	 * we only re-start the arb session when required.
	 */
	bool hw_state_invalidated;

	/** @irq_enabled: tracks the status of the kcr irqs */
	bool irq_enabled;
	/**
	 * @termination: tracks the status of a pending termination. Only
	 * re-initialized under gt->irq_lock and completed in &session_work.
	 */
	struct completion termination;

	struct mutex session_mutex;
	DECLARE_BITMAP(reserved_sessions, INTEL_PXP_MAX_HWDRM_SESSIONS);
	struct intel_pxp_session *hwdrm_sessions[INTEL_PXP_MAX_HWDRM_SESSIONS];
	struct intel_pxp_session arb_session;
	u8 next_tag_id[INTEL_PXP_MAX_HWDRM_SESSIONS];

	/** @session_work: worker that manages session events. */
	struct work_struct session_work;
	/** @session_events: pending session events, protected with gt->irq_lock. */
	u32 session_events;
#define PXP_TERMINATION_REQUEST  BIT(0)
#define PXP_TERMINATION_COMPLETE BIT(1)

	/**
	* @last_tee_msg_interrupted: tracks if the last tee msg transaction was
	* interrupted by a pending signal. Since the msg still ends up being sent,
	* we need to drop the response to clear this state.
	**/
	bool last_tee_msg_interrupted;
};

#endif /* __INTEL_PXP_TYPES_H__ */
