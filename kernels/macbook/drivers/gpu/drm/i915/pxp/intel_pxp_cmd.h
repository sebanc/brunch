/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_CMD_H__
#define __INTEL_PXP_CMD_H__

#include <linux/bits.h>
#include <linux/types.h>

struct intel_pxp;

int intel_pxp_terminate_sessions(struct intel_pxp *pxp, long mask);

static inline int intel_pxp_terminate_session(struct intel_pxp *pxp, u32 id)
{
	return intel_pxp_terminate_sessions(pxp, BIT(id));
}

#endif /* __INTEL_PXP_CMD_H__ */
