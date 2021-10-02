/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_CMD_H__
#define __INTEL_PXP_CMD_H__

#include "gt/intel_gt_buffer_pool.h"
#include "intel_pxp.h"

int intel_pxp_cmd_submit(struct intel_pxp *pxp, u32 *cmd,
			 int cmd_size_in_dw);
int intel_pxp_cmd_add_prolog(struct intel_pxp *pxp, u32 *cmd,
			     int session_type,
			     int session_index);
int intel_pxp_cmd_add_epilog(u32 *cmd);
int intel_pxp_cmd_add_inline_termination(u32 *cmd);
int intel_pxp_cmd_terminate_all_hw_session(struct intel_pxp *pxp,
					   int session_type);

#endif /* __INTEL_PXP_SM_H__ */
