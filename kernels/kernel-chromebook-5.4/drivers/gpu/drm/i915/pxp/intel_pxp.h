/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_H__
#define __INTEL_PXP_H__

#include <linux/workqueue.h>
#include <drm/drm_file.h>
#include "intel_pxp_context.h"

#define PXP_IRQ_VECTOR_DISPLAY_PXP_STATE_TERMINATED BIT(1)
#define PXP_IRQ_VECTOR_DISPLAY_APP_TERM_PER_FW_REQ BIT(2)
#define PXP_IRQ_VECTOR_PXP_DISP_STATE_RESET_COMPLETE BIT(3)

#define GEN12_KCR_SIP _MMIO(0x32260) /* KCR type0 session in play 0-31 */

struct intel_engine_cs;

#define ARB_SESSION_INDEX 0xf

enum pxp_session_types {
	SESSION_TYPE_TYPE0 = 0,
	SESSION_TYPE_TYPE1 = 1,

	SESSION_TYPE_MAX
};

enum pxp_protection_modes {
	PROTECTION_MODE_NONE = 0,
	PROTECTION_MODE_LM   = 2,
	PROTECTION_MODE_HM   = 3,
	PROTECTION_MODE_SM   = 6,

	PROTECTION_MODE_ALL
};

enum pxp_sm_status {
	PXP_SM_STATUS_SUCCESS,
	PXP_SM_STATUS_RETRY_REQUIRED,
	PXP_SM_STATUS_SESSION_NOT_AVAILABLE,
	PXP_SM_STATUS_ERROR_UNKNOWN
};

struct pxp_tag {
	union {
		u32 value;
		struct {
			u32 session_id  : 8;
			u32 instance_id : 8;
			u32 enable      : 1;
			u32 hm          : 1;
			u32 reserved_1  : 1;
			u32 sm          : 1;
			u32 reserved_2  : 12;
		};
	};
};

struct intel_pxp {
	struct work_struct irq_work;
	u32 handled_irr;
	u32 current_events;

	struct pxp_context ctx;

	struct intel_engine_cs *vcs_engine;
};

static inline int pxp_session_max(int session_type)
{
	return (session_type == SESSION_TYPE_TYPE0) ?
		PXP_MAX_NORMAL_TYPE0_SESSIONS : PXP_MAX_TYPE1_SESSIONS;
}

struct drm_i915_private;

#ifdef CONFIG_DRM_I915_PXP
void intel_pxp_irq_handler(struct intel_pxp *pxp, u16 iir);
int i915_pxp_teardown_required_callback(struct intel_pxp *pxp);
int i915_pxp_global_terminate_complete_callback(struct intel_pxp *pxp);

int intel_pxp_init(struct intel_pxp *pxp);
void intel_pxp_uninit(struct intel_pxp *pxp);
bool intel_pxp_gem_object_status(struct drm_i915_private *i915);
int i915_pxp_ops_ioctl(struct drm_device *dev, void *data, struct drm_file *drmfile);
void intel_pxp_close(struct intel_pxp *pxp, struct drm_file *drmfile);
#else
static inline void intel_pxp_irq_handler(struct intel_pxp *pxp, u16 iir)
{
}

static inline int i915_pxp_teardown_required_callback(struct intel_pxp *pxp)
{
	return 0;
}

static inline int i915_pxp_global_terminate_complete_callback(struct intel_pxp *pxp)
{
	return 0;
}

static inline int intel_pxp_init(struct intel_pxp *pxp)
{
	return 0;
}

static inline void intel_pxp_uninit(struct intel_pxp *pxp)
{
}

static inline bool intel_pxp_gem_object_status(struct drm_i915_private *i915)
{
	return false;
}

static inline int i915_pxp_ops_ioctl(struct drm_device *dev, void *data, struct drm_file *drmfile)
{
	return 0;
}

static inline void intel_pxp_close(struct intel_pxp *pxp, struct drm_file *drmfile)
{
}
#endif

#endif /* __INTEL_PXP_PM_H__ */
