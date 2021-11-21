/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_SESSION_H__
#define __INTEL_PXP_SESSION_H__

#include <linux/types.h>

struct work_struct;
struct intel_pxp;
struct drm_file;

#define ARB_SESSION I915_PROTECTED_CONTENT_DEFAULT_SESSION /* shorter define */

void intel_pxp_init_arb_session(struct intel_pxp *pxp);
void intel_pxp_fini_arb_session(struct intel_pxp *pxp);

int intel_pxp_sm_ioctl_reserve_session(struct intel_pxp *pxp, struct drm_file *drmfile,
				       int protection_mode, u32 *pxp_tag);
int intel_pxp_sm_ioctl_mark_session_in_play(struct intel_pxp *pxp,
					    struct drm_file *drmfile,
					    u32 session_id);
int intel_pxp_sm_ioctl_terminate_session(struct intel_pxp *pxp,
					 struct drm_file *drmfile,
					 u32 pxp_tag);

int intel_pxp_sm_ioctl_query_pxp_tag(struct intel_pxp *pxp,
				     u32 *session_is_alive, u32 *pxp_tag);

void intel_pxp_file_close(struct intel_pxp *pxp, struct drm_file *drmfile);

bool intel_pxp_session_is_in_play(struct intel_pxp *pxp, u32 id);

void intel_pxp_session_work(struct work_struct *work);

#endif /* __INTEL_PXP_SESSION_H__ */
