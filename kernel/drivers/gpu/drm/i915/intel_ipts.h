/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _INTEL_IPTS_H_
#define _INTEL_IPTS_H_

#include <linux/intel-ipts.h>

struct drm_i915_private;

typedef struct ipts_object {
	struct list_head list;
	struct drm_i915_gem_object *gem_obj;
	struct i915_vma *vma;
	void	*cpu_addr;
} ipts_object_t;

typedef struct ipts_i915 {
	struct drm_i915_private *to_i915;
	struct intel_guc_client *ipts_client;
	struct intel_context *ipts_context;
	struct {
		spinlock_t       lock;
		struct list_head list;
	} buffers;
	ipts_info_t *ipts;
	bool initialized;
	char hardware_id[10];
	struct work_struct raw_data_work;
	void (*handle_processed_data)(ipts_info_t *ipts);
} ipts_i915_t;

extern ipts_i915_t intel_ipts;

void intel_ipts_notify_handle_processed_data(void);
int intel_ipts_guc_submission_enable(void);
void intel_ipts_guc_submission_disable(void);

#endif // _INTEL_IPTS_IF_H_
