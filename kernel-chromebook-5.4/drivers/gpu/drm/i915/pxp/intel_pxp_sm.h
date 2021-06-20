/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_SM_H__
#define __INTEL_PXP_SM_H__

#include "intel_pxp.h"

/**
 * struct pxp_protected_session - linked list to track all active sessions.
 */
struct intel_pxp_sm_session {
	/** @list: linked list infrastructure, do not change its order. */
	struct list_head list;
	/** @index: Numeric identifier for this protected session */
	int index;
	/** @type: Type of session */
	int type;
	/** @protection_mode: mode of protection requested */
	int protection_mode;
	/** @context_id: context identifier of the protected session requestor */
	int context_id;
	/** @pid: pid of this session's creator */
	int pid;
	/** @drmfile: pointer to drm_file, which is allocated on device file open() call */
	struct drm_file *drmfile;

	/**
	 * @is_in_play: indicates whether the session has been established
	 *              in the HW root of trust if this flag is false, it
	 *              indicates an application has reserved this session,
	 *              but has not established the session in the hardware
	 *              yet.
	 */
	bool is_in_play;
};

int intel_pxp_sm_ioctl_reserve_session(struct intel_pxp *pxp, struct drm_file *drmfile,
				       int session_type, int protection_mode,
				       u32 *pxp_tag);
int intel_pxp_sm_ioctl_mark_session_in_play(struct intel_pxp *pxp, int session_type,
					    u32 session_id);
int intel_pxp_sm_ioctl_terminate_session(struct intel_pxp *pxp, int session_type,
					 int session_id);
int intel_pxp_sm_ioctl_query_pxp_tag(struct intel_pxp *pxp,
				     u32 *session_is_alive, u32 *pxp_tag);

bool intel_pxp_sm_is_hw_session_in_play(struct intel_pxp *pxp,
					int session_type, int session_index);
int intel_pxp_sm_terminate_all_sessions(struct intel_pxp *pxp, int session_type);
int intel_pxp_sm_close(struct intel_pxp *pxp, struct drm_file *drmfile);
#endif /* __INTEL_PXP_SM_H__ */
