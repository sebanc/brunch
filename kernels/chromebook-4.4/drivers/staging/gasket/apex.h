/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Apex kernel-userspace interface definitions.
 *
 * Copyright (C) 2018 Google, Inc.
 */
#ifndef __APEX_H__
#define __APEX_H__

#include <linux/ioctl.h>

/* Clock Gating ioctl. */
struct apex_gate_clock_ioctl {
	/* Enter or leave clock gated state. */
	u64 enable;

	/* If set, enter clock gating state, regardless of custom block's
	 * internal idle state
	 */
	u64 force_idle;
};

/*
 * Performance expectation ioctl.
 */
enum apex_performance_expectation {
	APEX_PERFORMANCE_LOW = 0,
	APEX_PERFORMANCE_MED = 1,
	APEX_PERFORMANCE_HIGH = 2,
	APEX_PERFORMANCE_MAX = 3,
};

struct apex_performance_expectation_ioctl {
	/* Expected performance from runtime. */
	u32 performance;
};

/* Base number for all Apex-common IOCTLs */
#define APEX_IOCTL_BASE 0x7F

/* Enable/Disable clock gating. */
#define APEX_IOCTL_GATE_CLOCK                                                  \
	_IOW(APEX_IOCTL_BASE, 0, struct apex_gate_clock_ioctl)

/*
 * Change performance expectation.
 */
#define APEX_IOCTL_PERFORMANCE_EXPECTATION                                     \
	_IOW(APEX_IOCTL_BASE, 1, struct apex_performance_expectation_ioctl)

#endif /* __APEX_H__ */
