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

#include <linux/slab.h>
#ifdef CONFIG_FB
#include <linux/fb.h>
#endif /* CONFIG_FB */
#include <linux/dma-buf.h>
#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic.h>
#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
#include <drm/drm_damage_helper.h>
#endif
#include "evdi_drm_drv.h"


struct evdi_fbdev {
	struct drm_fb_helper helper;
	struct evdi_framebuffer efb;
	struct list_head fbdev_list;
	struct fb_ops fb_ops;
	int fb_count;
};

struct drm_clip_rect evdi_framebuffer_sanitize_rect(
				const struct evdi_framebuffer *fb,
				const struct drm_clip_rect *dirty_rect)
{
	struct drm_clip_rect rect = *dirty_rect;

	if (rect.x1 > rect.x2) {
		unsigned short tmp = rect.x2;

		EVDI_WARN("Wrong clip rect: x1 > x2\n");
		rect.x2 = rect.x1;
		rect.x1 = tmp;
	}

	if (rect.y1 > rect.y2) {
		unsigned short tmp = rect.y2;

		EVDI_WARN("Wrong clip rect: y1 > y2\n");
		rect.y2 = rect.y1;
		rect.y1 = tmp;
	}


	if (rect.x1 > fb->base.width) {
		EVDI_WARN("Wrong clip rect: x1 > fb.width\n");
		rect.x1 = fb->base.width;
	}

	if (rect.y1 > fb->base.height) {
		EVDI_WARN("Wrong clip rect: y1 > fb.height\n");
		rect.y1 = fb->base.height;
	}

	if (rect.x2 > fb->base.width) {
		EVDI_WARN("Wrong clip rect: x2 > fb.width\n");
		rect.x2 = fb->base.width;
	}

	if (rect.y2 > fb->base.height) {
		EVDI_WARN("Wrong clip rect: y2 > fb.height\n");
		rect.y2 = fb->base.height;
	}

	return rect;
}

static int evdi_handle_damage(struct evdi_framebuffer *fb,
		       int x, int y, int width, int height)
{
	const struct drm_clip_rect dirty_rect = { x, y, x + width, y + height };
	const struct drm_clip_rect rect =
		evdi_framebuffer_sanitize_rect(fb, &dirty_rect);
	struct drm_device *dev = fb->base.dev;
	struct evdi_device *evdi = dev->dev_private;

	EVDI_CHECKPT();

	if (!fb->active)
		return 0;
	evdi_painter_set_scanout_buffer(evdi->painter, fb);
	evdi_painter_mark_dirty(evdi, &rect);

	return 0;
}

#ifdef CONFIG_FB
static int evdi_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	if (offset > info->fix.smem_len ||
	    size > info->fix.smem_len - offset)
		return -EINVAL;

	pos = (unsigned long)info->fix.smem_start + offset;

	pr_notice("mmap() framebuffer addr:%lu size:%lu\n", pos, size);

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return 0;
}

static void evdi_fb_fillrect(struct fb_info *info,
			     const struct fb_fillrect *rect)
{
	struct evdi_fbdev *efbdev = info->par;

	EVDI_CHECKPT();
	sys_fillrect(info, rect);
	evdi_handle_damage(&efbdev->efb, rect->dx, rect->dy, rect->width,
			   rect->height);
}

static void evdi_fb_copyarea(struct fb_info *info,
			     const struct fb_copyarea *region)
{
	struct evdi_fbdev *efbdev = info->par;

	EVDI_CHECKPT();
	sys_copyarea(info, region);
	evdi_handle_damage(&efbdev->efb, region->dx, region->dy, region->width,
			   region->height);
}

static void evdi_fb_imageblit(struct fb_info *info,
			      const struct fb_image *image)
{
	struct evdi_fbdev *efbdev = info->par;

	EVDI_CHECKPT();
	sys_imageblit(info, image);
	evdi_handle_damage(&efbdev->efb, image->dx, image->dy, image->width,
			   image->height);
}

/*
 * It's common for several clients to have framebuffer open simultaneously.
 * e.g. both fbcon and X. Makes things interesting.
 * Assumes caller is holding info->lock (for open and release at least)
 */
static int evdi_fb_open(struct fb_info *info, int user)
{
	struct evdi_fbdev *efbdev = info->par;

	efbdev->fb_count++;
	pr_notice("open /dev/fb%d user=%d fb_info=%p count=%d\n",
		  info->node, user, info, efbdev->fb_count);

	return 0;
}

/*
 * Assumes caller is holding info->lock mutex (for open and release at least)
 */
static int evdi_fb_release(struct fb_info *info, int user)
{
	struct evdi_fbdev *efbdev = info->par;

	efbdev->fb_count--;

	pr_warn("released /dev/fb%d user=%d count=%d\n",
		info->node, user, efbdev->fb_count);

	return 0;
}

static struct fb_ops evdifb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = evdi_fb_fillrect,
	.fb_copyarea = evdi_fb_copyarea,
	.fb_imageblit = evdi_fb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
	.fb_mmap = evdi_fb_mmap,
	.fb_open = evdi_fb_open,
	.fb_release = evdi_fb_release,
};
#endif /* CONFIG_FB */

#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
#else
/*
 * Function taken from
 * https://lists.freedesktop.org/archives/dri-devel/2018-September/188716.html
 */
static int evdi_user_framebuffer_dirty(
		struct drm_framebuffer *fb,
		__maybe_unused struct drm_file *file_priv,
		__always_unused unsigned int flags,
		__always_unused unsigned int color,
		__always_unused struct drm_clip_rect *clips,
		__always_unused unsigned int num_clips)
{
	struct evdi_framebuffer *efb = to_evdi_fb(fb);
	struct drm_device *dev = efb->base.dev;
	struct evdi_device *evdi = dev->dev_private;

	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state;
	struct drm_plane *plane;
	int ret = 0;
	int i;

	EVDI_CHECKPT();

	drm_modeset_acquire_init(&ctx,
		/*
		 * When called from ioctl, we are interruptable,
		 * but not when called internally (ie. defio worker)
		 */
		file_priv ? DRM_MODESET_ACQUIRE_INTERRUPTIBLE :	0);

	state = drm_atomic_state_alloc(fb->dev);
	if (!state) {
		ret = -ENOMEM;
		goto out;
	}
	state->acquire_ctx = &ctx;

	for (i = 0; i < num_clips; ++i)
		evdi_painter_mark_dirty(evdi, &clips[i]);

retry:

	drm_for_each_plane(plane, fb->dev) {
		struct drm_plane_state *plane_state;

		if (plane->state->fb != fb)
			continue;

		/*
		 * Even if it says 'get state' this function will create and
		 * initialize state if it does not exists. We use this property
		 * to force create state.
		 */
		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			goto out;
		}
	}

	ret = drm_atomic_commit(state);

out:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		ret = drm_modeset_backoff(&ctx);
		if (!ret)
			goto retry;
	}

	if (state)
		drm_atomic_state_put(state);

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;
}
#endif

static int evdi_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					       struct drm_file *file_priv,
					       unsigned int *handle)
{
	struct evdi_framebuffer *efb = to_evdi_fb(fb);

	return drm_gem_handle_create(file_priv, &efb->obj->base, handle);
}

static void evdi_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct evdi_framebuffer *efb = to_evdi_fb(fb);

	EVDI_CHECKPT();
	if (efb->obj)
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
		drm_gem_object_put(&efb->obj->base);
#else
		drm_gem_object_put_unlocked(&efb->obj->base);
#endif
	drm_framebuffer_cleanup(fb);
	kfree(efb);
}

static const struct drm_framebuffer_funcs evdifb_funcs = {
	.create_handle = evdi_user_framebuffer_create_handle,
	.destroy = evdi_user_framebuffer_destroy,
#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
	.dirty = drm_atomic_helper_dirtyfb,
#else
	.dirty = evdi_user_framebuffer_dirty,
#endif
};

static int
evdi_framebuffer_init(struct drm_device *dev,
		      struct evdi_framebuffer *efb,
		      const struct drm_mode_fb_cmd2 *mode_cmd,
		      struct evdi_gem_object *obj)
{
	efb->obj = obj;
	drm_helper_mode_fill_fb_struct(dev, &efb->base, mode_cmd);
	return drm_framebuffer_init(dev, &efb->base, &evdifb_funcs);
}

#ifdef CONFIG_FB
static int evdifb_create(struct drm_fb_helper *helper,
			 struct drm_fb_helper_surface_size *sizes)
{
	struct evdi_fbdev *efbdev = (struct evdi_fbdev *)helper;
	struct drm_device *dev = efbdev->helper.dev;
	struct fb_info *info;
	struct device *device = dev->dev;
	struct drm_framebuffer *fb;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct evdi_gem_object *obj;
	uint32_t size;
	int ret = 0;

	if (sizes->surface_bpp == 24) {
		sizes->surface_bpp = 32;
	} else if (sizes->surface_bpp != 32) {
		EVDI_ERROR("Not supported pixel format (bpp=%d)\n",
			   sizes->surface_bpp);
		return -EINVAL;
	}

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = mode_cmd.width * ((sizes->surface_bpp + 7) / 8);

	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = ALIGN(size, PAGE_SIZE);

	obj = evdi_gem_alloc_object(dev, size);
	if (!obj)
		goto out;

	ret = evdi_gem_vmap(obj);
	if (ret) {
		DRM_ERROR("failed to vmap fb\n");
		goto out_gfree;
	}

	info = framebuffer_alloc(0, device);
	if (!info) {
		ret = -ENOMEM;
		goto out_gfree;
	}
	info->par = efbdev;

	ret = evdi_framebuffer_init(dev, &efbdev->efb, &mode_cmd, obj);
	if (ret)
		goto out_gfree;

	fb = &efbdev->efb.base;

	efbdev->helper.fb = fb;
	efbdev->helper.fbdev = info;

	strcpy(info->fix.id, "evdidrmfb");

	info->screen_base = efbdev->efb.obj->vmapping;
	info->fix.smem_len = size;
	info->fix.smem_start = (unsigned long)efbdev->efb.obj->vmapping;

#if KERNEL_VERSION(4, 20, 0) <= LINUX_VERSION_CODE || defined(EL8)
	info->flags = FBINFO_DEFAULT;
#else
	info->flags = FBINFO_DEFAULT | FBINFO_CAN_FORCE_OUTPUT;
#endif

	efbdev->fb_ops = evdifb_ops;
	info->fbops = &efbdev->fb_ops;

#if KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE || defined(EL8)
	drm_fb_helper_fill_info(info, &efbdev->helper, sizes);
#else
	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(info, &efbdev->helper, sizes->fb_width,
			       sizes->fb_height);
#endif

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out_gfree;
	}

	DRM_DEBUG_KMS("allocated %dx%d vmal %p\n",
		      fb->width, fb->height, efbdev->efb.obj->vmapping);

	return ret;
 out_gfree:
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	drm_gem_object_put(&efbdev->efb.obj->base);
#else
	drm_gem_object_put_unlocked(&efbdev->efb.obj->base);
#endif
 out:
	return ret;
}

static struct drm_fb_helper_funcs evdi_fb_helper_funcs = {
	.fb_probe = evdifb_create,
};

static void evdi_fbdev_destroy(__always_unused struct drm_device *dev,
			       struct evdi_fbdev *efbdev)
{
	struct fb_info *info;

	if (efbdev->helper.fbdev) {
		info = efbdev->helper.fbdev;
		unregister_framebuffer(info);
		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);

		framebuffer_release(info);
	}
	drm_fb_helper_fini(&efbdev->helper);
	if (efbdev->efb.obj) {
		drm_framebuffer_unregister_private(&efbdev->efb.base);
		drm_framebuffer_cleanup(&efbdev->efb.base);
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
		drm_gem_object_put(&efbdev->efb.obj->base);
#else
		drm_gem_object_put_unlocked(&efbdev->efb.obj->base);
#endif
	}
}

int evdi_fbdev_init(struct drm_device *dev)
{
	struct evdi_device *evdi;
	struct evdi_fbdev *efbdev;
	int ret;

	evdi = dev->dev_private;
	efbdev = kzalloc(sizeof(struct evdi_fbdev), GFP_KERNEL);
	if (!efbdev)
		return -ENOMEM;

	evdi->fbdev = efbdev;
	drm_fb_helper_prepare(dev, &efbdev->helper, &evdi_fb_helper_funcs);

#if KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE
	ret = drm_fb_helper_init(dev, &efbdev->helper);
#else
	ret = drm_fb_helper_init(dev, &efbdev->helper, 1);
#endif
	if (ret) {
		kfree(efbdev);
		return ret;
	}

#if KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE
#else
	drm_fb_helper_single_add_all_connectors(&efbdev->helper);
#endif

	ret = drm_fb_helper_initial_config(&efbdev->helper, 32);
	if (ret) {
		drm_fb_helper_fini(&efbdev->helper);
		kfree(efbdev);
	}
	return ret;
}

void evdi_fbdev_cleanup(struct drm_device *dev)
{
	struct evdi_device *evdi = dev->dev_private;

	if (!evdi->fbdev)
		return;

	evdi_fbdev_destroy(dev, evdi->fbdev);
	kfree(evdi->fbdev);
	evdi->fbdev = NULL;
}

void evdi_fbdev_unplug(struct drm_device *dev)
{
	struct evdi_device *evdi = dev->dev_private;
	struct evdi_fbdev *efbdev;

	if (!evdi->fbdev)
		return;

	efbdev = evdi->fbdev;
	if (efbdev->helper.fbdev) {
		struct fb_info *info;

		info = efbdev->helper.fbdev;
#if KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE
		unregister_framebuffer(info);
#else
		unlink_framebuffer(info);
#endif
	}
}
#endif /* CONFIG_FB */

int evdi_fb_get_bpp(uint32_t format)
{
	const struct drm_format_info *info = drm_format_info(format);

	if (!info)
		return 0;
	return info->cpp[0] * 8;
}

struct drm_framebuffer *evdi_fb_user_fb_create(
					struct drm_device *dev,
					struct drm_file *file,
					const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *obj;
	struct evdi_framebuffer *efb;
	int ret;
	uint32_t size;
	int bpp = evdi_fb_get_bpp(mode_cmd->pixel_format);

	if (bpp != 32) {
		EVDI_ERROR("Unsupported bpp (%d)\n", bpp);
		return ERR_PTR(-EINVAL);
	}

	obj = drm_gem_object_lookup(file, mode_cmd->handles[0]);
	if (obj == NULL)
		return ERR_PTR(-ENOENT);

	size = mode_cmd->offsets[0] + mode_cmd->pitches[0] * mode_cmd->height;
	size = ALIGN(size, PAGE_SIZE);

	if (size > obj->size) {
		DRM_ERROR("object size not sufficient for fb %d %zu %u %d %d\n",
			  size, obj->size, mode_cmd->offsets[0],
			  mode_cmd->pitches[0], mode_cmd->height);
		goto err_no_mem;
	}

	efb = kzalloc(sizeof(*efb), GFP_KERNEL);
	if (efb == NULL)
		goto err_no_mem;
	efb->base.obj[0] = obj;

	ret = evdi_framebuffer_init(dev, efb, mode_cmd, to_evdi_bo(obj));
	if (ret)
		goto err_inval;
	return &efb->base;

 err_no_mem:
	drm_gem_object_put(obj);
	return ERR_PTR(-ENOMEM);
 err_inval:
	kfree(efb);
	drm_gem_object_put(obj);
	return ERR_PTR(-EINVAL);
}
