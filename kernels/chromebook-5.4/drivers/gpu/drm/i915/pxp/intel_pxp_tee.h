/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_TEE_H__
#define __INTEL_PXP_TEE_H__

#include "intel_pxp.h"

struct downstream_drm_i915_pxp_tee_io_message_params;

int intel_pxp_tee_component_init(struct intel_pxp *pxp);
void intel_pxp_tee_component_fini(struct intel_pxp *pxp);

int intel_pxp_tee_cmd_create_arb_session(struct intel_pxp *pxp,
					 int arb_session_id);

int intel_pxp_tee_ioctl_io_message(struct intel_pxp *pxp,
				   struct downstream_drm_i915_pxp_tee_io_message_params *params);

#endif /* __INTEL_PXP_TEE_H__ */
