/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2021 Intel Corporation
 */

#ifndef __I915_DRM_DOWNSTREAM_H__
#define __I915_DRM_DOWNSTREAM_H__

#include "drm.h"

/*
 * Modifications to structs/values defined here are subject to
 * backwards-compatibility constraints.
 *
 * Downstream declarations must be added here, not to i915_drm.h.
 * The values in i915_drm_downstream.h must also be kept
 * synchronized with values in i915_drm.h.
 */

#define DOWNSTREAM_DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)                  \
	[DRM_IOCTL_NR(DOWNSTREAM_DRM_IOCTL_##ioctl) - DRM_COMMAND_BASE] = { \
		.cmd = DOWNSTREAM_DRM_IOCTL_##ioctl,                        \
		.func = _func,                                          \
		.flags = _flags,                                        \
		.name = #ioctl                                          \
	}

/* DOWNSTREAM ioctl's */

/* DOWNSTREAM ioctl numbers go down from 0x5f */
#define DOWNSTREAM_DRM_I915_PXP_OPS			0x5f

#define DOWNSTREAM_DRM_IOCTL_I915_PXP_OPS		DRM_IOWR(DRM_COMMAND_BASE + DOWNSTREAM_DRM_I915_PXP_OPS, struct downstream_drm_i915_pxp_ops)

/* End DOWNSTREAM ioctl's */

/*
 * struct pxp_set_session_status_params - Params to reserved, set or destroy
 * the session from the PXP state machine.
 */
struct downstream_drm_i915_pxp_set_session_status_params {
	__u32 pxp_tag; /* in/out, session identifier tag */
	__u32 session_type; /* in, session type */
	__u32 session_mode; /* in, session mode */
#define DOWNSTREAM_DRM_I915_PXP_MODE_LM 0
#define DOWNSTREAM_DRM_I915_PXP_MODE_HM 1
#define DOWNSTREAM_DRM_I915_PXP_MODE_SM 2

	__u32 req_session_state; /* in, new session state */
	/* Request KMD to allocate session id and move it to INIT */
#define DOWNSTREAM_DRM_I915_PXP_REQ_SESSION_ID_INIT 0
	/* Inform KMD that UMD has completed the initialization */
#define DOWNSTREAM_DRM_I915_PXP_REQ_SESSION_IN_PLAY 1
	/* Request KMD to terminate the session */
#define DOWNSTREAM_DRM_I915_PXP_REQ_SESSION_TERMINATE 2
} __attribute__((packed));

/*
 * struct pxp_tee_io_message_params - Params to send/receive message to/from TEE.
 */
struct downstream_drm_i915_pxp_tee_io_message_params {
        __u64 msg_in; /* in - pointer to buffer containing input message */
        __u32 msg_in_size; /* in - input message size */
        __u64 msg_out; /* in - pointer to buffer to store the output message */
        __u32 msg_out_buf_size; /* in -  provided output message buffer size */
        __u32 msg_out_ret_size; /* out- output message actual size returned from TEE */
} __attribute__((packed));

/*
 * struct drm_i915_pxp_query_tag - Params to query the PXP tag of specified
 * session id and whether the session is alive from PXP state machine.
 */
struct downstream_drm_i915_pxp_query_tag {
        __u32 session_is_alive;

        /*
         * in  - Session ID, out pxp tag.
         * Tag format:
         * bits   0-6: session id
         * bit      7: rsvd
         * bits  8-15: instance id
         * bit     16: session enabled
         * bit     17: mode hm
         * bit     18: rsvd
         * bit     19: mode sm
         * bits 20-31: rsvd
         */
        __u32 pxp_tag;
#define DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_ID_MASK         (0x7f)
#define DOWNSTREAM_DRM_I915_PXP_TAG_INSTANCE_ID_MASK        (0xff << 8)
#define DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_ENABLED         (0x1 << 16)
#define DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_HM              (0x1 << 17)
#define DOWNSTREAM_DRM_I915_PXP_TAG_SESSION_SM              (0x1 << 19)
} __attribute__((packed));

/*
 * DRM_I915_PXP_OPS -
 *
 * PXP is an i915 componment, that helps user space to establish the hardware
 * protected session and manage the status of each alive software session,
 * as well as the life cycle of each session.
 *
 * This ioctl is to allow user space driver to create, set, and destroy each
 * session. It also provides the communication chanel to TEE (Trusted
 * Execution Environment) for the protected hardware session creation.
 */

struct downstream_drm_i915_pxp_ops {
	__u32 action; /* in - specified action of this operation */
#define DOWNSTREAM_DRM_I915_PXP_ACTION_SET_SESSION_STATUS 0
#define DOWNSTREAM_DRM_I915_PXP_ACTION_TEE_IO_MESSAGE 1
#define DOWNSTREAM_DRM_I915_PXP_ACTION_QUERY_PXP_TAG 2

	__u32 status; /* out - status output for this operation */
#define DOWNSTREAM_DRM_I915_PXP_OP_STATUS_SUCCESS 0
#define DOWNSTREAM_DRM_I915_PXP_OP_STATUS_RETRY_REQUIRED 1
#define DOWNSTREAM_DRM_I915_PXP_OP_STATUS_SESSION_NOT_AVAILABLE 2
#define DOWNSTREAM_DRM_I915_PXP_OP_STATUS_ERROR_UNKNOWN 3

	__u64 params; /* in/out - pointer to data matching the action */
} __attribute__((packed));

#define DOWNSTREAM_PERF_VERSION	(1000)

#endif /* __I915_DRM_DOWNSTREAM_H__ */
