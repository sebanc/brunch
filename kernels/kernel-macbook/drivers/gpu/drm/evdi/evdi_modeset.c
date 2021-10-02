// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic_helper.h>
#include "evdi_drm.h"
#include "evdi_drm_drv.h"
#include "evdi_cursor.h"
#include "evdi_params.h"
#include <drm/drm_gem_atomic_helper.h>

static void evdi_crtc_dpms(__always_unused struct drm_crtc *crtc,
			   __always_unused int mode)
{
	EVDI_CHECKPT();
}

static void evdi_crtc_disable(__always_unused struct drm_crtc *crtc)
{
	EVDI_CHECKPT();
}

static void evdi_crtc_destroy(struct drm_crtc *crtc)
{
	EVDI_CHECKPT();
	drm_crtc_cleanup(crtc);
	kfree(crtc);
}

static void evdi_crtc_commit(__always_unused struct drm_crtc *crtc)
{
	EVDI_CHECKPT();
}

static void evdi_crtc_set_nofb(__always_unused struct drm_crtc *crtc)
{
}

static void evdi_crtc_atomic_flush(
	struct drm_crtc *crtc
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	, struct drm_atomic_state *state
#else
	, __always_unused struct drm_crtc_state *old_state
#endif
	)
{
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
#else
	struct drm_crtc_state *crtc_state = crtc->state;
#endif
	struct evdi_device *evdi = crtc->dev->dev_private;


	if (crtc_state->mode_changed && crtc_state->active)
		evdi_painter_mode_changed_notify(evdi, &crtc_state->adjusted_mode);

	if (crtc_state->active_changed)
		evdi_painter_dpms_notify(evdi,
			crtc_state->active ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF);

	evdi_painter_set_vblank(evdi->painter, crtc, crtc_state->event);
	evdi_painter_send_update_ready_if_needed(evdi->painter);
	crtc_state->event = NULL;
}

static void evdi_mark_full_screen_dirty(struct evdi_device *evdi)
{
	const struct drm_clip_rect rect =
		evdi_painter_framebuffer_size(evdi->painter);

	evdi_painter_mark_dirty(evdi, &rect);
	evdi_painter_send_update_ready_if_needed(evdi->painter);
}

static int evdi_crtc_cursor_set(struct drm_crtc *crtc,
				struct drm_file *file,
				uint32_t handle,
				uint32_t width,
				uint32_t height,
				int32_t hot_x,
				int32_t hot_y)
{
	struct drm_device *dev = crtc->dev;
	struct evdi_device *evdi = dev->dev_private;
	struct drm_gem_object *obj = NULL;
	struct evdi_gem_object *eobj = NULL;
	/*
	 * evdi_crtc_cursor_set is callback function using
	 * deprecated cursor entry point.
	 * There is no info about underlaying pixel format.
	 * Hence we are assuming that it is in ARGB 32bpp format.
	 * This format it the only one supported in cursor composition
	 * function.
	 * This format is also enforced during framebuffer creation.
	 *
	 * Proper format will be available when driver start support
	 * universal planes for cursor.
	 */
	uint32_t format = DRM_FORMAT_ARGB8888;
	uint32_t stride = 4 * width;

	EVDI_CHECKPT();
	if (handle) {
		mutex_lock(&dev->struct_mutex);
		obj = drm_gem_object_lookup(file, handle);
		if (obj)
			eobj = to_evdi_bo(obj);
		else
			EVDI_ERROR("Failed to lookup gem object.\n");
		mutex_unlock(&dev->struct_mutex);
	}

	evdi_cursor_set(evdi->cursor,
			eobj, width, height, hot_x, hot_y,
			format, stride);
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	drm_gem_object_put(obj);
#else
	drm_gem_object_put_unlocked(obj);
#endif

	/*
	 * For now we don't care whether the application wanted the mouse set,
	 * or not.
	 */
	if (evdi->cursor_events_enabled)
		evdi_painter_send_cursor_set(evdi->painter, evdi->cursor);
	else
		evdi_mark_full_screen_dirty(evdi);
	return 0;
}

static int evdi_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct evdi_device *evdi = dev->dev_private;

	EVDI_CHECKPT();
	evdi_cursor_move(evdi->cursor, x, y);

	if (evdi->cursor_events_enabled)
		evdi_painter_send_cursor_move(evdi->painter, evdi->cursor);
	else
		evdi_mark_full_screen_dirty(evdi);

	return 0;
}

static struct drm_crtc_helper_funcs evdi_helper_funcs = {
	.mode_set_nofb  = evdi_crtc_set_nofb,
	.atomic_flush   = evdi_crtc_atomic_flush,

	.dpms           = evdi_crtc_dpms,
	.commit         = evdi_crtc_commit,
	.disable        = evdi_crtc_disable
};

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int evdi_enable_vblank(__always_unused struct drm_crtc *crtc)
{
	return 1;
}

static void evdi_disable_vblank(__always_unused struct drm_crtc *crtc)
{
}
#endif

static const struct drm_crtc_funcs evdi_crtc_funcs = {
	.reset                  = drm_atomic_helper_crtc_reset,
	.destroy                = evdi_crtc_destroy,
	.set_config             = drm_atomic_helper_set_config,
	.page_flip              = drm_atomic_helper_page_flip,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_crtc_destroy_state,

	.cursor_set2            = evdi_crtc_cursor_set,
	.cursor_move            = evdi_crtc_cursor_move,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.enable_vblank          = evdi_enable_vblank,
	.disable_vblank         = evdi_disable_vblank,
#endif
};

static void evdi_plane_atomic_update(struct drm_plane *plane,
				     struct drm_atomic_state *state)
{
	struct drm_plane_state *old_plane_state, *plane_state;
	struct evdi_device *evdi;
	struct evdi_painter *painter;
	struct drm_crtc *crtc;

	if (!plane || !plane->state) {
		EVDI_WARN("Plane state is null\n");
		return;
	}

	if (!plane->dev || !plane->dev->dev_private) {
		EVDI_WARN("Plane device is null\n");
		return;
	}

	old_plane_state = drm_atomic_get_old_plane_state(state, plane);
	plane_state = plane->state;
	evdi = plane->dev->dev_private;
	painter = evdi->painter;
	crtc = plane_state->crtc;

	if (!old_plane_state->crtc && plane_state->crtc)
		evdi_painter_dpms_notify(evdi, DRM_MODE_DPMS_ON);
	else if (old_plane_state->crtc && !plane_state->crtc)
		evdi_painter_dpms_notify(evdi, DRM_MODE_DPMS_OFF);

	if (plane_state->fb) {
		struct drm_framebuffer *fb = plane_state->fb;
		struct drm_framebuffer *old_fb = old_plane_state->fb;
		struct evdi_framebuffer *efb = to_evdi_fb(fb);

		const struct drm_clip_rect fullscreen_rect = {
			0, 0, fb->width, fb->height
		};

		if (!old_fb && crtc)
			evdi_painter_force_full_modeset(painter);

		if (old_fb &&
		    fb->format && old_fb->format &&
		    fb->format->format != old_fb->format->format)
			evdi_painter_force_full_modeset(painter);

		if (fb != old_fb ||
		    evdi_painter_needs_full_modeset(painter)) {

			evdi_painter_set_scanout_buffer(painter, efb);
			evdi_painter_mark_dirty(evdi, &fullscreen_rect);
		} else if (evdi_painter_get_num_dirts(painter) == 0) {
			evdi_painter_mark_dirty(evdi, &fullscreen_rect);
		}
	}
}

static void evdi_cursor_atomic_get_rect(struct drm_clip_rect *rect,
					struct drm_plane_state *state)
{
	rect->x1 = (state->crtc_x < 0) ? 0 : state->crtc_x;
	rect->y1 = (state->crtc_y < 0) ? 0 : state->crtc_y;
	rect->x2 = state->crtc_x + state->crtc_w;
	rect->y2 = state->crtc_y + state->crtc_h;
}

static void evdi_cursor_atomic_update(struct drm_plane *plane,
				      struct drm_atomic_state *state)
{
	struct drm_plane_state *old_plane_state;

	old_plane_state = drm_atomic_get_old_plane_state(state, plane);

	if (plane && plane->state && plane->dev && plane->dev->dev_private) {
		struct drm_plane_state *state = plane->state;
		struct evdi_device *evdi = plane->dev->dev_private;
		struct drm_framebuffer *fb = state->fb;
		struct evdi_framebuffer *efb = to_evdi_fb(fb);

		struct drm_clip_rect old_rect;
		struct drm_clip_rect rect;
		bool cursor_changed = false;
		bool cursor_position_changed = false;
		int32_t cursor_position_x = 0;
		int32_t cursor_position_y = 0;

		mutex_lock(&plane->dev->struct_mutex);

		evdi_cursor_position(evdi->cursor, &cursor_position_x,
		&cursor_position_y);
		evdi_cursor_move(evdi->cursor, state->crtc_x, state->crtc_y);
		cursor_position_changed = cursor_position_x != state->crtc_x ||
					  cursor_position_y != state->crtc_y;

		if (fb != old_plane_state->fb) {
			if (fb != NULL) {
				uint32_t stride = 4 * fb->width;

				evdi_cursor_set(evdi->cursor,
						efb->obj,
						fb->width,
						fb->height,
						0,
						0,
						fb->format->format,
						stride);
			}

			evdi_cursor_enable(evdi->cursor, fb != NULL);
			cursor_changed = true;
		}

		mutex_unlock(&plane->dev->struct_mutex);
		if (!evdi->cursor_events_enabled) {
			evdi_cursor_atomic_get_rect(&old_rect, old_plane_state);
			evdi_cursor_atomic_get_rect(&rect, state);

			evdi_painter_mark_dirty(evdi, &old_rect);
			evdi_painter_mark_dirty(evdi, &rect);
			return;
		}
		if (cursor_changed)
			evdi_painter_send_cursor_set(evdi->painter,
						     evdi->cursor);
		if (cursor_position_changed)
			evdi_painter_send_cursor_move(evdi->painter,
						      evdi->cursor);
	}
}

static const struct drm_plane_helper_funcs evdi_plane_helper_funcs = {
	.atomic_update = evdi_plane_atomic_update,
	.prepare_fb = drm_gem_plane_helper_prepare_fb
};

static const struct drm_plane_helper_funcs evdi_cursor_helper_funcs = {
	.atomic_update = evdi_cursor_atomic_update,
	.prepare_fb = drm_gem_plane_helper_prepare_fb
};

static const struct drm_plane_funcs evdi_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static const uint32_t formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ABGR8888,
};

static struct drm_plane *evdi_create_plane(
		struct drm_device *dev,
		enum drm_plane_type type,
		const struct drm_plane_helper_funcs *helper_funcs)
{
	struct drm_plane *plane;
	int ret;

	plane = kzalloc(sizeof(*plane), GFP_KERNEL);
	if (plane == NULL) {
		EVDI_ERROR("Failed to allocate primary plane\n");
		return NULL;
	}
	plane->format_default = true;

	ret = drm_universal_plane_init(dev,
				       plane,
				       0xFF,
				       &evdi_plane_funcs,
				       formats,
				       ARRAY_SIZE(formats),
				       NULL,
				       type,
				       NULL
				       );

	if (ret) {
		EVDI_ERROR("Failed to initialize primary plane\n");
		kfree(plane);
		return NULL;
	}

	drm_plane_helper_add(plane, helper_funcs);

	return plane;
}

static int evdi_crtc_init(struct drm_device *dev)
{
	struct drm_crtc *crtc = NULL;
	struct drm_plane *primary_plane = NULL;
	struct drm_plane *cursor_plane = NULL;
	int status = 0;

	EVDI_CHECKPT();
	crtc = kzalloc(sizeof(struct drm_crtc), GFP_KERNEL);
	if (crtc == NULL)
		return -ENOMEM;

	primary_plane = evdi_create_plane(dev, DRM_PLANE_TYPE_PRIMARY,
					  &evdi_plane_helper_funcs);
	status = drm_crtc_init_with_planes(dev, crtc,
					   primary_plane, cursor_plane,
					   &evdi_crtc_funcs,
					   NULL
					   );

	EVDI_DEBUG("drm_crtc_init: %d p%p\n", status, primary_plane);
	drm_crtc_helper_add(crtc, &evdi_helper_funcs);

	return 0;
}

static int evdi_atomic_check(struct drm_device *dev,
			     struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i;
	struct evdi_device *evdi = dev->dev_private;

	if (evdi_painter_needs_full_modeset(evdi->painter)) {
		for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
			crtc_state->active_changed = true;
			crtc_state->mode_changed = true;
		}
	}

	return drm_atomic_helper_check(dev, state);
}

static const struct drm_mode_config_funcs evdi_mode_funcs = {
	.fb_create = evdi_fb_user_fb_create,
	.output_poll_changed = NULL,
	.atomic_commit = drm_atomic_helper_commit,
	.atomic_check = evdi_atomic_check
};

void evdi_modeset_init(struct drm_device *dev)
{
	struct drm_encoder *encoder;

	EVDI_CHECKPT();

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 64;
	dev->mode_config.min_height = 64;

	dev->mode_config.max_width = 3840;
	dev->mode_config.max_height = 2160;

	dev->mode_config.prefer_shadow = 0;
	dev->mode_config.preferred_depth = 24;

	dev->mode_config.funcs = &evdi_mode_funcs;

	evdi_crtc_init(dev);

	encoder = evdi_encoder_init(dev);

	evdi_connector_init(dev, encoder);

	drm_mode_config_reset(dev);
}

void evdi_modeset_cleanup(struct drm_device *dev)
{
	EVDI_CHECKPT();
	drm_mode_config_cleanup(dev);
}
