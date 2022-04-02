/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Google, Inc.
 */
#ifndef __DRM_INPUT_HELPER_H__
#define __DRM_INPUT_HELPER_H__

#include <linux/input.h>

struct drm_device;

struct drm_input_handler {
	/*
	 * Callback to call for input activity. Will be called in an atomic
	 * context.
	 */
	void (*callback)(struct drm_input_handler *handler);

	struct input_handler handler;
};

#if defined(CONFIG_DRM_INPUT_HELPER)

int drm_input_handle_register(struct drm_device *dev,
			      struct drm_input_handler *handler);
void drm_input_handle_unregister(struct drm_input_handler *handler);

#else /* !CONFIG_DRM_INPUT_HELPER */

static inline int drm_input_handle_register(struct drm_device *dev,
					    struct drm_input_handler *handler)
{
	return 0;
}

static inline void
drm_input_handle_unregister(struct drm_input_handler *handler) {}

#endif /* CONFIG_DRM_INPUT_HELPER */

#endif /* __DRM_INPUT_HELPER_H__ */
