/*
 * Copyright 2011 Red Hat, Inc.
 * Copyright © 2014 The Chromium OS Authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software")
 * to deal in the software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * them Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTIBILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT, OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Adam Jackson <ajax@redhat.com>
 *	Ben Widawsky <ben@bwidawsk.net>
 */

/**
 * This is vgem, a (non-hardware-backed) GEM service.  This is used by Mesa's
 * software renderer and the X server for efficient buffer sharing.
 */

#include <linux/module.h>
#include <linux/ramfs.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include "vgem_drv.h"
#include "../drm_crtc_internal.h"

#define DRIVER_NAME	"vgem"
#define DRIVER_DESC	"Virtual GEM provider"
#define DRIVER_DATE	"20120112"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

void vgem_gem_put_pages(struct drm_vgem_gem_object *obj)
{
	drm_gem_put_pages(&obj->base, obj->pages, false, false);
	obj->pages = NULL;
}

static void vgem_gem_free_object(struct drm_gem_object *obj)
{
	struct drm_vgem_gem_object *vgem_obj = to_vgem_bo(obj);

	drm_gem_free_mmap_offset(obj);

	if (obj->import_attach) {
		dma_buf_put(obj->import_attach->dmabuf);
		obj->dma_buf = NULL;
	}

	drm_gem_object_release(obj);

	if (vgem_obj->pages)
		vgem_gem_put_pages(vgem_obj);

	vgem_obj->pages = NULL;

	kfree(vgem_obj);
}

int vgem_gem_get_pages(struct drm_vgem_gem_object *obj)
{
	struct page **pages;

	if (obj->pages || obj->base.import_attach)
		return 0;

	pages = drm_gem_get_pages(&obj->base);
	if (IS_ERR(pages)) {
		return PTR_ERR(pages);
	}

	obj->pages = pages;

	return 0;
}

static int vgem_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *gobj = vma->vm_private_data;
	struct drm_vgem_gem_object *obj = to_vgem_bo(gobj);
	loff_t num_pages;
	pgoff_t page_offset;
	int ret;

	/* We don't use vmf->pgoff since that has the fake offset */
	page_offset = ((unsigned long)vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;

	num_pages = DIV_ROUND_UP(obj->base.size, PAGE_SIZE);

	if (page_offset > num_pages)
		return VM_FAULT_SIGBUS;

	ret = vm_insert_page(vma, (unsigned long)vmf->virtual_address,
			     obj->pages[page_offset]);
	switch (ret) {
	case -EAGAIN:
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
	case -EBUSY:
		/*
		 * EBUSY is ok: this just means that another thread
		 * already did the job.
		 */
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

static const struct vm_operations_struct vgem_gem_vm_ops = {
	.fault = vgem_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static int vgem_open(struct drm_device *dev, struct drm_file *file)
{
	struct vgem_file *vfile;
	int ret;

	vfile = kzalloc(sizeof(*vfile), GFP_KERNEL);
	if (!vfile)
		return -ENOMEM;

	file->driver_priv = vfile;

	ret = vgem_fence_open(vfile);
	if (ret) {
		kfree(vfile);
		return ret;
	}

	return 0;
}

static void vgem_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct vgem_file *vfile = file->driver_priv;

	vgem_fence_close(vfile);
	kfree(vfile);
}

/* ioctls */

static struct drm_gem_object *vgem_gem_create(struct drm_device *dev,
					      struct drm_file *file,
					      unsigned int *handle,
					      unsigned long size)
{
	struct drm_vgem_gem_object *obj;
	struct drm_gem_object *gem_object;
	int err;

	size = roundup(size, PAGE_SIZE);

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	gem_object = &obj->base;

	err = drm_gem_object_init(dev, gem_object, size);
	if (err)
		goto out;

	err = vgem_gem_get_pages(obj);
	if (err)
		goto out;

	err = drm_gem_handle_create(file, gem_object, handle);
	if (err)
		goto handle_out;

	drm_gem_object_unreference_unlocked(gem_object);

	return gem_object;

handle_out:
	drm_gem_object_release(gem_object);
out:
	kfree(obj);
	return ERR_PTR(err);
}

static int vgem_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
				struct drm_mode_create_dumb *args)
{
	struct drm_gem_object *gem_object;
	uint64_t size;
	uint64_t pitch = args->width * DIV_ROUND_UP(args->bpp, 8);

	size = args->height * pitch;
	if (size == 0)
		return -EINVAL;

	gem_object = vgem_gem_create(dev, file, &args->handle, size);

	if (IS_ERR(gem_object)) {
		DRM_DEBUG_DRIVER("object creation failed\n");
		return PTR_ERR(gem_object);
	}

	args->size = gem_object->size;
	args->pitch = pitch;

	DRM_DEBUG_DRIVER("Created object of size %lld\n", size);

	return 0;
}

int vgem_gem_dumb_map(struct drm_file *file, struct drm_device *dev,
		      uint32_t handle, uint64_t *offset)
{
	int ret = 0;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(file, handle);
	if (!obj)
		return -ENOENT;

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto unref;

	BUG_ON(!obj->filp);

	obj->filp->private_data = obj;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);

unref:
	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

int vgem_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_vma_offset_node *node;
	struct drm_gem_object *obj = NULL;
	int ret = 0;

	drm_vma_offset_lock_lookup(dev->vma_offset_manager);

	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager,
						  vma->vm_pgoff,
						  vma_pages(vma));
	if (likely(node)) {
		obj = container_of(node, struct drm_gem_object, vma_node);
		if (!kref_get_unless_zero(&obj->refcount))
			obj = NULL;
	}
	drm_vma_offset_unlock_lookup(dev->vma_offset_manager);

	if (!obj)
		return -EINVAL;

	if (!drm_vma_node_is_allowed(node, filp)) {
		ret = -EACCES;
		goto unref;
	}

	if (obj->import_attach) {
		ret = dma_buf_mmap(obj->import_attach->dmabuf, vma, 0);
		goto unref;
	}

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret < 0)
		goto unref;

	vma->vm_flags |= VM_MIXEDMAP;
	vma->vm_flags &= ~VM_PFNMAP;

unref:
	drm_gem_object_unreference_unlocked(obj);
	return ret;
}


static struct drm_ioctl_desc vgem_ioctls[] = {
	DRM_IOCTL_DEF_DRV(VGEM_MODE_MAP_DUMB, drm_mode_mmap_dumb_ioctl,
			  DRM_CONTROL_ALLOW|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(VGEM_FENCE_ATTACH, vgem_fence_attach_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(VGEM_FENCE_SIGNAL, vgem_fence_signal_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
};

static const struct file_operations vgem_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= vgem_drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.unlocked_ioctl = drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.release	= drm_release,
};

static struct drm_driver vgem_driver = {
	.driver_features		=
		DRIVER_GEM | DRIVER_PRIME | DRIVER_RENDER,
	.open				= vgem_open,
	.preclose			= vgem_preclose,
	.gem_free_object_unlocked	= vgem_gem_free_object,
	.gem_vm_ops			= &vgem_gem_vm_ops,
	.ioctls				= vgem_ioctls,
	.num_ioctls			= ARRAY_SIZE(vgem_ioctls),
	.fops				= &vgem_driver_fops,
	.dumb_create			= vgem_gem_dumb_create,
	.dumb_map_offset		= vgem_gem_dumb_map,
	.prime_handle_to_fd		= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle		= drm_gem_prime_fd_to_handle,
	.gem_prime_export		= drm_gem_prime_export,
	.gem_prime_import		= drm_gem_prime_import,
	.gem_prime_pin			= vgem_gem_prime_pin,
	.gem_prime_unpin		= vgem_gem_prime_unpin,
	.gem_prime_get_sg_table		= vgem_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= vgem_gem_prime_import_sg_table,
	.gem_prime_vmap			= vgem_gem_prime_vmap,
	.gem_prime_vunmap		= vgem_gem_prime_vunmap,
	.gem_prime_mmap			= vgem_gem_prime_mmap,
	.name	= DRIVER_NAME,
	.desc	= DRIVER_DESC,
	.date	= DRIVER_DATE,
	.major	= DRIVER_MAJOR,
	.minor	= DRIVER_MINOR,
};

static int vgem_platform_probe(struct platform_device *pdev)
{
	vgem_driver.num_ioctls = ARRAY_SIZE(vgem_ioctls);

	return drm_platform_init(&vgem_driver, pdev);
}

static int vgem_platform_remove(struct platform_device *pdev)
{
	drm_put_dev(platform_get_drvdata(pdev));
	return 0;
}

static struct platform_driver vgem_platform_driver = {
	.probe		= vgem_platform_probe,
	.remove		= vgem_platform_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRIVER_NAME,
	},
};

static struct platform_device *vgem_device;

static int __init vgem_init(void)
{
	int ret;

	ret = platform_driver_register(&vgem_platform_driver);
	if (ret)
		goto out;

	vgem_device = platform_device_alloc("vgem", -1);
	if (!vgem_device) {
		ret = -ENOMEM;
		goto unregister_out;
	}

	vgem_device->dev.coherent_dma_mask = ~0ULL;
	vgem_device->dev.dma_mask = &vgem_device->dev.coherent_dma_mask;

	ret = platform_device_add(vgem_device);
	if (ret)
		goto put_out;

	return 0;

put_out:
	platform_device_put(vgem_device);
unregister_out:
	platform_driver_unregister(&vgem_platform_driver);
out:
	return ret;
}

static void __exit vgem_exit(void)
{
	platform_device_unregister(vgem_device);
	platform_driver_unregister(&vgem_platform_driver);
}

module_init(vgem_init);
module_exit(vgem_exit);

MODULE_AUTHOR("Red Hat, Inc.");
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
