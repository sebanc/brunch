/* SPDX-License-Identifier: GPL-2.0-only
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

#ifndef EVDI_DRV_H
#define EVDI_DRV_H

#include <linux/module.h>
#include <linux/version.h>
#include <linux/device.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#include <drm/drm_drv.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_irq.h>
#include <drm/drm_vblank.h>
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_gem.h>
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE || defined(EL8)
#include <linux/dma-resv.h>
#else
#include <linux/reservation.h>
#endif
#include "evdi_debug.h"


struct evdi_fbdev;
struct evdi_painter;

struct evdi_device {
	struct drm_device *ddev;
	struct drm_connector *conn;
	struct evdi_cursor *cursor;
	bool cursor_events_enabled;

	uint32_t sku_area_limit;

	struct evdi_fbdev *fbdev;
	struct evdi_painter *painter;
	struct i2c_adapter *i2c_adapter;

	int dev_index;
};

struct evdi_gem_object {
	struct drm_gem_object base;
	struct page **pages;
	void *vmapping;
	struct sg_table *sg;
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE || defined(EL8)
	struct dma_resv *resv;
	struct dma_resv _resv;
#else
	struct reservation_object *resv;
	struct reservation_object _resv;
#endif
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
void evdi_driver_postclose(struct drm_device *dev, struct drm_file *file_priv);

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

struct sg_table *evdi_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
evdi_prime_import_sg_table(struct drm_device *dev,
			   struct dma_buf_attachment *attach,
			   struct sg_table *sg);

int evdi_gem_vmap(struct evdi_gem_object *obj);
void evdi_gem_vunmap(struct evdi_gem_object *obj);
int evdi_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);

#if KERNEL_VERSION(4, 17, 0) <= LINUX_VERSION_CODE
vm_fault_t evdi_gem_fault(struct vm_fault *vmf);
#else
int evdi_gem_fault(struct vm_fault *vmf);
#endif

bool evdi_painter_is_connected(struct evdi_painter *painter);
void evdi_painter_close(struct evdi_device *evdi, struct drm_file *file);
u8 *evdi_painter_get_edid_copy(struct evdi_device *evdi);
int evdi_painter_get_num_dirts(struct evdi_painter *painter);
void evdi_painter_mark_dirty(struct evdi_device *evdi,
			     const struct drm_clip_rect *rect);
void evdi_painter_set_vblank(struct evdi_painter *painter,
			     struct drm_crtc *crtc,
			     struct drm_pending_vblank_event *vblank);
void evdi_painter_send_update_ready_if_needed(struct evdi_painter *painter);
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
int evdi_painter_ddcci_response_ioctl(struct drm_device *drm_dev, void *data,
				      struct drm_file *file);
int evdi_painter_enable_cursor_events_ioctl(struct drm_device *drm_dev, void *data,
					  struct drm_file *file);

int evdi_painter_init(struct evdi_device *evdi);
void evdi_painter_cleanup(struct evdi_painter *painter);
void evdi_painter_set_scanout_buffer(struct evdi_painter *painter,
				     struct evdi_framebuffer *buffer);

struct drm_clip_rect evdi_framebuffer_sanitize_rect(
			const struct evdi_framebuffer *fb,
			const struct drm_clip_rect *rect);

struct drm_device *evdi_drm_device_create(struct device *parent);
int evdi_drm_device_remove(struct drm_device *dev);

void evdi_painter_send_cursor_set(struct evdi_painter *painter,
				  struct evdi_cursor *cursor);
void evdi_painter_send_cursor_move(struct evdi_painter *painter,
				   struct evdi_cursor *cursor);
bool evdi_painter_needs_full_modeset(struct evdi_painter *painter);
void evdi_painter_force_full_modeset(struct evdi_painter *painter);
struct drm_clip_rect evdi_painter_framebuffer_size(struct evdi_painter *painter);
bool evdi_painter_i2c_data_notify(struct evdi_painter *painter, struct i2c_msg *msg);

int evdi_fb_get_bpp(uint32_t format);
#endif
