// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2018 DisplayLink (UK) Ltd.
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
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include "evdi_drv.h"

struct evdi_fbdev {
	struct drm_fb_helper helper;
	struct evdi_framebuffer ufb;
	struct list_head fbdev_list;
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
		EVDI_VERBOSE("Wrong clip rect: x2 > fb.width\n");
		rect.x2 = fb->base.width;
	}

	if (rect.y2 > fb->base.height) {
		EVDI_VERBOSE("Wrong clip rect: y2 > fb.height\n");
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
	evdi_painter_set_new_scanout_buffer(evdi, fb);
	evdi_painter_commit_scanout_buffer(evdi);
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
	struct evdi_fbdev *ufbdev = info->par;

	EVDI_CHECKPT();
	sys_fillrect(info, rect);
	evdi_handle_damage(&ufbdev->ufb, rect->dx, rect->dy, rect->width,
			   rect->height);
}

static void evdi_fb_copyarea(struct fb_info *info,
			     const struct fb_copyarea *region)
{
	struct evdi_fbdev *ufbdev = info->par;

	EVDI_CHECKPT();
	sys_copyarea(info, region);
	evdi_handle_damage(&ufbdev->ufb, region->dx, region->dy, region->width,
			   region->height);
}

static void evdi_fb_imageblit(struct fb_info *info,
			      const struct fb_image *image)
{
	struct evdi_fbdev *ufbdev = info->par;

	EVDI_CHECKPT();
	sys_imageblit(info, image);
	evdi_handle_damage(&ufbdev->ufb, image->dx, image->dy, image->width,
			   image->height);
}

/*
 * It's common for several clients to have framebuffer open simultaneously.
 * e.g. both fbcon and X. Makes things interesting.
 * Assumes caller is holding info->lock (for open and release at least)
 */
static int evdi_fb_open(struct fb_info *info, int user)
{
	struct evdi_fbdev *ufbdev = info->par;

	ufbdev->fb_count++;
	pr_notice("open /dev/fb%d user=%d fb_info=%p count=%d\n",
		  info->node, user, info, ufbdev->fb_count);

	return 0;
}

/*
 * Assumes caller is holding info->lock mutex (for open and release at least)
 */
static int evdi_fb_release(struct fb_info *info, int user)
{
	struct evdi_fbdev *ufbdev = info->par;

	ufbdev->fb_count--;

	pr_warn("released /dev/fb%d user=%d count=%d\n",
		info->node, user, ufbdev->fb_count);

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

static int evdi_user_framebuffer_dirty(struct drm_framebuffer *fb,
				       __always_unused struct drm_file *file,
				       __always_unused unsigned int flags,
				       __always_unused unsigned int color,
				       struct drm_clip_rect *clips,
				       unsigned int num_clips)
{
	struct evdi_framebuffer *ufb = to_evdi_fb(fb);
	struct drm_device *dev = ufb->base.dev;
	struct evdi_device *evdi = dev->dev_private;
	int i;
	int ret = 0;

	EVDI_CHECKPT();
	drm_modeset_lock_all(fb->dev);

	if (!ufb->active)
		goto unlock;

	if (ufb->obj->base.import_attach) {
		ret =
		    dma_buf_begin_cpu_access(
			ufb->obj->base.import_attach->dmabuf,
			DMA_FROM_DEVICE);
		if (ret)
			goto unlock;
	}

	for (i = 0; i < num_clips; i++) {
		ret = evdi_handle_damage(ufb, clips[i].x1, clips[i].y1,
					 clips[i].x2 - clips[i].x1,
					 clips[i].y2 - clips[i].y1);
		if (ret)
			goto unlock;
	}

	if (ufb->obj->base.import_attach)
		dma_buf_end_cpu_access(ufb->obj->base.import_attach->dmabuf,
				       DMA_FROM_DEVICE);
	atomic_add(1, &evdi->frame_count);
 unlock:
	drm_modeset_unlock_all(fb->dev);
	return ret;
}

static int evdi_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					       struct drm_file *file_priv,
					       unsigned int *handle)
{
	struct evdi_framebuffer *efb = to_evdi_fb(fb);

	return drm_gem_handle_create(file_priv, &efb->obj->base, handle);
}

static void evdi_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct evdi_framebuffer *ufb = to_evdi_fb(fb);

	EVDI_CHECKPT();

	if (ufb->obj)
		drm_gem_object_put_unlocked(&ufb->obj->base);

	drm_framebuffer_cleanup(fb);
	kfree(ufb);
}

static const struct drm_framebuffer_funcs evdifb_funcs = {
	.create_handle = evdi_user_framebuffer_create_handle,
	.destroy = evdi_user_framebuffer_destroy,
	.dirty = evdi_user_framebuffer_dirty,
};

static int
evdi_framebuffer_init(struct drm_device *dev,
		      struct evdi_framebuffer *ufb,
		      const struct drm_mode_fb_cmd2 *mode_cmd,
		      struct evdi_gem_object *obj)
{
	ufb->obj = obj;
	drm_helper_mode_fill_fb_struct(dev, &ufb->base, mode_cmd);
	return drm_framebuffer_init(dev, &ufb->base, &evdifb_funcs);
}

#ifdef CONFIG_FB
static int evdifb_create(struct drm_fb_helper *helper,
			 struct drm_fb_helper_surface_size *sizes)
{
	struct evdi_fbdev *ufbdev = (struct evdi_fbdev *)helper;
	struct drm_device *dev = ufbdev->helper.dev;
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

	ret = evdi_framebuffer_init(dev, &ufbdev->ufb, &mode_cmd, obj);
	if (ret)
		goto out_gfree;

	fb = &ufbdev->ufb.base;

	ufbdev->helper.fb = fb;
	ufbdev->helper.fbdev = info;

	info->screen_base = ufbdev->ufb.obj->vmapping;
	info->fix.smem_len = size;
	info->fix.smem_start = (unsigned long)ufbdev->ufb.obj->vmapping;

	info->flags = FBINFO_DEFAULT;
	info->fbops = &evdifb_ops;

	drm_fb_helper_fill_info(info, helper, sizes);

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out_gfree;
	}

	DRM_DEBUG_KMS("allocated %dx%d vmal %p\n",
		      fb->width, fb->height, ufbdev->ufb.obj->vmapping);

	return ret;
 out_gfree:
	drm_gem_object_put_unlocked(&ufbdev->ufb.obj->base);
 out:
	return ret;
}

static struct drm_fb_helper_funcs evdi_fb_helper_funcs = {
	.fb_probe = evdifb_create,
};

static void evdi_fbdev_destroy(__always_unused struct drm_device *dev,
			       struct evdi_fbdev *ufbdev)
{
	struct fb_info *info;

	if (ufbdev->helper.fbdev) {
		info = ufbdev->helper.fbdev;
		unregister_framebuffer(info);
		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);

		framebuffer_release(info);
	}
	drm_fb_helper_fini(&ufbdev->helper);
	drm_framebuffer_unregister_private(&ufbdev->ufb.base);
	drm_framebuffer_cleanup(&ufbdev->ufb.base);
	drm_gem_object_put_unlocked(&ufbdev->ufb.obj->base);
}

int evdi_fbdev_init(struct drm_device *dev)
{
	struct evdi_device *evdi;
	struct evdi_fbdev *ufbdev;
	int ret;

	evdi = dev->dev_private;
	ufbdev = kzalloc(sizeof(struct evdi_fbdev), GFP_KERNEL);
	if (!ufbdev)
		return -ENOMEM;

	evdi->fbdev = ufbdev;
	drm_fb_helper_prepare(dev, &ufbdev->helper, &evdi_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, &ufbdev->helper, 1);
	if (ret) {
		kfree(ufbdev);
		return ret;
	}

	drm_fb_helper_single_add_all_connectors(&ufbdev->helper);

	ret = drm_fb_helper_initial_config(&ufbdev->helper, 32);
	if (ret) {
		drm_fb_helper_fini(&ufbdev->helper);
		kfree(ufbdev);
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
	struct evdi_fbdev *ufbdev;

	if (!evdi->fbdev)
		return;

	ufbdev = evdi->fbdev;
	if (ufbdev->helper.fbdev) {
		struct fb_info *info;

		info = ufbdev->helper.fbdev;
		unlink_framebuffer(info);
	}
}
#endif /* CONFIG_FB */

int evdi_fb_get_bpp(uint32_t format)
{
	const struct drm_format_info *info;

	info = drm_format_info(format);
	if (info && info->depth)
		return info->cpp[0] * 8;
	return 0;
}

struct drm_framebuffer *evdi_fb_user_fb_create(
					struct drm_device *dev,
					struct drm_file *file,
					const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *obj;
	struct evdi_framebuffer *ufb;
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

	size = mode_cmd->pitches[0] * mode_cmd->height;
	size = ALIGN(size, PAGE_SIZE);

	if (size > obj->size) {
		DRM_ERROR("object size not sufficient for fb %d %zu %d %d\n",
			  size, obj->size, mode_cmd->pitches[0],
			  mode_cmd->height);
		goto err_no_mem;
	}

	ufb = kzalloc(sizeof(*ufb), GFP_KERNEL);
	if (ufb == NULL)
		goto err_no_mem;

	ret = evdi_framebuffer_init(dev, ufb, mode_cmd, to_evdi_bo(obj));
	if (ret)
		goto err_inval;
	return &ufb->base;

 err_no_mem:
	drm_gem_object_put(obj);
	return ERR_PTR(-ENOMEM);
 err_inval:
	kfree(ufb);
	drm_gem_object_put(obj);
	return ERR_PTR(-EINVAL);
}
