/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_TEE_H__
#define __INTEL_PXP_TEE_H__

#include "intel_pxp.h"

void intel_pxp_tee_component_init(struct intel_pxp *pxp);
void intel_pxp_tee_component_fini(struct intel_pxp *pxp);

int intel_pxp_tee_cmd_create_arb_session(struct intel_pxp *pxp);

int intel_pxp_tee_ioctl_io_message(struct intel_pxp *pxp,
				   void __user *msg_in_user_ptr, u32 msg_in_size,
				   void __user *msg_out_user_ptr, u32 *msg_out_size_ptr,
				   u32 msg_out_buf_size);

/* TEE command to create the arbitrary session */
#define PXP_TEE_ARB_CMD_BIN {0x00040002, 0x0000001e, 0x00000000, 0x00000008, 0x00000002, 0x0000000f}
#define PXP_TEE_ARB_CMD_DW_LEN (6)

#endif /* __INTEL_PXP_TEE_H__ */
