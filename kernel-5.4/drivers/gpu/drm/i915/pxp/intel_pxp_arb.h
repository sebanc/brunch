/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_ARB_H__
#define __INTEL_PXP_ARB_H__

#include <linux/types.h>

struct intel_pxp;

/**
 * struct pxp_protected_session - structure to track all active sessions.
 */
struct pxp_protected_session {
	/** @index: Numeric identifier for this protected session */
	int index;
	/** @type: Type of session */
	int type;
	/** @protection_mode: mode of protection requested */
	int protection_mode;
	/** @context_id: context identifier of the protected session requestor */
	int context_id;

	/**
	 * @is_in_play: indicates whether the session has been established
	 *              in the HW root of trust if this flag is false, it
	 *              indicates an application has reserved this session,
	 *              but has not * established the session in the
	 *              hardware yet.
	 */
	bool is_in_play;
};

int intel_pxp_arb_create_session(struct intel_pxp *pxp);
int intel_pxp_arb_terminate_session_with_global_terminate(struct intel_pxp *pxp);

#endif /* __INTEL_PXP_ARB_H__ */
