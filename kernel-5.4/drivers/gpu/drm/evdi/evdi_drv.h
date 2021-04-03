/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2019 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_DRV_H
#define EVDI_DRV_H

#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_rect.h>
# include <drm/drm_gem.h>
#include <drm/drm_cache.h>
#include <linux/dma-resv.h>
#include "evdi_debug.h"

#define DRIVER_NAME   "evdi"
#define DRIVER_DESC   "Extensible Virtual Display Interface"
#define DRIVER_DATE   "20180315"

#define DRIVER_MAJOR      1
#define DRIVER_MINOR      5
#define DRIVER_PATCHLEVEL 0

struct evdi_fbdev;
struct evdi_painter;

extern bool evdi_enable_cursor_blending __read_mostly;

struct evdi_device {
	struct device *dev;
	struct drm_device *ddev;
	struct evdi_cursor *cursor;
	uint32_t sku_area_limit;

	struct evdi_fbdev *fbdev;
	struct evdi_painter *painter;

	atomic_t frame_count;

	int dev_index;
};

struct evdi_gem_object {
	struct drm_gem_object base;
	struct page **pages;
	void *vmapping;
	struct sg_table *sg;
	struct dma_resv *resv;
	struct dma_resv _resv;
};

#define to_evdi_bo(x) container_of(x, struct evdi_gem_object, base)

struct evdi_framebuffer {
	struct drm_framebuffer base;
	struct evdi_gem_object *obj;
	bool active;
};

#define to_evdi_fb(x) container_of(x, struct evdi_framebuffer, base)

/* modeset */
void evdi_modeset_init(struct drm_device *dev);
void evdi_modeset_cleanup(struct drm_device *dev);
int evdi_connector_init(struct drm_device *dev, struct drm_encoder *encoder);

struct drm_encoder *evdi_encoder_init(struct drm_device *dev);

int evdi_driver_load(struct drm_device *dev, unsigned long flags);
void evdi_driver_unload(struct drm_device *dev);
void evdi_driver_preclose(struct drm_device *dev, struct drm_file *file_priv);

#ifdef CONFIG_COMPAT
long evdi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif

#ifdef CONFIG_FB
int evdi_fbdev_init(struct drm_device *dev);
void evdi_fbdev_cleanup(struct drm_device *dev);
void evdi_fbdev_unplug(struct drm_device *dev);
#endif /* CONFIG_FB */
struct drm_framebuffer *evdi_fb_user_fb_create(
				struct drm_device *dev,
				struct drm_file *file,
				const struct drm_mode_fb_cmd2 *mode_cmd);

int evdi_dumb_create(struct drm_file *file_priv,
		     struct drm_device *dev, struct drm_mode_create_dumb *args);
int evdi_gem_mmap(struct drm_file *file_priv,
		  struct drm_device *dev, uint32_t handle, uint64_t *offset);

void evdi_gem_free_object(struct drm_gem_object *gem_obj);
struct evdi_gem_object *evdi_gem_alloc_object(struct drm_device *dev,
					      size_t size);
uint32_t evdi_gem_object_handle_lookup(struct drm_file *filp,
				      struct drm_gem_object *obj);

struct drm_gem_object *evdi_gem_prime_import(struct drm_device *dev,
					     struct dma_buf *dma_buf);
struct dma_buf *evdi_gem_prime_export(struct drm_gem_object *obj, int flags);

int evdi_gem_vmap(struct evdi_gem_object *obj);
void evdi_gem_vunmap(struct evdi_gem_object *obj);
int evdi_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
vm_fault_t evdi_gem_fault(struct vm_fault *vmf);
void evdi_stats_init(struct evdi_device *evdi);
void evdi_stats_cleanup(struct evdi_device *evdi);

bool evdi_painter_is_connected(struct evdi_device *evdi);
void evdi_painter_close(struct evdi_device *evdi, struct drm_file *file);
u8 *evdi_painter_get_edid_copy(struct evdi_device *evdi);
void evdi_painter_mark_dirty(struct evdi_device *evdi,
			     const struct drm_clip_rect *rect);
void evdi_painter_send_update_ready_if_needed(struct evdi_device *evdi);
void evdi_painter_dpms_notify(struct evdi_device *evdi, int mode);
void evdi_painter_mode_changed_notify(struct evdi_device *evdi,
				      struct drm_display_mode *mode);
void evdi_painter_crtc_state_notify(struct evdi_device *evdi, int state);
unsigned int evdi_painter_poll(struct file *filp,
			       struct poll_table_struct *wait);

int evdi_painter_status_ioctl(struct drm_device *drm_dev, void *data,
			      struct drm_file *file);
int evdi_painter_connect_ioctl(struct drm_device *drm_dev, void *data,
			       struct drm_file *file);
int evdi_painter_grabpix_ioctl(struct drm_device *drm_dev, void *data,
			       struct drm_file *file);
int evdi_painter_request_update_ioctl(struct drm_device *drm_dev, void *data,
				      struct drm_file *file);

int evdi_painter_init(struct evdi_device *evdi);
void evdi_painter_cleanup(struct evdi_device *evdi);
void evdi_painter_set_new_scanout_buffer(struct evdi_device *evdi,
					 struct evdi_framebuffer *buffer);
void evdi_painter_commit_scanout_buffer(struct evdi_device *evdi);

struct drm_clip_rect evdi_framebuffer_sanitize_rect(
			const struct evdi_framebuffer *fb,
			const struct drm_clip_rect *rect);

int evdi_driver_setup_early(struct drm_device *dev);
void evdi_driver_setup_late(struct drm_device *dev);

void evdi_painter_send_cursor_set(struct evdi_painter *painter,
				  struct evdi_cursor *cursor);
void evdi_painter_send_cursor_move(struct evdi_painter *painter,
				   struct evdi_cursor *cursor);
bool evdi_painter_needs_full_modeset(struct evdi_device *evdi);
struct drm_clip_rect evdi_painter_framebuffer_size(
			struct evdi_painter *painter);

int evdi_fb_get_bpp(uint32_t format);
#endif

